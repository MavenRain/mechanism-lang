/* compact: deep copies of the surface catalogs that survive a declaration (host only, no Bend
 * counterpart; decision 156). The kernel half (terms, globals, check rows) is c/kernel/compact.c.
 * Scheme members are the checked member Decls, copied by compact_decl. Every function takes the destination heap
 * (heap->arena is the target arena); allocation failure sets heap->oom and returns a placeholder. */
#include <string.h>

#include "surface.h"

static void *compact_ctor_decl_item(Heap *heap, const void *item) {
  const CtorDecl *ctor = item;
  CtorDecl *out = heap_alloc(heap, sizeof(CtorDecl));
  if (out == NULL) return NULL;
  out->name = compact_str(heap, ctor->name);
  out->args = compact_tele(heap, &ctor->args);
  out->res_params = compact_term_vec(heap, &ctor->res_params);
  out->res_idx = compact_term_vec(heap, &ctor->res_idx);
  return out;
}

Vec compact_ctor_decls(Heap *heap, const Vec *ctors) { return compact_vec_with(heap, ctors, compact_ctor_decl_item); }

FamilyDecl compact_family_decl(Heap *heap, FamilyDecl family) {
  FamilyDecl out = family;
  out.name = compact_str(heap, family.name);
  out.params = compact_tele(heap, &family.params);
  out.indices = compact_tele(heap, &family.indices);
  out.level = compact_level(heap, family.level);
  return out;
}

static void *compact_family_raw_item(Heap *heap, const void *item) {
  const FamilyRaw *raw = item;
  FamilyRaw *out = heap_alloc(heap, sizeof(FamilyRaw));
  if (out == NULL) return NULL;
  out->family = compact_family_decl(heap, raw->family);
  out->ctors = compact_ctor_decls(heap, &raw->ctors);
  return out;
}

/* Scheme members are the checked member Decls (family_member_names precedent). */
static void *compact_member_item(Heap *heap, const void *item) {
  const Decl *member = item;
  Decl *out = heap_alloc(heap, sizeof(Decl));
  if (out == NULL) return NULL;
  *out = compact_decl(heap, *member);
  return out;
}

FamilyScheme compact_family_scheme(Heap *heap, FamilyScheme scheme) {
  FamilyScheme out;
  out.arity = compact_nat(heap, scheme.arity);
  out.family = compact_family_decl(heap, scheme.family);
  out.ctors = compact_ctor_decls(heap, &scheme.ctors);
  out.companions = compact_vec_with(heap, &scheme.companions, compact_family_raw_item);
  out.members = compact_vec_with(heap, &scheme.members, compact_member_item);
  out.origin = compact_globals(heap, scheme.origin);
  return out;
}

PolyScheme compact_poly_scheme(Heap *heap, PolyScheme scheme) {
  PolyScheme out;
  out.arity = compact_nat(heap, scheme.arity);
  out.decl = compact_decl(heap, scheme.decl);
  return out;
}

/* Cons lists are copied front to back with a tail pointer: the list order is the lookup order. */
PolyCatalog compact_poly_catalog(Heap *heap, PolyCatalog catalog) {
  const PolyRow *head = NULL;
  PolyRow *tail = NULL;
  for (const PolyRow *row = catalog; row != NULL; row = row->rest) {
    PolyRow *copy = heap_alloc(heap, sizeof(PolyRow));
    if (copy == NULL) return head;
    copy->name = compact_str(heap, row->name);
    copy->scheme = compact_poly_scheme(heap, row->scheme);
    copy->rest = NULL;
    *(tail == NULL ? &head : &tail->rest) = copy;
    tail = copy;
  }
  return head;
}

FamilyCatalog compact_family_catalog(Heap *heap, FamilyCatalog catalog) {
  const FamilyRow *head = NULL;
  FamilyRow *tail = NULL;
  for (const FamilyRow *row = catalog; row != NULL; row = row->rest) {
    FamilyRow *copy = heap_alloc(heap, sizeof(FamilyRow));
    if (copy == NULL) return head;
    copy->name = compact_str(heap, row->name);
    copy->scheme = compact_family_scheme(heap, row->scheme);
    copy->rest = NULL;
    *(tail == NULL ? &head : &tail->rest) = copy;
    tail = copy;
  }
  return head;
}

/* The member-level survivor of a group entry (USER ruling 2026-10-12 (b)): both tables are copied, the
 * origin and the symbolic one, since the symbolic table extends the origin with the group's families
 * and the members checked so far. */
FamilyGroupState compact_family_group_state(Heap *heap, const FamilyGroupState *group) {
  FamilyGroupState out;
  out.globals = compact_globals(heap, group->globals);
  out.symbolic = compact_globals(heap, group->symbolic);
  out.catalog = compact_family_catalog(heap, group->catalog);
  out.arity = compact_nat(heap, group->arity);
  out.raw.family = compact_family_decl(heap, group->raw.family);
  out.raw.ctors = compact_ctor_decls(heap, &group->raw.ctors);
  out.companions = compact_vec_with(heap, &group->companions, compact_family_raw_item);
  out.members = compact_vec_with(heap, &group->members, compact_member_item);
  return out;
}

/* The member-level survivor of a composition entry: the group state, the root name and the pending
 * specialization members (Decls, copied like scheme members). */
FamilyComposeState compact_family_compose_state(Heap *heap, const FamilyComposeState *state) {
  FamilyComposeState out;
  out.group = compact_family_group_state(heap, &state->group);
  out.name = compact_str(heap, state->name);
  out.pending = compact_vec_with(heap, &state->pending, compact_member_item);
  return out;
}

/* ---- the specialization-level survivor of a composition entry (decision 173): the lowered dependencies
 * (Str names, LevelVar pointers, PolyRename pointers) and the Composed fold value ---- */
static void *compact_level_item(Heap *heap, const void *item) { return (void *)compact_level(heap, item); }

static void *compact_rename_item(Heap *heap, const void *item) {
  const PolyRename *rename = item;
  PolyRename *out = heap_alloc(heap, sizeof(PolyRename));
  if (out == NULL) return NULL;
  out->source = compact_str(heap, rename->source);
  out->target = compact_str(heap, rename->target);
  return out;
}

static FamilyMaybeVec compact_maybe_renames(Heap *heap, FamilyMaybeVec maybe) {
  FamilyMaybeVec out;
  memset(&out, 0, sizeof out);
  out.some = maybe.some;
  if (maybe.some) out.value = compact_vec_with(heap, &maybe.value, compact_rename_item);
  return out;
}

static void *compact_family_imported_item(Heap *heap, const void *item) {
  const FamilyImported *imported = item;
  FamilyImported *out = heap_alloc(heap, sizeof(FamilyImported));
  if (out == NULL) return NULL;
  out->reused = imported->reused;
  out->family = compact_family_decl(heap, imported->family);
  out->ctors = compact_ctor_decls(heap, &imported->ctors);
  return out;
}

static void *compact_family_dependency_item(Heap *heap, const void *item) {
  const FamilyDependency *dependency = item;
  FamilyDependency *out = heap_alloc(heap, sizeof(FamilyDependency));
  if (out == NULL) return NULL;
  out->source = compact_str(heap, dependency->source);
  out->levels = compact_vec_with(heap, &dependency->levels, compact_level_item);
  out->alias = compact_str(heap, dependency->alias);
  out->reuse = compact_vec_with(heap, &dependency->reuse, compact_rename_item);
  out->exports = compact_maybe_renames(heap, dependency->exports);
  return out;
}

/* Composed{aliases, available, imports, members, trusted}: Str names, FamilyImported items and Decls. */
static FamilyComposed compact_family_composed(Heap *heap, const FamilyComposed *composed) {
  FamilyComposed out;
  out.aliases = compact_str_vec(heap, &composed->aliases);
  out.available = compact_str_vec(heap, &composed->available);
  out.imports = compact_vec_with(heap, &composed->imports, compact_family_imported_item);
  out.members = compact_vec_with(heap, &composed->members, compact_member_item);
  out.trusted = compact_vec_with(heap, &composed->trusted, compact_member_item);
  return out;
}

FamilyComposeStart compact_family_compose_start(Heap *heap, const FamilyComposeStart *start) {
  FamilyComposeStart out;
  out.dependencies = compact_vec_with(heap, &start->dependencies, compact_family_dependency_item);
  out.composed = compact_family_composed(heap, &start->composed);
  return out;
}

/* The member-level survivor of an instantiate entry (USER ruling 2026-10-12 (e)): the installed table and the
 * members from index `from` on, copied like scheme members; the checked ones are dropped. */
FamilyInstantiateStart compact_family_instantiate_start(Heap *heap, const FamilyInstantiateStart *start, size_t from) {
  FamilyInstantiateStart out;
  size_t i = from;
  out.installed = compact_globals(heap, start->installed);
  out.members = vec_new(heap->arena);
  for (;;) {
    VecGet got = vec_get(&start->members, i);
    void *copy;
    if (got.status != VEC_GET_OK) return out;
    copy = compact_member_item(heap, got.value);
    if (!vec_push(&out.members, copy)) heap->oom = true;
    i = i + 1;
  }
}

ElabProgram compact_program(Heap *heap, const ElabProgram *program) {
  ElabProgram out;
  out.globals = compact_globals(heap, program->globals);
  out.definitions = compact_poly_catalog(heap, program->definitions);
  out.families = compact_family_catalog(heap, program->families);
  out.rows = compact_check_rows(heap, &program->rows);
  return out;
}
