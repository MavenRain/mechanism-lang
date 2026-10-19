/* Port of tests/kernel_quantity.bend (oracle label unit-kernel-quantity). */
#include "driver.h"

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS linear path usage"));
  return driver_die(1, "FAIL linear path usage");
}

/* Bool.and is strict in Bend: every operand is computed before the conjunction. */
static bool linear_path_usage(Heap *heap) {
  Nat zero = nat_zero();
  Usage once = quantity_occurrence(heap, zero, QUANTITY_ONE);
  Usage dead = quantity_sequence(heap, once, quantity_unreachable(heap));
  bool both_once = quantity_exactly_once(zero, quantity_alternative(heap, once, once));
  bool once_or_empty = !quantity_exactly_once(zero, quantity_alternative(heap, once, quantity_empty(heap)));
  bool twice = !quantity_at_most_once(zero, quantity_sequence(heap, once, once));
  bool once_or_dead = quantity_exactly_once(zero, quantity_alternative(heap, once, dead));
  bool captured = quantity_used(zero, quantity_captures(heap, dead));
  bool scaled_out = !quantity_used(zero, quantity_scale(heap, QUANTITY_ZERO, once));
  return both_once && once_or_empty && twice && once_or_dead && captured && scaled_out;
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  bool ok = linear_path_usage(&heap);
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
