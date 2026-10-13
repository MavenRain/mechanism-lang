/* Port of bend2/tests/surface_prelude.bend (oracle label unit-surface-prelude): the four trusted
 * prelude catalogs (dependent, equality, congruence, families) instantiate through Poly and
 * Family, their specializations check and compute, and every catalog honours a zero budget. */
#include "driver.h"
#include "prelude.h"
#include "surface.h"

static Result done_value(void *value) {
  Result r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = error_make(ERROR_MISMATCH, str_lit(""));
  return r;
}

static Result fail_with(Error error) {
  Result r;
  r.kind = RESULT_FAIL;
  r.value = NULL;
  r.error = error;
  return r;
}

static Result ensure(bool ok, const char *message) {
  return ok ? done_value(NULL) : fail_with(error_make(ERROR_MISMATCH, str_lit(message)));
}

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL) heap->oom = true;
  else if (!vec_push(items, (void *)item)) heap->oom = true;
}

static Vec levels2(Heap *heap, const LevelVar *a, const LevelVar *b) {
  Vec levels = vec_new(heap->arena);
  push(heap, &levels, a);
  push(heap, &levels, b);
  return levels;
}

static Vec levels3(Heap *heap, const LevelVar *a, const LevelVar *b, const LevelVar *c) {
  Vec levels = levels2(heap, a, b);
  push(heap, &levels, c);
  return levels;
}

static Vec levels1(Heap *heap, const LevelVar *a) {
  Vec levels = vec_new(heap->arena);
  push(heap, &levels, a);
  return levels;
}

/* Result<Error.T, Global.T>: the Done value is a boxed Globals *. */
static Result done_globals(Heap *heap, Globals globals) {
  Globals *box = heap_alloc(heap, sizeof(Globals));
  if (box == NULL) return fail_with(error_host_oom());
  *box = globals;
  return done_value(box);
}

static Globals globals_of(Result r) { return *(const Globals *)r.value; }

/* installed(pair) = pair.globals, inlined: Poly.instantiate's Done value is a PolyInstance *. */
static Result install(Heap *heap, Globals globals, PolyCatalog catalog, const char *name, Vec levels, const char *alias) {
  Result r = poly_instantiate(heap, globals, catalog, budget_unlimited(), str_lit(name), levels, str_lit(alias));
  if (r.kind == RESULT_FAIL) return r;
  return done_globals(heap, ((const PolyInstance *)r.value)->globals);
}

/* Family.instantiate(globals, catalog, Budget.unlimited(), name, levels, alias, [], None{}). */
static Result instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, Vec levels, const char *alias) {
  Vec reuse = vec_new(heap->arena);
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return family_instantiate(heap, globals, catalog, budget_unlimited(), str_lit(name), &levels, str_lit(alias), &reuse, exports);
}

/* checked_globals(result) = result.globals, inlined: Elab.check_in's Done value is an ElabChecked *. */
static Result check_source(Heap *heap, Globals globals, const char *source) {
  Result r = elab_check_in(heap, globals, budget_unlimited(), str_lit(source));
  if (r.kind == RESULT_FAIL) return r;
  return done_globals(heap, ((const ElabChecked *)r.value)->globals);
}

static Result literal_nat(Heap *heap, const Term *term, const char *expected) {
  if (term->kind == TERM_LIT && term->lit.kind == LITERAL_INT) {
    return ensure(str_eq(big_to_string(heap, term->lit.integer), str_lit(expected)), "wrong computed prelude result");
  }
  return fail_with(error_make(ERROR_MISMATCH, str_lit("prelude result did not compute to a natural")));
}

static Result evaluates(Heap *heap, Globals globals, const char *name, const char *expected) {
  Result value = eval_eval(heap, globals, vec_new(heap->arena), term_global(heap, str_lit(name)));
  if (value.kind == RESULT_FAIL) return value;
  Result term = eval_normalize(heap, globals, nat_zero(), (const Value *)value.value);
  if (term.kind == RESULT_FAIL) return term;
  return literal_nat(heap, (const Term *)term.value, expected);
}

static Result dependent(Heap *heap) {
  Result step = dependent_catalog(heap, global_initial(heap), budget_unlimited());
  if (step.kind == RESULT_FAIL) return step;
  PolyCatalog catalog = (PolyCatalog)step.value;
  step = install(heap, global_initial(heap), catalog, "MechPi", levels2(heap, level_one(heap), level_one(heap)), "Pi");
  if (step.kind == RESULT_FAIL) return step;
  step = install(heap, globals_of(step), catalog, "MechSigma", levels2(heap, level_zero(), level_zero()), "Sigma");
  if (step.kind == RESULT_FAIL) return step;
  step = install(heap, globals_of(step), catalog, "mechSigmaMk", levels2(heap, level_zero(), level_zero()), "make");
  if (step.kind == RESULT_FAIL) return step;
  step = install(heap, globals_of(step), catalog, "mechSigmaFst", levels2(heap, level_zero(), level_zero()), "first");
  if (step.kind == RESULT_FAIL) return step;
  step = install(heap, globals_of(step), catalog, "mechSigmaSnd", levels2(heap, level_zero(), level_zero()), "second");
  if (step.kind == RESULT_FAIL) return step;
  step = install(heap, globals_of(step), catalog, "mechSigmaRec", levels3(heap, level_zero(), level_zero(), level_one(heap)), "fold");
  if (step.kind == RESULT_FAIL) return step;
  step = check_source(heap, globals_of(step),
                      "def B : Nat -> Type 0 := fun (x : Nat) => Nat "
                      "def p : Sigma Nat B := make Nat B 3 7 "
                      "def x : Nat := first Nat B p def y : Nat := second Nat B p "
                      "def combined : Nat := fold Nat B (fun (p : Sigma Nat B) => Nat) (fun (x : Nat) (y : Nat) => natAdd x y) p "
                      "def id : Pi Nat B := fun (x : Nat) => x def piValue : Nat := id 13");
  if (step.kind == RESULT_FAIL) return step;
  Globals globals = globals_of(step);
  step = evaluates(heap, globals, "x", "3");
  if (step.kind == RESULT_FAIL) return step;
  step = evaluates(heap, globals, "y", "7");
  if (step.kind == RESULT_FAIL) return step;
  step = evaluates(heap, globals, "combined", "10");
  if (step.kind == RESULT_FAIL) return step;
  return evaluates(heap, globals, "piValue", "13");
}

static Result equality(Heap *heap) {
  Result step = equality_catalog(heap, global_initial(heap), budget_unlimited());
  if (step.kind == RESULT_FAIL) return step;
  FamilyCatalog catalog = (FamilyCatalog)step.value;
  step = instantiate(heap, global_initial(heap), catalog, "MechEq", levels2(heap, level_one(heap), level_one(heap)), "Eq");
  if (step.kind == RESULT_FAIL) return step;
  step = instantiate(heap, globals_of(step), catalog, "MechTypeEq", levels1(heap, level_zero()), "TEq");
  if (step.kind == RESULT_FAIL) return step;
  step = check_source(heap, globals_of(step),
                      "def proof : Eq Nat 3 3 := Eq_refl Nat 3 "
                      "def symmetric : Eq Nat 3 3 := Eq_symm Nat 3 3 proof "
                      "def transitive : Eq Nat 3 3 := Eq_trans Nat 3 3 3 proof symmetric "
                      "def transported : Nat := Eq_transport Nat (fun (x : Nat) => Nat) 3 3 proof 11 "
                      "def casted : Nat := TEq_cast Nat Nat (TEq_refl Nat) 7");
  if (step.kind == RESULT_FAIL) return step;
  Globals globals = globals_of(step);
  step = evaluates(heap, globals, "transported", "11");
  if (step.kind == RESULT_FAIL) return step;
  return evaluates(heap, globals, "casted", "7");
}

static Result congruence(Heap *heap) {
  Result step = congruence_catalog(heap, global_initial(heap), budget_unlimited());
  if (step.kind == RESULT_FAIL) return step;
  FamilyCatalog catalog = (FamilyCatalog)step.value;
  step = instantiate(heap, global_initial(heap), catalog, "MechCongr", levels2(heap, level_one(heap), level_one(heap)), "Cong");
  if (step.kind == RESULT_FAIL) return step;
  step = check_source(heap, globals_of(step),
                      "def proof : Cong_Result Nat (natAdd 3 1) (natAdd 3 1) := Cong_congr Nat Nat (fun (x : Nat) => natAdd x 1) 3 3 mechReflCtor");
  if (step.kind == RESULT_FAIL) return step;
  return done_value(NULL);
}

static Result families(Heap *heap) {
  Result step = families_catalog(heap, global_initial(heap), budget_unlimited());
  if (step.kind == RESULT_FAIL) return step;
  FamilyCatalog catalog = (FamilyCatalog)step.value;
  step = instantiate(heap, global_initial(heap), catalog, "MechEq", levels1(heap, level_one(heap)), "BaseEq");
  if (step.kind == RESULT_FAIL) return step;
  step = instantiate(heap, globals_of(step), catalog, "MechSum", levels2(heap, level_zero(), level_zero()), "Sum");
  if (step.kind == RESULT_FAIL) return step;
  step = check_source(heap, globals_of(step),
                      "def proof : BaseEq Nat 3 3 := mechReflCtor "
                      "def right : Sum Nat Nat := mechInr 9 "
                      "def result : Nat := case right as self in Sum return Nat with | mechInl (x : Nat) => x | mechInr (y : Nat) => y");
  if (step.kind == RESULT_FAIL) return step;
  return evaluates(heap, globals_of(step), "result", "9");
}

static bool exhausted(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED;
}

static Result suite(Heap *heap) {
  Result step = dependent(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = equality(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = congruence(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = families(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = ensure(exhausted(dependent_catalog(heap, global_initial(heap), budget_of_polls(nat_zero()))), "dependent catalog ignored budget");
  if (step.kind == RESULT_FAIL) return step;
  step = ensure(exhausted(equality_catalog(heap, global_initial(heap), budget_of_polls(nat_zero()))), "equality catalog ignored budget");
  if (step.kind == RESULT_FAIL) return step;
  step = ensure(exhausted(congruence_catalog(heap, global_initial(heap), budget_of_polls(nat_zero()))), "congruence catalog ignored budget");
  if (step.kind == RESULT_FAIL) return step;
  return ensure(exhausted(families_catalog(heap, global_initial(heap), budget_of_polls(nat_zero()))), "family catalog ignored budget");
}

static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return driver_print(str_lit("PASS four trusted prelude catalogs and computed specializations"));
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
