/* Port of bend2/cli/kanon.bend (the kanon command line): check, erase, list axioms, emit WasmGC, build a
 * multi-file module, run an export on the node, wasmtime and kernel hosts, count circuit bounds and
 * print the spec count. Every IO(A) of the Bend program is a KanonIo here: live with the value, or the
 * program has already exited with code (IO.die, Out.cli_exit or a host failure after an out-of-memory
 * heap), and every later step of the do block is skipped. Argument lists are a Vec of Str pointers read
 * from index on (List<&2,String> in Bend list order). */
#include <stdio.h>
#include "cli.h"
#include "surface.h"
#include "wasm.h"

typedef struct {
  bool live;
  int code;
  const void *value;
} KanonIo;

static KanonIo io_pure(const void *value) {
  KanonIo r = {true, 0, value};
  return r;
}

static KanonIo io_died(int code) {
  KanonIo r = {false, code, NULL};
  return r;
}

static KanonIo io_unit(void) { return io_pure(NULL); }

static KanonIo io_host_failure(void) { return io_died(cli_host_failure()); }

static KanonIo io_die(uint32_t code, Str message) { return io_died(cli_die(code, message)); }

static Str cat(Heap *heap, Str a, Str b) { return heap_str_cat(heap, a, b); }

/* IO.pure(String, text): the text in a heap cell. */
static KanonIo io_text(Heap *heap, Str text) {
  Str *cell = heap_alloc(heap, sizeof(Str));
  if (cell == NULL) return io_host_failure();
  *cell = text;
  return io_pure(cell);
}

static Str text_of(KanonIo r) { return *(const Str *)r.value; }

/* IO.write(text): the text on stdout with no newline. */
static KanonIo io_write(Heap *heap, Str text) {
  if (heap->oom) return io_host_failure();
  if (text.len > 0) fwrite(text.data, 1, text.len, stdout);
  return io_unit();
}

/* args[index], or the empty string past the end (callers test the length first). */
static Str arg_at(const Vec *args, size_t index) {
  VecGet got = vec_get(args, index);
  switch (got.status) {
  case VEC_GET_OK: return *(const Str *)got.value;
  case VEC_GET_OUT_OF_RANGE: return str_empty();
  }
  return str_empty();
}

static size_t args_left(const Vec *args, size_t index) { return index < args->len ? args->len - index : 0; }

/* usage_text (26-27) */
Str kanon_usage_text(void) {
  return str_lit("usage: kanon check [--print|--erased] FILE | axioms FILE | circuit FILE | emit FILE -o OUT.wasm --export NAME | run FILE --export NAME [--host node|wasmtime|kernel|both] | build FILE... -o OUT.wasm --export NAME... | spec-count");
}

/* usage (29-30) */
static KanonIo usage(void) { return io_die(64, kanon_usage_text()); }

/* accept (32-35) */
static KanonIo accept(Heap *heap, Result result, uint32_t code, Str prefix) {
  if (heap->oom) return io_host_failure();
  switch (result.kind) {
  case RESULT_DONE: return io_pure(result.value);
  case RESULT_FAIL: return io_die(code, cat(heap, prefix, error_to_string(heap, result.error)));
  }
  return io_die(code, cat(heap, prefix, error_to_string(heap, result.error)));
}

/* read_result (37-40) */
static KanonIo read_result(Heap *heap, IoResult result, Str path) {
  if (heap->oom) return io_host_failure();
  switch (result.status) {
  case IO_DONE: return io_text(heap, result.value);
  case IO_FAIL: return io_die(64, cat(heap, str_lit("kanon: cannot read "), path));
  }
  return io_die(64, cat(heap, str_lit("kanon: cannot read "), path));
}

/* read_file (42-45) */
static KanonIo read_file(Heap *heap, Str path) { return read_result(heap, io_read_file(heap, path), path); }

/* checked_in (47-50): the value is a const ElabChecked *. */
static KanonIo checked_in(Heap *heap, Str path) {
  KanonIo source = read_file(heap, path);
  if (!source.live) return source;
  return accept(heap, elab_check_in(heap, global_initial(heap), budget_unlimited(), text_of(source)), 1, str_empty());
}

/* checked_rows (52-54) */
static const Vec *checked_rows(const ElabChecked *c) { return &c->rows; }

/* checked (56-59): the value is the const Vec * of rows. */
static KanonIo checked(Heap *heap, Str path) {
  KanonIo c = checked_in(heap, path);
  if (!c.live) return c;
  return io_pure(checked_rows((const ElabChecked *)c.value));
}

/* print_checked (61-64) */
static KanonIo print_checked(Heap *heap, bool print_form, const Vec *rows) {
  if (print_form) return io_write(heap, elab_checked_form(heap, rows));
  return io_unit();
}

/* run_check (66-69) */
static KanonIo run_check(Heap *heap, bool print_form, Str path) {
  KanonIo rows = checked(heap, path);
  if (!rows.live) return rows;
  return print_checked(heap, print_form, (const Vec *)rows.value);
}

/* erase_checked (71-73): the value is the const Vec * of erased rows. */
static KanonIo erase_checked(Heap *heap, const ElabChecked *c) {
  return accept(heap, erase_program(heap, c->globals, &c->rows), 1, str_empty());
}

/* run_erased (75-79) */
static KanonIo run_erased(Heap *heap, Str path) {
  KanonIo c = checked_in(heap, path);
  if (!c.live) return c;
  KanonIo erased = erase_checked(heap, (const ElabChecked *)c.value);
  if (!erased.live) return erased;
  return io_write(heap, erase_print(heap, (const Vec *)erased.value));
}

/* print_lines (81-87): one IO.print per line, in order. */
static KanonIo print_lines(Heap *heap, const Vec *lines, size_t index) {
  if (heap->oom) return io_host_failure();
  if (index >= lines->len) return io_unit();
  cli_print(arg_at(lines, index));
  return print_lines(heap, lines, index + 1);
}

/* run_axioms (89-92) */
static KanonIo run_axioms(Heap *heap, Str path) {
  KanonIo rows = checked(heap, path);
  if (!rows.live) return rows;
  Vec names = elab_disclosure_names(heap, (const Vec *)rows.value);
  return print_lines(heap, &names, 0);
}

/* ensure_output (94-97) */
static KanonIo ensure_output(Heap *heap, bool found, Str out) {
  if (found) return io_unit();
  return io_die(64, cat(heap, str_lit("kanon: cannot write "), out));
}

/* ensure_write (99-102) */
static KanonIo ensure_write(Heap *heap, Str out) { return ensure_output(heap, io_exists(heap, host_dirname(out)), out); }

/* wrote (104-107) */
static KanonIo wrote(Heap *heap, IoResult result, Str out) {
  if (heap->oom) return io_host_failure();
  switch (result.status) {
  case IO_DONE: return io_unit();
  case IO_FAIL: return io_die(64, cat(heap, str_lit("kanon: cannot write "), out));
  }
  return io_die(64, cat(heap, str_lit("kanon: cannot write "), out));
}

/* write_bytes (109-112): bytes = the List<U32> of byte values as a byte string. */
static KanonIo write_bytes(Heap *heap, Str out, Str bytes) { return wrote(heap, io_write_bytes(heap, out, bytes), out); }

/* run_emit (114-120) */
static KanonIo run_emit(Heap *heap, Str path, Str out, Str export_name) {
  KanonIo ready = ensure_write(heap, out);
  if (!ready.live) return ready;
  KanonIo c = checked_in(heap, path);
  if (!c.live) return c;
  KanonIo erased = erase_checked(heap, (const ElabChecked *)c.value);
  if (!erased.live) return erased;
  KanonIo bytes = accept(heap, emit_program(heap, (const Vec *)erased.value, export_name), 2, str_lit("kanon: emit: "));
  if (!bytes.live) return bytes;
  return write_bytes(heap, out, text_of(bytes));
}

/* dispatch_emit (122-125): exactly PATH -o OUT --export NAME. */
static KanonIo dispatch_emit(Heap *heap, const Vec *args, size_t index) {
  if (args_left(args, index) == 5 && str_eq(arg_at(args, index + 1), str_lit("-o")) && str_eq(arg_at(args, index + 3), str_lit("--export")))
    return run_emit(heap, arg_at(args, index), arg_at(args, index + 2), arg_at(args, index + 4));
  return usage();
}

/* read_files (127-135): the files joined by one newline, read in order; the first failure stops. */
static KanonIo read_files(Heap *heap, const Vec *paths, size_t index) {
  size_t left = args_left(paths, index);
  if (left == 0) return io_text(heap, str_empty());
  if (left == 1) return read_file(heap, arg_at(paths, index));
  KanonIo first = read_file(heap, arg_at(paths, index));
  if (!first.live) return first;
  KanonIo more = read_files(heap, paths, index + 1);
  if (!more.live) return more;
  return io_text(heap, cat(heap, cat(heap, text_of(first), str_lit("\n")), text_of(more)));
}

/* The boxed Str at args[index] (NULL past the end; callers test the length first). */
static void *arg_cell(const Vec *args, size_t index) {
  VecGet got = vec_get(args, index);
  switch (got.status) {
  case VEC_GET_OK: return got.value;
  case VEC_GET_OUT_OF_RANGE: return NULL;
  }
  return NULL;
}

static void push_cell(Heap *heap, Vec *vec, void *cell) {
  if (!vec_push(vec, cell)) heap->oom = true;
}

static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

/* run_build (137-144): paths and exports in command line order (Vec of Str pointers). */
static KanonIo run_build(Heap *heap, const Vec *paths, Str out, const Vec *exports) {
  KanonIo ready = ensure_write(heap, out);
  if (!ready.live) return ready;
  KanonIo source = read_files(heap, paths, 0);
  if (!source.live) return source;
  KanonIo c = accept(heap, elab_check_in(heap, global_initial(heap), budget_unlimited(), text_of(source)), 1, str_empty());
  if (!c.live) return c;
  KanonIo rows = erase_checked(heap, (const ElabChecked *)c.value);
  if (!rows.live) return rows;
  KanonIo bytes = accept(heap, emit_reactor(heap, (const Vec *)rows.value, exports), 2, str_lit("kanon: build: "));
  if (!bytes.live) return bytes;
  return write_bytes(heap, out, text_of(bytes));
}

/* The build flags read so far. The Bend code prepends paths and exports and reverses them in
 * build_finish; the C port appends them, which gives the same command line order. */
typedef struct {
  Vec paths;
  bool has_out;
  Str out;
  Vec exports;
} BuildFlags;

/* build_finish (146-151) */
static KanonIo build_finish(Heap *heap, const BuildFlags *flags) {
  if (flags->paths.len == 0) return usage();
  if (!flags->has_out) return usage();
  if (flags->exports.len == 0) return usage();
  return run_build(heap, &flags->paths, flags->out, &flags->exports);
}

static KanonIo build_flags(Heap *heap, BuildFlags *flags, const Vec *args, size_t index);

/* build_out (160-163): a second -o is a usage error. */
static KanonIo build_out(Heap *heap, BuildFlags *flags, Str output, const Vec *args, size_t rest) {
  if (flags->has_out) return usage();
  flags->has_out = true;
  flags->out = output;
  return build_flags(heap, flags, args, rest);
}

/* build_path (165-168): an unknown flag (a leading '-') is a usage error. */
static KanonIo build_path(Heap *heap, bool flag, void *path, BuildFlags *flags, const Vec *args, size_t rest) {
  if (flag) return usage();
  push_cell(heap, &flags->paths, path);
  return build_flags(heap, flags, args, rest);
}

/* build_flags (170-177) */
static KanonIo build_flags(Heap *heap, BuildFlags *flags, const Vec *args, size_t index) {
  size_t left = args_left(args, index);
  if (left == 0) return build_finish(heap, flags);
  Str head = arg_at(args, index);
  bool is_out = str_eq(head, str_lit("-o"));
  bool is_export = str_eq(head, str_lit("--export"));
  if (left >= 2 && is_out) return build_out(heap, flags, arg_at(args, index + 1), args, index + 2);
  if (left >= 2 && is_export) {
    push_cell(heap, &flags->exports, arg_cell(args, index + 1));
    return build_flags(heap, flags, args, index + 2);
  }
  if (is_out || is_export) return usage();
  return build_path(heap, str_starts_with(head, str_lit("-")), arg_cell(args, index), flags, args, index + 1);
}

/* dispatch_build (179-180) */
static KanonIo dispatch_build(Heap *heap, const Vec *args, size_t index) {
  BuildFlags flags = {vec_new(heap->arena), false, str_empty(), vec_new(heap->arena)};
  return build_flags(heap, &flags, args, index);
}

/* Mode (182-184): One{host} | Both{} */
typedef enum { MODE_ONE, MODE_BOTH } ModeKind;
typedef struct {
  ModeKind kind;
  HostKind host; /* MODE_ONE */
} Mode;

static Mode mode_one(HostKind host) {
  Mode m = {MODE_ONE, host};
  return m;
}

static Mode mode_both(void) {
  Mode m = {MODE_BOTH, HOST_NODE};
  return m;
}

/* add_rows (186-189): Global.add of every row, in order. */
static Globals add_rows(Heap *heap, const Vec *rows, size_t index, Globals g) {
  VecGet got = vec_get(rows, index);
  switch (got.status) {
  case VEC_GET_OK: {
    const CheckRow *row = got.value;
    return add_rows(heap, rows, index + 1, global_add(heap, row->name, row->entry, g));
  }
  case VEC_GET_OUT_OF_RANGE: return g;
  }
  return g;
}

/* globals_of (191-192) */
static Globals globals_of(Heap *heap, const Vec *rows) { return add_rows(heap, rows, 0, global_initial(heap)); }

/* C.Pair<Global.T, List<U32>>: the globals of the checked rows and the module bytes. */
typedef struct {
  Globals globals;
  Str bytes;
} KanonModule;

/* module_checked (194-200): the value is a const KanonModule *. */
static KanonIo module_checked(Heap *heap, const ElabChecked *c, Str export_name) {
  KanonIo erased = erase_checked(heap, c);
  if (!erased.live) return erased;
  KanonIo bytes = accept(heap, emit_program(heap, (const Vec *)erased.value, export_name), 2, str_lit("kanon: emit: "));
  if (!bytes.live) return bytes;
  KanonModule *m = heap_alloc(heap, sizeof(KanonModule));
  if (m == NULL) return io_host_failure();
  m->globals = globals_of(heap, &c->rows);
  m->bytes = text_of(bytes);
  return io_pure(m);
}

/* module_bytes (202-205) */
static KanonIo module_bytes(Heap *heap, Str path, Str export_name) {
  KanonIo c = checked_in(heap, path);
  if (!c.live) return c;
  return module_checked(heap, (const ElabChecked *)c.value, export_name);
}

/* One path of cleanup (210-214): F.exists, then F.remove_if_exists; the result is dropped. */
static void cleanup_path(Heap *heap, Str path) {
  bool exists = io_exists(heap, path);
  IoResult removed = io_remove_if_exists(heap, exists, path);
  (void)removed;
}

/* cleanup (207-214) over paths[index..] */
static KanonIo cleanup(Heap *heap, const Vec *paths, size_t index) {
  if (index >= paths->len) return io_unit();
  cleanup_path(heap, arg_at(paths, index));
  return cleanup(heap, paths, index + 1);
}

/* cleanup(wasm <> Host.captures(wasm)) */
static KanonIo cleanup_module(Heap *heap, Str wasm) {
  Vec captures = host_captures(heap, wasm);
  cleanup_path(heap, wasm);
  return cleanup(heap, &captures, 0);
}

/* answer (216-217) */
static KanonIo answer(Heap *heap, Big n) {
  Str text = big_to_string(heap, n);
  if (heap->oom) return io_host_failure();
  cli_print(text);
  return io_unit();
}

/* refused (219-220) */
static KanonIo refused(Heap *heap, HostKind h, Str m) {
  return io_die(2, cat(heap, cat3(heap, str_lit("kanon: run: "), host_name(h), str_lit(" invalid: ")), m));
}

/* show (222-226) */
static Str show(Heap *heap, HostOutcome o) {
  switch (o.kind) {
  case HOST_OUTCOME_VALUE: return big_to_string(heap, o.number);
  case HOST_OUTCOME_TRAP: return str_lit("trap");
  case HOST_OUTCOME_INVALID: return str_lit("invalid");
  }
  return str_lit("invalid");
}

/* disagree (228-229) */
static KanonIo disagree(Heap *heap, HostOutcome a, HostOutcome b) {
  Str head = cat(heap, str_lit("kanon: run: hosts disagree: node "), show(heap, a));
  return io_die(3, cat3(heap, head, str_lit(" wasmtime "), show(heap, b)));
}

/* report_one (231-235) */
static KanonIo report_one(Heap *heap, HostKind h, HostOutcome o) {
  if (heap->oom) return io_host_failure();
  switch (o.kind) {
  case HOST_OUTCOME_VALUE: return answer(heap, o.number);
  case HOST_OUTCOME_TRAP:
    return io_die(4, cat(heap, cat3(heap, str_lit("kanon: run: "), host_name(h), str_lit(" trap: ")), o.message));
  case HOST_OUTCOME_INVALID: return refused(heap, h, o.message);
  }
  return refused(heap, h, o.message);
}

/* report_equal (237-240) */
static KanonIo report_equal(Heap *heap, bool same, Big x, HostOutcome a, HostOutcome b) {
  if (same) return answer(heap, x);
  return disagree(heap, a, b);
}

/* report_both (242-250) with a = Value{x} */
static KanonIo report_value(Heap *heap, HostOutcome a, HostOutcome b) {
  switch (b.kind) {
  case HOST_OUTCOME_VALUE: return report_equal(heap, big_equal(a.number, b.number), a.number, a, b);
  case HOST_OUTCOME_TRAP: return disagree(heap, a, b);
  case HOST_OUTCOME_INVALID: return refused(heap, HOST_WASMTIME, b.message);
  }
  return refused(heap, HOST_WASMTIME, b.message);
}

/* report_both (242-250) with a = Trap{m} */
static KanonIo report_trap(Heap *heap, HostOutcome a, HostOutcome b) {
  switch (b.kind) {
  case HOST_OUTCOME_VALUE: return disagree(heap, a, b);
  case HOST_OUTCOME_TRAP: return io_die(4, str_lit("kanon: run: trap on both hosts"));
  case HOST_OUTCOME_INVALID: return refused(heap, HOST_WASMTIME, b.message);
  }
  return refused(heap, HOST_WASMTIME, b.message);
}

/* report_both (242-250) */
static KanonIo report_both(Heap *heap, HostOutcome a, HostOutcome b) {
  if (heap->oom) return io_host_failure();
  switch (a.kind) {
  case HOST_OUTCOME_VALUE: return report_value(heap, a, b);
  case HOST_OUTCOME_TRAP: return report_trap(heap, a, b);
  case HOST_OUTCOME_INVALID: return refused(heap, HOST_NODE, a.message);
  }
  return refused(heap, HOST_NODE, a.message);
}

/* need_missing (252-255) */
static KanonIo need_missing(HostText m) {
  if (m.some) return io_die(64, m.value);
  return io_unit();
}

/* need_runners (257-264) over hosts[index..count] */
static KanonIo need_runners(Heap *heap, const HostKind *hosts, size_t count, size_t index, Str root) {
  if (index >= count) return io_unit();
  KanonIo missing = need_missing(host_missing_runner(heap, root, hosts[index]));
  if (!missing.live) return missing;
  return need_runners(heap, hosts, count, index + 1, root);
}

/* need_runners(mode_hosts(m), root) (266-269): One{h} = [h], Both = [Node, Wasmtime]. */
static KanonIo need_mode_runners(Heap *heap, Mode m, Str root) {
  HostKind one[1] = {m.host};
  HostKind both[2] = {HOST_NODE, HOST_WASMTIME};
  switch (m.kind) {
  case MODE_ONE: return need_runners(heap, one, 1, 0, root);
  case MODE_BOTH: return need_runners(heap, both, 2, 0, root);
  }
  return need_runners(heap, both, 2, 0, root);
}

/* run_hosts (271-283) */
static KanonIo run_hosts(Heap *heap, Mode m, Str root, Globals globals, Str wasm, Str export_name) {
  switch (m.kind) {
  case MODE_ONE: {
    HostOutcome o = host_run_module(heap, root, globals, m.host, wasm, export_name);
    KanonIo cleaned = cleanup_module(heap, wasm);
    if (!cleaned.live) return cleaned;
    return report_one(heap, m.host, o);
  }
  case MODE_BOTH: {
    HostOutcome node = host_run_module(heap, root, globals, HOST_NODE, wasm, export_name);
    HostOutcome wasmtime = host_run_module(heap, root, globals, HOST_WASMTIME, wasm, export_name);
    KanonIo cleaned = cleanup_module(heap, wasm);
    if (!cleaned.live) return cleaned;
    return report_both(heap, node, wasmtime);
  }
  }
  return usage();
}

/* temp_result (285-288): the value is the temporary path. */
static KanonIo temp_result(Heap *heap, IoResult result) {
  if (heap->oom) return io_host_failure();
  switch (result.status) {
  case IO_DONE: return io_text(heap, result.value);
  case IO_FAIL: return io_die(64, cat(heap, str_lit("kanon: run: "), result.value));
  }
  return io_die(64, cat(heap, str_lit("kanon: run: "), result.value));
}

/* run_compiled (290-297) */
static KanonIo run_compiled(Heap *heap, const KanonModule *c, Str root, Str export_name, Mode m) {
  KanonIo temp = temp_result(heap, io_temp_file(heap, str_lit("kanon-run-"), str_lit(".wasm")));
  if (!temp.live) return temp;
  Str wasm = text_of(temp);
  KanonIo written = write_bytes(heap, wasm, c->bytes);
  if (!written.live) return written;
  return run_hosts(heap, m, root, c->globals, wasm, export_name);
}

/* run_existing (299-307) */
static KanonIo run_existing(Heap *heap, bool exists, Str path, Str export_name, Mode m) {
  if (!exists)
    return io_die(64, cat(heap, cat3(heap, str_lit("kanon: run: cannot read "), path, str_lit("\n")), kanon_usage_text()));
  Str root = host_root(heap);
  KanonIo runners = need_mode_runners(heap, m, root);
  if (!runners.live) return runners;
  KanonIo c = module_bytes(heap, path, export_name);
  if (!c.live) return c;
  return run_compiled(heap, (const KanonModule *)c.value, root, export_name, m);
}

/* run_run (309-312) */
static KanonIo run_run(Heap *heap, Str path, Str export_name, Mode m) {
  return run_existing(heap, io_exists(heap, path), path, export_name, m);
}

/* dispatch_run (314-321): PATH --export NAME [--host node|wasmtime|kernel|both] and nothing else. */
static KanonIo dispatch_run(Heap *heap, const Vec *args, size_t index) {
  size_t left = args_left(args, index);
  Str path = arg_at(args, index);
  Str name = arg_at(args, index + 2);
  Str host = arg_at(args, index + 4);
  bool exported = (left == 3 || left == 5) && str_eq(arg_at(args, index + 1), str_lit("--export"));
  bool hosted = exported && left == 5 && str_eq(arg_at(args, index + 3), str_lit("--host"));
  if (exported && left == 3) return run_run(heap, path, name, mode_both());
  if (hosted && str_eq(host, str_lit("node"))) return run_run(heap, path, name, mode_one(HOST_NODE));
  if (hosted && str_eq(host, str_lit("wasmtime"))) return run_run(heap, path, name, mode_one(HOST_WASMTIME));
  if (hosted && str_eq(host, str_lit("kernel"))) return run_run(heap, path, name, mode_one(HOST_KERNEL));
  if (hosted && str_eq(host, str_lit("both"))) return run_run(heap, path, name, mode_both());
  return usage();
}

/* dispatch_check (323-330) */
static KanonIo dispatch_check(Heap *heap, const Vec *args, size_t index) {
  size_t left = args_left(args, index);
  if (left == 0) return usage();
  Str head = arg_at(args, index);
  bool print_form = str_eq(head, str_lit("--print"));
  bool erased = str_eq(head, str_lit("--erased"));
  if (print_form && left >= 2) return run_check(heap, true, arg_at(args, index + 1));
  if (erased && left >= 2) return run_erased(heap, arg_at(args, index + 1));
  if (print_form || erased) return usage();
  return run_check(heap, false, head);
}

/* dispatch_axioms (332-335) */
static KanonIo dispatch_axioms(Heap *heap, const Vec *args, size_t index) {
  if (args_left(args, index) == 0) return usage();
  return run_axioms(heap, arg_at(args, index));
}

/* definition_rows (337-341): the Def rows as a Vec of CircuitGlobal pointers, in order. */
static void definition_rows(Heap *heap, const EntryRows *rows, Vec *out) {
  if (rows == NULL) return;
  switch (rows->entry.kind) {
  case ENTRY_DEF: {
    CircuitGlobal *g = heap_alloc(heap, sizeof(CircuitGlobal));
    if (g == NULL) {
      heap->oom = true;
      return;
    }
    g->name = rows->name;
    g->term = rows->entry.def.body;
    push_cell(heap, out, g);
    break;
  }
  case ENTRY_AXIOM: break;
  case ENTRY_PRIM: break;
  }
  definition_rows(heap, rows->rest, out);
}

/* global_lookup (343-345) */
static Vec global_lookup(Heap *heap, Globals g) {
  Vec out = vec_new(heap->arena);
  definition_rows(heap, g.entries, &out);
  return out;
}

static const bool kanon_true = true;
static const bool kanon_false = false;

static KanonIo io_bool(bool b) { return io_pure(b ? &kanon_true : &kanon_false); }

static bool bool_of(KanonIo r) { return *(const bool *)r.value; }

/* circuit_line (347-356): the value is the bad flag. */
static KanonIo circuit_line(Heap *heap, Str name, CircuitBigResult result, bool bad) {
  if (heap->oom) return io_host_failure();
  switch (result.kind) {
  case RESULT_DONE: {
    Str line = cat3(heap, name, str_lit(": depth "), big_to_string(heap, result.value));
    if (heap->oom) return io_host_failure();
    cli_print(line);
    return io_bool(bad);
  }
  case RESULT_FAIL: {
    Str line = cat3(heap, name, str_lit(": refused: "), result.error);
    if (heap->oom) return io_host_failure();
    cli_print(line);
    return io_bool(true);
  }
  }
  return io_bool(true);
}

/* circuit_rows (358-365) over rows[index..] (CheckRow pointers) */
static KanonIo circuit_rows(Heap *heap, const Vec *rows, size_t index, const Vec *globals, bool bad) {
  VecGet got = vec_get(rows, index);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return io_bool(bad);
  case VEC_GET_OK: {
    const CheckRow *row = got.value;
    switch (row->entry.kind) {
    case ENTRY_DEF: {
      KanonIo refused_row = circuit_line(heap, row->name, circuit_depth(heap, globals, row->entry.def.body), bad);
      if (!refused_row.live) return refused_row;
      return circuit_rows(heap, rows, index + 1, globals, bool_of(refused_row));
    }
    case ENTRY_AXIOM: return circuit_rows(heap, rows, index + 1, globals, bad);
    case ENTRY_PRIM: return circuit_rows(heap, rows, index + 1, globals, bad);
    }
    return circuit_rows(heap, rows, index + 1, globals, bad);
  }
  }
  return io_bool(bad);
}

/* circuit_exit (367-370) */
static KanonIo circuit_exit(bool bad) {
  if (bad) return io_died(cli_exit(1));
  return io_unit();
}

/* circuit_checked (372-377) */
static KanonIo circuit_checked(Heap *heap, const ElabChecked *c) {
  Vec globals = global_lookup(heap, c->globals);
  KanonIo bad = circuit_rows(heap, &c->rows, 0, &globals, false);
  if (!bad.live) return bad;
  return circuit_exit(bool_of(bad));
}

/* run_circuit (379-382) */
static KanonIo run_circuit(Heap *heap, Str path) {
  KanonIo c = checked_in(heap, path);
  if (!c.live) return c;
  return circuit_checked(heap, (const ElabChecked *)c.value);
}

/* dispatch_circuit (384-387) */
static KanonIo dispatch_circuit(Heap *heap, const Vec *args, size_t index) {
  if (args_left(args, index) == 0) return usage();
  return run_circuit(heap, arg_at(args, index));
}

/* dispatch_args (389-398) */
static KanonIo dispatch_args(Heap *heap, Str cmd, const Vec *args, size_t index) {
  if (str_eq(cmd, str_lit("spec-count"))) return io_write(heap, spec_count_print(heap));
  if (str_eq(cmd, str_lit("check"))) return dispatch_check(heap, args, index);
  if (str_eq(cmd, str_lit("axioms"))) return dispatch_axioms(heap, args, index);
  if (str_eq(cmd, str_lit("circuit"))) return dispatch_circuit(heap, args, index);
  if (str_eq(cmd, str_lit("emit"))) return dispatch_emit(heap, args, index);
  if (str_eq(cmd, str_lit("build"))) return dispatch_build(heap, args, index);
  if (str_eq(cmd, str_lit("run"))) return dispatch_run(heap, args, index);
  return usage();
}

/* The process exit code of a finished IO(Unit): 0 when it ran to the end. */
static int exit_code(KanonIo r) { return r.live ? 0 : r.code; }

/* usage (29-30) as a program. */
int kanon_usage(Heap *heap) {
  (void)heap;
  return exit_code(usage());
}

/* dispatch (405-406): reusable(args) is the identity on the argument Vec here. */
int kanon_dispatch(Heap *heap, Str cmd, const Vec *args, size_t index) { return exit_code(dispatch_args(heap, cmd, args, index)); }
