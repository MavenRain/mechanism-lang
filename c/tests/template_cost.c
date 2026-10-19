/* Port of tests/template_cost.bend (wrapper _bend2/test/template_cost.exe, gate leg
 * TEMPLATE-COST of dev/gates.sh): `SOURCE [--through MEMBER]` parses SOURCE, requires exactly one
 * family group, keeps its members up to and including MEMBER, elaborates the program from the empty
 * globals under an always-unspent poll callback, counts the polls, and prints one JSON report line;
 * failures die with `TEMPLATE-COST-FAIL MESSAGE` and exit 1. Definitions keep the Bend order and
 * names. */
#include "cli.h"
#include "driver.h"
#include "surface.h"

#include <stdio.h>

/* Result<String, A>: the Done payload sits in the field named for A. */
typedef enum { OUTCOME_DONE, OUTCOME_FAIL } OutcomeKind;

/* Result<String, String> */
typedef struct {
  OutcomeKind kind;
  Str message; /* OUTCOME_FAIL */
  Str text;    /* OUTCOME_DONE */
} Outcome;

/* Result<String, List<RecDef<S.T>>> */
typedef struct {
  OutcomeKind kind;
  Str message; /* OUTCOME_FAIL */
  Vec members; /* OUTCOME_DONE: SRecDef pointers */
} Selected;

/* Result<String, Common.Pair<List<S.Decl>, Nat>> */
typedef struct {
  OutcomeKind kind;
  Str message; /* OUTCOME_FAIL */
  Vec parsed;  /* OUTCOME_DONE: SDecl pointers */
  size_t members;
} Selection;

/* Common.Pair<Nat, Elab.Checked> */
typedef struct {
  uint64_t polls;
  const ElabChecked *checked;
} Counted;

static Vec no_vec(void) {
  Vec none = {0};
  return none;
}

static Outcome outcome_fail(Str message) {
  Outcome out;
  out.kind = OUTCOME_FAIL;
  out.message = message;
  out.text = str_lit("");
  return out;
}

static Outcome outcome_done(Str text) {
  Outcome out;
  out.kind = OUTCOME_DONE;
  out.message = str_lit("");
  out.text = text;
  return out;
}

static Selected selected_fail(Str message) {
  Selected out;
  out.kind = OUTCOME_FAIL;
  out.message = message;
  out.members = no_vec();
  return out;
}

static Selected selected_done(Vec members) {
  Selected out;
  out.kind = OUTCOME_DONE;
  out.message = str_lit("");
  out.members = members;
  return out;
}

static Selection selection_fail(Str message) {
  Selection out;
  out.kind = OUTCOME_FAIL;
  out.message = message;
  out.parsed = no_vec();
  out.members = 0;
  return out;
}

static Str cat(Heap *heap, Str a, Str b) {
  Str s = str_cat(heap->arena, a, b);
  if (s.len != a.len + b.len) heap->oom = true;
  return s;
}

/* unspent (13-14): B.of_poll(unit => (unspent(), False{})). */
static Callback unspent(void);

static CallbackPoll *unspent_poll(Heap *heap, void *ctx) {
  (void)ctx;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->next = unspent();
  out->spent = false;
  return out;
}

static Callback unspent(void) { return budget_of_poll(unspent_poll, NULL); }

/* counted_result (18-21): Fail keeps the error, Done pairs the poll count with the value. */
static Result counted_result(Heap *heap, uint64_t count, Result result) {
  switch (result.kind) {
  case RESULT_FAIL: return result;
  case RESULT_DONE: {
    Counted *pair = heap_alloc(heap, sizeof *pair);
    if (pair == NULL) return budget_result_fail(error_host_oom());
    pair->polls = count;
    pair->checked = result.value;
    return budget_result_done(pair);
  }
  }
  return result;
}

/* count_step (23-26) */
typedef struct {
  ResumeK resume;
  uint64_t count;
  DoneK done;
} CountFrame;

static Step *count_step(Heap *heap, Step *step, uint64_t count, DoneK done);

/* Frames and poll steps are one-shot small blocks, as in budget.c: count_resume releases its
 * frame before the wrapped continuation runs, and count_step releases each inner poll step after
 * it copies the resume (the runner releases the outer step it reads). A poll step is never the
 * static out-of-memory step of budget.c, which is a finished step. */
static Step *count_resume(Heap *heap, void *ctx, bool spent) {
  CountFrame *frame = ctx;
  if (frame == NULL) return NULL;
  ResumeK resume = frame->resume;
  uint64_t count = frame->count;
  DoneK done = frame->done;
  heap_release_small(heap, frame, sizeof *frame);
  return count_step(heap, resume.fn(heap, resume.ctx, spent), count + 1, done);
}

static Step *count_step(Heap *heap, Step *step, uint64_t count, DoneK done) {
  if (step == NULL) return NULL;
  switch (step->kind) {
  case STEP_FINISHED: return done.fn(heap, done.ctx, counted_result(heap, count, step->result));
  case STEP_POLL: {
    CountFrame *frame = heap_alloc_small(heap, sizeof *frame);
    Step *out = heap_alloc_small(heap, sizeof *out);
    if (frame == NULL || out == NULL) return NULL;
    frame->resume = step->resume;
    heap_release_small(heap, step, sizeof *step);
    frame->count = count;
    frame->done = done;
    out->kind = STEP_POLL;
    out->result = budget_result_done(NULL);
    out->resume.fn = count_resume;
    out->resume.ctx = frame;
    return out;
  }
  }
  return NULL;
}

/* result => B.Finished{result} */
static Step *finished_step(Heap *heap, void *ctx, Result result) {
  (void)ctx;
  Step *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->kind = STEP_FINISHED;
  out->result = result;
  out->resume.fn = NULL;
  out->resume.ctx = NULL;
  return out;
}

/* count_polls (28-29): CompLater{done => count_step(start_comp(action, Finished), 0, done)} */
static Step *count_polls_later(Heap *heap, void *ctx, DoneK done) {
  Comp *action = ctx;
  if (action == NULL) return NULL;
  DoneK finish;
  finish.fn = finished_step;
  finish.ctx = NULL;
  return count_step(heap, budget_start_comp(heap, *action, finish), 0, done);
}

static Comp count_polls(Heap *heap, Comp action) {
  Comp *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed != NULL) *boxed = action;
  Comp out;
  out.kind = COMP_LATER;
  out.value = NULL;
  out.error = error_host_oom();
  out.action.fn = count_polls_later;
  out.action.ctx = boxed;
  return out;
}

/* The first `count` members, in order (List.reverse of the reversed prefix). */
static Selected first_members(Heap *heap, const Vec *members, size_t count) {
  Vec out = vec_new(heap->arena);
  bool ok = true;
  for (size_t i = 0; i < count && ok; i++) {
    VecGet got = vec_get(members, i);
    ok = got.status == VEC_GET_OK && vec_push(&out, got.value);
  }
  if (!ok) heap->oom = true;
  return selected_done(out);
}

static Selected prefix(Heap *heap, Str last, const Vec *members, size_t index);

/* prefix_next (37-40); `next` counts the members taken so far (the reversed list). */
static Selected prefix_next(Heap *heap, bool found, Str last, const Vec *members, size_t next) {
  if (found) return first_members(heap, members, next);
  return prefix(heap, last, members, next);
}

/* prefix (42-46) over members[index..]. */
static Selected prefix(Heap *heap, Str last, const Vec *members, size_t index) {
  VecGet got = vec_get(members, index);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return selected_fail(cat(heap, str_lit("unknown last member: "), last));
  case VEC_GET_OK: {
    const SRecDef *member = got.value;
    if (member == NULL) return selected_fail(cat(heap, str_lit("unknown last member: "), last));
    return prefix_next(heap, str_eq(member->name, last), last, members, index + 1);
  }
  }
  return selected_fail(cat(heap, str_lit("unknown last member: "), last));
}

/* selected (48-51); has_last = Some. */
static Selected selected(Heap *heap, bool has_last, Str last, const Vec *members) {
  if (has_last) return prefix(heap, last, members, 0);
  return selected_done(*members);
}

/* selection (53-59) */
static Selection selection(Heap *heap, const Vec *parsed, bool has_last, Str last) {
  VecGet got = vec_get(parsed, 0);
  const SDecl *group = got.status == VEC_GET_OK ? got.value : NULL;
  if (parsed->len != 1 || group == NULL || group->kind != SDECL_POLY_GROUP)
    return selection_fail(str_lit("the benchmark requires one family group"));
  Selected members = selected(heap, has_last, last, &group->helpers);
  switch (members.kind) {
  case OUTCOME_FAIL: return selection_fail(members.message);
  case OUTCOME_DONE: {
    SDecl *decl = heap_alloc(heap, sizeof *decl);
    Selection out;
    out.kind = OUTCOME_DONE;
    out.message = str_lit("");
    out.parsed = vec_new(heap->arena);
    out.members = members.members.len;
    if (decl == NULL) {
      heap->oom = true;
      return out;
    }
    *decl = *group;
    decl->helpers = members.members;
    if (!vec_push(&out.parsed, decl)) heap->oom = true;
    return out;
  }
  }
  return selection_fail(members.message);
}

static size_t family_rows_length(const FamilyRows *rows) {
  size_t n = 0;
  for (const FamilyRows *row = rows; row != NULL; row = row->rest) n++;
  return n;
}

/* report (61-64) */
static Str report(Heap *heap, const Counted *checked, size_t members, uint64_t elapsed) {
  char line[512];
  int n = snprintf(line, sizeof line,
                   "{\"checked\":true,\"members\":%zu,\"entries\":%zu,\"families\":%zu,\"polls\":%llu,"
                   "\"elapsed_milliseconds\":%llu,\"allocated_words\":null,\"cpu_seconds\":null,"
                   "\"unsupported_metrics\":[\"allocated_words\",\"cpu_seconds\"]}",
                   members, checked->checked->rows.len, family_rows_length(checked->checked->globals.families),
                   (unsigned long long)checked->polls, (unsigned long long)elapsed);
  if (n < 0 || (size_t)n >= sizeof line) {
    heap->oom = true;
    return str_lit("");
  }
  Str out = str_cat(heap->arena, str_lit(""), str_n(line, (size_t)n));
  if (out.len != (size_t)n) heap->oom = true;
  return out;
}

/* measured_result (66-69) */
static Outcome measured_result(Heap *heap, Result result, size_t members, uint64_t elapsed) {
  switch (result.kind) {
  case RESULT_FAIL: return outcome_fail(error_to_string(heap, result.error));
  case RESULT_DONE: {
    const Counted *checked = result.value;
    if (checked == NULL || checked->checked == NULL) {
      heap->oom = true;
      return outcome_fail(str_lit(""));
    }
    return outcome_done(report(heap, checked, members, elapsed));
  }
  }
  return outcome_fail(error_to_string(heap, result.error));
}

/* measured (71-77): IO.now around the polled elaboration; Nat.sub truncates at 0. */
static Outcome measured(Heap *heap, const Selection *chosen) {
  uint64_t start = driver_now_ms();
  Result checked = budget_run_poll(heap, count_polls(heap, elab_elab_program_comp(heap, global_empty(), &chosen->parsed)),
                                   unspent());
  uint64_t stop = driver_now_ms();
  return measured_result(heap, checked, chosen->members, stop >= start ? stop - start : 0);
}

/* suite (79-84): F.read_file, R.kernel(Parser.program), R.lift(selection), measured. */
static Outcome suite(Heap *heap, Str path, bool has_last, Str last) {
  IoResult source = io_read_file(heap, path);
  switch (source.status) {
  case IO_FAIL: return outcome_fail(source.value);
  case IO_DONE: break;
  }
  Result parsed = parser_program(heap, source.value);
  switch (parsed.kind) {
  case RESULT_FAIL: return outcome_fail(error_to_string(heap, parsed.error));
  case RESULT_DONE: break;
  }
  const Vec *decls = parsed.value;
  if (decls == NULL) {
    heap->oom = true;
    return outcome_fail(str_lit(""));
  }
  Selection chosen = selection(heap, decls, has_last, last);
  switch (chosen.kind) {
  case OUTCOME_FAIL: return outcome_fail(chosen.message);
  case OUTCOME_DONE: break;
  }
  if (heap->oom) return outcome_fail(str_lit(""));
  return measured(heap, &chosen);
}

/* finish (86-89); an out-of-memory heap is the host failure exit. */
static int finish(Heap *heap, Outcome result) {
  if (heap->oom) return driver_host_failure();
  switch (result.kind) {
  case OUTCOME_DONE: return driver_print(result.text);
  case OUTCOME_FAIL: {
    Str message = cat(heap, str_lit("TEMPLATE-COST-FAIL "), result.message);
    if (heap->oom) return driver_host_failure();
    return driver_die_text(1, message);
  }
  }
  return driver_host_failure();
}

/* execute (91-94) */
static int execute(Heap *heap, Str path, bool has_last, Str last) {
  return finish(heap, suite(heap, path, has_last, last));
}

/* arguments (96-100) over IO.args() = argv[1..]. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return execute(heap, str_lit(argv[1]), false, str_lit(""));
  if (argc == 4 && str_eq(str_lit(argv[2]), str_lit("--through")))
    return execute(heap, str_lit(argv[1]), true, str_lit(argv[3]));
  return driver_die(1, "usage: template_cost.exe SOURCE [--through MEMBER]");
}

static int entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  arena_free(&arena);
  return code;
}

/* main (102-105) */
int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
