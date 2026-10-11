/* Port of bend2/tests/kernel_check.bend (oracle label unit-kernel-check). */
#include "driver.h"

/* success(result): Done{Unit} -> True; Fail -> False */
static bool success(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return true;
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* quantity_error(result): Fail{Error.Quantity{msg}} -> True; _ -> False */
static bool quantity_error(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return false;
    case RESULT_FAIL:
      return result.error.kind == ERROR_QUANTITY;
  }
  return false;
}

/* universe_error(result): Fail{Error.Universe{msg}} -> True; _ -> False */
static bool universe_error(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return false;
    case RESULT_FAIL:
      return result.error.kind == ERROR_UNIVERSE;
  }
  return false;
}

/* Term.Sec{Shape.SPi{Q.QOne{}, "x", dom}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}], body}]} */
static const Term *sec_x(Heap *heap, const Term *dom, const Term *body) {
  Vec binders = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_ONE, str_lit("x"));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  Vec legs = vec_new(heap->arena);
  Leg *leg = term_leg_new(heap, binders, body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, shape_spi(QUANTITY_ONE, str_lit("x"), (void *)dom), legs);
}

/* check_typed(ty, ctx): Fail -> False; Done{expected} ->
 * success(check(ctx, Many, Sec{SPi{One, "x", nat}, [Leg{[(One, "x")], Var 0}]}, expected)) &&
 * quantity_error(check(ctx, Many, Sec{SPi{One, "x", nat}, [Leg{[(One, "x")], Lit 0}]}, expected)) */
static bool check_typed(Heap *heap, Result ty, Ctx ctx) {
  switch (ty.kind) {
    case RESULT_FAIL:
      return false;
    case RESULT_DONE: {
      const Value *expected = ty.value;
      const Term *linear = sec_x(heap, prim_nat_ty(heap), term_var(heap, nat_zero()));
      const Term *dropped = sec_x(heap, prim_nat_ty(heap), term_lit(heap, literal_lint(big_zero())));
      bool a = success(check_check(heap, ctx, QUANTITY_MANY, linear, expected));
      bool b = quantity_error(check_check(heap, ctx, QUANTITY_MANY, dropped, expected));
      return a && b;
    }
  }
  return false;
}

/* verify(ok): True -> IO.print(...); False -> IO.die(1, "FAIL checker linearity and raw universe scope") */
static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS checker linearity and raw universe scope"));
  return driver_die(1, "FAIL checker linearity and raw universe scope");
}

static int run(Heap *heap) {
  Globals globals = global_initial(heap);
  Ctx ctx = check_make(heap, globals, budget_unlimited(), nat_zero());
  const Term *arrow = prim_arrow(heap, QUANTITY_ONE, str_lit("x"), prim_nat_ty(heap), prim_nat_ty(heap));
  bool typed = check_typed(heap, eval_eval(heap, globals, vec_new(heap->arena), arrow), ctx);
  const Term *raw = sec_x(heap, term_univ(heap, level_var(heap, nat_zero())), term_var(heap, nat_zero()));
  bool scoped = universe_error(check_check(heap, ctx, QUANTITY_MANY, raw, value_vuniv(heap, level_one(heap))));
  return verify(typed && scoped);
}

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
