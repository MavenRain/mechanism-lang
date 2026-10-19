/* Port of tests/surface_driver.bend (the native parser driver of dev/c-parser-check.py):
 * parses the one program source argument and prints OK, dies with 1 and the error text on a parse
 * error, or dies with 2 on any other argument count. */
#include "driver.h"
#include "surface.h"

/* respond(result) */
static int respond(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return driver_print(str_lit("OK"));
  return driver_die_text(1, error_to_string(heap, result.error));
}

/* run(args): the arguments after the program name. */
static int run(Heap *heap, int argc, char **argv) {
  if (argc == 2) return respond(heap, parser_program(heap, str_lit(argv[1])));
  return driver_die(2, "expected exactly one program source argument");
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
