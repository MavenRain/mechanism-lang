/* Port of tests/kernel_budget_callbacks.bend (oracle label unit-kernel-budget-callbacks). */
#include "driver.h"

/* scripted(answers): each poll answers the head and continues with the tail; an empty script
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

static IOCallback scripted_io(Heap *heap, const bool *answers, size_t len);

static IOCallbackPoll *scripted_io_poll(Heap *heap, void *ctx) {
  Script *s = ctx;
  if (s == NULL) return NULL;
  IOCallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = s->len == 0 ? true : s->answers[0];
  out->next = s->len == 0 ? scripted_io(heap, NULL, 0) : scripted_io(heap, s->answers + 1, s->len - 1);
  return out;
}

static IOCallback scripted_io(Heap *heap, const bool *answers, size_t len) {
  Script *s = heap_alloc(heap, sizeof *s);
  if (s != NULL) {
    s->answers = answers;
    s->len = len;
  }
  return budget_of_poll_io(scripted_io_poll, s);
}

/* The driver owns clock reads. Cancellation is observed at a kernel poll;
 * this does not preempt work performed between polls. */
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

static Comp ticks(Heap *heap, uint64_t count);

static Comp ticks_rest(Heap *heap, void *ctx, void *value) {
  (void)value;
  uint64_t *rest = ctx;
  return ticks(heap, *rest);
}

/* ticks(count): count ticks in sequence, each bound lazily after the one before. */
static Comp ticks(Heap *heap, uint64_t count) {
  if (count == 0) return budget_pure(kernel_unit());
  uint64_t *rest = heap_alloc(heap, sizeof *rest);
  if (rest == NULL) return budget_failed(error_host_oom());
  *rest = count - 1;
  NextK next;
  next.fn = ticks_rest;
  next.ctx = rest;
  return budget_bind(heap, budget_tick(heap), next);
}

static bool success(Result result) { return result.kind == RESULT_DONE; }

static bool refused(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED; }

static bool answer(CallbackPoll *response) { return response != NULL && response->spent; }

static bool next_answer(Heap *heap, Callback callback) { return answer(callback.poll(heap, callback.ctx)); }

static bool succeeded_at_next_poll(Heap *heap, CallbackState state) {
  return success(state.result) && next_answer(heap, state.callback);
}

static bool refused_at_next_poll(Heap *heap, CallbackState state) {
  return refused(state.result) && next_answer(heap, state.callback);
}

/* C.Pair{B.Polls{2n}, Done{Unit{}}} */
static bool remaining(BudgetState state) {
  NatToU64 left = nat_to_u64(state.budget.remaining);
  return state.budget.kind == BUDGET_POLLS && left.status == NAT_TO_U64_OK && left.value == 2 &&
         state.result.kind == RESULT_DONE;
}

static Comp tick_handler(Heap *heap, void *ctx, Error error) {
  (void)ctx;
  (void)error;
  return budget_tick(heap);
}

static HandlerK tick_handler_k(void) {
  HandlerK k;
  k.fn = tick_handler;
  k.ctx = NULL;
  return k;
}

static Comp recovered(Heap *heap) { return budget_recover(heap, budget_tick(heap), tick_handler_k()); }

static Comp must_not_run(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return budget_failed(error_make(ERROR_UNBOUND, str_lit("must not run")));
}

static Comp after_failure(Heap *heap) {
  NextK next;
  next.fn = must_not_run;
  next.ctx = NULL;
  return budget_bind(heap, budget_tick(heap), next);
}

static bool attempted(Result result) {
  if (result.kind != RESULT_DONE || result.value == NULL) return false;
  Result *inner = result.value;
  return refused(*inner);
}

static bool continue_run(Heap *heap, CallbackState state) {
  return success(state.result) && refused_at_next_poll(heap, budget_run_poll_state(heap, ticks(heap, 2), state.callback));
}

static Comp recovered_error(Heap *heap) {
  return budget_recover(heap, budget_failed(error_make(ERROR_UNBOUND, str_lit("probe"))), tick_handler_k());
}

static const bool script_ffttf[] = {false, false, true, false};
static const bool script_tf[] = {true, false};
static const bool script_tftf[] = {true, false, true, false};
static const bool script_ftf[] = {false, true, false};
static const bool script_t[] = {true};
static const bool script_ffftt_f[] = {false, false, false, true, true, false};

static bool pure_suite(Heap *heap) {
  return remaining(budget_run_state(heap, ticks(heap, 3), budget_of_polls(heap_nat_of_u64(heap, 5)))) &&
         success(budget_run(heap, ticks(heap, 3), budget_of_polls(heap_nat_of_u64(heap, 3)))) &&
         refused(budget_run(heap, ticks(heap, 3), budget_of_polls(heap_nat_of_u64(heap, 2)))) &&
         succeeded_at_next_poll(heap, budget_run_poll_state(heap, ticks(heap, 2), scripted(heap, script_ffttf, 4))) &&
         succeeded_at_next_poll(heap, budget_run_poll_state(heap, budget_pure(kernel_unit()), scripted(heap, script_tf, 2))) &&
         refused(budget_run_poll(heap, ticks(heap, 1000000), scripted(heap, script_tf, 2))) &&
         refused(budget_run_poll(heap, after_failure(heap), scripted(heap, script_tf, 2))) &&
         succeeded_at_next_poll(heap, budget_run_poll_state(heap, recovered(heap), scripted(heap, script_tftf, 4))) &&
         refused(budget_run(heap, recovered(heap), budget_of_polls(nat_zero()))) &&
         succeeded_at_next_poll(heap, budget_run_poll_state(heap, recovered_error(heap), scripted(heap, script_ftf, 3))) &&
         attempted(budget_run_poll(heap, budget_attempt(heap, budget_tick(heap)), scripted(heap, script_t, 1))) &&
         continue_run(heap, budget_run_poll_state(heap, ticks(heap, 2), scripted(heap, script_ffftt_f, 6)));
}

static bool io_continue(Heap *heap, IOCallbackState state) {
  Result after = budget_run_io(heap, budget_tick(heap), state.callback);
  return success(state.result) && refused(after);
}

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS budget callbacks, exact polls, recovery, laziness and clock deadline"));
  return driver_die(1, "FAIL budget callback regression");
}

static const bool io_script_tft[] = {true, false, true};

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  bool pure_ok = pure_suite(&heap);
  IOCallbackState state = budget_run_io_state(&heap, recovered(&heap), scripted_io(&heap, io_script_tft, 3));
  bool io_ok = io_continue(&heap, state);
  uint64_t expires = driver_now_ms();
  Result expired = budget_run_io(&heap, ticks(&heap, 1000000), deadline(&heap, expires));
  bool ok = pure_ok && io_ok && refused(expired);
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
