/* Port of tests/surface_family_members.bend (264 lines, oracle label surface-family-members): family
 * schema members, their renaming on specialization, explicit export names and their refusals, family reuse,
 * hidden universe levels, closed rechecking and the member budgets, 35 cases in Bend order. With no argv it runs
 * every case and prints FAMILY-MEMBERS-OK cases=<n> or FAMILY-MEMBERS-FAIL <case>: <message>; argv[1] selects
 * one case by name (unknown case: <name> when no case has it). Links _tcc/surface_family_groups_lib.o for the
 * Groups helpers (c/tests/surface_family_groups.h). */
#include "driver.h"
#include "prelude.h"
#include "surface_foundation_common.h"
#include "surface_raw_common.h"
#include "surface_family_groups.h"

/* A one-item Bend list literal; a NULL item or a failed push sets heap->oom. */
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

static Vec three_items(Heap *heap, void *first, void *second, void *third) {
  Vec items = two_items(heap, first, second);
  if (third == NULL) heap->oom = true;
  else if (!vec_push(&items, third)) heap->oom = true;
  return items;
}

static Str *str_box(Heap *heap, const char *text) {
  Str *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) heap->oom = true;
  else *box = str_lit(text);
  return box;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) heap->oom = true;
  else *box = decl;
  return box;
}

static FamilyRaw *raw_box(Heap *heap, FamilyDecl family, Vec ctors) {
  FamilyRaw *raw = heap_alloc(heap, sizeof *raw);
  if (raw == NULL) {
    heap->oom = true;
    return NULL;
  }
  raw->family = family;
  raw->ctors = ctors;
  return raw;
}

/* C.Pair{source, target}: one Family.Bindings row. */
static PolyRename *rename_box(Heap *heap, const char *source, const char *target) {
  PolyRename *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) {
    heap->oom = true;
    return NULL;
  }
  row->source = str_lit(source);
  row->target = str_lit(target);
  return row;
}

/* A two-row Family.Bindings literal. */
static Vec two_renames(Heap *heap, const char *source1, const char *target1, const char *source2, const char *target2) {
  return two_items(heap, rename_box(heap, source1, target1), rename_box(heap, source2, target2));
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
}

static FamilyMaybeVec some_exports(Vec bindings) {
  FamilyMaybeVec exports;
  exports.some = true;
  exports.value = bindings;
  return exports;
}

static Error failure(ErrorKind kind, const char *message) { return error_make(kind, str_lit(message)); }

static Task host_oom(Heap *heap) {
  heap->oom = true;
  return task_fail(str_lit("out of memory"));
}

/* R.kernel(Family.T, result): the Done value is the catalog row pointer. */
static Task kernel_catalog(Heap *heap, Result result, FamilyCatalog *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, result, &value));
  *out = value;
  return task_pure();
}

/* R.kernel(G.T, result): the Done value is a boxed Globals. */
static Task kernel_globals(Heap *heap, Result result, Globals *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, result, &value));
  if (value == NULL) return host_oom(heap);
  *out = *(const Globals *)value;
  return task_pure();
}

static const Term *ty(Heap *heap, const char *name) { return surface_family_groups_ty(heap, str_lit(name)); }

/* Groups.value(name, []) */
static const Term *value(Heap *heap, const char *name) {
  return surface_family_groups_value(heap, str_lit(name), vec_new(heap->arena));
}

static Decl definition(const char *name, const Term *type, const Term *body) {
  return surface_family_groups_definition(str_lit(name), type, body);
}

static Globals with_entry(Heap *heap, const char *name) { return surface_family_groups_with_entry(heap, str_lit(name)); }

/* P.Binder{quantity, name, ty} by value, for D.arrow and D.lambda. */
static TeleBinder binder(Quantity quantity, const char *name, const Term *type) {
  TeleBinder entry;
  entry.quantity = quantity;
  entry.name = str_lit(name);
  entry.ty = type;
  return entry;
}

/* The decimal text of a measured poll count (Nat.show). */
static Str show(Heap *heap, uint64_t count) { return heap_nat_show(heap, heap_nat_of_u64(heap, count)); }

/* Nat subtraction by one, floored at zero. */
static uint64_t pred(uint64_t count) {
  if (count == 0) return 0;
  return count - 1;
}

/* witness() */
static Decl witness(Heap *heap) { return definition("witness", ty(heap, "Marker"), value(heap, "Marker")); }

/* alias() */
static Decl alias(Heap *heap) { return definition("alias", ty(heap, "Marker"), term_global(heap, str_lit("witness"))); }

/* identity(level) */
static Decl identity(Heap *heap, const LevelVar *level) {
  const Term *type =
      dependent_arrow(heap, binder(QUANTITY_ZERO, "A", term_univ(heap, level)),
                      dependent_arrow(heap, binder(QUANTITY_MANY, "value", term_var(heap, nat_zero())),
                                      term_var(heap, heap_nat_of_u64(heap, 1))));
  const Term *body = dependent_lambda(
      heap, binder(QUANTITY_ZERO, "A", term_univ(heap, level)),
      dependent_lambda(heap, binder(QUANTITY_MANY, "value", term_var(heap, nat_zero())), term_var(heap, nat_zero())));
  return definition("identity", type, body);
}

/* [witness(), alias()] */
static Vec witness_alias(Heap *heap) { return two_items(heap, decl_box(heap, witness(heap)), decl_box(heap, alias(heap))); }

/* declaration(globals, catalog, members); the comp keeps the members pointer, so the caller owns it until the
 * run ends. */
static Comp declaration(Heap *heap, Globals globals, FamilyCatalog catalog, const Vec *members) {
  return family_declare_comp(heap, globals, catalog, heap_nat_of_u64(heap, 1),
                             surface_family_groups_marker(heap, str_lit("Marker")),
                             one_item(heap, surface_family_groups_ctor(heap)), members);
}

/* declare(globals, catalog, members) */
static Result declare(Heap *heap, Globals globals, FamilyCatalog catalog, const Vec *members) {
  return budget_run(heap, declaration(heap, globals, catalog, members), budget_unlimited());
}

/* Family.instantiate(globals, catalog, B.unlimited(), "Marker", [L.one()], name, reuse, exports) */
static Result instantiate_reuse(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, const Vec *reuse,
                                FamilyMaybeVec exports) {
  Vec levels = one_item(heap, (void *)level_one(heap));
  return family_instantiate(heap, globals, catalog, budget_unlimited(), str_lit("Marker"), &levels, str_lit(name), reuse,
                            exports);
}

/* instantiate(globals, catalog, name, exports) */
static Result instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, FamilyMaybeVec exports) {
  Vec reuse = vec_new(heap->arena);
  return instantiate_reuse(heap, globals, catalog, name, &reuse, exports);
}

/* The [L.one()] levels and [] reuse of instance(); the comp keeps both pointers, so the case function owns them
 * until its runs end. */
typedef struct {
  Vec levels;
  Vec reuse;
} InstanceArgs;

static InstanceArgs instance_args(Heap *heap) {
  InstanceArgs args;
  args.levels = one_item(heap, (void *)level_one(heap));
  args.reuse = vec_new(heap->arena);
  return args;
}

/* instance(globals, catalog, name, exports) */
static Comp instance(Heap *heap, const InstanceArgs *args, Globals globals, FamilyCatalog catalog, const char *name,
                     FamilyMaybeVec exports) {
  return family_instantiate_comp(heap, globals, catalog, str_lit("Marker"), &args->levels, str_lit(name), &args->reuse,
                                 exports);
}

/* inventory(value) */
static bool inventory(Heap *heap, FamilyMaybeVec names) {
  if (!names.some) return false;
  Vec expected = three_items(heap, str_box(heap, "witness"), str_box(heap, "alias"), str_box(heap, "identity"));
  return relational_strings(&names.value, &expected);
}

/* alias_entry(entry) */
static Task alias_entry(Heap *heap, MaybeEntry entry) {
  if (!entry.some) return task_fail(str_lit("alias missing"));
  switch (entry.value.kind) {
    case ENTRY_DEF:
      return relational_require(term_equal(entry.value.def.body, term_global(heap, str_lit("First_witness"))) &&
                                    term_equal(entry.value.def.ty, ty(heap, "First")),
                                str_lit("member reference was not renamed"));
    case ENTRY_AXIOM:
      return task_fail(str_lit("member is not a definition"));
    case ENTRY_PRIM:
      return task_fail(str_lit("member is not a definition"));
  }
  return task_fail(str_lit("member is not a definition"));
}

/* positive() */
static Task positive(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals first;
  Globals globals;
  const ElabChecked *checked = NULL;
  Vec members = three_items(heap, decl_box(heap, witness(heap)), decl_box(heap, alias(heap)),
                            decl_box(heap, identity(heap, level_var(heap, nat_zero()))));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(relational_require(inventory(heap, family_members(heap, catalog, str_lit("Marker"))) &&
                                        surface_raw_common_absent(family_members(heap, catalog, str_lit("missing")).some),
                                    str_lit("member inventory differs")));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "First", no_exports(heap)), &first));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, first, catalog, "Second", no_exports(heap)), &globals));
  RELATIONAL_TRY(relational_require(surface_raw_common_absent(global_find(str_lit("witness"), globals).some) &&
                                        surface_raw_common_absent(global_find_family(str_lit("Marker"), globals).some),
                                    str_lit("symbolic names escaped into ordinary globals")));
  RELATIONAL_TRY(relational_checked(heap, globals,
                                    str_lit("def firstClient : First := First_identity First First_alias\n"
                                            "def secondClient : Second := Second_identity Second Second_alias\n"),
                                    &checked));
  RELATIONAL_TRY(alias_entry(heap, global_find(str_lit("First_alias"), globals)));
  return surface_raw_common_refusal(heap, failure(ERROR_MISMATCH, "the term has type"),
                                    elab_check_in(heap, globals, budget_unlimited(), str_lit("def mixed : First := Second_alias\n")));
}

/* chosen() */
static Vec chosen(Heap *heap) { return two_renames(heap, "witness", "chosenWitness", "alias", "chosenAlias"); }

/* planned(value, families, members) */
static bool planned(MaybeFamilyInstanceNames plan, const Vec *families, const Vec *members) {
  if (!plan.some) return false;
  return relational_strings(&plan.value.families, families) && relational_strings(&plan.value.members, members);
}

/* name_plan(catalog, reuse, exports) */
static MaybeFamilyInstanceNames name_plan(Heap *heap, FamilyCatalog catalog, const Vec *reuse, Vec exports) {
  return family_instance_names(heap, catalog, str_lit("Marker"), str_lit("Chosen"), reuse, some_exports(exports));
}

/* no_plan(value) */
static bool no_plan(MaybeFamilyInstanceNames plan) { return surface_raw_common_absent(plan.some); }

/* bad_exports(expected, exports) */
static Task bad_exports(Heap *heap, Error expected, Vec exports) {
  FamilyCatalog catalog = family_empty();
  Vec members = witness_alias(heap);
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(relational_require(no_plan(name_plan(heap, catalog, &reuse, exports)),
                                    str_lit("invalid exports produced a name plan")));
  return surface_raw_common_refusal(heap, expected,
                                    instantiate(heap, global_empty(), catalog, "Chosen", some_exports(exports)));
}

/* List.reverse(items) */
static Vec reversed(Heap *heap, const Vec *items) {
  Vec out = vec_new(heap->arena);
  for (size_t i = items->len; i > 0; i--) {
    VecGet got = vec_get(items, i - 1);
    if (got.status == VEC_GET_OK && !vec_push(&out, got.value)) heap->oom = true;
  }
  return out;
}

/* chosen_names() */
static Task chosen_names(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals globals;
  const ElabChecked *checked = NULL;
  Vec members = witness_alias(heap);
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  Vec forward = chosen(heap);
  Vec exports = reversed(heap, &forward);
  Vec families = one_item(heap, str_box(heap, "Chosen"));
  Vec names = two_items(heap, str_box(heap, "chosenWitness"), str_box(heap, "chosenAlias"));
  RELATIONAL_TRY(relational_require(planned(name_plan(heap, catalog, &reuse, exports), &families, &names),
                                    str_lit("export name plan differs")));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "Chosen", some_exports(exports)), &globals));
  RELATIONAL_TRY(relational_checked(heap, globals,
                                    str_lit("mu ExportWitness : (0 x : Chosen) -> Type 0 with\n"
                                            "| exportWitness : ExportWitness chosenWitness\n"
                                            "def exportedComputes : ExportWitness chosenAlias := exportWitness\n"),
                                    &checked));
  return relational_require(surface_raw_common_absent(global_find(str_lit("Chosen_witness"), globals).some) &&
                                surface_raw_common_absent(global_find(str_lit("Chosen_alias"), globals).some),
                            str_lit("default names escaped an explicit export mapping"));
}

/* export_companion() */
static Task export_companion(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Vec group = two_items(heap,
                        raw_box(heap, surface_family_groups_marker(heap, str_lit("Marker")),
                                one_item(heap, surface_family_groups_ctor(heap))),
                        raw_box(heap, surface_family_groups_marker(heap, str_lit("Companion")),
                                one_item(heap, surface_family_groups_ctor(heap))));
  Vec members = witness_alias(heap);
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap,
                                family_declare_group(heap, global_empty(), family_empty(), budget_unlimited(),
                                                     heap_nat_of_u64(heap, 1), &group, &members),
                                &catalog));
  Vec exports = two_renames(heap, "witness", "Chosen_Companion", "alias", "chosenAlias");
  RELATIONAL_TRY(relational_require(no_plan(name_plan(heap, catalog, &reuse, exports)),
                                    str_lit("companion collision produced a name plan")));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Chosen_Companion")),
                                    instantiate(heap, global_empty(), catalog, "Chosen", some_exports(exports)));
}

/* export_ambient_constructor() */
static Task export_ambient_constructor(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  const ElabChecked *ambient = NULL;
  Globals installed;
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), str_lit("mu Flag : Type 0 with\n| other : Flag\n"), &ambient));
  Globals globals = relational_globals(ambient);
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, globals, catalog, "Chosen", some_exports(chosen(heap))), &installed));
  return surface_raw_common_refusal(
      heap, surface_raw_common_collision(heap, str_lit("other")),
      instantiate(heap, globals, catalog, "Chosen", some_exports(two_renames(heap, "witness", "other", "alias", "chosenAlias"))));
}

/* reuse_as_name() */
static Task reuse_as_name(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals globals;
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "First", no_exports(heap)), &globals));
  Vec reuse = one_item(heap, rename_box(heap, "Marker", "First"));
  Vec exports = two_renames(heap, "witness", "Chosen", "alias", "chosenAlias");
  RELATIONAL_TRY(relational_require(no_plan(name_plan(heap, catalog, &reuse, exports)),
                                    str_lit("as_name collision under reuse produced a name plan")));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Chosen")),
                                    instantiate_reuse(heap, globals, catalog, "Chosen", &reuse, some_exports(exports)));
}

/* reuse_exports() */
static Task reuse_exports(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals globals;
  Globals installed;
  const ElabChecked *checked = NULL;
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "First", no_exports(heap)), &globals));
  Vec reuse = one_item(heap, rename_box(heap, "Marker", "First"));
  Vec families = vec_new(heap->arena);
  Vec names = two_items(heap, str_box(heap, "chosenWitness"), str_box(heap, "chosenAlias"));
  RELATIONAL_TRY(relational_require(planned(name_plan(heap, catalog, &reuse, chosen(heap)), &families, &names),
                                    str_lit("reused family leaked into exported name plan")));
  RELATIONAL_TRY(kernel_globals(heap, instantiate_reuse(heap, globals, catalog, "Chosen", &reuse, some_exports(chosen(heap))),
                                &installed));
  RELATIONAL_TRY(relational_checked(heap, installed, str_lit("def reused : First := chosenAlias"), &checked));
  return surface_raw_common_refusal(
      heap, surface_raw_common_collision(heap, str_lit("First")),
      instantiate_reuse(heap, globals, catalog, "Chosen", &reuse,
                        some_exports(two_renames(heap, "witness", "First", "alias", "otherAlias"))));
}

/* Raw.measure(G.T, B.run_state(G.T, comp, B.of_polls(100000n))) */
static Task measure(Heap *heap, Comp comp, uint64_t *count) {
  return surface_raw_common_measure(heap, budget_run_state(heap, comp, budget_of_polls(heap_nat_of_u64(heap, 100000))),
                                    count);
}

/* "<label> " ++ Nat.show(actual) ++ ", expected " ++ Nat.show(expected) */
static Str polls_message(Heap *heap, const char *label, uint64_t actual, uint64_t expected) {
  Str head = str_cat(heap->arena, str_lit(label), show(heap, actual));
  return str_cat(heap->arena, str_cat(heap->arena, head, str_lit(", expected ")), show(heap, expected));
}

/* export_budget() */
static Task export_budget(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals fitted;
  uint64_t plain = 0;
  uint64_t exported = 0;
  InstanceArgs args = instance_args(heap);
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(measure(heap, instance(heap, &args, global_empty(), catalog, "Plain", no_exports(heap)), &plain));
  RELATIONAL_TRY(
      measure(heap, instance(heap, &args, global_empty(), catalog, "Chosen", some_exports(chosen(heap))), &exported));
  RELATIONAL_TRY(relational_require(exported == plain + 2, polls_message(heap, "export polls ", exported, plain + 2)));
  RELATIONAL_TRY(kernel_globals(heap,
                                budget_run_poll(heap, instance(heap, &args, global_empty(), catalog, "Fitted", no_exports(heap)),
                                                surface_foundation_common_countdown(heap, plain)),
                                &fitted));
  RELATIONAL_TRY(surface_raw_common_refusal(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, instance(heap, &args, global_empty(), catalog, "Chosen", some_exports(chosen(heap))),
                      surface_foundation_common_countdown(heap, plain))));
  return surface_raw_common_refusal(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap,
                      instance(heap, &args, global_empty(), catalog, "Chosen",
                               some_exports(two_renames(heap, "witness", "same", "alias", "same"))),
                      surface_foundation_common_countdown(heap, 1)));
}

/* export_rollback() */
static Task export_rollback(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals installed;
  uint64_t count = 0;
  InstanceArgs args = instance_args(heap);
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  Globals prepared = with_entry(heap, "seed");
  RELATIONAL_TRY(measure(heap, instance(heap, &args, prepared, catalog, "Chosen", some_exports(chosen(heap))), &count));
  RELATIONAL_TRY(surface_raw_common_refusal(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, instance(heap, &args, prepared, catalog, "Chosen", some_exports(chosen(heap))),
                      surface_foundation_common_countdown(heap, pred(count)))));
  RELATIONAL_TRY(relational_require(surface_raw_common_absent(global_find(str_lit("chosenWitness"), prepared).some) &&
                                        surface_raw_common_absent(global_find(str_lit("chosenAlias"), prepared).some) &&
                                        surface_raw_common_absent(global_find_family(str_lit("Chosen"), prepared).some),
                                    str_lit("a refused run changed the input environment")));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, prepared, catalog, "Chosen", some_exports(chosen(heap))), &installed));
  return relational_require(surface_raw_common_present(global_find(str_lit("chosenWitness"), installed).some) &&
                                surface_raw_common_present(global_find(str_lit("chosenAlias"), installed).some) &&
                                surface_raw_common_present(global_find_family(str_lit("Chosen"), installed).some),
                            str_lit("the repeated run installed no exported member"));
}

/* hidden_scope(level) */
static Decl hidden_scope(Heap *heap, const LevelVar *level) {
  const Term *domain = term_univ(heap, level_one(heap));
  const Term *identity_term = term_ann(heap, dependent_lambda(heap, binder(QUANTITY_MANY, "A", domain), term_var(heap, nat_zero())),
                                       dependent_arrow(heap, binder(QUANTITY_MANY, "A", domain), domain));
  Shape ignored = shape_spi(QUANTITY_MANY, str_lit("ignored"), (void *)term_univ(heap, level));
  return definition("hidden", domain,
                    term_out(heap, ignored, term_addr_pt(QUANTITY_MANY, term_univ(heap, level_zero())), identity_term));
}

/* hidden_body(decl, entry) */
static Task hidden_body(Decl decl, MaybeEntry entry) {
  if (!entry.some) return task_fail(str_lit("hidden member missing"));
  switch (entry.value.kind) {
    case ENTRY_DEF:
      if (decl.body.some) {
        return relational_require(term_equal(entry.value.def.body, decl.body.value),
                                  str_lit("a hidden universe argument was not specialized"));
      }
      return task_fail(str_lit("hidden member is not a definition"));
    case ENTRY_AXIOM:
      return task_fail(str_lit("hidden member is not a definition"));
    case ENTRY_PRIM:
      return task_fail(str_lit("hidden member is not a definition"));
  }
  return task_fail(str_lit("hidden member is not a definition"));
}

/* hidden_specialization() */
static Task hidden_specialization(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals globals;
  Vec members = one_item(heap, decl_box(heap, hidden_scope(heap, level_var(heap, nat_zero()))));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "Hidden", no_exports(heap)), &globals));
  return hidden_body(hidden_scope(heap, level_one(heap)), global_find(str_lit("Hidden_hidden"), globals));
}

/* closed_rechecking() */
static Task closed_rechecking(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals installed;
  Globals globals = with_entry(heap, "seed");
  Vec members =
      one_item(heap, decl_box(heap, definition("seedUse", term_univ(heap, level_one(heap)), term_global(heap, str_lit("seed")))));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, globals, family_empty(), &members), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, globals, catalog, "Compatible", no_exports(heap)), &installed));
  Entry changed;
  changed.kind = ENTRY_AXIOM;
  changed.def.ty = NULL;
  changed.def.body = NULL;
  changed.def.reducible = false;
  changed.def.rec_arg.some = false;
  changed.def.partial = false;
  changed.ty = term_univ(heap, level_succ(heap, level_one(heap)));
  changed.prim = (Prim){0};
  return surface_raw_common_refusal(
      heap, failure(ERROR_MISMATCH, "the term has type"),
      instantiate(heap, global_add(heap, str_lit("seed"), changed, global_empty()), catalog, "Changed", no_exports(heap)));
}

/* declaration_budget() */
static Task declaration_budget(Heap *heap) {
  uint64_t base = 0;
  uint64_t actual = 0;
  Vec none = vec_new(heap->arena);
  Vec members = one_item(heap, decl_box(heap, witness(heap)));
  RELATIONAL_TRY(measure(heap, declaration(heap, global_empty(), family_empty(), &none), &base));
  RELATIONAL_TRY(measure(heap, declaration(heap, global_empty(), family_empty(), &members), &actual));
  uint64_t allowance = base + 16;
  RELATIONAL_TRY(relational_require(actual == allowance, polls_message(heap, "member declaration polls ", actual, allowance)));
  return surface_raw_common_refusal(heap, surface_raw_common_budget_error(),
                                    budget_run_poll(heap, declaration(heap, global_empty(), family_empty(), &members),
                                                    surface_foundation_common_countdown(heap, pred(allowance))));
}

/* specialization_budget() */
static Task specialization_budget(Heap *heap) {
  FamilyCatalog empty = family_empty();
  FamilyCatalog catalog = family_empty();
  uint64_t base = 0;
  uint64_t actual = 0;
  InstanceArgs args = instance_args(heap);
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &none), &empty));
  RELATIONAL_TRY(measure(heap, instance(heap, &args, global_empty(), empty, "Empty", no_exports(heap)), &base));
  Vec members = one_item(heap, decl_box(heap, witness(heap)));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(measure(heap, instance(heap, &args, global_empty(), catalog, "Limited", no_exports(heap)), &actual));
  uint64_t allowance = base + 13;
  RELATIONAL_TRY(relational_require(actual == allowance, polls_message(heap, "member specialization polls ", actual, allowance)));
  return surface_raw_common_refusal(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, instance(heap, &args, global_empty(), catalog, "Limited", no_exports(heap)),
                      surface_foundation_common_countdown(heap, pred(allowance))));
}

/* The cases() rows: one function per Raw.Case lambda, in Bend order. */
static Task case_ordered(Heap *heap, void *ctx) {
  (void)ctx;
  return positive(heap);
}

static Task case_chosen_names(Heap *heap, void *ctx) {
  (void)ctx;
  return chosen_names(heap);
}

static Task case_exports_empty(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, failure(ERROR_MISMATCH, "every family member needs an export name"), vec_new(heap->arena));
}

static Task case_exports_missing(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, failure(ERROR_MISMATCH, "every family member needs an export name"),
                     one_item(heap, rename_box(heap, "witness", "chosenWitness")));
}

static Task case_exports_unknown(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, failure(ERROR_MISMATCH, "member exports must name each member once"),
                     three_items(heap, rename_box(heap, "witness", "chosenWitness"), rename_box(heap, "alias", "chosenAlias"),
                                 rename_box(heap, "unknown", "extra")));
}

static Task case_exports_duplicate_source(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, failure(ERROR_MISMATCH, "member exports must name each member once"),
                     two_renames(heap, "witness", "first", "witness", "second"));
}

static Task case_exports_duplicate_target(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, failure(ERROR_MISMATCH, "member exports repeat the target same"),
                     two_renames(heap, "witness", "same", "alias", "same"));
}

static Task case_exports_family_collision(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, surface_raw_common_collision(heap, str_lit("Chosen")),
                     two_renames(heap, "witness", "Chosen", "alias", "chosenAlias"));
}

static Task case_exports_ambient_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  return surface_raw_common_refusal(
      heap, surface_raw_common_collision(heap, str_lit("chosenAlias")),
      instantiate(heap, with_entry(heap, "chosenAlias"), catalog, "Chosen", some_exports(chosen(heap))));
}

static Task case_exports_template_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  return surface_raw_common_refusal(
      heap, surface_raw_common_collision(heap, str_lit("Marker")),
      instantiate(heap, global_empty(), catalog, "Chosen", some_exports(two_renames(heap, "witness", "Marker", "alias", "chosenAlias"))));
}

static Task case_exports_companion_collision(Heap *heap, void *ctx) {
  (void)ctx;
  return export_companion(heap);
}

static Task case_exports_constructor_collision(Heap *heap, void *ctx) {
  (void)ctx;
  return bad_exports(heap, surface_raw_common_collision(heap, str_lit("mark")),
                     two_renames(heap, "witness", "mark", "alias", "chosenAlias"));
}

static Task case_exports_ambient_constructor_collision(Heap *heap, void *ctx) {
  (void)ctx;
  return export_ambient_constructor(heap);
}

static Task case_exports_as_name_under_reuse(Heap *heap, void *ctx) {
  (void)ctx;
  return reuse_as_name(heap);
}

static Task case_exports_with_family_reuse(Heap *heap, void *ctx) {
  (void)ctx;
  return reuse_exports(heap);
}

static Task case_export_validation_budget(Heap *heap, void *ctx) {
  (void)ctx;
  return export_budget(heap);
}

static Task case_exports_late_budget_rollback(Heap *heap, void *ctx) {
  (void)ctx;
  return export_rollback(heap);
}

static Task case_definition_must_check(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members = one_item(heap, decl_box(heap, definition("witness", ty(heap, "Marker"), term_var(heap, nat_zero()))));
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "de Bruijn index 0 is outside the context"),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_postulate_refused(Heap *heap, void *ctx) {
  (void)ctx;
  /* Check.Decl{"witness", Check.Postulate{}, Groups.ty("Marker"), None{}} */
  Decl member = definition("witness", ty(heap, "Marker"), NULL);
  member.kind = DECL_POSTULATE;
  member.body.some = false;
  Vec members = one_item(heap, decl_box(heap, member));
  return surface_raw_common_refusal(heap, failure(ERROR_NOT_YET, "family schema members must be definitions"),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_body_required(Heap *heap, void *ctx) {
  (void)ctx;
  /* Check.Decl{"witness", Check.Definition{}, Groups.ty("Marker"), None{}} */
  Decl member = definition("witness", ty(heap, "Marker"), NULL);
  member.body.some = false;
  Vec members = one_item(heap, decl_box(heap, member));
  return surface_raw_common_refusal(heap, failure(ERROR_CANNOT_INFER, "the definition witness has no body"),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_forward_reference(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members = two_items(heap, decl_box(heap, alias(heap)), decl_box(heap, witness(heap)));
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "witness"),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_self_reference(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members =
      one_item(heap, decl_box(heap, definition("witness", ty(heap, "Marker"), term_global(heap, str_lit("witness")))));
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "witness"),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_duplicate_member(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members = two_items(heap, decl_box(heap, witness(heap)), decl_box(heap, witness(heap)));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("witness")),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_family_name_collision(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members = one_item(heap, decl_box(heap, definition("Marker", ty(heap, "Marker"), value(heap, "Marker"))));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Marker")),
                                    declare(heap, global_empty(), family_empty(), &members));
}

static Task case_existing_global_collision(Heap *heap, void *ctx) {
  (void)ctx;
  Vec members = one_item(heap, decl_box(heap, witness(heap)));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("witness")),
                                    declare(heap, with_entry(heap, "witness"), family_empty(), &members));
}

static Task case_hidden_level_scope(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  Vec scoped = one_item(heap, decl_box(heap, hidden_scope(heap, level_var(heap, nat_zero()))));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &scoped), &catalog));
  Vec escaped = one_item(heap, decl_box(heap, hidden_scope(heap, level_var(heap, heap_nat_of_u64(heap, 1)))));
  return surface_raw_common_refusal(heap, surface_raw_common_scope_error(),
                                    declare(heap, global_empty(), family_empty(), &escaped));
}

static Task case_hidden_level_specialization(Heap *heap, void *ctx) {
  (void)ctx;
  return hidden_specialization(heap);
}

static Task case_member_template_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("Other"), family_empty()), &catalog));
  Vec members = one_item(heap, decl_box(heap, definition("Other", ty(heap, "Marker"), value(heap, "Marker"))));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Other")),
                                    declare(heap, global_empty(), catalog, &members));
}

static Task case_cross_template_reference(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("Other"), family_empty()), &catalog));
  Vec members = one_item(heap, decl_box(heap, definition("other", ty(heap, "Other"), value(heap, "Other"))));
  return surface_raw_common_refusal(heap, failure(ERROR_NOT_YET, "references between family schemas are not supported"),
                                    declare(heap, global_empty(), catalog, &members));
}

static Task case_target_global_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  Vec members = witness_alias(heap);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  Globals globals = with_entry(heap, "First_alias");
  RELATIONAL_TRY(surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("First_alias")),
                                            instantiate(heap, globals, catalog, "First", no_exports(heap))));
  return relational_require(surface_raw_common_absent(global_find_family(str_lit("First"), globals).some) &&
                                surface_raw_common_absent(global_find(str_lit("First_witness"), globals).some),
                            str_lit("failed specialization modified input globals"));
}

static Task case_target_family_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog catalog = family_empty();
  Globals globals;
  Vec members = one_item(heap, decl_box(heap, witness(heap)));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &catalog));
  RELATIONAL_TRY(kernel_globals(heap,
                                check_declare_family_at(heap, global_empty(), budget_unlimited(), nat_zero(),
                                                        surface_family_groups_marker(heap, str_lit("First_witness"))),
                                &globals));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("First_witness")),
                                    instantiate(heap, globals, catalog, "First", no_exports(heap)));
}

static Task case_target_template_collision(Heap *heap, void *ctx) {
  (void)ctx;
  FamilyCatalog declared = family_empty();
  FamilyCatalog catalog = family_empty();
  Vec members = one_item(heap, decl_box(heap, witness(heap)));
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &members), &declared));
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("First_witness"), declared), &catalog));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("First_witness")),
                                    instantiate(heap, global_empty(), catalog, "First", no_exports(heap)));
}

static Task case_closed_rechecking(Heap *heap, void *ctx) {
  (void)ctx;
  return closed_rechecking(heap);
}

static Task case_declaration_member_budget(Heap *heap, void *ctx) {
  (void)ctx;
  return declaration_budget(heap);
}

static Task case_specialization_member_budget(Heap *heap, void *ctx) {
  (void)ctx;
  return specialization_budget(heap);
}

typedef struct {
  const char *name;
  Task (*test)(Heap *heap, void *ctx);
} CaseRow;

/* cases() */
static const CaseRow case_rows[] = {
    {"ordered-members-and-renaming", case_ordered},
    {"chosen-export-names-and-computation", case_chosen_names},
    {"exports-empty-mapping", case_exports_empty},
    {"exports-missing-member", case_exports_missing},
    {"exports-unknown-member", case_exports_unknown},
    {"exports-duplicate-source", case_exports_duplicate_source},
    {"exports-duplicate-target", case_exports_duplicate_target},
    {"exports-family-collision", case_exports_family_collision},
    {"exports-ambient-collision", case_exports_ambient_collision},
    {"exports-template-collision", case_exports_template_collision},
    {"exports-companion-collision", case_exports_companion_collision},
    {"exports-constructor-collision", case_exports_constructor_collision},
    {"exports-ambient-constructor-collision", case_exports_ambient_constructor_collision},
    {"exports-as-name-under-reuse", case_exports_as_name_under_reuse},
    {"exports-with-family-reuse", case_exports_with_family_reuse},
    {"export-validation-budget", case_export_validation_budget},
    {"exports-late-budget-rollback", case_exports_late_budget_rollback},
    {"definition-must-check", case_definition_must_check},
    {"postulate-refused", case_postulate_refused},
    {"body-required", case_body_required},
    {"forward-reference", case_forward_reference},
    {"self-reference", case_self_reference},
    {"duplicate-member", case_duplicate_member},
    {"family-name-collision", case_family_name_collision},
    {"existing-global-collision", case_existing_global_collision},
    {"hidden-level-scope", case_hidden_level_scope},
    {"hidden-level-specialization", case_hidden_level_specialization},
    {"member-template-collision", case_member_template_collision},
    {"cross-template-reference", case_cross_template_reference},
    {"target-global-collision", case_target_global_collision},
    {"target-family-collision", case_target_family_collision},
    {"target-template-collision", case_target_template_collision},
    {"closed-rechecking", case_closed_rechecking},
    {"declaration-member-budget", case_declaration_member_budget},
    {"specialization-member-budget", case_specialization_member_budget},
};

/* Raw.run(rows, 0n) over len rows starting at first. */
static Task run_rows(Heap *heap, size_t first, size_t len, Str *out) {
  RawCase *cases = heap_alloc(heap, (len == 0 ? 1 : len) * sizeof *cases);
  if (cases == NULL) return host_oom(heap);
  for (size_t i = 0; i < len; i++) {
    cases[i].name = str_lit(case_rows[first + i].name);
    cases[i].test = case_rows[first + i].test;
    cases[i].ctx = NULL;
  }
  return surface_raw_common_run(heap, cases, len, 0, out);
}

/* selected(cases(), name): the first row whose key is name runs alone; no such row fails. */
static Task selected(Heap *heap, Str name, Str *out) {
  size_t len = sizeof case_rows / sizeof case_rows[0];
  for (size_t i = 0; i < len; i++) {
    if (str_eq(str_lit(case_rows[i].name), name)) return run_rows(heap, i, 1, out);
  }
  return task_fail(str_cat(heap->arena, str_lit("unknown case: "), name));
}

/* arguments(args): no argument runs every case; the first argument selects one case. */
static Task arguments(Heap *heap, int argc, char **argv, Str *out) {
  if (argc > 1) return selected(heap, str_lit(argv[1]), out);
  return run_rows(heap, 0, sizeof case_rows / sizeof case_rows[0], out);
}

/* main() = R.finish("FAMILY-MEMBERS", ...) over IO.args(). */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = arguments(&heap, argc, argv, &fields);
  int code = relational_finish(&heap, str_lit("FAMILY-MEMBERS"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
