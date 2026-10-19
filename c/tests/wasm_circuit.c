/* C port of tests/wasm_circuit.bend: the circuit depth analysis over kernel terms
 * (primitive, nested and closure applications, folds, cycles, shapes, fields and the literal
 * bound). Each check prints PASS <name> or dies with the expected and observed strings. */
#include "driver.h"

/* nat() = Global{"Nat"} */
static const Term *nat(Heap *heap) { return term_global(heap, str_lit("Nat")); }

/* lit() = Lit{LInt{of_u32(7)}} */
static const Term *lit(Heap *heap) { return term_lit(heap, literal_lint(big_of_u32(heap, 7u))); }

/* SMu{"Tree", []} */
static Shape tree_shape(Heap *heap) { return shape_smu(str_lit("Tree"), vec_new(heap->arena)); }

/* SPi{QMany, "x", nat()} */
static Shape nat_pi(Heap *heap) { return shape_spi(QUANTITY_MANY, str_lit("x"), (void *)nat(heap)); }

/* app(head, arg) = Out{SPi{QMany, "x", nat()}, APt{QMany, arg}, head} */
static const Term *app(Heap *heap, const Term *head, const Term *arg) {
  return term_out(heap, nat_pi(heap), term_addr_pt(QUANTITY_MANY, arg), head);
}

/* add(a, b) = app(app(Global{"natAdd"}, a), b) */
static const Term *add(Heap *heap, const Term *a, const Term *b) {
  return app(heap, app(heap, term_global(heap, str_lit("natAdd")), a), b);
}

/* [Pair{QMany, name}] as a Vec of one Binder pointer */
static Vec one_binder(Heap *heap, const char *name) {
  Vec binders = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, str_lit(name));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  return binders;
}

/* Sec{SPi{QMany, "x", nat()}, [Leg{[Pair{QMany, "x"}], body}]} */
static const Term *one_leg_section(Heap *heap, const Term *body) {
  Vec legs = vec_new(heap->arena);
  Leg *leg = term_leg_new(heap, one_binder(heap, "x"), body);
  if (leg == NULL || !vec_push(&legs, leg)) heap->oom = true;
  return term_sec(heap, nat_pi(heap), legs);
}

/* identity() = Sec{SPi{QMany, "x", nat()}, [Leg{[Pair{QMany, "x"}], Var{0}}]} */
static const Term *identity(Heap *heap) {
  return one_leg_section(heap, term_var(heap, heap_nat_of_u64(heap, 0u)));
}

/* leaf() = In{SMu{"Tree", []}, ACtor{"leaf"}, []} */
static const Term *leaf(Heap *heap) {
  return term_in(heap, tree_shape(heap), term_addr_ctor(str_lit("leaf")), vec_new(heap->arena));
}

/* node(field) = In{SMu{"Tree", []}, ACtor{"node"}, [field]} */
static const Term *node(Heap *heap, const Term *field) {
  Vec args = vec_new(heap->arena);
  if (!vec_push(&args, (void *)field)) heap->oom = true;
  return term_in(heap, tree_shape(heap), term_addr_ctor(str_lit("node")), args);
}

/* tree() = In{SMu{"Tree", []}, ACtor{"node"}, [leaf()]} */
static const Term *tree(Heap *heap) { return node(heap, leaf(heap)); }

/* fold_tree() = Elim{ElimData{SMu{"Tree", []}, tree(), QMany, None, [
 *   Pair{ACtor{"leaf"}, Leg{[], lit()}},
 *   Pair{ACtor{"node"}, Leg{[Pair{QMany, "child"}], add(Var{0}, lit())}}]}} */
static const Term *fold_tree(Heap *heap) {
  Vec branches = vec_new(heap->arena);
  Leg leaf_leg = term_leg(vec_new(heap->arena), lit(heap));
  Leg node_leg = term_leg(one_binder(heap, "child"), add(heap, term_var(heap, heap_nat_of_u64(heap, 0u)), lit(heap)));
  Branch *leaf_branch = term_branch_new(heap, term_addr_ctor(str_lit("leaf")), leaf_leg);
  Branch *node_branch = term_branch_new(heap, term_addr_ctor(str_lit("node")), node_leg);
  if (leaf_branch == NULL || !vec_push(&branches, leaf_branch)) heap->oom = true;
  if (node_branch == NULL || !vec_push(&branches, node_branch)) heap->oom = true;
  return term_elim(heap, term_elim_data(tree_shape(heap), tree(heap), QUANTITY_MANY, term_motive_none(), branches));
}

/* [Pair{name, term}] as a Vec of one CircuitGlobal pointer */
static Vec one_global(Heap *heap, const char *name, const Term *term) {
  Vec globals = vec_new(heap->arena);
  CircuitGlobal *global = circuit_global_new(heap, str_lit(name), term);
  if (global == NULL || !vec_push(&globals, global)) heap->oom = true;
  return globals;
}

/* result(r) = Done{n} -> to_string(n); Fail{e} -> e */
static Str result(Heap *heap, CircuitBigResult r) {
  switch (r.kind) {
    case RESULT_DONE: return big_to_string(heap, r.value);
    case RESULT_FAIL: return r.error;
  }
  return str_lit("");
}

/* result(C.depth(globals, term)) */
static Str depth(Heap *heap, Vec globals, const Term *term) {
  return result(heap, circuit_depth(heap, &globals, term));
}

/* result(C.depth([], term)) */
static Str closed_depth(Heap *heap, const Term *term) { return depth(heap, vec_new(heap->arena), term); }

static Str actual_primitive(Heap *heap) { return closed_depth(heap, add(heap, lit(heap), lit(heap))); }

static Str actual_nested(Heap *heap) {
  return closed_depth(heap, add(heap, add(heap, lit(heap), lit(heap)), lit(heap)));
}

static Str actual_closure(Heap *heap) {
  return closed_depth(heap, app(heap, identity(heap), add(heap, add(heap, lit(heap), lit(heap)), lit(heap))));
}

static Str actual_unapplied(Heap *heap) { return closed_depth(heap, identity(heap)); }

static Str actual_fold(Heap *heap) { return closed_depth(heap, fold_tree(heap)); }

static Str actual_unknown(Heap *heap) { return closed_depth(heap, term_global(heap, str_lit("missing"))); }

/* globals [Pair{"loop", Global{"loop"}}], term Global{"loop"} */
static Str actual_cycle(Heap *heap) {
  const Term *loop = term_global(heap, str_lit("loop"));
  return depth(heap, one_global(heap, "loop", loop), loop);
}

/* Lan{SPar{nat(), nat()}, nat()} */
static Str actual_shape(Heap *heap) {
  Shape pair = shape_spar((void *)nat(heap), (void *)nat(heap));
  return closed_depth(heap, term_lan(heap, pair, nat(heap)));
}

static Str actual_opaque(Heap *heap) { return closed_depth(heap, app(heap, lit(heap), lit(heap))); }

static Str actual_type(Heap *heap) { return closed_depth(heap, term_univ(heap, level_var_lzero())); }

/* globals [Pair{"f", Sec{SPi{QMany, "x", nat()}, [Leg{[Pair{QMany, "x"}], Global{"f"}}]}}], term Global{"f"} */
static Str actual_captured_cycle(Heap *heap) {
  const Term *f = term_global(heap, str_lit("f"));
  return depth(heap, one_global(heap, "f", one_leg_section(heap, f)), f);
}

/* Let{"x", nat(), lit(), In{SMu{"Tree", []}, ACtor{"node"}, [Var{0}]}} */
static Str actual_ordinary_field(Heap *heap) {
  const Term *body = node(heap, term_var(heap, heap_nat_of_u64(heap, 0u)));
  return closed_depth(heap, term_let(heap, str_lit("x"), nat(heap), lit(heap), body));
}

/* In{SMu{"Tree", []}, ACtor{"node"}, [Lan{SMu{"Tree", []}, nat()}]} */
static Str actual_uncertified_recursive_field(Heap *heap) {
  return closed_depth(heap, node(heap, term_lan(heap, tree_shape(heap), nat(heap))));
}

/* result(C.bounded(Big{False, [0, 0, 0, 0, 4]})) */
static Str actual_overflow(Heap *heap) {
  static const uint32_t limbs[5] = {0u, 0u, 0u, 0u, 4u};
  Limbs digits = {limbs, 5u};
  Big n = {false, digits};
  return result(heap, circuit_bounded(n));
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

typedef Str (*Actual)(Heap *heap);

typedef struct {
  const char *name;
  const char *expected;
  Actual actual;
} Check;

/* The 14 checks of main, in order; the first failing check ends the run. */
static const Check CHECKS[] = {
    {"primitive", "1", actual_primitive},
    {"nested", "2", actual_nested},
    {"closure", "2", actual_closure},
    {"unapplied", "0", actual_unapplied},
    {"fold", "2", actual_fold},
    {"unknown", "unknown global missing", actual_unknown},
    {"cycle", "mu", actual_cycle},
    {"shape", "SPar", actual_shape},
    {"opaque", "opaque callee", actual_opaque},
    {"type", "0", actual_type},
    {"captured cycle", "mu", actual_captured_cycle},
    {"ordinary field", "0", actual_ordinary_field},
    {"uncertified recursive field", "mu", actual_uncertified_recursive_field},
    {"overflow", "unbounded iteration", actual_overflow},
};

static int run(Heap *heap) {
  size_t count = sizeof CHECKS / sizeof CHECKS[0];
  size_t i = 0;
  int code = 0;
  for (; i < count && code == 0 && !heap->oom; i += 1) {
    code = check(heap, CHECKS[i].name, CHECKS[i].expected, CHECKS[i].actual(heap));
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
