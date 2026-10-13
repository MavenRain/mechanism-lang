/* C port of bend2/kernel/global.bend: the global environment of definitions, axioms, primitives and
 * inductive families. Entries and families are first-match lists with one name index each, and every
 * Globals value is persistent (it stays valid after add, add_family and without_family), as in Bend. */
#include "kernel.h"

#include <string.h>

/* ---- Map<String, Maybe<A>>: a persistent hash trie. INDEX_DEPTH levels of INDEX_WIDTH slot nodes
 * take INDEX_BITS hash bits each, then a bucket list of leaves; set and del copy one path. ---- */
#define INDEX_BITS 5u
#define INDEX_WIDTH 32u
#define INDEX_DEPTH 6u

typedef struct IndexLeaf IndexLeaf;
struct IndexLeaf {
  Str key;
  const void *value;
  const IndexLeaf *rest;
};
struct IndexNode {
  const void *slots[INDEX_WIDTH]; /* IndexNode children above the bottom level, IndexLeaf lists at it */
};
/* Map.get: found when the key is present, value its Some payload. */
typedef struct {
  bool found;
  const void *value;
} IndexGet;

static const Entry entry_zero;
static const DefEntry def_entry_zero;
static const Family family_zero;

static uint64_t index_hash(Str key) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < key.len; i++) {
    h ^= (unsigned char)key.data[i];
    h *= 1099511628211ull;
  }
  return h;
}

static unsigned index_slot(uint64_t hash, unsigned level) {
  return (unsigned)((hash >> (INDEX_BITS * level)) & (INDEX_WIDTH - 1u));
}

static const IndexLeaf *leaf_find(const IndexLeaf *leaf, Str key) {
  const IndexLeaf *cell = leaf;
  while (cell != NULL && !str_eq(cell->key, key)) cell = cell->rest;
  return cell;
}

/* The bucket without key: the cells before the match are copied, the tail after it is shared. */
static const IndexLeaf *leaf_without(Heap *heap, const IndexLeaf *leaf, Str key) {
  if (leaf == NULL) return NULL;
  if (str_eq(leaf->key, key)) return leaf->rest;
  const IndexLeaf *rest = leaf_without(heap, leaf->rest, key);
  if (rest == leaf->rest) return leaf;
  IndexLeaf *copy = heap_alloc(heap, sizeof(IndexLeaf));
  if (copy == NULL) return leaf;
  copy->key = leaf->key;
  copy->value = leaf->value;
  copy->rest = rest;
  return copy;
}

static const IndexLeaf *leaf_set(Heap *heap, const IndexLeaf *leaf, Str key, const void *value) {
  IndexLeaf *cell = heap_alloc(heap, sizeof(IndexLeaf));
  if (cell == NULL) return leaf;
  cell->key = key;
  cell->value = value;
  cell->rest = leaf_without(heap, leaf, key);
  return cell;
}

static const void *node_set(Heap *heap, const void *node, Str key, uint64_t hash, const void *value, unsigned level) {
  if (level == INDEX_DEPTH) return leaf_set(heap, node, key, value);
  IndexNode *copy = heap_alloc(heap, sizeof(IndexNode));
  if (copy == NULL) return node;
  if (node != NULL) memcpy(copy, node, sizeof(IndexNode));
  unsigned slot = index_slot(hash, level);
  copy->slots[slot] = node_set(heap, copy->slots[slot], key, hash, value, level + 1u);
  return copy;
}

static const void *node_del(Heap *heap, const void *node, Str key, uint64_t hash, unsigned level) {
  if (node == NULL) return NULL;
  if (level == INDEX_DEPTH) return leaf_without(heap, node, key);
  const IndexNode *old = node;
  unsigned slot = index_slot(hash, level);
  const void *child = node_del(heap, old->slots[slot], key, hash, level + 1u);
  if (child == old->slots[slot]) return node;
  IndexNode *copy = heap_alloc(heap, sizeof(IndexNode));
  if (copy == NULL) return node;
  memcpy(copy, old, sizeof(IndexNode));
  copy->slots[slot] = child;
  return copy;
}

static IndexGet index_get(Index ix, Str key) {
  uint64_t hash = index_hash(key);
  const void *node = ix.root;
  for (unsigned level = 0; level < INDEX_DEPTH && node != NULL; level++) {
    node = ((const IndexNode *)node)->slots[index_slot(hash, level)];
  }
  const IndexLeaf *leaf = leaf_find(node, key);
  IndexGet result;
  result.found = leaf != NULL;
  result.value = leaf != NULL ? leaf->value : NULL;
  return result;
}

static Index index_set(Heap *heap, Index ix, Str key, const void *value) {
  Index result;
  result.root = node_set(heap, ix.root, key, index_hash(key), value, 0u);
  return result;
}

static Index index_del(Heap *heap, Index ix, Str key) {
  Index result;
  result.root = node_del(heap, ix.root, key, index_hash(key), 0u);
  return result;
}

/* ---- global.bend definitions in source order ---- */

Entry global_entry_def(DefEntry value) {
  Entry e = entry_zero;
  e.kind = ENTRY_DEF;
  e.def = value;
  return e;
}

Entry global_entry_axiom(const Term *ty) {
  Entry e = entry_zero;
  e.kind = ENTRY_AXIOM;
  e.ty = ty;
  return e;
}

Entry global_entry_prim(const Term *ty, Prim prim) {
  Entry e = entry_zero;
  e.kind = ENTRY_PRIM;
  e.ty = ty;
  e.prim = prim;
  return e;
}

const EntryRows *global_entry_rows(Heap *heap, Str name, Entry entry, const EntryRows *rest) {
  EntryRows *cell = heap_alloc(heap, sizeof(EntryRows));
  if (cell == NULL) return rest;
  cell->name = name;
  cell->entry = entry;
  cell->rest = rest;
  return cell;
}

const FamilyRows *global_family_rows(Heap *heap, Str name, Family family, const FamilyRows *rest) {
  FamilyRows *cell = heap_alloc(heap, sizeof(FamilyRows));
  if (cell == NULL) return rest;
  cell->name = name;
  cell->family = family;
  cell->rest = rest;
  return cell;
}

Globals global_empty(void) {
  Globals g;
  g.entries = NULL;
  g.families = NULL;
  g.entry_index.root = NULL;
  g.family_index.root = NULL;
  return g;
}

static MaybeEntry some_entry(const Entry *entry) {
  MaybeEntry result;
  result.some = entry != NULL;
  result.value = entry != NULL ? *entry : entry_zero;
  return result;
}

static MaybeFamily some_family(const Family *family) {
  MaybeFamily result;
  result.some = family != NULL;
  result.value = family != NULL ? *family : family_zero;
  return result;
}

MaybeEntry global_find_entry(const EntryRows *entries, Str name) {
  const EntryRows *row = entries;
  while (row != NULL && !str_eq(row->name, name)) row = row->rest;
  return some_entry(row != NULL ? &row->entry : NULL);
}

/* map_result and map_present: the value half of a Map.get result and the presence half of Map.has. */
static const void *map_result(IndexGet result) { return result.value; }

static bool map_present(IndexGet result) { return result.found; }

MaybeEntry global_find(Str name, Globals globals) {
  return some_entry(map_result(index_get(globals.entry_index, name)));
}

const EntryRows *global_remove_entry(Heap *heap, const EntryRows *entries, Str name) {
  const EntryRows *result = NULL;
  const EntryRows **tail = &result;
  for (const EntryRows *row = entries; row != NULL; row = row->rest) {
    if (str_eq(row->name, name)) continue;
    EntryRows *cell = heap_alloc(heap, sizeof(EntryRows));
    if (cell == NULL) return result;
    cell->name = row->name;
    cell->entry = row->entry;
    cell->rest = NULL;
    *tail = cell;
    tail = &cell->rest;
  }
  return result;
}

Globals global_add(Heap *heap, Str name, Entry entry, Globals globals) {
  bool present = map_present(index_get(globals.entry_index, name));
  const EntryRows *selected = present ? global_remove_entry(heap, globals.entries, name) : globals.entries;
  const EntryRows *entries = global_entry_rows(heap, name, entry, selected);
  Globals result = globals;
  result.entries = entries;
  if (entries != selected) result.entry_index = index_set(heap, globals.entry_index, name, &entries->entry);
  return result;
}

MaybeFamily global_find_family_entry(const FamilyRows *families, Str name) {
  const FamilyRows *row = families;
  while (row != NULL && !str_eq(row->name, name)) row = row->rest;
  return some_family(row != NULL ? &row->family : NULL);
}

MaybeFamily global_find_family(Str name, Globals globals) {
  return some_family(map_result(index_get(globals.family_index, name)));
}

const FamilyRows *global_remove_family(Heap *heap, const FamilyRows *families, Str name) {
  const FamilyRows *result = NULL;
  const FamilyRows **tail = &result;
  for (const FamilyRows *row = families; row != NULL; row = row->rest) {
    if (str_eq(row->name, name)) continue;
    FamilyRows *cell = heap_alloc(heap, sizeof(FamilyRows));
    if (cell == NULL) return result;
    cell->name = row->name;
    cell->family = row->family;
    cell->rest = NULL;
    *tail = cell;
    tail = &cell->rest;
  }
  return result;
}

Globals global_add_family(Heap *heap, Str name, Family family, Globals globals) {
  bool present = map_present(index_get(globals.family_index, name));
  const FamilyRows *selected = present ? global_remove_family(heap, globals.families, name) : globals.families;
  const FamilyRows *families = global_family_rows(heap, name, family, selected);
  Globals result = globals;
  result.families = families;
  if (families != selected) result.family_index = index_set(heap, globals.family_index, name, &families->family);
  return result;
}

const Term *global_entry_ty(Entry e) {
  switch (e.kind) {
    case ENTRY_DEF: return e.def.ty;
    case ENTRY_AXIOM: return e.ty;
    case ENTRY_PRIM: return e.ty;
  }
  return term_auto();
}

MaybeDefEntry global_def_of(Entry e) {
  MaybeDefEntry result;
  result.some = false;
  result.value = def_entry_zero;
  switch (e.kind) {
    case ENTRY_DEF:
      result.some = true;
      result.value = e.def;
      return result;
    case ENTRY_AXIOM: return result;
    case ENTRY_PRIM: return result;
  }
  return result;
}

MaybeDefEntry global_find_def_result(MaybeEntry e) {
  if (!e.some) {
    MaybeDefEntry none;
    none.some = false;
    none.value = def_entry_zero;
    return none;
  }
  return global_def_of(e.value);
}

MaybeDefEntry global_find_def(Str name, Globals globals) {
  return global_find_def_result(global_find(name, globals));
}

MaybePrim global_find_prim_result(MaybeEntry e) {
  MaybePrim result;
  result.some = e.some && e.value.kind == ENTRY_PRIM;
  result.value = result.some ? e.value.prim : PRIM_NAT_ADD;
  return result;
}

MaybePrim global_find_prim(Str name, Globals globals) {
  return global_find_prim_result(global_find(name, globals));
}

Globals global_add_prims(Heap *heap, const Vec *ps, Globals globals) {
  Globals acc = globals;
  for (size_t i = 0; i < ps->len; i++) {
    VecGet got = vec_get(ps, i);
    if (got.status != VEC_GET_OK) return acc;
    const Prim *p = got.value;
    acc = global_add(heap, prim_name(*p), global_entry_prim(prim_ty(heap, *p), *p), acc);
  }
  return acc;
}

Globals global_initial(Heap *heap) {
  Vec catalog = prim_catalog(heap);
  Globals base = global_add(heap, str_lit("Nat"), global_entry_axiom(term_univ(heap, level_one(heap))), global_empty());
  return global_add_prims(heap, &catalog, base);
}

/* index_rows: Bend indexes the rest before the head, so the first row of each name wins; here each
 * name is set once, front to back, which gives the same map. */
Index global_index_entry_rows(Heap *heap, const EntryRows *rows) {
  Index ix;
  ix.root = NULL;
  for (const EntryRows *row = rows; row != NULL; row = row->rest) {
    if (!map_present(index_get(ix, row->name))) ix = index_set(heap, ix, row->name, &row->entry);
  }
  return ix;
}

Index global_index_family_rows(Heap *heap, const FamilyRows *rows) {
  Index ix;
  ix.root = NULL;
  for (const FamilyRows *row = rows; row != NULL; row = row->rest) {
    if (!map_present(index_get(ix, row->name))) ix = index_set(heap, ix, row->name, &row->family);
  }
  return ix;
}

Globals global_of_lists(Heap *heap, const EntryRows *entries, const FamilyRows *families) {
  Globals g;
  g.entries = entries;
  g.families = families;
  g.entry_index = global_index_entry_rows(heap, entries);
  g.family_index = global_index_family_rows(heap, families);
  return g;
}

Globals global_without_family(Heap *heap, Str name, Globals globals) {
  Globals result = globals;
  result.families = global_remove_family(heap, globals.families, name);
  result.family_index = index_del(heap, globals.family_index, name);
  return result;
}

bool global_same_rec_arg(MaybeNat a, MaybeNat b) {
  if (!a.some && !b.some) return true;
  if (a.some && b.some) return nat_cmp(a.value, b.value) == 0;
  return false;
}

bool global_same_entry(Entry a, Entry b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case ENTRY_DEF:
      return term_equal(a.def.ty, b.def.ty) && term_equal(a.def.body, b.def.body) &&
             positivity_same_bool(a.def.reducible, b.def.reducible) &&
             global_same_rec_arg(a.def.rec_arg, b.def.rec_arg) &&
             positivity_same_bool(a.def.partial, b.def.partial);
    case ENTRY_AXIOM: return term_equal(a.ty, b.ty);
    case ENTRY_PRIM: return term_equal(a.ty, b.ty) && str_eq(prim_name(a.prim), prim_name(b.prim));
  }
  return false;
}

bool global_entry_kept(Entry entry, MaybeEntry found) {
  return found.some && global_same_entry(entry, found.value);
}

bool global_entries_kept(const EntryRows *entries, Globals later) {
  for (const EntryRows *row = entries; row != NULL; row = row->rest) {
    if (!global_entry_kept(row->entry, global_find(row->name, later))) return false;
  }
  return true;
}

bool global_family_kept(Family family, MaybeFamily found) {
  return found.some && positivity_same_family(family, found.value);
}

bool global_families_kept(const FamilyRows *families, Globals later) {
  for (const FamilyRows *row = families; row != NULL; row = row->rest) {
    if (!global_family_kept(row->family, global_find_family(row->name, later))) return false;
  }
  return true;
}

/* True when later has each entry and each family of earlier with an equal value: later only adds names. */
bool global_extended_by(Globals earlier, Globals later) {
  return global_entries_kept(earlier.entries, later) && global_families_kept(earlier.families, later);
}

/* ---- compact (host only, no Bend counterpart; decision 157) ---- */
/* Node by node copy of a trie into heap->arena: del leaves tombstone-free holes only through
 * copied nodes, so a rebuild from the row lists is not equivalent and is never done. Every leaf
 * value goes through remap, which returns the pointer the value takes in the new arena. */
static const IndexLeaf *leaf_copy(Heap *heap, const IndexLeaf *leaf, IndexRemap remap, void *ctx) {
  IndexLeaf *head = NULL;
  IndexLeaf *tail = NULL;
  for (const IndexLeaf *cursor = leaf; cursor != NULL; cursor = cursor->rest) {
    IndexLeaf *node = heap_alloc(heap, sizeof(IndexLeaf));
    if (node == NULL) return head;
    node->key = str_copy(heap->arena, cursor->key);
    if (node->key.data == NULL && cursor->key.len > 0) heap->oom = true;
    node->value = remap(ctx, cursor->key, cursor->value);
    node->rest = NULL;
    if (tail == NULL) head = node; else tail->rest = node;
    tail = node;
  }
  return head;
}

static const void *node_copy(Heap *heap, const void *node, unsigned level, IndexRemap remap, void *ctx) {
  if (node == NULL) return NULL;
  if (level >= INDEX_DEPTH) return leaf_copy(heap, node, remap, ctx);
  IndexNode *copy = heap_alloc(heap, sizeof(IndexNode));
  if (copy == NULL) return NULL;
  const IndexNode *old = node;
  for (unsigned slot = 0; slot < INDEX_WIDTH; slot++) {
    copy->slots[slot] = node_copy(heap, old->slots[slot], level + 1u, remap, ctx);
  }
  return copy;
}

Index index_copy(Heap *heap, Index ix, IndexRemap remap, void *ctx) {
  Index result;
  result.root = node_copy(heap, ix.root, 0u, remap, ctx);
  return result;
}
