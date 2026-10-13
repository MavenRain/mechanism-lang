/* Port of bend2/tests/surface_catalog.bend (oracle label unit-surface-catalog): raw family catalog trust
 * boundaries. surface_boundary and surface_shape_metadata import it as Test, so the shared definitions
 * carry the catalog_ prefix (c/tests/surface_catalog.h) and main compiles only outside
 * MECH_DRIVER_LIBRARY (the levels_regression precedent). */
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

static MaybeTermPtr some_term(const Term *term) {
  MaybeTermPtr body;
  body.some = true;
  body.value = term;
  return body;
}

static MaybeTermPtr no_term(void) {
  MaybeTermPtr body;
  body.some = false;
  body.value = NULL;
  return body;
}

static Decl decl_of(Str name, DeclKind kind, const Term *ty, MaybeTermPtr body) {
  Decl decl;
  decl.name = name;
  decl.kind = kind;
  decl.ty = ty;
  decl.body = body;
  return decl;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *box = heap_alloc(heap, sizeof(Decl));
  if (box != NULL) *box = decl;
  return box;
}

static CtorDecl *mark_ctor(Heap *heap) {
  CtorDecl *mark = heap_alloc(heap, sizeof(CtorDecl));
  if (mark == NULL) return NULL;
  mark->name = str_lit("mark");
  mark->args = vec_new(heap->arena);
  mark->res_params = vec_new(heap->arena);
  mark->res_idx = vec_new(heap->arena);
  return mark;
}

static FamilyRaw *raw_box(Heap *heap, FamilyDecl family, Vec ctors) {
  FamilyRaw *raw = heap_alloc(heap, sizeof(FamilyRaw));
  if (raw == NULL) return NULL;
  raw->family = family;
  raw->ctors = ctors;
  return raw;
}

static FamilyMemberElaborator *elaborator_box(Heap *heap, Comp (*fn)(Heap *, void *, Globals)) {
  FamilyMemberElaborator *box = heap_alloc(heap, sizeof(FamilyMemberElaborator));
  if (box == NULL) return NULL;
  box->fn = fn;
  box->ctx = NULL;
  return box;
}

static PolyRename *rename_box(Heap *heap, const char *source, const char *target) {
  PolyRename *rename = heap_alloc(heap, sizeof(PolyRename));
  if (rename == NULL) return NULL;
  rename->source = str_lit(source);
  rename->target = str_lit(target);
  return rename;
}

static FamilyDependency *dependency_box(Heap *heap, Str source, Vec levels, Str alias) {
  FamilyDependency *dependency = heap_alloc(heap, sizeof(FamilyDependency));
  if (dependency == NULL) return NULL;
  dependency->source = source;
  dependency->levels = levels;
  dependency->alias = alias;
  dependency->reuse = vec_new(heap->arena);
  dependency->exports.some = false;
  dependency->exports.value = vec_new(heap->arena);
  return dependency;
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
}

static FamilyMaybeVec some_exports(Vec renames) {
  FamilyMaybeVec exports;
  exports.some = true;
  exports.value = renames;
  return exports;
}

/* ensure(ok, message) */
Result catalog_ensure(bool ok, const char *message) {
  if (ok) return budget_result_done(kernel_unit());
  return budget_result_fail(error_make(ERROR_MISMATCH, str_lit(message)));
}

/* marker() */
FamilyDecl catalog_marker(Heap *heap) {
  FamilyDecl marker;
  marker.name = str_lit("Marker");
  marker.params = vec_new(heap->arena);
  marker.indices = vec_new(heap->arena);
  marker.level = level_one(heap);
  return marker;
}

/* ctors() */
Vec catalog_ctors(Heap *heap) { return one_item(heap, mark_ctor(heap)); }

/* marker_ty(name) */
const Term *catalog_marker_ty(Heap *heap, Str name) {
  Vec none = vec_new(heap->arena);
  return term_lan(heap, shape_smu(name, none), rules_diagram_of(heap, &none));
}

/* witness() */
Decl catalog_witness(Heap *heap) {
  Vec none = vec_new(heap->arena);
  const Term *body = term_in(heap, shape_smu(str_lit("Marker"), none), term_addr_ctor(str_lit("mark")), none);
  return decl_of(str_lit("witness"), DECL_DEFINITION, catalog_marker_ty(heap, str_lit("Marker")), some_term(body));
}

/* alias() */
Decl catalog_alias(Heap *heap) {
  return decl_of(str_lit("alias"), DECL_DEFINITION, catalog_marker_ty(heap, str_lit("Marker")),
                 some_term(term_global(heap, str_lit("witness"))));
}

/* member_alias(value) */
static bool member_alias(Heap *heap, MaybeEntry value) {
  const Term *body;
  if (!value.some || value.value.kind != ENTRY_DEF) return false;
  body = value.value.def.body;
  if (body->kind != TERM_GLOBAL) return false;
  return term_equal(value.value.def.ty, catalog_marker_ty(heap, str_lit("Small"))) && str_eq(body->name, str_lit("Small_witness"));
}

/* refused(result) */
static bool refused(Result result) { return result.kind == RESULT_FAIL; }

/* scope_refused(result) */
bool catalog_scope_refused(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_UNIVERSE &&
         str_eq(result.error.message, str_lit("universe level is outside the global parameter scope"));
}

/* budget_refused(result) */
bool catalog_budget_refused(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED;
}

/* forged(members): a catalog whose single "Marker" scheme carries the given members unchecked. */
static FamilyCatalog forged(Heap *heap, Vec members) {
  FamilyRow *row = heap_alloc(heap, sizeof(FamilyRow));
  if (row == NULL) {
    heap->oom = true;
    return family_empty();
  }
  row->name = str_lit("Marker");
  row->scheme.arity = nat_zero();
  row->scheme.family = catalog_marker(heap);
  row->scheme.ctors = catalog_ctors(heap);
  row->scheme.companions = vec_new(heap->arena);
  row->scheme.members = members;
  row->scheme.origin = global_empty();
  row->rest = NULL;
  return row;
}

static Comp pure_decl(Heap *heap, Decl decl) {
  Decl *box = decl_box(heap, decl);
  if (box == NULL) {
    heap->oom = true;
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("host out of memory")));
  }
  return budget_pure(box);
}

static Comp first_return(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_decl(heap, catalog_witness(heap));
}

/* first_callback(globals) */
static Comp first_callback(Heap *heap, void *ctx, Globals globals) {
  NextK next;
  (void)ctx;
  next.fn = first_return;
  next.ctx = NULL;
  return budget_bind(heap,
                     budget_lift(catalog_ensure(!global_find(str_lit("witness"), globals).some, "first callback saw a future member")),
                     next);
}

static Comp second_return(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_decl(heap, catalog_alias(heap));
}

/* second_callback(globals) */
Comp catalog_second_callback(Heap *heap, void *ctx, Globals globals) {
  NextK next;
  (void)ctx;
  next.fn = second_return;
  next.ctx = NULL;
  return budget_bind(heap,
                     budget_lift(catalog_ensure(global_find(str_lit("witness"), globals).some, "second callback missed prior member")),
                     next);
}

/* failed_callback(globals) */
static Comp failed_callback(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(error_make(ERROR_MISMATCH, str_lit("first callback failure")));
}

/* callback_error(result) */
static bool callback_error(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_MISMATCH &&
         str_eq(result.error.message, str_lit("first callback failure"));
}

static Decl forged_decl(Heap *heap, const char *name, DeclKind kind, MaybeTermPtr body) {
  return decl_of(str_lit(name), kind, catalog_marker_ty(heap, str_lit("Marker")), body);
}

/* suite() */
static Result suite(Heap *heap) {
  Nat zero = nat_zero();
  Nat one = heap_nat_of_u64(heap, 1);
  Str marker_name = str_lit("Marker");
  Vec no_reuse = vec_new(heap->arena);
  Vec no_levels = vec_new(heap->arena);
  Vec no_members = vec_new(heap->arena);
  Vec level_one_list = one_item(heap, (void *)level_one(heap));
  Vec group = one_item(heap, raw_box(heap, catalog_marker(heap), catalog_ctors(heap)));
  Vec first_then_second = two_items(heap, elaborator_box(heap, first_callback), elaborator_box(heap, catalog_second_callback));
  Vec failed_then_second = two_items(heap, elaborator_box(heap, failed_callback), elaborator_box(heap, catalog_second_callback));
  Vec witness_alias = two_items(heap, decl_box(heap, catalog_witness(heap)), decl_box(heap, catalog_alias(heap)));
  Vec exports = two_items(heap, rename_box(heap, "witness", "make"), rename_box(heap, "alias", "get"));
  Vec reuse_small = one_item(heap, rename_box(heap, "Marker", "Small"));
  Vec pack_deps = one_item(heap, dependency_box(heap, marker_name, one_item(heap, (void *)level_var(heap, zero)), str_lit("Inner")));
  Vec inner_deps = one_item(heap, dependency_box(heap, marker_name, vec_new(heap->arena), str_lit("Inner")));
  Vec postulate_members = one_item(heap, decl_box(heap, forged_decl(heap, "postulate", DECL_POSTULATE, no_term())));
  Vec missing_members = one_item(heap, decl_box(heap, forged_decl(heap, "missing", DECL_DEFINITION, no_term())));
  Vec wrong_members = one_item(heap, decl_box(heap, forged_decl(heap, "wrong", DECL_DEFINITION, some_term(term_univ(heap, level_one(heap))))));
  Vec wrong_again = one_item(heap, decl_box(heap, forged_decl(heap, "wrong", DECL_DEFINITION, some_term(term_univ(heap, level_one(heap))))));
  Vec hidden_members = one_item(
      heap, decl_box(heap, forged_decl(heap, "hidden", DECL_DEFINITION,
                                       some_term(term_ann(heap, term_auto(), term_univ(heap, level_var(heap, zero)))))));
  Vec duplicate_ctors = two_items(heap, mark_ctor(heap), mark_ctor(heap));
  Result step;
  FamilyCatalog catalog;
  FamilyCatalog composed;
  Globals globals;
  Globals exported;
  Globals reused;
  Globals packed;

  step = family_declare_group_elaborated(heap, global_empty(), family_empty(), budget_unlimited(), zero, &group, &first_then_second);
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(callback_error(family_declare_group_elaborated(heap, global_empty(), family_empty(), budget_unlimited(), zero,
                                                                       &group, &failed_then_second)),
                        "later callback replaced first error");
  if (step.kind == RESULT_FAIL) return step;
  step = family_declare(heap, global_empty(), family_empty(), budget_unlimited(), one, catalog_marker(heap), catalog_ctors(heap),
                        &witness_alias);
  if (step.kind == RESULT_FAIL) return step;
  catalog = step.value;
  step = family_instantiate(heap, global_empty(), catalog, budget_unlimited(), marker_name, &level_one_list, str_lit("Small"), &no_reuse,
                            no_exports(heap));
  if (step.kind == RESULT_FAIL) return step;
  globals = *(Globals *)step.value;
  step = catalog_ensure(member_alias(heap, global_find(str_lit("Small_alias"), globals)), "member global reference was not renamed");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(!global_find(str_lit("witness"), globals).some, "symbolic member escaped");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(!family_lookup(catalog, str_lit("Small")).some, "specialization changed catalog");
  if (step.kind == RESULT_FAIL) return step;
  step = family_instantiate(heap, globals, catalog, budget_unlimited(), marker_name, &level_one_list, str_lit("Again"), &no_reuse,
                            no_exports(heap));
  if (step.kind == RESULT_FAIL) return step;
  step = family_instantiate(heap, global_empty(), catalog, budget_unlimited(), marker_name, &level_one_list, str_lit("Exported"),
                            &no_reuse, some_exports(exports));
  if (step.kind == RESULT_FAIL) return step;
  exported = *(Globals *)step.value;
  step = catalog_ensure(global_find(str_lit("get"), exported).some, "export target missing");
  if (step.kind == RESULT_FAIL) return step;
  step = family_instantiate(heap, globals, catalog, budget_unlimited(), marker_name, &level_one_list, str_lit("Reused"), &reuse_small,
                            no_exports(heap));
  if (step.kind == RESULT_FAIL) return step;
  reused = *(Globals *)step.value;
  step = catalog_ensure(!global_find(marker_name, reused).some, "reused symbolic family escaped");
  if (step.kind == RESULT_FAIL) return step;
  step = family_compose(heap, global_empty(), catalog, budget_unlimited(), one, str_lit("Pack"), &pack_deps, &no_members);
  if (step.kind == RESULT_FAIL) return step;
  composed = step.value;
  step = family_instantiate(heap, global_empty(), composed, budget_unlimited(), str_lit("Pack"), &level_one_list, str_lit("Packed"),
                            &no_reuse, no_exports(heap));
  if (step.kind == RESULT_FAIL) return step;
  packed = *(Globals *)step.value;
  step = catalog_ensure(global_find(str_lit("Packed_Inner_alias"), packed).some, "composed member did not survive specialization");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_instantiate(heap, global_empty(), catalog, budget_unlimited(), marker_name, &level_one_list,
                                                   str_lit("Bad"), &no_reuse, some_exports(vec_new(heap->arena)))),
                        "incomplete export accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_instantiate(heap, global_empty(), forged(heap, postulate_members), budget_unlimited(), marker_name,
                                                   &no_levels, str_lit("Bad"), &no_reuse, no_exports(heap))),
                        "forged postulate member accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_instantiate(heap, global_empty(), forged(heap, missing_members), budget_unlimited(), marker_name,
                                                   &no_levels, str_lit("Bad"), &no_reuse, no_exports(heap))),
                        "forged missing member body accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_instantiate(heap, global_empty(), forged(heap, wrong_members), budget_unlimited(), marker_name,
                                                   &no_levels, str_lit("Bad"), &no_reuse, no_exports(heap))),
                        "forged ill-typed member accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_compose(heap, global_empty(), forged(heap, wrong_again), budget_unlimited(), zero, str_lit("Pack"),
                                               &inner_deps, &no_members)),
                        "composition accepted forged ill-typed member");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(catalog_scope_refused(family_declare(heap, global_empty(), family_empty(), budget_unlimited(), zero,
                                                             catalog_marker(heap), catalog_ctors(heap), &hidden_members)),
                        "hidden annotation escaped universe scope");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(refused(family_declare(heap, global_empty(), family_empty(), budget_unlimited(), zero, catalog_marker(heap),
                                               duplicate_ctors, &no_members)),
                        "duplicate constructor accepted");
  if (step.kind == RESULT_FAIL) return step;
  step = catalog_ensure(catalog_budget_refused(family_declare(heap, global_empty(), family_empty(), budget_of_polls(zero), zero,
                                                              catalog_marker(heap), catalog_ctors(heap), &no_members)),
                        "declaration ignored exhausted budget");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(catalog_budget_refused(family_instantiate(heap, global_empty(), catalog, budget_of_polls(zero), marker_name,
                                                                  &level_one_list, str_lit("Bad"), &no_reuse, no_exports(heap))),
                        "specialization ignored exhausted budget");
}

#ifndef MECH_DRIVER_LIBRARY
/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return driver_print(str_lit("PASS raw family catalog trust boundaries"));
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
#endif
