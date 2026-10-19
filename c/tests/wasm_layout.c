/* Port of tests/wasm_layout.bend (oracle label fixture-layout): links three erased programs
 * (basic, recursive, closure), encodes the type groups of each layout as a module and prints its
 * bytes as a decimal list in brackets, one line per program; a link failure dies with exit 1. */
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

/* E.KFun{E.Fid{name}, params, result, body} boxed as a list item */
static Kdecl *kfun(Heap *heap, const char *name, Vec params, Repr result, const Ktm *body) {
  return eterm_kdecl_new(heap, eterm_kfun(eterm_fid(str_lit(name)), params, result, body));
}

/* C.Pair{"main", R.Code{decls}} as the one row of a program */
static Vec main_rows(Heap *heap, Vec decls) {
  return list(heap, 1, erase_row_new(heap, str_lit("main"), erase_entry_code(decls)));
}

/* literal() = E.KLit{Lit.LInt{B.of_u32(42)}} */
static const Ktm *literal(Heap *heap) { return eterm_klit(heap, literal_lint(big_of_u32(heap, 42))); }

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
  Kdecl *lift = kfun(heap, "lift", list(heap, 2, eterm_repr_new(heap, link_nat_repr()), eterm_repr_new(heap, link_nat_repr())),
                     link_nat_repr(), eterm_kvar(heap, nat(heap, 1)));
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

/* Result<Err.T, List<U32>> of layout: the encoded bytes or the link error */
typedef struct {
  ResultKind kind;
  Str bytes;   /* RESULT_DONE */
  Error error; /* RESULT_FAIL */
} Layout;

/* layout(rows): build, group the comptypes, encode G.Module{ts, [], [], [], []} */
static Layout layout(Heap *heap, const Vec *rows) {
  Layout out;
  out.kind = RESULT_FAIL;
  out.bytes = str_empty();
  Result l = link_build(heap, rows);
  if (l.kind == RESULT_FAIL) {
    out.error = l.error;
    return out;
  }
  Result ts = link_comptype_groups(heap, l.value);
  if (ts.kind == RESULT_FAIL) {
    out.error = ts.error;
    return out;
  }
  U32List declared;
  declared.items = NULL;
  declared.len = 0;
  GcModule *m = gc_module(heap, *(const Vec *)ts.value, vec_new(heap->arena), vec_new(heap->arena), vec_new(heap->arena), declared);
  out.kind = RESULT_DONE;
  out.bytes = m == NULL ? str_empty() : gc_encode(heap, m);
  return out;
}

/* output(r): die with the error text, or print the bytes in brackets */
static int output(Heap *heap, Layout r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return driver_die_text(1, error_to_string(heap, r.error));
    case RESULT_DONE:
      return driver_print(heap_str_cat(heap, str_lit("["), heap_str_cat(heap, bytes_text(heap, r.bytes), str_lit("]"))));
  }
  return 0;
}

/* main(): basic, recursive, closure in order, stopping at the first die */
static int run(Heap *heap) {
  Vec b = basic(heap);
  int code = output(heap, layout(heap, &b));
  if (code != 0) return code;
  Vec r = recursive(heap);
  code = output(heap, layout(heap, &r));
  if (code != 0) return code;
  Vec c = closure(heap);
  return output(heap, layout(heap, &c));
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
