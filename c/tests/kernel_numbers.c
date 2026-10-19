/* Port of tests/kernel_numbers.bend (oracle label unit-kernel-numbers). */
#include "driver.h"

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS arbitrary precision arithmetic"));
  return driver_die(1, "FAIL arbitrary precision arithmetic");
}

static bool arithmetic_holds(Heap *heap, Big x, Big y) {
  bool sum = str_eq(big_to_string(heap, big_add(heap, x, y)), str_lit("123456789012345678910000000000"));
  bool product = str_eq(big_to_string(heap, big_mul(heap, x, y)), str_lit("1524157875323758556885594913688591961096125479"));
  bool difference = str_eq(big_to_string(heap, big_sub(heap, x, y)), str_lit("123456789012320987552197530422"));
  bool saturated = big_equal(big_sub(heap, y, x), big_zero());
  return sum && product && difference && saturated;
}

static int run(Heap *heap, MaybeBig a, MaybeBig b) {
  if (a.some && b.some) {
    bool ok = arithmetic_holds(heap, a.value, b.value);
    if (heap->oom) return driver_host_failure();
    return verify(ok);
  }
  if (!a.some) return driver_die(1, "FAIL parsing first integer");
  return driver_die(1, "FAIL parsing second integer");
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  MaybeBig a = big_of_decimal(&heap, str_lit("123456789012333333231098765211"));
  MaybeBig b = big_of_decimal(&heap, str_lit("12345678901234789"));
  int code = heap.oom ? driver_host_failure() : run(&heap, a, b);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
