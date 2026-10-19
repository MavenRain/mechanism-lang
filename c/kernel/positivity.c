/* Port of kernel/positivity.bend: constructor telescopes, families and the strict
 * positivity check. Same definitions in the same order. Telescope is a Vec of TeleBinder
 * pointers, List<Ctor> a Vec of Ctor pointers, List<String> a Vec of Str pointers. */
#include "kernel.h"

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

/* ---- constructors ---- */

TeleBinder positivity_binder(Quantity quantity, Str name, const Term *ty) {
  TeleBinder b;
  b.quantity = quantity;
  b.name = name;
  b.ty = ty;
  return b;
}

CtorStatus positivity_provisional(void) {
  CtorStatus s = {0};
  s.kind = CTOR_STATUS_PROVISIONAL;
  return s;
}

CtorStatus positivity_builtin(void) {
  CtorStatus s = {0};
  s.kind = CTOR_STATUS_BUILTIN;
  return s;
}

CtorStatus positivity_complete(Vec names) {
  CtorStatus s = {0};
  s.kind = CTOR_STATUS_COMPLETE;
  s.names = names;
  return s;
}

Ctor positivity_ctor(Str name, Vec args, Vec res_idx, Nat full_arity, bool self_rec) {
  Ctor c;
  c.name = name;
  c.args = args;
  c.res_idx = res_idx;
  c.full_arity = full_arity;
  c.self_rec = self_rec;
  return c;
}

Family positivity_family(Str name, Vec params, Vec indices, const LevelVar *level, CtorStatus status, Vec ctors,
                         bool positive) {
  Family f;
  f.name = name;
  f.params = params;
  f.indices = indices;
  f.level = level;
  f.status = status;
  f.ctors = ctors;
  f.positive = positive;
  return f;
}

TeleBinder *positivity_binder_new(Heap *heap, Quantity quantity, Str name, const Term *ty) {
  TeleBinder *boxed = heap_alloc(heap, sizeof(TeleBinder));
  if (boxed == NULL) return NULL;
  *boxed = positivity_binder(quantity, name, ty);
  return boxed;
}

Ctor *positivity_ctor_new(Heap *heap, Ctor ctor) {
  Ctor *boxed = heap_alloc(heap, sizeof(Ctor));
  if (boxed == NULL) return NULL;
  *boxed = ctor;
  return boxed;
}

/* ---- lookup ---- */

MaybeCtor positivity_find_ctor(const Vec *ctors, Str name) {
  MaybeCtor none = {0};
  size_t i;
  for (i = 0; i < ctors->len; i++) {
    const Ctor *c = at(ctors, i);
    if (c != NULL && str_eq(c->name, name)) {
      MaybeCtor some = {0};
      some.some = true;
      some.value = *c;
      return some;
    }
  }
  return none;
}

MaybeCtor positivity_ctor_of(Str name, Family family) {
  return positivity_find_ctor(&family.ctors, name);
}

/* ---- occurrence ---- */

bool positivity_occurs(Heap *heap, const Vec *names, const Term *term) {
  return term_exists_name(heap, true, names, term);
}

bool positivity_absent(Heap *heap, const Vec *names, const Term *term) {
  return !positivity_occurs(heap, names, term);
}

bool positivity_absent_list(Heap *heap, const Vec *terms, const Vec *names) {
  size_t i;
  for (i = 0; i < terms->len; i++) {
    if (!positivity_absent(heap, names, at(terms, i))) return false;
  }
  return true;
}

MaybePtr positivity_point_dom(Shape s) {
  MaybePtr none = {false, NULL};
  MaybePtr dom = {true, s.domain};
  switch (s.kind) {
    case SHAPE_PI: return dom;
    case SHAPE_ZK: return dom;
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

/* ---- strict positivity ---- */

bool positivity_positive_ran(Heap *heap, MaybePtr dom, const Term *cod, const Term *whole, const Vec *names) {
  if (!dom.some) return positivity_absent(heap, names, whole);
  return positivity_absent(heap, names, dom.value) && positivity_positive(heap, cod, names);
}

bool positivity_positive_out(Heap *heap, MaybePtr dom, Addr addr, const Term *head, const Term *whole,
                             const Vec *names) {
  if (!dom.some) return positivity_absent(heap, names, whole);
  switch (addr.kind) {
    case ADDR_PT: return positivity_absent(heap, names, addr.arg) && positivity_positive(heap, head, names);
    case ADDR_LEG: return positivity_absent(heap, names, whole);
    case ADDR_CTOR: return positivity_absent(heap, names, whole);
  }
  return positivity_absent(heap, names, whole);
}

static bool positive_lan(Heap *heap, const Term *term, const Vec *names) {
  if (term_family_member(shape_family(term->shape), names)) {
    Vec payload = shape_payload(heap, term->shape);
    return positivity_absent_list(heap, &payload, names) && positivity_absent(heap, names, term->body);
  }
  return positivity_absent(heap, names, term);
}

bool positivity_positive(Heap *heap, const Term *term, const Vec *names) {
  switch (term->kind) {
    case TERM_LAN: return positive_lan(heap, term, names);
    case TERM_RAN: return positivity_positive_ran(heap, positivity_point_dom(term->shape), term->body, term, names);
    case TERM_OUT:
      return positivity_positive_out(heap, positivity_point_dom(term->shape), term->addr, term->head, term, names);
    case TERM_GLOBAL: return true;
    case TERM_VAR: return true;
    case TERM_UNIV: return true;
    case TERM_LIT: return true;
    case TERM_AUTO: return true;
    case TERM_IN: return positivity_absent(heap, names, term);
    case TERM_ELIM: return positivity_absent(heap, names, term);
    case TERM_SEC: return positivity_absent(heap, names, term);
    case TERM_LET: return positivity_absent(heap, names, term);
    case TERM_ANN: return positivity_absent(heap, names, term);
  }
  return true;
}

bool positivity_fields_positive(Heap *heap, const Vec *args, const Vec *names) {
  size_t i;
  for (i = 0; i < args->len; i++) {
    const TeleBinder *b = at(args, i);
    if (b == NULL) return false;
    if (!positivity_positive(heap, b->ty, names)) return false;
  }
  return true;
}

Result positivity_ctor_fields(Heap *heap, const Vec *names, const Vec *args) {
  Result r = {0};
  if (positivity_fields_positive(heap, args, names)) {
    r.kind = RESULT_DONE;
    r.value = kernel_unit();
    return r;
  }
  r.kind = RESULT_FAIL;
  r.error = error_make(ERROR_NOT_YET, str_lit("a family that is not strictly positive arrives at M2"));
  return r;
}

bool positivity_self_rec(Heap *heap, const Vec *args, const Vec *names) {
  size_t i;
  for (i = 0; i < args->len; i++) {
    const TeleBinder *b = at(args, i);
    if (b != NULL && positivity_occurs(heap, names, b->ty)) return true;
  }
  return false;
}

/* ---- structural comparison ---- */

bool positivity_same_telescope(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const TeleBinder *x = at(a, i);
    const TeleBinder *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!quantity_equal(x->quantity, y->quantity)) return false;
    if (!str_eq(x->name, y->name)) return false;
    if (!term_equal(x->ty, y->ty)) return false;
  }
  return true;
}

bool positivity_same_status(CtorStatus a, CtorStatus b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case CTOR_STATUS_PROVISIONAL: return true;
    case CTOR_STATUS_BUILTIN: return true;
    case CTOR_STATUS_COMPLETE: return term_equal_strings(&a.names, &b.names);
  }
  return false;
}

bool positivity_same_bool(bool a, bool b) {
  return a ? b : !b;
}

bool positivity_same_ctors(const Vec *a, const Vec *b, bool ignore_self_rec) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Ctor *x = at(a, i);
    const Ctor *y = at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!str_eq(x->name, y->name)) return false;
    if (!positivity_same_telescope(&x->args, &y->args)) return false;
    if (!term_equal_list(&x->res_idx, &y->res_idx)) return false;
    if (nat_cmp(x->full_arity, y->full_arity) != 0) return false;
    if (!(ignore_self_rec || positivity_same_bool(x->self_rec, y->self_rec))) return false;
  }
  return true;
}

bool positivity_same_family_except(Family a, Family b, bool ignore_level, bool ignore_self_rec) {
  return str_eq(a.name, b.name) && positivity_same_telescope(&a.params, &b.params) &&
         positivity_same_telescope(&a.indices, &b.indices) &&
         (ignore_level || level_var_structural_equal(a.level, b.level)) &&
         positivity_same_status(a.status, b.status) && positivity_same_ctors(&a.ctors, &b.ctors, ignore_self_rec) &&
         positivity_same_bool(a.positive, b.positive);
}

bool positivity_same_family(Family a, Family b) {
  return positivity_same_family_except(a, b, false, false);
}
