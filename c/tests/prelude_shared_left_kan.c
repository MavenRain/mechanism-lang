/* Port of bend2/tests/prelude_shared_left_kan.bend (41 lines): the shared left kan prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-SHARED-LEFT-KAN-OK fields or PRELUDE-SHARED-LEFT-KAN-FAIL message. Same definitions in the same order. */
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

/* expected_entries() (bend2/tests/prelude_shared_left_kan.bend line 12, 944 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "Chosen", "Cocone", "Components", "ComposedMediator", "Constant", "DoubleEnd",
    "EqualMiddle_Category", "EqualMiddle_Hom", "EqualMiddle_assoc", "EqualMiddle_comp", "EqualMiddle_compId",
    "EqualMiddle_eqCongr", "EqualMiddle_eqTrans", "EqualMiddle_id", "EqualMiddle_idComp", "EqualSource_Category",
    "EqualSource_Hom", "EqualSource_assoc", "EqualSource_comp", "EqualSource_compId", "EqualSource_eqCongr",
    "EqualSource_eqTrans", "EqualSource_id", "EqualSource_idComp", "Eta", "Extension", "ExternalComposite_Functor",
    "ExternalComposite_Source_Category", "ExternalComposite_Source_Hom", "ExternalComposite_Source_assoc",
    "ExternalComposite_Source_comp", "ExternalComposite_Source_compId", "ExternalComposite_Source_eqCongr",
    "ExternalComposite_Source_eqTrans", "ExternalComposite_Source_id", "ExternalComposite_Source_idComp",
    "ExternalComposite_Target_Category", "ExternalComposite_Target_Hom", "ExternalComposite_Target_assoc",
    "ExternalComposite_Target_comp", "ExternalComposite_Target_compId", "ExternalComposite_Target_eqCongr",
    "ExternalComposite_Target_eqTrans", "ExternalComposite_Target_id", "ExternalComposite_Target_idComp",
    "ExternalComposite_eqCongr", "ExternalComposite_functorMap", "ExternalComposite_functorMapComp",
    "ExternalComposite_functorMapId", "ExternalComposite_functorObj", "ExternalFirst_Functor",
    "ExternalFirst_Source_Category", "ExternalFirst_Source_Hom", "ExternalFirst_Source_assoc",
    "ExternalFirst_Source_comp", "ExternalFirst_Source_compId", "ExternalFirst_Source_eqCongr",
    "ExternalFirst_Source_eqTrans", "ExternalFirst_Source_id", "ExternalFirst_Source_idComp",
    "ExternalFirst_Target_Category", "ExternalFirst_Target_Hom", "ExternalFirst_Target_assoc",
    "ExternalFirst_Target_comp", "ExternalFirst_Target_compId", "ExternalFirst_Target_eqCongr",
    "ExternalFirst_Target_eqTrans", "ExternalFirst_Target_id", "ExternalFirst_Target_idComp", "ExternalFirst_eqCongr",
    "ExternalFirst_functorMap", "ExternalFirst_functorMapComp", "ExternalFirst_functorMapId",
    "ExternalFirst_functorObj", "ExternalNat_Base_Functor", "ExternalNat_Base_Source_Category",
    "ExternalNat_Base_Source_Hom", "ExternalNat_Base_Source_assoc", "ExternalNat_Base_Source_comp",
    "ExternalNat_Base_Source_compId", "ExternalNat_Base_Source_eqCongr", "ExternalNat_Base_Source_eqTrans",
    "ExternalNat_Base_Source_id", "ExternalNat_Base_Source_idComp", "ExternalNat_Base_Target_Category",
    "ExternalNat_Base_Target_Hom", "ExternalNat_Base_Target_assoc", "ExternalNat_Base_Target_comp",
    "ExternalNat_Base_Target_compId", "ExternalNat_Base_Target_eqCongr", "ExternalNat_Base_Target_eqTrans",
    "ExternalNat_Base_Target_id", "ExternalNat_Base_Target_idComp", "ExternalNat_Base_eqCongr",
    "ExternalNat_Base_functorMap", "ExternalNat_Base_functorMapComp", "ExternalNat_Base_functorMapId",
    "ExternalNat_Base_functorObj", "ExternalNat_NatTrans", "ExternalNat_idNat", "ExternalNat_natApp",
    "ExternalNat_naturality", "ExternalNat_targetEqSymm", "ExternalNat_vcomp", "ExternalNat_vcompLaw",
    "ExternalSecond_Functor", "ExternalSecond_Source_Category", "ExternalSecond_Source_Hom",
    "ExternalSecond_Source_assoc", "ExternalSecond_Source_comp", "ExternalSecond_Source_compId",
    "ExternalSecond_Source_eqCongr", "ExternalSecond_Source_eqTrans", "ExternalSecond_Source_id",
    "ExternalSecond_Source_idComp", "ExternalSecond_Target_Category", "ExternalSecond_Target_Hom",
    "ExternalSecond_Target_assoc", "ExternalSecond_Target_comp", "ExternalSecond_Target_compId",
    "ExternalSecond_Target_eqCongr", "ExternalSecond_Target_eqTrans", "ExternalSecond_Target_id",
    "ExternalSecond_Target_idComp", "ExternalSecond_eqCongr", "ExternalSecond_functorMap",
    "ExternalSecond_functorMapComp", "ExternalSecond_functorMapId", "ExternalSecond_functorObj", "Forget",
    "IndexedEnd", "IndexedPlain", "Lift", "Mediator", "MixedCoconeContract", "MixedCoconeSort", "MixedExtensionSort",
    "MixedFactorContract", "MixedMiddleHom", "MixedSolutionSort", "MixedSourceHom", "MixedTailSort", "MixedTargetHom",
    "MixedUniqueContract", "Mixed_Base_Compose_Base_Composite_Functor", "Mixed_Base_Compose_Base_Composite_eqCongr",
    "Mixed_Base_Compose_Base_Composite_functorMap", "Mixed_Base_Compose_Base_Composite_functorMapComp",
    "Mixed_Base_Compose_Base_Composite_functorMapId", "Mixed_Base_Compose_Base_Composite_functorObj",
    "Mixed_Base_Compose_Base_First_Functor", "Mixed_Base_Compose_Base_First_eqCongr",
    "Mixed_Base_Compose_Base_First_functorMap", "Mixed_Base_Compose_Base_First_functorMapComp",
    "Mixed_Base_Compose_Base_First_functorMapId", "Mixed_Base_Compose_Base_First_functorObj",
    "Mixed_Base_Compose_Base_Middle_Category", "Mixed_Base_Compose_Base_Middle_Hom",
    "Mixed_Base_Compose_Base_Middle_assoc", "Mixed_Base_Compose_Base_Middle_comp",
    "Mixed_Base_Compose_Base_Middle_compId", "Mixed_Base_Compose_Base_Middle_eqCongr",
    "Mixed_Base_Compose_Base_Middle_eqTrans", "Mixed_Base_Compose_Base_Middle_id",
    "Mixed_Base_Compose_Base_Middle_idComp", "Mixed_Base_Compose_Base_Second_Functor",
    "Mixed_Base_Compose_Base_Second_eqCongr", "Mixed_Base_Compose_Base_Second_functorMap",
    "Mixed_Base_Compose_Base_Second_functorMapComp", "Mixed_Base_Compose_Base_Second_functorMapId",
    "Mixed_Base_Compose_Base_Second_functorObj", "Mixed_Base_Compose_Base_Source_Category",
    "Mixed_Base_Compose_Base_Source_Hom", "Mixed_Base_Compose_Base_Source_assoc",
    "Mixed_Base_Compose_Base_Source_comp", "Mixed_Base_Compose_Base_Source_compId",
    "Mixed_Base_Compose_Base_Source_eqCongr", "Mixed_Base_Compose_Base_Source_eqTrans",
    "Mixed_Base_Compose_Base_Source_id", "Mixed_Base_Compose_Base_Source_idComp",
    "Mixed_Base_Compose_Base_Target_Category", "Mixed_Base_Compose_Base_Target_Hom",
    "Mixed_Base_Compose_Base_Target_assoc", "Mixed_Base_Compose_Base_Target_comp",
    "Mixed_Base_Compose_Base_Target_compId", "Mixed_Base_Compose_Base_Target_eqCongr",
    "Mixed_Base_Compose_Base_Target_eqTrans", "Mixed_Base_Compose_Base_Target_id",
    "Mixed_Base_Compose_Base_Target_idComp", "Mixed_Base_Compose_Base_compFunctor",
    "Mixed_Base_Compose_Composite_NatTrans", "Mixed_Base_Compose_Composite_natApp",
    "Mixed_Base_Compose_Composite_naturality", "Mixed_Base_Compose_First_NatTrans", "Mixed_Base_Compose_First_natApp",
    "Mixed_Base_Compose_First_naturality", "Mixed_Base_Compose_Second_NatTrans", "Mixed_Base_Compose_Second_natApp",
    "Mixed_Base_Compose_Second_naturality", "Mixed_Base_Compose_targetEqSymm", "Mixed_Base_Compose_whiskerLeft",
    "Mixed_Base_Compose_whiskerRight", "Mixed_Base_Composite_Base_Functor",
    "Mixed_Base_Composite_Base_Source_Category", "Mixed_Base_Composite_Base_Source_Hom",
    "Mixed_Base_Composite_Base_Source_assoc", "Mixed_Base_Composite_Base_Source_comp",
    "Mixed_Base_Composite_Base_Source_compId", "Mixed_Base_Composite_Base_Source_eqCongr",
    "Mixed_Base_Composite_Base_Source_eqTrans", "Mixed_Base_Composite_Base_Source_id",
    "Mixed_Base_Composite_Base_Source_idComp", "Mixed_Base_Composite_Base_Target_Category",
    "Mixed_Base_Composite_Base_Target_Hom", "Mixed_Base_Composite_Base_Target_assoc",
    "Mixed_Base_Composite_Base_Target_comp", "Mixed_Base_Composite_Base_Target_compId",
    "Mixed_Base_Composite_Base_Target_eqCongr", "Mixed_Base_Composite_Base_Target_eqTrans",
    "Mixed_Base_Composite_Base_Target_id", "Mixed_Base_Composite_Base_Target_idComp",
    "Mixed_Base_Composite_Base_eqCongr", "Mixed_Base_Composite_Base_functorMap",
    "Mixed_Base_Composite_Base_functorMapComp", "Mixed_Base_Composite_Base_functorMapId",
    "Mixed_Base_Composite_Base_functorObj", "Mixed_Base_Composite_NatTrans", "Mixed_Base_Composite_idNat",
    "Mixed_Base_Composite_natApp", "Mixed_Base_Composite_naturality", "Mixed_Base_Composite_targetEqSymm",
    "Mixed_Base_Composite_vcomp", "Mixed_Base_Composite_vcompLaw", "Mixed_Base_First_Base_Functor",
    "Mixed_Base_First_Base_Source_Category", "Mixed_Base_First_Base_Source_Hom", "Mixed_Base_First_Base_Source_assoc",
    "Mixed_Base_First_Base_Source_comp", "Mixed_Base_First_Base_Source_compId",
    "Mixed_Base_First_Base_Source_eqCongr", "Mixed_Base_First_Base_Source_eqTrans", "Mixed_Base_First_Base_Source_id",
    "Mixed_Base_First_Base_Source_idComp", "Mixed_Base_First_Base_Target_Category",
    "Mixed_Base_First_Base_Target_Hom", "Mixed_Base_First_Base_Target_assoc", "Mixed_Base_First_Base_Target_comp",
    "Mixed_Base_First_Base_Target_compId", "Mixed_Base_First_Base_Target_eqCongr",
    "Mixed_Base_First_Base_Target_eqTrans", "Mixed_Base_First_Base_Target_id", "Mixed_Base_First_Base_Target_idComp",
    "Mixed_Base_First_Base_eqCongr", "Mixed_Base_First_Base_functorMap", "Mixed_Base_First_Base_functorMapComp",
    "Mixed_Base_First_Base_functorMapId", "Mixed_Base_First_Base_functorObj", "Mixed_Base_First_NatTrans",
    "Mixed_Base_First_idNat", "Mixed_Base_First_natApp", "Mixed_Base_First_naturality",
    "Mixed_Base_First_targetEqSymm", "Mixed_Base_First_vcomp", "Mixed_Base_First_vcompLaw",
    "Mixed_Base_Middle_Category", "Mixed_Base_Middle_Hom", "Mixed_Base_Middle_assoc", "Mixed_Base_Middle_comp",
    "Mixed_Base_Middle_compId", "Mixed_Base_Middle_eqCongr", "Mixed_Base_Middle_eqTrans", "Mixed_Base_Middle_id",
    "Mixed_Base_Middle_idComp", "Mixed_Base_Second_Base_Functor", "Mixed_Base_Second_Base_Source_Category",
    "Mixed_Base_Second_Base_Source_Hom", "Mixed_Base_Second_Base_Source_assoc", "Mixed_Base_Second_Base_Source_comp",
    "Mixed_Base_Second_Base_Source_compId", "Mixed_Base_Second_Base_Source_eqCongr",
    "Mixed_Base_Second_Base_Source_eqTrans", "Mixed_Base_Second_Base_Source_id",
    "Mixed_Base_Second_Base_Source_idComp", "Mixed_Base_Second_Base_Target_Category",
    "Mixed_Base_Second_Base_Target_Hom", "Mixed_Base_Second_Base_Target_assoc", "Mixed_Base_Second_Base_Target_comp",
    "Mixed_Base_Second_Base_Target_compId", "Mixed_Base_Second_Base_Target_eqCongr",
    "Mixed_Base_Second_Base_Target_eqTrans", "Mixed_Base_Second_Base_Target_id",
    "Mixed_Base_Second_Base_Target_idComp", "Mixed_Base_Second_Base_eqCongr", "Mixed_Base_Second_Base_functorMap",
    "Mixed_Base_Second_Base_functorMapComp", "Mixed_Base_Second_Base_functorMapId",
    "Mixed_Base_Second_Base_functorObj", "Mixed_Base_Second_NatTrans", "Mixed_Base_Second_idNat",
    "Mixed_Base_Second_natApp", "Mixed_Base_Second_naturality", "Mixed_Base_Second_targetEqSymm",
    "Mixed_Base_Second_vcomp", "Mixed_Base_Second_vcompLaw", "Mixed_Base_Source_Category", "Mixed_Base_Source_Hom",
    "Mixed_Base_Source_assoc", "Mixed_Base_Source_comp", "Mixed_Base_Source_compId", "Mixed_Base_Source_eqCongr",
    "Mixed_Base_Source_eqTrans", "Mixed_Base_Source_id", "Mixed_Base_Source_idComp", "Mixed_Base_Target_Category",
    "Mixed_Base_Target_Hom", "Mixed_Base_Target_assoc", "Mixed_Base_Target_comp", "Mixed_Base_Target_compId",
    "Mixed_Base_Target_eqCongr", "Mixed_Base_Target_eqTrans", "Mixed_Base_Target_id", "Mixed_Base_Target_idComp",
    "Mixed_Base_hcomp", "Mixed_Lan_Base_Base_Composite_Functor", "Mixed_Lan_Base_Base_Composite_eqCongr",
    "Mixed_Lan_Base_Base_Composite_functorMap", "Mixed_Lan_Base_Base_Composite_functorMapComp",
    "Mixed_Lan_Base_Base_Composite_functorMapId", "Mixed_Lan_Base_Base_Composite_functorObj",
    "Mixed_Lan_Base_Base_First_Functor", "Mixed_Lan_Base_Base_First_eqCongr", "Mixed_Lan_Base_Base_First_functorMap",
    "Mixed_Lan_Base_Base_First_functorMapComp", "Mixed_Lan_Base_Base_First_functorMapId",
    "Mixed_Lan_Base_Base_First_functorObj", "Mixed_Lan_Base_Base_Middle_Category", "Mixed_Lan_Base_Base_Middle_Hom",
    "Mixed_Lan_Base_Base_Middle_assoc", "Mixed_Lan_Base_Base_Middle_comp", "Mixed_Lan_Base_Base_Middle_compId",
    "Mixed_Lan_Base_Base_Middle_eqCongr", "Mixed_Lan_Base_Base_Middle_eqTrans", "Mixed_Lan_Base_Base_Middle_id",
    "Mixed_Lan_Base_Base_Middle_idComp", "Mixed_Lan_Base_Base_Second_Functor", "Mixed_Lan_Base_Base_Second_eqCongr",
    "Mixed_Lan_Base_Base_Second_functorMap", "Mixed_Lan_Base_Base_Second_functorMapComp",
    "Mixed_Lan_Base_Base_Second_functorMapId", "Mixed_Lan_Base_Base_Second_functorObj",
    "Mixed_Lan_Base_Base_Source_Category", "Mixed_Lan_Base_Base_Source_Hom", "Mixed_Lan_Base_Base_Source_assoc",
    "Mixed_Lan_Base_Base_Source_comp", "Mixed_Lan_Base_Base_Source_compId", "Mixed_Lan_Base_Base_Source_eqCongr",
    "Mixed_Lan_Base_Base_Source_eqTrans", "Mixed_Lan_Base_Base_Source_id", "Mixed_Lan_Base_Base_Source_idComp",
    "Mixed_Lan_Base_Base_Target_Category", "Mixed_Lan_Base_Base_Target_Hom", "Mixed_Lan_Base_Base_Target_assoc",
    "Mixed_Lan_Base_Base_Target_comp", "Mixed_Lan_Base_Base_Target_compId", "Mixed_Lan_Base_Base_Target_eqCongr",
    "Mixed_Lan_Base_Base_Target_eqTrans", "Mixed_Lan_Base_Base_Target_id", "Mixed_Lan_Base_Base_Target_idComp",
    "Mixed_Lan_Base_Base_compFunctor", "Mixed_Lan_Base_Composite_NatTrans", "Mixed_Lan_Base_Composite_natApp",
    "Mixed_Lan_Base_Composite_naturality", "Mixed_Lan_Base_First_NatTrans", "Mixed_Lan_Base_First_natApp",
    "Mixed_Lan_Base_First_naturality", "Mixed_Lan_Base_Second_NatTrans", "Mixed_Lan_Base_Second_natApp",
    "Mixed_Lan_Base_Second_naturality", "Mixed_Lan_Base_targetEqSymm", "Mixed_Lan_Base_whiskerLeft",
    "Mixed_Lan_Base_whiskerRight", "Mixed_Lan_LanCocone", "Mixed_Lan_LanFactor", "Mixed_Lan_LanSolution",
    "Mixed_Lan_LanTail", "Mixed_Lan_LeftKanExtension", "Mixed_Lan_coconeFirst", "Mixed_Lan_coconeSecond",
    "Mixed_Lan_desc_unique", "Mixed_Lan_lanDesc", "Mixed_Lan_lanFac", "Mixed_Lan_lanFunctor", "Mixed_Lan_lanSolve",
    "Mixed_Lan_lanTail", "Mixed_Lan_lanUniq", "Mixed_Lan_lanUnit", "Mixed_Lan_recordFirst", "Mixed_Lan_recordSecond",
    "Mixed_descNat", "Mixed_unitNat", "Original", "OtherChosen", "OtherCocone", "OtherComponents", "OtherMediator",
    "Run_Base_Compose_Base_Composite_Functor", "Run_Base_Compose_Base_Composite_eqCongr",
    "Run_Base_Compose_Base_Composite_functorMap", "Run_Base_Compose_Base_Composite_functorMapComp",
    "Run_Base_Compose_Base_Composite_functorMapId", "Run_Base_Compose_Base_Composite_functorObj",
    "Run_Base_Compose_Base_First_Functor", "Run_Base_Compose_Base_First_eqCongr",
    "Run_Base_Compose_Base_First_functorMap", "Run_Base_Compose_Base_First_functorMapComp",
    "Run_Base_Compose_Base_First_functorMapId", "Run_Base_Compose_Base_First_functorObj",
    "Run_Base_Compose_Base_Middle_Category", "Run_Base_Compose_Base_Middle_Hom", "Run_Base_Compose_Base_Middle_assoc",
    "Run_Base_Compose_Base_Middle_comp", "Run_Base_Compose_Base_Middle_compId",
    "Run_Base_Compose_Base_Middle_eqCongr", "Run_Base_Compose_Base_Middle_eqTrans", "Run_Base_Compose_Base_Middle_id",
    "Run_Base_Compose_Base_Middle_idComp", "Run_Base_Compose_Base_Second_Functor",
    "Run_Base_Compose_Base_Second_eqCongr", "Run_Base_Compose_Base_Second_functorMap",
    "Run_Base_Compose_Base_Second_functorMapComp", "Run_Base_Compose_Base_Second_functorMapId",
    "Run_Base_Compose_Base_Second_functorObj", "Run_Base_Compose_Base_Source_Category",
    "Run_Base_Compose_Base_Source_Hom", "Run_Base_Compose_Base_Source_assoc", "Run_Base_Compose_Base_Source_comp",
    "Run_Base_Compose_Base_Source_compId", "Run_Base_Compose_Base_Source_eqCongr",
    "Run_Base_Compose_Base_Source_eqTrans", "Run_Base_Compose_Base_Source_id", "Run_Base_Compose_Base_Source_idComp",
    "Run_Base_Compose_Base_Target_Category", "Run_Base_Compose_Base_Target_Hom", "Run_Base_Compose_Base_Target_assoc",
    "Run_Base_Compose_Base_Target_comp", "Run_Base_Compose_Base_Target_compId",
    "Run_Base_Compose_Base_Target_eqCongr", "Run_Base_Compose_Base_Target_eqTrans", "Run_Base_Compose_Base_Target_id",
    "Run_Base_Compose_Base_Target_idComp", "Run_Base_Compose_Base_compFunctor", "Run_Base_Compose_Composite_NatTrans",
    "Run_Base_Compose_Composite_natApp", "Run_Base_Compose_Composite_naturality", "Run_Base_Compose_First_NatTrans",
    "Run_Base_Compose_First_natApp", "Run_Base_Compose_First_naturality", "Run_Base_Compose_Second_NatTrans",
    "Run_Base_Compose_Second_natApp", "Run_Base_Compose_Second_naturality", "Run_Base_Compose_targetEqSymm",
    "Run_Base_Compose_whiskerLeft", "Run_Base_Compose_whiskerRight", "Run_Base_Composite_Base_Functor",
    "Run_Base_Composite_Base_Source_Category", "Run_Base_Composite_Base_Source_Hom",
    "Run_Base_Composite_Base_Source_assoc", "Run_Base_Composite_Base_Source_comp",
    "Run_Base_Composite_Base_Source_compId", "Run_Base_Composite_Base_Source_eqCongr",
    "Run_Base_Composite_Base_Source_eqTrans", "Run_Base_Composite_Base_Source_id",
    "Run_Base_Composite_Base_Source_idComp", "Run_Base_Composite_Base_Target_Category",
    "Run_Base_Composite_Base_Target_Hom", "Run_Base_Composite_Base_Target_assoc",
    "Run_Base_Composite_Base_Target_comp", "Run_Base_Composite_Base_Target_compId",
    "Run_Base_Composite_Base_Target_eqCongr", "Run_Base_Composite_Base_Target_eqTrans",
    "Run_Base_Composite_Base_Target_id", "Run_Base_Composite_Base_Target_idComp", "Run_Base_Composite_Base_eqCongr",
    "Run_Base_Composite_Base_functorMap", "Run_Base_Composite_Base_functorMapComp",
    "Run_Base_Composite_Base_functorMapId", "Run_Base_Composite_Base_functorObj", "Run_Base_Composite_NatTrans",
    "Run_Base_Composite_idNat", "Run_Base_Composite_natApp", "Run_Base_Composite_naturality",
    "Run_Base_Composite_targetEqSymm", "Run_Base_Composite_vcomp", "Run_Base_Composite_vcompLaw",
    "Run_Base_First_Base_Functor", "Run_Base_First_Base_Source_Category", "Run_Base_First_Base_Source_Hom",
    "Run_Base_First_Base_Source_assoc", "Run_Base_First_Base_Source_comp", "Run_Base_First_Base_Source_compId",
    "Run_Base_First_Base_Source_eqCongr", "Run_Base_First_Base_Source_eqTrans", "Run_Base_First_Base_Source_id",
    "Run_Base_First_Base_Source_idComp", "Run_Base_First_Base_Target_Category", "Run_Base_First_Base_Target_Hom",
    "Run_Base_First_Base_Target_assoc", "Run_Base_First_Base_Target_comp", "Run_Base_First_Base_Target_compId",
    "Run_Base_First_Base_Target_eqCongr", "Run_Base_First_Base_Target_eqTrans", "Run_Base_First_Base_Target_id",
    "Run_Base_First_Base_Target_idComp", "Run_Base_First_Base_eqCongr", "Run_Base_First_Base_functorMap",
    "Run_Base_First_Base_functorMapComp", "Run_Base_First_Base_functorMapId", "Run_Base_First_Base_functorObj",
    "Run_Base_First_NatTrans", "Run_Base_First_idNat", "Run_Base_First_natApp", "Run_Base_First_naturality",
    "Run_Base_First_targetEqSymm", "Run_Base_First_vcomp", "Run_Base_First_vcompLaw", "Run_Base_Middle_Category",
    "Run_Base_Middle_Hom", "Run_Base_Middle_assoc", "Run_Base_Middle_comp", "Run_Base_Middle_compId",
    "Run_Base_Middle_eqCongr", "Run_Base_Middle_eqTrans", "Run_Base_Middle_id", "Run_Base_Middle_idComp",
    "Run_Base_Second_Base_Functor", "Run_Base_Second_Base_Source_Category", "Run_Base_Second_Base_Source_Hom",
    "Run_Base_Second_Base_Source_assoc", "Run_Base_Second_Base_Source_comp", "Run_Base_Second_Base_Source_compId",
    "Run_Base_Second_Base_Source_eqCongr", "Run_Base_Second_Base_Source_eqTrans", "Run_Base_Second_Base_Source_id",
    "Run_Base_Second_Base_Source_idComp", "Run_Base_Second_Base_Target_Category", "Run_Base_Second_Base_Target_Hom",
    "Run_Base_Second_Base_Target_assoc", "Run_Base_Second_Base_Target_comp", "Run_Base_Second_Base_Target_compId",
    "Run_Base_Second_Base_Target_eqCongr", "Run_Base_Second_Base_Target_eqTrans", "Run_Base_Second_Base_Target_id",
    "Run_Base_Second_Base_Target_idComp", "Run_Base_Second_Base_eqCongr", "Run_Base_Second_Base_functorMap",
    "Run_Base_Second_Base_functorMapComp", "Run_Base_Second_Base_functorMapId", "Run_Base_Second_Base_functorObj",
    "Run_Base_Second_NatTrans", "Run_Base_Second_idNat", "Run_Base_Second_natApp", "Run_Base_Second_naturality",
    "Run_Base_Second_targetEqSymm", "Run_Base_Second_vcomp", "Run_Base_Second_vcompLaw", "Run_Base_Source_Category",
    "Run_Base_Source_Hom", "Run_Base_Source_assoc", "Run_Base_Source_comp", "Run_Base_Source_compId",
    "Run_Base_Source_eqCongr", "Run_Base_Source_eqTrans", "Run_Base_Source_id", "Run_Base_Source_idComp",
    "Run_Base_Target_Category", "Run_Base_Target_Hom", "Run_Base_Target_assoc", "Run_Base_Target_comp",
    "Run_Base_Target_compId", "Run_Base_Target_eqCongr", "Run_Base_Target_eqTrans", "Run_Base_Target_id",
    "Run_Base_Target_idComp", "Run_Base_hcomp", "Run_Lan_Base_Base_Composite_Functor",
    "Run_Lan_Base_Base_Composite_eqCongr", "Run_Lan_Base_Base_Composite_functorMap",
    "Run_Lan_Base_Base_Composite_functorMapComp", "Run_Lan_Base_Base_Composite_functorMapId",
    "Run_Lan_Base_Base_Composite_functorObj", "Run_Lan_Base_Base_First_Functor", "Run_Lan_Base_Base_First_eqCongr",
    "Run_Lan_Base_Base_First_functorMap", "Run_Lan_Base_Base_First_functorMapComp",
    "Run_Lan_Base_Base_First_functorMapId", "Run_Lan_Base_Base_First_functorObj", "Run_Lan_Base_Base_Middle_Category",
    "Run_Lan_Base_Base_Middle_Hom", "Run_Lan_Base_Base_Middle_assoc", "Run_Lan_Base_Base_Middle_comp",
    "Run_Lan_Base_Base_Middle_compId", "Run_Lan_Base_Base_Middle_eqCongr", "Run_Lan_Base_Base_Middle_eqTrans",
    "Run_Lan_Base_Base_Middle_id", "Run_Lan_Base_Base_Middle_idComp", "Run_Lan_Base_Base_Second_Functor",
    "Run_Lan_Base_Base_Second_eqCongr", "Run_Lan_Base_Base_Second_functorMap",
    "Run_Lan_Base_Base_Second_functorMapComp", "Run_Lan_Base_Base_Second_functorMapId",
    "Run_Lan_Base_Base_Second_functorObj", "Run_Lan_Base_Base_Source_Category", "Run_Lan_Base_Base_Source_Hom",
    "Run_Lan_Base_Base_Source_assoc", "Run_Lan_Base_Base_Source_comp", "Run_Lan_Base_Base_Source_compId",
    "Run_Lan_Base_Base_Source_eqCongr", "Run_Lan_Base_Base_Source_eqTrans", "Run_Lan_Base_Base_Source_id",
    "Run_Lan_Base_Base_Source_idComp", "Run_Lan_Base_Base_Target_Category", "Run_Lan_Base_Base_Target_Hom",
    "Run_Lan_Base_Base_Target_assoc", "Run_Lan_Base_Base_Target_comp", "Run_Lan_Base_Base_Target_compId",
    "Run_Lan_Base_Base_Target_eqCongr", "Run_Lan_Base_Base_Target_eqTrans", "Run_Lan_Base_Base_Target_id",
    "Run_Lan_Base_Base_Target_idComp", "Run_Lan_Base_Base_compFunctor", "Run_Lan_Base_Composite_NatTrans",
    "Run_Lan_Base_Composite_natApp", "Run_Lan_Base_Composite_naturality", "Run_Lan_Base_First_NatTrans",
    "Run_Lan_Base_First_natApp", "Run_Lan_Base_First_naturality", "Run_Lan_Base_Second_NatTrans",
    "Run_Lan_Base_Second_natApp", "Run_Lan_Base_Second_naturality", "Run_Lan_Base_targetEqSymm",
    "Run_Lan_Base_whiskerLeft", "Run_Lan_Base_whiskerRight", "Run_Lan_LanCocone", "Run_Lan_LanFactor",
    "Run_Lan_LanSolution", "Run_Lan_LanTail", "Run_Lan_LeftKanExtension", "Run_Lan_coconeFirst",
    "Run_Lan_coconeSecond", "Run_Lan_desc_unique", "Run_Lan_lanDesc", "Run_Lan_lanFac", "Run_Lan_lanFunctor",
    "Run_Lan_lanSolve", "Run_Lan_lanTail", "Run_Lan_lanUniq", "Run_Lan_lanUnit", "Run_Lan_recordFirst",
    "Run_Lan_recordSecond", "Run_descNat", "Run_unitNat", "WideCoconeContract", "WideCoconeSort", "WideExtensionSort",
    "WideFactorContract", "WideMiddleHom", "WideSolutionSort", "WideSourceHom", "WideTailSort", "WideTargetHom",
    "WideUniqueContract", "Wide_Base_Compose_Base_Composite_Functor", "Wide_Base_Compose_Base_Composite_eqCongr",
    "Wide_Base_Compose_Base_Composite_functorMap", "Wide_Base_Compose_Base_Composite_functorMapComp",
    "Wide_Base_Compose_Base_Composite_functorMapId", "Wide_Base_Compose_Base_Composite_functorObj",
    "Wide_Base_Compose_Base_First_Functor", "Wide_Base_Compose_Base_First_eqCongr",
    "Wide_Base_Compose_Base_First_functorMap", "Wide_Base_Compose_Base_First_functorMapComp",
    "Wide_Base_Compose_Base_First_functorMapId", "Wide_Base_Compose_Base_First_functorObj",
    "Wide_Base_Compose_Base_Middle_Category", "Wide_Base_Compose_Base_Middle_Hom",
    "Wide_Base_Compose_Base_Middle_assoc", "Wide_Base_Compose_Base_Middle_comp",
    "Wide_Base_Compose_Base_Middle_compId", "Wide_Base_Compose_Base_Middle_eqCongr",
    "Wide_Base_Compose_Base_Middle_eqTrans", "Wide_Base_Compose_Base_Middle_id",
    "Wide_Base_Compose_Base_Middle_idComp", "Wide_Base_Compose_Base_Second_Functor",
    "Wide_Base_Compose_Base_Second_eqCongr", "Wide_Base_Compose_Base_Second_functorMap",
    "Wide_Base_Compose_Base_Second_functorMapComp", "Wide_Base_Compose_Base_Second_functorMapId",
    "Wide_Base_Compose_Base_Second_functorObj", "Wide_Base_Compose_Base_Source_Category",
    "Wide_Base_Compose_Base_Source_Hom", "Wide_Base_Compose_Base_Source_assoc", "Wide_Base_Compose_Base_Source_comp",
    "Wide_Base_Compose_Base_Source_compId", "Wide_Base_Compose_Base_Source_eqCongr",
    "Wide_Base_Compose_Base_Source_eqTrans", "Wide_Base_Compose_Base_Source_id",
    "Wide_Base_Compose_Base_Source_idComp", "Wide_Base_Compose_Base_Target_Category",
    "Wide_Base_Compose_Base_Target_Hom", "Wide_Base_Compose_Base_Target_assoc", "Wide_Base_Compose_Base_Target_comp",
    "Wide_Base_Compose_Base_Target_compId", "Wide_Base_Compose_Base_Target_eqCongr",
    "Wide_Base_Compose_Base_Target_eqTrans", "Wide_Base_Compose_Base_Target_id",
    "Wide_Base_Compose_Base_Target_idComp", "Wide_Base_Compose_Base_compFunctor",
    "Wide_Base_Compose_Composite_NatTrans", "Wide_Base_Compose_Composite_natApp",
    "Wide_Base_Compose_Composite_naturality", "Wide_Base_Compose_First_NatTrans", "Wide_Base_Compose_First_natApp",
    "Wide_Base_Compose_First_naturality", "Wide_Base_Compose_Second_NatTrans", "Wide_Base_Compose_Second_natApp",
    "Wide_Base_Compose_Second_naturality", "Wide_Base_Compose_targetEqSymm", "Wide_Base_Compose_whiskerLeft",
    "Wide_Base_Compose_whiskerRight", "Wide_Base_Composite_Base_Functor", "Wide_Base_Composite_Base_Source_Category",
    "Wide_Base_Composite_Base_Source_Hom", "Wide_Base_Composite_Base_Source_assoc",
    "Wide_Base_Composite_Base_Source_comp", "Wide_Base_Composite_Base_Source_compId",
    "Wide_Base_Composite_Base_Source_eqCongr", "Wide_Base_Composite_Base_Source_eqTrans",
    "Wide_Base_Composite_Base_Source_id", "Wide_Base_Composite_Base_Source_idComp",
    "Wide_Base_Composite_Base_Target_Category", "Wide_Base_Composite_Base_Target_Hom",
    "Wide_Base_Composite_Base_Target_assoc", "Wide_Base_Composite_Base_Target_comp",
    "Wide_Base_Composite_Base_Target_compId", "Wide_Base_Composite_Base_Target_eqCongr",
    "Wide_Base_Composite_Base_Target_eqTrans", "Wide_Base_Composite_Base_Target_id",
    "Wide_Base_Composite_Base_Target_idComp", "Wide_Base_Composite_Base_eqCongr",
    "Wide_Base_Composite_Base_functorMap", "Wide_Base_Composite_Base_functorMapComp",
    "Wide_Base_Composite_Base_functorMapId", "Wide_Base_Composite_Base_functorObj", "Wide_Base_Composite_NatTrans",
    "Wide_Base_Composite_idNat", "Wide_Base_Composite_natApp", "Wide_Base_Composite_naturality",
    "Wide_Base_Composite_targetEqSymm", "Wide_Base_Composite_vcomp", "Wide_Base_Composite_vcompLaw",
    "Wide_Base_First_Base_Functor", "Wide_Base_First_Base_Source_Category", "Wide_Base_First_Base_Source_Hom",
    "Wide_Base_First_Base_Source_assoc", "Wide_Base_First_Base_Source_comp", "Wide_Base_First_Base_Source_compId",
    "Wide_Base_First_Base_Source_eqCongr", "Wide_Base_First_Base_Source_eqTrans", "Wide_Base_First_Base_Source_id",
    "Wide_Base_First_Base_Source_idComp", "Wide_Base_First_Base_Target_Category", "Wide_Base_First_Base_Target_Hom",
    "Wide_Base_First_Base_Target_assoc", "Wide_Base_First_Base_Target_comp", "Wide_Base_First_Base_Target_compId",
    "Wide_Base_First_Base_Target_eqCongr", "Wide_Base_First_Base_Target_eqTrans", "Wide_Base_First_Base_Target_id",
    "Wide_Base_First_Base_Target_idComp", "Wide_Base_First_Base_eqCongr", "Wide_Base_First_Base_functorMap",
    "Wide_Base_First_Base_functorMapComp", "Wide_Base_First_Base_functorMapId", "Wide_Base_First_Base_functorObj",
    "Wide_Base_First_NatTrans", "Wide_Base_First_idNat", "Wide_Base_First_natApp", "Wide_Base_First_naturality",
    "Wide_Base_First_targetEqSymm", "Wide_Base_First_vcomp", "Wide_Base_First_vcompLaw", "Wide_Base_Middle_Category",
    "Wide_Base_Middle_Hom", "Wide_Base_Middle_assoc", "Wide_Base_Middle_comp", "Wide_Base_Middle_compId",
    "Wide_Base_Middle_eqCongr", "Wide_Base_Middle_eqTrans", "Wide_Base_Middle_id", "Wide_Base_Middle_idComp",
    "Wide_Base_Second_Base_Functor", "Wide_Base_Second_Base_Source_Category", "Wide_Base_Second_Base_Source_Hom",
    "Wide_Base_Second_Base_Source_assoc", "Wide_Base_Second_Base_Source_comp", "Wide_Base_Second_Base_Source_compId",
    "Wide_Base_Second_Base_Source_eqCongr", "Wide_Base_Second_Base_Source_eqTrans", "Wide_Base_Second_Base_Source_id",
    "Wide_Base_Second_Base_Source_idComp", "Wide_Base_Second_Base_Target_Category",
    "Wide_Base_Second_Base_Target_Hom", "Wide_Base_Second_Base_Target_assoc", "Wide_Base_Second_Base_Target_comp",
    "Wide_Base_Second_Base_Target_compId", "Wide_Base_Second_Base_Target_eqCongr",
    "Wide_Base_Second_Base_Target_eqTrans", "Wide_Base_Second_Base_Target_id", "Wide_Base_Second_Base_Target_idComp",
    "Wide_Base_Second_Base_eqCongr", "Wide_Base_Second_Base_functorMap", "Wide_Base_Second_Base_functorMapComp",
    "Wide_Base_Second_Base_functorMapId", "Wide_Base_Second_Base_functorObj", "Wide_Base_Second_NatTrans",
    "Wide_Base_Second_idNat", "Wide_Base_Second_natApp", "Wide_Base_Second_naturality",
    "Wide_Base_Second_targetEqSymm", "Wide_Base_Second_vcomp", "Wide_Base_Second_vcompLaw",
    "Wide_Base_Source_Category", "Wide_Base_Source_Hom", "Wide_Base_Source_assoc", "Wide_Base_Source_comp",
    "Wide_Base_Source_compId", "Wide_Base_Source_eqCongr", "Wide_Base_Source_eqTrans", "Wide_Base_Source_id",
    "Wide_Base_Source_idComp", "Wide_Base_Target_Category", "Wide_Base_Target_Hom", "Wide_Base_Target_assoc",
    "Wide_Base_Target_comp", "Wide_Base_Target_compId", "Wide_Base_Target_eqCongr", "Wide_Base_Target_eqTrans",
    "Wide_Base_Target_id", "Wide_Base_Target_idComp", "Wide_Base_hcomp", "Wide_Lan_Base_Base_Composite_Functor",
    "Wide_Lan_Base_Base_Composite_eqCongr", "Wide_Lan_Base_Base_Composite_functorMap",
    "Wide_Lan_Base_Base_Composite_functorMapComp", "Wide_Lan_Base_Base_Composite_functorMapId",
    "Wide_Lan_Base_Base_Composite_functorObj", "Wide_Lan_Base_Base_First_Functor", "Wide_Lan_Base_Base_First_eqCongr",
    "Wide_Lan_Base_Base_First_functorMap", "Wide_Lan_Base_Base_First_functorMapComp",
    "Wide_Lan_Base_Base_First_functorMapId", "Wide_Lan_Base_Base_First_functorObj",
    "Wide_Lan_Base_Base_Middle_Category", "Wide_Lan_Base_Base_Middle_Hom", "Wide_Lan_Base_Base_Middle_assoc",
    "Wide_Lan_Base_Base_Middle_comp", "Wide_Lan_Base_Base_Middle_compId", "Wide_Lan_Base_Base_Middle_eqCongr",
    "Wide_Lan_Base_Base_Middle_eqTrans", "Wide_Lan_Base_Base_Middle_id", "Wide_Lan_Base_Base_Middle_idComp",
    "Wide_Lan_Base_Base_Second_Functor", "Wide_Lan_Base_Base_Second_eqCongr", "Wide_Lan_Base_Base_Second_functorMap",
    "Wide_Lan_Base_Base_Second_functorMapComp", "Wide_Lan_Base_Base_Second_functorMapId",
    "Wide_Lan_Base_Base_Second_functorObj", "Wide_Lan_Base_Base_Source_Category", "Wide_Lan_Base_Base_Source_Hom",
    "Wide_Lan_Base_Base_Source_assoc", "Wide_Lan_Base_Base_Source_comp", "Wide_Lan_Base_Base_Source_compId",
    "Wide_Lan_Base_Base_Source_eqCongr", "Wide_Lan_Base_Base_Source_eqTrans", "Wide_Lan_Base_Base_Source_id",
    "Wide_Lan_Base_Base_Source_idComp", "Wide_Lan_Base_Base_Target_Category", "Wide_Lan_Base_Base_Target_Hom",
    "Wide_Lan_Base_Base_Target_assoc", "Wide_Lan_Base_Base_Target_comp", "Wide_Lan_Base_Base_Target_compId",
    "Wide_Lan_Base_Base_Target_eqCongr", "Wide_Lan_Base_Base_Target_eqTrans", "Wide_Lan_Base_Base_Target_id",
    "Wide_Lan_Base_Base_Target_idComp", "Wide_Lan_Base_Base_compFunctor", "Wide_Lan_Base_Composite_NatTrans",
    "Wide_Lan_Base_Composite_natApp", "Wide_Lan_Base_Composite_naturality", "Wide_Lan_Base_First_NatTrans",
    "Wide_Lan_Base_First_natApp", "Wide_Lan_Base_First_naturality", "Wide_Lan_Base_Second_NatTrans",
    "Wide_Lan_Base_Second_natApp", "Wide_Lan_Base_Second_naturality", "Wide_Lan_Base_targetEqSymm",
    "Wide_Lan_Base_whiskerLeft", "Wide_Lan_Base_whiskerRight", "Wide_Lan_LanCocone", "Wide_Lan_LanFactor",
    "Wide_Lan_LanSolution", "Wide_Lan_LanTail", "Wide_Lan_LeftKanExtension", "Wide_Lan_coconeFirst",
    "Wide_Lan_coconeSecond", "Wide_Lan_desc_unique", "Wide_Lan_lanDesc", "Wide_Lan_lanFac", "Wide_Lan_lanFunctor",
    "Wide_Lan_lanSolve", "Wide_Lan_lanTail", "Wide_Lan_lanUniq", "Wide_Lan_lanUnit", "Wide_Lan_recordFirst",
    "Wide_Lan_recordSecond", "Wide_descNat", "Wide_unitNat", "componentLaw", "externalFactorWitness", "factorWitness",
    "forgetLan", "forgetUnit", "lanDescFirst", "lanDescSecond", "lanMapResult", "lanUnitResult", "lanVerticalFirst",
    "lanVerticalSecond", "otherComponentLaw", "pathTrans", "sharedLanInput", "uniqueWitness"
};

/* expected_families() (bend2/tests/prelude_shared_left_kan.bend line 15, 13 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "EqualMiddle", "EqualSource", "Mixed_Base_Middle", "Mixed_Base_Source", "Mixed_Base_Target", "Path", "Point",
    "Run_Base_Middle", "Run_Base_Source", "Run_Base_Target", "Wide_Base_Middle", "Wide_Base_Source",
    "Wide_Base_Target"
};

/* negatives() (bend2/tests/prelude_shared_left_kan.bend line 18, 7 names) */
static const char *const NEGATIVES[] = {
    "cocone-endpoint", "extension-sort", "missing-factor", "missing-uniqueness", "nominal-category",
    "uniqueness-endpoint", "wrong-factor"
};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "lanUnitResult", relational_natural(heap, 38));
  computation(heap, &out, "lanMapResult", relational_natural(heap, 40));
  computation(heap, &out, "lanDescFirst", relational_natural(heap, 42));
  computation(heap, &out, "lanDescSecond", relational_natural(heap, 50));
  computation(heap, &out, "lanVerticalFirst", relational_natural(heap, 48));
  computation(heap, &out, "lanVerticalSecond", relational_natural(heap, 80));
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech",
    "prelude/cat/heterogeneous-left-kan.mech", "prelude/cat/shared-left-kan.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/shared-left-kan.mech", "test/fixtures/prelude/shared-left-kan-runtime.mech"
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
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/shared-left-kan"), &negatives, globals,
                               negatives_prefix(64, false, true)));
  *fields = str_lit("entries=944 families=13 computations=6 negatives=7");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-SHARED-LEFT-KAN", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-SHARED-LEFT-KAN"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
