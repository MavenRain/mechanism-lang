#include "kernel.h"

bool probe(void) {
#ifdef BOUNDARY_SIGNED
  return level_in_scope(big_neg(big_one()), level_zero());
#else
  return level_in_scope(nat_zero(), level_zero());
#endif
}
