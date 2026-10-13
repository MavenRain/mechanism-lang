/* Port of bend2/wasm/gc_encode.bend: the WasmGC binary encoder. The Bend definitions build byte
 * lists and concatenate them (cat); the port appends to one StrBuf in the same order, so every
 * definition keeps its name and position and the bytes are identical. A failed push sets heap->oom. */
#include "wasm.h"

/* ---- constructors ---- */

GcHeaptype gc_heaptype(GcHeaptypeKind kind, uint32_t index) {
  GcHeaptype h;
  h.kind = kind;
  h.index = index;
  return h;
}

GcValtype gc_valtype_i32(void) {
  GcValtype v;
  v.kind = GC_VAL_I32;
  v.heap_type = gc_heaptype(GC_HEAP_EQ, 0);
  return v;
}

GcValtype gc_valtype_ref(GcHeaptype heap_type) {
  GcValtype v;
  v.kind = GC_VAL_REF;
  v.heap_type = heap_type;
  return v;
}

GcValtype *gc_valtype_box(Heap *heap, GcValtype value) {
  GcValtype *box = heap_alloc(heap, sizeof(GcValtype));
  if (box != NULL) *box = value;
  return box;
}

GcMaybeValtype gc_valtype_some(GcValtype value) {
  GcMaybeValtype m;
  m.some = true;
  m.value = value;
  return m;
}

GcMaybeValtype gc_valtype_none(void) {
  GcMaybeValtype m;
  m.some = false;
  m.value = gc_valtype_i32();
  return m;
}

static GcComptype *comptype_new(Heap *heap, GcComptypeKind kind) {
  GcComptype *c = heap_alloc(heap, sizeof(GcComptype));
  if (c == NULL) return NULL;
  c->kind = kind;
  c->fields = vec_new(heap->arena);
  c->field = gc_valtype_i32();
  c->params = vec_new(heap->arena);
  c->results = vec_new(heap->arena);
  return c;
}

GcComptype *gc_comptype_struct(Heap *heap, Vec fields) {
  GcComptype *c = comptype_new(heap, GC_COMP_STRUCT);
  if (c != NULL) c->fields = fields;
  return c;
}

GcComptype *gc_comptype_array(Heap *heap, GcValtype field) {
  GcComptype *c = comptype_new(heap, GC_COMP_ARRAY);
  if (c != NULL) c->field = field;
  return c;
}

GcComptype *gc_comptype_func(Heap *heap, Vec params, Vec results) {
  GcComptype *c = comptype_new(heap, GC_COMP_FUNC);
  if (c == NULL) return NULL;
  c->params = params;
  c->results = results;
  return c;
}

GcInstr *gc_instr(Heap *heap, GcInstrKind kind) {
  GcInstr *i = heap_alloc(heap, sizeof(GcInstr));
  if (i == NULL) return NULL;
  i->kind = kind;
  i->result = gc_valtype_none();
  i->body = vec_new(heap->arena);
  i->yes = vec_new(heap->arena);
  i->no = vec_new(heap->arena);
  i->source = gc_heaptype(GC_HEAP_EQ, 0);
  i->target = gc_heaptype(GC_HEAP_EQ, 0);
  i->heap_type = gc_heaptype(GC_HEAP_EQ, 0);
  return i;
}

GcInstr *gc_instr_index(Heap *heap, GcInstrKind kind, uint32_t index) {
  GcInstr *i = gc_instr(heap, kind);
  if (i != NULL) i->index = index;
  return i;
}

GcInstr *gc_instr_block(Heap *heap, GcMaybeValtype result, Vec body) {
  GcInstr *i = gc_instr(heap, GC_BLOCK);
  if (i == NULL) return NULL;
  i->result = result;
  i->body = body;
  return i;
}

GcInstr *gc_instr_if(Heap *heap, GcMaybeValtype result, Vec yes, Vec no) {
  GcInstr *i = gc_instr(heap, GC_IF);
  if (i == NULL) return NULL;
  i->result = result;
  i->yes = yes;
  i->no = no;
  return i;
}

GcInstr *gc_instr_br_on_cast(Heap *heap, uint32_t label, GcHeaptype source, GcHeaptype target) {
  GcInstr *i = gc_instr_index(heap, GC_BR_ON_CAST, label);
  if (i == NULL) return NULL;
  i->source = source;
  i->target = target;
  return i;
}

GcInstr *gc_instr_ref_cast(Heap *heap, GcHeaptype heap_type) {
  GcInstr *i = gc_instr(heap, GC_REF_CAST);
  if (i != NULL) i->heap_type = heap_type;
  return i;
}

GcInstr *gc_instr_struct_get(Heap *heap, uint32_t index, uint32_t field) {
  GcInstr *i = gc_instr_index(heap, GC_STRUCT_GET, index);
  if (i != NULL) i->field = field;
  return i;
}

GcFunc *gc_func(Heap *heap, uint32_t ftype, Vec locals, Vec body) {
  GcFunc *f = heap_alloc(heap, sizeof(GcFunc));
  if (f == NULL) return NULL;
  f->ftype = ftype;
  f->locals = locals;
  f->body = body;
  return f;
}

GcExport *gc_export(Heap *heap, Str name, uint32_t index) {
  GcExport *e = heap_alloc(heap, sizeof(GcExport));
  if (e == NULL) return NULL;
  e->name = name;
  e->index = index;
  return e;
}

GcImport *gc_import(Heap *heap, Str module, Str name, uint32_t ftype) {
  GcImport *i = heap_alloc(heap, sizeof(GcImport));
  if (i == NULL) return NULL;
  i->module = module;
  i->name = name;
  i->ftype = ftype;
  return i;
}

GcModule *gc_module(Heap *heap, Vec types, Vec imports, Vec funcs, Vec exports, U32List declared) {
  GcModule *m = heap_alloc(heap, sizeof(GcModule));
  if (m == NULL) return NULL;
  m->types = types;
  m->imports = imports;
  m->funcs = funcs;
  m->exports = exports;
  m->declared = declared;
  return m;
}

Vec *gc_group_box(Heap *heap, Vec group) {
  Vec *box = heap_alloc(heap, sizeof(Vec));
  if (box != NULL) *box = group;
  return box;
}

/* ---- the encoder: one byte list accumulator ---- */

/* x <> rest: one byte appended */
static void push(Heap *heap, StrBuf *out, uint32_t b) {
  if (!strbuf_push_byte(out, (char)(b & 255u))) heap->oom = true;
}

/* cat(xs, ys): the bytes of ys appended after xs */
static void cat(Heap *heap, StrBuf *out, Str ys) {
  if (!strbuf_push(out, ys)) heap->oom = true;
}

/* byte(n) = [U32.and(n, 255)] */
Str gc_byte(Heap *heap, uint32_t n) {
  StrBuf out = strbuf_new(heap->arena);
  push(heap, &out, n & 255u);
  return strbuf_str(&out);
}

/* leb_step(done, low, rest) = [low] when done, else (128 | low) <> rest; true when rest follows */
static bool leb_step(Heap *heap, StrBuf *out, bool done, uint32_t low) {
  if (done) {
    push(heap, out, low);
    return false;
  }
  push(heap, out, 128u | low);
  return true;
}

/* uleb_go(fuel, n, limit): fuel 0 gives []; else leb_step(n < limit, n & 127, uleb_go(fuel - 1, n >> 7, limit)) */
static void uleb_go(Heap *heap, StrBuf *out, uint32_t fuel, uint32_t n, uint32_t limit) {
  if (fuel == 0) return;
  if (leb_step(heap, out, n < limit, n & 127u)) uleb_go(heap, out, fuel - 1, n >> 7, limit);
}

/* uleb(n) = uleb_go(5, n, 128) */
static void uleb(Heap *heap, StrBuf *out, uint32_t n) { uleb_go(heap, out, 5, n, 128); }

Str gc_uleb(Heap *heap, uint32_t n) {
  StrBuf out = strbuf_new(heap->arena);
  uleb(heap, &out, n);
  return strbuf_str(&out);
}

/* signed_rest(sign, rest): the arithmetic shift, rest | 0xFE000000 when the sign bit was set */
static uint32_t signed_rest(bool sign, uint32_t rest) { return sign ? (rest | 4261412864u) : rest; }

/* sleb_go(fuel, n): fuel 0 gives []; else low = n & 127, rest = signed_rest(n >= 2^31, n >> 7),
 * done = (rest == 0 && low & 64 == 0) || (rest == 0xFFFFFFFF && low & 64 != 0); leb_step(done, low, sleb_go(fuel - 1, rest)) */
static void sleb_go(Heap *heap, StrBuf *out, uint32_t fuel, uint32_t n) {
  if (fuel == 0) return;
  uint32_t low = n & 127u;
  uint32_t rest = signed_rest(n >= 2147483648u, n >> 7);
  bool done = (rest == 0 && (low & 64u) == 0) || (rest == 4294967295u && (low & 64u) != 0);
  if (leb_step(heap, out, done, low)) sleb_go(heap, out, fuel - 1, rest);
}

/* sleb(n) = sleb_go(5, n) */
static void sleb(Heap *heap, StrBuf *out, uint32_t n) { sleb_go(heap, out, 5, n); }

Str gc_sleb(Heap *heap, uint32_t n) {
  StrBuf out = strbuf_new(heap->arena);
  sleb(heap, &out, n);
  return strbuf_str(&out);
}

/* positive_sleb(n) = uleb_go(5, n, 64): a non-negative type index as a signed LEB */
static void positive_sleb(Heap *heap, StrBuf *out, uint32_t n) { uleb_go(heap, out, 5, n, 64); }

/* heaptype(h): HEq 109, HI31 108, HFunc 112, HType{index} positive_sleb(index) */
static void heaptype(Heap *heap, StrBuf *out, GcHeaptype h) {
  switch (h.kind) {
    case GC_HEAP_EQ:
      push(heap, out, 109);
      return;
    case GC_HEAP_I31:
      push(heap, out, 108);
      return;
    case GC_HEAP_FUNC:
      push(heap, out, 112);
      return;
    case GC_HEAP_TYPE:
      positive_sleb(heap, out, h.index);
      return;
  }
}

/* valtype(v): I32 127, Ref{heap} 100 <> heaptype(heap) */
static void valtype(Heap *heap, StrBuf *out, GcValtype v) {
  switch (v.kind) {
    case GC_VAL_I32:
      push(heap, out, 127);
      return;
    case GC_VAL_REF:
      push(heap, out, 100);
      heaptype(heap, out, v.heap_type);
      return;
  }
}

/* The GcValtype at index of a Vec of GcValtype*; I32 when out of range (never, the loops stop at len). */
static GcValtype valtype_at(const Vec *vs, size_t index) {
  VecGet got = vec_get(vs, index);
  if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return gc_valtype_i32();
  return *(const GcValtype *)got.value;
}

/* valtypes(vs): the valtypes in order */
static void valtypes(Heap *heap, StrBuf *out, const Vec *vs) {
  for (size_t i = 0; i < vs->len; i++) valtype(heap, out, valtype_at(vs, i));
}

/* fields(vs): every valtype followed by 0 (an immutable field) */
static void fields(Heap *heap, StrBuf *out, const Vec *vs) {
  for (size_t i = 0; i < vs->len; i++) {
    valtype(heap, out, valtype_at(vs, i));
    push(heap, out, 0);
  }
}

/* length(xs) = uleb(List.length(xs)) */
static void length(Heap *heap, StrBuf *out, size_t count) { uleb(heap, out, (uint32_t)count); }

/* comptype(c): CStruct 95 <> length(vs) <> fields(vs); CArray 94 <> valtype(v) <> [1];
 * CFunc 96 <> length(params) <> valtypes(params) <> length(results) <> valtypes(results) */
static void comptype(Heap *heap, StrBuf *out, const GcComptype *c) {
  switch (c->kind) {
    case GC_COMP_STRUCT:
      push(heap, out, 95);
      length(heap, out, c->fields.len);
      fields(heap, out, &c->fields);
      return;
    case GC_COMP_ARRAY:
      push(heap, out, 94);
      valtype(heap, out, c->field);
      push(heap, out, 1);
      return;
    case GC_COMP_FUNC:
      push(heap, out, 96);
      length(heap, out, c->params.len);
      valtypes(heap, out, &c->params);
      length(heap, out, c->results.len);
      valtypes(heap, out, &c->results);
      return;
  }
}

/* The GcComptype at index of a rec group; NULL when out of range (never, the loops stop at len). */
static const GcComptype *comptype_at(const Vec *cs, size_t index) {
  VecGet got = vec_get(cs, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return NULL;
  return (const GcComptype *)got.value;
}

/* sub_finals(cs): every comptype as a final subtype with no supertype: 79 <> 0 <> comptype(c) */
static void sub_finals(Heap *heap, StrBuf *out, const Vec *cs) {
  for (size_t i = 0; i < cs->len; i++) {
    const GcComptype *c = comptype_at(cs, i);
    if (c == NULL) continue;
    push(heap, out, 79);
    push(heap, out, 0);
    comptype(heap, out, c);
  }
}

/* rectype(cs): [] gives [78, 0]; one comptype stands alone; two or more form a rec group 78 <> length <> sub_finals */
static void rectype(Heap *heap, StrBuf *out, const Vec *cs) {
  if (cs->len == 0) {
    push(heap, out, 78);
    push(heap, out, 0);
    return;
  }
  if (cs->len == 1) {
    const GcComptype *c = comptype_at(cs, 0);
    if (c != NULL) comptype(heap, out, c);
    return;
  }
  push(heap, out, 78);
  length(heap, out, cs->len);
  sub_finals(heap, out, cs);
}

/* rectypes(gs): every rec group in order */
static void rectypes(Heap *heap, StrBuf *out, const Vec *gs) {
  for (size_t i = 0; i < gs->len; i++) {
    VecGet got = vec_get(gs, i);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) continue;
    rectype(heap, out, (const Vec *)got.value);
  }
}

/* blocktype(b): None 64, Some{v} valtype(v) */
static void blocktype(Heap *heap, StrBuf *out, GcMaybeValtype b) {
  if (!b.some) {
    push(heap, out, 64);
    return;
  }
  valtype(heap, out, b.value);
}

/* instrs(xs): the instructions in order; Block and If splice their bodies with End (11) and Else (5),
 * which is the same byte sequence as the Bend List.append of body, End{} and rest */
static void instrs(Heap *heap, StrBuf *out, const Vec *xs) {
  for (size_t at = 0; at < xs->len; at++) {
    VecGet got = vec_get(xs, at);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) continue;
    const GcInstr *i = (const GcInstr *)got.value;
    switch (i->kind) {
      case GC_UNREACHABLE:
        push(heap, out, 0);
        break;
      case GC_BLOCK:
        push(heap, out, 2);
        blocktype(heap, out, i->result);
        instrs(heap, out, &i->body);
        push(heap, out, 11);
        break;
      case GC_IF:
        push(heap, out, 4);
        blocktype(heap, out, i->result);
        instrs(heap, out, &i->yes);
        push(heap, out, 5);
        instrs(heap, out, &i->no);
        push(heap, out, 11);
        break;
      case GC_BR:
        push(heap, out, 12);
        uleb(heap, out, i->index);
        break;
      case GC_BR_IF:
        push(heap, out, 13);
        uleb(heap, out, i->index);
        break;
      case GC_BR_ON_CAST:
        push(heap, out, 251);
        push(heap, out, 24);
        push(heap, out, 0);
        uleb(heap, out, i->index);
        heaptype(heap, out, i->source);
        heaptype(heap, out, i->target);
        break;
      case GC_RETURN:
        push(heap, out, 15);
        break;
      case GC_CALL:
        push(heap, out, 16);
        uleb(heap, out, i->index);
        break;
      case GC_RETURN_CALL:
        push(heap, out, 18);
        uleb(heap, out, i->index);
        break;
      case GC_CALL_REF:
        push(heap, out, 20);
        uleb(heap, out, i->index);
        break;
      case GC_RETURN_CALL_REF:
        push(heap, out, 21);
        uleb(heap, out, i->index);
        break;
      case GC_LOCAL_GET:
        push(heap, out, 32);
        uleb(heap, out, i->index);
        break;
      case GC_LOCAL_SET:
        push(heap, out, 33);
        uleb(heap, out, i->index);
        break;
      case GC_LOCAL_TEE:
        push(heap, out, 34);
        uleb(heap, out, i->index);
        break;
      case GC_I32_CONST:
        push(heap, out, 65);
        sleb(heap, out, i->index);
        break;
      case GC_I32_ADD:
        push(heap, out, 106);
        break;
      case GC_I32_SUB:
        push(heap, out, 107);
        break;
      case GC_I32_MUL:
        push(heap, out, 108);
        break;
      case GC_I32_DIV_U:
        push(heap, out, 110);
        break;
      case GC_I32_EQ:
        push(heap, out, 70);
        break;
      case GC_I32_NE:
        push(heap, out, 71);
        break;
      case GC_I32_LT_U:
        push(heap, out, 73);
        break;
      case GC_I32_GT_U:
        push(heap, out, 75);
        break;
      case GC_REF_I31:
        push(heap, out, 251);
        push(heap, out, 28);
        break;
      case GC_I31_GET_S:
        push(heap, out, 251);
        push(heap, out, 29);
        break;
      case GC_I31_GET_U:
        push(heap, out, 251);
        push(heap, out, 30);
        break;
      case GC_REF_CAST:
        push(heap, out, 251);
        push(heap, out, 22);
        heaptype(heap, out, i->heap_type);
        break;
      case GC_REF_FUNC:
        push(heap, out, 210);
        uleb(heap, out, i->index);
        break;
      case GC_STRUCT_NEW:
        push(heap, out, 251);
        push(heap, out, 0);
        uleb(heap, out, i->index);
        break;
      case GC_STRUCT_GET:
        push(heap, out, 251);
        push(heap, out, 2);
        uleb(heap, out, i->index);
        uleb(heap, out, i->field);
        break;
      case GC_ARRAY_NEW:
        push(heap, out, 251);
        push(heap, out, 6);
        uleb(heap, out, i->index);
        break;
      case GC_ARRAY_GET:
        push(heap, out, 251);
        push(heap, out, 11);
        uleb(heap, out, i->index);
        break;
      case GC_ARRAY_SET:
        push(heap, out, 251);
        push(heap, out, 14);
        uleb(heap, out, i->index);
        break;
      case GC_ARRAY_LEN:
        push(heap, out, 251);
        push(heap, out, 15);
        break;
      case GC_END:
        push(heap, out, 11);
        break;
      case GC_ELSE:
        push(heap, out, 5);
        break;
    }
  }
}

/* section(id, payload) = id <> length(payload) <> payload */
static void section(Heap *heap, StrBuf *out, uint32_t id, Str payload) {
  push(heap, out, id);
  length(heap, out, payload.len);
  cat(heap, out, payload);
}

/* locals_body(ls): every local as a run of one: 1 <> valtype(v) */
static void locals_body(Heap *heap, StrBuf *out, const Vec *ls) {
  for (size_t i = 0; i < ls->len; i++) {
    push(heap, out, 1);
    valtype(heap, out, valtype_at(ls, i));
  }
}

/* code_entry(f): payload = length(locals) <> locals_body(locals) <> instrs(body) <> [11]; length(payload) <> payload */
static void code_entry(Heap *heap, StrBuf *out, const GcFunc *f) {
  StrBuf payload = strbuf_new(heap->arena);
  length(heap, &payload, f->locals.len);
  locals_body(heap, &payload, &f->locals);
  instrs(heap, &payload, &f->body);
  push(heap, &payload, 11);
  Str bytes = strbuf_str(&payload);
  length(heap, out, bytes.len);
  cat(heap, out, bytes);
}

/* utf8(cs): the UTF-8 bytes of the code points. A Str already holds the UTF-8 encoding of the Bend
 * String (utf8_1, utf8_2 and utf8_4 re-encode what String.to_list decoded), so the bytes pass through. */
static void utf8(Heap *heap, StrBuf *out, Str s) { cat(heap, out, s); }

/* name(s) = length(bytes) <> bytes */
static void name(Heap *heap, StrBuf *out, Str s) {
  StrBuf bytes = strbuf_new(heap->arena);
  utf8(heap, &bytes, s);
  Str bs = strbuf_str(&bytes);
  length(heap, out, bs.len);
  cat(heap, out, bs);
}

/* exports(es): every export as name(n) <> 0 <> uleb(idx) (0 = a function export) */
static void exports(Heap *heap, StrBuf *out, const Vec *es) {
  for (size_t i = 0; i < es->len; i++) {
    VecGet got = vec_get(es, i);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) continue;
    const GcExport *e = (const GcExport *)got.value;
    name(heap, out, e->name);
    push(heap, out, 0);
    uleb(heap, out, e->index);
  }
}

/* imports(xs): every import as name(module) <> name(n) <> 0 <> uleb(ftype) (0 = a function import) */
static void imports(Heap *heap, StrBuf *out, const Vec *xs) {
  for (size_t i = 0; i < xs->len; i++) {
    VecGet got = vec_get(xs, i);
    if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) continue;
    const GcImport *x = (const GcImport *)got.value;
    name(heap, out, x->module);
    name(heap, out, x->name);
    push(heap, out, 0);
    uleb(heap, out, x->ftype);
  }
}

/* The GcFunc at index of a Vec of GcFunc*; NULL when out of range (never, the loops stop at len). */
static const GcFunc *func_at(const Vec *fs, size_t index) {
  VecGet got = vec_get(fs, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return NULL;
  return (const GcFunc *)got.value;
}

/* func_types(fs): the type index of every function */
static void func_types(Heap *heap, StrBuf *out, const Vec *fs) {
  for (size_t i = 0; i < fs->len; i++) {
    const GcFunc *f = func_at(fs, i);
    if (f != NULL) uleb(heap, out, f->ftype);
  }
}

/* codes(fs): the code entry of every function */
static void codes(Heap *heap, StrBuf *out, const Vec *fs) {
  for (size_t i = 0; i < fs->len; i++) {
    const GcFunc *f = func_at(fs, i);
    if (f != NULL) code_entry(heap, out, f);
  }
}

/* indices(ns): every index as uleb */
static void indices(Heap *heap, StrBuf *out, U32List ns) {
  for (size_t i = 0; i < ns.len; i++) uleb(heap, out, ns.items[i]);
}

/* element_section(ds): nothing when no function is declared; else section 9 with one declarative
 * segment (flags 3, element kind 0) listing the indices */
static void element_section(Heap *heap, StrBuf *out, U32List ds) {
  if (ds.len == 0) return;
  StrBuf payload = strbuf_new(heap->arena);
  push(heap, &payload, 1);
  push(heap, &payload, 3);
  push(heap, &payload, 0);
  length(heap, &payload, ds.len);
  indices(heap, &payload, ds);
  section(heap, out, 9, strbuf_str(&payload));
}

/* import_section(xs): nothing when there is no import; else section 2 with length <> imports */
static void import_section(Heap *heap, StrBuf *out, const Vec *xs) {
  if (xs->len == 0) return;
  StrBuf payload = strbuf_new(heap->arena);
  length(heap, &payload, xs->len);
  imports(heap, &payload, xs);
  section(heap, out, 2, strbuf_str(&payload));
}

/* encode(m): magic and version, then the type (1), import (2), function (3), export (7), element (9)
 * and code (10) sections */
Str gc_encode(Heap *heap, const GcModule *m) {
  static const uint32_t magic[8] = {0, 97, 115, 109, 1, 0, 0, 0};
  StrBuf out = strbuf_new(heap->arena);
  for (size_t i = 0; i < 8; i++) push(heap, &out, magic[i]);
  StrBuf types = strbuf_new(heap->arena);
  length(heap, &types, m->types.len);
  rectypes(heap, &types, &m->types);
  section(heap, &out, 1, strbuf_str(&types));
  import_section(heap, &out, &m->imports);
  StrBuf funcs = strbuf_new(heap->arena);
  length(heap, &funcs, m->funcs.len);
  func_types(heap, &funcs, &m->funcs);
  section(heap, &out, 3, strbuf_str(&funcs));
  StrBuf exported = strbuf_new(heap->arena);
  length(heap, &exported, m->exports.len);
  exports(heap, &exported, &m->exports);
  section(heap, &out, 7, strbuf_str(&exported));
  element_section(heap, &out, m->declared);
  StrBuf code = strbuf_new(heap->arena);
  length(heap, &code, m->funcs.len);
  codes(heap, &code, &m->funcs);
  section(heap, &out, 10, strbuf_str(&code));
  return strbuf_str(&out);
}
