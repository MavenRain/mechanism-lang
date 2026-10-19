/* Port of kernel/term.bend: kernel terms, the children walk, the name search and the
 * structural equality. Same definitions in the same order. The generic containers Addr<A>,
 * Leg<A>, Motive<A> and ElimData<A> are only ever instantiated at T, so the payload is a
 * const Term pointer; lists are Vecs of pointers in list order. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static Vec empty(Heap *heap) {
  return vec_new(heap->arena);
}

static Vec singleton(Heap *heap, const Term *term) {
  Vec out = empty(heap);
  push(heap, &out, (void *)term);
  return out;
}

/* List.append */
static Vec concat(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < a->len; i++) push(heap, &out, at(a, i));
  for (i = 0; i < b->len; i++) push(heap, &out, at(b, i));
  return out;
}

static void push_name(Heap *heap, Vec *xs, const char *text) {
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) return;
  *boxed = str_lit(text);
  push(heap, xs, boxed);
}

static const Term *term_at(const Vec *xs, size_t index) {
  return at(xs, index);
}

/* ---- constructors ---- */

Addr term_addr_pt(Quantity quantity, const Term *arg) {
  Addr a = {0};
  a.kind = ADDR_PT;
  a.quantity = quantity;
  a.arg = arg;
  return a;
}

Addr term_addr_leg(Big index) {
  Addr a = {0};
  a.kind = ADDR_LEG;
  a.index = index;
  return a;
}

Addr term_addr_ctor(Str name) {
  Addr a = {0};
  a.kind = ADDR_CTOR;
  a.name = name;
  return a;
}

Binder term_binder(Quantity quantity, Str name) {
  Binder b;
  b.quantity = quantity;
  b.name = name;
  return b;
}

Leg term_leg(Vec binders, const Term *body) {
  Leg leg;
  leg.binders = binders;
  leg.body = body;
  return leg;
}

Motive term_motive(MaybeStr ind, Vec indices, Str self, const Term *body) {
  Motive m;
  m.ind = ind;
  m.indices = indices;
  m.self = self;
  m.body = body;
  return m;
}

MaybeMotive term_motive_some(Motive motive) {
  MaybeMotive m;
  m.some = true;
  m.value = motive;
  return m;
}

MaybeMotive term_motive_none(void) {
  MaybeMotive m = {0};
  m.some = false;
  return m;
}

Branch term_branch(Addr addr, Leg leg) {
  Branch b;
  b.addr = addr;
  b.leg = leg;
  return b;
}

ElimData term_elim_data(Shape shape, const Term *scrut, Quantity scrut_q, MaybeMotive motive, Vec branches) {
  ElimData d;
  d.shape = shape;
  d.scrut = scrut;
  d.scrut_q = scrut_q;
  d.motive = motive;
  d.branches = branches;
  return d;
}

Binder *term_binder_new(Heap *heap, Quantity quantity, Str name) {
  Binder *boxed = heap_alloc(heap, sizeof(Binder));
  if (boxed == NULL) return NULL;
  *boxed = term_binder(quantity, name);
  return boxed;
}

Leg *term_leg_new(Heap *heap, Vec binders, const Term *body) {
  Leg *boxed = heap_alloc(heap, sizeof(Leg));
  if (boxed == NULL) return NULL;
  *boxed = term_leg(binders, body);
  return boxed;
}

Branch *term_branch_new(Heap *heap, Addr addr, Leg leg) {
  Branch *boxed = heap_alloc(heap, sizeof(Branch));
  if (boxed == NULL) return NULL;
  *boxed = term_branch(addr, leg);
  return boxed;
}

static const Term term_auto_value = {.kind = TERM_AUTO};

static Term *term_new(Heap *heap, TermKind kind) {
  Term *t = heap_alloc(heap, sizeof(Term));
  if (t != NULL) t->kind = kind;
  return t;
}

const Term *term_var(Heap *heap, Nat index) {
  Term *t = term_new(heap, TERM_VAR);
  if (t == NULL) return &term_auto_value;
  t->index = index;
  return t;
}

const Term *term_univ(Heap *heap, const LevelVar *level) {
  Term *t = term_new(heap, TERM_UNIV);
  if (t == NULL) return &term_auto_value;
  t->level = level;
  return t;
}

const Term *term_lan(Heap *heap, Shape shape, const Term *body) {
  Term *t = term_new(heap, TERM_LAN);
  if (t == NULL) return &term_auto_value;
  t->shape = shape;
  t->body = body;
  return t;
}

const Term *term_ran(Heap *heap, Shape shape, const Term *body) {
  Term *t = term_new(heap, TERM_RAN);
  if (t == NULL) return &term_auto_value;
  t->shape = shape;
  t->body = body;
  return t;
}

const Term *term_in(Heap *heap, Shape shape, Addr addr, Vec args) {
  Term *t = term_new(heap, TERM_IN);
  if (t == NULL) return &term_auto_value;
  t->shape = shape;
  t->addr = addr;
  t->args = args;
  return t;
}

const Term *term_elim(Heap *heap, ElimData data) {
  ElimData *boxed = heap_alloc(heap, sizeof(ElimData));
  Term *t = term_new(heap, TERM_ELIM);
  if (boxed == NULL || t == NULL) return &term_auto_value;
  *boxed = data;
  t->data = boxed;
  return t;
}

const Term *term_sec(Heap *heap, Shape shape, Vec legs) {
  Term *t = term_new(heap, TERM_SEC);
  if (t == NULL) return &term_auto_value;
  t->shape = shape;
  t->legs = legs;
  return t;
}

const Term *term_out(Heap *heap, Shape shape, Addr addr, const Term *head) {
  Term *t = term_new(heap, TERM_OUT);
  if (t == NULL) return &term_auto_value;
  t->shape = shape;
  t->addr = addr;
  t->head = head;
  return t;
}

const Term *term_let(Heap *heap, Str name, const Term *ty, const Term *value, const Term *body) {
  Term *t = term_new(heap, TERM_LET);
  if (t == NULL) return &term_auto_value;
  t->name = name;
  t->ty = ty;
  t->value = value;
  t->body = body;
  return t;
}

const Term *term_ann(Heap *heap, const Term *body, const Term *ty) {
  Term *t = term_new(heap, TERM_ANN);
  if (t == NULL) return &term_auto_value;
  t->body = body;
  t->ty = ty;
  return t;
}

const Term *term_global(Heap *heap, Str name) {
  Term *t = term_new(heap, TERM_GLOBAL);
  if (t == NULL) return &term_auto_value;
  t->name = name;
  return t;
}

const Term *term_lit(Heap *heap, Literal value) {
  Term *t = term_new(heap, TERM_LIT);
  if (t == NULL) return &term_auto_value;
  t->lit = value;
  return t;
}

const Term *term_auto(void) {
  return &term_auto_value;
}

/* ---- definitions in the Bend order ---- */

Addr term_aleg(Heap *heap, Nat index) {
  return term_addr_leg(big_of_nat(heap, index));
}

MaybeApt term_as_apt(Addr addr) {
  MaybeApt out = {0};
  switch (addr.kind) {
  case ADDR_PT:
    out.some = true;
    out.quantity = addr.quantity;
    out.arg = addr.arg;
    return out;
  case ADDR_LEG:
    return out;
  case ADDR_CTOR:
    return out;
  }
  return out;
}

MaybeBig term_as_aleg(Addr addr) {
  MaybeBig out = {0};
  switch (addr.kind) {
  case ADDR_PT:
    return out;
  case ADDR_LEG:
    out.some = true;
    out.value = addr.index;
    return out;
  case ADDR_CTOR:
    return out;
  }
  return out;
}

MaybeStr term_as_actor(Addr addr) {
  MaybeStr out = {0};
  switch (addr.kind) {
  case ADDR_PT:
    return out;
  case ADDR_LEG:
    return out;
  case ADDR_CTOR:
    out.some = true;
    out.value = addr.name;
    return out;
  }
  return out;
}

Vec term_addr_children(Heap *heap, Addr addr) {
  switch (addr.kind) {
  case ADDR_PT:
    return singleton(heap, addr.arg);
  case ADDR_LEG:
    return empty(heap);
  case ADDR_CTOR:
    return empty(heap);
  }
  return empty(heap);
}

Vec term_leg_children(Heap *heap, const Vec *legs) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < legs->len; i++) {
    const Leg *leg = at(legs, i);
    if (leg != NULL) push(heap, &out, (void *)leg->body);
  }
  return out;
}

Vec term_motive_children(Heap *heap, MaybeMotive motive) {
  if (motive.some) return singleton(heap, motive.value.body);
  return empty(heap);
}

Vec term_branch_children(Heap *heap, const Vec *branches) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < branches->len; i++) {
    const Branch *branch = at(branches, i);
    if (branch == NULL) continue;
    Vec from_addr = term_addr_children(heap, branch->addr);
    size_t j;
    for (j = 0; j < from_addr.len; j++) push(heap, &out, at(&from_addr, j));
    push(heap, &out, (void *)branch->leg.body);
  }
  return out;
}

Vec term_children(Heap *heap, const Term *term) {
  switch (term->kind) {
  case TERM_VAR:
    return empty(heap);
  case TERM_UNIV:
    return empty(heap);
  case TERM_LAN: {
    Vec payload = shape_payload(heap, term->shape);
    Vec body = singleton(heap, term->body);
    return concat(heap, &payload, &body);
  }
  case TERM_RAN: {
    Vec payload = shape_payload(heap, term->shape);
    Vec body = singleton(heap, term->body);
    return concat(heap, &payload, &body);
  }
  case TERM_IN: {
    Vec payload = shape_payload(heap, term->shape);
    Vec from_addr = term_addr_children(heap, term->addr);
    Vec tail = concat(heap, &from_addr, &term->args);
    return concat(heap, &payload, &tail);
  }
  case TERM_ELIM: {
    const ElimData *data = term->data;
    Vec payload = shape_payload(heap, data->shape);
    Vec from_motive = term_motive_children(heap, data->motive);
    Vec from_branches = term_branch_children(heap, &data->branches);
    Vec rest = concat(heap, &from_motive, &from_branches);
    Vec scrut = singleton(heap, data->scrut);
    Vec tail = concat(heap, &scrut, &rest);
    return concat(heap, &payload, &tail);
  }
  case TERM_SEC: {
    Vec payload = shape_payload(heap, term->shape);
    Vec from_legs = term_leg_children(heap, &term->legs);
    return concat(heap, &payload, &from_legs);
  }
  case TERM_OUT: {
    Vec payload = shape_payload(heap, term->shape);
    Vec from_addr = term_addr_children(heap, term->addr);
    Vec head = singleton(heap, term->head);
    Vec tail = concat(heap, &from_addr, &head);
    return concat(heap, &payload, &tail);
  }
  case TERM_LET: {
    Vec out = empty(heap);
    push(heap, &out, (void *)term->ty);
    push(heap, &out, (void *)term->value);
    push(heap, &out, (void *)term->body);
    return out;
  }
  case TERM_ANN: {
    Vec out = empty(heap);
    push(heap, &out, (void *)term->body);
    push(heap, &out, (void *)term->ty);
    return out;
  }
  case TERM_GLOBAL:
    return empty(heap);
  case TERM_LIT:
    return empty(heap);
  case TERM_AUTO:
    return empty(heap);
  }
  return empty(heap);
}

bool term_member(const Vec *names, Str name) {
  size_t i;
  for (i = 0; i < names->len; i++) {
    const Str *x = at(names, i);
    if (x != NULL && str_eq(*x, name)) return true;
  }
  return false;
}

bool term_family_member(MaybeStr name, const Vec *names) {
  if (name.some) return term_member(names, name.value);
  return false;
}

bool term_node_name(const Term *term, bool families, const Vec *names) {
  switch (term->kind) {
  case TERM_GLOBAL:
    return term_member(names, term->name);
  case TERM_LAN:
    return families && term_family_member(shape_family(term->shape), names);
  case TERM_RAN:
    return families && term_family_member(shape_family(term->shape), names);
  case TERM_IN:
    return families && term_family_member(shape_family(term->shape), names);
  case TERM_ELIM:
    return families && term_family_member(shape_family(term->data->shape), names);
  case TERM_SEC:
    return families && term_family_member(shape_family(term->shape), names);
  case TERM_OUT:
    return families && term_family_member(shape_family(term->shape), names);
  case TERM_VAR:
    return false;
  case TERM_UNIV:
    return false;
  case TERM_LET:
    return false;
  case TERM_ANN:
    return false;
  case TERM_LIT:
    return false;
  case TERM_AUTO:
    return false;
  }
  return false;
}

/* exists_walk(term :: rest) = node_name(term) || exists_walk(children(term) ++ rest): the list
 * front is the stack top, so the children go on in reverse and come off in the Bend order. */
bool term_exists_walk(Heap *heap, Vec todo, bool families, const Vec *names) {
  Vec stack = vec_reverse(heap->arena, &todo);
  if (stack.len != todo.len) {
    heap->oom = true;
    return false;
  }
  while (stack.len > 0) {
    const Term *term = term_at(&stack, stack.len - 1);
    stack.len -= 1;
    if (term == NULL) return false;
    if (term_node_name(term, families, names)) return true;
    Vec kids = term_children(heap, term);
    size_t i;
    for (i = kids.len; i > 0; i--) push(heap, &stack, at(&kids, i - 1));
    if (heap->oom) return false;
  }
  return false;
}

bool term_exists_name(Heap *heap, bool include_families, const Vec *names, const Term *term) {
  return term_exists_walk(heap, singleton(heap, term), include_families, names);
}

bool term_equal_list(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Term *x = term_at(a, i);
    const Term *y = term_at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!term_equal(x, y)) return false;
  }
  return true;
}

bool term_equal_strings(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Str *x = at(a, i);
    const Str *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!str_eq(*x, *y)) return false;
  }
  return true;
}

bool term_equal_option_string(MaybeStr a, MaybeStr b) {
  if (!a.some && !b.some) return true;
  if (a.some && b.some) return str_eq(a.value, b.value);
  return false;
}

bool term_equal_binders(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Binder *x = at(a, i);
    const Binder *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!quantity_equal(x->quantity, y->quantity)) return false;
    if (!str_eq(x->name, y->name)) return false;
  }
  return true;
}

bool term_equal_shape(Shape a, Shape b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
  case SHAPE_PI:
    return quantity_equal(a.quantity, b.quantity) && str_eq(a.name, b.name) && term_equal(a.domain, b.domain);
  case SHAPE_ARG:
    return quantity_equal(a.quantity, b.quantity) && str_eq(a.name, b.name);
  case SHAPE_ZK:
    return quantity_equal(a.quantity, b.quantity) && str_eq(a.name, b.name) && term_equal(a.domain, b.domain);
  case SHAPE_COLL:
    return big_equal(a.count, b.count);
  case SHAPE_PAR:
    return term_equal(a.left, b.left) && term_equal(a.right, b.right);
  case SHAPE_MU:
    return str_eq(a.name, b.name) && term_equal_list(&a.args, &b.args);
  case SHAPE_NU:
    return str_eq(a.name, b.name) && term_equal_list(&a.args, &b.args);
  case SHAPE_FHC:
    return term_equal(a.level, b.level);
  case SHAPE_MPC:
    return term_equal(a.parties, b.parties) && term_equal(a.access, b.access);
  }
  return false;
}

bool term_equal_addr(Addr a, Addr b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
  case ADDR_PT:
    return quantity_equal(a.quantity, b.quantity) && term_equal(a.arg, b.arg);
  case ADDR_LEG:
    return big_equal(a.index, b.index);
  case ADDR_CTOR:
    return str_eq(a.name, b.name);
  }
  return false;
}

bool term_equal_leg(Leg a, Leg b) {
  return term_equal_binders(&a.binders, &b.binders) && term_equal(a.body, b.body);
}

bool term_equal_legs(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Leg *x = at(a, i);
    const Leg *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!term_equal_leg(*x, *y)) return false;
  }
  return true;
}

bool term_equal_motive(MaybeMotive a, MaybeMotive b) {
  if (!a.some && !b.some) return true;
  if (a.some && b.some) {
    return term_equal_option_string(a.value.ind, b.value.ind) &&
           term_equal_strings(&a.value.indices, &b.value.indices) && str_eq(a.value.self, b.value.self) &&
           term_equal(a.value.body, b.value.body);
  }
  return false;
}

bool term_equal_branches(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Branch *x = at(a, i);
    const Branch *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!term_equal_addr(x->addr, y->addr)) return false;
    if (!term_equal_leg(x->leg, y->leg)) return false;
  }
  return true;
}

bool term_equal_elim(const ElimData *a, const ElimData *b) {
  return term_equal_shape(a->shape, b->shape) && term_equal(a->scrut, b->scrut) &&
         quantity_equal(a->scrut_q, b->scrut_q) && term_equal_motive(a->motive, b->motive) &&
         term_equal_branches(&a->branches, &b->branches);
}

bool term_equal(const Term *a, const Term *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case TERM_VAR:
    return nat_cmp(a->index, b->index) == 0;
  case TERM_UNIV:
    return level_var_structural_equal(a->level, b->level);
  case TERM_LAN:
    return term_equal_shape(a->shape, b->shape) && term_equal(a->body, b->body);
  case TERM_RAN:
    return term_equal_shape(a->shape, b->shape) && term_equal(a->body, b->body);
  case TERM_IN:
    return term_equal_shape(a->shape, b->shape) && term_equal_addr(a->addr, b->addr) &&
           term_equal_list(&a->args, &b->args);
  case TERM_ELIM:
    return term_equal_elim(a->data, b->data);
  case TERM_SEC:
    return term_equal_shape(a->shape, b->shape) && term_equal_legs(&a->legs, &b->legs);
  case TERM_OUT:
    return term_equal_shape(a->shape, b->shape) && term_equal_addr(a->addr, b->addr) &&
           term_equal(a->head, b->head);
  case TERM_LET:
    return str_eq(a->name, b->name) && term_equal(a->ty, b->ty) && term_equal(a->value, b->value) &&
           term_equal(a->body, b->body);
  case TERM_ANN:
    return term_equal(a->body, b->body) && term_equal(a->ty, b->ty);
  case TERM_GLOBAL:
    return str_eq(a->name, b->name);
  case TERM_LIT:
    return literal_equal(a->lit, b->lit);
  case TERM_AUTO:
    return true;
  }
  return false;
}

Vec term_formers(Heap *heap) {
  Vec out = empty(heap);
  push_name(heap, &out, "Lan");
  push_name(heap, &out, "Ran");
  return out;
}

Vec term_schema(Heap *heap) {
  Vec out = empty(heap);
  push_name(heap, &out, "In");
  push_name(heap, &out, "Elim");
  push_name(heap, &out, "Sec");
  push_name(heap, &out, "Out");
  return out;
}
