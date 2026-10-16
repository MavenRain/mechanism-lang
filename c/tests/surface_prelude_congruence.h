/* Library boundary of c/tests/surface_prelude_congruence.c (port of bend2/tests/surface_prelude_congruence.bend).
 * surface_prelude_transport imports the Bend module as Congruence; it links
 * _tcc/surface_prelude_congruence_lib.o (built with -DMECH_DRIVER_LIBRARY, which drops main). */
#ifndef MECH_SURFACE_PRELUDE_CONGRUENCE_H
#define MECH_SURFACE_PRELUDE_CONGRUENCE_H

#include <stdbool.h>

/* absent(value): the Maybe is None. The payload is never read, so the port takes the Maybe `some` flag. */
bool surface_prelude_congruence_absent(bool some);

#endif
