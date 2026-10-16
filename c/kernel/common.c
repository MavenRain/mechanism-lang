/* Port of bend2/kernel/common.bend plus the heap helpers every kernel file shares. */
#include "kernel.h"
#include <string.h>

Heap heap_new(Arena *arena) {
  Heap heap;
  heap.arena = arena;
  heap.oom = false;
  memset(heap.small_blocks, 0, sizeof heap.small_blocks);
  heap.memo = NULL;
  heap.conv_scratch = false;
  return heap;
}

void *heap_alloc(Heap *heap, size_t size) {
  void *out = arena_alloc(heap->arena, size);
  if (out == NULL) heap->oom = true;
  return out;
}

/* Size class of a small block: 1 for 1..8 bytes, 2 for 9..16, and so on; 0 for an empty request. */
static size_t heap_small_class(size_t size) { return (size + 7) / 8; }

void *heap_alloc_small(Heap *heap, size_t size) {
  size_t cls = heap_small_class(size);
  if (cls == 0 || cls > HEAP_SMALL_CLASSES) return heap_alloc(heap, size);
  void *block = heap->small_blocks[cls - 1];
  if (block == NULL) return heap_alloc(heap, cls * 8);
  heap->small_blocks[cls - 1] = *(void **)block;
  memset(block, 0, cls * 8);
  return block;
}

void heap_release_small(Heap *heap, void *block, size_t size) {
  size_t cls = heap_small_class(size);
  if (block == NULL || cls == 0 || cls > HEAP_SMALL_CLASSES) return;
  *(void **)block = heap->small_blocks[cls - 1];
  heap->small_blocks[cls - 1] = block;
}

Vec heap_vec_concat(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_concat(heap->arena, a, b);
  if (out.len != a->len + b->len) heap->oom = true;
  return out;
}

Vec heap_vec_reverse(Heap *heap, const Vec *items) {
  Vec out = vec_reverse(heap->arena, items);
  if (out.len != items->len) heap->oom = true;
  return out;
}

Str heap_str_cat(Heap *heap, Str a, Str b) {
  Str out = str_cat(heap->arena, a, b);
  if (out.len != a.len + b.len) heap->oom = true;
  return out;
}

Str heap_nat_show(Heap *heap, Nat n) {
  Str out = nat_to_decimal(heap->arena, n);
  if (out.len == 0) {
    if (nat_is_zero(n)) return str_lit("0");
    heap->oom = true;
  }
  return out;
}

Nat heap_nat_of_u64(Heap *heap, uint64_t value) {
  if (value == 0) return nat_zero();
  Nat out = nat_of_u64(heap->arena, value);
  if (value != 0 && nat_is_zero(out)) heap->oom = true;
  return out;
}

Nat heap_nat_add(Heap *heap, Nat a, Nat b) {
  if (nat_is_zero(a)) return b;
  if (nat_is_zero(b)) return a;
  Nat out = nat_add(heap->arena, a, b);
  if (nat_is_zero(out)) heap->oom = true;
  return out;
}

Nat heap_nat_succ(Heap *heap, Nat n) {
  Nat out = nat_succ(heap->arena, n);
  /* Either allocation in nat_succ can fail, leaving zero or the unchanged input. */
  if (nat_cmp(out, n) <= 0) heap->oom = true;
  return out;
}

static char kernel_unit_box;

void *kernel_unit(void) { return &kernel_unit_box; }

static bool kernel_bool_boxes[2] = {false, true};

void *kernel_bool_box(bool value) { return &kernel_bool_boxes[value ? 1 : 0]; }

bool kernel_bool_unbox(const void *boxed) { return boxed == &kernel_bool_boxes[1]; }

/* def at(-A: Data, xs: List<&2, A>, index: Nat) -> Maybe<&2, A> */
VecGet common_at(const Vec *xs, Nat index) {
  NatToU64 narrowed = nat_to_u64(index);
  VecGet out;
  if (narrowed.status == NAT_TO_U64_OVERFLOW || narrowed.value > (uint64_t)SIZE_MAX) {
    out.status = VEC_GET_OUT_OF_RANGE;
    out.value = NULL;
    return out;
  }
  return vec_get(xs, (size_t)narrowed.value);
}
