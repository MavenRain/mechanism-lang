/* Port of bend2/tests/surface_universes.bend (oracle label unit-surface-universes): MechEq
 * specializes at Prop, data and a high universe literal and keeps singleton elimination, and
 * MechSum takes the maximum of two unequal universes in both directions. Imports surface_catalog
 * as Test (c/tests/surface_catalog.h). */
#include "driver.h"
#include "prelude.h"
#include "surface.h"
#include "surface_catalog.h"

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

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL) heap->oom = true;
  else if (!vec_push(items, (void *)item)) heap->oom = true;
}

static Vec levels1(Heap *heap, const LevelVar *a) {
  Vec levels = vec_new(heap->arena);
  push(heap, &levels, a);
  return levels;
}

static Vec levels2(Heap *heap, const LevelVar *a, const LevelVar *b) {
  Vec levels = levels1(heap, a);
  push(heap, &levels, b);
  return levels;
}

static Vec terms0(Heap *heap) { return vec_new(heap->arena); }

static Vec terms1(Heap *heap, const Term *a) {
  Vec terms = vec_new(heap->arena);
  push(heap, &terms, a);
  return terms;
}

static Vec terms2(Heap *heap, const Term *a, const Term *b) {
  Vec terms = terms1(heap, a);
  push(heap, &terms, b);
  return terms;
}

static const TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  TeleBinder *b = heap_alloc(heap, sizeof(TeleBinder));
  if (b == NULL) return NULL;
  b->quantity = quantity;
  b->name = str_lit(name);
  b->ty = ty;
  return b;
}

static Addr ctor_addr(const char *name) {
  Addr addr;
  addr.kind = ADDR_CTOR;
  addr.quantity = QUANTITY_ZERO;
  addr.arg = NULL;
  addr.index = big_of_nat(NULL, nat_zero());
  addr.name = str_lit(name);
  return addr;
}

static Globals globals_of(Result r) { return *(const Globals *)r.value; }

/* Family.instantiate(globals, catalog, Budget.unlimited(), name, levels, alias, [], None{}): Done = Globals *. */
static Result instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, Vec levels, const char *alias) {
  Vec reuse = vec_new(heap->arena);
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return family_instantiate(heap, globals, catalog, budget_unlimited(), str_lit(name), &levels, str_lit(alias), &reuse, exports);
}

static Result high_value(Heap *heap, MaybeBig value) {
  if (value.some) return done_value((void *)level_var_lsucc(heap, value.value, level_zero()));
  return fail_with(error_make(ERROR_MISMATCH, str_lit("high universe literal did not parse")));
}

static const Term *member(Heap *heap, const char *name, Vec params, Vec indices) {
  return term_lan(heap, shape_smu(str_lit(name), indices), rules_diagram_of(heap, &params));
}

static Decl reflexive(Heap *heap, const char *name, const LevelVar *carrier) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, carrier)));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "x", term_var(heap, nat_zero())));
  const Term *result = member(heap, name, terms2(heap, term_var(heap, heap_nat_of_u64(heap, 1)), term_var(heap, nat_zero())),
                              terms1(heap, term_var(heap, nat_zero())));
  const Term *body = term_in(heap, shape_smu(str_lit(name), terms1(heap, term_var(heap, nat_zero()))), ctor_addr("mechReflCtor"),
                             terms0(heap));
  return dependent_definition(heap, str_lit("reflClient"), &binders, result, body);
}

static bool eq_metadata(Heap *heap, MaybeFamily family) {
  if (family.some) {
    return level_equal(heap, family_kernel_level(family.value), level_zero()) && check_engine_mu_zero_eliminable(family.value);
  }
  return false;
}

static Result equalities(Heap *heap, FamilyCatalog catalog) {
  Result step = high_value(heap, big_of_decimal(heap, str_lit("4611686018427387903")));
  if (step.kind == RESULT_FAIL) return step;
  const LevelVar *high = (const LevelVar *)step.value;
  step = instantiate(heap, global_empty(), catalog, "MechEq", levels1(heap, level_zero()), "EqProp");
  if (step.kind == RESULT_FAIL) return step;
  step = instantiate(heap, globals_of(step), catalog, "MechEq", levels1(heap, level_one(heap)), "EqData");
  if (step.kind == RESULT_FAIL) return step;
  step = instantiate(heap, globals_of(step), catalog, "MechEq", levels1(heap, high), "EqHigh");
  if (step.kind == RESULT_FAIL) return step;
  Globals globals = globals_of(step);
  step = check_check_decl(heap, globals, budget_unlimited(), reflexive(heap, "EqProp", level_zero()));
  if (step.kind == RESULT_FAIL) return step;
  step = check_check_decl(heap, globals, budget_unlimited(), reflexive(heap, "EqData", level_one(heap)));
  if (step.kind == RESULT_FAIL) return step;
  step = check_check_decl(heap, globals, budget_unlimited(), reflexive(heap, "EqHigh", high));
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(eq_metadata(heap, global_find_family(str_lit("EqHigh"), globals)), "equality lost Prop or singleton elimination");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(!global_find_family(str_lit("MechEq"), globals).some, "equality specialization installed the symbolic family");
}

static Decl sum_client(Heap *heap, const char *name, const LevelVar *left, const LevelVar *right, const char *ctor,
                       const Term *field) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, left))));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, level_succ(heap, right))));
  push(heap, &binders, binder(heap, QUANTITY_MANY, "value", field));
  const Term *result = member(heap, name, terms2(heap, term_var(heap, heap_nat_of_u64(heap, 2)), term_var(heap, heap_nat_of_u64(heap, 1))), terms0(heap));
  const Term *body = term_in(heap, shape_smu(str_lit(name), terms0(heap)), ctor_addr(ctor), terms1(heap, term_var(heap, nat_zero())));
  return dependent_definition(heap, str_lit("sumClient"), &binders, result, body);
}

static bool sum_level(Heap *heap, MaybeFamily family) {
  if (family.some) return level_equal(heap, family_kernel_level(family.value), level_of_u32(heap, 3));
  return false;
}

static Result sums(Heap *heap, FamilyCatalog catalog) {
  Result step = instantiate(heap, global_empty(), catalog, "MechSum", levels2(heap, level_zero(), level_of_u32(heap, 2)), "SumLowHigh");
  if (step.kind == RESULT_FAIL) return step;
  step = instantiate(heap, globals_of(step), catalog, "MechSum", levels2(heap, level_of_u32(heap, 2), level_zero()), "SumHighLow");
  if (step.kind == RESULT_FAIL) return step;
  Globals globals = globals_of(step);
  step = check_check_decl(heap, globals, budget_unlimited(),
                          sum_client(heap, "SumLowHigh", level_zero(), level_of_u32(heap, 2), "mechInl", term_var(heap, heap_nat_of_u64(heap, 1))));
  if (step.kind == RESULT_FAIL) return step;
  step = check_check_decl(heap, globals, budget_unlimited(),
                          sum_client(heap, "SumLowHigh", level_zero(), level_of_u32(heap, 2), "mechInr", term_var(heap, nat_zero())));
  if (step.kind == RESULT_FAIL) return step;
  step = check_check_decl(heap, globals, budget_unlimited(),
                          sum_client(heap, "SumHighLow", level_of_u32(heap, 2), level_zero(), "mechInl", term_var(heap, heap_nat_of_u64(heap, 1))));
  if (step.kind == RESULT_FAIL) return step;
  step = check_check_decl(heap, globals, budget_unlimited(),
                          sum_client(heap, "SumHighLow", level_of_u32(heap, 2), level_zero(), "mechInr", term_var(heap, nat_zero())));
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(sum_level(heap, global_find_family(str_lit("SumLowHigh"), globals)) &&
                            sum_level(heap, global_find_family(str_lit("SumHighLow"), globals)),
                        "sum maximum universe did not specialize in both directions");
}

static Result suite(Heap *heap) {
  Result step = families_catalog(heap, global_empty(), budget_unlimited());
  if (step.kind == RESULT_FAIL) return step;
  FamilyCatalog catalog = (FamilyCatalog)step.value;
  step = equalities(heap, catalog);
  if (step.kind == RESULT_FAIL) return step;
  return sums(heap, catalog);
}

static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) {
    return driver_print(str_lit("PASS Eq Prop/data/high universes and Sum unequal universes in both directions"));
  }
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
