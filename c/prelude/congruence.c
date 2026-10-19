/* prelude/congruence.bend: the MechCongr and Result equality families declared as one
 * elaborated group with the congr member. */
#include "prelude.h"

static Nat nat(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, nat(heap, index)); }

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL || !vec_push(items, (void *)item)) heap->oom = true;
}

static TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  return positivity_binder_new(heap, quantity, str_lit(name), ty);
}

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

Result congruence_variable(Heap *heap, Nat index) { return dependent_variable(heap, index); }

/* Term.Lan{Shape.SMu{name, [right]}, Rules.diagram_of([carrier, left])} */
const Term *congruence_eq(Heap *heap, Str name, const Term *carrier, const Term *left, const Term *right) {
  Vec params = terms2(heap, carrier, left);
  return term_lan(heap, shape_smu(name, terms1(heap, right)), rules_diagram_of(heap, &params));
}

FamilyRaw congruence_equality(Heap *heap, Str name, const LevelVar *level) {
  FamilyRaw raw;
  CtorDecl *refl = heap_alloc(heap, sizeof(CtorDecl));
  raw.family.name = name;
  raw.family.params = vec_new(heap->arena);
  raw.family.indices = vec_new(heap->arena);
  raw.family.level = level_zero();
  raw.ctors = vec_new(heap->arena);
  push(heap, &raw.family.params, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level)));
  push(heap, &raw.family.params, binder(heap, QUANTITY_ZERO, "x", var(heap, 0)));
  push(heap, &raw.family.indices, binder(heap, QUANTITY_ZERO, "y", var(heap, 1)));
  if (refl == NULL) {
    heap->oom = true;
    return raw;
  }
  refl->name = str_lit("mechReflCtor");
  refl->args = vec_new(heap->arena);
  refl->res_params = terms2(heap, var(heap, 1), var(heap, 0));
  refl->res_idx = terms1(heap, var(heap, 0));
  push(heap, &raw.ctors, refl);
  return raw;
}

Decl congruence_congr(Heap *heap, const LevelVar *u, const LevelVar *v) {
  Vec binders = vec_new(heap->arena);
  Vec branches = vec_new(heap->arena);
  TeleBinder point = positivity_binder(QUANTITY_MANY, str_lit("point"), var(heap, 1));
  MaybeStr ind;
  Str congr = str_lit("MechCongr");
  Str result = str_lit("Result");
  Str refl = str_lit("mechReflCtor");
  Branch *branch = NULL;
  ind.some = true;
  ind.value = congr;
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, u)));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, v)));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "f", dependent_arrow(heap, point, var(heap, 1))));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "x", var(heap, 2)));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "y", var(heap, 3)));
  push(heap, &binders, binder(heap, QUANTITY_MANY, "proof", congruence_eq(heap, congr, var(heap, 4), var(heap, 1), var(heap, 0))));
  branch = term_branch_new(
      heap, term_addr_ctor(refl),
      term_leg(vec_new(heap->arena),
               term_in(heap, shape_smu(result, terms1(heap, dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2)))),
                       term_addr_ctor(refl), vec_new(heap->arena))));
  push(heap, &branches, branch);
  return dependent_definition(
      heap, str_lit("congr"), &binders,
      congruence_eq(heap, result, var(heap, 4), dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 2)),
                    dependent_apply(heap, var(heap, 5), var(heap, 3), var(heap, 1))),
      term_elim(heap, term_elim_data(shape_smu(congr, terms1(heap, var(heap, 1))), var(heap, 0), QUANTITY_ONE,
                                     term_motive_some(term_motive(
                                         ind, names1(heap, "right"), str_lit("self"),
                                         congruence_eq(heap, result, var(heap, 6),
                                                       dependent_apply(heap, var(heap, 7), var(heap, 5), var(heap, 4)),
                                                       dependent_apply(heap, var(heap, 7), var(heap, 5), var(heap, 1))))),
                                     branches)));
}

static FamilyRaw *raw_box(Heap *heap, FamilyRaw raw) {
  FamilyRaw *boxed = heap_alloc(heap, sizeof(FamilyRaw));
  if (boxed != NULL) *boxed = raw;
  return boxed;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *boxed = heap_alloc(heap, sizeof(Decl));
  if (boxed != NULL) *boxed = decl;
  return boxed;
}

/* The member list crosses into the deferred declare chain by pointer, so it lives on the heap. */
static Vec *vec_box(Heap *heap) {
  Vec *boxed = heap_alloc(heap, sizeof(Vec));
  if (boxed != NULL) *boxed = vec_new(heap->arena);
  return boxed;
}

Comp congruence_catalog_comp(Heap *heap, Globals globals) {
  const LevelVar *u = level_var(heap, nat(heap, 0));
  const LevelVar *v = level_var(heap, nat(heap, 1));
  Vec families = vec_new(heap->arena);
  Vec *decls = vec_box(heap);
  Vec *members = vec_box(heap);
  if (decls == NULL || members == NULL) return budget_failed(error_host_oom());
  push(heap, &families, raw_box(heap, congruence_equality(heap, str_lit("MechCongr"), u)));
  push(heap, &families, raw_box(heap, congruence_equality(heap, str_lit("Result"), v)));
  push(heap, decls, decl_box(heap, congruence_congr(heap, u, v)));
  *members = family_raw_callbacks(heap, decls);
  return family_declare_group_elaborated_comp(heap, globals, family_empty(), nat(heap, 2), &families, members);
}

Result congruence_catalog(Heap *heap, Globals globals, Budget budget) {
  return budget_run(heap, congruence_catalog_comp(heap, globals), budget);
}
