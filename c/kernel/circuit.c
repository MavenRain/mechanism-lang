/* Port of bend2/kernel/circuit.bend: the depth certificate of a kernel term. Same definitions in
 * the same order. Lists are Vecs with the Bend head at index 0: globals hold CircuitGlobal
 * pointers, memo rows CircuitMemoRow pointers, envs CircuitValue pointers, seen lists Str
 * pointers, term lists Term pointers. The worklists of reads_fields and names_family are stacks
 * (list head LAST), so the prepending helpers push and `a <> b <> rest` pushes b then a. */
#include "kernel.h"

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static void push(Heap *heap, Vec *xs, void *item) {
  if (item == NULL || !vec_push(xs, item)) heap->oom = true;
}

/* item <> rest */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  size_t i;
  push(heap, &out, item);
  for (i = 0; i < rest->len; i++) push(heap, &out, at(rest, i));
  return out;
}

/* List.append(a, b) */
static Vec append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_concat(heap->arena, a, b);
  if (out.len != a->len + b->len) heap->oom = true;
  return out;
}

static Str *box_str(Heap *heap, Str s) {
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) return NULL;
  *boxed = s;
  return boxed;
}

static CircuitValue *box_value(Heap *heap, CircuitValue v) {
  CircuitValue *boxed = heap_alloc(heap, sizeof(CircuitValue));
  if (boxed == NULL) return NULL;
  *boxed = v;
  return boxed;
}

static CircuitOccur *box_occur(Heap *heap, Nat depth, const Term *term) {
  CircuitOccur *boxed = heap_alloc(heap, sizeof(CircuitOccur));
  if (boxed == NULL || term == NULL) return NULL;
  boxed->depth = depth;
  boxed->term = term;
  return boxed;
}

static CircuitMemoRow *box_memo(Heap *heap, Str name, Big value) {
  CircuitMemoRow *boxed = heap_alloc(heap, sizeof(CircuitMemoRow));
  if (boxed == NULL) return NULL;
  boxed->name = name;
  boxed->value = value;
  return boxed;
}

static MaybeBig some_big(Big n) {
  MaybeBig m = {0};
  m.some = true;
  m.value = n;
  return m;
}

static MaybeBig none_big(void) {
  MaybeBig m = {0};
  return m;
}

static MaybeStr some_str(const char *text) {
  MaybeStr m = {0};
  m.some = true;
  m.value = str_lit(text);
  return m;
}

static CircuitBigResult done_big(Big n) {
  CircuitBigResult r = {0};
  r.kind = RESULT_DONE;
  r.value = n;
  return r;
}

static CircuitBigResult fail_big(Str e) {
  CircuitBigResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = e;
  return r;
}

static CircuitValueResult done_value(CircuitValue v) {
  CircuitValueResult r = {0};
  r.kind = RESULT_DONE;
  r.value = v;
  return r;
}

static CircuitValueResult fail_value(Str e) {
  CircuitValueResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = e;
  return r;
}

static CircuitBoundResult done_bound(Big value, Vec memo) {
  CircuitBoundResult r = {0};
  r.kind = RESULT_DONE;
  r.value.value = value;
  r.value.memo = memo;
  return r;
}

static CircuitBoundResult fail_bound(Str e) {
  CircuitBoundResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = e;
  return r;
}

static Str unbounded(void) { return str_lit("unbounded iteration"); }

/* ---- costs and bounds ---- */

Str circuit_word(Heap *heap, Str head) {
  return heap_str_cat(heap, str_lit("circuit fragment arrives at V5: "), head);
}

MaybeBig circuit_cost_case(bool yes) {
  if (yes) return some_big(big_one());
  return none_big();
}

MaybeBig circuit_cost(Str name) {
  return circuit_cost_case(str_eq(name, str_lit("natAdd")) || str_eq(name, str_lit("natSub")) ||
                           str_eq(name, str_lit("natMul")) || str_eq(name, str_lit("natEq")) ||
                           str_eq(name, str_lit("natLt")));
}

Big circuit_choose(bool yes, Big a, Big b) {
  if (yes) return a;
  return b;
}

MaybeBig circuit_choose_maybe(bool yes, MaybeBig a, MaybeBig b) {
  if (yes) return a;
  return b;
}

Big circuit_max(Big a, Big b) { return circuit_choose(big_le(a, b), b, a); }

CircuitBigResult circuit_bound_result(bool ok, Big n) {
  if (ok) return done_big(n);
  return fail_big(unbounded());
}

/* OCaml's positive machine integer ceiling is 2^62 - 1. */
static const uint32_t circuit_ceiling_limbs[5] = {32767u, 32767u, 32767u, 32767u, 3u};

CircuitBigResult circuit_bounded(Big n) {
  Big ceiling;
  ceiling.negative = false;
  ceiling.digits.items = circuit_ceiling_limbs;
  ceiling.digits.len = 5u;
  return circuit_bound_result(big_le(n, ceiling), n);
}

/* ---- lookup ---- */

MaybeTermPtr circuit_lookup_hit(bool hit, const Term *value, MaybeTermPtr rest) {
  if (hit) {
    MaybeTermPtr some = {true, value};
    return some;
  }
  return rest;
}

/* The Bend recursion resolves the tail first and the head wins: fold from the back. */
MaybeTermPtr circuit_lookup(const Vec *rows, Str name) {
  MaybeTermPtr found = {false, NULL};
  size_t i = rows->len;
  while (i > 0) {
    const CircuitGlobal *row = at(rows, i - 1);
    i -= 1;
    if (row != NULL) found = circuit_lookup_hit(str_eq(row->name, name), row->term, found);
  }
  return found;
}

bool circuit_member(const Vec *xs, Str name) {
  size_t i;
  for (i = 0; i < xs->len; i++) {
    const Str *x = at(xs, i);
    if (x != NULL && str_eq(*x, name)) return true;
  }
  return false;
}

/* ---- values ---- */

CircuitValue circuit_vd(Big depth) {
  CircuitValue v = {0};
  v.kind = CIRCUIT_VD;
  v.depth = depth;
  return v;
}

CircuitValue circuit_vclo(Leg leg, Vec env, Vec seen) {
  CircuitValue v = {0};
  v.kind = CIRCUIT_VCLO;
  v.leg = leg;
  v.env = env;
  v.seen = seen;
  return v;
}

CircuitGlobal *circuit_global_new(Heap *heap, Str name, const Term *term) {
  CircuitGlobal *boxed = heap_alloc(heap, sizeof(CircuitGlobal));
  if (boxed == NULL) return NULL;
  boxed->name = name;
  boxed->term = term;
  return boxed;
}

Big circuit_vd_of(CircuitValue v) {
  switch (v.kind) {
    case CIRCUIT_VD: return v.depth;
    case CIRCUIT_VCLO: return big_zero();
  }
  return big_zero();
}

CircuitValue circuit_nth_default(MaybeCircuitValue v) {
  if (v.some) return v.value;
  return circuit_vd(big_zero());
}

Vec circuit_push(Heap *heap, size_t n, CircuitValue v, const Vec *env) {
  Vec out = vec_new(heap->arena);
  CircuitValue *boxed;
  size_t i;
  if (n == 0) return *env;
  boxed = box_value(heap, v);
  for (i = 0; i < n; i++) push(heap, &out, boxed);
  for (i = 0; i < env->len; i++) push(heap, &out, at(env, i));
  return out;
}

MaybeStr circuit_shape_error(Shape s) {
  MaybeStr none = {0};
  switch (s.kind) {
    case SHAPE_PI: return none;
    case SHAPE_ARG: return none;
    case SHAPE_COLL: return none;
    case SHAPE_MU: return none;
    case SHAPE_PAR: return some_str("SPar");
    case SHAPE_NU: return some_str("SNu");
    case SHAPE_ZK: return some_str("SZk");
    case SHAPE_FHC: return some_str("SFhc");
    case SHAPE_MPC: return some_str("SMpc");
  }
  return none;
}

CircuitValueResult circuit_type_result(MaybeStr e) {
  if (e.some) return fail_value(e.value);
  return done_value(circuit_vd(big_zero()));
}

/* ---- literal bounds ---- */

MaybeBig circuit_memo_lookup(const Vec *rows, Str name) {
  MaybeBig found = none_big();
  size_t i = rows->len;
  while (i > 0) {
    const CircuitMemoRow *row = at(rows, i - 1);
    i -= 1;
    if (row != NULL) found = circuit_choose_maybe(str_eq(row->name, name), some_big(row->value), found);
  }
  return found;
}

Big circuit_bound_value(CircuitBoundAnswer a) { return a.value; }

Vec circuit_bound_memo(CircuitBoundAnswer a) { return a.memo; }

CircuitBoundTask circuit_bterm(const Term *term, Vec seen) {
  CircuitBoundTask t = {0};
  t.kind = CIRCUIT_BTERM;
  t.term = term;
  t.seen = seen;
  return t;
}

CircuitBoundTask circuit_bargs(Vec args, Vec seen) {
  CircuitBoundTask t = {0};
  t.kind = CIRCUIT_BARGS;
  t.args = args;
  t.seen = seen;
  return t;
}

CircuitBoundTask circuit_bglobal(bool cycle, MaybeBig cached, MaybeTermPtr body, Str name, Vec seen) {
  CircuitBoundTask t = {0};
  t.kind = CIRCUIT_BGLOBAL;
  t.cycle = cycle;
  t.cached = cached;
  t.body = body;
  t.name = name;
  t.seen = seen;
  return t;
}

static CircuitBoundResult bound_term(Heap *heap, const Term *t, Vec seen, const Vec *globals, Str family, Vec memo) {
  switch (t->kind) {
    case TERM_ANN: return circuit_literal_bound_go(heap, circuit_bterm(t->body, seen), globals, family, memo);
    case TERM_LIT: return done_bound(big_zero(), memo);
    case TERM_GLOBAL: {
      Vec next = cons(heap, box_str(heap, t->name), &seen);
      CircuitBoundTask task = circuit_bglobal(circuit_member(&seen, t->name), circuit_memo_lookup(&memo, t->name),
                                              circuit_lookup(globals, t->name), t->name, next);
      return circuit_literal_bound_go(heap, task, globals, family, memo);
    }
    case TERM_IN: {
      if (t->shape.kind != SHAPE_MU) return fail_bound(unbounded());
      CircuitBoundResult answer = circuit_literal_bound_go(heap, circuit_bargs(t->args, seen), globals, family, memo);
      switch (answer.kind) {
        case RESULT_DONE: {
          Big extra = circuit_choose(str_eq(t->shape.name, family), big_one(), big_zero());
          return done_bound(big_add(heap, circuit_bound_value(answer.value), extra), circuit_bound_memo(answer.value));
        }
        case RESULT_FAIL: return answer;
      }
      return answer;
    }
    case TERM_VAR: return fail_bound(unbounded());
    case TERM_UNIV: return fail_bound(unbounded());
    case TERM_LAN: return fail_bound(unbounded());
    case TERM_RAN: return fail_bound(unbounded());
    case TERM_ELIM: return fail_bound(unbounded());
    case TERM_SEC: return fail_bound(unbounded());
    case TERM_OUT: return fail_bound(unbounded());
    case TERM_LET: return fail_bound(unbounded());
    case TERM_AUTO: return fail_bound(unbounded());
  }
  return fail_bound(unbounded());
}

/* Bend: max(value(a), value(rest)) with the memo threaded head to tail. */
static CircuitBoundResult bound_args(Heap *heap, const Vec *args, Vec seen, const Vec *globals, Str family, Vec memo) {
  Big best = big_zero();
  size_t i;
  for (i = 0; i < args->len; i++) {
    CircuitBoundResult a = circuit_literal_bound_go(heap, circuit_bterm(at(args, i), seen), globals, family, memo);
    if (a.kind == RESULT_FAIL) return a;
    best = circuit_max(circuit_bound_value(a.value), best);
    memo = circuit_bound_memo(a.value);
  }
  return done_bound(best, memo);
}

static CircuitBoundResult bound_global(Heap *heap, CircuitBoundTask task, const Vec *globals, Str family, Vec memo) {
  if (task.cycle) return fail_bound(unbounded());
  if (task.cached.some) return done_bound(task.cached.value, memo);
  if (!task.body.some) return fail_bound(unbounded());
  CircuitBoundResult a = circuit_literal_bound_go(heap, circuit_bterm(task.body.value, task.seen), globals, family, memo);
  switch (a.kind) {
    case RESULT_DONE: {
      Big v = circuit_bound_value(a.value);
      Vec rest = circuit_bound_memo(a.value);
      return done_bound(v, cons(heap, box_memo(heap, task.name, v), &rest));
    }
    case RESULT_FAIL: return a;
  }
  return a;
}

CircuitBoundResult circuit_literal_bound_go(Heap *heap, CircuitBoundTask task, const Vec *globals, Str family, Vec memo) {
  switch (task.kind) {
    case CIRCUIT_BTERM: return bound_term(heap, task.term, task.seen, globals, family, memo);
    case CIRCUIT_BARGS: return bound_args(heap, &task.args, task.seen, globals, family, memo);
    case CIRCUIT_BGLOBAL: return bound_global(heap, task, globals, family, memo);
  }
  return fail_bound(unbounded());
}

CircuitBigResult circuit_literal_bound(Heap *heap, CircuitBoundTask task, const Vec *globals, Str family) {
  Vec empty = vec_new(heap->arena);
  CircuitBoundResult a = circuit_literal_bound_go(heap, task, globals, family, empty);
  switch (a.kind) {
    case RESULT_DONE: return done_big(circuit_bound_value(a.value));
    case RESULT_FAIL: return fail_big(a.error);
  }
  return fail_big(a.error);
}

/* ---- field reads (Occur worklists as stacks) ---- */

static Nat leg_depth(Heap *heap, Nat depth, const Leg *leg) {
  return heap_nat_add(heap, depth, heap_nat_of_u64(heap, (uint64_t)leg->binders.len));
}

void circuit_addr_terms(Heap *heap, Addr a, Vec *rest) {
  switch (a.kind) {
    case ADDR_PT: push(heap, rest, (void *)a.arg); return;
    case ADDR_LEG: return;
    case ADDR_CTOR: return;
  }
}

void circuit_addr_occurs(Heap *heap, Addr a, Nat depth, Vec *rest) {
  switch (a.kind) {
    case ADDR_PT: push(heap, rest, box_occur(heap, depth, a.arg)); return;
    case ADDR_LEG: return;
    case ADDR_CTOR: return;
  }
}

void circuit_terms_occurs(Heap *heap, const Vec *ts, Nat depth, Vec *rest) {
  size_t i = ts->len;
  while (i > 0) {
    push(heap, rest, box_occur(heap, depth, at(ts, i - 1)));
    i -= 1;
  }
}

void circuit_legs_occurs(Heap *heap, const Vec *ls, Nat depth, Vec *rest) {
  size_t i = ls->len;
  while (i > 0) {
    const Leg *leg = at(ls, i - 1);
    i -= 1;
    if (leg == NULL) {
      heap->oom = true;
      return;
    }
    push(heap, rest, box_occur(heap, leg_depth(heap, depth, leg), leg->body));
  }
}

void circuit_branches_occurs(Heap *heap, const Vec *bs, Nat depth, Vec *rest) {
  size_t i = bs->len;
  while (i > 0) {
    const Branch *b = at(bs, i - 1);
    i -= 1;
    if (b == NULL) {
      heap->oom = true;
      return;
    }
    push(heap, rest, box_occur(heap, leg_depth(heap, depth, &b->leg), b->leg.body));
    circuit_addr_occurs(heap, b->addr, depth, rest);
  }
}

bool circuit_reads_fields(Heap *heap, const Vec *tasks, Nat count) {
  Vec stack = vec_reverse(heap->arena, tasks);
  if (stack.len != tasks->len) {
    heap->oom = true;
    return false;
  }
  while (stack.len > 0) {
    const CircuitOccur *o = at(&stack, stack.len - 1);
    stack.len -= 1;
    if (o == NULL) return false;
    Nat d = o->depth;
    const Term *t = o->term;
    switch (t->kind) {
      case TERM_VAR:
        if (nat_cmp(t->index, d) >= 0 && nat_cmp(t->index, heap_nat_add(heap, d, count)) < 0) return true;
        break;
      case TERM_IN:
        circuit_terms_occurs(heap, &t->args, d, &stack);
        circuit_addr_occurs(heap, t->addr, d, &stack);
        break;
      case TERM_ELIM:
        circuit_branches_occurs(heap, &t->data->branches, d, &stack);
        push(heap, &stack, box_occur(heap, d, t->data->scrut));
        break;
      case TERM_SEC: circuit_legs_occurs(heap, &t->legs, d, &stack); break;
      case TERM_OUT:
        push(heap, &stack, box_occur(heap, d, t->head));
        circuit_addr_occurs(heap, t->addr, d, &stack);
        break;
      case TERM_LET:
        push(heap, &stack, box_occur(heap, heap_nat_succ(heap, d), t->body));
        push(heap, &stack, box_occur(heap, d, t->value));
        break;
      case TERM_ANN: push(heap, &stack, box_occur(heap, d, t->body)); break;
      case TERM_UNIV: break;
      case TERM_LAN: break;
      case TERM_RAN: break;
      case TERM_GLOBAL: break;
      case TERM_LIT: break;
      case TERM_AUTO: break;
    }
    if (heap->oom) return false;
  }
  return false;
}

bool circuit_branches_read(Heap *heap, const Vec *bs) {
  size_t i;
  for (i = 0; i < bs->len; i++) {
    const Branch *b = at(bs, i);
    Vec one = vec_new(heap->arena);
    if (b == NULL) return false;
    push(heap, &one, box_occur(heap, nat_zero(), b->leg.body));
    if (circuit_reads_fields(heap, &one, heap_nat_of_u64(heap, (uint64_t)b->leg.binders.len))) return true;
  }
  return false;
}

/* ---- spines and family names (term worklists as stacks) ---- */

CircuitSpine circuit_spine(Heap *heap, const Term *t, Vec args) {
  Vec collected = vec_new(heap->arena);
  Vec reversed;
  CircuitSpine s;
  const Term *head = t;
  bool more = true;
  while (more) {
    more = head->kind == TERM_OUT && (head->shape.kind == SHAPE_PI || head->shape.kind == SHAPE_ARG);
    if (more) {
      if (head->addr.kind == ADDR_PT) push(heap, &collected, (void *)head->addr.arg);
      head = head->head;
    }
  }
  reversed = vec_reverse(heap->arena, &collected);
  if (reversed.len != collected.len) heap->oom = true;
  s.head = head;
  s.args = append(heap, &reversed, &args);
  return s;
}

void circuit_shape_terms(Heap *heap, Shape s, Vec *rest) {
  switch (s.kind) {
    case SHAPE_PI: push(heap, rest, s.domain); return;
    case SHAPE_ARG: return;
    case SHAPE_COLL: return;
    case SHAPE_PAR:
      push(heap, rest, s.right);
      push(heap, rest, s.left);
      return;
    case SHAPE_MU:
    case SHAPE_NU: {
      size_t i = s.args.len;
      while (i > 0) {
        push(heap, rest, at(&s.args, i - 1));
        i -= 1;
      }
      push(heap, rest, (void *)term_global(heap, s.name));
      return;
    }
    case SHAPE_ZK: push(heap, rest, s.domain); return;
    case SHAPE_FHC: push(heap, rest, s.level); return;
    case SHAPE_MPC:
      push(heap, rest, s.access);
      push(heap, rest, s.parties);
      return;
  }
}

void circuit_legs_terms(Heap *heap, const Vec *ls, Vec *rest) {
  size_t i = ls->len;
  while (i > 0) {
    const Leg *leg = at(ls, i - 1);
    i -= 1;
    if (leg == NULL) {
      heap->oom = true;
      return;
    }
    push(heap, rest, (void *)leg->body);
  }
}

void circuit_branches_terms(Heap *heap, const Vec *bs, Vec *rest) {
  size_t i = bs->len;
  while (i > 0) {
    const Branch *b = at(bs, i - 1);
    i -= 1;
    if (b == NULL) {
      heap->oom = true;
      return;
    }
    push(heap, rest, (void *)b->leg.body);
    circuit_addr_terms(heap, b->addr, rest);
  }
}

void circuit_motive_terms(Heap *heap, MaybeMotive m, Vec *rest) {
  if (m.some) push(heap, rest, (void *)m.value.body);
}

bool circuit_names_family(Heap *heap, const Vec *ts, Str fam) {
  Vec stack = vec_reverse(heap->arena, ts);
  if (stack.len != ts->len) {
    heap->oom = true;
    return false;
  }
  while (stack.len > 0) {
    const Term *t = at(&stack, stack.len - 1);
    stack.len -= 1;
    if (t == NULL) return false;
    switch (t->kind) {
      case TERM_GLOBAL:
        if (str_eq(t->name, fam)) return true;
        break;
      case TERM_LAN:
      case TERM_RAN:
        push(heap, &stack, (void *)t->body);
        circuit_shape_terms(heap, t->shape, &stack);
        break;
      case TERM_IN: {
        size_t i = t->args.len;
        while (i > 0) {
          push(heap, &stack, at(&t->args, i - 1));
          i -= 1;
        }
        circuit_addr_terms(heap, t->addr, &stack);
        circuit_shape_terms(heap, t->shape, &stack);
        break;
      }
      case TERM_SEC:
        circuit_legs_terms(heap, &t->legs, &stack);
        circuit_shape_terms(heap, t->shape, &stack);
        break;
      case TERM_OUT:
        push(heap, &stack, (void *)t->head);
        circuit_addr_terms(heap, t->addr, &stack);
        circuit_shape_terms(heap, t->shape, &stack);
        break;
      case TERM_ELIM:
        circuit_branches_terms(heap, &t->data->branches, &stack);
        circuit_motive_terms(heap, t->data->motive, &stack);
        push(heap, &stack, (void *)t->data->scrut);
        circuit_shape_terms(heap, t->data->shape, &stack);
        break;
      case TERM_LET:
        push(heap, &stack, (void *)t->body);
        push(heap, &stack, (void *)t->value);
        push(heap, &stack, (void *)t->ty);
        break;
      case TERM_ANN:
        push(heap, &stack, (void *)t->ty);
        push(heap, &stack, (void *)t->body);
        break;
      case TERM_VAR: break;
      case TERM_UNIV: break;
      case TERM_LIT: break;
      case TERM_AUTO: break;
    }
    if (heap->oom) return false;
  }
  return false;
}

CircuitValueResult circuit_certificate_result(CircuitBigResult cert, bool recursive, CircuitValue v) {
  switch (cert.kind) {
    case RESULT_DONE: return done_value(v);
    case RESULT_FAIL:
      if (recursive) return fail_value(str_lit("mu"));
      return done_value(v);
  }
  return done_value(v);
}

/* ---- queries ---- */

static CircuitQuery query_eval(const Term *t, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_EVAL;
  q.term = t;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_args(Vec args, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_ARGS;
  q.args = args;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_legs(Vec legs, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_LEGS;
  q.legs = legs;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_branches(Vec branches, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_BRANCHES;
  q.branches = branches;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_address(Addr addr, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_ADDRESS;
  q.addr = addr;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_apply(CircuitValue value, Vec args, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_APPLY;
  q.value = value;
  q.args = args;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_capp(CircuitSpine spine, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_CAPP;
  q.term = spine.head;
  q.args = spine.args;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_global(bool cycle, Str name, MaybeTermPtr body, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_GLOBAL;
  q.cycle = cycle;
  q.name = name;
  q.body = body;
  q.seen = seen;
  return q;
}

static CircuitQuery query_primitive(MaybeBig cost, const Term *head, Vec args, Vec env, Vec seen) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_PRIMITIVE;
  q.cost = cost;
  q.term = head;
  q.args = args;
  q.env = env;
  q.seen = seen;
  return q;
}

static CircuitQuery query_unwrap(CircuitValue value) {
  CircuitQuery q = {0};
  q.kind = CIRCUIT_QUERY_UNWRAP;
  q.value = value;
  return q;
}

CircuitQuery circuit_global_query(Heap *heap, MaybeBig c, Str name, Vec seen, MaybeTermPtr body) {
  if (c.some) return query_unwrap(circuit_vd(c.value));
  return query_global(circuit_member(&seen, name), name, body, cons(heap, box_str(heap, name), &seen));
}

Big circuit_mu_depth(Heap *heap, bool read, Big n, Big best) {
  if (read) return big_mul(heap, n, best);
  return best;
}

/* ---- eval ---- */

static CircuitValueResult eval_in(Heap *heap, const Term *t, Vec env, Vec seen, const Vec *globals) {
  switch (t->shape.kind) {
    case SHAPE_PI: {
      CircuitValueResult av = circuit_eval(heap, query_address(t->addr, env, seen), globals);
      if (av.kind == RESULT_FAIL) return av;
      CircuitValueResult vs = circuit_eval(heap, query_args(t->args, env, seen), globals);
      if (vs.kind == RESULT_FAIL) return vs;
      return done_value(circuit_vd(circuit_max(circuit_vd_of(av.value), circuit_vd_of(vs.value))));
    }
    case SHAPE_COLL: return circuit_eval(heap, query_args(t->args, env, seen), globals);
    case SHAPE_MU: {
      CircuitValueResult vs = circuit_eval(heap, query_args(t->args, env, seen), globals);
      if (vs.kind == RESULT_FAIL) return vs;
      return circuit_certificate_result(circuit_literal_bound(heap, circuit_bargs(t->args, seen), globals, t->shape.name),
                                        circuit_names_family(heap, &t->args, t->shape.name), vs.value);
    }
    case SHAPE_ARG: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_PAR: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_NU: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_ZK: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_FHC: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_MPC: return circuit_type_result(circuit_shape_error(t->shape));
  }
  return circuit_type_result(circuit_shape_error(t->shape));
}

static CircuitValueResult eval_sec(Heap *heap, const Term *t, Vec env, Vec seen, const Vec *globals) {
  switch (t->shape.kind) {
    case SHAPE_PI: {
      const Leg *leg = t->legs.len == 1u ? at(&t->legs, 0) : NULL;
      if (leg == NULL) return fail_value(str_lit("auto"));
      return done_value(circuit_vclo(*leg, env, seen));
    }
    case SHAPE_COLL: return circuit_eval(heap, query_legs(t->legs, env, seen), globals);
    case SHAPE_MU: return fail_value(str_lit("mu"));
    case SHAPE_ARG: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_PAR: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_NU: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_ZK: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_FHC: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_MPC: return circuit_type_result(circuit_shape_error(t->shape));
  }
  return circuit_type_result(circuit_shape_error(t->shape));
}

static CircuitValueResult eval_out(Heap *heap, const Term *t, Vec env, Vec seen, const Vec *globals) {
  switch (t->shape.kind) {
    case SHAPE_PI:
    case SHAPE_ARG:
      return circuit_eval(heap, query_capp(circuit_spine(heap, t, vec_new(heap->arena)), env, seen), globals);
    case SHAPE_COLL: {
      CircuitValueResult v = circuit_eval(heap, query_eval(t->head, env, seen), globals);
      if (v.kind == RESULT_FAIL) return v;
      return done_value(circuit_vd(circuit_vd_of(v.value)));
    }
    case SHAPE_MU: return fail_value(str_lit("mu"));
    case SHAPE_PAR: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_NU: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_ZK: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_FHC: return circuit_type_result(circuit_shape_error(t->shape));
    case SHAPE_MPC: return circuit_type_result(circuit_shape_error(t->shape));
  }
  return circuit_type_result(circuit_shape_error(t->shape));
}

static CircuitValueResult eval_elim_mu(Heap *heap, const ElimData *d, Vec env, Vec seen, const Vec *globals) {
  CircuitValueResult b = circuit_eval(heap, query_branches(d->branches, env, seen), globals);
  if (b.kind == RESULT_FAIL) return b;
  CircuitBigResult n = circuit_literal_bound(heap, circuit_bterm(d->scrut, seen), globals, d->shape.name);
  if (n.kind == RESULT_FAIL) return fail_value(n.error);
  CircuitBigResult nonzero = circuit_bound_result(big_positive(n.value), n.value);
  if (nonzero.kind == RESULT_FAIL) return fail_value(nonzero.error);
  CircuitBigResult depth = circuit_bounded(
      circuit_mu_depth(heap, circuit_branches_read(heap, &d->branches), nonzero.value, circuit_vd_of(b.value)));
  if (depth.kind == RESULT_FAIL) return fail_value(depth.error);
  return done_value(circuit_vd(depth.value));
}

static CircuitValueResult eval_elim_other(Heap *heap, const ElimData *d, Vec env, Vec seen, const Vec *globals) {
  CircuitValueResult allowed = circuit_type_result(circuit_shape_error(d->shape));
  if (allowed.kind == RESULT_FAIL) return allowed;
  CircuitValueResult sv = circuit_eval(heap, query_eval(d->scrut, env, seen), globals);
  if (sv.kind == RESULT_FAIL) return sv;
  CircuitValueResult bv = circuit_eval(heap, query_branches(d->branches, env, seen), globals);
  if (bv.kind == RESULT_FAIL) return bv;
  CircuitBigResult n = circuit_bounded(big_add(heap, big_one(), circuit_vd_of(bv.value)));
  if (n.kind == RESULT_FAIL) return fail_value(n.error);
  return done_value(circuit_vd(circuit_max(circuit_vd_of(sv.value), n.value)));
}

static CircuitValueResult eval_elim(Heap *heap, const Term *t, Vec env, Vec seen, const Vec *globals) {
  const ElimData *d = t->data;
  switch (d->shape.kind) {
    case SHAPE_MU: return eval_elim_mu(heap, d, env, seen, globals);
    case SHAPE_PI: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_ARG: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_COLL: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_PAR: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_NU: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_ZK: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_FHC: return eval_elim_other(heap, d, env, seen, globals);
    case SHAPE_MPC: return eval_elim_other(heap, d, env, seen, globals);
  }
  return eval_elim_other(heap, d, env, seen, globals);
}

static CircuitValueResult eval_term(Heap *heap, const Term *t, Vec env, Vec seen, const Vec *globals) {
  switch (t->kind) {
    case TERM_VAR: {
      VecGet got = common_at(&env, t->index);
      MaybeCircuitValue found = {0};
      if (got.status == VEC_GET_OK) {
        found.some = true;
        found.value = *(const CircuitValue *)got.value;
      }
      return done_value(circuit_nth_default(found));
    }
    case TERM_UNIV: return done_value(circuit_vd(big_zero()));
    case TERM_LAN: return circuit_type_result(circuit_shape_error(t->shape));
    case TERM_RAN: return circuit_type_result(circuit_shape_error(t->shape));
    case TERM_LIT: return done_value(circuit_vd(big_zero()));
    case TERM_AUTO: return fail_value(str_lit("auto"));
    case TERM_ANN: return circuit_eval(heap, query_eval(t->body, env, seen), globals);
    case TERM_LET: {
      CircuitValueResult val = circuit_eval(heap, query_eval(t->value, env, seen), globals);
      if (val.kind == RESULT_FAIL) return val;
      return circuit_eval(heap, query_eval(t->body, cons(heap, box_value(heap, val.value), &env), seen), globals);
    }
    case TERM_GLOBAL:
      return circuit_eval(
          heap, circuit_global_query(heap, circuit_cost(t->name), t->name, seen, circuit_lookup(globals, t->name)),
          globals);
    case TERM_IN: return eval_in(heap, t, env, seen, globals);
    case TERM_SEC: return eval_sec(heap, t, env, seen, globals);
    case TERM_OUT: return eval_out(heap, t, env, seen, globals);
    case TERM_ELIM: return eval_elim(heap, t, env, seen, globals);
  }
  return fail_value(str_lit("auto"));
}

/* Bend: VD{max(vd_of(head), vd_of(rest))}, the head evaluated first. */
static CircuitValueResult eval_args(Heap *heap, const Vec *args, Vec env, Vec seen, const Vec *globals) {
  Big best = big_zero();
  size_t i;
  for (i = 0; i < args->len; i++) {
    CircuitValueResult av = circuit_eval(heap, query_eval(at(args, i), env, seen), globals);
    if (av.kind == RESULT_FAIL) return av;
    best = circuit_max(circuit_vd_of(av.value), best);
  }
  return done_value(circuit_vd(best));
}

static CircuitValueResult eval_legs(Heap *heap, const Vec *legs, Vec env, Vec seen, const Vec *globals) {
  Big best = big_zero();
  size_t i;
  for (i = 0; i < legs->len; i++) {
    const Leg *leg = at(legs, i);
    if (leg == NULL) return fail_value(str_lit("auto"));
    Vec inner = circuit_push(heap, leg->binders.len, circuit_vd(big_zero()), &env);
    CircuitValueResult v = circuit_eval(heap, query_eval(leg->body, inner, seen), globals);
    if (v.kind == RESULT_FAIL) return v;
    best = circuit_max(circuit_vd_of(v.value), best);
  }
  return done_value(circuit_vd(best));
}

static CircuitValueResult eval_address(Heap *heap, Addr addr, Vec env, Vec seen, const Vec *globals) {
  switch (addr.kind) {
    case ADDR_PT: return circuit_eval(heap, query_eval(addr.arg, env, seen), globals);
    case ADDR_LEG: return done_value(circuit_vd(big_zero()));
    case ADDR_CTOR: return done_value(circuit_vd(big_zero()));
  }
  return done_value(circuit_vd(big_zero()));
}

static CircuitValueResult eval_branches(Heap *heap, const Vec *bs, Vec env, Vec seen, const Vec *globals) {
  Big best = big_zero();
  size_t i;
  for (i = 0; i < bs->len; i++) {
    const Branch *b = at(bs, i);
    if (b == NULL) return fail_value(str_lit("auto"));
    CircuitValueResult av = circuit_eval(heap, query_address(b->addr, env, seen), globals);
    if (av.kind == RESULT_FAIL) return av;
    Vec inner = circuit_push(heap, b->leg.binders.len, circuit_vd(circuit_vd_of(av.value)), &env);
    CircuitValueResult v = circuit_eval(heap, query_eval(b->leg.body, inner, seen), globals);
    if (v.kind == RESULT_FAIL) return v;
    best = circuit_max(circuit_vd_of(v.value), best);
  }
  return done_value(circuit_vd(best));
}

static CircuitValueResult eval_capp(Heap *heap, const Term *head, Vec args, Vec env, Vec seen, const Vec *globals) {
  if (head->kind == TERM_GLOBAL) {
    return circuit_eval(heap, query_primitive(circuit_cost(head->name), head, args, env, seen), globals);
  }
  CircuitValueResult v = circuit_eval(heap, query_eval(head, env, seen), globals);
  if (v.kind == RESULT_FAIL) return v;
  return circuit_eval(heap, query_apply(v.value, args, env, seen), globals);
}

static CircuitValueResult eval_primitive(Heap *heap, MaybeBig c, const Term *head, Vec args, Vec env, Vec seen,
                                         const Vec *globals) {
  if (c.some) {
    CircuitValueResult v = circuit_eval(heap, query_args(args, env, seen), globals);
    if (v.kind == RESULT_FAIL) return v;
    CircuitBigResult d = circuit_bounded(big_add(heap, c.value, circuit_vd_of(v.value)));
    if (d.kind == RESULT_FAIL) return fail_value(d.error);
    return done_value(circuit_vd(d.value));
  }
  CircuitValueResult v = circuit_eval(heap, query_eval(head, env, seen), globals);
  if (v.kind == RESULT_FAIL) return v;
  return circuit_eval(heap, query_apply(v.value, args, env, seen), globals);
}

static CircuitValueResult eval_apply(Heap *heap, CircuitValue v, const Vec *args, Vec env, Vec seen,
                                     const Vec *globals) {
  size_t i;
  for (i = 0; i < args->len; i++) {
    switch (v.kind) {
      case CIRCUIT_VD: return fail_value(str_lit("opaque callee"));
      case CIRCUIT_VCLO: {
        CircuitValueResult av = circuit_eval(heap, query_eval(at(args, i), env, seen), globals);
        if (av.kind == RESULT_FAIL) return av;
        Vec captured = cons(heap, box_value(heap, av.value), &v.env);
        Vec open = append(heap, &v.seen, &seen);
        CircuitValueResult rv = circuit_eval(heap, query_eval(v.leg.body, captured, open), globals);
        if (rv.kind == RESULT_FAIL) return rv;
        v = rv.value;
        break;
      }
    }
  }
  return done_value(v);
}

static CircuitValueResult eval_global(Heap *heap, bool cycle, Str name, MaybeTermPtr body, Vec seen,
                                      const Vec *globals) {
  if (cycle) return fail_value(str_lit("mu"));
  if (!body.some) return fail_value(heap_str_cat(heap, str_lit("unknown global "), name));
  return circuit_eval(heap, query_eval(body.value, vec_new(heap->arena), seen), globals);
}

static CircuitValueResult eval_unwrap(Heap *heap, CircuitValue v, const Vec *globals) {
  bool closure = v.kind == CIRCUIT_VCLO;
  while (closure) {
    Vec inner = cons(heap, box_value(heap, circuit_vd(big_zero())), &v.env);
    CircuitValueResult result = circuit_eval(heap, query_eval(v.leg.body, inner, v.seen), globals);
    if (result.kind == RESULT_FAIL) return result;
    v = result.value;
    closure = v.kind == CIRCUIT_VCLO;
  }
  return done_value(circuit_vd(circuit_vd_of(v)));
}

CircuitValueResult circuit_eval(Heap *heap, CircuitQuery query, const Vec *globals) {
  switch (query.kind) {
    case CIRCUIT_QUERY_EVAL: return eval_term(heap, query.term, query.env, query.seen, globals);
    case CIRCUIT_QUERY_ARGS: return eval_args(heap, &query.args, query.env, query.seen, globals);
    case CIRCUIT_QUERY_LEGS: return eval_legs(heap, &query.legs, query.env, query.seen, globals);
    case CIRCUIT_QUERY_BRANCHES: return eval_branches(heap, &query.branches, query.env, query.seen, globals);
    case CIRCUIT_QUERY_ADDRESS: return eval_address(heap, query.addr, query.env, query.seen, globals);
    case CIRCUIT_QUERY_APPLY: return eval_apply(heap, query.value, &query.args, query.env, query.seen, globals);
    case CIRCUIT_QUERY_CAPP: return eval_capp(heap, query.term, query.args, query.env, query.seen, globals);
    case CIRCUIT_QUERY_GLOBAL: return eval_global(heap, query.cycle, query.name, query.body, query.seen, globals);
    case CIRCUIT_QUERY_PRIMITIVE:
      return eval_primitive(heap, query.cost, query.term, query.args, query.env, query.seen, globals);
    case CIRCUIT_QUERY_UNWRAP: return eval_unwrap(heap, query.value, globals);
  }
  return fail_value(str_lit("auto"));
}

CircuitBigResult circuit_depth(Heap *heap, const Vec *globals, const Term *t) {
  Vec empty = vec_new(heap->arena);
  CircuitValueResult v = circuit_eval(heap, query_eval(t, empty, empty), globals);
  if (v.kind == RESULT_FAIL) return fail_big(v.error);
  CircuitValueResult r = circuit_eval(heap, query_unwrap(v.value), globals);
  if (r.kind == RESULT_FAIL) return fail_big(r.error);
  return done_big(circuit_vd_of(r.value));
}
