/* Port of tests/surface_numbers.bend (55 lines, oracle label surface-numbers): sixteen numeric surface
 * cases checked against G.initial() with the frozen checked form or refusal text, then a huge-width inj
 * round trip. Prints SURFACE-NUMBERS-OK fields or SURFACE-NUMBERS-FAIL message. No arguments. The case
 * table is generated from the Bend literals by W/hop279-run.sh (byte for byte). */
#include "driver.h"
#include "relational_common.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* NumericCase{source, success, expected} */
typedef struct {
  const char *source;
  bool success;
  const char *expected;
} NumericCase;

/* cases() */
static const NumericCase cases[] = {
  {"def p : Type 999999999999999999 := Prop", false, "mismatch: the term has type Type 1 and the expected type is Type 1000000000000000000\n"},
  {"def p : Sort (succ 999999999999999999) := Sort 999999999999999999", true, "def p : Type 1000000000000000000 := Type 999999999999999999\n\n"},
  {"def p : Nat := (tuple (0)).999999999999999999", false, "cannot infer: a section has no type of its own;  it needs an expected type\n"},
  {"def p : Nat := (tuple (0) : prod (Nat)).999999999999999999", false, "wrong leg: the leg address is outside the collection\n"},
  {"def p : Nat := (tuple (missing) : prod (Nat)).999999999999999999", false, "unbound: missing\n"},
  {"def p : sum (Nat) := inj 999999999999999999 of 1 0", false, "wrong leg: the leg address is outside the collection\n"},
  {"def p : sum (Nat) := inj 999999999999999999 of 1 missing", false, "wrong leg: the leg address is outside the collection\n"},
  {"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 (x : Nat) => x", false, "wrong leg: the leg address is outside the collection\n"},
  {"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 => 0", false, "missing branch: each branch binds its payload once\n"},
  {"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 (x : Nat) (y : Nat) => 0", false, "missing branch: each branch binds its payload once\n"},
  {"def p : Nat := (0).999999999999999999", false, "mismatch: a projection reads a pair or a tuple, and this scrutinee is neither\n"},
  {"def p : Nat := (tuple (0) : prod (Nat)).0", true, "def p : Nat := (Out SColl 1 (ALeg 0) ((Sec SColl 1 [ => 0]) : (Ran SColl 1 (Sec SColl 1 [ => Nat]))))\n\n"},
  {"def p : sum (Nat) := inj 0 of 1 0", true, "def p : (Lan SColl 1 (Sec SColl 1 [ => Nat])) := (In SColl 1 (ALeg 0) [0])\n\n"},
  {"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 0 (x : Nat) => x", true, "def p : Nat := (Elim SColl 1 ((In SColl 1 (ALeg 0) [0]) : (Lan SColl 1 (Sec SColl 1 [ => Nat]))) as s return Nat with | (ALeg 0) x => x)\n\n"},
  {"def p : Type 1000000000000000000 := Prop", false, "line 1, column 14: numeric literal too long\n"},
  {"def p : sum (Nat) := inj 1000000000000000000 of 1 0", false, "line 1, column 26: numeric literal too long\n"}
};

/* verify(success, expected, actual) */
static Task verify(Heap *heap, bool success, Str expected, Result actual) {
  switch (actual.kind) {
  case RESULT_DONE:
    if (success)
      return relational_require(
          str_eq(cat(heap, elab_checked_form(heap, (const Vec *)actual.value), str_lit("\n")), expected),
          str_lit("numeric checked form differs from original"));
    return task_fail(str_lit("numeric negative was accepted"));
  case RESULT_FAIL:
    if (success) return task_fail(error_to_string(heap, actual.error));
    return relational_require(
        str_eq(cat(heap, error_to_string(heap, actual.error), str_lit("\n")), expected),
        cat(heap, str_lit("numeric refusal differs from original: "), error_to_string(heap, actual.error)));
  }
  return task_fail(str_lit("unknown result kind"));
}

/* replay(rows, count) */
static Task replay(Heap *heap, uint64_t *count) {
  size_t len = sizeof cases / sizeof cases[0];
  for (size_t i = 0; i < len; i++) {
    RELATIONAL_TRY(verify(heap, cases[i].success, str_lit(cases[i].expected),
                          elab_check_text(heap, global_initial(heap), budget_unlimited(), str_lit(cases[i].source))));
    *count += 1;
  }
  return task_pure();
}

/* suite() */
static Task suite(Heap *heap, Str *fields) {
  uint64_t count = 0;
  const Vec *parsed = NULL;
  RELATIONAL_TRY(replay(heap, &count));
  RELATIONAL_TRY(relational_parse_roundtrip(
      heap, str_lit("def p : sum (Nat) := inj 999999999999999999 of 999999999999999999 0"), &parsed));
  *fields = cat(heap, cat(heap, str_lit("oracle="), str_of_u64(heap->arena, count)), str_lit(" huge-width-roundtrip=1"));
  return task_pure();
}

/* arguments(args) = R.finish("SURFACE-NUMBERS", suite()); main() = arguments([]) */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str fields = str_empty();
  Task result = suite(&heap, &fields);
  int code = relational_finish(&heap, str_lit("SURFACE-NUMBERS"), result, fields);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
