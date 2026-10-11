/* Allocation failures must reach the kernel's host-failure boundary. */
#include "kernel.h"
#include <stdio.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);
static size_t until_failure = SIZE_MAX;
static bool injected;

void *arena_alloc(Arena *arena, size_t size) {
  if (until_failure == 0) {
    until_failure = SIZE_MAX;
    injected = true;
    return NULL;
  }
  if (until_failure != SIZE_MAX) until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

int main(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Nat one = heap_nat_of_u64(&heap, 1);
  const Term *term = term_var(&heap, one);
  CircuitOccur task = {one, term};
  Vec tasks = vec_new(&arena);
  if (!vec_push(&tasks, &task) || heap.oom) return 70;
  bool normal = circuit_reads_fields(&heap, &tasks, one);
  until_failure = 1; /* Copy of the worklist succeeds, the field-range sum fails. */
  bool failed = circuit_reads_fields(&heap, &tasks, one);
  printf("field-read normal=%d failed=%d injected=%d oom=%d\n", normal, failed, injected, heap.oom);
  bool ok = normal && injected && heap.oom;
  arena_free(&arena);
  return ok ? 0 : 1;
}
