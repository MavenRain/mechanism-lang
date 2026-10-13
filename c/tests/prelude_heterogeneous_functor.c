/* Port of bend2/tests/prelude_heterogeneous_functor.bend (72 lines): the heterogeneous functor prelude suite. Reads
 * the category core, the heterogeneous functor prelude and the fixture clients under argv[1] (default "."), checks
 * the category-core mirror, the inventory against the initial globals, the required instances, the computations,
 * the pinned negative refusals, the universe arity refusal and the budget refusal, then prints
 * PRELUDE-HETEROGENEOUS-FUNCTOR-OK fields or PRELUDE-HETEROGENEOUS-FUNCTOR-FAIL message. */
#include "driver.h"
#include "relational_common.h"
#include "relational_program.h"
#include "relational_text.h"
#include <string.h>

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

static Str at_str(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return str_empty();
  return *(const Str *)got.value;
}

/* Base String.contains on byte strings. */
static bool has_substring(Str text, Str needle) {
  if (needle.len == 0) return true;
  if (needle.len > text.len) return false;
  for (size_t i = 0; i + needle.len <= text.len; i++) {
    if (memcmp(text.data + i, needle.data, needle.len) == 0) return true;
  }
  return false;
}

/* A List<String> literal as a Vec of boxed Str. */
static Vec strings(Heap *heap, const char *const *items, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) vec_push(&out, relational_str(heap, str_lit(items[i])));
  return out;
}

#define STRINGS(heap, items) strings(heap, items, sizeof items / sizeof items[0])

/* One C.Pair<String, T.T> row of values(). */
static void computation(Heap *heap, Vec *out, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->expected = expected;
  vec_push(out, row);
}

/* One C.Pair<String, String> row of mismatch_pins(). */
static void pin(Heap *heap, Vec *out, const char *name, const char *value) {
  TextRename *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->value = str_lit(value);
  vec_push(out, row);
}

/* expected_families() */
static const char *const EXPECTED_FAMILIES[] = {"Equal_Source", "Equal_Target", "High",      "Mixed_Source",
                                                "Mixed_Target", "Run_Source",   "Run_Target"};

/* negatives() */
static const char *const NEGATIVES[] = {"erased-object",         "missing-law",    "nominal-category",
                                        "nominal-equality",      "wrong-composition-law", "wrong-endpoint",
                                        "wrong-identity-law",    "wrong-object",   "wrong-source-level",
                                        "wrong-target-level"};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "objectValue", relational_natural(heap, 39));
  computation(heap, &out, "mapValue", relational_natural(heap, 40));
  computation(heap, &out, "compositionValue", relational_natural(heap, 80));
  return out;
}

/* mismatch_pins() */
static Vec mismatch_pins(Heap *heap) {
  Vec out = vec_new(heap->arena);
  pin(heap, &out, "missing-law", "expected type is (Lan SPi w mapId");
  pin(heap, &out, "nominal-category", "hom (Ran SPi 0 x C (Ran SPi 0 y C Type 1))");
  pin(heap, &out, "nominal-equality", "the term has type (Lan SMu Equal_Source");
  pin(heap, &out, "wrong-composition-law", "the constructor categoryRefl of Mixed_Target");
  pin(heap, &out, "wrong-endpoint", "the term has type (Ran SPi 0 C Type 1 (Ran SPi 0 D Type 3");
  pin(heap, &out, "wrong-identity-law", "the constructor categoryRefl of Run_Target");
  pin(heap, &out, "wrong-object", "the term has type Nat and the expected type is (Lan SMu High");
  pin(heap, &out, "wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1");
  pin(heap, &out, "wrong-target-level", "the term has type (Ran SPi 0 Obj Type 3");
  return out;
}

/* The R.read_all path lists of suite() */
static const char *const SOURCE_PATHS[] = {"prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech"};
static const char *const FIXTURE_PATHS[] = {"test/fixtures/prelude/heterogeneous-functor.mech",
                                            "test/fixtures/prelude/heterogeneous-functor-runtime.mech"};

/* R.prefixes(["Mixed", "Equal", "Run"], [...]) of suite() */
static const char *const INSTANCE_NAMES[] = {"Mixed", "Equal", "Run"};
static const char *const INSTANCE_MEMBERS[] = {
    "Functor",          "functorObj",         "functorMap",      "functorMapId",      "functorMapComp",
    "eqCongr",          "Source_Category",    "Source_Hom",      "Source_id",         "Source_comp",
    "Source_idComp",    "Source_compId",      "Source_assoc",    "Source_eqTrans",    "Source_eqCongr",
    "Target_Category",  "Target_Hom",         "Target_id",       "Target_comp",       "Target_idComp",
    "Target_compId",    "Target_assoc",       "Target_eqTrans",  "Target_eqCongr"};

/* refusal_error(error, name) */
static Task refusal_error(Heap *heap, Error error, Str name) {
  Vec pins = mismatch_pins(heap);
  switch (error.kind) {
  case ERROR_MISMATCH:
    return relational_require(!str_eq(name, str_lit("erased-object")) &&
                                  has_substring(error.message, text_lookup(&pins, name)),
                              cat(heap, name, str_lit(": wrong mismatch refusal")));
  case ERROR_QUANTITY:
    return relational_require(str_eq(name, str_lit("erased-object")) &&
                                  str_eq(error.message, str_lit("the erased binder x is read in a runtime position")),
                              cat(heap, name, str_lit(": wrong quantity refusal")));
  case ERROR_PARSE:
  case ERROR_NOT_YET:
  case ERROR_CARRY:
  case ERROR_UNBOUND:
  case ERROR_UNIVERSE:
  case ERROR_WRONG_LEG:
  case ERROR_MISSING_BRANCH:
  case ERROR_OVERFLOW:
  case ERROR_CANNOT_INFER:
  case ERROR_BUDGET_EXHAUSTED:
  case ERROR_INDEX_NOT_ZERO:
  case ERROR_INDEX_ABOVE_UNIVERSE:
  case ERROR_TERMINATION:
    return task_fail(cat(heap, name, str_lit(": wrong refusal")));
  }
  return task_fail(cat(heap, name, str_lit(": wrong refusal")));
}

/* refusal(result, name) */
static Task refusal(Heap *heap, Result result, Str name) {
  switch (result.kind) {
  case RESULT_DONE:
    return task_fail(cat(heap, name, str_lit(": expected refusal")));
  case RESULT_FAIL:
    return refusal_error(heap, result.error, name);
  }
  return task_fail(cat(heap, name, str_lit(": expected refusal")));
}

/* negative_loop(names, root, globals) */
static Task negative_loop(Heap *heap, const Vec *names, Str root, Globals globals) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    Str source = str_empty();
    RELATIONAL_TRY(relational_read(heap, root,
                                   cat3(heap, str_lit("test/neg/heterogeneous-functor/"), name, str_lit(".mech")),
                                   &source));
    RELATIONAL_TRY(refusal(heap, elab_check_in_compact(heap, globals, source), name));
  }
  return task_pure();
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str core = str_empty();
  Str category = str_empty();
  Str source = str_empty();
  Str fixture = str_empty();
  const Vec *parsed = NULL;
  const ElabChecked *symbolic = NULL;
  const ElabChecked *checked = NULL;
  Globals globals;
  Vec source_paths = STRINGS(heap, SOURCE_PATHS);
  Vec fixture_paths = STRINGS(heap, FIXTURE_PATHS);
  Vec families;
  Vec instance_names;
  Vec instance_members;
  Vec required;
  Vec rows;
  Vec negatives;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/category-core.mech"), &core));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/category.mech"), &category));
  RELATIONAL_TRY(relational_require(str_eq(text_first_lines(heap, text_rename_core(heap, core), 70),
                                           text_first_lines(heap, category, 70)),
                                    str_lit("category-core mirror changed")));
  RELATIONAL_TRY(relational_read_all(heap, root, &source_paths, &source));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, source, fixture), &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &symbolic));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(symbolic), str_lit("symbolic definitions escaped")));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  globals = relational_globals(checked);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_count_initial(heap, checked, 90, &families));
  instance_names = STRINGS(heap, INSTANCE_NAMES);
  instance_members = STRINGS(heap, INSTANCE_MEMBERS);
  required = relational_prefixes(heap, &instance_names, &instance_members);
  RELATIONAL_TRY(relational_required(heap, &required, globals));
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(relational_negative_inventory(heap, root, str_lit("test/neg/heterogeneous-functor"), &negatives));
  RELATIONAL_TRY(negative_loop(heap, &negatives, root, globals));
  RELATIONAL_TRY(relational_refuse_exact(
      heap,
      elab_check_in_compact(heap, global_empty(),
                    cat(heap, source, str_lit("specialize MechHeterogeneousFunctor (0, 1, 2) as Bad\n"))),
      str_lit("universe: the family schema MechHeterogeneousFunctor expects 4 universe arguments, got 3\n"),
      str_lit("universe arity")));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=90 instances=3 computations=3 negatives=12");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-HETEROGENEOUS-FUNCTOR", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-HETEROGENEOUS-FUNCTOR"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
