/* Port of tests/relational_program.bend (65 lines), same definitions in the same order.
 * G.initial() is global_initial(heap) (relational_common.c maps G.empty() to global_empty()); Elab.Rows is a Vec of CheckRow *. */
#include "relational_program.h"

static Task oom(void) { return task_fail(str_lit("out of memory")); }

static void *at_ptr(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* List.take / List.drop on a Vec of pointers. */
static Vec take(Heap *heap, const Vec *values, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < values->len && i < count; i++)
    if (!vec_push(&out, at_ptr(values, i))) {
      heap->oom = true;
      return out;
    }
  return out;
}

static Vec drop(Heap *heap, const Vec *values, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = count; i < values->len; i++)
    if (!vec_push(&out, at_ptr(values, i))) {
      heap->oom = true;
      return out;
    }
  return out;
}

Task program_probes(Heap *heap, const Vec *rows, Globals globals) {
  for (size_t i = 0; i < rows->len; i++) {
    const Probe *probe = at_ptr(rows, i);
    if (probe == NULL) return oom();
    Str payload = str_of_u64(heap->arena, probe->payload);
    Str source = cat(heap, cat(heap, cat(heap, str_lit("def lawProbe : Nat := "), probe->name), str_lit(" ")), payload);
    const ElabChecked *checked = NULL;
    const Term *actual = NULL;
    RELATIONAL_TRY(relational_checked(heap, globals, source, &checked));
    RELATIONAL_TRY(relational_normalize(heap, relational_globals(checked), str_lit("lawProbe"), &actual));
    Str message = cat(heap, cat(heap, cat(heap, str_lit("wrong computation: "), probe->name), str_lit("/")), payload);
    RELATIONAL_TRY(relational_require(term_equal(actual, relational_natural(heap, probe->expected)), message));
  }
  return task_pure();
}

Task program_inventory(Heap *heap, const ElabChecked *checked, const Vec *entries, const Vec *families) {
  return relational_inventories(heap, global_initial(heap), relational_globals(checked), relational_rows(checked), entries,
                                families);
}

Task program_sequential(Heap *heap, const Vec *declarations, const ElabChecked *checked, const ElabChecked **out) {
  for (size_t i = 0; i < declarations->len; i++) {
    Vec one = vec_new(heap->arena);
    if (!vec_push(&one, at_ptr(declarations, i))) return oom();
    const ElabChecked *next = NULL;
    RELATIONAL_TRY(relational_elaborated(heap, relational_globals(checked), &one, &next));
    ElabChecked *merged = heap_alloc(heap, sizeof *merged);
    if (merged == NULL) return oom();
    merged->globals = relational_globals(next);
    merged->rows = vec_concat(heap->arena, relational_rows(checked), relational_rows(next));
    checked = merged;
  }
  *out = checked;
  return task_pure();
}

Task program_staged(Heap *heap, Str templates, Str fixture, size_t header_size, const ElabChecked **out) {
  void *template_value = NULL;
  void *declarations_value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, templates), &template_value));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, fixture), &declarations_value));
  const Vec *template = template_value;
  const Vec *declarations = declarations_value;
  if (template == NULL || declarations == NULL) return oom();
  Vec header = take(heap, declarations, header_size);
  RELATIONAL_TRY(relational_require(header.len == header_size, str_lit("runtime fixture header missing")));
  Vec program = vec_concat(heap->arena, template, &header);
  const ElabChecked *checked = NULL;
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), &program, &checked));
  Vec rest = drop(heap, declarations, header_size);
  return program_sequential(heap, &rest, checked, out);
}

/* new_entries: the rows whose name is not an entry of initial, as CheckRow * so that
 * relational_entry_names reads them. */
Vec program_new_entries(Heap *heap, const EntryRows *entries, Globals initial) {
  Vec out = vec_new(heap->arena);
  for (const EntryRows *row = entries; row != NULL; row = row->rest) {
    if (relational_found(global_find(row->name, initial))) continue;
    CheckRow *kept = heap_alloc(heap, sizeof *kept);
    if (kept == NULL || !vec_push(&out, kept)) {
      heap->oom = true;
      return out;
    }
    kept->name = row->name;
    kept->entry = row->entry;
  }
  return out;
}

Task program_global_inventory(Heap *heap, Globals globals, const Vec *expected) {
  Vec fresh = program_new_entries(heap, globals.entries, global_initial(heap));
  Vec names = relational_entry_names(heap, &fresh);
  return relational_require(relational_same_names(heap, &names, expected), str_lit("global entry inventory changed"));
}

size_t program_globals_count(Globals globals) { return relational_entry_rows_length(globals.entries); }

size_t program_initial_count(Heap *heap) { return program_globals_count(global_initial(heap)); }

Task program_count_initial(Heap *heap, const ElabChecked *checked, size_t count, const Vec *expected_families) {
  const EntryRows *entries = checked->globals.entries;
  const Vec *rows = relational_rows(checked);
  Vec fresh = program_new_entries(heap, entries, global_initial(heap));
  bool ok = rows->len == count && fresh.len == count && relational_entry_rows_length(entries) == count + program_initial_count(heap);
  RELATIONAL_TRY(relational_require(ok, str_lit("entry inventory changed")));
  Vec names = relational_entry_names(heap, rows);
  return relational_inventories(heap, global_initial(heap), global_of_lists(heap, entries, checked->globals.families), rows,
                                &names, expected_families);
}
