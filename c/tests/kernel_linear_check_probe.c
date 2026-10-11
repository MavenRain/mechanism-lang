/* Port of bend2/tests/kernel_linear_check_probe.bend (no live oracle call: c-test compares the
 * output with c/tests/kernel_linear_check_probe.expected). */
#include "driver.h"

/* result(value): Done -> "linear raw check PASS"; Fail{e} -> "linear raw check FAIL " ++ E.to_string(e). */
static int result(Heap *heap, Result value) {
  switch (value.kind) {
    case RESULT_DONE:
      return driver_print(str_lit("linear raw check PASS"));
    case RESULT_FAIL:
      return driver_print(heap_str_cat(heap, str_lit("linear raw check FAIL "), error_to_string(heap, value.error)));
  }
  return driver_host_failure();
}

/* body = T.Sec{shape, [T.Leg{[C.Pair{Q.QOne{}, "x"}], T.Var{0n}}]} */
static const Term *body_term(Heap *heap, Shape shape) {
  Vec binders = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_ONE, str_lit("x"));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  Vec legs = vec_new(heap->arena);
  Leg *leg = term_leg_new(heap, binders, term_var(heap, nat_zero()));
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, shape, legs);
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Shape shape = shape_spi(QUANTITY_ONE, str_lit("x"), (void *)term_global(&heap, str_lit("Nat")));
  Globals globals = global_initial(&heap);
  const Term *body = body_term(&heap, shape);
  /* do Result: ty <- Eval.eval(globals, [], T.Ran{shape, T.Global{"Nat"}}); Check.check_term(globals, Unlimited, body, ty) */
  Result ty = eval_eval(&heap, globals, vec_new(heap.arena), term_ran(&heap, shape, term_global(&heap, str_lit("Nat"))));
  Result outcome = ty.kind == RESULT_FAIL ? ty : check_check_term(&heap, globals, budget_unlimited(), body, ty.value);
  int code = heap.oom ? driver_host_failure() : result(&heap, outcome);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
