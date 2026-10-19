/* Port of tests/surface_prenex.bend (51 lines, oracle label surface-prenex): the prenex fixture parses,
 * prints and checks without leaking its symbolic templates, audits, keeps its row order, normalizes four
 * computations, refuses the prenex negatives, an exhausted budget and a catalog-lifetime specialization, and
 * checks a large universe literal. Prints PRENEX-OK fields or PRENEX-FAIL message. With
 * -DMECH_DRIVER_LIBRARY the file is the Prenex library of surface_prenex_families (no main). */
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_prenex_common.h"
#include "surface_prenex.h"

/* zero() = In{SMu{"Tiny", []}, ACtor{"tinyZero"}, []} */
const Term *surface_prenex_zero(Heap *heap) {
  return term_in(heap, shape_smu(str_lit("Tiny"), vec_new(heap->arena)), term_addr_ctor(str_lit("tinyZero")),
                 vec_new(heap->arena));
}

/* succ(value) = In{SMu{"Tiny", []}, ACtor{"tinySucc"}, [value]} */
const Term *surface_prenex_succ(Heap *heap, const Term *value) {
  Vec args = vec_new(heap->arena);
  if (value == NULL || !vec_push(&args, (void *)value)) {
    heap->oom = true;
    return NULL;
  }
  return term_in(heap, shape_smu(str_lit("Tiny"), vec_new(heap->arena)), term_addr_ctor(str_lit("tinySucc")), args);
}

/* tiny() = Lan{SMu{"Tiny", []}, Rules.diagram_of([])} */
const Term *surface_prenex_tiny(Heap *heap) {
  Vec none = vec_new(heap->arena);
  return term_lan(heap, shape_smu(str_lit("Tiny"), vec_new(heap->arena)), rules_diagram_of(heap, &none));
}

#ifndef MECH_DRIVER_LIBRARY

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
  static const char *const names[] = {"proofIdentity", "dataIdentity",     "typeIdentity",  "againIdentity",
                                      "dataValue",     "againValue",       "proofValue",    "typeValue",
                                      "predicateType", "higherType",       "predicateWitness", "higherWitness",
                                      "joined",        "joinedWitness",    "applied",       "appliedValue"};
  return strings(heap, names, sizeof names / sizeof names[0]);
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

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str source = str_empty();
  const Vec *tree = NULL;
  const ElabChecked *program = NULL;
  const ElabChecked *large_checked = NULL;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/prenex.mech"), &source));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, source, &tree));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &program));
  Globals globals = relational_globals(program);
  static const char *const templates[] = {"identity", "functionType", "joinedSort", "appliedIdentity"};
  Vec escaped = strings(heap, templates, 4);
  RELATIONAL_TRY(relational_require(surface_prenex_common_absent_entries(&escaped, globals),
                                    str_lit("symbolic template escaped")));
  RELATIONAL_TRY(surface_foundation_common_audit(heap, globals));
  Vec names = relational_entry_names(heap, relational_rows(program));
  Vec expected_rows = rows(heap);
  RELATIONAL_TRY(relational_require(relational_strings(&names, &expected_rows), str_lit("row order changed")));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "dataValue", surface_prenex_succ(heap, surface_prenex_zero(heap)));
  push_row(heap, &computations, "againValue",
           surface_prenex_succ(heap, surface_prenex_succ(heap, surface_prenex_zero(heap))));
  push_row(heap, &computations, "typeValue", surface_prenex_tiny(heap));
  push_row(heap, &computations, "appliedValue", surface_prenex_succ(heap, surface_prenex_zero(heap)));
  RELATIONAL_TRY(relational_computations(heap, &computations, globals));
  RELATIONAL_TRY(surface_prenex_common_negatives(heap, surface_prenex_data_prenex_negatives,
                                                 surface_prenex_data_prenex_negatives_count, global_empty()));
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("budget"), str_lit(surface_prenex_data_prenex_identity), 0));
  RELATIONAL_TRY(surface_prenex_common_refusal(
      heap, str_lit("catalog-lifetime"), str_lit("unbound"), str_lit("unknown universe schema identity"),
      elab_check_in(heap, globals, budget_unlimited(), str_lit("specialize identity (1) as later"))));
  Str large = str_lit("def p : Sort (succ (succ 999999999999999999)) := Sort (succ 999999999999999999)");
  RELATIONAL_TRY(relational_parse_roundtrip(heap, large, &tree));
  RELATIONAL_TRY(relational_checked(heap, global_empty(), large, &large_checked));
  Str entries = str_of_u64(heap->arena, surface_foundation_common_entry_count(globals));
  Str negatives = str_of_u64(heap->arena, (uint64_t)surface_prenex_data_prenex_negatives_count + 2);
  *fields = str_cat(heap->arena,
                    str_cat(heap->arena, str_cat(heap->arena, str_lit("entries="), entries),
                            str_lit(" computations=4 negatives=")),
                    negatives);
  return task_pure();
}

/* arguments(args) = R.run("PRENEX", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRENEX"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }

#endif
