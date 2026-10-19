/* Shared view of tests/surface_catalog.bend: surface_boundary.bend and surface_shape_metadata.bend
 * import it as Test and read ensure, marker, ctors, marker_ty, witness, alias, scope_refused,
 * budget_refused and second_callback. The driver object keeps its main; the library object (compiled
 * with MECH_DRIVER_LIBRARY) exports these definitions alone, under the catalog_ prefix. */
#ifndef MECH_SURFACE_CATALOG_H
#define MECH_SURFACE_CATALOG_H

#include "surface.h"

/* ensure(ok, message): Done{Unit} or Fail{Error.Mismatch{message}}. */
Result catalog_ensure(bool ok, const char *message);
/* marker(): Check.FamilyDecl{"Marker", [], [], Level.one()} */
FamilyDecl catalog_marker(Heap *heap);
/* ctors(): [Check.CtorDecl{"mark", [], [], []}] as a Vec of CtorDecl pointers. */
Vec catalog_ctors(Heap *heap);
/* marker_ty(name): Term.Lan{Shape.SMu{name, []}, Rules.diagram_of([])} */
const Term *catalog_marker_ty(Heap *heap, Str name);
/* witness(): the definition "witness" of type marker_ty("Marker") whose body is Term.In{SMu{"Marker"}, ACtor{"mark"}, []}. */
Decl catalog_witness(Heap *heap);
/* alias(): the definition "alias" of type marker_ty("Marker") whose body is Term.Global{"witness"}. */
Decl catalog_alias(Heap *heap);
/* scope_refused(result): Fail{Error.Universe{"universe level is outside the global parameter scope"}}. */
bool catalog_scope_refused(Result result);
/* budget_refused(result): Fail{Error.Budget_exhausted{_}}. */
bool catalog_budget_refused(Result result);
/* second_callback(globals): a FamilyMemberElaborator fn that checks "witness" is already a member and returns alias(). */
Comp catalog_second_callback(Heap *heap, void *ctx, Globals globals);

#endif
