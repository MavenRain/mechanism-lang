/* surface_compact_swap: host-only check of the compact elaboration path (elab_check_in_compact: one arena per
 * group member and per composition member, compact_group_swap and compact_compose_swap between them) against
 * the budget-unlimited path (elab_check_in) on programs whose entries swap arenas with a non-empty row list
 * (definitions checked before the group, the rows the swap must copy) and a non-empty pending member list (a
 * specialize with a where member inside the group, the members the composition swap must copy). Decision 156:
 * the Done values of the two paths are equal, row for row. No Bend counterpart: the compact path is host only. */
#include "driver.h"
#include "surface.h"

/* box(): the Box family of surface_program (same escapes). */
static const char *BOX =
  "poly (u) mu Box (0 A : Sort u) : Sort (succ u) with | box : A -> Box A where def unbox : (0 A : Sort u) -> Box A -> A := fun (0 A : Sort u) (b : Box A) => match b as self in Box return A with | box (x : A) => x end\n";

/* One program per swap site. The source is before ++ box() ++ after: box() is a POLY_GROUP entry (one mu
 * family plus a where definition, parser poly_family), so the definitions in `before` are the rows the group
 * swap must copy (the m7 site, elab.c compact_group_swap); a composition entry after rows is the m8 site
 * (compact_compose_swap); a composition whose specialize line has concrete levels is not renamed
 * (family_poly.c compose_dependency: renamed = level_distinct_vars && global_extended_by), so the template
 * members join Composed.members and are the pending list the composition swap must copy (the m9 site,
 * compact.c compact_family_compose_state). */
typedef struct {
  const char *name;
  const char *before;
  const char *after;
} Program;

static const Program PROGRAMS[] = {
  {"group-after-rows",
   "def one : Nat := 1 def two : Nat := 2 ",
   "specialize Box (1) as B def y : Nat := B_unbox Nat (box one)"},
  {"compose-after-rows",
   "",
   "def one : Nat := 1 poly (u) group Bundle where specialize Box (u) as Inner end specialize Bundle (1) as Run "
   "def x : Nat := Run_Inner_unbox Nat (box 37)"},
  {"compose-with-pending-members",
   "",
   "def one : Nat := 1 poly (u) group Fixed where specialize Box (1) as Inner end specialize Fixed (1) as Run "
   "def x : Nat := Run_Inner_unbox Nat (box one)"},
  {"compose-with-member-after-rows",
   "",
   "def one : Nat := 1 poly (u) group Both where specialize Box (u) as Inner def keep : (0 A : Sort u) -> Inner A "
   "-> A := fun (0 A : Sort u) (b : Inner A) => Inner_unbox A b end specialize Both (1) as Run "
   "def z : Nat := Run_keep Nat (box one)"},
};

static Result done(void *value) { return (Result){RESULT_DONE, value, {0}}; }
static Result fail(Error error) { return (Result){RESULT_FAIL, NULL, error}; }
static Result mismatch(Str message) { return fail(error_make(ERROR_MISMATCH, message)); }
static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, const char *b, Str c) { return cat(heap, cat(heap, a, str_lit(b)), c); }

/* checked(path, result): the ElabChecked of a Done result, or the failure prefixed with the path name. */
static Result checked(Heap *heap, const char *path, Str name, Result result, const ElabChecked **out) {
  switch (result.kind) {
  case RESULT_FAIL:
    return mismatch(cat3(heap, cat3(heap, str_lit(path), " refused ", name), ": ", error_to_string(heap, result.error)));
  case RESULT_DONE:
    break;
  }
  *out = result.value;
  if (*out == NULL) return mismatch(cat3(heap, str_lit(path), " returned no checked program for ", name));
  return done(NULL);
}

/* row_text(rows, i): the entry text of row i, or an empty Str past the end. */
static Str row_text(Heap *heap, const Vec *rows, size_t i) {
  VecGet got = vec_get(rows, i);
  if (got.status != VEC_GET_OK) return str_lit("");
  return elab_entry_text(heap, got.value);
}

/* same_rows(name, expected, compact): the compact rows equal the unlimited rows, index by index. */
static Result same_rows(Heap *heap, Str name, const Vec *expected, const Vec *compact) {
  if (expected->len != compact->len)
    return mismatch(cat3(heap,
                         cat3(heap, cat(heap, str_lit("compact row count "), heap_nat_show(heap, heap_nat_of_u64(heap, compact->len))),
                              " differs from ", heap_nat_show(heap, heap_nat_of_u64(heap, expected->len))),
                         " for ", name));
  for (size_t i = 0; i < expected->len; i++) {
    Str want = row_text(heap, expected, i);
    Str have = row_text(heap, compact, i);
    if (!str_eq(want, have))
      return mismatch(cat3(heap,
                           cat3(heap, cat3(heap, cat(heap, str_lit("compact row "), heap_nat_show(heap, heap_nat_of_u64(heap, i))),
                                           " of ", name),
                                " is ", have),
                           " not ", want));
  }
  return done(NULL);
}

/* run_program(program): both paths accept, the rows agree and are non-empty. */
static Result run_program(Heap *heap, Program program, size_t *rows) {
  Str name = str_lit(program.name);
  Str source = cat3(heap, str_lit(program.before), BOX, str_lit(program.after));
  const ElabChecked *expected = NULL;
  const ElabChecked *compact = NULL;
  Result r = checked(heap, "unlimited", name, elab_check_in(heap, global_initial(heap), budget_unlimited(), source), &expected);
  if (r.kind == RESULT_FAIL) return r;
  r = checked(heap, "compact", name, elab_check_in_compact(heap, global_initial(heap), source), &compact);
  if (r.kind == RESULT_FAIL) return r;
  if (expected->rows.len < 2) return mismatch(cat(heap, str_lit("fewer than two rows for "), name));
  r = same_rows(heap, name, &expected->rows, &compact->rows);
  if (r.kind == RESULT_FAIL) return r;
  *rows = *rows + compact->rows.len;
  return done(NULL);
}

/* suite(): the do block stops at the first Fail. */
static Result suite(Heap *heap, size_t *rows) {
  size_t count = sizeof PROGRAMS / sizeof PROGRAMS[0];
  Result r = done(NULL);
  for (size_t i = 0; i < count && r.kind == RESULT_DONE; i++) r = run_program(heap, PROGRAMS[i], rows);
  return r;
}

/* report(result) */
static int report(Heap *heap, Result result, size_t rows) {
  if (result.kind == RESULT_DONE)
    return driver_print(cat3(heap,
                             cat(heap, str_lit("PASS surface compact swaps programs="),
                                 heap_nat_show(heap, heap_nat_of_u64(heap, sizeof PROGRAMS / sizeof PROGRAMS[0]))),
                             " rows=", heap_nat_show(heap, heap_nat_of_u64(heap, rows))));
  return driver_die_text(1, error_to_string(heap, result.error));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  size_t rows = 0;
  Result result = suite(&heap, &rows);
  int code = report(&heap, result, rows);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
