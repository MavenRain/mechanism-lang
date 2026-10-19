/* Port of tests/surface_prelude_transport.bend (85 lines, oracle label surface-prelude-transport): the
 * polymorphic equality catalog keeps MechEq (arity 2) and MechTypeEq (arity 1) with their members, seven
 * instances install over prelude/init.mech, the fixture client checks, a MechTypeEq instance reuses CastData
 * with exported member names, the library client checks and audits, and five negatives refuse. Prints
 * PRELUDE-TRANSPORT-OK fields or PRELUDE-TRANSPORT-FAIL message. */
#include "driver.h"
#include "prelude.h"
#include "surface_foundation_common.h"
#include "surface_prelude_congruence.h"
#include "surface_prelude_poly.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

static MaybeNat some_nat(Heap *heap, uint64_t value) {
  MaybeNat out;
  out.some = true;
  out.value = nat_of_u64(heap->arena, value);
  return out;
}

static Vec strings(Heap *heap, const char *const *names, size_t len) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < len; i++) {
    Str *boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) {
      heap->oom = true;
      return out;
    }
    *boxed = str_lit(names[i]);
    if (!vec_push(&out, boxed)) heap->oom = true;
  }
  return out;
}

static void push_rename(Heap *heap, Vec *out, const char *source, const char *target) {
  PolyRename *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) {
    heap->oom = true;
    return;
  }
  row->source = str_lit(source);
  row->target = str_lit(target);
  if (!vec_push(out, row)) heap->oom = true;
}

static void push_level(Heap *heap, Vec *levels, uint64_t index) {
  const LevelVar *level = surface_foundation_common_level(heap, index);
  if (level == NULL || !vec_push(levels, (void *)level)) heap->oom = true;
}

/* members(value, expected): Some{names} -> R.strings(names, expected); None -> False. */
static bool members(Heap *heap, FamilyMaybeVec value, const char *const *expected, size_t len) {
  if (!value.some) return false;
  Vec names = strings(heap, expected, len);
  return relational_strings(&value.value, &names);
}

/* inventory(catalog) */
static bool inventory(Heap *heap, FamilyCatalog catalog) {
  static const char *const eq_members[] = {"refl", "transport", "j", "symm", "trans", "congr"};
  static const char *const type_eq_members[] = {"refl", "cast", "symm", "trans"};
  return relational_optional_nat(family_arity(catalog, str_lit("MechEq")), some_nat(heap, 2)) &&
         members(heap, family_members(heap, catalog, str_lit("MechEq")), eq_members, 6) &&
         relational_optional_nat(family_arity(catalog, str_lit("MechTypeEq")), some_nat(heap, 1)) &&
         members(heap, family_members(heap, catalog, str_lit("MechTypeEq")), type_eq_members, 4);
}

/* library_client() */
static Str library_client(void) {
  return str_lit("def libraryValue : MechNat := libraryCast MechNat MechNat (CastData_refl MechNat) (mechSucc mechZero)\n"
                 "def librarySelf : MechNat := libraryCast MechNat MechNat (libraryRefl MechNat) mechZero\n"
                 "mu LibraryIsOne : (0 n : MechNat) -> Type 0 with\n"
                 "| libraryIsOne : LibraryIsOne (mechSucc mechZero)\n"
                 "def libraryComputes : LibraryIsOne libraryValue := libraryIsOne\n");
}

/* refusal(globals, source, expected) */
static Task refusal(Heap *heap, Globals globals, const char *source, const char *expected) {
  return surface_prelude_poly_refusal(heap, elab_check_in(heap, globals, budget_unlimited(), str_lit(source)),
                                      str_lit(expected), cat(heap, str_lit("expected refusal: "), str_lit(expected)));
}

/* negatives(installed, initial) */
static Task negatives(Heap *heap, Globals installed, Globals initial) {
  RELATIONAL_TRY(refusal(heap, installed,
                         "def wrongEndpoint : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) "
                         "mechZero (mechSucc mechZero) (TransportData_refl MechNat mechZero) mechZero\n",
                         "mismatch: the term has type (Lan SMu TransportData"));
  RELATIONAL_TRY(refusal(heap, installed,
                         "def wrongTypes : MechUnit := CastData_cast MechNat MechUnit (CastData_refl MechNat) mechZero\n",
                         "mismatch: the term has type (Lan SMu CastData"));
  RELATIONAL_TRY(refusal(heap, installed,
                         "def wrongUniverse : Type 0 := CastData_cast (Type 0) (Type 0) (CastData_refl (Type 0)) MechNat\n",
                         "mismatch: the term has type Type 2 and the expected type is Type 1"));
  RELATIONAL_TRY(refusal(heap, installed,
                         "def mixedInstance : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) "
                         "mechZero mechZero (TransportHigher_refl MechNat mechZero) mechZero\n",
                         "mismatch: the term has type (Lan SMu TransportHigher"));
  return refusal(heap, initial,
                 "def unavailable : MechNat := CastData_cast MechNat MechNat (CastData_refl MechNat) mechZero\n",
                 "unbound: CastData_cast");
}

/* F.Instance{name, levels, alias, ""} with one or two levels (second < 0: absent). */
static FoundationInstance instance(Heap *heap, const char *name, uint64_t u, int64_t v, const char *alias) {
  FoundationInstance row;
  row.name = str_lit(name);
  row.levels = vec_new(heap->arena);
  push_level(heap, &row.levels, u);
  if (v >= 0) push_level(heap, &row.levels, (uint64_t)v);
  row.alias = str_lit(alias);
  row.contract = str_empty();
  return row;
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  void *value = NULL;
  Str source = str_empty();
  Str client = str_empty();
  const ElabChecked *initial_checked = NULL;
  const ElabChecked *checked = NULL;
  Globals installed = global_empty();
  RELATIONAL_TRY(relational_kernel(heap, equality_catalog(heap, global_empty(), budget_unlimited()), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  RELATIONAL_TRY(relational_require(inventory(heap, catalog), str_lit("polymorphic equality inventory differs")));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("prelude/init.mech"), &source));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &initial_checked));
  Globals initial = relational_globals(initial_checked);
  FoundationInstance instances[7];
  instances[0] = instance(heap, "MechEq", 1, 1, "TransportData");
  instances[1] = instance(heap, "MechEq", 1, 2, "TransportHigher");
  instances[2] = instance(heap, "MechEq", 0, 0, "TransportProof");
  instances[3] = instance(heap, "MechEq", 0, 1, "TransportProofData");
  instances[4] = instance(heap, "MechEq", 1, 0, "TransportToProof");
  instances[5] = instance(heap, "MechTypeEq", 0, -1, "CastData");
  instances[6] = instance(heap, "MechTypeEq", 1, -1, "CastTypes");
  RELATIONAL_TRY(surface_foundation_common_install_families(heap, instances, 7, catalog, NULL, 0, initial, &installed));
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/transport.mech"), &client));
  RELATIONAL_TRY(relational_checked(heap, installed, client, &checked));
  Vec levels = vec_new(heap->arena);
  push_level(heap, &levels, 0);
  Vec reuse = vec_new(heap->arena);
  push_rename(heap, &reuse, "MechTypeEq", "CastData");
  FamilyMaybeVec exports;
  exports.some = true;
  exports.value = vec_new(heap->arena);
  push_rename(heap, &exports.value, "refl", "libraryRefl");
  push_rename(heap, &exports.value, "cast", "libraryCast");
  push_rename(heap, &exports.value, "symm", "librarySymm");
  push_rename(heap, &exports.value, "trans", "libraryTrans");
  value = NULL;
  RELATIONAL_TRY(relational_kernel(heap,
                                   family_instantiate(heap, relational_globals(checked), catalog, budget_unlimited(),
                                                      str_lit("MechTypeEq"), &levels, str_lit("NamedCast"), &reuse,
                                                      exports),
                                   &value));
  if (value == NULL) return task_fail(str_lit("instantiate returned no globals"));
  Globals named = *(const Globals *)value;
  RELATIONAL_TRY(relational_require(
      surface_prelude_congruence_absent(global_find_family(str_lit("NamedCast"), named).some) &&
          surface_prelude_congruence_absent(global_find(str_lit("NamedCast_cast"), named).some),
      str_lit("reuse installed a fresh family or a default member name")));
  RELATIONAL_TRY(relational_checked(heap, named, library_client(), &checked));
  RELATIONAL_TRY(surface_foundation_common_audit(heap, relational_globals(checked)));
  RELATIONAL_TRY(negatives(heap, installed, initial));
  RELATIONAL_TRY(relational_require(
      surface_prelude_congruence_absent(global_find(str_lit("cast"), initial).some) &&
          surface_prelude_congruence_absent(global_find_family(str_lit("MechTypeEq"), initial).some),
      str_lit("template checking changed ordinary globals")));
  *fields = str_lit("templates=2 instances=7 negatives=5");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-TRANSPORT", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-TRANSPORT"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
