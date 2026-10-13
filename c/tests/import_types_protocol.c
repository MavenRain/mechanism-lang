/* Port of bend2/tests/import_types_protocol.bend with the cases of import_types_protocol_data.bend
 * folded in as static data (oracle label import-types-protocol, no corpus lines: the gate is the exit
 * code and the PASS lines). Runs every case through Export.read_string, Translate.translate and
 * Lower, prints "PASS import-types NAME" or "FAIL import-types NAME: MESSAGE" per case, then
 * "IMPORT-TYPES-OK" (exit 0) or dies with 1 and "IMPORT-TYPES-FAIL " ++ the failures joined. */
#include <string.h>

#include "driver.h"
#include "import.h"

/* ---- import_types_protocol_data.bend ---- */

/* Probe */
typedef enum {
  PROBE_EQUIVALENT_PI,
  PROBE_EQUIVALENT_ZERO,
  PROBE_INFER,
  PROBE_SEMANTIC,
  PROBE_REFUSE,
  PROBE_GLOBAL_RESOLVER,
  PROBE_LOCAL_RESOLVER,
  PROBE_STRING_NODE,
  PROBE_PRENEX_RESOLVER,
  PROBE_ALIAS,
  PROBE_METADATA,
  PROBE_NAT_RESOLVER,
  PROBE_SHARED,
  PROBE_BUDGET,
  PROBE_SHARED_BUDGET
} Probe;

/* one `++` operand of a case source: prefix(), a literal, expression_chain(count, index) or
 * level_chain(count, index) */
typedef enum { PIECE_PREFIX, PIECE_TEXT, PIECE_EXPRESSION_CHAIN, PIECE_LEVEL_CHAIN } PieceKind;
typedef struct {
  PieceKind kind;
  const char *text;
  uint32_t count;
  uint32_t index;
} Piece;

/* Case{name, source, probe}: text is the Semantic prefix or the Refuse kind */
typedef struct {
  const char *name;
  const Piece *pieces;
  size_t piece_count;
  Probe probe;
  const char *text;
} Case;

/* prefix() */
static const char *const PREFIX = "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"68218e876d2a38b1985b8590fff244a83c321783\",\"version\":\"4.31.0\"}}}\n{\"in\":1,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"in\":2,\"str\":{\"pre\":0,\"str\":\"A\"}}\n{\"in\":3,\"str\":{\"pre\":0,\"str\":\"x\"}}\n{\"in\":4,\"str\":{\"pre\":0,\"str\":\"test\"}}\n{\"in\":5,\"str\":{\"pre\":0,\"str\":\"F\"}}\n{\"in\":6,\"str\":{\"pre\":0,\"str\":\"v\"}}\n{\"in\":7,\"str\":{\"pre\":0,\"str\":\"Nat\"}}\n{\"il\":1,\"succ\":0}\n{\"il\":2,\"param\":1}\n{\"il\":3,\"param\":6}\n{\"il\":4,\"imax\":[2,3]}\n{\"il\":5,\"succ\":4}\n{\"il\":6,\"max\":[2,3]}\n";

static const Piece pieces_0[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_1[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":5,\"app\":{\"fn\":3,\"arg\":1}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":4,\"body\":5,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":6,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_2[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"lam\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"app\":{\"fn\":3,\"arg\":2}}\n{\"ie\":5,\"forallE\":{\"name\":3,\"type\":1,\"body\":4,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":5,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_3[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"letE\":{\"name\":3,\"type\":2,\"value\":1,\"body\":3,\"nondep\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":4,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_4[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"bvar\":0}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_5[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"bvar\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_6[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_7[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_8[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":2}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[1],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_9[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,1],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_10[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_11[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":7,\"us\":[]}}\n{\"axiom\":{\"name\":7,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_12[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_13[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_14[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"natVal\":\"13\"}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_15[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"strVal\":\"retained\"}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_16[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"proj\":{\"typeName\":5,\"idx\":0,\"struct\":1}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_17[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[5,6]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[1,6],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[6,1],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_18[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"in\":8,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[8],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_19[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"in\":8,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,8],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_20[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"mdata\":{\"expr\":1,\"data\":{\"opaque\":{\"array\":[\"x\",true]}}}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_21[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":7,\"us\":[]}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"bvar\":0}\n{\"ie\":5,\"natVal\":\"123456789012345678901234567890\"}\n{\"ie\":6,\"app\":{\"fn\":4,\"arg\":5}}\n{\"ie\":7,\"forallE\":{\"name\":3,\"type\":3,\"body\":6,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":7,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":7,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_22[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"natVal\":\"1\"}\n{\"ie\":5,\"app\":{\"fn\":3,\"arg\":4}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":2,\"body\":5,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":6,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_23[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_24[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n", 0, 0}, {PIECE_EXPRESSION_CHAIN, "", 1100, 2}, {PIECE_TEXT, "{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1101,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_25[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_LEVEL_CHAIN, "", 1100, 7}, {PIECE_TEXT, "{\"ie\":1,\"sort\":1106}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,6],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_26[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n", 0, 0}};
static const Piece pieces_27[] = {{PIECE_PREFIX, "", 0, 0}, {PIECE_TEXT, "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"forallE\":{\"name\":3,\"type\":3,\"body\":3,\"binderInfo\":\"default\"}}\n{\"ie\":5,\"forallE\":{\"name\":3,\"type\":4,\"body\":4,\"binderInfo\":\"default\"}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":5,\"body\":5,\"binderInfo\":\"default\"}}\n{\"ie\":7,\"forallE\":{\"name\":3,\"type\":6,\"body\":6,\"binderInfo\":\"default\"}}\n{\"ie\":8,\"forallE\":{\"name\":3,\"type\":7,\"body\":7,\"binderInfo\":\"default\"}}\n{\"ie\":9,\"forallE\":{\"name\":3,\"type\":8,\"body\":8,\"binderInfo\":\"default\"}}\n{\"ie\":10,\"forallE\":{\"name\":3,\"type\":9,\"body\":9,\"binderInfo\":\"default\"}}\n{\"ie\":11,\"forallE\":{\"name\":3,\"type\":10,\"body\":10,\"binderInfo\":\"default\"}}\n{\"ie\":12,\"forallE\":{\"name\":3,\"type\":11,\"body\":11,\"binderInfo\":\"default\"}}\n{\"ie\":13,\"forallE\":{\"name\":3,\"type\":12,\"body\":12,\"binderInfo\":\"default\"}}\n{\"ie\":14,\"forallE\":{\"name\":3,\"type\":13,\"body\":13,\"binderInfo\":\"default\"}}\n{\"ie\":15,\"forallE\":{\"name\":3,\"type\":14,\"body\":14,\"binderInfo\":\"default\"}}\n{\"ie\":16,\"forallE\":{\"name\":3,\"type\":15,\"body\":15,\"binderInfo\":\"default\"}}\n{\"ie\":17,\"forallE\":{\"name\":3,\"type\":16,\"body\":16,\"binderInfo\":\"default\"}}\n{\"ie\":18,\"forallE\":{\"name\":3,\"type\":17,\"body\":17,\"binderInfo\":\"default\"}}\n{\"ie\":19,\"forallE\":{\"name\":3,\"type\":18,\"body\":18,\"binderInfo\":\"default\"}}\n{\"ie\":20,\"forallE\":{\"name\":3,\"type\":19,\"body\":19,\"binderInfo\":\"default\"}}\n{\"ie\":21,\"forallE\":{\"name\":3,\"type\":20,\"body\":20,\"binderInfo\":\"default\"}}\n{\"ie\":22,\"forallE\":{\"name\":3,\"type\":21,\"body\":21,\"binderInfo\":\"default\"}}\n{\"ie\":23,\"forallE\":{\"name\":3,\"type\":22,\"body\":22,\"binderInfo\":\"default\"}}\n{\"ie\":24,\"forallE\":{\"name\":3,\"type\":23,\"body\":23,\"binderInfo\":\"default\"}}\n{\"ie\":25,\"forallE\":{\"name\":3,\"type\":24,\"body\":24,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":25,\"isUnsafe\":false}}\n", 0, 0}};

/* cases() */
static const Case CASES[] = {
    {"closed-dependent-pi", pieces_0, 2, PROBE_EQUIVALENT_PI, ""},
    {"local-application-infers-domain", pieces_1, 2, PROBE_INFER, ""},
    {"lambda-result-type-and-application", pieces_2, 2, PROBE_EQUIVALENT_PI, ""},
    {"let-preserves-dependent-scope", pieces_3, 2, PROBE_EQUIVALENT_ZERO, ""},
    {"unbound-term-index", pieces_4, 2, PROBE_SEMANTIC, "type of test has 1 unbound"},
    {"binder-domain-is-outside-own-scope", pieces_5, 2, PROBE_SEMANTIC, "type of test has 1 unbound"},
    {"shared-node-scope-is-per-root", pieces_6, 2, PROBE_SEMANTIC, "type of test has 1 unbound"},
    {"unbound-universe", pieces_7, 2, PROBE_SEMANTIC, "type of test has unbound universe"},
    {"universe-instantiation-arity", pieces_8, 2, PROBE_SEMANTIC, "constant F expects 1 universe"},
    {"duplicate-universe-parameters", pieces_9, 2, PROBE_SEMANTIC, "duplicate universe parameters"},
    {"unknown-source-constant", pieces_10, 2, PROBE_SEMANTIC, "type references undeclared constant F"},
    {"mapping-is-explicit-even-for-nat", pieces_11, 2, PROBE_REFUSE, "deferred"},
    {"resolver-cannot-return-an-unknown-global", pieces_12, 2, PROBE_GLOBAL_RESOLVER, ""},
    {"resolver-cannot-capture-a-local", pieces_13, 2, PROBE_LOCAL_RESOLVER, ""},
    {"non-type-root-is-refused", pieces_14, 2, PROBE_REFUSE, "kernel"},
    {"string-refusal-retains-node", pieces_15, 2, PROBE_STRING_NODE, ""},
    {"projection-needs-target-representation", pieces_16, 2, PROBE_REFUSE, "unsupported"},
    {"resolver-receives-exact-prenex-instantiation", pieces_17, 2, PROBE_PRENEX_RESOLVER, ""},
    {"aliased-universe-identity", pieces_18, 2, PROBE_ALIAS, ""},
    {"aliased-duplicate-universe-identity", pieces_19, 2, PROBE_SEMANTIC, "duplicate universe parameters"},
    {"metadata-is-preserved-and-transparent", pieces_20, 2, PROBE_METADATA, ""},
    {"natural-literal-in-dependent-type", pieces_21, 2, PROBE_NAT_RESOLVER, ""},
    {"application-checks-argument-domain", pieces_22, 2, PROBE_REFUSE, "kernel"},
    {"shared-type-dag-is-not-expanded", pieces_23, 2, PROBE_SHARED, ""},
    {"expression-depth-is-an-explicit-refusal", pieces_24, 4, PROBE_SEMANTIC, "type expression DAG exceeds the depth limit"},
    {"level-depth-is-an-explicit-refusal", pieces_25, 3, PROBE_SEMANTIC, "type level DAG exceeds the depth limit"},
    {"lowering-observes-budget", pieces_26, 2, PROBE_BUDGET, ""},
    {"shared-pi-expansion-has-a-finite-budget", pieces_27, 2, PROBE_SHARED_BUDGET, ""},
};
static const size_t CASE_COUNT = 28;

/* concatenation of every Str* in parts */
static Str concat_all(Heap *heap, const Vec *parts) {
  size_t total = 0;
  size_t i = 0;
  for (i = 0; i < parts->len; i += 1) total += ((const Str *)parts->items[i])->len;
  char *buf = heap_alloc(heap, total == 0 ? 1 : total);
  Str out = str_lit("");
  if (buf == NULL) return out;
  size_t at = 0;
  for (i = 0; i < parts->len; i += 1) {
    const Str *part = parts->items[i];
    memcpy(buf + at, part->data, part->len);
    at += part->len;
  }
  out.data = buf;
  out.len = total;
  return out;
}

static Str *boxed(Heap *heap, Str value) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box != NULL) *box = value;
  return box;
}

/* expression_chain(left, index): `{"ie":N,"mdata":{"expr":N-1,"data":{}}}` lines, and
 * level_chain(left, index): `{"il":N,"succ":N-1}` lines, N from index for left lines */
static Str chain(Heap *heap, uint32_t left, uint32_t index, const char *open, const char *middle,
                 const char *close) {
  Vec parts = vec_new(heap->arena);
  uint32_t n = 0;
  for (n = 0; n < left; n += 1) {
    uint32_t at = index + n;
    Str line = heap_str_cat(heap, str_lit(open), str_of_u64(heap->arena, at));
    line = heap_str_cat(heap, line, str_lit(middle));
    line = heap_str_cat(heap, line, str_of_u64(heap->arena, at - 1));
    line = heap_str_cat(heap, line, str_lit(close));
    vec_push(&parts, boxed(heap, line));
  }
  return concat_all(heap, &parts);
}

static Str piece_text(Heap *heap, const Piece *piece) {
  switch (piece->kind) {
  case PIECE_PREFIX:
    return str_lit(PREFIX);
  case PIECE_TEXT:
    return str_lit(piece->text);
  case PIECE_EXPRESSION_CHAIN:
    return chain(heap, piece->count, piece->index, "{\"ie\":", ",\"mdata\":{\"expr\":", ",\"data\":{}}}\n");
  case PIECE_LEVEL_CHAIN:
    return chain(heap, piece->count, piece->index, "{\"il\":", ",\"succ\":", "}\n");
  }
  return str_lit("");
}

/* the source text of one case */
static Str case_source(Heap *heap, const Case *item) {
  Vec parts = vec_new(heap->arena);
  size_t i = 0;
  for (i = 0; i < item->piece_count; i += 1) vec_push(&parts, boxed(heap, piece_text(heap, &item->pieces[i])));
  return concat_all(heap, &parts);
}

/* ---- import_types_protocol.bend ---- */

/* Result<String, Unit> */
typedef struct {
  bool ok;
  Str message;
} Check;

/* Result<String, T.Row> */
typedef struct {
  bool ok;
  Str message;
  const TranslateRow *row;
} RowFound;

/* Result<String, Term.T> */
typedef struct {
  bool ok;
  Str message;
  const Term *term;
} TermFound;

static Check done(void) {
  Check out;
  out.ok = true;
  out.message = str_lit("");
  return out;
}

static Check fail(Str message) {
  Check out;
  out.ok = false;
  out.message = message;
  return out;
}

/* require(ok, message) */
static Check require(bool ok, Str message) {
  if (ok) return done();
  return fail(message);
}

/* kernel(value): Result<Error.T, A> -> Result<String, A> */
static Check kernel(Heap *heap, Result value) {
  if (value.kind == RESULT_DONE) return done();
  return fail(error_to_string(heap, value.error));
}

/* successful(value): Result<L.Error, Term.T> -> Result<String, Term.T> */
static TermFound successful(Heap *heap, LowerResult value) {
  TermFound out;
  out.ok = value.kind == LOWER_DONE;
  out.message = out.ok ? str_lit("") : lower_message(heap, value.error);
  out.term = out.ok ? value.value : NULL;
  return out;
}

/* row_choice(found, value, rest) + row(rows): the first row named "test" */
static RowFound row_at(const Vec *rows, size_t index) {
  RowFound out;
  out.ok = false;
  out.message = str_lit("missing test declaration");
  out.row = NULL;
  if (index >= rows->len) return out;
  const TranslateRow *candidate = rows->items[index];
  if (str_eq(candidate->name, str_lit("test"))) {
    out.ok = true;
    out.message = str_lit("");
    out.row = candidate;
    return out;
  }
  return row_at(rows, index + 1);
}

static RowFound row(const Vec *rows) { return row_at(rows, 0); }

/* lower_found(found, table, resolver) */
static LowerResult lower_found(Heap *heap, RowFound found, Translated *table, LowerResolver resolver) {
  if (found.ok) return lower_default(heap, resolver, global_initial(heap), table, found.row);
  LowerResult out;
  out.kind = LOWER_FAIL;
  out.value = NULL;
  out.error.kind = LOWER_UNSUPPORTED;
  out.error.message = found.message;
  return out;
}

/* lower(table, resolver) */
static LowerResult lower(Heap *heap, Translated *table, LowerResolver resolver) {
  Vec rows = translate_rows(table);
  return lower_found(heap, row(&rows), table, resolver);
}

/* missing() */
static LowerResolver missing(Heap *heap) { return lower_missing(heap, str_lit("no checked mapping for ")); }

/* equivalent(expected, actual) */
static Check equivalent(Heap *heap, const Term *expected, const Term *actual) {
  Globals globals = global_initial(heap);
  Result left = eval_eval(heap, globals, vec_new(heap->arena), expected);
  Check left_check = kernel(heap, left);
  if (!left_check.ok) return left_check;
  Result right = eval_eval(heap, globals, vec_new(heap->arena), actual);
  Check right_check = kernel(heap, right);
  if (!right_check.ok) return right_check;
  BoolResult same =
      conv_conv_type(heap, check_make(heap, globals, budget_unlimited(), nat_zero()), left.value, right.value);
  if (same.kind == RESULT_FAIL) return fail(error_to_string(heap, same.error));
  return require(same.value, str_lit("types are not convertible"));
}

/* refusal(kind, value) */
static Check refusal(Heap *heap, Str kind, LowerResult value) {
  if (value.kind == LOWER_DONE) return fail(heap_str_cat(heap, str_lit("expected "), kind));
  switch (value.error.kind) {
  case LOWER_DEFERRED:
    return require(str_eq(kind, str_lit("deferred")),
                   heap_str_cat(heap, str_lit("wrong lowering refusal: "), value.error.message));
  case LOWER_UNSUPPORTED:
    return require(str_eq(kind, str_lit("unsupported")),
                   heap_str_cat(heap, str_lit("wrong lowering refusal: "), value.error.message));
  case LOWER_KERNEL_ERROR:
    return require(str_eq(kind, str_lit("kernel")),
                   heap_str_cat(heap, str_lit("wrong lowering refusal: "), error_to_string(heap, value.error.error)));
  }
  return fail(str_lit("wrong lowering refusal: "));
}

/* semantic(prefix, value) */
static Check semantic(Heap *heap, Str prefix, TranslateResult value) {
  if (value.kind == JSON_DONE) return fail(heap_str_cat(heap, str_lit("expected semantic refusal: "), prefix));
  return require(value.error.line > 0 && str_starts_with(value.error.message, prefix),
                 heap_str_cat(heap, str_lit("wrong semantic refusal: "), value.error.message));
}

/* retained_string(value) */
static bool retained_string(TableGet value) {
  if (!value.present) return false;
  const TranslateNode *node = value.value;
  return node->kind == TRANSLATE_STRING_LITERAL && str_eq(node->value, str_lit("retained"));
}

/* retained_metadata(value) */
static bool retained_metadata(Heap *heap, TableGet value) {
  if (!value.present) return false;
  const TranslateNode *node = value.value;
  if (node->kind != TRANSLATE_METADATA) return false;
  Str rendered = json_to_string(heap, json_object(heap, node->data));
  return str_eq(node->expr, str_lit("1")) && str_eq(rendered, str_lit("{\"opaque\":{\"array\":[\"x\",true]}}"));
}

/* canonical_parameter(declaration) */
static bool canonical_parameter(const TranslateRow *declaration) {
  if (declaration->parameters.len != 1) return false;
  const TranslateParameter *parameter = declaration->parameters.items[0];
  return str_eq(parameter->source_name, str_lit("1")) && str_eq(parameter->name, str_lit("u"));
}

/* levels_match(levels) */
static bool levels_match(Heap *heap, const Vec *levels) {
  if (levels->len != 2) return false;
  const LevelVar *first = levels->items[0];
  const LevelVar *second = levels->items[1];
  const LevelVar *one = level_var(heap, nat_of_u64(heap->arena, 1));
  const LevelVar *zero = level_var(heap, nat_zero());
  return level_equal(heap, first, level_succ(heap, level_imax(heap, one, zero))) &&
         level_equal(heap, second, level_max(heap, one, zero));
}

/* count_resolver(count): the ctx is a heap-boxed Nat */
static LowerResolver count_resolver(Heap *heap, Nat count);

/* count_call(probe, count, name, levels) */
static LowerResolved count_call(Heap *heap, void *ctx, Str name, const Vec *levels) {
  Nat count = *(const Nat *)ctx;
  LowerResolved out;
  out.message = str_lit("");
  out.term = NULL;
  if (str_eq(name, str_lit("__count"))) {
    out.next = count_resolver(heap, count);
    out.ok = true;
    out.term = term_var(heap, count);
    return out;
  }
  out.next = count_resolver(heap, nat_succ(heap->arena, count));
  out.ok = str_eq(name, str_lit("F")) && levels_match(heap, levels);
  if (out.ok) {
    out.term = term_univ(heap, level_zero());
  } else {
    out.message = str_lit("wrong source reference or universe substitution");
  }
  return out;
}

static LowerResolver count_resolver(Heap *heap, Nat count) {
  LowerResolver resolver;
  Nat *box = heap_alloc(heap, sizeof(Nat));
  if (box != NULL) *box = count;
  resolver.call = count_call;
  resolver.ctx = box;
  return resolver;
}

/* count_result(value) */
static Check count_result(Heap *heap, LowerResolved value) {
  if (value.ok && value.term->kind == TERM_VAR) {
    return require(nat_cmp(value.term->index, nat_of_u64(heap->arena, 1)) == 0, str_lit("resolver did not run once"));
  }
  return fail(str_lit("resolver counter could not be read"));
}

/* count_pair(value) */
static Check count_pair(Heap *heap, LowerResolved value) { return count_result(heap, value); }

/* check_count(state) */
static Check check_count(Heap *heap, LowerState state) {
  Vec none = vec_new(heap->arena);
  return count_pair(heap, lower_resolver_call(heap, state.resolver, str_lit("__count"), &none));
}

/* counted_result(state, value) */
static Check counted_result(Heap *heap, LowerState state, LowerResult value) {
  TermFound term = successful(heap, value);
  if (!term.ok) return fail(term.message);
  Check counted = check_count(heap, state);
  if (!counted.ok) return counted;
  return equivalent(heap, term_univ(heap, level_zero()), term.term);
}

/* counted(table) */
static Check counted(Heap *heap, Translated *table) {
  Vec rows = translate_rows(table);
  RowFound declaration = row(&rows);
  if (!declaration.ok) return fail(declaration.message);
  LowerState state;
  state.work = nat_of_u64(heap->arena, 100000);
  state.budget = budget_of_polls(nat_of_u64(heap->arena, 100000));
  state.resolver = count_resolver(heap, nat_zero());
  LowerResult value = lower_row(heap, &state, declaration.row, global_initial(heap),
                                budget_of_polls(nat_of_u64(heap->arena, 100000)), table);
  return counted_result(heap, state, value);
}

/* nat_resolver() */
static LowerResolver nat_resolver(void);

/* nat_call(name, levels) */
static LowerResolved nat_call(Heap *heap, void *ctx, Str name, const Vec *levels) {
  LowerResolved out;
  (void)ctx;
  out.next = nat_resolver();
  out.ok = str_eq(name, str_lit("Nat")) && levels->len == 0;
  out.message = out.ok ? str_lit("") : str_lit("unexpected Nat resolution");
  out.term = out.ok ? prim_nat_ty(heap) : NULL;
  return out;
}

static LowerResolver nat_resolver(void) {
  LowerResolver resolver;
  resolver.call = nat_call;
  resolver.ctx = NULL;
  return resolver;
}

/* budget_error(value) */
static Check budget_error(Heap *heap, LowerResult value) {
  if (value.kind == LOWER_DONE) return fail(str_lit("shared Pi expansion escaped its budget"));
  if (value.error.kind == LOWER_KERNEL_ERROR && value.error.error.kind == ERROR_BUDGET_EXHAUSTED) return done();
  return fail(heap_str_cat(heap, str_lit("shared Pi expansion failed without a budget error: "),
                           lower_message(heap, value.error)));
}

/* inspect(probe, table) */
static Check inspect(Heap *heap, Probe probe, Str text, Translated *table) {
  switch (probe) {
  case PROBE_EQUIVALENT_PI: {
    TermFound term = successful(heap, lower(heap, table, missing(heap)));
    if (!term.ok) return fail(term.message);
    return equivalent(heap,
                      rules_arrow(heap, QUANTITY_MANY, str_lit("x"), term_univ(heap, level_one(heap)),
                                  term_var(heap, nat_zero())),
                      term.term);
  }
  case PROBE_EQUIVALENT_ZERO: {
    TermFound term = successful(heap, lower(heap, table, missing(heap)));
    if (!term.ok) return fail(term.message);
    return equivalent(heap, term_univ(heap, level_zero()), term.term);
  }
  case PROBE_INFER: {
    TermFound term = successful(heap, lower(heap, table, missing(heap)));
    if (!term.ok) return fail(term.message);
    return kernel(heap, check_infer_term(heap, global_initial(heap), budget_unlimited(), term.term));
  }
  case PROBE_REFUSE:
    return refusal(heap, text, lower(heap, table, missing(heap)));
  case PROBE_GLOBAL_RESOLVER:
    return refusal(heap, str_lit("kernel"), lower(heap, table, lower_fixed(term_global(heap, str_lit("uninstalled")))));
  case PROBE_LOCAL_RESOLVER:
    return refusal(heap, str_lit("kernel"), lower(heap, table, lower_fixed(term_var(heap, nat_zero()))));
  case PROBE_STRING_NODE: {
    Check retained = require(retained_string(table_get(translate_type_nodes(table), str_lit("1"))),
                             str_lit("string node vanished"));
    if (!retained.ok) return retained;
    return refusal(heap, str_lit("unsupported"), lower(heap, table, missing(heap)));
  }
  case PROBE_PRENEX_RESOLVER:
    return counted(heap, table);
  case PROBE_ALIAS: {
    Vec rows = translate_rows(table);
    RowFound declaration = row(&rows);
    if (!declaration.ok) return fail(declaration.message);
    Check canonical = require(canonical_parameter(declaration.row), str_lit("parameter identity was not canonical"));
    if (!canonical.ok) return canonical;
    TermFound term = successful(heap, lower(heap, table, missing(heap)));
    if (!term.ok) return fail(term.message);
    return done();
  }
  case PROBE_METADATA: {
    Check retained = require(retained_metadata(heap, table_get(translate_type_nodes(table), str_lit("2"))),
                             str_lit("metadata payload changed"));
    if (!retained.ok) return retained;
    TermFound term = successful(heap, lower(heap, table, missing(heap)));
    if (!term.ok) return fail(term.message);
    return equivalent(heap, term_univ(heap, level_zero()), term.term);
  }
  case PROBE_NAT_RESOLVER: {
    TermFound term = successful(heap, lower(heap, table, nat_resolver()));
    if (!term.ok) return fail(term.message);
    return done();
  }
  case PROBE_SHARED: {
    Vec rows = translate_rows(table);
    return require(table_size(translate_type_nodes(table)) == 3 && rows.len == 2,
                   str_lit("shared type nodes were copied or lost"));
  }
  case PROBE_BUDGET: {
    Vec rows = translate_rows(table);
    RowFound declaration = row(&rows);
    if (!declaration.ok) return fail(declaration.message);
    return refusal(heap, str_lit("kernel"),
                   lower_type(heap, missing(heap), global_initial(heap), table, declaration.row,
                              budget_of_polls(nat_zero())));
  }
  case PROBE_SHARED_BUDGET: {
    Check retained = require(table_size(translate_type_nodes(table)) == 25,
                             str_lit("shared Pi input was expanded in the source table"));
    if (!retained.ok) return retained;
    return budget_error(heap, lower(heap, table, missing(heap)));
  }
  case PROBE_SEMANTIC:
    return fail(str_lit("semantic probe was routed to successful translation"));
  }
  return fail(str_lit("semantic probe was routed to successful translation"));
}

/* translated(probe, value) */
static Check translated(Heap *heap, Probe probe, Str text, TranslateResult value) {
  if (probe == PROBE_SEMANTIC) return semantic(heap, text, value);
  if (value.kind == JSON_FAIL) return fail(value.error.message);
  return inspect(heap, probe, text, value.value);
}

/* test(source, probe) */
static Check test(Heap *heap, Str source, Probe probe, Str text) {
  ExportResult table = export_read_string(heap, source);
  if (table.kind == JSON_FAIL) return fail(table.error.message);
  return translated(heap, probe, text, translate_translate(heap, &table.value));
}

/* report(value, name): prints the verdict line, returns the failure entry when there is one */
static Str *report(Heap *heap, Check value, Str name) {
  if (value.ok) {
    driver_print(heap_str_cat(heap, str_lit("PASS import-types "), name));
    return NULL;
  }
  Str entry = heap_str_cat(heap, heap_str_cat(heap, name, str_lit(": ")), value.message);
  driver_print(heap_str_cat(heap, str_lit("FAIL import-types "), entry));
  return boxed(heap, entry);
}

/* run(rows): the failure entries in case order */
static Vec run(Heap *heap) {
  Vec failures = vec_new(heap->arena);
  size_t i = 0;
  for (i = 0; i < CASE_COUNT; i += 1) {
    const Case *item = &CASES[i];
    Str name = str_lit(item->name);
    Str *entry = report(heap, test(heap, case_source(heap, item), item->probe, str_lit(item->text)), name);
    if (entry != NULL) vec_push(&failures, entry);
  }
  return failures;
}

/* verdict(failures) */
static int verdict(Heap *heap, const Vec *failures) {
  if (failures->len == 0) return driver_print(str_lit("IMPORT-TYPES-OK"));
  return driver_die_text(1, heap_str_cat(heap, str_lit("IMPORT-TYPES-FAIL "),
                                         json_join_strings(heap, failures, str_lit(""), str_lit("; "))));
}

/* arguments(args) */
static int arguments(Heap *heap) {
  Vec failures = run(heap);
  return verdict(heap, &failures);
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
