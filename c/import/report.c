/* Port of bend2/import/report.bend: the parity report over a translated export. Every declaration
 * row is lowered with the missing-prelude resolver and the default budget (prepare), the summary
 * counts the outcomes per declaration kind, and files renders manifest.json, types.ndjson and
 * summary.txt. Every definition keeps its Bend name with the report_ prefix, in Bend order; the
 * Bend list recursions are index walkers with an accumulator (Vec items in order). A NULL item is a
 * failed allocation (heap->oom set) and is not stored, like the other import modules. */
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

/* a ++ b; a short result is a failed allocation */
static Str cat(Heap *heap, Str a, Str b) {
  Str out = str_cat(heap->arena, a, b);
  if (out.len < a.len + b.len) heap->oom = true;
  return out;
}

/* Nat.show and U32.show of a count */
static Str show_size(Heap *heap, size_t value) { return str_of_u64(heap->arena, (uint64_t)value); }

/* String.concat(parts) from INDEX */
static Str concat_from(Heap *heap, const Str *parts, size_t count, size_t index, Str acc) {
  if (index >= count) return acc;
  return concat_from(heap, parts, count, index + 1, cat(heap, acc, parts[index]));
}

static Str *box_str(Heap *heap, Str value) { return json_boxed_str(heap, value); }

static ReportEntry *box_entry(Heap *heap, ReportEntry value) {
  ReportEntry *box = heap_alloc(heap, sizeof(ReportEntry));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

static ReportOutputFile *box_file(Heap *heap, Str name, Str contents) {
  ReportOutputFile *box = heap_alloc(heap, sizeof(ReportOutputFile));
  if (box == NULL) return NULL;
  box->name = name;
  box->contents = contents;
  return box;
}

/* outcome(value) */
ReportOutcome report_outcome(Heap *heap, LowerResult value) {
  ReportOutcome out;
  out.kind = REPORT_KERNEL;
  out.term = NULL;
  out.detail = str_empty();
  switch (value.kind) {
  case LOWER_DONE:
    out.term = value.value;
    return out;
  case LOWER_FAIL:
    switch (value.error.kind) {
    case LOWER_DEFERRED:
      out.kind = REPORT_DEFERRED;
      out.detail = value.error.message;
      return out;
    case LOWER_UNSUPPORTED:
      out.kind = REPORT_UNSUPPORTED;
      out.detail = value.error.message;
      return out;
    case LOWER_KERNEL_ERROR:
      out.kind = REPORT_FAILED;
      out.detail = error_to_string(heap, value.error.error);
      return out;
    }
  }
  return out;
}

/* prepare_entries(rows, table) from INDEX: rows are TranslateRow* items; every row gets a fresh
 * missing-prelude resolver and the initial globals, as the Bend does per cons cell */
static Vec prepare_entries_from(Heap *heap, const Vec *rows, size_t index, Translated *table, Vec out) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return out;
  const TranslateRow *row = got.value;
  ReportEntry entry;
  entry.row = row;
  entry.outcome = report_outcome(
      heap, lower_default(heap, lower_missing(heap, str_lit("no checked prelude mapping for ")), global_initial(heap),
                          table, row));
  return prepare_entries_from(heap, rows, index + 1, table, with_item(heap, out, box_entry(heap, entry)));
}

Vec report_prepare_entries(Heap *heap, const Vec *rows, Translated *table) {
  return prepare_entries_from(heap, rows, 0, table, vec_new(heap->arena));
}

/* prepare(source) */
ReportResult report_prepare(Heap *heap, const Export *source) {
  TranslateResult table = translate_translate(heap, source);
  ReportResult out;
  out.kind = JSON_FAIL;
  out.value.source = source;
  out.value.types = table.value;
  out.value.entries = vec_new(heap->arena);
  out.error = table.error;
  switch (table.kind) {
  case JSON_FAIL:
    return out;
  case JSON_DONE: {
    Vec rows = translate_rows(table.value);
    out.kind = JSON_DONE;
    out.value.entries = report_prepare_entries(heap, &rows, table.value);
    return out;
  }
  }
  return out;
}

/* namespace(name, prefix) */
bool report_namespace(Heap *heap, Str name, Str prefix) {
  return str_eq(name, prefix) || str_starts_with(name, cat(heap, prefix, str_lit(".")));
}

/* external_name(name) */
bool report_external_name(Heap *heap, Str name) {
  return !(report_namespace(heap, name, str_lit("UnifiedAggregation")) ||
           report_namespace(heap, name, str_lit("ArrowCat")) || report_namespace(heap, name, str_lit("CompCatTheory")));
}

/* canonical_constant(value, names) */
TranslateIdSet report_canonical_constant(Heap *heap, ExportText value, TranslateIdSet names) {
  if (!value.present) return names;
  return translate_set_add(heap, names, value.value);
}

/* constant(node, source, names) */
TranslateIdSet report_constant(Heap *heap, const Expr *node, const Export *source, TranslateIdSet names) {
  switch (node->kind) {
  case EXPR_CONST:
    return report_canonical_constant(heap, export_canonical_name(source, node->name), names);
  case EXPR_BVAR:
  case EXPR_SORT:
  case EXPR_APPLY:
  case EXPR_LAM:
  case EXPR_FORALL:
  case EXPR_LET:
  case EXPR_PROJ:
  case EXPR_NAT_LITERAL:
  case EXPR_STRING_LITERAL:
  case EXPR_MDATA:
    return names;
  }
  return names;
}

/* constants(rows, source, names) from INDEX: rows are TableIndexed* items over Expr values */
static TranslateIdSet constants_from(Heap *heap, const Vec *rows, size_t index, const Export *source,
                                     TranslateIdSet names) {
  VecGet got = vec_get(rows, index);
  if (got.status != VEC_GET_OK) return names;
  const TableIndexed *row = got.value;
  return constants_from(heap, rows, index + 1, source, report_constant(heap, row->value, source, names));
}

TranslateIdSet report_constants(Heap *heap, const Vec *rows, const Export *source, TranslateIdSet names) {
  return constants_from(heap, rows, 0, source, names);
}

/* constant_names(source) */
TranslateIdSet report_constant_names(Heap *heap, const Export *source) {
  Vec rows = export_expressions(source);
  return report_constants(heap, &rows, source, translate_empty_set(heap));
}

/* status(outcome) */
Str report_status(ReportOutcome outcome) {
  switch (outcome.kind) {
  case REPORT_KERNEL:
    return str_lit("KERNEL_TYPE");
  case REPORT_DEFERRED:
    return str_lit("DEFERRED");
  case REPORT_UNSUPPORTED:
    return str_lit("UNSUPPORTED");
  case REPORT_FAILED:
    return str_lit("KERNEL_ERROR");
  }
  return str_lit("KERNEL_ERROR");
}

/* kind(entry) */
Str report_kind(const ReportEntry *entry) { return decl_kind_name(&entry->row->declaration.kind); }

/* entry_status(entry) */
Str report_entry_status(const ReportEntry *entry) { return report_status(entry->outcome); }

/* increment(yes, n) */
size_t report_increment(bool yes, size_t n) { return yes ? n + 1 : n; }

/* count_status(wanted, entries) from INDEX: entries are ReportEntry* items */
static size_t count_status_from(Str wanted, const Vec *entries, size_t index, size_t n) {
  VecGet got = vec_get(entries, index);
  if (got.status != VEC_GET_OK) return n;
  const ReportEntry *entry = got.value;
  return count_status_from(wanted, entries, index + 1, report_increment(str_eq(wanted, report_entry_status(entry)), n));
}

size_t report_count_status(Str wanted, const Vec *entries) { return count_status_from(wanted, entries, 0, 0); }

/* keep(yes, entry, rest): the Bend conses onto the filtered tail; the walker appends to the kept prefix */
Vec report_keep(Heap *heap, bool yes, ReportEntry *entry, Vec kept) {
  if (!yes) return kept;
  return with_item(heap, kept, entry);
}

/* filter_kind(wanted, entries) from INDEX */
static Vec filter_kind_from(Heap *heap, Str wanted, const Vec *entries, size_t index, Vec kept) {
  VecGet got = vec_get(entries, index);
  if (got.status != VEC_GET_OK) return kept;
  ReportEntry *entry = got.value;
  return filter_kind_from(heap, wanted, entries, index + 1,
                          report_keep(heap, str_eq(wanted, report_kind(entry)), entry, kept));
}

Vec report_filter_kind(Heap *heap, Str wanted, const Vec *entries) {
  return filter_kind_from(heap, wanted, entries, 0, vec_new(heap->arena));
}

/* external_display(value) */
bool report_external_display(Heap *heap, ExportText value) {
  if (!value.present) return false;
  return report_external_name(heap, value.value);
}

/* count_referenced(ids, source) from INDEX: ids are Str* items */
static size_t count_referenced_from(Heap *heap, const Vec *ids, size_t index, const Export *source, size_t n) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return n;
  Str id = *(Str *)got.value;
  return count_referenced_from(heap, ids, index + 1, source,
                               report_increment(report_external_display(heap, export_name_string(source, id)), n));
}

size_t report_count_referenced(Heap *heap, const Vec *ids, const Export *source) {
  return count_referenced_from(heap, ids, 0, source, 0);
}

/* count_declared(entries) from INDEX */
static size_t count_declared_from(Heap *heap, const Vec *entries, size_t index, size_t n) {
  VecGet got = vec_get(entries, index);
  if (got.status != VEC_GET_OK) return n;
  const ReportEntry *entry = got.value;
  return count_declared_from(heap, entries, index + 1, report_increment(report_external_name(heap, entry->row->name), n));
}

size_t report_count_declared(Heap *heap, const Vec *entries) { return count_declared_from(heap, entries, 0, 0); }

/* grammar(counts) */
Str report_grammar(Heap *heap, ExportCounts counts) {
  Str parts[9];
  parts[0] = str_lit("IMPORT-GRAMMAR lines=");
  parts[1] = show_size(heap, counts.lines);
  parts[2] = str_lit(" names=");
  parts[3] = show_size(heap, counts.names);
  parts[4] = str_lit(" levels=");
  parts[5] = show_size(heap, counts.levels);
  parts[6] = str_lit(" expressions=");
  parts[7] = show_size(heap, counts.exprs);
  parts[8] = str_lit(" format=3.1.0");
  return concat_from(heap, parts, 9, 0, str_empty());
}

/* declaration_count(counts) */
size_t report_declaration_count(ExportCounts counts) { return counts.declarations; }

/* kind_summary(name, entries) */
Str report_kind_summary(Heap *heap, Str name, const Vec *entries) {
  Vec kept = report_filter_kind(heap, name, entries);
  Str declared = show_size(heap, kept.len);
  Str parts[15];
  parts[0] = str_lit("PARITY-KIND ");
  parts[1] = name;
  parts[2] = str_lit(" declared=");
  parts[3] = declared;
  parts[4] = str_lit(" scoped_types=");
  parts[5] = declared;
  parts[6] = str_lit(" kernel_types=");
  parts[7] = show_size(heap, report_count_status(str_lit("KERNEL_TYPE"), &kept));
  parts[8] = str_lit(" deferred=");
  parts[9] = show_size(heap, report_count_status(str_lit("DEFERRED"), &kept));
  parts[10] = str_lit(" unsupported=");
  parts[11] = show_size(heap, report_count_status(str_lit("UNSUPPORTED"), &kept));
  parts[12] = str_lit(" kernel_errors=");
  parts[13] = show_size(heap, report_count_status(str_lit("KERNEL_ERROR"), &kept));
  parts[14] = str_lit(" NEVER=0");
  return concat_from(heap, parts, 15, 0, str_empty());
}

/* kind_summaries(kinds, entries) from INDEX: kinds are Str* items; the Nil case appends the status line */
static Vec kind_summaries_from(Heap *heap, const Vec *kinds, size_t index, const Vec *entries, Vec out) {
  VecGet got = vec_get(kinds, index);
  if (got.status != VEC_GET_OK) {
    return with_item(heap, out,
                     box_str(heap, str_lit("PARITY-STATUS mapping=not-checked values=M1 NEVER=0 (no NEVER ledger applied)")));
  }
  Str name = *(Str *)got.value;
  return kind_summaries_from(heap, kinds, index + 1, entries,
                             with_item(heap, out, box_str(heap, report_kind_summary(heap, name, entries))));
}

Vec report_kind_summaries(Heap *heap, const Vec *kinds, const Vec *entries) {
  return kind_summaries_from(heap, kinds, 0, entries, vec_new(heap->arena));
}

/* the kind list of summary(report), in Bend order */
static const char *const summary_kinds[8] = {"axiom", "def", "thm", "opaque", "quot", "inductive", "constructor",
                                             "recursor"};

static Vec summary_kind_names(Heap *heap, size_t index, Vec out) {
  if (index >= 8) return out;
  return summary_kind_names(heap, index + 1, with_item(heap, out, box_str(heap, str_lit(summary_kinds[index]))));
}

/* summary(report) */
Vec report_summary(Heap *heap, const Report *report) {
  TranslateIdSet names = report_constant_names(heap, report->source);
  Vec ids = translate_set_keys(heap, &names);
  ExportCounts counts = export_counts(report->source);
  Str parts[8];
  parts[0] = str_lit("IMPORT-COUNTS declarations=");
  parts[1] = show_size(heap, report_declaration_count(counts));
  parts[2] = str_lit(" external_referenced=");
  parts[3] = show_size(heap, report_count_referenced(heap, &ids, report->source));
  parts[4] = str_lit(" external_declared=");
  parts[5] = show_size(heap, report_count_declared(heap, &report->entries));
  parts[6] = str_lit(" const_names=");
  parts[7] = show_size(heap, ids.len);
  Str census = concat_from(heap, parts, 8, 0, str_empty());
  Vec kinds = summary_kind_names(heap, 0, vec_new(heap->arena));
  Vec out = vec_new(heap->arena);
  out = with_item(heap, out, box_str(heap, report_grammar(heap, counts)));
  out = with_item(heap, out, box_str(heap, census));
  return kind_summaries_from(heap, &kinds, 0, &report->entries, out);
}

/* detail(outcome) */
JsonField *report_detail(Heap *heap, ReportOutcome outcome) {
  switch (outcome.kind) {
  case REPORT_KERNEL: {
    Vec no_names = vec_new(heap->arena);
    return field(heap, "kernel_debug", json_text(heap, pp_term(heap, &no_names, outcome.term)));
  }
  case REPORT_DEFERRED:
  case REPORT_UNSUPPORTED:
  case REPORT_FAILED:
    return field(heap, "reason", json_text(heap, outcome.detail));
  }
  return NULL;
}

/* entry_record(entry) */
Json *report_entry_record(Heap *heap, const ReportEntry *entry) {
  const TranslateRow *row = entry->row;
  const DeclEntry *decl = &row->declaration;
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "source_name", json_number(heap, decl->name)));
  fields = with_item(heap, fields, field(heap, "name", json_text(heap, row->name)));
  fields = with_item(heap, fields, field(heap, "kind", json_text(heap, decl_kind_name(&decl->kind))));
  fields = with_item(heap, fields, field(heap, "source_line", json_number(heap, show_size(heap, decl->line))));
  fields = with_item(heap, fields, field(heap, "source_level_params", render_numbers(heap, &decl->level_params)));
  fields = with_item(heap, fields, field(heap, "parameters", json_items(heap, render_parameters(heap, &row->parameters))));
  fields = with_item(heap, fields, field(heap, "type", json_number(heap, row->root)));
  fields = with_item(heap, fields, field(heap, "dependencies", render_strings(heap, &row->dependencies)));
  fields = with_item(heap, fields, field(heap, "status", json_text(heap, report_status(entry->outcome))));
  fields = with_item(heap, fields, report_detail(heap, entry->outcome));
  Vec outer = vec_new(heap->arena);
  outer = with_item(heap, outer, field(heap, "declaration", json_object(heap, fields)));
  return json_object(heap, outer);
}

/* entry_records(entries) from INDEX */
static Vec entry_records_from(Heap *heap, const Vec *entries, size_t index, Vec out) {
  VecGet got = vec_get(entries, index);
  if (got.status != VEC_GET_OK) return out;
  return entry_records_from(heap, entries, index + 1, with_item(heap, out, report_entry_record(heap, got.value)));
}

Vec report_entry_records(Heap *heap, const Vec *entries) {
  return entry_records_from(heap, entries, 0, vec_new(heap->arena));
}

/* name_records(names) from INDEX: names are TableIndexed* items over Name values */
static Vec name_records_from(Heap *heap, const Vec *names, size_t index, Vec out) {
  VecGet got = vec_get(names, index);
  if (got.status != VEC_GET_OK) return out;
  return name_records_from(heap, names, index + 1, with_item(heap, out, render_name_record(heap, got.value)));
}

Vec report_name_records(Heap *heap, const Vec *names) { return name_records_from(heap, names, 0, vec_new(heap->arena)); }

/* json_lines(values) from INDEX, appended to one text buffer: every value and then "\n" */
static void json_lines_into(Heap *heap, StrBuf *buf, const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return;
  if (!strbuf_push(buf, json_to_string(heap, got.value))) heap->oom = true;
  if (!strbuf_push_byte(buf, '\n')) heap->oom = true;
  json_lines_into(heap, buf, values, index + 1);
}

Str report_json_lines(Heap *heap, const Vec *values) {
  StrBuf buf = strbuf_new(heap->arena);
  json_lines_into(heap, &buf, values, 0);
  return strbuf_str(&buf);
}

/* manifest(meta, count) */
Json *report_manifest(Heap *heap, ExportMetadata meta, size_t count) {
  Vec fields = vec_new(heap->arena);
  fields = with_item(heap, fields, field(heap, "schema", json_text(heap, str_lit("mechanism-types-0.1"))));
  fields = with_item(heap, fields, field(heap, "format_version", json_text(heap, meta.format_version)));
  fields = with_item(heap, fields, field(heap, "lean_version", json_text(heap, meta.lean_version)));
  fields = with_item(heap, fields, field(heap, "lean_githash", json_text(heap, meta.lean_githash)));
  fields = with_item(heap, fields, field(heap, "exporter_name", json_text(heap, meta.exporter_name)));
  fields = with_item(heap, fields, field(heap, "exporter_version", json_text(heap, meta.exporter_version)));
  fields = with_item(heap, fields, field(heap, "declarations", json_number(heap, show_size(heap, count))));
  fields = with_item(heap, fields, field(heap, "mapping_checked", json_boolean(heap, false)));
  fields = with_item(heap, fields, field(heap, "values_translated", json_boolean(heap, false)));
  return json_object(heap, fields);
}

/* Json.to_string(value) for a value that may be a failed allocation */
static Str json_text_of(Heap *heap, const Json *value) {
  if (value == NULL) return str_empty();
  return json_to_string(heap, value);
}

/* files(report) */
Vec report_files(Heap *heap, const Report *report) {
  Vec names = export_names(heap, report->source);
  Vec levels = translate_levels(heap, report->types);
  Vec nodes = translate_nodes(heap, report->types);
  Vec name_records = report_name_records(heap, &names);
  Vec level_records = render_level_records(heap, &levels);
  Vec node_records = render_node_records(heap, &nodes);
  Vec entry_records = report_entry_records(heap, &report->entries);
  Vec lines = report_summary(heap, report);
  StrBuf types = strbuf_new(heap->arena);
  Str manifest =
      cat(heap, json_text_of(heap, report_manifest(heap, export_metadata(report->source), report->entries.len)),
          str_lit("\n"));
  json_lines_into(heap, &types, &name_records, 0);
  json_lines_into(heap, &types, &level_records, 0);
  json_lines_into(heap, &types, &node_records, 0);
  json_lines_into(heap, &types, &entry_records, 0);
  Vec out = vec_new(heap->arena);
  out = with_item(heap, out, box_file(heap, str_lit("manifest.json"), manifest));
  out = with_item(heap, out, box_file(heap, str_lit("types.ndjson"), strbuf_str(&types)));
  out = with_item(heap, out,
                  box_file(heap, str_lit("summary.txt"), cat(heap, translate_join(heap, &lines, str_lit("\n")), str_lit("\n"))));
  return out;
}
