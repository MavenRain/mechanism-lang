/* Port of bend2/tests/surface_prelude_equality_ops.bend (54 lines, oracle label surface-prelude-equality-ops): ten
 * instances of MechEq and MechTypeEq from the equality catalog install against their frozen contracts on
 * prelude/init.mech, the equality-ops fixture client checks and audits, eleven computations normalize and
 * eight negatives refuse. Prints PRELUDE-EQUALITY-OPS-OK fields or PRELUDE-EQUALITY-OPS-FAIL message. */
#include "driver.h"
#include "prelude.h"
#include "surface_foundation_common.h"
#include "surface_foundation_data.h"

static void push_level(Heap *heap, Vec *levels, uint64_t index) {
  const LevelVar *level = surface_foundation_common_level(heap, index);
  if (level == NULL || !vec_push(levels, (void *)level)) heap->oom = true;
}

/* equality(alias, u, v) = F.Instance{"MechEq", [F.level(u), F.level(v)], alias, alias} */
static FoundationInstance equality(Heap *heap, const char *alias, uint64_t u, uint64_t v) {
  FoundationInstance row;
  row.name = str_lit("MechEq");
  row.levels = vec_new(heap->arena);
  push_level(heap, &row.levels, u);
  push_level(heap, &row.levels, v);
  row.alias = str_lit(alias);
  row.contract = str_lit(alias);
  return row;
}

/* cast(alias, u) = F.Instance{"MechTypeEq", [F.level(u)], alias, alias} */
static FoundationInstance cast(Heap *heap, const char *alias, uint64_t u) {
  FoundationInstance row;
  row.name = str_lit("MechTypeEq");
  row.levels = vec_new(heap->arena);
  push_level(heap, &row.levels, u);
  row.alias = str_lit(alias);
  row.contract = str_lit(alias);
  return row;
}

static void push_term(Heap *heap, Vec *terms, const Term *term) {
  if (term == NULL || !vec_push(terms, (void *)term)) heap->oom = true;
}

/* computations() = F.ones([...ten names]) ++ [Pair{"opsDependent", In{SMu{"OpsFiber", [F.zero(),
 * In{SMu{"OpsData", [F.zero()]}, ACtor{"mechReflCtor"}, []}]}, ACtor{"opsFiber"}, []}}] */
static Vec computations(Heap *heap) {
  static const char *const names[] = {"opsSymm",          "opsTrans",       "opsCongr",      "opsJ",
                                      "opsProofDataJ",    "opsCastSymm",    "opsCastTrans",  "opsHigherValue",
                                      "opsTypesValue",    "opsCastHigherValue"};
  Vec rows = surface_foundation_common_ones(heap, names, 10);
  Vec data_args = vec_new(heap->arena);
  push_term(heap, &data_args, surface_foundation_common_zero(heap));
  const Term *refl = term_in(heap, shape_smu(str_lit("OpsData"), data_args), term_addr_ctor(str_lit("mechReflCtor")),
                             vec_new(heap->arena));
  Vec fiber_args = vec_new(heap->arena);
  push_term(heap, &fiber_args, surface_foundation_common_zero(heap));
  push_term(heap, &fiber_args, refl);
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) {
    heap->oom = true;
    return rows;
  }
  row->name = str_lit("opsDependent");
  row->expected = term_in(heap, shape_smu(str_lit("OpsFiber"), fiber_args), term_addr_ctor(str_lit("opsFiber")),
                          vec_new(heap->arena));
  if (row->expected == NULL || !vec_push(&rows, row)) heap->oom = true;
  return rows;
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  void *value = NULL;
  Str source = str_empty();
  Str client = str_empty();
  const ElabChecked *initial = NULL;
  const ElabChecked *checked = NULL;
  Globals installed = global_empty();
  RELATIONAL_TRY(relational_kernel(heap, equality_catalog(heap, global_empty(), budget_unlimited()), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/init.mech"), &source));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &initial));
  FoundationInstance instances[10];
  instances[0] = equality(heap, "OpsData", 1, 1);
  instances[1] = equality(heap, "OpsProof", 0, 0);
  instances[2] = equality(heap, "OpsProofData", 0, 1);
  instances[3] = equality(heap, "OpsToProof", 1, 0);
  instances[4] = equality(heap, "OpsHigher", 1, 2);
  instances[5] = equality(heap, "OpsTypes", 2, 2);
  instances[6] = equality(heap, "OpsTall", 3, 1);
  instances[7] = cast(heap, "OpsCast", 0);
  instances[8] = cast(heap, "OpsCastHigher", 1);
  instances[9] = cast(heap, "OpsCastTall", 2);
  RELATIONAL_TRY(surface_foundation_common_install_families(
      heap, instances, 10, catalog, surface_foundation_data_equality_ops_contracts,
      surface_foundation_data_equality_ops_contracts_count, relational_globals(initial), &installed));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/equality-ops.mech"), &client));
  RELATIONAL_TRY(relational_checked(heap, installed, client, &checked));
  Globals globals = relational_globals(checked);
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  Vec rows = computations(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  RELATIONAL_TRY(surface_foundation_common_negatives(heap, surface_foundation_data_equality_ops_negatives,
                                                     surface_foundation_data_equality_ops_negatives_count, globals));
  *fields = str_lit("instances=10 computations=11 negatives=8");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-EQUALITY-OPS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-EQUALITY-OPS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
