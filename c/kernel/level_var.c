/* Port of bend2/kernel/level_var.bend: universe level expressions with variables. Levels are
 * immutable and shared by pointer; LZero is a single static value. Same definitions in the
 * same order. */
#include "kernel.h"

static const LevelVar level_var_zero_value = {.kind = LEVEL_VAR_ZERO};

static MaybeLevelVar level_none(void) {
  MaybeLevelVar value = {false, NULL};
  return value;
}

static MaybeLevelVar level_some(const LevelVar *level) {
  MaybeLevelVar value = {true, level};
  return value;
}

static LevelVar *level_var_alloc(Heap *heap, LevelVarKind kind) {
  LevelVar *level = heap_alloc(heap, sizeof(LevelVar));
  if (level != NULL) level->kind = kind;
  return level;
}

const LevelVar *level_var_lzero(void) { return &level_var_zero_value; }

const LevelVar *level_var_lsucc(Heap *heap, Big amount, const LevelVar *base) {
  LevelVar *level = level_var_alloc(heap, LEVEL_VAR_SUCC);
  if (level == NULL) return level_var_lzero();
  level->amount = amount;
  level->base = base;
  return level;
}

const LevelVar *level_var_lmax(Heap *heap, const LevelVar *left, const LevelVar *right) {
  LevelVar *level = level_var_alloc(heap, LEVEL_VAR_MAX);
  if (level == NULL) return level_var_lzero();
  level->left = left;
  level->right = right;
  return level;
}

const LevelVar *level_var_limax(Heap *heap, const LevelVar *left, const LevelVar *right) {
  LevelVar *level = level_var_alloc(heap, LEVEL_VAR_IMAX);
  if (level == NULL) return level_var_lzero();
  level->left = left;
  level->right = right;
  return level;
}

const LevelVar *level_var_lvar(Heap *heap, Nat index) {
  LevelVar *level = level_var_alloc(heap, LEVEL_VAR_VAR);
  if (level == NULL) return level_var_lzero();
  level->index = index;
  return level;
}

const LevelVar *level_var_offset_positive(Heap *heap, const LevelVar *level, Big amount) {
  switch (level->kind) {
    case LEVEL_VAR_SUCC: return level_var_lsucc(heap, big_add(heap, amount, level->amount), level->base);
    case LEVEL_VAR_ZERO: return level_var_lsucc(heap, amount, level_var_lzero());
    case LEVEL_VAR_MAX: return level_var_lsucc(heap, amount, level);
    case LEVEL_VAR_IMAX: return level_var_lsucc(heap, amount, level);
    case LEVEL_VAR_VAR: return level_var_lsucc(heap, amount, level);
  }
  return level;
}

const LevelVar *level_var_offset_select(Heap *heap, bool positive, Big amount, const LevelVar *level) {
  if (positive) return level_var_offset_positive(heap, level, amount);
  return level;
}

const LevelVar *level_var_offset(Heap *heap, Big amount, const LevelVar *level) {
  return level_var_offset_select(heap, big_positive(amount), amount, level);
}

bool level_var_in_scope(const LevelVar *level, Nat arity) {
  switch (level->kind) {
    case LEVEL_VAR_ZERO: return true;
    case LEVEL_VAR_SUCC: return big_positive(level->amount) && level_var_in_scope(level->base, arity);
    case LEVEL_VAR_MAX: return level_var_in_scope(level->left, arity) && level_var_in_scope(level->right, arity);
    case LEVEL_VAR_IMAX: return level_var_in_scope(level->left, arity) && level_var_in_scope(level->right, arity);
    case LEVEL_VAR_VAR: return nat_cmp(level->index, arity) < 0;
  }
  return false;
}

bool level_var_well_formed(const LevelVar *level) {
  switch (level->kind) {
    case LEVEL_VAR_ZERO: return true;
    case LEVEL_VAR_SUCC: return big_positive(level->amount) && level_var_well_formed(level->base);
    case LEVEL_VAR_MAX: return level_var_well_formed(level->left) && level_var_well_formed(level->right);
    case LEVEL_VAR_IMAX: return level_var_well_formed(level->left) && level_var_well_formed(level->right);
    case LEVEL_VAR_VAR: return true;
  }
  return false;
}

MaybeLevelVar level_var_subst_succ(Heap *heap, MaybeLevelVar value, Big amount) {
  if (!value.some) return level_none();
  return level_some(level_var_offset(heap, amount, value.value));
}

MaybeLevelVar level_var_subst_pair(Heap *heap, MaybeLevelVar a, MaybeLevelVar b, bool imax) {
  if (!a.some) return level_none();
  if (!b.some) return level_none();
  if (imax) return level_some(level_var_limax(heap, a.value, b.value));
  return level_some(level_var_lmax(heap, a.value, b.value));
}

MaybeLevelVar level_var_subst_visit(Heap *heap, const LevelVar *level, const Vec *args) {
  switch (level->kind) {
    case LEVEL_VAR_ZERO: return level_some(level_var_lzero());
    case LEVEL_VAR_SUCC: return level_var_subst_succ(heap, level_var_subst_visit(heap, level->base, args), level->amount);
    case LEVEL_VAR_MAX:
      return level_var_subst_pair(heap, level_var_subst_visit(heap, level->left, args),
                                  level_var_subst_visit(heap, level->right, args), false);
    case LEVEL_VAR_IMAX:
      return level_var_subst_pair(heap, level_var_subst_visit(heap, level->left, args),
                                  level_var_subst_visit(heap, level->right, args), true);
    case LEVEL_VAR_VAR: {
      VecGet got = common_at(args, level->index);
      if (got.status == VEC_GET_OK && got.value != NULL) return level_some((const LevelVar *)got.value);
      return level_none();
    }
  }
  return level_none();
}

MaybeLevelVar level_var_subst_valid(Heap *heap, bool valid, const Vec *args, const LevelVar *level) {
  if (valid) return level_var_subst_visit(heap, level, args);
  return level_none();
}

/* List.all(well_formed, args) */
static bool level_var_all_well_formed(const Vec *args) {
  for (size_t i = 0; i < args->len; i++) {
    VecGet got = vec_get(args, i);
    if (got.status == VEC_GET_OK && got.value != NULL && !level_var_well_formed((const LevelVar *)got.value)) return false;
  }
  return true;
}

MaybeLevelVar level_var_subst(Heap *heap, const Vec *args, const LevelVar *level) {
  bool scoped = level_var_in_scope(level, heap_nat_of_u64(heap, args->len));
  bool formed = level_var_all_well_formed(args);
  return level_var_subst_valid(heap, scoped && formed, args, level);
}

bool level_var_structural_equal(const LevelVar *a, const LevelVar *b) {
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case LEVEL_VAR_ZERO: return true;
    case LEVEL_VAR_SUCC: return big_equal(a->amount, b->amount) && level_var_structural_equal(a->base, b->base);
    case LEVEL_VAR_MAX:
      return level_var_structural_equal(a->left, b->left) && level_var_structural_equal(a->right, b->right);
    case LEVEL_VAR_IMAX:
      return level_var_structural_equal(a->left, b->left) && level_var_structural_equal(a->right, b->right);
    case LEVEL_VAR_VAR: return nat_cmp(a->index, b->index) == 0;
  }
  return false;
}
