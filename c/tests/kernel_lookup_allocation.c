/* Fault injection for a positive Nat increment in the C checker. */
#include "kernel.h"

#include <stdio.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);

static bool fail_next;

void *arena_alloc(Arena *arena, size_t size) {
  if (fail_next) {
    fail_next = false;
    return NULL;
  }
  return rt_test_arena_alloc_real(arena, size);
}

static int check_name_increment(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  Nat start = heap_nat_of_u64(&heap, 2);
  Nat one = heap_nat_of_u64(&heap, 1);
  Nat want = heap_nat_of_u64(&heap, 3);
  Local target = context_local(str_lit("target"), QUANTITY_MANY, NULL);
  LTree target_tree = {.kind = LTREE_LEAF, .x = target, .l = NULL, .r = NULL};
  Locals tail = {.w = one, .t = &target_tree, .rest = NULL};
  LView first = {.kind = LVIEW_CONS,
                 .head = context_local(str_lit("other"), QUANTITY_MANY, NULL),
                 .tail = &tail};
  MaybeNat normal = check_name_index(&heap, first, str_lit("target"), start);
  if (heap.oom || !normal.some || nat_cmp(normal.value, want) != 0) {
    puts("FAIL normal name lookup");
    arena_free(&arena);
    return 1;
  }

  fail_next = true;
  MaybeNat injected = check_name_index(&heap, first, str_lit("target"), start);
  if (!heap.oom) {
    printf("FAIL injected Nat increment: returned %s index without OOM\n",
           injected.some && nat_cmp(injected.value, start) == 0 ? "unchanged" : "another");
    arena_free(&arena);
    return 1;
  }
  puts("PASS injected Nat increment reports OOM");
  arena_free(&arena);
  return 0;
}

static int check_local_type_lookup(void) {
  Arena arena = arena_new(4096);
  Heap heap = heap_new(&arena);
  const Value *oldest = value_vuniv(&heap, level_zero());
  const Value *newest = value_vuniv(&heap, level_succ(&heap, level_zero()));
  Ctx ctx = context_make(&heap, global_empty(), budget_unlimited(), nat_zero());
  ctx = context_bind(&heap, str_lit("oldest"), QUANTITY_MANY, oldest, ctx);
  ctx = context_bind(&heap, str_lit("newest"), QUANTITY_MANY, newest, ctx);
  Nat level = heap_nat_of_u64(&heap, 1);
  Result normal = budget_run(&heap, check_engine_head_local_bounded(&heap, true, ctx.locals, ctx.size, level),
                             budget_unlimited());
  if (heap.oom || normal.kind != RESULT_DONE || normal.value != newest) {
    puts("FAIL normal local type lookup");
    arena_free(&arena);
    return 1;
  }

  fail_next = true;
  Result injected = budget_run(&heap, check_engine_head_local_bounded(&heap, true, ctx.locals, ctx.size, level),
                               budget_unlimited());
  if (!heap.oom) {
    printf("FAIL injected local type lookup: returned %s type without OOM\n",
           injected.kind == RESULT_DONE && injected.value == oldest ? "older" : "another");
    arena_free(&arena);
    return 1;
  }
  puts("PASS injected local type lookup reports OOM");
  arena_free(&arena);
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 1 && argv[1][0] == 'l') return check_local_type_lookup();
  if (check_name_increment() != 0) return 1;
  return check_local_type_lookup();
}
