/* Port of bend2/tests/surface_check_driver.bend (oracle label surface-check): checks SOURCE against the
 * initial globals, or against the empty globals under --empty, and prints the checked form of the rows. */
#include "driver.h"
#include "surface.h"

/* respond(result) */
static int respond(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return driver_print(elab_checked_form(heap, (const Vec *)result.value));
  return driver_die_text(1, error_to_string(heap, result.error));
}

/* run_empty(valid, source) */
static int run_empty(Heap *heap, bool valid, Str source) {
  if (valid) return respond(heap, elab_check_text_compact(heap, global_empty(), source));
  return driver_die(2, "expected SOURCE or --empty SOURCE");
}

/* run(args): argv[1..] are the IO.args() list. */
static int run(Heap *heap, int argc, char **argv) {
  if (argc == 2) return respond(heap, elab_check_text_compact(heap, global_initial(heap), str_lit(argv[1])));
  if (argc == 3) return run_empty(heap, str_eq(str_lit(argv[1]), str_lit("--empty")), str_lit(argv[2]));
  return driver_die(2, "expected SOURCE or --empty SOURCE");
}

/* main() */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
