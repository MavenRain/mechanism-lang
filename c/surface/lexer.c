/* Port of bend2/surface/lexer.bend: the scanner from source bytes to the token list. The Bend
 * version re-encodes its codepoint String as UTF-8 bytes before scanning (utf8_four .. utf8); the C
 * host reads source as bytes already, so those five definitions are the identity here and are not
 * ported. Columns advance per byte exactly as the Bend. The Bend conses tokens, text and bytes in
 * reverse and reverses them at the end; the C buffers push in order, which yields the same lists. */
#include "surface.h"

/* Mode */
typedef enum {
  MODE_SCAN,
  MODE_WORD,
  MODE_NUMBER,
  MODE_DOT_NUMBER,
  MODE_COMMENT,
  MODE_BYTE_STRING,
  MODE_ESCAPE,
  MODE_HEX_FIRST,
  MODE_HEX_SECOND
} ModeKind;
typedef struct {
  ModeKind kind;
  uint32_t high; /* HEX_SECOND */
} Mode;

/* State: Running{mode, source, loc, start, tokens, text, bytes} | Finished{tokens} | Failed{error} */
typedef enum { STATE_RUNNING, STATE_FINISHED, STATE_FAILED } StateKind;
typedef struct {
  StateKind kind;
  Mode mode;      /* RUNNING */
  Str source;     /* RUNNING: the unread bytes */
  TokenLoc loc;   /* RUNNING */
  TokenLoc start; /* RUNNING */
  Vec tokens;     /* RUNNING, FINISHED: Token pointers in emission order */
  StrBuf text;    /* RUNNING: the word or digits read so far */
  Vec bytes;      /* RUNNING: uint32_t pointers in source order */
  Error error;    /* FAILED */
} State;

typedef struct {
  bool some;
  TokenKind value;
} MaybeTokenKind;

static Mode mode_of(ModeKind kind) {
  Mode mode;
  mode.kind = kind;
  mode.high = 0;
  return mode;
}

static Mode mode_hex_second(uint32_t high) {
  Mode mode;
  mode.kind = MODE_HEX_SECOND;
  mode.high = high;
  return mode;
}

/* SCon views over the unread bytes: at(s, i) is byte i (0 past the end), rest(s, n) drops n bytes. */
static uint32_t at(Str s, size_t i) { return i < s.len ? (uint32_t)(unsigned char)s.data[i] : 0; }

static Str rest(Str s, size_t count) {
  size_t n = count < s.len ? count : s.len;
  Str out;
  out.data = s.data + n;
  out.len = s.len - n;
  return out;
}

/* SCon{c, ""} inside a message: the Bend Char is the codepoint of the scanned byte, so bytes 128..255
 * print as two UTF-8 bytes. */
static Str char_text(Heap *heap, uint32_t c) {
  StrBuf buf = strbuf_new(heap->arena);
  bool ok = c < 128 ? strbuf_push_byte(&buf, (char)c)
                    : (strbuf_push_byte(&buf, (char)(0xC0 | (c >> 6))) && strbuf_push_byte(&buf, (char)(0x80 | (c & 63))));
  if (!ok) heap->oom = true;
  return strbuf_str(&buf);
}

/* value <> bytes (in source order) */
static Vec push_byte(Heap *heap, Vec bytes, uint32_t value) {
  uint32_t *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return bytes;
  *boxed = value;
  if (!vec_push(&bytes, boxed)) heap->oom = true;
  return bytes;
}

/* SCon{c, text} (in source order) */
static StrBuf push_char(Heap *heap, StrBuf text, uint32_t c) {
  if (!strbuf_push_byte(&text, (char)c)) heap->oom = true;
  return text;
}

/* SCon{c, ""} */
static StrBuf text_of(Heap *heap, uint32_t c) { return push_char(heap, strbuf_new(heap->arena), c); }

static State running(Mode mode, Str source, TokenLoc loc, TokenLoc start, Vec tokens, StrBuf text, Vec bytes) {
  State state;
  state.kind = STATE_RUNNING;
  state.mode = mode;
  state.source = source;
  state.loc = loc;
  state.start = start;
  state.tokens = tokens;
  state.text = text;
  state.bytes = bytes;
  state.error = error_host_oom();
  return state;
}

/* Running{mode, source, loc, start, tokens, "", []} */
static State running_fresh(Heap *heap, Mode mode, Str source, TokenLoc loc, TokenLoc start, Vec tokens) {
  return running(mode, source, loc, start, tokens, strbuf_new(heap->arena), vec_new(heap->arena));
}

/* fail(loc, message): Failed{Error.Parse{message, line, col}} */
static State fail(Heap *heap, TokenLoc loc, Str message) {
  State state = running_fresh(heap, mode_of(MODE_SCAN), str_lit(""), loc, loc, vec_new(heap->arena));
  state.kind = STATE_FAILED;
  state.error = error_parse(message, loc.line, loc.col);
  return state;
}

/* emit(kind, source, loc, start, tokens): Running{Scan{}, source, loc, start, Tok{kind, start} <> tokens, "", []} */
static State emit(Heap *heap, TokenKind kind, Str source, TokenLoc loc, TokenLoc start, Vec tokens) {
  Token *token = heap_alloc(heap, sizeof *token);
  if (token != NULL) {
    token->kind = kind;
    token->loc = start;
    if (!vec_push(&tokens, token)) heap->oom = true;
  }
  return running_fresh(heap, mode_of(MODE_SCAN), source, loc, start, tokens);
}

static bool is_digit(uint32_t c) { return c >= 48 && c <= 57; }

/* Char.is_alpha: Char.is_upper || Char.is_lower, the ASCII letters. */
static bool is_alpha(uint32_t c) { return (c >= 65 && c <= 90) || (c >= 97 && c <= 122); }

static bool is_ident_start(uint32_t c) { return is_alpha(c) || c == '_'; }

static bool is_ident_char(uint32_t c) { return is_ident_start(c) || is_digit(c) || c == '\''; }

/* ident_kind(name): the Bend letter cases pick a keyword when the tail matches, else Ident{name}.
 * The table lists every (letter, tail) pair of those cases as a whole word. */
typedef struct {
  const char *word;
  TokenKindTag tag;
} Keyword;

static const Keyword keywords[] = {
    {"def", TOKEN_KDEF},        {"dec", TOKEN_KDEC},        {"axiom", TOKEN_KAXIOM},     {"as", TOKEN_KAS},
    {"absurd", TOKEN_KABSURD},  {"auto", TOKEN_KAUTO},      {"and", TOKEN_KAND},         {"fun", TOKEN_KFUN},
    {"fhc", TOKEN_KFHC},        {"inj", TOKEN_KINJ},        {"in", TOKEN_KIN},           {"input", TOKEN_KINPUT},
    {"of", TOKEN_KOF},          {"open", TOKEN_KOPEN},      {"case", TOKEN_KCASE},       {"match", TOKEN_KMATCH},
    {"mu", TOKEN_KMU},          {"mutual", TOKEN_KMUTUAL},  {"mpc", TOKEN_KMPC},         {"return", TOKEN_KRETURN},
    {"rec", TOKEN_KREC},        {"with", TOKEN_KWITH},      {"where", TOKEN_KWHERE},     {"tuple", TOKEN_KTUPLE},
    {"sum", TOKEN_KSUM},        {"specialize", TOKEN_KSPECIALIZE}, {"share", TOKEN_KSHARE}, {"prod", TOKEN_KPROD},
    {"poly", TOKEN_KPOLY},      {"prove", TOKEN_KPROVE},    {"Prop", TOKEN_KPROP},       {"Type", TOKEN_KTYPE},
    {"Sort", TOKEN_KSORT},      {"group", TOKEN_KGROUP},    {"let", TOKEN_KLET},         {"natAdd", TOKEN_KNAT_ADD},
    {"natSub", TOKEN_KNAT_SUB}, {"natMul", TOKEN_KNAT_MUL}, {"natEq", TOKEN_KNAT_EQ},    {"natLt", TOKEN_KNAT_LT},
    {"nu", TOKEN_KNU},          {"end", TOKEN_KEND},        {"enc", TOKEN_KENC},         {"eval", TOKEN_KEVAL},
    {"zk", TOKEN_KZK},          {"verify", TOKEN_KVERIFY}};

static TokenKind ident_kind_from(Str name, size_t from) {
  if (from >= sizeof keywords / sizeof keywords[0]) return token_ident(name);
  if (str_eq(name, str_lit(keywords[from].word))) return token_kind(keywords[from].tag);
  return ident_kind_from(name, from + 1);
}

static TokenKind ident_kind(Str name) { return ident_kind_from(name, 0); }

/* normalize_digits(digits): drop leading zeros, the empty remainder is "0". */
static Str normalize_digits(Str digits) {
  if (digits.len == 0) return str_lit("0");
  if (at(digits, 0) == '0') return normalize_digits(rest(digits, 1));
  return digits;
}

/* dot_kind(digits): "1" -> Some{Dot1}, "2" -> Some{Dot2}, else None */
static MaybeTokenKind dot_kind(Str digits) {
  MaybeTokenKind kind;
  bool one = str_eq(digits, str_lit("1"));
  kind.some = one || str_eq(digits, str_lit("2"));
  kind.value = token_kind(one ? TOKEN_DOT1 : TOKEN_DOT2);
  return kind;
}

/* finish_dot(kind, digits, source, loc, start, tokens) */
static State finish_dot(Heap *heap, MaybeTokenKind kind, Str digits, Str source, TokenLoc loc, TokenLoc start,
                        Vec tokens) {
  if (kind.some) return emit(heap, kind.value, source, loc, start, tokens);
  State dot = emit(heap, token_kind(TOKEN_DOT), source, loc, start, tokens);
  return emit(heap, token_nat_tok(normalize_digits(digits)), source, loc, token_advance(heap, start, 1), dot.tokens);
}

/* finish_number(mode, text, source, loc, start, tokens): text is already in source order. */
static State finish_number(Heap *heap, Mode mode, Str text, Str source, TokenLoc loc, TokenLoc start, Vec tokens) {
  if (mode.kind == MODE_DOT_NUMBER) return finish_dot(heap, dot_kind(text), text, source, loc, start, tokens);
  return emit(heap, token_nat_tok(normalize_digits(text)), source, loc, start, tokens);
}

/* hex_digit(c) */
static MaybeU32 hex_digit(uint32_t c) {
  MaybeU32 digit;
  bool lower = c >= 97 && c <= 102;
  bool upper = c >= 65 && c <= 70;
  digit.some = is_digit(c) || lower || upper;
  digit.value = is_digit(c) ? c - 48 : lower ? c - 87 : upper ? c - 55 : 0;
  return digit;
}

/* hex_step(mode, digit, source, loc, start, tokens, bytes) */
static State hex_step(Heap *heap, Mode mode, MaybeU32 digit, Str source, TokenLoc loc, TokenLoc start, Vec tokens,
                      Vec bytes) {
  ModeKind kind = mode.kind;
  if (!digit.some) return fail(heap, loc, str_lit("byte escape requires two hexadecimal digits"));
  switch (kind) {
  case MODE_HEX_FIRST:
    return running(mode_hex_second(digit.value), source, loc, start, tokens, strbuf_new(heap->arena), bytes);
  case MODE_HEX_SECOND:
    return running(mode_of(MODE_BYTE_STRING), source, token_advance(heap, loc, 4), start, tokens,
                   strbuf_new(heap->arena), push_byte(heap, bytes, mode.high * 16 + digit.value));
  case MODE_SCAN:
  case MODE_WORD:
  case MODE_NUMBER:
  case MODE_DOT_NUMBER:
  case MODE_COMMENT:
  case MODE_BYTE_STRING:
  case MODE_ESCAPE:
    return fail(heap, loc, str_lit("invalid scanner state"));
  }
  return fail(heap, loc, str_lit("invalid scanner state"));
}

/* The single byte tokens of scan: ':' '(' ')' '*' ',' '|' '[' ']'. */
static MaybeTokenKind single_kind(uint32_t c) {
  MaybeTokenKind kind;
  TokenKindTag tag = c == ':'   ? TOKEN_COLON
                     : c == '(' ? TOKEN_LPAREN
                     : c == ')' ? TOKEN_RPAREN
                     : c == '*' ? TOKEN_STAR
                     : c == ',' ? TOKEN_COMMA
                     : c == '|' ? TOKEN_PIPE
                     : c == '[' ? TOKEN_LBRACKET
                                : TOKEN_RBRACKET;
  kind.some = c == ':' || c == '(' || c == ')' || c == '*' || c == ',' || c == '|' || c == '[' || c == ']';
  kind.value = token_kind(tag);
  return kind;
}

/* The two byte tokens of scan: "->" "=>" ":=" "()". */
static MaybeTokenKind pair_kind(uint32_t c, uint32_t d) {
  MaybeTokenKind kind;
  bool arrow = c == '-' && d == '>';
  bool darrow = c == '=' && d == '>';
  bool colon_eq = c == ':' && d == '=';
  bool unit = c == '(' && d == ')';
  TokenKindTag tag = arrow ? TOKEN_ARROW : darrow ? TOKEN_DARROW : colon_eq ? TOKEN_COLON_EQ : TOKEN_UNIT_TOK;
  kind.some = arrow || darrow || colon_eq || unit;
  kind.value = token_kind(tag);
  return kind;
}

/* scan(source, loc, tokens) */
static State scan(Heap *heap, Str source, TokenLoc loc, Vec tokens) {
  uint32_t c = at(source, 0);
  uint32_t d = at(source, 1);
  bool two = source.len >= 2;
  MaybeTokenKind pair = pair_kind(c, d);
  MaybeTokenKind single = single_kind(c);
  if (source.len == 0) {
    State done = emit(heap, token_kind(TOKEN_EOF), source, loc, loc, tokens);
    done.kind = STATE_FINISHED;
    return done;
  }
  if (two && c == 'b' && d == '"')
    return running_fresh(heap, mode_of(MODE_BYTE_STRING), rest(source, 2), token_advance(heap, loc, 2), loc, tokens);
  if (c == ' ' || c == '\t' || c == '\r')
    return running_fresh(heap, mode_of(MODE_SCAN), rest(source, 1), token_advance(heap, loc, 1), loc, tokens);
  if (c == '\n') return running_fresh(heap, mode_of(MODE_SCAN), rest(source, 1), token_next_line(heap, loc), loc, tokens);
  if (two && c == '-' && d == '-')
    return running_fresh(heap, mode_of(MODE_COMMENT), rest(source, 2), token_advance(heap, loc, 2), loc, tokens);
  if (two && pair.some) return emit(heap, pair.value, rest(source, 2), token_advance(heap, loc, 2), loc, tokens);
  if (single.some) return emit(heap, single.value, rest(source, 1), token_advance(heap, loc, 1), loc, tokens);
  if (c == '.')
    return running_fresh(heap, mode_of(MODE_DOT_NUMBER), rest(source, 1), token_advance(heap, loc, 1), loc, tokens);
  if (is_digit(c))
    return running(mode_of(MODE_NUMBER), rest(source, 1), token_advance(heap, loc, 1), loc, tokens, text_of(heap, c),
                   vec_new(heap->arena));
  if (is_ident_start(c))
    return running(mode_of(MODE_WORD), rest(source, 1), token_advance(heap, loc, 1), loc, tokens, text_of(heap, c),
                   vec_new(heap->arena));
  return fail(heap, loc, SURFACE_CAT(heap, str_lit("unexpected character '"), char_text(heap, c), str_lit("'")));
}

/* The simple escapes of advance_state: \n \r \t \0 \\ \" */
static MaybeU32 escape_byte(uint32_t c) {
  MaybeU32 byte;
  byte.some = c == 'n' || c == 'r' || c == 't' || c == '0' || c == '\\' || c == '"';
  byte.value = c == 'n'    ? 10
               : c == 'r'  ? 13
               : c == 't'  ? 9
               : c == '0'  ? 0
               : c == '\\' ? 92
                           : 34;
  return byte;
}

/* advance_state(mode, source, loc, start, tokens, text, bytes) */
static State advance_state(Heap *heap, Mode mode, Str source, TokenLoc loc, TokenLoc start, Vec tokens, StrBuf text,
                           Vec bytes) {
  ModeKind kind = mode.kind;
  uint32_t c = at(source, 0);
  bool empty = source.len == 0;
  Str tail = rest(source, 1);
  MaybeU32 escape = escape_byte(c);
  switch (kind) {
  case MODE_SCAN:
    return scan(heap, source, loc, tokens);
  case MODE_WORD:
    if (!empty && is_ident_char(c))
      return running(mode, tail, token_advance(heap, loc, 1), start, tokens, push_char(heap, text, c),
                     vec_new(heap->arena));
    return emit(heap, ident_kind(strbuf_str(&text)), source, loc, start, tokens);
  case MODE_NUMBER:
    if (!empty && is_digit(c))
      return running(mode, tail, token_advance(heap, loc, 1), start, tokens, push_char(heap, text, c),
                     vec_new(heap->arena));
    return finish_number(heap, mode, strbuf_str(&text), source, loc, start, tokens);
  case MODE_DOT_NUMBER:
    if (!empty && is_digit(c))
      return running(mode, tail, token_advance(heap, loc, 1), start, tokens, push_char(heap, text, c),
                     vec_new(heap->arena));
    if (strbuf_str(&text).len == 0) return emit(heap, token_kind(TOKEN_DOT), source, loc, start, tokens);
    return finish_number(heap, mode, strbuf_str(&text), source, loc, start, tokens);
  case MODE_COMMENT:
    if (empty) return scan(heap, source, loc, tokens);
    if (c == '\n') return running_fresh(heap, mode_of(MODE_SCAN), tail, token_next_line(heap, loc), start, tokens);
    return running_fresh(heap, mode, tail, token_advance(heap, loc, 1), start, tokens);
  case MODE_BYTE_STRING:
    if (empty) return fail(heap, loc, str_lit("unterminated byte literal"));
    if (c == '"') return emit(heap, token_bytes(bytes), tail, token_advance(heap, loc, 1), start, tokens);
    if (c == '\n' || c == '\r') return fail(heap, loc, str_lit("unescaped newline in byte literal"));
    if (c == '\\') return running(mode_of(MODE_ESCAPE), tail, loc, start, tokens, strbuf_new(heap->arena), bytes);
    return running(mode, tail, token_advance(heap, loc, 1), start, tokens, strbuf_new(heap->arena),
                   push_byte(heap, bytes, c));
  case MODE_ESCAPE:
    if (empty) return fail(heap, loc, str_lit("unterminated byte escape"));
    if (c == 'x' && source.len >= 3)
      return running(mode_of(MODE_HEX_FIRST), tail, loc, start, tokens, strbuf_new(heap->arena), bytes);
    if (c == 'x') return fail(heap, loc, str_lit("invalid byte escape \\x"));
    if (escape.some)
      return running(mode_of(MODE_BYTE_STRING), tail, token_advance(heap, loc, 2), start, tokens,
                     strbuf_new(heap->arena), push_byte(heap, bytes, escape.value));
    return fail(heap, loc, SURFACE_CAT(heap, str_lit("invalid byte escape \\"), char_text(heap, c)));
  case MODE_HEX_FIRST:
  case MODE_HEX_SECOND:
    if (empty) return fail(heap, loc, str_lit("invalid byte escape \\x"));
    return hex_step(heap, mode, hex_digit(c), tail, loc, start, tokens, bytes);
  }
  return fail(heap, loc, str_lit("invalid scanner state"));
}

/* step(state) */
static State step(Heap *heap, State state) {
  if (state.kind == STATE_RUNNING)
    return advance_state(heap, state.mode, state.source, state.loc, state.start, state.tokens, state.text,
                         state.bytes);
  return state;
}

/* run(state): the Bend tail recursion as a loop; every step consumes a byte or emits, so it ends. */
static State run(Heap *heap, State state) {
  while (state.kind == STATE_RUNNING) state = step(heap, state);
  return state;
}

/* lex(source): Done value is a Vec * of Token pointers (Eof last); Fail carries Error.Parse. */
Result lexer_lex(Heap *heap, Str source) {
  TokenLoc origin = token_start(heap);
  State state = run(heap, running_fresh(heap, mode_of(MODE_SCAN), source, origin, origin, vec_new(heap->arena)));
  Vec *tokens = NULL;
  if (state.kind == STATE_FAILED) return budget_result_fail(state.error);
  tokens = heap_alloc(heap, sizeof *tokens);
  if (tokens == NULL) return budget_result_fail(error_host_oom());
  *tokens = state.tokens;
  return budget_result_done(tokens);
}
