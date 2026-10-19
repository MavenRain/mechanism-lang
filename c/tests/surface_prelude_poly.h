/* Library boundary of c/tests/surface_prelude_poly.c (port of tests/surface_prelude_poly.bend).
 * surface_foundation_common and surface_raw_common import the Bend module as Audit; they link
 * _tcc/surface_prelude_poly_lib.o (built with -DMECH_DRIVER_LIBRARY, which drops main). */
#ifndef MECH_SURFACE_PRELUDE_POLY_H
#define MECH_SURFACE_PRELUDE_POLY_H

#include "relational_common.h"
#include "surface.h"

/* Instance{name, levels, alias}: levels is a Vec of const LevelVar pointers. */
typedef struct {
  Str name;
  Vec levels;
  Str alias;
} PolyAuditInstance;

bool surface_prelude_poly_arity(MaybeNat value, uint64_t expected);
Globals surface_prelude_poly_globals(const ElabChecked *checked);
Task surface_prelude_poly_audit(Heap *heap, Globals globals);
Task surface_prelude_poly_install(Heap *heap, const PolyAuditInstance *instances, size_t count, FamilyCatalog catalog,
                                  Globals current, Globals *out);
bool surface_prelude_poly_starts(Str expected, Str actual);
Task surface_prelude_poly_refusal(Heap *heap, Result result, Str expected, Str message);

#endif
