/* Port of bend2/kernel/literal.bend: string and integer literals. */
#include "kernel.h"

Literal literal_lstring(Str value) {
  Literal lit;
  lit.kind = LITERAL_STRING;
  lit.string = value;
  lit.integer = big_zero();
  return lit;
}

Literal literal_lint(Big value) {
  Literal lit;
  lit.kind = LITERAL_INT;
  lit.string = str_empty();
  lit.integer = value;
  return lit;
}

static bool equal_string(Str x, Literal b) {
  switch (b.kind) {
  case LITERAL_STRING:
    return str_eq(x, b.string);
  case LITERAL_INT:
    return false;
  }
  return false;
}

static bool equal_int(Big x, Literal b) {
  switch (b.kind) {
  case LITERAL_STRING:
    return false;
  case LITERAL_INT:
    return big_equal(x, b.integer);
  }
  return false;
}

bool literal_equal(Literal a, Literal b) {
  switch (a.kind) {
  case LITERAL_STRING:
    return equal_string(a.string, b);
  case LITERAL_INT:
    return equal_int(a.integer, b);
  }
  return false;
}
