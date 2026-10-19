/* C port of tests/rust_let.bend: direct RIR regressions of the let lowering (the E1 emitter
 * slice precedes the let lifter). Driver: `rust_let`, no arguments. Each `check` lowers one fn
 * `probe(x: bool) -> t` whose body is `let y = value; y` once without and once with the type
 * annotation `t`, prints both with syntax_print and compares the bytes; `rejected` walks one
 * ill-typed let initializer through infer_walk. The report is built as one Str and written once
 * (the Bend IO.write). A failed allocation sets heap->oom; the driver then exits with
 * driver_host_failure. */
#include "driver.h"
#include "rust.h"
#include "surface.h"

/* C only: a placeholder for a failed Str cell allocation (heap->oom is set). */
static const Str rl_str_placeholder = {0};

/* C only: `item` pushed onto `xs`; a failed push sets heap->oom. */
static void rl_push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

/* C only: a Str on the heap, for a List<String> cell; a failed allocation sets heap->oom. */
static const Str *rl_str_box(Heap *heap, Str s) {
  Str *p = heap_alloc(heap, sizeof *p);
  if (p == NULL) {
    heap->oom = true;
    return &rl_str_placeholder;
  }
  *p = s;
  return p;
}

/* C only: the list `[names...]` of `count` Strings (Str pointers). */
static Vec rl_names(Heap *heap, const char *const *names, size_t count) {
  Vec out = vec_new(heap->arena);
  size_t at = 0;
  while (at < count) {
    rl_push(heap, &out, rl_str_box(heap, str_lit(names[at])));
    at = at + 1;
  }
  return out;
}

/* C only: Sig{name, [], params, ret}; a failed allocation sets heap->oom. */
static const InferSig *rl_sig(Heap *heap, const char *name, Vec params, const RirTy *ret) {
  InferSig *s = heap_alloc(heap, sizeof *s);
  if (s == NULL) {
    heap->oom = true;
    return NULL;
  }
  s->name = str_lit(name);
  s->gens = vec_new(heap->arena);
  s->params = params;
  s->ret = ret;
  return s;
}

/* C only: the one-element list [x]. */
static Vec rl_one(Heap *heap, const void *x) {
  Vec out = vec_new(heap->arena);
  rl_push(heap, &out, x);
  return out;
}

/* env(): Env{[Sig{"flip", [], [TBool], TBool}, Sig{"constant", [], [], TBool},
 * Sig{"convert", [], [TBool], TCon{"MechNat", []}}], [], []} */
static InferEnv rl_env(Heap *heap) {
  Vec sigs = vec_new(heap->arena);
  const InferSig *flip = rl_sig(heap, "flip", rl_one(heap, rir_tbool(heap)), rir_tbool(heap));
  const InferSig *constant = rl_sig(heap, "constant", vec_new(heap->arena), rir_tbool(heap));
  const RirTy *nat = rir_tcon(heap, str_lit("MechNat"), vec_new(heap->arena));
  const InferSig *convert = rl_sig(heap, "convert", rl_one(heap, rir_tbool(heap)), nat);
  rl_push(heap, &sigs, flip);
  rl_push(heap, &sigs, constant);
  rl_push(heap, &sigs, convert);
  InferEnv e = {sigs, vec_new(heap->arena), vec_new(heap->arena)};
  return e;
}

/* lowered(t, body): Syntax.print([Lo.fn_decl(env(), ["flip", "constant", "convert"], [], "probe",
 * [], [Param{"x", TBool}], t, body)]) */
static Str rl_lowered(Heap *heap, const RirTy *t, const RirEx *body) {
  static const char *const globals_text[3] = {"flip", "constant", "convert"};
  InferEnv e = rl_env(heap);
  Vec globals = rl_names(heap, globals_text, 3);
  Vec copies = vec_new(heap->arena);
  Vec gs = vec_new(heap->arena);
  const RirParam *x = rir_param(heap, str_lit("x"), rir_tbool(heap));
  Vec ps = rl_one(heap, x);
  const SDecl *d = lower_fn_decl(heap, &e, &globals, &copies, str_lit("probe"), &gs, &ps, t, body);
  if (d == NULL) {
    heap->oom = true;
    return str_lit("");
  }
  Vec decls = rl_one(heap, d);
  return syntax_print(heap, &decls);
}

/* C only: `"PASS " ++ label ++ "\n"` when `pass`, else `"FAIL " ++ label ++ "\n"`. */
static Str rl_line(Heap *heap, bool pass, const char *label) {
  StrBuf buf = strbuf_new(heap->arena);
  Str head = str_lit("FAIL ");
  if (pass) head = str_lit("PASS ");
  Str parts[3] = {head, str_lit(label), str_lit("\n")};
  size_t at = 0;
  while (at < 3) {
    if (!strbuf_push(&buf, parts[at])) heap->oom = true;
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* check(label, t, value): the lowering of `let y = value; y` without and with the annotation `t`
 * print the same bytes. */
static Str rl_check(Heap *heap, const char *label, const RirTy *t, const RirEx *value) {
  const RirEx *y_bare = rir_xvar(heap, str_lit("y"));
  const RirEx *bare = rir_xlet(heap, str_lit("y"), rir_no_ty(), value, y_bare);
  const RirEx *y_typed = rir_xvar(heap, str_lit("y"));
  const RirEx *typed = rir_xlet(heap, str_lit("y"), rir_some_ty(t), value, y_typed);
  Str left = rl_lowered(heap, t, bare);
  Str right = rl_lowered(heap, t, typed);
  return rl_line(heap, str_eq(left, right), label);
}

/* rejected(r): a Fail passes, a Done fails. */
static Str rl_rejected(Heap *heap, const InferNotesResult *r) {
  return rl_line(heap, !r->ok, "initializer type checking");
}

/* C only: the walk of Done{St{[JChk{[], let y = flip(0); y, TBool}], []}} on env(). */
static InferNotesResult rl_walk_bad_let(Heap *heap) {
  InferEnv e = rl_env(heap);
  const RirEx *zero = rir_xint(heap, str_lit("0"));
  const RirEx *call = rir_xcall(heap, str_lit("flip"), vec_new(heap->arena), rl_one(heap, zero));
  const RirEx *y = rir_xvar(heap, str_lit("y"));
  const RirEx *let = rir_xlet(heap, str_lit("y"), rir_no_ty(), call, y);
  const InferJob *job = infer_jchk(heap, vec_new(heap->arena), let, rir_tbool(heap));
  if (job == NULL) heap->oom = true;
  InferStResult start = {0};
  start.ok = true;
  start.msg = str_lit("");
  start.st.jobs = rl_one(heap, job);
  start.st.notes = vec_new(heap->arena);
  return infer_walk(heap, &e, start);
}

/* report() */
static Str rl_report(Heap *heap) {
  StrBuf buf = strbuf_new(heap->arena);
  const RirEx *x1 = rir_xvar(heap, str_lit("x"));
  Str a = rl_check(heap, "variable initializer", rir_tbool(heap), x1);
  const RirEx *x2 = rir_xvar(heap, str_lit("x"));
  const RirEx *flip = rir_xcall(heap, str_lit("flip"), vec_new(heap->arena), rl_one(heap, x2));
  Str b = rl_check(heap, "call initializer", rir_tbool(heap), flip);
  const RirEx *constant = rir_xcall(heap, str_lit("constant"), vec_new(heap->arena), vec_new(heap->arena));
  Str c = rl_check(heap, "nullary initializer", rir_tbool(heap), constant);
  const RirEx *x3 = rir_xvar(heap, str_lit("x"));
  const RirEx *convert = rir_xcall(heap, str_lit("convert"), vec_new(heap->arena), rl_one(heap, x3));
  const RirTy *nat = rir_tcon(heap, str_lit("MechNat"), vec_new(heap->arena));
  Str d = rl_check(heap, "different result type", nat, convert);
  InferNotesResult walked = rl_walk_bad_let(heap);
  Str e = rl_rejected(heap, &walked);
  Str parts[5] = {a, b, c, d, e};
  size_t at = 0;
  while (at < 5) {
    if (!strbuf_push(&buf, parts[at])) heap->oom = true;
    at = at + 1;
  }
  return strbuf_str(&buf);
}

/* main: IO.write(report()). */
static int rl_entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  Str text = rl_report(&heap);
  int code = 0;
  if (heap.oom) code = driver_host_failure();
  if (!heap.oom && text.len > 0) fwrite(text.data, 1, text.len, stdout);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, rl_entry); }
