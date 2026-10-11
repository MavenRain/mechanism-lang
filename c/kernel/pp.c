/* Port of bend2/kernel/pp.bend: the kernel term printer. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

static Vec empty(Heap *heap) {
  return vec_new(heap->arena);
}

/* A Str boxed for a names list; NULL after a failed allocation (heap->oom set). */
static Str *box_str(Heap *heap, Str s) {
  Str *out = heap_alloc(heap, sizeof(Str));
  if (out == NULL) return NULL;
  *out = s;
  return out;
}

static Str str_item(const Vec *xs, size_t index) {
  const Str *item = at(xs, index);
  return item == NULL ? str_empty() : *item;
}

/* name <> names */
static Vec cons_name(Heap *heap, Str name, const Vec *names) {
  Vec out = empty(heap);
  Str *boxed = box_str(heap, name);
  size_t i;
  if (boxed != NULL) push(heap, &out, boxed);
  for (i = 0; i < names->len; i++) push(heap, &out, at(names, i));
  return out;
}

/* Left to right concatenation of the parts. */
static Str cats(Heap *heap, const Str *parts, size_t count) {
  Str out = str_empty();
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

static void buf_push(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* U32.show */
static Str show_u32(Heap *heap, uint32_t n) {
  Str out = str_of_u64(heap->arena, n);
  if (out.len == 0) heap->oom = true;
  return out;
}

/* SCon{Char.from_u32(n), SNil{}} for a printable ASCII byte. */
static Str one_char(Heap *heap, uint32_t n) {
  char *bytes = heap_alloc(heap, 2);
  if (bytes == NULL) return str_empty();
  bytes[0] = (char)n;
  bytes[1] = 0;
  return str_n(bytes, 1);
}

Str pp_decimal_escape(Heap *heap, uint32_t n) {
  Str parts[4];
  parts[0] = str_lit("\\");
  parts[1] = show_u32(heap, n / 100);
  parts[2] = show_u32(heap, (n / 10) % 10);
  parts[3] = show_u32(heap, n % 10);
  return cats(heap, parts, 4);
}

Str pp_escape_byte(Heap *heap, uint32_t n) {
  if (n == 34) return str_lit("\\\"");
  if (n == 92) return str_lit("\\\\");
  if (n == 10) return str_lit("\\n");
  if (n == 13) return str_lit("\\r");
  if (n == 9) return str_lit("\\t");
  if (n == 8) return str_lit("\\b");
  if (n >= 32 && n <= 126) return one_char(heap, n);
  return pp_decimal_escape(heap, n);
}

/* The UTF-8 bytes of the code point n, each escaped. */
Str pp_escape_char(Heap *heap, uint32_t n) {
  Str parts[4];
  if (n < 128) return pp_escape_byte(heap, n);
  if (n < 2048) {
    parts[0] = pp_escape_byte(heap, 192 + n / 64);
    parts[1] = pp_escape_byte(heap, 128 + n % 64);
    return cats(heap, parts, 2);
  }
  if (n < 65536) {
    parts[0] = pp_escape_byte(heap, 224 + n / 4096);
    parts[1] = pp_escape_byte(heap, 128 + (n / 64) % 64);
    parts[2] = pp_escape_byte(heap, 128 + n % 64);
    return cats(heap, parts, 3);
  }
  parts[0] = pp_escape_byte(heap, 240 + n / 262144);
  parts[1] = pp_escape_byte(heap, 128 + (n / 4096) % 64);
  parts[2] = pp_escape_byte(heap, 128 + (n / 64) % 64);
  parts[3] = pp_escape_byte(heap, 128 + n % 64);
  return cats(heap, parts, 4);
}

/* Bend walks the code points and escape_char re-encodes each one as UTF-8 bytes, so for the
 * UTF-8 bytes the host holds the result is escape_byte over every byte in order. */
Str pp_escaped(Heap *heap, Str s) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < s.len; i++) {
    StrAt c = str_at(s, i);
    if (c.status == STR_INDEX_OK) buf_push(heap, &buf, pp_escape_byte(heap, c.value));
  }
  return strbuf_str(&buf);
}

Str pp_literal(Heap *heap, Literal lit) {
  Str parts[3];
  switch (lit.kind) {
  case LITERAL_STRING:
    parts[0] = str_lit("\"");
    parts[1] = pp_escaped(heap, lit.string);
    parts[2] = str_lit("\"");
    return cats(heap, parts, 3);
  case LITERAL_INT:
    return big_to_string(heap, lit.integer);
  }
  return str_empty();
}

Str pp_join(Heap *heap, const Vec *xs, Str sep) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < xs->len; i++) {
    if (i > 0) buf_push(heap, &buf, sep);
    buf_push(heap, &buf, str_item(xs, i));
  }
  return strbuf_str(&buf);
}

Vec pp_binder_names(Heap *heap, const Vec *binders) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < binders->len; i++) {
    const Binder *b = at(binders, i);
    Str *name = b == NULL ? NULL : box_str(heap, b->name);
    if (name != NULL) push(heap, &out, name);
  }
  return out;
}

/* List.append(List.reverse(added), names) */
Vec pp_push(Heap *heap, const Vec *names, const Vec *added) {
  Vec out = empty(heap);
  size_t i;
  for (i = added->len; i > 0; i--) push(heap, &out, at(added, i - 1));
  for (i = 0; i < names->len; i++) push(heap, &out, at(names, i));
  return out;
}

Vec pp_terms(Heap *heap, const Vec *terms, const Vec *names) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < terms->len; i++) {
    const Term *x = at(terms, i);
    Str *text = x == NULL ? NULL : box_str(heap, pp_term(heap, names, x));
    if (text != NULL) push(heap, &out, text);
  }
  return out;
}

static Str bracketed(Heap *heap, const Vec *args, const Vec *names) {
  Vec texts = pp_terms(heap, args, names);
  return pp_join(heap, &texts, str_lit("; "));
}

Str pp_shape(Heap *heap, const Vec *names, Shape s) {
  Str parts[8];
  switch (s.kind) {
  case SHAPE_PI:
    parts[0] = str_lit("SPi ");
    parts[1] = quantity_to_string(s.quantity);
    parts[2] = str_lit(" ");
    parts[3] = s.name;
    parts[4] = str_lit(" ");
    parts[5] = pp_term(heap, names, (const Term *)s.domain);
    return cats(heap, parts, 6);
  case SHAPE_ARG:
    parts[0] = str_lit("SArg ");
    parts[1] = quantity_to_string(s.quantity);
    parts[2] = str_lit(" ");
    parts[3] = s.name;
    return cats(heap, parts, 4);
  case SHAPE_COLL:
    parts[0] = str_lit("SColl ");
    parts[1] = big_to_string(heap, s.count);
    return cats(heap, parts, 2);
  case SHAPE_PAR:
    parts[0] = str_lit("SPar ");
    parts[1] = pp_term(heap, names, (const Term *)s.left);
    parts[2] = str_lit(" ");
    parts[3] = pp_term(heap, names, (const Term *)s.right);
    return cats(heap, parts, 4);
  case SHAPE_MU:
    parts[0] = str_lit("SMu ");
    parts[1] = s.name;
    parts[2] = str_lit(" [");
    parts[3] = bracketed(heap, &s.args, names);
    parts[4] = str_lit("]");
    return cats(heap, parts, 5);
  case SHAPE_NU:
    parts[0] = str_lit("SNu ");
    parts[1] = s.name;
    parts[2] = str_lit(" [");
    parts[3] = bracketed(heap, &s.args, names);
    parts[4] = str_lit("]");
    return cats(heap, parts, 5);
  case SHAPE_ZK:
    parts[0] = str_lit("SZk (");
    parts[1] = quantity_to_string(s.quantity);
    parts[2] = str_lit(" ");
    parts[3] = s.name;
    parts[4] = str_lit(" : ");
    parts[5] = pp_term(heap, names, (const Term *)s.domain);
    parts[6] = str_lit(")");
    return cats(heap, parts, 7);
  case SHAPE_FHC:
    parts[0] = str_lit("SFhc ");
    parts[1] = pp_term(heap, names, (const Term *)s.level);
    return cats(heap, parts, 2);
  case SHAPE_MPC:
    parts[0] = str_lit("SMpc (");
    parts[1] = pp_term(heap, names, (const Term *)s.parties);
    parts[2] = str_lit(", ");
    parts[3] = pp_term(heap, names, (const Term *)s.access);
    parts[4] = str_lit(")");
    return cats(heap, parts, 5);
  }
  return str_empty();
}

Vec pp_under(Heap *heap, Shape s, const Vec *names) {
  switch (s.kind) {
  case SHAPE_PI:
    return cons_name(heap, s.name, names);
  case SHAPE_ZK:
    return cons_name(heap, s.name, names);
  case SHAPE_ARG:
    return *names;
  case SHAPE_COLL:
    return *names;
  case SHAPE_PAR:
    return *names;
  case SHAPE_MU:
    return *names;
  case SHAPE_NU:
    return *names;
  case SHAPE_FHC:
    return *names;
  case SHAPE_MPC:
    return *names;
  }
  return *names;
}

Str pp_addr(Heap *heap, const Vec *names, Addr a) {
  Str parts[5];
  switch (a.kind) {
  case ADDR_PT:
    parts[0] = str_lit("(APt ");
    parts[1] = quantity_to_string(a.quantity);
    parts[2] = str_lit(" ");
    parts[3] = pp_term(heap, names, a.arg);
    parts[4] = str_lit(")");
    return cats(heap, parts, 5);
  case ADDR_LEG:
    parts[0] = str_lit("(ALeg ");
    parts[1] = big_to_string(heap, a.index);
    parts[2] = str_lit(")");
    return cats(heap, parts, 3);
  case ADDR_CTOR:
    parts[0] = str_lit("(ACtor ");
    parts[1] = a.name;
    parts[2] = str_lit(")");
    return cats(heap, parts, 3);
  }
  return str_empty();
}

Str pp_leg(Heap *heap, const Vec *names, const Leg *item) {
  Vec bnames = pp_binder_names(heap, &item->binders);
  Vec inner = pp_push(heap, names, &bnames);
  Str parts[3];
  parts[0] = pp_join(heap, &bnames, str_lit(" "));
  parts[1] = str_lit(" => ");
  parts[2] = pp_term(heap, &inner, item->body);
  return cats(heap, parts, 3);
}

Vec pp_legs(Heap *heap, const Vec *items, const Vec *names) {
  Vec out = empty(heap);
  size_t i;
  for (i = 0; i < items->len; i++) {
    const Leg *x = at(items, i);
    Str *text = x == NULL ? NULL : box_str(heap, pp_leg(heap, names, x));
    if (text != NULL) push(heap, &out, text);
  }
  return out;
}

Str pp_motive(Heap *heap, MaybeMotive item, const Vec *names) {
  Str parts[4];
  Vec pushed;
  Vec inner;
  if (!item.some) return str_lit("");
  pushed = pp_push(heap, names, &item.value.indices);
  inner = cons_name(heap, item.value.self, &pushed);
  parts[0] = str_lit(" as ");
  parts[1] = item.value.self;
  parts[2] = str_lit(" return ");
  parts[3] = pp_term(heap, &inner, item.value.body);
  return cats(heap, parts, 4);
}

Str pp_branches(Heap *heap, const Vec *items, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i;
  for (i = 0; i < items->len; i++) {
    const Branch *b = at(items, i);
    if (b == NULL) continue;
    buf_push(heap, &buf, str_lit(" | "));
    buf_push(heap, &buf, pp_addr(heap, names, b->addr));
    buf_push(heap, &buf, str_lit(" "));
    buf_push(heap, &buf, pp_leg(heap, names, &b->leg));
  }
  return strbuf_str(&buf);
}

Str pp_nth_name(Heap *heap, MaybeStr result, Nat index) {
  if (result.some) return result.value;
  return heap_str_cat(heap, str_lit("#"), heap_nat_show(heap, index));
}

/* List.get(names, index) */
static MaybeStr name_at(const Vec *names, Nat index) {
  VecGet got = common_at(names, index);
  MaybeStr out;
  out.some = got.status == VEC_GET_OK && got.value != NULL;
  out.value = out.some ? *(const Str *)got.value : str_empty();
  return out;
}

Str pp_term(Heap *heap, const Vec *names, const Term *tm) {
  Str parts[9];
  switch (tm->kind) {
  case TERM_VAR:
    return pp_nth_name(heap, name_at(names, tm->index), tm->index);
  case TERM_UNIV:
    parts[0] = str_lit("Type ");
    parts[1] = level_to_string(heap, tm->level);
    return cats(heap, parts, 2);
  case TERM_LAN: {
    Vec inner = pp_under(heap, tm->shape, names);
    parts[0] = str_lit("(Lan ");
    parts[1] = pp_shape(heap, names, tm->shape);
    parts[2] = str_lit(" ");
    parts[3] = pp_term(heap, &inner, tm->body);
    parts[4] = str_lit(")");
    return cats(heap, parts, 5);
  }
  case TERM_RAN: {
    Vec inner = pp_under(heap, tm->shape, names);
    parts[0] = str_lit("(Ran ");
    parts[1] = pp_shape(heap, names, tm->shape);
    parts[2] = str_lit(" ");
    parts[3] = pp_term(heap, &inner, tm->body);
    parts[4] = str_lit(")");
    return cats(heap, parts, 5);
  }
  case TERM_IN:
    parts[0] = str_lit("(In ");
    parts[1] = pp_shape(heap, names, tm->shape);
    parts[2] = str_lit(" ");
    parts[3] = pp_addr(heap, names, tm->addr);
    parts[4] = str_lit(" [");
    parts[5] = bracketed(heap, &tm->args, names);
    parts[6] = str_lit("])");
    return cats(heap, parts, 7);
  case TERM_ELIM:
    parts[0] = str_lit("(Elim ");
    parts[1] = pp_shape(heap, names, tm->data->shape);
    parts[2] = str_lit(" ");
    parts[3] = pp_term(heap, names, tm->data->scrut);
    parts[4] = pp_motive(heap, tm->data->motive, names);
    parts[5] = str_lit(" with");
    parts[6] = pp_branches(heap, &tm->data->branches, names);
    parts[7] = str_lit(")");
    return cats(heap, parts, 8);
  case TERM_SEC: {
    Vec texts = pp_legs(heap, &tm->legs, names);
    parts[0] = str_lit("(Sec ");
    parts[1] = pp_shape(heap, names, tm->shape);
    parts[2] = str_lit(" [");
    parts[3] = pp_join(heap, &texts, str_lit("; "));
    parts[4] = str_lit("])");
    return cats(heap, parts, 5);
  }
  case TERM_OUT:
    parts[0] = str_lit("(Out ");
    parts[1] = pp_shape(heap, names, tm->shape);
    parts[2] = str_lit(" ");
    parts[3] = pp_addr(heap, names, tm->addr);
    parts[4] = str_lit(" ");
    parts[5] = pp_term(heap, names, tm->head);
    parts[6] = str_lit(")");
    return cats(heap, parts, 7);
  case TERM_LET: {
    Vec inner = cons_name(heap, tm->name, names);
    parts[0] = str_lit("let ");
    parts[1] = tm->name;
    parts[2] = str_lit(" : ");
    parts[3] = pp_term(heap, names, tm->ty);
    parts[4] = str_lit(" := ");
    parts[5] = pp_term(heap, names, tm->value);
    parts[6] = str_lit(" in ");
    parts[7] = pp_term(heap, &inner, tm->body);
    return cats(heap, parts, 8);
  }
  case TERM_ANN:
    parts[0] = str_lit("(");
    parts[1] = pp_term(heap, names, tm->body);
    parts[2] = str_lit(" : ");
    parts[3] = pp_term(heap, names, tm->ty);
    parts[4] = str_lit(")");
    return cats(heap, parts, 5);
  case TERM_GLOBAL:
    return tm->name;
  case TERM_LIT:
    return pp_literal(heap, tm->lit);
  case TERM_AUTO:
    return str_lit("auto");
  }
  return str_empty();
}
