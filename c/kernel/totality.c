/* Port of bend2/kernel/totality.bend: the budgeted termination guard. Same definitions in the
 * same order. The term worklists are Vecs of Term pointers (head first); the member list is a
 * Vec of OrderMember pointers; the Budget.T result of spend and the Maybe values of the Comp
 * results are heap boxed. The globals argument is carried unused, as in the Bend. */
#include "kernel.h"

static bool push(Heap *heap, Vec *vec, void *item) {
  if (vec_push(vec, item)) return true;
  heap->oom = true;
  return false;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static NextK next_k(Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return next;
}

/* List.append(Term.children(term), rest) */
static Vec children_then(Heap *heap, const Term *term, const Vec *rest, size_t from) {
  Vec out = term_children(heap, term);
  size_t i;
  for (i = from; i < rest->len; i++) push(heap, &out, at(rest, i));
  return out;
}

Str totality_budget_msg(void) { return str_lit("the totality budget is exhausted"); }

Result totality_spend(Heap *heap, MaybeBudget state, Vec terms) {
  MaybeBudget cur = state;
  Vec todo = terms;
  for (;;) {
    const Term *t;
    if (!cur.some) return budget_result_fail(error_make(ERROR_BUDGET_EXHAUSTED, totality_budget_msg()));
    if (todo.len == 0) {
      Budget *boxed = heap_alloc(heap, sizeof(Budget));
      if (boxed == NULL) return budget_result_fail(error_host_oom());
      *boxed = cur.value;
      return budget_result_done(boxed);
    }
    t = at(&todo, 0);
    cur = budget_poll(heap, cur.value);
    todo = t == NULL ? children_then(heap, term_auto(), &todo, 1) : children_then(heap, t, &todo, 1);
  }
}

Vec totality_bodies(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < ms->len; i++) {
    const OrderMember *m = at(ms, i);
    if (m != NULL) push(heap, &out, (void *)m->body);
  }
  return out;
}

OrderCertResult totality_guard_group(Heap *heap, Budget budget, Globals globals, const Vec *members) {
  MaybeBudget some;
  Result remaining;
  (void)globals;
  some.some = true;
  some.value = budget;
  remaining = totality_spend(heap, some, totality_bodies(heap, members));
  switch (remaining.kind) {
    case RESULT_FAIL: {
      OrderCertResult failed = {0};
      failed.kind = RESULT_FAIL;
      failed.error = remaining.error;
      return failed;
    }
    case RESULT_DONE: return order_certify(heap, members);
  }
  return order_certify(heap, members);
}

MaybeNat totality_guarded_argument(MaybeOrderCert cert) {
  MaybeNat m = {0};
  if (!cert.some) return m;
  m.some = true;
  m.value = order_argument(cert.value);
  return m;
}

static Vec one_member(Heap *heap, Str name, const Term *body) {
  Vec members = vec_new(heap->arena);
  OrderMember *m = order_member_new(heap, name, body);
  if (m != NULL) push(heap, &members, m);
  return members;
}

MaybeNatResult totality_guard(Heap *heap, Budget budget, Globals globals, Str name, const Term *ty, const Term *body) {
  MaybeNatResult r = {0};
  Vec members = one_member(heap, name, body);
  OrderCertResult c;
  (void)ty;
  c = totality_guard_group(heap, budget, globals, &members);
  switch (c.kind) {
    case RESULT_FAIL:
      r.kind = RESULT_FAIL;
      r.error = c.error;
      return r;
    case RESULT_DONE:
      r.kind = RESULT_DONE;
      r.value = totality_guarded_argument(c.value);
      return r;
  }
  return r;
}

Comp totality_tick_comp(Heap *heap) { return budget_tick_with(heap, totality_budget_msg()); }

static Comp spend_comp_after_tick(Heap *heap, void *ctx, void *value) {
  const Vec *rest = ctx;
  (void)value;
  return totality_spend_comp(heap, *rest);
}

Comp totality_spend_comp(Heap *heap, Vec terms) {
  const Term *term;
  Vec *rest;
  if (terms.len == 0) return budget_pure(kernel_unit());
  term = at(&terms, 0);
  rest = heap_alloc(heap, sizeof(Vec));
  if (rest == NULL) return budget_failed(error_host_oom());
  *rest = term == NULL ? children_then(heap, term_auto(), &terms, 1) : children_then(heap, term, &terms, 1);
  return budget_bind(heap, totality_tick_comp(heap), next_k(spend_comp_after_tick, rest));
}

/* Result<Error.T, Maybe<Order.T>> with the Maybe boxed for Budget.lift. */
static Result cert_result_boxed(Heap *heap, OrderCertResult r) {
  switch (r.kind) {
    case RESULT_FAIL: return budget_result_fail(r.error);
    case RESULT_DONE: {
      MaybeOrderCert *boxed = heap_alloc(heap, sizeof(MaybeOrderCert));
      if (boxed == NULL) return budget_result_fail(error_host_oom());
      *boxed = r.value;
      return budget_result_done(boxed);
    }
  }
  return budget_result_fail(error_host_oom());
}

static Comp guard_group_after_spend(Heap *heap, void *ctx, void *value) {
  const Vec *members = ctx;
  (void)value;
  return budget_lift(cert_result_boxed(heap, order_certify(heap, members)));
}

Comp totality_guard_group_comp(Heap *heap, Globals globals, const Vec *members) {
  Vec *boxed = heap_alloc(heap, sizeof(Vec));
  (void)globals;
  if (boxed == NULL) return budget_failed(error_host_oom());
  *boxed = *members;
  return budget_bind(heap, totality_spend_comp(heap, totality_bodies(heap, members)), next_k(guard_group_after_spend, boxed));
}

static Comp guard_after_group(Heap *heap, void *ctx, void *value) {
  const MaybeOrderCert *cert = value;
  MaybeNat *boxed = heap_alloc(heap, sizeof(MaybeNat));
  (void)ctx;
  if (boxed == NULL || cert == NULL) return budget_failed(error_host_oom());
  *boxed = totality_guarded_argument(*cert);
  return budget_pure(boxed);
}

Comp totality_guard_comp(Heap *heap, Globals globals, Str name, const Term *ty, const Term *body) {
  Vec members = one_member(heap, name, body);
  (void)ty;
  return budget_bind(heap, totality_guard_group_comp(heap, globals, &members), next_k(guard_after_group, NULL));
}
