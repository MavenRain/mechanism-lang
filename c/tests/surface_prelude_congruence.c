/* Port of bend2/tests/surface_prelude_congruence.bend (53 lines, oracle label surface-prelude-congruence): the
 * congruence catalog keeps MechCongr (arity 2, member congr) and no Result family, an exhausted budget refuses
 * the catalog, prelude/init.mech checks, eight MechCongr instances install against their frozen contracts
 * without leaking the symbolic names, the fixture client checks and audits, four computations normalize and
 * five negatives refuse. Prints PRELUDE-CONGRUENCE-OK fields or PRELUDE-CONGRUENCE-FAIL message. With
 * -DMECH_DRIVER_LIBRARY the file is the Congruence library of surface_prelude_transport (no main). */
#include "driver.h"
#include "prelude.h"
#include "surface_foundation_common.h"
#include "surface_foundation_data.h"
#include "surface_prelude_congruence.h"

/* absent(value) */
bool surface_prelude_congruence_absent(bool some) {
  if (some) return false;
  return true;
}

#ifndef MECH_DRIVER_LIBRARY

static MaybeNat some_nat(Heap *heap, uint64_t value) {
  MaybeNat out;
  out.some = true;
  out.value = nat_of_u64(heap->arena, value);
  return out;
}

static Vec strings1(Heap *heap, const char *a) {
  Vec out = vec_new(heap->arena);
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) {
    heap->oom = true;
    return out;
  }
  *boxed = str_lit(a);
  if (!vec_push(&out, boxed)) heap->oom = true;
  return out;
}

/* members(value): Some{names} -> R.strings(names, ["congr"]); None -> False. */
static bool members(Heap *heap, FamilyMaybeVec value) {
  if (!value.some) return false;
  Vec expected = strings1(heap, "congr");
  return relational_strings(&value.value, &expected);
}

static Vec levels2(Heap *heap, uint64_t u, uint64_t v) {
  Vec out = vec_new(heap->arena);
  const LevelVar *a = surface_foundation_common_level(heap, u);
  const LevelVar *b = surface_foundation_common_level(heap, v);
  if (a == NULL || !vec_push(&out, (void *)a)) heap->oom = true;
  if (b == NULL || !vec_push(&out, (void *)b)) heap->oom = true;
  return out;
}

/* instance(alias, u, v) = F.Instance{"MechCongr", [F.level(u), F.level(v)], alias, alias} */
static FoundationInstance instance(Heap *heap, const char *alias, uint64_t u, uint64_t v) {
  FoundationInstance row;
  row.name = str_lit("MechCongr");
  row.levels = levels2(heap, u, v);
  row.alias = str_lit(alias);
  row.contract = str_lit(alias);
  return row;
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
  RELATIONAL_TRY(relational_kernel(heap, congruence_catalog(heap, global_empty(), budget_unlimited()), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  RELATIONAL_TRY(relational_require(
      relational_optional_nat(family_arity(catalog, str_lit("MechCongr")), some_nat(heap, 2)) &&
          members(heap, family_members(heap, catalog, str_lit("MechCongr"))) &&
          surface_prelude_congruence_absent(family_arity(catalog, str_lit("Result")).some),
      str_lit("catalog inventory differs")));
  RELATIONAL_TRY(surface_foundation_common_budget_refusal(
      heap, budget_run_poll(heap, congruence_catalog_comp(heap, global_empty()), surface_foundation_common_exhausted(heap))));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/init.mech"), &source));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &initial));
  FoundationInstance instances[8];
  instances[0] = instance(heap, "Same", 1, 1);
  instances[1] = instance(heap, "Again", 1, 1);
  instances[2] = instance(heap, "Up", 1, 2);
  instances[3] = instance(heap, "Down", 2, 1);
  instances[4] = instance(heap, "Proof", 0, 0);
  instances[5] = instance(heap, "ProofData", 0, 1);
  instances[6] = instance(heap, "DataProof", 1, 0);
  instances[7] = instance(heap, "Tall", 3, 0);
  RELATIONAL_TRY(surface_foundation_common_install_families(
      heap, instances, 8, catalog, surface_foundation_data_congruence_contracts,
      surface_foundation_data_congruence_contracts_count, relational_globals(initial), &installed));
  RELATIONAL_TRY(relational_require(
      surface_prelude_congruence_absent(global_find(str_lit("congr"), installed).some) &&
          surface_prelude_congruence_absent(global_find_family(str_lit("MechCongr"), installed).some) &&
          surface_prelude_congruence_absent(global_find_family(str_lit("Result"), installed).some),
      str_lit("symbolic families escaped")));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/congruence.mech"), &client));
  RELATIONAL_TRY(relational_checked(heap, installed, client, &checked));
  Globals globals = relational_globals(checked);
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  static const char *const computation_names[] = {"sameValue", "upValue", "downValue", "proofDataValue"};
  Vec rows = surface_foundation_common_ones(heap, computation_names, 4);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  RELATIONAL_TRY(surface_foundation_common_negatives(heap, surface_foundation_data_congruence_negatives,
                                                     surface_foundation_data_congruence_negatives_count, globals));
  *fields = str_lit("instances=8 computations=4 negatives=5");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-CONGRUENCE", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-CONGRUENCE"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }

#endif
