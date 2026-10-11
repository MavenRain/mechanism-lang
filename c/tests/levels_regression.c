/* Port of bend2/tests/levels_regression.bend (oracle label unit-levels-regression). */
#include "driver.h"

typedef struct {
  const char *name;
  bool holds;
} Case;

static bool subst_ok(Heap *heap, MaybeLevelVar value) {
  if (!value.some) return false;
  return level_equal(heap, value.value, level_of_u32(heap, 3));
}

static bool absent(MaybeLevelVar value) { return !value.some; }

static bool big_check(Heap *heap, MaybeBig value) {
  if (!value.some) return false;
  const LevelVar *largest = level_var_offset(heap, value.value, level_zero());
  const LevelVar *next = level_succ(heap, largest);
  bool ordered = level_le(heap, largest, next) && !level_le(heap, next, largest);
  bool nonzero = !level_equal(heap, next, level_zero());
  bool shown = str_eq(level_to_string(heap, next), str_lit("4611686018427387904"));
  return ordered && nonzero && shown;
}

/* List<T> as a Vec of LevelVar pointers. */
static Vec levels(Heap *heap, const LevelVar *first, const LevelVar *second, bool two) {
  Vec xs = vec_new(heap->arena);
  bool ok = vec_push(&xs, (void *)first) && (!two || vec_push(&xs, (void *)second));
  if (!ok) heap->oom = true;
  return xs;
}

static Case make_case(const char *name, bool holds) {
  Case c;
  c.name = name;
  c.holds = holds;
  return c;
}

#define CASE_COUNT 17

static void cases(Heap *h, Case *out) {
  const LevelVar *u = level_var(h, heap_nat_of_u64(h, 0));
  const LevelVar *v = level_var(h, heap_nat_of_u64(h, 1));
  const LevelVar *w = level_var(h, heap_nat_of_u64(h, 2));
  const LevelVar *zero = level_zero();
  const LevelVar *one = level_one(h);
  const LevelVar *eight = level_of_u32(h, 8);
  Vec two_args = levels(h, level_of_u32(h, 3), zero, true);
  Vec one_arg = levels(h, level_of_u32(h, 3), NULL, false);
  out[0] = make_case("imax-zero", level_equal(h, level_imax(h, u, zero), zero));
  out[1] = make_case("imax-zero-left", level_equal(h, level_imax(h, zero, u), u));
  out[2] = make_case("imax-self", level_equal(h, level_imax(h, u, u), u));
  out[3] = make_case("imax-distribution", level_equal(h, level_max(h, level_imax(h, u, v), level_imax(h, u, w)),
                                                      level_imax(h, u, level_max(h, v, w))));
  out[4] = make_case("successor-distribution", level_equal(h, level_max(h, level_succ(h, u), level_succ(h, v)),
                                                           level_succ(h, level_max(h, u, v))));
  out[5] = make_case("successor-domination",
                     level_equal(h, level_succ(h, u), level_max(h, level_succ(h, u), level_max(h, u, one))));
  out[6] = make_case("max-idempotent", level_equal(h, level_max(h, u, v),
                                                   level_max(h, level_max(h, v, u), level_max(h, u, v))));
  out[7] = make_case("imax-positive",
                     level_equal(h, level_max(h, u, level_succ(h, v)), level_imax(h, u, level_succ(h, v))));
  out[8] = make_case("imax-conditional", !level_equal(h, level_imax(h, u, v), level_max(h, u, v)));
  out[9] = make_case("symbolic-zero", !level_equal(h, u, zero) && !level_always_positive(u) &&
                                          level_always_positive(level_succ(h, u)));
  out[10] = make_case("conditional-positive", !level_always_positive(level_imax(h, one, u)) &&
                                                  level_always_positive(level_imax(h, u, one)));
  out[11] = make_case("le-successor", level_le(h, u, level_succ(h, u)) && !level_le(h, level_succ(h, u), u));
  out[12] = make_case("le-imax", level_le(h, level_imax(h, u, v), level_max(h, u, v)) &&
                                     !level_le(h, level_max(h, u, v), level_imax(h, u, v)));
  out[13] = make_case("no-finite-sampling",
                      !level_equal(h, level_max(h, eight, u), eight) && !level_le(h, u, eight));
  out[14] = make_case("beyond-host-int", big_check(h, big_of_decimal(h, str_lit("4611686018427387903"))));
  out[15] = make_case("scope", level_in_scope(heap_nat_of_u64(h, 1), u) &&
                                   !level_in_scope(heap_nat_of_u64(h, 0), u) &&
                                   !level_in_scope(heap_nat_of_u64(h, 1), level_max(h, u, v)));
  out[16] = make_case("substitution",
                      subst_ok(h, level_subst(h, &two_args, level_imax(h, u, level_max(h, v, one)))) &&
                          absent(level_subst(h, &one_arg, level_max(h, u, v))));
}

static int run(Heap *heap, const Case *all) {
  size_t i = 0;
  while (i < CASE_COUNT) {
    if (!all[i].holds) {
      Str message = heap_str_cat(heap, str_lit("FAIL "), str_lit(all[i].name));
      if (heap->oom) return driver_host_failure();
      fwrite(message.data, 1, message.len, stderr);
      fputc('\n', stderr);
      return 1;
    }
    i += 1;
  }
  return driver_print(str_lit("PASS 17 original universe algebra regression families"));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Case all[CASE_COUNT];
  cases(&heap, all);
  int code = heap.oom ? driver_host_failure() : run(&heap, all);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
