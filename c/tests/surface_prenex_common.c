/* Port of tests/surface_prenex_common.bend (108 lines): kind names the refusal class of an error,
 * refusal and parse_refusal expect a refusal with a given class and text, control checks an optional control
 * source, negatives, parsers and positives run the frozen tables of surface_prenex_data, absent_entries,
 * family_count and family_names inspect a global context, action/polls/exhausted measure and starve the
 * budgeted check of a program, and boundary_decl/ast_boundary refuse a prenex declaration outside a program
 * catalog. Library only. */
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_prelude_poly.h"
#include "surface_prenex_common.h"
#include "surface_raw_common.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* kind(error): parse, unbound, universe, mismatch and budget errors by name, every other error "unexpected". */
Str surface_prenex_common_kind(Error error) {
  switch (error.kind) {
  case ERROR_PARSE: return str_lit("parse");
  case ERROR_UNBOUND: return str_lit("unbound");
  case ERROR_UNIVERSE: return str_lit("universe");
  case ERROR_MISMATCH: return str_lit("mismatch");
  case ERROR_BUDGET_EXHAUSTED: return str_lit("budget");
  case ERROR_NOT_YET:
  case ERROR_CARRY:
  case ERROR_QUANTITY:
  case ERROR_WRONG_LEG:
  case ERROR_MISSING_BRANCH:
  case ERROR_OVERFLOW:
  case ERROR_CANNOT_INFER:
  case ERROR_INDEX_NOT_ZERO:
  case ERROR_INDEX_ABOVE_UNIVERSE:
  case ERROR_TERMINATION: return str_lit("unexpected");
  }
  return str_lit("unexpected");
}

/* refusal(label, expected_kind, expected, result): the kind "rendered" matches the prefix of E.to_string,
 * every other kind matches kind(error) and the prefix of E.message. */
Task surface_prenex_common_refusal(Heap *heap, Str label, Str expected_kind, Str expected, Result result) {
  if (result.kind == RESULT_DONE) return task_fail(cat(heap, label, str_lit(": expected refusal")));
  Str rendered = error_to_string(heap, result.error);
  bool matched = false;
  if (str_eq(expected_kind, str_lit("rendered"))) {
    matched = surface_prelude_poly_starts(expected, rendered);
  } else {
    matched = str_eq(surface_prenex_common_kind(result.error), expected_kind) &&
              surface_prelude_poly_starts(expected, error_message(heap, result.error));
  }
  return relational_require(matched, cat(heap, cat(heap, label, str_lit(": wrong refusal: ")), rendered));
}

/* control(source, globals): None{} passes, Some{source} must check in globals. */
Task surface_prenex_common_control(Heap *heap, const char *source, Globals globals) {
  if (source == NULL) return task_pure();
  const ElabChecked *checked = NULL;
  return relational_checked(heap, globals, str_lit(source), &checked);
}

/* negatives(cases, globals): each control checks, then the source is refused with its kind and detail. */
Task surface_prenex_common_negatives(Heap *heap, const PrenexNegative *cases, size_t len, Globals globals) {
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(surface_prenex_common_control(heap, cases[i].good, globals));
    RELATIONAL_TRY(surface_prenex_common_refusal(
        heap, str_lit(cases[i].name), str_lit(cases[i].kind), str_lit(cases[i].detail),
        elab_check_in(heap, globals, budget_unlimited(), str_lit(cases[i].source))));
  }
  return task_pure();
}

/* parse_refusal(label, expected, prefix, result): a parse error whose message starts with (prefix) or equals
 * expected; any other error fails with its rendering. */
Task surface_prenex_common_parse_refusal(Heap *heap, Str label, Str expected, bool prefix, Result result) {
  if (result.kind == RESULT_DONE) return task_fail(cat(heap, label, str_lit(": expected parse refusal")));
  if (result.error.kind != ERROR_PARSE) {
    return task_fail(cat(heap, cat(heap, label, str_lit(": ")), error_to_string(heap, result.error)));
  }
  Str message = result.error.message;
  bool matched = false;
  if (prefix) {
    matched = surface_prelude_poly_starts(expected, message);
  } else {
    matched = str_eq(expected, message);
  }
  return relational_require(matched, cat(heap, cat(heap, label, str_lit(": ")), message));
}

/* parsers(cases, prefix) */
Task surface_prenex_common_parsers(Heap *heap, const PrenexParseCase *cases, size_t len, bool prefix) {
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(surface_prenex_common_parse_refusal(heap, str_lit(cases[i].name), str_lit(cases[i].expected),
                                                       prefix, parser_program(heap, str_lit(cases[i].source))));
  }
  return task_pure();
}

/* positives(cases): each source round-trips through the parser and checks in G.empty(); rows are unused. */
Task surface_prenex_common_positives(Heap *heap, const PrenexPositive *cases, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const Vec *parsed = NULL;
    const ElabChecked *checked = NULL;
    RELATIONAL_TRY(relational_parse_roundtrip(heap, str_lit(cases[i].source), &parsed));
    RELATIONAL_TRY(relational_checked(heap, global_empty(), str_lit(cases[i].source), &checked));
  }
  return task_pure();
}

/* absent_entries(names, globals): no name has an entry. */
bool surface_prenex_common_absent_entries(const Vec *names, Globals globals) {
  for (size_t i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return false;
    if (!surface_raw_common_absent(global_find(*(const Str *)got.value, globals).some)) return false;
  }
  return true;
}

/* family_count(globals): R.length of the family rows. */
uint64_t surface_prenex_common_family_count(Globals globals) {
  uint64_t count = 0;
  for (const FamilyRows *rows = globals.families; rows != NULL; rows = rows->rest) count += 1;
  return count;
}

/* family_names(globals): R.family_names of the family rows. */
Vec surface_prenex_common_family_names(Heap *heap, Globals globals) {
  return relational_family_names(heap, globals.families);
}

/* action(globals, source): B.lift(Parser.program(source)), then Elab.elab_program_comp(globals, parsed). */
static Comp action_elab(Heap *heap, void *ctx, void *value) {
  return elab_elab_program_comp(heap, *(const Globals *)ctx, value);
}

Comp surface_prenex_common_action(Heap *heap, Globals globals, Str source) {
  Globals *ctx = heap_alloc(heap, sizeof *ctx);
  if (ctx == NULL) return budget_failed(error_host_oom());
  *ctx = globals;
  return budget_bind(heap, budget_lift(parser_program(heap, source)), (NextK){action_elab, ctx});
}

/* polls(source): Raw.measure of the check of source in G.empty() under B.of_polls(100000n). */
Task surface_prenex_common_polls(Heap *heap, Str source, uint64_t *polls) {
  return surface_raw_common_measure(
      heap,
      budget_run_state(heap, surface_prenex_common_action(heap, global_empty(), source),
                       budget_of_polls(nat_of_u64(heap->arena, 100000))),
      polls);
}

/* exhausted(label, source, allowance): the check of source runs out after allowance polls. */
Task surface_prenex_common_exhausted(Heap *heap, Str label, Str source, uint64_t allowance) {
  return surface_prenex_common_refusal(
      heap, label, str_lit("budget"), str_lit("the check budget is exhausted"),
      budget_run_poll(heap, surface_prenex_common_action(heap, global_empty(), source),
                      surface_foundation_common_countdown(heap, allowance)));
}

/* boundary_decl(declarations): one declaration, refused by Elab.elab_decl outside a program catalog. */
Task surface_prenex_common_boundary_decl(Heap *heap, const Vec *declarations) {
  VecGet first = vec_get(declarations, 0);
  if (declarations->len != 1 || first.status == VEC_GET_OUT_OF_RANGE) {
    return task_fail(str_lit("expected one boundary declaration"));
  }
  return surface_raw_common_refusal(
      heap, error_make(ERROR_MISMATCH, str_lit("a prenex declaration requires a program catalog")),
      elab_elab_decl(heap, first.value, check_make(heap, global_empty(), budget_unlimited(), nat_zero())));
}

/* ast_boundary(source) */
Task surface_prenex_common_ast_boundary(Heap *heap, Str source) {
  void *parsed = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, source), &parsed));
  if (parsed == NULL) return task_fail(str_lit("expected one boundary declaration"));
  return surface_prenex_common_boundary_decl(heap, parsed);
}
