/* kernel/erase_util.bend: the free variables of kernel and runtime terms, the capture
 * frames of closures and the runtime term reindexing (shift and prune). Every definition is
 * pure: a failed allocation sets heap->oom and the result is best effort, as in the other
 * kernel files. List<Nat> is a Vec of heap boxed Nat pointers (erase_util_nat_new), List<Slot>
 * a Vec of Slot box pointers (erase_repr_slot_box), List<FreeWork> and List<RuntimeWork> Vecs
 * of FreeWork and RuntimeWork pointers. */
#include "kernel.h"

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

/* Nat.sub: saturating at zero. */
static Nat sub(Heap *heap, Nat a, Nat b) {
  NatSub out = nat_sub(heap->arena, a, b);
  switch (out.status) {
  case NAT_SUB_OK:
    return out.value;
  case NAT_SUB_UNDERFLOW:
    return nat_zero();
  case NAT_SUB_OOM:
    heap->oom = true;
    return nat_zero();
  }
  return nat_zero();
}

static Nat nat_of_len(Heap *heap, size_t len) { return heap_nat_of_u64(heap, (uint64_t)len); }

/* The boxed Nat at index; zero out of range (every list item is a non NULL box). */
static Nat nat_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  const Nat *boxed = NULL;
  if (got.status == VEC_GET_OK) boxed = got.value;
  if (boxed == NULL) return nat_zero();
  return *boxed;
}

/* The Slot at index; SLOT_EXTRA (skipped by every walk) out of range. */
static Slot slot_at(const Vec *slots, size_t index) {
  VecGet got = vec_get(slots, index);
  if (got.status != VEC_GET_OK) return SLOT_EXTRA;
  return erase_repr_slot_unbox(got.value);
}

static void push_nat(Heap *heap, Vec *xs, Nat n) {
  Nat *boxed = erase_util_nat_new(heap, n);
  if (boxed != NULL) push(heap, xs, boxed);
}

static void push_free_work(Heap *heap, Vec *xs, Nat depth, const Term *term) {
  FreeWork *boxed = erase_util_free_work_new(heap, depth, term);
  if (boxed != NULL) push(heap, xs, boxed);
}

static void push_runtime_work(Heap *heap, Vec *xs, Nat depth, const Ktm *term) {
  RuntimeWork *boxed = erase_util_runtime_work_new(heap, depth, term);
  if (boxed != NULL) push(heap, xs, boxed);
}

/* ---- constructors and boxed list items ---- */

Nat *erase_util_nat_new(Heap *heap, Nat n) {
  Nat *boxed = heap_alloc(heap, sizeof(Nat));
  if (boxed == NULL) return NULL;
  *boxed = n;
  return boxed;
}

FreeWork *erase_util_free_work_new(Heap *heap, Nat depth, const Term *term) {
  FreeWork *boxed = heap_alloc(heap, sizeof(FreeWork));
  if (boxed == NULL) return NULL;
  boxed->depth = depth;
  boxed->term = term;
  return boxed;
}

RuntimeWork *erase_util_runtime_work_new(Heap *heap, Nat depth, const Ktm *term) {
  RuntimeWork *boxed = heap_alloc(heap, sizeof(RuntimeWork));
  if (boxed == NULL) return NULL;
  boxed->depth = depth;
  boxed->term = term;
  return boxed;
}

Reindex erase_util_shift(Nat amount) {
  Reindex out;
  out.kind = REINDEX_SHIFT;
  out.amount = amount;
  out.params = nat_zero();
  out.live.arena = NULL;
  out.live.items = NULL;
  out.live.len = 0;
  out.live.cap = 0;
  return out;
}

Reindex erase_util_prune(Nat params, Vec live) {
  Reindex out;
  out.kind = REINDEX_PRUNE;
  out.amount = nat_zero();
  out.params = params;
  out.live = live;
  return out;
}

Pruned erase_util_pruned(Vec reprs, Vec args, const Ktm *body) {
  Pruned out;
  out.reprs = reprs;
  out.args = args;
  out.body = body;
  return out;
}

/* ---- sorted Nat lists ---- */

bool erase_util_member(Nat n, const Vec *xs) {
  size_t i;
  for (i = 0; i < xs->len; i++) {
    Nat x = nat_at(xs, i);
    if (nat_cmp(n, x) == 0) return true;
  }
  return false;
}

Vec erase_util_insert(Heap *heap, Nat n, const Vec *xs) {
  Nat x;
  Vec rest;
  bool before;
  bool same;
  if (xs->len == 0) {
    Vec out = empty(heap);
    push_nat(heap, &out, n);
    return out;
  }
  x = nat_at(xs, 0);
  rest = list_tail(*xs);
  before = nat_cmp(n, x) < 0;
  same = nat_cmp(n, x) == 0;
  return erase_util_insert_order(heap, before, same, n, x, &rest);
}

Vec erase_util_insert_order(Heap *heap, bool before, bool same, Nat n, Nat x, const Vec *rest) {
  Nat *x_box = erase_util_nat_new(heap, x);
  if (before) {
    Nat *n_box = erase_util_nat_new(heap, n);
    Vec tail = cons(heap, x_box, rest);
    return cons(heap, n_box, &tail);
  }
  if (same) return cons(heap, x_box, rest);
  {
    Vec tail = erase_util_insert(heap, n, rest);
    return cons(heap, x_box, &tail);
  }
}

Vec erase_util_sorted(Heap *heap, const Vec *xs) {
  Vec out = empty(heap);
  size_t i;
  for (i = xs->len; i > 0; i--) {
    Nat x = nat_at(xs, i - 1);
    out = erase_util_insert(heap, x, &out);
  }
  return out;
}

/* ---- free variables of a kernel term ---- */

Vec erase_util_term_work(Heap *heap, const Vec *terms, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    if (got.status == VEC_GET_OK) push_free_work(heap, &out, depth, got.value);
  }
  return out;
}

Vec erase_util_leg_work(Heap *heap, const Vec *legs, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < legs->len; i++) {
    VecGet got = vec_get(legs, i);
    const Leg *leg = NULL;
    Nat count;
    Nat inner;
    if (got.status != VEC_GET_OK) continue;
    leg = got.value;
    count = nat_of_len(heap, leg->binders.len);
    inner = heap_nat_add(heap, depth, count);
    push_free_work(heap, &out, inner, leg->body);
  }
  return out;
}

Vec erase_util_branch_work(Heap *heap, const Vec *branches, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < branches->len; i++) {
    VecGet got = vec_get(branches, i);
    const Branch *branch = NULL;
    Nat count;
    Nat inner;
    if (got.status != VEC_GET_OK) continue;
    branch = got.value;
    count = nat_of_len(heap, branch->leg.binders.len);
    inner = heap_nat_add(heap, depth, count);
    push_free_work(heap, &out, inner, branch->leg.body);
  }
  return out;
}

Vec erase_util_addr_work(Heap *heap, Addr addr, Nat depth) {
  Vec out = empty(heap);
  switch (addr.kind) {
  case ADDR_PT:
    push_free_work(heap, &out, depth, addr.arg);
    return out;
  case ADDR_LEG:
    return out;
  case ADDR_CTOR:
    return out;
  }
  return out;
}

Vec erase_util_free_children(Heap *heap, const Term *term, Nat depth) {
  switch (term->kind) {
  case TERM_IN: {
    Vec head = erase_util_addr_work(heap, term->addr, depth);
    Vec rest = erase_util_term_work(heap, &term->args, depth);
    return append(heap, &head, &rest);
  }
  case TERM_OUT: {
    Vec head = erase_util_addr_work(heap, term->addr, depth);
    Vec rest = empty(heap);
    push_free_work(heap, &rest, depth, term->head);
    return append(heap, &head, &rest);
  }
  case TERM_SEC:
    return erase_util_leg_work(heap, &term->legs, depth);
  case TERM_ELIM: {
    const ElimData *data = term->data;
    Vec rest = erase_util_branch_work(heap, &data->branches, depth);
    FreeWork *scrut = erase_util_free_work_new(heap, depth, data->scrut);
    if (scrut == NULL) return rest;
    return cons(heap, scrut, &rest);
  }
  case TERM_LET: {
    Vec out = empty(heap);
    Nat inner;
    push_free_work(heap, &out, depth, term->value);
    inner = heap_nat_succ(heap, depth);
    push_free_work(heap, &out, inner, term->body);
    return out;
  }
  case TERM_ANN: {
    Vec out = empty(heap);
    push_free_work(heap, &out, depth, term->body);
    return out;
  }
  case TERM_VAR:
    return empty(heap);
  case TERM_UNIV:
    return empty(heap);
  case TERM_LAN:
    return empty(heap);
  case TERM_RAN:
    return empty(heap);
  case TERM_GLOBAL:
    return empty(heap);
  case TERM_LIT:
    return empty(heap);
  case TERM_AUTO:
    return empty(heap);
  }
  return empty(heap);
}

/* The Bend worklist recursion run as a loop over an explicit stack: the children of a term go
 * in front of the rest, so they are pushed in reverse order (same visit order, same output). */
Vec erase_util_free_walk(Heap *heap, const Vec *work) {
  Vec out = empty(heap);
  Vec stack = empty(heap);
  size_t i;
  for (i = work->len; i > 0; i--) {
    VecGet got = vec_get(work, i - 1);
    if (got.status == VEC_GET_OK) push(heap, &stack, got.value);
  }
  while (stack.len > 0) {
    VecGet top = vec_last(&stack);
    const FreeWork *item = NULL;
    const Term *term = NULL;
    stack.len -= 1;
    if (top.status != VEC_GET_OK) continue;
    item = top.value;
    term = item->term;
    if (term == NULL) continue;
    if (term->kind == TERM_VAR) {
      if (nat_cmp(term->index, item->depth) >= 0) {
        Nat rel = sub(heap, term->index, item->depth);
        push_nat(heap, &out, rel);
      }
    } else {
      Vec children = erase_util_free_children(heap, term, item->depth);
      size_t j;
      for (j = children.len; j > 0; j--) {
        VecGet child = vec_get(&children, j - 1);
        if (child.status == VEC_GET_OK) push(heap, &stack, child.value);
      }
    }
  }
  return out;
}

Vec erase_util_free_vars(Heap *heap, Nat depth, const Term *term) {
  Vec work = empty(heap);
  push_free_work(heap, &work, depth, term);
  return erase_util_free_walk(heap, &work);
}

/* ---- captures ---- */

MaybeNat erase_util_lookup(Heap *heap, const Vec *slots, Nat index, Nat seen) {
  MaybeNat out;
  Nat rest = index;
  Nat count = seen;
  size_t i;
  for (i = 0; i < slots->len; i++) {
    Slot slot = slot_at(slots, i);
    switch (slot) {
    case SLOT_EXTRA:
      count = heap_nat_succ(heap, count);
      break;
    case SLOT_KEEP:
      if (nat_is_zero(rest)) {
        out.some = true;
        out.value = count;
        return out;
      }
      rest = sub(heap, rest, heap_nat_of_u64(heap, 1));
      count = heap_nat_succ(heap, count);
      break;
    case SLOT_DROP:
      if (nat_is_zero(rest)) {
        out.some = false;
        out.value = nat_zero();
        return out;
      }
      rest = sub(heap, rest, heap_nat_of_u64(heap, 1));
      break;
    }
  }
  out.some = false;
  out.value = nat_zero();
  return out;
}

bool erase_util_is_some(MaybeNat value) { return value.some; }

Vec erase_util_filter_caps(Heap *heap, const Vec *xs, const Vec *slots) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < xs->len; i++) {
    VecGet got = vec_get(xs, i);
    Nat x;
    MaybeNat found;
    if (got.status != VEC_GET_OK) continue;
    x = nat_at(xs, i);
    found = erase_util_lookup(heap, slots, x, nat_zero());
    if (erase_util_is_some(found)) push(heap, &out, got.value);
  }
  return out;
}

Vec erase_util_captures_of(Heap *heap, EraseCtx ec, const Term *term) {
  Vec slots = erase_repr_slots(ec);
  Vec free = erase_util_free_vars(heap, nat_zero(), term);
  Vec caps = erase_util_filter_caps(heap, &free, &slots);
  return erase_util_sorted(heap, &caps);
}

Vec erase_util_frame(Heap *heap, const Vec *slots, const Vec *caps, Nat index) {
  Vec out = empty(heap);
  Nat at = index;
  size_t i;
  for (i = 0; i < slots->len; i++) {
    Slot slot = slot_at(slots, i);
    switch (slot) {
    case SLOT_EXTRA:
      break;
    case SLOT_DROP:
      push(heap, &out, erase_repr_slot_box(SLOT_DROP));
      at = heap_nat_succ(heap, at);
      break;
    case SLOT_KEEP: {
      bool keep = erase_util_member(at, caps);
      Slot kept = erase_repr_keep_slot(keep);
      push(heap, &out, erase_repr_slot_box(kept));
      at = heap_nat_succ(heap, at);
      break;
    }
    }
  }
  return out;
}

/* ---- free variables of a runtime term ---- */

Vec erase_util_runtime_work(Heap *heap, const Vec *terms, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    if (got.status == VEC_GET_OK) push_runtime_work(heap, &out, depth, got.value);
  }
  return out;
}

Vec erase_util_runtime_branch_work(Heap *heap, const Vec *branches, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < branches->len; i++) {
    VecGet got = vec_get(branches, i);
    const KtmBranch *branch = NULL;
    Nat inner;
    if (got.status != VEC_GET_OK) continue;
    branch = got.value;
    inner = heap_nat_add(heap, depth, branch->arity);
    push_runtime_work(heap, &out, inner, branch->body);
  }
  return out;
}

Vec erase_util_runtime_children(Heap *heap, const Ktm *term, Nat depth) {
  switch (term->kind) {
  case KTM_LET: {
    Vec out = empty(heap);
    Nat inner;
    push_runtime_work(heap, &out, depth, term->value);
    inner = heap_nat_succ(heap, depth);
    push_runtime_work(heap, &out, inner, term->body);
    return out;
  }
  case KTM_CLOS:
    return erase_util_runtime_work(heap, &term->captures, depth);
  case KTM_DELAY:
    return erase_util_runtime_work(heap, &term->captures, depth);
  case KTM_APP: {
    Vec rest = erase_util_runtime_work(heap, &term->args, depth);
    RuntimeWork *head = erase_util_runtime_work_new(heap, depth, term->head);
    if (head == NULL) return rest;
    return cons(heap, head, &rest);
  }
  case KTM_TAIL: {
    Vec rest = erase_util_runtime_work(heap, &term->args, depth);
    RuntimeWork *head = erase_util_runtime_work_new(heap, depth, term->head);
    if (head == NULL) return rest;
    return cons(heap, head, &rest);
  }
  case KTM_STRUCT:
    return erase_util_runtime_work(heap, &term->fields, depth);
  case KTM_TAG:
    return erase_util_runtime_work(heap, &term->fields, depth);
  case KTM_PROJ: {
    Vec out = empty(heap);
    push_runtime_work(heap, &out, depth, term->value);
    return out;
  }
  case KTM_FORCE: {
    Vec out = empty(heap);
    push_runtime_work(heap, &out, depth, term->value);
    return out;
  }
  case KTM_CASE: {
    Vec rest = erase_util_runtime_branch_work(heap, &term->branches, depth);
    RuntimeWork *scrut = erase_util_runtime_work_new(heap, depth, term->scrut);
    if (scrut == NULL) return rest;
    return cons(heap, scrut, &rest);
  }
  case KTM_VAR:
    return empty(heap);
  case KTM_LIT:
    return empty(heap);
  case KTM_GLOBAL:
    return empty(heap);
  case KTM_ERASED:
    return empty(heap);
  }
  return empty(heap);
}

/* The same explicit stack loop as erase_util_free_walk over runtime terms. */
Vec erase_util_runtime_walk(Heap *heap, const Vec *work) {
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
    const Ktm *term = NULL;
    stack.len -= 1;
    if (top.status != VEC_GET_OK) continue;
    item = top.value;
    term = item->term;
    if (term == NULL) continue;
    if (term->kind == KTM_VAR) {
      if (nat_cmp(term->index, item->depth) >= 0) {
        Nat rel = sub(heap, term->index, item->depth);
        push_nat(heap, &out, rel);
      }
    } else {
      Vec children = erase_util_runtime_children(heap, term, item->depth);
      size_t j;
      for (j = children.len; j > 0; j--) {
        VecGet child = vec_get(&children, j - 1);
        if (child.status == VEC_GET_OK) push(heap, &stack, child.value);
      }
    }
  }
  return out;
}

Vec erase_util_runtime_vars(Heap *heap, Nat depth, const Ktm *term) {
  Vec work = empty(heap);
  push_runtime_work(heap, &work, depth, term);
  return erase_util_runtime_walk(heap, &work);
}

/* ---- reindexing ---- */

Nat erase_util_count_before(Heap *heap, const Vec *xs, Nat index) {
  size_t count = 0;
  size_t i;
  for (i = 0; i < xs->len; i++) {
    Nat x = nat_at(xs, i);
    if (nat_cmp(x, index) < 0) count += 1;
  }
  return nat_of_len(heap, count);
}

Nat erase_util_reindex(Heap *heap, Reindex index, Nat i) {
  switch (index.kind) {
  case REINDEX_SHIFT:
    return heap_nat_add(heap, i, index.amount);
  case REINDEX_PRUNE: {
    Nat rel;
    Nat before;
    if (nat_cmp(i, index.params) < 0) return i;
    rel = sub(heap, i, index.params);
    before = erase_util_count_before(heap, &index.live, rel);
    return heap_nat_add(heap, index.params, before);
  }
  }
  return i;
}

Vec erase_util_reindex_list(Heap *heap, const Vec *terms, Reindex index, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    const Ktm *term = NULL;
    if (got.status != VEC_GET_OK) continue;
    term = erase_util_reindex_runtime(heap, index, depth, got.value);
    push(heap, &out, (void *)term);
  }
  return out;
}

Vec erase_util_reindex_branches(Heap *heap, const Vec *branches, Reindex index, Nat depth) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < branches->len; i++) {
    VecGet got = vec_get(branches, i);
    const KtmBranch *branch = NULL;
    const Ktm *body = NULL;
    KtmBranch *boxed = NULL;
    Nat inner;
    if (got.status != VEC_GET_OK) continue;
    branch = got.value;
    inner = heap_nat_add(heap, depth, branch->arity);
    body = erase_util_reindex_runtime(heap, index, inner, branch->body);
    boxed = eterm_branch_new(heap, branch->tag, branch->arity, body);
    if (boxed != NULL) push(heap, &out, boxed);
  }
  return out;
}

const Ktm *erase_util_reindex_runtime(Heap *heap, Reindex index, Nat depth, const Ktm *term) {
  switch (term->kind) {
  case KTM_VAR: {
    Nat rel;
    Nat moved;
    Nat shifted;
    if (nat_cmp(term->index, depth) < 0) return eterm_kvar(heap, term->index);
    rel = sub(heap, term->index, depth);
    moved = erase_util_reindex(heap, index, rel);
    shifted = heap_nat_add(heap, depth, moved);
    return eterm_kvar(heap, shifted);
  }
  case KTM_LET: {
    const Ktm *value = erase_util_reindex_runtime(heap, index, depth, term->value);
    Nat inner = heap_nat_succ(heap, depth);
    const Ktm *body = erase_util_reindex_runtime(heap, index, inner, term->body);
    return eterm_klet(heap, term->name, value, body);
  }
  case KTM_CLOS: {
    Vec captures = erase_util_reindex_list(heap, &term->captures, index, depth);
    return eterm_kclos(heap, term->fid, term->arity, captures);
  }
  case KTM_DELAY: {
    Vec captures = erase_util_reindex_list(heap, &term->captures, index, depth);
    return eterm_kdelay(heap, term->fid, captures);
  }
  case KTM_APP: {
    const Ktm *head = erase_util_reindex_runtime(heap, index, depth, term->head);
    Vec args = erase_util_reindex_list(heap, &term->args, index, depth);
    return eterm_kapp(heap, head, args);
  }
  case KTM_TAIL: {
    const Ktm *head = erase_util_reindex_runtime(heap, index, depth, term->head);
    Vec args = erase_util_reindex_list(heap, &term->args, index, depth);
    return eterm_ktail(heap, head, args);
  }
  case KTM_STRUCT: {
    Vec fields = erase_util_reindex_list(heap, &term->fields, index, depth);
    return eterm_kstruct(heap, term->tid, fields);
  }
  case KTM_TAG: {
    Vec fields = erase_util_reindex_list(heap, &term->fields, index, depth);
    return eterm_ktag(heap, term->tid, term->tag, fields);
  }
  case KTM_PROJ: {
    const Ktm *value = erase_util_reindex_runtime(heap, index, depth, term->value);
    return eterm_kproj(heap, term->tid, term->index, value);
  }
  case KTM_FORCE: {
    const Ktm *value = erase_util_reindex_runtime(heap, index, depth, term->value);
    return eterm_kforce(heap, value);
  }
  case KTM_CASE: {
    const Ktm *scrut = erase_util_reindex_runtime(heap, index, depth, term->scrut);
    Vec branches = erase_util_reindex_branches(heap, &term->branches, index, depth);
    return eterm_kcase(heap, term->tid, scrut, branches);
  }
  case KTM_LIT:
    return term;
  case KTM_GLOBAL:
    return term;
  case KTM_ERASED:
    return term;
  }
  return term;
}

const Ktm *erase_util_shift_runtime(Heap *heap, Nat by, Nat depth, const Ktm *term) {
  Reindex index = erase_util_shift(by);
  return erase_util_reindex_runtime(heap, index, depth, term);
}

Vec erase_util_live_captures(Heap *heap, const Vec *xs, Nat params, Nat count) {
  Vec out = empty(heap);
  Nat upper = heap_nat_add(heap, params, count);
  size_t i;
  for (i = 0; i < xs->len; i++) {
    Nat x = nat_at(xs, i);
    if (nat_cmp(x, params) >= 0 && nat_cmp(x, upper) < 0) {
      Nat rel = sub(heap, x, params);
      push_nat(heap, &out, rel);
    }
  }
  return out;
}

Vec erase_util_filter_live(Heap *heap, const Vec *xs, const Vec *live, Nat count, Nat index) {
  Vec out = empty(heap);
  Nat at = index;
  size_t i;
  for (i = 0; i < xs->len; i++) {
    VecGet got = vec_get(xs, i);
    Nat next;
    Nat from_end;
    if (got.status != VEC_GET_OK) continue;
    next = heap_nat_succ(heap, at);
    from_end = sub(heap, count, next);
    if (erase_util_member(from_end, live)) push(heap, &out, got.value);
    at = next;
  }
  return out;
}

Pruned erase_util_prune_captures(Heap *heap, const Vec *ps, const Vec *args, Nat params, const Ktm *body) {
  Nat count = nat_of_len(heap, ps->len);
  Nat zero = nat_zero();
  Vec vars = erase_util_runtime_vars(heap, zero, body);
  Vec found = erase_util_live_captures(heap, &vars, params, count);
  Vec live = erase_util_sorted(heap, &found);
  Reindex index = erase_util_prune(params, live);
  Vec reprs = erase_util_filter_live(heap, ps, &live, count, zero);
  Vec kept = erase_util_filter_live(heap, args, &live, count, zero);
  const Ktm *pruned = erase_util_reindex_runtime(heap, index, zero, body);
  return erase_util_pruned(reprs, kept, pruned);
}
