/* Port of bend2/tests/surface_family_groups.bend (234 lines, oracle label surface-family-groups): ordered
 * family schema groups with members and renaming, the group refusals, companion levels, atomic target
 * collisions, closed rechecking, budgets and member callbacks, 31 cases in Bend order. Takes no argv; prints
 * FAMILY-GROUPS-OK cases=<n> or FAMILY-GROUPS-FAIL <case>: <message>. With -DMECH_DRIVER_LIBRARY the file is
 * the Groups library of surface_family_members (no main): marker, ctor, ty, value, definition, single and
 * with_entry with the prefix surface_family_groups_ (c/tests/surface_family_groups.h). */
#include <string.h>
#include "driver.h"
#include "surface_foundation_common.h"
#include "surface_raw_common.h"
#include "surface_family_groups.h"

/* A one-item Bend list literal; a NULL item or a failed push sets heap->oom. */
static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (item == NULL) heap->oom = true;
  else if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

/* marker(name) = FamilyDecl{name, [], [], L.one()} */
FamilyDecl surface_family_groups_marker(Heap *heap, Str name) {
  FamilyDecl marker;
  marker.name = name;
  marker.params = vec_new(heap->arena);
  marker.indices = vec_new(heap->arena);
  marker.level = level_one(heap);
  return marker;
}

/* ctor() = CtorDecl{"mark", [], [], []} */
CtorDecl *surface_family_groups_ctor(Heap *heap) {
  CtorDecl *mark = heap_alloc(heap, sizeof *mark);
  if (mark == NULL) {
    heap->oom = true;
    return NULL;
  }
  mark->name = str_lit("mark");
  mark->args = vec_new(heap->arena);
  mark->res_params = vec_new(heap->arena);
  mark->res_idx = vec_new(heap->arena);
  return mark;
}

/* ty(name) = Lan{SMu{name, []}, Rules.diagram_of([])} */
const Term *surface_family_groups_ty(Heap *heap, Str name) {
  Vec none = vec_new(heap->arena);
  return term_lan(heap, shape_smu(name, vec_new(heap->arena)), rules_diagram_of(heap, &none));
}

/* value(name, args) = In{SMu{name, []}, ACtor{"mark"}, args} */
const Term *surface_family_groups_value(Heap *heap, Str name, Vec args) {
  return term_in(heap, shape_smu(name, vec_new(heap->arena)), term_addr_ctor(str_lit("mark")), args);
}

/* definition(name, ty, body) = Decl{name, Definition{}, ty, Some{body}} */
Decl surface_family_groups_definition(Str name, const Term *ty, const Term *body) {
  Decl decl;
  decl.name = name;
  decl.kind = DECL_DEFINITION;
  decl.ty = ty;
  decl.body.some = true;
  decl.body.value = body;
  return decl;
}

#ifndef MECH_DRIVER_LIBRARY

static Vec two_items(Heap *heap, void *first, void *second) {
  Vec items = one_item(heap, first);
  if (second == NULL) heap->oom = true;
  else if (!vec_push(&items, second)) heap->oom = true;
  return items;
}

static Vec three_items(Heap *heap, void *first, void *second, void *third) {
  Vec items = two_items(heap, first, second);
  if (third == NULL) heap->oom = true;
  else if (!vec_push(&items, third)) heap->oom = true;
  return items;
}

static Str *str_box(Heap *heap, const char *text) {
  Str *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) heap->oom = true;
  else *box = str_lit(text);
  return box;
}

static Decl *decl_box(Heap *heap, Decl decl) {
  Decl *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) heap->oom = true;
  else *box = decl;
  return box;
}

static FamilyRaw *raw_box(Heap *heap, FamilyDecl family, Vec ctors) {
  FamilyRaw *raw = heap_alloc(heap, sizeof *raw);
  if (raw == NULL) {
    heap->oom = true;
    return NULL;
  }
  raw->family = family;
  raw->ctors = ctors;
  return raw;
}

static FamilyMemberElaborator *elaborator_box(Heap *heap, Comp (*fn)(Heap *, void *, Globals), void *ctx) {
  FamilyMemberElaborator *box = heap_alloc(heap, sizeof *box);
  if (box == NULL) {
    heap->oom = true;
    return NULL;
  }
  box->fn = fn;
  box->ctx = ctx;
  return box;
}

/* CtorDecl{"mark", args, [], []} */
static CtorDecl *mark_with(Heap *heap, Vec args) {
  CtorDecl *mark = surface_family_groups_ctor(heap);
  if (mark != NULL) mark->args = args;
  return mark;
}

/* Decl{name, Postulate{}, ty, None{}} */
static Decl postulate(Str name, const Term *ty) {
  Decl decl;
  decl.name = name;
  decl.kind = DECL_POSTULATE;
  decl.ty = ty;
  decl.body.some = false;
  decl.body.value = NULL;
  return decl;
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
}

static Error failure(ErrorKind kind, const char *message) { return error_make(kind, str_lit(message)); }

static Task host_oom(Heap *heap) {
  heap->oom = true;
  return task_fail(str_lit("out of memory"));
}

/* R.kernel(Family.T, result): the Done value is the catalog row pointer. */
static Task kernel_catalog(Heap *heap, Result result, FamilyCatalog *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, result, &value));
  *out = value;
  return task_pure();
}

/* R.kernel(G.T, result): the Done value is a boxed Globals. */
static Task kernel_globals(Heap *heap, Result result, Globals *out) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, result, &value));
  if (value == NULL) return host_oom(heap);
  *out = *(const Globals *)value;
  return task_pure();
}

static const Term *ty(Heap *heap, const char *name) { return surface_family_groups_ty(heap, str_lit(name)); }

/* value(name, []) */
static const Term *bare(Heap *heap, const char *name) {
  return surface_family_groups_value(heap, str_lit(name), vec_new(heap->arena));
}

/* value("Second", [value("First", [])]) */
static const Term *nested(Heap *heap) {
  return surface_family_groups_value(heap, str_lit("Second"), one_item(heap, (void *)bare(heap, "First")));
}

/* C.Pair{marker(name), [ctor()]} */
static FamilyRaw *plain(Heap *heap, const char *name) {
  return raw_box(heap, surface_family_groups_marker(heap, str_lit(name)), one_item(heap, surface_family_groups_ctor(heap)));
}

/* families() */
static Vec families(Heap *heap) {
  Vec args = one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("first"), ty(heap, "First")));
  return two_items(heap, plain(heap, "First"),
                   raw_box(heap, surface_family_groups_marker(heap, str_lit("Second")), one_item(heap, mark_with(heap, args))));
}

/* witness() */
static Decl witness(Heap *heap) {
  return surface_family_groups_definition(str_lit("witness"), ty(heap, "Second"), nested(heap));
}

/* alias() */
static Decl alias(Heap *heap) {
  return surface_family_groups_definition(str_lit("alias"), ty(heap, "Second"), term_global(heap, str_lit("witness")));
}

/* [witness(), alias()] */
static Vec witness_alias(Heap *heap) { return two_items(heap, decl_box(heap, witness(heap)), decl_box(heap, alias(heap))); }

/* declare(globals, catalog, members, group) */
static Result declare(Heap *heap, Globals globals, FamilyCatalog catalog, const Vec *members, const Vec *group) {
  return family_declare_group(heap, globals, catalog, budget_unlimited(), heap_nat_of_u64(heap, 1), group, members);
}

/* standard() */
static Result standard(Heap *heap) {
  Vec members = witness_alias(heap);
  Vec group = families(heap);
  return declare(heap, global_empty(), family_empty(), &members, &group);
}

/* instantiate(globals, catalog, name) */
static Result instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, const char *name) {
  Vec levels = one_item(heap, (void *)level_one(heap));
  Vec reuse = vec_new(heap->arena);
  return family_instantiate(heap, globals, catalog, budget_unlimited(), str_lit("First"), &levels, str_lit(name), &reuse,
                            no_exports(heap));
}

/* positive() */
static Task positive(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals one;
  Globals two;
  const ElabChecked *client = NULL;
  const Term *actual = NULL;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "One"), &one));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, one, catalog, "Two"), &two));
  RELATIONAL_TRY(relational_checked(
      heap, two, str_lit("def client : One := case One_alias as self in One_Second return One with\n| mark (first : One) => first\n"),
      &client));
  Globals globals = relational_globals(client);
  RELATIONAL_TRY(relational_normalize(heap, globals, str_lit("client"), &actual));
  RELATIONAL_TRY(relational_require(term_equal(actual, bare(heap, "One")),
                                    str_lit("cross-family constructor reference was not specialized")));
  RELATIONAL_TRY(relational_require(surface_raw_common_absent(global_find_family(str_lit("First"), globals).some) &&
                                        surface_raw_common_absent(global_find_family(str_lit("Second"), globals).some) &&
                                        surface_raw_common_absent(global_find(str_lit("witness"), globals).some),
                                    str_lit("symbolic names escaped")));
  return surface_raw_common_refusal(heap, failure(ERROR_MISMATCH, "the term has type (Lan SMu Two_Second"),
                                    elab_check_in(heap, globals, budget_unlimited(), str_lit("def mixed : One_Second := Two_alias")));
}

/* complete(family) */
static bool complete(MaybeFamily family) {
  if (!family.some || family.value.status.kind != CTOR_STATUS_COMPLETE) return false;
  return family.value.positive;
}

/* guard(ok, message) */
static Comp guard(bool ok, const char *message) {
  if (ok) return budget_lift(budget_result_done(kernel_unit()));
  return budget_lift(budget_result_fail(failure(ERROR_MISMATCH, message)));
}

static Comp pure_decl(Heap *heap, Decl decl) {
  Decl *box = decl_box(heap, decl);
  if (box == NULL) return budget_failed(failure(ERROR_MISMATCH, "host out of memory"));
  return budget_pure(box);
}

static Comp first_return(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_decl(heap, witness(heap));
}

static Comp first_members(Heap *heap, void *ctx, void *value) {
  Globals globals = *(const Globals *)ctx;
  NextK next;
  (void)value;
  next.fn = first_return;
  next.ctx = NULL;
  return budget_bind(heap,
                     guard(surface_raw_common_absent(global_find(str_lit("witness"), globals).some) &&
                               surface_raw_common_absent(global_find(str_lit("alias"), globals).some),
                           "callback saw a future member"),
                     next);
}

/* first_callback(globals) */
static Comp first_callback(Heap *heap, void *ctx, Globals globals) {
  Globals *box = heap_alloc(heap, sizeof *box);
  NextK next;
  (void)ctx;
  if (box == NULL) {
    heap->oom = true;
    return budget_failed(failure(ERROR_MISMATCH, "host out of memory"));
  }
  *box = globals;
  next.fn = first_members;
  next.ctx = box;
  return budget_bind(heap,
                     guard(complete(global_find_family(str_lit("First"), globals)) &&
                               complete(global_find_family(str_lit("Second"), globals)),
                           "callback saw an incomplete family"),
                     next);
}

/* predecessor(entry) */
static Result predecessor(Heap *heap, MaybeEntry entry) {
  if (!entry.some) return budget_result_fail(failure(ERROR_UNBOUND, "callback predecessor"));
  switch (entry.value.kind) {
    case ENTRY_DEF:
      if (term_equal(entry.value.def.body, nested(heap))) return budget_result_done(kernel_unit());
      return budget_result_fail(failure(ERROR_MISMATCH, "callback predecessor changed"));
    case ENTRY_AXIOM:
      return budget_result_fail(failure(ERROR_MISMATCH, "unchecked predecessor"));
    case ENTRY_PRIM:
      return budget_result_fail(failure(ERROR_MISMATCH, "unchecked predecessor"));
  }
  return budget_result_fail(failure(ERROR_MISMATCH, "unchecked predecessor"));
}

static Comp second_return(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_decl(heap, alias(heap));
}

/* second_callback(globals) */
static Comp second_callback(Heap *heap, void *ctx, Globals globals) {
  NextK next;
  (void)ctx;
  next.fn = second_return;
  next.ctx = NULL;
  return budget_bind(heap, budget_lift(predecessor(heap, global_find(str_lit("witness"), globals))), next);
}

/* elaborate(catalog, group, members) */
static Comp elaborate(Heap *heap, FamilyCatalog catalog, const Vec *group, const Vec *members) {
  return family_declare_group_elaborated_comp(heap, global_empty(), catalog, heap_nat_of_u64(heap, 1), group, members);
}

/* catalog_members(value) */
static bool catalog_members(Heap *heap, FamilyMaybeVec value) {
  if (!value.some) return false;
  Vec expected = two_items(heap, str_box(heap, "witness"), str_box(heap, "alias"));
  return relational_strings(&value.value, &expected);
}

/* callback_order() */
static Task callback_order(Heap *heap) {
  FamilyCatalog callbacks = family_empty();
  FamilyCatalog raw = family_empty();
  Globals actual;
  Globals expected;
  Vec group = families(heap);
  Vec members = two_items(heap, elaborator_box(heap, first_callback, NULL), elaborator_box(heap, second_callback, NULL));
  RELATIONAL_TRY(kernel_catalog(heap, budget_run(heap, elaborate(heap, family_empty(), &group, &members), budget_unlimited()),
                                &callbacks));
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &raw));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), callbacks, "One"), &actual));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), raw, "One"), &expected));
  return relational_require(surface_raw_common_same_globals(actual, expected) &&
                                catalog_members(heap, family_members(heap, callbacks, str_lit("First"))),
                            str_lit("callback declaration changed the installed program"));
}

/* sentinel(globals) */
static Comp sentinel(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(failure(ERROR_MISMATCH, "a callback ran after an invalid predecessor"));
}

/* globals => B.pure(Check.Decl, member): ctx is the boxed member. */
static Comp pure_member(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)globals;
  return budget_pure(ctx);
}

/* callback_failure(expected, member) */
static Task callback_failure(Heap *heap, Error expected, Decl member) {
  Vec group = families(heap);
  Vec members = two_items(heap, elaborator_box(heap, pure_member, decl_box(heap, member)), elaborator_box(heap, sentinel, NULL));
  return surface_raw_common_exact(heap, expected,
                                  budget_run(heap, elaborate(heap, family_empty(), &group, &members), budget_unlimited()));
}

#endif

/* single(name, catalog) */
Result surface_family_groups_single(Heap *heap, Str name, FamilyCatalog catalog) {
  Vec none = vec_new(heap->arena);
  return family_declare(heap, global_empty(), catalog, budget_unlimited(), nat_zero(), surface_family_groups_marker(heap, name),
                        one_item(heap, surface_family_groups_ctor(heap)), &none);
}

/* with_entry(name) = G.add(name, G.Axiom{T.Univ{L.one()}}, G.empty()) */
Globals surface_family_groups_with_entry(Heap *heap, Str name) {
  Entry entry;
  memset(&entry, 0, sizeof entry);
  entry.kind = ENTRY_AXIOM;
  entry.ty = term_univ(heap, level_one(heap));
  return global_add(heap, name, entry, global_empty());
}

#ifndef MECH_DRIVER_LIBRARY

static Globals with_entry(Heap *heap, const char *name) { return surface_family_groups_with_entry(heap, str_lit(name)); }

/* companion_level(family) */
static Task companion_level(Heap *heap, MaybeFamily family) {
  if (!family.some) return task_fail(str_lit("specialized companion missing"));
  return relational_require(level_equal(heap, family.value.level, level_succ(heap, level_one(heap))),
                            str_lit("companion universe was not specialized"));
}

/* companion_specialization() */
static Task companion_specialization(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals globals;
  const ElabChecked *checked = NULL;
  FamilyDecl second;
  second.name = str_lit("Second");
  second.params = vec_new(heap->arena);
  second.indices = vec_new(heap->arena);
  second.level = level_succ(heap, level_var(heap, nat_zero()));
  Vec args = one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("A"), term_univ(heap, level_var(heap, nat_zero()))));
  Vec group = two_items(heap, plain(heap, "First"), raw_box(heap, second, one_item(heap, mark_with(heap, args))));
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, global_empty(), family_empty(), &none, &group), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, global_empty(), catalog, "Levels"), &globals));
  RELATIONAL_TRY(companion_level(heap, global_find_family(str_lit("Levels_Second"), globals)));
  RELATIONAL_TRY(relational_checked(heap, globals, str_lit("def levelWitness : Levels_Second := mark Levels"), &checked));
  return task_pure();
}

/* target_collision(name, companion) */
static Task target_collision(Heap *heap, const char *name, bool companion) {
  FamilyCatalog catalog = family_empty();
  Globals fresh;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  Globals globals = with_entry(heap, name);
  RELATIONAL_TRY(surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit(name)),
                                            instantiate(heap, globals, catalog, "One")));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, globals, catalog, "Two"), &fresh));
  bool kept = false;
  if (companion) kept = surface_raw_common_present(global_find_family(str_lit("Two_Second"), fresh).some);
  else kept = surface_raw_common_present(global_find(str_lit("Two_alias"), fresh).some);
  return relational_require(kept && surface_raw_common_present(global_find(str_lit("Two_witness"), fresh).some),
                            str_lit("the refused instance left the caller globals unusable"));
}

/* closed_rechecking() */
static Task closed_rechecking(Heap *heap) {
  FamilyCatalog catalog = family_empty();
  Globals installed;
  Globals globals = with_entry(heap, "Seed");
  Vec args = one_item(heap, positivity_binder_new(heap, QUANTITY_MANY, str_lit("seed"), term_global(heap, str_lit("Seed"))));
  Vec group = two_items(heap, plain(heap, "First"),
                        raw_box(heap, surface_family_groups_marker(heap, str_lit("Second")), one_item(heap, mark_with(heap, args))));
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(kernel_catalog(heap, declare(heap, globals, family_empty(), &none, &group), &catalog));
  RELATIONAL_TRY(kernel_globals(heap, instantiate(heap, globals, catalog, "Compatible"), &installed));
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "Seed"), instantiate(heap, global_empty(), catalog, "Missing"));
}

/* hidden_member() */
static Decl hidden_member(Heap *heap) {
  const Term *domain = term_univ(heap, level_one(heap));
  Shape shape = shape_spi(QUANTITY_MANY, str_lit("A"), (void *)domain);
  Vec binders = one_item(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("A")));
  Vec legs = one_item(heap, term_leg_new(heap, binders, term_var(heap, nat_zero())));
  const Term *identity = term_ann(heap, term_sec(heap, shape, legs), term_ran(heap, shape, domain));
  Shape ignored = shape_spi(QUANTITY_MANY, str_lit("ignored"), (void *)term_univ(heap, level_var(heap, heap_nat_of_u64(heap, 1))));
  return surface_family_groups_definition(
      str_lit("hidden"), domain, term_out(heap, ignored, term_addr_pt(QUANTITY_MANY, term_univ(heap, level_zero())), identity));
}

/* globals => B.failed(Check.Decl, E.Cannot_infer{"callback failed"}) */
static Comp failed_member(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)ctx;
  (void)globals;
  return budget_failed(failure(ERROR_CANNOT_INFER, "callback failed"));
}

/* callback_error() */
static Task callback_error(Heap *heap) {
  Vec group = families(heap);
  Vec members = three_items(heap, elaborator_box(heap, pure_member, decl_box(heap, witness(heap))),
                            elaborator_box(heap, failed_member, NULL), elaborator_box(heap, sentinel, NULL));
  return surface_raw_common_exact(heap, failure(ERROR_CANNOT_INFER, "callback failed"),
                                  budget_run(heap, elaborate(heap, family_empty(), &group, &members), budget_unlimited()));
}

/* reverse_group() = List.reverse(families()) */
static Vec reverse_group(Heap *heap) {
  Vec group = families(heap);
  Vec out = vec_new(heap->arena);
  for (size_t i = group.len; i > 0; i--) {
    VecGet got = vec_get(&group, i - 1);
    if (got.status == VEC_GET_OK && !vec_push(&out, got.value)) heap->oom = true;
  }
  return out;
}

/* callback_invalid() */
static Task callback_invalid(Heap *heap) {
  Vec empty = vec_new(heap->arena);
  Vec stop = one_item(heap, elaborator_box(heap, sentinel, NULL));
  RELATIONAL_TRY(surface_raw_common_exact(heap, failure(ERROR_MISMATCH, "a family schema group must be nonempty"),
                                          budget_run(heap, elaborate(heap, family_empty(), &empty, &stop), budget_unlimited())));
  Vec reversed = reverse_group(heap);
  return surface_raw_common_exact(heap, failure(ERROR_UNBOUND, "the family First is not declared"),
                                  budget_run(heap, elaborate(heap, family_empty(), &reversed, &stop), budget_unlimited()));
}

/* count : Nat <- Raw.measure(Family.T, B.run_state(elaborate([], families(), []), B.of_polls(100000n))) */
static Task measure_families(Heap *heap, uint64_t *count) {
  Vec group = families(heap);
  Vec none = vec_new(heap->arena);
  return surface_raw_common_measure(
      heap, budget_run_state(heap, elaborate(heap, family_empty(), &group, &none), budget_of_polls(heap_nat_of_u64(heap, 100000))),
      count);
}

/* callback_entry() */
static Task callback_entry(Heap *heap) {
  uint64_t count = 0;
  RELATIONAL_TRY(measure_families(heap, &count));
  Vec group = families(heap);
  Vec members = two_items(heap, elaborator_box(heap, sentinel, NULL), elaborator_box(heap, pure_member, decl_box(heap, alias(heap))));
  return surface_raw_common_exact(heap, surface_raw_common_budget_error(),
                                  budget_run_poll(heap, elaborate(heap, family_empty(), &group, &members),
                                                  surface_foundation_common_countdown(heap, count)));
}

/* callback_cancel() */
static Task callback_cancel(Heap *heap) {
  uint64_t count = 0;
  RELATIONAL_TRY(measure_families(heap, &count));
  Vec group = families(heap);
  Vec members = two_items(heap, elaborator_box(heap, pure_member, decl_box(heap, witness(heap))), elaborator_box(heap, sentinel, NULL));
  return surface_raw_common_exact(heap, surface_raw_common_budget_error(),
                                  budget_run_poll(heap, elaborate(heap, family_empty(), &group, &members),
                                                  surface_foundation_common_countdown(heap, 1 + count)));
}

/* member_precedence() */
static Task member_precedence(Heap *heap) {
  Vec members = two_items(
      heap, decl_box(heap, surface_family_groups_definition(str_lit("badBody"), ty(heap, "First"), term_var(heap, nat_zero()))),
      decl_box(heap, surface_family_groups_definition(str_lit("badLevel"), term_univ(heap, level_var(heap, heap_nat_of_u64(heap, 1))),
                                                      term_univ(heap, level_zero()))));
  Vec group = families(heap);
  RELATIONAL_TRY(surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "de Bruijn index 0"),
                                            declare(heap, global_empty(), family_empty(), &members, &group)));
  return surface_raw_common_refusal(heap, surface_raw_common_scope_error(),
                                    family_declare(heap, global_empty(), family_empty(), budget_unlimited(), heap_nat_of_u64(heap, 1),
                                                   surface_family_groups_marker(heap, str_lit("First")),
                                                   one_item(heap, surface_family_groups_ctor(heap)), &members));
}

/* The cases() lambdas, one function per Raw.Case in Bend order. */
static Task case_ordered(Heap *heap, void *ctx) {
  (void)ctx;
  return positive(heap);
}

static Task case_empty(Heap *heap, void *ctx) {
  Vec none = vec_new(heap->arena);
  Vec group = vec_new(heap->arena);
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_MISMATCH, "a family schema group must be nonempty"),
                                    declare(heap, global_empty(), family_empty(), &none, &group));
}

static Task case_duplicate_family(Heap *heap, void *ctx) {
  Vec none = vec_new(heap->arena);
  Vec group = two_items(heap, plain(heap, "First"), plain(heap, "First"));
  (void)ctx;
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("First")),
                                    declare(heap, global_empty(), family_empty(), &none, &group));
}

static Task case_forward_reference(Heap *heap, void *ctx) {
  Vec members = witness_alias(heap);
  Vec group = reverse_group(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "the family First is not declared"),
                                    declare(heap, global_empty(), family_empty(), &members, &group));
}

static Task case_duplicate_constructor(Heap *heap, void *ctx) {
  Vec none = vec_new(heap->arena);
  Vec ctors = two_items(heap, surface_family_groups_ctor(heap), surface_family_groups_ctor(heap));
  Vec group = two_items(heap, plain(heap, "First"), raw_box(heap, surface_family_groups_marker(heap, str_lit("Second")), ctors));
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_MISMATCH, "the constructor mark is already declared in Second"),
                                    declare(heap, global_empty(), family_empty(), &none, &group));
}

static Task case_member_family_collision(Heap *heap, void *ctx) {
  Vec members = one_item(heap, decl_box(heap, surface_family_groups_definition(str_lit("Second"), ty(heap, "Second"), nested(heap))));
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Second")),
                                    declare(heap, global_empty(), family_empty(), &members, &group));
}

static Task case_member_template_reference(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("Other"), family_empty()), &catalog));
  Vec members = one_item(heap, decl_box(heap, surface_family_groups_definition(str_lit("witness"), ty(heap, "Other"), nested(heap))));
  Vec group = families(heap);
  return surface_raw_common_refusal(heap, failure(ERROR_NOT_YET, "references between family schemas are not supported"),
                                    declare(heap, global_empty(), catalog, &members, &group));
}

static Task case_member_body(Heap *heap, void *ctx) {
  Vec members = one_item(heap, decl_box(heap, surface_family_groups_definition(str_lit("witness"), ty(heap, "Second"),
                                                                               term_ann(heap, bare(heap, "First"), ty(heap, "First")))));
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_MISMATCH, "the term has type (Lan SMu First"),
                                    declare(heap, global_empty(), family_empty(), &members, &group));
}

static Task case_member_forward_reference(Heap *heap, void *ctx) {
  Vec members = two_items(heap, decl_box(heap, alias(heap)), decl_box(heap, witness(heap)));
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_UNBOUND, "witness"),
                                    declare(heap, global_empty(), family_empty(), &members, &group));
}

static Task case_postulate(Heap *heap, void *ctx) {
  Vec members = one_item(heap, decl_box(heap, postulate(str_lit("witness"), ty(heap, "Second"))));
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, failure(ERROR_NOT_YET, "family schema members must be definitions"),
                                    declare(heap, global_empty(), family_empty(), &members, &group));
}

static Task case_companion_level_scope(Heap *heap, void *ctx) {
  FamilyDecl second;
  second.name = str_lit("Second");
  second.params = vec_new(heap->arena);
  second.indices = vec_new(heap->arena);
  second.level = level_succ(heap, level_var(heap, heap_nat_of_u64(heap, 1)));
  Vec none = vec_new(heap->arena);
  Vec group = two_items(heap, plain(heap, "First"), raw_box(heap, second, one_item(heap, surface_family_groups_ctor(heap))));
  (void)ctx;
  return surface_raw_common_refusal(heap, surface_raw_common_scope_error(),
                                    declare(heap, global_empty(), family_empty(), &none, &group));
}

static Task case_companion_level_specialization(Heap *heap, void *ctx) {
  (void)ctx;
  return companion_specialization(heap);
}

static Task case_companion_global_collision(Heap *heap, void *ctx) {
  Vec members = witness_alias(heap);
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Second")),
                                    declare(heap, with_entry(heap, "Second"), family_empty(), &members, &group));
}

static Task case_companion_template_collision(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("Second"), family_empty()), &catalog));
  Vec members = witness_alias(heap);
  Vec group = families(heap);
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("Second")),
                                    declare(heap, global_empty(), catalog, &members, &group));
}

static Task case_target_companion_collision_atomic(Heap *heap, void *ctx) {
  (void)ctx;
  return target_collision(heap, "One_Second", true);
}

static Task case_target_member_collision_atomic(Heap *heap, void *ctx) {
  (void)ctx;
  return target_collision(heap, "One_alias", false);
}

static Task case_target_companion_template_collision(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  RELATIONAL_TRY(kernel_catalog(heap, surface_family_groups_single(heap, str_lit("One_Second"), catalog), &catalog));
  return surface_raw_common_refusal(heap, surface_raw_common_collision(heap, str_lit("One_Second")),
                                    instantiate(heap, global_empty(), catalog, "One"));
}

static Task case_closed_rechecking(Heap *heap, void *ctx) {
  (void)ctx;
  return closed_rechecking(heap);
}

static Task case_arity(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  Vec levels = vec_new(heap->arena);
  Vec reuse = vec_new(heap->arena);
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  return surface_raw_common_refusal(heap, failure(ERROR_UNIVERSE, "the family schema First expects 1 universe arguments, got 0"),
                                    family_instantiate(heap, global_empty(), catalog, budget_unlimited(), str_lit("First"), &levels,
                                                       str_lit("One"), &reuse, no_exports(heap)));
}

static Task case_closed_levels(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  Vec levels = one_item(heap, (void *)level_var(heap, nat_zero()));
  Vec reuse = vec_new(heap->arena);
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  return surface_raw_common_refusal(heap, failure(ERROR_UNIVERSE, "universe arguments must be closed"),
                                    family_instantiate(heap, global_empty(), catalog, budget_unlimited(), str_lit("First"), &levels,
                                                       str_lit("One"), &reuse, no_exports(heap)));
}

static Task case_declaration_budget(Heap *heap, void *ctx) {
  Vec decls = witness_alias(heap);
  Vec members = family_raw_callbacks(heap, &decls);
  Vec group = families(heap);
  (void)ctx;
  return surface_raw_common_refusal(heap, surface_raw_common_budget_error(),
                                    budget_run_poll(heap, elaborate(heap, family_empty(), &group, &members),
                                                    surface_foundation_common_exhausted(heap)));
}

static Task case_specialization_budget(Heap *heap, void *ctx) {
  FamilyCatalog catalog = family_empty();
  Vec levels = one_item(heap, (void *)level_one(heap));
  Vec reuse = vec_new(heap->arena);
  (void)ctx;
  RELATIONAL_TRY(kernel_catalog(heap, standard(heap), &catalog));
  return surface_raw_common_refusal(
      heap, surface_raw_common_budget_error(),
      budget_run_poll(heap,
                      family_instantiate_comp(heap, global_empty(), catalog, str_lit("First"), &levels, str_lit("One"), &reuse,
                                              no_exports(heap)),
                      surface_foundation_common_exhausted(heap)));
}

static Task case_callback_order_and_parity(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_order(heap);
}

static Task case_callback_body_checked(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_failure(heap, failure(ERROR_UNBOUND, "de Bruijn index 0 is outside the context"),
                          surface_family_groups_definition(str_lit("witness"), ty(heap, "Second"), term_var(heap, nat_zero())));
}

static Task case_callback_postulate_refused(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_failure(heap, failure(ERROR_NOT_YET, "family schema members must be definitions"),
                          postulate(str_lit("witness"), ty(heap, "Second")));
}

static Task case_callback_hidden_level_scope(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_failure(heap, surface_raw_common_scope_error(), hidden_member(heap));
}

static Task case_callback_error_stops(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_error(heap);
}

static Task case_callback_invalid_family_stops(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_invalid(heap);
}

static Task case_callback_budget_stops(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_cancel(heap);
}

static Task case_callback_budget_stops_at_entry(Heap *heap, void *ctx) {
  (void)ctx;
  return callback_entry(heap);
}

static Task case_member_refusal_precedence(Heap *heap, void *ctx) {
  (void)ctx;
  return member_precedence(heap);
}

typedef struct {
  const char *name;
  Task (*test)(Heap *heap, void *ctx);
} CaseRow;

/* cases() */
static const CaseRow case_rows[] = {
    {"ordered-families-members-and-renaming", case_ordered},
    {"empty", case_empty},
    {"duplicate-family", case_duplicate_family},
    {"forward-reference", case_forward_reference},
    {"duplicate-constructor", case_duplicate_constructor},
    {"member-family-collision", case_member_family_collision},
    {"member-template-reference", case_member_template_reference},
    {"member-body", case_member_body},
    {"member-forward-reference", case_member_forward_reference},
    {"postulate", case_postulate},
    {"companion-level-scope", case_companion_level_scope},
    {"companion-level-specialization", case_companion_level_specialization},
    {"companion-global-collision", case_companion_global_collision},
    {"companion-template-collision", case_companion_template_collision},
    {"target-companion-collision-atomic", case_target_companion_collision_atomic},
    {"target-member-collision-atomic", case_target_member_collision_atomic},
    {"target-companion-template-collision", case_target_companion_template_collision},
    {"closed-rechecking", case_closed_rechecking},
    {"arity", case_arity},
    {"closed-levels", case_closed_levels},
    {"declaration-budget", case_declaration_budget},
    {"specialization-budget", case_specialization_budget},
    {"callback-order-and-parity", case_callback_order_and_parity},
    {"callback-body-checked", case_callback_body_checked},
    {"callback-postulate-refused", case_callback_postulate_refused},
    {"callback-hidden-level-scope", case_callback_hidden_level_scope},
    {"callback-error-stops", case_callback_error_stops},
    {"callback-invalid-family-stops", case_callback_invalid_family_stops},
    {"callback-budget-stops", case_callback_budget_stops},
    {"callback-budget-stops-at-entry", case_callback_budget_stops_at_entry},
    {"member-refusal-precedence", case_member_refusal_precedence},
};

/* Raw.run(cases(), 0n) */
static Task run_cases(Heap *heap, Str *out) {
  size_t len = sizeof case_rows / sizeof case_rows[0];
  RawCase *cases = heap_alloc(heap, len * sizeof *cases);
  if (cases == NULL) return host_oom(heap);
  for (size_t i = 0; i < len; i++) {
    cases[i].name = str_lit(case_rows[i].name);
    cases[i].test = case_rows[i].test;
    cases[i].ctx = NULL;
  }
  return surface_raw_common_run(heap, cases, len, 0, out);
}

/* arguments(args) = main() = R.finish("FAMILY-GROUPS", Raw.run(cases(), 0n)); argv is ignored. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = run_cases(&heap, &fields);
  int code = relational_finish(&heap, str_lit("FAMILY-GROUPS"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }

#endif
