/* Port of tests/kernel_declarations.bend (oracle label unit-kernel-declarations). */
#include "driver.h"

/* success(result): Done -> True; Fail -> False */
static bool success(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return true;
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* index_error(result): Fail{Error.Index_not_zero{msg}} -> True; _ -> False */
static bool index_error(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return false;
    case RESULT_FAIL:
      return result.error.kind == ERROR_INDEX_NOT_ZERO;
  }
  return false;
}

/* budget_error(result): Fail{Error.Budget_exhausted{msg}} -> True; _ -> False */
static bool budget_error(Result result) {
  switch (result.kind) {
    case RESULT_DONE:
      return false;
    case RESULT_FAIL:
      return result.error.kind == ERROR_BUDGET_EXHAUSTED;
  }
  return false;
}

static bool one_str(const Vec *names, const char *expected) {
  VecGet got = vec_get(names, 0);
  if (names->len != 1 || got.status != VEC_GET_OK || got.value == NULL) return false;
  return str_eq(*(const Str *)got.value, str_lit(expected));
}

/* positive_family(fam): Some{Family{name, p, i, l, Complete{[ctor]}, [Ctor{c, [], [], 0n, False}], True}} ->
 * name == "Flag" && ctor == "flag" && c == "flag"; _ -> False */
static bool positive_family(MaybeFamily fam) {
  if (!fam.some) return false;
  Family f = fam.value;
  if (f.status.kind != CTOR_STATUS_COMPLETE || !f.positive || f.ctors.len != 1) return false;
  VecGet got = vec_get(&f.ctors, 0);
  if (got.status != VEC_GET_OK || got.value == NULL) return false;
  const Ctor *c = got.value;
  if (c->args.len != 0 || c->res_idx.len != 0 || !nat_is_zero(c->full_arity) || c->self_rec) return false;
  return str_eq(f.name, str_lit("Flag")) && one_str(&f.status.names, "flag") && str_eq(c->name, str_lit("flag"));
}

/* installed(result): Done{globals} -> positive_family(Global.find_family("Flag", globals)); Fail -> False */
static bool installed(Result result) {
  switch (result.kind) {
    case RESULT_DONE: {
      const Globals *globals = result.value;
      if (globals == NULL) return false;
      return positive_family(global_find_family(str_lit("Flag"), *globals));
    }
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* ["Flag"] */
static Vec flag_group(Heap *heap) {
  Vec group = vec_new(heap->arena);
  Str *name = heap_alloc(heap, sizeof *name);
  if (name != NULL) *name = str_lit("Flag");
  if (name == NULL || !vec_push(&group, name)) heap->oom = true;
  return group;
}

/* [Check.CtorDecl{"flag", [], [], []}] */
static Vec flag_ctors(Heap *heap) {
  Vec ctors = vec_new(heap->arena);
  CtorDecl *decl = heap_alloc(heap, sizeof *decl);
  if (decl != NULL) {
    decl->name = str_lit("flag");
    decl->args = vec_new(heap->arena);
    decl->res_params = vec_new(heap->arena);
    decl->res_idx = vec_new(heap->arena);
  }
  if (decl == NULL || !vec_push(&ctors, decl)) heap->oom = true;
  return ctors;
}

static Result define_flag(Heap *heap, Globals globals, Budget budget) {
  Vec group = flag_group(heap);
  return check_define_ctors(heap, globals, budget, &group, str_lit("Flag"), flag_ctors(heap));
}

/* family_test(result): Done{globals} -> installed(define_ctors(globals, unlimited, ["Flag"], "Flag", [flag])) &&
 * budget_error(define_ctors(globals, of_polls(1), ["Flag"], "Flag", [flag])); Fail -> False */
static bool family_test(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_DONE: {
      const Globals *globals = result.value;
      if (globals == NULL) return false;
      bool a = installed(define_flag(heap, *globals, budget_unlimited()));
      bool b = budget_error(define_flag(heap, *globals, budget_of_polls(heap_nat_of_u64(heap, 1))));
      return a && b;
    }
    case RESULT_FAIL:
      return false;
  }
  return false;
}

/* verify(ok): True -> IO.print(...); False -> IO.die(1, "FAIL declaration kernel") */
static int verify(bool ok) {
  if (ok) return driver_print(str_lit("PASS declarations, family installation, erased indices, late budget"));
  return driver_die(1, "FAIL declaration kernel");
}

static Decl decl_of(Heap *heap, const char *name, const Term *body) {
  Decl d;
  d.name = str_lit(name);
  d.kind = DECL_DEFINITION;
  d.ty = prim_nat_ty(heap);
  d.body.some = true;
  d.body.value = body;
  return d;
}

/* declaration() = Check.Decl{"zero", Definition, Prim.nat_ty(), Some{Term.Lit{Lit.LInt{Big.zero()}}}} */
static Decl declaration(Heap *heap) { return decl_of(heap, "zero", term_lit(heap, literal_lint(big_zero()))); }

static Decl *decl_new(Heap *heap, Decl decl) {
  Decl *d = heap_alloc(heap, sizeof *d);
  if (d != NULL) *d = decl;
  return d;
}

/* [d, Check.Decl{"copy", Definition, Prim.nat_ty(), Some{Term.Global{"zero"}}}] */
static Vec two_decls(Heap *heap, Decl d) {
  Vec decls = vec_new(heap->arena);
  Decl *first = decl_new(heap, d);
  Decl *second = decl_new(heap, decl_of(heap, "copy", term_global(heap, str_lit("zero"))));
  if (first == NULL || !vec_push(&decls, first)) heap->oom = true;
  if (second == NULL || !vec_push(&decls, second)) heap->oom = true;
  return decls;
}

static FamilyDecl family_decl(Heap *heap, const char *name, Vec indices) {
  FamilyDecl f;
  f.name = str_lit(name);
  f.params = vec_new(heap->arena);
  f.indices = indices;
  f.level = level_one(heap);
  return f;
}

/* [Pos.Binder{Q.QOne{}, "i", Prim.nat_ty()}] */
static Vec bad_indices(Heap *heap) {
  Vec indices = vec_new(heap->arena);
  TeleBinder *b = positivity_binder_new(heap, QUANTITY_ONE, str_lit("i"), prim_nat_ty(heap));
  if (b == NULL || !vec_push(&indices, b)) heap->oom = true;
  return indices;
}

static int run(Heap *heap) {
  Globals g = global_initial(heap);
  Decl d = declaration(heap);
  Budget unlimited = budget_unlimited();
  bool decl_ok = success(check_check_decl(heap, g, unlimited, d));
  bool decls_ok = success(check_check_decls(heap, g, unlimited, two_decls(heap, d)));
  bool family_ok = family_test(heap, check_declare_family(heap, g, unlimited, family_decl(heap, "Flag", vec_new(heap->arena))));
  bool index_ok = index_error(check_declare_family(heap, g, unlimited, family_decl(heap, "Bad", bad_indices(heap))));
  return verify(decl_ok && decls_ok && family_ok && index_ok);
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
