/* Port of bend2/kernel/error.bend. */
#include "kernel.h"

Error error_make(ErrorKind kind, Str message) {
  Error e;
  e.kind = kind;
  e.message = message;
  e.line = nat_zero();
  e.column = nat_zero();
  return e;
}

Error error_parse(Str message, Nat line, Nat column) {
  Error e = error_make(ERROR_PARSE, message);
  e.line = line;
  e.column = column;
  return e;
}

Error error_host_oom(void) { return error_make(ERROR_NOT_YET, str_lit("host out of memory")); }

/* def termination_msg(name: String) -> String */
Str error_termination_msg(Heap *heap, Str name) {
  Str head = heap_str_cat(heap, str_lit("recursive definition "), name);
  return heap_str_cat(heap, head, str_lit(" failed the structural termination guard"));
}

/* def message(e: T) -> String */
Str error_message(Heap *heap, Error e) {
  switch (e.kind) {
    case ERROR_PARSE:
    case ERROR_NOT_YET:
    case ERROR_CARRY:
    case ERROR_UNBOUND:
    case ERROR_MISMATCH:
    case ERROR_UNIVERSE:
    case ERROR_QUANTITY:
    case ERROR_WRONG_LEG:
    case ERROR_MISSING_BRANCH:
    case ERROR_OVERFLOW:
    case ERROR_CANNOT_INFER:
    case ERROR_BUDGET_EXHAUSTED:
    case ERROR_INDEX_NOT_ZERO:
    case ERROR_INDEX_ABOVE_UNIVERSE:
      return e.message;
    case ERROR_TERMINATION:
      return error_termination_msg(heap, e.message);
  }
  return e.message;
}

static Str parse_to_string(Heap *heap, Error e) {
  StrBuf buf = strbuf_new(heap->arena);
  bool ok = strbuf_push(&buf, str_lit("line ")) && strbuf_push(&buf, heap_nat_show(heap, e.line)) &&
            strbuf_push(&buf, str_lit(", column ")) && strbuf_push(&buf, heap_nat_show(heap, e.column)) &&
            strbuf_push(&buf, str_lit(": ")) && strbuf_push(&buf, e.message);
  if (!ok) heap->oom = true;
  return strbuf_str(&buf);
}

/* def to_string(e: T) -> String */
Str error_to_string(Heap *heap, Error e) {
  switch (e.kind) {
    case ERROR_PARSE:
      return parse_to_string(heap, e);
    case ERROR_NOT_YET:
      return heap_str_cat(heap, str_lit("not yet: "), e.message);
    case ERROR_CARRY:
      return heap_str_cat(heap, str_lit("carry: "), e.message);
    case ERROR_UNBOUND:
      return heap_str_cat(heap, str_lit("unbound: "), e.message);
    case ERROR_MISMATCH:
      return heap_str_cat(heap, str_lit("mismatch: "), e.message);
    case ERROR_UNIVERSE:
      return heap_str_cat(heap, str_lit("universe: "), e.message);
    case ERROR_QUANTITY:
      return heap_str_cat(heap, str_lit("quantity: "), e.message);
    case ERROR_WRONG_LEG:
      return heap_str_cat(heap, str_lit("wrong leg: "), e.message);
    case ERROR_MISSING_BRANCH:
      return heap_str_cat(heap, str_lit("missing branch: "), e.message);
    case ERROR_OVERFLOW:
      return heap_str_cat(heap, str_lit("overflow: "), e.message);
    case ERROR_CANNOT_INFER:
      return heap_str_cat(heap, str_lit("cannot infer: "), e.message);
    case ERROR_BUDGET_EXHAUSTED:
      return heap_str_cat(heap, str_lit("budget: "), e.message);
    case ERROR_INDEX_NOT_ZERO:
      return heap_str_cat(heap, str_lit("index not zero: "), e.message);
    case ERROR_INDEX_ABOVE_UNIVERSE:
      return heap_str_cat(heap, str_lit("index above universe: "), e.message);
    case ERROR_TERMINATION:
      return heap_str_cat(heap, str_lit("termination: "), error_termination_msg(heap, e.message));
  }
  return e.message;
}
