/* Port of bend2/tests/kernel_erase_driver.bend (oracle label erase): checks SOURCE against the initial
 * globals with an unlimited budget, erases the checked program and prints the erased rows. */
#include "driver.h"
#include "surface.h"

/* erased(checked) */
static Result erased(Heap *heap, const ElabChecked *checked) {
  return erase_program(heap, checked->globals, &checked->rows);
}

/* process(source) + respond(result): Done prints Erase.print(rows), Fail dies with 1 and the error text. */
static int process(Heap *heap, Str source) {
  Result checked = elab_check_in(heap, global_initial(heap), budget_unlimited(), source);
  if (checked.kind != RESULT_DONE) return driver_die_text(1, error_to_string(heap, checked.error));
  Result rows = erased(heap, (const ElabChecked *)checked.value);
  if (rows.kind != RESULT_DONE) return driver_die_text(1, error_to_string(heap, rows.error));
  return driver_print(erase_print(heap, (const Vec *)rows.value));
}

/* run(args): argv[1..] are the IO.args() list. */
static int run(Heap *heap, int argc, char **argv) {
  if (argc == 2) return process(heap, str_lit(argv[1]));
  return driver_die(2, "expected SOURCE");
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
