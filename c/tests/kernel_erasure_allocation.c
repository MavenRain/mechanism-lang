/* A failed Nat.succ must reach the host-failure boundary in erasure and readback. */
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

static bool check_util_lookup(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Vec slots = vec_new(&arena);
  Nat zero = nat_zero();
  Nat one = heap_nat_of_u64(&heap, 1);
  if (!vec_push(&slots, erase_repr_slot_box(SLOT_EXTRA)) ||
      !vec_push(&slots, erase_repr_slot_box(SLOT_KEEP)) || heap.oom) return false;
  MaybeNat normal = erase_util_lookup(&heap, &slots, zero, one);
  bool valid = normal.some && nat_to_u64(normal.value).value == 2;
  injected = false;
  until_failure = 0;
  MaybeNat failed = erase_util_lookup(&heap, &slots, zero, one);
  printf("util normal=%llu failed=%llu injected=%d oom=%d\n",
         (unsigned long long)nat_to_u64(normal.value).value,
         (unsigned long long)nat_to_u64(failed.value).value, injected, heap.oom);
  bool ok = valid && injected && heap.oom;
  arena_free(&arena);
  return ok;
}

static bool check_erase_lookup(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Vec slots = vec_new(&arena);
  Nat zero = nat_zero();
  Nat one = heap_nat_of_u64(&heap, 1);
  if (!vec_push(&slots, erase_slot_box(ERASE_SLOT_EXTRA)) ||
      !vec_push(&slots, erase_slot_box(ERASE_SLOT_KEEP)) || heap.oom) return false;
  EraseFound normal = erase_lookup(&heap, &slots, zero, one);
  bool valid = normal.kind == ERASE_FOUND_KEEP && nat_to_u64(normal.index).value == 2;
  injected = false;
  until_failure = 0;
  EraseFound failed = erase_lookup(&heap, &slots, zero, one);
  printf("erase normal=%llu failed=%llu injected=%d oom=%d\n",
         (unsigned long long)nat_to_u64(normal.index).value,
         (unsigned long long)nat_to_u64(failed.index).value, injected, heap.oom);
  bool ok = valid && injected && heap.oom;
  arena_free(&arena);
  return ok;
}

static bool check_quote_head(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Nat one = heap_nat_of_u64(&heap, 1);
  Nat three = heap_nat_of_u64(&heap, 3);
  Head head = value_hlocal(one);
  const Term *normal = eval_quote_head(&heap, head, three);
  bool valid = normal->kind == TERM_VAR && nat_to_u64(normal->index).value == 1 && !heap.oom;
  injected = false;
  until_failure = 0;
  const Term *failed = eval_quote_head(&heap, head, three);
  printf("quote normal=%llu failed=%llu injected=%d oom=%d\n",
         (unsigned long long)nat_to_u64(normal->index).value,
         (unsigned long long)nat_to_u64(failed->index).value, injected, heap.oom);
  bool ok = valid && injected && heap.oom;
  arena_free(&arena);
  return ok;
}

int main(void) {
  bool util = check_util_lookup();
  bool erase = check_erase_lookup();
  bool quote = check_quote_head();
  bool ok = util && erase && quote;
  printf("erasure Nat allocation util=%d erase=%d quote=%d\n", util, erase, quote);
  return ok ? 0 : 1;
}
