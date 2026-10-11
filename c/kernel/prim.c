/* Port of bend2/kernel/prim.bend: the primitive operations on naturals, their names, types and
 * evaluation on literal arguments. Same definitions in the same order. List<T> is a Vec of boxed
 * Prim pointers; List<Literal.T> is a Vec of Literal pointers. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

Str prim_name(Prim p) {
  switch (p) {
    case PRIM_NAT_ADD: return str_lit("natAdd");
    case PRIM_NAT_SUB: return str_lit("natSub");
    case PRIM_NAT_MUL: return str_lit("natMul");
    case PRIM_NAT_EQ: return str_lit("natEq");
    case PRIM_NAT_LT: return str_lit("natLt");
  }
  return str_empty();
}

Vec prim_catalog(Heap *heap) {
  static const Prim items[] = {PRIM_NAT_ADD, PRIM_NAT_SUB, PRIM_NAT_MUL, PRIM_NAT_EQ, PRIM_NAT_LT};
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < sizeof items / sizeof items[0]; i++) {
    Prim *boxed = heap_alloc(heap, sizeof(Prim));
    if (boxed == NULL) return out;
    *boxed = items[i];
    push(heap, &out, boxed);
  }
  return out;
}

Nat prim_arity(Heap *heap, Prim p) {
  (void)p;
  return heap_nat_of_u64(heap, 2);
}

MaybePrim prim_find(const Vec *ps, Str n) {
  MaybePrim none = {false, PRIM_NAT_ADD};
  size_t i;
  for (i = 0; i < ps->len; i++) {
    const Prim *p = at(ps, i);
    if (p != NULL && str_eq(prim_name(*p), n)) {
      MaybePrim some = {true, *p};
      return some;
    }
  }
  return none;
}

MaybePrim prim_of_name(Heap *heap, Str n) {
  Vec catalog = prim_catalog(heap);
  return prim_find(&catalog, n);
}

const Term *prim_nat_ty(Heap *heap) {
  return term_global(heap, str_lit("Nat"));
}

const Term *prim_unit_ty(Heap *heap) {
  Shape coll = shape_scoll(big_zero());
  const Term *body = term_ran(heap, coll, term_sec(heap, coll, vec_new(heap->arena)));
  return term_ann(heap, body, term_univ(heap, level_one(heap)));
}

const Term *prim_bool_ty(Heap *heap) {
  Shape coll = shape_scoll(big_of_u32(heap, 2));
  Vec legs = vec_new(heap->arena);
  Leg *first = term_leg_new(heap, vec_new(heap->arena), prim_unit_ty(heap));
  Leg *second = term_leg_new(heap, vec_new(heap->arena), prim_unit_ty(heap));
  if (first != NULL) push(heap, &legs, first);
  if (second != NULL) push(heap, &legs, second);
  return term_lan(heap, coll, term_sec(heap, coll, legs));
}

const Term *prim_arrow(Heap *heap, Quantity q, Str n, const Term *dom, const Term *cod) {
  return term_ran(heap, shape_spi(q, n, (void *)dom), cod);
}

const Term *prim_result_ty(Heap *heap, Prim p) {
  switch (p) {
    case PRIM_NAT_ADD: return prim_nat_ty(heap);
    case PRIM_NAT_SUB: return prim_nat_ty(heap);
    case PRIM_NAT_MUL: return prim_nat_ty(heap);
    case PRIM_NAT_EQ: return prim_bool_ty(heap);
    case PRIM_NAT_LT: return prim_bool_ty(heap);
  }
  return term_auto();
}

const Term *prim_ty(Heap *heap, Prim p) {
  const Term *inner = prim_arrow(heap, QUANTITY_MANY, str_lit("b"), prim_nat_ty(heap), prim_result_ty(heap, p));
  return prim_arrow(heap, QUANTITY_MANY, str_lit("a"), prim_nat_ty(heap), inner);
}

const Value *prim_bool_value(Heap *heap, bool b) {
  Shape two = shape_scoll(big_of_u32(heap, 2));
  Vec args = vec_new(heap->arena);
  const Value *unit = value_vsec(heap, shape_scoll(big_zero()), vec_new(heap->arena));
  push(heap, &args, (void *)unit);
  return value_vin(heap, two, value_vaddr_leg(b ? big_one() : big_zero()), args);
}

const Value *prim_apply_pair(Heap *heap, Prim p, Big a, Big b) {
  switch (p) {
    case PRIM_NAT_ADD: return value_vlit(heap, literal_lint(big_add(heap, a, b)));
    case PRIM_NAT_SUB: return value_vlit(heap, literal_lint(big_sub(heap, a, b)));
    case PRIM_NAT_MUL: return value_vlit(heap, literal_lint(big_mul(heap, a, b)));
    case PRIM_NAT_EQ: return prim_bool_value(heap, big_equal(a, b));
    case PRIM_NAT_LT: return prim_bool_value(heap, !big_le(b, a));
  }
  return value_vlit(heap, literal_lstring(str_empty()));
}

MaybeValue prim_apply(Heap *heap, Prim p, const Vec *args) {
  MaybeValue none = {false, NULL};
  const Literal *a;
  const Literal *b;
  bool naturals;
  if (args->len != 2) return none;
  a = at(args, 0);
  b = at(args, 1);
  if (a == NULL || b == NULL) return none;
  if (a->kind != LITERAL_INT || b->kind != LITERAL_INT) return none;
  naturals = big_le(big_zero(), a->integer) && big_le(big_zero(), b->integer);
  if (!naturals) return none;
  {
    MaybeValue some = {true, prim_apply_pair(heap, p, a->integer, b->integer)};
    return some;
  }
}
