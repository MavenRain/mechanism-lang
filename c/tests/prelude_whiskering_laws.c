/* Port of tests/prelude_whiskering_laws.bend (41 lines): the whiskering laws prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-WHISKERING-LAWS-OK fields or PRELUDE-WHISKERING-LAWS-FAIL message. Same definitions in the same order. */
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

/* expected_entries() (tests/prelude_whiskering_laws.bend line 12, 1177 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "Constant", "Delta", "Embed", "End", "External_Functor", "External_Source_Category",
    "External_Source_Hom", "External_Source_assoc", "External_Source_comp", "External_Source_compId",
    "External_Source_eqCongr", "External_Source_eqTrans", "External_Source_id", "External_Source_idComp",
    "External_Target_Category", "External_Target_Hom", "External_Target_assoc", "External_Target_comp",
    "External_Target_compId", "External_Target_eqCongr", "External_Target_eqTrans", "External_Target_id",
    "External_Target_idComp", "External_eqCongr", "External_functorMap", "External_functorMapComp",
    "External_functorMapId", "External_functorObj", "FH", "FirstIdentityAlpha", "GI", "Gamma", "Horizontal",
    "Identity", "IndexedEnd", "Laws_Composite_NatTransEq", "Laws_Composite_Ops_Base_Functor",
    "Laws_Composite_Ops_Base_Source_Category", "Laws_Composite_Ops_Base_Source_Hom",
    "Laws_Composite_Ops_Base_Source_assoc", "Laws_Composite_Ops_Base_Source_comp",
    "Laws_Composite_Ops_Base_Source_compId", "Laws_Composite_Ops_Base_Source_eqCongr",
    "Laws_Composite_Ops_Base_Source_eqTrans", "Laws_Composite_Ops_Base_Source_id",
    "Laws_Composite_Ops_Base_Source_idComp", "Laws_Composite_Ops_Base_Target_Category",
    "Laws_Composite_Ops_Base_Target_Hom", "Laws_Composite_Ops_Base_Target_assoc",
    "Laws_Composite_Ops_Base_Target_comp", "Laws_Composite_Ops_Base_Target_compId",
    "Laws_Composite_Ops_Base_Target_eqCongr", "Laws_Composite_Ops_Base_Target_eqTrans",
    "Laws_Composite_Ops_Base_Target_id", "Laws_Composite_Ops_Base_Target_idComp", "Laws_Composite_Ops_Base_eqCongr",
    "Laws_Composite_Ops_Base_functorMap", "Laws_Composite_Ops_Base_functorMapComp",
    "Laws_Composite_Ops_Base_functorMapId", "Laws_Composite_Ops_Base_functorObj", "Laws_Composite_Ops_NatTrans",
    "Laws_Composite_Ops_idNat", "Laws_Composite_Ops_natApp", "Laws_Composite_Ops_naturality",
    "Laws_Composite_Ops_targetEqSymm", "Laws_Composite_Ops_vcomp", "Laws_Composite_Ops_vcompLaw",
    "Laws_Composite_eqRefl", "Laws_Composite_eqSymm", "Laws_Composite_eqTrans", "Laws_Composite_idVcomp",
    "Laws_Composite_vcompAssoc", "Laws_Composite_vcompCongr", "Laws_Composite_vcompId", "Laws_First_NatTransEq",
    "Laws_First_Ops_Base_Functor", "Laws_First_Ops_Base_Source_Category", "Laws_First_Ops_Base_Source_Hom",
    "Laws_First_Ops_Base_Source_assoc", "Laws_First_Ops_Base_Source_comp", "Laws_First_Ops_Base_Source_compId",
    "Laws_First_Ops_Base_Source_eqCongr", "Laws_First_Ops_Base_Source_eqTrans", "Laws_First_Ops_Base_Source_id",
    "Laws_First_Ops_Base_Source_idComp", "Laws_First_Ops_Base_Target_Category", "Laws_First_Ops_Base_Target_Hom",
    "Laws_First_Ops_Base_Target_assoc", "Laws_First_Ops_Base_Target_comp", "Laws_First_Ops_Base_Target_compId",
    "Laws_First_Ops_Base_Target_eqCongr", "Laws_First_Ops_Base_Target_eqTrans", "Laws_First_Ops_Base_Target_id",
    "Laws_First_Ops_Base_Target_idComp", "Laws_First_Ops_Base_eqCongr", "Laws_First_Ops_Base_functorMap",
    "Laws_First_Ops_Base_functorMapComp", "Laws_First_Ops_Base_functorMapId", "Laws_First_Ops_Base_functorObj",
    "Laws_First_Ops_NatTrans", "Laws_First_Ops_idNat", "Laws_First_Ops_natApp", "Laws_First_Ops_naturality",
    "Laws_First_Ops_targetEqSymm", "Laws_First_Ops_vcomp", "Laws_First_Ops_vcompLaw", "Laws_First_eqRefl",
    "Laws_First_eqSymm", "Laws_First_eqTrans", "Laws_First_idVcomp", "Laws_First_vcompAssoc", "Laws_First_vcompCongr",
    "Laws_First_vcompId", "Laws_Ops_Compose_Base_Composite_Functor", "Laws_Ops_Compose_Base_Composite_eqCongr",
    "Laws_Ops_Compose_Base_Composite_functorMap", "Laws_Ops_Compose_Base_Composite_functorMapComp",
    "Laws_Ops_Compose_Base_Composite_functorMapId", "Laws_Ops_Compose_Base_Composite_functorObj",
    "Laws_Ops_Compose_Base_First_Functor", "Laws_Ops_Compose_Base_First_eqCongr",
    "Laws_Ops_Compose_Base_First_functorMap", "Laws_Ops_Compose_Base_First_functorMapComp",
    "Laws_Ops_Compose_Base_First_functorMapId", "Laws_Ops_Compose_Base_First_functorObj",
    "Laws_Ops_Compose_Base_Middle_Category", "Laws_Ops_Compose_Base_Middle_Hom", "Laws_Ops_Compose_Base_Middle_assoc",
    "Laws_Ops_Compose_Base_Middle_comp", "Laws_Ops_Compose_Base_Middle_compId",
    "Laws_Ops_Compose_Base_Middle_eqCongr", "Laws_Ops_Compose_Base_Middle_eqTrans", "Laws_Ops_Compose_Base_Middle_id",
    "Laws_Ops_Compose_Base_Middle_idComp", "Laws_Ops_Compose_Base_Second_Functor",
    "Laws_Ops_Compose_Base_Second_eqCongr", "Laws_Ops_Compose_Base_Second_functorMap",
    "Laws_Ops_Compose_Base_Second_functorMapComp", "Laws_Ops_Compose_Base_Second_functorMapId",
    "Laws_Ops_Compose_Base_Second_functorObj", "Laws_Ops_Compose_Base_Source_Category",
    "Laws_Ops_Compose_Base_Source_Hom", "Laws_Ops_Compose_Base_Source_assoc", "Laws_Ops_Compose_Base_Source_comp",
    "Laws_Ops_Compose_Base_Source_compId", "Laws_Ops_Compose_Base_Source_eqCongr",
    "Laws_Ops_Compose_Base_Source_eqTrans", "Laws_Ops_Compose_Base_Source_id", "Laws_Ops_Compose_Base_Source_idComp",
    "Laws_Ops_Compose_Base_Target_Category", "Laws_Ops_Compose_Base_Target_Hom", "Laws_Ops_Compose_Base_Target_assoc",
    "Laws_Ops_Compose_Base_Target_comp", "Laws_Ops_Compose_Base_Target_compId",
    "Laws_Ops_Compose_Base_Target_eqCongr", "Laws_Ops_Compose_Base_Target_eqTrans", "Laws_Ops_Compose_Base_Target_id",
    "Laws_Ops_Compose_Base_Target_idComp", "Laws_Ops_Compose_Base_compFunctor", "Laws_Ops_Compose_Composite_NatTrans",
    "Laws_Ops_Compose_Composite_natApp", "Laws_Ops_Compose_Composite_naturality", "Laws_Ops_Compose_First_NatTrans",
    "Laws_Ops_Compose_First_natApp", "Laws_Ops_Compose_First_naturality", "Laws_Ops_Compose_Second_NatTrans",
    "Laws_Ops_Compose_Second_natApp", "Laws_Ops_Compose_Second_naturality", "Laws_Ops_Compose_targetEqSymm",
    "Laws_Ops_Compose_whiskerLeft", "Laws_Ops_Compose_whiskerRight", "Laws_Ops_Composite_Base_Functor",
    "Laws_Ops_Composite_Base_Source_Category", "Laws_Ops_Composite_Base_Source_Hom",
    "Laws_Ops_Composite_Base_Source_assoc", "Laws_Ops_Composite_Base_Source_comp",
    "Laws_Ops_Composite_Base_Source_compId", "Laws_Ops_Composite_Base_Source_eqCongr",
    "Laws_Ops_Composite_Base_Source_eqTrans", "Laws_Ops_Composite_Base_Source_id",
    "Laws_Ops_Composite_Base_Source_idComp", "Laws_Ops_Composite_Base_Target_Category",
    "Laws_Ops_Composite_Base_Target_Hom", "Laws_Ops_Composite_Base_Target_assoc",
    "Laws_Ops_Composite_Base_Target_comp", "Laws_Ops_Composite_Base_Target_compId",
    "Laws_Ops_Composite_Base_Target_eqCongr", "Laws_Ops_Composite_Base_Target_eqTrans",
    "Laws_Ops_Composite_Base_Target_id", "Laws_Ops_Composite_Base_Target_idComp", "Laws_Ops_Composite_Base_eqCongr",
    "Laws_Ops_Composite_Base_functorMap", "Laws_Ops_Composite_Base_functorMapComp",
    "Laws_Ops_Composite_Base_functorMapId", "Laws_Ops_Composite_Base_functorObj", "Laws_Ops_Composite_NatTrans",
    "Laws_Ops_Composite_idNat", "Laws_Ops_Composite_natApp", "Laws_Ops_Composite_naturality",
    "Laws_Ops_Composite_targetEqSymm", "Laws_Ops_Composite_vcomp", "Laws_Ops_Composite_vcompLaw",
    "Laws_Ops_First_Base_Functor", "Laws_Ops_First_Base_Source_Category", "Laws_Ops_First_Base_Source_Hom",
    "Laws_Ops_First_Base_Source_assoc", "Laws_Ops_First_Base_Source_comp", "Laws_Ops_First_Base_Source_compId",
    "Laws_Ops_First_Base_Source_eqCongr", "Laws_Ops_First_Base_Source_eqTrans", "Laws_Ops_First_Base_Source_id",
    "Laws_Ops_First_Base_Source_idComp", "Laws_Ops_First_Base_Target_Category", "Laws_Ops_First_Base_Target_Hom",
    "Laws_Ops_First_Base_Target_assoc", "Laws_Ops_First_Base_Target_comp", "Laws_Ops_First_Base_Target_compId",
    "Laws_Ops_First_Base_Target_eqCongr", "Laws_Ops_First_Base_Target_eqTrans", "Laws_Ops_First_Base_Target_id",
    "Laws_Ops_First_Base_Target_idComp", "Laws_Ops_First_Base_eqCongr", "Laws_Ops_First_Base_functorMap",
    "Laws_Ops_First_Base_functorMapComp", "Laws_Ops_First_Base_functorMapId", "Laws_Ops_First_Base_functorObj",
    "Laws_Ops_First_NatTrans", "Laws_Ops_First_idNat", "Laws_Ops_First_natApp", "Laws_Ops_First_naturality",
    "Laws_Ops_First_targetEqSymm", "Laws_Ops_First_vcomp", "Laws_Ops_First_vcompLaw", "Laws_Ops_Middle_Category",
    "Laws_Ops_Middle_Hom", "Laws_Ops_Middle_assoc", "Laws_Ops_Middle_comp", "Laws_Ops_Middle_compId",
    "Laws_Ops_Middle_eqCongr", "Laws_Ops_Middle_eqTrans", "Laws_Ops_Middle_id", "Laws_Ops_Middle_idComp",
    "Laws_Ops_Second_Base_Functor", "Laws_Ops_Second_Base_Source_Category", "Laws_Ops_Second_Base_Source_Hom",
    "Laws_Ops_Second_Base_Source_assoc", "Laws_Ops_Second_Base_Source_comp", "Laws_Ops_Second_Base_Source_compId",
    "Laws_Ops_Second_Base_Source_eqCongr", "Laws_Ops_Second_Base_Source_eqTrans", "Laws_Ops_Second_Base_Source_id",
    "Laws_Ops_Second_Base_Source_idComp", "Laws_Ops_Second_Base_Target_Category", "Laws_Ops_Second_Base_Target_Hom",
    "Laws_Ops_Second_Base_Target_assoc", "Laws_Ops_Second_Base_Target_comp", "Laws_Ops_Second_Base_Target_compId",
    "Laws_Ops_Second_Base_Target_eqCongr", "Laws_Ops_Second_Base_Target_eqTrans", "Laws_Ops_Second_Base_Target_id",
    "Laws_Ops_Second_Base_Target_idComp", "Laws_Ops_Second_Base_eqCongr", "Laws_Ops_Second_Base_functorMap",
    "Laws_Ops_Second_Base_functorMapComp", "Laws_Ops_Second_Base_functorMapId", "Laws_Ops_Second_Base_functorObj",
    "Laws_Ops_Second_NatTrans", "Laws_Ops_Second_idNat", "Laws_Ops_Second_natApp", "Laws_Ops_Second_naturality",
    "Laws_Ops_Second_targetEqSymm", "Laws_Ops_Second_vcomp", "Laws_Ops_Second_vcompLaw", "Laws_Ops_Source_Category",
    "Laws_Ops_Source_Hom", "Laws_Ops_Source_assoc", "Laws_Ops_Source_comp", "Laws_Ops_Source_compId",
    "Laws_Ops_Source_eqCongr", "Laws_Ops_Source_eqTrans", "Laws_Ops_Source_id", "Laws_Ops_Source_idComp",
    "Laws_Ops_Target_Category", "Laws_Ops_Target_Hom", "Laws_Ops_Target_assoc", "Laws_Ops_Target_comp",
    "Laws_Ops_Target_compId", "Laws_Ops_Target_eqCongr", "Laws_Ops_Target_eqTrans", "Laws_Ops_Target_id",
    "Laws_Ops_Target_idComp", "Laws_Ops_hcomp", "Laws_Second_NatTransEq", "Laws_Second_Ops_Base_Functor",
    "Laws_Second_Ops_Base_Source_Category", "Laws_Second_Ops_Base_Source_Hom", "Laws_Second_Ops_Base_Source_assoc",
    "Laws_Second_Ops_Base_Source_comp", "Laws_Second_Ops_Base_Source_compId", "Laws_Second_Ops_Base_Source_eqCongr",
    "Laws_Second_Ops_Base_Source_eqTrans", "Laws_Second_Ops_Base_Source_id", "Laws_Second_Ops_Base_Source_idComp",
    "Laws_Second_Ops_Base_Target_Category", "Laws_Second_Ops_Base_Target_Hom", "Laws_Second_Ops_Base_Target_assoc",
    "Laws_Second_Ops_Base_Target_comp", "Laws_Second_Ops_Base_Target_compId", "Laws_Second_Ops_Base_Target_eqCongr",
    "Laws_Second_Ops_Base_Target_eqTrans", "Laws_Second_Ops_Base_Target_id", "Laws_Second_Ops_Base_Target_idComp",
    "Laws_Second_Ops_Base_eqCongr", "Laws_Second_Ops_Base_functorMap", "Laws_Second_Ops_Base_functorMapComp",
    "Laws_Second_Ops_Base_functorMapId", "Laws_Second_Ops_Base_functorObj", "Laws_Second_Ops_NatTrans",
    "Laws_Second_Ops_idNat", "Laws_Second_Ops_natApp", "Laws_Second_Ops_naturality", "Laws_Second_Ops_targetEqSymm",
    "Laws_Second_Ops_vcomp", "Laws_Second_Ops_vcompLaw", "Laws_Second_eqRefl", "Laws_Second_eqSymm",
    "Laws_Second_eqTrans", "Laws_Second_idVcomp", "Laws_Second_vcompAssoc", "Laws_Second_vcompCongr",
    "Laws_Second_vcompId", "Laws_whiskerLeftCongr", "Laws_whiskerLeftId", "Laws_whiskerLeftVcomp",
    "Laws_whiskerRightCongr", "Laws_whiskerRightId", "Laws_whiskerRightVcomp", "Left", "Mixed_Composite_NatTransEq",
    "Mixed_Composite_Ops_Base_Functor", "Mixed_Composite_Ops_Base_Source_Category",
    "Mixed_Composite_Ops_Base_Source_Hom", "Mixed_Composite_Ops_Base_Source_assoc",
    "Mixed_Composite_Ops_Base_Source_comp", "Mixed_Composite_Ops_Base_Source_compId",
    "Mixed_Composite_Ops_Base_Source_eqCongr", "Mixed_Composite_Ops_Base_Source_eqTrans",
    "Mixed_Composite_Ops_Base_Source_id", "Mixed_Composite_Ops_Base_Source_idComp",
    "Mixed_Composite_Ops_Base_Target_Category", "Mixed_Composite_Ops_Base_Target_Hom",
    "Mixed_Composite_Ops_Base_Target_assoc", "Mixed_Composite_Ops_Base_Target_comp",
    "Mixed_Composite_Ops_Base_Target_compId", "Mixed_Composite_Ops_Base_Target_eqCongr",
    "Mixed_Composite_Ops_Base_Target_eqTrans", "Mixed_Composite_Ops_Base_Target_id",
    "Mixed_Composite_Ops_Base_Target_idComp", "Mixed_Composite_Ops_Base_eqCongr",
    "Mixed_Composite_Ops_Base_functorMap", "Mixed_Composite_Ops_Base_functorMapComp",
    "Mixed_Composite_Ops_Base_functorMapId", "Mixed_Composite_Ops_Base_functorObj", "Mixed_Composite_Ops_NatTrans",
    "Mixed_Composite_Ops_idNat", "Mixed_Composite_Ops_natApp", "Mixed_Composite_Ops_naturality",
    "Mixed_Composite_Ops_targetEqSymm", "Mixed_Composite_Ops_vcomp", "Mixed_Composite_Ops_vcompLaw",
    "Mixed_Composite_eqRefl", "Mixed_Composite_eqSymm", "Mixed_Composite_eqTrans", "Mixed_Composite_idVcomp",
    "Mixed_Composite_vcompAssoc", "Mixed_Composite_vcompCongr", "Mixed_Composite_vcompId", "Mixed_First_NatTransEq",
    "Mixed_First_Ops_Base_Functor", "Mixed_First_Ops_Base_Source_Category", "Mixed_First_Ops_Base_Source_Hom",
    "Mixed_First_Ops_Base_Source_assoc", "Mixed_First_Ops_Base_Source_comp", "Mixed_First_Ops_Base_Source_compId",
    "Mixed_First_Ops_Base_Source_eqCongr", "Mixed_First_Ops_Base_Source_eqTrans", "Mixed_First_Ops_Base_Source_id",
    "Mixed_First_Ops_Base_Source_idComp", "Mixed_First_Ops_Base_Target_Category", "Mixed_First_Ops_Base_Target_Hom",
    "Mixed_First_Ops_Base_Target_assoc", "Mixed_First_Ops_Base_Target_comp", "Mixed_First_Ops_Base_Target_compId",
    "Mixed_First_Ops_Base_Target_eqCongr", "Mixed_First_Ops_Base_Target_eqTrans", "Mixed_First_Ops_Base_Target_id",
    "Mixed_First_Ops_Base_Target_idComp", "Mixed_First_Ops_Base_eqCongr", "Mixed_First_Ops_Base_functorMap",
    "Mixed_First_Ops_Base_functorMapComp", "Mixed_First_Ops_Base_functorMapId", "Mixed_First_Ops_Base_functorObj",
    "Mixed_First_Ops_NatTrans", "Mixed_First_Ops_idNat", "Mixed_First_Ops_natApp", "Mixed_First_Ops_naturality",
    "Mixed_First_Ops_targetEqSymm", "Mixed_First_Ops_vcomp", "Mixed_First_Ops_vcompLaw", "Mixed_First_eqRefl",
    "Mixed_First_eqSymm", "Mixed_First_eqTrans", "Mixed_First_idVcomp", "Mixed_First_vcompAssoc",
    "Mixed_First_vcompCongr", "Mixed_First_vcompId", "Mixed_Ops_Compose_Base_Composite_Functor",
    "Mixed_Ops_Compose_Base_Composite_eqCongr", "Mixed_Ops_Compose_Base_Composite_functorMap",
    "Mixed_Ops_Compose_Base_Composite_functorMapComp", "Mixed_Ops_Compose_Base_Composite_functorMapId",
    "Mixed_Ops_Compose_Base_Composite_functorObj", "Mixed_Ops_Compose_Base_First_Functor",
    "Mixed_Ops_Compose_Base_First_eqCongr", "Mixed_Ops_Compose_Base_First_functorMap",
    "Mixed_Ops_Compose_Base_First_functorMapComp", "Mixed_Ops_Compose_Base_First_functorMapId",
    "Mixed_Ops_Compose_Base_First_functorObj", "Mixed_Ops_Compose_Base_Middle_Category",
    "Mixed_Ops_Compose_Base_Middle_Hom", "Mixed_Ops_Compose_Base_Middle_assoc", "Mixed_Ops_Compose_Base_Middle_comp",
    "Mixed_Ops_Compose_Base_Middle_compId", "Mixed_Ops_Compose_Base_Middle_eqCongr",
    "Mixed_Ops_Compose_Base_Middle_eqTrans", "Mixed_Ops_Compose_Base_Middle_id",
    "Mixed_Ops_Compose_Base_Middle_idComp", "Mixed_Ops_Compose_Base_Second_Functor",
    "Mixed_Ops_Compose_Base_Second_eqCongr", "Mixed_Ops_Compose_Base_Second_functorMap",
    "Mixed_Ops_Compose_Base_Second_functorMapComp", "Mixed_Ops_Compose_Base_Second_functorMapId",
    "Mixed_Ops_Compose_Base_Second_functorObj", "Mixed_Ops_Compose_Base_Source_Category",
    "Mixed_Ops_Compose_Base_Source_Hom", "Mixed_Ops_Compose_Base_Source_assoc", "Mixed_Ops_Compose_Base_Source_comp",
    "Mixed_Ops_Compose_Base_Source_compId", "Mixed_Ops_Compose_Base_Source_eqCongr",
    "Mixed_Ops_Compose_Base_Source_eqTrans", "Mixed_Ops_Compose_Base_Source_id",
    "Mixed_Ops_Compose_Base_Source_idComp", "Mixed_Ops_Compose_Base_Target_Category",
    "Mixed_Ops_Compose_Base_Target_Hom", "Mixed_Ops_Compose_Base_Target_assoc", "Mixed_Ops_Compose_Base_Target_comp",
    "Mixed_Ops_Compose_Base_Target_compId", "Mixed_Ops_Compose_Base_Target_eqCongr",
    "Mixed_Ops_Compose_Base_Target_eqTrans", "Mixed_Ops_Compose_Base_Target_id",
    "Mixed_Ops_Compose_Base_Target_idComp", "Mixed_Ops_Compose_Base_compFunctor",
    "Mixed_Ops_Compose_Composite_NatTrans", "Mixed_Ops_Compose_Composite_natApp",
    "Mixed_Ops_Compose_Composite_naturality", "Mixed_Ops_Compose_First_NatTrans", "Mixed_Ops_Compose_First_natApp",
    "Mixed_Ops_Compose_First_naturality", "Mixed_Ops_Compose_Second_NatTrans", "Mixed_Ops_Compose_Second_natApp",
    "Mixed_Ops_Compose_Second_naturality", "Mixed_Ops_Compose_targetEqSymm", "Mixed_Ops_Compose_whiskerLeft",
    "Mixed_Ops_Compose_whiskerRight", "Mixed_Ops_Composite_Base_Functor", "Mixed_Ops_Composite_Base_Source_Category",
    "Mixed_Ops_Composite_Base_Source_Hom", "Mixed_Ops_Composite_Base_Source_assoc",
    "Mixed_Ops_Composite_Base_Source_comp", "Mixed_Ops_Composite_Base_Source_compId",
    "Mixed_Ops_Composite_Base_Source_eqCongr", "Mixed_Ops_Composite_Base_Source_eqTrans",
    "Mixed_Ops_Composite_Base_Source_id", "Mixed_Ops_Composite_Base_Source_idComp",
    "Mixed_Ops_Composite_Base_Target_Category", "Mixed_Ops_Composite_Base_Target_Hom",
    "Mixed_Ops_Composite_Base_Target_assoc", "Mixed_Ops_Composite_Base_Target_comp",
    "Mixed_Ops_Composite_Base_Target_compId", "Mixed_Ops_Composite_Base_Target_eqCongr",
    "Mixed_Ops_Composite_Base_Target_eqTrans", "Mixed_Ops_Composite_Base_Target_id",
    "Mixed_Ops_Composite_Base_Target_idComp", "Mixed_Ops_Composite_Base_eqCongr",
    "Mixed_Ops_Composite_Base_functorMap", "Mixed_Ops_Composite_Base_functorMapComp",
    "Mixed_Ops_Composite_Base_functorMapId", "Mixed_Ops_Composite_Base_functorObj", "Mixed_Ops_Composite_NatTrans",
    "Mixed_Ops_Composite_idNat", "Mixed_Ops_Composite_natApp", "Mixed_Ops_Composite_naturality",
    "Mixed_Ops_Composite_targetEqSymm", "Mixed_Ops_Composite_vcomp", "Mixed_Ops_Composite_vcompLaw",
    "Mixed_Ops_First_Base_Functor", "Mixed_Ops_First_Base_Source_Category", "Mixed_Ops_First_Base_Source_Hom",
    "Mixed_Ops_First_Base_Source_assoc", "Mixed_Ops_First_Base_Source_comp", "Mixed_Ops_First_Base_Source_compId",
    "Mixed_Ops_First_Base_Source_eqCongr", "Mixed_Ops_First_Base_Source_eqTrans", "Mixed_Ops_First_Base_Source_id",
    "Mixed_Ops_First_Base_Source_idComp", "Mixed_Ops_First_Base_Target_Category", "Mixed_Ops_First_Base_Target_Hom",
    "Mixed_Ops_First_Base_Target_assoc", "Mixed_Ops_First_Base_Target_comp", "Mixed_Ops_First_Base_Target_compId",
    "Mixed_Ops_First_Base_Target_eqCongr", "Mixed_Ops_First_Base_Target_eqTrans", "Mixed_Ops_First_Base_Target_id",
    "Mixed_Ops_First_Base_Target_idComp", "Mixed_Ops_First_Base_eqCongr", "Mixed_Ops_First_Base_functorMap",
    "Mixed_Ops_First_Base_functorMapComp", "Mixed_Ops_First_Base_functorMapId", "Mixed_Ops_First_Base_functorObj",
    "Mixed_Ops_First_NatTrans", "Mixed_Ops_First_idNat", "Mixed_Ops_First_natApp", "Mixed_Ops_First_naturality",
    "Mixed_Ops_First_targetEqSymm", "Mixed_Ops_First_vcomp", "Mixed_Ops_First_vcompLaw", "Mixed_Ops_Middle_Category",
    "Mixed_Ops_Middle_Hom", "Mixed_Ops_Middle_assoc", "Mixed_Ops_Middle_comp", "Mixed_Ops_Middle_compId",
    "Mixed_Ops_Middle_eqCongr", "Mixed_Ops_Middle_eqTrans", "Mixed_Ops_Middle_id", "Mixed_Ops_Middle_idComp",
    "Mixed_Ops_Second_Base_Functor", "Mixed_Ops_Second_Base_Source_Category", "Mixed_Ops_Second_Base_Source_Hom",
    "Mixed_Ops_Second_Base_Source_assoc", "Mixed_Ops_Second_Base_Source_comp", "Mixed_Ops_Second_Base_Source_compId",
    "Mixed_Ops_Second_Base_Source_eqCongr", "Mixed_Ops_Second_Base_Source_eqTrans", "Mixed_Ops_Second_Base_Source_id",
    "Mixed_Ops_Second_Base_Source_idComp", "Mixed_Ops_Second_Base_Target_Category",
    "Mixed_Ops_Second_Base_Target_Hom", "Mixed_Ops_Second_Base_Target_assoc", "Mixed_Ops_Second_Base_Target_comp",
    "Mixed_Ops_Second_Base_Target_compId", "Mixed_Ops_Second_Base_Target_eqCongr",
    "Mixed_Ops_Second_Base_Target_eqTrans", "Mixed_Ops_Second_Base_Target_id", "Mixed_Ops_Second_Base_Target_idComp",
    "Mixed_Ops_Second_Base_eqCongr", "Mixed_Ops_Second_Base_functorMap", "Mixed_Ops_Second_Base_functorMapComp",
    "Mixed_Ops_Second_Base_functorMapId", "Mixed_Ops_Second_Base_functorObj", "Mixed_Ops_Second_NatTrans",
    "Mixed_Ops_Second_idNat", "Mixed_Ops_Second_natApp", "Mixed_Ops_Second_naturality",
    "Mixed_Ops_Second_targetEqSymm", "Mixed_Ops_Second_vcomp", "Mixed_Ops_Second_vcompLaw",
    "Mixed_Ops_Source_Category", "Mixed_Ops_Source_Hom", "Mixed_Ops_Source_assoc", "Mixed_Ops_Source_comp",
    "Mixed_Ops_Source_compId", "Mixed_Ops_Source_eqCongr", "Mixed_Ops_Source_eqTrans", "Mixed_Ops_Source_id",
    "Mixed_Ops_Source_idComp", "Mixed_Ops_Target_Category", "Mixed_Ops_Target_Hom", "Mixed_Ops_Target_assoc",
    "Mixed_Ops_Target_comp", "Mixed_Ops_Target_compId", "Mixed_Ops_Target_eqCongr", "Mixed_Ops_Target_eqTrans",
    "Mixed_Ops_Target_id", "Mixed_Ops_Target_idComp", "Mixed_Ops_hcomp", "Mixed_Second_NatTransEq",
    "Mixed_Second_Ops_Base_Functor", "Mixed_Second_Ops_Base_Source_Category", "Mixed_Second_Ops_Base_Source_Hom",
    "Mixed_Second_Ops_Base_Source_assoc", "Mixed_Second_Ops_Base_Source_comp", "Mixed_Second_Ops_Base_Source_compId",
    "Mixed_Second_Ops_Base_Source_eqCongr", "Mixed_Second_Ops_Base_Source_eqTrans", "Mixed_Second_Ops_Base_Source_id",
    "Mixed_Second_Ops_Base_Source_idComp", "Mixed_Second_Ops_Base_Target_Category",
    "Mixed_Second_Ops_Base_Target_Hom", "Mixed_Second_Ops_Base_Target_assoc", "Mixed_Second_Ops_Base_Target_comp",
    "Mixed_Second_Ops_Base_Target_compId", "Mixed_Second_Ops_Base_Target_eqCongr",
    "Mixed_Second_Ops_Base_Target_eqTrans", "Mixed_Second_Ops_Base_Target_id", "Mixed_Second_Ops_Base_Target_idComp",
    "Mixed_Second_Ops_Base_eqCongr", "Mixed_Second_Ops_Base_functorMap", "Mixed_Second_Ops_Base_functorMapComp",
    "Mixed_Second_Ops_Base_functorMapId", "Mixed_Second_Ops_Base_functorObj", "Mixed_Second_Ops_NatTrans",
    "Mixed_Second_Ops_idNat", "Mixed_Second_Ops_natApp", "Mixed_Second_Ops_naturality",
    "Mixed_Second_Ops_targetEqSymm", "Mixed_Second_Ops_vcomp", "Mixed_Second_Ops_vcompLaw", "Mixed_Second_eqRefl",
    "Mixed_Second_eqSymm", "Mixed_Second_eqTrans", "Mixed_Second_idVcomp", "Mixed_Second_vcompAssoc",
    "Mixed_Second_vcompCongr", "Mixed_Second_vcompId", "Mixed_whiskerLeftCongr", "Mixed_whiskerLeftId",
    "Mixed_whiskerLeftVcomp", "Mixed_whiskerRightCongr", "Mixed_whiskerRightId", "Mixed_whiskerRightVcomp", "PairEnd",
    "Right", "Run_Compose_Base_Composite_Functor", "Run_Compose_Base_Composite_eqCongr",
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
    "SeparateTarget_eqTrans", "SeparateTarget_id", "SeparateTarget_idComp", "Vertical", "Wide_Composite_NatTransEq",
    "Wide_Composite_Ops_Base_Functor", "Wide_Composite_Ops_Base_Source_Category",
    "Wide_Composite_Ops_Base_Source_Hom", "Wide_Composite_Ops_Base_Source_assoc",
    "Wide_Composite_Ops_Base_Source_comp", "Wide_Composite_Ops_Base_Source_compId",
    "Wide_Composite_Ops_Base_Source_eqCongr", "Wide_Composite_Ops_Base_Source_eqTrans",
    "Wide_Composite_Ops_Base_Source_id", "Wide_Composite_Ops_Base_Source_idComp",
    "Wide_Composite_Ops_Base_Target_Category", "Wide_Composite_Ops_Base_Target_Hom",
    "Wide_Composite_Ops_Base_Target_assoc", "Wide_Composite_Ops_Base_Target_comp",
    "Wide_Composite_Ops_Base_Target_compId", "Wide_Composite_Ops_Base_Target_eqCongr",
    "Wide_Composite_Ops_Base_Target_eqTrans", "Wide_Composite_Ops_Base_Target_id",
    "Wide_Composite_Ops_Base_Target_idComp", "Wide_Composite_Ops_Base_eqCongr", "Wide_Composite_Ops_Base_functorMap",
    "Wide_Composite_Ops_Base_functorMapComp", "Wide_Composite_Ops_Base_functorMapId",
    "Wide_Composite_Ops_Base_functorObj", "Wide_Composite_Ops_NatTrans", "Wide_Composite_Ops_idNat",
    "Wide_Composite_Ops_natApp", "Wide_Composite_Ops_naturality", "Wide_Composite_Ops_targetEqSymm",
    "Wide_Composite_Ops_vcomp", "Wide_Composite_Ops_vcompLaw", "Wide_Composite_eqRefl", "Wide_Composite_eqSymm",
    "Wide_Composite_eqTrans", "Wide_Composite_idVcomp", "Wide_Composite_vcompAssoc", "Wide_Composite_vcompCongr",
    "Wide_Composite_vcompId", "Wide_First_NatTransEq", "Wide_First_Ops_Base_Functor",
    "Wide_First_Ops_Base_Source_Category", "Wide_First_Ops_Base_Source_Hom", "Wide_First_Ops_Base_Source_assoc",
    "Wide_First_Ops_Base_Source_comp", "Wide_First_Ops_Base_Source_compId", "Wide_First_Ops_Base_Source_eqCongr",
    "Wide_First_Ops_Base_Source_eqTrans", "Wide_First_Ops_Base_Source_id", "Wide_First_Ops_Base_Source_idComp",
    "Wide_First_Ops_Base_Target_Category", "Wide_First_Ops_Base_Target_Hom", "Wide_First_Ops_Base_Target_assoc",
    "Wide_First_Ops_Base_Target_comp", "Wide_First_Ops_Base_Target_compId", "Wide_First_Ops_Base_Target_eqCongr",
    "Wide_First_Ops_Base_Target_eqTrans", "Wide_First_Ops_Base_Target_id", "Wide_First_Ops_Base_Target_idComp",
    "Wide_First_Ops_Base_eqCongr", "Wide_First_Ops_Base_functorMap", "Wide_First_Ops_Base_functorMapComp",
    "Wide_First_Ops_Base_functorMapId", "Wide_First_Ops_Base_functorObj", "Wide_First_Ops_NatTrans",
    "Wide_First_Ops_idNat", "Wide_First_Ops_natApp", "Wide_First_Ops_naturality", "Wide_First_Ops_targetEqSymm",
    "Wide_First_Ops_vcomp", "Wide_First_Ops_vcompLaw", "Wide_First_eqRefl", "Wide_First_eqSymm", "Wide_First_eqTrans",
    "Wide_First_idVcomp", "Wide_First_vcompAssoc", "Wide_First_vcompCongr", "Wide_First_vcompId",
    "Wide_Ops_Compose_Base_Composite_Functor", "Wide_Ops_Compose_Base_Composite_eqCongr",
    "Wide_Ops_Compose_Base_Composite_functorMap", "Wide_Ops_Compose_Base_Composite_functorMapComp",
    "Wide_Ops_Compose_Base_Composite_functorMapId", "Wide_Ops_Compose_Base_Composite_functorObj",
    "Wide_Ops_Compose_Base_First_Functor", "Wide_Ops_Compose_Base_First_eqCongr",
    "Wide_Ops_Compose_Base_First_functorMap", "Wide_Ops_Compose_Base_First_functorMapComp",
    "Wide_Ops_Compose_Base_First_functorMapId", "Wide_Ops_Compose_Base_First_functorObj",
    "Wide_Ops_Compose_Base_Middle_Category", "Wide_Ops_Compose_Base_Middle_Hom", "Wide_Ops_Compose_Base_Middle_assoc",
    "Wide_Ops_Compose_Base_Middle_comp", "Wide_Ops_Compose_Base_Middle_compId",
    "Wide_Ops_Compose_Base_Middle_eqCongr", "Wide_Ops_Compose_Base_Middle_eqTrans", "Wide_Ops_Compose_Base_Middle_id",
    "Wide_Ops_Compose_Base_Middle_idComp", "Wide_Ops_Compose_Base_Second_Functor",
    "Wide_Ops_Compose_Base_Second_eqCongr", "Wide_Ops_Compose_Base_Second_functorMap",
    "Wide_Ops_Compose_Base_Second_functorMapComp", "Wide_Ops_Compose_Base_Second_functorMapId",
    "Wide_Ops_Compose_Base_Second_functorObj", "Wide_Ops_Compose_Base_Source_Category",
    "Wide_Ops_Compose_Base_Source_Hom", "Wide_Ops_Compose_Base_Source_assoc", "Wide_Ops_Compose_Base_Source_comp",
    "Wide_Ops_Compose_Base_Source_compId", "Wide_Ops_Compose_Base_Source_eqCongr",
    "Wide_Ops_Compose_Base_Source_eqTrans", "Wide_Ops_Compose_Base_Source_id", "Wide_Ops_Compose_Base_Source_idComp",
    "Wide_Ops_Compose_Base_Target_Category", "Wide_Ops_Compose_Base_Target_Hom", "Wide_Ops_Compose_Base_Target_assoc",
    "Wide_Ops_Compose_Base_Target_comp", "Wide_Ops_Compose_Base_Target_compId",
    "Wide_Ops_Compose_Base_Target_eqCongr", "Wide_Ops_Compose_Base_Target_eqTrans", "Wide_Ops_Compose_Base_Target_id",
    "Wide_Ops_Compose_Base_Target_idComp", "Wide_Ops_Compose_Base_compFunctor", "Wide_Ops_Compose_Composite_NatTrans",
    "Wide_Ops_Compose_Composite_natApp", "Wide_Ops_Compose_Composite_naturality", "Wide_Ops_Compose_First_NatTrans",
    "Wide_Ops_Compose_First_natApp", "Wide_Ops_Compose_First_naturality", "Wide_Ops_Compose_Second_NatTrans",
    "Wide_Ops_Compose_Second_natApp", "Wide_Ops_Compose_Second_naturality", "Wide_Ops_Compose_targetEqSymm",
    "Wide_Ops_Compose_whiskerLeft", "Wide_Ops_Compose_whiskerRight", "Wide_Ops_Composite_Base_Functor",
    "Wide_Ops_Composite_Base_Source_Category", "Wide_Ops_Composite_Base_Source_Hom",
    "Wide_Ops_Composite_Base_Source_assoc", "Wide_Ops_Composite_Base_Source_comp",
    "Wide_Ops_Composite_Base_Source_compId", "Wide_Ops_Composite_Base_Source_eqCongr",
    "Wide_Ops_Composite_Base_Source_eqTrans", "Wide_Ops_Composite_Base_Source_id",
    "Wide_Ops_Composite_Base_Source_idComp", "Wide_Ops_Composite_Base_Target_Category",
    "Wide_Ops_Composite_Base_Target_Hom", "Wide_Ops_Composite_Base_Target_assoc",
    "Wide_Ops_Composite_Base_Target_comp", "Wide_Ops_Composite_Base_Target_compId",
    "Wide_Ops_Composite_Base_Target_eqCongr", "Wide_Ops_Composite_Base_Target_eqTrans",
    "Wide_Ops_Composite_Base_Target_id", "Wide_Ops_Composite_Base_Target_idComp", "Wide_Ops_Composite_Base_eqCongr",
    "Wide_Ops_Composite_Base_functorMap", "Wide_Ops_Composite_Base_functorMapComp",
    "Wide_Ops_Composite_Base_functorMapId", "Wide_Ops_Composite_Base_functorObj", "Wide_Ops_Composite_NatTrans",
    "Wide_Ops_Composite_idNat", "Wide_Ops_Composite_natApp", "Wide_Ops_Composite_naturality",
    "Wide_Ops_Composite_targetEqSymm", "Wide_Ops_Composite_vcomp", "Wide_Ops_Composite_vcompLaw",
    "Wide_Ops_First_Base_Functor", "Wide_Ops_First_Base_Source_Category", "Wide_Ops_First_Base_Source_Hom",
    "Wide_Ops_First_Base_Source_assoc", "Wide_Ops_First_Base_Source_comp", "Wide_Ops_First_Base_Source_compId",
    "Wide_Ops_First_Base_Source_eqCongr", "Wide_Ops_First_Base_Source_eqTrans", "Wide_Ops_First_Base_Source_id",
    "Wide_Ops_First_Base_Source_idComp", "Wide_Ops_First_Base_Target_Category", "Wide_Ops_First_Base_Target_Hom",
    "Wide_Ops_First_Base_Target_assoc", "Wide_Ops_First_Base_Target_comp", "Wide_Ops_First_Base_Target_compId",
    "Wide_Ops_First_Base_Target_eqCongr", "Wide_Ops_First_Base_Target_eqTrans", "Wide_Ops_First_Base_Target_id",
    "Wide_Ops_First_Base_Target_idComp", "Wide_Ops_First_Base_eqCongr", "Wide_Ops_First_Base_functorMap",
    "Wide_Ops_First_Base_functorMapComp", "Wide_Ops_First_Base_functorMapId", "Wide_Ops_First_Base_functorObj",
    "Wide_Ops_First_NatTrans", "Wide_Ops_First_idNat", "Wide_Ops_First_natApp", "Wide_Ops_First_naturality",
    "Wide_Ops_First_targetEqSymm", "Wide_Ops_First_vcomp", "Wide_Ops_First_vcompLaw", "Wide_Ops_Middle_Category",
    "Wide_Ops_Middle_Hom", "Wide_Ops_Middle_assoc", "Wide_Ops_Middle_comp", "Wide_Ops_Middle_compId",
    "Wide_Ops_Middle_eqCongr", "Wide_Ops_Middle_eqTrans", "Wide_Ops_Middle_id", "Wide_Ops_Middle_idComp",
    "Wide_Ops_Second_Base_Functor", "Wide_Ops_Second_Base_Source_Category", "Wide_Ops_Second_Base_Source_Hom",
    "Wide_Ops_Second_Base_Source_assoc", "Wide_Ops_Second_Base_Source_comp", "Wide_Ops_Second_Base_Source_compId",
    "Wide_Ops_Second_Base_Source_eqCongr", "Wide_Ops_Second_Base_Source_eqTrans", "Wide_Ops_Second_Base_Source_id",
    "Wide_Ops_Second_Base_Source_idComp", "Wide_Ops_Second_Base_Target_Category", "Wide_Ops_Second_Base_Target_Hom",
    "Wide_Ops_Second_Base_Target_assoc", "Wide_Ops_Second_Base_Target_comp", "Wide_Ops_Second_Base_Target_compId",
    "Wide_Ops_Second_Base_Target_eqCongr", "Wide_Ops_Second_Base_Target_eqTrans", "Wide_Ops_Second_Base_Target_id",
    "Wide_Ops_Second_Base_Target_idComp", "Wide_Ops_Second_Base_eqCongr", "Wide_Ops_Second_Base_functorMap",
    "Wide_Ops_Second_Base_functorMapComp", "Wide_Ops_Second_Base_functorMapId", "Wide_Ops_Second_Base_functorObj",
    "Wide_Ops_Second_NatTrans", "Wide_Ops_Second_idNat", "Wide_Ops_Second_natApp", "Wide_Ops_Second_naturality",
    "Wide_Ops_Second_targetEqSymm", "Wide_Ops_Second_vcomp", "Wide_Ops_Second_vcompLaw", "Wide_Ops_Source_Category",
    "Wide_Ops_Source_Hom", "Wide_Ops_Source_assoc", "Wide_Ops_Source_comp", "Wide_Ops_Source_compId",
    "Wide_Ops_Source_eqCongr", "Wide_Ops_Source_eqTrans", "Wide_Ops_Source_id", "Wide_Ops_Source_idComp",
    "Wide_Ops_Target_Category", "Wide_Ops_Target_Hom", "Wide_Ops_Target_assoc", "Wide_Ops_Target_comp",
    "Wide_Ops_Target_compId", "Wide_Ops_Target_eqCongr", "Wide_Ops_Target_eqTrans", "Wide_Ops_Target_id",
    "Wide_Ops_Target_idComp", "Wide_Ops_hcomp", "Wide_Second_NatTransEq", "Wide_Second_Ops_Base_Functor",
    "Wide_Second_Ops_Base_Source_Category", "Wide_Second_Ops_Base_Source_Hom", "Wide_Second_Ops_Base_Source_assoc",
    "Wide_Second_Ops_Base_Source_comp", "Wide_Second_Ops_Base_Source_compId", "Wide_Second_Ops_Base_Source_eqCongr",
    "Wide_Second_Ops_Base_Source_eqTrans", "Wide_Second_Ops_Base_Source_id", "Wide_Second_Ops_Base_Source_idComp",
    "Wide_Second_Ops_Base_Target_Category", "Wide_Second_Ops_Base_Target_Hom", "Wide_Second_Ops_Base_Target_assoc",
    "Wide_Second_Ops_Base_Target_comp", "Wide_Second_Ops_Base_Target_compId", "Wide_Second_Ops_Base_Target_eqCongr",
    "Wide_Second_Ops_Base_Target_eqTrans", "Wide_Second_Ops_Base_Target_id", "Wide_Second_Ops_Base_Target_idComp",
    "Wide_Second_Ops_Base_eqCongr", "Wide_Second_Ops_Base_functorMap", "Wide_Second_Ops_Base_functorMapComp",
    "Wide_Second_Ops_Base_functorMapId", "Wide_Second_Ops_Base_functorObj", "Wide_Second_Ops_NatTrans",
    "Wide_Second_Ops_idNat", "Wide_Second_Ops_natApp", "Wide_Second_Ops_naturality", "Wide_Second_Ops_targetEqSymm",
    "Wide_Second_Ops_vcomp", "Wide_Second_Ops_vcompLaw", "Wide_Second_eqRefl", "Wide_Second_eqSymm",
    "Wide_Second_eqTrans", "Wide_Second_idVcomp", "Wide_Second_vcompAssoc", "Wide_Second_vcompCongr",
    "Wide_Second_vcompId", "Wide_whiskerLeftCongr", "Wide_whiskerLeftId", "Wide_whiskerLeftVcomp",
    "Wide_whiskerRightCongr", "Wide_whiskerRightId", "Wide_whiskerRightVcomp", "certifiedWhisker", "componentLaw",
    "components", "horizontalFirst", "horizontalSecond", "identityValue", "leftCongrLaw", "leftCongrLeftAt",
    "leftCongrLhs", "leftCongrRhs", "leftCongrRightAt", "leftIdLaw", "leftIdLeftAt", "leftIdLhs", "leftIdRhs",
    "leftIdRightAt", "leftVcompLaw", "leftVcompLeftAt", "leftVcompLhs", "leftVcompRhs", "leftVcompRightAt",
    "naturalityWitness", "pairCode", "pathTrans", "rightCongrLaw", "rightCongrLeftAt", "rightCongrLhs",
    "rightCongrRhs", "rightCongrRightAt", "rightIdLaw", "rightIdLeftAt", "rightIdLhs", "rightIdRhs", "rightIdRightAt",
    "rightVcompLaw", "rightVcompLeftAt", "rightVcompLhs", "rightVcompRhs", "rightVcompRightAt", "sharedNatInput",
    "verticalValue"
};

/* expected_families() (tests/prelude_whiskering_laws.bend line 15, 12 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "Mixed_Ops_Middle", "Mixed_Ops_Source", "Mixed_Ops_Target", "Path", "Point", "Run_Middle", "Run_Source",
    "Run_Target", "SeparateTarget", "Wide_Ops_Middle", "Wide_Ops_Source", "Wide_Ops_Target"
};

/* negatives() (tests/prelude_whiskering_laws.bend line 18, 6 names) */
static const char *const NEGATIVES[] = {
    "false-components", "one-component", "missing-left-congruence", "missing-right-congruence", "nominal-target",
    "wrong-endpoint"
};

/* values() (24 Program.Probe rows) */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  probe(heap, &out, "rightIdLeftAt", 37, 10007);
  probe(heap, &out, "rightIdRightAt", 37, 10007);
  probe(heap, &out, "rightVcompLeftAt", 37, 10005);
  probe(heap, &out, "rightVcompRightAt", 37, 10005);
  probe(heap, &out, "rightCongrLeftAt", 37, 800);
  probe(heap, &out, "rightCongrRightAt", 37, 800);
  probe(heap, &out, "leftIdLeftAt", 37, 5007);
  probe(heap, &out, "leftIdRightAt", 37, 5007);
  probe(heap, &out, "leftVcompLeftAt", 37, 14207);
  probe(heap, &out, "leftVcompRightAt", 37, 14207);
  probe(heap, &out, "leftCongrLeftAt", 37, 13707);
  probe(heap, &out, "leftCongrRightAt", 37, 13707);
  probe(heap, &out, "rightIdLeftAt", 41, 10807);
  probe(heap, &out, "rightIdRightAt", 41, 10807);
  probe(heap, &out, "rightVcompLeftAt", 41, 10805);
  probe(heap, &out, "rightVcompRightAt", 41, 10805);
  probe(heap, &out, "rightCongrLeftAt", 41, 808);
  probe(heap, &out, "rightCongrRightAt", 41, 808);
  probe(heap, &out, "leftIdLeftAt", 41, 5407);
  probe(heap, &out, "leftIdRightAt", 41, 5407);
  probe(heap, &out, "leftVcompLeftAt", 41, 15407);
  probe(heap, &out, "leftVcompRightAt", 41, 15407);
  probe(heap, &out, "leftCongrLeftAt", 41, 14907);
  probe(heap, &out, "leftCongrRightAt", 41, 14907);
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech", "prelude/cat/nattrans-laws.mech",
    "prelude/cat/whiskering-laws.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/whiskering-laws.mech", "test/fixtures/prelude/shared-nattrans-runtime.mech",
    "test/fixtures/prelude/whiskering-laws-runtime.mech"
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
  RELATIONAL_TRY(program_probes(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/whiskering-laws"), &negatives, globals,
                               negatives_prefix(40, false, false)));
  *fields = str_lit("entries=1177 families=12 computations=24 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-WHISKERING-LAWS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-WHISKERING-LAWS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
