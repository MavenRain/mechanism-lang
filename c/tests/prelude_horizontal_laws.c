/* Port of tests/prelude_horizontal_laws.bend (41 lines): the horizontal laws prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-HORIZONTAL-LAWS-OK fields or PRELUDE-HORIZONTAL-LAWS-FAIL message. Same definitions in the same order. */
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

/* expected_entries() (tests/prelude_horizontal_laws.bend line 12, 1205 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "Constant", "Delta", "End", "External_Functor", "External_Source_Category",
    "External_Source_Hom", "External_Source_assoc", "External_Source_comp", "External_Source_compId",
    "External_Source_eqCongr", "External_Source_eqTrans", "External_Source_id", "External_Source_idComp",
    "External_Target_Category", "External_Target_Hom", "External_Target_assoc", "External_Target_comp",
    "External_Target_compId", "External_Target_eqCongr", "External_Target_eqTrans", "External_Target_id",
    "External_Target_idComp", "External_eqCongr", "External_functorMap", "External_functorMapComp",
    "External_functorMapId", "External_functorObj", "FH", "FirstIdentityAlpha", "GI", "Gamma", "Horizontal",
    "Identity", "IndexedEnd", "Laws_NatTransEqAt", "Laws_W_Composite_NatTransEq", "Laws_W_Composite_Ops_Base_Functor",
    "Laws_W_Composite_Ops_Base_Source_Category", "Laws_W_Composite_Ops_Base_Source_Hom",
    "Laws_W_Composite_Ops_Base_Source_assoc", "Laws_W_Composite_Ops_Base_Source_comp",
    "Laws_W_Composite_Ops_Base_Source_compId", "Laws_W_Composite_Ops_Base_Source_eqCongr",
    "Laws_W_Composite_Ops_Base_Source_eqTrans", "Laws_W_Composite_Ops_Base_Source_id",
    "Laws_W_Composite_Ops_Base_Source_idComp", "Laws_W_Composite_Ops_Base_Target_Category",
    "Laws_W_Composite_Ops_Base_Target_Hom", "Laws_W_Composite_Ops_Base_Target_assoc",
    "Laws_W_Composite_Ops_Base_Target_comp", "Laws_W_Composite_Ops_Base_Target_compId",
    "Laws_W_Composite_Ops_Base_Target_eqCongr", "Laws_W_Composite_Ops_Base_Target_eqTrans",
    "Laws_W_Composite_Ops_Base_Target_id", "Laws_W_Composite_Ops_Base_Target_idComp",
    "Laws_W_Composite_Ops_Base_eqCongr", "Laws_W_Composite_Ops_Base_functorMap",
    "Laws_W_Composite_Ops_Base_functorMapComp", "Laws_W_Composite_Ops_Base_functorMapId",
    "Laws_W_Composite_Ops_Base_functorObj", "Laws_W_Composite_Ops_NatTrans", "Laws_W_Composite_Ops_idNat",
    "Laws_W_Composite_Ops_natApp", "Laws_W_Composite_Ops_naturality", "Laws_W_Composite_Ops_targetEqSymm",
    "Laws_W_Composite_Ops_vcomp", "Laws_W_Composite_Ops_vcompLaw", "Laws_W_Composite_eqRefl",
    "Laws_W_Composite_eqSymm", "Laws_W_Composite_eqTrans", "Laws_W_Composite_idVcomp", "Laws_W_Composite_vcompAssoc",
    "Laws_W_Composite_vcompCongr", "Laws_W_Composite_vcompId", "Laws_W_First_NatTransEq",
    "Laws_W_First_Ops_Base_Functor", "Laws_W_First_Ops_Base_Source_Category", "Laws_W_First_Ops_Base_Source_Hom",
    "Laws_W_First_Ops_Base_Source_assoc", "Laws_W_First_Ops_Base_Source_comp", "Laws_W_First_Ops_Base_Source_compId",
    "Laws_W_First_Ops_Base_Source_eqCongr", "Laws_W_First_Ops_Base_Source_eqTrans", "Laws_W_First_Ops_Base_Source_id",
    "Laws_W_First_Ops_Base_Source_idComp", "Laws_W_First_Ops_Base_Target_Category",
    "Laws_W_First_Ops_Base_Target_Hom", "Laws_W_First_Ops_Base_Target_assoc", "Laws_W_First_Ops_Base_Target_comp",
    "Laws_W_First_Ops_Base_Target_compId", "Laws_W_First_Ops_Base_Target_eqCongr",
    "Laws_W_First_Ops_Base_Target_eqTrans", "Laws_W_First_Ops_Base_Target_id", "Laws_W_First_Ops_Base_Target_idComp",
    "Laws_W_First_Ops_Base_eqCongr", "Laws_W_First_Ops_Base_functorMap", "Laws_W_First_Ops_Base_functorMapComp",
    "Laws_W_First_Ops_Base_functorMapId", "Laws_W_First_Ops_Base_functorObj", "Laws_W_First_Ops_NatTrans",
    "Laws_W_First_Ops_idNat", "Laws_W_First_Ops_natApp", "Laws_W_First_Ops_naturality",
    "Laws_W_First_Ops_targetEqSymm", "Laws_W_First_Ops_vcomp", "Laws_W_First_Ops_vcompLaw", "Laws_W_First_eqRefl",
    "Laws_W_First_eqSymm", "Laws_W_First_eqTrans", "Laws_W_First_idVcomp", "Laws_W_First_vcompAssoc",
    "Laws_W_First_vcompCongr", "Laws_W_First_vcompId", "Laws_W_Ops_Compose_Base_Composite_Functor",
    "Laws_W_Ops_Compose_Base_Composite_eqCongr", "Laws_W_Ops_Compose_Base_Composite_functorMap",
    "Laws_W_Ops_Compose_Base_Composite_functorMapComp", "Laws_W_Ops_Compose_Base_Composite_functorMapId",
    "Laws_W_Ops_Compose_Base_Composite_functorObj", "Laws_W_Ops_Compose_Base_First_Functor",
    "Laws_W_Ops_Compose_Base_First_eqCongr", "Laws_W_Ops_Compose_Base_First_functorMap",
    "Laws_W_Ops_Compose_Base_First_functorMapComp", "Laws_W_Ops_Compose_Base_First_functorMapId",
    "Laws_W_Ops_Compose_Base_First_functorObj", "Laws_W_Ops_Compose_Base_Middle_Category",
    "Laws_W_Ops_Compose_Base_Middle_Hom", "Laws_W_Ops_Compose_Base_Middle_assoc",
    "Laws_W_Ops_Compose_Base_Middle_comp", "Laws_W_Ops_Compose_Base_Middle_compId",
    "Laws_W_Ops_Compose_Base_Middle_eqCongr", "Laws_W_Ops_Compose_Base_Middle_eqTrans",
    "Laws_W_Ops_Compose_Base_Middle_id", "Laws_W_Ops_Compose_Base_Middle_idComp",
    "Laws_W_Ops_Compose_Base_Second_Functor", "Laws_W_Ops_Compose_Base_Second_eqCongr",
    "Laws_W_Ops_Compose_Base_Second_functorMap", "Laws_W_Ops_Compose_Base_Second_functorMapComp",
    "Laws_W_Ops_Compose_Base_Second_functorMapId", "Laws_W_Ops_Compose_Base_Second_functorObj",
    "Laws_W_Ops_Compose_Base_Source_Category", "Laws_W_Ops_Compose_Base_Source_Hom",
    "Laws_W_Ops_Compose_Base_Source_assoc", "Laws_W_Ops_Compose_Base_Source_comp",
    "Laws_W_Ops_Compose_Base_Source_compId", "Laws_W_Ops_Compose_Base_Source_eqCongr",
    "Laws_W_Ops_Compose_Base_Source_eqTrans", "Laws_W_Ops_Compose_Base_Source_id",
    "Laws_W_Ops_Compose_Base_Source_idComp", "Laws_W_Ops_Compose_Base_Target_Category",
    "Laws_W_Ops_Compose_Base_Target_Hom", "Laws_W_Ops_Compose_Base_Target_assoc",
    "Laws_W_Ops_Compose_Base_Target_comp", "Laws_W_Ops_Compose_Base_Target_compId",
    "Laws_W_Ops_Compose_Base_Target_eqCongr", "Laws_W_Ops_Compose_Base_Target_eqTrans",
    "Laws_W_Ops_Compose_Base_Target_id", "Laws_W_Ops_Compose_Base_Target_idComp",
    "Laws_W_Ops_Compose_Base_compFunctor", "Laws_W_Ops_Compose_Composite_NatTrans",
    "Laws_W_Ops_Compose_Composite_natApp", "Laws_W_Ops_Compose_Composite_naturality",
    "Laws_W_Ops_Compose_First_NatTrans", "Laws_W_Ops_Compose_First_natApp", "Laws_W_Ops_Compose_First_naturality",
    "Laws_W_Ops_Compose_Second_NatTrans", "Laws_W_Ops_Compose_Second_natApp", "Laws_W_Ops_Compose_Second_naturality",
    "Laws_W_Ops_Compose_targetEqSymm", "Laws_W_Ops_Compose_whiskerLeft", "Laws_W_Ops_Compose_whiskerRight",
    "Laws_W_Ops_Composite_Base_Functor", "Laws_W_Ops_Composite_Base_Source_Category",
    "Laws_W_Ops_Composite_Base_Source_Hom", "Laws_W_Ops_Composite_Base_Source_assoc",
    "Laws_W_Ops_Composite_Base_Source_comp", "Laws_W_Ops_Composite_Base_Source_compId",
    "Laws_W_Ops_Composite_Base_Source_eqCongr", "Laws_W_Ops_Composite_Base_Source_eqTrans",
    "Laws_W_Ops_Composite_Base_Source_id", "Laws_W_Ops_Composite_Base_Source_idComp",
    "Laws_W_Ops_Composite_Base_Target_Category", "Laws_W_Ops_Composite_Base_Target_Hom",
    "Laws_W_Ops_Composite_Base_Target_assoc", "Laws_W_Ops_Composite_Base_Target_comp",
    "Laws_W_Ops_Composite_Base_Target_compId", "Laws_W_Ops_Composite_Base_Target_eqCongr",
    "Laws_W_Ops_Composite_Base_Target_eqTrans", "Laws_W_Ops_Composite_Base_Target_id",
    "Laws_W_Ops_Composite_Base_Target_idComp", "Laws_W_Ops_Composite_Base_eqCongr",
    "Laws_W_Ops_Composite_Base_functorMap", "Laws_W_Ops_Composite_Base_functorMapComp",
    "Laws_W_Ops_Composite_Base_functorMapId", "Laws_W_Ops_Composite_Base_functorObj", "Laws_W_Ops_Composite_NatTrans",
    "Laws_W_Ops_Composite_idNat", "Laws_W_Ops_Composite_natApp", "Laws_W_Ops_Composite_naturality",
    "Laws_W_Ops_Composite_targetEqSymm", "Laws_W_Ops_Composite_vcomp", "Laws_W_Ops_Composite_vcompLaw",
    "Laws_W_Ops_First_Base_Functor", "Laws_W_Ops_First_Base_Source_Category", "Laws_W_Ops_First_Base_Source_Hom",
    "Laws_W_Ops_First_Base_Source_assoc", "Laws_W_Ops_First_Base_Source_comp", "Laws_W_Ops_First_Base_Source_compId",
    "Laws_W_Ops_First_Base_Source_eqCongr", "Laws_W_Ops_First_Base_Source_eqTrans", "Laws_W_Ops_First_Base_Source_id",
    "Laws_W_Ops_First_Base_Source_idComp", "Laws_W_Ops_First_Base_Target_Category",
    "Laws_W_Ops_First_Base_Target_Hom", "Laws_W_Ops_First_Base_Target_assoc", "Laws_W_Ops_First_Base_Target_comp",
    "Laws_W_Ops_First_Base_Target_compId", "Laws_W_Ops_First_Base_Target_eqCongr",
    "Laws_W_Ops_First_Base_Target_eqTrans", "Laws_W_Ops_First_Base_Target_id", "Laws_W_Ops_First_Base_Target_idComp",
    "Laws_W_Ops_First_Base_eqCongr", "Laws_W_Ops_First_Base_functorMap", "Laws_W_Ops_First_Base_functorMapComp",
    "Laws_W_Ops_First_Base_functorMapId", "Laws_W_Ops_First_Base_functorObj", "Laws_W_Ops_First_NatTrans",
    "Laws_W_Ops_First_idNat", "Laws_W_Ops_First_natApp", "Laws_W_Ops_First_naturality",
    "Laws_W_Ops_First_targetEqSymm", "Laws_W_Ops_First_vcomp", "Laws_W_Ops_First_vcompLaw",
    "Laws_W_Ops_Middle_Category", "Laws_W_Ops_Middle_Hom", "Laws_W_Ops_Middle_assoc", "Laws_W_Ops_Middle_comp",
    "Laws_W_Ops_Middle_compId", "Laws_W_Ops_Middle_eqCongr", "Laws_W_Ops_Middle_eqTrans", "Laws_W_Ops_Middle_id",
    "Laws_W_Ops_Middle_idComp", "Laws_W_Ops_Second_Base_Functor", "Laws_W_Ops_Second_Base_Source_Category",
    "Laws_W_Ops_Second_Base_Source_Hom", "Laws_W_Ops_Second_Base_Source_assoc", "Laws_W_Ops_Second_Base_Source_comp",
    "Laws_W_Ops_Second_Base_Source_compId", "Laws_W_Ops_Second_Base_Source_eqCongr",
    "Laws_W_Ops_Second_Base_Source_eqTrans", "Laws_W_Ops_Second_Base_Source_id",
    "Laws_W_Ops_Second_Base_Source_idComp", "Laws_W_Ops_Second_Base_Target_Category",
    "Laws_W_Ops_Second_Base_Target_Hom", "Laws_W_Ops_Second_Base_Target_assoc", "Laws_W_Ops_Second_Base_Target_comp",
    "Laws_W_Ops_Second_Base_Target_compId", "Laws_W_Ops_Second_Base_Target_eqCongr",
    "Laws_W_Ops_Second_Base_Target_eqTrans", "Laws_W_Ops_Second_Base_Target_id",
    "Laws_W_Ops_Second_Base_Target_idComp", "Laws_W_Ops_Second_Base_eqCongr", "Laws_W_Ops_Second_Base_functorMap",
    "Laws_W_Ops_Second_Base_functorMapComp", "Laws_W_Ops_Second_Base_functorMapId",
    "Laws_W_Ops_Second_Base_functorObj", "Laws_W_Ops_Second_NatTrans", "Laws_W_Ops_Second_idNat",
    "Laws_W_Ops_Second_natApp", "Laws_W_Ops_Second_naturality", "Laws_W_Ops_Second_targetEqSymm",
    "Laws_W_Ops_Second_vcomp", "Laws_W_Ops_Second_vcompLaw", "Laws_W_Ops_Source_Category", "Laws_W_Ops_Source_Hom",
    "Laws_W_Ops_Source_assoc", "Laws_W_Ops_Source_comp", "Laws_W_Ops_Source_compId", "Laws_W_Ops_Source_eqCongr",
    "Laws_W_Ops_Source_eqTrans", "Laws_W_Ops_Source_id", "Laws_W_Ops_Source_idComp", "Laws_W_Ops_Target_Category",
    "Laws_W_Ops_Target_Hom", "Laws_W_Ops_Target_assoc", "Laws_W_Ops_Target_comp", "Laws_W_Ops_Target_compId",
    "Laws_W_Ops_Target_eqCongr", "Laws_W_Ops_Target_eqTrans", "Laws_W_Ops_Target_id", "Laws_W_Ops_Target_idComp",
    "Laws_W_Ops_hcomp", "Laws_W_Second_NatTransEq", "Laws_W_Second_Ops_Base_Functor",
    "Laws_W_Second_Ops_Base_Source_Category", "Laws_W_Second_Ops_Base_Source_Hom",
    "Laws_W_Second_Ops_Base_Source_assoc", "Laws_W_Second_Ops_Base_Source_comp",
    "Laws_W_Second_Ops_Base_Source_compId", "Laws_W_Second_Ops_Base_Source_eqCongr",
    "Laws_W_Second_Ops_Base_Source_eqTrans", "Laws_W_Second_Ops_Base_Source_id",
    "Laws_W_Second_Ops_Base_Source_idComp", "Laws_W_Second_Ops_Base_Target_Category",
    "Laws_W_Second_Ops_Base_Target_Hom", "Laws_W_Second_Ops_Base_Target_assoc", "Laws_W_Second_Ops_Base_Target_comp",
    "Laws_W_Second_Ops_Base_Target_compId", "Laws_W_Second_Ops_Base_Target_eqCongr",
    "Laws_W_Second_Ops_Base_Target_eqTrans", "Laws_W_Second_Ops_Base_Target_id",
    "Laws_W_Second_Ops_Base_Target_idComp", "Laws_W_Second_Ops_Base_eqCongr", "Laws_W_Second_Ops_Base_functorMap",
    "Laws_W_Second_Ops_Base_functorMapComp", "Laws_W_Second_Ops_Base_functorMapId",
    "Laws_W_Second_Ops_Base_functorObj", "Laws_W_Second_Ops_NatTrans", "Laws_W_Second_Ops_idNat",
    "Laws_W_Second_Ops_natApp", "Laws_W_Second_Ops_naturality", "Laws_W_Second_Ops_targetEqSymm",
    "Laws_W_Second_Ops_vcomp", "Laws_W_Second_Ops_vcompLaw", "Laws_W_Second_eqRefl", "Laws_W_Second_eqSymm",
    "Laws_W_Second_eqTrans", "Laws_W_Second_idVcomp", "Laws_W_Second_vcompAssoc", "Laws_W_Second_vcompCongr",
    "Laws_W_Second_vcompId", "Laws_W_whiskerLeftCongr", "Laws_W_whiskerLeftId", "Laws_W_whiskerLeftVcomp",
    "Laws_W_whiskerRightCongr", "Laws_W_whiskerRightId", "Laws_W_whiskerRightVcomp", "Laws_hcompCongr",
    "Laws_hcompExchange", "Laws_hcompId", "Laws_hcompIdId", "Laws_hcompVcomp", "Laws_idHcomp",
    "Laws_targetMiddleFour", "Left", "Mixed_NatTransEqAt", "Mixed_W_Composite_NatTransEq",
    "Mixed_W_Composite_Ops_Base_Functor", "Mixed_W_Composite_Ops_Base_Source_Category",
    "Mixed_W_Composite_Ops_Base_Source_Hom", "Mixed_W_Composite_Ops_Base_Source_assoc",
    "Mixed_W_Composite_Ops_Base_Source_comp", "Mixed_W_Composite_Ops_Base_Source_compId",
    "Mixed_W_Composite_Ops_Base_Source_eqCongr", "Mixed_W_Composite_Ops_Base_Source_eqTrans",
    "Mixed_W_Composite_Ops_Base_Source_id", "Mixed_W_Composite_Ops_Base_Source_idComp",
    "Mixed_W_Composite_Ops_Base_Target_Category", "Mixed_W_Composite_Ops_Base_Target_Hom",
    "Mixed_W_Composite_Ops_Base_Target_assoc", "Mixed_W_Composite_Ops_Base_Target_comp",
    "Mixed_W_Composite_Ops_Base_Target_compId", "Mixed_W_Composite_Ops_Base_Target_eqCongr",
    "Mixed_W_Composite_Ops_Base_Target_eqTrans", "Mixed_W_Composite_Ops_Base_Target_id",
    "Mixed_W_Composite_Ops_Base_Target_idComp", "Mixed_W_Composite_Ops_Base_eqCongr",
    "Mixed_W_Composite_Ops_Base_functorMap", "Mixed_W_Composite_Ops_Base_functorMapComp",
    "Mixed_W_Composite_Ops_Base_functorMapId", "Mixed_W_Composite_Ops_Base_functorObj",
    "Mixed_W_Composite_Ops_NatTrans", "Mixed_W_Composite_Ops_idNat", "Mixed_W_Composite_Ops_natApp",
    "Mixed_W_Composite_Ops_naturality", "Mixed_W_Composite_Ops_targetEqSymm", "Mixed_W_Composite_Ops_vcomp",
    "Mixed_W_Composite_Ops_vcompLaw", "Mixed_W_Composite_eqRefl", "Mixed_W_Composite_eqSymm",
    "Mixed_W_Composite_eqTrans", "Mixed_W_Composite_idVcomp", "Mixed_W_Composite_vcompAssoc",
    "Mixed_W_Composite_vcompCongr", "Mixed_W_Composite_vcompId", "Mixed_W_First_NatTransEq",
    "Mixed_W_First_Ops_Base_Functor", "Mixed_W_First_Ops_Base_Source_Category", "Mixed_W_First_Ops_Base_Source_Hom",
    "Mixed_W_First_Ops_Base_Source_assoc", "Mixed_W_First_Ops_Base_Source_comp",
    "Mixed_W_First_Ops_Base_Source_compId", "Mixed_W_First_Ops_Base_Source_eqCongr",
    "Mixed_W_First_Ops_Base_Source_eqTrans", "Mixed_W_First_Ops_Base_Source_id",
    "Mixed_W_First_Ops_Base_Source_idComp", "Mixed_W_First_Ops_Base_Target_Category",
    "Mixed_W_First_Ops_Base_Target_Hom", "Mixed_W_First_Ops_Base_Target_assoc", "Mixed_W_First_Ops_Base_Target_comp",
    "Mixed_W_First_Ops_Base_Target_compId", "Mixed_W_First_Ops_Base_Target_eqCongr",
    "Mixed_W_First_Ops_Base_Target_eqTrans", "Mixed_W_First_Ops_Base_Target_id",
    "Mixed_W_First_Ops_Base_Target_idComp", "Mixed_W_First_Ops_Base_eqCongr", "Mixed_W_First_Ops_Base_functorMap",
    "Mixed_W_First_Ops_Base_functorMapComp", "Mixed_W_First_Ops_Base_functorMapId",
    "Mixed_W_First_Ops_Base_functorObj", "Mixed_W_First_Ops_NatTrans", "Mixed_W_First_Ops_idNat",
    "Mixed_W_First_Ops_natApp", "Mixed_W_First_Ops_naturality", "Mixed_W_First_Ops_targetEqSymm",
    "Mixed_W_First_Ops_vcomp", "Mixed_W_First_Ops_vcompLaw", "Mixed_W_First_eqRefl", "Mixed_W_First_eqSymm",
    "Mixed_W_First_eqTrans", "Mixed_W_First_idVcomp", "Mixed_W_First_vcompAssoc", "Mixed_W_First_vcompCongr",
    "Mixed_W_First_vcompId", "Mixed_W_Ops_Compose_Base_Composite_Functor",
    "Mixed_W_Ops_Compose_Base_Composite_eqCongr", "Mixed_W_Ops_Compose_Base_Composite_functorMap",
    "Mixed_W_Ops_Compose_Base_Composite_functorMapComp", "Mixed_W_Ops_Compose_Base_Composite_functorMapId",
    "Mixed_W_Ops_Compose_Base_Composite_functorObj", "Mixed_W_Ops_Compose_Base_First_Functor",
    "Mixed_W_Ops_Compose_Base_First_eqCongr", "Mixed_W_Ops_Compose_Base_First_functorMap",
    "Mixed_W_Ops_Compose_Base_First_functorMapComp", "Mixed_W_Ops_Compose_Base_First_functorMapId",
    "Mixed_W_Ops_Compose_Base_First_functorObj", "Mixed_W_Ops_Compose_Base_Middle_Category",
    "Mixed_W_Ops_Compose_Base_Middle_Hom", "Mixed_W_Ops_Compose_Base_Middle_assoc",
    "Mixed_W_Ops_Compose_Base_Middle_comp", "Mixed_W_Ops_Compose_Base_Middle_compId",
    "Mixed_W_Ops_Compose_Base_Middle_eqCongr", "Mixed_W_Ops_Compose_Base_Middle_eqTrans",
    "Mixed_W_Ops_Compose_Base_Middle_id", "Mixed_W_Ops_Compose_Base_Middle_idComp",
    "Mixed_W_Ops_Compose_Base_Second_Functor", "Mixed_W_Ops_Compose_Base_Second_eqCongr",
    "Mixed_W_Ops_Compose_Base_Second_functorMap", "Mixed_W_Ops_Compose_Base_Second_functorMapComp",
    "Mixed_W_Ops_Compose_Base_Second_functorMapId", "Mixed_W_Ops_Compose_Base_Second_functorObj",
    "Mixed_W_Ops_Compose_Base_Source_Category", "Mixed_W_Ops_Compose_Base_Source_Hom",
    "Mixed_W_Ops_Compose_Base_Source_assoc", "Mixed_W_Ops_Compose_Base_Source_comp",
    "Mixed_W_Ops_Compose_Base_Source_compId", "Mixed_W_Ops_Compose_Base_Source_eqCongr",
    "Mixed_W_Ops_Compose_Base_Source_eqTrans", "Mixed_W_Ops_Compose_Base_Source_id",
    "Mixed_W_Ops_Compose_Base_Source_idComp", "Mixed_W_Ops_Compose_Base_Target_Category",
    "Mixed_W_Ops_Compose_Base_Target_Hom", "Mixed_W_Ops_Compose_Base_Target_assoc",
    "Mixed_W_Ops_Compose_Base_Target_comp", "Mixed_W_Ops_Compose_Base_Target_compId",
    "Mixed_W_Ops_Compose_Base_Target_eqCongr", "Mixed_W_Ops_Compose_Base_Target_eqTrans",
    "Mixed_W_Ops_Compose_Base_Target_id", "Mixed_W_Ops_Compose_Base_Target_idComp",
    "Mixed_W_Ops_Compose_Base_compFunctor", "Mixed_W_Ops_Compose_Composite_NatTrans",
    "Mixed_W_Ops_Compose_Composite_natApp", "Mixed_W_Ops_Compose_Composite_naturality",
    "Mixed_W_Ops_Compose_First_NatTrans", "Mixed_W_Ops_Compose_First_natApp", "Mixed_W_Ops_Compose_First_naturality",
    "Mixed_W_Ops_Compose_Second_NatTrans", "Mixed_W_Ops_Compose_Second_natApp",
    "Mixed_W_Ops_Compose_Second_naturality", "Mixed_W_Ops_Compose_targetEqSymm", "Mixed_W_Ops_Compose_whiskerLeft",
    "Mixed_W_Ops_Compose_whiskerRight", "Mixed_W_Ops_Composite_Base_Functor",
    "Mixed_W_Ops_Composite_Base_Source_Category", "Mixed_W_Ops_Composite_Base_Source_Hom",
    "Mixed_W_Ops_Composite_Base_Source_assoc", "Mixed_W_Ops_Composite_Base_Source_comp",
    "Mixed_W_Ops_Composite_Base_Source_compId", "Mixed_W_Ops_Composite_Base_Source_eqCongr",
    "Mixed_W_Ops_Composite_Base_Source_eqTrans", "Mixed_W_Ops_Composite_Base_Source_id",
    "Mixed_W_Ops_Composite_Base_Source_idComp", "Mixed_W_Ops_Composite_Base_Target_Category",
    "Mixed_W_Ops_Composite_Base_Target_Hom", "Mixed_W_Ops_Composite_Base_Target_assoc",
    "Mixed_W_Ops_Composite_Base_Target_comp", "Mixed_W_Ops_Composite_Base_Target_compId",
    "Mixed_W_Ops_Composite_Base_Target_eqCongr", "Mixed_W_Ops_Composite_Base_Target_eqTrans",
    "Mixed_W_Ops_Composite_Base_Target_id", "Mixed_W_Ops_Composite_Base_Target_idComp",
    "Mixed_W_Ops_Composite_Base_eqCongr", "Mixed_W_Ops_Composite_Base_functorMap",
    "Mixed_W_Ops_Composite_Base_functorMapComp", "Mixed_W_Ops_Composite_Base_functorMapId",
    "Mixed_W_Ops_Composite_Base_functorObj", "Mixed_W_Ops_Composite_NatTrans", "Mixed_W_Ops_Composite_idNat",
    "Mixed_W_Ops_Composite_natApp", "Mixed_W_Ops_Composite_naturality", "Mixed_W_Ops_Composite_targetEqSymm",
    "Mixed_W_Ops_Composite_vcomp", "Mixed_W_Ops_Composite_vcompLaw", "Mixed_W_Ops_First_Base_Functor",
    "Mixed_W_Ops_First_Base_Source_Category", "Mixed_W_Ops_First_Base_Source_Hom",
    "Mixed_W_Ops_First_Base_Source_assoc", "Mixed_W_Ops_First_Base_Source_comp",
    "Mixed_W_Ops_First_Base_Source_compId", "Mixed_W_Ops_First_Base_Source_eqCongr",
    "Mixed_W_Ops_First_Base_Source_eqTrans", "Mixed_W_Ops_First_Base_Source_id",
    "Mixed_W_Ops_First_Base_Source_idComp", "Mixed_W_Ops_First_Base_Target_Category",
    "Mixed_W_Ops_First_Base_Target_Hom", "Mixed_W_Ops_First_Base_Target_assoc", "Mixed_W_Ops_First_Base_Target_comp",
    "Mixed_W_Ops_First_Base_Target_compId", "Mixed_W_Ops_First_Base_Target_eqCongr",
    "Mixed_W_Ops_First_Base_Target_eqTrans", "Mixed_W_Ops_First_Base_Target_id",
    "Mixed_W_Ops_First_Base_Target_idComp", "Mixed_W_Ops_First_Base_eqCongr", "Mixed_W_Ops_First_Base_functorMap",
    "Mixed_W_Ops_First_Base_functorMapComp", "Mixed_W_Ops_First_Base_functorMapId",
    "Mixed_W_Ops_First_Base_functorObj", "Mixed_W_Ops_First_NatTrans", "Mixed_W_Ops_First_idNat",
    "Mixed_W_Ops_First_natApp", "Mixed_W_Ops_First_naturality", "Mixed_W_Ops_First_targetEqSymm",
    "Mixed_W_Ops_First_vcomp", "Mixed_W_Ops_First_vcompLaw", "Mixed_W_Ops_Middle_Category", "Mixed_W_Ops_Middle_Hom",
    "Mixed_W_Ops_Middle_assoc", "Mixed_W_Ops_Middle_comp", "Mixed_W_Ops_Middle_compId", "Mixed_W_Ops_Middle_eqCongr",
    "Mixed_W_Ops_Middle_eqTrans", "Mixed_W_Ops_Middle_id", "Mixed_W_Ops_Middle_idComp",
    "Mixed_W_Ops_Second_Base_Functor", "Mixed_W_Ops_Second_Base_Source_Category",
    "Mixed_W_Ops_Second_Base_Source_Hom", "Mixed_W_Ops_Second_Base_Source_assoc",
    "Mixed_W_Ops_Second_Base_Source_comp", "Mixed_W_Ops_Second_Base_Source_compId",
    "Mixed_W_Ops_Second_Base_Source_eqCongr", "Mixed_W_Ops_Second_Base_Source_eqTrans",
    "Mixed_W_Ops_Second_Base_Source_id", "Mixed_W_Ops_Second_Base_Source_idComp",
    "Mixed_W_Ops_Second_Base_Target_Category", "Mixed_W_Ops_Second_Base_Target_Hom",
    "Mixed_W_Ops_Second_Base_Target_assoc", "Mixed_W_Ops_Second_Base_Target_comp",
    "Mixed_W_Ops_Second_Base_Target_compId", "Mixed_W_Ops_Second_Base_Target_eqCongr",
    "Mixed_W_Ops_Second_Base_Target_eqTrans", "Mixed_W_Ops_Second_Base_Target_id",
    "Mixed_W_Ops_Second_Base_Target_idComp", "Mixed_W_Ops_Second_Base_eqCongr", "Mixed_W_Ops_Second_Base_functorMap",
    "Mixed_W_Ops_Second_Base_functorMapComp", "Mixed_W_Ops_Second_Base_functorMapId",
    "Mixed_W_Ops_Second_Base_functorObj", "Mixed_W_Ops_Second_NatTrans", "Mixed_W_Ops_Second_idNat",
    "Mixed_W_Ops_Second_natApp", "Mixed_W_Ops_Second_naturality", "Mixed_W_Ops_Second_targetEqSymm",
    "Mixed_W_Ops_Second_vcomp", "Mixed_W_Ops_Second_vcompLaw", "Mixed_W_Ops_Source_Category",
    "Mixed_W_Ops_Source_Hom", "Mixed_W_Ops_Source_assoc", "Mixed_W_Ops_Source_comp", "Mixed_W_Ops_Source_compId",
    "Mixed_W_Ops_Source_eqCongr", "Mixed_W_Ops_Source_eqTrans", "Mixed_W_Ops_Source_id", "Mixed_W_Ops_Source_idComp",
    "Mixed_W_Ops_Target_Category", "Mixed_W_Ops_Target_Hom", "Mixed_W_Ops_Target_assoc", "Mixed_W_Ops_Target_comp",
    "Mixed_W_Ops_Target_compId", "Mixed_W_Ops_Target_eqCongr", "Mixed_W_Ops_Target_eqTrans", "Mixed_W_Ops_Target_id",
    "Mixed_W_Ops_Target_idComp", "Mixed_W_Ops_hcomp", "Mixed_W_Second_NatTransEq", "Mixed_W_Second_Ops_Base_Functor",
    "Mixed_W_Second_Ops_Base_Source_Category", "Mixed_W_Second_Ops_Base_Source_Hom",
    "Mixed_W_Second_Ops_Base_Source_assoc", "Mixed_W_Second_Ops_Base_Source_comp",
    "Mixed_W_Second_Ops_Base_Source_compId", "Mixed_W_Second_Ops_Base_Source_eqCongr",
    "Mixed_W_Second_Ops_Base_Source_eqTrans", "Mixed_W_Second_Ops_Base_Source_id",
    "Mixed_W_Second_Ops_Base_Source_idComp", "Mixed_W_Second_Ops_Base_Target_Category",
    "Mixed_W_Second_Ops_Base_Target_Hom", "Mixed_W_Second_Ops_Base_Target_assoc",
    "Mixed_W_Second_Ops_Base_Target_comp", "Mixed_W_Second_Ops_Base_Target_compId",
    "Mixed_W_Second_Ops_Base_Target_eqCongr", "Mixed_W_Second_Ops_Base_Target_eqTrans",
    "Mixed_W_Second_Ops_Base_Target_id", "Mixed_W_Second_Ops_Base_Target_idComp", "Mixed_W_Second_Ops_Base_eqCongr",
    "Mixed_W_Second_Ops_Base_functorMap", "Mixed_W_Second_Ops_Base_functorMapComp",
    "Mixed_W_Second_Ops_Base_functorMapId", "Mixed_W_Second_Ops_Base_functorObj", "Mixed_W_Second_Ops_NatTrans",
    "Mixed_W_Second_Ops_idNat", "Mixed_W_Second_Ops_natApp", "Mixed_W_Second_Ops_naturality",
    "Mixed_W_Second_Ops_targetEqSymm", "Mixed_W_Second_Ops_vcomp", "Mixed_W_Second_Ops_vcompLaw",
    "Mixed_W_Second_eqRefl", "Mixed_W_Second_eqSymm", "Mixed_W_Second_eqTrans", "Mixed_W_Second_idVcomp",
    "Mixed_W_Second_vcompAssoc", "Mixed_W_Second_vcompCongr", "Mixed_W_Second_vcompId", "Mixed_W_whiskerLeftCongr",
    "Mixed_W_whiskerLeftId", "Mixed_W_whiskerLeftVcomp", "Mixed_W_whiskerRightCongr", "Mixed_W_whiskerRightId",
    "Mixed_W_whiskerRightVcomp", "Mixed_hcompCongr", "Mixed_hcompExchange", "Mixed_hcompId", "Mixed_hcompIdId",
    "Mixed_hcompVcomp", "Mixed_idHcomp", "Mixed_targetMiddleFour", "PairEnd", "Right",
    "Run_Compose_Base_Composite_Functor", "Run_Compose_Base_Composite_eqCongr",
    "Run_Compose_Base_Composite_functorMap", "Run_Compose_Base_Composite_functorMapComp",
    "Run_Compose_Base_Composite_functorMapId", "Run_Compose_Base_Composite_functorObj",
    "Run_Compose_Base_First_Functor", "Run_Compose_Base_First_eqCongr", "Run_Compose_Base_First_functorMap",
    "Run_Compose_Base_First_functorMapComp", "Run_Compose_Base_First_functorMapId",
    "Run_Compose_Base_First_functorObj", "Run_Compose_Base_Middle_Category", "Run_Compose_Base_Middle_Hom",
    "Run_Compose_Base_Middle_assoc", "Run_Compose_Base_Middle_comp", "Run_Compose_Base_Middle_compId",
    "Run_Compose_Base_Middle_eqCongr", "Run_Compose_Base_Middle_eqTrans", "Run_Compose_Base_Middle_id",
    "Run_Compose_Base_Middle_idComp", "Run_Compose_Base_Second_Functor", "Run_Compose_Base_Second_eqCongr",
    "Run_Compose_Base_Second_functorMap", "Run_Compose_Base_Second_functorMapComp",
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
    "Run_Target_idComp", "Run_hcomp", "SecondIdentityBeta", "SeparateTarget_Category", "SeparateTarget_Hom",
    "SeparateTarget_assoc", "SeparateTarget_comp", "SeparateTarget_compId", "SeparateTarget_eqCongr",
    "SeparateTarget_eqTrans", "SeparateTarget_id", "SeparateTarget_idComp", "Vertical", "Wide_NatTransEqAt",
    "Wide_W_Composite_NatTransEq", "Wide_W_Composite_Ops_Base_Functor", "Wide_W_Composite_Ops_Base_Source_Category",
    "Wide_W_Composite_Ops_Base_Source_Hom", "Wide_W_Composite_Ops_Base_Source_assoc",
    "Wide_W_Composite_Ops_Base_Source_comp", "Wide_W_Composite_Ops_Base_Source_compId",
    "Wide_W_Composite_Ops_Base_Source_eqCongr", "Wide_W_Composite_Ops_Base_Source_eqTrans",
    "Wide_W_Composite_Ops_Base_Source_id", "Wide_W_Composite_Ops_Base_Source_idComp",
    "Wide_W_Composite_Ops_Base_Target_Category", "Wide_W_Composite_Ops_Base_Target_Hom",
    "Wide_W_Composite_Ops_Base_Target_assoc", "Wide_W_Composite_Ops_Base_Target_comp",
    "Wide_W_Composite_Ops_Base_Target_compId", "Wide_W_Composite_Ops_Base_Target_eqCongr",
    "Wide_W_Composite_Ops_Base_Target_eqTrans", "Wide_W_Composite_Ops_Base_Target_id",
    "Wide_W_Composite_Ops_Base_Target_idComp", "Wide_W_Composite_Ops_Base_eqCongr",
    "Wide_W_Composite_Ops_Base_functorMap", "Wide_W_Composite_Ops_Base_functorMapComp",
    "Wide_W_Composite_Ops_Base_functorMapId", "Wide_W_Composite_Ops_Base_functorObj", "Wide_W_Composite_Ops_NatTrans",
    "Wide_W_Composite_Ops_idNat", "Wide_W_Composite_Ops_natApp", "Wide_W_Composite_Ops_naturality",
    "Wide_W_Composite_Ops_targetEqSymm", "Wide_W_Composite_Ops_vcomp", "Wide_W_Composite_Ops_vcompLaw",
    "Wide_W_Composite_eqRefl", "Wide_W_Composite_eqSymm", "Wide_W_Composite_eqTrans", "Wide_W_Composite_idVcomp",
    "Wide_W_Composite_vcompAssoc", "Wide_W_Composite_vcompCongr", "Wide_W_Composite_vcompId",
    "Wide_W_First_NatTransEq", "Wide_W_First_Ops_Base_Functor", "Wide_W_First_Ops_Base_Source_Category",
    "Wide_W_First_Ops_Base_Source_Hom", "Wide_W_First_Ops_Base_Source_assoc", "Wide_W_First_Ops_Base_Source_comp",
    "Wide_W_First_Ops_Base_Source_compId", "Wide_W_First_Ops_Base_Source_eqCongr",
    "Wide_W_First_Ops_Base_Source_eqTrans", "Wide_W_First_Ops_Base_Source_id", "Wide_W_First_Ops_Base_Source_idComp",
    "Wide_W_First_Ops_Base_Target_Category", "Wide_W_First_Ops_Base_Target_Hom", "Wide_W_First_Ops_Base_Target_assoc",
    "Wide_W_First_Ops_Base_Target_comp", "Wide_W_First_Ops_Base_Target_compId",
    "Wide_W_First_Ops_Base_Target_eqCongr", "Wide_W_First_Ops_Base_Target_eqTrans", "Wide_W_First_Ops_Base_Target_id",
    "Wide_W_First_Ops_Base_Target_idComp", "Wide_W_First_Ops_Base_eqCongr", "Wide_W_First_Ops_Base_functorMap",
    "Wide_W_First_Ops_Base_functorMapComp", "Wide_W_First_Ops_Base_functorMapId", "Wide_W_First_Ops_Base_functorObj",
    "Wide_W_First_Ops_NatTrans", "Wide_W_First_Ops_idNat", "Wide_W_First_Ops_natApp", "Wide_W_First_Ops_naturality",
    "Wide_W_First_Ops_targetEqSymm", "Wide_W_First_Ops_vcomp", "Wide_W_First_Ops_vcompLaw", "Wide_W_First_eqRefl",
    "Wide_W_First_eqSymm", "Wide_W_First_eqTrans", "Wide_W_First_idVcomp", "Wide_W_First_vcompAssoc",
    "Wide_W_First_vcompCongr", "Wide_W_First_vcompId", "Wide_W_Ops_Compose_Base_Composite_Functor",
    "Wide_W_Ops_Compose_Base_Composite_eqCongr", "Wide_W_Ops_Compose_Base_Composite_functorMap",
    "Wide_W_Ops_Compose_Base_Composite_functorMapComp", "Wide_W_Ops_Compose_Base_Composite_functorMapId",
    "Wide_W_Ops_Compose_Base_Composite_functorObj", "Wide_W_Ops_Compose_Base_First_Functor",
    "Wide_W_Ops_Compose_Base_First_eqCongr", "Wide_W_Ops_Compose_Base_First_functorMap",
    "Wide_W_Ops_Compose_Base_First_functorMapComp", "Wide_W_Ops_Compose_Base_First_functorMapId",
    "Wide_W_Ops_Compose_Base_First_functorObj", "Wide_W_Ops_Compose_Base_Middle_Category",
    "Wide_W_Ops_Compose_Base_Middle_Hom", "Wide_W_Ops_Compose_Base_Middle_assoc",
    "Wide_W_Ops_Compose_Base_Middle_comp", "Wide_W_Ops_Compose_Base_Middle_compId",
    "Wide_W_Ops_Compose_Base_Middle_eqCongr", "Wide_W_Ops_Compose_Base_Middle_eqTrans",
    "Wide_W_Ops_Compose_Base_Middle_id", "Wide_W_Ops_Compose_Base_Middle_idComp",
    "Wide_W_Ops_Compose_Base_Second_Functor", "Wide_W_Ops_Compose_Base_Second_eqCongr",
    "Wide_W_Ops_Compose_Base_Second_functorMap", "Wide_W_Ops_Compose_Base_Second_functorMapComp",
    "Wide_W_Ops_Compose_Base_Second_functorMapId", "Wide_W_Ops_Compose_Base_Second_functorObj",
    "Wide_W_Ops_Compose_Base_Source_Category", "Wide_W_Ops_Compose_Base_Source_Hom",
    "Wide_W_Ops_Compose_Base_Source_assoc", "Wide_W_Ops_Compose_Base_Source_comp",
    "Wide_W_Ops_Compose_Base_Source_compId", "Wide_W_Ops_Compose_Base_Source_eqCongr",
    "Wide_W_Ops_Compose_Base_Source_eqTrans", "Wide_W_Ops_Compose_Base_Source_id",
    "Wide_W_Ops_Compose_Base_Source_idComp", "Wide_W_Ops_Compose_Base_Target_Category",
    "Wide_W_Ops_Compose_Base_Target_Hom", "Wide_W_Ops_Compose_Base_Target_assoc",
    "Wide_W_Ops_Compose_Base_Target_comp", "Wide_W_Ops_Compose_Base_Target_compId",
    "Wide_W_Ops_Compose_Base_Target_eqCongr", "Wide_W_Ops_Compose_Base_Target_eqTrans",
    "Wide_W_Ops_Compose_Base_Target_id", "Wide_W_Ops_Compose_Base_Target_idComp",
    "Wide_W_Ops_Compose_Base_compFunctor", "Wide_W_Ops_Compose_Composite_NatTrans",
    "Wide_W_Ops_Compose_Composite_natApp", "Wide_W_Ops_Compose_Composite_naturality",
    "Wide_W_Ops_Compose_First_NatTrans", "Wide_W_Ops_Compose_First_natApp", "Wide_W_Ops_Compose_First_naturality",
    "Wide_W_Ops_Compose_Second_NatTrans", "Wide_W_Ops_Compose_Second_natApp", "Wide_W_Ops_Compose_Second_naturality",
    "Wide_W_Ops_Compose_targetEqSymm", "Wide_W_Ops_Compose_whiskerLeft", "Wide_W_Ops_Compose_whiskerRight",
    "Wide_W_Ops_Composite_Base_Functor", "Wide_W_Ops_Composite_Base_Source_Category",
    "Wide_W_Ops_Composite_Base_Source_Hom", "Wide_W_Ops_Composite_Base_Source_assoc",
    "Wide_W_Ops_Composite_Base_Source_comp", "Wide_W_Ops_Composite_Base_Source_compId",
    "Wide_W_Ops_Composite_Base_Source_eqCongr", "Wide_W_Ops_Composite_Base_Source_eqTrans",
    "Wide_W_Ops_Composite_Base_Source_id", "Wide_W_Ops_Composite_Base_Source_idComp",
    "Wide_W_Ops_Composite_Base_Target_Category", "Wide_W_Ops_Composite_Base_Target_Hom",
    "Wide_W_Ops_Composite_Base_Target_assoc", "Wide_W_Ops_Composite_Base_Target_comp",
    "Wide_W_Ops_Composite_Base_Target_compId", "Wide_W_Ops_Composite_Base_Target_eqCongr",
    "Wide_W_Ops_Composite_Base_Target_eqTrans", "Wide_W_Ops_Composite_Base_Target_id",
    "Wide_W_Ops_Composite_Base_Target_idComp", "Wide_W_Ops_Composite_Base_eqCongr",
    "Wide_W_Ops_Composite_Base_functorMap", "Wide_W_Ops_Composite_Base_functorMapComp",
    "Wide_W_Ops_Composite_Base_functorMapId", "Wide_W_Ops_Composite_Base_functorObj", "Wide_W_Ops_Composite_NatTrans",
    "Wide_W_Ops_Composite_idNat", "Wide_W_Ops_Composite_natApp", "Wide_W_Ops_Composite_naturality",
    "Wide_W_Ops_Composite_targetEqSymm", "Wide_W_Ops_Composite_vcomp", "Wide_W_Ops_Composite_vcompLaw",
    "Wide_W_Ops_First_Base_Functor", "Wide_W_Ops_First_Base_Source_Category", "Wide_W_Ops_First_Base_Source_Hom",
    "Wide_W_Ops_First_Base_Source_assoc", "Wide_W_Ops_First_Base_Source_comp", "Wide_W_Ops_First_Base_Source_compId",
    "Wide_W_Ops_First_Base_Source_eqCongr", "Wide_W_Ops_First_Base_Source_eqTrans", "Wide_W_Ops_First_Base_Source_id",
    "Wide_W_Ops_First_Base_Source_idComp", "Wide_W_Ops_First_Base_Target_Category",
    "Wide_W_Ops_First_Base_Target_Hom", "Wide_W_Ops_First_Base_Target_assoc", "Wide_W_Ops_First_Base_Target_comp",
    "Wide_W_Ops_First_Base_Target_compId", "Wide_W_Ops_First_Base_Target_eqCongr",
    "Wide_W_Ops_First_Base_Target_eqTrans", "Wide_W_Ops_First_Base_Target_id", "Wide_W_Ops_First_Base_Target_idComp",
    "Wide_W_Ops_First_Base_eqCongr", "Wide_W_Ops_First_Base_functorMap", "Wide_W_Ops_First_Base_functorMapComp",
    "Wide_W_Ops_First_Base_functorMapId", "Wide_W_Ops_First_Base_functorObj", "Wide_W_Ops_First_NatTrans",
    "Wide_W_Ops_First_idNat", "Wide_W_Ops_First_natApp", "Wide_W_Ops_First_naturality",
    "Wide_W_Ops_First_targetEqSymm", "Wide_W_Ops_First_vcomp", "Wide_W_Ops_First_vcompLaw",
    "Wide_W_Ops_Middle_Category", "Wide_W_Ops_Middle_Hom", "Wide_W_Ops_Middle_assoc", "Wide_W_Ops_Middle_comp",
    "Wide_W_Ops_Middle_compId", "Wide_W_Ops_Middle_eqCongr", "Wide_W_Ops_Middle_eqTrans", "Wide_W_Ops_Middle_id",
    "Wide_W_Ops_Middle_idComp", "Wide_W_Ops_Second_Base_Functor", "Wide_W_Ops_Second_Base_Source_Category",
    "Wide_W_Ops_Second_Base_Source_Hom", "Wide_W_Ops_Second_Base_Source_assoc", "Wide_W_Ops_Second_Base_Source_comp",
    "Wide_W_Ops_Second_Base_Source_compId", "Wide_W_Ops_Second_Base_Source_eqCongr",
    "Wide_W_Ops_Second_Base_Source_eqTrans", "Wide_W_Ops_Second_Base_Source_id",
    "Wide_W_Ops_Second_Base_Source_idComp", "Wide_W_Ops_Second_Base_Target_Category",
    "Wide_W_Ops_Second_Base_Target_Hom", "Wide_W_Ops_Second_Base_Target_assoc", "Wide_W_Ops_Second_Base_Target_comp",
    "Wide_W_Ops_Second_Base_Target_compId", "Wide_W_Ops_Second_Base_Target_eqCongr",
    "Wide_W_Ops_Second_Base_Target_eqTrans", "Wide_W_Ops_Second_Base_Target_id",
    "Wide_W_Ops_Second_Base_Target_idComp", "Wide_W_Ops_Second_Base_eqCongr", "Wide_W_Ops_Second_Base_functorMap",
    "Wide_W_Ops_Second_Base_functorMapComp", "Wide_W_Ops_Second_Base_functorMapId",
    "Wide_W_Ops_Second_Base_functorObj", "Wide_W_Ops_Second_NatTrans", "Wide_W_Ops_Second_idNat",
    "Wide_W_Ops_Second_natApp", "Wide_W_Ops_Second_naturality", "Wide_W_Ops_Second_targetEqSymm",
    "Wide_W_Ops_Second_vcomp", "Wide_W_Ops_Second_vcompLaw", "Wide_W_Ops_Source_Category", "Wide_W_Ops_Source_Hom",
    "Wide_W_Ops_Source_assoc", "Wide_W_Ops_Source_comp", "Wide_W_Ops_Source_compId", "Wide_W_Ops_Source_eqCongr",
    "Wide_W_Ops_Source_eqTrans", "Wide_W_Ops_Source_id", "Wide_W_Ops_Source_idComp", "Wide_W_Ops_Target_Category",
    "Wide_W_Ops_Target_Hom", "Wide_W_Ops_Target_assoc", "Wide_W_Ops_Target_comp", "Wide_W_Ops_Target_compId",
    "Wide_W_Ops_Target_eqCongr", "Wide_W_Ops_Target_eqTrans", "Wide_W_Ops_Target_id", "Wide_W_Ops_Target_idComp",
    "Wide_W_Ops_hcomp", "Wide_W_Second_NatTransEq", "Wide_W_Second_Ops_Base_Functor",
    "Wide_W_Second_Ops_Base_Source_Category", "Wide_W_Second_Ops_Base_Source_Hom",
    "Wide_W_Second_Ops_Base_Source_assoc", "Wide_W_Second_Ops_Base_Source_comp",
    "Wide_W_Second_Ops_Base_Source_compId", "Wide_W_Second_Ops_Base_Source_eqCongr",
    "Wide_W_Second_Ops_Base_Source_eqTrans", "Wide_W_Second_Ops_Base_Source_id",
    "Wide_W_Second_Ops_Base_Source_idComp", "Wide_W_Second_Ops_Base_Target_Category",
    "Wide_W_Second_Ops_Base_Target_Hom", "Wide_W_Second_Ops_Base_Target_assoc", "Wide_W_Second_Ops_Base_Target_comp",
    "Wide_W_Second_Ops_Base_Target_compId", "Wide_W_Second_Ops_Base_Target_eqCongr",
    "Wide_W_Second_Ops_Base_Target_eqTrans", "Wide_W_Second_Ops_Base_Target_id",
    "Wide_W_Second_Ops_Base_Target_idComp", "Wide_W_Second_Ops_Base_eqCongr", "Wide_W_Second_Ops_Base_functorMap",
    "Wide_W_Second_Ops_Base_functorMapComp", "Wide_W_Second_Ops_Base_functorMapId",
    "Wide_W_Second_Ops_Base_functorObj", "Wide_W_Second_Ops_NatTrans", "Wide_W_Second_Ops_idNat",
    "Wide_W_Second_Ops_natApp", "Wide_W_Second_Ops_naturality", "Wide_W_Second_Ops_targetEqSymm",
    "Wide_W_Second_Ops_vcomp", "Wide_W_Second_Ops_vcompLaw", "Wide_W_Second_eqRefl", "Wide_W_Second_eqSymm",
    "Wide_W_Second_eqTrans", "Wide_W_Second_idVcomp", "Wide_W_Second_vcompAssoc", "Wide_W_Second_vcompCongr",
    "Wide_W_Second_vcompId", "Wide_W_whiskerLeftCongr", "Wide_W_whiskerLeftId", "Wide_W_whiskerLeftVcomp",
    "Wide_W_whiskerRightCongr", "Wide_W_whiskerRightId", "Wide_W_whiskerRightVcomp", "Wide_hcompCongr",
    "Wide_hcompExchange", "Wide_hcompId", "Wide_hcompIdId", "Wide_hcompVcomp", "Wide_idHcomp",
    "Wide_targetMiddleFour", "certifiedHorizontal", "componentLaw", "components", "congruenceLaw", "congruenceLeftAt",
    "congruenceLhs", "congruenceRhs", "congruenceRightAt", "exchangeLaw", "exchangeLeftAt", "exchangeLhs",
    "exchangeRhs", "exchangeRightAt", "exchangeSwappedLaw", "exchangeSwappedLeftAt", "exchangeSwappedLhs",
    "exchangeSwappedRhs", "exchangeSwappedRightAt", "horizontalFirst", "horizontalSecond", "idBothLaw",
    "idBothLeftAt", "idBothLhs", "idBothRhs", "idBothRightAt", "idLeftLaw", "idLeftLeftAt", "idLeftLhs", "idLeftRhs",
    "idLeftRightAt", "idRightLaw", "idRightLeftAt", "idRightLhs", "idRightRhs", "idRightRightAt", "identityValue",
    "interchangeLaw", "interchangeLeftAt", "interchangeLhs", "interchangeRhs", "interchangeRightAt",
    "naturalityWitness", "pairCode", "pathTrans", "sharedNatInput", "verticalValue"
};

/* expected_families() (tests/prelude_horizontal_laws.bend line 15, 12 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "Mixed_W_Ops_Middle", "Mixed_W_Ops_Source", "Mixed_W_Ops_Target", "Path", "Point", "Run_Middle", "Run_Source",
    "Run_Target", "SeparateTarget", "Wide_W_Ops_Middle", "Wide_W_Ops_Source", "Wide_W_Ops_Target"
};

/* negatives() (tests/prelude_horizontal_laws.bend line 18, 7 names) */
static const char *const NEGATIVES[] = {
    "false-components", "one-component", "erased-object", "missing-first-congruence", "missing-second-congruence",
    "nominal-target", "wrong-endpoint"
};

/* values() (28 Program.Probe rows) */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  probe(heap, &out, "idLeftLeftAt", 37, 750);
  probe(heap, &out, "idLeftRightAt", 37, 750);
  probe(heap, &out, "idRightLeftAt", 37, 13707);
  probe(heap, &out, "idRightRightAt", 37, 13707);
  probe(heap, &out, "idBothLeftAt", 37, 5007);
  probe(heap, &out, "idBothRightAt", 37, 5007);
  probe(heap, &out, "congruenceLeftAt", 37, 837);
  probe(heap, &out, "congruenceRightAt", 37, 837);
  probe(heap, &out, "exchangeLeftAt", 37, 837);
  probe(heap, &out, "exchangeRightAt", 37, 837);
  probe(heap, &out, "exchangeSwappedLeftAt", 37, 750);
  probe(heap, &out, "exchangeSwappedRightAt", 37, 750);
  probe(heap, &out, "interchangeLeftAt", 37, 14205);
  probe(heap, &out, "interchangeRightAt", 37, 14205);
  probe(heap, &out, "idLeftLeftAt", 41, 754);
  probe(heap, &out, "idLeftRightAt", 41, 754);
  probe(heap, &out, "idRightLeftAt", 41, 14907);
  probe(heap, &out, "idRightRightAt", 41, 14907);
  probe(heap, &out, "idBothLeftAt", 41, 5407);
  probe(heap, &out, "idBothRightAt", 41, 5407);
  probe(heap, &out, "congruenceLeftAt", 41, 849);
  probe(heap, &out, "congruenceRightAt", 41, 849);
  probe(heap, &out, "exchangeLeftAt", 41, 849);
  probe(heap, &out, "exchangeRightAt", 41, 849);
  probe(heap, &out, "exchangeSwappedLeftAt", 41, 754);
  probe(heap, &out, "exchangeSwappedRightAt", 41, 754);
  probe(heap, &out, "interchangeLeftAt", 41, 15405);
  probe(heap, &out, "interchangeRightAt", 41, 15405);
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech", "prelude/cat/nattrans-laws.mech",
    "prelude/cat/whiskering-laws.mech", "prelude/cat/horizontal-laws.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/horizontal-laws.mech", "test/fixtures/prelude/shared-nattrans-runtime.mech",
    "test/fixtures/prelude/horizontal-laws-runtime.mech"
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
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  globals = relational_globals(checked);
  rows = values(heap);
  RELATIONAL_TRY(program_probes(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  full = vec_new(heap->arena);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/horizontal-laws"), &negatives, globals,
                               negatives_digest(40, 0, &full, false, false)));
  *fields = str_lit("entries=1205 families=12 computations=28 negatives=7");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-HORIZONTAL-LAWS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-HORIZONTAL-LAWS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
