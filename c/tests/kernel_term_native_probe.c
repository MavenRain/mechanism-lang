/* C-only probe of c/kernel/term.c, value.c, prim.c and positivity.c (slice S1.4). No Bend test
 * driver exercises these four files before the checker lands, so there is no oracle record:
 * c-test runs this binary with no arguments and compares the output with
 * c/tests/kernel_term_native_probe.expected, whose lines were checked by hand against the Bend
 * sources. Every line is `label: value`. */
#include "driver.h"

static Str cat(Heap *heap, Str a, Str b) {
  return heap_str_cat(heap, a, b);
}

static Str lit(const char *text) {
  return str_lit(text);
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

static void push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

static Vec names1(Heap *heap, const char *name) {
  Vec out = vec_new(heap->arena);
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed == NULL) return out;
  *boxed = lit(name);
  push(heap, &out, boxed);
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

static const char *kind_name(TermKind kind) {
  switch (kind) {
    case TERM_VAR: return "var";
    case TERM_UNIV: return "univ";
    case TERM_LAN: return "lan";
    case TERM_RAN: return "ran";
    case TERM_IN: return "in";
    case TERM_ELIM: return "elim";
    case TERM_SEC: return "sec";
    case TERM_OUT: return "out";
    case TERM_LET: return "let";
    case TERM_ANN: return "ann";
    case TERM_GLOBAL: return "global";
    case TERM_LIT: return "lit";
    case TERM_AUTO: return "auto";
  }
  return "?";
}

static const char *quantity_name(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return "zero";
    case QUANTITY_ONE: return "one";
    case QUANTITY_MANY: return "many";
  }
  return "?";
}

static Str render_term(Heap *heap, const Term *term);

static Str render_shape(Heap *heap, Shape s) {
  switch (s.kind) {
    case SHAPE_PI:
      return cat3(heap, cat3(heap, lit("Pi("), lit(quantity_name(s.quantity)), lit(" ")),
                  cat(heap, s.name, lit(": ")), cat(heap, render_term(heap, s.domain), lit(")")));
    case SHAPE_ZK: return lit("Zk");
    case SHAPE_ARG: return lit("Arg");
    case SHAPE_COLL: return cat3(heap, lit("Coll("), big_to_string(heap, s.count), lit(")"));
    case SHAPE_PAR: return lit("Par");
    case SHAPE_MU: return cat3(heap, lit("Mu("), s.name, lit(")"));
    case SHAPE_NU: return cat3(heap, lit("Nu("), s.name, lit(")"));
    case SHAPE_FHC: return lit("Fhc");
    case SHAPE_MPC: return lit("Mpc");
  }
  return lit("?");
}

static Str render_legs(Heap *heap, const Vec *legs) {
  Str out = lit("[");
  size_t i;
  for (i = 0; i < legs->len; i++) {
    VecGet got = vec_get(legs, i);
    const Leg *leg = got.status == VEC_GET_OK ? got.value : NULL;
    if (leg == NULL) return lit("[?]");
    out = cat(heap, out, i == 0 ? lit("") : lit(", "));
    out = cat3(heap, out, cat3(heap, lit("Leg("), heap_nat_show(heap, heap_nat_of_u64(heap, leg->binders.len)), lit(", ")),
               cat(heap, render_term(heap, leg->body), lit(")")));
  }
  return cat(heap, out, lit("]"));
}

static Str render_term(Heap *heap, const Term *term) {
  switch (term->kind) {
    case TERM_GLOBAL: return term->name;
    case TERM_UNIV: return cat(heap, lit("U"), level_to_string(heap, term->level));
    case TERM_RAN:
      return cat3(heap, cat3(heap, lit("Ran("), render_shape(heap, term->shape), lit(", ")), render_term(heap, term->body), lit(")"));
    case TERM_LAN:
      return cat3(heap, cat3(heap, lit("Lan("), render_shape(heap, term->shape), lit(", ")), render_term(heap, term->body), lit(")"));
    case TERM_SEC:
      return cat3(heap, cat3(heap, lit("Sec("), render_shape(heap, term->shape), lit(", ")), render_legs(heap, &term->legs), lit(")"));
    case TERM_ANN:
      return cat3(heap, cat3(heap, lit("Ann("), render_term(heap, term->body), lit(" : ")), render_term(heap, term->ty), lit(")"));
    case TERM_VAR: return cat(heap, lit("#"), heap_nat_show(heap, term->index));
    case TERM_IN: return lit("in");
    case TERM_ELIM: return lit("elim");
    case TERM_OUT: return lit("out");
    case TERM_LET: return lit("let");
    case TERM_LIT: return lit("lit");
    case TERM_AUTO: return lit("auto");
  }
  return lit("?");
}

static Str render_value(Heap *heap, const Value *value) {
  switch (value->kind) {
    case VALUE_LIT:
      if (value->lit.kind == LITERAL_INT) return cat(heap, lit("lit "), big_to_string(heap, value->lit.integer));
      return cat(heap, lit("lit string "), value->lit.string);
    case VALUE_IN:
      if (value->addr.kind == VADDR_LEG) {
        return cat3(heap, cat(heap, lit("in leg "), big_to_string(heap, value->addr.index)), lit(" args "),
                    heap_nat_show(heap, heap_nat_of_u64(heap, value->args.len)));
      }
      return lit("in");
    case VALUE_UNIV: return lit("univ");
    case VALUE_LAN: return lit("lan");
    case VALUE_RAN: return lit("ran");
    case VALUE_SEC: return lit("sec");
    case VALUE_NEUTRAL:
      if (value->head.kind == HEAD_LOCAL) return cat(heap, lit("neutral local "), heap_nat_show(heap, value->head.level));
      return cat(heap, lit("neutral global "), value->head.name);
  }
  return lit("?");
}

static Str render_maybe_value(Heap *heap, MaybeValue v) {
  return v.some ? render_value(heap, v.value) : lit("none");
}

static Str render_kinds(Heap *heap, const Vec *terms) {
  Str out = heap_nat_show(heap, heap_nat_of_u64(heap, terms->len));
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    const Term *t = got.status == VEC_GET_OK ? got.value : NULL;
    out = cat3(heap, out, lit(" "), lit(t == NULL ? "?" : kind_name(t->kind)));
  }
  return out;
}

static Str render_strings(Heap *heap, const Vec *names) {
  Str out = heap_nat_show(heap, heap_nat_of_u64(heap, names->len));
  size_t i;
  for (i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    const Str *s = got.status == VEC_GET_OK ? got.value : NULL;
    out = cat3(heap, out, lit(" "), s == NULL ? lit("?") : *s);
  }
  return out;
}

static Str render_result(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_DONE: return lit("done");
    case RESULT_FAIL: return cat(heap, lit(r.error.kind == ERROR_NOT_YET ? "fail not_yet: " : "fail: "), r.error.message);
  }
  return lit("?");
}

static const Literal *boxed_lit(Heap *heap, Literal value) {
  Literal *boxed = heap_alloc(heap, sizeof(Literal));
  if (boxed != NULL) *boxed = value;
  return boxed;
}

static Vec int_args(Heap *heap, uint32_t a, uint32_t b) {
  return list2(heap, boxed_lit(heap, literal_lint(big_of_u32(heap, a))), boxed_lit(heap, literal_lint(big_of_u32(heap, b))));
}

static Str apply(Heap *heap, Prim p, uint32_t a, uint32_t b) {
  Vec args = int_args(heap, a, b);
  return render_maybe_value(heap, prim_apply(heap, p, &args));
}

static int term_lines(Heap *heap) {
  const Term *nat = prim_nat_ty(heap);
  const Term *tree = term_global(heap, str_lit("Tree"));
  const Term *pi_nat_tree = term_ran(heap, shape_spi(QUANTITY_MANY, lit("x"), (void *)nat), tree);
  const Term *let_term = term_let(heap, lit("x"), term_univ(heap, level_zero()), term_lit(heap, literal_lint(big_one())), term_var(heap, nat_zero()));
  const Term *mu_tree = term_lan(heap, shape_smu(lit("Tree"), list1(heap, nat)), nat);
  Vec names = names1(heap, "Tree");
  Vec formers = term_formers(heap);
  Vec schema = term_schema(heap);
  Vec lan_children = term_children(heap, pi_nat_tree);
  Vec let_children = term_children(heap, let_term);
  Vec mu_children = term_children(heap, mu_tree);
  MaybeApt apt = term_as_apt(term_addr_pt(QUANTITY_ONE, nat));
  MaybeBig aleg = term_as_aleg(term_aleg(heap, heap_nat_of_u64(heap, 3)));
  int code = 0;
  code = code ? code : show(heap, "ctor kinds", cat3(heap, lit(kind_name(term_var(heap, nat_zero())->kind)), lit(" "), cat3(heap, lit(kind_name(term_auto()->kind)), lit(" "), lit(kind_name(let_term->kind)))));
  code = code ? code : show(heap, "children Ran(Pi Nat) Tree", render_kinds(heap, &lan_children));
  code = code ? code : show(heap, "children Let univ lit var", render_kinds(heap, &let_children));
  code = code ? code : show(heap, "children Lan(Mu Tree [Nat]) Nat", render_kinds(heap, &mu_children));
  code = code ? code : show(heap, "exists Tree in Ran(Pi Nat) Tree", yes_no(term_exists_name(heap, true, &names, pi_nat_tree)));
  code = code ? code : show(heap, "exists Tree in Nat", yes_no(term_exists_name(heap, true, &names, nat)));
  code = code ? code : show(heap, "exists Tree in Lan(Mu Tree) with families", yes_no(term_exists_name(heap, true, &names, mu_tree)));
  code = code ? code : show(heap, "exists Tree in Lan(Mu Tree) without families", yes_no(term_exists_name(heap, false, &names, mu_tree)));
  code = code ? code : show(heap, "equal bool_ty bool_ty", yes_no(term_equal(prim_bool_ty(heap), prim_bool_ty(heap))));
  code = code ? code : show(heap, "equal bool_ty unit_ty", yes_no(term_equal(prim_bool_ty(heap), prim_unit_ty(heap))));
  code = code ? code : show(heap, "formers", render_strings(heap, &formers));
  code = code ? code : show(heap, "schema", render_strings(heap, &schema));
  code = code ? code : show(heap, "as_apt one Nat", cat3(heap, yes_no(apt.some), lit(" "), lit(quantity_name(apt.quantity))));
  code = code ? code : show(heap, "as_aleg 3", aleg.some ? big_to_string(heap, aleg.value) : lit("none"));
  code = code ? code : show(heap, "as_actor of APt", yes_no(term_as_actor(term_addr_pt(QUANTITY_ONE, nat)).some));
  return code;
}

static int value_lines(Heap *heap) {
  const Value *var4 = value_var(heap, heap_nat_of_u64(heap, 4));
  const Value *univ = value_vuniv(heap, level_one(heap));
  MaybeLevelVar as_univ = value_as_univ(univ);
  int code = 0;
  code = code ? code : show(heap, "value_var 4", render_value(heap, var4));
  code = code ? code : show(heap, "as_univ VUniv 1", as_univ.some ? level_to_string(heap, as_univ.value) : lit("none"));
  code = code ? code : show(heap, "as_univ VLit", yes_no(value_as_univ(value_vlit(heap, literal_lint(big_zero()))).some));
  code = code ? code : show(heap, "head_equal local 1 local 1", yes_no(value_head_equal(value_hlocal(heap_nat_of_u64(heap, 1)), value_hlocal(heap_nat_of_u64(heap, 1)))));
  code = code ? code : show(heap, "head_equal local 1 local 2", yes_no(value_head_equal(value_hlocal(heap_nat_of_u64(heap, 1)), value_hlocal(heap_nat_of_u64(heap, 2)))));
  code = code ? code : show(heap, "head_equal local 1 global f", yes_no(value_head_equal(value_hlocal(heap_nat_of_u64(heap, 1)), value_hglobal(lit("f")))));
  code = code ? code : show(heap, "head_equal global f global f", yes_no(value_head_equal(value_hglobal(lit("f")), value_hglobal(lit("f")))));
  return code;
}

static int prim_lines(Heap *heap) {
  Vec catalog = prim_catalog(heap);
  Str names = lit("");
  size_t i;
  MaybePrim mul = prim_of_name(heap, lit("natMul"));
  MaybePrim div = prim_of_name(heap, lit("natDiv"));
  Vec one_arg = list1(heap, boxed_lit(heap, literal_lint(big_one())));
  Vec string_arg = list2(heap, boxed_lit(heap, literal_lstring(lit("a"))), boxed_lit(heap, literal_lint(big_one())));
  Vec negative = list2(heap, boxed_lit(heap, literal_lint(big_neg(big_one()))), boxed_lit(heap, literal_lint(big_one())));
  int code = 0;
  for (i = 0; i < catalog.len; i++) {
    VecGet got = vec_get(&catalog, i);
    const Prim *p = got.status == VEC_GET_OK ? got.value : NULL;
    names = cat3(heap, names, i == 0 ? lit("") : lit(" "), p == NULL ? lit("?") : prim_name(*p));
  }
  code = code ? code : show(heap, "prim names", names);
  code = code ? code : show(heap, "prim of_name natMul", mul.some ? prim_name(mul.value) : lit("none"));
  code = code ? code : show(heap, "prim of_name natDiv", div.some ? prim_name(div.value) : lit("none"));
  code = code ? code : show(heap, "prim arity", heap_nat_show(heap, prim_arity(heap, PRIM_NAT_LT)));
  code = code ? code : show(heap, "prim ty natAdd", render_term(heap, prim_ty(heap, PRIM_NAT_ADD)));
  code = code ? code : show(heap, "prim ty natEq", render_term(heap, prim_ty(heap, PRIM_NAT_EQ)));
  code = code ? code : show(heap, "apply natAdd 2 3", apply(heap, PRIM_NAT_ADD, 2, 3));
  code = code ? code : show(heap, "apply natSub 5 3", apply(heap, PRIM_NAT_SUB, 5, 3));
  code = code ? code : show(heap, "apply natSub 2 3", apply(heap, PRIM_NAT_SUB, 2, 3));
  code = code ? code : show(heap, "apply natMul 6 7", apply(heap, PRIM_NAT_MUL, 6, 7));
  code = code ? code : show(heap, "apply natEq 3 3", apply(heap, PRIM_NAT_EQ, 3, 3));
  code = code ? code : show(heap, "apply natEq 3 4", apply(heap, PRIM_NAT_EQ, 3, 4));
  code = code ? code : show(heap, "apply natLt 2 3", apply(heap, PRIM_NAT_LT, 2, 3));
  code = code ? code : show(heap, "apply natLt 3 2", apply(heap, PRIM_NAT_LT, 3, 2));
  code = code ? code : show(heap, "apply natLt 3 3", apply(heap, PRIM_NAT_LT, 3, 3));
  code = code ? code : show(heap, "apply natAdd one arg", render_maybe_value(heap, prim_apply(heap, PRIM_NAT_ADD, &one_arg)));
  code = code ? code : show(heap, "apply natAdd string arg", render_maybe_value(heap, prim_apply(heap, PRIM_NAT_ADD, &string_arg)));
  code = code ? code : show(heap, "apply natAdd negative arg", render_maybe_value(heap, prim_apply(heap, PRIM_NAT_ADD, &negative)));
  return code;
}

static int positivity_lines(Heap *heap) {
  const Term *nat = prim_nat_ty(heap);
  const Term *tree = term_global(heap, lit("Tree"));
  Vec names = names1(heap, "Tree");
  Shape pi_nat = shape_spi(QUANTITY_MANY, lit("x"), (void *)nat);
  Shape pi_tree = shape_spi(QUANTITY_MANY, lit("x"), (void *)tree);
  Shape coll2 = shape_scoll(big_of_u32(heap, 2));
  const Term *good_ran = term_ran(heap, pi_nat, tree);
  const Term *bad_ran = term_ran(heap, pi_tree, nat);
  const Term *coll_ran_tree = term_ran(heap, coll2, tree);
  const Term *coll_ran_nat = term_ran(heap, coll2, nat);
  const Term *mu_good = term_lan(heap, shape_smu(lit("Tree"), list1(heap, nat)), nat);
  const Term *mu_bad = term_lan(heap, shape_smu(lit("Tree"), list1(heap, tree)), nat);
  const Term *coll_lan_tree = term_lan(heap, coll2, tree);
  const Term *out_good = term_out(heap, pi_nat, term_addr_pt(QUANTITY_MANY, nat), tree);
  const Term *out_leg = term_out(heap, pi_nat, term_aleg(heap, nat_zero()), tree);
  const Term *out_coll = term_out(heap, coll2, term_addr_pt(QUANTITY_MANY, nat), tree);
  const Term *in_nat = term_in(heap, coll2, term_aleg(heap, nat_zero()), list1(heap, nat));
  const Term *in_tree = term_in(heap, coll2, term_aleg(heap, nat_zero()), list1(heap, tree));
  Vec fields_good = list2(heap, positivity_binder_new(heap, QUANTITY_MANY, lit("l"), tree), positivity_binder_new(heap, QUANTITY_MANY, lit("r"), tree));
  Vec fields_bad = list1(heap, positivity_binder_new(heap, QUANTITY_MANY, lit("f"), bad_ran));
  Vec fields_nat = list1(heap, positivity_binder_new(heap, QUANTITY_MANY, lit("n"), nat));
  Vec fields_nat_copy = list1(heap, positivity_binder_new(heap, QUANTITY_MANY, lit("n"), nat));
  Vec fields_one = list1(heap, positivity_binder_new(heap, QUANTITY_ONE, lit("n"), nat));
  Ctor leaf = positivity_ctor(lit("leaf"), vec_new(heap->arena), vec_new(heap->arena), nat_zero(), false);
  Ctor node = positivity_ctor(lit("node"), fields_good, vec_new(heap->arena), heap_nat_of_u64(heap, 2), true);
  Ctor node_flag = positivity_ctor(lit("node"), fields_good, vec_new(heap->arena), heap_nat_of_u64(heap, 2), false);
  Vec ctors = list2(heap, positivity_ctor_new(heap, leaf), positivity_ctor_new(heap, node));
  Vec ctors_flag = list2(heap, positivity_ctor_new(heap, leaf), positivity_ctor_new(heap, node_flag));
  Vec ctor_names = list2(heap, names1(heap, "leaf").items[0], names1(heap, "node").items[0]);
  Family family = positivity_family(lit("Tree"), fields_nat, vec_new(heap->arena), level_zero(), positivity_complete(ctor_names), ctors, true);
  Family same = positivity_family(lit("Tree"), fields_nat_copy, vec_new(heap->arena), level_zero(), positivity_complete(ctor_names), ctors, true);
  Family lifted = positivity_family(lit("Tree"), fields_nat, vec_new(heap->arena), level_one(heap), positivity_complete(ctor_names), ctors, true);
  Family flagged = positivity_family(lit("Tree"), fields_nat, vec_new(heap->arena), level_zero(), positivity_complete(ctor_names), ctors_flag, true);
  Family provisional = positivity_family(lit("Tree"), fields_nat, vec_new(heap->arena), level_zero(), positivity_provisional(), ctors, true);
  MaybeCtor found = positivity_ctor_of(lit("node"), family);
  MaybeCtor missing = positivity_ctor_of(lit("branch"), family);
  int code = 0;
  if (heap->oom) return driver_host_failure();
  code = code ? code : show(heap, "positive Global Tree", yes_no(positivity_positive(heap, tree, &names)));
  code = code ? code : show(heap, "positive Ran(Pi Nat) Tree", yes_no(positivity_positive(heap, good_ran, &names)));
  code = code ? code : show(heap, "positive Ran(Pi Tree) Nat", yes_no(positivity_positive(heap, bad_ran, &names)));
  code = code ? code : show(heap, "positive Ran(Coll 2) Tree", yes_no(positivity_positive(heap, coll_ran_tree, &names)));
  code = code ? code : show(heap, "positive Ran(Coll 2) Nat", yes_no(positivity_positive(heap, coll_ran_nat, &names)));
  code = code ? code : show(heap, "positive Lan(Mu Tree [Nat]) Nat", yes_no(positivity_positive(heap, mu_good, &names)));
  code = code ? code : show(heap, "positive Lan(Mu Tree [Tree]) Nat", yes_no(positivity_positive(heap, mu_bad, &names)));
  code = code ? code : show(heap, "positive Lan(Coll 2) Tree", yes_no(positivity_positive(heap, coll_lan_tree, &names)));
  code = code ? code : show(heap, "positive Out(Pi Nat) APt(Nat) Tree", yes_no(positivity_positive(heap, out_good, &names)));
  code = code ? code : show(heap, "positive Out(Pi Nat) ALeg(0) Tree", yes_no(positivity_positive(heap, out_leg, &names)));
  code = code ? code : show(heap, "positive Out(Coll 2) APt(Nat) Tree", yes_no(positivity_positive(heap, out_coll, &names)));
  code = code ? code : show(heap, "positive In(Coll 2) [Nat]", yes_no(positivity_positive(heap, in_nat, &names)));
  code = code ? code : show(heap, "positive In(Coll 2) [Tree]", yes_no(positivity_positive(heap, in_tree, &names)));
  code = code ? code : show(heap, "point_dom Pi Zk Coll", cat3(heap, yes_no(positivity_point_dom(pi_nat).some), cat(heap, lit(" "), yes_no(positivity_point_dom(shape_szk(QUANTITY_MANY, lit("z"), (void *)nat)).some)), cat(heap, lit(" "), yes_no(positivity_point_dom(coll2).some))));
  code = code ? code : show(heap, "fields_positive [l: Tree, r: Tree]", yes_no(positivity_fields_positive(heap, &fields_good, &names)));
  code = code ? code : show(heap, "fields_positive [f: Ran(Pi Tree) Nat]", yes_no(positivity_fields_positive(heap, &fields_bad, &names)));
  code = code ? code : show(heap, "ctor_fields good", render_result(heap, positivity_ctor_fields(heap, &names, &fields_good)));
  code = code ? code : show(heap, "ctor_fields bad", render_result(heap, positivity_ctor_fields(heap, &names, &fields_bad)));
  code = code ? code : show(heap, "self_rec [l: Tree, r: Tree]", yes_no(positivity_self_rec(heap, &fields_good, &names)));
  code = code ? code : show(heap, "self_rec [n: Nat]", yes_no(positivity_self_rec(heap, &fields_nat, &names)));
  code = code ? code : show(heap, "ctor_of node", found.some ? cat3(heap, found.value.name, lit(" arity "), heap_nat_show(heap, found.value.full_arity)) : lit("none"));
  code = code ? code : show(heap, "ctor_of branch", yes_no(missing.some));
  code = code ? code : show(heap, "same_telescope equal", yes_no(positivity_same_telescope(&fields_nat, &fields_nat_copy)));
  code = code ? code : show(heap, "same_telescope quantity differs", yes_no(positivity_same_telescope(&fields_nat, &fields_one)));
  code = code ? code : show(heap, "same_telescope length differs", yes_no(positivity_same_telescope(&fields_nat, &fields_good)));
  code = code ? code : show(heap, "same_status complete complete", yes_no(positivity_same_status(positivity_complete(ctor_names), positivity_complete(ctor_names))));
  code = code ? code : show(heap, "same_status provisional builtin", yes_no(positivity_same_status(positivity_provisional(), positivity_builtin())));
  code = code ? code : show(heap, "same_bool", cat3(heap, yes_no(positivity_same_bool(true, true)), lit(" "), cat3(heap, yes_no(positivity_same_bool(false, false)), lit(" "), yes_no(positivity_same_bool(true, false)))));
  code = code ? code : show(heap, "same_ctors self_rec differs strict", yes_no(positivity_same_ctors(&ctors, &ctors_flag, false)));
  code = code ? code : show(heap, "same_ctors self_rec differs ignored", yes_no(positivity_same_ctors(&ctors, &ctors_flag, true)));
  code = code ? code : show(heap, "same_family copy", yes_no(positivity_same_family(family, same)));
  code = code ? code : show(heap, "same_family level differs", yes_no(positivity_same_family(family, lifted)));
  code = code ? code : show(heap, "same_family_except ignore_level", yes_no(positivity_same_family_except(family, lifted, true, false)));
  code = code ? code : show(heap, "same_family status differs", yes_no(positivity_same_family(family, provisional)));
  code = code ? code : show(heap, "same_family_except ignore_self_rec", yes_no(positivity_same_family_except(family, flagged, false, true)));
  return code;
}

static int run(Heap *heap) {
  int code = term_lines(heap);
  code = code ? code : value_lines(heap);
  code = code ? code : prim_lines(heap);
  code = code ? code : positivity_lines(heap);
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
