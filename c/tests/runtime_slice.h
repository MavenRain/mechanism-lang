/* API of the C port of bend2/tests/runtime_slice.bend (c/tests/runtime_slice.c): the runtime slice of
 * an erased program, the erased functions reachable from one export.
 * Rows (List<Pair<String, Erase.Entry>>) is a Vec of EraseRow pointers, the shape erase_program and
 * erase_print use; Catalog (List<Pair<String, List<String>>>) is a Vec of SliceCatalogRow pointers;
 * a List<String> is a Vec of Str pointers boxed by slice_name_new. Lists stay immutable: slice_cons,
 * slice_tail and slice_append build fresh vectors in the Bend order. An allocation failure sets
 * heap->oom and the item is left out; callers read heap->oom before the result. */
#ifndef MECH_RUNTIME_SLICE_H
#define MECH_RUNTIME_SLICE_H

#include "kernel.h"

typedef struct {
  Str name;
  Vec deps; /* Str pointers */
} SliceCatalogRow;

/* Result<String, A> with A erased to a pointer: a Vec * (Rows or Catalog) or kernel_unit(). */
typedef enum { SLICE_DONE, SLICE_FAIL } SliceResultKind;
typedef struct {
  SliceResultKind kind;
  void *value; /* SLICE_DONE */
  Str message; /* SLICE_FAIL */
} SliceResult;

/* Maybe<List<String>>: value is read only when some. */
typedef struct {
  bool some;
  Vec value;
} SliceMaybeNames;

SliceResult slice_done(void *value);
SliceResult slice_fail(Str message);
bool slice_failed(SliceResult result);
Str *slice_name_new(Heap *heap, Str name);
Vec *slice_vec_new(Heap *heap, Vec vec);
void *slice_at(const Vec *xs, size_t index);
Str slice_name_at(const Vec *names, size_t index);
Vec slice_cons(Heap *heap, void *head, const Vec *rest);
Vec slice_tail(Heap *heap, const Vec *list);
Vec slice_append(Heap *heap, const Vec *a, const Vec *b);
Vec slice_names(Heap *heap, const char *const *names, size_t count);

bool slice_member(const Vec *names, Str name);
Vec slice_insert(Heap *heap, const Vec *names, Str name);
Vec slice_references(Heap *heap, const Vec *names, const Ktm *term);
SliceMaybeNames slice_lookup(const Vec *catalog, Str name);
SliceResult slice_catalog_rows(Heap *heap, const Vec *rows, const Vec *catalog);
Vec slice_dependencies(Heap *heap, SliceMaybeNames found);
SliceResult slice_select(Heap *heap, const Vec *rows, Str export);
SliceResult slice_require(bool condition, Str message);
Vec slice_row_names(Heap *heap, const Vec *rows);
bool slice_same_names(const Vec *left, const Vec *right);
SliceResult slice_self_test(Heap *heap);

#endif
