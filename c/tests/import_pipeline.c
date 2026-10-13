/* Port of bend2/tests/import_pipeline.bend (oracle label import-pipeline): MODE NDJSON TARGETS.
 * Reads the NDJSON source with Export.read_string, then dispatches on MODE: report prints the parity
 * report (summary strings and entry records) as one JSON line; mapping parses TARGETS as a JSON array
 * of [name, target] pairs and prints the inventory summary (map TSV, never TSV, declared, constants);
 * mapping-foundation does the same over Mapping.foundation_targets; every other mode translates the
 * export, finds the declaration named test and lowers its type with the resolver and budget the mode
 * selects (lower-univ, lower-var, lower-global, lower-nat, lower-probe; lower-zero, lower-one), then
 * prints [TAG, text] with TAG one of KERNEL_TYPE, DEFERRED, UNSUPPORTED, KERNEL_ERROR. Dies with 1 and
 * "LINE:message" on an import error, with 1 and the message on a target or mapping error, with 1 and
 * "missing test declaration" when no row is named test, and with 64 and the usage text on any other
 * argument count. */
#include "driver.h"
#include "import.h"

/* print_json(value) */
static int print_json(Heap *heap, const Json *value) { return driver_print(json_to_string(heap, value)); }

/* export_error(error): IO.die(1, U32.show(line) ++ ":" ++ message) */
static int export_error(Heap *heap, ExportError error) {
  return driver_die_text(
      1, heap_str_cat(heap, heap_str_cat(heap, str_of_u64(heap->arena, error.line), str_lit(":")), error.message));
}

/* report_result(result): [Render.strings(Report.summary(report)), Items{Report.entry_records(entries)}] */
static int report_result(Heap *heap, ReportResult result) {
  if (result.kind == JSON_FAIL) return export_error(heap, result.error);
  Vec summary = report_summary(heap, &result.value);
  Vec records = report_entry_records(heap, &result.value.entries);
  Vec parts = vec_new(heap->arena);
  vec_push(&parts, render_strings(heap, &summary));
  vec_push(&parts, json_items(heap, records));
  return print_json(heap, json_items(heap, parts));
}

/* mapping_summary(inventory): [Text{map_tsv}, Text{never_tsv}, Number{declared}, Number{constants}] */
static Json *mapping_summary(Heap *heap, const MappingInventory *inventory) {
  Vec parts = vec_new(heap->arena);
  vec_push(&parts, json_text(heap, mapping_map_tsv(heap, inventory)));
  vec_push(&parts, json_text(heap, mapping_never_tsv(heap, inventory)));
  vec_push(&parts, json_number(heap, str_of_u64(heap->arena, inventory->external_declared)));
  vec_push(&parts, json_number(heap, str_of_u64(heap->arena, inventory->const_names)));
  return json_items(heap, parts);
}

/* mapping_result(result) */
static int mapping_result(Heap *heap, MappingResult result) {
  if (result.kind == JSON_FAIL) return driver_die_text(1, result.message);
  return print_json(heap, mapping_summary(heap, &result.value));
}

/* text_item(value, index): the JSON_TEXT item at index of an Items node, or NULL */
static const Json *text_item(const Json *value, size_t index) {
  VecGet got = vec_get(&value->values, index);
  if (got.status != VEC_GET_OK) return NULL;
  const Json *item = got.value;
  return item->kind == JSON_TEXT ? item : NULL;
}

/* target(value): Items{[Text{name}, Text{target}]} -> Target{name, target}; NULL = "expected target pair" */
static MappingTarget *target(Heap *heap, const Json *value) {
  if (value->kind != JSON_ITEMS || value->values.len != 2) return NULL;
  const Json *name = text_item(value, 0);
  const Json *destination = text_item(value, 1);
  if (name == NULL || destination == NULL) return NULL;
  MappingTarget *box = heap_alloc(heap, sizeof(MappingTarget));
  if (box == NULL) return NULL;
  box->lean_name = name->text;
  box->target = destination->text;
  return box;
}

/* Result<String, List<Mapping.Target>>: value holds MappingTarget* items */
typedef struct {
  JsonResultKind kind;
  Vec value;
  Str message;
} TargetRows;

/* target_rows(items) and targets(value) */
static TargetRows targets(Heap *heap, const Json *value) {
  TargetRows out;
  out.kind = JSON_FAIL;
  out.value = vec_new(heap->arena);
  out.message = str_lit("expected targets array");
  if (value->kind != JSON_ITEMS) return out;
  size_t i = 0;
  for (i = 0; i < value->values.len; i += 1) {
    VecGet got = vec_get(&value->values, i);
    MappingTarget *row = got.status == VEC_GET_OK ? target(heap, got.value) : NULL;
    if (row == NULL) {
      out.message = str_lit("expected target pair");
      return out;
    }
    vec_push(&out.value, row);
  }
  out.kind = JSON_DONE;
  return out;
}

/* parsed_targets(result, source) */
static int parsed_targets(Heap *heap, JsonResult result, const Export *source) {
  if (result.kind == JSON_FAIL) return driver_die_text(1, result.message);
  TargetRows rows = targets(heap, result.value);
  if (rows.kind == JSON_FAIL) return driver_die_text(1, rows.message);
  return mapping_result(heap, mapping_inventory(heap, source, &rows.value));
}

/* probe_result(valid): (Lower.missing("extra resolution: "), Done{Univ{zero}} | Fail{...}) */
static LowerResolved probe_result(Heap *heap, bool valid) {
  LowerResolved out;
  out.next = lower_missing(heap, str_lit("extra resolution: "));
  out.ok = valid;
  out.message = valid ? str_lit("") : str_lit("wrong source reference or universe substitution");
  out.term = valid ? term_univ(heap, level_zero()) : NULL;
  return out;
}

/* level_at(levels, index): the const LevelVar * item, or NULL */
static const LevelVar *level_at(const Vec *levels, size_t index) {
  VecGet got = vec_get(levels, index);
  return got.status == VEC_GET_OK ? (const LevelVar *)got.value : NULL;
}

/* probe_call(name, levels): exactly two levels, name F, a = succ(imax(u1, u0)), b = max(u1, u0) */
static LowerResolved probe_call(Heap *heap, void *ctx, Str name, const Vec *levels) {
  (void)ctx;
  if (levels->len != 2) return probe_result(heap, false);
  const LevelVar *a = level_at(levels, 0);
  const LevelVar *b = level_at(levels, 1);
  if (a == NULL || b == NULL) return probe_result(heap, false);
  const LevelVar *u = level_var(heap, nat_of_u64(heap->arena, 1));
  const LevelVar *v = level_var(heap, nat_zero());
  bool named = str_eq(name, str_lit("F"));
  bool first = level_equal(heap, a, level_succ(heap, level_imax(heap, u, v)));
  bool second = level_equal(heap, b, level_max(heap, u, v));
  return probe_result(heap, named && first && second);
}

/* resolver(mode) */
static LowerResolver resolver(Heap *heap, Str mode) {
  if (str_eq(mode, str_lit("lower-univ"))) return lower_fixed(term_univ(heap, level_zero()));
  if (str_eq(mode, str_lit("lower-var"))) return lower_fixed(term_var(heap, nat_zero()));
  if (str_eq(mode, str_lit("lower-global"))) return lower_fixed(term_global(heap, str_lit("uninstalled")));
  if (str_eq(mode, str_lit("lower-nat"))) return lower_fixed(prim_nat_ty(heap));
  if (str_eq(mode, str_lit("lower-probe"))) {
    LowerResolver probe;
    probe.call = probe_call;
    probe.ctx = NULL;
    return probe;
  }
  return lower_missing(heap, str_lit("no checked mapping for "));
}

/* row_name(row) */
static Str row_name(const TranslateRow *row) { return row->name; }

/* test_row(rows): the first row named test, or NULL */
static const TranslateRow *test_row(const Vec *rows) {
  size_t i = 0;
  for (i = 0; i < rows->len; i += 1) {
    VecGet got = vec_get(rows, i);
    const TranslateRow *row = got.status == VEC_GET_OK ? (const TranslateRow *)got.value : NULL;
    if (row != NULL && str_eq(row_name(row), str_lit("test"))) return row;
  }
  return NULL;
}

/* tagged(tag, text): print_json([Text{tag}, Text{text}]) */
static int tagged(Heap *heap, const char *tag, Str text) {
  Vec parts = vec_new(heap->arena);
  vec_push(&parts, json_text(heap, str_lit(tag)));
  vec_push(&parts, json_text(heap, text));
  return print_json(heap, json_items(heap, parts));
}

/* lower_failure(error): the Fail arms of lower_result */
static int lower_failure(Heap *heap, LowerError error) {
  switch (error.kind) {
  case LOWER_DEFERRED:
    return tagged(heap, "DEFERRED", error.message);
  case LOWER_UNSUPPORTED:
    return tagged(heap, "UNSUPPORTED", error.message);
  case LOWER_KERNEL_ERROR:
    return tagged(heap, "KERNEL_ERROR", lower_message(heap, error));
  }
  return tagged(heap, "KERNEL_ERROR", lower_message(heap, error));
}

/* lower_result(result) */
static int lower_result(Heap *heap, LowerResult result) {
  Vec names = vec_new(heap->arena);
  switch (result.kind) {
  case LOWER_DONE:
    return tagged(heap, "KERNEL_TYPE", pp_term(heap, &names, (const Term *)result.value));
  case LOWER_FAIL:
    return lower_failure(heap, result.error);
  }
  return lower_failure(heap, result.error);
}

/* budget(mode) */
static Budget budget(Heap *heap, Str mode) {
  if (str_eq(mode, str_lit("lower-zero"))) return budget_of_polls(nat_zero());
  if (str_eq(mode, str_lit("lower-one"))) return budget_of_polls(nat_of_u64(heap->arena, 1));
  return budget_of_polls(nat_of_u64(heap->arena, 100000));
}

/* lower_row(row, table, mode) */
static int lower_test_row(Heap *heap, const TranslateRow *row, Translated *table, Str mode) {
  if (row == NULL) return driver_die(1, "missing test declaration");
  return lower_result(heap, lower_type(heap, resolver(heap, mode), global_initial(heap), table, row, budget(heap, mode)));
}

/* translated(result, mode) */
static int translated(Heap *heap, TranslateResult result, Str mode) {
  if (result.kind == JSON_FAIL) return export_error(heap, result.error);
  Vec rows = translate_rows(result.value);
  return lower_test_row(heap, test_row(&rows), result.value, mode);
}

/* dispatch(mode, source, target_text) */
static int dispatch(Heap *heap, Str mode, const Export *source, Str target_text) {
  if (str_eq(mode, str_lit("report"))) return report_result(heap, report_prepare(heap, source));
  if (str_eq(mode, str_lit("mapping"))) return parsed_targets(heap, json_parse(heap, target_text), source);
  if (str_eq(mode, str_lit("mapping-foundation"))) {
    Vec rows = mapping_foundation_targets(heap, source);
    return mapping_result(heap, mapping_inventory(heap, source, &rows));
  }
  return translated(heap, translate_translate(heap, source), mode);
}

/* parsed(result, mode, targets) */
static int parsed(Heap *heap, ExportResult result, Str mode, Str targets_text) {
  if (result.kind == JSON_FAIL) return export_error(heap, result.error);
  return dispatch(heap, mode, &result.value, targets_text);
}

/* arguments(args): the arguments after the program name. */
static int arguments(Heap *heap, int argc, char **argv) {
  if (argc == 4) {
    return parsed(heap, export_read_string(heap, str_lit(argv[2])), str_lit(argv[1]), str_lit(argv[3]));
  }
  return driver_die(64, "usage: import_pipeline MODE NDJSON TARGETS");
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
