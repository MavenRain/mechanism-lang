/* Port of bend2/rust/fragment.bend: the fragment pass, a walk over the lexer tokens that runs
 * before the parser. It refuses each construct outside the house-convention Rust subset and gives
 * the position and the construct name (see c/rust/FRONTEND.md for the rule table). The
 * definitions keep the Bend names (prefix fragment_) and the Bend order; the C only helpers stand
 * just before their first use.
 *
 * The Bend List<CT> is a Vec of FragmentCt pointers, and the Bend List<Refusal> is a Vec of
 * FragmentRefusal pointers in source order. The Bend walk recurses once per construct and conses
 * each refusal in front of the rest of the walk, so the C walk visits the same constructs front to
 * back and pushes the same refusals in the same order. */

#include "rust.h"

/* show(r) */
Str fragment_show(Heap *heap, const FragmentRefusal *r) {
  StrBuf buf = strbuf_new(heap->arena);
  bool ok = strbuf_push(&buf, rust_token_show_loc(heap, r->loc)) && strbuf_push(&buf, str_lit(" ")) &&
            strbuf_push(&buf, r->what);
  if (!ok) heap->oom = true;
  return strbuf_str(&buf);
}

/* C: the token classes. CKW is a keyword that cannot end an expression. CWORD is any other
 * identifier (`self`, `true` and raw identifiers included). CCLOSE is `)`, `]` or `?`. */
typedef enum {
  FRAGMENT_CLOOP,
  FRAGMENT_CWHILE,
  FRAGMENT_CFOR,
  FRAGMENT_CMUT,
  FRAGMENT_CUNSAFE,
  FRAGMENT_CAS,
  FRAGMENT_CDYN,
  FRAGMENT_CASYNC,
  FRAGMENT_CAWAIT,
  FRAGMENT_CIMPL,
  FRAGMENT_CUSE,
  FRAGMENT_CKW,
  FRAGMENT_CWORD,
  FRAGMENT_CLIFE,
  FRAGMENT_CCMT,
  FRAGMENT_CLIT,
  FRAGMENT_CAMP,
  FRAGMENT_CAMP_AMP,
  FRAGMENT_CDOT,
  FRAGMENT_CBANG,
  FRAGMENT_CLBRACK,
  FRAGMENT_CCLOSE,
  FRAGMENT_CLPAREN,
  FRAGMENT_CPATH_SEP,
  FRAGMENT_CLBRACE,
  FRAGMENT_CSEMI,
  FRAGMENT_COTHER
} FragmentC;

/* CT.CT */
typedef struct {
  FragmentC c;
  Str text;
  RustTokLoc loc;
} FragmentCt;

/* Entry.Entry: the C table holds the text as a C string literal. */
typedef struct {
  const char *text;
  FragmentC c;
} FragmentEntry;

/* keywords() */
static const FragmentEntry FRAGMENT_KEYWORDS[] = {
    {"loop", FRAGMENT_CLOOP},   {"while", FRAGMENT_CWHILE},  {"for", FRAGMENT_CFOR},
    {"mut", FRAGMENT_CMUT},     {"unsafe", FRAGMENT_CUNSAFE}, {"as", FRAGMENT_CAS},
    {"dyn", FRAGMENT_CDYN},     {"async", FRAGMENT_CASYNC},  {"await", FRAGMENT_CAWAIT},
    {"impl", FRAGMENT_CIMPL},   {"use", FRAGMENT_CUSE},      {"break", FRAGMENT_CKW},
    {"const", FRAGMENT_CKW},    {"continue", FRAGMENT_CKW},  {"crate", FRAGMENT_CKW},
    {"else", FRAGMENT_CKW},     {"enum", FRAGMENT_CKW},      {"extern", FRAGMENT_CKW},
    {"fn", FRAGMENT_CKW},       {"if", FRAGMENT_CKW},        {"in", FRAGMENT_CKW},
    {"let", FRAGMENT_CKW},      {"match", FRAGMENT_CKW},     {"mod", FRAGMENT_CKW},
    {"move", FRAGMENT_CKW},     {"pub", FRAGMENT_CKW},       {"ref", FRAGMENT_CKW},
    {"return", FRAGMENT_CKW},   {"static", FRAGMENT_CKW},    {"struct", FRAGMENT_CKW},
    {"super", FRAGMENT_CKW},    {"trait", FRAGMENT_CKW},     {"type", FRAGMENT_CKW},
    {"where", FRAGMENT_CKW},    {"Self", FRAGMENT_CKW}};
#define FRAGMENT_KEYWORD_COUNT (sizeof FRAGMENT_KEYWORDS / sizeof FRAGMENT_KEYWORDS[0])

/* puncts() */
static const FragmentEntry FRAGMENT_PUNCTS[] = {
    {"&", FRAGMENT_CAMP},    {"&&", FRAGMENT_CAMP_AMP}, {".", FRAGMENT_CDOT},
    {"!", FRAGMENT_CBANG},   {"[", FRAGMENT_CLBRACK},   {")", FRAGMENT_CCLOSE},
    {"]", FRAGMENT_CCLOSE},  {"?", FRAGMENT_CCLOSE},    {"(", FRAGMENT_CLPAREN},
    {"::", FRAGMENT_CPATH_SEP}, {"{", FRAGMENT_CLBRACE}, {";", FRAGMENT_CSEMI}};
#define FRAGMENT_PUNCT_COUNT (sizeof FRAGMENT_PUNCTS / sizeof FRAGMENT_PUNCTS[0])

/* lookup(text, table, fallback): `table` is the first `count` entries; the search starts at `at`. */
static FragmentC fragment_lookup(Str text, const FragmentEntry *table, size_t count, size_t at,
                                 FragmentC fallback) {
  if (at >= count) return fallback;
  if (str_eq(text, str_lit(table[at].text))) return table[at].c;
  return fragment_lookup(text, table, count, at + 1, fallback);
}

/* C only: CT{c, text, loc} */
static FragmentCt fragment_ct(FragmentC c, Str text, RustTokLoc loc) {
  FragmentCt out = {.c = c, .text = text, .loc = loc};
  return out;
}

/* classify_kind(kind, loc) */
static FragmentCt fragment_classify_kind(RustTokKind kind, RustTokLoc loc) {
  FragmentCt out = fragment_ct(FRAGMENT_COTHER, str_lit(""), loc);
  switch (kind.tag) {
    case RUST_TOK_IDENT:
      out = fragment_ct(fragment_lookup(kind.text, FRAGMENT_KEYWORDS, FRAGMENT_KEYWORD_COUNT, 0,
                                        FRAGMENT_CWORD),
                        kind.text, loc);
      break;
    case RUST_TOK_RAW_IDENT: out = fragment_ct(FRAGMENT_CWORD, kind.text, loc); break;
    case RUST_TOK_LIFETIME: out = fragment_ct(FRAGMENT_CLIFE, kind.text, loc); break;
    case RUST_TOK_INT: out = fragment_ct(FRAGMENT_CLIT, kind.text, loc); break;
    case RUST_TOK_STR: out = fragment_ct(FRAGMENT_CLIT, kind.text, loc); break;
    case RUST_TOK_CHR_LIT: out = fragment_ct(FRAGMENT_CLIT, kind.text, loc); break;
    case RUST_TOK_DOC_OUTER: out = fragment_ct(FRAGMENT_COTHER, kind.text, loc); break;
    case RUST_TOK_DOC_INNER: out = fragment_ct(FRAGMENT_COTHER, kind.text, loc); break;
    case RUST_TOK_COMMENT: out = fragment_ct(FRAGMENT_CCMT, kind.text, loc); break;
    case RUST_TOK_PUNCT:
      out = fragment_ct(fragment_lookup(kind.text, FRAGMENT_PUNCTS, FRAGMENT_PUNCT_COUNT, 0,
                                        FRAGMENT_COTHER),
                        kind.text, loc);
      break;
    case RUST_TOK_EOF: out = fragment_ct(FRAGMENT_COTHER, str_lit(""), loc); break;
  }
  return out;
}

/* classify(ts): `ts` holds the lexer's RustTok pointers; the result holds FragmentCt pointers in
 * the same order (the Bend maps the list front to back). */
static Vec fragment_classify(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  size_t at = 0;
  while (at < ts->len && !heap->oom) {
    VecGet got = vec_get(ts, at);
    if (got.status == VEC_GET_OK) {
      const RustTok *t = got.value;
      FragmentCt *cell = heap_alloc(heap, sizeof *cell);
      if (cell == NULL) heap->oom = true;
      if (cell != NULL) {
        *cell = fragment_classify_kind(t->kind, t->loc);
        if (!vec_push(&out, cell)) heap->oom = true;
      }
    }
    at = at + 1;
  }
  return out;
}

/* ender(c): true when a token of this class can end an expression. A `[` that follows such a
 * token is an index operation. */
static bool fragment_ender(FragmentC c) {
  return c == FRAGMENT_CWORD || c == FRAGMENT_CLIT || c == FRAGMENT_CCLOSE;
}

/* hdr_next(c, hdr): `hdr` is true from `impl` to the next `{` or `;`. There, `for` is the
 * `impl Trait for Type` keyword and not a loop. */
static bool fragment_hdr_next(FragmentC c, bool hdr) {
  if (c == FRAGMENT_CIMPL) return true;
  if (c == FRAGMENT_CLBRACE || c == FRAGMENT_CSEMI) return false;
  return hdr;
}

/* use_next(c, usei): `usei` is true from `use` to the next `;`. There, `as` is a rename and not a
 * cast. */
static bool fragment_use_next(FragmentC c, bool usei) {
  if (c == FRAGMENT_CUSE) return true;
  if (c == FRAGMENT_CSEMI) return false;
  return usei;
}

/* Maybe<Refusal>: `some` false = None. */
typedef struct {
  bool some;
  FragmentRefusal r;
} FragmentMaybeRefusal;

/* C only: Some{Refusal{l, what}} */
static FragmentMaybeRefusal fragment_some(RustTokLoc l, const char *what) {
  FragmentMaybeRefusal out = {.some = true, .r = {.loc = l, .what = str_lit(what)}};
  return out;
}

/* C only: None{} */
static FragmentMaybeRefusal fragment_none(void) {
  FragmentMaybeRefusal out = {.some = false};
  return out;
}

/* one(c, l, e, hdr, usei): the refusal for one token, from its class and the walk state. */
static FragmentMaybeRefusal fragment_one(FragmentC c, RustTokLoc l, bool e, bool hdr, bool usei) {
  FragmentMaybeRefusal out = fragment_none();
  switch (c) {
    case FRAGMENT_CLOOP: out = fragment_some(l, "loop"); break;
    case FRAGMENT_CWHILE: out = fragment_some(l, "while"); break;
    case FRAGMENT_CFOR:
      if (!hdr) out = fragment_some(l, "for");
      break;
    case FRAGMENT_CMUT: out = fragment_some(l, "mut binding"); break;
    case FRAGMENT_CUNSAFE: out = fragment_some(l, "unsafe"); break;
    case FRAGMENT_CAS:
      if (!usei) out = fragment_some(l, "as");
      break;
    case FRAGMENT_CDYN: out = fragment_some(l, "dyn Trait"); break;
    case FRAGMENT_CASYNC: out = fragment_some(l, "async"); break;
    case FRAGMENT_CAWAIT: out = fragment_some(l, ".await"); break;
    case FRAGMENT_CLIFE: out = fragment_some(l, "lifetime"); break;
    case FRAGMENT_CCMT: out = fragment_some(l, "comment"); break;
    case FRAGMENT_CLBRACK:
      if (e) out = fragment_some(l, "indexing");
      break;
    case FRAGMENT_CIMPL:
    case FRAGMENT_CUSE:
    case FRAGMENT_CKW:
    case FRAGMENT_CWORD:
    case FRAGMENT_CLIT:
    case FRAGMENT_CAMP:
    case FRAGMENT_CAMP_AMP:
    case FRAGMENT_CDOT:
    case FRAGMENT_CBANG:
    case FRAGMENT_CCLOSE:
    case FRAGMENT_CLPAREN:
    case FRAGMENT_CPATH_SEP:
    case FRAGMENT_CLBRACE:
    case FRAGMENT_CSEMI:
    case FRAGMENT_COTHER: break;
  }
  return out;
}

/* Maybe<String>: `some` false = None. */
typedef struct {
  bool some;
  Str what;
} FragmentMaybeStr;

/* C only: Some{what} */
static FragmentMaybeStr fragment_some_str(Str what) {
  FragmentMaybeStr out = {.some = true, .what = what};
  return out;
}

/* method_what(name): the construct name for a refused method call `.name(`. */
static FragmentMaybeStr fragment_method_what(Str name) {
  FragmentMaybeStr none = {.some = false};
  if (str_eq(name, str_lit("unwrap"))) return fragment_some_str(str_lit(".unwrap()"));
  if (str_eq(name, str_lit("expect"))) return fragment_some_str(str_lit(".expect("));
  if (str_eq(name, str_lit("scan"))) return fragment_some_str(str_lit(".scan("));
  return none;
}

/* macro_what(name): the construct name for a macro call `name!`. Only `vec!` is permitted. */
static FragmentMaybeStr fragment_macro_what(Heap *heap, Str name) {
  FragmentMaybeStr none = {.some = false};
  if (str_eq(name, str_lit("vec"))) return none;
  StrBuf buf = strbuf_new(heap->arena);
  bool named = str_eq(name, str_lit("panic")) || str_eq(name, str_lit("assert")) ||
               str_eq(name, str_lit("assert_eq"));
  bool ok = true;
  if (!named) ok = strbuf_push(&buf, str_lit("macro "));
  ok = ok && strbuf_push(&buf, name) && strbuf_push(&buf, str_lit("!"));
  if (!ok) heap->oom = true;
  return fragment_some_str(strbuf_str(&buf));
}

/* at(l, m) */
static FragmentMaybeRefusal fragment_at(RustTokLoc l, FragmentMaybeStr m) {
  FragmentMaybeRefusal out = {.some = m.some, .r = {.loc = l, .what = m.what}};
  return out;
}

/* push(m, tail): the C walk pushes `m` before it walks the tail, so the Vec keeps source order. */
static void fragment_push(Heap *heap, FragmentMaybeRefusal m, Vec *out) {
  if (!m.some) return;
  FragmentRefusal *cell = heap_alloc(heap, sizeof *cell);
  if (cell == NULL) {
    heap->oom = true;
    return;
  }
  *cell = m.r;
  if (!vec_push(out, cell)) heap->oom = true;
}

/* C only: the CT at `at`, NULL past the end (a multi-token case never matches past the end). */
static const FragmentCt *fragment_nth(const Vec *ts, size_t at) {
  VecGet got = vec_get(ts, at);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

/* C only: true when the CTs from `at` on start with the `count` classes of `cs`. */
static bool fragment_is_at(const Vec *ts, size_t at, const FragmentC *cs, size_t count) {
  size_t i = 0;
  bool ok = true;
  while (ok && i < count) {
    const FragmentCt *t = fragment_nth(ts, at + i);
    ok = t != NULL && t->c == cs[i];
    i = i + 1;
  }
  return ok;
}

/* C only: one walk step: the CTs it consumes and the walk state after it. */
typedef struct {
  size_t taken;
  bool e;
  bool hdr;
  bool usei;
} FragmentStep;

/* C only: FragmentStep{taken, e, hdr, usei} */
static FragmentStep fragment_step_of(size_t taken, bool e, bool hdr, bool usei) {
  FragmentStep out = {.taken = taken, .e = e, .hdr = hdr, .usei = usei};
  return out;
}

/* C only: one case of walk on the non-empty view at `at` (`t` is its head CT), in the Bend case
 * order: the cases with two or three tokens come first; each one consumes all of its tokens. */
static FragmentStep fragment_step(Heap *heap, const Vec *ts, size_t at, const FragmentCt *t,
                                  FragmentStep s, Vec *out) {
  static const FragmentC amp_life_mut[] = {FRAGMENT_CAMP, FRAGMENT_CLIFE, FRAGMENT_CMUT};
  static const FragmentC amp_mut[] = {FRAGMENT_CAMP, FRAGMENT_CMUT};
  static const FragmentC amp_amp_mut[] = {FRAGMENT_CAMP_AMP, FRAGMENT_CMUT};
  static const FragmentC dot_await[] = {FRAGMENT_CDOT, FRAGMENT_CAWAIT};
  static const FragmentC dot_word_paren[] = {FRAGMENT_CDOT, FRAGMENT_CWORD, FRAGMENT_CLPAREN};
  static const FragmentC dot_word_sep[] = {FRAGMENT_CDOT, FRAGMENT_CWORD, FRAGMENT_CPATH_SEP};
  static const FragmentC word_bang[] = {FRAGMENT_CWORD, FRAGMENT_CBANG};
  const FragmentCt *t1 = fragment_nth(ts, at + 1);
  if (fragment_is_at(ts, at, amp_life_mut, 3)) {
    fragment_push(heap, fragment_some(t->loc, "&mut"), out);
    fragment_push(heap, fragment_some(t1->loc, "lifetime"), out);
    return fragment_step_of(3, false, s.hdr, s.usei);
  }
  if (fragment_is_at(ts, at, amp_mut, 2)) {
    fragment_push(heap, fragment_some(t->loc, "&mut"), out);
    return fragment_step_of(2, false, s.hdr, s.usei);
  }
  if (fragment_is_at(ts, at, amp_amp_mut, 2)) {
    fragment_push(heap, fragment_some(rust_token_advance(heap, t->loc, 1), "&mut"), out);
    return fragment_step_of(2, false, s.hdr, s.usei);
  }
  if (fragment_is_at(ts, at, dot_await, 2)) {
    fragment_push(heap, fragment_some(t->loc, ".await"), out);
    return fragment_step_of(2, true, s.hdr, s.usei);
  }
  if (fragment_is_at(ts, at, dot_word_paren, 3) || fragment_is_at(ts, at, dot_word_sep, 3)) {
    fragment_push(heap, fragment_at(t->loc, fragment_method_what(t1->text)), out);
    return fragment_step_of(3, false, s.hdr, s.usei);
  }
  if (fragment_is_at(ts, at, word_bang, 2)) {
    fragment_push(heap, fragment_at(t->loc, fragment_macro_what(heap, t->text)), out);
    return fragment_step_of(2, false, s.hdr, s.usei);
  }
  fragment_push(heap, fragment_one(t->c, t->loc, s.e, s.hdr, s.usei), out);
  return fragment_step_of(1, fragment_ender(t->c), fragment_hdr_next(t->c, s.hdr),
                          fragment_use_next(t->c, s.usei));
}

/* walk(ts, e, hdr, usei): `e` is true when the previous token can end an expression. One
 * construct gives one refusal (fragment_step). */
static Vec fragment_walk(Heap *heap, const Vec *ts, bool e, bool hdr, bool usei) {
  Vec out = vec_new(heap->arena);
  FragmentStep s = fragment_step_of(0, e, hdr, usei);
  size_t at = 0;
  const FragmentCt *t = fragment_nth(ts, at);
  while (t != NULL && !heap->oom) {
    s = fragment_step(heap, ts, at, t, s, &out);
    at = at + s.taken;
    t = fragment_nth(ts, at);
  }
  return out;
}

/* refusals(ts): all refusals for a token list, in source order. An empty Vec means that the source
 * is in the fragment. */
Vec fragment_refusals(Heap *heap, const Vec *ts) {
  Vec cts = fragment_classify(heap, ts);
  return fragment_walk(heap, &cts, false, false, false);
}
