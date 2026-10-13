/* Port of bend2/tests/import_export.bend (oracle label import-export): reads the one NDJSON source
 * argument with Export.read_string and prints the JSON summary [metadata, counts, names,
 * declarations, expressions, group count]; dies with 1 and "LINE:message" on an import error, or
 * with 64 and the usage text on any other argument count. */
#include "driver.h"
#include "import.h"

/* item appended to out (a NULL item is a failed allocation, heap->oom set) */
static Vec with_json(Heap *heap, Vec out, Json *item) {
  if (item == NULL) return out;
  return json_vec_with(heap, out, item);
}

/* U32.show and Nat.show */
static Str show_u64(Heap *heap, uint64_t value) { return str_of_u64(heap->arena, value); }

/* maybe_text(value) */
static Json *maybe_text(Heap *heap, ExportText value) {
  if (value.present) return json_text(heap, value.value);
  return json_null(heap);
}

/* number_items(values) from INDEX: Json.Number of every Str* item */
static Vec number_items(Heap *heap, const Vec *values, size_t index, Vec out) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return out;
  return number_items(heap, values, index + 1, with_json(heap, out, json_number(heap, *(Str *)got.value)));
}

/* numbers(values) */
static Json *numbers(Heap *heap, const Vec *values) {
  return json_items(heap, number_items(heap, values, 0, vec_new(heap->arena)));
}

/* counts(value) */
static Json *counts(Heap *heap, ExportCounts value) {
  Vec out = vec_new(heap->arena);
  return json_items(
      heap,
      with_json(heap,
                with_json(heap,
                          with_json(heap,
                                    with_json(heap, with_json(heap, out, json_number(heap, show_u64(heap, value.lines))),
                                              json_number(heap, show_u64(heap, value.names))),
                                    json_number(heap, show_u64(heap, value.levels))),
                          json_number(heap, show_u64(heap, value.exprs))),
                json_number(heap, show_u64(heap, value.declarations))));
}

/* names(rows, source) from INDEX: TableIndexed* rows */
static Vec names(Heap *heap, const Vec *rows, size_t index, const Export *source, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  const TableIndexed *row = got.value;
  Vec fields = with_json(heap, with_json(heap, with_json(heap, vec_new(heap->arena), json_number(heap, row->index)),
                                         maybe_text(heap, export_name_string(source, row->index))),
                         maybe_text(heap, export_canonical_name(source, row->index)));
  return names(heap, rows, index + 1, source, with_json(heap, out, json_items(heap, fields)));
}

/* decl(declaration) */
static Json *decl(Heap *heap, const DeclEntry *declaration) {
  Vec name_refs = decl_name_references(heap, declaration);
  Vec expr_refs = decl_expr_references(heap, declaration);
  Vec out = vec_new(heap->arena);
  out = with_json(heap, out, json_number(heap, declaration->name));
  out = with_json(heap, out, json_text(heap, decl_kind_name(&declaration->kind)));
  out = with_json(heap, out, json_number(heap, declaration->typ));
  out = with_json(heap, out, numbers(heap, &declaration->level_params));
  out = with_json(heap, out, numbers(heap, &name_refs));
  out = with_json(heap, out, numbers(heap, &expr_refs));
  out = with_json(heap, out, json_number(heap, show_u64(heap, declaration->line)));
  return json_items(heap, out);
}

/* decls(rows) from INDEX: DeclEntry* rows */
static Vec decls(Heap *heap, const Vec *rows, size_t index, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  return decls(heap, rows, index + 1, with_json(heap, out, decl(heap, got.value)));
}

/* expr_kind(value) */
static Str expr_kind(const Expr *value) {
  switch (value->kind) {
    case EXPR_BVAR: return str_lit("bvar");
    case EXPR_SORT: return str_lit("sort");
    case EXPR_CONST: return str_lit("const");
    case EXPR_APPLY: return str_lit("app");
    case EXPR_LAM: return str_lit("lam");
    case EXPR_FORALL: return str_lit("forallE");
    case EXPR_LET: return str_lit("letE");
    case EXPR_PROJ: return str_lit("proj");
    case EXPR_NAT_LITERAL: return str_lit("natVal");
    case EXPR_STRING_LITERAL: return str_lit("strVal");
    case EXPR_MDATA: return str_lit("mdata");
  }
  return str_lit("bvar");
}

/* exprs(rows) from INDEX: TableIndexed* rows (value Expr*) */
static Vec exprs(Heap *heap, const Vec *rows, size_t index, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  const TableIndexed *row = got.value;
  const Expr *node = row->value;
  Vec name_refs = expr_name_references(heap, node);
  Vec level_refs = expr_level_references(heap, node);
  Vec expr_refs = expr_expr_references(heap, node);
  Vec fields = vec_new(heap->arena);
  fields = with_json(heap, fields, json_number(heap, row->index));
  fields = with_json(heap, fields, json_text(heap, expr_kind(node)));
  fields = with_json(heap, fields, numbers(heap, &name_refs));
  fields = with_json(heap, fields, numbers(heap, &level_refs));
  fields = with_json(heap, fields, numbers(heap, &expr_refs));
  return exprs(heap, rows, index + 1, with_json(heap, out, json_items(heap, fields)));
}

/* metadata(value) */
static Json *metadata(Heap *heap, ExportMetadata value) {
  Vec out = vec_new(heap->arena);
  out = with_json(heap, out, json_text(heap, value.exporter_name));
  out = with_json(heap, out, json_text(heap, value.exporter_version));
  out = with_json(heap, out, json_text(heap, value.lean_githash));
  out = with_json(heap, out, json_text(heap, value.lean_version));
  out = with_json(heap, out, json_text(heap, value.format_version));
  return json_items(heap, out);
}

/* summary(source) */
static Str summary(Heap *heap, const Export *source) {
  Vec name_rows = export_names(heap, source);
  Vec decl_rows = export_declarations(source);
  Vec expr_rows = export_expressions(source);
  Vec group_rows = export_groups(source);
  Vec out = vec_new(heap->arena);
  out = with_json(heap, out, metadata(heap, export_metadata(source)));
  out = with_json(heap, out, counts(heap, export_counts(source)));
  out = with_json(heap, out, json_items(heap, names(heap, &name_rows, 0, source, vec_new(heap->arena))));
  out = with_json(heap, out, json_items(heap, decls(heap, &decl_rows, 0, vec_new(heap->arena))));
  out = with_json(heap, out, json_items(heap, exprs(heap, &expr_rows, 0, vec_new(heap->arena))));
  out = with_json(heap, out, json_number(heap, show_u64(heap, group_rows.len)));
  return json_to_string(heap, json_items(heap, out));
}

/* result(value) */
static int result(Heap *heap, ExportResult value) {
  if (value.kind == JSON_DONE) return driver_print(summary(heap, &value.value));
  return driver_die_text(1, heap_str_cat(heap, heap_str_cat(heap, show_u64(heap, value.error.line), str_lit(":")),
                                         value.error.message));
}

/* arguments(args): the arguments after the program name. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 2) return result(heap, export_read_string(heap, str_lit(argv[1])));
  return driver_die(64, "usage: import_export NDJSON");
}

/* main() */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap, argc, argv);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
