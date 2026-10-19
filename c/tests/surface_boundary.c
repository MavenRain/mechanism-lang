/* Port of tests/surface_boundary.bend (oracle label unit-surface-boundary): raw family scope,
 * universality, callback cancellation, nested budgets and inventories. Imports surface_catalog.bend as
 * Test (c/tests/surface_catalog.h). */
#include "surface_catalog.h"
#include "driver.h"

static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (item == NULL) heap->oom = true;
  else if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

static Vec two_items(Heap *heap, void *first, void *second) {
  Vec items = one_item(heap, first);
  if (second == NULL) heap->oom = true;
  else if (!vec_push(&items, second)) heap->oom = true;
  return items;
}

static Error host_oom_error(void) { return error_make(ERROR_MISMATCH, str_lit("host out of memory")); }

static Nat nat_u64(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

/* Nat.sub(a, b): saturates at zero like the Bend primitive. */
static Nat nat_minus(Heap *heap, Nat a, Nat b) {
  NatSub sub = nat_sub(heap->arena, a, b);
  switch (sub.status) {
    case NAT_SUB_OK:
      return sub.value;
    case NAT_SUB_UNDERFLOW:
      return nat_zero();
    case NAT_SUB_OOM:
      heap->oom = true;
      return nat_zero();
  }
  return nat_zero();
}

static Nat *nat_box(Heap *heap, Nat value) {
  Nat *box = heap_alloc(heap, sizeof(Nat));
  if (box == NULL) heap->oom = true;
  else *box = value;
  return box;
}

static Str *str_box(Heap *heap, const char *text) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) heap->oom = true;
  else *box = str_lit(text);
  return box;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *box = heap_alloc(heap, sizeof(Decl));
  if (box == NULL) heap->oom = true;
  else *box = decl;
  return box;
}

static Vec *vec_box(Heap *heap, Vec items) {
  Vec *box = heap_alloc(heap, sizeof(Vec));
  if (box == NULL) heap->oom = true;
  else *box = items;
  return box;
}

static FamilyMemberElaborator *elaborator_box(Heap *heap, Comp (*fn)(Heap *, void *, Globals)) {
  FamilyMemberElaborator *box = heap_alloc(heap, sizeof(FamilyMemberElaborator));
  if (box == NULL) {
    heap->oom = true;
    return NULL;
  }
  box->fn = fn;
  box->ctx = NULL;
  return box;
}

static FamilyRaw *raw_box(Heap *heap, FamilyDecl family, Vec ctors) {
  FamilyRaw *raw = heap_alloc(heap, sizeof(FamilyRaw));
  if (raw == NULL) {
    heap->oom = true;
    return NULL;
  }
  raw->family = family;
  raw->ctors = ctors;
  return raw;
}

static CtorDecl *ctor_box(Heap *heap, const char *name, Vec args, Vec res_params, Vec res_idx) {
  CtorDecl *ctor = heap_alloc(heap, sizeof(CtorDecl));
  if (ctor == NULL) {
    heap->oom = true;
    return NULL;
  }
  ctor->name = str_lit(name);
  ctor->args = args;
  ctor->res_params = res_params;
  ctor->res_idx = res_idx;
  return ctor;
}

static FamilyDecl family_decl(Str name, Vec params, Vec indices, const LevelVar *level) {
  FamilyDecl decl;
  decl.name = name;
  decl.params = params;
  decl.indices = indices;
  decl.level = level;
  return decl;
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
}

static Globals globals_of(Result step) { return *(const Globals *)step.value; }

static Nat nat_of(Result step) { return *(const Nat *)step.value; }

/* exact(expected, result) */
static bool exact(Heap *heap, Error expected, Result result) {
  if (result.kind != RESULT_FAIL) return false;
  return str_eq(error_to_string(heap, expected), error_to_string(heap, result.error));
}

/* eq_family(carrier) */
static FamilyDecl eq_family(Heap *heap, const LevelVar *carrier) {
  Vec params = two_items(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, carrier)),
                         positivity_binder_new(heap, QUANTITY_ZERO, str_lit("x"), term_var(heap, nat_zero())));
  Vec indices = one_item(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("y"), term_var(heap, nat_u64(heap, 1))));
  return family_decl(str_lit("Eq"), params, indices, level_zero());
}

/* refl_ctor() */
static CtorDecl *refl_ctor(Heap *heap) {
  Vec res_params = two_items(heap, (void *)term_var(heap, nat_u64(heap, 1)), (void *)term_var(heap, nat_zero()));
  Vec res_idx = one_item(heap, (void *)term_var(heap, nat_zero()));
  return ctor_box(heap, "refl", vec_new(heap->arena), res_params, res_idx);
}

/* empty(name, level) */
static FamilyDecl empty(Heap *heap, const char *name, const LevelVar *level) {
  return family_decl(str_lit(name), vec_new(heap->arena), vec_new(heap->arena), level);
}

/* scoped(family, ctors) */
static bool scoped(Heap *heap, FamilyDecl family, Vec ctors) {
  Vec none = vec_new(heap->arena);
  return catalog_scope_refused(
      family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_u64(heap, 1), family, ctors, &none));
}

/* ignored_metadata() */
static const Term *ignored_metadata(Heap *heap) {
  Shape pi = shape_spi(QUANTITY_MANY, str_lit("A"), (void *)term_univ(heap, level_one(heap)));
  Vec binders = one_item(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("A")));
  Leg *leg = term_leg_new(heap, binders, term_var(heap, nat_zero()));
  const Term *identity;
  if (leg == NULL) {
    heap->oom = true;
    return term_auto();
  }
  identity = term_ann(heap, term_sec(heap, pi, one_item(heap, leg)), term_ran(heap, pi, term_univ(heap, level_one(heap))));
  return term_out(heap, shape_spi(QUANTITY_MANY, str_lit("ignored"), (void *)term_univ(heap, level_var(heap, nat_u64(heap, 1)))),
                  term_addr_pt(QUANTITY_MANY, term_univ(heap, level_zero())), identity);
}

/* scope_fields() */
static Result scope_fields(Heap *heap) {
  Vec none = vec_new(heap->arena);
  Result step = family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_u64(heap, 1),
                               eq_family(heap, level_var(heap, nat_zero())), one_item(heap, refl_ctor(heap)), &none);
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(scoped(heap, empty(heap, "Header", level_succ(heap, level_var(heap, nat_u64(heap, 1)))), vec_new(heap->arena)),
                        "free family header level accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(scoped(heap, eq_family(heap, level_var(heap, nat_u64(heap, 1))), one_item(heap, refl_ctor(heap))),
                        "free family parameter level accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      scoped(heap,
             family_decl(str_lit("Index"), vec_new(heap->arena),
                         one_item(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("P"),
                                                              term_univ(heap, level_var(heap, nat_u64(heap, 1))))),
                         level_zero()),
             vec_new(heap->arena)),
      "free family index level accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      scoped(heap, empty(heap, "Field", level_succ(heap, level_one(heap))),
             one_item(heap, ctor_box(heap, "field",
                                     one_item(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("P"),
                                                                          term_univ(heap, level_var(heap, nat_u64(heap, 1))))),
                                     vec_new(heap->arena), vec_new(heap->arena)))),
      "free constructor field level accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      scoped(heap, eq_family(heap, level_var(heap, nat_zero())),
             one_item(heap, ctor_box(heap, "refl", vec_new(heap->arena),
                                     two_items(heap,
                                               (void *)term_ann(heap, term_var(heap, nat_u64(heap, 1)),
                                                                term_univ(heap, level_var(heap, nat_u64(heap, 1)))),
                                               (void *)term_var(heap, nat_zero())),
                                     one_item(heap, (void *)term_var(heap, nat_zero()))))),
      "free constructor result parameter annotation accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      scoped(heap, eq_family(heap, level_var(heap, nat_zero())),
             one_item(heap, ctor_box(heap, "refl", vec_new(heap->arena),
                                     two_items(heap, (void *)term_var(heap, nat_u64(heap, 1)), (void *)term_var(heap, nat_zero())),
                                     one_item(heap, (void *)term_ann(heap, term_var(heap, nat_zero()),
                                                                     term_univ(heap, level_var(heap, nat_u64(heap, 1)))))))),
      "free constructor result index annotation accepted");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(
      scoped(heap, empty(heap, "Hidden", level_succ(heap, level_one(heap))),
             one_item(heap, ctor_box(heap, "hidden",
                                     one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("field"), ignored_metadata(heap))),
                                     vec_new(heap->arena), vec_new(heap->arena)))),
      "ignored shape metadata bypassed scope traversal");
}

/* counterfeit(carrier) */
static FamilyDecl counterfeit(Heap *heap, const LevelVar *carrier) {
  Vec params = one_item(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, level_succ(heap, carrier))));
  return family_decl(str_lit("Counterfeit"), params, vec_new(heap->arena), level_one(heap));
}

/* counterfeit_ctor() */
static CtorDecl *counterfeit_ctor(Heap *heap) {
  Vec args = one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("value"), term_var(heap, nat_zero())));
  Vec res_params = one_item(heap, (void *)term_var(heap, nat_u64(heap, 1)));
  return ctor_box(heap, "counterfeit", args, res_params, vec_new(heap->arena));
}

/* universality() */
static Result universality(Heap *heap) {
  Vec none = vec_new(heap->arena);
  Vec group;
  Globals globals;
  Result step = check_declare_family_at(heap, global_empty(), budget_unlimited(), nat_zero(), counterfeit(heap, level_zero()));
  if (step.kind == RESULT_FAIL) return step;
  globals = globals_of(step);
  group = one_item(heap, str_box(heap, "Counterfeit"));
  step = check_define_ctors_at(heap, globals, budget_unlimited(), nat_zero(), &group, str_lit("Counterfeit"),
                               one_item(heap, counterfeit_ctor(heap)));
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      exact(heap, error_make(ERROR_UNIVERSE, str_lit("a field of counterfeit exceeds its family universe")),
            family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_u64(heap, 1),
                           counterfeit(heap, level_var(heap, nat_zero())), one_item(heap, counterfeit_ctor(heap)), &none)),
      "closed sample substituted for universal field checking");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(
      exact(heap,
            error_make(ERROR_UNIVERSE, str_lit("a family schema must remain in Prop or Type for every universe substitution")),
            family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_u64(heap, 1),
                           empty(heap, "Mixed", level_var(heap, nat_zero())), vec_new(heap->arena), &none)),
      "mixed Prop and Type family accepted");
}

/* measured(state): Done{10000 - remaining} for a finished finite budget. */
static Result measured(Heap *heap, BudgetState state) {
  if (state.budget.kind == BUDGET_POLLS && state.result.kind == RESULT_DONE) {
    Nat *used = nat_box(heap, nat_minus(heap, nat_u64(heap, 10000), state.budget.remaining));
    if (used == NULL) return budget_result_fail(host_oom_error());
    return budget_result_done(used);
  }
  if (state.result.kind == RESULT_FAIL) return state.result;
  return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("measurement lost finite budget")));
}

/* measured_error(expected, state) */
static Result measured_error(Heap *heap, Error expected, BudgetState state) {
  if (state.budget.kind == BUDGET_POLLS && state.result.kind == RESULT_FAIL) {
    Nat *used;
    if (!str_eq(error_to_string(heap, expected), error_to_string(heap, state.result.error))) return state.result;
    used = nat_box(heap, nat_minus(heap, nat_u64(heap, 10000), state.budget.remaining));
    if (used == NULL) return budget_result_fail(host_oom_error());
    return budget_result_done(used);
  }
  return budget_result_fail(error_make(ERROR_MISMATCH, str_lit("expected a measured refusal")));
}

/* sentinel(globals) */
static Comp sentinel(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(error_make(ERROR_CANNOT_INFER, str_lit("callback must not run")));
}

/* cancel_callback(globals) */
static Comp cancel_callback(Heap *heap, void *ctx, Globals globals) {
  Decl *box;
  (void)ctx;
  (void)globals;
  box = decl_box(heap, catalog_witness(heap));
  if (box == NULL) return budget_failed(host_oom_error());
  return budget_pure(box);
}

static CallbackPoll *cancel_poll(Heap *heap, void *ctx);

/* cancel_after(remaining): the poll spends on the remaining-th call; ctx is the boxed count. */
static Callback cancel_after(Heap *heap, Nat remaining) { return budget_of_poll(cancel_poll, nat_box(heap, remaining)); }

static CallbackPoll *cancel_poll(Heap *heap, void *ctx) {
  const Nat *remaining = ctx;
  CallbackPoll *poll;
  if (remaining == NULL) return NULL;
  poll = heap_alloc(heap, sizeof(CallbackPoll));
  if (poll == NULL) return NULL;
  if (nat_is_zero(*remaining)) {
    poll->next = cancel_after(heap, nat_zero());
    poll->spent = true;
    return poll;
  }
  poll->next = cancel_after(heap, nat_minus(heap, *remaining, nat_u64(heap, 1)));
  poll->spent = false;
  return poll;
}

/* [Common.Pair{Test.marker(), Test.ctors()}] */
static Vec marker_group(Heap *heap) { return one_item(heap, raw_box(heap, catalog_marker(heap), catalog_ctors(heap))); }

/* callback_boundaries() */
static Result callback_boundaries(Heap *heap) {
  Vec families = marker_group(heap);
  Vec none = vec_new(heap->arena);
  Vec sentinel_only = one_item(heap, elaborator_box(heap, sentinel));
  Vec cancel_then_sentinel = two_items(heap, elaborator_box(heap, cancel_callback), elaborator_box(heap, sentinel));
  Vec sentinel_then_second = two_items(heap, elaborator_box(heap, sentinel), elaborator_box(heap, catalog_second_callback));
  Comp action = family_declare_group_elaborated_comp(heap, global_empty(), family_empty(), nat_zero(), &families, &none);
  Nat used;
  Result step = measured(heap, budget_run_state(heap, action, budget_of_polls(nat_u64(heap, 10000))));
  if (step.kind == RESULT_FAIL) return step;
  used = nat_of(step);
  step = catalog_ensure(catalog_budget_refused(family_declare_group_elaborated(heap, global_empty(), family_empty(), budget_of_polls(used),
                                                                               nat_zero(), &families, &sentinel_only)),
                        "callback ran before the family/member budget guard");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(
      catalog_budget_refused(budget_run_poll(
          heap,
          family_declare_group_elaborated_comp(heap, global_empty(), family_empty(), nat_zero(), &families, &cancel_then_sentinel),
          cancel_after(heap, nat_add(heap->arena, nat_u64(heap, 1), used)))),
      "callback ran after member cancellation");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(exact(heap, error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")),
                              family_declare_group_elaborated(heap, global_empty(), family_empty(), budget_unlimited(), nat_zero(),
                                                              &none, &sentinel_only)),
                        "callback ran before family validation");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(exact(heap, error_make(ERROR_CANNOT_INFER, str_lit("callback must not run")),
                              family_declare_group_elaborated(heap, global_empty(), family_empty(), budget_unlimited(), nat_zero(),
                                                              &families, &sentinel_then_second)),
                        "later callback replaced first refusal");
  if (step.kind == RESULT_FAIL) return step;
  return budget_result_done(kernel_unit());
}

/* count(values) over the three cons lists the driver inspects. */
static uint64_t count_entries(const EntryRows *rows) { return rows == NULL ? 0 : 1 + count_entries(rows->rest); }

static uint64_t count_families(const FamilyRows *rows) { return rows == NULL ? 0 : 1 + count_families(rows->rest); }

static uint64_t count_catalog(FamilyCatalog catalog) { return catalog == NULL ? 0 : 1 + count_catalog(catalog->rest); }

/* inventories(globals, entries_count, families_count) */
static bool inventories(Globals globals, uint64_t entries_count, uint64_t families_count) {
  return count_entries(globals.entries) == entries_count && count_families(globals.families) == families_count;
}

/* dependency(name, alias) */
static FamilyDependency *dependency(Heap *heap, const char *name, const char *alias) {
  FamilyDependency *box = heap_alloc(heap, sizeof(FamilyDependency));
  if (box == NULL) {
    heap->oom = true;
    return NULL;
  }
  box->source = str_lit(name);
  box->levels = vec_new(heap->arena);
  box->alias = str_lit(alias);
  box->reuse = vec_new(heap->arena);
  box->exports = no_exports(heap);
  return box;
}

/* nested(catalog, budget, members) */
static Result nested(Heap *heap, FamilyCatalog catalog, Budget budget, const Vec *members) {
  Vec dependencies = one_item(heap, dependency(heap, "Inner", "Nested"));
  return family_compose(heap, global_empty(), catalog, budget, nat_zero(), str_lit("Outer"), &dependencies, members);
}

/* nested_comp(catalog, members): the dependency list outlives this frame, so it is boxed. */
static Comp nested_comp(Heap *heap, FamilyCatalog catalog, const Vec *members) {
  Vec *dependencies = vec_box(heap, one_item(heap, dependency(heap, "Inner", "Nested")));
  if (dependencies == NULL) return budget_failed(host_oom_error());
  return family_compose_comp(heap, global_empty(), catalog, nat_zero(), str_lit("Outer"), dependencies, members);
}

/* nested_boundaries() */
static Result nested_boundaries(Heap *heap) {
  Vec none = vec_new(heap->arena);
  Vec sentinel_then_second = two_items(heap, elaborator_box(heap, sentinel), elaborator_box(heap, catalog_second_callback));
  Vec sentinel_only = one_item(heap, elaborator_box(heap, sentinel));
  Vec inner_deps = one_item(heap, dependency(heap, "Marker", "Local"));
  Vec absent_deps = one_item(heap, dependency(heap, "Missing", "Local"));
  FamilyCatalog catalog;
  FamilyCatalog closed;
  Globals globals;
  Nat used;
  Nat failed_at;
  Nat instance_polls;
  Result step = family_declare(heap, global_empty(), family_empty(), budget_unlimited(), nat_zero(), catalog_marker(heap),
                               catalog_ctors(heap), &none);
  if (step.kind == RESULT_FAIL) return step;
  catalog = step.value;
  step = family_compose(heap, global_empty(), catalog, budget_unlimited(), nat_zero(), str_lit("Inner"), &inner_deps, &none);
  if (step.kind == RESULT_FAIL) return step;
  catalog = step.value;
  step = measured(heap, budget_run_state(heap, nested_comp(heap, catalog, &none), budget_of_polls(nat_u64(heap, 10000))));
  if (step.kind == RESULT_FAIL) return step;
  used = nat_of(step);
  step = catalog_ensure(catalog_budget_refused(nested(heap, catalog, budget_of_polls(nat_minus(heap, used, nat_u64(heap, 1))), &none)),
                        "nested composition accepted exhausted budget");
  if (step.kind == RESULT_FAIL) return step;
  step = nested(heap, catalog, budget_of_polls(used), &none);
  if (step.kind == RESULT_FAIL) return step;
  closed = step.value;
  step = catalog_ensure(count_catalog(catalog) == 2 && !family_lookup(catalog, str_lit("Outer")).some,
                        "failed nested composition changed caller catalog");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(count_catalog(closed) == 3, "nested composition inventory changed");
  if (step.kind == RESULT_FAIL) return step;
  step = measured_error(heap, error_make(ERROR_CANNOT_INFER, str_lit("callback must not run")),
                        budget_run_state(heap, nested_comp(heap, catalog, &sentinel_then_second), budget_of_polls(nat_u64(heap, 10000))));
  if (step.kind == RESULT_FAIL) return step;
  failed_at = nat_of(step);
  step = catalog_ensure(catalog_budget_refused(nested(heap, catalog, budget_of_polls(nat_minus(heap, failed_at, nat_u64(heap, 1))),
                                                      &sentinel_then_second)),
                        "nested member refusal preceded its budget guard");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(exact(heap, error_make(ERROR_CANNOT_INFER, str_lit("callback must not run")),
                              nested(heap, catalog, budget_of_polls(failed_at), &sentinel_then_second)),
                        "nested callback first error changed");
  if (step.kind == RESULT_FAIL) return step;
  step = measured(heap, budget_run_state(heap,
                                         family_instantiate_comp(heap, global_empty(), closed, str_lit("Outer"), &none,
                                                                 str_lit("Instance"), &none, no_exports(heap)),
                                         budget_of_polls(nat_u64(heap, 10000))));
  if (step.kind == RESULT_FAIL) return step;
  instance_polls = nat_of(step);
  step = catalog_ensure(
      catalog_budget_refused(family_instantiate(heap, global_empty(), closed, budget_of_polls(nat_minus(heap, instance_polls, nat_u64(heap, 1))),
                                                str_lit("Outer"), &none, str_lit("Instance"), &none, no_exports(heap))),
      "nested specialization accepted exhausted budget");
  if (step.kind == RESULT_FAIL) return step;
  step = family_instantiate(heap, global_empty(), closed, budget_of_polls(instance_polls), str_lit("Outer"), &none, str_lit("Instance"),
                            &none, no_exports(heap));
  if (step.kind == RESULT_FAIL) return step;
  globals = globals_of(step);
  step = catalog_ensure(inventories(globals, 0, 1), "nested specialization family inventory changed");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(catalog_budget_refused(family_compose(heap, global_empty(), catalog, budget_of_polls(nat_u64(heap, 1)), nat_zero(),
                                                              str_lit("Absent"), &absent_deps, &sentinel_only)),
                        "dependency lookup preceded its budget guard");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(inventories(global_empty(), 0, 0) && count_catalog(closed) == 3,
                        "failed specialization altered caller inventories");
}

/* suite() */
static Result suite(Heap *heap) {
  Result step = scope_fields(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = universality(heap);
  if (step.kind == RESULT_FAIL) return step;
  step = callback_boundaries(heap);
  if (step.kind == RESULT_FAIL) return step;
  return nested_boundaries(heap);
}

/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE)
    return driver_print(str_lit("PASS raw family scope, universality, callback cancellation, nested budgets and inventories"));
  return driver_die_text(1, error_to_string(heap, result.error));
}

/* main() */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 24);
  Heap heap = heap_new(&arena);
  int code = report(&heap, suite(&heap));
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
