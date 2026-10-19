/* Port of kernel/quantity.bend: usage quantities and the per level usage intervals of
 * linear checking. A List<Common.Pair<Nat, Interval>> is a Vec of UsageRow pointers in list
 * order; a List<Nat> is a Vec of Nat pointers. Same definitions in the same order. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (item == NULL || !vec_push(xs, item)) heap->oom = true;
}

static UsageRow *row_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return (UsageRow *)got.value;
  return NULL;
}

static Nat *key_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status == VEC_GET_OK) return (Nat *)got.value;
  return NULL;
}

static UsageRow *row_make(Heap *heap, Nat level, Interval value) {
  UsageRow *row = heap_alloc(heap, sizeof(UsageRow));
  if (row == NULL) return NULL;
  row->level = level;
  row->value = value;
  return row;
}

static Interval interval_make(Quantity lo, Quantity hi) {
  Interval value = {lo, hi};
  return value;
}

static MaybeUsageRows rows_none(Heap *heap) {
  MaybeUsageRows value = {false, vec_new(heap->arena)};
  return value;
}

static MaybeUsageRows rows_some(Vec rows) {
  MaybeUsageRows value = {true, rows};
  return value;
}

static Usage usage_make(MaybeUsageRows paths, Vec reads) {
  Usage usage = {paths, reads};
  return usage;
}

uint32_t quantity_rank(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return 0;
    case QUANTITY_ONE: return 1;
    case QUANTITY_MANY: return 2;
  }
  return 0;
}

bool quantity_equal(Quantity a, Quantity b) { return quantity_rank(a) == quantity_rank(b); }

Str quantity_to_string(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return str_lit("0");
    case QUANTITY_ONE: return str_lit("1");
    case QUANTITY_MANY: return str_lit("w");
  }
  return str_lit("0");
}

Quantity quantity_runtime(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return QUANTITY_ZERO;
    case QUANTITY_ONE: return QUANTITY_ONE;
    case QUANTITY_MANY: return QUANTITY_ONE;
  }
  return q;
}

Quantity quantity_mul(Quantity a, Quantity b) {
  switch (a) {
    case QUANTITY_ZERO: return QUANTITY_ZERO;
    case QUANTITY_ONE: return b;
    case QUANTITY_MANY:
      switch (b) {
        case QUANTITY_ZERO: return QUANTITY_ZERO;
        case QUANTITY_ONE: return QUANTITY_MANY;
        case QUANTITY_MANY: return QUANTITY_MANY;
      }
      return QUANTITY_MANY;
  }
  return QUANTITY_ZERO;
}

Quantity quantity_add(Quantity a, Quantity b) {
  switch (a) {
    case QUANTITY_ZERO: return b;
    case QUANTITY_ONE:
      switch (b) {
        case QUANTITY_ZERO: return QUANTITY_ONE;
        case QUANTITY_ONE: return QUANTITY_MANY;
        case QUANTITY_MANY: return QUANTITY_MANY;
      }
      return QUANTITY_MANY;
    case QUANTITY_MANY:
      switch (b) {
        case QUANTITY_ZERO: return QUANTITY_MANY;
        case QUANTITY_ONE: return QUANTITY_MANY;
        case QUANTITY_MANY: return QUANTITY_MANY;
      }
      return QUANTITY_MANY;
  }
  return b;
}

Quantity quantity_minimum(Quantity a, Quantity b) {
  return quantity_rank(a) <= quantity_rank(b) ? a : b;
}

Quantity quantity_maximum(Quantity a, Quantity b) {
  return quantity_rank(a) >= quantity_rank(b) ? a : b;
}

Usage quantity_empty(Heap *heap) {
  return usage_make(rows_some(vec_new(heap->arena)), vec_new(heap->arena));
}

Usage quantity_unreachable(Heap *heap) { return usage_make(rows_none(heap), vec_new(heap->arena)); }

Interval quantity_interval(const Vec *xs, Nat level) {
  for (size_t i = 0; i < xs->len; i++) {
    UsageRow *row = row_at(xs, i);
    if (row != NULL && nat_cmp(row->level, level) == 0) return row->value;
  }
  return interval_make(QUANTITY_ZERO, QUANTITY_ZERO);
}

Vec quantity_occurrence_reads(Heap *heap, Nat level, Quantity q) {
  Vec reads = vec_new(heap->arena);
  push(heap, &reads, row_make(heap, level, interval_make(q, q)));
  return reads;
}

Usage quantity_occurrence(Heap *heap, Nat level, Quantity q) {
  Vec reads = quantity_occurrence_reads(heap, level, q);
  return usage_make(rows_some(reads), reads);
}

Quantity quantity_combine_mark(uint32_t mode, Quantity a, Quantity b) {
  if (mode == 0) return quantity_add(a, b);
  if (mode == 1) return quantity_minimum(a, b);
  if (mode == 2) return quantity_maximum(a, b);
  return a;
}

Interval quantity_combine_interval(Interval a, Interval b, uint32_t lo_mode, uint32_t hi_mode) {
  return interval_make(quantity_combine_mark(lo_mode, a.lo, b.lo), quantity_combine_mark(hi_mode, a.hi, b.hi));
}

Vec quantity_keys(Heap *heap, const Vec *xs) {
  Vec keys = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    UsageRow *row = row_at(xs, i);
    if (row != NULL) push(heap, &keys, &row->level);
  }
  return keys;
}

bool quantity_contains(const Vec *xs, Nat key) {
  for (size_t i = 0; i < xs->len; i++) {
    Nat *x = key_at(xs, i);
    if (x != NULL && nat_cmp(*x, key) == 0) return true;
  }
  return false;
}

/* The Bend fold prepends each new key to seen, so the result is the new keys in reverse order
 * of first appearance followed by the initial seen list. */
Vec quantity_unique(Heap *heap, const Vec *xs, Vec seen) {
  Vec fresh = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    Nat *x = key_at(xs, i);
    if (x != NULL && !quantity_contains(&seen, *x) && !quantity_contains(&fresh, *x)) push(heap, &fresh, x);
  }
  Vec reversed = vec_reverse(heap->arena, &fresh);
  Vec result = vec_concat(heap->arena, &reversed, &seen);
  if (result.len != fresh.len + seen.len) heap->oom = true;
  return result;
}

Vec quantity_combine_rows(Heap *heap, const Vec *rows, const Vec *a, const Vec *b, uint32_t lo_mode,
                          uint32_t hi_mode) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    Nat *key = key_at(rows, i);
    if (key == NULL) continue;
    Interval value =
        quantity_combine_interval(quantity_interval(a, *key), quantity_interval(b, *key), lo_mode, hi_mode);
    push(heap, &out, row_make(heap, *key, value));
  }
  return out;
}

Vec quantity_combine(Heap *heap, const Vec *a, const Vec *b, uint32_t lo_mode, uint32_t hi_mode) {
  Vec keys_a = quantity_keys(heap, a);
  Vec keys_b = quantity_keys(heap, b);
  Vec all = vec_concat(heap->arena, &keys_a, &keys_b);
  if (all.len != keys_a.len + keys_b.len) heap->oom = true;
  Vec rows = quantity_unique(heap, &all, vec_new(heap->arena));
  return quantity_combine_rows(heap, &rows, a, b, lo_mode, hi_mode);
}

MaybeUsageRows quantity_sequence_paths(Heap *heap, MaybeUsageRows a, MaybeUsageRows b) {
  if (!a.some) return rows_none(heap);
  if (!b.some) return rows_none(heap);
  return rows_some(quantity_combine(heap, &a.rows, &b.rows, 0, 0));
}

Usage quantity_sequence(Heap *heap, Usage a, Usage b) {
  return usage_make(quantity_sequence_paths(heap, a.paths, b.paths), quantity_combine(heap, &a.reads, &b.reads, 0, 0));
}

MaybeUsageRows quantity_alternative_paths(Heap *heap, MaybeUsageRows a, MaybeUsageRows b) {
  if (!a.some) return b;
  if (!b.some) return a;
  return rows_some(quantity_combine(heap, &a.rows, &b.rows, 1, 2));
}

Usage quantity_alternative(Heap *heap, Usage a, Usage b) {
  return usage_make(quantity_alternative_paths(heap, a.paths, b.paths),
                    quantity_combine(heap, &a.reads, &b.reads, 2, 2));
}

Vec quantity_scale_map(Heap *heap, const Vec *xs, Quantity lo, Quantity hi) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    UsageRow *row = row_at(xs, i);
    if (row == NULL) continue;
    Interval value = interval_make(quantity_mul(lo, row->value.lo), quantity_mul(hi, row->value.hi));
    push(heap, &out, row_make(heap, row->level, value));
  }
  return out;
}

MaybeUsageRows quantity_scale_paths(Heap *heap, MaybeUsageRows paths, Quantity lo, Quantity hi) {
  if (!paths.some) return rows_none(heap);
  return rows_some(quantity_scale_map(heap, &paths.rows, lo, hi));
}

Usage quantity_scale_range(Heap *heap, Interval bounds, Usage usage) {
  return usage_make(quantity_scale_paths(heap, usage.paths, bounds.lo, bounds.hi),
                    quantity_scale_map(heap, &usage.reads, bounds.lo, bounds.hi));
}

Usage quantity_scale(Heap *heap, Quantity q, Usage usage) {
  switch (q) {
    case QUANTITY_ZERO: return quantity_empty(heap);
    case QUANTITY_ONE: return usage;
    case QUANTITY_MANY: return quantity_scale_range(heap, interval_make(QUANTITY_MANY, QUANTITY_MANY), usage);
  }
  return usage;
}

Interval quantity_get_paths(MaybeUsageRows paths, Interval reads, Nat level) {
  if (!paths.some) return interval_make(QUANTITY_ONE, quantity_maximum(QUANTITY_ONE, reads.hi));
  return quantity_interval(&paths.rows, level);
}

Interval quantity_get(Usage usage, Nat level) {
  return quantity_get_paths(usage.paths, quantity_interval(&usage.reads, level), level);
}

Vec quantity_remove_map(Heap *heap, const Vec *xs, Nat level) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    UsageRow *row = row_at(xs, i);
    if (row != NULL && nat_cmp(row->level, level) != 0) push(heap, &out, row);
  }
  return out;
}

MaybeUsageRows quantity_remove_paths(Heap *heap, MaybeUsageRows paths, Nat level) {
  if (!paths.some) return rows_none(heap);
  return rows_some(quantity_remove_map(heap, &paths.rows, level));
}

Usage quantity_remove(Heap *heap, Usage usage, Nat level) {
  return usage_make(quantity_remove_paths(heap, usage.paths, level), quantity_remove_map(heap, &usage.reads, level));
}

bool quantity_interval_once(Interval value) {
  return quantity_equal(value.lo, QUANTITY_ONE) && quantity_equal(value.hi, QUANTITY_ONE);
}

bool quantity_exactly_once(Nat level, Usage usage) { return quantity_interval_once(quantity_get(usage, level)); }

bool quantity_interval_used(Interval value) { return !quantity_equal(value.hi, QUANTITY_ZERO); }

bool quantity_used(Nat level, Usage usage) {
  return quantity_interval_used(quantity_interval(&usage.reads, level));
}

bool quantity_interval_affine(Interval value) { return !quantity_equal(value.hi, QUANTITY_MANY); }

bool quantity_at_most_once(Nat level, Usage usage) {
  return quantity_interval_affine(quantity_get(usage, level));
}

Vec quantity_capture_paths(Heap *heap, MaybeUsageRows paths, const Vec *reads) {
  if (!paths.some) return *reads;
  return quantity_combine(heap, &paths.rows, reads, 3, 2);
}

Usage quantity_captures(Heap *heap, Usage usage) {
  return usage_make(rows_some(quantity_capture_paths(heap, usage.paths, &usage.reads)), usage.reads);
}
