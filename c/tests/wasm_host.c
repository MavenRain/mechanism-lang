/* Port of tests/wasm_host.bend: the host runner checks of cli/host.bend (c/cli/host.c).
 * The number parser, dirname, missing runner and kernel legs run in process; then the driver
 * writes a small module to /private/tmp, runs it under Node and Wasmtime, runs a custom Node
 * runner script over the trap, invalid and bad outputs, and removes every file it wrote. A pass
 * prints `NAME: ok`; the first failed check dies with exit 1 and `NAME: expected E, got A`; a
 * failed file effect dies with exit 1 and the effect message. The steps run in Bend order and the
 * first non-zero exit code stops the run (IO.die). */
#include "cli.h"
#include "driver.h"
#include "wasm.h"

#include <stdarg.h>

/* checked(ok, name, expected, actual) */
static int checked(Heap *heap, bool ok, Str name, Str expected, Str actual) {
  if (heap->oom) return driver_host_failure();
  if (ok) return driver_print(heap_str_cat(heap, name, str_lit(": ok")));
  Str head = heap_str_cat(heap, heap_str_cat(heap, name, str_lit(": expected ")), expected);
  Str message = heap_str_cat(heap, heap_str_cat(heap, head, str_lit(", got ")), actual);
  if (heap->oom) return driver_host_failure();
  return driver_die_text(1, message);
}

/* check(name, expected, actual) */
static int check(Heap *heap, const char *name, const char *expected, Str actual) {
  if (heap->oom) return driver_host_failure();
  return checked(heap, str_eq(str_lit(expected), actual), str_lit(name), str_lit(expected), actual);
}

/* result_text(r) */
static Str result_text(Heap *heap, HostOutcome r) {
  switch (r.kind) {
  case HOST_OUTCOME_VALUE: return heap_str_cat(heap, str_lit("value "), big_to_string(heap, r.number));
  case HOST_OUTCOME_TRAP: return heap_str_cat(heap, str_lit("trap "), r.message);
  case HOST_OUTCOME_INVALID: return heap_str_cat(heap, str_lit("invalid "), r.message);
  }
  return heap_str_cat(heap, str_lit("invalid "), r.message);
}

/* number_text(n) */
static Str number_text(Heap *heap, MaybeBig n) { return n.some ? big_to_string(heap, n.value) : str_lit("none"); }

/* missing_text(n) */
static Str missing_text(HostText n) { return n.some ? n.value : str_lit("none"); }

/* unit_result(r): Done -> IO.pure, Fail{e} -> IO.die(1, e) */
static int unit_result(Heap *heap, IoResult r) {
  if (heap->oom) return driver_host_failure();
  switch (r.status) {
  case IO_DONE: return 0;
  case IO_FAIL: return driver_die_text(1, r.value);
  }
  return driver_die_text(1, r.value);
}

/* write(path, text) */
static int write_text(Heap *heap, Str path, Str text) { return unit_result(heap, io_write_file(heap, path, text)); }

/* remove(path) */
static int remove_path(Heap *heap, Str path) { return unit_result(heap, io_remove(heap, path)); }

/* globals(n) = Global.add("main", Def{DefEntry{Global{"Nat"}, Lit{LInt{of_u32(n)}}, True, None, False}}, initial) */
static Globals globals(Heap *heap, uint32_t n) {
  DefEntry def;
  def.ty = term_global(heap, str_lit("Nat"));
  def.body = term_lit(heap, literal_lint(big_of_u32(heap, n)));
  def.reducible = true;
  def.rec_arg.some = false;
  def.rec_arg.value = nat_zero();
  def.partial = false;
  return global_add(heap, str_lit("main"), global_entry_def(def), global_initial(heap));
}

/* A Bend list literal of boxed items; a NULL item is a failed allocation (heap->oom set). */
static Vec list(Heap *heap, size_t count, ...) {
  Vec out = vec_new(heap->arena);
  va_list items;
  va_start(items, count);
  for (size_t i = 0; i < count; i++) {
    void *item = va_arg(items, void *);
    if (item == NULL || !vec_push(&out, item)) heap->oom = true;
  }
  va_end(items);
  return out;
}

/* module() = encode(Module{[[CFunc{[], [I32]}]], [], [Func{0, [], [I32_const{42}]}],
 *                          [Export{"main", 0}, Export{"quoted ' ;$()", 0}], []}) */
static Str module(Heap *heap) {
  static const U32List declared = {NULL, 0};
  Vec types = list(heap, 1,
                   gc_group_box(heap, list(heap, 1,
                                           gc_comptype_func(heap, list(heap, 0),
                                                            list(heap, 1, gc_valtype_box(heap, gc_valtype_i32()))))));
  Vec funcs = list(heap, 1, gc_func(heap, 0, list(heap, 0), list(heap, 1, gc_instr_index(heap, GC_I32_CONST, 42))));
  Vec exports = list(heap, 2, gc_export(heap, str_lit("main"), 0), gc_export(heap, str_lit("quoted ' ;$()"), 0));
  GcModule *m = heap->oom ? NULL : gc_module(heap, types, list(heap, 0), funcs, exports, declared);
  if (m == NULL) heap->oom = true;
  return m == NULL ? str_lit("") : gc_encode(heap, m);
}

/* custom_script(): the Node runner that prints a value, a trap, an invalid line or junk by argv[3] */
static Str custom_script(void) {
  return str_lit("const op=process.argv[3]; if(op==='value'){console.log('  -42 ')}else if(op==='trap'){console.error("
                 "'\\n  trap: deliberate\\nignored');process.exit(1)}else if(op==='invalid'){console.error('\\ninvalid: "
                 "rejected');process.exit(2)}else{console.log('not a number')}");
}

/* custom(wasm, name, expected) */
static int custom(Heap *heap, Str wasm, const char *name, const char *expected) {
  HostRunner runner = {str_lit("node"), str_lit("mechanism host ' ;$().js")};
  HostOutcome result = host_spawn(heap, runner, str_lit("/private/tmp"), wasm, str_lit(name));
  return check(heap, "custom capture", expected, result_text(heap, result));
}

/* The module legs: write the module, run it under Node and Wasmtime, the custom runner cases,
 * then remove every file written. */
static int module_legs(Heap *heap, Str wasm) {
  int code = unit_result(heap, io_write_bytes(heap, wasm, module(heap)));
  code = code != 0 ? code
                   : check(heap, "Node", "value 42",
                           result_text(heap, host_run_module(heap, str_lit("."), global_initial(heap), HOST_NODE, wasm,
                                                             str_lit("main"))));
  code = code != 0 ? code
                   : check(heap, "quoted argv", "value 42",
                           result_text(heap, host_run_module(heap, str_lit("."), global_initial(heap), HOST_NODE, wasm,
                                                             str_lit("quoted ' ;$()"))));
  code = code != 0 ? code
                   : check(heap, "Wasmtime", "value 42",
                           result_text(heap, host_run_module(heap, str_lit("."), global_initial(heap), HOST_WASMTIME,
                                                             wasm, str_lit("main"))));
  code = code != 0 ? code : write_text(heap, str_lit("/private/tmp/mechanism host ' ;$().js"), custom_script());
  code = code != 0 ? code : custom(heap, wasm, "value", "value -42");
  code = code != 0 ? code : custom(heap, wasm, "trap", "trap deliberate");
  code = code != 0 ? code : custom(heap, wasm, "invalid", "invalid rejected");
  code = code != 0 ? code : custom(heap, wasm, "bad", "invalid runner printed not a number");
  code = code != 0 ? code : remove_path(heap, str_lit("/private/tmp/mechanism host ' ;$().js"));
  code = code != 0 ? code : remove_path(heap, host_out_path(heap, wasm));
  code = code != 0 ? code : remove_path(heap, host_err_path(heap, wasm));
  return code != 0 ? code : remove_path(heap, wasm);
}

static int number_check(Heap *heap, const char *name, const char *expected, const char *text) {
  return check(heap, name, expected, number_text(heap, host_parse_number(heap, str_lit(text))));
}

static int dirname_check(Heap *heap, const char *name, const char *expected, const char *path) {
  return check(heap, name, expected, host_dirname(str_lit(path)));
}

/* main() */
static int run(Heap *heap) {
  static const char missing_root[] = "/private/tmp/mechanism-no-runner-directory";
  Str wasm = str_lit("/private/tmp/mechanism host ' ;$().wasm");
  int code = number_check(heap, "signed number", "-42", "-42");
  code = code != 0 ? code : number_check(heap, "positive sign", "42", "+42");
  code = code != 0 ? code : number_check(heap, "underscores", "12", "1__2_");
  code = code != 0 ? code : number_check(heap, "bad underscore", "none", "_1");
  code = code != 0 ? code : number_check(heap, "base hex", "42", "0x2a");
  code = code != 0 ? code : number_check(heap, "base binary", "2", "0b1_0");
  code = code != 0 ? code : number_check(heap, "unsigned wrap", "-4611686018427387904", "0u4611686018427387904");
  code = code != 0 ? code : number_check(heap, "signed minimum", "-4611686018427387904", "-4611686018427387904");
  code = code != 0 ? code : number_check(heap, "signed overflow", "none", "4611686018427387904");
  code = code != 0 ? code : number_check(heap, "unsigned overflow", "none", "0xffffffffffffffff");
  code = code != 0 ? code : dirname_check(heap, "relative dirname", "a", "a//b/");
  code = code != 0 ? code : dirname_check(heap, "root dirname", "/", "/a");
  code = code != 0 ? code : dirname_check(heap, "empty dirname", ".", "");
  code = code != 0 ? code
                   : check(heap, "missing runner", "kanon: run: cannot find dev/run-node.mjs beside the executable",
                           missing_text(host_missing_runner(heap, str_lit(missing_root), HOST_NODE)));
  code = code != 0 ? code
                   : check(heap, "kernel needs no runner", "none",
                           missing_text(host_missing_runner(heap, str_lit(missing_root), HOST_KERNEL)));
  code = code != 0 ? code
                   : check(heap, "kernel value", "value 42",
                           result_text(heap, host_kernel_outcome(heap, globals(heap, 42), str_lit("main"))));
  code = code != 0 ? code
                   : check(heap, "kernel bound", "trap 1073741824 is outside the i31 range",
                           result_text(heap, host_kernel_outcome(heap, globals(heap, 1073741824u), str_lit("main"))));
  return code != 0 ? code : module_legs(heap, wasm);
}

static int entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
