/* Port of bend2/tests/levels_protocol.bend (oracle label driver-levels-protocol): the universe level
 * protocol over the algebra, the checker and the polymorphism catalog. Prints the fixed OK line or
 * dies with 1 and the `name: error` lines joined by newline. arguments(args) ignores argv. */
#include "driver.h"
#include "levels_protocol.h"
#include "levels_regression.h"
#include "surface.h"

TR test_done(const void *value) {
  TR r;
  r.ok = true;
  r.value = value;
  r.error = str_empty();
  return r;
}

TR test_fail(Str message) {
  TR r;
  r.ok = false;
  r.value = NULL;
  r.error = message;
  return r;
}

/* require(ok, message) */
TR test_require(bool ok, Str message) { return ok ? test_done(NULL) : test_fail(message); }

/* kernel(result): Fail{error} -> Fail{E.to_string(error)} */
TR test_kernel(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return test_done(result.value);
  return test_fail(error_to_string(heap, result.error));
}

static Str cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }

static MaybeStr some_text(Str text) {
  MaybeStr m;
  m.some = true;
  m.value = text;
  return m;
}

static MaybeStr no_text(void) {
  MaybeStr m;
  m.some = false;
  m.value = str_empty();
  return m;
}

/* expect_message(expected, actual) */
static TR expect_message(Heap *heap, MaybeStr expected, Str actual) {
  if (!expected.some) return test_done(NULL);
  return test_require(str_eq(expected.value, actual), heap_str_cat(heap, str_lit("wrong diagnostic: "), actual));
}

/* expect(result, prefix, message) */
TR test_expect(Heap *heap, Result result, Str prefix, MaybeStr message) {
  if (result.kind == RESULT_DONE) return test_fail(cat3(heap, str_lit("expected "), prefix, str_lit(" refusal")));
  Str shown = error_to_string(heap, result.error);
  TR ok = test_require(str_starts_with(shown, heap_str_cat(heap, prefix, str_lit(":"))),
                       heap_str_cat(heap, str_lit("wrong refusal: "), shown));
  if (!ok.ok) return ok;
  return expect_message(heap, message, error_message(heap, result.error));
}

/* scope(result) */
static TR scope(Heap *heap, Result result) {
  return test_expect(heap, result, str_lit("universe"),
                     some_text(str_lit("universe level is outside the global parameter scope")));
}

/* collision(result, name) */
static TR collision(Heap *heap, Result result, Str name) {
  return test_expect(heap, result, str_lit("mismatch"),
                     some_text(cat3(heap, str_lit("the name "), name, str_lit(" is already declared"))));
}

/* same(expected, actual) */
static TR same(Heap *heap, const LevelVar *expected, const LevelVar *actual) {
  return test_require(level_equal(heap, expected, actual), str_lit("universe differs"));
}

/* universe(expected, value) */
static TR universe(Heap *heap, const LevelVar *expected, const void *boxed) {
  const Value *value = boxed;
  if (value != NULL && value->kind == VALUE_UNIV) return same(heap, expected, value->level);
  return test_fail(str_lit("expected a universe value"));
}

static MaybeTermPtr some_term(const Term *term) {
  MaybeTermPtr m;
  m.some = true;
  m.value = term;
  return m;
}

static MaybeTermPtr no_term(void) {
  MaybeTermPtr m;
  m.some = false;
  m.value = NULL;
  return m;
}

static Decl decl_of(Str name, DeclKind kind, const Term *ty, MaybeTermPtr body) {
  Decl d;
  d.name = name;
  d.kind = kind;
  d.ty = ty;
  d.body = body;
  return d;
}

/* definition(name, level) */
static Decl definition(Heap *heap, Str name, const LevelVar *level) {
  return decl_of(name, DECL_DEFINITION, term_univ(heap, level_succ(heap, level)), some_term(term_univ(heap, level)));
}

/* body(decl, body) */
static Decl body(Decl decl, const Term *term) {
  decl.body = some_term(term);
  return decl;
}

static const LevelVar *var_n(Heap *heap, uint64_t n) { return level_var(heap, heap_nat_of_u64(heap, n)); }

/* distributed(name) */
static Decl distributed(Heap *heap, Str name) {
  const LevelVar *u = var_n(heap, 0);
  const LevelVar *v = var_n(heap, 1);
  const LevelVar *w = var_n(heap, 2);
  return body(definition(heap, name, level_max(heap, level_imax(heap, u, v), level_imax(heap, u, w))),
              term_univ(heap, level_imax(heap, u, level_max(heap, v, w))));
}

/* primitive_count(globals) = List.length(entries) */
static uint64_t primitive_count(Globals globals) {
  uint64_t n = 0;
  const EntryRows *rows = globals.entries;
  while (rows != NULL) {
    n += 1;
    rows = rows->rest;
  }
  return n;
}

static Vec no_env(Heap *heap) { return vec_new(heap->arena); }

/* stored_entry(entry, globals, name, level, reference) */
static TR stored_entry(Heap *heap, MaybeEntry entry, Globals globals, Str name, const LevelVar *level,
                       const Term *reference) {
  if (!entry.some || entry.value.kind != ENTRY_DEF) return test_fail(str_lit("instance was not installed"));
  DefEntry def = entry.value.def;
  TR ok = test_require(term_equal(reference, term_global(heap, name)),
                       str_lit("instantiation did not return its global reference"));
  if (!ok.ok) return ok;
  TR tyv = test_kernel(heap, eval_eval(heap, globals, no_env(heap), def.ty));
  if (!tyv.ok) return tyv;
  ok = universe(heap, level_succ(heap, level), tyv.value);
  if (!ok.ok) return ok;
  TR value = test_kernel(heap, eval_eval(heap, globals, no_env(heap), def.body));
  if (!value.ok) return value;
  ok = universe(heap, level, value.value);
  if (!ok.ok) return ok;
  TR inferred = test_kernel(heap, check_infer_term(heap, globals, budget_unlimited(), reference));
  if (!inferred.ok) return inferred;
  return universe(heap, level_succ(heap, level), inferred.value);
}

/* stored(globals, name, level, reference) */
static TR stored(Heap *heap, Globals globals, Str name, const LevelVar *level, const Term *reference) {
  return stored_entry(heap, global_find(name, globals), globals, name, level, reference);
}

/* callback(limit) */
typedef struct {
  bool some;
  uint32_t left;
} Limit;

static CallbackPoll *callback_poll(Heap *heap, void *ctx) {
  Limit *limit = ctx;
  if (limit == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  bool spent = limit->some && limit->left == 0;
  out->spent = spent;
  out->next = limit->some ? test_callback(heap, true, spent ? 0 : limit->left - 1) : test_callback(heap, false, 0);
  return out;
}

Callback test_callback(Heap *heap, bool some, uint32_t left) {
  Limit *limit = heap_alloc(heap, sizeof *limit);
  if (limit != NULL) {
    limit->some = some;
    limit->left = left;
  }
  return budget_of_poll(callback_poll, limit);
}

/* count_step(step, count, done) */
typedef struct {
  ResumeK resume;
  uint32_t count;
  DoneK done;
} CountFrame;

static Step *count_step(Heap *heap, Step *step, uint32_t count, DoneK done);

static Step *count_resume(Heap *heap, void *ctx, bool spent) {
  CountFrame *frame = ctx;
  if (frame == NULL) return NULL;
  return count_step(heap, frame->resume.fn(heap, frame->resume.ctx, spent), frame->count + 1, frame->done);
}

static Step *count_step(Heap *heap, Step *step, uint32_t count, DoneK done) {
  if (step == NULL) return NULL;
  if (step->kind == STEP_FINISHED) {
    Measured *measured = heap_alloc(heap, sizeof *measured);
    if (measured == NULL) return NULL;
    measured->count = count;
    measured->result = step->result;
    return done.fn(heap, done.ctx, budget_result_done(measured));
  }
  CountFrame *frame = heap_alloc(heap, sizeof *frame);
  Step *out = heap_alloc(heap, sizeof *out);
  if (frame == NULL || out == NULL) return NULL;
  frame->resume = step->resume;
  frame->count = count;
  frame->done = done;
  out->kind = STEP_POLL;
  out->result = budget_result_done(NULL);
  out->resume.fn = count_resume;
  out->resume.ctx = frame;
  return out;
}

/* result => B.Finished{result} */
static Step *finished_step(Heap *heap, void *ctx, Result result) {
  (void)ctx;
  Step *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->kind = STEP_FINISHED;
  out->result = result;
  out->resume.fn = NULL;
  out->resume.ctx = NULL;
  return out;
}

/* counted(action) = CompLater{done => count_step(start_comp(action, Finished), 0, done)} */
static Step *counted_later(Heap *heap, void *ctx, DoneK done) {
  Comp *action = ctx;
  if (action == NULL) return NULL;
  DoneK finish;
  finish.fn = finished_step;
  finish.ctx = NULL;
  return count_step(heap, budget_start_comp(heap, *action, finish), 0, done);
}

static Comp counted(Heap *heap, Comp action) {
  Comp *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed != NULL) *boxed = action;
  Comp out;
  out.kind = COMP_LATER;
  out.value = NULL;
  out.error = error_host_oom();
  out.action.fn = counted_later;
  out.action.ctx = boxed;
  return out;
}

/* measure(action, limit) */
TR test_measure(Heap *heap, Comp action, bool some, uint32_t limit) {
  return test_kernel(heap, budget_run_poll(heap, counted(heap, action), test_callback(heap, some, limit)));
}

/* completed(measured) */
TR test_completed(Heap *heap, Measured measured) {
  TR value = test_kernel(heap, measured.result);
  if (!value.ok) return value;
  Completed *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return test_fail(str_lit("host out of memory"));
  out->count = measured.count;
  out->value = value.value;
  return test_done(out);
}

static const Measured *measured_of(TR r) { return r.value; }
static const Completed *completed_of(TR r) { return r.value; }

/* late_result(measured, limit, complete) */
static TR late_result(Heap *heap, const Measured *measured, uint32_t limit, uint32_t complete) {
  TR ok = test_expect(heap, measured->result, str_lit("budget"), some_text(str_lit("the check budget is exhausted")));
  if (!ok.ok) return ok;
  ok = test_require(measured->count > limit, str_lit("comparison returned before the late poll threshold"));
  if (!ok.ok) return ok;
  return test_require(measured->count < complete, str_lit("comparison continued through all work after exhaustion"));
}

typedef struct {
  const LevelVar *left;
  const LevelVar *right;
} LevelPair;

/* relation(): levels = reverse(relation_tail(11, [])) = [var 10, ..., var 0]; u = var 11 */
static LevelPair relation(Heap *heap) {
  const LevelVar *u = var_n(heap, 11);
  const LevelVar *max_acc = level_zero();
  const LevelVar *imax_acc = level_zero();
  int i = 10;
  while (i >= 0) {
    const LevelVar *value = var_n(heap, (uint64_t)i);
    max_acc = level_max(heap, max_acc, value);
    imax_acc = level_max(heap, imax_acc, level_imax(heap, u, value));
    i -= 1;
  }
  LevelPair pair;
  pair.left = level_imax(heap, u, max_acc);
  pair.right = imax_acc;
  return pair;
}

/* comparison_finish(measured, left, right, equality) */
static TR comparison_finish(Heap *heap, const Completed *done, const LevelVar *left, const LevelVar *right,
                            bool equality) {
  uint32_t complete = done->count;
  TR ok = test_require(kernel_bool_unbox(done->value), str_lit("unbounded symbolic comparison changed its result"));
  if (!ok.ok) return ok;
  ok = test_require(complete > 64, str_lit("symbolic comparison did not poll during its work"));
  if (!ok.ok) return ok;
  uint32_t limit = complete >> 1;
  TR late = test_measure(heap, level_eq_decide_m(heap, left, right, equality), true, limit);
  if (!late.ok) return late;
  return late_result(heap, measured_of(late), limit, complete);
}

/* comparison_relation(relation, equality) */
static TR comparison_relation(Heap *heap, LevelPair pair, bool equality) {
  TR measured = test_measure(heap, level_eq_decide_m(heap, pair.left, pair.right, equality), false, 0);
  if (!measured.ok) return measured;
  TR done = test_completed(heap, *measured_of(measured));
  if (!done.ok) return done;
  return comparison_finish(heap, completed_of(done), pair.left, pair.right, equality);
}

/* comparison_budget(equality) */
static TR comparison_budget(Heap *heap, bool equality) { return comparison_relation(heap, relation(heap), equality); }

/* nat_shape(domain) = SPi{QMany, "x", domain} */
static Shape nat_shape(const Term *domain) { return shape_spi(QUANTITY_MANY, str_lit("x"), (void *)domain); }

static Vec push_one(Heap *heap, void *item) {
  Vec v = vec_new(heap->arena);
  if (item == NULL || !vec_push(&v, item)) heap->oom = true;
  return v;
}

/* T.Sec{shape, [T.Leg{[Pair{QMany, "x"}], T.Var{0n}}]} */
static const Term *sec_x(Heap *heap, Shape shape) {
  Vec binders = push_one(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("x")));
  Vec legs = push_one(heap, term_leg_new(heap, binders, term_var(heap, nat_zero())));
  return term_sec(heap, shape, legs);
}

/* nat_identity() */
static Decl nat_identity(Heap *heap) {
  const Term *nat = prim_nat_ty(heap);
  return decl_of(str_lit("shapeIdentity"), DECL_DEFINITION, term_ran(heap, nat_shape(nat), nat),
                 some_term(sec_x(heap, nat_shape(nat))));
}

/* ignored_shape(globals, good, bad_body) */
static TR ignored_shape(Heap *heap, Globals globals, Decl good, const Term *bad_body) {
  TR entry = test_kernel(heap, check_check_decl(heap, globals, budget_unlimited(), good));
  if (!entry.ok) return entry;
  TR expected = test_kernel(heap, eval_eval(heap, globals, no_env(heap), good.ty));
  if (!expected.ok) return expected;
  Decl bad = body(good, bad_body);
  TR ok = scope(heap, check_check_decl(heap, globals, budget_unlimited(), bad));
  if (!ok.ok) return ok;
  ok = scope(heap, check_check_term(heap, globals, budget_unlimited(), bad_body, expected.value));
  if (!ok.ok) return ok;
  return scope(heap, check_check_scheme(heap, globals, budget_unlimited(), nat_zero(), bad));
}

static const Term *open_univ(Heap *heap) { return term_univ(heap, var_n(heap, 0)); }

/* symbolic_prop() */
static TR symbolic_prop(Heap *heap) {
  Decl prop = decl_of(str_lit("Proposition"), DECL_POSTULATE, term_univ(heap, level_zero()), no_term());
  TR entry = test_kernel(heap, check_check_decl(heap, global_initial(heap), budget_unlimited(), prop));
  if (!entry.ok) return entry;
  Globals globals = global_add(heap, str_lit("Proposition"), *(const Entry *)entry.value, global_initial(heap));
  const Term *ran = term_ran(heap, shape_spi(QUANTITY_MANY, str_lit("x"), (void *)open_univ(heap)),
                             term_global(heap, str_lit("Proposition")));
  Decl impredicative = decl_of(str_lit("impredicative"), DECL_DEFINITION, term_univ(heap, level_zero()), some_term(ran));
  return test_kernel(heap, check_check_scheme(heap, globals, budget_unlimited(), heap_nat_of_u64(heap, 1), impredicative));
}

/* impredicative() */
static TR impredicative(Heap *heap) {
  const Term *ran = term_ran(heap, shape_spi(QUANTITY_ZERO, str_lit("P"), (void *)term_univ(heap, level_zero())),
                             term_var(heap, nat_zero()));
  TR ty = test_kernel(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), ran));
  if (!ty.ok) return ty;
  return universe(heap, level_zero(), ty.value);
}

/* hidden_scope() */
static TR hidden_scope(Heap *heap) {
  const LevelVar *hidden = level_imax(heap, var_n(heap, 0), level_zero());
  TR ok = same(heap, level_zero(), hidden);
  if (!ok.ok) return ok;
  return scope(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), term_univ(heap, hidden)));
}

/* ignored_section() */
static TR ignored_section(Heap *heap) {
  return ignored_shape(heap, global_initial(heap), nat_identity(heap), sec_x(heap, nat_shape(open_univ(heap))));
}

static const Term *lit_zero(Heap *heap) { return term_lit(heap, literal_lint(big_zero())); }

/* pair_body(shape) = T.In{shape, APt{QMany, 0}, [0]} */
static const Term *pair_body(Heap *heap, Shape shape) {
  return term_in(heap, shape, term_addr_pt(QUANTITY_MANY, lit_zero(heap)), push_one(heap, (void *)lit_zero(heap)));
}

/* ignored_injection() */
static TR ignored_injection(Heap *heap) {
  const Term *nat = prim_nat_ty(heap);
  Decl pair = decl_of(str_lit("shapePair"), DECL_DEFINITION, term_lan(heap, nat_shape(nat), nat),
                      some_term(pair_body(heap, nat_shape(nat))));
  return ignored_shape(heap, global_initial(heap), pair, pair_body(heap, nat_shape(open_univ(heap))));
}

/* application_body(shape) = T.Out{shape, APt{QMany, 0}, Global{"shapeIdentity"}} */
static const Term *application_body(Heap *heap, Shape shape) {
  return term_out(heap, shape, term_addr_pt(QUANTITY_MANY, lit_zero(heap)), term_global(heap, str_lit("shapeIdentity")));
}

/* ignored_application() */
static TR ignored_application(Heap *heap) {
  TR identity = test_kernel(heap, check_check_decl(heap, global_initial(heap), budget_unlimited(), nat_identity(heap)));
  if (!identity.ok) return identity;
  Globals globals = global_add(heap, str_lit("shapeIdentity"), *(const Entry *)identity.value, global_initial(heap));
  const Term *bad = application_body(heap, nat_shape(open_univ(heap)));
  const Term *nat = prim_nat_ty(heap);
  Decl application = decl_of(str_lit("shapeApplication"), DECL_DEFINITION, nat,
                             some_term(application_body(heap, nat_shape(nat))));
  TR ok = ignored_shape(heap, globals, application, bad);
  if (!ok.ok) return ok;
  return scope(heap, check_infer_term(heap, globals, budget_unlimited(), bad));
}

/* Check.FamilyDecl{name, params, [], level} */
static FamilyDecl family_decl(Heap *heap, Str name, Vec params, const LevelVar *level) {
  FamilyDecl f;
  f.name = name;
  f.params = params;
  f.indices = vec_new(heap->arena);
  f.level = level;
  return f;
}

/* closed_family() */
static TR closed_family(Heap *heap) {
  FamilyDecl decl = family_decl(heap, str_lit("ClosedFamily"), vec_new(heap->arena), level_one(heap));
  TR globals = test_kernel(heap, check_declare_family(heap, global_initial(heap), budget_unlimited(), decl));
  if (!globals.ok) return globals;
  MaybeFamily found = global_find_family(str_lit("ClosedFamily"), *(const Globals *)globals.value);
  return test_require(found.some, str_lit("closed family disappeared"));
}

/* high_closed(value) */
static TR high_closed(Heap *heap, MaybeBig value) {
  if (!value.some) return test_fail(str_lit("large universe failed to parse"));
  const LevelVar *largest = level_var_offset(heap, value.value, level_zero());
  TR inferred = test_kernel(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), term_univ(heap, largest)));
  if (!inferred.ok) return inferred;
  return universe(heap, level_succ(heap, largest), inferred.value);
}

static MaybeStr exhausted(void) { return some_text(str_lit("the check budget is exhausted")); }

/* scheme_budget() */
static TR scheme_budget(Heap *heap) {
  Comp action = check_check_scheme_comp(heap, global_initial(heap), heap_nat_of_u64(heap, 3), distributed(heap, str_lit("budgeted")));
  return test_expect(heap, budget_run_poll(heap, action, test_callback(heap, true, 0)), str_lit("budget"), exhausted());
}

/* scheme_exact_failure(measured, complete) */
static TR scheme_exact_failure(Heap *heap, const Measured *measured, uint32_t complete) {
  TR ok = test_expect(heap, measured->result, str_lit("budget"), exhausted());
  if (!ok.ok) return ok;
  return test_require(measured->count == complete, str_lit("last-poll refusal did not stop at the measured threshold"));
}

/* scheme_exact_success(measured, complete) */
static TR scheme_exact_success(const Completed *done, uint32_t complete) {
  return test_require(done->count == complete, str_lit("one-higher budget changed the measured poll count"));
}

static Comp scheme_12(Heap *heap, Decl declaration) {
  return check_check_scheme_comp(heap, global_initial(heap), heap_nat_of_u64(heap, 12), declaration);
}

/* scheme_late_counts(baseline, full, declaration) */
static TR scheme_late_counts(Heap *heap, const Completed *baseline, const Completed *full, Decl declaration) {
  uint32_t base = baseline->count;
  uint32_t complete = full->count;
  TR ok = test_require(complete > base + 64, str_lit("complex comparison did not add polls beyond the same declaration shape"));
  if (!ok.ok) return ok;
  uint32_t half = complete >> 1;
  uint32_t limit = base + 1 > half ? base + 1 : half;
  TR late = test_measure(heap, scheme_12(heap, declaration), true, limit);
  if (!late.ok) return late;
  ok = late_result(heap, measured_of(late), limit, complete);
  if (!ok.ok) return ok;
  TR last = test_measure(heap, scheme_12(heap, declaration), true, complete - 1);
  if (!last.ok) return last;
  ok = scheme_exact_failure(heap, measured_of(last), complete);
  if (!ok.ok) return ok;
  TR exact = test_measure(heap, scheme_12(heap, declaration), true, complete);
  if (!exact.ok) return exact;
  TR done = test_completed(heap, *measured_of(exact));
  if (!done.ok) return done;
  return scheme_exact_success(completed_of(done), complete);
}

/* scheme_late_relation(pair) */
static TR scheme_late_relation(Heap *heap, LevelPair pair) {
  Decl declaration = body(definition(heap, str_lit("largeComparison"), pair.right), term_univ(heap, pair.left));
  TR baseline = test_measure(heap, scheme_12(heap, definition(heap, str_lit("plainComparison"), level_zero())), false, 0);
  if (!baseline.ok) return baseline;
  TR full = test_measure(heap, scheme_12(heap, declaration), false, 0);
  if (!full.ok) return full;
  TR baseline_done = test_completed(heap, *measured_of(baseline));
  if (!baseline_done.ok) return baseline_done;
  TR full_done = test_completed(heap, *measured_of(full));
  if (!full_done.ok) return full_done;
  return scheme_late_counts(heap, completed_of(baseline_done), completed_of(full_done), declaration);
}

/* catalog(name) */
static TR catalog(Heap *heap, Str name) {
  return test_kernel(heap, poly_declare(heap, global_initial(heap), poly_empty(), budget_unlimited(), heap_nat_of_u64(heap, 1),
                                        definition(heap, name, var_n(heap, 0))));
}

/* distributed_catalog() */
static TR distributed_catalog(Heap *heap) {
  return test_kernel(heap, poly_declare(heap, global_initial(heap), poly_empty(), budget_unlimited(), heap_nat_of_u64(heap, 3),
                                        distributed(heap, str_lit("distribution"))));
}

static PolyCatalog catalog_of(TR r) { return r.value; }
static const PolyInstance *instance_of(TR r) { return r.value; }

/* [l0, l1, ...] as a Vec of LevelVar pointers. */
static Vec level_list(Heap *heap, const LevelVar *const *items, size_t count) {
  Vec v = vec_new(heap->arena);
  size_t i = 0;
  while (i < count) {
    if (!vec_push(&v, (void *)items[i])) heap->oom = true;
    i += 1;
  }
  return v;
}

static Result instantiate(Heap *heap, Globals globals, PolyCatalog templates, const char *name, Vec levels, Str as_name) {
  return poly_instantiate(heap, globals, templates, budget_unlimited(), str_lit(name), levels, as_name);
}

/* template_symbolic() */
static TR template_symbolic(Heap *heap) {
  TR templates = distributed_catalog(heap);
  if (!templates.ok) return templates;
  TR ok = test_require(!global_find(str_lit("distribution"), global_initial(heap)).some, str_lit("bare template entered globals"));
  if (!ok.ok) return ok;
  return test_expect(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), term_global(heap, str_lit("distribution"))),
                     str_lit("unbound"), some_text(str_lit("distribution")));
}

/* template_branches() */
static TR template_branches(Heap *heap) {
  TR templates = distributed_catalog(heap);
  if (!templates.ok) return templates;
  const LevelVar *at_zero[3] = {level_of_u32(heap, 2), level_zero(), level_zero()};
  const LevelVar *at_three[3] = {level_of_u32(heap, 2), level_zero(), level_of_u32(heap, 3)};
  TR zero = test_kernel(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "distribution",
                                          level_list(heap, at_zero, 3), str_lit("atZero")));
  if (!zero.ok) return zero;
  TR ok = stored(heap, instance_of(zero)->globals, str_lit("atZero"), level_zero(), instance_of(zero)->term);
  if (!ok.ok) return ok;
  TR three = test_kernel(heap, instantiate(heap, instance_of(zero)->globals, catalog_of(templates), "distribution",
                                           level_list(heap, at_three, 3), str_lit("atThree")));
  if (!three.ok) return three;
  ok = stored(heap, instance_of(three)->globals, str_lit("atThree"), level_of_u32(heap, 3), instance_of(three)->term);
  if (!ok.ok) return ok;
  ok = stored(heap, instance_of(three)->globals, str_lit("atZero"), level_zero(), instance_of(zero)->term);
  if (!ok.ok) return ok;
  return test_require(!global_find(str_lit("distribution"), instance_of(three)->globals).some,
                      str_lit("template entered instantiated globals"));
}

/* template_duplicate() */
static TR template_duplicate(Heap *heap) {
  TR templates = catalog(heap, str_lit("same"));
  if (!templates.ok) return templates;
  return collision(heap, poly_declare(heap, global_initial(heap), catalog_of(templates), budget_unlimited(), heap_nat_of_u64(heap, 1),
                                      definition(heap, str_lit("same"), var_n(heap, 0))),
                   str_lit("same"));
}

/* instance_arity() */
static TR instance_arity(Heap *heap) {
  TR templates = distributed_catalog(heap);
  if (!templates.ok) return templates;
  const LevelVar *two[2] = {level_zero(), level_one(heap)};
  const LevelVar *four[4] = {level_zero(), level_one(heap), level_one(heap), level_one(heap)};
  TR ok = test_expect(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "distribution", level_list(heap, two, 2), str_lit("tooFew")),
                      str_lit("universe"), some_text(str_lit("the schema distribution expects 3 universe arguments, got 2")));
  if (!ok.ok) return ok;
  return test_expect(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "distribution", level_list(heap, four, 4), str_lit("tooMany")),
                     str_lit("universe"), some_text(str_lit("the schema distribution expects 3 universe arguments, got 4")));
}

/* instance_open() */
static TR instance_open(Heap *heap) {
  TR templates = catalog(heap, str_lit("poly"));
  if (!templates.ok) return templates;
  const LevelVar *open[1] = {var_n(heap, 0)};
  return test_expect(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "poly", level_list(heap, open, 1), str_lit("open")),
                     str_lit("universe"), some_text(str_lit("universe arguments must be closed")));
}

/* instance_collision(name) */
static TR instance_collision(Heap *heap, Str name) {
  TR templates = catalog(heap, str_lit("poly"));
  if (!templates.ok) return templates;
  const LevelVar *zero[1] = {level_zero()};
  return collision(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "poly", level_list(heap, zero, 1), name), name);
}

/* failed_instances() */
static TR failed_instances(Heap *heap) {
  TR templates = catalog(heap, str_lit("poly"));
  if (!templates.ok) return templates;
  uint64_t original_count = primitive_count(global_initial(heap));
  TR ok = test_expect(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "poly", vec_new(heap->arena), str_lit("retry")),
                      str_lit("universe"), some_text(str_lit("the schema poly expects 1 universe arguments, got 0")));
  if (!ok.ok) return ok;
  bool unchanged = original_count == primitive_count(global_initial(heap));
  bool missing = !global_find(str_lit("retry"), global_initial(heap)).some;
  ok = test_require(unchanged && missing, str_lit("failed instance changed original globals"));
  if (!ok.ok) return ok;
  const LevelVar *one[1] = {level_one(heap)};
  TR instance = test_kernel(heap, instantiate(heap, global_initial(heap), catalog_of(templates), "poly", level_list(heap, one, 1), str_lit("retry")));
  if (!instance.ok) return instance;
  Globals next = instance_of(instance)->globals;
  ok = test_require(primitive_count(next) == original_count + 1, str_lit("failed instance leaked an entry into the next environment"));
  if (!ok.ok) return ok;
  ok = stored(heap, next, str_lit("retry"), level_one(heap), instance_of(instance)->term);
  if (!ok.ok) return ok;
  const LevelVar *zero[1] = {level_zero()};
  ok = collision(heap, instantiate(heap, next, catalog_of(templates), "poly", level_list(heap, zero, 1), str_lit("retry")), str_lit("retry"));
  if (!ok.ok) return ok;
  return stored(heap, next, str_lit("retry"), level_one(heap), instance_of(instance)->term);
}

/* instance_budget() */
static TR instance_budget(Heap *heap) {
  TR templates = catalog(heap, str_lit("poly"));
  if (!templates.ok) return templates;
  const LevelVar *one[1] = {level_one(heap)};
  Comp action = poly_instantiate_comp(heap, global_initial(heap), catalog_of(templates), str_lit("poly"), level_list(heap, one, 1),
                                      str_lit("budgetInstance"));
  return test_expect(heap, budget_run_poll(heap, action, test_callback(heap, true, 0)), str_lit("budget"), exhausted());
}

typedef struct {
  const char *name;
  TR result;
} Named;

enum { ALGEBRA_COUNT = 19, CHECKER_COUNT = 23, POLY_COUNT = 12, CASE_COUNT = ALGEBRA_COUNT + CHECKER_COUNT + POLY_COUNT };

static void put(Named *out, size_t *at, const char *name, TR result) {
  if (*at < CASE_COUNT) {
    out[*at].name = name;
    out[*at].result = result;
  }
  *at += 1;
}

static const char *const ALGEBRA_NAMES[LEVELS_REGRESSION_CASE_COUNT] = {
    "imax-zero", "imax-zero-left", "imax-self", "imax-distributes-over-max", "successor-distributes-over-max",
    "successor-dominates-variable-and-one", "max-flattens-and-drops-duplicates", "imax-positive-right-is-max",
    "imax-symbolic-right-is-not-max", "symbolic-zero-is-not-positive", "imax-preserves-conditional-positivity",
    "le-successor-domination", "le-imax-case-split", "no-finite-sampling-of-unbounded-variables",
    "closed-levels-beyond-host-int", "scope-and-constructor-boundaries", "substitution-visits-imax-and-max"};

/* algebra_cases(): named_algebra over Algebra.cases() (both lists have 17 entries) plus the two budget cases. */
static void algebra_cases(Heap *heap, Named *out, size_t *at) {
  LevelsCase algebra[LEVELS_REGRESSION_CASE_COUNT];
  levels_regression_cases(heap, algebra);
  size_t i = 0;
  while (i < LEVELS_REGRESSION_CASE_COUNT) {
    put(out, at, ALGEBRA_NAMES[i], test_require(algebra[i].holds, str_lit(ALGEBRA_NAMES[i])));
    i += 1;
  }
  put(out, at, "equal-budget-expires-inside-symbolic-comparison", comparison_budget(heap, true));
  put(out, at, "le-budget-expires-inside-symbolic-comparison", comparison_budget(heap, false));
}

static Result scheme(Heap *heap, uint64_t arity, Decl decl) {
  return check_check_scheme(heap, global_initial(heap), budget_unlimited(), heap_nat_of_u64(heap, arity), decl);
}

static Result infer(Heap *heap, const Term *term) {
  return check_infer_term(heap, global_initial(heap), budget_unlimited(), term);
}

/* checker_cases() */
static void checker_cases(Heap *heap, Named *out, size_t *at) {
  Str mismatch = str_lit("mismatch");
  const LevelVar *u = var_n(heap, 0);
  const LevelVar *v = var_n(heap, 1);
  const LevelVar *eight = level_of_u32(heap, 8);
  put(out, at, "scheme-checks-imax-distribution-symbolically", test_kernel(heap, scheme(heap, 3, distributed(heap, str_lit("distribution")))));
  put(out, at, "point-former-preserves-prop-impredicativity", impredicative(heap));
  put(out, at, "symbolic-point-former-preserves-prop-impredicativity", symbolic_prop(heap));
  put(out, at, "scheme-refuses-finite-sampling-counterfeit",
      test_expect(heap, scheme(heap, 1, body(definition(heap, str_lit("counterfeit"), eight), term_univ(heap, level_max(heap, eight, u)))),
                  mismatch, no_text()));
  put(out, at, "scheme-refuses-imax-with-zero-branch-deleted",
      test_expect(heap, scheme(heap, 2, body(definition(heap, str_lit("badImMax"), level_max(heap, u, v)), term_univ(heap, level_imax(heap, u, v)))),
                  mismatch, no_text()));
  put(out, at, "ordinary-inference-refuses-free-level", scope(heap, infer(heap, open_univ(heap))));
  put(out, at, "ordinary-inference-refuses-erased-free-level", hidden_scope(heap));
  put(out, at, "raw-inference-refuses-free-level",
      scope(heap, check_infer_node(heap, check_make(heap, global_initial(heap), budget_unlimited(), nat_zero()), QUANTITY_MANY, open_univ(heap))));
  put(out, at, "ordinary-check-refuses-free-level",
      scope(heap, check_check_term(heap, global_initial(heap), budget_unlimited(), open_univ(heap), value_vuniv(heap, level_succ(heap, u)))));
  put(out, at, "ordinary-declaration-refuses-template",
      scope(heap, check_check_decl(heap, global_initial(heap), budget_unlimited(), definition(heap, str_lit("escaped"), u))));
  put(out, at, "scheme-refuses-out-of-range-type-level", scope(heap, scheme(heap, 1, definition(heap, str_lit("outside"), v))));
  put(out, at, "scheme-refuses-hidden-annotation-level",
      scope(heap, scheme(heap, 1, body(definition(heap, str_lit("annotation"), level_zero()),
                                        term_ann(heap, term_univ(heap, level_zero()), term_univ(heap, v))))));
  put(out, at, "scheme-checks-bound-shape-domain",
      test_kernel(heap, scheme(heap, 1, body(definition(heap, str_lit("shape"), u),
                                              term_ran(heap, shape_spi(QUANTITY_ZERO, str_lit("A"), (void *)term_univ(heap, u)),
                                                       term_univ(heap, level_zero()))))));
  put(out, at, "scheme-refuses-hidden-shape-domain-level",
      scope(heap, scheme(heap, 1, body(definition(heap, str_lit("shapeOutside"), u),
                                        term_ran(heap, shape_spi(QUANTITY_ZERO, str_lit("A"), (void *)term_univ(heap, v)),
                                                 term_univ(heap, level_zero()))))));
  put(out, at, "ignored-section-shape-refuses-free-level", ignored_section(heap));
  put(out, at, "ignored-injection-shape-refuses-free-level", ignored_injection(heap));
  put(out, at, "ignored-application-shape-refuses-free-level", ignored_application(heap));
  put(out, at, "closed-family-remains-supported", closed_family(heap));
  put(out, at, "family-refuses-open-universe",
      test_expect(heap, check_declare_family(heap, global_initial(heap), budget_unlimited(),
                                             family_decl(heap, str_lit("OpenFamily"), vec_new(heap->arena), u)),
                  str_lit("universe"), some_text(str_lit("family universe levels must be closed at M0 Stage A"))));
  Vec open_param = push_one(heap, positivity_binder_new(heap, QUANTITY_ZERO, str_lit("A"), term_univ(heap, u)));
  put(out, at, "family-refuses-open-parameter-type",
      scope(heap, check_declare_family(heap, global_initial(heap), budget_unlimited(),
                                       family_decl(heap, str_lit("OpenParameter"), open_param, level_one(heap)))));
  put(out, at, "closed-high-universe-checks-without-overflow", high_closed(heap, big_of_decimal(heap, str_lit("4611686018427387903"))));
  put(out, at, "scheme-honors-check-budget", scheme_budget(heap));
  put(out, at, "scheme-budget-expires-during-level-comparison", scheme_late_relation(heap, relation(heap)));
}

/* poly_cases() */
static void poly_cases(Heap *heap, Named *out, size_t *at) {
  const LevelVar *u = var_n(heap, 0);
  const LevelVar *eight = level_of_u32(heap, 8);
  put(out, at, "template-does-not-enter-monomorphic-globals", template_symbolic(heap));
  put(out, at, "closed-specializations-take-both-imax-branches", template_branches(heap));
  put(out, at, "template-refuses-invalid-universal-body",
      test_expect(heap, poly_declare(heap, global_initial(heap), poly_empty(), budget_unlimited(), heap_nat_of_u64(heap, 1),
                                     body(definition(heap, str_lit("counterfeit"), eight), term_univ(heap, level_max(heap, eight, u)))),
                  str_lit("mismatch"), no_text()));
  put(out, at, "template-refuses-duplicate-name", template_duplicate(heap));
  put(out, at, "template-refuses-existing-global-name",
      collision(heap, poly_declare(heap, global_initial(heap), poly_empty(), budget_unlimited(), nat_zero(),
                                   definition(heap, str_lit("Nat"), level_zero())),
                str_lit("Nat")));
  put(out, at, "instance-refuses-wrong-arity", instance_arity(heap));
  put(out, at, "instance-refuses-open-argument", instance_open(heap));
  put(out, at, "instance-refuses-template-name", instance_collision(heap, str_lit("poly")));
  put(out, at, "instance-refuses-existing-global-name", instance_collision(heap, str_lit("Nat")));
  put(out, at, "instance-refuses-unknown-template",
      test_expect(heap, instantiate(heap, global_initial(heap), poly_empty(), "missing", vec_new(heap->arena), str_lit("unknown")),
                  str_lit("unbound"), some_text(str_lit("unknown universe schema missing"))));
  put(out, at, "failed-instances-preserve-environments", failed_instances(heap));
  put(out, at, "instance-rechecks-with-caller-budget", instance_budget(heap));
}

/* failures(cases, []): `name: error` per failed case, in case order. */
static Vec failures(Heap *heap, const Named *all, size_t count) {
  Vec found = vec_new(heap->arena);
  size_t i = 0;
  while (i < count) {
    if (!all[i].result.ok) {
      Str line = cat3(heap, str_lit(all[i].name), str_lit(": "), all[i].result.error);
      Str *boxed = heap_alloc(heap, sizeof *boxed);
      if (boxed == NULL || !vec_push(&found, boxed)) heap->oom = true;
      if (boxed != NULL) *boxed = line;
    }
    i += 1;
  }
  return found;
}

/* finish(errors) */
static int finish(Heap *heap, const Vec *errors) {
  if (vec_get(errors, 0).status != VEC_GET_OK) {
    return driver_print(str_lit("LEVELS-RUNTIME-OK algebra=19 checker=23 poly=12 boundary_cases=1"));
  }
  Str joined = str_empty();
  size_t i = 0;
  VecGet got = vec_get(errors, i);
  while (got.status == VEC_GET_OK) {
    Str line = *(const Str *)got.value;
    joined = i == 0 ? line : cat3(heap, joined, str_lit("\n"), line);
    i += 1;
    got = vec_get(errors, i);
  }
  return driver_die_text(1, joined);
}

/* run() */
static int run(Heap *heap) {
  Named all[CASE_COUNT];
  size_t at = 0;
  algebra_cases(heap, all, &at);
  checker_cases(heap, all, &at);
  poly_cases(heap, all, &at);
  if (heap->oom) return driver_host_failure();
  Vec errors = failures(heap, all, at < CASE_COUNT ? at : CASE_COUNT);
  if (heap->oom) return driver_host_failure();
  return finish(heap, &errors);
}

int levels_protocol_run(Heap *heap) { return run(heap); }

#ifndef MECH_DRIVER_LIBRARY
/* arguments(args) ignores args; main() = run(). */
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
#endif
