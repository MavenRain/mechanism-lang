/* Port of bend2/tests/surface_prenex_exports.bend (81 lines, oracle label surface-prenex-exports): the export
 * runtime fixture parses, prints and checks on the initial globals with the exported row names in order, no
 * leaked default member names, no fresh family for a reuse and no axiom; two payloads normalize, the composed
 * exports keep their rows, the export positives check, the negatives and parser cases refuse, and three budget
 * boundaries refuse. Prints PRENEX-EXPORTS-OK fields, or dies with code 1 and PRENEX-EXPORTS FAIL message on
 * stderr. With -DMECH_DRIVER_LIBRARY the file is the Exports library of surface_prenex_dependency_exports. */
#include "driver.h"
#include "relational_syntax_equal.h"
#include "surface_prenex_common.h"
#include "surface_prenex_exports.h"
#include "surface_raw_common.h"

/* roundtrip(source, message) */
Task surface_prenex_exports_roundtrip(Heap *heap, Str source, Str message) {
  void *parsed = NULL;
  void *reparsed = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, source), &parsed));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, syntax_print(heap, parsed)), &reparsed));
  return relational_require(syntax_equal_declarations(parsed, reparsed), message);
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

/* positives(cases) */
static Task positives(Heap *heap, const PrenexPositive *cases, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const ElabChecked *checked = NULL;
    Str source = str_lit(cases[i].source);
    RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, source, str_lit("parse/print changed the export mapping")));
    RELATIONAL_TRY(relational_checked(heap, global_empty(), source, &checked));
  }
  return task_pure();
}

/* names() */
static Vec names(Heap *heap) {
  static const char *const items[] = {"Shared_unbox", "Shared_pack",  "Shared_unpack", "freshUnbox",
                                      "freshPack",    "freshUnpack",  "sharedUnbox",   "sharedPack",
                                      "sharedUnpack", "exportInput",  "namedPayload",  "reusedPayload"};
  return strings(heap, items, sizeof items / sizeof items[0]);
}

/* budgets() */
static Task budgets(Heap *heap) {
  Str group = str_lit(surface_prenex_data_prenex_exports_group);
  Str instance = str_lit(surface_prenex_data_prenex_exports_instance);
  uint64_t declaration = 0;
  uint64_t export_polls = 0;
  uint64_t reserved = 0;
  RELATIONAL_TRY(surface_prenex_common_polls(heap, group, &declaration));
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("instance-budget"), instance, declaration));
  RELATIONAL_TRY(surface_prenex_common_polls(heap, instance, &export_polls));
  RELATIONAL_TRY(relational_require(declaration < export_polls, str_lit("the export specialization consumed no budget")));
  uint64_t late = 0;
  if (export_polls > 0) late = export_polls - 1;
  RELATIONAL_TRY(surface_prenex_common_exhausted(heap, str_lit("late-budget"), instance, late));
  Str reservation = str_lit("poly (v) def chosenWitness : Sort (succ v) := Sort v ");
  RELATIONAL_TRY(surface_prenex_common_polls(heap, str_cat(heap->arena, reservation, group), &reserved));
  return surface_prenex_common_exhausted(heap, str_lit("collision-budget"), str_cat(heap->arena, reservation, instance),
                                         reserved);
}

/* composed() */
static Task composed(Heap *heap) {
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_checked(heap, global_empty(), str_lit(surface_prenex_data_prenex_exports_composed_instance),
                                    &program));
  static const char *const rows[] = {"chosenWitness", "chosenLocalAlias", "chosenPick"};
  static const char *const leaked[] = {"Inst_Local_witness", "Inst_Local_alias", "Inst_pick"};
  Vec actual = relational_entry_names(heap, relational_rows(program));
  Vec expected = strings(heap, rows, 3);
  RELATIONAL_TRY(relational_require(relational_strings(&actual, &expected),
                                    str_lit("composed export rows or declaration order changed")));
  Vec absent = strings(heap, leaked, 3);
  return relational_require(surface_prenex_common_absent_entries(&absent, relational_globals(program)),
                            str_lit("composed default member names leaked"));
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
static Task suite(Heap *heap, Str root, Str *fields) {
  Str source = str_empty();
  const ElabChecked *program = NULL;
  RELATIONAL_TRY(relational_read(heap, root, str_lit("test/fixtures/prelude/prenex-exports-runtime.mech"), &source));
  RELATIONAL_TRY(surface_prenex_exports_roundtrip(heap, source, str_lit("parse/print changed the export mapping")));
  Globals initial = global_initial(heap);
  RELATIONAL_TRY(relational_checked(heap, initial, source, &program));
  Globals globals = relational_globals(program);
  Vec entry_names = relational_entry_names(heap, relational_rows(program));
  Vec expected_names = names(heap);
  RELATIONAL_TRY(relational_require(relational_strings(&entry_names, &expected_names),
                                    str_lit("export row names or declaration order changed")));
  static const char *const defaults[] = {"Named_unbox", "Named_pack", "Named_unpack",
                                         "Reused_unbox", "Reused_pack", "Reused_unpack"};
  Vec leaked = strings(heap, defaults, 6);
  RELATIONAL_TRY(relational_require(surface_prenex_common_absent_entries(&leaked, globals),
                                    str_lit("default member names leaked")));
  RELATIONAL_TRY(relational_require(
      surface_raw_common_absent(global_find_family(str_lit("Reused"), globals).some) &&
          surface_raw_common_absent(global_find_family(str_lit("Reused_Wrapped"), globals).some) &&
          surface_prenex_common_family_count(globals) == surface_prenex_common_family_count(global_initial(heap)) + 4,
      str_lit("reuse installed a fresh family")));
  Vec disclosures = elab_disclosure_names(heap, relational_rows(program));
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(relational_require(relational_strings(&disclosures, &none), str_lit("exported members introduced an axiom")));
  Vec computations = vec_new(heap->arena);
  push_row(heap, &computations, "namedPayload", relational_natural(heap, 37));
  push_row(heap, &computations, "reusedPayload", relational_natural(heap, 37));
  RELATIONAL_TRY(relational_computations(heap, &computations, globals));
  RELATIONAL_TRY(composed(heap));
  RELATIONAL_TRY(positives(heap, surface_prenex_data_prenex_exports_positives, surface_prenex_data_prenex_exports_positives_count));
  RELATIONAL_TRY(surface_prenex_common_negatives(heap, surface_prenex_data_prenex_exports_negatives,
                                                 surface_prenex_data_prenex_exports_negatives_count, global_empty()));
  RELATIONAL_TRY(surface_prenex_common_parsers(heap, surface_prenex_data_prenex_exports_parsers,
                                               surface_prenex_data_prenex_exports_parsers_count, true));
  RELATIONAL_TRY(budgets(heap));
  Str out = field(heap, str_empty(), "entries=", (uint64_t)entry_names.len);
  out = field(heap, out, " positives=", (uint64_t)surface_prenex_data_prenex_exports_positives_count);
  out = field(heap, out, " negatives=", (uint64_t)surface_prenex_data_prenex_exports_negatives_count);
  out = field(heap, out, " parser=", (uint64_t)surface_prenex_data_prenex_exports_parsers_count);
  *fields = str_cat(heap->arena, out, str_lit(" budget=3"));
  return task_pure();
}

/* finish(result): Done prints PRENEX-EXPORTS-OK details; Fail dies with code 1. */
static int finish(Heap *heap, Task result, Str details) {
  if (result.ok) return driver_print(str_cat(heap->arena, str_lit("PRENEX-EXPORTS-OK "), details));
  return driver_die_text(1, str_cat(heap->arena, str_lit("PRENEX-EXPORTS FAIL "), result.message));
}

/* arguments(args) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str details = str_empty();
  Task result = suite(&heap, relational_root(argc, argv), &details);
  int code = finish(&heap, result, details);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }

#endif
