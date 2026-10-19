/* Port of tests/surface_parser.bend (oracle label unit-surface-parser): the term parser over
 * an arrow, a projection, a big number, nine accepted and four rejected sources, and the program
 * parser over a two-declaration source. */
#include "driver.h"
#include "surface.h"

/* accepted(result) */
static bool accepted(Result result) { return result.kind == RESULT_DONE; }

/* rejected(result) */
static bool rejected(Result result) { return !accepted(result); }

/* The Done payload of parser_term. */
static const STerm *term_of(Result result) { return (const STerm *)result.value; }

/* The INDEX-th element of a Vec of pointers, NULL past the end. */
static const void *item(const Vec *vec, size_t index) {
  VecGet got = vec_get(vec, index);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

/* Binder{QZero, NAME, SType{zero}} */
static bool type_binder(SBinder binder, const char *name) {
  return binder.quantity == QUANTITY_ZERO && str_eq(binder.name, str_lit(name)) && binder.ty->kind == STERM_TYPE &&
         big_equal(binder.ty->nat, big_zero());
}

/* arrow(result): SArrow{Binder{QZero, "A", Type}, SArrow{Binder{QZero, "B", Type}, SVar{"A"}}} */
static bool arrow(Result result) {
  if (!accepted(result)) return false;
  const STerm *outer = term_of(result);
  if (outer->kind != STERM_ARROW || !type_binder(outer->binder, "A")) return false;
  const STerm *inner = outer->body;
  if (inner->kind != STERM_ARROW || !type_binder(inner->binder, "B")) return false;
  return inner->body->kind == STERM_VAR && str_eq(inner->body->name, str_lit("A"));
}

/* projection(result): SApp{SVar{"f"}, SProj{SVar{"x"}, 10}} */
static bool projection(Heap *heap, Result result) {
  if (!accepted(result)) return false;
  const STerm *app = term_of(result);
  if (app->kind != STERM_APP || app->head->kind != STERM_VAR || app->arg->kind != STERM_PROJ) return false;
  const STerm *proj = app->arg;
  if (proj->head->kind != STERM_VAR || !str_eq(big_to_string(heap, proj->index), str_lit("10"))) return false;
  return str_eq(app->head->name, str_lit("f")) && str_eq(proj->head->name, str_lit("x"));
}

/* big(result): SNat{123456789012345678901234567890} */
static bool big(Heap *heap, Result result) {
  if (!accepted(result)) return false;
  const STerm *term = term_of(result);
  return term->kind == STERM_NAT &&
         str_eq(big_to_string(heap, term->nat), str_lit("123456789012345678901234567890"));
}

/* declarations(result): [DMu{[Fam{"Box", _, _, [ctor]}]}, DDef{"identity", _, _}] */
static bool declarations(Result result) {
  if (!accepted(result)) return false;
  const Vec *decls = (const Vec *)result.value;
  const SDecl *mu = (const SDecl *)item(decls, 0);
  const SDecl *def = (const SDecl *)item(decls, 1);
  if (mu == NULL || def == NULL || item(decls, 2) != NULL) return false;
  if (mu->kind != SDECL_MU || def->kind != SDECL_DEF) return false;
  const SFam *family = (const SFam *)item(&mu->families, 0);
  if (family == NULL || item(&mu->families, 1) != NULL) return false;
  if (item(&family->ctors, 0) == NULL || item(&family->ctors, 1) != NULL) return false;
  return str_eq(family->name, str_lit("Box")) && str_eq(def->name, str_lit("identity"));
}

/* main() */
static bool suite(Heap *heap) {
  return arrow(parser_term(heap, str_lit("(0 A B : Type) -> A"))) &&
         projection(heap, parser_term(heap, str_lit("f x.10"))) &&
         big(heap, parser_term(heap, str_lit("123456789012345678901234567890"))) &&
         accepted(parser_term(heap, str_lit("fun (A : Type) (1 x : A) => let y : A := x in y"))) &&
         accepted(parser_term(heap, str_lit("case x as z return Type with | 0 (x : Type) => x | 1 => ()"))) &&
         accepted(parser_term(heap, str_lit("match x as z in Vec n return Type with | Cons (0 A : Type) n x xs => x"))) &&
         accepted(parser_term(heap, str_lit("mpc[P, A] (fhc level (zk (0 w : W) * R))"))) &&
         accepted(parser_term(heap, str_lit("mpc P f (share x) (input p y)"))) &&
         accepted(parser_term(heap, str_lit("open P 0 proof (eval f (enc pk x))"))) &&
         accepted(parser_term(heap, str_lit("tuple (sum (), prod (Type, Prop), b\"ok\")"))) &&
         rejected(parser_term(heap, str_lit("match x with | 0 => x"))) &&
         rejected(parser_term(heap, str_lit("nu"))) &&
         rejected(parser_term(heap, str_lit("x ."))) &&
         rejected(parser_term(heap, str_lit("x def"))) &&
         declarations(parser_program(
             heap, str_lit("mu Box (A : Type) : Type with | box (x : A) : Box A def identity (A : Type) (x : A) : A := x")));
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-parser")) : driver_die(1, "FAIL surface-parser");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
