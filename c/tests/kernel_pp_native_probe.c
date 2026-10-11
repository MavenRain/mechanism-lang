/* C-only probe of c/kernel/pp.c, level_scope.c, context.c and rules.c (slice S1.7). No Bend test
 * driver exercises these four files before the checker lands, so there is no oracle record:
 * c-test runs this binary with no arguments and compares the output with
 * c/tests/kernel_pp_native_probe.expected, whose lines were checked by hand against the Bend
 * sources. Every line is `label: value`. */
#include "driver.h"

static Str lit(const char *text) {
  return str_lit(text);
}

static Str cat(Heap *heap, Str a, Str b) {
  return heap_str_cat(heap, a, b);
}

static Str cat3(Heap *heap, Str a, Str b, Str c) {
  return cat(heap, cat(heap, a, b), c);
}

static int show(Heap *heap, const char *label, Str value) {
  Str text = cat3(heap, lit(label), lit(": "), value);
  if (heap->oom) return driver_host_failure();
  return driver_print(text);
}

static Str yes_no(bool b) {
  return lit(b ? "true" : "false");
}

static Str show_len(Heap *heap, size_t len) {
  return heap_nat_show(heap, heap_nat_of_u64(heap, len));
}

static void push(Heap *heap, Vec *xs, const void *item) {
  if (item == NULL || !vec_push(xs, (void *)item)) heap->oom = true;
}

static void push_name(Heap *heap, Vec *xs, const char *name) {
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) {
    heap->oom = true;
    return;
  }
  *boxed = lit(name);
  push(heap, xs, boxed);
}

static Vec names0(Heap *heap) {
  return vec_new(heap->arena);
}

static Vec names1(Heap *heap, const char *a) {
  Vec out = vec_new(heap->arena);
  push_name(heap, &out, a);
  return out;
}

static Vec names2(Heap *heap, const char *a, const char *b) {
  Vec out = names1(heap, a);
  push_name(heap, &out, b);
  return out;
}

static Vec names3(Heap *heap, const char *a, const char *b, const char *c) {
  Vec out = names2(heap, a, b);
  push_name(heap, &out, c);
  return out;
}

static Vec list1(Heap *heap, const void *item) {
  Vec out = vec_new(heap->arena);
  push(heap, &out, item);
  return out;
}

static Vec list2(Heap *heap, const void *a, const void *b) {
  Vec out = list1(heap, a);
  push(heap, &out, b);
  return out;
}

static const char *quantity_name(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return "zero";
    case QUANTITY_ONE: return "one";
    case QUANTITY_MANY: return "many";
  }
  return "?";
}

static const char *error_kind_name(ErrorKind kind) {
  switch (kind) {
    case ERROR_PARSE: return "parse";
    case ERROR_NOT_YET: return "not_yet";
    case ERROR_CARRY: return "carry";
    case ERROR_UNBOUND: return "unbound";
    case ERROR_MISMATCH: return "mismatch";
    case ERROR_UNIVERSE: return "universe";
    case ERROR_QUANTITY: return "quantity";
    case ERROR_WRONG_LEG: return "wrong_leg";
    case ERROR_MISSING_BRANCH: return "missing_branch";
    case ERROR_OVERFLOW: return "overflow";
    case ERROR_CANNOT_INFER: return "cannot_infer";
    case ERROR_BUDGET_EXHAUSTED: return "budget_exhausted";
    case ERROR_INDEX_NOT_ZERO: return "index_not_zero";
    case ERROR_INDEX_ABOVE_UNIVERSE: return "index_above_universe";
    case ERROR_TERMINATION: return "termination";
  }
  return "?";
}

static Str render_error(Heap *heap, Error e) {
  return cat(heap, cat3(heap, lit("fail "), lit(error_kind_name(e.kind)), lit(": ")), e.message);
}

static Str render_result(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_DONE: return lit("done");
    case RESULT_FAIL: return render_error(heap, r.error);
  }
  return lit("?");
}

static Str render_term_result(Heap *heap, const Vec *names, Result r) {
  switch (r.kind) {
    case RESULT_DONE: return cat(heap, lit("done "), pp_term(heap, names, (const Term *)r.value));
    case RESULT_FAIL: return render_error(heap, r.error);
  }
  return lit("?");
}

static Str render_family_result(Heap *heap, FamilyResult r) {
  switch (r.kind) {
    case RESULT_DONE: return lit("done");
    case RESULT_FAIL: return render_error(heap, r.error);
  }
  return lit("?");
}

static Str render_vec_result(Heap *heap, VecResult r) {
  switch (r.kind) {
    case RESULT_DONE: return cat(heap, lit("done "), show_len(heap, r.value.len));
    case RESULT_FAIL: return render_error(heap, r.error);
  }
  return lit("?");
}

static Str render_strings(Heap *heap, const Vec *names) {
  Str out = show_len(heap, names->len);
  size_t i;
  for (i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    const Str *s = got.status == VEC_GET_OK ? got.value : NULL;
    out = cat3(heap, out, lit(" "), s == NULL ? lit("?") : *s);
  }
  return out;
}

static Str render_leg_arities(Heap *heap, const Vec *legs) {
  Str out = show_len(heap, legs->len);
  size_t i;
  for (i = 0; i < legs->len; i++) {
    VecGet got = vec_get(legs, i);
    const Leg *leg = got.status == VEC_GET_OK ? got.value : NULL;
    out = cat3(heap, out, lit(" "), leg == NULL ? lit("?") : show_len(heap, leg->binders.len));
  }
  return out;
}

static Str render_eta(Heap *heap, MaybeEtaRow row) {
  if (!row.some) return lit("none");
  return cat3(heap, cat(heap, lit("some ran="), yes_no(row.value.ran)), lit(" lan="), yes_no(row.value.lan));
}

static Str render_local(Heap *heap, MaybeLocal local) {
  if (!local.some) return lit("none");
  return cat3(heap, local.value.name, lit(" "), lit(quantity_name(local.value.quantity)));
}

static Str render_view(LView view) {
  switch (view.kind) {
    case LVIEW_NIL: return lit("nil");
    case LVIEW_CONS: return view.head.name;
  }
  return lit("?");
}

static Str bracket(Heap *heap, Str s) {
  return cat3(heap, lit("["), s, lit("]"));
}

static Str readable_row(Heap *heap, Quantity mode) {
  return cat3(heap, cat(heap, yes_no(context_readable(mode, QUANTITY_ZERO)), lit(" ")),
              cat(heap, yes_no(context_readable(mode, QUANTITY_ONE)), lit(" ")),
              yes_no(context_readable(mode, QUANTITY_MANY)));
}

/* The shared fixtures: a few globals, universes and one shape of every kind. */
typedef struct {
  const Term *nat;
  const Term *bool_g;
  const Term *zero_g;
  const Term *univ0;
  const Term *univ1;
  Shape spi_x;     /* SPi w x Nat */
  Shape sarg_y;    /* SArg 1 y */
  Shape scoll3;    /* SColl 3 */
  Shape spar;      /* SPar Type 0 Nat */
  Shape smu;       /* SMu Tree [Nat; Type 0] */
  Shape smu_empty; /* SMu T [] */
  Shape snu;       /* SNu Stream [Nat] */
  Shape szk;       /* SZk (0 w : Nat) */
  Shape sfhc;      /* SFhc Type 0 */
  Shape smpc;      /* SMpc (Nat, Type 0) */
} Fix;

static Fix fix_new(Heap *heap) {
  Fix f;
  f.nat = term_global(heap, lit("Nat"));
  f.bool_g = term_global(heap, lit("Bool"));
  f.zero_g = term_global(heap, lit("zero"));
  f.univ0 = term_univ(heap, level_zero());
  f.univ1 = term_univ(heap, level_one(heap));
  f.spi_x = shape_spi(QUANTITY_MANY, lit("x"), (void *)f.nat);
  f.sarg_y = shape_sarg(QUANTITY_ONE, lit("y"));
  f.scoll3 = shape_scoll(big_of_u32(heap, 3));
  f.spar = shape_spar((void *)f.univ0, (void *)f.nat);
  f.smu = shape_smu(lit("Tree"), list2(heap, f.nat, f.univ0));
  f.smu_empty = shape_smu(lit("T"), vec_new(heap->arena));
  f.snu = shape_snu(lit("Stream"), list1(heap, f.nat));
  f.szk = shape_szk(QUANTITY_ZERO, lit("w"), (void *)f.nat);
  f.sfhc = shape_sfhc((void *)f.univ0);
  f.smpc = shape_smpc((void *)f.nat, (void *)f.univ0);
  return f;
}

static Branch *branch_new(Heap *heap, Addr addr, const Leg *leg) {
  Branch *b = leg == NULL ? NULL : heap_alloc(heap, sizeof(Branch));
  if (b == NULL) {
    heap->oom = true;
    return NULL;
  }
  b->addr = addr;
  b->leg = *leg;
  return b;
}

static const Term *elim_term(Heap *heap, MaybeMotive motive, Vec branches) {
  ElimData data;
  data.shape = shape_scoll(big_of_u32(heap, 2));
  data.scrut = term_global(heap, lit("b"));
  data.scrut_q = QUANTITY_MANY;
  data.motive = motive;
  data.branches = branches;
  return term_elim(heap, data);
}

static MaybeStr no_str(void) {
  MaybeStr none;
  none.some = false;
  none.value = str_empty();
  return none;
}

static MaybeStr some_str(const char *text) {
  MaybeStr some;
  some.some = true;
  some.value = lit(text);
  return some;
}

static int pp_lines(Heap *heap, const Fix *f) {
  Vec none = names0(heap);
  Vec nx = names1(heap, "x");
  Vec ny = names1(heap, "y");
  Vec nk = names1(heap, "k");
  Vec nn = names1(heap, "n");
  Vec ab = names2(heap, "a", "b");
  Vec abc = names3(heap, "a", "b", "c");
  Vec under_pi = pp_under(heap, f->spi_x, &nn);
  Vec under_zk = pp_under(heap, f->szk, &nn);
  Vec under_coll = pp_under(heap, f->scoll3, &nn);
  Vec pushed = pp_push(heap, &nn, &ab);
  Shape scoll2 = shape_scoll(big_of_u32(heap, 2));
  const Term *lan_pi = term_lan(heap, f->spi_x, term_var(heap, nat_zero()));
  const Term *ran_pi = term_ran(heap, shape_spi(QUANTITY_ZERO, lit("a"), (void *)f->nat), term_var(heap, heap_nat_of_u64(heap, 1)));
  const Term *ran_coll = term_ran(heap, scoll2, term_var(heap, nat_zero()));
  const Leg *leg_nat = term_leg_new(heap, vec_new(heap->arena), f->nat);
  const Leg *leg_ab = term_leg_new(heap, list2(heap, term_binder_new(heap, QUANTITY_MANY, lit("a")), term_binder_new(heap, QUANTITY_ONE, lit("b"))),
                                   term_var(heap, nat_zero()));
  const Term *sec = term_sec(heap, scoll2, list2(heap, leg_nat, leg_ab));
  const Term *ann = term_ann(heap, f->zero_g, f->nat);
  const Term *let_t = term_let(heap, lit("n"), f->nat, f->zero_g, term_var(heap, nat_zero()));
  const Term *in_t = term_in(heap, shape_scoll(big_one()), term_addr_leg(big_zero()), list2(heap, f->zero_g, term_var(heap, nat_zero())));
  const Term *out_t = term_out(heap, f->spi_x, term_addr_pt(QUANTITY_MANY, f->zero_g), term_global(heap, lit("f")));
  const Leg *leg_t = term_leg_new(heap, vec_new(heap->arena), term_global(heap, lit("t")));
  const Leg *leg_v = term_leg_new(heap, list1(heap, term_binder_new(heap, QUANTITY_MANY, lit("v"))), term_var(heap, nat_zero()));
  const Term *elim_t = elim_term(heap, term_motive_none(),
                                 list2(heap, branch_new(heap, term_addr_leg(big_zero()), leg_t), branch_new(heap, term_addr_leg(big_one()), leg_v)));
  Motive motive = term_motive(no_str(), names1(heap, "i"), lit("s"), term_var(heap, heap_nat_of_u64(heap, 1)));
  const Term *elim_m = elim_term(heap, term_motive_some(motive), vec_new(heap->arena));
  int code = 0;
  code = code ? code : show(heap, "escaped quote", pp_escaped(heap, lit("a\"b")));
  code = code ? code : show(heap, "escaped backslash", pp_escaped(heap, lit("a\\b")));
  code = code ? code : show(heap, "escaped newline tab", pp_escaped(heap, lit("a\nb\tc")));
  code = code ? code : show(heap, "escaped cr bs", pp_escaped(heap, lit("\r\b")));
  code = code ? code : show(heap, "escaped e-acute", pp_escaped(heap, lit("\xc3\xa9")));
  code = code ? code : show(heap, "escaped del", pp_escaped(heap, lit("\x7f")));
  code = code ? code : show(heap, "escaped control 1", pp_escaped(heap, lit("\x01")));
  code = code ? code : show(heap, "escaped space tilde", pp_escaped(heap, lit("[ ~]")));
  code = code ? code : show(heap, "literal string", pp_literal(heap, literal_lstring(lit("hi"))));
  code = code ? code : show(heap, "literal int", pp_literal(heap, literal_lint(big_of_u32(heap, 42))));
  code = code ? code : show(heap, "literal negative int", pp_literal(heap, literal_lint(big_neg(big_of_u32(heap, 7)))));
  code = code ? code : show(heap, "join empty", bracket(heap, pp_join(heap, &none, lit(", "))));
  code = code ? code : show(heap, "join one", bracket(heap, pp_join(heap, &nx, lit(", "))));
  code = code ? code : show(heap, "join three", bracket(heap, pp_join(heap, &abc, lit(", "))));
  code = code ? code : show(heap, "shape SPi", pp_shape(heap, &none, f->spi_x));
  code = code ? code : show(heap, "shape SArg", pp_shape(heap, &none, f->sarg_y));
  code = code ? code : show(heap, "shape SColl", pp_shape(heap, &none, f->scoll3));
  code = code ? code : show(heap, "shape SPar", pp_shape(heap, &none, f->spar));
  code = code ? code : show(heap, "shape SMu", pp_shape(heap, &none, f->smu));
  code = code ? code : show(heap, "shape SMu empty", pp_shape(heap, &none, f->smu_empty));
  code = code ? code : show(heap, "shape SNu", pp_shape(heap, &none, f->snu));
  code = code ? code : show(heap, "shape SZk", pp_shape(heap, &none, f->szk));
  code = code ? code : show(heap, "shape SFhc", pp_shape(heap, &none, f->sfhc));
  code = code ? code : show(heap, "shape SMpc", pp_shape(heap, &none, f->smpc));
  code = code ? code : show(heap, "under SPi", render_strings(heap, &under_pi));
  code = code ? code : show(heap, "under SZk", render_strings(heap, &under_zk));
  code = code ? code : show(heap, "under SColl", render_strings(heap, &under_coll));
  code = code ? code : show(heap, "push", render_strings(heap, &pushed));
  code = code ? code : show(heap, "addr APt", pp_addr(heap, &none, term_addr_pt(QUANTITY_ONE, f->nat)));
  code = code ? code : show(heap, "addr ALeg", pp_addr(heap, &none, term_addr_leg(big_of_u32(heap, 2))));
  code = code ? code : show(heap, "addr ACtor", pp_addr(heap, &none, term_addr_ctor(lit("cons"))));
  code = code ? code : show(heap, "term var named", pp_term(heap, &nx, term_var(heap, nat_zero())));
  code = code ? code : show(heap, "term var unnamed", pp_term(heap, &nx, term_var(heap, heap_nat_of_u64(heap, 3))));
  code = code ? code : show(heap, "term univ zero", pp_term(heap, &none, f->univ0));
  code = code ? code : show(heap, "term univ one", pp_term(heap, &none, f->univ1));
  code = code ? code : show(heap, "term global", pp_term(heap, &none, f->nat));
  code = code ? code : show(heap, "term lan SPi", pp_term(heap, &none, lan_pi));
  code = code ? code : show(heap, "term ran SPi var1", pp_term(heap, &ny, ran_pi));
  code = code ? code : show(heap, "term ran SColl", pp_term(heap, &ny, ran_coll));
  code = code ? code : show(heap, "term sec", pp_term(heap, &none, sec));
  code = code ? code : show(heap, "term ann", pp_term(heap, &none, ann));
  code = code ? code : show(heap, "term let", pp_term(heap, &none, let_t));
  code = code ? code : show(heap, "term in", pp_term(heap, &nk, in_t));
  code = code ? code : show(heap, "term out", pp_term(heap, &none, out_t));
  code = code ? code : show(heap, "term elim", pp_term(heap, &none, elim_t));
  code = code ? code : show(heap, "term elim motive", pp_term(heap, &none, elim_m));
  code = code ? code : show(heap, "term lit", pp_term(heap, &none, term_lit(heap, literal_lstring(lit("a\"b")))));
  code = code ? code : show(heap, "term auto", pp_term(heap, &none, term_auto()));
  code = code ? code : show(heap, "nth_name none 5", pp_nth_name(heap, no_str(), heap_nat_of_u64(heap, 5)));
  code = code ? code : show(heap, "nth_name some", pp_nth_name(heap, some_str("z"), heap_nat_of_u64(heap, 5)));
  return code;
}

static int level_scope_lines(Heap *heap, const Fix *f) {
  Nat zero = nat_zero();
  Nat one = heap_nat_of_u64(heap, 1);
  Nat two = heap_nat_of_u64(heap, 2);
  const Term *univ_v0 = term_univ(heap, level_var(heap, nat_zero()));
  const Term *univ_v1 = term_univ(heap, level_var(heap, one));
  const Term *univ_v2 = term_univ(heap, level_var(heap, two));
  const Term *univ_s = term_univ(heap, level_var_lsucc(heap, big_one(), level_var(heap, nat_zero())));
  const Term *ok_term = term_ran(heap, shape_spi(QUANTITY_MANY, lit("x"), (void *)univ_v0), f->univ0);
  const Term *bad_body = term_lan(heap, f->spi_x, univ_v1);
  const Term *bad_dom = term_ran(heap, shape_spi(QUANTITY_MANY, lit("x"), (void *)univ_v2), f->nat);
  const Term *bad_let = term_let(heap, lit("n"), f->nat, f->zero_g, univ_v0);
  int code = 0;
  code = code ? code : show(heap, "level_scope zero arity 0", render_result(heap, level_scope_check(heap, zero, f->univ0)));
  code = code ? code : show(heap, "level_scope var 0 arity 1", render_result(heap, level_scope_check(heap, one, ok_term)));
  code = code ? code : show(heap, "level_scope var 1 arity 1", render_result(heap, level_scope_check(heap, one, bad_body)));
  code = code ? code : show(heap, "level_scope domain var 2 arity 2", render_result(heap, level_scope_check(heap, two, bad_dom)));
  code = code ? code : show(heap, "level_scope succ var 0 arity 1", render_result(heap, level_scope_check(heap, one, univ_s)));
  code = code ? code : show(heap, "level_scope let body var 0 arity 0", render_result(heap, level_scope_check(heap, zero, bad_let)));
  return code;
}

static int context_lines(Heap *heap) {
  const Value *ty = value_vuniv(heap, level_zero());
  const Value *val = value_vuniv(heap, level_one(heap));
  Ctx ctx0 = context_make(heap, global_empty(), budget_unlimited(), heap_nat_of_u64(heap, 2));
  Ctx c1 = context_bind(heap, lit("x"), QUANTITY_MANY, ty, ctx0);
  Ctx c2 = context_bind(heap, lit("y"), QUANTITY_ONE, ty, c1);
  Ctx c3 = context_define(heap, lit("z"), QUANTITY_ZERO, ty, val, c2);
  Ctx c4 = context_bind(heap, lit("w"), QUANTITY_ONE, ty, c3);
  Ctx c5 = context_bind(heap, lit("v"), QUANTITY_MANY, ty, c4);
  Ctx c6 = context_bind(heap, lit("u"), QUANTITY_ZERO, ty, c5);
  Ctx c7 = context_bind(heap, lit("t"), QUANTITY_MANY, ty, c6);
  Vec names_0 = context_names_of(heap, ctx0);
  Vec names_3 = context_names_of(heap, c3);
  Vec names_7 = context_names_of(heap, c7);
  Vec list_7 = context_to_list(heap, c7.locals);
  const Locals *rebuilt = context_from_list(heap, &list_7);
  Vec rebuilt_list = context_to_list(heap, rebuilt);
  Vec rebuilt_names = context_names(heap, &rebuilt_list);
  Vec env_0 = context_env_of(ctx0);
  Vec env_7 = context_env_of(c7);
  int code = 0;
  code = code ? code : show(heap, "context make size arity checked",
                            cat3(heap, cat(heap, heap_nat_show(heap, context_size_of(ctx0)), lit(" ")),
                                 cat(heap, heap_nat_show(heap, ctx0.level_arity), lit(" ")), yes_no(ctx0.levels_checked)));
  code = code ? code : show(heap, "context make names", render_strings(heap, &names_0));
  code = code ? code : show(heap, "context make env len", show_len(heap, env_0.len));
  code = code ? code : show(heap, "context 7 size", heap_nat_show(heap, context_size_of(c7)));
  code = code ? code : show(heap, "context 7 names", render_strings(heap, &names_7));
  code = code ? code : show(heap, "context 7 env len", show_len(heap, env_7.len));
  code = code ? code : show(heap, "context 7 lget 0", render_local(heap, context_lget(heap, c7.locals, nat_zero())));
  code = code ? code : show(heap, "context 7 lget 3", render_local(heap, context_lget(heap, c7.locals, heap_nat_of_u64(heap, 3))));
  code = code ? code : show(heap, "context 7 lget 4", render_local(heap, context_lget(heap, c7.locals, heap_nat_of_u64(heap, 4))));
  code = code ? code : show(heap, "context 7 lget 6", render_local(heap, context_lget(heap, c7.locals, heap_nat_of_u64(heap, 6))));
  code = code ? code : show(heap, "context 7 lget 7", render_local(heap, context_lget(heap, c7.locals, heap_nat_of_u64(heap, 7))));
  code = code ? code : show(heap, "context 7 lview head", render_view(context_lview(heap, c7.locals)));
  code = code ? code : show(heap, "context 0 lview", render_view(context_lview(heap, ctx0.locals)));
  code = code ? code : show(heap, "context 3 names", render_strings(heap, &names_3));
  code = code ? code : show(heap, "context 3 lget 1", render_local(heap, context_lget(heap, c3.locals, heap_nat_of_u64(heap, 1))));
  code = code ? code : show(heap, "readable mode zero", readable_row(heap, QUANTITY_ZERO));
  code = code ? code : show(heap, "readable mode one", readable_row(heap, QUANTITY_ONE));
  code = code ? code : show(heap, "readable mode many", readable_row(heap, QUANTITY_MANY));
  code = code ? code : show(heap, "context from_list names", render_strings(heap, &rebuilt_names));
  code = code ? code : show(heap, "context from_list lget 4", render_local(heap, context_lget(heap, rebuilt, heap_nat_of_u64(heap, 4))));
  return code;
}

static int rules_lines(Heap *heap, const Fix *f) {
  Vec none = names0(heap);
  Ctx ctx = context_make(heap, global_empty(), budget_unlimited(), nat_zero());
  Vec two = list2(heap, f->nat, f->bool_g);
  Vec empty_terms = vec_new(heap->arena);
  Vec legs2 = rules_term_legs(heap, &two);
  Vec admitted = rules_admitted(heap);
  Vec declared = rules_named_declared(heap);
  Vec present = rules_named_present(heap);
  MaybeFamily no_family = {0};
  MaybeValue no_value;
  MaybeValue some_value;
  const Value *vuniv = value_vuniv(heap, level_zero());
  int code = 0;
  no_value.some = false;
  no_value.value = NULL;
  some_value.some = true;
  some_value.value = value_vuniv(heap, level_one(heap));
  code = code ? code : show(heap, "admitted", render_strings(heap, &admitted));
  code = code ? code : show(heap, "named_declared", render_strings(heap, &declared));
  code = code ? code : show(heap, "named_present", render_strings(heap, &present));
  code = code ? code : show(heap, "eta_of SPi", render_eta(heap, rules_eta_of(f->spi_x)));
  code = code ? code : show(heap, "eta_of SArg", render_eta(heap, rules_eta_of(f->sarg_y)));
  code = code ? code : show(heap, "eta_of SColl", render_eta(heap, rules_eta_of(f->scoll3)));
  code = code ? code : show(heap, "eta_of SPar", render_eta(heap, rules_eta_of(f->spar)));
  code = code ? code : show(heap, "eta_of SMu", render_eta(heap, rules_eta_of(f->smu)));
  code = code ? code : show(heap, "eta_of SNu", render_eta(heap, rules_eta_of(f->snu)));
  code = code ? code : show(heap, "eta_of SZk", render_eta(heap, rules_eta_of(f->szk)));
  code = code ? code : show(heap, "eta_of SFhc", render_eta(heap, rules_eta_of(f->sfhc)));
  code = code ? code : show(heap, "eta_of SMpc", render_eta(heap, rules_eta_of(f->smpc)));
  code = code ? code : show(heap, "arrow", pp_term(heap, &none, rules_arrow(heap, QUANTITY_ZERO, lit("q"), f->nat, f->univ0)));
  code = code ? code : show(heap, "unit_ty", pp_term(heap, &none, rules_unit_ty(heap, level_zero())));
  code = code ? code : show(heap, "unit_val", pp_term(heap, &none, rules_unit_val(heap)));
  code = code ? code : show(heap, "bool_ty", pp_term(heap, &none, rules_bool_ty(heap)));
  code = code ? code : show(heap, "prod_ty 2", pp_term(heap, &none, rules_prod_ty(heap, &two)));
  code = code ? code : show(heap, "sum_ty 2", pp_term(heap, &none, rules_sum_ty(heap, &two)));
  code = code ? code : show(heap, "prod_ty 0", pp_term(heap, &none, rules_prod_ty(heap, &empty_terms)));
  code = code ? code : show(heap, "mpc_sub_ty 2", render_term_result(heap, &none, rules_mpc_sub_ty(heap, big_of_u32(heap, 2))));
  code = code ? code : show(heap, "mpc_sub_ty 0", render_term_result(heap, &none, rules_mpc_sub_ty(heap, big_zero())));
  code = code ? code : show(heap, "mpc_sub_ty -1", render_term_result(heap, &none, rules_mpc_sub_ty(heap, big_neg(big_one()))));
  code = code ? code : show(heap, "mpc_pred_ty 1", render_term_result(heap, &none, rules_mpc_pred_ty(heap, big_one())));
  code = code ? code : show(heap, "mpc_pred_ty -2", render_term_result(heap, &none, rules_mpc_pred_ty(heap, big_neg(big_of_u32(heap, 2)))));
  code = code ? code : show(heap, "mpc_party_count 0", render_result(heap, rules_mpc_party_count(big_zero())));
  code = code ? code : show(heap, "mpc_party_count 1", render_result(heap, rules_mpc_party_count(big_one())));
  code = code ? code : show(heap, "mpc_party_value VUniv", render_result(heap, rules_mpc_party_value(vuniv)));
  code = code ? code : show(heap, "coll_legs VUniv", render_vec_result(heap, rules_coll_legs(heap, vuniv)));
  code = code ? code : show(heap, "family_found none", render_family_result(heap, rules_family_found(heap, no_family, lit("Tree"))));
  code = code ? code : show(heap, "mu_family empty globals", render_family_result(heap, rules_mu_family(heap, ctx, lit("List"))));
  code = code ? code : show(heap, "elim_result none none", render_result(heap, rules_elim_result(heap, ctx, term_motive_none(), no_value, vuniv)));
  code = code ? code : show(heap, "elim_result none some", render_result(heap, rules_elim_result(heap, ctx, term_motive_none(), some_value, vuniv)));
  code = code ? code : show(heap, "term_legs 2", render_leg_arities(heap, &legs2));
  return code;
}

static int run(Heap *heap) {
  Fix f = fix_new(heap);
  int code = pp_lines(heap, &f);
  code = code ? code : level_scope_lines(heap, &f);
  code = code ? code : context_lines(heap);
  code = code ? code : rules_lines(heap, &f);
  if (code == 0 && heap->oom) return driver_host_failure();
  return code;
}

static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  int code;
  (void)argv;
  if (argc != 1) return driver_die(2, "expected no arguments");
  code = run(&heap);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
