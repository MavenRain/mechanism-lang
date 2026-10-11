/* C port of bend2/tests/runtime_reachable.bend (Bend lines 1-100): the on-demand erasure of the
 * globals reachable from a set of exports, the source-ordered rows and the self test. The Bend
 * module has no main; c-build compiles this unit with -c and prelude_runtime (S5.1) links it.
 * Definitions keep the Bend order and names. */
#include "runtime_reachable.h"

/* Maybe<Erase.Entry> */
typedef struct {
  bool some;
  EraseEntry value;
} ReachMaybeEntry;

static SliceResult done_vec(Heap *heap, Vec vec) {
  Vec *boxed = slice_vec_new(heap, vec);
  if (boxed == NULL) return slice_fail(str_lit("host out of memory"));
  return slice_done(boxed);
}

static Vec one(Heap *heap, void *item) {
  Vec empty = vec_new(heap->arena);
  return slice_cons(heap, item, &empty);
}

/* Bend 11-14: kernel */
static SliceResult kernel(Heap *heap, Result result) {
  switch (result.kind) {
    case RESULT_FAIL: {
      Str text = error_to_string(heap, result.error);
      return slice_fail(text);
    }
    case RESULT_DONE: return slice_done(result.value);
  }
  return slice_done(result.value);
}

/* Bend 16-19: erased */
static SliceResult erased(Heap *heap, MaybeEntry found, Globals globals, Str name) {
  if (!found.some) {
    Str message = heap_str_cat(heap, str_lit("missing runtime dependency: "), name);
    return slice_fail(message);
  }
  return kernel(heap, erase_entry(heap, globals, name, found.value));
}

/* Bend 21-47: visit (law), visit_new and visit_seen */
static SliceResult visit(Heap *heap, Globals globals, const Vec *catalog, Vec rows, Vec seen, const Vec *pending);

static SliceResult visit_new(Heap *heap, SliceMaybeNames found, Globals globals, const Vec *catalog, Vec rows, Vec seen, Str name, const Vec *rest) {
  if (found.some) {
    Vec more_seen = slice_cons(heap, slice_name_new(heap, name), &seen);
    Vec pending = slice_append(heap, &found.value, rest);
    return visit(heap, globals, catalog, rows, more_seen, &pending);
  }
  {
    SliceResult entry = erased(heap, global_find(name, globals), globals, name);
    const EraseEntry *boxed;
    EraseRow *row;
    Vec one_row;
    SliceResult grown;
    if (slice_failed(entry)) return entry;
    boxed = entry.value;
    if (boxed == NULL) return slice_fail(str_lit("host out of memory"));
    row = erase_row_new(heap, name, *boxed);
    if (row == NULL) return slice_fail(str_lit("host out of memory"));
    one_row = one(heap, row);
    grown = slice_catalog_rows(heap, &one_row, catalog);
    if (slice_failed(grown)) return grown;
    {
      const Vec *next_catalog = grown.value;
      Vec more_rows = slice_cons(heap, row, &rows);
      Vec more_seen = slice_cons(heap, slice_name_new(heap, name), &seen);
      Vec deps = slice_dependencies(heap, slice_lookup(next_catalog, name));
      Vec pending = slice_append(heap, &deps, rest);
      return visit(heap, globals, next_catalog, more_rows, more_seen, &pending);
    }
  }
}

static SliceResult visit_seen(Heap *heap, bool visited, Globals globals, const Vec *catalog, Vec rows, Vec seen, Str name, const Vec *rest) {
  if (visited) return visit(heap, globals, catalog, rows, seen, rest);
  return visit_new(heap, slice_lookup(catalog, name), globals, catalog, rows, seen, name, rest);
}

static SliceResult visit(Heap *heap, Globals globals, const Vec *catalog, Vec rows, Vec seen, const Vec *pending) {
  if (pending->len == 0u) return done_vec(heap, rows);
  {
    Str name = slice_name_at(pending, 0u);
    Vec rest = slice_tail(heap, pending);
    return visit_seen(heap, slice_member(&seen, name), globals, catalog, rows, seen, name, &rest);
  }
}

/* Bend 49-53: find_row */
static ReachMaybeEntry find_row(const Vec *rows, Str name) {
  ReachMaybeEntry out;
  size_t i;
  out.some = false;
  out.value = erase_entry_dropped();
  for (i = 0; i < rows->len; i++) {
    const EraseRow *row = slice_at(rows, i);
    if (row != NULL && str_eq(row->name, name)) {
      out.some = true;
      out.value = row->entry;
      return out;
    }
  }
  return out;
}

/* Bend 55-58: keep_row */
static Vec keep_row(Heap *heap, ReachMaybeEntry found, Str name, const Vec *rest) {
  if (!found.some) return *rest;
  return slice_cons(heap, erase_row_new(heap, name, found.value), rest);
}

/* Bend 60-63: ordered (the Bend recursion conses from the last source row backwards) */
static Vec ordered(Heap *heap, const Vec *source, const Vec *rows) {
  Vec acc = vec_new(heap->arena);
  size_t i = source->len;
  while (i > 0u) {
    const CheckRow *row;
    i--;
    row = slice_at(source, i);
    if (row != NULL) acc = keep_row(heap, find_row(rows, row->name), row->name, &acc);
  }
  return acc;
}

/* Bend 65-68: select_many */
SliceResult reach_select_many(Heap *heap, Globals globals, const Vec *source, const Vec *exports) {
  Vec empty = vec_new(heap->arena);
  SliceResult rows = visit(heap, globals, &empty, empty, empty, exports);
  if (slice_failed(rows)) return rows;
  return done_vec(heap, ordered(heap, source, rows.value));
}

/* Bend 70-73: select */
SliceResult reach_select(Heap *heap, Globals globals, const Vec *source, Str export) {
  Vec exports = one(heap, slice_name_new(heap, export));
  SliceResult rows = reach_select_many(heap, globals, source, &exports);
  if (slice_failed(rows)) return rows;
  return slice_select(heap, rows.value, export);
}

/* Bend 75-78: missing */
static SliceResult missing(SliceResult result) {
  switch (result.kind) {
    case SLICE_FAIL: return slice_require(str_eq(result.message, str_lit("missing runtime dependency: missing")), str_lit("missing dependency diagnostic"));
    case SLICE_DONE: return slice_fail(str_lit("missing runtime dependency accepted"));
  }
  return slice_fail(str_lit("missing runtime dependency accepted"));
}

/* Bend 80-85: named_rows */
static Vec named_rows(Heap *heap, const Vec *rows, const Vec *names) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < rows->len; i++) {
    EraseRow *row = slice_at(rows, i);
    if (row == NULL) continue;
    if (slice_member(names, row->name)) {
      if (!vec_push(&out, row)) heap->oom = true;
    }
  }
  return out;
}

/* Bend 87-100: self_test */
SliceResult reach_self_test(Heap *heap, Globals globals, const Vec *source) {
  static const char *const three_names[3] = {"helper", "alias", "root"};
  static const char *const repeated_names[3] = {"root", "alias", "root"};
  Vec expected_names = slice_names(heap, three_names, 3u);
  const Vec *selected;
  {
    SliceResult chosen = reach_select(heap, globals, source, str_lit("root"));
    if (slice_failed(chosen)) return chosen;
    selected = chosen.value;
  }
  {
    Vec names = slice_row_names(heap, selected);
    SliceResult checked = slice_require(slice_same_names(&names, &expected_names), str_lit("reachable declaration order or unused declaration"));
    if (slice_failed(checked)) return checked;
  }
  {
    SliceResult full = kernel(heap, erase_program(heap, globals, source));
    SliceResult sliced;
    Vec expected;
    Str printed_selected;
    Str printed_expected;
    SliceResult checked;
    if (slice_failed(full)) return full;
    sliced = slice_select(heap, full.value, str_lit("root"));
    if (slice_failed(sliced)) return sliced;
    expected = named_rows(heap, sliced.value, &expected_names);
    printed_selected = erase_print(heap, selected);
    printed_expected = erase_print(heap, &expected);
    checked = slice_require(str_eq(printed_selected, printed_expected), str_lit("reachable erasure differs from complete erasure"));
    if (slice_failed(checked)) return checked;
  }
  {
    Vec exports = slice_names(heap, repeated_names, 3u);
    SliceResult shared = reach_select_many(heap, globals, source, &exports);
    Vec names;
    SliceResult checked;
    if (slice_failed(shared)) return shared;
    names = slice_row_names(heap, shared.value);
    checked = slice_require(slice_same_names(&names, &expected_names), str_lit("shared dependencies or repeated export"));
    if (slice_failed(checked)) return checked;
  }
  {
    SliceResult dropped = reach_select(heap, globals, source, str_lit("hidden"));
    Vec hidden;
    Str printed_dropped;
    Str printed_hidden;
    SliceResult checked;
    if (slice_failed(dropped)) return dropped;
    hidden = one(heap, erase_row_new(heap, str_lit("hidden"), erase_entry_dropped()));
    printed_dropped = erase_print(heap, dropped.value);
    printed_hidden = erase_print(heap, &hidden);
    checked = slice_require(str_eq(printed_dropped, printed_hidden), str_lit("dropped declaration traversal"));
    if (slice_failed(checked)) return checked;
  }
  {
    SliceResult checked = missing(reach_select(heap, globals, source, str_lit("missing")));
    if (slice_failed(checked)) return checked;
  }
  return slice_done(kernel_unit());
}
