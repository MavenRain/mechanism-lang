/* Port of bend2/tests/relational_negatives.bend (113 lines): the refusal oracle of the prelude law drivers.
 * Definitions in the Bend order; Task(A) is Task {ok, message} with the payload in an out parameter. */
#include "relational_negatives.h"
#include "relational_digest.h"
#include "relational_text.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

static Str at_str(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return str_empty();
  return *(const Str *)got.value;
}

static const Pin *at_pin(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return NULL;
  return got.value;
}

/* Base Char.is_space: the space and the code points 9..13. */
static bool is_space(unsigned char c) { return c == 32 || (c >= 9 && c <= 13); }

/* Base String.trim = trim_end(trim_start(s)): drop Char.is_space at both ends. */
static Str trim(Str s) {
  size_t start = 0;
  size_t end = s.len;
  while (start < end && is_space((unsigned char)s.data[start])) start++;
  while (end > start && is_space((unsigned char)s.data[end - 1])) end--;
  return str_n(s.data + start, end - start);
}

/* Base String.length counts characters: every byte of the UTF-8 text except the continuation bytes. */
static size_t char_length(Str s) {
  size_t count = 0;
  for (size_t i = 0; i < s.len; i++) {
    if (((unsigned char)s.data[i] & 0xC0) != 0x80) count++;
  }
  return count;
}

Policy negatives_prefix(size_t minimum, bool distinct, bool common) {
  Policy policy = {POLICY_PREFIX, minimum, distinct, common, 0, NULL, false};
  return policy;
}

Policy negatives_digest(size_t minimum, uint32_t limit, const Vec *full, bool distinct, bool nominal) {
  Policy policy = {POLICY_DIGEST, minimum, distinct, false, limit, full, nominal};
  return policy;
}

/* needs_digest(policy) */
static bool needs_digest(Policy policy) {
  switch (policy.kind) {
    case POLICY_PREFIX: return false;
    case POLICY_DIGEST: return true;
  }
  return false;
}

/* fingerprint(needed, root, path): the trimmed .digest file, or "" when the policy has no digest. */
static Task fingerprint(Heap *heap, bool needed, Str root, Str path, Str *digest) {
  Str value = str_empty();
  if (!needed) {
    *digest = str_empty();
    return task_pure();
  }
  RELATIONAL_TRY(relational_read(heap, root, cat(heap, path, str_lit(".digest")), &value));
  *digest = trim(value);
  return task_pure();
}

/* load(names, root, directory, policy): one Pin per name, in the order of names. */
static Task load(Heap *heap, const Vec *names, Str root, Str directory, Policy policy, Vec *pins) {
  *pins = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    Str base = cat3(heap, directory, str_lit("/"), name);
    Str source = str_empty();
    Str expected = str_empty();
    Str digest = str_empty();
    Pin *pin = NULL;
    RELATIONAL_TRY(relational_read(heap, root, cat(heap, base, str_lit(".mech")), &source));
    RELATIONAL_TRY(relational_read(heap, root, cat(heap, base, str_lit(".err")), &expected));
    RELATIONAL_TRY(fingerprint(heap, needs_digest(policy), root, base, &digest));
    pin = heap_alloc(heap, sizeof *pin);
    if (pin == NULL) return task_fail(str_lit("out of memory"));
    pin->name = name;
    pin->source = source;
    pin->prefix = trim(expected);
    pin->digest = digest;
    if (!vec_push(pins, pin)) return task_fail(str_lit("out of memory"));
  }
  return task_pure();
}

/* prefix_of_all(prefix, others) */
bool negatives_prefix_of_all(Str prefix, const Vec *others) {
  for (size_t i = 0; i < others->len; i++) {
    const Pin *other = at_pin(others, i);
    if (other == NULL || !str_starts_with(other->prefix, prefix)) return false;
  }
  return true;
}

/* different_field(digest_mode, prefix, other_prefix, digest, other_digest) */
static bool different_field(bool digest_mode, Str prefix, Str other_prefix, Str digest, Str other_digest) {
  if (digest_mode) return !str_eq(digest, other_digest);
  return !str_starts_with(other_prefix, prefix);
}

/* distinct_row(same, digest_mode, prefix, other_prefix, digest, other_digest) */
static bool distinct_row(bool same, bool digest_mode, Str prefix, Str other_prefix, Str digest, Str other_digest) {
  if (same) return true;
  return different_field(digest_mode, prefix, other_prefix, digest, other_digest);
}

/* distinct_fields(name, prefix, digest, others, digest_mode): every row that is not the pin itself differs. */
static bool distinct_fields(Str name, Str prefix, Str digest, const Vec *others, bool digest_mode) {
  for (size_t i = 0; i < others->len; i++) {
    const Pin *other = at_pin(others, i);
    if (other == NULL) return false;
    if (!distinct_row(str_eq(name, other->name), digest_mode, prefix, other->prefix, digest, other->digest)) return false;
  }
  return true;
}

/* distinct_from(pin, others, digest_mode) */
bool negatives_distinct_from(const Pin *pin, const Vec *others, bool digest_mode) {
  return distinct_fields(pin->name, pin->prefix, pin->digest, others, digest_mode);
}

/* oracle_valid(pin, all, policy) */
bool negatives_oracle_valid(const Pin *pin, const Vec *all, Policy policy) {
  bool long_enough = char_length(pin->prefix) > policy.minimum;
  switch (policy.kind) {
    case POLICY_PREFIX:
      return long_enough && (!policy.distinct || negatives_distinct_from(pin, all, false)) &&
             (!policy.common || !negatives_prefix_of_all(pin->prefix, all));
    case POLICY_DIGEST:
      return long_enough && char_length(pin->digest) == 32 && (!policy.distinct || negatives_distinct_from(pin, all, true));
  }
  return false;
}

/* message(result, name): the refusal text, or the failure "<name>: expected refusal" when the check passed. */
static Task message(Heap *heap, Result result, Str name, Str *text) {
  switch (result.kind) {
    case RESULT_DONE: return task_fail(cat(heap, name, str_lit(": expected refusal")));
    case RESULT_FAIL:
      *text = error_to_string(heap, result.error);
      return task_pure();
  }
  return task_fail(name);
}

/* Neg.message for the drivers that record refusal texts (prelude_left_kan_laws --record-cocone-equality-oracles). */
Task negatives_message(Heap *heap, Result result, Str name, Str *text) { return message(heap, result, name, text); }

/* check_digest(policy, name, message, digest): the MD5 of the message (whole when the name is in full). */
static Task check_digest(Heap *heap, Policy policy, Str name, Str text, Str digest, bool *ok) {
  Str got = str_empty();
  switch (policy.kind) {
    case POLICY_PREFIX:
      *ok = true;
      return task_pure();
    case POLICY_DIGEST:
      got = relational_digest(heap, text, relational_contains(policy.full, name) ? 0 : policy.limit);
      *ok = str_eq(got, digest);
      return task_pure();
  }
  *ok = false;
  return task_pure();
}

/* nominal_ok(policy, name, message) */
static bool nominal_ok(Heap *heap, Policy policy, Str name, Str text) {
  Vec words = vec_new(heap->arena);
  switch (policy.kind) {
    case POLICY_PREFIX: return true;
    case POLICY_DIGEST:
      if (!(policy.nominal && str_eq(name, str_lit("nominal-target")))) return true;
      words = text_split(heap, text, ' ');
      return relational_contains(&words, str_lit("SeparateTarget"));
  }
  return false;
}

/* check_one(pin, all, globals, policy) */
static Task check_one(Heap *heap, const Pin *pin, const Vec *all, Globals globals, Policy policy) {
  Str actual = str_empty();
  bool hash_ok = false;
  RELATIONAL_TRY(relational_require(negatives_oracle_valid(pin, all, policy), cat(heap, pin->name, str_lit(": invalid refusal oracle"))));
  RELATIONAL_TRY(message(heap, elab_check_in(heap, globals, budget_unlimited(), pin->source), pin->name, &actual));
  RELATIONAL_TRY(check_digest(heap, policy, pin->name, actual, pin->digest, &hash_ok));
  return relational_require(str_starts_with(actual, pin->prefix) && hash_ok && nominal_ok(heap, policy, pin->name, actual),
                            cat3(heap, pin->name, str_lit(": wrong refusal: "), actual));
}

/* check_all(pins, all, globals, policy) */
static Task check_all(Heap *heap, const Vec *pins, const Vec *all, Globals globals, Policy policy) {
  for (size_t i = 0; i < pins->len; i++) {
    const Pin *pin = at_pin(pins, i);
    if (pin == NULL) return task_fail(str_lit("missing pin"));
    RELATIONAL_TRY(check_one(heap, pin, all, globals, policy));
  }
  return task_pure();
}

/* run(root, directory, names, globals, policy) */
Task negatives_run(Heap *heap, Str root, Str directory, const Vec *names, Globals globals, Policy policy) {
  Vec pins = vec_new(heap->arena);
  RELATIONAL_TRY(relational_negative_inventory(heap, root, directory, names));
  RELATIONAL_TRY(load(heap, names, root, directory, policy, &pins));
  return check_all(heap, &pins, &pins, globals, policy);
}
