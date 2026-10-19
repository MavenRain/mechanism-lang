/* Port of tests/wasm_order.bend (oracle label unit-wasm-order). */
#include "driver.h"

/* literal() = T.Lit{L.LInt{B.of_u32(0)}} */
static const Term *literal(Heap *heap) { return term_lit(heap, literal_lint(big_of_u32(heap, 0))); }

/* nat() = T.Global{"Nat"} */
static const Term *nat(Heap *heap) { return term_global(heap, str_lit("Nat")); }

/* lam(name, body) = T.Sec{S.SPi{QMany, "x", nat()}, [T.Leg{[C.Pair{QMany, name}], body}]} */
static const Term *lam(Heap *heap, const char *name, const Term *body) {
  Vec binders = vec_new(heap->arena);
  Vec legs = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, str_lit(name));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  Leg *leg = term_leg_new(heap, binders, body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, shape_spi(QUANTITY_MANY, str_lit("x"), (void *)nat(heap)), legs);
}

/* app(head, arg) = T.Out{S.SPi{QMany, "x", nat()}, T.APt{QMany, arg}, head} */
static const Term *app(Heap *heap, const Term *head, const Term *arg) {
  return term_out(heap, shape_spi(QUANTITY_MANY, str_lit("x"), (void *)nat(heap)), term_addr_pt(QUANTITY_MANY, arg),
                  head);
}

/* O.choose(motive, Some{T.Motive{None{}, [], "self", nat()}}, None{}) */
static MaybeMotive motive_of(Heap *heap, bool motive) {
  MaybeStr none;
  none.some = false;
  none.value = str_empty();
  if (motive) return term_motive_some(term_motive(none, vec_new(heap->arena), str_lit("self"), nat(heap)));
  return term_motive_none();
}

/* elim(scrut, body, motive) = T.Elim{T.ElimData{S.SMu{"Tree", []}, scrut, QMany, motive,
 *   [C.Pair{T.ACtor{"leaf"}, T.Leg{[], literal()}}, C.Pair{T.ACtor{"node"}, T.Leg{[C.Pair{QMany, "child"}], body}}]}} */
static const Term *elim(Heap *heap, const Term *scrut, const Term *body, bool motive) {
  Vec branches = vec_new(heap->arena);
  Vec child = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, str_lit("child"));
  if (binder == NULL || !vec_push(&child, binder)) heap->oom = true;
  Branch *leaf = term_branch_new(heap, term_addr_ctor(str_lit("leaf")), term_leg(vec_new(heap->arena), literal(heap)));
  Branch *node = term_branch_new(heap, term_addr_ctor(str_lit("node")), term_leg(child, body));
  if (leaf == NULL || !vec_push(&branches, leaf)) heap->oom = true;
  if (node == NULL || !vec_push(&branches, node)) heap->oom = true;
  Shape shape = shape_smu(str_lit("Tree"), vec_new(heap->arena));
  return term_elim(heap, term_elim_data(shape, scrut, QUANTITY_MANY, motive_of(heap, motive), branches));
}

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, heap_nat_of_u64(heap, index)); }

/* recursive(name, index) = lam("x", elim(T.Var{0n}, app(T.Global{name}, T.Var{index}), True{})) */
static const Term *recursive(Heap *heap, const char *name, uint64_t index) {
  return lam(heap, "x", elim(heap, var(heap, 0), app(heap, term_global(heap, str_lit(name)), var(heap, index)), true));
}

/* f2() = lam("x", lam("y", elim(T.Var{1n}, app(app(T.Global{"g"}, T.Var{0n}), T.Var{1n}), True{}))) */
static const Term *f2(Heap *heap) {
  const Term *body = app(heap, app(heap, term_global(heap, str_lit("g")), var(heap, 0)), var(heap, 1));
  return lam(heap, "x", lam(heap, "y", elim(heap, var(heap, 1), body, true)));
}

/* g2() = lam("x", lam("y", elim(T.Var{0n}, app(app(T.Global{"f"}, T.Var{2n}), T.Var{0n}), True{}))) */
static const Term *g2(Heap *heap) {
  const Term *body = app(heap, app(heap, term_global(heap, str_lit("f")), var(heap, 2)), var(heap, 0));
  return lam(heap, "x", lam(heap, "y", elim(heap, var(heap, 0), body, true)));
}

/* One member of a group: C.Pair{name, body}. */
static Vec group1(Heap *heap, const char *name, const Term *body) {
  Vec members = vec_new(heap->arena);
  OrderMember *m = order_member_new(heap, str_lit(name), body);
  if (m == NULL || !vec_push(&members, m)) heap->oom = true;
  return members;
}

static Vec group2(Heap *heap, const char *name1, const Term *body1, const char *name2, const Term *body2) {
  Vec members = group1(heap, name1, body1);
  OrderMember *m = order_member_new(heap, str_lit(name2), body2);
  if (m == NULL || !vec_push(&members, m)) heap->oom = true;
  return members;
}

/* certificate(r): Fail -> Err.to_string(e); Done{None} -> "none"; Done{Some{c}} -> Nat.show(O.argument(c)) */
static Str certificate(Heap *heap, OrderCertResult r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, r.error);
    case RESULT_DONE:
      if (r.value.some) return heap_nat_show(heap, order_argument(r.value.value));
      return str_lit("none");
  }
  return str_lit("none");
}

/* guarded(r): Fail -> Err.to_string(e); Done{None} -> "none"; Done{Some{k}} -> Nat.show(k) */
static Str guarded(Heap *heap, MaybeNatResult r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, r.error);
    case RESULT_DONE:
      if (r.value.some) return heap_nat_show(heap, r.value.value);
      return str_lit("none");
  }
  return str_lit("none");
}

/* translated_result(r): Fail -> Err.to_string(e); Done{t} -> "ok" */
static Str translated_result(Heap *heap, OrderTermResult r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, r.error);
    case RESULT_DONE:
      return str_lit("ok");
  }
  return str_lit("ok");
}

/* translated(c, body): Fail -> Err.to_string(e); Done{None} -> "none"; Done{Some{cert}} -> translated_result(O.translate(cert, body)) */
static Str translated(Heap *heap, OrderCertResult c, const Term *body) {
  switch (c.kind) {
    case RESULT_FAIL:
      return error_to_string(heap, c.error);
    case RESULT_DONE:
      if (c.value.some) return translated_result(heap, order_translate(heap, c.value.value, body));
      return str_lit("none");
  }
  return str_lit("none");
}

/* report(ok, name, expected, actual): PASS name, or die 1 with "name: expected E, observed A" */
static int report(Heap *heap, bool ok, Str name, Str expected, Str actual) {
  if (ok) return driver_print(heap_str_cat(heap, str_lit("PASS "), name));
  Str message = heap_str_cat(heap, heap_str_cat(heap, name, str_lit(": expected ")), expected);
  Str full = heap_str_cat(heap, heap_str_cat(heap, message, str_lit(", observed ")), actual);
  const char *cstr = str_cstr(heap->arena, full);
  if (cstr == NULL) return driver_host_failure();
  return driver_die(1, cstr);
}

/* check(name, expected, actual) = report(String.eq(expected, actual), name, expected, actual) */
static int check(Heap *heap, const char *name, const char *expected, Str actual) {
  Str want = str_lit(expected);
  return report(heap, str_eq(want, actual), str_lit(name), want, actual);
}

static Str guard_of(Heap *heap, uint64_t polls, const Term *body) {
  Budget budget = budget_of_polls(heap_nat_of_u64(heap, polls));
  return guarded(heap, totality_guard(heap, budget, global_initial(heap), str_lit("f"), nat(heap), body));
}

static const Term *nested_let(Heap *heap) {
  return term_let(heap, str_lit("x"), literal(heap), literal(heap), literal(heap));
}

static const char *const GUARD_MSG = "termination: recursive definition f failed the structural termination guard";
static const char *const BUDGET_MSG = "budget: the totality budget is exhausted";
static const char *const MOTIVE_MSG =
    "missing branch: the elimination of a structurally recursive definition carries its motive";

typedef Str (*Actual)(Heap *heap);

typedef struct {
  const char *name;
  const char *expected;
  Actual actual;
} Check;

static Str a_structural(Heap *heap) {
  Vec g = group1(heap, "f", recursive(heap, "f", 0));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_nondecreasing(Heap *heap) {
  Vec g = group1(heap, "f", recursive(heap, "f", 1));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_bare_self(Heap *heap) {
  Vec g = group1(heap, "f", lam(heap, "x", term_global(heap, str_lit("f"))));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_nonrecursive(Heap *heap) {
  Vec g = group1(heap, "f", lam(heap, "x", literal(heap)));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_mutual(Heap *heap) {
  Vec g = group2(heap, "f", recursive(heap, "g", 0), "g", recursive(heap, "f", 0));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_inconsistent(Heap *heap) {
  Vec g = group2(heap, "f", f2(heap), "g", g2(heap));
  return certificate(heap, order_certify(heap, &g));
}
static Str a_translation(Heap *heap) {
  Vec g = group1(heap, "f", recursive(heap, "f", 0));
  return translated(heap, order_certify(heap, &g), recursive(heap, "f", 0));
}
static Str a_missing_motive(Heap *heap) {
  Vec g = group1(heap, "f", recursive(heap, "f", 0));
  const Term *body = lam(heap, "x", elim(heap, var(heap, 0), app(heap, term_global(heap, str_lit("f")), var(heap, 0)), false));
  return translated(heap, order_certify(heap, &g), body);
}
static Str a_budget_zero(Heap *heap) { return guard_of(heap, 0, literal(heap)); }
static Str a_budget_one(Heap *heap) { return guard_of(heap, 1, literal(heap)); }
static Str a_budget_nested_fail(Heap *heap) { return guard_of(heap, 3, nested_let(heap)); }
static Str a_budget_nested_exact(Heap *heap) { return guard_of(heap, 4, nested_let(heap)); }

/* The 12 checks of main, in order; the first failing check ends the run. */
static const Check CHECKS[] = {
    {"structural", "0", a_structural},
    {"nondecreasing", NULL, a_nondecreasing},
    {"bare self", NULL, a_bare_self},
    {"nonrecursive", "none", a_nonrecursive},
    {"mutual", "0", a_mutual},
    {"inconsistent positions", NULL, a_inconsistent},
    {"translation", "ok", a_translation},
    {"missing motive", NULL, a_missing_motive},
    {"budget zero", NULL, a_budget_zero},
    {"budget one", "none", a_budget_one},
    {"budget nested fail", NULL, a_budget_nested_fail},
    {"budget nested exact", "none", a_budget_nested_exact},
};

/* The long expected texts, by check index (NULL rows above). */
static const char *expected_of(size_t i, const char *expected) {
  if (expected != NULL) return expected;
  if (i == 1 || i == 2 || i == 5) return GUARD_MSG;
  if (i == 7) return MOTIVE_MSG;
  return BUDGET_MSG;
}

static int run(Heap *heap) {
  size_t count = sizeof CHECKS / sizeof CHECKS[0];
  size_t i = 0;
  int code = 0;
  for (; i < count && code == 0 && !heap->oom; i += 1) {
    code = check(heap, CHECKS[i].name, expected_of(i, CHECKS[i].expected), CHECKS[i].actual(heap));
  }
  return code;
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
