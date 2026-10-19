/* Port of tests/surface_template_common.bend (69 lines): exact expects a refusal with a given rendered
 * text, negatives runs a frozen Data.Negative table through Elab.check_in, required_family/family look up a
 * family, family_rows/entries project a global context, run runs Raw.Case rows and counts them,
 * same_families/new_family_names/no_axioms compare contexts and disclosures, altered damages a family in one
 * of three ways and mismatch is the reuse refusal. Library only. */
#include "surface_prenex_common.h"
#include "surface_raw_common.h"
#include "surface_template_common.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* exact(label, expected, result): Done is a failure; Fail must render exactly as expected. */
Task surface_template_common_exact(Heap *heap, Str label, Str expected, Result result) {
  if (result.kind == RESULT_DONE) {
    return task_fail(cat(heap, cat(heap, label, str_lit(": expected refusal: ")), expected));
  }
  Str rendered = error_to_string(heap, result.error);
  return relational_require(str_eq(expected, rendered),
                            cat(heap, cat(heap, label, str_lit(": wrong refusal: ")), rendered));
}

/* negatives(cases, globals): each row's source checked with an unlimited budget must refuse with its detail. */
Task surface_template_common_negatives(Heap *heap, const PrenexNegative *cases, size_t len, Globals globals) {
  for (size_t i = 0; i < len; i++) {
    Result checked = elab_check_in(heap, globals, budget_unlimited(), str_lit(cases[i].source));
    RELATIONAL_TRY(surface_template_common_exact(heap, str_lit(cases[i].name), str_lit(cases[i].detail), checked));
  }
  return task_pure();
}

/* required_family(value): Some -> Done value, None -> Fail "missing family". */
Task surface_template_common_required_family(MaybeFamily value, Family *out) {
  if (!value.some) return task_fail(str_lit("missing family"));
  *out = value.value;
  return task_pure();
}

/* family(name, globals) */
Task surface_template_common_family(Str name, Globals globals, Family *out) {
  return surface_template_common_required_family(global_find_family(name, globals), out);
}

/* family_rows(globals): the families field. */
const FamilyRows *surface_template_common_family_rows(Globals globals) { return globals.families; }

/* entries(globals): the entries field. */
const EntryRows *surface_template_common_entries(Globals globals) { return globals.entries; }

/* run(cases, count): each case through Raw.labeled in order, stop at the first failure; Done the count. */
Task surface_template_common_run(Heap *heap, const RawCase *cases, size_t len, uint64_t count, uint64_t *out) {
  uint64_t ran = count;
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(surface_raw_common_labeled(heap, cases[i].name, cases[i].test(heap, cases[i].ctx)));
    ran++;
  }
  *out = ran;
  return task_pure();
}

/* same_families(a, b): the same family count and every family of a found unchanged in b. */
bool surface_template_common_same_families(Globals a, Globals b) {
  return surface_prenex_common_family_count(a) == surface_prenex_common_family_count(b) &&
         surface_raw_common_families(surface_template_common_family_rows(a), b);
}

/* new_family_names(globals, initial) */
Vec surface_template_common_new_family_names(Heap *heap, Globals globals, Globals initial) {
  return relational_family_names(heap,
                                 relational_new_families(heap, surface_template_common_family_rows(globals), initial));
}

/* no_axioms(rows): no disclosure names. */
bool surface_template_common_no_axioms(Heap *heap, const Vec *rows) {
  Vec names = elab_disclosure_names(heap, rows);
  Vec none = vec_new(heap->arena);
  return relational_strings(&names, &none);
}

/* altered(family, mode) */
Family surface_template_common_altered(Family family, uint64_t mode) {
  Family changed = family;
  switch (mode) {
  case 0: changed.positive = false; break;
  case 1: changed.status = positivity_provisional(); break;
  default: changed.ctors = vec_new(family.ctors.arena); break;
  }
  return changed;
}

/* mismatch(name) */
Error surface_template_common_mismatch(Heap *heap, Str name) {
  return error_make(ERROR_MISMATCH,
                    cat(heap, cat(heap, str_lit("the reused family "), name), str_lit(" does not match the template")));
}
