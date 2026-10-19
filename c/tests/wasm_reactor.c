/* Port of tests/wasm_reactor.bend (oracle label fixture-reactor): emits one erased program
 * as a reactor module with the six exports add, pair, sum, bool, unbool and big, and prints its
 * bytes as a decimal list in brackets; an emit failure dies with exit 1. */
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

/* A boxed String list item; NULL on a failed allocation (heap->oom set) */
static Str *text(Heap *heap, const char *s) {
  Str *box = arena_alloc(heap->arena, sizeof(Str));
  if (box == NULL) {
    heap->oom = true;
    return NULL;
  }
  *box = str_lit(s);
  return box;
}

/* E.KFun{E.Fid{name}, params, result, body} boxed as a list item */
static Kdecl *kfun(Heap *heap, const char *name, Vec params, Repr result, const Ktm *body) {
  return eterm_kdecl_new(heap, eterm_kfun(eterm_fid(str_lit(name)), params, result, body));
}

/* pair_tid() = E.Tid{"pair<union nat,union nat>"} */
static Tid pair_tid(void) { return eterm_tid(str_lit("pair<union nat,union nat>")); }

/* literal(n) = E.KLit{Lit.LInt{B.of_u32(n)}} */
static const Ktm *literal(Heap *heap, uint32_t n) { return eterm_klit(heap, literal_lint(big_of_u32(heap, n))); }

/* rows(): one "exports" row with add, pair, sum, bool, unbool and big */
static Vec rows(Heap *heap) {
  Tid bool_tid = eterm_tid(str_lit("sum<unit|unit>"));
  const Ktm *add_body = eterm_ktail(heap, eterm_kglobal(heap, str_lit("natAdd")),
                                    list(heap, 2, (void *)eterm_kvar(heap, nat(heap, 1)), (void *)eterm_kvar(heap, nat(heap, 0))));
  Kdecl *add = kfun(heap, "add", list(heap, 2, eterm_repr_new(heap, link_nat_repr()), eterm_repr_new(heap, link_nat_repr())),
                    link_nat_repr(), add_body);
  const Ktm *pair_body = eterm_kstruct(heap, pair_tid(),
                                       list(heap, 2, (void *)eterm_kvar(heap, nat(heap, 1)), (void *)eterm_kvar(heap, nat(heap, 0))));
  Kdecl *pair = kfun(heap, "pair", list(heap, 2, eterm_repr_new(heap, link_nat_repr()), eterm_repr_new(heap, link_nat_repr())),
                     eterm_rstruct(pair_tid()), pair_body);
  const Ktm *sum_body = eterm_kapp(heap, eterm_kglobal(heap, str_lit("natAdd")),
                                   list(heap, 2, (void *)eterm_kproj(heap, pair_tid(), nat(heap, 0), eterm_kvar(heap, nat(heap, 0))),
                                        (void *)eterm_kproj(heap, pair_tid(), nat(heap, 1), eterm_kvar(heap, nat(heap, 0)))));
  Kdecl *sum = kfun(heap, "sum", list(heap, 1, eterm_repr_new(heap, eterm_rstruct(pair_tid()))), link_nat_repr(), sum_body);
  Kdecl *bool_fn = kfun(heap, "bool", list(heap, 0), eterm_runion(bool_tid), eterm_ktag(heap, bool_tid, nat(heap, 1), list(heap, 0)));
  Vec branches = list(heap, 2, eterm_branch_new(heap, nat(heap, 0), nat(heap, 0), literal(heap, 0)),
                      eterm_branch_new(heap, nat(heap, 1), nat(heap, 0), literal(heap, 1)));
  Kdecl *unbool = kfun(heap, "unbool", list(heap, 1, eterm_repr_new(heap, eterm_runion(bool_tid))), link_nat_repr(),
                       eterm_kcase(heap, bool_tid, eterm_kvar(heap, nat(heap, 0)), branches));
  Kdecl *big = kfun(heap, "big", list(heap, 0), link_nat_repr(), literal(heap, 1073741824u));
  Vec decls = list(heap, 6, add, pair, sum, bool_fn, unbool, big);
  return list(heap, 1, erase_row_new(heap, str_lit("exports"), erase_entry_code(decls)));
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

/* main(): output(Emit.reactor(rows(), ["add", "pair", "sum", "bool", "unbool", "big"])) */
static int run(Heap *heap) {
  Vec r = rows(heap);
  Vec exports = list(heap, 6, text(heap, "add"), text(heap, "pair"), text(heap, "sum"), text(heap, "bool"), text(heap, "unbool"),
                     text(heap, "big"));
  return output(heap, emit_reactor(heap, &r, &exports));
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
