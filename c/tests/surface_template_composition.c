/* Port of tests/surface_template_composition.bend (150 lines, oracle label surface-template-composition):
 * a composition template elaborates to closed families and members in order, the composition refusals and
 * parsers, parity with explicit closed declarations, current globals at compose and specialize time, the
 * mixed-universe category prefix, and the raw compose refusals and cancellations. argv[1] (optional) is the
 * repository root (R.root). Prints TEMPLATE-COMPOSITION-OK negatives=24 parser=9 raw=<n>
 * type-excluded=negative-arity or TEMPLATE-COMPOSITION-FAIL <message>. */
#include <string.h>
#include "driver.h"
#include "surface_family_groups.h"
#include "surface_foundation_common.h"
#include "surface_prenex_common.h"
#include "surface_prenex_exports.h"
#include "surface_raw_common.h"
#include "surface_template_common.h"
#include "surface_template_data.h"

static Task host_oom(Heap *heap) {
  heap->oom = true;
  return task_fail(str_lit("out of memory"));
}

/* A Bend List<String> literal as a Vec of Str pointers. */
static Vec strings(Heap *heap, const char *const *items, size_t len) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < len; i++) {
    Str *boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL || !vec_push(&out, boxed)) {
      heap->oom = true;
      return out;
    }
    *boxed = str_lit(items[i]);
  }
  return out;
}

/* A one-item Bend list literal; a NULL item or a failed push sets heap->oom. */
static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (item == NULL) heap->oom = true;
  else if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

/* Appends one item; a NULL item or a failed push sets heap->oom. */
static void push_item(Heap *heap, Vec *items, void *item) {
  if (item == NULL) heap->oom = true;
  else if (!vec_push(items, item)) heap->oom = true;
}

/* A list that outlives the Comp built from it: the compose frame reads it when the budget runs. */
static const Vec *vec_box(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) {
    heap->oom = true;
    return NULL;
  }
  *boxed = items;
  return boxed;
}

/* C.Pair{name, expected}: one computations() row. */
static void push_row(Heap *heap, Vec *out, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL || expected == NULL) {
    heap->oom = true;
    return;
  }
  row->name = str_lit(name);
  row->expected = expected;
  if (!vec_push(out, row)) heap->oom = true;
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
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

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) heap->oom = true;
  else *box = decl;
  return box;
}

static FamilyMemberElaborator *elaborator_box(Heap *heap, Comp (*fn)(Heap *, void *, Globals), void *ctx) {
  FamilyMemberElaborator *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) {
    heap->oom = true;
    return NULL;
  }
  box->fn = fn;
  box->ctx = ctx;
  return box;
}

/* globals => B.pure(Check.Decl, member): ctx is the boxed member. */
static Comp pure_member(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)globals;
  return budget_pure(ctx);
}

/* globals => B.failed(Check.Decl, E.Unbound{"callback"}) */
static Comp callback_member(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(error_make(ERROR_UNBOUND, str_lit("callback")));
}

/* Check.Decl{name, Check.Postulate{}, ty, None{}} */
static Decl postulate(Str name, const Term *ty) {
  Decl decl;
  decl.name = name;
  decl.kind = DECL_POSTULATE;
  decl.ty = ty;
  decl.body.some = false;
  decl.body.value = NULL;
  return decl;
}

/* Groups.hidden_member() (a copy of the static def in surface_family_groups.c) */
static Decl hidden_member(Heap *heap) {
  const Term *domain = term_univ(heap, level_one(heap));
  Shape shape = shape_spi(QUANTITY_MANY, str_lit("A"), (void *)domain);
  Vec binders = one_item(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("A")));
  Vec legs = one_item(heap, term_leg_new(heap, binders, term_var(heap, nat_zero())));
  const Term *identity = term_ann(heap, term_sec(heap, shape, legs), term_ran(heap, shape, domain));
  Shape ignored = shape_spi(QUANTITY_MANY, str_lit("ignored"), (void *)term_univ(heap, level_var(heap, heap_nat_of_u64(heap, 1))));
  return surface_family_groups_definition(
      str_lit("hidden"), domain, term_out(heap, ignored, term_addr_pt(QUANTITY_MANY, term_univ(heap, level_zero())), identity));
}

/* universe(family, expected, name) */
static Task universe(Heap *heap, Family family, const LevelVar *expected, Str name) {
  return relational_require(level_equal(heap, family.level, expected),
                            str_cat(heap->arena, str_lit("wrong universe: "), name));
}

/* C.Pair{name, expected}: one universes() row. */
typedef struct {
  const char *name;
  const LevelVar *expected;
} UniverseRow;

/* universes(rows, globals) */
static Task universes(Heap *heap, const UniverseRow *rows, size_t len, Globals globals) {
  for (size_t i = 0; i < len; i++) {
    Family family;
    RELATIONAL_TRY(surface_template_common_family(str_lit(rows[i].name), globals, &family));
    RELATIONAL_TRY(universe(heap, family, rows[i].expected, str_lit(rows[i].name)));
  }
  return task_pure();
}

/* positive(source) */
static Task positive(Heap *heap, Str source) {
  const ElabChecked *symbolic = NULL;
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, source, str_lit("composition parse/print changed")));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &symbolic));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(symbolic), str_lit("composition leaked symbolic globals")));
  RELATIONAL_TRY(relational_checked(
      heap, global_empty(), str_cat(heap->arena, source, str_lit(surface_template_data_template_composition_client)),
      &program));
  Globals globals = relational_globals(program);
  static const char *const families[] = {"I_Item", "M_Item", "N_Inner_Left", "N_Inner_Right", "P_Left", "P_Right", "Tiny"};
  Vec family_names = surface_prenex_common_family_names(heap, globals);
  Vec expected_families = strings(heap, families, 7);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, &expected_families),
                                    str_lit("composition family inventory changed")));
  static const char *const entries[] = {"P_Left_unbox", "P_Right_unbox", "P_transfer", "N_Inner_Left_unbox",
                                        "N_Inner_Right_unbox", "N_Inner_transfer", "N_project", "M_Item_unbox",
                                        "I_Item_unbox", "low", "high", "result", "nested"};
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  Vec expected_entries = strings(heap, entries, 13);
  RELATIONAL_TRY(relational_require(relational_strings(&entry_names, &expected_entries) &&
                                        relational_entry_rows_length(surface_template_common_entries(globals)) == 13,
                                    str_lit("composition definition order changed")));
  RELATIONAL_TRY(relational_require(surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("composition introduced axioms")));
  RELATIONAL_TRY(relational_audit_entries(heap, surface_template_common_entries(globals), global_empty()));
  const LevelVar *one = level_one(heap);
  const UniverseRow rows[] = {{"P_Left", one},
                              {"P_Right", level_succ(heap, one)},
                              {"N_Inner_Right", level_succ(heap, one)},
                              {"M_Item", level_succ(heap, level_succ(heap, one))},
                              {"I_Item", one}};
  RELATIONAL_TRY(universes(heap, rows, sizeof rows / sizeof rows[0], globals));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "result", relational_tiny_type(heap));
  push_row(heap, &computations, "nested", relational_tiny_type(heap));
  return relational_computations(heap, &computations, globals);
}

/* Family.Dependency{source, [level], "Local", [], None{}} */
static FamilyDependency *dependency_box(Heap *heap, const char *source, const LevelVar *level) {
  FamilyDependency *dependency = heap_alloc(heap, sizeof *dependency);
  if (dependency == NULL) {
    heap->oom = true;
    return NULL;
  }
  dependency->source = str_lit(source);
  dependency->levels = one_item(heap, (void *)level);
  dependency->alias = str_lit("Local");
  dependency->reuse = vec_new(heap->arena);
  dependency->exports = no_exports(heap);
  return dependency;
}

/* dependencies() */
static const Vec *dependencies(Heap *heap) {
  return vec_box(heap, one_item(heap, dependency_box(heap, "Seed", level_zero())));
}

/* compose(globals, catalog, dependencies, members) */
static Comp compose(Heap *heap, Globals globals, FamilyCatalog catalog, const Vec *dependencies, const Vec *members) {
  return family_compose_comp(heap, globals, catalog, heap_nat_of_u64(heap, 1), str_lit("Composed"), dependencies,
                             members);
}

/* composed(catalog, members) */
static Result composed(Heap *heap, FamilyCatalog catalog, const Vec *members) {
  return budget_run(heap, compose(heap, global_empty(), catalog, dependencies(heap), members), budget_unlimited());
}

/* forbidden(globals) */
static Comp forbidden(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(error_make(ERROR_UNBOUND, str_lit("later")));
}

/* [] */
static const Vec *no_members(Heap *heap) { return vec_box(heap, vec_new(heap->arena)); }

/* [globals => B.pure(Check.Decl, decl)] ++ ([forbidden] when sentinel) */
static const Vec *pure_members(Heap *heap, Decl decl, bool sentinel) {
  Vec members = one_item(heap, elaborator_box(heap, pure_member, decl_box(heap, decl)));
  if (sentinel) push_item(heap, &members, elaborator_box(heap, forbidden, NULL));
  return vec_box(heap, members);
}

/* cancelled_dependencies(catalog) */
static Task cancelled_dependencies(Heap *heap, FamilyCatalog catalog) {
  const Vec *absent = vec_box(heap, one_item(heap, dependency_box(heap, "Absent", level_zero())));
  const Vec *members = vec_box(heap, one_item(heap, elaborator_box(heap, forbidden, NULL)));
  return surface_raw_common_exact(heap, surface_raw_common_budget_error(),
                                  budget_run_poll(heap, compose(heap, global_empty(), catalog, absent, members),
                                                  surface_foundation_common_countdown(heap, 1)));
}

/* cancelled_members(catalog) */
static Task cancelled_members(Heap *heap, FamilyCatalog catalog) {
  uint64_t count = 0;
  RELATIONAL_TRY(surface_raw_common_measure(
      heap,
      budget_run_state(heap, compose(heap, global_empty(), catalog, dependencies(heap), no_members(heap)),
                       budget_of_polls(heap_nat_of_u64(heap, 100000))),
      &count));
  Decl first = surface_family_groups_definition(str_lit("first"), term_univ(heap, level_one(heap)),
                                                term_univ(heap, level_zero()));
  return surface_raw_common_exact(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, compose(heap, global_empty(), catalog, dependencies(heap), pure_members(heap, first, true)),
                      surface_foundation_common_countdown(heap, 1 + count)));
}

/* The value the cases() closures capture. */
typedef struct {
  FamilyCatalog catalog;
} RawContext;

static Task case_empty_dependencies(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return surface_raw_common_exact(
      heap, error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")),
      budget_run(heap, compose(heap, global_empty(), raw->catalog, no_members(heap), no_members(heap)),
                 budget_unlimited()));
}

static Task case_raw_scope(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  const Vec *scoped =
      vec_box(heap, one_item(heap, dependency_box(heap, "Seed", level_var(heap, heap_nat_of_u64(heap, 1)))));
  return surface_raw_common_exact(
      heap, surface_raw_common_scope_error(),
      budget_run(heap, compose(heap, global_empty(), raw->catalog, scoped, no_members(heap)), budget_unlimited()));
}

static Task case_raw_member_body(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  Decl bad = surface_family_groups_definition(str_lit("bad"), term_univ(heap, level_one(heap)), term_var(heap, nat_zero()));
  return surface_raw_common_exact(heap, error_make(ERROR_UNBOUND, str_lit("de Bruijn index 0 is outside the context")),
                                  composed(heap, raw->catalog, pure_members(heap, bad, true)));
}

static Task case_raw_group_name(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  Decl named = surface_family_groups_definition(str_lit("Composed"), term_univ(heap, level_one(heap)),
                                                term_univ(heap, level_zero()));
  return surface_raw_common_exact(heap, surface_raw_common_collision(heap, str_lit("Composed")),
                                  composed(heap, raw->catalog, pure_members(heap, named, false)));
}

static Task case_raw_postulate(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  Decl postulated = postulate(str_lit("postulated"), term_univ(heap, level_one(heap)));
  return surface_raw_common_exact(heap, error_make(ERROR_NOT_YET, str_lit("family schema members must be definitions")),
                                  composed(heap, raw->catalog, pure_members(heap, postulated, false)));
}

static Task case_raw_hidden_scope(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return surface_raw_common_exact(heap, surface_raw_common_scope_error(),
                                  composed(heap, raw->catalog, pure_members(heap, hidden_member(heap), false)));
}

static Task case_callback_error(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  Vec members = one_item(heap, elaborator_box(heap, callback_member, NULL));
  push_item(heap, &members, elaborator_box(heap, forbidden, NULL));
  return surface_raw_common_exact(heap, error_make(ERROR_UNBOUND, str_lit("callback")),
                                  composed(heap, raw->catalog, vec_box(heap, members)));
}

static Task case_cancelled_dependencies(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return cancelled_dependencies(heap, raw->catalog);
}

static Task case_cancelled_members(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return cancelled_members(heap, raw->catalog);
}

/* Check.FamilyDecl{"Seed", [], [], L.one()} */
static FamilyDecl seed_family(Heap *heap) {
  FamilyDecl seed;
  seed.name = str_lit("Seed");
  seed.params = vec_new(heap->arena);
  seed.indices = vec_new(heap->arena);
  seed.level = level_one(heap);
  return seed;
}

/* Check.CtorDecl{"mark", args, [], []} */
static CtorDecl *mark_ctor(Heap *heap, Vec args) {
  CtorDecl *ctor = heap_alloc(heap, sizeof *ctor);
  if (ctor == NULL) {
    heap->oom = true;
    return NULL;
  }
  ctor->name = str_lit("mark");
  ctor->args = args;
  ctor->res_params = vec_new(heap->arena);
  ctor->res_idx = vec_new(heap->arena);
  return ctor;
}

/* raw(): Done count goes to *count. */
static Task raw(Heap *heap, uint64_t *count) {
  RawContext *ctx = heap_alloc(heap, sizeof *ctx);
  if (ctx == NULL) return host_oom(heap);
  Vec ctors = one_item(heap, mark_ctor(heap, vec_new(heap->arena)));
  Vec members = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap,
                                family_declare(heap, global_empty(), family_empty(), budget_unlimited(),
                                               heap_nat_of_u64(heap, 1), seed_family(heap), ctors, &members),
                                &ctx->catalog));
  RawCase cases[] = {
      {str_lit("empty-dependencies"), case_empty_dependencies, ctx},
      {str_lit("raw-scope"), case_raw_scope, ctx},
      {str_lit("raw-member-body"), case_raw_member_body, ctx},
      {str_lit("raw-group-name"), case_raw_group_name, ctx},
      {str_lit("raw-postulate"), case_raw_postulate, ctx},
      {str_lit("raw-hidden-scope"), case_raw_hidden_scope, ctx},
      {str_lit("callback-error"), case_callback_error, ctx},
      {str_lit("cancelled-dependencies"), case_cancelled_dependencies, ctx},
      {str_lit("cancelled-members"), case_cancelled_members, ctx},
  };
  return surface_template_common_run(heap, cases, sizeof cases / sizeof cases[0], 0, count);
}

/* current_globals() */
static Task current_globals(Heap *heap) {
  FamilyCatalog catalog = NULL;
  FamilyCatalog shared = NULL;
  Globals restored;
  Globals globals =
      global_add(heap, str_lit("External"), global_entry_axiom(term_univ(heap, level_one(heap))), global_empty());
  Vec args = one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("item"),
                                                  term_global(heap, str_lit("External"))));
  Vec ctors = one_item(heap, mark_ctor(heap, args));
  Vec members = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap,
                                family_declare(heap, globals, family_empty(), budget_unlimited(),
                                               heap_nat_of_u64(heap, 1), seed_family(heap), ctors, &members),
                                &catalog));
  RELATIONAL_TRY(surface_raw_common_exact(heap, error_make(ERROR_UNBOUND, str_lit("External")),
                                          composed(heap, catalog, no_members(heap))));
  RELATIONAL_TRY(kernel_catalog(
      heap, budget_run(heap, compose(heap, globals, catalog, dependencies(heap), no_members(heap)), budget_unlimited()),
      &shared));
  Vec levels = one_item(heap, (void *)level_zero());
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(surface_raw_common_exact(heap, error_make(ERROR_UNBOUND, str_lit("External")),
                                          family_instantiate(heap, global_empty(), shared, budget_unlimited(),
                                                             str_lit("Composed"), &levels, str_lit("Bad"), &reuse,
                                                             no_exports(heap))));
  RELATIONAL_TRY(kernel_globals(heap,
                                family_instantiate(heap, globals, shared, budget_unlimited(), str_lit("Composed"),
                                                   &levels, str_lit("Good"), &reuse, no_exports(heap)),
                                &restored));
  return relational_require(surface_raw_common_present(global_find_family(str_lit("Good_Local"), restored).some),
                            str_lit("failed specialization prevented a compatible instance"));
}

/* Raw.entries(rows, globals) over Elab.Rows: rows is a Vec of CheckRow pointers. */
static bool row_entries(const Vec *rows, Globals globals) {
  for (size_t i = 0; i < rows->len; i++) {
    VecGet got = vec_get(rows, i);
    if (got.status != VEC_GET_OK) return false;
    const CheckRow *row = got.value;
    if (!surface_raw_common_matched_entry(row->entry, global_find(row->name, globals))) return false;
  }
  return true;
}

/* direct_parity() */
static Task direct_parity(Heap *heap) {
  const ElabChecked *actual = NULL;
  const ElabChecked *expected = NULL;
  Str box = str_lit(surface_template_data_template_composition_box);
  Str grouped = str_cat(heap->arena, box, str_lit("poly (u) group Group where "));
  grouped = str_cat(heap->arena, grouped, str_lit(surface_template_data_template_composition_dependency));
  grouped = str_cat(heap->arena, grouped, str_lit("def member : Local := box end specialize Group (0) as G"));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), grouped, &actual));
  RELATIONAL_TRY(relational_checked(
      heap, global_empty(), str_cat(heap->arena, box, str_lit("specialize Box (0) as G_Local def G_member : G_Local := box")),
      &expected));
  Vec actual_names = relational_entry_names(heap, relational_rows(actual));
  Vec expected_names = relational_entry_names(heap, relational_rows(expected));
  return relational_require(surface_raw_common_same_globals(relational_globals(actual), relational_globals(expected)) &&
                                relational_strings(&actual_names, &expected_names) &&
                                row_entries(relational_rows(actual), relational_globals(expected)),
                            str_lit("composition differs from explicit closed declarations"));
}

/* prefix(members, []): the members up to and including the first one named assoc. */
static Task prefix(Heap *heap, const Vec *members, Vec *out) {
  Vec kept = vec_new(heap->arena);
  for (size_t i = 0; i < members->len; i++) {
    VecGet got = vec_get(members, i);
    if (got.status != VEC_GET_OK) break;
    const SRecDef *member = got.value;
    push_item(heap, &kept, got.value);
    if (str_eq(member->name, str_lit("assoc"))) {
      *out = kept;
      return task_pure();
    }
  }
  return task_fail(str_lit("category prefix has no assoc member"));
}

/* category_prefix(parsed) */
static Task category_prefix(Heap *heap, const Vec *parsed, Vec *out) {
  VecGet got = vec_get(parsed, 0);
  const SDecl *group = got.status == VEC_GET_OK ? got.value : NULL;
  if (parsed->len != 1 || group == NULL || group->kind != SDECL_POLY_GROUP)
    return task_fail(str_lit("category prelude is not one family group"));
  Vec members = vec_new(heap->arena);
  RELATIONAL_TRY(prefix(heap, &group->helpers, &members));
  *out = one_item(heap, (void *)sdecl_poly_group_new(heap, group->universes, group->family, group->families, members));
  return task_pure();
}

/* category(root) */
static Task category(Heap *heap, Str root) {
  Str source = str_empty();
  Str fixture = str_empty();
  void *parsed_value = NULL;
  void *client_value = NULL;
  void *program_value = NULL;
  Vec parsed = vec_new(heap->arena);
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/category.mech"), &source));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/composition-category.mech"), &fixture));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, source), &parsed_value));
  if (parsed_value == NULL) return host_oom(heap);
  RELATIONAL_TRY(category_prefix(heap, parsed_value, &parsed));
  RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, fixture, str_lit("category composition round trip changed")));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, fixture), &client_value));
  if (client_value == NULL) return host_oom(heap);
  Vec decls = vec_concat(heap->arena, &parsed, client_value);
  /* Bend: Elab.elab_program_in(G.empty(), B.unlimited(), parsed ++ client). The host runs the compact form
   * (elab_program_in with budget_unlimited and a per-declaration arena reset, the same Result): the plain
   * form keeps the whole 40-entry elaboration's garbage in one arena and grows past the 3800 MB cap. */
  RELATIONAL_TRY(relational_kernel(heap, elab_elab_program_in_compact(heap, global_empty(), &decls), &program_value));
  if (program_value == NULL) return host_oom(heap);
  const ElabChecked *program = program_value;
  static const char *const families[] = {"First_Source", "First_Target", "Second_Source", "Second_Target"};
  Vec family_names = surface_prenex_common_family_names(heap, relational_globals(program));
  Vec expected_families = strings(heap, families, 4);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, &expected_families),
                                    str_lit("mixed-universe category family inventory changed")));
  return relational_require(
      relational_rows(program)->len == 40 &&
          relational_entry_rows_length(surface_template_common_entries(relational_globals(program))) == 40 &&
          surface_template_common_no_axioms(heap, relational_rows(program)),
      str_lit("mixed-universe category entry inventory changed"));
}

/* suite(root) */
static Task suite(Heap *heap, Str root, Str *fields) {
  uint64_t count = 0;
  Str source = str_empty();
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/template-composition.mech"), &source));
  RELATIONAL_TRY(positive(heap, source));
  RELATIONAL_TRY(surface_template_common_negatives(heap, surface_template_data_template_composition_negatives,
                                                   surface_template_data_template_composition_negatives_count,
                                                   global_empty()));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_template_data_template_composition_parsers,
                                               surface_template_data_template_composition_parsers_count, false));
  RELATIONAL_TRY(direct_parity(heap));
  RELATIONAL_TRY(current_globals(heap));
  RELATIONAL_TRY(category(heap, root));
  RELATIONAL_TRY(raw(heap, &count));
  Str head = str_cat(heap->arena, str_lit("negatives=24 parser=9 raw="), str_of_u64(heap->arena, count));
  *fields = str_cat(heap->arena, head, str_lit(" type-excluded=negative-arity"));
  return task_pure();
}

/* arguments(args) = R.finish("TEMPLATE-COMPOSITION", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = suite(&heap, relational_root(argc, argv), &fields);
  int code = relational_finish(&heap, str_lit("TEMPLATE-COMPOSITION"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
