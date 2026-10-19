/* prelude/equality.bend: the MechEq family with refl, transport, j, symm, trans and congr,
 * and the MechTypeEq family with refl, cast, symm and trans, as one Family catalog. */
#include "prelude.h"

static Nat nat(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, nat(heap, index)); }

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL || !vec_push(items, (void *)item)) heap->oom = true;
}

static TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  return positivity_binder_new(heap, quantity, str_lit(name), ty);
}

static TeleBinder *zero(Heap *heap, const char *name, const Term *ty) { return binder(heap, QUANTITY_ZERO, name, ty); }

static TeleBinder *many(Heap *heap, const char *name, const Term *ty) { return binder(heap, QUANTITY_MANY, name, ty); }

static Vec terms1(Heap *heap, const Term *a) {
  Vec items = vec_new(heap->arena);
  push(heap, &items, a);
  return items;
}

static Vec terms2(Heap *heap, const Term *a, const Term *b) {
  Vec items = terms1(heap, a);
  push(heap, &items, b);
  return items;
}

static Vec names1(Heap *heap, const char *name) {
  Vec items = vec_new(heap->arena);
  Str *boxed = heap_alloc(heap, sizeof(Str));
  if (boxed != NULL) *boxed = str_lit(name);
  push(heap, &items, boxed);
  return items;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *boxed = heap_alloc(heap, sizeof(Decl));
  if (boxed != NULL) *boxed = decl;
  return boxed;
}

static const Term *univ_succ(Heap *heap, const LevelVar *u) { return term_univ(heap, level_succ(heap, u)); }

static Str mech_eq(void) { return str_lit("MechEq"); }

static Str mech_type_eq(void) { return str_lit("MechTypeEq"); }

Result equality_variable(Heap *heap, Nat index) { return dependent_variable(heap, index); }

const Term *equality_family(Heap *heap, Str name, const Vec *params, Vec indices) {
  return term_lan(heap, shape_smu(name, indices), rules_diagram_of(heap, params));
}

const Term *equality_erased_apply(Heap *heap, const Term *domain, const Term *head, const Term *argument) {
  return term_out(heap, shape_spi(QUANTITY_ZERO, str_lit("point"), (void *)domain), term_addr_pt(QUANTITY_ZERO, argument), head);
}

const Term *equality_reflexive(Heap *heap, Str name, const Term *index) {
  return term_in(heap, shape_smu(name, terms1(heap, index)), term_addr_ctor(str_lit("mechReflCtor")), vec_new(heap->arena));
}

const Term *equality_eliminate_at(Heap *heap, Str name, const Term *index, const Term *motive, const Term *scrut,
                                  const Term *branch) {
  MaybeStr ind;
  Vec branches = vec_new(heap->arena);
  ind.some = true;
  ind.value = name;
  push(heap, &branches, term_branch_new(heap, term_addr_ctor(str_lit("mechReflCtor")), term_leg(vec_new(heap->arena), branch)));
  return term_elim(heap, term_elim_data(shape_smu(name, terms1(heap, index)), scrut, QUANTITY_ONE,
                                        term_motive_some(term_motive(ind, names1(heap, "right"), str_lit("self"), motive)),
                                        branches));
}

const Term *equality_eliminate(Heap *heap, Str name, const Term *index, const Term *motive) {
  return equality_eliminate_at(heap, name, index, motive, var(heap, 1), var(heap, 0));
}

const Term *equality_eq(Heap *heap, const Term *carrier, const Term *left, const Term *right) {
  Vec params = terms2(heap, carrier, left);
  return equality_family(heap, mech_eq(), &params, terms1(heap, right));
}

const Term *equality_at_motive(Heap *heap, const Term *carrier, const Term *left, const Term *motive, const Term *right,
                               const Term *proof) {
  return equality_erased_apply(heap, equality_eq(heap, carrier, left, right), equality_erased_apply(heap, carrier, motive, right),
                               proof);
}

Decl equality_refl(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "x", var(heap, 0)));
  return dependent_definition(heap, str_lit("refl"), &binders, equality_eq(heap, var(heap, 1), var(heap, 0), var(heap, 0)),
                              equality_reflexive(heap, mech_eq(), var(heap, 0)));
}

Decl equality_transport(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec binders = vec_new(heap->arena);
  TeleBinder point = positivity_binder(QUANTITY_MANY, str_lit("point"), var(heap, 0));
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "B", dependent_arrow(heap, point, term_univ(heap, v))));
  push(heap, &binders, zero(heap, "x", var(heap, 1)));
  push(heap, &binders, zero(heap, "y", var(heap, 2)));
  push(heap, &binders, many(heap, "proof", equality_eq(heap, var(heap, 3), var(heap, 1), var(heap, 0))));
  push(heap, &binders, many(heap, "value", dependent_apply(heap, var(heap, 4), var(heap, 3), var(heap, 2))));
  return dependent_definition(heap, str_lit("transport"), &binders, dependent_apply(heap, var(heap, 5), var(heap, 4), var(heap, 2)),
                              equality_eliminate(heap, mech_eq(), var(heap, 2),
                                                 dependent_apply(heap, var(heap, 7), var(heap, 6), var(heap, 1))));
}

Decl equality_j(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec binders = vec_new(heap->arena);
  TeleBinder right = positivity_binder(QUANTITY_ZERO, str_lit("right"), var(heap, 1));
  TeleBinder proof = positivity_binder(QUANTITY_ZERO, str_lit("proof"), equality_eq(heap, var(heap, 2), var(heap, 1), var(heap, 0)));
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "x", var(heap, 0)));
  push(heap, &binders, zero(heap, "P", dependent_arrow(heap, right, dependent_arrow(heap, proof, term_univ(heap, v)))));
  push(heap, &binders,
       many(heap, "reflCase",
            equality_at_motive(heap, var(heap, 2), var(heap, 1), var(heap, 0), var(heap, 1), equality_reflexive(heap, mech_eq(), var(heap, 1)))));
  push(heap, &binders, zero(heap, "y", var(heap, 3)));
  push(heap, &binders, many(heap, "proof", equality_eq(heap, var(heap, 4), var(heap, 3), var(heap, 0))));
  return dependent_definition(
      heap, str_lit("j"), &binders, equality_at_motive(heap, var(heap, 5), var(heap, 4), var(heap, 3), var(heap, 1), var(heap, 0)),
      equality_eliminate_at(heap, mech_eq(), var(heap, 1),
                            equality_at_motive(heap, var(heap, 7), var(heap, 6), var(heap, 5), var(heap, 1), var(heap, 0)), var(heap, 0),
                            var(heap, 2)));
}

Decl equality_symm(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "x", var(heap, 0)));
  push(heap, &binders, zero(heap, "y", var(heap, 1)));
  push(heap, &binders, many(heap, "proof", equality_eq(heap, var(heap, 2), var(heap, 1), var(heap, 0))));
  return dependent_definition(heap, str_lit("symm"), &binders, equality_eq(heap, var(heap, 3), var(heap, 1), var(heap, 2)),
                              equality_eliminate_at(heap, mech_eq(), var(heap, 1), equality_eq(heap, var(heap, 5), var(heap, 1), var(heap, 4)),
                                                    var(heap, 0), equality_reflexive(heap, mech_eq(), var(heap, 2))));
}

Decl equality_trans(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "x", var(heap, 0)));
  push(heap, &binders, zero(heap, "y", var(heap, 1)));
  push(heap, &binders, zero(heap, "z", var(heap, 2)));
  push(heap, &binders, many(heap, "first", equality_eq(heap, var(heap, 3), var(heap, 2), var(heap, 1))));
  push(heap, &binders, many(heap, "second", equality_eq(heap, var(heap, 4), var(heap, 2), var(heap, 1))));
  return dependent_definition(heap, str_lit("trans"), &binders, equality_eq(heap, var(heap, 5), var(heap, 4), var(heap, 2)),
                              equality_eliminate_at(heap, mech_eq(), var(heap, 2), equality_eq(heap, var(heap, 7), var(heap, 6), var(heap, 1)),
                                                    var(heap, 0), var(heap, 1)));
}

Decl equality_congr(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  TeleBinder point = positivity_binder(QUANTITY_MANY, str_lit("point"), var(heap, 1));
  push(heap, &binders, zero(heap, "A", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "B", term_univ(heap, u)));
  push(heap, &binders, zero(heap, "f", dependent_arrow(heap, point, var(heap, 1))));
  push(heap, &binders, zero(heap, "x", var(heap, 2)));
  push(heap, &binders, zero(heap, "y", var(heap, 3)));
  push(heap, &binders, many(heap, "proof", equality_eq(heap, var(heap, 4), var(heap, 1), var(heap, 0))));
  return dependent_definition(
      heap, str_lit("congr"), &binders,
      equality_eq(heap, var(heap, 4), dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2)),
                  dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 1))),
      equality_eliminate_at(heap, mech_eq(), var(heap, 1),
                            equality_eq(heap, var(heap, 6), dependent_apply(heap, var(heap, 7), var(heap, 5), var(heap, 4)),
                                        dependent_apply(heap, var(heap, 7), var(heap, 5), var(heap, 1))),
                            var(heap, 0), equality_reflexive(heap, mech_eq(), dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2)))));
}

const Term *equality_type_eq(Heap *heap, const Term *left, const Term *right) {
  Vec params = terms1(heap, left);
  return equality_family(heap, mech_type_eq(), &params, terms1(heap, right));
}

Decl equality_type_refl(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", univ_succ(heap, u)));
  return dependent_definition(heap, str_lit("refl"), &binders, equality_type_eq(heap, var(heap, 0), var(heap, 0)),
                              equality_reflexive(heap, mech_type_eq(), var(heap, 0)));
}

Decl equality_cast(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", univ_succ(heap, u)));
  push(heap, &binders, zero(heap, "B", univ_succ(heap, u)));
  push(heap, &binders, many(heap, "proof", equality_type_eq(heap, var(heap, 1), var(heap, 0))));
  push(heap, &binders, many(heap, "value", var(heap, 2)));
  return dependent_definition(heap, str_lit("cast"), &binders, var(heap, 2),
                              equality_eliminate(heap, mech_type_eq(), var(heap, 2), var(heap, 1)));
}

Decl equality_type_symm(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", univ_succ(heap, u)));
  push(heap, &binders, zero(heap, "B", univ_succ(heap, u)));
  push(heap, &binders, many(heap, "proof", equality_type_eq(heap, var(heap, 1), var(heap, 0))));
  return dependent_definition(heap, str_lit("symm"), &binders, equality_type_eq(heap, var(heap, 1), var(heap, 2)),
                              equality_eliminate_at(heap, mech_type_eq(), var(heap, 1), equality_type_eq(heap, var(heap, 1), var(heap, 4)),
                                                    var(heap, 0), equality_reflexive(heap, mech_type_eq(), var(heap, 2))));
}

Decl equality_type_trans(Heap *heap, const LevelVar *u) {
  Vec binders = vec_new(heap->arena);
  push(heap, &binders, zero(heap, "A", univ_succ(heap, u)));
  push(heap, &binders, zero(heap, "B", univ_succ(heap, u)));
  push(heap, &binders, zero(heap, "C", univ_succ(heap, u)));
  push(heap, &binders, many(heap, "first", equality_type_eq(heap, var(heap, 2), var(heap, 1))));
  push(heap, &binders, many(heap, "second", equality_type_eq(heap, var(heap, 2), var(heap, 1))));
  return dependent_definition(heap, str_lit("trans"), &binders, equality_type_eq(heap, var(heap, 4), var(heap, 2)),
                              equality_eliminate_at(heap, mech_type_eq(), var(heap, 2), equality_type_eq(heap, var(heap, 6), var(heap, 1)),
                                                    var(heap, 0), var(heap, 1)));
}

Comp equality_declare_equality(Heap *heap, FamilyRaw raw, Globals globals, const Vec *members) {
  return family_declare_comp(heap, globals, family_empty(), nat(heap, 2), raw.family, raw.ctors, members);
}

typedef struct {
  Globals globals;
  const LevelVar *u;
} CatalogFrame;

/* The member list crosses into the deferred declare chain by pointer, so it lives on the heap. */
static Vec *vec_box(Heap *heap) {
  Vec *boxed = heap_alloc(heap, sizeof(Vec));
  if (boxed != NULL) *boxed = vec_new(heap->arena);
  return boxed;
}

/* The second bind of catalog_comp: MechTypeEq with its four members over the catalog holding MechEq. */
static Comp after_mech_eq(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  FamilyDecl type_eq;
  CtorDecl *refl = heap_alloc(heap, sizeof(CtorDecl));
  Vec ctors = vec_new(heap->arena);
  Vec *members = vec_box(heap);
  if (members == NULL) return budget_failed(error_host_oom());
  type_eq.name = mech_type_eq();
  type_eq.params = vec_new(heap->arena);
  type_eq.indices = vec_new(heap->arena);
  type_eq.level = level_zero();
  push(heap, &type_eq.params, zero(heap, "A", univ_succ(heap, frame->u)));
  push(heap, &type_eq.indices, zero(heap, "B", univ_succ(heap, frame->u)));
  if (refl == NULL) return budget_failed(error_host_oom());
  refl->name = str_lit("mechReflCtor");
  refl->args = vec_new(heap->arena);
  refl->res_params = terms1(heap, var(heap, 0));
  refl->res_idx = terms1(heap, var(heap, 0));
  push(heap, &ctors, refl);
  push(heap, members, decl_box(heap, equality_type_refl(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_cast(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_type_symm(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_type_trans(heap, frame->u)));
  return family_declare_comp(heap, frame->globals, (FamilyCatalog)value, nat(heap, 1), type_eq, ctors, members);
}

Comp equality_catalog_comp(Heap *heap, Globals globals) {
  CatalogFrame *frame = heap_alloc(heap, sizeof(CatalogFrame));
  const LevelVar *v = level_var(heap, nat(heap, 1));
  Vec *members = vec_box(heap);
  NextK next;
  if (frame == NULL || members == NULL) return budget_failed(error_host_oom());
  frame->globals = globals;
  frame->u = level_var(heap, nat(heap, 0));
  push(heap, members, decl_box(heap, equality_refl(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_transport(heap, frame->u, v)));
  push(heap, members, decl_box(heap, equality_j(heap, frame->u, v)));
  push(heap, members, decl_box(heap, equality_symm(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_trans(heap, frame->u)));
  push(heap, members, decl_box(heap, equality_congr(heap, frame->u)));
  next.fn = after_mech_eq;
  next.ctx = frame;
  return budget_bind(heap, equality_declare_equality(heap, congruence_equality(heap, mech_eq(), frame->u), globals, members), next);
}

Result equality_catalog(Heap *heap, Globals globals, Budget budget) {
  return budget_run(heap, equality_catalog_comp(heap, globals), budget);
}
