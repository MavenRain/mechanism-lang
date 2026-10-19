/* Port of tests/prelude_shared_nattrans.bend (41 lines): the shared nattrans prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-SHARED-NATTRANS-OK fields or PRELUDE-SHARED-NATTRANS-FAIL message. Same definitions in the same order. */
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

/* expected_entries() (tests/prelude_shared_nattrans.bend line 12, 591 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "Constant", "End", "External_Functor", "External_Source_Category", "External_Source_Hom",
    "External_Source_assoc", "External_Source_comp", "External_Source_compId", "External_Source_eqCongr",
    "External_Source_eqTrans", "External_Source_id", "External_Source_idComp", "External_Target_Category",
    "External_Target_Hom", "External_Target_assoc", "External_Target_comp", "External_Target_compId",
    "External_Target_eqCongr", "External_Target_eqTrans", "External_Target_id", "External_Target_idComp",
    "External_eqCongr", "External_functorMap", "External_functorMapComp", "External_functorMapId",
    "External_functorObj", "FH", "GI", "Gamma", "Horizontal", "Identity", "IndexedEnd", "Left",
    "Mixed_Compose_Base_Composite_Functor", "Mixed_Compose_Base_Composite_eqCongr",
    "Mixed_Compose_Base_Composite_functorMap", "Mixed_Compose_Base_Composite_functorMapComp",
    "Mixed_Compose_Base_Composite_functorMapId", "Mixed_Compose_Base_Composite_functorObj",
    "Mixed_Compose_Base_First_Functor", "Mixed_Compose_Base_First_eqCongr", "Mixed_Compose_Base_First_functorMap",
    "Mixed_Compose_Base_First_functorMapComp", "Mixed_Compose_Base_First_functorMapId",
    "Mixed_Compose_Base_First_functorObj", "Mixed_Compose_Base_Middle_Category", "Mixed_Compose_Base_Middle_Hom",
    "Mixed_Compose_Base_Middle_assoc", "Mixed_Compose_Base_Middle_comp", "Mixed_Compose_Base_Middle_compId",
    "Mixed_Compose_Base_Middle_eqCongr", "Mixed_Compose_Base_Middle_eqTrans", "Mixed_Compose_Base_Middle_id",
    "Mixed_Compose_Base_Middle_idComp", "Mixed_Compose_Base_Second_Functor", "Mixed_Compose_Base_Second_eqCongr",
    "Mixed_Compose_Base_Second_functorMap", "Mixed_Compose_Base_Second_functorMapComp",
    "Mixed_Compose_Base_Second_functorMapId", "Mixed_Compose_Base_Second_functorObj",
    "Mixed_Compose_Base_Source_Category", "Mixed_Compose_Base_Source_Hom", "Mixed_Compose_Base_Source_assoc",
    "Mixed_Compose_Base_Source_comp", "Mixed_Compose_Base_Source_compId", "Mixed_Compose_Base_Source_eqCongr",
    "Mixed_Compose_Base_Source_eqTrans", "Mixed_Compose_Base_Source_id", "Mixed_Compose_Base_Source_idComp",
    "Mixed_Compose_Base_Target_Category", "Mixed_Compose_Base_Target_Hom", "Mixed_Compose_Base_Target_assoc",
    "Mixed_Compose_Base_Target_comp", "Mixed_Compose_Base_Target_compId", "Mixed_Compose_Base_Target_eqCongr",
    "Mixed_Compose_Base_Target_eqTrans", "Mixed_Compose_Base_Target_id", "Mixed_Compose_Base_Target_idComp",
    "Mixed_Compose_Base_compFunctor", "Mixed_Compose_Composite_NatTrans", "Mixed_Compose_Composite_natApp",
    "Mixed_Compose_Composite_naturality", "Mixed_Compose_First_NatTrans", "Mixed_Compose_First_natApp",
    "Mixed_Compose_First_naturality", "Mixed_Compose_Second_NatTrans", "Mixed_Compose_Second_natApp",
    "Mixed_Compose_Second_naturality", "Mixed_Compose_targetEqSymm", "Mixed_Compose_whiskerLeft",
    "Mixed_Compose_whiskerRight", "Mixed_Composite_Base_Functor", "Mixed_Composite_Base_Source_Category",
    "Mixed_Composite_Base_Source_Hom", "Mixed_Composite_Base_Source_assoc", "Mixed_Composite_Base_Source_comp",
    "Mixed_Composite_Base_Source_compId", "Mixed_Composite_Base_Source_eqCongr",
    "Mixed_Composite_Base_Source_eqTrans", "Mixed_Composite_Base_Source_id", "Mixed_Composite_Base_Source_idComp",
    "Mixed_Composite_Base_Target_Category", "Mixed_Composite_Base_Target_Hom", "Mixed_Composite_Base_Target_assoc",
    "Mixed_Composite_Base_Target_comp", "Mixed_Composite_Base_Target_compId", "Mixed_Composite_Base_Target_eqCongr",
    "Mixed_Composite_Base_Target_eqTrans", "Mixed_Composite_Base_Target_id", "Mixed_Composite_Base_Target_idComp",
    "Mixed_Composite_Base_eqCongr", "Mixed_Composite_Base_functorMap", "Mixed_Composite_Base_functorMapComp",
    "Mixed_Composite_Base_functorMapId", "Mixed_Composite_Base_functorObj", "Mixed_Composite_NatTrans",
    "Mixed_Composite_idNat", "Mixed_Composite_natApp", "Mixed_Composite_naturality", "Mixed_Composite_targetEqSymm",
    "Mixed_Composite_vcomp", "Mixed_Composite_vcompLaw", "Mixed_First_Base_Functor",
    "Mixed_First_Base_Source_Category", "Mixed_First_Base_Source_Hom", "Mixed_First_Base_Source_assoc",
    "Mixed_First_Base_Source_comp", "Mixed_First_Base_Source_compId", "Mixed_First_Base_Source_eqCongr",
    "Mixed_First_Base_Source_eqTrans", "Mixed_First_Base_Source_id", "Mixed_First_Base_Source_idComp",
    "Mixed_First_Base_Target_Category", "Mixed_First_Base_Target_Hom", "Mixed_First_Base_Target_assoc",
    "Mixed_First_Base_Target_comp", "Mixed_First_Base_Target_compId", "Mixed_First_Base_Target_eqCongr",
    "Mixed_First_Base_Target_eqTrans", "Mixed_First_Base_Target_id", "Mixed_First_Base_Target_idComp",
    "Mixed_First_Base_eqCongr", "Mixed_First_Base_functorMap", "Mixed_First_Base_functorMapComp",
    "Mixed_First_Base_functorMapId", "Mixed_First_Base_functorObj", "Mixed_First_NatTrans", "Mixed_First_idNat",
    "Mixed_First_natApp", "Mixed_First_naturality", "Mixed_First_targetEqSymm", "Mixed_First_vcomp",
    "Mixed_First_vcompLaw", "Mixed_Middle_Category", "Mixed_Middle_Hom", "Mixed_Middle_assoc", "Mixed_Middle_comp",
    "Mixed_Middle_compId", "Mixed_Middle_eqCongr", "Mixed_Middle_eqTrans", "Mixed_Middle_id", "Mixed_Middle_idComp",
    "Mixed_Second_Base_Functor", "Mixed_Second_Base_Source_Category", "Mixed_Second_Base_Source_Hom",
    "Mixed_Second_Base_Source_assoc", "Mixed_Second_Base_Source_comp", "Mixed_Second_Base_Source_compId",
    "Mixed_Second_Base_Source_eqCongr", "Mixed_Second_Base_Source_eqTrans", "Mixed_Second_Base_Source_id",
    "Mixed_Second_Base_Source_idComp", "Mixed_Second_Base_Target_Category", "Mixed_Second_Base_Target_Hom",
    "Mixed_Second_Base_Target_assoc", "Mixed_Second_Base_Target_comp", "Mixed_Second_Base_Target_compId",
    "Mixed_Second_Base_Target_eqCongr", "Mixed_Second_Base_Target_eqTrans", "Mixed_Second_Base_Target_id",
    "Mixed_Second_Base_Target_idComp", "Mixed_Second_Base_eqCongr", "Mixed_Second_Base_functorMap",
    "Mixed_Second_Base_functorMapComp", "Mixed_Second_Base_functorMapId", "Mixed_Second_Base_functorObj",
    "Mixed_Second_NatTrans", "Mixed_Second_idNat", "Mixed_Second_natApp", "Mixed_Second_naturality",
    "Mixed_Second_targetEqSymm", "Mixed_Second_vcomp", "Mixed_Second_vcompLaw", "Mixed_Source_Category",
    "Mixed_Source_Hom", "Mixed_Source_assoc", "Mixed_Source_comp", "Mixed_Source_compId", "Mixed_Source_eqCongr",
    "Mixed_Source_eqTrans", "Mixed_Source_id", "Mixed_Source_idComp", "Mixed_Target_Category", "Mixed_Target_Hom",
    "Mixed_Target_assoc", "Mixed_Target_comp", "Mixed_Target_compId", "Mixed_Target_eqCongr", "Mixed_Target_eqTrans",
    "Mixed_Target_id", "Mixed_Target_idComp", "Mixed_hcomp", "PairEnd", "Right", "Run_Compose_Base_Composite_Functor",
    "Run_Compose_Base_Composite_eqCongr", "Run_Compose_Base_Composite_functorMap",
    "Run_Compose_Base_Composite_functorMapComp", "Run_Compose_Base_Composite_functorMapId",
    "Run_Compose_Base_Composite_functorObj", "Run_Compose_Base_First_Functor", "Run_Compose_Base_First_eqCongr",
    "Run_Compose_Base_First_functorMap", "Run_Compose_Base_First_functorMapComp",
    "Run_Compose_Base_First_functorMapId", "Run_Compose_Base_First_functorObj", "Run_Compose_Base_Middle_Category",
    "Run_Compose_Base_Middle_Hom", "Run_Compose_Base_Middle_assoc", "Run_Compose_Base_Middle_comp",
    "Run_Compose_Base_Middle_compId", "Run_Compose_Base_Middle_eqCongr", "Run_Compose_Base_Middle_eqTrans",
    "Run_Compose_Base_Middle_id", "Run_Compose_Base_Middle_idComp", "Run_Compose_Base_Second_Functor",
    "Run_Compose_Base_Second_eqCongr", "Run_Compose_Base_Second_functorMap", "Run_Compose_Base_Second_functorMapComp",
    "Run_Compose_Base_Second_functorMapId", "Run_Compose_Base_Second_functorObj", "Run_Compose_Base_Source_Category",
    "Run_Compose_Base_Source_Hom", "Run_Compose_Base_Source_assoc", "Run_Compose_Base_Source_comp",
    "Run_Compose_Base_Source_compId", "Run_Compose_Base_Source_eqCongr", "Run_Compose_Base_Source_eqTrans",
    "Run_Compose_Base_Source_id", "Run_Compose_Base_Source_idComp", "Run_Compose_Base_Target_Category",
    "Run_Compose_Base_Target_Hom", "Run_Compose_Base_Target_assoc", "Run_Compose_Base_Target_comp",
    "Run_Compose_Base_Target_compId", "Run_Compose_Base_Target_eqCongr", "Run_Compose_Base_Target_eqTrans",
    "Run_Compose_Base_Target_id", "Run_Compose_Base_Target_idComp", "Run_Compose_Base_compFunctor",
    "Run_Compose_Composite_NatTrans", "Run_Compose_Composite_natApp", "Run_Compose_Composite_naturality",
    "Run_Compose_First_NatTrans", "Run_Compose_First_natApp", "Run_Compose_First_naturality",
    "Run_Compose_Second_NatTrans", "Run_Compose_Second_natApp", "Run_Compose_Second_naturality",
    "Run_Compose_targetEqSymm", "Run_Compose_whiskerLeft", "Run_Compose_whiskerRight", "Run_Composite_Base_Functor",
    "Run_Composite_Base_Source_Category", "Run_Composite_Base_Source_Hom", "Run_Composite_Base_Source_assoc",
    "Run_Composite_Base_Source_comp", "Run_Composite_Base_Source_compId", "Run_Composite_Base_Source_eqCongr",
    "Run_Composite_Base_Source_eqTrans", "Run_Composite_Base_Source_id", "Run_Composite_Base_Source_idComp",
    "Run_Composite_Base_Target_Category", "Run_Composite_Base_Target_Hom", "Run_Composite_Base_Target_assoc",
    "Run_Composite_Base_Target_comp", "Run_Composite_Base_Target_compId", "Run_Composite_Base_Target_eqCongr",
    "Run_Composite_Base_Target_eqTrans", "Run_Composite_Base_Target_id", "Run_Composite_Base_Target_idComp",
    "Run_Composite_Base_eqCongr", "Run_Composite_Base_functorMap", "Run_Composite_Base_functorMapComp",
    "Run_Composite_Base_functorMapId", "Run_Composite_Base_functorObj", "Run_Composite_NatTrans",
    "Run_Composite_idNat", "Run_Composite_natApp", "Run_Composite_naturality", "Run_Composite_targetEqSymm",
    "Run_Composite_vcomp", "Run_Composite_vcompLaw", "Run_First_Base_Functor", "Run_First_Base_Source_Category",
    "Run_First_Base_Source_Hom", "Run_First_Base_Source_assoc", "Run_First_Base_Source_comp",
    "Run_First_Base_Source_compId", "Run_First_Base_Source_eqCongr", "Run_First_Base_Source_eqTrans",
    "Run_First_Base_Source_id", "Run_First_Base_Source_idComp", "Run_First_Base_Target_Category",
    "Run_First_Base_Target_Hom", "Run_First_Base_Target_assoc", "Run_First_Base_Target_comp",
    "Run_First_Base_Target_compId", "Run_First_Base_Target_eqCongr", "Run_First_Base_Target_eqTrans",
    "Run_First_Base_Target_id", "Run_First_Base_Target_idComp", "Run_First_Base_eqCongr", "Run_First_Base_functorMap",
    "Run_First_Base_functorMapComp", "Run_First_Base_functorMapId", "Run_First_Base_functorObj", "Run_First_NatTrans",
    "Run_First_idNat", "Run_First_natApp", "Run_First_naturality", "Run_First_targetEqSymm", "Run_First_vcomp",
    "Run_First_vcompLaw", "Run_Middle_Category", "Run_Middle_Hom", "Run_Middle_assoc", "Run_Middle_comp",
    "Run_Middle_compId", "Run_Middle_eqCongr", "Run_Middle_eqTrans", "Run_Middle_id", "Run_Middle_idComp",
    "Run_Second_Base_Functor", "Run_Second_Base_Source_Category", "Run_Second_Base_Source_Hom",
    "Run_Second_Base_Source_assoc", "Run_Second_Base_Source_comp", "Run_Second_Base_Source_compId",
    "Run_Second_Base_Source_eqCongr", "Run_Second_Base_Source_eqTrans", "Run_Second_Base_Source_id",
    "Run_Second_Base_Source_idComp", "Run_Second_Base_Target_Category", "Run_Second_Base_Target_Hom",
    "Run_Second_Base_Target_assoc", "Run_Second_Base_Target_comp", "Run_Second_Base_Target_compId",
    "Run_Second_Base_Target_eqCongr", "Run_Second_Base_Target_eqTrans", "Run_Second_Base_Target_id",
    "Run_Second_Base_Target_idComp", "Run_Second_Base_eqCongr", "Run_Second_Base_functorMap",
    "Run_Second_Base_functorMapComp", "Run_Second_Base_functorMapId", "Run_Second_Base_functorObj",
    "Run_Second_NatTrans", "Run_Second_idNat", "Run_Second_natApp", "Run_Second_naturality",
    "Run_Second_targetEqSymm", "Run_Second_vcomp", "Run_Second_vcompLaw", "Run_Source_Category", "Run_Source_Hom",
    "Run_Source_assoc", "Run_Source_comp", "Run_Source_compId", "Run_Source_eqCongr", "Run_Source_eqTrans",
    "Run_Source_id", "Run_Source_idComp", "Run_Target_Category", "Run_Target_Hom", "Run_Target_assoc",
    "Run_Target_comp", "Run_Target_compId", "Run_Target_eqCongr", "Run_Target_eqTrans", "Run_Target_id",
    "Run_Target_idComp", "Run_hcomp", "Vertical", "Wide_Compose_Base_Composite_Functor",
    "Wide_Compose_Base_Composite_eqCongr", "Wide_Compose_Base_Composite_functorMap",
    "Wide_Compose_Base_Composite_functorMapComp", "Wide_Compose_Base_Composite_functorMapId",
    "Wide_Compose_Base_Composite_functorObj", "Wide_Compose_Base_First_Functor", "Wide_Compose_Base_First_eqCongr",
    "Wide_Compose_Base_First_functorMap", "Wide_Compose_Base_First_functorMapComp",
    "Wide_Compose_Base_First_functorMapId", "Wide_Compose_Base_First_functorObj", "Wide_Compose_Base_Middle_Category",
    "Wide_Compose_Base_Middle_Hom", "Wide_Compose_Base_Middle_assoc", "Wide_Compose_Base_Middle_comp",
    "Wide_Compose_Base_Middle_compId", "Wide_Compose_Base_Middle_eqCongr", "Wide_Compose_Base_Middle_eqTrans",
    "Wide_Compose_Base_Middle_id", "Wide_Compose_Base_Middle_idComp", "Wide_Compose_Base_Second_Functor",
    "Wide_Compose_Base_Second_eqCongr", "Wide_Compose_Base_Second_functorMap",
    "Wide_Compose_Base_Second_functorMapComp", "Wide_Compose_Base_Second_functorMapId",
    "Wide_Compose_Base_Second_functorObj", "Wide_Compose_Base_Source_Category", "Wide_Compose_Base_Source_Hom",
    "Wide_Compose_Base_Source_assoc", "Wide_Compose_Base_Source_comp", "Wide_Compose_Base_Source_compId",
    "Wide_Compose_Base_Source_eqCongr", "Wide_Compose_Base_Source_eqTrans", "Wide_Compose_Base_Source_id",
    "Wide_Compose_Base_Source_idComp", "Wide_Compose_Base_Target_Category", "Wide_Compose_Base_Target_Hom",
    "Wide_Compose_Base_Target_assoc", "Wide_Compose_Base_Target_comp", "Wide_Compose_Base_Target_compId",
    "Wide_Compose_Base_Target_eqCongr", "Wide_Compose_Base_Target_eqTrans", "Wide_Compose_Base_Target_id",
    "Wide_Compose_Base_Target_idComp", "Wide_Compose_Base_compFunctor", "Wide_Compose_Composite_NatTrans",
    "Wide_Compose_Composite_natApp", "Wide_Compose_Composite_naturality", "Wide_Compose_First_NatTrans",
    "Wide_Compose_First_natApp", "Wide_Compose_First_naturality", "Wide_Compose_Second_NatTrans",
    "Wide_Compose_Second_natApp", "Wide_Compose_Second_naturality", "Wide_Compose_targetEqSymm",
    "Wide_Compose_whiskerLeft", "Wide_Compose_whiskerRight", "Wide_Composite_Base_Functor",
    "Wide_Composite_Base_Source_Category", "Wide_Composite_Base_Source_Hom", "Wide_Composite_Base_Source_assoc",
    "Wide_Composite_Base_Source_comp", "Wide_Composite_Base_Source_compId", "Wide_Composite_Base_Source_eqCongr",
    "Wide_Composite_Base_Source_eqTrans", "Wide_Composite_Base_Source_id", "Wide_Composite_Base_Source_idComp",
    "Wide_Composite_Base_Target_Category", "Wide_Composite_Base_Target_Hom", "Wide_Composite_Base_Target_assoc",
    "Wide_Composite_Base_Target_comp", "Wide_Composite_Base_Target_compId", "Wide_Composite_Base_Target_eqCongr",
    "Wide_Composite_Base_Target_eqTrans", "Wide_Composite_Base_Target_id", "Wide_Composite_Base_Target_idComp",
    "Wide_Composite_Base_eqCongr", "Wide_Composite_Base_functorMap", "Wide_Composite_Base_functorMapComp",
    "Wide_Composite_Base_functorMapId", "Wide_Composite_Base_functorObj", "Wide_Composite_NatTrans",
    "Wide_Composite_idNat", "Wide_Composite_natApp", "Wide_Composite_naturality", "Wide_Composite_targetEqSymm",
    "Wide_Composite_vcomp", "Wide_Composite_vcompLaw", "Wide_First_Base_Functor", "Wide_First_Base_Source_Category",
    "Wide_First_Base_Source_Hom", "Wide_First_Base_Source_assoc", "Wide_First_Base_Source_comp",
    "Wide_First_Base_Source_compId", "Wide_First_Base_Source_eqCongr", "Wide_First_Base_Source_eqTrans",
    "Wide_First_Base_Source_id", "Wide_First_Base_Source_idComp", "Wide_First_Base_Target_Category",
    "Wide_First_Base_Target_Hom", "Wide_First_Base_Target_assoc", "Wide_First_Base_Target_comp",
    "Wide_First_Base_Target_compId", "Wide_First_Base_Target_eqCongr", "Wide_First_Base_Target_eqTrans",
    "Wide_First_Base_Target_id", "Wide_First_Base_Target_idComp", "Wide_First_Base_eqCongr",
    "Wide_First_Base_functorMap", "Wide_First_Base_functorMapComp", "Wide_First_Base_functorMapId",
    "Wide_First_Base_functorObj", "Wide_First_NatTrans", "Wide_First_idNat", "Wide_First_natApp",
    "Wide_First_naturality", "Wide_First_targetEqSymm", "Wide_First_vcomp", "Wide_First_vcompLaw",
    "Wide_Middle_Category", "Wide_Middle_Hom", "Wide_Middle_assoc", "Wide_Middle_comp", "Wide_Middle_compId",
    "Wide_Middle_eqCongr", "Wide_Middle_eqTrans", "Wide_Middle_id", "Wide_Middle_idComp", "Wide_Second_Base_Functor",
    "Wide_Second_Base_Source_Category", "Wide_Second_Base_Source_Hom", "Wide_Second_Base_Source_assoc",
    "Wide_Second_Base_Source_comp", "Wide_Second_Base_Source_compId", "Wide_Second_Base_Source_eqCongr",
    "Wide_Second_Base_Source_eqTrans", "Wide_Second_Base_Source_id", "Wide_Second_Base_Source_idComp",
    "Wide_Second_Base_Target_Category", "Wide_Second_Base_Target_Hom", "Wide_Second_Base_Target_assoc",
    "Wide_Second_Base_Target_comp", "Wide_Second_Base_Target_compId", "Wide_Second_Base_Target_eqCongr",
    "Wide_Second_Base_Target_eqTrans", "Wide_Second_Base_Target_id", "Wide_Second_Base_Target_idComp",
    "Wide_Second_Base_eqCongr", "Wide_Second_Base_functorMap", "Wide_Second_Base_functorMapComp",
    "Wide_Second_Base_functorMapId", "Wide_Second_Base_functorObj", "Wide_Second_NatTrans", "Wide_Second_idNat",
    "Wide_Second_natApp", "Wide_Second_naturality", "Wide_Second_targetEqSymm", "Wide_Second_vcomp",
    "Wide_Second_vcompLaw", "Wide_Source_Category", "Wide_Source_Hom", "Wide_Source_assoc", "Wide_Source_comp",
    "Wide_Source_compId", "Wide_Source_eqCongr", "Wide_Source_eqTrans", "Wide_Source_id", "Wide_Source_idComp",
    "Wide_Target_Category", "Wide_Target_Hom", "Wide_Target_assoc", "Wide_Target_comp", "Wide_Target_compId",
    "Wide_Target_eqCongr", "Wide_Target_eqTrans", "Wide_Target_id", "Wide_Target_idComp", "Wide_hcomp",
    "componentLaw", "components", "horizontalFirst", "horizontalSecond", "identityValue", "mixedCompositeSort",
    "mixedFirstSort", "mixedHorizontal", "mixedLeft", "mixedRight", "mixedSecondSort", "naturalityWitness",
    "pathTrans", "sharedNatInput", "verticalValue", "wideHorizontal"
};

/* expected_families() (tests/prelude_shared_nattrans.bend line 15, 11 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "Mixed_Middle", "Mixed_Source", "Mixed_Target", "Path", "Point", "Run_Middle", "Run_Source", "Run_Target",
    "Wide_Middle", "Wide_Source", "Wide_Target"
};

/* negatives() (tests/prelude_shared_nattrans.bend line 18, 6 names) */
static const char *const NEGATIVES[] = {
    "horizontal-endpoint", "vertical-middle", "missing-naturality", "nominal-equality", "first-sort",
    "erased-component"
};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "horizontalFirst", relational_natural(heap, 37));
  computation(heap, &out, "horizontalSecond", relational_natural(heap, 43));
  computation(heap, &out, "verticalValue", relational_natural(heap, 48));
  computation(heap, &out, "identityValue", relational_natural(heap, 37));
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/shared-nattrans.mech", "test/fixtures/prelude/shared-nattrans-runtime.mech"
};

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str templates = str_empty();
  Vec templates_paths = STRINGS(heap, TEMPLATES_PATHS);
  Str fixture = str_empty();
  Vec fixture_paths = STRINGS(heap, FIXTURE_PATHS);
  const Vec *parsed = NULL;
  const ElabChecked *checked = NULL;
  Vec entries;
  Vec families;
  Globals globals;
  Vec rows;
  Vec negatives;
  RELATIONAL_TRY(relational_read_all(heap, root, &templates_paths, &templates));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, cat(heap, templates, str_lit("\n")), fixture), &parsed));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  globals = relational_globals(checked);
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/shared-nattrans"), &negatives, globals,
                               negatives_prefix(20, false, false)));
  *fields = str_lit("entries=591 families=11 computations=4 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-SHARED-NATTRANS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-SHARED-NATTRANS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
