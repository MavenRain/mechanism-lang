/* Port of bend2/tests/surface_elab.bend (oracle label unit-surface-elab): parses, elaborates and infers
 * nine term sources against the initial globals; seven must be accepted and two rejected. */
#include "driver.h"
#include "surface.h"

/* One accepts(source) or rejects(source) line of main(). */
typedef struct {
  bool accepts;
  const char *source;
} Case;

static const Case CASES[] = {
  {true, "Type"},
  {true, "1234567890123456789012345678901234567890"},
  {true, "(fun (0 A : Type) (1 x : A) => x : (0 A : Type) -> (1 x : A) -> A)"},
  {true, "let x : Nat := 7 in x"},
  {true, "((1, 2) : Nat * Nat).1"},
  {true, "((1, 2) : Nat * Nat).2"},
  {true, "Nat * Nat"},
  {false, "unknown"},
  {false, "(fun (1 x : Nat) => 7 : (1 x : Nat) -> Nat)"},
};

/* elaborate(source): Parser.term, Elab.elab with no expected type, Check.infer under QMany; the term. */
static Result elaborate(Heap *heap, Str source) {
  Ctx ctx = check_make(heap, global_initial(heap), budget_unlimited(), nat_zero());
  Result syntax = parser_term(heap, source);
  if (syntax.kind != RESULT_DONE) return syntax;
  MaybeValue none = {0}; /* None{} */
  Result term = elab_elab(heap, ctx, none, (const STerm *)syntax.value);
  if (term.kind != RESULT_DONE) return term;
  Result ty = check_infer(heap, ctx, QUANTITY_MANY, (const Term *)term.value);
  if (ty.kind != RESULT_DONE) return ty;
  return term;
}

/* accept_result(result, source) */
static int accept_result(Heap *heap, Result result, Str source) {
  if (result.kind == RESULT_DONE) return 0;
  Str tail = str_cat(heap->arena, str_lit(": "), error_to_string(heap, result.error));
  return driver_die_text(1, str_cat(heap->arena, source, tail));
}

/* reject_result(result, source) */
static int reject_result(Heap *heap, Result result, Str source) {
  if (result.kind != RESULT_DONE) return 0;
  return driver_die_text(1, str_cat(heap->arena, str_lit("unexpected acceptance: "), source));
}

/* accepts(source) / rejects(source) */
static int run_case(Heap *heap, Case c) {
  Str source = str_lit(c.source);
  Result result = elaborate(heap, source);
  if (c.accepts) return accept_result(heap, result, source);
  return reject_result(heap, result, source);
}

/* main(): the do block runs the cases in order and stops at the first die. */
static int run(Heap *heap) {
  size_t count = sizeof CASES / sizeof CASES[0];
  int code = 0;
  for (size_t i = 0; i < count && code == 0; i++) code = run_case(heap, CASES[i]);
  if (code != 0) return code;
  return driver_print(str_lit("PASS surface elaboration and trusted checking"));
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
