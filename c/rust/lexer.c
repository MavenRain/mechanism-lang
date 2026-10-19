/* Port of rust/lexer.bend: the scanner from Rust source text to the token list. Keywords stay
 * identifiers; a non-doc comment becomes a Comment token. The definitions keep the Bend names
 * (prefix rust_lexer_, as surface.h owns lexer_) and the Bend order; the C only helpers stand just
 * before their first use.
 *
 * The Bend String is a list of code points; the C source is the UTF-8 bytes of that String. Each
 * Char a Bend pattern names is ASCII, and no byte of a multi-byte UTF-8 sequence is ASCII, so a byte
 * compare at an offset that only ASCII bytes precede is the Bend Char compare. A pattern variable
 * `c` is one code point (rust_lexer_unit gives its byte length): the C advances a column per code
 * point as the Bend advances per Char, and copies the bytes of a code point where the Bend conses
 * the Char. Line{kind} and Block{depth} hold a uint64_t: kind is 0, 1 or 2 and depth is at most the
 * source length. The Bend conses tokens and text in reverse and reverses them at the end; the C
 * buffers push in order, which yields the same lists. run, a tail recursion in the Bend, is a loop.
 * Each struct value is bound to a local before it is passed on (trap T-202). A failed allocation
 * sets heap->oom; the helpers then return placeholders and the caller checks heap->oom. */
#include "rust.h"

/* Mode: Scan, Word, Raw, Number, Str, Life, Line{kind}, Block{depth}. */
typedef enum {
  RUST_LEXER_SCAN,
  RUST_LEXER_WORD,
  RUST_LEXER_RAW,
  RUST_LEXER_NUMBER,
  RUST_LEXER_STR,
  RUST_LEXER_LIFE,
  RUST_LEXER_LINE,
  RUST_LEXER_BLOCK
} RustLexerModeKind;
typedef struct {
  RustLexerModeKind kind;
  uint64_t n; /* LINE: kind (0 plain comment, 1 outer doc ///, 2 inner doc //!); BLOCK: depth */
} RustLexerMode;

/* State: Running{mode, source, loc, start, tokens, text, nl}, Finished{tokens}, Failed{message}.
 * `nl` counts newlines since the previous token; two or more mark a gap. */
typedef enum { RUST_LEXER_RUNNING, RUST_LEXER_FINISHED, RUST_LEXER_FAILED } RustLexerStateKind;
typedef struct {
  RustLexerStateKind kind;
  RustLexerMode mode; /* RUNNING */
  Str source;         /* RUNNING: the rest of the source */
  RustTokLoc loc;     /* RUNNING */
  RustTokLoc start;   /* RUNNING */
  Vec tokens;         /* RUNNING, FINISHED: RustTok pointers in source order */
  StrBuf text;        /* RUNNING: in source order */
  uint64_t nl;        /* RUNNING */
  Str message;        /* FAILED */
} RustLexerState;

/* C only: the Mode constructor. */
static RustLexerMode rust_lexer_mode(RustLexerModeKind kind, uint64_t n) {
  RustLexerMode mode;
  mode.kind = kind;
  mode.n = n;
  return mode;
}

/* C only: a State of `kind` with every field empty. */
static RustLexerState rust_lexer_blank(Heap *heap, RustLexerStateKind kind) {
  RustLexerState state;
  state.kind = kind;
  state.mode = rust_lexer_mode(RUST_LEXER_SCAN, 0);
  state.source = str_empty();
  state.loc.line = nat_zero();
  state.loc.col = nat_zero();
  state.start = state.loc;
  state.tokens = vec_new(heap->arena);
  state.text = strbuf_new(heap->arena);
  state.nl = 0;
  state.message = str_empty();
  return state;
}

/* C only: Running{mode, source, loc, start, tokens, text, nl} */
static RustLexerState rust_lexer_running(Heap *heap, RustLexerMode mode, Str source, RustTokLoc loc,
                                         RustTokLoc start, Vec tokens, StrBuf text, uint64_t nl) {
  RustLexerState state = rust_lexer_blank(heap, RUST_LEXER_RUNNING);
  state.mode = mode;
  state.source = source;
  state.loc = loc;
  state.start = start;
  state.tokens = tokens;
  state.text = text;
  state.nl = nl;
  return state;
}

/* C only: the byte at offset i of s, or -1 past its end. */
static int rust_lexer_at(Str s, size_t i) {
  if (i >= s.len) return -1;
  return (unsigned char)s.data[i];
}

/* C only: s without its first n bytes (empty when n is past the end). */
static Str rust_lexer_rest(Str s, size_t n) {
  size_t k = n < s.len ? n : s.len;
  Str out;
  out.data = s.data + k;
  out.len = s.len - k;
  return out;
}

/* C only: the bytes [from, from + len) of s, clamped to s. */
static Str rust_lexer_slice(Str s, size_t from, size_t len) {
  Str tail = rust_lexer_rest(s, from);
  Str out;
  out.data = tail.data;
  out.len = len < tail.len ? len : tail.len;
  return out;
}

/* C only: the byte length of the code point that starts s; 0 when s is empty. A lead byte names the
 * length of its UTF-8 sequence; a byte that does not start a whole sequence is one unit. */
static size_t rust_lexer_unit(Str s) {
  int b = rust_lexer_at(s, 0);
  size_t want = (b >= 0xC0 && b <= 0xDF) ? 2 : (b >= 0xE0 && b <= 0xEF) ? 3 : (b >= 0xF0 && b <= 0xF7) ? 4 : 1;
  size_t i = 1;
  if (s.len == 0) return 0;
  if (want > s.len) return 1;
  while (i < want && (rust_lexer_at(s, i) & 0xC0) == 0x80) i++;
  if (i == want) return want;
  return 1;
}

/* C only: the Char that starts s, for the ASCII class tests: its byte below 128, else 128 (each
 * test reads an ASCII range only, so every other code point behaves as 128). */
static uint32_t rust_lexer_code(Str s) {
  int b = rust_lexer_at(s, 0);
  if (b >= 0 && b < 128) return (uint32_t)b;
  return 128;
}

/* C only: SCon{c, text} where `c` holds the bytes of one code point or more (text in source order). */
static StrBuf rust_lexer_push_text(Heap *heap, StrBuf text, Str c) {
  StrBuf out = text;
  if (!strbuf_push(&out, c)) heap->oom = true;
  return out;
}

/* C only: SCon{c, ""} */
static StrBuf rust_lexer_one(Heap *heap, Str c) {
  StrBuf text = strbuf_new(heap->arena);
  return rust_lexer_push_text(heap, text, c);
}

/* C only: Tk.Tok{kind, loc, gap} <> tokens (tokens in source order). */
static Vec rust_lexer_push(Heap *heap, Vec tokens, RustTokKind kind, RustTokLoc loc, bool gap) {
  Vec out = tokens;
  RustTok *tok = heap_alloc(heap, sizeof *tok);
  if (tok == NULL) return out;
  tok->kind = kind;
  tok->loc = loc;
  tok->gap = gap;
  if (!vec_push(&out, tok)) heap->oom = true;
  return out;
}

/* fail(loc, message): Failed{show_loc(loc) ++ " " ++ message} */
static RustLexerState rust_lexer_fail(Heap *heap, RustTokLoc loc, Str message) {
  Str where = rust_token_show_loc(heap, loc);
  Str head = heap_str_cat(heap, where, str_lit(" "));
  RustLexerState state = rust_lexer_blank(heap, RUST_LEXER_FAILED);
  state.message = heap_str_cat(heap, head, message);
  return state;
}

/* gap(nl): nl >= 2 */
static bool rust_lexer_gap(uint64_t nl) { return nl >= 2; }

/* emit(kind, source, loc, start, tokens, nl): the token at `start`, then Scan with "" and nl = 0. */
static RustLexerState rust_lexer_emit(Heap *heap, RustTokKind kind, Str source, RustTokLoc loc, RustTokLoc start,
                                      Vec tokens, uint64_t nl) {
  bool gap = rust_lexer_gap(nl);
  Vec out = rust_lexer_push(heap, tokens, kind, start, gap);
  RustLexerMode scan = rust_lexer_mode(RUST_LEXER_SCAN, 0);
  StrBuf empty = strbuf_new(heap->arena);
  return rust_lexer_running(heap, scan, source, loc, start, out, empty, 0);
}

/* punct(p, n, source, loc, tokens, nl): emit(Punct{p}, source, advance(loc, n), loc, tokens, nl) */
static RustLexerState rust_lexer_punct(Heap *heap, Str p, uint64_t n, Str source, RustTokLoc loc, Vec tokens,
                                       uint64_t nl) {
  RustTokKind kind = rust_token_kind(RUST_TOK_PUNCT, p);
  RustTokLoc next = rust_token_advance(heap, loc, n);
  return rust_lexer_emit(heap, kind, source, next, loc, tokens, nl);
}

/* is_digit: Char.to_u32(c) in 48 .. 57 */
static bool rust_lexer_is_digit(uint32_t c) { return c >= 48 && c <= 57; }

/* is_ident_start: Char.is_alpha (the ASCII letters) or '_' */
static bool rust_lexer_is_ident_start(uint32_t c) { return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == '_'; }

static bool rust_lexer_is_ident_char(uint32_t c) { return rust_lexer_is_ident_start(c) || rust_lexer_is_digit(c); }

/* is_punct: SCon{c, ""} occurs in the punctuation string; a NUL Char does not. */
static bool rust_lexer_is_punct(uint32_t c) {
  static const char set[] = "()[]{},;:.=<>+-*/%!&|^?#@$~";
  size_t i = 0;
  while (set[i] != 0 && (uint32_t)(unsigned char)set[i] != c) i++;
  return set[i] != 0;
}

/* line_kind(kind, text): 0 Comment, 1 DocOuter, any other DocInner. */
static RustTokKind rust_lexer_line_kind(uint64_t kind, Str text) {
  if (kind == 0) return rust_token_kind(RUST_TOK_COMMENT, text);
  if (kind == 1) return rust_token_kind(RUST_TOK_DOC_OUTER, text);
  return rust_token_kind(RUST_TOK_DOC_INNER, text);
}

/* block_close(p, ...): at depth 0 the comment ends as Comment{""}, else the scan stays in Block{p}. */
static RustLexerState rust_lexer_block_close(Heap *heap, uint64_t p, Str source, RustTokLoc loc, RustTokLoc start,
                                             Vec tokens, uint64_t nl) {
  RustLexerMode mode = rust_lexer_mode(RUST_LEXER_BLOCK, p);
  StrBuf empty = strbuf_new(heap->arena);
  if (p == 0) {
    RustTokKind kind = rust_token_kind(RUST_TOK_COMMENT, str_empty());
    return rust_lexer_emit(heap, kind, source, loc, start, tokens, nl);
  }
  return rust_lexer_running(heap, mode, source, loc, start, tokens, empty, nl);
}

/* block_end(depth, ...): `*` `/` closes one level; depth 0 has no level to close. */
static RustLexerState rust_lexer_block_end(Heap *heap, uint64_t depth, Str source, RustTokLoc loc, RustTokLoc start,
                                           Vec tokens, uint64_t nl) {
  if (depth == 0) return rust_lexer_fail(heap, start, str_lit("invalid block comment depth"));
  return rust_lexer_block_close(heap, depth - 1, source, loc, start, tokens, nl);
}

/* C only: Running{mode, rest(source, n), advance(loc, n), loc, tokens, "", nl}, the scan cases that
 * enter a mode past an ASCII prefix of n bytes. */
static RustLexerState rust_lexer_open(Heap *heap, RustLexerMode mode, size_t n, Str source, RustTokLoc loc,
                                      Vec tokens, uint64_t nl) {
  Str tail = rust_lexer_rest(source, n);
  RustTokLoc next = rust_token_advance(heap, loc, n);
  StrBuf empty = strbuf_new(heap->arena);
  return rust_lexer_running(heap, mode, tail, next, loc, tokens, empty, nl);
}

/* C only: the multi-character punct cases of scan, in the Bend case order. */
static const char *const rust_lexer_puncts[] = {"::", "->", "=>", "==", "!=", "<=", ">=",
                                                "&&", "||", "..=", "..", "+=", "-=", "*="};
#define RUST_LEXER_PUNCTS (sizeof rust_lexer_puncts / sizeof rust_lexer_puncts[0])

/* C only: Running{mode, tail, advance(loc, 1), loc, tokens, SCon{c, ""}, nl}, the Number and Word
 * starts of scan. */
static RustLexerState rust_lexer_begin(Heap *heap, RustLexerModeKind kind, Str c, Str tail, RustTokLoc loc,
                                       Vec tokens, uint64_t nl) {
  RustLexerMode mode = rust_lexer_mode(kind, 0);
  RustTokLoc next = rust_token_advance(heap, loc, 1);
  StrBuf text = rust_lexer_one(heap, c);
  return rust_lexer_running(heap, mode, tail, next, loc, tokens, text, nl);
}

/* scan(source, loc, tokens, nl): the Scan mode; the cases in the Bend order. */
static RustLexerState rust_lexer_scan(Heap *heap, Str source, RustTokLoc loc, Vec tokens, uint64_t nl) {
  int c0 = rust_lexer_at(source, 0);
  int c1 = rust_lexer_at(source, 1);
  int c2 = rust_lexer_at(source, 2);
  int c3 = rust_lexer_at(source, 3);
  RustLexerMode scan = rust_lexer_mode(RUST_LEXER_SCAN, 0);
  if (source.len == 0) {
    RustTokKind eof = rust_token_kind(RUST_TOK_EOF, str_empty());
    bool gap = rust_lexer_gap(nl);
    RustLexerState done = rust_lexer_blank(heap, RUST_LEXER_FINISHED);
    done.tokens = rust_lexer_push(heap, tokens, eof, loc, gap);
    return done;
  }
  if (c0 == ' ' || c0 == '\t' || c0 == '\r') return rust_lexer_open(heap, scan, 1, source, loc, tokens, nl);
  if (c0 == '\n') {
    Str tail = rust_lexer_rest(source, 1);
    RustTokLoc next = rust_token_next_line(heap, loc);
    StrBuf empty = strbuf_new(heap->arena);
    return rust_lexer_running(heap, scan, tail, next, loc, tokens, empty, nl + 1);
  }
  if (c0 == '/' && c1 == '/' && c2 == '/' && c3 == '/') {
    RustLexerMode line = rust_lexer_mode(RUST_LEXER_LINE, 0);
    return rust_lexer_open(heap, line, 4, source, loc, tokens, nl);
  }
  if (c0 == '/' && c1 == '/' && c2 == '/') {
    RustLexerMode line = rust_lexer_mode(RUST_LEXER_LINE, 1);
    return rust_lexer_open(heap, line, 3, source, loc, tokens, nl);
  }
  if (c0 == '/' && c1 == '/' && c2 == '!') {
    RustLexerMode line = rust_lexer_mode(RUST_LEXER_LINE, 2);
    return rust_lexer_open(heap, line, 3, source, loc, tokens, nl);
  }
  if (c0 == '/' && c1 == '/') {
    RustLexerMode line = rust_lexer_mode(RUST_LEXER_LINE, 0);
    return rust_lexer_open(heap, line, 2, source, loc, tokens, nl);
  }
  if (c0 == '/' && c1 == '*') {
    RustLexerMode block = rust_lexer_mode(RUST_LEXER_BLOCK, 1);
    return rust_lexer_open(heap, block, 2, source, loc, tokens, nl);
  }
  if (c0 == 'r' && c1 == '#') {
    RustLexerMode raw = rust_lexer_mode(RUST_LEXER_RAW, 0);
    return rust_lexer_open(heap, raw, 2, source, loc, tokens, nl);
  }
  if (c0 == '"') {
    RustLexerMode str = rust_lexer_mode(RUST_LEXER_STR, 0);
    return rust_lexer_open(heap, str, 1, source, loc, tokens, nl);
  }
  if (c0 == '\'') {
    Str after_quote = rust_lexer_rest(source, 1);
    Str after_slash = rust_lexer_rest(source, 2);
    size_t esc = rust_lexer_unit(after_slash);
    size_t one = rust_lexer_unit(after_quote);
    RustLexerMode life = rust_lexer_mode(RUST_LEXER_LIFE, 0);
    /* '\'' '\\' c '\'': ChrLit{SCon{'\\', SCon{c, ""}}}, the bytes after the quote. */
    if (c1 == '\\' && esc > 0 && rust_lexer_at(source, 2 + esc) == '\'') {
      Str body = rust_lexer_slice(source, 1, 1 + esc);
      RustTokKind kind = rust_token_kind(RUST_TOK_CHR_LIT, body);
      Str tail = rust_lexer_rest(source, 3 + esc);
      RustTokLoc next = rust_token_advance(heap, loc, 4);
      return rust_lexer_emit(heap, kind, tail, next, loc, tokens, nl);
    }
    /* '\'' c '\'': ChrLit{SCon{c, ""}} */
    if (one > 0 && rust_lexer_at(source, 1 + one) == '\'') {
      Str body = rust_lexer_slice(source, 1, one);
      RustTokKind kind = rust_token_kind(RUST_TOK_CHR_LIT, body);
      Str tail = rust_lexer_rest(source, 2 + one);
      RustTokLoc next = rust_token_advance(heap, loc, 3);
      return rust_lexer_emit(heap, kind, tail, next, loc, tokens, nl);
    }
    return rust_lexer_open(heap, life, 1, source, loc, tokens, nl);
  }
  for (size_t i = 0; i < RUST_LEXER_PUNCTS; i++) {
    Str p = str_lit(rust_lexer_puncts[i]);
    if (str_starts_with(source, p)) {
      Str tail = rust_lexer_rest(source, p.len);
      return rust_lexer_punct(heap, p, p.len, tail, loc, tokens, nl);
    }
  }
  {
    size_t u = rust_lexer_unit(source);
    Str c = rust_lexer_slice(source, 0, u);
    Str tail = rust_lexer_rest(source, u);
    uint32_t code = rust_lexer_code(source);
    Str head;
    Str message;
    if (rust_lexer_is_digit(code)) return rust_lexer_begin(heap, RUST_LEXER_NUMBER, c, tail, loc, tokens, nl);
    if (rust_lexer_is_ident_start(code)) return rust_lexer_begin(heap, RUST_LEXER_WORD, c, tail, loc, tokens, nl);
    if (rust_lexer_is_punct(code)) return rust_lexer_punct(heap, c, 1, tail, loc, tokens, nl);
    head = heap_str_cat(heap, str_lit("unexpected character '"), c);
    message = heap_str_cat(heap, head, str_lit("'"));
    return rust_lexer_fail(heap, loc, message);
  }
}

/* C only: the Word, Raw, Number and Life cases of advance_state: an ident Char extends the text; any
 * other Char, or the end, emits `tag` with the text and leaves that Char to the scan. */
static RustLexerState rust_lexer_word(Heap *heap, RustLexerMode mode, RustTokKindTag tag, Str source, RustTokLoc loc,
                                      RustTokLoc start, Vec tokens, StrBuf text, uint64_t nl) {
  uint32_t code = rust_lexer_code(source);
  Str body;
  RustTokKind kind;
  if (source.len > 0 && rust_lexer_is_ident_char(code)) {
    size_t u = rust_lexer_unit(source);
    Str c = rust_lexer_slice(source, 0, u);
    Str tail = rust_lexer_rest(source, u);
    RustTokLoc next = rust_token_advance(heap, loc, 1);
    StrBuf more = rust_lexer_push_text(heap, text, c);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, more, nl);
  }
  body = strbuf_str(&text);
  kind = rust_token_kind(tag, body);
  return rust_lexer_emit(heap, kind, source, loc, start, tokens, nl);
}

/* C only: the Str cases of advance_state. An escape keeps `\` and the next Char in the text. */
static RustLexerState rust_lexer_string(Heap *heap, RustLexerMode mode, Str source, RustTokLoc loc, RustTokLoc start,
                                        Vec tokens, StrBuf text, uint64_t nl) {
  int c0 = rust_lexer_at(source, 0);
  Str after = rust_lexer_rest(source, 1);
  size_t esc = rust_lexer_unit(after);
  if (c0 == '\\' && esc > 0) {
    Str pair = rust_lexer_slice(source, 0, 1 + esc);
    Str tail = rust_lexer_rest(source, 1 + esc);
    RustTokLoc next = rust_token_advance(heap, loc, 2);
    StrBuf more = rust_lexer_push_text(heap, text, pair);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, more, nl);
  }
  if (c0 == '"') {
    Str body = strbuf_str(&text);
    RustTokKind kind = rust_token_kind(RUST_TOK_STR, body);
    RustTokLoc next = rust_token_advance(heap, loc, 1);
    return rust_lexer_emit(heap, kind, after, next, start, tokens, nl);
  }
  if (c0 == '\n') {
    RustTokLoc next = rust_token_next_line(heap, loc);
    StrBuf more = rust_lexer_push_text(heap, text, str_lit("\n"));
    return rust_lexer_running(heap, mode, after, next, start, tokens, more, nl);
  }
  if (source.len > 0) {
    size_t u = rust_lexer_unit(source);
    Str c = rust_lexer_slice(source, 0, u);
    Str tail = rust_lexer_rest(source, u);
    RustTokLoc next = rust_token_advance(heap, loc, 1);
    StrBuf more = rust_lexer_push_text(heap, text, c);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, more, nl);
  }
  return rust_lexer_fail(heap, start, str_lit("unterminated string literal"));
}

/* C only: the Line{kind} cases of advance_state. The newline ends the comment token and starts the
 * newline count at 1. */
static RustLexerState rust_lexer_line(Heap *heap, RustLexerMode mode, Str source, RustTokLoc loc, RustTokLoc start,
                                      Vec tokens, StrBuf text, uint64_t nl) {
  int c0 = rust_lexer_at(source, 0);
  Str body = strbuf_str(&text);
  RustTokKind kind = rust_lexer_line_kind(mode.n, body);
  if (c0 == '\n') {
    bool gap = rust_lexer_gap(nl);
    Vec out = rust_lexer_push(heap, tokens, kind, start, gap);
    Str tail = rust_lexer_rest(source, 1);
    RustTokLoc next = rust_token_next_line(heap, loc);
    RustLexerMode scan = rust_lexer_mode(RUST_LEXER_SCAN, 0);
    StrBuf empty = strbuf_new(heap->arena);
    return rust_lexer_running(heap, scan, tail, next, loc, out, empty, 1);
  }
  if (source.len > 0) {
    size_t u = rust_lexer_unit(source);
    Str c = rust_lexer_slice(source, 0, u);
    Str tail = rust_lexer_rest(source, u);
    RustTokLoc next = rust_token_advance(heap, loc, 1);
    StrBuf more = rust_lexer_push_text(heap, text, c);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, more, nl);
  }
  return rust_lexer_emit(heap, kind, source, loc, start, tokens, nl);
}

/* C only: the Block{depth} cases of advance_state; the text stays "". */
static RustLexerState rust_lexer_block(Heap *heap, RustLexerMode mode, Str source, RustTokLoc loc, RustTokLoc start,
                                       Vec tokens, uint64_t nl) {
  int c0 = rust_lexer_at(source, 0);
  int c1 = rust_lexer_at(source, 1);
  StrBuf empty = strbuf_new(heap->arena);
  if (c0 == '*' && c1 == '/') {
    Str tail = rust_lexer_rest(source, 2);
    RustTokLoc next = rust_token_advance(heap, loc, 2);
    return rust_lexer_block_end(heap, mode.n, tail, next, start, tokens, nl);
  }
  if (c0 == '/' && c1 == '*') {
    RustLexerMode inner = rust_lexer_mode(RUST_LEXER_BLOCK, mode.n + 1);
    Str tail = rust_lexer_rest(source, 2);
    RustTokLoc next = rust_token_advance(heap, loc, 2);
    return rust_lexer_running(heap, inner, tail, next, start, tokens, empty, nl);
  }
  if (c0 == '\n') {
    Str tail = rust_lexer_rest(source, 1);
    RustTokLoc next = rust_token_next_line(heap, loc);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, empty, nl);
  }
  if (source.len > 0) {
    size_t u = rust_lexer_unit(source);
    Str tail = rust_lexer_rest(source, u);
    RustTokLoc next = rust_token_advance(heap, loc, 1);
    return rust_lexer_running(heap, mode, tail, next, start, tokens, empty, nl);
  }
  return rust_lexer_fail(heap, start, str_lit("unterminated block comment"));
}

/* advance_state(mode, source, loc, start, tokens, text, nl): one step of the mode. */
static RustLexerState rust_lexer_advance_state(Heap *heap, RustLexerMode mode, Str source, RustTokLoc loc,
                                               RustTokLoc start, Vec tokens, StrBuf text, uint64_t nl) {
  switch (mode.kind) {
  case RUST_LEXER_SCAN:
    return rust_lexer_scan(heap, source, loc, tokens, nl);
  case RUST_LEXER_WORD:
    return rust_lexer_word(heap, mode, RUST_TOK_IDENT, source, loc, start, tokens, text, nl);
  case RUST_LEXER_RAW:
    return rust_lexer_word(heap, mode, RUST_TOK_RAW_IDENT, source, loc, start, tokens, text, nl);
  case RUST_LEXER_NUMBER:
    return rust_lexer_word(heap, mode, RUST_TOK_INT, source, loc, start, tokens, text, nl);
  case RUST_LEXER_STR:
    return rust_lexer_string(heap, mode, source, loc, start, tokens, text, nl);
  case RUST_LEXER_LIFE:
    return rust_lexer_word(heap, mode, RUST_TOK_LIFETIME, source, loc, start, tokens, text, nl);
  case RUST_LEXER_LINE:
    return rust_lexer_line(heap, mode, source, loc, start, tokens, text, nl);
  case RUST_LEXER_BLOCK:
    return rust_lexer_block(heap, mode, source, loc, start, tokens, nl);
  }
  /* Unreachable: the switch covers every Mode. */
  return rust_lexer_scan(heap, source, loc, tokens, nl);
}

/* step(state): a Running state advances; Finished and Failed stay. */
static RustLexerState rust_lexer_step(Heap *heap, RustLexerState state) {
  switch (state.kind) {
  case RUST_LEXER_RUNNING:
    return rust_lexer_advance_state(heap, state.mode, state.source, state.loc, state.start, state.tokens, state.text,
                                    state.nl);
  case RUST_LEXER_FINISHED:
  case RUST_LEXER_FAILED:
    return state;
  }
  return state;
}

/* run(state): the Bend tail recursion as a loop. Each step consumes a code point or moves to Scan,
 * which consumes one or stops, so it ends. */
static RustLexerState rust_lexer_run(Heap *heap, RustLexerState state) {
  RustLexerState s = state;
  while (s.kind == RUST_LEXER_RUNNING) s = rust_lexer_step(heap, s);
  return s;
}

/* lex(source): run(Running{Scan{}, source, start(), start(), [], "", 0}) */
RustLexResult rust_lexer_lex(Heap *heap, Str source) {
  RustTokLoc origin = rust_token_start(heap);
  RustLexerMode scan = rust_lexer_mode(RUST_LEXER_SCAN, 0);
  Vec none = vec_new(heap->arena);
  StrBuf empty = strbuf_new(heap->arena);
  RustLexerState first = rust_lexer_running(heap, scan, source, origin, origin, none, empty, 0);
  RustLexerState last = rust_lexer_run(heap, first);
  RustLexResult result;
  result.ok = last.kind == RUST_LEXER_FINISHED;
  result.tokens = last.tokens;
  result.message = last.message;
  return result;
}
