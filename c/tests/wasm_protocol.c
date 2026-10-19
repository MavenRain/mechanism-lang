/* C port of tests/wasm_protocol.bend: SUITE-WASM. For every ROOT/fixtures/<name>.kan that
 * checks and defines main, the driver erases and emits the program to OUTDIR/<name>.wasm, validates
 * it with wasm-opt (which also writes OUTDIR/<name>.wat), compares the .wat with
 * ROOT/golden/<name>.wat, runs the module under node (dev/run-node.mjs of the repo that holds ROOT)
 * and compares the printed answer with the kernel value of main. It prints EMIT <name> OK or
 * EMIT <name> FAIL: <message> per selected fixture, then WASM-OK <passed>/<total> and SUITE-WASM OK,
 * or SUITE-WASM FAIL with exit 1 (Out.cli_exit). The foreign effect wasm_fixture_io of
 * tests/effects/wasm_fixture.c is ported here as fixture_io (operation 0 lists the fixtures,
 * 1 validates a module); its answer is the Bend text protocol: '+' and the value, or '-' and the
 * message. Each fixture runs in its own arena, which is freed after its report line. */
#include "cli.h"
#include "driver.h"
#include "surface.h"
#include "wasm.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

/* Result<String, Unit>: ok, or the failure message */
typedef struct {
  bool ok;
  Str message;
} Check;

/* C.Pair<Nat, Nat>: passed and total */
typedef struct {
  size_t passed;
  size_t total;
} Counts;

static Check done(void) {
  Check r = {true, str_empty()};
  return r;
}

static Check fail(Str message) {
  Check r = {false, message};
  return r;
}

static Counts pair(size_t passed, size_t total) {
  Counts r = {passed, total};
  return r;
}

static Str cat(Heap *heap, Str a, Str b) { return heap_str_cat(heap, a, b); }

/* ---- wasm_fixture_io (18-20): the effect of tests/effects/wasm_fixture.c ---- */

static Str fixture_error(Heap *heap, const char *message) { return cat(heap, str_lit("-"), str_lit(message)); }

static bool fixture_has_nul(Str s) { return s.len > 0 && memchr(s.data, 0, s.len) != NULL; }

static int fixture_mkdir(const char *path) { return mkdir(path, 0755) == 0 || errno == EEXIST; }

static int fixture_compare(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* wf_list: create the parent of OUTDIR and OUTDIR, then '+' and the sorted ROOT/fixtures/*.kan
 * names without the suffix, one per line. */
static Str fixture_list(Heap *heap, Str root, Str outdir) {
  char *parent = str_cstr(heap->arena, outdir);
  char *second = str_cstr(heap->arena, outdir);
  char *path = str_cstr(heap->arena, cat(heap, root, str_lit("/fixtures")));
  if (parent == NULL || second == NULL || path == NULL || heap->oom) {
    heap->oom = true;
    return str_empty();
  }
  char *slash = strrchr(parent, '/');
  const char *where = parent;
  if (slash == NULL) {
    where = ".";
  } else if (slash == parent) {
    slash[1] = 0;
  } else {
    *slash = 0;
  }
  int okay = fixture_mkdir(where);
  if (!okay || !fixture_mkdir(second)) return fixture_error(heap, strerror(errno));
  DIR *dir = opendir(path);
  if (dir == NULL) return fixture_error(heap, strerror(errno));
  Vec names = vec_new(heap->arena);
  struct dirent *entry;
  errno = 0;
  while ((entry = readdir(dir)) != NULL) {
    size_t n = strlen(entry->d_name);
    if (n >= 4 && strcmp(entry->d_name + n - 4, ".kan") == 0) {
      char *name = str_cstr(heap->arena, str_n(entry->d_name, n - 4));
      if (name == NULL || !vec_push(&names, name)) heap->oom = true;
    }
  }
  int saved = errno;
  closedir(dir);
  if (heap->oom) return str_empty();
  if (saved != 0) return fixture_error(heap, strerror(saved));
  if (names.len > 1) qsort(names.items, names.len, sizeof(void *), fixture_compare);
  StrBuf out = strbuf_new(heap->arena);
  bool ok = strbuf_push_byte(&out, '+');
  size_t i = 0;
  for (; i < names.len && ok; i += 1) {
    VecGet got = vec_get(&names, i);
    if (got.status != VEC_GET_OK) return fixture_error(heap, "fixture listing out of range");
    if (i > 0) ok = strbuf_push_byte(&out, '\n');
    ok = ok && strbuf_push(&out, str_lit((const char *)got.value));
  }
  if (!ok) {
    heap->oom = true;
    return str_empty();
  }
  return strbuf_str(&out);
}

/* The first line of the wasm-opt stderr file, cut at the first CR or LF, or the strerror text of
 * CODE when the file is missing or its first line is empty. */
static Str fixture_failure(Heap *heap, const char *err, int code) {
  FILE *file = fopen(err, "r");
  char *line = NULL;
  size_t capacity = 0;
  if (file != NULL) {
    if (getline(&line, &capacity, file) < 0) {
      free(line);
      line = NULL;
    }
    fclose(file);
  }
  if (line != NULL) line[strcspn(line, "\r\n")] = 0;
  Str message = fixture_error(heap, line != NULL && line[0] != 0 ? line : strerror(code));
  free(line);
  return message;
}

/* wf_validate: wasm-opt BASE.wasm -S -o BASE.wat with the GC, reference-type, tail-call and
 * exception-handling features, its stderr in BASE.err; '+' when it exits 0. */
static Str fixture_validate(Heap *heap, Str base) {
  char *wasm = str_cstr(heap->arena, cat(heap, base, str_lit(".wasm")));
  char *wat = str_cstr(heap->arena, cat(heap, base, str_lit(".wat")));
  char *err = str_cstr(heap->arena, cat(heap, base, str_lit(".err")));
  if (wasm == NULL || wat == NULL || err == NULL || heap->oom) {
    heap->oom = true;
    return str_empty();
  }
  int fd = open(err, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  int code = fd < 0 ? errno : 0;
  if (code == 0) {
    posix_spawn_file_actions_t actions;
    code = posix_spawn_file_actions_init(&actions);
    if (code == 0) {
      code = posix_spawn_file_actions_adddup2(&actions, fd, STDERR_FILENO);
      if (code == 0 && fd != STDERR_FILENO) code = posix_spawn_file_actions_addclose(&actions, fd);
      char *args[] = {"wasm-opt", wasm, "-S", "-o", wat, "--enable-gc", "--enable-reference-types",
                      "--enable-tail-call", "--enable-exception-handling", NULL};
      pid_t pid;
      if (code == 0) code = posix_spawnp(&pid, "wasm-opt", &actions, NULL, args, environ);
      posix_spawn_file_actions_destroy(&actions);
      if (code == 0) {
        int status = 0;
        pid_t waited;
        do {
          waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited < 0) {
          code = errno;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
          code = EINVAL;
        }
      }
    }
    close(fd);
  }
  if (code != 0) return fixture_failure(heap, err, code);
  return str_lit("+");
}

static Str fixture_io(Heap *heap, uint32_t operation, Str first, Str second) {
  if (fixture_has_nul(first) || fixture_has_nul(second)) return fixture_error(heap, "fixture path contains NUL");
  if (operation == 0u) return fixture_list(heap, first, second);
  if (operation == 1u) return fixture_validate(heap, first);
  return fixture_error(heap, "unknown Wasm fixture operation");
}

/* foreign_result (22-26) */
static Check foreign_result(Heap *heap, Str text) {
  StrAt head = str_at(text, 0);
  if (head.status == STR_INDEX_OK && head.value == '+') return done();
  if (head.status == STR_INDEX_OK && head.value == '-') {
    return fail(cat(heap, str_lit("validate: "), str_n((const char *)text.data + 1, text.len - 1)));
  }
  return fail(str_lit("invalid fixture effect response"));
}

/* require (28-29) */
static Check require(bool ok, Str message) {
  if (ok) return done();
  return fail(message);
}

/* defines_main (31-34) */
static bool defines_main(const Vec *rows) {
  size_t i = 0;
  for (; i < rows->len; i += 1) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK && str_eq(((const CheckRow *)got.value)->name, str_lit("main"))) return true;
  }
  return false;
}

/* emitted (36-39): Erase.program then Emit.program(erased, "main"); the bytes on Done */
static Result emitted(Heap *heap, Globals globals, const Vec *rows) {
  Result erased = erase_program(heap, globals, rows);
  switch (erased.kind) {
    case RESULT_DONE: return emit_program(heap, (const Vec *)erased.value, str_lit("main"));
    case RESULT_FAIL: return erased;
  }
  return erased;
}

/* value (41-45): the integer of V.VLit{L.LInt{n}} */
typedef struct {
  bool ok;
  Big number;
  Str message;
} Expected;

static Expected expected_fail(Str message) {
  Expected r = {false, big_zero(), message};
  return r;
}

static Expected value(Heap *heap, Result result) {
  const Value *v = (const Value *)result.value;
  switch (result.kind) {
    case RESULT_FAIL: return expected_fail(error_to_string(heap, result.error));
    case RESULT_DONE:
      if (v != NULL && v->kind == VALUE_LIT) {
        switch (v->lit.kind) {
          case LITERAL_INT: {
            Expected r = {true, v->lit.integer, str_empty()};
            return r;
          }
          case LITERAL_STRING: return expected_fail(str_lit("kernel value not a literal"));
        }
      }
      return expected_fail(str_lit("kernel value not a literal"));
  }
  return expected_fail(str_lit("kernel value not a literal"));
}

/* weak (47-50) */
static Result weak(Heap *heap, Globals globals, Result result) {
  switch (result.kind) {
    case RESULT_FAIL: return result;
    case RESULT_DONE: return eval_whnf(heap, globals, (const Value *)result.value);
  }
  return result;
}

/* kernel_value (52-53) */
static Expected kernel_value(Heap *heap, Globals globals) {
  return value(heap, weak(heap, globals, eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, str_lit("main")))));
}

/* observed (55-58) */
static Str observed(Heap *heap, bool empty, uint32_t code, Str text) {
  if (empty) return cat(heap, str_lit("exit "), str_of_u64(heap->arena, code));
  return text;
}

/* answer (60-65) */
static Check answer(Heap *heap, MaybeU32 expected, uint32_t code, Str printed) {
  bool empty = printed.len == 0;
  if (!expected.some) {
    return require(code == 1u && empty, cat(heap, str_lit("node: expected a trap got "), observed(heap, empty, code, printed)));
  }
  Str want = str_of_u64(heap->arena, expected.value);
  Str message = cat(heap, cat(heap, cat(heap, str_lit("node: expected "), want), str_lit(" got ")), observed(heap, empty, code, printed));
  return require(code == 0u && str_eq(printed, want), message);
}

/* String.trim: drop Char.is_space (32 and 9..13) at both ends (as c/cli/host.c trim). */
static bool is_space(uint8_t c) { return c == ' ' || (c >= 9 && c <= 13); }

static Str trim(Str s) {
  size_t start = 0;
  size_t end = s.len;
  while (start < end && is_space((uint8_t)s.data[start])) start += 1;
  while (end > start && is_space((uint8_t)s.data[end - 1])) end -= 1;
  return str_n((const char *)s.data + start, end - start);
}

/* output (67-70) */
static Check output(Heap *heap, MaybeU32 expected, uint32_t code, IoResult r) {
  switch (r.status) {
    case IO_FAIL: return fail(cat(heap, str_lit("node: "), r.value));
    case IO_DONE: return answer(heap, expected, code, trim(r.value));
  }
  return fail(cat(heap, str_lit("node: "), r.value));
}

/* run_expected (72-78) */
static Check run_expected(Heap *heap, Expected expected, Str base, Str repo) {
  if (!expected.ok) return fail(expected.message);
  uint32_t code = host_process(heap, str_lit("node"), cat(heap, repo, str_lit("/dev/run-node.mjs")), cat(heap, base, str_lit(".wasm")),
                               str_lit("main"), cat(heap, base, str_lit(".out")), cat(heap, base, str_lit(".nodeerr")));
  IoResult printed = io_read_file(heap, cat(heap, base, str_lit(".out")));
  return output(heap, big_to_i31(expected.number), code, printed);
}

/* golden_same (80-83) */
static Check golden_same(IoResult a, IoResult b) {
  if (a.status == IO_DONE && b.status == IO_DONE) return require(str_eq(a.value, b.value), str_lit("golden differs"));
  return fail(str_lit("golden differs"));
}

/* golden_checked (85-88) */
static Check golden_checked(Heap *heap, Check r, Globals globals, Str base, Str repo) {
  if (!r.ok) return r;
  return run_expected(heap, kernel_value(heap, globals), base, repo);
}

/* validated (90-96) */
static Check validated(Heap *heap, Check r, Str root, Str name, Str base, Globals globals, Str repo) {
  if (!r.ok) return r;
  IoResult got = io_read_file(heap, cat(heap, base, str_lit(".wat")));
  IoResult want = io_read_file(heap, cat(heap, cat(heap, cat(heap, root, str_lit("/golden/")), name), str_lit(".wat")));
  return golden_checked(heap, golden_same(got, want), globals, base, repo);
}

/* written (98-103) */
static Check written(Heap *heap, IoResult r, Str root, Str name, Str base, Globals globals, Str repo) {
  switch (r.status) {
    case IO_FAIL: return fail(cat(heap, str_lit("emit: "), r.value));
    case IO_DONE: {
      Str validation = fixture_io(heap, 1u, base, str_empty());
      return validated(heap, foreign_result(heap, validation), root, name, base, globals, repo);
    }
  }
  return fail(cat(heap, str_lit("emit: "), r.value));
}

/* emit_result (105-110) */
static Check emit_result(Heap *heap, Result r, Str root, Str name, Str base, Globals globals, Str repo) {
  switch (r.kind) {
    case RESULT_FAIL: return fail(cat(heap, str_lit("emit: "), error_to_string(heap, r.error)));
    case RESULT_DONE: return written(heap, io_write_bytes(heap, cat(heap, base, str_lit(".wasm")), *(const Str *)r.value), root, name, base, globals, repo);
  }
  return fail(cat(heap, str_lit("emit: "), error_to_string(heap, r.error)));
}

/* report (112-119) */
static Counts report(Heap *heap, Str name, Check r) {
  if (heap->oom) return pair(0, 0);
  if (r.ok) {
    driver_print(cat(heap, cat(heap, str_lit("EMIT "), name), str_lit(" OK")));
    return pair(1, 1);
  }
  driver_print(cat(heap, cat(heap, cat(heap, str_lit("EMIT "), name), str_lit(" FAIL: ")), r.message));
  return pair(0, 1);
}

/* selected (121-126) */
static Counts selected(Heap *heap, bool yes, Str root, Str name, Str outdir, Globals globals, const Vec *rows) {
  if (!yes) return pair(0, 0);
  Str base = cat(heap, cat(heap, outdir, str_lit("/")), name);
  return report(heap, name, emit_result(heap, emitted(heap, globals, rows), root, name, base, globals, host_dirname(root)));
}

/* checked (128-131) */
static Counts checked(Heap *heap, Result r, Str root, Str name, Str outdir) {
  const ElabChecked *program = (const ElabChecked *)r.value;
  switch (r.kind) {
    case RESULT_FAIL: return pair(0, 0);
    case RESULT_DONE:
      if (program == NULL) return pair(0, 0);
      return selected(heap, defines_main(&program->rows), root, name, outdir, program->globals, &program->rows);
  }
  return pair(0, 0);
}

/* read_result (133-136) */
static Counts read_result(Heap *heap, IoResult r, Str root, Str name, Str outdir) {
  switch (r.status) {
    case IO_FAIL: return pair(0, 0);
    case IO_DONE: return checked(heap, elab_check_in(heap, global_initial(heap), budget_unlimited(), r.value), root, name, outdir);
  }
  return pair(0, 0);
}

/* counts (138-140) */
static Counts counts(Counts a, Counts b) { return pair(a.passed + b.passed, a.total + b.total); }

/* One fixture of fixtures (146-149) in its own arena: the name is copied in, the counts come out. */
typedef struct {
  bool oom;
  Counts counts;
} FixtureRun;

static FixtureRun fixture(Str root, Str name, Str outdir) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str source_path = cat(&heap, cat(&heap, cat(&heap, root, str_lit("/fixtures/")), name), str_lit(".kan"));
  Counts result = pair(0, 0);
  if (!heap.oom) result = read_result(&heap, io_read_file(&heap, source_path), root, name, outdir);
  FixtureRun run = {heap.oom, result};
  arena_free(&arena);
  return run;
}

/* fixtures (142-149) over String.split(names, '\n'); an empty piece is skipped. */
static FixtureRun fixtures(Str names, Str root, Str outdir) {
  FixtureRun total = {false, pair(0, 0)};
  size_t start = 0;
  bool more = true;
  while (more && !total.oom) {
    const uint8_t *at = names.len > start ? memchr(names.data + start, '\n', names.len - start) : NULL;
    size_t end = at != NULL ? (size_t)(at - (const uint8_t *)names.data) : names.len;
    Str name = str_n((const char *)names.data + start, end - start);
    if (name.len > 0) {
      FixtureRun one = fixture(root, name, outdir);
      total.oom = one.oom;
      total.counts = counts(total.counts, one.counts);
    }
    more = at != NULL;
    start = end + 1;
  }
  return total;
}

/* finish (151-156) */
static int finish(bool ok) {
  if (ok) {
    driver_print(str_lit("SUITE-WASM OK"));
    return 0;
  }
  driver_print(str_lit("SUITE-WASM FAIL"));
  return cli_exit(1u);
}

/* fail_out (158-161) */
static int fail_out(Heap *heap, Str message) {
  Str line = cat(heap, str_lit("SUITE "), message);
  if (heap->oom) return driver_host_failure();
  driver_print(line);
  return finish(false);
}

/* verdict (163-167) */
static int verdict(Heap *heap, Counts total) {
  char line[96];
  snprintf(line, sizeof line, "WASM-OK %zu/%zu", total.passed, total.total);
  if (heap->oom) return driver_host_failure();
  driver_print(str_lit(line));
  return finish(total.passed == total.total && total.total > 0);
}

/* listed (169-175) */
static int listed(Heap *heap, Str result, Str root, Str outdir) {
  StrAt head = str_at(result, 0);
  if (heap->oom) return driver_host_failure();
  if (head.status == STR_INDEX_OK && head.value == '+') {
    FixtureRun total = fixtures(str_n((const char *)result.data + 1, result.len - 1), root, outdir);
    if (total.oom) return driver_host_failure();
    return verdict(heap, total.counts);
  }
  if (head.status == STR_INDEX_OK && head.value == '-') return fail_out(heap, str_n((const char *)result.data + 1, result.len - 1));
  return fail_out(heap, str_lit("invalid fixture listing"));
}

/* run (177-180) */
static int run(Heap *heap, Str root, Str outdir) {
  Str names = fixture_io(heap, 0u, root, outdir);
  return listed(heap, names, root, outdir);
}

/* arguments (182-186) */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc < 2) return run(heap, str_lit("test"), str_lit("_build/wasm-suite"));
  Str root = str_lit(argv[1]);
  if (argc == 2) return run(heap, root, cat(heap, host_dirname(root), str_lit("/_build/wasm-suite")));
  return run(heap, root, str_lit(argv[2]));
}

/* main (188-191) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
