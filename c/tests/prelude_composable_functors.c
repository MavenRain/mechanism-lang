/* Port of bend2/tests/prelude_composable_functors.bend (79 lines): the composable functors prelude suite. Reads
 * the canonical heterogeneous functor prelude and the composable functors prelude under argv[1] (default "."), checks
 * the three functor mirrors (First, Second, Composite), the parse roundtrip, the empty symbolic check, the inventory
 * against the checked program and the globals, the computations, the pinned negative refusals, the universe arity
 * refusal and the budget refusal, then prints PRELUDE-COMPOSABLE-FUNCTORS-OK fields or
 * PRELUDE-COMPOSABLE-FUNCTORS-FAIL message. The list literals are generated from the Bend text (W/hop298-gen.py). */
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

/* One C.Pair<String, String> literal row. */
typedef struct {
  const char *name;
  const char *value;
} RenamePair;

/* A List<C.Pair<String, String>> literal as a Vec of boxed TextRename. */
static Vec renames(Heap *heap, const RenamePair *items, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) {
    TextRename *row = heap_alloc(heap, sizeof *row);
    if (row == NULL) return out;
    row->name = str_lit(items[i].name);
    row->value = str_lit(items[i].value);
    vec_push(&out, row);
  }
  return out;
}

#define RENAMES(heap, items) renames(heap, items, sizeof items / sizeof items[0])

/* One C.Pair<String, T.T> row of values(). */
static void computation(Heap *heap, Vec *out, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->expected = expected;
  vec_push(out, row);
}

/* expected_families() */
static const char *const EXPECTED_FAMILIES[] = {
    "Equal_Middle",
    "Equal_Source",
    "Equal_Target",
    "High",
    "Higher",
    "Mixed_Middle",
    "Mixed_Source",
    "Mixed_Target",
    "Run_Middle",
    "Run_Source",
    "Run_Target"};

/* negatives() */
static const char *const NEGATIVES[] = {
    "erased-object",
    "middle-record",
    "missing-law",
    "nominal-category",
    "nominal-equality",
    "reversed-functors",
    "wrong-identity-law",
    "wrong-middle-level",
    "wrong-source-level",
    "wrong-target-level"};

/* expected_entries() */
static const char *const EXPECTED_ENTRIES[] = {
    "Composed",
    "Equal_Composite_Functor",
    "Equal_Composite_eqCongr",
    "Equal_Composite_functorMap",
    "Equal_Composite_functorMapComp",
    "Equal_Composite_functorMapId",
    "Equal_Composite_functorObj",
    "Equal_First_Functor",
    "Equal_First_eqCongr",
    "Equal_First_functorMap",
    "Equal_First_functorMapComp",
    "Equal_First_functorMapId",
    "Equal_First_functorObj",
    "Equal_Middle_Category",
    "Equal_Middle_Hom",
    "Equal_Middle_assoc",
    "Equal_Middle_comp",
    "Equal_Middle_compId",
    "Equal_Middle_eqCongr",
    "Equal_Middle_eqTrans",
    "Equal_Middle_id",
    "Equal_Middle_idComp",
    "Equal_Second_Functor",
    "Equal_Second_eqCongr",
    "Equal_Second_functorMap",
    "Equal_Second_functorMapComp",
    "Equal_Second_functorMapId",
    "Equal_Second_functorObj",
    "Equal_Source_Category",
    "Equal_Source_Hom",
    "Equal_Source_assoc",
    "Equal_Source_comp",
    "Equal_Source_compId",
    "Equal_Source_eqCongr",
    "Equal_Source_eqTrans",
    "Equal_Source_id",
    "Equal_Source_idComp",
    "Equal_Target_Category",
    "Equal_Target_Hom",
    "Equal_Target_assoc",
    "Equal_Target_comp",
    "Equal_Target_compId",
    "Equal_Target_eqCongr",
    "Equal_Target_eqTrans",
    "Equal_Target_id",
    "Equal_Target_idComp",
    "Equal_compFunctor",
    "Lift",
    "MiddleCategory",
    "Mixed_Composite_Functor",
    "Mixed_Composite_eqCongr",
    "Mixed_Composite_functorMap",
    "Mixed_Composite_functorMapComp",
    "Mixed_Composite_functorMapId",
    "Mixed_Composite_functorObj",
    "Mixed_First_Functor",
    "Mixed_First_eqCongr",
    "Mixed_First_functorMap",
    "Mixed_First_functorMapComp",
    "Mixed_First_functorMapId",
    "Mixed_First_functorObj",
    "Mixed_Middle_Category",
    "Mixed_Middle_Hom",
    "Mixed_Middle_assoc",
    "Mixed_Middle_comp",
    "Mixed_Middle_compId",
    "Mixed_Middle_eqCongr",
    "Mixed_Middle_eqTrans",
    "Mixed_Middle_id",
    "Mixed_Middle_idComp",
    "Mixed_Second_Functor",
    "Mixed_Second_eqCongr",
    "Mixed_Second_functorMap",
    "Mixed_Second_functorMapComp",
    "Mixed_Second_functorMapId",
    "Mixed_Second_functorObj",
    "Mixed_Source_Category",
    "Mixed_Source_Hom",
    "Mixed_Source_assoc",
    "Mixed_Source_comp",
    "Mixed_Source_compId",
    "Mixed_Source_eqCongr",
    "Mixed_Source_eqTrans",
    "Mixed_Source_id",
    "Mixed_Source_idComp",
    "Mixed_Target_Category",
    "Mixed_Target_Hom",
    "Mixed_Target_assoc",
    "Mixed_Target_comp",
    "Mixed_Target_compId",
    "Mixed_Target_eqCongr",
    "Mixed_Target_eqTrans",
    "Mixed_Target_id",
    "Mixed_Target_idComp",
    "Mixed_compFunctor",
    "Raise",
    "Run_Composite_Functor",
    "Run_Composite_eqCongr",
    "Run_Composite_functorMap",
    "Run_Composite_functorMapComp",
    "Run_Composite_functorMapId",
    "Run_Composite_functorObj",
    "Run_First_Functor",
    "Run_First_eqCongr",
    "Run_First_functorMap",
    "Run_First_functorMapComp",
    "Run_First_functorMapId",
    "Run_First_functorObj",
    "Run_Middle_Category",
    "Run_Middle_Hom",
    "Run_Middle_assoc",
    "Run_Middle_comp",
    "Run_Middle_compId",
    "Run_Middle_eqCongr",
    "Run_Middle_eqTrans",
    "Run_Middle_id",
    "Run_Middle_idComp",
    "Run_Second_Functor",
    "Run_Second_eqCongr",
    "Run_Second_functorMap",
    "Run_Second_functorMapComp",
    "Run_Second_functorMapId",
    "Run_Second_functorObj",
    "Run_Source_Category",
    "Run_Source_Hom",
    "Run_Source_assoc",
    "Run_Source_comp",
    "Run_Source_compId",
    "Run_Source_eqCongr",
    "Run_Source_eqTrans",
    "Run_Source_id",
    "Run_Source_idComp",
    "Run_Target_Category",
    "Run_Target_Hom",
    "Run_Target_assoc",
    "Run_Target_comp",
    "Run_Target_compId",
    "Run_Target_eqCongr",
    "Run_Target_eqTrans",
    "Run_Target_id",
    "Run_Target_idComp",
    "Run_compFunctor",
    "SourceCategory",
    "TargetCategory",
    "composableInput",
    "compositeCompositionLaw",
    "compositeIdentityLaw",
    "compositionValue",
    "identityValue",
    "mapValue",
    "mixedCompositeSort",
    "mixedComposition",
    "mixedFirstSort",
    "mixedMiddleHom",
    "mixedSecondSort",
    "mixedSourceHom",
    "mixedTargetHom",
    "objectValue"};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "objectValue", relational_natural(heap, 78));
  computation(heap, &out, "mapValue", relational_natural(heap, 40));
  computation(heap, &out, "compositionValue", relational_natural(heap, 80));
  computation(heap, &out, "identityValue", relational_natural(heap, 37));
  return out;
}

/* mismatch_pins() */
static const RenamePair MISMATCH_PINS[] = {
    {"middle-record", "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D (Ran SPi 0 y D (Ran SPi w _ (Out SArg 0 y (APt 0 y) (Out SArg 0 x (APt 0 x) (Elim SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 4)) other as self"},
    {"missing-law", "expected type is (Lan SPi w map "},
    {"nominal-category", "Equal_Source"},
    {"nominal-equality", "Equal_Middle"},
    {"reversed-functors", "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D (Ran SPi 0 y D (Ran SPi w _ (Out SArg 0 y (APt 0 y) (Out SArg 0 x (APt 0 x) (Elim SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 1)) d as self return (Ran SPi 0 x D (Ran SPi 0 y D Type 1)) with | (ALeg 0) x y => x))) (Out SArg 0 y (APt 0 (Out SArg w _ (APt w y) obj)) (Out SArg 0 x (APt 0 (Out SArg w _ (APt w x) obj)) (Elim SPi w hom (Ran SPi 0 x E (Ran SPi 0 y E Type 1)) e as self return (Ran SPi 0 x E (Ran SPi 0 y E Type 1)) with | (ALeg 0) x y => x)))))) (Lan SPi w mapId (Ran SPi 0 x D (Lan SMu Equal_Target ["},
    {"wrong-identity-law", "the constructor categoryRefl of Run_Target"},
    {"wrong-middle-level", "the term has type (Ran SPi 0 Obj Type 3"},
    {"wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1"},
    {"wrong-target-level", "the term has type (Ran SPi 0 Obj Type 5"}};

/* The First mirror of suite(): Text.rename_words levels and mapping */
static const RenamePair FIRST_LEVELS[] = {
    {"u", "u"},
    {"v", "v"},
    {"w", "w"},
    {"z", "z"}};
static const RenamePair FIRST_MAPPING[] = {
    {"Functor", "First_Functor"},
    {"functorObj", "First_functorObj"},
    {"functorMap", "First_functorMap"},
    {"functorMapId", "First_functorMapId"},
    {"functorMapComp", "First_functorMapComp"},
    {"eqCongr", "First_eqCongr"},
    {"Source", "Source"},
    {"Source_Category", "Source_Category"},
    {"Source_Hom", "Source_Hom"},
    {"Source_id", "Source_id"},
    {"Source_comp", "Source_comp"},
    {"Source_idComp", "Source_idComp"},
    {"Source_compId", "Source_compId"},
    {"Source_assoc", "Source_assoc"},
    {"Source_eqTrans", "Source_eqTrans"},
    {"Source_eqCongr", "Source_eqCongr"},
    {"Target", "Middle"},
    {"Target_Category", "Middle_Category"},
    {"Target_Hom", "Middle_Hom"},
    {"Target_id", "Middle_id"},
    {"Target_comp", "Middle_comp"},
    {"Target_idComp", "Middle_idComp"},
    {"Target_compId", "Middle_compId"},
    {"Target_assoc", "Middle_assoc"},
    {"Target_eqTrans", "Middle_eqTrans"},
    {"Target_eqCongr", "Middle_eqCongr"}};

/* The Second mirror of suite(): Text.rename_words levels and mapping */
static const RenamePair SECOND_LEVELS[] = {
    {"u", "w"},
    {"v", "z"},
    {"w", "p"},
    {"z", "q"}};
static const RenamePair SECOND_MAPPING[] = {
    {"Functor", "Second_Functor"},
    {"functorObj", "Second_functorObj"},
    {"functorMap", "Second_functorMap"},
    {"functorMapId", "Second_functorMapId"},
    {"functorMapComp", "Second_functorMapComp"},
    {"eqCongr", "Second_eqCongr"},
    {"Source", "Middle"},
    {"Source_Category", "Middle_Category"},
    {"Source_Hom", "Middle_Hom"},
    {"Source_id", "Middle_id"},
    {"Source_comp", "Middle_comp"},
    {"Source_idComp", "Middle_idComp"},
    {"Source_compId", "Middle_compId"},
    {"Source_assoc", "Middle_assoc"},
    {"Source_eqTrans", "Middle_eqTrans"},
    {"Source_eqCongr", "Middle_eqCongr"},
    {"Target", "Target"},
    {"Target_Category", "Target_Category"},
    {"Target_Hom", "Target_Hom"},
    {"Target_id", "Target_id"},
    {"Target_comp", "Target_comp"},
    {"Target_idComp", "Target_idComp"},
    {"Target_compId", "Target_compId"},
    {"Target_assoc", "Target_assoc"},
    {"Target_eqTrans", "Target_eqTrans"},
    {"Target_eqCongr", "Target_eqCongr"}};

/* The Composite mirror of suite(): Text.rename_words levels and mapping */
static const RenamePair COMPOSITE_LEVELS[] = {
    {"u", "u"},
    {"v", "v"},
    {"w", "p"},
    {"z", "q"}};
static const RenamePair COMPOSITE_MAPPING[] = {
    {"Functor", "Composite_Functor"},
    {"functorObj", "Composite_functorObj"},
    {"functorMap", "Composite_functorMap"},
    {"functorMapId", "Composite_functorMapId"},
    {"functorMapComp", "Composite_functorMapComp"},
    {"eqCongr", "Composite_eqCongr"},
    {"Source", "Source"},
    {"Source_Category", "Source_Category"},
    {"Source_Hom", "Source_Hom"},
    {"Source_id", "Source_id"},
    {"Source_comp", "Source_comp"},
    {"Source_idComp", "Source_idComp"},
    {"Source_compId", "Source_compId"},
    {"Source_assoc", "Source_assoc"},
    {"Source_eqTrans", "Source_eqTrans"},
    {"Source_eqCongr", "Source_eqCongr"},
    {"Target", "Target"},
    {"Target_Category", "Target_Category"},
    {"Target_Hom", "Target_Hom"},
    {"Target_id", "Target_id"},
    {"Target_comp", "Target_comp"},
    {"Target_idComp", "Target_idComp"},
    {"Target_compId", "Target_compId"},
    {"Target_assoc", "Target_assoc"},
    {"Target_eqTrans", "Target_eqTrans"},
    {"Target_eqCongr", "Target_eqCongr"}};

/* refusal_error(error, name) */
static Task refusal_error(Heap *heap, Error error, Str name) {
  Vec pins = RENAMES(heap, MISMATCH_PINS);
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
                                   cat3(heap, str_lit("test/neg/composable-functors/"), name, str_lit(".mech")),
                                   &source));
    RELATIONAL_TRY(refusal(heap, elab_check_in_compact(heap, globals, source), name));
  }
  return task_pure();
}

/* One mirror check of suite(): R.assert(String.contains(target, Text.rename_words(body, levels, mapping)), message) */
static Task mirror(Heap *heap, Str target, Str body, const RenamePair *levels, size_t level_count,
                   const RenamePair *mapping, size_t mapping_count, const char *message) {
  Vec level_rows = renames(heap, levels, level_count);
  Vec mapping_rows = renames(heap, mapping, mapping_count);
  Str renamed = text_rename_words(heap, body, &level_rows, &mapping_rows);
  return relational_require(has_substring(target, renamed), str_lit(message));
}

#define MIRROR(heap, target, body, levels, mapping, message) \
  mirror(heap, target, body, levels, sizeof levels / sizeof levels[0], mapping, sizeof mapping / sizeof mapping[0], message)

/* The R.read_all path lists of suite() */
static const char *const SOURCE_PATHS[] = {"prelude/cat/category-core.mech", "prelude/cat/composable-functors.mech"};
static const char *const FIXTURE_PATHS[] = {"test/fixtures/prelude/composable-functors.mech",
                                            "test/fixtures/prelude/composable-functors-runtime.mech"};

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str canonical = str_empty();
  Str target = str_empty();
  Str body;
  Str source = str_empty();
  Str fixture = str_empty();
  const Vec *parsed = NULL;
  const ElabChecked *symbolic = NULL;
  const ElabChecked *checked = NULL;
  Globals globals;
  Vec source_paths = STRINGS(heap, SOURCE_PATHS);
  Vec fixture_paths = STRINGS(heap, FIXTURE_PATHS);
  Vec entries;
  Vec families;
  Vec rows;
  Vec negatives;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/heterogeneous-functor.mech"), &canonical));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/composable-functors.mech"), &target));
  body = text_member_body(heap, canonical, str_lit("def Functor "), str_lit("end"));
  RELATIONAL_TRY(relational_require(body.len > 1, str_lit("missing canonical functor members")));
  RELATIONAL_TRY(MIRROR(heap, target, body, FIRST_LEVELS, FIRST_MAPPING, "functor mirror changed: First"));
  RELATIONAL_TRY(MIRROR(heap, target, body, SECOND_LEVELS, SECOND_MAPPING, "functor mirror changed: Second"));
  RELATIONAL_TRY(MIRROR(heap, target, body, COMPOSITE_LEVELS, COMPOSITE_MAPPING, "functor mirror changed: Composite"));
  RELATIONAL_TRY(relational_read_all(heap, root, &source_paths, &source));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, source, fixture), &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &symbolic));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(symbolic), str_lit("symbolic definitions escaped")));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  globals = relational_globals(checked);
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  RELATIONAL_TRY(program_global_inventory(heap, globals, &entries));
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(relational_negative_inventory(heap, root, str_lit("test/neg/composable-functors"), &negatives));
  RELATIONAL_TRY(negative_loop(heap, &negatives, root, globals));
  RELATIONAL_TRY(relational_refuse_exact(
      heap,
      elab_check_in_compact(heap, global_empty(),
                            cat(heap, source, str_lit("specialize MechComposableFunctors (0, 1, 2, 3, 4) as Bad\n"))),
      str_lit("universe: the family schema MechComposableFunctors expects 6 universe arguments, got 5\n"),
      str_lit("universe arity")));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=158 instances=3 computations=4 negatives=12");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-COMPOSABLE-FUNCTORS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-COMPOSABLE-FUNCTORS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
