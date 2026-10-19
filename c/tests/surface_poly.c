/* Port of tests/surface_poly.bend (oracle label unit-surface-poly): universe-polymorphic
 * definitions, families, specializations and groups through the program parser, six rejected
 * programs, and one show/parse roundtrip of a term. */
#include "driver.h"
#include "surface.h"

/* accepted(result) */
static bool accepted(Result result) { return result.kind == RESULT_DONE; }

/* rejected(source) */
static bool rejected(Heap *heap, const char *source) { return !accepted(parser_program(heap, str_lit(source))); }

/* The INDEX-th element of a Vec of pointers, NULL past the end. */
static const void *item(const Vec *vec, size_t index) {
  VecGet got = vec_get(vec, index);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

/* poly(result): [DPoly{1, "universe", SSort{SuccLevel{VarLevel{0}}}, _}] */
static bool poly(Heap *heap, Result result) {
  if (!accepted(result)) return false;
  const Vec *decls = (const Vec *)result.value;
  const SDecl *decl = (const SDecl *)item(decls, 0);
  if (decl == NULL || item(decls, 1) != NULL || decl->kind != SDECL_POLY) return false;
  if (!str_eq(heap_nat_show(heap, decl->universes), str_lit("1")) || decl->ty->kind != STERM_SORT) return false;
  const Universe *sort = decl->ty->sort;
  if (sort->kind != UNIVERSE_SUCC_LEVEL || sort->left->kind != UNIVERSE_VAR_LEVEL) return false;
  return nat_is_zero(sort->left->index) && str_eq(decl->name, str_lit("universe"));
}

/* roundtrip_result(expected, result) */
static bool roundtrip_result(Heap *heap, Str expected, Result result) {
  if (!accepted(result)) return false;
  return str_eq(expected, syntax_show(heap, (const STerm *)result.value));
}

/* roundtrip(result) */
static bool roundtrip(Heap *heap, Result result) {
  if (!accepted(result)) return false;
  Str shown = syntax_show(heap, (const STerm *)result.value);
  return roundtrip_result(heap, shown, parser_term(heap, shown));
}

/* main() */
static bool suite(Heap *heap) {
  return poly(heap, parser_program(heap, str_lit("poly (u) def universe : Sort (succ u) := Sort u"))) &&
         accepted(parser_program(heap, str_lit("poly (u) mu Box (A : Sort u) : Sort u with | box : A -> Box A"))) &&
         accepted(parser_program(
             heap, str_lit("poly (u) mu Box (A : Sort u) : Sort u with | box : A -> Box A where def id (x : Sort u) "
                           ": Sort u := x end"))) &&
         accepted(parser_program(heap, str_lit("specialize Box (0) as SmallBox with (A := Nat) export (id := smallId)"))) &&
         accepted(parser_program(
             heap, str_lit("poly (u) group Bundle where specialize Box (u) as B export () def id (x : Sort u) : Sort u "
                           ":= x end"))) &&
         accepted(parser_program(heap, str_lit("poly (u) group Bundle where specialize Box (u) as B end"))) &&
         rejected(heap, "poly (u, u) def a : Type := Type") &&
         rejected(heap, "poly (succ) def a : Type := Type") &&
         rejected(heap, "poly (u) def a : Sort missing := Type") &&
         rejected(heap, "poly (u) group Empty where end") &&
         rejected(heap, "specialize Box (0) as B export () with (A := Nat)") &&
         rejected(heap, "specialize Box (0) as B export () export ()") &&
         roundtrip(heap, parser_term(heap, str_lit("fun (0 A : Type) (x : A) => case x as z return A with | 0 => x "
                                                   "| 1 => x")));
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-poly")) : driver_die(1, "FAIL surface-poly");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
