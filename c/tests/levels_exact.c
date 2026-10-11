/* Port of bend2/tests/levels_exact.bend (oracle label unit-levels-exact). */
#include "driver.h"

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS exact universe comparison"));
  return driver_die(1, "FAIL exact universe comparison");
}

static bool comparisons_hold(Heap *heap, Big amount) {
  const LevelVar *u = level_var(heap, heap_nat_of_u64(heap, 0));
  const LevelVar *v = level_var(heap, heap_nat_of_u64(heap, 1));
  const LevelVar *big = level_var_offset(heap, amount, u);
  bool imax_zero = level_equal(heap, level_imax(heap, u, level_zero()), level_zero());
  bool imax_succ = level_equal(heap, level_imax(heap, u, level_succ(heap, v)), level_max(heap, u, level_succ(heap, v)));
  bool succ_not_le = !level_le(heap, level_succ(heap, u), u);
  bool le_max = level_le(heap, u, level_max(heap, u, v));
  bool offset_assoc = level_equal(heap, level_var_offset(heap, big_one(), big),
                                  level_var_offset(heap, big_add(heap, amount, big_one()), u));
  bool big_not_le = !level_le(heap, big, level_succ(heap, u));
  return imax_zero && imax_succ && succ_not_le && le_max && offset_assoc && big_not_le;
}

static int test_big(Heap *heap, MaybeBig n) {
  if (!n.some) return driver_die(1, "FAIL universe offset parsing");
  bool ok = comparisons_hold(heap, n.value);
  if (heap->oom) return driver_host_failure();
  return verify(ok);
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  MaybeBig n = big_of_decimal(&heap, str_lit("1234567890123456789012345678901234567890"));
  int code = heap.oom ? driver_host_failure() : test_big(&heap, n);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
