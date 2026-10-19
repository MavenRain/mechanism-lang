/* Port of tests/surface_prelude_poly.bend (91 lines, oracle label surface-prelude-poly): the prelude
 * family catalog keeps its arities, prelude/init.mech checks, five closed instances of MechEq and MechSum
 * install and audit, the polymorphic fixture client checks and audits, and two negatives refuse (unequal
 * endpoints in an instantiated family, distinct instances not convertible). Prints PRELUDE-POLY-OK fields
 * or PRELUDE-POLY-FAIL message. With -DMECH_DRIVER_LIBRARY the file is the Audit library of
 * surface_foundation_common and surface_raw_common (no main). */
#include "driver.h"
#include "prelude.h"
#include "surface_prelude_poly.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* arity(value, expected) */
bool surface_prelude_poly_arity(MaybeNat value, uint64_t expected) {
  NatToU64 small = nat_to_u64(value.value);
  return value.some && small.status == NAT_TO_U64_OK && small.value == expected;
}

/* globals(checked) */
Globals surface_prelude_poly_globals(const ElabChecked *checked) { return relational_globals(checked); }

/* audit(globals) */
Task surface_prelude_poly_audit(Heap *heap, Globals globals) {
  RELATIONAL_TRY(relational_audit_entries(heap, globals.entries, global_empty()));
  return relational_audit_families(heap, globals.families);
}

/* install(instances, catalog, current): Family.instantiate(current, catalog, B.unlimited(), name, levels,
 * alias, [], None{}) per instance, in list order. */
Task surface_prelude_poly_install(Heap *heap, const PolyAuditInstance *instances, size_t count, FamilyCatalog catalog,
                                  Globals current, Globals *out) {
  Globals globals = current;
  for (size_t i = 0; i < count; i++) {
    Vec reuse = vec_new(heap->arena);
    FamilyMaybeVec exports;
    exports.some = false;
    exports.value = vec_new(heap->arena);
    void *value = NULL;
    RELATIONAL_TRY(relational_kernel(heap,
                                     family_instantiate(heap, globals, catalog, budget_unlimited(), instances[i].name,
                                                        &instances[i].levels, instances[i].alias, &reuse, exports),
                                     &value));
    if (value == NULL) return task_fail(str_lit("instantiate returned no globals"));
    globals = *(const Globals *)value;
  }
  *out = globals;
  return task_pure();
}

/* starts(expected, actual): expected is a prefix of actual. */
bool surface_prelude_poly_starts(Str expected, Str actual) { return str_starts_with(actual, expected); }

/* refusal(result, expected, message) */
Task surface_prelude_poly_refusal(Heap *heap, Result result, Str expected, Str message) {
  if (result.kind == RESULT_DONE) return task_fail(message);
  Str text = error_to_string(heap, result.error);
  return relational_require(surface_prelude_poly_starts(expected, text), cat(heap, str_lit("wrong refusal: "), text));
}

#ifndef MECH_DRIVER_LIBRARY

/* endpoint_text() */
static Str endpoint_text(void) {
  return str_lit("mismatch: the constructor mechReflCtor of PolyEqData gives the index "
                 "(In SMu MechNat [] (ACtor mechZero) []) and the type asks for "
                 "(In SMu MechNat [] (ACtor mechSucc) [(In SMu MechNat [] (ACtor mechZero) [])])");
}

/* instance_text() */
static Str instance_text(Heap *heap) {
  Str arguments = str_lit("(Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => (Lan SMu MechUnit [] (Sec SColl 0 []))])");
  return cat(heap,
             cat(heap, cat(heap, cat(heap, str_lit("mismatch: the term has type (Lan SMu PolySumLow [] "), arguments),
                           str_lit(") and the expected type is (Lan SMu MechSum [] ")),
                 arguments),
             str_lit(")"));
}

static Vec levels1(Heap *heap, const LevelVar *a) {
  Vec out = vec_new(heap->arena);
  if (a == NULL || !vec_push(&out, (void *)a)) heap->oom = true;
  return out;
}

static Vec levels2(Heap *heap, const LevelVar *a, const LevelVar *b) {
  Vec out = levels1(heap, a);
  if (b == NULL || !vec_push(&out, (void *)b)) heap->oom = true;
  return out;
}

static PolyAuditInstance instance(Str name, Vec levels, Str alias) {
  PolyAuditInstance row;
  row.name = name;
  row.levels = levels;
  row.alias = alias;
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
  RELATIONAL_TRY(relational_kernel(heap, families_catalog(heap, global_empty(), budget_unlimited()), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  RELATIONAL_TRY(relational_require(surface_prelude_poly_arity(family_arity(catalog, str_lit("MechEq")), 1) &&
                                        surface_prelude_poly_arity(family_arity(catalog, str_lit("MechSum")), 2),
                                    str_lit("prelude catalog arities changed")));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/init.mech"), &source));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &initial));
  PolyAuditInstance instances[5];
  instances[0] = instance(str_lit("MechEq"), levels1(heap, level_one(heap)), str_lit("PolyEqData"));
  instances[1] = instance(str_lit("MechEq"), levels1(heap, level_succ(heap, level_one(heap))), str_lit("PolyEqTypes"));
  instances[2] = instance(str_lit("MechEq"), levels1(heap, level_succ(heap, level_succ(heap, level_one(heap)))),
                          str_lit("PolyEqHigher"));
  instances[3] = instance(str_lit("MechSum"), levels2(heap, level_zero(), level_zero()), str_lit("PolySumLow"));
  instances[4] = instance(str_lit("MechSum"), levels2(heap, level_zero(), level_one(heap)), str_lit("PolySumMixed"));
  RELATIONAL_TRY(surface_prelude_poly_install(heap, instances, 5, catalog, surface_prelude_poly_globals(initial), &installed));
  RELATIONAL_TRY(surface_prelude_poly_audit(heap, installed));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/polymorphic.mech"), &client));
  RELATIONAL_TRY(relational_checked(heap, installed, client, &checked));
  Globals checked_globals = surface_prelude_poly_globals(checked);
  RELATIONAL_TRY(surface_prelude_poly_audit(heap, checked_globals));
  RELATIONAL_TRY(surface_prelude_poly_refusal(
      heap,
      elab_check_in(heap, installed, budget_unlimited(),
                    str_lit("def polyWrong : PolyEqData MechNat mechZero (mechSucc mechZero) := mechReflCtor\n")),
      endpoint_text(), str_lit("unequal endpoints accepted in an instantiated family")));
  RELATIONAL_TRY(surface_prelude_poly_refusal(
      heap,
      elab_check_in(heap, checked_globals, budget_unlimited(),
                    str_lit("def polyWrongInstance : MechSum MechNat MechUnit := polyLowLeft\n")),
      instance_text(heap), str_lit("distinct family instances became convertible")));
  *fields = str_lit("templates=2 instances=5 negatives=2");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-POLY", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-POLY"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }

#endif
