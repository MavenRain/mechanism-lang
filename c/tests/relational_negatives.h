/* Port of bend2/tests/relational_negatives.bend: the refusal oracle shared by the prelude law drivers.
 * Each negative pin is test/neg/<directory>/<name>.mech (the source), .err (the expected refusal prefix)
 * and, under a Digest policy, .digest (the MD5 of the refusal text). A library module (no main). */
#ifndef MECH_RELATIONAL_NEGATIVES_H
#define MECH_RELATIONAL_NEGATIVES_H

#include "relational_common.h"

/* Pin{name, source, prefix, digest} */
typedef struct {
  Str name;
  Str source;
  Str prefix;
  Str digest;
} Pin;

/* Policy = Prefix{minimum, distinct, common} | Digest{minimum, limit, full, distinct, nominal} */
typedef enum { POLICY_PREFIX, POLICY_DIGEST } PolicyKind;

typedef struct {
  PolicyKind kind;
  size_t minimum;  /* both: the refusal prefix must be longer than this many characters */
  bool distinct;   /* both: no two pins share a prefix (Prefix) or a digest (Digest) */
  bool common;     /* Prefix: no prefix is a prefix of every other pin */
  uint32_t limit;  /* Digest: byte prefix of the message that is hashed (0 = whole message) */
  const Vec *full; /* Digest: names (Vec of Str *) hashed in full regardless of limit */
  bool nominal;    /* Digest: the nominal-target refusal must mention SeparateTarget */
} Policy;

Policy negatives_prefix(size_t minimum, bool distinct, bool common);
Policy negatives_digest(size_t minimum, uint32_t limit, const Vec *full, bool distinct, bool nominal);
/* prefix_of_all(prefix, others), distinct_from(pin, others, digest_mode) and oracle_valid(pin, all,
 * policy), exported for the relational_oracle drivers (others and all: Vec of Pin *). */
bool negatives_prefix_of_all(Str prefix, const Vec *others);
bool negatives_distinct_from(const Pin *pin, const Vec *others, bool digest_mode);
bool negatives_oracle_valid(const Pin *pin, const Vec *all, Policy policy);
Task negatives_message(Heap *heap, Result result, Str name, Str *text);
Task negatives_run(Heap *heap, Str root, Str directory, const Vec *names, Globals globals, Policy policy);

#endif
