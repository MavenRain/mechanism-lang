/* Port of rust/infer.bend: type inference of the Rust importer (mech-rust C3, CD8 and CD9).
 * Input: the resolved crate of resolve.c (names are known to resolve). Output: for each fn, its
 * mech telescope (an STerm) and the notes of its body. A note is the result of inference for one
 * node: NCtor gives the type arguments of an XCtor, NMatch gives the result type of an XMatch. The
 * notes are in pre-order of the body (a node before its children, children left to right; the
 * scrutinee of a match before its arms), so lower walks the body in the same order and takes one
 * note for each XCtor and XMatch. Types compare with TRef and TBox removed: lower drops both.
 *
 * The definitions keep the Bend names (prefix infer_) and the Bend order; the C only helpers stand
 * just before their first use. A Bend List is a Vec of pointers in the Bend list order (head
 * first); a Bend Maybe of a pointer is the pointer, NULL = None.
 *
 * D-218a: a Bend walker over a list of types (strips, teqs, substs, mtys) gives one result for each
 * element of the list, so the C list walker is a loop over a C only walker of one type
 * (infer_strip_of, infer_teq_of, infer_subst_of, infer_mty_of); the Bend `i <> rest` of a TBox or
 * a TRef is the walk of `i` then the walk of `rest`. */
#include "rust.h"
#include "surface.h"

/* C: the static strings of this file. */
static const Str infer_u32_name = {"u32", 3};
static const Str infer_nat_name = {"Nat", 3};
static const Str infer_mech_bool = {"MechBool", 8};
static const Str infer_skip_name = {"_", 1};
static const Str infer_nat_small = {"nat_small", 9};
static const Str infer_nat_add = {"nat_add", 7};

/* C: push one pointer; a failed push sets heap->oom. */
static void infer_push(Heap *heap, Vec *v, const void *x) {
  if (!vec_push(v, (void *)x)) heap->oom = true;
}

/* C: element i of v, or NULL when i is out of range. */
static const void *infer_at(const Vec *v, size_t i) {
  VecGet g = vec_get(v, i);
  return g.status == VEC_GET_OK ? g.value : NULL;
}

/* C: x <> xs as a new Vec (head first); xs is not changed. */
static Vec infer_cons(Heap *heap, const void *x, const Vec *xs) {
  Vec one = vec_new(heap->arena);
  infer_push(heap, &one, x);
  return heap_vec_concat(heap, &one, xs);
}

/* C: [x] */
static Vec infer_single(Heap *heap, const void *x) {
  Vec one = vec_new(heap->arena);
  infer_push(heap, &one, x);
  return one;
}

/* C: the tail of xs (all but the head) as a new Vec. */
static Vec infer_tail(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 1; i < xs->len; i++) infer_push(heap, &out, infer_at(xs, i));
  return out;
}

/* ---- types ---- */

/* u32() */
static const RirTy *infer_u32(Heap *heap) {
  return rir_tcon(heap, infer_u32_name, vec_new(heap->arena));
}

/* nat() */
static const RirTy *infer_nat(Heap *heap) {
  return rir_tcon(heap, infer_nat_name, vec_new(heap->arena));
}

/* one(ts): the head of a list of one type (the default TBool is never used). */
static const RirTy *infer_one(Heap *heap, const Vec *ts) {
  const RirTy *t = infer_at(ts, 0);
  return t ? t : rir_tbool(heap);
}

/* fn_of(ts): `ret <> args` back to the TFn. */
static const RirTy *infer_fn_of(Heap *heap, const Vec *ts) {
  const RirTy *r = infer_at(ts, 0);
  return r ? rir_tfn(heap, infer_tail(heap, ts), r) : rir_tbool(heap);
}

static Vec infer_strips(Heap *heap, const Vec *ts);

/* C: the strips walk of one type (D-218a). */
static const RirTy *infer_strip_of(Heap *heap, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return rir_tbool(heap);
    case RIR_TCON:
      return rir_tcon(heap, t->name, infer_strips(heap, &t->args));
    case RIR_TBOX:
      return infer_strip_of(heap, t->inner);
    case RIR_TREF:
      return infer_strip_of(heap, t->inner);
    case RIR_TFN: {
      Vec ra = infer_cons(heap, t->ret, &t->args);
      Vec s = infer_strips(heap, &ra);
      return infer_fn_of(heap, &s);
    }
  }
  return rir_tbool(heap);
}

/* strips(ts): each type with TRef and TBox removed. */
static Vec infer_strips(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) infer_push(heap, &out, infer_strip_of(heap, infer_at(ts, i)));
  return out;
}

/* strip(t) = one(strips([t])) */
const RirTy *infer_strip(Heap *heap, const RirTy *t) {
  Vec ts = infer_single(heap, t);
  Vec s = infer_strips(heap, &ts);
  return infer_one(heap, &s);
}

static bool infer_teqs(const Vec *xs, const Vec *ys);

/* C: the teqs walk of one pair of types (D-218a). */
static bool infer_teq_of(const RirTy *a, const RirTy *b) {
  switch (a->kind) {
    case RIR_TBOOL:
      return b->kind == RIR_TBOOL;
    case RIR_TCON:
      return b->kind == RIR_TCON && str_eq(a->name, b->name) && infer_teqs(&a->args, &b->args);
    case RIR_TBOX:
      return b->kind == RIR_TBOX && infer_teq_of(a->inner, b->inner);
    case RIR_TREF:
      return b->kind == RIR_TREF && infer_teq_of(a->inner, b->inner);
    case RIR_TFN:
      return b->kind == RIR_TFN && infer_teq_of(a->ret, b->ret) && infer_teqs(&a->args, &b->args);
  }
  return false;
}

/* teqs(xs, ys): the same length and each pair the same type. */
static bool infer_teqs(const Vec *xs, const Vec *ys) {
  if (xs->len != ys->len) return false;
  for (size_t i = 0; i < xs->len; i++) {
    if (!infer_teq_of(infer_at(xs, i), infer_at(ys, i))) return false;
  }
  return true;
}

/* teq(a, b) = teqs([a], [b]) */
static bool infer_teq(Heap *heap, const RirTy *a, const RirTy *b) {
  Vec xs = infer_single(heap, a);
  Vec ys = infer_single(heap, b);
  return infer_teqs(&xs, &ys);
}

/* pick_ty(xs, name, gens, args): the type of `args` at the place of `name` in `gens`, else
 * TCon{name, xs}. */
static const RirTy *infer_pick_ty(Heap *heap, Vec xs, Str name, const Vec *gens, const Vec *args) {
  size_t n = gens->len < args->len ? gens->len : args->len;
  for (size_t i = 0; i < n; i++) {
    const Str *g = infer_at(gens, i);
    if (str_eq(*g, name)) return infer_at(args, i);
  }
  return rir_tcon(heap, name, xs);
}

static Vec infer_substs(Heap *heap, const Vec *gens, const Vec *args, const Vec *ts);

/* C: the substs walk of one type (D-218a). */
static const RirTy *infer_subst_of(Heap *heap, const Vec *gens, const Vec *args, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return rir_tbool(heap);
    case RIR_TCON:
      return infer_pick_ty(heap, infer_substs(heap, gens, args, &t->args), t->name, gens, args);
    case RIR_TBOX:
      return rir_tbox(heap, infer_subst_of(heap, gens, args, t->inner));
    case RIR_TREF:
      return rir_tref(heap, infer_subst_of(heap, gens, args, t->inner));
    case RIR_TFN: {
      Vec rx = infer_cons(heap, t->ret, &t->args);
      Vec s = infer_substs(heap, gens, args, &rx);
      return infer_fn_of(heap, &s);
    }
  }
  return rir_tbool(heap);
}

/* substs(gens, args, ts): replace each generic of `gens` by the type at the same place in `args`. */
static Vec infer_substs(Heap *heap, const Vec *gens, const Vec *args, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    infer_push(heap, &out, infer_subst_of(heap, gens, args, infer_at(ts, i)));
  }
  return out;
}

/* subst(gens, args, t) = one(substs(gens, args, [t])) */
static const RirTy *infer_subst(Heap *heap, const Vec *gens, const Vec *args, const RirTy *t) {
  Vec ts = infer_single(heap, t);
  Vec s = infer_substs(heap, gens, args, &ts);
  return infer_one(heap, &s);
}

/* ---- telescopes (CD8) ---- */

/* qty(copies, t) */
Quantity infer_qty(const Vec *copies, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return QUANTITY_MANY;
    case RIR_TCON:
      if (erase_typed_has(copies, t->name)) return QUANTITY_MANY;
      return QUANTITY_ONE;
    case RIR_TBOX:
      return QUANTITY_ONE;
    case RIR_TREF:
      return QUANTITY_MANY;
    case RIR_TFN:
      return QUANTITY_MANY;
  }
  return QUANTITY_MANY;
}

/* apps(xs, h): `h` applied to each of `xs`, left to right (the list comes first: the termination
 * check reads the args left to right). */
const STerm *infer_apps(Heap *heap, const Vec *xs, const STerm *h) {
  const STerm *acc = h;
  for (size_t i = 0; i < xs->len; i++) acc = sterm_app(heap, acc, infer_at(xs, i));
  return acc;
}

/* arrows(copies, ts, ms, body): the arrows of a TFn; `ts` gives the quantities, `ms` the mech types
 * of the same arguments. */
static const STerm *infer_arrows(Heap *heap, const Vec *copies, const Vec *ts, const Vec *ms, const STerm *body) {
  size_t n = ts->len < ms->len ? ts->len : ms->len;
  const STerm *acc = body;
  for (size_t i = n; i > 0; i--) {
    const RirTy *t = infer_at(ts, i - 1);
    acc = sterm_arrow(heap, sbinder(infer_qty(copies, t), infer_skip_name, infer_at(ms, i - 1)), acc);
  }
  return acc;
}

/* fn_mty(copies, ts, ms): `ms` is the list of mech types of `ret <> ts`. */
static const STerm *infer_fn_mty(Heap *heap, const Vec *copies, const Vec *ts, const Vec *ms) {
  const STerm *b = infer_at(ms, 0);
  if (!b) return sterm_var(heap, infer_mech_bool);
  Vec mr = infer_tail(heap, ms);
  return infer_arrows(heap, copies, ts, &mr, b);
}

/* C: the mtys walk of one type (D-218a). */
static const STerm *infer_mty_of(Heap *heap, const Vec *copies, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return sterm_var(heap, infer_mech_bool);
    case RIR_TCON: {
      Vec ms = infer_mtys(heap, copies, &t->args);
      return infer_apps(heap, &ms, sterm_var(heap, t->name));
    }
    case RIR_TBOX:
      return infer_mty_of(heap, copies, t->inner);
    case RIR_TREF:
      return infer_mty_of(heap, copies, t->inner);
    case RIR_TFN: {
      Vec rx = infer_cons(heap, t->ret, &t->args);
      Vec ms = infer_mtys(heap, copies, &rx);
      return infer_fn_mty(heap, copies, &t->args, &ms);
    }
  }
  return sterm_var(heap, infer_mech_bool);
}

/* mtys(copies, ts): the mech type of each type. */
Vec infer_mtys(Heap *heap, const Vec *copies, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) infer_push(heap, &out, infer_mty_of(heap, copies, infer_at(ts, i)));
  return out;
}

/* one_m(ms) */
static const STerm *infer_one_m(Heap *heap, const Vec *ms) {
  const STerm *m = infer_at(ms, 0);
  return m ? m : sterm_var(heap, infer_mech_bool);
}

/* mty(copies, t) = one_m(mtys(copies, [t])) */
const STerm *infer_mty(Heap *heap, const Vec *copies, const RirTy *t) {
  Vec ts = infer_single(heap, t);
  Vec ms = infer_mtys(heap, copies, &ts);
  return infer_one_m(heap, &ms);
}

/* binder_name(name) = Et.fn_back(name) */
Str infer_binder_name(Heap *heap, Str name) {
  return erase_typed_fn_back(heap, name);
}

/* params_tele(copies, ps, body) */
static const STerm *infer_params_tele(Heap *heap, const Vec *copies, const Vec *ps, const STerm *body) {
  const STerm *acc = body;
  for (size_t i = ps->len; i > 0; i--) {
    const RirParam *p = infer_at(ps, i - 1);
    SBinder b = sbinder(infer_qty(copies, p->ty), infer_binder_name(heap, p->name), infer_mty(heap, copies, p->ty));
    acc = sterm_arrow(heap, b, acc);
  }
  return acc;
}

/* gens_tele(gs, body): one `(0 g : Type 0)` binder for each generic. */
static const STerm *infer_gens_tele(Heap *heap, const Vec *gs, const STerm *body) {
  const STerm *acc = body;
  for (size_t i = gs->len; i > 0; i--) {
    const Str *g = infer_at(gs, i - 1);
    acc = sterm_arrow(heap, sbinder(QUANTITY_ZERO, *g, sterm_type(heap, big_zero())), acc);
  }
  return acc;
}

/* telescope(copies, gs, ps, ret) */
static const STerm *infer_telescope(Heap *heap, const Vec *copies, const Vec *gs, const Vec *ps, const RirTy *ret) {
  return infer_gens_tele(heap, gs, infer_params_tele(heap, copies, ps, infer_mty(heap, copies, ret)));
}

/* ---- tables ---- */

/* param_tys(ps) */
static Vec infer_param_tys(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = infer_at(ps, i);
    infer_push(heap, &out, p->ty);
  }
  return out;
}

/* C: Sig{name, gens, params, ret} */
static const InferSig *infer_sig(Heap *heap, Str name, Vec gens, Vec params, const RirTy *ret) {
  InferSig *s = heap_alloc(heap, sizeof(InferSig));
  if (!s) return NULL;
  s->name = name;
  s->gens = gens;
  s->params = params;
  s->ret = ret;
  return s;
}

/* C: Fam{name, gens, variants} */
static const InferFam *infer_fam(Heap *heap, Str name, Vec gens, Vec variants) {
  InferFam *f = heap_alloc(heap, sizeof(InferFam));
  if (!f) return NULL;
  f->name = name;
  f->gens = gens;
  f->variants = variants;
  return f;
}

/* nat_sigs(): [Sig{"nat_small", [], [u32()], nat()}, Sig{"nat_add", [], [&Nat, &Nat], nat()}] */
Vec infer_nat_sigs(Heap *heap) {
  Vec out = vec_new(heap->arena);
  Vec small = infer_single(heap, infer_u32(heap));
  infer_push(heap, &out, infer_sig(heap, infer_nat_small, vec_new(heap->arena), small, infer_nat(heap)));
  Vec add = infer_single(heap, rir_tref(heap, infer_nat(heap)));
  infer_push(heap, &add, rir_tref(heap, infer_nat(heap)));
  infer_push(heap, &out, infer_sig(heap, infer_nat_add, vec_new(heap->arena), add, infer_nat(heap)));
  return out;
}

/* item_env(p, e) */
static InferEnv infer_item_env(Heap *heap, const ResolvePlaced *p, InferEnv e) {
  const RirItem *it = p->item;
  InferEnv out = e;
  switch (it->kind) {
    case RIR_IUSE:
      return out;
    case RIR_IENUM:
      out.fams = infer_cons(heap, infer_fam(heap, it->name, it->generics, it->variants), &e.fams);
      return out;
    case RIR_IUNIT: {
      Vec vs = infer_single(heap, rir_variant(heap, it->ctor, vec_new(heap->arena)));
      const RirTy *t = rir_tcon(heap, it->name, vec_new(heap->arena));
      out.sigs = infer_cons(heap, infer_sig(heap, it->ctor, vec_new(heap->arena), vec_new(heap->arena), t), &e.sigs);
      out.fams = infer_cons(heap, infer_fam(heap, it->name, vec_new(heap->arena), vs), &e.fams);
      return out;
    }
    case RIR_IFN:
      out.sigs = infer_cons(heap, infer_sig(heap, it->name, it->generics, infer_param_tys(heap, &it->params), it->ret), &e.sigs);
      return out;
  }
  return out;
}

/* items_env(ps, e) */
static InferEnv infer_items_env(Heap *heap, const Vec *ps, InferEnv e) {
  InferEnv acc = e;
  for (size_t i = 0; i < ps->len; i++) acc = infer_item_env(heap, infer_at(ps, i), acc);
  return acc;
}

/* mods_env(ms, e) */
InferEnv infer_mods_env(Heap *heap, const Vec *ms, InferEnv e) {
  InferEnv acc = e;
  for (size_t i = 0; i < ms->len; i++) {
    const ResolveMod *m = infer_at(ms, i);
    acc = infer_items_env(heap, &m->items, acc);
  }
  return acc;
}

/* item_copies(p, cs): a Copy enum and a unit struct are copies. */
static Vec infer_item_copies(Heap *heap, const ResolvePlaced *p, const Vec *cs) {
  const RirItem *it = p->item;
  switch (it->kind) {
    case RIR_IUSE:
      return *cs;
    case RIR_IENUM:
      if (it->copy) return infer_cons(heap, &it->name, cs);
      return *cs;
    case RIR_IUNIT:
      return infer_cons(heap, &it->name, cs);
    case RIR_IFN:
      return *cs;
  }
  return *cs;
}

/* items_copies(ps, cs) */
static Vec infer_items_copies(Heap *heap, const Vec *ps, const Vec *cs) {
  Vec acc = *cs;
  for (size_t i = 0; i < ps->len; i++) acc = infer_item_copies(heap, infer_at(ps, i), &acc);
  return acc;
}

/* mods_copies(ms, cs) */
Vec infer_mods_copies(Heap *heap, const Vec *ms, const Vec *cs) {
  Vec acc = *cs;
  for (size_t i = 0; i < ms->len; i++) {
    const ResolveMod *m = infer_at(ms, i);
    acc = infer_items_copies(heap, &m->items, &acc);
  }
  return acc;
}

/* find_sig(sigs, name): the first Sig named `name`, NULL = None. */
static const InferSig *infer_find_sig(const Vec *sigs, Str name) {
  for (size_t i = 0; i < sigs->len; i++) {
    const InferSig *s = infer_at(sigs, i);
    if (str_eq(s->name, name)) return s;
  }
  return NULL;
}

/* find_fam(fams, name): the first Fam named `name`, NULL = None. */
static const InferFam *infer_find_fam(const Vec *fams, Str name) {
  for (size_t i = 0; i < fams->len; i++) {
    const InferFam *f = infer_at(fams, i);
    if (str_eq(f->name, name)) return f;
  }
  return NULL;
}

/* find_loc(ls, name): the type of the first local named `name`, NULL = None. */
const RirTy *infer_find_loc(const Vec *ls, Str name) {
  for (size_t i = 0; i < ls->len; i++) {
    const InferLoc *l = infer_at(ls, i);
    if (str_eq(l->name, name)) return l->ty;
  }
  return NULL;
}

/* find_fields(vs, ctor): the field types of the first variant named `ctor`, NULL = None. */
static const Vec *infer_find_fields(const Vec *vs, Str ctor) {
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = infer_at(vs, i);
    if (str_eq(v->name, ctor)) return &v->fields;
  }
  return NULL;
}

/* is_generic(f): False only for a found Fam without generics. */
static bool infer_is_generic(const InferFam *f) {
  if (!f) return true;
  return f->gens.len != 0;
}

/* env_sigs(e) */
static const Vec *infer_env_sigs(const InferEnv *e) {
  return &e->sigs;
}

/* env_fams(e) */
static const Vec *infer_env_fams(const InferEnv *e) {
  return &e->fams;
}

/* env_gens(e) */
static const Vec *infer_env_gens(const InferEnv *e) {
  return &e->gens;
}

/* fn_env(e, gs): `e` with the generics of one fn. */
InferEnv infer_fn_env(InferEnv e, Vec gs) {
  InferEnv out = e;
  out.gens = gs;
  return out;
}

/* ---- the walker (CD9) ----
 *
 * D-219a: the types Job, St and Syn are in rust.h (lower uses them). St keeps the Bend list order:
 * jobs head first, notes the last note first; ok makes new Vecs with heap_vec_concat. walk keeps its
 * own job stack and note list (D-216b) and gives step an St with empty lists: step only puts jobs
 * and notes in front of the St lists, so the result is the same.
 * D-219b: a Bend value that is built again from the fields of a matched value (the XCtor of synth,
 * the Fam of match_jobs, the Variant of remove_variant) is the matched pointer; two equal type
 * values (the TCon of synth) are one pointer. Values are never changed, so the result is the same. */

/* C: Maybe<List<Job>> */
typedef struct {
  bool some;
  Vec jobs; /* some: InferJob pointers */
} InferMaybeJobs;

/* C: Result<String, List<Job>> */
typedef struct {
  bool ok;
  Str msg;  /* !ok */
  Vec jobs; /* ok: InferJob pointers */
} InferJobsResult;

/* C: JChk{ls, x, want} */
const InferJob *infer_jchk(Heap *heap, Vec ls, const RirEx *x, const RirTy *want) {
  InferJob *j = heap_alloc(heap, sizeof(InferJob));
  if (!j) return NULL;
  j->kind = INFER_JCHK;
  j->locals = ls;
  j->e = x;
  j->fam = NULL;
  j->args = vec_new(heap->arena);
  j->arm = NULL;
  j->want = want;
  return j;
}

/* C: JArm{ls, fam, args, arm, want} */
static const InferJob *infer_jarm(Heap *heap, Vec ls, const InferFam *fam, Vec args, const RirArm *arm, const RirTy *want) {
  InferJob *j = heap_alloc(heap, sizeof(InferJob));
  if (!j) return NULL;
  j->kind = INFER_JARM;
  j->locals = ls;
  j->e = NULL;
  j->fam = fam;
  j->args = args;
  j->arm = arm;
  j->want = want;
  return j;
}

/* C: Loc{name, ty} */
static const InferLoc *infer_loc(Heap *heap, Str name, const RirTy *ty) {
  InferLoc *l = heap_alloc(heap, sizeof(InferLoc));
  if (!l) return NULL;
  l->name = name;
  l->ty = ty;
  return l;
}

/* C: NCtor{targs} */
static const InferNote *infer_note_ctor(Heap *heap, Vec targs) {
  InferNote *n = heap_alloc(heap, sizeof(InferNote));
  if (!n) return NULL;
  n->kind = INFER_NCTOR;
  n->targs = targs;
  n->ret = NULL;
  return n;
}

/* C: NMatch{ret} */
static const InferNote *infer_note_match(Heap *heap, const RirTy *ret) {
  InferNote *n = heap_alloc(heap, sizeof(InferNote));
  if (!n) return NULL;
  n->kind = INFER_NMATCH;
  n->targs = vec_new(heap->arena);
  n->ret = ret;
  return n;
}

/* C: Fail{m} of Result<String, St> */
static InferStResult infer_st_fail(Heap *heap, Str m) {
  InferStResult r = {0};
  r.ok = false;
  r.msg = m;
  r.st.jobs = vec_new(heap->arena);
  r.st.notes = vec_new(heap->arena);
  return r;
}

/* C: Fail{m} of Result<String, Syn> */
static InferSynResult infer_syn_fail(Heap *heap, Str m) {
  InferSynResult r = {0};
  r.ok = false;
  r.msg = m;
  r.syn.ty = NULL;
  r.syn.jobs = vec_new(heap->arena);
  return r;
}

/* C: Done{Syn{t, js}} */
static InferSynResult infer_syn_done(const RirTy *t, Vec js) {
  InferSynResult r = {0};
  r.ok = true;
  r.syn.ty = t;
  r.syn.jobs = js;
  return r;
}

/* cons_job(j, m) */
static InferMaybeJobs infer_cons_job(Heap *heap, const InferJob *j, InferMaybeJobs m) {
  if (!m.some) return m;
  InferMaybeJobs out = {true, infer_cons(heap, j, &m.jobs)};
  return out;
}

/* C: chk_all over the pairs from index i on (the Bend recursion over the tails). */
static InferMaybeJobs infer_chk_all_from(Heap *heap, const Vec *ls, const Vec *es, const Vec *ts, size_t i) {
  InferMaybeJobs nil = {true, vec_new(heap->arena)};
  InferMaybeJobs none = {false, vec_new(heap->arena)};
  if (i >= es->len && i >= ts->len) return nil;
  if (i >= es->len || i >= ts->len) return none;
  const RirEx *x = infer_at(es, i);
  const RirTy *t = infer_strip(heap, infer_at(ts, i));
  const InferJob *j = infer_jchk(heap, *ls, x, t);
  InferMaybeJobs rest = infer_chk_all_from(heap, ls, es, ts, i + 1);
  return infer_cons_job(heap, j, rest);
}

/* chk_all(ls, es, ts): a JChk for each pair of an expression and a type, None when the counts differ. */
static InferMaybeJobs infer_chk_all(Heap *heap, const Vec *ls, const Vec *es, const Vec *ts) {
  return infer_chk_all_from(heap, ls, es, ts, 0);
}

/* mismatch() */
static Str infer_mismatch(void) {
  return str_lit("type mismatch (CD9)");
}

/* arity() */
static Str infer_arity(void) {
  return str_lit("argument count (CD9)");
}

/* no_want() */
static Str infer_no_want(void) {
  return str_lit("constructor of a generic family with no expected type (CD9)");
}

/* args_jobs(m) */
static InferJobsResult infer_args_jobs(InferMaybeJobs m) {
  InferJobsResult r = {0};
  r.ok = m.some;
  r.msg = m.some ? str_empty() : infer_arity();
  r.jobs = m.jobs;
  return r;
}

/* same_len(xs, ys) */
static bool infer_same_len(const Vec *xs, const Vec *ys) {
  return xs->len == ys->len;
}

/* type_arity() */
static Str infer_type_arity(void) {
  return str_lit("type argument count (CD9)");
}

/* fam_arity(args, name, f): `same_len(args, [])` is `args->len == 0`. */
static bool infer_fam_arity(const Vec *args, Str name, const InferFam *f) {
  if (f) return infer_same_len(args, &f->gens);
  return (str_eq(name, infer_nat_name) || str_eq(name, infer_u32_name)) && args->len == 0;
}

/* C: Et.has(gs, name): `name` is one of the Strs of `gs`. */
static bool infer_has(const Vec *gs, Str name) {
  for (size_t i = 0; i < gs->len; i++) {
    const Str *g = infer_at(gs, i);
    if (g && str_eq(*g, name)) return true;
  }
  return false;
}

static bool infer_valid_tys(const InferEnv *e, const Vec *gens, const Vec *ts);

/* C: the valid_tys walk of one type (D-218a). */
static bool infer_valid_ty_of(const InferEnv *e, const Vec *gens, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return true;
    case RIR_TCON: {
      /* T-202: the Str argument and the nested call are not in one argument list. */
      const InferFam *f = infer_find_fam(infer_env_fams(e), t->name);
      bool head = infer_has(gens, t->name) ? t->args.len == 0 : infer_fam_arity(&t->args, t->name, f);
      return head && infer_valid_tys(e, gens, &t->args);
    }
    case RIR_TBOX:
      return infer_valid_ty_of(e, gens, t->inner);
    case RIR_TREF:
      return infer_valid_ty_of(e, gens, t->inner);
    case RIR_TFN:
      return infer_valid_ty_of(e, gens, t->ret) && infer_valid_tys(e, gens, &t->args);
  }
  return false;
}

/* valid_tys(e, gens, ts): each type names a generic, Nat, u32 or a Fam, with the right count of
 * type arguments. */
static bool infer_valid_tys(const InferEnv *e, const Vec *gens, const Vec *ts) {
  for (size_t i = 0; i < ts->len; i++) {
    if (!infer_valid_ty_of(e, gens, infer_at(ts, i))) return false;
  }
  return true;
}

/* syn_of(t, r) */
static InferSynResult infer_syn_of(Heap *heap, const RirTy *t, InferJobsResult r) {
  if (!r.ok) return infer_syn_fail(heap, r.msg);
  return infer_syn_done(t, r.jobs);
}

/* call_sig(ls, targs, args, s): a call of a fn of the table: the result type and the jobs of the
 * arguments. */
static InferSynResult infer_call_sig(Heap *heap, const Vec *ls, const Vec *targs, const Vec *args, const InferSig *s) {
  if (!s) return infer_syn_fail(heap, str_lit("unknown function (CD9)"));
  if (!infer_same_len(targs, &s->gens)) return infer_syn_fail(heap, infer_arity());
  Vec ps = infer_substs(heap, &s->gens, targs, &s->params);
  const RirTy *r = infer_strip(heap, infer_subst(heap, &s->gens, targs, s->ret));
  InferMaybeJobs m = infer_chk_all(heap, ls, args, &ps);
  InferJobsResult js = infer_args_jobs(m);
  return infer_syn_of(heap, r, js);
}

/* call_local(ls, args, t) */
static InferSynResult infer_call_local(Heap *heap, const Vec *ls, const Vec *args, const RirTy *t) {
  switch (t->kind) {
    case RIR_TFN: {
      const RirTy *r = infer_strip(heap, t->ret);
      InferMaybeJobs m = infer_chk_all(heap, ls, args, &t->args);
      InferJobsResult js = infer_args_jobs(m);
      return infer_syn_of(heap, r, js);
    }
    case RIR_TBOOL:
    case RIR_TCON:
    case RIR_TBOX:
    case RIR_TREF:
      return infer_syn_fail(heap, str_lit("call of a local that is not a function (CD9)"));
  }
  return infer_syn_fail(heap, str_lit("call of a local that is not a function (CD9)"));
}

/* call(e, ls, func, targs, args, l): `l` = the type of a local named `func`, NULL = None. */
static InferSynResult infer_call(Heap *heap, const InferEnv *e, const Vec *ls, Str func, const Vec *targs, const Vec *args,
                                 const RirTy *l) {
  if (l && targs->len == 0) return infer_call_local(heap, ls, args, infer_strip(heap, l));
  if (l) return infer_syn_fail(heap, infer_arity());
  if (!infer_valid_tys(e, infer_env_gens(e), targs)) return infer_syn_fail(heap, infer_type_arity());
  const InferSig *s = infer_find_sig(infer_env_sigs(e), func);
  return infer_call_sig(heap, ls, targs, args, s);
}

/* var_syn(name, l) */
static InferSynResult infer_var_syn(Heap *heap, Str name, const RirTy *l) {
  if (l) {
    const RirTy *t = infer_strip(heap, l);
    Vec none = vec_new(heap->arena);
    return infer_syn_done(t, none);
  }
  Str a = str_lit("unknown local `");
  Str b = str_lit("` (CD9)");
  Str m = heap_str_cat(heap, a, name);
  Str full = heap_str_cat(heap, m, b);
  return infer_syn_fail(heap, full);
}

/* synth(e, ls, x): the type of `x` in a synthesis position (a scrutinee); D-219b for the XCtor. */
InferSynResult infer_synth(Heap *heap, const InferEnv *e, const Vec *ls, const RirEx *x) {
  switch (x->kind) {
    case RIR_XVAR: {
      const RirTy *l = infer_find_loc(ls, x->name);
      return infer_var_syn(heap, x->name, l);
    }
    case RIR_XCALL: {
      const RirTy *l = infer_find_loc(ls, x->func);
      return infer_call(heap, e, ls, x->func, &x->targs, &x->args, l);
    }
    case RIR_XCTOR: {
      const InferFam *f = infer_find_fam(infer_env_fams(e), x->family);
      if (infer_is_generic(f)) return infer_syn_fail(heap, infer_no_want());
      Vec none = vec_new(heap->arena);
      const RirTy *t = rir_tcon(heap, x->family, none);
      const InferJob *j = infer_jchk(heap, *ls, x, t);
      Vec js = infer_single(heap, j);
      return infer_syn_done(t, js);
    }
    case RIR_XBORROW:
      return infer_synth(heap, e, ls, x->inner);
    case RIR_XCLONE:
      return infer_synth(heap, e, ls, x->inner);
    case RIR_XBOX:
      return infer_synth(heap, e, ls, x->inner);
    case RIR_XINT:
    case RIR_XBOOL:
    case RIR_XMATCH:
    case RIR_XIF:
    case RIR_XCLOSURE:
    case RIR_XLET:
      return infer_syn_fail(heap, infer_no_want());
  }
  return infer_syn_fail(heap, infer_no_want());
}

/* ok(b, js, ns, st): St{js ++ rest, ns ++ acc} when `b`, else a type mismatch. */
static InferStResult infer_ok(Heap *heap, bool b, const Vec *js, const Vec *ns, const InferSt *st) {
  if (!b) return infer_st_fail(heap, infer_mismatch());
  InferStResult r = {0};
  r.ok = true;
  r.msg = str_empty();
  r.st.jobs = heap_vec_concat(heap, js, &st->jobs);
  r.st.notes = heap_vec_concat(heap, ns, &st->notes);
  return r;
}

/* got(want, r, st) */
static InferStResult infer_got(Heap *heap, const RirTy *want, InferSynResult r, const InferSt *st) {
  if (!r.ok) return infer_st_fail(heap, r.msg);
  Vec ns = vec_new(heap->arena);
  bool same = infer_teq(heap, r.syn.ty, want);
  return infer_ok(heap, same, &r.syn.jobs, &ns, st);
}

/* ctor_ok(wargs, r, st) */
static InferStResult infer_ctor_ok(Heap *heap, const Vec *wargs, InferJobsResult r, const InferSt *st) {
  if (!r.ok) return infer_st_fail(heap, r.msg);
  const InferNote *n = infer_note_ctor(heap, *wargs);
  Vec ns = infer_single(heap, n);
  return infer_ok(heap, true, &r.jobs, &ns, st);
}

/* ctor_fields(ls, wargs, gs, args, f, st): `f` = the field types of the constructor, NULL = None. */
static InferStResult infer_ctor_fields(Heap *heap, const Vec *ls, const Vec *wargs, const Vec *gs, const Vec *args, const Vec *f,
                                       const InferSt *st) {
  if (!f) return infer_st_fail(heap, str_lit("unknown constructor (CD9)"));
  if (!infer_same_len(wargs, gs)) return infer_st_fail(heap, infer_mismatch());
  Vec fs = infer_substs(heap, gs, wargs, f);
  InferMaybeJobs m = infer_chk_all(heap, ls, args, &fs);
  InferJobsResult js = infer_args_jobs(m);
  return infer_ctor_ok(heap, wargs, js, st);
}

/* ctor_jobs(ls, wargs, args, f, ctor, st) */
static InferStResult infer_ctor_jobs(Heap *heap, const Vec *ls, const Vec *wargs, const Vec *args, const InferFam *f, Str ctor,
                                     const InferSt *st) {
  if (!f) return infer_st_fail(heap, str_lit("unknown family (CD9)"));
  const Vec *fs = infer_find_fields(&f->variants, ctor);
  return infer_ctor_fields(heap, ls, wargs, &f->gens, args, fs, st);
}

/* arm_list(ls, fam, args, arms, want): one JArm for each arm, in the order of the arms. */
Vec infer_arm_list(Heap *heap, const Vec *ls, const InferFam *fam, const Vec *args, const Vec *arms, const RirTy *want) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = infer_at(arms, i);
    const InferJob *j = infer_jarm(heap, *ls, fam, *args, a, want);
    infer_push(heap, &out, j);
  }
  return out;
}

/* remove_variant(vs, ctor): `vs` without its first variant named `ctor` (D-219b). */
static Vec infer_remove_variant(Heap *heap, const Vec *vs, Str ctor) {
  Vec out = vec_new(heap->arena);
  bool gone = false;
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = infer_at(vs, i);
    if (!gone && str_eq(v->name, ctor)) {
      gone = true;
    } else {
      infer_push(heap, &out, v);
    }
  }
  return out;
}

/* found_fields(f) */
static bool infer_found_fields(const Vec *f) {
  return f != NULL;
}

/* covers(arms, vs, name): each arm names a variant of family `name` left in `vs`, and the arms use
 * up `vs`. The Bend recursion over the arms is a loop. */
static bool infer_covers(Heap *heap, const Vec *arms, const Vec *vs, Str name) {
  Vec remaining = *vs;
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = infer_at(arms, i);
    if (!str_eq(a->pat.family, name)) return false;
    const Vec *fs = infer_find_fields(&remaining, a->pat.ctor);
    if (!infer_found_fields(fs)) return false;
    remaining = infer_remove_variant(heap, &remaining, a->pat.ctor);
  }
  return remaining.len == 0;
}

/* match_jobs(ls, arms, want, f, args, js, st): D-219b for the Fam of the JArms. */
static InferStResult infer_match_jobs(Heap *heap, const Vec *ls, const Vec *arms, const RirTy *want, const InferFam *f,
                                      const Vec *args, const Vec *js, const InferSt *st) {
  if (!f) return infer_st_fail(heap, str_lit("match on a type that is not a family (CD9)"));
  if (!infer_covers(heap, arms, &f->variants, f->name)) return infer_st_fail(heap, str_lit("match arms (CD9)"));
  Vec al = infer_arm_list(heap, ls, f, args, arms, want);
  Vec all = heap_vec_concat(heap, js, &al);
  const InferNote *n = infer_note_match(heap, want);
  Vec ns = infer_single(heap, n);
  return infer_ok(heap, true, &all, &ns, st);
}

/* scrut_fam(e, t) */
const InferFam *infer_scrut_fam(const InferEnv *e, const RirTy *t) {
  switch (t->kind) {
    case RIR_TCON:
      return infer_find_fam(infer_env_fams(e), t->name);
    case RIR_TBOOL:
    case RIR_TBOX:
    case RIR_TREF:
    case RIR_TFN:
      return NULL;
  }
  return NULL;
}

/* scrut_args(t) */
Vec infer_scrut_args(Heap *heap, const RirTy *t) {
  switch (t->kind) {
    case RIR_TCON:
      return t->args;
    case RIR_TBOOL:
    case RIR_TBOX:
    case RIR_TREF:
    case RIR_TFN:
      return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

/* on_match(e, ls, arms, want, r, st) */
static InferStResult infer_on_match(Heap *heap, const InferEnv *e, const Vec *ls, const Vec *arms, const RirTy *want,
                                    InferSynResult r, const InferSt *st) {
  if (!r.ok) return infer_st_fail(heap, r.msg);
  Vec args = infer_scrut_args(heap, r.syn.ty);
  const InferFam *f = infer_scrut_fam(e, r.syn.ty);
  return infer_match_jobs(heap, ls, arms, want, f, &args, &r.syn.jobs, st);
}

/* binds(bs, fs, ls): a Loc in front of `ls` for each BVar with a field type, the last bind first;
 * the Bend recursion over the pairs is a loop. */
static Vec infer_binds(Heap *heap, const Vec *bs, const Vec *fs, const Vec *ls) {
  Vec out = *ls;
  for (size_t i = 0; i < bs->len && i < fs->len; i++) {
    const RirBind *b = infer_at(bs, i);
    switch (b->kind) {
      case RIR_BVAR: {
        const RirTy *t = infer_at(fs, i);
        const InferLoc *l = infer_loc(heap, b->name, t);
        out = infer_cons(heap, l, &out);
        break;
      }
      case RIR_BSKIP:
        break;
    }
  }
  return out;
}

/* bind_arity(bs, fs) */
static bool infer_bind_arity(const Vec *bs, const Vec *fs) {
  return bs->len == fs->len;
}

/* closure_locs(ps, ls): a Loc in front of `ls` for each param, the last param first. */
Vec infer_closure_locs(Heap *heap, const Vec *ps, const Vec *ls) {
  Vec out = *ls;
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = infer_at(ps, i);
    const InferLoc *l = infer_loc(heap, p->name, p->ty);
    out = infer_cons(heap, l, &out);
  }
  return out;
}

/* arm_step(ls, gs, args, body, want, bs, f, st): `f` = the field types of the constructor of the
 * pattern, NULL = None. */
static InferStResult infer_arm_step(Heap *heap, const Vec *ls, const Vec *gs, const Vec *args, const RirEx *body,
                                    const RirTy *want, const Vec *bs, const Vec *f, const InferSt *st) {
  if (!f) return infer_st_fail(heap, str_lit("unknown constructor (CD9)"));
  if (!infer_bind_arity(bs, f)) return infer_st_fail(heap, str_lit("pattern field count (CD9)"));
  Vec fs = infer_substs(heap, gs, args, f);
  Vec inner = infer_binds(heap, bs, &fs, ls);
  const InferJob *j = infer_jchk(heap, inner, body, want);
  Vec js = infer_single(heap, j);
  Vec ns = vec_new(heap->arena);
  return infer_ok(heap, true, &js, &ns, st);
}

/* let_syn(ls, name, value, body, want, r, st): a let with no type takes the type of its
 * initializer (E1, U13); an initializer with no synthesized type is a refusal. Lowering consumes
 * the two jobs as the two parts of Syntax.SLet; checking the initializer again also visits its
 * synthesized child jobs, so the jobs of `r` are dropped. */
static InferStResult infer_let_syn(Heap *heap, const Vec *ls, Str name, const RirEx *value, const RirEx *body, const RirTy *want,
                                   InferSynResult r, const InferSt *st) {
  if (!r.ok) {
    Str a = str_lit("let `");
    Str b = str_lit("` with no type (E1)");
    Str m = heap_str_cat(heap, a, name);
    Str full = heap_str_cat(heap, m, b);
    return infer_st_fail(heap, full);
  }
  const InferLoc *l = infer_loc(heap, name, r.syn.ty);
  Vec inner = infer_cons(heap, l, ls);
  const InferJob *jv = infer_jchk(heap, *ls, value, r.syn.ty);
  const InferJob *jb = infer_jchk(heap, inner, body, want);
  Vec js = vec_new(heap->arena);
  infer_push(heap, &js, jv);
  infer_push(heap, &js, jb);
  Vec ns = vec_new(heap->arena);
  return infer_ok(heap, true, &js, &ns, st);
}

/* C: the JChk cases of step. `case other` of the Bend step = a shape that is not listed (an XCtor
 * with a want that is not a TCon, an XClosure with a want that is not a TFn) -> a type mismatch. */
static InferStResult infer_step_chk(Heap *heap, const InferEnv *e, const InferJob *j, const InferSt *st) {
  const RirEx *x = j->e;
  const RirTy *want = j->want;
  const Vec *ls = &j->locals;
  if (!x || !want) return infer_st_fail(heap, infer_mismatch());
  Vec none = vec_new(heap->arena);
  switch (x->kind) {
    case RIR_XVAR: {
      const RirTy *l = infer_find_loc(ls, x->name);
      InferSynResult r = infer_var_syn(heap, x->name, l);
      return infer_got(heap, want, r, st);
    }
    case RIR_XINT: {
      const RirTy *u = infer_u32(heap);
      bool same = infer_teq(heap, u, want);
      return infer_ok(heap, same, &none, &none, st);
    }
    case RIR_XBOOL: {
      const RirTy *b = rir_tbool(heap);
      bool same = infer_teq(heap, b, want);
      return infer_ok(heap, same, &none, &none, st);
    }
    case RIR_XCALL: {
      const RirTy *l = infer_find_loc(ls, x->func);
      InferSynResult r = infer_call(heap, e, ls, x->func, &x->targs, &x->args, l);
      return infer_got(heap, want, r, st);
    }
    case RIR_XCTOR: {
      if (want->kind != RIR_TCON) return infer_st_fail(heap, infer_mismatch());
      if (!str_eq(x->family, want->name)) return infer_st_fail(heap, infer_mismatch());
      const InferFam *f = infer_find_fam(infer_env_fams(e), x->family);
      return infer_ctor_jobs(heap, ls, &want->args, &x->args, f, x->ctor, st);
    }
    case RIR_XMATCH: {
      InferSynResult r = infer_synth(heap, e, ls, x->scrut);
      return infer_on_match(heap, e, ls, &x->arms, want, r, st);
    }
    case RIR_XIF: {
      const RirTy *b = rir_tbool(heap);
      const InferJob *jc = infer_jchk(heap, *ls, x->cond, b);
      const InferJob *jy = infer_jchk(heap, *ls, x->yes, want);
      const InferJob *jn = infer_jchk(heap, *ls, x->no, want);
      Vec js = vec_new(heap->arena);
      infer_push(heap, &js, jc);
      infer_push(heap, &js, jy);
      infer_push(heap, &js, jn);
      return infer_ok(heap, true, &js, &none, st);
    }
    case RIR_XCLOSURE: {
      if (want->kind != RIR_TFN) return infer_st_fail(heap, infer_mismatch());
      Vec pt = infer_param_tys(heap, &x->params);
      Vec spt = infer_strips(heap, &pt);
      bool same = infer_teqs(&spt, &want->args);
      Vec inner = infer_closure_locs(heap, &x->params, ls);
      const InferJob *jb = infer_jchk(heap, inner, x->body, want->ret);
      Vec js = infer_single(heap, jb);
      return infer_ok(heap, same, &js, &none, st);
    }
    case RIR_XBORROW: {
      const InferJob *ji = infer_jchk(heap, *ls, x->inner, want);
      Vec js = infer_single(heap, ji);
      return infer_ok(heap, true, &js, &none, st);
    }
    case RIR_XCLONE: {
      const InferJob *ji = infer_jchk(heap, *ls, x->inner, want);
      Vec js = infer_single(heap, ji);
      return infer_ok(heap, true, &js, &none, st);
    }
    case RIR_XBOX: {
      const InferJob *ji = infer_jchk(heap, *ls, x->inner, want);
      Vec js = infer_single(heap, ji);
      return infer_ok(heap, true, &js, &none, st);
    }
    case RIR_XLET: {
      if (x->ty.some) {
        const RirTy *t = x->ty.value;
        const InferLoc *l = infer_loc(heap, x->name, t);
        Vec inner = infer_cons(heap, l, ls);
        const InferJob *jv = infer_jchk(heap, *ls, x->value, t);
        const InferJob *jb = infer_jchk(heap, inner, x->body, want);
        Vec js = vec_new(heap->arena);
        infer_push(heap, &js, jv);
        infer_push(heap, &js, jb);
        return infer_ok(heap, true, &js, &none, st);
      }
      InferSynResult r = infer_synth(heap, e, ls, x->value);
      return infer_let_syn(heap, ls, x->name, x->value, x->body, want, r, st);
    }
  }
  return infer_st_fail(heap, infer_mismatch());
}

/* step(e, j, st): the result of one job on the state `st` (the jobs and notes of the job in front). */
InferStResult infer_step(Heap *heap, const InferEnv *e, const InferJob *j, const InferSt *st) {
  if (!j) return infer_st_fail(heap, infer_mismatch());
  switch (j->kind) {
    case INFER_JARM: {
      const InferFam *f = j->fam;
      const RirArm *a = j->arm;
      if (!f || !a) return infer_st_fail(heap, infer_mismatch());
      const Vec *fs = infer_find_fields(&f->variants, a->pat.ctor);
      return infer_arm_step(heap, &j->locals, &f->gens, &j->args, a->body, j->want, &a->pat.binds, fs, st);
    }
    case INFER_JCHK:
      return infer_step_chk(heap, e, j, st);
  }
  return infer_st_fail(heap, infer_mismatch());
}

/* rev(xs, acc): `xs` reversed in front of `acc`. */
static Vec infer_rev(Heap *heap, const Vec *xs, const Vec *acc) {
  Vec out = vec_new(heap->arena);
  for (size_t k = xs->len; k > 0; k--) infer_push(heap, &out, infer_at(xs, k - 1));
  for (size_t i = 0; i < acc->len; i++) infer_push(heap, &out, infer_at(acc, i));
  return out;
}

/* C: push the jobs `js` (head first) on the stack, the head last (D-216b). */
static void infer_front(Heap *heap, Vec *stack, const Vec *js) {
  for (size_t k = js->len; k > 0; k--) infer_push(heap, stack, infer_at(js, k - 1));
}

/* walk(e, r): run the jobs of `r` in order; the notes in pre-order. The Bend recursion is a loop
 * over a job stack (D-216b) that calls step with St{[], []} (D-219a): step only puts its jobs and
 * notes in front of the St lists, so the job stack takes the jobs of step from last to first and
 * the notes list takes rev(ns, []) of each step, which equals rev(acc, []) of the Bend walk. */
InferNotesResult infer_walk(Heap *heap, const InferEnv *e, InferStResult r) {
  InferNotesResult out = {0};
  out.ok = false;
  out.msg = r.msg;
  out.notes = vec_new(heap->arena);
  if (!r.ok) return out;
  Vec none = vec_new(heap->arena);
  InferSt empty = {none, none};
  Vec stack = vec_new(heap->arena);
  infer_front(heap, &stack, &r.st.jobs);
  Vec notes = infer_rev(heap, &r.st.notes, &none);
  while (stack.len > 0 && !heap->oom) {
    VecGet g = vec_last(&stack);
    if (g.status != VEC_GET_OK) break;
    stack.len -= 1;
    const InferJob *j = g.value;
    InferStResult s = infer_step(heap, e, j, &empty);
    if (!s.ok) {
      out.msg = s.msg;
      return out;
    }
    infer_front(heap, &stack, &s.st.jobs);
    Vec rn = infer_rev(heap, &s.st.notes, &none);
    for (size_t i = 0; i < rn.len; i++) infer_push(heap, &notes, infer_at(&rn, i));
  }
  if (heap->oom) {
    out.msg = str_lit("out of memory");
    return out;
  }
  out.ok = true;
  out.msg = str_empty();
  out.notes = notes;
  return out;
}

/* ---- the crate ---- */

/* C: Result<Rs.Bad, Maybe<Typed>>; typed NULL = None. */
typedef struct {
  bool ok;
  ResolveBad bad;           /* !ok */
  const InferTyped *typed;  /* ok */
} InferMaybeTypedResult;

/* C: Done{m} of Result<Rs.Bad, Maybe<Typed>> */
static InferMaybeTypedResult infer_maybe_done(const InferTyped *t) {
  InferMaybeTypedResult out = {0};
  out.ok = true;
  out.typed = t;
  return out;
}

/* C: Fail{Rs.Bad{module, pos, what}} of Result<Rs.Bad, Maybe<Typed>> */
static InferMaybeTypedResult infer_maybe_fail(Str module, AstPos pos, Str what) {
  InferMaybeTypedResult out = {0};
  out.ok = false;
  out.bad.module = module;
  out.bad.pos = pos;
  out.bad.what = what;
  out.typed = NULL;
  return out;
}

/* typed_of(module, pos, name, tele, r) */
static InferMaybeTypedResult infer_typed_of(Heap *heap, Str module, AstPos pos, Str name, const STerm *tele, InferNotesResult r) {
  if (!r.ok) return infer_maybe_fail(module, pos, r.msg);
  Str mech = infer_binder_name(heap, name);
  InferTyped *t = heap_alloc(heap, sizeof(InferTyped));
  if (!t) return infer_maybe_done(NULL);
  t->module = module;
  t->rust = name;
  t->mech = mech;
  t->pos = pos;
  t->tele = tele;
  t->notes = r.notes;
  return infer_maybe_done(t);
}

/* fn_typed(e, copies, module, pos, name, gs, ps, rt, body) */
static InferMaybeTypedResult infer_fn_typed(Heap *heap, const InferEnv *e, const Vec *copies, Str module, AstPos pos, Str name,
                                            const Vec *gs, const Vec *ps, const RirTy *rt, const RirEx *body) {
  const STerm *tele = infer_telescope(heap, copies, gs, ps, rt);
  InferEnv fe = infer_fn_env(*e, *gs);
  Vec none = vec_new(heap->arena);
  Vec ls = infer_closure_locs(heap, ps, &none);
  const RirTy *want = infer_strip(heap, rt);
  const InferJob *j = infer_jchk(heap, ls, body, want);
  InferStResult start = {0};
  start.ok = true;
  start.msg = str_empty();
  start.st.jobs = infer_single(heap, j);
  start.st.notes = vec_new(heap->arena);
  InferNotesResult r = infer_walk(heap, &fe, start);
  return infer_typed_of(heap, module, pos, name, tele, r);
}

/* variant_tys(vs): the field types of each variant, in order. */
static Vec infer_variant_tys(Heap *heap, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = infer_at(vs, i);
    if (!v) continue;
    for (size_t k = 0; k < v->fields.len; k++) infer_push(heap, &out, infer_at(&v->fields, k));
  }
  return out;
}

/* item_typed(e, copies, module, p) */
static InferMaybeTypedResult infer_item_typed(Heap *heap, const InferEnv *e, const Vec *copies, Str module, const ResolvePlaced *p) {
  const RirItem *it = p->item;
  switch (it->kind) {
    case RIR_IFN: {
      Vec pt = infer_param_tys(heap, &it->params);
      Vec ts = infer_cons(heap, it->ret, &pt);
      bool valid = infer_valid_tys(e, &it->generics, &ts);
      if (valid) return infer_fn_typed(heap, e, copies, module, p->pos, it->name, &it->generics, &it->params, it->ret, it->body);
      return infer_maybe_fail(module, p->pos, infer_type_arity());
    }
    case RIR_IENUM: {
      Vec vt = infer_variant_tys(heap, &it->variants);
      bool valid = infer_valid_tys(e, &it->generics, &vt);
      if (valid) return infer_maybe_done(NULL);
      return infer_maybe_fail(module, p->pos, infer_type_arity());
    }
    case RIR_IUSE:
    case RIR_IUNIT:
      return infer_maybe_done(NULL);
  }
  return infer_maybe_done(NULL);
}

/* keep(m, rest): `m <> rest` for Some, else `rest`. */
static Vec infer_keep(Heap *heap, const InferTyped *m, const Vec *rest) {
  if (m) return infer_cons(heap, m, rest);
  return *rest;
}

/* keep_done(m, r) */
static InferTypedResult infer_keep_done(Heap *heap, const InferTyped *m, InferTypedResult r) {
  if (!r.ok) return r;
  InferTypedResult out = r;
  out.typed = infer_keep(heap, m, &r.typed);
  return out;
}

/* keep_item(m, r): the refusal of this item before one of a later item. */
static InferTypedResult infer_keep_item(Heap *heap, InferMaybeTypedResult m, InferTypedResult r) {
  if (m.ok) return infer_keep_done(heap, m.typed, r);
  InferTypedResult out = {0};
  out.ok = false;
  out.bad = m.bad;
  out.typed = vec_new(heap->arena);
  return out;
}

/* append_done(ts, r) */
static InferTypedResult infer_append_done(Heap *heap, const Vec *ts, InferTypedResult r) {
  if (!r.ok) return r;
  InferTypedResult out = r;
  out.typed = heap_vec_concat(heap, ts, &r.typed);
  return out;
}

/* append_typed(m, r) */
static InferTypedResult infer_append_typed(Heap *heap, InferTypedResult m, InferTypedResult r) {
  if (m.ok) return infer_append_done(heap, &m.typed, r);
  return m;
}

/* C: Done{[]} of Result<Rs.Bad, List<Typed>> */
static InferTypedResult infer_typed_nil(Heap *heap) {
  InferTypedResult out = {0};
  out.ok = true;
  out.typed = vec_new(heap->arena);
  return out;
}

/* items_typed(ps, e, copies, module): the Bend right recursion is a loop from the last item to
 * the first, so each keep_item sees the result of the later items (the first refusal wins). */
static InferTypedResult infer_items_typed(Heap *heap, const Vec *ps, const InferEnv *e, const Vec *copies, Str module) {
  InferTypedResult r = infer_typed_nil(heap);
  for (size_t k = ps->len; k > 0; k--) {
    const ResolvePlaced *p = infer_at(ps, k - 1);
    if (!p) continue;
    InferMaybeTypedResult m = infer_item_typed(heap, e, copies, module, p);
    r = infer_keep_item(heap, m, r);
  }
  return r;
}

/* mods_typed(ms, e, copies): a loop from the last module to the first, as items_typed. */
static InferTypedResult infer_mods_typed(Heap *heap, const Vec *ms, const InferEnv *e, const Vec *copies) {
  InferTypedResult r = infer_typed_nil(heap);
  for (size_t k = ms->len; k > 0; k--) {
    const ResolveMod *m = infer_at(ms, k - 1);
    if (!m) continue;
    InferTypedResult t = infer_items_typed(heap, &m->items, e, copies, m->name);
    r = infer_append_typed(heap, t, r);
  }
  return r;
}

/* infer(o): the Typed of each fn of the crate; the first refusal wins. */
InferTypedResult infer_infer(Heap *heap, const ResolveOut *o) {
  Vec nat = infer_nat_sigs(heap);
  InferEnv e0 = {nat, vec_new(heap->arena), vec_new(heap->arena)};
  InferEnv e = infer_mods_env(heap, &o->mods, e0);
  Vec none = vec_new(heap->arena);
  Vec copies = infer_mods_copies(heap, &o->mods, &none);
  return infer_mods_typed(heap, &o->mods, &e, &copies);
}

/* C: append s; a failed append sets heap->oom. */
static void infer_buf_push(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* types_text(ts): `<module> <mech> : <tele> annotated <note count>` and a newline for each Typed
 * (one StrBuf, the same bytes as the Bend right fold). */
Str infer_types_text(Heap *heap, const Vec *ts) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const InferTyped *t = infer_at(ts, i);
    if (!t) continue;
    Str tele = syntax_show(heap, t->tele);
    Nat n = nat_of_u64(heap->arena, (uint64_t)t->notes.len);
    Str count = heap_nat_show(heap, n);
    infer_buf_push(heap, &buf, t->module);
    infer_buf_push(heap, &buf, str_lit(" "));
    infer_buf_push(heap, &buf, t->mech);
    infer_buf_push(heap, &buf, str_lit(" : "));
    infer_buf_push(heap, &buf, tele);
    infer_buf_push(heap, &buf, str_lit(" annotated "));
    infer_buf_push(heap, &buf, count);
    infer_buf_push(heap, &buf, str_lit("\n"));
  }
  return strbuf_str(&buf);
}
