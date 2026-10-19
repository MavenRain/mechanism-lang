/* Port of rust/resolve.bend: resolve for the Rust importer. It builds the module list of
 * lib.rs and its order (CD3), the item table with the mech name of each Rust name (CD5, the inverse
 * of D7), the collision checks, the name check of each body, and the `.mech` file of each module
 * (CD4). `src/nat.rs` is not parsed: it must be the fixed text of the emitter (CD7). The D7
 * functions, the module name rules and the text of `src/nat.rs` come from the emitter
 * (erase_typed.c, cli/rust_out.c), so the two directions use one set of rules.
 *
 * A refusal has the position of its item (CD13), and the shown text is
 * `src/<module>.rs:<line>:<col>: <construct>`. The definitions keep the Bend names (prefix
 * resolve_) and the Bend order; the C only helpers stand just before their first use.
 *
 * D-215a: each Bend Result<Bad, A> is the one C only struct ResolveR (ok, bad, and one field per A:
 * flag = Bool, list = any List as a Vec of pointers in source order, str = String, mod = Mod; later
 * parts add the fields of their A). A Bend List<String> is a Vec of Str pointers. */
#include "rust.h"
#include "cli.h"

/* C: Result<Bad, A> for each A of this file (D-215a). */
typedef struct {
  bool ok;
  ResolveBad bad;            /* !ok */
  bool flag;                 /* A = Bool */
  Vec list;                  /* A = List<..> (pointers, source order) */
  Str str;                   /* A = String */
  const ResolveMod *mod;     /* A = Mod */
  const ResolveRow *row;     /* A = Row */
  ResolveOut out;            /* A = Out */
} ResolveR;

/* C: Fail{Bad{module, pos, what}} */
static ResolveR resolve_fail(Str module, AstPos pos, Str what) {
  ResolveR r = {0};
  r.ok = false;
  r.bad.module = module;
  r.bad.pos = pos;
  r.bad.what = what;
  return r;
}

/* C: Done{b} */
static ResolveR resolve_done_flag(bool b) {
  ResolveR r = {0};
  r.ok = true;
  r.flag = b;
  return r;
}

/* C: Done{list} */
static ResolveR resolve_done_list(Vec list) {
  ResolveR r = {0};
  r.ok = true;
  r.list = list;
  return r;
}

/* C: Done{s} */
static ResolveR resolve_done_str(Str s) {
  ResolveR r = {0};
  r.ok = true;
  r.str = s;
  return r;
}

/* C: Done{m} */
static ResolveR resolve_done_mod(const ResolveMod *m) {
  ResolveR r = {0};
  r.ok = true;
  r.mod = m;
  return r;
}

/* C: push one pointer; a failed push sets heap->oom. */
static void resolve_push(Heap *heap, Vec *v, const void *x) {
  if (!vec_push(v, (void *)x)) heap->oom = true;
}

/* C: a ++ b ++ c */
static Str resolve_cat3(Heap *heap, Str a, Str b, Str c) {
  Str ab = heap_str_cat(heap, a, b);
  return heap_str_cat(heap, ab, c);
}

/* show(b): `src/<module>.rs:<line>:<col>: <construct>`. */
Str resolve_show(Heap *heap, const ResolveBad *b) {
  Str head = resolve_cat3(heap, str_lit("src/"), b->module, str_lit(".rs:"));
  Str at = resolve_cat3(heap, head, ast_show_pos(heap, b->pos), str_lit(": "));
  return heap_str_cat(heap, at, b->what);
}

/* top(): the position of the `//!` line. */
static AstPos resolve_top(Heap *heap) {
  return ast_pos(heap_nat_of_u64(heap, 1), heap_nat_of_u64(heap, 1));
}

/* ---- lib.rs ---- */

/* in_module(module, r): the refusal of lift moved to the module. */
static ResolveR resolve_in_module(Str module, LiftFlagResult r) {
  if (r.ok) return resolve_done_flag(r.flag);
  return resolve_fail(module, r.refusal.pos, r.refusal.what);
}

/* plain_in(module, m) */
static ResolveR resolve_plain_in(Str module, const AstMeta *m) {
  return resolve_in_module(module, lift_plain_result(m));
}

/* lib_mod(meta, vis, inline, items): a `pub mod <name>;` line with no docs and no attributes. */
static ResolveR resolve_lib_mod(const AstMeta *meta, AstVis vis, bool is_inline, const Vec *items) {
  if (vis == AST_PUB && items->len == 0) {
    if (is_inline) return resolve_fail(str_lit("lib"), ast_meta_pos(meta), str_lit("inline `mod` in lib.rs"));
    return resolve_plain_in(str_lit("lib"), meta);
  }
  return resolve_fail(str_lit("lib"), ast_meta_pos(meta), str_lit("item other than `pub mod <name>;` in lib.rs"));
}

/* mod_named(pos, name): a module name that the emitter writes: the D7 module name of itself, and
 * not one of the names of the crate layout. `nat` is the kernel module. */
static ResolveR resolve_mod_named(Heap *heap, AstPos pos, Str name) {
  bool ok = rust_out_mod_ok(name) && str_eq(rust_out_mod_name(heap, name), name);
  if (ok && !str_eq(name, str_lit("nat"))) {
    Vec reserved = rust_out_reserved(heap);
    ok = !erase_typed_has(&reserved, name);
  }
  if (ok) return resolve_done_flag(true);
  Str what = resolve_cat3(heap, str_lit("module name `"), name, str_lit("` that the emitter does not write (D7)"));
  return resolve_fail(str_lit("lib"), pos, what);
}

/* mod_decls(xs): the module names of the `pub mod` lines, in order. */
static ResolveR resolve_mod_decls(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const AstNode *n = g.value;
    if (n->kind != AST_IMOD) {
      Str what = resolve_cat3(heap, str_lit("item `"), ast_item_line(heap, n), str_lit("` in lib.rs"));
      return resolve_fail(str_lit("lib"), ast_item_pos(heap, n), what);
    }
    ResolveR ok = resolve_lib_mod(&n->meta, n->vis, n->is_inline, &n->items);
    if (!ok.ok) return ok;
    ResolveR named = resolve_mod_named(heap, ast_meta_pos(&n->meta), n->name);
    if (!named.ok) return named;
    resolve_push(heap, &out, &n->name);
  }
  return resolve_done_list(out);
}

/* distinct_mods(names) */
static ResolveR resolve_distinct_mods(Heap *heap, Vec names) {
  Str d = erase_typed_first_dup(&names);
  if (str_eq(d, str_empty())) return resolve_done_list(names);
  return resolve_fail(str_lit("lib"), resolve_top(heap), heap_str_cat(heap, str_lit("module name collision (D7): "), d));
}

/* lib_mods(r): the module names of lib.rs, in manifest order. */
ResolveNamesResult resolve_lib_mods(Heap *heap, const RustSrcFileResult *r) {
  ResolveNamesResult out = {0};
  ResolveR got = {0};
  if (!r->ok) got = resolve_fail(str_lit("lib"), ast_no_pos(heap), heap_str_cat(heap, str_lit("parse: "), r->message));
  if (r->ok) {
    ResolveR names = resolve_mod_decls(heap, &r->file.items);
    got = names;
    if (names.ok) got = resolve_distinct_mods(heap, names.list);
  }
  out.ok = got.ok;
  out.bad = got.bad;
  out.names = got.list;
  return out;
}

/* ---- module files ---- */

/* text_of(srcs, name): the text of the first Src (ResolveSrc pointers) named `name`. */
static MaybeStr resolve_text_of(const Vec *srcs, Str name) {
  MaybeStr out = {0};
  out.some = false;
  for (size_t i = 0; !out.some && i < srcs->len; i++) {
    VecGet g = vec_get(srcs, i);
    const ResolveSrc *s = g.status == VEC_GET_OK ? g.value : NULL;
    if (s && str_eq(s->name, name)) {
      out.some = true;
      out.value = s->text;
    }
  }
  return out;
}

/* drop_nat(xs): the names (Str pointers) other than `nat`, in order. */
static Vec resolve_drop_nat(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK && !str_eq(*(const Str *)g.value, str_lit("nat"))) resolve_push(heap, &out, g.value);
  }
  return out;
}

/* nat_text(t), CD7: the kernel module is the fixed text of the emitter, byte for byte. */
static ResolveR resolve_nat_text(Heap *heap, MaybeStr t) {
  if (!t.some) return resolve_fail(str_lit("nat"), ast_no_pos(heap), str_lit("no text for the module"));
  if (str_eq(t.value, rust_out_nat_rs())) return resolve_done_flag(true);
  return resolve_fail(str_lit("nat"), resolve_top(heap), str_lit("text other than the fixed `nat` module of the emitter (CD7)"));
}

/* nat_need(has, t) */
static ResolveR resolve_nat_need(Heap *heap, bool has, MaybeStr t) {
  if (has) return resolve_nat_text(heap, t);
  return resolve_done_flag(false);
}

/* placed(xs, rs): the position of each item of the IR (ResolvePlaced pointers); a unit struct and
 * its constructor fn are two Rust items (AstNode pointers) and one IR item (RirItem pointers). */
static Vec resolve_placed(Heap *heap, const Vec *xs, const Vec *rs) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (size_t j = 0; i < xs->len && j < rs->len; j++) {
    VecGet x = vec_get(xs, i);
    VecGet r = vec_get(rs, j);
    if (x.status != VEC_GET_OK || r.status != VEC_GET_OK) return out;
    const RirItem *item = r.value;
    ResolvePlaced *p = heap_alloc(heap, sizeof(ResolvePlaced));
    if (!p) return out;
    p->pos = ast_item_pos(heap, x.value);
    p->item = item;
    resolve_push(heap, &out, p);
    i += item->kind == RIR_IUNIT && i + 1 < xs->len ? 2 : 1;
  }
  return out;
}

/* file_ok(module, file), CD4: the stem of the file must give the module name back (D7), so
 * `rust-out` on the file writes the same module. */
static ResolveR resolve_file_ok(Heap *heap, Str module, Str file) {
  Str stem = erase_typed_stem(file);
  bool ok = str_eq(heap_str_cat(heap, stem, str_lit(".mech")), file) && rust_out_mod_ok(stem) &&
            str_eq(rust_out_mod_name(heap, stem), module);
  if (ok) return resolve_done_str(file);
  Str what = resolve_cat3(heap, str_lit("file `"), file, str_lit("` that does not give the module name (CD4)"));
  return resolve_fail(module, resolve_top(heap), what);
}

/* file_name(module, source): the file of the `//!` line of the emitter, else `<module>.mech`. */
static ResolveR resolve_file_name(Heap *heap, Str module, Str source) {
  Str file = source;
  if (str_eq(source, str_empty())) file = heap_str_cat(heap, module, str_lit(".mech"));
  return resolve_file_ok(heap, module, file);
}

/* src_items(f) */
static const Vec *resolve_src_items(const AstSrcFile *f) {
  return &f->items;
}

/* with_lift(name, items, r) */
static ResolveR resolve_with_lift(Heap *heap, Str name, const Vec *items, const LiftModuleResult *r) {
  if (!r->ok) return resolve_fail(name, r->refusal.pos, r->refusal.what);
  ResolveR file = resolve_file_name(heap, name, r->module.source);
  if (!file.ok) return file;
  ResolveMod *m = heap_alloc(heap, sizeof(ResolveMod));
  if (!m) return resolve_fail(name, ast_no_pos(heap), str_empty());
  m->name = name;
  m->file = file.str;
  m->items = resolve_placed(heap, items, &r->module.items);
  return resolve_done_mod(m);
}

/* parsed(name, r) */
static ResolveR resolve_parsed(Heap *heap, Str name, const RustSrcFileResult *r) {
  if (!r->ok) return resolve_fail(name, ast_no_pos(heap), heap_str_cat(heap, str_lit("parse: "), r->message));
  LiftModuleResult lifted = lift_lift(heap, &r->file);
  return resolve_with_lift(heap, name, resolve_src_items(&r->file), &lifted);
}

/* load_one(name, t) */
static ResolveR resolve_load_one(Heap *heap, Str name, MaybeStr t) {
  if (!t.some) return resolve_fail(name, ast_no_pos(heap), str_lit("no text for the module"));
  RustSrcFileResult f = rust_parser_file(heap, t.value);
  return resolve_parsed(heap, name, &f);
}

/* load(names, srcs): the Mod of each name (ResolveMod pointers), in order. */
static ResolveR resolve_load(Heap *heap, const Vec *names, const Vec *srcs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    VecGet g = vec_get(names, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    Str name = *(const Str *)g.value;
    ResolveR m = resolve_load_one(heap, name, resolve_text_of(srcs, name));
    if (!m.ok) return m;
    resolve_push(heap, &out, m.mod);
  }
  return resolve_done_list(out);
}


/* ---- names (CD5) ---- */

/* The inverse of rule D7 is in erase_typed.c (erase_typed_fn_back, erase_typed_lower_first): the
 * emitter and the importer use one definition. */

/* C: Done{row} */
static ResolveR resolve_done_row(const ResolveRow *row) {
  ResolveR r = {0};
  r.ok = true;
  r.row = row;
  return r;
}

/* C: Row{module, k, rust, mech, pos} on the heap; NULL when the heap is full. */
static const ResolveRow *resolve_row(Heap *heap, Str module, ResolveRk k, Str rust, Str mech, AstPos pos) {
  ResolveRow *row = heap_alloc(heap, sizeof(ResolveRow));
  if (!row) return NULL;
  row->module = module;
  row->kind = k;
  row->rust = rust;
  row->mech = mech;
  row->pos = pos;
  return row;
}

/* C: Done{[Row{module, k, rust, mech, pos}] ++ rest} */
static ResolveR resolve_row_then(Heap *heap, Str module, ResolveRk k, Str rust, Str mech, AstPos pos, const Vec *rest) {
  const ResolveRow *row = resolve_row(heap, module, k, rust, mech, pos);
  if (!row) return resolve_fail(module, pos, str_empty());
  Vec out = vec_new(heap->arena);
  resolve_push(heap, &out, row);
  return resolve_done_list(heap_vec_concat(heap, &out, rest));
}

/* named(module, k, shown, rust, mech, back, pos): a row if the name is canonical: D7 of the mech
 * name (`back`) is the Rust name. `shown` is the Rust name in the table. */
static ResolveR resolve_named(Heap *heap, Str module, ResolveRk k, Str shown, Str rust, Str mech, Str back, AstPos pos) {
  if (str_eq(back, rust)) {
    const ResolveRow *row = resolve_row(heap, module, k, shown, mech, pos);
    if (!row) return resolve_fail(module, pos, str_empty());
    return resolve_done_row(row);
  }
  Str head = resolve_cat3(heap, str_lit("name `"), rust, str_lit("` that is not canonical: D7 of `"));
  Str mid = resolve_cat3(heap, head, mech, str_lit("` is `"));
  return resolve_fail(module, pos, resolve_cat3(heap, mid, back, str_lit("` (CD5)")));
}

/* fn_row(module, k, rust, pos): a fn or the constructor fn of a unit struct: snake_case ->
 * camelCase. */
static ResolveR resolve_fn_row(Heap *heap, Str module, ResolveRk k, Str rust, AstPos pos) {
  Str mech = erase_typed_fn_back(heap, rust);
  return resolve_named(heap, module, k, rust, rust, mech, erase_typed_fn_name(heap, mech), pos);
}

/* variant_row(module, family, v, pos): a variant: UpperCamel -> a constructor with a lower-case
 * first letter. */
static ResolveR resolve_variant_row(Heap *heap, Str module, Str family, Str v, AstPos pos) {
  Str mech = erase_typed_lower_first(heap, v);
  Str shown = resolve_cat3(heap, family, str_lit("::"), v);
  return resolve_named(heap, module, RESOLVE_RK_VARIANT, shown, v, mech, erase_typed_upper_first(heap, mech), pos);
}

/* variant_rows(module, family, pos, vs): the row of each variant (RirVariant pointers), in order;
 * the first refusal stops. */
static ResolveR resolve_variant_rows(Heap *heap, Str module, Str family, AstPos pos, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    VecGet g = vec_get(vs, i);
    if (g.status != VEC_GET_OK) return resolve_fail(module, pos, str_empty());
    const RirVariant *v = g.value;
    ResolveR r = resolve_variant_row(heap, module, family, v->name, pos);
    if (!r.ok) return r;
    resolve_push(heap, &out, r.row);
  }
  return resolve_done_list(out);
}

/* item_rows(module, p): the rows of one item. A family name is kept (D7). */
static ResolveR resolve_item_rows(Heap *heap, Str module, const ResolvePlaced *p) {
  const RirItem *it = p->item;
  Vec none = vec_new(heap->arena);
  switch (it->kind) {
    case RIR_IUSE:
      return resolve_done_list(none);
    case RIR_IENUM: {
      ResolveR vr = resolve_variant_rows(heap, module, it->name, p->pos, &it->variants);
      if (!vr.ok) return vr;
      return resolve_row_then(heap, module, RESOLVE_RK_ENUM, it->name, it->name, p->pos, &vr.list);
    }
    case RIR_IUNIT: {
      ResolveR c = resolve_fn_row(heap, module, RESOLVE_RK_CTOR, it->ctor, p->pos);
      if (!c.ok) return c;
      Vec rest = vec_new(heap->arena);
      resolve_push(heap, &rest, c.row);
      return resolve_row_then(heap, module, RESOLVE_RK_STRUCT, it->name, it->name, p->pos, &rest);
    }
    case RIR_IFN: {
      ResolveR f = resolve_fn_row(heap, module, RESOLVE_RK_FN, it->name, p->pos);
      if (!f.ok) return f;
      resolve_push(heap, &none, f.row);
      return resolve_done_list(none);
    }
  }
  return resolve_done_list(none);
}

/* mod_rows(module, ps): the rows of the items (ResolvePlaced pointers), in order. */
static ResolveR resolve_mod_rows(Heap *heap, Str module, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    if (g.status != VEC_GET_OK) return resolve_fail(module, ast_no_pos(heap), str_empty());
    ResolveR rs = resolve_item_rows(heap, module, g.value);
    if (!rs.ok) return rs;
    out = heap_vec_concat(heap, &out, &rs.list);
  }
  return resolve_done_list(out);
}

/* all_rows(ms): the rows of the modules (ResolveMod pointers), in order. */
static ResolveR resolve_all_rows(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const ResolveMod *m = g.value;
    ResolveR rs = resolve_mod_rows(heap, m->name, &m->items);
    if (!rs.ok) return rs;
    out = heap_vec_concat(heap, &out, &rs.list);
  }
  return resolve_done_list(out);
}

/* rust_dup(seen, rows): the Rust item names of all modules and the names of the `nat` module must
 * be different (D7, as the emitter checks). A variant is not an item. D-216a: `seen` only feeds
 * Et.has, so the C adds each name at the end of a private copy (Bend: at the front). */
static ResolveR resolve_rust_dup(Heap *heap, const Vec *seen, const Vec *rows) {
  Vec none = vec_new(heap->arena);
  Vec names = heap_vec_concat(heap, seen, &none);
  for (size_t i = 0; i < rows->len; i++) {
    VecGet g = vec_get(rows, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const ResolveRow *row = g.value;
    if (row->kind != RESOLVE_RK_VARIANT) {
      if (erase_typed_has(&names, row->rust)) {
        return resolve_fail(row->module, row->pos, heap_str_cat(heap, str_lit("name collision (D7): "), row->rust));
      }
      resolve_push(heap, &names, &row->rust);
    }
  }
  return resolve_done_flag(true);
}

/* mech_dup(seen, rows): the mech names of all rows must be different, and different from the names
 * that the importer writes itself (MechBool, CD6) or maps to the kernel (CD7). D-216a as rust_dup. */
static ResolveR resolve_mech_dup(Heap *heap, const Vec *seen, const Vec *rows) {
  Vec none = vec_new(heap->arena);
  Vec names = heap_vec_concat(heap, seen, &none);
  for (size_t i = 0; i < rows->len; i++) {
    VecGet g = vec_get(rows, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const ResolveRow *row = g.value;
    if (erase_typed_has(&names, row->mech)) {
      Str head = resolve_cat3(heap, str_lit("mech name collision (CD5): `"), row->rust, str_lit("` gives `"));
      return resolve_fail(row->module, row->pos, resolve_cat3(heap, head, row->mech, str_lit("`")));
    }
    resolve_push(heap, &names, &row->mech);
  }
  return resolve_done_flag(true);
}

/* C: the static strings of this file. */
static const Str resolve_mech_reserved_list[] = {{"MechBool", 8}, {"mechFalse", 9}, {"mechTrue", 8}, {"Nat", 3}, {"natAdd", 6}};
static const Str resolve_nat_ty = {"Nat", 3};
static const Str resolve_nat_small = {"nat_small", 9};
static const Str resolve_nat_add = {"nat_add", 7};

/* mech_reserved(): ["MechBool", "mechFalse", "mechTrue", "Nat", "natAdd"] (Str pointers). */
static Vec resolve_mech_reserved(Heap *heap) {
  Vec out = vec_new(heap->arena);
  size_t n = sizeof resolve_mech_reserved_list / sizeof resolve_mech_reserved_list[0];
  for (size_t i = 0; i < n; i++) resolve_push(heap, &out, &resolve_mech_reserved_list[i]);
  return out;
}

/* ---- scope ---- */

/* Fam.Fam: a family name and its constructor names (Str pointers). */
typedef struct {
  Str name;
  Vec ctors;
} ResolveFam;

/* Scope.Scope: the visible names of one module: types (enums, unit structs, `Nat`), values (fns,
 * constructor fns, `nat_small`, `nat_add`) and the families with their variants. tys and fns are
 * Str pointers, fams ResolveFam pointers, each in Bend list order (head first). */
typedef struct {
  Vec tys;
  Vec fns;
  Vec fams;
} ResolveScope;

/* C: x <> xs as a new Vec (head first); xs is not changed. */
static Vec resolve_cons(Heap *heap, const void *x, const Vec *xs) {
  Vec one = vec_new(heap->arena);
  resolve_push(heap, &one, x);
  return heap_vec_concat(heap, &one, xs);
}

/* empty() */
static ResolveScope resolve_empty(Heap *heap) {
  ResolveScope sc;
  sc.tys = vec_new(heap->arena);
  sc.fns = vec_new(heap->arena);
  sc.fams = vec_new(heap->arena);
  return sc;
}

/* nat_scope(sc), CD7: `nat_decimal` has no kernel form, so it is not visible. */
static ResolveScope resolve_nat_scope(Heap *heap, ResolveScope sc) {
  ResolveScope out;
  out.tys = resolve_cons(heap, &resolve_nat_ty, &sc.tys);
  Vec add = resolve_cons(heap, &resolve_nat_add, &sc.fns);
  out.fns = resolve_cons(heap, &resolve_nat_small, &add);
  out.fams = sc.fams;
  return out;
}

/* tys_of(sc) */
static const Vec *resolve_tys_of(const ResolveScope *sc) {
  return &sc->tys;
}

/* fns_of(sc) */
static const Vec *resolve_fns_of(const ResolveScope *sc) {
  return &sc->fns;
}

/* fams_of(sc) */
static const Vec *resolve_fams_of(const ResolveScope *sc) {
  return &sc->fams;
}

/* variant_names(vs): the name of each variant (RirVariant pointers) as Str pointers, in order. */
static Vec resolve_variant_names(Heap *heap, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    VecGet g = vec_get(vs, i);
    if (g.status == VEC_GET_OK) resolve_push(heap, &out, &((const RirVariant *)g.value)->name);
  }
  return out;
}

/* param_names(ps): the name of each parameter (RirParam pointers) as Str pointers, in order. */
static Vec resolve_param_names(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    if (g.status == VEC_GET_OK) resolve_push(heap, &out, &((const RirParam *)g.value)->name);
  }
  return out;
}

/* bind_names(bs): the names of the BVar binders (RirBind pointers) as Str pointers, in order; a
 * BSkip has no name. */
static Vec resolve_bind_names(Heap *heap, const Vec *bs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < bs->len; i++) {
    VecGet g = vec_get(bs, i);
    if (g.status != VEC_GET_OK) return out;
    const RirBind *b = g.value;
    switch (b->kind) {
      case RIR_BVAR:
        resolve_push(heap, &out, &b->name);
        break;
      case RIR_BSKIP:
        break;
    }
  }
  return out;
}

/* item_scope(p, sc): the scope with the names of one item added at the front. */
static ResolveScope resolve_item_scope(Heap *heap, const ResolvePlaced *p, ResolveScope sc) {
  const RirItem *it = p->item;
  ResolveScope out = sc;
  switch (it->kind) {
    case RIR_IUSE:
      return out;
    case RIR_IENUM: {
      ResolveFam *fam = heap_alloc(heap, sizeof(ResolveFam));
      if (!fam) return out;
      fam->name = it->name;
      fam->ctors = resolve_variant_names(heap, &it->variants);
      out.tys = resolve_cons(heap, &it->name, &sc.tys);
      out.fams = resolve_cons(heap, fam, &sc.fams);
      return out;
    }
    case RIR_IUNIT:
      out.tys = resolve_cons(heap, &it->name, &sc.tys);
      out.fns = resolve_cons(heap, &it->ctor, &sc.fns);
      return out;
    case RIR_IFN:
      out.fns = resolve_cons(heap, &it->name, &sc.fns);
      return out;
  }
  return out;
}

/* items_scope(ps, sc): item_scope of each item (ResolvePlaced pointers), first to last. */
static ResolveScope resolve_items_scope(Heap *heap, const Vec *ps, ResolveScope sc) {
  ResolveScope out = sc;
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    if (g.status == VEC_GET_OK) out = resolve_item_scope(heap, g.value, out);
  }
  return out;
}

/* has_ctor(fams, family, ctor): some family named `family` has the constructor `ctor`. */
static bool resolve_has_ctor(const Vec *fams, Str family, Str ctor) {
  for (size_t i = 0; i < fams->len; i++) {
    VecGet g = vec_get(fams, i);
    const ResolveFam *f = g.value;
    if (g.status == VEC_GET_OK && str_eq(f->name, family) && erase_typed_has(&f->ctors, ctor)) return true;
  }
  return false;
}

/* ---- bodies ---- */

/* Job: JTy{gens, t}, JEx{gens, locals, e}, JArm{gens, locals, arm}. `gens`: the generics of the
 * item. `locals`: the parameters, closure parameters and pattern binders in scope; a local shadows
 * an item. Both are Str pointers in Bend list order and are never changed after the job is made. */
typedef enum { RESOLVE_JTY, RESOLVE_JEX, RESOLVE_JARM } ResolveJobKind;
typedef struct {
  ResolveJobKind kind;
  Vec gens;
  Vec locals;          /* JEX, JARM */
  const RirTy *t;      /* JTY */
  const RirEx *e;      /* JEX */
  const RirArm *arm;   /* JARM */
} ResolveJob;

/* C: one Job on the heap; NULL when the heap is full. */
static const ResolveJob *resolve_job(Heap *heap, ResolveJobKind kind, const Vec *gens, const Vec *locals,
                                     const RirTy *t, const RirEx *e, const RirArm *arm) {
  ResolveJob *j = heap_alloc(heap, sizeof(ResolveJob));
  if (!j) return NULL;
  j->kind = kind;
  j->gens = *gens;
  j->locals = *locals;
  j->t = t;
  j->e = e;
  j->arm = arm;
  return j;
}

/* C: push one job; a NULL job (full heap) is not pushed, heap->oom is set by heap_alloc. */
static void resolve_push_job(Heap *heap, Vec *v, const ResolveJob *j) {
  if (j) resolve_push(heap, v, j);
}

/* tjobs(gens, ts): JTy{gens, t} for each type (RirTy pointers), in order. */
static Vec resolve_tjobs(Heap *heap, const Vec *gens, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  Vec none = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    VecGet g = vec_get(ts, i);
    if (g.status == VEC_GET_OK) resolve_push_job(heap, &out, resolve_job(heap, RESOLVE_JTY, gens, &none, g.value, NULL, NULL));
  }
  return out;
}

/* ejobs(gens, locals, es): JEx{gens, locals, e} for each expression (RirEx pointers), in order. */
static Vec resolve_ejobs(Heap *heap, const Vec *gens, const Vec *locals, const Vec *es) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < es->len; i++) {
    VecGet g = vec_get(es, i);
    if (g.status == VEC_GET_OK) resolve_push_job(heap, &out, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, g.value, NULL));
  }
  return out;
}

/* arm_jobs(gens, locals, arms): JArm{gens, locals, a} for each arm (RirArm pointers), in order. */
static Vec resolve_arm_jobs(Heap *heap, const Vec *gens, const Vec *locals, const Vec *arms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < arms->len; i++) {
    VecGet g = vec_get(arms, i);
    if (g.status == VEC_GET_OK) resolve_push_job(heap, &out, resolve_job(heap, RESOLVE_JARM, gens, locals, NULL, NULL, g.value));
  }
  return out;
}

/* let_tys(t): the type of a let binder, when the Rust text gives one (E1), as RirTy pointers. */
static Vec resolve_let_tys(Heap *heap, RirMaybeTy t) {
  Vec out = vec_new(heap->arena);
  if (t.some) resolve_push(heap, &out, t.value);
  return out;
}

/* C: the work list of walk is a stack whose last element is the head of the Bend list, so the Bend
 * `xs ++ rest` pushes xs from its last job to its first (D-216b). */
static void resolve_front(Heap *heap, Vec *stack, const Vec *xs) {
  for (size_t i = xs->len; i > 0; i--) {
    VecGet g = vec_get(xs, i - 1);
    if (g.status == VEC_GET_OK) resolve_push(heap, stack, g.value);
  }
}

/* C: push one job at the front of the work list. */
static void resolve_front_one(Heap *heap, Vec *stack, const ResolveJob *j) {
  resolve_push_job(heap, stack, j);
}

/* C: Some{a ++ b ++ c} */
static MaybeStr resolve_some3(Heap *heap, Str a, Str b, Str c) {
  MaybeStr m;
  m.some = true;
  m.value = resolve_cat3(heap, a, b, c);
  return m;
}

/* C: one JTy step of walk; Some = the refusal text. */
static MaybeStr resolve_walk_ty(Heap *heap, const ResolveScope *sc, const ResolveJob *j, Vec *stack) {
  MaybeStr none = {0};
  const RirTy *t = j->t;
  Vec no_locals = vec_new(heap->arena);
  switch (t->kind) {
    case RIR_TBOOL:
      return none;
    case RIR_TCON: {
      if (!(erase_typed_has(&j->gens, t->name) || erase_typed_has(resolve_tys_of(sc), t->name))) {
        return resolve_some3(heap, str_lit("unknown type `"), t->name, str_lit("`"));
      }
      Vec more = resolve_tjobs(heap, &j->gens, &t->args);
      resolve_front(heap, stack, &more);
      return none;
    }
    case RIR_TBOX:
    case RIR_TREF:
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JTY, &j->gens, &no_locals, t->inner, NULL, NULL));
      return none;
    case RIR_TFN: {
      Vec tys = resolve_cons(heap, t->ret, &t->args);
      Vec more = resolve_tjobs(heap, &j->gens, &tys);
      resolve_front(heap, stack, &more);
      return none;
    }
  }
  return none;
}

/* C: one JEx step of walk; Some = the refusal text. */
static MaybeStr resolve_walk_ex(Heap *heap, const ResolveScope *sc, const ResolveJob *j, Vec *stack) {
  MaybeStr none = {0};
  const RirEx *e = j->e;
  const Vec *gens = &j->gens;
  const Vec *locals = &j->locals;
  switch (e->kind) {
    case RIR_XVAR:
      if (erase_typed_has(locals, e->name) || erase_typed_has(resolve_fns_of(sc), e->name)) return none;
      return resolve_some3(heap, str_lit("unknown name `"), e->name, str_lit("`"));
    case RIR_XINT:
    case RIR_XBOOL:
      return none;
    case RIR_XCALL: {
      if (!(erase_typed_has(locals, e->func) || erase_typed_has(resolve_fns_of(sc), e->func))) {
        return resolve_some3(heap, str_lit("unknown function `"), e->func, str_lit("`"));
      }
      Vec args = resolve_ejobs(heap, gens, locals, &e->args);
      Vec targs = resolve_tjobs(heap, gens, &e->targs);
      resolve_front(heap, stack, &args);
      resolve_front(heap, stack, &targs);
      return none;
    }
    case RIR_XCTOR: {
      if (!resolve_has_ctor(resolve_fams_of(sc), e->family, e->ctor)) {
        Str head = resolve_cat3(heap, str_lit("unknown constructor `"), e->family, str_lit("::"));
        return resolve_some3(heap, head, e->ctor, str_lit("`"));
      }
      Vec args = resolve_ejobs(heap, gens, locals, &e->args);
      resolve_front(heap, stack, &args);
      return none;
    }
    case RIR_XMATCH: {
      Vec arms = resolve_arm_jobs(heap, gens, locals, &e->arms);
      resolve_front(heap, stack, &arms);
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->scrut, NULL));
      return none;
    }
    case RIR_XIF:
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->no, NULL));
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->yes, NULL));
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->cond, NULL));
      return none;
    case RIR_XCLOSURE: {
      Vec names = resolve_param_names(heap, &e->params);
      Vec inner = heap_vec_concat(heap, &names, locals);
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, &inner, NULL, e->body, NULL));
      Vec tys = erase_typed_param_tys(heap, &e->params);
      Vec more = resolve_tjobs(heap, gens, &tys);
      resolve_front(heap, stack, &more);
      return none;
    }
    case RIR_XBORROW:
    case RIR_XCLONE:
    case RIR_XBOX:
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->inner, NULL));
      return none;
    case RIR_XLET: {
      Vec inner = resolve_cons(heap, &e->name, locals);
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, &inner, NULL, e->body, NULL));
      resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, gens, locals, NULL, e->value, NULL));
      Vec tys = resolve_let_tys(heap, e->ty);
      Vec more = resolve_tjobs(heap, gens, &tys);
      resolve_front(heap, stack, &more);
      return none;
    }
  }
  return none;
}

/* C: one JArm step of walk; Some = the refusal text. */
static MaybeStr resolve_walk_arm(Heap *heap, const ResolveScope *sc, const ResolveJob *j, Vec *stack) {
  MaybeStr none = {0};
  const RirPat *pat = &j->arm->pat;
  if (!resolve_has_ctor(resolve_fams_of(sc), pat->family, pat->ctor)) {
    Str head = resolve_cat3(heap, str_lit("unknown constructor `"), pat->family, str_lit("::"));
    return resolve_some3(heap, head, pat->ctor, str_lit("`"));
  }
  Vec names = resolve_bind_names(heap, &pat->binds);
  Vec inner = heap_vec_concat(heap, &names, &j->locals);
  resolve_front_one(heap, stack, resolve_job(heap, RESOLVE_JEX, &j->gens, &inner, NULL, j->arm->body, NULL));
  return none;
}

/* walk(sc, js): the first name that does not resolve, as the text of a refusal. The Bend
 * recursion over the job list is a loop over the stack of resolve_front (D-216b). */
static MaybeStr resolve_walk(Heap *heap, const ResolveScope *sc, const Vec *js) {
  MaybeStr none = {0};
  Vec stack = vec_new(heap->arena);
  resolve_front(heap, &stack, js);
  while (stack.len > 0 && !heap->oom) {
    VecGet g = vec_last(&stack);
    if (g.status != VEC_GET_OK) return none;
    stack.len -= 1;
    const ResolveJob *j = g.value;
    MaybeStr m = none;
    switch (j->kind) {
      case RESOLVE_JTY:
        m = resolve_walk_ty(heap, sc, j, &stack);
        break;
      case RESOLVE_JEX:
        m = resolve_walk_ex(heap, sc, j, &stack);
        break;
      case RESOLVE_JARM:
        m = resolve_walk_arm(heap, sc, j, &stack);
        break;
    }
    if (m.some) return m;
  }
  return none;
}

/* item_jobs(p): no job for a `use` line or a unit struct; the variant field types of an enum; the
 * return type, the parameter types and the body of a fn. */
static Vec resolve_item_jobs(Heap *heap, const ResolvePlaced *p) {
  const RirItem *it = p->item;
  switch (it->kind) {
    case RIR_IUSE:
      return vec_new(heap->arena);
    case RIR_IENUM: {
      Vec tys = rust_out_variant_tys(heap, &it->variants);
      return resolve_tjobs(heap, &it->generics, &tys);
    }
    case RIR_IUNIT:
      return vec_new(heap->arena);
    case RIR_IFN: {
      Vec ptys = erase_typed_param_tys(heap, &it->params);
      Vec tys = resolve_cons(heap, it->ret, &ptys);
      Vec out = resolve_tjobs(heap, &it->generics, &tys);
      Vec locals = resolve_param_names(heap, &it->params);
      resolve_push_job(heap, &out, resolve_job(heap, RESOLVE_JEX, &it->generics, &locals, NULL, it->body, NULL));
      return out;
    }
  }
  return vec_new(heap->arena);
}

/* place_pos(p) */
static AstPos resolve_place_pos(const ResolvePlaced *p) {
  return p->pos;
}

/* problem_at(module, pos, m, later): Some{what} = Fail{Bad{module, pos, what}}; None = later. */
static ResolveR resolve_problem_at(Str module, AstPos pos, MaybeStr m, ResolveR later) {
  if (m.some) return resolve_fail(module, pos, m.value);
  return later;
}

/* after_space(s): the text of a name problem after its first word: "unknown function `f`" gives
 * "function `f`"; no space gives "". */
static Str resolve_after_space(Str s) {
  for (size_t i = 0; i < s.len; i++) {
    if (rust_out_code_is((unsigned char)s.data[i], 32)) return str_n(s.data + i + 1, s.len - i - 1);
  }
  return str_empty();
}

/* early_at(module, pos, m, later): a name that the module has, but not at this item or before it.
 * The mech text keeps the item order, and a mech declaration sees only the earlier declarations and
 * itself. */
static ResolveR resolve_early_at(Heap *heap, Str module, AstPos pos, MaybeStr m, ResolveR later) {
  if (m.some) {
    Str what = resolve_cat3(heap, str_lit("use of the "), resolve_after_space(m.value),
                            str_lit(" before its declaration (mutual recursion or a forward reference)"));
    return resolve_fail(module, pos, what);
  }
  return later;
}

/* check_items(module, sc, seen, ps): `sc` = the used modules and all items of the module; `seen` =
 * the used modules and the items before `ps` (ResolvePlaced pointers). C: a loop that stops at the
 * first refusal. The Bend call is strict, but its result is the refusal of the first item that has
 * one (problem_at before early_at), so the two agree. */
static ResolveR resolve_check_items(Heap *heap, Str module, const ResolveScope *sc, ResolveScope seen, const Vec *ps) {
  ResolveScope now = seen;
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    if (g.status != VEC_GET_OK) return resolve_fail(module, ast_no_pos(heap), str_empty());
    const ResolvePlaced *p = g.value;
    now = resolve_item_scope(heap, p, now);
    Vec jobs = resolve_item_jobs(heap, p);
    ResolveR all = resolve_problem_at(module, resolve_place_pos(p), resolve_walk(heap, sc, &jobs), resolve_done_flag(true));
    if (!all.ok) return all;
    ResolveR early = resolve_early_at(heap, module, resolve_place_pos(p), resolve_walk(heap, &now, &jobs), resolve_done_flag(true));
    if (!early.ok) return early;
  }
  return resolve_done_flag(true);
}

/* ---- modules ---- */

/* UseAt.UseAt: the position of a `use crate::<module>::*;` line and its module. */
typedef struct {
  AstPos pos;
  Str module;
} ResolveUseAt;

/* uses_of(ps): the UseAt of each `use` item (ResolvePlaced pointers), in order, as ResolveUseAt
 * pointers. */
static Vec resolve_uses_of(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    if (g.status != VEC_GET_OK) return out;
    const ResolvePlaced *p = g.value;
    switch (p->item->kind) {
      case RIR_IUSE: {
        ResolveUseAt *u = heap_alloc(heap, sizeof(ResolveUseAt));
        if (!u) return out;
        u->pos = p->pos;
        u->module = p->item->module;
        resolve_push(heap, &out, u);
        break;
      }
      case RIR_IENUM:
      case RIR_IUNIT:
      case RIR_IFN:
        break;
    }
  }
  return out;
}

/* non_nat(us): the UseAt pointers whose module is not `nat`, in order. */
static Vec resolve_non_nat(Heap *heap, const Vec *us) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < us->len; i++) {
    VecGet g = vec_get(us, i);
    const ResolveUseAt *u = g.value;
    if (g.status == VEC_GET_OK && !str_eq(u->module, str_lit("nat"))) resolve_push(heap, &out, u);
  }
  return out;
}

/* mod_names(ms): the name of each Mod (ResolveMod pointers) as Str pointers, in order. */
static Vec resolve_mod_names(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status == VEC_GET_OK) resolve_push(heap, &out, &((const ResolveMod *)g.value)->name);
  }
  return out;
}

/* find_mod(ms, name): the first Mod named `name`; C: NULL = None. */
static const ResolveMod *resolve_find_mod(const Vec *ms, Str name) {
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    const ResolveMod *m = g.value;
    if (g.status == VEC_GET_OK && str_eq(m->name, name)) return m;
  }
  return NULL;
}

/* mod_scope(found, sc): the items of the found module added to sc; none = sc. */
static ResolveScope resolve_mod_scope(Heap *heap, const ResolveMod *found, ResolveScope sc) {
  if (found) return resolve_items_scope(heap, &found->items, sc);
  return sc;
}

/* add_mod(m, found, sc): `nat` adds the nat scope (CD7), any other module its items. */
static ResolveScope resolve_add_mod(Heap *heap, Str m, const ResolveMod *found, ResolveScope sc) {
  if (str_eq(m, str_lit("nat"))) return resolve_nat_scope(heap, sc);
  return resolve_mod_scope(heap, found, sc);
}

/* use_scope(ms, us, sc): the `use` lines give the visible modules (each was checked by
 * `known_uses`); first to last. */
static ResolveScope resolve_use_scope(Heap *heap, const Vec *ms, const Vec *us, ResolveScope sc) {
  ResolveScope out = sc;
  for (size_t i = 0; i < us->len; i++) {
    VecGet g = vec_get(us, i);
    const ResolveUseAt *u = g.value;
    if (g.status == VEC_GET_OK) out = resolve_add_mod(heap, u->module, resolve_find_mod(ms, u->module), out);
  }
  return out;
}

/* check_mods(all, ms): check_items of each module, first to last; the first refusal stops. */
static ResolveR resolve_check_mods(Heap *heap, const Vec *all, const Vec *ms) {
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const ResolveMod *m = g.value;
    Vec uses = resolve_uses_of(heap, &m->items);
    ResolveScope sc = resolve_use_scope(heap, all, &uses, resolve_items_scope(heap, &m->items, resolve_empty(heap)));
    ResolveScope seen = resolve_use_scope(heap, all, &uses, resolve_empty(heap));
    ResolveR ok = resolve_check_items(heap, m->name, &sc, seen, &m->items);
    if (!ok.ok) return ok;
  }
  return resolve_done_flag(true);
}

/* known_uses(names, nat, module, us): each `use` names a module of lib.rs, or `nat` when the crate
 * has it; the first other one is refused. */
static ResolveR resolve_known_uses(Heap *heap, const Vec *names, bool nat, Str module, const Vec *us) {
  for (size_t i = 0; i < us->len; i++) {
    VecGet g = vec_get(us, i);
    if (g.status != VEC_GET_OK) return resolve_fail(module, ast_no_pos(heap), str_empty());
    const ResolveUseAt *u = g.value;
    bool known = erase_typed_has(names, u->module) || (str_eq(u->module, str_lit("nat")) && nat);
    if (!known) {
      Str what = resolve_cat3(heap, str_lit("`use crate::"), u->module,
                              str_lit("::*;` of a module that lib.rs does not declare"));
      return resolve_fail(module, u->pos, what);
    }
  }
  return resolve_done_flag(true);
}

/* known_mods(names, nat, ms): known_uses of each module, first to last. */
static ResolveR resolve_known_mods(Heap *heap, const Vec *names, bool nat, const Vec *ms) {
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status != VEC_GET_OK) return resolve_fail(str_lit("lib"), ast_no_pos(heap), str_empty());
    const ResolveMod *m = g.value;
    Vec uses = resolve_uses_of(heap, &m->items);
    ResolveR ok = resolve_known_uses(heap, names, nat, m->name, &uses);
    if (!ok.ok) return ok;
  }
  return resolve_done_flag(true);
}

/* ---- order (CD3) ---- */

/* Dep.Dep: a module and its `use` lines other than `nat` (ResolveUseAt pointers). */
typedef struct {
  Str name;
  Vec uses;
} ResolveDep;

/* deps_of(ms): the Dep of each Mod, in order, as ResolveDep pointers. */
static Vec resolve_deps_of(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status != VEC_GET_OK) return out;
    const ResolveMod *m = g.value;
    ResolveDep *d = heap_alloc(heap, sizeof(ResolveDep));
    if (!d) return out;
    Vec uses = resolve_uses_of(heap, &m->items);
    d->name = m->name;
    d->uses = resolve_non_nat(heap, &uses);
    resolve_push(heap, &out, d);
  }
  return out;
}

/* all_in(acc, us): every `use` names a module of acc. */
static bool resolve_all_in(const Vec *acc, const Vec *us) {
  for (size_t i = 0; i < us->len; i++) {
    VecGet g = vec_get(us, i);
    const ResolveUseAt *u = g.value;
    if (g.status != VEC_GET_OK || !erase_typed_has(acc, u->module)) return false;
  }
  return true;
}

/* ready(acc, ds): the modules whose `use` lines name only modules in acc, in the order of ds, as
 * Str pointers. */
static Vec resolve_ready(Heap *heap, const Vec *acc, const Vec *ds) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ds->len; i++) {
    VecGet g = vec_get(ds, i);
    const ResolveDep *d = g.value;
    if (g.status == VEC_GET_OK && resolve_all_in(acc, &d->uses)) resolve_push(heap, &out, &d->name);
  }
  return out;
}

/* without(ds, r): the Deps whose name is not r, in order. */
static Vec resolve_without(Heap *heap, const Vec *ds, Str r) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ds->len; i++) {
    VecGet g = vec_get(ds, i);
    const ResolveDep *d = g.value;
    if (g.status == VEC_GET_OK && !str_eq(d->name, r)) resolve_push(heap, &out, d);
  }
  return out;
}

/* stuck(acc, module, us): the first `use` of a module that is not in acc; none = the module. */
static ResolveBad resolve_stuck(Heap *heap, const Vec *acc, Str module, const Vec *us) {
  ResolveBad b;
  b.module = module;
  for (size_t i = 0; i < us->len; i++) {
    VecGet g = vec_get(us, i);
    const ResolveUseAt *u = g.value;
    if (g.status == VEC_GET_OK && !erase_typed_has(acc, u->module)) {
      b.pos = u->pos;
      b.what = resolve_cat3(heap, str_lit("`use crate::"), u->module, str_lit("::*;` in a module cycle (CD3)"));
      return b;
    }
  }
  b.pos = ast_no_pos(heap);
  b.what = str_lit("module cycle (CD3)");
  return b;
}

/* finish(pending, acc): no module left = the order; a module left = a cycle at the first one.
 * D-217a: the C acc is in push order (the Bend list reversed), so the Bend List.reverse(acc) is the
 * C acc itself; acc is read only by Et.has, which does not see the order. */
static ResolveR resolve_finish(Heap *heap, const Vec *pending, const Vec *acc) {
  VecGet g = vec_get(pending, 0);
  if (g.status == VEC_GET_OK) {
    const ResolveDep *d = g.value;
    ResolveR r = {0};
    r.ok = false;
    r.bad = resolve_stuck(heap, acc, d->name, &d->uses);
    return r;
  }
  return resolve_done_list(*acc);
}

/* push(r, acc): Bend `r <> acc`. C name resolve_push_acc (resolve_push is the C helper); the C acc
 * is in push order (D-217a), so r goes at the end of a new Vec. */
static Vec resolve_push_acc(Heap *heap, const Str *r, const Vec *acc) {
  Vec one = vec_new(heap->arena);
  resolve_push(heap, &one, r);
  return heap_vec_concat(heap, acc, &one);
}

/* order(pending, acc, rs): Kahn's order: each step takes the least ready module by byte order (the
 * order of lib.rs). `rs`: the ready modules, sorted. No ready module and a module left: a cycle.
 * C: a loop; the Bend recursion uses only the head of rs. */
static ResolveR resolve_order(Heap *heap, Vec pending, Vec acc, Vec rs) {
  VecGet g = vec_get(&rs, 0);
  while (g.status == VEC_GET_OK) {
    const Str *r = g.value;
    Vec left = resolve_without(heap, &pending, *r);
    Vec now = resolve_push_acc(heap, r, &acc);
    Vec rd = resolve_ready(heap, &now, &left);
    pending = left;
    acc = now;
    rs = rust_out_sorted(heap, &rd);
    g = vec_get(&rs, 0);
  }
  return resolve_finish(heap, &pending, &acc);
}

/* start(ds) */
static ResolveR resolve_start(Heap *heap, const Vec *ds) {
  Vec none = vec_new(heap->arena);
  Vec rd = resolve_ready(heap, &none, ds);
  return resolve_order(heap, *ds, none, rust_out_sorted(heap, &rd));
}

/* found_list(m): [x] for Some{x}, [] for None (C: NULL). */
static Vec resolve_found_list(Heap *heap, const ResolveMod *m) {
  Vec out = vec_new(heap->arena);
  if (m) resolve_push(heap, &out, m);
  return out;
}

/* in_order(names, ms): the Mod of each name, in the order of names. */
static Vec resolve_in_order(Heap *heap, const Vec *names, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    VecGet g = vec_get(names, i);
    if (g.status == VEC_GET_OK) {
      Vec one = resolve_found_list(heap, resolve_find_mod(ms, *(const Str *)g.value));
      out = heap_vec_concat(heap, &out, &one);
    }
  }
  return out;
}

/* ---- crate ---- */

/* C: Done{Out{ms, rows}} */
static ResolveR resolve_done_out(const Vec *ms, const Vec *rows) {
  ResolveR r = {0};
  r.ok = true;
  r.out.mods = *ms;
  r.out.rows = *rows;
  return r;
}

/* collided(ms, rows): the Rust name and mech name collision checks, then the body checks. */
static ResolveR resolve_collided(Heap *heap, const Vec *ms, const Vec *rows) {
  Vec nat = rust_out_nat_names(heap);
  ResolveR a = resolve_rust_dup(heap, &nat, rows);
  if (!a.ok) return a;
  Vec reserved = resolve_mech_reserved(heap);
  ResolveR b = resolve_mech_dup(heap, &reserved, rows);
  if (!b.ok) return b;
  ResolveR c = resolve_check_mods(heap, ms, ms);
  if (!c.ok) return c;
  return resolve_done_out(ms, rows);
}

/* ordered(ms) */
static ResolveR resolve_ordered(Heap *heap, const Vec *ms) {
  ResolveR rows = resolve_all_rows(heap, ms);
  if (!rows.ok) return rows;
  return resolve_collided(heap, ms, &rows.list);
}

/* checked(nat, ms) */
static ResolveR resolve_checked(Heap *heap, bool nat, const Vec *ms) {
  Vec mod_names = resolve_mod_names(heap, ms);
  ResolveR ok = resolve_known_mods(heap, &mod_names, nat, ms);
  if (!ok.ok) return ok;
  Vec deps = resolve_deps_of(heap, ms);
  ResolveR names = resolve_start(heap, &deps);
  if (!names.ok) return names;
  Vec in_order = resolve_in_order(heap, &names.list, ms);
  return resolve_ordered(heap, &in_order);
}

/* crate_of(names, srcs) */
static ResolveR resolve_crate_of(Heap *heap, const Vec *names, const Vec *srcs) {
  ResolveR nat = resolve_nat_need(heap, erase_typed_has(names, str_lit("nat")), resolve_text_of(srcs, str_lit("nat")));
  if (!nat.ok) return nat;
  Vec rest = resolve_drop_nat(heap, names);
  ResolveR ms = resolve_load(heap, &rest, srcs);
  if (!ms.ok) return ms;
  return resolve_checked(heap, nat.flag, &ms.list);
}

/* resolve(lib, srcs): `lib` = the text of src/lib.rs; `srcs` = the text of each module file
 * (ResolveSrc pointers). */
ResolveOutResult resolve_resolve(Heap *heap, Str lib, const Vec *srcs) {
  ResolveOutResult out = {0};
  RustSrcFileResult f = rust_parser_file(heap, lib);
  ResolveNamesResult names = resolve_lib_mods(heap, &f);
  if (!names.ok) {
    out.ok = false;
    out.bad = names.bad;
    return out;
  }
  ResolveR r = resolve_crate_of(heap, &names.names, srcs);
  out.ok = r.ok;
  out.bad = r.bad;
  out.out = r.out;
  return out;
}

/* ---- text ---- */

/* kind_text(k) */
static Str resolve_kind_text(ResolveRk k) {
  switch (k) {
    case RESOLVE_RK_ENUM:
      return str_lit("enum");
    case RESOLVE_RK_VARIANT:
      return str_lit("variant");
    case RESOLVE_RK_STRUCT:
      return str_lit("struct");
    case RESOLVE_RK_CTOR:
      return str_lit("ctor");
    case RESOLVE_RK_FN:
      return str_lit("fn");
  }
  return str_empty();
}

/* C: append s; a failed append sets heap->oom. */
static void resolve_buf_push(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* rows_text(rows): `<module> <kind> <rust name> -> <mech name>` and a newline for each row. */
static Str resolve_rows_text(Heap *heap, const Vec *rows) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    VecGet g = vec_get(rows, i);
    if (g.status != VEC_GET_OK) return strbuf_str(&buf);
    const ResolveRow *row = g.value;
    resolve_buf_push(heap, &buf, row->module);
    resolve_buf_push(heap, &buf, str_lit(" "));
    resolve_buf_push(heap, &buf, resolve_kind_text(row->kind));
    resolve_buf_push(heap, &buf, str_lit(" "));
    resolve_buf_push(heap, &buf, row->rust);
    resolve_buf_push(heap, &buf, str_lit(" -> "));
    resolve_buf_push(heap, &buf, row->mech);
    resolve_buf_push(heap, &buf, str_lit("\n"));
  }
  return strbuf_str(&buf);
}

/* manifest_text(ms): `manifest <module> <file>` and a newline for each module. */
static Str resolve_manifest_text(Heap *heap, const Vec *ms) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    VecGet g = vec_get(ms, i);
    if (g.status != VEC_GET_OK) return strbuf_str(&buf);
    const ResolveMod *m = g.value;
    resolve_buf_push(heap, &buf, str_lit("manifest "));
    resolve_buf_push(heap, &buf, m->name);
    resolve_buf_push(heap, &buf, str_lit(" "));
    resolve_buf_push(heap, &buf, m->file);
    resolve_buf_push(heap, &buf, str_lit("\n"));
  }
  return strbuf_str(&buf);
}

/* table_text(o): one line for each row, `<module> <kind> <rust name> -> <mech name>`, then one
 * `manifest <module> <file>` line for each module, in manifest order. */
Str resolve_table_text(Heap *heap, const ResolveOut *o) {
  return heap_str_cat(heap, resolve_rows_text(heap, &o->rows), resolve_manifest_text(heap, &o->mods));
}
