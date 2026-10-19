/* C port of tests/rust_emit_oracle.bend, the DIFF-EXEC oracle of the rust_emit driver (a
 * library module of that driver: the Bend file has no main). A probe is a closed definition of the
 * last input file. `values`: the kernel value of each probe, in the Rust `{:?}` form. `probes`: the
 * generated probes of each first-order runtime function. `bin`: the text of
 * `src/bin/diff_exec.rs`, one `println!` for each probe.
 *
 * The definitions keep the Bend names (prefix reo_; the three IO entries are rust_emit_oracle_*)
 * and the Bend order. A Bend list is a Vec of pointers in the same order. A Bend function that
 * takes the work list `rest` and returns `front ++ rest` returns only the front here (forward
 * order); render keeps the work list as a stack whose top is the Bend list head and pushes each
 * front in reverse, so each step costs the length of its front. A failed allocation sets
 * heap->oom; the helpers then return placeholders and the IO entries check heap->oom. */
#include <stdio.h>

#include "rust_emit_oracle.h"
#include "cli.h"
#include "surface.h"

static const Str reo_str_placeholder = {0};

/* Appends `item` to `acc`; a failed push sets heap->oom. */
static void reo_push(Heap *heap, Vec *acc, const void *item) {
  if (!vec_push(acc, (void *)item)) heap->oom = true;
}

/* The concatenation of `n` parts in one buffer. */
static Str reo_cat(Heap *heap, const Str *parts, size_t n) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < n; i++) {
    if (!strbuf_push(&buf, parts[i])) heap->oom = true;
  }
  return strbuf_str(&buf);
}

static const Str *reo_str_box(Heap *heap, Str s) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) {
    heap->oom = true;
    return &reo_str_placeholder;
  }
  *box = s;
  return box;
}

/* Result<String, String>. */
typedef struct {
  bool ok;
  Str text; /* ok */
  Str why;  /* !ok */
} ReoText;

static ReoText reo_text_done(Str text) {
  ReoText r = {true, text, {0}};
  return r;
}

static ReoText reo_text_fail(Str why) {
  ReoText r = {false, {0}, why};
  return r;
}

/* The Rust `{:?}` text of a kernel value. `whnf` forces the head only, so each kept field is
 * forced in turn. An erased field is not forced and not printed. The printer is one loop on a work
 * list. `IVal`: a value that is not forced yet. `IText`: final text. */
typedef enum { REO_IVAL, REO_ITEXT } ReoItemKind;
typedef struct {
  ReoItemKind kind;
  const Value *v; /* REO_IVAL */
  Str s;          /* REO_ITEXT */
} ReoItem;

static const ReoItem reo_item_placeholder = {REO_ITEXT, NULL, {0}};

static const ReoItem *reo_ival(Heap *heap, const Value *v) {
  ReoItem *item = heap_alloc(heap, sizeof(ReoItem));
  if (item == NULL) {
    heap->oom = true;
    return &reo_item_placeholder;
  }
  item->kind = REO_IVAL;
  item->v = v;
  item->s = str_empty();
  return item;
}

static const ReoItem *reo_itext(Heap *heap, const char *s) {
  ReoItem *item = heap_alloc(heap, sizeof(ReoItem));
  if (item == NULL) {
    heap->oom = true;
    return &reo_item_placeholder;
  }
  item->kind = REO_ITEXT;
  item->v = NULL;
  item->s = str_lit(s);
  return item;
}

static const ReoItem *reo_itext_str(Heap *heap, Str s) {
  ReoItem *item = heap_alloc(heap, sizeof(ReoItem));
  if (item == NULL) {
    heap->oom = true;
    return &reo_item_placeholder;
  }
  item->kind = REO_ITEXT;
  item->v = NULL;
  item->s = s;
  return item;
}

/* Result<String, List<Item>>: on success `items` is the front of the work list (forward order). */
typedef struct {
  bool ok;
  Vec items; /* ok: ReoItem pointers */
  Str why;   /* !ok */
} ReoItems;

static ReoItems reo_items_done(Vec items) {
  ReoItems r = {true, items, {0}};
  return r;
}

static ReoItems reo_items_fail(Heap *heap, Str why) {
  ReoItems r = {false, vec_new(heap->arena), why};
  return r;
}

/* One field in front of the kept fields: NULL in `t` is an erased field (Bend None), so only a
 * kept field is appended. kept walks the fields forward, so each append is the Bend prepend onto
 * the kept fields after it. */
static void reo_keep_one(Heap *heap, const RirTy *t, const Value *a, Vec *acc) {
  if (t != NULL) reo_push(heap, acc, a);
}

/* The kept fields of a constructor value (Value pointers), up to the shorter of the two lists.
 * NULL in `tys` is an erased field. */
static Vec reo_kept(Heap *heap, const Vec *tys, const Vec *args) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < tys->len && i < args->len; i++) {
    reo_keep_one(heap, (const RirTy *)tys->items[i], (const Value *)args->items[i], &out);
  }
  return out;
}

/* `, v` for each value of `vals` from index `from` on, then `)`, appended to `acc`. */
static void reo_more_items(Heap *heap, const Vec *vals, size_t from, Vec *acc) {
  for (size_t i = from; i < vals->len; i++) {
    reo_push(heap, acc, reo_itext(heap, ", "));
    reo_push(heap, acc, reo_ival(heap, (const Value *)vals->items[i]));
  }
  reo_push(heap, acc, reo_itext(heap, ")"));
}

/* `(v1, v2, ..)` for the values of `vals`, nothing for no value, appended to `acc`. */
static void reo_field_items(Heap *heap, const Vec *vals, Vec *acc) {
  VecGet first = vec_get(vals, 0);
  switch (first.status) {
    case VEC_GET_OUT_OF_RANGE:
      return;
    case VEC_GET_OK:
      reo_push(heap, acc, reo_itext(heap, "("));
      reo_push(heap, acc, reo_ival(heap, (const Value *)first.value));
      reo_more_items(heap, vals, 1, acc);
      return;
  }
}

static ReoItems reo_not_data(Heap *heap, Str c) {
  const Str parts[2] = {str_lit("a constructor of a family that is not runtime data: "), c};
  return reo_items_fail(heap, reo_cat(heap, parts, 2));
}

static ReoItems reo_ctor_items(Heap *heap, Str c, const Vec *args, const Vec *tys, EtClass cls) {
  Vec front = vec_new(heap->arena);
  switch (cls.kind) {
    case ET_CBOOL:
      reo_push(heap, &front, reo_itext(heap, str_eq(c, str_lit("mechTrue")) ? "true" : "false"));
      return reo_items_done(front);
    case ET_CDATA:
      if (cls.item == NULL) return reo_not_data(heap, c);
      switch (cls.item->kind) {
        case RIR_IUNIT:
          reo_push(heap, &front, reo_itext_str(heap, cls.item->name));
          return reo_items_done(front);
        case RIR_IENUM: {
          reo_push(heap, &front, reo_itext_str(heap, erase_typed_upper_first(heap, c)));
          Vec fields = reo_kept(heap, tys, args);
          reo_field_items(heap, &fields, &front);
          return reo_items_done(front);
        }
        case RIR_IUSE:
        case RIR_IFN:
          return reo_not_data(heap, c);
      }
      return reo_not_data(heap, c);
    case ET_CFN:
    case ET_CPROP:
    case ET_CTYPE:
    case ET_CABSURD:
    case ET_CNO:
    case ET_CSKIP:
      return reo_not_data(heap, c);
  }
  return reo_not_data(heap, c);
}

static ReoItems reo_not_ctor(Heap *heap) {
  return reo_items_fail(heap, str_lit("the value is not a constructor and not an integer"));
}

/* The front for one forced value. Bend calls Global.find_family twice on the same name and
 * globals; it is pure, so the C port calls it once. */
static ReoItems reo_whnf_items(Heap *heap, const EtEnv *env, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return reo_items_fail(heap, error_to_string(heap, r.error));
    case RESULT_DONE:
      break;
  }
  const Value *v = (const Value *)r.value;
  if (v == NULL) return reo_not_ctor(heap);
  switch (v->kind) {
    case VALUE_LIT:
      switch (v->lit.kind) {
        case LITERAL_INT: {
          Vec front = vec_new(heap->arena);
          reo_push(heap, &front, reo_itext_str(heap, big_to_string(heap, v->lit.integer)));
          return reo_items_done(front);
        }
        case LITERAL_STRING:
          return reo_not_ctor(heap);
      }
      return reo_not_ctor(heap);
    case VALUE_IN:
      if (v->shape.kind == SHAPE_MU && v->addr.kind == VADDR_CTOR) {
        MaybeFamily fam = global_find_family(v->shape.name, erase_typed_globals_of(env));
        Vec tys = erase_typed_ctor_tys(heap, env, fam, v->addr.name);
        EtClass cls = erase_typed_family_or_skip(heap, env, fam);
        return reo_ctor_items(heap, v->addr.name, &v->args, &tys, cls);
      }
      return reo_not_ctor(heap);
    case VALUE_UNIV:
    case VALUE_LAN:
    case VALUE_RAN:
    case VALUE_SEC:
    case VALUE_NEUTRAL:
      return reo_not_ctor(heap);
  }
  return reo_not_ctor(heap);
}

static ReoItems reo_items_of(Heap *heap, const EtEnv *env, const Value *v) {
  return reo_whnf_items(heap, env, eval_whnf(heap, erase_typed_globals_of(env), v));
}

/* The work list stack: `items[0 .. *depth)` is live, the top (the Bend list head) is the last
 * live slot. A slot under the old top is reused before the Vec grows. */
static void reo_stack_push(Heap *heap, Vec *stack, size_t *depth, const ReoItem *item) {
  if (*depth < stack->len) {
    stack->items[*depth] = (void *)item;
  } else {
    reo_push(heap, stack, item);
  }
  if (!heap->oom) *depth += 1;
}

/* Pushes `front` so that its first item is the new top. */
static void reo_stack_front(Heap *heap, Vec *stack, size_t *depth, const Vec *front) {
  for (size_t i = front->len; i > 0; i--) {
    reo_stack_push(heap, stack, depth, (const ReoItem *)front->items[i - 1]);
  }
}

/* `@unsafe` in Bend: a forced value puts its fields on the work list, so the self-call is not
 * structural. The loop stops because a kernel value is a finite tree. */
static ReoText reo_render(Heap *heap, const EtEnv *env, ReoItems todo, Str out) {
  if (!todo.ok) return reo_text_fail(todo.why);
  Vec stack = vec_new(heap->arena);
  size_t depth = 0;
  reo_stack_front(heap, &stack, &depth, &todo.items);
  StrBuf buf = strbuf_new(heap->arena);
  if (!strbuf_push(&buf, out)) heap->oom = true;
  while (depth > 0 && depth <= stack.len && !heap->oom) {
    const ReoItem *item = (const ReoItem *)stack.items[depth - 1];
    depth -= 1;
    switch (item->kind) {
      case REO_ITEXT:
        if (!strbuf_push(&buf, item->s)) heap->oom = true;
        break;
      case REO_IVAL: {
        ReoItems next = reo_items_of(heap, env, item->v);
        if (!next.ok) return reo_text_fail(next.why);
        reo_stack_front(heap, &stack, &depth, &next.items);
        break;
      }
    }
  }
  return reo_text_done(strbuf_str(&buf));
}

static ReoText reo_shown(Heap *heap, const EtEnv *env, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return reo_text_fail(error_to_string(heap, r.error));
    case RESULT_DONE:
      return reo_render(heap, env, reo_items_of(heap, env, (const Value *)r.value), str_empty());
  }
  return reo_text_fail(error_to_string(heap, r.error));
}

static ReoText reo_eval_show(Heap *heap, const EtEnv *env, Str name) {
  const Term *global = term_global(heap, name);
  if (global == NULL) {
    heap->oom = true;
    return reo_text_fail(str_empty());
  }
  return reo_shown(heap, env, eval_eval(heap, erase_typed_globals_of(env), vec_new(heap->arena), global));
}

static Str reo_value_line(Heap *heap, Str name, ReoText r) {
  if (r.ok) {
    const Str parts[4] = {name, str_lit(" = "), r.text, str_lit("\n")};
    return reo_cat(heap, parts, 4);
  } else {
    const Str parts[4] = {name, str_lit(" ! "), r.why, str_lit("\n")};
    return reo_cat(heap, parts, 4);
  }
}

/* `names`: Str pointers. One value line for each name, in order. */
static Str reo_value_lines(Heap *heap, const EtEnv *env, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < names->len && !heap->oom; i++) {
    Str name = *(const Str *)names->items[i];
    if (!strbuf_push(&buf, reo_value_line(heap, name, reo_eval_show(heap, env, name)))) heap->oom = true;
  }
  return strbuf_str(&buf);
}

/* Probe generation. `Combo`: a name suffix and the argument text of one call. */
typedef struct {
  Str label;
  Str args;
} ReoCombo;

static const ReoCombo reo_combo_placeholder = {{0}, {0}};

static const ReoCombo *reo_combo(Heap *heap, Str label, Str args) {
  ReoCombo *combo = heap_alloc(heap, sizeof(ReoCombo));
  if (combo == NULL) {
    heap->oom = true;
    return &reo_combo_placeholder;
  }
  combo->label = label;
  combo->args = args;
  return combo;
}

/* ReoCombo pointers. */
static Vec reo_nat_vals(Heap *heap) {
  Vec out = vec_new(heap->arena);
  reo_push(heap, &out, reo_combo(heap, str_lit("Zero"), str_lit("mechZero")));
  reo_push(heap, &out, reo_combo(heap, str_lit("One"), str_lit("(mechSucc mechZero)")));
  reo_push(heap, &out, reo_combo(heap, str_lit("Two"), str_lit("(mechSucc (mechSucc mechZero))")));
  return out;
}

static Vec reo_bool_vals(Heap *heap) {
  Vec out = vec_new(heap->arena);
  reo_push(heap, &out, reo_combo(heap, str_lit("False"), str_lit("mechFalse")));
  reo_push(heap, &out, reo_combo(heap, str_lit("True"), str_lit("mechTrue")));
  return out;
}

/* `ctors`: Ctor pointers. True when no constructor has an argument. */
static bool reo_fieldless(const Vec *ctors) {
  for (size_t i = 0; i < ctors->len; i++) {
    const Ctor *ctor = (const Ctor *)ctors->items[i];
    if (ctor != NULL && ctor->args.len > 0) return false;
  }
  return true;
}

static Vec reo_ctor_vals(Heap *heap, const Vec *ctors) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < ctors->len; i++) {
    const Ctor *ctor = (const Ctor *)ctors->items[i];
    if (ctor != NULL) reo_push(heap, &out, reo_combo(heap, erase_typed_upper_first(heap, ctor->name), ctor->name));
  }
  return out;
}

/* Maybe<List<Pos.Ctor>>. */
typedef struct {
  bool some;
  Vec ctors; /* some: Ctor pointers */
} ReoMaybeCtors;

/* A plain family has no parameter and no index: its name is a closed type. */
static ReoMaybeCtors reo_plain(Heap *heap, MaybeFamily fam) {
  ReoMaybeCtors none = {false, vec_new(heap->arena)};
  if (!fam.some) return none;
  if (fam.value.params.len > 0) return none;
  if (fam.value.indices.len > 0) return none;
  ReoMaybeCtors some = {true, fam.value.ctors};
  return some;
}

static Vec reo_family_vals(Heap *heap, ReoMaybeCtors ctors) {
  if (!ctors.some) return vec_new(heap->arena);
  if (reo_fieldless(&ctors.ctors)) return reo_ctor_vals(heap, &ctors.ctors);
  return vec_new(heap->arena);
}

/* The small inputs of one parameter type (ReoCombo pointers). No input = no generated probe. A NULL
 * type (only an allocation failure makes one; heap->oom is then set) has no input. */
static Vec reo_ty_vals(Heap *heap, const EtEnv *env, const RirTy *t) {
  if (t == NULL) return vec_new(heap->arena);
  switch (t->kind) {
    case RIR_TBOOL:
      return reo_bool_vals(heap);
    case RIR_TREF:
      return reo_ty_vals(heap, env, t->inner);
    case RIR_TCON:
      if (t->args.len > 0) return vec_new(heap->arena);
      if (str_eq(t->name, str_lit("MechNat"))) return reo_nat_vals(heap);
      return reo_family_vals(heap, reo_plain(heap, global_find_family(t->name, erase_typed_globals_of(env))));
    case RIR_TBOX:
    case RIR_TFN:
      return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

static Str reo_plain_name(Str name, ReoMaybeCtors ctors) {
  if (ctors.some) return name;
  return str_empty();
}

/* The mech text of a closed result type, or "" for a type outside the rule. */
static Str reo_ret_text(Heap *heap, const EtEnv *env, const RirTy *t) {
  if (t == NULL) return str_empty();
  switch (t->kind) {
    case RIR_TBOOL:
      return str_lit("MechBool");
    case RIR_TCON:
      if (t->args.len > 0) return str_empty();
      if (str_eq(t->name, str_lit("Nat"))) return str_lit("Nat");
      return reo_plain_name(t->name, reo_plain(heap, global_find_family(t->name, erase_typed_globals_of(env))));
    case RIR_TBOX:
    case RIR_TREF:
    case RIR_TFN:
      return str_empty();
  }
  return str_empty();
}

static const ReoCombo *reo_glue(Heap *heap, const ReoCombo *c, const ReoCombo *v) {
  const Str label[2] = {c->label, v->label};
  const Str args[3] = {c->args, str_lit(" "), v->args};
  return reo_combo(heap, reo_cat(heap, label, 2), reo_cat(heap, args, 3));
}

/* `c` glued to each of `vs`, in order, appended to `acc` (the Bend tail follows them). */
static void reo_cross_one(Heap *heap, const ReoCombo *c, const Vec *vs, Vec *acc) {
  for (size_t i = 0; i < vs->len; i++) {
    const ReoCombo *v = (const ReoCombo *)vs->items[i];
    if (v != NULL) reo_push(heap, acc, reo_glue(heap, c, v));
  }
}

/* Each of `cs` glued to each of `vs`: the order of `cs` first, then the order of `vs`. */
static Vec reo_cross(Heap *heap, const Vec *cs, const Vec *vs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < cs->len; i++) {
    const ReoCombo *c = (const ReoCombo *)cs->items[i];
    if (c != NULL) reo_cross_one(heap, c, vs, &out);
  }
  return out;
}

/* `ps`: RirParam pointers. The left fold of cross over the parameter inputs. */
static Vec reo_combos(Heap *heap, const EtEnv *env, const Vec *ps, Vec acc) {
  for (size_t i = 0; i < ps->len; i++) {
    const RirParam *p = (const RirParam *)ps->items[i];
    Vec vals = reo_ty_vals(heap, env, p != NULL ? p->ty : NULL);
    acc = reo_cross(heap, &acc, &vals);
  }
  return acc;
}

static Str reo_gen_lines(Heap *heap, Str name, Str ret, const Vec *cs) {
  StrBuf buf = strbuf_new(heap->arena);
  Str upper = erase_typed_upper_first(heap, name);
  for (size_t i = 0; i < cs->len; i++) {
    const ReoCombo *c = (const ReoCombo *)cs->items[i];
    if (c == NULL) continue;
    const Str parts[9] = {str_lit("def probe"), upper, c->label, str_lit(" : "), ret, str_lit(" := "), name, c->args, str_lit("\n")};
    for (size_t j = 0; j < 9; j++) {
      if (!strbuf_push(&buf, parts[j])) heap->oom = true;
    }
  }
  return strbuf_str(&buf);
}

/* `generics`: Str pointers. A generic function or a result type outside the rule has no probe. */
static Str reo_gen_fn(Heap *heap, Str name, const Vec *generics, Str ret, const Vec *cs) {
  if (generics->len > 0) return str_empty();
  if (ret.len == 0) return str_empty();
  return reo_gen_lines(heap, name, ret, cs);
}

static Str reo_gen_class(Heap *heap, const EtEnv *env, Str name, EtClass cls) {
  switch (cls.kind) {
    case ET_CFN: {
      Vec unit = vec_new(heap->arena);
      reo_push(heap, &unit, reo_combo(heap, str_empty(), str_empty()));
      Vec cs = reo_combos(heap, env, &cls.params, unit);
      return reo_gen_fn(heap, name, &cls.generics, reo_ret_text(heap, env, cls.ret), &cs);
    }
    case ET_CDATA:
    case ET_CBOOL:
    case ET_CPROP:
    case ET_CTYPE:
    case ET_CABSURD:
    case ET_CNO:
    case ET_CSKIP:
      return str_empty();
  }
  return str_empty();
}

/* `names`: Str pointers. The generated probes of each name, in order. */
static Str reo_gen_file(Heap *heap, const EtEnv *env, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < names->len && !heap->oom; i++) {
    const Str *name = (const Str *)names->items[i];
    if (name == NULL) continue;
    if (!strbuf_push(&buf, reo_gen_class(heap, env, *name, erase_typed_name_class(heap, env, *name)))) heap->oom = true;
  }
  return strbuf_str(&buf);
}

static Str reo_bin_lines(Heap *heap, const Vec *names) {
  StrBuf buf = strbuf_new(heap->arena);
  for (size_t i = 0; i < names->len; i++) {
    const Str *name = (const Str *)names->items[i];
    if (name == NULL) continue;
    const Str parts[3] = {str_lit("    println!(\"{:?}\", "), erase_typed_fn_name(heap, *name), str_lit("());\n")};
    for (size_t j = 0; j < 3; j++) {
      if (!strbuf_push(&buf, parts[j])) heap->oom = true;
    }
  }
  return strbuf_str(&buf);
}

static Str reo_bin_text(Heap *heap, Str module, const Vec *names) {
  const Str parts[5] = {str_lit("//! DIFF-EXEC: prints the value of each probe, one line for each probe.\n\nuse mech_out::"), module,
                        str_lit("::*;\n\nfn main() {\n"), reo_bin_lines(heap, names), str_lit("}\n")};
  return reo_cat(heap, parts, 5);
}

/* The files in order. `last`: the declared names of the last file (Str pointers). `gen`: the
 * generated probes of all files. `bad`: the refusal lines (Str pointers) in source order (D-196 a:
 * the Bend list is in reverse order, so result_of joins it with no reverse). */
typedef struct {
  EtEnv env;
  Str module;
  Vec last;
  Str gen;
  Vec bad;
} ReoSt;

/* `r` holds a Vec * of SDecl pointers when done. */
static Vec reo_names_of(Heap *heap, Result r) {
  switch (r.kind) {
    case RESULT_FAIL:
      return vec_new(heap->arena);
    case RESULT_DONE: {
      const Vec *decls = r.value;
      if (decls == NULL) {
        heap->oom = true;
        return vec_new(heap->arena);
      }
      return erase_typed_decl_names(heap, decls);
    }
  }
  return vec_new(heap->arena);
}

static ReoSt reo_after(Heap *heap, EtEs es, Str module, Vec names, Str gen) {
  const Str parts[2] = {gen, reo_gen_file(heap, &es.env, &names)};
  ReoSt st = {es.env, module, names, reo_cat(heap, parts, 2), es.bad};
  return st;
}

/* One input file: the path as given, and the text. */
typedef struct {
  Str name;
  Str text;
} ReoSrc;

static ReoSt reo_check_src(Heap *heap, const EtEnv *env, Str gen, const ReoSrc *src) {
  EtEs es = erase_typed_emit_source(heap, env, src->text);
  return reo_after(heap, es, erase_typed_stem(src->name), reo_names_of(heap, parser_program(heap, src->text)), gen);
}

/* A file after a refused file is not read: its globals are not there. */
static ReoSt reo_check_step(Heap *heap, ReoSt st, const ReoSrc *src) {
  if (st.bad.len == 0) return reo_check_src(heap, &st.env, st.gen, src);
  return st;
}

/* `srcs`: ReoSrc pointers in order. */
static ReoSt reo_check_all(Heap *heap, const Vec *srcs, ReoSt st) {
  for (size_t i = 0; i < srcs->len && !heap->oom; i++) {
    const ReoSrc *src = (const ReoSrc *)srcs->items[i];
    if (src != NULL) st = reo_check_step(heap, st, src);
  }
  return st;
}

typedef enum { REO_MVALUES, REO_MPROBES, REO_MBIN } ReoMd;

static Str reo_text_of(Heap *heap, ReoMd md, const EtEnv *env, Str module, const Vec *last, Str gen) {
  switch (md) {
    case REO_MVALUES:
      return reo_value_lines(heap, env, last);
    case REO_MPROBES:
      return gen;
    case REO_MBIN:
      return reo_bin_text(heap, module, last);
  }
  return gen;
}

static ReoText reo_result_of(Heap *heap, ReoMd md, const ReoSt *st) {
  if (st->bad.len == 0) return reo_text_done(reo_text_of(heap, md, &st->env, st->module, &st->last, st->gen));
  const Str parts[2] = {erase_typed_join(heap, str_lit("\n"), &st->bad), str_lit("\n")};
  return reo_text_fail(reo_cat(heap, parts, 2));
}

/* Done: IO.write (the text on stdout with no newline, exit 0). Fail: IO.die 65 (the message and a
 * newline on stderr). An out-of-memory heap is the host failure exit. */
static int reo_finish(Heap *heap, ReoText r) {
  if (heap->oom) return cli_host_failure();
  if (r.ok) {
    if (r.text.len > 0) fwrite(r.text.data, 1, r.text.len, stdout);
    return 0;
  }
  return cli_die(65, r.why);
}

/* The files in order into `srcs` (ReoSrc pointers); 0, or the exit code of the Ro.read_file that
 * ended the run (its stderr line is written). */
static int reo_read_all(Heap *heap, const Vec *paths, Vec *srcs) {
  for (size_t i = 0; i < paths->len; i++) {
    const Str *path = (const Str *)paths->items[i];
    if (path == NULL) {
      heap->oom = true;
      return cli_host_failure();
    }
    RustOutText t = rust_out_read_file(heap, *path);
    if (!t.ok) return t.code;
    ReoSrc *src = heap_alloc(heap, sizeof(ReoSrc));
    if (src == NULL) {
      heap->oom = true;
      return cli_host_failure();
    }
    src->name = *path;
    src->text = t.text;
    reo_push(heap, srcs, src);
  }
  return 0;
}

static int reo_run(Heap *heap, ReoMd md, const Vec *paths) {
  Vec srcs = vec_new(heap->arena);
  int code = reo_read_all(heap, paths, &srcs);
  if (code != 0) return code;
  if (heap->oom) return cli_host_failure();
  ReoSt start = {erase_typed_env(global_initial(heap), vec_new(heap->arena)), str_empty(), vec_new(heap->arena), str_empty(),
                 vec_new(heap->arena)};
  ReoSt st = reo_check_all(heap, &srcs, start);
  return reo_finish(heap, reo_result_of(heap, md, &st));
}

int rust_emit_oracle_values(Heap *heap, const Vec *paths) { return reo_run(heap, REO_MVALUES, paths); }

int rust_emit_oracle_probes(Heap *heap, const Vec *paths) { return reo_run(heap, REO_MPROBES, paths); }

int rust_emit_oracle_bin(Heap *heap, const Vec *paths) { return reo_run(heap, REO_MBIN, paths); }
