/* Port of bend2/import/exprs.bend: the expression entries of a lean4export NDJSON stream
 * (Bvar, Sort, Const, Apply, Lam, Forall, Let, Proj, NatLiteral, StringLiteral, Mdata); every
 * index stays decimal text. */
#include "import.h"

/* Result<String, BinderInfo> */
typedef struct {
  JsonResultKind kind;
  ExprBinderInfo value;
  Str message;
} BinderInfoResult;

/* Result<String, Binder> */
typedef struct {
  JsonResultKind kind;
  ExprBinder value;
  Str message;
} BinderResult;

static ExprBinder binder_blank(void) {
  ExprBinder value;
  value.name = str_empty();
  value.typ = str_empty();
  value.body = str_empty();
  value.info = EXPR_BINDER_DEFAULT;
  return value;
}

static Expr expr_blank(Heap *heap, ExprKind kind) {
  Expr value;
  value.kind = kind;
  value.index = str_empty();
  value.name = str_empty();
  value.universes = vec_new(heap->arena);
  value.fn = str_empty();
  value.arg = str_empty();
  value.binder = binder_blank();
  value.typ = str_empty();
  value.value = str_empty();
  value.body = str_empty();
  value.nondep = false;
  value.type_name = str_empty();
  value.structure = str_empty();
  value.text = str_empty();
  value.expr = str_empty();
  value.data = vec_new(heap->arena);
  return value;
}

static ExprResult expr_done(Expr value) {
  ExprResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static ExprResult expr_fail(Heap *heap, Str message) {
  ExprResult result;
  result.kind = JSON_FAIL;
  result.value = expr_blank(heap, EXPR_BVAR);
  result.message = message;
  return result;
}

static BinderInfoResult info_done(ExprBinderInfo value) {
  BinderInfoResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static BinderInfoResult info_fail(Str message) {
  BinderInfoResult result;
  result.kind = JSON_FAIL;
  result.value = EXPR_BINDER_DEFAULT;
  result.message = message;
  return result;
}

static BinderResult binder_done(ExprBinder value) {
  BinderResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static BinderResult binder_fail(Str message) {
  BinderResult result;
  result.kind = JSON_FAIL;
  result.value = binder_blank();
  result.message = message;
  return result;
}

/* binder_info(text) */
static BinderInfoResult binder_info(Heap *heap, Str text) {
  if (str_eq(text, str_lit("default"))) return info_done(EXPR_BINDER_DEFAULT);
  if (str_eq(text, str_lit("implicit"))) return info_done(EXPR_BINDER_IMPLICIT);
  if (str_eq(text, str_lit("strictImplicit"))) return info_done(EXPR_BINDER_STRICT_IMPLICIT);
  if (str_eq(text, str_lit("instImplicit"))) return info_done(EXPR_BINDER_INST_IMPLICIT);
  return info_fail(heap_str_cat(heap, str_lit("invalid binderInfo "), text));
}

/* binder(value) */
static BinderResult binder(Heap *heap, const Json *value) {
  static const char *const names[] = {"name", "type", "body", "binderInfo"};
  Vec expected = json_expected_keys(heap, names, 4);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return binder_fail(fields.message);
  JsonTextResult name = json_nat_field(heap, &fields.value, str_lit("name"));
  if (name.kind == JSON_FAIL) return binder_fail(name.message);
  JsonTextResult typ = json_nat_field(heap, &fields.value, str_lit("type"));
  if (typ.kind == JSON_FAIL) return binder_fail(typ.message);
  JsonTextResult body = json_nat_field(heap, &fields.value, str_lit("body"));
  if (body.kind == JSON_FAIL) return binder_fail(body.message);
  JsonTextResult text = json_string_field(heap, &fields.value, str_lit("binderInfo"));
  if (text.kind == JSON_FAIL) return binder_fail(text.message);
  BinderInfoResult info = binder_info(heap, text.value);
  if (info.kind == JSON_FAIL) return binder_fail(info.message);
  ExprBinder out = binder_blank();
  out.name = name.value;
  out.typ = typ.value;
  out.body = body.value;
  out.info = info.value;
  return binder_done(out);
}

/* natural(valid, text) */
static ExprResult natural(Heap *heap, bool valid, Str text) {
  if (!valid) return expr_fail(heap, str_lit("invalid natural literal"));
  Expr out = expr_blank(heap, EXPR_NAT_LITERAL);
  out.text = text;
  return expr_done(out);
}

/* natural_text(text) */
static ExprResult natural_text(Heap *heap, Str text) {
  if (text.len == 0) return expr_fail(heap, str_lit("invalid natural literal"));
  return natural(heap, json_decimal_digits(text), text);
}

/* parse, cases "bvar" and "sort": Json.nat(value) */
static ExprResult parse_indexed(Heap *heap, ExprKind kind, const Json *value) {
  JsonTextResult index = json_nat(value);
  if (index.kind == JSON_FAIL) return expr_fail(heap, index.message);
  Expr out = expr_blank(heap, kind);
  out.index = index.value;
  return expr_done(out);
}

/* parse, case "const" */
static ExprResult parse_const(Heap *heap, const Json *value) {
  static const char *const names[] = {"name", "us"};
  Vec expected = json_expected_keys(heap, names, 2);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return expr_fail(heap, fields.message);
  JsonTextResult name = json_nat_field(heap, &fields.value, str_lit("name"));
  if (name.kind == JSON_FAIL) return expr_fail(heap, name.message);
  JsonListResult universes = json_nats_field(heap, &fields.value, str_lit("us"));
  if (universes.kind == JSON_FAIL) return expr_fail(heap, universes.message);
  Expr out = expr_blank(heap, EXPR_CONST);
  out.name = name.value;
  out.universes = universes.value;
  return expr_done(out);
}

/* parse, case "app" */
static ExprResult parse_app(Heap *heap, const Json *value) {
  static const char *const names[] = {"fn", "arg"};
  Vec expected = json_expected_keys(heap, names, 2);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return expr_fail(heap, fields.message);
  JsonTextResult fn = json_nat_field(heap, &fields.value, str_lit("fn"));
  if (fn.kind == JSON_FAIL) return expr_fail(heap, fn.message);
  JsonTextResult arg = json_nat_field(heap, &fields.value, str_lit("arg"));
  if (arg.kind == JSON_FAIL) return expr_fail(heap, arg.message);
  Expr out = expr_blank(heap, EXPR_APPLY);
  out.fn = fn.value;
  out.arg = arg.value;
  return expr_done(out);
}

/* parse, cases "lam" and "forallE": binder(value) */
static ExprResult parse_bound(Heap *heap, ExprKind kind, const Json *value) {
  BinderResult bound = binder(heap, value);
  if (bound.kind == JSON_FAIL) return expr_fail(heap, bound.message);
  Expr out = expr_blank(heap, kind);
  out.binder = bound.value;
  return expr_done(out);
}

/* parse, case "letE" */
static ExprResult parse_let(Heap *heap, const Json *value) {
  static const char *const names[] = {"name", "type", "value", "body", "nondep"};
  Vec expected = json_expected_keys(heap, names, 5);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return expr_fail(heap, fields.message);
  JsonTextResult name = json_nat_field(heap, &fields.value, str_lit("name"));
  if (name.kind == JSON_FAIL) return expr_fail(heap, name.message);
  JsonTextResult typ = json_nat_field(heap, &fields.value, str_lit("type"));
  if (typ.kind == JSON_FAIL) return expr_fail(heap, typ.message);
  JsonTextResult val = json_nat_field(heap, &fields.value, str_lit("value"));
  if (val.kind == JSON_FAIL) return expr_fail(heap, val.message);
  JsonTextResult body = json_nat_field(heap, &fields.value, str_lit("body"));
  if (body.kind == JSON_FAIL) return expr_fail(heap, body.message);
  JsonBoolResult nondep = json_bool_field(heap, &fields.value, str_lit("nondep"));
  if (nondep.kind == JSON_FAIL) return expr_fail(heap, nondep.message);
  Expr out = expr_blank(heap, EXPR_LET);
  out.name = name.value;
  out.typ = typ.value;
  out.value = val.value;
  out.body = body.value;
  out.nondep = nondep.value;
  return expr_done(out);
}

/* parse, case "proj" */
static ExprResult parse_proj(Heap *heap, const Json *value) {
  static const char *const names[] = {"typeName", "idx", "struct"};
  Vec expected = json_expected_keys(heap, names, 3);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return expr_fail(heap, fields.message);
  JsonTextResult type_name = json_nat_field(heap, &fields.value, str_lit("typeName"));
  if (type_name.kind == JSON_FAIL) return expr_fail(heap, type_name.message);
  JsonTextResult index = json_nat_field(heap, &fields.value, str_lit("idx"));
  if (index.kind == JSON_FAIL) return expr_fail(heap, index.message);
  JsonTextResult structure = json_nat_field(heap, &fields.value, str_lit("struct"));
  if (structure.kind == JSON_FAIL) return expr_fail(heap, structure.message);
  Expr out = expr_blank(heap, EXPR_PROJ);
  out.type_name = type_name.value;
  out.index = index.value;
  out.structure = structure.value;
  return expr_done(out);
}

/* parse, case "natVal" */
static ExprResult parse_nat_val(Heap *heap, const Json *value) {
  JsonTextResult text = json_string(value);
  if (text.kind == JSON_FAIL) return expr_fail(heap, text.message);
  return natural_text(heap, text.value);
}

/* parse, case "strVal" */
static ExprResult parse_str_val(Heap *heap, const Json *value) {
  JsonTextResult text = json_string(value);
  if (text.kind == JSON_FAIL) return expr_fail(heap, text.message);
  Expr out = expr_blank(heap, EXPR_STRING_LITERAL);
  out.text = text.value;
  return expr_done(out);
}

/* parse, case "mdata" */
static ExprResult parse_mdata(Heap *heap, const Json *value) {
  static const char *const names[] = {"expr", "data"};
  Vec expected = json_expected_keys(heap, names, 2);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return expr_fail(heap, fields.message);
  JsonTextResult expr = json_nat_field(heap, &fields.value, str_lit("expr"));
  if (expr.kind == JSON_FAIL) return expr_fail(heap, expr.message);
  JsonResult raw = json_field(heap, &fields.value, str_lit("data"));
  if (raw.kind == JSON_FAIL) return expr_fail(heap, raw.message);
  JsonListResult data = json_object_fields(raw.value);
  if (data.kind == JSON_FAIL) return expr_fail(heap, data.message);
  Expr out = expr_blank(heap, EXPR_MDATA);
  out.expr = expr.value;
  out.data = data.value;
  return expr_done(out);
}

/* parse(tag, value) */
ExprResult expr_parse(Heap *heap, Str tag, const Json *value) {
  if (str_eq(tag, str_lit("bvar"))) return parse_indexed(heap, EXPR_BVAR, value);
  if (str_eq(tag, str_lit("sort"))) return parse_indexed(heap, EXPR_SORT, value);
  if (str_eq(tag, str_lit("const"))) return parse_const(heap, value);
  if (str_eq(tag, str_lit("app"))) return parse_app(heap, value);
  if (str_eq(tag, str_lit("lam"))) return parse_bound(heap, EXPR_LAM, value);
  if (str_eq(tag, str_lit("forallE"))) return parse_bound(heap, EXPR_FORALL, value);
  if (str_eq(tag, str_lit("letE"))) return parse_let(heap, value);
  if (str_eq(tag, str_lit("proj"))) return parse_proj(heap, value);
  if (str_eq(tag, str_lit("natVal"))) return parse_nat_val(heap, value);
  if (str_eq(tag, str_lit("strVal"))) return parse_str_val(heap, value);
  if (str_eq(tag, str_lit("mdata"))) return parse_mdata(heap, value);
  return expr_fail(heap, heap_str_cat(heap, str_lit("unknown expression tag "), tag));
}

/* [item] appended to out, as a Str* */
static Vec with_text(Heap *heap, Vec out, Str item) {
  Str *box = json_boxed_str(heap, item);
  if (box == NULL) return out;
  return json_vec_with(heap, out, box);
}

/* name_references(value): the name indices the expression refers to. */
Vec expr_name_references(Heap *heap, const Expr *value) {
  Vec out = vec_new(heap->arena);
  switch (value->kind) {
    case EXPR_BVAR: return out;
    case EXPR_SORT: return out;
    case EXPR_CONST: return with_text(heap, out, value->name);
    case EXPR_APPLY: return out;
    case EXPR_LAM: return with_text(heap, out, value->binder.name);
    case EXPR_FORALL: return with_text(heap, out, value->binder.name);
    case EXPR_LET: return with_text(heap, out, value->name);
    case EXPR_PROJ: return with_text(heap, out, value->type_name);
    case EXPR_NAT_LITERAL: return out;
    case EXPR_STRING_LITERAL: return out;
    case EXPR_MDATA: return out;
  }
  return out;
}

/* level_references(value): the level indices the expression refers to. */
Vec expr_level_references(Heap *heap, const Expr *value) {
  Vec out = vec_new(heap->arena);
  switch (value->kind) {
    case EXPR_BVAR: return out;
    case EXPR_SORT: return with_text(heap, out, value->index);
    case EXPR_CONST: return value->universes;
    case EXPR_APPLY: return out;
    case EXPR_LAM: return out;
    case EXPR_FORALL: return out;
    case EXPR_LET: return out;
    case EXPR_PROJ: return out;
    case EXPR_NAT_LITERAL: return out;
    case EXPR_STRING_LITERAL: return out;
    case EXPR_MDATA: return out;
  }
  return out;
}

/* expr_references(value): the expression indices the expression refers to. */
Vec expr_expr_references(Heap *heap, const Expr *value) {
  Vec out = vec_new(heap->arena);
  switch (value->kind) {
    case EXPR_BVAR: return out;
    case EXPR_SORT: return out;
    case EXPR_CONST: return out;
    case EXPR_APPLY: return with_text(heap, with_text(heap, out, value->fn), value->arg);
    case EXPR_LAM: return with_text(heap, with_text(heap, out, value->binder.typ), value->binder.body);
    case EXPR_FORALL: return with_text(heap, with_text(heap, out, value->binder.typ), value->binder.body);
    case EXPR_LET:
      return with_text(heap, with_text(heap, with_text(heap, out, value->typ), value->value), value->body);
    case EXPR_PROJ: return with_text(heap, out, value->structure);
    case EXPR_NAT_LITERAL: return out;
    case EXPR_STRING_LITERAL: return out;
    case EXPR_MDATA: return with_text(heap, out, value->expr);
  }
  return out;
}
