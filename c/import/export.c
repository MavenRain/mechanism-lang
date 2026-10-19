/* Port of import/export.bend (import "Export"): the reader of a Lean 4 NDJSON export, format
 * 3.1.0. Line 1 holds the metadata object; every later line is one name, level or expression row
 * (keyed "in", "il" or "ie" next to its tag) or one declaration object. Rows are indexed by the
 * decimal text of their stream index and every reference must name a row read before it. The Bend
 * State is threaded linearly through the reader, so the C ExportState is updated in place: the Bend
 * with_names .. with_groups are the field assignments below. Every message text is the Bend text,
 * byte for byte. */
#include <string.h>

#include "import.h"

/* Result<String, State>: the state is updated in place, so the result carries the status only. */
typedef struct {
  JsonResultKind kind;
  Str message;
} StateResult;
/* Result<String, Metadata> */
typedef struct {
  JsonResultKind kind;
  ExportMetadata value;
  Str message;
} MetadataResult;
/* NameParts{key, text} */
typedef struct {
  Str key;
  Str text;
} NameParts;
/* Result<String, NameParts> */
typedef struct {
  JsonResultKind kind;
  NameParts value;
  Str message;
} NamePartsResult;
/* KeySplit{left, right}: Str* items */
typedef struct {
  Vec left;
  Vec right;
} KeySplit;

static StateResult state_done(void) {
  StateResult result;
  result.kind = JSON_DONE;
  result.message = str_empty();
  return result;
}
static StateResult state_fail(Str message) {
  StateResult result;
  result.kind = JSON_FAIL;
  result.message = message;
  return result;
}
static ExportMetadata empty_metadata(void) {
  ExportMetadata value;
  value.exporter_name = str_empty();
  value.exporter_version = str_empty();
  value.lean_githash = str_empty();
  value.lean_version = str_empty();
  value.format_version = str_empty();
  return value;
}
static MetadataResult metadata_done(ExportMetadata value) {
  MetadataResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}
static MetadataResult metadata_fail(Str message) {
  MetadataResult result;
  result.kind = JSON_FAIL;
  result.value = empty_metadata();
  result.message = message;
  return result;
}
static NamePartsResult parts_done(NameParts value) {
  NamePartsResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}
static NamePartsResult parts_fail(Str message) {
  NamePartsResult result;
  result.kind = JSON_FAIL;
  result.value.key = str_empty();
  result.value.text = str_empty();
  result.message = message;
  return result;
}
static ExportResult export_done(Export value) {
  ExportResult result;
  memset(&result, 0, sizeof result);
  result.kind = JSON_DONE;
  result.value = value;
  result.error.line = 0;
  result.error.message = str_empty();
  return result;
}
static ExportResult export_fail(ExportError error) {
  ExportResult result;
  memset(&result, 0, sizeof result);
  result.kind = JSON_FAIL;
  result.error = error;
  return result;
}
/* a ++ b ++ c */
static Str cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }
/* U32.show and Nat.show of a host count */
static Str show_u64(Heap *heap, uint64_t value) { return str_of_u64(heap->arena, value); }
/* Nat.sub: natural subtraction stops at zero. */
static size_t nat_sub_count(size_t a, size_t b) { return a > b ? a - b : 0; }
/* [item] appended to out, as a Str* */
static Vec with_text(Heap *heap, Vec out, Str item) {
  Str *box = json_boxed_str(heap, item);
  if (box == NULL) return out;
  return json_vec_with(heap, out, box);
}
/* item appended to out (a NULL item is a failed allocation, heap->oom set) */
static Vec with_item(Heap *heap, Vec out, void *item) {
  if (item == NULL) return out;
  return json_vec_with(heap, out, item);
}
/* the items of xs from INDEX appended to acc */
static Vec append_from(Heap *heap, Vec acc, const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status != VEC_GET_OK) return acc;
  return append_from(heap, json_vec_with(heap, acc, got.value), xs, index + 1);
}
/* the first COUNT items of xs from INDEX appended to acc */
static Vec prefix_from(Heap *heap, Vec acc, const Vec *xs, size_t index, size_t count) {
  VecGet got = vec_get(xs, index);
  if (index >= count || got.status != VEC_GET_OK) return acc;
  return prefix_from(heap, json_vec_with(heap, acc, got.value), xs, index + 1, count);
}
/* the Str of a boxed Str* (empty for NULL) */
static Str unbox_text(const void *box) { return box == NULL ? str_empty() : *(const Str *)box; }
static ExportNameRow *box_name_row(Heap *heap, ExportNameRow value) {
  ExportNameRow *box = heap_alloc(heap, sizeof(ExportNameRow));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static Level *box_level(Heap *heap, Level value) {
  Level *box = heap_alloc(heap, sizeof(Level));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static ExportExprRow *box_expr_row(Heap *heap, ExportExprRow value) {
  ExportExprRow *box = heap_alloc(heap, sizeof(ExportExprRow));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static DeclGroup *box_group(Heap *heap, DeclGroup value) {
  DeclGroup *box = heap_alloc(heap, sizeof(DeclGroup));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
/* Names.Anonymous{} */
static Name anonymous_name(void) {
  Name value;
  value.kind = NAME_ANONYMOUS;
  value.prefix = str_empty();
  value.text = str_empty();
  value.index = str_empty();
  return value;
}
/* Levels.LZero{} */
static Level zero_level(void) {
  Level value;
  value.kind = LEVEL_ZERO;
  value.index = str_empty();
  value.left = str_empty();
  value.right = str_empty();
  return value;
}
/* the absent Expr of a None{} */
static Expr no_expr(void) {
  Expr value;
  memset(&value, 0, sizeof value);
  value.kind = EXPR_BVAR;
  return value;
}
/* the absent Decls.T of a None{} */
static DeclEntry no_declaration(void) {
  DeclEntry value;
  memset(&value, 0, sizeof value);
  value.kind.tag = DECL_KIND_AXIOM;
  return value;
}

/* state_names(state) .. state_groups(state) */
static const Table *state_names(const ExportState *state) { return &state->names; }
static const Table *state_interned(const ExportState *state) { return &state->interned; }
static const Table *state_levels(const ExportState *state) { return &state->levels; }
static const Table *state_exprs(const ExportState *state) { return &state->exprs; }
static const Table *state_decls(const ExportState *state) { return &state->decls; }
static const Vec *state_expr_order(const ExportState *state) { return &state->expr_order; }
static const Vec *state_decl_order(const ExportState *state) { return &state->decl_order; }
static const Vec *state_groups(const ExportState *state) { return &state->groups; }

/* initial(): name 0 = Anonymous (key "", text "", canonical "0"), the interned key "" -> "0",
 * level 0 = LZero. */
static ExportState initial(Heap *heap) {
  ExportState state;
  ExportNameRow row;
  row.node = anonymous_name();
  row.key = str_empty();
  row.text = str_empty();
  row.canonical = str_lit("0");
  state.names = table_empty(heap);
  state.interned = table_empty(heap);
  state.levels = table_empty(heap);
  state.exprs = table_empty(heap);
  state.decls = table_empty(heap);
  state.expr_order = vec_new(heap->arena);
  state.decl_order = vec_new(heap->arena);
  state.groups = vec_new(heap->arena);
  table_put(heap, &state.names, str_lit("0"), box_name_row(heap, row));
  table_put(heap, &state.interned, str_empty(), json_boxed_str(heap, str_lit("0")));
  table_put(heap, &state.levels, str_lit("0"), box_level(heap, zero_level()));
  return state;
}

/* at_line(result, line): a String failure as ImportError{line, message}. */
static ExportError at_line(Str message, uint32_t line) {
  ExportError error;
  error.line = line;
  error.message = message;
  return error;
}

/* metadata_version(valid, version, exporter, exporter_version, lean_githash, lean_version) */
static MetadataResult metadata_version(Heap *heap, bool valid, Str version, Str exporter, Str exporter_version,
                                       Str lean_githash, Str lean_version) {
  ExportMetadata value;
  if (!valid) {
    return metadata_fail(cat3(heap, str_lit("unsupported export format "), version, str_lit(", expected 3.1.0")));
  }
  value.exporter_name = exporter;
  value.exporter_version = exporter_version;
  value.lean_githash = lean_githash;
  value.lean_version = lean_version;
  value.format_version = version;
  return metadata_done(value);
}

/* parse_metadata(value): {"meta": {"exporter": {"name", "version"}, "lean": {"githash", "version"},
 * "format": {"version"}}} */
static MetadataResult parse_metadata(Heap *heap, const Json *value) {
  static const char *const outer_keys[] = {"meta"};
  static const char *const meta_keys[] = {"exporter", "lean", "format"};
  static const char *const exporter_keys[] = {"name", "version"};
  static const char *const lean_keys[] = {"githash", "version"};
  static const char *const format_keys[] = {"version"};
  Vec expected_outer = json_expected_keys(heap, outer_keys, 1);
  JsonListResult outer = json_exact_object(heap, value, &expected_outer);
  if (outer.kind == JSON_FAIL) return metadata_fail(outer.message);
  JsonResult meta = json_field(heap, &outer.value, str_lit("meta"));
  if (meta.kind == JSON_FAIL) return metadata_fail(meta.message);
  Vec expected_meta = json_expected_keys(heap, meta_keys, 3);
  JsonListResult fields = json_exact_object(heap, meta.value, &expected_meta);
  if (fields.kind == JSON_FAIL) return metadata_fail(fields.message);
  JsonResult exporter = json_field(heap, &fields.value, str_lit("exporter"));
  if (exporter.kind == JSON_FAIL) return metadata_fail(exporter.message);
  Vec expected_exporter = json_expected_keys(heap, exporter_keys, 2);
  JsonListResult exporter_fields = json_exact_object(heap, exporter.value, &expected_exporter);
  if (exporter_fields.kind == JSON_FAIL) return metadata_fail(exporter_fields.message);
  JsonTextResult exporter_name = json_string_field(heap, &exporter_fields.value, str_lit("name"));
  if (exporter_name.kind == JSON_FAIL) return metadata_fail(exporter_name.message);
  JsonTextResult exporter_version = json_string_field(heap, &exporter_fields.value, str_lit("version"));
  if (exporter_version.kind == JSON_FAIL) return metadata_fail(exporter_version.message);
  JsonResult lean = json_field(heap, &fields.value, str_lit("lean"));
  if (lean.kind == JSON_FAIL) return metadata_fail(lean.message);
  Vec expected_lean = json_expected_keys(heap, lean_keys, 2);
  JsonListResult lean_fields = json_exact_object(heap, lean.value, &expected_lean);
  if (lean_fields.kind == JSON_FAIL) return metadata_fail(lean_fields.message);
  JsonTextResult lean_githash = json_string_field(heap, &lean_fields.value, str_lit("githash"));
  if (lean_githash.kind == JSON_FAIL) return metadata_fail(lean_githash.message);
  JsonTextResult lean_version = json_string_field(heap, &lean_fields.value, str_lit("version"));
  if (lean_version.kind == JSON_FAIL) return metadata_fail(lean_version.message);
  JsonResult format = json_field(heap, &fields.value, str_lit("format"));
  if (format.kind == JSON_FAIL) return metadata_fail(format.message);
  Vec expected_format = json_expected_keys(heap, format_keys, 1);
  JsonListResult format_fields = json_exact_object(heap, format.value, &expected_format);
  if (format_fields.kind == JSON_FAIL) return metadata_fail(format_fields.message);
  JsonTextResult version = json_string_field(heap, &format_fields.value, str_lit("version"));
  if (version.kind == JSON_FAIL) return metadata_fail(version.message);
  return metadata_version(heap, str_eq(version.value, str_lit("3.1.0")), version.value, exporter_name.value,
                          exporter_version.value, lean_githash.value, lean_version.value);
}

/* byte_length(text): the UTF-8 byte count (the C text already holds UTF-8 bytes). */
static size_t byte_length(Str text) { return text.len; }

/* ordinary_char(ch): a-z, A-Z, 0-9, '_' or '\''. A UTF-8 continuation or lead byte is never one, as
 * the Bend codepoint is never one. */
static bool ordinary_char(unsigned char code) {
  return (code >= 97 && code <= 122) || (code >= 65 && code <= 90) || (code >= 48 && code <= 57) || code == 95 ||
         code == 39;
}

static bool ordinary_from(Str text, size_t index) {
  StrAt at = str_at(text, index);
  if (at.status != STR_INDEX_OK) return true;
  return ordinary_char(at.value) && ordinary_from(text, index + 1);
}

/* ordinary_text(text) */
static bool ordinary_text(Str text) { return ordinary_from(text, 0); }

/* display_pick(ordinary, text) */
static Str display_pick(Heap *heap, bool ordinary, Str text) {
  if (ordinary) return text;
  return cat3(heap, str_lit("\xc2\xab"), json_quoted(heap, text), str_lit("\xc2\xbb"));
}

/* display_component(text) */
static Str display_component(Heap *heap, Str text) {
  if (text.len == 0) return str_lit("\xc2\xab\"\"\xc2\xbb");
  return display_pick(heap, ordinary_text(text) && !json_decimal_digits(text), text);
}

/* display_append(prefix, component) */
static Str display_append(Heap *heap, Str prefix, Str component) {
  if (prefix.len == 0) return component;
  return cat3(heap, prefix, str_lit("."), component);
}

/* str_parts(prefix, component): key ++ "s" ++ byte length ++ ":" ++ component */
static NameParts str_parts(Heap *heap, const ExportNameRow *prefix, Str component) {
  NameParts parts;
  parts.key = heap_str_cat(heap, cat3(heap, prefix->key, str_lit("s"), show_u64(heap, byte_length(component))),
                           heap_str_cat(heap, str_lit(":"), component));
  parts.text = display_append(heap, prefix->text, display_component(heap, component));
  return parts;
}

/* num_parts(prefix, number): key ++ "n" ++ number ++ ":" */
static NameParts num_parts(Heap *heap, const ExportNameRow *prefix, Str number) {
  NameParts parts;
  parts.key = cat3(heap, heap_str_cat(heap, prefix->key, str_lit("n")), number, str_lit(":"));
  parts.text = display_append(heap, prefix->text, number);
  return parts;
}

/* name_parts(node, names) */
static NamePartsResult name_parts(Heap *heap, Name node, const Table *names) {
  NameParts empty;
  empty.key = str_empty();
  empty.text = str_empty();
  switch (node.kind) {
    case NAME_ANONYMOUS: return parts_done(empty);
    case NAME_STR: {
      TableLookup prefix = table_lookup(heap, names, str_lit("name"), node.prefix);
      if (prefix.kind == JSON_FAIL) return parts_fail(prefix.message);
      return parts_done(str_parts(heap, prefix.value, node.text));
    }
    case NAME_NUM: {
      TableLookup prefix = table_lookup(heap, names, str_lit("name"), node.prefix);
      if (prefix.kind == JSON_FAIL) return parts_fail(prefix.message);
      return parts_done(num_parts(heap, prefix.value, node.index));
    }
  }
  return parts_done(empty);
}

/* name_install(parts, state, index, node): the canonical index of a key is the first index that
 * interned it. */
static void name_install(Heap *heap, NameParts parts, ExportState *state, Str index, Name node) {
  ExportNameRow row;
  Str canonical = unbox_text(table_default(table_get(state_interned(state), parts.key), json_boxed_str(heap, index)));
  table_put(heap, &state->interned, parts.key, json_boxed_str(heap, canonical));
  row.node = node;
  row.key = parts.key;
  row.text = parts.text;
  row.canonical = canonical;
  table_put(heap, &state->names, index, box_name_row(heap, row));
}

/* name_unique(duplicate, state, index, tag, value) */
static StateResult name_unique(Heap *heap, bool duplicate, ExportState *state, Str index, Str tag, const Json *value) {
  if (duplicate) return state_fail(heap_str_cat(heap, str_lit("duplicate name index "), index));
  NameResult node = name_parse(heap, tag, value);
  if (node.kind == JSON_FAIL) return state_fail(node.message);
  Vec references = name_references(heap, node.value);
  TableCheck checked = table_references(heap, &references, state_names(state), str_lit("name"));
  if (checked.kind == JSON_FAIL) return state_fail(checked.message);
  NamePartsResult parts = name_parts(heap, node.value, state_names(state));
  if (parts.kind == JSON_FAIL) return state_fail(parts.message);
  name_install(heap, parts.value, state, index, node.value);
  return state_done();
}

/* add_name(state, index, tag, value) */
static StateResult add_name(Heap *heap, ExportState *state, Str index, Str tag, const Json *value) {
  return name_unique(heap, table_has(state_names(state), index), state, index, tag, value);
}

/* level_unique(duplicate, state, index, tag, value) */
static StateResult level_unique(Heap *heap, bool duplicate, ExportState *state, Str index, Str tag, const Json *value) {
  if (duplicate) return state_fail(heap_str_cat(heap, str_lit("duplicate level index "), index));
  LevelResult node = level_parse(heap, tag, value);
  if (node.kind == JSON_FAIL) return state_fail(node.message);
  Vec name_refs = level_name_references(heap, node.value);
  TableCheck names_checked = table_references(heap, &name_refs, state_names(state), str_lit("name"));
  if (names_checked.kind == JSON_FAIL) return state_fail(names_checked.message);
  Vec level_refs = level_level_references(heap, node.value);
  TableCheck levels_checked = table_references(heap, &level_refs, state_levels(state), str_lit("level"));
  if (levels_checked.kind == JSON_FAIL) return state_fail(levels_checked.message);
  table_put(heap, &state->levels, index, box_level(heap, node.value));
  return state_done();
}

/* add_level(state, index, tag, value) */
static StateResult add_level(Heap *heap, ExportState *state, Str index, Str tag, const Json *value) {
  return level_unique(heap, table_has(state_levels(state), index), state, index, tag, value);
}

/* expr_unique(duplicate, state, index, tag, value, line) */
static StateResult expr_unique(Heap *heap, bool duplicate, ExportState *state, Str index, Str tag, const Json *value,
                               uint32_t line) {
  if (duplicate) return state_fail(heap_str_cat(heap, str_lit("duplicate expression index "), index));
  ExprResult node = expr_parse(heap, tag, value);
  if (node.kind == JSON_FAIL) return state_fail(node.message);
  Vec name_refs = expr_name_references(heap, &node.value);
  TableCheck names_checked = table_references(heap, &name_refs, state_names(state), str_lit("name"));
  if (names_checked.kind == JSON_FAIL) return state_fail(names_checked.message);
  Vec level_refs = expr_level_references(heap, &node.value);
  TableCheck levels_checked = table_references(heap, &level_refs, state_levels(state), str_lit("level"));
  if (levels_checked.kind == JSON_FAIL) return state_fail(levels_checked.message);
  Vec expr_refs = expr_expr_references(heap, &node.value);
  TableCheck exprs_checked = table_references(heap, &expr_refs, state_exprs(state), str_lit("expression"));
  if (exprs_checked.kind == JSON_FAIL) return state_fail(exprs_checked.message);
  ExportExprRow row;
  row.node = node.value;
  row.line = line;
  ExportExprRow *boxed = box_expr_row(heap, row);
  if (boxed == NULL) return state_done();
  table_put(heap, &state->exprs, index, boxed);
  state->expr_order = with_item(heap, state->expr_order, table_indexed(heap, index, &boxed->node));
  return state_done();
}

/* add_expr(state, index, tag, value, line) */
static StateResult add_expr(Heap *heap, ExportState *state, Str index, Str tag, const Json *value, uint32_t line) {
  return expr_unique(heap, table_has(state_exprs(state), index), state, index, tag, value, line);
}

/* decl_name(declaration) */
static Str decl_name(const DeclEntry *declaration) { return declaration->name; }

/* declaration_unique(duplicate, state, key, text, declaration) */
static StateResult declaration_unique(Heap *heap, bool duplicate, ExportState *state, Str key, Str text,
                                      DeclEntry *declaration) {
  if (duplicate) return state_fail(heap_str_cat(heap, str_lit("duplicate declaration "), text));
  table_put(heap, &state->decls, key, declaration);
  state->decl_order = with_item(heap, state->decl_order, declaration);
  return state_done();
}

/* declaration_named(name, state, declaration) */
static StateResult declaration_named(Heap *heap, const ExportNameRow *name, ExportState *state, DeclEntry *declaration) {
  return declaration_unique(heap, table_has(state_decls(state), name->key), state, name->key, name->text, declaration);
}

/* add_declaration(state, declaration) */
static StateResult add_declaration(Heap *heap, ExportState *state, DeclEntry *declaration) {
  Vec name_refs = decl_name_references(heap, declaration);
  TableCheck names_checked = table_references(heap, &name_refs, state_names(state), str_lit("name"));
  if (names_checked.kind == JSON_FAIL) return state_fail(names_checked.message);
  Vec expr_refs = decl_expr_references(heap, declaration);
  TableCheck exprs_checked = table_references(heap, &expr_refs, state_exprs(state), str_lit("expression"));
  if (exprs_checked.kind == JSON_FAIL) return state_fail(exprs_checked.message);
  TableLookup name = table_lookup(heap, state_names(state), str_lit("declaration name"), decl_name(declaration));
  if (name.kind == JSON_FAIL) return state_fail(name.message);
  return declaration_named(heap, name.value, state, declaration);
}

/* add_rows(rows, state) from the row at INDEX: DeclEntry* items */
static StateResult add_rows(Heap *heap, const Vec *rows, size_t index, ExportState *state) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return state_done();
  StateResult next = add_declaration(heap, state, got.value);
  if (next.kind == JSON_FAIL) return next;
  return add_rows(heap, rows, index + 1, state);
}

/* add_group(group, state) */
static void add_group(Heap *heap, const DeclParsed *parsed, ExportState *state) {
  if (!parsed->has_group) return;
  state->groups = with_item(heap, state->groups, box_group(heap, parsed->group));
}

/* add_parsed(parsed, state) */
static StateResult add_parsed(Heap *heap, const DeclParsed *parsed, ExportState *state) {
  StateResult next = add_rows(heap, &parsed->declarations, 0, state);
  if (next.kind == JSON_FAIL) return next;
  add_group(heap, parsed, state);
  return state_done();
}

/* add_declarations(state, tag, value, line) */
static StateResult add_declarations(Heap *heap, ExportState *state, Str tag, const Json *value, uint32_t line) {
  DeclParsedResult parsed = decl_parse(heap, tag, value, line);
  if (parsed.kind == JSON_FAIL) return state_fail(parsed.message);
  return add_parsed(heap, &parsed.value, state);
}

/* indexed_kind(key, index, tag, value, state, line) */
static StateResult indexed_kind(Heap *heap, Str key, Str index, Str tag, const Json *value, ExportState *state,
                                uint32_t line) {
  if (str_eq(key, str_lit("in"))) return add_name(heap, state, index, tag, value);
  if (str_eq(key, str_lit("il"))) return add_level(heap, state, index, tag, value);
  if (str_eq(key, str_lit("ie"))) return add_expr(heap, state, index, tag, value, line);
  return state_fail(str_lit("expected name, level, or expression index"));
}

/* indexed(first, second, state, line): first = Field{key, raw_index}, second = Field{tag, value} */
static StateResult indexed(Heap *heap, const JsonField *first, const JsonField *second, ExportState *state,
                           uint32_t line) {
  JsonTextResult index = json_nat(first->value);
  if (index.kind == JSON_FAIL) return state_fail(index.message);
  return indexed_kind(heap, first->key, index.value, second->key, second->value, state, line);
}

/* single(tag, value, state, line) */
static StateResult single(Heap *heap, Str tag, const Json *value, ExportState *state, uint32_t line) {
  if (str_eq(tag, str_lit("meta"))) return state_fail(str_lit("duplicate metadata"));
  return add_declarations(heap, state, tag, value, line);
}

/* index_order(key, first, second, state, line): the index field may come second. */
static StateResult index_order(Heap *heap, Str key, const JsonField *first, const JsonField *second,
                               ExportState *state, uint32_t line) {
  if (str_eq(key, str_lit("in"))) return indexed(heap, first, second, state, line);
  if (str_eq(key, str_lit("il"))) return indexed(heap, first, second, state, line);
  if (str_eq(key, str_lit("ie"))) return indexed(heap, first, second, state, line);
  return indexed(heap, second, first, state, line);
}

/* item_fields(fields, state, line): JsonField* items */
static StateResult item_fields(Heap *heap, const Vec *fields, ExportState *state, uint32_t line) {
  VecGet first = vec_get(fields, 0);
  VecGet second = vec_get(fields, 1);
  if (fields->len == 1 && first.status == VEC_GET_OK) {
    const JsonField *field = first.value;
    return single(heap, field->key, field->value, state, line);
  }
  if (fields->len == 2 && first.status == VEC_GET_OK && second.status == VEC_GET_OK) {
    const JsonField *field = first.value;
    return index_order(heap, field->key, field, second.value, state, line);
  }
  return state_fail(str_lit("invalid export object shape"));
}

/* item(value, state, line) */
static StateResult item(Heap *heap, const Json *value, ExportState *state, uint32_t line) {
  JsonListResult fields = json_object_fields(value);
  if (fields.kind == JSON_FAIL) return state_fail(fields.message);
  return item_fields(heap, &fields.value, state, line);
}

/* finish(state, meta, lines): the counts leave out name 0 and level 0. */
static Export finish(ExportState state, ExportMetadata meta, uint32_t lines) {
  Export source;
  source.meta = meta;
  source.totals.lines = lines;
  source.totals.names = nat_sub_count(table_size(state_names(&state)), 1);
  source.totals.levels = nat_sub_count(table_size(state_levels(&state)), 1);
  source.totals.exprs = table_size(state_exprs(&state));
  source.totals.declarations = table_size(state_decls(&state));
  source.state = state;
  return source;
}

/* read_remaining(lines, meta, state, previous) from the line at INDEX: Str* items */
static ExportResult read_remaining(Heap *heap, const Vec *lines, size_t index, ExportMetadata meta, ExportState *state,
                                   uint32_t previous) {
  VecGet got = vec_get(lines, index);
  if (got.status != VEC_GET_OK) return export_done(finish(*state, meta, previous));
  uint32_t line = previous + 1u;
  JsonResult value = json_parse(heap, unbox_text(got.value));
  if (value.kind == JSON_FAIL) return export_fail(at_line(value.message, line));
  StateResult next = item(heap, value.value, state, line);
  if (next.kind == JSON_FAIL) return export_fail(at_line(next.message, line));
  return read_remaining(heap, lines, index + 1, meta, state, line);
}

/* read_lines(lines) */
static ExportResult read_lines(Heap *heap, const Vec *lines) {
  VecGet first = vec_get(lines, 0);
  if (first.status != VEC_GET_OK) return export_fail(at_line(str_lit("missing initial metadata"), 1));
  JsonResult value = json_parse(heap, unbox_text(first.value));
  if (value.kind == JSON_FAIL) return export_fail(at_line(value.message, 1));
  MetadataResult meta = parse_metadata(heap, value.value);
  if (meta.kind == JSON_FAIL) return export_fail(at_line(meta.message, 1));
  ExportState state = initial(heap);
  return read_remaining(heap, lines, 1, meta.value, &state, 1);
}

/* split_lines(source, reversed, lines) from byte START: the segments between '\n' bytes, in order. */
static Vec split_lines(Heap *heap, Str source, size_t start, Vec lines) {
  StrSlice rest = str_slice(source, start, source.len);
  if (rest.status != STR_INDEX_OK) return lines;
  StrFind newline = str_find(rest.value, str_lit("\n"));
  if (!newline.found) return with_text(heap, lines, rest.value);
  StrSlice head = str_slice(rest.value, 0, newline.index);
  if (head.status != STR_INDEX_OK) return lines;
  return split_lines(heap, source, start + newline.index + 1, with_text(heap, lines, head.value));
}

/* trailing_line(lines): a final empty segment (the text ends with '\n') is dropped. */
static Vec trailing_line(Heap *heap, Vec lines) {
  VecGet last = vec_get(&lines, lines.len == 0 ? 0 : lines.len - 1);
  if (lines.len == 0 || last.status != VEC_GET_OK) return lines;
  if (unbox_text(last.value).len != 0) return lines;
  return prefix_from(heap, vec_new(heap->arena), &lines, 0, lines.len - 1);
}

/* read_string(source) */
ExportResult export_read_string(Heap *heap, Str source) {
  Vec lines = trailing_line(heap, split_lines(heap, source, 0, vec_new(heap->arena)));
  return read_lines(heap, &lines);
}

/* metadata(source) */
ExportMetadata export_metadata(const Export *source) { return source->meta; }

/* counts(source) */
ExportCounts export_counts(const Export *source) { return source->totals; }

/* source_state(source) */
static const ExportState *source_state(const Export *source) { return &source->state; }

/* name_node(row) */
static ExportName name_node(TableGet row) {
  ExportName result;
  const ExportNameRow *found = row.value;
  result.present = row.present;
  result.value = row.present ? found->node : anonymous_name();
  return result;
}

/* name(source, index) */
ExportName export_name(const Export *source, Str index) {
  return name_node(table_get(state_names(source_state(source)), index));
}

/* name_display(row) */
static ExportText name_display(TableGet row) {
  ExportText result;
  const ExportNameRow *found = row.value;
  result.present = row.present;
  result.value = row.present ? found->text : str_empty();
  return result;
}

/* name_string(source, index) */
ExportText export_name_string(const Export *source, Str index) {
  return name_display(table_get(state_names(source_state(source)), index));
}

/* name_canonical(row) */
static ExportText name_canonical(TableGet row) {
  ExportText result;
  const ExportNameRow *found = row.value;
  result.present = row.present;
  result.value = row.present ? found->canonical : str_empty();
  return result;
}

/* canonical_name(source, index) */
ExportText export_canonical_name(const Export *source, Str index) {
  return name_canonical(table_get(state_names(source_state(source)), index));
}

/* level(source, index) */
ExportLevel export_level(const Export *source, Str index) {
  ExportLevel result;
  TableGet row = table_get(state_levels(source_state(source)), index);
  const Level *found = row.value;
  result.present = row.present;
  result.value = row.present ? *found : zero_level();
  return result;
}

/* expr_node(row) */
static ExportExpr expr_node(TableGet row) {
  ExportExpr result;
  const ExportExprRow *found = row.value;
  result.present = row.present;
  result.value = row.present ? found->node : no_expr();
  return result;
}

/* expr(source, index) */
ExportExpr export_expr(const Export *source, Str index) {
  return expr_node(table_get(state_exprs(source_state(source)), index));
}

/* expr_source_line(row) */
static ExportLine expr_source_line(TableGet row) {
  ExportLine result;
  const ExportExprRow *found = row.value;
  result.present = row.present;
  result.value = row.present ? found->line : 0;
  return result;
}

/* expr_line(source, index) */
ExportLine export_expr_line(const Export *source, Str index) {
  return expr_source_line(table_get(state_exprs(source_state(source)), index));
}

/* expressions(source): the Bend list is prepended and reversed; the Vec is already in source order. */
Vec export_expressions(const Export *source) { return *state_expr_order(source_state(source)); }

/* declaration_named_lookup(row, decls) */
static ExportDeclaration declaration_named_lookup(TableGet row, const Table *decls) {
  ExportDeclaration result;
  const ExportNameRow *name = row.value;
  TableGet found = row.present ? table_get(decls, name->key) : row;
  const DeclEntry *declaration = found.value;
  result.present = found.present;
  result.value = found.present ? *declaration : no_declaration();
  return result;
}

/* declaration(source, index) */
ExportDeclaration export_declaration(const Export *source, Str index) {
  return declaration_named_lookup(table_get(state_names(source_state(source)), index),
                                  state_decls(source_state(source)));
}

/* declarations(source) */
Vec export_declarations(const Export *source) { return *state_decl_order(source_state(source)); }

/* groups(source) */
Vec export_groups(const Export *source) { return *state_groups(source_state(source)); }

/* index_order_length(order, a, b): equal lengths fall back to codepoint order. */
static Cmp index_order_length(Cmp order, Str a, Str b) {
  switch (order) {
    case CMP_LT: return order;
    case CMP_EQ: return json_lexical(a, b);
    case CMP_GT: return order;
  }
  return order;
}

/* Nat.cmp of two counts */
static Cmp cmp_count(size_t a, size_t b) {
  if (a < b) return CMP_LT;
  if (a > b) return CMP_GT;
  return CMP_EQ;
}

/* Json.string_length(text): the codepoint count of UTF-8 text, from byte INDEX. */
static size_t chars_from(Str text, size_t index, size_t count) {
  StrAt at = str_at(text, index);
  if (at.status != STR_INDEX_OK) return count;
  return chars_from(text, index + 1, (at.value & 0xC0) == 0x80 ? count : count + 1);
}

/* compare_index(a, b): shorter index texts first, then codepoint order. */
static Cmp compare_index(Str a, Str b) {
  return index_order_length(cmp_count(chars_from(a, 0, 0), chars_from(b, 0, 0)), a, b);
}

/* split_prepend(a, b, rest): a joins the left half, b the right half. */
static KeySplit split_prepend(Heap *heap, void *a, void *b, KeySplit rest) {
  KeySplit split;
  split.left = json_vec_with(heap, rest.left, a);
  split.right = json_vec_with(heap, rest.right, b);
  return split;
}

/* split_keys(keys) from INDEX: alternate items into the two halves, keeping their order. */
static KeySplit split_from(Heap *heap, const Vec *keys, size_t index, KeySplit acc) {
  VecGet one = vec_get(keys, index);
  VecGet two = vec_get(keys, index + 1);
  if (one.status != VEC_GET_OK) return acc;
  if (two.status != VEC_GET_OK) {
    acc.left = json_vec_with(heap, acc.left, one.value);
    return acc;
  }
  return split_from(heap, keys, index + 2, split_prepend(heap, one.value, two.value, acc));
}

/* split_keys(keys) */
static KeySplit split_keys(Heap *heap, const Vec *keys) {
  KeySplit empty;
  empty.left = vec_new(heap->arena);
  empty.right = vec_new(heap->arena);
  return split_from(heap, keys, 0, empty);
}

static Vec merge_from(Heap *heap, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc);

/* merge_choice(order, left, left_rest, right, right_rest): GT takes the right head first. */
static Vec merge_choice(Heap *heap, Cmp order, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc) {
  VecGet a = vec_get(left, li);
  VecGet b = vec_get(right, ri);
  if (order == CMP_GT) return merge_from(heap, left, li, right, ri + 1, json_vec_with(heap, acc, b.value));
  return merge_from(heap, left, li + 1, right, ri, json_vec_with(heap, acc, a.value));
}

/* merge_keys(left, right) from the heads at LI and RI */
static Vec merge_from(Heap *heap, const Vec *left, size_t li, const Vec *right, size_t ri, Vec acc) {
  VecGet a = vec_get(left, li);
  VecGet b = vec_get(right, ri);
  if (a.status != VEC_GET_OK) return append_from(heap, acc, right, ri);
  if (b.status != VEC_GET_OK) return append_from(heap, acc, left, li);
  return merge_choice(heap, compare_index(unbox_text(a.value), unbox_text(b.value)), left, li, right, ri, acc);
}

/* merge_keys(left, right) */
static Vec merge_keys(Heap *heap, const Vec *left, const Vec *right) {
  return merge_from(heap, left, 0, right, 0, vec_new(heap->arena));
}

Vec export_sort_keys(Heap *heap, const Vec *keys);

/* sort_split(split) */
static Vec sort_split(Heap *heap, KeySplit split) {
  Vec left = export_sort_keys(heap, &split.left);
  Vec right = export_sort_keys(heap, &split.right);
  return merge_keys(heap, &left, &right);
}

/* sort_keys(keys): merge sort by compare_index. */
Vec export_sort_keys(Heap *heap, const Vec *keys) {
  if (keys->len == 0) return vec_new(heap->arena);
  if (keys->len == 1) return append_from(heap, vec_new(heap->arena), keys, 0);
  return sort_split(heap, split_keys(heap, keys));
}

/* name_indexed(row, index, rest): Indexed{index, node} joins the list when the row is present. */
static Vec name_indexed(Heap *heap, TableGet row, Str index, Vec rest) {
  ExportNameRow *found = row.value;
  if (!row.present) return rest;
  return with_item(heap, rest, table_indexed(heap, index, &found->node));
}

/* ordered_names(keys, table) from INDEX: Str* keys */
static Vec ordered_names(Heap *heap, const Vec *keys, size_t index, const Table *table, Vec acc) {
  VecGet got = vec_get(keys, index);
  if (got.status != VEC_GET_OK) return acc;
  Str key = unbox_text(got.value);
  return ordered_names(heap, keys, index + 1, table, name_indexed(heap, table_get(table, key), key, acc));
}

/* names(source) */
Vec export_names(Heap *heap, const Export *source) {
  const Table *table = state_names(source_state(source));
  Vec keys = table_keys(heap, table);
  Vec sorted = export_sort_keys(heap, &keys);
  return ordered_names(heap, &sorted, 0, table, vec_new(heap->arena));
}
