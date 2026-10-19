/* Port of tests/surface_family_poly.bend (oracle label family-poly): universe polymorphic
 * family templates (Family = surface/family_poly) declare once, instantiate at closed levels, keep
 * Prop singleton elimination, take the maximum of unequal universes, rename recursive metadata,
 * refuse collisions, open levels, foreign templates and nonpositive fields, and spend budget.
 * The used defs of surface_raw_common (Raw), surface_template_common (Test),
 * surface_foundation_common (F), surface_prenex_common (Prenex), surface_universes (Universes) and
 * surface_boundary (Boundary) are ported locally, each under a comment naming its Bend source. */
#include "driver.h"
#include "prelude.h"
#include "surface.h"
#include "relational_common.h"

static void push(Heap *heap, Vec *items, const void *item) {
  if (item == NULL) heap->oom = true;
  else if (!vec_push(items, (void *)item)) heap->oom = true;
}

static Vec items0(Heap *heap) { return vec_new(heap->arena); }

static Vec items1(Heap *heap, const void *a) {
  Vec items = items0(heap);
  push(heap, &items, a);
  return items;
}

static Vec items2(Heap *heap, const void *a, const void *b) {
  Vec items = items1(heap, a);
  push(heap, &items, b);
  return items;
}

static Nat nat_u64(Heap *heap, uint64_t value) { return heap_nat_of_u64(heap, value); }

static const Term *var(Heap *heap, uint64_t index) { return term_var(heap, nat_u64(heap, index)); }

static const TeleBinder *binder(Heap *heap, Quantity quantity, const char *name, const Term *ty) {
  return positivity_binder_new(heap, quantity, str_lit(name), ty);
}

static TeleBinder binder_value(Quantity quantity, const char *name, const Term *ty) {
  TeleBinder b;
  b.quantity = quantity;
  b.name = str_lit(name);
  b.ty = ty;
  return b;
}

static FamilyDecl family_decl(Str name, Vec params, Vec indices, const LevelVar *level) {
  FamilyDecl decl;
  decl.name = name;
  decl.params = params;
  decl.indices = indices;
  decl.level = level;
  return decl;
}

static CtorDecl *ctor_decl(Heap *heap, const char *name, Vec args, Vec res_params, Vec res_idx) {
  CtorDecl *ctor = heap_alloc(heap, sizeof *ctor);
  if (ctor == NULL) return NULL;
  ctor->name = str_lit(name);
  ctor->args = args;
  ctor->res_params = res_params;
  ctor->res_idx = res_idx;
  return ctor;
}

static Str cat3(Heap *heap, const char *a, Str b, const char *c) {
  return str_cat(heap->arena, str_cat(heap->arena, str_lit(a), b), str_lit(c));
}

static Vec names1(Heap *heap, Str name) { return items1(heap, relational_str(heap, name)); }

/* Raw.refusal (surface_raw_common.bend 13): Audit.starts(expected, actual) = expected is a prefix of actual. */
static Task refusal(Heap *heap, Error expected, Result result) {
  Str text = error_to_string(heap, expected);
  if (result.kind == RESULT_DONE) return task_fail(str_cat(heap->arena, str_lit("expected refusal: "), text));
  Str actual = error_to_string(heap, result.error);
  return relational_require(str_starts_with(actual, text), str_cat(heap->arena, str_lit("wrong refusal: "), actual));
}

/* Raw.collision (surface_raw_common.bend 23) */
static Error collision(Heap *heap, Str name) {
  return error_make(ERROR_MISMATCH, cat3(heap, "the name ", name, " is already declared"));
}

/* Raw.budget_error (surface_raw_common.bend 74) */
static Error budget_error(void) { return error_make(ERROR_BUDGET_EXHAUSTED, str_lit("the check budget is exhausted")); }

/* Raw.scope_error (surface_raw_common.bend 77) */
static Error scope_error(void) {
  return error_make(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope"));
}

/* Raw.measure (surface_raw_common.bend 80; renamed raw_measure, Bend measure is below): Done = 100000n - remaining (Nat subtraction saturates). */
static Task raw_measure(Heap *heap, BudgetState state, uint64_t *polls) {
  if (state.budget.kind == BUDGET_POLLS && state.result.kind == RESULT_DONE) {
    NatToU64 remaining = nat_to_u64(state.budget.remaining);
    *polls = (remaining.status == NAT_TO_U64_OVERFLOW || remaining.value > 100000) ? 0 : 100000 - remaining.value;
    return task_pure();
  }
  if (state.result.kind == RESULT_FAIL) return task_fail(error_to_string(heap, state.result.error));
  return task_fail(str_lit("finite poll measurement disappeared"));
}

/* Raw.labeled (surface_raw_common.bend 61) */
static Task labeled(Heap *heap, Str name, Task result) {
  if (result.ok) return result;
  return task_fail(str_cat(heap->arena, str_cat(heap->arena, name, str_lit(": ")), result.message));
}

/* Test.required_family + Test.family (surface_template_common.bend 26, 31) */
static Task required_family(MaybeFamily value, Family *out) {
  if (!value.some) return task_fail(str_lit("missing family"));
  *out = value.value;
  return task_pure();
}

/* F.countdown (surface_foundation_common.bend 81): 0n answers exhausted, 1n+rest answers (countdown(rest), False). */
typedef struct {
  uint64_t remaining;
} Countdown;

static Callback countdown(Heap *heap, uint64_t remaining);

static CallbackPoll *countdown_poll(Heap *heap, void *ctx) {
  Countdown *c = ctx;
  if (c == NULL) return NULL;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->spent = c->remaining == 0;
  out->next = countdown(heap, c->remaining == 0 ? 0 : c->remaining - 1);
  return out;
}

static Callback countdown(Heap *heap, uint64_t remaining) {
  Countdown *c = heap_alloc(heap, sizeof *c);
  if (c != NULL) c->remaining = remaining;
  return budget_of_poll(countdown_poll, c);
}

/* F.exhausted (surface_foundation_common.bend 78): a callback that always answers (exhausted(), True),
 * the same answers as countdown(0n). */
static Callback exhausted(Heap *heap) { return countdown(heap, 0); }

/* Prenex.family_count (surface_prenex_common.bend 80): R.length of the family rows. */
static uint64_t family_count(Globals globals) {
  const FamilyRows *rows = globals.families;
  uint64_t count = 0;
  while (rows != NULL) {
    count += 1;
    rows = rows->rest;
  }
  return count;
}

/* Universes.member (surface_universes.bend 27) */
static const Term *member(Heap *heap, Str name, Vec params, Vec indices) {
  return term_lan(heap, shape_smu(name, indices), rules_diagram_of(heap, &params));
}

/* Universes.high_value (surface_universes.bend 22) */
static Result high_value(Heap *heap, MaybeBig value) {
  Result r;
  if (value.some) {
    r.kind = RESULT_DONE;
    r.value = (void *)level_var_lsucc(heap, value.value, level_zero());
    r.error = error_make(ERROR_MISMATCH, str_lit(""));
    return r;
  }
  r.kind = RESULT_FAIL;
  r.value = NULL;
  r.error = error_make(ERROR_MISMATCH, str_lit("high universe literal did not parse"));
  return r;
}

/* Universes.sum_client (surface_universes.bend 51) */
static Decl sum_client(Heap *heap, Str name, const LevelVar *left, const LevelVar *right, const char *ctor,
                       const Term *field) {
  Vec binders = items0(heap);
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, left))));
  push(heap, &binders, binder(heap, QUANTITY_ZERO, "B", term_univ(heap, level_succ(heap, right))));
  push(heap, &binders, binder(heap, QUANTITY_MANY, "value", field));
  const Term *result = member(heap, name, items2(heap, var(heap, 2), var(heap, 1)), items0(heap));
  const Term *body = term_in(heap, shape_smu(name, items0(heap)), term_addr_ctor(str_lit(ctor)), items1(heap, var(heap, 0)));
  return dependent_definition(heap, str_lit("sumClient"), &binders, result, body);
}

/* Boundary.counterfeit (surface_boundary.bend 49) */
static FamilyDecl counterfeit(Heap *heap, const LevelVar *carrier) {
  Vec params = items1(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, carrier))));
  return family_decl(str_lit("Counterfeit"), params, items0(heap), level_one(heap));
}

/* Boundary.counterfeit_ctor (surface_boundary.bend 52) */
static CtorDecl *counterfeit_ctor(Heap *heap) {
  Vec args = items1(heap, binder(heap, QUANTITY_MANY, "value", var(heap, 0)));
  return ctor_decl(heap, "counterfeit", args, items1(heap, var(heap, 1)), items0(heap));
}

/* ---- surface_family_poly.bend, in definition order ---- */

static FamilyDecl empty(Heap *heap, const char *name, const LevelVar *level) {
  return family_decl(str_lit(name), items0(heap), items0(heap), level);
}

static FamilyDecl eq(Heap *heap, const LevelVar *carrier) {
  Vec params = items2(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, carrier)), binder(heap, QUANTITY_ZERO, "x", var(heap, 0)));
  Vec indices = items1(heap, binder(heap, QUANTITY_ZERO, "y", var(heap, 1)));
  return family_decl(str_lit("TemplateEq"), params, indices, level_zero());
}

static CtorDecl *refl(Heap *heap) {
  return ctor_decl(heap, "templateRefl", items0(heap), items2(heap, var(heap, 1), var(heap, 0)), items1(heap, var(heap, 0)));
}

static Result declare(Heap *heap, Globals globals, FamilyCatalog catalog, uint64_t arity, FamilyDecl family, Vec ctors) {
  Vec members = items0(heap);
  return family_declare(heap, globals, catalog, budget_unlimited(), nat_u64(heap, arity), family, ctors, &members);
}

static Task standard(Heap *heap, FamilyCatalog *catalog) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, global_empty(), NULL, 1, eq(heap, level_var(heap, nat_zero())), items1(heap, refl(heap))), &value));
  *catalog = (FamilyCatalog)value;
  return task_pure();
}

static Result instance(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, Vec levels, Str target) {
  Vec reuse = items0(heap);
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = items0(heap);
  return family_instantiate(heap, globals, catalog, budget_unlimited(), name, &levels, target, &reuse, exports);
}

/* instance through R.kernel: Done = the new Globals. */
static Task instance_into(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name, Vec levels, const char *target,
                          Globals *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, instance(heap, globals, catalog, str_lit(name), levels, str_lit(target)), &value));
  *out = *(const Globals *)value;
  return task_pure();
}

static Task certificate(Family family) {
  if (family.status.kind == CTOR_STATUS_COMPLETE) return relational_require(family.positive, str_lit("family has no positivity verdict"));
  return task_fail(str_lit("instance is not a completed family"));
}

static Task family(Globals globals, Str name, Family *out) {
  RELATIONAL_TRY(required_family(global_find_family(name, globals), out));
  return certificate(*out);
}

static Task check_decl(Heap *heap, Globals globals, Decl decl) {
  void *value = NULL;
  return relational_kernel(heap, check_check_decl(heap, globals, budget_unlimited(), decl), &value);
}

static Task check_refl(Heap *heap, Globals globals, const char *name, const LevelVar *carrier) {
  Vec binders = items2(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, carrier)), binder(heap, QUANTITY_ZERO, "x", var(heap, 0)));
  const Term *result = member(heap, str_lit(name), items2(heap, var(heap, 1), var(heap, 0)), items1(heap, var(heap, 0)));
  const Term *body = term_in(heap, shape_smu(str_lit(name), items1(heap, var(heap, 0))), term_addr_ctor(str_lit("templateRefl")), items0(heap));
  return check_decl(heap, globals, dependent_definition(heap, str_lit("reflClient"), &binders, result, body));
}

static bool arity_one(MaybeNat value) {
  NatToU64 small = nat_to_u64(value.value);
  return value.some && small.status == NAT_TO_U64_OK && small.value == 1;
}

static Task catalog_only(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  RELATIONAL_TRY(relational_require(arity_one(family_arity(catalog, str_lit("TemplateEq"))), str_lit("template arity disappeared")));
  RELATIONAL_TRY(relational_require(!family_arity(catalog, str_lit("missing")).some, str_lit("unknown template has an arity")));
  RELATIONAL_TRY(relational_require(!global_find_family(str_lit("TemplateEq"), global_empty()).some, str_lit("template entered the family table")));
  return refusal(heap, error_make(ERROR_UNBOUND, str_lit("the family TemplateEq is not declared")),
                 check_infer_term(heap, global_empty(), budget_unlimited(), member(heap, str_lit("TemplateEq"), items0(heap), items0(heap))));
}

static Task equalities(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, high_value(heap, big_of_decimal(heap, str_lit("4611686018427387903"))), &value));
  const LevelVar *high = value;
  Globals globals = global_empty();
  RELATIONAL_TRY(instance_into(heap, global_empty(), catalog, "TemplateEq", items1(heap, level_zero()), "EqProp", &globals));
  RELATIONAL_TRY(instance_into(heap, globals, catalog, "TemplateEq", items1(heap, level_one(heap)), "EqData", &globals));
  RELATIONAL_TRY(instance_into(heap, globals, catalog, "TemplateEq", items1(heap, high), "EqHigh", &globals));
  RELATIONAL_TRY(check_refl(heap, globals, "EqProp", level_zero()));
  RELATIONAL_TRY(check_refl(heap, globals, "EqData", level_one(heap)));
  RELATIONAL_TRY(check_refl(heap, globals, "EqHigh", high));
  Family fam;
  RELATIONAL_TRY(family(globals, str_lit("EqHigh"), &fam));
  RELATIONAL_TRY(relational_require(level_equal(heap, family_kernel_level(fam), level_zero()), str_lit("equality no longer lives in Prop")));
  RELATIONAL_TRY(relational_require(check_engine_mu_zero_eliminable(fam), str_lit("equality lost its singleton elimination criterion")));
  return relational_require(!global_find_family(str_lit("TemplateEq"), globals).some, str_lit("specialization installed the original template"));
}

static Task sums(Heap *heap) {
  const LevelVar *left = level_succ(heap, level_var(heap, nat_zero()));
  const LevelVar *right = level_succ(heap, level_var(heap, nat_u64(heap, 1)));
  FamilyDecl decl = family_decl(str_lit("TemplateSum"),
                                items2(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, left)), binder(heap, QUANTITY_ZERO, "B", term_univ(heap, right))),
                                items0(heap), level_max(heap, left, right));
  Vec ctors = items2(heap,
                     ctor_decl(heap, "templateInl", items1(heap, binder(heap, QUANTITY_MANY, "left", var(heap, 1))), items2(heap, var(heap, 2), var(heap, 1)), items0(heap)),
                     ctor_decl(heap, "templateInr", items1(heap, binder(heap, QUANTITY_MANY, "right", var(heap, 0))), items2(heap, var(heap, 2), var(heap, 1)), items0(heap)));
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, global_empty(), NULL, 2, decl, ctors), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  Globals globals = global_empty();
  RELATIONAL_TRY(instance_into(heap, global_empty(), catalog, "TemplateSum", items2(heap, level_zero(), level_of_u32(heap, 2)), "SumLowHigh", &globals));
  RELATIONAL_TRY(instance_into(heap, globals, catalog, "TemplateSum", items2(heap, level_of_u32(heap, 2), level_zero()), "SumHighLow", &globals));
  RELATIONAL_TRY(check_decl(heap, globals, sum_client(heap, str_lit("SumLowHigh"), level_zero(), level_of_u32(heap, 2), "templateInl", var(heap, 1))));
  RELATIONAL_TRY(check_decl(heap, globals, sum_client(heap, str_lit("SumLowHigh"), level_zero(), level_of_u32(heap, 2), "templateInr", var(heap, 0))));
  RELATIONAL_TRY(check_decl(heap, globals, sum_client(heap, str_lit("SumHighLow"), level_of_u32(heap, 2), level_zero(), "templateInl", var(heap, 1))));
  RELATIONAL_TRY(check_decl(heap, globals, sum_client(heap, str_lit("SumHighLow"), level_of_u32(heap, 2), level_zero(), "templateInr", var(heap, 0))));
  Family fam;
  RELATIONAL_TRY(family(globals, str_lit("SumLowHigh"), &fam));
  return relational_require(level_equal(heap, family_kernel_level(fam), level_of_u32(heap, 3)), str_lit("sum result universe did not specialize max"));
}

static const Term *hidden(Heap *heap, Str name, Str motive_name, const LevelVar *carrier, const LevelVar *visible, const Term *argument) {
  Motive motive;
  motive.ind.some = true;
  motive.ind.value = motive_name;
  motive.indices = items0(heap);
  motive.self = str_lit("self");
  motive.body = term_univ(heap, carrier);
  MaybeMotive maybe;
  maybe.some = true;
  maybe.value = motive;
  Vec branches = items1(heap, term_branch_new(heap, term_addr_ctor(str_lit("templateNil")), term_leg(items0(heap), term_univ(heap, carrier))));
  const Term *elim = term_elim(heap, term_elim_data(shape_smu(name, items0(heap)), var(heap, 0), QUANTITY_ZERO, maybe, branches));
  const Term *identity = term_ann(heap, dependent_lambda(heap, binder_value(QUANTITY_ZERO, "T", term_univ(heap, visible)), var(heap, 0)),
                                  dependent_arrow(heap, binder_value(QUANTITY_ZERO, "T", term_univ(heap, visible)), term_univ(heap, visible)));
  return term_out(heap, shape_spi(QUANTITY_ZERO, str_lit("ignored"), (void *)elim), term_addr_pt(QUANTITY_ZERO, argument), identity);
}

static FamilyDecl recursive_decl(Heap *heap, const LevelVar *carrier) {
  Vec params = items1(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, carrier))));
  return family_decl(str_lit("TemplateList"), params, items0(heap), level_succ(heap, carrier));
}

static Vec recursive_ctors(Heap *heap, const Term *head) {
  Vec args = items2(heap, binder(heap, QUANTITY_MANY, "head", head),
                    binder(heap, QUANTITY_MANY, "tail", member(heap, str_lit("TemplateList"), items1(heap, var(heap, 1)), items0(heap))));
  return items2(heap, ctor_decl(heap, "templateNil", items0(heap), items1(heap, var(heap, 0)), items0(heap)),
                ctor_decl(heap, "templateCons", args, items1(heap, var(heap, 2)), items0(heap)));
}

static const Term *template_list_head(Heap *heap, const char *motive_name) {
  const LevelVar *carrier = level_succ(heap, level_var(heap, nat_zero()));
  return hidden(heap, str_lit("TemplateList"), str_lit(motive_name), carrier, carrier, var(heap, 0));
}

static Task recursive(Heap *heap, FamilyCatalog *catalog) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, global_empty(), NULL, 1, recursive_decl(heap, level_var(heap, nat_zero())),
                                                 recursive_ctors(heap, template_list_head(heap, "TemplateList"))),
                                   &value));
  *catalog = (FamilyCatalog)value;
  return task_pure();
}

static Task hidden_metadata(Str expected, const Term *term) {
  if (term != NULL && term->kind == TERM_OUT && term->shape.kind == SHAPE_PI) {
    const Term *domain = term->shape.domain;
    if (domain != NULL && domain->kind == TERM_ELIM && domain->data->shape.kind == SHAPE_MU && domain->data->motive.some &&
        domain->data->motive.value.ind.some) {
      RELATIONAL_TRY(relational_require(str_eq(domain->data->shape.name, expected), str_lit("stored recursive shape was not renamed")));
      return relational_require(str_eq(domain->data->motive.value.ind.value, expected), str_lit("stored recursive motive name was not renamed"));
    }
  }
  return task_fail(str_lit("stored metadata field lost its application, elimination, or motive"));
}

static const TeleBinder *binder_at(const Vec *items, size_t index) {
  VecGet got = vec_get(items, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return NULL;
  return got.value;
}

static Task recursive_metadata(Heap *heap, Str name, MaybeCtor ctor) {
  if (!ctor.some) return task_fail(str_lit("recursive constructor disappeared"));
  const TeleBinder *head = binder_at(&ctor.value.args, 0);
  const TeleBinder *tail = binder_at(&ctor.value.args, 1);
  if (head == NULL || tail == NULL || binder_at(&ctor.value.args, 2) != NULL) {
    return task_fail(str_lit("recursive constructor has the wrong field count"));
  }
  RELATIONAL_TRY(relational_require(ctor.value.self_rec, str_lit("recursive constructor lost its recursion flag")));
  RELATIONAL_TRY(hidden_metadata(name, head->ty));
  Vec specialized = names1(heap, name);
  Vec original = names1(heap, str_lit("TemplateList"));
  RELATIONAL_TRY(relational_require(term_exists_name(heap, true, &specialized, tail->ty), str_lit("recursive tail lost its specialized family")));
  return relational_require(!term_exists_name(heap, true, &original, tail->ty), str_lit("recursive tail retained its template name"));
}

static Task recursive_instance(Heap *heap, Globals globals, Str name, const LevelVar *carrier) {
  Family fam;
  RELATIONAL_TRY(family(globals, name, &fam));
  RELATIONAL_TRY(recursive_metadata(heap, name, positivity_ctor_of(str_lit("templateCons"), fam)));
  Vec binders = items2(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, carrier))), binder(heap, QUANTITY_MANY, "value", var(heap, 0)));
  const Term *nil = term_in(heap, shape_smu(name, items0(heap)), term_addr_ctor(str_lit("templateNil")), items0(heap));
  const Term *body = term_in(heap, shape_smu(name, items0(heap)), term_addr_ctor(str_lit("templateCons")), items2(heap, var(heap, 0), nil));
  RELATIONAL_TRY(check_decl(heap, globals, dependent_definition(heap, str_lit("listClient"), &binders, member(heap, name, items1(heap, var(heap, 1)), items0(heap)), body)));
  void *value = NULL;
  return relational_kernel(heap,
                           check_infer_term(heap, globals, budget_unlimited(),
                                            dependent_arrow(heap, binder_value(QUANTITY_ZERO, "A", term_univ(heap, level_succ(heap, carrier))),
                                                            member(heap, name, items1(heap, var(heap, 0)), items0(heap)))),
                           &value);
}

static Task recursives(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(recursive(heap, &catalog));
  Globals globals = global_empty();
  RELATIONAL_TRY(instance_into(heap, global_empty(), catalog, "TemplateList", items1(heap, level_zero()), "ListLow", &globals));
  RELATIONAL_TRY(instance_into(heap, globals, catalog, "TemplateList", items1(heap, level_of_u32(heap, 2)), "ListHigh", &globals));
  RELATIONAL_TRY(recursive_instance(heap, globals, str_lit("ListLow"), level_zero()));
  return recursive_instance(heap, globals, str_lit("ListHigh"), level_of_u32(heap, 2));
}

static Task scoped(Heap *heap, FamilyDecl decl, Vec ctors) {
  return refusal(heap, scope_error(), declare(heap, global_empty(), NULL, 1, decl, ctors));
}

static Task sampled(Heap *heap) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, check_declare_family(heap, global_empty(), budget_unlimited(), counterfeit(heap, level_zero())), &value));
  Globals declared = *(const Globals *)value;
  Vec group = names1(heap, str_lit("Counterfeit"));
  RELATIONAL_TRY(relational_kernel(heap, check_define_ctors(heap, declared, budget_unlimited(), &group, str_lit("Counterfeit"), items1(heap, counterfeit_ctor(heap))), &value));
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("a field of counterfeit exceeds its family universe")),
                 declare(heap, global_empty(), NULL, 1, counterfeit(heap, level_var(heap, nat_zero())), items1(heap, counterfeit_ctor(heap))));
}

static Task named_carrier(Heap *heap, const LevelVar *level, Globals *out) {
  Decl decl;
  decl.name = str_lit("Carrier");
  decl.kind = DECL_POSTULATE;
  decl.ty = term_univ(heap, level);
  decl.body.some = false;
  decl.body.value = NULL;
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, check_check_decl(heap, global_empty(), budget_unlimited(), decl), &value));
  *out = global_add(heap, str_lit("Carrier"), *(const Entry *)value, global_empty());
  return task_pure();
}

static Task nonpositive(Heap *heap) {
  const LevelVar *carrier = level_succ(heap, level_var(heap, nat_zero()));
  FamilyDecl decl = family_decl(str_lit("NegativeRecursive"), items1(heap, binder(heap, QUANTITY_ZERO, "A", term_univ(heap, carrier))), items0(heap), carrier);
  const Term *self = member(heap, str_lit("NegativeRecursive"), items1(heap, var(heap, 0)), items0(heap));
  const Term *function = dependent_arrow(heap, binder_value(QUANTITY_MANY, "self", self), var(heap, 1));
  CtorDecl *ctor = ctor_decl(heap, "negative", items1(heap, binder(heap, QUANTITY_MANY, "function", function)), items1(heap, var(heap, 1)), items0(heap));
  return refusal(heap, error_make(ERROR_NOT_YET, str_lit("a family that is not strictly positive arrives at M2")),
                 declare(heap, global_empty(), NULL, 1, decl, items1(heap, ctor)));
}

static Task duplicate(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  return refusal(heap, collision(heap, str_lit("TemplateEq")),
                 declare(heap, global_empty(), catalog, 1, eq(heap, level_var(heap, nat_zero())), items1(heap, refl(heap))));
}

static Task global_collision(Heap *heap) {
  Globals globals = global_empty();
  RELATIONAL_TRY(named_carrier(heap, level_one(heap), &globals));
  return refusal(heap, collision(heap, str_lit("Carrier")), declare(heap, globals, NULL, 0, empty(heap, "Carrier", level_one(heap)), items0(heap)));
}

/* Bend name family_collision; renamed: the surface library exports family_collision. */
static Task family_collision_case(Heap *heap) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, check_declare_family(heap, global_empty(), budget_unlimited(), empty(heap, "Existing", level_one(heap))), &value));
  Globals globals = *(const Globals *)value;
  return refusal(heap, collision(heap, str_lit("Existing")), declare(heap, globals, NULL, 0, empty(heap, "Existing", level_one(heap)), items0(heap)));
}

static Task instance_arity(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  RELATIONAL_TRY(refusal(heap, error_make(ERROR_UNIVERSE, str_lit("the family schema TemplateEq expects 1 universe arguments, got 0")),
                         instance(heap, global_empty(), catalog, str_lit("TemplateEq"), items0(heap), str_lit("TooFew"))));
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("the family schema TemplateEq expects 1 universe arguments, got 2")),
                 instance(heap, global_empty(), catalog, str_lit("TemplateEq"), items2(heap, level_zero(), level_one(heap)), str_lit("TooMany")));
}

static Task free_instance(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("universe arguments must be closed")),
                 instance(heap, global_empty(), catalog, str_lit("TemplateEq"), items1(heap, level_var(heap, nat_zero())), str_lit("Open")));
}

static Task other_template(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  return refusal(heap, error_make(ERROR_NOT_YET, str_lit("references between family schemas are not supported")),
                 declare(heap, global_empty(), catalog, 1, recursive_decl(heap, level_var(heap, nat_zero())),
                         recursive_ctors(heap, template_list_head(heap, "TemplateEq"))));
}

static Task target_collision(Heap *heap, Str target, Globals globals) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  return refusal(heap, collision(heap, target), instance(heap, globals, catalog, str_lit("TemplateEq"), items1(heap, level_zero()), target));
}

static Task global_target(Heap *heap) {
  Globals globals = global_empty();
  RELATIONAL_TRY(named_carrier(heap, level_one(heap), &globals));
  return target_collision(heap, str_lit("Carrier"), globals);
}

static Comp instantiate_comp(Heap *heap, FamilyCatalog catalog, const char *name, Vec levels) {
  Vec reuse = items0(heap);
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = items0(heap);
  return family_instantiate_comp(heap, global_empty(), catalog, str_lit(name), &levels, str_lit("Budgeted"), &reuse, exports);
}

static Task empty_instance_budget(Heap *heap) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, global_empty(), NULL, 0, empty(heap, "EmptyBudget", level_one(heap)), items0(heap)), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  return refusal(heap, budget_error(), budget_run_poll(heap, instantiate_comp(heap, catalog, "EmptyBudget", items0(heap)), exhausted(heap)));
}

static Task late_budget(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(recursive(heap, &catalog));
  return refusal(heap, budget_error(), budget_run_poll(heap, instantiate_comp(heap, catalog, "TemplateList", items1(heap, level_zero())), countdown(heap, 4)));
}

/* wrap (Bend 227): depth Ann layers of Univ(succ(carrier)) around term. */
static const Term *wrap(Heap *heap, uint64_t depth, const LevelVar *carrier, const Term *term) {
  if (depth == 0) return term;
  return wrap(heap, depth - 1, carrier, term_ann(heap, term, term_univ(heap, level_succ(heap, carrier))));
}

static FamilyDecl annotated(Heap *heap, uint64_t depth, const LevelVar *universe) {
  const Term *ty = wrap(heap, depth, level_succ(heap, universe), term_univ(heap, level_succ(heap, universe)));
  return family_decl(str_lit("AnnotationBudget"), items1(heap, binder(heap, QUANTITY_ZERO, "A", ty)), items0(heap), level_one(heap));
}

/* closed_check (Bend 235): declare_family_comp then define_ctors_comp in one budget computation. */
static Comp closed_check_ctors(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  Vec *group = heap_alloc(heap, sizeof(Vec));
  if (group == NULL || value == NULL) return budget_comp_pure(NULL);
  *group = names1(heap, str_lit("AnnotationBudget"));
  return check_define_ctors_comp(heap, *(const Globals *)value, nat_zero(), group, str_lit("AnnotationBudget"), items0(heap));
}

static Comp closed_check(Heap *heap, FamilyDecl decl) {
  NextK next;
  next.fn = closed_check_ctors;
  next.ctx = NULL;
  return budget_comp_bind(heap, check_declare_family_comp(heap, global_empty(), nat_zero(), decl), next);
}

/* type Measurement (Bend 240) */
typedef struct {
  FamilyCatalog catalog;
  uint64_t kernel_polls;
  uint64_t mapping_polls;
} Measurement;

static Task measure(Heap *heap, uint64_t depth, Measurement *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, global_empty(), NULL, 1, annotated(heap, depth, level_var(heap, nat_zero())), items0(heap)), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  uint64_t direct = 0;
  RELATIONAL_TRY(raw_measure(heap, budget_run_state(heap, closed_check(heap, annotated(heap, depth, level_zero())), budget_of_polls(nat_u64(heap, 100000))), &direct));
  uint64_t complete = 0;
  RELATIONAL_TRY(raw_measure(heap,
                             budget_run_state(heap, instantiate_comp(heap, catalog, "AnnotationBudget", items1(heap, level_zero())),
                                              budget_of_polls(nat_u64(heap, 100000))),
                             &complete));
  RELATIONAL_TRY(relational_require(!(complete < direct), str_lit("instance polling omitted part of closed kernel checking")));
  out->catalog = catalog;
  out->kernel_polls = direct;
  out->mapping_polls = complete < direct ? 0 : complete - direct;
  return task_pure();
}

static Task measured_mapping(Heap *heap, Measurement shallow, Measurement deep) {
  RELATIONAL_TRY(relational_require(shallow.mapping_polls < deep.mapping_polls,
                                    str_lit("substituting more raw Term nodes consumed no additional caller budget")));
  return refusal(heap, budget_error(),
                 budget_run_poll(heap, instantiate_comp(heap, deep.catalog, "AnnotationBudget", items1(heap, level_zero())),
                                 countdown(heap, deep.kernel_polls + shallow.mapping_polls)));
}

static Task substitution_budget(Heap *heap) {
  Measurement shallow;
  RELATIONAL_TRY(measure(heap, 0, &shallow));
  Measurement deep;
  RELATIONAL_TRY(measure(heap, 64, &deep));
  return measured_mapping(heap, shallow, deep);
}

static Task rollback(Heap *heap) {
  FamilyCatalog catalog = NULL;
  RELATIONAL_TRY(standard(heap, &catalog));
  Globals original = global_empty();
  RELATIONAL_TRY(instance_into(heap, global_empty(), catalog, "TemplateEq", items1(heap, level_zero()), "First", &original));
  RELATIONAL_TRY(refusal(heap, error_make(ERROR_UNIVERSE, str_lit("universe arguments must be closed")),
                         instance(heap, original, catalog, str_lit("TemplateEq"), items1(heap, level_var(heap, nat_zero())), str_lit("Retry"))));
  RELATIONAL_TRY(relational_require(family_count(original) == 1 && !global_find_family(str_lit("Retry"), original).some,
                                    str_lit("failed instance changed the input family table")));
  Globals result = global_empty();
  RELATIONAL_TRY(instance_into(heap, original, catalog, "TemplateEq", items1(heap, level_one(heap)), "Retry", &result));
  RELATIONAL_TRY(relational_require(family_count(result) == 2, str_lit("successful retry installed an incorrect number of families")));
  RELATIONAL_TRY(refusal(heap, collision(heap, str_lit("Retry")),
                         instance(heap, result, catalog, str_lit("TemplateEq"), items1(heap, level_zero()), str_lit("Retry"))));
  RELATIONAL_TRY(check_refl(heap, result, "Retry", level_one(heap)));
  return check_refl(heap, original, "First", level_zero());
}

static Task current_types(Heap *heap) {
  Globals low = global_empty();
  RELATIONAL_TRY(named_carrier(heap, level_one(heap), &low));
  Globals high = global_empty();
  RELATIONAL_TRY(named_carrier(heap, level_succ(heap, level_one(heap)), &high));
  CtorDecl *carry = ctor_decl(heap, "carry", items1(heap, binder(heap, QUANTITY_MANY, "value", term_global(heap, str_lit("Carrier")))), items0(heap), items0(heap));
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, low, NULL, 0, empty(heap, "UsesCarrier", level_one(heap)), items1(heap, carry)), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  RELATIONAL_TRY(refusal(heap, error_make(ERROR_UNIVERSE, str_lit("a field of carry exceeds its family universe")),
                         instance(heap, high, catalog, str_lit("UsesCarrier"), items0(heap), str_lit("Retry"))));
  RELATIONAL_TRY(relational_require(family_count(high) == 0, str_lit("failed recheck leaked its provisional family")));
  Globals result = global_empty();
  RELATIONAL_TRY(instance_into(heap, low, catalog, "UsesCarrier", items0(heap), "Retry", &result));
  Family fam;
  RELATIONAL_TRY(family(result, str_lit("Retry"), &fam));
  return relational_require(family_count(low) == 0, str_lit("retry changed the original environment"));
}

static Task missing_dependency(Heap *heap) {
  Globals globals = global_empty();
  RELATIONAL_TRY(named_carrier(heap, level_one(heap), &globals));
  FamilyDecl decl = family_decl(str_lit("UsesCarrier"), items1(heap, binder(heap, QUANTITY_ZERO, "value", term_global(heap, str_lit("Carrier")))), items0(heap),
                                level_one(heap));
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, declare(heap, globals, NULL, 0, decl, items0(heap)), &value));
  FamilyCatalog catalog = (FamilyCatalog)value;
  return refusal(heap, error_make(ERROR_UNBOUND, str_lit("Carrier")),
                 instance(heap, global_empty(), catalog, str_lit("UsesCarrier"), items0(heap), str_lit("MissingDependency")));
}

/* The inline case lambdas of cases (Bend 299-320), named by their case. */
static Task mixed_sort(Heap *heap) {
  return refusal(heap, error_make(ERROR_UNIVERSE, str_lit("a family schema must remain in Prop or Type for every universe substitution")),
                 declare(heap, global_empty(), NULL, 1, empty(heap, "Mixed", level_var(heap, nat_zero())), items0(heap)));
}

static Task free_header(Heap *heap) {
  return scoped(heap, empty(heap, "FreeHeader", level_succ(heap, level_var(heap, nat_u64(heap, 1)))), items0(heap));
}

static Task free_parameter(Heap *heap) { return scoped(heap, eq(heap, level_var(heap, nat_u64(heap, 1))), items1(heap, refl(heap))); }

static Task free_index(Heap *heap) {
  Vec indices = items1(heap, binder(heap, QUANTITY_ZERO, "P", term_univ(heap, level_var(heap, nat_u64(heap, 1)))));
  return scoped(heap, family_decl(str_lit("FreeIndex"), items0(heap), indices, level_zero()), items0(heap));
}

static Task free_field(Heap *heap) {
  Vec args = items1(heap, binder(heap, QUANTITY_ZERO, "P", term_univ(heap, level_var(heap, nat_u64(heap, 1)))));
  return scoped(heap, empty(heap, "FreeField", level_succ(heap, level_one(heap))), items1(heap, ctor_decl(heap, "freeField", args, items0(heap), items0(heap))));
}

static const Term *open_ann(Heap *heap, uint64_t index) {
  return term_ann(heap, var(heap, index), term_univ(heap, level_var(heap, nat_u64(heap, 1))));
}

static Task free_result_parameter(Heap *heap) {
  CtorDecl *ctor = ctor_decl(heap, "templateRefl", items0(heap), items2(heap, open_ann(heap, 1), var(heap, 0)), items1(heap, var(heap, 0)));
  return scoped(heap, eq(heap, level_var(heap, nat_zero())), items1(heap, ctor));
}

static Task free_result_index(Heap *heap) {
  CtorDecl *ctor = ctor_decl(heap, "templateRefl", items0(heap), items2(heap, var(heap, 1), var(heap, 0)), items1(heap, open_ann(heap, 0)));
  return scoped(heap, eq(heap, level_var(heap, nat_zero())), items1(heap, ctor));
}

static Task hidden_free_level(Heap *heap) {
  const Term *head = hidden(heap, str_lit("TemplateList"), str_lit("TemplateList"), level_var(heap, nat_u64(heap, 1)),
                            level_succ(heap, level_var(heap, nat_zero())), var(heap, 0));
  return scoped(heap, recursive_decl(heap, level_var(heap, nat_zero())), recursive_ctors(heap, head));
}

static Task duplicate_constructor(Heap *heap) {
  return refusal(heap, error_make(ERROR_MISMATCH, str_lit("the constructor templateRefl is already declared in TemplateEq")),
                 declare(heap, global_empty(), NULL, 1, eq(heap, level_var(heap, nat_zero())), items2(heap, refl(heap), refl(heap))));
}

static Task unknown_template(Heap *heap) {
  return refusal(heap, error_make(ERROR_UNBOUND, str_lit("unknown family universe schema Missing")),
                 instance(heap, global_empty(), NULL, str_lit("Missing"), items0(heap), str_lit("Unknown")));
}

static Task template_name_target(Heap *heap) { return target_collision(heap, str_lit("TemplateEq"), global_empty()); }

static Task empty_schema_budget(Heap *heap) {
  return refusal(heap, budget_error(),
                 budget_run_poll(heap, check_check_family_scheme_comp(heap, global_empty(), nat_zero(), empty(heap, "EmptyBudget", level_one(heap)), items0(heap)),
                                 exhausted(heap)));
}

static Task kernel_free_header(Heap *heap) {
  return refusal(heap, scope_error(),
                 check_check_family_scheme(heap, global_empty(), budget_unlimited(), nat_u64(heap, 1),
                                           empty(heap, "KernelFreeHeader", level_succ(heap, level_var(heap, nat_u64(heap, 3)))), items0(heap)));
}

static Task empty_catalog_budget(Heap *heap) {
  Vec *members = heap_alloc(heap, sizeof(Vec));
  if (members == NULL) return task_fail(str_lit("out of memory"));
  *members = items0(heap);
  return refusal(heap, budget_error(),
                 budget_run_poll(heap, family_declare_comp(heap, global_empty(), NULL, nat_zero(), empty(heap, "EmptyBudget", level_one(heap)), items0(heap), members),
                                 exhausted(heap)));
}

/* Raw.Case (surface_raw_common.bend 10) */
typedef struct {
  const char *name;
  Task (*test)(Heap *heap);
} RawCase;

typedef struct {
  const RawCase *items;
  size_t len;
} RawCases;

static const RawCase CASE_ROWS[] = {
    {"catalog-keeps-templates-out-of-globals", catalog_only},
    {"equality-at-prop-and-high-carrier-universes", equalities},
    {"sum-at-unequal-universes-in-both-directions", sums},
    {"recursive-fields-and-motive-metadata-are-renamed", recursives},
    {"closed-sample-does-not-prove-universal-field-bound", sampled},
    {"mixed-prop-type-result-sort-is-refused", mixed_sort},
    {"free-header-level-is-refused", free_header},
    {"free-parameter-level-is-refused", free_parameter},
    {"free-index-level-is-refused", free_index},
    {"free-constructor-field-level-is-refused", free_field},
    {"free-level-in-result-parameter-annotation-is-refused", free_result_parameter},
    {"free-level-in-result-index-annotation-is-refused", free_result_index},
    {"hidden-free-level-in-ignored-shape-is-refused", hidden_free_level},
    {"negative-recursive-position-is-refused", nonpositive},
    {"duplicate-template-name-is-refused", duplicate},
    {"existing-global-template-name-is-refused", global_collision},
    {"existing-family-template-name-is-refused", family_collision_case},
    {"duplicate-constructor-name-is-refused", duplicate_constructor},
    {"wrong-instance-arity-is-refused", instance_arity},
    {"free-instance-argument-is-refused", free_instance},
    {"unknown-family-template-is-refused", unknown_template},
    {"other-template-reference-in-motive-metadata-is-refused", other_template},
    {"instance-cannot-reuse-template-name", template_name_target},
    {"instance-cannot-replace-existing-global", global_target},
    {"empty-schema-honors-budget", empty_schema_budget},
    {"kernel-scheme-refuses-a-free-header-level", kernel_free_header},
    {"empty-catalog-declaration-honors-budget", empty_catalog_budget},
    {"empty-instance-honors-budget", empty_instance_budget},
    {"instance-budget-expires-after-entry", late_budget},
    {"raw-substitution-consumes-the-caller-budget", substitution_budget},
    {"failed-instances-preserve-globals-and-catalog", rollback},
    {"instance-rechecks-against-current-global-types", current_types},
    {"instance-rechecks-missing-global-dependency", missing_dependency},
};

static RawCases cases(void) {
  RawCases rows = {CASE_ROWS, sizeof CASE_ROWS / sizeof CASE_ROWS[0]};
  return rows;
}

/* Test.run (surface_template_common.bend 42): labeled cases in order, stop at the first failure. */
static Task test_run(Heap *heap, RawCases rows, size_t index, uint64_t *count) {
  if (index >= rows.len) return task_pure();
  RELATIONAL_TRY(labeled(heap, str_lit(rows.items[index].name), rows.items[index].test(heap)));
  *count += 1;
  return test_run(heap, rows, index + 1, count);
}

/* suite (Bend 328): not called by main; kept for parity with the Bend driver. */
static Task suite(Heap *heap, Str *out) {
  uint64_t count = 0;
  RELATIONAL_TRY(test_run(heap, cases(), 0, &count));
  *out = str_cat(heap->arena, str_cat(heap->arena, str_lit("cases="), str_of_u64(heap->arena, count)), str_lit(" type-excluded=negative-arity"));
  return task_pure();
}

static uint64_t report_case(Heap *heap, const char *name, Task result) {
  if (result.ok) {
    driver_print(str_cat(heap->arena, str_lit("PASS family-poly "), str_lit(name)));
    return 0;
  }
  driver_print(str_cat(heap->arena, cat3(heap, "FAIL family-poly ", str_lit(name), ": "), result.message));
  return 1;
}

/* finish_cases (Bend 344): Out.cli_exit(1) = exit status 1 with no output. */
static int finish_cases(Heap *heap, uint64_t count, uint64_t failures) {
  if (failures != 0) return 1;
  return driver_print(str_cat(heap->arena, str_cat(heap->arena, str_lit("FAMILY-POLY-OK cases="), str_of_u64(heap->arena, count)),
                              str_lit(" type-excluded=negative-arity")));
}

/* One case in its own arena (the kernel_protocol precedent); the case table is static. */
typedef struct {
  bool oom;
  uint64_t failed;
} CaseRun;

static CaseRun one_case(const RawCase *row) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Task result = row->test(&heap);
  CaseRun run = {heap.oom, 0};
  if (!heap.oom) run.failed = report_case(&heap, row->name, result);
  run.oom = run.oom || heap.oom;
  arena_free(&arena);
  return run;
}

static int run_cases(RawCases rows, size_t index, uint64_t count, uint64_t failures) {
  if (index >= rows.len) {
    Arena arena = arena_new(1 << 16);
    Heap heap = heap_new(&arena);
    int code = finish_cases(&heap, count, failures);
    if (heap.oom) code = driver_host_failure();
    arena_free(&arena);
    return code;
  }
  CaseRun run = one_case(&rows.items[index]);
  if (run.oom) return driver_host_failure();
  return run_cases(rows, index + 1, count + 1, failures + run.failed);
}

static int arguments(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return run_cases(cases(), 0, 0, 0);
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, arguments); }
