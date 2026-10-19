/* Port of rust/parser.bend: the recursive descent parser from Rust tokens to the AST. The
 * definitions keep the Bend names (prefix rust_parser_) and the Bend order; the C only helpers stand
 * just before their first use.
 *
 * List<&2, PT> is a view RustPts {all, at}: the RustPt pointers of `all` from index `at` on, so the
 * Bend `rest` of a match is the view one index on and Nil is `at >= len`. classify builds `all` once.
 * Parsed<A>{value, rest} is one C struct per A, declared before its first use; pv and pr are its two
 * field reads. A Bend Result<String, A> is a C struct {ok, value, message}. prec gives a uint64_t
 * (the Bend Nat values are 2..10). Each struct value is bound to a local before it is passed on
 * (trap T-202). A failed allocation sets heap->oom; the caller checks it. */
#include "rust.h"

/* K: the parser-local token kinds, in the Bend declaration order. */
typedef enum {
  RUST_PK_NAME,
  RUST_PK_RAW_NAME,
  RUST_PK_INT_L,
  RUST_PK_STR_L,
  RUST_PK_CHR_L,
  RUST_PK_LIFE_T,
  RUST_PK_DOC_O,
  RUST_PK_DOC_I,
  RUST_PK_CMT,
  RUST_PK_KFN,
  RUST_PK_KPUB,
  RUST_PK_KCRATE,
  RUST_PK_KMOD,
  RUST_PK_KUSE,
  RUST_PK_KSTRUCT,
  RUST_PK_KENUM,
  RUST_PK_KIMPL,
  RUST_PK_KTRAIT,
  RUST_PK_KFOR,
  RUST_PK_KCONST,
  RUST_PK_KTYPE,
  RUST_PK_KLET,
  RUST_PK_KMATCH,
  RUST_PK_KIF,
  RUST_PK_KELSE,
  RUST_PK_KSELF_V,
  RUST_PK_KSELF_T,
  RUST_PK_KSUPER,
  RUST_PK_KAS,
  RUST_PK_KTRUE,
  RUST_PK_KFALSE,
  RUST_PK_KMOVE,
  RUST_PK_KUNDER,
  RUST_PK_LPAREN,
  RUST_PK_RPAREN,
  RUST_PK_LBRACK,
  RUST_PK_RBRACK,
  RUST_PK_LBRACE,
  RUST_PK_RBRACE,
  RUST_PK_COMMA,
  RUST_PK_SEMI,
  RUST_PK_COLON,
  RUST_PK_DOT,
  RUST_PK_EQ,
  RUST_PK_LT,
  RUST_PK_GT,
  RUST_PK_PLUS,
  RUST_PK_MINUS,
  RUST_PK_STAR,
  RUST_PK_SLASH,
  RUST_PK_PERCENT,
  RUST_PK_BANG,
  RUST_PK_AMP,
  RUST_PK_PIPE,
  RUST_PK_CARET,
  RUST_PK_QUESTION,
  RUST_PK_HASH,
  RUST_PK_PATH_SEP,
  RUST_PK_ARROW,
  RUST_PK_FAT_ARROW,
  RUST_PK_EQ_EQ,
  RUST_PK_NE,
  RUST_PK_LE,
  RUST_PK_GE,
  RUST_PK_AND_AND,
  RUST_PK_OR_OR,
  RUST_PK_DOT_DOT,
  RUST_PK_OTHER_P,
  RUST_PK_EOF_K
} RustPk;

/* Info.Info: `text` is the source text (names, literal bodies, punctuation, doc lines). */
typedef struct {
  Str text;
  RustTokLoc loc;
  bool gap;
} RustPInfo;

/* PT.PT */
typedef struct {
  RustPk k;
  RustPInfo i;
} RustPt;

/* Entry.Entry: the C table holds the text as a C string literal. */
typedef struct {
  const char *text;
  RustPk k;
} RustPEntry;

/* List<&2, PT> (see the file comment). */
typedef struct {
  const Vec *all;
  size_t at;
} RustPts;

/* keywords() */
static const RustPEntry RUST_PARSER_KEYWORDS[] = {
    {"fn", RUST_PK_KFN},        {"pub", RUST_PK_KPUB},       {"crate", RUST_PK_KCRATE},
    {"mod", RUST_PK_KMOD},      {"use", RUST_PK_KUSE},       {"struct", RUST_PK_KSTRUCT},
    {"enum", RUST_PK_KENUM},    {"impl", RUST_PK_KIMPL},     {"trait", RUST_PK_KTRAIT},
    {"for", RUST_PK_KFOR},      {"const", RUST_PK_KCONST},   {"type", RUST_PK_KTYPE},
    {"let", RUST_PK_KLET},      {"match", RUST_PK_KMATCH},   {"if", RUST_PK_KIF},
    {"else", RUST_PK_KELSE},    {"self", RUST_PK_KSELF_V},   {"Self", RUST_PK_KSELF_T},
    {"super", RUST_PK_KSUPER},  {"as", RUST_PK_KAS},         {"true", RUST_PK_KTRUE},
    {"false", RUST_PK_KFALSE},  {"move", RUST_PK_KMOVE},     {"_", RUST_PK_KUNDER}};
#define RUST_PARSER_KEYWORD_COUNT (sizeof RUST_PARSER_KEYWORDS / sizeof RUST_PARSER_KEYWORDS[0])

/* puncts() */
static const RustPEntry RUST_PARSER_PUNCTS[] = {
    {"(", RUST_PK_LPAREN},     {")", RUST_PK_RPAREN},     {"[", RUST_PK_LBRACK},
    {"]", RUST_PK_RBRACK},     {"{", RUST_PK_LBRACE},     {"}", RUST_PK_RBRACE},
    {",", RUST_PK_COMMA},      {";", RUST_PK_SEMI},       {":", RUST_PK_COLON},
    {".", RUST_PK_DOT},        {"=", RUST_PK_EQ},         {"<", RUST_PK_LT},
    {">", RUST_PK_GT},         {"+", RUST_PK_PLUS},       {"-", RUST_PK_MINUS},
    {"*", RUST_PK_STAR},       {"/", RUST_PK_SLASH},      {"%", RUST_PK_PERCENT},
    {"!", RUST_PK_BANG},       {"&", RUST_PK_AMP},        {"|", RUST_PK_PIPE},
    {"^", RUST_PK_CARET},      {"?", RUST_PK_QUESTION},   {"#", RUST_PK_HASH},
    {"::", RUST_PK_PATH_SEP},  {"->", RUST_PK_ARROW},     {"=>", RUST_PK_FAT_ARROW},
    {"==", RUST_PK_EQ_EQ},     {"!=", RUST_PK_NE},        {"<=", RUST_PK_LE},
    {">=", RUST_PK_GE},        {"&&", RUST_PK_AND_AND},   {"||", RUST_PK_OR_OR},
    {"..", RUST_PK_DOT_DOT}};
#define RUST_PARSER_PUNCT_COUNT (sizeof RUST_PARSER_PUNCTS / sizeof RUST_PARSER_PUNCTS[0])

/* lookup(text, table, fallback): the first entry whose text equals `text`, from index `at` on. */
static RustPk rust_parser_lookup(Str text, const RustPEntry *table, size_t count, size_t at,
                                 RustPk fallback) {
  if (at >= count) return fallback;
  if (str_eq(text, str_lit(table[at].text))) return table[at].k;
  return rust_parser_lookup(text, table, count, at + 1, fallback);
}

/* C only: PT{k, Info{text, loc, gap}} */
static RustPt rust_parser_pt(RustPk k, Str text, RustTokLoc loc, bool gap) {
  RustPt pt;
  pt.k = k;
  pt.i.text = text;
  pt.i.loc = loc;
  pt.i.gap = gap;
  return pt;
}

/* classify_kind(kind, loc, gap) */
static RustPt rust_parser_classify_kind(RustTokKind kind, RustTokLoc loc, bool gap) {
  switch (kind.tag) {
  case RUST_TOK_IDENT: {
    RustPk k = rust_parser_lookup(kind.text, RUST_PARSER_KEYWORDS, RUST_PARSER_KEYWORD_COUNT, 0,
                                  RUST_PK_NAME);
    return rust_parser_pt(k, kind.text, loc, gap);
  }
  case RUST_TOK_RAW_IDENT:
    return rust_parser_pt(RUST_PK_RAW_NAME, kind.text, loc, gap);
  case RUST_TOK_LIFETIME:
    return rust_parser_pt(RUST_PK_LIFE_T, kind.text, loc, gap);
  case RUST_TOK_INT:
    return rust_parser_pt(RUST_PK_INT_L, kind.text, loc, gap);
  case RUST_TOK_STR:
    return rust_parser_pt(RUST_PK_STR_L, kind.text, loc, gap);
  case RUST_TOK_CHR_LIT:
    return rust_parser_pt(RUST_PK_CHR_L, kind.text, loc, gap);
  case RUST_TOK_DOC_OUTER:
    return rust_parser_pt(RUST_PK_DOC_O, kind.text, loc, gap);
  case RUST_TOK_DOC_INNER:
    return rust_parser_pt(RUST_PK_DOC_I, kind.text, loc, gap);
  case RUST_TOK_COMMENT:
    return rust_parser_pt(RUST_PK_CMT, str_lit("a comment"), loc, gap);
  case RUST_TOK_PUNCT: {
    RustPk k = rust_parser_lookup(kind.text, RUST_PARSER_PUNCTS, RUST_PARSER_PUNCT_COUNT, 0,
                                  RUST_PK_OTHER_P);
    return rust_parser_pt(k, kind.text, loc, gap);
  }
  case RUST_TOK_EOF:
    return rust_parser_pt(RUST_PK_EOF_K, str_lit("end of input"), loc, gap);
  }
  return rust_parser_pt(RUST_PK_EOF_K, str_lit("end of input"), loc, gap);
}

/* classify(ts): `ts` holds the lexer's RustTok pointers; the result holds RustPt pointers in the
 * same order (the Bend maps the list front to back). */
static Vec rust_parser_classify(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  size_t at = 0;
  while (at < ts->len && !heap->oom) {
    VecGet got = vec_get(ts, at);
    if (got.status == VEC_GET_OK) {
      const RustTok *t = got.value;
      RustPt pt = rust_parser_classify_kind(t->kind, t->loc, t->gap);
      RustPt *cell = heap_alloc(heap, sizeof *cell);
      if (cell != NULL) {
        *cell = pt;
        if (!vec_push(&out, cell)) heap->oom = true;
      }
    }
    at = at + 1;
  }
  return out;
}

/* C only: the head PT of a view, NULL for Nil. */
static const RustPt *rust_parser_head(RustPts ts) {
  VecGet got = vec_get(ts.all, ts.at);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

/* C only: the tail of a non-empty view (the `tail` of `Con{PT{..}, tail}`). */
static RustPts rust_parser_tail(RustPts ts) {
  RustPts out = ts;
  out.at = ts.at + 1;
  return out;
}

/* head_kind(ts) */
static RustPk rust_parser_head_kind(RustPts ts) {
  const RustPt *h = rust_parser_head(ts);
  if (h != NULL) return h->k;
  return RUST_PK_EOF_K;
}

/* head_gap(ts) */
static bool rust_parser_head_gap(RustPts ts) {
  const RustPt *h = rust_parser_head(ts);
  if (h != NULL) return h->i.gap;
  return false;
}

/* head_pos(ts): the position of the next token; it goes into the `Meta` of a construct. */
static AstPos rust_parser_head_pos(Heap *heap, RustPts ts) {
  const RustPt *h = rust_parser_head(ts);
  if (h != NULL) return ast_pos(h->i.loc.line, h->i.loc.col);
  return ast_no_pos(heap);
}

/* is_lit(k): literal-like kinds never satisfy `expect`, even when their text matches. */
static bool rust_parser_is_lit(RustPk k) {
  return k == RUST_PK_STR_L || k == RUST_PK_CHR_L || k == RUST_PK_DOC_O || k == RUST_PK_DOC_I;
}

/* is_seg(k) */
static bool rust_parser_is_seg(RustPk k) {
  return k == RUST_PK_NAME || k == RUST_PK_RAW_NAME || k == RUST_PK_KSELF_V ||
         k == RUST_PK_KSELF_T || k == RUST_PK_KCRATE || k == RUST_PK_KSUPER;
}

/* is_item(k) */
static bool rust_parser_is_item(RustPk k) {
  return k == RUST_PK_KFN || k == RUST_PK_KPUB || k == RUST_PK_KSTRUCT || k == RUST_PK_KENUM ||
         k == RUST_PK_KUSE || k == RUST_PK_KCONST || k == RUST_PK_KIMPL || k == RUST_PK_KTRAIT ||
         k == RUST_PK_KMOD || k == RUST_PK_KTYPE;
}

/* is_end(k) */
static bool rust_parser_is_end(RustPk k) { return k == RUST_PK_RBRACE || k == RUST_PK_EOF_K; }

/* Maybe<Nat> of prec: `some` false = None. */
typedef struct {
  bool some;
  uint64_t value;
} RustPPrec;

/* C only: prec's table, in the Bend case order. */
typedef struct {
  RustPk k;
  uint64_t value;
} RustPPrecEntry;
static const RustPPrecEntry RUST_PARSER_PRECS[] = {
    {RUST_PK_STAR, 10},   {RUST_PK_SLASH, 10}, {RUST_PK_PERCENT, 10}, {RUST_PK_PLUS, 9},
    {RUST_PK_MINUS, 9},   {RUST_PK_AMP, 7},    {RUST_PK_CARET, 6},    {RUST_PK_PIPE, 5},
    {RUST_PK_EQ_EQ, 4},   {RUST_PK_NE, 4},     {RUST_PK_LT, 4},       {RUST_PK_GT, 4},
    {RUST_PK_LE, 4},      {RUST_PK_GE, 4},     {RUST_PK_AND_AND, 3},  {RUST_PK_OR_OR, 2}};
#define RUST_PARSER_PREC_COUNT (sizeof RUST_PARSER_PRECS / sizeof RUST_PARSER_PRECS[0])

/* C only: prec's table search from index `at` on. */
static RustPPrec rust_parser_prec_at(RustPk k, size_t at) {
  RustPPrec none = {false, 0};
  if (at >= RUST_PARSER_PREC_COUNT) return none;
  if (RUST_PARSER_PRECS[at].k == k) {
    RustPPrec some = {true, RUST_PARSER_PRECS[at].value};
    return some;
  }
  return rust_parser_prec_at(k, at + 1);
}

/* prec(k): binary operator precedence (higher binds tighter); all are left-associative. */
static RustPPrec rust_parser_prec(RustPk k) { return rust_parser_prec_at(k, 0); }

/* Parsed<A>, pv and pr: see the file comment (one C struct per A, field reads). */

/* C only: the concatenation of `count` strings. */
static Str rust_parser_join(Heap *heap, const Str *parts, size_t count) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t at = 0;
  while (at < count) {
    if (!strbuf_push(&buf, parts[at])) heap->oom = true;
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* err(what, tokens): the Fail message. */
static Str rust_parser_err(Heap *heap, Str what, RustPts tokens) {
  const RustPt *h = rust_parser_head(tokens);
  if (h != NULL) {
    Str loc = rust_token_show_loc(heap, h->i.loc);
    Str parts[6] = {loc, str_lit(" expected "), what, str_lit(", found `"), h->i.text,
                    str_lit("`")};
    return rust_parser_join(heap, parts, 6);
  }
  Str parts[3] = {str_lit("expected "), what, str_lit(", found end of input")};
  return rust_parser_join(heap, parts, 3);
}

/* Result<String, List<PT>>: ok = Done{rest}, else Fail{message}. */
typedef struct {
  bool ok;
  RustPts rest;
  Str message;
} RustPRest;

/* expect(sym, tokens) */
static RustPRest rust_parser_expect(Heap *heap, Str sym, RustPts tokens) {
  RustPRest out;
  const RustPt *h = rust_parser_head(tokens);
  out.rest = tokens;
  out.message = str_empty();
  if (h != NULL && str_eq(h->i.text, sym) && !rust_parser_is_lit(h->k)) {
    out.ok = true;
    out.rest = rust_parser_tail(tokens);
    return out;
  }
  Str parts[3] = {str_lit("`"), sym, str_lit("`")};
  Str what = rust_parser_join(heap, parts, 3);
  out.ok = false;
  out.message = rust_parser_err(heap, what, tokens);
  return out;
}

/* Close: the closing bracket of a list in MExprs and MTys. */
typedef enum { RUST_PCLOSE_PAREN, RUST_PCLOSE_ANGLE, RUST_PCLOSE_BRACK } RustPClose;

/* After: what `MMeta` parses once outer docs and attributes are collected. */
typedef enum { RUST_PA_ITEM, RUST_PA_FIELD, RUST_PA_VARIANT, RUST_PA_STMT, RUST_PA_ARM } RustPAfterKind;
typedef struct {
  RustPAfterKind kind;
  bool named; /* AFIELD */
} RustPAfter;

/* Value: the tag says which field holds the parsed value; every list is a Vec of pointers (Str
 * pointers for VSTRS). `rest` is the Bend `rest` of every constructor. */
typedef enum {
  RUST_PV_NODE,
  RUST_PV_NODES,
  RUST_PV_TY,
  RUST_PV_TYS,
  RUST_PV_PAT,
  RUST_PV_PATS,
  RUST_PV_SEGS,
  RUST_PV_STMTS,
  RUST_PV_STMT,
  RUST_PV_ARM,
  RUST_PV_ARMS,
  RUST_PV_CPARAMS,
  RUST_PV_GENS,
  RUST_PV_PARAMS,
  RUST_PV_FIELD,
  RUST_PV_FIELDS,
  RUST_PV_BODY,
  RUST_PV_VARIANT,
  RUST_PV_VARIANTS,
  RUST_PV_USE,
  RUST_PV_USES,
  RUST_PV_STRS,
  RUST_PV_ATTR
} RustPValueKind;
typedef struct {
  RustPValueKind kind;
  const AstNode *node;       /* VNODE */
  const AstTy *ty;           /* VTY */
  const AstPat *pat;         /* VPAT */
  const AstStmt *stmt;       /* VSTMT */
  const AstArm *arm;         /* VARM */
  const AstField *field;     /* VFIELD */
  AstBody body;              /* VBODY */
  const AstVariant *variant; /* VVARIANT */
  const AstUseTree *use;     /* VUSE */
  const AstAttr *attr;       /* VATTR */
  Vec list;                  /* VNODES, VTYS, VPATS, VSEGS, VSTMTS, VARMS, VCPARAMS, VGENS, VPARAMS,
                                VFIELDS, VVARIANTS, VUSES, VSTRS */
  RustPts rest;
} RustPValue;

/* Result<String, Value>: ok = Done{value}, else Fail{message}. The read_* defs below return this
 * same struct: Result<String, Parsed<A>> is the Done case with value.kind the tag of A, so pv is the
 * field of that tag and pr is value.rest (D-205a). */
typedef struct {
  bool ok;
  RustPValue value;
  Str message;
} RustPResult;

/* mismatch(A, what): the Fail message. */
static Str rust_parser_mismatch(Heap *heap, const char *what) {
  Str parts[2] = {str_lit("internal parser result mismatch: "), str_lit(what)};
  return rust_parser_join(heap, parts, 2);
}

/* C only: the shared body of read_*: a Fail stays as it is, a Done of the tag `want` is the Parsed
 * value, any other Done is the mismatch Fail. */
static RustPResult rust_parser_read_as(Heap *heap, const RustPResult *r, RustPValueKind want,
                                       const char *what) {
  RustPResult out = *r;
  if (!r->ok || r->value.kind == want) return out;
  RustPValue none = {.kind = want};
  out.ok = false;
  out.value = none;
  out.message = rust_parser_mismatch(heap, what);
  return out;
}

/* read_node(r) */
static RustPResult rust_parser_read_node(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_NODE, "Node");
}

/* read_nodes(r) */
static RustPResult rust_parser_read_nodes(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_NODES, "Nodes");
}

/* read_ty(r) */
static RustPResult rust_parser_read_ty(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_TY, "Ty");
}

/* read_tys(r) */
static RustPResult rust_parser_read_tys(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_TYS, "Tys");
}

/* read_pat(r) */
static RustPResult rust_parser_read_pat(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_PAT, "Pat");
}

/* read_pats(r) */
static RustPResult rust_parser_read_pats(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_PATS, "Pats");
}

/* read_segs(r) */
static RustPResult rust_parser_read_segs(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_SEGS, "Segs");
}

/* read_stmts(r) */
static RustPResult rust_parser_read_stmts(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_STMTS, "Stmts");
}

/* read_stmt(r) */
static RustPResult rust_parser_read_stmt(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_STMT, "Stmt");
}

/* read_arm(r) */
static RustPResult rust_parser_read_arm(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_ARM, "Arm");
}

/* read_arms(r) */
static RustPResult rust_parser_read_arms(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_ARMS, "Arms");
}

/* read_cparams(r) */
static RustPResult rust_parser_read_cparams(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_CPARAMS, "CParams");
}

/* read_gens(r) */
static RustPResult rust_parser_read_gens(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_GENS, "Gens");
}

/* read_params(r) */
static RustPResult rust_parser_read_params(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_PARAMS, "Params");
}

/* read_field(r) */
static RustPResult rust_parser_read_field(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_FIELD, "Field");
}

/* read_fields(r) */
static RustPResult rust_parser_read_fields(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_FIELDS, "Fields");
}

/* read_body(r) */
static RustPResult rust_parser_read_body(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_BODY, "Body");
}

/* read_variant(r) */
static RustPResult rust_parser_read_variant(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_VARIANT, "Variant");
}

/* read_variants(r) */
static RustPResult rust_parser_read_variants(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_VARIANTS, "Variants");
}

/* read_use(r) */
static RustPResult rust_parser_read_use(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_USE, "Use");
}

/* read_uses(r) */
static RustPResult rust_parser_read_uses(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_USES, "Uses");
}

/* read_strs(r) */
static RustPResult rust_parser_read_strs(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_STRS, "Strs");
}

/* read_attr(r) */
static RustPResult rust_parser_read_attr(Heap *heap, const RustPResult *r) {
  return rust_parser_read_as(heap, r, RUST_PV_ATTR, "Attr");
}

/* Mode: the parse state. `ns` = no struct literal (an `if`/`match` scrutinee). One fat struct; the
 * tag says which fields hold data, the rest stay zero. Every Bend list is a Vec of pointers held in
 * source order: the Bend modes accumulate in reverse and reverse once at the close, the C modes push
 * at the end and the close takes the Vec as it is (D-205b). C names that differ from the Bend names:
 * MBinOp.p = p_maybe, MElse.body = stmts, MLetInit.ty = maybe_ty, MSeg/MSegArgs/MSegNext.ty = is_ty
 * (a Bool); MImplFor.ty and MConstVal.ty keep `ty` (an A.Ty). */
typedef enum {
  RUST_PM_BIN,
  RUST_PM_BIN_REST,
  RUST_PM_BIN_OP,
  RUST_PM_BIN_GO,
  RUST_PM_DONE,
  RUST_PM_UNARY,
  RUST_PM_UNARY_GO,
  RUST_PM_POST,
  RUST_PM_PRIM,
  RUST_PM_PATH_EXPR,
  RUST_PM_PAREN,
  RUST_PM_PAREN_REST,
  RUST_PM_EXPRS,
  RUST_PM_CLOSURE_RET,
  RUST_PM_CPARAMS,
  RUST_PM_CPARAM_TY,
  RUST_PM_IF,
  RUST_PM_IF_BODY,
  RUST_PM_ELSE,
  RUST_PM_MATCH,
  RUST_PM_ARMS,
  RUST_PM_ARM,
  RUST_PM_ARM_GUARD,
  RUST_PM_ARM_BODY,
  RUST_PM_ARM_END,
  RUST_PM_INITS,
  RUST_PM_STMTS,
  RUST_PM_STMT,
  RUST_PM_LET_TY,
  RUST_PM_LET_INIT,
  RUST_PM_STMT_ITEM,
  RUST_PM_STMT_EXPR,
  RUST_PM_STMT_END,
  RUST_PM_PAT_TOP,
  RUST_PM_PAT_OR,
  RUST_PM_PAT,
  RUST_PM_PAT_PATH,
  RUST_PM_PATS,
  RUST_PM_FIELD_PATS,
  RUST_PM_TY,
  RUST_PM_FN_RET,
  RUST_PM_TYS,
  RUST_PM_BOUNDS,
  RUST_PM_BOUNDS_NEXT,
  RUST_PM_SEG,
  RUST_PM_SEG_ARGS,
  RUST_PM_SEG_NEXT,
  RUST_PM_META,
  RUST_PM_ATTR,
  RUST_PM_STRS,
  RUST_PM_STRS_PATH,
  RUST_PM_INNER,
  RUST_PM_ITEMS,
  RUST_PM_ITEMS_GO,
  RUST_PM_DONE_NODES,
  RUST_PM_ITEM,
  RUST_PM_ITEM_V,
  RUST_PM_FN_SIG,
  RUST_PM_FN_BODY,
  RUST_PM_GENERICS,
  RUST_PM_GPARAMS,
  RUST_PM_PARAMS,
  RUST_PM_BODY,
  RUST_PM_BODY_END,
  RUST_PM_FIELDS,
  RUST_PM_FIELD,
  RUST_PM_FIELD_V,
  RUST_PM_VARIANTS,
  RUST_PM_VARIANT,
  RUST_PM_USE,
  RUST_PM_USE_SEG,
  RUST_PM_USES,
  RUST_PM_IMPL_FOR,
  RUST_PM_IMPL_BODY,
  RUST_PM_TRAIT_SUP,
  RUST_PM_TRAIT_BODY,
  RUST_PM_CONST_VAL,
  RUST_PM_TYPE_VAL
} RustPModeKind;
typedef struct {
  RustPModeKind kind;
  bool ns;               /* MBIN .. MBINGO, MUNARY, MUNARYGO, MPRIM, MPATHEXPR */
  uint64_t min;          /* MBIN .. MBINGO */
  const AstNode *lhs;    /* MBINREST, MBINOP, MBINGO */
  RustPPrec p_maybe;     /* MBINOP p */
  uint64_t p;            /* MBINGO */
  const AstNode *e;      /* MDONE, MPOST, MPARENREST, MSTMTEND */
  Str op;                /* MUNARYGO */
  Vec segs;              /* MPATHEXPR, MINITS, MPATPATH, MFIELDPATS (AstSeg pointers) */
  Vec acc;               /* every mode with an `acc` (pointers to the element type) */
  RustPClose close;      /* MEXPRS, MTYS */
  bool mv;               /* MCLOSURERET */
  Vec params;            /* MCLOSURERET (AstCParam pointers), MFNSIG (AstParam pointers) */
  const AstPat *pat;     /* MCPARAMTY, MARMGUARD, MARMBODY, MLETTY, MLETINIT */
  const AstNode *cond;   /* MIFBODY, MELSE */
  Vec stmts;             /* MELSE body (AstStmt pointers) */
  AstMeta meta;          /* MARM .. MARMBODY, MSTMT .. MSTMTEND, MITEM .. MFNBODY, MFIELD, MFIELDV,
                            MVARIANT, MIMPLFOR .. MTYPEVAL */
  AstMaybeNode guard;    /* MARMBODY */
  const AstArm *arm;     /* MARMEND */
  AstMaybeTy maybe_ty;   /* MLETINIT ty */
  bool is_ty;            /* MSEG, MSEGARGS, MSEGNEXT ty */
  bool ok;               /* MSEG, MUSESEG */
  Str name;              /* MFNRET, MSEGARGS, MFNSIG, MTRAITSUP, MTRAITBODY, MCONSTVAL, MTYPEVAL */
  Vec args;              /* MFNRET (AstTy pointers) */
  RustPAfter after;      /* MMETA */
  bool gap;              /* MMETA */
  Vec docs;              /* MMETA (Str pointers) */
  Vec attrs;             /* MMETA (AstAttr pointers) */
  Str s;                 /* MSTRSPATH */
  AstVis vis;            /* MITEMV, MFNSIG, MFNBODY, MFIELDV, MTRAITSUP, MTRAITBODY, MCONSTVAL,
                            MTYPEVAL */
  Vec gens;              /* MFNSIG, MIMPLFOR, MIMPLBODY, MTRAITSUP, MTRAITBODY, MTYPEVAL (AstGParam
                            pointers) */
  AstSig sig;            /* MFNBODY */
  bool st;               /* MBODY, MBODYEND */
  AstBody body;          /* MBODYEND */
  bool named;            /* MFIELDS, MFIELD, MFIELDV */
  const AstTy *ty;       /* MIMPLFOR, MCONSTVAL */
  AstMaybeTy tr;         /* MIMPLBODY */
  const AstTy *self_ty;  /* MIMPLBODY */
  Vec supers;            /* MTRAITBODY (AstTy pointers) */
} RustPMode;

/* after_meta(after, meta) */
static RustPMode rust_parser_after_meta(RustPAfter after, AstMeta meta) {
  RustPMode out = {.kind = RUST_PM_ITEM};
  switch (after.kind) {
    case RUST_PA_ITEM:
      out.kind = RUST_PM_ITEM;
      break;
    case RUST_PA_FIELD:
      out.kind = RUST_PM_FIELD;
      out.named = after.named;
      break;
    case RUST_PA_VARIANT:
      out.kind = RUST_PM_VARIANT;
      break;
    case RUST_PA_STMT:
      out.kind = RUST_PM_STMT;
      break;
    case RUST_PA_ARM:
      out.kind = RUST_PM_ARM;
      break;
  }
  out.meta = meta;
  return out;
}

/* lit(k, text) */
static AstLit rust_parser_lit(RustPk k, Str text) {
  if (k == RUST_PK_STR_L) return ast_lstr(text);
  if (k == RUST_PK_CHR_L) return ast_lchar(text);
  if (k == RUST_PK_KTRUE) return ast_lbool(true);
  if (k == RUST_PK_KFALSE) return ast_lbool(false);
  return ast_lint(text);
}

/* is_lit_start(k) */
static bool rust_parser_is_lit_start(RustPk k) {
  return k == RUST_PK_INT_L || k == RUST_PK_STR_L || k == RUST_PK_CHR_L || k == RUST_PK_KTRUE ||
         k == RUST_PK_KFALSE;
}

/* def parse(mode, tokens): one C-only function per Mode constructor in the Bend case order, then the
 * dispatcher rust_parser_parse (an exhaustive switch over the Mode tag) at the end (D-205c). A Mode
 * value is built with designated initializers, bound to a local and passed by pointer (T-202). */
static RustPResult rust_parser_parse(Heap *heap, const RustPMode *mode, RustPts tokens);

/* C only: the PT `n` places into a view, NULL past the end. */
static const RustPt *rust_parser_nth(RustPts ts, size_t n) {
  VecGet got = vec_get(ts.all, ts.at + n);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

/* C only: the PT `n` places into a view exists and has the kind `k`. */
static bool rust_parser_is_at(RustPts ts, size_t n, RustPk k) {
  const RustPt *t = rust_parser_nth(ts, n);
  return t != NULL && t->k == k;
}

/* C only: the view `n` places on (the `tail` of a pattern that matches `n` tokens). */
static RustPts rust_parser_drop(RustPts ts, size_t n) {
  RustPts out = ts;
  out.at = ts.at + n;
  return out;
}

/* C only: Done{VNode{e, rest}}. */
static RustPResult rust_parser_done_node(const AstNode *e, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_NODE;
  out.value.node = e;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: Fail{message}. */
static RustPResult rust_parser_fail(Str message) {
  RustPResult out = {.ok = false};
  out.value.kind = RUST_PV_NODE;
  out.message = message;
  return out;
}

/* C only: err(Value, what, tokens). */
static RustPResult rust_parser_fail_at(Heap *heap, const char *what, RustPts tokens) {
  Str text = str_lit(what);
  Str message = rust_parser_err(heap, text, tokens);
  return rust_parser_fail(message);
}

/* C only: read_nodes(parse(MExprs{[], close}, tokens)). */
static RustPResult rust_parser_exprs(Heap *heap, RustPClose close, RustPts tokens) {
  Vec none = vec_new(heap->arena);
  RustPMode em = {.kind = RUST_PM_EXPRS, .acc = none, .close = close};
  RustPResult r = rust_parser_parse(heap, &em, tokens);
  return rust_parser_read_nodes(heap, &r);
}

/* case MBin{ns, min} tokens */
static RustPResult rust_parser_parse_bin(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPMode um = {.kind = RUST_PM_UNARY, .ns = m->ns};
  RustPResult r = rust_parser_parse(heap, &um, tokens);
  RustPResult a = rust_parser_read_node(heap, &r);
  if (!a.ok) return a;
  RustPMode next = {.kind = RUST_PM_BIN_REST, .ns = m->ns, .min = m->min, .lhs = a.value.node};
  return rust_parser_parse(heap, &next, a.value.rest);
}

/* case MBinRest{ns, min, lhs} tokens */
static RustPResult rust_parser_parse_bin_rest(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPk k = rust_parser_head_kind(tokens);
  RustPPrec p = rust_parser_prec(k);
  RustPMode next = {
      .kind = RUST_PM_BIN_OP, .ns = m->ns, .min = m->min, .lhs = m->lhs, .p_maybe = p};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MBinOp{ns, min, lhs, None{}} tokens and case MBinOp{ns, min, lhs, Some{p}} tokens */
static RustPResult rust_parser_parse_bin_op(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (!m->p_maybe.some) return rust_parser_done_node(m->lhs, tokens);
  if (m->p_maybe.value >= m->min) {
    RustPMode go = {.kind = RUST_PM_BIN_GO,
                    .ns = m->ns,
                    .min = m->min,
                    .lhs = m->lhs,
                    .p = m->p_maybe.value};
    return rust_parser_parse(heap, &go, tokens);
  }
  RustPMode done = {.kind = RUST_PM_DONE, .e = m->lhs};
  return rust_parser_parse(heap, &done, tokens);
}

/* case MDone{e} tokens */
static RustPResult rust_parser_parse_done(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)heap;
  return rust_parser_done_node(m->e, tokens);
}

/* case MBinGo{ns, min, lhs, p} Con{PT{k, Info{op, loc, gap}}, tail} and case MBinGo{..} tokens */
static RustPResult rust_parser_parse_bin_go(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_head(tokens);
  if (h == NULL) return rust_parser_fail_at(heap, "an operator", tokens);
  Str op = h->i.text;
  RustPts tail = rust_parser_drop(tokens, 1);
  RustPMode bm = {.kind = RUST_PM_BIN, .ns = m->ns, .min = 1 + m->p};
  RustPResult r = rust_parser_parse(heap, &bm, tail);
  RustPResult a = rust_parser_read_node(heap, &r);
  if (!a.ok) return a;
  const AstNode *e = ast_ebinary(heap, op, m->lhs, a.value.node);
  RustPMode next = {.kind = RUST_PM_BIN_REST, .ns = m->ns, .min = m->min, .lhs = e};
  return rust_parser_parse(heap, &next, a.value.rest);
}

/* case MUnary{ns} Con{PT{Minus{} | Bang{} | Star{} | Amp{}, Info{op, ..}}, tail} and
 * case MUnary{ns} tokens */
static RustPResult rust_parser_parse_unary(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_head(tokens);
  if (h != NULL && (h->k == RUST_PK_MINUS || h->k == RUST_PK_BANG || h->k == RUST_PK_STAR ||
                    h->k == RUST_PK_AMP)) {
    RustPMode go = {.kind = RUST_PM_UNARY_GO, .ns = m->ns, .op = h->i.text};
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_parse(heap, &go, tail);
  }
  RustPMode pm = {.kind = RUST_PM_PRIM, .ns = m->ns};
  RustPResult r = rust_parser_parse(heap, &pm, tokens);
  RustPResult a = rust_parser_read_node(heap, &r);
  if (!a.ok) return a;
  RustPMode post = {.kind = RUST_PM_POST, .e = a.value.node};
  return rust_parser_parse(heap, &post, a.value.rest);
}

/* case MUnaryGo{ns, op} tokens */
static RustPResult rust_parser_parse_unary_go(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPMode um = {.kind = RUST_PM_UNARY, .ns = m->ns};
  RustPResult r = rust_parser_parse(heap, &um, tokens);
  RustPResult a = rust_parser_read_node(heap, &r);
  if (!a.ok) return a;
  const AstNode *e = ast_eunary(heap, m->op, a.value.node);
  return rust_parser_done_node(e, a.value.rest);
}

/* C only: parse(MPost{e}, tokens) for a new postfix head `e`. */
static RustPResult rust_parser_post_with(Heap *heap, const AstNode *e, RustPts tokens) {
  RustPMode next = {.kind = RUST_PM_POST, .e = e};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MPost{e} ..: `?`, `.name::<tys>(args)`, `.name(args)`, `.name`, `.0`, `(args)`, else Done. */
static RustPResult rust_parser_parse_post(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *second = rust_parser_nth(tokens, 1);
  bool dot = rust_parser_is_at(tokens, 0, RUST_PK_DOT);
  bool dot_name = dot && rust_parser_is_at(tokens, 1, RUST_PK_NAME);
  if (rust_parser_is_at(tokens, 0, RUST_PK_QUESTION)) {
    const AstNode *e = ast_etry(heap, m->e);
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_post_with(heap, e, tail);
  }
  if (dot_name && rust_parser_is_at(tokens, 2, RUST_PK_PATH_SEP) &&
      rust_parser_is_at(tokens, 3, RUST_PK_LT)) {
    Str name = second->i.text;
    Vec none = vec_new(heap->arena);
    RustPMode tm = {.kind = RUST_PM_TYS, .acc = none, .close = RUST_PCLOSE_ANGLE};
    RustPts tail = rust_parser_drop(tokens, 4);
    RustPResult r = rust_parser_parse(heap, &tm, tail);
    RustPResult gs = rust_parser_read_tys(heap, &r);
    if (!gs.ok) return gs;
    Str open = str_lit("(");
    RustPRest t2 = rust_parser_expect(heap, open, gs.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    RustPResult args = rust_parser_exprs(heap, RUST_PCLOSE_PAREN, t2.rest);
    if (!args.ok) return args;
    const AstNode *e = ast_emethod(heap, m->e, name, gs.value.list, args.value.list);
    return rust_parser_post_with(heap, e, args.value.rest);
  }
  if (dot_name && rust_parser_is_at(tokens, 2, RUST_PK_LPAREN)) {
    Str name = second->i.text;
    RustPts tail = rust_parser_drop(tokens, 3);
    RustPResult args = rust_parser_exprs(heap, RUST_PCLOSE_PAREN, tail);
    if (!args.ok) return args;
    Vec none = vec_new(heap->arena);
    const AstNode *e = ast_emethod(heap, m->e, name, none, args.value.list);
    return rust_parser_post_with(heap, e, args.value.rest);
  }
  if (dot_name || (dot && rust_parser_is_at(tokens, 1, RUST_PK_INT_L))) {
    const AstNode *e = ast_efield(heap, m->e, second->i.text);
    RustPts tail = rust_parser_drop(tokens, 2);
    return rust_parser_post_with(heap, e, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LPAREN)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult args = rust_parser_exprs(heap, RUST_PCLOSE_PAREN, tail);
    if (!args.ok) return args;
    const AstNode *e = ast_ecall(heap, m->e, args.value.list);
    return rust_parser_post_with(heap, e, args.value.rest);
  }
  return rust_parser_done_node(m->e, tokens);
}

/* C only: the MPrim cases on `|` and `move |`: read_cparams(parse(MCParams{[]}, tail)), then
 * parse(MClosureRet{mv, ps}, rest). */
static RustPResult rust_parser_prim_closure(Heap *heap, bool mv, RustPts tail) {
  Vec none = vec_new(heap->arena);
  RustPMode cm = {.kind = RUST_PM_CPARAMS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &cm, tail);
  RustPResult ps = rust_parser_read_cparams(heap, &r);
  if (!ps.ok) return ps;
  RustPMode next = {.kind = RUST_PM_CLOSURE_RET, .mv = mv, .params = ps.value.list};
  return rust_parser_parse(heap, &next, ps.value.rest);
}

/* C only: the MPrim cases on `||` and `move ||`: parse(MClosureRet{mv, []}, tail). */
static RustPResult rust_parser_prim_closure_empty(Heap *heap, bool mv, RustPts tail) {
  Vec none = vec_new(heap->arena);
  RustPMode next = {.kind = RUST_PM_CLOSURE_RET, .mv = mv, .params = none};
  return rust_parser_parse(heap, &next, tail);
}

/* C only: the MPrim cases on `name ! [` and `name ! (`: read_nodes(parse(MExprs{[], close}, tail)),
 * then Done{VNode{EMacro{name, bracket, as}, rest}}. */
static RustPResult rust_parser_prim_macro(Heap *heap, Str name, bool bracket, RustPClose close,
                                          RustPts tail) {
  RustPResult args = rust_parser_exprs(heap, close, tail);
  if (!args.ok) return args;
  const AstNode *e = ast_emacro(heap, name, bracket, args.value.list);
  return rust_parser_done_node(e, args.value.rest);
}

/* case MPrim{ns} ..: the primary expressions, in the Bend case order. */
static RustPResult rust_parser_parse_prim(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_head(tokens);
  if (h == NULL) return rust_parser_fail_at(heap, "an expression", tokens);
  RustPts tail = rust_parser_drop(tokens, 1);
  RustPts tail2 = rust_parser_drop(tokens, 2);
  RustPts tail3 = rust_parser_drop(tokens, 3);
  bool name_bang = h->k == RUST_PK_NAME && rust_parser_is_at(tokens, 1, RUST_PK_BANG);
  if (h->k == RUST_PK_LPAREN) {
    RustPMode pm = {.kind = RUST_PM_PAREN};
    return rust_parser_parse(heap, &pm, tail);
  }
  if (h->k == RUST_PK_LBRACE) {
    Vec none = vec_new(heap->arena);
    RustPMode sm = {.kind = RUST_PM_STMTS, .acc = none};
    RustPResult r = rust_parser_parse(heap, &sm, tail);
    RustPResult b = rust_parser_read_stmts(heap, &r);
    if (!b.ok) return b;
    const AstNode *e = ast_eblock(heap, b.value.list);
    return rust_parser_done_node(e, b.value.rest);
  }
  if (h->k == RUST_PK_KIF) {
    RustPMode im = {.kind = RUST_PM_IF};
    return rust_parser_parse(heap, &im, tail);
  }
  if (h->k == RUST_PK_KMATCH) {
    RustPMode mm = {.kind = RUST_PM_MATCH};
    return rust_parser_parse(heap, &mm, tail);
  }
  if (h->k == RUST_PK_PIPE) return rust_parser_prim_closure(heap, false, tail);
  if (h->k == RUST_PK_OR_OR) return rust_parser_prim_closure_empty(heap, false, tail);
  if (h->k == RUST_PK_KMOVE && rust_parser_is_at(tokens, 1, RUST_PK_PIPE)) {
    return rust_parser_prim_closure(heap, true, tail2);
  }
  if (h->k == RUST_PK_KMOVE && rust_parser_is_at(tokens, 1, RUST_PK_OR_OR)) {
    return rust_parser_prim_closure_empty(heap, true, tail2);
  }
  if (name_bang && rust_parser_is_at(tokens, 2, RUST_PK_LBRACK)) {
    return rust_parser_prim_macro(heap, h->i.text, true, RUST_PCLOSE_BRACK, tail3);
  }
  if (name_bang && rust_parser_is_at(tokens, 2, RUST_PK_LPAREN)) {
    return rust_parser_prim_macro(heap, h->i.text, false, RUST_PCLOSE_PAREN, tail3);
  }
  if (rust_parser_is_lit_start(h->k)) {
    AstLit lit = rust_parser_lit(h->k, h->i.text);
    const AstNode *e = ast_elit(heap, lit);
    RustPMode dm = {.kind = RUST_PM_DONE, .e = e};
    return rust_parser_parse(heap, &dm, tail);
  }
  Vec none = vec_new(heap->arena);
  RustPMode pm = {.kind = RUST_PM_PATH_EXPR, .ns = m->ns, .segs = none};
  return rust_parser_parse(heap, &pm, tokens);
}

/* C only: Done{V<list tag>{list, rest}} (the Bend `List.reverse(acc)` is the acc as it is, D-205b). */
static RustPResult rust_parser_done_list(RustPValueKind kind, Vec list, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = kind;
  out.value.list = list;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: `x <> acc` on a source-order acc: the copied Vec header with x pushed at the end. */
static Vec rust_parser_snoc(Heap *heap, Vec acc, const void *x) {
  Vec out = acc;
  if (!vec_push(&out, (void *)x)) heap->oom = true;
  return out;
}

/* C only: read_node(parse(MBin{ns, 0n}, tokens)). */
static RustPResult rust_parser_expr(Heap *heap, bool ns, RustPts tokens) {
  RustPMode bm = {.kind = RUST_PM_BIN, .ns = ns, .min = 0};
  RustPResult r = rust_parser_parse(heap, &bm, tokens);
  return rust_parser_read_node(heap, &r);
}

/* C only: read_ty(parse(MTy{}, tokens)). */
static RustPResult rust_parser_ty(Heap *heap, RustPts tokens) {
  RustPMode tm = {.kind = RUST_PM_TY};
  RustPResult r = rust_parser_parse(heap, &tm, tokens);
  return rust_parser_read_ty(heap, &r);
}

/* C only: read_stmts(parse(MStmts{[]}, tokens)). */
static RustPResult rust_parser_stmts(Heap *heap, RustPts tokens) {
  Vec none = vec_new(heap->arena);
  RustPMode sm = {.kind = RUST_PM_STMTS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &sm, tokens);
  return rust_parser_read_stmts(heap, &r);
}

/* case MPathExpr{ns, Nil{}} tokens, case MPathExpr{False{}, segs} Con{PT{LBrace{}, i}, tail} and
 * case MPathExpr{ns, segs} tokens */
static RustPResult rust_parser_parse_path_expr(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (m->segs.len == 0) {
    RustPk k = rust_parser_head_kind(tokens);
    Vec none = vec_new(heap->arena);
    RustPMode sm = {.kind = RUST_PM_SEG, .is_ty = false, .acc = none, .ok = rust_parser_is_seg(k)};
    RustPResult r = rust_parser_parse(heap, &sm, tokens);
    RustPResult s = rust_parser_read_segs(heap, &r);
    if (!s.ok) return s;
    RustPMode next = {.kind = RUST_PM_PATH_EXPR, .ns = m->ns, .segs = s.value.list};
    return rust_parser_parse(heap, &next, s.value.rest);
  }
  if (!m->ns && rust_parser_is_at(tokens, 0, RUST_PK_LBRACE)) {
    Vec none = vec_new(heap->arena);
    RustPMode im = {.kind = RUST_PM_INITS, .segs = m->segs, .acc = none};
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_parse(heap, &im, tail);
  }
  const AstNode *e = ast_epath(heap, m->segs);
  return rust_parser_done_node(e, tokens);
}

/* case MParen{} Con{PT{RParen{}, i}, tail} and case MParen{} tokens */
static RustPResult rust_parser_parse_paren(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  if (rust_parser_is_at(tokens, 0, RUST_PK_RPAREN)) {
    Vec none = vec_new(heap->arena);
    const AstNode *e = ast_etuple(heap, none);
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_done_node(e, tail);
  }
  RustPResult a = rust_parser_expr(heap, false, tokens);
  if (!a.ok) return a;
  RustPMode next = {.kind = RUST_PM_PAREN_REST, .e = a.value.node};
  return rust_parser_parse(heap, &next, a.value.rest);
}

/* case MParenRest{e} on `)`, on `,`, else err "`)` or `,`" */
static RustPResult rust_parser_parse_paren_rest(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RPAREN)) {
    const AstNode *e = ast_eparen(heap, m->e);
    return rust_parser_done_node(e, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) {
    Vec none = vec_new(heap->arena);
    Vec acc = rust_parser_snoc(heap, none, m->e);
    RustPMode em = {.kind = RUST_PM_EXPRS, .acc = acc, .close = RUST_PCLOSE_PAREN};
    RustPResult r = rust_parser_parse(heap, &em, tail);
    RustPResult as = rust_parser_read_nodes(heap, &r);
    if (!as.ok) return as;
    const AstNode *e = ast_etuple(heap, as.value.list);
    return rust_parser_done_node(e, as.value.rest);
  }
  return rust_parser_fail_at(heap, "`)` or `,`", tokens);
}

/* case MExprs{acc, CParen{}} on `)`, MExprs{acc, CBrack{}} on `]`, MExprs{acc, close} on `,`, and
 * MExprs{acc, close} tokens */
static RustPResult rust_parser_parse_exprs(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  bool paren = m->close == RUST_PCLOSE_PAREN && rust_parser_is_at(tokens, 0, RUST_PK_RPAREN);
  bool brack = m->close == RUST_PCLOSE_BRACK && rust_parser_is_at(tokens, 0, RUST_PK_RBRACK);
  if (paren || brack) return rust_parser_done_list(RUST_PV_NODES, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPResult a = rust_parser_expr(heap, false, tokens);
  if (!a.ok) return a;
  Vec acc = rust_parser_snoc(heap, m->acc, a.value.node);
  RustPMode next = {.kind = RUST_PM_EXPRS, .acc = acc, .close = m->close};
  return rust_parser_parse(heap, &next, a.value.rest);
}

/* case MCParams{acc} on `|`, on `,`, and MCParams{acc} tokens */
static RustPResult rust_parser_parse_cparams(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_PIPE)) {
    return rust_parser_done_list(RUST_PV_CPARAMS, m->acc, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPMode pm = {.kind = RUST_PM_PAT};
  RustPResult r = rust_parser_parse(heap, &pm, tokens);
  RustPResult p = rust_parser_read_pat(heap, &r);
  if (!p.ok) return p;
  RustPMode next = {.kind = RUST_PM_CPARAM_TY, .acc = m->acc, .pat = p.value.pat};
  return rust_parser_parse(heap, &next, p.value.rest);
}

/* case MCParamTy{acc, pat} on `:` and MCParamTy{acc, pat} tokens */
static RustPResult rust_parser_parse_cparam_ty(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_COLON)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    AstMaybeTy ty = ast_some_ty(t.value.ty);
    const AstCParam *cp = ast_cparam(heap, m->pat, ty);
    Vec acc = rust_parser_snoc(heap, m->acc, cp);
    RustPMode next = {.kind = RUST_PM_CPARAMS, .acc = acc};
    return rust_parser_parse(heap, &next, t.value.rest);
  }
  AstMaybeTy ty = ast_no_ty();
  const AstCParam *cp = ast_cparam(heap, m->pat, ty);
  Vec acc = rust_parser_snoc(heap, m->acc, cp);
  RustPMode next = {.kind = RUST_PM_CPARAMS, .acc = acc};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MClosureRet{mv, params} on `->` and MClosureRet{mv, params} tokens */
static RustPResult rust_parser_parse_closure_ret(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_ARROW)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    RustPResult b = rust_parser_expr(heap, false, t.value.rest);
    if (!b.ok) return b;
    AstMaybeTy ret = ast_some_ty(t.value.ty);
    const AstNode *e = ast_eclosure(heap, m->mv, m->params, ret, b.value.node);
    return rust_parser_done_node(e, b.value.rest);
  }
  RustPResult b = rust_parser_expr(heap, false, tokens);
  if (!b.ok) return b;
  AstMaybeTy ret = ast_no_ty();
  const AstNode *e = ast_eclosure(heap, m->mv, m->params, ret, b.value.node);
  return rust_parser_done_node(e, b.value.rest);
}

/* case MIf{} on `let` and MIf{} tokens */
static RustPResult rust_parser_parse_if(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  if (rust_parser_is_at(tokens, 0, RUST_PK_KLET)) {
    RustPMode pm = {.kind = RUST_PM_PAT_TOP};
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult r = rust_parser_parse(heap, &pm, tail);
    RustPResult p = rust_parser_read_pat(heap, &r);
    if (!p.ok) return p;
    Str eq = str_lit("=");
    RustPRest t2 = rust_parser_expect(heap, eq, p.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    RustPResult e = rust_parser_expr(heap, true, t2.rest);
    if (!e.ok) return e;
    const AstNode *cond = ast_elet(heap, p.value.pat, e.value.node);
    RustPMode next = {.kind = RUST_PM_IF_BODY, .cond = cond};
    return rust_parser_parse(heap, &next, e.value.rest);
  }
  RustPResult c = rust_parser_expr(heap, true, tokens);
  if (!c.ok) return c;
  RustPMode next = {.kind = RUST_PM_IF_BODY, .cond = c.value.node};
  return rust_parser_parse(heap, &next, c.value.rest);
}

/* case MIfBody{cond} tokens */
static RustPResult rust_parser_parse_if_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  Str open = str_lit("{");
  RustPRest t2 = rust_parser_expect(heap, open, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  RustPResult b = rust_parser_stmts(heap, t2.rest);
  if (!b.ok) return b;
  RustPMode next = {.kind = RUST_PM_ELSE, .cond = m->cond, .stmts = b.value.list};
  return rust_parser_parse(heap, &next, b.value.rest);
}

/* case MElse{cond, body} on `else if`, on `else {`, and MElse{cond, body} tokens */
static RustPResult rust_parser_parse_else(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 2);
  bool kelse = rust_parser_is_at(tokens, 0, RUST_PK_KELSE);
  if (kelse && rust_parser_is_at(tokens, 1, RUST_PK_KIF)) {
    RustPMode im = {.kind = RUST_PM_IF};
    RustPResult r = rust_parser_parse(heap, &im, tail);
    RustPResult e = rust_parser_read_node(heap, &r);
    if (!e.ok) return e;
    AstMaybeNode els = ast_some_node(e.value.node);
    const AstNode *x = ast_eif(heap, m->cond, m->stmts, els);
    return rust_parser_done_node(x, e.value.rest);
  }
  if (kelse && rust_parser_is_at(tokens, 1, RUST_PK_LBRACE)) {
    RustPResult s = rust_parser_stmts(heap, tail);
    if (!s.ok) return s;
    const AstNode *block = ast_eblock(heap, s.value.list);
    AstMaybeNode els = ast_some_node(block);
    const AstNode *x = ast_eif(heap, m->cond, m->stmts, els);
    return rust_parser_done_node(x, s.value.rest);
  }
  AstMaybeNode els = ast_no_node();
  const AstNode *x = ast_eif(heap, m->cond, m->stmts, els);
  return rust_parser_done_node(x, tokens);
}

/* case MMatch{} tokens */
static RustPResult rust_parser_parse_match(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  RustPResult s = rust_parser_expr(heap, true, tokens);
  if (!s.ok) return s;
  Str open = str_lit("{");
  RustPRest t2 = rust_parser_expect(heap, open, s.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  Vec none = vec_new(heap->arena);
  RustPMode am = {.kind = RUST_PM_ARMS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &am, t2.rest);
  RustPResult a = rust_parser_read_arms(heap, &r);
  if (!a.ok) return a;
  const AstNode *x = ast_ematch(heap, s.value.node, a.value.list);
  return rust_parser_done_node(x, a.value.rest);
}

/* C only: Done{VPat{p, rest}}. */
static RustPResult rust_parser_done_pat(const AstPat *p, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_PAT;
  out.value.pat = p;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: Done{VTy{t, rest}}. */
static RustPResult rust_parser_done_ty(const AstTy *t, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_TY;
  out.value.ty = t;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: Done{VStmt{s, rest}}. */
static RustPResult rust_parser_done_stmt(const AstStmt *s, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_STMT;
  out.value.stmt = s;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: Done{VArm{a, rest}}. */
static RustPResult rust_parser_done_arm(const AstArm *a, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_ARM;
  out.value.arm = a;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: parse(MMeta{after, head_gap(tokens), [], []}, tokens) (MArms with AArm, MStmts with AStmt). */
static RustPResult rust_parser_meta_at(Heap *heap, RustPAfterKind kind, RustPts tokens) {
  Vec docs = vec_new(heap->arena);
  Vec attrs = vec_new(heap->arena);
  RustPAfter after = {.kind = kind, .named = false};
  bool gap = rust_parser_head_gap(tokens);
  RustPMode mm = {.kind = RUST_PM_META, .after = after, .gap = gap, .docs = docs, .attrs = attrs};
  return rust_parser_parse(heap, &mm, tokens);
}

/* C only: read_pat(parse(MPatTop{}, tokens)). */
static RustPResult rust_parser_pat_top(Heap *heap, RustPts tokens) {
  RustPMode pm = {.kind = RUST_PM_PAT_TOP};
  RustPResult r = rust_parser_parse(heap, &pm, tokens);
  return rust_parser_read_pat(heap, &r);
}

/* C only: read_pat(parse(MPat{}, tokens)). */
static RustPResult rust_parser_pat_one(Heap *heap, RustPts tokens) {
  RustPMode pm = {.kind = RUST_PM_PAT};
  RustPResult r = rust_parser_parse(heap, &pm, tokens);
  return rust_parser_read_pat(heap, &r);
}

/* C only: read_pats(parse(MPats{[]}, tokens)). */
static RustPResult rust_parser_pats(Heap *heap, RustPts tokens) {
  Vec none = vec_new(heap->arena);
  RustPMode pm = {.kind = RUST_PM_PATS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &pm, tokens);
  return rust_parser_read_pats(heap, &r);
}

/* C only: read_tys(parse(MTys{[], CParen{}}, tokens)). */
static RustPResult rust_parser_tys_paren(Heap *heap, RustPts tokens) {
  Vec none = vec_new(heap->arena);
  RustPMode tm = {.kind = RUST_PM_TYS, .acc = none, .close = RUST_PCLOSE_PAREN};
  RustPResult r = rust_parser_parse(heap, &tm, tokens);
  return rust_parser_read_tys(heap, &r);
}

/* C only: read_segs(parse(MSeg{ty, [], is_seg(head_kind(tokens))}, tokens)). */
static RustPResult rust_parser_segs_at(Heap *heap, bool is_ty, RustPts tokens) {
  RustPk k = rust_parser_head_kind(tokens);
  Vec none = vec_new(heap->arena);
  RustPMode sm = {.kind = RUST_PM_SEG, .is_ty = is_ty, .acc = none, .ok = rust_parser_is_seg(k)};
  RustPResult r = rust_parser_parse(heap, &sm, tokens);
  return rust_parser_read_segs(heap, &r);
}

/* case MArms{acc} on `}` and MArms{acc} tokens */
static RustPResult rust_parser_parse_arms(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_done_list(RUST_PV_ARMS, m->acc, tail);
  }
  RustPResult r = rust_parser_meta_at(heap, RUST_PA_ARM, tokens);
  RustPResult a = rust_parser_read_arm(heap, &r);
  if (!a.ok) return a;
  Vec acc = rust_parser_snoc(heap, m->acc, a.value.arm);
  RustPMode next = {.kind = RUST_PM_ARMS, .acc = acc};
  return rust_parser_parse(heap, &next, a.value.rest);
}

/* case MArm{meta} tokens */
static RustPResult rust_parser_parse_arm(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult p = rust_parser_pat_top(heap, tokens);
  if (!p.ok) return p;
  RustPMode next = {.kind = RUST_PM_ARM_GUARD, .meta = m->meta, .pat = p.value.pat};
  return rust_parser_parse(heap, &next, p.value.rest);
}

/* case MArmGuard{meta, pat} on `if` and MArmGuard{meta, pat} tokens */
static RustPResult rust_parser_parse_arm_guard(Heap *heap, const RustPMode *m, RustPts tokens) {
  Str arrow = str_lit("=>");
  if (rust_parser_is_at(tokens, 0, RUST_PK_KIF)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult g = rust_parser_expr(heap, false, tail);
    if (!g.ok) return g;
    RustPRest t2 = rust_parser_expect(heap, arrow, g.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    AstMaybeNode guard = ast_some_node(g.value.node);
    RustPMode next = {.kind = RUST_PM_ARM_BODY, .meta = m->meta, .pat = m->pat, .guard = guard};
    return rust_parser_parse(heap, &next, t2.rest);
  }
  RustPRest t2 = rust_parser_expect(heap, arrow, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  AstMaybeNode guard = ast_no_node();
  RustPMode next = {.kind = RUST_PM_ARM_BODY, .meta = m->meta, .pat = m->pat, .guard = guard};
  return rust_parser_parse(heap, &next, t2.rest);
}

/* case MArmBody{meta, pat, guard} tokens */
static RustPResult rust_parser_parse_arm_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult b = rust_parser_expr(heap, false, tokens);
  if (!b.ok) return b;
  const AstArm *arm = ast_arm(heap, m->meta, m->pat, m->guard, b.value.node);
  RustPMode next = {.kind = RUST_PM_ARM_END, .arm = arm};
  return rust_parser_parse(heap, &next, b.value.rest);
}

/* case MArmEnd{arm} on `,` and MArmEnd{arm} tokens */
static RustPResult rust_parser_parse_arm_end(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)heap;
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_done_arm(m->arm, tail);
  }
  return rust_parser_done_arm(m->arm, tokens);
}

/* case MInits{segs, acc} on `}`, on `,`, on `..`, on `name :`, on `name`, and MInits{segs, acc}
 * tokens (err "a field initializer or `}`") */
static RustPResult rust_parser_parse_inits(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) {
    AstMaybeNode base = ast_no_node();
    const AstNode *x = ast_estruct(heap, m->segs, m->acc, base);
    return rust_parser_done_node(x, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_DOT_DOT)) {
    RustPResult e = rust_parser_expr(heap, false, tail);
    if (!e.ok) return e;
    Str close = str_lit("}");
    RustPRest t2 = rust_parser_expect(heap, close, e.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    AstMaybeNode base = ast_some_node(e.value.node);
    const AstNode *x = ast_estruct(heap, m->segs, m->acc, base);
    return rust_parser_done_node(x, t2.rest);
  }
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h != NULL && h->k == RUST_PK_NAME && rust_parser_is_at(tokens, 1, RUST_PK_COLON)) {
    RustPts tail2 = rust_parser_drop(tokens, 2);
    RustPResult e = rust_parser_expr(heap, false, tail2);
    if (!e.ok) return e;
    AstMaybeNode value = ast_some_node(e.value.node);
    const AstFieldInit *fi = ast_field_init(heap, h->i.text, value);
    Vec acc = rust_parser_snoc(heap, m->acc, fi);
    RustPMode next = {.kind = RUST_PM_INITS, .segs = m->segs, .acc = acc};
    return rust_parser_parse(heap, &next, e.value.rest);
  }
  if (h != NULL && h->k == RUST_PK_NAME) {
    AstMaybeNode value = ast_no_node();
    const AstFieldInit *fi = ast_field_init(heap, h->i.text, value);
    Vec acc = rust_parser_snoc(heap, m->acc, fi);
    RustPMode next = {.kind = RUST_PM_INITS, .segs = m->segs, .acc = acc};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_fail_at(heap, "a field initializer or `}`", tokens);
}

/* case MStmts{acc} on `}`, on `;`, and MStmts{acc} tokens */
static RustPResult rust_parser_parse_stmts(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) {
    return rust_parser_done_list(RUST_PV_STMTS, m->acc, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_SEMI)) return rust_parser_parse(heap, m, tail);
  RustPResult r = rust_parser_meta_at(heap, RUST_PA_STMT, tokens);
  RustPResult s = rust_parser_read_stmt(heap, &r);
  if (!s.ok) return s;
  Vec acc = rust_parser_snoc(heap, m->acc, s.value.stmt);
  RustPMode next = {.kind = RUST_PM_STMTS, .acc = acc};
  return rust_parser_parse(heap, &next, s.value.rest);
}

/* case MStmt{meta} on `let` and MStmt{meta} tokens (MStmtItem when is_item(head_kind), else
 * MStmtExpr) */
static RustPResult rust_parser_parse_stmt(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_KLET)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult p = rust_parser_pat_top(heap, tail);
    if (!p.ok) return p;
    RustPMode next = {.kind = RUST_PM_LET_TY, .meta = m->meta, .pat = p.value.pat};
    return rust_parser_parse(heap, &next, p.value.rest);
  }
  RustPk k = rust_parser_head_kind(tokens);
  if (rust_parser_is_item(k)) {
    RustPMode next = {.kind = RUST_PM_STMT_ITEM, .meta = m->meta};
    return rust_parser_parse(heap, &next, tokens);
  }
  RustPMode next = {.kind = RUST_PM_STMT_EXPR, .meta = m->meta};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MLetTy{meta, pat} on `:` and MLetTy{meta, pat} tokens */
static RustPResult rust_parser_parse_let_ty(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_COLON)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    AstMaybeTy mt = ast_some_ty(t.value.ty);
    RustPMode next = {.kind = RUST_PM_LET_INIT, .meta = m->meta, .pat = m->pat, .maybe_ty = mt};
    return rust_parser_parse(heap, &next, t.value.rest);
  }
  AstMaybeTy mt = ast_no_ty();
  RustPMode next = {.kind = RUST_PM_LET_INIT, .meta = m->meta, .pat = m->pat, .maybe_ty = mt};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MLetInit{meta, pat, ty} on `=` and MLetInit{meta, pat, ty} tokens */
static RustPResult rust_parser_parse_let_init(Heap *heap, const RustPMode *m, RustPts tokens) {
  Str semi = str_lit(";");
  if (rust_parser_is_at(tokens, 0, RUST_PK_EQ)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult e = rust_parser_expr(heap, false, tail);
    if (!e.ok) return e;
    RustPRest t2 = rust_parser_expect(heap, semi, e.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    AstMaybeNode init = ast_some_node(e.value.node);
    const AstStmt *s = ast_slet(heap, m->meta, m->pat, m->maybe_ty, init);
    return rust_parser_done_stmt(s, t2.rest);
  }
  RustPRest t2 = rust_parser_expect(heap, semi, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  AstMaybeNode init = ast_no_node();
  const AstStmt *s = ast_slet(heap, m->meta, m->pat, m->maybe_ty, init);
  return rust_parser_done_stmt(s, t2.rest);
}

/* case MStmtItem{meta} tokens */
static RustPResult rust_parser_parse_stmt_item(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPMode im = {.kind = RUST_PM_ITEM, .meta = m->meta};
  RustPResult r = rust_parser_parse(heap, &im, tokens);
  RustPResult n = rust_parser_read_node(heap, &r);
  if (!n.ok) return n;
  const AstStmt *s = ast_sitem(heap, n.value.node);
  return rust_parser_done_stmt(s, n.value.rest);
}

/* case MStmtExpr{meta} tokens */
static RustPResult rust_parser_parse_stmt_expr(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult e = rust_parser_expr(heap, false, tokens);
  if (!e.ok) return e;
  RustPMode next = {.kind = RUST_PM_STMT_END, .meta = m->meta, .e = e.value.node};
  return rust_parser_parse(heap, &next, e.value.rest);
}

/* case MStmtEnd{meta, e} on `;` and MStmtEnd{meta, e} tokens */
static RustPResult rust_parser_parse_stmt_end(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_SEMI)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    const AstStmt *s = ast_sexpr(heap, m->meta, m->e, true);
    return rust_parser_done_stmt(s, tail);
  }
  const AstStmt *s = ast_sexpr(heap, m->meta, m->e, false);
  return rust_parser_done_stmt(s, tokens);
}

/* case MPatTop{} tokens */
static RustPResult rust_parser_parse_pat_top(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  RustPResult p = rust_parser_pat_one(heap, tokens);
  if (!p.ok) return p;
  Vec none = vec_new(heap->arena);
  Vec acc = rust_parser_snoc(heap, none, p.value.pat);
  RustPMode next = {.kind = RUST_PM_PAT_OR, .acc = acc};
  return rust_parser_parse(heap, &next, p.value.rest);
}

/* case MPatOr{acc} on `|`, MPatOr{[p]} tokens, and MPatOr{acc} tokens */
static RustPResult rust_parser_parse_pat_or(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_PIPE)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult p = rust_parser_pat_one(heap, tail);
    if (!p.ok) return p;
    Vec acc = rust_parser_snoc(heap, m->acc, p.value.pat);
    RustPMode next = {.kind = RUST_PM_PAT_OR, .acc = acc};
    return rust_parser_parse(heap, &next, p.value.rest);
  }
  VecGet only = vec_get(&m->acc, 0);
  if (m->acc.len == 1 && only.status == VEC_GET_OK) return rust_parser_done_pat(only.value, tokens);
  const AstPat *x = ast_por(heap, m->acc);
  return rust_parser_done_pat(x, tokens);
}

/* case MPat{} on `_`, on `&`, on `(`, on `- int`, on any head (is_lit_start -> PLit, else
 * MPatPath{[]} on the same tokens), and MPat{} tokens (err "a pattern"; checked first: Nil matches no
 * other case) */
static RustPResult rust_parser_parse_pat(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h == NULL) return rust_parser_fail_at(heap, "a pattern", tokens);
  RustPts tail = rust_parser_drop(tokens, 1);
  if (h->k == RUST_PK_KUNDER) {
    const AstPat *x = ast_pwild(heap);
    return rust_parser_done_pat(x, tail);
  }
  if (h->k == RUST_PK_AMP) {
    RustPResult p = rust_parser_pat_one(heap, tail);
    if (!p.ok) return p;
    const AstPat *x = ast_pref(heap, p.value.pat);
    return rust_parser_done_pat(x, p.value.rest);
  }
  if (h->k == RUST_PK_LPAREN) {
    RustPResult ps = rust_parser_pats(heap, tail);
    if (!ps.ok) return ps;
    const AstPat *x = ast_ptuple(heap, ps.value.list);
    return rust_parser_done_pat(x, ps.value.rest);
  }
  const RustPt *second = rust_parser_nth(tokens, 1);
  if (h->k == RUST_PK_MINUS && second != NULL && second->k == RUST_PK_INT_L) {
    Str parts[2] = {str_lit("-"), second->i.text};
    Str text = rust_parser_join(heap, parts, 2);
    AstLit lit = ast_lint(text);
    const AstPat *x = ast_plit(heap, lit);
    RustPts tail2 = rust_parser_drop(tokens, 2);
    return rust_parser_done_pat(x, tail2);
  }
  if (rust_parser_is_lit_start(h->k)) {
    AstLit lit = rust_parser_lit(h->k, h->i.text);
    const AstPat *x = ast_plit(heap, lit);
    return rust_parser_done_pat(x, tail);
  }
  Vec none = vec_new(heap->arena);
  RustPMode pm = {.kind = RUST_PM_PAT_PATH, .segs = none};
  return rust_parser_parse(heap, &pm, tokens);
}

/* case MPatPath{Nil{}} tokens, MPatPath{segs} on `(`, on `{`, and MPatPath{segs} tokens */
static RustPResult rust_parser_parse_pat_path(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (m->segs.len == 0) {
    RustPResult s = rust_parser_segs_at(heap, false, tokens);
    if (!s.ok) return s;
    RustPMode next = {.kind = RUST_PM_PAT_PATH, .segs = s.value.list};
    return rust_parser_parse(heap, &next, s.value.rest);
  }
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_LPAREN)) {
    RustPResult ps = rust_parser_pats(heap, tail);
    if (!ps.ok) return ps;
    const AstPat *x = ast_ptuple_struct(heap, m->segs, ps.value.list);
    return rust_parser_done_pat(x, ps.value.rest);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LBRACE)) {
    Vec none = vec_new(heap->arena);
    RustPMode fm = {.kind = RUST_PM_FIELD_PATS, .segs = m->segs, .acc = none};
    return rust_parser_parse(heap, &fm, tail);
  }
  const AstPat *x = ast_ppath(heap, m->segs);
  return rust_parser_done_pat(x, tokens);
}

/* case MPats{acc} on `)`, on `,`, and MPats{acc} tokens */
static RustPResult rust_parser_parse_pats(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RPAREN)) {
    return rust_parser_done_list(RUST_PV_PATS, m->acc, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPResult p = rust_parser_pat_top(heap, tokens);
  if (!p.ok) return p;
  Vec acc = rust_parser_snoc(heap, m->acc, p.value.pat);
  RustPMode next = {.kind = RUST_PM_PATS, .acc = acc};
  return rust_parser_parse(heap, &next, p.value.rest);
}

/* case MFieldPats{segs, acc} on `}`, on `.. }`, on `,`, on `name :`, on `name`, and
 * MFieldPats{segs, acc} tokens (err "a field pattern or `}`") */
static RustPResult rust_parser_parse_field_pats(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  RustPts tail2 = rust_parser_drop(tokens, 2);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) {
    const AstPat *x = ast_pstruct(heap, m->segs, m->acc, false);
    return rust_parser_done_pat(x, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_DOT_DOT) && rust_parser_is_at(tokens, 1, RUST_PK_RBRACE)) {
    const AstPat *x = ast_pstruct(heap, m->segs, m->acc, true);
    return rust_parser_done_pat(x, tail2);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h != NULL && h->k == RUST_PK_NAME && rust_parser_is_at(tokens, 1, RUST_PK_COLON)) {
    RustPResult p = rust_parser_pat_top(heap, tail2);
    if (!p.ok) return p;
    AstMaybePat mp = ast_some_pat(p.value.pat);
    const AstFieldPat *fp = ast_field_pat(heap, h->i.text, mp);
    Vec acc = rust_parser_snoc(heap, m->acc, fp);
    RustPMode next = {.kind = RUST_PM_FIELD_PATS, .segs = m->segs, .acc = acc};
    return rust_parser_parse(heap, &next, p.value.rest);
  }
  if (h != NULL && h->k == RUST_PK_NAME) {
    AstMaybePat mp = ast_no_pat();
    const AstFieldPat *fp = ast_field_pat(heap, h->i.text, mp);
    Vec acc = rust_parser_snoc(heap, m->acc, fp);
    RustPMode next = {.kind = RUST_PM_FIELD_PATS, .segs = m->segs, .acc = acc};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_fail_at(heap, "a field pattern or `}`", tokens);
}

/* case MTy{} on `&`, on `(`, on `[`, on `_`, on `impl`, on `name (`, and MTy{} tokens (a path) */
static RustPResult rust_parser_parse_ty(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_AMP)) {
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    const AstTy *x = ast_tref(heap, t.value.ty);
    return rust_parser_done_ty(x, t.value.rest);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LPAREN)) {
    RustPResult ts = rust_parser_tys_paren(heap, tail);
    if (!ts.ok) return ts;
    const AstTy *x = ast_ttuple(heap, ts.value.list);
    return rust_parser_done_ty(x, ts.value.rest);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LBRACK)) {
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    Str close = str_lit("]");
    RustPRest t2 = rust_parser_expect(heap, close, t.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    const AstTy *x = ast_tslice(heap, t.value.ty);
    return rust_parser_done_ty(x, t2.rest);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_KUNDER)) {
    const AstTy *x = ast_tinfer(heap);
    return rust_parser_done_ty(x, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_KIMPL)) {
    Vec none = vec_new(heap->arena);
    RustPMode bm = {.kind = RUST_PM_BOUNDS, .acc = none};
    RustPResult r = rust_parser_parse(heap, &bm, tail);
    RustPResult bs = rust_parser_read_tys(heap, &r);
    if (!bs.ok) return bs;
    const AstTy *x = ast_timpl(heap, bs.value.list);
    return rust_parser_done_ty(x, bs.value.rest);
  }
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h != NULL && h->k == RUST_PK_NAME && rust_parser_is_at(tokens, 1, RUST_PK_LPAREN)) {
    RustPts tail2 = rust_parser_drop(tokens, 2);
    RustPResult ts = rust_parser_tys_paren(heap, tail2);
    if (!ts.ok) return ts;
    RustPMode next = {.kind = RUST_PM_FN_RET, .name = h->i.text, .args = ts.value.list};
    return rust_parser_parse(heap, &next, ts.value.rest);
  }
  RustPResult s = rust_parser_segs_at(heap, true, tokens);
  if (!s.ok) return s;
  const AstTy *x = ast_tpath(heap, s.value.list);
  return rust_parser_done_ty(x, s.value.rest);
}

/* case MFnRet{name, args} on `->` and MFnRet{name, args} tokens */
static RustPResult rust_parser_parse_fn_ret(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_ARROW)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    AstMaybeTy ret = ast_some_ty(t.value.ty);
    const AstTy *x = ast_tfn_sugar(heap, m->name, m->args, ret);
    return rust_parser_done_ty(x, t.value.rest);
  }
  AstMaybeTy ret = ast_no_ty();
  const AstTy *x = ast_tfn_sugar(heap, m->name, m->args, ret);
  return rust_parser_done_ty(x, tokens);
}

/* case MTys{acc, CParen{}} on `)`, MTys{acc, CAngle{}} on `>`, MTys{acc, close} on `,`, and
 * MTys{acc, close} tokens */
static RustPResult rust_parser_parse_tys(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  bool paren = m->close == RUST_PCLOSE_PAREN && rust_parser_is_at(tokens, 0, RUST_PK_RPAREN);
  bool angle = m->close == RUST_PCLOSE_ANGLE && rust_parser_is_at(tokens, 0, RUST_PK_GT);
  if (paren || angle) return rust_parser_done_list(RUST_PV_TYS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPResult t = rust_parser_ty(heap, tokens);
  if (!t.ok) return t;
  Vec acc = rust_parser_snoc(heap, m->acc, t.value.ty);
  RustPMode next = {.kind = RUST_PM_TYS, .acc = acc, .close = m->close};
  return rust_parser_parse(heap, &next, t.value.rest);
}

/* case MBounds{acc} tokens */
static RustPResult rust_parser_parse_bounds(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult t = rust_parser_ty(heap, tokens);
  if (!t.ok) return t;
  Vec acc = rust_parser_snoc(heap, m->acc, t.value.ty);
  RustPMode next = {.kind = RUST_PM_BOUNDS_NEXT, .acc = acc};
  return rust_parser_parse(heap, &next, t.value.rest);
}

/* case MBoundsNext{acc} on `+` and MBoundsNext{acc} tokens */
static RustPResult rust_parser_parse_bounds_next(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_PLUS)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPMode next = {.kind = RUST_PM_BOUNDS, .acc = m->acc};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_done_list(RUST_PV_TYS, m->acc, tokens);
}

/* case MSeg{ty, acc, True{}} on any head and MSeg{ty, acc, ok} tokens */
static RustPResult rust_parser_parse_seg(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (m->ok && h != NULL) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPMode next = {.kind = RUST_PM_SEG_ARGS, .is_ty = m->is_ty, .acc = m->acc, .name = h->i.text};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_fail_at(heap, "a path segment", tokens);
}

/* C only: parse(MSegNext{ty, A.Seg{name, args} <> acc}, tokens) from an MSegArgs mode. */
static RustPResult rust_parser_push_seg(Heap *heap, const RustPMode *m, Vec args, RustPts tokens) {
  const AstSeg *seg = ast_seg(heap, m->name, args);
  Vec acc = rust_parser_snoc(heap, m->acc, seg);
  RustPMode next = {.kind = RUST_PM_SEG_NEXT, .is_ty = m->is_ty, .acc = acc};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MSegArgs{True{}, acc, name} on `<`, MSegArgs{False{}, acc, name} on `::` `<` and
 * MSegArgs{ty, acc, name} tokens */
static RustPResult rust_parser_parse_seg_args(Heap *heap, const RustPMode *m, RustPts tokens) {
  bool angle = m->is_ty && rust_parser_is_at(tokens, 0, RUST_PK_LT);
  bool turbo = !m->is_ty && rust_parser_is_at(tokens, 0, RUST_PK_PATH_SEP) &&
               rust_parser_is_at(tokens, 1, RUST_PK_LT);
  if (angle || turbo) {
    size_t skip = 1;
    if (turbo) skip = 2;
    RustPts tail = rust_parser_drop(tokens, skip);
    Vec none = vec_new(heap->arena);
    RustPMode tm = {.kind = RUST_PM_TYS, .acc = none, .close = RUST_PCLOSE_ANGLE};
    RustPResult r = rust_parser_parse(heap, &tm, tail);
    RustPResult ts = rust_parser_read_tys(heap, &r);
    if (!ts.ok) return ts;
    return rust_parser_push_seg(heap, m, ts.value.list, ts.value.rest);
  }
  Vec none = vec_new(heap->arena);
  return rust_parser_push_seg(heap, m, none, tokens);
}

/* case MSegNext{ty, acc} on `::` and MSegNext{ty, acc} tokens */
static RustPResult rust_parser_parse_seg_next(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_PATH_SEP)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPk k = rust_parser_head_kind(tail);
    RustPMode next = {.kind = RUST_PM_SEG, .is_ty = m->is_ty, .acc = m->acc, .ok = rust_parser_is_seg(k)};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_done_list(RUST_PV_SEGS, m->acc, tokens);
}

/* C only: the arena copy of a String, for the Str pointer lists (docs, attribute args, inner docs). */
static const Str rust_parser_str_placeholder = {0};
static const Str *rust_parser_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (!box) {
    heap->oom = true;
    return &rust_parser_str_placeholder;
  }
  *box = s;
  return box;
}

/* case MMeta{after, gap, docs, attrs} on a DocO, on `#` `[`, and MMeta{after, gap, docs, attrs}
 * tokens */
static RustPResult rust_parser_parse_meta(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h != NULL && h->k == RUST_PK_DOC_O) {
    RustPts tail = rust_parser_drop(tokens, 1);
    const Str *text = rust_parser_str_box(heap, h->i.text);
    Vec docs = rust_parser_snoc(heap, m->docs, text);
    RustPMode next = {.kind = RUST_PM_META, .after = m->after, .gap = m->gap, .docs = docs, .attrs = m->attrs};
    return rust_parser_parse(heap, &next, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_HASH) && rust_parser_is_at(tokens, 1, RUST_PK_LBRACK)) {
    RustPts tail = rust_parser_drop(tokens, 2);
    RustPMode am = {.kind = RUST_PM_ATTR};
    RustPResult r = rust_parser_parse(heap, &am, tail);
    RustPResult a = rust_parser_read_attr(heap, &r);
    if (!a.ok) return a;
    Vec attrs = rust_parser_snoc(heap, m->attrs, a.value.attr);
    RustPMode next = {.kind = RUST_PM_META, .after = m->after, .gap = m->gap, .docs = m->docs, .attrs = attrs};
    return rust_parser_parse(heap, &next, a.value.rest);
  }
  AstPos pos = rust_parser_head_pos(heap, tokens);
  AstMeta meta = ast_meta(m->gap, m->docs, m->attrs, pos);
  RustPMode next = rust_parser_after_meta(m->after, meta);
  return rust_parser_parse(heap, &next, tokens);
}

/* C only: Done{VAttr{a, rest}}. */
static RustPResult rust_parser_done_attr(const AstAttr *a, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_ATTR;
  out.value.attr = a;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* case MAttr{} on Name `]`, on Name `(`, and MAttr{} tokens */
static RustPResult rust_parser_parse_attr(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  bool named = h != NULL && h->k == RUST_PK_NAME;
  if (named && rust_parser_is_at(tokens, 1, RUST_PK_RBRACK)) {
    RustPts tail = rust_parser_drop(tokens, 2);
    Vec empty = vec_new(heap->arena);
    MaybeVec none = {false, empty};
    const AstAttr *a = ast_attr(heap, h->i.text, none);
    return rust_parser_done_attr(a, tail);
  }
  if (named && rust_parser_is_at(tokens, 1, RUST_PK_LPAREN)) {
    RustPts tail = rust_parser_drop(tokens, 2);
    Vec empty = vec_new(heap->arena);
    RustPMode sm = {.kind = RUST_PM_STRS, .acc = empty};
    RustPResult r = rust_parser_parse(heap, &sm, tail);
    RustPResult s = rust_parser_read_strs(heap, &r);
    if (!s.ok) return s;
    Str close = str_lit("]");
    RustPRest t2 = rust_parser_expect(heap, close, s.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    MaybeVec args = {true, s.value.list};
    const AstAttr *a = ast_attr(heap, h->i.text, args);
    return rust_parser_done_attr(a, t2.rest);
  }
  return rust_parser_fail_at(heap, "an attribute", tokens);
}

/* case MStrs{acc} on `)`, on `,`, on Name, and MStrs{acc} tokens */
static RustPResult rust_parser_parse_strs(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RPAREN)) return rust_parser_done_list(RUST_PV_STRS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  if (h != NULL && h->k == RUST_PK_NAME) {
    RustPMode next = {.kind = RUST_PM_STRS_PATH, .acc = m->acc, .s = h->i.text};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_fail_at(heap, "an attribute argument", tokens);
}

/* case MStrsPath{acc, s} on `::` Name and MStrsPath{acc, s} tokens */
static RustPResult rust_parser_parse_strs_path(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_PATH_SEP) && h != NULL && h->k == RUST_PK_NAME) {
    RustPts tail = rust_parser_drop(tokens, 2);
    Str parts[3] = {m->s, str_lit("::"), h->i.text};
    Str s = rust_parser_join(heap, parts, 3);
    RustPMode next = {.kind = RUST_PM_STRS_PATH, .acc = m->acc, .s = s};
    return rust_parser_parse(heap, &next, tail);
  }
  const Str *text = rust_parser_str_box(heap, m->s);
  Vec acc = rust_parser_snoc(heap, m->acc, text);
  RustPMode next = {.kind = RUST_PM_STRS, .acc = acc};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MInner{acc} on a DocI and MInner{acc} tokens */
static RustPResult rust_parser_parse_inner(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h != NULL && h->k == RUST_PK_DOC_I) {
    RustPts tail = rust_parser_drop(tokens, 1);
    const Str *text = rust_parser_str_box(heap, h->i.text);
    Vec acc = rust_parser_snoc(heap, m->acc, text);
    RustPMode next = {.kind = RUST_PM_INNER, .acc = acc};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_done_list(RUST_PV_STRS, m->acc, tokens);
}

/* case MItems{acc} tokens: MDoneNodes{acc} at an end token, else MItemsGo{acc} */
static RustPResult rust_parser_parse_items(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPk k = rust_parser_head_kind(tokens);
  if (rust_parser_is_end(k)) {
    RustPMode next = {.kind = RUST_PM_DONE_NODES, .acc = m->acc};
    return rust_parser_parse(heap, &next, tokens);
  }
  RustPMode next = {.kind = RUST_PM_ITEMS_GO, .acc = m->acc};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MDoneNodes{acc} tokens */
static RustPResult rust_parser_parse_done_nodes(Heap *heap, const RustPMode *m, RustPts tokens) {
  return rust_parser_done_list(RUST_PV_NODES, m->acc, tokens);
}

/* case MItemsGo{acc} tokens */
static RustPResult rust_parser_parse_items_go(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult r = rust_parser_meta_at(heap, RUST_PA_ITEM, tokens);
  RustPResult n = rust_parser_read_node(heap, &r);
  if (!n.ok) return n;
  Vec acc = rust_parser_snoc(heap, m->acc, n.value.node);
  RustPMode next = {.kind = RUST_PM_ITEMS, .acc = acc};
  return rust_parser_parse(heap, &next, n.value.rest);
}

/* case MItem{meta} on `pub` `(` `crate` `)`, on `pub`, and MItem{meta} tokens */
static RustPResult rust_parser_parse_item(Heap *heap, const RustPMode *m, RustPts tokens) {
  bool pub = rust_parser_is_at(tokens, 0, RUST_PK_KPUB);
  bool crate = rust_parser_is_at(tokens, 1, RUST_PK_LPAREN) && rust_parser_is_at(tokens, 2, RUST_PK_KCRATE) &&
               rust_parser_is_at(tokens, 3, RUST_PK_RPAREN);
  if (pub && crate) {
    RustPts tail = rust_parser_drop(tokens, 4);
    RustPMode next = {.kind = RUST_PM_ITEM_V, .meta = m->meta, .vis = AST_PUB_CRATE};
    return rust_parser_parse(heap, &next, tail);
  }
  if (pub) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPMode next = {.kind = RUST_PM_ITEM_V, .meta = m->meta, .vis = AST_PUB};
    return rust_parser_parse(heap, &next, tail);
  }
  RustPMode next = {.kind = RUST_PM_ITEM_V, .meta = m->meta, .vis = AST_PRIV};
  return rust_parser_parse(heap, &next, tokens);
}

/* C only: read_gens(parse(MGenerics{}, tokens)). */
static RustPResult rust_parser_gens(Heap *heap, RustPts tokens) {
  RustPMode gm = {.kind = RUST_PM_GENERICS};
  RustPResult r = rust_parser_parse(heap, &gm, tokens);
  return rust_parser_read_gens(heap, &r);
}

/* C only: the MItemV case on `fn` Name (tail = the tokens after the name). */
static RustPResult rust_parser_item_fn(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  Str open = str_lit("(");
  RustPRest t2 = rust_parser_expect(heap, open, gs.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  Vec none = vec_new(heap->arena);
  RustPMode pm = {.kind = RUST_PM_PARAMS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &pm, t2.rest);
  RustPResult ps = rust_parser_read_params(heap, &r);
  if (!ps.ok) return ps;
  RustPMode next = {.kind = RUST_PM_FN_SIG,
                    .meta = m->meta,
                    .vis = m->vis,
                    .name = name,
                    .gens = gs.value.list,
                    .params = ps.value.list};
  return rust_parser_parse(heap, &next, ps.value.rest);
}

/* C only: the MItemV case on `struct` Name. */
static RustPResult rust_parser_item_struct(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  RustPMode bm = {.kind = RUST_PM_BODY, .st = true};
  RustPResult r = rust_parser_parse(heap, &bm, gs.value.rest);
  RustPResult b = rust_parser_read_body(heap, &r);
  if (!b.ok) return b;
  const AstNode *x = ast_istruct(heap, m->meta, m->vis, name, gs.value.list, b.value.body);
  return rust_parser_done_node(x, b.value.rest);
}

/* C only: the MItemV case on `enum` Name. */
static RustPResult rust_parser_item_enum(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  Str open = str_lit("{");
  RustPRest t2 = rust_parser_expect(heap, open, gs.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  Vec none = vec_new(heap->arena);
  RustPMode vm = {.kind = RUST_PM_VARIANTS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &vm, t2.rest);
  RustPResult vs = rust_parser_read_variants(heap, &r);
  if (!vs.ok) return vs;
  const AstNode *x = ast_ienum(heap, m->meta, m->vis, name, gs.value.list, vs.value.list);
  return rust_parser_done_node(x, vs.value.rest);
}

/* C only: the MItemV case on `use`. */
static RustPResult rust_parser_item_use(Heap *heap, const RustPMode *m, RustPts tail) {
  RustPMode um = {.kind = RUST_PM_USE};
  RustPResult r = rust_parser_parse(heap, &um, tail);
  RustPResult u = rust_parser_read_use(heap, &r);
  if (!u.ok) return u;
  Str semi = str_lit(";");
  RustPRest t2 = rust_parser_expect(heap, semi, u.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  const AstNode *x = ast_iuse(heap, m->meta, m->vis, u.value.use);
  return rust_parser_done_node(x, t2.rest);
}

/* C only: the MItemV case on `mod` Name `{` (tail = the tokens after the `{`). */
static RustPResult rust_parser_item_mod(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  Vec none = vec_new(heap->arena);
  RustPMode im = {.kind = RUST_PM_INNER, .acc = none};
  RustPResult r = rust_parser_parse(heap, &im, tail);
  RustPResult d = rust_parser_read_strs(heap, &r);
  if (!d.ok) return d;
  Vec empty = vec_new(heap->arena);
  RustPMode nm = {.kind = RUST_PM_ITEMS, .acc = empty};
  RustPResult r2 = rust_parser_parse(heap, &nm, d.value.rest);
  RustPResult n = rust_parser_read_nodes(heap, &r2);
  if (!n.ok) return n;
  Str close = str_lit("}");
  RustPRest t2 = rust_parser_expect(heap, close, n.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  const AstNode *x = ast_imod(heap, m->meta, m->vis, name, true, d.value.list, n.value.list);
  return rust_parser_done_node(x, t2.rest);
}

/* C only: the MItemV case on `impl`. */
static RustPResult rust_parser_item_impl(Heap *heap, const RustPMode *m, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  RustPResult t = rust_parser_ty(heap, gs.value.rest);
  if (!t.ok) return t;
  RustPMode next = {.kind = RUST_PM_IMPL_FOR, .meta = m->meta, .gens = gs.value.list, .ty = t.value.ty};
  return rust_parser_parse(heap, &next, t.value.rest);
}

/* C only: the MItemV case on `trait` Name. */
static RustPResult rust_parser_item_trait(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  RustPMode next = {.kind = RUST_PM_TRAIT_SUP, .meta = m->meta, .vis = m->vis, .name = name, .gens = gs.value.list};
  return rust_parser_parse(heap, &next, gs.value.rest);
}

/* C only: the MItemV case on `const` Name `:` (tail = the tokens after the `:`). */
static RustPResult rust_parser_item_const(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult t = rust_parser_ty(heap, tail);
  if (!t.ok) return t;
  RustPMode next = {.kind = RUST_PM_CONST_VAL, .meta = m->meta, .vis = m->vis, .name = name, .ty = t.value.ty};
  return rust_parser_parse(heap, &next, t.value.rest);
}

/* C only: the MItemV case on `type` Name. */
static RustPResult rust_parser_item_type(Heap *heap, const RustPMode *m, Str name, RustPts tail) {
  RustPResult gs = rust_parser_gens(heap, tail);
  if (!gs.ok) return gs;
  RustPMode next = {.kind = RUST_PM_TYPE_VAL, .meta = m->meta, .vis = m->vis, .name = name, .gens = gs.value.list};
  return rust_parser_parse(heap, &next, gs.value.rest);
}

/* case MItemV{meta, vis} on `fn` Name, `struct` Name, `enum` Name, `use`, `mod` Name `;`,
 * `mod` Name `{`, `impl`, `trait` Name, `const` Name `:`, `type` Name, and MItemV{meta, vis} tokens */
static RustPResult rust_parser_parse_item_v(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *n1 = rust_parser_nth(tokens, 1);
  bool named = n1 != NULL && n1->k == RUST_PK_NAME;
  Str name = str_empty();
  if (named) name = n1->i.text;
  RustPts tail1 = rust_parser_drop(tokens, 1);
  RustPts tail2 = rust_parser_drop(tokens, 2);
  RustPts tail3 = rust_parser_drop(tokens, 3);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KFN) && named) return rust_parser_item_fn(heap, m, name, tail2);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KSTRUCT) && named) return rust_parser_item_struct(heap, m, name, tail2);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KENUM) && named) return rust_parser_item_enum(heap, m, name, tail2);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KUSE)) return rust_parser_item_use(heap, m, tail1);
  bool is_mod = rust_parser_is_at(tokens, 0, RUST_PK_KMOD) && named;
  if (is_mod && rust_parser_is_at(tokens, 2, RUST_PK_SEMI)) {
    Vec inner = vec_new(heap->arena);
    Vec items = vec_new(heap->arena);
    const AstNode *x = ast_imod(heap, m->meta, m->vis, name, false, inner, items);
    return rust_parser_done_node(x, tail3);
  }
  if (is_mod && rust_parser_is_at(tokens, 2, RUST_PK_LBRACE)) return rust_parser_item_mod(heap, m, name, tail3);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KIMPL)) return rust_parser_item_impl(heap, m, tail1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KTRAIT) && named) return rust_parser_item_trait(heap, m, name, tail2);
  bool is_const = rust_parser_is_at(tokens, 0, RUST_PK_KCONST) && named;
  if (is_const && rust_parser_is_at(tokens, 2, RUST_PK_COLON)) return rust_parser_item_const(heap, m, name, tail3);
  if (rust_parser_is_at(tokens, 0, RUST_PK_KTYPE) && named) return rust_parser_item_type(heap, m, name, tail2);
  return rust_parser_fail_at(heap, "an item", tokens);
}

/* case MFnSig{meta, vis, name, gens, params} on `->` and MFnSig{meta, vis, name, gens, params}
 * tokens */
static RustPResult rust_parser_parse_fn_sig(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_ARROW)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    AstMaybeTy ret = ast_some_ty(t.value.ty);
    AstSig sig = ast_sig(m->name, m->gens, m->params, ret);
    RustPMode next = {.kind = RUST_PM_FN_BODY, .meta = m->meta, .vis = m->vis, .sig = sig};
    return rust_parser_parse(heap, &next, t.value.rest);
  }
  AstMaybeTy ret = ast_no_ty();
  AstSig sig = ast_sig(m->name, m->gens, m->params, ret);
  RustPMode next = {.kind = RUST_PM_FN_BODY, .meta = m->meta, .vis = m->vis, .sig = sig};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MFnBody{meta, vis, sig} on `;`, on `{`, and MFnBody{meta, vis, sig} tokens */
static RustPResult rust_parser_parse_fn_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_SEMI)) {
    Vec empty = vec_new(heap->arena);
    MaybeVec none = {false, empty};
    const AstNode *x = ast_ifn(heap, m->meta, m->vis, m->sig, none);
    return rust_parser_done_node(x, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LBRACE)) {
    RustPResult b = rust_parser_stmts(heap, tail);
    if (!b.ok) return b;
    MaybeVec body = {true, b.value.list};
    const AstNode *x = ast_ifn(heap, m->meta, m->vis, m->sig, body);
    return rust_parser_done_node(x, b.value.rest);
  }
  return rust_parser_fail_at(heap, "`{` or `;`", tokens);
}

/* case MGenerics{} on `<` and MGenerics{} tokens */
static RustPResult rust_parser_parse_generics(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  Vec none = vec_new(heap->arena);
  if (rust_parser_is_at(tokens, 0, RUST_PK_LT)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPMode next = {.kind = RUST_PM_GPARAMS, .acc = none};
    return rust_parser_parse(heap, &next, tail);
  }
  return rust_parser_done_list(RUST_PV_GENS, none, tokens);
}

/* case MGParams{acc} on `>`, on `,`, on Name `:`, on Name, and MGParams{acc} tokens */
static RustPResult rust_parser_parse_gparams(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_GT)) return rust_parser_done_list(RUST_PV_GENS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h == NULL || h->k != RUST_PK_NAME) return rust_parser_fail_at(heap, "a generic parameter", tokens);
  if (rust_parser_is_at(tokens, 1, RUST_PK_COLON)) {
    RustPts tail2 = rust_parser_drop(tokens, 2);
    Vec none = vec_new(heap->arena);
    RustPMode bm = {.kind = RUST_PM_BOUNDS, .acc = none};
    RustPResult r = rust_parser_parse(heap, &bm, tail2);
    RustPResult bs = rust_parser_read_tys(heap, &r);
    if (!bs.ok) return bs;
    const AstGParam *g = ast_gparam(heap, h->i.text, bs.value.list);
    Vec acc = rust_parser_snoc(heap, m->acc, g);
    RustPMode next = {.kind = RUST_PM_GPARAMS, .acc = acc};
    return rust_parser_parse(heap, &next, bs.value.rest);
  }
  Vec empty = vec_new(heap->arena);
  const AstGParam *g = ast_gparam(heap, h->i.text, empty);
  Vec acc = rust_parser_snoc(heap, m->acc, g);
  RustPMode next = {.kind = RUST_PM_GPARAMS, .acc = acc};
  return rust_parser_parse(heap, &next, tail);
}

/* C only: parse(MParams{p <> acc}, tokens). */
static RustPResult rust_parser_push_param(Heap *heap, const RustPMode *m, const AstParam *p, RustPts tokens) {
  Vec acc = rust_parser_snoc(heap, m->acc, p);
  RustPMode next = {.kind = RUST_PM_PARAMS, .acc = acc};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MParams{acc} on `)`, on `,`, on `&` `self`, on `self`, and MParams{acc} tokens */
static RustPResult rust_parser_parse_params(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RPAREN)) return rust_parser_done_list(RUST_PV_PARAMS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_AMP) && rust_parser_is_at(tokens, 1, RUST_PK_KSELF_V)) {
    RustPts tail2 = rust_parser_drop(tokens, 2);
    const AstParam *p = ast_pself(heap, true);
    return rust_parser_push_param(heap, m, p, tail2);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_KSELF_V)) {
    const AstParam *p = ast_pself(heap, false);
    return rust_parser_push_param(heap, m, p, tail);
  }
  RustPResult p = rust_parser_pat_one(heap, tokens);
  if (!p.ok) return p;
  Str colon = str_lit(":");
  RustPRest t2 = rust_parser_expect(heap, colon, p.value.rest);
  if (!t2.ok) return rust_parser_fail(t2.message);
  RustPResult t = rust_parser_ty(heap, t2.rest);
  if (!t.ok) return t;
  const AstParam *x = ast_ptyped(heap, p.value.pat, t.value.ty);
  return rust_parser_push_param(heap, m, x, t.value.rest);
}

/* C only: Done{VBody{b, rest}}. */
static RustPResult rust_parser_done_body(AstBody b, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_BODY;
  out.value.body = b;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: read_fields(parse(MFields{[], named}, tokens)). */
static RustPResult rust_parser_fields(Heap *heap, bool named, RustPts tokens) {
  Vec none = vec_new(heap->arena);
  RustPMode fm = {.kind = RUST_PM_FIELDS, .acc = none, .named = named};
  RustPResult r = rust_parser_parse(heap, &fm, tokens);
  return rust_parser_read_fields(heap, &r);
}

/* case MBody{True{}} on `;`, MBody{st} on `(`, on `{`, and MBody{st} tokens (`st` True: a struct
 * body, which a `;` closes when unit or tuple) */
static RustPResult rust_parser_parse_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (m->st && rust_parser_is_at(tokens, 0, RUST_PK_SEMI)) {
    AstBody unit = ast_bunit();
    return rust_parser_done_body(unit, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LPAREN)) {
    RustPResult f = rust_parser_fields(heap, false, tail);
    if (!f.ok) return f;
    AstBody body = ast_btuple(f.value.list);
    RustPMode next = {.kind = RUST_PM_BODY_END, .st = m->st, .body = body};
    return rust_parser_parse(heap, &next, f.value.rest);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LBRACE)) {
    RustPResult f = rust_parser_fields(heap, true, tail);
    if (!f.ok) return f;
    AstBody body = ast_bnamed(f.value.list);
    return rust_parser_done_body(body, f.value.rest);
  }
  AstBody unit = ast_bunit();
  return rust_parser_done_body(unit, tokens);
}

/* case MBodyEnd{True{}, body} on `;` and MBodyEnd{st, body} tokens */
static RustPResult rust_parser_parse_body_end(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)heap;
  if (m->st && rust_parser_is_at(tokens, 0, RUST_PK_SEMI)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    return rust_parser_done_body(m->body, tail);
  }
  return rust_parser_done_body(m->body, tokens);
}

/* case MFields{acc, named} on `)`, on `}`, on `,`, and MFields{acc, named} tokens */
static RustPResult rust_parser_parse_fields(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  bool close = rust_parser_is_at(tokens, 0, RUST_PK_RPAREN) || rust_parser_is_at(tokens, 0, RUST_PK_RBRACE);
  if (close) return rust_parser_done_list(RUST_PV_FIELDS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  Vec docs = vec_new(heap->arena);
  Vec attrs = vec_new(heap->arena);
  RustPAfter after = {.kind = RUST_PA_FIELD, .named = m->named};
  bool gap = rust_parser_head_gap(tokens);
  RustPMode mm = {.kind = RUST_PM_META, .after = after, .gap = gap, .docs = docs, .attrs = attrs};
  RustPResult r = rust_parser_parse(heap, &mm, tokens);
  RustPResult f = rust_parser_read_field(heap, &r);
  if (!f.ok) return f;
  Vec acc = rust_parser_snoc(heap, m->acc, f.value.field);
  RustPMode next = {.kind = RUST_PM_FIELDS, .acc = acc, .named = m->named};
  return rust_parser_parse(heap, &next, f.value.rest);
}

/* case MField{meta, named} on `pub` `(` `crate` `)`, on `pub`, and MField{meta, named} tokens */
static RustPResult rust_parser_parse_field(Heap *heap, const RustPMode *m, RustPts tokens) {
  bool is_pub = rust_parser_is_at(tokens, 0, RUST_PK_KPUB);
  bool crate = is_pub && rust_parser_is_at(tokens, 1, RUST_PK_LPAREN) &&
               rust_parser_is_at(tokens, 2, RUST_PK_KCRATE) && rust_parser_is_at(tokens, 3, RUST_PK_RPAREN);
  if (crate) {
    RustPts tail = rust_parser_drop(tokens, 4);
    RustPMode next = {.kind = RUST_PM_FIELD_V, .meta = m->meta, .named = m->named, .vis = AST_PUB_CRATE};
    return rust_parser_parse(heap, &next, tail);
  }
  if (is_pub) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPMode next = {.kind = RUST_PM_FIELD_V, .meta = m->meta, .named = m->named, .vis = AST_PUB};
    return rust_parser_parse(heap, &next, tail);
  }
  RustPMode next = {.kind = RUST_PM_FIELD_V, .meta = m->meta, .named = m->named, .vis = AST_PRIV};
  return rust_parser_parse(heap, &next, tokens);
}

/* C only: Done{VField{f, rest}}. */
static RustPResult rust_parser_done_field(const AstField *f, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_FIELD;
  out.value.field = f;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* case MFieldV{meta, True{}, vis} on Name `:`, MFieldV{meta, False{}, vis} tokens, and
 * MFieldV{meta, named, vis} tokens */
static RustPResult rust_parser_parse_field_v(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  bool named_at = h != NULL && h->k == RUST_PK_NAME && rust_parser_is_at(tokens, 1, RUST_PK_COLON);
  if (m->named && named_at) {
    RustPts tail = rust_parser_drop(tokens, 2);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    const AstField *f = ast_nfield(heap, m->meta, m->vis, h->i.text, t.value.ty);
    return rust_parser_done_field(f, t.value.rest);
  }
  if (!m->named) {
    RustPResult t = rust_parser_ty(heap, tokens);
    if (!t.ok) return t;
    const AstField *f = ast_tfield(heap, m->meta, m->vis, t.value.ty);
    return rust_parser_done_field(f, t.value.rest);
  }
  return rust_parser_fail_at(heap, "a field", tokens);
}

/* case MVariants{acc} on `}`, on `,`, and MVariants{acc} tokens */
static RustPResult rust_parser_parse_variants(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) return rust_parser_done_list(RUST_PV_VARIANTS, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPResult r = rust_parser_meta_at(heap, RUST_PA_VARIANT, tokens);
  RustPResult v = rust_parser_read_variant(heap, &r);
  if (!v.ok) return v;
  Vec acc = rust_parser_snoc(heap, m->acc, v.value.variant);
  RustPMode next = {.kind = RUST_PM_VARIANTS, .acc = acc};
  return rust_parser_parse(heap, &next, v.value.rest);
}

/* case MVariant{meta} on Name and MVariant{meta} tokens */
static RustPResult rust_parser_parse_variant(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (h == NULL || h->k != RUST_PK_NAME) return rust_parser_fail_at(heap, "a variant", tokens);
  RustPts tail = rust_parser_drop(tokens, 1);
  RustPMode bm = {.kind = RUST_PM_BODY, .st = false};
  RustPResult r = rust_parser_parse(heap, &bm, tail);
  RustPResult b = rust_parser_read_body(heap, &r);
  if (!b.ok) return b;
  const AstVariant *v = ast_variant(heap, m->meta, h->i.text, b.value.body);
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_VARIANT;
  out.value.variant = v;
  out.value.rest = b.value.rest;
  out.message = str_empty();
  return out;
}

/* C only: Done{VUse{u, rest}}. */
static RustPResult rust_parser_done_use(const AstUseTree *u, RustPts rest) {
  RustPResult out = {.ok = true};
  out.value.kind = RUST_PV_USE;
  out.value.use = u;
  out.value.rest = rest;
  out.message = str_empty();
  return out;
}

/* C only: read_use(parse(MUse{}, tokens)). */
static RustPResult rust_parser_use(Heap *heap, RustPts tokens) {
  RustPMode um = {.kind = RUST_PM_USE};
  RustPResult r = rust_parser_parse(heap, &um, tokens);
  return rust_parser_read_use(heap, &r);
}

/* case MUse{} on `*`, on `{`, and MUse{} tokens */
static RustPResult rust_parser_parse_use(Heap *heap, const RustPMode *m, RustPts tokens) {
  (void)m;
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_STAR)) {
    const AstUseTree *g = ast_uglob(heap);
    return rust_parser_done_use(g, tail);
  }
  if (rust_parser_is_at(tokens, 0, RUST_PK_LBRACE)) {
    Vec none = vec_new(heap->arena);
    RustPMode um = {.kind = RUST_PM_USES, .acc = none};
    RustPResult r = rust_parser_parse(heap, &um, tail);
    RustPResult us = rust_parser_read_uses(heap, &r);
    if (!us.ok) return us;
    const AstUseTree *g = ast_ugroup(heap, us.value.list);
    return rust_parser_done_use(g, us.value.rest);
  }
  RustPk k = rust_parser_head_kind(tokens);
  RustPMode next = {.kind = RUST_PM_USE_SEG, .ok = rust_parser_is_seg(k)};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MUseSeg{True{}} on k `::`, on k `as` Name, on k, and MUseSeg{ok} tokens */
static RustPResult rust_parser_parse_use_seg(Heap *heap, const RustPMode *m, RustPts tokens) {
  const RustPt *h = rust_parser_nth(tokens, 0);
  if (!m->ok || h == NULL) return rust_parser_fail_at(heap, "a use path", tokens);
  if (rust_parser_is_at(tokens, 1, RUST_PK_PATH_SEP)) {
    RustPts tail = rust_parser_drop(tokens, 2);
    RustPResult u = rust_parser_use(heap, tail);
    if (!u.ok) return u;
    const AstUseTree *x = ast_upath(heap, h->i.text, u.value.use);
    return rust_parser_done_use(x, u.value.rest);
  }
  const RustPt *a = rust_parser_nth(tokens, 2);
  if (rust_parser_is_at(tokens, 1, RUST_PK_KAS) && a != NULL && a->k == RUST_PK_NAME) {
    RustPts tail = rust_parser_drop(tokens, 3);
    MaybeStr alias = {true, a->i.text};
    const AstUseTree *x = ast_uname(heap, h->i.text, alias);
    return rust_parser_done_use(x, tail);
  }
  RustPts tail = rust_parser_drop(tokens, 1);
  MaybeStr none = {false, str_empty()};
  const AstUseTree *x = ast_uname(heap, h->i.text, none);
  return rust_parser_done_use(x, tail);
}

/* case MUses{acc} on `}`, on `,`, and MUses{acc} tokens */
static RustPResult rust_parser_parse_uses(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPts tail = rust_parser_drop(tokens, 1);
  if (rust_parser_is_at(tokens, 0, RUST_PK_RBRACE)) return rust_parser_done_list(RUST_PV_USES, m->acc, tail);
  if (rust_parser_is_at(tokens, 0, RUST_PK_COMMA)) return rust_parser_parse(heap, m, tail);
  RustPResult u = rust_parser_use(heap, tokens);
  if (!u.ok) return u;
  Vec acc = rust_parser_snoc(heap, m->acc, u.value.use);
  RustPMode next = {.kind = RUST_PM_USES, .acc = acc};
  return rust_parser_parse(heap, &next, u.value.rest);
}

/* case MImplFor{meta, gens, ty} on `for` and MImplFor{meta, gens, ty} tokens */
static RustPResult rust_parser_parse_impl_for(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_KFOR)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult s = rust_parser_ty(heap, tail);
    if (!s.ok) return s;
    AstMaybeTy tr = ast_some_ty(m->ty);
    RustPMode next = {.kind = RUST_PM_IMPL_BODY, .meta = m->meta, .gens = m->gens, .tr = tr, .self_ty = s.value.ty};
    return rust_parser_parse(heap, &next, s.value.rest);
  }
  AstMaybeTy tr = ast_no_ty();
  RustPMode next = {.kind = RUST_PM_IMPL_BODY, .meta = m->meta, .gens = m->gens, .tr = tr, .self_ty = m->ty};
  return rust_parser_parse(heap, &next, tokens);
}

/* C only: expect("{"), read_nodes(parse(MItems{[]}, ...)), expect("}"): Done{VNodes{items, t3}}. */
static RustPResult rust_parser_braced_items(Heap *heap, RustPts tokens) {
  Str open = str_lit("{");
  RustPRest t2 = rust_parser_expect(heap, open, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  Vec none = vec_new(heap->arena);
  RustPMode im = {.kind = RUST_PM_ITEMS, .acc = none};
  RustPResult r = rust_parser_parse(heap, &im, t2.rest);
  RustPResult n = rust_parser_read_nodes(heap, &r);
  if (!n.ok) return n;
  Str close = str_lit("}");
  RustPRest t3 = rust_parser_expect(heap, close, n.value.rest);
  if (!t3.ok) return rust_parser_fail(t3.message);
  return rust_parser_done_list(RUST_PV_NODES, n.value.list, t3.rest);
}

/* case MImplBody{meta, gens, tr, self_ty} tokens */
static RustPResult rust_parser_parse_impl_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult n = rust_parser_braced_items(heap, tokens);
  if (!n.ok) return n;
  const AstNode *x = ast_iimpl(heap, m->meta, m->gens, m->tr, m->self_ty, n.value.list);
  return rust_parser_done_node(x, n.value.rest);
}

/* case MTraitSup{meta, vis, name, gens} on `:` and MTraitSup{meta, vis, name, gens} tokens */
static RustPResult rust_parser_parse_trait_sup(Heap *heap, const RustPMode *m, RustPts tokens) {
  if (rust_parser_is_at(tokens, 0, RUST_PK_COLON)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    Vec none = vec_new(heap->arena);
    RustPMode bm = {.kind = RUST_PM_BOUNDS, .acc = none};
    RustPResult r = rust_parser_parse(heap, &bm, tail);
    RustPResult bs = rust_parser_read_tys(heap, &r);
    if (!bs.ok) return bs;
    RustPMode next = {.kind = RUST_PM_TRAIT_BODY,
                      .meta = m->meta,
                      .vis = m->vis,
                      .name = m->name,
                      .gens = m->gens,
                      .supers = bs.value.list};
    return rust_parser_parse(heap, &next, bs.value.rest);
  }
  Vec none = vec_new(heap->arena);
  RustPMode next = {.kind = RUST_PM_TRAIT_BODY,
                    .meta = m->meta,
                    .vis = m->vis,
                    .name = m->name,
                    .gens = m->gens,
                    .supers = none};
  return rust_parser_parse(heap, &next, tokens);
}

/* case MTraitBody{meta, vis, name, gens, supers} tokens */
static RustPResult rust_parser_parse_trait_body(Heap *heap, const RustPMode *m, RustPts tokens) {
  RustPResult n = rust_parser_braced_items(heap, tokens);
  if (!n.ok) return n;
  const AstNode *x = ast_itrait(heap, m->meta, m->vis, m->name, m->gens, m->supers, n.value.list);
  return rust_parser_done_node(x, n.value.rest);
}

/* case MConstVal{meta, vis, name, ty} on `=` and MConstVal{meta, vis, name, ty} tokens */
static RustPResult rust_parser_parse_const_val(Heap *heap, const RustPMode *m, RustPts tokens) {
  Str semi = str_lit(";");
  if (rust_parser_is_at(tokens, 0, RUST_PK_EQ)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult e = rust_parser_expr(heap, false, tail);
    if (!e.ok) return e;
    RustPRest t2 = rust_parser_expect(heap, semi, e.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    AstMaybeNode value = ast_some_node(e.value.node);
    const AstNode *x = ast_iconst(heap, m->meta, m->vis, m->name, m->ty, value);
    return rust_parser_done_node(x, t2.rest);
  }
  RustPRest t2 = rust_parser_expect(heap, semi, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  AstMaybeNode none = ast_no_node();
  const AstNode *x = ast_iconst(heap, m->meta, m->vis, m->name, m->ty, none);
  return rust_parser_done_node(x, t2.rest);
}

/* case MTypeVal{meta, vis, name, gens} on `=` and MTypeVal{meta, vis, name, gens} tokens */
static RustPResult rust_parser_parse_type_val(Heap *heap, const RustPMode *m, RustPts tokens) {
  Str semi = str_lit(";");
  if (rust_parser_is_at(tokens, 0, RUST_PK_EQ)) {
    RustPts tail = rust_parser_drop(tokens, 1);
    RustPResult t = rust_parser_ty(heap, tail);
    if (!t.ok) return t;
    RustPRest t2 = rust_parser_expect(heap, semi, t.value.rest);
    if (!t2.ok) return rust_parser_fail(t2.message);
    AstMaybeTy ty = ast_some_ty(t.value.ty);
    const AstNode *x = ast_itype(heap, m->meta, m->vis, m->name, m->gens, ty);
    return rust_parser_done_node(x, t2.rest);
  }
  RustPRest t2 = rust_parser_expect(heap, semi, tokens);
  if (!t2.ok) return rust_parser_fail(t2.message);
  AstMaybeTy none = ast_no_ty();
  const AstNode *x = ast_itype(heap, m->meta, m->vis, m->name, m->gens, none);
  return rust_parser_done_node(x, t2.rest);
}

/* def parse(mode, tokens): the dispatcher, an exhaustive switch over the Mode tag (D-205c). */
static RustPResult rust_parser_parse(Heap *heap, const RustPMode *mode, RustPts tokens) {
  RustPResult out = {.ok = false};
  switch (mode->kind) {
    case RUST_PM_BIN: out = rust_parser_parse_bin(heap, mode, tokens); break;
    case RUST_PM_BIN_REST: out = rust_parser_parse_bin_rest(heap, mode, tokens); break;
    case RUST_PM_BIN_OP: out = rust_parser_parse_bin_op(heap, mode, tokens); break;
    case RUST_PM_BIN_GO: out = rust_parser_parse_bin_go(heap, mode, tokens); break;
    case RUST_PM_DONE: out = rust_parser_parse_done(heap, mode, tokens); break;
    case RUST_PM_UNARY: out = rust_parser_parse_unary(heap, mode, tokens); break;
    case RUST_PM_UNARY_GO: out = rust_parser_parse_unary_go(heap, mode, tokens); break;
    case RUST_PM_POST: out = rust_parser_parse_post(heap, mode, tokens); break;
    case RUST_PM_PRIM: out = rust_parser_parse_prim(heap, mode, tokens); break;
    case RUST_PM_PATH_EXPR: out = rust_parser_parse_path_expr(heap, mode, tokens); break;
    case RUST_PM_PAREN: out = rust_parser_parse_paren(heap, mode, tokens); break;
    case RUST_PM_PAREN_REST: out = rust_parser_parse_paren_rest(heap, mode, tokens); break;
    case RUST_PM_EXPRS: out = rust_parser_parse_exprs(heap, mode, tokens); break;
    case RUST_PM_CLOSURE_RET: out = rust_parser_parse_closure_ret(heap, mode, tokens); break;
    case RUST_PM_CPARAMS: out = rust_parser_parse_cparams(heap, mode, tokens); break;
    case RUST_PM_CPARAM_TY: out = rust_parser_parse_cparam_ty(heap, mode, tokens); break;
    case RUST_PM_IF: out = rust_parser_parse_if(heap, mode, tokens); break;
    case RUST_PM_IF_BODY: out = rust_parser_parse_if_body(heap, mode, tokens); break;
    case RUST_PM_ELSE: out = rust_parser_parse_else(heap, mode, tokens); break;
    case RUST_PM_MATCH: out = rust_parser_parse_match(heap, mode, tokens); break;
    case RUST_PM_ARMS: out = rust_parser_parse_arms(heap, mode, tokens); break;
    case RUST_PM_ARM: out = rust_parser_parse_arm(heap, mode, tokens); break;
    case RUST_PM_ARM_GUARD: out = rust_parser_parse_arm_guard(heap, mode, tokens); break;
    case RUST_PM_ARM_BODY: out = rust_parser_parse_arm_body(heap, mode, tokens); break;
    case RUST_PM_ARM_END: out = rust_parser_parse_arm_end(heap, mode, tokens); break;
    case RUST_PM_INITS: out = rust_parser_parse_inits(heap, mode, tokens); break;
    case RUST_PM_STMTS: out = rust_parser_parse_stmts(heap, mode, tokens); break;
    case RUST_PM_STMT: out = rust_parser_parse_stmt(heap, mode, tokens); break;
    case RUST_PM_LET_TY: out = rust_parser_parse_let_ty(heap, mode, tokens); break;
    case RUST_PM_LET_INIT: out = rust_parser_parse_let_init(heap, mode, tokens); break;
    case RUST_PM_STMT_ITEM: out = rust_parser_parse_stmt_item(heap, mode, tokens); break;
    case RUST_PM_STMT_EXPR: out = rust_parser_parse_stmt_expr(heap, mode, tokens); break;
    case RUST_PM_STMT_END: out = rust_parser_parse_stmt_end(heap, mode, tokens); break;
    case RUST_PM_PAT_TOP: out = rust_parser_parse_pat_top(heap, mode, tokens); break;
    case RUST_PM_PAT_OR: out = rust_parser_parse_pat_or(heap, mode, tokens); break;
    case RUST_PM_PAT: out = rust_parser_parse_pat(heap, mode, tokens); break;
    case RUST_PM_PAT_PATH: out = rust_parser_parse_pat_path(heap, mode, tokens); break;
    case RUST_PM_PATS: out = rust_parser_parse_pats(heap, mode, tokens); break;
    case RUST_PM_FIELD_PATS: out = rust_parser_parse_field_pats(heap, mode, tokens); break;
    case RUST_PM_TY: out = rust_parser_parse_ty(heap, mode, tokens); break;
    case RUST_PM_FN_RET: out = rust_parser_parse_fn_ret(heap, mode, tokens); break;
    case RUST_PM_TYS: out = rust_parser_parse_tys(heap, mode, tokens); break;
    case RUST_PM_BOUNDS: out = rust_parser_parse_bounds(heap, mode, tokens); break;
    case RUST_PM_BOUNDS_NEXT: out = rust_parser_parse_bounds_next(heap, mode, tokens); break;
    case RUST_PM_SEG: out = rust_parser_parse_seg(heap, mode, tokens); break;
    case RUST_PM_SEG_ARGS: out = rust_parser_parse_seg_args(heap, mode, tokens); break;
    case RUST_PM_SEG_NEXT: out = rust_parser_parse_seg_next(heap, mode, tokens); break;
    case RUST_PM_META: out = rust_parser_parse_meta(heap, mode, tokens); break;
    case RUST_PM_ATTR: out = rust_parser_parse_attr(heap, mode, tokens); break;
    case RUST_PM_STRS: out = rust_parser_parse_strs(heap, mode, tokens); break;
    case RUST_PM_STRS_PATH: out = rust_parser_parse_strs_path(heap, mode, tokens); break;
    case RUST_PM_INNER: out = rust_parser_parse_inner(heap, mode, tokens); break;
    case RUST_PM_ITEMS: out = rust_parser_parse_items(heap, mode, tokens); break;
    case RUST_PM_ITEMS_GO: out = rust_parser_parse_items_go(heap, mode, tokens); break;
    case RUST_PM_DONE_NODES: out = rust_parser_parse_done_nodes(heap, mode, tokens); break;
    case RUST_PM_ITEM: out = rust_parser_parse_item(heap, mode, tokens); break;
    case RUST_PM_ITEM_V: out = rust_parser_parse_item_v(heap, mode, tokens); break;
    case RUST_PM_FN_SIG: out = rust_parser_parse_fn_sig(heap, mode, tokens); break;
    case RUST_PM_FN_BODY: out = rust_parser_parse_fn_body(heap, mode, tokens); break;
    case RUST_PM_GENERICS: out = rust_parser_parse_generics(heap, mode, tokens); break;
    case RUST_PM_GPARAMS: out = rust_parser_parse_gparams(heap, mode, tokens); break;
    case RUST_PM_PARAMS: out = rust_parser_parse_params(heap, mode, tokens); break;
    case RUST_PM_BODY: out = rust_parser_parse_body(heap, mode, tokens); break;
    case RUST_PM_BODY_END: out = rust_parser_parse_body_end(heap, mode, tokens); break;
    case RUST_PM_FIELDS: out = rust_parser_parse_fields(heap, mode, tokens); break;
    case RUST_PM_FIELD: out = rust_parser_parse_field(heap, mode, tokens); break;
    case RUST_PM_FIELD_V: out = rust_parser_parse_field_v(heap, mode, tokens); break;
    case RUST_PM_VARIANTS: out = rust_parser_parse_variants(heap, mode, tokens); break;
    case RUST_PM_VARIANT: out = rust_parser_parse_variant(heap, mode, tokens); break;
    case RUST_PM_USE: out = rust_parser_parse_use(heap, mode, tokens); break;
    case RUST_PM_USE_SEG: out = rust_parser_parse_use_seg(heap, mode, tokens); break;
    case RUST_PM_USES: out = rust_parser_parse_uses(heap, mode, tokens); break;
    case RUST_PM_IMPL_FOR: out = rust_parser_parse_impl_for(heap, mode, tokens); break;
    case RUST_PM_IMPL_BODY: out = rust_parser_parse_impl_body(heap, mode, tokens); break;
    case RUST_PM_TRAIT_SUP: out = rust_parser_parse_trait_sup(heap, mode, tokens); break;
    case RUST_PM_TRAIT_BODY: out = rust_parser_parse_trait_body(heap, mode, tokens); break;
    case RUST_PM_CONST_VAL: out = rust_parser_parse_const_val(heap, mode, tokens); break;
    case RUST_PM_TYPE_VAL: out = rust_parser_parse_type_val(heap, mode, tokens); break;
  }
  return out;
}

/* finish(f, tokens): Done{f} when the head is EofK, else the Fail of err "an item or end of input". */
static RustSrcFileResult rust_parser_finish(Heap *heap, AstSrcFile f, RustPts tokens) {
  RustSrcFileResult out = {.ok = true, .file = f};
  const RustPt *h = rust_parser_head(tokens);
  if (h != NULL && h->k == RUST_PK_EOF_K) return out;
  out.ok = false;
  out.message = rust_parser_err(heap, str_lit("an item or end of input"), tokens);
  return out;
}

/* C only: the Fail of a SrcFile Result. */
static RustSrcFileResult rust_parser_file_fail(AstSrcFile none, Str message) {
  RustSrcFileResult out = {.ok = false, .file = none, .message = message};
  return out;
}

/* from_tokens(raw): classify and parse the lexer tokens of a whole source file. `raw` holds the
 * lexer's RustTok pointers; the classified list lives in the arena so the views outlive the call. */
RustSrcFileResult rust_parser_from_tokens(Heap *heap, const Vec *raw) {
  Vec none = vec_new(heap->arena);
  AstSrcFile empty = ast_src_file(none, none);
  Vec *all = heap_alloc(heap, sizeof *all);
  if (all == NULL) {
    heap->oom = true;
    return rust_parser_file_fail(empty, str_empty());
  }
  *all = rust_parser_classify(heap, raw);
  RustPts tokens = {.all = all, .at = 0};
  RustPMode im = {.kind = RUST_PM_INNER, .acc = none};
  RustPResult r = rust_parser_parse(heap, &im, tokens);
  RustPResult d = rust_parser_read_strs(heap, &r);
  if (!d.ok) return rust_parser_file_fail(empty, d.message);
  Vec acc = vec_new(heap->arena);
  RustPMode nm = {.kind = RUST_PM_ITEMS, .acc = acc};
  RustPResult r2 = rust_parser_parse(heap, &nm, d.value.rest);
  RustPResult n = rust_parser_read_nodes(heap, &r2);
  if (!n.ok) return rust_parser_file_fail(empty, n.message);
  return rust_parser_finish(heap, ast_src_file(d.value.list, n.value.list), n.value.rest);
}

/* file(source): lex, classify and parse a whole source file. */
RustSrcFileResult rust_parser_file(Heap *heap, Str source) {
  RustLexResult raw = rust_lexer_lex(heap, source);
  if (raw.ok) return rust_parser_from_tokens(heap, &raw.tokens);
  Vec none = vec_new(heap->arena);
  return rust_parser_file_fail(ast_src_file(none, none), raw.message);
}
