/* C port of cli/rust_in.bend: `mech rust-in CRATE_DIR OUT_DIR`, read src/lib.rs and the file of
 * each `pub mod` line, import the crate (resolve, infer, lower, kernel check) and write one `.mech`
 * file for each module, `MANIFEST` (the file names in dependency order) and a copy of
 * `mech-carrier.mech` if the crate has one. The command puts each chunk of the carrier back in its
 * file before the kernel check (unit D, D2), so the check reads each carried proof. The exit codes
 * are the codes of rust-out: 64 for a usage error or a file that cannot be read, 65 for a refused
 * crate, 1 for an output directory that exists or a write failure. A refused crate writes no file.
 *
 * The definitions keep the Bend names (prefix rust_in_) and the Bend order. A Bend List<String> is a
 * Vec of Str pointers, a List<Lo.Low> a Vec of LowerLow pointers, a List<Rs.Mod> a Vec of ResolveMod
 * pointers, a List<C.Pair<String, String>> a Vec of IoFile pointers and a List<Kept> a Vec of
 * RustInKept pointers. A Bend list matched from a position (`Con{a, Con{b, rest}}`) is the Vec and
 * the index of the first element of `rest`. Each Bend IO(Unit) is an int: 0 = done (the program goes
 * on), else the exit status after the stderr line (D-197 b). A failed allocation sets heap->oom; the
 * helpers then return placeholders and the command checks heap->oom. T-202: each struct result is
 * bound to a local before it is passed to another call. */
#include "cli.h"
#include "rust.h"
#include "surface.h"

static const Str rust_in_str_placeholder;
static const IoFile rust_in_file_placeholder;
static const LowerLow rust_in_low_placeholder;
static const ResolveBad rust_in_bad_none;

/* Item `i` of `xs`, or NULL out of range. */
static const void *rust_in_at(const Vec *xs, size_t i) {
  VecGet got = vec_get(xs, i);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

/* String `i` of a Vec of Str pointers, or "" out of range (the Bend Nil case). */
static Str rust_in_str_at(const Vec *xs, size_t i) {
  const Str *s = rust_in_at(xs, i);
  return s == NULL ? str_empty() : *s;
}

/* Appends `item` to `acc`; a failed push sets heap->oom. */
static void rust_in_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

/* Appends the items of `xs` from index `start` to `acc` (a Bend List.append into the accumulator). */
static void rust_in_push_from(Heap *heap, Vec *acc, const Vec *xs, size_t start) {
  for (size_t i = start; i < xs->len; i++) rust_in_push(heap, acc, rust_in_at(xs, i));
}

static const Str *rust_in_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) return &rust_in_str_placeholder;
  *box = s;
  return box;
}

/* The concatenation of `n` parts in one buffer. */
static Str rust_in_cat(Heap *heap, const Str *parts, size_t n) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < n; i++) {
    if (!strbuf_push(&buf, parts[i])) heap->oom = true;
  }
  return strbuf_str(&buf);
}

/* C.Pair{name, contents}. */
static const IoFile *rust_in_io_file(Heap *heap, Str name, Str contents) {
  IoFile *f = heap_alloc(heap, sizeof(IoFile));
  if (f == NULL) return &rust_in_file_placeholder;
  f->name = name;
  f->contents = contents;
  return f;
}

/* Result<String, List<A>>: `why` = the refusal text (stderr), `items` = the list. */
typedef struct {
  bool ok;
  Str why;
  Vec items;
} RustInList;

static RustInList rust_in_list_done(Vec items) {
  RustInList r = {true, str_empty(), items};
  return r;
}

static RustInList rust_in_list_fail(Heap *heap, Str why) {
  RustInList r = {false, why, vec_new(heap->arena)};
  return r;
}

/* "REFUSED " ++ Rs.show(b) ++ "\n". */
static Str rust_in_refused_text(Heap *heap, const ResolveBad *b) {
  Str shown = resolve_show(heap, b);
  Str parts[] = {str_lit("REFUSED "), shown, str_lit("\n")};
  return rust_in_cat(heap, parts, 3);
}

static RustInLows rust_in_lows_fail(Heap *heap, ResolveBad b) {
  RustInLows r = {false, b, vec_new(heap->arena)};
  return r;
}

/* The lowered files, or the refusal of resolve or infer. */
static RustInLows rust_in_lower_if(Heap *heap, const ResolveOut *o, const InferTypedResult *r) {
  if (r->ok) {
    RustInLows done = {true, rust_in_bad_none, lower_lower(heap, o)};
    return done;
  }
  return rust_in_lows_fail(heap, r->bad);
}

RustInLows rust_in_lows_of(Heap *heap, const ResolveOutResult *r) {
  if (r->ok) {
    InferTypedResult typed = infer_infer(heap, &r->out);
    return rust_in_lower_if(heap, &r->out, &typed);
  }
  return rust_in_lows_fail(heap, r->bad);
}

static Str rust_in_manifest(Heap *heap, const Vec *ls) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = rust_in_at(ls, i);
    if (l != NULL) {
      if (!strbuf_push(&buf, l->file)) heap->oom = true;
      if (!strbuf_push(&buf, str_lit("\n"))) heap->oom = true;
    }
  }
  return strbuf_str(&buf);
}

static Vec rust_in_low_files(Heap *heap, const Vec *ls) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = rust_in_at(ls, i);
    if (l != NULL) rust_in_push(heap, &out, rust_in_io_file(heap, l->file, l->text));
  }
  return out;
}

static RustInList rust_in_kernel_ok(Heap *heap, Result r, const Vec *ls) {
  switch (r.kind) {
    case RESULT_DONE: {
      Vec files = rust_in_low_files(heap, ls);
      Str manifest = rust_in_manifest(heap, ls);
      rust_in_push(heap, &files, rust_in_io_file(heap, str_lit("MANIFEST"), manifest));
      return rust_in_list_done(files);
    }
    case RESULT_FAIL: {
      Str message = error_message(heap, r.error);
      Str parts[] = {str_lit("MECH-CHECK-FAIL "), message, str_lit("\n")};
      Str why = rust_in_cat(heap, parts, 3);
      return rust_in_list_fail(heap, why);
    }
  }
  return rust_in_list_fail(heap, str_empty());
}

/* The carrier merge (unit D, D2). `rust-out` writes a key line before each carried chunk:
 * `-- at FILE start` or `-- at FILE after NAME`. NAME is the first name of the nearest runtime
 * declaration before the chunk in FILE. */

/* type Key: KBad{} | KAt{file, after}. */
typedef enum { RUST_IN_KBAD, RUST_IN_KAT } RustInKeyKind;
typedef struct {
  RustInKeyKind kind;
  Str file;  /* KAt */
  Str after; /* KAt */
} RustInKey;

/* type Kept: one carried chunk, its file, its anchor ("" for the start of the file) and its lines
 * with no key line (Str pointers). */
typedef struct {
  Str file;
  Str after;
  Vec lines;
} RustInKept;

/* Result<String, Kept>. */
typedef struct {
  bool ok;
  Str why;
  const RustInKept *kept;
} RustInKeptResult;

static RustInKeptResult rust_in_kept_fail(Str why) {
  RustInKeptResult r = {false, why, NULL};
  return r;
}

static RustInKey rust_in_kbad(void) {
  RustInKey k = {RUST_IN_KBAD, str_empty(), str_empty()};
  return k;
}

static RustInKey rust_in_kat(Str file, Str after) {
  RustInKey k = {RUST_IN_KAT, file, after};
  return k;
}

/* key_tail(file, how, rest): rest = ws[i..]. */
static RustInKey rust_in_key_tail(Str file, Str how, const Vec *ws, size_t i) {
  if (i >= ws->len) {
    if (str_eq(how, str_lit("start"))) return rust_in_kat(file, str_empty());
    return rust_in_kbad();
  }
  Str name = rust_in_str_at(ws, i);
  if (i + 1 < ws->len) return rust_in_kbad();
  if (str_eq(how, str_lit("after")) && !str_eq(name, str_empty())) return rust_in_kat(file, name);
  return rust_in_kbad();
}

static RustInKey rust_in_key_of(const Vec *ws) {
  if (ws->len < 4) return rust_in_kbad();
  Str a = rust_in_str_at(ws, 0);
  Str b = rust_in_str_at(ws, 1);
  Str file = rust_in_str_at(ws, 2);
  Str how = rust_in_str_at(ws, 3);
  if (str_eq(a, str_lit("--")) && str_eq(b, str_lit("at"))) return rust_in_key_tail(file, how, ws, 4);
  return rust_in_kbad();
}

/* kept_key(k, line, rest): rest = chunk[1..]. */
static RustInKeptResult rust_in_kept_key(Heap *heap, RustInKey k, Str line, const Vec *chunk) {
  switch (k.kind) {
    case RUST_IN_KBAD: {
      Str parts[] = {str_lit("REFUSED carrier: the first line of a chunk is not a key line: "), line, str_lit("\n")};
      Str why = rust_in_cat(heap, parts, 3);
      return rust_in_kept_fail(why);
    }
    case RUST_IN_KAT: {
      RustInKept *kept = heap_alloc(heap, sizeof(RustInKept));
      if (kept == NULL) return rust_in_kept_fail(str_empty());
      kept->file = k.file;
      kept->after = k.after;
      kept->lines = vec_new(heap->arena);
      rust_in_push_from(heap, &kept->lines, chunk, 1);
      RustInKeptResult done = {true, str_empty(), kept};
      return done;
    }
  }
  return rust_in_kept_fail(str_empty());
}

static RustInKeptResult rust_in_kept_chunk(Heap *heap, const Vec *chunk) {
  if (chunk == NULL || chunk->len == 0) return rust_in_kept_fail(str_lit("REFUSED carrier: a chunk has no line\n"));
  Str line = rust_in_str_at(chunk, 0);
  Vec ws = rust_out_words(heap, line);
  RustInKey k = rust_in_key_of(&ws);
  return rust_in_kept_key(heap, k, line, chunk);
}

/* kept_more(a, b) of the Bend right recursion over the chunks: the first refusal in carrier order
 * wins, else the chunks in order. Here `acc` holds the chunks before `a`; false ends the walk with
 * the refusal of `a` in `acc`. */
static bool rust_in_kept_more(Heap *heap, RustInKeptResult a, RustInList *acc) {
  if (!a.ok) {
    acc->ok = false;
    acc->why = a.why;
    return false;
  }
  rust_in_push(heap, &acc->items, a.kept);
  return true;
}

/* The chunks of the carrier text, in the order of the carrier (Vec pointers of chunks_of). */
static RustInList rust_in_kept_all(Heap *heap, const Vec *chunks) {
  Vec items = vec_new(heap->arena);
  RustInList acc = rust_in_list_done(items);
  for (size_t i = 0; i < chunks->len; i++) {
    RustInKeptResult a = rust_in_kept_chunk(heap, rust_in_at(chunks, i));
    if (!rust_in_kept_more(heap, a, &acc)) return acc;
  }
  return acc;
}

/* first_word(ws[i..]). */
static Str rust_in_first_word(const Vec *ws, size_t i) { return rust_in_str_at(ws, i); }

/* name_after(ws[i..]). */
static Str rust_in_name_after(const Vec *ws, size_t i) {
  if (i >= ws->len) return str_empty();
  Str w = rust_in_str_at(ws, i);
  if (str_eq(w, str_lit("rec"))) return rust_in_first_word(ws, i + 1);
  return w;
}

static Str rust_in_head_name(const Vec *ws) {
  if (ws->len == 0) return str_empty();
  return rust_in_name_after(ws, 1);
}

/* The declared name of a printed chunk: `def NAME`, `def rec NAME`, `mu NAME`. */
static Str rust_in_chunk_name(Heap *heap, const Vec *chunk) {
  if (chunk == NULL || chunk->len == 0) return str_empty();
  Str line = rust_in_str_at(chunk, 0);
  Vec ws = rust_out_words(heap, line);
  return rust_in_head_name(&ws);
}

/* The lines of the kept chunks of `file` with the anchor `after`. */
static Vec rust_in_kept_at(Heap *heap, const Vec *ks, Str file, Str after) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ks->len; i++) {
    const RustInKept *k = rust_in_at(ks, i);
    if (k != NULL && str_eq(k->file, file) && str_eq(k->after, after)) rust_in_push_from(heap, &out, &k->lines, 0);
  }
  return out;
}

static Vec rust_in_named_at(Heap *heap, const Vec *ks, Str file, Str name) {
  if (str_eq(name, str_empty())) return vec_new(heap->arena);
  return rust_in_kept_at(heap, ks, file, name);
}

static Vec rust_in_merged_chunks(Heap *heap, const Vec *chunks, const Vec *ks, Str file) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < chunks->len; i++) {
    const Vec *chunk = rust_in_at(chunks, i);
    if (chunk != NULL) {
      rust_in_push_from(heap, &out, chunk, 0);
      Str name = rust_in_chunk_name(heap, chunk);
      Vec named = rust_in_named_at(heap, ks, file, name);
      rust_in_push_from(heap, &out, &named, 0);
    }
  }
  return out;
}

static Str rust_in_merged_text(Heap *heap, Str file, Str text, const Vec *ks) {
  Vec lines = rust_in_kept_at(heap, ks, file, str_empty());
  Vec chunks = rust_out_chunks_of(heap, text);
  Vec merged = rust_in_merged_chunks(heap, &chunks, ks, file);
  rust_in_push_from(heap, &lines, &merged, 0);
  Str joined = rust_out_join_lines(heap, &lines);
  Str parts[] = {joined, str_lit("\n")};
  return rust_in_cat(heap, parts, 2);
}

static bool rust_in_has_file(const Vec *ks, Str file) {
  for (size_t i = 0; i < ks->len; i++) {
    const RustInKept *k = rust_in_at(ks, i);
    if (k != NULL && str_eq(k->file, file)) return true;
  }
  return false;
}

/* A file with no kept chunk keeps its text with no change. */
static const LowerLow *rust_in_merged_low(Heap *heap, Str file, Str text, const Vec *ks) {
  LowerLow *l = heap_alloc(heap, sizeof(LowerLow));
  if (l == NULL) return &rust_in_low_placeholder;
  l->file = file;
  l->text = rust_in_has_file(ks, file) ? rust_in_merged_text(heap, file, text, ks) : text;
  return l;
}

static Vec rust_in_merged_lows(Heap *heap, const Vec *ls, const Vec *ks) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = rust_in_at(ls, i);
    if (l != NULL) rust_in_push(heap, &out, rust_in_merged_low(heap, l->file, l->text, ks));
  }
  return out;
}

static bool rust_in_names_have(Heap *heap, const Vec *chunks, Str after) {
  for (size_t i = 0; i < chunks->len; i++) {
    Str name = rust_in_chunk_name(heap, rust_in_at(chunks, i));
    if (str_eq(name, after)) return true;
  }
  return false;
}

static bool rust_in_file_place(Heap *heap, Str after, Str text) {
  if (str_eq(after, str_empty())) return true;
  Vec chunks = rust_out_chunks_of(heap, text);
  return rust_in_names_have(heap, &chunks, after);
}

/* A key has a place if its file is a file of the import and its anchor is the start or the name of
 * a declaration of that file (the first Low of the file decides). */
static bool rust_in_anchor_ok(Heap *heap, const Vec *ls, Str file, Str after) {
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = rust_in_at(ls, i);
    if (l != NULL && str_eq(l->file, file)) return rust_in_file_place(heap, after, l->text);
  }
  return false;
}

static Vec rust_in_keys_bad(Heap *heap, const Vec *ks, const Vec *ls) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ks->len; i++) {
    const RustInKept *k = rust_in_at(ks, i);
    if (k != NULL && !rust_in_anchor_ok(heap, ls, k->file, k->after)) {
      Str key = rust_out_key_line(heap, k->file, k->after);
      Str parts[] = {str_lit("REFUSED carrier: the import has no place for the key line: "), key, str_lit("\n")};
      Str why = rust_in_cat(heap, parts, 3);
      rust_in_push(heap, &out, rust_in_str_box(heap, why));
    }
  }
  return out;
}

static RustInList rust_in_merge_ok(Heap *heap, const Vec *bad, const Vec *ls, const Vec *ks) {
  if (bad->len == 0) {
    Vec merged = rust_in_merged_lows(heap, ls, ks);
    return rust_in_list_done(merged);
  }
  Str why = rust_in_str_at(bad, 0);
  return rust_in_list_fail(heap, why);
}

/* merge_kept(r, ls): r = the kept chunks (RustInKept pointers); the result holds LowerLow pointers. */
static RustInList rust_in_merge_kept(Heap *heap, RustInList r, const Vec *ls) {
  if (!r.ok) return rust_in_list_fail(heap, r.why);
  Vec bad = rust_in_keys_bad(heap, &r.items, ls);
  return rust_in_merge_ok(heap, &bad, ls, &r.items);
}

/* checked(r): r = the merged Lows; the result holds IoFile pointers. */
static RustInList rust_in_checked(Heap *heap, RustInList r) {
  if (!r.ok) return rust_in_list_fail(heap, r.why);
  if (heap->oom) return rust_in_list_fail(heap, str_empty());
  Globals globals = global_initial(heap);
  Budget budget = budget_unlimited();
  Str source = lower_source(heap, &r.items);
  Result k = elab_check_text(heap, globals, budget, source);
  return rust_in_kernel_ok(heap, k, &r.items);
}

/* The second carrier line records source order. Rust's module dependency graph cannot order files
 * whose only dependency is in a carried proof. Taking each file once verifies that the order is a
 * complete permutation. */

/* Result<String, C.Pair<Rs.Mod, List<Rs.Mod>>>: the taken module and the others in order. */
typedef struct {
  bool ok;
  Str why;
  const ResolveMod *mod;
  Vec rest; /* ResolveMod pointers */
} RustInTaken;

/* take_mod(ls, file, []): the first module whose file is `file`; rest = reverse(before) ++ rest of
 * the Bend = every other module in order. */
static RustInTaken rust_in_take_mod(Heap *heap, const Vec *ls, Str file) {
  RustInTaken t = {false, str_empty(), NULL, vec_new(heap->arena)};
  size_t at = ls->len;
  for (size_t i = 0; i < ls->len && at == ls->len; i++) {
    const ResolveMod *m = rust_in_at(ls, i);
    if (m != NULL && str_eq(file, m->file)) at = i;
  }
  if (at == ls->len) {
    Str parts[] = {str_lit("REFUSED carrier: unknown or repeated file in source order: "), file, str_lit("\n")};
    t.why = rust_in_cat(heap, parts, 3);
    return t;
  }
  t.ok = true;
  t.mod = rust_in_at(ls, at);
  for (size_t i = 0; i < ls->len; i++) {
    if (i != at) rust_in_push(heap, &t.rest, rust_in_at(ls, i));
  }
  return t;
}

static const ResolveMod *rust_in_taken_mod(const RustInTaken *taken) { return taken->mod; }

static Vec rust_in_taken_rest(const RustInTaken *taken) { return taken->rest; }

/* ordered_mods(files, ls): files = ws[start..]. The modules in the order of `files`; a file that is
 * unknown or repeated, or a module that `files` omits, is refused (the first in order). */
static RustInList rust_in_ordered_mods(Heap *heap, const Vec *ws, size_t start, const Vec *ls) {
  Vec items = vec_new(heap->arena);
  RustInList out = rust_in_list_done(items);
  Vec left = *ls;
  for (size_t i = start; i < ws->len; i++) {
    Str file = rust_in_str_at(ws, i);
    RustInTaken taken = rust_in_take_mod(heap, &left, file);
    if (!taken.ok) return rust_in_list_fail(heap, taken.why);
    rust_in_push(heap, &out.items, rust_in_taken_mod(&taken));
    left = rust_in_taken_rest(&taken);
  }
  if (left.len == 0) return out;
  const ResolveMod *m = rust_in_at(&left, 0);
  Str omitted = m == NULL ? str_empty() : m->file;
  Str parts[] = {str_lit("REFUSED carrier: source order omits file: "), omitted, str_lit("\n")};
  Str why = rust_in_cat(heap, parts, 3);
  return rust_in_list_fail(heap, why);
}

static RustInList rust_in_order_line(Heap *heap, const Vec *ws, const Vec *ls) {
  if (ws->len < 2) return rust_in_list_done(*ls);
  Str a = rust_in_str_at(ws, 0);
  Str b = rust_in_str_at(ws, 1);
  if (str_eq(a, str_lit("--")) && str_eq(b, str_lit("files"))) return rust_in_ordered_mods(heap, ws, 2, ls);
  return rust_in_list_done(*ls);
}

static RustInList rust_in_carrier_order(Heap *heap, const Vec *lines, const Vec *ls) {
  if (lines->len < 2) return rust_in_list_done(*ls);
  Str line = rust_in_str_at(lines, 1);
  Vec ws = rust_out_words(heap, line);
  return rust_in_order_line(heap, &ws, ls);
}

static RustInList rust_in_low_ok(Heap *heap, RustInLows r, Str carrier) {
  if (!r.ok) {
    Str why = rust_in_refused_text(heap, &r.bad);
    return rust_in_list_fail(heap, why);
  }
  Vec chunks = rust_out_chunks_of(heap, carrier);
  RustInList kept = rust_in_kept_all(heap, &chunks);
  RustInList merged = rust_in_merge_kept(heap, kept, &r.lows);
  return rust_in_checked(heap, merged);
}

/* Apply carrier order before lowering: the lowerer places the implicit MechBool family in the first
 * file, where carried anchors may need it. */
static RustInList rust_in_resolved_carrier(Heap *heap, const ResolveOutResult *r, Str carrier) {
  if (!r->ok) {
    Str why = rust_in_refused_text(heap, &r->bad);
    return rust_in_list_fail(heap, why);
  }
  Vec lines = rust_out_lines_of(heap, carrier);
  RustInList ordered = rust_in_carrier_order(heap, &lines, &r->out.mods);
  if (!ordered.ok) return rust_in_list_fail(heap, ordered.why);
  ResolveOutResult re;
  re.ok = true;
  re.bad = rust_in_bad_none;
  re.out.mods = ordered.items;
  re.out.rows = r->out.rows;
  RustInLows lows = rust_in_lows_of(heap, &re);
  return rust_in_low_ok(heap, lows, carrier);
}

/* The pure part: the text of src/lib.rs, of each module file (ResolveSrc pointers) and of the carrier
 * ("" if the crate has none) give the `.mech` files and MANIFEST (IoFile pointers), or the refusal
 * text. */
static RustInList rust_in_crate_merged(Heap *heap, Str lib, const Vec *srcs, Str carrier) {
  ResolveOutResult r = resolve_resolve(heap, lib, srcs);
  return rust_in_resolved_carrier(heap, &r, carrier);
}

/* The command. */

static int rust_in_usage(void) { return cli_die(64, str_lit("usage: mech rust-in CRATE_DIR OUT_DIR")); }

/* read_srcs(names, dir): DIR/src/NAME.rs for each name in order into `srcs` (ResolveSrc pointers);
 * stops at the first file that fails. */
static int rust_in_read_srcs(Heap *heap, const Vec *names, Str dir, Vec *srcs) {
  for (size_t i = 0; i < names->len; i++) {
    Str name = rust_in_str_at(names, i);
    Str parts[] = {dir, str_lit("/src/"), name, str_lit(".rs")};
    Str path = rust_in_cat(heap, parts, 4);
    RustOutText t = rust_out_read_file(heap, path);
    if (!t.ok) return t.code;
    ResolveSrc *src = heap_alloc(heap, sizeof(ResolveSrc));
    if (src == NULL) {
      heap->oom = true;
      return cli_host_failure();
    }
    src->name = name;
    src->text = t.text;
    rust_in_push(heap, srcs, src);
  }
  return 0;
}

static int rust_in_mods_read(Heap *heap, const ResolveNamesResult *r, Str dir, Vec *srcs) {
  if (r->ok) return rust_in_read_srcs(heap, &r->names, dir, srcs);
  Str why = rust_in_refused_text(heap, &r->bad);
  return heap->oom ? cli_host_failure() : cli_die(65, why);
}

/* carrier_file(path): [C.Pair{"mech-carrier.mech", text}] into `out`. */
static int rust_in_carrier_file(Heap *heap, Str path, Vec *out) {
  RustOutText t = rust_out_read_file(heap, path);
  if (!t.ok) return t.code;
  rust_in_push(heap, out, rust_in_io_file(heap, str_lit("mech-carrier.mech"), t.text));
  return 0;
}

/* The carrier is also copied with no change. The merge reads the same text. */
static int rust_in_carrier_of(Heap *heap, bool present, Str path, Vec *out) {
  if (present) return rust_in_carrier_file(heap, path, out);
  return 0;
}

/* A module's CD4 file name may be `mech-carrier.mech`. Refuse a collision before publication so the
 * carrier cannot overwrite a checked module. */
static Str rust_in_carried_text(const Vec *files) {
  const IoFile *f = rust_in_at(files, 0);
  return f == NULL ? str_empty() : f->contents;
}

static bool rust_in_carrier_name(const Vec *files) {
  for (size_t i = 0; i < files->len; i++) {
    const IoFile *f = rust_in_at(files, i);
    if (f != NULL && str_eq(f->name, str_lit("mech-carrier.mech"))) return true;
  }
  return false;
}

static RustInList rust_in_add_carrier(Heap *heap, const Vec *files, const Vec *carrier) {
  if (carrier->len == 0) return rust_in_list_done(*files);
  if (rust_in_carrier_name(files)) {
    return rust_in_list_fail(heap, str_lit("REFUSED output file `mech-carrier.mech` collides with the proof carrier\n"));
  }
  Vec out = vec_new(heap->arena);
  rust_in_push_from(heap, &out, files, 0);
  rust_in_push_from(heap, &out, carrier, 0);
  return rust_in_list_done(out);
}

static RustInList rust_in_with_carrier(Heap *heap, RustInList r, const Vec *carrier) {
  if (r.ok) return rust_in_add_carrier(heap, &r.items, carrier);
  return rust_in_list_fail(heap, r.why);
}

static int rust_in_emit_files(Heap *heap, RustInList r, Str out) {
  if (r.ok) {
    IoResult result = io_publish(heap, out, &r.items);
    return rust_out_wrote(heap, result);
  }
  return cli_die(65, r.why);
}

static int rust_in_import_crate(Heap *heap, Str dir, Str out) {
  Str target = io_normalize_target(out);
  bool present = io_exists(heap, target);
  IoResult absent = rust_out_absent(heap, present, out);
  int code = rust_out_wrote(heap, absent);
  if (code != 0) return code;
  Str lib_parts[] = {dir, str_lit("/src/lib.rs")};
  Str lib_path = rust_in_cat(heap, lib_parts, 2);
  RustOutText lib = rust_out_read_file(heap, lib_path);
  if (!lib.ok) return lib.code;
  RustSrcFileResult parsed = rust_parser_file(heap, lib.text);
  ResolveNamesResult names = resolve_lib_mods(heap, &parsed);
  Vec srcs = vec_new(heap->arena);
  code = rust_in_mods_read(heap, &names, dir, &srcs);
  if (code != 0) return code;
  Str car_parts[] = {dir, str_lit("/mech-carrier.mech")};
  Str car = rust_in_cat(heap, car_parts, 2);
  bool has_car = io_exists(heap, car);
  Vec carrier = vec_new(heap->arena);
  code = rust_in_carrier_of(heap, has_car, car, &carrier);
  if (code != 0) return code;
  Str carried = rust_in_carried_text(&carrier);
  RustInList merged = rust_in_crate_merged(heap, lib.text, &srcs, carried);
  RustInList files = rust_in_with_carrier(heap, merged, &carrier);
  if (heap->oom) return cli_host_failure();
  return rust_in_emit_files(heap, files, out);
}

int rust_in_run(Heap *heap, const Vec *args) {
  if (args->len != 2) return rust_in_usage();
  Str dir = rust_in_str_at(args, 0);
  Str out = rust_in_str_at(args, 1);
  return rust_in_import_crate(heap, dir, out);
}
