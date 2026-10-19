/* Port of the library half of tests/surface_prenex.bend: the Tiny terms that surface_prenex_families
 * imports as Prenex.zero, Prenex.succ and Prenex.tiny. _tcc/surface_prenex_lib.o is built from
 * c/tests/surface_prenex.c with -DMECH_DRIVER_LIBRARY (no main). */
#ifndef MECH_SURFACE_PRENEX_H
#define MECH_SURFACE_PRENEX_H

#include "relational_common.h"

const Term *surface_prenex_zero(Heap *heap);
const Term *surface_prenex_succ(Heap *heap, const Term *value);
const Term *surface_prenex_tiny(Heap *heap);

#endif
