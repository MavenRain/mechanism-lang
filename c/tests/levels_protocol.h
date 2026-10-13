/* Shared helpers of bend2/tests/levels_protocol.bend (imported as Test by kernel_large_addresses,
 * kernel_conversion_budget): R, require, kernel, expect, measure, completed and the poll callback.
 * The driver object keeps its main; the library object (MECH_DRIVER_LIBRARY) drops it. */
#ifndef MECH_LEVELS_PROTOCOL_H
#define MECH_LEVELS_PROTOCOL_H

#include "kernel.h"

/* R(A) = Result<String, A>: Done carries the kernel Result value pointer, Fail the message. */
typedef struct {
  bool ok;
  const void *value;
  Str error;
} TR;

/* Common.Pair<U32, Result<E.T, A>> returned by measure (boxed on the heap). */
typedef struct {
  uint32_t count;
  Result result;
} Measured;

/* Common.Pair<U32, A> returned by completed (boxed on the heap). */
typedef struct {
  uint32_t count;
  const void *value;
} Completed;

TR test_done(const void *value);
TR test_fail(Str message);
TR test_require(bool ok, Str message);
TR test_kernel(Heap *heap, Result result);
TR test_expect(Heap *heap, Result result, Str prefix, MaybeStr message);
/* callback(limit): None = never spent; Some{left} = spent after left polls. */
Callback test_callback(Heap *heap, bool some, uint32_t left);
/* measure(action, limit): Done value is a Measured *. */
TR test_measure(Heap *heap, Comp action, bool some, uint32_t limit);
/* completed(measured): Done value is a Completed *. */
TR test_completed(Heap *heap, Measured measured);

/* Suite entries: each runs one driver main body on the shared heap and returns the exit code;
 * kernel_numeric_regressions sequences them as the Bend composite does. */
int kernel_large_addresses_run(Heap *heap);
int kernel_projection_motive_run(Heap *heap);
int kernel_conversion_budget_run(Heap *heap);
int levels_protocol_run(Heap *heap);

#endif
