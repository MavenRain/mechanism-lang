/* Port of bend2/wasm/state.bend: the state monad the emitter threads through one function body.
 * Comp(A) is a closure from the state to the next state and a Result; the value of a Result is the
 * erased A (a pointer). A failed allocation sets heap->oom and yields a computation that fails with
 * the host oom error, so the program boundary sees heap->oom. */
#include "wasm.h"

/* C.Pair{state, result} */
static WasmStep state_step(WasmState state, Result result) {
  WasmStep out;
  out.state = state;
  out.result = result;
  return out;
}

static WasmComp state_comp(WasmStep (*fn)(Heap *heap, void *ctx, WasmState state), void *ctx) {
  WasmComp out;
  out.fn = fn;
  out.ctx = ctx;
  return out;
}

/* The computation of a failed allocation: Fail{host oom}, the state unchanged. */
static WasmStep state_oom_run(Heap *heap, void *ctx, WasmState state) {
  (void)heap;
  (void)ctx;
  return state_step(state, budget_result_fail(error_host_oom()));
}

static WasmComp state_oom(Heap *heap) {
  heap->oom = true;
  return state_comp(state_oom_run, NULL);
}

/* Comp.pure(value) = state => C.Pair{state, Done{value}}; the value is the closure context. */
static WasmStep state_pure_run(Heap *heap, void *ctx, WasmState state) {
  (void)heap;
  return state_step(state, budget_result_done(ctx));
}

WasmComp state_comp_pure(Heap *heap, void *value) {
  (void)heap;
  return state_comp(state_pure_run, value);
}

/* bind_result(C.Pair{s, Fail{e}}, next) = C.Pair{s, Fail{e}}; bind_result(C.Pair{s, Done{a}}, next) =
 * next(a)(s) */
WasmStep state_bind_result(Heap *heap, WasmStep step, WasmNext next) {
  switch (step.result.kind) {
    case RESULT_FAIL:
      return step;
    case RESULT_DONE: {
      WasmComp action = next.fn(heap, next.ctx, step.result.value);
      return action.fn(heap, action.ctx, step.state);
    }
  }
  return step;
}

/* Comp.bind(action, next) = state => bind_result(action(state), next) */
typedef struct {
  WasmComp action;
  WasmNext next;
} StateBind;

static WasmStep state_bind_run(Heap *heap, void *ctx, WasmState state) {
  const StateBind *b = ctx;
  return state_bind_result(heap, b->action.fn(heap, b->action.ctx, state), b->next);
}

WasmComp state_comp_bind(Heap *heap, WasmComp action, WasmNext next) {
  StateBind *b = arena_alloc(heap->arena, sizeof(StateBind));
  if (b == NULL) return state_oom(heap);
  b->action = action;
  b->next = next;
  return state_comp(state_bind_run, b);
}

/* lift(result) = state => C.Pair{state, result} */
static WasmStep state_lift_run(Heap *heap, void *ctx, WasmState state) {
  (void)heap;
  return state_step(state, *(const Result *)ctx);
}

WasmComp state_lift(Heap *heap, Result result) {
  Result *box = arena_alloc(heap->arena, sizeof(Result));
  if (box == NULL) return state_oom(heap);
  *box = result;
  return state_comp(state_lift_run, box);
}

/* fail(error) = lift(Fail{error}) */
WasmComp state_fail(Heap *heap, Error error) { return state_lift(heap, budget_result_fail(error)); }

/* U32.from_nat(n): the low 32 bits. */
static uint32_t state_u32(Nat n) {
  NatToU64 v = nat_to_u64(n);
  return v.status == NAT_TO_U64_OK ? (uint32_t)v.value : 0;
}

/* allocate(State{base, extra}, v) = C.Pair{State{base, extra ++ [v]}, Done{U32.from_nat(base + |extra|)}} */
WasmStep state_allocate(Heap *heap, WasmState state, GcValtype v) {
  uint32_t index = state_u32(heap_nat_add(heap, state.base, heap_nat_of_u64(heap, state.extra.len)));
  uint32_t *boxed = arena_alloc(heap->arena, sizeof(uint32_t));
  GcValtype *item = gc_valtype_box(heap, v);
  Vec extra = vec_new(heap->arena);
  for (size_t i = 0; i < state.extra.len; i++) {
    VecGet at = vec_get(&state.extra, i);
    if (at.status == VEC_GET_OUT_OF_RANGE || !vec_push(&extra, at.value)) heap->oom = true;
  }
  if (item == NULL || !vec_push(&extra, item)) heap->oom = true;
  WasmState next;
  next.base = state.base;
  next.extra = extra;
  if (boxed == NULL) {
    heap->oom = true;
    return state_step(next, budget_result_fail(error_host_oom()));
  }
  *boxed = index;
  return state_step(next, budget_result_done(boxed));
}

/* alloc(v) = state => allocate(state, v); the boxed v is the closure context. */
static WasmStep state_alloc_run(Heap *heap, void *ctx, WasmState state) {
  return state_allocate(heap, state, *(const GcValtype *)ctx);
}

WasmComp state_alloc(Heap *heap, GcValtype v) {
  GcValtype *box = gc_valtype_box(heap, v);
  if (box == NULL) return state_oom(heap);
  return state_comp(state_alloc_run, box);
}

/* pure(value) = Comp.pure(value) */
WasmComp state_pure(Heap *heap, void *value) { return state_comp_pure(heap, value); }

/* finish(ft, C.Pair{s, Fail{e}}) = Fail{e}; finish(ft, C.Pair{State{b, extra}, Done{code}}) =
 * Done{G.Func{ft, extra, code}} */
Result state_finish(Heap *heap, uint32_t ft, WasmStep step) {
  switch (step.result.kind) {
    case RESULT_FAIL:
      return step.result;
    case RESULT_DONE: {
      const Vec *code = step.result.value;
      if (code == NULL) return budget_result_done(gc_func(heap, ft, step.state.extra, vec_new(heap->arena)));
      return budget_result_done(gc_func(heap, ft, step.state.extra, *code));
    }
  }
  return step.result;
}
