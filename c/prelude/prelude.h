/* A <dir>/<file>.bend path in a comment names bend2/<dir>/<file>.bend in commit 8c90b64,
 * the last tree with the Bend sources. */
/* prelude ported to C: the trusted prelude catalogs. One section per Bend module
 * (dependent, congruence, equality, families), the same definition names in snake_case with the
 * module as prefix and the same definition order. Comp and Result payloads: a PolyCatalog or a
 * FamilyCatalog (pointer types, the surface precedent); the variable functions carry a
 * const LevelVar *. Allocation failure sets heap->oom and the constructors return their Auto or
 * empty placeholders (the kernel precedent). */
#ifndef MECH_PRELUDE_H
#define MECH_PRELUDE_H

#include "kernel.h"
#include "surface.h"

/* ---- dependent (prelude/dependent.bend) ---- */
Result dependent_variable(Heap *heap, Nat index);
const Term *dependent_arrow(Heap *heap, TeleBinder binder, const Term *body);
const Term *dependent_lambda(Heap *heap, TeleBinder binder, const Term *body);
/* binders: P.Telescope as a Vec of TeleBinder pointers. */
const Term *dependent_arrows(Heap *heap, const Vec *binders, const Term *body);
const Term *dependent_lambdas(Heap *heap, const Vec *binders, const Term *body);
Decl dependent_definition(Heap *heap, Str name, const Vec *binders, const Term *result, const Term *body);
Shape dependent_point(const Term *domain);
const Term *dependent_apply(Heap *heap, const Term *domain, const Term *head, const Term *argument);
const Term *dependent_sigma(Heap *heap, Nat a, Nat b);
const Term *dependent_pair(Heap *heap, Nat a, const Term *x, const Term *y);
/* branches: List<Pair<Addr, Leg>> as a Vec of Branch pointers. */
const Term *dependent_eliminate(Heap *heap, Nat a, const Term *scrut, const Term *motive, Vec branches);
Vec dependent_proj_branch(Heap *heap, Nat which);
const Term *dependent_first(Heap *heap, Nat a, const Term *scrut);
Decl dependent_pi(Heap *heap, const LevelVar *u, const LevelVar *v);
Vec dependent_parameters(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl dependent_sigma_type(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl dependent_make(Heap *heap, const LevelVar *u, const LevelVar *v);
Vec dependent_projections(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl dependent_fst(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl dependent_snd(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl dependent_recursor(Heap *heap, const LevelVar *u, const LevelVar *v, const LevelVar *w);
/* Done value: the PolyCatalog. */
Comp dependent_catalog_comp(Heap *heap, Globals globals);
Result dependent_catalog(Heap *heap, Globals globals, Budget budget);

/* ---- congruence (prelude/congruence.bend) ---- */
Result congruence_variable(Heap *heap, Nat index);
const Term *congruence_eq(Heap *heap, Str name, const Term *carrier, const Term *left, const Term *right);
FamilyRaw congruence_equality(Heap *heap, Str name, const LevelVar *level);
Decl congruence_congr(Heap *heap, const LevelVar *u, const LevelVar *v);
/* Done value: the FamilyCatalog. */
Comp congruence_catalog_comp(Heap *heap, Globals globals);
Result congruence_catalog(Heap *heap, Globals globals, Budget budget);

/* ---- equality (prelude/equality.bend) ---- */
Result equality_variable(Heap *heap, Nat index);
/* params and indices: Vecs of Term pointers. */
const Term *equality_family(Heap *heap, Str name, const Vec *params, Vec indices);
const Term *equality_erased_apply(Heap *heap, const Term *domain, const Term *head, const Term *argument);
const Term *equality_reflexive(Heap *heap, Str name, const Term *index);
const Term *equality_eliminate_at(Heap *heap, Str name, const Term *index, const Term *motive, const Term *scrut,
                                  const Term *branch);
const Term *equality_eliminate(Heap *heap, Str name, const Term *index, const Term *motive);
const Term *equality_eq(Heap *heap, const Term *carrier, const Term *left, const Term *right);
const Term *equality_at_motive(Heap *heap, const Term *carrier, const Term *left, const Term *motive,
                               const Term *right, const Term *proof);
Decl equality_refl(Heap *heap, const LevelVar *u);
Decl equality_transport(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl equality_j(Heap *heap, const LevelVar *u, const LevelVar *v);
Decl equality_symm(Heap *heap, const LevelVar *u);
Decl equality_trans(Heap *heap, const LevelVar *u);
Decl equality_congr(Heap *heap, const LevelVar *u);
const Term *equality_type_eq(Heap *heap, const Term *left, const Term *right);
Decl equality_type_refl(Heap *heap, const LevelVar *u);
Decl equality_cast(Heap *heap, const LevelVar *u);
Decl equality_type_symm(Heap *heap, const LevelVar *u);
Decl equality_type_trans(Heap *heap, const LevelVar *u);
/* members: a Vec of Decl pointers. */
Comp equality_declare_equality(Heap *heap, FamilyRaw raw, Globals globals, const Vec *members);
Comp equality_catalog_comp(Heap *heap, Globals globals);
Result equality_catalog(Heap *heap, Globals globals, Budget budget);

/* ---- families (prelude/families.bend) ---- */
Result families_variable(Heap *heap, Nat index);
Comp families_declare_equality(Heap *heap, FamilyRaw raw, Globals globals);
Comp families_catalog_comp(Heap *heap, Globals globals);
Result families_catalog(Heap *heap, Globals globals, Budget budget);

#endif
