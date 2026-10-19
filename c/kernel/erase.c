/* kernel/erase.bend: the erasure of checked terms to kernel terms. Quarters 1-4b of 4 = Bend lines
 * 1-1049 (the types Entry .. Signature, lookup .. tuple_jobs, tuple_finish .. ctor_indices with the law
 * bodies signature_value, remaining_signature and chain, mu_tag .. eval_terms with the law body app_jobs, mu_branch_body .. term with the law body term, tids_repr .. dedup_tids, def_finish .. program, print_decls .. print);
 * Comp values: EraseOut *, EraseOuts *, EraseChain *, EraseSignature *, const Value *, const Term *,
 * Vec * (Term, Ktm or Repr pointers), kernel bool boxes and kernel_unit. */
#include "kernel.h"

/* The values a do block carries between its binds. */
typedef struct {
  EraseCtx ec;
  EraseAcc ac;
  Nat ix;
  Vec slots;
  Vec jobs;
  Vec legs;
  Vec terms;
  EraseOut one;
  EraseOuts outs;
  const Value *ty;
  const Term *t;
  const Term *level;
  const Term *first;
  const Term *second;
  const Leg *leg;
  Str name;
  Str message;
  /* Quarter 2. */
  Vec params;
  Vec here;
  Vec caps;
  Vec outer;
  Vec reprs;
  Vec args;
  Vec tys;
  Vec env;
  Vec tele;
  Vec layout;
  Vec fields;
  Quantity q;
  const Value *dom;
  const Value *cod;
  Closure clo;
  Shape shape;
  Big count;
  Big raw_index;
  Nat index;
  bool keep;
  bool keep2;
  const Term *point;
  const Term *fibre;
  const Repr *repr;
  Fid fid;
  Addr addr;
  /* Quarter 3. */
  Str ctor;
  Str name2;
  Family family;
  Ctor ct;
  Vec penv;
  Vec rest;
  Vec flags;
  Vec binders;
  Vec values;
  const ElimData *data;
  const Value *sty;
  const Value *lty;
  const Value *hty;
  const Value *val;
  const Term *head;
  const Term *body;
  const Term *value_t;
  const Term *arg;
  bool tail;
  Nat arity;
  EraseCtx next;
  EraseCtx ec1;
  EraseCtx ec2;
  EraseOut two;
  EraseBranchOut bout;
  EraseBranches brs;
  /* Quarter 4a. */
  Motive motive;
  Vec idx;
  Vec names;
  Vec elim_branches;
  /* Quarter 4b. */
  Globals globals;
  Entry entry;
  const EraseEntry *erased;
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

static Comp mismatch(const char *message) {
  Str text = str_lit(message);
  return budget_failed(error_make(ERROR_MISMATCH, text));
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

static Str cats(Heap *heap, size_t count, const Str *parts) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

static Comp pure_vec(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = items;
  return budget_pure(boxed);
}

static Comp pure_out(Heap *heap, const Ktm *value, EraseAcc ac) {
  EraseOut out = erase_out(value, ac);
  EraseOut *boxed = erase_out_new(heap, out);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_outs(Heap *heap, Vec values, EraseAcc ac) {
  EraseOuts outs = erase_outs(values, ac);
  EraseOuts *boxed = erase_outs_new(heap, outs);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

/* A List<T.T> literal of host call arguments. */
static Vec term_list(Heap *heap, size_t count, const Term *const *items) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < count; i++) push(heap, &out, (void *)items[i]);
  return out;
}

/* Constructors. */
static const EraseEntry entry_zero;
static const EraseFound found_zero;
static const EraseSlot slot_boxes[3] = {ERASE_SLOT_DROP, ERASE_SLOT_KEEP, ERASE_SLOT_EXTRA};

EraseEntry erase_entry_dropped(void) {
  EraseEntry e = entry_zero;
  e.kind = ERASE_ENTRY_DROPPED;
  return e;
}

EraseEntry erase_entry_postulate(Repr repr) {
  EraseEntry e = entry_zero;
  e.kind = ERASE_ENTRY_POSTULATE;
  e.repr = repr;
  return e;
}

EraseEntry erase_entry_code(Vec decls) {
  EraseEntry e = entry_zero;
  e.kind = ERASE_ENTRY_CODE;
  e.decls = decls;
  return e;
}

void *erase_slot_box(EraseSlot slot) {
  switch (slot) {
    case ERASE_SLOT_DROP: return (void *)&slot_boxes[0];
    case ERASE_SLOT_KEEP: return (void *)&slot_boxes[1];
    case ERASE_SLOT_EXTRA: return (void *)&slot_boxes[2];
  }
  return (void *)&slot_boxes[2];
}

EraseSlot erase_slot_unbox(const void *boxed) { return *(const EraseSlot *)boxed; }

EraseFound erase_found_keep(Nat index) {
  EraseFound f = found_zero;
  f.kind = ERASE_FOUND_KEEP;
  f.index = index;
  return f;
}

EraseFound erase_found_drop(void) {
  EraseFound f = found_zero;
  f.kind = ERASE_FOUND_DROP;
  return f;
}

EraseFound erase_found_out(void) {
  EraseFound f = found_zero;
  f.kind = ERASE_FOUND_OUT;
  return f;
}

EraseAcc erase_acc(Nat next, Vec lifted, Vec groups) {
  EraseAcc ac;
  ac.next = next;
  ac.lifted = lifted;
  ac.groups = groups;
  return ac;
}

EraseOut erase_out(const Ktm *value, EraseAcc acc) {
  EraseOut out;
  out.value = value;
  out.acc = acc;
  return out;
}

EraseOuts erase_outs(Vec values, EraseAcc acc) {
  EraseOuts out;
  out.values = values;
  out.acc = acc;
  return out;
}

EraseChain erase_chain_make(Vec params, Repr result, const Ktm *body, EraseAcc acc) {
  EraseChain chain;
  chain.params = params;
  chain.result = result;
  chain.body = body;
  chain.acc = acc;
  return chain;
}

EraseSignature erase_signature(Vec params, Repr result) {
  EraseSignature sig;
  sig.params = params;
  sig.result = result;
  return sig;
}

ExpectedTy erase_expected_some(const Value *ty) {
  ExpectedTy e;
  e.some = true;
  e.ty = ty;
  return e;
}

ExpectedTy erase_expected_none(void) {
  ExpectedTy e;
  e.some = false;
  e.ty = NULL;
  return e;
}

EraseOut *erase_out_new(Heap *heap, EraseOut out) {
  EraseOut *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = out;
  return boxed;
}

EraseOuts *erase_outs_new(Heap *heap, EraseOuts outs) {
  EraseOuts *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = outs;
  return boxed;
}

EraseChain *erase_chain_new(Heap *heap, EraseChain chain) {
  EraseChain *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = chain;
  return boxed;
}

EraseSignature *erase_signature_new(Heap *heap, EraseSignature sig) {
  EraseSignature *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = sig;
  return boxed;
}

Pair *erase_job_new(Heap *heap, ExpectedTy expected, const Term *t) {
  ExpectedTy *boxed = heap_alloc(heap, sizeof *boxed);
  Pair *job;
  if (boxed == NULL) return NULL;
  *boxed = expected;
  job = heap_alloc(heap, sizeof *job);
  if (job == NULL) return NULL;
  job->first = boxed;
  job->second = (void *)t;
  return job;
}

/* lookup: the Bend recursion walks ix down per SKeep or SDrop slot and seen up per SExtra or SKeep
 * slot; the loop counts the SKeep and SDrop slots passed instead of decrementing ix. */
EraseFound erase_lookup(Heap *heap, const Vec *slots, Nat ix, Nat seen) {
  Nat passed = nat_zero();
  size_t i;
  for (i = 0; i < slots->len; i++) {
    VecGet got = vec_get(slots, i);
    EraseSlot slot;
    if (got.status != VEC_GET_OK) return erase_found_out();
    slot = erase_slot_unbox(got.value);
    switch (slot) {
      case ERASE_SLOT_EXTRA:
        seen = heap_nat_succ(heap, seen);
        break;
      case ERASE_SLOT_KEEP:
        if (nat_cmp(passed, ix) == 0) return erase_found_keep(seen);
        passed = heap_nat_succ(heap, passed);
        seen = heap_nat_succ(heap, seen);
        break;
      case ERASE_SLOT_DROP:
        if (nat_cmp(passed, ix) == 0) return erase_found_drop();
        passed = heap_nat_succ(heap, passed);
        break;
    }
  }
  return erase_found_out();
}

bool erase_quantity_runtime(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return false;
    case QUANTITY_ONE: return true;
    case QUANTITY_MANY: return true;
  }
  return false;
}

const Ktm *erase_out_value(EraseOut out) { return out.value; }

EraseAcc erase_out_acc(EraseOut out) { return out.acc; }

Vec erase_outs_values(EraseOuts out) { return out.values; }

EraseAcc erase_outs_acc(EraseOuts out) { return out.acc; }

EraseAcc erase_add_lift(Heap *heap, EraseAcc ac, const Kdecl *decl) {
  Vec one = empty(heap);
  Vec lifted;
  push(heap, &one, (void *)decl);
  lifted = append(heap, &ac.lifted, &one);
  return erase_acc(ac.next, lifted, ac.groups);
}

EraseAcc erase_add_groups(Heap *heap, EraseAcc ac, const Vec *tids) {
  Vec groups = append(heap, &ac.groups, tids);
  return erase_acc(ac.next, ac.lifted, groups);
}

Nat erase_next(EraseAcc ac) { return ac.next; }

EraseAcc erase_advance(Heap *heap, EraseAcc ac) {
  Nat n = heap_nat_succ(heap, ac.next);
  return erase_acc(n, ac.lifted, ac.groups);
}

Comp erase_position_ty(Heap *heap, EraseCtx ec, ExpectedTy expected, const Term *t) {
  if (expected.some) return budget_pure((void *)expected.ty);
  return erase_repr_infer(heap, ec, t);
}

Comp erase_var_found(Heap *heap, MaybeNat found, EraseAcc ac) {
  if (found.some) {
    const Ktm *var = eterm_kvar(heap, found.value);
    return pure_out(heap, var, ac);
  }
  return pure_out(heap, eterm_kerased(), ac);
}

Nat erase_kernel_slots(Heap *heap, const Vec *slots) {
  Nat n = nat_zero();
  size_t i;
  for (i = 0; i < slots->len; i++) {
    VecGet got = vec_get(slots, i);
    Slot slot;
    if (got.status != VEC_GET_OK) return n;
    slot = erase_repr_slot_unbox(got.value);
    switch (slot) {
      case SLOT_EXTRA: break;
      case SLOT_DROP:
      case SLOT_KEEP:
        n = heap_nat_succ(heap, n);
        break;
    }
  }
  return n;
}

static Comp var_arm_after_present(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Nat zero = nat_zero();
  MaybeNat found;
  (void)value;
  found = erase_util_lookup(heap, &f->slots, f->ix, zero);
  return erase_var_found(heap, found, f->ac);
}

Comp erase_var_arm(Heap *heap, EraseCtx ec, EraseAcc ac, Nat ix) {
  Vec slots = erase_repr_slots(ec);
  Nat count = erase_kernel_slots(heap, &slots);
  bool valid = nat_cmp(ix, count) < 0;
  Str shown = heap_nat_show(heap, ix);
  Str parts[2];
  Str message;
  Frame *f = frame_new(heap);
  parts[0] = str_lit("the erasure context has no binder at index ");
  parts[1] = shown;
  message = cats(heap, 2, parts);
  if (f == NULL) return comp_oom();
  f->slots = slots;
  f->ix = ix;
  f->ac = ac;
  return bind_frame(heap, check_engine_require(valid, error_make(ERROR_UNBOUND, message)), var_arm_after_present, f);
}

static Comp erase_fold_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseOuts *more = value;
  Vec values = cons(heap, (void *)f->one.value, &more->values);
  return pure_outs(heap, values, more->acc);
}

static Comp erase_fold_after_one(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseOut *one = value;
  f->one = *one;
  return bind_frame(heap, erase_erase_fold(heap, &f->jobs, f->ec, one->acc), erase_fold_after_more, f);
}

Comp erase_erase_fold(Heap *heap, const Vec *jobs, EraseCtx ec, EraseAcc ac) {
  VecGet got;
  Pair *job;
  ExpectedTy *expected;
  Frame *f;
  if (jobs->len == 0) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  got = vec_get(jobs, 0);
  if (got.status != VEC_GET_OK) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  job = got.value;
  expected = job->first;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->jobs = list_tail(*jobs);
  return bind_frame(heap, erase_term(heap, ec, ac, false, *expected, job->second), erase_fold_after_one, f);
}

MaybeLegPtr erase_as_lam(const Term *t) {
  MaybeLegPtr out;
  VecGet got;
  out.some = false;
  out.value = NULL;
  if (t->kind != TERM_SEC || t->shape.kind != SHAPE_PI || t->legs.len != 1) return out;
  got = vec_get(&t->legs, 0);
  if (got.status != VEC_GET_OK) return out;
  out.some = true;
  out.value = got.value;
  return out;
}

bool erase_function_node(const Value *value) {
  if (value->kind != VALUE_RAN) return false;
  return value->shape.kind == SHAPE_PI || value->shape.kind == SHAPE_ZK;
}

static Comp function_result_after_whnf(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  return pure_bool(erase_function_node(value));
}

Comp erase_function_result(Heap *heap, EraseCtx ec, const Value *ty) {
  return bind_with(heap, erase_repr_whnf(heap, ec, ty), function_result_after_whnf, NULL);
}

static Comp host_arg_keep_after_out(Heap *heap, void *ctx, void *value) {
  EraseOut *out = value;
  Vec values = empty(heap);
  (void)ctx;
  push(heap, &values, (void *)out->value);
  return pure_outs(heap, values, out->acc);
}

Comp erase_host_arg_keep(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t) {
  ExpectedTy expected;
  if (!keep) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  expected = erase_expected_some(ty);
  return bind_with(heap, erase_term(heap, ec, ac, false, expected, t), host_arg_keep_after_out, NULL);
}

static Comp host_arg_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_host_arg_keep(heap, kernel_bool_unbox(value), f->ec, f->ac, f->ty, f->t);
}

static Comp host_arg_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, f->ty), host_arg_after_keep, f);
}

Comp erase_host_arg(Heap *heap, EraseCtx ec, EraseAcc ac, const Term *t) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->t = t;
  return bind_frame(heap, erase_repr_infer(heap, ec, t), host_arg_after_ty, f);
}

static Comp host_args_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseOuts *more = value;
  Vec values = append(heap, &f->outs.values, &more->values);
  return pure_outs(heap, values, more->acc);
}

static Comp host_args_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseOuts *out = value;
  f->outs = *out;
  return bind_frame(heap, erase_host_args(heap, &f->terms, f->ec, out->acc), host_args_after_more, f);
}

Comp erase_host_args(Heap *heap, const Vec *ts, EraseCtx ec, EraseAcc ac) {
  VecGet got;
  Frame *f;
  if (ts->len == 0) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  got = vec_get(ts, 0);
  if (got.status != VEC_GET_OK) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->terms = list_tail(*ts);
  return bind_frame(heap, erase_host_arg(heap, ec, ac, got.value), host_args_after_out, f);
}

static Comp host_call_after_args(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseOuts *args = value;
  const Ktm *head = eterm_kglobal(heap, f->name);
  const Ktm *app = eterm_kapp(heap, head, args->values);
  return pure_out(heap, app, args->acc);
}

Comp erase_host_call(Heap *heap, EraseCtx ec, EraseAcc ac, Str name, const Vec *ts) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->name = name;
  return bind_frame(heap, erase_host_args(heap, ts, ec, ac), host_call_after_args, f);
}

Comp erase_leg_arg(const Leg *leg, Str message) {
  if (leg->binders.len == 0) return budget_pure((void *)leg->body);
  return budget_failed(error_make(ERROR_MISMATCH, message));
}

Comp erase_fhc_pick(Heap *heap, bool is_eval, EraseCtx ec, EraseAcc ac, const Term *level, const Term *first, const Term *second) {
  const Term *items[3];
  Vec args;
  Str name;
  items[0] = level;
  if (is_eval) {
    items[1] = first;
    items[2] = second;
    args = term_list(heap, 3, items);
    name = str_lit("fhcEval");
    return erase_host_call(heap, ec, ac, name, &args);
  }
  items[1] = second;
  args = term_list(heap, 2, items);
  name = str_lit("fhcEnc");
  return erase_host_call(heap, ec, ac, name, &args);
}

static Comp fhc_sec_after_is_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_fhc_pick(heap, kernel_bool_unbox(value), f->ec, f->ac, f->level, f->first, f->second);
}

static Comp fhc_sec_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_function_result(heap, f->ec, value), fhc_sec_after_is_eval, f);
}

static Comp fhc_sec_after_second(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->second = value;
  return bind_frame(heap, erase_repr_infer(heap, f->ec, f->first), fhc_sec_after_ty, f);
}

static Comp fhc_sec_after_first(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->first = value;
  return bind_frame(heap, erase_leg_arg(f->leg, f->message), fhc_sec_after_second, f);
}

Comp erase_fhc_sec(Heap *heap, EraseCtx ec, EraseAcc ac, const Term *level, const Vec *legs) {
  Str message = str_lit("a ciphertext section takes two arguments");
  VecGet a;
  VecGet b;
  Frame *f;
  if (legs->len != 2) return budget_failed(error_make(ERROR_MISMATCH, message));
  a = vec_get(legs, 0);
  b = vec_get(legs, 1);
  if (a.status != VEC_GET_OK || b.status != VEC_GET_OK) return budget_failed(error_make(ERROR_MISMATCH, message));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->level = level;
  f->leg = b.value;
  f->message = message;
  return bind_frame(heap, erase_leg_arg(a.value, message), fhc_sec_after_first, f);
}

static Comp leg_args_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec *more = value;
  Vec out = cons(heap, (void *)f->t, more);
  return pure_vec(heap, out);
}

static Comp leg_args_after_t(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->t = value;
  return bind_frame(heap, erase_leg_args(heap, &f->legs, f->message), leg_args_after_more, f);
}

Comp erase_leg_args(Heap *heap, const Vec *legs, Str message) {
  VecGet got;
  Frame *f;
  if (legs->len == 0) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  got = vec_get(legs, 0);
  if (got.status != VEC_GET_OK) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->legs = list_tail(*legs);
  f->message = message;
  return bind_frame(heap, erase_leg_arg(got.value, message), leg_args_after_t, f);
}

/* mpc_args: [x] and [p, x] both call mpcInput with the list as given; three or more call mpcShare
 * with the whole list (p <> f <> c <> cs). */
Comp erase_mpc_args(Heap *heap, const Vec *args, EraseCtx ec, EraseAcc ac) {
  Str input = str_lit("mpcInput");
  Str share = str_lit("mpcShare");
  if (args->len == 0) return mismatch("an mpc section takes one argument or more");
  if (args->len <= 2) return erase_host_call(heap, ec, ac, input, args);
  return erase_host_call(heap, ec, ac, share, args);
}

static Comp mpc_sec_after_args(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_mpc_args(heap, value, f->ec, f->ac);
}

Comp erase_mpc_sec(Heap *heap, EraseCtx ec, EraseAcc ac, const Vec *legs) {
  Str message = str_lit("an mpc section leg binds nothing");
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  return bind_frame(heap, erase_leg_args(heap, legs, message), mpc_sec_after_args, f);
}

Comp erase_zk_args(Heap *heap, const Vec *args, EraseCtx ec, EraseAcc ac, const Term *point) {
  Str prove = str_lit("zkProve");
  const Term *items[2];
  Vec call;
  if (args->len == 2) {
    VecGet inst = vec_get(args, 1);
    if (inst.status != VEC_GET_OK) return mismatch("a proof carries one relation leg");
    items[0] = inst.value;
    items[1] = point;
    call = term_list(heap, 2, items);
    return erase_host_call(heap, ec, ac, prove, &call);
  }
  if (args->len == 1) {
    items[0] = point;
    call = term_list(heap, 1, items);
    return erase_host_call(heap, ec, ac, prove, &call);
  }
  return mismatch("a proof carries one relation leg");
}

Comp erase_zk_in(Heap *heap, EraseCtx ec, EraseAcc ac, Addr addr, const Vec *args) {
  Str message = str_lit("a proof takes the point address");
  switch (addr.kind) {
    case ADDR_PT: return erase_zk_args(heap, args, ec, ac, addr.arg);
    case ADDR_LEG:
    case ADDR_CTOR: return budget_failed(error_make(ERROR_WRONG_LEG, message));
  }
  return budget_failed(error_make(ERROR_WRONG_LEG, message));
}

/* tuple_jobs: the Bend triple match stops at the first exhausted list; flags are kernel bool boxes,
 * types are Value pointers, legs are Leg pointers. */
Vec erase_tuple_jobs(Heap *heap, const Vec *legs, const Vec *types, const Vec *flags) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < legs->len && i < types->len && i < flags->len; i++) {
    VecGet leg = vec_get(legs, i);
    VecGet ty = vec_get(types, i);
    VecGet flag = vec_get(flags, i);
    if (leg.status != VEC_GET_OK || ty.status != VEC_GET_OK || flag.status != VEC_GET_OK) return out;
    if (kernel_bool_unbox(flag.value)) {
      ExpectedTy expected = erase_expected_some(ty.value);
      Pair *job = erase_job_new(heap, expected, ((const Leg *)leg.value)->body);
      if (job != NULL) push(heap, &out, job);
    }
  }
  return out;
}

/* Quarter 2 = Bend lines 258-524: tuple_finish .. ctor_indices with the law bodies signature_value,
 * remaining_signature and chain. */

static Comp pure_chain(Heap *heap, EraseChain chain) {
  EraseChain *boxed = erase_chain_new(heap, chain);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_signature(Heap *heap, EraseSignature sig) {
  EraseSignature *boxed = erase_signature_new(heap, sig);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

static Comp pure_str(Heap *heap, Str text) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = text;
  return budget_pure(boxed);
}

static Comp pure_big(Heap *heap, Big value) {
  Big *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp failed_mismatch(Str message) { return budget_failed(error_make(ERROR_MISMATCH, message)); }

/* List.length as a Nat. */
static Nat length_nat(Heap *heap, const Vec *xs) {
  Nat n = nat_zero();
  size_t i;
  for (i = 0; i < xs->len; i++) n = heap_nat_succ(heap, n);
  return n;
}

/* The `1n+ p` pattern: p for a positive n (zero, with heap->oom set, after a failed allocation). */
static Nat pred(Heap *heap, Nat n) {
  Nat zero = nat_zero();
  Nat one = heap_nat_succ(heap, zero);
  NatSub sub = nat_sub(heap->arena, n, one);
  switch (sub.status) {
    case NAT_SUB_OK: return sub.value;
    case NAT_SUB_UNDERFLOW: return zero;
    case NAT_SUB_OOM:
      heap->oom = true;
      return zero;
  }
  return zero;
}

/* tuple_finish */
static Comp tuple_finish_after_tid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Tid *tid = value;
  const Ktm *node = eterm_kstruct(heap, *tid, f->fields);
  return pure_out(heap, node, f->ac);
}

Comp erase_tuple_finish(Heap *heap, const Vec *fields, EraseAcc ac, EraseCtx ec, const Value *ty) {
  Frame *f;
  if (fields->len == 0) return pure_out(heap, eterm_kerased(), ac);
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ac = ac;
  f->fields = *fields;
  return bind_frame(heap, erase_repr_tid_of(heap, ec, ty), tuple_finish_after_tid, f);
}

/* tuple_type */
static Comp tuple_type_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *out = value;
  Vec values = erase_outs_values(*out);
  EraseAcc acc = erase_outs_acc(*out);
  return erase_tuple_finish(heap, &values, acc, f->ec, f->ty);
}

static Comp tuple_type_after_flags(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *flags = value;
  f->jobs = erase_tuple_jobs(heap, &f->legs, &f->tys, flags);
  return bind_frame(heap, erase_erase_fold(heap, &f->jobs, f->ec, f->ac), tuple_type_after_out, f);
}

static Comp tuple_type_after_tys(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *tys = value;
  f->tys = *tys;
  return bind_frame(heap, erase_repr_flags(heap, f->tys, f->ec), tuple_type_after_flags, f);
}

Comp erase_tuple_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *ty, Big count,
                      const Vec *legs) {
  const char *message = "a tuple stands at a type that is not a right former";
  Frame *f;
  switch (value->kind) {
    case VALUE_RAN:
      f = frame_new(heap);
      if (f == NULL) return comp_oom();
      f->ec = ec;
      f->ac = ac;
      f->ty = ty;
      f->legs = *legs;
      return bind_frame(heap, erase_repr_leg_types(heap, ec, count, value->diagram), tuple_type_after_tys, f);
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch(message);
  }
  return mismatch(message);
}

/* tuple_arm */
static Comp tuple_arm_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_tuple_type(heap, value, f->ec, f->ac, f->ty, f->count, &f->legs);
}

Comp erase_tuple_arm(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Big count, const Vec *legs) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = ty;
  f->count = count;
  f->legs = *legs;
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), tuple_arm_after_value, f);
}

/* capture_reprs */
static Comp capture_reprs_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  Vec out = cons(heap, (void *)f->repr, more);
  return pure_vec(heap, out);
}

static Comp capture_reprs_after_repr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->repr = value;
  return bind_frame(heap, erase_capture_reprs(heap, &f->caps, f->ec), capture_reprs_after_more, f);
}

static Comp capture_reprs_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_repr_repr_of(heap, f->ec, value), capture_reprs_after_repr, f);
}

Comp erase_capture_reprs(Heap *heap, const Vec *caps, EraseCtx ec) {
  VecGet got;
  const Nat *boxed;
  Nat ix;
  const Term *var;
  Frame *f;
  if (caps->len == 0) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  got = vec_get(caps, 0);
  if (got.status != VEC_GET_OK) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  boxed = got.value;
  ix = *boxed;
  var = term_var(heap, ix);
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->caps = list_tail(*caps);
  return bind_frame(heap, erase_repr_infer(heap, ec, var), capture_reprs_after_ty, f);
}

/* capture_args */
static Comp capture_args_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  Vec out = cons(heap, (void *)erase_out_value(f->one), more);
  return pure_vec(heap, out);
}

static Comp capture_args_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  f->one = *out;
  return bind_frame(heap, erase_capture_args(heap, &f->caps, f->ec, f->ac), capture_args_after_more, f);
}

Comp erase_capture_args(Heap *heap, const Vec *caps, EraseCtx ec, EraseAcc ac) {
  VecGet got;
  const Nat *boxed;
  Nat ix;
  Frame *f;
  if (caps->len == 0) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  got = vec_get(caps, 0);
  if (got.status != VEC_GET_OK) {
    Vec none = empty(heap);
    return pure_vec(heap, none);
  }
  boxed = got.value;
  ix = *boxed;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->caps = list_tail(*caps);
  return bind_frame(heap, erase_var_arm(heap, ec, ac, ix), capture_args_after_out, f);
}

/* lift_pruned: a NULL declaration box (heap->oom set) leaves the accumulator as it was. */
EraseOut erase_lift_pruned(Heap *heap, Pruned pruned, Fid fid, const Vec *params, Repr ret, EraseAcc ac) {
  Nat arity = length_nat(heap, params);
  const Ktm *clos = eterm_kclos(heap, fid, arity, pruned.args);
  Vec all = append(heap, &pruned.reprs, params);
  Kdecl fun = eterm_kfun(fid, all, ret, pruned.body);
  Kdecl *decl = eterm_kdecl_new(heap, fun);
  EraseAcc acc = decl == NULL ? ac : erase_add_lift(heap, ac, decl);
  return erase_out(clos, acc);
}

EraseOut erase_lift_finish(Heap *heap, EraseChain result, const Vec *reprs, const Vec *args, Fid fid) {
  Nat count = length_nat(heap, &result.params);
  Pruned pruned = erase_util_prune_captures(heap, reprs, args, count, result.body);
  return erase_lift_pruned(heap, pruned, fid, &result.params, result.result, result.acc);
}

/* lift_arm */
static Comp lift_arm_after_result(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseChain *result = value;
  EraseOut out = erase_lift_finish(heap, *result, &f->reprs, &f->args, f->fid);
  return pure_out(heap, erase_out_value(out), erase_out_acc(out));
}

static Comp lift_arm_after_args(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *args = value;
  Str self = erase_repr_self(f->ec);
  Nat next = erase_next(f->ac);
  Str shown = heap_nat_show(heap, next);
  Str parts[3];
  Str text;
  Vec slots = erase_repr_slots(f->ec);
  Nat zero = nat_zero();
  Vec framed;
  EraseCtx inner;
  EraseAcc advanced;
  Vec none;
  f->args = *args;
  parts[0] = self;
  parts[1] = str_lit("$");
  parts[2] = shown;
  text = cats(heap, 3, parts);
  f->fid = eterm_fid(text);
  framed = erase_util_frame(heap, &slots, &f->caps, zero);
  inner = erase_repr_replace_slots(f->ec, framed);
  advanced = erase_advance(heap, f->ac);
  none = empty(heap);
  return bind_frame(heap, erase_chain(heap, inner, advanced, none, f->ty, f->t), lift_arm_after_result, f);
}

static Comp lift_arm_after_reprs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *reprs = value;
  f->reprs = *reprs;
  return bind_frame(heap, erase_capture_args(heap, &f->outer, f->ec, f->ac), lift_arm_after_args, f);
}

Comp erase_lift_arm(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t) {
  Vec caps = erase_util_captures_of(heap, ec, t);
  Vec outer = vec_reverse(heap->arena, &caps);
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = ty;
  f->t = t;
  f->caps = caps;
  f->outer = outer;
  return bind_frame(heap, erase_capture_reprs(heap, &f->outer, ec), lift_arm_after_reprs, f);
}

/* add_param */
static Comp add_param_after_repr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec one = empty(heap);
  Vec out;
  push(heap, &one, value);
  out = append(heap, &f->params, &one);
  return pure_vec(heap, out);
}

Comp erase_add_param(Heap *heap, EraseCtx ec, bool keep, const Value *ty, const Vec *params) {
  Frame *f;
  if (!keep) return pure_vec(heap, *params);
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->params = *params;
  return bind_frame(heap, erase_repr_repr_of(heap, ec, ty), add_param_after_repr, f);
}

/* chain_stop */
static Comp chain_stop_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  EraseChain chain = erase_chain_make(f->params, *f->repr, erase_out_value(*out), erase_out_acc(*out));
  return pure_chain(heap, chain);
}

static Comp chain_stop_after_ret(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ExpectedTy expected = erase_expected_some(f->ty);
  f->repr = value;
  return bind_frame(heap, erase_term(heap, f->ec, f->ac, true, expected, f->t), chain_stop_after_out, f);
}

Comp erase_chain_stop(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->params = params;
  f->ty = ty;
  f->t = t;
  return bind_frame(heap, erase_repr_repr_of(heap, ec, ty), chain_stop_after_ret, f);
}

/* chain_point */
static Comp chain_point_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Slot slot = erase_repr_keep_slot(f->keep);
  EraseCtx inner = erase_repr_bind(heap, f->ec, f->name, f->q, f->dom, slot);
  return erase_chain(heap, inner, f->ac, f->params, value, f->leg->body);
}

static Comp chain_point_after_params(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *params = value;
  Nat size = erase_repr_size(f->ec);
  const Value *var = value_var(heap, size);
  Vec args = empty(heap);
  f->params = *params;
  push(heap, &args, (void *)var);
  return bind_frame(heap, erase_repr_open(heap, f->ec, f->clo, args), chain_point_after_cod, f);
}

static Comp chain_point_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_add_param(heap, f->ec, f->keep, f->dom, &f->params), chain_point_after_params, f);
}

Comp erase_chain_point(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, Quantity q, Str name, const Value *dom,
                       Closure clo, const Leg *leg) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->params = params;
  f->q = q;
  f->name = name;
  f->dom = dom;
  f->clo = clo;
  f->leg = leg;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, q, dom), chain_point_after_keep, f);
}

/* signature_rest */
static Comp signature_rest_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_signature_value(heap, value, f->ec, f->cod, f->here);
}

Comp erase_signature_rest(Heap *heap, const Pair *pair, Vec here) {
  const EraseCtx *ec = pair->first;
  const Value *cod = pair->second;
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = *ec;
  f->cod = cod;
  f->here = here;
  return bind_frame(heap, erase_repr_whnf(heap, *ec, cod), signature_rest_after_value, f);
}

EraseSignature erase_append_signature(Heap *heap, EraseSignature sig, const Vec *here) {
  Vec params = append(heap, here, &sig.params);
  return erase_signature(params, sig.result);
}

/* signature_value (law body) */
static Comp signature_value_after_sig(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseSignature *sig = value;
  EraseSignature out = erase_append_signature(heap, *sig, &f->here);
  return pure_signature(heap, out);
}

static Comp signature_value_after_ret(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Repr *ret = value;
  EraseSignature out = erase_signature(f->here, *ret);
  return pure_signature(heap, out);
}

Comp erase_signature_value(Heap *heap, const Value *value, EraseCtx ec, const Value *cod, Vec here) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->cod = cod;
  f->here = here;
  switch (value->kind) {
    case VALUE_RAN:
      return bind_frame(heap, erase_remaining_signature(heap, ec, value->shape, value->diagram),
                        signature_value_after_sig, f);
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return bind_frame(heap, erase_repr_repr_of(heap, ec, cod), signature_value_after_ret, f);
  }
  return bind_frame(heap, erase_repr_repr_of(heap, ec, cod), signature_value_after_ret, f);
}

/* signature_point */
static Comp signature_point_after_pair(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_signature_rest(heap, value, f->here);
}

static Comp signature_point_after_here(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *here = value;
  f->here = *here;
  return bind_frame(heap, erase_repr_under_point(heap, f->ec, f->name, f->q, f->dom, f->clo),
                    signature_point_after_pair, f);
}

static Comp signature_point_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  bool keep = kernel_bool_unbox(value);
  Vec none = empty(heap);
  return bind_frame(heap, erase_add_param(heap, f->ec, keep, f->dom, &none), signature_point_after_here, f);
}

Comp erase_signature_point(Heap *heap, EraseCtx ec, Quantity q, Str name, const Value *dom, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->q = q;
  f->name = name;
  f->dom = dom;
  f->clo = clo;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, q, dom), signature_point_after_keep, f);
}

/* remaining_signature (law body) */
static Comp remaining_signature_after_repr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Repr *repr = value;
  Vec none = empty(heap);
  EraseSignature out = erase_signature(none, *repr);
  f->repr = repr;
  return pure_signature(heap, out);
}

Comp erase_remaining_signature(Heap *heap, EraseCtx ec, Shape shape, Closure clo) {
  Frame *f;
  switch (shape.kind) {
    case SHAPE_PI:
    case SHAPE_ZK: return erase_signature_point(heap, ec, shape.quantity, shape.name, shape.domain, clo);
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_MU:
    case SHAPE_NU:
    case SHAPE_FHC:
    case SHAPE_MPC:
      f = frame_new(heap);
      if (f == NULL) return comp_oom();
      return bind_frame(heap, erase_repr_ran_repr(heap, ec, shape, clo), remaining_signature_after_repr, f);
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  return bind_frame(heap, erase_repr_ran_repr(heap, ec, shape, clo), remaining_signature_after_repr, f);
}

/* eta_args: [KVar{n-1}, .., KVar{0}]. */
Vec erase_eta_args(Heap *heap, Nat n) {
  Vec out = empty(heap);
  Nat p = n;
  while (!nat_is_zero(p)) {
    const Ktm *var;
    p = pred(heap, p);
    var = eterm_kvar(heap, p);
    push(heap, &out, (void *)var);
  }
  return out;
}

EraseChain erase_eta_finish(Heap *heap, EraseOut out, EraseSignature sig, const Vec *params) {
  Nat n = length_nat(heap, &sig.params);
  Nat zero = nat_zero();
  Vec all = append(heap, params, &sig.params);
  const Ktm *head = erase_util_shift_runtime(heap, n, zero, erase_out_value(out));
  Vec args = erase_eta_args(heap, n);
  const Ktm *body = eterm_ktail(heap, head, args);
  return erase_chain_make(all, sig.result, body, erase_out_acc(out));
}

/* eta_chain */
static Comp eta_chain_after_sig(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseSignature *sig = value;
  EraseChain chain = erase_eta_finish(heap, f->one, *sig, &f->params);
  return pure_chain(heap, chain);
}

static Comp eta_chain_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  f->one = *out;
  return bind_frame(heap, erase_remaining_signature(heap, f->ec, f->shape, f->clo), eta_chain_after_sig, f);
}

Comp erase_eta_chain(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t, Shape s,
                     Closure d) {
  ExpectedTy expected = erase_expected_some(ty);
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->params = params;
  f->shape = s;
  f->clo = d;
  return bind_frame(heap, erase_term(heap, ec, ac, false, expected, t), eta_chain_after_out, f);
}

Comp erase_chain_shape(Heap *heap, MaybeLegPtr leg, Shape shape, EraseCtx ec, EraseAcc ac, Vec params,
                       const Value *ty, const Term *t, Closure clo) {
  switch (shape.kind) {
    case SHAPE_PI:
    case SHAPE_ZK:
      if (leg.some) {
        return erase_chain_point(heap, ec, ac, params, shape.quantity, shape.name, shape.domain, clo, leg.value);
      }
      return erase_eta_chain(heap, ec, ac, params, ty, t, shape, clo);
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_MU:
    case SHAPE_NU:
    case SHAPE_FHC:
    case SHAPE_MPC: return erase_chain_stop(heap, ec, ac, params, ty, t);
  }
  return erase_chain_stop(heap, ec, ac, params, ty, t);
}

Comp erase_chain_value(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty,
                       const Term *t) {
  MaybeLegPtr leg;
  switch (value->kind) {
    case VALUE_RAN:
      leg = erase_as_lam(t);
      return erase_chain_shape(heap, leg, value->shape, ec, ac, params, ty, t, value->diagram);
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return erase_chain_stop(heap, ec, ac, params, ty, t);
  }
  return erase_chain_stop(heap, ec, ac, params, ty, t);
}

/* chain (law body) */
static Comp chain_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_chain_value(heap, value, f->ec, f->ac, f->params, f->ty, f->t);
}

Comp erase_chain(Heap *heap, EraseCtx ec, EraseAcc ac, Vec params, const Value *ty, const Term *t) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->params = params;
  f->ty = ty;
  f->t = t;
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), chain_after_value, f);
}

Comp erase_one_term(const Vec *args, Str message) {
  VecGet got;
  if (args->len != 1) return failed_mismatch(message);
  got = vec_get(args, 0);
  if (got.status != VEC_GET_OK) return failed_mismatch(message);
  return budget_pure(got.value);
}

Comp erase_point_term(Addr addr, Str message) {
  switch (addr.kind) {
    case ADDR_PT: return budget_pure((void *)addr.arg);
    case ADDR_LEG:
    case ADDR_CTOR: return failed_mismatch(message);
  }
  return failed_mismatch(message);
}

Comp erase_under_runtime(Heap *heap, const Pair *pair) {
  const EraseCtx *ec = pair->first;
  return erase_repr_runtime_ty(heap, *ec, pair->second);
}

Vec erase_job(Heap *heap, bool keep, const Value *ty, const Term *t) {
  Vec out = empty(heap);
  if (keep) {
    ExpectedTy expected = erase_expected_some(ty);
    Pair *job = erase_job_new(heap, expected, t);
    if (job != NULL) push(heap, &out, job);
  }
  return out;
}

/* pair_intro */
static Comp pair_intro_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *out = value;
  Vec values = erase_outs_values(*out);
  EraseAcc acc = erase_outs_acc(*out);
  return erase_tuple_finish(heap, &values, acc, f->ec, f->ty);
}

static Comp pair_intro_after_fibre_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec first = erase_job(heap, f->keep, f->dom, f->point);
  Vec second = erase_job(heap, f->keep2, value, f->fibre);
  f->jobs = append(heap, &first, &second);
  return bind_frame(heap, erase_erase_fold(heap, &f->jobs, f->ec, f->ac), pair_intro_after_out, f);
}

static Comp pair_intro_after_point_v(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec args = empty(heap);
  push(heap, &args, value);
  return bind_frame(heap, erase_repr_open(heap, f->ec, f->clo, args), pair_intro_after_fibre_ty, f);
}

static Comp pair_intro_after_keep2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep2 = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_eval(heap, f->ec, f->point), pair_intro_after_point_v, f);
}

static Comp pair_intro_after_pair(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_under_runtime(heap, value), pair_intro_after_keep2, f);
}

static Comp pair_intro_after_keep1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_under_point(heap, f->ec, f->name, f->q, f->dom, f->clo),
                    pair_intro_after_pair, f);
}

static Comp pair_intro_after_fibre(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->fibre = value;
  return bind_frame(heap, erase_repr_point_runtime(heap, f->ec, f->q, f->dom), pair_intro_after_keep1, f);
}

static Comp pair_intro_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Str message = str_lit("a pair takes one fibre");
  f->point = value;
  return bind_frame(heap, erase_one_term(&f->args, message), pair_intro_after_fibre, f);
}

Comp erase_pair_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Quantity q, Str x, const Value *dom,
                      Closure clo, Addr addr, const Vec *args) {
  Str message = str_lit("a pair takes a point address");
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = ty;
  f->q = q;
  f->name = x;
  f->dom = dom;
  f->clo = clo;
  f->args = *args;
  return bind_frame(heap, erase_point_term(addr, message), pair_intro_after_point, f);
}

Comp erase_leg_index(Heap *heap, Addr addr, Str message) {
  switch (addr.kind) {
    case ADDR_LEG: return pure_big(heap, addr.index);
    case ADDR_PT:
    case ADDR_CTOR: return failed_mismatch(message);
  }
  return failed_mismatch(message);
}

/* tag_intro: f->dom carries lty once it is found. */
static Comp tag_intro_after_tid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Tid *tid = value;
  Vec values = erase_outs_values(f->outs);
  EraseAcc acc = erase_outs_acc(f->outs);
  const Ktm *node = eterm_ktag(heap, *tid, f->index, values);
  return pure_out(heap, node, acc);
}

static Comp tag_intro_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *out = value;
  f->outs = *out;
  return bind_frame(heap, erase_repr_tid_of(heap, f->ec, f->ty), tag_intro_after_tid, f);
}

static Comp tag_intro_after_payload(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->jobs = erase_job(heap, f->keep, f->dom, value);
  return bind_frame(heap, erase_erase_fold(heap, &f->jobs, f->ec, f->ac), tag_intro_after_out, f);
}

static Comp tag_intro_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Str message = str_lit("an injection takes one payload");
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_one_term(&f->args, message), tag_intro_after_payload, f);
}

static Comp tag_intro_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Nat *index = value;
  f->index = *index;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, f->dom), tag_intro_after_keep, f);
}

static Comp tag_intro_after_lty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Str message = str_lit("the leg address is outside the collection");
  Nat upper = length_nat(heap, &f->tys);
  MaybeNat bounded = big_to_nat_bounded(heap, f->raw_index, upper);
  Nat *boxed = bounded.some ? erase_util_nat_new(heap, bounded.value) : NULL;
  f->dom = value;
  if (bounded.some && boxed == NULL) return comp_oom();
  return bind_frame(heap, check_engine_optional(bounded.some, boxed, error_make(ERROR_MISMATCH, message)),
                    tag_intro_after_index, f);
}

static Comp tag_intro_after_tys(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *tys = value;
  Str message = str_lit("the leg address is outside the collection");
  VecGet got;
  f->tys = *tys;
  got = big_at(heap, &f->tys, f->raw_index);
  return bind_frame(heap,
                    check_engine_optional(got.status == VEC_GET_OK, got.value, error_make(ERROR_MISMATCH, message)),
                    tag_intro_after_lty, f);
}

static Comp tag_intro_after_raw(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *raw = value;
  f->raw_index = *raw;
  return bind_frame(heap, erase_repr_leg_types(heap, f->ec, f->count, f->clo), tag_intro_after_tys, f);
}

Comp erase_tag_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Big count, Closure clo, Addr addr,
                     const Vec *args) {
  Str message = str_lit("an injection takes a leg address");
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = ty;
  f->count = count;
  f->clo = clo;
  f->args = *args;
  return bind_frame(heap, erase_leg_index(heap, addr, message), tag_intro_after_raw, f);
}

Comp erase_in_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr,
                   const Vec *args) {
  const char *past = "an introduction at a shape past M0";
  const char *former = "an introduction stands at a type that is not a left former";
  Shape s;
  switch (value->kind) {
    case VALUE_LAN:
      s = value->shape;
      switch (s.kind) {
        case SHAPE_PI:
        case SHAPE_ZK:
          return erase_pair_intro(heap, ec, ac, ty, s.quantity, s.name, s.domain, value->diagram, addr, args);
        case SHAPE_COLL: return erase_tag_intro(heap, ec, ac, ty, s.count, value->diagram, addr, args);
        case SHAPE_ARG:
        case SHAPE_PAR:
        case SHAPE_MU:
        case SHAPE_NU:
        case SHAPE_FHC:
        case SHAPE_MPC: return mismatch(past);
      }
      return mismatch(past);
    case VALUE_UNIV:
    case VALUE_RAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch(former);
  }
  return mismatch(former);
}

/* in_typed */
static Comp in_typed_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_in_type(heap, value, f->ec, f->ac, f->ty, f->addr, &f->args);
}

Comp erase_in_typed(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = ty;
  f->addr = addr;
  f->args = *args;
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), in_typed_after_value, f);
}

/* position: the Bend recursion over names as a loop; a failed get ends the list. */
MaybeNat erase_position(Heap *heap, const Vec *names, Str name, Nat index) {
  MaybeNat out;
  Nat at = index;
  size_t i;
  for (i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    const Str *x;
    if (got.status != VEC_GET_OK) {
      out.some = false;
      out.value = nat_zero();
      return out;
    }
    x = got.value;
    if (str_eq(*x, name)) {
      out.some = true;
      out.value = at;
      return out;
    }
    at = heap_nat_succ(heap, at);
  }
  out.some = false;
  out.value = nat_zero();
  return out;
}

Comp erase_constructor(Heap *heap, Addr addr) {
  Str message = str_lit("a constructor takes a constructor address");
  switch (addr.kind) {
    case ADDR_CTOR: return pure_str(heap, addr.name);
    case ADDR_PT:
    case ADDR_LEG: return failed_mismatch(message);
  }
  return failed_mismatch(message);
}

bool erase_field_kept(const MaybeRepr *field) { return field->some; }

/* optional_field */
static Comp optional_field_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  Vec one = empty(heap);
  f->one = *out;
  push(heap, &one, (void *)erase_out_value(*out));
  return pure_outs(heap, one, erase_out_acc(*out));
}

Comp erase_optional_field(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, const Value *ty, const Term *t) {
  ExpectedTy expected;
  Frame *f;
  if (!keep) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  expected = erase_expected_some(ty);
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  return bind_frame(heap, erase_term(heap, ec, ac, false, expected, t), optional_field_after_out, f);
}

/* mu_fields: f->t is the head argument, f->keep its layout flag, f->outs the head outs. */
static Comp mu_fields_after_tail(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *tail = value;
  Vec here = erase_outs_values(f->outs);
  Vec more = erase_outs_values(*tail);
  Vec all = append(heap, &here, &more);
  return pure_outs(heap, all, erase_outs_acc(*tail));
}

static Comp mu_fields_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec env = cons(heap, value, &f->env);
  EraseAcc acc = erase_outs_acc(f->outs);
  return bind_frame(heap, erase_mu_fields(heap, &f->tele, &f->args, &f->layout, f->ec, acc, env),
                    mu_fields_after_tail, f);
}

static Comp mu_fields_after_here(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *here = value;
  f->outs = *here;
  return bind_frame(heap, erase_repr_eval(heap, f->ec, f->t), mu_fields_after_value, f);
}

static Comp mu_fields_after_tyv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_optional_field(heap, f->keep, f->ec, f->ac, value, f->t), mu_fields_after_here, f);
}

Comp erase_mu_fields(Heap *heap, const Vec *tele, const Vec *args, const Vec *layout, EraseCtx ec, EraseAcc ac,
                     Vec env) {
  const char *message = "a branch binds a different number of fields than its constructor takes";
  VecGet binder;
  VecGet arg;
  VecGet field;
  const TeleBinder *b;
  Frame *f;
  if (tele->len == 0 && args->len == 0 && layout->len == 0) {
    Vec none = empty(heap);
    return pure_outs(heap, none, ac);
  }
  if (tele->len == 0 || args->len == 0 || layout->len == 0) return mismatch(message);
  binder = vec_get(tele, 0);
  arg = vec_get(args, 0);
  field = vec_get(layout, 0);
  if (binder.status != VEC_GET_OK || arg.status != VEC_GET_OK || field.status != VEC_GET_OK) {
    return mismatch(message);
  }
  b = binder.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->env = env;
  f->t = arg.value;
  f->keep = erase_field_kept(field.value);
  f->tele = list_tail(*tele);
  f->args = list_tail(*args);
  f->layout = list_tail(*layout);
  return bind_frame(heap, erase_repr_eval_env(heap, ec, env, b->ty), mu_fields_after_tyv, f);
}

Vec erase_ctor_args(Ctor ctor) { return ctor.args; }

Vec erase_ctor_indices(Ctor ctor) { return ctor.res_idx; }

/* ---- Quarter 3: Bend lines 525-798, mu_tag .. eval_terms (law body app_jobs at 575) ---- */

static Comp pure_branch_out(Heap *heap, const KtmBranch *branch, EraseAcc acc) {
  EraseBranchOut *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->branch = branch;
  boxed->acc = acc;
  return budget_pure(boxed);
}

static Comp pure_branches(Heap *heap, Vec branches, EraseAcc acc) {
  EraseBranches *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->branches = branches;
  boxed->acc = acc;
  return budget_pure(boxed);
}

static Comp pure_mu_binders(Heap *heap, EraseCtx ctx, Vec env, Vec values, Nat arity) {
  EraseMuBinders *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->ctx = ctx;
  boxed->env = env;
  boxed->values = values;
  boxed->arity = arity;
  return budget_pure(boxed);
}

/* mu_tag */
static Comp mu_tag_after_group(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *group = value;
  Tid tid = erase_repr_mu_tid(heap, f->name);
  Vec values = erase_outs_values(f->outs);
  EraseAcc acc = erase_outs_acc(f->outs);
  const Ktm *node = eterm_ktag(heap, tid, f->index, values);
  EraseAcc grouped = erase_add_groups(heap, acc, group);
  return pure_out(heap, node, grouped);
}

static Comp mu_tag_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *out = value;
  f->outs = *out;
  return bind_frame(heap, erase_repr_mu_group_tids(heap, f->ec, f->name, f->family), mu_tag_after_group, f);
}

static Comp mu_tag_after_layout(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *layout = value;
  f->layout = *layout;
  f->tele = erase_ctor_args(f->ct);
  return bind_frame(heap, erase_mu_fields(heap, &f->tele, &f->args, &f->layout, f->ec, f->ac, f->penv),
                    mu_tag_after_out, f);
}

static Comp mu_tag_after_ct(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctor *ct = value;
  Ctx checker = erase_repr_checker(f->ec);
  VecResult penv;
  f->ct = *ct;
  penv = rules_mu_param_env(heap, checker, f->clo);
  switch (penv.kind) {
    case RESULT_FAIL: return budget_failed(penv.error);
    case RESULT_DONE: break;
  }
  f->penv = penv.value;
  return bind_frame(heap, erase_repr_mu_layout(heap, f->ec, f->family, f->ct), mu_tag_after_layout, f);
}

static Comp mu_tag_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Nat *index = value;
  MaybeCtor found;
  Ctor *boxed;
  f->index = *index;
  found = positivity_ctor_of(f->ctor, f->family);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = found.value;
  return bind_frame(heap, check_engine_optional(found.some, boxed, error_make(ERROR_UNBOUND, f->message)),
                    mu_tag_after_ct, f);
}

static Comp mu_tag_after_names(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *names = value;
  Str parts[3];
  Nat zero = nat_zero();
  MaybeNat found = erase_position(heap, names, f->ctor, zero);
  Nat *boxed;
  parts[0] = f->ctor;
  parts[1] = str_lit(" is not a constructor of ");
  parts[2] = f->name;
  f->message = cats(heap, 3, parts);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = found.value;
  return bind_frame(heap, check_engine_optional(found.some, boxed, error_make(ERROR_UNBOUND, f->message)),
                    mu_tag_after_index, f);
}

static Comp mu_tag_after_ctor(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Str *ctor = value;
  Ctx checker = erase_repr_checker(f->ec);
  FamilyResult family;
  f->ctor = *ctor;
  family = rules_mu_family(heap, checker, f->name);
  switch (family.kind) {
    case RESULT_FAIL: return budget_failed(family.error);
    case RESULT_DONE: break;
  }
  f->family = family.value;
  return bind_frame(heap, erase_repr_ctor_names(heap, f->family, f->name), mu_tag_after_names, f);
}

Comp erase_mu_tag(Heap *heap, EraseCtx ec, EraseAcc ac, Str name, Closure clo, Addr addr, const Vec *args) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->name = name;
  f->clo = clo;
  f->args = *args;
  return bind_frame(heap, erase_constructor(heap, addr), mu_tag_after_ctor, f);
}

/* mu_intro_type */
static Comp mu_intro_shape(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, Addr addr, const Vec *args) {
  switch (value->shape.kind) {
    case SHAPE_MU: return erase_mu_tag(heap, ec, ac, value->shape.name, value->diagram, addr, args);
    case SHAPE_PI:
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_NU:
    case SHAPE_ZK:
    case SHAPE_FHC:
    case SHAPE_MPC: return mismatch("an introduction at a shape past M0");
  }
  return mismatch("an introduction at a shape past M0");
}

Comp erase_mu_intro_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, Addr addr, const Vec *args) {
  switch (value->kind) {
    case VALUE_LAN: return mu_intro_shape(heap, value, ec, ac, addr, args);
    case VALUE_UNIV:
    case VALUE_RAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch("an introduction stands at a type that is not a left former");
  }
  return mismatch("an introduction stands at a type that is not a left former");
}

/* mu_intro */
static Comp mu_intro_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_mu_intro_type(heap, value, f->ec, f->ac, f->addr, &f->args);
}

Comp erase_mu_intro(Heap *heap, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->addr = addr;
  f->args = *args;
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), mu_intro_after_whnf, f);
}

/* spine: a loop over the T.Out nodes at a point shape with a point address. */
static bool spine_step(const Term *t) {
  if (t == NULL || t->kind != TERM_OUT || t->addr.kind != ADDR_PT) return false;
  return t->shape.kind == SHAPE_PI || t->shape.kind == SHAPE_ARG;
}

EraseSpine erase_spine(Heap *heap, const Term *t, Vec args) {
  EraseSpine out;
  const Term *cur = t;
  Vec acc = args;
  while (spine_step(cur)) {
    acc = cons(heap, (void *)cur->addr.arg, &acc);
    cur = cur->head;
  }
  out.head = cur;
  out.args = acc;
  return out;
}

/* app_point */
static Comp app_point_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  Vec first = erase_job(heap, f->keep, f->dom, f->arg);
  Vec all = append(heap, &first, more);
  return pure_vec(heap, all);
}

static Comp app_point_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, erase_app_jobs(heap, &f->rest, f->ec, value), app_point_after_more, f);
}

static Comp app_point_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec one = empty(heap);
  push(heap, &one, value);
  return bind_frame(heap, erase_repr_open(heap, f->ec, f->clo, one), app_point_after_cod, f);
}

static Comp app_point_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_eval(heap, f->ec, f->arg), app_point_after_value, f);
}

Comp erase_app_point(Heap *heap, EraseCtx ec, Quantity q, const Value *dom, Closure clo, const Term *arg,
                     const Vec *rest) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->q = q;
  f->dom = dom;
  f->clo = clo;
  f->arg = arg;
  f->rest = *rest;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, q, dom), app_point_after_keep, f);
}

/* app_type */
static Comp app_type_shape(Heap *heap, const Value *value, EraseCtx ec, const Term *arg, const Vec *rest) {
  const Value *dom = value->shape.domain;
  switch (value->shape.kind) {
    case SHAPE_PI:
    case SHAPE_ZK: return erase_app_point(heap, ec, value->shape.quantity, dom, value->diagram, arg, rest);
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_MU:
    case SHAPE_NU:
    case SHAPE_FHC:
    case SHAPE_MPC: return mismatch("an application needs a point function type");
  }
  return mismatch("an application needs a point function type");
}

Comp erase_app_type(Heap *heap, const Value *value, EraseCtx ec, const Term *arg, const Vec *rest) {
  switch (value->kind) {
    case VALUE_RAN: return app_type_shape(heap, value, ec, arg, rest);
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch("an application needs a point function type");
  }
  return mismatch("an application needs a point function type");
}

/* app_jobs (law body) */
static Comp app_jobs_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_app_type(heap, value, f->ec, f->arg, &f->rest);
}

Comp erase_app_jobs(Heap *heap, const Vec *args, EraseCtx ec, const Value *ty) {
  VecGet got = vec_get(args, 0);
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->arg = got.value;
  f->rest = list_tail(*args);
  return bind_frame(heap, erase_repr_whnf(heap, ec, ty), app_jobs_after_whnf, f);
}

Comp erase_app_bare(Heap *heap, const Vec *args, EraseCtx ec, const Value *ty) {
  if (args->len == 0) return erase_function_result(heap, ec, ty);
  return pure_bool(false);
}

EraseOut erase_app_result(Heap *heap, bool bare, bool tail, const Ktm *head, Vec args, EraseAcc ac) {
  if (bare) return erase_out(head, ac);
  if (tail) return erase_out(eterm_ktail(heap, head, args), ac);
  return erase_out(eterm_kapp(heap, head, args), ac);
}

/* app_spine */
static Comp app_spine_after_bare(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  bool bare = kernel_bool_unbox(value);
  Vec values = erase_outs_values(f->outs);
  EraseAcc acc = erase_outs_acc(f->outs);
  EraseOut out = erase_app_result(heap, bare, f->tail, erase_out_value(f->one), values, acc);
  return pure_out(heap, out.value, out.acc);
}

static Comp app_spine_after_args(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOuts *outs = value;
  Vec values;
  f->outs = *outs;
  values = erase_outs_values(f->outs);
  return bind_frame(heap, erase_app_bare(heap, &values, f->ec, f->ty), app_spine_after_bare, f);
}

static Comp app_spine_after_head(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *head = value;
  EraseAcc acc;
  f->one = *head;
  acc = erase_out_acc(f->one);
  return bind_frame(heap, erase_erase_fold(heap, &f->jobs, f->ec, acc), app_spine_after_args, f);
}

static Comp app_spine_after_jobs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *jobs = value;
  ExpectedTy expected = erase_expected_some(f->hty);
  f->jobs = *jobs;
  return bind_frame(heap, erase_term(heap, f->ec, f->ac, false, expected, f->head), app_spine_after_head, f);
}

static Comp app_spine_after_hty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->hty = value;
  return bind_frame(heap, erase_app_jobs(heap, &f->args, f->ec, value), app_spine_after_jobs, f);
}

Comp erase_app_spine(Heap *heap, EraseSpine parts, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty) {
  Frame *f;
  if (parts.args.len == 0) return mismatch("an application at a point shape with no point address");
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->head = parts.head;
  f->args = parts.args;
  return bind_frame(heap, erase_repr_infer(heap, ec, parts.head), app_spine_after_hty, f);
}

Comp erase_app_arm(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Term *t) {
  Vec none = empty(heap);
  EraseSpine parts = erase_spine(heap, t, none);
  return erase_app_spine(heap, parts, ec, ac, tail, ty);
}

/* flags_prefix: the count of kept legs whose index is below the limit. */
Nat erase_flags_prefix(Heap *heap, const Vec *flags, Big limit, Nat index) {
  Nat total = nat_zero();
  Nat ix = index;
  size_t i;
  for (i = 0; i < flags->len; i++) {
    VecGet got = vec_get(flags, i);
    bool flag = got.status == VEC_GET_OK && kernel_bool_unbox(got.value);
    Big here = big_of_nat(heap, ix);
    Cmp order = big_compare(limit, here);
    bool past = big_cmp_le(order);
    if (flag && !past) total = heap_nat_succ(heap, total);
    ix = heap_nat_succ(heap, ix);
  }
  return total;
}

Nat erase_flags_before(Heap *heap, const Vec *flags, Big count) {
  Nat zero = nat_zero();
  return erase_flags_prefix(heap, flags, count, zero);
}

/* proj_type */
static Comp proj_type_after_tid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Tid *tid = value;
  Nat index = erase_flags_before(heap, &f->flags, f->raw_index);
  const Ktm *node = eterm_kproj(heap, *tid, index, erase_out_value(f->one));
  EraseAcc acc = erase_out_acc(f->one);
  return pure_out(heap, node, acc);
}

static Comp proj_type_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  f->one = *out;
  return bind_frame(heap, erase_repr_tid_of(heap, f->ec, f->ty), proj_type_after_tid, f);
}

static Comp proj_type_after_flags(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *flags = value;
  ExpectedTy expected = erase_expected_some(f->ty);
  f->flags = *flags;
  return bind_frame(heap, erase_term(heap, f->ec, f->ac, false, expected, f->head), proj_type_after_out, f);
}

Comp erase_proj_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, const Value *sty, Big n, Big index,
                     const Term *head) {
  Frame *f;
  switch (value->kind) {
    case VALUE_RAN: break;
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch("a projection stands on a value that is not a right former");
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->ty = sty;
  f->count = n;
  f->raw_index = index;
  f->head = head;
  return bind_frame(heap, erase_repr_leg_flags(heap, ec, n, value->diagram), proj_type_after_flags, f);
}

/* proj_arm */
static Comp proj_arm_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_proj_type(heap, value, f->ec, f->ac, f->ty, f->count, f->raw_index, f->head);
}

static Comp proj_arm_after_sty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_whnf(heap, f->ec, value), proj_arm_after_whnf, f);
}

static Comp proj_arm_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *index = value;
  f->raw_index = *index;
  return bind_frame(heap, erase_repr_infer(heap, f->ec, f->head), proj_arm_after_sty, f);
}

Comp erase_proj_arm(Heap *heap, EraseCtx ec, EraseAcc ac, Big n, Addr addr, const Term *head) {
  Str message = str_lit("a projection takes a leg address");
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->count = n;
  f->head = head;
  return bind_frame(heap, erase_leg_index(heap, addr, message), proj_arm_after_index, f);
}

/* let_runtime */
static Comp let_runtime_after_body(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *body = value;
  EraseAcc acc = erase_out_acc(*body);
  const Ktm *node = eterm_klet(heap, f->name, erase_out_value(f->one), erase_out_value(*body));
  return pure_out(heap, node, acc);
}

static Comp let_runtime_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  ExpectedTy expected = erase_expected_some(f->ty);
  EraseAcc acc;
  f->one = *out;
  acc = erase_out_acc(f->one);
  return bind_frame(heap, erase_term(heap, f->next, acc, f->tail, expected, f->body), let_runtime_after_body, f);
}

Comp erase_let_runtime(Heap *heap, bool keep, EraseCtx ec, EraseCtx next, EraseAcc ac, bool tail, const Value *ty,
                       Str name, const Value *lty, const Term *value, const Term *body) {
  Frame *f;
  ExpectedTy expected;
  if (!keep) {
    expected = erase_expected_some(ty);
    return erase_term(heap, next, ac, tail, expected, body);
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->next = next;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->name = name;
  f->body = body;
  expected = erase_expected_some(lty);
  return bind_frame(heap, erase_term(heap, ec, ac, false, expected, value), let_runtime_after_value, f);
}

/* let_arm */
static Comp let_arm_after_vv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Slot slot = erase_repr_keep_slot(f->keep);
  EraseCtx next = erase_repr_define(heap, f->ec, f->name, QUANTITY_MANY, f->lty, value, slot);
  return erase_let_runtime(heap, f->keep, f->ec, next, f->ac, f->tail, f->ty, f->name, f->lty, f->value_t, f->body);
}

static Comp let_arm_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->keep = kernel_bool_unbox(value);
  return bind_frame(heap, erase_repr_eval(heap, f->ec, f->value_t), let_arm_after_vv, f);
}

static Comp let_arm_after_lty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->lty = value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, value), let_arm_after_keep, f);
}

Comp erase_let_arm(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, Str name, const Term *lty,
                   const Term *value, const Term *body) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->name = name;
  f->value_t = value;
  f->body = body;
  return bind_frame(heap, erase_repr_eval(heap, ec, lty), let_arm_after_lty, f);
}

Shape erase_elim_shape(const ElimData *data) { return data->shape; }

/* pair_branch: the one branch of a pair elimination; the Comp carries a Leg pointer into it. */
Comp erase_pair_branch(const Vec *branches) {
  VecGet got;
  Branch *branch;
  if (branches->len != 1) return mismatch("a pair elimination takes one branch");
  got = vec_get(branches, 0);
  if (got.status != VEC_GET_OK) return mismatch("a pair elimination takes one branch");
  branch = got.value;
  return budget_pure(&branch->leg);
}

Comp erase_second_binder(Heap *heap, const Leg *leg) {
  VecGet got;
  const Binder *binder;
  if (leg->binders.len != 2) return mismatch("a pair branch takes two binders");
  got = vec_get(&leg->binders, 1);
  if (got.status != VEC_GET_OK) return mismatch("a pair branch takes two binders");
  binder = got.value;
  return pure_str(heap, binder->name);
}

const Term *erase_leg_body(const Leg *leg) { return leg->body; }

const Ktm *erase_project_let(Heap *heap, bool keep, Str name, Tid tid, Nat index, const Ktm *body) {
  const Ktm *var;
  const Ktm *proj;
  if (!keep) return body;
  var = eterm_kvar(heap, index);
  proj = eterm_kproj(heap, tid, index, var);
  return eterm_klet(heap, name, proj, body);
}

/* pair_elim */
static Comp pair_elim_after_tid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Tid *tid = value;
  Nat zero = nat_zero();
  Nat one = heap_nat_succ(heap, zero);
  Nat first = f->keep ? one : zero;
  Str scrut_name = str_lit("scrut");
  const Ktm *inner2 = erase_project_let(heap, f->keep2, f->name2, *tid, first, erase_out_value(f->two));
  const Ktm *inner1 = erase_project_let(heap, f->keep, f->name, *tid, zero, inner2);
  const Ktm *node = eterm_klet(heap, scrut_name, erase_out_value(f->one), inner1);
  EraseAcc acc = erase_out_acc(f->two);
  return pure_out(heap, node, acc);
}

static Comp pair_elim_after_body(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *body = value;
  f->two = *body;
  return bind_frame(heap, erase_repr_tid_of(heap, f->ec, f->sty), pair_elim_after_tid, f);
}

static Comp pair_elim_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ExpectedTy expected = erase_expected_some(value);
  EraseAcc acc = erase_out_acc(f->one);
  return bind_frame(heap, erase_term(heap, f->ec2, acc, f->tail, expected, erase_leg_body(f->leg)),
                    pair_elim_after_body, f);
}

static Comp pair_elim_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *scrut = value;
  Shape shape = erase_elim_shape(f->data);
  Globals globals = erase_repr_globals(f->ec);
  Vec env = erase_repr_env(f->ec);
  ShapeResult evaluated;
  Nat size;
  Nat size1;
  const Value *v0;
  const Value *v1;
  VAddr addr;
  Vec self_args;
  const Value *self;
  ExpectedValue expected;
  MaybeMotive motive;
  Ctx checker;
  f->one = *scrut;
  evaluated = eval_eval_shape(heap, shape, globals, env);
  switch (evaluated.kind) {
    case RESULT_FAIL: return budget_failed(evaluated.error);
    case RESULT_DONE: break;
  }
  size = erase_repr_size(f->ec);
  size1 = heap_nat_succ(heap, size);
  v0 = value_var(heap, size);
  v1 = value_var(heap, size1);
  addr = value_vaddr_pt(f->q, v0);
  self_args = empty(heap);
  push(heap, &self_args, (void *)v1);
  self = value_vin(heap, evaluated.value, addr, self_args);
  expected.some = true;
  expected.value = f->ty;
  motive = check_engine_elim_motive(f->data);
  checker = erase_repr_checker(f->ec);
  return bind_frame(heap, check_engine_elim_result(heap, motive, expected, checker, self), pair_elim_after_target, f);
}

static Comp pair_elim_after_keep2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ExpectedTy expected = erase_expected_some(f->sty);
  Slot slot;
  f->keep2 = kernel_bool_unbox(value);
  slot = erase_repr_keep_slot(f->keep2);
  f->ec2 = erase_repr_bind(heap, f->ec1, f->name2, QUANTITY_MANY, f->cod, slot);
  return bind_frame(heap, erase_term(heap, f->ec, f->ac, false, expected, check_engine_elim_scrut(f->data)),
                    pair_elim_after_scrut, f);
}

static Comp pair_elim_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  EraseCtx extra = erase_repr_extra(heap, f->ec);
  Slot slot = erase_repr_keep_slot(f->keep);
  f->cod = value;
  f->ec1 = erase_repr_bind(heap, extra, f->name, f->q, f->dom, slot);
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec1, f->cod), pair_elim_after_keep2, f);
}

static Comp pair_elim_after_keep1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Nat size = erase_repr_size(f->ec);
  const Value *var = value_var(heap, size);
  Vec args = empty(heap);
  f->keep = kernel_bool_unbox(value);
  push(heap, &args, (void *)var);
  return bind_frame(heap, erase_repr_open(heap, f->ec, f->clo, args), pair_elim_after_cod, f);
}

static Comp pair_elim_after_name2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Str *name2 = value;
  f->name2 = *name2;
  return bind_frame(heap, erase_repr_point_runtime(heap, f->ec, f->q, f->dom), pair_elim_after_keep1, f);
}

static Comp pair_elim_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->leg = value;
  return bind_frame(heap, erase_second_binder(heap, f->leg), pair_elim_after_name2, f);
}

Comp erase_pair_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Value *sty,
                     const ElimData *data, Quantity q, Str name, const Value *dom, Closure clo) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->sty = sty;
  f->data = data;
  f->q = q;
  f->name = name;
  f->dom = dom;
  f->clo = clo;
  return bind_frame(heap, erase_pair_branch(check_engine_elim_branches(data)), pair_elim_after_leg, f);
}

const KtmBranch *erase_branch_value(EraseBranchOut out) { return out.branch; }

EraseAcc erase_branch_acc(EraseBranchOut out) { return out.acc; }

Vec erase_branches_values(EraseBranches out) { return out.branches; }

EraseAcc erase_branches_acc(EraseBranches out) { return out.acc; }

/* find_leg: the first ALeg branch at the index. */
EraseMaybeLeg erase_find_leg(Heap *heap, const Vec *branches, Nat index) {
  EraseMaybeLeg out;
  Big wanted = big_of_nat(heap, index);
  size_t i;
  out.some = false;
  out.value = NULL;
  for (i = 0; i < branches->len; i++) {
    VecGet got = vec_get(branches, i);
    const Branch *branch = got.value;
    if (got.status != VEC_GET_OK || branch->addr.kind != ADDR_LEG) continue;
    if (!big_equal(branch->addr.index, wanted)) continue;
    out.some = true;
    out.value = &branch->leg;
    return out;
  }
  return out;
}

/* case_leg */
static Comp case_leg_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  Nat zero = nat_zero();
  Nat one = heap_nat_succ(heap, zero);
  Nat arity = f->keep ? one : zero;
  KtmBranch *branch = eterm_branch_new(heap, f->index, arity, erase_out_value(*out));
  EraseAcc acc = erase_out_acc(*out);
  return pure_branch_out(heap, branch, acc);
}

static Comp case_leg_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ExpectedTy expected = erase_expected_some(value);
  return bind_frame(heap, erase_term(heap, f->next, f->ac, f->tail, expected, f->leg->body), case_leg_after_out, f);
}

static Comp case_leg_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Shape shape = shape_scoll(f->count);
  VAddr addr = value_valeg(heap, f->index);
  Nat size = erase_repr_size(f->ec);
  const Value *var = value_var(heap, size);
  Vec args = empty(heap);
  Slot slot;
  const Value *self;
  ExpectedValue expected;
  MaybeMotive motive;
  Ctx checker;
  f->keep = kernel_bool_unbox(value);
  slot = erase_repr_keep_slot(f->keep);
  f->next = erase_repr_bind(heap, f->ec, f->name, f->q, f->lty, slot);
  push(heap, &args, (void *)var);
  self = value_vin(heap, shape, addr, args);
  expected.some = true;
  expected.value = f->ty;
  motive = check_engine_elim_motive(f->data);
  checker = erase_repr_checker(f->ec);
  return bind_frame(heap, check_engine_elim_result(heap, motive, expected, checker, self), case_leg_after_target, f);
}

Comp erase_case_leg(Heap *heap, const Leg *leg, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                    const ElimData *data, Big count, Nat index, const Value *lty) {
  VecGet got;
  const Binder *binder;
  Frame *f;
  if (leg->binders.len != 1) return mismatch("a case branch takes one binder");
  got = vec_get(&leg->binders, 0);
  if (got.status != VEC_GET_OK) return mismatch("a case branch takes one binder");
  binder = got.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->leg = leg;
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->data = data;
  f->count = count;
  f->index = index;
  f->lty = lty;
  f->q = binder->quantity;
  f->name = binder->name;
  return bind_frame(heap, erase_repr_point_runtime(heap, ec, binder->quantity, lty), case_leg_after_keep, f);
}

/* case_branches */
static Comp case_branches_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranches *more = value;
  const KtmBranch *first = erase_branch_value(f->bout);
  Vec all = cons(heap, (void *)first, &more->branches);
  EraseAcc acc = erase_branches_acc(*more);
  return pure_branches(heap, all, acc);
}

static Comp case_branches_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranchOut *out = value;
  Nat next;
  EraseAcc acc;
  f->bout = *out;
  next = heap_nat_succ(heap, f->index);
  acc = erase_branch_acc(f->bout);
  return bind_frame(heap, erase_case_branches(heap, &f->tys, f->ec, acc, f->tail, f->ty, f->data, f->count, next),
                    case_branches_after_more, f);
}

static Comp case_branches_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    erase_case_leg(heap, value, f->ec, f->ac, f->tail, f->ty, f->data, f->count, f->index, f->lty),
                    case_branches_after_out, f);
}

Comp erase_case_branches(Heap *heap, const Vec *types, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                         const ElimData *data, Big count, Nat index) {
  VecGet got = vec_get(types, 0);
  Frame *f;
  EraseMaybeLeg found;
  Str message;
  if (got.status != VEC_GET_OK) return pure_branches(heap, empty(heap), ac);
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->lty = got.value;
  f->tys = list_tail(*types);
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->data = data;
  f->count = count;
  f->index = index;
  found = erase_find_leg(heap, check_engine_elim_branches(data), index);
  message = str_lit("a case is missing a branch");
  return bind_frame(heap,
                    check_engine_optional(found.some, (void *)found.value, error_make(ERROR_MISMATCH, message)),
                    case_branches_after_leg, f);
}

/* case_elim */
static Comp case_elim_after_tid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Tid *tid = value;
  Vec branches = erase_branches_values(f->brs);
  EraseAcc acc = erase_branches_acc(f->brs);
  const Ktm *node = eterm_kcase(heap, *tid, erase_out_value(f->one), branches);
  return pure_out(heap, node, acc);
}

static Comp case_elim_after_branches(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranches *brs = value;
  f->brs = *brs;
  return bind_frame(heap, erase_repr_tid_of(heap, f->ec, f->sty), case_elim_after_tid, f);
}

static Comp case_elim_after_tys(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *tys = value;
  Nat zero = nat_zero();
  EraseAcc acc = erase_out_acc(f->one);
  f->tys = *tys;
  return bind_frame(heap, erase_case_branches(heap, &f->tys, f->ec, acc, f->tail, f->ty, f->data, f->count, zero),
                    case_elim_after_branches, f);
}

static Comp case_elim_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *scrut = value;
  f->one = *scrut;
  return bind_frame(heap, erase_repr_leg_types(heap, f->ec, f->count, f->clo), case_elim_after_tys, f);
}

Comp erase_case_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Value *sty,
                     const ElimData *data, Big count, Closure clo) {
  ExpectedTy expected = erase_expected_some(sty);
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->sty = sty;
  f->data = data;
  f->count = count;
  f->clo = clo;
  return bind_frame(heap, erase_term(heap, ec, ac, false, expected, check_engine_elim_scrut(data)),
                    case_elim_after_scrut, f);
}

/* elim_type */
static Comp elim_type_shape(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                            const Value *sty, const ElimData *data) {
  Shape s = value->shape;
  const Value *dom = s.domain;
  switch (s.kind) {
    case SHAPE_PI:
    case SHAPE_ZK: return erase_pair_elim(heap, ec, ac, tail, ty, sty, data, s.quantity, s.name, dom, value->diagram);
    case SHAPE_COLL: return erase_case_elim(heap, ec, ac, tail, ty, sty, data, s.count, value->diagram);
    case SHAPE_ARG:
    case SHAPE_PAR:
    case SHAPE_MU:
    case SHAPE_NU:
    case SHAPE_FHC:
    case SHAPE_MPC: return mismatch("an elimination at a shape past M0");
  }
  return mismatch("an elimination at a shape past M0");
}

Comp erase_elim_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                     const Value *sty, const ElimData *data) {
  switch (value->kind) {
    case VALUE_LAN: return elim_type_shape(heap, value, ec, ac, tail, ty, sty, data);
    case VALUE_UNIV:
    case VALUE_RAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch("an elimination stands on a value that is not a left former");
  }
  return mismatch("an elimination stands on a value that is not a left former");
}

/* elim_typed */
static Comp elim_typed_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_elim_type(heap, value, f->ec, f->ac, f->tail, f->ty, f->sty, f->data);
}

static Comp elim_typed_after_sty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->sty = value;
  return bind_frame(heap, erase_repr_whnf(heap, f->ec, value), elim_typed_after_whnf, f);
}

Comp erase_elim_typed(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const ElimData *data) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->ty = ty;
  f->data = data;
  return bind_frame(heap, erase_repr_infer(heap, ec, check_engine_elim_scrut(data)), elim_typed_after_sty, f);
}

/* mu_binders: the three lists walk together; a length mismatch is Error.Missing_branch. */
static Comp mu_binders_after_tyv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Nat size = erase_repr_size(f->ec);
  const Value *var = value_var(heap, size);
  Slot slot = erase_repr_keep_slot(f->keep);
  EraseCtx next = erase_repr_bind(heap, f->ec, f->name, f->q, value, slot);
  Vec env = cons(heap, (void *)var, &f->env);
  Vec values = cons(heap, (void *)var, &f->values);
  Nat zero = nat_zero();
  Nat one = heap_nat_succ(heap, zero);
  Nat step = f->keep ? one : zero;
  Nat arity = heap_nat_add(heap, f->arity, step);
  return erase_mu_binders(heap, &f->tele, &f->binders, &f->layout, next, env, values, arity);
}

Comp erase_mu_binders(Heap *heap, const Vec *tele, const Vec *binders, const Vec *layout, EraseCtx ec, Vec env,
                      Vec values, Nat arity) {
  VecGet t0 = vec_get(tele, 0);
  VecGet b0 = vec_get(binders, 0);
  VecGet l0 = vec_get(layout, 0);
  size_t present = (t0.status == VEC_GET_OK) + (b0.status == VEC_GET_OK) + (l0.status == VEC_GET_OK);
  const TeleBinder *tb;
  const Binder *bd;
  const MaybeRepr *field;
  Frame *f;
  Str message;
  if (present == 0) return pure_mu_binders(heap, ec, env, values, arity);
  if (present != 3) {
    message = str_lit("a branch binds a different number of fields than its constructor takes");
    return budget_failed(error_make(ERROR_MISSING_BRANCH, message));
  }
  tb = t0.value;
  bd = b0.value;
  field = l0.value;
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->q = tb->quantity;
  f->name = bd->name;
  f->keep = erase_field_kept(field);
  f->tele = list_tail(*tele);
  f->binders = list_tail(*binders);
  f->layout = list_tail(*layout);
  f->ec = ec;
  f->env = env;
  f->values = values;
  f->arity = arity;
  return bind_frame(heap, erase_repr_eval_env(heap, ec, env, tb->ty), mu_binders_after_tyv, f);
}

/* find_ctor_branch: the first ACtor branch with the name. */
EraseMaybeLeg erase_find_ctor_branch(const Vec *branches, Str name) {
  EraseMaybeLeg out;
  size_t i;
  out.some = false;
  out.value = NULL;
  for (i = 0; i < branches->len; i++) {
    VecGet got = vec_get(branches, i);
    const Branch *branch = got.value;
    if (got.status != VEC_GET_OK || branch->addr.kind != ADDR_CTOR) continue;
    if (!str_eq(branch->addr.name, name)) continue;
    out.some = true;
    out.value = &branch->leg;
    return out;
  }
  return out;
}

Vec erase_leg_binders(const Leg *leg) { return leg->binders; }

/* eval_terms */
static Comp eval_terms_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec all = cons(heap, (void *)f->val, value);
  return pure_vec(heap, all);
}

static Comp eval_terms_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->val = value;
  return bind_frame(heap, erase_eval_terms(heap, &f->terms, f->ec, f->env), eval_terms_after_more, f);
}

Comp erase_eval_terms(Heap *heap, const Vec *terms, EraseCtx ec, Vec env) {
  VecGet got = vec_get(terms, 0);
  Frame *f;
  if (got.status != VEC_GET_OK) return pure_vec(heap, empty(heap));
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->terms = list_tail(*terms);
  f->ec = ec;
  f->env = env;
  return bind_frame(heap, erase_repr_eval_env(heap, ec, env, got.value), eval_terms_after_value, f);
}

/* Quarter 4a: Bend lines 799-923 (mu_branch_body .. term, the law body term at Bend 918).
   New Frame fields for the merge (after `EraseBranches brs;`):
     Motive motive; Vec idx; Vec names; Vec elim_branches; */

static Comp not_yet(const char *message) {
  Str text = str_lit(message);
  return budget_failed(error_make(ERROR_NOT_YET, text));
}

/* mu_branch_body: the do block binds idx, target and out in turn. */
static Comp mu_branch_body_after_out(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *out = value;
  const Ktm *body = erase_out_value(*out);
  EraseAcc acc = erase_out_acc(*out);
  KtmBranch *branch = eterm_branch_new(heap, f->index, f->arity, body);
  if (branch == NULL) return comp_oom();
  return pure_branch_out(heap, branch, acc);
}

static Comp mu_branch_body_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *target = value;
  ExpectedTy expected = erase_expected_some(target);
  return bind_frame(heap, erase_term(heap, f->next, f->ac, f->tail, expected, f->body), mu_branch_body_after_out, f);
}

static Comp mu_branch_body_after_idx(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *idx = value;
  Vec reversed = vec_reverse(heap->arena, &f->values);
  Shape shape = shape_smu(f->name, *idx);
  VAddr addr = value_vaddr_ctor(f->ctor);
  const Value *self = value_vin(heap, shape, addr, reversed);
  Ctx checker = erase_repr_checker(f->ec);
  if (self == NULL) return comp_oom();
  f->idx = *idx;
  return bind_frame(heap, check_engine_mu_result(heap, checker, f->motive, f->idx, self), mu_branch_body_after_target,
                    f);
}

Comp erase_mu_branch_body(Heap *heap, const EraseMuBinders *bound, EraseCtx ec, EraseAcc ac, bool tail, Str name,
                          Motive motive, Nat index, Str ctor, const Vec *indices, const Term *body) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->next = bound->ctx;
  f->env = bound->env;
  f->values = bound->values;
  f->arity = bound->arity;
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->name = name;
  f->motive = motive;
  f->index = index;
  f->ctor = ctor;
  f->body = body;
  return bind_frame(heap, erase_eval_terms(heap, indices, ec, f->env), mu_branch_body_after_idx, f);
}

/* mu_branch_of: ct, leg, layout and bound bind in turn; the two K.optional sites carry Error.Unbound and
   Error.Missing_branch with the Bend messages byte for byte. */
static Comp mu_branch_of_after_bound(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseMuBinders *bound = value;
  const Term *body = erase_leg_body(f->leg);
  f->terms = erase_ctor_indices(f->ct);
  return erase_mu_branch_body(heap, bound, f->ec, f->ac, f->tail, f->name, f->motive, f->index, f->ctor, &f->terms,
                              body);
}

static Comp mu_branch_of_after_layout(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *layout = value;
  Vec none = empty(heap);
  Nat zero = nat_zero();
  f->layout = *layout;
  f->tele = erase_ctor_args(f->ct);
  f->binders = erase_leg_binders(f->leg);
  return bind_frame(heap, erase_mu_binders(heap, &f->tele, &f->binders, &f->layout, f->ec, f->penv, none, zero),
                    mu_branch_of_after_bound, f);
}

static Comp mu_branch_of_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->leg = value;
  return bind_frame(heap, erase_repr_mu_layout(heap, f->ec, f->family, f->ct), mu_branch_of_after_layout, f);
}

static Comp mu_branch_of_after_ct(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctor *ct = value;
  Str parts[4];
  Str message;
  EraseMaybeLeg found = erase_find_ctor_branch(&f->elim_branches, f->ctor);
  f->ct = *ct;
  parts[0] = str_lit("the elimination of ");
  parts[1] = f->name;
  parts[2] = str_lit(" has no branch at ");
  parts[3] = f->ctor;
  message = cats(heap, 4, parts);
  return bind_frame(heap,
                    check_engine_optional(found.some, (void *)found.value, error_make(ERROR_MISSING_BRANCH, message)),
                    mu_branch_of_after_leg, f);
}

Comp erase_mu_branch_of(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, Str name, Family family, Motive motive,
                        Vec penv, const Vec *branches, Nat index, Str ctor) {
  Frame *f = frame_new(heap);
  MaybeCtor found;
  Ctor *boxed;
  Str parts[3];
  Str message;
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->name = name;
  f->family = family;
  f->motive = motive;
  f->penv = penv;
  f->elim_branches = *branches;
  f->index = index;
  f->ctor = ctor;
  found = positivity_ctor_of(ctor, family);
  boxed = found.some ? heap_alloc(heap, sizeof *boxed) : NULL;
  if (found.some && boxed == NULL) return comp_oom();
  if (found.some) *boxed = found.value;
  parts[0] = ctor;
  parts[1] = str_lit(" is not a constructor of ");
  parts[2] = name;
  message = cats(heap, 3, parts);
  return bind_frame(heap, check_engine_optional(found.some, boxed, error_make(ERROR_UNBOUND, message)),
                    mu_branch_of_after_ct, f);
}

/* mu_branches: one branch per constructor name, the accumulator threads through. */
static Comp mu_branches_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranches *more = value;
  Vec values = cons(heap, (void *)f->bout.branch, &more->branches);
  EraseAcc acc = erase_branches_acc(*more);
  return pure_branches(heap, values, acc);
}

static Comp mu_branches_after_one(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranchOut *one = value;
  Nat next = heap_nat_succ(heap, f->index);
  EraseAcc acc = erase_branch_acc(*one);
  f->bout = *one;
  return bind_frame(heap,
                    erase_mu_branches(heap, &f->rest, next, f->ec, acc, f->tail, f->name, f->family, f->motive, f->penv,
                                      &f->elim_branches),
                    mu_branches_after_more, f);
}

Comp erase_mu_branches(Heap *heap, const Vec *names, Nat index, EraseCtx ec, EraseAcc ac, bool tail, Str name,
                       Family family, Motive motive, Vec penv, const Vec *branches) {
  VecGet got = vec_get(names, 0);
  Frame *f;
  const Str *ctor;
  switch (got.status) {
    case VEC_GET_OUT_OF_RANGE: {
      Vec none = empty(heap);
      return pure_branches(heap, none, ac);
    }
    case VEC_GET_OK: break;
  }
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  ctor = got.value;
  f->rest = list_tail(*names);
  f->index = index;
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->name = name;
  f->family = family;
  f->motive = motive;
  f->penv = penv;
  f->elim_branches = *branches;
  return bind_frame(heap, erase_mu_branch_of(heap, ec, ac, tail, name, family, motive, penv, branches, index, *ctor),
                    mu_branches_after_one, f);
}

/* mu_case: family and penv are lifted Rules results; motive, names, scrut, brs and group bind in turn. */
static Comp mu_case_after_group(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *group = value;
  Tid tid = erase_repr_mu_tid(heap, f->name);
  Vec branches = erase_branches_values(f->brs);
  EraseAcc acc = erase_branches_acc(f->brs);
  EraseAcc grouped = erase_add_groups(heap, acc, group);
  const Ktm *scrut = erase_out_value(f->one);
  const Ktm *kcase = eterm_kcase(heap, tid, scrut, branches);
  if (kcase == NULL) return comp_oom();
  return pure_out(heap, kcase, grouped);
}

static Comp mu_case_after_brs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseBranches *brs = value;
  f->brs = *brs;
  return bind_frame(heap, erase_repr_mu_group_tids(heap, f->ec, f->name, f->family), mu_case_after_group, f);
}

static Comp mu_case_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseOut *scrut = value;
  Nat zero = nat_zero();
  EraseAcc acc = erase_out_acc(*scrut);
  const Vec *branches = check_engine_elim_branches(f->data);
  f->one = *scrut;
  return bind_frame(heap,
                    erase_mu_branches(heap, &f->names, zero, f->ec, acc, f->tail, f->name, f->family, f->motive,
                                      f->penv, branches),
                    mu_case_after_brs, f);
}

static Comp mu_case_after_names(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *names = value;
  Ctx checker = erase_repr_checker(f->ec);
  VecResult penv = rules_mu_param_env(heap, checker, f->clo);
  ExpectedTy expected;
  f->names = *names;
  switch (penv.kind) {
    case RESULT_FAIL: return budget_failed(penv.error);
    case RESULT_DONE: break;
  }
  f->penv = penv.value;
  expected = erase_expected_some(f->sty);
  return bind_frame(heap, erase_term(heap, f->ec, f->ac, false, expected, check_engine_elim_scrut(f->data)),
                    mu_case_after_scrut, f);
}

static Comp mu_case_after_motive(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Motive *motive = value;
  f->motive = *motive;
  return bind_frame(heap, erase_repr_ctor_names(heap, f->family, f->name), mu_case_after_names, f);
}

Comp erase_mu_case(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const Value *sty, const ElimData *data, Str name,
                   Vec idx, Closure clo) {
  Frame *f = frame_new(heap);
  Ctx checker;
  FamilyResult family;
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->sty = sty;
  f->data = data;
  f->name = name;
  f->idx = idx;
  f->clo = clo;
  checker = erase_repr_checker(ec);
  family = rules_mu_family(heap, checker, name);
  switch (family.kind) {
    case RESULT_FAIL: return budget_failed(family.error);
    case RESULT_DONE: break;
  }
  f->family = family.value;
  return bind_frame(heap, check_engine_mu_motive_of(heap, name, f->family, &f->idx, data), mu_case_after_motive, f);
}

/* mu_elim_type: only a left former at a mu shape eliminates here. */
static Comp mu_elim_shape(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *sty,
                          const ElimData *data) {
  Shape s = value->shape;
  switch (s.kind) {
    case SHAPE_MU: return erase_mu_case(heap, ec, ac, tail, sty, data, s.name, s.args, value->diagram);
    case SHAPE_PI:
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_NU:
    case SHAPE_ZK:
    case SHAPE_FHC:
    case SHAPE_MPC: return mismatch("an elimination at a shape past M0");
  }
  return mismatch("an elimination at a shape past M0");
}

Comp erase_mu_elim_type(Heap *heap, const Value *value, EraseCtx ec, EraseAcc ac, bool tail, const Value *sty,
                        const ElimData *data) {
  switch (value->kind) {
    case VALUE_LAN: return mu_elim_shape(heap, value, ec, ac, tail, sty, data);
    case VALUE_UNIV:
    case VALUE_RAN:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL: return mismatch("an elimination stands on a value that is not a left former");
  }
  return mismatch("an elimination stands on a value that is not a left former");
}

/* mu_elim: infer the scrutinee type, whnf it, then dispatch on the value. */
static Comp mu_elim_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return erase_mu_elim_type(heap, value, f->ec, f->ac, f->tail, f->sty, f->data);
}

static Comp mu_elim_after_sty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->sty = value;
  return bind_frame(heap, erase_repr_whnf(heap, f->ec, f->sty), mu_elim_after_value, f);
}

Comp erase_mu_elim(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, const ElimData *data) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->data = data;
  return bind_frame(heap, erase_repr_infer(heap, ec, check_engine_elim_scrut(data)), mu_elim_after_sty, f);
}

/* sec_arm: the right former arms by shape. */
Comp erase_sec_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, const Value *ty, const Vec *legs,
                   const Term *t) {
  switch (shape.kind) {
    case SHAPE_PI: return erase_lift_arm(heap, ec, ac, ty, t);
    case SHAPE_ARG: return mismatch("an application frame shape has no introduction");
    case SHAPE_COLL: return erase_tuple_arm(heap, ec, ac, ty, shape.count, legs);
    case SHAPE_PAR: return not_yet("SPar arrives at M1");
    case SHAPE_MU: return not_yet("a right former at a mu shape arrives at M2");
    case SHAPE_NU: return not_yet("SNu arrives at M2");
    case SHAPE_ZK: return not_yet("Ran SZk arrives at V5");
    case SHAPE_FHC: return erase_fhc_sec(heap, ec, ac, shape.level, legs);
    case SHAPE_MPC: return erase_mpc_sec(heap, ec, ac, legs);
  }
  return erase_mpc_sec(heap, ec, ac, legs);
}

/* in_arm: the left former introduction arms by shape. */
Comp erase_in_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, const Value *ty, Addr addr, const Vec *args) {
  switch (shape.kind) {
    case SHAPE_PI: return erase_in_typed(heap, ec, ac, ty, addr, args);
    case SHAPE_ARG: return mismatch("an application frame shape has no introduction");
    case SHAPE_COLL: return erase_in_typed(heap, ec, ac, ty, addr, args);
    case SHAPE_PAR: return not_yet("SPar arrives at M1");
    case SHAPE_MU: return erase_mu_intro(heap, ec, ac, ty, addr, args);
    case SHAPE_NU: return not_yet("SNu arrives at M2");
    case SHAPE_ZK: return erase_zk_in(heap, ec, ac, addr, args);
    case SHAPE_FHC: return not_yet("Lan SFhc arrives at V5");
    case SHAPE_MPC: return not_yet("Session arrives at V5");
  }
  return not_yet("Session arrives at V5");
}

/* out_arm: the right former elimination arms by shape; fhc and mpc are host calls on the head. */
Comp erase_out_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, Addr addr,
                   const Term *head, const Term *t) {
  const Term *const heads[1] = { head };
  Vec one;
  Str name;
  switch (shape.kind) {
    case SHAPE_PI: return erase_app_arm(heap, ec, ac, tail, ty, t);
    case SHAPE_ARG: return erase_app_arm(heap, ec, ac, tail, ty, t);
    case SHAPE_COLL: return erase_proj_arm(heap, ec, ac, shape.count, addr, head);
    case SHAPE_PAR: return not_yet("SPar arrives at M1");
    case SHAPE_MU: return not_yet("a right former at a mu shape arrives at M2");
    case SHAPE_NU: return not_yet("SNu arrives at M2");
    case SHAPE_ZK: return not_yet("Ran SZk arrives at V5");
    case SHAPE_FHC:
      one = term_list(heap, 1, heads);
      name = str_lit("fhcDec");
      return erase_host_call(heap, ec, ac, name, &one);
    case SHAPE_MPC:
      one = term_list(heap, 1, heads);
      name = str_lit("mpcOpen");
      return erase_host_call(heap, ec, ac, name, &one);
  }
  one = term_list(heap, 1, heads);
  name = str_lit("mpcOpen");
  return erase_host_call(heap, ec, ac, name, &one);
}

/* elim_arm: the eliminator arms by the scrutinee shape. */
Comp erase_elim_arm(Heap *heap, Shape shape, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty,
                    const ElimData *data) {
  switch (shape.kind) {
    case SHAPE_PI: return erase_elim_typed(heap, ec, ac, tail, ty, data);
    case SHAPE_ARG: return mismatch("an application frame shape has no eliminator");
    case SHAPE_COLL: return erase_elim_typed(heap, ec, ac, tail, ty, data);
    case SHAPE_PAR: return not_yet("SPar arrives at M1");
    case SHAPE_MU: return erase_mu_elim(heap, ec, ac, tail, data);
    case SHAPE_NU: return not_yet("SNu arrives at M2");
    case SHAPE_ZK: return erase_elim_typed(heap, ec, ac, tail, ty, data);
    case SHAPE_FHC: return not_yet("Lan SFhc arrives at V5");
    case SHAPE_MPC: return not_yet("Session arrives at V5");
  }
  return not_yet("Session arrives at V5");
}

/* node: one arm per term constructor. */
Comp erase_node(Heap *heap, const Term *t, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty) {
  Shape elim_shape;
  ExpectedTy expected;
  const Ktm *leaf;
  switch (t->kind) {
    case TERM_VAR: return erase_var_arm(heap, ec, ac, t->index);
    case TERM_UNIV: return pure_out(heap, eterm_kerased(), ac);
    case TERM_LAN: return pure_out(heap, eterm_kerased(), ac);
    case TERM_RAN: return pure_out(heap, eterm_kerased(), ac);
    case TERM_SEC: return erase_sec_arm(heap, t->shape, ec, ac, ty, &t->legs, t);
    case TERM_IN: return erase_in_arm(heap, t->shape, ec, ac, ty, t->addr, &t->args);
    case TERM_OUT: return erase_out_arm(heap, t->shape, ec, ac, tail, ty, t->addr, t->head, t);
    case TERM_ELIM:
      elim_shape = erase_elim_shape(t->data);
      return erase_elim_arm(heap, elim_shape, ec, ac, tail, ty, t->data);
    case TERM_LET: return erase_let_arm(heap, ec, ac, tail, ty, t->name, t->ty, t->value, t->body);
    case TERM_ANN:
      expected = erase_expected_some(ty);
      return erase_term(heap, ec, ac, tail, expected, t->body);
    case TERM_GLOBAL:
      leaf = eterm_kglobal(heap, t->name);
      if (leaf == NULL) return comp_oom();
      return pure_out(heap, leaf, ac);
    case TERM_LIT:
      leaf = eterm_klit(heap, t->lit);
      if (leaf == NULL) return comp_oom();
      return pure_out(heap, leaf, ac);
    case TERM_AUTO: return not_yet("instances arrive at M2");
  }
  return not_yet("instances arrive at M2");
}

/* term_runtime: an erased position yields KErased without visiting the term. */
Comp erase_term_runtime(Heap *heap, bool keep, EraseCtx ec, EraseAcc ac, bool tail, const Value *ty, const Term *t) {
  if (keep) return erase_node(heap, t, ec, ac, tail, ty);
  return pure_out(heap, eterm_kerased(), ac);
}

/* term (law body): position type, runtime flag, then the node. */
static Comp term_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  bool keep = kernel_bool_unbox(value);
  return erase_term_runtime(heap, keep, f->ec, f->ac, f->tail, f->ty, f->t);
}

static Comp term_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, f->ty), term_after_keep, f);
}

Comp erase_term(Heap *heap, EraseCtx ec, EraseAcc ac, bool tail, ExpectedTy expected, const Term *t) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->ac = ac;
  f->tail = tail;
  f->t = t;
  return bind_frame(heap, erase_position_ty(heap, ec, expected, t), term_after_ty, f);
}

/* ---------------------------------------------------------------------------------------------
 * Quarter 4b: Bend lines 924-1049 (tids_repr .. dedup_tids, def_finish .. program, print_decls
 * .. print). Frame fields added: Globals globals; Entry entry; const EraseEntry *erased.
 * ------------------------------------------------------------------------------------------- */

/* Boxed entries and rows for Comp values and Vecs; NULL after a failed allocation. */
EraseEntry *erase_entry_new(Heap *heap, EraseEntry entry) {
  EraseEntry *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = entry;
  return boxed;
}

EraseRow *erase_row_new(Heap *heap, Str name, EraseEntry entry) {
  EraseRow *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  boxed->name = name;
  boxed->entry = entry;
  return boxed;
}

static Comp pure_entry(Heap *heap, EraseEntry entry) {
  EraseEntry *boxed = erase_entry_new(heap, entry);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

/* List.append into an accumulator: every item of xs joins out. */
static void extend(Heap *heap, Vec *out, const Vec *xs) {
  size_t i;
  for (i = 0; i < xs->len; i++) {
    VecGet got = vec_get(xs, i);
    if (got.status == VEC_GET_OK) push(heap, out, got.value);
  }
}

/* [tid]: the one-item list; empty after a failed allocation (heap->oom set). */
static Vec tid_single(Heap *heap, Tid tid) {
  Vec out = empty(heap);
  Tid *boxed = eterm_tid_new(heap, tid.name);
  if (boxed != NULL) push(heap, &out, boxed);
  return out;
}

/* tids_repr */
Vec erase_tids_repr(Heap *heap, Repr repr) {
  switch (repr.kind) {
    case REPR_I31: return empty(heap);
    case REPR_STRUCT: return tid_single(heap, repr.tid);
    case REPR_UNION: return tid_single(heap, repr.tid);
    case REPR_FUNC: return tid_single(heap, repr.tid);
    case REPR_THUNK: return tid_single(heap, repr.tid);
  }
  return tid_single(heap, repr.tid);
}

/* tids_reprs: reprs are Repr pointers. */
Vec erase_tids_reprs(Heap *heap, const Vec *reprs) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < reprs->len; i++) {
    VecGet got = vec_get(reprs, i);
    if (got.status == VEC_GET_OK) {
      const Repr *r = got.value;
      Vec one = erase_tids_repr(heap, *r);
      extend(heap, &out, &one);
    }
  }
  return out;
}

/* node_tid */
Vec erase_node_tid(Heap *heap, const Ktm *term) {
  switch (term->kind) {
    case KTM_VAR: return empty(heap);
    case KTM_LIT: return empty(heap);
    case KTM_GLOBAL: return empty(heap);
    case KTM_ERASED: return empty(heap);
    case KTM_LET: return empty(heap);
    case KTM_CLOS: return empty(heap);
    case KTM_APP: return empty(heap);
    case KTM_TAIL: return empty(heap);
    case KTM_STRUCT: return tid_single(heap, term->tid);
    case KTM_PROJ: return tid_single(heap, term->tid);
    case KTM_TAG: return tid_single(heap, term->tid);
    case KTM_CASE: return tid_single(heap, term->tid);
    case KTM_DELAY: return empty(heap);
    case KTM_FORCE: return empty(heap);
  }
  return empty(heap);
}

/* tids_walk: work is a List<RuntimeWork> (RuntimeWork pointers). The head's tids come first, then
 * its runtime children before the rest (the erase_util stack idiom: children pushed in reverse). */
Vec erase_tids_walk(Heap *heap, const Vec *work) {
  Vec out = empty(heap);
  Vec stack = empty(heap);
  size_t i;
  for (i = work->len; i > 0; i--) {
    VecGet got = vec_get(work, i - 1);
    if (got.status == VEC_GET_OK) push(heap, &stack, got.value);
  }
  while (stack.len > 0) {
    VecGet top = vec_last(&stack);
    const RuntimeWork *item = NULL;
    Nat zero = nat_zero();
    Vec node;
    Vec children;
    size_t j;
    stack.len -= 1;
    if (top.status != VEC_GET_OK) continue;
    item = top.value;
    if (item->term == NULL) continue;
    node = erase_node_tid(heap, item->term);
    extend(heap, &out, &node);
    children = erase_util_runtime_children(heap, item->term, zero);
    for (j = children.len; j > 0; j--) {
      VecGet child = vec_get(&children, j - 1);
      if (child.status == VEC_GET_OK) push(heap, &stack, child.value);
    }
  }
  return out;
}

/* tids_ktm */
Vec erase_tids_ktm(Heap *heap, const Ktm *t) {
  Vec work = empty(heap);
  Nat zero = nat_zero();
  RuntimeWork *item = erase_util_runtime_work_new(heap, zero, t);
  if (item != NULL) push(heap, &work, item);
  return erase_tids_walk(heap, &work);
}

/* tids_decl */
Vec erase_tids_decl(Heap *heap, Kdecl decl) {
  Vec out;
  Vec ret;
  Vec body;
  switch (decl.kind) {
    case KDECL_FUN:
      out = erase_tids_reprs(heap, &decl.params);
      ret = erase_tids_repr(heap, decl.result);
      body = erase_tids_ktm(heap, decl.body);
      extend(heap, &out, &ret);
      extend(heap, &out, &body);
      return out;
    case KDECL_REC: return decl.tids;
  }
  return decl.tids;
}

/* tids_decls: decls are Kdecl pointers. */
Vec erase_tids_decls(Heap *heap, const Vec *decls) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < decls->len; i++) {
    VecGet got = vec_get(decls, i);
    if (got.status == VEC_GET_OK) {
      const Kdecl *d = got.value;
      Vec one = erase_tids_decl(heap, *d);
      extend(heap, &out, &one);
    }
  }
  return out;
}

/* member_tid: tids are Tid pointers. */
bool erase_member_tid(const Vec *tids, Tid tid) {
  Str want = eterm_tid_text(tid);
  size_t i;
  for (i = 0; i < tids->len; i++) {
    VecGet got = vec_get(tids, i);
    if (got.status == VEC_GET_OK) {
      const Tid *t = got.value;
      Str have = eterm_tid_text(*t);
      if (str_eq(have, want)) return true;
    }
  }
  return false;
}

/* dedup_tids: every tid joins seen, only the unseen ones join the result. */
Vec erase_dedup_tids(Heap *heap, const Vec *tids, Vec seen) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < tids->len; i++) {
    VecGet got = vec_get(tids, i);
    if (got.status == VEC_GET_OK) {
      Tid *t = got.value;
      if (!erase_member_tid(&seen, *t)) push(heap, &out, t);
      seen = cons(heap, t, &seen);
    }
  }
  return out;
}

/* def_finish: the function declaration joins the lifted ones; the group lists every tid they
 * mention plus the accumulated groups, deduplicated. */
EraseEntry erase_def_finish(Heap *heap, EraseChain result, Str name) {
  Fid fid;
  Kdecl fun;
  Kdecl rec;
  Kdecl *fun_box;
  Kdecl *rec_box;
  Vec one;
  Vec ds;
  Vec mentioned;
  Vec tids;
  Vec group;
  Vec decls;
  Vec none;
  fid.name = name;
  fun = eterm_kfun(fid, result.params, result.result, result.body);
  fun_box = eterm_kdecl_new(heap, fun);
  if (fun_box == NULL) return erase_entry_code(empty(heap));
  one = empty(heap);
  push(heap, &one, fun_box);
  ds = append(heap, &result.acc.lifted, &one);
  mentioned = erase_tids_decls(heap, &ds);
  tids = append(heap, &mentioned, &result.acc.groups);
  none = empty(heap);
  group = erase_dedup_tids(heap, &tids, none);
  rec = eterm_krec(group);
  rec_box = eterm_kdecl_new(heap, rec);
  if (rec_box == NULL) return erase_entry_code(empty(heap));
  decls = cons(heap, rec_box, &ds);
  return erase_entry_code(decls);
}

/* def_code: chain the definition, then finish it. */
static Comp def_code_after_chain(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EraseChain *result = value;
  EraseEntry entry = erase_def_finish(heap, *result, f->name);
  if (heap->oom) return comp_oom();
  return pure_entry(heap, entry);
}

Comp erase_def_code(Heap *heap, EraseCtx ec, Str name, const Value *ty, const Term *body) {
  Frame *f = frame_new(heap);
  Nat zero = nat_zero();
  Vec lifted;
  Vec groups;
  Vec params;
  EraseAcc start;
  if (f == NULL) return comp_oom();
  f->name = name;
  lifted = empty(heap);
  groups = empty(heap);
  params = empty(heap);
  start = erase_acc(zero, lifted, groups);
  return bind_frame(heap, erase_chain(heap, ec, start, params, ty, body), def_code_after_chain, f);
}

/* axiom_runtime: a kept axiom becomes a postulate of its representation. */
static Comp axiom_runtime_after_repr(Heap *heap, void *ctx, void *value) {
  const Repr *repr = value;
  EraseEntry entry = erase_entry_postulate(*repr);
  (void)ctx;
  return pure_entry(heap, entry);
}

Comp erase_axiom_runtime(Heap *heap, bool keep, EraseCtx ec, const Value *ty) {
  EraseEntry dropped;
  if (keep) return bind_with(heap, erase_repr_repr_of(heap, ec, ty), axiom_runtime_after_repr, NULL);
  dropped = erase_entry_dropped();
  return pure_entry(heap, dropped);
}

/* def_runtime */
Comp erase_def_runtime(Heap *heap, bool keep, EraseCtx ec, Str name, const Value *ty, const Term *body) {
  EraseEntry dropped;
  if (keep) return erase_def_code(heap, ec, name, ty, body);
  dropped = erase_entry_dropped();
  return pure_entry(heap, dropped);
}

/* entry_kind: the do blocks bind the evaluated type, then the runtime flag. */
static Comp entry_kind_axiom_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  bool keep = kernel_bool_unbox(value);
  return erase_axiom_runtime(heap, keep, f->ec, f->ty);
}

static Comp entry_kind_axiom_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, f->ty), entry_kind_axiom_after_keep, f);
}

static Comp entry_kind_axiom(Heap *heap, const Term *ty, EraseCtx ec) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  return bind_frame(heap, erase_repr_eval(heap, ec, ty), entry_kind_axiom_after_ty, f);
}

static Comp entry_kind_def_after_keep(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  bool keep = kernel_bool_unbox(value);
  return erase_def_runtime(heap, keep, f->ec, f->name, f->ty, f->body);
}

static Comp entry_kind_def_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->ty = value;
  return bind_frame(heap, erase_repr_runtime_ty(heap, f->ec, f->ty), entry_kind_def_after_keep, f);
}

static Comp entry_kind_def(Heap *heap, DefEntry def, EraseCtx ec, Str name) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ec = ec;
  f->name = name;
  f->body = def.body;
  return bind_frame(heap, erase_repr_eval(heap, ec, def.ty), entry_kind_def_after_ty, f);
}

Comp erase_entry_kind(Heap *heap, Entry entry, EraseCtx ec, Str name) {
  EraseEntry dropped;
  switch (entry.kind) {
    case ENTRY_PRIM:
      dropped = erase_entry_dropped();
      return pure_entry(heap, dropped);
    case ENTRY_AXIOM: return entry_kind_axiom(heap, entry.ty, ec);
    case ENTRY_DEF: return entry_kind_def(heap, entry.def, ec, name);
  }
  return entry_kind_def(heap, entry.def, ec, name);
}

/* entry_comp: a fresh erase context over the globals with an unlimited budget. */
Comp erase_entry_comp(Heap *heap, Globals globals, Str name, Entry entry) {
  EraseCtx ec;
  Budget unlimited = budget_unlimited();
  Nat zero = nat_zero();
  ec.check = context_make(heap, globals, unlimited, zero);
  ec.slots = empty(heap);
  ec.self = name;
  return erase_entry_kind(heap, entry, ec, name);
}

/* decl */
Result erase_decl(Heap *heap, Globals globals, Budget budget, Str name, Entry entry) {
  return budget_run(heap, erase_entry_comp(heap, globals, name, entry), budget);
}

/* entry */
Result erase_entry(Heap *heap, Globals globals, Str name, Entry entry) {
  Budget unlimited = budget_unlimited();
  return erase_decl(heap, globals, unlimited, name, entry);
}

/* program_comp: rows are CheckRow pointers. Each entry is erased in the globals so far, then the
 * rest in the globals extended with it; the result rows are EraseRow pointers. */
static Comp program_comp_after_more(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *more = value;
  EraseRow *row = erase_row_new(heap, f->name, *f->erased);
  Vec rows;
  if (row == NULL) return comp_oom();
  rows = cons(heap, row, more);
  return pure_vec(heap, rows);
}

static Comp program_comp_after_entry(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Globals next;
  f->erased = value;
  next = global_add(heap, f->name, f->entry, f->globals);
  if (heap->oom) return comp_oom();
  return bind_frame(heap, erase_program_comp(heap, &f->rest, next), program_comp_after_more, f);
}

static Comp program_comp_row(Heap *heap, const CheckRow *row, Vec rest, Globals globals) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->name = row->name;
  f->entry = row->entry;
  f->globals = globals;
  f->rest = rest;
  return bind_frame(heap, erase_entry_comp(heap, globals, row->name, row->entry), program_comp_after_entry, f);
}

Comp erase_program_comp(Heap *heap, const Vec *rows, Globals globals) {
  VecGet got = vec_get(rows, 0);
  Vec none;
  switch (got.status) {
    case VEC_GET_OUT_OF_RANGE:
      none = empty(heap);
      return pure_vec(heap, none);
    case VEC_GET_OK: return program_comp_row(heap, got.value, list_tail(*rows), globals);
  }
  return program_comp_row(heap, got.value, list_tail(*rows), globals);
}

/* program_budget */
Result erase_program_budget(Heap *heap, Globals globals, Budget budget, const Vec *rows) {
  return budget_run(heap, erase_program_comp(heap, rows, globals), budget);
}

/* program */
Result erase_program(Heap *heap, Globals globals, const Vec *rows) {
  Budget unlimited = budget_unlimited();
  return erase_program_budget(heap, globals, unlimited, rows);
}

/* print_decls: decls are Kdecl pointers, one line each. */
Str erase_print_decls(Heap *heap, const Vec *decls) {
  Str out = str_lit("");
  Str newline = str_lit("\n");
  size_t i;
  for (i = 0; i < decls->len; i++) {
    VecGet got = vec_get(decls, i);
    if (got.status == VEC_GET_OK) {
      const Kdecl *d = got.value;
      Str line = eterm_print_decl(heap, *d);
      out = heap_str_cat(heap, out, line);
      out = heap_str_cat(heap, out, newline);
    }
  }
  return out;
}

/* print_entry */
Str erase_print_entry(Heap *heap, Str name, EraseEntry entry) {
  Str parts[5];
  switch (entry.kind) {
    case ERASE_ENTRY_DROPPED:
      parts[0] = str_lit("erased ");
      parts[1] = name;
      parts[2] = str_lit("\n");
      return cats(heap, 3, parts);
    case ERASE_ENTRY_POSTULATE:
      parts[0] = str_lit("axiom ");
      parts[1] = name;
      parts[2] = str_lit(" : ");
      parts[3] = eterm_print_repr(heap, entry.repr);
      parts[4] = str_lit("\n");
      return cats(heap, 5, parts);
    case ERASE_ENTRY_CODE: return erase_print_decls(heap, &entry.decls);
  }
  return erase_print_decls(heap, &entry.decls);
}

/* print: rows are EraseRow pointers. */
Str erase_print(Heap *heap, const Vec *rows) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < rows->len; i++) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK) {
      const EraseRow *row = got.value;
      Str text = erase_print_entry(heap, row->name, row->entry);
      out = heap_str_cat(heap, out, text);
    }
  }
  return out;
}
