/* Port of tests/relational_common.bend: the shared protocol of the relational test drivers.
 * Task(A) = IO(Result<String, A>) becomes Task {ok, message} with the payload in an out parameter
 * (the IO layer is the direct C call). A library module (no main): the prelude_* and relational_*
 * drivers link it together with relational_fs and relational_syntax_equal. */
#ifndef MECH_RELATIONAL_COMMON_H
#define MECH_RELATIONAL_COMMON_H

#include "surface.h"

/* Result<String, A>: ok with the payload in an out parameter, or the failure message. */
typedef struct {
  bool ok;
  Str message;
} Task;

/* Common.Pair<String, Term.T>: one row of computations(). */
typedef struct {
  Str name;
  const Term *expected;
} Computation;

/* Task.bind: stop at the first failure. */
#define RELATIONAL_TRY(task)                       \
  do {                                             \
    Task relational_try_ = (task);                 \
    if (!relational_try_.ok) return relational_try_; \
  } while (0)

Task task_pure(void);
Task task_fail(Str message);
Str *relational_str(Heap *heap, Str value);
Task relational_kernel(Heap *heap, Result result, void **value);
Task relational_require(bool ok, Str message);
Task relational_read(Heap *heap, Str root, Str path, Str *text);
Task relational_read_all(Heap *heap, Str root, const Vec *paths, Str *text);
Task relational_directory(Heap *heap, Str root, Str path, Vec *names);
Str relational_root(int argc, char **argv);
bool relational_strings(const Vec *a, const Vec *b);
Vec relational_sorted(Heap *heap, const Vec *values);
bool relational_same_names(Heap *heap, const Vec *a, const Vec *b);
Vec relational_prefix(Heap *heap, Str name, const Vec *members);
Vec relational_prefixes(Heap *heap, const Vec *names, const Vec *members);
bool relational_contains(const Vec *names, Str name);
Task relational_parse_roundtrip(Heap *heap, Str source, const Vec **parsed);
Task relational_checked(Heap *heap, Globals globals, Str source, const ElabChecked **checked);
Task relational_elaborated(Heap *heap, Globals globals, const Vec *parsed, const ElabChecked **checked);
size_t relational_entry_rows_length(const EntryRows *entries);
Vec relational_entry_names(Heap *heap, const Vec *rows);
bool relational_optional_nat(MaybeNat a, MaybeNat b);
bool relational_same_entry(Entry a, Entry b);
Task relational_audit_entry(Heap *heap, MaybeEntry initial, Str name, Entry entry);
Task relational_audit_entries(Heap *heap, const EntryRows *entries, Globals initial);
const FamilyRows *relational_new_families(Heap *heap, const FamilyRows *rows, Globals initial);
Vec relational_family_names(Heap *heap, const FamilyRows *rows);
Task relational_positive(Heap *heap, Family family, Str name);
Task relational_audit_families(Heap *heap, const FamilyRows *rows);
Task relational_inventories(Heap *heap, Globals initial, Globals globals, const Vec *rows, const Vec *expected_entries,
                            const Vec *expected_families);
Task relational_normalize(Heap *heap, Globals globals, Str name, const Term **term);
Task relational_computations(Heap *heap, const Vec *rows, Globals globals);
const Term *relational_natural(Heap *heap, uint32_t value);
const Term *relational_tiny(Heap *heap, uint64_t value);
Vec relational_filenames(Heap *heap, const Vec *names);
Task relational_negative_inventory(Heap *heap, Str root, Str path, const Vec *expected);
Task relational_refuse_exact(Heap *heap, Result result, Str expected, Str name);
Task relational_negative_exact(Heap *heap, Str root, Str directory, const Vec *names, Globals globals);
Callback relational_refuse_callback(void);
bool relational_budget_error(Result result);
Task relational_budget_refusal(Heap *heap, Str source);
int relational_finish(Heap *heap, Str tag, Task result, Str fields);
int relational_run(Heap *heap, Str tag, Task (*action)(Heap *heap, void *ctx, Str *fields), void *ctx);
Globals relational_globals(const ElabChecked *checked);
const Vec *relational_rows(const ElabChecked *checked);
bool relational_empty_symbolic(const ElabChecked *checked);
Task relational_counted_inventory(Heap *heap, const ElabChecked *checked, size_t count, const Vec *family_names_expected);
Task relational_all_families(Heap *heap, Globals globals);
bool relational_found(MaybeEntry entry);
Task relational_required(Heap *heap, const Vec *names, Globals globals);
const Term *relational_tiny_type(Heap *heap);
Task relational_quotations(Heap *heap, const Vec *names, Globals globals);

#endif
