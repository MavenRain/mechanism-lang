#include "surface.h"

Result probe(Heap *heap) {
  Vec empty = vec_new(heap->arena);
#ifdef BOUNDARY_SIGNED
  return family_compose(heap, global_empty(), family_empty(), budget_unlimited(), big_neg(big_one()),
                        str_lit("Boundary"), &empty, &empty);
#else
  return family_compose(heap, global_empty(), family_empty(), budget_unlimited(), nat_zero(),
                        str_lit("Boundary"), &empty, &empty);
#endif
}
