/* C only: a unit check of the line and column tracking of c/rust/lexer.c (rust/lexer.bend)
 * across comments. The Bend gates do not reach it: the parser stops at the first comment token, so a
 * position after a multi-line comment shows only in the lex and check modes of rust_frontend. Each
 * case lexes a source and compares show_tokens (the `rust_frontend lex` text) with the text that the
 * Bend rules give: a newline in a line comment ends the token and starts the next line
 * (lexer.bend:174-175); a newline in a block comment of any depth starts the next line and does not
 * count toward a gap (lexer.bend:184-185). One `PASS rust_lexer_lines <name>` or `FAIL ...` line per
 * case; the exit code is 1 when a case fails. */
#include <stdio.h>
#include <string.h>

#include "driver.h"
#include "rust.h"

typedef struct {
  const char *name;
  const char *source;
  const char *expected;
} RllCase;

static const RllCase rll_cases[] = {
    {"block comment newline", "/* a\nb */\nx", "1:1 comment\n3:1 ident x\n3:2 eof\n"},
    {"nested block comment newline", "/* a /* b\n */ c\n */ y", "1:1 comment\n3:5 ident y\n3:6 eof\n"},
    {"line comment newline", "// a\nz", "1:1 comment\n2:1 ident z\n2:2 eof\n"},
    {"block comment newlines are no gap", "f(/*\n\n*/)\n\nw",
     "1:1 ident f\n1:2 punct (\n1:3 comment\n3:3 punct )\n5:1 ident w +gap\n5:2 eof\n"},
};

/* C only: `text` pushed onto `buf`; a failed push sets heap->oom. */
static void rll_push(Heap *heap, StrBuf *buf, Str text) {
  if (!strbuf_push(buf, text)) heap->oom = true;
}

/* C only: show_tokens(ts) of rust_frontend.c when the lex is ok, else `LEX-FAIL message`. */
static Str rll_show(Heap *heap, const RustLexResult *r) {
  StrBuf buf = strbuf_new(heap->arena);
  if (!r->ok) {
    rll_push(heap, &buf, str_lit("LEX-FAIL "));
    rll_push(heap, &buf, r->message);
    rll_push(heap, &buf, str_lit("\n"));
    return strbuf_str(&buf);
  }
  size_t at = 0;
  while (at < r->tokens.len && !heap->oom) {
    VecGet got = vec_get(&r->tokens, at);
    if (got.status == VEC_GET_OK) {
      rll_push(heap, &buf, rust_token_show(heap, got.value));
      rll_push(heap, &buf, str_lit("\n"));
    }
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* C only: one case; PASS, or FAIL with the expected and the lexed text. */
static bool rll_case(const RllCase *c) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  RustLexResult lexed = rust_lexer_lex(&heap, str_lit(c->source));
  Str got = rll_show(&heap, &lexed);
  Str want = str_lit(c->expected);
  bool same = !heap.oom && got.len == want.len && memcmp(got.data, want.data, want.len) == 0;
  if (same) {
    printf("PASS rust_lexer_lines %s\n", c->name);
  } else {
    printf("FAIL rust_lexer_lines %s\n--- expected\n%s--- got\n", c->name, c->expected);
    if (got.len > 0) fwrite(got.data, 1, got.len, stdout);
  }
  arena_free(&arena);
  return same;
}

/* C only: every case in order; exit code 1 when one fails. */
static int rll_entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  size_t count = sizeof rll_cases / sizeof rll_cases[0];
  size_t failed = 0;
  size_t at = 0;
  while (at < count) {
    if (!rll_case(&rll_cases[at])) failed = failed + 1;
    at = at + 1;
  }
  if (failed > 0) return 1;
  return 0;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, rll_entry); }
