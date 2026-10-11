/* Port of bend2/tests/surface_equality.bend (oracle label surface-equality).
 * The named assertions and refusal prefixes mirror test/equality.ml. */
#include "driver.h"

#define CASE_COUNT 15

/* require(ok, message): Done{Unit} when ok, else Fail{Mismatch{message}}. */
static Result require(bool ok, Str message) {
  if (ok) return budget_result_done(kernel_unit());
  return budget_result_fail(error_make(ERROR_MISMATCH, message));
}

/* prefix(expected, actual): expected is a prefix of actual. */
static bool prefix(Str expected, Str actual) { return str_starts_with(actual, expected); }

/* refusal(expected, result): Done -> Mismatch "expected refusal: ..."; Fail{actual} -> the expected text
 * must prefix the actual text, else Mismatch "wrong refusal: ...". */
static Result refusal(Heap *heap, Error expected, Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return budget_result_fail(
          error_make(ERROR_MISMATCH, heap_str_cat(heap, str_lit("expected refusal: "), error_to_string(heap, expected))));
    case RESULT_FAIL: {
      Str actual = error_to_string(heap, result.error);
      return require(prefix(error_to_string(heap, expected), actual), heap_str_cat(heap, str_lit("wrong refusal: "), actual));
    }
  }
  return budget_result_fail(error_host_oom());
}

/* accepted(result): Done{value} -> Done{Unit}; Fail{error} -> Fail{error}. */
static Result accepted(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return budget_result_done(kernel_unit());
    case RESULT_FAIL:
      return budget_result_fail(result.error);
  }
  return budget_result_fail(error_host_oom());
}

static Vec push_one(Heap *heap, void *item) {
  Vec v = vec_new(heap->arena);
  if (item == NULL || !vec_push(&v, item)) heap->oom = true;
  return v;
}

static Vec push_two(Heap *heap, void *first, void *second) {
  Vec v = push_one(heap, first);
  if (second == NULL || !vec_push(&v, second)) heap->oom = true;
  return v;
}

/* Term.Var{index} */
static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, heap_nat_of_u64(heap, index)); }

/* params(carrier) = [P.Binder{QZero, "A", Univ{carrier}}, P.Binder{QZero, "x", Var{0n}}] */
static Vec params(Heap *heap, const LevelVar *carrier) {
  return push_two(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, carrier)),
                  positivity_binder_new(heap, QUANTITY_ZERO, str_lit("x"), var(heap, 0)));
}

/* Check.FamilyDecl{name, params, indices, level} */
static FamilyDecl family_decl(const char *name, Vec params, Vec indices, const LevelVar *level) {
  FamilyDecl decl;
  decl.name = str_lit(name);
  decl.params = params;
  decl.indices = indices;
  decl.level = level;
  return decl;
}

/* anchored(quantity, carrier, sort) = FamilyDecl{"AnchoredEquality", params(carrier), [Binder{quantity, "y", Var{1n}}], sort} */
static FamilyDecl anchored(Heap *heap, Quantity quantity, const LevelVar *carrier, const LevelVar *sort) {
  Vec indices = push_one(heap, positivity_binder_new(heap, quantity, str_lit("y"), var(heap, 1)));
  return family_decl("AnchoredEquality", params(heap, carrier), indices, sort);
}

/* declared(family) = Check.declare_family(Global.empty(), B.unlimited(), family) */
static Result declared(Heap *heap, FamilyDecl family) {
  return check_declare_family(heap, global_empty(), budget_unlimited(), family);
}

static bool family_positive(Family family) { return family.positive; }

/* equality_metadata(family) */
static Result equality_metadata(MaybeFamily family) {
  if (!family.some) return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("checked equality family disappeared")));
  Result checked = require(family_positive(family.value), str_lit("equality lost its positivity verdict"));
  if (checked.kind == RESULT_FAIL) return checked;
  return require(check_engine_mu_zero_eliminable(family.value), str_lit("nullary equality lost large elimination"));
}

/* [name] as the ctor group */
static Vec group_of(Heap *heap, const char *name) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed != NULL) *boxed = str_lit(name);
  return push_one(heap, boxed);
}

/* Check.CtorDecl{name, args, res_params, res_idx} */
static CtorDecl *ctor_decl(Heap *heap, const char *name, Vec args, Vec res_params, Vec res_idx) {
  CtorDecl *decl = heap_alloc(heap, sizeof *decl);
  if (decl == NULL) {
    heap->oom = true;
    return NULL;
  }
  decl->name = str_lit(name);
  decl->args = args;
  decl->res_params = res_params;
  decl->res_idx = res_idx;
  return decl;
}

/* The Globals payload of a Done result. */
static Globals *done_globals(Result result) { return result.kind == RESULT_DONE ? (Globals *)result.value : NULL; }

/* checked_equality(carrier) */
static Result checked_equality(Heap *heap, const LevelVar *carrier) {
  Result first = declared(heap, anchored(heap, QUANTITY_ZERO, carrier, level_zero()));
  if (first.kind == RESULT_FAIL) return first;
  Globals *globals = done_globals(first);
  if (globals == NULL) return budget_result_fail(error_host_oom());
  Vec group = group_of(heap, "AnchoredEquality");
  Vec ctors = push_one(heap, ctor_decl(heap, "anchoredRefl", vec_new(heap->arena),
                                       push_two(heap, (void *)var(heap, 1), (void *)var(heap, 0)),
                                       push_one(heap, (void *)var(heap, 0))));
  Result second = check_define_ctors(heap, *globals, budget_unlimited(), &group, str_lit("AnchoredEquality"), ctors);
  if (second.kind == RESULT_FAIL) return second;
  Globals *defined = done_globals(second);
  if (defined == NULL) return budget_result_fail(error_host_oom());
  return equality_metadata(global_find_family(str_lit("AnchoredEquality"), *defined));
}

/* highest(value): Some{number} -> checked_equality(LSucc{number, zero}); None -> Mismatch. */
static Result highest(Heap *heap, MaybeBig value) {
  if (!value.some) return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("nonnegative level construction failed")));
  LevelVar *level = heap_alloc(heap, sizeof *level);
  if (level == NULL) {
    heap->oom = true;
    return budget_result_fail(error_host_oom());
  }
  level->kind = LEVEL_VAR_SUCC;
  level->amount = value.value;
  level->base = level_zero();
  level->left = NULL;
  level->right = NULL;
  level->index = nat_zero();
  return checked_equality(heap, level);
}

/* simple(index) = FamilyDecl{"PropositionIndex", [], [Binder{QZero, "index", index}], zero} */
static FamilyDecl simple(Heap *heap, const Term *index) {
  Vec indices = push_one(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("index"), index));
  return family_decl("PropositionIndex", vec_new(heap->arena), indices, level_zero());
}

/* hidden_ctors(level, quantity) */
static Result hidden_ctors(Heap *heap, const LevelVar *level, Quantity quantity) {
  Vec params = push_one(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, level_one(heap))));
  Result first = declared(heap, family_decl("HiddenData", params, vec_new(heap->arena), level));
  if (first.kind == RESULT_FAIL) return first;
  Globals *globals = done_globals(first);
  if (globals == NULL) return budget_result_fail(error_host_oom());
  Vec group = group_of(heap, "HiddenData");
  Vec args = push_one(heap, positivity_binder_new(heap, quantity, str_lit("value"), var(heap, 0)));
  Vec ctors = push_one(heap, ctor_decl(heap, "hideData", args, push_one(heap, (void *)var(heap, 1)), vec_new(heap->arena)));
  return check_define_ctors(heap, *globals, budget_unlimited(), &group, str_lit("HiddenData"), ctors);
}

/* field_bound(quantity) */
static Result field_bound(Heap *heap, Quantity quantity) {
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("a field of hideData exceeds its family universe")),
                 hidden_ctors(heap, level_zero(), quantity));
}

/* positive(family) */
static Result positive(MaybeFamily family) {
  if (!family.some) return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("accepted data family disappeared")));
  return require(family_positive(family.value), str_lit("accepted data family lost its positivity verdict"));
}

/* field_at_bound() */
static Result field_at_bound(Heap *heap) {
  Result result = hidden_ctors(heap, level_one(heap), QUANTITY_MANY);
  if (result.kind == RESULT_FAIL) return result;
  Globals *globals = done_globals(result);
  if (globals == NULL) return budget_result_fail(error_host_oom());
  return positive(global_find_family(str_lit("HiddenData"), *globals));
}

/* A Comp that fails with the host out-of-memory error. */
static Comp comp_host_oom(void) {
  Comp comp;
  comp.kind = COMP_ERROR;
  comp.value = NULL;
  comp.error = error_host_oom();
  comp.action.fn = NULL;
  comp.action.ctx = NULL;
  return comp;
}

/* index_must_be_type() */
static Result index_must_be_type(Heap *heap) {
  Result context = budget_run(
      heap, check_check_telescope(heap, params(heap, level_one(heap)), check_make(heap, global_empty(), budget_unlimited(), nat_zero())),
      budget_unlimited());
  if (context.kind == RESULT_FAIL) return context;
  Ctx *ctx = context.value;
  TeleBinder *binder = positivity_binder_new(heap, QUANTITY_ZERO, str_lit("index"), var(heap, 0));
  if (ctx == NULL || binder == NULL) {
    heap->oom = true;
    return budget_result_fail(error_host_oom());
  }
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("a term used as a type is not a universe:")),
                 budget_run(heap, check_index_rules(heap, *ctx, str_lit("AnchoredEquality"), level_zero(), binder), budget_unlimited()));
}

/* countdown(remaining): a poll callback that answers spent once remaining reaches 0n. */
typedef struct {
  uint64_t remaining;
} Countdown;

static Callback countdown(Heap *heap, uint64_t remaining);

static CallbackPoll *countdown_poll(Heap *heap, void *ctx) {
  Countdown *c = ctx;
  if (c == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = c->remaining == 0;
  out->next = countdown(heap, c->remaining == 0 ? 0 : c->remaining - 1);
  return out;
}

static Callback countdown(Heap *heap, uint64_t remaining) {
  Countdown *c = heap_alloc(heap, sizeof *c);
  if (c != NULL) c->remaining = remaining;
  return budget_of_poll(countdown_poll, c);
}

/* index_action(sort, carrier) = Check.index_rules(make(empty, unlimited, 0n), "BudgetedIndex", sort, Binder{QZero, "index", Univ{carrier}}) */
static Comp index_action(Heap *heap, const LevelVar *sort, const LevelVar *carrier) {
  TeleBinder *binder = positivity_binder_new(heap, QUANTITY_ZERO, str_lit("index"), term_univ(heap, carrier));
  if (binder == NULL) {
    heap->oom = true;
    return comp_host_oom();
  }
  return check_index_rules(heap, check_make(heap, global_empty(), budget_unlimited(), nat_zero()), str_lit("BudgetedIndex"), sort, binder);
}

static Error budget_error(void) { return error_make(ERROR_BUDGET_EXHAUSTED, str_lit("the check budget is exhausted")); }

/* index_budget() */
static Result index_budget(Heap *heap) {
  return refusal(heap, budget_error(), budget_run_poll(heap, index_action(heap, level_zero(), level_one(heap)), countdown(heap, 0)));
}

/* Nat payload of a measurement, boxed. */
static Result done_nat(Heap *heap, Nat value) {
  Nat *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) {
    heap->oom = true;
    return budget_result_fail(error_host_oom());
  }
  *boxed = value;
  return budget_result_done(boxed);
}

/* measured(state): Pair{Polls{remaining}, Done} -> Done{100000n - remaining}; Pair{_, Fail{error}} -> Fail{error};
 * other -> Mismatch. */
static Result measured(Heap *heap, BudgetState state) {
  if (state.budget.kind == BUDGET_POLLS && state.result.kind == RESULT_DONE) {
    NatSub diff = nat_sub(heap->arena, heap_nat_of_u64(heap, 100000), state.budget.remaining);
    switch (diff.status) {
      case NAT_SUB_OK:
        return done_nat(heap, diff.value);
      case NAT_SUB_UNDERFLOW:
        return done_nat(heap, nat_zero());
      case NAT_SUB_OOM:
        heap->oom = true;
        return budget_result_fail(error_host_oom());
    }
  }
  if (state.result.kind == RESULT_FAIL) return budget_result_fail(state.result.error);
  return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("finite index poll measurement disappeared")));
}

/* poll_answer(response): the exhausted flag of a poll. */
static bool poll_answer(Heap *heap, const CallbackPoll *response) {
  if (response == NULL) {
    heap->oom = true;
    return false;
  }
  return response->spent;
}

/* next_poll(callback) */
static bool next_poll(Heap *heap, Callback callback) { return poll_answer(heap, callback.poll(heap, callback.ctx)); }

/* exact_callback_boundary(state) */
static Result exact_callback_boundary(Heap *heap, CallbackState state) {
  if (state.result.kind == RESULT_FAIL) return budget_result_fail(state.result.error);
  return require(next_poll(heap, state.callback), str_lit("Prop index used fewer callback polls than the measured path"));
}

/* The u64 value of a boxed Nat payload. */
static uint64_t allowance_of(Heap *heap, const Result *result) {
  const Nat *boxed = result->value;
  if (boxed == NULL) {
    heap->oom = true;
    return 0;
  }
  NatToU64 narrowed = nat_to_u64(*boxed);
  switch (narrowed.status) {
    case NAT_TO_U64_OK:
      return narrowed.value;
    case NAT_TO_U64_OVERFLOW:
      return UINT64_MAX;
  }
  return 0;
}

/* prop_skips_level_poll() */
static Result prop_skips_level_poll(Heap *heap) {
  Result allowance = measured(
      heap, budget_run_state(heap, index_action(heap, level_zero(), level_zero()), budget_of_polls(heap_nat_of_u64(heap, 100000))));
  if (allowance.kind == RESULT_FAIL) return allowance;
  uint64_t polls = allowance_of(heap, &allowance);
  Result checked =
      exact_callback_boundary(heap, budget_run_poll_state(heap, index_action(heap, level_zero(), level_zero()), countdown(heap, polls)));
  if (checked.kind == RESULT_FAIL) return checked;
  return refusal(heap, budget_error(), budget_run_poll(heap, index_action(heap, level_one(heap), level_zero()), countdown(heap, polls)));
}

/* Case{name, run}: the run result is taken eagerly, in list order. */
typedef struct {
  const char *name;
  Result result;
} Row;

static void cases(Heap *heap, Row rows[CASE_COUNT]) {
  rows[0].name = "prop-data-index";
  rows[0].result = checked_equality(heap, level_one(heap));
  rows[1].name = "prop-high-universe-index";
  rows[1].result = highest(heap, big_of_decimal(heap, str_lit("4611686018427387903")));
  rows[2].name = "prop-type-index";
  rows[2].result = accepted(declared(heap, simple(heap, term_univ(heap, level_one(heap)))));
  rows[3].name = "type-index-at-bound";
  rows[3].result = accepted(declared(heap, anchored(heap, QUANTITY_ZERO, level_one(heap), level_one(heap))));
  rows[4].name = "type-index-above-bound";
  rows[4].result = refusal(heap, error_make(ERROR_INDEX_ABOVE_UNIVERSE, str_lit("the index y of AnchoredEquality lives at")),
                           declared(heap, anchored(heap, QUANTITY_ZERO, level_succ(heap, level_one(heap)), level_one(heap))));
  rows[5].name = "prop-runtime-index";
  rows[5].result = refusal(heap, error_make(ERROR_INDEX_NOT_ZERO, str_lit("the index y of AnchoredEquality is at quantity")),
                           declared(heap, anchored(heap, QUANTITY_MANY, level_one(heap), level_zero())));
  rows[6].name = "prop-unbound-index-type";
  rows[6].result = refusal(heap, error_make(ERROR_UNBOUND, str_lit("de Bruijn index 0 is outside the context")),
                           declared(heap, simple(heap, var(heap, 0))));
  rows[7].name = "prop-index-must-be-type";
  rows[7].result = index_must_be_type(heap);
  rows[8].name = "prop-erased-data-field";
  rows[8].result = field_bound(heap, QUANTITY_ZERO);
  rows[9].name = "prop-runtime-data-field";
  rows[9].result = field_bound(heap, QUANTITY_MANY);
  rows[10].name = "data-field-at-bound";
  rows[10].result = field_at_bound(heap);
  rows[11].name = "prop-index-refuses-free-universe";
  rows[11].result = refusal(heap, error_make(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope")),
                            declared(heap, simple(heap, term_univ(heap, level_var(heap, nat_zero())))));
  rows[12].name = "family-refuses-free-universe";
  rows[12].result = refusal(heap, error_make(ERROR_UNIVERSE, str_lit("family universe levels must be closed at M0 Stage A")),
                            declared(heap, anchored(heap, QUANTITY_ZERO, level_one(heap), level_var(heap, nat_zero()))));
  rows[13].name = "index-type-honors-budget";
  rows[13].result = index_budget(heap);
  rows[14].name = "prop-index-skips-level-poll";
  rows[14].result = prop_skips_level_poll(heap);
}

/* report(name, result): PASS line -> 0n; FAIL line with the error text -> 1n. */
static int report(Heap *heap, const Row *row) {
  switch (row->result.kind) {
    case RESULT_DONE:
      driver_print(heap_str_cat(heap, str_lit("PASS equality "), str_lit(row->name)));
      return 0;
    case RESULT_FAIL:
      driver_print(heap_str_cat(heap, heap_str_cat(heap, str_lit("FAIL equality "), str_lit(row->name)),
                                heap_str_cat(heap, str_lit(": "), error_to_string(heap, row->result.error))));
      return 1;
  }
  return 1;
}

/* run(cases, failures): at the end 0 failures prints the OK line, otherwise dies with code 1. */
static int run(Heap *heap, const Row rows[CASE_COUNT], size_t index, int failures) {
  if (index >= CASE_COUNT) {
    if (failures == 0) return driver_print(str_lit("EQUALITY-OK cases=15"));
    return driver_die(1, "equality assertions failed");
  }
  return run(heap, rows, index + 1, failures + report(heap, &rows[index]));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Row rows[CASE_COUNT];
  cases(&heap, rows);
  int code = heap.oom ? driver_host_failure() : run(&heap, rows, 0, 0);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
