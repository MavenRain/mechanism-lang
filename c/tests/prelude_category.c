/* Port of tests/prelude_category.bend (60 lines): the category prelude suite. Reads the prelude and the
 * fixture client under argv[1] (default "."), checks the inventory, the computations, the quotations, the
 * negatives and the budget refusal, then prints PRELUDE-CATEGORY-OK fields or PRELUDE-CATEGORY-FAIL message. */
#include "driver.h"
#include "relational_common.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

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

/* One C.Pair<String, T.T> row of values(). */
static void computation(Heap *heap, Vec *out, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->expected = expected;
  vec_push(out, row);
}

/* expected_families() */
static const char *const EXPECTED_FAMILIES[] = {"Higher", "IndexedValue", "Point", "Small", "Tiny", "Types", "Up"};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "identityValue", relational_tiny(heap, 1));
  computation(heap, &out, "compositionValue", relational_tiny(heap, 0));
  computation(heap, &out, "reverseValue", relational_tiny(heap, 1));
  computation(heap, &out, "heterogeneousValue", relational_tiny(heap, 1));
  computation(heap, &out, "upValue", relational_tiny_type(heap));
  computation(heap, &out, "higherValue", relational_tiny_type(heap));
  computation(heap, &out, "capturedValue", relational_tiny(heap, 1));
  return out;
}

/* negatives() */
static const char *const NEGATIVES[] = {"unequal-endpoint",   "wrong-composition", "wrong-law",
                                        "wrong-dependent-index", "wrong-index-carrier", "erased-endpoint",
                                        "nominal-equality",   "wrong-level-arity"};

static const char *const QUOTATIONS[] = {"capturedClosed", "indexedClosed", "indexedMotiveClosed"};

/* negative_selected(template, source, globals, text, expected, name) */
static Task negative_selected(Heap *heap, bool template, Str source, Globals globals, Str text, Str expected, Str name) {
  if (template)
    return relational_refuse_exact(heap, elab_check_in(heap, global_empty(), budget_unlimited(), cat(heap, source, text)),
                                   expected, name);
  return relational_refuse_exact(heap, elab_check_in(heap, globals, budget_unlimited(), text), expected, name);
}

/* negative(root, source, globals, name) */
static Task negative(Heap *heap, Str root, Str source, Globals globals, Str name) {
  Str text = str_empty();
  Str expected = str_empty();
  RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("test/neg/category/"), name, str_lit(".mech")), &text));
  RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("test/neg/category/"), name, str_lit(".err")), &expected));
  return negative_selected(heap, str_eq(name, str_lit("wrong-level-arity")), source, globals, text, expected, name);
}

/* negative_loop(names, root, source, globals) */
static Task negative_loop(Heap *heap, const Vec *names, Str root, Str source, Globals globals) {
  for (size_t i = 0; i < names->len; i++) RELATIONAL_TRY(negative(heap, root, source, globals, at_str(names, i)));
  return task_pure();
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str source = str_empty();
  Str client = str_empty();
  const Vec *parsed = NULL;
  const ElabChecked *template = NULL;
  const ElabChecked *checked = NULL;
  Globals globals;
  Vec families;
  Vec rows;
  Vec quoted;
  Vec negatives;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/category.mech"), &source));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/category.mech"), &client));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, source, &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &template));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(template), str_lit("symbolic category escaped")));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), cat(heap, source, client), &checked));
  families = strings(heap, EXPECTED_FAMILIES, sizeof EXPECTED_FAMILIES / sizeof EXPECTED_FAMILIES[0]);
  RELATIONAL_TRY(relational_counted_inventory(heap, checked, 181, &families));
  globals = relational_globals(checked);
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  quoted = strings(heap, QUOTATIONS, sizeof QUOTATIONS / sizeof QUOTATIONS[0]);
  RELATIONAL_TRY(relational_quotations(heap, &quoted, globals));
  negatives = strings(heap, NEGATIVES, sizeof NEGATIVES / sizeof NEGATIVES[0]);
  RELATIONAL_TRY(relational_negative_inventory(heap, root, str_lit("test/neg/category"), &negatives));
  RELATIONAL_TRY(negative_loop(heap, &negatives, root, source, globals));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=181 computations=7 negatives=9 quotation=3");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-CATEGORY", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-CATEGORY"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
