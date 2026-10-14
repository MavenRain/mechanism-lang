/* Port of bend2/rust/lift.bend: lifts the Rust AST of the frontend back to the erased program IR.
 * It is the inverse of each lowering function of emit.bend, so emit_text(lift(f)) is the text of f
 * for each file that emit.bend writes. A shape outside the image of emit.bend is a refusal with the
 * position of the nearest construct that has a Meta (CD13). The definitions keep the Bend names
 * (prefix lift_) and the Bend order; the C only helpers stand just before their first use.
 *
 * D-211a: each Bend Result<Refusal, A> is the one C only struct LiftR (ok, refusal, and one field
 * per A: flag = Bool, list = any List as a Vec of pointers in source order, ty = R.Ty; later parts
 * add the fields of their A). A Bend List<TK> is a Vec of LiftTK pointers. The Bend `tks` recurses
 * on the head (its nested kinds) before the rest, so the C loop that visits the heads in order and
 * recurses into each head meets the same first TKBad. */

#include "rust.h"

/* show(r): `<line>:<col>: <construct>`; the caller that knows the file adds `<file>:`. */
Str lift_show(Heap *heap, const LiftRefusal *r) {
  return heap_str_cat(heap, heap_str_cat(heap, ast_show_pos(heap, r->pos), str_lit(": ")), r->what);
}

/* Job (defined with the expressions below). */
typedef struct LiftJob LiftJob;

/* C: Result<Refusal, A> for each A of this file (D-211a). */
typedef struct {
  bool ok;
  LiftRefusal refusal; /* !ok */
  bool flag;           /* A = Bool */
  Vec list;            /* A = List<..> (pointers, source order) */
  const RirTy *ty;     /* A = R.Ty */
  RirPat pat;          /* A = R.Pat */
  const LiftJob *job;  /* A = Job */
  RirMaybeTy mty;      /* A = Maybe<R.Ty> */
  const RirEx *ex;     /* C: the one R.Ex of walk on one job */
  const RirItem *item; /* A = R.Item */
  Str str;             /* A = String */
} LiftR;

/* C: Fail{Refusal{pos, what}} */
static LiftR lift_fail(AstPos pos, Str what) {
  LiftR r = {0};
  r.ok = false;
  r.refusal.pos = pos;
  r.refusal.what = what;
  return r;
}

/* C: Done{b} */
static LiftR lift_done_flag(bool b) {
  LiftR r = {0};
  r.ok = true;
  r.flag = b;
  return r;
}

/* C: Done{list} */
static LiftR lift_done_list(Vec list) {
  LiftR r = {0};
  r.ok = true;
  r.list = list;
  return r;
}

/* C: Done{t} */
static LiftR lift_done_ty(const RirTy *t) {
  LiftR r = {0};
  r.ok = true;
  r.ty = t;
  return r;
}

/* C: push one pointer; a failed push sets heap->oom. */
static void lift_push(Heap *heap, Vec *v, const void *x) {
  if (!vec_push(v, (void *)x)) heap->oom = true;
}

/* C: [x] */
static Vec lift_list1(Heap *heap, const void *x) {
  Vec out = vec_new(heap->arena);
  lift_push(heap, &out, x);
  return out;
}

/* ---- shared checks ---- */

/* plain(m): a construct with no docs and no attributes. */
static LiftR lift_plain(const AstMeta *m) {
  if (m->docs.len == 0 && m->attrs.len == 0) return lift_done_flag(true);
  if (m->docs.len == 0) return lift_fail(m->pos, str_lit("attribute"));
  return lift_fail(m->pos, str_lit("doc comment"));
}

/* strs_eq(xs, ys): xs and ys are lists of Str pointers. */
static bool lift_strs_eq(const Vec *xs, const Vec *ys) {
  bool eq = xs->len == ys->len;
  for (size_t i = 0; eq && i < xs->len; i++) {
    VecGet x = vec_get(xs, i);
    VecGet y = vec_get(ys, i);
    eq = x.status == VEC_GET_OK && y.status == VEC_GET_OK && str_eq(*(const Str *)x.value, *(const Str *)y.value);
  }
  return eq;
}

/* attr_args(ats): the arguments of the first attribute when it has a list, else []. */
static Vec lift_attr_args(Heap *heap, const Vec *ats) {
  VecGet g = vec_get(ats, 0);
  if (g.status == VEC_GET_OK && ((const AstAttr *)g.value)->args.some) return ((const AstAttr *)g.value)->args.value;
  return vec_new(heap->arena);
}

/* derived(m): the copy flag of an item whose only attribute is one of the two derive lists of the
 * emitter (emit_derive). */
static LiftR lift_derived(Heap *heap, const AstMeta *m) {
  if (m->docs.len != 0) return lift_fail(m->pos, str_lit("doc comment"));
  VecGet g = vec_get(&m->attrs, 0);
  if (m->attrs.len != 1 || g.status != VEC_GET_OK || !((const AstAttr *)g.value)->args.some)
    return lift_fail(m->pos, str_lit("attributes other than one `derive` list"));
  const AstAttr *a = g.value;
  bool is_derive = str_eq(a->name, str_lit("derive"));
  Vec copy_attrs = emit_derive(heap, true);
  Vec move_attrs = emit_derive(heap, false);
  Vec copy_args = lift_attr_args(heap, &copy_attrs);
  Vec move_args = lift_attr_args(heap, &move_attrs);
  if (is_derive && lift_strs_eq(&a->args.value, &copy_args)) return lift_done_flag(true);
  if (is_derive && lift_strs_eq(&a->args.value, &move_args)) return lift_done_flag(false);
  return lift_fail(m->pos, str_lit("derive list other than the emitter's"));
}

/* is_pub(pos, vis) */
static LiftR lift_is_pub(AstPos pos, AstVis vis) {
  LiftR out = lift_done_flag(true);
  switch (vis) {
  case AST_PUB:
    out = lift_done_flag(true);
    break;
  case AST_PRIV:
    out = lift_fail(pos, str_lit("item that is not `pub`"));
    break;
  case AST_PUB_CRATE:
    out = lift_fail(pos, str_lit("`pub(crate)` item"));
    break;
  }
  return out;
}

/* is_priv(pos, vis) */
static LiftR lift_is_priv(AstPos pos, AstVis vis) {
  LiftR out = lift_done_flag(true);
  switch (vis) {
  case AST_PRIV:
    out = lift_done_flag(true);
    break;
  case AST_PUB:
    out = lift_fail(pos, str_lit("`pub` other than on an item"));
    break;
  case AST_PUB_CRATE:
    out = lift_fail(pos, str_lit("`pub(crate)` other than on an item"));
    break;
  }
  return out;
}

/* ---- types ---- */

/* TK: the head of one type, the string tests of `tks`. */
typedef enum { LIFT_TKBOOL, LIFT_TKCON, LIFT_TKBOX, LIFT_TKREF, LIFT_TKFN, LIFT_TKBAD } LiftTKKind;
typedef struct {
  LiftTKKind kind;
  Str name;           /* TKCON */
  Vec args;           /* TKCON, TKFN (AstTy pointers) */
  const AstTy *inner; /* TKBOX, TKREF */
  const AstTy *ret;   /* TKFN */
  Str what;           /* TKBAD */
} LiftTK;

/* C: TKBad{what} */
static LiftTK lift_tk_bad(Str what) {
  LiftTK k = {0};
  k.kind = LIFT_TKBAD;
  k.what = what;
  return k;
}

/* C: TKBool{}, TKBox{i}, TKRef{i} */
static LiftTK lift_tk_inner(LiftTKKind kind, const AstTy *inner) {
  LiftTK k = {0};
  k.kind = kind;
  k.inner = inner;
  return k;
}

/* bool_ty(args) */
static LiftTK lift_bool_ty(const Vec *args) {
  if (args->len == 0) return lift_tk_inner(LIFT_TKBOOL, NULL);
  return lift_tk_bad(str_lit("`bool` with generic arguments"));
}

/* box_ty(args) */
static LiftTK lift_box_ty(const Vec *args) {
  VecGet g = vec_get(args, 0);
  if (args->len == 1 && g.status == VEC_GET_OK) return lift_tk_inner(LIFT_TKBOX, g.value);
  return lift_tk_bad(str_lit("`Box` with other than one generic argument"));
}

/* C: the TPath case of ty_kind. */
static LiftTK lift_ty_path(const Vec *segs) {
  VecGet g = vec_get(segs, 0);
  if (segs->len != 1 || g.status != VEC_GET_OK) return lift_tk_bad(str_lit("type path with more than one segment"));
  const AstSeg *s = g.value;
  if (str_eq(s->name, str_lit("bool"))) return lift_bool_ty(&s->args);
  if (str_eq(s->name, str_lit("Box"))) return lift_box_ty(&s->args);
  LiftTK k = {0};
  k.kind = LIFT_TKCON;
  k.name = s->name;
  k.args = s->args;
  return k;
}

/* C: the TImpl case of ty_kind. */
static LiftTK lift_ty_impl(Heap *heap, const Vec *bounds) {
  VecGet g = vec_get(bounds, 0);
  const AstTy *b = NULL;
  if (g.status == VEC_GET_OK) b = g.value;
  if (bounds->len != 1 || b == NULL || b->kind != AST_TFN_SUGAR || !b->ret.some)
    return lift_tk_bad(str_lit("`impl` type other than `impl Fn(..) -> T`"));
  if (!str_eq(b->name, str_lit("Fn")))
    return lift_tk_bad(heap_str_cat(heap, heap_str_cat(heap, str_lit("`impl "), b->name), str_lit("`")));
  LiftTK k = {0};
  k.kind = LIFT_TKFN;
  k.args = b->args;
  k.ret = b->ret.value;
  return k;
}

/* ty_kind(t) */
static LiftTK lift_ty_kind(Heap *heap, const AstTy *t) {
  LiftTK out = lift_tk_bad(str_empty());
  switch (t->kind) {
  case AST_TPATH:
    out = lift_ty_path(&t->segs);
    break;
  case AST_TREF:
    out = lift_tk_inner(LIFT_TKREF, t->inner);
    break;
  case AST_TIMPL:
    out = lift_ty_impl(heap, &t->bounds);
    break;
  case AST_TTUPLE:
    out = lift_tk_bad(str_lit("tuple type"));
    break;
  case AST_TSLICE:
    out = lift_tk_bad(str_lit("slice type"));
    break;
  case AST_TINFER:
    out = lift_tk_bad(str_lit("type `_`"));
    break;
  case AST_TFN_SUGAR:
    out = lift_tk_bad(heap_str_cat(heap, heap_str_cat(heap, str_lit("`"), t->name), str_lit("(..)` type")));
    break;
  }
  return out;
}

static const LiftTK lift_tk_placeholder = {LIFT_TKBAD};

/* C: a heap copy of k, for the lists of LiftTK pointers. */
static const LiftTK *lift_tk_box(Heap *heap, LiftTK k) {
  LiftTK *box = heap_alloc(heap, sizeof(LiftTK));
  if (!box) return &lift_tk_placeholder;
  *box = k;
  return box;
}

/* ty_kinds(ts) */
static Vec lift_ty_kinds(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    VecGet g = vec_get(ts, i);
    if (g.status == VEC_GET_OK) lift_push(heap, &out, lift_tk_box(heap, lift_ty_kind(heap, g.value)));
  }
  return out;
}

/* one_ty(xs): the head of xs, TBool when xs is empty. */
static const RirTy *lift_one_ty(Heap *heap, const Vec *xs) {
  VecGet g = vec_get(xs, 0);
  if (g.status == VEC_GET_OK) return g.value;
  return rir_tbool(heap);
}

static LiftR lift_tks(Heap *heap, AstPos pos, const Vec *xs);

/* C: tks on one TK (the Bend head case): ys <- tks(nested kinds), then the R.Ty of the head. */
static LiftR lift_tk(Heap *heap, AstPos pos, const LiftTK *k) {
  LiftR out = lift_fail(pos, k->what);
  switch (k->kind) {
  case LIFT_TKBOOL:
    out = lift_done_ty(rir_tbool(heap));
    break;
  case LIFT_TKCON: {
    Vec kinds = lift_ty_kinds(heap, &k->args);
    LiftR ys = lift_tks(heap, pos, &kinds);
    if (ys.ok) out = lift_done_ty(rir_tcon(heap, k->name, ys.list));
    if (!ys.ok) out = ys;
    break;
  }
  case LIFT_TKBOX: {
    Vec one = lift_list1(heap, lift_tk_box(heap, lift_ty_kind(heap, k->inner)));
    LiftR ys = lift_tks(heap, pos, &one);
    if (ys.ok) out = lift_done_ty(rir_tbox(heap, lift_one_ty(heap, &ys.list)));
    if (!ys.ok) out = ys;
    break;
  }
  case LIFT_TKREF: {
    Vec one = lift_list1(heap, lift_tk_box(heap, lift_ty_kind(heap, k->inner)));
    LiftR ys = lift_tks(heap, pos, &one);
    if (ys.ok) out = lift_done_ty(rir_tref(heap, lift_one_ty(heap, &ys.list)));
    if (!ys.ok) out = ys;
    break;
  }
  case LIFT_TKFN: {
    Vec kinds = lift_ty_kinds(heap, &k->args);
    LiftR ys = lift_tks(heap, pos, &kinds);
    Vec one = lift_list1(heap, lift_tk_box(heap, lift_ty_kind(heap, k->ret)));
    LiftR rs = ys;
    if (ys.ok) rs = lift_tks(heap, pos, &one);
    if (rs.ok) out = lift_done_ty(rir_tfn(heap, ys.list, lift_one_ty(heap, &rs.list)));
    if (!rs.ok) out = rs;
    break;
  }
  case LIFT_TKBAD:
    out = lift_fail(pos, k->what);
    break;
  }
  return out;
}

/* tks(pos, xs) */
static LiftR lift_tks(Heap *heap, AstPos pos, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    LiftR one = lift_fail(pos, str_empty());
    if (g.status == VEC_GET_OK) one = lift_tk(heap, pos, g.value);
    if (!one.ok) return one;
    lift_push(heap, &out, one.ty);
  }
  return lift_done_list(out);
}

/* tys(pos, ts) */
static LiftR lift_tys(Heap *heap, AstPos pos, const Vec *ts) {
  Vec kinds = lift_ty_kinds(heap, ts);
  return lift_tks(heap, pos, &kinds);
}

/* ty(pos, t) */
static LiftR lift_ty(Heap *heap, AstPos pos, const AstTy *t) {
  Vec one = lift_list1(heap, t);
  LiftR ys = lift_tys(heap, pos, &one);
  if (!ys.ok) return ys;
  return lift_done_ty(lift_one_ty(heap, &ys.list));
}

/* ---- parameters and patterns ---- */

/* C: Done{p} */
static LiftR lift_done_pat(RirPat p) {
  LiftR r = {0};
  r.ok = true;
  r.pat = p;
  return r;
}

/* C: segs is a path of n segments, each with no generic arguments (the Bend patterns
 * `Con{A.Seg{x, Nil{}}, ..}`). */
static bool lift_bare_segs(const Vec *segs, size_t n) {
  bool ok = segs->len == n;
  for (size_t i = 0; ok && i < n; i++) {
    VecGet g = vec_get(segs, i);
    ok = g.status == VEC_GET_OK && ((const AstSeg *)g.value)->args.len == 0;
  }
  return ok;
}

/* C: the name of segment i of segs (empty when i is out of range). */
static Str lift_seg_name(const Vec *segs, size_t i) {
  VecGet g = vec_get(segs, i);
  if (g.status == VEC_GET_OK) return ((const AstSeg *)g.value)->name;
  return str_empty();
}

/* C: p is the pattern `name` (A.PPath{Con{A.Seg{name, Nil{}}, Nil{}}}). */
static bool lift_name_pat(const AstPat *p) {
  return p != NULL && p->kind == AST_PPATH && lift_bare_segs(&p->segs, 1);
}

/* params(pos, ps) */
static LiftR lift_params(Heap *heap, AstPos pos, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    const AstParam *p = NULL;
    if (g.status == VEC_GET_OK) p = g.value;
    if (p == NULL || p->kind != AST_PTYPED || !lift_name_pat(p->pat))
      return lift_fail(pos, str_lit("parameter other than `name: Type`"));
    LiftR rt = lift_ty(heap, pos, p->ty);
    if (!rt.ok) return rt;
    Str name = lift_seg_name(&p->pat->segs, 0);
    lift_push(heap, &out, rir_param(heap, name, rt.ty));
  }
  return lift_done_list(out);
}

/* cparams(pos, ps) */
static LiftR lift_cparams(Heap *heap, AstPos pos, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    const AstCParam *p = NULL;
    if (g.status == VEC_GET_OK) p = g.value;
    if (p == NULL || !lift_name_pat(p->pat) || !p->ty.some)
      return lift_fail(pos, str_lit("closure parameter other than `name: Type`"));
    LiftR rt = lift_ty(heap, pos, p->ty.value);
    if (!rt.ok) return rt;
    Str name = lift_seg_name(&p->pat->segs, 0);
    lift_push(heap, &out, rir_param(heap, name, rt.ty));
  }
  return lift_done_list(out);
}

/* binds(pos, ps) */
static LiftR lift_binds(Heap *heap, AstPos pos, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    VecGet g = vec_get(ps, i);
    const AstPat *p = NULL;
    if (g.status == VEC_GET_OK) p = g.value;
    bool is_name = lift_name_pat(p);
    bool is_wild = p != NULL && p->kind == AST_PWILD;
    if (!is_name && !is_wild) return lift_fail(pos, str_lit("constructor field pattern other than a name or `_`"));
    if (is_name) lift_push(heap, &out, rir_bvar(heap, lift_seg_name(&p->segs, 0)));
    if (is_wild) lift_push(heap, &out, rir_bskip(heap));
  }
  return lift_done_list(out);
}

/* pat(pos, p) */
static LiftR lift_pat(Heap *heap, AstPos pos, const AstPat *p) {
  bool ctor_path = lift_bare_segs(&p->segs, 2);
  Str family = lift_seg_name(&p->segs, 0);
  Str ctor = lift_seg_name(&p->segs, 1);
  if (p->kind == AST_PPATH && ctor_path) {
    Vec none = vec_new(heap->arena);
    RirPat rp = rir_pctor(family, ctor, none);
    return lift_done_pat(rp);
  }
  if (p->kind == AST_PTUPLE_STRUCT && ctor_path && p->items.len > 0) {
    LiftR rbs = lift_binds(heap, pos, &p->items);
    if (!rbs.ok) return rbs;
    RirPat rp = rir_pctor(family, ctor, rbs.list);
    return lift_done_pat(rp);
  }
  return lift_fail(pos, str_lit("pattern other than `Family::Ctor` or `Family::Ctor(x, _)`"));
}

/* no_guard(pos, g) */
static LiftR lift_no_guard(AstPos pos, const AstMaybeNode *g) {
  if (g->some) return lift_fail(pos, str_lit("match guard"));
  return lift_done_flag(true);
}

static const RirPat lift_pat_placeholder = {0};

/* C: a heap copy of p, for the lists of R.Pat pointers. */
static const RirPat *lift_pat_box(Heap *heap, RirPat p) {
  RirPat *box = heap_alloc(heap, sizeof(RirPat));
  if (!box) return &lift_pat_placeholder;
  *box = p;
  return box;
}

/* arm_pats(xs): per arm, in order, plain(meta), no_guard, then pat. */
static LiftR lift_arm_pats(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) {
      const AstArm *a = g.value;
      LiftR ok = lift_plain(&a->meta);
      if (!ok.ok) return ok;
      AstPos at = ast_meta_pos(&a->meta);
      LiftR gd = lift_no_guard(at, &a->guard);
      if (!gd.ok) return gd;
      LiftR rp = lift_pat(heap, at, a->pat);
      if (!rp.ok) return rp;
      lift_push(heap, &out, lift_pat_box(heap, rp.pat));
    }
  }
  return lift_done_list(out);
}

/* ---- expressions ---- */

/* K: the head of one expression, the string tests of `walk`. KIF holds the statements of the two
 * branches. One fat struct; the comment on each field names the tags that use it. */
typedef enum {
  LIFT_KVAR,
  LIFT_KINT,
  LIFT_KBOOL,
  LIFT_KCALL,
  LIFT_KCTOR,
  LIFT_KMATCH,
  LIFT_KIF,
  LIFT_KCLOSURE,
  LIFT_KBORROW,
  LIFT_KCLONE,
  LIFT_KBOX,
  LIFT_KLET,
  LIFT_KBLOCK,
  LIFT_KBAD
} LiftKKind;
typedef struct {
  LiftKKind kind;
  Str name;             /* KVAR, KLET */
  Str text;             /* KINT */
  bool value;           /* KBOOL */
  Str func;             /* KCALL */
  Vec targs;            /* KCALL (AstTy pointers) */
  Vec args;             /* KCALL, KCTOR (AstNode pointers) */
  Str family;           /* KCTOR */
  Str ctor;             /* KCTOR */
  const AstNode *scrut; /* KMATCH */
  Vec arms;             /* KMATCH (AstArm pointers) */
  const AstNode *cond;  /* KIF */
  Vec yes;              /* KIF (AstStmt pointers) */
  Vec no;               /* KIF (AstStmt pointers) */
  Vec params;           /* KCLOSURE (AstCParam pointers) */
  const AstNode *body;  /* KCLOSURE */
  const AstNode *inner; /* KBORROW, KCLONE, KBOX */
  AstMaybeTy ty;        /* KLET */
  const AstNode *init;  /* KLET */
  Vec rest;             /* KLET (AstStmt pointers) */
  Vec stmts;            /* KBLOCK (AstStmt pointers) */
  Str what;             /* KBAD */
} LiftK;

/* C: KBad{what} */
static LiftK lift_k_bad(Str what) {
  LiftK k = {.kind = LIFT_KBAD, .what = what};
  return k;
}

/* C: KBorrow{i}, KClone{i}, KBox{i} */
static LiftK lift_k_inner(LiftKKind kind, const AstNode *inner) {
  LiftK k = {.kind = kind, .inner = inner};
  return k;
}

/* C: a ++ b ++ c */
static Str lift_cat3(Heap *heap, Str a, Str b, Str c) {
  Str ab = heap_str_cat(heap, a, b);
  return heap_str_cat(heap, ab, c);
}

/* box_kind(args) */
static LiftK lift_box_kind(const Vec *args) {
  VecGet g = vec_get(args, 0);
  if (args->len == 1 && g.status == VEC_GET_OK) return lift_k_inner(LIFT_KBOX, g.value);
  return lift_k_bad(str_lit("`Box::new` with other than one argument"));
}

/* ctor_kind(family, ctor, args) */
static LiftK lift_ctor_kind(Str family, Str ctor, const Vec *args) {
  if (args->len == 0) return lift_k_bad(str_lit("constructor call with no argument"));
  LiftK k = {.kind = LIFT_KCTOR, .family = family, .ctor = ctor, .args = *args};
  return k;
}

/* closure_kind(ps, r, body) */
static LiftK lift_closure_kind(const Vec *ps, const AstMaybeTy *r, const AstNode *body) {
  if (r->some) return lift_k_bad(str_lit("closure with a return type"));
  LiftK k = {.kind = LIFT_KCLOSURE, .params = *ps, .body = body};
  return k;
}

/* C: the EPath cases of kind. */
static LiftK lift_kind_path(Heap *heap, const Vec *segs) {
  Str first = lift_seg_name(segs, 0);
  Str second = lift_seg_name(segs, 1);
  if (lift_bare_segs(segs, 1)) {
    LiftK k = {.kind = LIFT_KVAR, .name = first};
    return k;
  }
  if (lift_bare_segs(segs, 2)) {
    Vec none = vec_new(heap->arena);
    LiftK k = {.kind = LIFT_KCTOR, .family = first, .ctor = second, .args = none};
    return k;
  }
  return lift_k_bad(str_lit("path other than `name` or `Family::Ctor`"));
}

/* C: the ELit cases of kind. */
static LiftK lift_kind_lit(const AstLit *lit) {
  LiftK out = lift_k_bad(str_lit("string or char literal"));
  switch (lit->kind) {
  case AST_LINT: {
    LiftK k = {.kind = LIFT_KINT, .text = lit->text};
    out = k;
    break;
  }
  case AST_LBOOL: {
    LiftK k = {.kind = LIFT_KBOOL, .value = lit->boolean};
    out = k;
    break;
  }
  case AST_LSTR:
    out = lift_k_bad(str_lit("string or char literal"));
    break;
  case AST_LCHAR:
    out = lift_k_bad(str_lit("string or char literal"));
    break;
  }
  return out;
}

/* C: the ECall cases of kind. */
static LiftK lift_kind_call(const AstNode *n) {
  const AstNode *f = n->func;
  if (f != NULL && f->kind == AST_EPATH && f->segs.len == 1) {
    VecGet g = vec_get(&f->segs, 0);
    if (g.status == VEC_GET_OK) {
      const AstSeg *s = g.value;
      LiftK k = {.kind = LIFT_KCALL, .func = s->name, .targs = s->args, .args = n->args};
      return k;
    }
  }
  if (f != NULL && f->kind == AST_EPATH && lift_bare_segs(&f->segs, 2)) {
    Str family = lift_seg_name(&f->segs, 0);
    Str ctor = lift_seg_name(&f->segs, 1);
    if (str_eq(family, str_lit("Box")) && str_eq(ctor, str_lit("new"))) return lift_box_kind(&n->args);
    return lift_ctor_kind(family, ctor, &n->args);
  }
  return lift_k_bad(str_lit("call of an expression other than a fn or constructor path"));
}

/* C: the EMethod cases of kind. */
static LiftK lift_kind_method(Heap *heap, const AstNode *n) {
  if (n->generics.len == 0 && n->args.len == 0) {
    if (str_eq(n->name, str_lit("clone"))) return lift_k_inner(LIFT_KCLONE, n->recv);
    return lift_k_bad(lift_cat3(heap, str_lit("method call `."), n->name, str_lit("()`")));
  }
  return lift_k_bad(lift_cat3(heap, str_lit("method call `."), n->name, str_lit("`")));
}

/* C: the EUnary case of kind. */
static LiftK lift_kind_unary(Heap *heap, const AstNode *n) {
  if (str_eq(n->op, str_lit("&"))) return lift_k_inner(LIFT_KBORROW, n->inner);
  return lift_k_bad(lift_cat3(heap, str_lit("unary operator `"), n->op, str_lit("`")));
}

/* C: the EClosure case of kind. */
static LiftK lift_kind_closure(const AstNode *n) {
  if (n->mv) return lift_k_bad(str_lit("`move` closure"));
  return lift_closure_kind(&n->params, &n->ret, n->body);
}

/* C: the EIf cases of kind (EIf.body = stmts). */
static LiftK lift_kind_if(const AstNode *n) {
  if (!n->els.some) return lift_k_bad(str_lit("`if` without `else`"));
  const AstNode *e = n->els.value;
  if (e != NULL && e->kind == AST_EBLOCK) {
    LiftK k = {.kind = LIFT_KIF, .cond = n->cond, .yes = n->stmts, .no = e->stmts};
    return k;
  }
  /* The Bend refusal text, as two adjacent literals so the source rule on the else-if keyword pair
   * does not count it. */
  return lift_k_bad(str_lit("`else" " if`"));
}

/* C: the EBlock cases of kind: a block that starts with a `let`. */
static LiftK lift_kind_block(const AstNode *n) {
  VecGet g = vec_get(&n->stmts, 0);
  if (g.status == VEC_GET_OK && ((const AstStmt *)g.value)->kind == AST_SLET) {
    LiftK k = {.kind = LIFT_KBLOCK, .stmts = n->stmts};
    return k;
  }
  return lift_k_bad(str_lit("block"));
}

/* kind(n) */
static LiftK lift_kind(Heap *heap, const AstNode *n) {
  LiftK out = lift_k_bad(str_lit("item in an expression"));
  switch (n->kind) {
  case AST_EPATH:
    out = lift_kind_path(heap, &n->segs);
    break;
  case AST_ELIT:
    out = lift_kind_lit(&n->lit);
    break;
  case AST_ECALL:
    out = lift_kind_call(n);
    break;
  case AST_EMETHOD:
    out = lift_kind_method(heap, n);
    break;
  case AST_EUNARY:
    out = lift_kind_unary(heap, n);
    break;
  case AST_ECLOSURE:
    out = lift_kind_closure(n);
    break;
  case AST_EIF:
    out = lift_kind_if(n);
    break;
  case AST_EMATCH: {
    LiftK k = {.kind = LIFT_KMATCH, .scrut = n->scrut, .arms = n->arms};
    out = k;
    break;
  }
  case AST_EFIELD:
    out = lift_k_bad(str_lit("field access"));
    break;
  case AST_ETRY:
    out = lift_k_bad(str_lit("`?` operator"));
    break;
  case AST_EBINARY:
    out = lift_k_bad(lift_cat3(heap, str_lit("binary operator `"), n->op, str_lit("`")));
    break;
  case AST_EBLOCK:
    out = lift_kind_block(n);
    break;
  case AST_ELET:
    out = lift_k_bad(str_lit("`let` expression"));
    break;
  case AST_ESTRUCT:
    out = lift_k_bad(str_lit("struct expression"));
    break;
  case AST_ETUPLE:
    out = lift_k_bad(str_lit("tuple expression"));
    break;
  case AST_EPAREN:
    out = lift_k_bad(str_lit("parenthesized expression"));
    break;
  case AST_EMACRO:
    out = lift_k_bad(lift_cat3(heap, str_lit("macro call `"), n->name, str_lit("!`")));
    break;
  case AST_IFN:
  case AST_ISTRUCT:
  case AST_IENUM:
  case AST_IUSE:
  case AST_IMOD:
  case AST_IIMPL:
  case AST_ITRAIT:
  case AST_ICONST:
  case AST_ITYPE:
    out = lift_k_bad(str_lit("item in an expression"));
    break;
  }
  return out;
}

/* Job: one expression to lift, with the position of its nearest statement, arm or item. */
struct LiftJob {
  AstPos at;
  LiftK k;
};

static const LiftJob lift_job_placeholder = {.k = {.kind = LIFT_KBAD}};

/* C: Job{at, k} on the heap. */
static const LiftJob *lift_job(Heap *heap, AstPos at, const LiftK *k) {
  LiftJob *box = heap_alloc(heap, sizeof(LiftJob));
  if (!box) return &lift_job_placeholder;
  box->at = at;
  box->k = *k;
  return box;
}

/* C: Done{j} */
static LiftR lift_done_job(const LiftJob *j) {
  LiftR r = {0};
  r.ok = true;
  r.job = j;
  return r;
}

/* jobs(at, ns): Job pointers in source order. */
static Vec lift_jobs(Heap *heap, AstPos at, const Vec *ns) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ns->len; i++) {
    VecGet g = vec_get(ns, i);
    if (g.status == VEC_GET_OK) {
      LiftK k = lift_kind(heap, g.value);
      lift_push(heap, &out, lift_job(heap, at, &k));
    }
  }
  return out;
}

/* stmt_refusal(s) */
static LiftRefusal lift_stmt_refusal(Heap *heap, const AstStmt *s) {
  LiftRefusal r = {0};
  switch (s->kind) {
  case AST_SLET:
    r.pos = ast_meta_pos(&s->meta);
    r.what = str_lit("`let` statement");
    break;
  case AST_SEXPR:
    r.pos = ast_meta_pos(&s->meta);
    r.what = str_lit("statement before the tail expression");
    break;
  case AST_SITEM:
    r.pos = ast_item_pos(heap, s->item);
    r.what = str_lit("item in a block");
    break;
  }
  return r;
}

/* let_rest(at, meta, name, t, v, rest): a `let` statement before the tail (E1), lifted to KLet. */
static LiftR lift_let_rest(Heap *heap, AstPos at, const AstMeta *meta, Str name, AstMaybeTy t, const AstNode *v,
                           const Vec *rest) {
  if (rest->len == 0) return lift_fail(at, str_lit("`let` with no tail expression"));
  LiftR ok = lift_plain(meta);
  if (!ok.ok) return ok;
  LiftK k = {.kind = LIFT_KLET, .name = name, .ty = t, .init = v, .rest = *rest};
  return lift_done_job(lift_job(heap, at, &k));
}

/* let_init(at, meta, name, t, init, rest) */
static LiftR lift_let_init(Heap *heap, AstPos at, const AstMeta *meta, Str name, AstMaybeTy t, AstMaybeNode init,
                           const Vec *rest) {
  if (!init.some) return lift_fail(at, str_lit("`let` without an initializer"));
  return lift_let_rest(heap, at, meta, name, t, init.value, rest);
}

/* let_job(at, meta, p, t, init, rest) */
static LiftR lift_let_job(Heap *heap, AstPos at, const AstMeta *meta, const AstPat *p, AstMaybeTy t,
                          AstMaybeNode init, const Vec *rest) {
  if (lift_name_pat(p)) return lift_let_init(heap, at, meta, lift_seg_name(&p->segs, 0), t, init, rest);
  return lift_fail(at, str_lit("`let` with a pattern"));
}

/* C: xs without its head (the Bend `rest` of `Con{x, rest}`). */
static Vec lift_drop1(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 1; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) lift_push(heap, &out, g.value);
  }
  return out;
}

/* tail_job(pos, stmts): the inverse of `Em.tail`: a block that is one tail expression, or lets
 * before it. Case order of the Bend match: one SExpr, an SLet head, any other head, Nil. */
static LiftR lift_tail_job(Heap *heap, AstPos pos, const Vec *stmts) {
  VecGet g = vec_get(stmts, 0);
  if (g.status != VEC_GET_OK) return lift_fail(pos, str_lit("empty block"));
  const AstStmt *s = g.value;
  if (s->kind == AST_SEXPR && stmts->len == 1) {
    LiftR ok = lift_plain(&s->meta);
    if (!ok.ok) return ok;
    AstPos at = ast_meta_pos(&s->meta);
    if (s->semi) return lift_fail(at, str_lit("expression statement"));
    LiftK k = lift_kind(heap, s->expr);
    return lift_done_job(lift_job(heap, at, &k));
  }
  if (s->kind == AST_SLET) {
    Vec rest = lift_drop1(heap, stmts);
    return lift_let_job(heap, ast_meta_pos(&s->meta), &s->meta, s->pat, s->ty, s->init, &rest);
  }
  LiftRefusal r = lift_stmt_refusal(heap, s);
  return lift_fail(r.pos, r.what);
}

/* arm_block(pos, j) */
static LiftR lift_arm_block(AstPos pos, const LiftJob *j) {
  if (j->k.kind == LIFT_KIF || j->k.kind == LIFT_KLET) return lift_done_job(j);
  return lift_fail(pos, str_lit("block in a match arm"));
}

/* arm_job(pos, body): the inverse of `Em.arm_body`: a block in an arm body holds an `if` or `let`. */
static LiftR lift_arm_job(Heap *heap, AstPos pos, const AstNode *body) {
  if (body->kind == AST_EBLOCK) {
    LiftR j = lift_tail_job(heap, pos, &body->stmts);
    if (!j.ok) return j;
    return lift_arm_block(pos, j.job);
  }
  LiftK k = lift_kind(heap, body);
  return lift_done_job(lift_job(heap, pos, &k));
}

/* arm_jobs(xs): Job pointers in arm order. */
static LiftR lift_arm_jobs(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) {
      const AstArm *a = g.value;
      LiftR j = lift_arm_job(heap, ast_meta_pos(&a->meta), a->body);
      if (!j.ok) return j;
      lift_push(heap, &out, j.job);
    }
  }
  return lift_done_list(out);
}

/* one(xs) */
static const RirEx *lift_one(Heap *heap, const Vec *xs) {
  VecGet g = vec_get(xs, 0);
  if (g.status == VEC_GET_OK) return g.value;
  return rir_xbool(heap, false);
}

/* C: Done{m} */
static LiftR lift_done_mty(RirMaybeTy m) {
  LiftR r = {0};
  r.ok = true;
  r.mty = m;
  return r;
}

/* let_ty(pos, t): the type of a let binder, `None` when the Rust text gives none (E1). */
static LiftR lift_let_ty(Heap *heap, AstPos pos, AstMaybeTy t) {
  RirMaybeTy none = rir_no_ty();
  if (!t.some) return lift_done_mty(none);
  LiftR rt = lift_ty(heap, pos, t.value);
  if (!rt.ok) return rt;
  RirMaybeTy some = rir_some_ty(rt.ty);
  return lift_done_mty(some);
}

/* if_of(xs) */
static const RirEx *lift_if_of(Heap *heap, const Vec *xs) {
  VecGet c = vec_get(xs, 0);
  VecGet y = vec_get(xs, 1);
  VecGet n = vec_get(xs, 2);
  if (c.status == VEC_GET_OK && y.status == VEC_GET_OK && n.status == VEC_GET_OK)
    return rir_xif(heap, c.value, y.value, n.value);
  return rir_xbool(heap, false);
}

/* Zip: the patterns (RirPat pointers) and bodies (RirEx pointers) of the arms of one match. */
typedef struct {
  Vec pats;
  Vec bodies;
} LiftZip;

/* zip_arms(z): RirArm pointers, as many as the shorter list. */
static Vec lift_zip_arms(Heap *heap, const LiftZip *z) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < z->pats.len && i < z->bodies.len; i++) {
    VecGet p = vec_get(&z->pats, i);
    VecGet b = vec_get(&z->bodies, i);
    if (p.status == VEC_GET_OK && b.status == VEC_GET_OK) {
      RirPat pat = *(const RirPat *)p.value;
      lift_push(heap, &out, rir_arm(heap, pat, b.value));
    }
  }
  return out;
}

static LiftR lift_walk(Heap *heap, const Vec *xs);

/* C: Done{x} */
static LiftR lift_done_ex(const RirEx *x) {
  LiftR r = {0};
  r.ok = true;
  r.ex = x;
  return r;
}

/* C: walk([Job{at, kind(n)}]) */
static LiftR lift_walk_node(Heap *heap, AstPos at, const AstNode *n) {
  LiftK k = lift_kind(heap, n);
  Vec one = lift_list1(heap, lift_job(heap, at, &k));
  return lift_walk(heap, &one);
}

/* C: the head R.Ex of walk(Con{j, rest}). Each Bend case runs every check of its head before
 * walk(rest), and KBlock walks its tail job in place of the head, so walk is this function on each
 * job in order. Inside a case the check order of the Bend do-block is kept. */
static LiftR lift_walk_job(Heap *heap, const LiftJob *j) {
  AstPos at = j->at;
  const LiftK *k = &j->k;
  switch (k->kind) {
  case LIFT_KVAR:
    return lift_done_ex(rir_xvar(heap, k->name));
  case LIFT_KINT:
    return lift_done_ex(rir_xint(heap, k->text));
  case LIFT_KBOOL:
    return lift_done_ex(rir_xbool(heap, k->value));
  case LIFT_KCALL: {
    LiftR ts = lift_tys(heap, at, &k->targs);
    if (!ts.ok) return ts;
    Vec js = lift_jobs(heap, at, &k->args);
    LiftR ys = lift_walk(heap, &js);
    if (!ys.ok) return ys;
    return lift_done_ex(rir_xcall(heap, k->func, ts.list, ys.list));
  }
  case LIFT_KCTOR: {
    Vec js = lift_jobs(heap, at, &k->args);
    LiftR ys = lift_walk(heap, &js);
    if (!ys.ok) return ys;
    return lift_done_ex(rir_xctor(heap, k->family, k->ctor, ys.list));
  }
  case LIFT_KMATCH: {
    LiftR ss = lift_walk_node(heap, at, k->scrut);
    if (!ss.ok) return ss;
    LiftR ps = lift_arm_pats(heap, &k->arms);
    if (!ps.ok) return ps;
    LiftR bj = lift_arm_jobs(heap, &k->arms);
    if (!bj.ok) return bj;
    LiftR bs = lift_walk(heap, &bj.list);
    if (!bs.ok) return bs;
    LiftZip z = {.pats = ps.list, .bodies = bs.list};
    Vec arms = lift_zip_arms(heap, &z);
    const RirEx *scrut = lift_one(heap, &ss.list);
    return lift_done_ex(rir_xmatch(heap, scrut, arms));
  }
  case LIFT_KIF: {
    LiftR yj = lift_tail_job(heap, at, &k->yes);
    if (!yj.ok) return yj;
    LiftR nj = lift_tail_job(heap, at, &k->no);
    if (!nj.ok) return nj;
    LiftK ck = lift_kind(heap, k->cond);
    Vec three = vec_new(heap->arena);
    lift_push(heap, &three, lift_job(heap, at, &ck));
    lift_push(heap, &three, yj.job);
    lift_push(heap, &three, nj.job);
    LiftR parts = lift_walk(heap, &three);
    if (!parts.ok) return parts;
    return lift_done_ex(lift_if_of(heap, &parts.list));
  }
  case LIFT_KCLOSURE: {
    LiftR rps = lift_cparams(heap, at, &k->params);
    if (!rps.ok) return rps;
    LiftR bs = lift_walk_node(heap, at, k->body);
    if (!bs.ok) return bs;
    const RirEx *body = lift_one(heap, &bs.list);
    return lift_done_ex(rir_xclosure(heap, rps.list, body));
  }
  case LIFT_KBORROW: {
    LiftR ys = lift_walk_node(heap, at, k->inner);
    if (!ys.ok) return ys;
    return lift_done_ex(rir_xborrow(heap, lift_one(heap, &ys.list)));
  }
  case LIFT_KCLONE: {
    LiftR ys = lift_walk_node(heap, at, k->inner);
    if (!ys.ok) return ys;
    return lift_done_ex(rir_xclone(heap, lift_one(heap, &ys.list)));
  }
  case LIFT_KBOX: {
    LiftR ys = lift_walk_node(heap, at, k->inner);
    if (!ys.ok) return ys;
    return lift_done_ex(rir_xbox(heap, lift_one(heap, &ys.list)));
  }
  case LIFT_KLET: {
    LiftR rt = lift_let_ty(heap, at, k->ty);
    if (!rt.ok) return rt;
    LiftR vs = lift_walk_node(heap, at, k->init);
    if (!vs.ok) return vs;
    LiftR bj = lift_tail_job(heap, at, &k->rest);
    if (!bj.ok) return bj;
    Vec bjs = lift_list1(heap, bj.job);
    LiftR bs = lift_walk(heap, &bjs);
    if (!bs.ok) return bs;
    const RirEx *value = lift_one(heap, &vs.list);
    const RirEx *body = lift_one(heap, &bs.list);
    return lift_done_ex(rir_xlet(heap, k->name, rt.mty, value, body));
  }
  case LIFT_KBLOCK: {
    LiftR jb = lift_tail_job(heap, at, &k->stmts);
    if (!jb.ok) return jb;
    return lift_walk_job(heap, jb.job);
  }
  case LIFT_KBAD:
    return lift_fail(at, k->what);
  }
  return lift_fail(at, k->what);
}

/* walk(xs): R.Ex pointers in source order. */
static LiftR lift_walk(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) {
      LiftR x = lift_walk_job(heap, g.value);
      if (!x.ok) return x;
      lift_push(heap, &out, x.ex);
    }
  }
  return lift_done_list(out);
}

/* ---- items ---- */

/* C: Done{s} */
static LiftR lift_done_str(Str s) {
  LiftR r = {0};
  r.ok = true;
  r.str = s;
  return r;
}

/* use_module(pos, tree): the module of `use crate::<module>::*;`. */
static LiftR lift_use_module(AstPos pos, const AstUseTree *tree) {
  const AstUseTree *sub = NULL;
  if (tree != NULL && tree->kind == AST_UPATH) sub = tree->sub;
  bool shape = sub != NULL && sub->kind == AST_UPATH && sub->sub != NULL && sub->sub->kind == AST_UGLOB;
  if (shape && str_eq(tree->seg, str_lit("crate"))) return lift_done_str(sub->seg);
  return lift_fail(pos, str_lit("`use` other than `use crate::<module>::*;`"));
}

/* C: placeholder of lift_str_box when the heap is out of memory (emit.c idiom). */
static const Str lift_str_placeholder = {0};

/* C: a heap copy of s, for the lists of Str pointers (emit.c emit_str_box idiom). */
static const Str *lift_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (!box) return &lift_str_placeholder;
  *box = s;
  return box;
}

/* enum_gens(pos, gs): the names of the enum generics (Str pointers); a generic with a bound is a
 * refusal. */
static LiftR lift_enum_gens(Heap *heap, AstPos pos, const Vec *gs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    VecGet g = vec_get(gs, i);
    if (g.status == VEC_GET_OK) {
      const AstGParam *p = g.value;
      if (p->bounds.len != 0)
        return lift_fail(pos, lift_cat3(heap, str_lit("bound on the enum generic `"), p->name, str_lit("`")));
      lift_push(heap, &out, lift_str_box(heap, p->name));
    }
  }
  return lift_done_list(out);
}

/* clone_bound(pos, b) */
static LiftR lift_clone_bound(AstPos pos, Str b) {
  if (str_eq(b, str_lit("Clone"))) return lift_done_flag(true);
  return lift_fail(pos, str_lit("fn generic bound other than `Clone`"));
}

/* fn_gens(pos, gs): the inverse of `Em.bounds(gs, [Clone])`. A generic matches when it has
 * exactly one bound, a path of one segment with no generic arguments (the Bend pattern
 * `Con{A.TPath{Con{A.Seg{b, Nil{}}, Nil{}}}, Nil{}}`); clone_bound then checks the name. */
static LiftR lift_fn_gens(Heap *heap, AstPos pos, const Vec *gs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    VecGet g = vec_get(gs, i);
    if (g.status == VEC_GET_OK) {
      const AstGParam *p = g.value;
      VecGet hb = vec_get(&p->bounds, 0);
      const AstTy *t = NULL;
      if (p->bounds.len == 1 && hb.status == VEC_GET_OK) t = hb.value;
      if (t == NULL || t->kind != AST_TPATH || !lift_bare_segs(&t->segs, 1))
        return lift_fail(pos, lift_cat3(heap, str_lit("fn generic `"), p->name,
                                        str_lit("` without exactly the bound `Clone`")));
      LiftR ok = lift_clone_bound(pos, lift_seg_name(&t->segs, 0));
      if (!ok.ok) return ok;
      lift_push(heap, &out, lift_str_box(heap, p->name));
    }
  }
  return lift_done_list(out);
}

/* fields(xs): the R.Ty pointers of tuple fields; a named field is a refusal. */
static LiftR lift_fields(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) {
      const AstField *f = g.value;
      AstPos at = ast_meta_pos(&f->meta);
      if (f->kind == AST_NFIELD) return lift_fail(at, str_lit("named field"));
      LiftR ok = lift_plain(&f->meta);
      if (!ok.ok) return ok;
      LiftR v = lift_is_priv(at, f->vis);
      if (!v.ok) return v;
      LiftR rt = lift_ty(heap, at, f->ty);
      if (!rt.ok) return rt;
      lift_push(heap, &out, rt.ty);
    }
  }
  return lift_done_list(out);
}

/* variant_fields(pos, body) */
static LiftR lift_variant_fields(Heap *heap, AstPos pos, const AstBody *body) {
  Vec none = vec_new(heap->arena);
  LiftR out = lift_done_list(none);
  switch (body->kind) {
  case AST_BUNIT:
    break;
  case AST_BTUPLE:
    if (body->fields.len == 0) out = lift_fail(pos, str_lit("variant with empty parentheses"));
    if (body->fields.len != 0) out = lift_fields(heap, &body->fields);
    break;
  case AST_BNAMED:
    out = lift_fail(pos, str_lit("variant with named fields"));
    break;
  }
  return out;
}

/* variants(xs): R.Variant pointers in source order. */
static LiftR lift_variants(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    VecGet g = vec_get(xs, i);
    if (g.status == VEC_GET_OK) {
      const AstVariant *v = g.value;
      LiftR ok = lift_plain(&v->meta);
      if (!ok.ok) return ok;
      LiftR fs = lift_variant_fields(heap, ast_meta_pos(&v->meta), &v->body);
      if (!fs.ok) return fs;
      lift_push(heap, &out, rir_variant(heap, v->name, fs.list));
    }
  }
  return lift_done_list(out);
}

/* ret_ty(pos, r) */
static LiftR lift_ret_ty(Heap *heap, AstPos pos, AstMaybeTy r) {
  if (r.some) return lift_ty(heap, pos, r.value);
  return lift_fail(pos, str_lit("fn without a return type"));
}

/* fn_body(pos, b): `b` holds AstStmt pointers when some. */
static LiftR lift_fn_body(Heap *heap, AstPos pos, const MaybeVec *b) {
  if (b->some) return lift_tail_job(heap, pos, &b->value);
  return lift_fail(pos, str_lit("fn without a body"));
}

/* C: Done{x} */
static LiftR lift_done_item(const RirItem *x) {
  LiftR r = {0};
  r.ok = true;
  r.item = x;
  return r;
}

/* fn_item(meta, vis, sig, body): the inverse of `Em.fn_item`. */
static LiftR lift_fn_item(Heap *heap, const AstMeta *meta, AstVis vis, const AstSig *sig, const MaybeVec *body) {
  AstPos at = ast_meta_pos(meta);
  LiftR ok = lift_plain(meta);
  if (!ok.ok) return ok;
  LiftR v = lift_is_pub(at, vis);
  if (!v.ok) return v;
  LiftR gs = lift_fn_gens(heap, at, &sig->generics);
  if (!gs.ok) return gs;
  LiftR rps = lift_params(heap, at, &sig->params);
  if (!rps.ok) return rps;
  LiftR rt = lift_ret_ty(heap, at, sig->ret);
  if (!rt.ok) return rt;
  LiftR b = lift_fn_body(heap, at, body);
  if (!b.ok) return b;
  Vec bs = lift_list1(heap, b.job);
  LiftR es = lift_walk(heap, &bs);
  if (!es.ok) return es;
  const RirEx *one = lift_one(heap, &es.list);
  return lift_done_item(rir_ifn(heap, sig->name, gs.list, rps.list, rt.ty, one));
}

/* unit_copy(pos, cp) */
static LiftR lift_unit_copy(AstPos pos, bool cp) {
  if (cp) return lift_done_flag(true);
  return lift_fail(pos, str_lit("unit struct without `Copy` in its derive list"));
}

/* body_ok(pos, name, body) */
static LiftR lift_body_ok(Heap *heap, AstPos pos, Str name, const AstBody *body) {
  LiftR out = lift_done_flag(true);
  switch (body->kind) {
  case AST_BUNIT:
    break;
  case AST_BTUPLE:
    out = lift_fail(pos, lift_cat3(heap, str_lit("struct `"), name, str_lit("` with fields")));
    break;
  case AST_BNAMED:
    out = lift_fail(pos, lift_cat3(heap, str_lit("struct `"), name, str_lit("` with fields")));
    break;
  }
  return out;
}

/* unit_body(pos, name, gs, body): a struct of this fragment is a unit struct, with no generics
 * and no fields. */
static LiftR lift_unit_body(Heap *heap, AstPos pos, Str name, const Vec *gs, const AstBody *body) {
  if (gs->len == 0) return lift_body_ok(heap, pos, name, body);
  return lift_fail(pos, lift_cat3(heap, str_lit("struct `"), name, str_lit("` with generics")));
}

/* C: the refusal text of a unit struct without its constructor fn. */
static Str lift_no_ctor(Heap *heap, Str name) {
  return lift_cat3(heap, str_lit("unit struct `"), name, str_lit("` without its constructor fn"));
}

/* ctor_of(pos, name, f): the constructor fn of the unit struct `name`,
 * `fn <ctor>() -> name { name }` (R.IFn{ctor, [], [], R.TCon{name, []}, R.XVar{name}}). */
static LiftR lift_ctor_of(Heap *heap, AstPos pos, Str name, const RirItem *f) {
  bool shape = f != NULL && f->kind == RIR_IFN && f->generics.len == 0 && f->params.len == 0 && f->ret != NULL &&
               f->ret->kind == RIR_TCON && f->ret->args.len == 0 && f->body != NULL && f->body->kind == RIR_XVAR;
  if (shape && str_eq(f->ret->name, name) && str_eq(f->body->name, name)) return lift_done_str(f->name);
  return lift_fail(pos, lift_no_ctor(heap, name));
}

/* C: one step of items: the R.Item of the head n, where next is the node after n (NULL at the
 * end). *used is the count of nodes the step takes: 2 for a unit struct and its constructor fn,
 * else 1. The if-tests keep the Bend case order: IUse, IEnum, IStruct followed by an IFn,
 * IStruct, IFn, other. */
static LiftR lift_item_head(Heap *heap, const AstNode *n, const AstNode *next, size_t *used) {
  *used = 1;
  if (n->kind == AST_IUSE) {
    AstPos at = ast_meta_pos(&n->meta);
    LiftR ok = lift_plain(&n->meta);
    if (!ok.ok) return ok;
    LiftR v = lift_is_priv(at, n->vis);
    if (!v.ok) return v;
    LiftR m = lift_use_module(at, n->tree);
    if (!m.ok) return m;
    return lift_done_item(rir_iuse(heap, m.str));
  }
  if (n->kind == AST_IENUM) {
    AstPos at = ast_meta_pos(&n->meta);
    LiftR cp = lift_derived(heap, &n->meta);
    if (!cp.ok) return cp;
    LiftR v = lift_is_pub(at, n->vis);
    if (!v.ok) return v;
    LiftR gs = lift_enum_gens(heap, at, &n->generics);
    if (!gs.ok) return gs;
    LiftR rvs = lift_variants(heap, &n->variants);
    if (!rvs.ok) return rvs;
    return lift_done_item(rir_ienum(heap, n->name, gs.list, cp.flag, rvs.list));
  }
  if (n->kind == AST_ISTRUCT) {
    AstPos at = ast_meta_pos(&n->meta);
    LiftR u = lift_unit_body(heap, at, n->name, &n->generics, &n->struct_body);
    if (!u.ok) return u;
    if (next == NULL || next->kind != AST_IFN) return lift_fail(at, lift_no_ctor(heap, n->name));
    *used = 2;
    LiftR cp = lift_derived(heap, &n->meta);
    if (!cp.ok) return cp;
    LiftR c = lift_unit_copy(at, cp.flag);
    if (!c.ok) return c;
    LiftR v = lift_is_pub(at, n->vis);
    if (!v.ok) return v;
    LiftR f = lift_fn_item(heap, &next->meta, next->vis, &next->sig, &next->fn_body);
    if (!f.ok) return f;
    LiftR ctor = lift_ctor_of(heap, at, n->name, f.item);
    if (!ctor.ok) return ctor;
    return lift_done_item(rir_iunit(heap, n->name, ctor.str));
  }
  if (n->kind == AST_IFN) return lift_fn_item(heap, &n->meta, n->vis, &n->sig, &n->fn_body);
  return lift_fail(ast_item_pos(heap, n), lift_cat3(heap, str_lit("item `"), ast_item_line(heap, n), str_lit("`")));
}

/* items(xs): R.Item pointers in source order. */
static LiftR lift_items(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  size_t used = 1;
  for (size_t i = 0; i < xs->len; i += used) {
    VecGet g = vec_get(xs, i);
    VecGet h = vec_get(xs, i + 1);
    const AstNode *next = NULL;
    if (h.status == VEC_GET_OK) next = h.value;
    used = 1;
    if (g.status == VEC_GET_OK) {
      LiftR x = lift_item_head(heap, g.value, next, &used);
      if (!x.ok) return x;
      lift_push(heap, &out, x.item);
    }
  }
  return lift_done_list(out);
}

/* ---- file ---- */

/* strip(pre, s): s without the prefix pre, or None. Bend compares one char at a time; UTF-8 is
 * prefix-free, so the byte prefix test gives the same answer. */
static MaybeStr lift_strip(Str pre, Str s) {
  MaybeStr out = {0};
  out.some = false;
  StrSlice rest = str_slice(s, pre.len, s.len);
  if (str_starts_with(s, pre) && rest.status == STR_INDEX_OK) {
    out.some = true;
    out.value = rest.value;
  }
  return out;
}

/* C: String.reverse as a byte reversal. source_in applies it twice around a strip of the ASCII
 * ".`", so its result is the same text as with the char reversal of Bend. */
static Str lift_reverse(Heap *heap, Str s) {
  StrBuf b = strbuf_new(heap->arena);
  for (size_t i = s.len; i > 0; i--) {
    StrAt c = str_at(s, i - 1);
    if (c.status == STR_INDEX_OK && !strbuf_push_byte(&b, (char)c.value)) heap->oom = true;
  }
  return strbuf_str(&b);
}

/* unreversed(m) */
static Str lift_unreversed(Heap *heap, MaybeStr m) {
  if (m.some) return lift_reverse(heap, m.value);
  return str_empty();
}

/* source_in(m) */
static Str lift_source_in(Heap *heap, MaybeStr m) {
  if (m.some) {
    Str r = lift_reverse(heap, m.value);
    MaybeStr s = lift_strip(str_lit(".`"), r);
    return lift_unreversed(heap, s);
  }
  return str_empty();
}

/* source_of(inner): the inverse of the `//!` line of `Em.lower`. Any other `//!` text gives "":
 * CD4 then names the file `<module>.mech` (C2). `inner` holds Str pointers. */
static Str lift_source_of(Heap *heap, const Vec *inner) {
  VecGet g = vec_get(inner, 0);
  if (inner->len == 1 && g.status == VEC_GET_OK) {
    MaybeStr s = lift_strip(str_lit(" Emitted by `mech rust-out` from `"), *(const Str *)g.value);
    return lift_source_in(heap, s);
  }
  return str_empty();
}

/* lift(f) */
LiftModuleResult lift_lift(Heap *heap, const AstSrcFile *f) {
  LiftModuleResult out = {0};
  LiftR xs = lift_items(heap, &f->items);
  if (!xs.ok) {
    out.ok = false;
    out.refusal = xs.refusal;
    return out;
  }
  Str source = lift_source_of(heap, &f->inner);
  out.ok = true;
  out.module = rir_module(source, xs.list);
  return out;
}

/* C: plain(m) as a public Result<Refusal, Bool>, for resolve.bend (Li.plain). */
LiftFlagResult lift_plain_result(const AstMeta *m) {
  LiftR r = lift_plain(m);
  LiftFlagResult out = {0};
  out.ok = r.ok;
  out.refusal = r.refusal;
  out.flag = r.flag;
  return out;
}
