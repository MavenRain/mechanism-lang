/* Port of bend2/import/lower.bend: lowering a translated Lean type DAG into a checked kernel term.
 * Comp(A) = State -> State & Result<Error, A> is direct style here: every action takes the heap and
 * the LowerState by pointer (threaded in and out) and returns a LowerResult whose payload is the A
 * erased to a pointer (const Term *, const LevelVar *, const Value *, Vec *, LowerBoundContext *,
 * LowerFunctionDomain *, Shape *, or NULL for Unit). The monadic plumbing of the Bend file (pure,
 * failed, bind_value, bind_result, Comp.pure, Comp.bind, kernel_error, kernel_result, kernel_state,
 * discard) folds into the early returns below; the definition order otherwise follows the Bend. */
#include "import.h"

/* message(error) */
Str lower_message(Heap *heap, LowerError error) {
  switch (error.kind) {
  case LOWER_DEFERRED:
    return error.message;
  case LOWER_UNSUPPORTED:
    return error.message;
  case LOWER_KERNEL_ERROR:
    return error_to_string(heap, error.error);
  }
  return str_empty();
}

/* Deferred{message} */
static LowerError deferred(Str message) {
  LowerError error;
  error.kind = LOWER_DEFERRED;
  error.message = message;
  error.error = error_make(ERROR_MISMATCH, str_empty());
  return error;
}

/* Unsupported{message} */
static LowerError unsupported(Str message) {
  LowerError error;
  error.kind = LOWER_UNSUPPORTED;
  error.message = message;
  error.error = error_make(ERROR_MISMATCH, str_empty());
  return error;
}

/* KernelError{error} */
static LowerError kernel_error(Error error) {
  LowerError wrapped;
  wrapped.kind = LOWER_KERNEL_ERROR;
  wrapped.message = str_empty();
  wrapped.error = error;
  return wrapped;
}

/* pure(value) */
static LowerResult pure(void *value) {
  LowerResult result;
  result.kind = LOWER_DONE;
  result.value = value;
  result.error = unsupported(str_empty());
  return result;
}

/* failed(error) */
static LowerResult failed(LowerError error) {
  LowerResult result;
  result.kind = LOWER_FAIL;
  result.value = NULL;
  result.error = error;
  return result;
}

/* The host ran out of memory inside an action: heap->oom is set, the driver exits 70 after the
 * run; the error text never reaches the oracle because the Bend host has no such path. */
static LowerResult host_failure(Heap *heap) {
  heap->oom = true;
  return failed(kernel_error(error_make(ERROR_BUDGET_EXHAUSTED, str_lit("host out of memory"))));
}

/* kernel(action): run a kernel budget computation against the state's budget (kernel_state +
 * kernel_result + kernel_error of the Bend) */
static LowerResult kernel(Heap *heap, LowerState *state, Comp action) {
  BudgetState run = budget_run_state(heap, action, state->budget);
  state->budget = run.budget;
  if (run.result.kind == RESULT_FAIL) return failed(kernel_error(run.result.error));
  if (run.result.value == NULL && heap->oom) return host_failure(heap);
  return pure(run.result.value);
}

/* lift(value) */
static LowerResult lift(Heap *heap, LowerState *state, Result value) {
  return kernel(heap, state, budget_lift(value));
}

/* depth_result(ok, work, budget, resolver, domain) */
static LowerResult depth_result(Heap *heap, bool ok, Str domain) {
  if (ok) return pure(NULL);
  return failed(unsupported(heap_str_cat(heap, domain, str_lit(" lowering exceeds the depth limit of 1024"))));
}

/* polled(value, work, resolver, depth, domain) */
static LowerResult polled(Heap *heap, LowerState *state, MaybeBudget value, uint32_t depth, Str domain) {
  if (!value.some) {
    state->budget = budget_of_polls(nat_zero());
    return failed(kernel_error(
        error_make(ERROR_BUDGET_EXHAUSTED, heap_str_cat(heap, domain, str_lit(" lowering budget is exhausted")))));
  }
  state->budget = value.value;
  return depth_result(heap, depth <= 1024, domain);
}

/* stepped(state, depth, domain) */
static LowerResult stepped(Heap *heap, LowerState *state, uint32_t depth, Str domain) {
  if (nat_is_zero(state->work)) {
    return failed(kernel_error(
        error_make(ERROR_BUDGET_EXHAUSTED, str_lit("type lowering exceeds the work limit of 100000 nodes"))));
  }
  NatSub remaining = nat_sub(heap->arena, state->work, nat_of_u64(heap->arena, 1));
  if (remaining.status != NAT_SUB_OK) return host_failure(heap);
  state->work = remaining.value;
  return polled(heap, state, budget_poll(heap, state->budget), depth, domain);
}

/* step(depth, domain) */
static LowerResult step(Heap *heap, LowerState *state, uint32_t depth, Str domain) {
  return stepped(heap, state, depth, domain);
}

/* require(value, error): a table lookup that must be present */
static LowerResult require(TableGet value, LowerError error) {
  if (!value.present) return failed(error);
  return pure(value.value);
}

/* resolver_call(resolver, name, levels) */
LowerResolved lower_resolver_call(Heap *heap, LowerResolver resolver, Str name, const Vec *levels) {
  return resolver.call(heap, resolver.ctx, name, levels);
}

/* resolve(name, levels): resolved_value + resolved + resolve_state of the Bend; the resolver's
 * successor replaces the one in the state */
static LowerResult resolve(Heap *heap, LowerState *state, Str name, const Vec *levels) {
  LowerResolved value = lower_resolver_call(heap, state->resolver, name, levels);
  state->resolver = value.next;
  if (!value.ok) return failed(deferred(value.message));
  return pure((void *)value.term);
}

/* missing(prefix): every name fails with prefix ++ name; ctx is the heap-boxed prefix */
static LowerResolved missing_call(Heap *heap, void *ctx, Str name, const Vec *levels) {
  LowerResolved result;
  Str prefix = ctx == NULL ? str_empty() : *(const Str *)ctx;
  (void)levels;
  result.next.call = missing_call;
  result.next.ctx = ctx;
  result.ok = false;
  result.message = heap_str_cat(heap, prefix, name);
  result.term = NULL;
  return result;
}

LowerResolver lower_missing(Heap *heap, Str prefix) {
  LowerResolver resolver;
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box != NULL) *box = prefix;
  resolver.call = missing_call;
  resolver.ctx = box;
  return resolver;
}

/* fixed(term): every name resolves to the same term; ctx is the term */
static LowerResolved fixed_call(Heap *heap, void *ctx, Str name, const Vec *levels) {
  LowerResolved result;
  (void)heap;
  (void)name;
  (void)levels;
  result.next.call = fixed_call;
  result.next.ctx = ctx;
  result.ok = true;
  result.message = str_empty();
  result.term = ctx;
  return result;
}

LowerResolver lower_fixed(const Term *term) {
  LowerResolver resolver;
  resolver.call = fixed_call;
  resolver.ctx = (void *)term;
  return resolver;
}

/* infer(ctx, term) -> const Value * (inferred(result) = the ty field) */
static LowerResult infer(Heap *heap, LowerState *state, Ctx ctx, const Term *term) {
  LowerResult result = kernel(heap, state, check_engine_infer_uses(heap, ctx, QUANTITY_ZERO, term));
  if (result.kind == LOWER_FAIL) return result;
  if (result.value == NULL) return host_failure(heap);
  return pure((void *)((const Inferred *)result.value)->ty);
}

/* check(ctx, term, ty) -> Unit */
static LowerResult check(Heap *heap, LowerState *state, Ctx ctx, const Term *term, const Value *ty) {
  LowerResult uses = kernel(heap, state, check_engine_check_uses(heap, ctx, QUANTITY_ZERO, term, ty));
  if (uses.kind == LOWER_FAIL) return uses;
  return pure(NULL);
}

/* universe(ctx, term) -> const LevelVar * */
static LowerResult universe(Heap *heap, LowerState *state, Ctx ctx, const Term *term) {
  return kernel(heap, state, check_engine_infer_univ(heap, ctx, term));
}

/* evaluate(ctx, term) -> const Value * */
static LowerResult evaluate(Heap *heap, LowerState *state, Ctx ctx, const Term *term) {
  return lift(heap, state, eval_eval(heap, check_globals_of(ctx), check_env_of(ctx), term));
}

/* quote(ctx, value) -> const Term * */
static LowerResult quote(Heap *heap, LowerState *state, Ctx ctx, const Value *value) {
  return lift(heap, state, eval_quote(heap, check_globals_of(ctx), check_size_of(ctx), value));
}

/* LevelResult{cache, value}: the cache is a Table of const LevelVar * updated in place, the value
 * travels as the LowerResult payload (level_cache and level_value fold away). */

/* positions(parameters, index, table): parameter source name -> heap-boxed Nat position */
static Table positions(Heap *heap, const Vec *parameters, size_t index, Table table) {
  VecGet got = vec_get(parameters, index);
  if (got.status != VEC_GET_OK) return table;
  Nat *box = heap_alloc(heap, sizeof(Nat));
  if (box == NULL) return table;
  *box = nat_of_u64(heap->arena, (uint64_t)index);
  table_put(heap, &table, ((const TranslateParameter *)got.value)->source_name, box);
  return positions(heap, parameters, index + 1, table);
}

static LowerResult lower_level_cached(Heap *heap, LowerState *state, uint32_t depth, Table *table,
                                      const Table *position_table, Table *cache, Str id);
static LowerResult lower_level_node(Heap *heap, LowerState *state, const TranslateLevel *node, uint32_t depth,
                                    Table *table, const Table *position_table, Table *cache);

/* level_miss(found, depth, table, positions, cache, id) */
static LowerResult level_miss(Heap *heap, LowerState *state, TableGet found, uint32_t depth, Table *table,
                              const Table *position_table, Table *cache, Str id) {
  if (found.present) return pure(found.value);
  LowerResult unit = step(heap, state, depth, str_lit("universe"));
  if (unit.kind == LOWER_FAIL) return unit;
  LowerResult node =
      require(table_get(table, id), unsupported(heap_str_cat(heap, str_lit("missing type level "), id)));
  if (node.kind == LOWER_FAIL) return node;
  LowerResult result = lower_level_node(heap, state, node.value, depth, table, position_table, cache);
  if (result.kind == LOWER_FAIL) return result;
  table_put(heap, cache, id, result.value);
  return result;
}

/* lower_level_cached(depth, table, positions, cache, id) */
static LowerResult lower_level_cached(Heap *heap, LowerState *state, uint32_t depth, Table *table,
                                      const Table *position_table, Table *cache, Str id) {
  return level_miss(heap, state, table_get(cache, id), depth, table, position_table, cache, id);
}

/* combine_levels(imax, a, b) */
static const LevelVar *combine_levels(Heap *heap, bool imax, const LevelVar *a, const LevelVar *b) {
  if (imax) return level_imax(heap, a, b);
  return level_max(heap, a, b);
}

/* level_pair(depth, table, positions, cache, left, right, imax) */
static LowerResult level_pair(Heap *heap, LowerState *state, uint32_t depth, Table *table,
                              const Table *position_table, Table *cache, Str left, Str right, bool imax) {
  LowerResult a = lower_level_cached(heap, state, depth + 1, table, position_table, cache, left);
  if (a.kind == LOWER_FAIL) return a;
  LowerResult b = lower_level_cached(heap, state, depth + 1, table, position_table, cache, right);
  if (b.kind == LOWER_FAIL) return b;
  return pure((void *)combine_levels(heap, imax, a.value, b.value));
}

/* lower_level_node(node, depth, table, positions, cache) */
static LowerResult lower_level_node(Heap *heap, LowerState *state, const TranslateLevel *node, uint32_t depth,
                                    Table *table, const Table *position_table, Table *cache) {
  switch (node->kind) {
  case TRANSLATE_LZERO:
    return pure((void *)level_zero());
  case TRANSLATE_LPARAM: {
    LowerResult position = require(table_get(position_table, node->source_name),
                                   unsupported(heap_str_cat(heap, str_lit("unbound universe parameter "), node->name)));
    if (position.kind == LOWER_FAIL) return position;
    return pure((void *)level_var(heap, *(const Nat *)position.value));
  }
  case TRANSLATE_LSUCC: {
    LowerResult a = lower_level_cached(heap, state, depth + 1, table, position_table, cache, node->child);
    if (a.kind == LOWER_FAIL) return a;
    return pure((void *)level_succ(heap, a.value));
  }
  case TRANSLATE_LMAX:
    return level_pair(heap, state, depth, table, position_table, cache, node->left, node->right, false);
  case TRANSLATE_LIMAX:
    return level_pair(heap, state, depth, table, position_table, cache, node->left, node->right, true);
  }
  return failed(unsupported(str_lit("missing type level")));
}

/* lower_level(table, params, id) -> const LevelVar * */
static LowerResult lower_level(Heap *heap, LowerState *state, Translated *table, const Vec *params, Str id) {
  Table position_table = positions(heap, params, 0, table_empty(heap));
  Table cache = table_empty(heap);
  return lower_level_cached(heap, state, 0, translate_level_nodes(table), &position_table, &cache, id);
}

/* lower_levels(ids, table, params) -> Vec * of const LevelVar *, built in source order */
static LowerResult lower_levels(Heap *heap, LowerState *state, const Vec *ids, size_t index, Translated *table,
                                const Vec *params, Vec *levels) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return pure(levels);
  LowerResult level = lower_level(heap, state, table, params, *(const Str *)got.value);
  if (level.kind == LOWER_FAIL) return level;
  if (!vec_push(levels, level.value)) return host_failure(heap);
  return lower_levels(heap, state, ids, index + 1, table, params, levels);
}

/* BoundContext{domain, ctx}: LowerBoundContext in import.h */

static LowerResult lower(Heap *heap, LowerState *state, uint32_t depth, Ctx initial, Ctx ctx, Translated *table,
                         const Vec *params, Str id);

/* bind_domain(depth, initial, ctx, table, params, typ, name) -> LowerBoundContext * */
static LowerResult bind_domain(Heap *heap, LowerState *state, uint32_t depth, Ctx initial, Ctx ctx,
                               Translated *table, const Vec *params, Str typ, Str name) {
  LowerResult domain = lower(heap, state, depth + 1, initial, ctx, table, params, typ);
  if (domain.kind == LOWER_FAIL) return domain;
  LowerResult level = universe(heap, state, ctx, domain.value);
  if (level.kind == LOWER_FAIL) return level;
  LowerResult value = evaluate(heap, state, ctx, domain.value);
  if (value.kind == LOWER_FAIL) return value;
  LowerBoundContext *inner = heap_alloc(heap, sizeof(LowerBoundContext));
  if (inner == NULL) return host_failure(heap);
  inner->domain = domain.value;
  inner->ctx = check_bind(heap, name, QUANTITY_MANY, value.value, ctx);
  return pure(inner);
}

/* function_shape(value) -> Shape * (the shape of a VRan value) */
static LowerResult function_shape(Heap *heap, const Value *value) {
  if (value->kind == VALUE_RAN) {
    Shape *box = heap_alloc(heap, sizeof(Shape));
    if (box == NULL) return host_failure(heap);
    *box = value->shape;
    return pure(box);
  }
  return failed(
      kernel_error(error_make(ERROR_MISMATCH, str_lit("the head of an imported application is not a function"))));
}

/* FunctionDomain{quantity, name, value}: LowerFunctionDomain in import.h */

/* function_domain(shape) -> LowerFunctionDomain * */
static LowerResult function_domain(Heap *heap, const Shape *shape) {
  if (shape->kind == SHAPE_PI) {
    LowerFunctionDomain *domain = heap_alloc(heap, sizeof(LowerFunctionDomain));
    if (domain == NULL) return host_failure(heap);
    domain->quantity = shape->quantity;
    domain->name = shape->name;
    domain->value = shape->domain;
    return pure(domain);
  }
  return failed(kernel_error(
      error_make(ERROR_MISMATCH, str_lit("the head of an imported application is not a dependent function"))));
}

/* application(domain, depth, initial, ctx, table, params, argument, head) -> const Term * */
static LowerResult application(Heap *heap, LowerState *state, const LowerFunctionDomain *domain, uint32_t depth,
                               Ctx initial, Ctx ctx, Translated *table, const Vec *params, Str argument,
                               const Term *head) {
  LowerResult arg = lower(heap, state, depth + 1, initial, ctx, table, params, argument);
  if (arg.kind == LOWER_FAIL) return arg;
  LowerResult unit = check(heap, state, ctx, arg.value, domain->value);
  if (unit.kind == LOWER_FAIL) return unit;
  return pure((void *)term_out(heap, shape_sarg(domain->quantity, domain->name),
                               term_addr_pt(domain->quantity, arg.value), head));
}

/* bound_index(value) */
static LowerResult bound_index(Heap *heap, MaybeU32 value) {
  if (!value.some) return failed(unsupported(str_lit("invalid de Bruijn index in a type")));
  return pure((void *)term_var(heap, nat_of_u64(heap->arena, value.value)));
}

/* bound_number(value) */
static LowerResult bound_number(Heap *heap, MaybeBig value) {
  if (!value.some) return failed(unsupported(str_lit("invalid de Bruijn index in a type")));
  return bound_index(heap, big_to_u32(value.value));
}

/* Term.Ann{Term.Sec{SPi{many, name, domain}, [Leg{[(many, name)], body}]}, arrow(many, name, domain, codomain)} */
static LowerResult lambda_term(Heap *heap, Str name, const Term *domain, const Term *body, const Term *codomain) {
  Vec binders = vec_new(heap->arena);
  Vec legs = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, name);
  if (binder == NULL || !vec_push(&binders, binder)) return host_failure(heap);
  Leg *leg = term_leg_new(heap, binders, body);
  if (leg == NULL || !vec_push(&legs, leg)) return host_failure(heap);
  const Term *section = term_sec(heap, shape_spi(QUANTITY_MANY, name, (void *)domain), legs);
  return pure((void *)term_ann(heap, section, rules_arrow(heap, QUANTITY_MANY, name, domain, codomain)));
}

/* lower_node(node, depth, initial, ctx, table, params) -> const Term * */
static LowerResult lower_node(Heap *heap, LowerState *state, const TranslateNode *node, uint32_t depth, Ctx initial,
                              Ctx ctx, Translated *table, const Vec *params) {
  switch (node->kind) {
  case TRANSLATE_BOUND:
    return bound_number(heap, big_of_decimal(heap, node->index));
  case TRANSLATE_SORT: {
    LowerResult value = lower_level(heap, state, table, params, node->level);
    if (value.kind == LOWER_FAIL) return value;
    return pure((void *)term_univ(heap, value.value));
  }
  case TRANSLATE_CONSTANT: {
    Vec collected = vec_new(heap->arena);
    LowerResult levels = lower_levels(heap, state, &node->universes, 0, table, params, &collected);
    if (levels.kind == LOWER_FAIL) return levels;
    LowerResult term = resolve(heap, state, node->name, levels.value);
    if (term.kind == LOWER_FAIL) return term;
    LowerResult ty = infer(heap, state, initial, term.value);
    if (ty.kind == LOWER_FAIL) return ty;
    return term;
  }
  case TRANSLATE_APPLY: {
    LowerResult head = lower(heap, state, depth + 1, initial, ctx, table, params, node->fn);
    if (head.kind == LOWER_FAIL) return head;
    LowerResult head_type = infer(heap, state, ctx, head.value);
    if (head_type.kind == LOWER_FAIL) return head_type;
    LowerResult whnf = lift(heap, state, eval_whnf(heap, check_globals_of(ctx), head_type.value));
    if (whnf.kind == LOWER_FAIL) return whnf;
    if (whnf.value == NULL) return host_failure(heap);
    LowerResult shape = function_shape(heap, whnf.value);
    if (shape.kind == LOWER_FAIL) return shape;
    LowerResult domain = function_domain(heap, shape.value);
    if (domain.kind == LOWER_FAIL) return domain;
    return application(heap, state, domain.value, depth, initial, ctx, table, params, node->arg, head.value);
  }
  case TRANSLATE_PI: {
    LowerResult inner = bind_domain(heap, state, depth, initial, ctx, table, params, node->binder.typ, node->binder.name);
    if (inner.kind == LOWER_FAIL) return inner;
    LowerBoundContext *bound = inner.value;
    LowerResult body = lower(heap, state, depth + 1, initial, bound->ctx, table, params, node->binder.body);
    if (body.kind == LOWER_FAIL) return body;
    return pure((void *)rules_arrow(heap, QUANTITY_MANY, node->binder.name, bound->domain, body.value));
  }
  case TRANSLATE_LAMBDA: {
    LowerResult inner = bind_domain(heap, state, depth, initial, ctx, table, params, node->binder.typ, node->binder.name);
    if (inner.kind == LOWER_FAIL) return inner;
    LowerBoundContext *bound = inner.value;
    LowerResult body = lower(heap, state, depth + 1, initial, bound->ctx, table, params, node->binder.body);
    if (body.kind == LOWER_FAIL) return body;
    LowerResult body_type = infer(heap, state, bound->ctx, body.value);
    if (body_type.kind == LOWER_FAIL) return body_type;
    LowerResult codomain = quote(heap, state, bound->ctx, body_type.value);
    if (codomain.kind == LOWER_FAIL) return codomain;
    return lambda_term(heap, node->binder.name, bound->domain, body.value, codomain.value);
  }
  case TRANSLATE_LET: {
    LowerResult typ = lower(heap, state, depth + 1, initial, ctx, table, params, node->typ);
    if (typ.kind == LOWER_FAIL) return typ;
    LowerResult level = universe(heap, state, ctx, typ.value);
    if (level.kind == LOWER_FAIL) return level;
    LowerResult typ_value = evaluate(heap, state, ctx, typ.value);
    if (typ_value.kind == LOWER_FAIL) return typ_value;
    LowerResult value = lower(heap, state, depth + 1, initial, ctx, table, params, node->value);
    if (value.kind == LOWER_FAIL) return value;
    LowerResult unit = check(heap, state, ctx, value.value, typ_value.value);
    if (unit.kind == LOWER_FAIL) return unit;
    LowerResult value_value = evaluate(heap, state, ctx, value.value);
    if (value_value.kind == LOWER_FAIL) return value_value;
    Ctx inner = check_define(heap, node->name, QUANTITY_MANY, typ_value.value, value_value.value, ctx);
    LowerResult body = lower(heap, state, depth + 1, initial, inner, table, params, node->body);
    if (body.kind == LOWER_FAIL) return body;
    return pure((void *)term_let(heap, node->name, typ.value, value.value, body.value));
  }
  case TRANSLATE_PROJECTION:
    return failed(unsupported(heap_str_cat(
        heap, heap_str_cat(heap, heap_str_cat(heap, str_lit("projection "), node->type_name), str_lit(".")),
        heap_str_cat(heap, node->index, str_lit(" needs a checked prelude representation")))));
  case TRANSLATE_NAT_LITERAL: {
    MaybeBig value = big_of_decimal(heap, node->value);
    if (!value.some) return failed(unsupported(str_lit("invalid natural literal in a type")));
    return pure((void *)term_lit(heap, literal_lint(value.value)));
  }
  case TRANSLATE_STRING_LITERAL:
    return failed(unsupported(str_lit("strVal in a type arrives at M1 with String")));
  case TRANSLATE_METADATA:
    return lower(heap, state, depth + 1, initial, ctx, table, params, node->expr);
  }
  return failed(unsupported(str_lit("missing type expression")));
}

/* lower(depth, initial, ctx, table, params, id) -> const Term * */
static LowerResult lower(Heap *heap, LowerState *state, uint32_t depth, Ctx initial, Ctx ctx, Translated *table,
                         const Vec *params, Str id) {
  LowerResult unit = step(heap, state, depth, str_lit("type"));
  if (unit.kind == LOWER_FAIL) return unit;
  LowerResult node = require(table_get(translate_type_nodes(table), id),
                             unsupported(heap_str_cat(heap, str_lit("missing type expression "), id)));
  if (node.kind == LOWER_FAIL) return node;
  return lower_node(heap, state, node.value, depth, initial, ctx, table, params);
}

/* lower_row(row, globals, budget, table) -> const Term * */
LowerResult lower_row(Heap *heap, LowerState *state, const TranslateRow *row, Globals globals, Budget budget,
                      Translated *table) {
  Ctx initial = check_make(heap, globals, budget, nat_of_u64(heap->arena, (uint64_t)row->parameters.len));
  LowerResult term = lower(heap, state, 0, initial, initial, table, &row->parameters, row->root);
  if (term.kind == LOWER_FAIL) return term;
  LowerResult level = universe(heap, state, initial, term.value);
  if (level.kind == LOWER_FAIL) return level;
  return term;
}

/* lower_type(resolver, globals, table, row, budget): discard(lower_row(...)(State{100000, budget, resolver})) */
LowerResult lower_type(Heap *heap, LowerResolver resolver, Globals globals, Translated *table, const TranslateRow *row,
                       Budget budget) {
  LowerState state;
  state.work = nat_of_u64(heap->arena, 100000);
  state.budget = budget;
  state.resolver = resolver;
  return lower_row(heap, &state, row, globals, budget, table);
}

/* lower_default(resolver, globals, table, row) */
LowerResult lower_default(Heap *heap, LowerResolver resolver, Globals globals, Translated *table,
                          const TranslateRow *row) {
  return lower_type(heap, resolver, globals, table, row, budget_of_polls(nat_of_u64(heap->arena, 100000)));
}
