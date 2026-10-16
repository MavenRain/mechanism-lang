/* Port of bend2/tests/surface_foundation_common.bend: shared helpers of the foundation prelude drivers
 * (congruence, dependent, equality_ops, transport). Library only (no main); _tcc/surface_foundation_common.o
 * links into its importers with _tcc/surface_prelude_poly_lib.o and _tcc/surface_foundation_data.o. */
#ifndef MECH_SURFACE_FOUNDATION_COMMON_H
#define MECH_SURFACE_FOUNDATION_COMMON_H

#include "relational_common.h"
#include "surface.h"
#include "surface_foundation_data.h"

/* Instance{name, levels, alias, contract}: levels is a Vec of const LevelVar pointers; an empty contract
 * name checks no contract. */
typedef struct {
  Str name;
  Vec levels;
  Str alias;
  Str contract;
} FoundationInstance;

const LevelVar *surface_foundation_common_level(Heap *heap, uint64_t index);
Task surface_foundation_common_contract(Heap *heap, const FoundationContract *contracts, size_t count, Str name,
                                        Str *source);
Task surface_foundation_common_check_contract(Heap *heap, Globals globals, Str name, const FoundationContract *contracts,
                                              size_t count, Globals *out);
Task surface_foundation_common_install_families(Heap *heap, const FoundationInstance *instances, size_t len,
                                                FamilyCatalog catalog, const FoundationContract *contracts,
                                                size_t count, Globals globals, Globals *out);
uint64_t surface_foundation_common_entry_count(Globals globals);
Globals surface_foundation_common_instance_globals(PolyInstance instance);
Task surface_foundation_common_install_terms(Heap *heap, const FoundationInstance *instances, size_t len,
                                             PolyCatalog catalog, const FoundationContract *contracts, size_t count,
                                             Globals globals, uint64_t added, Globals *out_globals,
                                             uint64_t *out_added);
Task surface_foundation_common_negatives(Heap *heap, const FoundationNegative *cases, size_t len, Globals globals);
Callback surface_foundation_common_exhausted(Heap *heap);
Callback surface_foundation_common_countdown(Heap *heap, uint64_t remaining);
Task surface_foundation_common_budget_refusal(Heap *heap, Result result);
const Term *surface_foundation_common_zero(Heap *heap);
const Term *surface_foundation_common_one(Heap *heap);
Vec surface_foundation_common_ones(Heap *heap, const char *const *names, size_t len);
Task surface_foundation_common_audit(Heap *heap, Globals globals);

#endif
