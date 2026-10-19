/* Port of wasm/link.bend: the program table built from the erased kernel rows (functions,
 * postulates and mu legs) and the layout of the runtime type ids. Every definition keeps its Bend
 * name and order. Lists are Vecs of pointers, Maybe<String> is LinkMaybeStr, Maybe<MuLeg> is a
 * MuLeg pointer (NULL = None), Result<Err.T, X> is the kernel Result with X boxed on the heap, and
 * choose(b, yes, no) is `b ? yes : no` at each site. A failed allocation sets heap->oom and a
 * Result chain stops with Fail{error_host_oom}. */
#include "wasm.h"

static const Result result_zero;
static const LinkLeg leg_zero;
static const Form form_zero;

/* ---- host helpers: boxes for Vec items and Result values; NULL after a failed allocation ---- */

static Str *link_str_box(Heap *heap, Str value) {
  Str *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = value;
  return box;
}

static Vec *link_vec_box(Heap *heap, Vec value) {
  Vec *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = value;
  return box;
}

/* Done{value}; a NULL box is a failed allocation (heap->oom set) and becomes Fail{host oom}. */
static Result link_done(void *value) {
  Result r = result_zero;
  r.kind = value == NULL ? RESULT_FAIL : RESULT_DONE;
  r.value = value;
  r.error = error_host_oom();
  return r;
}

static Result link_fail(Error error) {
  Result r = result_zero;
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static Str link_cat(Heap *heap, Str a, Str b) {
  Str out = str_cat(heap->arena, a, b);
  if (out.len != a.len + b.len) heap->oom = true;
  return out;
}

/* Fail{Err.Mismatch{text ++ s}} */
static Result link_mismatch(Heap *heap, const char *text, Str s) {
  return link_fail(error_make(ERROR_MISMATCH, link_cat(heap, str_lit(text), s)));
}

/* String.drop(s, n) for n at most the length (every caller drops a matched ASCII prefix). */
static Str link_drop(Str s, size_t n) {
  StrSlice slice = str_slice(s, n, s.len);
  return slice.status == STR_INDEX_OK ? slice.value : str_empty();
}

static Str link_str_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  return got.status == VEC_GET_OK && got.value != NULL ? *(Str *)got.value : str_empty();
}

static void *link_item_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  return got.status == VEC_GET_OK ? got.value : NULL;
}

/* x <> xs at the tail of a Vec built in list order; a NULL item is a failed allocation. */
static void link_push(Heap *heap, Vec *xs, void *item) {
  if (item == NULL || !vec_push(xs, item)) heap->oom = true;
}

/* List.append(xs, ys): ys joins the Vec being built. */
static void link_append(Heap *heap, Vec *out, const Vec *ys) {
  for (size_t i = 0; i < ys->len; i++) link_push(heap, out, link_item_at(ys, i));
}

/* Nat.read(s): the decimal digits of s, None when s is empty or holds a non-digit. */
static MaybeNat link_nat_read(Heap *heap, Str s) {
  MaybeNat out;
  NatParse parsed = nat_parse_decimal(heap->arena, s);
  out.some = false;
  out.value = nat_zero();
  switch (parsed.status) {
    case NAT_PARSE_OK:
      out.some = true;
      out.value = parsed.value;
      return out;
    case NAT_PARSE_OOM:
      heap->oom = true;
      return out;
    case NAT_PARSE_EMPTY:
      return out;
    case NAT_PARSE_BAD_DIGIT:
      return out;
  }
  return out;
}

/* ---- constructors ---- */

LinkLeg link_lunit(void) {
  LinkLeg l = leg_zero;
  l.kind = LINK_LEG_UNIT;
  return l;
}

LinkLeg link_lrepr(Repr repr) {
  LinkLeg l = leg_zero;
  l.kind = LINK_LEG_REPR;
  l.repr = repr;
  return l;
}

LinkLeg link_lmu(Str tid, Vec fields) {
  LinkLeg l = leg_zero;
  l.kind = LINK_LEG_MU;
  l.tid = tid;
  l.fields = fields;
  return l;
}

LinkLeg *link_leg_new(Heap *heap, LinkLeg leg) {
  LinkLeg *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = leg;
  return box;
}

Form link_fi31(void) {
  Form f = form_zero;
  f.kind = FORM_I31;
  return f;
}

Form link_funit(void) {
  Form f = form_zero;
  f.kind = FORM_UNIT;
  return f;
}

Form link_fany(void) {
  Form f = form_zero;
  f.kind = FORM_ANY;
  return f;
}

Form link_ffields(Vec fields) {
  Form f = form_zero;
  f.kind = FORM_FIELDS;
  f.fields = fields;
  return f;
}

Form link_fsum(Vec legs) {
  Form f = form_zero;
  f.kind = FORM_SUM;
  f.legs = legs;
  return f;
}

Form link_ffn(Nat arity) {
  Form f = form_zero;
  f.kind = FORM_FN;
  f.arity = arity;
  return f;
}

Form *link_form_new(Heap *heap, Form form) {
  Form *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = form;
  return box;
}

LinkFn *link_fn_new(Heap *heap, Vec params, Repr result, const Ktm *body) {
  LinkFn *fn = heap_alloc(heap, sizeof *fn);
  if (fn == NULL) return NULL;
  fn->params = params;
  fn->result = result;
  fn->body = body;
  return fn;
}

MuLeg *link_mu_leg_new(Heap *heap, Str tid, Str family, Nat tag, Vec fields) {
  MuLeg *m = heap_alloc(heap, sizeof *m);
  if (m == NULL) return NULL;
  m->tid = tid;
  m->family = family;
  m->tag = tag;
  m->fields = fields;
  return m;
}

Pair *link_row_new(Heap *heap, Str name, void *value) {
  Pair *row = heap_alloc(heap, sizeof *row);
  Str *key = link_str_box(heap, name);
  if (row == NULL || key == NULL || value == NULL) return NULL;
  row->first = key;
  row->second = value;
  return row;
}

/* ---- the definitions, in the Bend order ---- */

/* choose(b, yes, no) is `b ? yes : no` at every site below. */

LinkMaybeStr link_inside(Str prefix, Str s) {
  LinkMaybeStr out;
  bool hit = str_starts_with(s, prefix) && str_ends_with(s, str_lit(">"));
  size_t rest = hit && s.len >= prefix.len ? s.len - prefix.len : 0;
  size_t keep = rest > 0 ? rest - 1 : 0;
  StrSlice slice = str_slice(s, hit ? prefix.len : 0, hit ? prefix.len + keep : 0);
  out.some = hit;
  out.value = slice.status == STR_INDEX_OK ? slice.value : str_empty();
  return out;
}

/* split_push(c, rest) and split_cut(cut, c, rest): Bend builds the pieces from the right and
 * prepends each char to the head piece; the port walks the bytes left to right, the open piece is
 * the slice [start, i) of the text (the push is the loop's advance) and split_cut closes it. */
static void link_split_cut(Heap *heap, Vec *pieces, Str s, size_t start, size_t end) {
  StrSlice slice = str_slice(s, start, end);
  link_push(heap, pieces, link_str_box(heap, slice.status == STR_INDEX_OK ? slice.value : str_empty()));
}

/* The separator and the brackets are ASCII, so the byte walk matches the code point walk. */
Vec link_split_chars(Heap *heap, Str s, char sep, size_t depth) {
  Vec pieces = vec_new(heap->arena);
  size_t start = 0;
  for (size_t i = 0; i < s.len; i++) {
    StrAt at = str_at(s, i);
    unsigned char c = at.status == STR_INDEX_OK ? at.value : 0;
    bool open = c == '<';
    bool close = c == '>';
    bool cut = !open && !close && c == (unsigned char)sep && depth == 0;
    size_t down = depth > 0 ? depth - 1 : 0;
    depth = open ? depth + 1 : (close ? down : depth);
    if (cut) {
      link_split_cut(heap, &pieces, s, start, i);
      start = i + 1;
    }
  }
  link_split_cut(heap, &pieces, s, start, s.len);
  return pieces;
}

Vec link_split_top(Heap *heap, char sep, Str s) { return link_split_chars(heap, s, sep, 0); }

Result link_repr_of_text(Heap *heap, Str s) {
  if (str_eq(s, str_lit("i31"))) return link_done(eterm_repr_new(heap, eterm_ri31()));
  if (str_starts_with(s, str_lit("struct ")))
    return link_done(eterm_repr_new(heap, eterm_rstruct(eterm_tid(link_drop(s, 7)))));
  if (str_starts_with(s, str_lit("union ")))
    return link_done(eterm_repr_new(heap, eterm_runion(eterm_tid(link_drop(s, 6)))));
  if (str_starts_with(s, str_lit("func ")))
    return link_done(eterm_repr_new(heap, eterm_rfunc(eterm_tid(link_drop(s, 5)))));
  if (str_starts_with(s, str_lit("thunk ")))
    return link_done(eterm_repr_new(heap, eterm_rthunk(eterm_tid(link_drop(s, 6)))));
  return link_mismatch(heap, "a repr does not parse:  ", s);
}

Result link_parse_reprs(Heap *heap, const Vec *xs) {
  Vec rs = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    Result r = link_repr_of_text(heap, link_str_at(xs, i));
    if (r.kind == RESULT_FAIL) return r;
    link_push(heap, &rs, r.value);
  }
  return link_done(link_vec_box(heap, rs));
}

Result link_parse_fields_case(Heap *heap, bool empty, Str body) {
  if (empty) return link_done(link_vec_box(heap, vec_new(heap->arena)));
  Vec parts = link_split_top(heap, ',', body);
  return link_parse_reprs(heap, &parts);
}

Result link_parse_fields(Heap *heap, Str body) {
  return link_parse_fields_case(heap, body.len == 0, body);
}

Result link_parse_leg(Heap *heap, bool unit, Str s) {
  if (unit) return link_done(link_leg_new(heap, link_lunit()));
  Result r = link_repr_of_text(heap, s);
  if (r.kind == RESULT_FAIL) return r;
  return link_done(link_leg_new(heap, link_lrepr(*(Repr *)r.value)));
}

Result link_parse_legs(Heap *heap, const Vec *xs) {
  Vec ls = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    Str s = link_str_at(xs, i);
    Result l = link_parse_leg(heap, str_eq(s, str_lit("unit")), s);
    if (l.kind == RESULT_FAIL) return l;
    link_push(heap, &ls, l.value);
  }
  return link_done(link_vec_box(heap, ls));
}

Result link_parse_sum(Heap *heap, bool empty, Str s) {
  if (empty) return link_done(link_form_new(heap, link_fsum(vec_new(heap->arena))));
  Vec parts = link_split_top(heap, '|', s);
  Result ls = link_parse_legs(heap, &parts);
  if (ls.kind == RESULT_FAIL) return ls;
  return link_done(link_form_new(heap, link_fsum(*(Vec *)ls.value)));
}

Result link_parse_arity(Heap *heap, MaybeNat n, Str t) {
  if (n.some) return link_done(link_form_new(heap, link_ffn(n.value)));
  return link_mismatch(heap, "an arity does not parse:  ", t);
}

/* The pair and tuple arms of parse_wrapped: rs <- parse_fields(body); FFields{rs}. */
static Result link_parse_wrapped_fields(Heap *heap, Str body) {
  Result rs = link_parse_fields(heap, body);
  if (rs.kind == RESULT_FAIL) return rs;
  return link_done(link_form_new(heap, link_ffields(*(Vec *)rs.value)));
}

Result link_parse_wrapped(Heap *heap, LinkMaybeStr fn, LinkMaybeStr pair, LinkMaybeStr tuple,
                          LinkMaybeStr sum, Str t) {
  if (fn.some) return link_parse_arity(heap, link_nat_read(heap, fn.value), t);
  if (pair.some) return link_parse_wrapped_fields(heap, pair.value);
  if (tuple.some) return link_parse_wrapped_fields(heap, tuple.value);
  if (sum.some) return link_parse_sum(heap, sum.value.len == 0, sum.value);
  return link_mismatch(heap, "a tid does not parse:  ", t);
}

Result link_parse_tid(Heap *heap, Str t) {
  if (str_eq(t, str_lit("i31"))) return link_done(link_form_new(heap, link_fi31()));
  if (str_eq(t, str_lit("unit"))) return link_done(link_form_new(heap, link_funit()));
  if (str_eq(t, str_lit("any"))) return link_done(link_form_new(heap, link_fany()));
  return link_parse_wrapped(heap, link_inside(str_lit("fn<"), t), link_inside(str_lit("pair<"), t),
                            link_inside(str_lit("tuple<"), t), link_inside(str_lit("sum<"), t), t);
}

MuLeg *link_mu_fields(Heap *heap, Result parsed, Str t, Str fam, Nat k) {
  if (parsed.kind == RESULT_FAIL) return NULL;
  return link_mu_leg_new(heap, t, fam, k, *(Vec *)parsed.value);
}

MuLeg *link_mu_index(Heap *heap, LinkMaybeStr family, MaybeNat index, Str t, Str fam,
                     const Vec *fields) {
  if (!(family.some && index.some)) return NULL;
  return link_mu_fields(heap, link_parse_reprs(heap, fields), t, fam, index.value);
}

MuLeg *link_mu_parts(Heap *heap, const Vec *parts, Str t) {
  if (parts->len < 2) return NULL;
  Str fam = link_str_at(parts, 0);
  Str k = link_str_at(parts, 1);
  Vec fs = vec_new(heap->arena);
  for (size_t i = 2; i < parts->len; i++) link_push(heap, &fs, link_item_at(parts, i));
  return link_mu_index(heap, link_inside(str_lit("mu<"), fam), link_nat_read(heap, k), t, fam, &fs);
}

MuLeg *link_mu_inside(Heap *heap, LinkMaybeStr body, Str t) {
  if (!body.some) return NULL;
  Vec parts = link_split_top(heap, ',', body.value);
  return link_mu_parts(heap, &parts, t);
}

MuLeg *link_parse_mu_leg(Heap *heap, Str t) {
  return link_mu_inside(heap, link_inside(str_lit("leg<"), t), t);
}

Result link_fields_form(Heap *heap, const Form *f, Str t) {
  if (f->kind == FORM_FIELDS) return link_done(link_vec_box(heap, f->fields));
  return link_mismatch(heap, "a struct tid is not a pair or a tuple:  ", t);
}

Result link_fields_of(Heap *heap, Str t) {
  Result f = link_parse_tid(heap, t);
  if (f.kind == RESULT_FAIL) return f;
  return link_fields_form(heap, f.value, t);
}

Result link_arity_form(Heap *heap, const Form *f, Str t) {
  if (f->kind == FORM_FN) return link_done(erase_util_nat_new(heap, f->arity));
  return link_mismatch(heap, "a function repr is not fn<n>:  ", t);
}

Result link_arity_of_fn(Heap *heap, Str t) {
  Result f = link_parse_tid(heap, t);
  if (f.kind == RESULT_FAIL) return f;
  return link_arity_form(heap, f.value, t);
}

Result link_sum_form(Heap *heap, const Form *f, Str t) {
  if (f->kind == FORM_SUM) return link_done(link_vec_box(heap, f->legs));
  return link_mismatch(heap, "a case scrutinee is not a sum:  ", t);
}

Result link_sum_legs(Heap *heap, Str t) {
  Result f = link_parse_tid(heap, t);
  if (f.kind == RESULT_FAIL) return f;
  return link_sum_form(heap, f.value, t);
}

Str link_fn_key(Heap *heap, Nat n) {
  return link_cat(heap, link_cat(heap, str_lit("fn<"), nat_to_decimal(heap->arena, n)), str_lit(">"));
}

Repr link_nat_repr(void) { return eterm_runion(eterm_tid(str_lit("nat"))); }

Repr link_any_repr(void) { return eterm_runion(eterm_tid(str_lit("any"))); }

LinkMaybeStr link_tid_text(Repr r) {
  LinkMaybeStr out;
  out.some = false;
  out.value = str_empty();
  switch (r.kind) {
    case REPR_I31:
      return out;
    case REPR_STRUCT:
    case REPR_UNION:
    case REPR_FUNC:
    case REPR_THUNK:
      out.some = true;
      out.value = r.tid.name;
      return out;
  }
  return out;
}

Vec link_decls_of_entry(Heap *heap, EraseEntry e) {
  switch (e.kind) {
    case ERASE_ENTRY_CODE:
      return e.decls;
    case ERASE_ENTRY_DROPPED:
    case ERASE_ENTRY_POSTULATE:
      return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

/* ds: Kdecl pointers; the result holds Pair pointers (first = Str *, second = LinkFn *). */
Vec link_funs_of_decls(Heap *heap, const Vec *ds) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ds->len; i++) {
    const Kdecl *d = link_item_at(ds, i);
    if (d == NULL) continue;
    switch (d->kind) {
      case KDECL_FUN:
        link_push(heap, &out,
                  link_row_new(heap, d->fid.name, link_fn_new(heap, d->params, d->result, d->body)));
        break;
      case KDECL_REC:
        break;
    }
  }
  return out;
}

/* rows: EraseRow pointers. */
Vec link_funs_of_rows(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    const EraseRow *row = link_item_at(rows, i);
    if (row == NULL) continue;
    Vec decls = link_decls_of_entry(heap, row->entry);
    Vec funs = link_funs_of_decls(heap, &decls);
    link_append(heap, &out, &funs);
  }
  return out;
}

/* rows: EraseRow pointers; the result holds Pair pointers (first = Str *, second = Repr *). */
Vec link_posts_of_rows(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    const EraseRow *row = link_item_at(rows, i);
    if (row == NULL) continue;
    switch (row->entry.kind) {
      case ERASE_ENTRY_POSTULATE:
        link_push(heap, &out, link_row_new(heap, row->name, eterm_repr_new(heap, row->entry.repr)));
        break;
      case ERASE_ENTRY_DROPPED:
      case ERASE_ENTRY_CODE:
        break;
    }
  }
  return out;
}

/* mu_cons(m, rest): m joins the Vec being built when it is Some. */
void link_mu_cons(Heap *heap, MuLeg *m, Vec *rest) {
  if (m != NULL) link_push(heap, rest, m);
}

/* ts: Tid pointers; the result holds MuLeg pointers. */
Vec link_mus_of_tids(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const Tid *t = link_item_at(ts, i);
    if (t == NULL) continue;
    link_mu_cons(heap, link_parse_mu_leg(heap, t->name), &out);
  }
  return out;
}

Vec link_mus_of_decls(Heap *heap, const Vec *ds) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ds->len; i++) {
    const Kdecl *d = link_item_at(ds, i);
    if (d == NULL) continue;
    switch (d->kind) {
      case KDECL_REC: {
        Vec mus = link_mus_of_tids(heap, &d->tids);
        link_append(heap, &out, &mus);
        break;
      }
      case KDECL_FUN:
        break;
    }
  }
  return out;
}

Vec link_mus_of_rows(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rows->len; i++) {
    const EraseRow *row = link_item_at(rows, i);
    if (row == NULL) continue;
    Vec decls = link_decls_of_entry(heap, row->entry);
    Vec mus = link_mus_of_decls(heap, &decls);
    link_append(heap, &out, &mus);
  }
  return out;
}

Prog link_program_table(Heap *heap, const Vec *rows) {
  Prog p;
  p.funs = link_funs_of_rows(heap, rows);
  p.posts = link_posts_of_rows(heap, rows);
  p.mus = link_mus_of_rows(heap, rows);
  return p;
}

/* rows: Pair pointers with first = Str *; the second of the first row named name, NULL = None. */
void *link_lookup(const Vec *rows, Str name) {
  for (size_t i = 0; i < rows->len; i++) {
    const Pair *row = link_item_at(rows, i);
    const Str *key = row == NULL ? NULL : row->first;
    if (key != NULL && str_eq(*key, name)) return row->second;
  }
  return NULL;
}

/* ms: MuLeg pointers; the legs of one family, in order. */
Vec link_mu_family(Heap *heap, const Vec *ms, Str family) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    MuLeg *m = link_item_at(ms, i);
    if (m != NULL && str_eq(m->family, family)) link_push(heap, &out, m);
  }
  return out;
}

Nat link_mu_top(Heap *heap, const Vec *ms) {
  Nat top = nat_zero();
  for (size_t i = 0; i < ms->len; i++) {
    const MuLeg *m = link_item_at(ms, i);
    if (m == NULL) continue;
    Nat next = nat_succ(heap->arena, m->tag);
    top = nat_cmp(next, top) >= 0 ? next : top;
  }
  return top;
}

LinkLeg link_mu_at(const Vec *ms, Nat index) {
  for (size_t i = 0; i < ms->len; i++) {
    const MuLeg *m = link_item_at(ms, i);
    if (m != NULL && nat_cmp(m->tag, index) == 0)
      return m->fields.len == 0 ? link_lunit() : link_lmu(m->tid, m->fields);
  }
  return link_lunit();
}

/* n legs from index upward, LinkLeg pointers; a count beyond the host range is an allocation failure. */
Vec link_mu_order(Heap *heap, Nat n, Nat index, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  NatToU64 count = nat_to_u64(n);
  if (count.status == NAT_TO_U64_OVERFLOW) {
    heap->oom = true;
    return out;
  }
  Nat at = index;
  for (uint64_t i = 0; i < count.value; i++) {
    link_push(heap, &out, link_leg_new(heap, link_mu_at(ms, at)));
    at = nat_succ(heap->arena, at);
  }
  return out;
}

Result link_sum_mu(Heap *heap, const Vec *ms, Str t) {
  if (ms->len == 0) return link_sum_legs(heap, t);
  return link_done(link_vec_box(heap, link_mu_order(heap, link_mu_top(heap, ms), nat_zero(), ms)));
}

Result link_sum_legs_p(Heap *heap, const Prog *p, Str t) {
  Vec family = link_mu_family(heap, &p->mus, t);
  return link_sum_mu(heap, &family, t);
}

Vec link_leg_fields(Heap *heap, LinkLeg l) {
  Vec out = vec_new(heap->arena);
  switch (l.kind) {
    case LINK_LEG_UNIT:
      return out;
    case LINK_LEG_REPR:
      link_push(heap, &out, eterm_repr_new(heap, l.repr));
      return out;
    case LINK_LEG_MU:
      return l.fields;
  }
  return out;
}

/* ---- Part 2 (Bend lines 300-763): the representation inference and the key collector ----
 * HeadKind is LinkHead (kernel.h owns HeadKind). Keys holds C.Pair rows (first = Str *, second =
 * Tspec * or Fspec *). env is List<E.Repr> in list order (index 0 = the head), Repr pointers; r <> env
 * and List.append build a new Vec. The list recursions of collect (NReprs, NLegs, NTerms, NBranches,
 * NFuns) are loops that merge in list order and stop at the first Fail, which is the Bend order. */

static const Keys keys_zero;
static const Tspec tspec_zero;
static const Fspec fspec_zero;
static const LinkHead head_zero;
static const Need need_zero;
static const InferQuery query_zero;

static Nat link_nat(Heap *heap, uint64_t n) { return nat_of_u64(heap->arena, n); }

/* List.length(xs) */
static Nat link_len(Heap *heap, const Vec *xs) { return link_nat(heap, xs->len); }

/* Nat.sub(a, b), zero below zero. */
static Nat link_nat_sub(Heap *heap, Nat a, Nat b) {
  NatSub d = nat_sub(heap->arena, a, b);
  if (d.status == NAT_SUB_OK) return d.value;
  if (d.status != NAT_SUB_UNDERFLOW) heap->oom = true;
  return nat_zero();
}

/* Nat.show(n) */
static Str link_show(Heap *heap, Nat n) { return nat_to_decimal(heap->arena, n); }

static Str link_cat3(Heap *heap, Str a, Str b, Str c) { return link_cat(heap, link_cat(heap, a, b), c); }

static Str link_cat4(Heap *heap, Str a, Str b, Str c, Str d) {
  return link_cat(heap, link_cat3(heap, a, b, c), d);
}

static Str link_cat5(Heap *heap, Str a, Str b, Str c, Str d, Str e) {
  return link_cat(heap, link_cat4(heap, a, b, c, d), e);
}

static Result link_not_yet(const char *text) { return link_fail(error_make(ERROR_NOT_YET, str_lit(text))); }

/* List.get(xs, n): NULL beyond the Vec or the host range. */
static void *link_nat_at(const Vec *xs, Nat n) {
  NatToU64 i = nat_to_u64(n);
  if (i.status != NAT_TO_U64_OK || i.value >= xs->len) return NULL;
  return link_item_at(xs, i.value);
}

/* item <> xs as a new Vec. */
static Vec link_cons(Heap *heap, void *item, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  link_push(heap, &out, item);
  link_append(heap, &out, xs);
  return out;
}

/* List.append(xs, ys) as a new Vec. */
static Vec link_concat(Heap *heap, const Vec *xs, const Vec *ys) {
  Vec out = vec_new(heap->arena);
  link_append(heap, &out, xs);
  link_append(heap, &out, ys);
  return out;
}

/* List.reverse(xs) */
static Vec link_reverse(Heap *heap, const Vec *xs) {
  Vec out = vec_reverse(heap->arena, xs);
  if (out.len != xs->len) heap->oom = true;
  return out;
}

/* List.take(xs, n) for n at most the length. */
static Vec link_take(Heap *heap, const Vec *xs, size_t n) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < n && i < xs->len; i++) link_push(heap, &out, link_item_at(xs, i));
  return out;
}

/* [item] */
static Vec link_singleton(Heap *heap, void *item) {
  Vec out = vec_new(heap->arena);
  link_push(heap, &out, item);
  return out;
}

static Result link_keys_done(Heap *heap, Keys k) { return link_done(link_keys_new(heap, k)); }

static Keys link_keys_of(Result r) { return *(Keys *)r.value; }

/* The LMu view of a leg: LRepr{r} is LMu{leg_key([r]), [r]}; LUnit has none. */
static bool link_leg_parts(Heap *heap, LinkLeg l, Str *t, Vec *rs) {
  switch (l.kind) {
    case LINK_LEG_UNIT:
      return false;
    case LINK_LEG_REPR:
      *rs = link_singleton(heap, eterm_repr_new(heap, l.repr));
      *t = link_leg_key(heap, rs);
      return true;
    case LINK_LEG_MU:
      *t = l.tid;
      *rs = l.fields;
      return true;
  }
  return false;
}

/* ---- constructors ---- */

LinkHead link_hfun(Str name, const LinkFn *fn) {
  LinkHead h = head_zero;
  h.kind = LINK_HEAD_FUN;
  h.name = name;
  h.fn = fn;
  return h;
}

LinkHead link_hprim(Str name) {
  LinkHead h = head_zero;
  h.kind = LINK_HEAD_PRIM;
  h.name = name;
  return h;
}

LinkHead link_hvalue(void) {
  LinkHead h = head_zero;
  h.kind = LINK_HEAD_VALUE;
  return h;
}

InferQuery link_infer_of(const Ktm *term, Vec env) {
  InferQuery q = query_zero;
  q.kind = INFER_QUERY_INFER;
  q.term = term;
  q.env = env;
  return q;
}

InferQuery link_infer_app(LinkHead kind, const Ktm *head, Vec args, Vec env) {
  InferQuery q = query_zero;
  q.kind = INFER_QUERY_APP;
  q.head_kind = kind;
  q.head = head;
  q.args = args;
  q.env = env;
  return q;
}

Tspec link_ts_limbs(void) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_LIMBS;
  return s;
}

Tspec link_ts_big_nat(void) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_BIG_NAT;
  return s;
}

Tspec link_ts_runtime(Vec params, Str result) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_RUNTIME;
  s.names = params;
  s.text = result;
  return s;
}

Tspec link_ts_clos(void) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_CLOS;
  return s;
}

Tspec link_ts_fn(Nat arity) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_FN;
  s.arity = arity;
  return s;
}

Tspec link_ts_struct(Vec fields) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_STRUCT;
  s.fields = fields;
  return s;
}

Tspec link_ts_leg(Vec fields) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_LEG;
  s.fields = fields;
  return s;
}

Tspec link_ts_pap(Nat count) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_PAP;
  s.count = count;
  return s;
}

Tspec link_ts_sig(Vec params, Repr result) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_SIG;
  s.params = params;
  s.result = result;
  return s;
}

Tspec link_ts_apply(Nat arity) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_APPLY;
  s.arity = arity;
  return s;
}

Tspec link_ts_entry(void) {
  Tspec s = tspec_zero;
  s.kind = TSPEC_ENTRY;
  return s;
}

Tspec *link_tspec_new(Heap *heap, Tspec s) {
  Tspec *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = s;
  return box;
}

Fspec link_fs_runtime(Str name) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_RUNTIME;
  s.name = name;
  return s;
}

Fspec link_fs_prog(Str name) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_PROG;
  s.name = name;
  return s;
}

Fspec link_fs_wrap(Str name, Nat captures) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_WRAP;
  s.name = name;
  s.captures = captures;
  return s;
}

Fspec link_fs_apply(Nat arity) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_APPLY;
  s.arity = arity;
  return s;
}

Fspec link_fs_papw(Nat arity, Nat applied) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_PAPW;
  s.arity = arity;
  s.applied = applied;
  return s;
}

Fspec link_fs_entry(void) {
  Fspec s = fspec_zero;
  s.kind = FSPEC_ENTRY;
  return s;
}

Fspec *link_fspec_new(Heap *heap, Fspec s) {
  Fspec *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = s;
  return box;
}

Keys *link_keys_new(Heap *heap, Keys k) {
  Keys *box = heap_alloc(heap, sizeof *box);
  if (box != NULL) *box = k;
  return box;
}

Need link_nrepr(Repr repr) {
  Need q = need_zero;
  q.kind = NEED_REPR;
  q.repr = repr;
  return q;
}

Need link_nreprs(Vec reprs) {
  Need q = need_zero;
  q.kind = NEED_REPRS;
  q.reprs = reprs;
  return q;
}

Need link_nstruct(Str tid) {
  Need q = need_zero;
  q.kind = NEED_STRUCT;
  q.tid = tid;
  return q;
}

Need link_nsum(Str tid) {
  Need q = need_zero;
  q.kind = NEED_SUM;
  q.tid = tid;
  return q;
}

Need link_nlegs(Vec legs) {
  Need q = need_zero;
  q.kind = NEED_LEGS;
  q.legs = legs;
  return q;
}

Need link_nwrap(Str name, LinkHead kind) {
  Need q = need_zero;
  q.kind = NEED_WRAP;
  q.name = name;
  q.head_kind = kind;
  return q;
}

Need link_nwalk(const Ktm *term, Vec env) {
  Need q = need_zero;
  q.kind = NEED_WALK;
  q.term = term;
  q.env = env;
  return q;
}

Need link_nterms(Vec terms, Vec env) {
  Need q = need_zero;
  q.kind = NEED_TERMS;
  q.terms = terms;
  q.env = env;
  return q;
}

Need link_nbranches(Vec branches, Repr scrut, Vec env) {
  Need q = need_zero;
  q.kind = NEED_BRANCHES;
  q.branches = branches;
  q.scrut = scrut;
  q.env = env;
  return q;
}

Need link_napp(const Ktm *head, Vec args, Vec env) {
  Need q = need_zero;
  q.kind = NEED_APP;
  q.head = head;
  q.args = args;
  q.env = env;
  return q;
}

Need link_napply(LinkHead kind, const Ktm *head, Nat count, Vec env) {
  Need q = need_zero;
  q.kind = NEED_APPLY;
  q.head_kind = kind;
  q.head = head;
  q.count = count;
  q.env = env;
  return q;
}

Need link_nsaturated(Str name, Nat arity, Repr result, Nat count, bool saturated) {
  Need q = need_zero;
  q.kind = NEED_SATURATED;
  q.name = name;
  q.arity = arity;
  q.result = result;
  q.count = count;
  q.saturated = saturated;
  return q;
}

Need link_nglobal(Str name, LinkHead kind) {
  Need q = need_zero;
  q.kind = NEED_GLOBAL;
  q.name = name;
  q.head_kind = kind;
  return q;
}

Need link_nfun(Str name, const LinkFn *fn) {
  Need q = need_zero;
  q.kind = NEED_FUN;
  q.name = name;
  q.fn = fn;
  return q;
}

Need link_nfuns(Vec funs) {
  Need q = need_zero;
  q.kind = NEED_FUNS;
  q.funs = funs;
  return q;
}

/* ---- the definitions, in the Bend order ---- */

/* r: Maybe<E.Repr> as a Repr pointer, NULL = None. */
Result link_repr_at(Repr *r, Error error) { return r == NULL ? link_fail(error) : link_done(r); }

/* l: Maybe<Leg> as a LinkLeg pointer, NULL = None. */
Result link_branch_at(Heap *heap, LinkLeg *l, Str t, Nat k) {
  if (l != NULL) return link_done(l);
  return link_fail(error_make(ERROR_WRONG_LEG, link_cat4(heap, str_lit("leg "), link_show(heap, k), str_lit(" of "), t)));
}

Result link_branch_tid(Heap *heap, LinkMaybeStr t, const Prog *p, Nat k) {
  if (!t.some) return link_fail(error_make(ERROR_MISMATCH, str_lit("a case scrutinee has no tid")));
  Result ls = link_sum_legs_p(heap, p, t.value);
  if (ls.kind == RESULT_FAIL) return ls;
  return link_branch_at(heap, link_nat_at(ls.value, k), t.value, k);
}

Result link_branch_leg(Heap *heap, const Prog *p, Repr scrut, KtmBranch b) {
  return link_branch_tid(heap, link_tid_text(scrut), p, b.tag);
}

/* fs: Repr pointers; the Done value is the reversed Vec. */
Result link_branch_fields(Heap *heap, bool ok, const Vec *fs, Nat k, Nat n, Nat count) {
  if (ok) return link_done(link_vec_box(heap, link_reverse(heap, fs)));
  Str head = link_cat4(heap, str_lit("leg "), link_show(heap, k), str_lit(" binds "), link_show(heap, count));
  return link_fail(error_make(ERROR_MISMATCH,
                              link_cat3(heap, head, str_lit(" fields and the branch binds "), link_show(heap, n))));
}

Result link_branch_binders(Heap *heap, const Prog *p, Repr scrut, KtmBranch b) {
  if (nat_is_zero(b.arity)) return link_done(link_vec_box(heap, vec_new(heap->arena)));
  Result l = link_branch_tid(heap, link_tid_text(scrut), p, b.tag);
  if (l.kind == RESULT_FAIL) return l;
  Vec fs = link_leg_fields(heap, *(LinkLeg *)l.value);
  Nat count = link_len(heap, &fs);
  return link_branch_fields(heap, nat_cmp(count, b.arity) == 0, &fs, b.tag, b.arity, count);
}

bool link_is_prim(Str n) {
  return str_eq(n, str_lit("natAdd")) || str_eq(n, str_lit("natSub")) || str_eq(n, str_lit("natMul")) ||
         str_eq(n, str_lit("natEq")) || str_eq(n, str_lit("natLt"));
}

Repr link_prim_result(Str n) {
  bool compare = str_eq(n, str_lit("natEq")) || str_eq(n, str_lit("natLt"));
  return compare ? eterm_runion(eterm_tid(str_lit("sum<unit|unit>"))) : link_nat_repr();
}

/* f: Maybe<Fn> as a LinkFn pointer, NULL = None. */
LinkHead link_head_lookup(const LinkFn *f, Str name) {
  if (f != NULL) return link_hfun(name, f);
  return link_is_prim(name) ? link_hprim(name) : link_hvalue();
}

LinkHead link_head_kind(const Prog *p, const Ktm *h) {
  if (h->kind != KTM_GLOBAL) return link_hvalue();
  return link_head_lookup(link_lookup(&p->funs, h->name), h->name);
}

/* head_kind(p, E.KGlobal{name}) without building the term. */
static LinkHead link_head_of_global(const Prog *p, Str name) {
  return link_head_lookup(link_lookup(&p->funs, name), name);
}

bool link_nullary_closure(Heap *heap, Repr r) {
  return r.kind == REPR_FUNC && str_eq(r.tid.name, link_fn_key(heap, nat_zero()));
}

Repr link_steps_arity(Heap *heap, Nat m, Nat k) {
  if (nat_cmp(m, k) > 0) return eterm_rfunc(eterm_tid(link_fn_key(heap, link_nat_sub(heap, m, k))));
  return link_any_repr();
}

Result link_steps_result(Heap *heap, Repr r, Nat k) {
  if (nat_is_zero(k)) {
    if (link_nullary_closure(heap, r)) return link_done(eterm_repr_new(heap, link_any_repr()));
    return link_done(eterm_repr_new(heap, r));
  }
  switch (r.kind) {
    case REPR_FUNC: {
      Result m = link_arity_of_fn(heap, r.tid.name);
      if (m.kind == RESULT_FAIL) return m;
      return link_done(eterm_repr_new(heap, link_steps_arity(heap, *(Nat *)m.value, k)));
    }
    case REPR_UNION:
      return link_done(eterm_repr_new(heap, link_any_repr()));
    case REPR_THUNK:
      return link_not_yet("KForce arrives at M2");
    case REPR_I31:
    case REPR_STRUCT:
      return link_fail(error_make(ERROR_MISMATCH, str_lit("an application head is not a function")));
  }
  return link_fail(error_host_oom());
}

Result link_after_compare(Heap *heap, Cmp c, Nat m, Repr res, Nat k) {
  switch (c) {
    case CMP_EQ:
      return link_done(eterm_repr_new(heap, res));
    case CMP_GT:
      return link_steps_result(heap, res, link_nat_sub(heap, k, m));
    case CMP_LT:
      return link_steps_result(heap, eterm_rfunc(eterm_tid(link_fn_key(heap, m))), k);
  }
  return link_fail(error_host_oom());
}

Result link_after_head(Heap *heap, Nat m, Repr res, Nat k) {
  int c = nat_cmp(k, m);
  return link_after_compare(heap, c < 0 ? CMP_LT : c == 0 ? CMP_EQ : CMP_GT, m, res, k);
}

/* post: Maybe<E.Repr> as a Repr pointer, NULL = None. Both arms fail. */
Result link_global_value(Heap *heap, const Repr *post, Str name) {
  if (post == NULL)
    return link_fail(error_make(ERROR_UNBOUND, link_cat(heap, str_lit("no runtime declaration for "), name)));
  return link_fail(error_make(ERROR_UNBOUND, link_cat3(heap, str_lit("axiom "), name, str_lit(" has no body"))));
}

Result link_global_head(Heap *heap, LinkHead h, const Prog *p, Str name) {
  switch (h.kind) {
    case LINK_HEAD_FUN: {
      Nat arity = link_len(heap, &h.fn->params);
      Repr r = h.fn->params.len == 0 ? h.fn->result : eterm_rfunc(eterm_tid(link_fn_key(heap, arity)));
      return link_done(eterm_repr_new(heap, r));
    }
    case LINK_HEAD_PRIM:
      return link_done(eterm_repr_new(heap, eterm_rfunc(eterm_tid(link_fn_key(heap, link_nat(heap, 2))))));
    case LINK_HEAD_VALUE:
      return link_global_value(heap, link_lookup(&p->posts, name), name);
  }
  return link_fail(error_host_oom());
}

Result link_global_repr(Heap *heap, const Prog *p, Str name) {
  return link_global_head(heap, link_head_of_global(p, name), p, name);
}

Result link_infer_query(Heap *heap, InferQuery q, const Prog *p) {
  switch (q.kind) {
    case INFER_QUERY_INFER: {
      const Ktm *tm = q.term;
      switch (tm->kind) {
        case KTM_VAR: {
          Str text = link_cat3(heap, str_lit("KVar "), link_show(heap, tm->index), str_lit(" is out of scope"));
          return link_repr_at(link_nat_at(&q.env, tm->index), error_make(ERROR_UNBOUND, text));
        }
        case KTM_LIT:
          return link_done(eterm_repr_new(heap, link_nat_repr()));
        case KTM_GLOBAL:
          return link_global_repr(heap, p, tm->name);
        case KTM_ERASED:
          return link_done(eterm_repr_new(heap, link_any_repr()));
        case KTM_LET: {
          Result r = link_infer_query(heap, link_infer_of(tm->value, q.env), p);
          if (r.kind == RESULT_FAIL) return r;
          return link_infer_query(heap, link_infer_of(tm->body, link_cons(heap, r.value, &q.env)), p);
        }
        case KTM_CLOS:
          return link_done(eterm_repr_new(heap, eterm_rfunc(eterm_tid(link_fn_key(heap, tm->arity)))));
        case KTM_APP:
        case KTM_TAIL:
          return link_infer_query(heap, link_infer_app(link_head_kind(p, tm->head), tm->head, tm->args, q.env), p);
        case KTM_STRUCT:
          return link_done(eterm_repr_new(heap, eterm_rstruct(tm->tid)));
        case KTM_TAG:
          return link_done(eterm_repr_new(heap, eterm_runion(tm->tid)));
        case KTM_PROJ: {
          Result rs = link_fields_of(heap, tm->tid.name);
          if (rs.kind == RESULT_FAIL) return rs;
          Str text = link_cat4(heap, str_lit("field "), link_show(heap, tm->index), str_lit(" of "), tm->tid.name);
          return link_repr_at(link_nat_at(rs.value, tm->index), error_make(ERROR_WRONG_LEG, text));
        }
        case KTM_CASE: {
          const KtmBranch *b = link_item_at(&tm->branches, 0);
          if (b == NULL) return link_done(eterm_repr_new(heap, link_any_repr()));
          Result bindings = link_branch_binders(heap, p, eterm_runion(tm->tid), *b);
          if (bindings.kind == RESULT_FAIL) return bindings;
          return link_infer_query(heap, link_infer_of(b->body, link_concat(heap, bindings.value, &q.env)), p);
        }
        case KTM_DELAY:
          return link_not_yet("KDelay arrives at M2");
        case KTM_FORCE:
          return link_not_yet("KForce arrives at M2");
      }
      break;
    }
    case INFER_QUERY_APP: {
      Nat count = link_len(heap, &q.args);
      switch (q.head_kind.kind) {
        case LINK_HEAD_FUN:
          return link_after_head(heap, link_len(heap, &q.head_kind.fn->params), q.head_kind.fn->result, count);
        case LINK_HEAD_PRIM:
          return link_after_head(heap, link_nat(heap, 2), link_prim_result(q.head_kind.name), count);
        case LINK_HEAD_VALUE: {
          Result r = link_infer_query(heap, link_infer_of(q.head, q.env), p);
          if (r.kind == RESULT_FAIL) return r;
          return link_steps_result(heap, *(Repr *)r.value, count);
        }
      }
      break;
    }
  }
  return link_fail(error_host_oom());
}

/* env: Repr pointers in list order. */
Result link_infer(Heap *heap, const Prog *p, const Vec *env, const Ktm *tm) {
  return link_infer_query(heap, link_infer_of(tm, *env), p);
}

/* f: Maybe<Fn> as a LinkFn pointer, NULL = None; the Done value is the first count params. */
Result link_capture_found(Heap *heap, const LinkFn *f, Str name, Nat count) {
  if (f == NULL) return link_fail(error_make(ERROR_UNBOUND, link_cat(heap, str_lit("no function named "), name)));
  if (nat_cmp(count, link_len(heap, &f->params)) > 0)
    return link_fail(error_make(ERROR_MISMATCH, link_cat(heap, str_lit("invalid capture count for "), name)));
  NatToU64 n = nat_to_u64(count);
  return link_done(link_vec_box(heap, link_take(heap, &f->params, n.status == NAT_TO_U64_OK ? n.value : f->params.len)));
}

Result link_capture_reprs(Heap *heap, const Prog *p, Str name, Nat count) {
  return link_capture_found(heap, link_lookup(&p->funs, name), name, count);
}

Keys link_no_keys(Heap *heap) {
  Keys k = keys_zero;
  k.types = vec_new(heap->arena);
  k.funcs = vec_new(heap->arena);
  return k;
}

Keys link_merge(Heap *heap, Keys a, Keys b) {
  Keys k = keys_zero;
  k.types = link_concat(heap, &a.types, &b.types);
  k.funcs = link_concat(heap, &a.funcs, &b.funcs);
  return k;
}

/* ks: Keys pointers. */
Keys link_merge_all(Heap *heap, const Vec *ks) {
  Keys acc = link_no_keys(heap);
  for (size_t i = 0; i < ks->len; i++) {
    const Keys *k = link_item_at(ks, i);
    if (k == NULL) continue;
    acc = link_merge(heap, acc, *k);
  }
  return acc;
}

Keys link_ty_key(Heap *heap, Str k, Tspec s) {
  Keys out = link_no_keys(heap);
  link_push(heap, &out.types, link_row_new(heap, k, link_tspec_new(heap, s)));
  return out;
}

Keys link_fn_key_of(Heap *heap, Str k, Fspec s) {
  Keys out = link_no_keys(heap);
  link_push(heap, &out.funcs, link_row_new(heap, k, link_fspec_new(heap, s)));
  return out;
}

Keys link_fnty_keys(Heap *heap, Nat n) {
  return link_merge(heap, link_ty_key(heap, str_lit("clos"), link_ts_clos()),
                    link_ty_key(heap, link_fn_key(heap, n), link_ts_fn(n)));
}

Keys link_nat_data_keys(Heap *heap) {
  return link_merge(heap, link_ty_key(heap, str_lit("nat-limbs"), link_ts_limbs()),
                    link_ty_key(heap, str_lit("nat-big"), link_ts_big_nat()));
}

/* ps: Str pointers. */
Keys link_runtime_key(Heap *heap, Str n, Vec ps, Str r) {
  return link_merge(heap, link_ty_key(heap, link_cat(heap, str_lit("nat-sig:"), n), link_ts_runtime(ps, r)),
                    link_fn_key_of(heap, link_cat(heap, str_lit("nat-runtime:"), n), link_fs_runtime(n)));
}

/* The runtime_key rows of nat_runtime_keys, in the Bend order. */
typedef struct {
  const char *name;
  const char *params[6];
  size_t count;
  const char *result;
} RuntimeSig;

static const RuntimeSig runtime_sigs[] = {
    {"length", {"n"}, 1, "i"},
    {"digit", {"n", "i"}, 2, "i"},
    {"copy", {"a", "a", "i", "i"}, 4, "a"},
    {"normal", {"a", "i"}, 2, "n"},
    {"compareDigits", {"n", "n", "i"}, 3, "i"},
    {"compare", {"n", "n"}, 2, "i"},
    {"addLoop", {"n", "n", "a", "i", "i", "i"}, 6, "n"},
    {"subLoop", {"n", "n", "a", "i", "i", "i"}, 6, "n"},
    {"mulLoop", {"n", "n", "a", "i", "i", "i"}, 6, "n"},
    {"slowAdd", {"n", "n"}, 2, "n"},
    {"slowSub", {"n", "n"}, 2, "n"},
    {"slowMul", {"n", "n"}, 2, "n"},
    {"natAdd", {"n", "n"}, 2, "n"},
    {"natSub", {"n", "n"}, 2, "n"},
    {"natMul", {"n", "n"}, 2, "n"},
    {"natEq", {"n", "n"}, 2, "n"},
    {"natLt", {"n", "n"}, 2, "n"},
};

Keys link_nat_runtime_keys(Heap *heap) {
  Keys acc = link_nat_data_keys(heap);
  size_t rows = sizeof runtime_sigs / sizeof runtime_sigs[0];
  for (size_t i = 0; i < rows; i++) {
    const RuntimeSig *sig = &runtime_sigs[i];
    Vec ps = vec_new(heap->arena);
    for (size_t j = 0; j < sig->count && j < 6; j++) link_push(heap, &ps, link_str_box(heap, str_lit(sig->params[j])));
    acc = link_merge(heap, acc, link_runtime_key(heap, str_lit(sig->name), ps, str_lit(sig->result)));
  }
  return acc;
}

/* rs: Repr pointers; the printed reprs joined by ",". */
Str link_repr_texts(Heap *heap, const Vec *rs) {
  Str out = str_empty();
  bool first = true;
  for (size_t i = 0; i < rs->len; i++) {
    const Repr *r = link_item_at(rs, i);
    if (r == NULL) continue;
    Str text = eterm_print_repr(heap, *r);
    out = first ? text : link_cat3(heap, out, str_lit(","), text);
    first = false;
  }
  return out;
}

Str link_leg_key(Heap *heap, const Vec *rs) {
  return link_cat3(heap, str_lit("leg<"), link_repr_texts(heap, rs), str_lit(">"));
}

Str link_sig_key(Heap *heap, const Vec *ps, Repr r) {
  return link_cat5(heap, str_lit("sig<"), link_repr_texts(heap, ps), str_lit("->"), eterm_print_repr(heap, r),
                   str_lit(">"));
}

Str link_env_tid(Heap *heap, const Vec *rs) {
  return link_cat3(heap, str_lit("tuple<"), link_repr_texts(heap, rs), str_lit(">"));
}

Str link_wrap_fkey(Heap *heap, Str n, Nat c) {
  return link_cat4(heap, str_lit("wrap:"), n, str_lit(":"), link_show(heap, c));
}

Str link_apply_fkey(Heap *heap, Nat n) { return link_cat(heap, str_lit("apply:"), link_show(heap, n)); }

Str link_apply_ty_key(Heap *heap, Nat n) {
  return link_cat3(heap, str_lit("applyfn<"), link_show(heap, n), str_lit(">"));
}

Str link_pap_key(Heap *heap, Nat m, Nat k) {
  return link_cat5(heap, str_lit("pap<"), link_show(heap, m), str_lit(","), link_show(heap, k), str_lit(">"));
}

Str link_papw_fkey(Heap *heap, Nat m, Nat k) {
  return link_cat4(heap, str_lit("papw:"), link_show(heap, m), str_lit(":"), link_show(heap, k));
}

/* Maybe<C.Pair<String, List<E.Repr>>>: a Pair with first = Str *, second = Vec * (Repr pointers);
 * NULL = None. */
Pair *link_leg_type(Heap *heap, LinkLeg l) {
  Str t;
  Vec rs;
  if (!link_leg_parts(heap, l, &t, &rs)) return NULL;
  return link_row_new(heap, t, link_vec_box(heap, rs));
}

Keys link_apply_keys(Heap *heap, Nat k) {
  Keys types = link_merge(heap, link_ty_key(heap, str_lit("clos"), link_ts_clos()),
                          link_ty_key(heap, link_apply_ty_key(heap, k), link_ts_apply(k)));
  return link_merge(heap, types, link_fn_key_of(heap, link_apply_fkey(heap, k), link_fs_apply(k)));
}

Result link_need_apply_positive(Heap *heap, Repr r, Nat k) {
  switch (r.kind) {
    case REPR_FUNC: {
      Result m = link_arity_of_fn(heap, r.tid.name);
      if (m.kind == RESULT_FAIL) return m;
      return link_keys_done(heap, link_apply_keys(heap, k));
    }
    case REPR_UNION:
      return link_keys_done(heap, link_apply_keys(heap, k));
    case REPR_THUNK:
      return link_not_yet("KForce arrives at M2");
    case REPR_I31:
    case REPR_STRUCT:
      return link_fail(error_make(ERROR_MISMATCH, str_lit("an application head is not a function")));
  }
  return link_fail(error_host_oom());
}

Result link_need_apply(Heap *heap, Repr r, Nat k) {
  if (nat_is_zero(k))
    return link_keys_done(heap, link_nullary_closure(heap, r) ? link_apply_keys(heap, nat_zero()) : link_no_keys(heap));
  return link_need_apply_positive(heap, r, k);
}

Result link_collect(Heap *heap, Need q, const Prog *p) {
  switch (q.kind) {
    case NEED_REPR:
      switch (q.repr.kind) {
        case REPR_I31:
          return link_keys_done(heap, link_no_keys(heap));
        case REPR_UNION:
          return link_keys_done(heap, str_eq(q.repr.tid.name, str_lit("nat")) ? link_nat_data_keys(heap)
                                                                              : link_no_keys(heap));
        case REPR_STRUCT:
          return link_collect(heap, link_nstruct(q.repr.tid.name), p);
        case REPR_FUNC: {
          Result n = link_arity_of_fn(heap, q.repr.tid.name);
          if (n.kind == RESULT_FAIL) return n;
          return link_keys_done(heap, link_fnty_keys(heap, *(Nat *)n.value));
        }
        case REPR_THUNK:
          return link_not_yet("RThunk arrives at M2");
      }
      break;
    case NEED_REPRS: {
      Keys acc = link_no_keys(heap);
      for (size_t i = 0; i < q.reprs.len; i++) {
        const Repr *r = link_item_at(&q.reprs, i);
        if (r == NULL) continue;
        Result k = link_collect(heap, link_nrepr(*r), p);
        if (k.kind == RESULT_FAIL) return k;
        acc = link_merge(heap, acc, link_keys_of(k));
      }
      return link_keys_done(heap, acc);
    }
    case NEED_STRUCT: {
      Result rs = link_fields_of(heap, q.tid);
      if (rs.kind == RESULT_FAIL) return rs;
      Vec fields = *(Vec *)rs.value;
      Result k = link_collect(heap, link_nreprs(fields), p);
      if (k.kind == RESULT_FAIL) return k;
      return link_keys_done(heap, link_merge(heap, link_ty_key(heap, q.tid, link_ts_struct(fields)), link_keys_of(k)));
    }
    case NEED_SUM: {
      Result ls = link_sum_legs_p(heap, p, q.tid);
      if (ls.kind == RESULT_FAIL) return ls;
      return link_collect(heap, link_nlegs(*(Vec *)ls.value), p);
    }
    case NEED_LEGS: {
      Keys acc = link_no_keys(heap);
      for (size_t i = 0; i < q.legs.len; i++) {
        const LinkLeg *l = link_item_at(&q.legs, i);
        Str t;
        Vec rs;
        if (l == NULL || !link_leg_parts(heap, *l, &t, &rs)) continue;
        Result k = link_collect(heap, link_nreprs(rs), p);
        if (k.kind == RESULT_FAIL) return k;
        acc = link_merge(heap, acc, link_merge(heap, link_keys_of(k), link_ty_key(heap, t, link_ts_leg(rs))));
      }
      return link_keys_done(heap, acc);
    }
    case NEED_WRAP:
      switch (q.head_kind.kind) {
        case LINK_HEAD_FUN: {
          const LinkFn *fn = q.head_kind.fn;
          Result kp = link_collect(heap, link_nreprs(fn->params), p);
          if (kp.kind == RESULT_FAIL) return kp;
          Result kr = link_collect(heap, link_nrepr(fn->result), p);
          if (kr.kind == RESULT_FAIL) return kr;
          Keys wrap = link_fn_key_of(heap, link_wrap_fkey(heap, q.name, nat_zero()), link_fs_wrap(q.name, nat_zero()));
          Keys head = link_merge(heap, link_fnty_keys(heap, link_len(heap, &fn->params)), wrap);
          return link_keys_done(heap, link_merge(heap, head, link_merge(heap, link_keys_of(kp), link_keys_of(kr))));
        }
        case LINK_HEAD_PRIM: {
          Keys wrap = link_fn_key_of(heap, link_wrap_fkey(heap, q.name, nat_zero()), link_fs_wrap(q.name, nat_zero()));
          Keys head = link_merge(heap, link_nat_runtime_keys(heap), link_fnty_keys(heap, link_nat(heap, 2)));
          return link_keys_done(heap, link_merge(heap, head, wrap));
        }
        case LINK_HEAD_VALUE: {
          Result r = link_global_repr(heap, p, q.name);
          if (r.kind == RESULT_FAIL) return r;
          return link_keys_done(heap, link_no_keys(heap));
        }
      }
      break;
    case NEED_GLOBAL:
      if (q.head_kind.kind == LINK_HEAD_FUN && q.head_kind.fn->params.len == 0)
        return link_keys_done(heap, link_no_keys(heap));
      return link_collect(heap, link_nwrap(q.name, q.head_kind), p);
    case NEED_WALK: {
      const Ktm *tm = q.term;
      switch (tm->kind) {
        case KTM_VAR:
          return link_keys_done(heap, link_no_keys(heap));
        case KTM_LIT:
          return link_keys_done(heap, link_nat_data_keys(heap));
        case KTM_ERASED:
          return link_keys_done(heap, link_no_keys(heap));
        case KTM_GLOBAL:
          return link_collect(heap, link_nglobal(tm->name, link_head_kind(p, tm)), p);
        case KTM_LET: {
          Result kv = link_collect(heap, link_nwalk(tm->value, q.env), p);
          if (kv.kind == RESULT_FAIL) return kv;
          Result r = link_infer(heap, p, &q.env, tm->value);
          if (r.kind == RESULT_FAIL) return r;
          Result kr = link_collect(heap, link_nrepr(*(Repr *)r.value), p);
          if (kr.kind == RESULT_FAIL) return kr;
          Result kb = link_collect(heap, link_nwalk(tm->body, link_cons(heap, r.value, &q.env)), p);
          if (kb.kind == RESULT_FAIL) return kb;
          return link_keys_done(heap, link_merge(heap, link_merge(heap, link_keys_of(kv), link_keys_of(kr)), link_keys_of(kb)));
        }
        case KTM_CLOS: {
          Str f = tm->fid.name;
          Result kc = link_collect(heap, link_nterms(tm->captures, q.env), p);
          if (kc.kind == RESULT_FAIL) return kc;
          Nat count = link_len(heap, &tm->captures);
          Result rs = link_capture_reprs(heap, p, f, count);
          if (rs.kind == RESULT_FAIL) return rs;
          Vec reprs = *(Vec *)rs.value;
          Result kr = link_collect(heap, link_nreprs(reprs), p);
          if (kr.kind == RESULT_FAIL) return kr;
          Vec env_reprs = reprs.len == 0
                              ? vec_new(heap->arena)
                              : link_singleton(heap, eterm_repr_new(heap, eterm_rstruct(eterm_tid(link_env_tid(heap, &reprs)))));
          Result ke = link_collect(heap, link_nreprs(env_reprs), p);
          if (ke.kind == RESULT_FAIL) return ke;
          Keys wrap = link_fn_key_of(heap, link_wrap_fkey(heap, f, count), link_fs_wrap(f, count));
          Keys head = link_merge(heap, link_merge(heap, link_fnty_keys(heap, tm->arity), wrap), link_keys_of(ke));
          return link_keys_done(heap, link_merge(heap, head, link_merge(heap, link_keys_of(kc), link_keys_of(kr))));
        }
        case KTM_APP:
        case KTM_TAIL:
          return link_collect(heap, link_napp(tm->head, tm->args, q.env), p);
        case KTM_STRUCT: {
          Result kt = link_collect(heap, link_nstruct(tm->tid.name), p);
          if (kt.kind == RESULT_FAIL) return kt;
          Result kf = link_collect(heap, link_nterms(tm->fields, q.env), p);
          if (kf.kind == RESULT_FAIL) return kf;
          return link_keys_done(heap, link_merge(heap, link_keys_of(kt), link_keys_of(kf)));
        }
        case KTM_PROJ: {
          Result kt = link_collect(heap, link_nstruct(tm->tid.name), p);
          if (kt.kind == RESULT_FAIL) return kt;
          Result kx = link_collect(heap, link_nwalk(tm->value, q.env), p);
          if (kx.kind == RESULT_FAIL) return kx;
          return link_keys_done(heap, link_merge(heap, link_keys_of(kt), link_keys_of(kx)));
        }
        case KTM_TAG: {
          Result kt = link_collect(heap, link_nsum(tm->tid.name), p);
          if (kt.kind == RESULT_FAIL) return kt;
          Result kp = link_collect(heap, link_nterms(tm->fields, q.env), p);
          if (kp.kind == RESULT_FAIL) return kp;
          return link_keys_done(heap, link_merge(heap, link_keys_of(kt), link_keys_of(kp)));
        }
        case KTM_CASE: {
          Result ks = link_collect(heap, link_nwalk(tm->scrut, q.env), p);
          if (ks.kind == RESULT_FAIL) return ks;
          Need legs = tm->branches.len == 0 ? link_nlegs(vec_new(heap->arena)) : link_nsum(eterm_tid_text(tm->tid));
          Result kt = link_collect(heap, legs, p);
          if (kt.kind == RESULT_FAIL) return kt;
          Result r = link_infer(heap, p, &q.env, tm);
          if (r.kind == RESULT_FAIL) return r;
          Result kr = link_collect(heap, link_nrepr(*(Repr *)r.value), p);
          if (kr.kind == RESULT_FAIL) return kr;
          Result kb = link_collect(heap, link_nbranches(tm->branches, eterm_runion(tm->tid), q.env), p);
          if (kb.kind == RESULT_FAIL) return kb;
          Keys head = link_merge(heap, link_keys_of(ks), link_keys_of(kt));
          return link_keys_done(heap, link_merge(heap, head, link_merge(heap, link_keys_of(kr), link_keys_of(kb))));
        }
        case KTM_DELAY:
          return link_not_yet("KDelay arrives at M2");
        case KTM_FORCE:
          return link_not_yet("KForce arrives at M2");
      }
      break;
    }
    case NEED_TERMS: {
      Keys acc = link_no_keys(heap);
      for (size_t i = 0; i < q.terms.len; i++) {
        const Ktm *t = link_item_at(&q.terms, i);
        if (t == NULL) continue;
        Result k = link_collect(heap, link_nwalk(t, q.env), p);
        if (k.kind == RESULT_FAIL) return k;
        acc = link_merge(heap, acc, link_keys_of(k));
      }
      return link_keys_done(heap, acc);
    }
    case NEED_BRANCHES: {
      Keys acc = link_no_keys(heap);
      for (size_t i = 0; i < q.branches.len; i++) {
        const KtmBranch *b = link_item_at(&q.branches, i);
        if (b == NULL) continue;
        Result bindings = link_branch_binders(heap, p, q.scrut, *b);
        if (bindings.kind == RESULT_FAIL) return bindings;
        Vec bs = *(Vec *)bindings.value;
        Result kb = link_collect(heap, link_nwalk(b->body, link_concat(heap, &bs, &q.env)), p);
        if (kb.kind == RESULT_FAIL) return kb;
        Result kr = link_collect(heap, link_nreprs(bs), p);
        if (kr.kind == RESULT_FAIL) return kr;
        acc = link_merge(heap, acc, link_merge(heap, link_keys_of(kb), link_keys_of(kr)));
      }
      return link_keys_done(heap, acc);
    }
    case NEED_APP: {
      Result ka = link_collect(heap, link_nterms(q.args, q.env), p);
      if (ka.kind == RESULT_FAIL) return ka;
      Need apply = link_napply(link_head_kind(p, q.head), q.head, link_len(heap, &q.args), q.env);
      Result kh = link_collect(heap, apply, p);
      if (kh.kind == RESULT_FAIL) return kh;
      return link_keys_done(heap, link_merge(heap, link_keys_of(ka), link_keys_of(kh)));
    }
    case NEED_APPLY:
      switch (q.head_kind.kind) {
        case LINK_HEAD_FUN: {
          if (nat_is_zero(q.count)) return link_collect(heap, link_nwalk(q.head, q.env), p);
          Nat m = link_len(heap, &q.head_kind.fn->params);
          /* nat_cmp is hoisted out of the argument list: tcc 0.9.28rc (AArch64) loads the second Nat with
           * `ldp x2, x3` over the register that holds the address of q.count (PROGRESS hop 36, hop 154). */
          bool full = nat_cmp(q.count, m) >= 0;
          Need saturated = link_nsaturated(q.head_kind.name, m, q.head_kind.fn->result, q.count, full);
          return link_collect(heap, saturated, p);
        }
        case LINK_HEAD_PRIM: {
          if (nat_is_zero(q.count)) return link_collect(heap, link_nwalk(q.head, q.env), p);
          Nat two = link_nat(heap, 2);
          /* hoisted for the same tcc ldp miscompile as the LINK_HEAD_FUN arm */
          bool full = nat_cmp(q.count, two) >= 0;
          Need saturated = link_nsaturated(q.head_kind.name, two, link_prim_result(q.head_kind.name), q.count, full);
          Result ks = link_collect(heap, saturated, p);
          if (ks.kind == RESULT_FAIL) return ks;
          return link_keys_done(heap, link_merge(heap, link_nat_runtime_keys(heap), link_keys_of(ks)));
        }
        case LINK_HEAD_VALUE: {
          Result kh = link_collect(heap, link_nwalk(q.head, q.env), p);
          if (kh.kind == RESULT_FAIL) return kh;
          Result r = link_infer(heap, p, &q.env, q.head);
          if (r.kind == RESULT_FAIL) return r;
          Result ka = link_need_apply(heap, *(Repr *)r.value, q.count);
          if (ka.kind == RESULT_FAIL) return ka;
          return link_keys_done(heap, link_merge(heap, link_keys_of(kh), link_keys_of(ka)));
        }
      }
      break;
    case NEED_SATURATED: {
      if (q.saturated) return link_need_apply(heap, q.result, link_nat_sub(heap, q.count, q.arity));
      Result kw = link_collect(heap, link_nwrap(q.name, link_head_of_global(p, q.name)), p);
      if (kw.kind == RESULT_FAIL) return kw;
      Result ka = link_need_apply(heap, eterm_rfunc(eterm_tid(link_fn_key(heap, q.arity))), q.count);
      if (ka.kind == RESULT_FAIL) return ka;
      return link_keys_done(heap, link_merge(heap, link_keys_of(kw), link_keys_of(ka)));
    }
    case NEED_FUN: {
      const LinkFn *fn = q.fn;
      Result kp = link_collect(heap, link_nreprs(fn->params), p);
      if (kp.kind == RESULT_FAIL) return kp;
      Result kr = link_collect(heap, link_nrepr(fn->result), p);
      if (kr.kind == RESULT_FAIL) return kr;
      Result kb = link_collect(heap, link_nwalk(fn->body, link_reverse(heap, &fn->params)), p);
      if (kb.kind == RESULT_FAIL) return kb;
      Keys sig = link_ty_key(heap, link_sig_key(heap, &fn->params, fn->result), link_ts_sig(fn->params, fn->result));
      Keys prog = link_fn_key_of(heap, link_cat(heap, str_lit("fun:"), q.name), link_fs_prog(q.name));
      Keys head = link_merge(heap, link_merge(heap, sig, prog), link_merge(heap, link_keys_of(kp), link_keys_of(kr)));
      return link_keys_done(heap, link_merge(heap, head, link_keys_of(kb)));
    }
    case NEED_FUNS: {
      Keys acc = link_no_keys(heap);
      for (size_t i = 0; i < q.funs.len; i++) {
        const Pair *row = link_item_at(&q.funs, i);
        const Str *name = row == NULL ? NULL : row->first;
        if (name == NULL) continue;
        Result k = link_collect(heap, link_nfun(*name, row->second), p);
        if (k.kind == RESULT_FAIL) return k;
        acc = link_merge(heap, acc, link_keys_of(k));
      }
      return link_keys_done(heap, acc);
    }
  }
  return link_fail(error_host_oom());
}

/* ---- Part 3 (Bend lines 763-1180): the layout ---- */

static const Linked linked_zero;

/* The key of a C.Pair<String, A> row; "" for a missing row. */
static Str link_row_key(const Pair *row) {
  if (row == NULL || row->first == NULL) return str_empty();
  return *(const Str *)row->first;
}

/* The Nat at index i of a Nat list; zero beyond the list. */
static Nat link_nat_item(const Vec *xs, size_t i) {
  const Nat *n = link_item_at(xs, i);
  return n == NULL ? nat_zero() : *n;
}

/* C.Pair{first, second} boxed; NULL after a failed allocation. */
static Pair *link_pair_of(Heap *heap, void *first, void *second) {
  Pair *p = arena_alloc(heap->arena, sizeof(Pair));
  if (p == NULL) {
    heap->oom = true;
    return NULL;
  }
  p->first = first;
  p->second = second;
  return p;
}

/* The list without its head. */
static Vec link_tail(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 1; i < xs->len; i++) link_push(heap, &out, link_item_at(xs, i));
  return out;
}

/* A Nat as a host count; a Nat beyond the host range is a failed allocation. */
static size_t link_count(Heap *heap, Nat n) {
  NatToU64 v = nat_to_u64(n);
  if (v.status != NAT_TO_U64_OK) {
    heap->oom = true;
    return 0;
  }
  return (size_t)v.value;
}

/* U32.from_nat(n): the low 32 bits. */
static uint32_t link_u32(Nat n) {
  NatToU64 v = nat_to_u64(n);
  return v.status == NAT_TO_U64_OK ? (uint32_t)v.value : 0;
}

static uint32_t *link_u32_box(Heap *heap, uint32_t value) {
  uint32_t *p = arena_alloc(heap->arena, sizeof(uint32_t));
  if (p == NULL) {
    heap->oom = true;
    return NULL;
  }
  *p = value;
  return p;
}

/* The U32 of a Done index result; 0 after a failed allocation. */
static uint32_t link_u32_of(Result r) { return r.value == NULL ? 0 : *(const uint32_t *)r.value; }

/* G.Ref{heap type} boxed */
static GcValtype *link_vt_ref(Heap *heap, GcHeaptypeKind kind, uint32_t index) {
  return gc_valtype_box(heap, gc_valtype_ref(gc_heaptype(kind, index)));
}

/* G.I32{} boxed */
static GcValtype *link_vt_i32(Heap *heap) { return gc_valtype_box(heap, gc_valtype_i32()); }

void *link_first(const Pair *p) { return p == NULL ? NULL : p->first; }

void *link_second(const Pair *p) { return p == NULL ? NULL : p->second; }

Vec link_keys_types(Keys k) { return k.types; }

Vec link_keys_funcs(Keys k) { return k.funcs; }

Vec link_prog_funcs(const Prog *p) { return p->funs; }

Vec link_prog_mus(const Prog *p) { return p->mus; }

bool link_contains_name(const Vec *xs, Str n) {
  for (size_t i = 0; i < xs->len; i++) {
    if (str_eq(link_str_at(xs, i), n)) return true;
  }
  return false;
}

bool link_contains_nat(const Vec *xs, Nat n) {
  for (size_t i = 0; i < xs->len; i++) {
    if (nat_cmp(link_nat_item(xs, i), n) == 0) return true;
  }
  return false;
}

/* dedup_rows(xs, seen): the rows whose key is not in seen nor in an earlier row, in order. */
Vec link_dedup_rows(Heap *heap, const Vec *xs, const Vec *seen) {
  Vec empty = vec_new(heap->arena);
  Vec names = link_concat(heap, seen, &empty);
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    Pair *row = link_item_at(xs, i);
    Str k = link_row_key(row);
    if (!link_contains_name(&names, k)) link_push(heap, &out, row);
    link_push(heap, &names, link_str_box(heap, k));
  }
  return out;
}

Vec link_dedup_nat(Heap *heap, const Vec *xs, const Vec *seen) {
  Vec empty = vec_new(heap->arena);
  Vec done = link_concat(heap, seen, &empty);
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    void *item = link_item_at(xs, i);
    if (!link_contains_nat(&done, link_nat_item(xs, i))) link_push(heap, &out, item);
    link_push(heap, &done, item);
  }
  return out;
}

Vec link_arity_keys(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const Tspec *s = link_second(link_item_at(ts, i));
    if (s != NULL && s->kind == TSPEC_FN) link_push(heap, &out, erase_util_nat_new(heap, s->arity));
  }
  return out;
}

Vec link_call_keys(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const Tspec *s = link_second(link_item_at(ts, i));
    if (s != NULL && s->kind == TSPEC_APPLY) link_push(heap, &out, erase_util_nat_new(heap, s->arity));
  }
  return out;
}

Vec link_arity_diffs(Heap *heap, const Vec *ks, Nat m) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ks->len; i++) {
    Nat k = link_nat_item(ks, i);
    if (nat_cmp(m, k) > 0) link_push(heap, &out, erase_util_nat_new(heap, link_nat_sub(heap, m, k)));
  }
  return out;
}

Vec link_call_diffs(Heap *heap, const Vec *ms, Nat k) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    Nat m = link_nat_item(ms, i);
    if (nat_cmp(k, m) > 0 && !nat_is_zero(m)) link_push(heap, &out, erase_util_nat_new(heap, link_nat_sub(heap, k, m)));
  }
  return out;
}

Vec link_all_arity_diffs(Heap *heap, const Vec *ms, const Vec *ks) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    Vec d = link_arity_diffs(heap, ks, link_nat_item(ms, i));
    link_append(heap, &out, &d);
  }
  return out;
}

Vec link_all_call_diffs(Heap *heap, const Vec *ks, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ks->len; i++) {
    Vec d = link_call_diffs(heap, ms, link_nat_item(ks, i));
    link_append(heap, &out, &d);
  }
  return out;
}

Nat link_max_nat(const Vec *xs) {
  Nat best = nat_zero();
  for (size_t i = 0; i < xs->len; i++) {
    Nat n = link_nat_item(xs, i);
    if (nat_cmp(n, best) > 0) best = n;
  }
  return best;
}

/* close_arities(fuel, a, k): C.Pair{a, k} boxed (first and second are Vec * of Nat *). */
Pair *link_close_arities(Heap *heap, Nat fuel, Vec a, Vec k) {
  Vec empty = vec_new(heap->arena);
  Nat one = link_nat(heap, 1);
  while (!nat_is_zero(fuel)) {
    Vec ad = link_all_arity_diffs(heap, &a, &k);
    Vec a1 = link_concat(heap, &a, &ad);
    Vec a2 = link_dedup_nat(heap, &a1, &empty);
    Vec kd = link_all_call_diffs(heap, &k, &a);
    Vec k1 = link_concat(heap, &k, &kd);
    Vec k2 = link_dedup_nat(heap, &k1, &empty);
    bool same = a.len == a2.len && k.len == k2.len;
    fuel = same ? nat_zero() : link_nat_sub(heap, fuel, one);
    a = a2;
    k = k2;
  }
  return link_pair_of(heap, link_vec_box(heap, a), link_vec_box(heap, k));
}

Keys link_helper_pap(Heap *heap, const Vec *ks, Nat m) {
  Keys acc = link_no_keys(heap);
  for (size_t i = 0; i < ks->len; i++) {
    Nat k = link_nat_item(ks, i);
    if (nat_cmp(m, k) > 0) {
      Nat d = link_nat_sub(heap, m, k);
      Keys pap = link_ty_key(heap, link_pap_key(heap, m, k), link_ts_pap(k));
      Keys papw = link_fn_key_of(heap, link_papw_fkey(heap, m, k), link_fs_papw(m, k));
      Keys fn = link_ty_key(heap, link_fn_key(heap, d), link_ts_fn(d));
      acc = link_merge(heap, acc, link_merge(heap, pap, link_merge(heap, papw, fn)));
    }
  }
  return acc;
}

Keys link_helper_arities(Heap *heap, const Vec *ms, const Vec *ks) {
  Keys acc = link_no_keys(heap);
  for (size_t i = 0; i < ms->len; i++) {
    Nat m = link_nat_item(ms, i);
    Keys fn = link_ty_key(heap, link_fn_key(heap, m), link_ts_fn(m));
    acc = link_merge(heap, acc, link_merge(heap, fn, link_helper_pap(heap, ks, m)));
  }
  return acc;
}

Keys link_helper_calls(Heap *heap, const Vec *ks) {
  Keys acc = link_no_keys(heap);
  for (size_t i = 0; i < ks->len; i++) {
    Nat k = link_nat_item(ks, i);
    Keys ty = link_ty_key(heap, link_apply_ty_key(heap, k), link_ts_apply(k));
    Keys fn = link_fn_key_of(heap, link_apply_fkey(heap, k), link_fs_apply(k));
    acc = link_merge(heap, acc, link_merge(heap, ty, fn));
  }
  return acc;
}

Keys link_helper_keys(Heap *heap, const Vec *a, const Vec *k) {
  return link_merge(heap, link_helper_calls(heap, k), link_helper_arities(heap, a, k));
}

unsigned link_trank(const Tspec *t) {
  unsigned rank = 0;
  switch (t->kind) {
    case TSPEC_LIMBS: rank = 0; break;
    case TSPEC_BIG_NAT: rank = 1; break;
    case TSPEC_CLOS: rank = 2; break;
    case TSPEC_FN: rank = 3; break;
    case TSPEC_STRUCT: rank = 4; break;
    case TSPEC_LEG: rank = 5; break;
    case TSPEC_PAP: rank = 6; break;
    case TSPEC_RUNTIME: rank = 7; break;
    case TSPEC_SIG: rank = 7; break;
    case TSPEC_APPLY: rank = 8; break;
    case TSPEC_ENTRY: rank = 9; break;
  }
  return rank;
}

unsigned link_frank(const Fspec *f) {
  unsigned rank = 0;
  switch (f->kind) {
    case FSPEC_PROG: rank = 0; break;
    case FSPEC_WRAP: rank = 1; break;
    case FSPEC_APPLY: rank = 2; break;
    case FSPEC_PAPW: rank = 3; break;
    case FSPEC_RUNTIME: rank = 4; break;
    case FSPEC_ENTRY: rank = 4; break;
  }
  return rank;
}

/* text_depth(cs): the count of '<' in the key */
size_t link_text_depth(Str s) {
  size_t depth = 0;
  for (size_t i = 0; i < s.len; i++) {
    StrAt at = str_at(s, i);
    if (at.status == STR_INDEX_OK && at.value == '<') depth++;
  }
  return depth;
}

bool link_type_before(const Pair *a, const Pair *b) {
  unsigned ra = link_trank(link_second(a));
  unsigned rb = link_trank(link_second(b));
  return ra < rb || (ra == rb && link_text_depth(link_row_key(a)) <= link_text_depth(link_row_key(b)));
}

/* insert_type(xs, x): x before the first row it goes before, else last */
Vec link_insert_type(Heap *heap, const Vec *xs, Pair *x) {
  Vec out = vec_new(heap->arena);
  bool placed = false;
  for (size_t i = 0; i < xs->len; i++) {
    Pair *y = link_item_at(xs, i);
    if (!placed && link_type_before(x, y)) {
      link_push(heap, &out, x);
      placed = true;
    }
    link_push(heap, &out, y);
  }
  if (!placed) link_push(heap, &out, x);
  return out;
}

/* order_types(xs): insertion from the last row to the first, as the Bend recursion */
Vec link_order_types(Heap *heap, const Vec *xs) {
  Vec sorted = vec_new(heap->arena);
  for (size_t i = xs->len; i > 0; i--) sorted = link_insert_type(heap, &sorted, link_item_at(xs, i - 1));
  return sorted;
}

Vec link_insert_func(Heap *heap, const Vec *xs, Pair *x) {
  Vec out = vec_new(heap->arena);
  bool placed = false;
  unsigned rx = link_frank(link_second(x));
  for (size_t i = 0; i < xs->len; i++) {
    Pair *y = link_item_at(xs, i);
    if (!placed && rx <= link_frank(link_second(y))) {
      link_push(heap, &out, x);
      placed = true;
    }
    link_push(heap, &out, y);
  }
  if (!placed) link_push(heap, &out, x);
  return out;
}

Vec link_order_funcs(Heap *heap, const Vec *xs) {
  Vec sorted = vec_new(heap->arena);
  for (size_t i = xs->len; i > 0; i--) sorted = link_insert_func(heap, &sorted, link_item_at(xs, i - 1));
  return sorted;
}

Vec link_family_names(Heap *heap, const Vec *ms) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    const MuLeg *m = link_item_at(ms, i);
    if (m != NULL) link_push(heap, &out, link_str_box(heap, m->family));
  }
  return out;
}

/* edges_fields(rs, fam, families): C.Pair{fam, g} rows for the union fields g in families */
Vec link_edges_fields(Heap *heap, const Vec *rs, Str fam, const Vec *families) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rs->len; i++) {
    const Repr *r = link_item_at(rs, i);
    if (r != NULL && r->kind == REPR_UNION && link_contains_name(families, r->tid.name)) {
      link_push(heap, &out, link_row_new(heap, fam, link_str_box(heap, r->tid.name)));
    }
  }
  return out;
}

Vec link_mu_edges(Heap *heap, const Vec *ms, const Vec *families) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ms->len; i++) {
    const MuLeg *m = link_item_at(ms, i);
    if (m == NULL) continue;
    Vec es = link_edges_fields(heap, &m->fields, m->family, families);
    link_append(heap, &out, &es);
  }
  return out;
}

Vec link_edge_targets(Heap *heap, const Vec *es, Str name) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < es->len; i++) {
    Pair *e = link_item_at(es, i);
    if (str_eq(link_row_key(e), name)) link_push(heap, &out, link_second(e));
  }
  return out;
}

/* reach_go(todo, seen, es): the names reachable from todo; a worklist in place of the Bend
 * recursion (the result is only ever tested for membership). */
Vec link_reach_go(Heap *heap, Vec todo, Vec seen, const Vec *es) {
  for (size_t i = 0; i < todo.len; i++) {
    Str n = link_str_at(&todo, i);
    if (link_contains_name(&seen, n)) continue;
    link_push(heap, &seen, link_item_at(&todo, i));
    Vec next = link_edge_targets(heap, es, n);
    link_append(heap, &todo, &next);
  }
  return seen;
}

Vec link_reach(Heap *heap, const Vec *es, Str f) {
  return link_reach_go(heap, link_edge_targets(heap, es, f), vec_new(heap->arena), es);
}

Str link_min_compare(Cmp c, Str a, Str b) { return c == CMP_GT ? b : a; }

/* string_min(a, b): min_result((a, b), String.cmp(a, b)) */
Str link_string_min(Str a, Str b) {
  int c = str_cmp(a, b);
  return link_min_compare(c > 0 ? CMP_GT : (c == 0 ? CMP_EQ : CMP_LT), a, b);
}

Str link_component_min(Heap *heap, const Vec *fams, const Vec *es, Str f, Str best) {
  Vec from_f = link_reach(heap, es, f);
  for (size_t i = 0; i < fams->len; i++) {
    Str g = link_str_at(fams, i);
    bool mutual = link_contains_name(&from_f, g);
    if (mutual) {
      Vec from_g = link_reach(heap, es, g);
      mutual = link_contains_name(&from_g, f);
    }
    if (mutual) best = link_string_min(g, best);
  }
  return best;
}

Str link_group_mu(Heap *heap, const MuLeg *m, Str key, const Vec *fams, const Vec *es) {
  if (m == NULL) return key;
  Vec from_f = link_reach(heap, es, m->family);
  if (!link_contains_name(&from_f, m->family)) return key;
  return link_cat(heap, str_lit("group:"), link_component_min(heap, fams, es, m->family, m->family));
}

Str link_group_key(Heap *heap, Str key, const Vec *fams, const Vec *es) {
  return link_group_mu(heap, link_parse_mu_leg(heap, key), key, fams, es);
}

/* take_group(xs, g, fams, es): C.Pair{yes, no} boxed (Vec * of rows each), in order */
Pair *link_take_group(Heap *heap, const Vec *xs, Str g, const Vec *fams, const Vec *es) {
  Vec yes = vec_new(heap->arena);
  Vec no = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    Pair *row = link_item_at(xs, i);
    if (str_eq(link_group_key(heap, link_row_key(row), fams, es), g)) {
      link_push(heap, &yes, row);
    } else {
      link_push(heap, &no, row);
    }
  }
  return link_pair_of(heap, link_vec_box(heap, yes), link_vec_box(heap, no));
}

/* group_types(xs, fams, es): Vec * groups, each the head row and the later rows of its group */
Vec link_group_types(Heap *heap, const Vec *xs, const Vec *fams, const Vec *es) {
  Vec out = vec_new(heap->arena);
  Vec rest = *xs;
  while (rest.len > 0) {
    Pair *row = link_item_at(&rest, 0);
    Vec tail = link_tail(heap, &rest);
    Pair *parts = link_take_group(heap, &tail, link_group_key(heap, link_row_key(row), fams, es), fams, es);
    if (parts == NULL) break;
    const Vec *yes = parts->first;
    const Vec *no = parts->second;
    link_push(heap, &out, link_vec_box(heap, link_cons(heap, row, yes)));
    rest = *no;
  }
  return out;
}

Vec link_flatten_types(Heap *heap, const Vec *gs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    const Vec *g = link_item_at(gs, i);
    if (g != NULL) link_append(heap, &out, g);
  }
  return out;
}

/* indices(xs, index): C.Pair{key, index} rows (second = Nat *), counting from index */
Vec link_indices(Heap *heap, const Vec *xs, Nat index) {
  Vec out = vec_new(heap->arena);
  Nat n = index;
  for (size_t i = 0; i < xs->len; i++) {
    link_push(heap, &out, link_row_new(heap, link_row_key(link_item_at(xs, i)), erase_util_nat_new(heap, n)));
    n = nat_succ(heap->arena, n);
  }
  return out;
}

static bool link_is_declared(const Fspec *s) {
  if (s == NULL) return false;
  return s->kind == FSPEC_WRAP || s->kind == FSPEC_PAPW;
}

/* declared(fs, index): the indices of the FSWrap and FSPapw rows as U32 */
U32List link_declared(Heap *heap, const Vec *fs, Nat index) {
  U32List out;
  size_t count = 0;
  for (size_t i = 0; i < fs->len; i++) {
    if (link_is_declared(link_second(link_item_at(fs, i)))) count++;
  }
  uint32_t *items = count == 0 ? NULL : arena_alloc(heap->arena, count * sizeof(uint32_t));
  if (count > 0 && items == NULL) {
    heap->oom = true;
    count = 0;
  }
  size_t at = 0;
  Nat n = index;
  for (size_t i = 0; i < fs->len && at < count; i++) {
    if (link_is_declared(link_second(link_item_at(fs, i)))) items[at++] = link_u32(n);
    n = nat_succ(heap->arena, n);
  }
  out.items = items;
  out.len = count;
  return out;
}

Linked *link_linked_new(Heap *heap, Linked l) {
  Linked *p = arena_alloc(heap->arena, sizeof(Linked));
  if (p == NULL) {
    heap->oom = true;
    return NULL;
  }
  *p = l;
  return p;
}

Linked link_finish_layout(Heap *heap, const Prog *p, Keys base) {
  Vec empty = vec_new(heap->arena);
  Vec tys0 = link_dedup_rows(heap, &base.types, &empty);
  Vec fns0 = link_dedup_rows(heap, &base.funcs, &empty);
  Vec ak = link_arity_keys(heap, &tys0);
  Vec ar = link_dedup_nat(heap, &ak, &empty);
  Vec ck = link_call_keys(heap, &tys0);
  Vec ca = link_dedup_nat(heap, &ck, &empty);
  Nat fuel = nat_succ(heap->arena, nat_add(heap->arena, link_max_nat(&ar), link_max_nat(&ca)));
  Pair *closed = link_close_arities(heap, fuel, ar, ca);
  Vec a = closed == NULL ? empty : *(const Vec *)closed->first;
  Vec k = closed == NULL ? empty : *(const Vec *)closed->second;
  Keys extra = link_helper_keys(heap, &a, &k);
  Vec all_types = link_concat(heap, &tys0, &extra.types);
  Vec unique_types = link_dedup_rows(heap, &all_types, &empty);
  Vec ordered = link_order_types(heap, &unique_types);
  Vec families = link_family_names(heap, &p->mus);
  Vec es = link_mu_edges(heap, &p->mus, &families);
  Vec groups = link_group_types(heap, &ordered, &families, &es);
  Vec ts = link_flatten_types(heap, &groups);
  Vec all_funcs = link_concat(heap, &fns0, &extra.funcs);
  Vec unique_funcs = link_dedup_rows(heap, &all_funcs, &empty);
  Vec funcs = link_order_funcs(heap, &unique_funcs);
  Linked l = linked_zero;
  l.prog = *p;
  l.types = ts;
  l.groups = groups;
  l.tmap = link_indices(heap, &ts, nat_zero());
  l.funcs = funcs;
  l.fmap = link_indices(heap, &funcs, nat_zero());
  l.arities = a;
  l.calls = k;
  l.declared = link_declared(heap, &funcs, nat_zero());
  return l;
}

/* build(rows): Done{Linked *} */
Result link_build(Heap *heap, const Vec *rows) {
  Prog p = link_program_table(heap, rows);
  Result collected = link_collect(heap, link_nfuns(p.funs), &p);
  if (collected.kind == RESULT_FAIL) return collected;
  Keys k = collected.value == NULL ? link_no_keys(heap) : link_keys_of(collected);
  if (collected.value == NULL) heap->oom = true;
  Keys entry = link_merge(heap, link_ty_key(heap, str_lit("entryfn"), link_ts_entry()),
                          link_fn_key_of(heap, str_lit("entry"), link_fs_entry()));
  return link_done(link_linked_new(heap, link_finish_layout(heap, &p, link_merge(heap, k, entry))));
}

/* index_result(found, msg): Done{uint32_t *} or Fail{Err.Unbound{msg}} */
Result link_index_result(Heap *heap, const Nat *found, Str msg) {
  if (found == NULL) return link_fail(error_make(ERROR_UNBOUND, msg));
  return link_done(link_u32_box(heap, link_u32(*found)));
}

Result link_type_index(Heap *heap, const Linked *l, Str key) {
  return link_index_result(heap, link_lookup(&l->tmap, key), link_cat(heap, str_lit("no type index for "), key));
}

Result link_func_index(Heap *heap, const Linked *l, Str key) {
  return link_index_result(heap, link_lookup(&l->fmap, key), link_cat(heap, str_lit("no function index for "), key));
}

/* valtype_of(l, r): Done{GcValtype *} */
Result link_valtype_of(Heap *heap, const Linked *l, Repr r) {
  Result out = result_zero;
  Result i = result_zero;
  switch (r.kind) {
    case REPR_I31: out = link_done(link_vt_ref(heap, GC_HEAP_I31, 0)); break;
    case REPR_UNION: out = link_done(link_vt_ref(heap, GC_HEAP_EQ, 0)); break;
    case REPR_STRUCT:
      i = link_type_index(heap, l, r.tid.name);
      out = i.kind == RESULT_FAIL ? i : link_done(link_vt_ref(heap, GC_HEAP_TYPE, link_u32_of(i)));
      break;
    case REPR_FUNC:
      i = link_type_index(heap, l, str_lit("clos"));
      out = i.kind == RESULT_FAIL ? i : link_done(link_vt_ref(heap, GC_HEAP_TYPE, link_u32_of(i)));
      break;
    case REPR_THUNK: out = link_not_yet("RThunk arrives at M2"); break;
  }
  return out;
}

/* valtypes_of(rs, l): Done{Vec * of GcValtype *}, the first failure in list order */
Result link_valtypes_of(Heap *heap, const Vec *rs, const Linked *l) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < rs->len; i++) {
    const Repr *r = link_item_at(rs, i);
    if (r == NULL) continue;
    Result v = link_valtype_of(heap, l, *r);
    if (v.kind == RESULT_FAIL) return v;
    link_push(heap, &out, v.value);
  }
  return link_done(link_vec_box(heap, out));
}

/* eqs(n): n copies of G.Ref{G.HEq{}} */
Vec link_eqs(Heap *heap, Nat n) {
  Vec out = vec_new(heap->arena);
  size_t count = link_count(heap, n);
  for (size_t i = 0; i < count; i++) link_push(heap, &out, link_vt_ref(heap, GC_HEAP_EQ, 0));
  return out;
}

Result link_runtime_vtype(Heap *heap, bool other, const Linked *l, Str s) {
  if (other) {
    return link_fail(error_make(ERROR_MISMATCH, link_cat(heap, str_lit("unknown natural runtime value type: "), s)));
  }
  Result i = link_type_index(heap, l, str_lit("nat-limbs"));
  if (i.kind == RESULT_FAIL) return i;
  return link_done(link_vt_ref(heap, GC_HEAP_TYPE, link_u32_of(i)));
}

Result link_runtime_valtype(Heap *heap, const Linked *l, Str s) {
  if (str_eq(s, str_lit("i"))) return link_done(link_vt_i32(heap));
  if (str_eq(s, str_lit("n"))) return link_done(link_vt_ref(heap, GC_HEAP_EQ, 0));
  return link_runtime_vtype(heap, !str_eq(s, str_lit("a")), l, s);
}

/* runtime_vtypes(ss, l): ss are Str pointers */
Result link_runtime_vtypes(Heap *heap, const Vec *ss, const Linked *l) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ss->len; i++) {
    Result v = link_runtime_valtype(heap, l, link_str_at(ss, i));
    if (v.kind == RESULT_FAIL) return v;
    link_push(heap, &out, v.value);
  }
  return link_done(link_vec_box(heap, out));
}

/* G.CFunc{params, [result]} */
static Result link_cfunc_one(Heap *heap, Vec params, void *result) {
  return link_done(gc_comptype_func(heap, params, link_singleton(heap, result)));
}

/* G.CFunc{eqs(1 + n), [G.Ref{G.HEq{}}]} */
static Result link_cfunc_eqs(Heap *heap, Nat n) {
  return link_cfunc_one(heap, link_eqs(heap, nat_succ(heap->arena, n)), link_vt_ref(heap, GC_HEAP_EQ, 0));
}

/* comptype_of(s, l): Done{GcComptype *} */
Result link_comptype_of(Heap *heap, const Tspec *s, const Linked *l) {
  Result out = result_zero;
  Result i = result_zero;
  Result vs = result_zero;
  Result v = result_zero;
  Vec fields = vec_new(heap->arena);
  switch (s->kind) {
    case TSPEC_LIMBS: out = link_done(gc_comptype_array(heap, gc_valtype_i32())); break;
    case TSPEC_BIG_NAT:
      i = link_type_index(heap, l, str_lit("nat-limbs"));
      if (i.kind == RESULT_FAIL) {
        out = i;
        break;
      }
      link_push(heap, &fields, link_vt_i32(heap));
      link_push(heap, &fields, link_vt_ref(heap, GC_HEAP_TYPE, link_u32_of(i)));
      out = link_done(gc_comptype_struct(heap, fields));
      break;
    case TSPEC_RUNTIME:
      vs = link_runtime_vtypes(heap, &s->names, l);
      if (vs.kind == RESULT_FAIL) {
        out = vs;
        break;
      }
      v = link_runtime_valtype(heap, l, s->text);
      out = v.kind == RESULT_FAIL ? v : link_cfunc_one(heap, *(const Vec *)vs.value, v.value);
      break;
    case TSPEC_CLOS:
      link_push(heap, &fields, link_vt_i32(heap));
      link_push(heap, &fields, link_vt_ref(heap, GC_HEAP_FUNC, 0));
      link_push(heap, &fields, link_vt_ref(heap, GC_HEAP_EQ, 0));
      out = link_done(gc_comptype_struct(heap, fields));
      break;
    case TSPEC_FN: out = link_cfunc_eqs(heap, s->arity); break;
    case TSPEC_STRUCT: out = link_done(gc_comptype_struct(heap, link_eqs(heap, link_len(heap, &s->fields)))); break;
    case TSPEC_LEG:
      fields = link_eqs(heap, link_len(heap, &s->fields));
      out = link_done(gc_comptype_struct(heap, link_cons(heap, link_vt_ref(heap, GC_HEAP_I31, 0), &fields)));
      break;
    case TSPEC_PAP: out = link_done(gc_comptype_struct(heap, link_eqs(heap, nat_succ(heap->arena, s->count)))); break;
    case TSPEC_SIG:
      vs = link_valtypes_of(heap, &s->params, l);
      if (vs.kind == RESULT_FAIL) {
        out = vs;
        break;
      }
      v = link_valtype_of(heap, l, s->result);
      out = v.kind == RESULT_FAIL ? v : link_cfunc_one(heap, *(const Vec *)vs.value, v.value);
      break;
    case TSPEC_APPLY: out = link_cfunc_eqs(heap, s->arity); break;
    case TSPEC_ENTRY: out = link_cfunc_one(heap, vec_new(heap->arena), link_vt_i32(heap)); break;
  }
  return out;
}

/* comptypes(xs, l): Done{Vec * of GcComptype *} */
Result link_comptypes(Heap *heap, const Vec *xs, const Linked *l) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < xs->len; i++) {
    const Tspec *s = link_second(link_item_at(xs, i));
    if (s == NULL) continue;
    Result c = link_comptype_of(heap, s, l);
    if (c.kind == RESULT_FAIL) return c;
    link_push(heap, &out, c.value);
  }
  return link_done(link_vec_box(heap, out));
}

/* comptypes_groups(gs, l): Done{Vec * of Vec * of GcComptype *} */
Result link_comptypes_groups(Heap *heap, const Vec *gs, const Linked *l) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gs->len; i++) {
    const Vec *g = link_item_at(gs, i);
    if (g == NULL) continue;
    Result cs = link_comptypes(heap, g, l);
    if (cs.kind == RESULT_FAIL) return cs;
    link_push(heap, &out, cs.value);
  }
  return link_done(link_vec_box(heap, out));
}

Result link_comptype_groups(Heap *heap, const Linked *l) { return link_comptypes_groups(heap, &l->groups, l); }
