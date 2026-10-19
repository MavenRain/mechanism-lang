/* Port of the library half of tests/surface_prenex_exports.bend: roundtrip, which
 * surface_prenex_dependency_exports imports as Exports.roundtrip. _tcc/surface_prenex_exports_lib.o is built
 * from c/tests/surface_prenex_exports.c with -DMECH_DRIVER_LIBRARY (no main). */
#ifndef MECH_SURFACE_PRENEX_EXPORTS_H
#define MECH_SURFACE_PRENEX_EXPORTS_H

#include "relational_common.h"

Task surface_prenex_exports_roundtrip(Heap *heap, Str source, Str message);

#endif
