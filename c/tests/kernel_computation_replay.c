/* A Comp retains its captured continuation state when callers run it more than once. */
#include "driver.h"

static bool inferred_level(Heap *heap, Result result, const LevelVar *expected) {
  Result inferred = check_infer_result(result);
  if (inferred.kind != RESULT_DONE) return false;
  const Value *ty = inferred.value;
  return ty->kind == VALUE_UNIV && level_equal(heap, ty->level, expected);
}

static int run(Heap *heap) {
  Ctx ctx = check_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  const LevelVar *one = level_succ(heap, level_zero());
  const LevelVar *two = level_succ(heap, one);
  Comp saved = check_engine_infer_uses(heap, ctx, QUANTITY_MANY, term_univ(heap, level_zero()));
  if (!inferred_level(heap, budget_run(heap, saved, budget_unlimited()), one))
    return driver_die(1, "first inference must infer Type 1");
  Comp other = check_engine_infer_uses(heap, ctx, QUANTITY_MANY, term_univ(heap, one));
  if (!inferred_level(heap, budget_run(heap, other, budget_unlimited()), two))
    return driver_die(1, "intervening inference must infer Type 2");
  if (!inferred_level(heap, budget_run(heap, saved, budget_unlimited()), one))
    return driver_die(1, "replayed inference must retain Type 1");
  return driver_print(str_lit("PASS checker computations retain captured state across repeated runs"));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
