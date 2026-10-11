/* A one-shot arena fault inside Nat.succ must reach the Heap failure boundary. */
#include "driver.h"

void *rt_test_arena_alloc_real(Arena *arena, size_t size);

static size_t allocations;
static size_t fail_at;

void *arena_alloc(Arena *arena, size_t size) {
  allocations++;
  if (allocations == fail_at) return NULL;
  return rt_test_arena_alloc_real(arena, size);
}

int main(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Nat one = heap_nat_of_u64(&heap, 1);
  Nat two = heap_nat_of_u64(&heap, 2);
  ElimData elim = {0};
  elim.scrut = term_var(&heap, one);
  elim.motive.some = true;
  elim.branches = vec_new(&arena);
  Vec rows = vec_new(&arena);
  OrderUnitResult normal = order_valid_elim(&heap, &elim, two, one, &rows);
  if (normal.kind != RESULT_FAIL || heap.oom) {
    arena_free(&arena);
    return driver_die(1, "invalid elimination setup");
  }

  /* order_valid_elim first allocates index + k. Fail only the following
   * allocation of the constant one used by Nat.succ. */
  fail_at = allocations + 2;
  OrderUnitResult injected = order_valid_elim(&heap, &elim, two, one, &rows);
  if (!heap.oom) {
    fprintf(stderr, "FAIL: dropped Nat.succ allocation, result=%d, heap.oom=%d\n",
            injected.kind, heap.oom);
    arena_free(&arena);
    return 1;
  }
  arena_free(&arena);
  return driver_print(str_lit("PASS order arithmetic reports allocation failure"));
}
