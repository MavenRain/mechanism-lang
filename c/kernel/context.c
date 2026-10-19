/* Port of kernel/context.bend: the typing context with its skew-binary local list. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

static Vec empty(Heap *heap) {
  return vec_new(heap->arena);
}

static bool nat_eq(Nat a, Nat b) {
  return nat_cmp(a, b) == 0;
}

static bool nat_lt(Nat a, Nat b) {
  return nat_cmp(a, b) < 0;
}

/* Nat.sub, saturating like the compiled Base. */
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

/* Nat.div(w, 2n) */
static Nat half_of(Heap *heap, Nat w) {
  NatDiv out = nat_divmod(heap->arena, w, heap_nat_of_u64(heap, 2));
  switch (out.status) {
  case NAT_DIV_OK:
    return out.quotient;
  case NAT_DIV_BY_ZERO:
    heap->oom = true;
    return nat_zero();
  case NAT_DIV_OOM:
    heap->oom = true;
    return nat_zero();
  }
  return nat_zero();
}

static MaybeLocal some_local(Local x) {
  MaybeLocal out;
  out.some = true;
  out.value = x;
  return out;
}

static MaybeLocal none_local(void) {
  MaybeLocal out;
  Local x;
  x.name = str_empty();
  x.quantity = QUANTITY_ZERO;
  x.ty = NULL;
  out.some = false;
  out.value = x;
  return out;
}

Local context_local(Str name, Quantity quantity, const Value *ty) {
  Local out;
  out.name = name;
  out.quantity = quantity;
  out.ty = ty;
  return out;
}

const LTree *context_lleaf(Heap *heap, Local x) {
  LTree *out = heap_alloc(heap, sizeof(LTree));
  if (out == NULL) return NULL;
  out->kind = LTREE_LEAF;
  out->x = x;
  out->l = NULL;
  out->r = NULL;
  return out;
}

const LTree *context_lnode(Heap *heap, Local x, const LTree *l, const LTree *r) {
  LTree *out = heap_alloc(heap, sizeof(LTree));
  if (out == NULL) return NULL;
  out->kind = LTREE_NODE;
  out->x = x;
  out->l = l;
  out->r = r;
  return out;
}

const Locals *context_lcons_cell(Heap *heap, Nat w, const LTree *t, const Locals *rest) {
  Locals *out = heap_alloc(heap, sizeof(Locals));
  if (out == NULL) return rest;
  out->w = w;
  out->t = t;
  out->rest = rest;
  return out;
}

const Locals *context_lcons_pair(Heap *heap, bool same, Local x, Nat w1, const LTree *t1, Nat w2,
                                 const LTree *t2, const Locals *rest) {
  if (same) {
    const LTree *node = context_lnode(heap, x, t1, t2);
    if (node == NULL) return rest;
    return context_lcons_cell(heap, heap_nat_succ(heap, heap_nat_add(heap, w1, w2)), node, rest);
  } else {
    const LTree *leaf = context_lleaf(heap, x);
    if (leaf == NULL) return rest;
    return context_lcons_cell(
        heap, heap_nat_of_u64(heap, 1), leaf,
        context_lcons_cell(heap, w1, t1, context_lcons_cell(heap, w2, t2, rest)));
  }
}

const Locals *context_lcons_one(Heap *heap, Local x, Nat w1, const LTree *t1, const Locals *rest) {
  if (rest == NULL) {
    const LTree *leaf = context_lleaf(heap, x);
    if (leaf == NULL) return rest;
    return context_lcons_cell(heap, heap_nat_of_u64(heap, 1), leaf,
                              context_lcons_cell(heap, w1, t1, NULL));
  }
  return context_lcons_pair(heap, nat_eq(w1, rest->w), x, w1, t1, rest->w, rest->t, rest->rest);
}

const Locals *context_lcons(Heap *heap, Local x, const Locals *xs) {
  if (xs == NULL) {
    const LTree *leaf = context_lleaf(heap, x);
    if (leaf == NULL) return xs;
    return context_lcons_cell(heap, heap_nat_of_u64(heap, 1), leaf, NULL);
  }
  return context_lcons_one(heap, x, xs->w, xs->t, xs->rest);
}

MaybeLocal context_ltree_side(Heap *heap, bool left, Nat half, const LTree *l, const LTree *r, Nat p) {
  if (left) return context_ltree_get(heap, half, l, p);
  return context_ltree_get(heap, half, r, sub(heap, p, half));
}

MaybeLocal context_ltree_node(Heap *heap, bool root, Nat half, Local x, const LTree *l, const LTree *r,
                              Nat p) {
  if (root) return some_local(x);
  return context_ltree_side(heap, nat_lt(p, half), half, l, r, p);
}

MaybeLocal context_ltree_get(Heap *heap, Nat w, const LTree *t, Nat i) {
  switch (t->kind) {
  case LTREE_LEAF:
    return some_local(t->x);
  case LTREE_NODE:
    return context_ltree_node(heap, nat_is_zero(i), half_of(heap, w), t->x, t->l, t->r,
                              sub(heap, i, heap_nat_of_u64(heap, 1)));
  }
  return none_local();
}

MaybeLocal context_lget_step(Heap *heap, bool inside, Nat w, const LTree *t, const Locals *rest, Nat i) {
  if (inside) return context_ltree_get(heap, w, t, i);
  return context_lget(heap, rest, sub(heap, i, w));
}

MaybeLocal context_lget(Heap *heap, const Locals *xs, Nat i) {
  if (xs == NULL) return none_local();
  return context_lget_step(heap, nat_lt(i, xs->w), xs->w, xs->t, xs->rest, i);
}

LView context_lview_tree(Heap *heap, Nat w, const LTree *t, const Locals *rest) {
  LView out;
  out.kind = LVIEW_CONS;
  out.head = t->x;
  switch (t->kind) {
  case LTREE_LEAF:
    out.tail = rest;
    return out;
  case LTREE_NODE: {
    Nat half = half_of(heap, w);
    out.tail = context_lcons_cell(heap, half, t->l, context_lcons_cell(heap, half, t->r, rest));
    return out;
  }
  }
  out.tail = rest;
  return out;
}

LView context_lview(Heap *heap, const Locals *xs) {
  if (xs == NULL) {
    LView out;
    out.kind = LVIEW_NIL;
    out.head = none_local().value;
    out.tail = NULL;
    return out;
  }
  return context_lview_tree(heap, xs->w, xs->t, xs->rest);
}

/* x <> ltree_list(l, ltree_list(r, acc)): the tree items in pre-order, then acc. */
static void ltree_push(Heap *heap, Vec *out, const LTree *t) {
  Local *boxed = heap_alloc(heap, sizeof(Local));
  if (boxed == NULL) return;
  *boxed = t->x;
  push(heap, out, boxed);
  switch (t->kind) {
  case LTREE_LEAF:
    return;
  case LTREE_NODE:
    ltree_push(heap, out, t->l);
    ltree_push(heap, out, t->r);
    return;
  }
}

Vec context_ltree_list(Heap *heap, const LTree *t, const Vec *acc) {
  Vec out = empty(heap);
  size_t i;
  ltree_push(heap, &out, t);
  for (i = 0; i < acc->len; i++) push(heap, &out, at(acc, i));
  return out;
}

Vec context_to_list(Heap *heap, const Locals *xs) {
  Vec out = empty(heap);
  const Locals *cell;
  for (cell = xs; cell != NULL; cell = cell->rest) ltree_push(heap, &out, cell->t);
  return out;
}

/* from_list(Con{x, rest}) = lcons(x, from_list(rest)): the fold runs from the last item back. */
const Locals *context_from_list(Heap *heap, const Vec *xs) {
  const Locals *out = NULL;
  size_t i;
  for (i = xs->len; i > 0; i--) {
    const Local *x = at(xs, i - 1);
    if (x == NULL) return out;
    out = context_lcons(heap, *x, out);
  }
  return out;
}

Ctx context_make(Heap *heap, Globals globals, Budget budget, Nat level_arity) {
  Ctx out;
  out.globals = globals;
  out.env = empty(heap);
  out.locals = NULL;
  out.size = nat_zero();
  out.budget = budget;
  out.level_arity = level_arity;
  out.levels_checked = false;
  return out;
}

Globals context_globals_of(Ctx ctx) {
  return ctx.globals;
}

Vec context_env_of(Ctx ctx) {
  return ctx.env;
}

Nat context_size_of(Ctx ctx) {
  return ctx.size;
}

Vec context_names(Heap *heap, const Vec *locals) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < locals->len; i++) {
    const Local *x = at(locals, i);
    Str *name = heap_alloc(heap, sizeof(Str));
    if (x == NULL || name == NULL) return out;
    *name = x->name;
    push(heap, &out, name);
  }
  return out;
}

Vec context_names_of(Heap *heap, Ctx ctx) {
  Vec locals = context_to_list(heap, ctx.locals);
  return context_names(heap, &locals);
}

/* value <> env */
static Vec env_cons(Heap *heap, const Value *value, const Vec *env) {
  Vec out = empty(heap);
  size_t i;
  push(heap, &out, (void *)value);
  for (i = 0; i < env->len; i++) push(heap, &out, at(env, i));
  return out;
}

Ctx context_bind(Heap *heap, Str name, Quantity q, const Value *ty, Ctx ctx) {
  Ctx out = ctx;
  out.env = env_cons(heap, value_var(heap, ctx.size), &ctx.env);
  out.locals = context_lcons(heap, context_local(name, q, ty), ctx.locals);
  out.size = heap_nat_succ(heap, ctx.size);
  return out;
}

Ctx context_define(Heap *heap, Str name, Quantity q, const Value *ty, const Value *value, Ctx ctx) {
  Ctx out = ctx;
  out.env = env_cons(heap, value, &ctx.env);
  out.locals = context_lcons(heap, context_local(name, q, ty), ctx.locals);
  out.size = heap_nat_succ(heap, ctx.size);
  return out;
}

bool context_readable(Quantity mode, Quantity q) {
  return quantity_equal(mode, QUANTITY_ZERO) || !quantity_equal(q, QUANTITY_ZERO);
}
