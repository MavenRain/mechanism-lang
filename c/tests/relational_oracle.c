/* Port of tests/relational_oracle.bend (47 lines): the refusal oracle cases of
 * relational_negatives (distinct_from and oracle_valid over hand-built pins). The live oracle is
 * the relational-oracle capture (manifest checks: the seed alpha and the non-ASCII seed).
 * Usage: one oracle seed. */
#include "driver.h"
#include "relational_negatives.h"

typedef struct {
  const char *label;
  bool valid;
} Case;

/* require(label, valid) */
static int require(Heap *heap, Case item) {
  Str message = str_empty();
  if (item.valid) return 0;
  message = heap_str_cat(heap, str_lit("RELATIONAL-ORACLE-FAIL "), str_lit(item.label));
  if (heap->oom) return driver_host_failure();
  return driver_die_text(1, message);
}

/* The Bend list literal [first] or [first, second] (second NULL for one row). */
static Vec pins(Heap *heap, Pin *first, Pin *second) {
  Vec list = vec_new(heap->arena);
  if (!vec_push(&list, first)) heap->oom = true;
  if (second != NULL && !vec_push(&list, second)) heap->oom = true;
  return list;
}

/* suite(seed) */
static int suite(Heap *heap, Str seed) {
  Str prefix = heap_str_cat(heap, str_lit("quantity: "), seed);
  Pin pin = {str_lit("self"), str_empty(), prefix, str_lit("0123456789abcdef0123456789abcdef")};
  Pin other = {str_lit("other"), str_empty(), str_lit("mismatch: unrelated"), str_lit("1123456789abcdef0123456789abcdef")};
  Pin duplicate = {str_lit("other"), str_empty(), prefix, str_lit("0123456789abcdef0123456789abcdef")};
  Pin longer = {str_lit("other"), str_empty(), heap_str_cat(heap, prefix, str_lit(" suffix")), str_empty()};
  Pin shorter = {str_lit("other"), str_empty(), str_lit("quantity:"), str_empty()};
  Pin short_pin = {str_lit("short"), str_empty(), str_lit("mismatch:"), str_empty()};
  Pin bad_digest = {str_lit("bad"), str_empty(), prefix, str_lit("0123456789abcdef0123456789abcde")};
  Vec none = vec_new(heap->arena);
  Vec self_list = pins(heap, &pin, NULL);
  Vec pin_other = pins(heap, &pin, &other);
  Vec duplicate_list = pins(heap, &duplicate, NULL);
  Vec longer_list = pins(heap, &longer, NULL);
  Vec shorter_list = pins(heap, &shorter, NULL);
  Vec pin_duplicate = pins(heap, &pin, &duplicate);
  Vec short_list = pins(heap, &short_pin, NULL);
  Vec pin_longer = pins(heap, &pin, &longer);
  Vec bad_list = pins(heap, &bad_digest, NULL);
  Policy distinct_prefix = negatives_prefix(9, true, false);
  Policy plain_prefix = negatives_prefix(9, false, false);
  Policy common_prefix = negatives_prefix(9, false, true);
  Policy distinct_digest = negatives_digest(9, 128, &none, true, false);
  Policy plain_digest = negatives_digest(9, 128, &none, false, false);
  if (heap->oom) return driver_host_failure();
  Case cases[] = {
    {"self prefix", negatives_distinct_from(&pin, &self_list, false)},
    {"different prefix", negatives_distinct_from(&pin, &pin_other, false)},
    {"duplicate prefix", !negatives_distinct_from(&pin, &duplicate_list, false)},
    {"prefix overlap", !negatives_distinct_from(&pin, &longer_list, false)},
    {"prefix direction", negatives_distinct_from(&pin, &shorter_list, false)},
    {"self digest", negatives_distinct_from(&pin, &self_list, true)},
    {"different digest", negatives_distinct_from(&pin, &pin_other, true)},
    {"duplicate digest", !negatives_distinct_from(&pin, &duplicate_list, true)},
    {"valid prefix oracle", negatives_oracle_valid(&pin, &pin_other, distinct_prefix)},
    {"duplicate prefix oracle", !negatives_oracle_valid(&pin, &pin_duplicate, distinct_prefix)},
    {"distinct exemption", negatives_oracle_valid(&pin, &pin_duplicate, plain_prefix)},
    {"strict minimum", !negatives_oracle_valid(&short_pin, &short_list, plain_prefix)},
    {"common prefix refused", !negatives_oracle_valid(&pin, &pin_longer, common_prefix)},
    {"different common prefix", negatives_oracle_valid(&pin, &pin_other, common_prefix)},
    {"valid digest oracle", negatives_oracle_valid(&pin, &pin_other, distinct_digest)},
    {"digest length", !negatives_oracle_valid(&bad_digest, &bad_list, plain_digest)},
    {"duplicate digest oracle", !negatives_oracle_valid(&pin, &pin_duplicate, distinct_digest)},
    {"digest distinct exemption", negatives_oracle_valid(&pin, &pin_duplicate, plain_digest)},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    int code = require(heap, cases[i]);
    if (code != 0) return code;
  }
  return driver_print(str_lit("RELATIONAL-ORACLE-OK cases=18"));
}

/* arguments(args) */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return suite(heap, str_lit(argv[1]));
  return driver_die(2, "expected one oracle seed");
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
