/* Port of kernel/rules.bend: the typing rule helpers shared by the checker (arrows, finite
 * collections, unit and bool, mu parameter environments, mpc party sets, elimination results) and
 * the eta table. Same definitions in the same order. List<Term.T> is a Vec of Term pointers,
 * List<Value.T> a Vec of Value pointers, List<Value.VLeg<Value.T>> a Vec of VLeg pointers,
 * List<String> a Vec of Str pointers, List<Shape.T<Term.T>> a Vec of Shape pointers and
 * List<Common.Pair<String, EtaRow>> a Vec of EtaEntry pointers. */
#include "kernel.h"

static Result done(const void *value) { return budget_result_done((void *)value); }

static Result fail(ErrorKind kind, const char *message) {
  return budget_result_fail(error_make(kind, str_lit(message)));
}

static void push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static void push_str(Heap *heap, Vec *xs, Str s) {
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) return;
  *boxed = s;
  push(heap, xs, boxed);
}

static Vec str_list(Heap *heap, const char *const *names, size_t count) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < count; i++) push_str(heap, &out, str_lit(names[i]));
  return out;
}

static VecResult vec_done(Vec value) {
  VecResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_make(ERROR_NOT_YET, str_empty());
  return r;
}

static VecResult vec_fail(Heap *heap, Error error) {
  VecResult r;
  r.kind = RESULT_FAIL;
  r.value = vec_new(heap->arena);
  r.error = error;
  return r;
}

static FamilyResult family_done(Family value) {
  FamilyResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_make(ERROR_NOT_YET, str_empty());
  return r;
}

static FamilyResult family_fail(Error error) {
  FamilyResult r;
  Family none = {{NULL, 0}, {NULL, NULL, 0, 0}, {NULL, NULL, 0, 0}, NULL, {CTOR_STATUS_PROVISIONAL, {NULL, NULL, 0, 0}},
                 {NULL, NULL, 0, 0}, false};
  r.kind = RESULT_FAIL;
  r.value = none;
  r.error = error;
  return r;
}

static BigResult big_done(Big value) {
  BigResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_make(ERROR_NOT_YET, str_empty());
  return r;
}

static BigResult big_fail(Error error) {
  BigResult r;
  r.kind = RESULT_FAIL;
  r.value = big_zero();
  r.error = error;
  return r;
}

static const char *const not_finite = "the party set of an mpc type is a finite enumeration";

const Term *rules_arrow(Heap *heap, Quantity q, Str name, const Term *dom, const Term *cod) {
  return term_ran(heap, shape_spi(q, name, (void *)dom), cod);
}

Vec rules_term_legs(Heap *heap, const Vec *items) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < items->len; i++) {
    Leg *leg = term_leg_new(heap, vec_new(heap->arena), at(items, i));
    if (leg == NULL) return out;
    push(heap, &out, leg);
  }
  return out;
}

const Term *rules_diagram_of(Heap *heap, const Vec *items) {
  Shape coll = shape_coll(heap, heap_nat_of_u64(heap, items->len));
  return term_sec(heap, coll, rules_term_legs(heap, items));
}

const Term *rules_sum_ty(Heap *heap, const Vec *items) {
  Shape coll = shape_coll(heap, heap_nat_of_u64(heap, items->len));
  return term_lan(heap, coll, rules_diagram_of(heap, items));
}

const Term *rules_prod_ty(Heap *heap, const Vec *items) {
  Shape coll = shape_coll(heap, heap_nat_of_u64(heap, items->len));
  return term_ran(heap, coll, rules_diagram_of(heap, items));
}

const Term *rules_unit_ty(Heap *heap, const LevelVar *level) {
  Vec none = vec_new(heap->arena);
  return term_ann(heap, rules_prod_ty(heap, &none), term_univ(heap, level));
}

const Term *rules_unit_val(Heap *heap) {
  return term_sec(heap, shape_coll(heap, nat_zero()), vec_new(heap->arena));
}

const Term *rules_bool_ty(Heap *heap) {
  return prim_bool_ty(heap);
}

const Value *rules_bool_value(Heap *heap, bool value) {
  return prim_bool_value(heap, value);
}

VecResult rules_coll_legs(Heap *heap, const Value *value) {
  if (value->kind == VALUE_SEC) return vec_done(value->legs);
  return vec_fail(heap, error_make(ERROR_MISMATCH, str_lit("the diagram is not a section of the declared width")));
}

VecResult rules_coll_legs_of(Heap *heap, Ctx ctx, Closure clo) {
  Result value = eval_open_closure(heap, context_globals_of(ctx), clo, vec_new(heap->arena));
  if (value.kind == RESULT_FAIL) return vec_fail(heap, value.error);
  return rules_coll_legs(heap, value.value);
}

Result rules_coll_leg_ty_found(Heap *heap, VecGet leg, Ctx ctx) {
  if (leg.status != VEC_GET_OK) return fail(ERROR_WRONG_LEG, "the leg address is outside the collection");
  {
    const VLeg *found = leg.value;
    return eval_open_closure(heap, context_globals_of(ctx), found->clo, vec_new(heap->arena));
  }
}

Result rules_coll_leg_ty(Heap *heap, Ctx ctx, const Vec *legs, Big index) {
  return rules_coll_leg_ty_found(heap, big_at(heap, legs, index), ctx);
}

VecResult rules_force_legs(Heap *heap, const Vec *legs, Ctx ctx) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < legs->len; i++) {
    const VLeg *leg = at(legs, i);
    Result value = eval_open_closure(heap, context_globals_of(ctx), leg->clo, vec_new(heap->arena));
    if (value.kind == RESULT_FAIL) return vec_fail(heap, value.error);
    push(heap, &out, value.value);
  }
  return vec_done(out);
}

VecResult rules_mu_param_env(Heap *heap, Ctx ctx, Closure clo) {
  VecResult legs = rules_coll_legs_of(heap, ctx, clo);
  if (legs.kind == RESULT_FAIL) return legs;
  {
    VecResult values = rules_force_legs(heap, &legs.value, ctx);
    if (values.kind == RESULT_FAIL) return values;
    return vec_done(vec_reverse(heap->arena, &values.value));
  }
}

FamilyResult rules_family_ready(Family family) {
  if (family.status.kind == CTOR_STATUS_COMPLETE && !family.positive) {
    return family_fail(error_make(ERROR_NOT_YET, str_lit("a family that is not strictly positive arrives at M2")));
  }
  return family_done(family);
}

FamilyResult rules_family_found(Heap *heap, MaybeFamily family, Str name) {
  if (!family.some) {
    Str message = heap_str_cat(heap, heap_str_cat(heap, str_lit("the family "), name), str_lit(" is not declared"));
    return family_fail(error_make(ERROR_UNBOUND, message));
  }
  return rules_family_ready(family.value);
}

FamilyResult rules_mu_family(Heap *heap, Ctx ctx, Str name) {
  return rules_family_found(heap, global_find_family(name, context_globals_of(ctx)), name);
}

Vec rules_mpc_fields(Heap *heap, Cmp sign, Big count, const Vec *acc) {
  Vec out = vec_new(heap->arena);
  size_t i;
  while (sign == CMP_GT) {
    Big next = big_sub(heap, count, big_one());
    push(heap, &out, term_global(heap, str_lit("Nat")));
    if (heap->oom) return out;
    sign = big_sign(next);
    count = next;
  }
  for (i = 0; i < acc->len; i++) push(heap, &out, at(acc, i));
  return out;
}

Result rules_mpc_sub_count(Heap *heap, Cmp sign, Big count) {
  switch (sign) {
    case CMP_LT: return fail(ERROR_MISMATCH, "an mpc subset width must be nonnegative");
    case CMP_EQ:
    case CMP_GT: {
      Vec none = vec_new(heap->arena);
      Vec fields = rules_mpc_fields(heap, sign, count, &none);
      return done(rules_prod_ty(heap, &fields));
    }
  }
  return fail(ERROR_MISMATCH, "an mpc subset width must be nonnegative");
}

Result rules_mpc_sub_ty(Heap *heap, Big count) {
  return rules_mpc_sub_count(heap, big_sign(count), count);
}

Result rules_mpc_pred_ty(Heap *heap, Big count) {
  Result subset = rules_mpc_sub_ty(heap, count);
  if (subset.kind == RESULT_FAIL) return subset;
  return done(rules_arrow(heap, QUANTITY_ZERO, str_lit("q"), subset.value, term_univ(heap, level_zero())));
}

Result rules_mpc_party_count(Big count) {
  if (big_equal(count, big_zero())) return done(kernel_unit());
  return fail(ERROR_MISMATCH, not_finite);
}

Result rules_mpc_party_value(const Value *value) {
  if (value->kind == VALUE_RAN && value->shape.kind == SHAPE_COLL) return rules_mpc_party_count(value->shape.count);
  return fail(ERROR_MISMATCH, not_finite);
}

Result rules_mpc_party(Heap *heap, Ctx ctx, const Value *value) {
  Result reduced = eval_whnf(heap, context_globals_of(ctx), value);
  if (reduced.kind == RESULT_FAIL) return reduced;
  return rules_mpc_party_value(reduced.value);
}

Result rules_mpc_party_legs(Heap *heap, const Vec *legs, Ctx ctx) {
  size_t i;
  for (i = 0; i < legs->len; i++) {
    const VLeg *leg = at(legs, i);
    Result value = eval_open_closure(heap, context_globals_of(ctx), leg->clo, vec_new(heap->arena));
    if (value.kind == RESULT_FAIL) return value;
    {
      Result ok = rules_mpc_party(heap, ctx, value.value);
      if (ok.kind == RESULT_FAIL) return ok;
    }
  }
  return done(kernel_unit());
}

BigResult rules_mpc_parties_value(Heap *heap, const Value *value, Ctx ctx) {
  if (value->kind == VALUE_LAN && value->shape.kind == SHAPE_COLL) {
    VecResult legs = rules_coll_legs_of(heap, ctx, value->diagram);
    if (legs.kind == RESULT_FAIL) return big_fail(legs.error);
    {
      Result ok = rules_mpc_party_legs(heap, &legs.value, ctx);
      if (ok.kind == RESULT_FAIL) return big_fail(ok.error);
      return big_done(value->shape.count);
    }
  }
  return big_fail(error_make(ERROR_MISMATCH, str_lit(not_finite)));
}

BigResult rules_mpc_parties(Heap *heap, Ctx ctx, const Value *parties) {
  Result value = eval_whnf(heap, context_globals_of(ctx), parties);
  if (value.kind == RESULT_FAIL) return big_fail(value.error);
  return rules_mpc_parties_value(heap, value.value, ctx);
}

/* self <> List.append(List.reverse(indices), env) */
static Vec extend_env(Heap *heap, const Value *self, const Vec *indices, Vec env) {
  Vec out = vec_new(heap->arena);
  size_t i;
  push(heap, &out, self);
  for (i = indices->len; i > 0; i--) push(heap, &out, at(indices, i - 1));
  for (i = 0; i < env.len; i++) push(heap, &out, at(&env, i));
  return out;
}

Result rules_mu_result(Heap *heap, Ctx ctx, Motive motive, const Vec *indices, const Value *self) {
  Vec env = extend_env(heap, self, indices, context_env_of(ctx));
  return eval_eval(heap, context_globals_of(ctx), env, motive.body);
}

Result rules_elim_result(Heap *heap, Ctx ctx, MaybeMotive motive, MaybeValue expected, const Value *self) {
  if (motive.some) {
    Vec none = vec_new(heap->arena);
    Vec env = extend_env(heap, self, &none, context_env_of(ctx));
    return eval_eval(heap, context_globals_of(ctx), env, motive.value.body);
  }
  if (expected.some) return done(expected.value);
  return fail(ERROR_CANNOT_INFER, "an elimination without a motive needs an expected type");
}

static MaybeEtaRow eta_some(bool ran, bool lan) {
  MaybeEtaRow some;
  some.some = true;
  some.value.ran = ran;
  some.value.lan = lan;
  return some;
}

static MaybeEtaRow eta_none(void) {
  MaybeEtaRow none = {false, {false, false}};
  return none;
}

MaybeEtaRow rules_eta_of(Shape shape) {
  switch (shape.kind) {
    case SHAPE_PI: return eta_some(true, true);
    case SHAPE_ARG: return eta_none();
    case SHAPE_COLL: return eta_some(true, false);
    case SHAPE_PAR: return eta_none();
    case SHAPE_NU: return eta_none();
    case SHAPE_MU: return eta_some(false, false);
    case SHAPE_ZK: return eta_some(false, false);
    case SHAPE_FHC: return eta_some(false, false);
    case SHAPE_MPC: return eta_some(false, false);
  }
  return eta_none();
}

static void push_shape(Heap *heap, Vec *xs, Shape shape) {
  Shape *boxed = heap_alloc(heap, sizeof(Shape));
  if (boxed == NULL) return;
  *boxed = shape;
  push(heap, xs, boxed);
}

Vec rules_samples(Heap *heap) {
  Vec out = vec_new(heap->arena);
  const Term *univ = term_univ(heap, level_zero());
  push_shape(heap, &out, shape_spi(QUANTITY_MANY, str_lit("x"), (void *)univ));
  push_shape(heap, &out, shape_coll(heap, nat_zero()));
  push_shape(heap, &out, shape_spar((void *)term_univ(heap, level_zero()), (void *)term_univ(heap, level_zero())));
  push_shape(heap, &out, shape_smu(str_lit("x"), vec_new(heap->arena)));
  push_shape(heap, &out, shape_snu(str_lit("x"), vec_new(heap->arena)));
  push_shape(heap, &out, shape_szk(QUANTITY_ZERO, str_lit("w"), (void *)term_univ(heap, level_zero())));
  push_shape(heap, &out, shape_sfhc((void *)term_univ(heap, level_zero())));
  push_shape(heap, &out, shape_smpc((void *)term_univ(heap, level_zero()), (void *)term_univ(heap, level_zero())));
  return out;
}

Vec rules_eta_entry(Heap *heap, MaybeEtaRow row, Str name) {
  Vec out = vec_new(heap->arena);
  if (!row.some) return out;
  {
    EtaEntry *entry = heap_alloc(heap, sizeof(EtaEntry));
    if (entry == NULL) return out;
    entry->name = name;
    entry->eta = row.value;
    push(heap, &out, entry);
    return out;
  }
}

Vec rules_eta_rows(Heap *heap, const Vec *shapes) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < shapes->len; i++) {
    const Shape *s = at(shapes, i);
    Vec entry = rules_eta_entry(heap, rules_eta_of(*s), shape_name(*s));
    size_t j;
    for (j = 0; j < entry.len; j++) push(heap, &out, at(&entry, j));
  }
  return out;
}

Vec rules_eta_table(Heap *heap) {
  Vec samples = rules_samples(heap);
  return rules_eta_rows(heap, &samples);
}

Vec rules_admitted_names(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < rows->len; i++) {
    const EtaEntry *row = at(rows, i);
    push_str(heap, &out, row->name);
  }
  return out;
}

Vec rules_admitted(Heap *heap) {
  Vec table = rules_eta_table(heap);
  return rules_admitted_names(heap, &table);
}

Vec rules_named_declared(Heap *heap) {
  static const char *const names[] = {"proof-irrelevance", "subsingleton-large-elimination", "literal-fast-path",
                                      "zk-fhc", "zk-mpc", "fhc-mpc"};
  return str_list(heap, names, sizeof names / sizeof names[0]);
}

Vec rules_named_present(Heap *heap) {
  static const char *const names[] = {"proof-irrelevance", "subsingleton-large-elimination", "literal-fast-path"};
  return str_list(heap, names, sizeof names / sizeof names[0]);
}
