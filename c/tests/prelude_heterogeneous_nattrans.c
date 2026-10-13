/* Port of bend2/tests/prelude_heterogeneous_nattrans.bend (46 lines): the heterogeneous natural transformation
 * prelude suite. Reads the prelude layers and the fixture clients under argv[1] (default "."), checks the inventory
 * against the initial globals, the required instances, the computations, the pinned negatives, the universe arity
 * refusal and the budget refusal, then prints PRELUDE-HETEROGENEOUS-NATTRANS-OK fields or
 * PRELUDE-HETEROGENEOUS-NATTRANS-FAIL message. */
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

/* expected_families() */
static const char *const EXPECTED_FAMILIES[] = {"Equal_Base_Source", "Equal_Base_Target", "Mixed_Base_Source",
                                                "Mixed_Base_Target", "Path",              "Point",
                                                "Run_Base_Source",   "Run_Base_Target",   "Wide_Base_Source",
                                                "Wide_Base_Target"};

/* negatives() */
static const char *const NEGATIVES[] = {"erased-object",     "missing-law",       "nominal-category", "nominal-equality",
                                        "target-record",     "wrong-endpoint",    "wrong-middle",     "wrong-nat-sort",
                                        "wrong-naturality",  "wrong-source-level", "wrong-target-level"};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "componentValue", relational_natural(heap, 37));
  computation(heap, &out, "identityValue", relational_natural(heap, 42));
  computation(heap, &out, "verticalForward", relational_natural(heap, 74));
  computation(heap, &out, "verticalReverse", relational_natural(heap, 37));
  computation(heap, &out, "verticalNested", relational_natural(heap, 111));
  return out;
}

/* The R.read_all path lists of suite() */
static const char *const SOURCE_PATHS[] = {"prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
                                           "prelude/cat/heterogeneous-nattrans.mech"};
static const char *const FIXTURE_PATHS[] = {"test/fixtures/prelude/heterogeneous-nattrans.mech",
                                            "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech"};

/* R.prefixes(["Mixed", "Wide", "Equal", "Run"], [...]) of suite() */
static const char *const INSTANCE_NAMES[] = {"Mixed", "Wide", "Equal", "Run"};
static const char *const INSTANCE_MEMBERS[] = {
    "NatTrans",           "natApp",              "naturality",          "idNat",
    "vcomp",              "vcompLaw",            "targetEqSymm",        "Base_Functor",
    "Base_functorObj",    "Base_functorMap",     "Base_functorMapId",   "Base_functorMapComp",
    "Base_eqCongr",       "Base_Source_Category", "Base_Source_Hom",    "Base_Source_id",
    "Base_Source_comp",   "Base_Source_idComp",  "Base_Source_compId",  "Base_Source_assoc",
    "Base_Source_eqTrans", "Base_Source_eqCongr", "Base_Target_Category", "Base_Target_Hom",
    "Base_Target_id",     "Base_Target_comp",    "Base_Target_idComp",  "Base_Target_compId",
    "Base_Target_assoc",  "Base_Target_eqTrans", "Base_Target_eqCongr"};

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
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
  RELATIONAL_TRY(relational_read_all(heap, root, &source_paths, &source));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, source, fixture), &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &symbolic));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(symbolic), str_lit("symbolic definitions escaped")));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  globals = relational_globals(checked);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_count_initial(heap, checked, 150, &families));
  instance_names = STRINGS(heap, INSTANCE_NAMES);
  instance_members = STRINGS(heap, INSTANCE_MEMBERS);
  required = relational_prefixes(heap, &instance_names, &instance_members);
  RELATIONAL_TRY(relational_required(heap, &required, globals));
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/heterogeneous-nattrans"), &negatives, globals,
                               negatives_prefix(9, true, false)));
  RELATIONAL_TRY(relational_refuse_exact(
      heap,
      elab_check_in_compact(heap, global_empty(),
                    cat(heap, source, str_lit("specialize MechHeterogeneousNatTrans (0, 1, 2) as Bad\n"))),
      str_lit("universe: the family schema MechHeterogeneousNatTrans expects 4 universe arguments, got 3\n"),
      str_lit("universe arity")));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=150 instances=4 computations=5 negatives=13");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-HETEROGENEOUS-NATTRANS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-HETEROGENEOUS-NATTRANS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
