/* Port of bend2/kernel/order.bend: the structural recursion certificate. Same definitions in
 * the same order. List<Status> is a Vec of pointers to the static status cells, index 0 the
 * head (the innermost binder); List<Step> a Vec of OrderStep pointers, head first; List<Call>
 * a Vec of OrderCall pointers; List<Row> a Vec of OrderRow pointers; the member list a Vec of
 * OrderMember pointers; List<Peeled> a Vec of OrderPeeled pointers. The task builders
 * (term_tasks .. elim_tasks) append to an output Vec in the Bend list order instead of
 * prepending to `rest`; walk runs the Bend worklist recursion as a loop over an explicit
 * stack with the same visiting order. */
#include "kernel.h"

static OrderStatus status_cells[3] = {ORDER_PRINCIPAL, ORDER_SMALLER, ORDER_OTHER};

static void *status_cell(OrderStatus status) {
  switch (status) {
    case ORDER_PRINCIPAL: return &status_cells[0];
    case ORDER_SMALLER: return &status_cells[1];
    case ORDER_OTHER: return &status_cells[2];
  }
  return &status_cells[2];
}

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

/* item <> rest as a fresh Vec. */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  size_t i;
  push(heap, &out, item);
  for (i = 0; i < rest->len; i++) push(heap, &out, at(rest, i));
  return out;
}

static Nat pred(Heap *heap, Nat n) {
  NatSub r = nat_sub(heap->arena, n, heap_nat_of_u64(heap, 1));
  switch (r.status) {
    case NAT_SUB_OK: return r.value;
    case NAT_SUB_UNDERFLOW: return nat_zero();
    case NAT_SUB_OOM: heap->oom = true; return nat_zero();
  }
  return nat_zero();
}

static bool nat_eq(Nat a, Nat b) { return nat_cmp(a, b) == 0; }

static OrderUnitResult unit_done(void) {
  OrderUnitResult r = {0};
  r.kind = RESULT_DONE;
  return r;
}

static OrderUnitResult unit_fail(Error error) {
  OrderUnitResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static OrderCertResult cert_fail(Error error) {
  OrderCertResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static OrderCertResult cert_done(MaybeOrderCert value) {
  OrderCertResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static MaybeVec vec_none(void) {
  MaybeVec m = {0};
  return m;
}

static MaybeVec vec_some(Vec value) {
  MaybeVec m;
  m.some = true;
  m.value = value;
  return m;
}

/* ---- constructors (host helpers) ---- */

OrderStep *order_step_new(Heap *heap, Nat scrut, MaybeStr ctor) {
  OrderStep *boxed = heap_alloc(heap, sizeof(OrderStep));
  if (boxed == NULL) return NULL;
  boxed->scrut = scrut;
  boxed->ctor = ctor;
  return boxed;
}

OrderCall *order_call_new(Heap *heap, Str callee, Vec chain) {
  OrderCall *boxed = heap_alloc(heap, sizeof(OrderCall));
  if (boxed == NULL) return NULL;
  boxed->callee = callee;
  boxed->chain = chain;
  return boxed;
}

OrderRow *order_row_new(Heap *heap, Str member, Vec calls) {
  OrderRow *boxed = heap_alloc(heap, sizeof(OrderRow));
  if (boxed == NULL) return NULL;
  boxed->member = member;
  boxed->calls = calls;
  return boxed;
}

OrderCert order_certificate(Vec group, Nat arg, Vec rows) {
  OrderCert c;
  c.group = group;
  c.arg = arg;
  c.rows = rows;
  return c;
}

OrderMember *order_member_new(Heap *heap, Str name, const Term *body) {
  OrderMember *boxed = heap_alloc(heap, sizeof(OrderMember));
  if (boxed == NULL) return NULL;
  boxed->name = name;
  boxed->body = body;
  return boxed;
}

OrderPeeled *order_peeled_new(Heap *heap, OrderPeeled peeled) {
  OrderPeeled *boxed = heap_alloc(heap, sizeof(OrderPeeled));
  if (boxed == NULL) return NULL;
  *boxed = peeled;
  return boxed;
}

/* ---- definitions ---- */

bool order_member(const Vec *xs, Str n) {
  size_t i;
  for (i = 0; i < xs->len; i++) {
    const Str *x = at(xs, i);
    if (x != NULL && str_eq(*x, n)) return true;
  }
  return false;
}

bool order_mentions(Heap *heap, const Vec *group, const Term *t) {
  return term_exists_name(heap, false, group, t);
}

/* Sec{SPi | SZk, [Leg{[binder], body}]}: the one leg with one binder. */
static const Term *peel_step(const Term *t) {
  const Leg *leg;
  if (t->kind != TERM_SEC) return NULL;
  if (t->shape.kind != SHAPE_PI && t->shape.kind != SHAPE_ZK) return NULL;
  if (t->legs.len != 1) return NULL;
  leg = at(&t->legs, 0);
  if (leg == NULL || leg->binders.len != 1) return NULL;
  return leg->body;
}

OrderPeeled order_peel(Heap *heap, const Term *t, Nat n, Str name) {
  OrderPeeled p;
  const Term *cur = t;
  Nat count = n;
  const Term *next = peel_step(cur);
  while (next != NULL) {
    cur = next;
    count = heap_nat_succ(heap, count);
    next = peel_step(cur);
  }
  p.name = name;
  p.formals = count;
  p.inner = cur;
  return p;
}

OrderSpine order_spine(Heap *heap, const Term *t, Vec args, Vec shapes) {
  OrderSpine sp;
  const Term *cur = t;
  Vec cur_args = args;
  Vec cur_shapes = shapes;
  while (cur->kind == TERM_OUT && cur->addr.kind == ADDR_PT) {
    cur_args = cons(heap, (void *)cur->addr.arg, &cur_args);
    cur_shapes = cons(heap, (void *)&cur->shape, &cur_shapes);
    cur = cur->head;
  }
  sp.head = cur;
  sp.args = cur_args;
  sp.shapes = cur_shapes;
  return sp;
}

MaybeOrderStatus order_status_at(const Vec *st, Nat ix) {
  MaybeOrderStatus m = {0};
  VecGet got = common_at(st, ix);
  switch (got.status) {
    case VEC_GET_OK: {
      const OrderStatus *cell = got.value;
      if (cell == NULL) return m;
      m.some = true;
      m.value = *cell;
      return m;
    }
    case VEC_GET_OUT_OF_RANGE: return m;
  }
  return m;
}

bool order_is_smaller(MaybeOrderStatus s) {
  if (!s.some) return false;
  switch (s.value) {
    case ORDER_SMALLER: return true;
    case ORDER_PRINCIPAL: return false;
    case ORDER_OTHER: return false;
  }
  return false;
}

bool order_is_principal_or_smaller(MaybeOrderStatus s) {
  if (!s.some) return false;
  switch (s.value) {
    case ORDER_PRINCIPAL: return true;
    case ORDER_SMALLER: return true;
    case ORDER_OTHER: return false;
  }
  return false;
}

bool order_guarded_arg(MaybePtr arg, const Vec *st) {
  const Term *t = arg.value;
  if (!arg.some || t == NULL || t->kind != TERM_VAR) return false;
  return order_is_smaller(order_status_at(st, t->index));
}

bool order_guarded_call(const Vec *st, const Vec *args, Nat k) {
  MaybePtr arg = {0};
  VecGet got = common_at(args, k);
  switch (got.status) {
    case VEC_GET_OK:
      arg.some = true;
      arg.value = got.value;
      break;
    case VEC_GET_OUT_OF_RANGE: break;
  }
  return order_guarded_arg(arg, st);
}

Vec order_under(Heap *heap, size_t n, OrderStatus status, const Vec *st) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < n; i++) push(heap, &out, status_cell(status));
  for (i = 0; i < st->len; i++) push(heap, &out, at(st, i));
  return out;
}

Vec order_under_point_result(Heap *heap, MaybePtr dom, const Vec *st) {
  if (dom.some) return order_under(heap, 1, ORDER_OTHER, st);
  return *st;
}

Vec order_under_point(Heap *heap, Shape s, const Vec *st) {
  return order_under_point_result(heap, shape_point_dom(s), st);
}

/* ---- tasks ---- */

static OrderTask *task_new(Heap *heap, OrderTaskKind kind) {
  OrderTask *task = heap_alloc(heap, sizeof(OrderTask));
  if (task == NULL) return NULL;
  task->kind = kind;
  return task;
}

OrderTask *order_visit(Heap *heap, Vec status, Vec chain, const Term *term) {
  OrderTask *task = task_new(heap, ORDER_TASK_VISIT);
  if (task == NULL) return NULL;
  task->status = status;
  task->chain = chain;
  task->term = term;
  return task;
}

OrderTask *order_check_call(Heap *heap, bool valid, Str callee, Vec chain) {
  OrderTask *task = task_new(heap, ORDER_TASK_CHECK_CALL);
  if (task == NULL) return NULL;
  task->valid = valid;
  task->callee = callee;
  task->chain = chain;
  return task;
}

OrderTask *order_refuse(Heap *heap) { return task_new(heap, ORDER_TASK_REFUSE); }

static void push_task(Heap *heap, Vec *out, OrderTask *task) {
  if (task == NULL) return;
  push(heap, out, task);
}

void order_term_tasks(Heap *heap, const Vec *ts, Vec st, Vec chain, Vec *out) {
  size_t i;
  for (i = 0; i < ts->len; i++) push_task(heap, out, order_visit(heap, st, chain, at(ts, i)));
}

void order_shape_tasks(Heap *heap, Shape s, Vec st, Vec chain, Vec *out) {
  Vec payload = shape_payload(heap, s);
  order_term_tasks(heap, &payload, st, chain, out);
}

void order_shapes_tasks(Heap *heap, const Vec *ss, Vec st, Vec chain, Vec *out) {
  size_t i;
  for (i = 0; i < ss->len; i++) {
    const Shape *s = at(ss, i);
    if (s != NULL) order_shape_tasks(heap, *s, st, chain, out);
  }
}

void order_addr_tasks(Heap *heap, Addr a, Vec st, Vec chain, Vec *out) {
  Vec children = term_addr_children(heap, a);
  order_term_tasks(heap, &children, st, chain, out);
}

void order_leg_tasks(Heap *heap, const Vec *ls, Vec st, Vec chain, Vec *out) {
  size_t i;
  for (i = 0; i < ls->len; i++) {
    const Leg *leg = at(ls, i);
    if (leg == NULL) continue;
    push_task(heap, out, order_visit(heap, order_under(heap, leg->binders.len, ORDER_OTHER, &st), chain, leg->body));
  }
}

void order_spine_tasks(Heap *heap, OrderSpine sp, Vec st, Vec chain, const Vec *group, Nat k, Vec *out) {
  const Term *head = sp.head;
  switch (head->kind) {
    case TERM_GLOBAL:
      if (order_member(group, head->name)) {
        push_task(heap, out, order_check_call(heap, order_guarded_call(&st, &sp.args, k), head->name, chain));
      }
      order_shapes_tasks(heap, &sp.shapes, st, chain, out);
      order_term_tasks(heap, &sp.args, st, chain, out);
      return;
    case TERM_OUT:
      order_shape_tasks(heap, head->shape, st, chain, out);
      order_addr_tasks(heap, head->addr, st, chain, out);
      push_task(heap, out, order_visit(heap, st, chain, head->head));
      order_shapes_tasks(heap, &sp.shapes, st, chain, out);
      order_term_tasks(heap, &sp.args, st, chain, out);
      return;
    case TERM_VAR:
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_IN:
    case TERM_ELIM:
    case TERM_SEC:
    case TERM_LET:
    case TERM_ANN:
    case TERM_LIT:
    case TERM_AUTO:
      push_task(heap, out, order_visit(heap, st, chain, head));
      order_shapes_tasks(heap, &sp.shapes, st, chain, out);
      order_term_tasks(heap, &sp.args, st, chain, out);
      return;
  }
}

MaybeNat order_scrut_var(const Term *t) {
  MaybeNat m = {0};
  if (t->kind != TERM_VAR) return m;
  m.some = true;
  m.value = t->index;
  return m;
}

bool order_special_scrut(MaybeNat v, const Vec *st) {
  if (!v.some) return false;
  return order_is_principal_or_smaller(order_status_at(st, v.value));
}

Vec order_branch_chain(Heap *heap, MaybeNat v, bool special, Addr a, Vec chain) {
  OrderStep *step;
  if (!v.some || !special) return chain;
  step = order_step_new(heap, v.value, term_as_actor(a));
  if (step == NULL) return chain;
  return cons(heap, step, &chain);
}

void order_branch_tasks(Heap *heap, const Vec *bs, Vec st, Vec chain, MaybeNat v, bool special, Vec *out) {
  size_t i;
  for (i = 0; i < bs->len; i++) {
    const Branch *b = at(bs, i);
    Vec inner;
    if (b == NULL) continue;
    inner = order_under(heap, b->leg.binders.len, special ? ORDER_SMALLER : ORDER_OTHER, &st);
    order_addr_tasks(heap, b->addr, st, chain, out);
    push_task(heap, out, order_visit(heap, inner, order_branch_chain(heap, v, special, b->addr, chain), b->leg.body));
  }
}

void order_motive_tasks(Heap *heap, MaybeMotive m, Vec st, Vec chain, Vec *out) {
  if (!m.some) return;
  push_task(heap, out, order_visit(heap, order_under(heap, 1 + m.value.indices.len, ORDER_OTHER, &st), chain, m.value.body));
}

void order_elim_tasks(Heap *heap, const ElimData *e, Vec st, Vec chain, Vec *out) {
  MaybeNat v = order_scrut_var(e->scrut);
  bool special = order_special_scrut(v, &st);
  push_task(heap, out, order_visit(heap, st, chain, e->scrut));
  order_shape_tasks(heap, e->shape, st, chain, out);
  order_motive_tasks(heap, e->motive, st, chain, out);
  order_branch_tasks(heap, &e->branches, st, chain, v, special, out);
}

/* The tasks a Visit expands into, in the Bend list order; false for Refuse. */
static bool visit_tasks(Heap *heap, const OrderTask *task, const Vec *group, Nat k, Vec *out) {
  const Term *t = task->term;
  Vec st = task->status;
  Vec chain = task->chain;
  Vec empty;
  switch (t->kind) {
    case TERM_GLOBAL: return !order_member(group, t->name);
    case TERM_LAN:
    case TERM_RAN:
      order_shape_tasks(heap, t->shape, st, chain, out);
      push_task(heap, out, order_visit(heap, order_under_point(heap, t->shape, &st), chain, t->body));
      return true;
    case TERM_IN:
      order_shape_tasks(heap, t->shape, st, chain, out);
      order_addr_tasks(heap, t->addr, st, chain, out);
      order_term_tasks(heap, &t->args, st, chain, out);
      return true;
    case TERM_SEC:
      order_shape_tasks(heap, t->shape, st, chain, out);
      order_leg_tasks(heap, &t->legs, st, chain, out);
      return true;
    case TERM_OUT:
      empty = vec_new(heap->arena);
      order_spine_tasks(heap, order_spine(heap, t, empty, empty), st, chain, group, k, out);
      return true;
    case TERM_ELIM: order_elim_tasks(heap, t->data, st, chain, out); return true;
    case TERM_LET:
      push_task(heap, out, order_visit(heap, st, chain, t->ty));
      push_task(heap, out, order_visit(heap, st, chain, t->value));
      push_task(heap, out, order_visit(heap, order_under(heap, 1, ORDER_OTHER, &st), chain, t->body));
      return true;
    case TERM_ANN:
      push_task(heap, out, order_visit(heap, st, chain, t->body));
      push_task(heap, out, order_visit(heap, st, chain, t->ty));
      return true;
    case TERM_VAR:
    case TERM_UNIV:
    case TERM_LIT:
    case TERM_AUTO: return true;
  }
  return true;
}

MaybeVec order_walk(Heap *heap, Vec tasks, const Vec *group, Nat k, Vec calls) {
  Vec stack = vec_reverse(heap->arena, &tasks);
  Vec found = vec_reverse(heap->arena, &calls);
  while (stack.len > 0) {
    const OrderTask *task = at(&stack, stack.len - 1);
    stack.len = stack.len - 1;
    if (task == NULL) continue;
    switch (task->kind) {
      case ORDER_TASK_REFUSE: return vec_none();
      case ORDER_TASK_CHECK_CALL: {
        OrderCall *call;
        if (!task->valid) return vec_none();
        call = order_call_new(heap, task->callee, task->chain);
        if (call != NULL) push(heap, &found, call);
        break;
      }
      case ORDER_TASK_VISIT: {
        Vec next = vec_new(heap->arena);
        size_t i;
        if (!visit_tasks(heap, task, group, k, &next)) return vec_none();
        for (i = next.len; i > 0; i--) push(heap, &stack, at(&next, i - 1));
        break;
      }
    }
  }
  return vec_some(found);
}

Vec order_seed(Heap *heap, Nat n, Nat index, Nat formals, Nat k) {
  Vec out = vec_new(heap->arena);
  Nat remaining = n;
  Nat ix = index;
  while (!nat_is_zero(remaining)) {
    Nat offset = heap_nat_add(heap, ix, k);
    Nat next = heap_nat_succ(heap, offset);
    bool principal = nat_eq(next, formals);
    push(heap, &out, status_cell(principal ? ORDER_PRINCIPAL : ORDER_OTHER));
    ix = heap_nat_succ(heap, ix);
    remaining = pred(heap, remaining);
  }
  return out;
}

MaybeVec order_passes(Heap *heap, OrderRule rule, const Vec *group, Nat k, Nat formals, const Term *body) {
  switch (rule) {
    case ORDER_RULE_STRUCTURAL: {
      Vec tasks = vec_new(heap->arena);
      Vec chain = vec_new(heap->arena);
      Vec calls = vec_new(heap->arena);
      push_task(heap, &tasks, order_visit(heap, order_seed(heap, formals, nat_zero(), formals, k), chain, body));
      return order_walk(heap, tasks, group, k, calls);
    }
  }
  return vec_none();
}

Vec order_names(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < ms->len; i++) {
    OrderMember *m = at(ms, i);
    if (m != NULL) push(heap, &out, &m->name);
  }
  return out;
}

Vec order_peeled(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < ms->len; i++) {
    const OrderMember *m = at(ms, i);
    OrderPeeled *p;
    if (m == NULL) continue;
    p = order_peeled_new(heap, order_peel(heap, m->body, nat_zero(), m->name));
    if (p != NULL) push(heap, &out, p);
  }
  return out;
}

bool order_any_mentions(Heap *heap, const Vec *ms, const Vec *group) {
  size_t i;
  for (i = 0; i < ms->len; i++) {
    const OrderMember *m = at(ms, i);
    if (m != NULL && order_mentions(heap, group, m->body)) return true;
  }
  return false;
}

MaybeNat order_min_bound(MaybeNat a, Nat n) {
  MaybeNat m;
  m.some = true;
  m.value = n;
  if (a.some && nat_cmp(a.value, n) <= 0) m.value = a.value;
  return m;
}

MaybeNat order_bound(Heap *heap, const Vec *ms, const Vec *group, MaybeNat acc) {
  MaybeNat cur = acc;
  size_t i;
  for (i = 0; i < ms->len; i++) {
    const OrderPeeled *p = at(ms, i);
    if (p != NULL && order_mentions(heap, group, p->inner)) cur = order_min_bound(cur, p->formals);
  }
  return cur;
}

Nat order_bound_default(MaybeNat b) {
  if (b.some) return b.value;
  return nat_zero();
}

MaybeVec order_row_join(Heap *heap, MaybeVec cs, MaybeVec rs, Str n) {
  OrderRow *row;
  if (!cs.some || !rs.some) return vec_none();
  row = order_row_new(heap, n, cs.value);
  if (row == NULL) return vec_some(rs.value);
  return vec_some(cons(heap, row, &rs.value));
}

static MaybeVec rows_from(Heap *heap, const Vec *ms, size_t from, const Vec *group, Nat k) {
  const OrderPeeled *p;
  MaybeVec calls;
  if (from >= ms->len) return vec_some(vec_new(heap->arena));
  p = at(ms, from);
  if (p == NULL) return rows_from(heap, ms, from + 1, group, k);
  calls = order_passes(heap, ORDER_RULE_STRUCTURAL, group, k, p->formals, p->inner);
  return order_row_join(heap, calls, rows_from(heap, ms, from + 1, group, k), p->name);
}

MaybeVec order_rows_at(Heap *heap, const Vec *ms, const Vec *group, Nat k) {
  return rows_from(heap, ms, 0, group, k);
}

Error order_refusal(const Vec *ms) {
  const OrderMember *m = at(ms, 0);
  if (m == NULL) return error_make(ERROR_TERMINATION, str_lit(""));
  return error_make(ERROR_TERMINATION, m->name);
}

OrderSearch order_search(Nat remaining, Nat index) {
  OrderSearch s = {0};
  s.kind = ORDER_SEARCH;
  s.remaining = remaining;
  s.index = index;
  return s;
}

OrderSearch order_candidate(MaybeVec rows, Nat remaining, Nat index) {
  OrderSearch s = {0};
  s.kind = ORDER_CANDIDATE;
  s.rows = rows;
  s.remaining = remaining;
  s.index = index;
  return s;
}

OrderCertResult order_first_fit(Heap *heap, OrderSearch s, const Vec *ps, const Vec *group, Error error) {
  OrderSearch cur = s;
  for (;;) {
    switch (cur.kind) {
      case ORDER_SEARCH:
        if (nat_is_zero(cur.remaining)) return cert_fail(error);
        cur = order_candidate(order_rows_at(heap, ps, group, cur.index), pred(heap, cur.remaining), cur.index);
        break;
      case ORDER_CANDIDATE:
        if (cur.rows.some) {
          MaybeOrderCert some;
          some.some = true;
          some.value = order_certificate(*group, cur.index, cur.rows.value);
          return cert_done(some);
        }
        cur = order_search(cur.remaining, heap_nat_succ(heap, cur.index));
        break;
    }
  }
}

OrderCertResult order_certify_used(Heap *heap, bool used, const Vec *ms) {
  Vec group;
  Vec ps;
  Nat n;
  MaybeNat none = {0};
  if (!used) {
    MaybeOrderCert nothing = {0};
    return cert_done(nothing);
  }
  group = order_names(heap, ms);
  ps = order_peeled(heap, ms);
  n = order_bound_default(order_bound(heap, &ps, &group, none));
  return order_first_fit(heap, order_search(n, nat_zero()), &ps, &group, order_refusal(ms));
}

OrderCertResult order_certify(Heap *heap, const Vec *members) {
  Vec group = order_names(heap, members);
  return order_certify_used(heap, order_any_mentions(heap, members, &group), members);
}

Nat order_argument(OrderCert cert) { return cert.arg; }

Str order_one_elim_msg(void) {
  return str_lit("a structurally recursive definition eliminates its recursive argument at the head of its body");
}

Str order_motive_msg(void) {
  return str_lit("the elimination of a structurally recursive definition carries its motive");
}

Str order_ctor_key_msg(void) {
  return str_lit("every branch of the elimination of a structurally recursive definition keys a constructor");
}

OrderUnitResult order_valid_calls(const Vec *cs, Str name) {
  size_t i;
  for (i = 0; i < cs->len; i++) {
    const OrderCall *c = at(cs, i);
    const OrderStep *first = c == NULL ? NULL : at(&c->chain, 0);
    if (first == NULL || !first->ctor.some) return unit_fail(error_make(ERROR_TERMINATION, name));
  }
  return unit_done();
}

OrderUnitResult order_valid_rows(const Vec *rs) {
  size_t i;
  for (i = 0; i < rs->len; i++) {
    const OrderRow *r = at(rs, i);
    OrderUnitResult checked;
    if (r == NULL) continue;
    checked = order_valid_calls(&r->calls, r->member);
    if (checked.kind == RESULT_FAIL) return checked;
  }
  return unit_done();
}

OrderUnitResult order_valid_branches(const Vec *bs) {
  size_t i;
  for (i = 0; i < bs->len; i++) {
    const Branch *b = at(bs, i);
    if (b == NULL || b->addr.kind != ADDR_CTOR) return unit_fail(error_make(ERROR_WRONG_LEG, order_ctor_key_msg()));
  }
  return unit_done();
}

OrderUnitResult order_valid_motive(MaybeMotive m, const Vec *bs, const Vec *rows) {
  OrderUnitResult branches;
  if (!m.some) return unit_fail(error_make(ERROR_MISSING_BRANCH, order_motive_msg()));
  branches = order_valid_branches(bs);
  if (branches.kind == RESULT_FAIL) return branches;
  return order_valid_rows(rows);
}

OrderUnitResult order_valid_elim(Heap *heap, const ElimData *e, Nat formals, Nat k, const Vec *rows) {
  const Term *scrut = e->scrut;
  if (scrut->kind != TERM_VAR) return unit_fail(error_make(ERROR_MISMATCH, order_one_elim_msg()));
  Nat offset = heap_nat_add(heap, scrut->index, k);
  Nat next = heap_nat_succ(heap, offset);
  if (!nat_eq(next, formals)) {
    return unit_fail(error_make(ERROR_MISMATCH, order_one_elim_msg()));
  }
  return order_valid_motive(e->motive, &e->branches, rows);
}

OrderUnitResult order_translate_inner(Heap *heap, OrderPeeled p, Nat k, const Vec *rows) {
  if (p.inner->kind != TERM_ELIM) return unit_fail(error_make(ERROR_MISMATCH, order_one_elim_msg()));
  return order_valid_elim(heap, p.inner->data, p.formals, k, rows);
}

OrderTermResult order_translate_used(Heap *heap, bool used, OrderCert cert, const Term *body) {
  OrderTermResult r = {0};
  OrderUnitResult checked;
  r.kind = RESULT_DONE;
  r.value = body;
  if (!used) return r;
  checked = order_translate_inner(heap, order_peel(heap, body, nat_zero(), str_lit("")), cert.arg, &cert.rows);
  if (checked.kind == RESULT_FAIL) {
    r.kind = RESULT_FAIL;
    r.value = NULL;
    r.error = checked.error;
  }
  return r;
}

OrderTermResult order_translate(Heap *heap, OrderCert cert, const Term *body) {
  return order_translate_used(heap, order_mentions(heap, &cert.group, body), cert, body);
}
