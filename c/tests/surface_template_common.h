/* Port of bend2/tests/surface_template_common.bend: shared helpers of the template drivers
 * (surface_template_composition, surface_template_reuse, surface_template_symbolic_reuse). Library only (no
 * main); _tcc/surface_template_common.o links into its importers with _tcc/surface_template_data.o,
 * _tcc/surface_prenex_common.o, _tcc/surface_prenex_data.o, _tcc/surface_raw_common.o and
 * $(C_FOUNDATION_COMMON). surface_family_poly.c keeps its own inline copies of the defs it uses. */
#ifndef MECH_SURFACE_TEMPLATE_COMMON_H
#define MECH_SURFACE_TEMPLATE_COMMON_H

#include "relational_common.h"
#include "surface.h"
#include "surface_prenex_data.h"
#include "surface_raw_common.h"

Task surface_template_common_exact(Heap *heap, Str label, Str expected, Result result);
Task surface_template_common_negatives(Heap *heap, const PrenexNegative *cases, size_t len, Globals globals);
/* Result<String, P.Family>: the Done family goes to *out. */
Task surface_template_common_required_family(MaybeFamily value, Family *out);
Task surface_template_common_family(Str name, Globals globals, Family *out);
const FamilyRows *surface_template_common_family_rows(Globals globals);
const EntryRows *surface_template_common_entries(Globals globals);
/* run(cases, count): Done count + the number of cases run goes to *out. */
Task surface_template_common_run(Heap *heap, const RawCase *cases, size_t len, uint64_t count, uint64_t *out);
bool surface_template_common_same_families(Globals a, Globals b);
/* new_family_names(globals, initial): a Vec of Str pointers. */
Vec surface_template_common_new_family_names(Heap *heap, Globals globals, Globals initial);
/* no_axioms(rows): rows is an Elab.Rows Vec (relational_rows). */
bool surface_template_common_no_axioms(Heap *heap, const Vec *rows);
/* altered(family, mode): mode 0 clears positive, mode 1 sets the Provisional status, mode 2 and above drops the
 * constructors. */
Family surface_template_common_altered(Family family, uint64_t mode);
Error surface_template_common_mismatch(Heap *heap, Str name);

#endif
