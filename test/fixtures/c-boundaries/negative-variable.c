#include "kernel.h"

const LevelVar *probe(Heap *heap) {
#ifdef BOUNDARY_SIGNED
  return level_var(heap, big_neg(big_one()));
#else
  return level_var(heap, nat_zero());
#endif
}
