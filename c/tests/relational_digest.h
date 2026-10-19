/* Port of tests/relational_digest.bend + relational_digest.c: the MD5 of the UTF-8 byte
 * prefix of a message (limit 0 = the whole message), as 32 lowercase hex digits. */
#ifndef MECH_RELATIONAL_DIGEST_H
#define MECH_RELATIONAL_DIGEST_H

#include "relational_common.h"

Str relational_digest(Heap *heap, Str text, uint32_t limit);

#endif
