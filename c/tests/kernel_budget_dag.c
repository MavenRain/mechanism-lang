/* Port of bend2/tests/kernel_budget_dag.bend (oracle label unit-kernel-budget-dag). */
#include "driver.h"

/* level_dag(depth): a level DAG of the given depth, each node sharing one prior node twice. */
static const LevelVar *level_dag(Heap *heap, uint64_t depth) {
  if (depth == 0) return level_zero();
  const LevelVar *prior = level_dag(heap, depth - 1);
  return level_max(heap, prior, prior);
}

/* term_dag(depth): a term DAG of the given depth, each Ran sharing one prior term twice. */
static const Term *term_dag(Heap *heap, uint64_t depth) {
  if (depth == 0) return term_univ(heap, level_zero());
  const Term *prior = term_dag(heap, depth - 1);
  return term_ran(heap, shape_spi(QUANTITY_ZERO, str_lit("x"), (void *)prior), prior);
}

/* extracted(result): Done -> true, Fail -> false. */
static bool extracted(Result result) { return result.kind == RESULT_DONE; }

/* exhausted(result): Fail{Budget_exhausted} -> true, anything else -> false. */
static bool exhausted(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_BUDGET_EXHAUSTED;
}

static int verify(bool ok) {
  if (ok) {
    return driver_print(
        str_lit("PASS successful universe extraction skips rendering; shared term DAG exhausts bounded polls"));
  }
  return driver_die(1, "FAIL DAG budget regression");
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Ctx ctx = context_make(&heap, global_initial(&heap), budget_unlimited(), nat_zero());
  /* Bool.and is strict: both runs happen. */
  bool skipped = extracted(budget_run(
      &heap, check_engine_universe_value(&heap, value_vuniv(&heap, level_dag(&heap, 60)), ctx),
      budget_of_polls(nat_zero())));
  bool bounded = exhausted(budget_run(&heap, check_engine_infer_uses(&heap, ctx, QUANTITY_ZERO, term_dag(&heap, 40)),
                                      budget_of_polls(heap_nat_of_u64(&heap, 100))));
  bool ok = skipped && bounded;
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
