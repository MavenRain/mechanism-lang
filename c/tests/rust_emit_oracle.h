/* Public part of the C port of bend2/tests/rust_emit_oracle.bend (a library module of the
 * rust_emit driver; the Bend file has no main). Each entry is the Bend `IO(Unit)` of the same
 * name: it writes its output on stdout or stderr and returns the exit code. `paths` holds Str
 * pointers (the Bend List<String>, in order). */
#ifndef MECHANISM_RUST_EMIT_ORACLE_H
#define MECHANISM_RUST_EMIT_ORACLE_H

#include "rust.h"

int rust_emit_oracle_values(Heap *heap, const Vec *paths);
int rust_emit_oracle_probes(Heap *heap, const Vec *paths);
int rust_emit_oracle_bin(Heap *heap, const Vec *paths);

#endif
