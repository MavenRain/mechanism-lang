/* Port of bend2/tests/kernel_protocol.bend (SUITE-KERNEL): the fixture groups and raw assertions of Veil
 * test/main.ml over ROOT (default test/veil/test): PARSE round trips, CHECK and ERASE golden forms, NEG and
 * ERASE-NEG exact refusals, the two raw kernel negatives, the recursive values and the migrated erasure
 * fixtures. Each case runs in its own arena; its line is printed before the arena is freed. */
#include "driver.h"
#include "cli.h"
#include "surface.h"
#include "relational_common.h"
#include "relational_syntax_equal.h"
#include "kernel_recursive.h"

/* Mode */
typedef enum {
  MODE_PARSE,
  MODE_CHECKED,
  MODE_ERASED,
  MODE_NEGATIVE,
  MODE_ERASURE_NEGATIVE,
  MODE_KERNEL_NEGATIVE,
  MODE_RECURSIVE_VALUES,
  MODE_MIGRATED
} Mode;

/* Source{name, path} */
typedef struct {
  Str name;
  Str path;
} Source;

/* C.Pair<Nat, Nat> with the host allocation flag. */
typedef struct {
  bool oom;
  size_t passed;
  size_t total;
} Count;

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }
static Str cat3(Heap *heap, Str a, Str b, Str c) { return cat(heap, cat(heap, a, b), c); }

static Str at_str(const Vec *vec, size_t index) {
  VecGet got = vec_get(vec, index);
  if (got.status == VEC_GET_OK && got.value != NULL) return *(const Str *)got.value;
  return str_empty();
}

static void push(Heap *heap, Vec *vec, void *item) {
  if (item == NULL || !vec_push(vec, item)) heap->oom = true;
}

/* sources(directory, names): path = directory "/" name ".kan" */
static Vec sources(Heap *heap, Str directory, const Vec *names) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    Source *source = heap_alloc(heap, sizeof *source);
    if (source == NULL) {
      heap->oom = true;
      return out;
    }
    Str name = at_str(names, i);
    source->name = name;
    source->path = cat3(heap, directory, str_lit("/"), cat(heap, name, str_lit(".kan")));
    push(heap, &out, source);
  }
  return out;
}

/* kan_name + kan_names: the names ending ".kan", the suffix dropped, the order kept. */
static Vec kan_names(Heap *heap, const Vec *names) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    Str name = at_str(names, i);
    if (str_ends_with(name, str_lit(".kan"))) push(heap, &out, relational_str(heap, str_n(name.data, name.len - 4)));
  }
  return out;
}

/* listed(root, path) */
static Task listed(Heap *heap, Str root, Str path, Vec *out) {
  Vec names = vec_new(heap->arena);
  RELATIONAL_TRY(relational_directory(heap, root, path, &names));
  Vec kept = kan_names(heap, &names);
  *out = relational_sorted(heap, &kept);
  return task_pure();
}

/* optional_listed(present, root, path) */
static Task optional_listed(Heap *heap, bool present, Str root, Str path, Vec *out) {
  if (present) return listed(heap, root, path, out);
  *out = vec_new(heap->arena);
  return task_pure();
}

/* String.trim: drop Char.is_space (32 and 9..13) at both ends (as wasm_protocol.c trim). */
static bool is_space(uint8_t c) { return c == ' ' || (c >= 9 && c <= 13); }

static Str trim(Str s) {
  size_t start = 0;
  size_t end = s.len;
  while (start < end && is_space((uint8_t)s.data[start])) start += 1;
  while (end > start && is_space((uint8_t)s.data[end - 1])) end -= 1;
  return str_n((const char *)s.data + start, end - start);
}

/* round_trip(source): message_result (E.message) on both parses. */
static Task round_trip(Heap *heap, Str source) {
  Result first = parser_program(heap, source);
  if (first.kind == RESULT_FAIL) return task_fail(error_message(heap, first.error));
  const Vec *decls = (const Vec *)first.value;
  Result second = parser_program(heap, syntax_print(heap, decls));
  if (second.kind == RESULT_FAIL) return task_fail(error_message(heap, second.error));
  return relational_require(syntax_equal_same_list_2_s_decl(decls, (const Vec *)second.value),
                            str_lit("the printed text parses to a different tree"));
}

/* refusal(result, want, missing): Done -> missing; Fail -> the message must equal trim(want). */
static Task refusal(Heap *heap, ResultKind kind, Error error, Str want, const char *missing) {
  if (kind == RESULT_DONE) return task_fail(str_lit(missing));
  Str got = error_message(heap, error);
  return relational_require(str_eq(got, trim(want)), cat3(heap, str_lit("the message is \""), got, str_lit("\"")));
}

static Vec one(Heap *heap, void *item) {
  Vec out = vec_new(heap->arena);
  push(heap, &out, item);
  return out;
}

/* guarded_self() */
static const Term *guarded_self(Heap *heap) {
  Shape arrow = shape_spi(QUANTITY_MANY, str_lit("n"), (void *)term_global(heap, str_lit("N")));
  const Term *call = term_out(heap, arrow, term_addr_pt(QUANTITY_MANY, term_var(heap, nat_zero())), term_global(heap, str_lit("self")));
  Vec inner_binders = one(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("m")));
  Vec branches = one(heap, term_branch_new(heap, term_addr_ctor(str_lit("succ")), term_leg(inner_binders, call)));
  Motive motive = {{true, str_lit("N")}, vec_new(heap->arena), str_lit("x"), term_global(heap, str_lit("N"))};
  const Term *inner = term_elim(heap, term_elim_data(shape_smu(str_lit("N"), vec_new(heap->arena)), term_var(heap, nat_zero()),
                                                     QUANTITY_MANY, term_motive_some(motive), branches));
  Vec outer_binders = one(heap, term_binder_new(heap, QUANTITY_MANY, str_lit("n")));
  return term_sec(heap, arrow, one(heap, term_leg_new(heap, outer_binders, inner)));
}

/* guarded_position(position) */
static Task guarded_position(Heap *heap, MaybeNat position) {
  if (!position.some) return task_fail(str_lit("the guarded self reference answers no position"));
  return relational_require(nat_is_zero(position.value), cat(heap, str_lit("the guarded position is "), heap_nat_show(heap, position.value)));
}

/* T.Ran{Shape.SMu{"F", [T.Univ{L.zero()}]}, T.Sec{Shape.coll(T.T, 0n), []}} */
static const Term *right_mu_former(Heap *heap) {
  Shape mu = shape_smu(str_lit("F"), one(heap, (void *)term_univ(heap, level_zero())));
  return term_ran(heap, mu, term_sec(heap, shape_coll(heap, nat_zero()), vec_new(heap->arena)));
}

/* kernel_negative(name) */
static Task kernel_negative(Heap *heap, Str name) {
  if (str_eq(name, str_lit("smu"))) {
    Result mu = check_infer_term(heap, global_initial(heap), budget_unlimited(), right_mu_former(heap));
    return refusal(heap, mu.kind, mu.error, str_lit("a right former at a mu shape arrives at M2"),
                   "the right former at a mu shape is admitted at M1");
  }
  if (str_eq(name, str_lit("self"))) {
    MaybeNatResult self = totality_guard(heap, budget_unlimited(), global_initial(heap), str_lit("self"),
                                         term_global(heap, str_lit("Nat")), term_global(heap, str_lit("self")));
    RELATIONAL_TRY(refusal(heap, self.kind, self.error, str_lit("recursive definition self failed the structural termination guard"),
                           "the unguarded self reference is admitted at M1"));
    MaybeNatResult guarded = totality_guard(heap, budget_unlimited(), global_initial(heap), str_lit("self"),
                                            term_global(heap, str_lit("Nat")), guarded_self(heap));
    if (guarded.kind == RESULT_FAIL) return task_fail(error_to_string(heap, guarded.error));
    return guarded_position(heap, guarded.value);
  }
  return task_fail(cat3(heap, str_lit("no kernel negative is named \""), name, str_lit("\"")));
}

/* recursive_values() */
static Task recursive_values(Heap *heap) {
  const ElabChecked *checked = NULL;
  RELATIONAL_TRY(relational_checked(heap, global_initial(heap), kernel_recursive_source(), &checked));
  return relational_kernel(heap, kernel_recursive_validate(heap, checked), NULL);
}

/* Erasure.erased(checked) (bend2/tests/kernel_erase_driver.bend 9-11) */
static Result erasure_erased(Heap *heap, const ElabChecked *checked) {
  return erase_program(heap, checked->globals, &checked->rows);
}

/* Erasure.process(source) (bend2/tests/kernel_erase_driver.bend 13-17): the printed rows as a boxed Str. */
static Result erasure_process(Heap *heap, Str source) {
  Result checked = elab_check_in(heap, global_initial(heap), budget_unlimited(), source);
  if (checked.kind == RESULT_FAIL) return checked;
  Result rows = erasure_erased(heap, (const ElabChecked *)checked.value);
  if (rows.kind == RESULT_FAIL) return rows;
  Result done = {RESULT_DONE, relational_str(heap, erase_print(heap, (const Vec *)rows.value)), {0}};
  return done;
}

/* run_case(mode, root, name, path, fixtures) */
static Task run_case(Heap *heap, Mode mode, Str root, Str name, Str path, const Vec *fixtures) {
  Str source = str_empty();
  Str golden = str_empty();
  void *value = NULL;
  const ElabChecked *checked = NULL;
  switch (mode) {
    case MODE_PARSE:
      RELATIONAL_TRY(relational_read(heap, root, path, &source));
      return round_trip(heap, source);
    case MODE_CHECKED:
      RELATIONAL_TRY(relational_read(heap, root, path, &source));
      RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("golden/"), name, str_lit(".checked")), &golden));
      RELATIONAL_TRY(relational_kernel(heap, elab_check_text(heap, global_initial(heap), budget_unlimited(), source), &value));
      return relational_require(str_eq(elab_checked_form(heap, (const Vec *)value), golden), str_lit("the checked form is not the golden text"));
    case MODE_ERASED:
      RELATIONAL_TRY(relational_read(heap, root, path, &source));
      RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("golden/"), name, str_lit(".erased")), &golden));
      RELATIONAL_TRY(relational_kernel(heap, erasure_process(heap, source), &value));
      return relational_require(value != NULL && str_eq(*(const Str *)value, golden), str_lit("the erased form is not the golden text"));
    case MODE_NEGATIVE: {
      RELATIONAL_TRY(relational_read(heap, root, path, &source));
      RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("neg/"), name, str_lit(".err")), &golden));
      Result result = elab_check_text(heap, global_initial(heap), budget_unlimited(), source);
      return refusal(heap, result.kind, result.error, golden, "the file checks and the negative expects it to fail");
    }
    case MODE_ERASURE_NEGATIVE: {
      RELATIONAL_TRY(relational_read(heap, root, path, &source));
      RELATIONAL_TRY(relational_read(heap, root, cat3(heap, str_lit("erase-neg/"), name, str_lit(".err")), &golden));
      RELATIONAL_TRY(relational_checked(heap, global_initial(heap), source, &checked));
      Result result = erasure_erased(heap, checked);
      return refusal(heap, result.kind, result.error, golden, "the file erases and the negative expects the erasure to fail");
    }
    case MODE_KERNEL_NEGATIVE: return kernel_negative(heap, name);
    case MODE_RECURSIVE_VALUES: return recursive_values(heap);
    case MODE_MIGRATED: return relational_require(relational_contains(fixtures, name), str_lit("the migrated erasure fixture is missing"));
  }
  return task_fail(str_lit("unknown mode"));
}

/* report(quiet, kind, name, result) with passed_case: the case line, printed from the case arena. */
static bool report(Heap *heap, bool quiet, Str kind, Str name, Task result) {
  if (result.ok) {
    if (!quiet) driver_print(cat3(heap, kind, str_lit(" "), cat(heap, name, str_lit(" OK"))));
    return true;
  }
  Str line = cat3(heap, kind, str_lit(" "), cat3(heap, name, str_lit(" FAIL: "), result.message));
  if (!heap->oom) driver_print(line);
  return false;
}

/* One case of group_loop in its own arena: the Source and fixtures stay in the parent arena. */
typedef struct {
  bool oom;
  bool ok;
} CaseRun;

static CaseRun one_case(Mode mode, bool quiet, Str kind, Str root, const Source *source, const Vec *fixtures) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Task result = run_case(&heap, mode, root, source->name, source->path, fixtures);
  bool ok = heap.oom ? false : report(&heap, quiet, kind, source->name, result);
  CaseRun run = {heap.oom, ok};
  arena_free(&arena);
  return run;
}

/* group(mode, quiet, kind, label, root, cases, fixtures) = group_loop(..., 0n, length(cases)) */
static Count group(Mode mode, bool quiet, const char *kind, const char *label, Str root, const Vec *cases, const Vec *fixtures) {
  Count count = {false, 0, cases->len};
  for (size_t i = 0; i < cases->len && !count.oom; i++) {
    VecGet got = vec_get(cases, i);
    if (got.status == VEC_GET_OK && got.value != NULL) {
      CaseRun run = one_case(mode, quiet, str_lit(kind), root, (const Source *)got.value, fixtures);
      count.oom = run.oom;
      if (run.ok) count.passed += 1;
    }
  }
  if (count.oom) return count;
  char line[128];
  snprintf(line, sizeof line, "%s %zu/%zu", label, count.passed, count.total);
  driver_print(str_lit(line));
  return count;
}

/* sound(count) */
static bool sound(Count count) { return count.passed == count.total; }

/* full(count) */
static bool full(Count count) { return count.passed == count.total && count.total != 0; }

/* verdict(ok) */
static int verdict(bool ok) {
  if (ok) {
    driver_print(str_lit("SUITE-KERNEL OK"));
    return 0;
  }
  driver_print(str_lit("SUITE-KERNEL FAIL"));
  return cli_exit(1u);
}

/* fail_out(message) */
static int fail_out(Heap *heap, Str message) {
  Str line = cat(heap, str_lit("SUITE "), message);
  if (heap->oom) return driver_host_failure();
  driver_print(line);
  return verdict(false);
}

static Vec names_of(Heap *heap, const char *const *names, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) push(heap, &out, relational_str(heap, str_lit(names[i])));
  return out;
}

static const char *const KERNEL_NEGATIVES[] = {"smu", "self"};
static const char *const RECURSIVE[] = {"values"};
static const char *const MIGRATED[] = {"mu-erase", "mu-rec-direct", "mu-rec-indexed", "mu-rec-mutual"};

/* run(root, fixtures, negatives, erase_negatives) */
static int run(Heap *heap, Str root, const Vec *fixtures, const Vec *negatives, const Vec *erase_negatives) {
  Vec positive = sources(heap, str_lit("fixtures"), fixtures);
  Vec negative = sources(heap, str_lit("neg"), negatives);
  Vec erase_negative = sources(heap, str_lit("erase-neg"), erase_negatives);
  Vec rest = vec_concat(heap->arena, &negative, &erase_negative);
  Vec parse_cases = vec_concat(heap->arena, &positive, &rest);
  Vec kneg_names = names_of(heap, KERNEL_NEGATIVES, sizeof KERNEL_NEGATIVES / sizeof KERNEL_NEGATIVES[0]);
  Vec kneg = sources(heap, str_empty(), &kneg_names);
  Vec rec_names = names_of(heap, RECURSIVE, sizeof RECURSIVE / sizeof RECURSIVE[0]);
  Vec rec = sources(heap, str_empty(), &rec_names);
  Vec migrated_names = names_of(heap, MIGRATED, sizeof MIGRATED / sizeof MIGRATED[0]);
  Vec migrated = sources(heap, str_empty(), &migrated_names);
  Vec none = vec_new(heap->arena);
  if (heap->oom) return driver_host_failure();
  Count parsed = group(MODE_PARSE, true, "PARSE", "PARSE-OK", root, &parse_cases, &none);
  if (parsed.oom) return driver_host_failure();
  Count checked = group(MODE_CHECKED, false, "CHECK", "CHECK-OK", root, &positive, &none);
  if (checked.oom) return driver_host_failure();
  Count erased = group(MODE_ERASED, false, "ERASE", "ERASE-OK", root, &positive, &none);
  if (erased.oom) return driver_host_failure();
  Count refused = group(MODE_NEGATIVE, false, "NEG", "NEG-OK", root, &negative, &none);
  if (refused.oom) return driver_host_failure();
  Count unerased = group(MODE_ERASURE_NEGATIVE, false, "ERASE-NEG", "ERASE-NEG-OK", root, &erase_negative, &none);
  if (unerased.oom) return driver_host_failure();
  Count closed = group(MODE_KERNEL_NEGATIVE, false, "KNEG", "KNEG-OK", root, &kneg, &none);
  if (closed.oom) return driver_host_failure();
  Count recursive = group(MODE_RECURSIVE_VALUES, false, "REC", "REC-OK", root, &rec, &none);
  if (recursive.oom) return driver_host_failure();
  Count migration = group(MODE_MIGRATED, false, "MIGRATED", "MIGRATED-OK", root, &migrated, fixtures);
  if (migration.oom) return driver_host_failure();
  return verdict(full(parsed) && full(checked) && full(erased) && full(refused) && full(closed) && full(recursive) &&
                 full(migration) && sound(unerased));
}

/* list_optional(root) */
static Task list_optional(Heap *heap, Str root, Vec *out) {
  return optional_listed(heap, io_exists(heap, cat(heap, root, str_lit("/erase-neg"))), root, str_lit("erase-neg"), out);
}

/* list_inputs(root) + listed_run(root, result) */
static int listed_run(Heap *heap, Str root) {
  Vec fixtures = vec_new(heap->arena);
  Vec negatives = vec_new(heap->arena);
  Vec erase_negatives = vec_new(heap->arena);
  Task listing = listed(heap, root, str_lit("fixtures"), &fixtures);
  if (listing.ok) listing = listed(heap, root, str_lit("neg"), &negatives);
  if (listing.ok) listing = list_optional(heap, root, &erase_negatives);
  if (heap->oom) return driver_host_failure();
  if (!listing.ok) return fail_out(heap, listing.message);
  return run(heap, root, &fixtures, &negatives, &erase_negatives);
}

/* root_argument(args) */
static Str root_argument(int argc, char **argv) {
  if (argc > 1) return str_lit(argv[1]);
  return str_lit("test/veil/test");
}

/* main() */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = listed_run(&heap, root_argument(argc, argv));
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
