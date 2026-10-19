/* Port of tests/surface_expectations.bend (oracle label driver-surface-expectations): the
 * injection expectation helper and the public elaboration of SInj agree on every width case.
 * R.require and R.finish of tests/relational_common.bend are inlined here because the rest of
 * that module needs the parser, the elaborator and the cli output (S5.1). */
#include "driver.h"
#include "surface.h"

/* Result<String, Unit>: ok carries Unit, otherwise the failure message. */
typedef struct {
  bool ok;
  Str message;
} Check;

/* R.require(ok, message) */
static Check require(bool ok, Str message) {
  Check out;
  out.ok = ok;
  out.message = message;
  return out;
}

static Leg *leg_or_oom(Heap *heap, Leg *leg) {
  if (leg == NULL) heap->oom = true;
  return leg;
}

/* good(): T.Leg{[], T.Global{"Nat"}} */
static Leg *good(Heap *heap) {
  return leg_or_oom(heap, term_leg_new(heap, vec_new(heap->arena), term_global(heap, str_lit("Nat"))));
}

/* bad(index): T.Leg{[], T.Var{index}} */
static Leg *bad(Heap *heap, uint64_t index) {
  const Term *var = term_var(heap, heap_nat_of_u64(heap, index));
  return leg_or_oom(heap, term_leg_new(heap, vec_new(heap->arena), var));
}

/* The first `count` of the two legs as a list (count is 0, 1 or 2). */
static Vec legs(Heap *heap, size_t count, Leg *first, Leg *second) {
  Vec out = vec_new(heap->arena);
  if (count >= 1 && !vec_push(&out, first)) heap->oom = true;
  if (count >= 2 && !vec_push(&out, second)) heap->oom = true;
  return out;
}

/* expected(count, legs):
 * V.VLan{Shape.SColl{count}, V.Closure{[], T.Sec{Shape.coll(List.length(legs)), legs}}, None{}} */
static MaybeValue expected(Heap *heap, Big count, Vec legs) {
  MaybeValue out;
  Shape coll = shape_coll(heap, heap_nat_of_u64(heap, (uint64_t)legs.len));
  Closure diagram = value_closure(vec_new(heap->arena), term_sec(heap, coll, legs));
  MaybeLevelVar level;
  level.some = false;
  level.value = NULL;
  out.some = true;
  out.value = value_vlan(heap, shape_scoll(count), diagram, level);
  return out;
}

static MaybeValue no_expectation(void) {
  MaybeValue out;
  out.some = false;
  out.value = NULL;
  return out;
}

/* helper_result(result): Fail{error} -> E.to_string(error); Done{values} -> "OK:" ++ Nat.show(length) */
static Str helper_result(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, result.error);
    case RESULT_DONE: {
      const Vec *values = result.value;
      uint64_t count = values == NULL ? 0u : (uint64_t)values->len;
      return str_cat(heap->arena, str_lit("OK:"), str_of_u64(heap->arena, count));
    }
  }
  return str_lit("");
}

/* elab_result(result): Fail{error} -> E.to_string(error); Done{term} -> "OK" */
static Str elab_result(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, result.error);
    case RESULT_DONE:
      return str_lit("OK");
  }
  return str_lit("");
}

static Budget zero_polls(void) { return budget_of_polls(nat_zero()); }

/* probe(name, count, ty, helper, elaborated): a fresh checker context; the helper and the public
 * elaboration of SInj{0, count, SProp} must both print the expected strings. */
static Check probe(Heap *heap, const char *name, Big count, MaybeValue ty, const char *helper,
                   const char *elaborated) {
  Ctx ctx = check_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  Comp helper_comp = elab_term_injection_expectations(heap, ctx, ty, count);
  Str actual_helper = helper_result(heap, budget_run(heap, helper_comp, zero_polls()));
  const STerm *syntax = sterm_inj(heap, big_zero(), count, sterm_prop());
  Comp elab_comp = elab_term_elab(heap, ctx, ty, syntax);
  Str actual_elab = elab_result(heap, budget_run(heap, elab_comp, zero_polls()));
  Check checked = require(str_eq(actual_helper, str_lit(helper)),
                          str_cat(heap->arena, str_lit(name), str_lit(": helper result changed")));
  if (!checked.ok) return checked;
  return require(str_eq(actual_elab, str_lit(elaborated)),
                 str_cat(heap->arena, str_lit(name), str_lit(": public elaboration result changed")));
}

/* finite(name, count, legs, helper, elaborated) =
 * probe(name, Big.of_nat(count), Some{expected(Big.of_nat(count), legs)}, helper, elaborated) */
static Check finite(Heap *heap, const char *name, uint64_t count, Vec legs, const char *helper,
                    const char *elaborated) {
  Big width = big_of_nat(heap, heap_nat_of_u64(heap, count));
  return probe(heap, name, width, expected(heap, width, legs), helper, elaborated);
}

static Check suite(Heap *heap) {
  const char *wrong = "wrong leg: the leg address is outside the collection";
  const char *unbound = "unbound: de Bruijn index 0 is outside the environment";
  const char *negative = "mismatch: a collection width must be nonnegative";
  Big minus_one = big_neg(big_one());
  Big huge = big_mul(heap, big_of_nat(heap, heap_nat_of_u64(heap, 999999999u)),
                     big_of_nat(heap, heap_nat_of_u64(heap, 1000000000u)));
  Check checked;
  checked = finite(heap, "valid-one", 1, legs(heap, 1, good(heap), NULL), "OK:1", "OK");
  if (!checked.ok) return checked;
  checked = finite(heap, "valid-two", 2, legs(heap, 2, good(heap), good(heap)), "OK:2", "OK");
  if (!checked.ok) return checked;
  checked = finite(heap, "extra-good", 1, legs(heap, 2, good(heap), good(heap)), "OK:1", "OK");
  if (!checked.ok) return checked;
  checked = finite(heap, "extra-bad", 1, legs(heap, 2, good(heap), bad(heap, 0)), "OK:1", "OK");
  if (!checked.ok) return checked;
  checked = finite(heap, "missing-leg", 2, legs(heap, 1, good(heap), NULL), wrong, wrong);
  if (!checked.ok) return checked;
  checked = finite(heap, "zero-extra-bad", 0, legs(heap, 1, bad(heap, 0), NULL), "OK:0", "OK");
  if (!checked.ok) return checked;
  checked = finite(heap, "bad-before-missing", 2, legs(heap, 1, bad(heap, 0), NULL), unbound, unbound);
  if (!checked.ok) return checked;
  checked = finite(heap, "first-of-two-errors", 2, legs(heap, 2, bad(heap, 0), bad(heap, 1)), unbound, unbound);
  if (!checked.ok) return checked;
  checked = finite(heap, "zero-empty", 0, legs(heap, 0, NULL, NULL), "OK:0", "OK");
  if (!checked.ok) return checked;
  checked = probe(heap, "mismatch-width", big_one(),
                  expected(heap, big_of_nat(heap, heap_nat_of_u64(heap, 2)), legs(heap, 1, bad(heap, 0), NULL)),
                  "OK:0", "OK");
  if (!checked.ok) return checked;
  checked = probe(heap, "huge-count", huge, expected(heap, huge, legs(heap, 1, good(heap), NULL)), wrong, wrong);
  if (!checked.ok) return checked;
  checked = probe(heap, "negative-count", minus_one, expected(heap, minus_one, legs(heap, 1, good(heap), NULL)),
                  negative, negative);
  if (!checked.ok) return checked;
  checked = probe(heap, "negative-none", minus_one, no_expectation(), negative, negative);
  if (!checked.ok) return checked;
  return probe(heap, "negative-mismatch", minus_one, expected(heap, big_one(), legs(heap, 1, bad(heap, 0), NULL)),
               negative, negative);
}

/* R.finish("SURFACE-EXPECTATIONS", result): Done{fields} -> print TAG-OK fields;
 * Fail{message} -> print TAG-FAIL message, then Out.cli_exit(1). */
static int finish(Heap *heap, Check result) {
  if (result.ok) return driver_print(str_lit("SURFACE-EXPECTATIONS-OK cases=14 zero-poll=14"));
  driver_print(str_cat(heap->arena, str_lit("SURFACE-EXPECTATIONS-FAIL "), result.message));
  return 1;
}

/* arguments(args): the arguments are not read. */
static int run(Heap *heap) { return finish(heap, suite(heap)); }

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
