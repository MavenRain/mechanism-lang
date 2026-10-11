/* C port of bend2/tests/runtime_slice.bend (Bend lines 1-179): the reference traversal over erased
 * terms, the function catalog, the reachability visit, the declaration and row selection and the
 * self test. The Bend module has no main: c-test builds this unit as the runtime_slice check (the
 * main below prints PASS) and prelude_runtime (S5.1) links it with RUNTIME_SLICE_LIBRARY defined,
 * which drops the main. Definitions keep the Bend order and names. */
#include "driver.h"
#include "runtime_slice.h"

SliceResult slice_done(void *value) {
  SliceResult out;
  out.kind = SLICE_DONE;
  out.value = value;
  out.message = str_empty();
  return out;
}

SliceResult slice_fail(Str message) {
  SliceResult out;
  out.kind = SLICE_FAIL;
  out.value = NULL;
  out.message = message;
  return out;
}

bool slice_failed(SliceResult result) {
  switch (result.kind) {
    case SLICE_DONE: return false;
    case SLICE_FAIL: return true;
  }
  return true;
}

Str *slice_name_new(Heap *heap, Str name) {
  Str *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = name;
  return boxed;
}

Vec *slice_vec_new(Heap *heap, Vec vec) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return NULL;
  *boxed = vec;
  return boxed;
}

/* Done{vec} boxed; an allocation failure is reported as Fail (heap->oom is already set). */
static SliceResult done_vec(Heap *heap, Vec vec) {
  Vec *boxed = slice_vec_new(heap, vec);
  if (boxed == NULL) return slice_fail(str_lit("host out of memory"));
  return slice_done(boxed);
}

/* Total indexing: NULL out of range (loops stay within len). */
void *slice_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  switch (got.status) {
    case VEC_GET_OK: return got.value;
    case VEC_GET_OUT_OF_RANGE: return NULL;
  }
  return NULL;
}

Str slice_name_at(const Vec *names, size_t index) {
  const Str *boxed = slice_at(names, index);
  if (boxed == NULL) return str_empty();
  return *boxed;
}

/* A NULL item is an allocation failure upstream: flag it and leave the item out. */
static void push(Heap *heap, Vec *xs, void *item) {
  if (item == NULL) {
    heap->oom = true;
    return;
  }
  if (!vec_push(xs, item)) heap->oom = true;
}

static void push_all(Heap *heap, Vec *out, const Vec *xs, size_t from) {
  size_t i;
  for (i = from; i < xs->len; i++) push(heap, out, slice_at(xs, i));
}

/* Bend `Con{head, rest}`: a fresh list with head first. */
Vec slice_cons(Heap *heap, void *head, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  push(heap, &out, head);
  push_all(heap, &out, rest, 0u);
  return out;
}

/* The rest of a non-empty list. */
Vec slice_tail(Heap *heap, const Vec *list) {
  Vec out = vec_new(heap->arena);
  push_all(heap, &out, list, 1u);
  return out;
}

/* List.append(a, b). */
Vec slice_append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_new(heap->arena);
  push_all(heap, &out, a, 0u);
  push_all(heap, &out, b, 0u);
  return out;
}

/* A literal List<String>. */
Vec slice_names(Heap *heap, const char *const *names, size_t count) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < count; i++) push(heap, &out, slice_name_new(heap, str_lit(names[i])));
  return out;
}

static Vec one(Heap *heap, void *item) {
  Vec empty = vec_new(heap->arena);
  return slice_cons(heap, item, &empty);
}

/* Bend 14-17: member */
bool slice_member(const Vec *names, Str name) {
  size_t i;
  for (i = 0; i < names->len; i++) {
    Str head = slice_name_at(names, i);
    if (str_eq(head, name)) return true;
  }
  return false;
}

/* Bend 19-20: insert */
Vec slice_insert(Heap *heap, const Vec *names, Str name) {
  if (slice_member(names, name)) return *names;
  return slice_cons(heap, slice_name_new(heap, name), names);
}

/* Bend 27-30: reference_list */
static Vec reference_list(Heap *heap, Vec names, const Vec *terms) {
  size_t i;
  for (i = 0; i < terms->len; i++) {
    const Ktm *term = slice_at(terms, i);
    if (term != NULL) names = slice_references(heap, &names, term);
  }
  return names;
}

/* Bend 32-35: reference_branches */
static Vec reference_branches(Heap *heap, Vec names, const Vec *branches) {
  size_t i;
  for (i = 0; i < branches->len; i++) {
    const KtmBranch *branch = slice_at(branches, i);
    if (branch != NULL) names = slice_references(heap, &names, branch->body);
  }
  return names;
}

/* Bend 22-25 and 37-52: references */
Vec slice_references(Heap *heap, const Vec *names, const Ktm *term) {
  switch (term->kind) {
    case KTM_VAR: return *names;
    case KTM_LIT: return *names;
    case KTM_ERASED: return *names;
    case KTM_GLOBAL: return slice_insert(heap, names, term->name);
    case KTM_LET: {
      Vec value = slice_references(heap, names, term->value);
      return slice_references(heap, &value, term->body);
    }
    case KTM_CLOS: {
      Vec with = slice_insert(heap, names, term->fid.name);
      return reference_list(heap, with, &term->captures);
    }
    case KTM_DELAY: {
      Vec with = slice_insert(heap, names, term->fid.name);
      return reference_list(heap, with, &term->captures);
    }
    case KTM_APP: {
      Vec head = slice_references(heap, names, term->head);
      return reference_list(heap, head, &term->args);
    }
    case KTM_TAIL: {
      Vec head = slice_references(heap, names, term->head);
      return reference_list(heap, head, &term->args);
    }
    case KTM_STRUCT: return reference_list(heap, *names, &term->fields);
    case KTM_TAG: return reference_list(heap, *names, &term->fields);
    case KTM_PROJ: return slice_references(heap, names, term->value);
    case KTM_FORCE: return slice_references(heap, names, term->value);
    case KTM_CASE: {
      Vec scrut = slice_references(heap, names, term->scrut);
      return reference_branches(heap, scrut, &term->branches);
    }
  }
  return *names;
}

/* Bend 54-58: lookup */
SliceMaybeNames slice_lookup(const Vec *catalog, Str name) {
  SliceMaybeNames out;
  size_t i;
  out.some = false;
  out.value.arena = NULL;
  out.value.items = NULL;
  out.value.len = 0u;
  out.value.cap = 0u;
  for (i = 0; i < catalog->len; i++) {
    const SliceCatalogRow *row = slice_at(catalog, i);
    if (row != NULL && str_eq(row->name, name)) {
      out.some = true;
      out.value = row->deps;
      return out;
    }
  }
  return out;
}

/* Bend 60-63: add_function */
static SliceResult add_function(Heap *heap, SliceMaybeNames found, const Vec *catalog, Str name, const Ktm *body) {
  if (found.some) {
    Str message = heap_str_cat(heap, str_lit("duplicate erased function: "), name);
    return slice_fail(message);
  }
  SliceCatalogRow *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return slice_fail(str_lit("host out of memory"));
  Vec empty = vec_new(heap->arena);
  row->name = name;
  row->deps = slice_references(heap, &empty, body);
  return done_vec(heap, slice_cons(heap, row, catalog));
}

/* Bend 65-72: catalog_decls */
static SliceResult catalog_decls(Heap *heap, const Vec *decls, Vec catalog) {
  size_t i;
  for (i = 0; i < decls->len; i++) {
    const Kdecl *decl = slice_at(decls, i);
    if (decl == NULL) continue;
    switch (decl->kind) {
      case KDECL_REC: break;
      case KDECL_FUN: {
        SliceMaybeNames found = slice_lookup(&catalog, decl->fid.name);
        SliceResult added = add_function(heap, found, &catalog, decl->fid.name, decl->body);
        if (slice_failed(added)) return added;
        catalog = *(const Vec *)added.value;
        break;
      }
    }
  }
  return done_vec(heap, catalog);
}

/* Bend 74-82: catalog_rows */
SliceResult slice_catalog_rows(Heap *heap, const Vec *rows, const Vec *catalog) {
  Vec acc = *catalog;
  size_t i;
  for (i = 0; i < rows->len; i++) {
    const EraseRow *row = slice_at(rows, i);
    if (row == NULL) continue;
    switch (row->entry.kind) {
      case ERASE_ENTRY_DROPPED: break;
      case ERASE_ENTRY_POSTULATE: break;
      case ERASE_ENTRY_CODE: {
        SliceResult next = catalog_decls(heap, &row->entry.decls, acc);
        if (slice_failed(next)) return next;
        acc = *(const Vec *)next.value;
        break;
      }
    }
  }
  return done_vec(heap, acc);
}

/* Bend 84-87: dependencies */
Vec slice_dependencies(Heap *heap, SliceMaybeNames found) {
  if (found.some) return found.value;
  return vec_new(heap->arena);
}

/* Bend 89-103: visit (law) and visit_one */
static Vec visit(Heap *heap, const Vec *catalog, Vec needed, const Vec *pending);

static Vec visit_one(Heap *heap, bool seen, const Vec *catalog, Vec needed, Str name, const Vec *rest) {
  if (seen) return visit(heap, catalog, needed, rest);
  Vec more_needed = slice_cons(heap, slice_name_new(heap, name), &needed);
  Vec deps = slice_dependencies(heap, slice_lookup(catalog, name));
  Vec pending = slice_append(heap, &deps, rest);
  return visit(heap, catalog, more_needed, &pending);
}

static Vec visit(Heap *heap, const Vec *catalog, Vec needed, const Vec *pending) {
  if (pending->len == 0u) return needed;
  Str name = slice_name_at(pending, 0u);
  Vec rest = slice_tail(heap, pending);
  return visit_one(heap, slice_member(&needed, name), catalog, needed, name, &rest);
}

/* Bend 105-111: select_decls */
static Vec select_decls(Heap *heap, const Vec *decls, const Vec *needed) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < decls->len; i++) {
    Kdecl *decl = slice_at(decls, i);
    if (decl == NULL) continue;
    switch (decl->kind) {
      case KDECL_REC: push(heap, &out, decl); break;
      case KDECL_FUN:
        if (slice_member(needed, decl->fid.name)) push(heap, &out, decl);
        break;
    }
  }
  return out;
}

/* Bend 113-118: select_rows */
static Vec select_rows(Heap *heap, const Vec *rows, const Vec *needed) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < rows->len; i++) {
    EraseRow *row = slice_at(rows, i);
    if (row == NULL) continue;
    switch (row->entry.kind) {
      case ERASE_ENTRY_DROPPED: push(heap, &out, row); break;
      case ERASE_ENTRY_POSTULATE: push(heap, &out, row); break;
      case ERASE_ENTRY_CODE: {
        Vec decls = select_decls(heap, &row->entry.decls, needed);
        push(heap, &out, erase_row_new(heap, row->name, erase_entry_code(decls)));
        break;
      }
    }
  }
  return out;
}

/* Bend 120-123: select */
SliceResult slice_select(Heap *heap, const Vec *rows, Str export) {
  Vec empty = vec_new(heap->arena);
  SliceResult catalog = slice_catalog_rows(heap, rows, &empty);
  if (slice_failed(catalog)) return catalog;
  Vec exports = one(heap, slice_name_new(heap, export));
  Vec needed = visit(heap, catalog.value, vec_new(heap->arena), &exports);
  return done_vec(heap, select_rows(heap, rows, &needed));
}

/* Bend 125-126: require */
SliceResult slice_require(bool condition, Str message) {
  if (condition) return slice_done(kernel_unit());
  return slice_fail(message);
}

/* Bend 128-131: includes */
static bool includes(const Vec *actual, const Vec *expected) {
  size_t i;
  for (i = 0; i < actual->len; i++) {
    Str head = slice_name_at(actual, i);
    if (!slice_member(expected, head)) return false;
  }
  return true;
}

/* Bend 133-134: same_set */
static bool same_set(const Vec *actual, const Vec *expected) {
  return includes(actual, expected) && includes(expected, actual);
}

static const Ktm *global(Heap *heap, const char *name) { return eterm_kglobal(heap, str_lit(name)); }

static Fid fid_of(const char *name) {
  Fid fid;
  fid.name = str_lit(name);
  return fid;
}

/* Bend 136-140: reference_test */
static SliceResult reference_test(Heap *heap) {
  static const char *const expected_names[16] = {"direct", "bound", "body", "closure", "capture", "delay", "delayed-capture", "call", "argument", "tail", "tail-argument", "projection", "payload", "scrutinee", "branch", "forced"};
  Tid tid;
  Nat zero = heap_nat_of_u64(heap, 0u);
  Nat one_n = heap_nat_of_u64(heap, 1u);
  Vec fields = vec_new(heap->arena);
  Vec capture = one(heap, (void *)global(heap, "capture"));
  Vec delayed = one(heap, (void *)global(heap, "delayed-capture"));
  Vec argument = one(heap, (void *)global(heap, "argument"));
  Vec tail_argument = one(heap, (void *)global(heap, "tail-argument"));
  Vec payload = one(heap, (void *)global(heap, "payload"));
  Vec branches = one(heap, eterm_branch_new(heap, zero, one_n, global(heap, "branch")));
  tid.name = str_lit("slice-test");
  push(heap, &fields, (void *)global(heap, "direct"));
  push(heap, &fields, (void *)global(heap, "direct"));
  push(heap, &fields, (void *)eterm_klet(heap, str_lit("x"), global(heap, "bound"), global(heap, "body")));
  push(heap, &fields, (void *)eterm_kclos(heap, fid_of("closure"), one_n, capture));
  push(heap, &fields, (void *)eterm_kdelay(heap, fid_of("delay"), delayed));
  push(heap, &fields, (void *)eterm_kapp(heap, global(heap, "call"), argument));
  push(heap, &fields, (void *)eterm_ktail(heap, global(heap, "tail"), tail_argument));
  push(heap, &fields, (void *)eterm_kproj(heap, tid, zero, global(heap, "projection")));
  push(heap, &fields, (void *)eterm_ktag(heap, tid, zero, payload));
  push(heap, &fields, (void *)eterm_kcase(heap, tid, global(heap, "scrutinee"), branches));
  push(heap, &fields, (void *)eterm_kforce(heap, global(heap, "forced")));
  push(heap, &fields, (void *)eterm_kvar(heap, zero));
  push(heap, &fields, (void *)eterm_kerased());
  push(heap, &fields, (void *)eterm_klit(heap, literal_lint(big_zero())));
  {
    const Ktm *body = eterm_kstruct(heap, tid, fields);
    Vec expected = slice_names(heap, expected_names, 16u);
    Vec empty = vec_new(heap->arena);
    Vec found = slice_references(heap, &empty, body);
    return slice_require(same_set(&found, &expected), str_lit("runtime reference traversal"));
  }
}

/* Bend 142-143: fn */
static Kdecl *fn(Heap *heap, const char *name, const Ktm *body) {
  Repr result = eterm_ri31();
  return eterm_kdecl_new(heap, eterm_kfun(fid_of(name), vec_new(heap->arena), result, body));
}

/* Bend 145-148: duplicate */
static SliceResult duplicate(Heap *heap, SliceResult result) {
  switch (result.kind) {
    case SLICE_DONE: return slice_fail(str_lit("duplicate function accepted"));
    case SLICE_FAIL: {
      Str message = heap_str_cat(heap, str_lit("wrong duplicate refusal: "), result.message);
      return slice_require(str_eq(result.message, str_lit("duplicate erased function: root")), message);
    }
  }
  return slice_fail(str_lit("duplicate function accepted"));
}

/* Bend 150-153: row_names */
Vec slice_row_names(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < rows->len; i++) {
    const EraseRow *row = slice_at(rows, i);
    if (row != NULL) push(heap, &out, slice_name_new(heap, row->name));
  }
  return out;
}

/* Bend 155-164: same_names */
bool slice_same_names(const Vec *left, const Vec *right) {
  size_t i;
  if (left->len != right->len) return false;
  for (i = 0; i < left->len; i++) {
    Str name = slice_name_at(left, i);
    Str other = slice_name_at(right, i);
    if (!str_eq(name, other)) return false;
  }
  return true;
}

static EraseRow *row(Heap *heap, const char *name, EraseEntry entry) { return erase_row_new(heap, str_lit(name), entry); }

static Vec two(Heap *heap, void *a, void *b) {
  Vec rest = one(heap, b);
  return slice_cons(heap, a, &rest);
}

static Vec three(Heap *heap, void *a, void *b, void *c) {
  Vec rest = two(heap, b, c);
  return slice_cons(heap, a, &rest);
}

/* Bend 166-179: self_test */
SliceResult slice_self_test(Heap *heap) {
  Tid *tid = eterm_tid_new(heap, str_lit("slice-test"));
  Nat zero = heap_nat_of_u64(heap, 0u);
  Vec empty = vec_new(heap->arena);
  Kdecl *root = fn(heap, "root", eterm_kapp(heap, global(heap, "middle"), empty));
  Kdecl *middle = fn(heap, "middle", eterm_kclos(heap, fid_of("lifted"), zero, empty));
  Kdecl *lifted = fn(heap, "lifted", global(heap, "root"));
  Kdecl *dead = fn(heap, "dead", global(heap, "unavailable"));
  Kdecl *rec = eterm_kdecl_new(heap, eterm_krec(one(heap, tid)));
  Vec rows = vec_new(heap->arena);
  Vec expected = vec_new(heap->arena);
  push(heap, &rows, row(heap, "entry", erase_entry_code(two(heap, rec, root))));
  push(heap, &rows, row(heap, "helpers", erase_entry_code(three(heap, middle, lifted, dead))));
  push(heap, &rows, row(heap, "type", erase_entry_dropped()));
  push(heap, &rows, row(heap, "postulate", erase_entry_postulate(eterm_ri31())));
  push(heap, &expected, row(heap, "entry", erase_entry_code(two(heap, rec, root))));
  push(heap, &expected, row(heap, "helpers", erase_entry_code(two(heap, middle, lifted))));
  push(heap, &expected, row(heap, "type", erase_entry_dropped()));
  push(heap, &expected, row(heap, "postulate", erase_entry_postulate(eterm_ri31())));
  {
    SliceResult checked = reference_test(heap);
    if (slice_failed(checked)) return checked;
  }
  {
    SliceResult selected = slice_select(heap, &rows, str_lit("root"));
    if (slice_failed(selected)) return selected;
    const Vec *chosen = selected.value;
    Str printed_selected = erase_print(heap, chosen);
    Str printed_expected = erase_print(heap, &expected);
    Vec selected_names = slice_row_names(heap, chosen);
    Vec expected_names = slice_row_names(heap, &expected);
    SliceResult checked = slice_require(str_eq(printed_selected, printed_expected) && slice_same_names(&selected_names, &expected_names), str_lit("runtime function closure or retained metadata"));
    if (slice_failed(checked)) return checked;
  }
  {
    Vec refs = slice_references(heap, &empty, global(heap, "unavailable"));
    SliceResult checked = slice_require(slice_member(&refs, str_lit("unavailable")), str_lit("unknown references must remain visible"));
    if (slice_failed(checked)) return checked;
  }
  {
    Vec dup_rows = one(heap, row(heap, "duplicate", erase_entry_code(two(heap, root, root))));
    return duplicate(heap, slice_select(heap, &dup_rows, str_lit("root")));
  }
}

#ifndef RUNTIME_SLICE_LIBRARY
/* The c-test driver: the Bend module has no main and no oracle; PASS on Done, the message on Fail. */
static int report(SliceResult result) {
  switch (result.kind) {
    case SLICE_DONE: return driver_print(str_lit("PASS runtime slice: reference traversal, function closure, duplicate refusal"));
    case SLICE_FAIL: {
      fputs("FAIL runtime slice: ", stderr);
      if (result.message.len > 0) fwrite(result.message.data, 1, result.message.len, stderr);
      fputc('\n', stderr);
      return 1;
    }
  }
  return 1;
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  SliceResult result = slice_self_test(&heap);
  int code = heap.oom ? driver_host_failure() : report(result);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
#endif
