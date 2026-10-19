/* C port of tests/wasm_circuit_bounds.bend: the circuit depth bounds over folds, aliases,
 * cycles, foreign payloads, collection fields and the 63-bit machine-integer depth limit. Each of
 * the 18 cases prints PASS <name> or FAIL <name>; the run ends with CIRCUIT-BOUNDS <passed>/<total>
 * and exits 1 (Out.cli_exit) unless all 18 pass. */
#include "cli.h"
#include "driver.h"

#include <stdio.h>

/* shape() = SMu{"Bound", []} */
static Shape shape(Heap *heap) { return shape_smu(str_lit("Bound"), vec_new(heap->arena)); }

/* ctor(args) = In{shape(), ACtor{"node"}, args} */
static const Term *ctor(Heap *heap, Vec args) {
  return term_in(heap, shape(heap), term_addr_ctor(str_lit("node")), args);
}

/* [a] and [a, b] as Vecs of term pointers */
static Vec one(Heap *heap, const Term *a) {
  Vec args = vec_new(heap->arena);
  if (!vec_push(&args, (void *)a)) heap->oom = true;
  return args;
}

static Vec two(Heap *heap, const Term *a, const Term *b) {
  Vec args = one(heap, a);
  if (!vec_push(&args, (void *)b)) heap->oom = true;
  return args;
}

/* leaf() = ctor([]) */
static const Term *leaf(Heap *heap) { return ctor(heap, vec_new(heap->arena)); }

/* three() = ctor([ctor([leaf()])]) */
static const Term *three(Heap *heap) { return ctor(heap, one(heap, ctor(heap, one(heap, leaf(heap))))); }

/* zero() = Lit{LInt{B.zero()}} */
static const Term *zero(Heap *heap) { return term_lit(heap, literal_lint(big_zero())); }

/* other(args) = In{SMu{"Other", []}, ACtor{"o"}, args} */
static const Term *other(Heap *heap, Vec args) {
  return term_in(heap, shape_smu(str_lit("Other"), vec_new(heap->arena)), term_addr_ctor(str_lit("o")), args);
}

/* pair(a, b) = In{S.coll(2), T.aleg(0), [a, b]} */
static const Term *pair(Heap *heap, const Term *a, const Term *b) {
  return term_in(heap, shape_coll(heap, heap_nat_of_u64(heap, 2u)), term_aleg(heap, heap_nat_of_u64(heap, 0u)), two(heap, a, b));
}

static const Term *global(Heap *heap, const char *name) { return term_global(heap, str_lit(name)); }

static void push_global(Heap *heap, Vec *rows, const char *name, const Term *term) {
  CircuitGlobal *row = circuit_global_new(heap, str_lit(name), term);
  if (row == NULL || !vec_push(rows, row)) heap->oom = true;
}

/* globals() = [alias = Global{"tree"}, tree = three(), cycleA = Global{"cycleB"},
 *              cycleB = ctor([Global{"cycleA"}])] */
static Vec globals(Heap *heap) {
  Vec rows = vec_new(heap->arena);
  push_global(heap, &rows, "alias", global(heap, "tree"));
  push_global(heap, &rows, "tree", three(heap));
  push_global(heap, &rows, "cycleA", global(heap, "cycleB"));
  push_global(heap, &rows, "cycleB", ctor(heap, one(heap, global(heap, "cycleA"))));
  return rows;
}

/* Elim{ElimData{eshape, scrut, QMany, None, [Pair{addr, Leg{binders, body}}]}} */
static const Term *one_branch(Heap *heap, Shape eshape, const Term *scrut, Addr addr, Vec binders, const Term *body) {
  Vec branches = vec_new(heap->arena);
  Branch *branch = term_branch_new(heap, addr, term_leg(binders, body));
  if (branch == NULL || !vec_push(&branches, branch)) heap->oom = true;
  return term_elim(heap, term_elim_data(eshape, scrut, QUANTITY_MANY, term_motive_none(), branches));
}

/* branch(scrut, body): one node leg with no binders */
static const Term *branch(Heap *heap, const Term *scrut, const Term *body) {
  return one_branch(heap, shape(heap), scrut, term_addr_ctor(str_lit("node")), vec_new(heap->arena), body);
}

/* fold(scrut, body): one node leg binding [Pair{QMany, "x"}] with body pair(Var{0}, body) */
static const Term *fold(Heap *heap, const Term *scrut, const Term *body) {
  Vec binders = vec_new(heap->arena);
  Binder *binder = term_binder_new(heap, QUANTITY_MANY, str_lit("x"));
  if (binder == NULL || !vec_push(&binders, binder)) heap->oom = true;
  const Term *leg_body = pair(heap, term_var(heap, heap_nat_of_u64(heap, 0u)), body);
  return one_branch(heap, shape(heap), scrut, term_addr_ctor(str_lit("node")), binders, leg_body);
}

/* scaled(0) = Global{"natMul"}; scaled(1+p) = fold(three(), scaled(p)) */
static const Term *scaled(Heap *heap, uint32_t n) {
  if (n == 0u) return global(heap, "natMul");
  return fold(heap, three(heap), scaled(heap, n - 1u));
}

/* scaled_once(0) = Global{"natMul"}; scaled_once(1+p) = branch(three(), scaled_once(p)) */
static const Term *scaled_once(Heap *heap, uint32_t n) {
  if (n == 0u) return global(heap, "natMul");
  return branch(heap, three(heap), scaled_once(heap, n - 1u));
}

/* apply(head, arg) = Out{SPi{QMany, "x", Global{"Nat"}}, APt{QMany, arg}, head} */
static const Term *apply(Heap *heap, const Term *head, const Term *arg) {
  Shape pi = shape_spi(QUANTITY_MANY, str_lit("x"), (void *)global(heap, "Nat"));
  return term_out(heap, pi, term_addr_pt(QUANTITY_MANY, arg), head);
}

/* increment(body) = apply(apply(Global{"natMul"}, body), zero()) */
static const Term *increment(Heap *heap, const Term *body) {
  return apply(heap, apply(heap, global(heap, "natMul"), body), zero(heap));
}

/* near_limit(0) = Global{"natMul"}; near_limit(1+p) = increment(fold(ctor([leaf()]), near_limit(p))) */
static const Term *near_limit(Heap *heap, uint32_t n) {
  if (n == 0u) return global(heap, "natMul");
  return increment(heap, fold(heap, ctor(heap, one(heap, leaf(heap))), near_limit(heap, n - 1u)));
}

/* Preserve the source oracle's 63-bit machine-integer depth bound. */
static const Term *limit(Heap *heap) { return near_limit(heap, 61u); }

/* case_limit() = Elim{ElimData{S.coll(1), zero(), QMany, None, [Pair{T.aleg(0), Leg{[], limit()}}]}} */
static const Term *case_limit(Heap *heap) {
  Shape coll = shape_coll(heap, heap_nat_of_u64(heap, 1u));
  return one_branch(heap, coll, zero(heap), term_aleg(heap, heap_nat_of_u64(heap, 0u)), vec_new(heap->arena), limit(heap));
}

/* good(n) = Done{B.of_u32(n)} */
static CircuitBigResult good(Heap *heap, uint32_t n) {
  CircuitBigResult r = {RESULT_DONE, big_of_u32(heap, n), str_lit("")};
  return r;
}

static CircuitBigResult fail(const char *message) {
  CircuitBigResult r = {RESULT_FAIL, big_zero(), str_lit(message)};
  return r;
}

/* Done{B.Big{False, [32767, 32767, 32767, 32767, 3]}} */
static CircuitBigResult integer_limit(void) {
  static const uint32_t limbs[5] = {32767u, 32767u, 32767u, 32767u, 3u};
  Limbs digits = {limbs, 5u};
  Big n = {false, digits};
  CircuitBigResult r = {RESULT_DONE, n, str_lit("")};
  return r;
}

/* Case{name, term, expected} */
typedef struct {
  Str name;
  const Term *term;
  CircuitBigResult expected;
} Case;

static void push_case(Heap *heap, Vec *rows, const char *name, const Term *term, CircuitBigResult expected) {
  Case *row = heap_alloc(heap, sizeof(Case));
  if (row == NULL) {
    heap->oom = true;
    return;
  }
  row->name = str_lit(name);
  row->term = term;
  row->expected = expected;
  if (!vec_push(rows, row)) heap->oom = true;
}

/* cases(): the 18 rows in Bend order */
static Vec cases(Heap *heap) {
  Vec rows = vec_new(heap->arena);
  const Term *mul = global(heap, "natMul");
  push_case(heap, &rows, "alias", fold(heap, global(heap, "alias"), mul), good(heap, 3u));
  push_case(heap, &rows, "alias-once", branch(heap, global(heap, "alias"), mul), good(heap, 1u));
  push_case(heap, &rows, "leaf-work", branch(heap, leaf(heap), mul), good(heap, 1u));
  push_case(heap, &rows, "literal-field", fold(heap, ctor(heap, one(heap, zero(heap))), mul), good(heap, 1u));
  push_case(heap, &rows, "literal-scrutinee", branch(heap, zero(heap), mul), fail("unbounded iteration"));
  const Term *payload = other(heap, one(heap, other(heap, one(heap, other(heap, vec_new(heap->arena))))));
  push_case(heap, &rows, "foreign-payload", fold(heap, ctor(heap, one(heap, payload)), mul), good(heap, 1u));
  push_case(heap, &rows, "opaque-sibling",
            branch(heap, ctor(heap, two(heap, leaf(heap), term_var(heap, heap_nat_of_u64(heap, 0u)))), zero(heap)),
            fail("unbounded iteration"));
  push_case(heap, &rows, "coll-field-value", ctor(heap, one(heap, pair(heap, zero(heap), zero(heap)))), good(heap, 0u));
  push_case(heap, &rows, "recursive-field-uncertified",
            ctor(heap, one(heap, ctor(heap, one(heap, pair(heap, zero(heap), zero(heap)))))), fail("mu"));
  push_case(heap, &rows, "unknown-field-global", ctor(heap, one(heap, global(heap, "nosuch"))),
            fail("unknown global nosuch"));
  push_case(heap, &rows, "cycle-bound", branch(heap, global(heap, "cycleA"), mul), fail("unbounded iteration"));
  push_case(heap, &rows, "cycle-value", global(heap, "cycleA"), fail("mu"));
  push_case(heap, &rows, "representable-depth", scaled(heap, 10u), good(heap, 59049u));
  push_case(heap, &rows, "no-fold-depth", scaled_once(heap, 10u), good(heap, 1u));
  push_case(heap, &rows, "overflow", scaled(heap, 63u), fail("unbounded iteration"));
  push_case(heap, &rows, "integer-limit", limit(heap), integer_limit());
  push_case(heap, &rows, "primitive-overflow", increment(heap, limit(heap)), fail("unbounded iteration"));
  push_case(heap, &rows, "case-overflow", case_limit(heap), fail("unbounded iteration"));
  return rows;
}

/* same(a, b): Done/Done by B.to_string, Fail/Fail by String.eq, otherwise False */
static bool same(Heap *heap, CircuitBigResult a, CircuitBigResult b) {
  switch (a.kind) {
    case RESULT_DONE:
      if (b.kind == RESULT_DONE) return str_eq(big_to_string(heap, a.value), big_to_string(heap, b.value));
      return false;
    case RESULT_FAIL:
      if (b.kind == RESULT_FAIL) return str_eq(a.error, b.error);
      return false;
  }
  return false;
}

/* report(ok, name): print PASS name (1) or FAIL name (0) */
static size_t report(Heap *heap, bool ok, Str name) {
  if (ok) {
    driver_print(heap_str_cat(heap, str_lit("PASS "), name));
    return 1u;
  }
  driver_print(heap_str_cat(heap, str_lit("FAIL "), name));
  return 0u;
}

/* finish(ok) = pure Unit, or Out.cli_exit(1) */
static int finish(bool ok) {
  if (ok) return 0;
  return cli_exit(1u);
}

/* run(rows, passed, total) */
static int run(Heap *heap, const Vec *rows) {
  Vec scope = globals(heap);
  size_t passed = 0u;
  size_t total = 0u;
  size_t i = 0u;
  for (; i < rows->len && !heap->oom; i += 1u) {
    VecGet got = vec_get(rows, i);
    if (got.status != VEC_GET_OK) return driver_host_failure();
    const Case *row = got.value;
    passed += report(heap, same(heap, circuit_depth(heap, &scope, row->term), row->expected), row->name);
    total += 1u;
  }
  if (heap->oom) return driver_host_failure();
  char line[64];
  snprintf(line, sizeof line, "CIRCUIT-BOUNDS %zu/%zu", passed, total);
  driver_print(str_lit(line));
  return finish(passed == 18u && total == 18u);
}

/* arguments(args) = run(cases(), 0, 0); the arguments are ignored */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Vec rows = cases(&heap);
  int code = heap.oom ? driver_host_failure() : run(&heap, &rows);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
