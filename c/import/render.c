/* Port of bend2/import/render.bend: the JSON rendering of a translated export (Translate.T) for the
 * import drivers and the report. Bend lists are Vec items rendered in order by recursive index
 * walkers; a NULL value (a failed allocation, heap->oom set) is not stored, like the other import
 * renderers. Every definition keeps its Bend name with the render_ prefix, in Bend order. */
#include "import.h"

/* out with item appended; a NULL item is a failed allocation (heap->oom set) and is not stored */
static Vec with_item(Heap *heap, Vec out, void *item) {
  if (item == NULL) return out;
  return json_vec_with(heap, out, item);
}

/* Json.Field{key, value}: NULL when the value or the boxing allocation failed */
static JsonField *field(Heap *heap, const char *key, Json *value) {
  if (value == NULL) return NULL;
  return json_field_new(heap, str_lit(key), value);
}

/* number(value) */
Json *render_number(Heap *heap, Str value) { return json_number(heap, value); }

/* number_items(values) from INDEX: values are Str* items */
static Vec number_items(Heap *heap, const Vec *values, size_t index, Vec out) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return out;
  return number_items(heap, values, index + 1, with_item(heap, out, render_number(heap, *(Str *)got.value)));
}

Vec render_number_items(Heap *heap, const Vec *values) {
  return number_items(heap, values, 0, vec_new(heap->arena));
}

/* numbers(values) */
Json *render_numbers(Heap *heap, const Vec *values) {
  return json_items(heap, render_number_items(heap, values));
}

/* string(value) */
Json *render_string(Heap *heap, Str value) { return json_text(heap, value); }

/* string_items(values) from INDEX: values are Str* items */
static Vec string_items(Heap *heap, const Vec *values, size_t index, Vec out) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return out;
  return string_items(heap, values, index + 1, with_item(heap, out, render_string(heap, *(Str *)got.value)));
}

Vec render_string_items(Heap *heap, const Vec *values) {
  return string_items(heap, values, 0, vec_new(heap->arena));
}

/* strings(values) */
Json *render_strings(Heap *heap, const Vec *values) {
  return json_items(heap, render_string_items(heap, values));
}

/* binder_info(info) */
Str render_binder_info(ExprBinderInfo info) {
  switch (info) {
    case EXPR_BINDER_DEFAULT: return str_lit("default");
    case EXPR_BINDER_IMPLICIT: return str_lit("implicit");
    case EXPR_BINDER_STRICT_IMPLICIT: return str_lit("strictImplicit");
    case EXPR_BINDER_INST_IMPLICIT: return str_lit("instImplicit");
  }
  return str_lit("default");
}

/* binder(value) */
Json *render_binder(Heap *heap, const TranslateBinder *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, value->name)));
  fields = with_item(heap, fields, field(heap, "type", json_number(heap, value->typ)));
  fields = with_item(heap, fields, field(heap, "body", json_number(heap, value->body)));
  fields = with_item(heap, fields, field(heap, "binderInfo", json_text(heap, render_binder_info(value->info))));
  return json_object(heap, fields);
}

/* the Constant{source_name, name, universes} object of node_field */
static Json *constant_object(Heap *heap, const TranslateNode *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "source_name", json_number(heap, value->source_name)));
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, value->name)));
  fields = with_item(heap, fields, field(heap, "universes", render_numbers(heap, &value->universes)));
  return json_object(heap, fields);
}

/* the Apply{fn, arg} object of node_field */
static Json *apply_object(Heap *heap, const TranslateNode *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "fn", json_number(heap, value->fn)));
  fields = with_item(heap, fields, field(heap, "arg", json_number(heap, value->arg)));
  return json_object(heap, fields);
}

/* the Let{name, typ, value, body, nondep} object of node_field */
static Json *let_object(Heap *heap, const TranslateNode *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, value->name)));
  fields = with_item(heap, fields, field(heap, "type", json_number(heap, value->typ)));
  fields = with_item(heap, fields, field(heap, "value", json_number(heap, value->value)));
  fields = with_item(heap, fields, field(heap, "body", json_number(heap, value->body)));
  fields = with_item(heap, fields, field(heap, "nondep", json_boolean(heap, value->nondep)));
  return json_object(heap, fields);
}

/* the Projection{source_type_name, type_name, index, structure} object of node_field */
static Json *projection_object(Heap *heap, const TranslateNode *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "source_type_name", json_number(heap, value->source_type_name)));
  fields = with_item(heap, fields, field(heap, "typeName", json_text(heap, value->type_name)));
  fields = with_item(heap, fields, field(heap, "idx", json_number(heap, value->index)));
  fields = with_item(heap, fields, field(heap, "struct", json_number(heap, value->structure)));
  return json_object(heap, fields);
}

/* the Metadata{expr, data} object of node_field */
static Json *metadata_object(Heap *heap, const TranslateNode *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "expr", json_number(heap, value->expr)));
  fields = with_item(heap, fields, field(heap, "data", json_object(heap, value->data)));
  return json_object(heap, fields);
}

/* node_field(value) */
JsonField *render_node_field(Heap *heap, const TranslateNode *value) {
  switch (value->kind) {
    case TRANSLATE_BOUND: return field(heap, "bvar", json_number(heap, value->index));
    case TRANSLATE_SORT: return field(heap, "sort", json_number(heap, value->level));
    case TRANSLATE_CONSTANT: return field(heap, "const", constant_object(heap, value));
    case TRANSLATE_APPLY: return field(heap, "app", apply_object(heap, value));
    case TRANSLATE_LAMBDA: return field(heap, "lam", render_binder(heap, &value->binder));
    case TRANSLATE_PI: return field(heap, "forallE", render_binder(heap, &value->binder));
    case TRANSLATE_LET: return field(heap, "letE", let_object(heap, value));
    case TRANSLATE_PROJECTION: return field(heap, "proj", projection_object(heap, value));
    case TRANSLATE_NAT_LITERAL: return field(heap, "natVal", json_text(heap, value->value));
    case TRANSLATE_STRING_LITERAL: return field(heap, "strVal", json_text(heap, value->value));
    case TRANSLATE_METADATA: return field(heap, "mdata", metadata_object(heap, value));
  }
  return field(heap, "bvar", json_number(heap, value->index));
}

/* node_record(row) */
Json *render_node_record(Heap *heap, const TableIndexed *row) {
  const TranslateNode *node = row->value;
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "expr", json_number(heap, row->index)));
  fields = with_item(heap, fields, render_node_field(heap, node));
  return json_object(heap, fields);
}

/* node_records(rows) from INDEX: rows are TableIndexed* items */
static Vec node_records(Heap *heap, const Vec *rows, size_t index, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  return node_records(heap, rows, index + 1, with_item(heap, out, render_node_record(heap, got.value)));
}

Vec render_node_records(Heap *heap, const Vec *rows) { return node_records(heap, rows, 0, vec_new(heap->arena)); }

/* numbers([left, right]) */
static Json *pair_numbers(Heap *heap, Str left, Str right) {
  Vec values = vec_new(heap->arena);
  values = with_item(heap, values, json_boxed_str(heap, left));
  values = with_item(heap, values, json_boxed_str(heap, right));
  return render_numbers(heap, &values);
}

/* the LParam{source_name, name} object of level_field */
static Json *param_object(Heap *heap, const TranslateLevel *level) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "source_name", json_number(heap, level->source_name)));
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, level->name)));
  return json_object(heap, fields);
}

/* level_field(level) */
JsonField *render_level_field(Heap *heap, const TranslateLevel *level) {
  switch (level->kind) {
    case TRANSLATE_LZERO: return field(heap, "zero", json_boolean(heap, true));
    case TRANSLATE_LSUCC: return field(heap, "succ", json_number(heap, level->child));
    case TRANSLATE_LMAX: return field(heap, "max", pair_numbers(heap, level->left, level->right));
    case TRANSLATE_LIMAX: return field(heap, "imax", pair_numbers(heap, level->left, level->right));
    case TRANSLATE_LPARAM: return field(heap, "param", param_object(heap, level));
  }
  return field(heap, "zero", json_boolean(heap, true));
}

/* level_record(row) */
Json *render_level_record(Heap *heap, const TableIndexed *row) {
  const TranslateLevel *level = row->value;
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "level", json_number(heap, row->index)));
  fields = with_item(heap, fields, render_level_field(heap, level));
  return json_object(heap, fields);
}

/* level_records(rows) from INDEX: rows are TableIndexed* items */
static Vec level_records(Heap *heap, const Vec *rows, size_t index, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  return level_records(heap, rows, index + 1, with_item(heap, out, render_level_record(heap, got.value)));
}

Vec render_level_records(Heap *heap, const Vec *rows) { return level_records(heap, rows, 0, vec_new(heap->arena)); }

/* the Str{prefix, text} object of name_field */
static Json *name_str_object(Heap *heap, const Name *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "pre", json_number(heap, value->prefix)));
  fields = with_item(heap, fields, field(heap, "str", json_text(heap, value->text)));
  return json_object(heap, fields);
}

/* the Num{prefix, index} object of name_field */
static Json *name_num_object(Heap *heap, const Name *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "pre", json_number(heap, value->prefix)));
  fields = with_item(heap, fields, field(heap, "i", json_number(heap, value->index)));
  return json_object(heap, fields);
}

/* name_field(value) */
JsonField *render_name_field(Heap *heap, const Name *value) {
  switch (value->kind) {
    case NAME_ANONYMOUS: return field(heap, "anonymous", json_boolean(heap, true));
    case NAME_STR: return field(heap, "str", name_str_object(heap, value));
    case NAME_NUM: return field(heap, "num", name_num_object(heap, value));
  }
  return field(heap, "anonymous", json_boolean(heap, true));
}

/* name_record(row) */
Json *render_name_record(Heap *heap, const TableIndexed *row) {
  const Name *value = row->value;
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "name", json_number(heap, row->index)));
  fields = with_item(heap, fields, render_name_field(heap, value));
  return json_object(heap, fields);
}

/* parameter(value) */
Json *render_parameter(Heap *heap, const TranslateParameter *value) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "source_name", json_number(heap, value->source_name)));
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, value->name)));
  return json_object(heap, fields);
}

/* parameters(values) from INDEX: values are TranslateParameter* items */
static Vec parameters(Heap *heap, const Vec *values, size_t index, Vec out) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return out;
  return parameters(heap, values, index + 1, with_item(heap, out, render_parameter(heap, got.value)));
}

Vec render_parameters(Heap *heap, const Vec *values) { return parameters(heap, values, 0, vec_new(heap->arena)); }

/* declaration_id(value) */
Str render_declaration_id(const DeclEntry *value) { return value->name; }

/* row_summary(value) */
Json *render_row_summary(Heap *heap, const TranslateRow *value) {
  Vec items = vec_new(heap->arena);
  items = with_item(heap, items, json_number(heap, render_declaration_id(&value->declaration)));
  items = with_item(heap, items, json_text(heap, value->name));
  items = with_item(heap, items, json_items(heap, render_parameters(heap, &value->parameters)));
  items = with_item(heap, items, json_number(heap, value->root));
  items = with_item(heap, items, render_strings(heap, &value->dependencies));
  return json_items(heap, items);
}

/* row_summaries(values) from INDEX: values are TranslateRow* items */
static Vec row_summaries(Heap *heap, const Vec *values, size_t index, Vec out) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return out;
  return row_summaries(heap, values, index + 1, with_item(heap, out, render_row_summary(heap, got.value)));
}

Vec render_row_summaries(Heap *heap, const Vec *values) {
  return row_summaries(heap, values, 0, vec_new(heap->arena));
}

/* scoped_summary(table) */
Json *render_scoped_summary(Heap *heap, Translated *table) {
  Vec rows = translate_rows(table);
  Vec levels = translate_levels(heap, table);
  Vec nodes = translate_nodes(heap, table);
  Vec items = vec_new(heap->arena);
  items = with_item(heap, items, json_items(heap, render_row_summaries(heap, &rows)));
  items = with_item(heap, items, json_items(heap, render_level_records(heap, &levels)));
  items = with_item(heap, items, json_items(heap, render_node_records(heap, &nodes)));
  return json_items(heap, items);
}
