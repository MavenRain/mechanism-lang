/* Port of bend2/tests/wasm_nat_runtime.bend (oracle label fixture-nat): links the Nat runtime keys
 * over an empty program, emits every runtime helper, adds one wrapper per primitive (natAdd, natSub,
 * natMul, natEq, natLt) that takes two i32 digits and a digit index, encodes the module and prints its
 * bytes as a decimal list in brackets; a link failure dies with exit 1. */
#include <stdarg.h>

#include "driver.h"
#include "wasm.h"

static const Result result_zero;

/* The end marker of a seq() argument list. */
static GcInstr stop;

/* A Bend instruction list literal: seq(heap, i1, ..., iN, &stop); a NULL item is a failed
 * allocation (heap->oom set) and is not stored. */
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

/* A Bend valtype list literal of count items, each boxed. */
static Vec vals(Heap *heap, size_t count, const GcValtype *items) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) {
    GcValtype *v = gc_valtype_box(heap, items[i]);
    if (v == NULL || !vec_push(&out, v)) heap->oom = true;
  }
  return out;
}

/* The five primitive names, in the Bend list order of module() */
static const char *const prims[] = {"natAdd", "natSub", "natMul", "natEq", "natLt"};
#define PRIM_COUNT (sizeof prims / sizeof prims[0])

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

/* cat(a, b) = List.append(a, b) */
static Vec cat(Heap *heap, const Vec *a, const Vec *b) { return nat_runtime_cat(heap, a, b); }

/* funcs(l, fs): one runtime_func per FSRuntime{name} row in order; any other Fspec fails with
 * "unexpected runtime fixture function". Done value: Vec * of GcFunc *. */
static Result funcs(Heap *heap, const Linked *l, const Vec *fs) {
  Vec *out = arena_alloc(heap->arena, sizeof(Vec));
  if (out == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  *out = vec_new(heap->arena);
  for (size_t i = 0; i < fs->len; i++) {
    VecGet row = vec_get(fs, i);
    if (row.status == VEC_GET_OUT_OF_RANGE) continue;
    const Fspec *spec = ((const Pair *)row.value)->second;
    if (spec->kind != FSPEC_RUNTIME) {
      return fail(error_make(ERROR_MISMATCH, str_lit("unexpected runtime fixture function")));
    }
    Result f = nat_runtime_runtime_func(heap, l, spec->name);
    if (f.kind == RESULT_FAIL) return f;
    if (!vec_push(out, f.value)) heap->oom = true;
  }
  return done(out);
}

/* wrapper(l, ft, name): G.Func{ft, [], [Local_get 0, Ref_i31, Local_get 1, Ref_i31, Call op,
 * Local_get 2, Call digit]} with op = "nat-runtime:" ++ name and digit = "nat-runtime:digit".
 * Done value: GcFunc *. */
static Result wrapper(Heap *heap, const Linked *l, uint32_t ft, Str name) {
  Result op = link_func_index(heap, l, heap_str_cat(heap, str_lit("nat-runtime:"), name));
  if (op.kind == RESULT_FAIL) return op;
  Result digit = link_func_index(heap, l, str_lit("nat-runtime:digit"));
  if (digit.kind == RESULT_FAIL) return digit;
  Vec code = seq(heap, gc_instr_index(heap, GC_LOCAL_GET, 0), gc_instr(heap, GC_REF_I31),
                 gc_instr_index(heap, GC_LOCAL_GET, 1), gc_instr(heap, GC_REF_I31),
                 gc_instr_index(heap, GC_CALL, *(const uint32_t *)op.value), gc_instr_index(heap, GC_LOCAL_GET, 2),
                 gc_instr_index(heap, GC_CALL, *(const uint32_t *)digit.value), &stop);
  GcFunc *f = gc_func(heap, ft, vec_new(heap->arena), code);
  if (f == NULL) heap->oom = true;
  return done(f);
}

/* wrappers(l, ft, ns): one wrapper per primitive name in order. Done value: Vec * of GcFunc *. */
static Result wrappers(Heap *heap, const Linked *l, uint32_t ft) {
  Vec *out = arena_alloc(heap->arena, sizeof(Vec));
  if (out == NULL) {
    heap->oom = true;
    return fail(error_host_oom());
  }
  *out = vec_new(heap->arena);
  for (size_t k = 0; k < PRIM_COUNT; k++) {
    Result f = wrapper(heap, l, ft, str_lit(prims[k]));
    if (f.kind == RESULT_FAIL) return f;
    if (f.value == NULL || !vec_push(out, f.value)) heap->oom = true;
  }
  return done(out);
}

/* exports(ns, i): G.Export{n, i + k} for the k-th primitive name */
static Vec exports(Heap *heap, uint32_t i) {
  Vec out = vec_new(heap->arena);
  for (size_t k = 0; k < PRIM_COUNT; k++) {
    GcExport *e = gc_export(heap, str_lit(prims[k]), i + (uint32_t)k);
    if (e == NULL || !vec_push(&out, e)) heap->oom = true;
  }
  return out;
}

/* Result<Err.T, List<U32>> of module: the encoded bytes or the link error */
typedef struct {
  ResultKind kind;
  Str bytes;   /* RESULT_DONE */
  Error error; /* RESULT_FAIL */
} Encoded;

static Encoded encoded_fail(Error e) {
  Encoded out;
  out.kind = RESULT_FAIL;
  out.bytes = str_empty();
  out.error = e;
  return out;
}

/* module(l): groups, then the runtime funcs, then the wrappers (the first Fail wins); the types are
 * groups ++ [[CFunc{[I32, I32, I32], [I32]}]], the funcs rs ++ ws, the exports start at the length of
 * the funcs rows. */
static Encoded module(Heap *heap, const Linked *l) {
  Result groups = link_comptype_groups(heap, l);
  if (groups.kind == RESULT_FAIL) return encoded_fail(groups.error);
  Result rs = funcs(heap, l, &l->funcs);
  if (rs.kind == RESULT_FAIL) return encoded_fail(rs.error);
  Result ws = wrappers(heap, l, (uint32_t)l->types.len);
  if (ws.kind == RESULT_FAIL) return encoded_fail(ws.error);
  GcValtype i32 = gc_valtype_i32();
  const GcValtype params[] = {i32, i32, i32};
  const GcValtype results[] = {i32};
  GcComptype *wrap_type = gc_comptype_func(heap, vals(heap, 3, params), vals(heap, 1, results));
  Vec group = vec_new(heap->arena);
  if (wrap_type == NULL || !vec_push(&group, wrap_type)) heap->oom = true;
  Vec *boxed = gc_group_box(heap, group);
  Vec tail = vec_new(heap->arena);
  if (boxed == NULL || !vec_push(&tail, boxed)) heap->oom = true;
  Vec types = cat(heap, (const Vec *)groups.value, &tail);
  Vec all = cat(heap, (const Vec *)rs.value, (const Vec *)ws.value);
  U32List declared;
  declared.items = NULL;
  declared.len = 0;
  GcModule *m = gc_module(heap, types, vec_new(heap->arena), all, exports(heap, (uint32_t)l->funcs.len), declared);
  Encoded out;
  out.kind = RESULT_DONE;
  out.bytes = m == NULL ? str_empty() : gc_encode(heap, m);
  out.error = error_host_oom();
  return out;
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
static int output(Heap *heap, Encoded r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return driver_die_text(1, error_to_string(heap, r.error));
    case RESULT_DONE:
      return driver_print(heap_str_cat(heap, str_lit("["), heap_str_cat(heap, bytes_text(heap, r.bytes), str_lit("]"))));
  }
  return 0;
}

/* main(): output(module(L.finish_layout(L.Prog{[], [], []}, L.nat_runtime_keys()))) */
static int run(Heap *heap) {
  Prog p;
  p.funs = vec_new(heap->arena);
  p.posts = vec_new(heap->arena);
  p.mus = vec_new(heap->arena);
  Linked l = link_finish_layout(heap, &p, link_nat_runtime_keys(heap));
  return output(heap, module(heap, &l));
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
