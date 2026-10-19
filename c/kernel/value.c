/* Port of kernel/value.bend: evaluated values, closures, stuck eliminators and neutral
 * spines. Same definitions in the same order. The generic containers are only ever instantiated
 * at T, so the payload is a const Value pointer; lists are Vecs of pointers in list order. */
#include "kernel.h"

/* ---- constructors ---- */

Head value_hlocal(Nat level) {
  Head h = {0};
  h.kind = HEAD_LOCAL;
  h.level = level;
  return h;
}

Head value_hglobal(Str name) {
  Head h = {0};
  h.kind = HEAD_GLOBAL;
  h.name = name;
  return h;
}

Closure value_closure(Vec env, const Term *body) {
  Closure c;
  c.env = env;
  c.body = body;
  return c;
}

VLeg value_vleg(Vec binders, Closure clo) {
  VLeg leg;
  leg.binders = binders;
  leg.clo = clo;
  return leg;
}

VAddr value_vaddr_pt(Quantity quantity, const Value *arg) {
  VAddr a = {0};
  a.kind = VADDR_PT;
  a.quantity = quantity;
  a.arg = arg;
  return a;
}

VAddr value_vaddr_leg(Big index) {
  VAddr a = {0};
  a.kind = VADDR_LEG;
  a.index = index;
  return a;
}

VAddr value_vaddr_ctor(Str name) {
  VAddr a = {0};
  a.kind = VADDR_CTOR;
  a.name = name;
  return a;
}

StuckElim value_stuck_elim(Shape shape, Quantity scrut_q, MaybeMotive motive, Vec branches, Vec env) {
  StuckElim s;
  s.shape = shape;
  s.scrut_q = scrut_q;
  s.motive = motive;
  s.branches = branches;
  s.env = env;
  return s;
}

Spine value_sout(Shape shape, VAddr addr) {
  Spine s = {0};
  s.kind = SPINE_OUT;
  s.shape = shape;
  s.addr = addr;
  return s;
}

Spine value_selim(StuckElim data) {
  Spine s = {0};
  s.kind = SPINE_ELIM;
  s.data = data;
  return s;
}

VLeg *value_vleg_new(Heap *heap, Vec binders, Closure clo) {
  VLeg *boxed = heap_alloc(heap, sizeof(VLeg));
  if (boxed == NULL) return NULL;
  *boxed = value_vleg(binders, clo);
  return boxed;
}

Spine *value_spine_new(Heap *heap, Spine spine) {
  Spine *boxed = heap_alloc(heap, sizeof(Spine));
  if (boxed == NULL) return NULL;
  *boxed = spine;
  return boxed;
}

/* VLit{LString{""}}: the zeroed Literal is LITERAL_STRING with the empty string. */
static const Value value_oom_placeholder = {.kind = VALUE_LIT};

static Value *value_new(Heap *heap, ValueKind kind) {
  Value *v = heap_alloc(heap, sizeof(Value));
  if (v != NULL) v->kind = kind;
  return v;
}

const Value *value_vuniv(Heap *heap, const LevelVar *level) {
  Value *v = value_new(heap, VALUE_UNIV);
  if (v == NULL) return &value_oom_placeholder;
  v->level = level;
  return v;
}

const Value *value_vlan(Heap *heap, Shape shape, Closure diagram, MaybeLevelVar level) {
  Value *v = value_new(heap, VALUE_LAN);
  if (v == NULL) return &value_oom_placeholder;
  v->shape = shape;
  v->diagram = diagram;
  v->former_level = level;
  return v;
}

const Value *value_vran(Heap *heap, Shape shape, Closure diagram, MaybeLevelVar level) {
  Value *v = value_new(heap, VALUE_RAN);
  if (v == NULL) return &value_oom_placeholder;
  v->shape = shape;
  v->diagram = diagram;
  v->former_level = level;
  return v;
}

const Value *value_vin(Heap *heap, Shape shape, VAddr addr, Vec args) {
  Value *v = value_new(heap, VALUE_IN);
  if (v == NULL) return &value_oom_placeholder;
  v->shape = shape;
  v->addr = addr;
  v->args = args;
  return v;
}

const Value *value_vsec(Heap *heap, Shape shape, Vec legs) {
  Value *v = value_new(heap, VALUE_SEC);
  if (v == NULL) return &value_oom_placeholder;
  v->shape = shape;
  v->legs = legs;
  return v;
}

const Value *value_vlit(Heap *heap, Literal value) {
  Value *v = value_new(heap, VALUE_LIT);
  if (v == NULL) return &value_oom_placeholder;
  v->lit = value;
  return v;
}

const Value *value_vneutral(Heap *heap, Head head, Vec spine) {
  Value *v = value_new(heap, VALUE_NEUTRAL);
  if (v == NULL) return &value_oom_placeholder;
  v->head = head;
  v->spine = spine;
  return v;
}

/* ---- definitions in the Bend order ---- */

VAddr value_valeg(Heap *heap, Nat index) {
  return value_vaddr_leg(big_of_nat(heap, index));
}

const Value *value_var(Heap *heap, Nat level) {
  return value_vneutral(heap, value_hlocal(level), vec_new(heap->arena));
}

Closure value_close(Vec env, const Term *body) {
  return value_closure(env, body);
}

MaybeLevelVar value_as_univ(const Value *value) {
  MaybeLevelVar out = {0};
  switch (value->kind) {
  case VALUE_UNIV:
    out.some = true;
    out.value = value->level;
    return out;
  case VALUE_LAN:
    return out;
  case VALUE_RAN:
    return out;
  case VALUE_IN:
    return out;
  case VALUE_SEC:
    return out;
  case VALUE_LIT:
    return out;
  case VALUE_NEUTRAL:
    return out;
  }
  return out;
}

bool value_head_equal(Head a, Head b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
  case HEAD_LOCAL:
    return nat_cmp(a.level, b.level) == 0;
  case HEAD_GLOBAL:
    return str_eq(a.name, b.name);
  }
  return false;
}
