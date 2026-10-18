#include "surface.h"

Comp probe(Heap *heap) {
  Vec empty = vec_new(heap->arena);
#ifdef BOUNDARY_SIGNED
  return family_compose_comp(heap, global_empty(), family_empty(), big_neg(big_one()),
                             str_lit("Boundary"), &empty, &empty);
#else
  return family_compose_comp(heap, global_empty(), family_empty(), heap_nat_of_u64(heap, 1),
                             str_lit("Boundary"), &empty, &empty);
#endif
}
