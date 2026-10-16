/* Port of bend2/tests/surface_prenex_dependency_exports.bend (105 lines, oracle label
 * surface-prenex-dependency-exports): the dependency export positives roundtrip, check without symbolic
 * leaks and keep the closed instance rows; the closed instance keeps its order and its prefixed dependency
 * family; the negatives refuse with the exact text; the parser cases refuse; two budget boundaries refuse; the
 * export and dependency runtime fixtures parse, print and check on the initial globals with no axiom, no fresh
 * family for a nested reuse and two payloads that normalize. Prints PRENEX-DEPENDENCY-EXPORTS-OK fields, or
 * dies with code 1 and PRENEX-DEPENDENCY-EXPORTS FAIL message on stderr. */
#include "driver.h"
#include "surface_prenex_common.h"
#include "surface_prenex_exports.h"
#include "surface_raw_common.h"

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

/* exact(label, expected, result) */
static Task exact(Heap *heap, Str label, Str expected, Result result) {
  switch (result.kind) {
  case RESULT_DONE:
    return task_fail(str_cat(heap->arena, str_cat(heap->arena, label, str_lit(": expected refusal: ")), expected));
  case RESULT_FAIL: {
    Str rendered = error_to_string(heap, result.error);
    return relational_require(str_eq(rendered, expected),
                              str_cat(heap->arena, str_cat(heap->arena, label, str_lit(": wrong refusal: ")), rendered));
  }
  }
  return task_fail(str_lit("unknown result kind"));
}

/* negatives(cases) */
static Task negatives(Heap *heap, const PrenexNegative *cases, size_t len) {
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(exact(heap, str_lit(cases[i].name), str_lit(cases[i].detail),
                         elab_check_in(heap, global_initial(heap), budget_unlimited(), str_lit(cases[i].source))));
  }
  return task_pure();
}

/* roundtrip(source) = Exports.roundtrip(source, "parse/print changed a dependency export mapping") */
static Task roundtrip(Heap *heap, Str source) {
  return surface_prenex_exports_roundtrip(heap, source, str_lit("parse/print changed a dependency export mapping"));
}

/* positives(cases) */
static Task positives(Heap *heap, const PrenexPositive *cases, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const ElabChecked *program = NULL;
    const ElabChecked *closed = NULL;
    Str source = str_lit(cases[i].source);
    RELATIONAL_TRY(roundtrip(heap, source));
    RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &program));
    RELATIONAL_TRY(relational_require(relational_empty_symbolic(program),
                                      str_lit("symbolic declarations escaped composition")));
    RELATIONAL_TRY(relational_checked(
        heap, global_empty(), str_cat(heap->arena, source, str_lit("specialize Pair (0) as Closed")), &closed));
    Vec names = relational_entry_names(heap, relational_rows(closed));
    Vec expected = strings(heap, cases[i].rows, cases[i].rows_count);
    RELATIONAL_TRY(relational_require(
        relational_strings(&names, &expected),
        str_lit("the closed instance rows differ from the chosen names in declaration order")));
  }
  return task_pure();
}

/* instance() */
static Task instance(Heap *heap) {
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_checked(heap, global_empty(), str_lit(surface_prenex_data_prenex_dependency_exports_instance),
                                    &program));
  Globals globals = relational_globals(program);
  static const char *const order[] = {"Run_w", "Run_a", "Run_pick"};
  static const char *const hidden[] = {"w",           "a",           "Local_witness",
                                       "Local_alias", "Run_Local_witness", "Run_Local_alias"};
  Vec names = relational_entry_names(heap, relational_rows(program));
  Vec expected = strings(heap, order, 3);
  RELATIONAL_TRY(
      relational_require(relational_strings(&names, &expected), str_lit("dependency exports changed declaration order")));
  RELATIONAL_TRY(relational_require(surface_raw_common_present(global_find_family(str_lit("Run_Local"), globals).some),
                                    str_lit("the closed instance lost the prefixed dependency family")));
  Vec absent = strings(heap, hidden, 6);
  return relational_require(surface_prenex_common_absent_entries(&absent, globals),
                            str_lit("default or local export names escaped"));
}

/* measured(label, expected, state): expected NULL is None{}; polls = 100000n - remaining (Nat, floor 0). */
static Task measured(Heap *heap, Str label, const char *expected, BudgetState state, uint64_t *polls) {
  if (state.budget.kind != BUDGET_POLLS) return task_fail(str_lit("finite poll measurement disappeared"));
  if (expected != NULL) {
    RELATIONAL_TRY(exact(heap, str_cat(heap->arena, label, str_lit(" baseline")), str_lit(expected), state.result));
  } else {
    void *checked = NULL;
    RELATIONAL_TRY(relational_kernel(heap, state.result, &checked));
  }
  NatToU64 remaining = nat_to_u64(state.budget.remaining);
  *polls = (remaining.status == NAT_TO_U64_OVERFLOW || remaining.value > 100000) ? 0 : 100000 - remaining.value;
  return task_pure();
}

/* group(body) = "poly (v) group Pair where " ++ body ++ " end " */
static Str group(Heap *heap, Str body) {
  return str_cat(heap->arena, str_cat(heap->arena, str_lit("poly (v) group Pair where "), body), str_lit(" end "));
}

/* budgets() */
static Task budgets(Heap *heap) {
  Str reservation = str_lit("poly (u) def w : Sort (succ u) := Sort u ");
  Str seed = str_lit(surface_prenex_data_prenex_dependency_exports_seed);
  Str dependency = str_lit(surface_prenex_data_prenex_dependency_exports_dependency);
  uint64_t ceiling = 0;
  uint64_t composition = 0;
  RELATIONAL_TRY(surface_prenex_common_polls(heap, str_cat(heap->arena, reservation, seed), &ceiling));
  RELATIONAL_TRY(surface_prenex_common_exhausted(
      heap, str_lit("collision-budget"),
      str_cat(heap->arena, reservation, str_lit(surface_prenex_data_prenex_dependency_exports_complete)), ceiling + 3));
  Str baseline =
      str_cat(heap->arena, seed, group(heap, str_cat(heap->arena, dependency, str_lit("export ()"))));
  RELATIONAL_TRY(measured(heap, str_lit("composition-budget"), "mismatch: every family member needs an export name",
                          budget_run_state(heap, surface_prenex_common_action(heap, global_empty(), baseline),
                                           budget_of_polls(nat_of_u64(heap->arena, 100000))),
                          &composition));
  return surface_prenex_common_exhausted(
      heap, str_lit("composition-budget"),
      str_cat(heap->arena, seed,
              group(heap, str_cat(heap->arena, dependency, str_lit("export (witness := Local, alias := a)")))),
      composition);
}

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

static Str field(Heap *heap, Str acc, const char *label, uint64_t value) {
  return str_cat(heap->arena, str_cat(heap->arena, acc, str_lit(label)), str_of_u64(heap->arena, value));
}

/* suite(root) */
static Task suite(Heap *heap, Str root, Str *fields) {
  Str base = str_empty();
  Str extra = str_empty();
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(positives(heap, surface_prenex_data_prenex_dependency_exports_positives,
                           surface_prenex_data_prenex_dependency_exports_positives_count));
  RELATIONAL_TRY(instance(heap));
  RELATIONAL_TRY(negatives(heap, surface_prenex_data_prenex_dependency_exports_negatives,
                           surface_prenex_data_prenex_dependency_exports_negatives_count));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_prenex_data_prenex_dependency_exports_parsers,
                                               surface_prenex_data_prenex_dependency_exports_parsers_count, true));
  RELATIONAL_TRY(budgets(heap));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/prenex-exports-runtime.mech"), &base));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/dependency-exports-runtime.mech"), &extra));
  Str source = str_cat(heap->arena, base, extra);
  RELATIONAL_TRY(roundtrip(heap, source));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, &program));
  Globals globals = relational_globals(program);
  Vec disclosures = elab_disclosure_names(heap, relational_rows(program));
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(
      relational_require(relational_strings(&disclosures, &none), str_lit("dependency exports introduced an axiom")));
  RELATIONAL_TRY(relational_require(
      surface_raw_common_absent(global_find_family(str_lit("NestedRun_Again"), globals).some) &&
          surface_prenex_common_family_count(globals) == surface_prenex_common_family_count(global_initial(heap)) + 8,
      str_lit("nested reuse installed fresh families")));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "dependencyPayload", relational_natural(heap, 37));
  push_row(heap, &computations, "nestedPayload", relational_natural(heap, 37));
  RELATIONAL_TRY(relational_computations(heap, &computations, globals));
  Str out = field(heap, str_empty(), "positives=", (uint64_t)surface_prenex_data_prenex_dependency_exports_positives_count);
  out = field(heap, out, " negatives=", (uint64_t)surface_prenex_data_prenex_dependency_exports_negatives_count);
  out = field(heap, out, " parser=", (uint64_t)surface_prenex_data_prenex_dependency_exports_parsers_count);
  *fields = str_cat(heap->arena, out, str_lit(" budget=2"));
  return task_pure();
}

/* finish(result): Done prints PRENEX-DEPENDENCY-EXPORTS-OK details; Fail dies with code 1. */
static int finish(Heap *heap, Task result, Str details) {
  if (result.ok) return driver_print(str_cat(heap->arena, str_lit("PRENEX-DEPENDENCY-EXPORTS-OK "), details));
  return driver_die_text(1, str_cat(heap->arena, str_lit("PRENEX-DEPENDENCY-EXPORTS FAIL "), result.message));
}

/* arguments(args) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str details = str_empty();
  Task result = suite(&heap, relational_root(argc, argv), &details);
  int code = finish(&heap, result, details);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
