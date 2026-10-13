/* Port of bend2/kernel/budget.bend: the poll budget, the Comp continuation monad and its runners.
 * Bend closures become a function pointer with a heap allocated context; the tail recursive
 * runners (run_step, run_poll_step, run_io_step) become loops over the same states. */
#include "kernel.h"

/* Finished step returned when the heap cannot hold a new step. */
static Step step_oom = {
    STEP_FINISHED,
    {RESULT_FAIL, NULL, {ERROR_NOT_YET, {"host out of memory", 18}, {NULL, 0}, {NULL, 0}}},
    {NULL, NULL}};

Budget budget_unlimited(void) {
  Budget b;
  b.kind = BUDGET_UNLIMITED;
  b.remaining = nat_zero();
  return b;
}

Budget budget_of_polls(Nat remaining) {
  Budget b;
  b.kind = BUDGET_POLLS;
  b.remaining = remaining;
  return b;
}

/* p of Polls{1n+p} */
static Nat predecessor(Heap *heap, Nat n) {
  NatSub sub = nat_sub(heap->arena, n, heap_nat_of_u64(heap, 1));
  switch (sub.status) {
    case NAT_SUB_OK:
      return sub.value;
    case NAT_SUB_OOM:
      heap->oom = true;
      return nat_zero();
    case NAT_SUB_UNDERFLOW:
      return nat_zero();
  }
  return nat_zero();
}

MaybeBudget budget_poll(Heap *heap, Budget budget) {
  MaybeBudget out;
  out.some = false;
  out.value = budget;
  switch (budget.kind) {
    case BUDGET_UNLIMITED:
      out.some = true;
      out.value = budget_unlimited();
      return out;
    case BUDGET_POLLS:
      if (nat_is_zero(budget.remaining)) return out;
      out.some = true;
      out.value = budget_of_polls(predecessor(heap, budget.remaining));
      return out;
  }
  return out;
}

bool budget_exhausted(Budget budget) {
  switch (budget.kind) {
    case BUDGET_POLLS:
      return nat_is_zero(budget.remaining);
    case BUDGET_UNLIMITED:
      return false;
  }
  return false;
}

Result budget_result_done(void *value) {
  Result r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_make(ERROR_NOT_YET, str_empty());
  return r;
}

Result budget_result_fail(Error error) {
  Result r;
  r.kind = RESULT_FAIL;
  r.value = NULL;
  r.error = error;
  return r;
}

/* Steps are one-shot: the run loops read each one once and return it to the heap free list.
 * The static step_oom is never recycled. */
static Step *step_finished(Heap *heap, Result result) {
  Step *step = heap_alloc_small(heap, sizeof *step);
  if (step == NULL) return &step_oom;
  step->kind = STEP_FINISHED;
  step->result = result;
  step->resume.fn = NULL;
  step->resume.ctx = NULL;
  return step;
}

static Step *step_poll(Heap *heap, ResumeK resume) {
  Step *step = heap_alloc_small(heap, sizeof *step);
  if (step == NULL) return &step_oom;
  step->kind = STEP_POLL;
  step->result = budget_result_done(NULL);
  step->resume = resume;
  return step;
}

static void step_release(Heap *heap, Step *step) {
  if (step != &step_oom) heap_release_small(heap, step, sizeof *step);
}

/* Reads the result of a finished step and releases the step. */
static Result step_result(Heap *heap, Step *step) {
  Result result = step->result;
  step_release(heap, step);
  return result;
}

static Step *call_done(Heap *heap, DoneK done, Result result) { return done.fn(heap, done.ctx, result); }

static Comp comp_later(LaterK action) {
  Comp c;
  c.kind = COMP_LATER;
  c.value = NULL;
  c.error = error_make(ERROR_NOT_YET, str_empty());
  c.action = action;
  return c;
}

Step *budget_start_comp(Heap *heap, Comp action, DoneK done) {
  switch (action.kind) {
    case COMP_READY:
      return call_done(heap, done, budget_result_done(action.value));
    case COMP_ERROR:
      return call_done(heap, done, budget_result_fail(action.error));
    case COMP_LATER:
      return action.action.fn(heap, action.action.ctx, done);
  }
  return step_finished(heap, budget_result_fail(error_host_oom()));
}

Comp budget_pure(void *value) {
  Comp c;
  c.kind = COMP_READY;
  c.value = value;
  c.error = error_make(ERROR_NOT_YET, str_empty());
  c.action.fn = NULL;
  c.action.ctx = NULL;
  return c;
}

Comp budget_failed(Error error) {
  Comp c;
  c.kind = COMP_ERROR;
  c.value = NULL;
  c.error = error;
  c.action.fn = NULL;
  c.action.ctx = NULL;
  return c;
}

Step *budget_bind_result(Heap *heap, Result first, NextK next, DoneK done) {
  switch (first.kind) {
    case RESULT_FAIL:
      return call_done(heap, done, first);
    case RESULT_DONE:
      return budget_start_comp(heap, next.fn(heap, next.ctx, first.value), done);
  }
  return call_done(heap, done, first);
}

typedef struct {
  LaterK action;
  NextK next;
} BindCtx;

typedef struct {
  NextK next;
  DoneK done;
} BindDoneCtx;

/* The done-side records (BindDoneCtx, TickResumeCtx, RecoverDoneCtx, the attempt DoneK box) are
 * built once per start of an action and consumed by the one continuation call that reads them,
 * so each is returned to the heap free list at that call, after its fields are copied out. */
static Step *bind_done(Heap *heap, void *ctx, Result result) {
  BindDoneCtx *c = ctx;
  NextK next = c->next;
  DoneK done = c->done;
  heap_release_small(heap, c, sizeof *c);
  return budget_bind_result(heap, result, next, done);
}

static Step *bind_action(Heap *heap, void *ctx, DoneK done) {
  BindCtx *c = ctx;
  BindDoneCtx *inner = heap_alloc_small(heap, sizeof *inner);
  if (inner == NULL) return call_done(heap, done, budget_result_fail(error_host_oom()));
  inner->next = c->next;
  inner->done = done;
  DoneK k;
  k.fn = bind_done;
  k.ctx = inner;
  return c->action.fn(heap, c->action.ctx, k);
}

Comp budget_bind(Heap *heap, Comp first, NextK next) {
  switch (first.kind) {
    case COMP_READY:
      /* A ready value needs no suspension. Only tick yields Poll, so the poll order is unchanged. */
      return next.fn(heap, next.ctx, first.value);
    case COMP_ERROR:
      return first;
    case COMP_LATER: {
      BindCtx *c = heap_alloc(heap, sizeof *c);
      if (c == NULL) return budget_failed(error_host_oom());
      c->action = first.action;
      c->next = next;
      LaterK k;
      k.fn = bind_action;
      k.ctx = c;
      return comp_later(k);
    }
  }
  return first;
}

Step *budget_tick_result(Heap *heap, bool spent, Str message, DoneK done) {
  if (spent) return call_done(heap, done, budget_result_fail(error_make(ERROR_BUDGET_EXHAUSTED, message)));
  return call_done(heap, done, budget_result_done(kernel_unit()));
}

typedef struct {
  Str message;
} TickCtx;

typedef struct {
  Str message;
  DoneK done;
} TickResumeCtx;

static Step *tick_resume(Heap *heap, void *ctx, bool spent) {
  TickResumeCtx *c = ctx;
  Str message = c->message;
  DoneK done = c->done;
  heap_release_small(heap, c, sizeof *c);
  return budget_tick_result(heap, spent, message, done);
}

static Step *tick_action(Heap *heap, void *ctx, DoneK done) {
  TickCtx *c = ctx;
  TickResumeCtx *r = heap_alloc_small(heap, sizeof *r);
  if (r == NULL) return call_done(heap, done, budget_result_fail(error_host_oom()));
  r->message = c->message;
  r->done = done;
  ResumeK k;
  k.fn = tick_resume;
  k.ctx = r;
  return step_poll(heap, k);
}

Comp budget_tick_with(Heap *heap, Str message) {
  TickCtx *c = heap_alloc(heap, sizeof *c);
  if (c == NULL) return budget_failed(error_host_oom());
  c->message = message;
  LaterK k;
  k.fn = tick_action;
  k.ctx = c;
  return comp_later(k);
}

Comp budget_tick(Heap *heap) { return budget_tick_with(heap, str_lit("the check budget is exhausted")); }

Comp budget_lift(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return budget_pure(result.value);
    case RESULT_FAIL:
      return budget_failed(result.error);
  }
  return budget_failed(result.error);
}

/* Resumes a poll step; the step is released before its continuation runs. */
static Step *resume_with(Heap *heap, Step *step, bool spent) {
  ResumeK resume = step->resume;
  step_release(heap, step);
  return resume.fn(heap, resume.ctx, spent);
}

/* @unsafe def run_step: the tail recursion over polls as a loop. */
BudgetState budget_run_step(Heap *heap, Step *step, Budget budget) {
  Step *cur = step;
  Budget b = budget;
  while (cur->kind == STEP_POLL) {
    switch (b.kind) {
      case BUDGET_UNLIMITED:
        cur = resume_with(heap, cur, false);
        break;
      case BUDGET_POLLS:
        if (nat_is_zero(b.remaining)) {
          cur = resume_with(heap, cur, true);
        } else {
          Nat remaining = predecessor(heap, b.remaining);
          cur = resume_with(heap, cur, false);
          b = budget_of_polls(remaining);
        }
        break;
    }
  }
  BudgetState state;
  state.budget = b;
  state.result = step_result(heap, cur);
  return state;
}

static Step *finished_done(Heap *heap, void *ctx, Result result) {
  (void)ctx;
  return step_finished(heap, result);
}

static DoneK finished_k(void) {
  DoneK k;
  k.fn = finished_done;
  k.ctx = NULL;
  return k;
}

BudgetState budget_run_state(Heap *heap, Comp action, Budget budget) {
  return budget_run_step(heap, budget_start_comp(heap, action, finished_k()), budget);
}

Result budget_discard(BudgetState state) { return state.result; }

Result budget_run(Heap *heap, Comp action, Budget budget) { return budget_discard(budget_run_state(heap, action, budget)); }

Comp budget_comp_pure(void *value) { return budget_pure(value); }

Comp budget_comp_bind(Heap *heap, Comp first, NextK next) { return budget_bind(heap, first, next); }

Step *budget_recover_result(Heap *heap, Result result, HandlerK handler, DoneK done) {
  switch (result.kind) {
    case RESULT_DONE:
      return call_done(heap, done, result);
    case RESULT_FAIL:
      return budget_start_comp(heap, handler.fn(heap, handler.ctx, result.error), done);
  }
  return call_done(heap, done, result);
}

typedef struct {
  LaterK action;
  HandlerK handler;
} RecoverCtx;

typedef struct {
  HandlerK handler;
  DoneK done;
} RecoverDoneCtx;

static Step *recover_done(Heap *heap, void *ctx, Result result) {
  RecoverDoneCtx *c = ctx;
  HandlerK handler = c->handler;
  DoneK done = c->done;
  heap_release_small(heap, c, sizeof *c);
  return budget_recover_result(heap, result, handler, done);
}

static Step *recover_action(Heap *heap, void *ctx, DoneK done) {
  RecoverCtx *c = ctx;
  RecoverDoneCtx *inner = heap_alloc_small(heap, sizeof *inner);
  if (inner == NULL) return call_done(heap, done, budget_result_fail(error_host_oom()));
  inner->handler = c->handler;
  inner->done = done;
  DoneK k;
  k.fn = recover_done;
  k.ctx = inner;
  return c->action.fn(heap, c->action.ctx, k);
}

Comp budget_recover(Heap *heap, Comp action, HandlerK handler) {
  switch (action.kind) {
    case COMP_READY:
      return action;
    case COMP_ERROR:
      return handler.fn(heap, handler.ctx, action.error);
    case COMP_LATER: {
      RecoverCtx *c = heap_alloc(heap, sizeof *c);
      if (c == NULL) return budget_failed(error_host_oom());
      c->action = action.action;
      c->handler = handler;
      LaterK k;
      k.fn = recover_action;
      k.ctx = c;
      return comp_later(k);
    }
  }
  return action;
}

static Result *box_result(Heap *heap, Result result) {
  Result *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed != NULL) *boxed = result;
  return boxed;
}

static Step *attempt_done(Heap *heap, void *ctx, Result result) {
  DoneK *outer = ctx;
  DoneK done = *outer;
  heap_release_small(heap, outer, sizeof *outer);
  Result *boxed = box_result(heap, result);
  if (boxed == NULL) return call_done(heap, done, budget_result_fail(error_host_oom()));
  return call_done(heap, done, budget_result_done(boxed));
}

static Step *attempt_action(Heap *heap, void *ctx, DoneK done) {
  LaterK *action = ctx;
  DoneK *outer = heap_alloc_small(heap, sizeof *outer);
  if (outer == NULL) return call_done(heap, done, budget_result_fail(error_host_oom()));
  *outer = done;
  DoneK k;
  k.fn = attempt_done;
  k.ctx = outer;
  return action->fn(heap, action->ctx, k);
}

static Comp pure_boxed(Heap *heap, Result result) {
  Result *boxed = box_result(heap, result);
  if (boxed == NULL) return budget_failed(error_host_oom());
  return budget_pure(boxed);
}

Comp budget_attempt(Heap *heap, Comp action) {
  switch (action.kind) {
    case COMP_READY:
      return pure_boxed(heap, budget_result_done(action.value));
    case COMP_ERROR:
      return pure_boxed(heap, budget_result_fail(action.error));
    case COMP_LATER: {
      LaterK *inner = heap_alloc(heap, sizeof *inner);
      if (inner == NULL) return budget_failed(error_host_oom());
      *inner = action.action;
      LaterK k;
      k.fn = attempt_action;
      k.ctx = inner;
      return comp_later(k);
    }
  }
  return action;
}

Callback budget_of_poll(CallbackPoll *(*poll)(Heap *heap, void *ctx), void *ctx) {
  Callback cb;
  cb.poll = poll;
  cb.ctx = ctx;
  return cb;
}

static CallbackState callback_state(Callback callback, Result result) {
  CallbackState state;
  state.callback = callback;
  state.result = result;
  return state;
}

/* @unsafe def resume_poll: (next, spent) = response; run_poll_step(resume(spent), next) */
CallbackState budget_resume_poll(Heap *heap, ResumeK resume, CallbackPoll *response) {
  return budget_run_poll_step(heap, resume.fn(heap, resume.ctx, response->spent), response->next);
}

/* @unsafe def run_poll_step: the mutual tail recursion with resume_poll as a loop. */
CallbackState budget_run_poll_step(Heap *heap, Step *step, Callback callback) {
  Step *cur = step;
  Callback cb = callback;
  while (cur->kind == STEP_POLL) {
    CallbackPoll *response = cb.poll(heap, cb.ctx);
    if (response == NULL) {
      heap->oom = true;
      return callback_state(cb, budget_result_fail(error_host_oom()));
    }
    cur = resume_with(heap, cur, response->spent);
    cb = response->next;
  }
  return callback_state(cb, step_result(heap, cur));
}

CallbackState budget_run_poll_state(Heap *heap, Comp action, Callback callback) {
  return budget_run_poll_step(heap, budget_start_comp(heap, action, finished_k()), callback);
}

Result budget_discard_poll(CallbackState state) { return state.result; }

Result budget_run_poll(Heap *heap, Comp action, Callback callback) {
  return budget_discard_poll(budget_run_poll_state(heap, action, callback));
}

IOCallback budget_of_poll_io(IOCallbackPoll *(*poll)(Heap *heap, void *ctx), void *ctx) {
  IOCallback cb;
  cb.poll = poll;
  cb.ctx = ctx;
  return cb;
}

static IOCallbackState io_callback_state(IOCallback callback, Result result) {
  IOCallbackState state;
  state.callback = callback;
  state.result = result;
  return state;
}

IOCallbackState budget_resume_io(Heap *heap, ResumeK resume, IOCallbackPoll *response) {
  return budget_run_io_step(heap, resume.fn(heap, resume.ctx, response->spent), response->next);
}

/* @unsafe def run_io_step: IO.bind(poll(Unit{}), response => resume_io(...)) as a loop. */
IOCallbackState budget_run_io_step(Heap *heap, Step *step, IOCallback callback) {
  Step *cur = step;
  IOCallback cb = callback;
  while (cur->kind == STEP_POLL) {
    IOCallbackPoll *response = cb.poll(heap, cb.ctx);
    if (response == NULL) {
      heap->oom = true;
      return io_callback_state(cb, budget_result_fail(error_host_oom()));
    }
    cur = resume_with(heap, cur, response->spent);
    cb = response->next;
  }
  return io_callback_state(cb, step_result(heap, cur));
}

IOCallbackState budget_run_io_state(Heap *heap, Comp action, IOCallback callback) {
  return budget_run_io_step(heap, budget_start_comp(heap, action, finished_k()), callback);
}

Result budget_discard_io(IOCallbackState state) { return state.result; }

Result budget_run_io(Heap *heap, Comp action, IOCallback callback) {
  return budget_discard_io(budget_run_io_state(heap, action, callback));
}
