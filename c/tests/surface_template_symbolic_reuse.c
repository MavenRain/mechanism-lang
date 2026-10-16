/* Port of bend2/tests/surface_template_symbolic_reuse.bend (124 lines, oracle label surface-template-symbolic-reuse):
 * symbolic templates reuse families by identity (nested, dependent groups), a composed family refuses a weakened
 * certificate before it runs a member, cancellation lands before the certificate poll, the reuse refusals and
 * parsers, and the shared category chain contracts. argv[1] (optional) is the repository root (R.root). Prints
 * TEMPLATE-SYMBOLIC-REUSE-OK negatives=12 parser=6 raw=<n> or TEMPLATE-SYMBOLIC-REUSE-FAIL <message>. */
#include <string.h>
#include "driver.h"
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

/* expected_rows(): expected_members("N") ++ expected_members("Higher") ++ expected_members("Reused") ++ the defs. */
static const char *const expected_rows[] = {
    "N_Inner_Base_unbox",           "N_Inner_Both_Left_unbox",      "N_Inner_Both_Right_unbox",
    "N_Inner_Again_unbox",          "N_Other_unbox",                "Higher_Inner_Base_unbox",
    "Higher_Inner_Both_Left_unbox", "Higher_Inner_Both_Right_unbox", "Higher_Inner_Again_unbox",
    "Higher_Other_unbox",           "Reused_Inner_Base_unbox",      "Reused_Inner_Both_Left_unbox",
    "Reused_Inner_Both_Right_unbox", "Reused_Inner_Again_unbox",    "Reused_Other_unbox",
    "payload",                      "value",                        "again",
    "reused",                       "right"};

/* positive() */
static Task positive(Heap *heap) {
  Str templates = str_lit(surface_template_data_template_symbolic_reuse_templates);
  const ElabChecked *symbolic = NULL;
  const ElabChecked *original = NULL;
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, templates, str_lit("symbolic reuse round trip changed")));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), templates, &symbolic));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(symbolic), str_lit("symbolic reuse leaked globals")));
  Str instances =
      str_cat(heap->arena, templates, str_lit("specialize Nested (0, 0) as N specialize Nested (1, 2) as Higher "));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), instances, &original));
  RELATIONAL_TRY(relational_checked(
      heap, global_initial(heap),
      str_cat(heap->arena, instances,
              str_lit("specialize Nested (0, 0) as Reused with (Inner_Base := N_Inner_Base, Inner_Both_Right := "
                      "N_Inner_Both_Right) def payload : N_Inner_Base Nat := box 37 def value : Nat := "
                      "N_Inner_Both_Left_unbox Nat payload def again : Nat := N_Inner_Again_unbox Nat payload def "
                      "reused : Nat := Reused_Inner_Again_unbox Nat payload def right : Nat := N_Other_unbox Nat (box "
                      "41 : N_Inner_Both_Right Nat)")),
      &program));
  Globals globals = relational_globals(program);
  static const char *const families[] = {"Higher_Inner_Base", "Higher_Inner_Both_Right", "N_Inner_Base",
                                         "N_Inner_Both_Right"};
  Vec new_names = surface_template_common_new_family_names(heap, globals, global_initial(heap));
  Vec expected_families = strings(heap, families, 4);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &new_names, &expected_families) &&
                                        surface_template_common_same_families(globals, relational_globals(original)),
                                    str_lit("wrong symbolic reuse family inventory")));
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  Vec expected_entries = strings(heap, expected_rows, sizeof expected_rows / sizeof expected_rows[0]);
  RELATIONAL_TRY(relational_require(relational_strings(&entry_names, &expected_entries) &&
                                        surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("wrong symbolic reuse member inventory")));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "value", relational_natural(heap, 37));
  push_row(heap, &computations, "again", relational_natural(heap, 37));
  push_row(heap, &computations, "reused", relational_natural(heap, 37));
  push_row(heap, &computations, "right", relational_natural(heap, 41));
  return relational_computations(heap, &computations, globals);
}

/* dependent() */
static Task dependent(Heap *heap) {
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_checked(
      heap, global_empty(),
      str_cat(heap->arena, str_lit(surface_template_data_template_symbolic_reuse_marker),
              str_lit("poly (u) group Dependent where specialize Marker (u) as Original specialize Marker (u) as "
                      "Partial with (Marker := Original) specialize Marker (u) as Shared with (Marker := Original, "
                      "Wrapper := Original_Wrapper) def witness : Original_Wrapper mark := Shared_value end "
                      "specialize Dependent (1) as D")),
      &program));
  static const char *const families[] = {"D_Original", "D_Original_Wrapper", "D_Partial_Wrapper"};
  static const char *const entries[] = {"D_Original_value", "D_Partial_value", "D_Shared_value", "D_witness"};
  Vec family_names = surface_prenex_common_family_names(heap, relational_globals(program));
  Vec expected_families = strings(heap, families, 3);
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  Vec expected_entries = strings(heap, entries, 4);
  return relational_require(relational_same_names(heap, &family_names, &expected_families) &&
                                relational_strings(&entry_names, &expected_entries) &&
                                surface_template_common_no_axioms(heap, relational_rows(program)),
                            str_lit("dependent symbolic reuse changed identities"));
}

/* Family.Dependency{"Seed", [L.zero()], alias, reuse, None{}} */
static FamilyDependency *dependency_box(Heap *heap, const char *alias, Vec reuse) {
  FamilyDependency *dependency = heap_alloc(heap, sizeof *dependency);
  if (dependency == NULL) {
    heap->oom = true;
    return NULL;
  }
  dependency->source = str_lit("Seed");
  dependency->levels = one_item(heap, (void *)level_zero());
  dependency->alias = str_lit(alias);
  dependency->reuse = reuse;
  dependency->exports = no_exports(heap);
  return dependency;
}

/* compose(globals, catalog, members) */
static Comp compose(Heap *heap, Globals globals, FamilyCatalog catalog, const Vec *members) {
  Vec dependencies = vec_new(heap->arena);
  FamilyDependency *fresh = dependency_box(heap, "Fresh", vec_new(heap->arena));
  FamilyDependency *copy = dependency_box(heap, "Copy", one_item(heap, rename_box(heap, "Seed", "Existing")));
  if (fresh == NULL || copy == NULL || !vec_push(&dependencies, fresh) || !vec_push(&dependencies, copy))
    heap->oom = true;
  return family_compose_comp(heap, globals, catalog, heap_nat_of_u64(heap, 1), str_lit("Shared"),
                             vec_box(heap, dependencies), members);
}

/* forbidden(globals) */
static Comp forbidden(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(error_make(ERROR_UNBOUND, str_lit("callback")));
}

/* [g => forbidden(g)] */
static const Vec *forbidden_members(Heap *heap) {
  FamilyMemberElaborator *member = heap_alloc(heap, sizeof *member);
  if (member == NULL) {
    heap->oom = true;
    return vec_box(heap, vec_new(heap->arena));
  }
  member->fn = forbidden;
  member->ctx = NULL;
  return vec_box(heap, one_item(heap, member));
}

/* [] */
static const Vec *no_members(Heap *heap) { return vec_box(heap, vec_new(heap->arena)); }

/* certificate_failure(result) */
static bool certificate_failure(Result result) {
  if (result.kind != RESULT_FAIL) return false;
  if (result.error.kind != ERROR_MISMATCH) return false;
  return str_eq(result.error.message, str_lit("the reused family Existing does not match the template"));
}

/* frontier(globals, catalog, allowance): the first allowance whose poll run hits the certificate refusal. */
static Task frontier(Heap *heap, Globals globals, FamilyCatalog catalog, uint64_t allowance, uint64_t *out) {
  for (uint64_t polls = allowance;; polls++) {
    Result result = budget_run_poll(heap, compose(heap, globals, catalog, forbidden_members(heap)),
                                    surface_foundation_common_countdown(heap, polls));
    if (heap->oom) return host_oom(heap);
    if (certificate_failure(result)) {
      *out = polls;
      return task_pure();
    }
    if (polls >= 512) return task_fail(str_lit("no certificate refusal below 512 polls"));
  }
}

/* cancelled(globals, catalog, original) */
static Task cancelled(Heap *heap, Globals globals, FamilyCatalog catalog, Family original) {
  uint64_t poll = 0;
  Globals corrupted =
      global_add_family(heap, str_lit("Existing"), surface_template_common_altered(original, 1), globals);
  RELATIONAL_TRY(frontier(heap, corrupted, catalog, 1, &poll));
  RELATIONAL_TRY(surface_raw_common_exact(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, compose(heap, corrupted, catalog, forbidden_members(heap)),
                      surface_foundation_common_countdown(heap, poll - 1))));
  return relational_require(3 < poll, str_lit("cancellation preempted no certificate poll or ran a member"));
}

/* empty_companions(value): Some{Nil{}} */
static bool empty_companions(FamilyMaybeVec value) { return value.some && value.value.len == 0; }

/* preserve(globals, catalog, original) */
static Task preserve(Heap *heap, Globals globals, FamilyCatalog catalog, Family original) {
  FamilyCatalog shared = NULL;
  Globals installed;
  RELATIONAL_TRY(kernel_catalog(
      heap, budget_run(heap, compose(heap, globals, catalog, no_members(heap)), budget_unlimited()), &shared));
  Vec levels = one_item(heap, (void *)level_zero());
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_globals(heap,
                                family_instantiate(heap, globals, shared, budget_unlimited(), str_lit("Shared"),
                                                   &levels, str_lit("Instance"), &reuse, no_exports(heap)),
                                &installed));
  return relational_require(
      surface_raw_common_matched_family(original, global_find_family(str_lit("Existing"), installed)) &&
          empty_companions(family_companions(heap, shared, str_lit("Shared"))),
      str_lit("caller certificate changed"));
}

/* arity_one(value): Some{1} */
static bool arity_one(MaybeNat value) {
  if (!value.some) return false;
  NatToU64 small = nat_to_u64(value.value);
  return small.status == NAT_TO_U64_OK && small.value == 1;
}

/* before_members(globals, catalog, original) */
static Task before_members(Heap *heap, Globals globals, FamilyCatalog catalog, Family original) {
  FamilyCatalog shared = NULL;
  Globals altered =
      global_add_family(heap, str_lit("Existing"), surface_template_common_altered(original, 0), globals);
  RELATIONAL_TRY(surface_raw_common_exact(
      heap, surface_template_common_mismatch(heap, str_lit("Existing")),
      budget_run(heap, compose(heap, altered, catalog, forbidden_members(heap)), budget_unlimited())));
  RELATIONAL_TRY(kernel_catalog(
      heap, budget_run(heap, compose(heap, globals, catalog, no_members(heap)), budget_unlimited()), &shared));
  return relational_require(arity_one(family_arity(shared, str_lit("Shared"))),
                            str_lit("failed reuse ran a member or hid the composed schema"));
}

/* weakened(globals, catalog, original, mode) */
static Task weakened(Heap *heap, Globals globals, FamilyCatalog catalog, Family original, uint64_t mode) {
  Globals altered =
      global_add_family(heap, str_lit("Existing"), surface_template_common_altered(original, mode), globals);
  return surface_raw_common_exact(
      heap, surface_template_common_mismatch(heap, str_lit("Existing")),
      budget_run(heap, compose(heap, altered, catalog, no_members(heap)), budget_unlimited()));
}

/* The values the raw() case closures capture. */
typedef struct {
  Globals globals;
  FamilyCatalog catalog;
  Family original;
} RawContext;

static Task case_preserve(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return preserve(heap, raw->globals, raw->catalog, raw->original);
}

static Task case_cancelled(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return cancelled(heap, raw->globals, raw->catalog, raw->original);
}

static Task case_before_members(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return before_members(heap, raw->globals, raw->catalog, raw->original);
}

static Task case_provisional(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return weakened(heap, raw->globals, raw->catalog, raw->original, 1);
}

static Task case_constructors(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return weakened(heap, raw->globals, raw->catalog, raw->original, 2);
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

/* Check.CtorDecl{"seed", [], [], []} */
static CtorDecl *seed_ctor(Heap *heap) {
  CtorDecl *ctor = heap_alloc(heap, sizeof *ctor);
  if (ctor == NULL) {
    heap->oom = true;
    return NULL;
  }
  ctor->name = str_lit("seed");
  ctor->args = vec_new(heap->arena);
  ctor->res_params = vec_new(heap->arena);
  ctor->res_idx = vec_new(heap->arena);
  return ctor;
}

/* raw(): Done count goes to *count. */
static Task raw(Heap *heap, uint64_t *count) {
  FamilyCatalog catalog = NULL;
  RawContext *ctx = heap_alloc(heap, sizeof *ctx);
  if (ctx == NULL) return host_oom(heap);
  Vec ctors = one_item(heap, seed_ctor(heap));
  Vec members = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap,
                                family_declare(heap, global_empty(), family_empty(), budget_unlimited(),
                                               heap_nat_of_u64(heap, 1), seed_family(heap), ctors, &members),
                                &catalog));
  Vec levels = one_item(heap, (void *)level_zero());
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_globals(heap,
                                family_instantiate(heap, global_empty(), catalog, budget_unlimited(), str_lit("Seed"),
                                                   &levels, str_lit("Existing"), &reuse, no_exports(heap)),
                                &ctx->globals));
  ctx->catalog = catalog;
  RELATIONAL_TRY(surface_template_common_family(str_lit("Existing"), ctx->globals, &ctx->original));
  RawCase cases[] = {
      {str_lit("preserve-caller"), case_preserve, ctx},
      {str_lit("cancelled"), case_cancelled, ctx},
      {str_lit("refuse-before-members"), case_before_members, ctx},
      {str_lit("provisional"), case_provisional, ctx},
      {str_lit("constructors"), case_constructors, ctx},
  };
  return surface_template_common_run(heap, cases, sizeof cases / sizeof cases[0], 0, count);
}

/* suite(root) */
static Task suite(Heap *heap, Str root, Str *fields) {
  uint64_t count = 0;
  Str source = str_empty();
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(positive(heap));
  RELATIONAL_TRY(dependent(heap));
  RELATIONAL_TRY(surface_template_common_negatives(heap, surface_template_data_template_symbolic_reuse_negatives,
                                                   surface_template_data_template_symbolic_reuse_negatives_count,
                                                   global_initial(heap)));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_template_data_template_symbolic_reuse_parsers,
                                               surface_template_data_template_symbolic_reuse_parsers_count, true));
  RELATIONAL_TRY(raw(heap, &count));
  static const char *const files[] = {"prelude/cat/category-core.mech",       "prelude/cat/heterogeneous-functor.mech",
                                      "prelude/cat/composable-functors.mech", "prelude/cat/identity-functor.mech",
                                      "prelude/cat/shared-functor-chain.mech",
                                      "test/fixtures/prelude/symbolic-reuse-contracts.mech"};
  Vec paths = strings(heap, files, 6);
  RELATIONAL_TRY(relational_read_all(heap, root, &paths, &source));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, &program));
  static const char *const families[] = {"Chain_Middle", "Chain_Source", "Chain_Target"};
  Vec new_names = surface_template_common_new_family_names(heap, relational_globals(program), global_initial(heap));
  Vec expected_families = strings(heap, families, 3);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &new_names, &expected_families) &&
                                        surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("shared category inventory or axioms changed")));
  *fields = str_cat(heap->arena, str_lit("negatives=12 parser=6 raw="), str_of_u64(heap->arena, count));
  return task_pure();
}

/* arguments(args) = R.finish("TEMPLATE-SYMBOLIC-REUSE", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = suite(&heap, relational_root(argc, argv), &fields);
  int code = relational_finish(&heap, str_lit("TEMPLATE-SYMBOLIC-REUSE"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
