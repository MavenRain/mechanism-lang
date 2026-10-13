/* Port of bend2/tests/surface_transform.bend (oracle label unit-surface-transform): Poly.map_term
 * renames a family and substitutes its level under a 100 poll budget, exhausts a 5 poll budget, and
 * map_levels rejects out-of-scope and unsubstituted level variables. */
#include "driver.h"
#include "surface.h"

static Str *transform_name_box(Heap *heap, const char *text) {
  Str *copy = heap_alloc(heap, sizeof(Str));
  if (copy != NULL) *copy = str_lit(text);
  return copy;
}

static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

/* source() */
static const Term *source(Heap *heap) {
  Vec shape_args = one_item(heap, (void *)term_univ(heap, level_var(heap, nat_zero())));
  Motive motive;
  MaybeMotive some_motive;
  Vec binders = one_item(heap, term_binder_new(heap, QUANTITY_ZERO, str_lit("Box")));
  const Term *leg_body = term_ann(heap, term_global(heap, str_lit("Box")), term_univ(heap, level_var(heap, nat_zero())));
  Leg *leg = term_leg_new(heap, binders, leg_body);
  Vec branches;
  if (leg == NULL) return term_auto();
  branches = one_item(heap, term_branch_new(heap, term_addr_ctor(str_lit("box")), *leg));
  motive.ind.some = true;
  motive.ind.value = str_lit("Box");
  motive.indices = one_item(heap, transform_name_box(heap, "index"));
  motive.self = str_lit("self");
  motive.body = term_univ(heap, level_var(heap, nat_zero()));
  some_motive.some = true;
  some_motive.value = motive;
  return term_elim(heap, term_elim_data(shape_smu(str_lit("Box"), shape_args), term_global(heap, str_lit("value")), QUANTITY_ONE,
                                        some_motive, branches));
}

static const Term *item_term(const Vec *items, size_t index) {
  VecGet got = vec_get(items, index);
  if (got.status != VEC_GET_OK) return NULL;
  return (const Term *)got.value;
}

/* transformed(result) */
static bool transformed(Heap *heap, Result result) {
  const Term *term;
  const ElimData *data;
  const Term *shape_level;
  const Branch *branch;
  const Binder *binder;
  VecGet got;
  if (result.kind != RESULT_DONE) return false;
  term = (const Term *)result.value;
  if (term->kind != TERM_ELIM) return false;
  data = term->data;
  if (data->shape.kind != SHAPE_MU || data->shape.args.len != 1) return false;
  shape_level = item_term(&data->shape.args, 0);
  if (shape_level == NULL || shape_level->kind != TERM_UNIV) return false;
  if (data->scrut->kind != TERM_GLOBAL) return false;
  if (!data->motive.some || !data->motive.value.ind.some || data->motive.value.body->kind != TERM_UNIV) return false;
  if (data->branches.len != 1) return false;
  got = vec_get(&data->branches, 0);
  if (got.status != VEC_GET_OK) return false;
  branch = (const Branch *)got.value;
  if (branch->addr.kind != ADDR_CTOR || branch->leg.binders.len != 1) return false;
  got = vec_get(&branch->leg.binders, 0);
  if (got.status != VEC_GET_OK) return false;
  binder = (const Binder *)got.value;
  if (branch->leg.body->kind != TERM_ANN || branch->leg.body->body->kind != TERM_GLOBAL || branch->leg.body->ty->kind != TERM_UNIV)
    return false;
  return str_eq(data->shape.name, str_lit("SmallBox")) && str_eq(data->motive.value.ind.value, str_lit("SmallBox")) &&
         str_eq(branch->leg.body->body->name, str_lit("SmallBox")) && str_eq(binder->name, str_lit("Box")) &&
         str_eq(branch->addr.name, str_lit("box")) && level_equal(heap, shape_level->level, level_one(heap)) &&
         level_equal(heap, data->motive.value.body->level, level_one(heap)) &&
         level_equal(heap, branch->leg.body->ty->level, level_one(heap));
}

/* exhausted(result) */
static bool exhausted(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED; }

/* out_of_scope(result) */
static bool out_of_scope(Result result) { return result.kind == RESULT_FAIL && result.error.kind == ERROR_UNIVERSE; }

static PolyAction action(Heap *heap) {
  PolyRename *rename = heap_alloc(heap, sizeof(PolyRename));
  Vec names = vec_new(heap->arena);
  if (rename == NULL) return poly_action(poly_preserve_levels(), names, vec_new(heap->arena));
  rename->source = str_lit("Box");
  rename->target = str_lit("SmallBox");
  if (!vec_push(&names, rename)) heap->oom = true;
  return poly_action(poly_substitute_levels(one_item(heap, (void *)level_one(heap))), names, vec_new(heap->arena));
}

/* main() */
static bool suite(Heap *heap) {
  PolyAction act = action(heap);
  return transformed(heap, budget_run(heap, poly_map_term(heap, act, source(heap)), budget_of_polls(heap_nat_of_u64(heap, 100)))) &&
         exhausted(budget_run(heap, poly_map_term(heap, act, source(heap)), budget_of_polls(heap_nat_of_u64(heap, 5)))) &&
         out_of_scope(poly_map_levels(heap, poly_scope_levels(nat_zero()),
                                      term_ann(heap, term_auto(), term_univ(heap, level_var(heap, nat_zero()))))) &&
         out_of_scope(poly_map_levels(heap, poly_substitute_levels(vec_new(heap->arena)), term_univ(heap, level_var(heap, nat_zero()))));
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-transform")) : driver_die(1, "FAIL surface-transform");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
