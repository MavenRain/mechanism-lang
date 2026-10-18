#include "kernel.h"

Result probe(Heap *heap) {
#ifdef BOUNDARY_SIGNED
  return check_check_scheme(heap, global_initial(heap), budget_unlimited(), big_neg(big_one()), (Decl){0});
#else
  return check_check_scheme(heap, global_initial(heap), budget_unlimited(), nat_zero(), (Decl){0});
#endif
}
