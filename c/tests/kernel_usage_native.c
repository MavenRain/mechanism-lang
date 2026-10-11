/* Port of bend2/tests/kernel_usage_native.bend (oracle label kernel-usage-native). */
#include "driver.h"

/* Regression: Bend 2.0.25 native C miscomputed the former nested Bool.or/pick
 * binder predicate for a dynamic linear identity. Keep close_valid's explicit
 * cases unless this matrix and the real linear checker pass on both backends. */

static Quantity quantity(uint64_t value) {
  if (value == 0) return QUANTITY_ZERO;
  if (value == 1) return QUANTITY_ONE;
  return QUANTITY_MANY;
}

static Usage usage(Heap *heap, uint64_t kind, Nat level) {
  if (kind == 0) return quantity_empty(heap);
  if (kind == 1) return quantity_occurrence(heap, level, QUANTITY_ONE);
  if (kind == 2) {
    return quantity_sequence(heap, quantity_occurrence(heap, level, QUANTITY_ONE),
                             quantity_occurrence(heap, level, QUANTITY_ONE));
  }
  if (kind == 3) return quantity_alternative(heap, quantity_empty(heap), quantity_occurrence(heap, level, QUANTITY_ONE));
  if (kind == 4) {
    return quantity_alternative(heap, quantity_occurrence(heap, level, QUANTITY_ONE),
                                quantity_occurrence(heap, level, QUANTITY_MANY));
  }
  return quantity_unreachable(heap);
}

static bool expected_use(bool affine, uint64_t kind) {
  if (kind == 1) return true;
  if (kind == 5) return true;
  if (affine && kind == 0) return true;
  if (affine && kind == 3) return true;
  return false;
}

static bool expected(bool outer, uint64_t mode, uint64_t binder, bool affine, uint64_t kind) {
  if (outer) return true;
  if (mode == 0) return true;
  if (binder == 0) return true;
  if (binder == 2) return true;
  return expected_use(affine, kind);
}

static bool same(bool a, bool b) { return a == b; }

static bool trial(Heap *heap, uint64_t index, Nat level) {
  uint64_t kind = index % 6;
  bool affine = (index / 6) % 2 == 1;
  uint64_t binder = (index / 12) % 3;
  uint64_t mode = (index / 36) % 3;
  bool outer = index / 108 == 1;
  bool valid = check_engine_close_valid(outer, quantity(mode), quantity(binder), affine, level, usage(heap, kind, level));
  return same(valid, expected(outer, mode, binder, affine, kind));
}

/* Maybe<Nat>: the first failing case index, scanning from the top. */
typedef struct {
  bool some;
  uint64_t index;
} Selected;

static Selected walk(Heap *heap, uint64_t left, Nat level);

static Selected selected(Heap *heap, bool valid, uint64_t index, Nat level) {
  if (valid) return walk(heap, index, level);
  return (Selected){true, index};
}

static Selected walk(Heap *heap, uint64_t left, Nat level) {
  if (left == 0) return (Selected){false, 0};
  uint64_t index = left - 1;
  return selected(heap, trial(heap, index, level), index, level);
}

static int finish(Heap *heap, Selected value) {
  if (!value.some) return driver_print(str_lit("KERNEL-USAGE-NATIVE-OK cases=216"));
  Str shown = heap_nat_show(heap, heap_nat_of_u64(heap, value.index));
  char *message = str_cstr(heap->arena, str_cat(heap->arena, str_lit("usage matrix failed case "), shown));
  if (message == NULL || heap->oom) return driver_host_failure();
  return driver_die(1, message);
}

static int parsed(Heap *heap, NatParse value) {
  switch (value.status) {
    case NAT_PARSE_OK: {
      Selected found = walk(heap, 216, value.value);
      return heap->oom ? driver_host_failure() : finish(heap, found);
    }
    case NAT_PARSE_EMPTY:
    case NAT_PARSE_BAD_DIGIT:
      return driver_die(2, "invalid dynamic level");
    case NAT_PARSE_OOM:
      return driver_host_failure();
  }
  return driver_host_failure();
}

static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return parsed(heap, nat_parse_decimal(heap->arena, str_lit(argv[1])));
  return driver_die(2, "expected dynamic level");
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
