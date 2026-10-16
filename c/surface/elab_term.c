/* elab_term.c: port of bend2/surface/elab_term.bend (surface elaboration of terms).
 * Chunk 1 = Bend 1-432: the record types MuRef, Point, Spine, Former and Bound and the definitions
 * no_expect .. elab_pair, in Bend order. Comp plumbing follows c/kernel/erase.c: one ElabFrame per do
 * block, each `<-` a static continuation that reads the frame from ctx. */
#include "surface.h"

/* Common.Pair<Value.T, Value.T> (fhc_expect, mpc_shape). */
typedef struct {
  const Value *first;
  const Value *second;
} ValuePair;

/* Bend MpcExpected{parties, access, parties_value, ty}. */
typedef struct {
  const Term *parties;
  const Term *access;
  const Value *parties_value;
  const Value *ty;
} MpcExpected;

/* Bend ConstructorFields{terms, env}. */
typedef struct {
  Vec terms;
  Vec env;
} ConstructorFields;

typedef struct ElabFrame ElabFrame;
struct ElabFrame {
  Ctx ctx;
  MaybeValue expected;
  bool left;
  Str message;
  Nat count;
  Big width;
  ElabFormer former;
  ElabPoint point;
  const STerm *left_syntax;
  const STerm *right_syntax;
  const Term *term;
  const Term *left_term;
  const Value *value;
  Vec items;
  Vec rest;
  /* chunk 2 */
  ElabBound bound;
  MaybeValue cod;
  Str word;
  Quantity quantity;
  uint32_t group_kind;
  bool evaluate;
  const Value *level_value;
  const Value *ty_value;
  ValuePair pair;
  MpcExpected mpc;
  Big count_big;
  Big index;
  Nat which;
  Shape shape;
  Closure diagram;
  Family family;
  Ctor ctor;
  Vec env;
  Vec terms;
  Vec args;
  Vec binders;
  Vec shares;
  const Term *head_term;
  const Term *arg_term;
  const Term *key;
  const Term *cipher;
  const Term *scrut;
  const Term *subset;
  const Term *parties_term;
  const Term *function_term;
  const Term *proof;
  const Term *instance;
  const Term *witness;
  const Term *relation;
  const Term *body;
  const STerm *syntax;
  const STerm *body_syntax;
  const STerm *party_syntax;
  const STerm *value_syntax;
  const STerm *proof_syntax;
  const STerm *parties_syntax;
  const STerm *function_syntax;
  const STerm *witness_syntax;
  const STerm *relation_syntax;
};

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static ElabFrame *frame_new(Heap *heap) { return heap_alloc(heap, sizeof(ElabFrame)); }

static Comp bind_with(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return budget_bind(heap, first, next);
}

static Comp bind_frame(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), ElabFrame *frame) {
  if (frame == NULL) return comp_oom();
  return bind_with(heap, first, fn, frame);
}

static Comp mismatch(const char *message) {
  Str text = str_lit(message);
  return budget_failed(error_make(ERROR_MISMATCH, text));
}

/* Tree nodes are NULL after a failed allocation: the Comp fails with the host oom error. */
static Comp pure_term(const Term *term) {
  if (term == NULL) return comp_oom();
  return budget_pure((void *)term);
}

static Comp pure_value(const Value *value) {
  if (value == NULL) return comp_oom();
  return budget_pure((void *)value);
}

static Vec empty(Heap *heap) { return vec_new(heap->arena); }

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

/* [item] */
static Vec singleton(Heap *heap, void *item) {
  Vec out = empty(heap);
  push(heap, &out, item);
  return out;
}

/* Bend `x <> xs`: a fresh list with item first. */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec out = singleton(heap, item);
  size_t i;
  for (i = 0; i < rest->len; i++) {
    VecGet got = vec_get(rest, i);
    if (got.status == VEC_GET_OK) push(heap, &out, got.value);
  }
  return out;
}

/* The Bend list tail: the same items from index 1, never pushed to. */
static Vec list_tail(Vec xs) {
  Vec out = xs;
  if (xs.len == 0) return xs;
  out.items = xs.items + 1;
  out.len = xs.len - 1;
  out.cap = out.len;
  return out;
}

static Str cats(Heap *heap, size_t count, const Str *parts) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

/* Comp values are pointers: a list result is a boxed Vec. */
static Comp pure_vec(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = items;
  return budget_pure(boxed);
}

static Comp lift_vec(Heap *heap, VecResult result) {
  switch (result.kind) {
  case RESULT_DONE:
    return pure_vec(heap, result.value);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return budget_failed(result.error);
}

static MaybeValue value_none(void) {
  MaybeValue out;
  out.some = false;
  out.value = NULL;
  return out;
}

static MaybeValue value_some(const Value *value) {
  MaybeValue out;
  out.some = true;
  out.value = value;
  return out;
}

/* List<Maybe<Value.T>> items are boxed MaybeValue pointers. */
static MaybeValue *maybe_value_new(Heap *heap, MaybeValue value) {
  MaybeValue *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = value;
  return boxed;
}

static MaybeStr str_none(void) {
  MaybeStr out;
  out.some = false;
  out.value = str_lit("");
  return out;
}

static MaybeStr str_some(Str value) {
  MaybeStr out;
  out.some = true;
  out.value = value;
  return out;
}

static Nat nat_of_len(Heap *heap, size_t len) { return heap_nat_of_u64(heap, (uint64_t)len); }

static Comp leg_types(Heap *heap, const Vec *legs, Ctx ctx, Big count);
static Comp elab_terms(Heap *heap, const Vec *terms, Ctx ctx);
static Comp elab_typed_items(Heap *heap, const Vec *items, const Vec *expectations, Ctx ctx);

/* ---- Bend 32-56 ---- */

static Error no_expect(Heap *heap, Str what) {
  Str parts[2];
  parts[0] = what;
  parts[1] = str_lit(" needs an expected type");
  return error_make(ERROR_CANNOT_INFER, cats(heap, 2, parts));
}

static Globals globals_of(Ctx ctx) { return context_globals_of(ctx); }

static Vec env_of(Ctx ctx) { return context_env_of(ctx); }

static Nat size_of(Ctx ctx) { return context_size_of(ctx); }

static Comp eval_in(Heap *heap, Ctx ctx, const Term *term) {
  return budget_lift(eval_eval(heap, globals_of(ctx), env_of(ctx), term));
}

static Comp whnf_in(Heap *heap, Ctx ctx, const Value *value) {
  return budget_lift(eval_whnf(heap, globals_of(ctx), value));
}

static Comp quote_in(Heap *heap, Ctx ctx, const Value *value) {
  return budget_lift(eval_quote(heap, globals_of(ctx), size_of(ctx), value));
}

static Comp univ_of_int_after_level(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(term_univ(heap, value));
}

static Comp univ_of_int(Heap *heap, Big level) {
  const Universe *nat_level = universe_nat_level(heap, level);
  if (nat_level == NULL) return comp_oom();
  return bind_with(heap, budget_lift(universe_lower(heap, nat_level)), univ_of_int_after_level, NULL);
}

/* ---- Bend 58-107: shape and syntax views ---- */

static MaybeElabPoint point_none(void) {
  MaybeElabPoint out;
  out.some = false;
  out.value.quantity = QUANTITY_ZERO;
  out.value.name = str_lit("");
  out.value.domain = NULL;
  return out;
}

static MaybeElabPoint point_some(Shape shape) {
  MaybeElabPoint out;
  out.some = true;
  out.value.quantity = shape.quantity;
  out.value.name = shape.name;
  out.value.domain = shape.domain;
  return out;
}

static MaybeElabPoint point_of(Shape shape) {
  switch (shape.kind) {
  case SHAPE_PI:
  case SHAPE_ZK:
    return point_some(shape);
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return point_none();
  }
  return point_none();
}

static MaybeElabPoint zk_point(Shape shape) {
  switch (shape.kind) {
  case SHAPE_ZK:
    return point_some(shape);
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return point_none();
  }
  return point_none();
}

static MaybeValue fhc_level(Shape shape) {
  switch (shape.kind) {
  case SHAPE_FHC:
    return value_some(shape.level);
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_MPC:
    return value_none();
  }
  return value_none();
}

static ElabSpine spine_of(Heap *heap, const STerm *term, Vec args) {
  ElabSpine out;
  if (term != NULL && term->kind == STERM_APP) {
    Vec more = cons(heap, (void *)term->arg, &args);
    return spine_of(heap, term->head, more);
  }
  out.head = term;
  out.args = args;
  return out;
}

/* ---- Bend 109-180: constructor and family lookup ---- */

static Str family_name(Family family) { return family.name; }

static MaybeElabFamilyCtor family_ctor_none(void) {
  MaybeElabFamilyCtor out = {.some = false};
  return out;
}

static MaybeElabFamilyCtor family_ctor_some(Family family, Ctor ctor) {
  MaybeElabFamilyCtor out;
  out.some = true;
  out.value.family = family;
  out.value.ctor = ctor;
  return out;
}

static MaybeElabFamilyCtor select_ctor(MaybeElabFamilyCtor candidate, MaybeElabFamilyCtor selected) {
  bool earlier;
  if (!candidate.some) return selected;
  if (!selected.some) return candidate;
  earlier = str_cmp(family_name(candidate.value.family), family_name(selected.value.family)) < 0;
  return earlier ? candidate : selected;
}

static MaybeElabFamilyCtor ctor_candidate(MaybeCtor ctor, Family family) {
  if (!ctor.some) return family_ctor_none();
  return family_ctor_some(family, ctor.value);
}

static MaybeElabFamilyCtor scan_ctors(const FamilyRows *families, Str name) {
  if (families == NULL) return family_ctor_none();
  return select_ctor(ctor_candidate(positivity_ctor_of(name, families->family), families->family),
                     scan_ctors(families->rest, name));
}

static MaybeElabFamilyCtor find_ctor(Str name, Globals globals) { return scan_ctors(globals.families, name); }

static MaybeElabMuRef mu_ref_none(void) {
  MaybeElabMuRef out = {.some = false};
  return out;
}

static MaybeElabMuRef ctor_ref(MaybeElabFamilyCtor ctor) {
  MaybeElabMuRef out;
  if (!ctor.some) return mu_ref_none();
  out.some = true;
  out.value.kind = ELAB_RCTOR;
  out.value.family = ctor.value.family;
  out.value.ctor = ctor.value.ctor;
  return out;
}

static MaybeElabMuRef family_ref(MaybeFamily family, Str name, Globals globals) {
  MaybeElabMuRef out = {.some = false};
  if (!family.some) return ctor_ref(find_ctor(name, globals));
  out.some = true;
  out.value.kind = ELAB_RFAM;
  out.value.family = family.value;
  return out;
}

static MaybeElabMuRef local_ref(MaybeNat local, Str name, Globals globals) {
  if (local.some) return mu_ref_none();
  return family_ref(global_find_family(name, globals), name, globals);
}

static MaybeElabMuRef mu_ref_of(Heap *heap, Ctx ctx, Str name) {
  return local_ref(check_index_of_name(heap, ctx, name), name, globals_of(ctx));
}

static MaybeStr expected_family_result(Result result) {
  const Value *value;
  switch (result.kind) {
  case RESULT_DONE:
    value = result.value;
    if (value != NULL && value->kind == VALUE_LAN && value->shape.kind == SHAPE_MU) return str_some(value->shape.name);
    return str_none();
  case RESULT_FAIL:
    return str_none();
  }
  return str_none();
}

static MaybeStr expected_family(Heap *heap, Ctx ctx, MaybeValue expected) {
  if (!expected.some) return str_none();
  return expected_family_result(eval_whnf(heap, globals_of(ctx), expected.value));
}

/* ---- Bend 182-203: arity and collection formers ---- */

static Comp arity_is(Heap *heap, Str what, Nat want, Nat got) {
  Str parts[5];
  if (nat_cmp(want, got) == 0) return budget_pure(kernel_unit());
  parts[0] = what;
  parts[1] = str_lit(" takes ");
  parts[2] = heap_nat_show(heap, want);
  parts[3] = str_lit(" arguments and the term gives ");
  parts[4] = heap_nat_show(heap, got);
  return budget_failed(error_make(ERROR_MISMATCH, cats(heap, 5, parts)));
}

/* legs: List<Term.Leg<Term.T>> as Leg pointers, each with no binders. */
static Vec legs_of(Heap *heap, const Vec *terms) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    Leg *leg = heap_alloc(heap, sizeof *leg);
    if (got.status != VEC_GET_OK || leg == NULL) return out;
    *leg = term_leg(empty(heap), got.value);
    push(heap, &out, leg);
  }
  return out;
}

static Shape coll_of_len(Heap *heap, size_t len) { return shape_scoll(big_of_nat(heap, nat_of_len(heap, len))); }

static const Term *diagram_of(Heap *heap, const Vec *terms) {
  return term_sec(heap, coll_of_len(heap, terms->len), legs_of(heap, terms));
}

static const Term *sum_ty(Heap *heap, const Vec *terms) {
  return term_lan(heap, coll_of_len(heap, terms->len), diagram_of(heap, terms));
}

static const Term *prod_ty(Heap *heap, const Vec *terms) {
  return term_ran(heap, coll_of_len(heap, terms->len), diagram_of(heap, terms));
}

static const Term *unit_val(Heap *heap) { return term_sec(heap, shape_scoll(big_zero()), empty(heap)); }

/* ---- Bend 217-291: require, pairs, Former, Point and Bound views ---- */

static Comp require_value(MaybeValue value, Error error) {
  if (value.some) return budget_pure((void *)value.value);
  return budget_failed(error);
}

static Comp require_point(Heap *heap, MaybeElabPoint point, Error error) {
  ElabPoint *boxed;
  if (!point.some) return budget_failed(error);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = point.value;
  return budget_pure(boxed);
}

static Comp former_box(Heap *heap, const Value *value) {
  ElabFormer *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->shape = value->shape;
  boxed->diagram = value->diagram;
  boxed->level = value->former_level;
  return budget_pure(boxed);
}

static bool is_former(bool left, const Value *value) {
  bool lan = left && value->kind == VALUE_LAN;
  bool ran = !left && value->kind == VALUE_RAN;
  return lan || ran;
}

static Comp former(Heap *heap, bool left, const Value *value, Str message) {
  if (value != NULL && is_former(left, value)) return former_box(heap, value);
  return budget_failed(error_make(ERROR_MISMATCH, message));
}

static Shape former_shape(ElabFormer former_value) { return former_value.shape; }

static Closure former_diagram(ElabFormer former_value) { return former_value.diagram; }

static Quantity point_q(ElabPoint point) { return point.quantity; }

static Str point_name(ElabPoint point) { return point.name; }

static const Value *point_domain(ElabPoint point) { return point.domain; }

static Quantity bound_q(ElabBound bound) { return bound.quantity; }

static Str bound_name(ElabBound bound) { return bound.name; }

static const Term *bound_type(ElabBound bound) { return bound.ty; }

static Ctx bound_context(ElabBound bound) { return bound.context; }

static const Value *inferred_type(Inferred value) { return value.ty; }

/* ---- Bend 293-307: inference helpers ---- */

static Comp infer_type_after_inferred(Heap *heap, void *ctx, void *value) {
  Inferred *inferred = value;
  (void)heap;
  (void)ctx;
  return pure_value(inferred_type(*inferred));
}

static Comp infer_type(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  return bind_with(heap, check_engine_infer_uses(heap, ctx, quantity_runtime(mode), term), infer_type_after_inferred,
                   NULL);
}

static Comp type_of_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return whnf_in(heap, f->ctx, value);
}

static Comp type_of(Heap *heap, Ctx ctx, const Term *term) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, infer_type(heap, ctx, QUANTITY_ZERO, term), type_of_after_ty, f);
}

static Comp expected_former_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return former(heap, f->left, value, f->message);
}

static Comp expected_former_after_require(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, whnf_in(heap, f->ctx, value), expected_former_after_whnf, f);
}

static Comp expected_former(Heap *heap, Ctx ctx, MaybeValue expected, bool left, Str word, Str message) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->left = left;
  f->message = message;
  return bind_frame(heap, require_value(expected, no_expect(heap, word)), expected_former_after_require, f);
}

/* ---- Bend 309-389: leg expectations ---- */

static Vec none_expectations(Heap *heap, Nat count) {
  Vec out = empty(heap);
  NatToU64 n = nat_to_u64(count);
  uint64_t i;
  if (n.status != NAT_TO_U64_OK) {
    heap->oom = true;
    return out;
  }
  for (i = 0; i < n.value; i++) {
    MaybeValue *item = maybe_value_new(heap, value_none());
    if (item == NULL) return out;
    push(heap, &out, item);
  }
  return out;
}

static Comp leg_types_after_tail(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec *tail = value;
  MaybeValue *item = maybe_value_new(heap, value_some(f->value));
  if (item == NULL) return comp_oom();
  return pure_vec(heap, cons(heap, item, tail));
}

static Comp leg_types_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->value = value;
  return bind_frame(heap, leg_types(heap, &f->rest, f->ctx, big_sub(heap, f->width, big_one())), leg_types_after_tail,
                    f);
}

static Comp leg_types(Heap *heap, const Vec *legs, Ctx ctx, Big count) {
  bool done = big_equal(count, big_zero());
  VecGet got;
  VLeg *leg;
  ElabFrame *f;
  if (legs->len == 0) {
    if (done) return pure_vec(heap, empty(heap));
    return budget_failed(error_make(ERROR_WRONG_LEG, str_lit("the leg address is outside the collection")));
  }
  if (done) return pure_vec(heap, empty(heap));
  got = vec_get(legs, 0);
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  leg = got.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->width = count;
  f->rest = list_tail(*legs);
  return bind_frame(heap, budget_lift(eval_open_closure(heap, globals_of(ctx), leg->clo, empty(heap))),
                    leg_types_after_ty, f);
}

static Comp requested_leg_types_after_legs(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec *legs = value;
  return leg_types(heap, legs, f->ctx, f->width);
}

static Comp requested_leg_types_nonnegative(Heap *heap, Ctx ctx, Closure diagram, Big count) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->width = count;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, ctx, diagram)), requested_leg_types_after_legs, f);
}

static Comp requested_leg_types(Heap *heap, Ctx ctx, Closure diagram, Big count) {
  if (big_le(big_zero(), count)) return requested_leg_types_nonnegative(heap, ctx, diagram, count);
  return mismatch("a collection width must be nonnegative");
}

static Comp matching_leg_types(Heap *heap, bool same, Ctx ctx, Closure diagram, Nat count) {
  if (!same) return pure_vec(heap, none_expectations(heap, count));
  return requested_leg_types(heap, ctx, diagram, big_of_nat(heap, count));
}

static bool is_coll_former(bool left, const Value *value) {
  return is_former(left, value) && value->shape.kind == SHAPE_COLL;
}

static Comp expected_leg_types(Heap *heap, bool left, const Value *value, Ctx ctx, Nat count) {
  if (value != NULL && is_coll_former(left, value)) {
    bool same = big_equal(value->shape.count, big_of_nat(heap, count));
    return matching_leg_types(heap, same, ctx, value->diagram, count);
  }
  return pure_vec(heap, none_expectations(heap, count));
}

static Comp leg_expectations_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return expected_leg_types(heap, f->left, value, f->ctx, f->count);
}

static Comp leg_expectations(Heap *heap, Ctx ctx, MaybeValue expected, Nat count, bool left) {
  ElabFrame *f;
  if (!expected.some) return pure_vec(heap, none_expectations(heap, count));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->count = count;
  f->left = left;
  return bind_frame(heap, whnf_in(heap, ctx, expected.value), leg_expectations_after_whnf, f);
}

/* An absent expected leg is represented sparsely for injections. This avoids
 * allocating a list proportional to an untrusted arbitrary-precision width. */
static Comp injection_leg_types(Heap *heap, const Value *value, Ctx ctx, Big count) {
  if (value != NULL && is_coll_former(true, value)) {
    if (big_equal(value->shape.count, count)) return requested_leg_types(heap, ctx, value->diagram, count);
    return pure_vec(heap, empty(heap));
  }
  return pure_vec(heap, empty(heap));
}

static Comp injection_expectations_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return injection_leg_types(heap, value, f->ctx, f->width);
}

static Comp injection_expectations_nonnegative(Heap *heap, Ctx ctx, MaybeValue expected, Big count) {
  ElabFrame *f;
  if (!expected.some) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->width = count;
  return bind_frame(heap, whnf_in(heap, ctx, expected.value), injection_expectations_after_whnf, f);
}

Comp elab_term_injection_expectations(Heap *heap, Ctx ctx, MaybeValue expected, Big count) {
  if (big_le(big_zero(), count)) return injection_expectations_nonnegative(heap, ctx, expected, count);
  return mismatch("a collection width must be nonnegative");
}

/* ---- Bend 391-432: item lists and pairs ---- */

static Comp elab_terms_after_tail(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec *tail = value;
  return pure_vec(heap, cons(heap, (void *)f->term, tail));
}

static Comp elab_terms_after_head(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, elab_terms(heap, &f->rest, f->ctx), elab_terms_after_tail, f);
}

static Comp elab_terms(Heap *heap, const Vec *terms, Ctx ctx) {
  VecGet got;
  ElabFrame *f;
  if (terms->len == 0) return pure_vec(heap, empty(heap));
  got = vec_get(terms, 0);
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->rest = list_tail(*terms);
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), got.value), elab_terms_after_head, f);
}

static Comp elab_typed_items_after_tail(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec *tail = value;
  return pure_vec(heap, cons(heap, (void *)f->term, tail));
}

static Comp elab_typed_items_after_head(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, elab_typed_items(heap, &f->items, &f->rest, f->ctx), elab_typed_items_after_tail, f);
}

static Comp elab_typed_items(Heap *heap, const Vec *items, const Vec *expectations, Ctx ctx) {
  VecGet item;
  VecGet expected;
  MaybeValue *slot;
  ElabFrame *f;
  if (items->len == 0 && expectations->len == 0) return pure_vec(heap, empty(heap));
  if (items->len == 0 || expectations->len == 0) return mismatch("the item count and the leg count differ");
  item = vec_get(items, 0);
  expected = vec_get(expectations, 0);
  if (item.status != VEC_GET_OK || expected.status != VEC_GET_OK) {
    return mismatch("the item count and the leg count differ");
  }
  slot = expected.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->items = list_tail(*items);
  f->rest = list_tail(*expectations);
  return bind_frame(heap, elab_term_elab(heap, ctx, *slot, item.value), elab_typed_items_after_head, f);
}

static Comp elab_items_after_expectations(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec *expectations = value;
  return elab_typed_items(heap, &f->items, expectations, f->ctx);
}

static Comp elab_items(Heap *heap, Ctx ctx, MaybeValue expected, const Vec *items, bool left) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->items = *items;
  return bind_frame(heap, leg_expectations(heap, ctx, expected, nat_of_len(heap, items->len), left),
                    elab_items_after_expectations, f);
}

static Comp elab_pair_after_domain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Term *domain = value;
  Shape shape = shape_spi(point_q(f->point), point_name(f->point), (void *)domain);
  Addr addr = term_addr_pt(point_q(f->point), f->left_term);
  return pure_term(term_in(heap, shape, addr, singleton(heap, (void *)f->term)));
}

static Comp elab_pair_after_right(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, quote_in(heap, f->ctx, point_domain(f->point)), elab_pair_after_domain, f);
}

static Comp elab_pair_after_codomain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(value), f->right_syntax), elab_pair_after_right, f);
}

static Comp elab_pair_after_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec args = singleton(heap, value);
  return bind_frame(heap, budget_lift(eval_open_closure(heap, globals_of(f->ctx), former_diagram(f->former), args)),
                    elab_pair_after_codomain, f);
}

static Comp elab_pair_after_left(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->left_term = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->left_term), elab_pair_after_value, f);
}

static Comp elab_pair_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  ElabPoint *point = value;
  f->point = *point;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(point_domain(f->point)), f->left_syntax),
                    elab_pair_after_left, f);
}

static Comp elab_pair_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  ElabFormer *former_value = value;
  Error error = error_make(ERROR_MISMATCH, str_lit("a pair needs a point former"));
  f->former = *former_value;
  return bind_frame(heap, require_point(heap, point_of(former_shape(f->former)), error), elab_pair_after_point, f);
}

static Comp elab_pair(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *left, const STerm *right) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->left_syntax = left;
  f->right_syntax = right;
  return bind_frame(heap,
                    expected_former(heap, ctx, expected, true, str_lit("a pair"),
                                    str_lit("a pair needs a left former as its expected type")),
                    elab_pair_after_former, f);
}

/* ---- Bend 433-489: binders, functions and groups ---- */

static Comp elab_fun(Heap *heap, const Vec *binders, Ctx ctx, MaybeValue expected, const STerm *body);

static Comp pure_nat(Heap *heap, Nat value) {
  Nat *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

/* Comp(Maybe<Value.T>) values are boxed MaybeValue pointers. */
static Comp pure_maybe_value(Heap *heap, MaybeValue value) {
  MaybeValue *boxed = maybe_value_new(heap, value);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_value_pair(Heap *heap, const Value *first_value, const Value *second_value) {
  ValuePair *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->first = first_value;
  boxed->second = second_value;
  return budget_pure(boxed);
}

static Comp lift_big(Heap *heap, BigResult result) {
  Big *boxed;
  switch (result.kind) {
  case RESULT_DONE:
    boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) return comp_oom();
    *boxed = result.value;
    return budget_pure(boxed);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return budget_failed(result.error);
}

static Str cat2(Heap *heap, Str a, const char *b) {
  Str parts[2];
  parts[0] = a;
  parts[1] = str_lit(b);
  return cats(heap, 2, parts);
}

static Comp elab_binder_after_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  ElabBound *bound = heap_alloc(heap, sizeof *bound);
  if (bound == NULL) return comp_oom();
  bound->quantity = f->quantity;
  bound->name = f->word;
  bound->ty = f->term;
  bound->context = check_bind(heap, f->word, f->quantity, value, f->ctx);
  return budget_pure(bound);
}

static Comp elab_binder_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->term), elab_binder_after_value, f);
}

static Comp elab_binder(Heap *heap, Ctx ctx, SBinder binder) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->quantity = binder.quantity;
  f->word = binder.name;
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), binder.ty), elab_binder_after_ty, f);
}

static Comp codomain_value_after_open(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_maybe_value(heap, value_some(value));
}

static Comp codomain_value(Heap *heap, const Value *value, Ctx ctx) {
  bool point = value != NULL && value->kind == VALUE_RAN &&
               (value->shape.kind == SHAPE_PI || value->shape.kind == SHAPE_ZK);
  const Value *var;
  if (!point) return pure_maybe_value(heap, value_none());
  var = value_var(heap, size_of(ctx));
  if (var == NULL) return comp_oom();
  return bind_with(heap,
                   budget_lift(eval_open_closure(heap, globals_of(ctx), value->diagram, singleton(heap, (void *)var))),
                   codomain_value_after_open, NULL);
}

static Comp cod_of_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return codomain_value(heap, value, f->ctx);
}

static Comp cod_of(Heap *heap, Ctx ctx, MaybeValue expected) {
  ElabFrame *f;
  if (!expected.some) return pure_maybe_value(heap, value_none());
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, whnf_in(heap, ctx, expected.value), cod_of_after_whnf, f);
}

static Comp elab_fun_after_body(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Binder *binder = term_binder_new(heap, bound_q(f->bound), bound_name(f->bound));
  Leg *leg;
  if (binder == NULL) return comp_oom();
  leg = term_leg_new(heap, singleton(heap, binder), value);
  if (leg == NULL) return comp_oom();
  return pure_term(term_sec(heap, shape_spi(bound_q(f->bound), bound_name(f->bound), (void *)bound_type(f->bound)),
                            singleton(heap, leg)));
}

static Comp elab_fun_after_cod(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->cod = *(MaybeValue *)value;
  return bind_frame(heap, elab_fun(heap, &f->rest, bound_context(f->bound), f->cod, f->body_syntax),
                    elab_fun_after_body, f);
}

static Comp elab_fun_after_bound(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->bound = *(ElabBound *)value;
  return bind_frame(heap, cod_of(heap, f->ctx, f->expected), elab_fun_after_cod, f);
}

static Comp elab_fun(Heap *heap, const Vec *binders, Ctx ctx, MaybeValue expected, const STerm *body) {
  ElabFrame *f;
  VecGet head;
  if (binders->len == 0) return elab_term_elab(heap, ctx, expected, body);
  head = vec_get(binders, 0);
  if (head.status != VEC_GET_OK || head.value == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected = expected;
  f->body_syntax = body;
  f->rest = list_tail(*binders);
  return bind_frame(heap, elab_binder(heap, ctx, *(const SBinder *)head.value), elab_fun_after_bound, f);
}

static const Term *group_term(Heap *heap, uint32_t kind, ElabBound bound, const Term *body) {
  Shape pi = shape_spi(bound.quantity, bound.name, (void *)bound.ty);
  if (kind == 0) return term_ran(heap, pi, body);
  if (kind == 1) return term_lan(heap, pi, body);
  return term_lan(heap, shape_szk(bound.quantity, bound.name, (void *)bound.ty), body);
}

static Comp elab_group_after_body(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return pure_term(group_term(heap, f->group_kind, f->bound, value));
}

static Comp elab_group_after_bound(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->bound = *(ElabBound *)value;
  return bind_frame(heap, elab_term_elab(heap, bound_context(f->bound), value_none(), f->body_syntax),
                    elab_group_after_body, f);
}

static Comp elab_group(Heap *heap, Ctx ctx, SBinder binder, const STerm *body, uint32_t kind) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->body_syntax = body;
  f->group_kind = kind;
  return bind_frame(heap, elab_binder(heap, ctx, binder), elab_group_after_bound, f);
}

/* ---- Bend 491-527: application, ciphertexts ---- */

static Comp elab_app_point_after_arg(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return pure_term(term_out(heap, shape_sarg(point_q(f->point), point_name(f->point)),
                            term_addr_pt(point_q(f->point), value), f->head_term));
}

static Comp elab_app_point_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->point = *(ElabPoint *)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(point_domain(f->point)), f->syntax),
                    elab_app_point_after_arg, f);
}

static Comp elab_app_point_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap,
                    require_point(heap, point_of(former_shape(f->former)), error_make(ERROR_MISMATCH, f->message)),
                    elab_app_point_after_point, f);
}

static Comp elab_app_point_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, former(heap, false, value, f->message), elab_app_point_after_former, f);
}

static Comp elab_app_point_after_head(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->head_term = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->head_term), elab_app_point_after_ty, f);
}

static Comp elab_app_point(Heap *heap, Ctx ctx, const STerm *head, const STerm *arg) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->syntax = arg;
  f->message = str_lit("the head of an application is not a function");
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), head), elab_app_point_after_head, f);
}

static Comp fhc_expect_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return pure_value_pair(heap, f->level_value, value);
}

static Comp fhc_expect_after_level(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->level_value = value;
  return bind_frame(heap,
                    budget_lift(eval_open_closure(heap, globals_of(f->ctx), former_diagram(f->former), empty(heap))),
                    fhc_expect_after_ty, f);
}

static Comp fhc_expect_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap,
                    require_value(fhc_level(former_shape(f->former)),
                                  error_make(ERROR_MISMATCH, cat2(heap, f->word, " needs a ciphertext former"))),
                    fhc_expect_after_level, f);
}

static Comp fhc_expect(Heap *heap, Ctx ctx, MaybeValue expected, Str word) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->word = word;
  return bind_frame(heap,
                    expected_former(heap, ctx, expected, false, word,
                                    cat2(heap, word, " needs a ciphertext type as its expected type")),
                    fhc_expect_after_former, f);
}

static Comp elab_cipher_after_level(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec two = singleton(heap, (void *)f->left_term);
  push(heap, &two, (void *)f->term);
  return pure_term(term_sec(heap, shape_sfhc(value), legs_of(heap, &two)));
}

static Comp elab_cipher_after_right(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, quote_in(heap, f->ctx, f->pair.first), elab_cipher_after_level, f);
}

static Comp elab_cipher_after_left(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->left_term = value;
  return bind_frame(heap,
                    elab_term_elab(heap, f->ctx, f->evaluate ? value_none() : value_some(f->pair.second),
                                   f->right_syntax),
                    elab_cipher_after_right, f);
}

static Comp elab_cipher_after_cipher(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->pair = *(ValuePair *)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->left_syntax), elab_cipher_after_left, f);
}

static Comp elab_cipher(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *left, const STerm *right,
                        bool evaluate) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->left_syntax = left;
  f->right_syntax = right;
  f->evaluate = evaluate;
  return bind_frame(heap, fhc_expect(heap, ctx, expected, str_lit(evaluate ? "eval" : "enc")),
                    elab_cipher_after_cipher, f);
}

static Comp elab_dec_after_level_term(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return pure_term(term_out(heap, shape_sfhc(value), term_addr_pt(QUANTITY_ONE, f->key), f->cipher));
}

static Comp elab_dec_after_level(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, quote_in(heap, f->ctx, value), elab_dec_after_level_term, f);
}

static Comp elab_dec_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap,
                    require_value(fhc_level(former_shape(f->former)),
                                  error_make(ERROR_MISMATCH, str_lit("dec needs a ciphertext former"))),
                    elab_dec_after_level, f);
}

static Comp elab_dec_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, former(heap, false, value, str_lit("dec needs a ciphertext as its argument")),
                    elab_dec_after_former, f);
}

static Comp elab_dec_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, whnf_in(heap, f->ctx, value), elab_dec_after_whnf, f);
}

static Comp elab_dec_after_cipher(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->cipher = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->cipher), elab_dec_after_ty, f);
}

static Comp elab_dec_after_key(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->key = value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->right_syntax), elab_dec_after_cipher, f);
}

static Comp elab_dec(Heap *heap, Ctx ctx, const STerm *key, const STerm *cipher) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->right_syntax = cipher;
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), key), elab_dec_after_key, f);
}

/* ---- Bend 529-593: shares ---- */

static Comp mpc_shape(Heap *heap, Shape shape, Str message) {
  if (shape.kind == SHAPE_MPC) return pure_value_pair(heap, shape.parties, shape.access);
  return budget_failed(error_make(ERROR_MISMATCH, message));
}

static const Term *mpc_parties(MpcExpected expected) { return expected.parties; }

static const Term *mpc_access(MpcExpected expected) { return expected.access; }

static const Value *mpc_value(MpcExpected expected) { return expected.parties_value; }

static const Value *mpc_type(MpcExpected expected) { return expected.ty; }

static Comp mpc_expect_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  MpcExpected *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->parties = f->parties_term;
  boxed->access = f->term;
  boxed->parties_value = f->pair.first;
  boxed->ty = value;
  return budget_pure(boxed);
}

static Comp mpc_expect_after_access(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap,
                    budget_lift(eval_open_closure(heap, globals_of(f->ctx), former_diagram(f->former), empty(heap))),
                    mpc_expect_after_ty, f);
}

static Comp mpc_expect_after_parties(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->parties_term = value;
  return bind_frame(heap, quote_in(heap, f->ctx, f->pair.second), mpc_expect_after_access, f);
}

static Comp mpc_expect_after_mpc(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->pair = *(ValuePair *)value;
  return bind_frame(heap, quote_in(heap, f->ctx, f->pair.first), mpc_expect_after_parties, f);
}

static Comp mpc_expect_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap, mpc_shape(heap, former_shape(f->former), cat2(heap, f->word, " needs a share former")),
                    mpc_expect_after_mpc, f);
}

static Comp mpc_expect(Heap *heap, Ctx ctx, MaybeValue expected, Str word) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->word = word;
  return bind_frame(heap,
                    expected_former(heap, ctx, expected, false, word,
                                    cat2(heap, word, " needs a share type as its expected type")),
                    mpc_expect_after_former, f);
}

static const Term *mpc_sec(Heap *heap, MpcExpected mpc, const Vec *terms) {
  return term_sec(heap, shape_smpc((void *)mpc_parties(mpc), (void *)mpc_access(mpc)), legs_of(heap, terms));
}

static Comp elab_share_after_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec one = singleton(heap, value);
  return pure_term(mpc_sec(heap, f->mpc, &one));
}

static Comp elab_share_after_mpc(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->mpc = *(MpcExpected *)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(mpc_type(f->mpc)), f->value_syntax),
                    elab_share_after_value, f);
}

static Comp elab_share(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *value) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value_syntax = value;
  return bind_frame(heap, mpc_expect(heap, ctx, expected, str_lit("share")), elab_share_after_mpc, f);
}

static Comp elab_input_after_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec two = singleton(heap, (void *)f->parties_term);
  push(heap, &two, value);
  return pure_term(mpc_sec(heap, f->mpc, &two));
}

static Comp elab_input_after_party(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->parties_term = value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(mpc_type(f->mpc)), f->value_syntax),
                    elab_input_after_value, f);
}

static Comp elab_input_after_mpc(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->mpc = *(MpcExpected *)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(mpc_value(f->mpc)), f->party_syntax),
                    elab_input_after_party, f);
}

static Comp elab_input(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *party, const STerm *value) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->party_syntax = party;
  f->value_syntax = value;
  return bind_frame(heap, mpc_expect(heap, ctx, expected, str_lit("input")), elab_input_after_mpc, f);
}

static Comp elab_join_after_shares(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *shares = value;
  Vec rest = cons(heap, (void *)f->function_term, shares);
  Vec all = cons(heap, (void *)f->parties_term, &rest);
  return pure_term(mpc_sec(heap, f->mpc, &all));
}

static Comp elab_join_after_function(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->function_term = value;
  return bind_frame(heap, elab_terms(heap, &f->shares, f->ctx), elab_join_after_shares, f);
}

static Comp elab_join_after_parties(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->parties_term = value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->function_syntax), elab_join_after_function, f);
}

static Comp elab_join_after_subset_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(value), f->parties_syntax), elab_join_after_parties, f);
}

static Comp elab_join_after_subset(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->subset = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->subset), elab_join_after_subset_value, f);
}

static Comp elab_join_after_count(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->count_big = *(Big *)value;
  return bind_frame(heap, budget_lift(rules_mpc_sub_ty(heap, f->count_big)), elab_join_after_subset, f);
}

static Comp elab_join_after_mpc(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->mpc = *(MpcExpected *)value;
  return bind_frame(heap, lift_big(heap, rules_mpc_parties(heap, f->ctx, mpc_value(f->mpc))), elab_join_after_count, f);
}

static Comp elab_join(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *parties, const STerm *function,
                      const Vec *shares) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->parties_syntax = parties;
  f->function_syntax = function;
  f->shares = *shares;
  return bind_frame(heap, mpc_expect(heap, ctx, expected, str_lit("mpc")), elab_join_after_mpc, f);
}

/* ---- Bend 595-649: open, prove, verify ---- */

static Quantity proof_quantity(MaybeQuantity quantity) {
  if (quantity.some) return quantity.value;
  return QUANTITY_ZERO;
}

static Comp elab_open_after_access(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Shape subset_pi = shape_spi(QUANTITY_ZERO, str_lit("q"), (void *)f->subset);
  const Term *inner = term_in(heap, subset_pi, term_addr_pt(QUANTITY_ZERO, f->parties_term),
                              singleton(heap, (void *)f->proof));
  if (inner == NULL) return comp_oom();
  return pure_term(term_out(heap, shape_smpc((void *)f->term, value), term_addr_pt(f->quantity, inner), f->scrut));
}

static Comp elab_open_after_parties_type(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, quote_in(heap, f->ctx, f->pair.second), elab_open_after_access, f);
}

static Comp elab_open_after_proof(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->proof = value;
  return bind_frame(heap, quote_in(heap, f->ctx, f->pair.first), elab_open_after_parties_type, f);
}

static Comp elab_open_after_parties(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->parties_term = value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->proof_syntax), elab_open_after_proof, f);
}

static Comp elab_open_after_subset_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(value), f->parties_syntax), elab_open_after_parties, f);
}

static Comp elab_open_after_subset(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->subset = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->subset), elab_open_after_subset_value, f);
}

static Comp elab_open_after_count(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->count_big = *(Big *)value;
  return bind_frame(heap, budget_lift(rules_mpc_sub_ty(heap, f->count_big)), elab_open_after_subset, f);
}

static Comp elab_open_after_mpc(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->pair = *(ValuePair *)value;
  return bind_frame(heap, lift_big(heap, rules_mpc_parties(heap, f->ctx, f->pair.first)), elab_open_after_count, f);
}

static Comp elab_open_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap, mpc_shape(heap, former_shape(f->former), str_lit("open needs a share former")),
                    elab_open_after_mpc, f);
}

static Comp elab_open_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, former(heap, false, value, str_lit("open needs a share as its argument")),
                    elab_open_after_former, f);
}

static Comp elab_open_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, whnf_in(heap, f->ctx, value), elab_open_after_whnf, f);
}

static Comp elab_open_after_share(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->scrut = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->scrut), elab_open_after_ty, f);
}

static Comp elab_open(Heap *heap, Ctx ctx, MaybeQuantity quantity, const STerm *parties, const STerm *proof,
                      const STerm *share) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->quantity = proof_quantity(quantity);
  f->parties_syntax = parties;
  f->proof_syntax = proof;
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), share), elab_open_after_share, f);
}

static Comp elab_prove_after_domain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Vec args = singleton(heap, (void *)f->relation);
  push(heap, &args, (void *)f->instance);
  return pure_term(term_in(heap, shape_szk(point_q(f->point), point_name(f->point), value),
                           term_addr_pt(point_q(f->point), f->witness), args));
}

static Comp elab_prove_after_relation(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->relation = value;
  return bind_frame(heap, quote_in(heap, f->ctx, point_domain(f->point)), elab_prove_after_domain, f);
}

static Comp elab_prove_after_relation_type(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(value), f->relation_syntax),
                    elab_prove_after_relation, f);
}

static Comp elab_prove_after_witness_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap,
                    budget_lift(eval_open_closure(heap, globals_of(f->ctx), former_diagram(f->former),
                                                  singleton(heap, value))),
                    elab_prove_after_relation_type, f);
}

static Comp elab_prove_after_witness(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->witness = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->witness), elab_prove_after_witness_value, f);
}

static Comp elab_prove_after_instance_type(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  (void)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(point_domain(f->point)), f->witness_syntax),
                    elab_prove_after_witness, f);
}

static Comp elab_prove_after_instance(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->instance = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->instance), elab_prove_after_instance_type, f);
}

static Comp elab_prove_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->point = *(ElabPoint *)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->syntax), elab_prove_after_instance, f);
}

static Comp elab_prove_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap,
                    require_point(heap, zk_point(former_shape(f->former)),
                                  error_make(ERROR_MISMATCH, str_lit("a proof needs a zk former"))),
                    elab_prove_after_point, f);
}

static Comp elab_prove(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *instance, const STerm *witness,
                       const STerm *relation) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->syntax = instance;
  f->witness_syntax = witness;
  f->relation_syntax = relation;
  return bind_frame(heap,
                    expected_former(heap, ctx, expected, true, str_lit("a proof"),
                                    str_lit("a proof needs a zk type as its expected type")),
                    elab_prove_after_former, f);
}

/* One branch [Pair{aleg 0, Leg{binders, body}}]. */
static Vec one_branch(Heap *heap, Vec binders, const Term *body) {
  Branch *branch = term_branch_new(heap, term_aleg(heap, heap_nat_of_u64(heap, 0)), term_leg(binders, body));
  if (branch == NULL) return empty(heap);
  return singleton(heap, branch);
}

static Vec two_binders(Heap *heap, Quantity q1, Str n1, Quantity q2, Str n2) {
  Vec binders = empty(heap);
  Binder *first_binder = term_binder_new(heap, q1, n1);
  Binder *second_binder = term_binder_new(heap, q2, n2);
  if (first_binder == NULL || second_binder == NULL) return binders;
  push(heap, &binders, first_binder);
  push(heap, &binders, second_binder);
  return binders;
}

static Comp elab_verify_after_relation(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Quantity q = point_q(f->point);
  Str name = point_name(f->point);
  const Term *univ = term_univ(heap, level_zero());
  MaybeMotive motive = term_motive_some(term_motive(str_none(), empty(heap), str_lit("p"), univ));
  Vec branches = one_branch(heap, two_binders(heap, q, name, QUANTITY_ZERO, str_lit("r")), value);
  if (univ == NULL) return comp_oom();
  return pure_term(term_elim(heap, term_elim_data(shape_szk(q, name, (void *)f->term), f->proof, QUANTITY_ONE,
                                                  motive, branches)));
}

static Comp elab_verify_after_relation_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Nat size = size_of(f->ctx);
  Nat two_more = heap_nat_succ(heap, heap_nat_succ(heap, size));
  return bind_frame(heap, budget_lift(eval_quote(heap, globals_of(f->ctx), two_more, value)),
                    elab_verify_after_relation, f);
}

static Comp elab_verify_after_domain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Value *var = value_var(heap, size_of(f->ctx));
  f->term = value;
  if (var == NULL) return comp_oom();
  return bind_frame(heap,
                    budget_lift(eval_open_closure(heap, globals_of(f->ctx), former_diagram(f->former),
                                                  singleton(heap, (void *)var))),
                    elab_verify_after_relation_value, f);
}

static Comp elab_verify_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->point = *(ElabPoint *)value;
  return bind_frame(heap, quote_in(heap, f->ctx, point_domain(f->point)), elab_verify_after_domain, f);
}

static Comp elab_verify_after_former(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->former = *(ElabFormer *)value;
  return bind_frame(heap,
                    require_point(heap, zk_point(former_shape(f->former)),
                                  error_make(ERROR_MISMATCH, str_lit("verify needs a zk former"))),
                    elab_verify_after_point, f);
}

static Comp elab_verify_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, former(heap, true, value, str_lit("verify needs a zk type as the type of its proof")),
                    elab_verify_after_former, f);
}

static Comp elab_verify_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, whnf_in(heap, f->ctx, value), elab_verify_after_whnf, f);
}

static Comp elab_verify_after_proof(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->proof = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->proof), elab_verify_after_ty, f);
}

static Comp elab_verify_after_instance_type(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  (void)value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_none(), f->proof_syntax), elab_verify_after_proof, f);
}

static Comp elab_verify_after_instance(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->instance = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->instance), elab_verify_after_instance_type, f);
}

static Comp elab_verify(Heap *heap, Ctx ctx, const STerm *instance, const STerm *proof) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->proof_syntax = proof;
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), instance), elab_verify_after_instance, f);
}

/* ---- Bend 652-668: projections ---- */

static Vec proj_branch(Heap *heap, Quantity quantity, Nat which) {
  const Term *var = term_var(heap, heap_nat_of_u64(heap, nat_is_zero(which) ? 1 : 0));
  if (var == NULL) return empty(heap);
  return one_branch(heap, two_binders(heap, quantity, str_lit("x"), QUANTITY_MANY, str_lit("y")), var);
}

static Comp projection_index(Heap *heap, Big index) {
  if (big_equal(index, big_one())) return pure_nat(heap, heap_nat_of_u64(heap, 0));
  if (big_equal(index, big_of_u32(heap, 2))) return pure_nat(heap, heap_nat_of_u64(heap, 1));
  return budget_failed(error_make(ERROR_WRONG_LEG, str_lit("a pair carries the projections .1 and .2 only")));
}

static Comp projection_body_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return budget_lift(eval_open_closure(heap, globals_of(f->ctx), f->diagram, singleton(heap, value)));
}

static Comp projection_body_after_domain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  MaybeMotive motive = term_motive_some(term_motive(str_none(), empty(heap), str_lit("self"), value));
  StuckElim data = value_stuck_elim(f->shape, QUANTITY_ONE, motive,
                                    proj_branch(heap, point_q(f->point), heap_nat_of_u64(heap, 0)), env_of(f->ctx));
  const Value *var = value_var(heap, size_of(f->ctx));
  if (var == NULL) return comp_oom();
  return bind_frame(heap, budget_lift(eval_elim_value(heap, globals_of(f->ctx), data, var)),
                    projection_body_after_point, f);
}

static Comp projection_body(Heap *heap, Nat which, Ctx ctx, Shape shape, ElabPoint point, Closure diagram) {
  ElabFrame *f;
  if (nat_is_zero(which)) return pure_value(point_domain(point));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = shape;
  f->point = point;
  f->diagram = diagram;
  return bind_frame(heap,
                    budget_lift(eval_quote(heap, globals_of(ctx), heap_nat_succ(heap, size_of(ctx)),
                                           point_domain(point))),
                    projection_body_after_domain, f);
}

/* ---- Bend 670-677: elab_pair_proj ---- */

static Comp elab_pair_proj_after_domain(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Quantity q = point_q(f->point);
  Str name = point_name(f->point);
  MaybeMotive motive = term_motive_some(term_motive(str_none(), empty(heap), str_lit("self"), f->body));
  return pure_term(term_elim(heap, term_elim_data(shape_spi(q, name, value), f->scrut, QUANTITY_ONE, motive,
                                                  proj_branch(heap, q, f->which))));
}

static Comp elab_pair_proj_after_body(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->body = value;
  return bind_frame(heap, quote_in(heap, f->ctx, point_domain(f->point)), elab_pair_proj_after_domain, f);
}

static Comp elab_pair_proj_after_body_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Nat one_more = heap_nat_succ(heap, size_of(f->ctx));
  return bind_frame(heap, budget_lift(eval_quote(heap, globals_of(f->ctx), one_more, value)),
                    elab_pair_proj_after_body, f);
}

static Comp elab_pair_proj_after_which(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->which = *(Nat *)value;
  return bind_frame(heap, projection_body(heap, f->which, f->ctx, f->shape, f->point, f->diagram),
                    elab_pair_proj_after_body_value, f);
}

static Comp elab_pair_proj_after_point(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->point = *(ElabPoint *)value;
  return bind_frame(heap, projection_index(heap, f->index), elab_pair_proj_after_which, f);
}

static Comp elab_pair_proj(Heap *heap, Ctx ctx, const Term *scrut, Shape shape, Closure diagram, Big index) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->scrut = scrut;
  f->shape = shape;
  f->diagram = diagram;
  f->index = index;
  return bind_frame(heap,
                    require_point(heap, point_of(shape),
                                  error_make(ERROR_MISMATCH, str_lit("a pair projection needs a point former"))),
                    elab_pair_proj_after_point, f);
}

/* ---- Bend 679-695: projection_type, elab_proj ---- */

static Comp projection_type(Heap *heap, const Value *ty, Ctx ctx, const Term *scrut, Big index) {
  switch (ty->kind) {
  case VALUE_LAN:
    return elab_pair_proj(heap, ctx, scrut, ty->shape, ty->diagram, index);
  case VALUE_RAN:
    if (ty->shape.kind == SHAPE_COLL) {
      return pure_term(term_out(heap, shape_scoll(ty->shape.count), term_addr_leg(index), scrut));
    }
    return mismatch("a leg projection needs a collection former");
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_UNIV:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    return mismatch("a projection reads a pair or a tuple, and this scrutinee is neither");
  }
  return mismatch("a projection reads a pair or a tuple, and this scrutinee is neither");
}

static Comp elab_proj_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return projection_type(heap, value, f->ctx, f->scrut, f->index);
}

static Comp elab_proj_after_scrut(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->scrut = value;
  return bind_frame(heap, type_of(heap, f->ctx, f->scrut), elab_proj_after_ty, f);
}

static Comp elab_proj(Heap *heap, Ctx ctx, const STerm *scrut, Big index) {
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->index = index;
  return bind_frame(heap, elab_term_elab(heap, ctx, value_none(), scrut), elab_proj_after_scrut, f);
}

/* ---- Bend 697-744: family and constructor accessors, ctor_in ---- */

static Vec family_params(Family family) { return family.params; }

static Vec family_indices(Family family) { return family.indices; }

static Str ctor_name(Ctor ctor) { return ctor.name; }

static Vec ctor_args(Ctor ctor) { return ctor.args; }

static Vec ctor_indices(Ctor ctor) { return ctor.res_idx; }

static ElabFamilyCtor prefer_ctor(MaybeElabFamilyCtor candidate, ElabFamilyCtor fallback) {
  if (candidate.some) return candidate.value;
  return fallback;
}

static MaybeElabFamilyCtor expected_ctor_family(MaybeFamily family, Str name) {
  if (!family.some) return family_ctor_none();
  return ctor_candidate(positivity_ctor_of(name, family.value), family.value);
}

static MaybeElabFamilyCtor expected_ctor(MaybeStr name, Ctx ctx, Str ctor) {
  if (!name.some) return family_ctor_none();
  return expected_ctor_family(global_find_family(name.value, globals_of(ctx)), ctor);
}

static ElabFamilyCtor ctor_in(Heap *heap, Ctx ctx, MaybeValue expected, Family family, Ctor ctor) {
  ElabFamilyCtor fallback;
  fallback.family = family;
  fallback.ctor = ctor;
  return prefer_ctor(expected_ctor(expected_family(heap, ctx, expected), ctx, ctor_name(ctor)), fallback);
}

/* ---- Bend 746-758: prepend_split, split_at ---- */

/* Common.Pair<List<Term.T>, List<Term.T>>: the params before, the indices after. */
typedef struct {
  Vec before;
  Vec after;
} TermSplit;

static TermSplit prepend_split(Heap *heap, const Term *head, TermSplit parts) {
  TermSplit out;
  out.before = cons(heap, (void *)head, &parts.before);
  out.after = parts.after;
  return out;
}

/* count is the params telescope length, so a size_t carries the Bend Nat exactly. */
static TermSplit split_at(Heap *heap, size_t count, Vec terms) {
  TermSplit out;
  VecGet got;
  if (count == 0) {
    out.before = empty(heap);
    out.after = terms;
    return out;
  }
  got = vec_get(&terms, 0);
  if (got.status != VEC_GET_OK) {
    out.before = empty(heap);
    out.after = empty(heap);
    return out;
  }
  return prepend_split(heap, got.value, split_at(heap, count - 1, list_tail(terms)));
}

/* ---- Bend 760-767: elab_fam_ref ---- */

static Comp elab_fam_ref_after_args(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *args = value;
  TermSplit parts = split_at(heap, family_params(f->family).len, *args);
  return pure_term(term_lan(heap, shape_smu(family_name(f->family), parts.after), diagram_of(heap, &parts.before)));
}

static Comp elab_fam_ref_after_checked(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  (void)value;
  return bind_frame(heap, elab_terms(heap, &f->args, f->ctx), elab_fam_ref_after_args, f);
}

static Comp elab_fam_ref(Heap *heap, Ctx ctx, Family family, Vec args) {
  ElabFrame *f = frame_new(heap);
  Nat params_count;
  Nat want;
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->family = family;
  f->args = args;
  params_count = nat_of_len(heap, family_params(family).len);
  want = heap_nat_add(heap, params_count, nat_of_len(heap, family_indices(family).len));
  return bind_frame(heap, arity_is(heap, family_name(family), want, nat_of_len(heap, args.len)),
                    elab_fam_ref_after_checked, f);
}

/* ---- Bend 769-791: matching_param_env, ctor_param_shape, ctor_param_env ---- */

static Comp matching_param_env(Heap *heap, bool same, Ctx ctx, Closure diagram, Error error) {
  if (same) return lift_vec(heap, rules_mu_param_env(heap, ctx, diagram));
  return budget_failed(error);
}

static Comp ctor_param_shape(Heap *heap, const Value *value, Ctx ctx, Str family, Error error) {
  if (value->kind == VALUE_LAN && value->shape.kind == SHAPE_MU) {
    return matching_param_env(heap, str_eq(value->shape.name, family), ctx, value->diagram, error);
  }
  return budget_failed(error);
}

/* The frame carries the family name in message and the constructor name in word. */
static Comp ctor_param_env_after_whnf(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  Str parts[4];
  parts[0] = str_lit("the constructor ");
  parts[1] = f->word;
  parts[2] = str_lit(" expects family ");
  parts[3] = f->message;
  return ctor_param_shape(heap, value, f->ctx, f->message, error_make(ERROR_MISMATCH, cats(heap, 4, parts)));
}

static Comp ctor_param_env_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, whnf_in(heap, f->ctx, value), ctor_param_env_after_whnf, f);
}

static Comp ctor_param_env(Heap *heap, Vec params, Ctx ctx, MaybeValue expected, Str family, Str ctor) {
  ElabFrame *f;
  Str parts[2];
  if (params.len == 0) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->message = family;
  f->word = ctor;
  parts[0] = str_lit("the constructor ");
  parts[1] = ctor;
  return bind_frame(heap, require_value(expected, no_expect(heap, cats(heap, 2, parts))), ctor_param_env_after_ty, f);
}

/* ---- Bend 793-818: ConstructorFields, elab_ctor_fields ---- */

static Vec constructor_terms(Heap *heap, ConstructorFields fields) { return vec_reverse(heap->arena, &fields.terms); }

static Vec constructor_env(ConstructorFields fields) { return fields.env; }

static Comp pure_fields(Heap *heap, Vec terms, Vec env) {
  ConstructorFields *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->terms = terms;
  boxed->env = env;
  return budget_pure(boxed);
}

static Comp elab_ctor_fields(Heap *heap, Vec telescope, Vec args, Ctx ctx, Vec env, Vec terms);

static Comp elab_ctor_fields_after_value(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return elab_ctor_fields(heap, f->binders, f->args, f->ctx, cons(heap, value, &f->env),
                          cons(heap, (void *)f->term, &f->terms));
}

static Comp elab_ctor_fields_after_term(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, eval_in(heap, f->ctx, f->term), elab_ctor_fields_after_value, f);
}

static Comp elab_ctor_fields_after_ty(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, value_some(value), f->syntax), elab_ctor_fields_after_term, f);
}

static Comp elab_ctor_fields(Heap *heap, Vec telescope, Vec args, Ctx ctx, Vec env, Vec terms) {
  VecGet binder = vec_get(&telescope, 0);
  VecGet arg = vec_get(&args, 0);
  const TeleBinder *head;
  ElabFrame *f;
  if (telescope.len == 0 && args.len == 0) return pure_fields(heap, terms, env);
  if (binder.status != VEC_GET_OK || arg.status != VEC_GET_OK) {
    return mismatch("constructor field arity changed during elaboration");
  }
  head = binder.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->env = env;
  f->terms = terms;
  f->binders = list_tail(telescope);
  f->args = list_tail(args);
  f->syntax = arg.value;
  return bind_frame(heap, budget_lift(eval_eval(heap, globals_of(ctx), env, head->ty)), elab_ctor_fields_after_ty, f);
}

/* ---- Bend 820-838: eval_terms, quote_values ---- */

static Comp eval_terms(Heap *heap, Vec terms, Ctx ctx, Vec env);

static Comp eval_terms_after_tail(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *tail = value;
  return pure_vec(heap, cons(heap, (void *)f->value, tail));
}

static Comp eval_terms_after_head(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->value = value;
  return bind_frame(heap, eval_terms(heap, f->rest, f->ctx, f->env), eval_terms_after_tail, f);
}

static Comp eval_terms(Heap *heap, Vec terms, Ctx ctx, Vec env) {
  VecGet got = vec_get(&terms, 0);
  ElabFrame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->env = env;
  f->rest = list_tail(terms);
  return bind_frame(heap, budget_lift(eval_eval(heap, globals_of(ctx), env, got.value)), eval_terms_after_head, f);
}

static Comp quote_values(Heap *heap, Vec values, Ctx ctx);

static Comp quote_values_after_tail(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *tail = value;
  return pure_vec(heap, cons(heap, (void *)f->term, tail));
}

static Comp quote_values_after_head(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  f->term = value;
  return bind_frame(heap, quote_values(heap, f->rest, f->ctx), quote_values_after_tail, f);
}

static Comp quote_values(Heap *heap, Vec values, Ctx ctx) {
  VecGet got = vec_get(&values, 0);
  ElabFrame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->rest = list_tail(values);
  return bind_frame(heap, quote_in(heap, ctx, got.value), quote_values_after_head, f);
}

/* ---- Bend 840-851: elab_ctor_ref ---- */

static Comp elab_ctor_ref_after_quoted(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *indices = value;
  return pure_term(term_in(heap, shape_smu(family_name(f->family), *indices), term_addr_ctor(ctor_name(f->ctor)),
                           f->terms));
}

static Comp elab_ctor_ref_after_indices(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *indices = value;
  return bind_frame(heap, quote_values(heap, *indices, f->ctx), elab_ctor_ref_after_quoted, f);
}

static Comp elab_ctor_ref_after_fields(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const ConstructorFields *fields = value;
  f->terms = constructor_terms(heap, *fields);
  f->env = constructor_env(*fields);
  return bind_frame(heap, eval_terms(heap, ctor_indices(f->ctor), f->ctx, f->env), elab_ctor_ref_after_indices, f);
}

static Comp elab_ctor_ref_after_env(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  const Vec *env = value;
  return bind_frame(heap, elab_ctor_fields(heap, ctor_args(f->ctor), f->args, f->ctx, *env, empty(heap)),
                    elab_ctor_ref_after_fields, f);
}

static Comp elab_ctor_ref_after_checked(Heap *heap, void *ctx, void *value) {
  ElabFrame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    ctor_param_env(heap, family_params(f->family), f->ctx, f->expected, family_name(f->family),
                                   ctor_name(f->ctor)),
                    elab_ctor_ref_after_env, f);
}

static Comp elab_ctor_ref(Heap *heap, Ctx ctx, MaybeValue expected, Family family, Ctor ctor, Vec args) {
  ElabFamilyCtor resolved = ctor_in(heap, ctx, expected, family, ctor);
  ElabFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected = expected;
  f->family = resolved.family;
  f->ctor = resolved.ctor;
  f->args = args;
  return bind_frame(heap,
                    arity_is(heap, ctor_name(resolved.ctor), nat_of_len(heap, ctor_args(resolved.ctor).len),
                             nat_of_len(heap, args.len)),
                    elab_ctor_ref_after_checked, f);
}

/* ---- Bend 853-899: elab_mu_ref .. elab_app ---- */

static Comp elab_mu_ref(Heap *heap, ElabMuRef reference, Ctx ctx, MaybeValue expected, Vec args) {
  switch (reference.kind) {
  case ELAB_RFAM:
    return elab_fam_ref(heap, ctx, reference.family, args);
  case ELAB_RCTOR:
    return elab_ctor_ref(heap, ctx, expected, reference.family, reference.ctor, args);
  }
  return elab_fam_ref(heap, ctx, reference.family, args);
}

static const Term *variable(Heap *heap, MaybeNat index, Str name) {
  if (index.some) return term_var(heap, index.value);
  return term_global(heap, name);
}

static Comp elaborate_name(Heap *heap, MaybeElabMuRef reference, Ctx ctx, MaybeValue expected, Str name) {
  if (reference.some) return elab_mu_ref(heap, reference.value, ctx, expected, empty(heap));
  return pure_term(variable(heap, check_index_of_name(heap, ctx, name), name));
}

static MaybeElabMuRef spine_reference(Heap *heap, ElabSpine spine, Ctx ctx) {
  if (spine.head->kind == STERM_VAR) return mu_ref_of(heap, ctx, spine.head->name);
  return mu_ref_none();
}

static Vec spine_args(ElabSpine spine) { return spine.args; }

static Comp elaborate_application(Heap *heap, MaybeElabMuRef reference, Ctx ctx, MaybeValue expected,
                                  const STerm *head, const STerm *arg, Vec args) {
  if (reference.some) return elab_mu_ref(heap, reference.value, ctx, expected, args);
  return elab_app_point(heap, ctx, head, arg);
}

/* spine_of(SApp{head, arg}, []) peels the application node first, so the walk starts at head with [arg]. */
static Comp elab_app(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *head, const STerm *arg) {
  ElabSpine spine = spine_of(heap, head, singleton(heap, (void *)arg));
  return elaborate_application(heap, spine_reference(heap, spine, ctx), ctx, expected, head, arg, spine_args(spine));
}

/* ---- Bend 901-1121: branch targets, motives, collection and mu cases ---- */

typedef struct {
  Ctx ctx;
  Vec env;
  Vec values;
} BindState;

typedef struct {
  bool some;
  MaybeValue value;
} MaybeExpectation;

/* One frame per do block of this part; every `<-` is a NAME_after_X continuation. */
typedef struct {
  Ctx ctx;
  MaybeValue expected;
  MaybeMotive motive;
  MaybeSMotive syntax_motive;
  MaybeSTerm annotation;
  MaybeStr ind;
  Vec indices;
  Vec legs;
  Vec env;
  Vec values;
  Vec branches;  /* SBranch pointers */
  Vec telescope; /* TeleBinder pointers */
  Vec names;     /* Str pointers */
  Vec fields;    /* SField pointers */
  size_t from;
  Shape shape;
  Closure diagram;
  MaybeLevelVar level;
  Big index;
  Big count;
  Quantity quantity;
  Str name;
  Str self;
  Str ctor_name;
  Family family;
  Ctor ctor;
  BindState state;
  const Value *ty;
  const Value *expected_value;
  const Term *scrut;
  const Term *term;
  const STerm *body;
  const STerm *scrut_syntax;
  Branch *head;
  bool fibered;
} CaseFrame;

static CaseFrame *case_frame_new(Heap *heap) { return heap_alloc(heap, sizeof(CaseFrame)); }

static Comp bind_case(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), CaseFrame *frame) {
  if (frame == NULL) return comp_oom();
  return bind_with(heap, first, fn, frame);
}

static void *case_item(const Vec *xs, size_t i) {
  VecGet got = vec_get(xs, i);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

static Str cat_ss(Heap *heap, Str a, Str b) {
  Str parts[2];
  parts[0] = a;
  parts[1] = b;
  return cats(heap, 2, parts);
}

static Comp pure_maybe_motive(Heap *heap, MaybeMotive value) {
  MaybeMotive *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp pure_state(Heap *heap, BindState value) {
  BindState *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp pure_branch(Branch *branch) {
  if (branch == NULL) return comp_oom();
  return budget_pure(branch);
}

static Comp require_ctor(Heap *heap, MaybeCtor ctor, Error error) {
  Ctor *boxed;
  if (!ctor.some) return budget_failed(error);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ctor.value;
  return budget_pure(boxed);
}

static Comp require_family(Heap *heap, MaybeFamily family, Error error) {
  Family *boxed;
  if (!family.some) return budget_failed(error);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = family.value;
  return budget_pure(boxed);
}

/* branch_target (901-908) */
static Comp branch_target_after_value(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_maybe_value(heap, value_some(value));
}

static Comp branch_target(Heap *heap, MaybeMotive motive, MaybeValue expected, Ctx ctx, Vec indices, const Value *self) {
  Vec env;
  Vec outer;
  size_t i;
  if (!motive.some) return pure_maybe_value(heap, expected);
  env = empty(heap);
  push(heap, &env, (void *)self);
  for (i = indices.len; i > 0; i--) push(heap, &env, case_item(&indices, i - 1));
  outer = env_of(ctx);
  for (i = 0; i < outer.len; i++) push(heap, &env, case_item(&outer, i));
  return bind_with(heap, budget_lift(eval_eval(heap, globals_of(ctx), env, motive.value.body)), branch_target_after_value, NULL);
}

/* coll_motive_plain (910-915) */
static Comp coll_motive_plain(Heap *heap, MaybeStr ind) {
  if (!ind.some) return budget_pure(kernel_unit());
  return budget_failed(error_make(
      ERROR_MISMATCH, cat_ss(heap, str_lit("a case at a collection takes no index clause and its motive names "), ind.value)));
}

/* elab_motive (917-926) */
static Comp elab_motive_after_body(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return pure_maybe_motive(heap, term_motive_some(term_motive(str_none(), empty(heap), f->self, value)));
}

static Comp elab_motive_after_checked(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  (void)value;
  return bind_case(heap, elab_term_elab(heap, check_bind(heap, f->self, QUANTITY_ZERO, f->ty, f->ctx), value_none(), f->body),
                   elab_motive_after_body, f);
}

static Comp elab_motive(Heap *heap, MaybeSMotive motive, Ctx ctx, const Value *ty) {
  CaseFrame *f;
  if (!motive.some) return pure_maybe_motive(heap, term_motive_none());
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->ty = ty;
  f->self = motive.value.self;
  f->body = motive.value.body;
  return bind_case(heap, coll_motive_plain(heap, motive.value.ind), elab_motive_after_checked, f);
}

/* coll_branch_body (928-938) */
static Comp coll_branch_body_after_body(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  Binder *binder = term_binder_new(heap, f->quantity, f->name);
  if (binder == NULL) return comp_oom();
  return pure_branch(term_branch_new(heap, term_addr_leg(f->index), term_leg(singleton(heap, binder), value)));
}

static Comp coll_branch_body_after_target(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  MaybeValue target = *(MaybeValue *)value;
  return bind_case(heap, elab_term_elab(heap, check_bind(heap, f->name, f->quantity, f->ty, f->ctx), target, f->body),
                   coll_branch_body_after_body, f);
}

static Comp coll_branch_body_after_ty(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *var = value_var(heap, size_of(f->ctx));
  const Value *self;
  f->ty = value;
  if (var == NULL) return comp_oom();
  self = value_vin(heap, f->shape, value_vaddr_leg(f->index), singleton(heap, (void *)var));
  if (self == NULL) return comp_oom();
  return bind_case(heap, branch_target(heap, f->motive, f->expected, f->ctx, empty(heap), self), coll_branch_body_after_target, f);
}

static Comp coll_branch_body(Heap *heap, Vec binders, Ctx ctx, Shape shape, Vec legs, MaybeMotive motive, MaybeValue expected,
                             Big index, const STerm *body) {
  CaseFrame *f;
  const SBinder *binder;
  if (binders.len != 1) return budget_failed(error_make(ERROR_MISSING_BRANCH, str_lit("each branch binds its payload once")));
  binder = case_item(&binders, 0);
  if (binder == NULL) return comp_oom();
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = shape;
  f->motive = motive;
  f->expected = expected;
  f->index = index;
  f->body = body;
  f->quantity = binder->quantity;
  f->name = binder->name;
  return bind_case(heap, budget_lift(rules_coll_leg_ty(heap, ctx, &legs, index)), coll_branch_body_after_ty, f);
}

/* elab_branch (940-946) */
static Comp elab_branch(Heap *heap, const SBranch *branch, Ctx ctx, Shape shape, Vec legs, MaybeMotive motive, MaybeValue expected) {
  Str parts[3];
  if (branch == NULL) return comp_oom();
  switch (branch->kind) {
  case SBRANCH_CTOR:
    parts[0] = str_lit("the branch at ");
    parts[1] = branch->name;
    parts[2] = str_lit(" keys a constructor and a case at a collection keys a leg");
    return budget_failed(error_make(ERROR_WRONG_LEG, cats(heap, 3, parts)));
  case SBRANCH_LEG:
    return coll_branch_body(heap, branch->binders, ctx, shape, legs, motive, expected, branch->index, branch->body);
  }
  return comp_oom();
}

/* elab_coll_branches (948-957) */
static Comp elab_coll_branches_from(Heap *heap, Vec branches, size_t from, Ctx ctx, Shape shape, Vec legs, MaybeMotive motive,
                                    MaybeValue expected);

static Comp elab_coll_branches_after_tail(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return pure_vec(heap, cons(heap, f->head, (const Vec *)value));
}

static Comp elab_coll_branches_after_head(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->head = value;
  return bind_case(heap, elab_coll_branches_from(heap, f->branches, f->from + 1, f->ctx, f->shape, f->legs, f->motive, f->expected),
                   elab_coll_branches_after_tail, f);
}

static Comp elab_coll_branches_from(Heap *heap, Vec branches, size_t from, Ctx ctx, Shape shape, Vec legs, MaybeMotive motive,
                                    MaybeValue expected) {
  CaseFrame *f;
  if (from >= branches.len) return pure_vec(heap, empty(heap));
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->branches = branches;
  f->from = from;
  f->ctx = ctx;
  f->shape = shape;
  f->legs = legs;
  f->motive = motive;
  f->expected = expected;
  return bind_case(heap, elab_branch(heap, case_item(&branches, from), ctx, shape, legs, motive, expected),
                   elab_coll_branches_after_head, f);
}

static Comp elab_coll_branches(Heap *heap, Vec branches, Ctx ctx, Shape shape, Vec legs, MaybeMotive motive, MaybeValue expected) {
  return elab_coll_branches_from(heap, branches, 0, ctx, shape, legs, motive, expected);
}

/* elab_coll_case (959-965) */
static Comp elab_coll_case_after_branches(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  Vec branches = *(Vec *)value;
  return pure_term(term_elim(heap, term_elim_data(shape_scoll(f->count), f->scrut, QUANTITY_ONE, f->motive, branches)));
}

static Comp elab_coll_case_after_motive(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->motive = *(MaybeMotive *)value;
  return bind_case(heap, elab_coll_branches(heap, f->branches, f->ctx, shape_scoll(f->count), f->legs, f->motive, f->expected),
                   elab_coll_case_after_branches, f);
}

static Comp elab_coll_case_after_legs(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *ty;
  f->legs = *(Vec *)value;
  ty = value_vlan(heap, shape_scoll(f->count), f->diagram, f->level);
  if (ty == NULL) return comp_oom();
  return bind_case(heap, elab_motive(heap, f->syntax_motive, f->ctx, ty), elab_coll_case_after_motive, f);
}

static Comp elab_coll_case(Heap *heap, Ctx ctx, MaybeValue expected, const Term *scrut, Big count, Closure diagram, MaybeLevelVar level,
                           MaybeSMotive motive, Vec branches) {
  CaseFrame *f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected = expected;
  f->scrut = scrut;
  f->count = count;
  f->diagram = diagram;
  f->level = level;
  f->syntax_motive = motive;
  f->branches = branches;
  return bind_case(heap, lift_vec(heap, rules_coll_legs_of(heap, ctx, diagram)), elab_coll_case_after_legs, f);
}

/* BindState accessors (967-983) */
static Ctx bind_context(BindState state) { return state.ctx; }

static Vec bind_env(BindState state) { return state.env; }

static Vec bind_values(Heap *heap, BindState state) { return vec_reverse(heap->arena, &state.values); }

/* bind_indices (985-995) */
static Comp bind_indices_from(Heap *heap, Vec telescope, Vec names, size_t from, Ctx ctx, Vec env, Vec values);

static Comp bind_indices_after_ty(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *var = value_var(heap, size_of(f->ctx));
  if (var == NULL) return comp_oom();
  return bind_indices_from(heap, f->telescope, f->names, f->from + 1, check_bind(heap, f->name, QUANTITY_ZERO, value, f->ctx),
                           cons(heap, (void *)var, &f->env), cons(heap, (void *)var, &f->values));
}

static Comp bind_indices_from(Heap *heap, Vec telescope, Vec names, size_t from, Ctx ctx, Vec env, Vec values) {
  CaseFrame *f;
  const TeleBinder *binder;
  const Str *name;
  BindState state;
  bool more_binders = from < telescope.len;
  bool more_names = from < names.len;
  if (!more_binders && !more_names) {
    state.ctx = ctx;
    state.env = env;
    state.values = values;
    return pure_state(heap, state);
  }
  if (!more_binders || !more_names) {
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("motive index arity changed during elaboration")));
  }
  binder = case_item(&telescope, from);
  name = case_item(&names, from);
  if (binder == NULL || name == NULL) return comp_oom();
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->telescope = telescope;
  f->names = names;
  f->from = from;
  f->ctx = ctx;
  f->env = env;
  f->values = values;
  f->name = *name;
  return bind_case(heap, budget_lift(eval_eval(heap, globals_of(ctx), env, binder->ty)), bind_indices_after_ty, f);
}

static Comp bind_indices(Heap *heap, Vec telescope, Vec names, Ctx ctx, Vec env, Vec values) {
  return bind_indices_from(heap, telescope, names, 0, ctx, env, values);
}

/* motive_arity (997-999) */
static Comp motive_arity(Heap *heap, Str name, Nat want, Nat got) {
  Str parts[6];
  if (nat_cmp(want, got) == 0) return budget_pure(kernel_unit());
  parts[0] = str_lit("the motive of ");
  parts[1] = name;
  parts[2] = str_lit(" binds ");
  parts[3] = heap_nat_show(heap, got);
  parts[4] = str_lit(" index names and the family has ");
  parts[5] = heap_nat_show(heap, want);
  return budget_failed(error_make(ERROR_MISMATCH, cats(heap, 6, parts)));
}

/* elab_mu_motive (1001-1012) */
static Comp elab_mu_motive_after_body(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return pure_maybe_motive(heap, term_motive_some(term_motive(f->ind, f->names, f->self, value)));
}

static Comp elab_mu_motive_after_state(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *self_type;
  f->state = *(BindState *)value;
  self_type = value_vlan(heap, shape_smu(f->name, bind_values(heap, f->state)), f->diagram, f->level);
  if (self_type == NULL) return comp_oom();
  return bind_case(heap,
                   elab_term_elab(heap, check_bind(heap, f->self, QUANTITY_ZERO, self_type, bind_context(f->state)), value_none(),
                                  f->body),
                   elab_mu_motive_after_body, f);
}

static Comp elab_mu_motive_after_checked(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  (void)value;
  return bind_case(heap, bind_indices(heap, f->family.indices, f->names, f->ctx, f->env, empty(heap)), elab_mu_motive_after_state, f);
}

static Comp elab_mu_motive(Heap *heap, MaybeSMotive motive, Ctx ctx, Str name, Family family, Closure diagram, MaybeLevelVar level,
                           Vec env) {
  CaseFrame *f;
  if (!motive.some) return pure_maybe_motive(heap, term_motive_none());
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->name = name;
  f->family = family;
  f->diagram = diagram;
  f->level = level;
  f->env = env;
  f->self = motive.value.self;
  f->ind = motive.value.ind;
  f->names = motive.value.indices;
  f->body = motive.value.body;
  return bind_case(heap, motive_arity(heap, name, nat_of_len(heap, family.indices.len), nat_of_len(heap, motive.value.indices.len)),
                   elab_mu_motive_after_checked, f);
}

/* field_agrees (1014-1019) */
static Comp field_agrees(Heap *heap, bool agrees, Str name) {
  Str parts[3];
  if (agrees) return budget_pure(kernel_unit());
  parts[0] = str_lit("the annotation of constructor field ");
  parts[1] = name;
  parts[2] = str_lit(" differs from its declared type");
  return budget_failed(error_make(ERROR_MISMATCH, cats(heap, 3, parts)));
}

/* check_field_annotation (1021-1032) */
static Comp check_field_annotation_after_agrees(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return field_agrees(heap, kernel_bool_unbox(value), f->name);
}

static Comp check_field_annotation_after_actual(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  /* D-269a: a top-level conversion entry (scratch query under the D-261a guard). */
  return bind_case(heap, check_engine_conv_type_entry(heap, f->ctx, value, f->expected_value),
                   check_field_annotation_after_agrees, f);
}

static Comp check_field_annotation_after_level(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  (void)value;
  return bind_case(heap, eval_in(heap, f->ctx, f->term), check_field_annotation_after_actual, f);
}

static Comp check_field_annotation_after_term(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->term = value;
  return bind_case(heap, check_engine_infer_univ(heap, f->ctx, f->term), check_field_annotation_after_level, f);
}

static Comp check_field_annotation(Heap *heap, MaybeSTerm annotation, Ctx ctx, const Value *expected, Str name) {
  CaseFrame *f;
  if (!annotation.some) return budget_pure(kernel_unit());
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected_value = expected;
  f->name = name;
  return bind_case(heap, elab_term_elab(heap, ctx, value_none(), annotation.value), check_field_annotation_after_term, f);
}

/* bind_fields (1034-1046) */
static Comp bind_fields_from(Heap *heap, Vec telescope, Vec fields, size_t from, Ctx ctx, Vec env, Vec values);

static Comp bind_fields_after_checked(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *var;
  (void)value;
  var = value_var(heap, size_of(f->ctx));
  if (var == NULL) return comp_oom();
  return bind_fields_from(heap, f->telescope, f->fields, f->from + 1, check_bind(heap, f->name, f->quantity, f->ty, f->ctx),
                          cons(heap, (void *)var, &f->env), cons(heap, (void *)var, &f->values));
}

static Comp bind_fields_after_ty(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->ty = value;
  return bind_case(heap, check_field_annotation(heap, f->annotation, f->ctx, f->ty, f->name), bind_fields_after_checked, f);
}

static Comp bind_fields_from(Heap *heap, Vec telescope, Vec fields, size_t from, Ctx ctx, Vec env, Vec values) {
  CaseFrame *f;
  const TeleBinder *binder;
  const SField *field;
  BindState state;
  bool more_binders = from < telescope.len;
  bool more_fields = from < fields.len;
  if (!more_binders && !more_fields) {
    state.ctx = ctx;
    state.env = env;
    state.values = values;
    return pure_state(heap, state);
  }
  if (!more_binders || !more_fields) {
    return budget_failed(error_make(ERROR_MISSING_BRANCH, str_lit("constructor field arity changed during elaboration")));
  }
  binder = case_item(&telescope, from);
  field = case_item(&fields, from);
  if (binder == NULL || field == NULL) return comp_oom();
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->telescope = telescope;
  f->fields = fields;
  f->from = from;
  f->ctx = ctx;
  f->env = env;
  f->values = values;
  f->quantity = field->quantity;
  f->name = field->name;
  f->annotation = field->ty;
  return bind_case(heap, budget_lift(eval_eval(heap, globals_of(ctx), env, binder->ty)), bind_fields_after_ty, f);
}

static Comp bind_fields(Heap *heap, Vec telescope, Vec fields, Ctx ctx, Vec env, Vec values) {
  return bind_fields_from(heap, telescope, fields, 0, ctx, env, values);
}

/* branch_arity (1048-1050) */
static Comp branch_arity(Heap *heap, Str name, Nat want, Nat got) {
  Str parts[7];
  if (nat_cmp(want, got) == 0) return budget_pure(kernel_unit());
  parts[0] = str_lit("the branch at ");
  parts[1] = name;
  parts[2] = str_lit(" binds ");
  parts[3] = heap_nat_show(heap, got);
  parts[4] = str_lit(" fields and ");
  parts[5] = name;
  parts[6] = cat_ss(heap, str_lit(" takes "), heap_nat_show(heap, want));
  return budget_failed(error_make(ERROR_MISSING_BRANCH, cats(heap, 7, parts)));
}

/* field_binders (1052-1057): List<Pair<Quantity, String>> as Binder pointers in field order. */
static Vec field_binders(Heap *heap, Vec fields) {
  Vec binders = empty(heap);
  size_t i;
  for (i = 0; i < fields.len; i++) {
    const SField *field = case_item(&fields, i);
    if (field == NULL) continue;
    push(heap, &binders, term_binder_new(heap, field->quantity, field->name));
  }
  return binders;
}

/* elab_mu_branch (1059-1072) */
static Comp elab_mu_branch_after_body(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return pure_branch(term_branch_new(heap, term_addr_ctor(f->ctor_name), term_leg(field_binders(heap, f->fields), value)));
}

static Comp elab_mu_branch_after_target(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  MaybeValue target = *(MaybeValue *)value;
  return bind_case(heap, elab_term_elab(heap, bind_context(f->state), target, f->body), elab_mu_branch_after_body, f);
}

static Comp elab_mu_branch_after_indices(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  const Value *self;
  f->indices = *(Vec *)value;
  self = value_vin(heap, shape_smu(f->name, f->indices), value_vaddr_ctor(f->ctor_name), bind_values(heap, f->state));
  if (self == NULL) return comp_oom();
  return bind_case(heap, branch_target(heap, f->motive, f->expected, f->ctx, f->indices, self), elab_mu_branch_after_target, f);
}

static Comp elab_mu_branch_after_state(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->state = *(BindState *)value;
  return bind_case(heap, eval_terms(heap, ctor_indices(f->ctor), f->ctx, bind_env(f->state)), elab_mu_branch_after_indices, f);
}

static Comp elab_mu_branch_after_checked(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  (void)value;
  return bind_case(heap, bind_fields(heap, ctor_args(f->ctor), f->fields, f->ctx, f->env, empty(heap)), elab_mu_branch_after_state, f);
}

static Comp elab_mu_branch_after_ctor(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->ctor = *(Ctor *)value;
  return bind_case(heap,
                   branch_arity(heap, f->ctor_name, nat_of_len(heap, ctor_args(f->ctor).len), nat_of_len(heap, f->fields.len)),
                   elab_mu_branch_after_checked, f);
}

static Comp elab_mu_branch(Heap *heap, const SBranch *branch, Ctx ctx, Str name, Family family, MaybeMotive motive, Vec env,
                           MaybeValue expected) {
  CaseFrame *f;
  Str parts[4];
  if (branch == NULL) return comp_oom();
  switch (branch->kind) {
  case SBRANCH_LEG:
    parts[0] = str_lit("the branch ");
    parts[1] = big_to_string(heap, branch->index);
    parts[2] = str_lit(" keys a leg and a case at ");
    parts[3] = cat_ss(heap, name, str_lit(" keys a constructor"));
    return budget_failed(error_make(ERROR_WRONG_LEG, cats(heap, 4, parts)));
  case SBRANCH_CTOR:
    f = case_frame_new(heap);
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->name = name;
    f->family = family;
    f->motive = motive;
    f->env = env;
    f->expected = expected;
    f->ctor_name = branch->name;
    f->fields = branch->fields;
    f->body = branch->body;
    parts[0] = branch->name;
    parts[1] = str_lit(" is not a constructor of ");
    parts[2] = name;
    return bind_case(heap,
                     require_ctor(heap, positivity_ctor_of(branch->name, family), error_make(ERROR_UNBOUND, cats(heap, 3, parts))),
                     elab_mu_branch_after_ctor, f);
  }
  return comp_oom();
}

/* elab_mu_branches (1074-1083) */
static Comp elab_mu_branches_from(Heap *heap, Vec branches, size_t from, Ctx ctx, Str name, Family family, MaybeMotive motive,
                                  Vec env, MaybeValue expected);

static Comp elab_mu_branches_after_tail(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return pure_vec(heap, cons(heap, f->head, (const Vec *)value));
}

static Comp elab_mu_branches_after_head(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->head = value;
  return bind_case(heap,
                   elab_mu_branches_from(heap, f->branches, f->from + 1, f->ctx, f->name, f->family, f->motive, f->env, f->expected),
                   elab_mu_branches_after_tail, f);
}

static Comp elab_mu_branches_from(Heap *heap, Vec branches, size_t from, Ctx ctx, Str name, Family family, MaybeMotive motive,
                                  Vec env, MaybeValue expected) {
  CaseFrame *f;
  if (from >= branches.len) return pure_vec(heap, empty(heap));
  f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->branches = branches;
  f->from = from;
  f->ctx = ctx;
  f->name = name;
  f->family = family;
  f->motive = motive;
  f->env = env;
  f->expected = expected;
  return bind_case(heap, elab_mu_branch(heap, case_item(&branches, from), ctx, name, family, motive, env, expected),
                   elab_mu_branches_after_head, f);
}

static Comp elab_mu_branches(Heap *heap, Vec branches, Ctx ctx, Str name, Family family, MaybeMotive motive, Vec env,
                             MaybeValue expected) {
  return elab_mu_branches_from(heap, branches, 0, ctx, name, family, motive, env, expected);
}

/* elab_mu_case (1085-1093) */
static Comp elab_mu_case_after_indices(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  Vec indices = *(Vec *)value;
  return pure_term(
      term_elim(heap, term_elim_data(shape_smu(f->name, indices), f->scrut, QUANTITY_ONE, f->motive, f->branches)));
}

static Comp elab_mu_case_after_branches(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->branches = *(Vec *)value;
  return bind_case(heap, quote_values(heap, f->indices, f->ctx), elab_mu_case_after_indices, f);
}

static Comp elab_mu_case_after_motive(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->motive = *(MaybeMotive *)value;
  return bind_case(heap, elab_mu_branches(heap, f->branches, f->ctx, f->name, f->family, f->motive, f->env, f->expected),
                   elab_mu_case_after_branches, f);
}

static Comp elab_mu_case_after_env(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->env = *(Vec *)value;
  return bind_case(heap, elab_mu_motive(heap, f->syntax_motive, f->ctx, f->name, f->family, f->diagram, f->level, f->env),
                   elab_mu_case_after_motive, f);
}

static Comp elab_mu_case_after_family(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->family = *(Family *)value;
  return bind_case(heap, lift_vec(heap, rules_mu_param_env(heap, f->ctx, f->diagram)), elab_mu_case_after_env, f);
}

static Comp elab_mu_case(Heap *heap, Ctx ctx, MaybeValue expected, const Term *scrut, Str name, Vec indices, Closure diagram,
                         MaybeLevelVar level, MaybeSMotive motive, Vec branches) {
  Str parts[3];
  CaseFrame *f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected = expected;
  f->scrut = scrut;
  f->name = name;
  f->indices = indices;
  f->diagram = diagram;
  f->level = level;
  f->syntax_motive = motive;
  f->branches = branches;
  parts[0] = str_lit("the family ");
  parts[1] = name;
  parts[2] = str_lit(" is not declared");
  return bind_case(heap,
                   require_family(heap, global_find_family(name, globals_of(ctx)), error_make(ERROR_UNBOUND, cats(heap, 3, parts))),
                   elab_mu_case_after_family, f);
}

/* eliminate_type (1095-1107): the Bend arms in order, VLan{SMu} first for either flag. */
static Comp eliminate_type(Heap *heap, const Value *ty, bool fibered, Ctx ctx, MaybeValue expected, const Term *scrut,
                           MaybeSMotive motive, Vec branches) {
  if (ty == NULL) return comp_oom();
  if (ty->kind == VALUE_LAN && ty->shape.kind == SHAPE_MU) {
    return elab_mu_case(heap, ctx, expected, scrut, ty->shape.name, ty->shape.args, ty->diagram, ty->former_level, motive, branches);
  }
  if (fibered) return mismatch("a match needs a mu family as the type of its scrutinee");
  if (ty->kind != VALUE_LAN) return mismatch("a case needs a left former as the type of its scrutinee");
  if (ty->shape.kind == SHAPE_COLL) {
    return elab_coll_case(heap, ctx, expected, scrut, ty->shape.count, ty->diagram, ty->former_level, motive, branches);
  }
  return mismatch("a case eliminates a collection");
}

/* elab_case (1109-1114) */
static Comp elab_case_after_ty(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  return eliminate_type(heap, value, f->fibered, f->ctx, f->expected, f->scrut, f->syntax_motive, f->branches);
}

static Comp elab_case_after_scrut(Heap *heap, void *ctx, void *value) {
  CaseFrame *f = ctx;
  f->scrut = value;
  return bind_case(heap, type_of(heap, f->ctx, f->scrut), elab_case_after_ty, f);
}

static Comp elab_case(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *scrut, MaybeSMotive motive, Vec branches, bool fibered) {
  CaseFrame *f = case_frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->expected = expected;
  f->scrut_syntax = scrut;
  f->syntax_motive = motive;
  f->branches = branches;
  f->fibered = fibered;
  return bind_case(heap, elab_term_elab(heap, ctx, value_none(), scrut), elab_case_after_scrut, f);
}

/* optional_expectation (1116-1121) */
static MaybeValue optional_expectation(MaybeExpectation value) { return value.some ? value.value : value_none(); }

/* ===== chunk 3b: elab (1123-1225), telescopes, chains, family and constructor declarations,
 * elab_decl (1227-1365) ===== */

/* One frame per do block of the elab dispatcher: the arms that bind more than one value keep the
 * earlier results here (first = FHC_TY level, MPC_TY parties, LET ty, ANN ty; second = MPC_TY access,
 * LET value; ty_value = LET and ANN). */
typedef struct {
  Ctx ctx;
  MaybeValue expected;
  const STerm *syntax;
  const Term *first;
  const Term *second;
  const Value *ty_value;
} DispatchFrame;

static DispatchFrame *dispatch_frame(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *syntax) {
  DispatchFrame *f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return NULL;
  f->ctx = ctx;
  f->expected = expected;
  f->syntax = syntax;
  f->first = NULL;
  f->second = NULL;
  f->ty_value = NULL;
  return f;
}

static Comp bind_dispatch(Heap *heap, DispatchFrame *f, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value)) {
  if (f == NULL) return comp_oom();
  return bind_with(heap, first, fn, f);
}

/* Syntax.SNat{value}: Term.Lit{Literal.LInt{value}} */
static Literal int_literal(Big value) {
  Literal lit;
  lit.kind = LITERAL_INT;
  lit.string = str_lit("");
  lit.integer = value;
  return lit;
}

/* Syntax.STuple{items} (1146-1149) */
static Comp tuple_after_terms(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  const Vec *terms = value;
  return pure_term(term_sec(heap, coll_of_len(heap, f->syntax->items.len), legs_of(heap, terms)));
}

/* Syntax.SSum{items} (1150-1153) */
static Comp sum_after_terms(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(sum_ty(heap, value));
}

/* Syntax.SProd{items} (1154-1157) */
static Comp prod_after_terms(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(prod_ty(heap, value));
}

/* Syntax.SInj{index, count, value} (1160-1164): Bignum.at over the expectations list is big_at; a
 * missing entry is the Maybe None that optional_expectation turns into no expectation. */
static Comp inj_after_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  return pure_term(term_in(heap, shape_scoll(f->syntax->count), term_addr_leg(f->syntax->index),
                           singleton(heap, value)));
}

static Comp inj_after_expectations(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  const Vec *expectations = value;
  VecGet got = big_at(heap, expectations, f->syntax->index);
  MaybeExpectation at;
  at.some = got.status == VEC_GET_OK;
  at.value = at.some ? *(MaybeValue *)got.value : value_none();
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, optional_expectation(at), f->syntax->value),
                       inj_after_value);
}

/* Syntax.SAbsurd{value} (1165-1168) */
static Comp absurd_after_value(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(term_elim(heap, term_elim_data(shape_scoll(big_zero()), value, QUANTITY_ONE,
                                                  term_motive_none(), empty(heap))));
}

/* Syntax.SFhcTy{level, ty} (1183-1187) */
static Comp fhc_after_ty(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  return pure_term(term_ran(heap, shape_sfhc((void *)f->first), value));
}

static Comp fhc_after_level(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->first = value;
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, value_none(), f->syntax->ty), fhc_after_ty);
}

/* Syntax.SMpcTy{parties, access, ty} (1194-1199) */
static Comp mpc_after_ty(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  return pure_term(term_ran(heap, shape_smpc((void *)f->first, (void *)f->second), value));
}

static Comp mpc_after_access(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->second = value;
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, value_none(), f->syntax->ty), mpc_after_ty);
}

static Comp mpc_after_parties(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->first = value;
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, value_none(), f->syntax->access),
                       mpc_after_access);
}

/* Syntax.SLet{name, ty, value, body} (1208-1215) */
static Comp let_after_body(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  return pure_term(term_let(heap, f->syntax->name, f->first, f->second, value));
}

static Comp let_after_value_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  Ctx extended = check_define(heap, f->syntax->name, QUANTITY_MANY, f->ty_value, value, f->ctx);
  return bind_dispatch(heap, f, elab_term_elab(heap, extended, f->expected, f->syntax->body), let_after_body);
}

static Comp let_after_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->second = value;
  return bind_dispatch(heap, f, eval_in(heap, f->ctx, f->second), let_after_value_value);
}

static Comp let_after_ty_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->ty_value = value;
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, value_some(f->ty_value), f->syntax->value),
                       let_after_value);
}

static Comp let_after_ty(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->first = value;
  return bind_dispatch(heap, f, eval_in(heap, f->ctx, f->first), let_after_ty_value);
}

/* Syntax.SAnn{value, ty} (1216-1221) */
static Comp ann_after_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  return pure_term(term_ann(heap, value, f->first));
}

static Comp ann_after_ty_value(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->ty_value = value;
  return bind_dispatch(heap, f, elab_term_elab(heap, f->ctx, value_some(f->ty_value), f->syntax->value),
                       ann_after_value);
}

static Comp ann_after_ty(Heap *heap, void *ctx, void *value) {
  DispatchFrame *f = ctx;
  f->first = value;
  return bind_dispatch(heap, f, eval_in(heap, f->ctx, f->first), ann_after_ty_value);
}

/* elab (1123-1225): the dispatcher over every Syntax kind, in the order of the Bend match. */
Comp elab_term_elab(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *syntax) {
  switch (syntax->kind) {
  case STERM_VAR:
    return elaborate_name(heap, mu_ref_of(heap, ctx, syntax->name), ctx, expected, syntax->name);
  case STERM_NAT:
    return pure_term(term_lit(heap, int_literal(syntax->nat)));
  case STERM_PROP:
    return pure_term(term_univ(heap, level_zero()));
  case STERM_TYPE:
    return univ_of_int(heap, big_add(heap, big_one(), syntax->nat));
  case STERM_SORT:
    return bind_with(heap, budget_lift(universe_lower(heap, syntax->sort)), univ_of_int_after_level, NULL);
  case STERM_PRIM:
    return pure_term(term_global(heap, syntax_prim_name(syntax->prim)));
  case STERM_UNIT:
    return pure_term(unit_val(heap));
  case STERM_AUTO:
    return pure_term(term_auto());
  case STERM_PAIR:
    return elab_pair(heap, ctx, expected, syntax->left, syntax->right);
  case STERM_TUPLE:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_items(heap, ctx, expected, &syntax->items, false), tuple_after_terms);
  case STERM_SUM:
    return bind_with(heap, elab_terms(heap, &syntax->items, ctx), sum_after_terms, NULL);
  case STERM_PROD:
    return bind_with(heap, elab_terms(heap, &syntax->items, ctx), prod_after_terms, NULL);
  case STERM_PROJ:
    return elab_proj(heap, ctx, syntax->head, syntax->index);
  case STERM_INJ:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_term_injection_expectations(heap, ctx, expected, syntax->count), inj_after_expectations);
  case STERM_ABSURD:
    return bind_with(heap, elab_term_elab(heap, ctx, value_none(), syntax->value), absurd_after_value, NULL);
  case STERM_APP:
    return elab_app(heap, ctx, expected, syntax->head, syntax->arg);
  case STERM_FUN:
    return elab_fun(heap, &syntax->binders, ctx, expected, syntax->body);
  case STERM_ARROW:
    return elab_group(heap, ctx, syntax->binder, syntax->body, 0);
  case STERM_STAR:
    return elab_group(heap, ctx, syntax->binder, syntax->body, 1);
  case STERM_ZK_TY:
    return elab_group(heap, ctx, syntax->binder, syntax->body, 2);
  case STERM_PROVE:
    return elab_prove(heap, ctx, expected, syntax->key, syntax->witness, syntax->relation);
  case STERM_VERIFY:
    return elab_verify(heap, ctx, syntax->key, syntax->proof);
  case STERM_FHC_TY:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_term_elab(heap, ctx, value_none(), syntax->level), fhc_after_level);
  case STERM_ENC:
    return elab_cipher(heap, ctx, expected, syntax->key, syntax->value, false);
  case STERM_EVAL:
    return elab_cipher(heap, ctx, expected, syntax->function, syntax->value, true);
  case STERM_DEC:
    return elab_dec(heap, ctx, syntax->key, syntax->value);
  case STERM_MPC_TY:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_term_elab(heap, ctx, value_none(), syntax->parties), mpc_after_parties);
  case STERM_SHARE:
    return elab_share(heap, ctx, expected, syntax->value);
  case STERM_INPUT:
    return elab_input(heap, ctx, expected, syntax->party, syntax->value);
  case STERM_JOIN:
    return elab_join(heap, ctx, expected, syntax->parties, syntax->function, &syntax->items);
  case STERM_OPEN:
    return elab_open(heap, ctx, syntax->quantity, syntax->parties, syntax->proof, syntax->value);
  case STERM_LET:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_term_elab(heap, ctx, value_none(), syntax->ty), let_after_ty);
  case STERM_ANN:
    return bind_dispatch(heap, dispatch_frame(heap, ctx, expected, syntax),
                         elab_term_elab(heap, ctx, value_none(), syntax->ty), ann_after_ty);
  case STERM_CASE:
    return elab_case(heap, ctx, expected, syntax->scrut, syntax->motive, syntax->branches, false);
  case STERM_MATCH:
    return elab_case(heap, ctx, expected, syntax->scrut, syntax->motive, syntax->branches, true);
  }
  return elab_case(heap, ctx, expected, syntax->scrut, syntax->motive, syntax->branches, true);
}

/* TelescopeState (1227-1238): telescope is a Vec of TeleBinder pointers (Positivity.Telescope). */
typedef struct {
  Vec telescope;
  Ctx ctx;
} TelescopeState;

static Vec telescope_terms(const TelescopeState *state) { return state->telescope; }

static Ctx telescope_context(const TelescopeState *state) { return state->ctx; }

static Comp pure_telescope_state(Heap *heap, Vec telescope, Ctx ctx) {
  TelescopeState *state = heap_alloc(heap, sizeof *state);
  if (state == NULL) return comp_oom();
  state->telescope = telescope;
  state->ctx = ctx;
  return budget_pure(state);
}

/* Positivity.Binder{bound_q(bound), bound_name(bound), bound_type(bound)} */
static TeleBinder *tele_binder_of(Heap *heap, const ElabBound *bound) {
  TeleBinder *binder = heap_alloc(heap, sizeof *binder);
  if (binder == NULL) return NULL;
  binder->quantity = bound->quantity;
  binder->name = bound->name;
  binder->ty = bound->ty;
  return binder;
}

/* The Bend prepends each binder and reverses at Nil; pushing in order builds the same list, so the
 * accumulator is returned as is at Nil. */
typedef struct {
  const Vec *binders;
  size_t from;
  const STerm *syntax;
  Vec telescope;
} TelescopeFrame;

static Comp elab_telescope_from(Heap *heap, const Vec *binders, size_t from, Ctx ctx, Vec telescope);

static Comp elab_telescope_after_bound(Heap *heap, void *ctx, void *value) {
  TelescopeFrame *f = ctx;
  const ElabBound *bound = value;
  TeleBinder *binder = tele_binder_of(heap, bound);
  if (binder == NULL) return comp_oom();
  push(heap, &f->telescope, binder);
  return elab_telescope_from(heap, f->binders, f->from + 1, bound->context, f->telescope);
}

/* elab_telescope (1240-1248) */
static Comp elab_telescope_from(Heap *heap, const Vec *binders, size_t from, Ctx ctx, Vec telescope) {
  VecGet got = vec_get(binders, from);
  TelescopeFrame *f;
  if (got.status != VEC_GET_OK) return pure_telescope_state(heap, telescope, ctx);
  f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return comp_oom();
  f->binders = binders;
  f->from = from;
  f->syntax = NULL;
  f->telescope = telescope;
  return bind_with(heap, elab_binder(heap, ctx, *(const SBinder *)got.value), elab_telescope_after_bound, f);
}

static Comp elab_telescope(Heap *heap, const Vec *binders, Ctx ctx, Vec telescope) {
  return elab_telescope_from(heap, binders, 0, ctx, telescope);
}

/* Chain (1250-1266) */
typedef struct {
  Vec telescope;
  const STerm *body;
  Ctx ctx;
} Chain;

static Vec chain_telescope(const Chain *chain) { return chain->telescope; }

static const STerm *chain_body(const Chain *chain) { return chain->body; }

static Ctx chain_context(const Chain *chain) { return chain->ctx; }

static Comp pure_chain(Heap *heap, Vec telescope, const STerm *body, Ctx ctx) {
  Chain *chain = heap_alloc(heap, sizeof *chain);
  if (chain == NULL) return comp_oom();
  chain->telescope = telescope;
  chain->body = body;
  chain->ctx = ctx;
  return budget_pure(chain);
}

static Comp elab_chain(Heap *heap, const STerm *syntax, Ctx ctx, Vec telescope);

static Comp elab_chain_after_bound(Heap *heap, void *ctx, void *value) {
  TelescopeFrame *f = ctx;
  const ElabBound *bound = value;
  TeleBinder *binder = tele_binder_of(heap, bound);
  if (binder == NULL) return comp_oom();
  push(heap, &f->telescope, binder);
  return elab_chain(heap, f->syntax->body, bound->context, f->telescope);
}

/* elab_chain (1268-1276): SArrow extends the telescope, any other syntax ends the chain. */
static Comp elab_chain(Heap *heap, const STerm *syntax, Ctx ctx, Vec telescope) {
  TelescopeFrame *f;
  if (syntax->kind != STERM_ARROW) return pure_chain(heap, telescope, syntax, ctx);
  f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return comp_oom();
  f->binders = NULL;
  f->from = 0;
  f->syntax = syntax;
  f->telescope = telescope;
  return bind_with(heap, elab_binder(heap, ctx, syntax->binder), elab_chain_after_bound, f);
}

/* header_universe (1278-1283): the Comp value is the const LevelVar * */
static Comp header_universe(const Term *term) {
  if (term->kind == TERM_UNIV) return budget_pure((void *)term->level);
  return budget_failed(error_make(ERROR_UNIVERSE, str_lit("the header of a family ends in a universe")));
}

static Comp elab_univ_after_term(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  return header_universe(value);
}

/* elab_univ (1285-1288) */
static Comp elab_univ(Heap *heap, Ctx ctx, const STerm *syntax) {
  return bind_with(heap, elab_term_elab(heap, ctx, value_none(), syntax), elab_univ_after_term, NULL);
}

/* elab_fam_decl (1290-1297) */
typedef struct {
  SFam family; /* owned copy: the binders Vec must outlive a budget tick suspension */
  Str name;
  const STerm *ty;
  const TelescopeState *params;
  const Chain *indices;
} FamFrame;

static Comp elab_fam_decl_after_level(Heap *heap, void *ctx, void *value) {
  FamFrame *f = ctx;
  FamilyDecl *decl = heap_alloc(heap, sizeof *decl);
  if (decl == NULL) return comp_oom();
  decl->name = f->name;
  decl->params = telescope_terms(f->params);
  decl->indices = chain_telescope(f->indices);
  decl->level = value;
  return budget_pure(decl);
}

static Comp elab_fam_decl_after_indices(Heap *heap, void *ctx, void *value) {
  FamFrame *f = ctx;
  f->indices = value;
  return bind_with(heap, elab_univ(heap, chain_context(f->indices), chain_body(f->indices)),
                   elab_fam_decl_after_level, f);
}

static Comp elab_fam_decl_after_params(Heap *heap, void *ctx, void *value) {
  FamFrame *f = ctx;
  f->params = value;
  return bind_with(heap, elab_chain(heap, f->ty, telescope_context(f->params), empty(heap)),
                   elab_fam_decl_after_indices, f);
}

Comp elab_term_elab_fam_decl(Heap *heap, SFam family, Ctx ctx) {
  FamFrame *f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return comp_oom();
  f->family = family;
  f->name = f->family.name;
  f->ty = f->family.ty;
  f->params = NULL;
  f->indices = NULL;
  return bind_with(heap, elab_telescope(heap, &f->family.params, ctx, empty(heap)), elab_fam_decl_after_params, f);
}

/* diagram_bodies (1299-1304): legs is a Vec of Leg pointers */
static Vec diagram_bodies(Heap *heap, const Vec *legs) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < legs->len; i++) {
    VecGet got = vec_get(legs, i);
    if (got.status == VEC_GET_OK) push(heap, &out, (void *)((const Leg *)got.value)->body);
  }
  return out;
}

/* mu_params_of (1306-1311): the Comp value is a boxed Vec of Term pointers */
static Comp mu_params_of(Heap *heap, const Term *term) {
  if (term->kind == TERM_SEC) return pure_vec(heap, diagram_bodies(heap, &term->legs));
  return budget_failed(error_make(ERROR_MISMATCH, str_lit("the diagram is not a section of the declared width")));
}

/* Common.Pair<List<Term.T>, List<Term.T>> of mu_result */
typedef struct {
  Vec params;
  Vec indices;
} MuResult;

static Str another_type_text(Heap *heap, Str name) {
  return str_cat(heap->arena, str_cat(heap->arena, str_lit("a constructor of "), name),
                 str_lit(" ends at another type"));
}

static Comp matching_mu_result_after_params(Heap *heap, void *ctx, void *value) {
  MuResult *result = heap_alloc(heap, sizeof *result);
  if (result == NULL) return comp_oom();
  result->params = *(const Vec *)value;
  result->indices = *(const Vec *)ctx;
  return budget_pure(result);
}

/* matching_mu_result (1313-1320) */
static Comp matching_mu_result(Heap *heap, bool same, Str name, const Term *diagram, Vec indices) {
  Vec *boxed;
  if (!same) return budget_failed(error_make(ERROR_MISMATCH, another_type_text(heap, name)));
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = indices;
  return bind_with(heap, mu_params_of(heap, diagram), matching_mu_result_after_params, boxed);
}

/* mu_result (1322-1327): Term.Lan{Shape.SMu{family, indices}, diagram} */
static Comp mu_result(Heap *heap, const Term *term, Str name) {
  if (term->kind == TERM_LAN && term->shape.kind == SHAPE_MU)
    return matching_mu_result(heap, str_eq(name, term->shape.name), name, term->body, term->shape.args);
  return budget_failed(error_make(ERROR_MISMATCH, another_type_text(heap, name)));
}

/* elab_ctor_decl (1329-1336) */
typedef struct {
  Str ctor_name;
  Str name;
  const Chain *chain;
} CtorFrame;

static Comp elab_ctor_decl_after_result(Heap *heap, void *ctx, void *value) {
  CtorFrame *f = ctx;
  const MuResult *result = value;
  CtorDecl *decl = heap_alloc(heap, sizeof *decl);
  if (decl == NULL) return comp_oom();
  decl->name = f->ctor_name;
  decl->args = chain_telescope(f->chain);
  decl->res_params = result->params;
  decl->res_idx = result->indices;
  return budget_pure(decl);
}

static Comp elab_ctor_decl_after_term(Heap *heap, void *ctx, void *value) {
  CtorFrame *f = ctx;
  return bind_with(heap, mu_result(heap, value, f->name), elab_ctor_decl_after_result, f);
}

static Comp elab_ctor_decl_after_chain(Heap *heap, void *ctx, void *value) {
  CtorFrame *f = ctx;
  f->chain = value;
  return bind_with(heap, elab_term_elab(heap, chain_context(f->chain), value_none(), chain_body(f->chain)),
                   elab_ctor_decl_after_term, f);
}

static Comp elab_ctor_decl(Heap *heap, SFamCtor ctor, Ctx ctx, Str name) {
  CtorFrame *f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return comp_oom();
  f->ctor_name = ctor.name;
  f->name = name;
  f->chain = NULL;
  return bind_with(heap, elab_chain(heap, ctor.ty, ctx, empty(heap)), elab_ctor_decl_after_chain, f);
}

/* elab_ctor_decls (1338-1346): head <> tail */
typedef struct {
  const Vec *ctors;
  size_t from;
  Ctx ctx;
  Str name;
  const CtorDecl *head;
} CtorsFrame;

static Comp elab_ctor_decls_from(Heap *heap, const Vec *ctors, size_t from, Ctx ctx, Str name);

static Comp elab_ctor_decls_after_tail(Heap *heap, void *ctx, void *value) {
  CtorsFrame *f = ctx;
  const Vec *tail = value;
  return pure_vec(heap, cons(heap, (void *)f->head, tail));
}

static Comp elab_ctor_decls_after_head(Heap *heap, void *ctx, void *value) {
  CtorsFrame *f = ctx;
  f->head = value;
  return bind_with(heap, elab_ctor_decls_from(heap, f->ctors, f->from + 1, f->ctx, f->name),
                   elab_ctor_decls_after_tail, f);
}

static Comp elab_ctor_decls_from(Heap *heap, const Vec *ctors, size_t from, Ctx ctx, Str name) {
  VecGet got = vec_get(ctors, from);
  CtorsFrame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return comp_oom();
  f->ctors = ctors;
  f->from = from;
  f->ctx = ctx;
  f->name = name;
  f->head = NULL;
  return bind_with(heap, elab_ctor_decl(heap, *(const SFamCtor *)got.value, ctx, name), elab_ctor_decls_after_head,
                   f);
}

Comp elab_term_elab_ctor_decls(Heap *heap, const Vec *ctors, Ctx ctx, Str name) {
  return elab_ctor_decls_from(heap, ctors, 0, ctx, name);
}

/* elab_decl (1348-1365) */
typedef struct {
  Str name;
  Ctx ctx;
  const STerm *body;
  const Term *ty;
} DeclFrame;

static Comp decl_of(Heap *heap, Str name, DeclKind kind, const Term *ty, MaybeTermPtr body) {
  Decl *decl = heap_alloc(heap, sizeof *decl);
  if (decl == NULL) return comp_oom();
  decl->name = name;
  decl->kind = kind;
  decl->ty = ty;
  decl->body = body;
  return budget_pure(decl);
}

static Comp elab_decl_def_after_body(Heap *heap, void *ctx, void *value) {
  DeclFrame *f = ctx;
  MaybeTermPtr body;
  body.some = true;
  body.value = value;
  return decl_of(heap, f->name, DECL_DEFINITION, f->ty, body);
}

static Comp elab_decl_def_after_value(Heap *heap, void *ctx, void *value) {
  DeclFrame *f = ctx;
  return bind_with(heap, elab_term_elab(heap, f->ctx, value_some(value), f->body), elab_decl_def_after_body, f);
}

static Comp elab_decl_def_after_ty(Heap *heap, void *ctx, void *value) {
  DeclFrame *f = ctx;
  f->ty = value;
  return bind_with(heap, eval_in(heap, f->ctx, f->ty), elab_decl_def_after_value, f);
}

static Comp elab_decl_axiom_after_ty(Heap *heap, void *ctx, void *value) {
  DeclFrame *f = ctx;
  MaybeTermPtr body;
  body.some = false;
  body.value = NULL;
  return decl_of(heap, f->name, DECL_POSTULATE, value, body);
}

static DeclFrame *decl_frame(Heap *heap, const SDecl *decl, Ctx ctx) {
  DeclFrame *f = heap_alloc(heap, sizeof *f);
  if (f == NULL) return NULL;
  f->name = decl->name;
  f->ctx = ctx;
  f->body = decl->body;
  f->ty = NULL;
  return f;
}

static Comp prenex_needs_catalog(void) {
  return budget_failed(error_make(ERROR_MISMATCH, str_lit("a prenex declaration requires a program catalog")));
}

Comp elab_term_elab_decl(Heap *heap, const SDecl *decl, Ctx ctx) {
  DeclFrame *f;
  switch (decl->kind) {
  case SDECL_DEF:
    f = decl_frame(heap, decl, ctx);
    if (f == NULL) return comp_oom();
    return bind_with(heap, elab_term_elab(heap, ctx, value_none(), decl->ty), elab_decl_def_after_ty, f);
  case SDECL_AXIOM:
    f = decl_frame(heap, decl, ctx);
    if (f == NULL) return comp_oom();
    return bind_with(heap, elab_term_elab(heap, ctx, value_none(), decl->ty), elab_decl_axiom_after_ty, f);
  case SDECL_MU:
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a mu group declares no entry of its own")));
  case SDECL_REC:
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a recursive group is elaborated as a group")));
  case SDECL_POLY:
  case SDECL_POLY_MU:
  case SDECL_POLY_GROUP:
  case SDECL_POLY_COMPOSE:
  case SDECL_SPECIALIZE:
    return prenex_needs_catalog();
  }
  return prenex_needs_catalog();
}

/* ---- exports for elab.c (the statics above keep their Bend names) ---- */
Comp elab_term_eval_in(Heap *heap, Ctx ctx, const Term *term) { return eval_in(heap, ctx, term); }

Comp elab_term_elab_telescope(Heap *heap, const Vec *binders, Ctx ctx, Vec telescope) {
  return elab_telescope(heap, binders, ctx, telescope);
}

Ctx elab_term_telescope_context(const void *state) { return telescope_context(state); }

Comp elab_term_elab_ctor_decl(Heap *heap, SFamCtor ctor, Ctx ctx, Str name) {
  return elab_ctor_decl(heap, ctor, ctx, name);
}
