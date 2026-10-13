/* compact: deep copies of the kernel state that survives a declaration (host only, no Bend
 * counterpart). The Bend bundle ran under a garbage collector, so the per-declaration garbage of
 * the checker was reclaimed for free; the C arena never frees, so the prelude drivers copy the
 * surviving Globals into a fresh arena after each declaration and free the old one (decision 156).
 * Every function takes the destination heap (heap->arena is the target arena) and returns the copy
 * built there; allocation failure sets heap->oom and returns a placeholder, as everywhere else. */
#include "kernel.h"

#include <string.h>

Nat compact_nat(Heap *heap, Nat n) {
  if (n.len == 0) return n;
  uint32_t *limbs = heap_alloc(heap, n.len * sizeof(uint32_t));
  if (limbs == NULL) return nat_zero();
  memcpy(limbs, n.limbs, n.len * sizeof(uint32_t));
  Nat out = {limbs, n.len};
  return out;
}

MaybeNat compact_maybe_nat(Heap *heap, MaybeNat n) {
  MaybeNat out = {n.some, n.some ? compact_nat(heap, n.value) : nat_zero()};
  return out;
}

Big compact_big(Heap *heap, Big b) {
  if (b.digits.len == 0) return b;
  uint32_t *items = heap_alloc(heap, b.digits.len * sizeof(uint32_t));
  if (items == NULL) {
    Big zero = {false, {NULL, 0}};
    return zero;
  }
  memcpy(items, b.digits.items, b.digits.len * sizeof(uint32_t));
  Big out = {b.negative, {items, b.digits.len}};
  return out;
}

Str compact_str(Heap *heap, Str s) {
  if (s.len == 0) return str_empty();
  Str out = str_copy(heap->arena, s);
  if (out.data == NULL) heap->oom = true;
  return out;
}

MaybeStr compact_maybe_str(Heap *heap, MaybeStr s) {
  MaybeStr out = {s.some, s.some ? compact_str(heap, s.value) : str_empty()};
  return out;
}

Vec compact_vec_with(Heap *heap, const Vec *vec, CompactItem f) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < vec->len; i++) {
    void *item = vec->items[i];
    void *copy = item == NULL ? NULL : f(heap, item);
    if (!vec_push(&out, copy)) heap->oom = true;
  }
  return out;
}

static void *compact_str_item(Heap *heap, const void *item) {
  Str *out = heap_alloc(heap, sizeof(Str));
  if (out == NULL) return NULL;
  *out = compact_str(heap, *(const Str *)item);
  return out;
}

Vec compact_str_vec(Heap *heap, const Vec *vec) { return compact_vec_with(heap, vec, compact_str_item); }

const LevelVar *compact_level(Heap *heap, const LevelVar *level) {
  if (level == NULL) return NULL;
  LevelVar *out = heap_alloc(heap, sizeof(LevelVar));
  if (out == NULL) return NULL;
  out->kind = level->kind;
  out->amount = compact_big(heap, level->amount);
  out->base = compact_level(heap, level->base);
  out->left = compact_level(heap, level->left);
  out->right = compact_level(heap, level->right);
  out->index = compact_nat(heap, level->index);
  return out;
}

static void *compact_term_item(Heap *heap, const void *item) { return (void *)compact_term(heap, item); }

Vec compact_term_vec(Heap *heap, const Vec *vec) { return compact_vec_with(heap, vec, compact_term_item); }

/* Shape payload pointers inside a Term are const Term * (kernel.h, term section comment). */
Shape compact_shape_term(Heap *heap, Shape shape) {
  Shape out = shape;
  out.name = compact_str(heap, shape.name);
  out.domain = (void *)compact_term(heap, shape.domain);
  out.count = compact_big(heap, shape.count);
  out.left = (void *)compact_term(heap, shape.left);
  out.right = (void *)compact_term(heap, shape.right);
  out.args = compact_term_vec(heap, &shape.args);
  out.level = (void *)compact_term(heap, shape.level);
  out.parties = (void *)compact_term(heap, shape.parties);
  out.access = (void *)compact_term(heap, shape.access);
  return out;
}

Addr compact_addr(Heap *heap, Addr addr) {
  Addr out = addr;
  out.arg = compact_term(heap, addr.arg);
  out.index = compact_big(heap, addr.index);
  out.name = compact_str(heap, addr.name);
  return out;
}

static void *compact_binder_item(Heap *heap, const void *item) {
  const Binder *binder = item;
  Binder *out = heap_alloc(heap, sizeof(Binder));
  if (out == NULL) return NULL;
  out->quantity = binder->quantity;
  out->name = compact_str(heap, binder->name);
  return out;
}

Leg compact_leg(Heap *heap, Leg leg) {
  Leg out;
  out.binders = compact_vec_with(heap, &leg.binders, compact_binder_item);
  out.body = compact_term(heap, leg.body);
  return out;
}

static void *compact_leg_item(Heap *heap, const void *item) {
  Leg *out = heap_alloc(heap, sizeof(Leg));
  if (out == NULL) return NULL;
  *out = compact_leg(heap, *(const Leg *)item);
  return out;
}

Motive compact_motive(Heap *heap, Motive motive) {
  Motive out;
  out.ind = compact_maybe_str(heap, motive.ind);
  out.indices = compact_str_vec(heap, &motive.indices);
  out.self = compact_str(heap, motive.self);
  out.body = compact_term(heap, motive.body);
  return out;
}

static void *compact_branch_item(Heap *heap, const void *item) {
  const Branch *branch = item;
  Branch *out = heap_alloc(heap, sizeof(Branch));
  if (out == NULL) return NULL;
  out->addr = compact_addr(heap, branch->addr);
  out->leg = compact_leg(heap, branch->leg);
  return out;
}

const ElimData *compact_elim(Heap *heap, const ElimData *data) {
  if (data == NULL) return NULL;
  ElimData *out = heap_alloc(heap, sizeof(ElimData));
  if (out == NULL) return NULL;
  out->shape = compact_shape_term(heap, data->shape);
  out->scrut = compact_term(heap, data->scrut);
  out->scrut_q = data->scrut_q;
  out->motive.some = data->motive.some;
  if (data->motive.some) out->motive.value = compact_motive(heap, data->motive.value);
  out->branches = compact_vec_with(heap, &data->branches, compact_branch_item);
  return out;
}

Literal compact_literal(Heap *heap, Literal lit) {
  Literal out = lit;
  out.string = compact_str(heap, lit.string);
  out.integer = compact_big(heap, lit.integer);
  return out;
}

/* Terms are built by term_new (zeroed) plus the fields of one constructor; the payloads overlay
 * in unions (kernel.h), so the copy reads only the fields of the term's own kind. */
const Term *compact_term(Heap *heap, const Term *term) {
  if (term == NULL) return NULL;
  Term *out = heap_alloc(heap, sizeof(Term));
  if (out == NULL) return NULL;
  out->kind = term->kind;
  switch (term->kind) {
  case TERM_VAR:
    out->index = compact_nat(heap, term->index);
    break;
  case TERM_UNIV:
    out->level = compact_level(heap, term->level);
    break;
  case TERM_LAN:
  case TERM_RAN:
    out->shape = compact_shape_term(heap, term->shape);
    out->body = compact_term(heap, term->body);
    break;
  case TERM_IN:
    out->shape = compact_shape_term(heap, term->shape);
    out->addr = compact_addr(heap, term->addr);
    out->args = compact_term_vec(heap, &term->args);
    break;
  case TERM_ELIM:
    out->data = compact_elim(heap, term->data);
    break;
  case TERM_SEC:
    out->shape = compact_shape_term(heap, term->shape);
    out->legs = compact_vec_with(heap, &term->legs, compact_leg_item);
    break;
  case TERM_OUT:
    out->shape = compact_shape_term(heap, term->shape);
    out->addr = compact_addr(heap, term->addr);
    out->head = compact_term(heap, term->head);
    break;
  case TERM_LET:
    out->name = compact_str(heap, term->name);
    out->ty = compact_term(heap, term->ty);
    out->value = compact_term(heap, term->value);
    out->body = compact_term(heap, term->body);
    break;
  case TERM_ANN:
    out->body = compact_term(heap, term->body);
    out->ty = compact_term(heap, term->ty);
    break;
  case TERM_GLOBAL:
    out->name = compact_str(heap, term->name);
    break;
  case TERM_LIT:
    out->lit = compact_literal(heap, term->lit);
    break;
  case TERM_AUTO:
    break;
  }
  return out;
}

MaybeTermPtr compact_maybe_term(Heap *heap, MaybeTermPtr term) {
  MaybeTermPtr out = {term.some, term.some ? compact_term(heap, term.value) : NULL};
  return out;
}

static void *compact_tele_item(Heap *heap, const void *item) {
  const TeleBinder *binder = item;
  TeleBinder *out = heap_alloc(heap, sizeof(TeleBinder));
  if (out == NULL) return NULL;
  out->quantity = binder->quantity;
  out->name = compact_str(heap, binder->name);
  out->ty = compact_term(heap, binder->ty);
  return out;
}

Vec compact_tele(Heap *heap, const Vec *binders) { return compact_vec_with(heap, binders, compact_tele_item); }

static void *compact_ctor_item(Heap *heap, const void *item) {
  const Ctor *ctor = item;
  Ctor *out = heap_alloc(heap, sizeof(Ctor));
  if (out == NULL) return NULL;
  out->name = compact_str(heap, ctor->name);
  out->args = compact_tele(heap, &ctor->args);
  out->res_idx = compact_term_vec(heap, &ctor->res_idx);
  out->full_arity = compact_nat(heap, ctor->full_arity);
  out->self_rec = ctor->self_rec;
  return out;
}

Vec compact_ctors(Heap *heap, const Vec *ctors) { return compact_vec_with(heap, ctors, compact_ctor_item); }

CtorStatus compact_ctor_status(Heap *heap, CtorStatus status) {
  CtorStatus out;
  out.kind = status.kind;
  out.names = compact_str_vec(heap, &status.names);
  return out;
}

Family compact_family(Heap *heap, Family family) {
  Family out;
  out.name = compact_str(heap, family.name);
  out.params = compact_tele(heap, &family.params);
  out.indices = compact_tele(heap, &family.indices);
  out.level = compact_level(heap, family.level);
  out.status = compact_ctor_status(heap, family.status);
  out.ctors = compact_ctors(heap, &family.ctors);
  out.positive = family.positive;
  return out;
}

Entry compact_entry(Heap *heap, Entry entry) {
  Entry out = entry;
  out.def.ty = compact_term(heap, entry.def.ty);
  out.def.body = compact_term(heap, entry.def.body);
  out.def.rec_arg = compact_maybe_nat(heap, entry.def.rec_arg);
  out.ty = compact_term(heap, entry.ty);
  return out;
}

Decl compact_decl(Heap *heap, Decl decl) {
  Decl out = decl;
  out.name = compact_str(heap, decl.name);
  out.ty = compact_term(heap, decl.ty);
  out.body = compact_maybe_term(heap, decl.body);
  return out;
}

static void *compact_check_row_item(Heap *heap, const void *item) {
  const CheckRow *row = item;
  CheckRow *out = heap_alloc(heap, sizeof(CheckRow));
  if (out == NULL) return NULL;
  out->name = compact_str(heap, row->name);
  out->entry = compact_entry(heap, row->entry);
  return out;
}

Vec compact_check_rows(Heap *heap, const Vec *rows) { return compact_vec_with(heap, rows, compact_check_row_item); }

Error compact_error(Heap *heap, Error error) {
  Error out = error;
  out.message = compact_str(heap, error.message);
  out.line = compact_nat(heap, error.line);
  out.column = compact_nat(heap, error.column);
  return out;
}

/* Cons lists are copied front to back with a tail pointer: the list order is the lookup order. */
const EntryRows *compact_entry_rows(Heap *heap, const EntryRows *rows) {
  EntryRows *head = NULL;
  EntryRows *tail = NULL;
  const EntryRows *cursor = rows;
  for (; cursor != NULL; cursor = cursor->rest) {
    EntryRows *node = heap_alloc(heap, sizeof(EntryRows));
    if (node == NULL) return head;
    node->name = compact_str(heap, cursor->name);
    node->entry = compact_entry(heap, cursor->entry);
    node->rest = NULL;
    if (tail == NULL) head = node; else tail->rest = node;
    tail = node;
  }
  return head;
}

const FamilyRows *compact_family_rows(Heap *heap, const FamilyRows *rows) {
  FamilyRows *head = NULL;
  FamilyRows *tail = NULL;
  const FamilyRows *cursor = rows;
  for (; cursor != NULL; cursor = cursor->rest) {
    FamilyRows *node = heap_alloc(heap, sizeof(FamilyRows));
    if (node == NULL) return head;
    node->name = compact_str(heap, cursor->name);
    node->family = compact_family(heap, cursor->family);
    node->rest = NULL;
    if (tail == NULL) head = node; else tail->rest = node;
    tail = node;
  }
  return head;
}

/* The index leaves point at the Entry / Family inside the row cell of the same name (global_add,
 * global_index_*_rows), so the copied leaves point into the copied rows; a leaf whose name has no
 * row (never seen, kept for safety) gets its own deep copy. */
typedef struct {
  Heap *heap;
  const EntryRows *entries;
  const FamilyRows *families;
} CompactRemap;

static const void *compact_remap_entry(void *ctx, Str key, const void *old) {
  CompactRemap *remap = ctx;
  for (const EntryRows *row = remap->entries; row != NULL; row = row->rest) {
    if (str_eq(row->name, key)) return &row->entry;
  }
  if (old == NULL) return NULL;
  Entry *copy = heap_alloc(remap->heap, sizeof(Entry));
  if (copy == NULL) return NULL;
  *copy = compact_entry(remap->heap, *(const Entry *)old);
  return copy;
}

static const void *compact_remap_family(void *ctx, Str key, const void *old) {
  CompactRemap *remap = ctx;
  for (const FamilyRows *row = remap->families; row != NULL; row = row->rest) {
    if (str_eq(row->name, key)) return &row->family;
  }
  if (old == NULL) return NULL;
  Family *copy = heap_alloc(remap->heap, sizeof(Family));
  if (copy == NULL) return NULL;
  *copy = compact_family(remap->heap, *(const Family *)old);
  return copy;
}

Globals compact_globals(Heap *heap, Globals globals) {
  Globals out;
  out.entries = compact_entry_rows(heap, globals.entries);
  out.families = compact_family_rows(heap, globals.families);
  CompactRemap remap = {heap, out.entries, out.families};
  out.entry_index = index_copy(heap, globals.entry_index, compact_remap_entry, &remap);
  out.family_index = index_copy(heap, globals.family_index, compact_remap_family, &remap);
  return out;
}
