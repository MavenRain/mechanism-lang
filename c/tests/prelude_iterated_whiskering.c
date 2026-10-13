/* Port of bend2/tests/prelude_iterated_whiskering.bend (41 lines): the iterated whiskering prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-ITERATED-WHISKERING-OK fields or PRELUDE-ITERATED-WHISKERING-FAIL message. Same definitions in the same order. */
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

/* One Program.Probe{name, payload, expected} row of values(). */
static void probe(Heap *heap, Vec *out, const char *name, uint32_t payload, uint32_t expected) {
  Probe *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->payload = payload;
  row->expected = expected;
  vec_push(out, row);
}

/* expected_entries() (bend2/tests/prelude_iterated_whiskering.bend line 12, 902 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "AlphaM1M2", "AlphaM2T", "Conjugate", "Constant", "ConstantM1M2", "ConstantM2T", "EndMiddle1",
    "EndMiddle2", "EndTarget", "IndexedMap", "IndexedMiddle1", "IndexedMiddle2", "IndexedSource", "Left",
    "Middle1_Category", "Middle1_Hom", "Middle1_assoc", "Middle1_comp", "Middle1_compId", "Middle1_eqCongr",
    "Middle1_eqTrans", "Middle1_id", "Middle1_idComp", "Middle2_Category", "Middle2_Hom", "Middle2_assoc",
    "Middle2_comp", "Middle2_compId", "Middle2_eqCongr", "Middle2_eqTrans", "Middle2_id", "Middle2_idComp",
    "Mixed_ABC_Base_Composite_Functor", "Mixed_ABC_Base_Composite_eqCongr", "Mixed_ABC_Base_Composite_functorMap",
    "Mixed_ABC_Base_Composite_functorMapComp", "Mixed_ABC_Base_Composite_functorMapId",
    "Mixed_ABC_Base_Composite_functorObj", "Mixed_ABC_Base_First_Functor", "Mixed_ABC_Base_First_eqCongr",
    "Mixed_ABC_Base_First_functorMap", "Mixed_ABC_Base_First_functorMapComp", "Mixed_ABC_Base_First_functorMapId",
    "Mixed_ABC_Base_First_functorObj", "Mixed_ABC_Base_Middle_Category", "Mixed_ABC_Base_Middle_Hom",
    "Mixed_ABC_Base_Middle_assoc", "Mixed_ABC_Base_Middle_comp", "Mixed_ABC_Base_Middle_compId",
    "Mixed_ABC_Base_Middle_eqCongr", "Mixed_ABC_Base_Middle_eqTrans", "Mixed_ABC_Base_Middle_id",
    "Mixed_ABC_Base_Middle_idComp", "Mixed_ABC_Base_Second_Functor", "Mixed_ABC_Base_Second_eqCongr",
    "Mixed_ABC_Base_Second_functorMap", "Mixed_ABC_Base_Second_functorMapComp", "Mixed_ABC_Base_Second_functorMapId",
    "Mixed_ABC_Base_Second_functorObj", "Mixed_ABC_Base_Source_Category", "Mixed_ABC_Base_Source_Hom",
    "Mixed_ABC_Base_Source_assoc", "Mixed_ABC_Base_Source_comp", "Mixed_ABC_Base_Source_compId",
    "Mixed_ABC_Base_Source_eqCongr", "Mixed_ABC_Base_Source_eqTrans", "Mixed_ABC_Base_Source_id",
    "Mixed_ABC_Base_Source_idComp", "Mixed_ABC_Base_Target_Category", "Mixed_ABC_Base_Target_Hom",
    "Mixed_ABC_Base_Target_assoc", "Mixed_ABC_Base_Target_comp", "Mixed_ABC_Base_Target_compId",
    "Mixed_ABC_Base_Target_eqCongr", "Mixed_ABC_Base_Target_eqTrans", "Mixed_ABC_Base_Target_id",
    "Mixed_ABC_Base_Target_idComp", "Mixed_ABC_Base_compFunctor", "Mixed_ABC_Composite_NatTrans",
    "Mixed_ABC_Composite_natApp", "Mixed_ABC_Composite_naturality", "Mixed_ABC_First_NatTrans",
    "Mixed_ABC_First_natApp", "Mixed_ABC_First_naturality", "Mixed_ABC_Second_NatTrans", "Mixed_ABC_Second_natApp",
    "Mixed_ABC_Second_naturality", "Mixed_ABC_targetEqSymm", "Mixed_ABC_whiskerLeft", "Mixed_ABC_whiskerRight",
    "Mixed_ABD_Base_Composite_Functor", "Mixed_ABD_Base_Composite_eqCongr", "Mixed_ABD_Base_Composite_functorMap",
    "Mixed_ABD_Base_Composite_functorMapComp", "Mixed_ABD_Base_Composite_functorMapId",
    "Mixed_ABD_Base_Composite_functorObj", "Mixed_ABD_Base_First_Functor", "Mixed_ABD_Base_First_eqCongr",
    "Mixed_ABD_Base_First_functorMap", "Mixed_ABD_Base_First_functorMapComp", "Mixed_ABD_Base_First_functorMapId",
    "Mixed_ABD_Base_First_functorObj", "Mixed_ABD_Base_Middle_Category", "Mixed_ABD_Base_Middle_Hom",
    "Mixed_ABD_Base_Middle_assoc", "Mixed_ABD_Base_Middle_comp", "Mixed_ABD_Base_Middle_compId",
    "Mixed_ABD_Base_Middle_eqCongr", "Mixed_ABD_Base_Middle_eqTrans", "Mixed_ABD_Base_Middle_id",
    "Mixed_ABD_Base_Middle_idComp", "Mixed_ABD_Base_Second_Functor", "Mixed_ABD_Base_Second_eqCongr",
    "Mixed_ABD_Base_Second_functorMap", "Mixed_ABD_Base_Second_functorMapComp", "Mixed_ABD_Base_Second_functorMapId",
    "Mixed_ABD_Base_Second_functorObj", "Mixed_ABD_Base_Source_Category", "Mixed_ABD_Base_Source_Hom",
    "Mixed_ABD_Base_Source_assoc", "Mixed_ABD_Base_Source_comp", "Mixed_ABD_Base_Source_compId",
    "Mixed_ABD_Base_Source_eqCongr", "Mixed_ABD_Base_Source_eqTrans", "Mixed_ABD_Base_Source_id",
    "Mixed_ABD_Base_Source_idComp", "Mixed_ABD_Base_Target_Category", "Mixed_ABD_Base_Target_Hom",
    "Mixed_ABD_Base_Target_assoc", "Mixed_ABD_Base_Target_comp", "Mixed_ABD_Base_Target_compId",
    "Mixed_ABD_Base_Target_eqCongr", "Mixed_ABD_Base_Target_eqTrans", "Mixed_ABD_Base_Target_id",
    "Mixed_ABD_Base_Target_idComp", "Mixed_ABD_Base_compFunctor", "Mixed_ABD_Composite_NatTrans",
    "Mixed_ABD_Composite_natApp", "Mixed_ABD_Composite_naturality", "Mixed_ABD_First_NatTrans",
    "Mixed_ABD_First_natApp", "Mixed_ABD_First_naturality", "Mixed_ABD_Second_NatTrans", "Mixed_ABD_Second_natApp",
    "Mixed_ABD_Second_naturality", "Mixed_ABD_targetEqSymm", "Mixed_ABD_whiskerLeft", "Mixed_ABD_whiskerRight",
    "Mixed_ACD_Base_Composite_Functor", "Mixed_ACD_Base_Composite_eqCongr", "Mixed_ACD_Base_Composite_functorMap",
    "Mixed_ACD_Base_Composite_functorMapComp", "Mixed_ACD_Base_Composite_functorMapId",
    "Mixed_ACD_Base_Composite_functorObj", "Mixed_ACD_Base_First_Functor", "Mixed_ACD_Base_First_eqCongr",
    "Mixed_ACD_Base_First_functorMap", "Mixed_ACD_Base_First_functorMapComp", "Mixed_ACD_Base_First_functorMapId",
    "Mixed_ACD_Base_First_functorObj", "Mixed_ACD_Base_Middle_Category", "Mixed_ACD_Base_Middle_Hom",
    "Mixed_ACD_Base_Middle_assoc", "Mixed_ACD_Base_Middle_comp", "Mixed_ACD_Base_Middle_compId",
    "Mixed_ACD_Base_Middle_eqCongr", "Mixed_ACD_Base_Middle_eqTrans", "Mixed_ACD_Base_Middle_id",
    "Mixed_ACD_Base_Middle_idComp", "Mixed_ACD_Base_Second_Functor", "Mixed_ACD_Base_Second_eqCongr",
    "Mixed_ACD_Base_Second_functorMap", "Mixed_ACD_Base_Second_functorMapComp", "Mixed_ACD_Base_Second_functorMapId",
    "Mixed_ACD_Base_Second_functorObj", "Mixed_ACD_Base_Source_Category", "Mixed_ACD_Base_Source_Hom",
    "Mixed_ACD_Base_Source_assoc", "Mixed_ACD_Base_Source_comp", "Mixed_ACD_Base_Source_compId",
    "Mixed_ACD_Base_Source_eqCongr", "Mixed_ACD_Base_Source_eqTrans", "Mixed_ACD_Base_Source_id",
    "Mixed_ACD_Base_Source_idComp", "Mixed_ACD_Base_Target_Category", "Mixed_ACD_Base_Target_Hom",
    "Mixed_ACD_Base_Target_assoc", "Mixed_ACD_Base_Target_comp", "Mixed_ACD_Base_Target_compId",
    "Mixed_ACD_Base_Target_eqCongr", "Mixed_ACD_Base_Target_eqTrans", "Mixed_ACD_Base_Target_id",
    "Mixed_ACD_Base_Target_idComp", "Mixed_ACD_Base_compFunctor", "Mixed_ACD_Composite_NatTrans",
    "Mixed_ACD_Composite_natApp", "Mixed_ACD_Composite_naturality", "Mixed_ACD_First_NatTrans",
    "Mixed_ACD_First_natApp", "Mixed_ACD_First_naturality", "Mixed_ACD_Second_NatTrans", "Mixed_ACD_Second_natApp",
    "Mixed_ACD_Second_naturality", "Mixed_ACD_targetEqSymm", "Mixed_ACD_whiskerLeft", "Mixed_ACD_whiskerRight",
    "Mixed_BCD_Base_Composite_Functor", "Mixed_BCD_Base_Composite_eqCongr", "Mixed_BCD_Base_Composite_functorMap",
    "Mixed_BCD_Base_Composite_functorMapComp", "Mixed_BCD_Base_Composite_functorMapId",
    "Mixed_BCD_Base_Composite_functorObj", "Mixed_BCD_Base_First_Functor", "Mixed_BCD_Base_First_eqCongr",
    "Mixed_BCD_Base_First_functorMap", "Mixed_BCD_Base_First_functorMapComp", "Mixed_BCD_Base_First_functorMapId",
    "Mixed_BCD_Base_First_functorObj", "Mixed_BCD_Base_Middle_Category", "Mixed_BCD_Base_Middle_Hom",
    "Mixed_BCD_Base_Middle_assoc", "Mixed_BCD_Base_Middle_comp", "Mixed_BCD_Base_Middle_compId",
    "Mixed_BCD_Base_Middle_eqCongr", "Mixed_BCD_Base_Middle_eqTrans", "Mixed_BCD_Base_Middle_id",
    "Mixed_BCD_Base_Middle_idComp", "Mixed_BCD_Base_Second_Functor", "Mixed_BCD_Base_Second_eqCongr",
    "Mixed_BCD_Base_Second_functorMap", "Mixed_BCD_Base_Second_functorMapComp", "Mixed_BCD_Base_Second_functorMapId",
    "Mixed_BCD_Base_Second_functorObj", "Mixed_BCD_Base_Source_Category", "Mixed_BCD_Base_Source_Hom",
    "Mixed_BCD_Base_Source_assoc", "Mixed_BCD_Base_Source_comp", "Mixed_BCD_Base_Source_compId",
    "Mixed_BCD_Base_Source_eqCongr", "Mixed_BCD_Base_Source_eqTrans", "Mixed_BCD_Base_Source_id",
    "Mixed_BCD_Base_Source_idComp", "Mixed_BCD_Base_Target_Category", "Mixed_BCD_Base_Target_Hom",
    "Mixed_BCD_Base_Target_assoc", "Mixed_BCD_Base_Target_comp", "Mixed_BCD_Base_Target_compId",
    "Mixed_BCD_Base_Target_eqCongr", "Mixed_BCD_Base_Target_eqTrans", "Mixed_BCD_Base_Target_id",
    "Mixed_BCD_Base_Target_idComp", "Mixed_BCD_Base_compFunctor", "Mixed_BCD_Composite_NatTrans",
    "Mixed_BCD_Composite_natApp", "Mixed_BCD_Composite_naturality", "Mixed_BCD_First_NatTrans",
    "Mixed_BCD_First_natApp", "Mixed_BCD_First_naturality", "Mixed_BCD_Second_NatTrans", "Mixed_BCD_Second_natApp",
    "Mixed_BCD_Second_naturality", "Mixed_BCD_targetEqSymm", "Mixed_BCD_whiskerLeft", "Mixed_BCD_whiskerRight",
    "Mixed_Middle1_Category", "Mixed_Middle1_Hom", "Mixed_Middle1_assoc", "Mixed_Middle1_comp",
    "Mixed_Middle1_compId", "Mixed_Middle1_eqCongr", "Mixed_Middle1_eqTrans", "Mixed_Middle1_id",
    "Mixed_Middle1_idComp", "Mixed_Middle2_Category", "Mixed_Middle2_Hom", "Mixed_Middle2_assoc",
    "Mixed_Middle2_comp", "Mixed_Middle2_compId", "Mixed_Middle2_eqCongr", "Mixed_Middle2_eqTrans",
    "Mixed_Middle2_id", "Mixed_Middle2_idComp", "Mixed_Source_Category", "Mixed_Source_Hom", "Mixed_Source_assoc",
    "Mixed_Source_comp", "Mixed_Source_compId", "Mixed_Source_eqCongr", "Mixed_Source_eqTrans", "Mixed_Source_id",
    "Mixed_Source_idComp", "Mixed_Target_Category", "Mixed_Target_Hom", "Mixed_Target_assoc", "Mixed_Target_comp",
    "Mixed_Target_compId", "Mixed_Target_eqCongr", "Mixed_Target_eqTrans", "Mixed_Target_id", "Mixed_Target_idComp",
    "Mixed_whiskerCommute", "Mixed_whiskerLeftComp", "Mixed_whiskerRightComp", "PairEndMiddle2", "PairEndTarget",
    "Right", "Run_ABC_Base_Composite_Functor", "Run_ABC_Base_Composite_eqCongr", "Run_ABC_Base_Composite_functorMap",
    "Run_ABC_Base_Composite_functorMapComp", "Run_ABC_Base_Composite_functorMapId",
    "Run_ABC_Base_Composite_functorObj", "Run_ABC_Base_First_Functor", "Run_ABC_Base_First_eqCongr",
    "Run_ABC_Base_First_functorMap", "Run_ABC_Base_First_functorMapComp", "Run_ABC_Base_First_functorMapId",
    "Run_ABC_Base_First_functorObj", "Run_ABC_Base_Middle_Category", "Run_ABC_Base_Middle_Hom",
    "Run_ABC_Base_Middle_assoc", "Run_ABC_Base_Middle_comp", "Run_ABC_Base_Middle_compId",
    "Run_ABC_Base_Middle_eqCongr", "Run_ABC_Base_Middle_eqTrans", "Run_ABC_Base_Middle_id",
    "Run_ABC_Base_Middle_idComp", "Run_ABC_Base_Second_Functor", "Run_ABC_Base_Second_eqCongr",
    "Run_ABC_Base_Second_functorMap", "Run_ABC_Base_Second_functorMapComp", "Run_ABC_Base_Second_functorMapId",
    "Run_ABC_Base_Second_functorObj", "Run_ABC_Base_Source_Category", "Run_ABC_Base_Source_Hom",
    "Run_ABC_Base_Source_assoc", "Run_ABC_Base_Source_comp", "Run_ABC_Base_Source_compId",
    "Run_ABC_Base_Source_eqCongr", "Run_ABC_Base_Source_eqTrans", "Run_ABC_Base_Source_id",
    "Run_ABC_Base_Source_idComp", "Run_ABC_Base_Target_Category", "Run_ABC_Base_Target_Hom",
    "Run_ABC_Base_Target_assoc", "Run_ABC_Base_Target_comp", "Run_ABC_Base_Target_compId",
    "Run_ABC_Base_Target_eqCongr", "Run_ABC_Base_Target_eqTrans", "Run_ABC_Base_Target_id",
    "Run_ABC_Base_Target_idComp", "Run_ABC_Base_compFunctor", "Run_ABC_Composite_NatTrans",
    "Run_ABC_Composite_natApp", "Run_ABC_Composite_naturality", "Run_ABC_First_NatTrans", "Run_ABC_First_natApp",
    "Run_ABC_First_naturality", "Run_ABC_Second_NatTrans", "Run_ABC_Second_natApp", "Run_ABC_Second_naturality",
    "Run_ABC_targetEqSymm", "Run_ABC_whiskerLeft", "Run_ABC_whiskerRight", "Run_ABD_Base_Composite_Functor",
    "Run_ABD_Base_Composite_eqCongr", "Run_ABD_Base_Composite_functorMap", "Run_ABD_Base_Composite_functorMapComp",
    "Run_ABD_Base_Composite_functorMapId", "Run_ABD_Base_Composite_functorObj", "Run_ABD_Base_First_Functor",
    "Run_ABD_Base_First_eqCongr", "Run_ABD_Base_First_functorMap", "Run_ABD_Base_First_functorMapComp",
    "Run_ABD_Base_First_functorMapId", "Run_ABD_Base_First_functorObj", "Run_ABD_Base_Middle_Category",
    "Run_ABD_Base_Middle_Hom", "Run_ABD_Base_Middle_assoc", "Run_ABD_Base_Middle_comp", "Run_ABD_Base_Middle_compId",
    "Run_ABD_Base_Middle_eqCongr", "Run_ABD_Base_Middle_eqTrans", "Run_ABD_Base_Middle_id",
    "Run_ABD_Base_Middle_idComp", "Run_ABD_Base_Second_Functor", "Run_ABD_Base_Second_eqCongr",
    "Run_ABD_Base_Second_functorMap", "Run_ABD_Base_Second_functorMapComp", "Run_ABD_Base_Second_functorMapId",
    "Run_ABD_Base_Second_functorObj", "Run_ABD_Base_Source_Category", "Run_ABD_Base_Source_Hom",
    "Run_ABD_Base_Source_assoc", "Run_ABD_Base_Source_comp", "Run_ABD_Base_Source_compId",
    "Run_ABD_Base_Source_eqCongr", "Run_ABD_Base_Source_eqTrans", "Run_ABD_Base_Source_id",
    "Run_ABD_Base_Source_idComp", "Run_ABD_Base_Target_Category", "Run_ABD_Base_Target_Hom",
    "Run_ABD_Base_Target_assoc", "Run_ABD_Base_Target_comp", "Run_ABD_Base_Target_compId",
    "Run_ABD_Base_Target_eqCongr", "Run_ABD_Base_Target_eqTrans", "Run_ABD_Base_Target_id",
    "Run_ABD_Base_Target_idComp", "Run_ABD_Base_compFunctor", "Run_ABD_Composite_NatTrans",
    "Run_ABD_Composite_natApp", "Run_ABD_Composite_naturality", "Run_ABD_First_NatTrans", "Run_ABD_First_natApp",
    "Run_ABD_First_naturality", "Run_ABD_Second_NatTrans", "Run_ABD_Second_natApp", "Run_ABD_Second_naturality",
    "Run_ABD_targetEqSymm", "Run_ABD_whiskerLeft", "Run_ABD_whiskerRight", "Run_ACD_Base_Composite_Functor",
    "Run_ACD_Base_Composite_eqCongr", "Run_ACD_Base_Composite_functorMap", "Run_ACD_Base_Composite_functorMapComp",
    "Run_ACD_Base_Composite_functorMapId", "Run_ACD_Base_Composite_functorObj", "Run_ACD_Base_First_Functor",
    "Run_ACD_Base_First_eqCongr", "Run_ACD_Base_First_functorMap", "Run_ACD_Base_First_functorMapComp",
    "Run_ACD_Base_First_functorMapId", "Run_ACD_Base_First_functorObj", "Run_ACD_Base_Middle_Category",
    "Run_ACD_Base_Middle_Hom", "Run_ACD_Base_Middle_assoc", "Run_ACD_Base_Middle_comp", "Run_ACD_Base_Middle_compId",
    "Run_ACD_Base_Middle_eqCongr", "Run_ACD_Base_Middle_eqTrans", "Run_ACD_Base_Middle_id",
    "Run_ACD_Base_Middle_idComp", "Run_ACD_Base_Second_Functor", "Run_ACD_Base_Second_eqCongr",
    "Run_ACD_Base_Second_functorMap", "Run_ACD_Base_Second_functorMapComp", "Run_ACD_Base_Second_functorMapId",
    "Run_ACD_Base_Second_functorObj", "Run_ACD_Base_Source_Category", "Run_ACD_Base_Source_Hom",
    "Run_ACD_Base_Source_assoc", "Run_ACD_Base_Source_comp", "Run_ACD_Base_Source_compId",
    "Run_ACD_Base_Source_eqCongr", "Run_ACD_Base_Source_eqTrans", "Run_ACD_Base_Source_id",
    "Run_ACD_Base_Source_idComp", "Run_ACD_Base_Target_Category", "Run_ACD_Base_Target_Hom",
    "Run_ACD_Base_Target_assoc", "Run_ACD_Base_Target_comp", "Run_ACD_Base_Target_compId",
    "Run_ACD_Base_Target_eqCongr", "Run_ACD_Base_Target_eqTrans", "Run_ACD_Base_Target_id",
    "Run_ACD_Base_Target_idComp", "Run_ACD_Base_compFunctor", "Run_ACD_Composite_NatTrans",
    "Run_ACD_Composite_natApp", "Run_ACD_Composite_naturality", "Run_ACD_First_NatTrans", "Run_ACD_First_natApp",
    "Run_ACD_First_naturality", "Run_ACD_Second_NatTrans", "Run_ACD_Second_natApp", "Run_ACD_Second_naturality",
    "Run_ACD_targetEqSymm", "Run_ACD_whiskerLeft", "Run_ACD_whiskerRight", "Run_BCD_Base_Composite_Functor",
    "Run_BCD_Base_Composite_eqCongr", "Run_BCD_Base_Composite_functorMap", "Run_BCD_Base_Composite_functorMapComp",
    "Run_BCD_Base_Composite_functorMapId", "Run_BCD_Base_Composite_functorObj", "Run_BCD_Base_First_Functor",
    "Run_BCD_Base_First_eqCongr", "Run_BCD_Base_First_functorMap", "Run_BCD_Base_First_functorMapComp",
    "Run_BCD_Base_First_functorMapId", "Run_BCD_Base_First_functorObj", "Run_BCD_Base_Middle_Category",
    "Run_BCD_Base_Middle_Hom", "Run_BCD_Base_Middle_assoc", "Run_BCD_Base_Middle_comp", "Run_BCD_Base_Middle_compId",
    "Run_BCD_Base_Middle_eqCongr", "Run_BCD_Base_Middle_eqTrans", "Run_BCD_Base_Middle_id",
    "Run_BCD_Base_Middle_idComp", "Run_BCD_Base_Second_Functor", "Run_BCD_Base_Second_eqCongr",
    "Run_BCD_Base_Second_functorMap", "Run_BCD_Base_Second_functorMapComp", "Run_BCD_Base_Second_functorMapId",
    "Run_BCD_Base_Second_functorObj", "Run_BCD_Base_Source_Category", "Run_BCD_Base_Source_Hom",
    "Run_BCD_Base_Source_assoc", "Run_BCD_Base_Source_comp", "Run_BCD_Base_Source_compId",
    "Run_BCD_Base_Source_eqCongr", "Run_BCD_Base_Source_eqTrans", "Run_BCD_Base_Source_id",
    "Run_BCD_Base_Source_idComp", "Run_BCD_Base_Target_Category", "Run_BCD_Base_Target_Hom",
    "Run_BCD_Base_Target_assoc", "Run_BCD_Base_Target_comp", "Run_BCD_Base_Target_compId",
    "Run_BCD_Base_Target_eqCongr", "Run_BCD_Base_Target_eqTrans", "Run_BCD_Base_Target_id",
    "Run_BCD_Base_Target_idComp", "Run_BCD_Base_compFunctor", "Run_BCD_Composite_NatTrans",
    "Run_BCD_Composite_natApp", "Run_BCD_Composite_naturality", "Run_BCD_First_NatTrans", "Run_BCD_First_natApp",
    "Run_BCD_First_naturality", "Run_BCD_Second_NatTrans", "Run_BCD_Second_natApp", "Run_BCD_Second_naturality",
    "Run_BCD_targetEqSymm", "Run_BCD_whiskerLeft", "Run_BCD_whiskerRight", "Run_Middle1_Category", "Run_Middle1_Hom",
    "Run_Middle1_assoc", "Run_Middle1_comp", "Run_Middle1_compId", "Run_Middle1_eqCongr", "Run_Middle1_eqTrans",
    "Run_Middle1_id", "Run_Middle1_idComp", "Run_Middle2_Category", "Run_Middle2_Hom", "Run_Middle2_assoc",
    "Run_Middle2_comp", "Run_Middle2_compId", "Run_Middle2_eqCongr", "Run_Middle2_eqTrans", "Run_Middle2_id",
    "Run_Middle2_idComp", "Run_Source_Category", "Run_Source_Hom", "Run_Source_assoc", "Run_Source_comp",
    "Run_Source_compId", "Run_Source_eqCongr", "Run_Source_eqTrans", "Run_Source_id", "Run_Source_idComp",
    "Run_Target_Category", "Run_Target_Hom", "Run_Target_assoc", "Run_Target_comp", "Run_Target_compId",
    "Run_Target_eqCongr", "Run_Target_eqTrans", "Run_Target_id", "Run_Target_idComp", "Run_whiskerCommute",
    "Run_whiskerLeftComp", "Run_whiskerRightComp", "SeparateTarget_Category", "SeparateTarget_Hom",
    "SeparateTarget_assoc", "SeparateTarget_comp", "SeparateTarget_compId", "SeparateTarget_eqCongr",
    "SeparateTarget_eqTrans", "SeparateTarget_id", "SeparateTarget_idComp", "Shift", "Source_Category", "Source_Hom",
    "Source_assoc", "Source_comp", "Source_compId", "Source_eqCongr", "Source_eqTrans", "Source_id", "Source_idComp",
    "Stretch", "Target_Category", "Target_Hom", "Target_assoc", "Target_comp", "Target_compId", "Target_eqCongr",
    "Target_eqTrans", "Target_id", "Target_idComp", "Wide_ABC_Base_Composite_Functor",
    "Wide_ABC_Base_Composite_eqCongr", "Wide_ABC_Base_Composite_functorMap", "Wide_ABC_Base_Composite_functorMapComp",
    "Wide_ABC_Base_Composite_functorMapId", "Wide_ABC_Base_Composite_functorObj", "Wide_ABC_Base_First_Functor",
    "Wide_ABC_Base_First_eqCongr", "Wide_ABC_Base_First_functorMap", "Wide_ABC_Base_First_functorMapComp",
    "Wide_ABC_Base_First_functorMapId", "Wide_ABC_Base_First_functorObj", "Wide_ABC_Base_Middle_Category",
    "Wide_ABC_Base_Middle_Hom", "Wide_ABC_Base_Middle_assoc", "Wide_ABC_Base_Middle_comp",
    "Wide_ABC_Base_Middle_compId", "Wide_ABC_Base_Middle_eqCongr", "Wide_ABC_Base_Middle_eqTrans",
    "Wide_ABC_Base_Middle_id", "Wide_ABC_Base_Middle_idComp", "Wide_ABC_Base_Second_Functor",
    "Wide_ABC_Base_Second_eqCongr", "Wide_ABC_Base_Second_functorMap", "Wide_ABC_Base_Second_functorMapComp",
    "Wide_ABC_Base_Second_functorMapId", "Wide_ABC_Base_Second_functorObj", "Wide_ABC_Base_Source_Category",
    "Wide_ABC_Base_Source_Hom", "Wide_ABC_Base_Source_assoc", "Wide_ABC_Base_Source_comp",
    "Wide_ABC_Base_Source_compId", "Wide_ABC_Base_Source_eqCongr", "Wide_ABC_Base_Source_eqTrans",
    "Wide_ABC_Base_Source_id", "Wide_ABC_Base_Source_idComp", "Wide_ABC_Base_Target_Category",
    "Wide_ABC_Base_Target_Hom", "Wide_ABC_Base_Target_assoc", "Wide_ABC_Base_Target_comp",
    "Wide_ABC_Base_Target_compId", "Wide_ABC_Base_Target_eqCongr", "Wide_ABC_Base_Target_eqTrans",
    "Wide_ABC_Base_Target_id", "Wide_ABC_Base_Target_idComp", "Wide_ABC_Base_compFunctor",
    "Wide_ABC_Composite_NatTrans", "Wide_ABC_Composite_natApp", "Wide_ABC_Composite_naturality",
    "Wide_ABC_First_NatTrans", "Wide_ABC_First_natApp", "Wide_ABC_First_naturality", "Wide_ABC_Second_NatTrans",
    "Wide_ABC_Second_natApp", "Wide_ABC_Second_naturality", "Wide_ABC_targetEqSymm", "Wide_ABC_whiskerLeft",
    "Wide_ABC_whiskerRight", "Wide_ABD_Base_Composite_Functor", "Wide_ABD_Base_Composite_eqCongr",
    "Wide_ABD_Base_Composite_functorMap", "Wide_ABD_Base_Composite_functorMapComp",
    "Wide_ABD_Base_Composite_functorMapId", "Wide_ABD_Base_Composite_functorObj", "Wide_ABD_Base_First_Functor",
    "Wide_ABD_Base_First_eqCongr", "Wide_ABD_Base_First_functorMap", "Wide_ABD_Base_First_functorMapComp",
    "Wide_ABD_Base_First_functorMapId", "Wide_ABD_Base_First_functorObj", "Wide_ABD_Base_Middle_Category",
    "Wide_ABD_Base_Middle_Hom", "Wide_ABD_Base_Middle_assoc", "Wide_ABD_Base_Middle_comp",
    "Wide_ABD_Base_Middle_compId", "Wide_ABD_Base_Middle_eqCongr", "Wide_ABD_Base_Middle_eqTrans",
    "Wide_ABD_Base_Middle_id", "Wide_ABD_Base_Middle_idComp", "Wide_ABD_Base_Second_Functor",
    "Wide_ABD_Base_Second_eqCongr", "Wide_ABD_Base_Second_functorMap", "Wide_ABD_Base_Second_functorMapComp",
    "Wide_ABD_Base_Second_functorMapId", "Wide_ABD_Base_Second_functorObj", "Wide_ABD_Base_Source_Category",
    "Wide_ABD_Base_Source_Hom", "Wide_ABD_Base_Source_assoc", "Wide_ABD_Base_Source_comp",
    "Wide_ABD_Base_Source_compId", "Wide_ABD_Base_Source_eqCongr", "Wide_ABD_Base_Source_eqTrans",
    "Wide_ABD_Base_Source_id", "Wide_ABD_Base_Source_idComp", "Wide_ABD_Base_Target_Category",
    "Wide_ABD_Base_Target_Hom", "Wide_ABD_Base_Target_assoc", "Wide_ABD_Base_Target_comp",
    "Wide_ABD_Base_Target_compId", "Wide_ABD_Base_Target_eqCongr", "Wide_ABD_Base_Target_eqTrans",
    "Wide_ABD_Base_Target_id", "Wide_ABD_Base_Target_idComp", "Wide_ABD_Base_compFunctor",
    "Wide_ABD_Composite_NatTrans", "Wide_ABD_Composite_natApp", "Wide_ABD_Composite_naturality",
    "Wide_ABD_First_NatTrans", "Wide_ABD_First_natApp", "Wide_ABD_First_naturality", "Wide_ABD_Second_NatTrans",
    "Wide_ABD_Second_natApp", "Wide_ABD_Second_naturality", "Wide_ABD_targetEqSymm", "Wide_ABD_whiskerLeft",
    "Wide_ABD_whiskerRight", "Wide_ACD_Base_Composite_Functor", "Wide_ACD_Base_Composite_eqCongr",
    "Wide_ACD_Base_Composite_functorMap", "Wide_ACD_Base_Composite_functorMapComp",
    "Wide_ACD_Base_Composite_functorMapId", "Wide_ACD_Base_Composite_functorObj", "Wide_ACD_Base_First_Functor",
    "Wide_ACD_Base_First_eqCongr", "Wide_ACD_Base_First_functorMap", "Wide_ACD_Base_First_functorMapComp",
    "Wide_ACD_Base_First_functorMapId", "Wide_ACD_Base_First_functorObj", "Wide_ACD_Base_Middle_Category",
    "Wide_ACD_Base_Middle_Hom", "Wide_ACD_Base_Middle_assoc", "Wide_ACD_Base_Middle_comp",
    "Wide_ACD_Base_Middle_compId", "Wide_ACD_Base_Middle_eqCongr", "Wide_ACD_Base_Middle_eqTrans",
    "Wide_ACD_Base_Middle_id", "Wide_ACD_Base_Middle_idComp", "Wide_ACD_Base_Second_Functor",
    "Wide_ACD_Base_Second_eqCongr", "Wide_ACD_Base_Second_functorMap", "Wide_ACD_Base_Second_functorMapComp",
    "Wide_ACD_Base_Second_functorMapId", "Wide_ACD_Base_Second_functorObj", "Wide_ACD_Base_Source_Category",
    "Wide_ACD_Base_Source_Hom", "Wide_ACD_Base_Source_assoc", "Wide_ACD_Base_Source_comp",
    "Wide_ACD_Base_Source_compId", "Wide_ACD_Base_Source_eqCongr", "Wide_ACD_Base_Source_eqTrans",
    "Wide_ACD_Base_Source_id", "Wide_ACD_Base_Source_idComp", "Wide_ACD_Base_Target_Category",
    "Wide_ACD_Base_Target_Hom", "Wide_ACD_Base_Target_assoc", "Wide_ACD_Base_Target_comp",
    "Wide_ACD_Base_Target_compId", "Wide_ACD_Base_Target_eqCongr", "Wide_ACD_Base_Target_eqTrans",
    "Wide_ACD_Base_Target_id", "Wide_ACD_Base_Target_idComp", "Wide_ACD_Base_compFunctor",
    "Wide_ACD_Composite_NatTrans", "Wide_ACD_Composite_natApp", "Wide_ACD_Composite_naturality",
    "Wide_ACD_First_NatTrans", "Wide_ACD_First_natApp", "Wide_ACD_First_naturality", "Wide_ACD_Second_NatTrans",
    "Wide_ACD_Second_natApp", "Wide_ACD_Second_naturality", "Wide_ACD_targetEqSymm", "Wide_ACD_whiskerLeft",
    "Wide_ACD_whiskerRight", "Wide_BCD_Base_Composite_Functor", "Wide_BCD_Base_Composite_eqCongr",
    "Wide_BCD_Base_Composite_functorMap", "Wide_BCD_Base_Composite_functorMapComp",
    "Wide_BCD_Base_Composite_functorMapId", "Wide_BCD_Base_Composite_functorObj", "Wide_BCD_Base_First_Functor",
    "Wide_BCD_Base_First_eqCongr", "Wide_BCD_Base_First_functorMap", "Wide_BCD_Base_First_functorMapComp",
    "Wide_BCD_Base_First_functorMapId", "Wide_BCD_Base_First_functorObj", "Wide_BCD_Base_Middle_Category",
    "Wide_BCD_Base_Middle_Hom", "Wide_BCD_Base_Middle_assoc", "Wide_BCD_Base_Middle_comp",
    "Wide_BCD_Base_Middle_compId", "Wide_BCD_Base_Middle_eqCongr", "Wide_BCD_Base_Middle_eqTrans",
    "Wide_BCD_Base_Middle_id", "Wide_BCD_Base_Middle_idComp", "Wide_BCD_Base_Second_Functor",
    "Wide_BCD_Base_Second_eqCongr", "Wide_BCD_Base_Second_functorMap", "Wide_BCD_Base_Second_functorMapComp",
    "Wide_BCD_Base_Second_functorMapId", "Wide_BCD_Base_Second_functorObj", "Wide_BCD_Base_Source_Category",
    "Wide_BCD_Base_Source_Hom", "Wide_BCD_Base_Source_assoc", "Wide_BCD_Base_Source_comp",
    "Wide_BCD_Base_Source_compId", "Wide_BCD_Base_Source_eqCongr", "Wide_BCD_Base_Source_eqTrans",
    "Wide_BCD_Base_Source_id", "Wide_BCD_Base_Source_idComp", "Wide_BCD_Base_Target_Category",
    "Wide_BCD_Base_Target_Hom", "Wide_BCD_Base_Target_assoc", "Wide_BCD_Base_Target_comp",
    "Wide_BCD_Base_Target_compId", "Wide_BCD_Base_Target_eqCongr", "Wide_BCD_Base_Target_eqTrans",
    "Wide_BCD_Base_Target_id", "Wide_BCD_Base_Target_idComp", "Wide_BCD_Base_compFunctor",
    "Wide_BCD_Composite_NatTrans", "Wide_BCD_Composite_natApp", "Wide_BCD_Composite_naturality",
    "Wide_BCD_First_NatTrans", "Wide_BCD_First_natApp", "Wide_BCD_First_naturality", "Wide_BCD_Second_NatTrans",
    "Wide_BCD_Second_natApp", "Wide_BCD_Second_naturality", "Wide_BCD_targetEqSymm", "Wide_BCD_whiskerLeft",
    "Wide_BCD_whiskerRight", "Wide_Middle1_Category", "Wide_Middle1_Hom", "Wide_Middle1_assoc", "Wide_Middle1_comp",
    "Wide_Middle1_compId", "Wide_Middle1_eqCongr", "Wide_Middle1_eqTrans", "Wide_Middle1_id", "Wide_Middle1_idComp",
    "Wide_Middle2_Category", "Wide_Middle2_Hom", "Wide_Middle2_assoc", "Wide_Middle2_comp", "Wide_Middle2_compId",
    "Wide_Middle2_eqCongr", "Wide_Middle2_eqTrans", "Wide_Middle2_id", "Wide_Middle2_idComp", "Wide_Source_Category",
    "Wide_Source_Hom", "Wide_Source_assoc", "Wide_Source_comp", "Wide_Source_compId", "Wide_Source_eqCongr",
    "Wide_Source_eqTrans", "Wide_Source_id", "Wide_Source_idComp", "Wide_Target_Category", "Wide_Target_Hom",
    "Wide_Target_assoc", "Wide_Target_comp", "Wide_Target_compId", "Wide_Target_eqCongr", "Wide_Target_eqTrans",
    "Wide_Target_id", "Wide_Target_idComp", "Wide_whiskerCommute", "Wide_whiskerLeftComp", "Wide_whiskerRightComp",
    "certified", "componentLaw", "componentLawM1M2", "componentLawM2T", "components", "leftLaw", "leftLeftAt",
    "leftLhs", "leftRhs", "leftRightAt", "mixedLaw", "mixedLeftAt", "mixedLhs", "mixedRhs", "mixedRightAt",
    "pairCode", "pathCongr", "pathTrans", "rightLaw", "rightLeftAt", "rightLhs", "rightRhs", "rightRightAt", "swap"
};

/* expected_families() (bend2/tests/prelude_iterated_whiskering.bend line 15, 14 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "Middle1", "Middle2", "Mixed_Middle1", "Mixed_Middle2", "Mixed_Source", "Mixed_Target", "Path", "SeparateTarget",
    "Source", "Target", "Wide_Middle1", "Wide_Middle2", "Wide_Source", "Wide_Target"
};

/* negatives() (bend2/tests/prelude_iterated_whiskering.bend line 18, 6 names) */
static const char *const NEGATIVES[] = {
    "false-components", "wrong-precomposition-order", "missing-postcomposition", "missing-precomposition",
    "nominal-target", "incompatible-endpoints"
};

/* values() (18 Program.Probe rows) */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  probe(heap, &out, "rightLeftAt", 0, 32);
  probe(heap, &out, "rightRightAt", 0, 32);
  probe(heap, &out, "leftLeftAt", 0, 1314);
  probe(heap, &out, "leftRightAt", 0, 1314);
  probe(heap, &out, "mixedLeftAt", 0, 1317);
  probe(heap, &out, "mixedRightAt", 0, 1317);
  probe(heap, &out, "rightLeftAt", 37, 180);
  probe(heap, &out, "rightRightAt", 37, 180);
  probe(heap, &out, "leftLeftAt", 37, 5051);
  probe(heap, &out, "leftRightAt", 37, 5051);
  probe(heap, &out, "mixedLeftAt", 37, 5054);
  probe(heap, &out, "mixedRightAt", 37, 5054);
  probe(heap, &out, "rightLeftAt", 41, 196);
  probe(heap, &out, "rightRightAt", 41, 196);
  probe(heap, &out, "leftLeftAt", 41, 5455);
  probe(heap, &out, "leftRightAt", 41, 5455);
  probe(heap, &out, "mixedLeftAt", 41, 5458);
  probe(heap, &out, "mixedRightAt", 41, 5458);
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-whiskering.mech",
    "prelude/cat/iterated-whiskering.mech", "test/fixtures/prelude/iterated-whiskering.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/iterated-whiskering-runtime.mech"
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
  Vec full;
  RELATIONAL_TRY(relational_read_all(heap, root, &templates_paths, &templates));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, cat(heap, templates, str_lit("\n")), fixture), &parsed));
  RELATIONAL_TRY(program_staged(heap, templates, fixture, 6, &checked));
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  globals = relational_globals(checked);
  rows = values(heap);
  RELATIONAL_TRY(program_probes(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  full = vec_new(heap->arena);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/iterated-whiskering"), &negatives, globals,
                               negatives_digest(0, 256, &full, false, false)));
  *fields = str_lit("entries=902 families=14 computations=18 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-ITERATED-WHISKERING", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-ITERATED-WHISKERING"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
