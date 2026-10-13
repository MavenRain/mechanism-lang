/* bend2/prelude/families.bend: the MechEq equality family (one universe) and the MechSum family
 * (two universes) as a Family catalog. */
#include "prelude.h"

static Nat nat(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, nat(heap, index)); }

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL || !vec_push(items, (void *)item)) heap->oom = true;
}

static TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  return positivity_binder_new(heap, quantity, str_lit(name), ty);
}

Result families_variable(Heap *heap, Nat index) { return dependent_variable(heap, index); }

/* The member list crosses into the deferred declare chain by pointer, so it lives on the heap. */
static Vec *vec_box(Heap *heap) {
  Vec *boxed = heap_alloc(heap, sizeof(Vec));
  if (boxed != NULL) *boxed = vec_new(heap->arena);
  return boxed;
}

Comp families_declare_equality(Heap *heap, FamilyRaw raw, Globals globals) {
  Vec *members = vec_box(heap);
  if (members == NULL) return budget_failed(error_host_oom());
  return family_declare_comp(heap, globals, family_empty(), nat(heap, 1), raw.family, raw.ctors, members);
}

/* Check.CtorDecl{name, [P.Binder{Q.QMany{}, field, Term.Var{index}}], [Term.Var{2n}, Term.Var{1n}], []} */
static CtorDecl *injection(Heap *heap, const char *name, const char *field, uint64_t index) {
  CtorDecl *ctor = heap_alloc(heap, sizeof(CtorDecl));
  if (ctor == NULL) return NULL;
  ctor->name = str_lit(name);
  ctor->args = vec_new(heap->arena);
  ctor->res_params = vec_new(heap->arena);
  ctor->res_idx = vec_new(heap->arena);
  push(heap, &ctor->args, binder(heap, QUANTITY_MANY, field, var(heap, index)));
  push(heap, &ctor->res_params, var(heap, 2));
  push(heap, &ctor->res_params, var(heap, 1));
  return ctor;
}

typedef struct {
  Globals globals;
  const LevelVar *u;
  const LevelVar *v;
} CatalogFrame;

/* The second bind of catalog_comp: MechSum over the catalog holding MechEq. */
static Comp after_equality(Heap *heap, void *ctx, void *value) {
  CatalogFrame *frame = ctx;
  FamilyDecl sum;
  Vec ctors = vec_new(heap->arena);
  Vec *members = vec_box(heap);
  if (members == NULL) return budget_failed(error_host_oom());
  sum.name = str_lit("MechSum");
  sum.params = vec_new(heap->arena);
  sum.indices = vec_new(heap->arena);
  sum.level = level_max(heap, level_succ(heap, frame->u), level_succ(heap, frame->v));
  push(heap, &sum.params, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, frame->u))));
  push(heap, &sum.params, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, level_succ(heap, frame->v))));
  push(heap, &ctors, injection(heap, "mechInl", "left", 1));
  push(heap, &ctors, injection(heap, "mechInr", "right", 0));
  return family_declare_comp(heap, frame->globals, (FamilyCatalog)value, nat(heap, 2), sum, ctors, members);
}

Comp families_catalog_comp(Heap *heap, Globals globals) {
  CatalogFrame *frame = heap_alloc(heap, sizeof(CatalogFrame));
  NextK next;
  if (frame == NULL) return budget_failed(error_host_oom());
  frame->globals = globals;
  frame->u = level_var(heap, nat(heap, 0));
  frame->v = level_var(heap, nat(heap, 1));
  next.fn = after_equality;
  next.ctx = frame;
  return budget_bind(heap, families_declare_equality(heap, congruence_equality(heap, str_lit("MechEq"), frame->u), globals), next);
}

Result families_catalog(Heap *heap, Globals globals, Budget budget) {
  return budget_run(heap, families_catalog_comp(heap, globals), budget);
}
