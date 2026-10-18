#include "kernel.h"

const LevelVar *probe(Heap *heap) {
#ifdef BOUNDARY_SIGNED
  return level_of_u32(heap, big_neg(big_one()));
#else
  return level_of_u32(heap, 0u);
#endif
}
