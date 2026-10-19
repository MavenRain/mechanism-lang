/* check (kernel/check.bend): declarations, families and constructors over the engine. */
#include "kernel.h"
#include <string.h>

/* One zeroed frame per bind: the union of the free variables of the continuations. */
typedef struct {
  Ctx ctx;
  Globals globals;
  Nat nat;               /* size, arity */
  Str str;               /* name, family */
  const Term *term;      /* ty */
  const Term *body;
  const Value *value;    /* tyv */
  const LevelVar *level; /* the declared level */
  const LevelVar *actual;
  Vec vec;               /* the remaining telescope, decls or ctors */
  Vec names;             /* the one name group of check_family_scheme_comp */
  const Vec *group;
  Decl decl;
  FamilyDecl fdecl;
  CtorDecl cdecl;
  Family family;
  const TeleBinder *binder;
  Entry entry;
  const Ctor *ctor;
  const Ctx *actx;
  const EnvUses *penv;
  size_t np;
  size_t depth;
} CheckFrame;

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static CheckFrame *frame_new(Heap *heap) {
  CheckFrame *frame = heap_alloc(heap, sizeof *frame);
  if (frame == NULL) return NULL;
  memset(frame, 0, sizeof *frame);
  return frame;
}

static Comp bind_with(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return budget_bind(heap, first, next);
}

static Comp bind_frame(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), CheckFrame *frame) {
  if (frame == NULL) return comp_oom();
  return bind_with(heap, first, fn, frame);
}

static Comp pure_unit(void) { return budget_pure(kernel_unit()); }

static Comp pure_bool(bool value) { return budget_pure(kernel_bool_box(value)); }

static Comp pure_ctx(Heap *heap, Ctx ctx) {
  Ctx *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ctx;
  return budget_pure(boxed);
}

static Comp pure_entry(Heap *heap, Entry entry) {
  Entry *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = entry;
  return budget_pure(boxed);
}

static Comp pure_globals(Heap *heap, Globals globals) {
  Globals *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = globals;
  return budget_pure(boxed);
}

static Comp pure_vec(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = items;
  return budget_pure(boxed);
}

static Comp pure_ctor(Heap *heap, Ctor ctor) {
  Ctor *boxed = positivity_ctor_new(heap, ctor);
  if (boxed == NULL) return comp_oom();
  return budget_pure(boxed);
}

/* Bend `a ++ b ++ ...` over fixed part arrays. */
static Str cats(Heap *heap, size_t count, const Str *parts) {
  Str out = str_lit("");
  size_t i;
  for (i = 0; i < count; i++) out = heap_str_cat(heap, out, parts[i]);
  return out;
}

/* Nat.sub(n, 1n): saturating. */
static Nat pred(Heap *heap, Nat n) {
  NatSub sub = nat_sub(heap->arena, n, heap_nat_of_u64(heap, 1));
  switch (sub.status) {
  case NAT_SUB_OK:
    return sub.value;
  case NAT_SUB_UNDERFLOW:
    return nat_zero();
  case NAT_SUB_OOM:
    heap->oom = true;
    return nat_zero();
  }
  return nat_zero();
}

static Nat nat_of_len(Heap *heap, size_t len) { return heap_nat_of_u64(heap, (uint64_t)len); }

/* The Bend list tail: the same items from index 1, never pushed to. */
static Vec list_tail(Vec xs) {
  Vec out = xs;
  if (xs.len == 0) return xs;
  out.items = xs.items + 1;
  out.len = xs.len - 1;
  out.cap = out.len;
  return out;
}

static void *head_of(const Vec *xs) {
  VecGet got = vec_get(xs, 0);
  if (got.status == VEC_GET_OK) return got.value;
  return NULL;
}

static Vec singleton(Heap *heap, void *item) {
  Vec out = vec_new(heap->arena);
  if (!vec_push(&out, item)) heap->oom = true;
  return out;
}

/* Bend `item <> rest`: a new list, the item first. */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  size_t i;
  if (!vec_push(&out, item)) heap->oom = true;
  for (i = 0; i < rest->len; i++) {
    VecGet got = vec_get(rest, i);
    if (got.status == VEC_GET_OK && !vec_push(&out, got.value)) heap->oom = true;
  }
  return out;
}

/* Local and Ctx are the context types (Context.Local, Context.Ctx). */

Ctx check_make(Heap *heap, Globals globals, Budget budget, Nat level_arity) {
  Ctx ctx;
  ctx.globals = globals;
  ctx.env = vec_new(heap->arena);
  ctx.locals = NULL;
  ctx.size = nat_zero();
  ctx.budget = budget;
  ctx.level_arity = level_arity;
  ctx.levels_checked = false;
  return ctx;
}

Globals check_globals_of(Ctx ctx) { return ctx.globals; }

Vec check_env_of(Ctx ctx) { return ctx.env; }

Nat check_size_of(Ctx ctx) { return ctx.size; }

Vec check_names(Heap *heap, const Vec *locals) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < locals->len; i++) {
    VecGet got = vec_get(locals, i);
    Local *local = got.value;
    if (got.status != VEC_GET_OK) continue;
    if (!vec_push(&out, &local->name)) heap->oom = true;
  }
  return out;
}

Vec check_names_of(Heap *heap, Ctx ctx) {
  Vec locals = context_to_list(heap, ctx.locals);
  return check_names(heap, &locals);
}

MaybeNat check_name_index(Heap *heap, LView view, Str name, Nat index) {
  MaybeNat out;
  LView v = view;
  Nat i = index;
  while (v.kind == LVIEW_CONS) {
    if (str_eq(v.head.name, name)) {
      out.some = true;
      out.value = i;
      return out;
    }
    i = heap_nat_succ(heap, i);
    v = context_lview(heap, v.tail);
  }
  out.some = false;
  out.value = nat_zero();
  return out;
}

MaybeNat check_index_of_name(Heap *heap, Ctx ctx, Str name) {
  return check_name_index(heap, context_lview(heap, ctx.locals), name, nat_zero());
}

Ctx check_bind(Heap *heap, Str name, Quantity q, const Value *ty, Ctx ctx) {
  Ctx out = ctx;
  out.env = cons(heap, (void *)value_var(heap, ctx.size), &ctx.env);
  out.locals = context_lcons(heap, context_local(name, q, ty), ctx.locals);
  out.size = heap_nat_succ(heap, ctx.size);
  return out;
}

Ctx check_define(Heap *heap, Str name, Quantity q, const Value *ty, const Value *value, Ctx ctx) {
  Ctx out = ctx;
  out.env = cons(heap, (void *)value, &ctx.env);
  out.locals = context_lcons(heap, context_local(name, q, ty), ctx.locals);
  out.size = heap_nat_succ(heap, ctx.size);
  return out;
}

bool check_readable(Quantity mode, Quantity q) {
  return quantity_equal(mode, QUANTITY_ZERO) || !quantity_equal(q, QUANTITY_ZERO);
}

Budget check_context_budget(Ctx ctx) { return ctx.budget; }

Result check_infer_result(Result result) {
  switch (result.kind) {
  case RESULT_FAIL:
    return result;
  case RESULT_DONE: {
    const Inferred *inferred = result.value;
    return budget_result_done((void *)inferred->ty);
  }
  }
  return result;
}

Result check_infer(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  return check_infer_result(
      budget_run(heap, check_engine_infer_uses(heap, ctx, quantity_runtime(mode), term), check_context_budget(ctx)));
}

Result check_infer_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  return budget_run(heap, check_engine_infer_scoped(heap, ctx, mode, term), check_context_budget(ctx));
}

Result check_check_result(Result result) {
  switch (result.kind) {
  case RESULT_FAIL:
    return result;
  case RESULT_DONE:
    return budget_result_done(kernel_unit());
  }
  return result;
}

Result check_check(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected) {
  return check_check_result(budget_run(heap, check_engine_check_uses(heap, ctx, quantity_runtime(mode), term, expected),
                                       check_context_budget(ctx)));
}

Result check_infer_univ(Heap *heap, Ctx ctx, const Term *term) {
  return budget_run(heap, check_engine_infer_univ(heap, ctx, term), check_context_budget(ctx));
}

/* The law type_valued: see check_type_valued below. */

static Comp type_valued_node_after_cod(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  return check_type_valued(heap, f->globals, heap_nat_succ(heap, f->nat), value);
}

Comp check_type_valued_node(Heap *heap, Globals globals, Nat size, const Value *value) {
  if (value->kind == VALUE_UNIV) return pure_bool(true);
  if (value->kind == VALUE_RAN && value->shape.kind == SHAPE_PI) {
    CheckFrame *f = frame_new(heap);
    if (f == NULL) return comp_oom();
    f->globals = globals;
    f->nat = size;
    return bind_frame(
        heap,
        budget_lift(eval_open_closure(heap, globals, value->diagram, singleton(heap, (void *)value_var(heap, size)))),
        type_valued_node_after_cod, f);
  }
  return pure_bool(false);
}

static Comp type_valued_after_whnf(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  return check_type_valued_node(heap, f->globals, f->nat, value);
}

Comp check_type_valued(Heap *heap, Globals globals, Nat size, const Value *value) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->nat = size;
  return bind_frame(heap, budget_lift(eval_whnf(heap, globals, value)), type_valued_after_whnf, f);
}

/* The entry of a checked definition. Family scheme instances under an injective level renaming reuse it. */
Entry check_checked_entry(const Term *ty, const Term *body) {
  DefEntry def;
  def.ty = ty;
  def.body = body;
  def.reducible = true;
  def.rec_arg.some = false;
  def.rec_arg.value = nat_zero();
  def.partial = false;
  return global_entry_def(def);
}

static Comp check_definition_after_uses(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return pure_entry(heap, check_checked_entry(f->term, f->body));
}

static Comp check_definition_after_erased(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  bool erased = kernel_bool_unbox(value);
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, erased ? QUANTITY_ZERO : QUANTITY_ONE, f->body, f->value),
                    check_definition_after_uses, f);
}

static Comp check_definition_after_body(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  f->body = value;
  return bind_frame(heap, check_type_valued(heap, check_globals_of(f->ctx), nat_zero(), f->value),
                    check_definition_after_erased, f);
}

Comp check_check_definition(Heap *heap, Ctx ctx, Str name, const Term *ty, MaybeTermPtr body, const Value *tyv) {
  CheckFrame *f = frame_new(heap);
  Str parts[3];
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->term = ty;
  f->value = tyv;
  parts[0] = str_lit("the definition ");
  parts[1] = name;
  parts[2] = str_lit(" has no body");
  return bind_frame(heap,
                    check_engine_optional(body.some, (void *)body.value,
                                          error_make(ERROR_CANNOT_INFER, cats(heap, 3, parts))),
                    check_definition_after_body, f);
}

Comp check_check_decl_kind(Heap *heap, DeclKind kind, Ctx ctx, Str name, const Term *ty, MaybeTermPtr body,
                           const Value *tyv) {
  switch (kind) {
  case DECL_POSTULATE:
    return pure_entry(heap, global_entry_axiom(ty));
  case DECL_DEFINITION:
    return check_check_definition(heap, ctx, name, ty, body, tyv);
  }
  return pure_entry(heap, global_entry_axiom(ty));
}

static Comp check_decl_comp_after_tyv(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  return check_check_decl_kind(heap, f->decl.kind, f->ctx, f->decl.name, f->decl.ty, f->decl.body, value);
}

static Comp check_decl_comp_after_level(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->decl.ty), check_decl_comp_after_tyv, f);
}

Comp check_check_decl_comp(Heap *heap, Globals globals, Nat arity, Decl decl) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->ctx = check_make(heap, globals, budget_unlimited(), arity);
  return bind_frame(heap, check_engine_infer_univ(heap, f->ctx, decl.ty), check_decl_comp_after_level, f);
}

Result check_check_decl_at(Heap *heap, Globals globals, Budget budget, Nat arity, Decl decl) {
  return budget_run(heap, check_check_decl_comp(heap, globals, arity, decl), budget);
}

Result check_check_decl(Heap *heap, Globals globals, Budget budget, Decl decl) {
  return check_check_decl_at(heap, globals, budget, nat_zero(), decl);
}

static Comp check_scheme_comp_after_entry(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

Comp check_check_scheme_comp(Heap *heap, Globals globals, Nat arity, Decl decl) {
  return bind_with(heap, check_check_decl_comp(heap, globals, arity, decl), check_scheme_comp_after_entry, NULL);
}

Result check_check_scheme(Heap *heap, Globals globals, Budget budget, Nat arity, Decl decl) {
  return budget_run(heap, check_check_scheme_comp(heap, globals, arity, decl), budget);
}

Str check_decl_name(Decl decl) { return decl.name; }

bool check_absent_entry(MaybeEntry value) { return !value.some; }

static Comp check_decls_comp_after_rows(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Vec *rows = value;
  CheckRow *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return comp_oom();
  row->name = f->str;
  row->entry = f->entry;
  return pure_vec(heap, cons(heap, row, rows));
}

static Comp check_decls_comp_after_entry(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Entry *entry = value;
  f->entry = *entry;
  f->str = check_decl_name(f->decl);
  return bind_frame(heap,
                    check_check_decls_comp(heap, global_add(heap, f->str, f->entry, f->globals), list_tail(f->vec)),
                    check_decls_comp_after_rows, f);
}

static Comp check_decls_comp_after_absent(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_check_decl_comp(heap, f->globals, nat_zero(), f->decl), check_decls_comp_after_entry,
                    f);
}

Comp check_check_decls_comp(Heap *heap, Globals globals, Vec decls) {
  CheckFrame *f;
  const Decl *decl;
  Str parts[3];
  if (decls.len == 0) return pure_vec(heap, vec_new(heap->arena));
  decl = head_of(&decls);
  if (decl == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->vec = decls;
  f->decl = *decl;
  parts[0] = str_lit("the name ");
  parts[1] = check_decl_name(*decl);
  parts[2] = str_lit(" is already declared");
  return bind_frame(heap,
                    check_engine_require(check_absent_entry(global_find(check_decl_name(*decl), globals)),
                                         error_make(ERROR_MISMATCH, cats(heap, 3, parts))),
                    check_decls_comp_after_absent, f);
}

Result check_check_decls(Heap *heap, Globals globals, Budget budget, Vec decls) {
  return budget_run(heap, check_check_decls_comp(heap, globals, decls), budget);
}

static Comp check_telescope_after_tyv(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  return check_check_telescope(heap, list_tail(f->vec),
                               check_bind(heap, f->binder->name, f->binder->quantity, value, f->ctx));
}

static Comp check_telescope_after_level(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->binder->ty), check_telescope_after_tyv, f);
}

Comp check_check_telescope(Heap *heap, Vec tele, Ctx ctx) {
  CheckFrame *f;
  const TeleBinder *binder;
  if (tele.len == 0) return pure_ctx(heap, ctx);
  binder = head_of(&tele);
  if (binder == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->ctx = ctx;
  f->binder = binder;
  return bind_frame(heap, check_engine_infer_univ(heap, ctx, binder->ty), check_telescope_after_level, f);
}

static Comp index_rules_after_below(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[10];
  parts[0] = str_lit("the index ");
  parts[1] = f->binder->name;
  parts[2] = str_lit(" of ");
  parts[3] = f->str;
  parts[4] = str_lit(" lives at ");
  parts[5] = level_to_string(heap, f->actual);
  parts[6] = str_lit(" and ");
  parts[7] = f->str;
  parts[8] = str_lit(" is declared at ");
  parts[9] = level_to_string(heap, f->level);
  return check_engine_require(kernel_bool_unbox(value), error_make(ERROR_INDEX_ABOVE_UNIVERSE, cats(heap, 10, parts)));
}

static Comp index_rules_after_actual(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  f->actual = value;
  return bind_frame(heap,
                    level_always_zero(f->level) ? pure_bool(true) : level_eq_decide_m(heap, f->actual, f->level, false),
                    index_rules_after_below, f);
}

static Comp index_rules_after_ok(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_univ(heap, f->ctx, f->binder->ty), index_rules_after_actual, f);
}

Comp check_index_rules(Heap *heap, Ctx ctx, Str family, const LevelVar *level, const TeleBinder *binder) {
  CheckFrame *f = frame_new(heap);
  Str parts[7];
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->str = family;
  f->level = level;
  f->binder = binder;
  parts[0] = str_lit("the index ");
  parts[1] = binder->name;
  parts[2] = str_lit(" of ");
  parts[3] = family;
  parts[4] = str_lit(" is at quantity ");
  parts[5] = quantity_to_string(binder->quantity);
  parts[6] = str_lit(" and every index binder is at 0");
  return bind_frame(heap,
                    check_engine_require(quantity_equal(binder->quantity, QUANTITY_ZERO),
                                         error_make(ERROR_INDEX_NOT_ZERO, cats(heap, 7, parts))),
                    index_rules_after_ok, f);
}

static Comp check_index_telescope_after_next(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Ctx *next = value;
  return check_check_index_telescope(heap, list_tail(f->vec), *next, f->str, f->level);
}

static Comp check_index_telescope_after_ok(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_check_telescope(heap, singleton(heap, (void *)f->binder), f->ctx),
                    check_index_telescope_after_next, f);
}

Comp check_check_index_telescope(Heap *heap, Vec tele, Ctx ctx, Str name, const LevelVar *level) {
  CheckFrame *f;
  const TeleBinder *binder;
  if (tele.len == 0) return pure_ctx(heap, ctx);
  binder = head_of(&tele);
  if (binder == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->ctx = ctx;
  f->str = name;
  f->level = level;
  f->binder = binder;
  return bind_frame(heap, check_index_rules(heap, ctx, name, level, binder), check_index_telescope_after_ok, f);
}

bool check_absent_family(MaybeFamily value) { return !value.some; }

static Comp declare_family_comp_after_ictx(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Family family;
  (void)value;
  family.name = f->fdecl.name;
  family.params = f->fdecl.params;
  family.indices = f->fdecl.indices;
  family.level = f->fdecl.level;
  family.status.kind = CTOR_STATUS_PROVISIONAL;
  family.status.names = vec_new(heap->arena);
  family.ctors = vec_new(heap->arena);
  family.positive = false;
  return pure_globals(heap, global_add_family(heap, f->fdecl.name, family, f->globals));
}

static Comp declare_family_comp_after_pctx(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Ctx *pctx = value;
  return bind_frame(heap, check_check_index_telescope(heap, f->fdecl.indices, *pctx, f->fdecl.name, f->fdecl.level),
                    declare_family_comp_after_ictx, f);
}

static Comp declare_family_comp_after_absent(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    check_check_telescope(heap, f->fdecl.params, check_make(heap, f->globals, budget_unlimited(), f->nat)),
                    declare_family_comp_after_pctx, f);
}

static Comp declare_family_comp_after_scope(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[3];
  (void)value;
  parts[0] = str_lit("the family ");
  parts[1] = f->fdecl.name;
  parts[2] = str_lit(" is already declared");
  return bind_frame(heap,
                    check_engine_require(check_absent_family(global_find_family(f->fdecl.name, f->globals)),
                                         error_make(ERROR_MISMATCH, cats(heap, 3, parts))),
                    declare_family_comp_after_absent, f);
}

static Comp declare_family_comp_after_tick(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str message = nat_is_zero(f->nat) ? str_lit("family universe levels must be closed at M0 Stage A")
                                    : str_lit("universe level is outside the global parameter scope");
  (void)value;
  return bind_frame(heap, check_engine_require(level_in_scope(f->nat, f->fdecl.level), error_make(ERROR_UNIVERSE, message)),
                    declare_family_comp_after_scope, f);
}

Comp check_declare_family_comp(Heap *heap, Globals globals, Nat arity, FamilyDecl decl) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->nat = arity;
  f->fdecl = decl;
  return bind_frame(heap, budget_tick(heap), declare_family_comp_after_tick, f);
}

Result check_declare_family_at(Heap *heap, Globals globals, Budget budget, Nat arity, FamilyDecl decl) {
  return budget_run(heap, check_declare_family_comp(heap, globals, arity, decl), budget);
}

Result check_declare_family(Heap *heap, Globals globals, Budget budget, FamilyDecl decl) {
  return check_declare_family_at(heap, globals, budget, nat_zero(), decl);
}

bool check_parameter_at(const Term *term, Nat index) {
  const Term *t = term;
  while (t->kind == TERM_ANN) t = t->body;
  if (t->kind == TERM_VAR) return nat_cmp(t->index, index) == 0;
  return false;
}

bool check_parameters_at(Heap *heap, const Vec *terms, Nat index) {
  Nat at = index;
  size_t i;
  for (i = 0; i < terms->len; i++) {
    VecGet got = vec_get(terms, i);
    at = pred(heap, at);
    if (got.status != VEC_GET_OK) continue;
    if (!check_parameter_at(got.value, at)) return false;
  }
  return true;
}

static Comp ctor_fields_after_next(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Ctx *next = value;
  return check_ctor_fields(heap, list_tail(f->vec), *next, f->str, f->level);
}

static Comp ctor_fields_after_ok(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_check_telescope(heap, singleton(heap, (void *)f->binder), f->ctx), ctor_fields_after_next,
                    f);
}

static Comp ctor_fields_after_below(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[3];
  parts[0] = str_lit("a field of ");
  parts[1] = f->str;
  parts[2] = str_lit(" exceeds its family universe");
  return bind_frame(heap,
                    check_engine_require(kernel_bool_unbox(value), error_make(ERROR_UNIVERSE, cats(heap, 3, parts))),
                    ctor_fields_after_ok, f);
}

static Comp ctor_fields_after_actual(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  return bind_frame(heap, level_eq_decide_m(heap, value, f->level, false), ctor_fields_after_below, f);
}

Comp check_ctor_fields(Heap *heap, Vec tele, Ctx ctx, Str name, const LevelVar *level) {
  CheckFrame *f;
  const TeleBinder *binder;
  if (tele.len == 0) return pure_ctx(heap, ctx);
  binder = head_of(&tele);
  if (binder == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->ctx = ctx;
  f->str = name;
  f->level = level;
  f->binder = binder;
  return bind_frame(heap, check_engine_infer_univ(heap, ctx, binder->ty), ctor_fields_after_actual, f);
}

static Comp check_ctor_comp_after_ienv(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Ctor ctor;
  (void)value;
  ctor.name = f->cdecl.name;
  ctor.args = f->cdecl.args;
  ctor.res_idx = f->cdecl.res_idx;
  ctor.full_arity = nat_of_len(heap, f->np + f->depth);
  ctor.self_rec = positivity_self_rec(heap, &f->cdecl.args, f->group);
  return pure_ctor(heap, ctor);
}

static Comp check_ctor_comp_after_count(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[2];
  (void)value;
  parts[0] = str_lit("the result indices of ");
  parts[1] = f->cdecl.name;
  return bind_frame(heap,
                    check_engine_telescope_uses(heap, *f->actx, QUANTITY_ZERO, cats(heap, 2, parts), &f->family.indices,
                                                &f->cdecl.res_idx, check_engine_env_values(*f->penv)),
                    check_ctor_comp_after_ienv, f);
}

static Comp check_ctor_comp_after_penv(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  size_t ni = f->family.indices.len;
  Str parts[7];
  f->penv = value;
  parts[0] = f->cdecl.name;
  parts[1] = str_lit(" gives ");
  parts[2] = heap_nat_show(heap, nat_of_len(heap, f->cdecl.res_idx.len));
  parts[3] = str_lit(" result indices and ");
  parts[4] = f->family.name;
  parts[5] = str_lit(" takes ");
  parts[6] = heap_nat_show(heap, nat_of_len(heap, ni));
  return bind_frame(heap,
                    check_engine_require(f->cdecl.res_idx.len == ni, error_make(ERROR_MISMATCH, cats(heap, 7, parts))),
                    check_ctor_comp_after_count, f);
}

static Comp check_ctor_comp_after_preserved(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[2];
  (void)value;
  parts[0] = str_lit("the parameters of ");
  parts[1] = f->cdecl.name;
  return bind_frame(heap,
                    check_engine_telescope_uses(heap, *f->actx, QUANTITY_ZERO, cats(heap, 2, parts), &f->family.params,
                                                &f->cdecl.res_params, vec_new(heap->arena)),
                    check_ctor_comp_after_penv, f);
}

static Comp check_ctor_comp_after_positive(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Str parts[3];
  (void)value;
  f->np = f->family.params.len;
  f->depth = f->cdecl.args.len;
  parts[0] = str_lit("the constructor ");
  parts[1] = f->cdecl.name;
  parts[2] = str_lit(" must preserve its family parameters");
  return bind_frame(heap,
                    check_engine_require(f->cdecl.res_params.len == f->np &&
                                             check_parameters_at(heap, &f->cdecl.res_params,
                                                                 nat_of_len(heap, f->depth + f->np)),
                                         error_make(ERROR_MISMATCH, cats(heap, 3, parts))),
                    check_ctor_comp_after_preserved, f);
}

static Comp check_ctor_comp_after_actx(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  f->actx = value;
  return bind_frame(heap, budget_lift(positivity_ctor_fields(heap, f->group, &f->cdecl.args)),
                    check_ctor_comp_after_positive, f);
}

Comp check_check_ctor_comp(Heap *heap, Ctx ctx, Family family, const Vec *group, CtorDecl decl) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->family = family;
  f->group = group;
  f->cdecl = decl;
  return bind_frame(heap, check_ctor_fields(heap, decl.args, ctx, decl.name, family.level), check_ctor_comp_after_actx, f);
}

bool check_provisional(Family family) { return family.status.kind == CTOR_STATUS_PROVISIONAL; }

static Comp check_ctors_comp_after_rest(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  Vec *rest = value;
  return pure_vec(heap, cons(heap, (void *)f->ctor, rest));
}

static Comp check_ctors_comp_after_ctor(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  f->ctor = value;
  return bind_frame(heap, check_check_ctors_comp(heap, list_tail(f->vec), f->ctx, f->family, f->group),
                    check_ctors_comp_after_rest, f);
}

static Comp check_ctors_comp_after_tick(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap, check_check_ctor_comp(heap, f->ctx, f->family, f->group, f->cdecl), check_ctors_comp_after_ctor,
                    f);
}

Comp check_check_ctors_comp(Heap *heap, Vec ctors, Ctx ctx, Family family, const Vec *group) {
  CheckFrame *f;
  const CtorDecl *decl;
  if (ctors.len == 0) return pure_vec(heap, vec_new(heap->arena));
  decl = head_of(&ctors);
  if (decl == NULL) return comp_oom();
  f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->vec = ctors;
  f->ctx = ctx;
  f->family = family;
  f->group = group;
  f->cdecl = *decl;
  return bind_frame(heap, budget_tick(heap), check_ctors_comp_after_tick, f);
}

Vec check_ctor_names(Heap *heap, const Vec *ctors) {
  Vec out = vec_new(heap->arena);
  size_t i;
  for (i = 0; i < ctors->len; i++) {
    VecGet got = vec_get(ctors, i);
    Ctor *ctor = got.value;
    if (got.status != VEC_GET_OK) continue;
    if (!vec_push(&out, &ctor->name)) heap->oom = true;
  }
  return out;
}

Family check_completed(Heap *heap, Family family, Vec ctors) {
  Family out = family;
  out.status.kind = CTOR_STATUS_COMPLETE;
  out.status.names = check_ctor_names(heap, &ctors);
  out.ctors = ctors;
  out.positive = true;
  return out;
}

static Comp define_ctors_comp_after_ctors(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Vec *ctors = value;
  return pure_globals(heap, global_add_family(heap, f->str, check_completed(heap, f->family, *ctors), f->globals));
}

static Comp define_ctors_comp_after_ctx(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Ctx *c = value;
  return bind_frame(heap, check_check_ctors_comp(heap, f->vec, *c, f->family, f->group), define_ctors_comp_after_ctors, f);
}

static Comp define_ctors_comp_after_state(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    check_check_telescope(heap, check_engine_fam_params(f->family),
                                          check_make(heap, f->globals, budget_unlimited(), f->nat)),
                    define_ctors_comp_after_ctx, f);
}

static Comp define_ctors_comp_after_family(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Family *family = value;
  Str parts[3];
  f->family = *family;
  parts[0] = str_lit("the constructors of ");
  parts[1] = f->str;
  parts[2] = str_lit(" are already installed");
  return bind_frame(heap,
                    check_engine_require(check_provisional(f->family), error_make(ERROR_MISMATCH, cats(heap, 3, parts))),
                    define_ctors_comp_after_state, f);
}

static Comp define_ctors_comp_after_tick(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  MaybeFamily found = global_find_family(f->str, f->globals);
  Family *boxed = NULL;
  Str parts[3];
  (void)value;
  if (found.some) {
    boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) return comp_oom();
    *boxed = found.value;
  }
  parts[0] = str_lit("the family ");
  parts[1] = f->str;
  parts[2] = str_lit(" is not declared");
  return bind_frame(heap, check_engine_optional(found.some, boxed, error_make(ERROR_UNBOUND, cats(heap, 3, parts))),
                    define_ctors_comp_after_family, f);
}

Comp check_define_ctors_comp(Heap *heap, Globals globals, Nat arity, const Vec *group, Str name, Vec ctors) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->nat = arity;
  f->group = group;
  f->str = name;
  f->vec = ctors;
  return bind_frame(heap, budget_tick(heap), define_ctors_comp_after_tick, f);
}

Result check_define_ctors_at(Heap *heap, Globals globals, Budget budget, Nat arity, const Vec *group, Str name,
                             Vec ctors) {
  return budget_run(heap, check_define_ctors_comp(heap, globals, arity, group, name, ctors), budget);
}

Result check_define_ctors(Heap *heap, Globals globals, Budget budget, const Vec *group, Str name, Vec ctors) {
  return check_define_ctors_at(heap, globals, budget, nat_zero(), group, name, ctors);
}

Str check_family_decl_name(FamilyDecl decl) { return decl.name; }

const LevelVar *check_family_decl_level(FamilyDecl decl) { return decl.level; }

static Comp check_family_scheme_comp_after_defined(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

static Comp check_family_scheme_comp_after_stable(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  (void)value;
  f->str = check_family_decl_name(f->fdecl);
  f->names = singleton(heap, &f->str);
  return bind_frame(heap, check_define_ctors_comp(heap, f->globals, f->nat, &f->names, f->str, f->vec),
                    check_family_scheme_comp_after_defined, f);
}

static Comp check_family_scheme_comp_after_globals(Heap *heap, void *ctx, void *value) {
  CheckFrame *f = ctx;
  const Globals *globals = value;
  f->globals = *globals;
  f->level = check_family_decl_level(f->fdecl);
  return bind_frame(
      heap,
      check_engine_require(
          level_always_zero(f->level) || level_always_positive(f->level),
          error_make(ERROR_UNIVERSE,
                     str_lit("a family schema must remain in Prop or Type for every universe substitution"))),
      check_family_scheme_comp_after_stable, f);
}

Comp check_check_family_scheme_comp(Heap *heap, Globals globals, Nat arity, FamilyDecl decl, Vec ctors) {
  CheckFrame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->nat = arity;
  f->fdecl = decl;
  f->vec = ctors;
  return bind_frame(heap, check_declare_family_comp(heap, globals, arity, decl), check_family_scheme_comp_after_globals,
                    f);
}

Result check_check_family_scheme(Heap *heap, Globals globals, Budget budget, Nat arity, FamilyDecl decl, Vec ctors) {
  return budget_run(heap, check_check_family_scheme_comp(heap, globals, arity, decl, ctors), budget);
}

Result check_infer_term(Heap *heap, Globals globals, Budget budget, const Term *term) {
  return check_infer(heap, check_make(heap, globals, budget, nat_zero()), QUANTITY_MANY, term);
}

Result check_check_term(Heap *heap, Globals globals, Budget budget, const Term *term, const Value *ty) {
  return check_check(heap, check_make(heap, globals, budget, nat_zero()), QUANTITY_MANY, term, ty);
}
