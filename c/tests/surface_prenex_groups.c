/* Port of bend2/tests/surface_prenex_groups.bend (74 lines, oracle label surface-prenex-groups): the prenex
 * groups fixture parses, prints and checks with its member and family inventories in order and no axiom,
 * audits, normalizes seven computations, checks the group instance and the positives, refuses the negatives
 * and parser cases, keeps the group budget below two template checks, refuses a catalog-lifetime
 * specialization and a prenex group declaration outside a program catalog. Prints PRENEX-GROUPS-OK fields or
 * PRENEX-GROUPS-FAIL message. */
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_prenex_common.h"
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

/* rows() = R.prefixes(["Data", "Types", "Proofs", "Again"], ["unbox", "pack", "unpack", "level"]) ++ values */
static Vec rows(Heap *heap) {
  static const char *const prefixes[] = {"Data", "Types", "Proofs", "Again"};
  static const char *const members[] = {"unbox", "pack", "unpack", "level"};
  static const char *const values[] = {"dataValue", "typeValue", "proofValue", "againValue",
                                       "One_make",  "oneValue",  "plainValue"};
  Vec names = strings(heap, prefixes, 4);
  Vec suffixes = strings(heap, members, 4);
  Vec prefixed = relational_prefixes(heap, &names, &suffixes);
  Vec rest = strings(heap, values, sizeof values / sizeof values[0]);
  return heap_vec_concat(heap, &prefixed, &rest);
}

/* families() */
static Vec families(Heap *heap) {
  static const char *const names[] = {"Tiny",  "Truth",          "Data",   "Data_Wrapped",   "Types",
                                      "Types_Wrapped", "Proofs", "Proofs_Wrapped", "Again", "Again_Wrapped",
                                      "One",   "Plain",          "Plain_Second"};
  return strings(heap, names, sizeof names / sizeof names[0]);
}

/* In{SMu{family, []}, ACtor{ctor}, args} with args NULL for []. */
static const Term *in_ctor(Heap *heap, const char *family, const char *ctor, const Term *arg) {
  Vec args = vec_new(heap->arena);
  if (arg != NULL && !vec_push(&args, (void *)arg)) {
    heap->oom = true;
    return NULL;
  }
  return term_in(heap, shape_smu(str_lit(family), vec_new(heap->arena)), term_addr_ctor(str_lit(ctor)), args);
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

/* computations() */
static Vec computations(Heap *heap) {
  Vec out = vec_new(heap->arena);
  const Term *zero = in_ctor(heap, "Tiny", "zero", NULL);
  if (zero == NULL) {
    heap->oom = true;
    return out;
  }
  Vec none = vec_new(heap->arena);
  push_row(heap, &out, "dataValue", in_ctor(heap, "Tiny", "next", zero));
  push_row(heap, &out, "typeValue",
           term_lan(heap, shape_smu(str_lit("Tiny"), vec_new(heap->arena)), rules_diagram_of(heap, &none)));
  push_row(heap, &out, "againValue", zero);
  push_row(heap, &out, "Data_level", term_univ(heap, level_succ(heap, level_succ(heap, level_zero()))));
  push_row(heap, &out, "Types_level", term_univ(heap, level_succ(heap, level_zero())));
  push_row(heap, &out, "oneValue", in_ctor(heap, "One", "single", zero));
  const Term *first = in_ctor(heap, "Plain", "first", NULL);
  push_row(heap, &out, "plainValue", first == NULL ? NULL : in_ctor(heap, "Plain_Second", "second", first));
  return out;
}

/* boundary(): DPolyGroup{1n, Fam{"Empty", [], SType{Bignum.zero()}, []}, [], []} outside a program catalog. */
static Task boundary(Heap *heap) {
  SFam family;
  family.name = str_lit("Empty");
  family.params = vec_new(heap->arena);
  family.ty = sterm_type(heap, big_zero());
  family.ctors = vec_new(heap->arena);
  const SDecl *decl =
      sdecl_poly_group_new(heap, nat_of_u64(heap->arena, 1), family, vec_new(heap->arena), vec_new(heap->arena));
  if (family.ty == NULL || decl == NULL) {
    heap->oom = true;
    return task_fail(str_lit("out of memory"));
  }
  return surface_raw_common_refusal(
      heap, error_make(ERROR_MISMATCH, str_lit("a prenex declaration requires a program catalog")),
      elab_elab_decl(heap, decl, check_make(heap, global_empty(), budget_unlimited(), nat_zero())));
}

/* budgets() */
static Task budgets(Heap *heap) {
  Str group = str_lit(surface_prenex_data_prenex_groups_group);
  Str first = str_lit(surface_prenex_data_prenex_groups_first);
  uint64_t ceiling = 0;
  uint64_t templates = 0;
  uint64_t grouped = 0;
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("declaration-budget"), group, 0));
  RELATIONAL_TRY(surface_prenex_common_polls(heap, group, &ceiling));
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("instance-budget"),
                                                 str_lit(surface_prenex_data_prenex_groups_instance), ceiling));
  RELATIONAL_TRY(surface_prenex_common_polls(
      heap, str_cat(heap->arena, first, str_lit("poly (u) mu Second : Sort (succ u) with | second : Second ")),
      &templates));
  RELATIONAL_TRY(surface_prenex_common_polls(
      heap, str_cat(heap->arena, first, str_lit("and Second : Sort (succ u) with | second : Second ")), &grouped));
  Str message = str_cat(heap->arena, str_lit("group elaboration repeats the universal check: "),
                        str_of_u64(heap->arena, grouped));
  message = str_cat(heap->arena, str_cat(heap->arena, message, str_lit(" polls against ")),
                    str_of_u64(heap->arena, templates));
  message = str_cat(heap->arena, message, str_lit(" for the same two families as templates"));
  return relational_require(grouped < 1 + 2 * templates, message);
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
  const ElabChecked *instance = NULL;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/prenex-groups.mech"), &source));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, source, &parsed));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &program));
  Globals globals = relational_globals(program);
  Vec names = relational_entry_names(heap, relational_rows(program));
  Vec expected_rows = rows(heap);
  RELATIONAL_TRY(relational_require(relational_strings(&names, &expected_rows) &&
                                        surface_foundation_common_entry_count(globals) == expected_rows.len,
                                    str_lit("member inventory or row order changed")));
  Vec family_names = surface_prenex_common_family_names(heap, globals);
  Vec expected_families = families(heap);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, &expected_families),
                                    str_lit("family inventory or symbolic isolation changed")));
  Vec disclosures = elab_disclosure_names(heap, relational_rows(program));
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(relational_require(relational_strings(&disclosures, &none), str_lit("unexpected axiom disclosure")));
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  Vec expected = computations(heap);
  RELATIONAL_TRY(relational_computations(heap, &expected, globals));
  RELATIONAL_TRY(
      relational_checked(heap, global_empty(), str_lit(surface_prenex_data_prenex_groups_instance), &instance));
  RELATIONAL_TRY(surface_prenex_common_positives(heap, surface_prenex_data_prenex_groups_positives,
                                                 surface_prenex_data_prenex_groups_positives_count));
  RELATIONAL_TRY(surface_prenex_common_negatives(heap, surface_prenex_data_prenex_groups_negatives,
                                                 surface_prenex_data_prenex_groups_negatives_count, global_empty()));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_prenex_data_prenex_groups_parsers,
                                               surface_prenex_data_prenex_groups_parsers_count, false));
  RELATIONAL_TRY(budgets(heap));
  RELATIONAL_TRY(surface_prenex_common_refusal(
      heap, str_lit("catalog-lifetime"), str_lit("unbound"), str_lit("unknown universe schema Box"),
      elab_check_in(heap, globals, budget_unlimited(), str_lit("specialize Box (1, 2) as Later"))));
  RELATIONAL_TRY(boundary(heap));
  Str out = field(heap, str_empty(), "families=", surface_prenex_common_family_count(globals));
  out = field(heap, out, " entries=", surface_foundation_common_entry_count(globals));
  *fields = field(heap, out, " computations=7 negatives=",
                  (uint64_t)surface_prenex_data_prenex_groups_negatives_count +
                      (uint64_t)surface_prenex_data_prenex_groups_parsers_count + 4);
  return task_pure();
}

/* arguments(args) = R.run("PRENEX-GROUPS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRENEX-GROUPS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
