/* Port of tests/surface_prenex_common.bend: shared helpers of the prenex drivers (surface_prenex,
 * surface_prenex_exports, surface_prenex_dependency_exports, surface_prenex_families, surface_prenex_groups).
 * Library only (no main); _tcc/surface_prenex_common.o links into its importers with
 * _tcc/surface_prenex_data.o, _tcc/surface_raw_common.o, _tcc/surface_foundation_common.o and
 * _tcc/surface_prelude_poly_lib.o. */
#ifndef MECH_SURFACE_PRENEX_COMMON_H
#define MECH_SURFACE_PRENEX_COMMON_H

#include "relational_common.h"
#include "surface.h"
#include "surface_prenex_data.h"

Str surface_prenex_common_kind(Error error);
Task surface_prenex_common_refusal(Heap *heap, Str label, Str expected_kind, Str expected, Result result);
/* control(source, globals): source NULL is None{}. */
Task surface_prenex_common_control(Heap *heap, const char *source, Globals globals);
Task surface_prenex_common_negatives(Heap *heap, const PrenexNegative *cases, size_t len, Globals globals);
Task surface_prenex_common_parse_refusal(Heap *heap, Str label, Str expected, bool prefix, Result result);
Task surface_prenex_common_parsers(Heap *heap, const PrenexParseCase *cases, size_t len, bool prefix);
Task surface_prenex_common_positives(Heap *heap, const PrenexPositive *cases, size_t len);
/* absent_entries(names, globals): names is a Vec of Str pointers. */
bool surface_prenex_common_absent_entries(const Vec *names, Globals globals);
uint64_t surface_prenex_common_family_count(Globals globals);
Vec surface_prenex_common_family_names(Heap *heap, Globals globals);
Comp surface_prenex_common_action(Heap *heap, Globals globals, Str source);
Task surface_prenex_common_polls(Heap *heap, Str source, uint64_t *polls);
Task surface_prenex_common_exhausted(Heap *heap, Str label, Str source, uint64_t allowance);
/* boundary_decl(declarations): declarations is a Vec of const SDecl pointers. */
Task surface_prenex_common_boundary_decl(Heap *heap, const Vec *declarations);
Task surface_prenex_common_ast_boundary(Heap *heap, Str source);

#endif
