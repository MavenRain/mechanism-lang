/* Port of tests/kernel_quantity_native_probe.bend. The Bend driver has no JS build, no
 * _bend2 wrapper and no harness call, so there is no oracle record: c-test runs it with level 0
 * and compares the output with c/tests/kernel_quantity_native_probe.expected. Usage: one
 * natural level argument. */
#include "driver.h"

typedef struct {
  const char *label;
  bool value;
} Line;

static int show(Heap *heap, Line line) {
  Str text = heap_str_cat(heap, heap_str_cat(heap, str_lit(line.label), str_lit(": ")),
                          str_lit(line.value ? "true" : "false"));
  if (heap->oom) return driver_host_failure();
  return driver_print(text);
}

static bool close_valid(Nat level, Nat outer, Quantity mode, Quantity q, bool affine, Usage uses) {
  bool checked = affine ? quantity_at_most_once(level, uses) : quantity_exactly_once(level, uses);
  return nat_cmp(level, outer) < 0 || quantity_equal(mode, QUANTITY_ZERO) || !quantity_equal(q, QUANTITY_ONE) || checked;
}

static bool affine_pick(bool affine, Nat level, Usage uses) {
  return affine ? quantity_at_most_once(level, uses) : quantity_exactly_once(level, uses);
}

static bool inner(Quantity q, bool affine, Nat level, Usage uses) {
  return !quantity_equal(q, QUANTITY_ONE) || affine_pick(affine, level, uses);
}

static bool middle(Quantity mode, Quantity q, bool affine, Nat level, Usage uses) {
  return quantity_equal(mode, QUANTITY_ZERO) || inner(q, affine, level, uses);
}

static bool outer(Nat level, Nat bound, Quantity mode, Quantity q, bool affine, Usage uses) {
  return nat_cmp(level, bound) < 0 || middle(mode, q, affine, level, uses);
}

static int run(Heap *heap, Nat n) {
  bool same = nat_cmp(n, n) == 0;
  Usage occ = quantity_occurrence(heap, n, QUANTITY_ONE);
  Usage empty = quantity_empty(heap);
  Line lines[] = {
      {"or FT", false || same},
      {"or TF", same || false},
      {"or FF refused", !(false || false)},
      {"or TT", same || same},
      {"pick F", same},
      {"nested OR", false || (false || (false || same))},
      {"not T or T", !same || same},
      {"not T or false refused", !(!same || false)},
      {"not Qequal or T", !quantity_equal(QUANTITY_ONE, QUANTITY_ONE) || same},
      {"qone equality", quantity_equal(QUANTITY_ONE, QUANTITY_ONE)},
      {"nat zero equality", same},
      {"once singleton", quantity_exactly_once(n, occ)},
      {"once sequence left", quantity_exactly_once(n, quantity_sequence(heap, empty, occ))},
      {"once sequence right", quantity_exactly_once(n, quantity_sequence(heap, occ, empty))},
      {"once scale", quantity_exactly_once(n, quantity_scale(heap, QUANTITY_ONE, occ))},
      {"nested close valid", close_valid(n, n, QUANTITY_ONE, QUANTITY_ONE, false, occ)},
      {"at most singleton", quantity_at_most_once(n, occ)},
      {"affine pick false", affine_pick(false, n, occ)},
      {"affine pick true", affine_pick(true, n, occ)},
      {"inner", inner(QUANTITY_ONE, false, n, occ)},
      {"middle", middle(QUANTITY_ONE, QUANTITY_ONE, false, n, occ)},
      {"outer", outer(n, n, QUANTITY_ONE, QUANTITY_ONE, false, occ)},
      {"twice refused", !quantity_exactly_once(n, quantity_sequence(heap, occ, occ))},
      {"zero refused", !quantity_exactly_once(n, empty)},
  };
  if (heap->oom) return driver_host_failure();
  for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
    int code = show(heap, lines[i]);
    if (code != 0) return code;
  }
  return 0;
}

static int parsed(Heap *heap, NatParse value) {
  switch (value.status) {
    case NAT_PARSE_OK: return run(heap, value.value);
    case NAT_PARSE_EMPTY: return driver_die(2, "invalid natural level");
    case NAT_PARSE_BAD_DIGIT: return driver_die(2, "invalid natural level");
    case NAT_PARSE_OOM: return driver_host_failure();
  }
  return driver_host_failure();
}

static int arguments(Heap *heap, int argc, char **argv) {
  if (argc != 2) return driver_die(2, "expected a natural level");
  return parsed(heap, nat_parse_decimal(heap->arena, str_lit(argv[1])));
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
