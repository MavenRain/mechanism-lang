/* Port of kernel/level_eq.bend: universe level ordering by case split on the variables
 * that can be zero, exact and under a budget. Bend cons lists are NatList and ArmList cells
 * (NULL is Nil); Comp values are const NatList *, Normal *, Big * and the kernel bool box. */
#include "kernel.h"

/* ---- host helpers (not in the Bend source) ---- */

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static NextK next_k(Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return next;
}

static Normal normal_make(Big constant, const ArmList *arms) {
  Normal value;
  value.constant = constant;
  value.arms = arms;
  return value;
}

const NatList *level_eq_cons(Heap *heap, Nat head, const NatList *tail) {
  NatList *cell = heap_alloc(heap, sizeof *cell);
  if (cell == NULL) return tail;
  cell->head = head;
  cell->tail = tail;
  return cell;
}

const ArmList *level_eq_arm_cons(Heap *heap, Nat key, Big amount, const ArmList *rest) {
  ArmList *cell = heap_alloc(heap, sizeof *cell);
  if (cell == NULL) return rest;
  cell->key = key;
  cell->amount = amount;
  cell->rest = rest;
  return cell;
}

static Comp pure_normal(Heap *heap, Normal value) {
  Normal *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp pure_big(Heap *heap, Big value) {
  Big *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp pure_bool(bool value) { return budget_pure(kernel_bool_box(value)); }

/* ---- exact relation ---- */

bool level_eq_member(const NatList *xs, Nat index) {
  const NatList *cursor = xs;
  while (cursor != NULL) {
    if (nat_cmp(cursor->head, index) == 0) return true;
    cursor = cursor->tail;
  }
  return false;
}

const NatList *level_eq_insert(Heap *heap, const NatList *xs, Nat index) {
  if (level_eq_member(xs, index)) return xs;
  return level_eq_cons(heap, index, xs);
}

const NatList *level_eq_variables(Heap *heap, const LevelVar *level, const NatList *found) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return found;
  case LEVEL_VAR_SUCC:
    return level_eq_variables(heap, level->base, found);
  case LEVEL_VAR_MAX:
    return level_eq_variables(heap, level->right, level_eq_variables(heap, level->left, found));
  case LEVEL_VAR_IMAX:
    return level_eq_variables(heap, level->right, level_eq_variables(heap, level->left, found));
  case LEVEL_VAR_VAR:
    return level_eq_insert(heap, found, level->index);
  }
  return found;
}

MaybeBig level_eq_find_arm(const ArmList *xs, Nat index) {
  MaybeBig none;
  none.some = false;
  none.value = big_zero();
  const ArmList *cursor = xs;
  while (cursor != NULL) {
    if (nat_cmp(cursor->key, index) == 0) {
      MaybeBig some;
      some.some = true;
      some.value = cursor->amount;
      return some;
    }
    cursor = cursor->rest;
  }
  return none;
}

const ArmList *level_eq_merge_arm(Heap *heap, const ArmList *xs, Nat index, Big amount) {
  if (xs == NULL) return level_eq_arm_cons(heap, index, amount, NULL);
  if (nat_cmp(xs->key, index) == 0) {
    return level_eq_arm_cons(heap, xs->key, big_maximum(amount, xs->amount), xs->rest);
  }
  return level_eq_arm_cons(heap, xs->key, xs->amount, level_eq_merge_arm(heap, xs->rest, index, amount));
}

const ArmList *level_eq_merge_arms(Heap *heap, const ArmList *xs, const ArmList *acc) {
  const ArmList *merged = acc;
  const ArmList *cursor = xs;
  while (cursor != NULL) {
    merged = level_eq_merge_arm(heap, merged, cursor->key, cursor->amount);
    cursor = cursor->rest;
  }
  return merged;
}

Normal level_eq_maximum(Heap *heap, Normal a, Normal b) {
  return normal_make(big_maximum(a.constant, b.constant), level_eq_merge_arms(heap, a.arms, b.arms));
}

const ArmList *level_eq_shift_arms(Heap *heap, const ArmList *xs, Big amount) {
  if (xs == NULL) return NULL;
  return level_eq_arm_cons(heap, xs->key, big_add(heap, amount, xs->amount), level_eq_shift_arms(heap, xs->rest, amount));
}

Normal level_eq_shift(Heap *heap, Normal value, Big amount) {
  return normal_make(big_add(heap, value.constant, amount), level_eq_shift_arms(heap, value.arms, amount));
}

Normal level_eq_imax_combine(Heap *heap, Normal right, Normal left, Big amount) {
  if (right.arms == NULL) {
    if (big_equal(right.constant, big_zero())) return normal_make(amount, NULL);
    return level_eq_maximum(heap, left, normal_make(big_add(heap, right.constant, amount), NULL));
  }
  return level_eq_maximum(heap, left, level_eq_shift(heap, right, amount));
}

static const ArmList *var_arms(Heap *heap, const NatList *positive, Nat index, Big amount) {
  if (level_eq_member(positive, index)) return level_eq_arm_cons(heap, index, amount, NULL);
  return NULL;
}

Normal level_eq_normal(Heap *heap, const LevelVar *level, const NatList *positive, Big amount) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return normal_make(amount, NULL);
  case LEVEL_VAR_SUCC:
    return level_eq_normal(heap, level->base, positive, big_add(heap, amount, level->amount));
  case LEVEL_VAR_VAR:
    return normal_make(amount, var_arms(heap, positive, level->index, amount));
  case LEVEL_VAR_MAX:
    return level_eq_maximum(heap, level_eq_normal(heap, level->left, positive, amount),
                            level_eq_normal(heap, level->right, positive, amount));
  case LEVEL_VAR_IMAX:
    return level_eq_imax_combine(heap, level_eq_normal(heap, level->right, positive, big_zero()),
                                 level_eq_normal(heap, level->left, positive, amount), amount);
  }
  return normal_make(amount, NULL);
}

Big level_eq_minimum_value(Heap *heap, const ArmList *xs, Big acc) {
  Big result = acc;
  const ArmList *cursor = xs;
  while (cursor != NULL) {
    result = big_maximum(result, big_add(heap, cursor->amount, big_one()));
    cursor = cursor->rest;
  }
  return result;
}

bool level_eq_arm_below(MaybeBig other, Big amount) {
  if (other.some) return big_le(amount, other.value);
  return false;
}

bool level_eq_arms_below(const ArmList *xs, const ArmList *ys) {
  const ArmList *cursor = xs;
  while (cursor != NULL) {
    if (!level_eq_arm_below(level_eq_find_arm(ys, cursor->key), cursor->amount)) return false;
    cursor = cursor->rest;
  }
  return true;
}

bool level_eq_below(Heap *heap, Normal a, Normal b) {
  bool constant_below = big_le(a.constant, level_eq_minimum_value(heap, b.arms, b.constant));
  return constant_below && level_eq_arms_below(a.arms, b.arms);
}

bool level_eq_cases(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                    const LevelVar *right) {
  if (remaining == NULL) {
    return level_eq_below(heap, level_eq_normal(heap, left, positive, big_zero()),
                          level_eq_normal(heap, right, positive, big_zero()));
  }
  bool zero_case = level_eq_cases(heap, remaining->tail, positive, left, right);
  return zero_case &&
         level_eq_cases(heap, remaining->tail, level_eq_cons(heap, remaining->head, positive), left, right);
}

bool level_eq_le(Heap *heap, const LevelVar *left, const LevelVar *right) {
  bool formed = level_var_well_formed(left) && level_var_well_formed(right);
  if (!formed) return false;
  const NatList *all = level_eq_variables(heap, left, level_eq_variables(heap, right, NULL));
  return level_eq_cases(heap, all, NULL, left, right);
}

bool level_eq_equal(Heap *heap, const LevelVar *left, const LevelVar *right) {
  return level_eq_le(heap, left, right) && level_eq_le(heap, right, left);
}

bool level_eq_always_zero(const LevelVar *level) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return true;
  case LEVEL_VAR_SUCC:
    return false;
  case LEVEL_VAR_MAX:
    return level_eq_always_zero(level->left) && level_eq_always_zero(level->right);
  case LEVEL_VAR_IMAX:
    return level_eq_always_zero(level->right);
  case LEVEL_VAR_VAR:
    return false;
  }
  return false;
}

bool level_eq_always_positive(const LevelVar *level) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return false;
  case LEVEL_VAR_SUCC:
    return big_positive(level->amount);
  case LEVEL_VAR_MAX:
    return level_eq_always_positive(level->left) || level_eq_always_positive(level->right);
  case LEVEL_VAR_IMAX:
    return level_eq_always_positive(level->right);
  case LEVEL_VAR_VAR:
    return false;
  }
  return false;
}

Big level_eq_constant(Normal value) { return value.constant; }

MaybeBig level_eq_closed_value(Heap *heap, const LevelVar *level) {
  MaybeBig result;
  result.some = level_var_in_scope(level, nat_zero());
  result.value = result.some ? level_eq_constant(level_eq_normal(heap, level, NULL, big_zero())) : big_zero();
  return result;
}

/* ---- budgeted relation (the laws variables_m .. cases_m and their @unsafe bodies) ---- */

typedef struct {
  const LevelVar *level;
  const NatList *found;
} VariablesCtx;

Comp level_eq_validate_offset(Heap *heap, bool valid, const LevelVar *base, const NatList *found) {
  if (!valid) {
    return budget_failed(error_make(ERROR_UNIVERSE, str_lit("a universe successor offset must be positive")));
  }
  return level_eq_variables_m(heap, base, found);
}

static Comp variables_right(Heap *heap, void *ctx, void *value) {
  const LevelVar *right = ctx;
  return level_eq_variables_m(heap, right, value);
}

static Comp variables_pair(Heap *heap, const LevelVar *left, const LevelVar *right, const NatList *found) {
  return budget_bind(heap, level_eq_variables_m(heap, left, found), next_k(variables_right, (void *)right));
}

Comp level_eq_variables_node(Heap *heap, const LevelVar *level, const NatList *found) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return budget_pure((void *)found);
  case LEVEL_VAR_VAR:
    return budget_pure((void *)level_eq_insert(heap, found, level->index));
  case LEVEL_VAR_SUCC:
    return level_eq_validate_offset(heap, big_positive(level->amount), level->base, found);
  case LEVEL_VAR_MAX:
    return variables_pair(heap, level->left, level->right, found);
  case LEVEL_VAR_IMAX:
    return variables_pair(heap, level->left, level->right, found);
  }
  return budget_pure((void *)found);
}

static Comp variables_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  VariablesCtx *c = ctx;
  return level_eq_variables_node(heap, c->level, c->found);
}

Comp level_eq_variables_m(Heap *heap, const LevelVar *level, const NatList *found) {
  VariablesCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->level = level;
  c->found = found;
  return budget_bind(heap, budget_tick(heap), next_k(variables_after_tick, c));
}

typedef struct {
  const ArmList *xs;
  const ArmList *acc;
} MergeCtx;

static Comp merge_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  MergeCtx *c = ctx;
  return level_eq_merge_node(heap, c->xs, c->acc);
}

Comp level_eq_merge_m(Heap *heap, const ArmList *xs, const ArmList *acc) {
  MergeCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->xs = xs;
  c->acc = acc;
  return budget_bind(heap, budget_tick(heap), next_k(merge_after_tick, c));
}

Comp level_eq_merge_node(Heap *heap, const ArmList *xs, const ArmList *acc) {
  if (xs == NULL) return budget_pure((void *)acc);
  return level_eq_merge_m(heap, xs->rest, level_eq_merge_arm(heap, acc, xs->key, xs->amount));
}

static Comp maximum_with_arms(Heap *heap, void *ctx, void *value) {
  Big *constant = ctx;
  return pure_normal(heap, normal_make(*constant, value));
}

Comp level_eq_maximum_m(Heap *heap, Normal a, Normal b) {
  Big *constant = heap_alloc(heap, sizeof *constant);
  if (constant == NULL) return comp_oom();
  *constant = big_maximum(a.constant, b.constant);
  return budget_bind(heap, level_eq_merge_m(heap, a.arms, b.arms), next_k(maximum_with_arms, constant));
}

typedef struct {
  Nat key;
  Big amount; /* already shifted */
} ShiftHeadCtx;

static Comp shift_arms_with_tail(Heap *heap, void *ctx, void *value) {
  ShiftHeadCtx *c = ctx;
  return budget_pure((void *)level_eq_arm_cons(heap, c->key, c->amount, value));
}

Comp level_eq_shift_arms_node(Heap *heap, const ArmList *xs, Big amount) {
  if (xs == NULL) return budget_pure(NULL);
  ShiftHeadCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->key = xs->key;
  c->amount = big_add(heap, amount, xs->amount);
  return budget_bind(heap, level_eq_shift_arms_m(heap, xs->rest, amount), next_k(shift_arms_with_tail, c));
}

typedef struct {
  const ArmList *xs;
  Big amount;
} ShiftArmsCtx;

static Comp shift_arms_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  ShiftArmsCtx *c = ctx;
  return level_eq_shift_arms_node(heap, c->xs, c->amount);
}

Comp level_eq_shift_arms_m(Heap *heap, const ArmList *xs, Big amount) {
  ShiftArmsCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->xs = xs;
  c->amount = amount;
  return budget_bind(heap, budget_tick(heap), next_k(shift_arms_after_tick, c));
}

static Comp shift_with_arms(Heap *heap, void *ctx, void *value) {
  Big *constant = ctx;
  return pure_normal(heap, normal_make(*constant, value));
}

Comp level_eq_shift_m(Heap *heap, Normal value, Big amount) {
  Big *constant = heap_alloc(heap, sizeof *constant);
  if (constant == NULL) return comp_oom();
  *constant = big_add(heap, value.constant, amount);
  return budget_bind(heap, level_eq_shift_arms_m(heap, value.arms, amount), next_k(shift_with_arms, constant));
}

typedef struct {
  Normal right;
  Big amount;
  Normal lhs;
} ImaxCtx;

static Comp imax_with_rhs(Heap *heap, void *ctx, void *value) {
  ImaxCtx *c = ctx;
  Normal *rhs = value;
  return level_eq_maximum_m(heap, c->lhs, *rhs);
}

static Comp imax_with_lhs(Heap *heap, void *ctx, void *value) {
  ImaxCtx *c = ctx;
  Normal *lhs = value;
  c->lhs = *lhs;
  return budget_bind(heap, level_eq_shift_m(heap, c->right, c->amount), next_k(imax_with_rhs, c));
}

Comp level_eq_imax_nonzero(Heap *heap, bool zero, Normal right, const LevelVar *left, const NatList *positive,
                           Big amount) {
  if (zero) return pure_normal(heap, normal_make(amount, NULL));
  ImaxCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->right = right;
  c->amount = amount;
  c->lhs = normal_make(big_zero(), NULL);
  return budget_bind(heap, level_eq_normal_m(heap, left, positive, amount), next_k(imax_with_lhs, c));
}

bool level_eq_normal_zero(Normal value) {
  if (value.arms != NULL) return false;
  return big_equal(value.constant, big_zero());
}

Comp level_eq_imax_helper(Heap *heap, Normal right, const LevelVar *left, const NatList *positive, Big amount) {
  return level_eq_imax_nonzero(heap, level_eq_normal_zero(right), right, left, positive, amount);
}

typedef struct {
  const LevelVar *level; /* the right operand for LMax, the left one for LIMax */
  const NatList *positive;
  Big amount;
  Normal lhs;
} NormalCtx;

static Comp normal_max_with_rhs(Heap *heap, void *ctx, void *value) {
  NormalCtx *c = ctx;
  Normal *rhs = value;
  return level_eq_maximum_m(heap, c->lhs, *rhs);
}

static Comp normal_max_with_lhs(Heap *heap, void *ctx, void *value) {
  NormalCtx *c = ctx;
  Normal *lhs = value;
  c->lhs = *lhs;
  return budget_bind(heap, level_eq_normal_m(heap, c->level, c->positive, c->amount), next_k(normal_max_with_rhs, c));
}

static Comp normal_imax_with_rhs(Heap *heap, void *ctx, void *value) {
  NormalCtx *c = ctx;
  Normal *rhs = value;
  return level_eq_imax_helper(heap, *rhs, c->level, c->positive, c->amount);
}

static NormalCtx *normal_ctx(Heap *heap, const LevelVar *level, const NatList *positive, Big amount) {
  NormalCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return NULL;
  c->level = level;
  c->positive = positive;
  c->amount = amount;
  c->lhs = normal_make(big_zero(), NULL);
  return c;
}

Comp level_eq_normal_node(Heap *heap, const LevelVar *level, const NatList *positive, Big amount) {
  switch (level->kind) {
  case LEVEL_VAR_ZERO:
    return pure_normal(heap, normal_make(amount, NULL));
  case LEVEL_VAR_VAR:
    return pure_normal(heap, normal_make(amount, var_arms(heap, positive, level->index, amount)));
  case LEVEL_VAR_SUCC:
    return level_eq_normal_m(heap, level->base, positive, big_add(heap, amount, level->amount));
  case LEVEL_VAR_MAX: {
    NormalCtx *c = normal_ctx(heap, level->right, positive, amount);
    if (c == NULL) return comp_oom();
    return budget_bind(heap, level_eq_normal_m(heap, level->left, positive, amount), next_k(normal_max_with_lhs, c));
  }
  case LEVEL_VAR_IMAX: {
    NormalCtx *c = normal_ctx(heap, level->left, positive, amount);
    if (c == NULL) return comp_oom();
    return budget_bind(heap, level_eq_normal_m(heap, level->right, positive, big_zero()),
                       next_k(normal_imax_with_rhs, c));
  }
  }
  return pure_normal(heap, normal_make(amount, NULL));
}

static Comp normal_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  NormalCtx *c = ctx;
  return level_eq_normal_node(heap, c->level, c->positive, c->amount);
}

Comp level_eq_normal_m(Heap *heap, const LevelVar *level, const NatList *positive, Big amount) {
  NormalCtx *c = normal_ctx(heap, level, positive, amount);
  if (c == NULL) return comp_oom();
  return budget_bind(heap, budget_tick(heap), next_k(normal_after_tick, c));
}

typedef struct {
  const ArmList *xs;
  Big acc;
} MinimumCtx;

Comp level_eq_minimum_node(Heap *heap, const ArmList *xs, Big acc) {
  if (xs == NULL) return pure_big(heap, acc);
  return level_eq_minimum_m(heap, xs->rest, big_maximum(acc, big_add(heap, xs->amount, big_one())));
}

static Comp minimum_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  MinimumCtx *c = ctx;
  return level_eq_minimum_node(heap, c->xs, c->acc);
}

Comp level_eq_minimum_m(Heap *heap, const ArmList *xs, Big acc) {
  MinimumCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->xs = xs;
  c->acc = acc;
  return budget_bind(heap, budget_tick(heap), next_k(minimum_after_tick, c));
}

typedef struct {
  const ArmList *xs;
  const ArmList *ys;
  bool same;
} ArmsBelowCtx;

Comp level_eq_arms_below_node(Heap *heap, const ArmList *xs, const ArmList *ys, bool same) {
  if (xs == NULL) return pure_bool(same);
  bool head_below = level_eq_arm_below(level_eq_find_arm(ys, xs->key), xs->amount);
  return level_eq_arms_below_m(heap, xs->rest, ys, same && head_below);
}

static Comp arms_below_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  ArmsBelowCtx *c = ctx;
  return level_eq_arms_below_node(heap, c->xs, c->ys, c->same);
}

Comp level_eq_arms_below_m(Heap *heap, const ArmList *xs, const ArmList *ys, bool same) {
  ArmsBelowCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->xs = xs;
  c->ys = ys;
  c->same = same;
  return budget_bind(heap, budget_tick(heap), next_k(arms_below_after_tick, c));
}

typedef struct {
  Normal a;
  Normal b;
} BelowCtx;

static Comp below_with_min(Heap *heap, void *ctx, void *value) {
  BelowCtx *c = ctx;
  Big *min = value;
  return level_eq_arms_below_m(heap, c->a.arms, c->b.arms, big_le(c->a.constant, *min));
}

Comp level_eq_below_m(Heap *heap, Normal a, Normal b) {
  BelowCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->a = a;
  c->b = b;
  return budget_bind(heap, level_eq_minimum_m(heap, b.arms, b.constant), next_k(below_with_min, c));
}

Comp level_eq_relation_reverse(Heap *heap, bool holds, bool equality, Normal left, Normal right) {
  if (holds && equality) return level_eq_below_m(heap, right, left);
  return pure_bool(holds);
}

typedef struct {
  Normal left;
  Normal right;
  bool equality;
} CompareCtx;

static Comp compare_with_holds(Heap *heap, void *ctx, void *value) {
  CompareCtx *c = ctx;
  return level_eq_relation_reverse(heap, kernel_bool_unbox(value), c->equality, c->left, c->right);
}

Comp level_eq_compare_normal(Heap *heap, Normal left, Normal right, bool equality) {
  CompareCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->left = left;
  c->right = right;
  c->equality = equality;
  return budget_bind(heap, level_eq_below_m(heap, left, right), next_k(compare_with_holds, c));
}

Comp level_eq_cases_continue(Heap *heap, bool holds, const NatList *remaining, const NatList *positive,
                             const LevelVar *left, const LevelVar *right, bool equality) {
  if (!holds) return pure_bool(false);
  return level_eq_cases_m(heap, remaining, positive, left, right, equality);
}

typedef struct {
  const NatList *remaining;
  const NatList *positive;
  const LevelVar *left;
  const LevelVar *right;
  bool equality;
  Normal lhs;
} CasesCtx;

static CasesCtx *cases_ctx(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                           const LevelVar *right, bool equality) {
  CasesCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return NULL;
  c->remaining = remaining;
  c->positive = positive;
  c->left = left;
  c->right = right;
  c->equality = equality;
  c->lhs = normal_make(big_zero(), NULL);
  return c;
}

static Comp cases_leaf_with_rhs(Heap *heap, void *ctx, void *value) {
  CasesCtx *c = ctx;
  Normal *rhs = value;
  return level_eq_compare_normal(heap, c->lhs, *rhs, c->equality);
}

static Comp cases_leaf_with_lhs(Heap *heap, void *ctx, void *value) {
  CasesCtx *c = ctx;
  Normal *lhs = value;
  c->lhs = *lhs;
  return budget_bind(heap, level_eq_normal_m(heap, c->right, c->positive, big_zero()), next_k(cases_leaf_with_rhs, c));
}

static Comp cases_with_holds(Heap *heap, void *ctx, void *value) {
  CasesCtx *c = ctx;
  /* c->remaining holds the index being split; c->positive is the positive list without it. */
  return level_eq_cases_continue(heap, kernel_bool_unbox(value), c->remaining->tail,
                                 level_eq_cons(heap, c->remaining->head, c->positive), c->left, c->right, c->equality);
}

Comp level_eq_cases_node(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                         const LevelVar *right, bool equality) {
  CasesCtx *c = cases_ctx(heap, remaining, positive, left, right, equality);
  if (c == NULL) return comp_oom();
  if (remaining == NULL) {
    return budget_bind(heap, level_eq_normal_m(heap, left, positive, big_zero()), next_k(cases_leaf_with_lhs, c));
  }
  return budget_bind(heap, level_eq_cases_m(heap, remaining->tail, positive, left, right, equality),
                     next_k(cases_with_holds, c));
}

static Comp cases_after_tick(Heap *heap, void *ctx, void *value) {
  (void)value;
  CasesCtx *c = ctx;
  return level_eq_cases_node(heap, c->remaining, c->positive, c->left, c->right, c->equality);
}

Comp level_eq_cases_m(Heap *heap, const NatList *remaining, const NatList *positive, const LevelVar *left,
                      const LevelVar *right, bool equality) {
  CasesCtx *c = cases_ctx(heap, remaining, positive, left, right, equality);
  if (c == NULL) return comp_oom();
  return budget_bind(heap, budget_tick(heap), next_k(cases_after_tick, c));
}

typedef struct {
  const LevelVar *left;
  const LevelVar *right;
  bool equality;
} DecideCtx;

static Comp decide_with_all(Heap *heap, void *ctx, void *value) {
  DecideCtx *c = ctx;
  return level_eq_cases_m(heap, value, NULL, c->left, c->right, c->equality);
}

static Comp decide_with_first(Heap *heap, void *ctx, void *value) {
  DecideCtx *c = ctx;
  return budget_bind(heap, level_eq_variables_m(heap, c->right, value), next_k(decide_with_all, c));
}

Comp level_eq_decide_m(Heap *heap, const LevelVar *left, const LevelVar *right, bool equality) {
  DecideCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return comp_oom();
  c->left = left;
  c->right = right;
  c->equality = equality;
  return budget_bind(heap, level_eq_variables_m(heap, left, NULL), next_k(decide_with_first, c));
}

Result level_eq_le_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right) {
  return budget_run(heap, level_eq_decide_m(heap, left, right, false), budget);
}

Result level_eq_equal_budget(Heap *heap, Budget budget, const LevelVar *left, const LevelVar *right) {
  return budget_run(heap, level_eq_decide_m(heap, left, right, true), budget);
}
