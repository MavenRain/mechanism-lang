/* Port of tests/prelude_nattrans_laws.bend (41 lines): the nattrans laws prelude suite. Reads the prelude layers and the fixture
 * clients under argv[1] (default "."), runs the relational checks of suite() in the Bend order and prints
 * PRELUDE-NATTRANS-LAWS-OK fields or PRELUDE-NATTRANS-LAWS-FAIL message. Same definitions in the same order. */
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

/* expected_entries() (tests/prelude_nattrans_laws.bend line 12, 234 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "AssocLeft", "AssocRight", "Beta", "BetaComponents", "Components", "CongrLeft", "CongrRight", "Constant",
    "DoubleEnd", "IndexedEnd", "Laws_NatTransEq", "Laws_Ops_Base_Functor", "Laws_Ops_Base_Source_Category",
    "Laws_Ops_Base_Source_Hom", "Laws_Ops_Base_Source_assoc", "Laws_Ops_Base_Source_comp",
    "Laws_Ops_Base_Source_compId", "Laws_Ops_Base_Source_eqCongr", "Laws_Ops_Base_Source_eqTrans",
    "Laws_Ops_Base_Source_id", "Laws_Ops_Base_Source_idComp", "Laws_Ops_Base_Target_Category",
    "Laws_Ops_Base_Target_Hom", "Laws_Ops_Base_Target_assoc", "Laws_Ops_Base_Target_comp",
    "Laws_Ops_Base_Target_compId", "Laws_Ops_Base_Target_eqCongr", "Laws_Ops_Base_Target_eqTrans",
    "Laws_Ops_Base_Target_id", "Laws_Ops_Base_Target_idComp", "Laws_Ops_Base_eqCongr", "Laws_Ops_Base_functorMap",
    "Laws_Ops_Base_functorMapComp", "Laws_Ops_Base_functorMapId", "Laws_Ops_Base_functorObj", "Laws_Ops_NatTrans",
    "Laws_Ops_idNat", "Laws_Ops_natApp", "Laws_Ops_naturality", "Laws_Ops_targetEqSymm", "Laws_Ops_vcomp",
    "Laws_Ops_vcompLaw", "Laws_eqRefl", "Laws_eqSymm", "Laws_eqTrans", "Laws_idVcomp", "Laws_vcompAssoc",
    "Laws_vcompCongr", "Laws_vcompId", "LeftIdentity", "Mixed_NatTransEq", "Mixed_Ops_Base_Functor",
    "Mixed_Ops_Base_Source_Category", "Mixed_Ops_Base_Source_Hom", "Mixed_Ops_Base_Source_assoc",
    "Mixed_Ops_Base_Source_comp", "Mixed_Ops_Base_Source_compId", "Mixed_Ops_Base_Source_eqCongr",
    "Mixed_Ops_Base_Source_eqTrans", "Mixed_Ops_Base_Source_id", "Mixed_Ops_Base_Source_idComp",
    "Mixed_Ops_Base_Target_Category", "Mixed_Ops_Base_Target_Hom", "Mixed_Ops_Base_Target_assoc",
    "Mixed_Ops_Base_Target_comp", "Mixed_Ops_Base_Target_compId", "Mixed_Ops_Base_Target_eqCongr",
    "Mixed_Ops_Base_Target_eqTrans", "Mixed_Ops_Base_Target_id", "Mixed_Ops_Base_Target_idComp",
    "Mixed_Ops_Base_eqCongr", "Mixed_Ops_Base_functorMap", "Mixed_Ops_Base_functorMapComp",
    "Mixed_Ops_Base_functorMapId", "Mixed_Ops_Base_functorObj", "Mixed_Ops_NatTrans", "Mixed_Ops_idNat",
    "Mixed_Ops_natApp", "Mixed_Ops_naturality", "Mixed_Ops_targetEqSymm", "Mixed_Ops_vcomp", "Mixed_Ops_vcompLaw",
    "Mixed_eqRefl", "Mixed_eqSymm", "Mixed_eqTrans", "Mixed_idVcomp", "Mixed_vcompAssoc", "Mixed_vcompCongr",
    "Mixed_vcompId", "RightIdentity", "Run_Base_Functor", "Run_Base_Source_Category", "Run_Base_Source_Hom",
    "Run_Base_Source_assoc", "Run_Base_Source_comp", "Run_Base_Source_compId", "Run_Base_Source_eqCongr",
    "Run_Base_Source_eqTrans", "Run_Base_Source_id", "Run_Base_Source_idComp", "Run_Base_Target_Category",
    "Run_Base_Target_Hom", "Run_Base_Target_assoc", "Run_Base_Target_comp", "Run_Base_Target_compId",
    "Run_Base_Target_eqCongr", "Run_Base_Target_eqTrans", "Run_Base_Target_id", "Run_Base_Target_idComp",
    "Run_Base_eqCongr", "Run_Base_functorMap", "Run_Base_functorMapComp", "Run_Base_functorMapId",
    "Run_Base_functorObj", "Run_NatTrans", "Run_idNat", "Run_natApp", "Run_naturality", "Run_targetEqSymm",
    "Run_vcomp", "Run_vcompLaw", "Separate_NatTransEq", "Separate_Ops_Base_Functor",
    "Separate_Ops_Base_Source_Category", "Separate_Ops_Base_Source_Hom", "Separate_Ops_Base_Source_assoc",
    "Separate_Ops_Base_Source_comp", "Separate_Ops_Base_Source_compId", "Separate_Ops_Base_Source_eqCongr",
    "Separate_Ops_Base_Source_eqTrans", "Separate_Ops_Base_Source_id", "Separate_Ops_Base_Source_idComp",
    "Separate_Ops_Base_Target_Category", "Separate_Ops_Base_Target_Hom", "Separate_Ops_Base_Target_assoc",
    "Separate_Ops_Base_Target_comp", "Separate_Ops_Base_Target_compId", "Separate_Ops_Base_Target_eqCongr",
    "Separate_Ops_Base_Target_eqTrans", "Separate_Ops_Base_Target_id", "Separate_Ops_Base_Target_idComp",
    "Separate_Ops_Base_eqCongr", "Separate_Ops_Base_functorMap", "Separate_Ops_Base_functorMapComp",
    "Separate_Ops_Base_functorMapId", "Separate_Ops_Base_functorObj", "Separate_Ops_NatTrans", "Separate_Ops_idNat",
    "Separate_Ops_natApp", "Separate_Ops_naturality", "Separate_Ops_targetEqSymm", "Separate_Ops_vcomp",
    "Separate_Ops_vcompLaw", "Separate_eqRefl", "Separate_eqSymm", "Separate_eqTrans", "Separate_idVcomp",
    "Separate_vcompAssoc", "Separate_vcompCongr", "Separate_vcompId", "Wide_NatTransEq", "Wide_Ops_Base_Functor",
    "Wide_Ops_Base_Source_Category", "Wide_Ops_Base_Source_Hom", "Wide_Ops_Base_Source_assoc",
    "Wide_Ops_Base_Source_comp", "Wide_Ops_Base_Source_compId", "Wide_Ops_Base_Source_eqCongr",
    "Wide_Ops_Base_Source_eqTrans", "Wide_Ops_Base_Source_id", "Wide_Ops_Base_Source_idComp",
    "Wide_Ops_Base_Target_Category", "Wide_Ops_Base_Target_Hom", "Wide_Ops_Base_Target_assoc",
    "Wide_Ops_Base_Target_comp", "Wide_Ops_Base_Target_compId", "Wide_Ops_Base_Target_eqCongr",
    "Wide_Ops_Base_Target_eqTrans", "Wide_Ops_Base_Target_id", "Wide_Ops_Base_Target_idComp", "Wide_Ops_Base_eqCongr",
    "Wide_Ops_Base_functorMap", "Wide_Ops_Base_functorMapComp", "Wide_Ops_Base_functorMapId",
    "Wide_Ops_Base_functorObj", "Wide_Ops_NatTrans", "Wide_Ops_idNat", "Wide_Ops_natApp", "Wide_Ops_naturality",
    "Wide_Ops_targetEqSymm", "Wide_Ops_vcomp", "Wide_Ops_vcompLaw", "Wide_eqRefl", "Wide_eqSymm", "Wide_eqTrans",
    "Wide_idVcomp", "Wide_vcompAssoc", "Wide_vcompCongr", "Wide_vcompId", "assocLaw", "assocLeftAt", "assocRightAt",
    "betaComponentLaw", "certifiedComponent", "componentLaw", "componentValue", "congrLaw", "congrLeftAt",
    "congrRightAt", "heterogeneousNatInput", "identityValue", "leftIdentityAt", "leftIdentityLaw", "mixedAssoc",
    "mixedComponents", "mixedCongr", "mixedLeftId", "mixedRefl", "mixedRightId", "mixedSymm", "mixedTrans",
    "naturalityWitness", "pathTrans", "reflLaw", "rightIdentityAt", "rightIdentityLaw", "symmLaw", "transLaw",
    "verticalForward", "verticalNested", "verticalReverse", "wideAssoc", "wideRelation"
};

/* expected_families() (tests/prelude_nattrans_laws.bend line 15, 10 names) */
static const char *const EXPECTED_FAMILIES[] = {
    "Mixed_Ops_Base_Source", "Mixed_Ops_Base_Target", "Path", "Point", "Run_Base_Source", "Run_Base_Target",
    "Separate_Ops_Base_Source", "Separate_Ops_Base_Target", "Wide_Ops_Base_Source", "Wide_Ops_Base_Target"
};

/* negatives() (tests/prelude_nattrans_laws.bend line 18, 6 names) */
static const char *const NEGATIVES[] = {
    "false-components", "one-component", "nominal-equality", "wrong-endpoint", "trans-middle", "missing-congruence"
};

/* values() (12 Program.Probe rows) */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  probe(heap, &out, "leftIdentityAt", 37, 37);
  probe(heap, &out, "rightIdentityAt", 37, 37);
  probe(heap, &out, "assocLeftAt", 37, 111);
  probe(heap, &out, "assocRightAt", 37, 111);
  probe(heap, &out, "congrLeftAt", 37, 74);
  probe(heap, &out, "congrRightAt", 37, 74);
  probe(heap, &out, "leftIdentityAt", 41, 41);
  probe(heap, &out, "rightIdentityAt", 41, 41);
  probe(heap, &out, "assocLeftAt", 41, 123);
  probe(heap, &out, "assocRightAt", 41, 123);
  probe(heap, &out, "congrLeftAt", 41, 82);
  probe(heap, &out, "congrRightAt", 41, 82);
  return out;
}

/* The R.read_all path list of templates in suite() */
static const char *const TEMPLATES_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/heterogeneous-nattrans.mech", "prelude/cat/nattrans-laws.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/nattrans-laws.mech", "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech",
    "test/fixtures/prelude/nattrans-laws-runtime.mech"
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
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/nattrans-laws"), &negatives, globals,
                               negatives_prefix(40, false, false)));
  *fields = str_lit("entries=234 families=10 computations=12 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-NATTRANS-LAWS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-NATTRANS-LAWS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
