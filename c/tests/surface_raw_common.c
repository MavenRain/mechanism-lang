/* Port of bend2/tests/surface_raw_common.bend (84 lines): refusal and exact compare a refusal with the
 * expected kernel error (prefix or equal text), collision/budget_error/scope_error build the expected errors,
 * entries/families/same_globals compare two global tables, labeled and run sequence named cases, and
 * measure reads the polls a finite budget spent. Library only: no main. */
#include "driver.h"
#include "surface_prelude_poly.h"
#include "surface_raw_common.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* refusal(expected, result): Audit.starts(E.to_string(expected), E.to_string(actual)). */
Task surface_raw_common_refusal(Heap *heap, Error expected, Result result) {
  Str text = error_to_string(heap, expected);
  if (result.kind == RESULT_DONE) return task_fail(cat(heap, str_lit("expected refusal: "), text));
  Str actual = error_to_string(heap, result.error);
  return relational_require(surface_prelude_poly_starts(text, actual), cat(heap, str_lit("wrong refusal: "), actual));
}

/* exact(expected, result): String.eq(E.to_string(expected), E.to_string(actual)). */
Task surface_raw_common_exact(Heap *heap, Error expected, Result result) {
  Str text = error_to_string(heap, expected);
  if (result.kind == RESULT_DONE) return task_fail(cat(heap, str_lit("expected refusal: "), text));
  Str actual = error_to_string(heap, result.error);
  return relational_require(str_eq(text, actual), cat(heap, str_lit("wrong refusal: "), actual));
}

/* collision(name) */
Error surface_raw_common_collision(Heap *heap, Str name) {
  return error_make(ERROR_MISMATCH, cat(heap, cat(heap, str_lit("the name "), name), str_lit(" is already declared")));
}

/* absent(value): None answers True; the Maybe payload is not read. */
bool surface_raw_common_absent(bool some) { return !some; }

/* present(value) = Bool.not(absent(value)) */
bool surface_raw_common_present(bool some) { return !surface_raw_common_absent(some); }

/* matched_entry(expected, actual) */
bool surface_raw_common_matched_entry(Entry expected, MaybeEntry actual) {
  return actual.some && relational_same_entry(expected, actual.value);
}

/* matched_family(expected, actual) */
bool surface_raw_common_matched_family(Family expected, MaybeFamily actual) {
  return actual.some && positivity_same_family(expected, actual.value);
}

/* entries(rows, globals): every row is found in globals with the same entry. */
bool surface_raw_common_entries(const EntryRows *rows, Globals globals) {
  for (const EntryRows *row = rows; row != NULL; row = row->rest) {
    if (!surface_raw_common_matched_entry(row->entry, global_find(row->name, globals))) return false;
  }
  return true;
}

/* families(rows, globals): every row is found in globals with the same family. */
bool surface_raw_common_families(const FamilyRows *rows, Globals globals) {
  for (const FamilyRows *row = rows; row != NULL; row = row->rest) {
    if (!surface_raw_common_matched_family(row->family, global_find_family(row->name, globals))) return false;
  }
  return true;
}

/* R.length over family rows */
static size_t family_rows_length(const FamilyRows *rows) {
  size_t count = 0;
  for (const FamilyRows *row = rows; row != NULL; row = row->rest) count++;
  return count;
}

/* same_globals(actual, expected): equal row counts, and every actual row found in expected. */
bool surface_raw_common_same_globals(Globals actual, Globals expected) {
  return relational_entry_rows_length(actual.entries) == relational_entry_rows_length(expected.entries) &&
         family_rows_length(actual.families) == family_rows_length(expected.families) &&
         surface_raw_common_entries(actual.entries, expected) && surface_raw_common_families(actual.families, expected);
}

/* labeled(name, result) */
Task surface_raw_common_labeled(Heap *heap, Str name, Task result) {
  if (result.ok) return result;
  return task_fail(cat(heap, cat(heap, name, str_lit(": ")), result.message));
}

/* run(cases, count): each case in order, stop at the first failure; Done "cases=" ++ Nat.show(count + run). */
Task surface_raw_common_run(Heap *heap, const RawCase *cases, size_t len, uint64_t count, Str *out) {
  uint64_t ran = count;
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(surface_raw_common_labeled(heap, cases[i].name, cases[i].test(heap, cases[i].ctx)));
    ran++;
  }
  *out = cat(heap, str_lit("cases="), str_of_u64(heap->arena, ran));
  return task_pure();
}

/* budget_error() */
Error surface_raw_common_budget_error(void) {
  return error_make(ERROR_BUDGET_EXHAUSTED, str_lit("the check budget is exhausted"));
}

/* scope_error() */
Error surface_raw_common_scope_error(void) {
  return error_make(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope"));
}

/* measure(result): Polls{remaining} with Done answers 100000n - remaining (Nat subtraction saturates at 0),
 * any Fail answers its text, any other pair answers the disappearance message. */
Task surface_raw_common_measure(Heap *heap, BudgetState result, uint64_t *polls) {
  if (result.budget.kind == BUDGET_POLLS && result.result.kind == RESULT_DONE) {
    NatToU64 remaining = nat_to_u64(result.budget.remaining);
    *polls = (remaining.status == NAT_TO_U64_OVERFLOW || remaining.value > 100000) ? 0 : 100000 - remaining.value;
    return task_pure();
  }
  if (result.result.kind == RESULT_FAIL) return task_fail(error_to_string(heap, result.result.error));
  return task_fail(str_lit("finite poll measurement disappeared"));
}
