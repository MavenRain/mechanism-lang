/* Regression: a failed job concatenation must not accept an ill-typed conditional. */
#include "rust.h"
#include "surface.h"

#include <stdio.h>
#include <stdint.h>

void *rt_test_arena_alloc_real(Arena *arena, size_t size);

static size_t until_failure = SIZE_MAX;
static bool injected;
static size_t alloc_calls;
static size_t failed_size;
static unsigned failures;

void *arena_alloc(Arena *arena, size_t size) {
  alloc_calls += 1;
  if (until_failure == 0) {
    until_failure = SIZE_MAX;
    injected = true;
    failed_size = size;
    return NULL;
  }
  if (until_failure != SIZE_MAX) until_failure -= 1;
  return rt_test_arena_alloc_real(arena, size);
}

static void check(bool ok, const char *label) {
  if (!ok) {
    failures += 1;
    printf("FAIL %s\n", label);
  }
}

static void invalid_conditional(size_t fail_at) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Vec none = vec_new(&arena);
  InferEnv env = {none, none, none};
  RirTy boolean = {.kind = RIR_TBOOL};
  RirEx zero = {.kind = RIR_XINT, .text = {"0", 1}};
  RirEx yes = {.kind = RIR_XBOOL, .boolean = true};
  RirEx no = {.kind = RIR_XBOOL, .boolean = false};
  RirEx conditional = {.kind = RIR_XIF, .cond = &zero, .yes = &yes, .no = &no};
  InferJob job = {.kind = INFER_JCHK, .locals = none, .e = &conditional, .want = &boolean};
  InferSt empty = {none, none};
  until_failure = fail_at;
  injected = false;
  InferStResult step = infer_step(&heap, &env, &job, &empty);
  until_failure = SIZE_MAX;
  InferNotesResult walked = infer_walk(&heap, &env, step);
  if (injected && !heap.oom && walked.ok) {
    printf("fault=%zu step_ok=%d jobs=%zu heap_oom=%d walk_ok=%d\n", fail_at, step.ok,
           step.st.jobs.len, heap.oom, walked.ok);
  }
  check(heap.oom || !walked.ok, "conditional checks survive allocation failure");
  arena_free(&arena);
}

static Str lower_keyword(Heap *heap, size_t fail_at, size_t *calls) {
  Vec none = vec_new(heap->arena);
  InferEnv env = {none, none, none};
  RirTy boolean = {.kind = RIR_TBOOL};
  RirParam param = {.name = {"case", 4}, .ty = &boolean};
  RirEx variable = {.kind = RIR_XVAR, .name = {"case", 4}};
  Vec params = vec_new(heap->arena);
  Vec globals = vec_new(heap->arena);
  Str reserved = {"caseLocal", 9};
  if (!vec_push(&params, &param)) heap->oom = true;
  if (!vec_push(&globals, &reserved)) heap->oom = true;
  alloc_calls = 0;
  until_failure = fail_at;
  injected = false;
  const SDecl *decl = lower_fn_decl(heap, &env, &globals, &none, str_lit("first"), &none,
                                  &params, &boolean, &variable);
  until_failure = SIZE_MAX;
  *calls = alloc_calls;
  if (heap->oom) return str_empty();
  Vec decls = vec_new(heap->arena);
  if (!vec_push(&decls, (void *)decl)) heap->oom = true;
  return syntax_print(heap, &decls);
}

static void keyword_lowering(void) {
  Arena baseline_arena = arena_new(1 << 16);
  Heap baseline_heap = heap_new(&baseline_arena);
  size_t calls;
  Str expected = lower_keyword(&baseline_heap, SIZE_MAX, &calls);
  check(!baseline_heap.oom && expected.len != 0, "keyword lowering baseline");
  for (size_t at = 0; at < calls; at++) {
    Arena arena = arena_new(1 << 16);
    Heap heap = heap_new(&arena);
    size_t observed_calls;
    Str actual = lower_keyword(&heap, at, &observed_calls);
    bool same = str_eq(expected, actual);
    if (injected && !heap.oom && !same)
      printf("lower_fault=%zu allocation_size=%zu heap_oom=%d output_differs=1\n", at,
             failed_size, heap.oom);
    check(heap.oom || same, "keyword lowering preserves names on allocation failure");
    arena_free(&arena);
  }
  arena_free(&baseline_arena);
}

int main(void) {
  invalid_conditional(SIZE_MAX);
  for (size_t at = 0; at < 8; at++) invalid_conditional(at);
  keyword_lowering();
  if (failures != 0) return 1;
  puts("PASS inference and lowering allocation failures preserve jobs and names");
  return 0;
}
