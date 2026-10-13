/* Port of bend2/tests/surface_shape_metadata.bend (no oracle call; c-test compares the output with
 * c/tests/surface_shape_metadata.expected): the SZk, SFhc and SMpc shape payloads of a hidden member
 * body are renamed and level-substituted by specialisation and are traversed by the universe scope
 * check. Imports surface_catalog.bend as Test (c/tests/surface_catalog.h). */
#include "surface_catalog.h"
#include "driver.h"

static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (item == NULL) heap->oom = true;
  else if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

/* payload(name, level) */
static const Term *payload(Heap *heap, Str name, const LevelVar *level) {
  return term_ann(heap, term_global(heap, name), term_univ(heap, level));
}

/* shape(mode, name, level) */
static Shape shape(Heap *heap, uint64_t mode, Str name, const LevelVar *level) {
  if (mode == 0) return shape_szk(QUANTITY_ZERO, str_lit("w"), (void *)payload(heap, name, level));
  if (mode == 1) return shape_sfhc((void *)payload(heap, name, level));
  if (mode == 2) return shape_smpc((void *)payload(heap, name, level), (void *)term_univ(heap, level_zero()));
  return shape_smpc((void *)term_univ(heap, level_zero()), (void *)payload(heap, name, level));
}

/* hidden_body(shape) */
static const Term *hidden_body(Heap *heap, Shape inner) {
  Shape pi = shape_spi(QUANTITY_MANY, str_lit("A"), (void *)term_univ(heap, level_one(heap)));
  Vec binders = one_item(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("A")));
  Leg *leg = term_leg_new(heap, binders, term_var(heap, nat_zero()));
  const Term *identity;
  if (leg == NULL) {
    heap->oom = true;
    return term_auto();
  }
  identity = term_ann(heap, term_sec(heap, pi, one_item(heap, leg)), term_ran(heap, pi, term_univ(heap, level_one(heap))));
  return term_out(heap, shape_spi(QUANTITY_MANY, str_lit("ignored"), (void *)term_lan(heap, inner, term_univ(heap, level_zero()))),
                  term_addr_pt(QUANTITY_MANY, term_univ(heap, level_zero())), identity);
}

/* hidden(shape) */
static Decl *hidden(Heap *heap, Shape inner) {
  Decl *decl = heap_alloc(heap, sizeof(Decl));
  if (decl == NULL) return NULL;
  decl->name = str_lit("hidden");
  decl->kind = DECL_DEFINITION;
  decl->ty = term_univ(heap, level_one(heap));
  decl->body.some = true;
  decl->body.value = hidden_body(heap, inner);
  return decl;
}

/* declare(shape) */
static Result declare(Heap *heap, Shape inner) {
  Vec members = one_item(heap, hidden(heap, inner));
  return family_declare(heap, global_empty(), family_empty(), budget_unlimited(), heap_nat_of_u64(heap, 1), catalog_marker(heap),
                        catalog_ctors(heap), &members);
}

/* renamed(entry, expected) */
static bool renamed(MaybeEntry entry, const Term *expected) {
  if (!entry.some || entry.value.kind != ENTRY_DEF) return false;
  return term_equal(entry.value.def.body, expected);
}

/* check_shape(mode) */
static Result check_shape(Heap *heap, uint64_t mode) {
  Str marker = str_lit("Marker");
  Vec no_reuse = vec_new(heap->arena);
  Vec level_one_list = one_item(heap, (void *)level_one(heap));
  FamilyMaybeVec no_exports;
  Result step;
  FamilyCatalog catalog;
  Globals globals;
  no_exports.some = false;
  no_exports.value = vec_new(heap->arena);
  step = declare(heap, shape(heap, mode, marker, level_var(heap, nat_zero())));
  if (step.kind == RESULT_FAIL) return step;
  catalog = step.value;
  step = family_instantiate(heap, global_empty(), catalog, budget_unlimited(), marker, &level_one_list, str_lit("Closed"), &no_reuse,
                            no_exports);
  if (step.kind == RESULT_FAIL) return step;
  globals = *(Globals *)step.value;
  step = catalog_ensure(renamed(global_find(str_lit("Closed_hidden"), globals),
                                hidden_body(heap, shape(heap, mode, str_lit("Closed"), level_one(heap)))),
                        "crypto shape payload did not specialize names and levels");
  if (step.kind == RESULT_FAIL) return step;
  return catalog_ensure(catalog_scope_refused(declare(heap, shape(heap, mode, marker, level_var(heap, heap_nat_of_u64(heap, 1))))),
                        "crypto shape payload bypassed universe scope");
}

/* suite() */
static Result suite(Heap *heap) {
  Result step = check_shape(heap, 0);
  if (step.kind == RESULT_FAIL) return step;
  step = check_shape(heap, 1);
  if (step.kind == RESULT_FAIL) return step;
  step = check_shape(heap, 2);
  if (step.kind == RESULT_FAIL) return step;
  return check_shape(heap, 3);
}

/* report(result) */
static int report(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return driver_print(str_lit("VEIL-TEMPLATES OK shapes=4"));
  return driver_die_text(1, error_to_string(heap, result.error));
}

/* main(); arguments(args) ignores its argument list. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 24);
  Heap heap = heap_new(&arena);
  int code = report(&heap, suite(&heap));
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
