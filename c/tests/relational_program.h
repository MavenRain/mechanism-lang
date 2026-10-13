/* Port of bend2/tests/relational_program.bend: program-level helpers of the relational test
 * drivers (law probes, staged elaboration, entry inventories). A library module (no main). */
#ifndef MECH_RELATIONAL_PROGRAM_H
#define MECH_RELATIONAL_PROGRAM_H

#include "relational_common.h"

/* Probe{name, payload, expected} */
typedef struct {
  Str name;
  uint32_t payload;
  uint32_t expected;
} Probe;

Task program_probes(Heap *heap, const Vec *rows, Globals globals);
Task program_inventory(Heap *heap, const ElabChecked *checked, const Vec *entries, const Vec *families);
Task program_sequential(Heap *heap, const Vec *declarations, const ElabChecked *checked, const ElabChecked **out);
Task program_staged(Heap *heap, Str templates, Str fixture, size_t header_size, const ElabChecked **out);
Vec program_new_entries(Heap *heap, const EntryRows *entries, Globals initial);
Task program_global_inventory(Heap *heap, Globals globals, const Vec *expected);
size_t program_globals_count(Globals globals);
size_t program_initial_count(Heap *heap);
Task program_count_initial(Heap *heap, const ElabChecked *checked, size_t count, const Vec *expected_families);

#endif
