/* Port of bend2/tests/wasm_oracle.bend (oracle label fixture-oracle): 33 static erased snapshots
 * from the original Wasm fixtures, each emitted by Emit.program with the export "main". One JSON
 * line per snapshot: the name, the original runtime answer as text, and the module bytes as a
 * decimal list; an emit failure dies with exit 1 and "name: error". The rows_N bodies are a
 * mechanical transcription of the Bend snapshots; the helpers above them are the Bend
 * constructors (E.*, Lit.LInt, B.Big, C.Pair, R.Code) as C calls. */
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
    const void *item = va_arg(items, const void *);
    if (item == NULL || !vec_push(&out, (void *)item)) heap->oom = true;
  }
  va_end(items);
  return out;
}

/* B.Big{sign, [limbs]}: big(heap, negative, n, limb1, ..., limbN), base 2^15 limbs, least
 * significant first, copied into the arena and made canonical by big_make. */
static Big big(Heap *heap, bool negative, size_t count, ...) {
  uint32_t *digits = count == 0 ? NULL : heap_alloc(heap, count * sizeof(uint32_t));
  if (count != 0 && digits == NULL) heap->oom = true;
  size_t len = digits == NULL ? 0 : count;
  va_list limbs;
  va_start(limbs, count);
  for (size_t i = 0; i < len; i++) digits[i] = va_arg(limbs, unsigned int);
  va_end(limbs);
  Limbs value = {digits, len};
  return big_make(heap, negative, value);
}

/* E.RUnion{E.Tid{t}}, E.RStruct{E.Tid{t}}, E.RFunc{E.Tid{t}} */
static Repr runion(const char *t) { return eterm_runion(eterm_tid(str_lit(t))); }
static Repr rstruct(const char *t) { return eterm_rstruct(eterm_tid(str_lit(t))); }
static Repr rfunc(const char *t) { return eterm_rfunc(eterm_tid(str_lit(t))); }

/* A Repr boxed as a KFun parameter list item */
static Repr *param(Heap *heap, Repr r) { return eterm_repr_new(heap, r); }

/* E.Tid{t} boxed as a KRec list item */
static Tid *tid_box(Heap *heap, const char *t) { return eterm_tid_new(heap, str_lit(t)); }

/* E.KRec{[tids]} and E.KFun{E.Fid{f}, params, result, body} boxed as R.Code list items */
static Kdecl *krec(Heap *heap, Vec tids) { return eterm_kdecl_new(heap, eterm_krec(tids)); }
static Kdecl *kfun(Heap *heap, const char *f, Vec params, Repr result, const Ktm *body) {
  return eterm_kdecl_new(heap, eterm_kfun(eterm_fid(str_lit(f)), params, result, body));
}

/* The Ktm constructors with Nat fields from small literals and Tid, Fid and String fields from
 * C strings */
static const Ktm *kvar(Heap *heap, uint64_t n) { return eterm_kvar(heap, nat_of_u64(heap->arena, n)); }
static const Ktm *kint(Heap *heap, Big b) { return eterm_klit(heap, literal_lint(b)); }
static const Ktm *kglobal(Heap *heap, const char *name) { return eterm_kglobal(heap, str_lit(name)); }
static const Ktm *klet(Heap *heap, const char *name, const Ktm *value, const Ktm *body) {
  return eterm_klet(heap, str_lit(name), value, body);
}
static const Ktm *kclos(Heap *heap, const char *f, uint64_t arity, Vec captures) {
  Nat n = nat_of_u64(heap->arena, arity);
  return eterm_kclos(heap, eterm_fid(str_lit(f)), n, captures);
}
static const Ktm *kapp(Heap *heap, const Ktm *head, Vec args) { return eterm_kapp(heap, head, args); }
static const Ktm *ktail(Heap *heap, const Ktm *head, Vec args) { return eterm_ktail(heap, head, args); }
static const Ktm *kstruct(Heap *heap, const char *t, Vec fields) { return eterm_kstruct(heap, eterm_tid(str_lit(t)), fields); }
static const Ktm *kproj(Heap *heap, const char *t, uint64_t index, const Ktm *value) {
  Nat n = nat_of_u64(heap->arena, index);
  return eterm_kproj(heap, eterm_tid(str_lit(t)), n, value);
}
static const Ktm *ktag(Heap *heap, const char *t, uint64_t tag, Vec fields) {
  Nat n = nat_of_u64(heap->arena, tag);
  return eterm_ktag(heap, eterm_tid(str_lit(t)), n, fields);
}
static const Ktm *kcase(Heap *heap, const char *t, const Ktm *scrut, Vec branches) {
  return eterm_kcase(heap, eterm_tid(str_lit(t)), scrut, branches);
}

/* E.Branch{tag, arity, body} boxed as a KCase list item */
static KtmBranch *branch(Heap *heap, uint64_t tag, uint64_t arity, const Ktm *body) {
  Nat t = nat_of_u64(heap->arena, tag);
  Nat a = nat_of_u64(heap->arena, arity);
  return eterm_branch_new(heap, t, a, body);
}

/* C.Pair{name, R.Code{decls}} boxed as a row */
static EraseRow *code_row(Heap *heap, const char *name, Vec decls) {
  return erase_row_new(heap, str_lit(name), erase_entry_code(decls));
}

/* rows_0(): circuit-named-tree */
static Vec rows_0(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "circuit-named-tree", list(heap, 12,
      krec(heap, list(heap, 3, tid_box(heap, "mu<CircuitTree>"), tid_box(heap, "leg<mu<CircuitTree>,0,union nat>"), tid_box(heap, "leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"))),
      kfun(heap, "tree", list(heap, 0), runion("mu<CircuitTree>"), ktag(heap, "mu<CircuitTree>", 1, list(heap, 2, ktag(heap, "mu<CircuitTree>", 0, list(heap, 1, kint(heap, big(heap, false, 1, 3u)))), ktag(heap, "mu<CircuitTree>", 0, list(heap, 1, kint(heap, big(heap, false, 1, 4u))))))),
      krec(heap, list(heap, 1, tid_box(heap, "mu<CircuitTree>"))),
      kfun(heap, "treeAlias", list(heap, 0), runion("mu<CircuitTree>"), kglobal(heap, "tree")),
      krec(heap, list(heap, 3, tid_box(heap, "mu<CircuitTree>"), tid_box(heap, "leg<mu<CircuitTree>,0,union nat>"), tid_box(heap, "leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"))),
      kfun(heap, "leaf", list(heap, 0), runion("mu<CircuitTree>"), ktag(heap, "mu<CircuitTree>", 0, list(heap, 1, kint(heap, big(heap, false, 1, 7u))))),
      krec(heap, list(heap, 4, tid_box(heap, "nat"), tid_box(heap, "mu<CircuitTree>"), tid_box(heap, "leg<mu<CircuitTree>,0,union nat>"), tid_box(heap, "leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"))),
      kfun(heap, "bump", list(heap, 1, param(heap, runion("nat"))), runion("nat"), kcase(heap, "mu<CircuitTree>", kglobal(heap, "leaf"), list(heap, 2, branch(heap, 0, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kvar(heap, 1)))), branch(heap, 1, 2, kint(heap, big(heap, false, 0)))))),
      krec(heap, list(heap, 4, tid_box(heap, "nat"), tid_box(heap, "mu<CircuitTree>"), tid_box(heap, "leg<mu<CircuitTree>,0,union nat>"), tid_box(heap, "leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"))),
      kfun(heap, "branch", list(heap, 0), runion("nat"), kcase(heap, "mu<CircuitTree>", kglobal(heap, "treeAlias"), list(heap, 2, branch(heap, 0, 1, kvar(heap, 0)), branch(heap, 1, 2, kint(heap, big(heap, false, 1, 34u)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "bump"), list(heap, 1, kint(heap, big(heap, false, 1, 1u)))), kglobal(heap, "branch")))))));
}

/* rows_1(): d01-lit-prims */
static Vec rows_1(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d01-lit-prims", list(heap, 8,
      krec(heap, list(heap, 2, tid_box(heap, "sum<unit|unit>"), tid_box(heap, "nat"))),
      kfun(heap, "toNat", list(heap, 1, param(heap, runion("sum<unit|unit>"))), runion("nat"), kcase(heap, "sum<unit|unit>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 0, kint(heap, big(heap, false, 1, 1u)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "scaled", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natMul"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 2u)), kint(heap, big(heap, false, 1, 3u)))), kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 1, 7u)), kint(heap, big(heap, false, 1, 4u))))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "flags", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "toNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 1, 4u)), kint(heap, big(heap, false, 1, 4u)))))), kapp(heap, kglobal(heap, "toNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kint(heap, big(heap, false, 1, 2u))))))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "scaled"), kglobal(heap, "flags")))))));
}

/* rows_2(): d02-tail-call */
static Vec rows_2(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d02-tail-call", list(heap, 8,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "step", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "chain", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "step"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kint(heap, big(heap, false, 1, 1u)))), kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "outer", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "chain"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kint(heap, big(heap, false, 1, 2u)))), kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "outer"), list(heap, 2, kint(heap, big(heap, false, 1, 3u)), kint(heap, big(heap, false, 1, 4u))))))));
}

/* rows_3(): d03-pair */
static Vec rows_3(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d03-pair", list(heap, 8,
      krec(heap, list(heap, 1, tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "p", list(heap, 0), rstruct("pair<union nat,union nat>"), kstruct(heap, "pair<union nat,union nat>", list(heap, 2, kint(heap, big(heap, false, 1, 11u)), kint(heap, big(heap, false, 1, 6u))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "mk", list(heap, 1, param(heap, runion("nat"))), rstruct("pair<union nat,union nat>"), kstruct(heap, "pair<union nat,union nat>", list(heap, 2, kvar(heap, 0), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u))))))),
      krec(heap, list(heap, 1, tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "q", list(heap, 0), rstruct("pair<union nat,union nat>"), ktail(heap, kglobal(heap, "mk"), list(heap, 1, kint(heap, big(heap, false, 1, 4u))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, klet(heap, "scrut", kglobal(heap, "p"), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), klet(heap, "scrut", kglobal(heap, "p"), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 0)))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, klet(heap, "scrut", kglobal(heap, "q"), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), klet(heap, "scrut", kglobal(heap, "q"), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 0))))))))))));
}

/* rows_4(): d04-tuple */
static Vec rows_4(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d04-tuple", list(heap, 4,
      krec(heap, list(heap, 1, tid_box(heap, "tuple<union nat,union nat,union nat>"))),
      kfun(heap, "t", list(heap, 0), rstruct("tuple<union nat,union nat,union nat>"), kstruct(heap, "tuple<union nat,union nat,union nat>", list(heap, 3, kint(heap, big(heap, false, 1, 2u)), kint(heap, big(heap, false, 1, 3u)), kint(heap, big(heap, false, 1, 4u))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,union nat,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kproj(heap, "tuple<union nat,union nat,union nat>", 0, kglobal(heap, "t")), kproj(heap, "tuple<union nat,union nat,union nat>", 1, kglobal(heap, "t")))), kproj(heap, "tuple<union nat,union nat,union nat>", 2, kglobal(heap, "t"))))))));
}

/* rows_5(): d05-sum-legs */
static Vec rows_5(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d05-sum-legs", list(heap, 12,
      krec(heap, list(heap, 1, tid_box(heap, "sum<unit|unit|union nat|union nat>"))),
      kfun(heap, "z0", list(heap, 0), runion("sum<unit|unit|union nat|union nat>"), ktag(heap, "sum<unit|unit|union nat|union nat>", 0, list(heap, 0))),
      krec(heap, list(heap, 1, tid_box(heap, "sum<unit|unit|union nat|union nat>"))),
      kfun(heap, "z1", list(heap, 0), runion("sum<unit|unit|union nat|union nat>"), ktag(heap, "sum<unit|unit|union nat|union nat>", 1, list(heap, 0))),
      krec(heap, list(heap, 1, tid_box(heap, "sum<unit|unit|union nat|union nat>"))),
      kfun(heap, "p2", list(heap, 0), runion("sum<unit|unit|union nat|union nat>"), ktag(heap, "sum<unit|unit|union nat|union nat>", 2, list(heap, 1, kint(heap, big(heap, false, 1, 5u))))),
      krec(heap, list(heap, 1, tid_box(heap, "sum<unit|unit|union nat|union nat>"))),
      kfun(heap, "p3", list(heap, 0), runion("sum<unit|unit|union nat|union nat>"), ktag(heap, "sum<unit|unit|union nat|union nat>", 3, list(heap, 1, kint(heap, big(heap, false, 1, 6u))))),
      krec(heap, list(heap, 2, tid_box(heap, "sum<unit|unit|union nat|union nat>"), tid_box(heap, "nat"))),
      kfun(heap, "read", list(heap, 1, param(heap, runion("sum<unit|unit|union nat|union nat>"))), runion("nat"), kcase(heap, "sum<unit|unit|union nat|union nat>", kvar(heap, 0), list(heap, 4, branch(heap, 0, 0, kint(heap, big(heap, false, 1, 1u))), branch(heap, 1, 0, kint(heap, big(heap, false, 1, 2u))), branch(heap, 2, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 10u))))), branch(heap, 3, 1, ktail(heap, kglobal(heap, "natMul"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 10u)))))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "read"), list(heap, 1, kglobal(heap, "z0"))), kapp(heap, kglobal(heap, "read"), list(heap, 1, kglobal(heap, "z1"))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "read"), list(heap, 1, kglobal(heap, "p2"))), kapp(heap, kglobal(heap, "read"), list(heap, 1, kglobal(heap, "p3")))))))))));
}

/* rows_6(): d06-closure-capture */
static Vec rows_6(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d06-closure-capture", list(heap, 8,
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<func fn<1>,union nat>"))),
      kfun(heap, "mk$0", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      kfun(heap, "mk", list(heap, 1, param(heap, runion("nat"))), rstruct("pair<func fn<1>,union nat>"), kstruct(heap, "pair<func fn<1>,union nat>", list(heap, 2, kclos(heap, "mk$0", 1, list(heap, 1, kvar(heap, 0))), kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 2u))))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<func fn<1>,union nat>"))),
      kfun(heap, "mkLet$0", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      kfun(heap, "mkLet", list(heap, 1, param(heap, runion("nat"))), rstruct("pair<func fn<1>,union nat>"), klet(heap, "z", kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u)))), kstruct(heap, "pair<func fn<1>,union nat>", list(heap, 2, kclos(heap, "mkLet$0", 1, list(heap, 1, kvar(heap, 0))), kvar(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<func fn<1>,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, klet(heap, "scrut", kapp(heap, kglobal(heap, "mk"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))), klet(heap, "f", kproj(heap, "pair<func fn<1>,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<func fn<1>,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), list(heap, 1, kint(heap, big(heap, false, 1, 6u)))), klet(heap, "scrut", kapp(heap, kglobal(heap, "mk"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))), klet(heap, "f", kproj(heap, "pair<func fn<1>,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<func fn<1>,union nat>", 1, kvar(heap, 1)), kvar(heap, 0)))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, klet(heap, "scrut", kapp(heap, kglobal(heap, "mkLet"), list(heap, 1, kint(heap, big(heap, false, 1, 2u)))), klet(heap, "f", kproj(heap, "pair<func fn<1>,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<func fn<1>,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), list(heap, 1, kint(heap, big(heap, false, 1, 4u)))), klet(heap, "scrut", kapp(heap, kglobal(heap, "mkLet"), list(heap, 1, kint(heap, big(heap, false, 1, 2u)))), klet(heap, "f", kproj(heap, "pair<func fn<1>,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<func fn<1>,union nat>", 1, kvar(heap, 1)), kvar(heap, 0))))))))))));
}

/* rows_7(): d07-partial-over */
static Vec rows_7(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d07-partial-over", list(heap, 10,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "add3", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0))), kint(heap, big(heap, false, 1, 3u))))),
      krec(heap, list(heap, 2, tid_box(heap, "fn<2>"), tid_box(heap, "nat"))),
      kfun(heap, "later", list(heap, 3, param(heap, rfunc("fn<2>")), param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kapp(heap, kvar(heap, 2), list(heap, 1, kvar(heap, 1))), list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "half", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kapp(heap, kglobal(heap, "add3"), list(heap, 1, kint(heap, big(heap, false, 1, 4u)))), list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "alias", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kapp(heap, kglobal(heap, "natAdd"), list(heap, 1, kint(heap, big(heap, false, 1, 1u)))), list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "half"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))), kapp(heap, kglobal(heap, "alias"), list(heap, 1, kint(heap, big(heap, false, 1, 2u)))))), kapp(heap, kglobal(heap, "later"), list(heap, 3, kglobal(heap, "natAdd"), kint(heap, big(heap, false, 1, 4u)), kint(heap, big(heap, false, 1, 6u))))))))));
}

/* rows_8(): d08-erased-poly */
static Vec rows_8(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d08-erased-poly", list(heap, 6,
      krec(heap, list(heap, 1, tid_box(heap, "any"))),
      kfun(heap, "id", list(heap, 1, param(heap, runion("any"))), runion("any"), kvar(heap, 0)),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "idNat", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "id"), list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "id"), list(heap, 1, kint(heap, big(heap, false, 1, 3u)))), kapp(heap, kglobal(heap, "idNat"), list(heap, 1, kint(heap, big(heap, false, 1, 4u))))))))));
}

/* rows_9(): d09-let-chain */
static Vec rows_9(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d09-let-chain", list(heap, 4,
      krec(heap, list(heap, 2, tid_box(heap, "sum<union nat|union nat>"), tid_box(heap, "nat"))),
      kfun(heap, "pick", list(heap, 1, param(heap, runion("sum<union nat|union nat>"))), runion("nat"), klet(heap, "base", kint(heap, big(heap, false, 1, 10u)), klet(heap, "bump", kcase(heap, "sum<union nat|union nat>", kvar(heap, 1), list(heap, 2, branch(heap, 0, 1, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u))))), branch(heap, 1, 1, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 2u))))))), klet(heap, "total", kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0))), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 100u)))))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<union nat|union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "pick"), list(heap, 1, ktag(heap, "sum<union nat|union nat>", 0, list(heap, 1, kint(heap, big(heap, false, 1, 3u)))))), kapp(heap, kglobal(heap, "pick"), list(heap, 1, ktag(heap, "sum<union nat|union nat>", 1, list(heap, 1, kint(heap, big(heap, false, 1, 4u))))))))))));
}

/* rows_10(): d10-i31-boundary */
static Vec rows_10(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d10-i31-boundary", list(heap, 2,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 2, 32767u, 32767u)), kint(heap, big(heap, false, 1, 1u))))))));
}

/* rows_11(): d11-poly-pair */
static Vec rows_11(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d11-poly-pair", list(heap, 4,
      krec(heap, list(heap, 2, tid_box(heap, "any"), tid_box(heap, "pair<union any,union any>"))),
      kfun(heap, "box", list(heap, 1, param(heap, runion("any"))), rstruct("pair<union any,union any>"), kstruct(heap, "pair<union any,union any>", list(heap, 2, kvar(heap, 0), kvar(heap, 0)))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), klet(heap, "scrut", kapp(heap, kglobal(heap, "box"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 1))))))));
}

/* rows_12(): d12-poly-case */
static Vec rows_12(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d12-poly-case", list(heap, 4,
      krec(heap, list(heap, 1, tid_box(heap, "any"))),
      kfun(heap, "id", list(heap, 1, param(heap, runion("any"))), runion("any"), kvar(heap, 0)),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), kcase(heap, "sum<unit|unit>", kapp(heap, kglobal(heap, "id"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kint(heap, big(heap, false, 1, 1u)))))), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 0, kint(heap, big(heap, false, 1, 1u)))))))));
}

/* rows_13(): d13-function-case */
static Vec rows_13(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d13-function-case", list(heap, 6,
      krec(heap, list(heap, 3, tid_box(heap, "fn<1>"), tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "read", list(heap, 1, param(heap, rfunc("fn<1>"))), runion("nat"), kcase(heap, "sum<unit|unit>", kapp(heap, kvar(heap, 0), list(heap, 1, kint(heap, big(heap, false, 1, 1u)))), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 0, kint(heap, big(heap, false, 1, 1u)))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "eqOne", list(heap, 1, param(heap, runion("nat"))), runion("sum<unit|unit>"), ktail(heap, kglobal(heap, "natEq"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "read"), list(heap, 1, kglobal(heap, "eqOne")))))));
}

/* rows_14(): d14-generic-capture */
static Vec rows_14(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d14-generic-capture", list(heap, 7,
      krec(heap, list(heap, 1, tid_box(heap, "any"))),
      kfun(heap, "id", list(heap, 1, param(heap, runion("any"))), runion("any"), kvar(heap, 0)),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<func fn<1>,union nat>"))),
      kfun(heap, "mk$0", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      kfun(heap, "mk", list(heap, 1, param(heap, runion("nat"))), rstruct("pair<func fn<1>,union nat>"), klet(heap, "z", kapp(heap, kglobal(heap, "id"), list(heap, 1, kvar(heap, 0))), kstruct(heap, "pair<func fn<1>,union nat>", list(heap, 2, kclos(heap, "mk$0", 1, list(heap, 1, kvar(heap, 0))), kvar(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<func fn<1>,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, klet(heap, "scrut", kapp(heap, kglobal(heap, "mk"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))), klet(heap, "f", kproj(heap, "pair<func fn<1>,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<func fn<1>,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), list(heap, 1, kint(heap, big(heap, false, 1, 6u))))))));
}

/* rows_15(): d15-generic-aggregates */
static Vec rows_15(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "d15-generic-aggregates", list(heap, 24,
      krec(heap, list(heap, 1, tid_box(heap, "any"))),
      kfun(heap, "id", list(heap, 1, param(heap, runion("any"))), runion("any"), kvar(heap, 0)),
      krec(heap, list(heap, 2, tid_box(heap, "any"), tid_box(heap, "pair<union any,union any>"))),
      kfun(heap, "box", list(heap, 1, param(heap, runion("any"))), rstruct("pair<union any,union any>"), kstruct(heap, "pair<union any,union any>", list(heap, 2, kvar(heap, 0), kvar(heap, 0)))),
      krec(heap, list(heap, 2, tid_box(heap, "pair<union any,union any>"), tid_box(heap, "any"))),
      kfun(heap, "first", list(heap, 1, param(heap, rstruct("pair<union any,union any>"))), runion("any"), klet(heap, "scrut", kvar(heap, 0), klet(heap, "a", kproj(heap, "pair<union any,union any>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union any,union any>", 1, kvar(heap, 1)), kvar(heap, 1))))),
      krec(heap, list(heap, 2, tid_box(heap, "any"), tid_box(heap, "tuple<union any,union any>"))),
      kfun(heap, "pack", list(heap, 1, param(heap, runion("any"))), rstruct("tuple<union any,union any>"), kstruct(heap, "tuple<union any,union any>", list(heap, 2, kvar(heap, 0), kvar(heap, 0)))),
      krec(heap, list(heap, 2, tid_box(heap, "any"), tid_box(heap, "sum<union any|union any>"))),
      kfun(heap, "choose", list(heap, 1, param(heap, runion("any"))), runion("sum<union any|union any>"), ktag(heap, "sum<union any|union any>", 1, list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 2, tid_box(heap, "sum<union any|union any>"), tid_box(heap, "any"))),
      kfun(heap, "unchoose", list(heap, 1, param(heap, runion("sum<union any|union any>"))), runion("any"), kcase(heap, "sum<union any|union any>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 1, kvar(heap, 0)), branch(heap, 1, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "any"))),
      kfun(heap, "fromEmpty", list(heap, 0), runion("nat"), kcase(heap, "any", eterm_kerased(), list(heap, 0))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<union nat|union nat>"))),
      kfun(heap, "sumNat", list(heap, 0), runion("nat"), kcase(heap, "sum<union nat|union nat>", kapp(heap, kglobal(heap, "id"), list(heap, 1, kapp(heap, kglobal(heap, "choose"), list(heap, 1, kint(heap, big(heap, false, 1, 7u)))))), list(heap, 2, branch(heap, 0, 1, kvar(heap, 0)), branch(heap, 1, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u)))))))),
      krec(heap, list(heap, 3, tid_box(heap, "nat"), tid_box(heap, "pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "nested", list(heap, 0), runion("nat"), klet(heap, "scrut", klet(heap, "scrut", kapp(heap, kglobal(heap, "box"), list(heap, 1, kapp(heap, kglobal(heap, "box"), list(heap, 1, kint(heap, big(heap, false, 1, 5u)))))), klet(heap, "a", kproj(heap, "pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>", 1, kvar(heap, 1)), kvar(heap, 1)))), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "inc", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<func fn<1>,func fn<1>>"))),
      kfun(heap, "fromClosure", list(heap, 0), runion("nat"), ktail(heap, kproj(heap, "tuple<func fn<1>,func fn<1>>", 0, kapp(heap, kglobal(heap, "pack"), list(heap, 1, kglobal(heap, "inc")))), list(heap, 1, kint(heap, big(heap, false, 1, 8u))))),
      krec(heap, list(heap, 4, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"), tid_box(heap, "tuple<union nat,union nat>"), tid_box(heap, "sum<union nat|union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "first"), list(heap, 1, kstruct(heap, "pair<union nat,union nat>", list(heap, 2, kint(heap, big(heap, false, 1, 3u)), kint(heap, big(heap, false, 1, 4u)))))), kproj(heap, "tuple<union nat,union nat>", 1, kapp(heap, kglobal(heap, "pack"), list(heap, 1, kint(heap, big(heap, false, 1, 9u))))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "unchoose"), list(heap, 1, ktag(heap, "sum<union nat|union nat>", 0, list(heap, 1, kint(heap, big(heap, false, 1, 2u)))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "sumNat"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "nested"), kglobal(heap, "fromClosure")))))))))))));
}

/* rows_16(): mu-cata-depth */
static Vec rows_16(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "mu-cata-depth", list(heap, 30,
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "dbl", list(heap, 1, param(heap, runion("mu<N>"))), runion("mu<N>"), kcase(heap, "mu<N>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, ktag(heap, "mu<N>", 0, list(heap, 0))), branch(heap, 1, 1, ktag(heap, "mu<N>", 1, list(heap, 1, ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kvar(heap, 0))))))))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n0", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, ktag(heap, "mu<N>", 0, list(heap, 0))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n1", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n0")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n2", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n1")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n3", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n2")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n4", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n3")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n5", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n4")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n6", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n5")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n7", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n6")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n8", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n7")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n9", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n8")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n10", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n9")))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "n11", list(heap, 0), runion("mu<N>"), ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "dbl"), list(heap, 1, kglobal(heap, "n10")))))),
      krec(heap, list(heap, 4, tid_box(heap, "mu<N>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "toNat", list(heap, 1, param(heap, runion("mu<N>"))), runion("nat"), kcase(heap, "mu<N>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kapp(heap, kglobal(heap, "toNat"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 0)), kapp(heap, kglobal(heap, "toNat"), list(heap, 1, kglobal(heap, "n11")))))))));
}

/* rows_17(): mu-dependent-copy */
static Vec rows_17(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "mu-dependent-copy", list(heap, 12,
      krec(heap, list(heap, 3, tid_box(heap, "mu<V>"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "copy", list(heap, 1, param(heap, runion("mu<V>"))), runion("mu<V>"), kcase(heap, "mu<V>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, ktag(heap, "mu<V>", 0, list(heap, 0))), branch(heap, 1, 2, ktag(heap, "mu<V>", 1, list(heap, 2, kvar(heap, 1), kapp(heap, kglobal(heap, "copy"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 4, tid_box(heap, "mu<V>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "total", list(heap, 1, param(heap, runion("mu<V>"))), runion("nat"), kcase(heap, "mu<V>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 2, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kapp(heap, kglobal(heap, "total"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<V>"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "empty", list(heap, 0), runion("mu<V>"), ktail(heap, kglobal(heap, "copy"), list(heap, 1, ktag(heap, "mu<V>", 0, list(heap, 0))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<V>"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "values", list(heap, 0), runion("mu<V>"), ktag(heap, "mu<V>", 1, list(heap, 2, kint(heap, big(heap, false, 1, 17u)), ktag(heap, "mu<V>", 1, list(heap, 2, kint(heap, big(heap, false, 1, 25u)), kglobal(heap, "empty")))))),
      krec(heap, list(heap, 1, tid_box(heap, "mu<V>"))),
      kfun(heap, "copied", list(heap, 0), runion("mu<V>"), ktail(heap, kglobal(heap, "copy"), list(heap, 1, kglobal(heap, "values")))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "total"), list(heap, 1, kglobal(heap, "copied")))))));
}

/* rows_18(): mu-dependent-layout */
static Vec rows_18(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "mu-dependent-layout", list(heap, 11,
      krec(heap, list(heap, 2, tid_box(heap, "mu<Pack>"), tid_box(heap, "leg<mu<Pack>,0,union any,union nat>"))),
      kfun(heap, "pn", list(heap, 0), runion("mu<Pack>"), ktag(heap, "mu<Pack>", 0, list(heap, 2, kint(heap, big(heap, false, 1, 7u)), kint(heap, big(heap, false, 1, 8u))))),
      krec(heap, list(heap, 2, tid_box(heap, "mu<Pack>"), tid_box(heap, "leg<mu<Pack>,0,union any,union nat>"))),
      kfun(heap, "pu", list(heap, 0), runion("mu<Pack>"), ktag(heap, "mu<Pack>", 0, list(heap, 2, eterm_kerased(), kint(heap, big(heap, false, 1, 9u))))),
      krec(heap, list(heap, 3, tid_box(heap, "nat"), tid_box(heap, "mu<Pack>"), tid_box(heap, "leg<mu<Pack>,0,union any,union nat>"))),
      kfun(heap, "pf$0", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 1, 1u))))),
      kfun(heap, "pf", list(heap, 0), runion("mu<Pack>"), ktag(heap, "mu<Pack>", 0, list(heap, 2, kclos(heap, "pf$0", 1, list(heap, 0)), kint(heap, big(heap, false, 1, 10u))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<Pack>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Pack>,0,union any,union nat>"))),
      kfun(heap, "last", list(heap, 1, param(heap, runion("mu<Pack>"))), runion("nat"), kcase(heap, "mu<Pack>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 2, kvar(heap, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "last"), list(heap, 1, kglobal(heap, "pn"))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "last"), list(heap, 1, kglobal(heap, "pu"))), kapp(heap, kglobal(heap, "last"), list(heap, 1, kglobal(heap, "pf")))))))))));
}

/* rows_19(): mu-mutual-emit */
static Vec rows_19(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "mu-mutual-emit", list(heap, 8,
      krec(heap, list(heap, 6, tid_box(heap, "mu<A>"), tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"), tid_box(heap, "leg<mu<A>,0>"), tid_box(heap, "leg<mu<A>,1,union mu<B>>"))),
      kfun(heap, "sizeA", list(heap, 1, param(heap, runion("mu<A>"))), runion("mu<N>"), kcase(heap, "mu<A>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, ktag(heap, "mu<N>", 0, list(heap, 0))), branch(heap, 1, 1, ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "sizeB"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 5, tid_box(heap, "mu<B>"), tid_box(heap, "mu<N>"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"), tid_box(heap, "leg<mu<B>,0,union mu<A>>"))),
      kfun(heap, "sizeB", list(heap, 1, param(heap, runion("mu<B>"))), runion("mu<N>"), kcase(heap, "mu<B>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, ktag(heap, "mu<N>", 1, list(heap, 1, kapp(heap, kglobal(heap, "sizeA"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 4, tid_box(heap, "mu<N>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<N>,0>"), tid_box(heap, "leg<mu<N>,1,union mu<N>>"))),
      kfun(heap, "toNat", list(heap, 1, param(heap, runion("mu<N>"))), runion("nat"), kcase(heap, "mu<N>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kapp(heap, kglobal(heap, "toNat"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 6, tid_box(heap, "nat"), tid_box(heap, "mu<A>"), tid_box(heap, "mu<B>"), tid_box(heap, "leg<mu<A>,0>"), tid_box(heap, "leg<mu<A>,1,union mu<B>>"), tid_box(heap, "leg<mu<B>,0,union mu<A>>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "toNat"), list(heap, 1, kapp(heap, kglobal(heap, "sizeA"), list(heap, 1, ktag(heap, "mu<A>", 1, list(heap, 1, ktag(heap, "mu<B>", 0, list(heap, 1, ktag(heap, "mu<A>", 1, list(heap, 1, ktag(heap, "mu<B>", 0, list(heap, 1, ktag(heap, "mu<A>", 0, list(heap, 0))))))))))))))))));
}

/* rows_20(): mu-parameter-layout */
static Vec rows_20(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "mu-parameter-layout", list(heap, 10,
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union any>"))),
      kfun(heap, "unboxN", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union any>"))),
      kfun(heap, "unboxF", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, ktail(heap, kvar(heap, 0), list(heap, 1, kint(heap, big(heap, false, 1, 8u)))))))),
      krec(heap, list(heap, 4, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,union nat>"), tid_box(heap, "leg<mu<Box>,0,union any>"))),
      kfun(heap, "unboxP", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kproj(heap, "tuple<union nat,union nat>", 1, kvar(heap, 0)))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union any>"))),
      kfun(heap, "unboxU", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kint(heap, big(heap, false, 1, 9u)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), kint(heap, big(heap, false, 1, 9u))))));
}

/* rows_21(): nat-big */
static Vec rows_21(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "nat-big", list(heap, 4,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "mul", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natMul"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natSub"), list(heap, 2, kapp(heap, kglobal(heap, "mul"), list(heap, 2, kint(heap, big(heap, false, 4, 1u, 0u, 0u, 1u)), kint(heap, big(heap, false, 4, 5u, 0u, 0u, 1u)))), kint(heap, big(heap, false, 7, 32766u, 32767u, 32767u, 5u, 0u, 0u, 1u))))))));
}

/* rows_22(): nat-runtime-capture */
static Vec rows_22(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "nat-runtime-capture", list(heap, 6,
      krec(heap, list(heap, 2, tid_box(heap, "any"), tid_box(heap, "pair<union any,union any>"))),
      kfun(heap, "box", list(heap, 1, param(heap, runion("any"))), rstruct("pair<union any,union any>"), kstruct(heap, "pair<union any,union any>", list(heap, 2, kvar(heap, 0), kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "mk", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natSub"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0))), kvar(heap, 1)))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "mk"), list(heap, 2, klet(heap, "scrut", kapp(heap, kglobal(heap, "box"), list(heap, 1, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), klet(heap, "a", kproj(heap, "pair<union nat,union nat>", 0, kvar(heap, 0)), klet(heap, "y", kproj(heap, "pair<union nat,union nat>", 1, kvar(heap, 1)), kvar(heap, 1)))), kint(heap, big(heap, false, 1, 19u))))))));
}

/* rows_23(): nat-runtime-edges */
static Vec rows_23(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "nat-runtime-edges", list(heap, 68,
      krec(heap, list(heap, 2, tid_box(heap, "sum<unit|unit>"), tid_box(heap, "nat"))),
      kfun(heap, "boolNat", list(heap, 1, param(heap, runion("sum<unit|unit>"))), runion("nat"), kcase(heap, "sum<unit|unit>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 0, kint(heap, big(heap, false, 1, 1u)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "checkNat", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "smallAdd", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 2, 32767u, 32767u)), kint(heap, big(heap, false, 1, 1u)))), kint(heap, big(heap, false, 3, 0u, 0u, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "smallMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 2, 0u, 1u)), kint(heap, big(heap, false, 2, 0u, 1u)))), kint(heap, big(heap, false, 3, 0u, 0u, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "bigAdd", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 5, 28949u, 24680u, 7906u, 18564u, 8u)))), kint(heap, big(heap, false, 7, 29226u, 15579u, 17832u, 32378u, 13100u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "mixedAdd", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 17u)))), kint(heap, big(heap, false, 7, 294u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "reverseAdd", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 17u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 7, 294u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "bigSub", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 5, 28949u, 24680u, 7906u, 18564u, 8u)))), kint(heap, big(heap, false, 7, 4096u, 31754u, 2018u, 28018u, 13083u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "mixedSub", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 17u)))), kint(heap, big(heap, false, 7, 260u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "truncated", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 1, 17u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "zeroSub", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "borrow", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 7, 0u, 0u, 0u, 0u, 0u, 0u, 1u)), kint(heap, big(heap, false, 6, 32767u, 32767u, 32767u, 32767u, 32767u, 32767u)))), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "carry", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 6, 32767u, 32767u, 32767u, 32767u, 32767u, 32767u)), kint(heap, big(heap, false, 1, 1u)))), kint(heap, big(heap, false, 7, 0u, 0u, 0u, 0u, 0u, 0u, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "bigMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 5, 28949u, 24680u, 7906u, 18564u, 8u)))), kint(heap, big(heap, false, 11, 23481u, 10731u, 3816u, 6581u, 397u, 9285u, 32068u, 24649u, 30124u, 14155u, 85u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "mixedMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 32767u)))), kint(heap, big(heap, false, 8, 32491u, 9377u, 13741u, 28879u, 721u, 13985u, 31865u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "reverseMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 1, 32767u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 8, 32491u, 9377u, 13741u, 28879u, 721u, 13985u, 31865u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "zeroMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 0)))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "reverseZero", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 0)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "oneMul", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 1u)))), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "reverseOne", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natMul"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "demote", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 278u, 23667u, 9924u, 13814u, 13092u, 31875u, 9u)))), kint(heap, big(heap, false, 2, 32767u, 32767u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "heterogeneous", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kint(heap, big(heap, false, 3, 3u, 2u, 1u)), kint(heap, big(heap, false, 3, 32766u, 1u, 1u)))), kint(heap, big(heap, false, 1, 5u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "eqSame", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "eqDifferent", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 278u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "eqMixed", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 3u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "eqReverse", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natEq"), list(heap, 2, kint(heap, big(heap, false, 1, 3u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ltSame", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ltNext", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 278u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ltPrevious", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 7, 278u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ltMixed", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 1, 3u)), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 1, 1u))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ltReverse", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "boolNat"), list(heap, 1, kapp(heap, kglobal(heap, "natLt"), list(heap, 2, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), kint(heap, big(heap, false, 1, 3u)))))), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "alias", list(heap, 0), runion("nat"), klet(heap, "a", kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)), klet(heap, "twice", kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 0), kvar(heap, 0))), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "checkNat"), list(heap, 2, kvar(heap, 1), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), kapp(heap, kglobal(heap, "checkNat"), list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 7, 554u, 14566u, 19851u, 27628u, 26184u, 30982u, 19u))))))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "partial", list(heap, 0), runion("nat"), klet(heap, "mul", kapp(heap, kglobal(heap, "natMul"), list(heap, 1, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))), ktail(heap, kglobal(heap, "checkNat"), list(heap, 2, kapp(heap, kglobal(heap, "natSub"), list(heap, 2, kapp(heap, kvar(heap, 0), list(heap, 1, kint(heap, big(heap, false, 1, 3u)))), kint(heap, big(heap, false, 7, 829u, 5465u, 29777u, 8674u, 6509u, 30090u, 29u)))), kint(heap, big(heap, false, 1, 2u)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "smallAdd"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "smallMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "bigAdd"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "mixedAdd"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "reverseAdd"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "bigSub"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "mixedSub"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "truncated"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "zeroSub"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "borrow"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "carry"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "bigMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "mixedMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "reverseMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "zeroMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "reverseZero"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "oneMul"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "reverseOne"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "demote"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "heterogeneous"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "eqSame"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "eqDifferent"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "eqMixed"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "eqReverse"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "ltSame"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "ltNext"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "ltPrevious"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "ltMixed"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "ltReverse"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "alias"), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kglobal(heap, "partial"), kint(heap, big(heap, false, 0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));
}

/* rows_24(): nat-runtime-export */
static Vec rows_24(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "nat-runtime-export", list(heap, 2,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))));
}

/* rows_25(): nat-runtime-mu */
static Vec rows_25(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "nat-runtime-mu", list(heap, 4,
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union nat>"))),
      kfun(heap, "read", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 3, tid_box(heap, "nat"), tid_box(heap, "mu<Box>"), tid_box(heap, "leg<mu<Box>,0,union nat>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natSub"), list(heap, 2, kapp(heap, kglobal(heap, "read"), list(heap, 1, ktag(heap, "mu<Box>", 0, list(heap, 1, kint(heap, big(heap, false, 7, 277u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u)))))), kint(heap, big(heap, false, 7, 254u, 23667u, 9925u, 13814u, 13092u, 31875u, 9u))))))));
}

/* rows_26(): one-branches */
static Vec rows_26(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "one-branches", list(heap, 8,
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "choose", list(heap, 2, param(heap, runion("nat")), param(heap, runion("sum<unit|unit>"))), runion("nat"), kcase(heap, "sum<unit|unit>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kvar(heap, 1)), branch(heap, 1, 0, kvar(heap, 1))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "nested", list(heap, 2, param(heap, runion("nat")), param(heap, runion("sum<unit|unit>"))), runion("nat"), kcase(heap, "sum<unit|unit>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kcase(heap, "sum<unit|unit>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kvar(heap, 1)), branch(heap, 1, 0, kvar(heap, 1))))), branch(heap, 1, 0, kvar(heap, 1))))),
      krec(heap, list(heap, 2, tid_box(heap, "sum<union nat|union nat>"), tid_box(heap, "nat"))),
      kfun(heap, "payload", list(heap, 1, param(heap, runion("sum<union nat|union nat>"))), runion("nat"), kcase(heap, "sum<union nat|union nat>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 1, kvar(heap, 0)), branch(heap, 1, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "sum<unit|unit>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "choose"), list(heap, 2, kint(heap, big(heap, false, 1, 7u)), ktag(heap, "sum<unit|unit>", 0, list(heap, 0))))))));
}

/* rows_27(): one-fields */
static Vec rows_27(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "one-fields", list(heap, 8,
      krec(heap, list(heap, 3, tid_box(heap, "nat"), tid_box(heap, "mu<Box>"), tid_box(heap, "leg<mu<Box>,0,union nat>"))),
      kfun(heap, "pack", list(heap, 1, param(heap, runion("nat"))), runion("mu<Box>"), ktag(heap, "mu<Box>", 0, list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union nat>"))),
      kfun(heap, "unpack", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "pair<union nat,union nat>"))),
      kfun(heap, "pair", list(heap, 1, param(heap, runion("nat"))), rstruct("pair<union nat,union nat>"), kstruct(heap, "pair<union nat,union nat>", list(heap, 2, kvar(heap, 0), kint(heap, big(heap, false, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "unpack"), list(heap, 1, kapp(heap, kglobal(heap, "pack"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))))))));
}

/* rows_28(): one-identity */
static Vec rows_28(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "one-identity", list(heap, 14,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "identity", list(heap, 1, param(heap, runion("nat"))), runion("nat"), kvar(heap, 0)),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "through", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "identity"), list(heap, 1, kvar(heap, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "alias", list(heap, 1, param(heap, runion("nat"))), runion("nat"), klet(heap, "y", kvar(heap, 0), kvar(heap, 0))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "discardAlias", list(heap, 1, param(heap, runion("nat"))), runion("nat"), klet(heap, "y", kvar(heap, 0), kint(heap, big(heap, false, 0)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ghost", list(heap, 0), runion("nat"), kint(heap, big(heap, false, 0))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "erasedThenOnce", list(heap, 1, param(heap, runion("nat"))), runion("nat"), klet(heap, "y", kapp(heap, kglobal(heap, "ghost"), list(heap, 0)), kvar(heap, 1))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "through"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))))));
}

/* rows_29(): one-runtime-capture */
static Vec rows_29(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "one-runtime-capture", list(heap, 19,
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,func fn<0>>"))),
      kfun(heap, "duplicate$0", list(heap, 0), runion("nat"), kint(heap, big(heap, false, 0))),
      kfun(heap, "duplicate", list(heap, 1, param(heap, runion("nat"))), rstruct("tuple<union nat,func fn<0>>"), kstruct(heap, "tuple<union nat,func fn<0>>", list(heap, 2, kvar(heap, 0), kclos(heap, "duplicate$0", 0, list(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,func fn<1>>"))),
      kfun(heap, "split$0", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kvar(heap, 0)))),
      kfun(heap, "split", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), rstruct("tuple<union nat,func fn<1>>"), kstruct(heap, "tuple<union nat,func fn<1>>", list(heap, 2, kvar(heap, 1), kclos(heap, "split$0", 1, list(heap, 1, kvar(heap, 0)))))),
      krec(heap, list(heap, 4, tid_box(heap, "nat"), tid_box(heap, "mu<At>"), tid_box(heap, "tuple<union nat,func fn<1>>"), tid_box(heap, "leg<mu<At>,0>"))),
      kfun(heap, "constructorCapture$0", list(heap, 1, param(heap, runion("nat"))), runion("mu<At>"), ktag(heap, "mu<At>", 0, list(heap, 0))),
      kfun(heap, "constructorCapture", list(heap, 1, param(heap, runion("nat"))), rstruct("tuple<union nat,func fn<1>>"), kstruct(heap, "tuple<union nat,func fn<1>>", list(heap, 2, kvar(heap, 0), kclos(heap, "constructorCapture$0", 1, list(heap, 0))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "ignore", list(heap, 0), runion("nat"), kint(heap, big(heap, false, 0))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,func fn<1>>"))),
      kfun(heap, "callCapture$0", list(heap, 1, param(heap, runion("nat"))), runion("nat"), ktail(heap, kglobal(heap, "ignore"), list(heap, 0))),
      kfun(heap, "callCapture", list(heap, 1, param(heap, runion("nat"))), rstruct("tuple<union nat,func fn<1>>"), kstruct(heap, "tuple<union nat,func fn<1>>", list(heap, 2, kvar(heap, 0), kclos(heap, "callCapture$0", 1, list(heap, 0))))),
      krec(heap, list(heap, 2, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,func fn<1>>"))),
      kfun(heap, "proofCapture$0", list(heap, 1, param(heap, runion("nat"))), runion("nat"), kint(heap, big(heap, false, 0))),
      kfun(heap, "proofCapture", list(heap, 1, param(heap, runion("nat"))), rstruct("tuple<union nat,func fn<1>>"), kstruct(heap, "tuple<union nat,func fn<1>>", list(heap, 2, kvar(heap, 0), kclos(heap, "proofCapture$0", 1, list(heap, 0))))),
      krec(heap, list(heap, 3, tid_box(heap, "nat"), tid_box(heap, "tuple<union nat,func fn<0>>"), tid_box(heap, "tuple<union nat,func fn<1>>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kproj(heap, "tuple<union nat,func fn<0>>", 0, kapp(heap, kglobal(heap, "duplicate"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))), kapp(heap, kproj(heap, "tuple<union nat,func fn<1>>", 1, kapp(heap, kglobal(heap, "split"), list(heap, 2, kint(heap, big(heap, false, 1, 11u)), kint(heap, big(heap, false, 1, 13u))))), list(heap, 1, kint(heap, big(heap, false, 1, 17u)))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kproj(heap, "tuple<union nat,func fn<1>>", 0, kapp(heap, kglobal(heap, "constructorCapture"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))), kapp(heap, kglobal(heap, "natAdd"), list(heap, 2, kproj(heap, "tuple<union nat,func fn<1>>", 0, kapp(heap, kglobal(heap, "callCapture"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))), kproj(heap, "tuple<union nat,func fn<1>>", 0, kapp(heap, kglobal(heap, "proofCapture"), list(heap, 1, kint(heap, big(heap, false, 1, 7u)))))))))))))));
}

/* rows_30(): one-shadowing */
static Vec rows_30(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "one-shadowing", list(heap, 9,
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "identity", list(heap, 1, param(heap, runion("nat"))), runion("nat"), kvar(heap, 0)),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "shadow", list(heap, 1, param(heap, runion("nat"))), runion("nat"), klet(heap, "y", kvar(heap, 0), klet(heap, "x", kint(heap, big(heap, false, 0)), kvar(heap, 1)))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "closure$0", list(heap, 2, param(heap, runion("nat")), param(heap, runion("nat"))), runion("nat"), kvar(heap, 1)),
      kfun(heap, "closure", list(heap, 1, param(heap, runion("nat"))), runion("nat"), klet(heap, "f", kclos(heap, "closure$0", 1, list(heap, 1, kvar(heap, 0))), ktail(heap, kvar(heap, 0), list(heap, 1, kint(heap, big(heap, false, 0)))))),
      krec(heap, list(heap, 1, tid_box(heap, "nat"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "shadow"), list(heap, 1, kint(heap, big(heap, false, 1, 7u))))))));
}

/* rows_31(): sl-match-indexed */
static Vec rows_31(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "sl-match-indexed", list(heap, 6,
      krec(heap, list(heap, 4, tid_box(heap, "mu<V>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "total", list(heap, 1, param(heap, runion("mu<V>"))), runion("nat"), kcase(heap, "mu<V>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 2, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kvar(heap, 1), kapp(heap, kglobal(heap, "total"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<Box>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<Box>,0,union any>"))),
      kfun(heap, "unbox", list(heap, 1, param(heap, runion("mu<Box>"))), runion("nat"), kcase(heap, "mu<Box>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, kvar(heap, 0))))),
      krec(heap, list(heap, 4, tid_box(heap, "nat"), tid_box(heap, "mu<V>"), tid_box(heap, "leg<mu<V>,0>"), tid_box(heap, "leg<mu<V>,1,union nat,union mu<V>>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "total"), list(heap, 1, ktag(heap, "mu<V>", 1, list(heap, 2, kint(heap, big(heap, false, 1, 17u)), ktag(heap, "mu<V>", 1, list(heap, 2, kint(heap, big(heap, false, 1, 25u)), ktag(heap, "mu<V>", 0, list(heap, 0))))))))))));
}

/* rows_32(): sl-mutual-three */
static Vec rows_32(Heap *heap) {
  return list(heap, 1,
    code_row(heap, "sl-mutual-three", list(heap, 8,
      krec(heap, list(heap, 4, tid_box(heap, "mu<A>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<A>,0>"), tid_box(heap, "leg<mu<A>,1,union mu<B>>"))),
      kfun(heap, "sizeA", list(heap, 1, param(heap, runion("mu<A>"))), runion("nat"), kcase(heap, "mu<A>", kvar(heap, 0), list(heap, 2, branch(heap, 0, 0, kint(heap, big(heap, false, 0))), branch(heap, 1, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kapp(heap, kglobal(heap, "sizeB"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<B>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<B>,0,union mu<C>>"))),
      kfun(heap, "sizeB", list(heap, 1, param(heap, runion("mu<B>"))), runion("nat"), kcase(heap, "mu<B>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kapp(heap, kglobal(heap, "sizeC"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 3, tid_box(heap, "mu<C>"), tid_box(heap, "nat"), tid_box(heap, "leg<mu<C>,0,union mu<A>>"))),
      kfun(heap, "sizeC", list(heap, 1, param(heap, runion("mu<C>"))), runion("nat"), kcase(heap, "mu<C>", kvar(heap, 0), list(heap, 1, branch(heap, 0, 1, ktail(heap, kglobal(heap, "natAdd"), list(heap, 2, kint(heap, big(heap, false, 1, 1u)), kapp(heap, kglobal(heap, "sizeA"), list(heap, 1, kvar(heap, 0))))))))),
      krec(heap, list(heap, 8, tid_box(heap, "nat"), tid_box(heap, "mu<A>"), tid_box(heap, "mu<B>"), tid_box(heap, "mu<C>"), tid_box(heap, "leg<mu<A>,0>"), tid_box(heap, "leg<mu<A>,1,union mu<B>>"), tid_box(heap, "leg<mu<C>,0,union mu<A>>"), tid_box(heap, "leg<mu<B>,0,union mu<C>>"))),
      kfun(heap, "main", list(heap, 0), runion("nat"), ktail(heap, kglobal(heap, "sizeA"), list(heap, 1, ktag(heap, "mu<A>", 1, list(heap, 1, ktag(heap, "mu<B>", 0, list(heap, 1, ktag(heap, "mu<C>", 0, list(heap, 1, ktag(heap, "mu<A>", 0, list(heap, 0))))))))))))));
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

/* output(name, expected, r): die with "name: error", or print the JSON line of the bytes */
static int output(Heap *heap, const char *name, const char *expected, Result r) {
  switch (r.kind) {
    case RESULT_FAIL: {
      Str err = error_to_string(heap, r.error);
      Str tail = heap_str_cat(heap, str_lit(": "), err);
      return driver_die_text(1, heap_str_cat(heap, str_lit(name), tail));
    }
    case RESULT_DONE: {
      Str bytes = bytes_text(heap, *(const Str *)r.value);
      Str close = heap_str_cat(heap, bytes, str_lit("]}"));
      Str head = heap_str_cat(heap, str_lit("{\"name\":\""), str_lit(name));
      Str mid = heap_str_cat(heap, str_lit("\",\"expected\":"), str_lit(expected));
      Str open = heap_str_cat(heap, mid, str_lit(",\"bytes\":["));
      Str front = heap_str_cat(heap, head, open);
      return driver_print(heap_str_cat(heap, front, close));
    }
  }
  return 0;
}

/* One output call of main: the snapshot name, the original runtime answer, the rows */
typedef struct {
  const char *name;
  const char *expected;
  Vec (*rows)(Heap *heap);
} OracleCase;

/* main(): every snapshot in order through Emit.program(rows, "main"), stopping at the first die */
static int run(Heap *heap) {
  static const OracleCase cases[] = {
      {"circuit-named-tree", "{\"value\":42}", rows_0},
      {"d01-lit-prims", "{\"value\":17}", rows_1},
      {"d02-tail-call", "{\"value\":10}", rows_2},
      {"d03-pair", "{\"value\":26}", rows_3},
      {"d04-tuple", "{\"value\":10}", rows_4},
      {"d05-sum-legs", "{\"value\":78}", rows_5},
      {"d06-closure-capture", "{\"value\":31}", rows_6},
      {"d07-partial-over", "{\"value\":25}", rows_7},
      {"d08-erased-poly", "{\"value\":7}", rows_8},
      {"d09-let-chain", "{\"value\":232}", rows_9},
      {"d10-i31-boundary", "{\"trap\":true}", rows_10},
      {"d11-poly-pair", "{\"value\":5}", rows_11},
      {"d12-poly-case", "{\"value\":1}", rows_12},
      {"d13-function-case", "{\"value\":1}", rows_13},
      {"d14-generic-capture", "{\"value\":11}", rows_14},
      {"d15-generic-aggregates", "{\"value\":36}", rows_15},
      {"mu-cata-depth", "{\"value\":4095}", rows_16},
      {"mu-dependent-copy", "{\"value\":42}", rows_17},
      {"mu-dependent-layout", "{\"value\":27}", rows_18},
      {"mu-mutual-emit", "{\"value\":4}", rows_19},
      {"mu-parameter-layout", "{\"value\":9}", rows_20},
      {"nat-big", "{\"value\":7}", rows_21},
      {"nat-runtime-capture", "{\"value\":19}", rows_22},
      {"nat-runtime-edges", "{\"value\":32}", rows_23},
      {"nat-runtime-export", "{\"trap\":true}", rows_24},
      {"nat-runtime-mu", "{\"value\":23}", rows_25},
      {"one-branches", "{\"value\":7}", rows_26},
      {"one-fields", "{\"value\":7}", rows_27},
      {"one-identity", "{\"value\":7}", rows_28},
      {"one-runtime-capture", "{\"value\":58}", rows_29},
      {"one-shadowing", "{\"value\":7}", rows_30},
      {"sl-match-indexed", "{\"value\":42}", rows_31},
      {"sl-mutual-three", "{\"value\":3}", rows_32}
  };
  size_t count = sizeof cases / sizeof cases[0];
  int code = 0;
  for (size_t i = 0; i < count && code == 0; i++) {
    Vec rows = cases[i].rows(heap);
    Result r = emit_program(heap, &rows, str_lit("main"));
    code = output(heap, cases[i].name, cases[i].expected, r);
  }
  return code;
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
