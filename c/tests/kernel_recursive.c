/* Port of bend2/tests/kernel_recursive.bend (oracle label unit-kernel-recursive): recursive values compute,
 * guarded neutrals stay neutral, opacity is honoured, and two raw kernel negatives fail with the exact text. */
#include "driver.h"
#include "surface.h"

/* source(): transcribed verbatim from the Bend driver (same escapes). */
static const char *SOURCE =
  "\nmu N : Type 0 with | zero : N | succ : N -> N\nmu A : Type 0 with | leaf : A | node : B -> A\nand B : Type 0 with | cons : A -> B\nmu V : (0 i : N) -> Type 0 with\n| vz : V zero\n| vs : (0 i : N) -> V i -> V (succ i)\ndef rec length : (0 i : N) -> V i -> N := fun (0 i : N) (v : V i) => case v as x in V j return N with | vz => zero | vs 0 j w => succ (length j w)\ndef rec double : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ m => succ (succ (double m))\ndef rec sizeA : A -> Nat := fun (a : A) => case a as x in A return Nat with | leaf => 0 | node b => natAdd 1 (sizeB b)\nand sizeB : B -> Nat := fun (b : B) => case b as x in B return Nat with | cons a => natAdd 1 (sizeA a)\ndef rec f : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ m => g m\nand g : N -> N := fun (n : N) => zero\nand spare : N := zero\ndef rec keep : Nat -> N -> Nat := fun (a : Nat) => fun (n : N) => case n as x in N return Nat with | zero => a | succ m => keep a m\ndef rec choose : N -> Nat -> Nat := fun (n : N) => case n as x in N return (Nat -> Nat) with | zero => fun (a : Nat) => a | succ m => choose m\ndef doubled : N := double (succ (succ zero))\ndef four : N := succ (succ (succ (succ zero)))\ndef z : N := zero\ndef mutualValue : Nat := sizeA (node (cons leaf))\ndef two : Nat := 2\ndef helper : N := f (succ zero)\ndef helperDirect : N := g zero\ndef kept : Nat := keep 7 (succ (succ zero))\ndef seven : Nat := 7\ndef chosen : Nat := choose (succ (succ zero)) 7\ndef indexed : N := length (succ zero) (vs zero vz)\ndef one : N := succ zero\ndef partial : N -> Nat := keep 7\ndef neutral : N -> Nat := fun (n : N) => keep 7 n\n";

static Result done(void *value) { return (Result){RESULT_DONE, value, {0}}; }
static Result fail(Error error) { return (Result){RESULT_FAIL, NULL, error}; }
static Result mismatch(Str message) { return fail(error_make(ERROR_MISMATCH, message)); }

/* require(ok, message) */
static Result require(Heap *heap, bool ok, Str message) {
  (void)heap;
  if (ok) return done(NULL);
  return mismatch(message);
}

static Str cat(Heap *heap, Str a, const char *b) { return str_cat(heap->arena, a, str_lit(b)); }

/* singleton(item) */
static Vec singleton(Heap *heap, void *item) {
  Vec v = vec_new(heap->arena);
  if (!vec_push(&v, item)) heap->oom = true;
  return v;
}

/* normal(globals, name) -> Pp.term([], normalize(eval(Global{name}))) through *out. */
static Result normal(Heap *heap, Globals globals, Str name, Str *out) {
  Result value = eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, name));
  if (value.kind != RESULT_DONE) return value;
  Result term = eval_normalize(heap, globals, nat_zero(), (const Value *)value.value);
  if (term.kind != RESULT_DONE) return term;
  Vec names = vec_new(heap->arena);
  *out = pp_term(heap, &names, (const Term *)term.value);
  return done(NULL);
}

typedef struct {
  const char *actual;
  const char *expected;
} NamePair;

static const NamePair PAIRS[] = {
  {"doubled", "four"}, {"mutualValue", "two"}, {"helper", "z"}, {"helperDirect", "z"},
  {"spare", "z"}, {"kept", "seven"}, {"chosen", "seven"}, {"indexed", "one"},
};

/* compare_pairs(pairs, globals): one Con step. */
static Result compare_pair(Heap *heap, Globals globals, NamePair pair) {
  Str got = str_lit("");
  Str want = str_lit("");
  Result a = normal(heap, globals, str_lit(pair.actual), &got);
  if (a.kind != RESULT_DONE) return a;
  Result b = normal(heap, globals, str_lit(pair.expected), &want);
  if (b.kind != RESULT_DONE) return b;
  return require(heap, str_eq(got, want), cat(heap, str_lit(pair.actual), " did not compute"));
}

static Result compare_pairs(Heap *heap, Globals globals) {
  size_t count = sizeof PAIRS / sizeof PAIRS[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++) r = compare_pair(heap, globals, PAIRS[i]);
  return r;
}

/* unfolded(globals, name) -> whnf(eval(Global{name})) through *out. */
static Result unfolded(Heap *heap, Globals globals, Str name, const Value **out) {
  Result value = eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, name));
  if (value.kind != RESULT_DONE) return value;
  Result head = eval_whnf(heap, globals, (const Value *)value.value);
  if (head.kind != RESULT_DONE) return head;
  *out = (const Value *)head.value;
  return done(NULL);
}

/* is_neutral(value) */
static bool is_neutral(const Value *value) { return value->kind == VALUE_NEUTRAL; }

static const char *const NEUTRALS[] = {"double", "keep", "partial"};

/* neutrals(names, globals): one Con step. */
static Result neutral_name(Heap *heap, Globals globals, const char *name) {
  const Value *value = NULL;
  Result r = unfolded(heap, globals, str_lit(name), &value);
  if (r.kind != RESULT_DONE) return r;
  return require(heap, is_neutral(value), cat(heap, str_lit(name), " unfolded before its guarded argument"));
}

static Result neutrals(Heap *heap, Globals globals) {
  size_t count = sizeof NEUTRALS / sizeof NEUTRALS[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++) r = neutral_name(heap, globals, NEUTRALS[i]);
  return r;
}

/* neutral_lambda(value, globals): VSec{s, [VLeg{binders, clo}]} opened on var 0 must stay neutral. */
static Result neutral_lambda(Heap *heap, const Value *value, Globals globals) {
  VecGet leg = vec_get(&value->legs, 0);
  if (value->kind != VALUE_SEC || value->legs.len != 1 || leg.value == NULL) return mismatch(str_lit("neutral lambda"));
  Closure clo = ((const VLeg *)leg.value)->clo;
  Result body = eval_open_closure(heap, globals, clo, singleton(heap, (void *)value_var(heap, nat_zero())));
  if (body.kind != RESULT_DONE) return body;
  return require(heap, is_neutral((const Value *)body.value), str_lit("recursive call unfolded on a neutral argument"));
}

/* opaque_def(entry): the same DefEntry with reducible = False. */
static Entry opaque_def(DefEntry entry) {
  Entry e = {0};
  e.kind = ENTRY_DEF;
  e.def = entry;
  e.def.reducible = false;
  return e;
}

/* validate(checked) */
static Result validate(Heap *heap, const ElabChecked *checked) {
  Globals globals = checked->globals;
  Result r = compare_pairs(heap, globals);
  if (r.kind != RESULT_DONE) return r;
  r = neutrals(heap, globals);
  if (r.kind != RESULT_DONE) return r;
  const Value *value = NULL;
  r = unfolded(heap, globals, str_lit("neutral"), &value);
  if (r.kind != RESULT_DONE) return r;
  r = neutral_lambda(heap, value, globals);
  if (r.kind != RESULT_DONE) return r;
  MaybeDefEntry entry = global_find_def(str_lit("double"), globals);
  if (!entry.some) return mismatch(str_lit("double definition"));
  Globals opaque = global_add(heap, str_lit("double"), opaque_def(entry.value), globals);
  r = unfolded(heap, opaque, str_lit("doubled"), &value);
  if (r.kind != RESULT_DONE) return r;
  return require(heap, is_neutral(value), str_lit("an explicitly opaque recursive definition unfolded"));
}

/* mu_negative(result): Fail{Not_yet{"a right former at a mu shape arrives at M2"}} */
static bool mu_negative(Result result) {
  if (result.kind != RESULT_FAIL || result.error.kind != ERROR_NOT_YET) return false;
  return str_eq(result.error.message, str_lit("a right former at a mu shape arrives at M2"));
}

/* self_negative(result): any Fail whose message is the termination guard text. */
static bool self_negative(Heap *heap, MaybeNatResult result) {
  if (result.kind != RESULT_FAIL) return false;
  return str_eq(error_message(heap, result.error), str_lit("recursive definition self failed the structural termination guard"));
}

/* T.Ran{S.SMu{"F", [T.Univ{L.zero()}]}, T.Sec{S.SColl{Width.of_nat(0n)}, []}} */
static const Term *right_mu_former(Heap *heap) {
  Shape mu = shape_smu(str_lit("F"), singleton(heap, (void *)term_univ(heap, level_zero())));
  const Term *body = term_sec(heap, shape_scoll(big_of_nat(heap, nat_zero())), vec_new(heap->arena));
  return term_ran(heap, mu, body);
}

/* suite() */
static Result suite(Heap *heap) {
  Result mu = check_infer_term(heap, global_initial(heap), budget_unlimited(), right_mu_former(heap));
  Result r = require(heap, mu_negative(mu), str_lit("right mu former was admitted"));
  if (r.kind != RESULT_DONE) return r;
  MaybeNatResult self = totality_guard(heap, budget_unlimited(), global_initial(heap), str_lit("self"),
                                       term_global(heap, str_lit("Nat")), term_global(heap, str_lit("self")));
  r = require(heap, self_negative(heap, self), str_lit("unguarded recursion was admitted"));
  if (r.kind != RESULT_DONE) return r;
  Result checked = elab_check_in(heap, global_initial(heap), budget_unlimited(), str_lit(SOURCE));
  if (checked.kind != RESULT_DONE) return checked;
  return validate(heap, (const ElabChecked *)checked.value);
}

/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE)
    return driver_print(str_lit("PASS original recursive values, guarded neutrals, opacity and raw kernel negatives"));
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
