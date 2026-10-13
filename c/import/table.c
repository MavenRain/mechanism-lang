/* Port of bend2/import/table.bend (import "Table"): Table.T<A> = Map<String, Maybe<A>>, the index
 * tables of the export reader. Keys are index texts; rows are heap-boxed values held as void*. The
 * Bend table is persistent and threaded linearly through the reader, so the C table is updated in
 * place. Every message text is the Bend text, byte for byte. */
#include "import.h"

/* empty() */
Table table_empty(Heap *heap) {
  Table table;
  table.tree = map_new(heap->arena);
  return table;
}

/* put(table, key, value) */
void table_put(Heap *heap, Table *table, Str key, void *value) {
  if (value == NULL) return;
  if (map_put(&table->tree, key, value) == MAP_PUT_OOM) heap->oom = true;
}

/* get_result(result): the Maybe of a Map.get (None{} when the key is absent). */
static TableGet get_result(MapGet result) {
  TableGet value;
  value.present = result.found;
  value.value = result.found ? result.value : NULL;
  return value;
}

/* get(table, key) */
TableGet table_get(const Table *table, Str key) { return get_result(map_get(&table->tree, key)); }

/* present(value) */
bool table_present(TableGet value) { return value.present; }

/* has(table, key) */
bool table_has(const Table *table, Str key) { return table_present(table_get(table, key)); }

typedef struct {
  Heap *heap;
  Vec keys;
} KeysContext;

static void keys_each(void *ctx, Str key, void *value) {
  KeysContext *context = ctx;
  Str *box = json_boxed_str(context->heap, key);
  (void)value;
  if (box == NULL) return;
  context->keys = json_vec_with(context->heap, context->keys, box);
}

/* keys(table) */
Vec table_keys(Heap *heap, const Table *table) {
  KeysContext context;
  context.heap = heap;
  context.keys = vec_new(heap->arena);
  map_each(&table->tree, keys_each, &context);
  return context.keys;
}

/* size(table): List.length(keys(table)) */
size_t table_size(const Table *table) { return table->tree.len; }

/* default(value, fallback) */
void *table_default(TableGet value, void *fallback) { return value.present ? value.value : fallback; }

/* require(value, message) */
TableLookup table_require(TableGet value, Str message) {
  TableLookup result;
  result.kind = value.present ? JSON_DONE : JSON_FAIL;
  result.value = value.value;
  result.message = value.present ? str_empty() : message;
  return result;
}

/* lookup(table, kind, index): "undefined " ++ kind ++ " index " ++ index on a miss. */
TableLookup table_lookup(Heap *heap, const Table *table, Str kind, Str index) {
  TableGet value = table_get(table, index);
  if (value.present) return table_require(value, str_empty());
  return table_require(value, heap_str_cat(heap, heap_str_cat(heap, str_lit("undefined "), kind),
                                           heap_str_cat(heap, str_lit(" index "), index)));
}

/* reference_present(yes, kind, index) */
TableCheck table_reference_present(Heap *heap, bool yes, Str kind, Str index) {
  TableCheck result;
  result.kind = yes ? JSON_DONE : JSON_FAIL;
  result.message = yes ? str_empty()
                       : heap_str_cat(heap, heap_str_cat(heap, str_lit("undefined or forward "), kind),
                                      heap_str_cat(heap, str_lit(" index "), index));
  return result;
}

/* references(indices, table, kind) from the item at INDEX: the first missing index fails. */
static TableCheck references_from(Heap *heap, const Vec *indices, size_t index, const Table *table, Str kind) {
  VecGet got = vec_get(indices, index);
  if (got.status != VEC_GET_OK) return table_reference_present(heap, true, kind, str_empty());
  {
    Str *text = got.value;
    TableCheck checked = table_reference_present(heap, table_has(table, *text), kind, *text);
    if (checked.kind == JSON_FAIL) return checked;
  }
  return references_from(heap, indices, index + 1, table, kind);
}

/* references(indices, table, kind) */
TableCheck table_references(Heap *heap, const Vec *indices, const Table *table, Str kind) {
  return references_from(heap, indices, 0, table, kind);
}

/* Indexed{index, value}, heap-boxed */
TableIndexed *table_indexed(Heap *heap, Str index, void *value) {
  TableIndexed *box = heap_alloc(heap, sizeof(TableIndexed));
  if (box == NULL) return NULL;
  box->index = index;
  box->value = value;
  return box;
}
