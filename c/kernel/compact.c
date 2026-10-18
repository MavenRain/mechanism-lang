/* compact: deep copies of the kernel state that survives a declaration (host only, no Bend
 * counterpart). The Bend bundle ran under a garbage collector, so the per-declaration garbage of
 * the checker was reclaimed for free; the C arena never frees, so the prelude drivers copy the
 * surviving Globals into a fresh arena after each declaration and free the old one (decision 156).
 * Every function takes the destination heap (heap->arena is the target arena) and returns the copy
 * built there; allocation failure sets heap->oom and returns a placeholder, as everywhere else. */
#include "kernel.h"

#include <stdint.h>
#include <stdlib.h>
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

/* Shape payload pointers inside a Term are const Term * (kernel.h, term section comment). The copy
 * has two halves (D-348a): the Term children and the args vector, then the name and count. The
 * consing path of compact_term copies the second half only when the shape belongs to a new node. */
static Shape compact_shape_children(Heap *heap, Shape shape) {
  Shape out = shape;
  out.domain = (void *)compact_term(heap, shape.domain);
  out.left = (void *)compact_term(heap, shape.left);
  out.right = (void *)compact_term(heap, shape.right);
  out.args = compact_term_vec(heap, &shape.args);
  out.level = (void *)compact_term(heap, shape.level);
  out.parties = (void *)compact_term(heap, shape.parties);
  out.access = (void *)compact_term(heap, shape.access);
  return out;
}

static Shape compact_shape_scalars(Heap *heap, Shape shape) {
  Shape out = shape;
  out.name = compact_str(heap, shape.name);
  out.count = compact_big(heap, shape.count);
  return out;
}

Shape compact_shape_term(Heap *heap, Shape shape) {
  return compact_shape_scalars(heap, compact_shape_children(heap, shape));
}

static Addr compact_addr_children(Heap *heap, Addr addr) {
  Addr out = addr;
  out.arg = compact_term(heap, addr.arg);
  return out;
}

static Addr compact_addr_scalars(Heap *heap, Addr addr) {
  Addr out = addr;
  out.index = compact_big(heap, addr.index);
  out.name = compact_str(heap, addr.name);
  return out;
}

Addr compact_addr(Heap *heap, Addr addr) { return compact_addr_scalars(heap, compact_addr_children(heap, addr)); }

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

/* ---- sharing memo (D-240a) ----
 * Without a memo, a subterm reached along two paths is copied twice, so every ping-pong copy of the
 * elaboration state turns more of its term DAG into a tree (hop 239: the copy after the last
 * declaration of prelude_shared_nattrans crossed the 3.5 GB cap). The memo maps each source Term to
 * its copy for one pass: open addressing with linear probing over a power-of-two table in malloc
 * memory outside the arenas. Terms are immutable after construction, so a shared copy reads the same
 * as two equal copies and the output bytes do not change. */
#define COMPACT_MEMO_FIRST 4096u

/* Keys and values are const Term * or, for the erased-term copies (D-265a), const Ktm *. Both are
 * only compared as addresses, and one source object maps to one copy of its own type. */
typedef struct {
  const void *key;
  const void *value;
} CompactMemoSlot;

/* One canonical copy of the pass with its structural hash (D-348a); term == NULL marks an empty slot. */
typedef struct {
  uint64_t hash;
  const Term *term;
} CompactConsSlot;

struct CompactMemo {
  size_t capacity; /* a power of two */
  size_t count;
  CompactMemoSlot *slots;
  size_t cons_capacity; /* a power of two */
  size_t cons_count;
  CompactConsSlot *cons_slots;
};

static size_t compact_memo_hash(const void *key) {
  uint64_t bits = (uint64_t)(uintptr_t)key >> 3;
  bits = bits ^ (bits >> 29);
  bits = bits * UINT64_C(0x9E3779B97F4A7C15);
  return (size_t)(bits ^ (bits >> 32));
}

/* The slot that holds key, or the empty slot where key goes. The table is never full (load <= 1/2),
 * and the mask keeps every probe inside slots[0 .. capacity). */
static CompactMemoSlot *compact_memo_slot(const CompactMemo *memo, const void *key) {
  size_t mask = memo->capacity - 1;
  size_t at = compact_memo_hash(key) & mask;
  for (;;) {
    CompactMemoSlot *slot = memo->slots + at;
    if (slot->key == NULL || slot->key == key) return slot;
    at = (at + 1) & mask;
  }
}

/* larger starts as a copy of *memo, so the structural table fields (cons_*) pass through unchanged
 * when *memo = larger replaces the pointer table (D-348a, hop 350). */
static bool compact_memo_grow(CompactMemo *memo) {
  CompactMemo larger = *memo;
  size_t i = 0;
  larger.capacity = memo->capacity * 2;
  larger.slots = calloc(larger.capacity, sizeof(CompactMemoSlot));
  if (larger.slots == NULL) return false;
  for (; i < memo->capacity; i = i + 1) {
    const CompactMemoSlot *old = memo->slots + i;
    if (old->key != NULL) *compact_memo_slot(&larger, old->key) = *old;
  }
  free(memo->slots);
  *memo = larger;
  return true;
}

bool compact_memo_open(Heap *heap) {
  CompactMemo *memo;
  if (heap->memo != NULL) return false;
  memo = malloc(sizeof(CompactMemo));
  if (memo == NULL) {
    heap->oom = true;
    return false;
  }
  memo->capacity = COMPACT_MEMO_FIRST;
  memo->count = 0;
  memo->slots = calloc(memo->capacity, sizeof(CompactMemoSlot));
  memo->cons_capacity = COMPACT_MEMO_FIRST;
  memo->cons_count = 0;
  memo->cons_slots = calloc(memo->cons_capacity, sizeof(CompactConsSlot));
  if (memo->slots == NULL || memo->cons_slots == NULL) {
    free(memo->slots);
    free(memo->cons_slots);
    free(memo);
    heap->oom = true;
    return false;
  }
  heap->memo = memo;
  return true;
}

void compact_memo_close(Heap *heap, bool opened) {
  if (!opened || heap->memo == NULL) return;
  free(heap->memo->slots);
  free(heap->memo->cons_slots);
  free(heap->memo);
  heap->memo = NULL;
}

static const void *compact_memo_find(const Heap *heap, const void *term) {
  if (heap->memo == NULL) return NULL;
  return compact_memo_slot(heap->memo, term)->value;
}

static void compact_memo_record(Heap *heap, const void *term, const void *copy) {
  CompactMemo *memo = heap->memo;
  CompactMemoSlot *slot;
  if (memo == NULL) return;
  if ((memo->count + 1) * 2 > memo->capacity && !compact_memo_grow(memo)) {
    heap->oom = true;
    return;
  }
  slot = compact_memo_slot(memo, term);
  if (slot->key == NULL) memo->count = memo->count + 1;
  slot->key = term;
  slot->value = copy;
}

/* ---- structural sharing (D-348a) ----
 * The pointer memo shares a copy only where the source already shared it. The state that family
 * instantiation builds is a tree (hop 347: up to 3.46M copied Terms per pass with under 4k memo
 * hits), so inside a pass each new Term is also looked up by structure: a node whose kind, scalars
 * and child copies equal those of a node already copied in the same pass returns that copy. The
 * children are canonical copies first, so child pointers compare by address, while strings,
 * naturals, bignums and levels compare by content. Terms are immutable and no code outside this file
 * compares Term addresses, so a shared copy reads the same as two equal copies and the output bytes
 * do not change. The table lives in malloc memory next to the pointer memo and dies with it. */
static uint64_t cons_mix(uint64_t hash, uint64_t value) {
  uint64_t bits = (hash ^ value) * UINT64_C(0x9E3779B97F4A7C15);
  return bits ^ (bits >> 29);
}

static uint64_t cons_ptr(uint64_t hash, const void *ptr) { return cons_mix(hash, (uint64_t)(uintptr_t)ptr); }

static uint64_t cons_bytes(uint64_t hash, const void *data, size_t len) {
  const unsigned char *bytes = data;
  uint64_t fnv = UINT64_C(0xCBF29CE484222325);
  size_t i = 0;
  for (; i < len; i = i + 1) fnv = (fnv ^ bytes[i]) * UINT64_C(0x100000001B3);
  return cons_mix(cons_mix(hash, (uint64_t)len), fnv);
}

static uint64_t cons_str(uint64_t hash, Str s) { return cons_bytes(hash, s.data, s.len); }

static uint64_t cons_nat(uint64_t hash, Nat n) { return cons_bytes(hash, n.limbs, n.len * sizeof(uint32_t)); }

static uint64_t cons_big(uint64_t hash, Big b) {
  return cons_bytes(cons_mix(hash, b.negative ? 2u : 1u), b.digits.items, b.digits.len * sizeof(uint32_t));
}

static bool cons_bytes_eq(const void *a, const void *b, size_t len) { return len == 0 || memcmp(a, b, len) == 0; }

static bool cons_str_eq(Str a, Str b) { return a.len == b.len && cons_bytes_eq(a.data, b.data, a.len); }

static bool cons_nat_eq(Nat a, Nat b) {
  return a.len == b.len && cons_bytes_eq(a.limbs, b.limbs, a.len * sizeof(uint32_t));
}

static bool cons_big_eq(Big a, Big b) {
  return a.negative == b.negative && a.digits.len == b.digits.len &&
         cons_bytes_eq(a.digits.items, b.digits.items, a.digits.len * sizeof(uint32_t));
}

static uint64_t cons_level(uint64_t hash, const LevelVar *level) {
  if (level == NULL) return cons_mix(hash, 0);
  return cons_nat(cons_level(cons_level(cons_level(cons_big(cons_mix(hash, (uint64_t)level->kind + 1), level->amount),
                                                   level->base),
                                        level->left),
                             level->right),
                  level->index);
}

static bool cons_level_eq(const LevelVar *a, const LevelVar *b) {
  if (a == b) return true;
  if (a == NULL || b == NULL) return false;
  return a->kind == b->kind && cons_big_eq(a->amount, b->amount) && cons_nat_eq(a->index, b->index) &&
         cons_level_eq(a->base, b->base) && cons_level_eq(a->left, b->left) && cons_level_eq(a->right, b->right);
}

/* Vectors of canonical Term pointers compare item by item by address. */
static uint64_t cons_ptr_vec(uint64_t hash, const Vec *vec) {
  uint64_t out = cons_mix(hash, (uint64_t)vec->len);
  size_t i = 0;
  for (; i < vec->len; i = i + 1) out = cons_ptr(out, vec->items[i]);
  return out;
}

static bool cons_ptr_vec_eq(const Vec *a, const Vec *b) {
  return a->len == b->len && cons_bytes_eq(a->items, b->items, a->len * sizeof(void *));
}

static uint64_t cons_str_vec(uint64_t hash, const Vec *vec) {
  uint64_t out = cons_mix(hash, (uint64_t)vec->len);
  size_t i = 0;
  for (; i < vec->len; i = i + 1) {
    const Str *item = vec->items[i];
    out = item == NULL ? cons_mix(out, 0) : cons_str(out, *item);
  }
  return out;
}

static bool cons_str_vec_eq(const Vec *a, const Vec *b) {
  size_t i = 0;
  if (a->len != b->len) return false;
  for (; i < a->len; i = i + 1) {
    const Str *x = a->items[i];
    const Str *y = b->items[i];
    if (x != y && (x == NULL || y == NULL || !cons_str_eq(*x, *y))) return false;
  }
  return true;
}

static uint64_t cons_binders(uint64_t hash, const Vec *vec) {
  uint64_t out = cons_mix(hash, (uint64_t)vec->len);
  size_t i = 0;
  for (; i < vec->len; i = i + 1) {
    const Binder *item = vec->items[i];
    out = item == NULL ? cons_mix(out, 0) : cons_str(cons_mix(out, (uint64_t)item->quantity + 1), item->name);
  }
  return out;
}

static bool cons_binders_eq(const Vec *a, const Vec *b) {
  size_t i = 0;
  if (a->len != b->len) return false;
  for (; i < a->len; i = i + 1) {
    const Binder *x = a->items[i];
    const Binder *y = b->items[i];
    if (x != y && (x == NULL || y == NULL || x->quantity != y->quantity || !cons_str_eq(x->name, y->name))) return false;
  }
  return true;
}

static uint64_t cons_leg(uint64_t hash, const Leg *leg) { return cons_ptr(cons_binders(hash, &leg->binders), leg->body); }

static bool cons_leg_eq(const Leg *a, const Leg *b) {
  return a->body == b->body && cons_binders_eq(&a->binders, &b->binders);
}

static uint64_t cons_legs(uint64_t hash, const Vec *vec) {
  uint64_t out = cons_mix(hash, (uint64_t)vec->len);
  size_t i = 0;
  for (; i < vec->len; i = i + 1) {
    const Leg *item = vec->items[i];
    out = item == NULL ? cons_mix(out, 0) : cons_leg(out, item);
  }
  return out;
}

static bool cons_legs_eq(const Vec *a, const Vec *b) {
  size_t i = 0;
  if (a->len != b->len) return false;
  for (; i < a->len; i = i + 1) {
    const Leg *x = a->items[i];
    const Leg *y = b->items[i];
    if (x != y && (x == NULL || y == NULL || !cons_leg_eq(x, y))) return false;
  }
  return true;
}

/* Every Shape field is compared, as compact_shape_term copies every field whatever the kind. */
static uint64_t cons_shape(uint64_t hash, const Shape *s) {
  uint64_t out = cons_str(cons_mix(cons_mix(hash, (uint64_t)s->kind + 1), (uint64_t)s->quantity + 1), s->name);
  out = cons_big(cons_ptr(out, s->domain), s->count);
  out = cons_ptr_vec(cons_ptr(cons_ptr(out, s->left), s->right), &s->args);
  return cons_ptr(cons_ptr(cons_ptr(out, s->level), s->parties), s->access);
}

static bool cons_shape_eq(const Shape *a, const Shape *b) {
  return a->kind == b->kind && a->quantity == b->quantity && a->domain == b->domain && a->left == b->left &&
         a->right == b->right && a->level == b->level && a->parties == b->parties && a->access == b->access &&
         cons_str_eq(a->name, b->name) && cons_big_eq(a->count, b->count) && cons_ptr_vec_eq(&a->args, &b->args);
}

static uint64_t cons_addr(uint64_t hash, const Addr *a) {
  uint64_t out = cons_ptr(cons_mix(cons_mix(hash, (uint64_t)a->kind + 1), (uint64_t)a->quantity + 1), a->arg);
  return cons_str(cons_big(out, a->index), a->name);
}

static bool cons_addr_eq(const Addr *a, const Addr *b) {
  return a->kind == b->kind && a->quantity == b->quantity && a->arg == b->arg && cons_big_eq(a->index, b->index) &&
         cons_str_eq(a->name, b->name);
}

static uint64_t cons_motive(uint64_t hash, const Motive *m) {
  uint64_t out = cons_mix(hash, m->ind.some ? 2u : 1u);
  out = m->ind.some ? cons_str(out, m->ind.value) : out;
  return cons_ptr(cons_str(cons_str_vec(out, &m->indices), m->self), m->body);
}

static bool cons_motive_eq(const Motive *a, const Motive *b) {
  return a->ind.some == b->ind.some && (!a->ind.some || cons_str_eq(a->ind.value, b->ind.value)) &&
         cons_str_vec_eq(&a->indices, &b->indices) && cons_str_eq(a->self, b->self) && a->body == b->body;
}

static uint64_t cons_branches(uint64_t hash, const Vec *vec) {
  uint64_t out = cons_mix(hash, (uint64_t)vec->len);
  size_t i = 0;
  for (; i < vec->len; i = i + 1) {
    const Branch *item = vec->items[i];
    out = item == NULL ? cons_mix(out, 0) : cons_leg(cons_addr(out, &item->addr), &item->leg);
  }
  return out;
}

static bool cons_branches_eq(const Vec *a, const Vec *b) {
  size_t i = 0;
  if (a->len != b->len) return false;
  for (; i < a->len; i = i + 1) {
    const Branch *x = a->items[i];
    const Branch *y = b->items[i];
    if (x != y && (x == NULL || y == NULL || !cons_addr_eq(&x->addr, &y->addr) || !cons_leg_eq(&x->leg, &y->leg)))
      return false;
  }
  return true;
}

static uint64_t cons_elim(uint64_t hash, const ElimData *d) {
  uint64_t out;
  if (d == NULL) return cons_mix(hash, 0);
  out = cons_mix(cons_ptr(cons_shape(hash, &d->shape), d->scrut), (uint64_t)d->scrut_q + 1);
  out = cons_mix(out, d->motive.some ? 2u : 1u);
  out = d->motive.some ? cons_motive(out, &d->motive.value) : out;
  return cons_branches(out, &d->branches);
}

static bool cons_elim_eq(const ElimData *a, const ElimData *b) {
  if (a == b) return true;
  if (a == NULL || b == NULL) return false;
  return a->scrut == b->scrut && a->scrut_q == b->scrut_q && a->motive.some == b->motive.some &&
         cons_shape_eq(&a->shape, &b->shape) && (!a->motive.some || cons_motive_eq(&a->motive.value, &b->motive.value)) &&
         cons_branches_eq(&a->branches, &b->branches);
}

static uint64_t cons_literal(uint64_t hash, const Literal *lit) {
  return cons_big(cons_str(cons_mix(hash, (uint64_t)lit->kind + 1), lit->string), lit->integer);
}

static bool cons_literal_eq(const Literal *a, const Literal *b) {
  return a->kind == b->kind && cons_str_eq(a->string, b->string) && cons_big_eq(a->integer, b->integer);
}

/* The fields compared per kind are the fields compact_term copies for that kind. */
static uint64_t cons_term(const Term *t) {
  uint64_t hash = cons_mix(UINT64_C(0x6A09E667F3BCC908), (uint64_t)t->kind + 1);
  switch (t->kind) {
  case TERM_VAR:
    return cons_nat(hash, t->index);
  case TERM_UNIV:
    return cons_level(hash, t->level);
  case TERM_LAN:
  case TERM_RAN:
    return cons_ptr(cons_shape(hash, &t->shape), t->body);
  case TERM_IN:
    return cons_ptr_vec(cons_addr(cons_shape(hash, &t->shape), &t->addr), &t->args);
  case TERM_ELIM:
    return cons_elim(hash, t->data);
  case TERM_SEC:
    return cons_legs(cons_shape(hash, &t->shape), &t->legs);
  case TERM_OUT:
    return cons_ptr(cons_addr(cons_shape(hash, &t->shape), &t->addr), t->head);
  case TERM_LET:
    return cons_ptr(cons_ptr(cons_ptr(cons_str(hash, t->name), t->ty), t->value), t->body);
  case TERM_ANN:
    return cons_ptr(cons_ptr(hash, t->body), t->ty);
  case TERM_GLOBAL:
    return cons_str(hash, t->name);
  case TERM_LIT:
    return cons_literal(hash, &t->lit);
  case TERM_AUTO:
    return hash;
  }
  return hash;
}

static bool cons_term_eq(const Term *a, const Term *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
  case TERM_VAR:
    return cons_nat_eq(a->index, b->index);
  case TERM_UNIV:
    return cons_level_eq(a->level, b->level);
  case TERM_LAN:
  case TERM_RAN:
    return a->body == b->body && cons_shape_eq(&a->shape, &b->shape);
  case TERM_IN:
    return cons_ptr_vec_eq(&a->args, &b->args) && cons_addr_eq(&a->addr, &b->addr) && cons_shape_eq(&a->shape, &b->shape);
  case TERM_ELIM:
    return cons_elim_eq(a->data, b->data);
  case TERM_SEC:
    return cons_legs_eq(&a->legs, &b->legs) && cons_shape_eq(&a->shape, &b->shape);
  case TERM_OUT:
    return a->head == b->head && cons_addr_eq(&a->addr, &b->addr) && cons_shape_eq(&a->shape, &b->shape);
  case TERM_LET:
    return a->ty == b->ty && a->value == b->value && a->body == b->body && cons_str_eq(a->name, b->name);
  case TERM_ANN:
    return a->body == b->body && a->ty == b->ty;
  case TERM_GLOBAL:
    return cons_str_eq(a->name, b->name);
  case TERM_LIT:
    return cons_literal_eq(&a->lit, &b->lit);
  case TERM_AUTO:
    return true;
  }
  return false;
}

static size_t cons_home(uint64_t hash, size_t capacity) { return (size_t)(hash ^ (hash >> 32)) & (capacity - 1); }

/* The slot that holds a Term equal to key, or the empty slot where key goes (load <= 1/2, so the
 * probe ends; the mask keeps it inside cons_slots[0 .. cons_capacity)). */
static CompactConsSlot *compact_cons_slot(const CompactMemo *memo, const Term *key, uint64_t hash) {
  size_t mask = memo->cons_capacity - 1;
  size_t at = cons_home(hash, memo->cons_capacity);
  for (;;) {
    CompactConsSlot *slot = memo->cons_slots + at;
    if (slot->term == NULL || (slot->hash == hash && cons_term_eq(slot->term, key))) return slot;
    at = (at + 1) & mask;
  }
}

static bool compact_cons_grow(CompactMemo *memo) {
  size_t capacity = memo->cons_capacity * 2;
  CompactConsSlot *slots = calloc(capacity, sizeof(CompactConsSlot));
  size_t i = 0;
  if (slots == NULL) return false;
  for (; i < memo->cons_capacity; i = i + 1) {
    const CompactConsSlot *old = memo->cons_slots + i;
    size_t at = cons_home(old->hash, capacity);
    if (old->term == NULL) continue;
    while (slots[at].term != NULL) at = (at + 1) & (capacity - 1);
    slots[at] = *old;
  }
  free(memo->cons_slots);
  memo->cons_slots = slots;
  memo->cons_capacity = capacity;
  return true;
}

/* NULL after an allocation failure (heap->oom set); the node is then copied without sharing. */
static CompactConsSlot *compact_cons_find(Heap *heap, const Term *key, uint64_t hash) {
  CompactMemo *memo = heap->memo;
  if (heap->oom) return NULL;
  if ((memo->cons_count + 1) * 2 > memo->cons_capacity && !compact_cons_grow(memo)) {
    heap->oom = true;
    return NULL;
  }
  return compact_cons_slot(memo, key, hash);
}

/* The scalar half of a node copy (names, indices, levels, literals, shape name and count): run only
 * for a node that the structural lookup did not find. */
static Term compact_term_scalars(Heap *heap, Term key) {
  Term out = key;
  switch (key.kind) {
  case TERM_VAR:
    out.index = compact_nat(heap, key.index);
    break;
  case TERM_UNIV:
    out.level = compact_level(heap, key.level);
    break;
  case TERM_LAN:
  case TERM_RAN:
  case TERM_SEC:
    out.shape = compact_shape_scalars(heap, key.shape);
    break;
  case TERM_IN:
  case TERM_OUT:
    out.shape = compact_shape_scalars(heap, key.shape);
    out.addr = compact_addr_scalars(heap, key.addr);
    break;
  case TERM_LET:
  case TERM_GLOBAL:
    out.name = compact_str(heap, key.name);
    break;
  case TERM_LIT:
    out.lit = compact_literal(heap, key.lit);
    break;
  case TERM_ELIM:
  case TERM_ANN:
  case TERM_AUTO:
    break;
  }
  return out;
}

/* The copy of term inside a pass: the pointer memo, then the children, then the structural lookup.
 * The key holds the canonical children and the source scalars (equal by content to their copies). */
static const Term *compact_term_shared(Heap *heap, const Term *term) {
  const Term *seen = compact_memo_find(heap, term);
  CompactConsSlot *slot;
  uint64_t hash;
  Term key;
  Term *out;
  if (seen != NULL) return seen;
  memset(&key, 0, sizeof key);
  key.kind = term->kind;
  switch (term->kind) {
  case TERM_VAR:
    key.index = term->index;
    break;
  case TERM_UNIV:
    key.level = term->level;
    break;
  case TERM_LAN:
  case TERM_RAN:
    key.shape = compact_shape_children(heap, term->shape);
    key.body = compact_term(heap, term->body);
    break;
  case TERM_IN:
    key.shape = compact_shape_children(heap, term->shape);
    key.addr = compact_addr_children(heap, term->addr);
    key.args = compact_term_vec(heap, &term->args);
    break;
  case TERM_ELIM:
    key.data = compact_elim(heap, term->data);
    break;
  case TERM_SEC:
    key.shape = compact_shape_children(heap, term->shape);
    key.legs = compact_vec_with(heap, &term->legs, compact_leg_item);
    break;
  case TERM_OUT:
    key.shape = compact_shape_children(heap, term->shape);
    key.addr = compact_addr_children(heap, term->addr);
    key.head = compact_term(heap, term->head);
    break;
  case TERM_LET:
    key.name = term->name;
    key.ty = compact_term(heap, term->ty);
    key.value = compact_term(heap, term->value);
    key.body = compact_term(heap, term->body);
    break;
  case TERM_ANN:
    key.body = compact_term(heap, term->body);
    key.ty = compact_term(heap, term->ty);
    break;
  case TERM_GLOBAL:
    key.name = term->name;
    break;
  case TERM_LIT:
    key.lit = term->lit;
    break;
  case TERM_AUTO:
    break;
  }
  hash = cons_term(&key);
  slot = compact_cons_find(heap, &key, hash);
  if (slot != NULL && slot->term != NULL) {
    compact_memo_record(heap, term, slot->term);
    return slot->term;
  }
  out = heap_alloc(heap, sizeof(Term));
  if (out == NULL) return NULL;
  *out = compact_term_scalars(heap, key);
  if (slot != NULL) {
    slot->hash = hash;
    slot->term = out;
    heap->memo->cons_count = heap->memo->cons_count + 1;
  }
  compact_memo_record(heap, term, out);
  return out;
}

/* Terms are built by term_new (zeroed) plus the fields of one constructor; the payloads overlay
 * in unions (kernel.h), so the copy reads only the fields of the term's own kind. Inside a pass
 * (heap->memo != NULL) compact_term_shared copies; outside one each call makes a fresh copy. */
const Term *compact_term(Heap *heap, const Term *term) {
  Term *out;
  if (term == NULL) return NULL;
  if (heap->memo != NULL) return compact_term_shared(heap, term);
  out = heap_alloc(heap, sizeof(Term));
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

/* ---- erased terms (D-265a) ----
 * c/tests/runtime_reachable.c erases each reachable entry on its own step arena and copies the
 * EraseEntry into the caller heap before it frees that arena. Ktm values are immutable after
 * construction, so inside a pass with the memo open a shared subterm keeps one copy, and the static
 * KErased node of eterm_kerased stays the same node. Every field is copied whatever the kind (the
 * fields a kind does not use are zero, and a zero field copies to zero). */
static Tid compact_tid(Heap *heap, Tid tid) { return eterm_tid(compact_str(heap, tid.name)); }

static Fid compact_fid(Heap *heap, Fid fid) { return eterm_fid(compact_str(heap, fid.name)); }

static Repr compact_repr(Heap *heap, Repr repr) {
  Repr out = repr;
  out.tid = compact_tid(heap, repr.tid);
  return out;
}

static void *compact_tid_item(Heap *heap, const void *item) {
  Tid *out = heap_alloc(heap, sizeof(Tid));
  if (out == NULL) return NULL;
  *out = compact_tid(heap, *(const Tid *)item);
  return out;
}

static void *compact_repr_item(Heap *heap, const void *item) {
  Repr *out = heap_alloc(heap, sizeof(Repr));
  if (out == NULL) return NULL;
  *out = compact_repr(heap, *(const Repr *)item);
  return out;
}

static void *compact_ktm_item(Heap *heap, const void *item) { return (void *)compact_ktm(heap, item); }

static void *compact_ktm_branch_item(Heap *heap, const void *item) {
  const KtmBranch *branch = item;
  KtmBranch *out = heap_alloc(heap, sizeof(KtmBranch));
  if (out == NULL) return NULL;
  out->tag = compact_nat(heap, branch->tag);
  out->arity = compact_nat(heap, branch->arity);
  out->body = compact_ktm(heap, branch->body);
  return out;
}

const Ktm *compact_ktm(Heap *heap, const Ktm *term) {
  const Ktm *seen;
  Ktm *out;
  if (term == NULL || term == eterm_kerased()) return term;
  seen = compact_memo_find(heap, term);
  if (seen != NULL) return seen;
  out = heap_alloc(heap, sizeof(Ktm));
  if (out == NULL) return eterm_kerased();
  out->kind = term->kind;
  out->index = compact_nat(heap, term->index);
  out->lit = compact_literal(heap, term->lit);
  out->name = compact_str(heap, term->name);
  out->value = compact_ktm(heap, term->value);
  out->body = compact_ktm(heap, term->body);
  out->fid = compact_fid(heap, term->fid);
  out->arity = compact_nat(heap, term->arity);
  out->captures = compact_vec_with(heap, &term->captures, compact_ktm_item);
  out->head = compact_ktm(heap, term->head);
  out->args = compact_vec_with(heap, &term->args, compact_ktm_item);
  out->tid = compact_tid(heap, term->tid);
  out->fields = compact_vec_with(heap, &term->fields, compact_ktm_item);
  out->tag = compact_nat(heap, term->tag);
  out->scrut = compact_ktm(heap, term->scrut);
  out->branches = compact_vec_with(heap, &term->branches, compact_ktm_branch_item);
  compact_memo_record(heap, term, out);
  return out;
}

Kdecl compact_kdecl(Heap *heap, Kdecl decl) {
  Kdecl out;
  out.kind = decl.kind;
  out.fid = compact_fid(heap, decl.fid);
  out.params = compact_vec_with(heap, &decl.params, compact_repr_item);
  out.result = compact_repr(heap, decl.result);
  out.body = compact_ktm(heap, decl.body);
  out.tids = compact_vec_with(heap, &decl.tids, compact_tid_item);
  return out;
}

static void *compact_kdecl_item(Heap *heap, const void *item) {
  Kdecl *out = heap_alloc(heap, sizeof(Kdecl));
  if (out == NULL) return NULL;
  *out = compact_kdecl(heap, *(const Kdecl *)item);
  return out;
}

EraseEntry compact_erase_entry(Heap *heap, EraseEntry entry) {
  EraseEntry out;
  out.kind = entry.kind;
  out.repr = compact_repr(heap, entry.repr);
  out.decls = compact_vec_with(heap, &entry.decls, compact_kdecl_item);
  return out;
}
