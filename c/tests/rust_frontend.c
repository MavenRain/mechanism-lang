/* C port of tests/rust_frontend.bend. Driver: `rust_frontend <lex|parse|print|check|pos>
 * <source text>`. `check` is the full frontend: lex, fragment pass, parse, print; a refused source
 * gives one `REFUSED <line>:<col> <construct>` line per refusal. Each report is built as one Str
 * and written once (the Bend IO.write). A failed allocation sets heap->oom; the driver then exits
 * with driver_host_failure. */
#include "driver.h"
#include "rust.h"

/* C only: the concatenation of `count` strings onto `buf`. */
static void rf_push(Heap *heap, StrBuf *buf, const Str *parts, size_t count) {
  size_t at = 0;
  while (at < count) {
    if (!strbuf_push(buf, parts[at])) heap->oom = true;
    at = at + 1;
  }
}

/* C only: `prefix ++ msg ++ "\n"`. */
static Str rf_line(Heap *heap, const char *prefix, Str msg) {
  StrBuf buf = strbuf_new(heap->arena);
  Str parts[3] = {str_lit(prefix), msg, str_lit("\n")};
  rf_push(heap, &buf, parts, 3);
  return strbuf_str(&buf);
}

/* show_tokens(ts) */
static Str rf_show_tokens(Heap *heap, const Vec *ts) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t at = 0;
  while (at < ts->len && !heap->oom) {
    VecGet got = vec_get(ts, at);
    if (got.status == VEC_GET_OK) {
      Str parts[2] = {rust_token_show(heap, got.value), str_lit("\n")};
      rf_push(heap, &buf, parts, 2);
    }
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* lex_report(r) */
static Str rf_lex_report(Heap *heap, const RustLexResult *r) {
  if (r->ok) return rf_show_tokens(heap, &r->tokens);
  return rf_line(heap, "LEX-FAIL ", r->message);
}

/* item_lines(ns) when `pos` is false, else pos_lines(ns): one line per item. */
static Str rf_item_lines(Heap *heap, const Vec *ns, bool pos) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t at = 0;
  while (at < ns->len && !heap->oom) {
    VecGet got = vec_get(ns, at);
    if (got.status == VEC_GET_OK) {
      const AstNode *n = got.value;
      if (pos) {
        Str head[2] = {ast_show_pos(heap, ast_item_pos(heap, n)), str_lit(" ")};
        rf_push(heap, &buf, head, 2);
      }
      Str parts[2] = {ast_item_line(heap, n), str_lit("\n")};
      rf_push(heap, &buf, parts, 2);
    }
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* parse_report(r) */
static Str rf_parse_report(Heap *heap, const RustSrcFileResult *r) {
  if (!r->ok) return rf_line(heap, "PARSE-FAIL ", r->message);
  StrBuf buf = strbuf_new(heap->arena);
  Str parts[2] = {str_lit("PARSE-OK\n"), rf_item_lines(heap, &r->file.items, false)};
  rf_push(heap, &buf, parts, 2);
  return strbuf_str(&buf);
}

/* pos_report(r) */
static Str rf_pos_report(Heap *heap, const RustSrcFileResult *r) {
  if (!r->ok) return rf_line(heap, "PARSE-FAIL ", r->message);
  return rf_item_lines(heap, &r->file.items, true);
}

/* print_report(r) */
static Str rf_print_report(Heap *heap, const RustSrcFileResult *r) {
  if (!r->ok) return rf_line(heap, "PARSE-FAIL ", r->message);
  return print_file(heap, &r->file);
}

/* refused_lines(rs) */
static Str rf_refused_lines(Heap *heap, const Vec *rs) {
  StrBuf buf = strbuf_new(heap->arena);
  size_t at = 0;
  while (at < rs->len && !heap->oom) {
    VecGet got = vec_get(rs, at);
    if (got.status == VEC_GET_OK) {
      Str parts[3] = {str_lit("REFUSED "), fragment_show(heap, got.value), str_lit("\n")};
      rf_push(heap, &buf, parts, 3);
    }
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* check_tokens(rs, ts) */
static Str rf_check_tokens(Heap *heap, const Vec *rs, const Vec *ts) {
  if (rs->len > 0) return rf_refused_lines(heap, rs);
  RustSrcFileResult r = rust_parser_from_tokens(heap, ts);
  return rf_print_report(heap, &r);
}

/* check_report(r) */
static Str rf_check_report(Heap *heap, const RustLexResult *r) {
  if (!r->ok) return rf_line(heap, "LEX-FAIL ", r->message);
  Vec rs = fragment_refusals(heap, &r->tokens);
  return rf_check_tokens(heap, &rs, &r->tokens);
}

/* Md: the mode tag. */
typedef enum { RF_MLEX, RF_MPARSE, RF_MPRINT, RF_MCHECK, RF_MPOS, RF_MNONE } RfMd;

/* mode_of(mode) */
static RfMd rf_mode_of(Str mode) {
  if (str_eq(mode, str_lit("lex"))) return RF_MLEX;
  if (str_eq(mode, str_lit("parse"))) return RF_MPARSE;
  if (str_eq(mode, str_lit("print"))) return RF_MPRINT;
  if (str_eq(mode, str_lit("check"))) return RF_MCHECK;
  if (str_eq(mode, str_lit("pos"))) return RF_MPOS;
  return RF_MNONE;
}

/* run_mode(m, src) */
static Str rf_run_mode(Heap *heap, RfMd m, Str src) {
  Str out = str_empty();
  RustLexResult lexed;
  RustSrcFileResult parsed;
  switch (m) {
    case RF_MLEX:
      lexed = rust_lexer_lex(heap, src);
      out = rf_lex_report(heap, &lexed);
      break;
    case RF_MPARSE:
      parsed = rust_parser_file(heap, src);
      out = rf_parse_report(heap, &parsed);
      break;
    case RF_MPRINT:
      parsed = rust_parser_file(heap, src);
      out = rf_print_report(heap, &parsed);
      break;
    case RF_MCHECK:
      lexed = rust_lexer_lex(heap, src);
      out = rf_check_report(heap, &lexed);
      break;
    case RF_MPOS:
      parsed = rust_parser_file(heap, src);
      out = rf_pos_report(heap, &parsed);
      break;
    case RF_MNONE: out = str_lit("unknown mode\n"); break;
  }
  return out;
}

/* run_args(args) */
static Str rf_run_args(Heap *heap, int argc, char **argv) {
  if (argc < 3) return str_lit("usage: rust_frontend.bend <lex|parse|print|check|pos> <source text>\n");
  return rf_run_mode(heap, rf_mode_of(str_lit(argv[1])), str_lit(argv[2]));
}

/* main: IO.write(run_args(args)). */
static int rf_entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Str text = rf_run_args(&heap, argc, argv);
  int code = 0;
  if (heap.oom) code = driver_host_failure();
  if (!heap.oom && text.len > 0) fwrite(text.data, 1, text.len, stdout);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, rf_entry); }
