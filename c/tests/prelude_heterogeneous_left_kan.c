/* Port of bend2/tests/prelude_heterogeneous_left_kan.bend (47 lines): the heterogeneous left kan prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-HETEROGENEOUS-LEFT-KAN-OK fields or PRELUDE-HETEROGENEOUS-LEFT-KAN-FAIL message. Same definitions in the same order. */
#include "driver.h"
#include "relational_common.h"
#include "relational_negatives.h"
#include "relational_program.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

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

/* expected_families() (bend2/tests/prelude_heterogeneous_left_kan.bend line 15, 13 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "EqualMiddle", "EqualSource", "Mixed_Base_Base_Middle", "Mixed_Base_Base_Source", "Mixed_Base_Base_Target",
    "Path", "Point", "Run_Base_Base_Middle", "Run_Base_Base_Source", "Run_Base_Base_Target", "Wide_Base_Base_Middle",
    "Wide_Base_Base_Source", "Wide_Base_Base_Target"
};

/* negatives() (bend2/tests/prelude_heterogeneous_left_kan.bend line 18, 13 names) */
static const char *const NEGATIVES[] = {
    "cocone-endpoint", "cocone-sort", "extension-sort", "middle-level", "missing-factor", "missing-uniqueness",
    "nominal-category", "solution-sort", "source-level", "tail-sort", "target-level", "uniqueness-endpoint",
    "wrong-factor"
};

/* expected_entries() (bend2/tests/prelude_heterogeneous_left_kan.bend line 21, 294 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "Chosen", "Cocone", "Components", "Constant", "DoubleEnd", "EqualMiddle_Category",
    "EqualMiddle_Hom", "EqualMiddle_assoc", "EqualMiddle_comp", "EqualMiddle_compId", "EqualMiddle_eqCongr",
    "EqualMiddle_eqTrans", "EqualMiddle_id", "EqualMiddle_idComp", "EqualSource_Category", "EqualSource_Hom",
    "EqualSource_assoc", "EqualSource_comp", "EqualSource_compId", "EqualSource_eqCongr", "EqualSource_eqTrans",
    "EqualSource_id", "EqualSource_idComp", "Eta", "Extension", "Forget", "IndexedEnd", "IndexedPlain", "Lift",
    "Mediator", "MixedCoconeContract", "MixedCoconeSort", "MixedExtensionSort", "MixedFactorContract",
    "MixedMiddleHom", "MixedSolutionSort", "MixedSourceHom", "MixedTailSort", "MixedTargetHom", "MixedUniqueContract",
    "Mixed_Base_Base_Composite_Functor", "Mixed_Base_Base_Composite_eqCongr", "Mixed_Base_Base_Composite_functorMap",
    "Mixed_Base_Base_Composite_functorMapComp", "Mixed_Base_Base_Composite_functorMapId",
    "Mixed_Base_Base_Composite_functorObj", "Mixed_Base_Base_First_Functor", "Mixed_Base_Base_First_eqCongr",
    "Mixed_Base_Base_First_functorMap", "Mixed_Base_Base_First_functorMapComp", "Mixed_Base_Base_First_functorMapId",
    "Mixed_Base_Base_First_functorObj", "Mixed_Base_Base_Middle_Category", "Mixed_Base_Base_Middle_Hom",
    "Mixed_Base_Base_Middle_assoc", "Mixed_Base_Base_Middle_comp", "Mixed_Base_Base_Middle_compId",
    "Mixed_Base_Base_Middle_eqCongr", "Mixed_Base_Base_Middle_eqTrans", "Mixed_Base_Base_Middle_id",
    "Mixed_Base_Base_Middle_idComp", "Mixed_Base_Base_Second_Functor", "Mixed_Base_Base_Second_eqCongr",
    "Mixed_Base_Base_Second_functorMap", "Mixed_Base_Base_Second_functorMapComp",
    "Mixed_Base_Base_Second_functorMapId", "Mixed_Base_Base_Second_functorObj", "Mixed_Base_Base_Source_Category",
    "Mixed_Base_Base_Source_Hom", "Mixed_Base_Base_Source_assoc", "Mixed_Base_Base_Source_comp",
    "Mixed_Base_Base_Source_compId", "Mixed_Base_Base_Source_eqCongr", "Mixed_Base_Base_Source_eqTrans",
    "Mixed_Base_Base_Source_id", "Mixed_Base_Base_Source_idComp", "Mixed_Base_Base_Target_Category",
    "Mixed_Base_Base_Target_Hom", "Mixed_Base_Base_Target_assoc", "Mixed_Base_Base_Target_comp",
    "Mixed_Base_Base_Target_compId", "Mixed_Base_Base_Target_eqCongr", "Mixed_Base_Base_Target_eqTrans",
    "Mixed_Base_Base_Target_id", "Mixed_Base_Base_Target_idComp", "Mixed_Base_Base_compFunctor",
    "Mixed_Base_Composite_NatTrans", "Mixed_Base_Composite_natApp", "Mixed_Base_Composite_naturality",
    "Mixed_Base_First_NatTrans", "Mixed_Base_First_natApp", "Mixed_Base_First_naturality",
    "Mixed_Base_Second_NatTrans", "Mixed_Base_Second_natApp", "Mixed_Base_Second_naturality",
    "Mixed_Base_targetEqSymm", "Mixed_Base_whiskerLeft", "Mixed_Base_whiskerRight", "Mixed_LanCocone",
    "Mixed_LanFactor", "Mixed_LanSolution", "Mixed_LanTail", "Mixed_LeftKanExtension", "Mixed_coconeFirst",
    "Mixed_coconeSecond", "Mixed_desc_unique", "Mixed_lanDesc", "Mixed_lanFac", "Mixed_lanFunctor", "Mixed_lanSolve",
    "Mixed_lanTail", "Mixed_lanUniq", "Mixed_lanUnit", "Mixed_recordFirst", "Mixed_recordSecond", "Original",
    "OtherChosen", "OtherCocone", "OtherComponents", "OtherMediator", "Run_Base_Base_Composite_Functor",
    "Run_Base_Base_Composite_eqCongr", "Run_Base_Base_Composite_functorMap", "Run_Base_Base_Composite_functorMapComp",
    "Run_Base_Base_Composite_functorMapId", "Run_Base_Base_Composite_functorObj", "Run_Base_Base_First_Functor",
    "Run_Base_Base_First_eqCongr", "Run_Base_Base_First_functorMap", "Run_Base_Base_First_functorMapComp",
    "Run_Base_Base_First_functorMapId", "Run_Base_Base_First_functorObj", "Run_Base_Base_Middle_Category",
    "Run_Base_Base_Middle_Hom", "Run_Base_Base_Middle_assoc", "Run_Base_Base_Middle_comp",
    "Run_Base_Base_Middle_compId", "Run_Base_Base_Middle_eqCongr", "Run_Base_Base_Middle_eqTrans",
    "Run_Base_Base_Middle_id", "Run_Base_Base_Middle_idComp", "Run_Base_Base_Second_Functor",
    "Run_Base_Base_Second_eqCongr", "Run_Base_Base_Second_functorMap", "Run_Base_Base_Second_functorMapComp",
    "Run_Base_Base_Second_functorMapId", "Run_Base_Base_Second_functorObj", "Run_Base_Base_Source_Category",
    "Run_Base_Base_Source_Hom", "Run_Base_Base_Source_assoc", "Run_Base_Base_Source_comp",
    "Run_Base_Base_Source_compId", "Run_Base_Base_Source_eqCongr", "Run_Base_Base_Source_eqTrans",
    "Run_Base_Base_Source_id", "Run_Base_Base_Source_idComp", "Run_Base_Base_Target_Category",
    "Run_Base_Base_Target_Hom", "Run_Base_Base_Target_assoc", "Run_Base_Base_Target_comp",
    "Run_Base_Base_Target_compId", "Run_Base_Base_Target_eqCongr", "Run_Base_Base_Target_eqTrans",
    "Run_Base_Base_Target_id", "Run_Base_Base_Target_idComp", "Run_Base_Base_compFunctor",
    "Run_Base_Composite_NatTrans", "Run_Base_Composite_natApp", "Run_Base_Composite_naturality",
    "Run_Base_First_NatTrans", "Run_Base_First_natApp", "Run_Base_First_naturality", "Run_Base_Second_NatTrans",
    "Run_Base_Second_natApp", "Run_Base_Second_naturality", "Run_Base_targetEqSymm", "Run_Base_whiskerLeft",
    "Run_Base_whiskerRight", "Run_LanCocone", "Run_LanFactor", "Run_LanSolution", "Run_LanTail",
    "Run_LeftKanExtension", "Run_coconeFirst", "Run_coconeSecond", "Run_desc_unique", "Run_lanDesc", "Run_lanFac",
    "Run_lanFunctor", "Run_lanSolve", "Run_lanTail", "Run_lanUniq", "Run_lanUnit", "Run_recordFirst",
    "Run_recordSecond", "WideCoconeContract", "WideCoconeSort", "WideExtensionSort", "WideFactorContract",
    "WideMiddleHom", "WideSolutionSort", "WideSourceHom", "WideTailSort", "WideTargetHom", "WideUniqueContract",
    "Wide_Base_Base_Composite_Functor", "Wide_Base_Base_Composite_eqCongr", "Wide_Base_Base_Composite_functorMap",
    "Wide_Base_Base_Composite_functorMapComp", "Wide_Base_Base_Composite_functorMapId",
    "Wide_Base_Base_Composite_functorObj", "Wide_Base_Base_First_Functor", "Wide_Base_Base_First_eqCongr",
    "Wide_Base_Base_First_functorMap", "Wide_Base_Base_First_functorMapComp", "Wide_Base_Base_First_functorMapId",
    "Wide_Base_Base_First_functorObj", "Wide_Base_Base_Middle_Category", "Wide_Base_Base_Middle_Hom",
    "Wide_Base_Base_Middle_assoc", "Wide_Base_Base_Middle_comp", "Wide_Base_Base_Middle_compId",
    "Wide_Base_Base_Middle_eqCongr", "Wide_Base_Base_Middle_eqTrans", "Wide_Base_Base_Middle_id",
    "Wide_Base_Base_Middle_idComp", "Wide_Base_Base_Second_Functor", "Wide_Base_Base_Second_eqCongr",
    "Wide_Base_Base_Second_functorMap", "Wide_Base_Base_Second_functorMapComp", "Wide_Base_Base_Second_functorMapId",
    "Wide_Base_Base_Second_functorObj", "Wide_Base_Base_Source_Category", "Wide_Base_Base_Source_Hom",
    "Wide_Base_Base_Source_assoc", "Wide_Base_Base_Source_comp", "Wide_Base_Base_Source_compId",
    "Wide_Base_Base_Source_eqCongr", "Wide_Base_Base_Source_eqTrans", "Wide_Base_Base_Source_id",
    "Wide_Base_Base_Source_idComp", "Wide_Base_Base_Target_Category", "Wide_Base_Base_Target_Hom",
    "Wide_Base_Base_Target_assoc", "Wide_Base_Base_Target_comp", "Wide_Base_Base_Target_compId",
    "Wide_Base_Base_Target_eqCongr", "Wide_Base_Base_Target_eqTrans", "Wide_Base_Base_Target_id",
    "Wide_Base_Base_Target_idComp", "Wide_Base_Base_compFunctor", "Wide_Base_Composite_NatTrans",
    "Wide_Base_Composite_natApp", "Wide_Base_Composite_naturality", "Wide_Base_First_NatTrans",
    "Wide_Base_First_natApp", "Wide_Base_First_naturality", "Wide_Base_Second_NatTrans", "Wide_Base_Second_natApp",
    "Wide_Base_Second_naturality", "Wide_Base_targetEqSymm", "Wide_Base_whiskerLeft", "Wide_Base_whiskerRight",
    "Wide_LanCocone", "Wide_LanFactor", "Wide_LanSolution", "Wide_LanTail", "Wide_LeftKanExtension",
    "Wide_coconeFirst", "Wide_coconeSecond", "Wide_desc_unique", "Wide_lanDesc", "Wide_lanFac", "Wide_lanFunctor",
    "Wide_lanSolve", "Wide_lanTail", "Wide_lanUniq", "Wide_lanUnit", "Wide_recordFirst", "Wide_recordSecond",
    "componentLaw", "factorWitness", "forgetLan", "forgetUnit", "heterogeneousLanInput", "lanDescFirst",
    "lanDescSecond", "lanMapResult", "lanUnitResult", "otherComponentLaw", "pathTrans", "uniqueWitness"
};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "lanUnitResult", relational_natural(heap, 38));
  computation(heap, &out, "lanMapResult", relational_natural(heap, 40));
  computation(heap, &out, "lanDescFirst", relational_natural(heap, 42));
  computation(heap, &out, "lanDescSecond", relational_natural(heap, 50));
  return out;
}

/* The R.read_all path list of source in suite() */
static const char *const SOURCE_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/composable-functors.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/heterogeneous-left-kan.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/heterogeneous-left-kan.mech", "test/fixtures/prelude/heterogeneous-left-kan-runtime.mech"
};

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str source = str_empty();
  Vec source_paths = STRINGS(heap, SOURCE_PATHS);
  Str fixture = str_empty();
  Vec fixture_paths = STRINGS(heap, FIXTURE_PATHS);
  const Vec *parsed = NULL;
  const ElabChecked *checked = NULL;
  Globals globals;
  Vec entries;
  Vec families;
  Vec rows;
  Vec negatives;
  RELATIONAL_TRY(relational_read_all(heap, root, &source_paths, &source));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, source, fixture), &parsed));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  globals = relational_globals(checked);
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  RELATIONAL_TRY(program_global_inventory(heap, globals, &entries));
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/heterogeneous-left-kan"), &negatives, globals,
                               negatives_prefix(9, true, false)));
  /* D-353a (host only, the D-352a form): Bend checks this source with check_in under Budget.unlimited.
   * The compact entry is the same check under budget_unlimited (decision 156, D-254a), but its work
   * heaps run each conversion query in a scratch arena (D-261a), so the refusal no longer keeps the
   * garbage of the whole elaboration in the caller arena. The refusal value and its text do not change. */
  RELATIONAL_TRY(relational_refuse_exact(
      heap,
      elab_check_in_compact(heap, global_empty(), cat(heap, source, str_lit("specialize MechHeterogeneousLeftKan (0, 1, 2, 3, 4) as Bad\n"))),
      str_lit("universe: the family schema MechHeterogeneousLeftKan expects 6 universe arguments, got 5\n"), str_lit("universe arity")));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=294 instances=3 computations=4 negatives=15");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-HETEROGENEOUS-LEFT-KAN", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-HETEROGENEOUS-LEFT-KAN"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
