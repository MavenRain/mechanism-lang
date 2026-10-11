#include "rt.h"

#include <string.h>

Vec vec_new(Arena *arena) {
  Vec vec;
  vec.arena = arena;
  vec.items = NULL;
  vec.len = 0;
  vec.cap = 0;
  return vec;
}

static bool vec_reserve(Vec *vec, size_t extra) {
  size_t need = vec->len + extra;
  if (need <= vec->cap) return true;
  size_t cap = vec->cap == 0 ? 8 : vec->cap;
  while (cap < need) cap *= 2;
  void **items = arena_alloc(vec->arena, cap * sizeof(void *));
  if (items == NULL) return false;
  if (vec->len > 0) memcpy(items, vec->items, vec->len * sizeof(void *));
  vec->items = items;
  vec->cap = cap;
  return true;
}

bool vec_push(Vec *vec, void *item) {
  if (!vec_reserve(vec, 1)) return false;
  vec->items[vec->len] = item;
  vec->len += 1;
  return true;
}

VecGet vec_get(const Vec *vec, size_t index) {
  VecGet out;
  if (index >= vec->len) {
    out.status = VEC_GET_OUT_OF_RANGE;
    out.value = NULL;
    return out;
  }
  out.status = VEC_GET_OK;
  out.value = vec->items[index];
  return out;
}

VecGet vec_last(const Vec *vec) {
  return vec->len == 0 ? vec_get(vec, 0) : vec_get(vec, vec->len - 1);
}

Vec vec_map(Arena *arena, const Vec *vec, void *(*f)(void *ctx, void *item), void *ctx) {
  Vec out = vec_new(arena);
  for (size_t i = 0; i < vec->len; i++) {
    if (!vec_push(&out, f(ctx, vec->items[i]))) return vec_new(arena);
  }
  return out;
}

Vec vec_filter(Arena *arena, const Vec *vec, bool (*keep)(void *ctx, void *item), void *ctx) {
  Vec out = vec_new(arena);
  for (size_t i = 0; i < vec->len; i++) {
    if (keep(ctx, vec->items[i]) && !vec_push(&out, vec->items[i])) return vec_new(arena);
  }
  return out;
}

void *vec_fold(const Vec *vec, void *init, void *(*f)(void *ctx, void *acc, void *item), void *ctx) {
  void *acc = init;
  for (size_t i = 0; i < vec->len; i++) acc = f(ctx, acc, vec->items[i]);
  return acc;
}

Vec vec_concat(Arena *arena, const Vec *a, const Vec *b) {
  Vec out = vec_new(arena);
  for (size_t i = 0; i < a->len; i++) {
    if (!vec_push(&out, a->items[i])) return vec_new(arena);
  }
  for (size_t i = 0; i < b->len; i++) {
    if (!vec_push(&out, b->items[i])) return vec_new(arena);
  }
  return out;
}

Vec vec_reverse(Arena *arena, const Vec *vec) {
  Vec out = vec_new(arena);
  for (size_t i = vec->len; i > 0; i--) {
    if (!vec_push(&out, vec->items[i - 1])) return vec_new(arena);
  }
  return out;
}
