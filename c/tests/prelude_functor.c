/* Port of tests/prelude_functor.bend (60 lines): the functor prelude suite. Reads the category prelude and
 * the functor fixture client under argv[1] (default "."), checks the inventory, the required instances, the
 * computations, the negatives and the budget refusal, then prints PRELUDE-FUNCTOR-OK fields or
 * PRELUDE-FUNCTOR-FAIL message. */
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
static const char *const EXPECTED_FAMILIES[] = {"Higher", "Point", "Small", "Tiny", "Types", "Up"};

/* values() */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  computation(heap, &out, "objectIdentity", relational_tiny(heap, 1));
  computation(heap, &out, "objectForward", relational_tiny(heap, 0));
  computation(heap, &out, "objectReverse", relational_tiny(heap, 1));
  computation(heap, &out, "mapIdentity", relational_tiny(heap, 1));
  computation(heap, &out, "mapForward", relational_tiny(heap, 1));
  computation(heap, &out, "mapReverse", relational_tiny(heap, 0));
  computation(heap, &out, "mapNested", relational_tiny(heap, 2));
  return out;
}

/* negatives() */
static const char *const NEGATIVES[] = {"wrong-object",  "wrong-endpoint", "wrong-identity-law", "wrong-composition-law",
                                        "missing-law",   "erased-object",  "wrong-level-arity"};

/* R.prefixes(["Small", "Types", "Up", "Higher"], [...]) of suite() */
static const char *const INSTANCE_NAMES[] = {"Small", "Types", "Up", "Higher"};
static const char *const INSTANCE_MEMBERS[] = {"eqTrans",      "eqCongr",       "Functor",   "functorObj", "functorMap",
                                               "functorMapId", "functorMapComp", "idFunctor", "compFunctor"};

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
  RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("test/neg/functor/"), name, str_lit(".mech")), &text));
  RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("test/neg/functor/"), name, str_lit(".err")), &expected));
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
  Vec instance_names;
  Vec instance_members;
  Vec required;
  Vec rows;
  Vec negatives;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/cat/category.mech"), &source));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/functor.mech"), &client));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, source, client), &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &template));
  RELATIONAL_TRY(relational_require(relational_empty_symbolic(template), str_lit("symbolic functor escaped")));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), cat(heap, source, client), &checked));
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(relational_counted_inventory(heap, checked, 177, &families));
  globals = relational_globals(checked);
  instance_names = STRINGS(heap, INSTANCE_NAMES);
  instance_members = STRINGS(heap, INSTANCE_MEMBERS);
  required = relational_prefixes(heap, &instance_names, &instance_members);
  RELATIONAL_TRY(relational_required(heap, &required, globals));
  rows = values(heap);
  RELATIONAL_TRY(relational_computations(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  RELATIONAL_TRY(relational_negative_inventory(heap, root, str_lit("test/neg/functor"), &negatives));
  RELATIONAL_TRY(negative_loop(heap, &negatives, root, source, globals));
  RELATIONAL_TRY(relational_budget_refusal(heap, source));
  *fields = str_lit("entries=177 instances=4 computations=7 negatives=8");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-FUNCTOR", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-FUNCTOR"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
