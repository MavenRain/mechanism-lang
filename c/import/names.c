/* Port of import/names.bend: the name entries of a lean4export NDJSON stream
 * (Anonymous, Str{prefix, text}, Num{prefix, index}); the indices stay decimal text. */
#include "import.h"

static NameResult name_done(Name value) {
  NameResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static Name name_blank(NameKind kind) {
  Name value;
  value.kind = kind;
  value.prefix = str_empty();
  value.text = str_empty();
  value.index = str_empty();
  return value;
}

static NameResult name_fail(Str message) {
  NameResult result;
  result.kind = JSON_FAIL;
  result.value = name_blank(NAME_ANONYMOUS);
  result.message = message;
  return result;
}

/* Str{prefix, text} */
static Name name_str(Str prefix, Str text) {
  Name value = name_blank(NAME_STR);
  value.prefix = prefix;
  value.text = text;
  return value;
}

/* Num{prefix, index} */
static Name name_num(Str prefix, Str index) {
  Name value = name_blank(NAME_NUM);
  value.prefix = prefix;
  value.index = index;
  return value;
}

/* the expected key list of exact_object: ["pre", "str"] or ["pre", "i"] */
static Vec keys(Heap *heap, const char *first, const char *second) {
  Vec expected = vec_new(heap->arena);
  Str *a = json_boxed_str(heap, str_lit(first));
  Str *b = json_boxed_str(heap, str_lit(second));
  if (a == NULL || b == NULL) return expected;
  return json_vec_with(heap, json_vec_with(heap, expected, a), b);
}

/* parse, case "str" */
static NameResult parse_str(Heap *heap, const Json *value) {
  Vec expected = keys(heap, "pre", "str");
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return name_fail(fields.message);
  JsonTextResult prefix = json_nat_field(heap, &fields.value, str_lit("pre"));
  if (prefix.kind == JSON_FAIL) return name_fail(prefix.message);
  JsonTextResult text = json_string_field(heap, &fields.value, str_lit("str"));
  if (text.kind == JSON_FAIL) return name_fail(text.message);
  return name_done(name_str(prefix.value, text.value));
}

/* parse, case "num" */
static NameResult parse_num(Heap *heap, const Json *value) {
  Vec expected = keys(heap, "pre", "i");
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return name_fail(fields.message);
  JsonTextResult prefix = json_nat_field(heap, &fields.value, str_lit("pre"));
  if (prefix.kind == JSON_FAIL) return name_fail(prefix.message);
  JsonTextResult index = json_nat_field(heap, &fields.value, str_lit("i"));
  if (index.kind == JSON_FAIL) return name_fail(index.message);
  return name_done(name_num(prefix.value, index.value));
}

/* parse(tag, value) */
NameResult name_parse(Heap *heap, Str tag, const Json *value) {
  if (str_eq(tag, str_lit("str"))) return parse_str(heap, value);
  if (str_eq(tag, str_lit("num"))) return parse_num(heap, value);
  return name_fail(heap_str_cat(heap, str_lit("unknown name tag "), tag));
}

/* [prefix] */
static Vec single(Heap *heap, Vec out, Str item) {
  Str *box = json_boxed_str(heap, item);
  if (box == NULL) return out;
  return json_vec_with(heap, out, box);
}

/* references(value): the name indices the entry refers to, as Str* items. */
Vec name_references(Heap *heap, Name value) {
  Vec out = vec_new(heap->arena);
  switch (value.kind) {
    case NAME_ANONYMOUS: return out;
    case NAME_STR: return single(heap, out, value.prefix);
    case NAME_NUM: return single(heap, out, value.prefix);
  }
  return out;
}
