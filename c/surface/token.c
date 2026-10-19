/* Port of surface/token.bend: source locations, token kinds, their descriptions and tags.
 * Loc columns are Nats (Error.Parse carries them), advanced by the byte counts the lexer passes. */
#include "surface.h"

static Vec no_bytes(void) {
  Vec bytes;
  bytes.arena = NULL;
  bytes.items = NULL;
  bytes.len = 0;
  bytes.cap = 0;
  return bytes;
}

/* The payload free constructors LParen{} .. KNatLt{} and Eof{}. */
TokenKind token_kind(TokenKindTag tag) {
  TokenKind kind;
  kind.tag = tag;
  kind.name = str_lit("");
  kind.digits = str_lit("");
  kind.bytes = no_bytes();
  return kind;
}

/* Ident{name} */
TokenKind token_ident(Str name) {
  TokenKind kind = token_kind(TOKEN_IDENT);
  kind.name = name;
  return kind;
}

/* NatTok{digits} */
TokenKind token_nat_tok(Str digits) {
  TokenKind kind = token_kind(TOKEN_NAT_TOK);
  kind.digits = digits;
  return kind;
}

/* Bytes{bytes} */
TokenKind token_bytes(Vec bytes) {
  TokenKind kind = token_kind(TOKEN_BYTES);
  kind.bytes = bytes;
  return kind;
}

/* start(): Loc{1n, 1n} */
TokenLoc token_start(Heap *heap) {
  TokenLoc loc;
  loc.line = heap_nat_of_u64(heap, 1);
  loc.col = heap_nat_of_u64(heap, 1);
  return loc;
}

/* advance(l, n): Loc{line, Nat.add(col, n)} */
TokenLoc token_advance(Heap *heap, TokenLoc l, uint64_t n) {
  TokenLoc loc;
  loc.line = l.line;
  loc.col = heap_nat_add(heap, l.col, heap_nat_of_u64(heap, n));
  return loc;
}

/* next_line(l): Loc{1n + line, 1n} */
TokenLoc token_next_line(Heap *heap, TokenLoc l) {
  TokenLoc loc;
  loc.line = heap_nat_succ(heap, l.line);
  loc.col = heap_nat_of_u64(heap, 1);
  return loc;
}

/* describe(k) */
Str token_describe(Heap *heap, TokenKind k) {
  TokenKindTag kind = k.tag;
  switch (kind) {
  case TOKEN_LPAREN: return str_lit("'('");
  case TOKEN_RPAREN: return str_lit("')'");
  case TOKEN_COLON: return str_lit("':'");
  case TOKEN_COLON_EQ: return str_lit("':='");
  case TOKEN_ARROW: return str_lit("'->'");
  case TOKEN_DARROW: return str_lit("'=>'");
  case TOKEN_STAR: return str_lit("'*'");
  case TOKEN_COMMA: return str_lit("','");
  case TOKEN_DOT: return str_lit("'.'");
  case TOKEN_DOT1: return str_lit("'.1'");
  case TOKEN_DOT2: return str_lit("'.2'");
  case TOKEN_PIPE: return str_lit("'|'");
  case TOKEN_UNIT_TOK: return str_lit("'()'");
  case TOKEN_KDEF: return str_lit("'def'");
  case TOKEN_KAXIOM: return str_lit("'axiom'");
  case TOKEN_KFUN: return str_lit("'fun'");
  case TOKEN_KINJ: return str_lit("'inj'");
  case TOKEN_KOF: return str_lit("'of'");
  case TOKEN_KCASE: return str_lit("'case'");
  case TOKEN_KMATCH: return str_lit("'match'");
  case TOKEN_KAS: return str_lit("'as'");
  case TOKEN_KRETURN: return str_lit("'return'");
  case TOKEN_KWITH: return str_lit("'with'");
  case TOKEN_KTUPLE: return str_lit("'tuple'");
  case TOKEN_KSUM: return str_lit("'sum'");
  case TOKEN_KPROD: return str_lit("'prod'");
  case TOKEN_KABSURD: return str_lit("'absurd'");
  case TOKEN_KPROP: return str_lit("'Prop'");
  case TOKEN_KTYPE: return str_lit("'Type'");
  case TOKEN_KSORT: return str_lit("'Sort'");
  case TOKEN_KPOLY: return str_lit("'poly'");
  case TOKEN_KGROUP: return str_lit("'group'");
  case TOKEN_KSPECIALIZE: return str_lit("'specialize'");
  case TOKEN_KWHERE: return str_lit("'where'");
  case TOKEN_KLET: return str_lit("'let'");
  case TOKEN_KIN: return str_lit("'in'");
  case TOKEN_KAUTO: return str_lit("'auto'");
  case TOKEN_KMU: return str_lit("'mu'");
  case TOKEN_KMUTUAL: return str_lit("'mutual'");
  case TOKEN_KEND: return str_lit("'end'");
  case TOKEN_KNU: return str_lit("'nu'");
  case TOKEN_KZK: return str_lit("'zk'");
  case TOKEN_KPROVE: return str_lit("'prove'");
  case TOKEN_KVERIFY: return str_lit("'verify'");
  case TOKEN_KFHC: return str_lit("'fhc'");
  case TOKEN_KENC: return str_lit("'enc'");
  case TOKEN_KEVAL: return str_lit("'eval'");
  case TOKEN_KDEC: return str_lit("'dec'");
  case TOKEN_KMPC: return str_lit("'mpc'");
  case TOKEN_KINPUT: return str_lit("'input'");
  case TOKEN_KSHARE: return str_lit("'share'");
  case TOKEN_KOPEN: return str_lit("'open'");
  case TOKEN_LBRACKET: return str_lit("'['");
  case TOKEN_RBRACKET: return str_lit("']'");
  case TOKEN_KAND: return str_lit("'and'");
  case TOKEN_KREC: return str_lit("'rec'");
  case TOKEN_KNAT_ADD: return str_lit("'natAdd'");
  case TOKEN_KNAT_SUB: return str_lit("'natSub'");
  case TOKEN_KNAT_MUL: return str_lit("'natMul'");
  case TOKEN_KNAT_EQ: return str_lit("'natEq'");
  case TOKEN_KNAT_LT: return str_lit("'natLt'");
  case TOKEN_IDENT: return SURFACE_CAT(heap, str_lit("identifier "), k.name);
  case TOKEN_NAT_TOK: return SURFACE_CAT(heap, str_lit("number "), k.digits);
  case TOKEN_BYTES: return str_lit("byte literal");
  case TOKEN_EOF: return str_lit("end of input");
  }
  return str_lit("end of input");
}

/* tag(k) */
uint32_t token_tag(TokenKind k) {
  TokenKindTag kind = k.tag;
  switch (kind) {
  case TOKEN_LPAREN: return 0;
  case TOKEN_RPAREN: return 1;
  case TOKEN_COLON: return 2;
  case TOKEN_COLON_EQ: return 3;
  case TOKEN_ARROW: return 4;
  case TOKEN_DARROW: return 5;
  case TOKEN_STAR: return 6;
  case TOKEN_COMMA: return 7;
  case TOKEN_DOT: return 8;
  case TOKEN_DOT1: return 9;
  case TOKEN_DOT2: return 10;
  case TOKEN_PIPE: return 11;
  case TOKEN_UNIT_TOK: return 12;
  case TOKEN_KDEF: return 13;
  case TOKEN_KAXIOM: return 14;
  case TOKEN_KFUN: return 15;
  case TOKEN_KINJ: return 16;
  case TOKEN_KOF: return 17;
  case TOKEN_KCASE: return 18;
  case TOKEN_KMATCH: return 19;
  case TOKEN_KAS: return 20;
  case TOKEN_KRETURN: return 21;
  case TOKEN_KWITH: return 22;
  case TOKEN_KTUPLE: return 23;
  case TOKEN_KSUM: return 24;
  case TOKEN_KPROD: return 25;
  case TOKEN_KABSURD: return 26;
  case TOKEN_KPROP: return 27;
  case TOKEN_KTYPE: return 28;
  case TOKEN_KSORT: return 29;
  case TOKEN_KPOLY: return 30;
  case TOKEN_KGROUP: return 31;
  case TOKEN_KSPECIALIZE: return 32;
  case TOKEN_KWHERE: return 33;
  case TOKEN_KLET: return 34;
  case TOKEN_KIN: return 35;
  case TOKEN_KAUTO: return 36;
  case TOKEN_KMU: return 37;
  case TOKEN_KMUTUAL: return 38;
  case TOKEN_KEND: return 39;
  case TOKEN_KNU: return 40;
  case TOKEN_KZK: return 41;
  case TOKEN_KPROVE: return 42;
  case TOKEN_KVERIFY: return 43;
  case TOKEN_KFHC: return 44;
  case TOKEN_KENC: return 45;
  case TOKEN_KEVAL: return 46;
  case TOKEN_KDEC: return 47;
  case TOKEN_KMPC: return 48;
  case TOKEN_KINPUT: return 49;
  case TOKEN_KSHARE: return 50;
  case TOKEN_KOPEN: return 51;
  case TOKEN_LBRACKET: return 52;
  case TOKEN_RBRACKET: return 53;
  case TOKEN_KAND: return 54;
  case TOKEN_KREC: return 55;
  case TOKEN_KNAT_ADD: return 56;
  case TOKEN_KNAT_SUB: return 57;
  case TOKEN_KNAT_MUL: return 58;
  case TOKEN_KNAT_EQ: return 59;
  case TOKEN_KNAT_LT: return 60;
  case TOKEN_IDENT: return 61;
  case TOKEN_NAT_TOK: return 62;
  case TOKEN_BYTES: return 63;
  case TOKEN_EOF: return 64;
  }
  return 64;
}
