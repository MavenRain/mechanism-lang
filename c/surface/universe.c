/* Port of bend2/surface/universe.bend: surface universe expressions retain their syntax; lowering
 * to kernel levels is separate. Same definitions in the same order. */
#include "surface.h"

Str surface_cat(Heap *heap, const Str *parts, size_t count) {
  StrBuf buf = strbuf_new(heap->arena);
  bool ok = true;
  for (size_t i = 0; i < count; i++) {
    if (parts[i].len > 0) ok = strbuf_push(&buf, parts[i]) && ok;
  }
  if (!ok) heap->oom = true;
  if (buf.len == 0) return str_empty();
  return strbuf_str(&buf);
}

static const Universe universe_placeholder = {UNIVERSE_NAT_LEVEL, {false, {NULL, 0}}, {NULL, 0}, NULL, NULL};

static Universe *universe_alloc(Heap *heap, UniverseKind kind) {
  Universe *node = heap_alloc(heap, sizeof(Universe));
  if (node) node->kind = kind;
  return node;
}

const Universe *universe_nat_level(Heap *heap, Big value) {
  Universe *node = universe_alloc(heap, UNIVERSE_NAT_LEVEL);
  if (!node) return &universe_placeholder;
  node->value = value;
  return node;
}

const Universe *universe_var_level(Heap *heap, Nat index) {
  Universe *node = universe_alloc(heap, UNIVERSE_VAR_LEVEL);
  if (!node) return &universe_placeholder;
  node->index = index;
  return node;
}

const Universe *universe_succ_level(Heap *heap, const Universe *value) {
  Universe *node = universe_alloc(heap, UNIVERSE_SUCC_LEVEL);
  if (!node) return &universe_placeholder;
  node->left = value;
  return node;
}

const Universe *universe_max_level(Heap *heap, const Universe *left, const Universe *right) {
  Universe *node = universe_alloc(heap, UNIVERSE_MAX_LEVEL);
  if (!node) return &universe_placeholder;
  node->left = left;
  node->right = right;
  return node;
}

const Universe *universe_imax_level(Heap *heap, const Universe *left, const Universe *right) {
  Universe *node = universe_alloc(heap, UNIVERSE_IMAX_LEVEL);
  if (!node) return &universe_placeholder;
  node->left = left;
  node->right = right;
  return node;
}

Str universe_text(Heap *heap, const Universe *level) {
  switch (level->kind) {
    case UNIVERSE_NAT_LEVEL:
      return big_to_string(heap, level->value);
    case UNIVERSE_VAR_LEVEL:
      return heap_str_cat(heap, str_lit("u"), heap_nat_show(heap, level->index));
    case UNIVERSE_SUCC_LEVEL:
      return SURFACE_CAT(heap, str_lit("(succ "), universe_text(heap, level->left), str_lit(")"));
    case UNIVERSE_MAX_LEVEL:
      return SURFACE_CAT(heap, str_lit("(max "), universe_text(heap, level->left), str_lit(" "),
                         universe_text(heap, level->right), str_lit(")"));
    case UNIVERSE_IMAX_LEVEL:
      return SURFACE_CAT(heap, str_lit("(imax "), universe_text(heap, level->left), str_lit(" "),
                         universe_text(heap, level->right), str_lit(")"));
  }
  return str_empty();
}

Result universe_lower_nat(Heap *heap, MaybeBig value) {
  if (value.some) return budget_result_done((void *)level_var_offset(heap, value.value, level_zero()));
  return budget_result_fail(error_make(ERROR_UNIVERSE, str_lit("a sort level must be nonnegative")));
}

Result universe_lower(Heap *heap, const Universe *level) {
  switch (level->kind) {
    case UNIVERSE_NAT_LEVEL: {
      MaybeBig some = {true, level->value};
      return universe_lower_nat(heap, some);
    }
    case UNIVERSE_VAR_LEVEL:
      return budget_result_done((void *)level_var(heap, level->index));
    case UNIVERSE_SUCC_LEVEL: {
      Result value = universe_lower(heap, level->left);
      if (value.kind == RESULT_FAIL) return value;
      return budget_result_done((void *)level_succ(heap, value.value));
    }
    case UNIVERSE_MAX_LEVEL: {
      Result left = universe_lower(heap, level->left);
      if (left.kind == RESULT_FAIL) return left;
      Result right = universe_lower(heap, level->right);
      if (right.kind == RESULT_FAIL) return right;
      return budget_result_done((void *)level_max(heap, left.value, right.value));
    }
    case UNIVERSE_IMAX_LEVEL: {
      Result left = universe_lower(heap, level->left);
      if (left.kind == RESULT_FAIL) return left;
      Result right = universe_lower(heap, level->right);
      if (right.kind == RESULT_FAIL) return right;
      return budget_result_done((void *)level_imax(heap, left.value, right.value));
    }
  }
  return budget_result_fail(error_host_oom());
}
