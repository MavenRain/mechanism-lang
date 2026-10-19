/* C port of rust/erase_typed.bend: typed erasure for `mech rust-out`. Each checked
 * declaration gets a class: runtime function, runtime family, proposition, type-valued
 * definition, absurd, or refused with a reason. The sort of a type is read from its shape (a
 * family level, a universe at the end of a telescope, the kind of a bound variable); each other
 * shape is refused.
 *
 * Bend 2 has no mutual recursion, so each Bend walker takes a list and a single value goes through
 * it as a list of one element (`one_er(ers(env, ctx, [t]))`). C has mutual recursion:
 * erase_typed_er is the single-value walker and erase_typed_ers maps it over a list. A Bend
 * `List<Knd>` context is an EtCtx cons list (rust.h), so a prepend is one node. Strings are byte
 * slices: each Bend Char test reads only an ASCII range, and no byte of a multi-byte UTF-8 code
 * point is in an ASCII range, so a walk over the bytes gives the same text as the Bend walk over
 * the code points. A failed allocation sets heap->oom; the helpers then return placeholders and
 * the caller checks heap->oom. */
#include "rust.h"
#include "surface.h"

static const Str erase_typed_str_placeholder = {0};
static const EtEr erase_typed_er_placeholder = {ET_ENO, NULL, {0}};

/* Appends `item` to `acc`; a failed push sets heap->oom. */
static void erase_typed_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

static void erase_typed_put(Heap *heap, StrBuf *buf, unsigned char c) {
  if (!strbuf_push_byte(buf, (char)c)) heap->oom = true;
}

static void erase_typed_puts(Heap *heap, StrBuf *buf, Str s) {
  if (!strbuf_push(buf, s)) heap->oom = true;
}

/* The concatenation of `n` parts in one buffer. */
static Str erase_typed_cat(Heap *heap, const Str *parts, size_t n) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < n; i++) erase_typed_puts(heap, &buf, parts[i]);
  return strbuf_str(&buf);
}

static Str erase_typed_cat2(Heap *heap, Str a, Str b) {
  const Str parts[2] = {a, b};
  return erase_typed_cat(heap, parts, 2);
}

/* ---- Names (D7) ---- */

static bool erase_typed_is_upper(unsigned char c) { return c >= 65 && c <= 90; }

bool erase_typed_is_lower(unsigned char c) { return c >= 97 && c <= 122; }

static unsigned char erase_typed_lower(unsigned char c) {
  if (erase_typed_is_upper(c)) return (unsigned char)(c + 32);
  return c;
}

static unsigned char erase_typed_upper(unsigned char c) {
  if (erase_typed_is_lower(c)) return (unsigned char)(c - 32);
  return c;
}

/* Pushes the snake form of `s` (each upper-case letter becomes `_` and the lower-case letter). */
static void erase_typed_snake_rest(Heap *heap, StrBuf *out, Str s) {
  for (size_t i = 0; i < s.len; i++) {
    unsigned char c = (unsigned char)s.data[i];
    if (erase_typed_is_upper(c)) {
      erase_typed_put(heap, out, '_');
      erase_typed_put(heap, out, erase_typed_lower(c));
    } else {
      erase_typed_put(heap, out, c);
    }
  }
}

static Str erase_typed_snake(Heap *heap, Str s) {
  if (s.len == 0) return str_empty();
  StrBuf out = strbuf_new(heap->arena);
  erase_typed_put(heap, &out, erase_typed_lower((unsigned char)s.data[0]));
  erase_typed_snake_rest(heap, &out, str_n(s.data + 1, s.len - 1));
  return strbuf_str(&out);
}

/* `s` with its first byte mapped by `f` (the first Char of the Bend text). */
static Str erase_typed_map_first(Heap *heap, Str s, unsigned char (*f)(unsigned char)) {
  if (s.len == 0) return str_empty();
  StrBuf out = strbuf_new(heap->arena);
  erase_typed_put(heap, &out, f((unsigned char)s.data[0]));
  erase_typed_puts(heap, &out, str_n(s.data + 1, s.len - 1));
  return strbuf_str(&out);
}

Str erase_typed_upper_first(Heap *heap, Str s) { return erase_typed_map_first(heap, s, erase_typed_upper); }

static const char *const erase_typed_keywords[] = {
    "as",    "async",    "await",  "box",     "break",  "const",  "continue", "crate", "do",     "dyn",
    "else",  "enum",     "extern", "false",   "final",  "fn",     "for",      "if",    "impl",   "in",
    "let",   "loop",     "macro",  "match",   "mod",    "move",   "mut",      "override", "priv", "pub",
    "ref",   "return",   "self",   "static",  "struct", "super",  "trait",    "true",  "try",    "type",
    "typeof", "unsafe",  "unsized", "use",    "virtual", "where", "while",    "yield"};

/* has(keywords(), name): the keyword list is a static table, so no list is built per call. */
static bool erase_typed_is_keyword(Str name) {
  for (size_t i = 0; i < sizeof(erase_typed_keywords) / sizeof(erase_typed_keywords[0]); i++) {
    if (str_eq(str_lit(erase_typed_keywords[i]), name)) return true;
  }
  return false;
}

bool erase_typed_has(const Vec *xs, Str name) {
  for (size_t i = 0; i < xs->len; i++) {
    const Str *x = xs->items[i];
    if (x != NULL && str_eq(*x, name)) return true;
  }
  return false;
}

Str erase_typed_rust_name(Heap *heap, Str s) {
  if (erase_typed_is_keyword(s)) return erase_typed_cat2(heap, s, str_lit("_"));
  return s;
}

Str erase_typed_fn_name(Heap *heap, Str s) { return erase_typed_rust_name(heap, erase_typed_snake(heap, s)); }

/* ---- The inverse of the name map. The importer reads a Rust name with it. The emitter refuses a
 * mech name that does not come back from its Rust name. ---- */

static bool erase_typed_is_under(unsigned char c) { return c == 95; }

/* camel_step(c, r) on the text r = out[pos, end): a `_` before a lower-case letter goes and the
 * letter becomes upper case, else `c` goes in front. Returns the new start. `r` is the camelCase
 * form of the text after `c`; its first byte is lower case exactly when the first byte after `c`
 * is, so the test of a `_` before a lower-case letter can use it. */
static size_t erase_typed_camel_step(unsigned char c, char *out, size_t pos, size_t end) {
  if (pos < end && erase_typed_is_under(c) && erase_typed_is_lower((unsigned char)out[pos])) {
    out[pos] = (char)erase_typed_upper((unsigned char)out[pos]);
    return pos;
  }
  if (pos == 0) return pos;
  out[pos - 1] = (char)c;
  return pos - 1;
}

/* The Bend right fold `camel_step(c, camel_rest(rest))`, run from the last byte to the first into
 * the tail of one buffer of the input length (the output is never longer than the input). */
static Str erase_typed_camel_rest(Heap *heap, Str s) {
  if (s.len == 0) return str_empty();
  char *out = heap_alloc(heap, s.len);
  if (out == NULL) return erase_typed_str_placeholder;
  size_t pos = s.len;
  for (size_t i = s.len; i > 0; i--) pos = erase_typed_camel_step((unsigned char)s.data[i - 1], out, pos, s.len);
  return str_n(out + pos, s.len - pos);
}

/* `snake_case` -> `camelCase`: a `_` before a lower-case letter goes, and the letter becomes upper
 * case. The first character stays. */
static Str erase_typed_camel(Heap *heap, Str s) {
  if (s.len == 0) return str_empty();
  return erase_typed_cat2(heap, str_n(s.data, 1), erase_typed_camel_rest(heap, str_n(s.data + 1, s.len - 1)));
}

Str erase_typed_lower_first(Heap *heap, Str s) { return erase_typed_map_first(heap, s, erase_typed_lower); }

/* The inverse of `rust_name`: a keyword with `_` at its end loses the `_`. Bend computes the base
 * as reverse(drop_head(reverse(s))) (s without its last code point) and tests base ++ "_" == s;
 * that holds exactly when the last byte is `_` (a one-byte code point), so base = s without that
 * byte. Bend drop_head has no other caller, so it has no C counterpart. */
static Str erase_typed_unkw(Str s) {
  if (s.len == 0 || s.data[s.len - 1] != '_') return s;
  Str base = str_n(s.data, s.len - 1);
  if (erase_typed_is_keyword(base)) return base;
  return s;
}

/* The inverse of `fn_name`. */
Str erase_typed_fn_back(Heap *heap, Str rust) { return erase_typed_camel(heap, erase_typed_unkw(rust)); }

static Str erase_typed_back_text(Heap *heap, Str rust, Str back) {
  const Str parts[5] = {str_lit("a name that does not come back from the Rust name `"), rust,
                        str_lit("`: the importer gives `"), back, str_lit("` (D7)")};
  return erase_typed_cat(heap, parts, 5);
}

Str erase_typed_first(const Vec *xs) {
  VecGet g = vec_get(xs, 0);
  if (g.status == VEC_GET_OK && g.value != NULL) return *(const Str *)g.value;
  return str_lit("");
}

static bool erase_typed_no_names(const Vec *xs) { return xs->len == 0; }

/* The Bend walk stops at the shorter list and asks whether the other one is empty too. */
static bool erase_typed_same_count(const Vec *xs, const Vec *ys) {
  if (xs->len == 0) return erase_typed_no_names(ys);
  return xs->len == ys->len;
}

/* ---- Kinds and sorts ---- */

EtEnv erase_typed_env(Globals globals, Vec copies) {
  EtEnv env;
  env.globals = globals;
  env.copies = copies;
  return env;
}

Globals erase_typed_globals_of(const EtEnv *env) { return env->globals; }

static const Vec *erase_typed_copies_of(const EtEnv *env) { return &env->copies; }

static bool erase_typed_is_pi(const Term *t) { return t->kind == TERM_RAN && t->shape.kind == SHAPE_PI; }

static EtEnd erase_typed_end_of(const Term *t) {
  const Term *u = t;
  while (erase_typed_is_pi(u) && u->body != NULL) u = u->body;
  if (u->kind != TERM_UNIV) return ET_END_NONE;
  if (level_always_zero(u->level)) return ET_END_PROP;
  return ET_END_TYPE;
}

static EtKnd erase_typed_knd(EtKndKind kind, Str name, Vec bds) {
  EtKnd k;
  k.kind = kind;
  k.name = name;
  k.bds = bds;
  return k;
}

static EtKnd erase_typed_kind_of(Heap *heap, EtEnd e, Str name) {
  switch (e) {
    case ET_END_PROP:
      return erase_typed_knd(ET_KPROP, str_empty(), vec_new(heap->arena));
    case ET_END_TYPE:
      return erase_typed_knd(ET_KGEN, name, vec_new(heap->arena));
    case ET_END_NONE:
      return erase_typed_knd(ET_KVAL, str_empty(), vec_new(heap->arena));
  }
  return erase_typed_knd(ET_KVAL, str_empty(), vec_new(heap->arena));
}

/* `k <> ctx`. A failed allocation sets heap->oom and returns `ctx`. */
static const EtCtx *erase_typed_cons(Heap *heap, EtKnd k, const EtCtx *ctx) {
  EtCtx *node = heap_alloc(heap, sizeof(EtCtx));
  if (node == NULL) return ctx;
  node->head = k;
  node->rest = ctx;
  return node;
}

static EtKnd erase_typed_kind_at(Heap *heap, const EtCtx *ctx, Nat index) {
  EtKnd val = erase_typed_knd(ET_KVAL, str_empty(), vec_new(heap->arena));
  NatToU64 n = nat_to_u64(index);
  switch (n.status) {
    case NAT_TO_U64_OVERFLOW:
      return val;
    case NAT_TO_U64_OK:
      break;
  }
  const EtCtx *at = ctx;
  for (uint64_t i = 0; i < n.value && at != NULL; i++) at = at->rest;
  if (at == NULL) return val;
  return at->head;
}

static bool erase_typed_is_zero(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO:
      return true;
    case QUANTITY_ONE:
      return false;
    case QUANTITY_MANY:
      return false;
  }
  return false;
}

static bool erase_typed_is_one(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO:
      return false;
    case QUANTITY_ONE:
      return true;
    case QUANTITY_MANY:
      return false;
  }
  return false;
}

/* ---- Type erasure ---- */

static EtEr erase_typed_edata(const RirTy *t) {
  EtEr e = {ET_EDATA, t, {0}};
  return e;
}

static EtEr erase_typed_eprop(void) {
  EtEr e = {ET_EPROP, NULL, {0}};
  return e;
}

static EtEr erase_typed_eno(Str why) {
  EtEr e = {ET_ENO, NULL, why};
  return e;
}

static const EtEr *erase_typed_er_box(Heap *heap, EtEr e) {
  EtEr *box = heap_alloc(heap, sizeof(EtEr));
  if (box == NULL) return &erase_typed_er_placeholder;
  *box = e;
  return box;
}

/* one_er: the head of `xs` (EtEr pointers), or ENo "no type". */
static EtEr erase_typed_one_er(const Vec *xs) {
  VecGet g = vec_get(xs, 0);
  if (g.status == VEC_GET_OK && g.value != NULL) return *(const EtEr *)g.value;
  return erase_typed_eno(str_lit("no type"));
}

/* The bodies of the legs (Leg pointers), as Term pointers. */
static Vec erase_typed_leg_bodies(Heap *heap, const Vec *legs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < legs->len; i++) {
    const Leg *leg = legs->items[i];
    if (leg != NULL) erase_typed_push(heap, &out, leg->body);
  }
  return out;
}

/* Result<String, List<R.Ty>>. While keep_args folds from the right, `tys` holds the kept types
 * last first; keep_args reverses it at the end. */
typedef struct {
  bool ok;
  Str why;  /* !ok */
  Vec tys;  /* ok: RirTy pointers */
} EtTys;

static EtTys erase_typed_fail(Heap *heap, Str why) {
  EtTys r = {false, why, vec_new(heap->arena)};
  return r;
}

static EtTys erase_typed_push_ty(Heap *heap, const RirTy *t, EtTys r) {
  if (!r.ok) return r;
  erase_typed_push(heap, &r.tys, t);
  return r;
}

/* The arguments of a family that stay: one for each `Type`-kinded parameter. */
static EtTys erase_typed_keep_arg(Heap *heap, EtEnd e, EtEr a, EtTys tail) {
  switch (e) {
    case ET_END_TYPE:
      switch (a.kind) {
        case ET_EDATA:
          return erase_typed_push_ty(heap, a.ty, tail);
        case ET_EPROP:
          return erase_typed_fail(heap, str_lit("a proposition as a type argument"));
        case ET_ENO:
          return erase_typed_fail(heap, a.why);
      }
      return tail;
    case ET_END_PROP:
      return tail;
    case ET_END_NONE:
      return tail;
  }
  return tail;
}

/* The Bend right fold keep_arg(end_of(p_i), a_i, keep_args(rest)): it stops at the end of the
 * parameters (Done []) or at the first parameter without an argument (Fail), then keep_arg runs
 * from that point back to the first parameter, so a failure on the left wins. */
static EtTys erase_typed_keep_args(Heap *heap, const Vec *params, const Vec *args) {
  size_t stop = params->len;
  EtTys r = {true, {0}, vec_new(heap->arena)};
  if (args->len < params->len) {
    stop = args->len;
    r = erase_typed_fail(heap, str_lit("a family with too few arguments"));
  }
  for (size_t i = stop; i > 0; i--) {
    const TeleBinder *p = params->items[i - 1];
    const EtEr *a = args->items[i - 1];
    if (p != NULL && a != NULL) r = erase_typed_keep_arg(heap, erase_typed_end_of(p->ty), *a, r);
  }
  if (r.ok) r.tys = heap_vec_reverse(heap, &r.tys);
  return r;
}

static EtEr erase_typed_con_ty(Heap *heap, Str name, EtTys r) {
  if (r.ok) return erase_typed_edata(rir_tcon(heap, name, r.tys));
  return erase_typed_eno(r.why);
}

/* `MechBool` is `bool` by name (prelude contract). */
static EtEr erase_typed_family_ty(Heap *heap, Str name, MaybeFamily fam, const Vec *args) {
  if (!fam.some) return erase_typed_eno(erase_typed_cat2(heap, str_lit("an unknown family: "), name));
  if (level_always_zero(fam.value.level)) return erase_typed_eprop();
  if (str_eq(name, str_lit("MechBool"))) return erase_typed_edata(rir_tbool(heap));
  return erase_typed_con_ty(heap, name, erase_typed_keep_args(heap, &fam.value.params, args));
}

static EtEr erase_typed_global_end(Heap *heap, Str name, EtEnd e) {
  switch (e) {
    case ET_END_PROP:
      return erase_typed_eprop();
    case ET_END_TYPE:
      if (str_eq(name, str_lit("Nat"))) return erase_typed_edata(rir_tcon(heap, str_lit("Nat"), vec_new(heap->arena)));
      return erase_typed_eno(erase_typed_cat2(heap, str_lit("a type-valued definition in a runtime type (D3): "), name));
    case ET_END_NONE:
      return erase_typed_eno(erase_typed_cat2(heap, str_lit("a value in a type: "), name));
  }
  return erase_typed_eno(str_empty());
}

static EtEr erase_typed_global_ty(Heap *heap, Str name, MaybeEntry entry) {
  if (!entry.some) return erase_typed_eno(erase_typed_cat2(heap, str_lit("an unknown name in a type: "), name));
  return erase_typed_global_end(heap, name, erase_typed_end_of(global_entry_ty(entry.value)));
}

static EtEr erase_typed_var_ty(Heap *heap, EtKnd k) {
  switch (k.kind) {
    case ET_KGEN:
      return erase_typed_edata(rir_tcon(heap, k.name, vec_new(heap->arena)));
    case ET_KPROP:
      return erase_typed_eprop();
    case ET_KVAL:
      return erase_typed_eno(str_lit("a value in a type"));
    case ET_KLAM:
      return erase_typed_eno(str_lit("a value in a type"));
  }
  return erase_typed_eno(str_empty());
}

static const RirTy *erase_typed_own_or_ref(Heap *heap, bool by_value, const RirTy *t) {
  if (by_value) return t;
  return rir_tref(heap, t);
}

/* Ownership of a binder (D5): a function is `&impl Fn`, a Copy value or a quantity-1 value is
 * owned, each other value is a borrow. */
static const RirTy *erase_typed_mode(Heap *heap, const Vec *copies, Quantity q, const RirTy *t) {
  switch (t->kind) {
    case RIR_TBOOL:
      return t;
    case RIR_TCON:
      return erase_typed_own_or_ref(heap, erase_typed_is_one(q) || erase_typed_has(copies, t->name), t);
    case RIR_TBOX:
      return t;
    case RIR_TREF:
      return t;
    case RIR_TFN:
      return rir_tref(heap, t);
  }
  return t;
}

static EtEr erase_typed_dropped(const RirTy *ret) {
  if (ret->kind == RIR_TFN) return erase_typed_edata(ret);
  return erase_typed_eno(str_lit("a function type with erased arguments only"));
}

static EtEr erase_typed_kept(Heap *heap, const RirTy *p, const RirTy *ret) {
  Vec args = vec_new(heap->arena);
  erase_typed_push(heap, &args, p);
  if (ret->kind != RIR_TFN) return erase_typed_edata(rir_tfn(heap, args, ret));
  for (size_t i = 0; i < ret->args.len; i++) erase_typed_push(heap, &args, ret->args.items[i]);
  return erase_typed_edata(rir_tfn(heap, args, ret->ret));
}

/* The erasure of `(q x : A) -> B` as the type of a value. */
static EtEr erase_typed_fn_er(Heap *heap, const Vec *copies, Quantity q, EtEr b, EtEnd e, EtEr a) {
  switch (b.kind) {
    case ET_EPROP:
      return erase_typed_eprop();
    case ET_ENO:
      return erase_typed_eno(b.why);
    case ET_EDATA:
      switch (e) {
        case ET_END_TYPE:
          return erase_typed_eno(str_lit("a polymorphic function type"));
        case ET_END_PROP:
          return erase_typed_dropped(b.ty);
        case ET_END_NONE:
          switch (a.kind) {
            case ET_EPROP:
              return erase_typed_dropped(b.ty);
            case ET_ENO:
              if (erase_typed_is_zero(q)) return erase_typed_dropped(b.ty);
              return erase_typed_eno(a.why);
            case ET_EDATA:
              if (erase_typed_is_zero(q)) return erase_typed_dropped(b.ty);
              return erase_typed_kept(heap, erase_typed_mode(heap, copies, q, a.ty), b.ty);
          }
          return erase_typed_eno(str_empty());
      }
      return erase_typed_eno(str_empty());
  }
  return erase_typed_eno(str_empty());
}

static Vec erase_typed_ers(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Vec *ts);

/* The single-value walker of `ers`: Bend `ers(env, ctx, t <> rest)` = er(t) <> ers(rest), and the
 * Out and Ann cases replace `t` by its head or body, so they loop here. */
static EtEr erase_typed_er(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Term *t) {
  const Term *u = t;
  while (u != NULL &&
         (u->kind == TERM_ANN || (u->kind == TERM_OUT && (u->shape.kind == SHAPE_PI || u->shape.kind == SHAPE_ARG)))) {
    if (u->kind == TERM_ANN) {
      u = u->body;
    } else {
      u = u->head;
    }
  }
  EtEr other = erase_typed_eno(str_lit("a type shape outside the M0 fragment"));
  if (u == NULL) return other;
  switch (u->kind) {
    case TERM_VAR:
      return erase_typed_var_ty(heap, erase_typed_kind_at(heap, ctx, u->index));
    case TERM_LAN:
      if (u->shape.kind == SHAPE_MU && u->body != NULL && u->body->kind == TERM_SEC) {
        Vec bodies = erase_typed_leg_bodies(heap, &u->body->legs);
        Vec args = erase_typed_ers(heap, env, ctx, &bodies);
        return erase_typed_family_ty(heap, u->shape.name, global_find_family(u->shape.name, env->globals), &args);
      }
      return other;
    case TERM_RAN:
      if (u->shape.kind == SHAPE_PI && u->body != NULL && u->shape.domain != NULL) {
        const Term *dom = u->shape.domain;
        EtEnd e = erase_typed_end_of(dom);
        const EtCtx *inner = erase_typed_cons(heap, erase_typed_kind_of(heap, e, u->shape.name), ctx);
        return erase_typed_fn_er(heap, erase_typed_copies_of(env), u->shape.quantity, erase_typed_er(heap, env, inner, u->body),
                                 e, erase_typed_er(heap, env, ctx, dom));
      }
      return other;
    case TERM_GLOBAL:
      return erase_typed_global_ty(heap, u->name, global_find(u->name, env->globals));
    case TERM_UNIV:
      return other;
    case TERM_IN:
      return other;
    case TERM_ELIM:
      return other;
    case TERM_SEC:
      return other;
    case TERM_OUT:
      return other;
    case TERM_LET:
      return other;
    case TERM_ANN:
      return other;
    case TERM_LIT:
      return other;
    case TERM_AUTO:
      return other;
  }
  return other;
}

static Vec erase_typed_ers(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const Term *t = ts->items[i];
    erase_typed_push(heap, &out, erase_typed_er_box(heap, erase_typed_er(heap, env, ctx, t)));
  }
  return out;
}

/* The erasure of the result type of a telescope. */
static EtEr erase_typed_result_of(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Term *ty) {
  const EtCtx *at = ctx;
  const Term *u = ty;
  while (erase_typed_is_pi(u) && u->body != NULL && u->shape.domain != NULL) {
    const Term *dom = u->shape.domain;
    at = erase_typed_cons(heap, erase_typed_kind_of(heap, erase_typed_end_of(dom), u->shape.name), at);
    u = u->body;
  }
  return erase_typed_er(heap, env, at, u);
}

/* ---- Binders (part b, Bend line 427 on) ---- */

/* Bd: the erasure of one binder. BGen = a generic, BDrop = no runtime argument, BParam = a
 * runtime parameter of type `ty`, BNo = a refusal with the reason `why`. */
typedef enum { ET_BGEN, ET_BDROP, ET_BPARAM, ET_BNO } EtBdKind;
struct EtBd {
  EtBdKind kind;
  const RirTy *ty; /* ET_BPARAM */
  Str why;         /* ET_BNO */
};

static const EtBd erase_typed_bd_placeholder = {ET_BDROP, NULL, {0}};

static EtBd erase_typed_bd(EtBdKind kind, const RirTy *ty, Str why) {
  EtBd b;
  b.kind = kind;
  b.ty = ty;
  b.why = why;
  return b;
}

static const EtBd *erase_typed_bd_box(Heap *heap, EtBd b) {
  EtBd *box = heap_alloc(heap, sizeof(EtBd));
  if (box == NULL) return &erase_typed_bd_placeholder;
  *box = b;
  return box;
}

static const Str *erase_typed_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) return &erase_typed_str_placeholder;
  *box = s;
  return box;
}

static EtBd erase_typed_binder_er(Quantity q, EtEnd e, EtEr a) {
  Str none = str_lit("");
  switch (e) {
    case ET_END_TYPE:
      if (erase_typed_is_zero(q)) return erase_typed_bd(ET_BGEN, NULL, none);
      return erase_typed_bd(ET_BNO, NULL, str_lit("a type argument with a runtime quantity"));
    case ET_END_PROP:
      return erase_typed_bd(ET_BDROP, NULL, none);
    case ET_END_NONE:
      switch (a.kind) {
        case ET_EPROP:
          return erase_typed_bd(ET_BDROP, NULL, none);
        case ET_ENO:
          if (erase_typed_is_zero(q)) return erase_typed_bd(ET_BDROP, NULL, none);
          return erase_typed_bd(ET_BNO, NULL, a.why);
        case ET_EDATA:
          if (erase_typed_is_zero(q)) return erase_typed_bd(ET_BDROP, NULL, none);
          return erase_typed_bd(ET_BPARAM, a.ty, none);
      }
      break;
  }
  return erase_typed_bd(ET_BDROP, NULL, none);
}

/* The binders of a function type, in step, as fn_er decides them (EtBd pointers): a binder
 * keeps its argument only as BParam. */
static Vec erase_typed_tele_bds(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Term *ty) {
  Vec out = vec_new(heap->arena);
  const EtCtx *at = ctx;
  const Term *u = ty;
  while (u != NULL && erase_typed_is_pi(u) && u->body != NULL && u->shape.domain != NULL) {
    const Term *dom = u->shape.domain;
    EtEnd e = erase_typed_end_of(dom);
    EtBd bd = erase_typed_binder_er(u->shape.quantity, e, erase_typed_er(heap, env, at, dom));
    erase_typed_push(heap, &out, erase_typed_bd_box(heap, bd));
    at = erase_typed_cons(heap, erase_typed_kind_of(heap, e, u->shape.name), at);
    u = u->body;
  }
  return out;
}

/* The kind of a lambda binder. A value keeps the binders of its type: a call of it with SArg
 * frames takes the decision for each argument from there. */
static EtKnd erase_typed_bind_kind(Heap *heap, const EtEnv *env, const EtCtx *ks, EtEnd e, Str x,
                                   const Term *dom) {
  switch (e) {
    case ET_END_PROP:
      return erase_typed_knd(ET_KPROP, str_lit(""), vec_new(heap->arena));
    case ET_END_TYPE:
      return erase_typed_knd(ET_KGEN, x, vec_new(heap->arena));
    case ET_END_NONE:
      return erase_typed_knd(ET_KLAM, str_lit(""), erase_typed_tele_bds(heap, env, ks, dom));
  }
  return erase_typed_knd(ET_KPROP, str_lit(""), vec_new(heap->arena));
}

/* The name of the lambda binder at the head of a body, and the body below it. */
static Str erase_typed_peel_name(const Term *body, Str fallback) {
  const Term *u = body;
  while (u != NULL && u->kind == TERM_ANN) u = u->body;
  if (u != NULL && u->kind == TERM_SEC && u->shape.kind == SHAPE_PI) return u->shape.name;
  return fallback;
}

static const Term *erase_typed_peel_body(const Term *body) {
  const Term *u = body;
  while (u != NULL && u->kind == TERM_ANN) u = u->body;
  if (u != NULL && u->kind == TERM_SEC && u->shape.kind == SHAPE_PI) {
    VecGet g = vec_get(&u->legs, 0);
    if (g.status == VEC_GET_OK && g.value != NULL) return ((const Leg *)g.value)->body;
  }
  return u;
}

static bool erase_typed_mentions(Str g, const Vec *ts);

static bool erase_typed_mentions_ty(Str g, const RirTy *t) {
  if (t == NULL) return false;
  switch (t->kind) {
    case RIR_TBOOL:
      return false;
    case RIR_TCON:
      return str_eq(t->name, g) || erase_typed_mentions(g, &t->args);
    case RIR_TBOX:
    case RIR_TREF:
      return erase_typed_mentions_ty(g, t->inner);
    case RIR_TFN:
      return erase_typed_mentions_ty(g, t->ret) || erase_typed_mentions(g, &t->args);
  }
  return false;
}

/* Whether any type of `ts` (RirTy pointers) names `g`. */
static bool erase_typed_mentions(Str g, const Vec *ts) {
  for (size_t i = 0; i < ts->len; i++) {
    if (erase_typed_mentions_ty(g, ts->items[i])) return true;
  }
  return false;
}

/* R1: a generic stays only if the erased types mention it (Str pointers, order kept). */
static Vec erase_typed_used(Heap *heap, const Vec *gens, const Vec *tys) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gens->len; i++) {
    const Str *g = gens->items[i];
    if (g != NULL && erase_typed_mentions(*g, tys)) erase_typed_push(heap, &out, g);
  }
  return out;
}

/* The types of the parameters (RirParam pointers in, RirTy pointers out). */
Vec erase_typed_param_tys(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = ps->items[i];
    if (p != NULL) erase_typed_push(heap, &out, p->ty);
  }
  return out;
}

/* Head: HFn{generics, params, ret}, HProp{}, HNo{why}. */
typedef enum { ET_HFN, ET_HPROP, ET_HNO } EtHeadKind;
typedef struct {
  EtHeadKind kind;
  Vec generics;     /* ET_HFN: Str pointers */
  Vec params;       /* ET_HFN: RirParam pointers */
  const RirTy *ret; /* ET_HFN */
  Str why;          /* ET_HNO */
} EtHead;

/* Acc{gens, params, bad}: C keeps each list in binder order. Bend prepends, so its lists run
 * last binder first, and fn_head and head_of reverse them; the C lists need no reverse. */
typedef struct {
  Vec gens;   /* Str pointers */
  Vec params; /* RirParam pointers */
  Vec bad;    /* Str pointers: the refusal reasons of the binders */
} EtAcc;

static EtHead erase_typed_head(Heap *heap, EtHeadKind kind, Str why) {
  EtHead h;
  h.kind = kind;
  h.generics = vec_new(heap->arena);
  h.params = vec_new(heap->arena);
  h.ret = NULL;
  h.why = why;
  return h;
}

static EtHead erase_typed_fn_head(Heap *heap, const Vec *gens, const Vec *params, EtEr e) {
  switch (e.kind) {
    case ET_EPROP:
      return erase_typed_head(heap, ET_HPROP, str_lit(""));
    case ET_ENO:
      return erase_typed_head(heap, ET_HNO, e.why);
    case ET_EDATA: {
      EtHead h = erase_typed_head(heap, ET_HFN, str_lit(""));
      Vec tys = vec_new(heap->arena);
      Vec pts = erase_typed_param_tys(heap, params);
      erase_typed_push(heap, &tys, e.ty);
      for (size_t i = 0; i < pts.len; i++) erase_typed_push(heap, &tys, pts.items[i]);
      h.generics = erase_typed_used(heap, gens, &tys);
      h.params = *params;
      h.ret = e.ty;
      return h;
    }
  }
  return erase_typed_head(heap, ET_HNO, str_lit(""));
}

/* With a refused binder, the head is HNo with the reason of the first one. */
static EtHead erase_typed_head_of(Heap *heap, const EtAcc *acc, EtEr e) {
  if (acc->bad.len > 0) return erase_typed_head(heap, ET_HNO, erase_typed_first(&acc->bad));
  return erase_typed_fn_head(heap, &acc->gens, &acc->params, e);
}

static void erase_typed_tele_step(Heap *heap, const Vec *copies, Quantity q, Str x, Str name, EtAcc *acc,
                                  EtBd bd) {
  switch (bd.kind) {
    case ET_BGEN:
      erase_typed_push(heap, &acc->gens, erase_typed_str_box(heap, x));
      return;
    case ET_BDROP:
      return;
    case ET_BPARAM:
      erase_typed_push(heap, &acc->params,
                       rir_param(heap, erase_typed_fn_name(heap, name), erase_typed_mode(heap, copies, q, bd.ty)));
      return;
    case ET_BNO: {
      const Str parts[4] = {bd.why, str_lit(" (binder "), x, str_lit(")")};
      erase_typed_push(heap, &acc->bad, erase_typed_str_box(heap, erase_typed_cat(heap, parts, 4)));
      return;
    }
  }
}

/* A Pi telescope and the lambdas of the body, in step. Parameter names come from the lambdas,
 * generic names from the telescope. */
static EtHead erase_typed_tele(Heap *heap, const EtEnv *env, const EtCtx *ctx, const Term *ty, const Term *body) {
  EtAcc acc;
  const EtCtx *at = ctx;
  const Term *u = ty;
  const Term *b = body;
  acc.gens = vec_new(heap->arena);
  acc.params = vec_new(heap->arena);
  acc.bad = vec_new(heap->arena);
  while (u != NULL && erase_typed_is_pi(u) && u->body != NULL && u->shape.domain != NULL) {
    const Term *dom = u->shape.domain;
    Quantity q = u->shape.quantity;
    Str x = u->shape.name;
    EtEnd e = erase_typed_end_of(dom);
    EtBd bd = erase_typed_binder_er(q, e, erase_typed_er(heap, env, at, dom));
    erase_typed_tele_step(heap, erase_typed_copies_of(env), q, x, erase_typed_peel_name(b, x), &acc, bd);
    at = erase_typed_cons(heap, erase_typed_kind_of(heap, e, x), at);
    b = erase_typed_peel_body(b);
    u = u->body;
  }
  return erase_typed_head_of(heap, &acc, erase_typed_er(heap, env, at, u));
}

/* ---- Families ---- */

static const RirTy *erase_typed_box_if(Heap *heap, bool self, const RirTy *t) {
  if (self) return rir_tbox(heap, t);
  return t;
}

/* A field that holds its own family goes in a `Box`. */
static const RirTy *erase_typed_boxed(Heap *heap, Str own, const RirTy *t) {
  if (t == NULL) return t;
  switch (t->kind) {
    case RIR_TCON:
      return erase_typed_box_if(heap, str_eq(t->name, own), t);
    case RIR_TBOOL:
    case RIR_TBOX:
    case RIR_TREF:
    case RIR_TFN:
      return t;
  }
  return t;
}

static EtTys erase_typed_field_step(Heap *heap, Str own, EtBd bd, EtTys tail) {
  switch (bd.kind) {
    case ET_BGEN:
      return erase_typed_fail(heap, str_lit("a type-valued constructor field"));
    case ET_BDROP:
      return tail;
    case ET_BPARAM:
      return erase_typed_push_ty(heap, erase_typed_boxed(heap, own, bd.ty), tail);
    case ET_BNO:
      return erase_typed_fail(heap, bd.why);
  }
  return tail;
}

/* fields: a right fold over the constructor arguments (TeleBinder pointers). The binders are
 * decided left to right (each sees the kinds of the ones before it), then folded from the last
 * one back, so the leftmost refusal wins; push_ty appends, so the kept types are reversed at
 * the end. */
static EtTys erase_typed_fields(Heap *heap, const EtEnv *env, const EtCtx *ctx, Str own, const Vec *args) {
  Vec bds = vec_new(heap->arena);
  const EtCtx *at = ctx;
  EtTys r = {true, {0}, vec_new(heap->arena)};
  for (size_t i = 0; i < args->len; i++) {
    const TeleBinder *b = args->items[i];
    if (b != NULL && b->ty != NULL) {
      EtEnd e = erase_typed_end_of(b->ty);
      EtBd bd = erase_typed_binder_er(b->quantity, e, erase_typed_er(heap, env, at, b->ty));
      erase_typed_push(heap, &bds, erase_typed_bd_box(heap, bd));
      at = erase_typed_cons(heap, erase_typed_kind_of(heap, e, b->name), at);
    }
  }
  for (size_t i = bds.len; i > 0; i--) {
    const EtBd *bd = bds.items[i - 1];
    r = erase_typed_field_step(heap, own, *bd, r);
  }
  if (r.ok) r.tys = heap_vec_reverse(heap, &r.tys);
  return r;
}

/* Result<String, List<R.Variant>>: vars are appended (reversed at the end of variants). */
typedef struct {
  bool ok;
  Str why;  /* !ok */
  Vec vars; /* ok: RirVariant pointers */
} EtVars;

static EtVars erase_typed_vars_fail(Heap *heap, Str why) {
  EtVars r = {false, why, vec_new(heap->arena)};
  return r;
}

static EtVars erase_typed_push_variant(Heap *heap, Str name, EtTys fs, EtVars vs) {
  if (!fs.ok) return erase_typed_vars_fail(heap, fs.why);
  if (!vs.ok) return vs;
  erase_typed_push(heap, &vs.vars, rir_variant(heap, name, fs.tys));
  return vs;
}

/* The fields of a constructor, if its name comes back from the variant name (D7). */
static EtTys erase_typed_ctor_kept(Heap *heap, Str name, EtTys fs) {
  Str rust = erase_typed_upper_first(heap, name);
  Str back = erase_typed_lower_first(heap, rust);
  if (str_eq(back, name)) return fs;
  return erase_typed_fail(heap, erase_typed_back_text(heap, rust, back));
}

/* variants: a right fold over the constructors (Ctor pointers), from the last one back. */
static EtVars erase_typed_variants(Heap *heap, const EtEnv *env, const EtCtx *ctx, Str own, const Vec *ctors) {
  EtVars r = {true, {0}, vec_new(heap->arena)};
  for (size_t i = ctors->len; i > 0; i--) {
    const Ctor *c = ctors->items[i - 1];
    if (c != NULL) {
      EtTys fs = erase_typed_ctor_kept(heap, c->name, erase_typed_fields(heap, env, ctx, own, &c->args));
      r = erase_typed_push_variant(heap, erase_typed_upper_first(heap, c->name), fs, r);
    }
  }
  if (r.ok) r.vars = heap_vec_reverse(heap, &r.vars);
  return r;
}

static const EtCtx *erase_typed_param_kinds(Heap *heap, const Vec *params, const EtCtx *ctx) {
  const EtCtx *at = ctx;
  for (size_t i = 0; i < params->len; i++) {
    const TeleBinder *b = params->items[i];
    if (b != NULL && b->ty != NULL) at = erase_typed_cons(heap, erase_typed_kind_of(heap, erase_typed_end_of(b->ty), b->name), at);
  }
  return at;
}

/* `ks` has the innermost binder first, so the names are collected and reversed: the result is
 * in source order (Str pointers). */
static Vec erase_typed_gen_names(Heap *heap, const EtCtx *ks) {
  Vec out = vec_new(heap->arena);
  for (const EtCtx *k = ks; k != NULL; k = k->rest) {
    switch (k->head.kind) {
      case ET_KGEN:
        erase_typed_push(heap, &out, &k->head.name);
        break;
      case ET_KPROP:
      case ET_KVAL:
      case ET_KLAM:
        break;
    }
  }
  return heap_vec_reverse(heap, &out);
}

static bool erase_typed_all_unit(const Vec *vs) {
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = vs->items[i];
    if (v != NULL && v->fields.len > 0) return false;
  }
  return true;
}

static Vec erase_typed_variant_tys(Heap *heap, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = vs->items[i];
    if (v != NULL) {
      for (size_t j = 0; j < v->fields.len; j++) erase_typed_push(heap, &out, v->fields.items[j]);
    }
  }
  return out;
}

static MaybeStr erase_typed_unit_ctor(const Vec *vs) {
  MaybeStr r;
  VecGet g = vec_get(vs, 0);
  r.some = false;
  r.value = str_lit("");
  if (vs->len == 1 && g.status == VEC_GET_OK && g.value != NULL) {
    const RirVariant *v = g.value;
    if (v->fields.len == 0) {
      r.some = true;
      r.value = v->name;
    }
  }
  return r;
}

/* ---- Classes ---- */

static EtClass erase_typed_class(Heap *heap, EtClassKind kind) {
  EtClass c;
  c.kind = kind;
  c.name = str_lit("");
  c.generics = vec_new(heap->arena);
  c.params = vec_new(heap->arena);
  c.ret = NULL;
  c.item = NULL;
  c.why = str_lit("");
  return c;
}

static EtClass erase_typed_cno(Heap *heap, Str why) {
  EtClass c = erase_typed_class(heap, ET_CNO);
  c.why = why;
  return c;
}

static EtClass erase_typed_cdata(Heap *heap, const RirItem *item) {
  EtClass c = erase_typed_class(heap, ET_CDATA);
  c.item = item;
  return c;
}

/* The class `c` of the mech name, if the name comes back from its Rust name (D7). */
static EtClass erase_typed_name_kept(Heap *heap, Str mech, Str rust, Str back, EtClass c) {
  if (str_eq(back, mech)) return c;
  return erase_typed_cno(heap, erase_typed_back_text(heap, rust, back));
}

/* `ctor` is the variant name of the constructor; `variants` refuses a constructor with an
 * upper-case first letter, so the mech name is `lower_first(ctor)`. */
static EtClass erase_typed_unit_kept(Heap *heap, Str name, Str ctor) {
  Str mech = erase_typed_lower_first(heap, ctor);
  Str rust = erase_typed_fn_name(heap, mech);
  return erase_typed_name_kept(heap, mech, rust, erase_typed_fn_back(heap, rust),
                               erase_typed_cdata(heap, rir_iunit(heap, name, rust)));
}

static EtClass erase_typed_fn_class(Heap *heap, Str name, Vec gens, Vec params, const RirTy *ret) {
  Str rust = erase_typed_fn_name(heap, name);
  EtClass c = erase_typed_class(heap, ET_CFN);
  c.name = rust;
  c.generics = gens;
  c.params = params;
  c.ret = ret;
  return erase_typed_name_kept(heap, name, rust, erase_typed_fn_back(heap, rust), c);
}

static EtClass erase_typed_unit_class(Heap *heap, Str name, Vec gens, Vec vars, MaybeStr u) {
  Str unused = str_lit("a type parameter with no use in a field");
  if (u.some) {
    if (erase_typed_no_names(&gens)) return erase_typed_unit_kept(heap, name, u.value);
    return erase_typed_cno(heap, unused);
  }
  Vec tys = erase_typed_variant_tys(heap, &vars);
  Vec kept = erase_typed_used(heap, &gens, &tys);
  if (erase_typed_same_count(&kept, &gens))
    return erase_typed_cdata(heap, rir_ienum(heap, name, gens, erase_typed_all_unit(&vars), vars));
  return erase_typed_cno(heap, unused);
}

static EtClass erase_typed_data_class(Heap *heap, Str name, Vec gens, Vec vars) {
  return erase_typed_unit_class(heap, name, gens, vars, erase_typed_unit_ctor(&vars));
}

static EtClass erase_typed_family_class(Heap *heap, Str name, Vec gens, EtVars vs) {
  if (!vs.ok) return erase_typed_cno(heap, vs.why);
  if (str_eq(name, str_lit("MechBool"))) return erase_typed_class(heap, ET_CBOOL);
  return erase_typed_data_class(heap, name, gens, vs.vars);
}

static EtClass erase_typed_classify_family(Heap *heap, const EtEnv *env, const Family *fam) {
  const EtCtx *ctx = erase_typed_param_kinds(heap, &fam->params, NULL);
  if (level_always_zero(fam->level)) return erase_typed_class(heap, ET_CPROP);
  return erase_typed_family_class(heap, fam->name, erase_typed_gen_names(heap, ctx),
                                  erase_typed_variants(heap, env, ctx, fam->name, &fam->ctors));
}

static bool erase_typed_prop_family(MaybeFamily fam) {
  if (!fam.some) return false;
  return level_always_zero(fam.value.level);
}

/* D4: the body is, below its lambdas, a case with no branch on a value of a Prop family. */
static bool erase_typed_absurd(const EtEnv *env, const Term *body) {
  const Term *u = body;
  while (u != NULL) {
    switch (u->kind) {
      case TERM_SEC: {
        VecGet g = vec_get(&u->legs, 0);
        if (u->shape.kind != SHAPE_PI || g.status != VEC_GET_OK || g.value == NULL) return false;
        u = ((const Leg *)g.value)->body;
        break;
      }
      case TERM_ANN:
        u = u->body;
        break;
      case TERM_ELIM:
        if (u->data->shape.kind == SHAPE_MU && u->data->branches.len == 0)
          return erase_typed_prop_family(global_find_family(u->data->shape.name, erase_typed_globals_of(env)));
        return false;
      case TERM_VAR:
      case TERM_UNIV:
      case TERM_LAN:
      case TERM_RAN:
      case TERM_IN:
      case TERM_OUT:
      case TERM_LET:
      case TERM_GLOBAL:
      case TERM_LIT:
      case TERM_AUTO:
        return false;
    }
  }
  return false;
}

static EtClass erase_typed_head_class(Heap *heap, Str name, EtHead h) {
  switch (h.kind) {
    case ET_HFN:
      return erase_typed_fn_class(heap, name, h.generics, h.params, h.ret);
    case ET_HPROP:
      return erase_typed_class(heap, ET_CPROP);
    case ET_HNO:
      return erase_typed_cno(heap, h.why);
  }
  return erase_typed_cno(heap, h.why);
}

static EtClass erase_typed_run_class(Heap *heap, const EtEnv *env, Str name, const Term *ty, const Term *body) {
  if (erase_typed_absurd(env, body)) return erase_typed_class(heap, ET_CABSURD);
  return erase_typed_head_class(heap, name, erase_typed_tele(heap, env, NULL, ty, body));
}

static EtClass erase_typed_by_result(Heap *heap, EtEr e, EtClass c) {
  switch (e.kind) {
    case ET_EPROP:
      return erase_typed_class(heap, ET_CPROP);
    case ET_ENO:
      return erase_typed_cno(heap, e.why);
    case ET_EDATA:
      return c;
  }
  return c;
}

static EtClass erase_typed_by_end(Heap *heap, EtEnd e, EtClass c) {
  switch (e) {
    case ET_END_PROP:
      return erase_typed_class(heap, ET_CPROP);
    case ET_END_TYPE:
      return erase_typed_class(heap, ET_CTYPE);
    case ET_END_NONE:
      return c;
  }
  return c;
}

static EtClass erase_typed_def_class(Heap *heap, const EtEnv *env, Str name, const Term *ty, const Term *body) {
  EtClass run = erase_typed_run_class(heap, env, name, ty, body);
  EtClass res = erase_typed_by_result(heap, erase_typed_result_of(heap, env, NULL, ty), run);
  return erase_typed_by_end(heap, erase_typed_end_of(ty), res);
}

static EtClass erase_typed_axiom_class(Heap *heap, const EtEnv *env, const Term *ty) {
  EtClass res = erase_typed_by_result(heap, erase_typed_result_of(heap, env, NULL, ty),
                                      erase_typed_cno(heap, str_lit("a postulate in the runtime")));
  return erase_typed_by_end(heap, erase_typed_end_of(ty), res);
}

static EtClass erase_typed_entry_class(Heap *heap, const EtEnv *env, Str name, Entry entry) {
  switch (entry.kind) {
    case ENTRY_DEF:
      return erase_typed_def_class(heap, env, name, entry.def.ty, entry.def.body);
    case ENTRY_AXIOM:
      return erase_typed_axiom_class(heap, env, entry.ty);
    case ENTRY_PRIM:
      return erase_typed_cno(heap, str_lit("a primitive"));
  }
  return erase_typed_cno(heap, str_lit("a primitive"));
}

/* A name with no entry and no family is a constructor: no line. */
EtClass erase_typed_family_or_skip(Heap *heap, const EtEnv *env, MaybeFamily fam) {
  if (fam.some) return erase_typed_classify_family(heap, env, &fam.value);
  return erase_typed_class(heap, ET_CSKIP);
}

static EtClass erase_typed_entry_or(Heap *heap, const EtEnv *env, Str name, MaybeEntry entry, MaybeFamily fam) {
  if (entry.some) return erase_typed_entry_class(heap, env, name, entry.value);
  return erase_typed_family_or_skip(heap, env, fam);
}

EtClass erase_typed_name_class(Heap *heap, const EtEnv *env, Str name) {
  Globals globals = erase_typed_globals_of(env);
  return erase_typed_entry_or(heap, env, name, global_find(name, globals), global_find_family(name, globals));
}

/* ---- The classify table (part b2, Bend line 819 on) ---- */

/* List.append: `a` then `b`; a short result (OOM) sets heap->oom. */
static Vec erase_typed_append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_concat(heap->arena, a, b);
  if (out.len != a->len + b->len) heap->oom = true;
  return out;
}

/* join_rest: `sep ++ x` for every x of `xs` from index `from` on, into `buf`. */
static void erase_typed_join_rest(Heap *heap, StrBuf *buf, Str sep, const Vec *xs, size_t from) {
  for (size_t i = from; i < xs->len; i++) {
    const Str *x = xs->items[i];
    erase_typed_puts(heap, buf, sep);
    if (x != NULL) erase_typed_puts(heap, buf, *x);
  }
}

/* `xs` (Str pointers) joined by `sep`; "" for no items. */
Str erase_typed_join(Heap *heap, Str sep, const Vec *xs) {
  if (xs->len == 0) return str_lit("");
  StrBuf buf = strbuf_new(heap->arena);
  erase_typed_puts(heap, &buf, erase_typed_first(xs));
  erase_typed_join_rest(heap, &buf, sep, xs, 1);
  return strbuf_str(&buf);
}

/* open ++ join(", ", xs) ++ close; "" for no items. */
static Str erase_typed_wrap(Heap *heap, Str open, Str close, const Vec *xs) {
  if (xs->len == 0) return str_lit("");
  const Str parts[3] = {open, erase_typed_join(heap, str_lit(", "), xs), close};
  return erase_typed_cat(heap, parts, 3);
}

static Str erase_typed_show_ty(Heap *heap, const RirTy *t);

/* The text of each type of `ts` (RirTy pointers), as Str pointers. */
static Vec erase_typed_show_tys(Heap *heap, const Vec *ts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ts->len; i++) {
    const RirTy *t = ts->items[i];
    erase_typed_push(heap, &out, erase_typed_str_box(heap, erase_typed_show_ty(heap, t)));
  }
  return out;
}

static Str erase_typed_show_ty(Heap *heap, const RirTy *t) {
  if (t == NULL) return str_lit("");
  switch (t->kind) {
  case RIR_TBOOL:
    return str_lit("bool");
  case RIR_TCON: {
    Vec args = erase_typed_show_tys(heap, &t->args);
    return erase_typed_cat2(heap, t->name, erase_typed_wrap(heap, str_lit("<"), str_lit(">"), &args));
  }
  case RIR_TBOX: {
    const Str parts[3] = {str_lit("Box<"), erase_typed_show_ty(heap, t->inner), str_lit(">")};
    return erase_typed_cat(heap, parts, 3);
  }
  case RIR_TREF:
    return erase_typed_cat2(heap, str_lit("&"), erase_typed_show_ty(heap, t->inner));
  case RIR_TFN: {
    Vec args = erase_typed_show_tys(heap, &t->args);
    const Str parts[4] = {str_lit("impl Fn("), erase_typed_join(heap, str_lit(", "), &args), str_lit(") -> "),
                          erase_typed_show_ty(heap, t->ret)};
    return erase_typed_cat(heap, parts, 4);
  }
  }
  return str_lit("");
}

/* "name: ty" for each RirParam pointer of `ps`. */
static Vec erase_typed_show_params(Heap *heap, const Vec *ps) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = ps->items[i];
    if (p != NULL) {
      const Str parts[3] = {p->name, str_lit(": "), erase_typed_show_ty(heap, p->ty)};
      erase_typed_push(heap, &out, erase_typed_str_box(heap, erase_typed_cat(heap, parts, 3)));
    }
  }
  return out;
}

/* "g: Clone" for each generic name (Str pointers). */
static Vec erase_typed_bounds(Heap *heap, const Vec *gens) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < gens->len; i++) {
    const Str *g = gens->items[i];
    if (g != NULL) erase_typed_push(heap, &out, erase_typed_str_box(heap, erase_typed_cat2(heap, *g, str_lit(": Clone"))));
  }
  return out;
}

/* name ++ wrap("(", ")", fields) for each RirVariant pointer of `vs`. */
static Vec erase_typed_show_variants(Heap *heap, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < vs->len; i++) {
    const RirVariant *v = vs->items[i];
    if (v != NULL) {
      Vec fs = erase_typed_show_tys(heap, &v->fields);
      Str text = erase_typed_cat2(heap, v->name, erase_typed_wrap(heap, str_lit("("), str_lit(")"), &fs));
      erase_typed_push(heap, &out, erase_typed_str_box(heap, text));
    }
  }
  return out;
}

static Str erase_typed_show_item(Heap *heap, const RirItem *item) {
  if (item == NULL) return str_lit("");
  switch (item->kind) {
  case RIR_IUSE:
    return erase_typed_cat2(heap, str_lit("use "), item->module);
  case RIR_IENUM: {
    Vec vars = erase_typed_show_variants(heap, &item->variants);
    const Str parts[7] = {item->copy ? str_lit("copy ") : str_lit(""),
                          str_lit("enum "),
                          item->name,
                          erase_typed_wrap(heap, str_lit("<"), str_lit(">"), &item->generics),
                          str_lit(" { "),
                          erase_typed_join(heap, str_lit(", "), &vars),
                          str_lit(" }")};
    return erase_typed_cat(heap, parts, 7);
  }
  case RIR_IUNIT: {
    const Str parts[4] = {str_lit("struct "), item->name, str_lit("; fn "), item->ctor};
    return erase_typed_cat(heap, parts, 4);
  }
  case RIR_IFN:
    return erase_typed_cat2(heap, str_lit("fn "), item->name);
  }
  return str_lit("");
}

/* The table row of one declared name. */
static Str erase_typed_line(Heap *heap, Str name, const EtClass *c) {
  switch (c->kind) {
  case ET_CFN: {
    Vec bounds = erase_typed_bounds(heap, &c->generics);
    Vec params = erase_typed_show_params(heap, &c->params);
    const Str parts[10] = {str_lit("fn "),
                           name,
                           str_lit(" = pub fn "),
                           c->name,
                           erase_typed_wrap(heap, str_lit("<"), str_lit(">"), &bounds),
                           str_lit("("),
                           erase_typed_join(heap, str_lit(", "), &params),
                           str_lit(") -> "),
                           erase_typed_show_ty(heap, c->ret),
                           str_lit("\n")};
    return erase_typed_cat(heap, parts, 10);
  }
  case ET_CDATA: {
    const Str parts[5] = {str_lit("data "), name, str_lit(" = "), erase_typed_show_item(heap, c->item), str_lit("\n")};
    return erase_typed_cat(heap, parts, 5);
  }
  case ET_CBOOL: {
    const Str parts[3] = {str_lit("bool "), name, str_lit("\n")};
    return erase_typed_cat(heap, parts, 3);
  }
  case ET_CPROP: {
    const Str parts[3] = {str_lit("prop "), name, str_lit("\n")};
    return erase_typed_cat(heap, parts, 3);
  }
  case ET_CTYPE: {
    const Str parts[3] = {str_lit("type "), name, str_lit("\n")};
    return erase_typed_cat(heap, parts, 3);
  }
  case ET_CABSURD: {
    const Str parts[3] = {str_lit("absurd "), name, str_lit("\n")};
    return erase_typed_cat(heap, parts, 3);
  }
  case ET_CNO: {
    const Str parts[5] = {str_lit("refused "), name, str_lit(": "), c->why, str_lit("\n")};
    return erase_typed_cat(heap, parts, 5);
  }
  case ET_CSKIP:
    return str_lit("");
  }
  return str_lit("");
}

/* The Rust item names of a class, for the collision check (D7). */
static Vec erase_typed_item_names(Heap *heap, const EtClass *c) {
  Vec out = vec_new(heap->arena);
  switch (c->kind) {
  case ET_CFN:
    erase_typed_push(heap, &out, erase_typed_str_box(heap, c->name));
    return out;
  case ET_CDATA:
    if (c->item == NULL) return out;
    switch (c->item->kind) {
    case RIR_IENUM:
      erase_typed_push(heap, &out, erase_typed_str_box(heap, c->item->name));
      return out;
    case RIR_IUNIT:
      erase_typed_push(heap, &out, erase_typed_str_box(heap, c->item->name));
      erase_typed_push(heap, &out, erase_typed_str_box(heap, c->item->ctor));
      return out;
    case RIR_IUSE:
    case RIR_IFN:
      return out;
    }
    return out;
  case ET_CBOOL:
  case ET_CPROP:
  case ET_CTYPE:
  case ET_CABSURD:
  case ET_CNO:
  case ET_CSKIP:
    return out;
  }
  return out;
}

static Vec erase_typed_copy_names(Heap *heap, const EtClass *c) {
  Vec out = vec_new(heap->arena);
  switch (c->kind) {
  case ET_CDATA:
    if (c->item == NULL) return out;
    switch (c->item->kind) {
    case RIR_IENUM:
      if (c->item->copy) erase_typed_push(heap, &out, erase_typed_str_box(heap, c->item->name));
      return out;
    case RIR_IUNIT:
      erase_typed_push(heap, &out, erase_typed_str_box(heap, c->item->name));
      return out;
    case RIR_IUSE:
    case RIR_IFN:
      return out;
    }
    return out;
  case ET_CFN:
  case ET_CBOOL:
  case ET_CPROP:
  case ET_CTYPE:
  case ET_CABSURD:
  case ET_CNO:
  case ET_CSKIP:
    return out;
  }
  return out;
}

/* St: `names` = the Rust item names so far (Str pointers, newest step first, as the
 * Bend prepend); `out` = the table text. */
typedef struct {
  EtEnv env;
  Vec names;
  Str out;
} EtSt;

/* One row: the env and names of the next St; the row text goes to `out` (steps
 * owns the buffer, so the table text is not copied once per row). */
static EtSt erase_typed_step(Heap *heap, EtSt st, StrBuf *out, Str name) {
  EtClass c = erase_typed_name_class(heap, &st.env, name);
  Vec copies = erase_typed_copy_names(heap, &c);
  Vec items = erase_typed_item_names(heap, &c);
  EtSt next;
  next.env = erase_typed_env(erase_typed_globals_of(&st.env), erase_typed_append(heap, &copies, erase_typed_copies_of(&st.env)));
  next.names = erase_typed_append(heap, &items, &st.names);
  next.out = st.out;
  erase_typed_puts(heap, out, erase_typed_line(heap, name, &c));
  return next;
}

static EtSt erase_typed_steps(Heap *heap, EtSt st, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  erase_typed_puts(heap, &buf, st.out);
  EtSt cur = st;
  for (size_t i = 0; i < names->len; i++) {
    const Str *name = names->items[i];
    if (name != NULL) cur = erase_typed_step(heap, cur, &buf, *name);
  }
  cur.out = strbuf_str(&buf);
  return cur;
}

/* Declared names in source order (a `mu` adds no row, so the rows do not
 * have the position of a family). `decls` holds SDecl pointers. */
Vec erase_typed_decl_names(Heap *heap, const Vec *decls) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < decls->len; i++) {
    const SDecl *d = decls->items[i];
    if (d != NULL) {
      Vec names = elab_declared_names(heap, d);
      for (size_t j = 0; j < names.len; j++) erase_typed_push(heap, &out, names.items[j]);
    }
  }
  return out;
}

static EtSt erase_typed_failed(Heap *heap, Error e) {
  EtSt st;
  st.env = erase_typed_env(global_empty(), vec_new(heap->arena));
  st.names = vec_new(heap->arena);
  const Str parts[3] = {str_lit("error: "), error_to_string(heap, e), str_lit("\n")};
  st.out = erase_typed_cat(heap, parts, 3);
  return st;
}

/* Checks one source file on top of `env` and classifies its declarations.
 * `r` holds an ElabChecked * when done. */
static EtSt erase_typed_checked_steps(Heap *heap, Vec copies, Vec names, const Vec *decls, Result r) {
  switch (r.kind) {
  case RESULT_FAIL:
    return erase_typed_failed(heap, r.error);
  case RESULT_DONE: {
    const ElabChecked *checked = r.value;
    if (checked == NULL) heap->oom = true;
    EtSt st;
    st.env = erase_typed_env(checked != NULL ? checked->globals : global_empty(), copies);
    st.names = names;
    st.out = str_lit("");
    Vec declared = erase_typed_decl_names(heap, decls);
    return erase_typed_steps(heap, st, &declared);
  }
  }
  return erase_typed_failed(heap, r.error);
}

/* `r` holds a Vec * of SDecl pointers when done. */
static EtSt erase_typed_parsed_steps(Heap *heap, Globals globals, Vec copies, Vec names, Result r) {
  switch (r.kind) {
  case RESULT_FAIL:
    return erase_typed_failed(heap, r.error);
  case RESULT_DONE: {
    const Vec *parsed = r.value;
    Vec none = vec_new(heap->arena);
    if (parsed == NULL) heap->oom = true;
    const Vec *decls = parsed != NULL ? parsed : &none;
    return erase_typed_checked_steps(heap, copies, names, decls,
                                     elab_elab_program_in(heap, globals, budget_unlimited(), decls));
  }
  }
  return erase_typed_failed(heap, r.error);
}

static EtSt erase_typed_classify_source(Heap *heap, const EtEnv *env, Vec names, Str source) {
  return erase_typed_parsed_steps(heap, env->globals, env->copies, names, parser_program(heap, source));
}

/* Whether `name` is one of xs[from..]. */
static bool erase_typed_has_from(const Vec *xs, size_t from, Str name) {
  for (size_t i = from; i < xs->len; i++) {
    const Str *x = xs->items[i];
    if (x != NULL && str_eq(*x, name)) return true;
  }
  return false;
}

/* The first name that occurs again later in `names`; "" for none. */
Str erase_typed_first_dup(const Vec *names) {
  for (size_t i = 0; i < names->len; i++) {
    const Str *x = names->items[i];
    if (x != NULL && erase_typed_has_from(names, i + 1, *x)) return *x;
  }
  return str_lit("");
}

static Str erase_typed_collision_line(Heap *heap, const Vec *names) {
  Str d = erase_typed_first_dup(names);
  if (str_eq(d, str_lit(""))) return str_lit("names: no collision\n");
  const Str parts[3] = {str_lit("refused: name collision (D7): "), d, str_lit("\n")};
  return erase_typed_cat(heap, parts, 3);
}

static Str erase_typed_finish(Heap *heap, const EtSt *st) {
  return erase_typed_cat2(heap, st->out, erase_typed_collision_line(heap, &st->names));
}

Str erase_typed_classify1(Heap *heap, Str source) {
  EtEnv env = erase_typed_env(global_initial(heap), vec_new(heap->arena));
  EtSt st = erase_typed_classify_source(heap, &env, vec_new(heap->arena), source);
  return erase_typed_finish(heap, &st);
}

static Str erase_typed_second(Heap *heap, const EtSt *st, Str source) {
  EtSt next = erase_typed_classify_source(heap, &st->env, st->names, source);
  const Str parts[4] = {str_lit("## 1\n"), st->out, str_lit("## 2\n"), erase_typed_finish(heap, &next)};
  return erase_typed_cat(heap, parts, 4);
}

Str erase_typed_classify2(Heap *heap, Str first_source, Str second_source) {
  EtEnv env = erase_typed_env(global_initial(heap), vec_new(heap->arena));
  EtSt st = erase_typed_classify_source(heap, &env, vec_new(heap->arena), first_source);
  return erase_typed_second(heap, &st, second_source);
}

/* ---- Bodies (part c, Bend line 1015 on) ---- */

/* Want: the use of a result (by value, by reference, or a reference to a function with these
 * argument types). */
typedef enum { ET_WOWN, ET_WREF, ET_WFN } EtWantKind;
typedef struct {
  EtWantKind kind;
  Vec args; /* ET_WFN: RirTy pointers */
} EtWant;

/* Rx: an expression, or why there is none. */
typedef enum { ET_XOK, ET_XNO } EtRxKind;
typedef struct {
  EtRxKind kind;
  const RirEx *ex; /* ET_XOK */
  Str why;         /* ET_XNO */
} EtRx;

/* List<Maybe<R.Param>> as a cons list: NULL = Nil, a NULL head = None (an erased variable), index
 * 0 = the head. */
typedef struct EtSlots EtSlots;
struct EtSlots {
  const RirParam *head;
  const EtSlots *rest;
};

/* Job{ks, vs, want, term}. */
typedef struct {
  const EtCtx *ks;
  const EtSlots *vs;
  EtWant want;
  const Term *term;
} EtJob;

/* Build: how to make an expression from the results of the jobs of a plan. ET_BUILD_MATCH: the
 * first result is the scrutinee, then one result for each arm. */
typedef enum {
  ET_BUILD_LEAF,
  ET_BUILD_SAME,
  ET_BUILD_CALL,
  ET_BUILD_CTOR,
  ET_BUILD_MATCH,
  ET_BUILD_IF,
  ET_BUILD_CLOSURE,
  ET_BUILD_LET
} EtBuildKind;
typedef struct {
  EtBuildKind kind;
  EtRx rx;          /* LEAF */
  Str func;         /* CALL */
  Vec targs;        /* CALL: RirTy pointers */
  EtWant want;      /* CALL, CTOR, MATCH, IF, CLOSURE, LET */
  Str family;       /* CTOR */
  Str ctor;         /* CTOR */
  Vec boxes;        /* CTOR: bool pointers */
  Vec pats;         /* MATCH: RirPat pointers */
  bool true_first;  /* IF */
  Vec params;       /* CLOSURE: RirParam pointers */
  Str name;         /* LET */
  const RirTy *ty;  /* LET */
} EtBuild;

/* Plan{jobs, build}: jobs = EtJob pointers. */
typedef struct {
  Vec jobs;
  EtBuild build;
} EtPlan;

static const Term erase_typed_auto_term = {.kind = TERM_AUTO};
static const EtJob erase_typed_job_placeholder = {NULL, NULL, {ET_WOWN, {0}}, &erase_typed_auto_term};
static const bool erase_typed_true = true;
static const bool erase_typed_false = false;

static EtWant erase_typed_want(EtWantKind kind, Vec args) {
  EtWant w;
  w.kind = kind;
  w.args = args;
  return w;
}

static EtRx erase_typed_xok(const RirEx *ex) {
  EtRx r = {ET_XOK, ex, {0}};
  return r;
}

static EtRx erase_typed_xno(Str why) {
  EtRx r = {ET_XNO, NULL, why};
  return r;
}

/* `slot <> vs`. A failed allocation sets heap->oom and returns `vs`. */
static const EtSlots *erase_typed_slot_cons(Heap *heap, const RirParam *slot, const EtSlots *vs) {
  EtSlots *node = heap_alloc(heap, sizeof(EtSlots));
  if (node == NULL) return vs;
  node->head = slot;
  node->rest = vs;
  return node;
}

static const EtJob *erase_typed_job(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, const Term *term) {
  EtJob *box = heap_alloc(heap, sizeof(EtJob));
  if (box == NULL) return &erase_typed_job_placeholder;
  box->ks = ks;
  box->vs = vs;
  box->want = want;
  box->term = term;
  return box;
}

static EtBuild erase_typed_build(Heap *heap, EtBuildKind kind, EtWant want) {
  EtBuild b = {0};
  b.kind = kind;
  b.rx = erase_typed_xno(str_empty());
  b.targs = vec_new(heap->arena);
  b.want = want;
  b.boxes = vec_new(heap->arena);
  b.pats = vec_new(heap->arena);
  b.params = vec_new(heap->arena);
  return b;
}

static EtPlan erase_typed_plan(Vec jobs, EtBuild build) {
  EtPlan p;
  p.jobs = jobs;
  p.build = build;
  return p;
}

/* slot_at: the slot at `index`, or NULL (None) past the end; an index past u64 is past the end. */
static const RirParam *erase_typed_slot_at(const EtSlots *vs, Nat index) {
  NatToU64 n = nat_to_u64(index);
  switch (n.status) {
    case NAT_TO_U64_OVERFLOW:
      return NULL;
    case NAT_TO_U64_OK:
      break;
  }
  const EtSlots *at = vs;
  for (uint64_t i = 0; i < n.value && at != NULL; i++) at = at->rest;
  if (at == NULL) return NULL;
  return at->head;
}

static EtRx erase_typed_wanted(Heap *heap, EtWant want, const RirEx *ex) {
  switch (want.kind) {
    case ET_WOWN:
      return erase_typed_xok(ex);
    case ET_WREF:
      return erase_typed_xok(rir_xborrow(heap, ex));
    case ET_WFN:
      return erase_typed_xok(rir_xborrow(heap, ex));
  }
  return erase_typed_xok(ex);
}

static bool erase_typed_is_box(const RirTy *t) {
  if (t == NULL) return false;
  switch (t->kind) {
    case RIR_TBOX:
      return true;
    case RIR_TBOOL:
    case RIR_TCON:
    case RIR_TREF:
    case RIR_TFN:
      return false;
  }
  return false;
}

static EtRx erase_typed_ref_use(Heap *heap, Str name, bool in_box, EtWant want) {
  switch (want.kind) {
    case ET_WOWN:
      if (in_box) return erase_typed_xno(str_lit("a boxed field by value (M0)"));
      return erase_typed_xok(rir_xclone(heap, rir_xvar(heap, name)));
    case ET_WREF:
      return erase_typed_xok(rir_xvar(heap, name));
    case ET_WFN:
      return erase_typed_xok(rir_xvar(heap, name));
  }
  return erase_typed_xok(rir_xvar(heap, name));
}

static EtRx erase_typed_var_use(Heap *heap, Str name, const RirTy *ty, EtWant want) {
  if (ty == NULL) return erase_typed_wanted(heap, want, rir_xvar(heap, name));
  switch (ty->kind) {
    case RIR_TREF:
      return erase_typed_ref_use(heap, name, erase_typed_is_box(ty->inner), want);
    case RIR_TBOOL:
    case RIR_TCON:
    case RIR_TBOX:
    case RIR_TFN:
      return erase_typed_wanted(heap, want, rir_xvar(heap, name));
  }
  return erase_typed_wanted(heap, want, rir_xvar(heap, name));
}

static EtRx erase_typed_var_rx(Heap *heap, const RirParam *slot, EtWant want) {
  if (slot == NULL) return erase_typed_xno(str_lit("an erased variable in a runtime position"));
  return erase_typed_var_use(heap, slot->name, slot->ty, want);
}

static EtPlan erase_typed_leaf(Heap *heap, EtRx rx) {
  EtBuild b = erase_typed_build(heap, ET_BUILD_LEAF, erase_typed_want(ET_WOWN, vec_new(heap->arena)));
  b.rx = rx;
  return erase_typed_plan(vec_new(heap->arena), b);
}

static EtPlan erase_typed_no_plan(Heap *heap, Str why) { return erase_typed_leaf(heap, erase_typed_xno(why)); }

static EtWant erase_typed_fn_want(Heap *heap, const RirTy *inner) {
  if (inner == NULL) return erase_typed_want(ET_WREF, vec_new(heap->arena));
  switch (inner->kind) {
    case RIR_TFN:
      return erase_typed_want(ET_WFN, inner->args);
    case RIR_TBOOL:
    case RIR_TCON:
    case RIR_TBOX:
    case RIR_TREF:
      return erase_typed_want(ET_WREF, vec_new(heap->arena));
  }
  return erase_typed_want(ET_WREF, vec_new(heap->arena));
}

static EtWant erase_typed_want_of(Heap *heap, const RirTy *t) {
  if (t == NULL) return erase_typed_want(ET_WOWN, vec_new(heap->arena));
  switch (t->kind) {
    case RIR_TREF:
      return erase_typed_fn_want(heap, t->inner);
    case RIR_TBOOL:
    case RIR_TCON:
    case RIR_TBOX:
    case RIR_TFN:
      return erase_typed_want(ET_WOWN, vec_new(heap->arena));
  }
  return erase_typed_want(ET_WOWN, vec_new(heap->arena));
}

/* Constructors. One entry for each constructor argument: a RirTy pointer = a kept field with its
 * variant type, NULL (None) = an erased field. */

static const RirTy *erase_typed_kept_ty(Heap *heap, Str own, EtBd bd) {
  switch (bd.kind) {
    case ET_BPARAM:
      return erase_typed_boxed(heap, own, bd.ty);
    case ET_BGEN:
    case ET_BDROP:
    case ET_BNO:
      return NULL;
  }
  return NULL;
}

/* field_tys: the binders (TeleBinder pointers) left to right, each seeing the kinds of the ones
 * before it. */
static Vec erase_typed_field_tys(Heap *heap, const EtEnv *env, const EtCtx *ctx, Str own, const Vec *args) {
  Vec out = vec_new(heap->arena);
  const EtCtx *at = ctx;
  for (size_t i = 0; i < args->len; i++) {
    const TeleBinder *b = args->items[i];
    if (b != NULL && b->ty != NULL) {
      EtEnd e = erase_typed_end_of(b->ty);
      EtBd bd = erase_typed_binder_er(b->quantity, e, erase_typed_er(heap, env, at, b->ty));
      erase_typed_push(heap, &out, erase_typed_kept_ty(heap, own, bd));
      at = erase_typed_cons(heap, erase_typed_kind_of(heap, e, b->name), at);
    }
  }
  return out;
}

static Vec erase_typed_ctor_args_tys(Heap *heap, const EtEnv *env, const EtCtx *ctx, Str own, MaybeCtor ctor) {
  if (!ctor.some) return vec_new(heap->arena);
  return erase_typed_field_tys(heap, env, ctx, own, &ctor.value.args);
}

Vec erase_typed_ctor_tys(Heap *heap, const EtEnv *env, MaybeFamily fam, Str c) {
  if (!fam.some) return vec_new(heap->arena);
  const Family *f = &fam.value;
  /* Sequenced into locals: tcc (arm64) corrupts the argument list when the struct-returning
   * positivity_find_ctor call is nested next to the erase_typed_param_kinds call (hop 202 F-201a). */
  const EtCtx *kinds = erase_typed_param_kinds(heap, &f->params, NULL);
  MaybeCtor ctor = positivity_find_ctor(&f->ctors, c);
  return erase_typed_ctor_args_tys(heap, env, kinds, f->name, ctor);
}

/* kept_jobs: one WOwn job for each kept field (`tys` = RirTy pointers, NULL = erased), zipped
 * with the field terms `fs` up to the shorter list. kept_job is the step. */
static void erase_typed_kept_job(Heap *heap, const EtCtx *ks, const EtSlots *vs, const RirTy *t, const Term *f, Vec *jobs) {
  if (t == NULL) return;
  erase_typed_push(heap, jobs, erase_typed_job(heap, ks, vs, erase_typed_want(ET_WOWN, vec_new(heap->arena)), f));
}

static Vec erase_typed_kept_jobs(Heap *heap, const EtCtx *ks, const EtSlots *vs, const Vec *tys, const Vec *fs) {
  Vec jobs = vec_new(heap->arena);
  for (size_t i = 0; i < tys->len && i < fs->len; i++) erase_typed_kept_job(heap, ks, vs, tys->items[i], fs->items[i], &jobs);
  return jobs;
}

/* box_flags: one flag (a bool pointer) for each kept field; box_step is the step. */
static void erase_typed_box_step(Heap *heap, const RirTy *t, Vec *flags) {
  if (t == NULL) return;
  erase_typed_push(heap, flags, erase_typed_is_box(t) ? &erase_typed_true : &erase_typed_false);
}

static Vec erase_typed_box_flags(Heap *heap, const Vec *tys) {
  Vec flags = vec_new(heap->arena);
  for (size_t i = 0; i < tys->len; i++) erase_typed_box_step(heap, tys->items[i], &flags);
  return flags;
}

static EtPlan erase_typed_enum_plan(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, Str family, Str ctor,
                                    const Vec *fs, const Vec *tys) {
  EtBuild b = erase_typed_build(heap, ET_BUILD_CTOR, want);
  b.family = family;
  b.ctor = ctor;
  b.boxes = erase_typed_box_flags(heap, tys);
  return erase_typed_plan(erase_typed_kept_jobs(heap, ks, vs, tys, fs), b);
}

static EtPlan erase_typed_item_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want, Str fam,
                                    Str c, const Vec *fs, const RirItem *item) {
  Str why = str_lit("a constructor of an item that is not data");
  if (item == NULL) return erase_typed_no_plan(heap, why);
  switch (item->kind) {
    case RIR_IUNIT:
      return erase_typed_leaf(heap, erase_typed_wanted(heap, want, rir_xcall(heap, item->ctor, vec_new(heap->arena),
                                                                             vec_new(heap->arena))));
    case RIR_IENUM: {
      Vec tys = erase_typed_ctor_tys(heap, env, global_find_family(fam, erase_typed_globals_of(env)), c);
      return erase_typed_enum_plan(heap, ks, vs, want, item->name, erase_typed_upper_first(heap, c), fs, &tys);
    }
    case RIR_IUSE:
    case RIR_IFN:
      return erase_typed_no_plan(heap, why);
  }
  return erase_typed_no_plan(heap, why);
}

static EtPlan erase_typed_ctor_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want, Str fam,
                                    Str c, const Vec *fs, EtClass cls) {
  Str why = erase_typed_cat2(heap, str_lit("a constructor outside the M0 fragment: "), c);
  switch (cls.kind) {
    case ET_CBOOL:
      return erase_typed_leaf(heap, erase_typed_wanted(heap, want, rir_xbool(heap, str_eq(c, str_lit("mechTrue")))));
    case ET_CDATA:
      return erase_typed_item_plan(heap, env, ks, vs, want, fam, c, fs, cls.item);
    case ET_CFN:
    case ET_CPROP:
    case ET_CTYPE:
    case ET_CABSURD:
    case ET_CNO:
    case ET_CSKIP:
      return erase_typed_no_plan(heap, why);
  }
  return erase_typed_no_plan(heap, why);
}

/* ---- Applications ---- */

/* Sarg{q, dom, arg}: dom NULL = None (an SArg frame). */
typedef struct {
  Quantity q;
  const Term *dom;
  const Term *arg;
} EtSarg;

/* Spine{head, args}: args = EtSarg pointers. */
typedef struct {
  const Term *head;
  Vec args;
} EtSpine;

static const EtSarg erase_typed_sarg_placeholder = {QUANTITY_ZERO, NULL, &erase_typed_auto_term};

static const EtSarg *erase_typed_sarg(Heap *heap, Quantity q, const Term *dom, const Term *arg) {
  EtSarg *box = heap_alloc(heap, sizeof(EtSarg));
  if (box == NULL) return &erase_typed_sarg_placeholder;
  box->q = q;
  box->dom = dom;
  box->arg = arg;
  return box;
}

/* An Out frame with a point address and a Pi or Arg shape: one argument of the spine. */
static bool erase_typed_is_app(const Term *t) {
  if (t == NULL || t->kind != TERM_OUT || t->addr.kind != ADDR_PT || t->head == NULL) return false;
  return t->shape.kind == SHAPE_PI || t->shape.kind == SHAPE_ARG;
}

/* spine(t, acc): the frames from the outermost in (each Bend step prepends to acc), so the
 * arguments are the walked frames reversed, then `acc`. */
static EtSpine erase_typed_spine(Heap *heap, const Term *t, const Vec *acc) {
  Vec walked = vec_new(heap->arena);
  const Term *u = t;
  while (erase_typed_is_app(u)) {
    const Term *dom = u->shape.kind == SHAPE_PI ? (const Term *)u->shape.domain : NULL;
    erase_typed_push(heap, &walked, erase_typed_sarg(heap, u->shape.quantity, dom, u->addr.arg));
    u = u->head;
  }
  Vec rev = heap_vec_reverse(heap, &walked);
  Vec args = vec_concat(heap->arena, &rev, acc);
  if (args.len != rev.len + acc->len) heap->oom = true;
  EtSpine s;
  s.head = u;
  s.args = args;
  return s;
}

static Vec erase_typed_ref_jobs(Heap *heap, const EtCtx *ks, const EtSlots *vs, const Vec *args) {
  Vec jobs = vec_new(heap->arena);
  for (size_t i = 0; i < args->len; i++) {
    const EtSarg *s = args->items[i];
    if (s != NULL) erase_typed_push(heap, &jobs, erase_typed_job(heap, ks, vs, erase_typed_want(ET_WREF, vec_new(heap->arena)), s->arg));
  }
  return jobs;
}

/* keep_term: `arg <> tail` for a kept binder. The tail is built back to front (appended), so the
 * caller reverses it once at the end. */
static Vec erase_typed_keep_term(Heap *heap, EtBd bd, const Term *arg, Vec tail) {
  switch (bd.kind) {
    case ET_BPARAM:
      erase_typed_push(heap, &tail, arg);
      return tail;
    case ET_BGEN:
    case ET_BDROP:
    case ET_BNO:
      return tail;
  }
  return tail;
}

/* Kept: the runtime arguments of a call of a local function (Term pointers), or why there are
 * none. */
typedef struct {
  bool ok;
  Vec args; /* ok */
  Str why;  /* !ok */
} EtKept;

static EtKept erase_typed_kept_cons(Heap *heap, EtBd bd, const Term *arg, EtKept rest) {
  if (!rest.ok) return rest;
  rest.args = erase_typed_keep_term(heap, bd, arg, rest.args);
  return rest;
}

/* head_bds: the binders of the type of a local function (EtBd pointers). */
static Vec erase_typed_head_bds(Heap *heap, EtKnd k) {
  switch (k.kind) {
    case ET_KLAM:
      return k.bds;
    case ET_KGEN:
    case ET_KPROP:
    case ET_KVAL:
      return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

/* kept_args: the runtime arguments of a call of a local function, in step with `bds` (EtBd
 * pointers), the binders of its type. An SPi frame decides with its own domain (in `ks`); an SArg
 * frame takes the next binder of the type (sarg_kept), and an SArg frame past the known binders
 * stops the walk with KeptNo. bds_tail = one index step (the tail of Nil is Nil). The binders are
 * decided left to right up to the stop, then folded from the last one back with kept_cons (the
 * Bend right fold); kept arguments are appended, so they are reversed once at the end. */
static EtKept erase_typed_kept_args(Heap *heap, const EtEnv *env, const EtCtx *ks, const Vec *bds, const Vec *args) {
  Vec decided = vec_new(heap->arena);
  Vec terms = vec_new(heap->arena);
  EtKept r = {true, vec_new(heap->arena), {0}};
  size_t bi = 0;
  for (size_t i = 0; i < args->len && r.ok; i++) {
    const EtSarg *s = args->items[i];
    if (s != NULL && s->dom != NULL) {
      EtBd bd = erase_typed_binder_er(s->q, erase_typed_end_of(s->dom), erase_typed_er(heap, env, ks, s->dom));
      erase_typed_push(heap, &decided, erase_typed_bd_box(heap, bd));
      erase_typed_push(heap, &terms, s->arg);
      bi += 1;
    } else if (s != NULL) {
      VecGet g = vec_get(bds, bi);
      if (g.status == VEC_GET_OK) {
        erase_typed_push(heap, &decided, g.value != NULL ? g.value : &erase_typed_bd_placeholder);
        erase_typed_push(heap, &terms, s->arg);
        bi += 1;
      } else {
        r.ok = false;
        r.why = str_lit("an SArg argument of a local function with no known binder for it");
      }
    }
  }
  for (size_t i = decided.len; i > 0 && i <= terms.len; i--) {
    const EtBd *bd = decided.items[i - 1];
    r = erase_typed_kept_cons(heap, *bd, terms.items[i - 1], r);
  }
  if (r.ok) r.args = heap_vec_reverse(heap, &r.args);
  return r;
}

/* zip_jobs: one job for each runtime argument, wanted as its parameter type says, up to the
 * shorter of `tys` (RirTy pointers) and `args` (Term pointers). */
static Vec erase_typed_zip_jobs(Heap *heap, const EtCtx *ks, const EtSlots *vs, const Vec *tys, const Vec *args) {
  Vec jobs = vec_new(heap->arena);
  for (size_t i = 0; i < tys->len && i < args->len; i++)
    erase_typed_push(heap, &jobs, erase_typed_job(heap, ks, vs, erase_typed_want_of(heap, tys->items[i]), args->items[i]));
  return jobs;
}

static EtPlan erase_typed_local_call(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, Str name, const Vec *tys,
                                     EtKept kept) {
  if (!kept.ok) return erase_typed_no_plan(heap, kept.why);
  EtBuild b = erase_typed_build(heap, ET_BUILD_CALL, want);
  b.func = name;
  return erase_typed_plan(erase_typed_zip_jobs(heap, ks, vs, tys, &kept.args), b);
}

/* ---- Type arguments (part c2, Bend line 1297 on) ---- */

/* The erasure of a type argument: the type below its lambdas. */
static EtEr erase_typed_targ_er(Heap *heap, const EtEnv *env, const EtCtx *ks, const Term *arg) {
  const EtCtx *at = ks;
  const Term *u = arg;
  while (u != NULL && u->kind == TERM_SEC && u->shape.kind == SHAPE_PI && u->shape.domain != NULL) {
    VecGet g = vec_get(&u->legs, 0);
    if (g.status != VEC_GET_OK || g.value == NULL) break;
    const Term *dom = u->shape.domain;
    at = erase_typed_cons(heap, erase_typed_kind_of(heap, erase_typed_end_of(dom), u->shape.name), at);
    u = ((const Leg *)g.value)->body;
  }
  return erase_typed_er(heap, env, at, u);
}

/* Cargs{targs, jobs, bad}: targs = RirTy pointers, jobs = EtJob pointers, bad = Str pointers. The
 * Bend call_args is a right fold that prepends at each step, so each list ends in argument order;
 * the C walk appends in that same order. */
typedef struct {
  Vec targs;
  Vec jobs;
  Vec bad;
} EtCargs;

static EtCargs erase_typed_cargs(Heap *heap) {
  EtCargs ca;
  ca.targs = vec_new(heap->arena);
  ca.jobs = vec_new(heap->arena);
  ca.bad = vec_new(heap->arena);
  return ca;
}

static void erase_typed_push_job(Heap *heap, const EtJob *j, EtCargs *ca) { erase_typed_push(heap, &ca->jobs, j); }

static void erase_typed_push_bad(Heap *heap, Str why, EtCargs *ca) {
  erase_typed_push(heap, &ca->bad, erase_typed_str_box(heap, why));
}

static void erase_typed_push_ty_arg(Heap *heap, const RirTy *t, EtCargs *ca) { erase_typed_push(heap, &ca->targs, t); }

static void erase_typed_push_targ(Heap *heap, EtEr e, EtCargs *ca) {
  switch (e.kind) {
    case ET_EDATA:
      erase_typed_push_ty_arg(heap, e.ty, ca);
      return;
    case ET_EPROP:
      erase_typed_push_bad(heap, str_lit("a Prop as a type argument"), ca);
      return;
    case ET_ENO:
      erase_typed_push_bad(heap, e.why, ca);
      return;
  }
}

static void erase_typed_gen_step(Heap *heap, bool turbofish, EtEr e, EtCargs *ca) {
  if (turbofish) erase_typed_push_targ(heap, e, ca);
}

static void erase_typed_arg_step(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, bool turbofish,
                                 Quantity q, const Term *arg, EtBd bd, EtCargs *ca) {
  switch (bd.kind) {
    case ET_BGEN:
      erase_typed_gen_step(heap, turbofish, erase_typed_targ_er(heap, env, ks, arg), ca);
      return;
    case ET_BDROP:
      return;
    case ET_BPARAM: {
      const RirTy *t = erase_typed_mode(heap, erase_typed_copies_of(env), q, bd.ty);
      erase_typed_push_job(heap, erase_typed_job(heap, ks, vs, erase_typed_want_of(heap, t), arg), ca);
      return;
    }
    case ET_BNO:
      erase_typed_push_bad(heap, bd.why, ca);
      return;
  }
}

/* args_end: the end of the telescope; arguments left over (from `next` on) are a refusal. */
static void erase_typed_args_end(Heap *heap, const Vec *args, size_t next, EtCargs *ca) {
  if (next < args->len) erase_typed_push_bad(heap, str_lit("more arguments than the telescope of the callee (M0)"), ca);
}

/* The telescope of the callee and the arguments, in step. The mode of each argument comes from the
 * telescope, not from the application node. Each binder is decided with the type context `tctx`
 * of the binders before it; the context grows by one kind at each step. */
static EtCargs erase_typed_call_args(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, const Vec *gens,
                                     const EtCtx *tctx, const Term *ty, const Vec *args) {
  EtCargs ca = erase_typed_cargs(heap);
  const EtCtx *at = tctx;
  const Term *u = ty;
  size_t i = 0;
  while (u != NULL && erase_typed_is_pi(u) && u->body != NULL && u->shape.domain != NULL) {
    if (i >= args->len) {
      erase_typed_push_bad(heap, str_lit("a partial application (M0)"), &ca);
      return ca;
    }
    const EtSarg *s = args->items[i];
    const Term *arg = s != NULL ? s->arg : &erase_typed_auto_term;
    const Term *dom = u->shape.domain;
    Quantity q = u->shape.quantity;
    Str x = u->shape.name;
    EtEnd e = erase_typed_end_of(dom);
    EtBd bd = erase_typed_binder_er(q, e, erase_typed_er(heap, env, at, dom));
    erase_typed_arg_step(heap, env, ks, vs, erase_typed_has(gens, x), q, arg, bd, &ca);
    at = erase_typed_cons(heap, erase_typed_kind_of(heap, e, x), at);
    u = u->body;
    i += 1;
  }
  erase_typed_args_end(heap, args, i, &ca);
  return ca;
}

static EtPlan erase_typed_call_built(Heap *heap, Str name, EtWant want, EtCargs ca) {
  VecGet g = vec_get(&ca.bad, 0);
  if (g.status == VEC_GET_OK) {
    const Str *why = g.value;
    return erase_typed_no_plan(heap, why != NULL ? *why : str_empty());
  }
  EtBuild b = erase_typed_build(heap, ET_BUILD_CALL, want);
  b.func = name;
  b.targs = ca.targs;
  return erase_typed_plan(ca.jobs, b);
}

static EtPlan erase_typed_fn_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                  EtClass cls, MaybeEntry entry, const Vec *args) {
  switch (cls.kind) {
    case ET_CFN:
      if (entry.some)
        return erase_typed_call_built(
            heap, cls.name, want,
            erase_typed_call_args(heap, env, ks, vs, &cls.generics, NULL, global_entry_ty(entry.value), args));
      return erase_typed_no_plan(heap, str_lit("a call of a name with no entry"));
    case ET_CABSURD:
      return erase_typed_no_plan(heap, str_lit("a call of an absurd function (D4)"));
    case ET_CNO:
      return erase_typed_no_plan(heap, cls.why);
    case ET_CDATA:
    case ET_CBOOL:
    case ET_CPROP:
    case ET_CTYPE:
    case ET_CSKIP:
      return erase_typed_no_plan(heap, str_lit("a call of a name that is not a function"));
  }
  return erase_typed_no_plan(heap, str_lit("a call of a name that is not a function"));
}

/* natAdd is the primitive addition: its arguments are borrowed. */
static EtPlan erase_typed_call_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                    Str name, const Vec *args) {
  if (str_eq(name, str_lit("natAdd"))) {
    EtBuild b = erase_typed_build(heap, ET_BUILD_CALL, want);
    b.func = str_lit("nat_add");
    return erase_typed_plan(erase_typed_ref_jobs(heap, ks, vs, args), b);
  }
  Globals globals = erase_typed_globals_of(env);
  return erase_typed_fn_plan(heap, env, ks, vs, want, erase_typed_name_class(heap, env, name), global_find(name, globals),
                             args);
}

/* A call of a local variable: only a slot of type `&dyn Fn(..)` (TRef{TFn}) is callable. */
static EtPlan erase_typed_local_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                     const RirParam *slot, EtKnd k, const Vec *args) {
  if (slot != NULL && slot->ty != NULL && slot->ty->kind == RIR_TREF && slot->ty->inner != NULL &&
      slot->ty->inner->kind == RIR_TFN) {
    Vec bds = erase_typed_head_bds(heap, k);
    return erase_typed_local_call(heap, ks, vs, want, slot->name, &slot->ty->inner->args,
                                  erase_typed_kept_args(heap, env, ks, &bds, args));
  }
  return erase_typed_no_plan(heap, str_lit("a call of a variable that is not a function"));
}

static EtPlan erase_typed_app_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                   EtSpine sp) {
  const Term *head = sp.head;
  if (head == NULL) return erase_typed_no_plan(heap, str_lit("an application head outside the M0 fragment"));
  switch (head->kind) {
    case TERM_GLOBAL:
      return erase_typed_call_plan(heap, env, ks, vs, want, head->name, &sp.args);
    case TERM_VAR:
      return erase_typed_local_plan(heap, env, ks, vs, want, erase_typed_slot_at(vs, head->index),
                                    erase_typed_kind_at(heap, ks, head->index), &sp.args);
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_IN:
    case TERM_ELIM:
    case TERM_SEC:
    case TERM_OUT:
    case TERM_LET:
    case TERM_ANN:
    case TERM_LIT:
    case TERM_AUTO:
      return erase_typed_no_plan(heap, str_lit("an application head outside the M0 fragment"));
  }
  return erase_typed_no_plan(heap, str_lit("an application head outside the M0 fragment"));
}

/* ---- Lambdas: nested lambdas give one closure ---- */

/* slot_of: the parameter of a kept binder (NULL = None for an erased binder). */
static const RirParam *erase_typed_slot_of(Heap *heap, const Vec *copies, Quantity q, Str name, EtBd bd) {
  switch (bd.kind) {
    case ET_BPARAM:
      return rir_param(heap, erase_typed_fn_name(heap, name), erase_typed_mode(heap, copies, q, bd.ty));
    case ET_BGEN:
    case ET_BDROP:
    case ET_BNO:
      return NULL;
  }
  return NULL;
}

/* push_slot: Bend prepends and reverses once at the end of lams; C appends in binder order. */
static void erase_typed_push_slot(Heap *heap, const RirParam *slot, Vec *params) {
  if (slot != NULL) erase_typed_push(heap, params, slot);
}

/* Lam{ks, vs, params, body}: params = RirParam pointers in binder order. */
typedef struct {
  const EtCtx *ks;
  const EtSlots *vs;
  Vec params;
  const Term *body;
} EtLam;

/* A closure that is an argument takes the reference mode of each declared argument of the
 * parameter: a generic argument is a borrow also when the type of the binder is Copy. `decl` is
 * empty for each other closure. A TRef is kept as the same pointer (Bend rebuilds an equal value). */
static const RirTy *erase_typed_borrowed(Heap *heap, const RirTy *t) {
  if (t != NULL && t->kind == RIR_TREF) return t;
  return rir_tref(heap, t);
}

static const RirTy *erase_typed_as_decl(Heap *heap, const RirTy *d, const RirTy *ty) {
  if (d != NULL && d->kind == RIR_TREF) return erase_typed_borrowed(heap, ty);
  return ty;
}

/* decl_ty: `decl` is the RirTy pointer list from index `at` on (past the end = Nil). */
static const RirTy *erase_typed_decl_ty(Heap *heap, const Vec *decl, size_t at, const RirTy *ty) {
  VecGet g = vec_get(decl, at);
  if (g.status == VEC_GET_OK) return erase_typed_as_decl(heap, g.value, ty);
  return ty;
}

static const RirParam *erase_typed_decl_slot(Heap *heap, const RirParam *slot, const Vec *decl, size_t at) {
  if (slot == NULL) return NULL;
  return rir_param(heap, slot->name, erase_typed_decl_ty(heap, decl, at, slot->ty));
}

/* decl_tail: one index step (the tail of Nil is Nil). */
static size_t erase_typed_decl_tail(const Vec *decl, size_t at) { return at < decl->len ? at + 1 : at; }

/* An erased binder has no declared argument. */
static size_t erase_typed_decl_rest(const RirParam *slot, const Vec *decl, size_t at) {
  if (slot == NULL) return at;
  return erase_typed_decl_tail(decl, at);
}

static EtLam erase_typed_lams(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, const Vec *decl,
                              const Term *t) {
  EtLam lam;
  lam.params = vec_new(heap->arena);
  const EtCtx *k = ks;
  const EtSlots *v = vs;
  const Term *u = t;
  size_t at = 0;
  while (u != NULL && u->kind == TERM_SEC && u->shape.kind == SHAPE_PI && u->shape.domain != NULL) {
    VecGet g = vec_get(&u->legs, 0);
    if (g.status != VEC_GET_OK || g.value == NULL) break;
    const Term *dom = u->shape.domain;
    Quantity q = u->shape.quantity;
    Str x = u->shape.name;
    EtEnd e = erase_typed_end_of(dom);
    const RirParam *own = erase_typed_slot_of(heap, erase_typed_copies_of(env), q, x,
                                              erase_typed_binder_er(q, e, erase_typed_er(heap, env, k, dom)));
    const RirParam *slot = erase_typed_decl_slot(heap, own, decl, at);
    const EtCtx *next = erase_typed_cons(heap, erase_typed_bind_kind(heap, env, k, e, x, dom), k);
    v = erase_typed_slot_cons(heap, slot, v);
    erase_typed_push_slot(heap, slot, &lam.params);
    at = erase_typed_decl_rest(own, decl, at);
    k = next;
    u = ((const Leg *)g.value)->body;
  }
  lam.ks = k;
  lam.vs = v;
  lam.body = u;
  return lam;
}

static EtPlan erase_typed_lam_done(Heap *heap, EtWant want, EtLam lam) {
  Vec jobs = vec_new(heap->arena);
  erase_typed_push(heap, &jobs,
                   erase_typed_job(heap, lam.ks, lam.vs, erase_typed_want(ET_WOWN, vec_new(heap->arena)), lam.body));
  EtBuild b = erase_typed_build(heap, ET_BUILD_CLOSURE, want);
  b.params = lam.params;
  return erase_typed_plan(jobs, b);
}

static EtPlan erase_typed_lam_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                   const Term *t) {
  Vec none = vec_new(heap->arena);
  switch (want.kind) {
    case ET_WOWN:
      return erase_typed_lam_done(heap, erase_typed_want(ET_WOWN, vec_new(heap->arena)),
                                  erase_typed_lams(heap, env, ks, vs, &none, t));
    case ET_WREF:
      return erase_typed_lam_done(heap, erase_typed_want(ET_WREF, vec_new(heap->arena)),
                                  erase_typed_lams(heap, env, ks, vs, &none, t));
    case ET_WFN:
      return erase_typed_lam_done(heap, erase_typed_want(ET_WREF, vec_new(heap->arena)),
                                  erase_typed_lams(heap, env, ks, vs, &want.args, t));
  }
  return erase_typed_lam_done(heap, erase_typed_want(ET_WOWN, vec_new(heap->arena)),
                              erase_typed_lams(heap, env, ks, vs, &none, t));
}

/* ---- part d (Bend line 1503 on) ---- */

/* An owned want with no arguments (Bend WOwn{}). */
static EtWant erase_typed_wown(Heap *heap) { return erase_typed_want(ET_WOWN, vec_new(heap->arena)); }

/* The single-job plan Plan{[job], build}. */
static EtPlan erase_typed_single(Heap *heap, const EtJob *job, EtBuild build) {
  Vec jobs = vec_new(heap->arena);
  erase_typed_push(heap, &jobs, job);
  return erase_typed_plan(jobs, build);
}

/* Scope{ks, vs, binds}: binds = RirBind pointers in binder order (Bend prepends, then `br`
 * reverses; C appends and does not reverse). */
typedef struct {
  const EtCtx *ks;
  const EtSlots *vs;
  Vec binds;
} EtScope;

static void erase_typed_bind_some(Heap *heap, Str name, const RirTy *ty, EtScope *sc) {
  sc->ks = erase_typed_cons(heap, erase_typed_knd(ET_KVAL, str_empty(), vec_new(heap->arena)), sc->ks);
  sc->vs = erase_typed_slot_cons(heap, rir_param(heap, name, rir_tref(heap, ty)), sc->vs);
  erase_typed_push(heap, &sc->binds, rir_bvar(heap, name));
}

static void erase_typed_bind_none(Heap *heap, EtScope *sc) {
  sc->ks = erase_typed_cons(heap, erase_typed_knd(ET_KPROP, str_empty(), vec_new(heap->arena)), sc->ks);
  sc->vs = erase_typed_slot_cons(heap, NULL, sc->vs);
}

/* bind_step: t NULL = None (an erased field). */
static void erase_typed_bind_step(Heap *heap, Str x, const RirTy *t, EtScope *sc) {
  if (t == NULL) {
    erase_typed_bind_none(heap, sc);
  } else {
    erase_typed_bind_some(heap, erase_typed_fn_name(heap, x), t, sc);
  }
}

/* scoped: binders = Binder pointers, tys = RirTy pointers (NULL = None); a binder past the end of
 * tys is bound as None (Bend: the Nil arm recurses with []). */
static EtScope erase_typed_scoped(Heap *heap, const Vec *binders, const Vec *tys, EtScope sc) {
  EtScope out = sc;
  for (size_t i = 0; i < binders->len; i++) {
    const Binder *b = binders->items[i];
    Str x = b != NULL ? b->name : str_empty();
    VecGet t = vec_get(tys, i);
    switch (t.status) {
      case VEC_GET_OK:
        erase_typed_bind_step(heap, x, t.value, &out);
        break;
      case VEC_GET_OUT_OF_RANGE:
        erase_typed_bind_none(heap, &out);
        break;
    }
  }
  return out;
}

/* Br{pat, job}. */
typedef struct {
  RirPat pat;
  const EtJob *job;
} EtBr;

static const RirPat erase_typed_pat_placeholder = {0};
static const EtBr erase_typed_br_placeholder = {.job = &erase_typed_job_placeholder};

static const RirPat *erase_typed_pat_box(Heap *heap, RirPat p) {
  RirPat *box = heap_alloc(heap, sizeof(RirPat));
  if (box == NULL) return &erase_typed_pat_placeholder;
  *box = p;
  return box;
}

static const EtBr *erase_typed_br_box(Heap *heap, RirPat pat, const EtJob *job) {
  EtBr *box = heap_alloc(heap, sizeof(EtBr));
  if (box == NULL) return &erase_typed_br_placeholder;
  box->pat = pat;
  box->job = job;
  return box;
}

static const EtBr *erase_typed_br(Heap *heap, Str fam, Str c, const Term *body, EtScope sc) {
  RirPat pat = rir_pctor(fam, erase_typed_upper_first(heap, c), sc.binds);
  return erase_typed_br_box(heap, pat, erase_typed_job(heap, sc.ks, sc.vs, erase_typed_wown(heap), body));
}

/* The `case other` arm of br_of: Br{PCtor{fam, "?", []}, Job{ks, vs, WOwn{}, Auto{}}}. */
static const EtBr *erase_typed_br_unknown(Heap *heap, const EtCtx *ks, const EtSlots *vs, Str fam) {
  RirPat unknown = rir_pctor(fam, str_lit("?"), vec_new(heap->arena));
  return erase_typed_br_box(heap, unknown,
                            erase_typed_job(heap, ks, vs, erase_typed_wown(heap), &erase_typed_auto_term));
}

static const EtBr *erase_typed_br_of(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, Str fam,
                                     MaybeFamily family, const Branch *b) {
  if (b == NULL) return erase_typed_br_unknown(heap, ks, vs, fam);
  switch (b->addr.kind) {
    case ADDR_CTOR: {
      Str c = b->addr.name;
      EtScope start;
      start.ks = ks;
      start.vs = vs;
      start.binds = vec_new(heap->arena);
      Vec tys = erase_typed_ctor_tys(heap, env, family, c);
      return erase_typed_br(heap, fam, c, b->leg.body, erase_typed_scoped(heap, &b->leg.binders, &tys, start));
    }
    case ADDR_PT:
    case ADDR_LEG:
      return erase_typed_br_unknown(heap, ks, vs, fam);
  }
  return erase_typed_br_unknown(heap, ks, vs, fam);
}

/* brs: one Br per branch, in branch order (EtBr pointers). */
static Vec erase_typed_brs(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, Str fam,
                           MaybeFamily family, const Vec *branches) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < branches->len; i++)
    erase_typed_push(heap, &out, erase_typed_br_of(heap, env, ks, vs, fam, family, branches->items[i]));
  return out;
}

static Vec erase_typed_br_jobs(Heap *heap, const Vec *bs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < bs->len; i++) {
    const EtBr *b = bs->items[i];
    erase_typed_push(heap, &out, b != NULL ? b->job : &erase_typed_job_placeholder);
  }
  return out;
}

static Vec erase_typed_br_pats(Heap *heap, const Vec *bs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < bs->len; i++) {
    const EtBr *b = bs->items[i];
    erase_typed_push(heap, &out, erase_typed_pat_box(heap, b != NULL ? b->pat : erase_typed_pat_placeholder));
  }
  return out;
}

/* nones_k: one KProp prepended for each binder. */
static const EtCtx *erase_typed_nones_k(Heap *heap, const Vec *binders, const EtCtx *ks) {
  const EtCtx *k = ks;
  for (size_t i = 0; i < binders->len; i++)
    k = erase_typed_cons(heap, erase_typed_knd(ET_KPROP, str_empty(), vec_new(heap->arena)), k);
  return k;
}

/* nones_v: one None prepended for each binder. */
static const EtSlots *erase_typed_nones_v(Heap *heap, const Vec *binders, const EtSlots *vs) {
  const EtSlots *v = vs;
  for (size_t i = 0; i < binders->len; i++) v = erase_typed_slot_cons(heap, NULL, v);
  return v;
}

/* R6: a case on a proof with one branch is the body of the branch. */
static EtPlan erase_typed_prop_plan(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, const Vec *branches) {
  VecGet g = vec_get(branches, 0);
  switch (g.status) {
    case VEC_GET_OUT_OF_RANGE:
      return erase_typed_no_plan(heap, str_lit("a case with no branch (D4)"));
    case VEC_GET_OK:
      break;
  }
  const Branch *b = g.value;
  if (b == NULL) return erase_typed_no_plan(heap, str_lit("a case with no branch (D4)"));
  const EtJob *job = erase_typed_job(heap, erase_typed_nones_k(heap, &b->leg.binders, ks),
                                     erase_typed_nones_v(heap, &b->leg.binders, vs), want, b->leg.body);
  return erase_typed_single(heap, job, erase_typed_build(heap, ET_BUILD_SAME, erase_typed_wown(heap)));
}

/* leg_jobs: an owned job for the body of each branch, in branch order, pushed onto `out`. */
static void erase_typed_leg_jobs(Heap *heap, const EtCtx *ks, const EtSlots *vs, const Vec *branches, Vec *out) {
  for (size_t i = 0; i < branches->len; i++) {
    const Branch *b = branches->items[i];
    const Term *body = b != NULL ? b->leg.body : &erase_typed_auto_term;
    erase_typed_push(heap, out, erase_typed_job(heap, ks, vs, erase_typed_wown(heap), body));
  }
}

/* first_is_true: the first branch is the `mechTrue` constructor; True when the first branch is not
 * a constructor branch or there is no branch. */
static bool erase_typed_first_is_true(const Vec *branches) {
  VecGet g = vec_get(branches, 0);
  switch (g.status) {
    case VEC_GET_OUT_OF_RANGE:
      return true;
    case VEC_GET_OK:
      break;
  }
  const Branch *b = g.value;
  if (b == NULL) return true;
  switch (b->addr.kind) {
    case ADDR_CTOR:
      return str_eq(b->addr.name, str_lit("mechTrue"));
    case ADDR_PT:
    case ADDR_LEG:
      return true;
  }
  return true;
}

static EtPlan erase_typed_bool_plan(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, const Term *scrut,
                                    const Vec *branches) {
  Vec jobs = vec_new(heap->arena);
  erase_typed_push(heap, &jobs, erase_typed_job(heap, ks, vs, erase_typed_wown(heap), scrut));
  erase_typed_leg_jobs(heap, ks, vs, branches, &jobs);
  EtBuild b = erase_typed_build(heap, ET_BUILD_IF, want);
  b.true_first = erase_typed_first_is_true(branches);
  return erase_typed_plan(jobs, b);
}

static EtPlan erase_typed_match_plan(Heap *heap, const EtCtx *ks, const EtSlots *vs, EtWant want, bool by_value,
                                     const Term *scrut, const Vec *bs) {
  EtWant scrut_want = by_value ? erase_typed_wown(heap) : erase_typed_want(ET_WREF, vec_new(heap->arena));
  Vec head = vec_new(heap->arena);
  erase_typed_push(heap, &head, erase_typed_job(heap, ks, vs, scrut_want, scrut));
  Vec rest = erase_typed_br_jobs(heap, bs);
  Vec jobs = erase_typed_append(heap, &head, &rest);
  EtBuild b = erase_typed_build(heap, ET_BUILD_MATCH, want);
  b.pats = erase_typed_br_pats(heap, bs);
  return erase_typed_plan(jobs, b);
}

static EtPlan erase_typed_elim_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                    Str fam, const Term *scrut, const Vec *branches, EtClass cls) {
  switch (cls.kind) {
    case ET_CPROP:
      return erase_typed_prop_plan(heap, ks, vs, want, branches);
    case ET_CBOOL:
      return erase_typed_bool_plan(heap, ks, vs, want, scrut, branches);
    case ET_CDATA: {
      bool by_value = erase_typed_has(erase_typed_copies_of(env), fam);
      MaybeFamily family = global_find_family(fam, erase_typed_globals_of(env));
      Vec bs = erase_typed_brs(heap, env, ks, vs, fam, family, branches);
      return erase_typed_match_plan(heap, ks, vs, want, by_value, scrut, &bs);
    }
    case ET_CFN:
    case ET_CTYPE:
    case ET_CABSURD:
    case ET_CNO:
    case ET_CSKIP:
      return erase_typed_no_plan(heap, str_lit("a case on a family outside the M0 fragment"));
  }
  return erase_typed_no_plan(heap, str_lit("a case on a family outside the M0 fragment"));
}

static EtRx erase_typed_lit_rx(Heap *heap, EtWant want, Literal lit) {
  switch (lit.kind) {
    case LITERAL_INT: {
      if (!big_le(lit.integer, big_of_u32(heap, 4294967295u)))
        return erase_typed_xno(str_lit("a literal above u32 (M0)"));
      Vec args = vec_new(heap->arena);
      erase_typed_push(heap, &args, rir_xint(heap, pp_literal(heap, literal_lint(lit.integer))));
      return erase_typed_wanted(heap, want, rir_xcall(heap, str_lit("nat_small"), vec_new(heap->arena), args));
    }
    case LITERAL_STRING:
      return erase_typed_xno(str_lit("a literal that is not an integer (M0)"));
  }
  return erase_typed_xno(str_lit("a literal that is not an integer (M0)"));
}

/* A let (E1). The binder enters `vs` as an owned `Param{rn, t}`: `var_use` moves it on an owned
 * use and borrows it on a `WRef` use. Two owned uses of a non-Copy binder do not compile (rustc
 * E0382), so E1 accepts a binder of a Copy type only; a non-Copy, function, type or Prop-sorted
 * binder is a located refusal. A proof binder (an `EProp` type) is erased like a proof parameter.
 * ED2: the Rust name of the binder must not collide with a slot of `vs` or with the Rust name of a
 * global after the name map. */
Vec erase_typed_let_tys(Heap *heap, const RirTy *t) {
  Vec out = vec_new(heap->arena);
  if (t != NULL) erase_typed_push(heap, &out, t);
  return out;
}

static bool erase_typed_has_slot(const EtSlots *vs, Str name) {
  for (const EtSlots *at = vs; at != NULL; at = at->rest)
    if (at->head != NULL && str_eq(at->head->name, name)) return true;
  return false;
}

static bool erase_typed_global_clash(Heap *heap, const EntryRows *entries, Str rn) {
  for (const EntryRows *at = entries; at != NULL; at = at->rest)
    if (str_eq(erase_typed_fn_name(heap, at->name), rn)) return true;
  return false;
}

static const EntryRows *erase_typed_entries_of(Globals g) { return g.entries; }

static bool erase_typed_let_clash(Heap *heap, const EtEnv *env, const EtSlots *vs, Str rn) {
  return erase_typed_has_slot(vs, rn) ||
         erase_typed_global_clash(heap, erase_typed_entries_of(erase_typed_globals_of(env)), rn);
}

static bool erase_typed_let_copy(const Vec *copies, const RirTy *t) {
  if (t == NULL) return false;
  switch (t->kind) {
    case RIR_TBOOL:
      return true;
    case RIR_TCON:
      return erase_typed_has(copies, t->name);
    case RIR_TBOX:
    case RIR_TREF:
    case RIR_TFN:
      return false;
  }
  return false;
}

static EtPlan erase_typed_let_typed(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                    Str rn, const RirTy *t, const Term *value, const Term *body) {
  if (t != NULL && t->kind == RIR_TFN) return erase_typed_no_plan(heap, str_lit("a let of a function type (E1)"));
  if (!erase_typed_let_copy(erase_typed_copies_of(env), t))
    return erase_typed_no_plan(heap, str_lit("a let of a non-Copy type (E1)"));
  Vec jobs = vec_new(heap->arena);
  erase_typed_push(heap, &jobs, erase_typed_job(heap, ks, vs, erase_typed_wown(heap), value));
  const EtCtx *ks2 = erase_typed_cons(heap, erase_typed_knd(ET_KVAL, str_empty(), vec_new(heap->arena)), ks);
  const EtSlots *vs2 = erase_typed_slot_cons(heap, rir_param(heap, rn, t), vs);
  erase_typed_push(heap, &jobs, erase_typed_job(heap, ks2, vs2, erase_typed_wown(heap), body));
  EtBuild b = erase_typed_build(heap, ET_BUILD_LET, want);
  b.name = rn;
  b.ty = t;
  return erase_typed_plan(jobs, b);
}

static EtPlan erase_typed_let_er(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want, Str rn,
                                 EtEr er, const Term *value, const Term *body) {
  switch (er.kind) {
    case ET_EDATA:
      return erase_typed_let_typed(heap, env, ks, vs, want, rn, er.ty, value, body);
    case ET_EPROP: {
      const EtCtx *ks2 = erase_typed_cons(heap, erase_typed_knd(ET_KPROP, str_empty(), vec_new(heap->arena)), ks);
      const EtJob *job = erase_typed_job(heap, ks2, erase_typed_slot_cons(heap, NULL, vs), want, body);
      return erase_typed_single(heap, job, erase_typed_build(heap, ET_BUILD_SAME, erase_typed_wown(heap)));
    }
    case ET_ENO:
      return erase_typed_no_plan(heap, er.why);
  }
  return erase_typed_no_plan(heap, er.why);
}

/* The KVal and KLam arms of let_kind (the same in Bend). */
static EtPlan erase_typed_let_value(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                    Str rn, const Term *ty, const Term *value, const Term *body) {
  if (erase_typed_let_clash(heap, env, vs, rn)) {
    Str parts[3] = {str_lit("the let binder `"), rn,
                    str_lit("` collides with a name in scope after the name map (E1)")};
    return erase_typed_no_plan(heap, erase_typed_cat(heap, parts, 3));
  }
  Vec tys = vec_new(heap->arena);
  erase_typed_push(heap, &tys, ty);
  Vec es = erase_typed_ers(heap, env, ks, &tys);
  return erase_typed_let_er(heap, env, ks, vs, want, rn, erase_typed_one_er(&es), value, body);
}

static EtPlan erase_typed_let_kind(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                   Str rn, EtKnd k, const Term *ty, const Term *value, const Term *body) {
  switch (k.kind) {
    case ET_KGEN:
    case ET_KPROP:
      return erase_typed_no_plan(heap, str_lit("a let of a type (E1)"));
    case ET_KVAL:
    case ET_KLAM:
      return erase_typed_let_value(heap, env, ks, vs, want, rn, ty, value, body);
  }
  return erase_typed_no_plan(heap, str_lit("a let of a type (E1)"));
}

static EtPlan erase_typed_let_plan(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                   Str name, const Term *ty, const Term *value, const Term *body) {
  return erase_typed_let_kind(heap, env, ks, vs, want, erase_typed_fn_name(heap, name),
                              erase_typed_kind_of(heap, erase_typed_end_of(ty), name), ty, value, body);
}

/* The plan of one term: its top node or its application spine only (not recursive: the nested
 * terms are the jobs of the plan; run_jobs in part d2 recurses through them). */
static EtPlan erase_typed_plan_term(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, EtWant want,
                                    const Term *term) {
  Str outside = str_lit("a term shape outside the M0 fragment");
  if (term == NULL) return erase_typed_no_plan(heap, outside);
  switch (term->kind) {
    case TERM_VAR:
      return erase_typed_leaf(heap, erase_typed_var_rx(heap, erase_typed_slot_at(vs, term->index), want));
    case TERM_IN:
      if (term->shape.kind != SHAPE_MU || term->addr.kind != ADDR_CTOR) return erase_typed_no_plan(heap, outside);
      return erase_typed_ctor_plan(heap, env, ks, vs, want, term->shape.name, term->addr.name, &term->args,
                                   erase_typed_name_class(heap, env, term->shape.name));
    case TERM_ELIM: {
      const ElimData *d = term->data;
      if (d == NULL || d->shape.kind != SHAPE_MU) return erase_typed_no_plan(heap, outside);
      return erase_typed_elim_plan(heap, env, ks, vs, want, d->shape.name, d->scrut, &d->branches,
                                   erase_typed_name_class(heap, env, d->shape.name));
    }
    case TERM_OUT: {
      if (term->addr.kind != ADDR_PT) return erase_typed_no_plan(heap, outside);
      if (term->shape.kind != SHAPE_PI && term->shape.kind != SHAPE_ARG) return erase_typed_no_plan(heap, outside);
      const Term *dom = term->shape.kind == SHAPE_PI ? (const Term *)term->shape.domain : NULL;
      Vec acc = vec_new(heap->arena);
      erase_typed_push(heap, &acc, erase_typed_sarg(heap, term->shape.quantity, dom, term->addr.arg));
      return erase_typed_app_plan(heap, env, ks, vs, want, erase_typed_spine(heap, term->head, &acc));
    }
    case TERM_GLOBAL: {
      Vec none = vec_new(heap->arena);
      return erase_typed_call_plan(heap, env, ks, vs, want, term->name, &none);
    }
    case TERM_SEC:
      if (term->shape.kind != SHAPE_PI) return erase_typed_no_plan(heap, outside);
      return erase_typed_lam_plan(heap, env, ks, vs, want, term);
    case TERM_ANN:
      return erase_typed_single(heap, erase_typed_job(heap, ks, vs, want, term->body),
                                erase_typed_build(heap, ET_BUILD_SAME, erase_typed_wown(heap)));
    case TERM_LIT:
      return erase_typed_leaf(heap, erase_typed_lit_rx(heap, want, term->lit));
    case TERM_LET:
      return erase_typed_let_plan(heap, env, ks, vs, want, term->name, term->ty, term->value, term->body);
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_AUTO:
      return erase_typed_no_plan(heap, outside);
  }
  return erase_typed_no_plan(heap, outside);
}

static EtPlan erase_typed_plan_of(Heap *heap, const EtEnv *env, const EtJob *job) {
  return erase_typed_plan_term(heap, env, job->ks, job->vs, job->want, job->term);
}

/* ---- part d2 (Bend line 1751 on) ---- */

/* Result<String, List<R.Ex>>: Done{xs} (ok, xs = RirEx pointers) or Fail{why}. */
typedef struct {
  bool ok;
  Vec xs;
  Str why;
} EtExs;

/* push_ex as a left to right step: the first XNo wins and later results are dropped, which is
 * the result of the Bend right fold `push_ex(r, oks(rest))`. */
static void erase_typed_push_ex(Heap *heap, EtRx r, EtExs *acc) {
  if (!acc->ok) return;
  switch (r.kind) {
    case ET_XNO:
      acc->ok = false;
      acc->why = r.why;
      return;
    case ET_XOK:
      erase_typed_push(heap, &acc->xs, r.ex);
      return;
  }
}

/* oks: the expressions of the results (EtRx pointers), or the reason of the first XNo. */
static EtExs erase_typed_oks(Heap *heap, const Vec *rs) {
  EtExs acc = {true, vec_new(heap->arena), {0}};
  for (size_t i = 0; i < rs->len; i++) {
    const EtRx *r = rs->items[i];
    if (r != NULL) erase_typed_push_ex(heap, *r, &acc);
  }
  return acc;
}

static bool erase_typed_uses(Str name, const RirEx *x);

/* uses(name, xs) over a Vec of RirEx pointers. */
static bool erase_typed_uses_all(Str name, const Vec *xs) {
  for (size_t i = 0; i < xs->len; i++) {
    if (erase_typed_uses(name, xs->items[i])) return true;
  }
  return false;
}

/* arm_bodies has no C counterpart: the XMatch case walks the arm bodies in place. */
static bool erase_typed_uses_arms(Str name, const Vec *arms) {
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = arms->items[i];
    if (a != NULL && erase_typed_uses(name, a->body)) return true;
  }
  return false;
}

/* A use of the name in the expression (a shadow counts as a use): uses(name, [x]). */
static bool erase_typed_uses(Str name, const RirEx *x) {
  if (x == NULL) return false;
  switch (x->kind) {
    case RIR_XVAR:
      return str_eq(x->name, name);
    case RIR_XINT:
      return false;
    case RIR_XBOOL:
      return false;
    case RIR_XCALL:
      return str_eq(x->func, name) || erase_typed_uses_all(name, &x->args);
    case RIR_XCTOR:
      return erase_typed_uses_all(name, &x->args);
    case RIR_XMATCH:
      return erase_typed_uses(name, x->scrut) || erase_typed_uses_arms(name, &x->arms);
    case RIR_XIF:
      return erase_typed_uses(name, x->cond) || erase_typed_uses(name, x->yes) || erase_typed_uses(name, x->no);
    case RIR_XCLOSURE:
      return erase_typed_uses(name, x->body);
    case RIR_XBORROW:
      return erase_typed_uses(name, x->inner);
    case RIR_XCLONE:
      return erase_typed_uses(name, x->inner);
    case RIR_XBOX:
      return erase_typed_uses(name, x->inner);
    case RIR_XLET:
      return erase_typed_uses(name, x->value) || erase_typed_uses(name, x->body);
  }
  return false;
}

/* R2: a parameter with no use in the body gets `_` in front of its name. */
static Vec erase_typed_under(Heap *heap, const Vec *params, const RirEx *body) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < params->len; i++) {
    const RirParam *p = params->items[i];
    if (p == NULL) {
      erase_typed_push(heap, &out, p);
    } else {
      Str name = erase_typed_uses(p->name, body) ? p->name : erase_typed_cat2(heap, str_lit("_"), p->name);
      erase_typed_push(heap, &out, rir_param(heap, name, p->ty));
    }
  }
  return out;
}

/* A kept BVar and a BSkip are returned as the same pointer (same value as the Bend rebuild). */
static const RirBind *erase_typed_skip_bind(Heap *heap, const RirBind *b, const RirEx *body) {
  if (b == NULL) return b;
  switch (b->kind) {
    case RIR_BVAR:
      if (erase_typed_uses(b->name, body)) return b;
      return rir_bskip(heap);
    case RIR_BSKIP:
      return b;
  }
  return b;
}

static Vec erase_typed_skip_binds(Heap *heap, const Vec *binds, const RirEx *body) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < binds->len; i++) {
    erase_typed_push(heap, &out, erase_typed_skip_bind(heap, binds->items[i], body));
  }
  return out;
}

static RirPat erase_typed_skip_pat(Heap *heap, const RirPat *p, const RirEx *body) {
  return rir_pctor(p->family, p->ctor, erase_typed_skip_binds(heap, &p->binds, body));
}

/* zip_arms(pats, bodies) with bodies = the RirEx pointers of xs from index `from` on; the
 * shorter list ends the zip. */
static Vec erase_typed_zip_arms(Heap *heap, const Vec *pats, const Vec *xs, size_t from) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < pats->len && from + i < xs->len; i++) {
    const RirPat *p = pats->items[i];
    const RirEx *b = xs->items[from + i];
    const RirPat *pat = p != NULL ? p : &erase_typed_pat_placeholder;
    erase_typed_push(heap, &out, rir_arm(heap, erase_typed_skip_pat(heap, pat, b), b));
  }
  return out;
}

/* R3: an arm with no field that builds its own constructor again. */
static bool erase_typed_same_arm(Str ctor, const Vec *binds, const RirEx *body) {
  if (binds->len != 0) return false;
  if (body == NULL || body->kind != RIR_XCTOR) return false;
  return str_eq(body->ctor, ctor);
}

static bool erase_typed_all_same(const Vec *arms) {
  for (size_t i = 0; i < arms->len; i++) {
    const RirArm *a = arms->items[i];
    if (a == NULL || !erase_typed_same_arm(a->pat.ctor, &a->pat.binds, a->body)) return false;
  }
  return true;
}

static bool erase_typed_any_arm(const Vec *arms) { return arms->len != 0; }

static const RirEx *erase_typed_match_ex(Heap *heap, const RirEx *scrut, Vec arms) {
  if (erase_typed_any_arm(&arms) && erase_typed_all_same(&arms)) return scrut;
  return rir_xmatch(heap, scrut, arms);
}

static EtRx erase_typed_match_rx(Heap *heap, EtWant want, const Vec *pats, const Vec *xs) {
  if (xs->len == 0) return erase_typed_xno(str_lit("internal: a match with no scrutinee"));
  const RirEx *scrut = xs->items[0];
  return erase_typed_wanted(heap, want, erase_typed_match_ex(heap, scrut, erase_typed_zip_arms(heap, pats, xs, 1)));
}

static bool erase_typed_is_true(const RirEx *x) {
  if (x == NULL || x->kind != RIR_XBOOL) return false;
  return x->boolean;
}

static bool erase_typed_is_false(const RirEx *x) {
  if (x == NULL || x->kind != RIR_XBOOL) return false;
  return !x->boolean;
}

/* R4: `if c { true } else { false }` is `c`. */
static const RirEx *erase_typed_if_ex(Heap *heap, const RirEx *c, const RirEx *yes, const RirEx *no) {
  if (erase_typed_is_true(yes) && erase_typed_is_false(no)) return c;
  return rir_xif(heap, c, yes, no);
}

static EtRx erase_typed_if_rx(Heap *heap, bool true_first, EtWant want, const Vec *xs) {
  if (xs->len < 3) return erase_typed_xno(str_lit("internal: an if without two arms"));
  const RirEx *c = xs->items[0];
  const RirEx *a = xs->items[1];
  const RirEx *b = xs->items[2];
  return erase_typed_wanted(heap, want, erase_typed_if_ex(heap, c, true_first ? a : b, true_first ? b : a));
}

static EtRx erase_typed_let_rx(Heap *heap, Str name, const RirTy *ty, EtWant want, const Vec *xs) {
  if (xs->len < 2) return erase_typed_xno(str_lit("internal: a let without two parts"));
  const RirEx *v = xs->items[0];
  const RirEx *b = xs->items[1];
  return erase_typed_wanted(heap, want, rir_xlet(heap, name, rir_some_ty(ty), v, b));
}

static EtRx erase_typed_closure_rx(Heap *heap, EtWant want, const Vec *params, const Vec *xs) {
  if (xs->len == 0) return erase_typed_xno(str_lit("internal: a closure with no body"));
  const RirEx *body = xs->items[0];
  return erase_typed_wanted(heap, want, rir_xclosure(heap, erase_typed_under(heap, params, body), body));
}

/* box_args: XBox{x} where the flag is true; the expressions past the last flag are kept as they
 * are, and the flags past the last expression give nothing. */
static Vec erase_typed_box_args(Heap *heap, const Vec *flags, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < flags->len && i < xs->len; i++) {
    const bool *flag = flags->items[i];
    const RirEx *x = xs->items[i];
    erase_typed_push(heap, &out, flag != NULL && *flag ? rir_xbox(heap, x) : x);
  }
  for (; i < xs->len; i++) erase_typed_push(heap, &out, xs->items[i]);
  return out;
}

static EtRx erase_typed_same_rx(const Vec *xs) {
  if (xs->len == 0) return erase_typed_xno(str_lit("internal: no result"));
  return erase_typed_xok(xs->items[0]);
}

static EtRx erase_typed_built(Heap *heap, EtExs r, const EtBuild *b) {
  if (!r.ok) return erase_typed_xno(r.why);
  switch (b->kind) {
    case ET_BUILD_LEAF:
      return b->rx;
    case ET_BUILD_SAME:
      return erase_typed_same_rx(&r.xs);
    case ET_BUILD_CALL:
      return erase_typed_wanted(heap, b->want, rir_xcall(heap, b->func, b->targs, r.xs));
    case ET_BUILD_CTOR:
      return erase_typed_wanted(heap, b->want, rir_xctor(heap, b->family, b->ctor, erase_typed_box_args(heap, &b->boxes, &r.xs)));
    case ET_BUILD_MATCH:
      return erase_typed_match_rx(heap, b->want, &b->pats, &r.xs);
    case ET_BUILD_IF:
      return erase_typed_if_rx(heap, b->true_first, b->want, &r.xs);
    case ET_BUILD_CLOSURE:
      return erase_typed_closure_rx(heap, b->want, &b->params, &r.xs);
    case ET_BUILD_LET:
      return erase_typed_let_rx(heap, b->name, b->ty, b->want, &r.xs);
  }
  return erase_typed_xno(str_lit("internal: no result"));
}

static Vec erase_typed_jobs_of(const EtPlan *p) { return p->jobs; }

static const EtBuild *erase_typed_build_of(const EtPlan *p) { return &p->build; }

static const EtRx erase_typed_rx_placeholder = {ET_XNO, NULL, {0}};

static const EtRx *erase_typed_rx_box(Heap *heap, EtRx r) {
  EtRx *box = heap_alloc(heap, sizeof(EtRx));
  if (box == NULL) return &erase_typed_rx_placeholder;
  *box = r;
  return box;
}

/* The one walker: a job list gives one result (EtRx pointer) for each job. D-194 (i): the
 * recursion is one level per plan nesting level (on the 1 GiB stack of the rt main wrapper); the
 * walk over the sibling jobs is a loop. */
static Vec erase_typed_run_jobs(Heap *heap, const EtEnv *env, const Vec *jobs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < jobs->len; i++) {
    const EtJob *job = jobs->items[i];
    EtPlan plan = erase_typed_plan_of(heap, env, job != NULL ? job : &erase_typed_job_placeholder);
    Vec inner_jobs = erase_typed_jobs_of(&plan);
    Vec inner = erase_typed_run_jobs(heap, env, &inner_jobs);
    EtRx rx = erase_typed_built(heap, erase_typed_oks(heap, &inner), erase_typed_build_of(&plan));
    erase_typed_push(heap, &out, erase_typed_rx_box(heap, rx));
  }
  return out;
}

static EtRx erase_typed_one_rx(const Vec *rs) {
  if (rs->len == 0) return erase_typed_xno(str_lit("internal: no result"));
  const EtRx *r = rs->items[0];
  if (r == NULL) return erase_typed_rx_placeholder;
  return *r;
}

/* ---- Definitions and modules ---- */

/* Fctx{ks, vs, body}: the context of a function body. */
typedef struct {
  const EtCtx *ks;
  const EtSlots *vs;
  const Term *body;
} EtFctx;

/* The context of a function body: the telescope and the lambdas, in step (as `tele`). The Bend
 * tail recursion is a loop; the kind and the slot of each binder use the context before it. */
static EtFctx erase_typed_fctx(Heap *heap, const EtEnv *env, const EtCtx *ks, const EtSlots *vs, const Term *ty,
                               const Term *body) {
  const EtCtx *k = ks;
  const EtSlots *v = vs;
  const Term *u = ty;
  const Term *b = body;
  while (u != NULL && erase_typed_is_pi(u) && u->body != NULL && u->shape.domain != NULL) {
    const Term *dom = u->shape.domain;
    Quantity q = u->shape.quantity;
    Str x = u->shape.name;
    EtEnd e = erase_typed_end_of(dom);
    const RirParam *slot = erase_typed_slot_of(heap, erase_typed_copies_of(env), q, erase_typed_peel_name(b, x),
                                               erase_typed_binder_er(q, e, erase_typed_er(heap, env, k, dom)));
    const EtCtx *next = erase_typed_cons(heap, erase_typed_bind_kind(heap, env, k, e, x, dom), k);
    v = erase_typed_slot_cons(heap, slot, v);
    k = next;
    u = u->body;
    b = erase_typed_peel_body(b);
  }
  EtFctx f = {k, v, b};
  return f;
}

static EtRx erase_typed_body_rx(Heap *heap, const EtEnv *env, EtFctx f) {
  Vec jobs = vec_new(heap->arena);
  erase_typed_push(heap, &jobs, erase_typed_job(heap, f.ks, f.vs, erase_typed_wown(heap), f.body));
  Vec rs = erase_typed_run_jobs(heap, env, &jobs);
  return erase_typed_one_rx(&rs);
}

/* It: no item, one item, or why the name is refused. */
typedef enum { ET_IT_NONE, ET_IT_ONE, ET_IT_BAD } EtItKind;
typedef struct {
  EtItKind kind;
  const RirItem *item; /* ET_IT_ONE */
  Str why;             /* ET_IT_BAD */
} EtIt;

static EtIt erase_typed_it(EtItKind kind, const RirItem *item, Str why) {
  EtIt it = {kind, item, why};
  return it;
}

static EtIt erase_typed_fn_it(Heap *heap, Str name, Vec generics, const Vec *params, const RirTy *ret, EtRx r) {
  switch (r.kind) {
    case ET_XOK:
      return erase_typed_it(ET_IT_ONE, rir_ifn(heap, name, generics, erase_typed_under(heap, params, r.ex), ret, r.ex),
                            str_lit(""));
    case ET_XNO:
      return erase_typed_it(ET_IT_BAD, NULL, r.why);
  }
  return erase_typed_it(ET_IT_BAD, NULL, r.why);
}

static EtIt erase_typed_fn_entry(Heap *heap, const EtEnv *env, Str name, Vec generics, const Vec *params,
                                 const RirTy *ret, MaybeEntry entry) {
  if (entry.some && entry.value.kind == ENTRY_DEF) {
    EtFctx f = erase_typed_fctx(heap, env, NULL, NULL, entry.value.def.ty, entry.value.def.body);
    return erase_typed_fn_it(heap, name, generics, params, ret, erase_typed_body_rx(heap, env, f));
  }
  return erase_typed_it(ET_IT_BAD, NULL, str_lit("a function with no definition"));
}

static EtIt erase_typed_class_item(Heap *heap, const EtEnv *env, const EtClass *c, MaybeEntry entry) {
  switch (c->kind) {
    case ET_CFN:
      return erase_typed_fn_entry(heap, env, c->name, c->generics, &c->params, c->ret, entry);
    case ET_CDATA:
      return erase_typed_it(ET_IT_ONE, c->item, str_lit(""));
    case ET_CNO:
      return erase_typed_it(ET_IT_BAD, NULL, c->why);
    case ET_CBOOL:
    case ET_CPROP:
    case ET_CTYPE:
    case ET_CABSURD:
    case ET_CSKIP:
      return erase_typed_it(ET_IT_NONE, NULL, str_lit(""));
  }
  return erase_typed_it(ET_IT_NONE, NULL, str_lit(""));
}

/* D-196 (a): `items` and `bad` grow at the end (source order); the Bend lists are in reverse
 * order and module_of reverses them, so C does not. */
static EtEs erase_typed_item_step(Heap *heap, EtEs es, Str name, EtIt it) {
  EtEs out = es;
  switch (it.kind) {
    case ET_IT_NONE:
      return out;
    case ET_IT_ONE:
      erase_typed_push(heap, &out.items, it.item);
      return out;
    case ET_IT_BAD: {
      const Str parts[4] = {str_lit("refused "), name, str_lit(": "), it.why};
      erase_typed_push(heap, &out.bad, erase_typed_str_box(heap, erase_typed_cat(heap, parts, 4)));
      return out;
    }
  }
  return out;
}

static EtEs erase_typed_emit_step(Heap *heap, EtEs es, Str name) {
  EtClass c = erase_typed_name_class(heap, &es.env, name);
  Globals globals = erase_typed_globals_of(&es.env);
  EtIt it = erase_typed_class_item(heap, &es.env, &c, global_find(name, globals));
  Vec copies = erase_typed_copy_names(heap, &c);
  EtEs next = es;
  next.env = erase_typed_env(globals, erase_typed_append(heap, &copies, erase_typed_copies_of(&es.env)));
  return erase_typed_item_step(heap, next, name, it);
}

/* `names` holds Str pointers. The Bend recursion over the names is a loop. */
static EtEs erase_typed_emit_steps(Heap *heap, EtEs es, const Vec *names) {
  EtEs cur = es;
  for (size_t i = 0; i < names->len; i++) {
    const Str *name = names->items[i];
    if (name != NULL) cur = erase_typed_emit_step(heap, cur, *name);
  }
  return cur;
}

static EtEs erase_typed_es(EtEnv env, Vec items, Vec bad) {
  EtEs es = {env, items, bad};
  return es;
}

static EtEs erase_typed_emit_failed(Heap *heap, Error e) {
  Vec bad = vec_new(heap->arena);
  erase_typed_push(heap, &bad, erase_typed_str_box(heap, erase_typed_cat2(heap, str_lit("error: "), error_to_string(heap, e))));
  return erase_typed_es(erase_typed_env(global_empty(), vec_new(heap->arena)), vec_new(heap->arena), bad);
}

/* `r` holds an ElabChecked * when done; `decls` holds SDecl pointers. */
static EtEs erase_typed_emit_checked(Heap *heap, Vec copies, const Vec *decls, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return erase_typed_emit_failed(heap, r.error);
    case RESULT_DONE: {
      const ElabChecked *checked = r.value;
      if (checked == NULL) heap->oom = true;
      EtEnv env = erase_typed_env(checked != NULL ? checked->globals : global_empty(), copies);
      Vec declared = erase_typed_decl_names(heap, decls);
      return erase_typed_emit_steps(heap, erase_typed_es(env, vec_new(heap->arena), vec_new(heap->arena)), &declared);
    }
  }
  return erase_typed_emit_failed(heap, r.error);
}

/* `r` holds a Vec * of SDecl pointers when done. */
static EtEs erase_typed_emit_parsed(Heap *heap, Globals globals, Vec copies, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return erase_typed_emit_failed(heap, r.error);
    case RESULT_DONE: {
      const Vec *parsed = r.value;
      Vec none = vec_new(heap->arena);
      if (parsed == NULL) heap->oom = true;
      const Vec *decls = parsed != NULL ? parsed : &none;
      return erase_typed_emit_checked(heap, copies, decls, elab_elab_program_in(heap, globals, budget_unlimited(), decls));
    }
  }
  return erase_typed_emit_failed(heap, r.error);
}

EtEs erase_typed_emit_source(Heap *heap, const EtEnv *env, Str source) {
  return erase_typed_emit_parsed(heap, env->globals, env->copies, parser_program(heap, source));
}

static bool erase_typed_item_nat(Heap *heap, const RirItem *item) {
  if (item == NULL) return false;
  switch (item->kind) {
    case RIR_IFN: {
      Vec ret = vec_new(heap->arena);
      erase_typed_push(heap, &ret, item->ret);
      Vec ptys = erase_typed_param_tys(heap, &item->params);
      Vec ts = erase_typed_append(heap, &ret, &ptys);
      return erase_typed_mentions(str_lit("Nat"), &ts);
    }
    case RIR_IUSE:
    case RIR_IENUM:
    case RIR_IUNIT:
      return false;
  }
  return false;
}

/* `items` holds RirItem pointers. */
static bool erase_typed_nat_used(Heap *heap, const Vec *items) {
  for (size_t i = 0; i < items->len; i++) {
    if (erase_typed_item_nat(heap, items->items[i])) return true;
  }
  return false;
}

static Vec erase_typed_nat_use(Heap *heap, bool nat) {
  Vec out = vec_new(heap->arena);
  if (nat) erase_typed_push(heap, &out, rir_iuse(heap, str_lit("nat")));
  return out;
}

/* The file name with no `.mech` at its end. The Bend walk keeps every char until the rest is
 * exactly ".mech", so only a suffix after at least one char goes; ".mech" is ASCII, so the byte
 * test equals the code point walk. */
Str erase_typed_stem(Str s) {
  Str suffix = str_lit(".mech");
  if (s.len > suffix.len && str_eq(str_n(s.data + (s.len - suffix.len), suffix.len), suffix))
    return str_n(s.data, s.len - suffix.len);
  return s;
}

static Vec erase_typed_module_items(Heap *heap, const Vec *pre, const Vec *items) {
  Vec nat = erase_typed_nat_use(heap, erase_typed_nat_used(heap, items));
  Vec tail = erase_typed_append(heap, &nat, items);
  return erase_typed_append(heap, pre, &tail);
}

static EtModuleResult erase_typed_module_result(bool ok, RirModule module, Str why) {
  EtModuleResult r = {ok, module, why};
  return r;
}

static EtModuleResult erase_typed_module_of(Heap *heap, Str source, const Vec *pre, const EtEs *es) {
  if (es->bad.len == 0)
    return erase_typed_module_result(true, rir_module(source, erase_typed_module_items(heap, pre, &es->items)),
                                     str_lit(""));
  return erase_typed_module_result(false, rir_module(source, vec_new(heap->arena)),
                                   erase_typed_cat2(heap, erase_typed_join(heap, str_lit("\n"), &es->bad), str_lit("\n")));
}

/* One module. `name` is the file name for the `//!` line. */
EtModuleResult erase_typed_emit1(Heap *heap, Str name, Str source) {
  EtEnv env = erase_typed_env(global_initial(heap), vec_new(heap->arena));
  EtEs es = erase_typed_emit_source(heap, &env, source);
  Vec pre = vec_new(heap->arena);
  return erase_typed_module_of(heap, name, &pre, &es);
}

static EtModuleResult erase_typed_emit2_of(Heap *heap, const EtSt *st, Str first_name, Str name, Str source) {
  Vec pre = vec_new(heap->arena);
  erase_typed_push(heap, &pre, rir_iuse(heap, erase_typed_stem(first_name)));
  EtEs es = erase_typed_emit_source(heap, &st->env, source);
  return erase_typed_module_of(heap, name, &pre, &es);
}

/* The module of file 2, checked in the globals of file 1. */
EtModuleResult erase_typed_emit2(Heap *heap, Str first_name, Str first_source, Str name, Str source) {
  EtEnv env = erase_typed_env(global_initial(heap), vec_new(heap->arena));
  EtSt st = erase_typed_classify_source(heap, &env, vec_new(heap->arena), first_source);
  return erase_typed_emit2_of(heap, &st, first_name, name, source);
}
