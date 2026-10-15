/* C only: the underscore rule of c/rust/lower.c local names, which the Bend gates do not reach.
 * bend2/rust/lower.bend:50-59: is_word(s) is `s != ""`, and under_rest drops the leading `_` of
 * a local spelling when the rest is a word, so the emitter's unused binder `_x` lowers to `x`.
 * Driver: `rust_lower_under`, no arguments. Each case lowers two fns `probe` whose parameter or
 * let binder spellings differ, prints both with syntax_print and compares the bytes: an `_`
 * spelling with a non-empty rest must print as the bare rest (one-character and longer rests),
 * and a spelling without the leading `_` must keep its own name. Prints one PASS or FAIL line
 * per case and exits 1 when a case fails. A failed allocation sets heap->oom; the driver then
 * exits with driver_host_failure. */
#include "driver.h"
#include "rust.h"
#include "surface.h"

/* C only: a placeholder for a failed Str cell allocation (heap->oom is set). */
static const Str ru_str_placeholder = {0};

/* C only: `item` pushed onto `xs`; a failed push sets heap->oom. */
static void ru_push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

/* C only: a Str on the heap, for a List<String> cell; a failed allocation sets heap->oom. */
static const Str *ru_str_box(Heap *heap, Str s) {
  Str *p = heap_alloc(heap, sizeof *p);
  if (p == NULL) {
    heap->oom = true;
    return &ru_str_placeholder;
  }
  *p = s;
  return p;
}

/* C only: the one-element list [x]. */
static Vec ru_one(Heap *heap, const void *x) {
  Vec out = vec_new(heap->arena);
  ru_push(heap, &out, x);
  return out;
}

/* C only: Sig{"flip", [], [TBool], TBool}; a failed allocation sets heap->oom. */
static const InferSig *ru_flip_sig(Heap *heap) {
  InferSig *s = heap_alloc(heap, sizeof *s);
  if (s == NULL) {
    heap->oom = true;
    return NULL;
  }
  s->name = str_lit("flip");
  s->gens = vec_new(heap->arena);
  s->params = ru_one(heap, rir_tbool(heap));
  s->ret = rir_tbool(heap);
  return s;
}

/* C only: Syntax.print([Lo.fn_decl(Env{[flip], [], []}, ["flip"], [], "probe", [],
 * [Param{param, TBool}], TBool, body)]). */
static Str ru_lowered(Heap *heap, const char *param, const RirEx *body) {
  Vec sigs = ru_one(heap, ru_flip_sig(heap));
  InferEnv e = {sigs, vec_new(heap->arena), vec_new(heap->arena)};
  Vec globals = ru_one(heap, ru_str_box(heap, str_lit("flip")));
  Vec copies = vec_new(heap->arena);
  Vec gs = vec_new(heap->arena);
  Vec ps = ru_one(heap, rir_param(heap, str_lit(param), rir_tbool(heap)));
  const SDecl *d = lower_fn_decl(heap, &e, &globals, &copies, str_lit("probe"), &gs, &ps, rir_tbool(heap), body);
  if (d == NULL) {
    heap->oom = true;
    return str_lit("");
  }
  Vec decls = ru_one(heap, d);
  return syntax_print(heap, &decls);
}

/* C only: the fn `probe(param: bool) -> bool { flip(param) }`. */
static Str ru_param_fn(Heap *heap, const char *param) {
  const RirEx *v = rir_xvar(heap, str_lit(param));
  const RirEx *call = rir_xcall(heap, str_lit("flip"), vec_new(heap->arena), ru_one(heap, v));
  return ru_lowered(heap, param, call);
}

/* C only: the fn `probe(x: bool) -> bool { let binder = flip(x); binder }`. */
static Str ru_let_fn(Heap *heap, const char *binder) {
  const RirEx *x = rir_xvar(heap, str_lit("x"));
  const RirEx *call = rir_xcall(heap, str_lit("flip"), vec_new(heap->arena), ru_one(heap, x));
  const RirEx *use = rir_xvar(heap, str_lit(binder));
  const RirEx *let = rir_xlet(heap, str_lit(binder), rir_no_ty(), call, use);
  return ru_lowered(heap, "x", let);
}

/* C only: `"PASS " ++ label ++ "\n"` when `pass`, else `"FAIL " ++ label ++ "\n"`. */
static Str ru_line(Heap *heap, bool pass, const char *label) {
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

/* C only: one case; `same` asks for equal bytes, else for different bytes. */
typedef struct {
  const char *label;
  bool let_binder;
  const char *left;
  const char *right;
  bool same;
} RuCase;

static const RuCase ru_cases[5] = {
    {"rust_lower_under param _x lowers as x", false, "_x", "x", true},
    {"rust_lower_under param _xs lowers as xs", false, "_xs", "xs", true},
    {"rust_lower_under let _y lowers as y", true, "_y", "y", true},
    {"rust_lower_under let _ys lowers as ys", true, "_ys", "ys", true},
    {"rust_lower_under param x_ keeps its name", false, "x_", "x", false},
};

/* C only: the bytes of the case's fn for one spelling. */
static Str ru_fn(Heap *heap, bool let_binder, const char *name) {
  if (let_binder) return ru_let_fn(heap, name);
  return ru_param_fn(heap, name);
}

/* C only: true when the case holds. */
static bool ru_holds(Heap *heap, const RuCase *c) {
  Str l = ru_fn(heap, c->let_binder, c->left);
  Str r = ru_fn(heap, c->let_binder, c->right);
  bool eq = str_eq(l, r);
  if (c->same) return eq;
  return !eq;
}

/* main: one PASS or FAIL line per case; exit 1 when a case fails. */
static int ru_entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 16);
  Heap heap = heap_new(&arena);
  StrBuf buf = strbuf_new(heap.arena);
  size_t failed = 0;
  size_t at = 0;
  while (at < 5) {
    const RuCase *c = &ru_cases[at];
    bool ok = ru_holds(&heap, c);
    if (!ok) failed = failed + 1;
    if (!strbuf_push(&buf, ru_line(&heap, ok, c->label))) heap.oom = true;
    at = at + 1;
  }
  Str text = strbuf_str(&buf);
  int code = 0;
  if (failed > 0) code = 1;
  if (heap.oom) code = driver_host_failure();
  if (!heap.oom && text.len > 0) fwrite(text.data, 1, text.len, stdout);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, ru_entry); }
