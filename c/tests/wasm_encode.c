/* Port of bend2/tests/wasm_encode.bend (oracle label fixture-encode): prints the encoded fixture
 * module, the encoded effects module and three LEB128 probes, each as a decimal byte list in
 * brackets, one per line. */
#include <stdarg.h>

#include "driver.h"
#include "wasm.h"

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

/* G.I32{} boxed */
static GcValtype *i32(Heap *heap) { return gc_valtype_box(heap, gc_valtype_i32()); }

/* G.Ref{G.HType{index}} boxed */
static GcValtype *ref_type(Heap *heap, uint32_t index) {
  return gc_valtype_box(heap, gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, index)));
}

/* G.Ref{G.HI31{}} boxed */
static GcValtype *ref_i31(Heap *heap) { return gc_valtype_box(heap, gc_valtype_ref(gc_heaptype(GC_HEAP_I31, 0))); }

/* G.I32_const{n} */
static GcInstr *i32_const(Heap *heap, uint32_t n) { return gc_instr_index(heap, GC_I32_CONST, n); }

/* fixture(): the module of bend2/tests/wasm_encode.bend */
static GcModule *fixture(Heap *heap) {
  static const uint32_t declared[1] = {6};
  Vec types = list(heap, 5,
                   gc_group_box(heap, list(heap, 1, gc_comptype_func(heap, list(heap, 0), list(heap, 1, i32(heap))))),
                   gc_group_box(heap, list(heap, 1, gc_comptype_struct(heap, list(heap, 1, i32(heap))))),
                   gc_group_box(heap, list(heap, 1, gc_comptype_array(heap, gc_valtype_i32()))),
                   gc_group_box(heap, list(heap, 1, gc_comptype_func(heap, list(heap, 0), list(heap, 1, ref_i31(heap))))),
                   gc_group_box(heap, list(heap, 2, gc_comptype_struct(heap, list(heap, 1, ref_type(heap, 5))),
                                           gc_comptype_func(heap, list(heap, 1, ref_type(heap, 4)), list(heap, 1, i32(heap))))));
  Vec funcs = list(
      heap, 7,
      gc_func(heap, 0, list(heap, 0),
              list(heap, 3, i32_const(heap, 42), gc_instr(heap, GC_REF_I31), gc_instr(heap, GC_I31_GET_U))),
      gc_func(heap, 0, list(heap, 0),
              list(heap, 3, i32_const(heap, 42), gc_instr_index(heap, GC_STRUCT_NEW, 1), gc_instr_struct_get(heap, 1, 0))),
      gc_func(heap, 0, list(heap, 0),
              list(heap, 5, i32_const(heap, 42), i32_const(heap, 3), gc_instr_index(heap, GC_ARRAY_NEW, 2), i32_const(heap, 1),
                   gc_instr_index(heap, GC_ARRAY_GET, 2))),
      gc_func(heap, 0, list(heap, 0),
              list(heap, 2, i32_const(heap, 1),
                   gc_instr_if(heap, gc_valtype_some(gc_valtype_i32()), list(heap, 1, i32_const(heap, 42)),
                               list(heap, 1, i32_const(heap, 0))))),
      gc_func(heap, 0, list(heap, 0), list(heap, 1, gc_instr_index(heap, GC_RETURN_CALL, 0))),
      gc_func(heap, 0, list(heap, 1, ref_type(heap, 4)),
              list(heap, 7, gc_instr_index(heap, GC_REF_FUNC, 6), gc_instr_index(heap, GC_STRUCT_NEW, 4),
                   gc_instr_index(heap, GC_LOCAL_SET, 0), gc_instr_index(heap, GC_LOCAL_GET, 0),
                   gc_instr_index(heap, GC_LOCAL_GET, 0), gc_instr_struct_get(heap, 4, 0), gc_instr_index(heap, GC_CALL_REF, 5))),
      gc_func(heap, 5, list(heap, 0), list(heap, 1, i32_const(heap, 42))));
  Vec exports = list(heap, 6, gc_export(heap, str_lit("i31"), 0), gc_export(heap, str_lit("struct"), 1),
                     gc_export(heap, str_lit("array"), 2), gc_export(heap, str_lit("if"), 3),
                     gc_export(heap, str_lit("tail"), 4), gc_export(heap, str_lit("closure"), 5));
  U32List ds;
  ds.items = declared;
  ds.len = 1;
  return gc_module(heap, types, list(heap, 0), funcs, exports, ds);
}

/* effects(): one imported host function called by one exported function */
static GcModule *effects(Heap *heap) {
  Vec types = list(heap, 2,
                   gc_group_box(heap, list(heap, 1, gc_comptype_func(heap, list(heap, 1, i32(heap)), list(heap, 1, i32(heap))))),
                   gc_group_box(heap, list(heap, 1, gc_comptype_func(heap, list(heap, 0), list(heap, 1, i32(heap))))));
  Vec imports = list(heap, 1, gc_import(heap, str_lit("host"), str_lit("effect"), 0));
  Vec funcs = list(heap, 1, gc_func(heap, 1, list(heap, 0), list(heap, 2, i32_const(heap, 40), gc_instr_index(heap, GC_CALL, 0))));
  Vec exports = list(heap, 1, gc_export(heap, str_lit("run"), 1));
  U32List ds;
  ds.items = NULL;
  ds.len = 0;
  return gc_module(heap, types, imports, funcs, exports, ds);
}

/* IO.print("[" ++ bytes_text(bytes) ++ "]") */
static int print_bytes(Heap *heap, Str bytes) {
  Str text = heap_str_cat(heap, str_lit("["), heap_str_cat(heap, bytes_text(heap, bytes), str_lit("]")));
  return driver_print(text);
}

/* The encoding of a module; no bytes after an allocation failure. */
static Str encoded(Heap *heap, const GcModule *m) {
  if (m == NULL) return str_empty();
  return gc_encode(heap, m);
}

/* main() */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  print_bytes(&heap, encoded(&heap, fixture(&heap)));
  print_bytes(&heap, encoded(&heap, effects(&heap)));
  print_bytes(&heap, gc_uleb(&heap, 4294967295u));
  print_bytes(&heap, gc_sleb(&heap, 2147483648u));
  print_bytes(&heap, gc_sleb(&heap, 4294967295u));
  int code = heap.oom ? driver_host_failure() : 0;
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
