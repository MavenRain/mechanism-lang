/* Port of kernel/shape.bend: the shape functor T<A> over a payload type A, erased to
 * void pointers. Same definitions in the same order. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static Shape shape_blank(ShapeKind kind) {
  Shape s = {0};
  s.kind = kind;
  return s;
}

Shape shape_spi(Quantity quantity, Str name, void *domain) {
  Shape s = shape_blank(SHAPE_PI);
  s.quantity = quantity;
  s.name = name;
  s.domain = domain;
  return s;
}

Shape shape_sarg(Quantity quantity, Str name) {
  Shape s = shape_blank(SHAPE_ARG);
  s.quantity = quantity;
  s.name = name;
  return s;
}

Shape shape_scoll(Big count) {
  Shape s = shape_blank(SHAPE_COLL);
  s.count = count;
  return s;
}

Shape shape_spar(void *left, void *right) {
  Shape s = shape_blank(SHAPE_PAR);
  s.left = left;
  s.right = right;
  return s;
}

Shape shape_smu(Str name, Vec args) {
  Shape s = shape_blank(SHAPE_MU);
  s.name = name;
  s.args = args;
  return s;
}

Shape shape_snu(Str name, Vec args) {
  Shape s = shape_blank(SHAPE_NU);
  s.name = name;
  s.args = args;
  return s;
}

Shape shape_szk(Quantity quantity, Str name, void *domain) {
  Shape s = shape_blank(SHAPE_ZK);
  s.quantity = quantity;
  s.name = name;
  s.domain = domain;
  return s;
}

Shape shape_sfhc(void *level) {
  Shape s = shape_blank(SHAPE_FHC);
  s.level = level;
  return s;
}

Shape shape_smpc(void *parties, void *access) {
  Shape s = shape_blank(SHAPE_MPC);
  s.parties = parties;
  s.access = access;
  return s;
}

Shape shape_coll(Heap *heap, Nat count) { return shape_scoll(big_of_nat(heap, count)); }

Vec shape_payload(Heap *heap, Shape s) {
  Vec items = vec_new(heap->arena);
  switch (s.kind) {
    case SHAPE_PI:
      push(heap, &items, s.domain);
      return items;
    case SHAPE_ARG: return items;
    case SHAPE_COLL: return items;
    case SHAPE_PAR:
      push(heap, &items, s.left);
      push(heap, &items, s.right);
      return items;
    case SHAPE_MU: return s.args;
    case SHAPE_NU: return s.args;
    case SHAPE_ZK:
      push(heap, &items, s.domain);
      return items;
    case SHAPE_FHC:
      push(heap, &items, s.level);
      return items;
    case SHAPE_MPC:
      push(heap, &items, s.parties);
      push(heap, &items, s.access);
      return items;
  }
  return items;
}

MaybeStr shape_family(Shape s) {
  MaybeStr none = {false, str_empty()};
  MaybeStr some = {true, s.name};
  switch (s.kind) {
    case SHAPE_MU: return some;
    case SHAPE_NU: return some;
    case SHAPE_PI: return none;
    case SHAPE_ARG: return none;
    case SHAPE_COLL: return none;
    case SHAPE_PAR: return none;
    case SHAPE_ZK: return none;
    case SHAPE_FHC: return none;
    case SHAPE_MPC: return none;
  }
  return none;
}

MaybePtr shape_point_dom(Shape s) {
  MaybePtr none = {false, NULL};
  MaybePtr some = {true, s.domain};
  switch (s.kind) {
    case SHAPE_PI: return some;
    case SHAPE_ZK: return some;
    case SHAPE_ARG: return none;
    case SHAPE_COLL: return none;
    case SHAPE_PAR: return none;
    case SHAPE_MU: return none;
    case SHAPE_NU: return none;
    case SHAPE_FHC: return none;
    case SHAPE_MPC: return none;
  }
  return none;
}

bool shape_is_pi(Shape s) {
  switch (s.kind) {
    case SHAPE_PI: return true;
    case SHAPE_ARG: return false;
    case SHAPE_COLL: return false;
    case SHAPE_PAR: return false;
    case SHAPE_MU: return false;
    case SHAPE_NU: return false;
    case SHAPE_ZK: return false;
    case SHAPE_FHC: return false;
    case SHAPE_MPC: return false;
  }
  return false;
}

Str shape_name(Shape shape) {
  switch (shape.kind) {
    case SHAPE_PI: return str_lit("SPi");
    case SHAPE_ARG: return str_lit("SArg");
    case SHAPE_COLL: return str_lit("SColl");
    case SHAPE_PAR: return str_lit("SPar");
    case SHAPE_MU: return str_lit("SMu");
    case SHAPE_NU: return str_lit("SNu");
    case SHAPE_ZK: return str_lit("SZk");
    case SHAPE_FHC: return str_lit("SFhc");
    case SHAPE_MPC: return str_lit("SMpc");
  }
  return str_empty();
}

Vec shape_declared(Heap *heap) {
  static const char *const names[] = {"SPi", "SArg", "SColl", "SPar", "SMu", "SNu", "SZk", "SFhc", "SMpc"};
  Vec items = vec_new(heap->arena);
  for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
    Str *item = heap_alloc(heap, sizeof(Str));
    if (item == NULL) return items;
    *item = str_lit(names[i]);
    push(heap, &items, item);
  }
  return items;
}
