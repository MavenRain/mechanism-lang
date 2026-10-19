/* Port of tests/surface_program.bend (oracle label unit-surface-program): whole programs through
 * Elab.check_text; declarations, templates, composition and reuse are accepted or rejected with the exact
 * text, and two programs sit exactly on their trusted-check poll boundary. */
#include "driver.h"
#include "surface.h"

/* box(): transcribed verbatim from the Bend driver (same escapes). */
static const char *BOX =
  "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A where def unbox : (0 A : Sort u) -> Box A -> A := fun (0 A : Sort u) (b : Box A) => match b as self in Box return A with | box (x : A) => x end\n";

typedef enum { CASE_ACCEPTS, CASE_REJECTS, CASE_REJECTS_WITH, CASE_BUDGET } CaseKind;

/* One suite() line: the helper called, whether box() is prepended, the source, the rejects_with message. */
typedef struct {
  CaseKind kind;
  bool boxed;
  const char *source;
  const char *message;
} Case;

static const Case CASES[] = {
  {CASE_ACCEPTS, false, "def x : Nat := 3 def y : Nat := natAdd x 4", ""},
  {CASE_ACCEPTS, false, "mu Flag : Type 0 with | off : Flag | on : Flag def flag : Flag := on def choose : Flag -> Nat := fun (x : Flag) => case x as self in Flag return Nat with | off => 0 | on => 1", ""},
  {CASE_ACCEPTS, false, "poly (u) def identity : (0 A : Sort u) -> A -> A := fun (0 A : Sort u) (x : A) => x specialize identity (1) as id def x : Nat := id Nat 9", ""},
  {CASE_ACCEPTS, true, "specialize Box (1) as Small export (unbox := get) def x : Nat := get Nat (box 37)", ""},
  {CASE_ACCEPTS, true, "poly (u) group Bundle where specialize Box (u) as Inner end specialize Bundle (1) as Run def x : Nat := Run_Inner_unbox Nat (box 37)", ""},
  {CASE_ACCEPTS, true, "specialize Box (1) as First specialize Box (1) as Second with (Box := First) def x : Nat := Second_unbox Nat (box 37)", ""},
  {CASE_REJECTS_WITH, false, "def x : Nat := 1 def x : Nat := 2", "the name x is already declared"},
  {CASE_ACCEPTS, false, "def g : Nat := 1 poly (u) mu Box : Sort (succ u) with | box : Box where def m : Nat := g end specialize Box (0) as B", ""},
  {CASE_REJECTS_WITH, false, "def g : Nat := 1 poly (u) mu Box : Sort (succ u) with | box : Box where def m : Nat := g end def g : Nat := 2 specialize Box (0) as B", "the name g is already declared"},
  {CASE_REJECTS_WITH, false, "mu N : Type 0 with | z : N | s : N -> N def g : N := z def rec g : N -> N := fun (n : N) => match n as self in N return N with | z => z | s (p : N) => s (g p)", "the name g is already declared"},
  {CASE_ACCEPTS, false, "mu N : Type 0 with | z : N | s : N -> N def rec id : N -> N := fun (n : N) => match n as self in N return N with | z => z | s (p : N) => s (id p) def result : N := id (s z)", ""},
  {CASE_REJECTS, false, "def rec loop : Nat -> Nat := fun (n : Nat) => loop n", ""},
  {CASE_REJECTS, false, "poly (u) def identity : (0 A : Sort u) -> A -> A := fun (0 A : Sort u) (x : A) => x specialize identity () as bad", ""},
  {CASE_REJECTS, true, "specialize Box (1) as Small export ()", ""},
  {CASE_REJECTS, true, "mu Wrong : Type 0 with | wrong : Wrong specialize Box (1) as Small with (Box := Wrong)", ""},
  {CASE_BUDGET, false, "def x : Nat := 3 def y : Nat := natAdd x 4", ""},
  {CASE_BUDGET, true, "specialize Box (1) as Small def x : Nat := Small_unbox Nat (box 37)", ""}
};

static Result done(void *value) { return (Result){RESULT_DONE, value, {0}}; }
static Result fail(Error error) { return (Result){RESULT_FAIL, NULL, error}; }
static Result mismatch(Str message) { return fail(error_make(ERROR_MISMATCH, message)); }
static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, const char *b, Str c) { return cat(heap, cat(heap, a, str_lit(b)), c); }

/* ensure(ok, message) */
static Result ensure(bool ok, Str message) {
  if (ok) return done(NULL);
  return mismatch(message);
}

/* Elab.check_text(Global.initial(), Budget.unlimited(), source) */
static Result check_text(Heap *heap, Str source) {
  return elab_check_text(heap, global_initial(heap), budget_unlimited(), source);
}

/* accepted(result) */
static Result accepted(Result result) {
  if (result.kind == RESULT_DONE) return done(NULL);
  return result;
}

/* rejected(result, source) */
static Result rejected(Heap *heap, Result result, Str source) {
  if (result.kind == RESULT_DONE) return mismatch(cat(heap, str_lit("unexpected program acceptance: "), source));
  return done(NULL);
}

/* rejected_with(result, expected) */
static Result rejected_with(Heap *heap, Result result, Str expected) {
  if (result.kind == RESULT_DONE) return mismatch(cat(heap, str_lit("expected rejection: "), expected));
  Str actual = error_to_string(heap, result.error);
  return ensure(str_eq(expected, actual), cat3(heap, cat(heap, str_lit("expected "), expected), "; actual ", actual));
}

/* rejects_with(source, message) */
static Result rejects_with(Heap *heap, Str source, Str message) {
  return rejected_with(heap, check_text(heap, source), error_to_string(heap, error_make(ERROR_MISMATCH, message)));
}

/* expect_budget(result, source, count) */
static Result expect_budget(Heap *heap, Result result, Str source, Nat count) {
  if (result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED) return done(NULL);
  Str head = cat(heap, str_lit("budget boundary "), heap_nat_show(heap, count));
  if (result.kind == RESULT_FAIL)
    return mismatch(cat3(heap, cat3(heap, head, " returned ", error_to_string(heap, result.error)), " for ", source));
  return mismatch(cat3(heap, head, " accepted ", source));
}

/* Nat.sub(a, b): truncated subtraction. */
static Nat nat_minus(Heap *heap, Nat a, Nat b) {
  NatSub sub = nat_sub(heap->arena, a, b);
  switch (sub.status) {
    case NAT_SUB_OK: return sub.value;
    case NAT_SUB_UNDERFLOW: return nat_zero();
    case NAT_SUB_OOM: heap->oom = true; return nat_zero();
  }
  return nat_zero();
}

/* measured(state): the polls used (10000 - remaining) through *out. */
static Result measured(Heap *heap, BudgetState state, Nat *out) {
  if (state.budget.kind == BUDGET_POLLS && state.result.kind == RESULT_DONE) {
    *out = nat_minus(heap, heap_nat_of_u64(heap, 10000), state.budget.remaining);
    return done(NULL);
  }
  if (state.result.kind == RESULT_FAIL) return state.result;
  return mismatch(str_lit("measurement lost budget state"));
}

/* budget_boundary(source) */
static Result budget_boundary(Heap *heap, Str source) {
  Result parsed = parser_program(heap, source);
  if (parsed.kind != RESULT_DONE) return parsed;
  const Vec *decls = (const Vec *)parsed.value;
  Comp action = elab_elab_program_comp(heap, global_initial(heap), decls);
  Nat used = nat_zero();
  Result m = measured(heap, budget_run_state(heap, action, budget_of_polls(heap_nat_of_u64(heap, 10000))), &used);
  if (m.kind != RESULT_DONE) return m;
  Result spent = ensure(!nat_is_zero(used), str_lit("program consumed no trusted-check polls"));
  if (spent.kind != RESULT_DONE) return spent;
  Result checked = elab_elab_program_in(heap, global_initial(heap), budget_of_polls(used), decls);
  if (checked.kind != RESULT_DONE) return checked;
  Nat fewer = nat_minus(heap, used, heap_nat_of_u64(heap, 1));
  return expect_budget(heap, elab_elab_program_in(heap, global_initial(heap), budget_of_polls(fewer), decls), source, used);
}

/* The source of one case: box() ++ source when boxed. */
static Str case_source(Heap *heap, Case c) {
  if (c.boxed) return cat(heap, str_lit(BOX), str_lit(c.source));
  return str_lit(c.source);
}

/* One suite() line. */
static Result run_case(Heap *heap, Case c) {
  Str source = case_source(heap, c);
  switch (c.kind) {
    case CASE_ACCEPTS: return accepted(check_text(heap, source));
    case CASE_REJECTS: return rejected(heap, check_text(heap, source), source);
    case CASE_REJECTS_WITH: return rejects_with(heap, source, str_lit(c.message));
    case CASE_BUDGET: return budget_boundary(heap, source);
  }
  return mismatch(str_lit("unknown case kind"));
}

/* suite(): the do block stops at the first Fail. */
static Result suite(Heap *heap) {
  size_t count = sizeof CASES / sizeof CASES[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++) r = run_case(heap, CASES[i]);
  return r;
}

/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE)
    return driver_print(str_lit("PASS surface declarations, templates, composition, reuse and shared budget"));
  return driver_die_text(1, error_to_string(heap, result.error));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = report(&heap, suite(&heap));
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
