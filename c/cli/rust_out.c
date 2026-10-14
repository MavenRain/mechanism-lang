/* C port of bend2/cli/rust_out.bend: `mech rust-out FILE.mech... OUT_DIR`, one Rust module for each
 * input file (file i is checked in the globals of the files before it), `src/lib.rs`, `src/nat.rs`
 * if a module uses the kernel `Nat`, `Cargo.toml` and `mech-carrier.mech`. An output directory that
 * exists is refused.
 *
 * The definitions keep the Bend names (prefix rust_out_) and the Bend order. A Bend List<String> is
 * a Vec of Str pointers; a Bend list of RIR values is a Vec of the RIR pointers. Strings are byte
 * slices: each Bend Char test here reads only an ASCII code (code_is is called with n < 128), and no
 * byte of a multi-byte UTF-8 code point is in the ASCII range, so a walk over the bytes gives the
 * same answer as the Bend walk over the code points. The byte order of two UTF-8 strings is the
 * order of their code point lists, so str_le over the bytes is the Bend str_le. A failed allocation
 * sets heap->oom; the helpers then return placeholders and the caller checks heap->oom. */
#include "cli.h"
#include "rust.h"
#include "surface.h"

static const Str rust_out_str_placeholder = {0};
static const Str rust_out_empty_line = {"", 0};

/* Appends `item` to `acc`; a failed push sets heap->oom. */
static void rust_out_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

/* Appends each item of `xs` to `acc` (a Bend List.append into the accumulator). */
static void rust_out_push_all(Heap *heap, Vec *acc, const Vec *xs) {
  for (size_t i = 0; i < xs->len; i++) rust_out_push(heap, acc, xs->items[i]);
}

static const Str *rust_out_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) return &rust_out_str_placeholder;
  *box = s;
  return box;
}

/* The list of one string. */
static Vec rust_out_one(Heap *heap, Str s) {
  Vec out = vec_new(heap->arena);
  rust_out_push(heap, &out, rust_out_str_box(heap, s));
  return out;
}

/* The concatenation of `n` parts in one buffer. */
static Str rust_out_cat(Heap *heap, const Str *parts, size_t n) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < n; i++) {
    if (!strbuf_push(&buf, parts[i])) heap->oom = true;
  }
  return strbuf_str(&buf);
}

/* Byte `i` of `s`, or 0 out of range (the callers keep i < s.len). */
static unsigned char rust_out_byte(Str s, size_t i) {
  StrAt at = str_at(s, i);
  return at.status == STR_INDEX_OK ? at.value : 0;
}

/* Bytes [start, end) of `s`, or "" out of range. */
static Str rust_out_slice(Str s, size_t start, size_t end) {
  StrSlice sl = str_slice(s, start, end);
  return sl.status == STR_INDEX_OK ? sl.value : str_empty();
}

static Str rust_out_from(Str s, size_t start) { return rust_out_slice(s, start, s.len); }

/* String `i` of a Vec of Str pointers, or "" out of range. */
static Str rust_out_str_at(const Vec *xs, size_t i) {
  VecGet got = vec_get(xs, i);
  const Str *s = got.status == VEC_GET_OK ? got.value : NULL;
  return s == NULL ? str_empty() : *s;
}

/* Module names (D7). */

bool rust_out_code_is(unsigned char c, uint32_t n) {
  uint32_t u = c;
  return u >= n && u <= n;
}

static bool rust_out_is_digit(unsigned char c) { return c >= 48 && c <= 57; }

static bool rust_out_mod_char(unsigned char c) {
  return erase_typed_is_lower(c) || rust_out_is_digit(c) || rust_out_code_is(c, 95) ||
         rust_out_code_is(c, 45);
}

static bool rust_out_mod_chars(Str s) {
  for (size_t i = 0; i < s.len; i++) {
    if (!rust_out_mod_char(rust_out_byte(s, i))) return false;
  }
  return true;
}

/* A stem is a lower-case letter, then lower-case letters, digits, `_` or `-`. */
bool rust_out_mod_ok(Str s) {
  if (s.len == 0) return false;
  return erase_typed_is_lower(rust_out_byte(s, 0)) && rust_out_mod_chars(rust_out_from(s, 1));
}

/* Each `-` becomes `_`. */
static Str rust_out_dash(Heap *heap, Str s) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < s.len; i++) {
    unsigned char c = rust_out_byte(s, i);
    unsigned char d = rust_out_code_is(c, 45) ? (unsigned char)'_' : c;
    if (!strbuf_push_byte(&buf, (char)d)) heap->oom = true;
  }
  return strbuf_str(&buf);
}

Str rust_out_mod_name(Heap *heap, Str stem) {
  return erase_typed_rust_name(heap, rust_out_dash(heap, stem));
}

/* The text after the last `/`. D-198 (b): the one Bend call is base_name(path, SNil{}), so the C form
 * has no `cur` accumulator. */
static Str rust_out_base_name(Str s) {
  size_t start = 0;
  for (size_t i = 0; i < s.len; i++) {
    if (rust_out_code_is(rust_out_byte(s, i), 47)) start = i + 1;
  }
  return rust_out_from(s, start);
}

/* The refusal line of an input file name: none, or one line. */
static Vec rust_out_name_bad(Heap *heap, Str name) {
  Str st = erase_typed_stem(name);
  if (str_eq(st, name)) {
    Str parts[] = {str_lit("refused "), name, str_lit(": the name of an input file must end in .mech")};
    return rust_out_one(heap, rust_out_cat(heap, parts, 3));
  }
  if (rust_out_mod_ok(st)) return vec_new(heap->arena);
  Str parts[] = {str_lit("refused "), name,
                 str_lit(": the file stem must be a lower-case letter, then lower-case letters, digits, "
                         "`_` or `-` (D7)")};
  return rust_out_one(heap, rust_out_cat(heap, parts, 3));
}

/* Module names that the crate layout uses. */
static const Str rust_out_reserved_list[] = {{"bin", 3}, {"lib", 3}, {"main", 4}, {"nat", 3}};
static const Str rust_out_nat_list[] = {{"Nat", 3}, {"nat_small", 9}, {"nat_add", 7}, {"nat_decimal", 11}};
static const Str rust_out_nat_mod_name = {"nat", 3};

/* A Vec of pointers to the `n` static strings `xs`. */
static Vec rust_out_static_list(Heap *heap, const Str *xs, size_t n) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < n; i++) rust_out_push(heap, &out, &xs[i]);
  return out;
}

Vec rust_out_reserved(Heap *heap) {
  return rust_out_static_list(heap, rust_out_reserved_list,
                              sizeof rust_out_reserved_list / sizeof rust_out_reserved_list[0]);
}

Vec rust_out_nat_names(Heap *heap) {
  return rust_out_static_list(heap, rust_out_nat_list, sizeof rust_out_nat_list / sizeof rust_out_nat_list[0]);
}

/* Sorted `use` lines (R7). rustfmt sorts lower-case names by byte. */

static bool rust_out_char_lt(unsigned char a, unsigned char b) { return a >= b ? false : true; }

static bool rust_out_str_le(Str a, Str b) {
  for (size_t i = 0; i < a.len; i++) {
    if (i >= b.len) return false;
    unsigned char x = rust_out_byte(a, i);
    unsigned char y = rust_out_byte(b, i);
    if (rust_out_char_lt(x, y)) return true;
    if (rust_out_char_lt(y, x)) return false;
  }
  return true;
}

/* `x` goes before the first item `y` with str_le(x, y). */
static Vec rust_out_insert(Heap *heap, const Vec *xs, const Str *x) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  while (i < xs->len && !rust_out_str_le(*x, rust_out_str_at(xs, i))) {
    rust_out_push(heap, &out, xs->items[i]);
    i++;
  }
  rust_out_push(heap, &out, x);
  for (; i < xs->len; i++) rust_out_push(heap, &out, xs->items[i]);
  return out;
}

/* Bend insert(sorted(rest), x): the last item is inserted first. */
Vec rust_out_sorted(Heap *heap, const Vec *xs) {
  Vec acc = vec_new(heap->arena);
  for (size_t k = xs->len; k > 0; k--) {
    const Str *x = xs->items[k - 1];
    acc = rust_out_insert(heap, &acc, x == NULL ? &rust_out_str_placeholder : x);
  }
  return acc;
}

/* The names that an item list mentions: types, called functions and families. A local closure with
 * the name of a function of a module before counts as a use of that module.
 *
 * Bend tys_refs/exs_refs walk a work list (`args ++ rest`); the C walkers recurse on one value and
 * push onto `acc` in the same preorder (a node, then its children left to right, then the rest). */

static void rust_out_ty_refs(Heap *heap, Vec *acc, const RirTy *t);

static void rust_out_tys_refs(Heap *heap, Vec *acc, const Vec *ts) {
  for (size_t i = 0; i < ts->len; i++) rust_out_ty_refs(heap, acc, ts->items[i]);
}

static void rust_out_ty_refs(Heap *heap, Vec *acc, const RirTy *t) {
  if (t == NULL) return;
  switch (t->kind) {
  case RIR_TBOOL:
    return;
  case RIR_TCON:
    rust_out_push(heap, acc, &t->name);
    rust_out_tys_refs(heap, acc, &t->args);
    return;
  case RIR_TBOX:
    rust_out_ty_refs(heap, acc, t->inner);
    return;
  case RIR_TREF:
    rust_out_ty_refs(heap, acc, t->inner);
    return;
  case RIR_TFN:
    rust_out_tys_refs(heap, acc, &t->args);
    rust_out_ty_refs(heap, acc, t->ret);
    return;
  }
}

/* The family of each arm (RirArm pointers). */
static void rust_out_arm_fams(Heap *heap, Vec *acc, const Vec *arms) {
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *arm = arms->items[i];
    if (arm != NULL) rust_out_push(heap, acc, &arm->pat.family);
  }
}

static void rust_out_ex_refs(Heap *heap, Vec *acc, const RirEx *e);

static void rust_out_exs_refs(Heap *heap, Vec *acc, const Vec *es) {
  for (size_t i = 0; i < es->len; i++) rust_out_ex_refs(heap, acc, es->items[i]);
}

static void rust_out_ex_refs(Heap *heap, Vec *acc, const RirEx *e) {
  if (e == NULL) return;
  switch (e->kind) {
  case RIR_XVAR:
    return;
  case RIR_XINT:
    return;
  case RIR_XBOOL:
    return;
  case RIR_XCALL:
    rust_out_push(heap, acc, &e->func);
    rust_out_tys_refs(heap, acc, &e->targs);
    rust_out_exs_refs(heap, acc, &e->args);
    return;
  case RIR_XCTOR:
    rust_out_push(heap, acc, &e->family);
    rust_out_exs_refs(heap, acc, &e->args);
    return;
  case RIR_XMATCH: {
    rust_out_arm_fams(heap, acc, &e->arms);
    rust_out_ex_refs(heap, acc, e->scrut);
    Vec bodies = emit_arm_bodies(heap, &e->arms);
    rust_out_exs_refs(heap, acc, &bodies);
    return;
  }
  case RIR_XIF:
    rust_out_ex_refs(heap, acc, e->cond);
    rust_out_ex_refs(heap, acc, e->yes);
    rust_out_ex_refs(heap, acc, e->no);
    return;
  case RIR_XLET: {
    Vec tys = erase_typed_let_tys(heap, e->ty.some ? e->ty.value : NULL);
    rust_out_tys_refs(heap, acc, &tys);
    rust_out_ex_refs(heap, acc, e->value);
    rust_out_ex_refs(heap, acc, e->body);
    return;
  }
  case RIR_XCLOSURE: {
    Vec tys = erase_typed_param_tys(heap, &e->params);
    rust_out_tys_refs(heap, acc, &tys);
    rust_out_ex_refs(heap, acc, e->body);
    return;
  }
  case RIR_XBORROW:
    rust_out_ex_refs(heap, acc, e->inner);
    return;
  case RIR_XCLONE:
    rust_out_ex_refs(heap, acc, e->inner);
    return;
  case RIR_XBOX:
    rust_out_ex_refs(heap, acc, e->inner);
    return;
  }
}

/* The fields of each variant (RirVariant pointers), as RirTy pointers in order. */
Vec rust_out_variant_tys(Heap *heap, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = vs->items[i];
    if (v != NULL) rust_out_push_all(heap, &out, &v->fields);
  }
  return out;
}

static Vec rust_out_item_refs(Heap *heap, const RirItem *item) {
  Vec acc = vec_new(heap->arena);
  if (item == NULL) return acc;
  switch (item->kind) {
  case RIR_IUSE:
    return acc;
  case RIR_IENUM: {
    Vec tys = rust_out_variant_tys(heap, &item->variants);
    rust_out_tys_refs(heap, &acc, &tys);
    return acc;
  }
  case RIR_IUNIT:
    return acc;
  case RIR_IFN: {
    Vec tys = vec_new(heap->arena);
    rust_out_push(heap, &tys, item->ret);
    Vec params = erase_typed_param_tys(heap, &item->params);
    rust_out_push_all(heap, &tys, &params);
    rust_out_tys_refs(heap, &acc, &tys);
    rust_out_ex_refs(heap, &acc, item->body);
    return acc;
  }
  }
  return acc;
}

static Vec rust_out_items_refs(Heap *heap, const Vec *items) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < items->len; i++) {
    Vec refs = rust_out_item_refs(heap, items->items[i]);
    rust_out_push_all(heap, &out, &refs);
  }
  return out;
}

/* The Rust item names of a module, for the collision check (D7). */
static Vec rust_out_item_names(Heap *heap, const RirItem *item) {
  Vec out = vec_new(heap->arena);
  if (item == NULL) return out;
  switch (item->kind) {
  case RIR_IUSE:
    return out;
  case RIR_IENUM:
    rust_out_push(heap, &out, &item->name);
    return out;
  case RIR_IUNIT:
    rust_out_push(heap, &out, &item->name);
    rust_out_push(heap, &out, &item->ctor);
    return out;
  case RIR_IFN:
    rust_out_push(heap, &out, &item->name);
    return out;
  }
  return out;
}

static Vec rust_out_items_names(Heap *heap, const Vec *items) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < items->len; i++) {
    Vec names = rust_out_item_names(heap, items->items[i]);
    rust_out_push_all(heap, &out, &names);
  }
  return out;
}

/* Some name of `names` is in `refs`. */
static bool rust_out_hits(const Vec *names, const Vec *refs) {
  for (size_t i = 0; i < names->len; i++) {
    if (erase_typed_has(refs, rust_out_str_at(names, i))) return true;
  }
  return false;
}

/* Mod{name, text, names, nat}: `text` = the Rust text of the module, `names` = its item names (Str
 * pointers), `nat` = the module uses the kernel `Nat`. */
typedef struct {
  Str name;
  Str text;
  Vec names;
  bool nat;
} RoMod;

static const RoMod rust_out_mod_placeholder = {{0}, {0}, {0}, false};

/* The names of the modules (RoMod pointers) that `refs` uses, in the order of `mods`. */
static Vec rust_out_used_mods(Heap *heap, const Vec *mods, const Vec *refs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < mods->len; i++) {
    const RoMod *m = mods->items[i];
    if (m != NULL && rust_out_hits(&m->names, refs)) rust_out_push(heap, &out, &m->name);
  }
  return out;
}

/* One IUse item for each name, in order (RirItem pointers). */
static Vec rust_out_use_items(Heap *heap, const Vec *names) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) rust_out_push(heap, &out, rir_iuse(heap, rust_out_str_at(names, i)));
  return out;
}

static Vec rust_out_nat_mod(Heap *heap, bool nat) {
  if (nat) return rust_out_static_list(heap, &rust_out_nat_mod_name, 1);
  return vec_new(heap->arena);
}

/* The module of one file. `mods`: the modules of the files before it (RoMod pointers); `items`:
 * the RirItem pointers of the file in source order. The result is boxed (a placeholder on OOM). */
static const RoMod *rust_out_new_mod(Heap *heap, const Vec *mods, Str name, const Vec *items) {
  Vec refs = rust_out_items_refs(heap, items);
  Vec nat_names = rust_out_nat_names(heap);
  bool nat = rust_out_hits(&nat_names, &refs);
  Vec mod_list = rust_out_nat_mod(heap, nat);
  Vec used = rust_out_used_mods(heap, mods, &refs);
  rust_out_push_all(heap, &mod_list, &used);
  Vec sorted = rust_out_sorted(heap, &mod_list);
  Vec all = rust_out_use_items(heap, &sorted);
  rust_out_push_all(heap, &all, items);
  RirModule module = rir_module(name, all);
  RoMod *m = heap_alloc(heap, sizeof(RoMod));
  if (m == NULL) return &rust_out_mod_placeholder;
  m->name = rust_out_mod_name(heap, erase_typed_stem(name));
  m->text = emit_text(heap, &module);
  m->names = rust_out_items_names(heap, items);
  m->nat = nat;
  return m;
}

/* The carrier (D2). A chunk starts at a line with a lower-case letter in column 1. The comment lines
 * directly before that line go with the chunk. */

/* The parts of `s` between the bytes with code `code`, the part after the last one included, as Str
 * pointers (slices of `s`) in order. D-198 (a): Bend lines_go/words_go keep `cur` and `done` in
 * reverse order for a tail call; the C loop keeps the start of the current part and pushes in order,
 * which gives the same list. */
static Vec rust_out_parts(Heap *heap, Str s, uint32_t code) {
  Vec done = vec_new(heap->arena);
  size_t start = 0;
  for (size_t i = 0; i < s.len; i++) {
    if (rust_out_code_is(rust_out_byte(s, i), code)) {
      rust_out_push(heap, &done, rust_out_str_box(heap, rust_out_slice(s, start, i)));
      start = i + 1;
    }
  }
  rust_out_push(heap, &done, rust_out_str_box(heap, rust_out_from(s, start)));
  return done;
}

/* The lines of a text: "" gives [""], "a\n" gives ["a", ""]. */
Vec rust_out_lines_of(Heap *heap, Str s) { return rust_out_parts(heap, s, 10); }

/* The words of a line: the parts between single spaces. */
Vec rust_out_words(Heap *heap, Str s) { return rust_out_parts(heap, s, 32); }

Str rust_out_join_lines(Heap *heap, const Vec *lines) {
  return erase_typed_join(heap, str_lit("\n"), lines);
}

typedef enum { RO_LHEAD, RO_LBLANK, RO_LNOTE, RO_LBODY } RoLk;

static bool rust_out_dash_first(Str s) { return s.len > 0 && rust_out_code_is(rust_out_byte(s, 0), 45); }

static RoLk rust_out_line_kind(Str line) {
  if (line.len == 0) return RO_LBLANK;
  unsigned char c = rust_out_byte(line, 0);
  if (rust_out_code_is(c, 45) && rust_out_dash_first(rust_out_from(line, 1))) return RO_LNOTE;
  if (erase_typed_is_lower(c)) return RO_LHEAD;
  return RO_LBODY;
}

/* Ch{done, cur, pend, lead}: a chunk is a Vec of line Str pointers; `done` holds Vec pointers.
 * `pend`: the blank and comment lines after the last line of `cur`. `lead`: the comment lines after
 * the last blank line. D-198 (c): each Bend list here is in reverse order; the C Vecs are in source
 * order, so each Bend prepend is a push, and Bend chunk_lists (which reverses each list back) has no
 * C counterpart: ch_done returns `done` as it is. A step replaces a list that it ends with a new
 * Vec, so no two live Vecs share an item array. */
typedef struct {
  Vec done;
  Vec cur;
  Vec pend;
  Vec lead;
} RoCh;

/* `cur`, if it holds a line, becomes the last chunk of `done`. */
static void rust_out_closed(Heap *heap, const Vec *cur, Vec *done) {
  if (cur->len == 0) return;
  Vec *box = heap_alloc(heap, sizeof(Vec));
  if (box == NULL) return;
  *box = *cur;
  rust_out_push(heap, done, box);
}

static void rust_out_ch_kind(Heap *heap, RoLk k, RoCh *ch, const Str *line) {
  switch (k) {
  case RO_LHEAD: {
    rust_out_closed(heap, &ch->cur, &ch->done);
    Vec cur = vec_new(heap->arena);
    rust_out_push_all(heap, &cur, &ch->lead);
    rust_out_push(heap, &cur, line);
    ch->cur = cur;
    ch->pend = vec_new(heap->arena);
    ch->lead = vec_new(heap->arena);
    return;
  }
  case RO_LBLANK:
    rust_out_push(heap, &ch->pend, &rust_out_empty_line);
    ch->lead = vec_new(heap->arena);
    return;
  case RO_LNOTE:
    rust_out_push(heap, &ch->pend, line);
    rust_out_push(heap, &ch->lead, line);
    return;
  case RO_LBODY:
    rust_out_push_all(heap, &ch->cur, &ch->pend);
    rust_out_push(heap, &ch->cur, line);
    ch->pend = vec_new(heap->arena);
    ch->lead = vec_new(heap->arena);
    return;
  }
}

static void rust_out_ch_step(Heap *heap, RoCh *ch, const Str *line) {
  rust_out_ch_kind(heap, rust_out_line_kind(*line), ch, line);
}

static void rust_out_ch_steps(Heap *heap, const Vec *lines, RoCh *ch) {
  for (size_t i = 0; i < lines->len; i++) {
    const Str *line = lines->items[i];
    rust_out_ch_step(heap, ch, line == NULL ? &rust_out_str_placeholder : line);
  }
}

static Vec rust_out_ch_done(Heap *heap, RoCh *ch) {
  rust_out_closed(heap, &ch->cur, &ch->done);
  return ch->done;
}

/* The chunks of a text: Vec pointers, each a Vec of line Str pointers, all in source order. */
Vec rust_out_chunks_of(Heap *heap, Str text) {
  RoCh ch = {vec_new(heap->arena), vec_new(heap->arena), vec_new(heap->arena), vec_new(heap->arena)};
  Vec lines = rust_out_lines_of(heap, text);
  rust_out_ch_steps(heap, &lines, &ch);
  return rust_out_ch_done(heap, &ch);
}

/* The side of a declaration: runtime (Rust), non-runtime (carrier), or both. */
typedef enum { RO_VNONE, RO_VRUN, RO_VPROOF, RO_VMIXED } RoVd;

static RoVd rust_out_class_vd(const EtClass *c) {
  switch (c->kind) {
  case ET_CFN:
    return RO_VRUN;
  case ET_CDATA:
    return RO_VRUN;
  case ET_CBOOL:
    return RO_VRUN;
  case ET_CPROP:
    return RO_VPROOF;
  case ET_CTYPE:
    return RO_VPROOF;
  case ET_CABSURD:
    return RO_VPROOF;
  case ET_CNO:
    return RO_VRUN;
  case ET_CSKIP:
    return RO_VNONE;
  }
  return RO_VNONE;
}

static RoVd rust_out_with_run(RoVd b) {
  switch (b) {
  case RO_VNONE:
    return RO_VRUN;
  case RO_VRUN:
    return RO_VRUN;
  case RO_VPROOF:
    return RO_VMIXED;
  case RO_VMIXED:
    return RO_VMIXED;
  }
  return RO_VMIXED;
}

static RoVd rust_out_with_proof(RoVd b) {
  switch (b) {
  case RO_VNONE:
    return RO_VPROOF;
  case RO_VRUN:
    return RO_VMIXED;
  case RO_VPROOF:
    return RO_VPROOF;
  case RO_VMIXED:
    return RO_VMIXED;
  }
  return RO_VMIXED;
}

static RoVd rust_out_both(RoVd a, RoVd b) {
  switch (a) {
  case RO_VNONE:
    return b;
  case RO_VRUN:
    return rust_out_with_run(b);
  case RO_VPROOF:
    return rust_out_with_proof(b);
  case RO_VMIXED:
    return RO_VMIXED;
  }
  return RO_VMIXED;
}

/* Bend both(class_vd(name_class(env, name)), names_vd(rest, env)): a right fold, so the C loop runs
 * from the last name to the first. */
static RoVd rust_out_names_vd(Heap *heap, const Vec *names, const EtEnv *env) {
  RoVd acc = RO_VNONE;
  for (size_t k = names->len; k > 0; k--) {
    EtClass c = erase_typed_name_class(heap, env, rust_out_str_at(names, k - 1));
    acc = rust_out_both(rust_out_class_vd(&c), acc);
  }
  return acc;
}

/* `p` is a prefix of `s` (Bend compares one Char at a time; equal bytes = equal Chars). */
static bool rust_out_prefix(Str p, Str s) {
  if (p.len > s.len) return false;
  for (size_t i = 0; i < p.len; i++) {
    if (rust_out_byte(p, i) != rust_out_byte(s, i)) return false;
  }
  return true;
}

/* `p` occurs in `s`; "" occurs in each string. A non-empty valid UTF-8 `p` starts with a byte that is
 * not a continuation byte, so a byte match starts at a Char boundary, as in Bend. */
static bool rust_out_contains(Str s, Str p) {
  for (size_t i = 0; i < s.len; i++) {
    if (rust_out_prefix(p, rust_out_from(s, i))) return true;
  }
  return p.len == 0;
}

/* ---- part b (Bend line 427 on) ---- */

/* Car{text, bad}: the carried chunks of one file and its refusal lines (Str pointers), both in
 * SOURCE order. The Bend carry prepends the Car parts of chunk i onto the Car of the chunks after
 * it, so each part of an earlier chunk comes first: the forward walk of carry appends. */
typedef struct {
  Vec text;
  Vec bad;
} RoCar;

static RoCar rust_out_car(Heap *heap) {
  RoCar car = {vec_new(heap->arena), vec_new(heap->arena)};
  return car;
}

/* car_text: the chunk (its key line first) and then an empty line join the text. */
static void rust_out_car_text(Heap *heap, RoCar *car, const Vec *chunk) {
  rust_out_push_all(heap, &car->text, chunk);
  rust_out_push(heap, &car->text, &rust_out_empty_line);
}

static void rust_out_car_bad(Heap *heap, RoCar *car, Str why) {
  rust_out_push(heap, &car->bad, rust_out_str_box(heap, why));
}

/* The key line of a carried chunk (unit D, D2): the file of the chunk and `prev`, the first name of
 * the nearest runtime declaration before the chunk in that file ("" if there is none). */
Str rust_out_key_line(Heap *heap, Str file, Str prev) {
  if (prev.len == 0) {
    Str parts[] = {str_lit("-- at "), file, str_lit(" start")};
    return rust_out_cat(heap, parts, 3);
  }
  Str parts[] = {str_lit("-- at "), file, str_lit(" after "), prev};
  return rust_out_cat(heap, parts, 4);
}

static bool rust_out_is_run(RoVd v) {
  switch (v) {
    case RO_VNONE:
      return false;
    case RO_VRUN:
      return true;
    case RO_VPROOF:
      return false;
    case RO_VMIXED:
      return false;
  }
  return false;
}

static void rust_out_add_vd(Heap *heap, const Vec *chunk, Str name, RoVd v, RoCar *car, Str file, Str prev) {
  switch (v) {
    case RO_VNONE:
      return;
    case RO_VRUN:
      return;
    case RO_VPROOF: {
      Vec keyed = rust_out_one(heap, rust_out_key_line(heap, file, prev));
      rust_out_push_all(heap, &keyed, chunk);
      rust_out_car_text(heap, car, &keyed);
      return;
    }
    case RO_VMIXED: {
      Str parts[] = {str_lit("refused "), file, str_lit(": carrier: the declaration of "), name,
                     str_lit(" has runtime names and non-runtime names (D2)")};
      rust_out_car_bad(heap, car, rust_out_cat(heap, parts, 5));
      return;
    }
  }
}

static bool rust_out_any_has(const Vec *lines, Str p) {
  for (size_t i = 0; i < lines->len; i++) {
    if (rust_out_contains(rust_out_str_at(lines, i), p)) return true;
  }
  return false;
}

static void rust_out_add_chunk(Heap *heap, const Vec *chunk, Str name, RoVd v, RoCar *car, Str file, Str prev) {
  Str key[] = {str_lit(" "), name};
  if (rust_out_any_has(chunk, rust_out_cat(heap, key, 2))) {
    rust_out_add_vd(heap, chunk, name, v, car, file, prev);
    return;
  }
  Str parts[] = {str_lit("refused "), file, str_lit(": carrier: a top-level chunk does not have the name "), name,
                 str_lit(" (D2)")};
  rust_out_car_bad(heap, car, rust_out_cat(heap, parts, 5));
}

/* carry: chunk i (a Vec of Str pointers) is declaration i (SDecl pointers). One forward walk over
 * the pairs (see RoCar for the order); a count mismatch then adds one refusal line, the Bend base
 * case. `prev` is the first name of the nearest runtime declaration before the pair. */
static RoCar rust_out_carry(Heap *heap, const Vec *chunks, const Vec *decls, const EtEnv *env, Str file) {
  RoCar car = rust_out_car(heap);
  Str prev = str_empty();
  size_t n = chunks->len < decls->len ? chunks->len : decls->len;
  for (size_t i = 0; i < n; i++) {
    const Vec *chunk = chunks->items[i];
    const SDecl *decl = decls->items[i];
    if (chunk == NULL || decl == NULL) {
      heap->oom = true;
      return car;
    }
    Vec names = elab_declared_names(heap, decl);
    Str first = erase_typed_first(&names);
    RoVd v = rust_out_names_vd(heap, &names, env);
    rust_out_add_chunk(heap, chunk, first, v, &car, file, prev);
    if (rust_out_is_run(v)) prev = first;
  }
  if (chunks->len < decls->len) {
    Str parts[] = {str_lit("refused "), file, str_lit(": carrier: more declarations than top-level chunks (D2)")};
    rust_out_car_bad(heap, &car, rust_out_cat(heap, parts, 3));
  }
  if (decls->len < chunks->len) {
    Str parts[] = {str_lit("refused "), file, str_lit(": carrier: more top-level chunks than declarations (D2)")};
    rust_out_car_bad(heap, &car, rust_out_cat(heap, parts, 3));
  }
  return car;
}

static RoCar rust_out_car_parsed(Heap *heap, Result r, const Vec *chunks, const EtEnv *env, Str file) {
  switch (r.kind) {
    case RESULT_FAIL: {
      RoCar car = rust_out_car(heap);
      Str parts[] = {str_lit("refused "), file, str_lit(": carrier: "), error_to_string(heap, r.error)};
      rust_out_car_bad(heap, &car, rust_out_cat(heap, parts, 4));
      return car;
    }
    case RESULT_DONE: {
      const Vec *decls = r.value;
      Vec none = vec_new(heap->arena);
      if (decls == NULL) heap->oom = true;
      return rust_out_carry(heap, chunks, decls != NULL ? decls : &none, env, file);
    }
  }
  return rust_out_car(heap);
}

static RoCar rust_out_carried(Heap *heap, const EtEnv *env, Str file, Str text) {
  Vec chunks = rust_out_chunks_of(heap, text);
  return rust_out_car_parsed(heap, parser_program(heap, text), &chunks, env, file);
}

/* block(name, lines) pushed onto `acc`: nothing for no lines, else "-- from NAME", "" and the lines. */
static void rust_out_block(Heap *heap, Vec *acc, Str name, const Vec *lines) {
  if (lines->len == 0) return;
  Str parts[] = {str_lit("-- from "), name};
  rust_out_push(heap, acc, rust_out_str_box(heap, rust_out_cat(heap, parts, 2)));
  rust_out_push(heap, acc, &rust_out_empty_line);
  rust_out_push_all(heap, acc, lines);
}

/* The crate. */

/* One input file: the name with no directory, and the text. */
typedef struct {
  Str name;
  Str text;
} RoSrc;

/* Acc{env, mods, carrier, bad}: the files so far. Unlike the Bend Acc, `mods` (RoMod pointers) is in
 * SOURCE order (new_mod sorts the module names it reads, so the order of its `mods` argument does
 * not change its result); finish reverses it once for dup_bad, which reads the Bend order. One
 * RoAcc owns its Vecs: the steps push into them in place and never copy them. */
typedef struct {
  EtEnv env;
  Vec mods;
  Vec carrier;
  Vec bad;
} RoAcc;

static void rust_out_file_car(Heap *heap, const RoCar *car, RoAcc *acc, Str name) {
  rust_out_block(heap, &acc->carrier, name, &car->text);
  rust_out_push_all(heap, &acc->bad, &car->bad);
}

/* file_es: es.items and es.bad are in source order (D-196 a), so the Bend List.reverse calls have no
 * C counterpart. The old acc->bad is empty here (file_step), as in Bend. */
static void rust_out_file_es(Heap *heap, const EtEs *es, RoAcc *acc, Str name, Str text) {
  RoCar car = rust_out_carried(heap, &es->env, name, text);
  const RoMod *m = rust_out_new_mod(heap, &acc->mods, name, &es->items);
  acc->env = es->env;
  rust_out_push(heap, &acc->mods, m);
  acc->bad = rust_out_name_bad(heap, name);
  rust_out_push_all(heap, &acc->bad, &es->bad);
  rust_out_file_car(heap, &car, acc, name);
}

static void rust_out_file_go(Heap *heap, RoAcc *acc, const RoSrc *src) {
  EtEs es = erase_typed_emit_source(heap, &acc->env, src->text);
  rust_out_file_es(heap, &es, acc, src->name, src->text);
}

/* A file after a refused file is not read: its globals are not there. */
static void rust_out_file_step(Heap *heap, RoAcc *acc, const RoSrc *src) {
  if (acc->bad.len == 0) rust_out_file_go(heap, acc, src);
}

static void rust_out_file_steps(Heap *heap, const Vec *srcs, RoAcc *acc) {
  for (size_t i = 0; i < srcs->len; i++) {
    const RoSrc *src = srcs->items[i];
    if (src == NULL) {
      heap->oom = true;
      return;
    }
    rust_out_file_step(heap, acc, src);
  }
}

static Vec rust_out_mod_names(Heap *heap, const Vec *mods) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < mods->len; i++) {
    const RoMod *m = mods->items[i];
    if (m != NULL) rust_out_push(heap, &out, &m->name);
  }
  return out;
}

static Vec rust_out_all_names(Heap *heap, const Vec *mods) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < mods->len; i++) {
    const RoMod *m = mods->items[i];
    if (m != NULL) rust_out_push_all(heap, &out, &m->names);
  }
  return out;
}

static bool rust_out_any_nat(const Vec *mods) {
  for (size_t i = 0; i < mods->len; i++) {
    const RoMod *m = mods->items[i];
    if (m != NULL && m->nat) return true;
  }
  return false;
}

/* dup_line pushed onto `acc`: nothing for d = "", else the collision line. */
static void rust_out_dup_line(Heap *heap, Vec *acc, Str what, Str d) {
  if (d.len == 0) return;
  Str parts[] = {str_lit("refused: "), what, str_lit(" collision (D7): "), d};
  rust_out_push(heap, acc, rust_out_str_box(heap, rust_out_cat(heap, parts, 4)));
}

/* dup_bad pushed onto `acc`; `mods` in the Bend order (the last file first). */
static void rust_out_dup_bad(Heap *heap, const Vec *mods, Vec *acc) {
  Vec mod_list = rust_out_mod_names(heap, mods);
  Vec reserved = rust_out_reserved(heap);
  rust_out_push_all(heap, &mod_list, &reserved);
  Vec names = rust_out_all_names(heap, mods);
  Vec nat = rust_out_nat_names(heap);
  rust_out_push_all(heap, &names, &nat);
  rust_out_dup_line(heap, acc, str_lit("module name"), erase_typed_first_dup(&mod_list));
  rust_out_dup_line(heap, acc, str_lit("name"), erase_typed_first_dup(&names));
}

/* quote() = the `"` character; the C literals below write it as \". */
static Str rust_out_cargo_toml(void) {
  return str_lit("[package]\nname = \"mech_out\"\nversion = \"0.1.0\"\nedition = \"2021\"\nlicense = \"MIT OR Apache-2.0\"\n\n[dependencies]\n");
}

static Str rust_out_pub_mods(Heap *heap, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    if (!strbuf_push(&buf, str_lit("pub mod ")) || !strbuf_push(&buf, rust_out_str_at(names, i)) ||
        !strbuf_push(&buf, str_lit(";\n")))
      heap->oom = true;
  }
  return strbuf_str(&buf);
}

static Str rust_out_lib_rs(Heap *heap, const Vec *names) {
  Vec sorted = rust_out_sorted(heap, names);
  Str parts[] = {str_lit("//! Emitted by `mech rust-out`.\n\n"), rust_out_pub_mods(heap, &sorted)};
  return rust_out_cat(heap, parts, 2);
}

static Str rust_out_carrier_head(void) {
  return str_lit("-- Carrier of `mech rust-out`: the declarations with no Rust form, in source order.");
}

/* The kernel `Nat` module (Q5): limbs in base 10^9, no dependency. The Bend Et.join("\n", lines) ++
 * "\n" is one literal here: each line ends in "\n". */
/* nat_rs literal begin */
Str rust_out_nat_rs(void) {
  return str_lit(
      "//! Kernel `Nat` for emitted crates: an unbounded natural number.\n"
      "\n"
      "use std::fmt;\n"
      "\n"
      "const BASE: u64 = 1_000_000_000;\n"
      "\n"
      "/// Little-endian limbs in base 10^9 with no trailing zero limb.\n"
      "#[derive(Clone, PartialEq, Eq)]\n"
      "pub struct Nat(Vec<u32>);\n"
      "\n"
      "fn limb(value: u64) -> u32 {\n"
      "    u32::try_from(value % BASE).unwrap_or(0)\n"
      "}\n"
      "\n"
      "fn pushed(limbs: Vec<u32>, last: u32) -> Vec<u32> {\n"
      "    limbs.into_iter().chain(std::iter::once(last)).collect()\n"
      "}\n"
      "\n"
      "fn trimmed(limbs: Vec<u32>) -> Vec<u32> {\n"
      "    let kept = limbs.iter().rposition(|l| *l != 0).map_or(0, |i| i + 1);\n"
      "    limbs.into_iter().take(kept).collect()\n"
      "}\n"
      "\n"
      "pub fn nat_small(value: u32) -> Nat {\n"
      "    let wide = u64::from(value);\n"
      "    Nat(trimmed(vec![limb(wide), limb(wide / BASE)]))\n"
      "}\n"
      "\n"
      "pub fn nat_add(left: &Nat, right: &Nat) -> Nat {\n"
      "    let width = left.0.len().max(right.0.len());\n"
      "    let (limbs, carry) = (0..width).fold((Vec::new(), 0u64), |(limbs, carry), i| {\n"
      "        let sum = u64::from(left.0.get(i).copied().unwrap_or(0))\n"
      "            + u64::from(right.0.get(i).copied().unwrap_or(0))\n"
      "            + carry;\n"
      "        (pushed(limbs, limb(sum)), sum / BASE)\n"
      "    });\n"
      "    Nat(trimmed(pushed(limbs, limb(carry))))\n"
      "}\n"
      "\n"
      "pub fn nat_decimal(n: &Nat) -> String {\n"
      "    n.0.split_last().map_or_else(\n"
      "        || String::from(\"0\"),\n"
      "        |(top, rest)| {\n"
      "            rest.iter()\n"
      "                .rev()\n"
      "                .fold(top.to_string(), |text, l| format!(\"{text}{l:09}\"))\n"
      "        },\n"
      "    )\n"
      "}\n"
      "\n"
      "impl fmt::Debug for Nat {\n"
      "    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {\n"
      "        f.write_str(&nat_decimal(self))\n"
      "    }\n"
      "}\n");
}
/* nat_rs literal end */

static const IoFile rust_out_file_placeholder = {{0}, {0}};

/* C.Pair{name, contents} as a boxed IoFile (a placeholder on OOM). */
static const IoFile *rust_out_pair(Heap *heap, Str name, Str contents) {
  IoFile *f = heap_alloc(heap, sizeof(IoFile));
  if (f == NULL) {
    heap->oom = true;
    return &rust_out_file_placeholder;
  }
  f->name = name;
  f->contents = contents;
  return f;
}

/* src_files pushed onto `acc`: "NAME.rs" and the text of each module, in the order of `mods`. */
static void rust_out_src_files(Heap *heap, const Vec *mods, Vec *acc) {
  for (size_t i = 0; i < mods->len; i++) {
    const RoMod *m = mods->items[i];
    if (m == NULL) continue;
    Str parts[] = {m->name, str_lit(".rs")};
    rust_out_push(heap, acc, rust_out_pair(heap, rust_out_cat(heap, parts, 2), m->text));
  }
}

static void rust_out_nat_file(Heap *heap, bool nat, Vec *acc) {
  if (nat) rust_out_push(heap, acc, rust_out_pair(heap, str_lit("nat.rs"), rust_out_nat_rs()));
}

/* Out{root, src}: the files (IoFile pointers) of the crate directory and of `src/`. */
typedef struct {
  Vec root;
  Vec src;
} RoOut;

/* Result<String, Out>: `out` when ok, else `why`. */
typedef struct {
  bool ok;
  RoOut out;
  Str why;
} RoCrate;

/* crate_out: `mods` in source order; `files` = the input file names (Str pointers) in order. */
static RoCrate rust_out_crate_out(Heap *heap, const Vec *bad, const Vec *mods, const Vec *carrier, const Vec *files) {
  RoCrate r = {false, {vec_new(heap->arena), vec_new(heap->arena)}, str_empty()};
  if (bad->len > 0) {
    Str parts[] = {rust_out_join_lines(heap, bad), str_lit("\n")};
    r.why = rust_out_cat(heap, parts, 2);
    return r;
  }
  bool nat = rust_out_any_nat(mods);
  Vec carrier_lines = rust_out_one(heap, rust_out_carrier_head());
  Str files_parts[] = {str_lit("-- files "), erase_typed_join(heap, str_lit(" "), files)};
  rust_out_push(heap, &carrier_lines, rust_out_str_box(heap, rust_out_cat(heap, files_parts, 2)));
  rust_out_push(heap, &carrier_lines, &rust_out_empty_line);
  rust_out_push_all(heap, &carrier_lines, carrier);
  rust_out_push(heap, &r.out.root, rust_out_pair(heap, str_lit("Cargo.toml"), rust_out_cargo_toml()));
  rust_out_push(heap, &r.out.root, rust_out_pair(heap, str_lit("mech-carrier.mech"), rust_out_join_lines(heap, &carrier_lines)));
  Vec lib_names = rust_out_nat_mod(heap, nat);
  Vec names = rust_out_mod_names(heap, mods);
  rust_out_push_all(heap, &lib_names, &names);
  rust_out_push(heap, &r.out.src, rust_out_pair(heap, str_lit("lib.rs"), rust_out_lib_rs(heap, &lib_names)));
  rust_out_nat_file(heap, nat, &r.out.src);
  rust_out_src_files(heap, mods, &r.out.src);
  r.ok = true;
  return r;
}

static RoCrate rust_out_finish(Heap *heap, const RoAcc *acc, const Vec *files) {
  Vec bend_mods = vec_new(heap->arena);
  for (size_t i = acc->mods.len; i > 0; i--) rust_out_push(heap, &bend_mods, acc->mods.items[i - 1]);
  Vec bad = vec_new(heap->arena);
  rust_out_push_all(heap, &bad, &acc->bad);
  rust_out_dup_bad(heap, &bend_mods, &bad);
  return rust_out_crate_out(heap, &bad, &acc->mods, &acc->carrier, files);
}

/* Rust dependencies omit proof-only edges. Keep every source file in order, including files with
 * only runtime declarations or only carried declarations. */
static Vec rust_out_source_files(Heap *heap, const Vec *srcs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < srcs->len; i++) {
    const RoSrc *src = srcs->items[i];
    if (src != NULL) rust_out_push(heap, &out, &src->name);
  }
  return out;
}

/* The files of the crate, or the refusal lines. `srcs`: RoSrc pointers in order. */
static RoCrate rust_out_crate_of(Heap *heap, const Vec *srcs) {
  RoAcc acc = {erase_typed_env(global_initial(heap), vec_new(heap->arena)), vec_new(heap->arena),
               vec_new(heap->arena), vec_new(heap->arena)};
  rust_out_file_steps(heap, srcs, &acc);
  Vec files = rust_out_source_files(heap, srcs);
  return rust_out_finish(heap, &acc, &files);
}

/* The command. Each Bend IO(Unit) is an int: 0 = done (the program goes on), else the exit status
 * after the stderr line (D-197 b). */

static int rust_out_usage(void) { return cli_die(64, str_lit("usage: mech rust-out FILE.mech... OUT_DIR")); }

static RustOutText rust_out_read_result(Heap *heap, IoResult result, Str path) {
  RustOutText t = {false, str_empty(), 0};
  switch (result.status) {
    case IO_DONE:
      t.ok = true;
      t.text = result.value;
      return t;
    case IO_FAIL: {
      Str parts[] = {str_lit("mech: cannot read "), path};
      Str message = rust_out_cat(heap, parts, 2);
      t.code = heap->oom ? cli_host_failure() : cli_die(64, message);
      return t;
    }
  }
  return t;
}

RustOutText rust_out_read_file(Heap *heap, Str path) { return rust_out_read_result(heap, io_read_file(heap, path), path); }

/* read_srcs: the files in order into `srcs` (RoSrc pointers); stops at the first file that fails. */
static int rust_out_read_srcs(Heap *heap, const Vec *paths, Vec *srcs) {
  for (size_t i = 0; i < paths->len; i++) {
    Str path = rust_out_str_at(paths, i);
    RustOutText t = rust_out_read_file(heap, path);
    if (!t.ok) return t.code;
    RoSrc *src = heap_alloc(heap, sizeof(RoSrc));
    if (src == NULL) {
      heap->oom = true;
      return cli_host_failure();
    }
    src->name = rust_out_base_name(path);
    src->text = t.text;
    rust_out_push(heap, srcs, src);
  }
  return 0;
}

int rust_out_wrote(Heap *heap, IoResult result) {
  switch (result.status) {
    case IO_DONE:
      return 0;
    case IO_FAIL: {
      Str parts[] = {str_lit("mech: "), result.value};
      Str message = rust_out_cat(heap, parts, 2);
      return heap->oom ? cli_host_failure() : cli_die(1, message);
    }
  }
  return 0;
}

static RustOutText rust_out_made(Heap *heap, IoResult result) {
  RustOutText t = {false, str_empty(), 0};
  switch (result.status) {
    case IO_DONE:
      t.ok = true;
      t.text = result.value;
      return t;
    case IO_FAIL: {
      Str parts[] = {str_lit("mech: "), result.value};
      Str message = rust_out_cat(heap, parts, 2);
      t.code = heap->oom ? cli_host_failure() : cli_die(1, message);
      return t;
    }
  }
  return t;
}

IoResult rust_out_absent(Heap *heap, bool present, Str out) {
  if (present) {
    Str parts[] = {str_lit("output already exists: "), out};
    IoResult fail = {IO_FAIL, rust_out_cat(heap, parts, 2)};
    return fail;
  }
  IoResult done = {IO_DONE, str_empty()};
  return done;
}

/* The crate goes to a temporary directory next to the target, then one rename publishes it. `src`
 * is made the same way (the host has no mkdir). A write failure leaves the temporary directory. */
static int rust_out_write_crate(Heap *heap, Str out, const RoOut *o) {
  Str target = io_normalize_target(out);
  RustOutText top = rust_out_made(heap, io_fs(heap, HOST_FS_STAGE, target, str_empty()));
  if (!top.ok) return top.code;
  int code = rust_out_wrote(heap, io_write_files(heap, &o->root, 0, top.text));
  if (code != 0) return code;
  Str src_parts[] = {top.text, str_lit("/src")};
  Str src_dir = rust_out_cat(heap, src_parts, 2);
  if (heap->oom) return cli_host_failure();
  RustOutText part = rust_out_made(heap, io_fs(heap, HOST_FS_STAGE, src_dir, str_empty()));
  if (!part.ok) return part.code;
  code = rust_out_wrote(heap, io_write_files(heap, &o->src, 0, part.text));
  if (code != 0) return code;
  code = rust_out_wrote(heap, io_publish_rename(heap, false, part.text, src_dir, out));
  if (code != 0) return code;
  bool present = io_exists(heap, target);
  return rust_out_wrote(heap, io_publish_rename(heap, present, top.text, target, out));
}

static int rust_out_emit_crate(Heap *heap, const RoCrate *r, Str out) {
  if (r->ok) return rust_out_write_crate(heap, out, &r->out);
  return cli_die(65, r->why);
}

/* Cmd{paths, out} | CmdNone{}: `paths` = the input files in order (Str pointers), `out` = the last
 * argument. */
typedef enum { RO_CMD, RO_CMD_NONE } RoCmdKind;
typedef struct {
  RoCmdKind kind;
  Vec paths;
  Str out;
} RoCmd;

/* cmd_of: no arguments = CmdNone; else the arguments before the last one are the paths. The Bend
 * cmd_push (a prepend onto the Cmd of the rest) is the forward push here. */
static RoCmd rust_out_cmd_of(Heap *heap, const Vec *args) {
  RoCmd c = {RO_CMD_NONE, vec_new(heap->arena), str_empty()};
  if (args->len == 0) return c;
  for (size_t i = 0; i + 1 < args->len; i++) rust_out_push(heap, &c.paths, args->items[i]);
  c.kind = RO_CMD;
  c.out = rust_out_str_at(args, args->len - 1);
  return c;
}

static int rust_out_run_cmd(Heap *heap, const RoCmd *c) {
  switch (c->kind) {
    case RO_CMD_NONE:
      return rust_out_usage();
    case RO_CMD: {
      if (c->paths.len == 0) return rust_out_usage();
      bool present = io_exists(heap, io_normalize_target(c->out));
      int code = rust_out_wrote(heap, rust_out_absent(heap, present, c->out));
      if (code != 0) return code;
      Vec srcs = vec_new(heap->arena);
      code = rust_out_read_srcs(heap, &c->paths, &srcs);
      if (code != 0) return code;
      RoCrate r = rust_out_crate_of(heap, &srcs);
      if (heap->oom) return cli_host_failure();
      return rust_out_emit_crate(heap, &r, c->out);
    }
  }
  return rust_out_usage();
}

int rust_out_run(Heap *heap, const Vec *args) {
  RoCmd c = rust_out_cmd_of(heap, args);
  if (heap->oom) return cli_host_failure();
  return rust_out_run_cmd(heap, &c);
}
