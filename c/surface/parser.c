/* Port of bend2/surface/parser.bend, chunk 1 = Bend lines 1-422: Parsed, the helpers value..starts_atom,
 * PValue, the read_* group, Mode, collection..family_separator and the extended_decl law.
 * Chunk 2 = parse (Bend 423-888), chunk 3 = Bend 888-1155 and the exports; the three chunks are
 * concatenated into c/surface/parser.c in the chunk 3 hop. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rt.h"
#include "kernel.h"
#include "surface.h"

/* List<&2, Token.T> is a cursor into the lexer's token Vec: the parser never builds a token list,
 * every `Token.Tok{..} <> tail` in the Bend re-conses the token it just took, which is the cursor
 * before the advance. Nil = pos past the end (the lexer puts Eof last). */
typedef struct {
  const Vec *all;
  size_t pos;
} Tokens;

/* One step of `match tokens`: some = Con{token, tail}, !some = Nil. */
typedef struct {
  bool some;
  const Token *token;
  Tokens tail;
} TokensHead;

static Tokens tokens_new(const Vec *all) {
  Tokens tokens;
  tokens.all = all;
  tokens.pos = 0;
  return tokens;
}

static TokensHead tokens_head(Tokens tokens) {
  TokensHead head;
  VecGet got = vec_get(tokens.all, tokens.pos);
  head.some = got.status == VEC_GET_OK;
  head.token = head.some ? (const Token *)got.value : NULL;
  head.tail = tokens;
  head.tail.pos = tokens.pos + 1;
  return head;
}

static bool head_is(TokensHead head, TokenKindTag tag) { return head.some && head.token->kind.tag == tag; }

/* Allocation failures: heap_alloc and the syntax constructors set heap->oom and return a placeholder;
 * the parser entry (chunk 3) turns heap->oom into Error.HostOom. */
static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static Str name_placeholder;

/* Boxes a String for a List<String> Vec (Str pointers, the surface.h convention). */
static Str *name_new(Heap *heap, Str name) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (!boxed) return &name_placeholder;
  *boxed = name;
  return boxed;
}

/* type Parsed<A>: Parsed{value, rest}, one struct per A used by the helpers; value and rest are field reads. */
typedef struct {
  Vec value; /* Str pointers */
  Tokens rest;
} ParsedNames;

typedef struct {
  Quantity value;
  Tokens rest;
} ParsedQuantity;

typedef struct {
  MaybeQuantity value;
  Tokens rest;
} ParsedMaybeQuantity;

/* Result<&2, &2, Error.T, X> for the X this file returns. */
typedef struct {
  ResultKind kind;
  Tokens value;
  Error error;
} TokensResult;

typedef struct {
  ResultKind kind;
  Big value;
  Error error;
} PBigResult;

typedef struct {
  ResultKind kind;
  const STerm *value;
  Error error;
} TermResult;

typedef struct {
  ResultKind kind;
  const Universe *value;
  Error error;
} LevelResult;

typedef struct {
  ResultKind kind;
  Error error;
} UnitResult;

typedef struct {
  ResultKind kind;
  Vec value;
  Error error;
} PVecResult;

static TokensResult done_tokens(Tokens value) {
  TokensResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static TokensResult fail_tokens(Error err) {
  TokensResult result;
  result.kind = RESULT_FAIL;
  result.value = tokens_new(NULL);
  result.error = err;
  return result;
}

static PBigResult done_big(Big value) {
  PBigResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static PBigResult fail_big(Error err) {
  PBigResult result;
  result.kind = RESULT_FAIL;
  result.value = big_zero();
  result.error = err;
  return result;
}

static TermResult done_term(const STerm *value) {
  TermResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static TermResult fail_term(Error err) {
  TermResult result;
  result.kind = RESULT_FAIL;
  result.value = sterm_unit();
  result.error = err;
  return result;
}

static LevelResult done_level(const Universe *value) {
  LevelResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static LevelResult fail_level(Error err) {
  LevelResult result;
  result.kind = RESULT_FAIL;
  result.value = NULL;
  result.error = err;
  return result;
}

static UnitResult done_unit(void) {
  UnitResult result;
  result.kind = RESULT_DONE;
  result.error = error_host_oom();
  return result;
}

static UnitResult fail_unit(Error err) {
  UnitResult result;
  result.kind = RESULT_FAIL;
  result.error = err;
  return result;
}

static PVecResult done_vec(Vec value) {
  PVecResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static PVecResult fail_vec(Heap *heap, Error err) {
  PVecResult result;
  result.kind = RESULT_FAIL;
  result.value = vec_new(heap->arena);
  result.error = err;
  return result;
}

/* error(loc, message) = Error.Parse{message, line, col} */
static Error error(TokenLoc loc, Str message) { return error_parse(message, loc.line, loc.col); }

/* expected(what, tokens): the Fail payload; the callers wrap it in their own Result type. */
static Error expected(Heap *heap, Str what, Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  if (head.some)
    return error(head.token->loc, SURFACE_CAT(heap, str_lit("expected "), what, str_lit(", found "),
                                              token_describe(heap, head.token->kind)));
  return error(token_start(heap), str_lit("unexpected end of input"));
}

/* expect(kind, tokens): Done{tail} when the head has the same tag, else expected(describe(kind), tokens). */
static TokensResult expect(Heap *heap, TokenKind kind, Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, kind.tag)) return done_tokens(head.tail);
  return fail_tokens(expected(heap, token_describe(heap, kind), tokens));
}

static PBigResult bounded_number_result(MaybeBig value, TokenLoc loc) {
  if (value.some) return done_big(value.value);
  return fail_big(error(loc, str_lit("invalid natural literal")));
}

/* digits are ASCII (lexer normalize_digits), so String.length is the byte count. */
static PBigResult bounded_number(Heap *heap, Str digits, TokenLoc loc) {
  if (digits.len <= 18) return bounded_number_result(big_of_decimal(heap, digits), loc);
  return fail_big(error(loc, str_lit("numeric literal too long")));
}

static TermResult natural(Heap *heap, MaybeBig parsed, TokenLoc loc) {
  if (parsed.some) return done_term(sterm_nat(heap, parsed.value));
  return fail_term(error(loc, str_lit("invalid natural literal")));
}

/* bytes_term(bytes): bytesCons b0 (bytesCons b1 (.. bytesNil)); bytes holds uint32_t pointers. */
static const STerm *bytes_term_from(Heap *heap, const Vec *bytes, size_t from) {
  VecGet got = vec_get(bytes, from);
  const STerm *byte;
  if (got.status != VEC_GET_OK) return sterm_var(heap, str_lit("bytesNil"));
  byte = sterm_nat(heap, big_of_u32(heap, *(const uint32_t *)got.value));
  return sterm_app(heap, sterm_app(heap, sterm_var(heap, str_lit("bytesCons")), byte),
                   bytes_term_from(heap, bytes, from + 1));
}

static const STerm *bytes_term(Heap *heap, const Vec *bytes) { return bytes_term_from(heap, bytes, 0); }

/* names(tokens, acc): the Bend conses and reverses; acc is pushed in source order here, so the
 * returned Vec is the Bend result when the caller passes an empty acc. */
static ParsedNames names(Heap *heap, Tokens tokens, Vec acc) {
  TokensHead head = tokens_head(tokens);
  ParsedNames parsed;
  if (head_is(head, TOKEN_IDENT)) {
    push(heap, &acc, name_new(heap, head.token->kind.name));
    return names(heap, head.tail, acc);
  }
  parsed.value = acc;
  parsed.rest = tokens;
  return parsed;
}

static ParsedQuantity parsed_quantity(Quantity value, Tokens rest) {
  ParsedQuantity parsed;
  parsed.value = value;
  parsed.rest = rest;
  return parsed;
}

/* mark(tokens): a leading 0 or 1 natural is a quantity mark; any other head stays in the input. */
static ParsedQuantity mark(Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  bool nat = head_is(head, TOKEN_NAT_TOK);
  if (nat && str_eq(head.token->kind.digits, str_lit("0"))) return parsed_quantity(QUANTITY_ZERO, head.tail);
  if (nat && str_eq(head.token->kind.digits, str_lit("1"))) return parsed_quantity(QUANTITY_ONE, head.tail);
  return parsed_quantity(QUANTITY_MANY, tokens);
}

static ParsedMaybeQuantity parsed_maybe_quantity(bool some, Quantity value, Tokens rest) {
  ParsedMaybeQuantity parsed;
  parsed.value.some = some;
  parsed.value.value = value;
  parsed.rest = rest;
  return parsed;
}

static ParsedMaybeQuantity proof_mark(Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  bool nat = head_is(head, TOKEN_NAT_TOK);
  if (nat && str_eq(head.token->kind.digits, str_lit("0")))
    return parsed_maybe_quantity(true, QUANTITY_ZERO, head.tail);
  if (nat && str_eq(head.token->kind.digits, str_lit("1")))
    return parsed_maybe_quantity(true, QUANTITY_ONE, head.tail);
  return parsed_maybe_quantity(false, QUANTITY_MANY, tokens);
}

/* make_binders(names, q, ty): one Binder{q, name, ty} per name (SBinder pointers). */
static Vec make_binders(Heap *heap, const Vec *names, Quantity q, const STerm *ty) {
  Vec binders = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < names->len; i++) {
    VecGet got = vec_get(names, i);
    SBinder binder;
    if (got.status != VEC_GET_OK) return binders;
    binder.quantity = q;
    binder.name = *(const Str *)got.value;
    binder.ty = ty;
    push(heap, &binders, (void *)sbinder_new(heap, binder));
  }
  return binders;
}

/* arrows(binders, body, star): right fold, SStar when star else SArrow. */
static const STerm *arrows_from(Heap *heap, const Vec *binders, size_t from, const STerm *body, bool star) {
  VecGet got = vec_get(binders, from);
  const STerm *inner;
  if (got.status != VEC_GET_OK) return body;
  inner = arrows_from(heap, binders, from + 1, body, star);
  if (star) return sterm_star(heap, *(const SBinder *)got.value, inner);
  return sterm_arrow(heap, *(const SBinder *)got.value, inner);
}

static const STerm *arrows(Heap *heap, const Vec *binders, const STerm *body, bool star) {
  return arrows_from(heap, binders, 0, body, star);
}

/* fields(binders): Field{q, name, Some{ty}} per binder (SField pointers). */
static Vec fields(Heap *heap, const Vec *binders) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < binders->len; i++) {
    VecGet got = vec_get(binders, i);
    const SBinder *binder;
    SField field;
    if (got.status != VEC_GET_OK) return out;
    binder = (const SBinder *)got.value;
    field.quantity = binder->quantity;
    field.name = binder->name;
    field.ty.some = true;
    field.ty.value = binder->ty;
    push(heap, &out, (void *)sfield_new(heap, field));
  }
  return out;
}

static TermResult zk(Heap *heap, const STerm *body, TokenLoc loc) {
  if (body->kind == STERM_STAR) return done_term(sterm_zk_ty(heap, body->binder, body->body));
  return fail_term(error(loc, str_lit("expected '(q w : W) * R' after 'zk'")));
}

/* index_of(names, name, index): the position of name counted from index. */
static MaybeNat index_of_from(Heap *heap, const Vec *names, size_t from, Str name, Nat index) {
  VecGet got = vec_get(names, from);
  MaybeNat out;
  out.some = false;
  out.value = nat_zero();
  if (got.status != VEC_GET_OK) return out;
  if (str_eq(*(const Str *)got.value, name)) {
    out.some = true;
    out.value = index;
    return out;
  }
  return index_of_from(heap, names, from + 1, name, heap_nat_succ(heap, index));
}

static MaybeNat index_of(Heap *heap, const Vec *names, Str name, Nat index) {
  return index_of_from(heap, names, 0, name, index);
}

static LevelResult universe_var(Heap *heap, MaybeNat index, Str name, TokenLoc loc) {
  if (index.some) return done_level(universe_var_level(heap, index.value));
  return fail_level(error(loc, SURFACE_CAT(heap, str_lit("unbound universe "), name)));
}

/* starts_atom(tokens): the 28 head kinds that open an atom. */
static bool starts_atom(Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  if (!head.some) return false;
  switch (head.token->kind.tag) {
    case TOKEN_IDENT:
    case TOKEN_NAT_TOK:
    case TOKEN_BYTES:
    case TOKEN_LPAREN:
    case TOKEN_UNIT_TOK:
    case TOKEN_KPROP:
    case TOKEN_KTYPE:
    case TOKEN_KSORT:
    case TOKEN_KAUTO:
    case TOKEN_KTUPLE:
    case TOKEN_KSUM:
    case TOKEN_KPROD:
    case TOKEN_KNAT_ADD:
    case TOKEN_KNAT_SUB:
    case TOKEN_KNAT_MUL:
    case TOKEN_KNAT_EQ:
    case TOKEN_KNAT_LT:
    case TOKEN_KNU:
    case TOKEN_KPROVE:
    case TOKEN_KVERIFY:
    case TOKEN_KFHC:
    case TOKEN_KENC:
    case TOKEN_KEVAL:
    case TOKEN_KDEC:
    case TOKEN_KMPC:
    case TOKEN_KINPUT:
    case TOKEN_KSHARE:
    case TOKEN_KOPEN:
      return true;
    case TOKEN_RPAREN:
    case TOKEN_COLON:
    case TOKEN_COLON_EQ:
    case TOKEN_ARROW:
    case TOKEN_DARROW:
    case TOKEN_STAR:
    case TOKEN_COMMA:
    case TOKEN_DOT:
    case TOKEN_DOT1:
    case TOKEN_DOT2:
    case TOKEN_PIPE:
    case TOKEN_KDEF:
    case TOKEN_KAXIOM:
    case TOKEN_KFUN:
    case TOKEN_KINJ:
    case TOKEN_KOF:
    case TOKEN_KCASE:
    case TOKEN_KMATCH:
    case TOKEN_KAS:
    case TOKEN_KRETURN:
    case TOKEN_KWITH:
    case TOKEN_KABSURD:
    case TOKEN_KPOLY:
    case TOKEN_KGROUP:
    case TOKEN_KSPECIALIZE:
    case TOKEN_KWHERE:
    case TOKEN_KLET:
    case TOKEN_KIN:
    case TOKEN_KMU:
    case TOKEN_KMUTUAL:
    case TOKEN_KEND:
    case TOKEN_KZK:
    case TOKEN_LBRACKET:
    case TOKEN_RBRACKET:
    case TOKEN_KAND:
    case TOKEN_KREC:
    case TOKEN_EOF:
      return false;
  }
  return false;
}

/* type PValue: the payload of the parse dispatcher; every arm carries its rest. */
typedef enum {
  PVALUE_TERM,
  PVALUE_BINDERS,
  PVALUE_FIELDS,
  PVALUE_BRANCHES,
  PVALUE_LEVEL,
  PVALUE_TERMS,
  PVALUE_DECL,
  PVALUE_DECLS,
  PVALUE_FAMILY,
  PVALUE_FAMILIES,
  PVALUE_CTORS,
  PVALUE_REC,
  PVALUE_RECS
} PValueKind;

typedef struct {
  PValueKind kind;
  const STerm *term;     /* TERM */
  Vec list;              /* BINDERS (SBinder*), FIELDS (SField*), BRANCHES (SBranch*), TERMS (STerm*),
                            DECLS (SDecl*), FAMILIES (SFam*), CTORS (SFamCtor*), RECS (SRecDef*) */
  const Universe *level; /* LEVEL */
  const SDecl *decl;     /* DECL */
  const SFam *family;    /* FAMILY */
  const SRecDef *rec;    /* REC */
  Tokens rest;
} PValue;

typedef struct {
  ResultKind kind;
  PValue value;
  Error error;
} PValueResult;

static PValue value_of(PValueKind kind, Tokens rest) {
  PValue value;
  value.kind = kind;
  value.term = NULL;
  value.list.arena = NULL;
  value.list.items = NULL;
  value.list.len = 0;
  value.list.cap = 0;
  value.level = NULL;
  value.decl = NULL;
  value.family = NULL;
  value.rec = NULL;
  value.rest = rest;
  return value;
}

static PValue term_value(const STerm *term, Tokens rest) {
  PValue value = value_of(PVALUE_TERM, rest);
  value.term = term;
  return value;
}

static PValue list_value(PValueKind kind, Vec list, Tokens rest) {
  PValue value = value_of(kind, rest);
  value.list = list;
  return value;
}

static PValue binders_value(Vec binders, Tokens rest) { return list_value(PVALUE_BINDERS, binders, rest); }
static PValue fields_value(Vec fields_list, Tokens rest) { return list_value(PVALUE_FIELDS, fields_list, rest); }
static PValue branches_value(Vec branches, Tokens rest) { return list_value(PVALUE_BRANCHES, branches, rest); }

static PValue level_value(const Universe *level, Tokens rest) {
  PValue value = value_of(PVALUE_LEVEL, rest);
  value.level = level;
  return value;
}

static PValue terms_value(Vec terms, Tokens rest) { return list_value(PVALUE_TERMS, terms, rest); }

static PValue decl_value(const SDecl *decl, Tokens rest) {
  PValue value = value_of(PVALUE_DECL, rest);
  value.decl = decl;
  return value;
}

static PValue decls_value(Vec decls, Tokens rest) { return list_value(PVALUE_DECLS, decls, rest); }

static PValue family_value(const SFam *family, Tokens rest) {
  PValue value = value_of(PVALUE_FAMILY, rest);
  value.family = family;
  return value;
}

static PValue families_value(Vec families, Tokens rest) { return list_value(PVALUE_FAMILIES, families, rest); }
static PValue ctors_value(Vec ctors, Tokens rest) { return list_value(PVALUE_CTORS, ctors, rest); }

static PValue rec_value(const SRecDef *rec, Tokens rest) {
  PValue value = value_of(PVALUE_REC, rest);
  value.rec = rec;
  return value;
}

static PValue recs_value(Vec recs, Tokens rest) { return list_value(PVALUE_RECS, recs, rest); }

static PValueResult done_value(PValue value) {
  PValueResult result;
  result.kind = RESULT_DONE;
  result.value = value;
  result.error = error_host_oom();
  return result;
}

static PValueResult fail_value(Error err) {
  PValueResult result;
  result.kind = RESULT_FAIL;
  result.value = value_of(PVALUE_TERM, tokens_new(NULL));
  result.error = err;
  return result;
}

/* read_X(result): Done{Parsed{x, rest}} when the Done value is an X, Fail passes through, any other
 * Done value is the internal mismatch error. The C result keeps the PValue: the caller reads the X
 * field and value.rest. */
static PValueResult read_value(Heap *heap, PValueResult result, PValueKind want, const char *label) {
  if (result.kind == RESULT_FAIL) return result;
  if (result.value.kind == want) return result;
  return fail_value(error_parse(SURFACE_CAT(heap, str_lit("internal parser result mismatch: "), str_lit(label)),
                                heap_nat_of_u64(heap, 1), heap_nat_of_u64(heap, 1)));
}

static PValueResult read_term(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_TERM, "Term"); }

static PValueResult read_binders(Heap *heap, PValueResult result) {
  return read_value(heap, result, PVALUE_BINDERS, "Binders");
}

static PValueResult read_fields(Heap *heap, PValueResult result) {
  return read_value(heap, result, PVALUE_FIELDS, "Fields");
}

static PValueResult read_branches(Heap *heap, PValueResult result) {
  return read_value(heap, result, PVALUE_BRANCHES, "Branches");
}

static PValueResult read_level(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_LEVEL, "Level"); }

static PValueResult read_terms(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_TERMS, "Terms"); }

static PValueResult read_decl(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_DECL, "Decl"); }

static PValueResult read_decls(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_DECLS, "Decls"); }

static PValueResult read_family(Heap *heap, PValueResult result) {
  return read_value(heap, result, PVALUE_FAMILY, "Family");
}

static PValueResult read_families(Heap *heap, PValueResult result) {
  return read_value(heap, result, PVALUE_FAMILIES, "Families");
}

static PValueResult read_ctors(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_CTORS, "Ctors"); }

static PValueResult read_rec(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_REC, "Rec"); }

static PValueResult read_recs(Heap *heap, PValueResult result) { return read_value(heap, result, PVALUE_RECS, "Recs"); }

/* type Mode: the parse dispatcher state, one tag per Bend constructor in declaration order. */
typedef enum {
  P_TERM,
  P_ARROW,
  P_ARROW_TRY,
  P_ARROW_PLAIN,
  P_ARROW_REST,
  P_APP,
  P_APP_REST,
  P_APP_NEXT,
  P_ATOM,
  P_HEAD,
  P_POSTFIX,
  P_RETURN,
  P_PAREN_REST,
  P_BINDER_GROUP,
  P_BINDER_NAMES,
  P_BINDERS,
  P_FUN,
  P_LET,
  P_CASE,
  P_CASE_MOTIVE,
  P_CASE_RETURN,
  P_BRANCHES,
  P_FIELDS,
  P_BARE_FIELD,
  P_LEVEL,
  P_LEVEL_PAREN,
  P_ITEMS,
  P_ITEMS_BODY,
  P_ITEMS_NEXT,
  P_ITEMS_REST,
  P_ATOMS,
  P_ATOMS_NEXT,
  P_ATOMS_DONE,
  P_DECL,
  P_DECLS,
  P_DEF,
  P_REC_GROUP,
  P_REC_REST,
  P_FAMILY,
  P_FAMILY_CTORS,
  P_FAMILY_GROUP,
  P_FAMILY_REST,
  P_MUTUAL
} ModeKind;

typedef struct {
  ModeKind kind;
  PValueResult attempt;      /* ARROW_TRY */
  const STerm *head;        /* ARROW_REST, APP_REST, APP_NEXT, POSTFIX, RETURN, PAREN_REST */
  Quantity q;               /* BINDER_NAMES, BARE_FIELD */
  Vec acc;                  /* BINDERS (SBinder*), BRANCHES (SBranch*), FIELDS and BARE_FIELD (SField*),
                               ITEMS_NEXT, ITEMS_REST, ATOMS, ATOMS_NEXT, ATOMS_DONE (STerm*), DECLS (SDecl*),
                               REC_GROUP, REC_REST (SRecDef*), FAMILY_CTORS (SFamCtor*),
                               FAMILY_GROUP, FAMILY_REST, MUTUAL (SFam*); pushed in source order */
  bool fibered;             /* CASE, CASE_MOTIVE, CASE_RETURN, BRANCHES */
  const STerm *scrut;       /* CASE_MOTIVE, CASE_RETURN */
  Str name;                 /* CASE_RETURN */
  MaybeStr ind;             /* CASE_RETURN */
  Vec indices;              /* CASE_RETURN (Str*) */
  Tokens original;          /* BARE_FIELD */
  uint64_t collection_kind; /* ITEMS, ITEMS_BODY, ITEMS_NEXT, ITEMS_REST: 0 tuple, 1 sum, other prod */
  bool rec;                 /* DEF */
} Mode;

static Mode mode_of(ModeKind kind) {
  Mode mode;
  mode.kind = kind;
  mode.attempt = fail_value(error_host_oom());
  mode.head = NULL;
  mode.q = QUANTITY_MANY;
  mode.acc = mode.attempt.value.list;
  mode.fibered = false;
  mode.scrut = NULL;
  mode.name = str_lit("");
  mode.ind.some = false;
  mode.ind.value = str_lit("");
  mode.indices = mode.attempt.value.list;
  mode.original = tokens_new(NULL);
  mode.collection_kind = 0;
  mode.rec = false;
  return mode;
}

/* collection(kind, items): 0 tuple, 1 sum, other prod. */
static const STerm *collection(Heap *heap, uint64_t kind, Vec items) {
  if (kind == 0) return sterm_tuple(heap, items);
  if (kind == 1) return sterm_sum(heap, items);
  return sterm_prod(heap, items);
}

static const STerm *elimination(Heap *heap, bool fibered, const STerm *scrut, MaybeSMotive motive, Vec branches) {
  if (fibered) return sterm_match(heap, scrut, motive, branches);
  return sterm_case(heap, scrut, motive, branches);
}

static UnitResult numeric_branch(bool fibered, TokenLoc loc) {
  if (fibered) return fail_unit(error(loc, str_lit("a match branch keys a constructor, not a leg number")));
  return done_unit();
}

/* mutual_count(families, loc): at least two; the Bend reverses its consed acc, the C acc is already in
 * source order, so the Vec passes through. */
static PVecResult mutual_count(Heap *heap, Vec families, TokenLoc loc) {
  if (families.len >= 2) return done_vec(families);
  return fail_vec(heap, error(loc, str_lit("a mutual group needs at least two mu declarations")));
}

/* make_def(name, binders, ty, body): DDef{name, ty, body}, or the arrow type and SFun when binders exist. */
static const SDecl *make_def(Heap *heap, Str name, Vec binders, const STerm *ty, const STerm *body) {
  if (binders.len == 0) return sdecl_def_new(heap, name, ty, body);
  return sdecl_def_new(heap, name, arrows(heap, &binders, ty, false), sterm_fun(heap, binders, body));
}

static TokensResult family_separator(Heap *heap, Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, TOKEN_KWITH)) return done_tokens(head.tail);
  if (head_is(head, TOKEN_COLON_EQ)) return done_tokens(head.tail);
  return fail_tokens(expected(heap, str_lit("':=' or 'with'"), tokens));
}

/* law extended_decl: for tokens, for scope (List<String> as Str pointers): Result<Error, PValue>; defined in chunk 3. */
static PValueResult extended_decl(Heap *heap, Tokens tokens, Vec scope);

/* ---- parse (parser.bend 423-886): the mode dispatcher ----
   Bend arity: parse(mode, scope, tokens). One static function per mode arm, in the Bend arm
   order, and the dispatcher switch at the end. Token lists are cursors (Decision 66); every
   Bend `x <> acc` is a push in source order and the `List.reverse(acc)` at the use site is
   the identity (Decision 65). */

static PValueResult parse(Heap *heap, Mode mode, Vec scope, Tokens tokens);

static Vec empty_vec(Heap *heap) { return vec_new(heap->arena); }

static void push_all_from(Heap *heap, Vec *dst, const Vec *src, size_t from) {
  VecGet got = vec_get(src, from);
  if (got.status != VEC_GET_OK) return;
  push(heap, dst, got.value);
  push_all_from(heap, dst, src, from + 1);
}

static bool failed(PValueResult result) { return result.kind == RESULT_FAIL; }

static PValueResult term_done(const STerm *term, Tokens rest) { return done_value(term_value(term, rest)); }

static PValueResult fail_expected(Heap *heap, const char *what, Tokens tokens) {
  return fail_value(expected(heap, str_lit(what), tokens));
}

static PValueResult fail_at(TokenLoc loc, const char *message) { return fail_value(error(loc, str_lit(message))); }

static bool head_is_word(TokensHead head, const char *word) {
  return head_is(head, TOKEN_IDENT) && str_eq(head.token->kind.name, str_lit(word));
}

/* read_term(parse(P<kind>{}, scope, tokens)) */
static PValueResult sub_term(Heap *heap, ModeKind kind, Vec scope, Tokens tokens) {
  return read_term(heap, parse(heap, mode_of(kind), scope, tokens));
}

/* read_level(parse(PLevel{}, scope, tokens)) */
static PValueResult sub_level(Heap *heap, Vec scope, Tokens tokens) {
  return read_level(heap, parse(heap, mode_of(P_LEVEL), scope, tokens));
}

static Mode mode_head(ModeKind kind, const STerm *head) {
  Mode m = mode_of(kind);
  m.head = head;
  return m;
}

static Mode mode_acc(ModeKind kind, Vec acc) {
  Mode m = mode_of(kind);
  m.acc = acc;
  return m;
}

static Mode mode_items(ModeKind kind, uint64_t collection_kind) {
  Mode m = mode_of(kind);
  m.collection_kind = collection_kind;
  return m;
}

static Mode mode_items_acc(ModeKind kind, uint64_t collection_kind, Vec acc) {
  Mode m = mode_items(kind, collection_kind);
  m.acc = acc;
  return m;
}

static Mode mode_branches(bool fibered, Vec acc) {
  Mode m = mode_acc(P_BRANCHES, acc);
  m.fibered = fibered;
  return m;
}

static Mode mode_case(ModeKind kind, bool fibered, const STerm *scrut) {
  Mode m = mode_of(kind);
  m.fibered = fibered;
  m.scrut = scrut;
  return m;
}

static Mode mode_def(bool rec) {
  Mode m = mode_of(P_DEF);
  m.rec = rec;
  return m;
}

static MaybeSMotive no_motive(Heap *heap) {
  MaybeStr ind;
  MaybeSMotive motive;
  ind.some = false;
  ind.value = str_lit("");
  motive.some = false;
  motive.value = smotive(str_lit(""), ind, empty_vec(heap), NULL);
  return motive;
}

typedef const STerm *(*BinaryMaker)(Heap *, const STerm *, const STerm *);
typedef const Universe *(*LevelMaker)(Heap *, const Universe *, const Universe *);

/* PTerm */
static PValueResult parse_term(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_KFUN)) return parse(heap, mode_of(P_FUN), scope, h.tail);
  if (head_is(h, TOKEN_KLET)) return parse(heap, mode_of(P_LET), scope, h.tail);
  if (head_is(h, TOKEN_KCASE)) return parse(heap, mode_case(P_CASE, false, NULL), scope, h.tail);
  if (head_is(h, TOKEN_KMATCH)) return parse(heap, mode_case(P_CASE, true, NULL), scope, h.tail);
  if (head_is(h, TOKEN_KZK)) {
    PValueResult a = sub_term(heap, P_ARROW, scope, h.tail);
    if (failed(a)) return a;
    TermResult x = zk(heap, a.value.term, h.token->loc);
    if (x.kind == RESULT_FAIL) return fail_value(x.error);
    return term_done(x.value, a.value.rest);
  }
  return parse(heap, mode_of(P_ARROW), scope, tokens);
}

/* PArrow */
static PValueResult parse_arrow(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_LPAREN)) return parse(heap, mode_of(P_ARROW_PLAIN), scope, tokens);
  Mode m = mode_of(P_ARROW_TRY);
  m.attempt = parse(heap, mode_of(P_BINDER_GROUP), scope, tokens);
  return parse(heap, m, scope, tokens);
}

/* PArrowTry: the Done{BindersValue{bs, Arrow|Star ...}} arms */
static PValueResult arrow_try_body(Heap *heap, Vec scope, Vec binders, Tokens tail, bool star) {
  PValueResult a = sub_term(heap, P_TERM, scope, tail);
  if (failed(a)) return a;
  return term_done(arrows(heap, &binders, a.value.term, star), a.value.rest);
}

static PValueResult parse_arrow_try(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult attempt = mode.attempt;
  bool binders = attempt.kind == RESULT_DONE && attempt.value.kind == PVALUE_BINDERS;
  if (!binders) return parse(heap, mode_of(P_ARROW_PLAIN), scope, tokens);
  TokensHead h = tokens_head(attempt.value.rest);
  if (head_is(h, TOKEN_ARROW)) return arrow_try_body(heap, scope, attempt.value.list, h.tail, false);
  if (head_is(h, TOKEN_STAR)) return arrow_try_body(heap, scope, attempt.value.list, h.tail, true);
  return parse(heap, mode_of(P_ARROW_PLAIN), scope, tokens);
}

/* PArrowPlain */
static PValueResult parse_arrow_plain(Heap *heap, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_APP, scope, tokens);
  if (failed(a)) return a;
  return parse(heap, mode_head(P_ARROW_REST, a.value.term), scope, a.value.rest);
}

/* PArrowRest */
static PValueResult parse_arrow_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  bool arrow = head_is(h, TOKEN_ARROW);
  bool star = head_is(h, TOKEN_STAR);
  if (!arrow && !star) return term_done(mode.head, tokens);
  PValueResult a = sub_term(heap, P_TERM, scope, h.tail);
  if (failed(a)) return a;
  SBinder binder = sbinder(QUANTITY_MANY, str_lit("_"), mode.head);
  const STerm *term = arrow ? sterm_arrow(heap, binder, a.value.term) : sterm_star(heap, binder, a.value.term);
  return term_done(term, a.value.rest);
}

/* PReturn */
static PValueResult parse_return(Mode mode, Tokens tokens) { return term_done(mode.head, tokens); }

/* PApp */
static PValueResult parse_app(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  if (head_is(h1, TOKEN_KABSURD)) {
    PValueResult a = sub_term(heap, P_APP, scope, h1.tail);
    if (failed(a)) return a;
    return term_done(sterm_absurd(heap, a.value.term), a.value.rest);
  }
  if (head_is(h1, TOKEN_KINJ)) {
    TokensHead h2 = tokens_head(h1.tail);
    TokensHead h3 = tokens_head(h2.tail);
    TokensHead h4 = tokens_head(h3.tail);
    bool shape = head_is(h2, TOKEN_NAT_TOK) && head_is(h3, TOKEN_KOF) && head_is(h4, TOKEN_NAT_TOK);
    if (!shape) return fail_expected(heap, "'K of N' after 'inj'", h1.tail);
    PBigResult k = bounded_number(heap, h2.token->kind.digits, h2.token->loc);
    if (k.kind == RESULT_FAIL) return fail_value(k.error);
    PBigResult n = bounded_number(heap, h4.token->kind.digits, h4.token->loc);
    if (n.kind == RESULT_FAIL) return fail_value(n.error);
    PValueResult a = sub_term(heap, P_APP, scope, h4.tail);
    if (failed(a)) return a;
    return term_done(sterm_inj(heap, k.value, n.value, a.value.term), a.value.rest);
  }
  PValueResult a = sub_term(heap, P_ATOM, scope, tokens);
  if (failed(a)) return a;
  return parse(heap, mode_head(P_APP_REST, a.value.term), scope, a.value.rest);
}

/* PAppRest */
static PValueResult parse_app_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  ModeKind next = starts_atom(tokens) ? P_APP_NEXT : P_RETURN;
  return parse(heap, mode_head(next, mode.head), scope, tokens);
}

/* PAppNext */
static PValueResult parse_app_next(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tokens);
  if (failed(a)) return a;
  return parse(heap, mode_head(P_APP_REST, sterm_app(heap, mode.head, a.value.term)), scope, a.value.rest);
}

/* PAtom */
static PValueResult parse_atom(Heap *heap, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_HEAD, scope, tokens);
  if (failed(a)) return a;
  return parse(heap, mode_head(P_POSTFIX, a.value.term), scope, a.value.rest);
}

/* PPostfix */
static PValueResult parse_postfix(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  if (head_is(h1, TOKEN_DOT1)) {
    return parse(heap, mode_head(P_POSTFIX, sterm_proj(heap, mode.head, big_of_u32(heap, 1))), scope, h1.tail);
  }
  if (head_is(h1, TOKEN_DOT2)) {
    return parse(heap, mode_head(P_POSTFIX, sterm_proj(heap, mode.head, big_of_u32(heap, 2))), scope, h1.tail);
  }
  if (head_is(h1, TOKEN_DOT)) {
    TokensHead h2 = tokens_head(h1.tail);
    if (!head_is(h2, TOKEN_NAT_TOK)) return fail_at(h1.token->loc, "expected a leg number after '.'");
    PBigResult k = bounded_number(heap, h2.token->kind.digits, h2.token->loc);
    if (k.kind == RESULT_FAIL) return fail_value(k.error);
    return parse(heap, mode_head(P_POSTFIX, sterm_proj(heap, mode.head, k.value)), scope, h2.tail);
  }
  return term_done(mode.head, tokens);
}

/* PHead helpers, in the Bend arm order */
static PValueResult head_natural(Heap *heap, TokensHead h1) {
  TermResult n = natural(heap, big_of_decimal(heap, h1.token->kind.digits), h1.token->loc);
  if (n.kind == RESULT_FAIL) return fail_value(n.error);
  return term_done(n.value, h1.tail);
}

static PValueResult head_sort(Heap *heap, Vec scope, Tokens tail) {
  PValueResult a = sub_level(heap, scope, tail);
  if (failed(a)) return a;
  return term_done(sterm_sort(heap, a.value.level), a.value.rest);
}

static PValueResult head_type(Heap *heap, TokensHead h1) {
  TokensHead h2 = tokens_head(h1.tail);
  if (!head_is(h2, TOKEN_NAT_TOK)) return term_done(sterm_type(heap, big_zero()), h1.tail);
  PBigResult n = bounded_number(heap, h2.token->kind.digits, h2.token->loc);
  if (n.kind == RESULT_FAIL) return fail_value(n.error);
  return term_done(sterm_type(heap, n.value), h2.tail);
}

static PValueResult head_paren(Heap *heap, Vec scope, Tokens tail) {
  PValueResult a = sub_term(heap, P_TERM, scope, tail);
  if (failed(a)) return a;
  return parse(heap, mode_head(P_PAREN_REST, a.value.term), scope, a.value.rest);
}

static PValueResult head_prove(Heap *heap, Vec scope, Tokens tail) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tail);
  if (failed(a)) return a;
  PValueResult b = sub_term(heap, P_ATOM, scope, a.value.rest);
  if (failed(b)) return b;
  PValueResult c = sub_term(heap, P_ATOM, scope, b.value.rest);
  if (failed(c)) return c;
  return term_done(sterm_prove(heap, a.value.term, b.value.term, c.value.term), c.value.rest);
}

/* verify, fhc, enc, eval, dec, input: two atoms */
static PValueResult binary_atoms(Heap *heap, Vec scope, Tokens tail, BinaryMaker make) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tail);
  if (failed(a)) return a;
  PValueResult b = sub_term(heap, P_ATOM, scope, a.value.rest);
  if (failed(b)) return b;
  return term_done(make(heap, a.value.term, b.value.term), b.value.rest);
}

static PValueResult head_share(Heap *heap, Vec scope, Tokens tail) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tail);
  if (failed(a)) return a;
  return term_done(sterm_share(heap, a.value.term), a.value.rest);
}

static PValueResult head_mpc(Heap *heap, Vec scope, TokensHead h1) {
  TokensHead h2 = tokens_head(h1.tail);
  if (head_is(h2, TOKEN_LBRACKET)) {
    PValueResult a = sub_term(heap, P_TERM, scope, h2.tail);
    if (failed(a)) return a;
    TokensResult t1 = expect(heap, token_kind(TOKEN_COMMA), a.value.rest);
    if (t1.kind == RESULT_FAIL) return fail_value(t1.error);
    PValueResult b = sub_term(heap, P_TERM, scope, t1.value);
    if (failed(b)) return b;
    TokensResult t2 = expect(heap, token_kind(TOKEN_RBRACKET), b.value.rest);
    if (t2.kind == RESULT_FAIL) return fail_value(t2.error);
    PValueResult c = sub_term(heap, P_ATOM, scope, t2.value);
    if (failed(c)) return c;
    return term_done(sterm_mpc_ty(heap, a.value.term, b.value.term, c.value.term), c.value.rest);
  }
  PValueResult a = sub_term(heap, P_ATOM, scope, h1.tail);
  if (failed(a)) return a;
  PValueResult b = sub_term(heap, P_ATOM, scope, a.value.rest);
  if (failed(b)) return b;
  PValueResult c = sub_term(heap, P_ATOM, scope, b.value.rest);
  if (failed(c)) return c;
  PValueResult cs = read_terms(heap, parse(heap, mode_acc(P_ATOMS, empty_vec(heap)), scope, c.value.rest));
  if (failed(cs)) return cs;
  Vec values = empty_vec(heap);
  push(heap, &values, (void *)c.value.term);
  push_all_from(heap, &values, &cs.value.list, 0);
  return term_done(sterm_join(heap, a.value.term, b.value.term, values), cs.value.rest);
}

static PValueResult head_open(Heap *heap, Vec scope, Tokens tail) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tail);
  if (failed(a)) return a;
  ParsedMaybeQuantity q = proof_mark(a.value.rest);
  PValueResult b = sub_term(heap, P_ATOM, scope, q.rest);
  if (failed(b)) return b;
  PValueResult c = sub_term(heap, P_ATOM, scope, b.value.rest);
  if (failed(c)) return c;
  return term_done(sterm_open(heap, q.value, a.value.term, b.value.term, c.value.term), c.value.rest);
}

/* PHead */
static PValueResult parse_head(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  Tokens tail = h.tail;
  if (head_is(h, TOKEN_IDENT)) return term_done(sterm_var(heap, h.token->kind.name), tail);
  if (head_is(h, TOKEN_BYTES)) return term_done(bytes_term(heap, &h.token->kind.bytes), tail);
  if (head_is(h, TOKEN_KPROP)) return term_done(sterm_prop(), tail);
  if (head_is(h, TOKEN_KAUTO)) return term_done(sterm_auto(), tail);
  if (head_is(h, TOKEN_UNIT_TOK)) return term_done(sterm_unit(), tail);
  if (head_is(h, TOKEN_NAT_TOK)) return head_natural(heap, h);
  if (head_is(h, TOKEN_KSORT)) return head_sort(heap, scope, tail);
  if (head_is(h, TOKEN_KTYPE)) return head_type(heap, h);
  if (head_is(h, TOKEN_KNAT_ADD)) return term_done(sterm_prim(heap, SPRIM_ADD), tail);
  if (head_is(h, TOKEN_KNAT_SUB)) return term_done(sterm_prim(heap, SPRIM_SUB), tail);
  if (head_is(h, TOKEN_KNAT_MUL)) return term_done(sterm_prim(heap, SPRIM_MUL), tail);
  if (head_is(h, TOKEN_KNAT_EQ)) return term_done(sterm_prim(heap, SPRIM_EQ), tail);
  if (head_is(h, TOKEN_KNAT_LT)) return term_done(sterm_prim(heap, SPRIM_LT), tail);
  if (head_is(h, TOKEN_KTUPLE)) return parse(heap, mode_items(P_ITEMS, 0), scope, tail);
  if (head_is(h, TOKEN_KSUM)) return parse(heap, mode_items(P_ITEMS, 1), scope, tail);
  if (head_is(h, TOKEN_KPROD)) return parse(heap, mode_items(P_ITEMS, 2), scope, tail);
  if (head_is(h, TOKEN_KMU)) return fail_at(h.token->loc, "a mu group is a declaration and not a term");
  if (head_is(h, TOKEN_KAND)) return fail_at(h.token->loc, "'and' joins two members of a mu group and is not a term");
  if (head_is(h, TOKEN_KNU)) return fail_at(h.token->loc, "nu arrives at M2");
  if (head_is(h, TOKEN_KZK)) return fail_at(h.token->loc, "a zk type is not an atom");
  if (head_is(h, TOKEN_LPAREN)) return head_paren(heap, scope, tail);
  if (head_is(h, TOKEN_KPROVE)) return head_prove(heap, scope, tail);
  if (head_is(h, TOKEN_KVERIFY)) return binary_atoms(heap, scope, tail, sterm_verify);
  if (head_is(h, TOKEN_KFHC)) return binary_atoms(heap, scope, tail, sterm_fhc_ty);
  if (head_is(h, TOKEN_KENC)) return binary_atoms(heap, scope, tail, sterm_enc);
  if (head_is(h, TOKEN_KEVAL)) return binary_atoms(heap, scope, tail, sterm_eval);
  if (head_is(h, TOKEN_KDEC)) return binary_atoms(heap, scope, tail, sterm_dec);
  if (head_is(h, TOKEN_KSHARE)) return head_share(heap, scope, tail);
  if (head_is(h, TOKEN_KINPUT)) return binary_atoms(heap, scope, tail, sterm_input);
  if (head_is(h, TOKEN_KMPC)) return head_mpc(heap, scope, h);
  if (head_is(h, TOKEN_KOPEN)) return head_open(heap, scope, tail);
  return fail_expected(heap, "a term", tokens);
}

/* PParenRest */
static PValueResult parse_paren_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_RPAREN)) return term_done(mode.head, h.tail);
  bool comma = head_is(h, TOKEN_COMMA);
  bool colon = head_is(h, TOKEN_COLON);
  if (!comma && !colon) return fail_expected(heap, "')', ',' or ':'", tokens);
  PValueResult a = sub_term(heap, P_TERM, scope, h.tail);
  if (failed(a)) return a;
  TokensResult t = expect(heap, token_kind(TOKEN_RPAREN), a.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  const STerm *term = comma ? sterm_pair(heap, mode.head, a.value.term) : sterm_ann(heap, mode.head, a.value.term);
  return term_done(term, t.value);
}

/* PBinderGroup */
static PValueResult parse_binder_group(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_LPAREN)) return fail_expected(heap, "'('", tokens);
  ParsedQuantity q = mark(h.tail);
  Mode m = mode_of(P_BINDER_NAMES);
  m.q = q.value;
  return parse(heap, m, scope, q.rest);
}

/* PBinderNames */
static PValueResult parse_binder_names(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_IDENT)) return fail_expected(heap, "a binder name and ':'", tokens);
  Vec first = empty_vec(heap);
  push(heap, &first, name_new(heap, h.token->kind.name));
  ParsedNames ns = names(heap, h.tail, first);
  TokensResult t1 = expect(heap, token_kind(TOKEN_COLON), ns.rest);
  if (t1.kind == RESULT_FAIL) return fail_value(t1.error);
  PValueResult a = sub_term(heap, P_TERM, scope, t1.value);
  if (failed(a)) return a;
  TokensResult t2 = expect(heap, token_kind(TOKEN_RPAREN), a.value.rest);
  if (t2.kind == RESULT_FAIL) return fail_value(t2.error);
  return done_value(binders_value(make_binders(heap, &ns.value, mode.q, a.value.term), t2.value));
}

/* PBinders: List.append(List.reverse(bs), acc) then List.reverse at the end = push each binder */
static PValueResult parse_binders(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_LPAREN)) return done_value(binders_value(mode.acc, tokens));
  PValueResult a = read_binders(heap, parse(heap, mode_of(P_BINDER_GROUP), scope, tokens));
  if (failed(a)) return a;
  Vec acc = mode.acc;
  push_all_from(heap, &acc, &a.value.list, 0);
  return parse(heap, mode_acc(P_BINDERS, acc), scope, a.value.rest);
}

/* PFun: PBinders{List.reverse(a)} starts from the first group in source order */
static PValueResult parse_fun(Heap *heap, Vec scope, Tokens tokens) {
  PValueResult a = read_binders(heap, parse(heap, mode_of(P_BINDER_GROUP), scope, tokens));
  if (failed(a)) return a;
  PValueResult b = read_binders(heap, parse(heap, mode_acc(P_BINDERS, a.value.list), scope, a.value.rest));
  if (failed(b)) return b;
  TokensResult t = expect(heap, token_kind(TOKEN_DARROW), b.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult c = sub_term(heap, P_TERM, scope, t.value);
  if (failed(c)) return c;
  return term_done(sterm_fun(heap, b.value.list, c.value.term), c.value.rest);
}

/* PLet */
static PValueResult parse_let(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  bool shape = head_is(h1, TOKEN_IDENT) && head_is(h2, TOKEN_COLON);
  if (!shape) return fail_expected(heap, "a name and ':' after 'let'", tokens);
  PValueResult a = sub_term(heap, P_TERM, scope, h2.tail);
  if (failed(a)) return a;
  TokensResult t1 = expect(heap, token_kind(TOKEN_COLON_EQ), a.value.rest);
  if (t1.kind == RESULT_FAIL) return fail_value(t1.error);
  PValueResult b = sub_term(heap, P_TERM, scope, t1.value);
  if (failed(b)) return b;
  TokensResult t2 = expect(heap, token_kind(TOKEN_KIN), b.value.rest);
  if (t2.kind == RESULT_FAIL) return fail_value(t2.error);
  PValueResult c = sub_term(heap, P_TERM, scope, t2.value);
  if (failed(c)) return c;
  const STerm *term = sterm_let(heap, h1.token->kind.name, a.value.term, b.value.term, c.value.term);
  return term_done(term, c.value.rest);
}

/* PItems */
static PValueResult parse_items(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_UNIT_TOK)) return term_done(collection(heap, mode.collection_kind, empty_vec(heap)), h.tail);
  if (head_is(h, TOKEN_LPAREN)) return parse(heap, mode_items(P_ITEMS_BODY, mode.collection_kind), scope, h.tail);
  return fail_expected(heap, "'(' after collection keyword", tokens);
}

/* PItemsBody */
static PValueResult parse_items_body(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_RPAREN)) return term_done(collection(heap, mode.collection_kind, empty_vec(heap)), h.tail);
  return parse(heap, mode_items_acc(P_ITEMS_NEXT, mode.collection_kind, empty_vec(heap)), scope, tokens);
}

/* PItemsNext */
static PValueResult parse_items_next(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_TERM, scope, tokens);
  if (failed(a)) return a;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)a.value.term);
  return parse(heap, mode_items_acc(P_ITEMS_REST, mode.collection_kind, acc), scope, a.value.rest);
}

/* PItemsRest */
static PValueResult parse_items_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_COMMA)) {
    return parse(heap, mode_items_acc(P_ITEMS_NEXT, mode.collection_kind, mode.acc), scope, h.tail);
  }
  if (head_is(h, TOKEN_RPAREN)) return term_done(collection(heap, mode.collection_kind, mode.acc), h.tail);
  return fail_expected(heap, "',' or ')'", tokens);
}

/* PAtoms */
static PValueResult parse_atoms(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  ModeKind next = starts_atom(tokens) ? P_ATOMS_NEXT : P_ATOMS_DONE;
  return parse(heap, mode_acc(next, mode.acc), scope, tokens);
}

/* PAtomsNext */
static PValueResult parse_atoms_next(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_ATOM, scope, tokens);
  if (failed(a)) return a;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)a.value.term);
  return parse(heap, mode_acc(P_ATOMS, acc), scope, a.value.rest);
}

/* PAtomsDone */
static PValueResult parse_atoms_done(Mode mode, Tokens tokens) { return done_value(terms_value(mode.acc, tokens)); }

/* PLevel */
static PValueResult parse_level(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_NAT_TOK)) {
    PBigResult n = bounded_number(heap, h.token->kind.digits, h.token->loc);
    if (n.kind == RESULT_FAIL) return fail_value(n.error);
    return done_value(level_value(universe_nat_level(heap, n.value), h.tail));
  }
  if (head_is(h, TOKEN_IDENT)) {
    Str name = h.token->kind.name;
    LevelResult u = universe_var(heap, index_of(heap, &scope, name, nat_zero()), name, h.token->loc);
    if (u.kind == RESULT_FAIL) return fail_value(u.error);
    return done_value(level_value(u.value, h.tail));
  }
  if (head_is(h, TOKEN_LPAREN)) return parse(heap, mode_of(P_LEVEL_PAREN), scope, h.tail);
  return fail_expected(heap, "a universe level", tokens);
}

/* PLevelParen */
static PValueResult level_close(Heap *heap, const Universe *level, Tokens rest) {
  TokensResult t = expect(heap, token_kind(TOKEN_RPAREN), rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  return done_value(level_value(level, t.value));
}

static PValueResult level_binary(Heap *heap, Vec scope, Tokens tail, LevelMaker make) {
  PValueResult a = sub_level(heap, scope, tail);
  if (failed(a)) return a;
  PValueResult b = sub_level(heap, scope, a.value.rest);
  if (failed(b)) return b;
  return level_close(heap, make(heap, a.value.level, b.value.level), b.value.rest);
}

static PValueResult parse_level_paren(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is_word(h, "succ")) {
    PValueResult a = sub_level(heap, scope, h.tail);
    if (failed(a)) return a;
    return level_close(heap, universe_succ_level(heap, a.value.level), a.value.rest);
  }
  if (head_is_word(h, "max")) return level_binary(heap, scope, h.tail, universe_max_level);
  if (head_is_word(h, "imax")) return level_binary(heap, scope, h.tail, universe_imax_level);
  PValueResult a = sub_level(heap, scope, tokens);
  if (failed(a)) return a;
  return level_close(heap, a.value.level, a.value.rest);
}

/* PCase */
static PValueResult parse_case(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult a = sub_term(heap, P_TERM, scope, tokens);
  if (failed(a)) return a;
  return parse(heap, mode_case(P_CASE_MOTIVE, mode.fibered, a.value.term), scope, a.value.rest);
}

/* PCaseMotive */
static PValueResult parse_case_motive(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  if (head_is(h1, TOKEN_KWITH)) {
    PValueResult bs = read_branches(heap, parse(heap, mode_branches(mode.fibered, empty_vec(heap)), scope, h1.tail));
    if (failed(bs)) return bs;
    const STerm *term = elimination(heap, mode.fibered, mode.scrut, no_motive(heap), bs.value.list);
    return term_done(term, bs.value.rest);
  }
  if (!head_is(h1, TOKEN_KAS)) return fail_expected(heap, "'as' or 'with'", tokens);
  TokensHead h2 = tokens_head(h1.tail);
  if (!head_is(h2, TOKEN_IDENT)) return fail_at(h1.token->loc, "expected 'NAME [in FAMILY IDX..] return TYPE' after 'as'");
  Mode m = mode_case(P_CASE_RETURN, mode.fibered, mode.scrut);
  m.name = h2.token->kind.name;
  TokensHead h3 = tokens_head(h2.tail);
  TokensHead h4 = tokens_head(h3.tail);
  if (head_is(h3, TOKEN_KIN) && head_is(h4, TOKEN_IDENT)) {
    ParsedNames ns = names(heap, h4.tail, empty_vec(heap));
    m.ind.some = true;
    m.ind.value = h4.token->kind.name;
    m.indices = ns.value;
    return parse(heap, m, scope, ns.rest);
  }
  m.indices = empty_vec(heap);
  return parse(heap, m, scope, h2.tail);
}

/* PCaseReturn */
static PValueResult parse_case_return(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_KRETURN)) return fail_expected(heap, "'return'", tokens);
  PValueResult body = sub_term(heap, P_TERM, scope, h.tail);
  if (failed(body)) return body;
  TokensResult t = expect(heap, token_kind(TOKEN_KWITH), body.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult bs = read_branches(heap, parse(heap, mode_branches(mode.fibered, empty_vec(heap)), scope, t.value));
  if (failed(bs)) return bs;
  MaybeSMotive motive;
  motive.some = true;
  motive.value = smotive(mode.name, mode.ind, mode.indices, body.value.term);
  return term_done(elimination(heap, mode.fibered, mode.scrut, motive, bs.value.list), bs.value.rest);
}

/* PBranches */
static PValueResult branch_leg(Heap *heap, Mode mode, Vec scope, TokensHead h2) {
  UnitResult checked = numeric_branch(mode.fibered, h2.token->loc);
  if (checked.kind == RESULT_FAIL) return fail_value(checked.error);
  PBigResult k = bounded_number(heap, h2.token->kind.digits, h2.token->loc);
  if (k.kind == RESULT_FAIL) return fail_value(k.error);
  PValueResult bs = read_binders(heap, parse(heap, mode_acc(P_BINDERS, empty_vec(heap)), scope, h2.tail));
  if (failed(bs)) return bs;
  TokensResult t = expect(heap, token_kind(TOKEN_DARROW), bs.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult body = sub_term(heap, P_TERM, scope, t.value);
  if (failed(body)) return body;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)sbranch_leg_new(heap, k.value, bs.value.list, body.value.term));
  return parse(heap, mode_branches(mode.fibered, acc), scope, body.value.rest);
}

static PValueResult branch_ctor(Heap *heap, Mode mode, Vec scope, TokensHead h2) {
  PValueResult fs = read_fields(heap, parse(heap, mode_acc(P_FIELDS, empty_vec(heap)), scope, h2.tail));
  if (failed(fs)) return fs;
  TokensResult t = expect(heap, token_kind(TOKEN_DARROW), fs.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult body = sub_term(heap, P_TERM, scope, t.value);
  if (failed(body)) return body;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)sbranch_ctor_new(heap, h2.token->kind.name, fs.value.list, body.value.term));
  return parse(heap, mode_branches(mode.fibered, acc), scope, body.value.rest);
}

static PValueResult parse_branches(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  if (!head_is(h1, TOKEN_PIPE)) return done_value(branches_value(mode.acc, tokens));
  TokensHead h2 = tokens_head(h1.tail);
  if (head_is(h2, TOKEN_NAT_TOK)) return branch_leg(heap, mode, scope, h2);
  if (head_is(h2, TOKEN_IDENT)) return branch_ctor(heap, mode, scope, h2);
  return fail_at(h1.token->loc, "expected a leg number or a constructor name after '|'");
}

/* PFields: List.append(List.reverse(fields(bs)), acc) = push each field */
static PValueResult parse_fields(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_LPAREN)) {
    PValueResult bs = read_binders(heap, parse(heap, mode_of(P_BINDER_GROUP), scope, tokens));
    if (failed(bs)) return bs;
    Vec made = fields(heap, &bs.value.list);
    Vec acc = mode.acc;
    push_all_from(heap, &acc, &made, 0);
    return parse(heap, mode_acc(P_FIELDS, acc), scope, bs.value.rest);
  }
  ParsedQuantity q = mark(tokens);
  Mode m = mode_acc(P_BARE_FIELD, mode.acc);
  m.original = tokens;
  m.q = q.value;
  return parse(heap, m, scope, q.rest);
}

/* PBareField */
static PValueResult parse_bare_field(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_IDENT)) return done_value(fields_value(mode.acc, mode.original));
  MaybeSTerm none;
  none.some = false;
  none.value = NULL;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)sfield_new(heap, sfield(mode.q, h.token->kind.name, none)));
  return parse(heap, mode_acc(P_FIELDS, acc), scope, h.tail);
}

/* PDecls: the Eof arm returns DeclsValue{decls, []}; the cursor past Eof is the empty list */
static PValueResult parse_decls(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_EOF)) return done_value(decls_value(mode.acc, h.tail));
  PValueResult d = read_decl(heap, parse(heap, mode_of(P_DECL), scope, tokens));
  if (failed(d)) return d;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)d.value.decl);
  return parse(heap, mode_acc(P_DECLS, acc), scope, d.value.rest);
}

/* PDecl */
static PValueResult decl_families(Heap *heap, Mode group, Vec scope, Tokens tail) {
  PValueResult fs = read_families(heap, parse(heap, group, scope, tail));
  if (failed(fs)) return fs;
  return done_value(decl_value(sdecl_mu_new(heap, fs.value.list), fs.value.rest));
}

static PValueResult parse_decl(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  TokensHead h3 = tokens_head(h2.tail);
  if (head_is(h1, TOKEN_KDEF) && head_is(h2, TOKEN_KREC)) {
    PValueResult ds = read_recs(heap, parse(heap, mode_acc(P_REC_GROUP, empty_vec(heap)), scope, h2.tail));
    if (failed(ds)) return ds;
    return done_value(decl_value(sdecl_rec_new(heap, ds.value.list), ds.value.rest));
  }
  bool def_name = head_is(h1, TOKEN_KDEF) && head_is(h2, TOKEN_IDENT);
  bool def_shape = def_name && (head_is(h3, TOKEN_COLON) || head_is(h3, TOKEN_LPAREN));
  if (def_shape) return parse(heap, mode_def(false), scope, h1.tail);
  if (head_is(h1, TOKEN_KAXIOM) && head_is(h2, TOKEN_IDENT) && head_is(h3, TOKEN_COLON)) {
    PValueResult ty = sub_term(heap, P_TERM, scope, h3.tail);
    if (failed(ty)) return ty;
    return done_value(decl_value(sdecl_axiom_new(heap, h2.token->kind.name, ty.value.term), ty.value.rest));
  }
  if (head_is(h1, TOKEN_KMU)) return decl_families(heap, mode_acc(P_FAMILY_GROUP, empty_vec(heap)), scope, h1.tail);
  if (head_is(h1, TOKEN_KMUTUAL)) return decl_families(heap, mode_acc(P_MUTUAL, empty_vec(heap)), scope, h1.tail);
  if (head_is(h1, TOKEN_KNU)) return fail_at(h1.token->loc, "nu arrives at M2");
  if (head_is(h1, TOKEN_KPOLY) || head_is(h1, TOKEN_KSPECIALIZE)) return extended_decl(heap, tokens, scope);
  return fail_expected(heap, "'def NAME :', 'def rec NAME :', 'axiom NAME :', 'mu NAME' or 'mutual'", tokens);
}

/* PDef */
static PValueResult def_rec(Heap *heap, Vec scope, TokensHead h1, TokensHead h2) {
  PValueResult ty = sub_term(heap, P_TERM, scope, h2.tail);
  if (failed(ty)) return ty;
  TokensResult t = expect(heap, token_kind(TOKEN_COLON_EQ), ty.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult body = sub_term(heap, P_TERM, scope, t.value);
  if (failed(body)) return body;
  const SRecDef *rec = srec_def_new(heap, h1.token->kind.name, ty.value.term, body.value.term);
  return done_value(rec_value(rec, body.value.rest));
}

static PValueResult def_plain(Heap *heap, Vec scope, TokensHead h1) {
  PValueResult bs = read_binders(heap, parse(heap, mode_acc(P_BINDERS, empty_vec(heap)), scope, h1.tail));
  if (failed(bs)) return bs;
  TokensResult t1 = expect(heap, token_kind(TOKEN_COLON), bs.value.rest);
  if (t1.kind == RESULT_FAIL) return fail_value(t1.error);
  PValueResult ty = sub_term(heap, P_TERM, scope, t1.value);
  if (failed(ty)) return ty;
  TokensResult t2 = expect(heap, token_kind(TOKEN_COLON_EQ), ty.value.rest);
  if (t2.kind == RESULT_FAIL) return fail_value(t2.error);
  PValueResult body = sub_term(heap, P_TERM, scope, t2.value);
  if (failed(body)) return body;
  const SDecl *decl = make_def(heap, h1.token->kind.name, bs.value.list, ty.value.term, body.value.term);
  return done_value(decl_value(decl, body.value.rest));
}

static PValueResult parse_def(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  if (mode.rec && head_is(h1, TOKEN_IDENT) && head_is(h2, TOKEN_COLON)) return def_rec(heap, scope, h1, h2);
  if (!mode.rec && head_is(h1, TOKEN_IDENT)) return def_plain(heap, scope, h1);
  return fail_expected(heap, "a definition name and ':'", tokens);
}

/* PRecGroup */
static PValueResult parse_rec_group(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult d = read_rec(heap, parse(heap, mode_def(true), scope, tokens));
  if (failed(d)) return d;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)d.value.rec);
  return parse(heap, mode_acc(P_REC_REST, acc), scope, d.value.rest);
}

/* PRecRest */
static PValueResult parse_rec_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_KAND)) return parse(heap, mode_acc(P_REC_GROUP, mode.acc), scope, h.tail);
  return done_value(recs_value(mode.acc, tokens));
}

/* PFamily */
static PValueResult parse_family(Heap *heap, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (!head_is(h, TOKEN_IDENT)) return fail_expected(heap, "a family name", tokens);
  PValueResult bs = read_binders(heap, parse(heap, mode_acc(P_BINDERS, empty_vec(heap)), scope, h.tail));
  if (failed(bs)) return bs;
  TokensResult t = expect(heap, token_kind(TOKEN_COLON), bs.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult ty = sub_term(heap, P_TERM, scope, t.value);
  if (failed(ty)) return ty;
  TokensResult sep = family_separator(heap, ty.value.rest);
  if (sep.kind == RESULT_FAIL) return fail_value(sep.error);
  PValueResult cs = read_ctors(heap, parse(heap, mode_acc(P_FAMILY_CTORS, empty_vec(heap)), scope, sep.value));
  if (failed(cs)) return cs;
  const SFam *family = sfam_new(heap, sfam(h.token->kind.name, bs.value.list, ty.value.term, cs.value.list));
  return done_value(family_value(family, cs.value.rest));
}

/* PFamilyCtors */
static PValueResult parse_family_ctors(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  if (!head_is(h1, TOKEN_PIPE)) return done_value(ctors_value(mode.acc, tokens));
  TokensHead h2 = tokens_head(h1.tail);
  if (!head_is(h2, TOKEN_IDENT)) return fail_at(h1.token->loc, "expected a constructor name and ':' after '|'");
  PValueResult bs = read_binders(heap, parse(heap, mode_acc(P_BINDERS, empty_vec(heap)), scope, h2.tail));
  if (failed(bs)) return bs;
  TokensResult t = expect(heap, token_kind(TOKEN_COLON), bs.value.rest);
  if (t.kind == RESULT_FAIL) return fail_value(t.error);
  PValueResult ty = sub_term(heap, P_TERM, scope, t.value);
  if (failed(ty)) return ty;
  Vec acc = mode.acc;
  const STerm *ctor_ty = arrows(heap, &bs.value.list, ty.value.term, false);
  push(heap, &acc, (void *)sfam_ctor_new(heap, h2.token->kind.name, ctor_ty));
  return parse(heap, mode_acc(P_FAMILY_CTORS, acc), scope, ty.value.rest);
}

/* PFamilyGroup */
static PValueResult parse_family_group(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  PValueResult f = read_family(heap, parse(heap, mode_of(P_FAMILY), scope, tokens));
  if (failed(f)) return f;
  Vec acc = mode.acc;
  push(heap, &acc, (void *)f.value.family);
  return parse(heap, mode_acc(P_FAMILY_REST, acc), scope, f.value.rest);
}

/* PFamilyRest */
static PValueResult parse_family_rest(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_KAND)) return parse(heap, mode_acc(P_FAMILY_GROUP, mode.acc), scope, h.tail);
  return done_value(families_value(mode.acc, tokens));
}

/* PMutual: the acc reaches mutual_count without a reverse; mutual_count owns the convention */
static PValueResult parse_mutual(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  TokensHead h = tokens_head(tokens);
  if (head_is(h, TOKEN_KMU)) {
    PValueResult f = read_family(heap, parse(heap, mode_of(P_FAMILY), scope, h.tail));
    if (failed(f)) return f;
    Vec acc = mode.acc;
    push(heap, &acc, (void *)f.value.family);
    return parse(heap, mode_acc(P_MUTUAL, acc), scope, f.value.rest);
  }
  if (head_is(h, TOKEN_KEND)) {
    PVecResult fs = mutual_count(heap, mode.acc, h.token->loc);
    if (fs.kind == RESULT_FAIL) return fail_value(fs.error);
    return done_value(families_value(fs.value, h.tail));
  }
  return fail_expected(heap, "'mu' or 'end' in a mutual group", tokens);
}

static PValueResult parse(Heap *heap, Mode mode, Vec scope, Tokens tokens) {
  switch (mode.kind) {
  case P_TERM: return parse_term(heap, scope, tokens);
  case P_ARROW: return parse_arrow(heap, scope, tokens);
  case P_ARROW_TRY: return parse_arrow_try(heap, mode, scope, tokens);
  case P_ARROW_PLAIN: return parse_arrow_plain(heap, scope, tokens);
  case P_ARROW_REST: return parse_arrow_rest(heap, mode, scope, tokens);
  case P_APP: return parse_app(heap, scope, tokens);
  case P_APP_REST: return parse_app_rest(heap, mode, scope, tokens);
  case P_APP_NEXT: return parse_app_next(heap, mode, scope, tokens);
  case P_ATOM: return parse_atom(heap, scope, tokens);
  case P_HEAD: return parse_head(heap, scope, tokens);
  case P_POSTFIX: return parse_postfix(heap, mode, scope, tokens);
  case P_RETURN: return parse_return(mode, tokens);
  case P_PAREN_REST: return parse_paren_rest(heap, mode, scope, tokens);
  case P_BINDER_GROUP: return parse_binder_group(heap, scope, tokens);
  case P_BINDER_NAMES: return parse_binder_names(heap, mode, scope, tokens);
  case P_BINDERS: return parse_binders(heap, mode, scope, tokens);
  case P_FUN: return parse_fun(heap, scope, tokens);
  case P_LET: return parse_let(heap, scope, tokens);
  case P_CASE: return parse_case(heap, mode, scope, tokens);
  case P_CASE_MOTIVE: return parse_case_motive(heap, mode, scope, tokens);
  case P_CASE_RETURN: return parse_case_return(heap, mode, scope, tokens);
  case P_BRANCHES: return parse_branches(heap, mode, scope, tokens);
  case P_FIELDS: return parse_fields(heap, mode, scope, tokens);
  case P_BARE_FIELD: return parse_bare_field(heap, mode, scope, tokens);
  case P_LEVEL: return parse_level(heap, scope, tokens);
  case P_LEVEL_PAREN: return parse_level_paren(heap, scope, tokens);
  case P_ITEMS: return parse_items(heap, mode, scope, tokens);
  case P_ITEMS_BODY: return parse_items_body(heap, mode, scope, tokens);
  case P_ITEMS_NEXT: return parse_items_next(heap, mode, scope, tokens);
  case P_ITEMS_REST: return parse_items_rest(heap, mode, scope, tokens);
  case P_ATOMS: return parse_atoms(heap, mode, scope, tokens);
  case P_ATOMS_NEXT: return parse_atoms_next(heap, mode, scope, tokens);
  case P_ATOMS_DONE: return parse_atoms_done(mode, tokens);
  case P_DECL: return parse_decl(heap, scope, tokens);
  case P_DECLS: return parse_decls(heap, mode, scope, tokens);
  case P_DEF: return parse_def(heap, mode, scope, tokens);
  case P_REC_GROUP: return parse_rec_group(heap, mode, scope, tokens);
  case P_REC_REST: return parse_rec_rest(heap, mode, scope, tokens);
  case P_FAMILY: return parse_family(heap, scope, tokens);
  case P_FAMILY_CTORS: return parse_family_ctors(heap, mode, scope, tokens);
  case P_FAMILY_GROUP: return parse_family_group(heap, mode, scope, tokens);
  case P_FAMILY_REST: return parse_family_rest(heap, mode, scope, tokens);
  case P_MUTUAL: return parse_mutual(heap, mode, scope, tokens);
  }
  /* unreachable: every ModeKind returns above */
  return fail_expected(heap, "a parser mode", tokens);
}

/* ---- chunk 3 = Bend 888-1155: finish_term .. extended_decl, then the exports term and program ---- */

/* Result<&2, &2, Error.T, Parsed<X>> for the X chunk 3 returns. ListParsed holds List<String> (Str
 * pointers), List<Universe.T> (Universe pointers), List<Common.Pair<String, String>> (PolyRename pointers
 * whose first and second are Str pointers), List<Syntax.RecDef<Syntax.T>> (SRecDef pointers) and
 * List<Syntax.Specialization> (SSpecialization pointers). */
typedef struct {
  ResultKind kind;
  Vec value;
  Tokens rest;
  Error error;
} ListParsed;
typedef struct {
  ResultKind kind;
  MaybeVec value;
  Tokens rest;
  Error error;
} ExportsParsed;
typedef struct {
  ResultKind kind;
  Str value;
  Tokens rest;
  Error error;
} NameParsed;
typedef struct {
  ResultKind kind;
  SSpecialization value;
  Tokens rest;
  Error error;
} SpecParsed;
typedef struct {
  ResultKind kind;
  const SRecDef *value;
  Error error;
} RecDefResult;
typedef struct {
  ResultKind kind;
  const SDecl *value;
  Error error;
} DeclResult;

static MaybeVec none_vec(void);

/* The error slot of a Done result is never read; it holds this placeholder. */
static Error placeholder_error(void) { return error_host_oom(); }

static ListParsed list_done(Vec value, Tokens rest) {
  ListParsed r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.rest = rest;
  r.error = placeholder_error();
  return r;
}

static ListParsed list_fail(Error err) {
  ListParsed r;
  r.kind = RESULT_FAIL;
  r.value.arena = NULL;
  r.value.items = NULL;
  r.value.len = 0;
  r.value.cap = 0;
  r.rest.all = NULL;
  r.rest.pos = 0;
  r.error = err;
  return r;
}

static ListParsed list_expected(Heap *heap, const char *what, Tokens tokens) {
  return list_fail(expected(heap, str_lit(what), tokens));
}

static ExportsParsed exports_done(MaybeVec value, Tokens rest) {
  ExportsParsed r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.rest = rest;
  r.error = placeholder_error();
  return r;
}

static ExportsParsed exports_fail(Error err) {
  ExportsParsed r;
  r.kind = RESULT_FAIL;
  r.value.some = false;
  r.value.value.arena = NULL;
  r.value.value.items = NULL;
  r.value.value.len = 0;
  r.value.value.cap = 0;
  r.rest.all = NULL;
  r.rest.pos = 0;
  r.error = err;
  return r;
}

static NameParsed name_done(Str value, Tokens rest) {
  NameParsed r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.rest = rest;
  r.error = placeholder_error();
  return r;
}

static NameParsed name_fail(Error err) {
  NameParsed r;
  r.kind = RESULT_FAIL;
  r.value = str_empty();
  r.rest.all = NULL;
  r.rest.pos = 0;
  r.error = err;
  return r;
}

static SpecParsed spec_done(SSpecialization value, Tokens rest) {
  SpecParsed r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.rest = rest;
  r.error = placeholder_error();
  return r;
}

static SpecParsed spec_fail(Heap *heap, Error err) {
  SpecParsed r;
  r.kind = RESULT_FAIL;
  r.value = sspecialization(str_empty(), empty_vec(heap), str_empty(), empty_vec(heap), none_vec());
  r.rest.all = NULL;
  r.rest.pos = 0;
  r.error = err;
  return r;
}

static RecDefResult recdef_done(const SRecDef *value) {
  RecDefResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = placeholder_error();
  return r;
}

static RecDefResult recdef_fail(Error err) {
  RecDefResult r;
  r.kind = RESULT_FAIL;
  r.value = NULL;
  r.error = err;
  return r;
}

static DeclResult decl_done(const SDecl *value) {
  DeclResult r;
  r.kind = RESULT_DONE;
  r.value = value;
  r.error = placeholder_error();
  return r;
}

static DeclResult decl_fail(Error err) {
  DeclResult r;
  r.kind = RESULT_FAIL;
  r.value = NULL;
  r.error = err;
  return r;
}

static MaybeVec none_vec(void) {
  MaybeVec m;
  m.some = false;
  m.value.arena = NULL;
  m.value.items = NULL;
  m.value.len = 0;
  m.value.cap = 0;
  return m;
}

static MaybeVec some_vec(Vec value) {
  MaybeVec m;
  m.some = true;
  m.value = value;
  return m;
}

/* Common.Pair{local, target} as the PolyRename record {source, target} that family_poly.c reads
 * (mapping_find, validate_export_binding): the same record on both sides of the port boundary. */
static PolyRename pair_placeholder;
static PolyRename *pair_new(Heap *heap, Str first, Str second) {
  PolyRename *pair = heap_alloc(heap, sizeof *pair);
  if (!pair) return &pair_placeholder;
  pair->source = first;
  pair->target = second;
  return pair;
}

/* finish_term: Parsed{term, [Eof]} is Done{term}; anything else is "expected end of input". */
static TermResult finish_term(Heap *heap, PValue parsed) {
  TokensHead head = tokens_head(parsed.rest);
  if (head_is(head, TOKEN_EOF) && !tokens_head(head.tail).some) return done_term(parsed.term);
  return fail_term(expected(heap, str_lit("end of input"), parsed.rest));
}

static UnitResult duplicate_universe(Heap *heap, MaybeNat index, Str name, TokenLoc loc) {
  if (index.some) return fail_unit(error(loc, str_cat(heap->arena, str_lit("duplicate universe "), name)));
  return done_unit();
}

static UnitResult check_universe_name(Heap *heap, Str name, const Vec *acc, TokenLoc loc) {
  bool reserved = str_eq(name, str_lit("succ")) || str_eq(name, str_lit("max")) || str_eq(name, str_lit("imax"));
  if (reserved) return fail_unit(error(loc, str_cat(heap->arena, str_lit("reserved universe name "), name)));
  return duplicate_universe(heap, index_of(heap, acc, name, nat_zero()), name, loc);
}

/* universe_names: `name <> acc` then List.reverse at ')' is push in source order. */
static ListParsed universe_names(Heap *heap, Tokens tokens, Vec acc, bool entry) {
  TokensHead head = tokens_head(tokens);
  if (entry) {
    if (head_is(head, TOKEN_IDENT)) {
      Str name = head.token->kind.name;
      UnitResult checked = check_universe_name(heap, name, &acc, head.token->loc);
      if (checked.kind == RESULT_FAIL) return list_fail(checked.error);
      push(heap, &acc, name_new(heap, name));
      return universe_names(heap, head.tail, acc, false);
    }
    return list_expected(heap, "a universe binder", tokens);
  }
  if (head_is(head, TOKEN_COMMA)) return universe_names(heap, head.tail, acc, true);
  if (head_is(head, TOKEN_RPAREN)) return list_done(acc, head.tail);
  return list_expected(heap, "',' or ')' after a universe binder", tokens);
}

static ListParsed universe_arguments(Heap *heap, bool entry, Tokens tokens, Vec scope, Vec acc) {
  TokensHead head = tokens_head(tokens);
  if (!entry) {
    if (head_is(head, TOKEN_COMMA)) return universe_arguments(heap, true, head.tail, scope, acc);
    if (head_is(head, TOKEN_RPAREN)) return list_done(acc, head.tail);
    return list_expected(heap, "',' or ')' after a universe argument", tokens);
  }
  PValueResult u = sub_level(heap, scope, tokens);
  if (failed(u)) return list_fail(u.error);
  push(heap, &acc, (void *)u.value.level);
  return universe_arguments(heap, false, u.value.rest, scope, acc);
}

static ListParsed mapping_list(Heap *heap, bool entry, Tokens tokens, Vec acc) {
  TokensHead h1 = tokens_head(tokens);
  if (entry) {
    TokensHead h2 = tokens_head(h1.tail);
    TokensHead h3 = tokens_head(h2.tail);
    if (head_is(h1, TOKEN_IDENT) && head_is(h2, TOKEN_COLON_EQ) && head_is(h3, TOKEN_IDENT)) {
      push(heap, &acc, pair_new(heap, h1.token->kind.name, h3.token->kind.name));
      return mapping_list(heap, false, h3.tail, acc);
    }
    return list_expected(heap, "a family binding 'LOCAL := EXISTING'", tokens);
  }
  if (head_is(h1, TOKEN_COMMA)) return mapping_list(heap, true, h1.tail, acc);
  if (head_is(h1, TOKEN_RPAREN)) return list_done(acc, h1.tail);
  return list_expected(heap, "',' or ')' after a family binding", tokens);
}

static ListParsed export_mapping_list(Heap *heap, bool entry, Tokens tokens, Vec acc) {
  TokensHead h1 = tokens_head(tokens);
  if (entry) {
    TokensHead h2 = tokens_head(h1.tail);
    TokensHead h3 = tokens_head(h2.tail);
    if (head_is(h1, TOKEN_IDENT) && head_is(h2, TOKEN_COLON_EQ) && head_is(h3, TOKEN_IDENT)) {
      push(heap, &acc, pair_new(heap, h1.token->kind.name, h3.token->kind.name));
      return export_mapping_list(heap, false, h3.tail, acc);
    }
    if (head_is(h1, TOKEN_IDENT) && head_is(h2, TOKEN_COLON_EQ)) {
      return list_expected(heap, "a global name after ':='", h2.tail);
    }
    return list_expected(heap, "a member export 'MEMBER := NAME'", tokens);
  }
  if (head_is(h1, TOKEN_COMMA)) return export_mapping_list(heap, true, h1.tail, acc);
  if (head_is(h1, TOKEN_RPAREN)) return list_done(acc, h1.tail);
  return list_expected(heap, "',' or ')' after a member export", tokens);
}

static ListParsed reuse_bindings(Heap *heap, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  if (head_is(h1, TOKEN_KWITH) && head_is(h2, TOKEN_LPAREN)) return mapping_list(heap, true, h2.tail, empty_vec(heap));
  if (head_is(h1, TOKEN_KWITH)) return list_expected(heap, "'(' and a nonempty list of family bindings", h1.tail);
  return list_done(empty_vec(heap), tokens);
}

/* export_bindings: the Bend matches the identifier spelled "export" character by character. */
static ExportsParsed export_bindings(Heap *heap, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  TokensHead h3 = tokens_head(h2.tail);
  bool export_word = head_is_word(h1, "export");
  if (export_word && head_is(h2, TOKEN_UNIT_TOK)) return exports_done(some_vec(empty_vec(heap)), h2.tail);
  if (export_word && head_is(h2, TOKEN_LPAREN) && head_is(h3, TOKEN_RPAREN)) {
    return exports_done(some_vec(empty_vec(heap)), h3.tail);
  }
  if (export_word && head_is(h2, TOKEN_LPAREN)) {
    ListParsed mapping = export_mapping_list(heap, true, h2.tail, empty_vec(heap));
    if (mapping.kind == RESULT_FAIL) return exports_fail(mapping.error);
    return exports_done(some_vec(mapping.value), mapping.rest);
  }
  if (export_word) return exports_fail(expected(heap, str_lit("'(' after 'export'"), h1.tail));
  return exports_done(none_vec(), tokens);
}

static UnitResult specialization_tail(Heap *heap, MaybeVec exports, Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  (void)heap;
  if (!exports.some && head_is(head, TOKEN_KWITH)) {
    return fail_unit(error(head.token->loc, str_lit("one 'with' clause per specialization")));
  }
  if (exports.some && head_is(head, TOKEN_KWITH)) {
    return fail_unit(error(head.token->loc, str_lit("'with' must precede 'export' in a specialization")));
  }
  if (head_is_word(head, "export")) {
    return fail_unit(error(head.token->loc, str_lit("one export clause per specialization")));
  }
  return done_unit();
}

static NameParsed specialization_as(Heap *heap, Tokens tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  if (head_is(h1, TOKEN_KAS) && head_is(h2, TOKEN_IDENT)) return name_done(h2.token->kind.name, h2.tail);
  return name_fail(expected(heap, str_lit("'as' and a fresh name"), tokens));
}

static SpecParsed specialization(Heap *heap, Tokens tokens, Vec scope) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  TokensHead h3 = tokens_head(h2.tail);
  if (head_is(h1, TOKEN_KSPECIALIZE) && head_is(h2, TOKEN_IDENT) && head_is(h3, TOKEN_LPAREN)) {
    ListParsed levels = universe_arguments(heap, true, h3.tail, scope, empty_vec(heap));
    if (levels.kind == RESULT_FAIL) return spec_fail(heap, levels.error);
    NameParsed alias = specialization_as(heap, levels.rest);
    if (alias.kind == RESULT_FAIL) return spec_fail(heap, alias.error);
    ListParsed reuse = reuse_bindings(heap, alias.rest);
    if (reuse.kind == RESULT_FAIL) return spec_fail(heap, reuse.error);
    ExportsParsed exports = export_bindings(heap, reuse.rest);
    if (exports.kind == RESULT_FAIL) return spec_fail(heap, exports.error);
    UnitResult checked = specialization_tail(heap, exports.value, exports.rest);
    if (checked.kind == RESULT_FAIL) return spec_fail(heap, checked.error);
    return spec_done(sspecialization(h2.token->kind.name, levels.value, alias.value, reuse.value, exports.value),
                     exports.rest);
  }
  return spec_fail(heap, expected(heap, str_lit("'specialize NAME (LEVELS) as NAME'"), tokens));
}

static RecDefResult member(Heap *heap, const SDecl *decl, Tokens tokens) {
  if (decl->kind == SDECL_DEF) return recdef_done(srec_def_new(heap, decl->name, decl->ty, decl->body));
  return recdef_fail(expected(heap, str_lit("a nonrecursive member definition"), tokens));
}

/* finish_members: the Bend reports the error on the one-token list [KEnd at loc]; end_tokens is the
 * cursor at that 'end' token, which `expected` describes the same way (Decision 72). */
static ListParsed finish_members(Heap *heap, Vec acc, Tokens tail, Tokens end_tokens) {
  if (acc.len == 0) return list_expected(heap, "a nonempty list of member definitions", end_tokens);
  return list_done(acc, tail);
}

static bool member_start(Tokens tokens) {
  TokensHead head = tokens_head(tokens);
  return head_is(head, TOKEN_IDENT) || head_is(head, TOKEN_KREC);
}

static TokensResult member_input(Heap *heap, bool valid, Tokens tokens) {
  TokensResult r;
  r.kind = valid ? RESULT_DONE : RESULT_FAIL;
  r.value = tokens;
  r.error = valid ? placeholder_error() : expected(heap, str_lit("'def' or 'end' in family members"), tokens);
  return r;
}

static ListParsed members(Heap *heap, Tokens tokens, Vec scope, Vec acc) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, TOKEN_KEND)) return finish_members(heap, acc, head.tail, tokens);
  if (head_is(head, TOKEN_KDEF)) {
    TokensResult input = member_input(heap, member_start(head.tail), tokens);
    if (input.kind == RESULT_FAIL) return list_fail(input.error);
    PValueResult decl = read_decl(heap, parse(heap, mode_of(P_DECL), scope, input.value));
    if (failed(decl)) return list_fail(decl.error);
    RecDefResult entry = member(heap, decl.value.decl, input.value);
    if (entry.kind == RESULT_FAIL) return list_fail(entry.error);
    push(heap, &acc, (void *)entry.value);
    return members(heap, decl.value.rest, scope, acc);
  }
  return list_expected(heap, "'def' or 'end' in family members", tokens);
}

static ListParsed optional_members(Heap *heap, Tokens tokens, Vec scope) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, TOKEN_KWHERE)) return members(heap, head.tail, scope, empty_vec(heap));
  return list_done(empty_vec(heap), tokens);
}

static ListParsed finish_dependencies(Heap *heap, Vec acc, Tokens tail) {
  if (acc.len == 0) return list_expected(heap, "at least one template specialization in a group", tail);
  return list_done(acc, tail);
}

static ListParsed dependencies(Heap *heap, Tokens tokens, Vec scope, Vec acc) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, TOKEN_KSPECIALIZE)) {
    SpecParsed entry = specialization(heap, tokens, scope);
    if (entry.kind == RESULT_FAIL) return list_fail(entry.error);
    push(heap, &acc, (void *)sspecialization_new(heap, entry.value));
    return dependencies(heap, entry.rest, scope, acc);
  }
  return finish_dependencies(heap, acc, tokens);
}

static ListParsed compose_members(Heap *heap, Tokens tokens, Vec scope) {
  TokensHead head = tokens_head(tokens);
  if (head_is(head, TOKEN_KEND)) return list_done(empty_vec(heap), head.tail);
  return members(heap, tokens, scope, empty_vec(heap));
}

static DeclResult poly_family(Heap *heap, Nat arity, Vec families, Vec definitions) {
  VecGet first = vec_get(&families, 0);
  if (first.status != VEC_GET_OK) {
    Nat one = nat_of_u64(heap->arena, 1);
    return decl_fail(error_parse(str_lit("expected a nonempty family group"), one, one));
  }
  const SFam *family = (const SFam *)first.value;
  if (families.len == 1 && definitions.len == 0) return decl_done(sdecl_poly_mu_new(heap, arity, *family));
  Vec tail = empty_vec(heap);
  push_all_from(heap, &tail, &families, 1);
  return decl_done(sdecl_poly_group_new(heap, arity, *family, tail, definitions));
}

/* poly_def: the Bend reports the error on [KPoly at original_loc]; poly_tokens is the cursor at that
 * 'poly' token (Decision 72). */
static DeclResult poly_def(Heap *heap, Nat arity, const SDecl *decl, Tokens poly_tokens) {
  if (decl->kind == SDECL_DEF) return decl_done(sdecl_poly_new(heap, arity, decl->name, decl->ty, decl->body));
  return decl_fail(
      expected(heap, str_lit("a nonrecursive definition or single family after universe binders"), poly_tokens));
}

static PValueResult scoped_poly(Heap *heap, Tokens tokens, Vec scope, Tokens poly_tokens) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  TokensHead h3 = tokens_head(h2.tail);
  Nat arity = nat_of_u64(heap->arena, scope.len);
  if (head_is(h1, TOKEN_KGROUP) && head_is(h2, TOKEN_IDENT) && head_is(h3, TOKEN_KWHERE)) {
    ListParsed deps = dependencies(heap, h3.tail, scope, empty_vec(heap));
    if (deps.kind == RESULT_FAIL) return fail_value(deps.error);
    ListParsed defs = compose_members(heap, deps.rest, scope);
    if (defs.kind == RESULT_FAIL) return fail_value(defs.error);
    const SDecl *decl = sdecl_poly_compose_new(heap, arity, h2.token->kind.name, deps.value, defs.value);
    return done_value(decl_value(decl, defs.rest));
  }
  if (head_is(h1, TOKEN_KGROUP)) return fail_expected(heap, "'group NAME where' after universe binders", tokens);
  if (head_is(h1, TOKEN_KMU)) {
    PValueResult families = read_families(heap, parse(heap, mode_acc(P_FAMILY_GROUP, empty_vec(heap)), scope, h1.tail));
    if (failed(families)) return families;
    ListParsed definitions = optional_members(heap, families.value.rest, scope);
    if (definitions.kind == RESULT_FAIL) return fail_value(definitions.error);
    DeclResult decl = poly_family(heap, arity, families.value.list, definitions.value);
    if (decl.kind == RESULT_FAIL) return fail_value(decl.error);
    return done_value(decl_value(decl.value, definitions.rest));
  }
  PValueResult parsed = read_decl(heap, parse(heap, mode_of(P_DECL), scope, tokens));
  if (failed(parsed)) return parsed;
  DeclResult decl = poly_def(heap, arity, parsed.value.decl, poly_tokens);
  if (decl.kind == RESULT_FAIL) return fail_value(decl.error);
  return done_value(decl_value(decl.value, parsed.value.rest));
}

static PValueResult extended_decl(Heap *heap, Tokens tokens, Vec scope) {
  TokensHead h1 = tokens_head(tokens);
  TokensHead h2 = tokens_head(h1.tail);
  TokensHead h3 = tokens_head(h2.tail);
  if (head_is(h1, TOKEN_KPOLY) && head_is(h2, TOKEN_LPAREN)) {
    ListParsed universes = universe_names(heap, h2.tail, empty_vec(heap), true);
    if (universes.kind == RESULT_FAIL) return fail_value(universes.error);
    return scoped_poly(heap, universes.rest, universes.value, tokens);
  }
  if (head_is(h1, TOKEN_KPOLY)) {
    return fail_expected(heap, "'(' and a nonempty list of universe binders after 'poly'", tokens);
  }
  if (head_is(h1, TOKEN_KSPECIALIZE) && head_is(h2, TOKEN_IDENT) && head_is(h3, TOKEN_LPAREN)) {
    SpecParsed entry = specialization(heap, tokens, scope);
    if (entry.kind == RESULT_FAIL) return fail_value(entry.error);
    return done_value(decl_value(sdecl_specialize_new(heap, entry.value), entry.rest));
  }
  return fail_expected(
      heap, "NAME, '(' with a nonempty list of universe levels, ')', 'as' and a name after 'specialize'", tokens);
}

/* ---- exports: term and program. A host allocation failure anywhere in the parse (heap->oom) is
 * Error.HostOom, which the Bend host raised; the C port reports it as a Fail. ---- */
static Result export_result(Heap *heap, ResultKind kind, void *value, Error err) {
  Result r;
  r.kind = heap->oom ? RESULT_FAIL : kind;
  r.value = heap->oom ? NULL : value;
  r.error = heap->oom ? error_host_oom() : err;
  return r;
}

Result parser_term(Heap *heap, Str source) {
  Result lexed = lexer_lex(heap, source);
  if (lexed.kind == RESULT_FAIL) return export_result(heap, RESULT_FAIL, NULL, lexed.error);
  PValueResult parsed = sub_term(heap, P_TERM, empty_vec(heap), tokens_new((const Vec *)lexed.value));
  if (failed(parsed)) return export_result(heap, RESULT_FAIL, NULL, parsed.error);
  TermResult finished = finish_term(heap, parsed.value);
  if (finished.kind == RESULT_FAIL) return export_result(heap, RESULT_FAIL, NULL, finished.error);
  return export_result(heap, RESULT_DONE, (void *)finished.value, placeholder_error());
}

Result parser_program(Heap *heap, Str source) {
  Result lexed = lexer_lex(heap, source);
  if (lexed.kind == RESULT_FAIL) return export_result(heap, RESULT_FAIL, NULL, lexed.error);
  Tokens tokens = tokens_new((const Vec *)lexed.value);
  PValueResult parsed = read_decls(heap, parse(heap, mode_acc(P_DECLS, empty_vec(heap)), empty_vec(heap), tokens));
  if (failed(parsed)) return export_result(heap, RESULT_FAIL, NULL, parsed.error);
  Vec *decls = heap_alloc(heap, sizeof *decls);
  if (!decls) return export_result(heap, RESULT_FAIL, NULL, error_host_oom());
  *decls = parsed.value.list;
  return export_result(heap, RESULT_DONE, decls, placeholder_error());
}
