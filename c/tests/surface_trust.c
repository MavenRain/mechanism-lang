/* Port of bend2/tests/surface_trust.bend (oracle label unit-surface-trust): family declaration error
 * precedence, constructor parameter permutation, reuse metadata mutations, ambient global rechecking on
 * instantiation and composition, and the mutual group reuse refusal text. */
#include <string.h>
#include "driver.h"
#include "surface.h"

static Result done(void *value) { return (Result){RESULT_DONE, value, {0}}; }
static Result fail(Error error) { return (Result){RESULT_FAIL, NULL, error}; }
static Result mismatch(Str message) { return fail(error_make(ERROR_MISMATCH, message)); }
static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* ensure(ok, message) */
static Result ensure(bool ok, Str message) {
  if (ok) return done(NULL);
  return mismatch(message);
}

/* refused(result) */
static bool refused(Result result) { return result.kind == RESULT_FAIL; }

/* exact(expected, result) */
static bool exact(Heap *heap, Str expected, Result result) {
  if (result.kind != RESULT_FAIL) return false;
  return str_eq(expected, error_to_string(heap, result.error));
}

/* expect_exact(expected, result) */
static Result expect_exact(Heap *heap, Error expected, Result result) {
  Str want = error_to_string(heap, expected);
  if (result.kind != RESULT_FAIL) return mismatch(cat(heap, str_lit("expected rejection: "), want));
  Str got = error_to_string(heap, result.error);
  Str text = cat(heap, cat(heap, cat(heap, str_lit("expected "), want), str_lit("; actual ")), got);
  return ensure(str_eq(want, got), text);
}

/* ---- Bend literal builders: records are boxed on the heap, lists are Vecs of pointers ---- */
static void *boxed(Heap *heap, const void *value, size_t size) {
  void *copy = heap_alloc(heap, size);
  if (copy == NULL) {
    heap->oom = true;
    return NULL;
  }
  memcpy(copy, value, size);
  return copy;
}

static Vec list_of(Heap *heap, size_t count, void *const *items) {
  Vec v = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) {
    if (items[i] == NULL || !vec_push(&v, items[i])) heap->oom = true;
  }
  return v;
}

static Vec list1(Heap *heap, void *a) {
  void *items[] = {a};
  return list_of(heap, 1, items);
}

static Vec list2(Heap *heap, void *a, void *b) {
  void *items[] = {a, b};
  return list_of(heap, 2, items);
}

/* A heap copy of a Vec, for the const Vec * parameters of the family API. */
static Vec *vec_box(Heap *heap, Vec v) {
  static Vec none;
  Vec *copy = boxed(heap, &v, sizeof v);
  if (copy == NULL) return &none;
  return copy;
}

static Vec *empty_list(Heap *heap) { return vec_box(heap, vec_new(heap->arena)); }

/* Positivity.Binder{quantity, name, ty} */
static TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  TeleBinder b = {quantity, str_lit(name), ty};
  return boxed(heap, &b, sizeof b);
}

/* Check.FamilyDecl{name, params, indices, level} */
static FamilyDecl family_decl(const char *name, Vec params, Vec indices, const LevelVar *level) {
  FamilyDecl d = {str_lit(name), params, indices, level};
  return d;
}

/* Check.CtorDecl{name, args, res_params, res_idx} */
static CtorDecl *ctor_decl(Heap *heap, const char *name, Vec args, Vec res_params, Vec res_idx) {
  CtorDecl c = {str_lit(name), args, res_params, res_idx};
  return boxed(heap, &c, sizeof c);
}

/* Check.Decl{name, Check.Definition{}, ty, Some{body}} */
static Decl *definition(Heap *heap, const char *name, const Term *ty, const Term *body) {
  Decl d;
  d.name = str_lit(name);
  d.kind = DECL_DEFINITION;
  d.ty = ty;
  d.body.some = true;
  d.body.value = body;
  return boxed(heap, &d, sizeof d);
}

/* Common.Pair{source, target}: one reuse binding. */
static PolyRename *rename_pair(Heap *heap, const char *source, const char *target) {
  PolyRename r = {str_lit(source), str_lit(target)};
  return boxed(heap, &r, sizeof r);
}

/* None{} exports */
static FamilyMaybeVec none_exports(Heap *heap) {
  FamilyMaybeVec m;
  m.some = false;
  m.value = vec_new(heap->arena);
  return m;
}

static const Term *nat_type(Heap *heap) { return term_global(heap, str_lit("Nat")); }
static const Term *univ_one(Heap *heap) { return term_univ(heap, level_one(heap)); }
static Nat nat_one(Heap *heap) { return heap_nat_of_u64(heap, 1); }
static const Term *var0(Heap *heap) { return term_var(heap, nat_zero()); }
static const Term *var1(Heap *heap) { return term_var(heap, nat_one(heap)); }

/* family_type(name) = Term.Lan{Shape.SMu{name, []}, Rules.diagram_of([])} */
static const Term *family_type(Heap *heap, const char *name) {
  Vec empty = vec_new(heap->arena);
  return term_lan(heap, shape_smu(str_lit(name), vec_new(heap->arena)), rules_diagram_of(heap, &empty));
}

/* Family.declare(globals, [], Budget.unlimited(), arity, family, ctors, members) */
static Result declare(Heap *heap, Globals globals, Nat arity, FamilyDecl family, Vec ctors, const Vec *members) {
  return family_declare(heap, globals, family_empty(), budget_unlimited(), arity, family, ctors, members);
}

/* Family.instantiate(globals, catalog, Budget.unlimited(), name, [], as_name, reuse, None{}) */
static Result instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, const char *as_name,
                          const Vec *reuse) {
  return family_instantiate(heap, globals, catalog, budget_unlimited(), str_lit(name), empty_list(heap),
                            str_lit(as_name), reuse, none_exports(heap));
}

/* precedence() */
static Result precedence(Heap *heap) {
  FamilyDecl family = family_decl("First", vec_new(heap->arena), vec_new(heap->arena), level_one(heap));
  Vec ctors = list1(heap, ctor_decl(heap, "first", vec_new(heap->arena), vec_new(heap->arena), vec_new(heap->arena)));
  Vec *members = vec_box(
      heap, list2(heap, definition(heap, "badBody", family_type(heap, "First"), var0(heap)),
                  definition(heap, "badLevel", term_univ(heap, level_var(heap, nat_one(heap))), term_univ(heap, level_zero()))));
  FamilyRaw raw = {family, ctors};
  Vec *families = vec_box(heap, list1(heap, boxed(heap, &raw, sizeof raw)));
  Result group = family_declare_group(heap, global_empty(), family_empty(), budget_unlimited(), nat_one(heap), families, members);
  Result r = expect_exact(heap, error_make(ERROR_UNBOUND, str_lit("de Bruijn index 0 is outside the context")), group);
  if (r.kind != RESULT_DONE) return r;
  Result single = declare(heap, global_empty(), nat_one(heap), family, ctors, members);
  return expect_exact(heap, error_make(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope")), single);
}

/* permutation() */
static Result permutation(Heap *heap) {
  Vec params = list2(heap, binder(heap, QUANTITY_ZERO, "A", univ_one(heap)), binder(heap, QUANTITY_ZERO, "B", univ_one(heap)));
  FamilyDecl family = family_decl("Pair", params, vec_new(heap->arena), level_one(heap));
  Vec straight = list1(heap, ctor_decl(heap, "pair", vec_new(heap->arena), list2(heap, (void *)var1(heap), (void *)var0(heap)),
                                       vec_new(heap->arena)));
  Vec swapped = list1(heap, ctor_decl(heap, "pair", vec_new(heap->arena), list2(heap, (void *)var0(heap), (void *)var1(heap)),
                                      vec_new(heap->arena)));
  Result catalog = declare(heap, global_empty(), nat_zero(), family, straight, empty_list(heap));
  if (catalog.kind != RESULT_DONE) return catalog;
  Result again = declare(heap, global_empty(), nat_zero(), family, swapped, empty_list(heap));
  return ensure(refused(again), str_lit("constructor parameter permutation accepted"));
}

/* indexed() */
static FamilyDecl indexed(Heap *heap) {
  Vec indices = list2(heap, binder(heap, QUANTITY_ZERO, "i", nat_type(heap)), binder(heap, QUANTITY_ZERO, "j", nat_type(heap)));
  return family_decl("Indexed", vec_new(heap->arena), indices, level_one(heap));
}

/* indexed_ctor() */
static CtorDecl *indexed_ctor(Heap *heap) {
  Vec args = list2(heap, binder(heap, QUANTITY_MANY, "x", nat_type(heap)), binder(heap, QUANTITY_MANY, "y", nat_type(heap)));
  Vec res_idx = list2(heap, (void *)var1(heap), (void *)var0(heap));
  return ctor_decl(heap, "indexed", args, vec_new(heap->arena), res_idx);
}

/* The four reuse_refused modes 0n..3n of the Bend driver. */
typedef enum { MUTATE_INDICES, MUTATE_ARITY, MUTATE_RECURSION, MUTATE_POSITIVITY } Mutation;

/* altered_ctor(ctor, mode) */
static Ctor *altered_ctor(Heap *heap, const Ctor *ctor, Mutation mode) {
  Ctor c = *ctor;
  switch (mode) {
    case MUTATE_INDICES: c.res_idx = list2(heap, (void *)var0(heap), (void *)var1(heap)); break;
    case MUTATE_ARITY: c.full_arity = heap_nat_succ(heap, ctor->full_arity); break;
    case MUTATE_RECURSION: c.self_rec = !ctor->self_rec; break;
    case MUTATE_POSITIVITY: break;
  }
  return boxed(heap, &c, sizeof c);
}

/* altered_ctors(ctors, mode) */
static Vec altered_ctors(Heap *heap, const Vec *ctors, Mutation mode) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ctors->len; i++) {
    VecGet got = vec_get(ctors, i);
    if (got.status != VEC_GET_OK) break;
    Ctor *c = altered_ctor(heap, (const Ctor *)got.value, mode);
    if (c == NULL || !vec_push(&out, c)) heap->oom = true;
  }
  return out;
}

/* altered(family, mode) */
static Family altered(Heap *heap, Family family, Mutation mode) {
  Vec ctors = altered_ctors(heap, &family.ctors, mode);
  family.ctors = ctors;
  if (mode == MUTATE_POSITIVITY) family.positive = false;
  return family;
}

/* reuse_refused(globals, catalog, family, mode) */
static bool reuse_refused(Heap *heap, Globals globals, FamilyCatalog catalog, Family family, Mutation mode) {
  Globals with = global_add_family(heap, str_lit("Existing"), altered(heap, family, mode), globals);
  Vec *reuse = vec_box(heap, list1(heap, rename_pair(heap, "Indexed", "Existing")));
  return refused(instantiate(heap, with, catalog, "Indexed", "Alias", reuse));
}

typedef struct {
  Mutation mode;
  const char *message;
} ReuseCheck;

static const ReuseCheck REUSE_CHECKS[] = {
  {MUTATE_INDICES, "reused index permutation accepted"},
  {MUTATE_ARITY, "reused constructor arity mutation accepted"},
  {MUTATE_RECURSION, "reused self-recursion mutation accepted"},
  {MUTATE_POSITIVITY, "reused positivity mutation accepted"},
};

/* reuse_metadata(globals, catalog, family) */
static Result reuse_metadata(Heap *heap, Globals globals, FamilyCatalog catalog, MaybeFamily family) {
  if (!family.some) return mismatch(str_lit("indexed positive control missing family"));
  size_t count = sizeof REUSE_CHECKS / sizeof REUSE_CHECKS[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++)
    r = ensure(reuse_refused(heap, globals, catalog, family.value, REUSE_CHECKS[i].mode), str_lit(REUSE_CHECKS[i].message));
  return r;
}

/* reuse() */
static Result reuse(Heap *heap) {
  Result declared = declare(heap, global_initial(heap), nat_zero(), indexed(heap), list1(heap, indexed_ctor(heap)), empty_list(heap));
  if (declared.kind != RESULT_DONE) return declared;
  FamilyCatalog catalog = (FamilyCatalog)declared.value;
  Result existing = instantiate(heap, global_initial(heap), catalog, "Indexed", "Existing", empty_list(heap));
  if (existing.kind != RESULT_DONE) return existing;
  Globals globals = *(Globals *)existing.value;
  Vec *reuse_list = vec_box(heap, list1(heap, rename_pair(heap, "Indexed", "Existing")));
  Result reused = instantiate(heap, globals, catalog, "Indexed", "Alias", reuse_list);
  if (reused.kind != RESULT_DONE) return reused;
  return reuse_metadata(heap, globals, catalog, global_find_family(str_lit("Existing"), globals));
}

/* Global.add("Ambient", Global.Axiom{ty}, globals) */
static Globals with_ambient(Heap *heap, const Term *ty, Globals globals) {
  return global_add(heap, str_lit("Ambient"), global_entry_axiom(ty), globals);
}

/* Check.FamilyDecl{"Box", [], [], Level.one()} */
static FamilyDecl box_family(Heap *heap) {
  return family_decl("Box", vec_new(heap->arena), vec_new(heap->arena), level_one(heap));
}

/* Check.CtorDecl{"box", [], [], []} */
static Vec bare_box_ctors(Heap *heap) {
  return list1(heap, ctor_decl(heap, "box", vec_new(heap->arena), vec_new(heap->arena), vec_new(heap->arena)));
}

/* Check.Decl{"m", Check.Definition{}, Term.Univ{Level.one()}, Some{Term.Global{"Ambient"}}} */
static Vec *ambient_members(Heap *heap) {
  return vec_box(heap, list1(heap, definition(heap, "m", univ_one(heap), term_global(heap, str_lit("Ambient")))));
}

/* The shared tail of ambient() and ambient_member(): Good is accepted, Missing and Changed are refused. */
static Result ambient_checks(Heap *heap, Globals globals, FamilyCatalog catalog, const char *missing, const char *changed) {
  Result control = instantiate(heap, globals, catalog, "Box", "Good", empty_list(heap));
  if (control.kind != RESULT_DONE) return control;
  Result r = ensure(refused(instantiate(heap, global_initial(heap), catalog, "Box", "Missing", empty_list(heap))), str_lit(missing));
  if (r.kind != RESULT_DONE) return r;
  Globals changed_globals = with_ambient(heap, nat_type(heap), globals);
  return ensure(refused(instantiate(heap, changed_globals, catalog, "Box", "Changed", empty_list(heap))), str_lit(changed));
}

/* ambient() */
static Result ambient(Heap *heap) {
  Globals globals = with_ambient(heap, univ_one(heap), global_initial(heap));
  Vec args = list1(heap, binder(heap, QUANTITY_MANY, "value", term_global(heap, str_lit("Ambient"))));
  Vec ctors = list1(heap, ctor_decl(heap, "box", args, vec_new(heap->arena), vec_new(heap->arena)));
  Result declared = declare(heap, globals, nat_zero(), box_family(heap), ctors, empty_list(heap));
  if (declared.kind != RESULT_DONE) return declared;
  return ambient_checks(heap, globals, (FamilyCatalog)declared.value, "missing ambient global bypassed rechecking",
                        "changed ambient global bypassed rechecking");
}

/* ambient_member() */
static Result ambient_member(Heap *heap) {
  Globals globals = with_ambient(heap, univ_one(heap), global_initial(heap));
  Result declared = declare(heap, globals, nat_zero(), box_family(heap), bare_box_ctors(heap), ambient_members(heap));
  if (declared.kind != RESULT_DONE) return declared;
  return ambient_checks(heap, globals, (FamilyCatalog)declared.value, "missing ambient global bypassed member rechecking",
                        "changed ambient global bypassed member rechecking");
}

/* [box_dependency()] = [Family.Dependency{"Box", [], "Inner", [], None{}}] */
static Vec *box_dependencies(Heap *heap) {
  FamilyDependency d;
  d.source = str_lit("Box");
  d.levels = vec_new(heap->arena);
  d.alias = str_lit("Inner");
  d.reuse = vec_new(heap->arena);
  d.exports = none_exports(heap);
  return vec_box(heap, list1(heap, boxed(heap, &d, sizeof d)));
}

/* Family.compose(globals, catalog, Budget.unlimited(), 0n, name, [box_dependency()], []) */
static Result compose(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name) {
  return family_compose(heap, globals, catalog, budget_unlimited(), nat_zero(), str_lit(name), box_dependencies(heap),
                        empty_list(heap));
}

/* ambient_composed() */
static Result ambient_composed(Heap *heap) {
  Globals globals = with_ambient(heap, univ_one(heap), global_initial(heap));
  Result declared = declare(heap, globals, nat_zero(), box_family(heap), bare_box_ctors(heap), ambient_members(heap));
  if (declared.kind != RESULT_DONE) return declared;
  FamilyCatalog catalog = (FamilyCatalog)declared.value;
  Result control = compose(heap, globals, catalog, "Good");
  if (control.kind != RESULT_DONE) return control;
  Result r = ensure(refused(compose(heap, global_initial(heap), catalog, "Missing")),
                    str_lit("missing ambient global bypassed composed member rechecking"));
  if (r.kind != RESULT_DONE) return r;
  return ensure(refused(compose(heap, with_ambient(heap, nat_type(heap), globals), catalog, "Changed")),
                str_lit("changed ambient global bypassed composed member rechecking"));
}

/* mutual(): source transcribed verbatim from the Bend driver. */
static const char *MUTUAL_SOURCE =
  "poly (u) mu P : Sort (succ u) with | pp : P and Q : Sort (succ u) with | qq : P -> Q where def mkp : P := pp end mu A : "
  "Sort (succ 0) with | pp : A and B : Sort (succ 0) with | qq : A -> B specialize P (0) as X with (P := A, Q := B)";

static Result mutual(Heap *heap) {
  Str expected = error_to_string(
      heap, error_make(ERROR_MISMATCH,
                       str_lit("the reused family B is a member of a mutual group, which family reuse does not support")));
  Result checked = elab_check_in(heap, global_empty(), budget_unlimited(), str_lit(MUTUAL_SOURCE));
  return ensure(exact(heap, expected, checked), str_lit("mutual group member reuse refusal changed"));
}

typedef Result (*Section)(Heap *heap);

static const Section SECTIONS[] = {precedence, permutation, reuse, ambient, ambient_member, ambient_composed, mutual};

/* suite(): the do block stops at the first Fail. */
static Result suite(Heap *heap) {
  size_t count = sizeof SECTIONS / sizeof SECTIONS[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++) r = SECTIONS[i](heap);
  return r;
}

/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE)
    return driver_print(str_lit("PASS family order, reuse metadata, rechecking, and refusal precedence"));
  return driver_die_text(1, error_to_string(heap, result.error));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = report(&heap, suite(&heap));
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
