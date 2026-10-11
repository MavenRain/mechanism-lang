/* Port of bend2/tests/kernel_projection_motive.bend (oracle label unit-kernel-projection-motive). */
#include "driver.h"

static MaybeStr no_ind(void) {
  MaybeStr none;
  none.some = false;
  none.value = str_empty();
  return none;
}

static MaybeLevelVar no_level(void) {
  MaybeLevelVar none;
  none.some = false;
  none.value = NULL;
  return none;
}

/* Comp failure after a failed host allocation (heap->oom set). */
static Comp host_oom(Heap *heap) {
  heap->oom = true;
  return budget_failed(error_host_oom());
}

/* The ctx and diagram of legacy_body's second case, carried into the continuation. */
typedef struct {
  Ctx ctx;
  Closure d;
} OpenCtx;

/* point -> Engine.open(ctx, d, [point]) */
static Comp open_point(Heap *heap, void *raw, void *value) {
  OpenCtx *c = raw;
  Vec args = vec_new(heap->arena);
  if (!vec_push(&args, value)) return host_oom(heap);
  return check_engine_open(heap, c->ctx, c->d, args);
}

/* Exact pre-correction helper, retained only as an executable negative control.
 * legacy_body(which, ctx, s, d, q, dom): 0n -> pure(dom); 1n+p ->
 * point <- Engine.conv_project(ctx, s, q, q, None, 0n, V.var(C.size_of(ctx))); Engine.open(ctx, d, [point]) */
static Comp legacy_body(Heap *heap, uint64_t which, Ctx ctx, Shape s, Closure d, Quantity q, const Value *dom) {
  if (which == 0) return budget_pure((void *)dom);
  OpenCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return host_oom(heap);
  c->ctx = ctx;
  c->d = d;
  NextK next;
  next.fn = open_point;
  next.ctx = c;
  Comp point = check_engine_conv_project(heap, ctx, s, q, q, term_motive_none(), nat_zero(),
                                         value_var(heap, context_size_of(ctx)));
  return budget_bind(heap, point, next);
}

/* body -> return Some{T.Motive{None{}, [], "self", body}} (a heap boxed MaybeMotive, as conv_proj_motive) */
static Comp some_motive(Heap *heap, void *raw, void *value) {
  (void)raw;
  MaybeMotive *motive = heap_alloc(heap, sizeof *motive);
  if (motive == NULL) return host_oom(heap);
  *motive = term_motive_some(term_motive(no_ind(), vec_new(heap->arena), str_lit("self"), value));
  return budget_pure(motive);
}

/* The bound context of legacy_motive's quote step. */
typedef struct {
  Ctx bound;
} QuoteCtx;

/* v -> body <- Engine.quote(C.bind(n, q, dom, ctx), v); some_motive(body) */
static Comp quote_value(Heap *heap, void *raw, void *value) {
  QuoteCtx *c = raw;
  NextK next;
  next.fn = some_motive;
  next.ctx = NULL;
  return budget_bind(heap, check_engine_quote(heap, c->bound, value), next);
}

/* legacy_motive(ctx, s, d, q, n, dom, which) -> B.Comp(Maybe<T.Motive<T.T>>) */
static Comp legacy_motive(Heap *heap, Ctx ctx, Shape s, Closure d, Quantity q, Str n, const Value *dom,
                          uint64_t which) {
  QuoteCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return host_oom(heap);
  c->bound = context_bind(heap, n, q, dom, ctx);
  NextK next;
  next.fn = quote_value;
  next.ctx = c;
  return budget_bind(heap, legacy_body(heap, which, ctx, s, d, q, dom), next);
}

/* motive_body(value): None -> Fail{E.Cannot_infer{"missing projection motive"}}; Some{Motive{.., body}} -> Done{body} */
static Result motive_body(const MaybeMotive *value) {
  if (value == NULL || !value->some)
    return budget_result_fail(error_make(ERROR_CANNOT_INFER, str_lit("missing projection motive")));
  return budget_result_done((void *)value->value.body);
}

/* motive(q, old, which) -> Result<E.T, T.T> */
static Result motive(Heap *heap, Quantity q, bool old, uint64_t which) {
  Ctx ctx = context_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  const Value *dom = value_vuniv(heap, level_one(heap));
  Shape shape = shape_spi(q, str_lit("A"), (void *)dom);
  Closure diagram = value_close(vec_new(heap->arena), term_var(heap, nat_zero()));
  Comp action;
  if (old) {
    action = legacy_motive(heap, ctx, shape, diagram, q, str_lit("A"), dom, which);
  } else {
    action = check_engine_conv_proj_motive(heap, ctx, shape, diagram, q, str_lit("A"), dom,
                                           heap_nat_of_u64(heap, which));
  }
  Result value = budget_run(heap, action, budget_unlimited());
  switch (value.kind) {
    case RESULT_FAIL:
      return value;
    case RESULT_DONE:
      return motive_body(value.value);
  }
  return value;
}

/* pair_context(q) = Check.bind("pair", Many, V.VLan{Shape.SPi{q, "A", VUniv 1}, V.close([], Var 0), None}, make(..)) */
static Ctx pair_context(Heap *heap, Quantity q) {
  Shape shape = shape_spi(q, str_lit("A"), (void *)value_vuniv(heap, level_one(heap)));
  const Value *lan = value_vlan(heap, shape, value_close(vec_new(heap->arena), term_var(heap, nat_zero())), no_level());
  Ctx base = check_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  return check_bind(heap, str_lit("pair"), QUANTITY_MANY, lan, base);
}

/* expected(q) = T.Elim{T.ElimData{Shape.SPi{q, "A", Univ 1}, Var 0, One, Some{Motive{None, [], "self", Univ 1}},
 * [Pair{ALeg{Width.of_nat(0n)}, Leg{[(q, "x"), (Many, "y")], Var 1}}]}} */
static const Term *expected(Heap *heap, Quantity q) {
  Vec binders = vec_new(heap->arena);
  Binder *x = term_binder_new(heap, q, str_lit("x"));
  Binder *y = term_binder_new(heap, QUANTITY_MANY, str_lit("y"));
  if (x == NULL || !vec_push(&binders, x)) heap->oom = true;
  if (y == NULL || !vec_push(&binders, y)) heap->oom = true;
  Leg leg = term_leg(binders, term_var(heap, heap_nat_of_u64(heap, 1)));
  Vec branches = vec_new(heap->arena);
  Branch *branch = term_branch_new(heap, term_addr_leg(big_of_nat(heap, nat_zero())), leg);
  if (branch == NULL || !vec_push(&branches, branch)) heap->oom = true;
  Motive motive = term_motive(no_ind(), vec_new(heap->arena), str_lit("self"), term_univ(heap, level_one(heap)));
  Shape shape = shape_spi(q, str_lit("A"), (void *)term_univ(heap, level_one(heap)));
  ElimData data = term_elim_data(shape, term_var(heap, nat_zero()), QUANTITY_ONE, term_motive_some(motive), branches);
  return term_elim(heap, data);
}

/* universe(value): V.VUniv{level} -> L.equal(level, L.one()); _ -> False */
static bool universe(Heap *heap, const Value *value) {
  if (value == NULL || value->kind != VALUE_UNIV) return false;
  return level_equal(heap, value->level, level_one(heap));
}

static BoolResult bool_done(bool value) {
  BoolResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_host_oom();
  return r;
}

static BoolResult bool_fail(Error error) {
  BoolResult r;
  r.kind = RESULT_FAIL;
  r.value = false;
  r.error = error;
  return r;
}

/* current(q): body <- motive(q, False, 1n); inferred <- Check.infer(pair_context(q), Zero, body);
 * first <- motive(q, False, 0n); return T.equal(body, expected(q)) && universe(inferred) && T.equal(first, Univ 1) */
static BoolResult current(Heap *heap, Quantity q) {
  Result body = motive(heap, q, false, 1);
  if (body.kind == RESULT_FAIL) return bool_fail(body.error);
  Result inferred = check_infer(heap, pair_context(heap, q), QUANTITY_ZERO, body.value);
  if (inferred.kind == RESULT_FAIL) return bool_fail(inferred.error);
  Result first = motive(heap, q, false, 0);
  if (first.kind == RESULT_FAIL) return bool_fail(first.error);
  bool shaped = term_equal(body.value, expected(heap, q));
  bool typed = universe(heap, inferred.value);
  bool plain = term_equal(first.value, term_univ(heap, level_one(heap)));
  return bool_done(shaped && typed && plain);
}

/* legacy_error(result): Fail{E.Cannot_infer{message}} -> message == "an elimination without a motive needs an
 * expected type"; _ -> False */
static bool legacy_error(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return false;
    case RESULT_FAIL:
      return result.error.kind == ERROR_CANNOT_INFER &&
             str_eq(result.error.message, str_lit("an elimination without a motive needs an expected type"));
  }
  return false;
}

/* old_refused(q): body <- motive(q, True, 1n); return legacy_error(Check.infer(pair_context(q), Zero, body)) */
static BoolResult old_refused(Heap *heap, Quantity q) {
  Result body = motive(heap, q, true, 1);
  if (body.kind == RESULT_FAIL) return bool_fail(body.error);
  return bool_done(legacy_error(check_infer(heap, pair_context(heap, q), QUANTITY_ZERO, body.value)));
}

/* IO.die(1, message) with a heap Str. */
static int die_str(Str message) {
  if (message.len > 0) fwrite(message.data, 1, message.len, stderr);
  fputc('\n', stderr);
  return 1;
}

/* require(result, label): Done{True} -> pure; Done{False} -> die "FAIL " ++ label;
 * Fail{error} -> die "FAIL " ++ label ++ ": " ++ E.to_string(error) */
static int require(Heap *heap, BoolResult result, const char *label) {
  Str prefix = str_cat(heap->arena, str_lit("FAIL "), str_lit(label));
  switch (result.kind) {
    case RESULT_DONE:
      if (result.value) return 0;
      return die_str(prefix);
    case RESULT_FAIL:
      return die_str(str_cat(heap->arena, str_cat(heap->arena, prefix, str_lit(": ")), error_to_string(heap, result.error)));
  }
  return 0;
}

static int run(Heap *heap) {
  int code = require(heap, current(heap, QUANTITY_ZERO), "current zero");
  if (code != 0) return code;
  code = require(heap, current(heap, QUANTITY_ONE), "current one");
  if (code != 0) return code;
  code = require(heap, current(heap, QUANTITY_MANY), "current many");
  if (code != 0) return code;
  code = require(heap, old_refused(heap, QUANTITY_ZERO), "legacy zero");
  if (code != 0) return code;
  code = require(heap, old_refused(heap, QUANTITY_ONE), "legacy one");
  if (code != 0) return code;
  code = require(heap, old_refused(heap, QUANTITY_MANY), "legacy many");
  if (code != 0) return code;
  return driver_print(str_lit("PASS dependent pair projection motives: 9 positive assertions, 3 old-port refusals"));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
