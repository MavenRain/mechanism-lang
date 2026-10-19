/* Port of tests/surface_prenex_families.bend (68 lines, oracle label surface-prenex-families): the
 * prenex families fixture parses, prints and checks with its family and entry inventories in order, audits,
 * normalizes five computations, refuses the families negatives, two exhausted budgets, a specialization with
 * no polls left, a catalog-lifetime specialization and a prenex family declaration outside a program catalog.
 * Prints PRENEX-FAMILIES-OK fields or PRENEX-FAMILIES-FAIL message. */
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_prenex_common.h"
#include "surface_prenex.h"
#include "surface_raw_common.h"

/* A Bend List<String> literal as a Vec of Str pointers. */
static Vec strings(Heap *heap, const char *const *items, size_t len) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < len; i++) {
    Str *boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL || !vec_push(&out, boxed)) {
      heap->oom = true;
      return out;
    }
    *boxed = str_lit(items[i]);
  }
  return out;
}

/* rows() */
static Vec rows(Heap *heap) {
  static const char *const names[] = {"dataBox",   "typeBox",   "proofBox",    "againBox",   "unbox",
                                      "dataValue", "typeValue", "items",       "types",      "head",
                                      "listValue", "up",        "down",        "upType",     "dataRefl",
                                      "proofRefl", "boxIdentity", "boxValue"};
  return strings(heap, names, sizeof names / sizeof names[0]);
}

/* families() */
static Vec families(Heap *heap) {
  static const char *const names[] = {"Tiny",     "Truth",  "DataBox", "TypeBox", "ProofBox",
                                      "AgainBox", "TinyList", "TypeList", "Up",    "Down",
                                      "DataEq",   "ProofEq", "AnnotatedBox"};
  return strings(heap, names, sizeof names / sizeof names[0]);
}

/* boundary(): DPolyMu{1n, Fam{"Empty", [], SType{Bignum.zero()}, []}} outside a program catalog. */
static Task boundary(Heap *heap) {
  SFam family;
  family.name = str_lit("Empty");
  family.params = vec_new(heap->arena);
  family.ty = sterm_type(heap, big_zero());
  family.ctors = vec_new(heap->arena);
  const SDecl *decl = sdecl_poly_mu_new(heap, nat_of_u64(heap->arena, 1), family);
  if (family.ty == NULL || decl == NULL) {
    heap->oom = true;
    return task_fail(str_lit("out of memory"));
  }
  return surface_raw_common_refusal(
      heap, error_make(ERROR_MISMATCH, str_lit("a prenex declaration requires a program catalog")),
      elab_elab_decl(heap, decl, check_make(heap, global_empty(), budget_unlimited(), nat_zero())));
}

/* specialization_poll(state) */
static Task specialization_poll(Heap *heap, const ElabProgram *state) {
  Vec levels = vec_new(heap->arena);
  const LevelVar *one = level_succ(heap, level_zero());
  if (one == NULL || !vec_push(&levels, (void *)one)) {
    heap->oom = true;
    return task_fail(str_lit("out of memory"));
  }
  Vec reuse = vec_new(heap->arena);
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return surface_prenex_common_refusal(
      heap, str_lit("specialize-entry-budget"), str_lit("budget"), str_lit("the check budget is exhausted"),
      budget_run(heap,
                 elab_specialize_kind(heap, true, state->globals, state->definitions, state->families, state->rows,
                                      str_lit("Box"), &levels, str_lit("DataBox"), &reuse, exports),
                 budget_of_polls(nat_zero())));
}

/* specialization_boundary() */
static Task specialization_boundary(Heap *heap) {
  void *declarations = NULL;
  void *state = NULL;
  RELATIONAL_TRY(
      relational_kernel(heap, parser_program(heap, str_lit(surface_prenex_data_prenex_families_box)), &declarations));
  if (declarations == NULL) return task_fail(str_lit("out of memory"));
  ElabProgram empty;
  empty.globals = global_empty();
  empty.definitions = NULL;
  empty.families = NULL;
  empty.rows = vec_new(heap->arena);
  RELATIONAL_TRY(relational_kernel(
      heap, budget_run(heap, elab_program_loop(heap, declarations, empty), budget_unlimited()), &state));
  if (state == NULL) return task_fail(str_lit("out of memory"));
  return specialization_poll(heap, state);
}

static void push_row(Heap *heap, Vec *out, const char *name, const Term *expected) {
  Computation *row = heap_alloc(heap, sizeof *row);
  if (row == NULL || expected == NULL) {
    heap->oom = true;
    return;
  }
  row->name = str_lit(name);
  row->expected = expected;
  if (!vec_push(out, row)) heap->oom = true;
}

static Str field(Heap *heap, Str acc, const char *label, uint64_t value) {
  return str_cat(heap->arena, str_cat(heap->arena, acc, str_lit(label)), str_of_u64(heap->arena, value));
}

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str source = str_empty();
  const Vec *parsed = NULL;
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/prenex-families.mech"), &source));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, source, &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &program));
  Globals globals = relational_globals(program);
  Vec family_names = surface_prenex_common_family_names(heap, globals);
  Vec expected_families = families(heap);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, &expected_families),
                                    str_lit("family inventory or symbolic isolation changed")));
  Vec names = relational_entry_names(heap, relational_rows(program));
  Vec expected_rows = rows(heap);
  RELATIONAL_TRY(relational_require(relational_strings(&names, &expected_rows) &&
                                        surface_foundation_common_entry_count(globals) == expected_rows.len,
                                    str_lit("entry inventory or row order changed")));
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "dataValue", surface_prenex_succ(heap, surface_prenex_zero(heap)));
  push_row(heap, &computations, "typeValue", surface_prenex_tiny(heap));
  push_row(heap, &computations, "listValue", surface_prenex_succ(heap, surface_prenex_zero(heap)));
  push_row(heap, &computations, "upType", surface_prenex_tiny(heap));
  push_row(heap, &computations, "boxValue", surface_prenex_succ(heap, surface_prenex_zero(heap)));
  RELATIONAL_TRY(relational_computations(heap, &computations, globals));
  RELATIONAL_TRY(surface_prenex_common_negatives(heap, surface_prenex_data_prenex_families_negatives,
                                                 surface_prenex_data_prenex_families_negatives_count, global_empty()));
  RELATIONAL_TRY(
      surface_prenex_common_exhausted(heap, str_lit("budget"), str_lit(surface_prenex_data_prenex_families_box), 0));
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("specialize-budget"),
                                                 str_lit(surface_prenex_data_prenex_families_instance), 60));
  RELATIONAL_TRY(specialization_boundary(heap));
  RELATIONAL_TRY(surface_prenex_common_refusal(
      heap, str_lit("lifetime"), str_lit("unbound"), str_lit("unknown universe schema Box"),
      elab_check_in(heap, globals, budget_unlimited(), str_lit("specialize Box (1) as Later"))));
  RELATIONAL_TRY(boundary(heap));
  Str out = field(heap, str_empty(), "families=", surface_prenex_common_family_count(globals));
  out = field(heap, out, " entries=", surface_foundation_common_entry_count(globals));
  *fields = field(heap, out, " computations=5 negatives=",
                  (uint64_t)surface_prenex_data_prenex_families_negatives_count + 4);
  return task_pure();
}

/* arguments(args) = R.run("PRENEX-FAMILIES", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRENEX-FAMILIES"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
