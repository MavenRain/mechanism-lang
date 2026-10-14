/* Allocation failures must not produce a successful, incomplete resolved crate. */
#include <stdio.h>
#include "rust.h"

void *rt_test_arena_alloc_real(Arena *arena, size_t size);
static size_t review_allocations;
static size_t review_until_failure = SIZE_MAX;
static bool review_injected;

void *arena_alloc(Arena *arena, size_t size) {
  review_allocations += 1;
  if (review_until_failure == 0) {
    review_until_failure = SIZE_MAX;
    review_injected = true;
    return NULL;
  }
  if (review_until_failure != SIZE_MAX) review_until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

static size_t review_resolve(size_t fail_at, bool *ok) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  ResolveSrc source = {str_lit("sample"), str_lit("pub fn f(x: bool) -> bool { x }")};
  Vec sources = vec_new(&arena);
  if (!vec_push(&sources, &source)) {
    *ok = false;
    arena_free(&arena);
    return 0;
  }
  review_allocations = 0;
  review_injected = false;
  review_until_failure = fail_at;
  ResolveOutResult result = resolve_resolve(&heap, str_lit("pub mod sample;"), &sources);
  size_t calls = review_allocations;
  review_until_failure = SIZE_MAX;
  bool complete = result.out.mods.len == 1 && result.out.rows.len == 1;
  bool lost = review_injected && result.ok && !heap.oom && !complete;
  if (lost) {
    printf("FAIL resolve allocation=%zu mods=%zu rows=%zu ok=%d oom=%d\n", fail_at,
           result.out.mods.len, result.out.rows.len, result.ok, heap.oom);
  }
  *ok = !lost && (fail_at != SIZE_MAX || (result.ok && !heap.oom && complete));
  arena_free(&arena);
  return calls;
}

int main(void) {
  bool ok = false;
  size_t allocations = review_resolve(SIZE_MAX, &ok);
  if (!ok) return 1;
  size_t failed = 0;
  for (size_t i = 0; i < allocations; i++) {
    review_resolve(i, &ok);
    if (!ok) failed += 1;
  }
  printf("rust_resolve_oom allocation-cases=%zu failed=%zu\n", allocations, failed);
  return failed == 0 ? 0 : 1;
}
