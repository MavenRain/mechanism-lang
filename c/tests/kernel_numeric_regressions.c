/* Port of tests/kernel_numeric_regressions.bend: the composite of Wide (kernel_large_addresses),
 * Projection (kernel_projection_motive), Conversion (kernel_conversion_budget) and Levels (levels_protocol)
 * run in that order on one heap; IO.die in any suite ends the sequence with its code. No _bend2 oracle
 * artifact exists for this driver (W/oracle/no-oracle-drivers.txt). */
#include "driver.h"
#include "levels_protocol.h"

static int run(Heap *heap) {
  int code = kernel_large_addresses_run(heap);
  if (code != 0 || heap->oom) return code;
  code = kernel_projection_motive_run(heap);
  if (code != 0 || heap->oom) return code;
  code = kernel_conversion_budget_run(heap);
  if (code != 0 || heap->oom) return code;
  return levels_protocol_run(heap);
}

/* arguments(args) ignores args; main() = arguments([]). */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
