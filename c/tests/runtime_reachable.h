/* API of the C port of bend2/tests/runtime_reachable.bend (c/tests/runtime_reachable.c): the
 * reachable runtime slice erased on demand from checked globals. SourceRows
 * (List<Pair<String, Global.Entry>>) is a Vec of CheckRow pointers, the shape erase_program takes;
 * results are SliceResult values whose Done payload is a Vec * of EraseRow pointers (Slice.Rows)
 * or kernel_unit(). The Bend module has no main: prelude_runtime (S5.1) runs reach_self_test. */
#ifndef MECH_RUNTIME_REACHABLE_H
#define MECH_RUNTIME_REACHABLE_H

#include "runtime_slice.h"

SliceResult reach_select_many(Heap *heap, Globals globals, const Vec *source, const Vec *exports);
SliceResult reach_select(Heap *heap, Globals globals, const Vec *source, Str export);
SliceResult reach_self_test(Heap *heap, Globals globals, const Vec *source);

#endif
