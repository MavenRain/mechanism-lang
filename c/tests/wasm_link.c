/* Port of bend2/tests/wasm_link.bend (oracle label unit-wasm-link): ten checks of the link
 * inference over a three-row program table; each prints `PASS name`, and the first mismatch dies
 * with exit 1 and the expected and observed texts. */
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

/* E.KErased{} as a list item */
static void *erased(void) { return (void *)eterm_kerased(); }

/* report(ok, name, expected, actual): PASS line, or die with exit 1 */
static int report(Heap *heap, bool ok, Str name, Str expected, Str actual) {
  if (ok) return driver_print(heap_str_cat(heap, str_lit("PASS "), name));
  Str observed = heap_str_cat(heap, str_lit(", observed "), actual);
  Str message = heap_str_cat(heap, name, heap_str_cat(heap, str_lit(": expected "), heap_str_cat(heap, expected, observed)));
  return driver_die_text(1, message);
}

/* check(name, expected, actual) */
static int check(Heap *heap, const char *name, const char *expected, Str actual) {
  Str want = str_lit(expected);
  return report(heap, str_eq(want, actual), str_lit(name), want, actual);
}

/* repr(r): the printed repr, or the error text */
static Str repr(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_DONE:
      return r.value == NULL ? str_empty() : eterm_print_repr(heap, *(const Repr *)r.value);
    case RESULT_FAIL:
      return error_to_string(heap, r.error);
  }
  return str_empty();
}

/* reprs(rs): the printed reprs joined with ";" */
static Str reprs(Heap *heap, const Vec *rs) {
  Str out = str_empty();
  for (size_t i = 0; i < rs->len; i++) {
    VecGet at = vec_get(rs, i);
    if (at.status == VEC_GET_OUT_OF_RANGE || at.value == NULL) continue;
    Str text = eterm_print_repr(heap, *(const Repr *)at.value);
    out = i == 0 ? text : heap_str_cat(heap, out, heap_str_cat(heap, str_lit(";"), text));
  }
  return out;
}

/* bindings(r): the joined reprs, or the error text */
static Str bindings(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_DONE:
      return r.value == NULL ? str_empty() : reprs(heap, (const Vec *)r.value);
    case RESULT_FAIL:
      return error_to_string(heap, r.error);
  }
  return str_empty();
}

/* program(): the table of f (fn of two nats), tree (a two-leg mu) and axiom (a postulate) */
static Prog program(Heap *heap) {
  Kdecl f = eterm_kfun(eterm_fid(str_lit("f")),
                       list(heap, 2, eterm_repr_new(heap, link_nat_repr()), eterm_repr_new(heap, link_nat_repr())),
                       eterm_rfunc(eterm_tid(str_lit("fn<1>"))), eterm_kerased());
  Kdecl tree = eterm_krec(list(heap, 2, eterm_tid_new(heap, str_lit("leg<mu<Tree>,0>")),
                               eterm_tid_new(heap, str_lit("leg<mu<Tree>,1,union nat,func fn<2>>"))));
  Vec rows = list(heap, 3, erase_row_new(heap, str_lit("f"), erase_entry_code(list(heap, 1, eterm_kdecl_new(heap, f)))),
                  erase_row_new(heap, str_lit("tree"), erase_entry_code(list(heap, 1, eterm_kdecl_new(heap, tree)))),
                  erase_row_new(heap, str_lit("axiom"), erase_entry_postulate(link_nat_repr())));
  return link_program_table(heap, &rows);
}

/* E.KApp{E.KGlobal{"f"}, [E.KErased{} x count]} for count 1, 2 or 3 */
static const Ktm *apply_f(Heap *heap, size_t count) {
  Vec args = count == 1 ? list(heap, 1, erased())
             : count == 2 ? list(heap, 2, erased(), erased())
                          : list(heap, 3, erased(), erased(), erased());
  return eterm_kapp(heap, eterm_kglobal(heap, str_lit("f")), args);
}

/* L.infer(program(), [], tm) */
static Result infer(Heap *heap, const Ktm *tm) {
  Prog p = program(heap);
  Vec env = vec_new(heap->arena);
  return link_infer(heap, &p, &env, tm);
}

/* main(): the ten checks in the Bend order, stopping at the first die */
static int checks(Heap *heap) {
  int code = check(heap, "partial", "func fn<1>", repr(heap, infer(heap, apply_f(heap, 1))));
  if (code != 0) return code;
  code = check(heap, "exact", "func fn<1>", repr(heap, infer(heap, apply_f(heap, 2))));
  if (code != 0) return code;
  code = check(heap, "over", "union any", repr(heap, infer(heap, apply_f(heap, 3))));
  if (code != 0) return code;
  code = check(heap, "zero specialized", "union any",
               repr(heap, link_steps_result(heap, eterm_rfunc(eterm_tid(str_lit("fn<0>"))), nat(heap, 0))));
  if (code != 0) return code;
  Prog mu = program(heap);
  code = check(heap, "mu fields", "func fn<2>;union nat",
               bindings(heap, link_branch_binders(heap, &mu, eterm_runion(eterm_tid(str_lit("mu<Tree>"))),
                                                  eterm_branch(nat(heap, 1), nat(heap, 2), eterm_kerased()))));
  if (code != 0) return code;
  code = check(heap, "nested fields", "struct pair<union nat,func fn<2>>;union sum<unit|union nat>",
               bindings(heap, link_fields_of(heap, str_lit("tuple<struct pair<union nat,func fn<2>>,union sum<unit|union nat>>"))));
  if (code != 0) return code;
  Prog captured = program(heap);
  code = check(heap, "capture", "union nat", bindings(heap, link_capture_reprs(heap, &captured, str_lit("f"), nat(heap, 1))));
  if (code != 0) return code;
  code = check(heap, "unbound", "unbound: KVar 3 is out of scope", repr(heap, infer(heap, eterm_kvar(heap, nat(heap, 3)))));
  if (code != 0) return code;
  code = check(heap, "postulate", "unbound: axiom axiom has no body",
               repr(heap, infer(heap, eterm_kglobal(heap, str_lit("axiom")))));
  if (code != 0) return code;
  return check(heap, "force", "not yet: KForce arrives at M2", repr(heap, infer(heap, eterm_kforce(heap, eterm_kerased()))));
}

static int entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = checks(&heap);
  int out = heap.oom ? driver_host_failure() : code;
  arena_free(&arena);
  return out;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, entry); }
