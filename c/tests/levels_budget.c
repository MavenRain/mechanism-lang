/* Port of tests/levels_budget.bend (oracle label unit-levels-budget). */
#include "driver.h"

static bool exhausted(Result result) {
  if (result.kind != RESULT_FAIL) return false;
  return result.error.kind == ERROR_BUDGET_EXHAUSTED;
}

static bool accepted(Result result) {
  if (result.kind != RESULT_DONE) return false;
  return kernel_bool_unbox(result.value);
}

static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS threaded universe budget and late exhaustion"));
  return driver_die(1, "FAIL threaded universe budget and late exhaustion");
}

static Budget polls(Heap *heap, uint64_t count) { return budget_of_polls(heap_nat_of_u64(heap, count)); }

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  const LevelVar *u = level_var(&heap, heap_nat_of_u64(&heap, 0));
  const LevelVar *v = level_succ(&heap, level_var(&heap, heap_nat_of_u64(&heap, 1)));
  const LevelVar *a = level_imax(&heap, u, v);
  const LevelVar *b = level_max(&heap, u, v);
  bool ok = exhausted(level_eq_equal_budget(&heap, polls(&heap, 0), a, b)) &&
            exhausted(level_eq_equal_budget(&heap, polls(&heap, 20), a, b)) &&
            accepted(level_eq_equal_budget(&heap, polls(&heap, 1000), a, b));
  int code = heap.oom ? driver_host_failure() : verify(ok);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
