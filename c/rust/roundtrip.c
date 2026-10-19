/* Port of rust/roundtrip.bend: RT-MECH (unit D) of the Rust importer. Two checked programs
 * are equal up to bound names. A kernel term has de Bruijn indices, and each binder keeps its
 * source name for the printer only. So `alpha` is term_equal (c/kernel/term.c) with these names
 * ignored: the name of SPi and SZk, the names of a leg, the `self` name and the index names of a
 * motive, and the name of Let. Each other field is compared: quantities, family names,
 * constructor names, global names, literals, levels, and an ascription (Ann) with its type.
 * Definitions in Bend order. A Bend List is a Vec of pointers in the Bend list order; an index out
 * of range or a NULL pointer gives false (a Bend list has neither). The Bend Bool.and over pure
 * calls is the C && (the same value). */
#include "rust.h"

/* C: the value of a None (MaybeStr with some = false). */
static const Str roundtrip_empty = {"", 0};

/* C: the pointer at index i, NULL when i is out of range. */
static const void *roundtrip_at(const Vec *xs, size_t i) {
  VecGet got = vec_get(xs, i);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

/* alpha_list(a, b): pairwise alpha over two term lists of the same length. */
static bool roundtrip_alpha_list(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Term *x = roundtrip_at(a, i);
    const Term *y = roundtrip_at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!roundtrip_alpha(x, y)) return false;
  }
  return true;
}

/* same_count(a, b): the index names of a motive are binders: the count is compared. */
static bool roundtrip_same_count(const Vec *a, const Vec *b) {
  return a->len == b->len;
}

/* alpha_binders(a, b): the quantities of two binder lists (Binder pointers) of the same length;
 * the names are ignored. */
static bool roundtrip_alpha_binders(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Binder *x = roundtrip_at(a, i);
    const Binder *y = roundtrip_at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!quantity_equal(x->quantity, y->quantity)) return false;
  }
  return true;
}

/* alpha_shape(a, b): term_equal_shape without the SPi, SArg and SZk names. */
static bool roundtrip_alpha_shape(const Shape *a, const Shape *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case SHAPE_PI:
    return quantity_equal(a->quantity, b->quantity) && roundtrip_alpha(a->domain, b->domain);
  case SHAPE_ARG:
    return quantity_equal(a->quantity, b->quantity);
  case SHAPE_ZK:
    return quantity_equal(a->quantity, b->quantity) && roundtrip_alpha(a->domain, b->domain);
  case SHAPE_COLL:
    return big_equal(a->count, b->count);
  case SHAPE_PAR:
    return roundtrip_alpha(a->left, b->left) && roundtrip_alpha(a->right, b->right);
  case SHAPE_MU:
    return str_eq(a->name, b->name) && roundtrip_alpha_list(&a->args, &b->args);
  case SHAPE_NU:
    return str_eq(a->name, b->name) && roundtrip_alpha_list(&a->args, &b->args);
  case SHAPE_FHC:
    return roundtrip_alpha(a->level, b->level);
  case SHAPE_MPC:
    return roundtrip_alpha(a->parties, b->parties) && roundtrip_alpha(a->access, b->access);
  }
  return false;
}

/* alpha_addr(a, b): APt quantity and argument, ALeg index, ACtor name. */
static bool roundtrip_alpha_addr(const Addr *a, const Addr *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case ADDR_PT:
    return quantity_equal(a->quantity, b->quantity) && roundtrip_alpha(a->arg, b->arg);
  case ADDR_LEG:
    return big_equal(a->index, b->index);
  case ADDR_CTOR:
    return str_eq(a->name, b->name);
  }
  return false;
}

/* alpha_leg(a, b): the binder quantities and the body. */
static bool roundtrip_alpha_leg(const Leg *a, const Leg *b) {
  return roundtrip_alpha_binders(&a->binders, &b->binders) && roundtrip_alpha(a->body, b->body);
}

/* alpha_legs(a, b): pairwise alpha_leg over two leg lists (Leg pointers) of the same length. */
static bool roundtrip_alpha_legs(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Leg *x = roundtrip_at(a, i);
    const Leg *y = roundtrip_at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!roundtrip_alpha_leg(x, y)) return false;
  }
  return true;
}

/* alpha_motive(a, b): both None, or both Some with the same family name, the same index count and
 * alpha bodies; the self name and the index names are ignored. */
static bool roundtrip_alpha_motive(const MaybeMotive *a, const MaybeMotive *b) {
  if (!a->some && !b->some) return true;
  if (a->some && b->some) {
    return term_equal_option_string(a->value.ind, b->value.ind) &&
           roundtrip_same_count(&a->value.indices, &b->value.indices) &&
           roundtrip_alpha(a->value.body, b->value.body);
  }
  return false;
}

/* alpha_branches(a, b): pairwise address and leg over two branch lists (Branch pointers) of the
 * same length. */
static bool roundtrip_alpha_branches(const Vec *a, const Vec *b) {
  size_t i;
  if (a->len != b->len) return false;
  for (i = 0; i < a->len; i++) {
    const Branch *x = roundtrip_at(a, i);
    const Branch *y = roundtrip_at(b, i);
    if (x == NULL || y == NULL) return false;
    if (!roundtrip_alpha_addr(&x->addr, &y->addr)) return false;
    if (!roundtrip_alpha_leg(&x->leg, &y->leg)) return false;
  }
  return true;
}

/* alpha_elim(a, b): shape, scrutinee, scrutinee quantity, motive and branches. */
static bool roundtrip_alpha_elim(const ElimData *a, const ElimData *b) {
  if (a == NULL || b == NULL) return false;
  return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha(a->scrut, b->scrut) &&
         quantity_equal(a->scrut_q, b->scrut_q) && roundtrip_alpha_motive(&a->motive, &b->motive) &&
         roundtrip_alpha_branches(&a->branches, &b->branches);
}

/* alpha(a, b): term_equal up to bound names (the law at the head of roundtrip.bend). */
bool roundtrip_alpha(const Term *a, const Term *b) {
  if (a == NULL || b == NULL) return false;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case TERM_VAR:
    return nat_cmp(a->index, b->index) == 0;
  case TERM_UNIV:
    return level_var_structural_equal(a->level, b->level);
  case TERM_LAN:
    return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha(a->body, b->body);
  case TERM_RAN:
    return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha(a->body, b->body);
  case TERM_IN:
    return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha_addr(&a->addr, &b->addr) &&
           roundtrip_alpha_list(&a->args, &b->args);
  case TERM_ELIM:
    return roundtrip_alpha_elim(a->data, b->data);
  case TERM_SEC:
    return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha_legs(&a->legs, &b->legs);
  case TERM_OUT:
    return roundtrip_alpha_shape(&a->shape, &b->shape) && roundtrip_alpha_addr(&a->addr, &b->addr) &&
           roundtrip_alpha(a->head, b->head);
  case TERM_LET:
    return roundtrip_alpha(a->ty, b->ty) && roundtrip_alpha(a->value, b->value) &&
           roundtrip_alpha(a->body, b->body);
  case TERM_ANN:
    return roundtrip_alpha(a->body, b->body) && roundtrip_alpha(a->ty, b->ty);
  case TERM_GLOBAL:
    return str_eq(a->name, b->name);
  case TERM_LIT:
    return literal_equal(a->lit, b->lit);
  case TERM_AUTO:
    return true;
  }
  return false;
}

/* same_entry(a, b): two rows agree if they have the same class and their terms are equal up to
 * bound names. The fields are the fields that Elab.checked_form prints: the type of each class,
 * and the body of a `def`. */
static bool roundtrip_same_entry(const Entry *a, const Entry *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case ENTRY_DEF:
    return roundtrip_alpha(a->def.ty, b->def.ty) && roundtrip_alpha(a->def.body, b->def.body);
  case ENTRY_AXIOM:
    return roundtrip_alpha(a->ty, b->ty);
  case ENTRY_PRIM:
    return roundtrip_alpha(a->ty, b->ty);
  }
  return false;
}

/* C: Some(name). */
static MaybeStr roundtrip_some(Str name) {
  MaybeStr r;
  r.some = true;
  r.value = name;
  return r;
}

/* first_diff(a, b): the name of the first row that differs, in the order of the programs (Elab.Rows:
 * Vec of CheckRow pointers). The rows are compared in pairs, so the row order and the row names are
 * part of the law. None if all rows agree. The Bend right recursion is a loop: a pair that agrees
 * goes to the next pair, a pair that differs gives the name of the row of a, and the longer list
 * gives the name of its next row. A NULL row pointer ends its list (D-225a). */
MaybeStr roundtrip_first_diff(const Vec *a, const Vec *b) {
  MaybeStr none;
  size_t n = a->len > b->len ? a->len : b->len;
  size_t i;
  none.some = false;
  none.value = roundtrip_empty;
  for (i = 0; i < n; i++) {
    const CheckRow *x = roundtrip_at(a, i);
    const CheckRow *y = roundtrip_at(b, i);
    if (x == NULL && y == NULL) return none;
    if (y == NULL) return roundtrip_some(x->name);
    if (x == NULL) return roundtrip_some(y->name);
    if (!(str_eq(x->name, y->name) && roundtrip_same_entry(&x->entry, &y->entry))) return roundtrip_some(x->name);
  }
  return none;
}
