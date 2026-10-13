/* Port of bend2/tests/relational_common.bend, same definition order. Task(A) is Task {ok, message}
 * plus an out parameter; lift, Task.pure, task_next and Task.bind collapse into task_pure, task_fail
 * and RELATIONAL_TRY. The file effects (io.bend read_file, relational_fs) run inline: the error text
 * is `path: message` with message = strerror(errno) (the Bend effect's text for a NULL message). */
#include "relational_common.h"

#include <errno.h>
#include <string.h>

#include "driver.h"
#include "relational_fs.h"
#include "relational_syntax_equal.h"

/* Task.pure / lift(Done) */
Task task_pure(void) { return (Task){true, str_empty()}; }

/* lift(Fail) */
Task task_fail(Str message) { return (Task){false, message}; }

static Str cat(Heap *heap, Str a, Str b) { return heap_str_cat(heap, a, b); }
static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

/* Box a Str for the Str-pointer vectors (List<String>). */
Str *relational_str(Heap *heap, Str value) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = value;
  return boxed;
}

static Str at_str(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE || got.value == NULL) return str_empty();
  return *(const Str *)got.value;
}

static void *at_ptr(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return NULL;
  return got.value;
}

static Task push_str(Heap *heap, Vec *out, Str value) {
  Str *boxed = relational_str(heap, value);
  if (boxed == NULL || !vec_push(out, boxed)) {
    heap->oom = true;
    return task_fail(str_lit("host out of memory"));
  }
  return task_pure();
}

static Str errno_text(Heap *heap, int code) { return str_copy(heap->arena, str_lit(strerror(code))); }

/* kernel(result) */
Task relational_kernel(Heap *heap, Result result, void **value) {
  if (result.kind == RESULT_FAIL) return task_fail(error_to_string(heap, result.error));
  if (value != NULL) *value = result.value;
  return task_pure();
}

/* require(ok, message); assert is the same Task. */
Task relational_require(bool ok, Str message) {
  if (ok) return task_pure();
  return task_fail(message);
}

static int fs_errno(FsResult result) {
  switch (result.status) {
  case FS_OK: return 0;
  case FS_ERRNO: return result.code;
  case FS_OOM: return ENOMEM;
  case FS_BAD_PATH: return EILSEQ;
  }
  return EIO;
}

/* read(root, path) = FileIO.read_file(root ++ "/" ++ path); the error text names the full path. */
Task relational_read(Heap *heap, Str root, Str path, Str *text) {
  Str full = cat3(heap, root, str_lit("/"), path);
  FsResult result = fs_read(heap->arena, full);
  int code = fs_errno(result);
  if (code != 0) return task_fail(cat3(heap, full, str_lit(": "), errno_text(heap, code)));
  *text = result.value;
  return task_pure();
}

/* read_all(root, paths) = "\n" ++ head ++ read_all(rest) */
Task relational_read_all(Heap *heap, Str root, const Vec *paths, Str *text) {
  Str acc = str_empty();
  for (size_t i = 0; i < paths->len; i++) {
    Str head = str_empty();
    RELATIONAL_TRY(relational_read(heap, root, at_str(paths, i), &head));
    acc = cat3(heap, acc, str_lit("\n"), head);
  }
  *text = acc;
  return task_pure();
}

/* directory(root, path): FileIO.convert names the relative path in the error text. */
Task relational_directory(Heap *heap, Str root, Str path, Vec *names) {
  RelationalDir listed = relational_list_dir(heap, cat3(heap, root, str_lit("/"), path));
  if (listed.code != 0) return task_fail(cat3(heap, path, str_lit(": "), errno_text(heap, listed.code)));
  *names = listed.names;
  return task_pure();
}

/* root(args) */
Str relational_root(int argc, char **argv) {
  if (argc > 1) return str_lit(argv[1]);
  return str_lit(".");
}

/* strings(a, b) */
bool relational_strings(const Vec *a, const Vec *b) {
  if (a->len != b->len) return false;
  for (size_t i = 0; i < a->len; i++)
    if (!str_eq(at_str(a, i), at_str(b, i))) return false;
  return true;
}

/* sorted(values) = List.sort(String.is_le): a stable insertion sort. */
Vec relational_sorted(Heap *heap, const Vec *values) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < values->len; i++) {
    void *item = at_ptr(values, i);
    if (item == NULL || !vec_push(&out, item)) {
      heap->oom = true;
      return out;
    }
    size_t j = out.len - 1;
    while (j > 0 && str_cmp(at_str(&out, j - 1), *(const Str *)item) > 0) {
      out.items[j] = out.items[j - 1];
      j--;
    }
    out.items[j] = item;
  }
  return out;
}

/* same_names(a, b) */
bool relational_same_names(Heap *heap, const Vec *a, const Vec *b) {
  Vec sa = relational_sorted(heap, a);
  Vec sb = relational_sorted(heap, b);
  return relational_strings(&sa, &sb);
}

/* prefix(name, members) */
Vec relational_prefix(Heap *heap, Str name, const Vec *members) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < members->len; i++) {
    Task pushed = push_str(heap, &out, cat3(heap, name, str_lit("_"), at_str(members, i)));
    if (!pushed.ok) return out;
  }
  return out;
}

/* prefixes(names, members) */
Vec relational_prefixes(Heap *heap, const Vec *names, const Vec *members) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    Vec part = relational_prefix(heap, at_str(names, i), members);
    out = vec_concat(heap->arena, &out, &part);
  }
  return out;
}

/* contains(names, name) */
bool relational_contains(const Vec *names, Str name) {
  for (size_t i = 0; i < names->len; i++)
    if (str_eq(at_str(names, i), name)) return true;
  return false;
}

/* parse_roundtrip(source) */
Task relational_parse_roundtrip(Heap *heap, Str source, const Vec **parsed) {
  void *first = NULL;
  void *second = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, source), &first));
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, syntax_print(heap, first)), &second));
  RELATIONAL_TRY(relational_require(syntax_equal_declarations(first, second), str_lit("parse/print changed")));
  *parsed = first;
  return task_pure();
}

/* checked(globals, source) */
Task relational_checked(Heap *heap, Globals globals, Str source, const ElabChecked **checked) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, elab_check_in_compact(heap, globals, source), &value));
  *checked = value;
  return task_pure();
}

/* elaborated(globals, parsed) */
Task relational_elaborated(Heap *heap, Globals globals, const Vec *parsed, const ElabChecked **checked) {
  void *value = NULL;
  RELATIONAL_TRY(relational_kernel(heap, elab_elab_program_in_compact(heap, globals, parsed), &value));
  *checked = value;
  return task_pure();
}

/* entry_names(rows): rows is a Vec of CheckRow pointers. */
Vec relational_entry_names(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    CheckRow *row = at_ptr(rows, i);
    if (row == NULL || !vec_push(&out, &row->name)) {
      heap->oom = true;
      return out;
    }
  }
  return out;
}

/* optional_nat(a, b) */
bool relational_optional_nat(MaybeNat a, MaybeNat b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return nat_cmp(a.value, b.value) == 0;
}

/* same_entry(a, b) */
bool relational_same_entry(Entry a, Entry b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
  case ENTRY_DEF:
    return term_equal(a.def.ty, b.def.ty) && term_equal(a.def.body, b.def.body) && a.def.reducible == b.def.reducible &&
           relational_optional_nat(a.def.rec_arg, b.def.rec_arg) && a.def.partial == b.def.partial;
  case ENTRY_AXIOM: return term_equal(a.ty, b.ty);
  case ENTRY_PRIM: return term_equal(a.ty, b.ty) && str_eq(prim_name(a.prim), prim_name(b.prim));
  }
  return false;
}

/* audit_entry(initial, name, entry) */
Task relational_audit_entry(Heap *heap, MaybeEntry initial, Str name, Entry entry) {
  if (initial.some)
    return relational_require(relational_same_entry(initial.value, entry), cat(heap, str_lit("builtin changed: "), name));
  switch (entry.kind) {
  case ENTRY_DEF: return task_pure();
  case ENTRY_AXIOM:
  case ENTRY_PRIM: return task_fail(cat(heap, str_lit("trusted entry: "), name));
  }
  return task_fail(cat(heap, str_lit("trusted entry: "), name));
}

/* audit_entries(entries, initial): entries is the Globals cons list. */
Task relational_audit_entries(Heap *heap, const EntryRows *entries, Globals initial) {
  for (const EntryRows *row = entries; row != NULL; row = row->rest)
    RELATIONAL_TRY(relational_audit_entry(heap, global_find(row->name, initial), row->name, row->entry));
  return task_pure();
}

/* new_family(original, row, rest) + new_families(rows, initial) */
const FamilyRows *relational_new_families(Heap *heap, const FamilyRows *rows, Globals initial) {
  if (rows == NULL) return NULL;
  const FamilyRows *rest = relational_new_families(heap, rows->rest, initial);
  if (global_find_family(rows->name, initial).some) return rest;
  return global_family_rows(heap, rows->name, rows->family, rest);
}

/* family_names(rows) */
Vec relational_family_names(Heap *heap, const FamilyRows *rows) {
  Vec out = vec_new(heap->arena);
  for (const FamilyRows *row = rows; row != NULL; row = row->rest) {
    if (!vec_push(&out, (void *)&row->name)) {
      heap->oom = true;
      return out;
    }
  }
  return out;
}

/* positive(family, name) */
Task relational_positive(Heap *heap, Family family, Str name) {
  switch (family.status.kind) {
  case CTOR_STATUS_COMPLETE:
    return relational_require(family.positive, cat(heap, str_lit("unchecked family: "), name));
  case CTOR_STATUS_PROVISIONAL:
  case CTOR_STATUS_BUILTIN: return task_fail(cat(heap, str_lit("unexpected family: "), name));
  }
  return task_fail(cat(heap, str_lit("unexpected family: "), name));
}

/* audit_families(rows) */
Task relational_audit_families(Heap *heap, const FamilyRows *rows) {
  for (const FamilyRows *row = rows; row != NULL; row = row->rest)
    RELATIONAL_TRY(relational_positive(heap, row->family, row->name));
  return task_pure();
}

/* inventories(initial, globals, rows, expected_entries, expected_families) */
Task relational_inventories(Heap *heap, Globals initial, Globals globals, const Vec *rows, const Vec *expected_entries,
                            const Vec *expected_families) {
  Vec names = relational_entry_names(heap, rows);
  Vec axioms = elab_axiom_names(heap, rows);
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &names, expected_entries), str_lit("entry inventory changed")));
  RELATIONAL_TRY(relational_require(relational_strings(&axioms, &none), str_lit("unexpected axioms")));
  RELATIONAL_TRY(relational_audit_entries(heap, globals.entries, initial));
  const FamilyRows *families = relational_new_families(heap, globals.families, initial);
  Vec family_names = relational_family_names(heap, families);
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, expected_families),
                                    str_lit("family inventory changed")));
  return relational_audit_families(heap, families);
}

/* normalize(globals, name) */
Task relational_normalize(Heap *heap, Globals globals, Str name, const Term **term) {
  void *value = NULL;
  void *normal = NULL;
  Vec env = vec_new(heap->arena);
  RELATIONAL_TRY(relational_kernel(heap, eval_eval(heap, globals, env, term_global(heap, name)), &value));
  RELATIONAL_TRY(relational_kernel(heap, eval_normalize(heap, globals, nat_zero(), value), &normal));
  *term = normal;
  return task_pure();
}

/* computations(rows, globals): rows is a Vec of Computation pointers. */
Task relational_computations(Heap *heap, const Vec *rows, Globals globals) {
  Vec none = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    const Computation *row = at_ptr(rows, i);
    if (row == NULL) return task_fail(str_lit("wrong computation: "));
    const Term *actual = NULL;
    RELATIONAL_TRY(relational_normalize(heap, globals, row->name, &actual));
    RELATIONAL_TRY(relational_require(term_equal(actual, row->expected),
                                      cat3(heap, cat(heap, str_lit("wrong computation: "), row->name), str_lit(" = "),
                                           pp_term(heap, &none, actual))));
  }
  return task_pure();
}

/* natural(value) */
const Term *relational_natural(Heap *heap, uint32_t value) { return term_lit(heap, literal_lint(big_of_u32(heap, value))); }

/* tiny(value): zero, then value applications of next. */
const Term *relational_tiny(Heap *heap, uint64_t value) {
  Vec none = vec_new(heap->arena);
  const Term *term = term_in(heap, shape_smu(str_lit("Tiny"), none), term_addr_ctor(str_lit("zero")), none);
  for (uint64_t i = 0; i < value; i++) {
    Vec args = vec_new(heap->arena);
    if (!vec_push(&args, (void *)term)) {
      heap->oom = true;
      return NULL;
    }
    term = term_in(heap, shape_smu(str_lit("Tiny"), none), term_addr_ctor(str_lit("next")), args);
  }
  return term;
}

/* filename_entry(keep, name, rest) + filenames(names): the .mech names without the extension. */
Vec relational_filenames(Heap *heap, const Vec *names) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    if (!str_ends_with(name, str_lit(".mech"))) continue;
    Task pushed = push_str(heap, &out, str_n(name.data, name.len - 5));
    if (!pushed.ok) return out;
  }
  return out;
}

/* negative_inventory(root, path, expected) */
Task relational_negative_inventory(Heap *heap, Str root, Str path, const Vec *expected) {
  Vec names = vec_new(heap->arena);
  RELATIONAL_TRY(relational_directory(heap, root, path, &names));
  Vec files = relational_filenames(heap, &names);
  return relational_require(relational_same_names(heap, &files, expected), str_lit("negative inventory changed"));
}

/* refuse_exact(result, expected, name) */
Task relational_refuse_exact(Heap *heap, Result result, Str expected, Str name) {
  if (result.kind == RESULT_DONE) return task_fail(cat(heap, name, str_lit(": expected refusal")));
  Str actual = cat(heap, error_to_string(heap, result.error), str_lit("\n"));
  return relational_require(str_eq(actual, expected), cat3(heap, name, str_lit(": "), actual));
}

/* negative_exact(root, directory, names, globals) */
Task relational_negative_exact(Heap *heap, Str root, Str directory, const Vec *names, Globals globals) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    Str text = str_empty();
    Str expected = str_empty();
    RELATIONAL_TRY(relational_read(heap, root, cat3(heap, directory, name, str_lit(".mech")), &text));
    RELATIONAL_TRY(relational_read(heap, root, cat3(heap, directory, name, str_lit(".err")), &expected));
    RELATIONAL_TRY(relational_refuse_exact(heap, elab_check_in_compact(heap, globals, text), expected, name));
  }
  return task_pure();
}

/* refuse_callback(): every poll answers (refuse_callback(), True). */
static CallbackPoll *refuse_poll(Heap *heap, void *ctx) {
  (void)ctx;
  CallbackPoll *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return NULL;
  out->next = relational_refuse_callback();
  out->spent = true;
  return out;
}

Callback relational_refuse_callback(void) { return budget_of_poll(refuse_poll, NULL); }

/* budget_error(result) */
bool relational_budget_error(Result result) {
  if (result.kind != RESULT_FAIL || result.error.kind != ERROR_BUDGET_EXHAUSTED) return false;
  return str_eq(result.error.message, str_lit("the check budget is exhausted"));
}

/* budget_refusal(source) */
Task relational_budget_refusal(Heap *heap, Str source) {
  void *parsed = NULL;
  RELATIONAL_TRY(relational_kernel(heap, parser_program(heap, source), &parsed));
  Result run = budget_run_poll(heap, elab_elab_program_comp(heap, global_empty(), parsed), relational_refuse_callback());
  return relational_require(relational_budget_error(run), str_lit("budget: expected immediate exhaustion"));
}

/* finish(tag, result): TAG-OK fields, or TAG-FAIL message and Out.cli_exit(1). */
int relational_finish(Heap *heap, Str tag, Task result, Str fields) {
  if (result.ok) return driver_print(cat3(heap, tag, str_lit("-OK "), fields));
  driver_print(cat3(heap, tag, str_lit("-FAIL "), result.message));
  return 1;
}

/* run(tag, action) */
int relational_run(Heap *heap, Str tag, Task (*action)(Heap *heap, void *ctx, Str *fields), void *ctx) {
  Str fields = str_empty();
  Task result = action(heap, ctx, &fields);
  return relational_finish(heap, tag, result, fields);
}

/* globals(checked) */
Globals relational_globals(const ElabChecked *checked) { return checked->globals; }

/* rows(checked) */
const Vec *relational_rows(const ElabChecked *checked) { return &checked->rows; }

/* empty_symbolic(checked) */
bool relational_empty_symbolic(const ElabChecked *checked) {
  return checked->globals.entries == NULL && checked->globals.families == NULL && checked->rows.len == 0;
}

size_t relational_entry_rows_length(const EntryRows *entries) {
  size_t n = 0;
  for (const EntryRows *row = entries; row != NULL; row = row->rest) n++;
  return n;
}

/* counted_inventory(checked, count, family_names_expected) */
Task relational_counted_inventory(Heap *heap, const ElabChecked *checked, size_t count, const Vec *family_names_expected) {
  Vec family_names = relational_family_names(heap, checked->globals.families);
  Vec axioms = elab_axiom_names(heap, &checked->rows);
  Vec none = vec_new(heap->arena);
  RELATIONAL_TRY(relational_require(checked->rows.len == count && relational_entry_rows_length(checked->globals.entries) == count,
                                    str_lit("entry inventory changed")));
  RELATIONAL_TRY(relational_require(relational_same_names(heap, &family_names, family_names_expected),
                                    str_lit("family inventory changed")));
  RELATIONAL_TRY(relational_require(relational_strings(&axioms, &none), str_lit("unexpected axioms")));
  RELATIONAL_TRY(relational_audit_entries(heap, checked->globals.entries, global_empty()));
  return relational_audit_families(heap, checked->globals.families);
}

/* all_families(globals) */
Task relational_all_families(Heap *heap, Globals globals) { return relational_audit_families(heap, globals.families); }

/* found(entry) */
bool relational_found(MaybeEntry entry) { return entry.some; }

/* required(names, globals) */
Task relational_required(Heap *heap, const Vec *names, Globals globals) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    RELATIONAL_TRY(relational_require(relational_found(global_find(name, globals)), cat(heap, str_lit("missing member: "), name)));
  }
  return task_pure();
}

/* tiny_type() */
const Term *relational_tiny_type(Heap *heap) {
  Vec none = vec_new(heap->arena);
  return term_lan(heap, shape_smu(str_lit("Tiny"), none), rules_diagram_of(heap, &none));
}

/* quotations(names, globals) */
Task relational_quotations(Heap *heap, const Vec *names, Globals globals) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    const Term *term = term_global(heap, name);
    void *expected = NULL;
    void *value = NULL;
    void *quoted = NULL;
    void *quoted_value = NULL;
    Vec env = vec_new(heap->arena);
    Ctx ctx = check_make(heap, globals, budget_unlimited(), nat_zero());
    RELATIONAL_TRY(relational_kernel(heap, check_infer(heap, ctx, QUANTITY_ZERO, term), &expected));
    RELATIONAL_TRY(relational_kernel(heap, eval_eval(heap, globals, env, term), &value));
    RELATIONAL_TRY(relational_kernel(heap, eval_quote(heap, globals, nat_zero(), value), &quoted));
    RELATIONAL_TRY(relational_kernel(heap, check_check_term(heap, globals, budget_unlimited(), quoted, expected), NULL));
    RELATIONAL_TRY(relational_kernel(heap, eval_eval(heap, globals, env, quoted), &quoted_value));
    BoolResult same = conv_conv(heap, check_make(heap, globals, budget_unlimited(), nat_zero()), expected, value, quoted_value);
    if (same.kind == RESULT_FAIL) return task_fail(error_to_string(heap, same.error));
    RELATIONAL_TRY(relational_require(same.value, cat(heap, str_lit("quotation changed "), name)));
  }
  return task_pure();
}
