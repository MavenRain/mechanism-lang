/* Port of bend2/tests/kernel_head_ty_invalid_level.bend (oracle label unit-kernel-head-ty-invalid-level). */
#include "driver.h"

#include <stdio.h>

/* Expected = Missing | UniverseVariable{index}. */
typedef enum { EXPECTED_MISSING, EXPECTED_UNIVERSE_VARIABLE } ExpectedKind;
typedef struct {
  ExpectedKind kind;
  Nat index; /* EXPECTED_UNIVERSE_VARIABLE */
} Expected;

typedef struct {
  const char *name;
  Ctx context;
  Nat level;
  Expected expected;
} Fixture;

#define FIXTURE_COUNT 14u

static Expected missing(void) {
  Expected e;
  e.kind = EXPECTED_MISSING;
  e.index = nat_zero();
  return e;
}

static Expected universe_variable(Heap *heap, uint64_t index) {
  Expected e;
  e.kind = EXPECTED_UNIVERSE_VARIABLE;
  e.index = heap_nat_of_u64(heap, index);
  return e;
}

static Fixture fixture(const char *name, Ctx context, Nat level, Expected expected) {
  Fixture f;
  f.name = name;
  f.context = context;
  f.level = level;
  f.expected = expected;
  return f;
}

static const Value *typed(Heap *heap, uint64_t index) {
  return value_vuniv(heap, level_var_lvar(heap, heap_nat_of_u64(heap, index)));
}

static Ctx empty(Heap *heap) {
  return context_make(heap, global_empty(), budget_unlimited(), heap_nat_of_u64(heap, 3));
}

static Ctx one(Heap *heap) {
  return context_bind(heap, str_lit("oldest"), QUANTITY_MANY, typed(heap, 0), empty(heap));
}

static Ctx three(Heap *heap) {
  return context_bind(heap, str_lit("newest"), QUANTITY_MANY, typed(heap, 2),
                      context_bind(heap, str_lit("middle"), QUANTITY_MANY, typed(heap, 1), one(heap)));
}

/* Raw inconsistent contexts preserve the old missing-list behavior separately. */
static Ctx raw(Heap *heap, uint64_t size, const Vec *locals) {
  Ctx ctx;
  ctx.globals = global_empty();
  ctx.env = vec_new(heap->arena);
  ctx.locals = context_from_list(heap, locals);
  ctx.size = heap_nat_of_u64(heap, size);
  ctx.budget = budget_unlimited();
  ctx.level_arity = heap_nat_of_u64(heap, 3);
  ctx.levels_checked = false;
  return ctx;
}

/* C.Local{name, QMany, typed(index)} as a heap cell for a List<Local>. */
static Local *local_of(Heap *heap, const char *name, uint64_t index) {
  Local *x = heap_alloc(heap, sizeof *x);
  if (x != NULL) *x = context_local(str_lit(name), QUANTITY_MANY, typed(heap, index));
  return x;
}

/* [x] */
static Vec locals_of(Heap *heap, Local *x) {
  Vec xs = vec_new(heap->arena);
  if (x == NULL || !vec_push(&xs, x)) heap->oom = true;
  return xs;
}

/* Nat.read.max() = Nat.mul(16777215, 16777217) = 2^48 - 1. */
static Nat read_max(Heap *heap) { return heap_nat_of_u64(heap, 281474976710655ull); }

static size_t cases(Heap *heap, Fixture *out) {
  Vec extra = locals_of(heap, local_of(heap, "extra", 2));
  Vec extra_greater = locals_of(heap, local_of(heap, "extra", 2));
  Vec none = vec_new(heap->arena);
  Vec newest = locals_of(heap, local_of(heap, "newest", 2));
  out[0] = fixture("size1-equal", one(heap), heap_nat_of_u64(heap, 1), missing());
  out[1] = fixture("size1-greater", one(heap), heap_nat_of_u64(heap, 2), missing());
  out[2] = fixture("size1-max-level", one(heap), read_max(heap), missing());
  out[3] = fixture("size0-empty", empty(heap), nat_zero(), missing());
  out[4] = fixture("size0-extra-local", raw(heap, 0, &extra), nat_zero(), missing());
  out[5] = fixture("size0-extra-local-greater", raw(heap, 0, &extra_greater), heap_nat_of_u64(heap, 1), missing());
  out[6] = fixture("size3-equal", three(heap), heap_nat_of_u64(heap, 3), missing());
  out[7] = fixture("size3-greater", three(heap), heap_nat_of_u64(heap, 4), missing());
  out[8] = fixture("size1-valid", one(heap), nat_zero(), universe_variable(heap, 0));
  out[9] = fixture("size3-oldest", three(heap), nat_zero(), universe_variable(heap, 0));
  out[10] = fixture("size3-middle", three(heap), heap_nat_of_u64(heap, 1), universe_variable(heap, 1));
  out[11] = fixture("size3-newest", three(heap), heap_nat_of_u64(heap, 2), universe_variable(heap, 2));
  out[12] = fixture("size3-missing-list", raw(heap, 3, &none), nat_zero(), missing());
  out[13] = fixture("size3-short-list-present", raw(heap, 3, &newest), heap_nat_of_u64(heap, 2),
                    universe_variable(heap, 2));
  return FIXTURE_COUNT;
}

/* (Missing, Fail{Unbound{message}}) -> message is the out-of-context text;
 * (UniverseVariable{index}, Done{VUniv{LVar{actual}}}) -> index == actual; anything else -> false. */
static bool missing_matches(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_UNBOUND &&
         str_eq(result.error.message, str_lit("a local level is outside the context"));
}

static bool universe_variable_matches(Nat index, Result result) {
  if (result.kind != RESULT_DONE || result.value == NULL) return false;
  const Value *value = result.value;
  return value->kind == VALUE_UNIV && value->level != NULL && value->level->kind == LEVEL_VAR_VAR &&
         nat_cmp(index, value->level->index) == 0;
}

static bool result_matches(Expected expected, Result result) {
  switch (expected.kind) {
  case EXPECTED_MISSING:
    return missing_matches(result);
  case EXPECTED_UNIVERSE_VARIABLE:
    return universe_variable_matches(expected.index, result);
  }
  return false;
}

static bool finite_result_matches(bool same_fuel, Expected expected, Result result) {
  if (same_fuel) return result_matches(expected, result);
  return false;
}

/* Pair{Polls{remaining}, result} -> fuel == remaining and the result matches; other budgets -> false. */
static bool finite_matches(Heap *heap, uint64_t fuel, Expected expected, BudgetState state) {
  switch (state.budget.kind) {
  case BUDGET_POLLS:
    return finite_result_matches(nat_cmp(heap_nat_of_u64(heap, fuel), state.budget.remaining) == 0, expected,
                                 state.result);
  case BUDGET_UNLIMITED:
    return false;
  }
  return false;
}

/* scripted(answers): each poll answers the head and continues with the tail; an empty script
 * answers true forever. */
typedef struct {
  const bool *answers;
  size_t len;
} Script;

static Callback scripted(Heap *heap, const bool *answers, size_t len);

static CallbackPoll *scripted_poll(Heap *heap, void *ctx) {
  Script *s = ctx;
  if (s == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = s->len == 0 ? true : s->answers[0];
  out->next = s->len == 0 ? scripted(heap, NULL, 0) : scripted(heap, s->answers + 1, s->len - 1);
  return out;
}

static Callback scripted(Heap *heap, const bool *answers, size_t len) {
  Script *s = heap_alloc(heap, sizeof *s);
  if (s != NULL) {
    s->answers = answers;
    s->len = len;
  }
  return budget_of_poll(scripted_poll, s);
}

static bool polled_answer(CallbackPoll *state) { return state != NULL && state->spent; }

static bool next_answer(Heap *heap, Callback callback) { return polled_answer(callback.poll(heap, callback.ctx)); }

static bool callback_result_matches(Heap *heap, bool same_result, Callback callback) {
  if (same_result) return next_answer(heap, callback);
  return false;
}

static bool callback_matches(Heap *heap, Expected expected, CallbackState state) {
  return callback_result_matches(heap, result_matches(expected, state.result), state.callback);
}

/* IO.die(1, "HEAD-TY-INVALID-LEVEL FAIL " ++ name ++ suffix) on a failed check; 0 = IO.pure(Unit). */
static int require(Heap *heap, bool ok, const char *name, const char *suffix) {
  if (heap->oom) return driver_host_failure();
  if (ok) return 0;
  char message[128];
  snprintf(message, sizeof message, "HEAD-TY-INVALID-LEVEL FAIL %s%s", name, suffix);
  return driver_die(1, message);
}

static const bool script_tf[] = {true, false};

static Comp head_ty(Heap *heap, Fixture f) { return check_engine_head_ty(heap, f.context, value_hlocal(f.level)); }

static int run_case(Heap *heap, Fixture f) {
  int unlimited =
      require(heap, result_matches(f.expected, budget_run(heap, head_ty(heap, f), budget_unlimited())), f.name,
              "/unlimited");
  if (unlimited != 0) return unlimited;
  int zero = require(heap,
                     finite_matches(heap, 0, f.expected,
                                    budget_run_state(heap, head_ty(heap, f), budget_of_polls(nat_zero()))),
                     f.name, "/fuel0");
  if (zero != 0) return zero;
  int one_poll = require(
      heap,
      finite_matches(heap, 1, f.expected,
                     budget_run_state(heap, head_ty(heap, f), budget_of_polls(heap_nat_of_u64(heap, 1)))),
      f.name, "/fuel1");
  if (one_poll != 0) return one_poll;
  return require(heap,
                 callback_matches(heap, f.expected,
                                  budget_run_poll_state(heap, head_ty(heap, f), scripted(heap, script_tf, 2))),
                 f.name, "/callback");
}

static int run_cases(Heap *heap, const Fixture *fixtures, size_t count, size_t index) {
  if (index >= count) return driver_print(str_lit("HEAD-TY-INVALID-LEVEL OK cases=14 observations=56"));
  int checked = run_case(heap, fixtures[index]);
  if (checked != 0) return checked;
  return run_cases(heap, fixtures, count, index + 1);
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Fixture fixtures[FIXTURE_COUNT];
  size_t count = cases(&heap, fixtures);
  int code = heap.oom ? driver_host_failure() : run_cases(&heap, fixtures, count, 0);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
