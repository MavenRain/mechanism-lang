/* Port of tests/prelude_runtime.bend (oracle label prelude-runtime): `[--reachable] SOURCE
 * DIRECTORY EXPORT...` checks SOURCE against the initial globals with an unlimited budget, refuses
 * source axioms and export names that are not plain file names, erases the whole program (or, with
 * --reachable, only the rows reachable from the exports), then for each export in order evaluates it
 * to a natural number, writes its WasmGC module to DIRECTORY/EXPORT.wasm and prints EXPORT TAB VALUE;
 * `--slice-self-test` runs the runtime_slice and runtime_reachable self tests and prints
 * RUNTIME-SLICE-OK. Result<String, A> values are SliceResult values (runtime_slice.h). Definitions keep
 * the Bend order and names. */
#include "cli.h"
#include "driver.h"
#include "runtime_reachable.h"
#include "surface.h"
#include "wasm.h"

/* Result<String, Prepared> with Prepared{globals, rows}; rows is a Slice.Rows Vec of EraseRow pointers. */
typedef struct {
  SliceResultKind kind;
  Str message;     /* SLICE_FAIL */
  Globals globals; /* SLICE_DONE */
  Vec *rows;       /* SLICE_DONE */
} Prepared;

/* Result<String, String> */
typedef struct {
  SliceResultKind kind;
  Str message; /* SLICE_FAIL */
  Str text;    /* SLICE_DONE */
} Answer;

/* Result<String, Pair<String, List<U32>>>: the answer text and the module bytes. */
typedef struct {
  SliceResultKind kind;
  Str message; /* SLICE_FAIL */
  Str answer;  /* SLICE_DONE */
  Str bytes;   /* SLICE_DONE */
} Exported;

static Prepared prepared_fail(Str message) {
  Prepared p;
  p.kind = SLICE_FAIL;
  p.message = message;
  p.globals = global_empty();
  p.rows = NULL;
  return p;
}

static Prepared prepared_done(Globals globals, Vec *rows) {
  Prepared p;
  p.kind = SLICE_DONE;
  p.message = str_empty();
  p.globals = globals;
  p.rows = rows;
  return p;
}

static Answer answer_fail(Str message) {
  Answer a;
  a.kind = SLICE_FAIL;
  a.message = message;
  a.text = str_empty();
  return a;
}

static Answer answer_done(Str text) {
  Answer a;
  a.kind = SLICE_DONE;
  a.message = str_empty();
  a.text = text;
  return a;
}

static Exported exported_fail(Str message) {
  Exported e;
  e.kind = SLICE_FAIL;
  e.message = message;
  e.answer = str_empty();
  e.bytes = str_empty();
  return e;
}

static Exported exported_done(Str answer, Str bytes) {
  Exported e;
  e.kind = SLICE_DONE;
  e.message = str_empty();
  e.answer = answer;
  e.bytes = bytes;
  return e;
}

/* A Done payload is NULL only after an allocation failure (heap->oom is set, so the IO boundary
 * reports the host failure). */
static const char HOST_OOM[] = "host out of memory";

static int die_text(Heap *heap, int code, Str message) {
  if (heap->oom) return driver_host_failure();
  return driver_die_text(code, message);
}

static int print_line(Heap *heap, Str text) {
  if (heap->oom) return driver_host_failure();
  return driver_print(text);
}

/* kernel (21-24) */
static SliceResult kernel(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_DONE: return slice_done(result.value);
    case RESULT_FAIL: return slice_fail(error_to_string(heap, result.error));
  }
  return slice_fail(error_to_string(heap, result.error));
}

/* no_axioms (26-29) */
static SliceResult no_axioms(Vec names) {
  if (names.len == 0) return slice_done(kernel_unit());
  return slice_fail(str_lit("runtime fixture declares axioms"));
}

/* final_component (31-35) + basename (37-40): the text after the last slash, "/" for "/". A slash
 * byte never occurs inside a UTF-8 sequence, so the byte scan cuts where the Bend char scan cuts. */
static Str basename(Str normalized) {
  size_t start = normalized.len;
  if (str_eq(normalized, str_lit("/"))) return normalized;
  while (start > 0 && normalized.data[start - 1] != '/') start--;
  return str_n((const char *)normalized.data + start, normalized.len - start);
}

/* The check of valid_exports (47) and prepare_export (87). */
static SliceResult valid_name(Str name) {
  return slice_require(str_eq(basename(io_normalize_target(name)), name), str_lit("invalid export path"));
}

/* valid_exports (42-48) */
static SliceResult valid_exports(const Vec *names) {
  size_t i;
  for (i = 0; i < names->len; i++) {
    SliceResult checked = valid_name(slice_name_at(names, i));
    if (slice_failed(checked)) return checked;
  }
  return slice_done(kernel_unit());
}

/* prepare_mode (50-59) */
static Prepared prepare_mode(Heap *heap, bool reachable, Globals globals, const Vec *rows, const Vec *names) {
  SliceResult erased;
  if (reachable) {
    erased = reach_select_many(heap, globals, rows, names);
  } else {
    erased = kernel(heap, erase_program(heap, globals, rows));
  }
  if (slice_failed(erased)) return prepared_fail(erased.message);
  if (erased.value == NULL) return prepared_fail(str_lit(HOST_OOM));
  return prepared_done(globals, erased.value);
}

/* prepare_rows (61-67) */
static Prepared prepare_rows(Heap *heap, const ElabChecked *checked, bool reachable, const Vec *names) {
  SliceResult step = no_axioms(elab_axiom_names(heap, &checked->rows));
  if (slice_failed(step)) return prepared_fail(step.message);
  step = valid_exports(names);
  if (slice_failed(step)) return prepared_fail(step.message);
  return prepare_mode(heap, reachable, checked->globals, &checked->rows, names);
}

/* prepare (69-72). The check runs on the host-only compact path (D-253a): elab_check_in_compact is
 * check_in under budget_unlimited with a fresh arena per declaration, so each declaration's checker
 * garbage is freed after its step (the Bend bundle reclaimed it by garbage collection). Its rows,
 * globals and error text equal those of check_in with budget_unlimited (decision 156). */
static Prepared prepare(Heap *heap, Str text, bool reachable, const Vec *names) {
  SliceResult checked = kernel(heap, elab_check_in_compact(heap, global_initial(heap), text));
  if (slice_failed(checked)) return prepared_fail(checked.message);
  if (checked.value == NULL) return prepared_fail(str_lit(HOST_OOM));
  return prepare_rows(heap, checked.value, reachable, names);
}

/* natural (74-78) */
static Answer natural(Heap *heap, const Value *value, Str export) {
  if (value == NULL || value->kind != VALUE_LIT) return answer_fail(heap_str_cat(heap, str_lit("not a literal: "), export));
  switch (value->lit.kind) {
    case LITERAL_INT: return answer_done(big_to_string(heap, value->lit.integer));
    case LITERAL_STRING: return answer_fail(heap_str_cat(heap, str_lit("not a natural number: "), export));
  }
  return answer_fail(heap_str_cat(heap, str_lit("not a natural number: "), export));
}

/* selected (80-83) */
static SliceResult selected(Heap *heap, bool reachable, Vec *rows, Str export) {
  if (reachable) return slice_select(heap, rows, export);
  return slice_done(rows);
}

/* prepare_export (85-93) */
static Exported prepare_export(Heap *heap, Globals globals, Vec *rows, Str export, bool reachable) {
  SliceResult checked = valid_name(export);
  SliceResult value;
  SliceResult normal;
  Answer answer;
  SliceResult chosen;
  SliceResult bytes;
  if (slice_failed(checked)) return exported_fail(checked.message);
  value = kernel(heap, eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, export)));
  if (slice_failed(value)) return exported_fail(value.message);
  normal = kernel(heap, eval_whnf(heap, globals, value.value));
  if (slice_failed(normal)) return exported_fail(normal.message);
  answer = natural(heap, normal.value, export);
  if (answer.kind == SLICE_FAIL) return exported_fail(answer.message);
  chosen = selected(heap, reachable, rows, export);
  if (slice_failed(chosen)) return exported_fail(chosen.message);
  if (chosen.value == NULL) return exported_fail(str_lit(HOST_OOM));
  bytes = kernel(heap, emit_program(heap, chosen.value, export));
  if (slice_failed(bytes)) return exported_fail(bytes.message);
  if (bytes.value == NULL) return exported_fail(str_lit(HOST_OOM));
  return exported_done(answer.text, *(const Str *)bytes.value);
}

/* written (103-109): the print step; exports goes on with the rest when it returns 0. */
static int written(Heap *heap, IoResult result, Str answer, Str export) {
  switch (result.status) {
    case IO_FAIL: return die_text(heap, 1, result.value);
    case IO_DONE: return print_line(heap, heap_str_cat(heap, heap_str_cat(heap, export, str_lit("\t")), answer));
  }
  return die_text(heap, 1, result.value);
}

/* write_export (111-117) */
static int write_export(Heap *heap, Exported result, Str export, Str directory) {
  switch (result.kind) {
    case SLICE_FAIL: return die_text(heap, 1, result.message);
    case SLICE_DONE: {
      Str path = heap_str_cat(heap, heap_str_cat(heap, heap_str_cat(heap, directory, str_lit("/")), export), str_lit(".wasm"));
      return written(heap, io_write_bytes(heap, path, result.bytes), result.answer, export);
    }
  }
  return die_text(heap, 1, result.message);
}

/* exports (119-122): one write_export per name in order; the first non-zero exit code stops it. */
static int exports(Heap *heap, Globals globals, Vec *rows, Str directory, bool reachable, const Vec *names) {
  size_t i;
  for (i = 0; i < names->len; i++) {
    Str name = slice_name_at(names, i);
    int code = write_export(heap, prepare_export(heap, globals, rows, name, reachable), name, directory);
    if (code != 0) return code;
  }
  return 0;
}

/* run_prepared (124-127) */
static int run_prepared(Heap *heap, Prepared result, Str directory, bool reachable, const Vec *names) {
  switch (result.kind) {
    case SLICE_FAIL: return die_text(heap, 1, result.message);
    case SLICE_DONE: return exports(heap, result.globals, result.rows, directory, reachable, names);
  }
  return die_text(heap, 1, result.message);
}

/* run_read (129-132) */
static int run_read(Heap *heap, IoResult result, Str directory, bool reachable, const Vec *names) {
  switch (result.status) {
    case IO_FAIL: return die_text(heap, 1, result.value);
    case IO_DONE: return run_prepared(heap, prepare(heap, result.value, reachable, names), directory, reachable, names);
  }
  return die_text(heap, 1, result.value);
}

/* run (134-137) */
static int run(Heap *heap, Str source, Str directory, bool reachable, const Vec *names) {
  return run_read(heap, io_read_file(heap, source), directory, reachable, names);
}

/* self_test (139-142) */
static int self_test(Heap *heap, SliceResult result) {
  switch (result.kind) {
    case SLICE_DONE: return print_line(heap, str_lit("RUNTIME-SLICE-OK"));
    case SLICE_FAIL: return die_text(heap, 1, result.message);
  }
  return die_text(heap, 1, result.message);
}

/* refused (144-147) */
static SliceResult refused(Prepared result, Str message) {
  switch (result.kind) {
    case SLICE_FAIL: return slice_done(kernel_unit());
    case SLICE_DONE: return slice_fail(message);
  }
  return slice_fail(message);
}

/* axiom_refused (149-152) */
static SliceResult axiom_refused(Prepared result) {
  switch (result.kind) {
    case SLICE_FAIL: return slice_require(str_eq(result.message, str_lit("runtime fixture declares axioms")), str_lit("unreachable axiom diagnostic"));
    case SLICE_DONE: return slice_fail(str_lit("unreachable axiom accepted"));
  }
  return slice_fail(str_lit("unreachable axiom accepted"));
}

/* reachable_test (154-156) */
static SliceResult reachable_test(Heap *heap, const ElabChecked *checked) {
  return reach_self_test(heap, checked->globals, &checked->rows);
}

static const char *const ROOT_NAME[1] = {"root"};

/* runtime_self_test (158-165) */
static SliceResult runtime_self_test(Heap *heap) {
  SliceResult checked = slice_self_test(heap);
  SliceResult source;
  Vec root;
  if (slice_failed(checked)) return checked;
  source = kernel(heap, elab_check_in(heap, global_initial(heap), budget_unlimited(),
                                      str_lit("def hidden : Type := Nat\ndef helper : Nat -> Nat := fun (n : Nat) => n\n"
                                              "def unused : Nat := 99\ndef alias : Nat -> Nat := helper\ndef root : Nat := alias 7\n")));
  if (slice_failed(source)) return source;
  if (source.value == NULL) return slice_fail(str_lit(HOST_OOM));
  checked = reachable_test(heap, source.value);
  if (slice_failed(checked)) return checked;
  root = slice_names(heap, ROOT_NAME, 1u);
  checked = axiom_refused(prepare(heap, str_lit("axiom unused : Nat\ndef root : Nat := 7\n"), true, &root));
  if (slice_failed(checked)) return checked;
  checked = refused(prepare(heap, str_lit("def unused : Nat := Type\ndef root : Nat := 7\n"), true, &root),
                    str_lit("unchecked unreachable declaration"));
  if (slice_failed(checked)) return checked;
  return slice_done(kernel_unit());
}

/* usage (167-168) */
static int usage(void) { return driver_die(1, "usage: prelude_runtime [--reachable] SOURCE DIRECTORY EXPORT..."); }

/* export_names (170-173): argv[first..count) as a List<String> of boxed names. */
static Vec export_names(Heap *heap, char **argv, int first, int count) {
  Vec names = vec_new(heap->arena);
  int i;
  for (i = first; i < count; i++) {
    Str *name = slice_name_new(heap, str_lit(argv[i]));
    if (name == NULL || !vec_push(&names, name)) heap->oom = true;
  }
  return names;
}

/* arguments (175-181) over IO.args() = argv[1..]. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2 && str_eq(str_lit(argv[1]), str_lit("--slice-self-test"))) return self_test(heap, runtime_self_test(heap));
  if (argc >= 2 && str_eq(str_lit(argv[1]), str_lit("--reachable"))) {
    if (argc >= 5) {
      Vec names = export_names(heap, argv, 4, argc);
      return run(heap, str_lit(argv[2]), str_lit(argv[3]), true, &names);
    }
    return usage();
  }
  if (argc >= 4) {
    Vec names = export_names(heap, argv, 3, argc);
    return run(heap, str_lit(argv[1]), str_lit(argv[2]), false, &names);
  }
  return usage();
}

/* D-274a (host only): the driver heap carries the D-261a scratch flag, so the conversion queries of
 * erase_program (the whole program when --reachable is absent) and of eval and emit run in a scratch
 * arena under the budget_unlimited guard and are freed after each query; the verdict is a bool, so no
 * output changes (D-262a reasoning). */
static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code;
  heap.conv_scratch = true;
  code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

/* main (183-186) */
int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
