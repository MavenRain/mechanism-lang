/* Port of tests/prelude_left_kan_roundtrip.bend (69 lines): the left Kan cocone and descent round-trip
 * contracts. Reads `_tcc/left-kan-roundtrip-contracts/focused-source.mech` under argv[1] (default "."), checks it
 * from G.initial(), the required round-trip contracts, that no axiom was introduced, the computations and the
 * negatives in test/neg/left-kan-roundtrip, then prints PRELUDE-LEFT-KAN-ROUNDTRIP-OK fields or
 * PRELUDE-LEFT-KAN-ROUNDTRIP-FAIL message. With exactly ROOT --record-oracles it prints the refusal text of each
 * negative (ROUNDTRIP-ORACLES); with any other flag it dies with code 64. Definitions are in the Bend order. */
#include "driver.h"
#include "relational_common.h"
#include "relational_negatives.h"
#include "relational_program.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

static Str at_str(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return str_empty();
  return *(const Str *)got.value;
}

/* A List<String> literal as a Vec of boxed Str. */
static Vec strings(Heap *heap, const char *const *items, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) vec_push(&out, relational_str(heap, str_lit(items[i])));
  return out;
}

#define STRINGS(heap, items) strings(heap, items, sizeof items / sizeof items[0])

/* One C.Pair<String, T.T> row {name, R.natural(value)}. */
static void computation(Heap *heap, Vec *out, const char *name, uint32_t value) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->expected = relational_natural(heap, value);
  vec_push(out, row);
}

/* required() */
static const char *const REQUIRED[] = {"Laws_postCoconeDesc", "Laws_descPostCocone", "WidePostCoconeDescContract",
                                       "WideDescPostCoconeContract"};

/* require_rows(names, rows) */
static Task require_rows(Heap *heap, const Vec *names, const Vec *rows) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    RELATIONAL_TRY(relational_require(relational_contains(rows, name),
                                      cat(heap, str_lit("missing round-trip contract: "), name)));
  }
  return task_pure();
}

/* negatives() */
static const char *const NEGATIVES[] = {"cocone-missing-solution", "cocone-wrong-solution",
                                        "desc-wrong-transformation", "desc-nominal-equality"};

/* values(), appended onto out */
static void values(Heap *heap, Vec *out) {
  computation(heap, out, "lanRoundtripCocone", 3822);
  computation(heap, out, "lanRoundtripCoconeReference", 3822);
  computation(heap, out, "lanRoundtripOtherCocone", 4937);
  computation(heap, out, "lanRoundtripOtherCoconeReference", 4937);
  computation(heap, out, "lanRoundtripDesc", 3822);
  computation(heap, out, "lanRoundtripDescReference", 3822);
  computation(heap, out, "lanRoundtripOtherDesc", 4937);
  computation(heap, out, "lanRoundtripOtherDescReference", 4937);
}

/* checked(root) */
static Task checked_contracts(Heap *heap, Str root, const ElabChecked **checked) {
  Str source = str_empty();
  Vec required;
  Vec axioms;
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(relational_read(heap, root, str_lit("_tcc/left-kan-roundtrip-contracts/focused-source.mech"),
                                 &source));
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, checked));
  required = STRINGS(heap, REQUIRED);
  {
    Vec names = relational_entry_names(heap, relational_rows(*checked));
    RELATIONAL_TRY(require_rows(heap, &required, &names));
  }
  axioms = elab_axiom_names(heap, relational_rows(*checked));
  RELATIONAL_TRY(relational_require(relational_strings(&axioms, &none), str_lit("round-trip laws introduced an axiom")));
  return task_pure();
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  const ElabChecked *checked = NULL;
  Globals globals;
  Vec rows = vec_new(heap->arena);
  Vec negatives;
  RELATIONAL_TRY(checked_contracts(heap, root, &checked));
  globals = relational_globals(checked);
  values(heap, &rows);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/left-kan-roundtrip"), &negatives, globals,
                               negatives_prefix(64, true, false)));
  *fields = str_lit("laws=2 contracts=2 computations=8 negatives=4");
  return task_pure();
}

/* oracles(root, names, globals): name ++ "\t" ++ message ++ "\n" per name, in order. */
static Task oracles(Heap *heap, Str root, const Vec *names, Globals globals, Str *text) {
  Str out = str_empty();
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    Str source = str_empty();
    Str message = str_empty();
    RELATIONAL_TRY(relational_read(heap, root, cat(heap, cat(heap, str_lit("test/neg/left-kan-roundtrip/"), name),
                                                   str_lit(".mech")), &source));
    /* D-360a (host only, the D-355a form): Bend checks each source with check_in under Budget.unlimited.
     * The compact entry is the same check under budget_unlimited (decision 156, D-254a), but its work
     * heaps run each conversion query in a scratch arena (D-261a), so a refusal no longer keeps the
     * garbage of the whole elaboration in the caller arena. The refusal value and its text do not change. */
    RELATIONAL_TRY(negatives_message(heap, elab_check_in_compact(heap, globals, source), name, &message));
    out = cat(heap, out, cat(heap, cat(heap, cat(heap, name, str_lit("\t")), message), str_lit("\n")));
  }
  *text = out;
  return task_pure();
}

/* record(root) */
static Task record(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  const ElabChecked *checked = NULL;
  Vec negatives;
  RELATIONAL_TRY(checked_contracts(heap, root, &checked));
  negatives = STRINGS(heap, NEGATIVES);
  return oracles(heap, root, &negatives, relational_globals(checked), fields);
}

/* selected(is_record, root) */
static int selected(Heap *heap, bool is_record, Str *root) {
  if (is_record) return relational_run(heap, str_lit("ROUNDTRIP-ORACLES"), record, root);
  return driver_die(64, "expected ROOT [--record-oracles]");
}

/* arguments(args): exactly ROOT FLAG selects the record mode; anything else runs suite(R.root(args)). */
static int arguments(Heap *heap, int argc, char **argv) {
  Str root = relational_root(argc, argv);
  if (argc == 3) return selected(heap, str_eq(str_lit(argv[2]), str_lit("--record-oracles")), &root);
  return relational_run(heap, str_lit("PRELUDE-LEFT-KAN-ROUNDTRIP"), suite, &root);
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
