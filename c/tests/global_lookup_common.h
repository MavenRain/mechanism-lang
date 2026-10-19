/* The helpers of tests/kernel_global_lookup.bend, shared with kernel_global_index.c (whose Bend
 * source imports kernel_global_lookup.bend as L). */
#ifndef MECH_GLOBAL_LOOKUP_COMMON_H
#define MECH_GLOBAL_LOOKUP_COMMON_H

#include "driver.h"

/* entry(name) = G.Axiom{T.Global{name}} */
static inline Entry lookup_entry(Heap *heap, Str name) {
  return global_entry_axiom(term_global(heap, name));
}

/* entries() = [x: first, y: tail, x: second] */
static inline const EntryRows *lookup_entries(Heap *heap) {
  const EntryRows *rows = global_entry_rows(heap, str_lit("x"), lookup_entry(heap, str_lit("second")), NULL);
  rows = global_entry_rows(heap, str_lit("y"), lookup_entry(heap, str_lit("tail")), rows);
  return global_entry_rows(heap, str_lit("x"), lookup_entry(heap, str_lit("first")), rows);
}

/* entry_is: Some{Axiom{Global{name}}} with the expected name */
static inline bool lookup_entry_is(MaybeEntry result, Str expected) {
  return result.some && result.value.kind == ENTRY_AXIOM && result.value.ty->kind == TERM_GLOBAL &&
         str_eq(result.value.ty->name, expected);
}

/* family(name) = P.Family{name, [], [], L.zero(), P.Builtin{}, [], False{}} */
static inline Family lookup_family(Heap *heap, Str name) {
  Family family;
  family.name = name;
  family.params = vec_new(heap->arena);
  family.indices = vec_new(heap->arena);
  family.level = level_zero();
  family.status.kind = CTOR_STATUS_BUILTIN;
  family.status.names = vec_new(heap->arena);
  family.ctors = vec_new(heap->arena);
  family.positive = false;
  return family;
}

/* families() = [x: first, y: tail, x: second] */
static inline const FamilyRows *lookup_families(Heap *heap) {
  const FamilyRows *rows = global_family_rows(heap, str_lit("x"), lookup_family(heap, str_lit("second")), NULL);
  rows = global_family_rows(heap, str_lit("y"), lookup_family(heap, str_lit("tail")), rows);
  return global_family_rows(heap, str_lit("x"), lookup_family(heap, str_lit("first")), rows);
}

/* family_is: Some{Family{name, ...}} with the expected name */
static inline bool lookup_family_is(MaybeFamily result, Str expected) {
  return result.some && str_eq(result.value.name, expected);
}

/* missing: None */
static inline bool lookup_missing_entry(MaybeEntry result) { return !result.some; }

static inline bool lookup_missing_family(MaybeFamily result) { return !result.some; }

#endif
