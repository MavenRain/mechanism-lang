/* C port of rust/print.bend: the Rust pretty printer of the M0 frontend (Pr.file). Bend has one
 * self-recursive `pp(Job)` because Bend 2 has no forward references; here each Job constructor is one
 * static function pp_<job>, in the Job type order, with the prototypes up front (design D-186a).
 * JPick and JSel are an if over the computed MaybeStr. The two Jobs passed as a deferred `alt` (JVert
 * from JArgs, JChainV from JChain) are a PrintAlt, run by pp_alt only when the first choice fails.
 * Widths are size_t (Bend Nat) and print_sub saturates at 0. Lengths count UTF-8 code points. A
 * failed allocation sets heap->oom and the printer goes on with empty strings. */
#include <string.h>
#include "rust.h"

#define PL(s) str_lit(s)
#define CAT(h, ...) print_cat(h, (const Str[]){__VA_ARGS__}, sizeof((const Str[]){__VA_ARGS__}) / sizeof(Str))
#define PRINT_ITEM_CASES \
  case AST_IFN:          \
  case AST_ISTRUCT:      \
  case AST_IENUM:        \
  case AST_IUSE:         \
  case AST_IMOD:         \
  case AST_IIMPL:        \
  case AST_ITRAIT:       \
  case AST_ICONST:       \
  case AST_ITYPE:

static Str print_cat(Heap *h, const Str *parts, size_t n) {
  size_t total = 0;
  for (size_t i = 0; i < n; i++) {
    total += parts[i].len;
  }
  if (total == 0) {
    return str_empty();
  }
  char *data = heap_alloc(h, total);
  if (data == NULL) {
    return str_empty();
  }
  size_t at = 0;
  for (size_t i = 0; i < n; i++) {
    if (parts[i].len > 0) {
      memcpy(data + at, parts[i].data, parts[i].len);
    }
    at += parts[i].len;
  }
  return str_n(data, at);
}

/* Element i of a Vec of pointers, or NULL out of range. */
static const void *print_at(const Vec *v, size_t i) {
  VecGet g = vec_get(v, i);
  return g.status == VEC_GET_OK ? g.value : NULL;
}

static Str print_str_at(const Vec *v, size_t i) {
  const Str *s = print_at(v, i);
  return s != NULL ? *s : str_empty();
}

/* A list printer's output buffer. */
typedef struct {
  Heap *heap;
  StrBuf buf;
} PrintOut;

static PrintOut print_out(Heap *h) {
  PrintOut o;
  o.heap = h;
  o.buf = strbuf_new(h->arena);
  return o;
}

static void print_push(PrintOut *o, Str s) {
  if (s.len > 0 && !strbuf_push(&o->buf, s)) {
    o->heap->oom = true;
  }
}

static Str print_done(const PrintOut *o) {
  return o->buf.len > 0 ? strbuf_str(&o->buf) : str_empty();
}

/* ---- leaves ---- */

static size_t print_slen(Str s) {
  size_t n = 0;
  for (size_t i = 0; i < s.len; i++) {
    if (((unsigned char)s.data[i] & 0xC0) != 0x80) {
      n++;
    }
  }
  return n;
}

/* Code points before the first newline. */
static size_t print_flen(Str s) {
  size_t n = 0;
  for (size_t i = 0; i < s.len && s.data[i] != '\n'; i++) {
    if (((unsigned char)s.data[i] & 0xC0) != 0x80) {
      n++;
    }
  }
  return n;
}

static bool print_one_line(Str s) {
  for (size_t i = 0; i < s.len; i++) {
    if (s.data[i] == '\n') {
      return false;
    }
  }
  return true;
}

static bool print_le(size_t a, size_t b) {
  return a <= b;
}

static size_t print_sub(size_t a, size_t b) {
  return a > b ? a - b : 0;
}

static size_t print_min(size_t a, size_t b) {
  return a < b ? a : b;
}

static Str print_rep(Heap *h, char c, size_t n) {
  if (n == 0) {
    return str_empty();
  }
  char *data = heap_alloc(h, n);
  if (data == NULL) {
    return str_empty();
  }
  memset(data, c, n);
  return str_n(data, n);
}

static Str print_sp(Heap *h, size_t n) {
  return print_rep(h, ' ', n);
}

static size_t print_room(size_t ind, size_t used) {
  return print_sub(100, ind + used);
}

static MaybeStr print_fit(Str s, size_t avail) {
  MaybeStr m;
  m.some = print_one_line(s) && print_le(print_slen(s), avail);
  m.value = m.some ? s : str_empty();
  return m;
}

static Str print_capped(Heap *h, Str s, size_t limit) {
  return CAT(h, s, print_le(print_slen(s), limit) ? PL("") : PL("\n"));
}

static Str print_qs(Heap *h, size_t q) {
  return print_rep(h, '?', q);
}

static Str print_join(Heap *h, const Vec *xs, Str sep) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < xs->len; i++) {
    print_push(&o, i == 0 ? str_empty() : sep);
    print_push(&o, print_str_at(xs, i));
  }
  return print_done(&o);
}

static Str print_vis(AstVis v) {
  switch (v) {
  case AST_PRIV:
    return PL("");
  case AST_PUB:
    return PL("pub ");
  case AST_PUB_CRATE:
    return PL("pub(crate) ");
  }
  return str_empty();
}

static Str print_lit(Heap *h, const AstLit *l) {
  switch (l->kind) {
  case AST_LINT:
    return l->text;
  case AST_LSTR:
    return CAT(h, PL("\""), l->text, PL("\""));
  case AST_LCHAR:
    return CAT(h, PL("'"), l->text, PL("'"));
  case AST_LBOOL:
    if (l->boolean) {
      return PL("true");
    } else {
      return PL("false");
    }
  }
  return str_empty();
}

static Str print_attr_args(Heap *h, const MaybeVec *m) {
  if (!m->some) {
    return str_empty();
  }
  return CAT(h, PL("("), print_join(h, &m->value, PL(", ")), PL(")"));
}

static Str print_attrs(Heap *h, const Vec *xs, Str pre) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < xs->len; i++) {
    const AstAttr *a = print_at(xs, i);
    if (a != NULL) {
      print_push(&o, CAT(h, pre, PL("#["), a->name, print_attr_args(h, &a->args), PL("]\n")));
    }
  }
  return print_done(&o);
}

static Str print_lines(Heap *h, const Vec *xs, Str pre) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < xs->len; i++) {
    print_push(&o, CAT(h, pre, print_str_at(xs, i), PL("\n")));
  }
  return print_done(&o);
}

static Str print_meta(Heap *h, const AstMeta *m, size_t ind) {
  return CAT(h, m->gap ? PL("\n") : PL(""), print_lines(h, &m->docs, CAT(h, print_sp(h, ind), PL("///"))),
             print_attrs(h, &m->attrs, print_sp(h, ind)));
}

static Str print_meta_mark(const AstMeta *m) {
  return m->docs.len == 0 && m->attrs.len == 0 ? PL("") : PL("\n");
}

static size_t print_body_len(const MaybeVec *b) {
  return b->some ? 2 : 1;
}

/* ovf(e, single): whether the last argument may overflow its line and keep the head on it. */
static bool print_ovf(const AstNode *e, bool single) {
  if (e == NULL) {
    return false;
  }
  switch (e->kind) {
  case AST_ECLOSURE:
  case AST_EBLOCK:
    return true;
  case AST_EMATCH:
  case AST_EIF:
  case AST_ESTRUCT:
  case AST_ECALL:
  case AST_EMETHOD:
  case AST_ETUPLE:
  case AST_EMACRO:
    return single;
  case AST_ETRY:
  case AST_EUNARY:
    return print_ovf(e->inner, single);
  case AST_EPATH:
  case AST_ELIT:
  case AST_EFIELD:
  case AST_EBINARY:
  case AST_ELET:
  case AST_EPAREN:
  PRINT_ITEM_CASES
    return false;
  }
  return false;
}

/* ---- method chains ---- */

typedef enum { PRINT_LMETH, PRINT_LFIELD } PrintLinkKind;
typedef struct {
  PrintLinkKind kind;
  Str name;
  const Vec *gens; /* LMETH (AstTy pointers) */
  const Vec *args; /* LMETH (AstNode pointers) */
  size_t q;
} PrintLink;
typedef struct {
  const AstNode *root;
  size_t rq;
  Vec links; /* PrintLink pointers, root-outward */
} PrintChain;

static void print_push_link(Heap *h, Vec *acc, PrintLinkKind kind, const AstNode *e, size_t q) {
  PrintLink *l = heap_alloc(h, sizeof *l);
  if (l == NULL) {
    return;
  }
  l->kind = kind;
  l->name = e->name;
  l->gens = &e->generics;
  l->args = &e->args;
  l->q = q;
  if (!vec_push(acc, l)) {
    h->oom = true;
  }
}

/* chain(e, q, acc): the walk pushes outermost first; Bend prepends, so the result is reversed. */
static PrintChain print_chain_walk(Heap *h, const AstNode *e, size_t q, Vec *acc) {
  PrintChain c;
  c.root = e;
  c.rq = q;
  if (e != NULL) {
    switch (e->kind) {
    case AST_ETRY:
      return print_chain_walk(h, e->inner, q + 1, acc);
    case AST_EMETHOD:
      print_push_link(h, acc, PRINT_LMETH, e, q);
      return print_chain_walk(h, e->recv, 0, acc);
    case AST_EFIELD:
      print_push_link(h, acc, PRINT_LFIELD, e, q);
      return print_chain_walk(h, e->recv, 0, acc);
    case AST_EPATH:
    case AST_ELIT:
    case AST_ECALL:
    case AST_EUNARY:
    case AST_EBINARY:
    case AST_ECLOSURE:
    case AST_EBLOCK:
    case AST_EIF:
    case AST_ELET:
    case AST_EMATCH:
    case AST_ESTRUCT:
    case AST_ETUPLE:
    case AST_EPAREN:
    case AST_EMACRO:
    PRINT_ITEM_CASES
      break;
    }
  }
  c.links = heap_vec_reverse(h, acc);
  return c;
}

static PrintChain print_chain(Heap *h, const AstNode *e) {
  Vec acc = vec_new(h->arena);
  return print_chain_walk(h, e, 0, &acc);
}

static size_t print_cnt(const Vec *ls, size_t n) {
  size_t total = n;
  for (size_t i = 0; i < ls->len; i++) {
    const PrintLink *l = print_at(ls, i);
    total += l != NULL ? l->q + 1 : 0;
  }
  return total;
}

static size_t print_chain_cnt(const PrintChain *c) {
  return print_cnt(&c->links, c->rq);
}

static Str print_chain_mark(Heap *h, Str s, const PrintChain *c) {
  return CAT(h, s, print_le(2, print_chain_cnt(c)) && print_slen(s) > 60 ? PL("\n") : PL(""));
}

/* mixed(s): struct variant width, 0 when the flags hold both an "m" and another flag. */
static size_t print_mixed(Str s) {
  bool m = false;
  bool o = false;
  for (size_t i = 0; i < s.len; i++) {
    if (s.data[i] == 'm') {
      m = true;
    } else {
      o = true;
    }
  }
  return m && o ? 0 : 35;
}

/* ---- jobs ---- */

typedef enum { PRINT_ALT_VERT, PRINT_ALT_CHAINV } PrintAltKind;
typedef struct {
  PrintAltKind kind;
  Str head;            /* VERT */
  Str close;           /* VERT */
  const Vec *args;     /* VERT */
  const AstNode *root; /* CHAINV */
  size_t rq;           /* CHAINV */
  const Vec *ls;       /* CHAINV */
  size_t ind;
  size_t avail; /* CHAINV */
} PrintAlt;

static Str print_sep(bool first, Str s) {
  return first ? str_empty() : s;
}

static size_t print_ind4(size_t ind) {
  return ind + 4;
}

static Str pp_alt(Heap *h, const PrintAlt *alt);
static Str pp_ty(Heap *h, const AstTy *t);
static Str pp_tys(Heap *h, const Vec *ts, Str sep, bool first);
static Str pp_ret(Heap *h, AstMaybeTy r);
static Str pp_ann(Heap *h, AstMaybeTy t);
static Str pp_eq_ty(Heap *h, AstMaybeTy t);
static Str pp_turbo(Heap *h, const Vec *ts);
static Str pp_segs(Heap *h, const Vec *segs, bool turbo, bool first);
static Str pp_pat(Heap *h, const AstPat *p);
static Str pp_pats(Heap *h, const Vec *ps, Str sep, bool first);
static Str pp_fpats(Heap *h, const Vec *fs, bool first);
static Str pp_pstruct(Heap *h, const Vec *fs, bool rest);
static Str pp_gens(Heap *h, const Vec *gs);
static Str pp_glist(Heap *h, const Vec *gs, bool first);
static Str pp_param(Heap *h, const AstParam *p);
static Str pp_params(Heap *h, const Vec *ps, bool first);
static Str pp_param_lines(Heap *h, const Vec *ps, size_t ind);
static Str pp_cparams(Heap *h, const Vec *cs, bool first);
static Str pp_use(Heap *h, const AstUseTree *u);
static Str pp_uses(Heap *h, const Vec *us, bool first);
static Str pp_fl(Heap *h, const AstNode *e);
static Str pp_fls(Heap *h, const Vec *es, bool first);
static Str pp_fls_sep(Heap *h, const Vec *es, size_t n);
static Str pp_if_flat(Heap *h, const AstNode *c, const Vec *b, AstMaybeNode x);
static Str pp_struct_fl(Heap *h, const Vec *fs, AstMaybeNode base);
static Str pp_inits_fl(Heap *h, const Vec *fs, bool first);
static Str pp_base_fl(Heap *h, AstMaybeNode base, bool empty);
static Str pp_links_fl(Heap *h, const Vec *ls, size_t n);
static Str pp_ex(Heap *h, const AstNode *e, size_t ind, size_t avail);
static Str pp_brk(Heap *h, const AstNode *e, size_t ind, size_t avail);
static Str pp_cat(Heap *h, Str head, const AstNode *e, size_t ind, size_t avail);
static Str pp_args(Heap *h, Str head, Str close, const Vec *args, size_t ind, size_t avail);
static Str pp_ovf(Heap *h, Str head, Str close, const Vec *args, size_t ind, size_t avail, const PrintAlt *alt);
static Str pp_ovf3(Heap *h, Str pre, Str close, const AstNode *last, bool ok, size_t ind, size_t avail,
                   const PrintAlt *alt);
static Str pp_vert(Heap *h, Str head, Str close, const Vec *args, size_t ind);
static Str pp_arg_lines(Heap *h, const Vec *args, size_t ind);
static Str pp_chain(Heap *h, const PrintChain *c, size_t ind, size_t avail);
static Str pp_ch_ovf(Heap *h, Str pre, const Vec *ls, size_t ind, size_t budget, const PrintAlt *alt);
static Str pp_ch_ovf2(Heap *h, Str pre, const PrintLink *l, size_t ind, size_t budget, const PrintAlt *alt);
static Str pp_chain_v(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind, size_t avail);
static Str pp_chain_v2(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind);
static Str pp_absorb(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind, size_t avail);
static Str pp_links(Heap *h, const Vec *ls, size_t from, size_t ind);
static Str pp_link(Heap *h, const PrintLink *l, size_t ind, size_t avail);
static Str pp_inits(Heap *h, const Vec *fs, size_t ind);
static Str pp_base(Heap *h, AstMaybeNode base, size_t ind);
static Str pp_block(Heap *h, const Vec *ss, size_t ind);
static Str pp_stmts(Heap *h, const Vec *ss, size_t ind);
static Str pp_stmt(Heap *h, const AstStmt *s, size_t ind);
static Str pp_let(Heap *h, Str head, AstMaybeNode v, size_t ind);
static Str pp_if(Heap *h, const AstNode *c, const Vec *b, AstMaybeNode x, size_t ind);
static Str pp_else(Heap *h, AstMaybeNode x, size_t ind);
static Str pp_arms(Heap *h, const Vec *arms, size_t ind);
static Str pp_arm(Heap *h, Str head, const AstNode *b, size_t ind);
static Str pp_guard(Heap *h, AstMaybeNode g);
static Str pp_items(Heap *h, const Vec *ns, size_t ind);
static Str pp_item(Heap *h, const AstNode *n, size_t ind);
static Str pp_fn(Heap *h, Str head, const Vec *ps, AstMaybeTy r, const MaybeVec *body, size_t ind);
static Str pp_fn_v(Heap *h, Str head, const Vec *ps, AstMaybeTy r, size_t ind);
static Str pp_body(Heap *h, const MaybeVec *body, size_t ind);
static Str pp_sbody(Heap *h, const AstBody *b, size_t ind);
static Str pp_vbody(Heap *h, const AstBody *b, size_t ind, size_t w);
static Str pp_vflags(Heap *h, const Vec *vs, size_t ind);
static Str pp_nfields(Heap *h, const Vec *fs, size_t ind);
static Str pp_nfields_fl(Heap *h, const Vec *fs, bool first);
static Str pp_tfields(Heap *h, const Vec *fs, bool first);
static Str pp_variants(Heap *h, const Vec *vs, size_t ind, size_t w);
static Str pp_mod_body(Heap *h, const Vec *inner, const Vec *ns, size_t ind);
static Str pp_ibody(Heap *h, const Vec *ns, size_t ind);

/* The deferred alternative of JArgs (JVert) and JChain (JChainV). */
static Str pp_alt(Heap *h, const PrintAlt *alt) {
  switch (alt->kind) {
  case PRINT_ALT_VERT:
    return pp_vert(h, alt->head, alt->close, alt->args, alt->ind);
  case PRINT_ALT_CHAINV:
    return pp_chain_v(h, CAT(h, pp_ex(h, alt->root, alt->ind, alt->avail), print_qs(h, alt->rq)), alt->ls, 0,
                      alt->ind, alt->avail);
  }
  return str_empty();
}

/* ---- types ---- */

static Str pp_ty(Heap *h, const AstTy *t) {
  if (t == NULL) {
    return str_empty();
  }
  switch (t->kind) {
  case AST_TPATH:
    return pp_segs(h, &t->segs, false, true);
  case AST_TREF:
    return CAT(h, PL("&"), pp_ty(h, t->inner));
  case AST_TTUPLE:
    if (t->items.len == 1) {
      return CAT(h, PL("("), pp_ty(h, print_at(&t->items, 0)), PL(",)"));
    } else {
      return CAT(h, PL("("), pp_tys(h, &t->items, PL(", "), true), PL(")"));
    }
  case AST_TSLICE:
    return CAT(h, PL("["), pp_ty(h, t->inner), PL("]"));
  case AST_TINFER:
    return PL("_");
  case AST_TIMPL:
    return CAT(h, PL("impl "), pp_tys(h, &t->bounds, PL(" + "), true));
  case AST_TFN_SUGAR:
    return CAT(h, t->name, PL("("), pp_tys(h, &t->args, PL(", "), true), PL(")"), pp_ret(h, t->ret));
  }
  return str_empty();
}

static Str pp_tys(Heap *h, const Vec *ts, Str sep, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ts->len; i++) {
    print_push(&o, print_sep(i == 0 && first, sep));
    print_push(&o, pp_ty(h, print_at(ts, i)));
  }
  return print_done(&o);
}

static Str pp_ret(Heap *h, AstMaybeTy r) {
  return r.some ? CAT(h, PL(" -> "), pp_ty(h, r.value)) : str_empty();
}

static Str pp_ann(Heap *h, AstMaybeTy t) {
  return t.some ? CAT(h, PL(": "), pp_ty(h, t.value)) : str_empty();
}

static Str pp_eq_ty(Heap *h, AstMaybeTy t) {
  return t.some ? CAT(h, PL(" = "), pp_ty(h, t.value)) : str_empty();
}

static Str pp_turbo(Heap *h, const Vec *ts) {
  return ts->len == 0 ? str_empty() : CAT(h, PL("::<"), pp_tys(h, ts, PL(", "), true), PL(">"));
}

static Str pp_segs(Heap *h, const Vec *segs, bool turbo, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < segs->len; i++) {
    const AstSeg *s = print_at(segs, i);
    if (s != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL("::")));
      print_push(&o, s->name);
      if (s->args.len > 0) {
        print_push(&o, CAT(h, turbo ? PL("::<") : PL("<"), pp_tys(h, &s->args, PL(", "), true), PL(">")));
      }
    }
  }
  return print_done(&o);
}

/* ---- patterns ---- */

static Str pp_pat(Heap *h, const AstPat *p) {
  if (p == NULL) {
    return str_empty();
  }
  switch (p->kind) {
  case AST_PWILD:
    return PL("_");
  case AST_PPATH:
    return pp_segs(h, &p->segs, true, true);
  case AST_PTUPLE_STRUCT:
    return CAT(h, pp_segs(h, &p->segs, true, true), PL("("), pp_pats(h, &p->items, PL(", "), true), PL(")"));
  case AST_PSTRUCT:
    return CAT(h, pp_segs(h, &p->segs, true, true), pp_pstruct(h, &p->fields, p->rest));
  case AST_PTUPLE:
    if (p->items.len == 1) {
      return CAT(h, PL("("), pp_pat(h, print_at(&p->items, 0)), PL(",)"));
    } else {
      return CAT(h, PL("("), pp_pats(h, &p->items, PL(", "), true), PL(")"));
    }
  case AST_PLIT:
    return print_lit(h, &p->lit);
  case AST_PREF:
    return CAT(h, PL("&"), pp_pat(h, p->inner));
  case AST_POR:
    return pp_pats(h, &p->alts, PL(" | "), true);
  }
  return str_empty();
}

static Str pp_pats(Heap *h, const Vec *ps, Str sep, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ps->len; i++) {
    print_push(&o, print_sep(i == 0 && first, sep));
    print_push(&o, pp_pat(h, print_at(ps, i)));
  }
  return print_done(&o);
}

static Str pp_fpats(Heap *h, const Vec *fs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstFieldPat *f = print_at(fs, i);
    if (f != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      print_push(&o, f->name);
      print_push(&o, f->pat.some ? CAT(h, PL(": "), pp_pat(h, f->pat.value)) : str_empty());
    }
  }
  return print_done(&o);
}

static Str pp_pstruct(Heap *h, const Vec *fs, bool rest) {
  if (fs->len == 0) {
    return rest ? PL(" { .. }") : PL(" {}");
  }
  return CAT(h, PL(" { "), pp_fpats(h, fs, true), rest ? PL(", ..") : PL(""), PL(" }"));
}

/* ---- generics and parameters ---- */

static Str pp_gens(Heap *h, const Vec *gs) {
  return gs->len == 0 ? str_empty() : CAT(h, PL("<"), pp_glist(h, gs, true), PL(">"));
}

static Str pp_glist(Heap *h, const Vec *gs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < gs->len; i++) {
    const AstGParam *g = print_at(gs, i);
    if (g != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      print_push(&o, g->name);
      print_push(&o, g->bounds.len > 0 ? CAT(h, PL(": "), pp_tys(h, &g->bounds, PL(" + "), true)) : str_empty());
    }
  }
  return print_done(&o);
}

static Str pp_param(Heap *h, const AstParam *p) {
  if (p == NULL) {
    return str_empty();
  }
  switch (p->kind) {
  case AST_PSELF:
    return p->by_ref ? PL("&self") : PL("self");
  case AST_PTYPED:
    return CAT(h, pp_pat(h, p->pat), PL(": "), pp_ty(h, p->ty));
  }
  return str_empty();
}

static Str pp_params(Heap *h, const Vec *ps, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ps->len; i++) {
    print_push(&o, print_sep(i == 0 && first, PL(", ")));
    print_push(&o, pp_param(h, print_at(ps, i)));
  }
  return print_done(&o);
}

static Str pp_param_lines(Heap *h, const Vec *ps, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ps->len; i++) {
    print_push(&o, CAT(h, print_sp(h, ind), pp_param(h, print_at(ps, i)), PL(",\n")));
  }
  return print_done(&o);
}

static Str pp_cparams(Heap *h, const Vec *cs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < cs->len; i++) {
    const AstCParam *c = print_at(cs, i);
    if (c != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      print_push(&o, CAT(h, pp_pat(h, c->pat), pp_ann(h, c->ty)));
    }
  }
  return print_done(&o);
}

/* ---- use trees ---- */

static Str pp_use(Heap *h, const AstUseTree *u) {
  if (u == NULL) {
    return str_empty();
  }
  switch (u->kind) {
  case AST_UPATH:
    return CAT(h, u->seg, PL("::"), pp_use(h, u->sub));
  case AST_UNAME:
    return u->alias.some ? CAT(h, u->name, PL(" as "), u->alias.value) : u->name;
  case AST_UGLOB:
    return PL("*");
  case AST_UGROUP:
    return CAT(h, PL("{"), pp_uses(h, &u->items, true), PL("}"));
  }
  return str_empty();
}

static Str pp_uses(Heap *h, const Vec *us, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < us->len; i++) {
    print_push(&o, print_sep(i == 0 && first, PL(", ")));
    print_push(&o, pp_use(h, print_at(us, i)));
  }
  return print_done(&o);
}

/* ---- expressions, flat ---- */

static Str pp_fl(Heap *h, const AstNode *e) {
  if (e == NULL) {
    return str_empty();
  }
  switch (e->kind) {
  case AST_EPATH:
    return pp_segs(h, &e->segs, true, true);
  case AST_ELIT:
    return print_lit(h, &e->lit);
  case AST_ECALL:
    return CAT(h, pp_fl(h, e->func), PL("("), print_capped(h, pp_fls(h, &e->args, true), 60), PL(")"));
  case AST_EMETHOD: {
    PrintChain c = print_chain(h, e);
    return print_chain_mark(h,
                            CAT(h, pp_fl(h, e->recv), PL("."), e->name, pp_turbo(h, &e->generics), PL("("),
                                print_capped(h, pp_fls(h, &e->args, true), 60), PL(")")),
                            &c);
  }
  case AST_EFIELD: {
    PrintChain c = print_chain(h, e);
    return print_chain_mark(h, CAT(h, pp_fl(h, e->recv), PL("."), e->name), &c);
  }
  case AST_ETRY: {
    PrintChain c = print_chain(h, e);
    return print_chain_mark(h, CAT(h, pp_fl(h, e->inner), PL("?")), &c);
  }
  case AST_EUNARY:
    return CAT(h, e->op, pp_fl(h, e->inner));
  case AST_EBINARY:
    return CAT(h, pp_fl(h, e->lhs), PL(" "), e->op, PL(" "), pp_fl(h, e->rhs));
  case AST_ECLOSURE:
    return CAT(h, e->mv ? PL("move ") : PL(""), PL("|"), pp_cparams(h, &e->params, true), PL("|"), pp_ret(h, e->ret),
               PL(" "), pp_fl(h, e->body));
  case AST_EBLOCK:
    return e->stmts.len == 0 ? PL("{}") : PL("{\n}");
  case AST_EIF:
    return pp_if_flat(h, e->cond, &e->stmts, e->els);
  case AST_ELET:
    return CAT(h, PL("let "), pp_pat(h, e->pat), PL(" = "), pp_fl(h, e->init));
  case AST_EMATCH:
    return e->arms.len == 0 ? CAT(h, PL("match "), pp_fl(h, e->scrut), PL(" {}")) : PL("match\n");
  case AST_ESTRUCT:
    return CAT(h, pp_segs(h, &e->segs, true, true), pp_struct_fl(h, &e->fields, e->base));
  case AST_ETUPLE:
    if (e->items.len == 1) {
      return CAT(h, PL("("), pp_fl(h, print_at(&e->items, 0)), PL(",)"));
    } else {
      return CAT(h, PL("("), print_capped(h, pp_fls(h, &e->items, true), 60), PL(")"));
    }
  case AST_EPAREN:
    return CAT(h, PL("("), pp_fl(h, e->inner), PL(")"));
  case AST_EMACRO:
    return CAT(h, e->name, PL("!"), e->bracket ? PL("[") : PL("("), print_capped(h, pp_fls(h, &e->args, true), 60),
               e->bracket ? PL("]") : PL(")"));
  PRINT_ITEM_CASES
    return PL("item\n");
  }
  return str_empty();
}

static Str pp_fls(Heap *h, const Vec *es, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < es->len; i++) {
    print_push(&o, print_sep(i == 0 && first, PL(", ")));
    print_push(&o, pp_fl(h, print_at(es, i)));
  }
  return print_done(&o);
}

/* fls_sep: the first n expressions (Bend init_from), each followed by ", ". */
static Str pp_fls_sep(Heap *h, const Vec *es, size_t n) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < n && i < es->len; i++) {
    print_push(&o, pp_fl(h, print_at(es, i)));
    print_push(&o, PL(", "));
  }
  return print_done(&o);
}

/* The tail of a block that holds exactly one statement, an SExpr without `;`; NULL otherwise. */
static const AstNode *print_lone_tail(const Vec *ss) {
  const AstStmt *s = ss->len == 1 ? print_at(ss, 0) : NULL;
  return s != NULL && s->kind == AST_SEXPR && !s->semi ? s->expr : NULL;
}

static Str pp_if_flat(Heap *h, const AstNode *c, const Vec *b, AstMaybeNode x) {
  const AstNode *a = print_lone_tail(b);
  const AstNode *b2 =
      x.some && x.value != NULL && x.value->kind == AST_EBLOCK ? print_lone_tail(&x.value->stmts) : NULL;
  if (a == NULL || b2 == NULL) {
    return PL("if\n");
  }
  return print_capped(
      h, CAT(h, PL("if "), pp_fl(h, c), PL(" { "), pp_fl(h, a), PL(" } else { "), pp_fl(h, b2), PL(" }")), 50);
}

static Str pp_struct_fl(Heap *h, const Vec *fs, AstMaybeNode base) {
  if (fs->len == 0 && !base.some) {
    return PL(" {}");
  }
  return CAT(h, PL(" { "),
             print_capped(h, CAT(h, pp_inits_fl(h, fs, true), pp_base_fl(h, base, fs->len == 0)), 18), PL(" }"));
}

static Str pp_inits_fl(Heap *h, const Vec *fs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstFieldInit *f = print_at(fs, i);
    if (f != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      print_push(&o, f->name);
      print_push(&o, f->value.some ? CAT(h, PL(": "), pp_fl(h, f->value.value)) : str_empty());
    }
  }
  return print_done(&o);
}

static Str pp_base_fl(Heap *h, AstMaybeNode base, bool empty) {
  if (!base.some) {
    return str_empty();
  }
  return CAT(h, empty ? PL("..") : PL(", .."), pp_fl(h, base.value));
}

/* links_fl: the first n links (Bend init_links), flat. */
static Str pp_links_fl(Heap *h, const Vec *ls, size_t n) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < n && i < ls->len; i++) {
    const PrintLink *l = print_at(ls, i);
    if (l != NULL) {
      switch (l->kind) {
      case PRINT_LFIELD:
        print_push(&o, CAT(h, PL("."), l->name, print_qs(h, l->q)));
        break;
      case PRINT_LMETH:
        print_push(&o, CAT(h, PL("."), l->name, pp_turbo(h, l->gens), PL("("), pp_fls(h, l->args, true), PL(")"),
                           print_qs(h, l->q)));
        break;
      }
    }
  }
  return print_done(&o);
}

/* ---- expressions, broken over lines ---- */

static Str pp_ex(Heap *h, const AstNode *e, size_t ind, size_t avail) {
  MaybeStr f = print_fit(pp_fl(h, e), avail);
  if (f.some) {
    return f.value;
  }
  return pp_brk(h, e, ind, avail);
}

static Str pp_brk(Heap *h, const AstNode *e, size_t ind, size_t avail) {
  if (e == NULL) {
    return str_empty();
  }
  switch (e->kind) {
  case AST_ECALL:
    return pp_args(h, CAT(h, pp_fl(h, e->func), PL("(")), PL(")"), &e->args, ind, avail);
  case AST_EMACRO:
    return pp_args(h, CAT(h, e->name, PL("!"), e->bracket ? PL("[") : PL("(")), e->bracket ? PL("]") : PL(")"),
                   &e->args, ind, avail);
  case AST_EMETHOD:
  case AST_EFIELD:
  case AST_ETRY: {
    PrintChain c = print_chain(h, e);
    return pp_chain(h, &c, ind, avail);
  }
  case AST_ESTRUCT:
    return CAT(h, pp_segs(h, &e->segs, true, true), PL(" {\n"), pp_inits(h, &e->fields, print_ind4(ind)),
               pp_base(h, e->base, print_ind4(ind)), print_sp(h, ind), PL("}"));
  case AST_EBLOCK:
    return pp_block(h, &e->stmts, ind);
  case AST_EIF:
    return pp_if(h, e->cond, &e->stmts, e->els, ind);
  case AST_EMATCH:
    if (e->arms.len == 0) {
      return CAT(h, PL("match "), pp_fl(h, e->scrut), PL(" {}"));
    } else {
      return CAT(h, PL("match "), pp_fl(h, e->scrut), PL(" {\n"), pp_arms(h, &e->arms, print_ind4(ind)),
                 print_sp(h, ind), PL("}"));
    }
  case AST_ECLOSURE:
    return pp_cat(h,
                  CAT(h, e->mv ? PL("move ") : PL(""), PL("|"), pp_cparams(h, &e->params, true), PL("|"),
                      pp_ret(h, e->ret), PL(" ")),
                  e->body, ind, avail);
  case AST_EBINARY:
    return pp_cat(h, CAT(h, pp_fl(h, e->lhs), PL(" "), e->op, PL(" ")), e->rhs, ind, avail);
  case AST_EUNARY:
    return pp_cat(h, e->op, e->inner, ind, avail);
  case AST_ELET:
    return pp_cat(h, CAT(h, PL("let "), pp_pat(h, e->pat), PL(" = ")), e->init, ind, avail);
  case AST_ETUPLE:
    return pp_args(h, PL("("), PL(")"), &e->items, ind, avail);
  case AST_EPAREN:
    return CAT(h, pp_cat(h, PL("("), e->inner, ind, print_sub(avail, 1)), PL(")"));
  case AST_EPATH:
  case AST_ELIT:
  PRINT_ITEM_CASES
    return pp_fl(h, e);
  }
  return str_empty();
}

static Str pp_cat(Heap *h, Str head, const AstNode *e, size_t ind, size_t avail) {
  return CAT(h, head, pp_ex(h, e, ind, print_sub(avail, print_slen(head))));
}

static PrintAlt print_alt_vert(Str head, Str close, const Vec *args, size_t ind) {
  PrintAlt alt = {0};
  alt.kind = PRINT_ALT_VERT;
  alt.head = head;
  alt.close = close;
  alt.args = args;
  alt.ind = ind;
  return alt;
}

static PrintAlt print_alt_chainv(const AstNode *root, size_t rq, const Vec *ls, size_t ind, size_t avail) {
  PrintAlt alt = {0};
  alt.kind = PRINT_ALT_CHAINV;
  alt.root = root;
  alt.rq = rq;
  alt.ls = ls;
  alt.ind = ind;
  alt.avail = avail;
  return alt;
}

static Str pp_args(Heap *h, Str head, Str close, const Vec *args, size_t ind, size_t avail) {
  MaybeStr f = print_fit(CAT(h, head, print_capped(h, pp_fls(h, args, true), 60), close), avail);
  if (f.some) {
    return f.value;
  }
  PrintAlt alt = print_alt_vert(head, close, args, ind);
  return pp_ovf(h, head, close, args, ind, avail, &alt);
}

static Str pp_ovf(Heap *h, Str head, Str close, const Vec *args, size_t ind, size_t avail, const PrintAlt *alt) {
  size_t n = args->len;
  if (n == 0) {
    return pp_alt(h, alt);
  }
  const AstNode *last = print_at(args, n - 1);
  return pp_ovf3(h, CAT(h, head, pp_fls_sep(h, args, n - 1)), close, last, print_ovf(last, n == 1), ind, avail, alt);
}

/* ovf3 = ovf_pick(pre, ex(last, ...), close, ok, avail) else alt; the body is built only when ok and pre
 * is one line, the cases where ovf_pick can return Some. */
static Str pp_ovf3(Heap *h, Str pre, Str close, const AstNode *last, bool ok, size_t ind, size_t avail,
                   const PrintAlt *alt) {
  if (ok && print_one_line(pre)) {
    Str body = pp_ex(h, last, ind, print_sub(avail, print_slen(pre) + print_slen(close)));
    if (!print_one_line(body) && print_le(print_slen(pre) + print_flen(body), avail)) {
      return CAT(h, pre, body, close);
    }
  }
  return pp_alt(h, alt);
}

static Str pp_vert(Heap *h, Str head, Str close, const Vec *args, size_t ind) {
  return CAT(h, head, PL("\n"), pp_arg_lines(h, args, print_ind4(ind)), print_sp(h, ind), close);
}

static Str pp_arg_lines(Heap *h, const Vec *args, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < args->len; i++) {
    print_push(&o, CAT(h, print_sp(h, ind), pp_ex(h, print_at(args, i), ind, print_room(ind, 1)), PL(",\n")));
  }
  return print_done(&o);
}

static Str pp_chain(Heap *h, const PrintChain *c, size_t ind, size_t avail) {
  size_t budget = print_le(2, print_chain_cnt(c)) ? print_min(avail, 60) : avail;
  PrintAlt alt = print_alt_chainv(c->root, c->rq, &c->links, ind, avail);
  return pp_ch_ovf(h, CAT(h, pp_fl(h, c->root), print_qs(h, c->rq)), &c->links, ind, budget, &alt);
}

static Str pp_ch_ovf(Heap *h, Str pre, const Vec *ls, size_t ind, size_t budget, const PrintAlt *alt) {
  size_t n = ls->len;
  if (n == 0) {
    return pp_alt(h, alt);
  }
  return pp_ch_ovf2(h, CAT(h, pre, pp_links_fl(h, ls, n - 1)), print_at(ls, n - 1), ind, budget, alt);
}

static Str pp_ch_ovf2(Heap *h, Str pre, const PrintLink *l, size_t ind, size_t budget, const PrintAlt *alt) {
  if (l == NULL) {
    return pp_alt(h, alt);
  }
  switch (l->kind) {
  case PRINT_LFIELD:
    return pp_alt(h, alt);
  case PRINT_LMETH:
    return pp_ovf(h, CAT(h, pre, PL("."), l->name, pp_turbo(h, l->gens), PL("(")), CAT(h, PL(")"), print_qs(h, l->q)),
                  l->args, ind, budget, alt);
  }
  return str_empty();
}

static Str pp_chain_v(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind, size_t avail) {
  if (print_le(print_slen(rs), 4) && print_one_line(rs)) {
    return pp_absorb(h, rs, ls, from, ind, avail);
  }
  return pp_chain_v2(h, rs, ls, from, ind);
}

static Str pp_chain_v2(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind) {
  return CAT(h, rs, pp_links(h, ls, from, print_ind4(ind)));
}

static Str pp_absorb(Heap *h, Str rs, const Vec *ls, size_t from, size_t ind, size_t avail) {
  if (from >= ls->len) {
    return rs;
  }
  return pp_chain_v(h, CAT(h, rs, pp_link(h, print_at(ls, from), ind, print_sub(avail, print_slen(rs)))), ls,
                    from + 1, ind, avail);
}

static Str pp_links(Heap *h, const Vec *ls, size_t from, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = from; i < ls->len; i++) {
    print_push(&o, CAT(h, PL("\n"), print_sp(h, ind), pp_link(h, print_at(ls, i), ind, print_room(ind, 1))));
  }
  return print_done(&o);
}

static Str pp_link(Heap *h, const PrintLink *l, size_t ind, size_t avail) {
  if (l == NULL) {
    return str_empty();
  }
  switch (l->kind) {
  case PRINT_LFIELD:
    return CAT(h, PL("."), l->name, print_qs(h, l->q));
  case PRINT_LMETH:
    return pp_args(h, CAT(h, PL("."), l->name, pp_turbo(h, l->gens), PL("(")), CAT(h, PL(")"), print_qs(h, l->q)),
                   l->args, ind, avail);
  }
  return str_empty();
}

/* ---- statements ---- */

static Str pp_inits(Heap *h, const Vec *fs, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstFieldInit *f = print_at(fs, i);
    if (f != NULL && f->value.some) {
      print_push(&o, CAT(h, print_sp(h, ind), f->name, PL(": "),
                         pp_ex(h, f->value.value, ind, print_room(ind, print_slen(f->name) + 3)), PL(",\n")));
    } else if (f != NULL) {
      print_push(&o, CAT(h, print_sp(h, ind), f->name, PL(",\n")));
    }
  }
  return print_done(&o);
}

static Str pp_base(Heap *h, AstMaybeNode base, size_t ind) {
  if (!base.some) {
    return str_empty();
  }
  return CAT(h, print_sp(h, ind), PL(".."), pp_fl(h, base.value), PL("\n"));
}

static Str pp_block(Heap *h, const Vec *ss, size_t ind) {
  if (ss->len == 0) {
    return PL("{}");
  }
  return CAT(h, PL("{\n"), pp_stmts(h, ss, print_ind4(ind)), print_sp(h, ind), PL("}"));
}

static Str pp_stmts(Heap *h, const Vec *ss, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ss->len; i++) {
    print_push(&o, pp_stmt(h, print_at(ss, i), ind));
    print_push(&o, PL("\n"));
  }
  return print_done(&o);
}

static Str pp_stmt(Heap *h, const AstStmt *s, size_t ind) {
  if (s == NULL) {
    return str_empty();
  }
  switch (s->kind) {
  case AST_SLET:
    return CAT(h, print_meta(h, &s->meta, ind), print_sp(h, ind),
               pp_let(h, CAT(h, PL("let "), pp_pat(h, s->pat), pp_ann(h, s->ty)), s->init, ind));
  case AST_SEXPR:
    if (s->expr != NULL && s->expr->kind == AST_EIF) {
      return CAT(h, print_meta(h, &s->meta, ind), print_sp(h, ind),
                 pp_if(h, s->expr->cond, &s->expr->stmts, s->expr->els, ind), s->semi ? PL(";") : PL(""));
    } else {
      return CAT(h, print_meta(h, &s->meta, ind), print_sp(h, ind),
                 pp_ex(h, s->expr, ind, print_room(ind, s->semi ? 1 : 0)), s->semi ? PL(";") : PL(""));
    }
  case AST_SITEM:
    return pp_item(h, s->item, ind);
  }
  return str_empty();
}

static Str pp_let(Heap *h, Str head, AstMaybeNode v, size_t ind) {
  if (!v.some) {
    return CAT(h, head, PL(";"));
  }
  return CAT(h, head, PL(" = "), pp_ex(h, v.value, ind, print_room(ind, print_slen(head) + 4)), PL(";"));
}

static Str pp_if(Heap *h, const AstNode *c, const Vec *b, AstMaybeNode x, size_t ind) {
  return CAT(h, PL("if "), pp_fl(h, c), PL(" "), pp_block(h, b, ind), pp_else(h, x, ind));
}

static Str pp_else(Heap *h, AstMaybeNode x, size_t ind) {
  if (!x.some || x.value == NULL) {
    return str_empty();
  }
  if (x.value->kind == AST_EIF) {
    return CAT(h, PL(" else "), pp_if(h, x.value->cond, &x.value->stmts, x.value->els, ind));
  }
  if (x.value->kind == AST_EBLOCK) {
    return CAT(h, PL(" else "), pp_block(h, &x.value->stmts, ind));
  }
  return CAT(h, PL(" else "), pp_fl(h, x.value));
}

static Str pp_arms(Heap *h, const Vec *arms, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < arms->len; i++) {
    const AstArm *a = print_at(arms, i);
    if (a != NULL) {
      print_push(&o, CAT(h, print_meta(h, &a->meta, ind), print_sp(h, ind),
                         pp_arm(h, CAT(h, pp_pat(h, a->pat), pp_guard(h, a->guard), PL(" =>")), a->body, ind),
                         PL("\n")));
    }
  }
  return print_done(&o);
}

static Str pp_arm(Heap *h, Str head, const AstNode *b, size_t ind) {
  if (b != NULL && b->kind == AST_EBLOCK) {
    return CAT(h, head, PL(" "), pp_block(h, &b->stmts, ind));
  }
  return CAT(h, head, PL(" "), pp_ex(h, b, ind, print_room(ind, print_slen(head) + 2)), PL(","));
}

static Str pp_guard(Heap *h, AstMaybeNode g) {
  return g.some ? CAT(h, PL(" if "), pp_fl(h, g.value)) : str_empty();
}

/* ---- items ---- */

static Str pp_items(Heap *h, const Vec *ns, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < ns->len; i++) {
    print_push(&o, pp_item(h, print_at(ns, i), ind));
    print_push(&o, PL("\n"));
  }
  return print_done(&o);
}

/* meta(m, ind) sp(ind): the lead of every item. */
static Str print_lead(Heap *h, const AstNode *n, size_t ind) {
  return CAT(h, print_meta(h, &n->meta, ind), print_sp(h, ind));
}

static Str pp_item(Heap *h, const AstNode *n, size_t ind) {
  if (n == NULL) {
    return str_empty();
  }
  switch (n->kind) {
  case AST_IFN:
    return CAT(h, print_lead(h, n, ind),
               pp_fn(h, CAT(h, print_vis(n->vis), PL("fn "), n->sig.name, pp_gens(h, &n->sig.generics)),
                     &n->sig.params, n->sig.ret, &n->fn_body, ind));
  case AST_ISTRUCT:
    return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("struct "), n->name, pp_gens(h, &n->generics),
               pp_sbody(h, &n->struct_body, ind));
  case AST_IENUM:
    if (n->variants.len == 0) {
      return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("enum "), n->name, pp_gens(h, &n->generics),
                 PL(" {}"));
    } else {
      return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("enum "), n->name, pp_gens(h, &n->generics),
                 PL(" {\n"),
                 pp_variants(h, &n->variants, print_ind4(ind),
                             print_mixed(pp_vflags(h, &n->variants, print_ind4(ind)))),
                 print_sp(h, ind), PL("}"));
    }
  case AST_IUSE:
    return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("use "), pp_use(h, n->tree), PL(";"));
  case AST_IMOD:
    return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("mod "), n->name,
               n->is_inline ? pp_mod_body(h, &n->inner_docs, &n->items, ind) : PL(";"));
  case AST_IIMPL:
    if (n->tr.some) {
      return CAT(h, print_lead(h, n, ind), PL("impl"), pp_gens(h, &n->generics), PL(" "), pp_ty(h, n->tr.value),
                 PL(" for "), pp_ty(h, n->self_ty), pp_ibody(h, &n->items, ind));
    } else {
      return CAT(h, print_lead(h, n, ind), PL("impl"), pp_gens(h, &n->generics), PL(" "), pp_ty(h, n->self_ty),
                 pp_ibody(h, &n->items, ind));
    }
  case AST_ITRAIT:
    if (n->supers.len == 0) {
      return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("trait "), n->name, pp_gens(h, &n->generics),
                 pp_ibody(h, &n->items, ind));
    } else {
      return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("trait "), n->name, pp_gens(h, &n->generics),
                 PL(": "), pp_tys(h, &n->supers, PL(" + "), true), pp_ibody(h, &n->items, ind));
    }
  case AST_ICONST:
    return CAT(h, print_lead(h, n, ind),
               pp_let(h, CAT(h, print_vis(n->vis), PL("const "), n->name, PL(": "), pp_ty(h, n->ty)), n->value, ind));
  case AST_ITYPE:
    return CAT(h, print_lead(h, n, ind), print_vis(n->vis), PL("type "), n->name, pp_gens(h, &n->generics),
               pp_eq_ty(h, n->maybe_ty), PL(";"));
  case AST_EPATH:
  case AST_ELIT:
  case AST_ECALL:
  case AST_EMETHOD:
  case AST_EFIELD:
  case AST_ETRY:
  case AST_EUNARY:
  case AST_EBINARY:
  case AST_ECLOSURE:
  case AST_EBLOCK:
  case AST_EIF:
  case AST_ELET:
  case AST_EMATCH:
  case AST_ESTRUCT:
  case AST_ETUPLE:
  case AST_EPAREN:
  case AST_EMACRO:
    return pp_fl(h, n);
  }
  return str_empty();
}

static Str pp_fn(Heap *h, Str head, const Vec *ps, AstMaybeTy r, const MaybeVec *body, size_t ind) {
  MaybeStr f = print_fit(CAT(h, head, PL("("), pp_params(h, ps, true), PL(")"), pp_ret(h, r)),
                         print_room(ind, print_body_len(body)));
  return CAT(h, f.some ? f.value : pp_fn_v(h, head, ps, r, ind), pp_body(h, body, ind));
}

static Str pp_fn_v(Heap *h, Str head, const Vec *ps, AstMaybeTy r, size_t ind) {
  return CAT(h, head, PL("(\n"), pp_param_lines(h, ps, print_ind4(ind)), print_sp(h, ind), PL(")"), pp_ret(h, r));
}

static Str pp_body(Heap *h, const MaybeVec *body, size_t ind) {
  if (!body->some) {
    return PL(";");
  }
  if (body->value.len == 0) {
    return PL(" {}");
  }
  return CAT(h, PL(" "), pp_block(h, &body->value, ind));
}

static Str pp_sbody(Heap *h, const AstBody *b, size_t ind) {
  switch (b->kind) {
  case AST_BUNIT:
    return PL(";");
  case AST_BTUPLE:
    return CAT(h, PL("("), pp_tfields(h, &b->fields, true), PL(");"));
  case AST_BNAMED:
    if (b->fields.len == 0) {
      return PL(" {}");
    } else {
      return CAT(h, PL(" {\n"), pp_nfields(h, &b->fields, print_ind4(ind)), print_sp(h, ind), PL("}"));
    }
  }
  return str_empty();
}

static Str pp_vbody(Heap *h, const AstBody *b, size_t ind, size_t w) {
  switch (b->kind) {
  case AST_BUNIT:
    return PL("");
  case AST_BTUPLE:
    return CAT(h, PL("("), pp_tfields(h, &b->fields, true), PL(")"));
  case AST_BNAMED:
    if (b->fields.len == 0) {
      return PL(" {}");
    } else {
      MaybeStr f = print_fit(CAT(h, PL(" { "), print_capped(h, pp_nfields_fl(h, &b->fields, true), w), PL(" }")), 100);
      return f.some ? f.value : pp_sbody(h, b, ind);
    }
  }
  return str_empty();
}

static Str pp_vflags(Heap *h, const Vec *vs, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < vs->len; i++) {
    const AstVariant *v = print_at(vs, i);
    if (v != NULL) {
      print_push(&o, print_one_line(CAT(h, print_meta_mark(&v->meta), pp_vbody(h, &v->body, ind, 35))) ? PL("s")
                                                                                                        : PL("m"));
    }
  }
  return print_done(&o);
}

static Str pp_nfields(Heap *h, const Vec *fs, size_t ind) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstField *f = print_at(fs, i);
    if (f != NULL) {
      switch (f->kind) {
      case AST_NFIELD:
        print_push(&o, CAT(h, print_meta(h, &f->meta, ind), print_sp(h, ind), print_vis(f->vis), f->name, PL(": "),
                           pp_ty(h, f->ty), PL(",\n")));
        break;
      case AST_TFIELD:
        print_push(&o, CAT(h, print_meta(h, &f->meta, ind), print_sp(h, ind), print_vis(f->vis), pp_ty(h, f->ty),
                           PL(",\n")));
        break;
      }
    }
  }
  return print_done(&o);
}

static Str pp_nfields_fl(Heap *h, const Vec *fs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstField *f = print_at(fs, i);
    if (f != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      switch (f->kind) {
      case AST_NFIELD:
        print_push(&o, CAT(h, print_meta_mark(&f->meta), print_vis(f->vis), f->name, PL(": "), pp_ty(h, f->ty)));
        break;
      case AST_TFIELD:
        print_push(&o, CAT(h, print_meta_mark(&f->meta), print_vis(f->vis), pp_ty(h, f->ty)));
        break;
      }
    }
  }
  return print_done(&o);
}

static Str pp_tfields(Heap *h, const Vec *fs, bool first) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < fs->len; i++) {
    const AstField *f = print_at(fs, i);
    if (f != NULL) {
      print_push(&o, print_sep(i == 0 && first, PL(", ")));
      switch (f->kind) {
      case AST_TFIELD:
        print_push(&o, CAT(h, print_vis(f->vis), pp_ty(h, f->ty)));
        break;
      case AST_NFIELD:
        print_push(&o, CAT(h, print_vis(f->vis), f->name, PL(": "), pp_ty(h, f->ty)));
        break;
      }
    }
  }
  return print_done(&o);
}

static Str pp_variants(Heap *h, const Vec *vs, size_t ind, size_t w) {
  PrintOut o = print_out(h);
  for (size_t i = 0; i < vs->len; i++) {
    const AstVariant *v = print_at(vs, i);
    if (v != NULL) {
      print_push(&o, CAT(h, print_meta(h, &v->meta, ind), print_sp(h, ind), v->name, pp_vbody(h, &v->body, ind, w),
                         PL(",\n")));
    }
  }
  return print_done(&o);
}

static Str pp_mod_body(Heap *h, const Vec *inner, const Vec *ns, size_t ind) {
  if (inner->len == 0 && ns->len == 0) {
    return PL(" {}");
  }
  return CAT(h, PL(" {\n"), print_lines(h, inner, CAT(h, print_sp(h, print_ind4(ind)), PL("//!"))),
             pp_items(h, ns, print_ind4(ind)), print_sp(h, ind), PL("}"));
}

static Str pp_ibody(Heap *h, const Vec *ns, size_t ind) {
  if (ns->len == 0) {
    return PL(" {}");
  }
  return CAT(h, PL(" {\n"), pp_items(h, ns, print_ind4(ind)), print_sp(h, ind), PL("}"));
}

/* ---- file ---- */

Str print_file(Heap *heap, const AstSrcFile *f) {
  return CAT(heap, print_lines(heap, &f->inner, PL("//!")), pp_items(heap, &f->items, 0));
}
