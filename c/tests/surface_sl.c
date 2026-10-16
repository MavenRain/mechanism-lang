/* Port of bend2/tests/surface_sl.bend: the SL surface sugar cases of surface_sl_data. Each case parses,
 * prints and reparses a source, refuses an invalid source in the parser or the elaborator, or checks that
 * two spellings give the same declarations or the same checked form. Prints one SL-SURFACE <name> OK (or
 * FAIL: message) line per case, then SL-SURFACE-OK <n>/36 and SL-SURFACE OK; fewer than 36 passes die
 * with code 1 and SL-SURFACE FAIL. Takes no argv. */
#include "driver.h"
#include "relational_common.h"
#include "relational_syntax_equal.h"
#include "surface_sl_data.h"

/* refusal(result, expected, message) */
static Task refusal(Heap *heap, Result result, Str expected, Str message) {
  if (result.kind == RESULT_DONE) return task_fail(message);
  return relational_require(str_eq(error_message(heap, result.error), expected), error_to_string(heap, result.error));
}

/* same_tree(first, second) */
static Task same_tree(Heap *heap, Str first, Str second) {
  void *a = NULL;
  void *b = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, first), &a));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, second), &b));
  return relational_require(syntax_equal_declarations(a, b), str_lit("sugar changed the family tree"));
}

/* same_checked(first, second) */
static Task same_checked(Heap *heap, Str first, Str second) {
  void *a = NULL;
  void *b = NULL;
  RELATIONAL_TRY(relational_kernel(heap, elab_check_text(heap, global_initial(heap), budget_unlimited(), first), &a));
  RELATIONAL_TRY(relational_kernel(heap, elab_check_text(heap, global_initial(heap), budget_unlimited(), second), &b));
  return relational_require(str_eq(elab_checked_form(heap, a), elab_checked_form(heap, b)),
                            str_lit("match changed checked elimination"));
}

/* action(action) */
static Task action(Heap *heap, SlAction act) {
  const Vec *parsed = NULL;
  switch (act.kind) {
    case SL_ROUND_TRIP:
      return relational_parse_roundtrip(heap, str_lit(act.first), &parsed);
    case SL_PARSE_REFUSAL:
      return refusal(heap, parser_program(heap, str_lit(act.first)), str_lit(act.second),
                     str_lit("invalid source parsed"));
    case SL_SAME_TREE:
      return same_tree(heap, str_lit(act.first), str_lit(act.second));
    case SL_SAME_CHECKED:
      return same_checked(heap, str_lit(act.first), str_lit(act.second));
    case SL_CHECKED_REFUSAL:
      return refusal(heap, elab_check_text(heap, global_initial(heap), budget_unlimited(), str_lit(act.first)),
                     str_lit(act.second), str_lit("invalid source elaborated"));
  }
  return task_fail(str_lit("unknown SL surface action"));
}

/* report(name, result): prints the case line and returns 1 for a pass, 0 for a failure. */
static uint64_t report(Heap *heap, Str name, Task result) {
  Str head = str_cat(heap->arena, str_lit("SL-SURFACE "), name);
  if (result.ok) {
    driver_print(str_cat(heap->arena, head, str_lit(" OK")));
    return 1;
  }
  driver_print(str_cat(heap->arena, str_cat(heap->arena, head, str_lit(" FAIL: ")), result.message));
  return 0;
}

/* outcome(ok) */
static int outcome(bool ok) {
  if (ok) return driver_print(str_lit("SL-SURFACE OK"));
  return driver_die(1, "SL-SURFACE FAIL");
}

/* finish(passed) */
static int finish(Heap *heap, uint64_t passed) {
  driver_print(str_cat(heap->arena, str_cat(heap->arena, str_lit("SL-SURFACE-OK "), str_of_u64(heap->arena, passed)),
                       str_lit("/36")));
  return outcome(passed == 36);
}

/* run(cases, count): the cases from index on, in Bend order. */
static int run(Heap *heap, size_t index, uint64_t count) {
  if (index >= surface_sl_data_cases_count) return finish(heap, count);
  const SlCase *c = &surface_sl_data_cases[index];
  uint64_t passed = report(heap, str_lit(c->name), action(heap, c->action));
  return run(heap, index + 1, count + passed);
}

/* arguments(args) = main() = run(Data.cases(), 0) */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap, 0, 0);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
