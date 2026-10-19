/* Port of tests/mapping_protocol.bend (oracle label mapping-protocol: one corpus call through the JS
 * build, no arguments; c-test also compares the output with c/tests/mapping_protocol.expected). Reads the
 * lean4export sample with Export.read_string, runs Mapping.inventory over several target lists and prints
 * "PASS mapping NAME" or "FAIL mapping NAME" per case, then "MAPPING-OK" (exit 0) or dies with 1 and
 * "MAPPING-FAIL N cases"; an inventory failure dies with 1 and "MAPPING-FAIL " ++ the message. */
#include <string.h>

#include "driver.h"
#include "import.h"

/* source(): one lean4export 3.1.0 NDJSON document, one row per adjacent literal */
static const char SOURCE[] =
  "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"test\",\"version\":\"4.31.0\"}}}\n"
  "{\"in\":1,\"str\":{\"pre\":0,\"str\":\"Zeta\"}}\n"
  "{\"in\":2,\"str\":{\"pre\":0,\"str\":\"Alpha\"}}\n"
  "{\"in\":3,\"str\":{\"pre\":0,\"str\":\"Alpha\"}}\n"
  "{\"in\":4,\"str\":{\"pre\":0,\"str\":\"Unused\"}}\n"
  "{\"in\":5,\"str\":{\"pre\":0,\"str\":\"UnifiedAggregation\"}}\n"
  "{\"in\":6,\"str\":{\"pre\":5,\"str\":\"inside\"}}\n"
  "{\"in\":7,\"str\":{\"pre\":0,\"str\":\"UnifiedAggregationExtra\"}}\n"
  "{\"in\":8,\"str\":{\"pre\":7,\"str\":\"outside\"}}\n"
  "{\"in\":9,\"str\":{\"pre\":0,\"str\":\"Lean\"}}\n"
  "{\"in\":10,\"str\":{\"pre\":9,\"str\":\"Omega\"}}\n"
  "{\"in\":11,\"str\":{\"pre\":10,\"str\":\"reflect\"}}\n"
  "{\"in\":12,\"str\":{\"pre\":9,\"str\":\"OmegaExtra\"}}\n"
  "{\"in\":13,\"str\":{\"pre\":12,\"str\":\"ordinary\"}}\n"
  "{\"in\":14,\"str\":{\"pre\":0,\"str\":\"_private\"}}\n"
  "{\"in\":15,\"str\":{\"pre\":14,\"str\":\"Init\"}}\n"
  "{\"in\":16,\"str\":{\"pre\":15,\"str\":\"Omega\"}}\n"
  "{\"in\":17,\"str\":{\"pre\":16,\"str\":\"helper\"}}\n"
  "{\"in\":18,\"str\":{\"pre\":9,\"str\":\"Syntax\"}}\n"
  "{\"in\":19,\"str\":{\"pre\":0,\"str\":\"Array\"}}\n"
  "{\"in\":20,\"str\":{\"pre\":19,\"str\":\"push\"}}\n"
  "{\"in\":21,\"str\":{\"pre\":0,\"str\":\"ArrowCat\"}}\n"
  "{\"in\":22,\"str\":{\"pre\":21,\"str\":\"inside\"}}\n"
  "{\"in\":23,\"str\":{\"pre\":0,\"str\":\"CompCatTheory\"}}\n"
  "{\"in\":24,\"str\":{\"pre\":23,\"str\":\"inside\"}}\n"
  "{\"ie\":0,\"sort\":0}\n"
  "{\"ie\":1,\"const\":{\"name\":1,\"us\":[]}}\n"
  "{\"ie\":2,\"const\":{\"name\":2,\"us\":[]}}\n"
  "{\"ie\":3,\"const\":{\"name\":3,\"us\":[]}}\n"
  "{\"ie\":4,\"const\":{\"name\":6,\"us\":[]}}\n"
  "{\"ie\":5,\"const\":{\"name\":8,\"us\":[]}}\n"
  "{\"ie\":6,\"const\":{\"name\":11,\"us\":[]}}\n"
  "{\"ie\":7,\"const\":{\"name\":13,\"us\":[]}}\n"
  "{\"ie\":8,\"const\":{\"name\":17,\"us\":[]}}\n"
  "{\"ie\":9,\"const\":{\"name\":18,\"us\":[]}}\n"
  "{\"ie\":10,\"const\":{\"name\":20,\"us\":[]}}\n"
  "{\"ie\":11,\"const\":{\"name\":22,\"us\":[]}}\n"
  "{\"ie\":12,\"const\":{\"name\":24,\"us\":[]}}\n"
  "{\"axiom\":{\"name\":1,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":2,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":6,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":8,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":11,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":13,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":17,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":18,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":20,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":22,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
  "{\"axiom\":{\"name\":24,\"levelParams\":[],\"type\":0,\"isUnsafe\":false}}\n"
    ;

#define CASE_COUNT 14

/* the case names of cases() in order */
static const char *const CASE_NAMES[CASE_COUNT] = {
    "canonical-identities-and-stable-order",
    "declared-and-referenced-counts-stay-distinct",
    "only-exact-public-omega-namespace-is-never",
    "candidate-never-claims-type-parity",
    "duplicate-source-target-refused",
    "unknown-source-target-refused",
    "unreferenced-source-target-refused",
    "in-house-source-target-refused",
    "never-target-conflict-refused",
    "tab-target-refused",
    "newline-target-refused",
    "empty-target-refused",
    "sentinel-target-refused",
    "tsv-schema-is-stable"};

/* Result<String, List<Pair<String, Bool>>>: the verdict of every case, or the first inventory failure */
typedef struct {
  JsonResultKind kind;
  bool ok[CASE_COUNT]; /* JSON_DONE, in CASE_NAMES order */
  Str message;         /* JSON_FAIL */
} Cases;

static Cases cases_fail(Str message) {
  Cases out;
  size_t i = 0;
  for (i = 0; i < CASE_COUNT; i += 1) out.ok[i] = false;
  out.kind = JSON_FAIL;
  out.message = message;
  return out;
}

/* a Maybe<String> target: Some{value} or None{} */
static ExportText some(const char *value) {
  ExportText out;
  out.present = true;
  out.value = str_lit(value);
  return out;
}

static ExportText none(void) {
  ExportText out;
  out.present = false;
  out.value = str_empty();
  return out;
}

/* M.Target{lean_name, target} */
static MappingTarget target(const char *lean_name, const char *value) {
  MappingTarget out;
  out.lean_name = str_lit(lean_name);
  out.target = str_lit(value);
  return out;
}

/* the target list literal [M.Target{..}, ..] as MappingTarget* items */
static Vec target_list(Heap *heap, const MappingTarget *items, size_t count) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (i = 0; i < count; i += 1) {
    MappingTarget *box = heap_alloc(heap, sizeof(MappingTarget));
    if (box == NULL) return out;
    *box = items[i];
    vec_push(&out, box);
  }
  return out;
}

/* M.inventory(source, [items..]) */
static MappingResult inventory(Heap *heap, const Export *source, const MappingTarget *items, size_t count) {
  Vec rows = target_list(heap, items, count);
  return mapping_inventory(heap, source, &rows);
}

/* refused(r) */
static bool refused(MappingResult r) { return r.kind == JSON_FAIL; }

/* row_names(rows): every row name followed by a newline */
static Str row_names(Heap *heap, const Vec *rows) {
  Str out = str_lit("");
  size_t i = 0;
  for (i = 0; i < rows->len; i += 1) {
    VecGet got = vec_get(rows, i);
    if (got.status != VEC_GET_OK) return out;
    const MappingRow *row = got.value;
    out = heap_str_cat(heap, out, heap_str_cat(heap, row->lean_name, str_lit("\n")));
  }
  return out;
}

/* names(inventory) */
static Str names(Heap *heap, const MappingInventory *inventory) { return row_names(heap, &inventory->rows); }

/* counts(inventory) */
static bool counts(const MappingInventory *inventory) {
  return inventory->rows.len == 8 && inventory->external_declared == 9 && inventory->const_names == 11;
}

/* target_eq(a, b) */
static bool target_eq(ExportText a, ExportText b) {
  if (a.present && b.present) return str_eq(a.value, b.value);
  return !a.present && !b.present;
}

/* has_row(rows, name, target, verdict) */
static bool has_row(const Vec *rows, Str name, ExportText target, Str verdict) {
  size_t i = 0;
  for (i = 0; i < rows->len; i += 1) {
    VecGet got = vec_get(rows, i);
    if (got.status != VEC_GET_OK) return false;
    const MappingRow *row = got.value;
    bool hit = str_eq(row->lean_name, name) && target_eq(row->target, target) &&
               str_eq(mapping_verdict_name(row->verdict), verdict);
    if (hit) return true;
  }
  return false;
}

/* mapped_rows(inventory) */
static bool mapped_rows(const MappingInventory *inventory) {
  return has_row(&inventory->rows, str_lit("Alpha"), some("MechAlpha"), str_lit("NAME_ONLY")) &&
         has_row(&inventory->rows, str_lit("Zeta"), none(), str_lit("UNMAPPED"));
}

/* cases() */
static Cases cases(Heap *heap) {
  Cases out;
  ExportResult read = export_read_string(heap, str_lit(SOURCE));
  if (read.kind == JSON_FAIL) return cases_fail(read.error.message);
  const Export *source = &read.value;
  const MappingTarget mapped_targets[1] = {target("Alpha", "MechAlpha")};
  const MappingTarget first_targets[2] = {target("Zeta", "MechZeta"), target("Alpha", "MechAlpha")};
  const MappingTarget reordered_targets[2] = {target("Alpha", "MechAlpha"), target("Zeta", "MechZeta")};
  const MappingTarget duplicate_targets[2] = {target("Alpha", "One"), target("Alpha", "Two")};
  const MappingTarget unknown_targets[1] = {target("Missing", "One")};
  const MappingTarget unreferenced_targets[1] = {target("Unused", "One")};
  const MappingTarget in_house_targets[1] = {target("UnifiedAggregation.inside", "One")};
  const MappingTarget never_targets[1] = {target("Lean.Omega.reflect", "One")};
  const MappingTarget tab_targets[1] = {target("Alpha", "bad\ttarget")};
  const MappingTarget newline_targets[1] = {target("Alpha", "bad\ntarget")};
  const MappingTarget empty_targets[1] = {target("Alpha", "")};
  const MappingTarget sentinel_targets[1] = {target("Alpha", "-")};
  MappingResult base = inventory(heap, source, NULL, 0);
  if (base.kind == JSON_FAIL) return cases_fail(base.message);
  MappingResult mapped = inventory(heap, source, mapped_targets, 1);
  if (mapped.kind == JSON_FAIL) return cases_fail(mapped.message);
  MappingResult first = inventory(heap, source, first_targets, 2);
  if (first.kind == JSON_FAIL) return cases_fail(first.message);
  MappingResult reordered = inventory(heap, source, reordered_targets, 2);
  if (reordered.kind == JSON_FAIL) return cases_fail(reordered.message);
  Str first_map = mapping_map_tsv(heap, &first.value);
  Str never_header = str_lit("lean_name\tratification\tmilestone\nLean.Omega.reflect\tR-V4:omega-reflection\tNEVER\n");
  out.kind = JSON_DONE;
  out.message = str_lit("");
  out.ok[0] = str_eq(names(heap, &base.value),
                     str_lit("Alpha\nArray.push\nLean.Omega.reflect\nLean.OmegaExtra.ordinary\nLean.Syntax\n"
                             "UnifiedAggregationExtra.outside\nZeta\n_private.Init.Omega.helper\n"));
  out.ok[1] = counts(&base.value);
  out.ok[2] = str_eq(mapping_never_tsv(heap, &base.value), never_header);
  out.ok[3] = mapped_rows(&mapped.value);
  out.ok[4] = refused(inventory(heap, source, duplicate_targets, 2));
  out.ok[5] = refused(inventory(heap, source, unknown_targets, 1));
  out.ok[6] = refused(inventory(heap, source, unreferenced_targets, 1));
  out.ok[7] = refused(inventory(heap, source, in_house_targets, 1));
  out.ok[8] = refused(inventory(heap, source, never_targets, 1));
  out.ok[9] = refused(inventory(heap, source, tab_targets, 1));
  out.ok[10] = refused(inventory(heap, source, newline_targets, 1));
  out.ok[11] = refused(inventory(heap, source, empty_targets, 1));
  out.ok[12] = refused(inventory(heap, source, sentinel_targets, 1));
  out.ok[13] = str_eq(first_map, mapping_map_tsv(heap, &reordered.value)) &&
               str_starts_with(first_map, str_lit("lean_name\ttarget\tverdict\nAlpha\tMechAlpha\tNAME_ONLY\n")) &&
               str_eq(mapping_never_tsv(heap, &first.value), never_header);
  return out;
}

/* report(ok, name): prints the verdict line, returns the failure count of this case */
static size_t report(Heap *heap, bool ok, Str name) {
  if (ok) {
    driver_print(heap_str_cat(heap, str_lit("PASS mapping "), name));
    return 0;
  }
  driver_print(heap_str_cat(heap, str_lit("FAIL mapping "), name));
  return 1;
}

/* verdict(failed) */
static int verdict(Heap *heap, size_t failed) {
  if (failed == 0) return driver_print(str_lit("MAPPING-OK"));
  Str text = heap_str_cat(heap, str_lit("MAPPING-FAIL "), str_of_u64(heap->arena, failed));
  return driver_die_text(1, heap_str_cat(heap, text, str_lit(" cases")));
}

/* reports(rows, failed) */
static int reports(Heap *heap, const Cases *rows) {
  size_t failed = 0;
  size_t i = 0;
  for (i = 0; i < CASE_COUNT; i += 1) failed += report(heap, rows->ok[i], str_lit(CASE_NAMES[i]));
  return verdict(heap, failed);
}

/* result(r) */
static int result(Heap *heap, const Cases *r) {
  if (r->kind == JSON_FAIL) return driver_die_text(1, heap_str_cat(heap, str_lit("MAPPING-FAIL "), r->message));
  return reports(heap, r);
}

/* arguments(args) */
static int arguments(Heap *heap) {
  Cases r = cases(heap);
  return result(heap, &r);
}

/* main() */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = arguments(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
