/* The kernel_recursive driver as a library (MECH_DRIVER_LIBRARY drops its main): kernel_protocol imports
 * bend2/tests/kernel_recursive.bend as Recursive and calls Recursive.source() and Recursive.validate(checked). */
#ifndef MECH_KERNEL_RECURSIVE_H
#define MECH_KERNEL_RECURSIVE_H

#include "surface.h"

/* Recursive.source() */
Str kernel_recursive_source(void);
/* Recursive.validate(checked): Result<Error.T, Unit> */
Result kernel_recursive_validate(Heap *heap, const ElabChecked *checked);

#endif
