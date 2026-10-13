/* Port of bend2/tests/kernel_conversion_budget.bend (oracle label unit-kernel-conversion-budget). */
#include "driver.h"
#include "levels_protocol.h"

static MaybeStr some_text(const char *text) {
  MaybeStr m;
  m.some = true;
  m.value = str_lit(text);
  return m;
}

static const LevelVar *var(Heap *heap, uint64_t index) { return level_var(heap, heap_nat_of_u64(heap, index)); }

static const LevelVar *left(Heap *heap) {
  return level_imax(heap, var(heap, 0), level_max(heap, var(heap, 1), var(heap, 2)));
}

static const LevelVar *right(Heap *heap) {
  return level_max(heap, level_imax(heap, var(heap, 0), var(heap, 1)), level_imax(heap, var(heap, 0), var(heap, 2)));
}

static Ctx three(Heap *heap) {
  return context_make(heap, global_initial(heap), budget_unlimited(), heap_nat_of_u64(heap, 3));
}

static MaybeLevelVar some_level(const LevelVar *level) {
  MaybeLevelVar m;
  m.some = true;
  m.value = level;
  return m;
}

static Shape coll(Heap *heap, uint64_t width) { return shape_scoll(big_of_nat(heap, heap_nat_of_u64(heap, width))); }

static Comp action(Heap *heap, bool collection) {
  if (collection) {
    return check_engine_conv_annotations(heap, coll(heap, 0), some_level(left(heap)), some_level(right(heap)));
  }
  return check_engine_conv_structural_whnf(heap, three(heap), value_vuniv(heap, left(heap)),
                                           value_vuniv(heap, right(heap)));
}

static TR failed(Heap *heap, const Measured *measured, uint32_t complete) {
  TR ok = test_expect(heap, measured->result, str_lit("budget"), some_text("the check budget is exhausted"));
  if (!ok.ok) return ok;
  return test_require(measured->count == complete, str_lit("conversion did not refuse the final measured poll"));
}

static bool same_of(const Completed *done) { return kernel_bool_unbox(done->value); }

static TR succeeded(const Completed *done, uint32_t complete) {
  return test_require(same_of(done) && done->count == complete,
                      str_lit("one-higher conversion budget did not succeed at the same count"));
}

static TR boundaries(Heap *heap, const Completed *done, bool collection) {
  uint32_t complete = done->count;
  TR ok = test_require(same_of(done) && complete > 64, str_lit("conversion omitted symbolic comparison polls"));
  if (!ok.ok) return ok;
  TR refused = test_measure(heap, action(heap, collection), true, complete - 1);
  if (!refused.ok) return refused;
  ok = failed(heap, (const Measured *)refused.value, complete);
  if (!ok.ok) return ok;
  TR accepted = test_measure(heap, action(heap, collection), true, complete);
  if (!accepted.ok) return accepted;
  TR finished = test_completed(heap, *(const Measured *)accepted.value);
  if (!finished.ok) return finished;
  return succeeded((const Completed *)finished.value, complete);
}

static TR check(Heap *heap, bool collection) {
  TR measured = test_measure(heap, action(heap, collection), false, 0);
  if (!measured.ok) return measured;
  TR done = test_completed(heap, *(const Measured *)measured.value);
  if (!done.ok) return done;
  return boundaries(heap, (const Completed *)done.value, collection);
}

static TR short_result(const Completed *done) {
  return test_require(done->count == 0 && !same_of(done), str_lit("unequal shapes polled their ignored annotations"));
}

static TR short_circuit(Heap *heap) {
  Closure diagram = value_close(vec_new(heap->arena), term_sec(heap, coll(heap, 0), vec_new(heap->arena)));
  Comp formers = check_engine_conv_formers(heap, three(heap), coll(heap, 0), diagram, some_level(left(heap)),
                                           coll(heap, 1), diagram, some_level(right(heap)));
  TR measured = test_measure(heap, formers, true, 0);
  if (!measured.ok) return measured;
  TR done = test_completed(heap, *(const Measured *)measured.value);
  if (!done.ok) return done;
  return short_result((const Completed *)done.value);
}

static int finish(TR result) {
  if (!result.ok) return driver_die_text(1, result.error);
  return driver_print(
      str_lit("PASS conversion level budgets: universes, annotations, exact boundaries, shape short circuit"));
}

static Comp universe_action(Heap *heap) {
  const LevelVar *top = level_succ(heap, level_max(heap, left(heap), right(heap)));
  return check_engine_conv(heap, three(heap), value_vuniv(heap, top), value_vuniv(heap, left(heap)),
                           value_vuniv(heap, right(heap)));
}

/* Test.R(U32): the Done value points at the measured count. */
static TR universe_counts(const Completed *measured, const Completed *reference) {
  TR ok = test_require(same_of(measured) && same_of(reference) && measured->count == reference->count,
                       str_lit("universe value conversion changed comparison polls"));
  if (!ok.ok) return ok;
  ok = test_require(measured->count > 0, str_lit("universe conversion omitted comparison polls"));
  if (!ok.ok) return ok;
  return test_done(&measured->count);
}

static TR universe_dispatch(Heap *heap) {
  TR measured = test_measure(heap, universe_action(heap), false, 0);
  if (!measured.ok) return measured;
  TR done = test_completed(heap, *(const Measured *)measured.value);
  if (!done.ok) return done;
  TR reference = test_measure(
      heap, check_engine_conv_type(heap, three(heap), value_vuniv(heap, left(heap)), value_vuniv(heap, right(heap))),
      false, 0);
  if (!reference.ok) return reference;
  TR structural = test_completed(heap, *(const Measured *)reference.value);
  if (!structural.ok) return structural;
  TR counted = universe_counts((const Completed *)done.value, (const Completed *)structural.value);
  if (!counted.ok) return counted;
  uint32_t complete = *(const uint32_t *)counted.value;
  TR refused = test_measure(heap, universe_action(heap), true, complete - 1);
  if (!refused.ok) return refused;
  TR ok = failed(heap, (const Measured *)refused.value, complete);
  if (!ok.ok) return ok;
  TR accepted = test_measure(heap, universe_action(heap), true, complete);
  if (!accepted.ok) return accepted;
  TR finished = test_completed(heap, *(const Measured *)accepted.value);
  if (!finished.ok) return finished;
  return succeeded((const Completed *)finished.value, complete);
}

static TR run(Heap *heap) {
  TR ok = check(heap, false);
  if (!ok.ok) return ok;
  ok = check(heap, true);
  if (!ok.ok) return ok;
  ok = short_circuit(heap);
  if (!ok.ok) return ok;
  return universe_dispatch(heap);
}

/* arguments(args) ignores args; main() = finish(run()). */
int kernel_conversion_budget_run(Heap *heap) { return finish(run(heap)); }

#ifndef MECH_DRIVER_LIBRARY
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = kernel_conversion_budget_run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
#endif
