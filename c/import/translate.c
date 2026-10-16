/* Port of bend2/import/translate.bend (import "Translate"): the typed view of an export. Source
 * identities remain decimal strings, including values beyond UInt32. The Bend T is threaded linearly
 * through the visitors, so the one heap-boxed Translated is updated in place (with_* are the table_put
 * calls of commit_level, commit_expr and add_declaration). The Bend id sets are persistent values that
 * cached summaries share, so extensions of cached sets first copy them. Linear accumulators may
 * use set_add in place. Every message text is the Bend text, byte for byte. */
#include "import.h"
#include <string.h>

/* The one Unit row of an IdSet table (table_put never stores a NULL row). */
static char unit_row;

/* LevelVisit{table, params}, ExprVisit{table, summary}, ExprBuild{table, node, summary} */
typedef struct {
  Translated *table;
  TranslateIdSet params;
} LevelVisit;
typedef struct {
  Translated *table;
  TranslateSummary summary;
} ExprVisit;
typedef struct {
  Translated *table;
  TranslateNode node;
  TranslateSummary summary;
} ExprBuild;

/* Result<Export.Error, Summary | LevelVisit | ExprVisit | ExprBuild> */
typedef struct {
  JsonResultKind kind;
  TranslateSummary value;
  ExportError error;
} SummaryResult;
typedef struct {
  JsonResultKind kind;
  LevelVisit value;
  ExportError error;
} LevelVisitResult;
typedef struct {
  JsonResultKind kind;
  ExprVisit value;
  ExportError error;
} ExprVisitResult;
typedef struct {
  JsonResultKind kind;
  ExprBuild value;
  ExportError error;
} ExprBuildResult;

/* Export.ImportError{line, message} */
static ExportError import_error(uint32_t line, Str message) {
  ExportError error;
  error.line = line;
  error.message = message;
  return error;
}

/* String.append */
static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* Nat.show */
static Str show_size(Heap *heap, size_t value) { return str_of_u64(heap->arena, (uint64_t)value); }

static TranslateCheck check_done(void) {
  TranslateCheck result;
  result.kind = JSON_DONE;
  result.error = import_error(0, str_empty());
  return result;
}
static TranslateCheck check_fail(ExportError error) {
  TranslateCheck result;
  result.kind = JSON_FAIL;
  result.error = error;
  return result;
}
static TranslateText text_done(Str value) {
  TranslateText result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static TranslateText text_fail(ExportError error) {
  TranslateText result;
  result.kind = JSON_FAIL;
  result.value = str_empty();
  result.error = error;
  return result;
}
static TranslateList list_done(Vec value) {
  TranslateList result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static TranslateList list_fail(ExportError error) {
  TranslateList result;
  result.kind = JSON_FAIL;
  memset(&result.value, 0, sizeof(result.value));
  result.error = error;
  return result;
}
static SummaryResult summary_done(TranslateSummary value) {
  SummaryResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static SummaryResult summary_fail(ExportError error) {
  SummaryResult result;
  result.kind = JSON_FAIL;
  memset(&result.value, 0, sizeof(result.value));
  result.error = error;
  return result;
}
static LevelVisitResult levelvisit_done(LevelVisit value) {
  LevelVisitResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static LevelVisitResult levelvisit_fail(ExportError error) {
  LevelVisitResult result;
  result.kind = JSON_FAIL;
  memset(&result.value, 0, sizeof(result.value));
  result.error = error;
  return result;
}
static ExprVisitResult exprvisit_done(ExprVisit value) {
  ExprVisitResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static ExprVisitResult exprvisit_fail(ExportError error) {
  ExprVisitResult result;
  result.kind = JSON_FAIL;
  memset(&result.value, 0, sizeof(result.value));
  result.error = error;
  return result;
}
static ExprBuildResult exprbuild_done(ExprBuild value) {
  ExprBuildResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static ExprBuildResult exprbuild_fail(ExportError error) {
  ExprBuildResult result;
  result.kind = JSON_FAIL;
  memset(&result.value, 0, sizeof(result.value));
  result.error = error;
  return result;
}
static TranslateResult translate_done(Translated *value) {
  TranslateResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.error = import_error(0, str_empty());
  return result;
}
static TranslateResult translate_fail(ExportError error) {
  TranslateResult result;
  result.kind = JSON_FAIL;
  result.value = NULL;
  result.error = error;
  return result;
}

/* Heap boxes of the table rows and list items (NULL = allocation failed, heap->oom set). */
static TranslateNode *box_node(Heap *heap, TranslateNode value) {
  TranslateNode *box = heap_alloc(heap, sizeof(TranslateNode));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static TranslateLevel *box_level(Heap *heap, TranslateLevel value) {
  TranslateLevel *box = heap_alloc(heap, sizeof(TranslateLevel));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static TranslateSummary *box_summary(Heap *heap, TranslateSummary value) {
  TranslateSummary *box = heap_alloc(heap, sizeof(TranslateSummary));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static TranslateIdSet *box_set(Heap *heap, TranslateIdSet value) {
  TranslateIdSet *box = heap_alloc(heap, sizeof(TranslateIdSet));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static TranslateParameter *box_parameter(Heap *heap, TranslateParameter value) {
  TranslateParameter *box = heap_alloc(heap, sizeof(TranslateParameter));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}
static TranslateRow *box_row(Heap *heap, TranslateRow value) {
  TranslateRow *box = heap_alloc(heap, sizeof(TranslateRow));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

/* item appended to out (a NULL item is a failed allocation, heap->oom set) */
static Vec with_item(Heap *heap, Vec out, void *item) {
  if (item == NULL) return out;
  return json_vec_with(heap, out, item);
}

/* empty_set() */
TranslateIdSet translate_empty_set(Heap *heap) {
  TranslateIdSet set;
  set.values = table_empty(heap);
  return set;
}

/* every Str* key of KEYS from INDEX put into SET in place: SET is always a fresh copy here */
static void insert_from(Heap *heap, const Vec *keys, size_t index, TranslateIdSet *set) {
  VecGet got = vec_get(keys, index);
  if (got.status != VEC_GET_OK) return;
  table_put(heap, &set->values, *(Str *)got.value, &unit_row);
  insert_from(heap, keys, index + 1, set);
}

/* A fresh set with the same keys: the Bend sets are persistent values shared by cached summaries. */
static TranslateIdSet set_copy(Heap *heap, TranslateIdSet set) {
  Vec keys = table_keys(heap, &set.values);
  TranslateIdSet out = translate_empty_set(heap);
  insert_from(heap, &keys, 0, &out);
  return out;
}

/* set_add(set, key) */
/* Consumes a linear set accumulator. Cached expression summaries must be copied before use.
 * The fold accumulators report_canonical_constant, mapping_add_external and mapping_add_display
 * never read the input set again. The copy per add was quadratic in the arena: hop 335 profiled
 * `mech map-inventory` on the uat export at 13.4M table_put calls under set_copy (2.4 GB of
 * 2.5 GB before the memcap kill; footMB 3806-3831 in hops 330-333). */
TranslateIdSet translate_set_add(Heap *heap, TranslateIdSet set, Str key) {
  table_put(heap, &set.values, key, &unit_row);
  return set;
}

/* set_has(set, key) */
bool translate_set_has(const TranslateIdSet *set, Str key) { return table_has(&set->values, key); }

/* set_keys(set) */
Vec translate_set_keys(Heap *heap, const TranslateIdSet *set) { return table_keys(heap, &set->values); }

/* set_insert_all(keys, set): the set itself when there is nothing to insert */
TranslateIdSet translate_set_insert_all(Heap *heap, const Vec *keys, TranslateIdSet set) {
  if (vec_get(keys, 0).status != VEC_GET_OK) return set;
  TranslateIdSet out = set_copy(heap, set);
  insert_from(heap, keys, 0, &out);
  return out;
}

/* union(a, b) */
TranslateIdSet translate_union(Heap *heap, TranslateIdSet a, TranslateIdSet b) {
  Vec keys = translate_set_keys(heap, &a);
  return translate_set_insert_all(heap, &keys, b);
}

/* empty_summary() */
TranslateSummary translate_empty_summary(Heap *heap) {
  TranslateSummary summary;
  summary.binders = big_zero();
  summary.params = translate_empty_set(heap);
  summary.constants = translate_empty_set(heap);
  return summary;
}

/* max_big(order, a, b) */
Big translate_max_big(Cmp order, Big a, Big b) {
  switch (order) {
    case CMP_GT:
      return a;
    case CMP_LT:
    case CMP_EQ:
      return b;
  }
  return b;
}

/* merge(a, b) */
TranslateSummary translate_merge(Heap *heap, TranslateSummary a, TranslateSummary b) {
  TranslateSummary out;
  out.binders = translate_max_big(big_compare(a.binders, b.binders), a.binders, b.binders);
  out.params = translate_union(heap, a.params, b.params);
  out.constants = translate_union(heap, a.constants, b.constants);
  return out;
}

/* decrement(order, number) */
Big translate_decrement(Heap *heap, Cmp order, Big number) {
  switch (order) {
    case CMP_GT:
      return big_sub(heap, number, big_one());
    case CMP_LT:
    case CMP_EQ:
      return big_zero();
  }
  return big_zero();
}

/* under_binder(summary) */
TranslateSummary translate_under_binder(Heap *heap, TranslateSummary summary) {
  summary.binders = translate_decrement(heap, big_compare(summary.binders, big_zero()), summary.binders);
  return summary;
}

/* empty() (NULL = allocation failed, heap->oom set) */
Translated *translate_empty(Heap *heap) {
  Translated *table = heap_alloc(heap, sizeof(Translated));
  if (table == NULL) return NULL;
  table->type_nodes = table_empty(heap);
  table->level_nodes = table_empty(heap);
  table->summaries = table_empty(heap);
  table->level_params = table_empty(heap);
  table->decl_rows = vec_new(heap->arena);
  return table;
}

/* require(value, line, message) on a Maybe<String> */
static TranslateText require_text(ExportText value, uint32_t line, Str message) {
  if (value.present) return text_done(value.value);
  return text_fail(import_error(line, message));
}

/* ensure(ok, line, message) */
TranslateCheck translate_ensure(bool ok, uint32_t line, Str message) {
  if (ok) return check_done();
  return check_fail(import_error(line, message));
}

/* named(source, line, id) */
TranslateText translate_named(Heap *heap, const Export *source, uint32_t line, Str id) {
  return require_text(export_name_string(source, id), line, cat(heap, str_lit("missing source name "), id));
}

/* canonical(source, line, id) */
TranslateText translate_canonical(Heap *heap, const Export *source, uint32_t line, Str id) {
  return require_text(export_canonical_name(source, id), line, cat(heap, str_lit("missing source name identity "), id));
}

/* join(items, separator) from INDEX */
static Str join_from(Heap *heap, const Vec *items, size_t index, Str separator) {
  VecGet one = vec_get(items, index);
  if (one.status != VEC_GET_OK) return str_empty();
  VecGet two = vec_get(items, index + 1);
  if (two.status != VEC_GET_OK) return *(Str *)one.value;
  return cat(heap, *(Str *)one.value, cat(heap, separator, join_from(heap, items, index + 1, separator)));
}

/* join(items, separator) */
Str translate_join(Heap *heap, const Vec *items, Str separator) { return join_from(heap, items, 0, separator); }

/* named_all(ids, source, line) from INDEX: the display names as Str* items */
static TranslateList named_all_from(Heap *heap, const Vec *ids, size_t index, const Export *source, uint32_t line, Vec out) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return list_done(out);
  TranslateText name = translate_named(heap, source, line, *(Str *)got.value);
  if (name.kind == JSON_FAIL) return list_fail(name.error);
  return named_all_from(heap, ids, index + 1, source, line, with_item(heap, out, json_boxed_str(heap, name.value)));
}

/* named_all(ids, source, line) */
TranslateList translate_named_all(Heap *heap, const Vec *ids, const Export *source, uint32_t line) {
  return named_all_from(heap, ids, 0, source, line, vec_new(heap->arena));
}

/* type_nodes(value) .. decl_rows(value); the with_* of the Bend are in-place table_put calls */
Table *translate_type_nodes(Translated *value) { return &value->type_nodes; }
Table *translate_level_nodes(Translated *value) { return &value->level_nodes; }
Table *translate_summaries(Translated *value) { return &value->summaries; }
Table *translate_level_params(Translated *value) { return &value->level_params; }
Vec translate_decl_rows(const Translated *value) { return value->decl_rows; }

/* levelvisit_table .. exprbuild_summary */
static Translated *levelvisit_table(LevelVisit value) { return value.table; }
static TranslateIdSet levelvisit_params(LevelVisit value) { return value.params; }
static Translated *exprvisit_table(ExprVisit value) { return value.table; }
static TranslateSummary exprvisit_summary(ExprVisit value) { return value.summary; }
static Translated *exprbuild_table(ExprBuild value) { return value.table; }
static TranslateNode exprbuild_node(ExprBuild value) { return value.node; }
static TranslateSummary exprbuild_summary(ExprBuild value) { return value.summary; }

/* summary_binders(value), summary_params(value), summary_constants(value) */
Big translate_summary_binders(TranslateSummary value) { return value.binders; }
TranslateIdSet translate_summary_params(TranslateSummary value) { return value.params; }
TranslateIdSet translate_summary_constants(TranslateSummary value) { return value.constants; }

/* commit_level(table, id, node, params) */
static LevelVisit commit_level(Heap *heap, Translated *table, Str id, TranslateLevel node, TranslateIdSet params) {
  LevelVisit visit;
  table_put(heap, translate_level_nodes(table), id, box_level(heap, node));
  table_put(heap, translate_level_params(table), id, box_set(heap, params));
  visit.table = table;
  visit.params = params;
  return visit;
}

/* commit_expr(result, id) */
static ExprVisit commit_expr(Heap *heap, ExprBuild result, Str id) {
  ExprVisit visit;
  Translated *table = exprbuild_table(result);
  TranslateSummary summary = exprbuild_summary(result);
  table_put(heap, translate_type_nodes(table), id, box_node(heap, exprbuild_node(result)));
  table_put(heap, translate_summaries(table), id, box_summary(heap, summary));
  visit.table = table;
  visit.summary = summary;
  return visit;
}

/* law visit_level, law level_raw */
static LevelVisitResult visit_level(Heap *heap, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id);
static LevelVisitResult level_raw(Heap *heap, Level raw, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id);

/* A Level node of KIND with every text field empty. */
static TranslateLevel level_blank(TranslateLevelKind kind) {
  TranslateLevel node;
  node.kind = kind;
  node.child = str_empty();
  node.left = str_empty();
  node.right = str_empty();
  node.source_name = str_empty();
  node.name = str_empty();
  return node;
}

/* level_cached(cached, depth, source, line, table, id) */
static LevelVisitResult level_cached(Heap *heap, TableGet cached, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id) {
  if (cached.present) {
    LevelVisit visit;
    visit.table = table;
    visit.params = *(TranslateIdSet *)cached.value;
    return levelvisit_done(visit);
  }
  TranslateCheck depth_ok = translate_ensure(depth <= 1024, line, str_lit("type level DAG exceeds the depth limit of 1024"));
  if (depth_ok.kind == JSON_FAIL) return levelvisit_fail(depth_ok.error);
  ExportLevel raw = export_level(source, id);
  if (!raw.present) return levelvisit_fail(import_error(line, cat(heap, str_lit("missing source level "), id)));
  return level_raw(heap, raw.value, depth, source, line, table, id);
}

/* level_pair(depth, source, line, table, id, left, right, node) */
static LevelVisitResult level_pair(Heap *heap, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id, Str left, Str right, TranslateLevel node) {
  LevelVisitResult a = visit_level(heap, depth + 1, source, line, table, left);
  if (a.kind == JSON_FAIL) return a;
  LevelVisitResult b = visit_level(heap, depth + 1, source, line, levelvisit_table(a.value), right);
  if (b.kind == JSON_FAIL) return b;
  return levelvisit_done(commit_level(heap, levelvisit_table(b.value), id, node, translate_union(heap, levelvisit_params(a.value), levelvisit_params(b.value))));
}

/* LMax{left, right} or LImax{left, right} */
static TranslateLevel level_pair_node(TranslateLevelKind kind, Str left, Str right) {
  TranslateLevel node = level_blank(kind);
  node.left = left;
  node.right = right;
  return node;
}

/* level_raw(raw, depth, source, line, table, id) */
static LevelVisitResult level_raw(Heap *heap, Level raw, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id) {
  switch (raw.kind) {
    case LEVEL_ZERO:
      return levelvisit_done(commit_level(heap, table, id, level_blank(TRANSLATE_LZERO), translate_empty_set(heap)));
    case LEVEL_PARAM: {
      TranslateText name = translate_named(heap, source, line, raw.index);
      if (name.kind == JSON_FAIL) return levelvisit_fail(name.error);
      TranslateText identity = translate_canonical(heap, source, line, raw.index);
      if (identity.kind == JSON_FAIL) return levelvisit_fail(identity.error);
      TranslateLevel node = level_blank(TRANSLATE_LPARAM);
      node.source_name = identity.value;
      node.name = name.value;
      return levelvisit_done(commit_level(heap, table, id, node, translate_set_add(heap, translate_empty_set(heap), identity.value)));
    }
    case LEVEL_SUCC: {
      LevelVisitResult a = visit_level(heap, depth + 1, source, line, table, raw.index);
      if (a.kind == JSON_FAIL) return a;
      TranslateLevel node = level_blank(TRANSLATE_LSUCC);
      node.child = raw.index;
      return levelvisit_done(commit_level(heap, levelvisit_table(a.value), id, node, levelvisit_params(a.value)));
    }
    case LEVEL_MAX:
      return level_pair(heap, depth, source, line, table, id, raw.left, raw.right, level_pair_node(TRANSLATE_LMAX, raw.left, raw.right));
    case LEVEL_IMAX:
      return level_pair(heap, depth, source, line, table, id, raw.left, raw.right, level_pair_node(TRANSLATE_LIMAX, raw.left, raw.right));
  }
  return levelvisit_fail(import_error(line, cat(heap, str_lit("missing source level "), id)));
}

/* visit_level(depth, source, line, table, id) */
static LevelVisitResult visit_level(Heap *heap, uint32_t depth, const Export *source, uint32_t line, Translated *table, Str id) {
  return level_cached(heap, table_get(translate_level_params(table), id), depth, source, line, table, id);
}

/* visit_levels(ids, source, line, table, params) from INDEX */
static LevelVisitResult visit_levels_from(Heap *heap, const Vec *ids, size_t index, const Export *source, uint32_t line, Translated *table, TranslateIdSet params) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) {
    LevelVisit visit;
    visit.table = table;
    visit.params = params;
    return levelvisit_done(visit);
  }
  LevelVisitResult next = visit_level(heap, 0, source, line, table, *(Str *)got.value);
  if (next.kind == JSON_FAIL) return next;
  return visit_levels_from(heap, ids, index + 1, source, line, levelvisit_table(next.value), translate_union(heap, params, levelvisit_params(next.value)));
}

/* visit_levels(ids, source, line, table, params) */
static LevelVisitResult visit_levels(Heap *heap, const Vec *ids, const Export *source, uint32_t line, Translated *table, TranslateIdSet params) {
  return visit_levels_from(heap, ids, 0, source, line, table, params);
}

/* law visit_expr, law expr_raw */
static ExprVisitResult visit_expr(Heap *heap, uint32_t depth, const Export *source, Translated *table, Str id);
static ExprBuildResult expr_raw(Heap *heap, Expr raw, uint32_t depth, const Export *source, uint32_t line, Translated *table);

/* A Node of KIND with every field empty. */
static TranslateNode node_blank(TranslateNodeKind kind) {
  TranslateNode node;
  memset(&node, 0, sizeof(node));
  node.kind = kind;
  return node;
}

/* ExprBuild{table, node, summary} */
static ExprBuild build(Translated *table, TranslateNode node, TranslateSummary summary) {
  ExprBuild result;
  result.table = table;
  result.node = node;
  result.summary = summary;
  return result;
}

/* Summary{binders, params, constants} */
static TranslateSummary summary_of(Big binders, TranslateIdSet params, TranslateIdSet constants) {
  TranslateSummary summary;
  summary.binders = binders;
  summary.params = params;
  summary.constants = constants;
  return summary;
}

/* natural_summary(value, line) */
static SummaryResult natural_summary(Heap *heap, MaybeBig value, uint32_t line) {
  if (!value.some) return summary_fail(import_error(line, str_lit("a type has an invalid de Bruijn index")));
  return summary_done(summary_of(big_add(heap, value.value, big_one()), translate_empty_set(heap), translate_empty_set(heap)));
}

/* binder_node(pi, binder) */
static TranslateNode binder_node(bool pi, TranslateBinder binder) {
  TranslateNode node = node_blank(pi ? TRANSLATE_PI : TRANSLATE_LAMBDA);
  node.binder = binder;
  return node;
}

/* expr_binder(binder, depth, source, line, table, pi) */
static ExprBuildResult expr_binder(Heap *heap, ExprBinder binder, uint32_t depth, const Export *source, uint32_t line, Translated *table, bool pi) {
  TranslateText name = translate_named(heap, source, line, binder.name);
  if (name.kind == JSON_FAIL) return exprbuild_fail(name.error);
  ExprVisitResult a = visit_expr(heap, depth + 1, source, table, binder.typ);
  if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
  ExprVisitResult b = visit_expr(heap, depth + 1, source, exprvisit_table(a.value), binder.body);
  if (b.kind == JSON_FAIL) return exprbuild_fail(b.error);
  TranslateBinder named;
  named.name = name.value;
  named.typ = binder.typ;
  named.body = binder.body;
  named.info = binder.info;
  return exprbuild_done(build(exprvisit_table(b.value), binder_node(pi, named),
                              translate_merge(heap, exprvisit_summary(a.value), translate_under_binder(heap, exprvisit_summary(b.value)))));
}

/* decl_arity(declaration): List.length(levels) */
static size_t decl_arity(DeclEntry declaration) { return declaration.level_params.len; }

/* add_constant(summary, identity) */
static TranslateSummary add_constant(Heap *heap, TranslateSummary summary, Str identity) {
  summary.constants = translate_set_add(heap, set_copy(heap, summary.constants), identity);
  return summary;
}

/* expr_raw(raw, depth, source, line, table) */
static ExprBuildResult expr_raw(Heap *heap, Expr raw, uint32_t depth, const Export *source, uint32_t line, Translated *table) {
  switch (raw.kind) {
    case EXPR_BVAR: {
      TranslateCheck in_range = translate_ensure(!str_eq(raw.index, str_lit("4611686018427387903")), line, str_lit("a type has an invalid de Bruijn index"));
      if (in_range.kind == JSON_FAIL) return exprbuild_fail(in_range.error);
      SummaryResult summary = natural_summary(heap, big_of_decimal(heap, raw.index), line);
      if (summary.kind == JSON_FAIL) return exprbuild_fail(summary.error);
      TranslateNode node = node_blank(TRANSLATE_BOUND);
      node.index = raw.index;
      return exprbuild_done(build(table, node, summary.value));
    }
    case EXPR_SORT: {
      LevelVisitResult a = visit_level(heap, 0, source, line, table, raw.index);
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      TranslateNode node = node_blank(TRANSLATE_SORT);
      node.level = raw.index;
      return exprbuild_done(build(levelvisit_table(a.value), node, summary_of(big_zero(), levelvisit_params(a.value), translate_empty_set(heap))));
    }
    case EXPR_CONST: {
      TranslateText display = translate_named(heap, source, line, raw.name);
      if (display.kind == JSON_FAIL) return exprbuild_fail(display.error);
      ExportDeclaration declaration = export_declaration(source, raw.name);
      if (!declaration.present) return exprbuild_fail(import_error(line, cat(heap, str_lit("type references undeclared constant "), display.value)));
      size_t expected = decl_arity(declaration.value);
      size_t given = raw.universes.len;
      TranslateCheck arity = translate_ensure(
          expected == given, line,
          cat(heap, str_lit("constant "),
              cat(heap, display.value,
                  cat(heap, str_lit(" expects "),
                      cat(heap, show_size(heap, expected), cat(heap, str_lit(" universe arguments but has "), show_size(heap, given)))))));
      if (arity.kind == JSON_FAIL) return exprbuild_fail(arity.error);
      LevelVisitResult a = visit_levels(heap, &raw.universes, source, line, table, translate_empty_set(heap));
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      TranslateText identity = translate_canonical(heap, source, line, raw.name);
      if (identity.kind == JSON_FAIL) return exprbuild_fail(identity.error);
      TranslateNode node = node_blank(TRANSLATE_CONSTANT);
      node.source_name = raw.name;
      node.name = display.value;
      node.universes = raw.universes;
      return exprbuild_done(build(levelvisit_table(a.value), node,
                                  summary_of(big_zero(), levelvisit_params(a.value), translate_set_add(heap, translate_empty_set(heap), identity.value))));
    }
    case EXPR_APPLY: {
      ExprVisitResult a = visit_expr(heap, depth + 1, source, table, raw.fn);
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      ExprVisitResult b = visit_expr(heap, depth + 1, source, exprvisit_table(a.value), raw.arg);
      if (b.kind == JSON_FAIL) return exprbuild_fail(b.error);
      TranslateNode node = node_blank(TRANSLATE_APPLY);
      node.fn = raw.fn;
      node.arg = raw.arg;
      return exprbuild_done(build(exprvisit_table(b.value), node, translate_merge(heap, exprvisit_summary(a.value), exprvisit_summary(b.value))));
    }
    case EXPR_LAM:
      return expr_binder(heap, raw.binder, depth, source, line, table, false);
    case EXPR_FORALL:
      return expr_binder(heap, raw.binder, depth, source, line, table, true);
    case EXPR_LET: {
      TranslateText name = translate_named(heap, source, line, raw.name);
      if (name.kind == JSON_FAIL) return exprbuild_fail(name.error);
      ExprVisitResult a = visit_expr(heap, depth + 1, source, table, raw.typ);
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      ExprVisitResult b = visit_expr(heap, depth + 1, source, exprvisit_table(a.value), raw.value);
      if (b.kind == JSON_FAIL) return exprbuild_fail(b.error);
      ExprVisitResult c = visit_expr(heap, depth + 1, source, exprvisit_table(b.value), raw.body);
      if (c.kind == JSON_FAIL) return exprbuild_fail(c.error);
      TranslateNode node = node_blank(TRANSLATE_LET);
      node.name = name.value;
      node.typ = raw.typ;
      node.value = raw.value;
      node.body = raw.body;
      node.nondep = raw.nondep;
      return exprbuild_done(build(exprvisit_table(c.value), node,
                                  translate_merge(heap, translate_merge(heap, exprvisit_summary(a.value), exprvisit_summary(b.value)),
                                                  translate_under_binder(heap, exprvisit_summary(c.value)))));
    }
    case EXPR_PROJ: {
      TranslateText name = translate_named(heap, source, line, raw.type_name);
      if (name.kind == JSON_FAIL) return exprbuild_fail(name.error);
      TranslateText identity = translate_canonical(heap, source, line, raw.type_name);
      if (identity.kind == JSON_FAIL) return exprbuild_fail(identity.error);
      ExprVisitResult a = visit_expr(heap, depth + 1, source, table, raw.structure);
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      TranslateNode node = node_blank(TRANSLATE_PROJECTION);
      node.source_type_name = raw.type_name;
      node.type_name = name.value;
      node.index = raw.index;
      node.structure = raw.structure;
      return exprbuild_done(build(exprvisit_table(a.value), node, add_constant(heap, exprvisit_summary(a.value), identity.value)));
    }
    case EXPR_NAT_LITERAL: {
      TranslateNode node = node_blank(TRANSLATE_NAT_LITERAL);
      node.value = raw.text;
      return exprbuild_done(build(table, node, translate_empty_summary(heap)));
    }
    case EXPR_STRING_LITERAL: {
      TranslateNode node = node_blank(TRANSLATE_STRING_LITERAL);
      node.value = raw.text;
      return exprbuild_done(build(table, node, translate_empty_summary(heap)));
    }
    case EXPR_MDATA: {
      ExprVisitResult a = visit_expr(heap, depth + 1, source, table, raw.expr);
      if (a.kind == JSON_FAIL) return exprbuild_fail(a.error);
      TranslateNode node = node_blank(TRANSLATE_METADATA);
      node.expr = raw.expr;
      node.data = raw.data;
      return exprbuild_done(build(exprvisit_table(a.value), node, exprvisit_summary(a.value)));
    }
  }
  return exprbuild_fail(import_error(line, str_lit("a type has an invalid de Bruijn index")));
}

/* expr_cached(cached, depth, source, table, id) */
static ExprVisitResult expr_cached(Heap *heap, TableGet cached, uint32_t depth, const Export *source, Translated *table, Str id) {
  if (cached.present) {
    ExprVisit visit;
    visit.table = table;
    visit.summary = *(TranslateSummary *)cached.value;
    return exprvisit_done(visit);
  }
  /* Table.default(U32, Export.expr_line(source, id), 0) */
  ExportLine at = export_expr_line(source, id);
  uint32_t line = at.present ? at.value : 0;
  TranslateCheck depth_ok = translate_ensure(depth <= 1024, line, str_lit("type expression DAG exceeds the depth limit of 1024"));
  if (depth_ok.kind == JSON_FAIL) return exprvisit_fail(depth_ok.error);
  ExportExpr raw = export_expr(source, id);
  if (!raw.present) return exprvisit_fail(import_error(line, cat(heap, str_lit("missing source expression "), id)));
  ExprBuildResult built = expr_raw(heap, raw.value, depth, source, line, table);
  if (built.kind == JSON_FAIL) return exprvisit_fail(built.error);
  return exprvisit_done(commit_expr(heap, built.value, id));
}

/* visit_expr(depth, source, table, id) */
static ExprVisitResult visit_expr(Heap *heap, uint32_t depth, const Export *source, Translated *table, Str id) {
  return expr_cached(heap, table_get(translate_summaries(table), id), depth, source, table, id);
}

/* parameters(ids, source, line) from INDEX: TranslateParameter* items */
static TranslateList parameters_from(Heap *heap, const Vec *ids, size_t index, const Export *source, uint32_t line, Vec out) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return list_done(out);
  Str id = *(Str *)got.value;
  TranslateText name = translate_named(heap, source, line, id);
  if (name.kind == JSON_FAIL) return list_fail(name.error);
  TranslateText identity = translate_canonical(heap, source, line, id);
  if (identity.kind == JSON_FAIL) return list_fail(identity.error);
  TranslateParameter parameter;
  parameter.source_name = identity.value;
  parameter.name = name.value;
  return parameters_from(heap, ids, index + 1, source, line, with_item(heap, out, box_parameter(heap, parameter)));
}

/* parameters(ids, source, line) */
static TranslateList parameters(Heap *heap, const Vec *ids, const Export *source, uint32_t line) {
  return parameters_from(heap, ids, 0, source, line, vec_new(heap->arena));
}

/* bound_parameters(parameters, set) from INDEX */
static TranslateIdSet bound_parameters_from(Heap *heap, const Vec *parameters, size_t index, TranslateIdSet set) {
  VecGet got = vec_get(parameters, index);
  if (got.status != VEC_GET_OK) return set;
  return bound_parameters_from(heap, parameters, index + 1, translate_set_add(heap, set, ((TranslateParameter *)got.value)->source_name));
}

/* bound_parameters(parameters, set) */
static TranslateIdSet bound_parameters(Heap *heap, const Vec *parameters, TranslateIdSet set) {
  return bound_parameters_from(heap, parameters, 0, set);
}

/* not_bound(found, key, rest): the key kept when it is not bound */
static Vec not_bound(Heap *heap, bool found, Str key, Vec rest) {
  if (found) return rest;
  return with_item(heap, rest, json_boxed_str(heap, key));
}

/* unbound_keys(keys, bound) from INDEX, in key order */
static Vec unbound_keys_from(Heap *heap, const Vec *keys, size_t index, const TranslateIdSet *bound, Vec out) {
  VecGet got = vec_get(keys, index);
  if (got.status != VEC_GET_OK) return out;
  Str key = *(Str *)got.value;
  return unbound_keys_from(heap, keys, index + 1, bound, not_bound(heap, translate_set_has(bound, key), key, out));
}

/* unbound_keys(keys, bound) */
static Vec unbound_keys(Heap *heap, const Vec *keys, const TranslateIdSet *bound) {
  return unbound_keys_from(heap, keys, 0, bound, vec_new(heap->arena));
}

/* add_declaration(declaration, source, table) */
static TranslateResult add_declaration(Heap *heap, DeclEntry declaration, const Export *source, Translated *table) {
  uint32_t line = declaration.line;
  TranslateText name = translate_named(heap, source, line, declaration.name);
  if (name.kind == JSON_FAIL) return translate_fail(name.error);
  TranslateList params = parameters(heap, &declaration.level_params, source, line);
  if (params.kind == JSON_FAIL) return translate_fail(params.error);
  TranslateIdSet bound = bound_parameters(heap, &params.value, translate_empty_set(heap));
  TranslateCheck distinct = translate_ensure(table_size(&bound.values) == params.value.len, line,
                                             cat(heap, str_lit("duplicate universe parameters in "), name.value));
  if (distinct.kind == JSON_FAIL) return translate_fail(distinct.error);
  ExprVisitResult a = visit_expr(heap, 0, source, table, declaration.typ);
  if (a.kind == JSON_FAIL) return translate_fail(a.error);
  TranslateSummary summary = exprvisit_summary(a.value);
  TranslateCheck closed = translate_ensure(
      big_equal(translate_summary_binders(summary), big_zero()), line,
      cat(heap, str_lit("type of "),
          cat(heap, name.value,
              cat(heap, str_lit(" has "), cat(heap, big_to_string(heap, translate_summary_binders(summary)), str_lit(" unbound term binders"))))));
  if (closed.kind == JSON_FAIL) return translate_fail(closed.error);
  TranslateIdSet summary_params = translate_summary_params(summary);
  Vec param_keys = translate_set_keys(heap, &summary_params);
  Vec unbound_ids = unbound_keys(heap, &param_keys, &bound);
  Vec sorted_unbound = export_sort_keys(heap, &unbound_ids);
  TranslateList unbound = translate_named_all(heap, &sorted_unbound, source, line);
  if (unbound.kind == JSON_FAIL) return translate_fail(unbound.error);
  TranslateCheck universes = translate_ensure(
      unbound.value.len == 0, line,
      cat(heap, str_lit("type of "),
          cat(heap, name.value, cat(heap, str_lit(" has unbound universe parameters: "), translate_join(heap, &unbound.value, str_lit(", "))))));
  if (universes.kind == JSON_FAIL) return translate_fail(universes.error);
  TranslateIdSet summary_constants = translate_summary_constants(summary);
  Vec constant_keys = translate_set_keys(heap, &summary_constants);
  Vec sorted_constants = export_sort_keys(heap, &constant_keys);
  TranslateList dependencies = translate_named_all(heap, &sorted_constants, source, line);
  if (dependencies.kind == JSON_FAIL) return translate_fail(dependencies.error);
  Translated *visited = exprvisit_table(a.value);
  TranslateRow row;
  row.declaration = declaration;
  row.name = name.value;
  row.parameters = params.value;
  row.root = declaration.typ;
  row.dependencies = dependencies.value;
  visited->decl_rows = with_item(heap, visited->decl_rows, box_row(heap, row));
  return translate_done(visited);
}

/* translate_declarations(declarations, source, table) from INDEX: the rows are pushed in source order,
 * so the final List.reverse of the Bend is the identity here */
static TranslateResult translate_declarations_from(Heap *heap, const Vec *declarations, size_t index, const Export *source, Translated *table) {
  VecGet got = vec_get(declarations, index);
  if (got.status != VEC_GET_OK) return translate_done(table);
  TranslateResult next = add_declaration(heap, *(DeclEntry *)got.value, source, table);
  if (next.kind == JSON_FAIL) return next;
  return translate_declarations_from(heap, declarations, index + 1, source, next.value);
}

/* translate_declarations(declarations, source, table) */
static TranslateResult translate_declarations(Heap *heap, const Vec *declarations, const Export *source, Translated *table) {
  return translate_declarations_from(heap, declarations, 0, source, table);
}

/* translate(source); a failed allocation of the table reports line 0 (heap->oom is set as well) */
TranslateResult translate_translate(Heap *heap, const Export *source) {
  Translated *table = translate_empty(heap);
  if (table == NULL) return translate_fail(import_error(0, str_lit("out of memory")));
  Vec declarations = export_declarations(source);
  return translate_declarations(heap, &declarations, source, table);
}

/* rows(table) */
Vec translate_rows(const Translated *table) { return translate_decl_rows(table); }

/* indexed_value(item, id, rest): Indexed{id, value} appended when the row is present */
static Vec indexed_value(Heap *heap, TableGet item, Str id, Vec rest) {
  if (!item.present) return rest;
  return with_item(heap, rest, table_indexed(heap, id, item.value));
}

/* indexed(ids, table) from INDEX */
static Vec indexed_from(Heap *heap, const Vec *ids, size_t index, const Table *table, Vec out) {
  VecGet got = vec_get(ids, index);
  if (got.status != VEC_GET_OK) return out;
  Str id = *(Str *)got.value;
  return indexed_from(heap, ids, index + 1, table, indexed_value(heap, table_get(table, id), id, out));
}

/* indexed(ids, table) */
static Vec indexed(Heap *heap, const Vec *ids, const Table *table) { return indexed_from(heap, ids, 0, table, vec_new(heap->arena)); }

/* nodes(table) */
Vec translate_nodes(Heap *heap, Translated *table) {
  Table *nodes = translate_type_nodes(table);
  Vec keys = table_keys(heap, nodes);
  Vec sorted = export_sort_keys(heap, &keys);
  return indexed(heap, &sorted, nodes);
}

/* levels(table) */
Vec translate_levels(Heap *heap, Translated *table) {
  Table *levels = translate_level_nodes(table);
  Vec keys = table_keys(heap, levels);
  Vec sorted = export_sort_keys(heap, &keys);
  return indexed(heap, &sorted, levels);
}
