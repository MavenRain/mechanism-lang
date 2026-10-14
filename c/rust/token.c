/* Port of bend2/rust/token.bend: the one-based source positions of the Rust frontend, its token
 * kinds and their one-line descriptions. surface.h already owns the token_ names and rust_out.c
 * includes both headers, so the definitions take the prefix rust_token_; they keep the Bend order.
 * A column counts code points, as the Bend Loc counts Chars. A failed allocation sets heap->oom;
 * the helpers then return placeholders and the caller checks heap->oom. */
#include "rust.h"

/* A KindT constructor: Ident{name} .. Punct{text} carry `text`; Eof{} carries "". */
RustTokKind rust_token_kind(RustTokKindTag tag, Str text) {
  RustTokKind kind;
  kind.tag = tag;
  kind.text = text;
  return kind;
}

/* start: Loc{1, 1} */
RustTokLoc rust_token_start(Heap *heap) {
  RustTokLoc loc;
  loc.line = heap_nat_of_u64(heap, 1);
  loc.col = heap_nat_of_u64(heap, 1);
  return loc;
}

/* advance(l, n): Loc{line, col + n} */
RustTokLoc rust_token_advance(Heap *heap, RustTokLoc l, uint64_t n) {
  Nat step = heap_nat_of_u64(heap, n);
  RustTokLoc loc;
  loc.line = l.line;
  loc.col = heap_nat_add(heap, l.col, step);
  return loc;
}

/* next_line(l): Loc{1 + line, 1} */
RustTokLoc rust_token_next_line(Heap *heap, RustTokLoc l) {
  RustTokLoc loc;
  loc.line = heap_nat_succ(heap, l.line);
  loc.col = heap_nat_of_u64(heap, 1);
  return loc;
}

/* show_loc(l): `line:col` */
Str rust_token_show_loc(Heap *heap, RustTokLoc l) {
  Str line = heap_nat_show(heap, l.line);
  Str col = heap_nat_show(heap, l.col);
  Str head = heap_str_cat(heap, line, str_lit(":"));
  return heap_str_cat(heap, head, col);
}

Str rust_token_show_kind(Heap *heap, RustTokKind k) {
  switch (k.tag) {
  case RUST_TOK_IDENT:
    return heap_str_cat(heap, str_lit("ident "), k.text);
  case RUST_TOK_RAW_IDENT:
    return heap_str_cat(heap, str_lit("raw "), k.text);
  case RUST_TOK_LIFETIME:
    return heap_str_cat(heap, str_lit("lifetime "), k.text);
  case RUST_TOK_INT:
    return heap_str_cat(heap, str_lit("int "), k.text);
  case RUST_TOK_STR:
    return heap_str_cat(heap, str_lit("str "), k.text);
  case RUST_TOK_CHR_LIT:
    return heap_str_cat(heap, str_lit("char "), k.text);
  case RUST_TOK_DOC_OUTER:
    return heap_str_cat(heap, str_lit("doc "), k.text);
  case RUST_TOK_DOC_INNER:
    return heap_str_cat(heap, str_lit("innerdoc "), k.text);
  case RUST_TOK_COMMENT:
    return str_lit("comment");
  case RUST_TOK_PUNCT:
    return heap_str_cat(heap, str_lit("punct "), k.text);
  case RUST_TOK_EOF:
    return str_lit("eof");
  }
  return str_lit("eof");
}

Str rust_token_show_gap(bool g) {
  if (g) return str_lit(" +gap");
  return str_empty();
}

/* show(t): show_loc(loc) ++ " " ++ show_kind(kind) ++ show_gap(gap) */
Str rust_token_show(Heap *heap, const RustTok *t) {
  Str loc = rust_token_show_loc(heap, t->loc);
  Str kind = rust_token_show_kind(heap, t->kind);
  Str head = heap_str_cat(heap, loc, str_lit(" "));
  Str body = heap_str_cat(heap, head, kind);
  return heap_str_cat(heap, body, rust_token_show_gap(t->gap));
}
