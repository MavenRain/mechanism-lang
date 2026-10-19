/* C port of tests/kernel_eval.bend: evaluator beta reduction, the natAdd primitive and the
 * read back of a closure that captured a let-bound value. */
#include "driver.h"

/* app(head, arg) = Out{SPi{QMany, "x", nat_ty}, APt{QMany, arg}, head} */
static const Term *app(Heap *heap, const Term *head, const Term *arg) {
  Shape shape = shape_spi(QUANTITY_MANY, str_lit("x"), (void *)prim_nat_ty(heap));
  return term_out(heap, shape, term_addr_pt(QUANTITY_MANY, arg), head);
}

/* nat(n) = Lit{LInt{of_u32(n)}} */
static const Term *nat(Heap *heap, uint32_t n) {
  return term_lit(heap, literal_lint(big_of_u32(heap, n)));
}

/* check_nat: Done{VLit{LInt{got}}} with got = want */
static bool check_nat(Heap *heap, Result result, uint32_t want) {
  switch (result.kind) {
    case RESULT_DONE: {
      const Value *value = result.value;
      if (value->kind != VALUE_LIT) return false;
      if (value->lit.kind != LITERAL_INT) return false;
      return big_equal(value->lit.integer, big_of_u32(heap, want));
    }
    case RESULT_FAIL: return false;
  }
  return false;
}

/* [Pair{QMany, "x"}] as a Vec of one Binder pointer */
static Vec one_binder(Heap *heap) {
  Vec binders = vec_new(heap->arena);
  Binder *x = term_binder_new(heap, QUANTITY_MANY, str_lit("x"));
  if (x == NULL || !vec_push(&binders, x)) heap->oom = true;
  return binders;
}

/* Sec{SPi{QMany, "x", nat_ty}, [Leg{[Pair{QMany, "x"}], body}]} */
static const Term *one_leg_section(Heap *heap, const Term *body) {
  Shape shape = shape_spi(QUANTITY_MANY, str_lit("x"), (void *)prim_nat_ty(heap));
  Vec legs = vec_new(heap->arena);
  Leg *leg = term_leg_new(heap, one_binder(heap), body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, shape, legs);
}

/* identity() = Sec{SPi{QMany, "x", nat_ty}, [Leg{[Pair{QMany, "x"}], Var{0}}]} */
static const Term *identity(Heap *heap) {
  return one_leg_section(heap, term_var(heap, heap_nat_of_u64(heap, 0u)));
}

/* captured() = Let{"outer", nat_ty, nat(41), Sec{..., [Leg{[Pair{QMany, "x"}], Var{1}}]}} */
static const Term *captured(Heap *heap) {
  const Term *section = one_leg_section(heap, term_var(heap, heap_nat_of_u64(heap, 1u)));
  return term_let(heap, str_lit("outer"), prim_nat_ty(heap), nat(heap, 41u), section);
}

/* check_capture: Done{Sec{s, [Leg{bs, Lit{LInt{got}}}]}} with got = 41 */
static bool check_capture(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_DONE: {
      const Term *term = result.value;
      if (term->kind != TERM_SEC) return false;
      if (term->legs.len != 1u) return false;
      VecGet got = vec_get(&term->legs, 0u);
      switch (got.status) {
        case VEC_GET_OK: {
          const Leg *leg = got.value;
          if (leg->body->kind != TERM_LIT) return false;
          if (leg->body->lit.kind != LITERAL_INT) return false;
          return big_equal(leg->body->lit.integer, big_of_u32(heap, 41u));
        }
        case VEC_GET_OUT_OF_RANGE: return false;
      }
      return false;
    }
    case RESULT_FAIL: return false;
  }
  return false;
}

/* readback: Done{value} -> check_capture(quote(globals, 0, value)); Fail -> False */
static bool readback(Heap *heap, Result result, Globals globals) {
  switch (result.kind) {
    case RESULT_DONE:
      return check_capture(heap, eval_quote(heap, globals, nat_zero(), result.value));
    case RESULT_FAIL: return false;
  }
  return false;
}

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS evaluator beta, primitives, closure readback"));
  return driver_die(1, "FAIL evaluator beta, primitives, closure readback");
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Globals globals = global_initial(&heap);
  Vec empty = vec_new(heap.arena);
  const Term *nat_add = term_global(&heap, str_lit("natAdd"));
  bool ok = check_nat(&heap, eval_eval(&heap, globals, empty, app(&heap, identity(&heap), nat(&heap, 42u))), 42u) &&
            check_nat(&heap, eval_eval(&heap, globals, empty, app(&heap, app(&heap, nat_add, nat(&heap, 17u)), nat(&heap, 25u))), 42u) &&
            check_nat(&heap, eval_eval(&heap, globals, empty, app(&heap, captured(&heap), nat(&heap, 99u))), 41u) &&
            readback(&heap, eval_eval(&heap, globals, empty, captured(&heap)), globals);
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
