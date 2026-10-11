/* Port of bend2/tests/kernel_conversion.bend (oracle label unit-kernel-conversion). */
#include "driver.h"

/* result_true(result): Done{ok} -> ok; Fail -> False */
static bool result_true(BoolResult result) {
  switch (result.kind) {
    case RESULT_DONE:
      return result.value;
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* result_false(result): Done{ok} -> Bool.not(ok); Fail -> False */
static bool result_false(BoolResult result) {
  switch (result.kind) {
    case RESULT_DONE:
      return !result.value;
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* Value.VNeutral{Value.HGlobal{name}, []} */
static const Value *neutral_global(Heap *heap, const char *name) {
  return value_vneutral(heap, value_hglobal(str_lit(name)), vec_new(heap->arena));
}

/* Global.add(name, Global.Axiom{Term.Univ{level}}, globals) */
static Globals add_axiom(Heap *heap, const char *name, const LevelVar *level, Globals globals) {
  return global_add(heap, str_lit(name), global_entry_axiom(term_univ(heap, level)), globals);
}

/* Prim.arrow(Q.QOne{}, name, Prim.nat_ty(), Prim.nat_ty()) */
static const Term *nat_arrow(Heap *heap, const char *name) {
  return prim_arrow(heap, QUANTITY_ONE, str_lit(name), prim_nat_ty(heap), prim_nat_ty(heap));
}

/* eta_value(ctx, ty, value): Done{eta} -> result_true(Conv.conv(ctx, ty, Value.var(0n), eta)); Fail -> False */
static bool eta_value(Heap *heap, Ctx ctx, const Value *ty, Result value) {
  switch (value.kind) {
    case RESULT_DONE:
      return result_true(conv_conv(heap, ctx, ty, value_var(heap, nat_zero()), value.value));
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* Term.Sec{Shape.SPi{Q.QOne{}, "x", nat}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}],
 * Term.Out{Shape.SPi{Q.QOne{}, "x", nat}, Term.APt{Q.QOne{}, Term.Var{0n}}, Term.Var{1n}}}]} */
static const Term *eta_term(Heap *heap) {
  Shape pi = shape_spi(QUANTITY_ONE, str_lit("x"), (void *)prim_nat_ty(heap));
  const Term *body = term_out(heap, pi, term_addr_pt(QUANTITY_ONE, term_var(heap, nat_zero())),
                              term_var(heap, heap_nat_of_u64(heap, 1)));
  Vec binders = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_ONE, str_lit("x"));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  Vec legs = vec_new(heap->arena);
  Leg *leg = term_leg_new(heap, binders, body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, pi, legs);
}

/* eta(ctx, ty): Fail -> False; Done{tyv} ->
 * eta_value(Context.bind("f", Many, tyv, ctx), tyv, Eval.eval(Check.globals_of(ctx), [Value.var(0n)], eta_term)) */
static bool eta(Heap *heap, Ctx ctx, Result ty) {
  switch (ty.kind) {
    case RESULT_FAIL:
      return false;
    case RESULT_DONE: {
      const Value *tyv = ty.value;
      Vec env = vec_new(heap->arena);
      if (!vec_push(&env, (void *)value_var(heap, nat_zero()))) heap->oom = true;
      Ctx bound = context_bind(heap, str_lit("f"), QUANTITY_MANY, tyv, ctx);
      return eta_value(heap, bound, tyv, eval_eval(heap, check_globals_of(ctx), env, eta_term(heap)));
    }
  }
  return false;
}

/* literals(ctx, nat): Done{ty} -> result_false(Conv.conv(ctx, ty, VLit 0, VLit 1)); Fail -> False */
static bool literals(Heap *heap, Ctx ctx, Result nat) {
  switch (nat.kind) {
    case RESULT_DONE: {
      const Value *zero = value_vlit(heap, literal_lint(big_zero()));
      const Value *one = value_vlit(heap, literal_lint(big_one()));
      return result_false(conv_conv(heap, ctx, nat.value, zero, one));
    }
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* type_pair(ctx, a, b): Done{x} Done{y} -> result_true(Conv.conv_type(ctx, x, y)); _ _ -> False */
static bool type_pair(Heap *heap, Ctx ctx, Result a, Result b) {
  if (a.kind != RESULT_DONE || b.kind != RESULT_DONE) return false;
  return result_true(conv_conv_type(heap, ctx, a.value, b.value));
}

static bool type_conversions(Heap *heap) {
  Globals g = add_axiom(heap, "P", level_zero(), add_axiom(heap, "Q", level_zero(), global_initial(heap)));
  Ctx ctx = check_make(heap, g, budget_unlimited(), nat_zero());
  const Value *p = neutral_global(heap, "P");
  const Value *q = neutral_global(heap, "Q");
  Ctx proofs = context_bind(heap, str_lit("second"), QUANTITY_ZERO, p,
                            context_bind(heap, str_lit("first"), QUANTITY_ZERO, p, ctx));
  bool distinct = result_false(conv_conv_type(heap, ctx, p, q));
  bool same = result_true(conv_conv_type(heap, ctx, p, p));
  bool levels = result_false(conv_conv_type(heap, ctx, value_vuniv(heap, level_zero()), value_vuniv(heap, level_one(heap))));
  bool irrelevant =
      result_true(conv_conv(heap, proofs, p, value_var(heap, nat_zero()), value_var(heap, heap_nat_of_u64(heap, 1))));
  bool arrows = type_pair(heap, ctx, eval_eval(heap, g, vec_new(heap->arena), nat_arrow(heap, "left")),
                          eval_eval(heap, g, vec_new(heap->arena), nat_arrow(heap, "right")));
  return distinct && same && levels && irrelevant && arrows;
}

static bool universe_values(Heap *heap) {
  Globals g = add_axiom(heap, "P", level_zero(),
                        add_axiom(heap, "Q", level_zero(),
                                  add_axiom(heap, "R", level_one(heap),
                                            add_axiom(heap, "S", level_one(heap), global_initial(heap)))));
  Ctx ctx = check_make(heap, g, budget_unlimited(), nat_zero());
  const Value *p = neutral_global(heap, "P");
  const Value *q = neutral_global(heap, "Q");
  const Value *r = neutral_global(heap, "R");
  const Value *s = neutral_global(heap, "S");
  const Value *zero = value_vuniv(heap, level_zero());
  const Value *one = value_vuniv(heap, level_one(heap));
  bool pp = result_true(conv_conv(heap, ctx, zero, p, p));
  bool pq = result_false(conv_conv(heap, ctx, zero, p, q));
  bool rr = result_true(conv_conv(heap, ctx, one, r, r));
  bool rs = result_false(conv_conv(heap, ctx, one, r, s));
  return pp && pq && rr && rs;
}

/* verify(ok): True -> IO.print(...); False -> IO.die(1, "FAIL conversion") */
static int verify(bool ok) {
  if (ok)
    return driver_print(str_lit("PASS conversion function eta, universe equality, unequal literals, type distinction, "
                                "proof irrelevance, universe inhabitants"));
  return driver_die(1, "FAIL conversion");
}

static int run(Heap *heap) {
  Globals g = global_initial(heap);
  Ctx ctx = check_make(heap, g, budget_unlimited(), nat_zero());
  bool universes = universe_values(heap);
  bool types = type_conversions(heap);
  bool etas = eta(heap, ctx, eval_eval(heap, g, vec_new(heap->arena), nat_arrow(heap, "x")));
  bool lits = literals(heap, ctx, eval_eval(heap, g, vec_new(heap->arena), prim_nat_ty(heap)));
  const Value *joined = value_vuniv(heap, level_max(heap, level_one(heap), level_zero()));
  bool max = result_true(conv_conv_type(heap, ctx, joined, value_vuniv(heap, level_one(heap))));
  return verify(universes && types && etas && lits && max);
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
