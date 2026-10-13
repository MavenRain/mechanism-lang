/* Port of bend2/tests/wasm_emit.bend (oracle label fixture-emit): emits five erased programs
 * (basic, closure, partial, tag_case, big_arithmetic) as complete wasm modules with the export
 * "main" and prints the bytes of each as a decimal list in brackets, one line per program; an emit
 * failure dies with exit 1. recursive() is defined but not run, as in the Bend driver. */
#include <stdarg.h>

#include "driver.h"
#include "wasm.h"

/* A Bend list literal of boxed items: list(heap, n, item1, ..., itemN); a NULL item is a failed
 * allocation (heap->oom set) and is not stored. */
static Vec list(Heap *heap, size_t count, ...) {
  Vec out = vec_new(heap->arena);
  va_list items;
  va_start(items, count);
  for (size_t i = 0; i < count; i++) {
    void *item = va_arg(items, void *);
    if (item == NULL || !vec_push(&out, item)) heap->oom = true;
  }
  va_end(items);
  return out;
}

/* A Bend Nat literal */
static Nat nat(Heap *heap, uint64_t n) { return nat_of_u64(heap->arena, n); }

/* L.nat_repr() boxed as a list item */
static Repr *nat_param(Heap *heap) { return eterm_repr_new(heap, link_nat_repr()); }

/* E.KFun{E.Fid{name}, params, result, body} boxed as a list item */
static Kdecl *kfun(Heap *heap, const char *name, Vec params, Repr result, const Ktm *body) {
  return eterm_kdecl_new(heap, eterm_kfun(eterm_fid(str_lit(name)), params, result, body));
}

/* C.Pair{"main", R.Code{decls}} as the one row of a program */
static Vec main_rows(Heap *heap, Vec decls) {
  return list(heap, 1, erase_row_new(heap, str_lit("main"), erase_entry_code(decls)));
}

/* E.KLit{Lit.LInt{B.of_u32(n)}} */
static const Ktm *int_lit(Heap *heap, uint32_t n) { return eterm_klit(heap, literal_lint(big_of_u32(heap, n))); }

/* literal() = E.KLit{Lit.LInt{B.of_u32(42)}} */
static const Ktm *literal(Heap *heap) { return int_lit(heap, 42); }

/* basic(): main returns the literal */
static Vec basic(Heap *heap) {
  return main_rows(heap, list(heap, 1, kfun(heap, "main", list(heap, 0), link_nat_repr(), literal(heap))));
}

/* recursive(): a three-leg mu and a main that builds its leg 0 */
static Vec recursive(Heap *heap) {
  Kdecl *rec = eterm_kdecl_new(heap, eterm_krec(list(heap, 3, eterm_tid_new(heap, str_lit("leg<mu<Tree>,0>")),
                                                     eterm_tid_new(heap, str_lit("leg<mu<Tree>,1,union mu<Tree>>")),
                                                     eterm_tid_new(heap, str_lit("leg<mu<Tree>,2,union mu<Tree>>")))));
  Kdecl *main_fn = kfun(heap, "main", list(heap, 0), eterm_runion(eterm_tid(str_lit("mu<Tree>"))),
                        eterm_ktag(heap, eterm_tid(str_lit("mu<Tree>")), nat(heap, 0), list(heap, 0)));
  return main_rows(heap, list(heap, 2, rec, main_fn));
}

/* closure(): main applies a closure of lift over one captured literal */
static Vec closure(Heap *heap) {
  Kdecl *lift = kfun(heap, "lift", list(heap, 2, nat_param(heap), nat_param(heap)), link_nat_repr(), eterm_kvar(heap, nat(heap, 1)));
  const Ktm *clos = eterm_kclos(heap, eterm_fid(str_lit("lift")), nat(heap, 1), list(heap, 1, (void *)literal(heap)));
  Kdecl *main_fn = kfun(heap, "main", list(heap, 0), link_nat_repr(), eterm_kapp(heap, clos, list(heap, 1, (void *)literal(heap))));
  return main_rows(heap, list(heap, 2, lift, main_fn));
}

/* bytes_text(xs): the bytes as decimal numbers separated by commas; "" for no bytes */
static Str bytes_text(Heap *heap, Str xs) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < xs.len; i++) {
    StrAt at = str_at(xs, i);
    if (at.status == STR_INDEX_OUT_OF_RANGE) continue;
    if (i > 0 && !strbuf_push_byte(&buf, ',')) heap->oom = true;
    if (!strbuf_push_u64(&buf, at.value)) heap->oom = true;
  }
  return strbuf_str(&buf);
}

/* layout(rows) = Emit.program(rows, "main"); the Done value is the Str * of the module bytes */
static Result layout(Heap *heap, const Vec *rows) { return emit_program(heap, rows, str_lit("main")); }

/* output(r): die with the error text, or print the bytes in brackets */
static int output(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return driver_die_text(1, error_to_string(heap, r.error));
    case RESULT_DONE:
      return driver_print(heap_str_cat(heap, str_lit("["), heap_str_cat(heap, bytes_text(heap, *(const Str *)r.value), str_lit("]"))));
  }
  return 0;
}

/* partial(): add tail-calls natAdd; main partially applies add to 40, then applies the result to 2 */
static Vec partial(Heap *heap) {
  const Ktm *add_body = eterm_ktail(heap, eterm_kglobal(heap, str_lit("natAdd")),
                                    list(heap, 2, (void *)eterm_kvar(heap, nat(heap, 1)), (void *)eterm_kvar(heap, nat(heap, 0))));
  Kdecl *add = kfun(heap, "add", list(heap, 2, nat_param(heap), nat_param(heap)), link_nat_repr(), add_body);
  const Ktm *value = eterm_kapp(heap, eterm_kglobal(heap, str_lit("add")), list(heap, 1, (void *)int_lit(heap, 40)));
  const Ktm *body = eterm_kapp(heap, eterm_kvar(heap, nat(heap, 0)), list(heap, 1, (void *)int_lit(heap, 2)));
  Kdecl *main_fn = kfun(heap, "main", list(heap, 0), link_nat_repr(), eterm_klet(heap, str_lit("f"), value, body));
  return main_rows(heap, list(heap, 2, add, main_fn));
}

/* tag_case(): a two-leg mu; main cases on leg 1 of the literal, branch 0 gives zero, branch 1 its field */
static Vec tag_case(Heap *heap) {
  Kdecl *rec = eterm_kdecl_new(heap, eterm_krec(list(heap, 2, eterm_tid_new(heap, str_lit("leg<mu<Tree>,0>")),
                                                     eterm_tid_new(heap, str_lit("leg<mu<Tree>,1,union nat>")))));
  const Ktm *scrut = eterm_ktag(heap, eterm_tid(str_lit("mu<Tree>")), nat(heap, 1), list(heap, 1, (void *)literal(heap)));
  Vec branches = list(heap, 2, eterm_branch_new(heap, nat(heap, 0), nat(heap, 0), eterm_klit(heap, literal_lint(big_zero()))),
                      eterm_branch_new(heap, nat(heap, 1), nat(heap, 1), eterm_kvar(heap, nat(heap, 0))));
  Kdecl *main_fn = kfun(heap, "main", list(heap, 0), link_nat_repr(), eterm_kcase(heap, eterm_tid(str_lit("mu<Tree>")), scrut, branches));
  return main_rows(heap, list(heap, 2, rec, main_fn));
}

/* big_arithmetic(): main applies natSub to two literals above the i31 range */
static Vec big_arithmetic(Heap *heap) {
  const Ktm *body = eterm_kapp(heap, eterm_kglobal(heap, str_lit("natSub")),
                               list(heap, 2, (void *)int_lit(heap, 1073741865u), (void *)int_lit(heap, 1073741823u)));
  return main_rows(heap, list(heap, 1, kfun(heap, "main", list(heap, 0), link_nat_repr(), body)));
}

/* main(): basic, closure, partial, tag_case, big_arithmetic in order, stopping at the first die */
static int run(Heap *heap) {
  (void)recursive;
  Vec b = basic(heap);
  int code = output(heap, layout(heap, &b));
  if (code != 0) return code;
  Vec c = closure(heap);
  code = output(heap, layout(heap, &c));
  if (code != 0) return code;
  Vec p = partial(heap);
  code = output(heap, layout(heap, &p));
  if (code != 0) return code;
  Vec t = tag_case(heap);
  code = output(heap, layout(heap, &t));
  if (code != 0) return code;
  Vec a = big_arithmetic(heap);
  return output(heap, layout(heap, &a));
}

static int entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  int out = heap.oom ? driver_host_failure() : code;
  arena_free(&arena);
  return out;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
