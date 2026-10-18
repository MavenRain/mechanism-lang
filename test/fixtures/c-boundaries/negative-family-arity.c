#include "surface.h"

Result probe(Heap *heap) {
  Vec empty = vec_new(heap->arena);
#ifdef BOUNDARY_SIGNED
  return family_declare(heap, global_empty(), family_empty(), budget_unlimited(), big_neg(big_one()),
                        (FamilyDecl){0}, empty, &empty);
#else
  return family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_zero(),
                        (FamilyDecl){0}, empty, &empty);
#endif
}
