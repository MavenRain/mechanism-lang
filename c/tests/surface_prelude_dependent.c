/* Port of bend2/tests/surface_prelude_dependent.bend (83 lines, oracle label surface-prelude-dependent): the
 * dependent catalog holds six templates with their arities, an exhausted budget refuses the catalog, 29
 * instances install over prelude/init.mech against their frozen contracts, the fixture client checks and
 * audits, twelve computations normalize and six negatives refuse. Prints PRELUDE-DEPENDENT-OK fields or
 * PRELUDE-DEPENDENT-FAIL message. */
#include "driver.h"
#include "prelude.h"
#include "surface_foundation_common.h"
#include "surface_foundation_data.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* One C.Pair<String, Nat> row of arities(). */
typedef struct {
  const char *name;
  uint64_t arity;
} ArityRow;

/* arities(rows, catalog) */
static Task arities(Heap *heap, const ArityRow *rows, size_t len, PolyCatalog catalog) {
  for (size_t i = 0; i < len; i++) {
    MaybeNat expected;
    expected.some = true;
    expected.value = nat_of_u64(heap->arena, rows[i].arity);
    RELATIONAL_TRY(relational_require(relational_optional_nat(poly_arity(catalog, str_lit(rows[i].name)), expected),
                                      cat(heap, str_lit("wrong template arity: "), str_lit(rows[i].name))));
  }
  return task_pure();
}

static void push_level(Heap *heap, Vec *levels, uint64_t index) {
  const LevelVar *level = surface_foundation_common_level(heap, index);
  if (level == NULL || !vec_push(levels, (void *)level)) heap->oom = true;
}

static FoundationInstance instance(Heap *heap, const char *name, Vec levels, Str alias, Str contract) {
  FoundationInstance row;
  row.name = str_lit(name);
  row.levels = levels;
  row.alias = alias;
  row.contract = contract;
  return row;
}

static Vec levels2(Heap *heap, uint64_t u, uint64_t v) {
  Vec out = vec_new(heap->arena);
  push_level(heap, &out, u);
  push_level(heap, &out, v);
  return out;
}

/* group(prefix, u, v, w): six instances written to out[0..5]. */
static void group(Heap *heap, const char *prefix, uint64_t u, uint64_t v, uint64_t w, FoundationInstance *out) {
  Str p = str_lit(prefix);
  Vec rec = levels2(heap, u, v);
  push_level(heap, &rec, w);
  out[0] = instance(heap, "MechPi", levels2(heap, 1 + u, 1 + v), cat(heap, p, str_lit("Pi")), str_empty());
  out[1] = instance(heap, "MechSigma", levels2(heap, u, v), cat(heap, p, str_lit("Pair")), str_empty());
  out[2] = instance(heap, "mechSigmaMk", levels2(heap, u, v), cat(heap, p, str_lit("Mk")), str_empty());
  out[3] = instance(heap, "mechSigmaFst", levels2(heap, u, v), cat(heap, p, str_lit("Fst")), str_empty());
  out[4] = instance(heap, "mechSigmaSnd", levels2(heap, u, v), cat(heap, p, str_lit("Snd")), str_empty());
  out[5] = instance(heap, "mechSigmaRec", rec, cat(heap, p, str_lit("Rec")), p);
}

/* pi(name, u, v) = F.Instance{"MechPi", [F.level(u), F.level(v)], name, name} */
static FoundationInstance pi(Heap *heap, const char *name, uint64_t u, uint64_t v) {
  return instance(heap, "MechPi", levels2(heap, u, v), str_lit(name), str_lit(name));
}

/* instances(): 29 rows written to out. */
static void instances(Heap *heap, FoundationInstance *out) {
  group(heap, "Small", 0, 0, 1, out);
  group(heap, "Up", 0, 1, 2, out + 6);
  group(heap, "Down", 1, 0, 0, out + 12);
  group(heap, "Tall", 1, 2, 3, out + 18);
  out[24] = pi(heap, "ProofPi", 0, 0);
  out[25] = pi(heap, "ProofDataPi", 0, 1);
  out[26] = pi(heap, "DataProofPi", 1, 0);
  out[27] = pi(heap, "HigherProofPi", 2, 0);
  out[28] = pi(heap, "HigherPi", 2, 1);
}

/* check_count(state) */
static Task check_count(uint64_t count) {
  return relational_require(count == 29, str_lit("wrong instance count"));
}

static void push_row(Heap *heap, Vec *rows, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL || expected == NULL) {
    heap->oom = true;
    return;
  }
  row->name = str_lit(name);
  row->expected = expected;
  if (!vec_push(rows, row)) heap->oom = true;
}

/* computations() */
static Vec computations(Heap *heap) {
  Vec no_items = vec_new(heap->arena);
  const Term *nat = term_lan(heap, shape_smu(str_lit("MechNat"), vec_new(heap->arena)), rules_diagram_of(heap, &no_items));
  const Term *trivial =
      term_in(heap, shape_smu(str_lit("MechTrue"), vec_new(heap->arena)), term_addr_ctor(str_lit("mechTrivial")),
              vec_new(heap->arena));
  Vec succ_args = vec_new(heap->arena);
  const Term *one = surface_foundation_common_one(heap);
  if (one == NULL || !vec_push(&succ_args, (void *)one)) heap->oom = true;
  const Term *pair_rec =
      term_in(heap, shape_smu(str_lit("MechNat"), vec_new(heap->arena)), term_addr_ctor(str_lit("mechSucc")), succ_args);
  Vec rows = vec_new(heap->arena);
  push_row(heap, &rows, "pairFirst", surface_foundation_common_zero(heap));
  push_row(heap, &rows, "pairSecond", surface_foundation_common_one(heap));
  push_row(heap, &rows, "pairRec", pair_rec);
  push_row(heap, &rows, "depSecond", surface_foundation_common_one(heap));
  push_row(heap, &rows, "depRec", surface_foundation_common_one(heap));
  push_row(heap, &rows, "downFirst", nat);
  push_row(heap, &rows, "downSecond", surface_foundation_common_one(heap));
  push_row(heap, &rows, "upSecond", nat);
  push_row(heap, &rows, "upRec", nat);
  push_row(heap, &rows, "piValue", surface_foundation_common_one(heap));
  push_row(heap, &rows, "downRec", trivial);
  push_row(heap, &rows, "proofPiValue", trivial);
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
  uint64_t count = 0;
  RELATIONAL_TRY(relational_kernel(heap, dependent_catalog(heap, global_empty(), budget_unlimited()), &value));
  PolyCatalog catalog = (PolyCatalog)value;
  RELATIONAL_TRY(surface_foundation_common_budget_refusal(
      heap, budget_run_poll(heap, dependent_catalog_comp(heap, global_empty()), surface_foundation_common_exhausted(heap))));
  static const ArityRow arity_rows[] = {{"MechPi", 2},       {"MechSigma", 2},    {"mechSigmaMk", 2},
                                        {"mechSigmaFst", 2}, {"mechSigmaSnd", 2}, {"mechSigmaRec", 3}};
  RELATIONAL_TRY(arities(heap, arity_rows, 6, catalog));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/init.mech"), &source));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &initial));
  FoundationInstance rows[29];
  instances(heap, rows);
  RELATIONAL_TRY(surface_foundation_common_install_terms(
      heap, rows, 29, catalog, surface_foundation_data_dependent_contracts, surface_foundation_data_dependent_contracts_count,
      relational_globals(initial), 0, &installed, &count));
  RELATIONAL_TRY(check_count(count));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/dependent.mech"), &client));
  RELATIONAL_TRY(relational_checked(heap, installed, client, &checked));
  Globals globals = relational_globals(checked);
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  Vec expected = computations(heap);
  RELATIONAL_TRY(relational_computations(heap, &expected, globals));
  RELATIONAL_TRY(surface_foundation_common_negatives(heap, surface_foundation_data_dependent_negatives,
                                                     surface_foundation_data_dependent_negatives_count, globals));
  *fields = str_lit("templates=6 instances=29 computations=12 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-DEPENDENT", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-DEPENDENT"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
