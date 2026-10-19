/* C port of tests/rust_import.bend: the Rust import driver. Driver: `rust_import <mode>
 * <source text> ...`. Mode `lift`: parse, lift to the erased IR, print with the emitter (or one
 * `REFUSED <line>:<col>: <construct>` line, or `PARSE-FAIL <message>`). Modes `resolve`, `types`,
 * `lower` and `check`: the text of src/lib.rs, then a module name and the text of its file for each
 * module. Modes `seed-check <program text>` and `rt-mech <program text> <program text>`: mech text,
 * kernel check, and the row comparison of rust/roundtrip.bend. Mode `run <crate dir> <out dir>`
 * calls rust_in_run (the `rust-in` verb, c/cli/rust_in.c) and exits with its code; each other mode
 * writes one text on stdout and exits 0. A failed allocation sets heap->oom; the driver then exits
 * with driver_host_failure. */
#include "driver.h"
#include "rust.h"
#include "surface.h"
#include "cli.h"

/* C only: `item` pushed onto `xs`; a failed push sets heap->oom. */
static void ri_push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

/* C only: element i of a Vec of Str pointers; "" when i is out of range (a Bend list has no NULL). */
static Str ri_str_at(const Vec *xs, size_t i) {
  VecGet got = vec_get(xs, i);
  const Str *s = got.status == VEC_GET_OK ? got.value : NULL;
  if (s == NULL) return str_empty();
  return *s;
}

/* C only: the elements of xs from index `from` on (a Bend list tail). */
static Vec ri_rest(Heap *heap, const Vec *xs, size_t from) {
  Vec rest = vec_new(heap->arena);
  for (size_t i = from; i < xs->len; i++) {
    VecGet got = vec_get(xs, i);
    if (got.status == VEC_GET_OK) ri_push(heap, &rest, got.value);
  }
  return rest;
}

/* C only: the parts joined in order (the Bend ++ chain). */
static Str ri_cat(Heap *heap, const Str *parts, size_t n) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < n; i++) {
    if (!strbuf_push(&buf, parts[i])) heap->oom = true;
  }
  return strbuf_str(&buf);
}

/* C only: "REFUSED " ++ Rs.show(b) ++ "\n", the Fail arm of resolved, typed, inferred,
 * lower_report and check_report. */
static Str ri_refused(Heap *heap, const ResolveBad *b) {
  Str parts[] = {str_lit("REFUSED "), resolve_show(heap, b), str_lit("\n")};
  return ri_cat(heap, parts, 3);
}

/* lifted(r) */
static Str ri_lifted(Heap *heap, const LiftModuleResult *r) {
  if (r->ok) return emit_text(heap, &r->module);
  Str parts[] = {str_lit("REFUSED "), lift_show(heap, &r->refusal), str_lit("\n")};
  return ri_cat(heap, parts, 3);
}

/* lift_report(r) */
static Str ri_lift_report(Heap *heap, const RustSrcFileResult *r) {
  if (r->ok) {
    LiftModuleResult m = lift_lift(heap, &r->file);
    return ri_lifted(heap, &m);
  }
  Str parts[] = {str_lit("PARSE-FAIL "), r->message, str_lit("\n")};
  return ri_cat(heap, parts, 3);
}

/* resolved(r) */
static Str ri_resolved(Heap *heap, const ResolveOutResult *r) {
  if (r->ok) return resolve_table_text(heap, &r->out);
  return ri_refused(heap, &r->bad);
}

/* srcs_of(xs): one ResolveSrc for each pair of a module name and the text of its file; a last
 * element with no pair is dropped (the Bend `case other: []`). */
static Vec ri_srcs_of(Heap *heap, const Vec *xs) {
  Vec srcs = vec_new(heap->arena);
  for (size_t i = 0; i + 1 < xs->len; i = i + 2) {
    ResolveSrc *s = heap_alloc(heap, sizeof *s);
    if (s == NULL) {
      heap->oom = true;
      return srcs;
    }
    s->name = ri_str_at(xs, i);
    s->text = ri_str_at(xs, i + 1);
    ri_push(heap, &srcs, s);
  }
  return srcs;
}

/* typed(r) */
static Str ri_typed(Heap *heap, const InferTypedResult *r) {
  if (r->ok) return infer_types_text(heap, &r->typed);
  return ri_refused(heap, &r->bad);
}

/* inferred(r) */
static Str ri_inferred(Heap *heap, const ResolveOutResult *r) {
  if (r->ok) {
    InferTypedResult t = infer_infer(heap, &r->out);
    return ri_typed(heap, &t);
  }
  return ri_refused(heap, &r->bad);
}

/* lower_report(r) */
static Str ri_lower_report(Heap *heap, const RustInLows *r) {
  if (r->ok) return lower_files_text(heap, &r->lows);
  return ri_refused(heap, &r->bad);
}

/* axiom_count(rows) */
static Str ri_axiom_count(Heap *heap, const Vec *rows) {
  Vec names = elab_axiom_names(heap, rows);
  return str_of_u64(heap->arena, names.len);
}

/* row_marks(rows): C returns the length of the Bend list of marks (the only use). */
static size_t ri_row_marks(const Vec *rows) { return rows->len; }

/* C only: the rows of a Done (Elab.Rows: a Vec of CheckRow pointers); no rows when the value is
 * NULL. */
static const Vec ri_no_rows = {0};
static const Vec *ri_rows_of(const Result *r) {
  const Vec *rows = r->value;
  if (rows == NULL) return &ri_no_rows;
  return rows;
}

/* check_rows(r) */
static Str ri_check_rows(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_DONE: {
      const Vec *rows = ri_rows_of(&r);
      Str parts[] = {str_lit("MECH-CHECK-OK "), str_of_u64(heap->arena, ri_row_marks(rows)),
                     str_lit(" rows axioms="), ri_axiom_count(heap, rows), str_lit("\n")};
      return ri_cat(heap, parts, 5);
    }
    case RESULT_FAIL: {
      Str parts[] = {str_lit("MECH-CHECK-FAIL "), error_message(heap, r.error), str_lit("\n")};
      return ri_cat(heap, parts, 3);
    }
  }
  return str_empty();
}

/* check_report(r) */
static Str ri_check_report(Heap *heap, const RustInLows *r) {
  if (r->ok) {
    Result rows = elab_check_text(heap, global_initial(heap), budget_unlimited(), lower_source(heap, &r->lows));
    return ri_check_rows(heap, rows);
  }
  return ri_refused(heap, &r->bad);
}

/* checked(src) */
static Result ri_checked(Heap *heap, Str src) {
  return elab_check_text(heap, global_initial(heap), budget_unlimited(), src);
}

/* second_text(more): the first element of `more`, or "". */
static Str ri_second_text(const Vec *more) { return ri_str_at(more, 0); }

/* rt_verdict(n, d) */
static Str ri_rt_verdict(Heap *heap, Str n, MaybeStr d) {
  if (d.some) {
    Str parts[] = {str_lit("RT-MECH-FAIL "), d.value, str_lit("\n")};
    return ri_cat(heap, parts, 3);
  }
  Str parts[] = {str_lit("RT-MECH-OK "), n, str_lit("\n")};
  return ri_cat(heap, parts, 3);
}

/* rt_right(x, b) */
static Str ri_rt_right(Heap *heap, const Vec *x, Result b) {
  switch (b.kind) {
    case RESULT_DONE:
      return ri_rt_verdict(heap, str_of_u64(heap->arena, ri_row_marks(x)), roundtrip_first_diff(x, ri_rows_of(&b)));
    case RESULT_FAIL: {
      Str parts[] = {str_lit("MECH-CHECK-FAIL right "), error_message(heap, b.error), str_lit("\n")};
      return ri_cat(heap, parts, 3);
    }
  }
  return str_empty();
}

/* rt_report(a, b) */
static Str ri_rt_report(Heap *heap, Result a, Result b) {
  switch (a.kind) {
    case RESULT_DONE:
      return ri_rt_right(heap, ri_rows_of(&a), b);
    case RESULT_FAIL: {
      Str parts[] = {str_lit("MECH-CHECK-FAIL left "), error_message(heap, a.error), str_lit("\n")};
      return ri_cat(heap, parts, 3);
    }
  }
  return str_empty();
}

/* type Md */
typedef enum {
  RI_MLIFT,
  RI_MRESOLVE,
  RI_MTYPES,
  RI_MLOWER,
  RI_MCHECK,
  RI_MSEED_CHECK,
  RI_MRT_MECH,
  RI_MNONE
} RiMd;

/* mode_of(mode): the first name that equals `mode` (the Bool.pick chain), else MNone. */
static RiMd ri_mode_of(Str mode) {
  static const char *const names[] = {"lift", "resolve", "types", "lower", "check", "seed-check", "rt-mech"};
  static const RiMd modes[] = {RI_MLIFT, RI_MRESOLVE, RI_MTYPES, RI_MLOWER, RI_MCHECK, RI_MSEED_CHECK, RI_MRT_MECH};
  for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
    if (str_eq(mode, str_lit(names[i]))) return modes[i];
  }
  return RI_MNONE;
}

/* run_mode(m, src, more) */
static Str ri_run_mode(Heap *heap, RiMd m, Str src, const Vec *more) {
  switch (m) {
    case RI_MLIFT: {
      RustSrcFileResult f = rust_parser_file(heap, src);
      return ri_lift_report(heap, &f);
    }
    case RI_MRESOLVE: {
      Vec srcs = ri_srcs_of(heap, more);
      ResolveOutResult r = resolve_resolve(heap, src, &srcs);
      return ri_resolved(heap, &r);
    }
    case RI_MTYPES: {
      Vec srcs = ri_srcs_of(heap, more);
      ResolveOutResult r = resolve_resolve(heap, src, &srcs);
      return ri_inferred(heap, &r);
    }
    case RI_MLOWER: {
      Vec srcs = ri_srcs_of(heap, more);
      ResolveOutResult r = resolve_resolve(heap, src, &srcs);
      RustInLows ls = rust_in_lows_of(heap, &r);
      return ri_lower_report(heap, &ls);
    }
    case RI_MCHECK: {
      Vec srcs = ri_srcs_of(heap, more);
      ResolveOutResult r = resolve_resolve(heap, src, &srcs);
      RustInLows ls = rust_in_lows_of(heap, &r);
      return ri_check_report(heap, &ls);
    }
    case RI_MSEED_CHECK:
      return ri_check_rows(heap, ri_checked(heap, src));
    case RI_MRT_MECH: {
      Result a = ri_checked(heap, src);
      Result b = ri_checked(heap, ri_second_text(more));
      return ri_rt_report(heap, a, b);
    }
    case RI_MNONE:
      return str_lit("unknown mode\n");
  }
  return str_empty();
}

/* dispatch(mode, src, more) */
static Str ri_dispatch(Heap *heap, Str mode, Str src, const Vec *more) {
  return ri_run_mode(heap, ri_mode_of(mode), src, more);
}

/* run_args(args) */
static Str ri_run_args(Heap *heap, const Vec *args) {
  if (args->len >= 2) {
    Vec rest = ri_rest(heap, args, 2);
    return ri_dispatch(heap, ri_str_at(args, 0), ri_str_at(args, 1), &rest);
  }
  return str_lit("usage: rust_import.bend lift <source text> | resolve|types|lower|check <lib.rs text> (<module> <text>)* | seed-check <program text> | rt-mech <program text> <program text> | run <crate dir> <out dir>\n");
}

/* C only: IO.write(text) and exit 0; driver_host_failure when an allocation failed. */
static int ri_write(Heap *heap, Str text) {
  if (heap->oom) return driver_host_failure();
  if (text.len > 0) fwrite(text.data, 1, text.len, stdout);
  return 0;
}

/* run_io(args): `run` does IO (rust_in_run); each other mode writes the text of run_args. */
static int ri_run_io(Heap *heap, const Vec *args) {
  if (args->len >= 1 && str_eq(ri_str_at(args, 0), str_lit("run"))) {
    Vec rest = ri_rest(heap, args, 1);
    if (heap->oom) return driver_host_failure();
    return rust_in_run(heap, &rest);
  }
  Str text = ri_run_args(heap, args);
  return ri_write(heap, text);
}

/* C only: IO.args() as a Vec of Str pointers (argv[1..argc)). */
static Vec ri_args(Heap *heap, int argc, char **argv) {
  Vec args = vec_new(heap->arena);
  for (int i = 1; i < argc; i++) {
    Str *s = heap_alloc(heap, sizeof *s);
    if (s == NULL) {
      heap->oom = true;
      return args;
    }
    *s = str_lit(argv[i]);
    ri_push(heap, &args, s);
  }
  return args;
}

/* main: run_io(IO.args()). */
static int ri_entry(int argc, char **argv) {
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Vec args = ri_args(&heap, argc, argv);
  int code = 0;
  if (heap.oom) code = driver_host_failure();
  if (!heap.oom) code = ri_run_io(&heap, &args);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, ri_entry); }
