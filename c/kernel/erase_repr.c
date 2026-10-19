/* kernel/erase_repr.bend: the erasure context and the runtime representation (Repr) of a
 * type. Comp values: EraseCtx *, const Value *, Nat *, Vec * (Str, Value, MaybeRepr or Tid pointers,
 * or kernel bool boxes), Str *, Repr *, Tid *, MaybeRepr *, Pair * (first = EraseCtx *, second =
 * const Value *) and the kernel bool box. */
#include "kernel.h"

/* The values a do block carries between its binds. */
typedef struct {
  EraseCtx ec;
  Nat n;
  Nat index;
  Quantity q;
  Str name;
  const Value *dom;
  const Value *ty;
  Closure clo;
  Vec vec;
  Vec env;
  Big count;
  bool keep;
  Family family;
  Str text;
  MaybeRepr here;
} Frame;

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static Frame *frame_new(Heap *heap) { return heap_alloc(heap, sizeof(Frame)); }

static Comp bind_with(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return budget_bind(heap, first, next);
}

static Comp bind_frame(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), Frame *frame) {
  if (frame == NULL) return comp_oom();
  return bind_with(heap, first, fn, frame);
}

static Comp pure_bool(bool value) { return budget_pure(kernel_bool_box(value)); }

static Comp pure_vec(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = items;
  return budget_pure(boxed);
}

static Comp pure_nat(Heap *heap, Nat n) {
  Nat *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = n;
  return budget_pure(boxed);
}

static Str *box_str(Heap *heap, Str s) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = s;
  return boxed;
}

static Comp pure_str(Heap *heap, Str s) {
  Str *boxed = box_str(heap, s);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_repr(Heap *heap, Repr r) {
  Repr *boxed = eterm_repr_new(heap, r);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_tid(Heap *heap, Tid t) {
  Tid *boxed = eterm_tid_new(heap, t.name);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_maybe_repr(Heap *heap, MaybeRepr m) {
  MaybeRepr *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = m;
  return budget_pure(boxed);
}

static Comp pure_ectx(Heap *heap, EraseCtx ec) {
  EraseCtx *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ec;
  return budget_pure(boxed);
}

static Vec empty(Heap *heap) { return vec_new(heap->arena); }

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

/* Bend `x <> xs`: a fresh list with item first. */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec out = empty(heap);
  size_t i;
  push(heap, &out, item);
  for (i = 0; i < rest->len; i++) {
    VecGet got = vec_get(rest, i);
    if (got.status == VEC_GET_OK) push(heap, &out, got.value);
  }
  return out;
}

/* List.append */
static Vec append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < a->len; i++) {
    VecGet got = vec_get(a, i);
    if (got.status == VEC_GET_OK) push(heap, &out, got.value);
  }
  for (i = 0; i < b->len; i++) {
    VecGet got = vec_get(b, i);
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

/* Bend `a ++ b ++ ...` over fixed part arrays. */
static Str cats(Heap *heap, size_t count, const Str *parts) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

/* Nat.sub(n, 1n): saturating. */
static Nat pred(Heap *heap, Nat n) {
  NatSub sub = nat_sub(heap->arena, n, heap_nat_of_u64(heap, 1));
  switch (sub.status) {
  case NAT_SUB_OK:
    return sub.value;
  case NAT_SUB_UNDERFLOW:
    return nat_zero();
  case NAT_SUB_OOM:
    heap->oom = true;
    return nat_zero();
  }
  return nat_zero();
}

static Nat nat_of_len(Heap *heap, size_t len) { return heap_nat_of_u64(heap, (uint64_t)len); }

/* B.lift over a VecResult: the DONE payload becomes a heap allocated Vec. */
static Comp lift_vec(Heap *heap, VecResult result) {
  switch (result.kind) {
  case RESULT_DONE:
    return pure_vec(heap, result.value);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return comp_oom();
}

/* ---- Slot and Ctx ---- */

void *erase_repr_slot_box(Slot slot) {
  static Slot drop = SLOT_DROP;
  static Slot keep = SLOT_KEEP;
  static Slot extra = SLOT_EXTRA;
  switch (slot) {
  case SLOT_DROP:
    return &drop;
  case SLOT_KEEP:
    return &keep;
  case SLOT_EXTRA:
    return &extra;
  }
  return &drop;
}

Slot erase_repr_slot_unbox(const void *boxed) {
  const Slot *slot = boxed;
  if (slot == NULL) return SLOT_DROP;
  return *slot;
}

EraseCtx erase_repr_ctx(Ctx check, Vec slots, Str self) {
  EraseCtx ec;
  ec.check = check;
  ec.slots = slots;
  ec.self = self;
  return ec;
}

Ctx erase_repr_checker(EraseCtx ec) { return ec.check; }

Vec erase_repr_slots(EraseCtx ec) { return ec.slots; }

Str erase_repr_self(EraseCtx ec) { return ec.self; }

Globals erase_repr_globals(EraseCtx ec) { return context_globals_of(erase_repr_checker(ec)); }

Nat erase_repr_size(EraseCtx ec) { return context_size_of(erase_repr_checker(ec)); }

Vec erase_repr_env(EraseCtx ec) { return context_env_of(erase_repr_checker(ec)); }

EraseCtx erase_repr_bind(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *ty, Slot slot) {
  return erase_repr_ctx(context_bind(heap, name, q, ty, ec.check), cons(heap, erase_repr_slot_box(slot), &ec.slots),
                        ec.self);
}

EraseCtx erase_repr_define(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *ty, const Value *value,
                           Slot slot) {
  return erase_repr_ctx(context_define(heap, name, q, ty, value, ec.check),
                        cons(heap, erase_repr_slot_box(slot), &ec.slots), ec.self);
}

EraseCtx erase_repr_extra(Heap *heap, EraseCtx ec) {
  return erase_repr_ctx(ec.check, cons(heap, erase_repr_slot_box(SLOT_EXTRA), &ec.slots), ec.self);
}

EraseCtx erase_repr_replace_slots(EraseCtx ec, Vec slots) { return erase_repr_ctx(ec.check, slots, ec.self); }

EraseCtx erase_repr_fresh(Heap *heap, EraseCtx ec) {
  return erase_repr_ctx(context_make(heap, context_globals_of(ec.check), budget_unlimited(), nat_zero()), empty(heap),
                        ec.self);
}

Slot erase_repr_keep_slot(bool keep) { return keep ? SLOT_KEEP : SLOT_DROP; }

/* ---- the checker calls ---- */

Comp erase_repr_eval(Heap *heap, EraseCtx ec, const Term *term) {
  return check_engine_eval(heap, erase_repr_checker(ec), term);
}

Comp erase_repr_eval_env(Heap *heap, EraseCtx ec, Vec env, const Term *term) {
  return budget_lift(eval_eval(heap, erase_repr_globals(ec), env, term));
}

Comp erase_repr_whnf(Heap *heap, EraseCtx ec, const Value *value) {
  return check_engine_whnf(heap, erase_repr_checker(ec), value);
}

Comp erase_repr_open(Heap *heap, EraseCtx ec, Closure clo, Vec args) {
  return check_engine_open(heap, erase_repr_checker(ec), clo, args);
}

static Comp infer_after_result(Heap *heap, void *ctx, void *value) {
  const Inferred *result = value;
  (void)heap;
  (void)ctx;
  if (result == NULL) return comp_oom();
  return budget_pure((void *)check_engine_inferred_ty(*result));
}

Comp erase_repr_infer(Heap *heap, EraseCtx ec, const Term *term) {
  return bind_with(heap, check_engine_infer_uses(heap, erase_repr_checker(ec), QUANTITY_ONE, term), infer_after_result,
                   NULL);
}

static Comp proof_free_after_level(Heap *heap, void *ctx, void *value) {
  const LevelVar *level = value;
  (void)ctx;
  return pure_bool(!level_equal(heap, level, level_zero()));
}

static Comp proof_free_after_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Term *term = value;
  return bind_frame(heap, check_engine_infer_univ(heap, erase_repr_checker(f->ec), term), proof_free_after_level, f);
}

Comp erase_repr_proof_free(Heap *heap, EraseCtx ec, const Value *value) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, check_engine_quote(heap, erase_repr_checker(ec), value), proof_free_after_term, f);
}

Comp erase_repr_runtime_collection(Heap *heap, bool empty_coll, EraseCtx ec, const Value *value) {
  if (empty_coll) return pure_bool(false);
  return erase_repr_proof_free(heap, ec, value);
}

/* VLan{SColl{n}, d, l} and VRan{SColl{n}, d, l} rebuild the same value in Bend; the host passes it on. */
static Comp runtime_former(Heap *heap, EraseCtx ec, const Value *value) {
  if (value->shape.kind == SHAPE_COLL) {
    return erase_repr_runtime_collection(heap, big_equal(value->shape.count, big_zero()), ec, value);
  }
  return erase_repr_proof_free(heap, ec, value);
}

Comp erase_repr_runtime_node(Heap *heap, EraseCtx ec, const Value *value) {
  switch (value->kind) {
  case VALUE_UNIV:
    return pure_bool(false);
  case VALUE_LAN:
    return runtime_former(heap, ec, value);
  case VALUE_RAN:
    return runtime_former(heap, ec, value);
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    return erase_repr_proof_free(heap, ec, value);
  }
  return erase_repr_proof_free(heap, ec, value);
}

static Comp runtime_ty_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_repr_runtime_node(heap, f->ec, value);
}

Comp erase_repr_runtime_ty(Heap *heap, EraseCtx ec, const Value *ty) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), runtime_ty_after_value, f);
}

Comp erase_repr_point_runtime(Heap *heap, EraseCtx ec, Quantity q, const Value *dom) {
  switch (q) {
  case QUANTITY_ZERO:
    return pure_bool(false);
  case QUANTITY_ONE:
  case QUANTITY_MANY:
    return erase_repr_runtime_ty(heap, ec, dom);
  }
  return erase_repr_runtime_ty(heap, ec, dom);
}

/* ---- the tids ---- */

Repr erase_repr_any_repr(void) { return eterm_runion(eterm_tid(str_lit("any"))); }

Tid erase_repr_mu_tid(Heap *heap, Str name) {
  Str parts[3];
  parts[0] = str_lit("mu<");
  parts[1] = name;
  parts[2] = str_lit(">");
  return eterm_tid(cats(heap, 3, parts));
}

Str erase_repr_repr_commas(Heap *heap, const Vec *rs) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < rs->len; i++) {
    VecGet got = vec_get(rs, i);
    const Repr *r = got.status == VEC_GET_OK ? got.value : NULL;
    Str parts[3];
    if (r == NULL) continue;
    parts[0] = out;
    parts[1] = str_lit(",");
    parts[2] = eterm_print_repr(heap, *r);
    out = cats(heap, 3, parts);
  }
  return out;
}

Tid erase_repr_mu_leg_tid(Heap *heap, Str name, Nat index, const Vec *rs) {
  Str parts[6];
  parts[0] = str_lit("leg<");
  parts[1] = eterm_tid_text(erase_repr_mu_tid(heap, name));
  parts[2] = str_lit(",");
  parts[3] = heap_nat_show(heap, index);
  parts[4] = erase_repr_repr_commas(heap, rs);
  parts[5] = str_lit(">");
  return eterm_tid(cats(heap, 6, parts));
}

Str erase_repr_join(Heap *heap, const Vec *texts, Str sep) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < texts->len; i++) {
    VecGet got = vec_get(texts, i);
    const Str *x = got.status == VEC_GET_OK ? got.value : NULL;
    Str parts[3];
    if (x == NULL) continue;
    parts[0] = out;
    parts[1] = i == 0 ? str_lit("") : sep;
    parts[2] = *x;
    out = cats(heap, 3, parts);
  }
  return out;
}

/* ---- under_point and the arity ---- */

static Comp under_point_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *cod = value;
  EraseCtx *bound = heap_alloc(heap, sizeof *bound);
  Pair *pair = heap_alloc(heap, sizeof *pair);
  if (bound == NULL || pair == NULL) return comp_oom();
  *bound = erase_repr_bind(heap, f->ec, f->name, f->q, f->dom, SLOT_DROP);
  pair->first = bound;
  pair->second = (void *)cod;
  return budget_pure(pair);
}

Comp erase_repr_under_point(Heap *heap, EraseCtx ec, Str name, Quantity q, const Value *dom, Closure clo) {
  Frame *f = frame_new(heap);
  Vec args = empty(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->name = name;
  f->q = q;
  f->dom = dom;
  push(heap, &args, (void *)value_var(heap, erase_repr_size(ec)));
  return bind_frame(heap, erase_repr_open(heap, ec, clo, args), under_point_after_cod, f);
}

static Comp repr_text_after_r(Heap *heap, void *ctx, void *value) {
  const Repr *r = value;
  Vec out = empty(heap);
  Str *text;
  (void)ctx;
  if (r == NULL) return comp_oom();
  text = box_str(heap, eterm_print_repr(heap, *r));
  if (text == NULL) return comp_oom();
  push(heap, &out, text);
  return pure_vec(heap, out);
}

Comp erase_repr_repr_text(Heap *heap, EraseCtx ec, bool keep, const Value *ty) {
  if (!keep) return pure_vec(heap, empty(heap));
  return bind_with(heap, erase_repr_repr_of(heap, ec, ty), repr_text_after_r, NULL);
}

Comp erase_repr_arity_rest(Heap *heap, EraseCtx ec, Nat n, const Value *value) {
  switch (value->kind) {
  case VALUE_RAN:
    return erase_repr_arity_of(heap, ec, n, value->shape, value->diagram);
  case VALUE_UNIV:
  case VALUE_LAN:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    return pure_nat(heap, n);
  }
  return pure_nat(heap, n);
}

static Comp arity_open_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_repr_arity_rest(heap, f->ec, f->keep ? heap_nat_succ(heap, f->n) : f->n, value);
}

Comp erase_repr_arity_open(Heap *heap, const Pair *pair, Nat n, bool keep) {
  Frame *f = frame_new(heap);
  const EraseCtx *ec = pair->first;
  const Value *cod = pair->second;
  if (f == NULL || ec == NULL) return comp_oom();
  f->ec = *ec;
  f->n = n;
  f->keep = keep;
  return bind_frame(heap, erase_repr_whnf(heap, *ec, cod), arity_open_after_value, f);
}

static Comp arity_point_after_pair(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Pair *pair = value;
  if (pair == NULL) return comp_oom();
  return erase_repr_arity_open(heap, pair, f->n, f->keep);
}

static Comp arity_point_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_under_point(heap, f->ec, f->name, f->q, f->dom, f->clo), arity_point_after_pair, f);
}

Comp erase_repr_arity_point(Heap *heap, EraseCtx ec, Nat n, Quantity q, Str name, const Value *dom, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->n = n;
  f->q = q;
  f->name = name;
  f->dom = dom;
  f->clo = clo;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, q, dom), arity_point_after_keep, f);
}

Comp erase_repr_arity_of(Heap *heap, EraseCtx ec, Nat n, Shape shape, Closure clo) {
  switch (shape.kind) {
  case SHAPE_PI:
  case SHAPE_ZK:
    return erase_repr_arity_point(heap, ec, n, shape.quantity, shape.name, shape.domain, clo);
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return pure_nat(heap, n);
  }
  return pure_nat(heap, n);
}

/* ---- the pair texts ---- */

static Comp pair_second_after_second(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *second = value;
  if (second == NULL) return comp_oom();
  return pure_vec(heap, append(heap, &f->vec, second));
}

static Comp pair_second_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_repr_text(heap, f->ec, kernel_bool_unbox(value), f->dom), pair_second_after_second,
                    f);
}

Comp erase_repr_pair_second(Heap *heap, const Pair *pair, Vec first) {
  Frame *f = frame_new(heap);
  const EraseCtx *ec = pair->first;
  const Value *cod = pair->second;
  if (f == NULL || ec == NULL) return comp_oom();
  f->ec = *ec;
  f->dom = cod;
  f->vec = first;
  return bind_frame(heap, erase_repr_runtime_ty(heap, *ec, cod), pair_second_after_keep, f);
}

static Comp pair_texts_after_pair(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Pair *pair = value;
  if (pair == NULL) return comp_oom();
  return erase_repr_pair_second(heap, pair, f->vec);
}

static Comp pair_texts_after_first(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *first = value;
  if (first == NULL) return comp_oom();
  f->vec = *first;
  return bind_frame(heap, erase_repr_under_point(heap, f->ec, f->name, f->q, f->dom, f->clo), pair_texts_after_pair, f);
}

static Comp pair_texts_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_repr_text(heap, f->ec, kernel_bool_unbox(value), f->dom), pair_texts_after_first, f);
}

Comp erase_repr_pair_texts(Heap *heap, EraseCtx ec, Quantity q, Str name, const Value *dom, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->q = q;
  f->name = name;
  f->dom = dom;
  f->clo = clo;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, q, dom), pair_texts_after_keep, f);
}

/* ---- the leg types and flags ---- */

static Comp leg_types_loop_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  if (more == NULL) return comp_oom();
  return pure_vec(heap, cons(heap, (void *)f->ty, more));
}

static Comp leg_types_loop_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_leg_types_loop(heap, pred(heap, f->n), heap_nat_succ(heap, f->index), f->ec, f->vec),
                    leg_types_loop_after_more, f);
}

Comp erase_repr_leg_types_loop(Heap *heap, Nat n, Nat index, EraseCtx ec, Vec legs) {
  Frame *f;
  if (nat_is_zero(n)) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->n = n;
  f->index = index;
  f->ec = ec;
  f->vec = legs;
  return bind_frame(heap,
                    budget_lift(rules_coll_leg_ty(heap, erase_repr_checker(ec), &f->vec, big_of_nat(heap, index))),
                    leg_types_loop_after_ty, f);
}

Comp erase_repr_bounded_leg_types(Heap *heap, MaybeNat count, EraseCtx ec, Vec legs) {
  if (count.some) return erase_repr_leg_types_loop(heap, count.value, nat_zero(), ec, legs);
  /* Visit at most the actual list plus its first missing leg, preserving Wrong_leg. */
  return erase_repr_leg_types_loop(heap, heap_nat_succ(heap, nat_of_len(heap, legs.len)), nat_zero(), ec, legs);
}

static Comp leg_types_after_legs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *legs = value;
  if (legs == NULL) return comp_oom();
  return erase_repr_bounded_leg_types(heap, big_to_nat_bounded(heap, f->count, nat_of_len(heap, legs->len)), f->ec,
                                      *legs);
}

Comp erase_repr_leg_types(Heap *heap, EraseCtx ec, Big count, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->count = count;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, erase_repr_checker(ec), clo)), leg_types_after_legs,
                    f);
}

static Comp flags_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  if (more == NULL) return comp_oom();
  return pure_vec(heap, cons(heap, kernel_bool_box(f->keep), more));
}

static Comp flags_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_flags(heap, list_tail(f->vec), f->ec), flags_after_more, f);
}

Comp erase_repr_flags(Heap *heap, Vec types, EraseCtx ec) {
  VecGet got = vec_get(&types, 0);
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = types;
  f->ec = ec;
  return bind_frame(heap, erase_repr_runtime_ty(heap, ec, got.value), flags_after_keep, f);
}

static Comp leg_flags_after_tys(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *tys = value;
  if (tys == NULL) return comp_oom();
  return erase_repr_flags(heap, *tys, f->ec);
}

Comp erase_repr_leg_flags(Heap *heap, EraseCtx ec, Big count, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_leg_types(heap, ec, count, clo), leg_flags_after_tys, f);
}

/* ---- the sum and tuple texts ---- */

static Comp sum_text_after_r(Heap *heap, void *ctx, void *value) {
  const Repr *r = value;
  (void)ctx;
  if (r == NULL) return comp_oom();
  return pure_str(heap, eterm_print_repr(heap, *r));
}

Comp erase_repr_sum_text(Heap *heap, EraseCtx ec, const Value *ty, bool keep) {
  if (!keep) return pure_str(heap, str_lit("unit"));
  return bind_with(heap, erase_repr_repr_of(heap, ec, ty), sum_text_after_r, NULL);
}

static Comp sum_texts_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  Str *here;
  if (more == NULL) return comp_oom();
  here = box_str(heap, f->text);
  if (here == NULL) return comp_oom();
  return pure_vec(heap, cons(heap, here, more));
}

static Comp sum_texts_after_here(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Str *here = value;
  if (here == NULL) return comp_oom();
  f->text = *here;
  return bind_frame(heap, erase_repr_sum_texts(heap, list_tail(f->vec), f->ec), sum_texts_after_more, f);
}

static Comp sum_texts_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_sum_text(heap, f->ec, f->ty, kernel_bool_unbox(value)), sum_texts_after_here, f);
}

Comp erase_repr_sum_texts(Heap *heap, Vec types, EraseCtx ec) {
  VecGet got = vec_get(&types, 0);
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = types;
  f->ec = ec;
  f->ty = got.value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, ec, got.value), sum_texts_after_keep, f);
}

static Comp tuple_texts_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  if (more == NULL) return comp_oom();
  return pure_vec(heap, append(heap, &f->env, more));
}

static Comp tuple_texts_after_here(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *here = value;
  if (here == NULL) return comp_oom();
  f->env = *here;
  return bind_frame(heap, erase_repr_tuple_texts(heap, list_tail(f->vec), f->ec), tuple_texts_after_more, f);
}

static Comp tuple_texts_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_repr_text(heap, f->ec, kernel_bool_unbox(value), f->ty), tuple_texts_after_here, f);
}

Comp erase_repr_tuple_texts(Heap *heap, Vec types, EraseCtx ec) {
  VecGet got = vec_get(&types, 0);
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = types;
  f->ec = ec;
  f->ty = got.value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, ec, got.value), tuple_texts_after_keep, f);
}

/* ---- the collection reprs ---- */

static Repr joined_tid_repr(Heap *heap, Str open, const Vec *texts, Str sep, bool is_struct) {
  Str parts[3];
  parts[0] = open;
  parts[1] = erase_repr_join(heap, texts, sep);
  parts[2] = str_lit(">");
  if (is_struct) return eterm_rstruct(eterm_tid(cats(heap, 3, parts)));
  return eterm_runion(eterm_tid(cats(heap, 3, parts)));
}

static Comp ran_collection_after_texts(Heap *heap, void *ctx, void *value) {
  const Vec *texts = value;
  (void)ctx;
  if (texts == NULL) return comp_oom();
  return pure_repr(heap, joined_tid_repr(heap, str_lit("tuple<"), texts, str_lit(","), true));
}

static Comp ran_collection_after_types(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *types = value;
  if (types == NULL) return comp_oom();
  return bind_frame(heap, erase_repr_tuple_texts(heap, *types, f->ec), ran_collection_after_texts, f);
}

Comp erase_repr_ran_collection(Heap *heap, bool empty_coll, EraseCtx ec, Big count, Closure clo) {
  Frame *f;
  if (empty_coll) return pure_repr(heap, erase_repr_any_repr());
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_leg_types(heap, ec, count, clo), ran_collection_after_types, f);
}

static Comp lan_collection_after_texts(Heap *heap, void *ctx, void *value) {
  const Vec *texts = value;
  (void)ctx;
  if (texts == NULL) return comp_oom();
  return pure_repr(heap, joined_tid_repr(heap, str_lit("sum<"), texts, str_lit("|"), false));
}

static Comp lan_collection_after_types(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *types = value;
  if (types == NULL) return comp_oom();
  return bind_frame(heap, erase_repr_sum_texts(heap, *types, f->ec), lan_collection_after_texts, f);
}

Comp erase_repr_lan_collection(Heap *heap, bool empty_coll, EraseCtx ec, Big count, Closure clo) {
  Frame *f;
  if (empty_coll) return pure_repr(heap, erase_repr_any_repr());
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_leg_types(heap, ec, count, clo), lan_collection_after_types, f);
}

static Comp ran_repr_after_count(Heap *heap, void *ctx, void *value) {
  const Nat *count = value;
  Str parts[3];
  (void)ctx;
  if (count == NULL) return comp_oom();
  parts[0] = str_lit("fn<");
  parts[1] = heap_nat_show(heap, *count);
  parts[2] = str_lit(">");
  return pure_repr(heap, eterm_rfunc(eterm_tid(cats(heap, 3, parts))));
}

Comp erase_repr_ran_repr(Heap *heap, EraseCtx ec, Shape shape, Closure clo) {
  switch (shape.kind) {
  case SHAPE_PI:
  case SHAPE_ZK:
    return bind_with(heap, erase_repr_arity_of(heap, ec, nat_zero(), shape, clo), ran_repr_after_count, NULL);
  case SHAPE_COLL:
    return erase_repr_ran_collection(heap, big_equal(shape.count, big_zero()), ec, shape.count, clo);
  case SHAPE_ARG:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return pure_repr(heap, erase_repr_any_repr());
  }
  return pure_repr(heap, erase_repr_any_repr());
}

static Comp lan_repr_after_texts(Heap *heap, void *ctx, void *value) {
  const Vec *texts = value;
  (void)ctx;
  if (texts == NULL) return comp_oom();
  return pure_repr(heap, joined_tid_repr(heap, str_lit("pair<"), texts, str_lit(","), true));
}

Comp erase_repr_lan_repr(Heap *heap, EraseCtx ec, Shape shape, Closure clo) {
  switch (shape.kind) {
  case SHAPE_MU:
    return pure_repr(heap, eterm_runion(erase_repr_mu_tid(heap, shape.name)));
  case SHAPE_PI:
    return bind_with(heap, erase_repr_pair_texts(heap, ec, shape.quantity, shape.name, shape.domain, clo),
                     lan_repr_after_texts, NULL);
  case SHAPE_COLL:
    return erase_repr_lan_collection(heap, big_equal(shape.count, big_zero()), ec, shape.count, clo);
  case SHAPE_ARG:
  case SHAPE_PAR:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return pure_repr(heap, erase_repr_any_repr());
  }
  return pure_repr(heap, erase_repr_any_repr());
}

static Comp repr_node_neutral(Heap *heap, const Value *value) {
  if (value->head.kind == HEAD_GLOBAL && value->spine.len == 0) {
    if (str_eq(value->head.name, str_lit("Nat"))) return pure_repr(heap, eterm_runion(eterm_tid(str_lit("nat"))));
    return pure_repr(heap, erase_repr_any_repr());
  }
  return pure_repr(heap, erase_repr_any_repr());
}

Comp erase_repr_repr_node(Heap *heap, EraseCtx ec, const Value *value) {
  switch (value->kind) {
  case VALUE_NEUTRAL:
    return repr_node_neutral(heap, value);
  case VALUE_RAN:
    return erase_repr_ran_repr(heap, ec, value->shape, value->diagram);
  case VALUE_LAN:
    return erase_repr_lan_repr(heap, ec, value->shape, value->diagram);
  case VALUE_UNIV:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
    return pure_repr(heap, erase_repr_any_repr());
  }
  return pure_repr(heap, erase_repr_any_repr());
}

static Comp repr_of_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_repr_repr_node(heap, f->ec, value);
}

Comp erase_repr_repr_of(Heap *heap, EraseCtx ec, const Value *value) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_whnf(heap, ec, value), repr_of_after_value, f);
}

Comp erase_repr_repr_tid(Heap *heap, Repr repr) {
  switch (repr.kind) {
  case REPR_I31:
    return budget_failed(
        error_make(ERROR_MISMATCH, str_lit("a structured value stands at a type whose repr is i31")));
  case REPR_STRUCT:
  case REPR_UNION:
  case REPR_FUNC:
  case REPR_THUNK:
    return pure_tid(heap, repr.tid);
  }
  return pure_tid(heap, repr.tid);
}

static Comp tid_of_after_repr(Heap *heap, void *ctx, void *value) {
  const Repr *repr = value;
  (void)ctx;
  if (repr == NULL) return comp_oom();
  return erase_repr_repr_tid(heap, *repr);
}

Comp erase_repr_tid_of(Heap *heap, EraseCtx ec, const Value *value) {
  return bind_with(heap, erase_repr_repr_of(heap, ec, value), tid_of_after_repr, NULL);
}

static Comp optional_repr_after_r(Heap *heap, void *ctx, void *value) {
  const Repr *r = value;
  MaybeRepr out;
  (void)ctx;
  if (r == NULL) return comp_oom();
  out.some = true;
  out.value = *r;
  return pure_maybe_repr(heap, out);
}

Comp erase_repr_optional_repr(Heap *heap, EraseCtx ec, bool keep, const Value *ty) {
  MaybeRepr none;
  if (keep) return bind_with(heap, erase_repr_repr_of(heap, ec, ty), optional_repr_after_r, NULL);
  none.some = false;
  none.value = eterm_ri31();
  return pure_maybe_repr(heap, none);
}

/* ---- the mu layouts ---- */

static Comp mu_leg_reprs_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  MaybeRepr *here;
  if (more == NULL) return comp_oom();
  here = heap_alloc(heap, sizeof *here);
  if (here == NULL) return comp_oom();
  *here = f->here;
  return pure_vec(heap, cons(heap, here, more));
}

static Comp mu_leg_reprs_after_here(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const MaybeRepr *here = value;
  if (here == NULL) return comp_oom();
  f->here = *here;
  return bind_frame(heap,
                    erase_repr_mu_leg_reprs(heap, list_tail(f->vec),
                                            erase_repr_bind(heap, f->ec, f->name, f->q, f->ty, SLOT_DROP),
                                            cons(heap, (void *)value_var(heap, erase_repr_size(f->ec)), &f->env)),
                    mu_leg_reprs_after_more, f);
}

static Comp mu_leg_reprs_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_optional_repr(heap, f->ec, kernel_bool_unbox(value), f->ty), mu_leg_reprs_after_here,
                    f);
}

static Comp mu_leg_reprs_after_tyv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_point_runtime(heap, f->ec, f->q, f->ty), mu_leg_reprs_after_keep, f);
}

Comp erase_repr_mu_leg_reprs(Heap *heap, Vec tele, EraseCtx ec, Vec env) {
  VecGet got = vec_get(&tele, 0);
  const TeleBinder *binder;
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  binder = got.value;
  f = frame_new(heap);
  if (f == NULL || binder == NULL) return comp_oom();
  f->vec = tele;
  f->ec = ec;
  f->env = env;
  f->q = binder->quantity;
  f->name = binder->name;
  return bind_frame(heap, erase_repr_eval_env(heap, ec, env, binder->ty), mu_leg_reprs_after_tyv, f);
}

static Comp mu_parameters_after_tyv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *tyv = value;
  return erase_repr_mu_parameters(heap, list_tail(f->vec), erase_repr_bind(heap, f->ec, f->name, f->q, tyv, SLOT_DROP));
}

Comp erase_repr_mu_parameters(Heap *heap, Vec tele, EraseCtx ec) {
  VecGet got = vec_get(&tele, 0);
  const TeleBinder *binder;
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_ectx(heap, ec);
  binder = got.value;
  f = frame_new(heap);
  if (f == NULL || binder == NULL) return comp_oom();
  f->vec = tele;
  f->ec = ec;
  f->q = binder->quantity;
  f->name = binder->name;
  return bind_frame(heap, erase_repr_eval(heap, ec, binder->ty), mu_parameters_after_tyv, f);
}

static Comp mu_layout_after_params(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseCtx *params = value;
  if (params == NULL) return comp_oom();
  return erase_repr_mu_leg_reprs(heap, f->vec, *params, erase_repr_env(*params));
}

Comp erase_repr_mu_layout(Heap *heap, EraseCtx ec, Family family, Ctor ctor) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = ctor.args;
  return bind_frame(heap, erase_repr_mu_parameters(heap, family.params, erase_repr_fresh(heap, ec)),
                    mu_layout_after_params, f);
}

Vec erase_repr_kept_reprs(Heap *heap, const Vec *rs) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < rs->len; i++) {
    VecGet got = vec_get(rs, i);
    const MaybeRepr *r = got.status == VEC_GET_OK ? got.value : NULL;
    Repr *kept;
    if (r == NULL || !r->some) continue;
    kept = eterm_repr_new(heap, r->value);
    if (kept == NULL) return out;
    push(heap, &out, kept);
  }
  return out;
}

Comp erase_repr_ctor_names(Heap *heap, Family family, Str name) {
  Str parts[3];
  switch (family.status.kind) {
  case CTOR_STATUS_COMPLETE:
    return pure_vec(heap, family.status.names);
  case CTOR_STATUS_PROVISIONAL:
    parts[0] = str_lit("the family ");
    parts[1] = name;
    parts[2] = str_lit(" is still under declaration");
    return budget_failed(error_make(ERROR_UNBOUND, cats(heap, 3, parts)));
  case CTOR_STATUS_BUILTIN:
    parts[0] = str_lit("the family ");
    parts[1] = name;
    parts[2] = str_lit(" is a kernel family and has no branches");
    return budget_failed(error_make(ERROR_MISMATCH, cats(heap, 3, parts)));
  }
  return pure_vec(heap, family.status.names);
}

static Comp group_tids_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  Vec kept;
  Tid *tid;
  if (more == NULL) return comp_oom();
  kept = erase_repr_kept_reprs(heap, &f->env);
  tid = eterm_tid_new(heap, erase_repr_mu_leg_tid(heap, f->name, f->index, &kept).name);
  if (tid == NULL) return comp_oom();
  return pure_vec(heap, cons(heap, tid, more));
}

static Comp group_tids_after_rs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *rs = value;
  if (rs == NULL) return comp_oom();
  f->env = *rs;
  return bind_frame(heap,
                    erase_repr_group_tids(heap, list_tail(f->vec), heap_nat_succ(heap, f->index), f->ec, f->name, f->family),
                    group_tids_after_more, f);
}

static Comp group_tids_after_ct(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctor *ct = value;
  if (ct == NULL) return comp_oom();
  return bind_frame(heap, erase_repr_mu_layout(heap, f->ec, f->family, *ct), group_tids_after_rs, f);
}

Comp erase_repr_group_tids(Heap *heap, Vec names, Nat index, EraseCtx ec, Str name, Family family) {
  VecGet got = vec_get(&names, 0);
  const Str *ctor;
  MaybeCtor found;
  Ctor *boxed = NULL;
  Str parts[3];
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  ctor = got.value;
  f = frame_new(heap);
  if (f == NULL || ctor == NULL) return comp_oom();
  f->vec = names;
  f->index = index;
  f->ec = ec;
  f->name = name;
  f->family = family;
  found = positivity_ctor_of(*ctor, family);
  if (found.some) {
    boxed = positivity_ctor_new(heap, found.value);
    if (boxed == NULL) return comp_oom();
  }
  parts[0] = *ctor;
  parts[1] = str_lit(" is not a constructor of ");
  parts[2] = name;
  return bind_frame(heap, check_engine_optional(found.some, boxed, error_make(ERROR_UNBOUND, cats(heap, 3, parts))),
                    group_tids_after_ct, f);
}

static Comp mu_group_tids_after_names(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *names = value;
  if (names == NULL) return comp_oom();
  return erase_repr_group_tids(heap, *names, nat_zero(), f->ec, f->name, f->family);
}

Comp erase_repr_mu_group_tids(Heap *heap, EraseCtx ec, Str name, Family family) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->name = name;
  f->family = family;
  return bind_frame(heap, erase_repr_ctor_names(heap, family, name), mu_group_tids_after_names, f);
}
