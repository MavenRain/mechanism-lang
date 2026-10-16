/* Port of bend2/tests/surface_raw_common.bend: the shared refusal, inventory and case-runner helpers of the
 * raw surface drivers. Library only (no main); _tcc/surface_raw_common.o links into its importers. */
#ifndef MECH_SURFACE_RAW_COMMON_H
#define MECH_SURFACE_RAW_COMMON_H

#include "relational_common.h"
#include "surface.h"

/* Case{name, test: Unit -> Result<String, Unit>}: test(heap, ctx) runs the case. */
typedef struct {
  Str name;
  Task (*test)(Heap *heap, void *ctx);
  void *ctx;
} RawCase;

Task surface_raw_common_refusal(Heap *heap, Error expected, Result result);
Task surface_raw_common_exact(Heap *heap, Error expected, Result result);
Error surface_raw_common_collision(Heap *heap, Str name);
bool surface_raw_common_absent(bool some);
bool surface_raw_common_present(bool some);
bool surface_raw_common_matched_entry(Entry expected, MaybeEntry actual);
bool surface_raw_common_matched_family(Family expected, MaybeFamily actual);
bool surface_raw_common_entries(const EntryRows *rows, Globals globals);
bool surface_raw_common_families(const FamilyRows *rows, Globals globals);
bool surface_raw_common_same_globals(Globals actual, Globals expected);
Task surface_raw_common_labeled(Heap *heap, Str name, Task result);
Task surface_raw_common_run(Heap *heap, const RawCase *cases, size_t len, uint64_t count, Str *out);
Error surface_raw_common_budget_error(void);
Error surface_raw_common_scope_error(void);
Task surface_raw_common_measure(Heap *heap, BudgetState result, uint64_t *polls);

#endif
