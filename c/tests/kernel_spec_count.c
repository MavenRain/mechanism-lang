/* Port of bend2/tests/kernel_spec_count.bend (oracle label unit-kernel-spec-count):
 * IO.print(Spec.print()). */
#include "driver.h"

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str report = spec_count_print(&heap);
  int code = 0;
  if (heap.oom) {
    code = driver_host_failure();
  } else {
    code = driver_print(report);
  }
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
