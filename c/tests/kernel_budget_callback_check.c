/* Port of tests/kernel_budget_callback_check.bend (oracle label unit-kernel-budget-callback-check).
 * The Test.* helpers are the statics of c/tests/kernel_budget_callbacks.c (scripted, deadline, refused,
 * next_answer) and Dag.term_dag is the static of c/tests/kernel_budget_dag.c, copied here. */
#include "driver.h"

/* cancel_after(remaining): 0 -> every poll answers (cancel_after(0), True); 1+rest -> (cancel_after(rest), False). */
typedef struct {
  uint64_t remaining;
} Cancel;

static Callback cancel_after(Heap *heap, uint64_t remaining);

static CallbackPoll *cancel_after_poll(Heap *heap, void *ctx) {
  Cancel *c = ctx;
  if (c == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = c->remaining == 0;
  out->next = cancel_after(heap, c->remaining == 0 ? 0 : c->remaining - 1);
  return out;
}

static Callback cancel_after(Heap *heap, uint64_t remaining) {
  Cancel *c = heap_alloc(heap, sizeof *c);
  if (c != NULL) c->remaining = remaining;
  return budget_of_poll(cancel_after_poll, c);
}

/* Test.scripted(answers): each poll answers the head and continues with the tail; an empty script
 * answers true (spent) forever. */
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

/* Test.deadline(expires): the driver owns clock reads; cancellation is observed at a kernel poll. */
typedef struct {
  uint64_t expires;
} Deadline;

static IOCallback deadline(Heap *heap, uint64_t expires);

static IOCallbackPoll *deadline_poll(Heap *heap, void *ctx) {
  Deadline *d = ctx;
  if (d == NULL) return NULL;
  IOCallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  uint64_t now = driver_now_ms();
  out->next = deadline(heap, d->expires);
  out->spent = d->expires <= now;
  return out;
}

static IOCallback deadline(Heap *heap, uint64_t expires) {
  Deadline *d = heap_alloc(heap, sizeof *d);
  if (d != NULL) d->expires = expires;
  return budget_of_poll_io(deadline_poll, d);
}

/* Test.refused(result): Fail{Budget_exhausted} -> True. */
static bool refused(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED; }

static bool answer(CallbackPoll *response) { return response != NULL && response->spent; }

/* Test.next_answer(callback): the next poll answers spent. */
static bool next_answer(Heap *heap, Callback callback) { return answer(callback.poll(heap, callback.ctx)); }

/* Dag.term_dag(depth): a term DAG of the given depth, each Ran sharing one prior term twice. */
static const Term *term_dag(Heap *heap, uint64_t depth) {
  if (depth == 0) return term_univ(heap, level_zero());
  const Term *prior = term_dag(heap, depth - 1);
  return term_ran(heap, shape_spi(QUANTITY_ZERO, str_lit("x"), (void *)prior), prior);
}

/* comparison() = Eq.decide_m(L.imax(L.var(0n), L.succ(L.var(1n))), L.max(L.var(0n), L.succ(L.var(1n))), True{}) */
static Comp comparison(Heap *heap) {
  const LevelVar *left = level_imax(heap, level_var(heap, nat_zero()), level_succ(heap, level_var(heap, heap_nat_of_u64(heap, 1))));
  const LevelVar *right = level_max(heap, level_var(heap, nat_zero()), level_succ(heap, level_var(heap, heap_nat_of_u64(heap, 1))));
  return level_eq_decide_m(heap, left, right, true);
}

/* accepted(result): Done{True} -> True. */
static bool accepted(Result result) { return result.kind == RESULT_DONE && kernel_bool_unbox(result.value); }

/* Nat.sub saturates at zero. */
static uint64_t nat_sub_u64(Heap *heap, uint64_t a, Nat b) {
  NatSub diff = nat_sub(heap->arena, heap_nat_of_u64(heap, a), b);
  switch (diff.status) {
    case NAT_SUB_OK: {
      NatToU64 small = nat_to_u64(diff.value);
      return small.status == NAT_TO_U64_OK ? small.value : 0;
    }
    case NAT_SUB_UNDERFLOW:
      return 0;
    case NAT_SUB_OOM:
      heap->oom = true;
      return 0;
  }
  return 0;
}

/* comparison_boundary(state): Pair{Polls{remaining}, Done{True}} -> used = 10000 - remaining; 20 < used and
 * the comparison succeeds with cancel_after(used) and is refused with cancel_after(used - 1) and cancel_after(20). */
static bool comparison_boundary(Heap *heap, BudgetState state) {
  if (state.budget.kind != BUDGET_POLLS || !accepted(state.result)) return false;
  uint64_t used = nat_sub_u64(heap, 10000, state.budget.remaining);
  return 20 < used && accepted(budget_run_poll(heap, comparison(heap), cancel_after(heap, used))) &&
         refused(budget_run_poll(heap, comparison(heap), cancel_after(heap, used == 0 ? 0 : used - 1))) &&
         refused(budget_run_poll(heap, comparison(heap), cancel_after(heap, 20)));
}

/* proof_probe() = K.conv_is_prop(Context.make(G.initial(), B.unlimited(), 0n), V.VNeutral{V.HGlobal{"absent"}, []}) */
static Comp proof_probe(Heap *heap) {
  Ctx ctx = context_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  return check_engine_conv_is_prop(heap, ctx, value_vneutral(heap, value_hglobal(str_lit("absent")), vec_new(heap->arena)));
}

/* proof_false(result): Done{False} -> True. */
static bool proof_false(Result result) { return result.kind == RESULT_DONE && !kernel_bool_unbox(result.value); }

/* proof_state((next, result)) = proof_false(result) && Test.next_answer(next) */
static bool proof_state(Heap *heap, CallbackState state) { return proof_false(state.result) && next_answer(heap, state.callback); }

/* proof_remainder(state): Pair{Polls{0n}, Done{False}} -> True. */
static bool proof_remainder(BudgetState state) {
  return state.budget.kind == BUDGET_POLLS && nat_is_zero(state.budget.remaining) && proof_false(state.result);
}

/* after_probe(): result <- proof_probe(); tick <- B.tick(); return result */
static Comp after_probe_return(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)value;
  return budget_pure(ctx);
}

static Comp after_probe_tick(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  NextK next;
  next.fn = after_probe_return;
  next.ctx = value;
  return budget_bind(heap, budget_tick(heap), next);
}

static Comp after_probe(Heap *heap) {
  NextK next;
  next.fn = after_probe_tick;
  next.ctx = NULL;
  return budget_bind(heap, proof_probe(heap), next);
}

/* totality_message(result): Fail{Budget_exhausted{message}} -> message == "the totality budget is exhausted". */
static bool totality_message(Result result) {
  return refused(result) && str_eq(result.error.message, str_lit("the totality budget is exhausted"));
}

static const bool script_fftf[] = {false, false, true, false};
static const bool script_tff[] = {true, false, false};
static const bool script_ftf[] = {false, true, false};
static const bool script_fft[] = {false, false, true};

static bool suite(Heap *heap) {
  Ctx ctx = context_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  return comparison_boundary(heap, budget_run_state(heap, comparison(heap), budget_of_polls(heap_nat_of_u64(heap, 10000)))) &&
         proof_state(heap, budget_run_poll_state(heap, proof_probe(heap), scripted(heap, script_fftf, 4))) &&
         refused(budget_run_poll(heap, proof_probe(heap), scripted(heap, script_tff, 3))) &&
         refused(budget_run_poll(heap, proof_probe(heap), scripted(heap, script_ftf, 3))) &&
         proof_remainder(budget_run_state(heap, proof_probe(heap), budget_of_polls(heap_nat_of_u64(heap, 2)))) &&
         refused(budget_run_poll(heap, after_probe(heap), scripted(heap, script_fft, 3))) &&
         refused(budget_run_poll(heap, check_engine_infer_uses(heap, ctx, QUANTITY_ZERO, term_dag(heap, 40)), cancel_after(heap, 100))) &&
         totality_message(budget_run_poll(
             heap,
             totality_guard_comp(heap, global_initial(heap), str_lit("constant"), term_global(heap, str_lit("Nat")),
                                 term_global(heap, str_lit("Nat"))),
             cancel_after(heap, 0)));
}

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS callback kernel comparison, conversion recovery, scope and totality"));
  return driver_die(1, "FAIL kernel callback integration");
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool checks = suite(&heap);
  uint64_t expires = driver_now_ms();
  Result expired = budget_run_io(&heap, comparison(&heap), deadline(&heap, expires));
  int code = heap.oom ? driver_host_failure() : verify(checks && refused(expired));
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
