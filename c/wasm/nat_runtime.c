/* Port of bend2/wasm/nat_runtime.bend: the WasmGC bodies of the Nat runtime helpers (length, digit,
 * copy, normal, compareDigits, compare, the add, sub and mul loops, the slow paths) and of the five
 * primitives natAdd, natSub, natMul, natEq and natLt. A Nat is an i31 when it fits in 30 bits and a
 * nat-big struct over a nat-limbs array of 15-bit limbs otherwise. Lists are Vecs of pointers; a
 * failed allocation sets heap->oom and leaves the item out. */
#include <stdarg.h>

#include "wasm.h"

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

/* Some{I32} and Some{Ref{heap type}} as block types */
static GcMaybeValtype bt_i32(void) { return gc_valtype_some(gc_valtype_i32()); }
static GcMaybeValtype bt_ref(GcHeaptypeKind kind, uint32_t index) {
  return gc_valtype_some(gc_valtype_ref(gc_heaptype(kind, index)));
}

/* Instructions: op without an immediate, at with one index, get/put the locals, num an I32_const. */
static GcInstr *op(Heap *heap, GcInstrKind kind) { return gc_instr(heap, kind); }
static GcInstr *at(Heap *heap, GcInstrKind kind, uint32_t index) { return gc_instr_index(heap, kind, index); }
static GcInstr *get(Heap *heap, uint32_t i) { return gc_instr_index(heap, GC_LOCAL_GET, i); }
static GcInstr *put(Heap *heap, uint32_t i) { return gc_instr_index(heap, GC_LOCAL_SET, i); }
static GcInstr *num(Heap *heap, uint32_t v) { return gc_instr_index(heap, GC_I32_CONST, v); }
/* Br_on_cast{0, HEq, HI31} */
static GcInstr *br_i31(Heap *heap) {
  return gc_instr_br_on_cast(heap, 0, gc_heaptype(GC_HEAP_EQ, 0), gc_heaptype(GC_HEAP_I31, 0));
}

static NatBody body(Vec locals, Vec code) {
  NatBody out;
  out.locals = locals;
  out.code = code;
  return out;
}

/* cat(a, b) = a ++ b */
Vec nat_runtime_cat(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < a->len; i++) {
    VecGet item = vec_get(a, i);
    if (item.status == VEC_GET_OUT_OF_RANGE || !vec_push(&out, item.value)) heap->oom = true;
  }
  for (size_t i = 0; i < b->len; i++) {
    VecGet item = vec_get(b, i);
    if (item.status == VEC_GET_OUT_OF_RANGE || !vec_push(&out, item.value)) heap->oom = true;
  }
  return out;
}

/* low_limb(i): local i mod 32768 */
Vec nat_runtime_low_limb(Heap *heap, uint32_t i) {
  return seq(heap, get(heap, i), get(heap, i), num(heap, 32768), op(heap, GC_I32_DIV_U), num(heap, 32768),
             op(heap, GC_I32_MUL), op(heap, GC_I32_SUB), &stop);
}

/* nat_length(r): the limb count of local 0 (0, 1 or 2 for an i31) */
NatBody nat_runtime_nat_length(Heap *heap, const NatRuntime *r) {
  return body(
      vals(heap, 1, (GcValtype[]){gc_valtype_i32()}),
      seq(heap,
          gc_instr_block(heap, bt_ref(GC_HEAP_I31, 0),
                         seq(heap, get(heap, 0), br_i31(heap), gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, r->big)),
                             gc_instr_struct_get(heap, r->big, 1), op(heap, GC_ARRAY_LEN), op(heap, GC_RETURN), &stop)),
          op(heap, GC_I31_GET_U), put(heap, 1), get(heap, 1), num(heap, 0), op(heap, GC_I32_EQ),
          gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 0), &stop),
                      seq(heap, get(heap, 1), num(heap, 32768), op(heap, GC_I32_LT_U),
                          gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 1), &stop), seq(heap, num(heap, 2), &stop)),
                          &stop)),
          &stop));
}

/* nat_digit(r): limb local 1 of local 0 (0 past the end) */
NatBody nat_runtime_nat_digit(Heap *heap, const NatRuntime *r) {
  return body(
      vals(heap, 2, (GcValtype[]){gc_valtype_ref(gc_heaptype(GC_HEAP_TYPE, r->array)), gc_valtype_i32()}),
      seq(heap,
          gc_instr_block(
              heap, bt_ref(GC_HEAP_I31, 0),
              seq(heap, get(heap, 0), br_i31(heap), gc_instr_ref_cast(heap, gc_heaptype(GC_HEAP_TYPE, r->big)),
                  gc_instr_struct_get(heap, r->big, 1), put(heap, 2), get(heap, 1), get(heap, 2), op(heap, GC_ARRAY_LEN),
                  op(heap, GC_I32_LT_U),
                  gc_instr_if(heap, bt_i32(), seq(heap, get(heap, 2), get(heap, 1), at(heap, GC_ARRAY_GET, r->array), &stop),
                              seq(heap, num(heap, 0), &stop)),
                  op(heap, GC_RETURN), &stop)),
          op(heap, GC_I31_GET_U), put(heap, 3), get(heap, 1), num(heap, 0), op(heap, GC_I32_EQ),
          gc_instr_if(heap, bt_i32(), nat_runtime_low_limb(heap, 3),
                      seq(heap, get(heap, 1), num(heap, 1), op(heap, GC_I32_EQ),
                          gc_instr_if(heap, bt_i32(), seq(heap, get(heap, 3), num(heap, 32768), op(heap, GC_I32_DIV_U), &stop),
                                      seq(heap, num(heap, 0), &stop)),
                          &stop)),
          &stop));
}

/* nat_copy(r): copy limbs [local 2, local 3) of local 0 into local 1 */
NatBody nat_runtime_nat_copy(Heap *heap, const NatRuntime *r) {
  return body(vec_new(heap->arena),
              seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_LT_U),
                  gc_instr_if(heap, bt_ref(GC_HEAP_TYPE, r->array),
                              seq(heap, get(heap, 1), get(heap, 2), get(heap, 0), get(heap, 2), at(heap, GC_ARRAY_GET, r->array),
                                  at(heap, GC_ARRAY_SET, r->array), get(heap, 0), get(heap, 1), get(heap, 2), num(heap, 1),
                                  op(heap, GC_I32_ADD), get(heap, 3), at(heap, GC_RETURN_CALL, r->copy), &stop),
                              seq(heap, get(heap, 1), &stop)),
                  &stop));
}

/* nat_normal(r): the canonical Nat of the first local 1 limbs of local 0 */
NatBody nat_runtime_nat_normal(Heap *heap, const NatRuntime *r) {
  Vec small = seq(heap, get(heap, 0), num(heap, 0), at(heap, GC_ARRAY_GET, r->array), get(heap, 1), num(heap, 2),
                  op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_i32(),
                              seq(heap, get(heap, 0), num(heap, 1), at(heap, GC_ARRAY_GET, r->array), num(heap, 32768),
                                  op(heap, GC_I32_MUL), &stop),
                              seq(heap, num(heap, 0), &stop)),
                  op(heap, GC_I32_ADD), op(heap, GC_REF_I31), &stop);
  Vec big = seq(heap, num(heap, 1), get(heap, 0), num(heap, 0), get(heap, 1), at(heap, GC_ARRAY_NEW, r->array), num(heap, 0),
                get(heap, 1), at(heap, GC_CALL, r->copy), at(heap, GC_STRUCT_NEW, r->big), &stop);
  Vec trim = seq(heap, get(heap, 0), get(heap, 1), num(heap, 1), op(heap, GC_I32_SUB), at(heap, GC_RETURN_CALL, r->normal), &stop);
  Vec sized = seq(heap, get(heap, 1), num(heap, 3), op(heap, GC_I32_LT_U), gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), small, big),
                  &stop);
  Vec nonzero = seq(heap, get(heap, 0), get(heap, 1), num(heap, 1), op(heap, GC_I32_SUB), at(heap, GC_ARRAY_GET, r->array),
                    num(heap, 0), op(heap, GC_I32_EQ), gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), trim, sized), &stop);
  return body(vec_new(heap->arena),
              seq(heap, get(heap, 1), num(heap, 0), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), seq(heap, num(heap, 0), op(heap, GC_REF_I31), &stop), nonzero),
                  &stop));
}

/* nat_compare_digits(r): compare the first local 2 limbs of locals 0 and 1 (0 eq, 1 lt, 2 gt) */
NatBody nat_runtime_nat_compare_digits(Heap *heap, const NatRuntime *r) {
  Vec differ = seq(heap, get(heap, 3), get(heap, 4), op(heap, GC_I32_LT_U),
                   gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 1), &stop), seq(heap, num(heap, 2), &stop)), &stop);
  Vec next = seq(heap, get(heap, 2), num(heap, 1), op(heap, GC_I32_SUB), put(heap, 2), get(heap, 0), get(heap, 2),
                 at(heap, GC_CALL, r->digit), put(heap, 3), get(heap, 1), get(heap, 2), at(heap, GC_CALL, r->digit), put(heap, 4),
                 get(heap, 3), get(heap, 4), op(heap, GC_I32_EQ),
                 gc_instr_if(heap, bt_i32(),
                             seq(heap, get(heap, 0), get(heap, 1), get(heap, 2), at(heap, GC_RETURN_CALL, r->cmp_digits), &stop),
                             differ),
                 &stop);
  return body(vals(heap, 2, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32()}),
              seq(heap, get(heap, 2), num(heap, 0), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 0), &stop), next), &stop));
}

/* nat_compare(r): compare locals 0 and 1 by limb count, then by limbs */
NatBody nat_runtime_nat_compare(Heap *heap, const NatRuntime *r) {
  return body(
      vals(heap, 2, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32()}),
      seq(heap, get(heap, 0), at(heap, GC_CALL, r->length), put(heap, 2), get(heap, 1), at(heap, GC_CALL, r->length), put(heap, 3),
          get(heap, 2), get(heap, 3), op(heap, GC_I32_EQ),
          gc_instr_if(heap, bt_i32(),
                      seq(heap, get(heap, 0), get(heap, 1), get(heap, 2), at(heap, GC_RETURN_CALL, r->cmp_digits), &stop),
                      seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_LT_U),
                          gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 1), &stop), seq(heap, num(heap, 2), &stop)), &stop)),
          &stop));
}

/* nat_add_loop(r): limb local 3 of local 0 + local 1 + carry local 5 into local 2, up to local 4 */
NatBody nat_runtime_nat_add_loop(Heap *heap, const NatRuntime *r) {
  Vec head = seq(heap, get(heap, 0), get(heap, 3), at(heap, GC_CALL, r->digit), get(heap, 1), get(heap, 3),
                 at(heap, GC_CALL, r->digit), op(heap, GC_I32_ADD), get(heap, 5), op(heap, GC_I32_ADD), put(heap, 6), get(heap, 2),
                 get(heap, 3), &stop);
  Vec low = nat_runtime_low_limb(heap, 6);
  Vec tail = seq(heap, at(heap, GC_ARRAY_SET, r->array), get(heap, 0), get(heap, 1), get(heap, 2), get(heap, 3), num(heap, 1),
                 op(heap, GC_I32_ADD), get(heap, 4), get(heap, 6), num(heap, 32768), op(heap, GC_I32_DIV_U),
                 at(heap, GC_RETURN_CALL, r->add_loop), &stop);
  Vec rest = nat_runtime_cat(heap, &low, &tail);
  Vec step = nat_runtime_cat(heap, &head, &rest);
  return body(vals(heap, 1, (GcValtype[]){gc_valtype_i32()}),
              seq(heap, get(heap, 3), get(heap, 4), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0),
                              seq(heap, get(heap, 2), get(heap, 3), get(heap, 5), at(heap, GC_ARRAY_SET, r->array), get(heap, 2),
                                  get(heap, 4), num(heap, 1), op(heap, GC_I32_ADD), at(heap, GC_RETURN_CALL, r->normal), &stop),
                              step),
                  &stop));
}

/* nat_sub_loop(r): limb local 3 of local 0 - local 1 - borrow local 5 into local 2, up to local 4 */
NatBody nat_runtime_nat_sub_loop(Heap *heap, const NatRuntime *r) {
  Vec step = seq(heap, get(heap, 0), get(heap, 3), at(heap, GC_CALL, r->digit), put(heap, 6), get(heap, 1), get(heap, 3),
                 at(heap, GC_CALL, r->digit), get(heap, 5), op(heap, GC_I32_ADD), put(heap, 7), get(heap, 6), get(heap, 7),
                 op(heap, GC_I32_LT_U), put(heap, 5), get(heap, 5),
                 gc_instr_if(heap, bt_i32(),
                             seq(heap, get(heap, 6), num(heap, 32768), op(heap, GC_I32_ADD), get(heap, 7), op(heap, GC_I32_SUB), &stop),
                             seq(heap, get(heap, 6), get(heap, 7), op(heap, GC_I32_SUB), &stop)),
                 put(heap, 8), get(heap, 2), get(heap, 3), get(heap, 8), at(heap, GC_ARRAY_SET, r->array), get(heap, 0), get(heap, 1),
                 get(heap, 2), get(heap, 3), num(heap, 1), op(heap, GC_I32_ADD), get(heap, 4), get(heap, 5),
                 at(heap, GC_RETURN_CALL, r->sub_loop), &stop);
  return body(vals(heap, 3, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32(), gc_valtype_i32()}),
              seq(heap, get(heap, 3), get(heap, 4), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0),
                              seq(heap, get(heap, 2), get(heap, 4), at(heap, GC_RETURN_CALL, r->normal), &stop), step),
                  &stop));
}

/* nat_mul_loop(r): schoolbook product of locals 0 and 1 into local 2 at limbs local 3, local 4 */
NatBody nat_runtime_nat_mul_loop(Heap *heap, const NatRuntime *r) {
  Vec head = seq(heap, get(heap, 0), get(heap, 3), at(heap, GC_CALL, r->digit), get(heap, 1), get(heap, 4),
                 at(heap, GC_CALL, r->digit), op(heap, GC_I32_MUL), get(heap, 2), get(heap, 3), get(heap, 4), op(heap, GC_I32_ADD),
                 at(heap, GC_ARRAY_GET, r->array), op(heap, GC_I32_ADD), get(heap, 5), op(heap, GC_I32_ADD), put(heap, 6),
                 get(heap, 2), get(heap, 3), get(heap, 4), op(heap, GC_I32_ADD), &stop);
  Vec low = nat_runtime_low_limb(heap, 6);
  Vec tail = seq(heap, at(heap, GC_ARRAY_SET, r->array), get(heap, 0), get(heap, 1), get(heap, 2), get(heap, 3), get(heap, 4),
                 num(heap, 1), op(heap, GC_I32_ADD), get(heap, 6), num(heap, 32768), op(heap, GC_I32_DIV_U),
                 at(heap, GC_RETURN_CALL, r->mul_loop), &stop);
  Vec rest = nat_runtime_cat(heap, &low, &tail);
  Vec step = nat_runtime_cat(heap, &head, &rest);
  Vec row = seq(heap, get(heap, 4), get(heap, 8), op(heap, GC_I32_EQ),
                gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0),
                            seq(heap, get(heap, 2), get(heap, 3), get(heap, 4), op(heap, GC_I32_ADD), get(heap, 5),
                                at(heap, GC_ARRAY_SET, r->array), get(heap, 0), get(heap, 1), get(heap, 2), get(heap, 3),
                                num(heap, 1), op(heap, GC_I32_ADD), num(heap, 0), num(heap, 0),
                                at(heap, GC_RETURN_CALL, r->mul_loop), &stop),
                            step),
                &stop);
  return body(vals(heap, 3, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32(), gc_valtype_i32()}),
              seq(heap, get(heap, 0), at(heap, GC_CALL, r->length), put(heap, 7), get(heap, 1), at(heap, GC_CALL, r->length),
                  put(heap, 8), get(heap, 3), get(heap, 7), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0),
                              seq(heap, get(heap, 2), get(heap, 2), op(heap, GC_ARRAY_LEN), at(heap, GC_RETURN_CALL, r->normal), &stop),
                              row),
                  &stop));
}

/* nat_slow_add(r): local 0 + local 1 over a fresh array one limb longer than the longer operand */
NatBody nat_runtime_nat_slow_add(Heap *heap, const NatRuntime *r) {
  return body(vals(heap, 2, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32()}),
              seq(heap, get(heap, 0), at(heap, GC_CALL, r->length), put(heap, 2), get(heap, 1), at(heap, GC_CALL, r->length),
                  put(heap, 3), get(heap, 2), get(heap, 3), op(heap, GC_I32_LT_U),
                  gc_instr_if(heap, gc_valtype_none(), seq(heap, get(heap, 3), put(heap, 2), &stop), seq(heap, &stop)),
                  get(heap, 0), get(heap, 1), num(heap, 0), get(heap, 2), num(heap, 1), op(heap, GC_I32_ADD),
                  at(heap, GC_ARRAY_NEW, r->array), num(heap, 0), get(heap, 2), num(heap, 0), at(heap, GC_RETURN_CALL, r->add_loop),
                  &stop));
}

/* nat_slow_sub(r): local 0 - local 1, truncated at 0 */
NatBody nat_runtime_nat_slow_sub(Heap *heap, const NatRuntime *r) {
  return body(vec_new(heap->arena),
              seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_CALL, r->cmp), num(heap, 1), op(heap, GC_I32_EQ),
                  gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), seq(heap, num(heap, 0), op(heap, GC_REF_I31), &stop),
                              seq(heap, get(heap, 0), get(heap, 1), num(heap, 0), get(heap, 0), at(heap, GC_CALL, r->length),
                                  at(heap, GC_ARRAY_NEW, r->array), num(heap, 0), get(heap, 0), at(heap, GC_CALL, r->length),
                                  num(heap, 0), at(heap, GC_RETURN_CALL, r->sub_loop), &stop)),
                  &stop));
}

/* nat_slow_mul(r): local 0 * local 1 over a fresh array of the summed limb counts */
NatBody nat_runtime_nat_slow_mul(Heap *heap, const NatRuntime *r) {
  return body(vec_new(heap->arena),
              seq(heap, get(heap, 0), get(heap, 1), num(heap, 0), get(heap, 0), at(heap, GC_CALL, r->length), get(heap, 1),
                  at(heap, GC_CALL, r->length), op(heap, GC_I32_ADD), at(heap, GC_ARRAY_NEW, r->array), num(heap, 0), num(heap, 0),
                  num(heap, 0), at(heap, GC_RETURN_CALL, r->mul_loop), &stop));
}

/* find_kind(ns, name): the kind of the first row named name */
MaybeNatHelperKind nat_runtime_find_kind(const NatNamed *ns, size_t count, Str name) {
  MaybeNatHelperKind out;
  out.some = false;
  out.value.kind = NAT_HELPER_LENGTH;
  out.value.primitive = NAT_PRIM_ADD;
  for (size_t i = 0; i < count; i++) {
    if (str_eq(str_lit(ns[i].name), name)) {
      out.some = true;
      out.value = ns[i].kind;
      return out;
    }
  }
  return out;
}

/* kind(name): the helper kind of a runtime function name */
MaybeNatHelperKind nat_runtime_kind(Str name) {
  static const NatNamed named[] = {
      {"length", {NAT_HELPER_LENGTH, NAT_PRIM_ADD}},
      {"digit", {NAT_HELPER_DIGIT, NAT_PRIM_ADD}},
      {"copy", {NAT_HELPER_COPY, NAT_PRIM_ADD}},
      {"normal", {NAT_HELPER_NORMAL, NAT_PRIM_ADD}},
      {"compareDigits", {NAT_HELPER_COMPARE_DIGITS, NAT_PRIM_ADD}},
      {"compare", {NAT_HELPER_COMPARE, NAT_PRIM_ADD}},
      {"addLoop", {NAT_HELPER_ADD_LOOP, NAT_PRIM_ADD}},
      {"subLoop", {NAT_HELPER_SUB_LOOP, NAT_PRIM_ADD}},
      {"mulLoop", {NAT_HELPER_MUL_LOOP, NAT_PRIM_ADD}},
      {"slowAdd", {NAT_HELPER_SLOW_ADD, NAT_PRIM_ADD}},
      {"slowSub", {NAT_HELPER_SLOW_SUB, NAT_PRIM_ADD}},
      {"slowMul", {NAT_HELPER_SLOW_MUL, NAT_PRIM_ADD}},
      {"natAdd", {NAT_HELPER_PRIM, NAT_PRIM_ADD}},
      {"natSub", {NAT_HELPER_PRIM, NAT_PRIM_SUB}},
      {"natMul", {NAT_HELPER_PRIM, NAT_PRIM_MUL}},
      {"natEq", {NAT_HELPER_PRIM, NAT_PRIM_EQ}},
      {"natLt", {NAT_HELPER_PRIM, NAT_PRIM_LT}},
  };
  return nat_runtime_find_kind(named, sizeof(named) / sizeof(named[0]), name);
}

/* One index lookup of nat_indices: on Done store the index and return true, on Fail keep the error. */
static bool nat_runtime_index(Result found, uint32_t *slot, Result *fail) {
  if (found.kind == RESULT_FAIL) {
    *fail = found;
    return false;
  }
  const uint32_t *index = found.value;
  *slot = index == NULL ? 0 : *index;
  return true;
}

/* nat_indices(l): the nat-limbs and nat-big type indices and the twelve helper function indices */
Result nat_runtime_nat_indices(Heap *heap, const Linked *l) {
  NatRuntime r;
  Result fail = budget_result_fail(error_host_oom());
  bool ok = nat_runtime_index(link_type_index(heap, l, str_lit("nat-limbs")), &r.array, &fail) &&
            nat_runtime_index(link_type_index(heap, l, str_lit("nat-big")), &r.big, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:length")), &r.length, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:digit")), &r.digit, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:copy")), &r.copy, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:normal")), &r.normal, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:compareDigits")), &r.cmp_digits, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:compare")), &r.cmp, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:addLoop")), &r.add_loop, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:subLoop")), &r.sub_loop, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:mulLoop")), &r.mul_loop, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:slowAdd")), &r.slow_add, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:slowSub")), &r.slow_sub, &fail) &&
            nat_runtime_index(link_func_index(heap, l, str_lit("nat-runtime:slowMul")), &r.slow_mul, &fail);
  if (!ok) return fail;
  NatRuntime *box = arena_alloc(heap->arena, sizeof(NatRuntime));
  if (box == NULL) {
    heap->oom = true;
    return fail;
  }
  *box = r;
  return budget_result_done(box);
}

/* slow(p, r): the big-number path of a primitive over locals 0 and 1 */
Vec nat_runtime_slow(Heap *heap, NatPrimitive p, const NatRuntime *r) {
  switch (p) {
    case NAT_PRIM_ADD:
      return seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_RETURN_CALL, r->slow_add), &stop);
    case NAT_PRIM_SUB:
      return seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_RETURN_CALL, r->slow_sub), &stop);
    case NAT_PRIM_MUL:
      return seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_RETURN_CALL, r->slow_mul), &stop);
    case NAT_PRIM_EQ:
      return seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_CALL, r->cmp), num(heap, 0), op(heap, GC_I32_EQ),
                 op(heap, GC_REF_I31), op(heap, GC_RETURN), &stop);
    case NAT_PRIM_LT:
      return seq(heap, get(heap, 0), get(heap, 1), at(heap, GC_CALL, r->cmp), num(heap, 1), op(heap, GC_I32_EQ),
                 op(heap, GC_REF_I31), op(heap, GC_RETURN), &stop);
  }
  return vec_new(heap->arena);
}

/* cast(param, dst, slow): unbox the i31 local param into dst, or run slow on a big operand */
Vec nat_runtime_cast(Heap *heap, uint32_t param, uint32_t dst, const Vec *slow) {
  Vec head = seq(heap, get(heap, param), br_i31(heap), put(heap, param), &stop);
  return seq(heap, gc_instr_block(heap, bt_ref(GC_HEAP_I31, 0), nat_runtime_cat(heap, &head, slow)), op(heap, GC_I31_GET_U),
             put(heap, dst), &stop);
}

/* small(p, slow): the i31 path of a primitive over the unboxed locals 2 and 3 */
Vec nat_runtime_small(Heap *heap, NatPrimitive p, const Vec *slow) {
  switch (p) {
    case NAT_PRIM_ADD:
      return seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_ADD), put(heap, 4), get(heap, 4), num(heap, 1073741823),
                 op(heap, GC_I32_GT_U),
                 gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), *slow, seq(heap, get(heap, 4), op(heap, GC_REF_I31), &stop)), &stop);
    case NAT_PRIM_SUB:
      return seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_LT_U),
                 gc_instr_if(heap, bt_i32(), seq(heap, num(heap, 0), &stop),
                             seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_SUB), &stop)),
                 op(heap, GC_REF_I31), &stop);
    case NAT_PRIM_MUL:
      return seq(heap, get(heap, 2), num(heap, 0), op(heap, GC_I32_EQ),
                 gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), seq(heap, num(heap, 0), op(heap, GC_REF_I31), &stop),
                             seq(heap, get(heap, 3), num(heap, 1073741823), get(heap, 2), op(heap, GC_I32_DIV_U),
                                 op(heap, GC_I32_GT_U),
                                 gc_instr_if(heap, bt_ref(GC_HEAP_EQ, 0), *slow,
                                             seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_MUL), op(heap, GC_REF_I31), &stop)),
                                 &stop)),
                 &stop);
    case NAT_PRIM_EQ:
      return seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_EQ), op(heap, GC_REF_I31), &stop);
    case NAT_PRIM_LT:
      return seq(heap, get(heap, 2), get(heap, 3), op(heap, GC_I32_LT_U), op(heap, GC_REF_I31), &stop);
  }
  return vec_new(heap->arena);
}

/* nat_primitive(r, p): unbox both operands, then the i31 path with the slow path as fallback */
NatBody nat_runtime_nat_primitive(Heap *heap, const NatRuntime *r, NatPrimitive p) {
  Vec ss = nat_runtime_slow(heap, p, r);
  Vec first = nat_runtime_cast(heap, 0, 2, &ss);
  Vec second = nat_runtime_cast(heap, 1, 3, &ss);
  Vec tail = nat_runtime_small(heap, p, &ss);
  Vec rest = nat_runtime_cat(heap, &second, &tail);
  return body(vals(heap, 3, (GcValtype[]){gc_valtype_i32(), gc_valtype_i32(), gc_valtype_i32()}),
              nat_runtime_cat(heap, &first, &rest));
}

/* make_body(k, r) */
NatBody nat_runtime_make_body(Heap *heap, NatHelperKind k, const NatRuntime *r) {
  switch (k.kind) {
    case NAT_HELPER_LENGTH:
      return nat_runtime_nat_length(heap, r);
    case NAT_HELPER_DIGIT:
      return nat_runtime_nat_digit(heap, r);
    case NAT_HELPER_COPY:
      return nat_runtime_nat_copy(heap, r);
    case NAT_HELPER_NORMAL:
      return nat_runtime_nat_normal(heap, r);
    case NAT_HELPER_COMPARE_DIGITS:
      return nat_runtime_nat_compare_digits(heap, r);
    case NAT_HELPER_COMPARE:
      return nat_runtime_nat_compare(heap, r);
    case NAT_HELPER_ADD_LOOP:
      return nat_runtime_nat_add_loop(heap, r);
    case NAT_HELPER_SUB_LOOP:
      return nat_runtime_nat_sub_loop(heap, r);
    case NAT_HELPER_MUL_LOOP:
      return nat_runtime_nat_mul_loop(heap, r);
    case NAT_HELPER_SLOW_ADD:
      return nat_runtime_nat_slow_add(heap, r);
    case NAT_HELPER_SLOW_SUB:
      return nat_runtime_nat_slow_sub(heap, r);
    case NAT_HELPER_SLOW_MUL:
      return nat_runtime_nat_slow_mul(heap, r);
    case NAT_HELPER_PRIM:
      return nat_runtime_nat_primitive(heap, r, k.primitive);
  }
  return body(vec_new(heap->arena), vec_new(heap->arena));
}

/* finish(ft, Body{ls, code}) = G.Func{ft, ls, code} */
GcFunc *nat_runtime_finish(Heap *heap, uint32_t ft, NatBody b) { return gc_func(heap, ft, b.locals, b.code); }

/* emit_kind(k, name, l): Fail{Err.Unbound} for an unknown name, else the helper function over its
 * nat-sig type */
Result nat_runtime_emit_kind(Heap *heap, MaybeNatHelperKind k, Str name, const Linked *l) {
  if (!k.some) {
    return budget_result_fail(error_make(ERROR_UNBOUND, heap_str_cat(heap, str_lit("unknown Nat runtime helper: "), name)));
  }
  Result ft = link_type_index(heap, l, heap_str_cat(heap, str_lit("nat-sig:"), name));
  if (ft.kind == RESULT_FAIL) return ft;
  Result r = nat_runtime_nat_indices(heap, l);
  if (r.kind == RESULT_FAIL) return r;
  const uint32_t *index = ft.value;
  const NatRuntime *runtime = r.value;
  return budget_result_done(nat_runtime_finish(heap, index == NULL ? 0 : *index, nat_runtime_make_body(heap, k.value, runtime)));
}

/* runtime_func(l, name) = emit_kind(kind(name), name, l) */
Result nat_runtime_runtime_func(Heap *heap, const Linked *l, Str name) {
  return nat_runtime_emit_kind(heap, nat_runtime_kind(name), name, l);
}
