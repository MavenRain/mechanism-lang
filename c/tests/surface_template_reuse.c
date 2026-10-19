/* Port of tests/surface_template_reuse.bend (104 lines, oracle label surface-template-reuse): template
 * reuse keeps the reused family identities (positive, dependent, partial dependent and level reuse), the reuse
 * refusals and parsers, six raw cases (identity reuse, budget, weakened certificates, a failed reuse), and the
 * category prelude reuse contracts. argv[1] (optional) is the repository root (R.root). Prints
 * TEMPLATE-REUSE-OK negatives=17 parser=6 raw=<n> or TEMPLATE-REUSE-FAIL <message>. */
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

/* positive() */
static Task positive(Heap *heap) {
  Str source = str_lit(surface_template_data_template_reuse_positive);
  const ElabChecked *program = NULL;
  const ElabChecked *original = NULL;
  Family existing;
  RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, source, str_lit("reuse round trip changed")));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, &program));
  RELATIONAL_TRY(
      relational_checked(heap, global_initial(heap), str_lit(surface_template_data_template_reuse_base), &original));
  Globals globals = relational_globals(program);
  RELATIONAL_TRY(surface_template_common_family(str_lit("Existing"), relational_globals(original), &existing));
  RELATIONAL_TRY(relational_require(
      surface_raw_common_matched_family(existing, global_find_family(str_lit("Existing"), globals)),
      str_lit("reused family was replaced")));
  static const char *const families[] = {"Existing", "Partial_Right"};
  Vec new_names = surface_template_common_new_family_names(heap, globals, global_initial(heap));
  Vec expected_families = strings(heap, families, 2);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &new_names, &expected_families),
                                    str_lit("wrong reuse family inventory")));
  static const char *const definitions[] = {"Existing_unbox",      "Shared_Left_unbox",   "Shared_Right_unbox",
                                            "Partial_Left_unbox",  "Partial_Right_unbox", "N_Inner_Left_unbox",
                                            "N_Inner_Right_unbox", "payload",             "value",
                                            "partial",             "nested"};
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  Vec expected_entries = strings(heap, definitions, 11);
  RELATIONAL_TRY(relational_require(relational_strings(&entry_names, &expected_entries) &&
                                        surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("wrong definition inventory")));
  RELATIONAL_TRY(relational_audit_entries(heap, surface_template_common_entries(globals), global_initial(heap)));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "value", relational_natural(heap, 37));
  push_row(heap, &computations, "partial", relational_natural(heap, 37));
  push_row(heap, &computations, "nested", relational_natural(heap, 37));
  return relational_computations(heap, &computations, globals);
}

/* dependent() */
static Task dependent(Heap *heap) {
  Str base = str_lit(surface_template_data_template_reuse_dependent_base);
  const ElabChecked *original = NULL;
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_checked(heap, global_empty(), base, &original));
  RELATIONAL_TRY(relational_checked(
      heap, global_empty(),
      str_cat(heap->arena, base,
              str_lit("specialize Marker (0) as Shared with (Marker := Old, Wrapper := Old_Wrapper) def witness : "
                      "Old_Wrapper mark := Shared_value")),
      &program));
  return relational_require(
      surface_template_common_same_families(relational_globals(program), relational_globals(original)),
      str_lit("dependent reuse changed family identities"));
}

/* partial_dependent() */
static Task partial_dependent(Heap *heap) {
  Str base = str_lit(surface_template_data_template_reuse_dependent_base);
  const ElabChecked *original = NULL;
  const ElabChecked *program = NULL;
  Family old;
  const Term *value = NULL;
  RELATIONAL_TRY(relational_checked(heap, global_empty(), base, &original));
  RELATIONAL_TRY(relational_checked(
      heap, global_empty(), str_cat(heap->arena, base, str_lit("specialize Marker (0) as Sh with (Marker := Old)")),
      &program));
  Globals globals = relational_globals(program);
  Globals before = relational_globals(original);
  RELATIONAL_TRY(surface_template_common_family(str_lit("Old"), before, &old));
  RELATIONAL_TRY(relational_require(surface_raw_common_matched_family(old, global_find_family(str_lit("Old"), globals)),
                                    str_lit("partial reuse replaced the reused family")));
  static const char *const families[] = {"Sh_Wrapper"};
  Vec new_names = surface_template_common_new_family_names(heap, globals, before);
  Vec expected_families = strings(heap, families, 1);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &new_names, &expected_families),
                                    str_lit("wrong partial reuse family inventory")));
  RELATIONAL_TRY(relational_require(surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("partial reuse introduced axioms")));
  RELATIONAL_TRY(relational_normalize(heap, globals, str_lit("Sh_value"), &value));
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  return relational_require(relational_contains(&entry_names, str_lit("Sh_value")),
                            str_lit("the fresh companion member is missing"));
}

/* level_reuse(source, argument, message) */
static Task level_reuse(Heap *heap, Str source, Str argument, Str message) {
  const ElabChecked *checked = NULL;
  const ElabChecked *program = NULL;
  Family original;
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &checked));
  Str specialize = str_cat(heap->arena, str_lit("specialize R ("), argument);
  specialize = str_cat(heap->arena, specialize, str_lit(") as Y with (R := E)"));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), str_cat(heap->arena, source, specialize), &program));
  RELATIONAL_TRY(surface_template_common_family(str_lit("E"), relational_globals(checked), &original));
  return relational_require(
      surface_raw_common_matched_family(original, global_find_family(str_lit("E"), relational_globals(program))),
      message);
}

/* instance(globals, catalog) */
static Comp instance(Heap *heap, Globals globals, FamilyCatalog catalog) {
  Vec levels = one_item(heap, (void *)level_zero());
  Vec reuse = one_item(heap, rename_box(heap, "Marker", "Existing"));
  return family_instantiate_comp(heap, globals, catalog, str_lit("Marker"), &levels, str_lit("Alias"), &reuse,
                                 no_exports(heap));
}

/* weakened(globals, catalog, original, mode) */
static Task weakened(Heap *heap, Globals globals, FamilyCatalog catalog, Family original, uint64_t mode) {
  Globals altered =
      global_add_family(heap, str_lit("Existing"), surface_template_common_altered(original, mode), globals);
  return surface_raw_common_exact(heap, surface_template_common_mismatch(heap, str_lit("Existing")),
                                  budget_run(heap, instance(heap, altered, catalog), budget_unlimited()));
}

/* The values the raw() case closures capture. */
typedef struct {
  Globals globals;
  FamilyCatalog catalog;
  Globals reused;
  Family original;
} RawContext;

static Task case_identity(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  (void)heap;
  return relational_require(surface_raw_common_same_globals(raw->globals, raw->reused),
                            str_lit("root family reuse changed globals"));
}

static Task case_budget(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return surface_raw_common_exact(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap, instance(heap, raw->globals, raw->catalog), surface_foundation_common_exhausted(heap)));
}

static Task case_weakened_positive(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return weakened(heap, raw->globals, raw->catalog, raw->original, 0);
}

static Task case_weakened_status(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return weakened(heap, raw->globals, raw->catalog, raw->original, 1);
}

static Task case_weakened_ctors(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  return weakened(heap, raw->globals, raw->catalog, raw->original, 2);
}

static Task case_failed_keeps(Heap *heap, void *ctx) {
  const RawContext *raw = ctx;
  (void)heap;
  return relational_require(
      surface_raw_common_matched_family(raw->original, global_find_family(str_lit("Existing"), raw->globals)),
      str_lit("failed reuse changed original family"));
}

/* Check.FamilyDecl{"Marker", [], [], L.one()} */
static FamilyDecl marker_family(Heap *heap) {
  FamilyDecl marker;
  marker.name = str_lit("Marker");
  marker.params = vec_new(heap->arena);
  marker.indices = vec_new(heap->arena);
  marker.level = level_one(heap);
  return marker;
}

/* Check.CtorDecl{"marker", [], [], []} */
static CtorDecl *marker_ctor(Heap *heap) {
  CtorDecl *ctor = heap_alloc(heap, sizeof *ctor);
  if (ctor == NULL) {
    heap->oom = true;
    return NULL;
  }
  ctor->name = str_lit("marker");
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
  Vec ctors = one_item(heap, marker_ctor(heap));
  Vec members = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap,
                                family_declare(heap, global_empty(), family_empty(), budget_unlimited(),
                                               heap_nat_of_u64(heap, 1), marker_family(heap), ctors, &members),
                                &catalog));
  Vec levels = one_item(heap, (void *)level_zero());
  Vec reuse = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_globals(heap,
                                family_instantiate(heap, global_empty(), catalog, budget_unlimited(), str_lit("Marker"),
                                                   &levels, str_lit("Existing"), &reuse, no_exports(heap)),
                                &ctx->globals));
  ctx->catalog = catalog;
  RELATIONAL_TRY(
      kernel_globals(heap, budget_run(heap, instance(heap, ctx->globals, catalog), budget_unlimited()), &ctx->reused));
  RELATIONAL_TRY(surface_template_common_family(str_lit("Existing"), ctx->globals, &ctx->original));
  RawCase cases[] = {
      {str_lit("identity reuse keeps globals"), case_identity, ctx},
      {str_lit("an exhausted budget refuses the reuse"), case_budget, ctx},
      {str_lit("a weakened certificate refuses the reuse"), case_weakened_positive, ctx},
      {str_lit("a weakened certificate refuses the reuse"), case_weakened_status, ctx},
      {str_lit("a weakened certificate refuses the reuse"), case_weakened_ctors, ctx},
      {str_lit("a failed reuse keeps the original family"), case_failed_keeps, ctx},
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
  RELATIONAL_TRY(partial_dependent(heap));
  RELATIONAL_TRY(level_reuse(heap, str_lit(surface_template_data_template_reuse_level_base), str_lit("(max 0 0)"),
                             str_lit("level reuse changed the reused family")));
  RELATIONAL_TRY(level_reuse(heap, str_lit(surface_template_data_template_reuse_param_level_base), str_lit("(max 0 0)"),
                             str_lit("parameterized level reuse changed the reused family")));
  RELATIONAL_TRY(level_reuse(heap, str_lit(surface_template_data_template_reuse_decl_level_base), str_lit("0"),
                             str_lit("declared level reuse changed the reused family")));
  RELATIONAL_TRY(surface_template_common_negatives(heap, surface_template_data_template_reuse_negatives,
                                                   surface_template_data_template_reuse_negatives_count,
                                                   global_initial(heap)));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_template_data_template_reuse_parsers,
                                               surface_template_data_template_reuse_parsers_count, true));
  RELATIONAL_TRY(raw(heap, &count));
  static const char *const files[] = {"prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
                                      "prelude/cat/composable-functors.mech", "prelude/cat/identity-functor.mech",
                                      "test/fixtures/prelude/reuse-contracts.mech"};
  Vec paths = strings(heap, files, 5);
  RELATIONAL_TRY(relational_read_all(heap, root, &paths, &source));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, &program));
  RELATIONAL_TRY(relational_require(surface_template_common_no_axioms(heap, relational_rows(program)),
                                    str_lit("category reuse introduced axioms")));
  static const char *const families[] = {"Middle", "Source", "Target"};
  Vec new_names = surface_template_common_new_family_names(heap, relational_globals(program), global_initial(heap));
  Vec expected_families = strings(heap, families, 3);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &new_names, &expected_families),
                                    str_lit("category specializations allocated fresh families")));
  *fields = str_cat(heap->arena, str_lit("negatives=17 parser=6 raw="), str_of_u64(heap->arena, count));
  return task_pure();
}

/* arguments(args) = R.finish("TEMPLATE-REUSE", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = suite(&heap, relational_root(argc, argv), &fields);
  int code = relational_finish(&heap, str_lit("TEMPLATE-REUSE"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
