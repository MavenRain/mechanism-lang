/* Port of bend2/rust/lower.bend: lower of the Rust importer (mech-rust C4, CD6, CD7, CD10, CD11).
 * Input: the resolved crate of resolve.c, after infer.c gave no refusal. Output: one `.mech` text
 * for each module in manifest order (the `nat` module has no file, CD7), printed with
 * syntax_print (CD11, C-LEARN L4). Every expression retains its checking type and local typing
 * context. `lower` drops XBorrow, XClone, XBox, TBox and TRef (CD8); the emitter makes them again.
 *
 * The definitions keep the Bend names (prefix lower_) and the Bend order; the C only helpers stand
 * just before their first use. A Bend List is a Vec of pointers in the Bend list order (head
 * first); a Bend Maybe of a pointer is the pointer, NULL = None. List<String> is a Vec of Str
 * pointers. */
#include "rust.h"
#include "surface.h"

/* C: the static strings of this file. */
static const Str lower_mech_bool = {"MechBool", 8};
static const Str lower_nat_add = {"nat_add", 7};
static const Str lower_nat_add_mech = {"natAdd", 6};
static const Str lower_nat_small = {"nat_small", 9};
static const Str lower_local = {"Local", 5};
static const Str lower_skip = {"_", 1};
static const Str lower_self = {"self", 4};
static const Str lower_mech_false = {"mechFalse", 9};
static const Str lower_mech_true = {"mechTrue", 8};
static Str lower_str_placeholder = {"", 0};

/* C: push one pointer; a failed push sets heap->oom. */
static void lower_push(Heap *heap, Vec *v, const void *x) {
  if (!vec_push(v, (void *)x)) heap->oom = true;
}

/* C: element i of v, or NULL when i is out of range. */
static const void *lower_at(const Vec *v, size_t i) {
  VecGet g = vec_get(v, i);
  return g.status == VEC_GET_OK ? g.value : NULL;
}

/* C: x <> xs as a new Vec (head first); xs is not changed. */
static Vec lower_cons(Heap *heap, const void *x, const Vec *xs) {
  Vec one = vec_new(heap->arena);
  lower_push(heap, &one, x);
  return heap_vec_concat(heap, &one, xs);
}

/* C: a Str on the heap, for a List<String> cell; a failed allocation sets heap->oom. */
static const Str *lower_str_box(Heap *heap, Str s) {
  Str *p = heap_alloc(heap, sizeof *p);
  if (p == NULL) {
    heap->oom = true;
    return &lower_str_placeholder;
  }
  *p = s;
  return p;
}

/* C: the Str of a List<String> cell; a NULL cell is "". */
static Str lower_str_at(const Vec *v, size_t i) {
  const Str *p = lower_at(v, i);
  return p == NULL ? str_empty() : *p;
}

/* type Low */
/* LowerLow {file, text} is public in rust.h. */

/* C: Low{file, text} on the heap; NULL when the allocation fails (heap_alloc sets heap->oom). */
static const LowerLow *lower_low_new(Heap *heap, Str file, Str text) {
  LowerLow *p = heap_alloc(heap, sizeof *p);
  if (p == NULL) return NULL;
  p->file = file;
  p->text = text;
  return p;
}

/* type0() = SType{0} */
static const STerm *lower_type0(Heap *heap) {
  return sterm_type(heap, big_zero());
}

/* one_t(ts): the head of a list of one term (the default is never used). */
static const STerm *lower_one_t(Heap *heap, const Vec *ts) {
  const STerm *t = lower_at(ts, 0);
  return t == NULL ? sterm_var(heap, lower_mech_bool) : t;
}

/* nat_lit(m) */
static const STerm *lower_nat_lit(Heap *heap, MaybeBig m) {
  if (m.some) return sterm_nat(heap, m.value);
  return sterm_nat(heap, big_zero());
}

/* head_name(func): CD7, nat_add is natAdd, nat_small(<n>) is the literal <n>. */
static Str lower_head_name(Heap *heap, Str func) {
  if (str_eq(func, lower_nat_add)) return lower_nat_add_mech;
  return infer_binder_name(heap, func);
}

/* call_t(func, head, ms, ts, local): type arguments first, then the arguments. */
static const STerm *lower_call_t(Heap *heap, Str func, Str head, const Vec *ms, const Vec *ts, bool local) {
  if (str_eq(func, lower_nat_small) && !local) return lower_one_t(heap, ts);
  Vec all = heap_vec_concat(heap, ms, ts);
  return infer_apps(heap, &all, sterm_var(heap, head));
}

/* is_word(s) */
static bool lower_is_word(Str s) {
  return s.len != 0;
}

/* under_rest(s): the emitter writes an unused local binder `x` as `_x`; `_x` lowers to `x`. */
static Str lower_under_rest(Str s) {
  if (s.len == 0) return s;
  Str rest = str_n(s.data + 1, s.len - 1);
  if (s.data[0] == '_' && lower_is_word(rest)) return rest;
  return s;
}

/* identifier(k, name) */
static Str lower_identifier(Heap *heap, TokenKind k, Str name) {
  if (k.tag == TOKEN_IDENT) return name;
  return heap_str_cat(heap, name, lower_local);
}

/* local_name(name) */
static Str lower_local_name(Heap *heap, Str name) {
  Str candidate = infer_binder_name(heap, lower_under_rest(name));
  return lower_identifier(heap, lexer_ident_kind(candidate), candidate);
}

/* type Name: one injective map for all local spellings in a function. Repeated spellings still
 * shadow normally. Fresh names also avoid globals and generic binders. */
typedef struct {
  Str rust;
  Str mech;
} LowerName;

/* lookup(ns, raw): ns holds LowerName pointers. */
static Str lower_lookup(Heap *heap, const Vec *ns, Str raw) {
  for (size_t i = 0; i < ns->len; i++) {
    const LowerName *n = lower_at(ns, i);
    if (n != NULL && str_eq(n->rust, raw)) return n->mech;
  }
  return lower_local_name(heap, raw);
}

/* named(ns, raw) */
static bool lower_named(const Vec *ns, Str raw) {
  for (size_t i = 0; i < ns->len; i++) {
    const LowerName *n = lower_at(ns, i);
    if (n != NULL && str_eq(n->rust, raw)) return true;
  }
  return false;
}

/* has(ss, s) */
static bool lower_has(const Vec *ss, Str s) {
  for (size_t i = 0; i < ss->len; i++) {
    if (str_eq(lower_str_at(ss, i), s)) return true;
  }
  return false;
}

/* fresh(fuel, used, s): one step of fuel for each element of fuel. */
static Str lower_fresh(Heap *heap, const Vec *fuel, const Vec *used, Str s) {
  Str acc = s;
  for (size_t i = 0; i < fuel->len; i++) {
    if (!lower_has(used, acc)) return acc;
    acc = heap_str_cat(heap, acc, lower_local);
  }
  return acc;
}

/* C: a LowerName on the heap; a failed allocation sets heap->oom and gives NULL. */
static const LowerName *lower_name_new(Heap *heap, Str rust, Str mech) {
  LowerName *n = heap_alloc(heap, sizeof *n);
  if (n == NULL) {
    heap->oom = true;
    return NULL;
  }
  n->rust = rust;
  n->mech = mech;
  return n;
}

/* name_map(raws, used, ns): raws are Str pointers, used Str pointers, ns LowerName pointers. */
static Vec lower_name_map(Heap *heap, const Vec *raws, const Vec *used, const Vec *ns) {
  Vec u = *used;
  Vec n = *ns;
  for (size_t i = 0; i < raws->len; i++) {
    Str raw = lower_str_at(raws, i);
    Str m = lower_fresh(heap, &u, &u, lower_local_name(heap, raw));
    if (!lower_named(&n, raw)) {
      const LowerName *cell = lower_name_new(heap, raw, m);
      u = lower_cons(heap, lower_str_box(heap, m), &u);
      if (cell != NULL) n = lower_cons(heap, cell, &n);
    }
  }
  return n;
}

/* param_names(ps): ps holds RirParam pointers. */
static Vec lower_param_names(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = lower_at(ps, i);
    if (p != NULL) lower_push(heap, &out, lower_str_box(heap, p->name));
  }
  return out;
}

/* C: bind_names of bs, pushed onto out. */
static void lower_bind_names_into(Heap *heap, Vec *out, const Vec *bs) {
  for (size_t i = 0; i < bs->len; i++) {
    const RirBind *b = lower_at(bs, i);
    if (b == NULL) continue;
    switch (b->kind) {
      case RIR_BVAR:
        lower_push(heap, out, lower_str_box(heap, b->name));
        break;
      case RIR_BSKIP:
        break;
    }
  }
}

/* bind_names(bs): bs holds RirBind pointers. */
static Vec lower_bind_names(Heap *heap, const Vec *bs) {
  Vec out = vec_new(heap->arena);
  lower_bind_names_into(heap, &out, bs);
  return out;
}

/* arm_names(arms): arms holds RirArm pointers. */
static Vec lower_arm_names(Heap *heap, const Vec *arms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = lower_at(arms, i);
    if (a != NULL) lower_bind_names_into(heap, &out, &a->pat.binds);
  }
  return out;
}

/* field_of(b, ns): a BSkip is a binder with no use. */
static const SField *lower_field_of(Heap *heap, const RirBind *b, const Vec *ns) {
  MaybeSTerm none = {false, NULL};
  switch (b->kind) {
    case RIR_BVAR:
      return sfield_new(heap, sfield(QUANTITY_MANY, lower_lookup(heap, ns, b->name), none));
    case RIR_BSKIP:
      return sfield_new(heap, sfield(QUANTITY_MANY, lower_skip, none));
  }
  return sfield_new(heap, sfield(QUANTITY_MANY, lower_skip, none));
}

/* fields_of(bs, ns): SField pointers. */
static Vec lower_fields_of(Heap *heap, const Vec *bs, const Vec *ns) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < bs->len; i++) {
    const RirBind *b = lower_at(bs, i);
    if (b != NULL) lower_push(heap, &out, lower_field_of(heap, b, ns));
  }
  return out;
}

/* arm_bodies(arms): RirEx pointers. */
static Vec lower_arm_bodies(Heap *heap, const Vec *arms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = lower_at(arms, i);
    if (a != NULL) lower_push(heap, &out, a->body);
  }
  return out;
}

/* branches(arms, ts, ns): `ts` are the lowered arm bodies, in the order of the arms; the zip
 * stops at the shorter list. SBranch pointers. */
static Vec lower_branches(Heap *heap, const Vec *arms, const Vec *ts, const Vec *ns) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < arms->len && i < ts->len; i++) {
    const RirArm *a = lower_at(arms, i);
    const STerm *t = lower_at(ts, i);
    if (a == NULL || t == NULL) break;
    Vec fields = lower_fields_of(heap, &a->pat.binds, ns);
    lower_push(heap, &out, sbranch_ctor_new(heap, erase_typed_lower_first(heap, a->pat.ctor), fields, t));
  }
  return out;
}

/* motive_of(family, w): a `case` needs a motive (`as self in <family> return <type>`, as in
 * prelude/init.mech): the return type is the expected type of the match. w NULL = None. */
static MaybeSMotive lower_motive_of(Heap *heap, Str family, const STerm *w) {
  MaybeSMotive m;
  m.some = false;
  m.value = smotive(lower_self, (MaybeStr){false, str_empty()}, vec_new(heap->arena), w);
  if (w == NULL) return m;
  m.some = true;
  m.value = smotive(lower_self, (MaybeStr){true, family}, vec_new(heap->arena), w);
  return m;
}

/* match_t(arms, ts, m, ns): `ts` is the lowered scrutinee, then the lowered arm bodies. */
static const STerm *lower_match_t(Heap *heap, const Vec *arms, const Vec *ts, MaybeSMotive m, const Vec *ns) {
  const STerm *s = lower_at(ts, 0);
  if (s == NULL) return sterm_var(heap, lower_mech_bool);
  Vec rest = vec_new(heap->arena);
  for (size_t i = 1; i < ts->len; i++) lower_push(heap, &rest, lower_at(ts, i));
  return sterm_case(heap, s, m, lower_branches(heap, arms, &rest, ns));
}

/* if_t(ts, m): `ts` is the condition, the yes branch and the no branch (CD6). */
static const STerm *lower_if_t(Heap *heap, const Vec *ts, MaybeSMotive m) {
  const STerm *c = lower_at(ts, 0);
  const STerm *y = lower_at(ts, 1);
  const STerm *n = lower_at(ts, 2);
  if (c == NULL || y == NULL || n == NULL) return sterm_var(heap, lower_mech_bool);
  Vec bs = vec_new(heap->arena);
  lower_push(heap, &bs, sbranch_ctor_new(heap, lower_mech_false, vec_new(heap->arena), n));
  lower_push(heap, &bs, sbranch_ctor_new(heap, lower_mech_true, vec_new(heap->arena), y));
  return sterm_case(heap, c, m, bs);
}

/* ty_family(t): the family of a type (a reference or a box is its inner type). */
static Str lower_ty_family(const RirTy *t) {
  const RirTy *cur = t;
  while (cur != NULL) {
    switch (cur->kind) {
      case RIR_TCON:
        return cur->name;
      case RIR_TREF:
        cur = cur->inner;
        break;
      case RIR_TBOX:
        cur = cur->inner;
        break;
      case RIR_TBOOL:
        return lower_mech_bool;
      case RIR_TFN:
        return lower_mech_bool;
    }
  }
  return lower_mech_bool;
}

/* type Job: the same checking jobs as inference, including each binder's local scope. */
typedef struct {
  Vec locals; /* InferLoc pointers, head first */
  const RirEx *ex;
  const RirTy *want;
} LowerJob;

/* C: Job{ls, x, w} on the heap; a failed allocation sets heap->oom and gives NULL. */
static const LowerJob *lower_job_new(Heap *heap, Vec ls, const RirEx *x, const RirTy *w) {
  LowerJob *j = heap_alloc(heap, sizeof *j);
  if (j == NULL) {
    heap->oom = true;
    return NULL;
  }
  j->locals = ls;
  j->ex = x;
  j->want = w;
  return j;
}

/* step_jobs(r): all failures here are excluded by the successful In.infer precondition. */
static Vec lower_step_jobs(Heap *heap, InferStResult r) {
  if (r.ok) return r.st.jobs;
  return vec_new(heap->arena);
}

/* C: In.step(e, j, In.St{[], []}) */
static InferStResult lower_step_empty(Heap *heap, const InferEnv *e, const InferJob *j) {
  InferSt st;
  st.jobs = vec_new(heap->arena);
  st.notes = vec_new(heap->arena);
  return infer_step(heap, e, j, &st);
}

/* C: push the elements of xs from the last to the first, so the head of xs is the top (the last
 * element) of the stack. */
static void lower_push_rev(Heap *heap, Vec *stack, const Vec *xs) {
  for (size_t i = xs->len; i > 0; i--) lower_push(heap, stack, lower_at(xs, i - 1));
}

/* C: remove and give the top of a non-empty stack. */
static const void *lower_pop(Vec *stack) {
  const void *top = lower_at(stack, stack->len - 1);
  stack->len -= 1;
  return top;
}

/* jobs_of(e, js): js holds InferJob pointers, the result LowerJob pointers. A JChk is a Job; any
 * other job is replaced by its own step jobs, in front of the rest. The Bend recursion is a loop
 * over a stack of jobs (the head of the Bend list is the top). */
static Vec lower_jobs_of(Heap *heap, const InferEnv *e, const Vec *js) {
  Vec out = vec_new(heap->arena);
  Vec stack = vec_new(heap->arena);
  lower_push_rev(heap, &stack, js);
  while (stack.len > 0 && !heap->oom) {
    const InferJob *j = lower_pop(&stack);
    if (j == NULL) continue;
    switch (j->kind) {
      case INFER_JCHK:
        const LowerJob *nj = lower_job_new(heap, j->locals, j->e, j->want);
        lower_push(heap, &out, nj);
        break;
      case INFER_JARM: {
        InferStResult r = lower_step_empty(heap, e, j);
        Vec next = lower_step_jobs(heap, r);
        lower_push_rev(heap, &stack, &next);
        break;
      }
    }
  }
  return out;
}

/* children(e, ls, x, w) */
static Vec lower_children(Heap *heap, const InferEnv *e, const Vec *ls, const RirEx *x, const RirTy *w) {
  const InferJob *j = infer_jchk(heap, *ls, x, w);
  if (j == NULL) {
    heap->oom = true;
    return vec_new(heap->arena);
  }
  InferStResult r = lower_step_empty(heap, e, j);
  Vec js = lower_step_jobs(heap, r);
  return lower_jobs_of(heap, e, &js);
}

/* match_fam_jobs(e, ls, scrut, arms, want, t, f): f NULL = None. */
static Vec lower_match_fam_jobs(Heap *heap, const InferEnv *e, const Vec *ls, const RirEx *scrut, const Vec *arms,
                                const RirTy *want, const RirTy *t, const InferFam *f) {
  if (f == NULL) return vec_new(heap->arena);
  Vec args = infer_scrut_args(heap, t);
  Vec arm_jobs = infer_arm_list(heap, ls, f, &args, arms, want);
  Vec rest = lower_jobs_of(heap, e, &arm_jobs);
  const LowerJob *head = lower_job_new(heap, *ls, scrut, t);
  return lower_cons(heap, head, &rest);
}

/* match_jobs(e, ls, scrut, arms, want, r) */
static Vec lower_match_jobs(Heap *heap, const InferEnv *e, const Vec *ls, const RirEx *scrut, const Vec *arms,
                            const RirTy *want, InferSynResult r) {
  if (r.ok) {
    const RirTy *t = r.syn.ty;
    const InferFam *f = infer_scrut_fam(e, t);
    return lower_match_fam_jobs(heap, e, ls, scrut, arms, want, t, f);
  }
  return vec_new(heap->arena);
}

/* scrut_type(js): the expected type of the first job (TBool when there is none). */
static const RirTy *lower_scrut_type(Heap *heap, const Vec *js) {
  const LowerJob *j = lower_at(js, 0);
  if (j == NULL) return rir_tbool(heap);
  return j->want;
}

/* is_local(m): m NULL = None. */
static bool lower_is_local(const RirTy *m) {
  return m != NULL;
}

/* C: an SBinder on the heap; a failed allocation sets heap->oom. */
static const SBinder *lower_binder_new(Heap *heap, SBinder b) {
  const SBinder *p = sbinder_new(heap, b);
  if (p == NULL) heap->oom = true;
  return p;
}

/* param_binders(ps, copies, ns): SBinder pointers. */
static Vec lower_param_binders(Heap *heap, const Vec *ps, const Vec *copies, const Vec *ns) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = lower_at(ps, i);
    if (p == NULL) continue;
    Quantity q = infer_qty(copies, p->ty);
    Str name = lower_lookup(heap, ns, p->name);
    const STerm *ty = infer_mty(heap, copies, p->ty);
    SBinder b = sbinder(q, name, ty);
    const SBinder *pb = lower_binder_new(heap, b);
    lower_push(heap, &out, pb);
  }
  return out;
}

/* gen_binders(gs): gs holds Str pointers; SBinder pointers. */
static Vec lower_gen_binders(Heap *heap, const Vec *gs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    Str g = lower_str_at(gs, i);
    const STerm *ty = lower_type0(heap);
    SBinder b = sbinder(QUANTITY_ZERO, g, ty);
    const SBinder *pb = lower_binder_new(heap, b);
    lower_push(heap, &out, pb);
  }
  return out;
}

/* fun_of(bs, body): a fn with no binder is its body. */
static const STerm *lower_fun_of(Heap *heap, const Vec *bs, const STerm *body) {
  if (bs->len == 0) return body;
  return sterm_fun(heap, *bs, body);
}

/* let_t(name, ty, ts): a let. The first child job is the initializer with its (given or
 * synthesized) type, the second is the body (E1). */
static const STerm *lower_let_t(Heap *heap, Str name, const STerm *ty, const Vec *ts) {
  const STerm *v = lower_at(ts, 0);
  const STerm *b = lower_at(ts, 1);
  if (v != NULL && b != NULL) return sterm_let(heap, name, ty, v, b);
  return lower_one_t(heap, ts);
}

static Vec lower_lows(Heap *heap, const InferEnv *e, const Vec *copies, const Vec *ns, const Vec *js);

/* C: the lowered term of one Job{ls, x, w} of lows. XBorrow, XClone and XBox give the Job of
 * their inner expression with the same locals and type. A rebuilt R.Ex of the Bend is x (D-219b). */
static const STerm *lower_low_job(Heap *heap, const InferEnv *e, const Vec *copies, const Vec *ns, const Vec *ls,
                                  const RirEx *x0, const RirTy *w) {
  const RirEx *x = x0;
  while (x != NULL) {
    switch (x->kind) {
      case RIR_XVAR: {
        Str n = lower_lookup(heap, ns, x->name);
        return sterm_var(heap, n);
      }
      case RIR_XINT: {
        MaybeBig m = big_of_decimal(heap, x->text);
        return lower_nat_lit(heap, m);
      }
      case RIR_XBOOL: {
        Str n = lower_mech_false;
        if (x->boolean) n = lower_mech_true;
        return sterm_var(heap, n);
      }
      case RIR_XCALL: {
        bool local = lower_is_local(infer_find_loc(ls, x->func));
        Str head;
        if (local) {
          head = lower_lookup(heap, ns, x->func);
        } else {
          head = lower_head_name(heap, x->func);
        }
        Vec ms = infer_mtys(heap, copies, &x->targs);
        Vec kids = lower_children(heap, e, ls, x, w);
        Vec ts = lower_lows(heap, e, copies, ns, &kids);
        return lower_call_t(heap, x->func, head, &ms, &ts, local);
      }
      case RIR_XCTOR: {
        Vec kids = lower_children(heap, e, ls, x, w);
        Vec ts = lower_lows(heap, e, copies, ns, &kids);
        Str c = erase_typed_lower_first(heap, x->ctor);
        const STerm *h = sterm_var(heap, c);
        const STerm *t = infer_apps(heap, &ts, h);
        const STerm *ty = infer_mty(heap, copies, w);
        return sterm_ann(heap, t, ty);
      }
      case RIR_XMATCH: {
        InferSynResult r = infer_synth(heap, e, ls, x->scrut);
        Vec jobs = lower_match_jobs(heap, e, ls, x->scrut, &x->arms, w, r);
        Vec ts = lower_lows(heap, e, copies, ns, &jobs);
        const RirTy *st = lower_scrut_type(heap, &jobs);
        Str fam = lower_ty_family(st);
        const STerm *wt = infer_mty(heap, copies, w);
        MaybeSMotive m = lower_motive_of(heap, fam, wt);
        return lower_match_t(heap, &x->arms, &ts, m, ns);
      }
      case RIR_XIF: {
        Vec kids = lower_children(heap, e, ls, x, w);
        Vec ts = lower_lows(heap, e, copies, ns, &kids);
        const STerm *wt = infer_mty(heap, copies, w);
        MaybeSMotive m = lower_motive_of(heap, lower_mech_bool, wt);
        return lower_if_t(heap, &ts, m);
      }
      case RIR_XCLOSURE: {
        Vec bs = lower_param_binders(heap, &x->params, copies, ns);
        Vec kids = lower_children(heap, e, ls, x, w);
        Vec ts = lower_lows(heap, e, copies, ns, &kids);
        const STerm *body = lower_one_t(heap, &ts);
        return lower_fun_of(heap, &bs, body);
      }
      case RIR_XBORROW:
        x = x->inner;
        break;
      case RIR_XCLONE:
        x = x->inner;
        break;
      case RIR_XBOX:
        x = x->inner;
        break;
      case RIR_XLET: {
        Vec js = lower_children(heap, e, ls, x, w);
        Str name = lower_lookup(heap, ns, x->name);
        const RirTy *st = lower_scrut_type(heap, &js);
        const STerm *ty = infer_mty(heap, copies, st);
        Vec ts = lower_lows(heap, e, copies, ns, &js);
        return lower_let_t(heap, name, ty, &ts);
      }
    }
  }
  return sterm_var(heap, lower_mech_bool);
}

/* lows(e, copies, ns, js): reuse inference's child jobs so calls, constructors and closures
 * retain their expected types. A constructor is annotated even in synthesis position. js holds
 * LowerJob pointers; the result STerm pointers, one for each job, in the order of the jobs. */
static Vec lower_lows(Heap *heap, const InferEnv *e, const Vec *copies, const Vec *ns, const Vec *js) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < js->len; i++) {
    const LowerJob *j = lower_at(js, i);
    if (j == NULL) continue;
    const STerm *t = lower_low_job(heap, e, copies, ns, &j->locals, j->ex, j->want);
    lower_push(heap, &out, t);
  }
  return out;
}

/* calls(name, es): CD10, a fn that calls itself is a `def rec`. The Bend recursion over the list
 * of pending expressions is a loop over a stack (the head of the Bend list is the top). */
static bool lower_calls(Heap *heap, Str name, const Vec *es) {
  Vec stack = vec_new(heap->arena);
  lower_push_rev(heap, &stack, es);
  while (stack.len > 0 && !heap->oom) {
    const RirEx *x = lower_pop(&stack);
    if (x == NULL) continue;
    switch (x->kind) {
      case RIR_XVAR:
        break;
      case RIR_XINT:
        break;
      case RIR_XBOOL:
        break;
      case RIR_XCALL:
        if (str_eq(x->func, name)) return true;
        lower_push_rev(heap, &stack, &x->args);
        break;
      case RIR_XCTOR:
        lower_push_rev(heap, &stack, &x->args);
        break;
      case RIR_XMATCH: {
        Vec bodies = lower_arm_bodies(heap, &x->arms);
        lower_push_rev(heap, &stack, &bodies);
        lower_push(heap, &stack, x->scrut);
        break;
      }
      case RIR_XIF:
        lower_push(heap, &stack, x->no);
        lower_push(heap, &stack, x->yes);
        lower_push(heap, &stack, x->cond);
        break;
      case RIR_XCLOSURE:
        lower_push(heap, &stack, x->body);
        break;
      case RIR_XBORROW:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XCLONE:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XBOX:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XLET:
        lower_push(heap, &stack, x->body);
        lower_push(heap, &stack, x->value);
        break;
    }
  }
  return false;
}

/* rec_or_def(r, name, tele, body) */
static const SDecl *lower_rec_or_def(Heap *heap, bool r, Str name, const STerm *tele, const STerm *body) {
  if (r) {
    Vec defs = vec_new(heap->arena);
    const SRecDef *d = srec_def_new(heap, name, tele, body);
    if (d == NULL) heap->oom = true;
    lower_push(heap, &defs, d);
    return sdecl_rec_new(heap, defs);
  }
  return sdecl_def_new(heap, name, tele, body);
}

/* C: push every element of xs onto out, in order. */
static void lower_push_all(Heap *heap, Vec *out, const Vec *xs) {
  for (size_t i = 0; i < xs->len; i++) lower_push(heap, out, lower_at(xs, i));
}

/* all_names(es): every local binder name of es (Str pointers), in the order of the Bend
 * pre-order walk; the walk is a loop over a stack of pending expressions. */
static Vec lower_all_names(Heap *heap, const Vec *es) {
  Vec out = vec_new(heap->arena);
  Vec stack = vec_new(heap->arena);
  lower_push_rev(heap, &stack, es);
  while (stack.len > 0 && !heap->oom) {
    const RirEx *x = lower_pop(&stack);
    if (x == NULL) continue;
    switch (x->kind) {
      case RIR_XVAR:
        break;
      case RIR_XINT:
        break;
      case RIR_XBOOL:
        break;
      case RIR_XCALL:
        lower_push_rev(heap, &stack, &x->args);
        break;
      case RIR_XCTOR:
        lower_push_rev(heap, &stack, &x->args);
        break;
      case RIR_XMATCH: {
        Vec names = lower_arm_names(heap, &x->arms);
        lower_push_all(heap, &out, &names);
        Vec bodies = lower_arm_bodies(heap, &x->arms);
        lower_push_rev(heap, &stack, &bodies);
        lower_push(heap, &stack, x->scrut);
        break;
      }
      case RIR_XIF:
        lower_push(heap, &stack, x->no);
        lower_push(heap, &stack, x->yes);
        lower_push(heap, &stack, x->cond);
        break;
      case RIR_XCLOSURE: {
        Vec names = lower_param_names(heap, &x->params);
        lower_push_all(heap, &out, &names);
        lower_push(heap, &stack, x->body);
        break;
      }
      case RIR_XBORROW:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XCLONE:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XBOX:
        lower_push(heap, &stack, x->inner);
        break;
      case RIR_XLET: {
        const Str *nm = lower_str_box(heap, x->name);
        lower_push(heap, &out, nm);
        lower_push(heap, &stack, x->body);
        lower_push(heap, &stack, x->value);
        break;
      }
    }
  }
  return out;
}

/* arrows(bs, body): bs holds SBinder pointers; the Bend right recursion is a loop from the last
 * binder to the first. */
static const STerm *lower_arrows(Heap *heap, const Vec *bs, const STerm *body) {
  const STerm *acc = body;
  for (size_t i = bs->len; i > 0; i--) {
    const SBinder *b = lower_at(bs, i - 1);
    if (b == NULL) continue;
    SBinder bv = *b;
    acc = sterm_arrow(heap, bv, acc);
  }
  return acc;
}

/* fn_decl(e, globals, copies, name, gs, ps, ret, body): the names of the params and of every
 * local binder are mapped away from the generics and the globals, the binders are the generics
 * (quantity 0) then the params, and the body is the one job lowered at the stripped return type. */
const SDecl *lower_fn_decl(Heap *heap, const InferEnv *e, const Vec *globals, const Vec *copies, Str name,
                           const Vec *gs, const Vec *ps, const RirTy *ret, const RirEx *body) {
  Vec body_one = vec_new(heap->arena);
  lower_push(heap, &body_one, body);
  Vec pnames = lower_param_names(heap, ps);
  Vec anames = lower_all_names(heap, &body_one);
  Vec raws = heap_vec_concat(heap, &pnames, &anames);
  Vec used = heap_vec_concat(heap, gs, globals);
  Vec none = vec_new(heap->arena);
  Vec ns = lower_name_map(heap, &raws, &used, &none);
  Vec gbs = lower_gen_binders(heap, gs);
  Vec pbs = lower_param_binders(heap, ps, copies, &ns);
  Vec bs = heap_vec_concat(heap, &gbs, &pbs);
  InferEnv fe = infer_fn_env(*e, *gs);
  Vec locs = infer_closure_locs(heap, ps, &none);
  const RirTy *want = infer_strip(heap, ret);
  const LowerJob *job = lower_job_new(heap, locs, body, want);
  Vec jobs = vec_new(heap->arena);
  lower_push(heap, &jobs, job);
  Vec ts = lower_lows(heap, &fe, copies, &ns, &jobs);
  const STerm *low = lower_one_t(heap, &ts);
  const STerm *fun = lower_fun_of(heap, &bs, low);
  const STerm *mret = infer_mty(heap, copies, ret);
  const STerm *tele = lower_arrows(heap, &bs, mret);
  Str bname = infer_binder_name(heap, name);
  bool r = lower_calls(heap, name, &body_one);
  return lower_rec_or_def(heap, r, bname, tele, fun);
}

/* gen_vars(gs): gs holds Str pointers; an SVar for each generic (STerm pointers). */
static Vec lower_gen_vars(Heap *heap, const Vec *gs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    const STerm *v = sterm_var(heap, lower_str_at(gs, i));
    lower_push(heap, &out, v);
  }
  return out;
}

/* field_arrows(fs, copies, res): a constructor field is a binder at quantity many of the
 * constructor type. fs holds RirTy pointers; the Bend right recursion is a loop from the last
 * field to the first. */
static const STerm *lower_field_arrows(Heap *heap, const Vec *fs, const Vec *copies, const STerm *res) {
  const STerm *acc = res;
  for (size_t i = fs->len; i > 0; i--) {
    const RirTy *f = lower_at(fs, i - 1);
    if (f == NULL) continue;
    const STerm *ty = infer_mty(heap, copies, f);
    SBinder b = sbinder(QUANTITY_MANY, lower_skip, ty);
    acc = sterm_arrow(heap, b, acc);
  }
  return acc;
}

/* fam_ctors(vs, copies, res): vs holds RirVariant pointers; SFamCtor pointers. */
static Vec lower_fam_ctors(Heap *heap, const Vec *vs, const Vec *copies, const STerm *res) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = lower_at(vs, i);
    if (v == NULL) continue;
    Str n = erase_typed_lower_first(heap, v->name);
    const STerm *ty = lower_field_arrows(heap, &v->fields, copies, res);
    const SFamCtor *c = sfam_ctor_new(heap, n, ty);
    lower_push(heap, &out, c);
  }
  return out;
}

/* C: DMu{[f]}, a `mu` declaration of one family. */
static const SDecl *lower_mu_one(Heap *heap, SFam f) {
  const SFam *p = sfam_new(heap, f);
  Vec fams = vec_new(heap->arena);
  lower_push(heap, &fams, p);
  return sdecl_mu_new(heap, fams);
}

/* enum_decl(copies, name, gs, vs): IEnum, a `mu` family; the generics are quantity-0 type
 * parameters. */
static const SDecl *lower_enum_decl(Heap *heap, const Vec *copies, Str name, const Vec *gs, const Vec *vs) {
  Vec params = lower_gen_binders(heap, gs);
  const STerm *ty = lower_type0(heap);
  Vec gvs = lower_gen_vars(heap, gs);
  const STerm *head = sterm_var(heap, name);
  const STerm *res = infer_apps(heap, &gvs, head);
  Vec ctors = lower_fam_ctors(heap, vs, copies, res);
  SFam f = sfam(name, params, ty, ctors);
  return lower_mu_one(heap, f);
}

/* unit_decl(name, ctor): IUnit, a family with one constructor. */
static const SDecl *lower_unit_decl(Heap *heap, Str name, Str ctor) {
  Vec params = vec_new(heap->arena);
  const STerm *ty = lower_type0(heap);
  Str cn = infer_binder_name(heap, ctor);
  const STerm *res = sterm_var(heap, name);
  const SFamCtor *c = sfam_ctor_new(heap, cn, res);
  Vec ctors = vec_new(heap->arena);
  lower_push(heap, &ctors, c);
  SFam f = sfam(name, params, ty, ctors);
  return lower_mu_one(heap, f);
}

/* bool_decl(): CD6, MechBool is `bool` on the way back and makes no Rust item. */
static const SDecl *lower_bool_decl(Heap *heap) {
  Vec params = vec_new(heap->arena);
  const STerm *ty = lower_type0(heap);
  const STerm *false_res = sterm_var(heap, lower_mech_bool);
  const SFamCtor *false_ctor = sfam_ctor_new(heap, lower_mech_false, false_res);
  const STerm *true_res = sterm_var(heap, lower_mech_bool);
  const SFamCtor *true_ctor = sfam_ctor_new(heap, lower_mech_true, true_res);
  Vec ctors = vec_new(heap->arena);
  lower_push(heap, &ctors, false_ctor);
  lower_push(heap, &ctors, true_ctor);
  SFam f = sfam(lower_mech_bool, params, ty, ctors);
  return lower_mu_one(heap, f);
}

/* item_decls(ps, copies, e, globals): ps holds ResolvePlaced pointers; SDecl pointers. An IUse
 * makes no declaration. */
static Vec lower_item_decls(Heap *heap, const Vec *ps, const Vec *copies, const InferEnv *e,
                            const Vec *globals) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const ResolvePlaced *p = lower_at(ps, i);
    if (p == NULL || p->item == NULL) continue;
    const RirItem *it = p->item;
    switch (it->kind) {
      case RIR_IFN: {
        const SDecl *d = lower_fn_decl(heap, e, globals, copies, it->name, &it->generics, &it->params,
                                       it->ret, it->body);
        lower_push(heap, &out, d);
        break;
      }
      case RIR_IENUM: {
        const SDecl *d = lower_enum_decl(heap, copies, it->name, &it->generics, &it->variants);
        lower_push(heap, &out, d);
        break;
      }
      case RIR_IUNIT: {
        const SDecl *d = lower_unit_decl(heap, it->name, it->ctor);
        lower_push(heap, &out, d);
        break;
      }
      case RIR_IUSE:
        break;
    }
  }
  return out;
}

/* with_bool(first, ds): the first module also holds the MechBool family, in front. */
static Vec lower_with_bool(Heap *heap, bool first, const Vec *ds) {
  if (first) {
    const SDecl *b = lower_bool_decl(heap);
    return lower_cons(heap, b, ds);
  }
  return *ds;
}

/* not_nat(m, rest): m <> rest unless m is the `nat` module. C: the keep test of code_mods, which
 * pushes m in place of the Bend cons. */
static bool lower_not_nat(const ResolveMod *m) {
  return !str_eq(m->name, str_lit("nat"));
}

/* code_mods(ms): ms holds ResolveMod pointers; every module but `nat`, in order. */
static Vec lower_code_mods(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    const ResolveMod *m = lower_at(ms, i);
    if (m != NULL && lower_not_nat(m)) lower_push(heap, &out, m);
  }
  return out;
}

/* mods_low(ms, first, copies, e, globals): one Low for each module (LowerLow pointers); only the
 * first module gets the MechBool family. */
static Vec lower_mods_low(Heap *heap, const Vec *ms, bool first, const Vec *copies, const InferEnv *e,
                          const Vec *globals) {
  Vec out = vec_new(heap->arena);
  bool f = first;
  for (size_t i = 0; i < ms->len; i++) {
    const ResolveMod *m = lower_at(ms, i);
    if (m == NULL) continue;
    Vec items = lower_item_decls(heap, &m->items, copies, e, globals);
    Vec ds = lower_with_bool(heap, f, &items);
    Str text = syntax_print(heap, &ds);
    const LowerLow *low = lower_low_new(heap, m->file, text);
    lower_push(heap, &out, low);
    f = false;
  }
  return out;
}

/* global_names(rows): rows holds ResolveRow pointers; the mech name of each row, then the names
 * of the built-in items (Str pointers). */
static Vec lower_global_names(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    const ResolveRow *r = lower_at(rows, i);
    if (r == NULL) continue;
    lower_push(heap, &out, lower_str_box(heap, r->mech));
  }
  lower_push(heap, &out, lower_str_box(heap, lower_mech_bool));
  lower_push(heap, &out, lower_str_box(heap, lower_mech_true));
  lower_push(heap, &out, lower_str_box(heap, lower_mech_false));
  lower_push(heap, &out, lower_str_box(heap, str_lit("Nat")));
  lower_push(heap, &out, lower_str_box(heap, lower_nat_add_mech));
  return out;
}

/* lower(o): the code modules of o with the copies and the env of every module (the `nat`
 * module included). */
Vec lower_lower(Heap *heap, const ResolveOut *o) {
  Vec code = lower_code_mods(heap, &o->mods);
  Vec none = vec_new(heap->arena);
  Vec copies = infer_mods_copies(heap, &o->mods, &none);
  Vec sigs = infer_nat_sigs(heap);
  InferEnv base = {sigs, vec_new(heap->arena), vec_new(heap->arena)};
  InferEnv e = infer_mods_env(heap, &o->mods, base);
  Vec globals = lower_global_names(heap, &o->rows);
  return lower_mods_low(heap, &code, true, &copies, &e, &globals);
}

/* C: push s onto buf; a failed push sets heap->oom. */
static void lower_buf_push(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* files_text(ls): mode `lower`, a `-- file <name>` comment line, then the text of the file (one
 * StrBuf, the same bytes as the Bend right fold). */
Str lower_files_text(Heap *heap, const Vec *ls) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = lower_at(ls, i);
    if (l == NULL) continue;
    lower_buf_push(heap, &buf, str_lit("-- file "));
    lower_buf_push(heap, &buf, l->file);
    lower_buf_push(heap, &buf, str_lit("\n"));
    lower_buf_push(heap, &buf, l->text);
  }
  return strbuf_str(&buf);
}

/* source(ls): mode `check`, the files joined in manifest order (as `kanon build`, CF11). */
Str lower_source(Heap *heap, const Vec *ls) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < ls->len; i++) {
    const LowerLow *l = lower_at(ls, i);
    if (l == NULL) continue;
    lower_buf_push(heap, &buf, l->text);
    lower_buf_push(heap, &buf, str_lit("\n"));
  }
  return strbuf_str(&buf);
}
