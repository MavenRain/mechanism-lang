/* Port of the library half of tests/surface_family_groups.bend: the marker family, constructor, type,
 * value, definition, single declaration and one-entry globals that surface_family_members imports as
 * Groups.marker, Groups.ctor, Groups.ty, Groups.value, Groups.definition, Groups.single and
 * Groups.with_entry. _tcc/surface_family_groups_lib.o is built from c/tests/surface_family_groups.c with
 * -DMECH_DRIVER_LIBRARY (no main). */
#ifndef MECH_SURFACE_FAMILY_GROUPS_H
#define MECH_SURFACE_FAMILY_GROUPS_H

#include "relational_common.h"
#include "surface.h"

FamilyDecl surface_family_groups_marker(Heap *heap, Str name);
/* Boxed for a list; NULL after a failed allocation (heap->oom set). */
CtorDecl *surface_family_groups_ctor(Heap *heap);
const Term *surface_family_groups_ty(Heap *heap, Str name);
const Term *surface_family_groups_value(Heap *heap, Str name, Vec args);
Decl surface_family_groups_definition(Str name, const Term *ty, const Term *body);
Result surface_family_groups_single(Heap *heap, Str name, FamilyCatalog catalog);
Globals surface_family_groups_with_entry(Heap *heap, Str name);

#endif
