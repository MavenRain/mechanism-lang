/* bend2/prelude/dependent.bend: the Pi and Sigma catalog (MechPi, MechSigma, mechSigmaMk,
 * mechSigmaFst, mechSigmaSnd, mechSigmaRec) as a Poly catalog. */
#include "prelude.h"

static Nat nat(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

static Nat nat_plus(Heap *heap, uint64_t amount, Nat n) { return heap_nat_add(heap, nat(heap, amount), n); }

static Vec binders1(Heap *heap, TeleBinder *a) {
  Vec items = vec_new(heap->arena);
  if (!vec_push(&items, a)) heap->oom = true;
  return items;
}

static Vec binders2(Heap *heap, TeleBinder *a, TeleBinder *b) {
  Vec items = binders1(heap, a);
  if (!vec_push(&items, b)) heap->oom = true;
  return items;
}

static Vec binders3(Heap *heap, TeleBinder *a, TeleBinder *b, TeleBinder *c) {
  Vec items = binders2(heap, a, b);
  if (!vec_push(&items, c)) heap->oom = true;
  return items;
}

/* List.append(&2, P.Binder, left, right) */
static Vec binders_append(Heap *heap, const Vec *left, const Vec *right) {
  Vec items = vec_new(heap->arena);
  size_t i = 0;
  for (i = 0; i < left->len; i = i + 1) {
    VecGet got = vec_get(left, i);
    if (got.status == VEC_GET_OK && !vec_push(&items, got.value)) heap->oom = true;
  }
  for (i = 0; i < right->len; i = i + 1) {
    VecGet got = vec_get(right, i);
    if (got.status == VEC_GET_OK && !vec_push(&items, got.value)) heap->oom = true;
  }
  return items;
}

static TeleBinder *zero_binder(Heap *heap, const char *name, const Term *ty) {
  return positivity_binder_new(heap, QUANTITY_ZERO, str_lit(name), ty);
}

static TeleBinder *many_binder(Heap *heap, const char *name, const Term *ty) {
  return positivity_binder_new(heap, QUANTITY_MANY, str_lit(name), ty);
}

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, nat(heap, index)); }

/* Nonnegative universe indices are enforced by Nat in the native API. */
Result dependent_variable(Heap *heap, Nat index) { return budget_result_done((void *)level_var(heap, index)); }

const Term *dependent_arrow(Heap *heap, TeleBinder binder, const Term *body) {
  return term_ran(heap, shape_spi(binder.quantity, binder.name, (void *)binder.ty), body);
}

const Term *dependent_lambda(Heap *heap, TeleBinder binder, const Term *body) {
  Vec names = vec_new(heap->arena);
  Vec legs = vec_new(heap->arena);
  Binder *leg_binder = term_binder_new(heap, binder.quantity, binder.name);
  if (leg_binder == NULL || !vec_push(&names, leg_binder)) heap->oom = true;
  Leg *leg = term_leg_new(heap, names, body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, shape_spi(binder.quantity, binder.name, (void *)binder.ty), legs);
}

static const Term *arrows_from(Heap *heap, const Vec *binders, size_t index, const Term *body) {
  VecGet got = vec_get(binders, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return body;
  return dependent_arrow(heap, *(const TeleBinder *)got.value, arrows_from(heap, binders, index + 1, body));
}

const Term *dependent_arrows(Heap *heap, const Vec *binders, const Term *body) {
  return arrows_from(heap, binders, 0, body);
}

static const Term *lambdas_from(Heap *heap, const Vec *binders, size_t index, const Term *body) {
  VecGet got = vec_get(binders, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return body;
  return dependent_lambda(heap, *(const TeleBinder *)got.value, lambdas_from(heap, binders, index + 1, body));
}

const Term *dependent_lambdas(Heap *heap, const Vec *binders, const Term *body) {
  return lambdas_from(heap, binders, 0, body);
}

Decl dependent_definition(Heap *heap, Str name, const Vec *binders, const Term *result, const Term *body) {
  Decl decl;
  decl.name = name;
  decl.kind = DECL_DEFINITION;
  decl.ty = dependent_arrows(heap, binders, result);
  decl.body.some = true;
  decl.body.value = dependent_lambdas(heap, binders, body);
  return decl;
}

Shape dependent_point(const Term *domain) { return shape_spi(QUANTITY_MANY, str_lit("point"), (void *)domain); }

const Term *dependent_apply(Heap *heap, const Term *domain, const Term *head, const Term *argument) {
  return term_out(heap, dependent_point(domain), term_addr_pt(QUANTITY_MANY, argument), head);
}

const Term *dependent_sigma(Heap *heap, Nat a, Nat b) {
  return term_lan(heap, dependent_point(term_var(heap, a)),
                  dependent_apply(heap, term_var(heap, nat_plus(heap, 1, a)), term_var(heap, nat_plus(heap, 1, b)), var(heap, 0)));
}

const Term *dependent_pair(Heap *heap, Nat a, const Term *x, const Term *y) {
  Vec args = vec_new(heap->arena);
  if (!vec_push(&args, (void *)y)) heap->oom = true;
  return term_in(heap, dependent_point(term_var(heap, a)), term_addr_pt(QUANTITY_MANY, x), args);
}

const Term *dependent_eliminate(Heap *heap, Nat a, const Term *scrut, const Term *motive, Vec branches) {
  MaybeStr ind;
  ind.some = false;
  ind.value = str_lit("");
  return term_elim(heap, term_elim_data(dependent_point(term_var(heap, a)), scrut, QUANTITY_ONE,
                                        term_motive_some(term_motive(ind, vec_new(heap->arena), str_lit("self"), motive)),
                                        branches));
}

/* Nat.sub(1n, which): truncated at zero. */
static Nat one_minus(Heap *heap, Nat which) {
  NatSub sub = nat_sub(heap->arena, nat(heap, 1), which);
  switch (sub.status) {
    case NAT_SUB_OK: return sub.value;
    case NAT_SUB_UNDERFLOW: return nat(heap, 0);
    case NAT_SUB_OOM: heap->oom = true; return nat(heap, 0);
  }
  return nat(heap, 0);
}

Vec dependent_proj_branch(Heap *heap, Nat which) {
  Vec names = vec_new(heap->arena);
  Vec branches = vec_new(heap->arena);
  Binder *x = term_binder_new(heap, QUANTITY_MANY, str_lit("x"));
  Binder *y = term_binder_new(heap, QUANTITY_MANY, str_lit("y"));
  if (x == NULL || y == NULL || !vec_push(&names, x) || !vec_push(&names, y)) heap->oom = true;
  Branch *branch = term_branch_new(heap, term_aleg(heap, nat(heap, 0)), term_leg(names, term_var(heap, one_minus(heap, which))));
  if (branch == NULL || !vec_push(&branches, branch)) heap->oom = true;
  return branches;
}

const Term *dependent_first(Heap *heap, Nat a, const Term *scrut) {
  return dependent_eliminate(heap, a, scrut, term_var(heap, nat_plus(heap, 1, a)), dependent_proj_branch(heap, nat(heap, 0)));
}

Decl dependent_pi(Heap *heap, const LevelVar *u, const LevelVar *v) {
  TeleBinder x0 = positivity_binder(QUANTITY_MANY, str_lit("x"), var(heap, 0));
  TeleBinder x1 = positivity_binder(QUANTITY_MANY, str_lit("x"), var(heap, 1));
  Vec binders = binders2(heap, zero_binder(heap, "A", term_univ(heap, u)),
                         zero_binder(heap, "B", dependent_arrow(heap, x0, term_univ(heap, v))));
  return dependent_definition(heap, str_lit("MechPi"), &binders, term_univ(heap, level_imax(heap, u, v)),
                              dependent_arrow(heap, x1, dependent_apply(heap, var(heap, 2), var(heap, 1), var(heap, 0))));
}

Vec dependent_parameters(Heap *heap, const LevelVar *u, const LevelVar *v) {
  TeleBinder x0 = positivity_binder(QUANTITY_MANY, str_lit("x"), var(heap, 0));
  return binders2(heap, zero_binder(heap, "A", term_univ(heap, level_succ(heap, u))),
                  zero_binder(heap, "B", dependent_arrow(heap, x0, term_univ(heap, level_succ(heap, v)))));
}

Decl dependent_sigma_type(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec params = dependent_parameters(heap, u, v);
  return dependent_definition(heap, str_lit("MechSigma"), &params,
                              term_univ(heap, level_max(heap, level_succ(heap, u), level_succ(heap, v))),
                              dependent_sigma(heap, nat(heap, 1), nat(heap, 0)));
}

Decl dependent_make(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec params = dependent_parameters(heap, u, v);
  Vec fields = binders2(heap, many_binder(heap, "x", var(heap, 1)),
                        many_binder(heap, "y", dependent_apply(heap, var(heap, 2), var(heap, 1), var(heap, 0))));
  Vec binders = binders_append(heap, &params, &fields);
  return dependent_definition(heap, str_lit("mechSigmaMk"), &binders, dependent_sigma(heap, nat(heap, 3), nat(heap, 2)),
                              dependent_pair(heap, nat(heap, 3), var(heap, 1), var(heap, 0)));
}

Vec dependent_projections(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec params = dependent_parameters(heap, u, v);
  Vec pair = binders1(heap, many_binder(heap, "pair", dependent_sigma(heap, nat(heap, 1), nat(heap, 0))));
  return binders_append(heap, &params, &pair);
}

Decl dependent_fst(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec binders = dependent_projections(heap, u, v);
  return dependent_definition(heap, str_lit("mechSigmaFst"), &binders, var(heap, 2),
                              dependent_first(heap, nat(heap, 2), var(heap, 0)));
}

Decl dependent_snd(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec binders = dependent_projections(heap, u, v);
  return dependent_definition(
      heap, str_lit("mechSigmaSnd"), &binders,
      dependent_apply(heap, var(heap, 2), var(heap, 1), dependent_first(heap, nat(heap, 2), var(heap, 0))),
      dependent_eliminate(heap, nat(heap, 2), var(heap, 0),
                          dependent_apply(heap, var(heap, 3), var(heap, 2), dependent_first(heap, nat(heap, 3), var(heap, 0))),
                          dependent_proj_branch(heap, nat(heap, 1))));
}

Decl dependent_recursor(Heap *heap, const LevelVar *u, const LevelVar *v, const LevelVar *w) {
  Vec params = dependent_parameters(heap, u, v);
  TeleBinder pair_binder = positivity_binder(QUANTITY_MANY, str_lit("pair"), dependent_sigma(heap, nat(heap, 1), nat(heap, 0)));
  TeleBinder x2 = positivity_binder(QUANTITY_MANY, str_lit("x"), var(heap, 2));
  TeleBinder y = positivity_binder(QUANTITY_MANY, str_lit("y"), dependent_apply(heap, var(heap, 3), var(heap, 2), var(heap, 0)));
  const Term *step_ty = dependent_arrow(
      heap, x2,
      dependent_arrow(heap, y,
                      dependent_apply(heap, dependent_sigma(heap, nat(heap, 4), nat(heap, 3)), var(heap, 2),
                                      dependent_pair(heap, nat(heap, 4), var(heap, 1), var(heap, 0)))));
  Vec fields = binders3(heap, zero_binder(heap, "P", dependent_arrow(heap, pair_binder, term_univ(heap, w))),
                        many_binder(heap, "step", step_ty),
                        many_binder(heap, "pair", dependent_sigma(heap, nat(heap, 3), nat(heap, 2))));
  Vec binders = binders_append(heap, &params, &fields);
  Vec names = vec_new(heap->arena);
  Vec branches = vec_new(heap->arena);
  Binder *x = term_binder_new(heap, QUANTITY_MANY, str_lit("x"));
  Binder *yb = term_binder_new(heap, QUANTITY_MANY, str_lit("y"));
  if (x == NULL || yb == NULL || !vec_push(&names, x) || !vec_push(&names, yb)) heap->oom = true;
  const Term *leg_body = dependent_apply(
      heap, dependent_apply(heap, var(heap, 6), var(heap, 5), var(heap, 1)),
      dependent_apply(heap, var(heap, 6), var(heap, 3), var(heap, 1)), var(heap, 0));
  Branch *branch = term_branch_new(heap, term_aleg(heap, nat(heap, 0)), term_leg(names, leg_body));
  if (branch == NULL || !vec_push(&branches, branch)) heap->oom = true;
  return dependent_definition(
      heap, str_lit("mechSigmaRec"), &binders,
      dependent_apply(heap, dependent_sigma(heap, nat(heap, 4), nat(heap, 3)), var(heap, 2), var(heap, 0)),
      dependent_eliminate(heap, nat(heap, 4), var(heap, 0),
                          dependent_apply(heap, dependent_sigma(heap, nat(heap, 5), nat(heap, 4)), var(heap, 3), var(heap, 0)),
                          branches));
}

/* The do-block of catalog_comp: each bind receives the catalog so far and declares the next entry. */
typedef struct {
  Globals globals;
  const LevelVar *u;
  const LevelVar *v;
  const LevelVar *w;
} CatalogFrame;

static Comp bind_to(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), CatalogFrame *frame) {
  NextK next;
  next.fn = fn;
  next.ctx = frame;
  return budget_bind(heap, first, next);
}

static Comp after_recursor(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  return poly_declare_comp(heap, frame->globals, (PolyCatalog)value, nat(heap, 3), dependent_recursor(heap, frame->u, frame->v, frame->w));
}

static Comp after_snd(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  return bind_to(heap, poly_declare_comp(heap, frame->globals, (PolyCatalog)value, nat(heap, 2), dependent_snd(heap, frame->u, frame->v)),
                 after_recursor, frame);
}

static Comp after_fst(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  return bind_to(heap, poly_declare_comp(heap, frame->globals, (PolyCatalog)value, nat(heap, 2), dependent_fst(heap, frame->u, frame->v)),
                 after_snd, frame);
}

static Comp after_make(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  return bind_to(heap, poly_declare_comp(heap, frame->globals, (PolyCatalog)value, nat(heap, 2), dependent_make(heap, frame->u, frame->v)),
                 after_fst, frame);
}

static Comp after_sigma(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  return bind_to(heap,
                 poly_declare_comp(heap, frame->globals, (PolyCatalog)value, nat(heap, 2), dependent_sigma_type(heap, frame->u, frame->v)),
                 after_make, frame);
}

Comp dependent_catalog_comp(Heap *heap, Globals globals) {
  CatalogFrame *frame = heap_alloc(heap, sizeof(CatalogFrame));
  if (frame == NULL) return budget_failed(error_host_oom());
  frame->globals = globals;
  frame->u = level_var(heap, nat(heap, 0));
  frame->v = level_var(heap, nat(heap, 1));
  frame->w = level_var(heap, nat(heap, 2));
  return bind_to(heap, poly_declare_comp(heap, globals, poly_empty(), nat(heap, 2), dependent_pi(heap, frame->u, frame->v)),
                 after_sigma, frame);
}

Result dependent_catalog(Heap *heap, Globals globals, Budget budget) {
  return budget_run(heap, dependent_catalog_comp(heap, globals), budget);
}
