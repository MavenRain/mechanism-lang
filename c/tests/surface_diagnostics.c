/* Port of bend2/tests/surface_diagnostics.bend (oracle label unit-surface-diagnostics): the exact
 * error text of four rejected programs (family members and a recursive poly definition). */
#include "driver.h"
#include "surface.h"

/* exact(result, expected) */
static bool exact(Heap *heap, Result result, const char *expected) {
  if (result.kind == RESULT_DONE) return false;
  return str_eq(error_to_string(heap, result.error), str_lit(expected));
}

/* main() */
static bool suite(Heap *heap) {
  return exact(heap, parser_program(heap, str_lit("poly (u) mu P : Prop with where end")),
               "line 1, column 33: expected a nonempty list of member definitions, found 'end'") &&
         exact(heap, parser_program(heap, str_lit("poly (u) mu P : Prop with where def end")),
               "line 1, column 33: expected 'def' or 'end' in family members, found 'def'") &&
         exact(heap, parser_program(heap, str_lit("poly (u) mu P : Prop with where def 0 end")),
               "line 1, column 33: expected 'def' or 'end' in family members, found 'def'") &&
         exact(heap, parser_program(heap, str_lit("\n\npoly (u) def rec p : Sort u := p")),
               "line 3, column 1: expected a nonrecursive definition or single family after universe binders, "
               "found 'poly'");
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-diagnostics")) : driver_die(1, "FAIL surface-diagnostics");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
