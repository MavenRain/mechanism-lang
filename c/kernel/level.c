/* Port of bend2/kernel/level.bend: the universe level API over level_var and level_eq. */
#include "kernel.h"

const LevelVar *level_zero(void) { return level_var_lzero(); }

const LevelVar *level_one(Heap *heap) { return level_var_lsucc(heap, big_one(), level_var_lzero()); }

const LevelVar *level_succ(Heap *heap, const LevelVar *level) { return level_var_offset(heap, big_one(), level); }

const LevelVar *level_max(Heap *heap, const LevelVar *left, const LevelVar *right) {
  return level_var_lmax(heap, left, right);
}

const LevelVar *level_imax(Heap *heap, const LevelVar *left, const LevelVar *right) {
  return level_var_limax(heap, left, right);
}

const LevelVar *level_of_u32(Heap *heap, uint32_t value) {
  return level_var_offset(heap, big_of_u32(heap, value), level_var_lzero());
}

const LevelVar *level_var(Heap *heap, Nat index) { return level_var_lvar(heap, index); }

bool level_seen_var(const NatList *seen, Nat index) {
  const NatList *cursor = seen;
  while (cursor != NULL) {
    if (nat_cmp(cursor->head, index) == 0) return true;
    cursor = cursor->tail;
  }
  return false;
}

/* distinct_vars_from over the list tail starting at position `from`. */
static bool distinct_vars_at(Heap *heap, const Vec *levels, size_t from, const NatList *seen) {
  if (from >= levels->len) return true;
  VecGet got = vec_get(levels, from);
  if (got.status == VEC_GET_OUT_OF_RANGE) return false;
  const LevelVar *level = got.value;
  switch (level->kind) {
  case LEVEL_VAR_VAR:
    if (level_seen_var(seen, level->index)) return false;
    return distinct_vars_at(heap, levels, from + 1, level_eq_cons(heap, level->index, seen));
  case LEVEL_VAR_ZERO:
    return false;
  case LEVEL_VAR_SUCC:
    return false;
  case LEVEL_VAR_MAX:
    return false;
  case LEVEL_VAR_IMAX:
    return false;
  }
  return false;
}

bool level_distinct_vars_from(Heap *heap, const Vec *levels, const NatList *seen) {
  return distinct_vars_at(heap, levels, 0, seen);
}

/* True when every level is a variable and no variable repeats: the substitution only renames
 * variables. */
bool level_distinct_vars(Heap *heap, const Vec *levels) { return level_distinct_vars_from(heap, levels, NULL); }

bool level_in_scope(Nat arity, const LevelVar *level) { return level_var_in_scope(level, arity); }

MaybeLevelVar level_subst(Heap *heap, const Vec *args, const LevelVar *level) {
  return level_var_subst(heap, args, level);
}

bool level_equal(Heap *heap, const LevelVar *left, const LevelVar *right) {
  return level_eq_equal(heap, left, right);
}

bool level_le(Heap *heap, const LevelVar *left, const LevelVar *right) { return level_eq_le(heap, left, right); }

Result level_equal_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right) {
  return level_eq_equal_budget(heap, budget, left, right);
}

Result level_le_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right) {
  return level_eq_le_budget(heap, budget, left, right);
}

bool level_always_zero(const LevelVar *level) { return level_eq_always_zero(level); }

bool level_always_positive(const LevelVar *level) { return level_eq_always_positive(level); }

static Str cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }

static Str render_binary(Heap *heap, Str head, const LevelVar *left, const LevelVar *right) {
  Str inner = cat3(heap, level_render(heap, left), str_lit(", "), level_render(heap, right));
  return cat3(heap, head, inner, str_lit(")"));
}

Str level_render(Heap *heap, const LevelVar *level) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return str_lit("0");
  case LEVEL_VAR_SUCC: {
    Str inner = cat3(heap, level_render(heap, level->base), str_lit(" + "), big_to_string(heap, level->amount));
    return cat3(heap, str_lit("("), inner, str_lit(")"));
  }
  case LEVEL_VAR_MAX:
    return render_binary(heap, str_lit("max("), level->left, level->right);
  case LEVEL_VAR_IMAX:
    return render_binary(heap, str_lit("imax("), level->left, level->right);
  case LEVEL_VAR_VAR:
    return heap_str_cat(heap, str_lit("u"), heap_nat_show(heap, level->index));
  }
  return str_empty();
}

Str level_show_closed(Heap *heap, MaybeBig closed, const LevelVar *level) {
  if (closed.some) return big_to_string(heap, closed.value);
  return level_render(heap, level);
}

Str level_to_string(Heap *heap, const LevelVar *level) {
  return level_show_closed(heap, level_eq_closed_value(heap, level), level);
}
