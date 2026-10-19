/* Port of wasm/emit.bend: kernel terms (E.Ktm) to WasmGC instruction lists over a link layout.
 * An S.Comp(A) def runs in direct state-passing form (WasmState *st updated in place, the Result
 * returned), which is the Bend T -> C.Pair<T, Result> threading with the pair split into the pointer
 * and the return value; a Fail keeps the state at the failure point, as bind_result does. Lists are
 * Vecs of pointers; a failed allocation sets heap->oom and leaves the item out. */
#include <stdarg.h>

#include "wasm.h"

static const Result result_zero;

/* The end marker of a seq() argument list. */
static GcInstr stop;

/* A Bend instruction list literal: seq(heap, i1, ..., iN, &stop) */
static Vec seq(Heap *heap, ...) {
  Vec out = vec_new(heap->arena);
  va_list items;
  va_start(items, heap);
  for (GcInstr *item = va_arg(items, GcInstr *); item != &stop; item = va_arg(items, GcInstr *)) {
    if (item == NULL || !vec_push(&out, item)) heap->oom = true;
  }
  va_end(items);
  return out;
}

static GcInstr *op(Heap *heap, GcInstrKind kind) { return gc_instr(heap, kind); }
static GcInstr *at(Heap *heap, GcInstrKind kind, uint32_t index) { return gc_instr_index(heap, kind, index); }
static GcInstr *num(Heap *heap, uint32_t v) { return gc_instr_index(heap, GC_I32_CONST, v); }

static Result done(void *value) {
  Result r = result_zero;
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static Result fail(Error e) {
  Result r = result_zero;
  r.kind = RESULT_FAIL;
  r.error = e;
  return r;
}

/* Done{list} with the Vec boxed */
static Result done_vec(Heap *heap, Vec v) {
  Vec *box = arena_alloc(heap->arena, sizeof(Vec));
  if (box == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  *box = v;
  return done(box);
}

static Result done_empty(Heap *heap) { return done_vec(heap, vec_new(heap->arena)); }

static uint32_t index_of(Result r) { return *(const uint32_t *)r.value; }

/* U32.from_nat(n), as link.c's link_u32 (0 when n does not fit) */
static uint32_t u32_of(Nat n) {
  NatToU64 v = nat_to_u64(n);
  return v.status == NAT_TO_U64_OK ? (uint32_t)v.value : 0;
}

/* S.alloc(v) in direct form */
static Result alloc_local(Heap *heap, WasmState *st, GcValtype v) {
  WasmStep step = state_allocate(heap, *st, v);
  *st = step.state;
  return step.result;
}

/* layout(c) */
const Linked *emit_layout(const EmitCtx *c) { return c->layout; }

/* prog(l) */
const Prog *emit_prog(const Linked *l) { return &l->prog; }

/* cat(a, b) = a ++ b */
Vec emit_cat(Heap *heap, const Vec *a, const Vec *b) { return nat_runtime_cat(heap, a, b); }

/* cats(xs): the lists of xs (Vec * items) concatenated in order */
Vec emit_cats(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet x = vec_get(xs, i);
    if (x.status == VEC_GET_OUT_OF_RANGE) continue;
    out = emit_cat(heap, &out, (const Vec *)x.value);
  }
  return out;
}

/* same(a, b): the printed reprs are equal */
bool emit_same(Heap *heap, Repr a, Repr b) { return str_eq(eterm_print_repr(heap, a), eterm_print_repr(heap, b)); }

/* coerce_diff(l, src, dst): the cast from src to dst, checked in the Bend case order */
Result emit_coerce_diff(Heap *heap, const Linked *l, Repr src, Repr dst) {
  if (src.kind == REPR_STRUCT && dst.kind == REPR_STRUCT) {
    Result af = link_fields_of(heap, src.tid.name);
    if (af.kind == RESULT_FAIL) return af;
    Result bf = link_fields_of(heap, dst.tid.name);
    if (bf.kind == RESULT_FAIL) return bf;
    if (((const Vec *)af.value)->len == ((const Vec *)bf.value)->len) return done_empty(heap);
    return fail(error_make(ERROR_MISMATCH, str_lit("aggregate representations have different widths")));
  }
  Result i = result_zero;
  switch (dst.kind) {
    case REPR_UNION:
      return done_empty(heap);
    case REPR_I31:
      return done_vec(heap, seq(heap, gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_I31, 0)), &stop));
    case REPR_STRUCT:
      i = link_type_index(heap, l, dst.tid.name);
      if (i.kind == RESULT_FAIL) return i;
      return done_vec(heap, seq(heap, gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, index_of(i))), &stop));
    case REPR_FUNC:
      i = link_type_index(heap, l, str_lit("clos"));
      if (i.kind == RESULT_FAIL) return i;
      return done_vec(heap, seq(heap, gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, index_of(i))), &stop));
    case REPR_THUNK:
      return fail(error_make(ERROR_NOT_YET, str_lit("RThunk arrives at M2")));
  }
  return fail(error_host_oom());
}

/* coerce(l, src, dst): no cast when the reprs print the same */
Result emit_coerce(Heap *heap, const Linked *l, Repr src, Repr dst) {
  if (emit_same(heap, src, dst)) return done_empty(heap);
  return emit_coerce_diff(heap, l, src, dst);
}

/* terminal(List.reverse(body)): the last instruction of body is Unreachable */
bool emit_terminal(const Vec *body) {
  if (body->len == 0) return false;
  VecGet last = vec_get(body, body->len - 1);
  return last.status != VEC_GET_OUT_OF_RANGE && ((const GcInstr *)last.value)->kind == GC_UNREACHABLE;
}

/* with_coercion(body, cast): body as is when it ends in Unreachable, else body ++ cast */
Vec emit_with_coercion(Heap *heap, const Vec *body, const Vec *cast) {
  if (emit_terminal(body)) return *body;
  return emit_cat(heap, body, cast);
}

/* empty_cast(r): r is Done{[]} */
bool emit_empty_cast(Result r) { return r.kind == RESULT_DONE && ((const Vec *)r.value)->len == 0; }

/* tail_ok(c, tail, r) = tail && empty_cast(coerce(l, r, res)) */
bool emit_tail_ok(Heap *heap, const EmitCtx *c, bool tail, Repr r) {
  return tail && emit_empty_cast(emit_coerce(heap, c->layout, r, c->result));
}

/* reprs(env): the Repr * of each EmitBinding * in env */
Vec emit_reprs(Heap *heap, const Vec *env) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < env->len; i++) {
    VecGet b = vec_get(env, i);
    if (b.status == VEC_GET_OUT_OF_RANGE) continue;
    if (!vec_push(&out, (void *)&((const EmitBinding *)b.value)->repr)) heap->oom = true;
  }
  return out;
}

/* rep(c, env, tm) = L.infer(prog(layout(c)), reprs(env), tm) */
Result emit_rep(Heap *heap, const EmitCtx *c, const Vec *env, const Ktm *tm) {
  Vec rs = emit_reprs(heap, env);
  return link_infer(heap, emit_prog(emit_layout(c)), &rs, tm);
}

/* stores(xs, arr, local, i): [Local_get local, I32_const i, I32_const x, Array_set arr] per limb */
Vec emit_stores(Heap *heap, Limbs xs, uint32_t arr, uint32_t local, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t k = 0; k < xs.len; k++) {
    Vec one = seq(heap, at(heap, GC_LOCAL_GET, local), num(heap, i + (uint32_t)k), num(heap, xs.items[k]),
                  at(heap, GC_ARRAY_SET, arr), &stop);
    out = emit_cat(heap, &out, &one);
  }
  return out;
}

/* literal_big(l, digits): a nat-big struct over a fresh nat-limbs array local */
Result emit_literal_big(Heap *heap, WasmState *st, const Linked *l, MaybeLimbs digits) {
  if (!digits.some) return fail(error_make(ERROR_MISMATCH, str_lit("a Nat literal is negative")));
  Result ai = link_type_index(heap, l, str_lit("nat-limbs"));
  if (ai.kind == RESULT_FAIL) return ai;
  Result bi = link_type_index(heap, l, str_lit("nat-big"));
  if (bi.kind == RESULT_FAIL) return bi;
  Result idx = alloc_local(heap, st, gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, index_of(ai))));
  if (idx.kind == RESULT_FAIL) return idx;
  uint32_t a = index_of(ai);
  uint32_t x = index_of(idx);
  Vec head = seq(heap, num(heap, 0), num(heap, (uint32_t)digits.value.len), at(heap, GC_ARRAY_NEW, a),
                 at(heap, GC_LOCAL_SET, x), &stop);
  Vec body = emit_stores(heap, digits.value, a, x, 0);
  Vec tail = seq(heap, num(heap, 1), at(heap, GC_LOCAL_GET, x), at(heap, GC_STRUCT_NEW, index_of(bi)), &stop);
  Vec front = emit_cat(heap, &head, &body);
  return done_vec(heap, emit_cat(heap, &front, &tail));
}

/* literal_small(l, n, small): an i31 when n fits, else literal_big over the 15-bit limbs */
Result emit_literal_small(Heap *heap, WasmState *st, const Linked *l, Big n, MaybeU32 small) {
  if (small.some) return done_vec(heap, seq(heap, num(heap, small.value), op(heap, GC_REF_I31), &stop));
  return emit_literal_big(heap, st, l, big_limbs15(n));
}

/* literal(l, lit) */
Result emit_literal(Heap *heap, WasmState *st, const Linked *l, const Literal *lit) {
  switch (lit->kind) {
    case LITERAL_INT:
      return emit_literal_small(heap, st, l, lit->integer, big_to_i31(lit->integer));
    case LITERAL_STRING:
      return fail(error_make(ERROR_NOT_YET, str_lit("a string literal has no wasm form at M0")));
  }
  return fail(error_host_oom());
}

/* closure_value(c, n, m): a clos struct of arity m over the wrapper of n and an empty i31 env */
Result emit_closure_value(Heap *heap, WasmState *st, const EmitCtx *c, Str n, Nat m) {
  (void)st;
  Result wi = link_func_index(heap, emit_layout(c), link_wrap_fkey(heap, n, nat_of_u64(heap->arena, 0)));
  if (wi.kind == RESULT_FAIL) return wi;
  Result ci = link_type_index(heap, emit_layout(c), str_lit("clos"));
  if (ci.kind == RESULT_FAIL) return ci;
  return done_vec(heap, seq(heap, num(heap, u32_of(m)), at(heap, GC_REF_FUNC, index_of(wi)), num(heap, 0),
                            op(heap, GC_REF_I31), at(heap, GC_STRUCT_NEW, index_of(ci)), &stop));
}

/* optional(m, e): Fail{e} on None (NULL), else Done{a} */
Result emit_optional(const void *m, Error e) {
  if (m == NULL) return fail(e);
  return done((void *)m);
}

/* nth(xs, i): the i-th item, or out of range */
VecGet emit_nth(const Vec *xs, Nat i) { return common_at(xs, i); }

/* var(b, i): Local_get of the binding, or Unbound when b is None (NULL) */
Result emit_var(Heap *heap, WasmState *st, const EmitBinding *b, Nat i) {
  (void)st;
  if (b != NULL) return done_vec(heap, seq(heap, at(heap, GC_LOCAL_GET, b->index), &stop));
  Str head = str_cat(heap->arena, str_lit("KVar "), heap_nat_show(heap, i));
  return fail(error_make(ERROR_UNBOUND, str_cat(heap->arena, head, str_lit(" is out of scope"))));
}

/* each(c, env, xs): go(c, env, False, x) per term, the lists concatenated in order */
Result emit_each(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet x = vec_get(xs, i);
    if (x.status == VEC_GET_OUT_OF_RANGE) continue;
    Result a = emit_go(heap, st, c, env, false, (const Ktm *)x.value);
    if (a.kind == RESULT_FAIL) return a;
    out = emit_cat(heap, &out, (const Vec *)a.value);
  }
  return done_vec(heap, out);
}

/* each2(c, env, xs, rs): arg_at(c, env, x, r) per pair; a length mismatch fails after the common prefix */
Result emit_each2(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Vec *xs, const Vec *rs) {
  Vec out = vec_new(heap->arena);
  size_t n = xs->len < rs->len ? xs->len : rs->len;
  for (size_t i = 0; i < n; i++) {
    VecGet x = vec_get(xs, i);
    VecGet r = vec_get(rs, i);
    if (x.status == VEC_GET_OUT_OF_RANGE || r.status == VEC_GET_OUT_OF_RANGE) continue;
    Result a = emit_arg_at(heap, st, c, env, (const Ktm *)x.value, *(const Repr *)r.value);
    if (a.kind == RESULT_FAIL) return a;
    out = emit_cat(heap, &out, (const Vec *)a.value);
  }
  if (xs->len < rs->len)
    return fail(error_make(ERROR_MISMATCH, str_lit("an argument list is shorter than its signature")));
  if (xs->len > rs->len)
    return fail(error_make(ERROR_MISMATCH, str_lit("an argument list is longer than its signature")));
  return done_vec(heap, out);
}

/* global_fun(m, c, name, tail, r): a (tail) call of fun:name at arity 0, else its closure value */
Result emit_global_fun(Heap *heap, WasmState *st, Nat m, const EmitCtx *c, Str name, bool tail, Repr r) {
  if (!nat_is_zero(m)) return emit_closure_value(heap, st, c, name, m);
  Result fi = link_func_index(heap, emit_layout(c), str_cat(heap->arena, str_lit("fun:"), name));
  if (fi.kind == RESULT_FAIL) return fi;
  GcInstrKind call = emit_tail_ok(heap, c, tail, r) ? GC_RETURN_CALL : GC_CALL;
  return done_vec(heap, seq(heap, at(heap, call, index_of(fi)), &stop));
}

/* global_kind(k, c, name, tail) */
Result emit_global_kind(Heap *heap, WasmState *st, LinkHead k, const EmitCtx *c, Str name, bool tail) {
  Result r = result_zero;
  switch (k.kind) {
    case LINK_HEAD_FUN:
      return emit_global_fun(heap, st, nat_of_u64(heap->arena, k.fn->params.len), c, name, tail, k.fn->result);
    case LINK_HEAD_PRIM:
      return emit_closure_value(heap, st, c, name, nat_of_u64(heap->arena, 2));
    case LINK_HEAD_VALUE:
      r = link_global_repr(heap, emit_prog(emit_layout(c)), name);
      if (r.kind == RESULT_FAIL) return r;
      return fail(error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("no runtime value for "), name)));
  }
  return fail(error_host_oom());
}

/* global(c, name, tail) = global_kind(L.head_kind(prog(layout(c)), E.KGlobal{name}), c, name, tail) */
Result emit_global(Heap *heap, WasmState *st, const EmitCtx *c, Str name, bool tail) {
  LinkHead k = link_head_kind(emit_prog(emit_layout(c)), eterm_kglobal(heap, name));
  return emit_global_kind(heap, st, k, c, name, tail);
}

/* List.take(xs, n) */
static Vec take_vec(Heap *heap, const Vec *xs, size_t n) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len && i < n; i++) {
    VecGet x = vec_get(xs, i);
    if (x.status != VEC_GET_OUT_OF_RANGE && !vec_push(&out, x.value)) heap->oom = true;
  }
  return out;
}

/* List.drop(xs, n) */
static Vec drop_vec(Heap *heap, const Vec *xs, size_t n) {
  Vec out = vec_new(heap->arena);
  for (size_t i = n; i < xs->len; i++) {
    VecGet x = vec_get(xs, i);
    if (x.status != VEC_GET_OUT_OF_RANGE && !vec_push(&out, x.value)) heap->oom = true;
  }
  return out;
}

/* direct_last(c, env, tail, r, ia, fi, rest): the saturated call, then steps over the extra args */
Result emit_direct_last(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr r,
                        const Vec *ia, uint32_t fi, const Vec *rest) {
  if (rest->len == 0) {
    GcInstrKind call = emit_tail_ok(heap, c, tail, r) ? GC_RETURN_CALL : GC_CALL;
    Vec last = seq(heap, at(heap, call, fi), &stop);
    return done_vec(heap, emit_cat(heap, ia, &last));
  }
  Vec call = seq(heap, at(heap, GC_CALL, fi), &stop);
  Vec head = emit_cat(heap, ia, &call);
  return emit_steps(heap, st, c, env, tail, &head, r, rest);
}

/* direct_short(short, c, env, tail, name, params, result, args) */
Result emit_direct_short(Heap *heap, WasmState *st, bool is_short, const EmitCtx *c, const Vec *env, bool tail,
                         Str name, const Vec *params, Repr result, const Vec *args) {
  Nat m = nat_of_u64(heap->arena, params->len);
  if (is_short) {
    Result ih = emit_closure_value(heap, st, c, name, m);
    if (ih.kind == RESULT_FAIL) return ih;
    return emit_steps(heap, st, c, env, tail, (const Vec *)ih.value, eterm_rfunc(eterm_tid(link_fn_key(heap, m))),
                      args);
  }
  Vec taken = take_vec(heap, args, params->len);
  Result ia = emit_each2(heap, st, c, env, &taken, params);
  if (ia.kind == RESULT_FAIL) return ia;
  Result fi = link_func_index(heap, emit_layout(c), str_cat(heap->arena, str_lit("fun:"), name));
  if (fi.kind == RESULT_FAIL) return fi;
  Vec rest = drop_vec(heap, args, params->len);
  return emit_direct_last(heap, st, c, env, tail, result, (const Vec *)ia.value, index_of(fi), &rest);
}

/* direct(c, env, tail, name, params, result, args): short when fewer args than params */
Result emit_direct(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Str name,
                   const Vec *params, Repr result, const Vec *args) {
  return emit_direct_short(heap, st, args->len < params->len, c, env, tail, name, params, result, args);
}

/* primitive_short(short, c, env, tail, name, args): a nat-runtime call on two args, else its closure */
Result emit_primitive_short(Heap *heap, WasmState *st, bool is_short, const EmitCtx *c, const Vec *env, bool tail,
                            Str name, const Vec *args) {
  Nat two = nat_of_u64(heap->arena, 2);
  if (is_short) {
    Result ih = emit_closure_value(heap, st, c, name, two);
    if (ih.kind == RESULT_FAIL) return ih;
    return emit_steps(heap, st, c, env, tail, (const Vec *)ih.value,
                      eterm_rfunc(eterm_tid(link_fn_key(heap, two))), args);
  }
  Vec taken = take_vec(heap, args, 2);
  Result ia = emit_each(heap, st, c, env, &taken);
  if (ia.kind == RESULT_FAIL) return ia;
  Result fi = link_func_index(heap, emit_layout(c), str_cat(heap->arena, str_lit("nat-runtime:"), name));
  if (fi.kind == RESULT_FAIL) return fi;
  Vec call = seq(heap, at(heap, GC_CALL, index_of(fi)), &stop);
  Vec head = emit_cat(heap, (const Vec *)ia.value, &call);
  Vec rest = drop_vec(heap, args, 2);
  return emit_steps(heap, st, c, env, tail, &head, link_prim_result(name), &rest);
}

/* apply_kind(k, c, env, tail, head, args) */
Result emit_apply_kind(Heap *heap, WasmState *st, LinkHead k, const EmitCtx *c, const Vec *env, bool tail,
                       const Ktm *head, const Vec *args) {
  Result ih = result_zero;
  Result hr = result_zero;
  switch (k.kind) {
    case LINK_HEAD_FUN:
      if (args->len == 0) return emit_go(heap, st, c, env, tail, head);
      return emit_direct(heap, st, c, env, tail, k.name, &k.fn->params, k.fn->result, args);
    case LINK_HEAD_PRIM:
      if (args->len == 0) return emit_go(heap, st, c, env, tail, head);
      return emit_primitive_short(heap, st, args->len < 2, c, env, tail, k.name, args);
    case LINK_HEAD_VALUE:
      ih = emit_go(heap, st, c, env, false, head);
      if (ih.kind == RESULT_FAIL) return ih;
      hr = emit_rep(heap, c, env, head);
      if (hr.kind == RESULT_FAIL) return hr;
      return emit_steps(heap, st, c, env, tail, (const Vec *)ih.value, *(const Repr *)hr.value, args);
  }
  return fail(error_host_oom());
}

/* apply(c, env, tail, h, args) = apply_kind(L.head_kind(prog(layout(c)), h), ...) */
Result emit_apply(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *h,
                  const Vec *args) {
  return emit_apply_kind(heap, st, link_head_kind(emit_prog(emit_layout(c)), h), c, env, tail, h, args);
}

/* arg_at(c, env, tm, r): the term, then its cast to r */
Result emit_arg_at(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm, Repr r) {
  Result code = emit_go(heap, st, c, env, false, tm);
  if (code.kind == RESULT_FAIL) return code;
  Result tr = emit_rep(heap, c, env, tm);
  if (tr.kind == RESULT_FAIL) return tr;
  Result cz = emit_coerce(heap, emit_layout(c), *(const Repr *)tr.value, r);
  if (cz.kind == RESULT_FAIL) return cz;
  return done_vec(heap, emit_cat(heap, (const Vec *)code.value, (const Vec *)cz.value));
}

/* steps_apply(c, env, tail, head, r, args): head, the args, a call of apply-k, the cast of the result */
Result emit_steps_apply(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head,
                        Repr r, const Vec *args) {
  Nat k = nat_of_u64(heap->arena, args->len);
  Result ia = emit_each(heap, st, c, env, args);
  if (ia.kind == RESULT_FAIL) return ia;
  Result fi = link_func_index(heap, emit_layout(c), link_apply_fkey(heap, k));
  if (fi.kind == RESULT_FAIL) return fi;
  Result want = link_steps_result(heap, r, k);
  if (want.kind == RESULT_FAIL) return want;
  Result cz = emit_coerce(heap, emit_layout(c), link_any_repr(), *(const Repr *)want.value);
  if (cz.kind == RESULT_FAIL) return cz;
  GcInstrKind call = emit_tail_ok(heap, c, tail, link_any_repr()) ? GC_RETURN_CALL : GC_CALL;
  Vec callv = seq(heap, at(heap, call, index_of(fi)), &stop);
  Vec front = emit_cat(heap, head, (const Vec *)ia.value);
  Vec mid = emit_cat(heap, &front, &callv);
  return done_vec(heap, emit_cat(heap, &mid, (const Vec *)cz.value));
}

/* steps_type(c, env, tail, head, r, args): only a function or union head applies */
Result emit_steps_type(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head,
                       Repr r, const Vec *args) {
  Result m = result_zero;
  switch (r.kind) {
    case REPR_FUNC:
      m = link_arity_of_fn(heap, r.tid.name);
      if (m.kind == RESULT_FAIL) return m;
      return emit_steps_apply(heap, st, c, env, tail, head, r, args);
    case REPR_UNION:
      return emit_steps_apply(heap, st, c, env, tail, head, r, args);
    case REPR_THUNK:
      return fail(error_make(ERROR_NOT_YET, str_lit("KForce arrives at M2")));
    case REPR_I31:
    case REPR_STRUCT:
      return fail(error_make(ERROR_MISMATCH, str_lit("an application head is not a function")));
  }
  return fail(error_host_oom());
}

/* steps(c, env, tail, head, r, args): head as is when no args and r is not a nullary closure */
Result emit_steps(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Vec *head, Repr r,
                  const Vec *args) {
  if (args->len == 0 && !link_nullary_closure(heap, r)) return done_vec(heap, *head);
  return emit_steps_type(heap, st, c, env, tail, head, r, args);
}

/* cats([a, b, c]) and cats([a, b, c, d]) */
static Vec cat3(Heap *heap, const Vec *a, const Vec *b, const Vec *c) {
  Vec ab = emit_cat(heap, a, b);
  return emit_cat(heap, &ab, c);
}

static Vec cat4(Heap *heap, const Vec *a, const Vec *b, const Vec *c, const Vec *d) {
  Vec abc = cat3(heap, a, b, c);
  return emit_cat(heap, &abc, d);
}

/* List.length(xs) as a Nat */
static Nat length_of(Heap *heap, const Vec *xs) { return nat_of_u64(heap->arena, xs->len); }

/* [Binding{index, repr}] */
static Vec binding_list(Heap *heap, uint32_t index, Repr repr) {
  Vec out = vec_new(heap->arena);
  EmitBinding *b = arena_alloc(heap->arena, sizeof *b);
  if (b == NULL) {
    heap->oom = true;
    return out;
  }
  b->index = index;
  b->repr = repr;
  if (!vec_push(&out, b)) heap->oom = true;
  return out;
}

/* Done{Reads} with the record boxed */
static Result done_reads(Heap *heap, EmitReads r) {
  EmitReads *box = arena_alloc(heap->arena, sizeof *box);
  if (box == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  *box = r;
  return done(box);
}

/* capture_env(l, n, rs, code): n = 0 -> [I32_const 0, Ref_i31]; else code ++ [Struct_new{type_index(l, env_tid(rs))}] */
Result emit_capture_env(Heap *heap, const Linked *l, Nat n, const Vec *rs, const Vec *code) {
  if (nat_is_zero(n)) return done_vec(heap, seq(heap, num(heap, 0), op(heap, GC_REF_I31), &stop));
  Result i = link_type_index(heap, l, link_env_tid(heap, rs));
  if (i.kind == RESULT_FAIL) return i;
  Vec last = seq(heap, at(heap, GC_STRUCT_NEW, index_of(i)), &stop);
  return done_vec(heap, emit_cat(heap, code, &last));
}

/* tag_payload(p, c, env, head, args): None -> head; Some{key, rs} -> head ++ each2(args, rs) ++ [Struct_new{key}] */
Result emit_tag_payload(Heap *heap, WasmState *st, const Pair *p, const EmitCtx *c, const Vec *env, const Vec *head,
                        const Vec *args) {
  if (p == NULL) return done_vec(heap, *head);
  Result li = link_type_index(heap, emit_layout(c), *(const Str *)p->first);
  if (li.kind == RESULT_FAIL) return li;
  Result code = emit_each2(heap, st, c, env, args, (const Vec *)p->second);
  if (code.kind == RESULT_FAIL) return code;
  Vec last = seq(heap, at(heap, GC_STRUCT_NEW, index_of(li)), &stop);
  return done_vec(heap, cat3(heap, head, (const Vec *)code.value, &last));
}

/* case_nonempty: no branches -> code ++ [Unreachable]; else case_term */
Result emit_case_nonempty(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Tid tid,
                          const Ktm *sc, const Vec *code, const Vec *bs) {
  if (bs->len == 0) {
    Vec last = seq(heap, op(heap, GC_UNREACHABLE), &stop);
    return done_vec(heap, emit_cat(heap, code, &last));
  }
  return emit_case_term(heap, st, c, env, tail, tid, sc, code, bs);
}

/* go, case E.KLet{name, v, b} */
static Result go_let(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *tm) {
  Result iv = emit_go(heap, st, c, env, false, tm->value);
  if (iv.kind == RESULT_FAIL) return iv;
  Result vr = emit_rep(heap, c, env, tm->value);
  if (vr.kind == RESULT_FAIL) return vr;
  Repr r = *(const Repr *)vr.value;
  Result vt = link_valtype_of(heap, emit_layout(c), r);
  if (vt.kind == RESULT_FAIL) return vt;
  Result idx = alloc_local(heap, st, *(const GcValtype *)vt.value);
  if (idx.kind == RESULT_FAIL) return idx;
  Vec bound = binding_list(heap, index_of(idx), r);
  Vec inner = emit_cat(heap, &bound, env);
  Result ib = emit_go(heap, st, c, &inner, tail, tm->body);
  if (ib.kind == RESULT_FAIL) return ib;
  Vec set = seq(heap, at(heap, GC_LOCAL_SET, index_of(idx)), &stop);
  return done_vec(heap, cat3(heap, (const Vec *)iv.value, &set, (const Vec *)ib.value));
}

/* go, case E.KClos{E.Fid{f}, n, caps} */
static Result go_clos(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm) {
  Nat count = length_of(heap, &tm->captures);
  Result rs = link_capture_reprs(heap, emit_prog(emit_layout(c)), tm->fid.name, count);
  if (rs.kind == RESULT_FAIL) return rs;
  Result ic = emit_each2(heap, st, c, env, &tm->captures, (const Vec *)rs.value);
  if (ic.kind == RESULT_FAIL) return ic;
  Result wi = link_func_index(heap, emit_layout(c), link_wrap_fkey(heap, tm->fid.name, count));
  if (wi.kind == RESULT_FAIL) return wi;
  Result ci = link_type_index(heap, emit_layout(c), str_lit("clos"));
  if (ci.kind == RESULT_FAIL) return ci;
  Result ie = emit_capture_env(heap, emit_layout(c), count, (const Vec *)rs.value, (const Vec *)ic.value);
  if (ie.kind == RESULT_FAIL) return ie;
  Vec head = seq(heap, num(heap, u32_of(tm->arity)), at(heap, GC_REF_FUNC, index_of(wi)), &stop);
  Vec last = seq(heap, at(heap, GC_STRUCT_NEW, index_of(ci)), &stop);
  return done_vec(heap, cat3(heap, &head, (const Vec *)ie.value, &last));
}

/* go, case E.KStruct{E.Tid{t}, fs} */
static Result go_struct(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm) {
  Result rs = link_fields_of(heap, tm->tid.name);
  if (rs.kind == RESULT_FAIL) return rs;
  Result ti = link_type_index(heap, emit_layout(c), tm->tid.name);
  if (ti.kind == RESULT_FAIL) return ti;
  Result code = emit_each2(heap, st, c, env, &tm->fields, (const Vec *)rs.value);
  if (code.kind == RESULT_FAIL) return code;
  Vec last = seq(heap, at(heap, GC_STRUCT_NEW, index_of(ti)), &stop);
  return done_vec(heap, emit_cat(heap, (const Vec *)code.value, &last));
}

/* go, case E.KProj{E.Tid{t}, k, x} */
static Result go_proj(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm) {
  Result code = emit_arg_at(heap, st, c, env, tm->value, eterm_rstruct(tm->tid));
  if (code.kind == RESULT_FAIL) return code;
  Result ti = link_type_index(heap, emit_layout(c), tm->tid.name);
  if (ti.kind == RESULT_FAIL) return ti;
  Result r = emit_rep(heap, c, env, tm);
  if (r.kind == RESULT_FAIL) return r;
  Result cz = emit_coerce(heap, emit_layout(c), link_any_repr(), *(const Repr *)r.value);
  if (cz.kind == RESULT_FAIL) return cz;
  Vec get = seq(heap, gc_instr_struct_get(heap, index_of(ti), u32_of(tm->index)), &stop);
  return done_vec(heap, cat3(heap, (const Vec *)code.value, &get, (const Vec *)cz.value));
}

/* go, case E.KTag{E.Tid{t}, k, ps} */
static Result go_tag(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, const Ktm *tm) {
  Result legs = link_sum_legs_p(heap, emit_prog(emit_layout(c)), tm->tid.name);
  if (legs.kind == RESULT_FAIL) return legs;
  VecGet nth = emit_nth((const Vec *)legs.value, tm->tag);
  Str leg_k = str_cat(heap->arena, str_lit("leg "), heap_nat_show(heap, tm->tag));
  Str msg = str_cat(heap->arena, str_cat(heap->arena, leg_k, str_lit(" of ")), tm->tid.name);
  Result leg = emit_optional(nth.status == VEC_GET_OK ? nth.value : NULL, error_make(ERROR_WRONG_LEG, msg));
  if (leg.kind == RESULT_FAIL) return leg;
  Vec head = seq(heap, num(heap, u32_of(tm->tag)), op(heap, GC_REF_I31), &stop);
  return emit_tag_payload(heap, st, link_leg_type(heap, *(const LinkLeg *)leg.value), c, env, &head, &tm->fields);
}

/* go, case E.KCase{tid, sc, bs} */
static Result go_case(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *tm) {
  Result code = emit_go(heap, st, c, env, false, tm->scrut);
  if (code.kind == RESULT_FAIL) return code;
  return emit_case_nonempty(heap, st, c, env, tail, tm->tid, tm->scrut, (const Vec *)code.value, &tm->branches);
}

/* go(c, env, tail, tm): the instructions that leave the value of tm on the stack */
Result emit_go(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, const Ktm *tm) {
  switch (tm->kind) {
    case KTM_VAR: {
      VecGet b = emit_nth(env, tm->index);
      return emit_var(heap, st, b.status == VEC_GET_OK ? (const EmitBinding *)b.value : NULL, tm->index);
    }
    case KTM_LIT:
      return emit_literal(heap, st, emit_layout(c), &tm->lit);
    case KTM_GLOBAL:
      return emit_global(heap, st, c, tm->name, tail);
    case KTM_ERASED:
      return done_vec(heap, seq(heap, num(heap, 0), op(heap, GC_REF_I31), &stop));
    case KTM_LET:
      return go_let(heap, st, c, env, tail, tm);
    case KTM_CLOS:
      return go_clos(heap, st, c, env, tm);
    case KTM_APP:
      return emit_apply(heap, st, c, env, false, tm->head, &tm->args);
    case KTM_TAIL:
      return emit_apply(heap, st, c, env, tail, tm->head, &tm->args);
    case KTM_STRUCT:
      return go_struct(heap, st, c, env, tm);
    case KTM_PROJ:
      return go_proj(heap, st, c, env, tm);
    case KTM_TAG:
      return go_tag(heap, st, c, env, tm);
    case KTM_CASE:
      return go_case(heap, st, c, env, tail, tm);
    case KTM_DELAY:
      return fail(error_make(ERROR_NOT_YET, str_lit("KDelay arrives at M2")));
    case KTM_FORCE:
      return fail(error_make(ERROR_NOT_YET, str_lit("KForce arrives at M2")));
  }
  return fail(error_host_oom());
}

/* add_leg_key(p, keys): None -> keys; Some{key, rs} -> key <> keys */
Vec emit_add_leg_key(Heap *heap, const Pair *p, const Vec *keys) {
  if (p == NULL) return *keys;
  Vec head = vec_new(heap->arena);
  if (!vec_push(&head, p->first)) heap->oom = true;
  return emit_cat(heap, &head, keys);
}

/* leg_keys(ls): the payload keys of the legs, in leg order (a right fold of add_leg_key) */
Vec emit_leg_keys(Heap *heap, const Vec *ls) {
  Vec keys = vec_new(heap->arena);
  for (size_t i = ls->len; i > 0; i--) {
    VecGet l = vec_get(ls, i - 1);
    if (l.status == VEC_GET_OUT_OF_RANGE) continue;
    keys = emit_add_leg_key(heap, link_leg_type(heap, *(const LinkLeg *)l.value), &keys);
  }
  return keys;
}

/* has_string(xs, x) */
bool emit_has_string(const Vec *xs, Str x) {
  for (size_t i = 0; i < xs->len; i++) {
    VecGet h = vec_get(xs, i);
    if (h.status == VEC_GET_OUT_OF_RANGE) continue;
    if (str_eq(*(const Str *)h.value, x)) return true;
  }
  return false;
}

/* unique(xs, seen): xs without the strings already in seen or earlier in xs, first occurrences in order */
Vec emit_unique(Heap *heap, const Vec *xs, const Vec *seen) {
  Vec out = vec_new(heap->arena);
  Vec known = emit_cat(heap, seen, &out);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet x = vec_get(xs, i);
    if (x.status == VEC_GET_OUT_OF_RANGE) continue;
    if (!emit_has_string(&known, *(const Str *)x.value) && !vec_push(&out, x.value)) heap->oom = true;
    if (!vec_push(&known, x.value)) heap->oom = true;
  }
  return out;
}

/* indices(l, keys): type_index of every key in order, the first failure wins */
Result emit_indices(Heap *heap, const Linked *l, const Vec *keys) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < keys->len; j++) {
    VecGet k = vec_get(keys, j);
    if (k.status == VEC_GET_OUT_OF_RANGE) continue;
    Result i = link_type_index(heap, l, *(const Str *)k.value);
    if (i.kind == RESULT_FAIL) return i;
    if (!vec_push(&out, (void *)i.value)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* cast_tags(shapes, depth): Br_on_cast{depth + j, HEq, HType{i_j}} per shape, then Unreachable */
Vec emit_cast_tags(Heap *heap, const Vec *shapes, uint32_t depth) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < shapes->len; j++) {
    VecGet i = vec_get(shapes, j);
    if (i.status == VEC_GET_OUT_OF_RANGE) continue;
    GcInstr *br = gc_instr_br_on_cast(heap, depth + (uint32_t)j, gc_heaptype(GC_HEAP_EQ, 0),
                                      gc_heaptype(GC_HEAP_TYPE, *(const uint32_t *)i.value));
    if (br == NULL || !vec_push(&out, br)) heap->oom = true;
  }
  GcInstr *end = op(heap, GC_UNREACHABLE);
  if (end == NULL || !vec_push(&out, end)) heap->oom = true;
  return out;
}

/* tag_layers(shapes, code): per shape i with n shapes after it,
 * code := [Block{Some{Ref{HType{i}}}, code}, Struct_get{i, 0}, I31_get_u] ++ (n = 0 ? [] : [Br{n}]) */
Vec emit_tag_layers(Heap *heap, const Vec *shapes, const Vec *code) {
  Vec acc = *code;
  for (size_t j = 0; j < shapes->len; j++) {
    VecGet i = vec_get(shapes, j);
    if (i.status == VEC_GET_OUT_OF_RANGE) continue;
    uint32_t ix = *(const uint32_t *)i.value;
    uint32_t n = (uint32_t)(shapes->len - j - 1);
    Vec post = n == 0 ? vec_new(heap->arena) : seq(heap, at(heap, GC_BR, n), &stop);
    GcMaybeValtype bt = gc_valtype_some(gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, ix)));
    Vec layer = seq(heap, gc_instr_block(heap, bt, acc), gc_instr_struct_get(heap, ix, 0), op(heap, GC_I31_GET_U), &stop);
    acc = emit_cat(heap, &layer, &post);
  }
  return acc;
}

/* tag_block(sv, shapes): the I32 block that leaves the tag of the scrutinee in local sv */
GcInstr *emit_tag_block(Heap *heap, uint32_t sv, const Vec *shapes) {
  uint32_t p = (uint32_t)shapes->len;
  GcInstr *to_i31 = gc_instr_br_on_cast(heap, 0, gc_heaptype(GC_HEAP_EQ, 0), gc_heaptype(GC_HEAP_I31, 0));
  Vec head = seq(heap, at(heap, GC_LOCAL_GET, sv), to_i31, &stop);
  Vec casts = emit_cast_tags(heap, shapes, 1);
  Vec core = emit_cat(heap, &head, &casts);
  GcMaybeValtype bt = gc_valtype_some(gc_valtype_ref(gc_heaptype(GC_HEAP_I31, 0)));
  Vec first = seq(heap, gc_instr_block(heap, bt, core), op(heap, GC_I31_GET_U), &stop);
  Vec post = p == 0 ? vec_new(heap->arena) : seq(heap, at(heap, GC_BR, p), &stop);
  Vec start = emit_cat(heap, &first, &post);
  return gc_instr_block(heap, gc_valtype_some(gc_valtype_i32()), emit_tag_layers(heap, shapes, &start));
}

/* reads_code(r), reads_env(r) */
Vec emit_reads_code(EmitReads r) { return r.code; }

Vec emit_reads_env(EmitReads r) { return r.env; }

/* leg_reads(c, sv, li, i, rs): per field r, a fresh local set from Struct_get{li, i + 1} of the cast scrutinee;
 * Done{EmitReads *}, env in reverse field order */
Result emit_leg_reads(Heap *heap, WasmState *st, const EmitCtx *c, uint32_t sv, uint32_t li, uint32_t i,
                      const Vec *rs) {
  if (rs->len == 0) {
    EmitReads none = {vec_new(heap->arena), vec_new(heap->arena)};
    return done_reads(heap, none);
  }
  VecGet head = vec_get(rs, 0);
  if (head.status == VEC_GET_OUT_OF_RANGE) return fail(error_host_oom());
  Repr r = *(const Repr *)head.value;
  Result vt = link_valtype_of(heap, emit_layout(c), r);
  if (vt.kind == RESULT_FAIL) return vt;
  Result idx = alloc_local(heap, st, *(const GcValtype *)vt.value);
  if (idx.kind == RESULT_FAIL) return idx;
  Result cz = emit_coerce(heap, emit_layout(c), link_any_repr(), r);
  if (cz.kind == RESULT_FAIL) return cz;
  Vec rest = drop_vec(heap, rs, 1);
  Result more = emit_leg_reads(heap, st, c, sv, li, i + 1, &rest);
  if (more.kind == RESULT_FAIL) return more;
  EmitReads m = *(const EmitReads *)more.value;
  Vec get = seq(heap, at(heap, GC_LOCAL_GET, sv), gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, li)),
                gc_instr_struct_get(heap, li, i + 1), &stop);
  Vec set = seq(heap, at(heap, GC_LOCAL_SET, index_of(idx)), &stop);
  Vec more_code = emit_reads_code(m);
  Vec more_env = emit_reads_env(m);
  Vec bound = binding_list(heap, index_of(idx), r);
  EmitReads out = {cat4(heap, &get, (const Vec *)cz.value, &set, &more_code), emit_cat(heap, &more_env, &bound)};
  return done_reads(heap, out);
}

/* branch_code(c, env, tail, cr, body) = with_coercion(go(c, env, tail, body), coerce(l, rep(body), cr)) */
Result emit_branch_code(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr cr,
                        const Ktm *body) {
  Result ib = emit_go(heap, st, c, env, tail, body);
  if (ib.kind == RESULT_FAIL) return ib;
  Result br = emit_rep(heap, c, env, body);
  if (br.kind == RESULT_FAIL) return br;
  Result cz = emit_coerce(heap, emit_layout(c), *(const Repr *)br.value, cr);
  if (cz.kind == RESULT_FAIL) return cz;
  return done_vec(heap, emit_with_coercion(heap, (const Vec *)ib.value, (const Vec *)cz.value));
}

/* branch_payload(Pair{key, rs}, c, env, tail, sv, cr, body) = reads code ++ branch_code(reads env ++ env) */
Result emit_branch_payload(Heap *heap, WasmState *st, const Pair *p, const EmitCtx *c, const Vec *env, bool tail,
                           uint32_t sv, Repr cr, const Ktm *body) {
  Result li = link_type_index(heap, emit_layout(c), *(const Str *)p->first);
  if (li.kind == RESULT_FAIL) return li;
  Result reads = emit_leg_reads(heap, st, c, sv, index_of(li), 0, (const Vec *)p->second);
  if (reads.kind == RESULT_FAIL) return reads;
  EmitReads rd = *(const EmitReads *)reads.value;
  Vec read_env = emit_reads_env(rd);
  Vec inner = emit_cat(heap, &read_env, env);
  Result ib = emit_branch_code(heap, st, c, &inner, tail, cr, body);
  if (ib.kind == RESULT_FAIL) return ib;
  Vec read_code = emit_reads_code(rd);
  return done_vec(heap, emit_cat(heap, &read_code, (const Vec *)ib.value));
}

/* branch_bindings(rs, c, env, tail, sr, sv, cr, b): no binders -> branch_code; else the payload of leg k */
Result emit_branch_bindings(Heap *heap, WasmState *st, const Vec *rs, const EmitCtx *c, const Vec *env, bool tail,
                            Repr sr, uint32_t sv, Repr cr, const KtmBranch *b) {
  if (rs->len == 0) return emit_branch_code(heap, st, c, env, tail, cr, b->body);
  Result leg = link_branch_tid(heap, link_tid_text(sr), emit_prog(emit_layout(c)), b->tag);
  if (leg.kind == RESULT_FAIL) return leg;
  Result p = emit_optional(link_leg_type(heap, *(const LinkLeg *)leg.value),
                           error_make(ERROR_MISMATCH, str_lit("a branch binds a field of a payload free leg")));
  if (p.kind == RESULT_FAIL) return p;
  return emit_branch_payload(heap, st, (const Pair *)p.value, c, env, tail, sv, cr, b->body);
}

/* branches(...): [Local_get tg, I32_const k, I32_eq, If{Some{cvt}, branch, the other branches}], Unreachable last */
Result emit_branches(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Repr sr, uint32_t sv,
                     GcValtype cvt, Repr cr, uint32_t tg, const Vec *bs) {
  if (bs->len == 0) return done_vec(heap, seq(heap, op(heap, GC_UNREACHABLE), &stop));
  VecGet first = vec_get(bs, 0);
  if (first.status == VEC_GET_OUT_OF_RANGE) return fail(error_host_oom());
  const KtmBranch *b = (const KtmBranch *)first.value;
  Result rs = link_branch_binders(heap, emit_prog(emit_layout(c)), sr, *b);
  if (rs.kind == RESULT_FAIL) return rs;
  Result ib = emit_branch_bindings(heap, st, (const Vec *)rs.value, c, env, tail, sr, sv, cr, b);
  if (ib.kind == RESULT_FAIL) return ib;
  Vec rest = drop_vec(heap, bs, 1);
  Result others = emit_branches(heap, st, c, env, tail, sr, sv, cvt, cr, tg, &rest);
  if (others.kind == RESULT_FAIL) return others;
  GcInstr *pick = gc_instr_if(heap, gc_valtype_some(cvt), *(const Vec *)ib.value, *(const Vec *)others.value);
  return done_vec(heap, seq(heap, at(heap, GC_LOCAL_GET, tg), num(heap, u32_of(b->tag)), op(heap, GC_I32_EQ), pick,
                            &stop));
}

/* case_term(c, env, tail, tid, sc, code, bs): store the scrutinee, compute its tag, dispatch on the branches */
Result emit_case_term(Heap *heap, WasmState *st, const EmitCtx *c, const Vec *env, bool tail, Tid tid, const Ktm *sc,
                      const Vec *code, const Vec *bs) {
  LinkMaybeStr text = link_tid_text(eterm_runion(tid));
  Result stid =
      emit_optional(text.some ? &text.value : NULL, error_make(ERROR_MISMATCH, str_lit("a case scrutinee has no tid")));
  if (stid.kind == RESULT_FAIL) return stid;
  Result legs = link_sum_legs_p(heap, emit_prog(emit_layout(c)), *(const Str *)stid.value);
  if (legs.kind == RESULT_FAIL) return legs;
  Vec keys = emit_leg_keys(heap, (const Vec *)legs.value);
  Vec none = vec_new(heap->arena);
  Vec distinct = emit_unique(heap, &keys, &none);
  Result shapes = emit_indices(heap, emit_layout(c), &distinct);
  if (shapes.kind == RESULT_FAIL) return shapes;
  Result sv = alloc_local(heap, st, gc_valtype_ref(gc_heaptype(GC_HEAP_EQ, 0)));
  if (sv.kind == RESULT_FAIL) return sv;
  Result tg = alloc_local(heap, st, gc_valtype_i32());
  if (tg.kind == RESULT_FAIL) return tg;
  Result cr = emit_rep(heap, c, env, eterm_kcase(heap, tid, sc, *bs));
  if (cr.kind == RESULT_FAIL) return cr;
  Repr crr = *(const Repr *)cr.value;
  Result cvt = link_valtype_of(heap, emit_layout(c), crr);
  if (cvt.kind == RESULT_FAIL) return cvt;
  Result dis = emit_branches(heap, st, c, env, tail, eterm_runion(tid), index_of(sv), *(const GcValtype *)cvt.value,
                             crr, index_of(tg), bs);
  if (dis.kind == RESULT_FAIL) return dis;
  Vec mid = seq(heap, at(heap, GC_LOCAL_SET, index_of(sv)), emit_tag_block(heap, index_of(sv), (const Vec *)shapes.value),
                at(heap, GC_LOCAL_SET, index_of(tg)), &stop);
  return done_vec(heap, cat3(heap, code, &mid, (const Vec *)dis.value));
}
/* Nat.sub(a, b), truncated at zero (as link.c's link_nat_sub) */
static Nat nat_minus(Heap *heap, Nat a, Nat b) {
  NatSub d = nat_sub(heap->arena, a, b);
  if (d.status == NAT_SUB_OOM) heap->oom = true;
  return d.status == NAT_SUB_OK ? d.value : nat_of_u64(heap->arena, 0);
}
/* A Nat as a loop count (SIZE_MAX when it does not fit: no list is that long) */
static size_t count_of(Nat n) {
  NatToU64 v = nat_to_u64(n);
  return v.status == NAT_TO_U64_OK ? (size_t)v.value : SIZE_MAX;
}
/* List.reverse(xs) */
static Vec reverse_vec(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = xs->len; j > 0; j--) {
    VecGet x = vec_get(xs, j - 1);
    if (x.status != VEC_GET_OK || !vec_push(&out, x.value)) heap->oom = true;
  }
  return out;
}
/* G.Ref{G.HType{index}} */
static GcValtype ref_type(uint32_t index) { return gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, index)); }
/* G.Ref_cast{G.HType{index}} */
static GcInstr *cast_type(Heap *heap, uint32_t index) {
  return gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, index));
}
/* G.Struct_get{index, f} */
static GcInstr *field(Heap *heap, uint32_t index, uint32_t f) { return gc_instr_struct_get(heap, index, f); }
/* the locals list [a, b] */
static Vec locals2(Heap *heap, GcValtype a, GcValtype b) {
  Vec out = vec_new(heap->arena);
  GcValtype *x = gc_valtype_box(heap, a);
  GcValtype *y = gc_valtype_box(heap, b);
  if (x == NULL || y == NULL || !vec_push(&out, x) || !vec_push(&out, y)) heap->oom = true;
  return out;
}
/* Done{G.Func}, or the oom failure when gc_func could not allocate */
static Result done_func(Heap *heap, GcFunc *f) {
  if (f == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  return done(f);
}
/* Done{bytes} with the Str boxed */
static Result done_str(Heap *heap, Str s) {
  Str *box = arena_alloc(heap->arena, sizeof(Str));
  if (box == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  *box = s;
  return done(box);
}
/* the oom failure for a list item that is NULL */
static Result missing_item(Heap *heap) {
  heap->oom = true;
  return fail(error_host_oom());
}
/* Unit{} */
static int unit_value;

/* param_bindings(ps, i): Binding{i, p} per parameter, the index counting up */
Vec emit_param_bindings(Heap *heap, const Vec *ps, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ps->len; j++) {
    VecGet p = vec_get(ps, j);
    EmitBinding *b = arena_alloc(heap->arena, sizeof(EmitBinding));
    if (p.status != VEC_GET_OK || p.value == NULL || b == NULL) {
      heap->oom = true;
    } else {
      b->index = i + (uint32_t)j;
      b->repr = *(const Repr *)p.value;
      if (!vec_push(&out, b)) heap->oom = true;
    }
  }
  return out;
}

/* param_env(ps) = List.reverse(param_bindings(ps, 0)) */
Vec emit_param_env(Heap *heap, const Vec *ps) {
  Vec bs = emit_param_bindings(heap, ps, 0);
  return reverse_vec(heap, &bs);
}

/* prog_func(l, f): the WasmGC function of the program function f */
Result emit_prog_func(Heap *heap, const Linked *l, const LinkFn *f) {
  Result ft = link_type_index(heap, l, link_sig_key(heap, &f->params, f->result));
  if (ft.kind == RESULT_FAIL) return ft;
  Vec rev = reverse_vec(heap, &f->params);
  Result br = link_infer(heap, &l->prog, &rev, f->body);
  if (br.kind == RESULT_FAIL) return br;
  Result cz = emit_coerce(heap, l, *(const Repr *)br.value, f->result);
  if (cz.kind == RESULT_FAIL) return cz;
  EmitCtx c = {l, f->result};
  Vec env = emit_param_env(heap, &f->params);
  WasmState st = {length_of(heap, &f->params), vec_new(heap->arena)};
  Result code = emit_go(heap, &st, &c, &env, true, f->body);
  Result action = code.kind == RESULT_FAIL
                      ? code
                      : done_vec(heap, emit_with_coercion(heap, (const Vec *)code.value, (const Vec *)cz.value));
  WasmStep step = {st, action};
  return state_finish(heap, index_of(ft), step);
}

/* wrapper_args(ps, l, i): each parameter read from local i on, coerced from any_repr */
Result emit_wrapper_args(Heap *heap, const Vec *ps, const Linked *l, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ps->len; j++) {
    VecGet p = vec_get(ps, j);
    if (p.status != VEC_GET_OK || p.value == NULL) return missing_item(heap);
    Result cz = emit_coerce(heap, l, link_any_repr(), *(const Repr *)p.value);
    if (cz.kind == RESULT_FAIL) return cz;
    Vec get = seq(heap, at(heap, GC_LOCAL_GET, i + (uint32_t)j), &stop);
    out = cat3(heap, &out, &get, (const Vec *)cz.value);
  }
  return done_vec(heap, out);
}

/* wrapper_captures(rs, l, ei, i): each capture read from field i on of the env struct ei */
Result emit_wrapper_captures(Heap *heap, const Vec *rs, const Linked *l, uint32_t ei, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < rs->len; j++) {
    VecGet r = vec_get(rs, j);
    if (r.status != VEC_GET_OK || r.value == NULL) return missing_item(heap);
    Result cz = emit_coerce(heap, l, link_any_repr(), *(const Repr *)r.value);
    if (cz.kind == RESULT_FAIL) return cz;
    Vec get = seq(heap, at(heap, GC_LOCAL_GET, 0), cast_type(heap, ei), field(heap, ei, i + (uint32_t)j), &stop);
    out = cat3(heap, &out, &get, (const Vec *)cz.value);
  }
  return done_vec(heap, out);
}

/* wrapper_env(n, l, rs): no reads at 0 captures, else the captures of the env struct */
Result emit_wrapper_env(Heap *heap, Nat n, const Linked *l, const Vec *rs) {
  if (nat_is_zero(n)) return done_empty(heap);
  Result ei = link_type_index(heap, l, link_env_tid(heap, rs));
  if (ei.kind == RESULT_FAIL) return ei;
  return emit_wrapper_captures(heap, rs, l, index_of(ei), 0);
}

/* the HFun arm of wrapper_kind */
static Result wrapper_fun(Heap *heap, const LinkFn *fn, const Linked *l, Str name, Nat caps) {
  if (fn == NULL) return missing_item(heap);
  Result ft = link_type_index(heap, l, link_fn_key(heap, nat_minus(heap, length_of(heap, &fn->params), caps)));
  if (ft.kind == RESULT_FAIL) return ft;
  Result fi = link_func_index(heap, l, str_cat(heap->arena, str_lit("fun:"), name));
  if (fi.kind == RESULT_FAIL) return fi;
  Result rs = link_capture_reprs(heap, &l->prog, name, caps);
  if (rs.kind == RESULT_FAIL) return rs;
  Result cap_ins = emit_wrapper_env(heap, caps, l, (const Vec *)rs.value);
  if (cap_ins.kind == RESULT_FAIL) return cap_ins;
  Vec rest = drop_vec(heap, &fn->params, count_of(caps));
  Result arg_ins = emit_wrapper_args(heap, &rest, l, 1);
  if (arg_ins.kind == RESULT_FAIL) return arg_ins;
  Vec call = seq(heap, at(heap, GC_RETURN_CALL, index_of(fi)), &stop);
  Vec body = cat3(heap, (const Vec *)cap_ins.value, (const Vec *)arg_ins.value, &call);
  return done_func(heap, gc_func(heap, index_of(ft), vec_new(heap->arena), body));
}

/* the HPrim arm of wrapper_kind */
static Result wrapper_prim(Heap *heap, const Linked *l, Str n) {
  Result ft = link_type_index(heap, l, link_fn_key(heap, nat_of_u64(heap->arena, 2)));
  if (ft.kind == RESULT_FAIL) return ft;
  Result fi = link_func_index(heap, l, str_cat(heap->arena, str_lit("nat-runtime:"), n));
  if (fi.kind == RESULT_FAIL) return fi;
  Vec body = seq(heap, at(heap, GC_LOCAL_GET, 1), at(heap, GC_LOCAL_GET, 2), at(heap, GC_RETURN_CALL, index_of(fi)),
                 &stop);
  return done_func(heap, gc_func(heap, index_of(ft), vec_new(heap->arena), body));
}

/* wrapper_kind(k, l, name, caps) */
Result emit_wrapper_kind(Heap *heap, LinkHead k, const Linked *l, Str name, Nat caps) {
  switch (k.kind) {
  case LINK_HEAD_FUN:
    return wrapper_fun(heap, k.fn, l, name, caps);
  case LINK_HEAD_PRIM:
    return wrapper_prim(heap, l, k.name);
  case LINK_HEAD_VALUE:
    return fail(error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("no runtime value for "), name)));
  }
  return fail(error_make(ERROR_MISMATCH, str_lit("unknown head kind")));
}

/* wrapper(l, name, caps) = wrapper_kind(L.head_kind(prog(l), E.KGlobal{name}), l, name, caps) */
Result emit_wrapper(Heap *heap, const Linked *l, Str name, Nat caps) {
  return emit_wrapper_kind(heap, link_head_kind(&l->prog, eterm_kglobal(heap, name)), l, name, caps);
}

/* gets(n, start): Local_get of start .. start + n - 1 */
Vec emit_gets(Heap *heap, Nat n, uint32_t start) {
  Vec out = vec_new(heap->arena);
  size_t count = count_of(n);
  bool ok = true;
  for (size_t j = 0; j < count && ok; j++) {
    GcInstr *g = at(heap, GC_LOCAL_GET, start + (uint32_t)j);
    ok = g != NULL && vec_push(&out, g);
  }
  if (!ok) heap->oom = true;
  return out;
}

/* [Local_get{cl}, Struct_get{ci, 2}]: the env of the closure in local cl */
static Vec clos_env(Heap *heap, uint32_t cl, uint32_t ci) {
  return seq(heap, at(heap, GC_LOCAL_GET, cl), field(heap, ci, 2), &stop);
}

/* apply_arm_kind(order, l, k, m, cl, ci, fm): k arguments against a closure of arity m */
Result emit_apply_arm_kind(Heap *heap, Cmp order, const Linked *l, Nat k, Nat m, uint32_t cl, uint32_t ci,
                           uint32_t fm) {
  switch (order) {
  case CMP_EQ: {
    Vec env = clos_env(heap, cl, ci);
    Vec args = emit_gets(heap, k, 1);
    Vec call = seq(heap, at(heap, GC_LOCAL_GET, cl), field(heap, ci, 1), cast_type(heap, fm),
                   at(heap, GC_RETURN_CALL_REF, fm), &stop);
    return done_vec(heap, cat3(heap, &env, &args, &call));
  }
  case CMP_LT: {
    Nat over = nat_minus(heap, k, m);
    Result rest = link_func_index(heap, l, link_apply_fkey(heap, over));
    if (rest.kind == RESULT_FAIL) return rest;
    Vec env = clos_env(heap, cl, ci);
    Vec args = emit_gets(heap, m, 1);
    Vec call = seq(heap, at(heap, GC_LOCAL_GET, cl), field(heap, ci, 1), cast_type(heap, fm),
                   at(heap, GC_CALL_REF, fm), &stop);
    Vec more = emit_gets(heap, over, u32_of(heap_nat_succ(heap, m)));
    Vec tail = seq(heap, at(heap, GC_RETURN_CALL, index_of(rest)), &stop);
    Vec head = cat3(heap, &env, &args, &call);
    return done_vec(heap, cat3(heap, &head, &more, &tail));
  }
  case CMP_GT: {
    Result pi = link_type_index(heap, l, link_pap_key(heap, m, k));
    if (pi.kind == RESULT_FAIL) return pi;
    Result fw = link_func_index(heap, l, link_papw_fkey(heap, m, k));
    if (fw.kind == RESULT_FAIL) return fw;
    Vec pre = seq(heap, num(heap, u32_of(nat_minus(heap, m, k))), at(heap, GC_REF_FUNC, index_of(fw)),
                  at(heap, GC_LOCAL_GET, 0), &stop);
    Vec args = emit_gets(heap, k, 1);
    Vec post = seq(heap, at(heap, GC_STRUCT_NEW, index_of(pi)), at(heap, GC_STRUCT_NEW, ci), &stop);
    return done_vec(heap, cat3(heap, &pre, &args, &post));
  }
  }
  return fail(error_make(ERROR_MISMATCH, str_lit("unknown order")));
}

/* apply_arm(l, k, m, cl, ci) = apply_arm_kind(Nat.cmp(m, k), ..) under fm = type_index(fn_key(m)) */
Result emit_apply_arm(Heap *heap, const Linked *l, Nat k, Nat m, uint32_t cl, uint32_t ci) {
  Result fm = link_type_index(heap, l, link_fn_key(heap, m));
  if (fm.kind == RESULT_FAIL) return fm;
  int o = nat_cmp(m, k);
  Cmp order = o < 0 ? CMP_LT : (o == 0 ? CMP_EQ : CMP_GT);
  return emit_apply_arm_kind(heap, order, l, k, m, cl, ci, index_of(fm));
}

/* apply_chain from item j: an If per arity, Unreachable after the last */
static Result chain_from(Heap *heap, const Vec *ms, size_t j, const Linked *l, Nat k, uint32_t cl, uint32_t ci,
                         uint32_t ar) {
  if (j >= ms->len) return done_vec(heap, seq(heap, op(heap, GC_UNREACHABLE), &stop));
  VecGet mv = vec_get(ms, j);
  if (mv.status != VEC_GET_OK || mv.value == NULL) return missing_item(heap);
  Nat m = *(const Nat *)mv.value;
  Result b = emit_apply_arm(heap, l, k, m, cl, ci);
  if (b.kind == RESULT_FAIL) return b;
  Result more = chain_from(heap, ms, j + 1, l, k, cl, ci, ar);
  if (more.kind == RESULT_FAIL) return more;
  GcInstr *pick = gc_instr_if(heap, gc_valtype_some(gc_valtype_ref(gc_heaptype(GC_HEAP_EQ, 0))),
                              *(const Vec *)b.value, *(const Vec *)more.value);
  return done_vec(heap, seq(heap, at(heap, GC_LOCAL_GET, ar), num(heap, u32_of(m)), op(heap, GC_I32_EQ), pick, &stop));
}

/* apply_chain(ms, l, k, cl, ci, ar) */
Result emit_apply_chain(Heap *heap, const Vec *ms, const Linked *l, Nat k, uint32_t cl, uint32_t ci, uint32_t ar) {
  return chain_from(heap, ms, 0, l, k, cl, ci, ar);
}

/* arities(l): the closure arities of the layout */
const Vec *emit_arities(const Linked *l) { return &l->arities; }

/* apply_func(l, k): apply k arguments to a closure of any arity */
Result emit_apply_func(Heap *heap, const Linked *l, Nat k) {
  uint32_t cl = u32_of(heap_nat_succ(heap, k));
  uint32_t ar = u32_of(heap_nat_succ(heap, heap_nat_succ(heap, k)));
  Result ft = link_type_index(heap, l, link_apply_ty_key(heap, k));
  if (ft.kind == RESULT_FAIL) return ft;
  Result cr = link_type_index(heap, l, str_lit("clos"));
  if (cr.kind == RESULT_FAIL) return cr;
  uint32_t ci = index_of(cr);
  Result chain = emit_apply_chain(heap, emit_arities(l), l, k, cl, ci, ar);
  if (chain.kind == RESULT_FAIL) return chain;
  Vec pre = seq(heap, at(heap, GC_LOCAL_GET, 0), cast_type(heap, ci), at(heap, GC_LOCAL_SET, cl),
                at(heap, GC_LOCAL_GET, cl), field(heap, ci, 0), at(heap, GC_LOCAL_SET, ar), &stop);
  Vec locals = locals2(heap, ref_type(ci), gc_valtype_i32());
  return done_func(heap, gc_func(heap, index_of(ft), locals, emit_cat(heap, &pre, (const Vec *)chain.value)));
}

/* saved_args(n, pv, pi, i): fields i .. i + n - 1 of the pap struct in local pv */
Vec emit_saved_args(Heap *heap, Nat n, uint32_t pv, uint32_t pi, uint32_t i) {
  Vec out = vec_new(heap->arena);
  size_t count = count_of(n);
  bool ok = true;
  for (size_t j = 0; j < count && ok; j++) {
    Vec two = seq(heap, at(heap, GC_LOCAL_GET, pv), field(heap, pi, i + (uint32_t)j), &stop);
    ok = two.len == 2;
    out = emit_cat(heap, &out, &two);
  }
  if (!ok) heap->oom = true;
  return out;
}

/* papw_func(l, m, k): the wrapper of a closure of arity m with k saved arguments */
Result emit_papw_func(Heap *heap, const Linked *l, Nat m, Nat k) {
  Nat n = nat_minus(heap, m, k);
  uint32_t pv = u32_of(heap_nat_succ(heap, n));
  uint32_t cv = u32_of(heap_nat_succ(heap, heap_nat_succ(heap, n)));
  Result ft = link_type_index(heap, l, link_fn_key(heap, n));
  if (ft.kind == RESULT_FAIL) return ft;
  Result cr = link_type_index(heap, l, str_lit("clos"));
  if (cr.kind == RESULT_FAIL) return cr;
  Result pr = link_type_index(heap, l, link_pap_key(heap, m, k));
  if (pr.kind == RESULT_FAIL) return pr;
  Result fr = link_type_index(heap, l, link_fn_key(heap, m));
  if (fr.kind == RESULT_FAIL) return fr;
  uint32_t ci = index_of(cr);
  uint32_t pi = index_of(pr);
  uint32_t fm = index_of(fr);
  Vec pre = seq(heap, at(heap, GC_LOCAL_GET, 0), cast_type(heap, pi), at(heap, GC_LOCAL_SET, pv),
                at(heap, GC_LOCAL_GET, pv), field(heap, pi, 0), cast_type(heap, ci), at(heap, GC_LOCAL_SET, cv),
                at(heap, GC_LOCAL_GET, cv), field(heap, ci, 2), &stop);
  Vec saved = emit_saved_args(heap, k, pv, pi, 1);
  Vec args = emit_gets(heap, n, 1);
  Vec call = seq(heap, at(heap, GC_LOCAL_GET, cv), field(heap, ci, 1), cast_type(heap, fm),
                 at(heap, GC_RETURN_CALL_REF, fm), &stop);
  Vec locals = locals2(heap, ref_type(pi), ref_type(ci));
  return done_func(heap, gc_func(heap, index_of(ft), locals, cat4(heap, &pre, &saved, &args, &call)));
}

/* entry_func(l, export): the i32 entry over fun:export */
Result emit_entry_func(Heap *heap, const Linked *l, Str export) {
  Result ft = link_type_index(heap, l, str_lit("entryfn"));
  if (ft.kind == RESULT_FAIL) return ft;
  Result fi = link_func_index(heap, l, str_cat(heap->arena, str_lit("fun:"), export));
  if (fi.kind == RESULT_FAIL) return fi;
  Vec body = seq(heap, at(heap, GC_CALL, index_of(fi)), gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_I31, 0)),
                 op(heap, GC_I31_GET_S), &stop);
  return done_func(heap, gc_func(heap, index_of(ft), vec_new(heap->arena), body));
}

/* one_func(l, export, spec) */
Result emit_one_func(Heap *heap, const Linked *l, Str export, const Fspec *spec) {
  switch (spec->kind) {
  case FSPEC_RUNTIME:
    return nat_runtime_runtime_func(heap, l, spec->name);
  case FSPEC_PROG: {
    Vec funs = link_prog_funcs(&l->prog);
    Error e = error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("no function named "), spec->name));
    Result f = emit_optional(link_lookup(&funs, spec->name), e);
    if (f.kind == RESULT_FAIL) return f;
    return emit_prog_func(heap, l, (const LinkFn *)f.value);
  }
  case FSPEC_WRAP:
    return emit_wrapper(heap, l, spec->name, spec->captures);
  case FSPEC_APPLY:
    return emit_apply_func(heap, l, spec->arity);
  case FSPEC_PAPW:
    return emit_papw_func(heap, l, spec->arity, spec->applied);
  case FSPEC_ENTRY:
    return emit_entry_func(heap, l, export);
  }
  return fail(error_make(ERROR_MISMATCH, str_lit("unknown function spec")));
}

/* export_name(e): "" on None (NULL) */
Str emit_export_name(const Str *e) { return e == NULL ? str_empty() : *e; }

/* the FSEntry function of a module with no export: Unreachable */
static Result entry_stub(Heap *heap, const Linked *l) {
  Result ft = link_type_index(heap, l, str_lit("entryfn"));
  if (ft.kind == RESULT_FAIL) return ft;
  Vec body = seq(heap, op(heap, GC_UNREACHABLE), &stop);
  return done_func(heap, gc_func(heap, index_of(ft), vec_new(heap->arena), body));
}

/* funcs(fs, l, export): one function per Fspec row, in order (first Fail wins) */
Result emit_funcs(Heap *heap, const Vec *fs, const Linked *l, const Str *export) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < fs->len; j++) {
    VecGet row = vec_get(fs, j);
    const Pair *p = row.status == VEC_GET_OK ? row.value : NULL;
    const Fspec *s = p == NULL ? NULL : p->second;
    if (s == NULL) return missing_item(heap);
    Result f = s->kind == FSPEC_ENTRY && export == NULL ? entry_stub(heap, l)
                                                        : emit_one_func(heap, l, emit_export_name(export), s);
    if (f.kind == RESULT_FAIL) return f;
    if (!vec_push(&out, f.value)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* check_export(f, export): Done when export is a Nat definition of arity 0 */
Result emit_check_export(Heap *heap, const LinkFn *f, Str export) {
  if (f == NULL) return fail(error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("no definition named "), export)));
  bool nat0 = f->params.len == 0 && emit_same(heap, f->result, link_nat_repr());
  if (nat0) return done(&unit_value);
  return fail(error_make(ERROR_MISMATCH, str_cat(heap->arena, export, str_lit(" is not a Nat definition of arity 0"))));
}

/* program_linked(l, export): the encoded module bytes (Done value Str *) */
Result emit_program_linked(Heap *heap, const Linked *l, Str export) {
  Result types = link_comptype_groups(heap, l);
  if (types.kind == RESULT_FAIL) return types;
  Result functions = emit_funcs(heap, &l->funcs, l, &export);
  if (functions.kind == RESULT_FAIL) return functions;
  Result ei = link_func_index(heap, l, str_lit("entry"));
  if (ei.kind == RESULT_FAIL) return ei;
  Vec exports = vec_new(heap->arena);
  GcExport *e = gc_export(heap, export, index_of(ei));
  if (e == NULL || !vec_push(&exports, e)) heap->oom = true;
  GcModule *m = gc_module(heap, *(const Vec *)types.value, vec_new(heap->arena), *(const Vec *)functions.value,
                          exports, l->declared);
  if (m == NULL) return missing_item(heap);
  return done_str(heap, gc_encode(heap, m));
}

/* program(rows, export) */
Result emit_program(Heap *heap, const Vec *rows, Str export) {
  Prog p = link_program_table(heap, rows);
  Vec funs = link_prog_funcs(&p);
  Result u = emit_check_export(heap, link_lookup(&funs, export), export);
  if (u.kind == RESULT_FAIL) return u;
  Result l = link_build(heap, rows);
  if (l.kind == RESULT_FAIL) return l;
  return emit_program_linked(heap, (const Linked *)l.value, export);
}
/* Done{v} with the valtype boxed */
static Result done_valtype(Heap *heap, GcValtype v) {
  GcValtype *box = gc_valtype_box(heap, v);
  if (box == NULL) return missing_item(heap);
  return done(box);
}
/* Done{body} with the N.Body boxed */
static Result done_body(Heap *heap, NatBody b) {
  NatBody *box = arena_alloc(heap->arena, sizeof(NatBody));
  if (box == NULL) return missing_item(heap);
  *box = b;
  return done(box);
}
/* x <> rest */
static Vec cons_vec(Heap *heap, const void *x, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  if (!vec_push(&out, (void *)x)) heap->oom = true;
  return emit_cat(heap, &out, rest);
}
/* the Pair row at j of a C.Pair<String, L.Fn> list, NULL when absent */
static const Pair *row_at(const Vec *ds, size_t j) {
  VecGet x = vec_get(ds, j);
  return x.status == VEC_GET_OK ? x.value : NULL;
}
/* G.If{None{}, [G.Unreachable{}], []} */
static GcInstr *trap_if(Heap *heap) {
  return gc_instr_if(heap, gc_valtype_none(), seq(heap, op(heap, GC_UNREACHABLE), &stop), vec_new(heap->arena));
}

/* host_find(hs, r): the handle whose representation prints as r */
Result emit_host_find(Heap *heap, const Vec *hs, Repr r) {
  for (size_t j = 0; j < hs->len; j++) {
    VecGet x = vec_get(hs, j);
    const EmitHostHandle *h = x.status == VEC_GET_OK ? x.value : NULL;
    if (h == NULL) return missing_item(heap);
    if (emit_same(heap, h->repr, r)) return done((void *)h);
  }
  return fail(error_make(ERROR_MISMATCH,
                         str_cat(heap->arena, str_lit("unsupported host representation "), eterm_print_repr(heap, r))));
}

/* handle_type(h) = G.Ref{G.HType{index}} */
GcValtype emit_handle_type(const EmitHostHandle *h) { return ref_type(h->index); }

/* host_type(hs, r): I32 for the Nat representation, else the reference type of its handle */
Result emit_host_type(Heap *heap, const Vec *hs, Repr r) {
  if (emit_same(heap, r, link_nat_repr())) return done_valtype(heap, gc_valtype_i32());
  Result h = emit_host_find(heap, hs, r);
  if (h.kind == RESULT_FAIL) return h;
  return done_valtype(heap, emit_handle_type((const EmitHostHandle *)h.value));
}

/* host_types(rs, hs): host_type per representation, the first failure wins */
Result emit_host_types(Heap *heap, const Vec *rs, const Vec *hs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < rs->len; j++) {
    VecGet x = vec_get(rs, j);
    const Repr *r = x.status == VEC_GET_OK ? x.value : NULL;
    if (r == NULL) return missing_item(heap);
    Result t = emit_host_type(heap, hs, *r);
    if (t.kind == RESULT_FAIL) return t;
    if (!vec_push(&out, t.value)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* host_read(h, i) = [Local_get{i}, Ref_cast{HType{index}}, Struct_get{index, 0}] */
Vec emit_host_read(Heap *heap, const EmitHostHandle *h, uint32_t i) {
  return seq(heap, at(heap, GC_LOCAL_GET, i), cast_type(heap, h->index), field(heap, h->index, 0), &stop);
}

/* host_argument(hs, i, r): a Nat argument is range checked and boxed as i31, a handle argument is unwrapped */
Result emit_host_argument(Heap *heap, const Vec *hs, uint32_t i, Repr r) {
  if (emit_same(heap, r, link_nat_repr()))
    return done_vec(heap, seq(heap, at(heap, GC_LOCAL_GET, i), num(heap, 1073741823), op(heap, GC_I32_GT_U),
                              trap_if(heap), at(heap, GC_LOCAL_GET, i), op(heap, GC_REF_I31), &stop));
  Result h = emit_host_find(heap, hs, r);
  if (h.kind == RESULT_FAIL) return h;
  return done_vec(heap, emit_host_read(heap, (const EmitHostHandle *)h.value, i));
}

/* zeros(n) = n copies of I32_const{0} */
Vec emit_zeros(Heap *heap, Nat n) {
  Vec out = vec_new(heap->arena);
  size_t k = count_of(n);
  for (size_t j = 0; j < k && !heap->oom; j++) {
    GcInstr *z = num(heap, 0);
    if (z == NULL || !vec_push(&out, z)) heap->oom = true;
  }
  return out;
}

/* host_wrap(h) = Body{[], zeros(padding) ++ [Struct_new{index}]} */
NatBody emit_host_wrap(Heap *heap, const EmitHostHandle *h) {
  Vec z = emit_zeros(heap, h->padding);
  Vec s = seq(heap, at(heap, GC_STRUCT_NEW, h->index), &stop);
  NatBody b = {vec_new(heap->arena), emit_cat(heap, &z, &s)};
  return b;
}

/* host_result(hs, local, r): a Nat result is unboxed from i31 and range checked, a handle result is wrapped */
Result emit_host_result(Heap *heap, const Vec *hs, uint32_t local, Repr r) {
  if (emit_same(heap, r, link_nat_repr())) {
    GcValtype *i32 = gc_valtype_box(heap, gc_valtype_i32());
    Vec locals = vec_new(heap->arena);
    if (i32 == NULL || !vec_push(&locals, i32)) heap->oom = true;
    NatBody b = {locals, seq(heap, gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_I31, 0)), op(heap, GC_I31_GET_S),
                             at(heap, GC_LOCAL_SET, local), at(heap, GC_LOCAL_GET, local), num(heap, 1073741823),
                             op(heap, GC_I32_GT_U), trap_if(heap), at(heap, GC_LOCAL_GET, local), &stop)};
    return done_body(heap, b);
  }
  Result h = emit_host_find(heap, hs, r);
  if (h.kind == RESULT_FAIL) return h;
  return done_body(heap, emit_host_wrap(heap, (const EmitHostHandle *)h.value));
}

/* host_arguments(ps, hs, i): host_argument per parameter, the local index counting up from i */
Result emit_host_arguments(Heap *heap, const Vec *ps, const Vec *hs, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ps->len; j++) {
    VecGet x = vec_get(ps, j);
    const Repr *p = x.status == VEC_GET_OK ? x.value : NULL;
    if (p == NULL) return missing_item(heap);
    Result a = emit_host_argument(heap, hs, i + (uint32_t)j, *p);
    if (a.kind == RESULT_FAIL) return a;
    out = emit_cat(heap, &out, (const Vec *)a.value);
  }
  return done_vec(heap, out);
}

/* host_func(ft, fi, args, Body{ls, code}) = Func{ft, ls, args ++ [Call{fi}] ++ code} */
GcFunc *emit_host_func(Heap *heap, uint32_t ft, uint32_t fi, const Vec *args, const NatBody *body) {
  Vec call = seq(heap, at(heap, GC_CALL, fi), &stop);
  return gc_func(heap, ft, body->locals, cat3(heap, args, &call, &body->code));
}

/* host_wrapper(l, hs, ft, name, Fn{ps, r, b}): fun:<name> between the argument and the result conversions */
Result emit_host_wrapper(Heap *heap, const Linked *l, const Vec *hs, uint32_t ft, Str name, const LinkFn *f) {
  Result fi = link_func_index(heap, l, str_cat(heap->arena, str_lit("fun:"), name));
  if (fi.kind == RESULT_FAIL) return fi;
  Result args = emit_host_arguments(heap, &f->params, hs, 0);
  if (args.kind == RESULT_FAIL) return args;
  Result result = emit_host_result(heap, hs, (uint32_t)f->params.len, f->result);
  if (result.kind == RESULT_FAIL) return result;
  return done_func(heap,
                   emit_host_func(heap, ft, index_of(fi), (const Vec *)args.value, (const NatBody *)result.value));
}

/* host_definitions(ns, p): the C.Pair{name, Fn} row of every export name, the first missing name fails */
Result emit_host_definitions(Heap *heap, const Vec *ns, const Prog *p) {
  Vec funs = link_prog_funcs(p);
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ns->len; j++) {
    VecGet x = vec_get(ns, j);
    const Str *n = x.status == VEC_GET_OK ? x.value : NULL;
    if (n == NULL) return missing_item(heap);
    Error e = error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("no definition named "), *n));
    Result f = emit_optional(link_lookup(&funs, *n), e);
    if (f.kind == RESULT_FAIL) return f;
    Pair *row = link_row_new(heap, *n, f.value);
    if (row == NULL || !vec_push(&out, row)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* host_reprs(ds): the result then the parameter representations of every definition, in order */
Vec emit_host_reprs(Heap *heap, const Vec *ds) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ds->len; j++) {
    const Pair *d = row_at(ds, j);
    const LinkFn *f = d == NULL ? NULL : d->second;
    if (f == NULL) {
      heap->oom = true;
      continue;
    }
    if (!vec_push(&out, (void *)&f->result)) heap->oom = true;
    out = emit_cat(heap, &out, &f->params);
  }
  return out;
}

/* text_order((texts, order)) = order: the String.cmp result as a Cmp */
Cmp emit_text_order(int order) {
  if (order < 0) return CMP_LT;
  if (order > 0) return CMP_GT;
  return CMP_EQ;
}

/* repr_insert_order(order, x, y, rest): x before y on LT, x dropped on EQ, x inserted further on GT */
Vec emit_repr_insert_order(Heap *heap, Cmp order, const Repr *x, const Repr *y, const Vec *rest) {
  switch (order) {
  case CMP_EQ:
    return cons_vec(heap, y, rest);
  case CMP_LT: {
    Vec tail = cons_vec(heap, y, rest);
    return cons_vec(heap, x, &tail);
  }
  case CMP_GT: {
    Vec tail = emit_repr_insert(heap, rest, x);
    return cons_vec(heap, y, &tail);
  }
  }
  return cons_vec(heap, y, rest);
}

/* repr_insert(xs, x): x into the list sorted by printed representation, without duplicates */
Vec emit_repr_insert(Heap *heap, const Vec *xs, const Repr *x) {
  VecGet h = vec_get(xs, 0);
  const Repr *y = h.status == VEC_GET_OK ? h.value : NULL;
  if (y == NULL) {
    Vec none = vec_new(heap->arena);
    return cons_vec(heap, x, &none);
  }
  Vec rest = drop_vec(heap, xs, 1);
  Cmp order = emit_text_order(str_cmp(eterm_print_repr(heap, *x), eterm_print_repr(heap, *y)));
  return emit_repr_insert_order(heap, order, x, y, &rest);
}

/* repr_unique(xs) = repr_insert(repr_unique(rest), x): a right fold from the last element */
Vec emit_repr_unique(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = xs->len; j > 0; j--) {
    VecGet x = vec_get(xs, j - 1);
    const Repr *r = x.status == VEC_GET_OK ? x.value : NULL;
    if (r == NULL) {
      heap->oom = true;
      continue;
    }
    out = emit_repr_insert(heap, &out, r);
  }
  return out;
}

/* supported(rs): the representations other than Nat; a function or thunk representation fails */
Result emit_supported(Heap *heap, const Vec *rs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < rs->len; j++) {
    VecGet x = vec_get(rs, j);
    const Repr *r = x.status == VEC_GET_OK ? x.value : NULL;
    if (r == NULL) return missing_item(heap);
    switch (r->kind) {
    case REPR_FUNC:
    case REPR_THUNK:
      return fail(error_make(ERROR_NOT_YET, str_lit("host function and thunk values are not supported")));
    case REPR_I31:
    case REPR_STRUCT:
    case REPR_UNION:
      break;
    }
    if (!emit_same(heap, *r, link_nat_repr()) && !vec_push(&out, (void *)r)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* handles(rs, l, base, padding): HostHandle{r, base + padding, padding, valtype_of(r)}, padding counting up */
Result emit_handles(Heap *heap, const Vec *rs, const Linked *l, Nat base, Nat padding) {
  Vec out = vec_new(heap->arena);
  Nat p = padding;
  for (size_t j = 0; j < rs->len; j++) {
    VecGet x = vec_get(rs, j);
    const Repr *r = x.status == VEC_GET_OK ? x.value : NULL;
    if (r == NULL) return missing_item(heap);
    Result v = link_valtype_of(heap, l, *r);
    if (v.kind == RESULT_FAIL) return v;
    EmitHostHandle *h = arena_alloc(heap->arena, sizeof(EmitHostHandle));
    if (h == NULL) return missing_item(heap);
    h->repr = *r;
    h->index = u32_of(heap_nat_add(heap, base, p));
    h->padding = p;
    h->payload = *(const GcValtype *)v.value;
    if (!vec_push(&out, h)) heap->oom = true;
    p = heap_nat_succ(heap, p);
  }
  return done_vec(heap, out);
}

/* i32s(n) = n copies of I32 */
Vec emit_i32s(Heap *heap, Nat n) {
  Vec out = vec_new(heap->arena);
  size_t k = count_of(n);
  for (size_t j = 0; j < k && !heap->oom; j++) {
    GcValtype *t = gc_valtype_box(heap, gc_valtype_i32());
    if (t == NULL || !vec_push(&out, t)) heap->oom = true;
  }
  return out;
}

/* handle_types(hs): one rec group [CStruct{payload <> i32s(padding)}] per handle */
Vec emit_handle_types(Heap *heap, const Vec *hs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < hs->len; j++) {
    VecGet x = vec_get(hs, j);
    const EmitHostHandle *h = x.status == VEC_GET_OK ? x.value : NULL;
    if (h == NULL) {
      heap->oom = true;
      continue;
    }
    Vec head = vec_new(heap->arena);
    GcValtype *v = gc_valtype_box(heap, h->payload);
    if (v == NULL || !vec_push(&head, v)) heap->oom = true;
    Vec pad = emit_i32s(heap, h->padding);
    GcComptype *s = gc_comptype_struct(heap, emit_cat(heap, &head, &pad));
    Vec group = vec_new(heap->arena);
    if (s == NULL || !vec_push(&group, s)) heap->oom = true;
    Vec *g = gc_group_box(heap, group);
    if (g == NULL || !vec_push(&out, g)) heap->oom = true;
  }
  return out;
}

/* host_signatures(ds, hs): one rec group [CFunc{params, [result]}] per definition, the first failure wins */
Result emit_host_signatures(Heap *heap, const Vec *ds, const Vec *hs) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ds->len; j++) {
    const Pair *d = row_at(ds, j);
    const LinkFn *f = d == NULL ? NULL : d->second;
    if (f == NULL) return missing_item(heap);
    Result params = emit_host_types(heap, &f->params, hs);
    if (params.kind == RESULT_FAIL) return params;
    Result result = emit_host_type(heap, hs, f->result);
    if (result.kind == RESULT_FAIL) return result;
    Vec results = vec_new(heap->arena);
    if (!vec_push(&results, result.value)) heap->oom = true;
    GcComptype *t = gc_comptype_func(heap, *(const Vec *)params.value, results);
    Vec group = vec_new(heap->arena);
    if (t == NULL || !vec_push(&group, t)) heap->oom = true;
    Vec *g = gc_group_box(heap, group);
    if (g == NULL || !vec_push(&out, g)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* host_wrappers(ds, l, hs, ft): host_wrapper per definition, the type index counting up from ft */
Result emit_host_wrappers(Heap *heap, const Vec *ds, const Linked *l, const Vec *hs, uint32_t ft) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ds->len; j++) {
    const Pair *d = row_at(ds, j);
    const Str *n = d == NULL ? NULL : d->first;
    const LinkFn *f = d == NULL ? NULL : d->second;
    if (n == NULL || f == NULL) return missing_item(heap);
    Result w = emit_host_wrapper(heap, l, hs, ft + (uint32_t)j, *n, f);
    if (w.kind == RESULT_FAIL) return w;
    if (!vec_push(&out, w.value)) heap->oom = true;
  }
  return done_vec(heap, out);
}

/* host_exports(ds, base): Export{name, base + j} per definition */
Vec emit_host_exports(Heap *heap, const Vec *ds, uint32_t base) {
  Vec out = vec_new(heap->arena);
  for (size_t j = 0; j < ds->len; j++) {
    const Pair *d = row_at(ds, j);
    const Str *n = d == NULL ? NULL : d->first;
    GcExport *e = n == NULL ? NULL : gc_export(heap, *n, base + (uint32_t)j);
    if (e == NULL || !vec_push(&out, e)) heap->oom = true;
  }
  return out;
}

/* reactor_linked(l, ns): the module with the host handle types, the signatures, the wrappers and their exports */
Result emit_reactor_linked(Heap *heap, const Linked *l, const Vec *ns) {
  Result ds = emit_host_definitions(heap, ns, &l->prog);
  if (ds.kind == RESULT_FAIL) return ds;
  const Vec *dv = ds.value;
  Vec reprs = emit_host_reprs(heap, dv);
  Vec distinct = emit_repr_unique(heap, &reprs);
  Result rs = emit_supported(heap, &distinct);
  if (rs.kind == RESULT_FAIL) return rs;
  Nat ntypes = heap_nat_of_u64(heap, l->types.len);
  Result hs = emit_handles(heap, (const Vec *)rs.value, l, ntypes, heap_nat_of_u64(heap, 0));
  if (hs.kind == RESULT_FAIL) return hs;
  const Vec *hv = hs.value;
  Result sigs = emit_host_signatures(heap, dv, hv);
  if (sigs.kind == RESULT_FAIL) return sigs;
  Result types = link_comptype_groups(heap, l);
  if (types.kind == RESULT_FAIL) return types;
  Result functions = emit_funcs(heap, &l->funcs, l, NULL);
  if (functions.kind == RESULT_FAIL) return functions;
  uint32_t ft = u32_of(heap_nat_add(heap, ntypes, heap_nat_of_u64(heap, hv->len)));
  Result wrappers = emit_host_wrappers(heap, dv, l, hv, ft);
  if (wrappers.kind == RESULT_FAIL) return wrappers;
  const Vec *fv = functions.value;
  Vec handle_types = emit_handle_types(heap, hv);
  Vec extra = emit_cat(heap, &handle_types, (const Vec *)sigs.value);
  Vec all_types = emit_cat(heap, (const Vec *)types.value, &extra);
  Vec all_funcs = emit_cat(heap, fv, (const Vec *)wrappers.value);
  Vec exports = emit_host_exports(heap, dv, (uint32_t)fv->len);
  GcModule *m = gc_module(heap, all_types, vec_new(heap->arena), all_funcs, exports, l->declared);
  if (m == NULL) return missing_item(heap);
  return done_str(heap, gc_encode(heap, m));
}

/* valid_exports(ns): at least one export and no duplicate names */
Result emit_valid_exports(Heap *heap, const Vec *ns) {
  if (ns->len == 0) return fail(error_make(ERROR_MISMATCH, str_lit("build needs at least one export")));
  Vec seen = vec_new(heap->arena);
  Vec distinct = emit_unique(heap, ns, &seen);
  if (distinct.len == ns->len) return done(&unit_value);
  return fail(error_make(ERROR_MISMATCH, str_lit("duplicate export name")));
}

/* reactor(rows, exports) = valid_exports, then L.build, then reactor_linked */
Result emit_reactor(Heap *heap, const Vec *rows, const Vec *exports) {
  Result u = emit_valid_exports(heap, exports);
  if (u.kind == RESULT_FAIL) return u;
  Result l = link_build(heap, rows);
  if (l.kind == RESULT_FAIL) return l;
  return emit_reactor_linked(heap, (const Linked *)l.value, exports);
}
