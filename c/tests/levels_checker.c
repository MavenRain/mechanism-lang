/* Port of tests/levels_checker.bend (oracle label unit-levels-checker). */
#include "driver.h"

/* success(result): Done -> True. */
static bool success(Result result) { return result.kind == RESULT_DONE; }

/* scope(result): Fail{Universe{msg}} with the global parameter scope message. */
static bool scope(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_UNIVERSE &&
         str_eq(result.error.message, str_lit("universe level is outside the global parameter scope"));
}

/* mismatch(result): Fail{Mismatch} -> True. */
static bool mismatch(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_MISMATCH; }

/* budget_error(result): Fail{Budget_exhausted} -> True. */
static bool budget_error(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED; }

/* universe(result): Done{VUniv{level}} -> L.equal(level, L.zero()). */
static bool universe(Heap *heap, Result result) {
  if (result.kind != RESULT_DONE) return false;
  const Value *value = result.value;
  return value != NULL && value->kind == VALUE_UNIV && level_equal(heap, value->level, level_zero());
}

/* Check.Decl{name, Definition, ty, Some{body}} */
static Decl decl_of(const char *name, const Term *ty, const Term *body) {
  Decl d;
  d.name = str_lit(name);
  d.kind = DECL_DEFINITION;
  d.ty = ty;
  d.body.some = true;
  d.body.value = body;
  return d;
}

/* definition(name, level) = Decl{name, Definition, Univ{succ level}, Some{Univ{level}}} */
static Decl definition(Heap *heap, const char *name, const LevelVar *level) {
  return decl_of(name, term_univ(heap, level_succ(heap, level)), term_univ(heap, level));
}

/* distributed() = Decl{"distribution", Definition, Univ{succ(max(imax(u,v), imax(u,w)))}, Some{Univ{imax(u, max(v,w))}}} */
static Decl distributed(Heap *heap) {
  const LevelVar *u = level_var(heap, nat_zero());
  const LevelVar *v = level_var(heap, heap_nat_of_u64(heap, 1));
  const LevelVar *w = level_var(heap, heap_nat_of_u64(heap, 2));
  const LevelVar *ty = level_succ(heap, level_max(heap, level_imax(heap, u, v), level_imax(heap, u, w)));
  return decl_of("distribution", term_univ(heap, ty), term_univ(heap, level_imax(heap, u, level_max(heap, v, w))));
}

/* open_universe() = T.Univ{L.var(0n)} */
static const Term *open_universe(Heap *heap) { return term_univ(heap, level_var(heap, nat_zero())); }

static MaybeStr no_ind(void) {
  MaybeStr none;
  none.some = false;
  none.value = str_empty();
  return none;
}

static Vec push_one(Heap *heap, void *item) {
  Vec v = vec_new(heap->arena);
  if (item == NULL || !vec_push(&v, item)) heap->oom = true;
  return v;
}

/* T.Sec{S.SPi{Q.QMany{}, "x", u}, [T.Leg{[Pair{Q.QMany{}, "x"}], T.Var{0n}}]} */
static const Term *sec_many(Heap *heap, Shape shape) {
  Vec binders = push_one(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("x")));
  Vec legs = push_one(heap, term_leg_new(heap, binders, term_var(heap, nat_zero())));
  return term_sec(heap, shape, legs);
}

enum { RAW_SCOPE_COUNT = 10 };

/* raw_scope_terms(): the 10 raw terms whose fields carry the open universe u. */
static void raw_scope_terms(Heap *heap, const Term *terms[RAW_SCOPE_COUNT]) {
  const Term *u = open_universe(heap);
  Shape pi_u = shape_spi(QUANTITY_MANY, str_lit("x"), (void *)u);
  Addr leg0 = term_addr_leg(big_of_nat(heap, nat_zero()));
  const Term *nat = term_global(heap, str_lit("Nat"));
  terms[0] = u;
  terms[1] = term_univ(heap, level_imax(heap, level_var(heap, nat_zero()), level_zero()));
  terms[2] = term_ann(heap, term_univ(heap, level_zero()), u);
  terms[3] = sec_many(heap, pi_u);
  terms[4] = term_in(heap, pi_u, leg0, vec_new(heap->arena));
  terms[5] = term_out(heap, pi_u, leg0, nat);
  terms[6] = term_in(heap, shape_scoll(big_of_nat(heap, heap_nat_of_u64(heap, 1))), term_addr_pt(QUANTITY_ZERO, u), vec_new(heap->arena));
  Motive motive = term_motive(no_ind(), vec_new(heap->arena), str_lit("self"), u);
  terms[7] = term_elim(heap, term_elim_data(shape_scoll(big_of_nat(heap, nat_zero())), nat, QUANTITY_ZERO, term_motive_some(motive),
                                            vec_new(heap->arena)));
  Vec branches = push_one(heap, term_branch_new(heap, leg0, term_leg(vec_new(heap->arena), u)));
  terms[8] = term_elim(heap, term_elim_data(shape_scoll(big_of_nat(heap, heap_nat_of_u64(heap, 1))), nat, QUANTITY_ZERO,
                                            term_motive_none(), branches));
  terms[9] = term_let(heap, str_lit("x"), term_univ(heap, level_zero()), u, term_var(heap, nat_zero()));
}

/* all_scoped(terms): Bool.and is strict, both checks run for every term. */
static bool all_scoped(Heap *heap, Globals g, const Term *const terms[RAW_SCOPE_COUNT], size_t index) {
  if (index >= RAW_SCOPE_COUNT) return true;
  bool inferred = scope(check_infer_term(heap, g, budget_unlimited(), terms[index]));
  bool checked = scope(check_check_term(heap, g, budget_unlimited(), terms[index], value_vuniv(heap, level_one(heap))));
  bool rest = all_scoped(heap, g, terms, index + 1);
  return inferred && checked && rest;
}

/* Check.FamilyDecl{name, params, [], L.one()} */
static FamilyDecl family_decl(Heap *heap, Vec params) {
  FamilyDecl f;
  f.name = str_lit("F");
  f.params = params;
  f.indices = vec_new(heap->arena);
  f.level = level_one(heap);
  return f;
}

typedef struct {
  const char *name;
  bool ok;
} Row;

static Nat arity(Heap *heap, uint64_t n) { return heap_nat_of_u64(heap, n); }

/* cases(): the 10 named checks in Bend order. */
static void cases(Heap *heap, Row rows[RAW_SCOPE_COUNT]) {
  Globals g = global_initial(heap);
  Budget unlimited = budget_unlimited();
  const Term *terms[RAW_SCOPE_COUNT];
  raw_scope_terms(heap, terms);
  Shape pi_p = shape_spi(QUANTITY_ZERO, str_lit("P"), (void *)term_univ(heap, level_zero()));
  Decl counterfeit = decl_of("bad", term_univ(heap, level_succ(heap, level_of_u32(heap, 8))),
                             term_univ(heap, level_max(heap, level_of_u32(heap, 8), level_var(heap, nat_zero()))));
  Decl imax_zero = decl_of("bad", term_univ(heap, level_succ(heap, level_max(heap, level_var(heap, nat_zero()), level_var(heap, arity(heap, 1))))),
                           term_univ(heap, level_imax(heap, level_var(heap, nat_zero()), level_var(heap, arity(heap, 1)))));
  Vec params = push_one(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, level_var(heap, nat_zero()))));
  rows[0].name = "symbolic-distribution";
  rows[0].ok = success(check_check_scheme(heap, g, unlimited, arity(heap, 3), distributed(heap)));
  rows[1].name = "prop-impredicativity";
  rows[1].ok = universe(heap, check_infer_term(heap, g, unlimited, term_ran(heap, pi_p, term_var(heap, nat_zero()))));
  rows[2].name = "finite-sampling-counterfeit";
  rows[2].ok = mismatch(check_check_scheme(heap, g, unlimited, arity(heap, 1), counterfeit));
  rows[3].name = "imax-zero-branch";
  rows[3].ok = mismatch(check_check_scheme(heap, g, unlimited, arity(heap, 2), imax_zero));
  rows[4].name = "raw-fields-scope";
  rows[4].ok = all_scoped(heap, g, terms, 0);
  rows[5].name = "ordinary-template-escape";
  rows[5].ok = scope(check_check_decl(heap, g, unlimited, definition(heap, "escaped", level_var(heap, nat_zero()))));
  rows[6].name = "outside-scheme-scope";
  rows[6].ok = scope(check_check_scheme(heap, g, unlimited, arity(heap, 1), definition(heap, "outside", level_var(heap, arity(heap, 1)))));
  rows[7].name = "closed-family";
  rows[7].ok = success(check_declare_family(heap, g, unlimited, family_decl(heap, vec_new(heap->arena))));
  rows[8].name = "family-parameter-scope";
  rows[8].ok = scope(check_declare_family(heap, g, unlimited, family_decl(heap, params)));
  rows[9].name = "scheme-budget";
  rows[9].ok = budget_error(check_check_scheme(heap, g, budget_of_polls(nat_zero()), arity(heap, 3), distributed(heap)));
}

/* run(rows): the first False dies with "FAIL <name>"; all True prints the PASS line. */
static int run(Heap *heap, const Row rows[RAW_SCOPE_COUNT], size_t index) {
  if (index >= RAW_SCOPE_COUNT) {
    return driver_print(str_lit("PASS universe schemes, impredicativity, 10 raw-field scopes, family scope, budget"));
  }
  if (!rows[index].ok) {
    return driver_die(1, heap_str_cat(heap, str_lit("FAIL "), str_lit(rows[index].name)).data);
  }
  return run(heap, rows, index + 1);
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Row rows[RAW_SCOPE_COUNT];
  cases(&heap, rows);
  int code = heap.oom ? driver_host_failure() : run(&heap, rows, 0);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
