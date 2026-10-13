/* Shared view of bend2/tests/levels_regression.bend: levels_protocol.bend imports it as Algebra and
 * reads only Algebra.cases(). The driver object keeps its main; the library object (compiled with
 * MECH_DRIVER_LIBRARY) exports the case table alone. */
#ifndef MECH_LEVELS_REGRESSION_H
#define MECH_LEVELS_REGRESSION_H

#include "kernel.h"

typedef struct {
  const char *name;
  bool holds;
} LevelsCase;

enum { LEVELS_REGRESSION_CASE_COUNT = 17 };

/* cases(): the 17 algebra regression families in Bend order. */
void levels_regression_cases(Heap *h, LevelsCase *out);

#endif
