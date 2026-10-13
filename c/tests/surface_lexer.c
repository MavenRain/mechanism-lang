/* Port of bend2/tests/surface_lexer.bend (oracle label unit-surface-lexer): the token stream text of
 * nine sources covers keywords, byte literals and escapes, a long comment and dotted numbers. */
#include "driver.h"
#include "surface.h"

/* bytes_text(bytes): U32.show(byte) ++ "," per byte, from index FROM. */
static Str bytes_text(Heap *heap, const Vec *bytes, size_t from) {
  VecGet got = vec_get(bytes, from);
  if (got.status != VEC_GET_OK) return str_lit("");
  return SURFACE_CAT(heap, str_of_u64(heap->arena, *(const uint32_t *)got.value), str_lit(","),
                     bytes_text(heap, bytes, from + 1));
}

/* kind_text(kind) */
static Str kind_text(Heap *heap, TokenKind kind) {
  if (kind.tag == TOKEN_BYTES) return SURFACE_CAT(heap, str_lit("bytes["), bytes_text(heap, &kind.bytes, 0), str_lit("]"));
  return token_describe(heap, kind);
}

/* token_text(token) */
static Str token_text(Heap *heap, const Token *token) {
  return SURFACE_CAT(heap, kind_text(heap, token->kind), str_lit("@"), heap_nat_show(heap, token->loc.line),
                     str_lit(":"), heap_nat_show(heap, token->loc.col), str_lit(";"));
}

/* tokens_text(tokens), from index FROM. */
static Str tokens_text(Heap *heap, const Vec *tokens, size_t from) {
  VecGet got = vec_get(tokens, from);
  if (got.status != VEC_GET_OK) return str_lit("");
  return SURFACE_CAT(heap, token_text(heap, (const Token *)got.value), tokens_text(heap, tokens, from + 1));
}

/* result_text(result) */
static Str result_text(Heap *heap, Result result) {
  if (result.kind == RESULT_DONE) return tokens_text(heap, (const Vec *)result.value, 0);
  if (result.error.kind == ERROR_PARSE)
    return SURFACE_CAT(heap, result.error.message, str_lit("@"), heap_nat_show(heap, result.error.line), str_lit(":"),
                       heap_nat_show(heap, result.error.column));
  return str_lit("unexpected error");
}

/* check(source, expected) */
static bool check(Heap *heap, Str source, const char *expected) {
  return str_eq(result_text(heap, lexer_lex(heap, source)), str_lit(expected));
}

/* padding(length, acc): LENGTH x characters in front of ACC. */
static Str padding(Heap *heap, size_t length, Str acc) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t i = 0;
  for (; i < length; i = i + 1) {
    if (!strbuf_push_byte(&buf, 'x')) heap->oom = true;
  }
  if (!strbuf_push(&buf, acc)) heap->oom = true;
  return strbuf_str(&buf);
}

/* main() */
static bool suite(Heap *heap) {
  return check(heap, SURFACE_CAT(heap, str_lit("--"), padding(heap, 90000, str_lit("\nb\"\303\251\360\237\230\200\""))),
               "bytes[195,169,240,159,152,128,]@2:1;end of input@2:10;") &&
         check(heap, str_lit("def x := 123456789012345678901234567890 -- hi\n x.1.2.10"),
               "'def'@1:1;identifier x@1:5;':='@1:7;number 123456789012345678901234567890@1:10;identifier "
               "x@2:2;'.1'@2:3;'.2'@2:5;'.'@2:7;number 10@2:8;end of input@2:10;") &&
         check(heap, str_lit("b\"\303\251\\x00\\n\""), "bytes[195,169,0,10,]@1:1;end of input@1:12;") &&
         check(heap, str_lit("b\"\\xg0\""), "byte escape requires two hexadecimal digits@1:3") &&
         check(heap, str_lit("b\"x\n"), "unescaped newline in byte literal@1:4") &&
         check(heap, str_lit("b\"abc"), "unterminated byte literal@1:6") &&
         check(heap, str_lit("000.01"), "number 0@1:1;'.'@1:4;number 1@1:5;end of input@1:7;") &&
         check(heap, str_lit("b\"\\x\""), "invalid byte escape \\x@1:3") &&
         check(heap, str_lit("nu mpc[P, A] T () -> =>"),
               "'nu'@1:1;'mpc'@1:4;'['@1:7;identifier P@1:8;','@1:9;identifier A@1:11;']'@1:12;identifier "
               "T@1:14;'()'@1:16;'->'@1:19;'=>'@1:22;end of input@1:24;");
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-lexer")) : driver_die(1, "FAIL surface-lexer");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
