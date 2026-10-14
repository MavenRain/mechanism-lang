/* Port of bend2/kernel/check_engine.bend: the bidirectional type checker. Comp values are
 * Inferred *, Former *, Point *, Usage *, Ctx *, Big *, const Value *, const Term *,
 * const LevelVar *, kernel_unit() and the kernel bool box. The Bend do blocks become
 * budget_bind chains; each continuation keeps its free variables in a heap allocated Frame. */
#include "kernel.h"

#include <stddef.h>

/* ---- host helpers (not in the Bend source) ---- */

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static NextK next_k(Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  next.fn = fn;
  next.ctx = ctx;
  return next;
}

/* The free variables of one continuation; a zeroed Frame is allocated per bind.
 * A Comp can run again, so its captured frame stays live until the arena is freed. */
typedef struct {
  Ctx ctx;
  const Value * value;
  Quantity mode;
  const Term * term;
  Vec vec;
  Str name;
  const Value * value2;
  Usage uses;
  Former former;
  Vec vec2;
  const Term * term2;
  const Vec * items;
  Shape shape;
  Big big;
  Nat nat;
  Inferred inferred;
  bool flag;
  const Value * value3;
  Vec vec3;
  ExpectedValue expected;
  Point point;
  Closure clo;
  Nat nat2;
  const ElimData * data;
  Quantity q;
  const LevelVar * level;
  Family family;
  Str name2;
  const Value * value4;
  Motive mot;
  Vec vec4;
  Addr addr;
  const Term * term3;
  Ctx inner;
  Leg leg;
  Big big2;
  Mpc mpc;
  Str message;
  Binder binder;
  MaybeMotive motive;
  const Value * value5;
  VAddr vaddr;
  MaybeLevelVar expected_level;
  Ctor ctor;
  Mu mu;
  Vec vec5;
  Usage uses2;
  Big big3;
  Inferred inferred2;
  Closure clo2;
  VAddr vaddr2;
  Shape shape2;
  const Locals * locals;
  Binder binder2;
  Nat nat3;
  Fields fields;
  ProofArgs proof;
  const LevelVar * level2;
  Quantity q2;
  Former former2;
  const Term * term4;
  StuckElim stuck;
  StuckElim stuck2;
  Leg leg2;
  MaybeLevelVar expected_level2;
  const Vec * items2;
} Frame;

/* Each bind allocates only the prefix of Frame up to the last field that its continuation chain
 * touches (hop 232: a full Frame is 3504 bytes and the arena never frees it inside one declaration).
 * The fields are ordered by the number of chains that use them. */
#define FRAME_UPTO(field) (offsetof(Frame, field) + sizeof(((Frame *)0)->field))

static Frame *frame_new_upto(Heap *heap, size_t size) { return heap_alloc(heap, size); }

static Frame *frame_new(Heap *heap) { return heap_alloc(heap, sizeof(Frame)); }

static Comp bind_frame(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), Frame *frame) {
  if (frame == NULL) return comp_oom();
  return budget_bind(heap, first, next_k(fn, frame));
}

/* Nat.sub: natural subtraction saturates at zero. */
static Nat sub(Heap *heap, Nat a, Nat b) {
  NatSub out = nat_sub(heap->arena, a, b);
  switch (out.status) {
  case NAT_SUB_OK:
    return out.value;
  case NAT_SUB_UNDERFLOW:
    return nat_zero();
  case NAT_SUB_OOM:
    heap->oom = true;
    return nat_zero();
  }
  return nat_zero();
}

static Nat pred(Heap *heap, Nat n) { return sub(heap, n, heap_nat_of_u64(heap, 1)); }

static bool nat_lt(Nat a, Nat b) { return nat_cmp(a, b) < 0; }

static Comp pure_inferred(Heap *heap, const Value *ty, Usage uses) {
  Inferred *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->ty = ty;
  boxed->uses = uses;
  return budget_pure(boxed);
}

static Comp pure_usage(Heap *heap, Usage uses) {
  Usage *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = uses;
  return budget_pure(boxed);
}

static Comp pure_ctx(Heap *heap, Ctx ctx) {
  Ctx *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ctx;
  return budget_pure(boxed);
}

static Comp pure_big(Heap *heap, Big value) {
  Big *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = value;
  return budget_pure(boxed);
}

static Comp pure_unit(void) { return budget_pure(kernel_unit()); }

static Error error_text(ErrorKind kind, const char *text) { return error_make(kind, str_lit(text)); }

static Str cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }

static MaybeLevelVar level_some(const LevelVar *level) {
  MaybeLevelVar out;
  out.some = true;
  out.value = level;
  return out;
}

static MaybeLevelVar level_none(void) {
  MaybeLevelVar out;
  out.some = false;
  out.value = NULL;
  return out;
}

static ExpectedValue expected_some(const Value *value) {
  ExpectedValue out;
  out.some = true;
  out.value = value;
  return out;
}

static ExpectedValue expected_none(void) {
  ExpectedValue out;
  out.some = false;
  out.value = NULL;
  return out;
}

/* ---- the Bend definitions in order ---- */

Comp check_engine_require(bool valid, Error error) {
  if (valid) return pure_unit();
  return budget_failed(error);
}

Comp check_engine_optional(bool some, void *value, Error error) {
  if (some) return budget_pure(value);
  return budget_failed(error);
}

Comp check_engine_require_by(Heap *heap, bool valid, ErrorThunk error) {
  if (valid) return pure_unit();
  return budget_failed(error.fn(heap, error.ctx));
}

Comp check_engine_optional_by(Heap *heap, bool some, void *value, ErrorThunk error) {
  if (some) return budget_pure(value);
  return budget_failed(error.fn(heap, error.ctx));
}

const Value *check_engine_inferred_ty(Inferred value) { return value.ty; }

Usage check_engine_inferred_uses(Inferred value) { return value.uses; }

Comp check_engine_eval(Heap *heap, Ctx ctx, const Term *term) {
  return budget_lift(eval_eval(heap, context_globals_of(ctx), context_env_of(ctx), term));
}

Comp check_engine_whnf(Heap *heap, Ctx ctx, const Value *value) {
  return budget_lift(eval_whnf(heap, context_globals_of(ctx), value));
}

Comp check_engine_whnf_core(Heap *heap, Ctx ctx, const Value *value) {
  return budget_lift(eval_whnf_core(heap, context_globals_of(ctx), value));
}

Comp check_engine_unfold_step(Heap *heap, Ctx ctx, const Value *value) {
  MaybeValueResult result = eval_unfold_step(heap, context_globals_of(ctx), value);
  switch (result.kind) {
  case RESULT_DONE: {
    ExpectedValue *boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) return comp_oom();
    boxed->some = result.value.some;
    boxed->value = result.value.value;
    return budget_pure(boxed);
  }
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return comp_oom();
}

Comp check_engine_open(Heap *heap, Ctx ctx, Closure clo, Vec args) {
  return budget_lift(eval_open_closure(heap, context_globals_of(ctx), clo, args));
}

Comp check_engine_quote(Heap *heap, Ctx ctx, const Value *value) {
  return budget_lift(eval_quote(heap, context_globals_of(ctx), context_size_of(ctx), value));
}

Str check_engine_pp_result(Heap *heap, Result result, const Vec *names) {
  switch (result.kind) {
  case RESULT_FAIL:
    return str_lit("a value that does not read back");
  case RESULT_DONE:
    return pp_term(heap, names, (const Term *)result.value);
  }
  return str_lit("a value that does not read back");
}

Str check_engine_pp_value(Heap *heap, Ctx ctx, const Value *value) {
  Vec names = context_names_of(heap, ctx);
  return check_engine_pp_result(heap, eval_normalize(heap, context_globals_of(ctx), context_size_of(ctx), value),
                                &names);
}

Comp check_engine_check_scope_node(Heap *heap, const Term *term, Nat arity) {
  (void)heap;
  switch (term->kind) {
  case TERM_UNIV:
    return check_engine_require(level_in_scope(arity, term->level),
                                error_text(ERROR_UNIVERSE, "universe level is outside the global parameter scope"));
  case TERM_VAR:
  case TERM_LAN:
  case TERM_RAN:
  case TERM_IN:
  case TERM_ELIM:
  case TERM_SEC:
  case TERM_OUT:
  case TERM_LET:
  case TERM_ANN:
  case TERM_GLOBAL:
  case TERM_LIT:
  case TERM_AUTO:
    return pure_unit();
  }
  return pure_unit();
}

static Comp scope_walk_after_valid(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Vec children = term_children(heap, f->term);
  (void)value;
  return check_engine_scope_walk(heap, vec_concat(heap->arena, &children, &f->vec), f->nat);
}

static Comp scope_walk_after_tick(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_check_scope_node(heap, f->term, f->nat), scope_walk_after_valid, f);
}

Comp check_engine_scope_walk(Heap *heap, Vec todo, Nat arity) {
  VecGet head = vec_get(&todo, 0);
  Frame *f;
  switch (head.status) {
  case VEC_GET_OUT_OF_RANGE:
    return pure_unit();
  case VEC_GET_OK:
    break;
  }
  f = frame_new_upto(heap, FRAME_UPTO(nat));
  if (f == NULL) return comp_oom();
  f->term = head.value;
  f->vec = todo;
  f->vec.items = todo.items + 1;
  f->vec.len = todo.len - 1;
  f->nat = arity;
  return bind_frame(heap, budget_tick(heap), scope_walk_after_tick, f);
}

static Comp scope_after_walk(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Ctx checked = f->ctx;
  (void)value;
  checked.levels_checked = true;
  return pure_ctx(heap, checked);
}

Comp check_engine_scope(Heap *heap, Ctx ctx, const Term *term) {
  Vec todo;
  Frame *f;
  if (ctx.levels_checked) return pure_ctx(heap, ctx);
  todo = vec_new(heap->arena);
  if (!vec_push(&todo, (void *)term)) return comp_oom();
  f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, check_engine_scope_walk(heap, todo, ctx.level_arity), scope_after_walk, f);
}

bool check_engine_close_valid(bool is_outer, Quantity mode, Quantity q, bool affine, Nat level, Usage uses) {
  if (is_outer) return true;
  switch (mode) {
  case QUANTITY_ZERO:
    return true;
  case QUANTITY_ONE:
  case QUANTITY_MANY:
    break;
  }
  switch (q) {
  case QUANTITY_ZERO:
    return true;
  case QUANTITY_MANY:
    return true;
  case QUANTITY_ONE:
    if (affine) return quantity_at_most_once(level, uses);
    return quantity_exactly_once(level, uses);
  }
  return true;
}

Comp check_engine_close_local(Heap *heap, bool valid, Str name, bool affine) {
  if (valid) return pure_unit();
  if (affine) {
    return budget_failed(error_make(ERROR_QUANTITY, cat3(heap, str_lit("the linear alias "), name,
                                                         str_lit(" may be used at most once"))));
  }
  return budget_failed(error_make(
      ERROR_QUANTITY,
      cat3(heap, str_lit("the linear binder "), name, str_lit(" must be used exactly once on every runtime path"))));
}

static Comp close_continue_after_local(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_close_loop(heap, context_lview(heap, f->locals), f->nat, f->nat2, f->mode, f->flag,
                                 quantity_remove(heap, f->uses, f->nat));
}

Comp check_engine_close_continue(Heap *heap, bool outside, const Locals *rest, Nat level, Nat outer, Quantity mode,
                                 bool affine, Usage uses, Str name, Quantity q) {
  Frame *f;
  if (outside) return pure_usage(heap, uses);
  f = frame_new_upto(heap, FRAME_UPTO(locals));
  if (f == NULL) return comp_oom();
  f->locals = rest;
  f->nat = level;
  f->nat2 = outer;
  f->mode = mode;
  f->flag = affine;
  f->uses = uses;
  return bind_frame(heap,
                    check_engine_close_local(heap, check_engine_close_valid(false, mode, q, affine, level, uses), name,
                                             affine),
                    close_continue_after_local, f);
}

Comp check_engine_close_loop(Heap *heap, LView locals, Nat size, Nat outer, Quantity mode, bool affine, Usage uses) {
  Nat level;
  switch (locals.kind) {
  case LVIEW_NIL:
    return pure_usage(heap, uses);
  case LVIEW_CONS:
    break;
  }
  if (nat_is_zero(size)) return pure_usage(heap, uses);
  level = pred(heap, size);
  return check_engine_close_continue(heap, nat_lt(level, outer), locals.tail, level, outer, mode, affine, uses,
                                     locals.head.name, locals.head.quantity);
}

Comp check_engine_close(Heap *heap, Ctx ctx, Nat outer, Quantity mode, Usage uses, bool affine) {
  return check_engine_close_loop(heap, context_lview(heap, ctx.locals), ctx.size, outer, mode, affine, uses);
}

Comp check_engine_head_local(MaybeLocal value) {
  if (value.some) return budget_pure((void *)value.value.ty);
  return budget_failed(error_text(ERROR_UNBOUND, "a local level is outside the context"));
}

Comp check_engine_head_global(Heap *heap, MaybeEntry entry, Globals globals, Str name) {
  if (!entry.some) return budget_failed(error_make(ERROR_UNBOUND, name));
  return budget_lift(eval_eval(heap, globals, vec_new(heap->arena), global_entry_ty(entry.value)));
}

Comp check_engine_head_local_bounded(Heap *heap, bool in_bounds, const Locals *locals, Nat size, Nat level) {
  MaybeLocal none;
  if (in_bounds) {
    return check_engine_head_local(context_lget(heap, locals, sub(heap, size, heap_nat_succ(heap, level))));
  }
  none.some = false;
  none.value = context_local(str_empty(), QUANTITY_ZERO, NULL);
  return check_engine_head_local(none);
}

Comp check_engine_head_ty(Heap *heap, Ctx ctx, Head head) {
  switch (head.kind) {
  case HEAD_LOCAL:
    return check_engine_head_local_bounded(heap, nat_lt(head.level, ctx.size), ctx.locals, ctx.size, head.level);
  case HEAD_GLOBAL:
    return check_engine_head_global(heap, global_find(head.name, ctx.globals), ctx.globals, head.name);
  }
  return comp_oom();
}

Comp check_engine_universe_value(Heap *heap, const Value *value, Ctx ctx) {
  switch (value->kind) {
  case VALUE_UNIV:
    return budget_pure((void *)value->level);
  case VALUE_LAN:
  case VALUE_RAN:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    break;
  }
  return budget_failed(error_make(ERROR_UNIVERSE, heap_str_cat(heap, str_lit("a term used as a type is not a universe:  "),
                                                                check_engine_pp_value(heap, ctx, value))));
}

static Comp infer_univ_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_universe_value(heap, value, f->ctx);
}

static Comp infer_univ_after_infer(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  return bind_frame(heap, check_engine_whnf(heap, f->ctx, check_engine_inferred_ty(*got)), infer_univ_after_whnf, f);
}

Comp check_engine_infer_univ(Heap *heap, Ctx ctx, const Term *term) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, check_engine_infer_uses(heap, ctx, QUANTITY_ZERO, term), infer_univ_after_infer, f);
}

static Error local_infer_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_QUANTITY, cat3(heap, str_lit("the erased binder "), f->name,
                                         str_lit(" is read in a runtime position")));
}

static Comp local_infer_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  /* tcc 0.9.28rc (arm64) clobbers the pending heap->arena register when a two-register struct
   * argument is loaded inside a nested call: hoist the inner call into a local. */
  Nat next = heap_nat_succ(heap, f->nat);
  (void)value;
  return pure_inferred(heap, f->value, quantity_occurrence(heap, sub(heap, f->nat2, next), f->mode));
}

Comp check_engine_local_infer(Heap *heap, MaybeLocal local, Nat index, Nat size, Quantity mode) {
  Frame *f;
  ErrorThunk thunk;
  if (!local.some) {
    return budget_failed(error_make(
        ERROR_UNBOUND, cat3(heap, str_lit("de Bruijn index "), heap_nat_show(heap, index), str_lit(" is outside the context"))));
  }
  f = frame_new_upto(heap, FRAME_UPTO(nat2));
  if (f == NULL) return comp_oom();
  f->name = local.value.name;
  f->value = local.value.ty;
  f->nat = index;
  f->nat2 = size;
  f->mode = mode;
  thunk.fn = local_infer_error;
  thunk.ctx = f;
  return bind_frame(heap, check_engine_require_by(heap, context_readable(mode, local.value.quantity), thunk),
                    local_infer_after_require, f);
}

Comp check_engine_infer_var(Heap *heap, Ctx ctx, Quantity mode, Nat index) {
  return check_engine_local_infer(heap, context_lget(heap, ctx.locals, index), index, ctx.size, mode);
}

static Comp infer_ann_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_inferred(heap, f->value, *uses);
}

Comp check_engine_infer_ann(Heap *heap, Ctx ctx, Quantity mode, const Term *body, const Value *ty) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value));
  if (f == NULL) return comp_oom();
  f->value = ty;
  return bind_frame(heap, check_engine_check_uses(heap, ctx, mode, body, ty), infer_ann_after_check, f);
}

static Comp infer_node_univ_of_level(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_inferred(heap, value_vuniv(heap, value), quantity_empty(heap));
}

static Comp infer_node_ann_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_infer_ann(heap, f->ctx, f->mode, f->term, value);
}

static Comp infer_node_ann_after_univ(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term2), infer_node_ann_after_eval, f);
}

static Comp infer_node_global_after_head(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_inferred(heap, value, quantity_empty(heap));
}

static Comp infer_node_lit_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    budget_lift(eval_eval(heap, context_globals_of(f->ctx), vec_new(heap->arena), prim_nat_ty(heap))),
                    infer_node_global_after_head, f);
}

static Comp infer_node_lit(Heap *heap, Ctx ctx, Literal lit) {
  Frame *f;
  switch (lit.kind) {
  case LITERAL_INT:
    f = frame_new_upto(heap, FRAME_UPTO(ctx));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    return bind_frame(heap,
                      check_engine_require(big_le(big_zero(), lit.integer),
                                           error_text(ERROR_MISMATCH, "a Nat literal must be nonnegative")),
                      infer_node_lit_after_require, f);
  case LITERAL_STRING:
    return budget_failed(error_text(ERROR_NOT_YET, "string types arrive at M1"));
  }
  return comp_oom();
}

Comp check_engine_infer_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  Frame *f;
  switch (term->kind) {
  case TERM_VAR:
    return check_engine_infer_var(heap, ctx, mode, term->index);
  case TERM_UNIV:
    return pure_inferred(heap, value_vuniv(heap, level_succ(heap, term->level)), quantity_empty(heap));
  case TERM_LAN:
    return bind_frame(heap, check_engine_form(heap, ctx, true, term->shape, term->body, level_none()),
                      infer_node_univ_of_level, frame_new(heap));
  case TERM_RAN:
    return bind_frame(heap, check_engine_form(heap, ctx, false, term->shape, term->body, level_none()),
                      infer_node_univ_of_level, frame_new(heap));
  case TERM_OUT:
    return check_engine_elim_out(heap, ctx, mode, term->shape, term->addr, term->head);
  case TERM_ELIM:
    return check_engine_elim_elim(heap, ctx, mode, term->data, expected_none());
  case TERM_LET:
    return check_engine_let_body(heap, ctx, mode, term->name, term->ty, term->value, term->body, expected_none());
  case TERM_ANN:
    f = frame_new_upto(heap, FRAME_UPTO(term2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->term = term->body;
    f->term2 = term->ty;
    return bind_frame(heap, check_engine_infer_univ(heap, ctx, term->ty), infer_node_ann_after_univ, f);
  case TERM_GLOBAL:
    return bind_frame(heap, check_engine_head_ty(heap, ctx, value_hglobal(term->name)), infer_node_global_after_head,
                      frame_new(heap));
  case TERM_LIT:
    return infer_node_lit(heap, ctx, term->lit);
  case TERM_IN:
    return budget_failed(
        error_text(ERROR_CANNOT_INFER, "an injection has no type of its own;  it needs an expected type"));
  case TERM_SEC:
    return budget_failed(
        error_text(ERROR_CANNOT_INFER, "a section has no type of its own;  it needs an expected type"));
  case TERM_AUTO:
    return budget_failed(error_text(ERROR_NOT_YET, "instances arrive at M2"));
  }
  return comp_oom();
}

static Comp infer_scoped_after_scope(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctx *inner = value;
  return check_engine_infer_node(heap, *inner, f->mode, f->term);
}

Comp check_engine_infer_scoped(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->mode = mode;
  f->term = term;
  return bind_frame(heap, check_engine_scope(heap, ctx, term), infer_scoped_after_scope, f);
}

static Comp infer_uses_after_tick(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_infer_scoped(heap, f->ctx, f->mode, f->term);
}

Comp check_engine_infer_uses(Heap *heap, Ctx ctx, Quantity mode, const Term *term) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->term = term;
  return bind_frame(heap, budget_tick(heap), infer_uses_after_tick, f);
}

Comp check_engine_ensure_result(Heap *heap, bool same, Ctx ctx, const Value *got, const Value *expected) {
  if (same) return pure_unit();
  return budget_failed(error_make(
      ERROR_MISMATCH, heap_str_cat(heap,
                                   cat3(heap, str_lit("the term has type "), check_engine_pp_value(heap, ctx, got),
                                        str_lit(" and the expected type is ")),
                                   check_engine_pp_value(heap, ctx, expected))));
}

static Comp ensure_after_conv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_ensure_result(heap, kernel_bool_unbox(value), f->ctx, f->value, f->value2);
}

Comp check_engine_ensure(Heap *heap, Ctx ctx, const Value *got, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = got;
  f->value2 = expected;
  return bind_frame(heap, check_engine_conv_type(heap, ctx, got, expected), ensure_after_conv, f);
}

static Comp check_inferred_after_ensure(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_usage(heap, f->uses);
}

Comp check_engine_check_inferred(Heap *heap, Inferred got, Ctx ctx, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(uses));
  if (f == NULL) return comp_oom();
  f->uses = got.uses;
  return bind_frame(heap, check_engine_ensure(heap, ctx, got.ty, expected), check_inferred_after_ensure, f);
}

Comp check_engine_former_equal_result(Heap *heap, bool same, const LevelVar *got, const LevelVar *expected) {
  if (same) return pure_unit();
  return budget_failed(error_make(
      ERROR_UNIVERSE, heap_str_cat(heap,
                                   cat3(heap, str_lit("the former lives at "), level_to_string(heap, got),
                                        str_lit(" and the expected universe is ")),
                                   level_to_string(heap, expected))));
}

static Comp check_former_equal_after_decide(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_former_equal_result(heap, kernel_bool_unbox(value), f->level, (const LevelVar *)f->value);
}

Comp check_engine_check_former_equal(Heap *heap, const LevelVar *got, const LevelVar *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(level));
  if (f == NULL) return comp_oom();
  f->level = got;
  f->value = (const Value *)expected;
  return bind_frame(heap, level_eq_decide_m(heap, got, expected, true), check_former_equal_after_decide, f);
}

static Comp check_former_level_after_form(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_check_former_equal(heap, value, f->level);
}

Comp check_engine_check_former_level(Heap *heap, const LevelVar *level, Ctx ctx, bool left, Shape shape,
                                     const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(level));
  if (f == NULL) return comp_oom();
  f->level = level;
  return bind_frame(heap, check_engine_form(heap, ctx, left, shape, diagram, level_some(level)),
                    check_former_level_after_form, f);
}

Comp check_engine_check_former_value(Heap *heap, const Value *expected, Ctx ctx, bool left, Shape shape,
                                     const Term *diagram) {
  switch (expected->kind) {
  case VALUE_UNIV:
    return check_engine_check_former_level(heap, expected->level, ctx, left, shape, diagram);
  case VALUE_LAN:
  case VALUE_RAN:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    break;
  }
  return budget_failed(error_make(
      ERROR_UNIVERSE, heap_str_cat(heap, str_lit("a type former is checked against a type that is not a universe:  "),
                                   check_engine_pp_value(heap, ctx, expected))));
}

static Comp check_former_after_value(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_usage(heap, quantity_empty(heap));
}

static Comp check_former_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_former_value(heap, value, f->ctx, f->flag, f->shape, f->term),
                    check_former_after_value, f);
}

Comp check_engine_check_former(Heap *heap, Ctx ctx, bool left, Shape shape, const Term *diagram,
                               const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(flag));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->flag = left;
  f->shape = shape;
  f->term = diagram;
  return bind_frame(heap, check_engine_whnf(heap, ctx, expected), check_former_after_whnf, f);
}

static Comp check_node_after_inferred(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  return check_engine_check_inferred(heap, *got, f->ctx, f->value);
}

static Comp check_node_let_after_body(Heap *heap, void *ctx, void *value) {
  const Inferred *got = value;
  (void)ctx;
  return pure_usage(heap, check_engine_inferred_uses(*got));
}

Comp check_engine_check_node(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected) {
  Frame *f;
  switch (term->kind) {
  case TERM_SEC:
    return check_engine_intro_sec(heap, ctx, mode, term->shape, &term->legs, expected);
  case TERM_IN:
    return check_engine_intro_in(heap, ctx, mode, term->shape, term->addr, &term->args, expected);
  case TERM_ELIM:
    f = frame_new_upto(heap, FRAME_UPTO(value));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->value = expected;
    return bind_frame(heap, check_engine_elim_elim(heap, ctx, mode, term->data, expected_some(expected)),
                      check_node_after_inferred, f);
  case TERM_LAN:
    return check_engine_check_former(heap, ctx, true, term->shape, term->body, expected);
  case TERM_RAN:
    return check_engine_check_former(heap, ctx, false, term->shape, term->body, expected);
  case TERM_LET:
    return bind_frame(heap,
                      check_engine_let_body(heap, ctx, mode, term->name, term->ty, term->value, term->body,
                                            expected_some(expected)),
                      check_node_let_after_body, frame_new(heap));
  case TERM_VAR:
  case TERM_UNIV:
  case TERM_OUT:
  case TERM_ANN:
  case TERM_GLOBAL:
  case TERM_LIT:
  case TERM_AUTO:
    break;
  }
  f = frame_new_upto(heap, FRAME_UPTO(value));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = expected;
  return bind_frame(heap, check_engine_infer_uses(heap, ctx, mode, term), check_node_after_inferred, f);
}

static Comp check_uses_after_scope(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctx *inner = value;
  return check_engine_check_node(heap, *inner, f->mode, f->term, f->value);
}

Comp check_engine_check_uses(Heap *heap, Ctx ctx, Quantity mode, const Term *term, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->mode = mode;
  f->term = term;
  f->value = expected;
  return bind_frame(heap, check_engine_scope(heap, ctx, term), check_uses_after_scope, f);
}

bool check_engine_linear_used(Heap *heap, LView locals, Nat size, Usage uses) {
  Nat level;
  bool here;
  switch (locals.kind) {
  case LVIEW_NIL:
    return false;
  case LVIEW_CONS:
    break;
  }
  if (nat_is_zero(size)) return false;
  level = pred(heap, size);
  here = quantity_equal(locals.head.quantity, QUANTITY_ONE) && quantity_used(level, uses);
  return here || check_engine_linear_used(heap, context_lview(heap, locals.tail), level, uses);
}

bool check_engine_has_linear(Heap *heap, Ctx ctx, Usage uses) {
  return check_engine_linear_used(heap, context_lview(heap, ctx.locals), ctx.size, uses);
}

static Comp let_finish_after_close(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *free = value;
  return pure_inferred(heap, f->value, quantity_sequence(heap, quantity_scale(heap, f->mode, f->uses), *free));
}

Comp check_engine_let_finish(Heap *heap, Inferred result, Ctx inner, Nat size, Quantity mode, Usage def_uses,
                             bool linear) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(uses));
  if (f == NULL) return comp_oom();
  f->value = result.ty;
  f->mode = mode;
  f->uses = def_uses;
  return bind_frame(heap, check_engine_close(heap, inner, size, mode, result.uses, linear), let_finish_after_close, f);
}

static Comp let_inner_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_inferred(heap, f->value, *uses);
}

Comp check_engine_let_inner(Heap *heap, ExpectedValue expected, Ctx inner, Quantity mode, const Term *body) {
  Frame *f;
  if (!expected.some) return check_engine_infer_uses(heap, inner, mode, body);
  f = frame_new_upto(heap, FRAME_UPTO(value));
  if (f == NULL) return comp_oom();
  f->value = expected.value;
  return bind_frame(heap, check_engine_check_uses(heap, inner, mode, body, expected.value), let_inner_after_check, f);
}

static Comp let_defined_after_inner(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *result = value;
  return check_engine_let_finish(heap, *result, f->ctx, f->nat, f->mode, f->uses, f->flag);
}

Comp check_engine_let_defined(Heap *heap, Ctx ctx, Quantity mode, Str name, const Value *ty, const Value *value,
                              const Term *body, ExpectedValue expected, Usage uses) {
  bool linear = check_engine_has_linear(heap, ctx, uses);
  Quantity q = quantity_equal(mode, QUANTITY_ZERO) ? QUANTITY_ZERO : (linear ? QUANTITY_ONE : QUANTITY_MANY);
  Ctx inner = context_define(heap, name, q, ty, value, ctx);
  Frame *f = frame_new_upto(heap, FRAME_UPTO(flag));
  if (f == NULL) return comp_oom();
  f->ctx = inner;
  f->nat = context_size_of(ctx);
  f->mode = mode;
  f->uses = uses;
  f->flag = linear;
  return bind_frame(heap, check_engine_let_inner(heap, expected, inner, mode, body), let_defined_after_inner, f);
}

static Comp let_typed_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_let_defined(heap, f->ctx, f->mode, f->name, f->value, value, f->term, f->expected, f->uses);
}

static Comp let_typed_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term2), let_typed_after_eval, f);
}

Comp check_engine_let_typed(Heap *heap, Ctx ctx, Quantity mode, Str name, const Value *ty, const Term *value,
                            const Term *body, ExpectedValue expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(expected));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->name = name;
  f->value = ty;
  f->term = body;
  f->term2 = value;
  f->expected = expected;
  return bind_frame(heap, check_engine_check_uses(heap, ctx, quantity_runtime(mode), value, ty), let_typed_after_check,
                    f);
}

static Comp let_body_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_let_typed(heap, f->ctx, f->mode, f->name, value, f->term2, f->term, f->expected);
}

static Comp let_body_after_univ(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term3), let_body_after_eval, f);
}

Comp check_engine_let_body(Heap *heap, Ctx ctx, Quantity mode, Str name, const Term *ty, const Term *value,
                           const Term *body, ExpectedValue expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->name = name;
  f->term = body;
  f->term2 = value;
  f->term3 = ty;
  f->expected = expected;
  return bind_frame(heap, check_engine_infer_univ(heap, ctx, ty), let_body_after_univ, f);
}

static Comp pure_former(Heap *heap, const Value *value) {
  Former *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->whole = value;
  boxed->shape = value->shape;
  boxed->diagram = value->diagram;
  boxed->level = value->former_level;
  return budget_pure(boxed);
}

Comp check_engine_former_value(Heap *heap, const Value *value, bool left, Str message) {
  switch (value->kind) {
  case VALUE_LAN:
    if (left) return pure_former(heap, value);
    break;
  case VALUE_RAN:
    if (!left) return pure_former(heap, value);
    break;
  case VALUE_UNIV:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    break;
  }
  return budget_failed(error_make(ERROR_MISMATCH, message));
}

static Comp former_view_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_former_value(heap, value, f->flag, f->message);
}

Comp check_engine_former_view(Heap *heap, Ctx ctx, bool left, const Value *value, Str message) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(message));
  if (f == NULL) return comp_oom();
  f->flag = left;
  f->message = message;
  return bind_frame(heap, check_engine_whnf(heap, ctx, value), former_view_after_whnf, f);
}

Shape check_engine_former_shape(Former f) { return f.shape; }

Closure check_engine_former_clo(Former f) { return f.diagram; }

const Value *check_engine_former_whole(Former f) { return f.whole; }

static Comp pure_point(Heap *heap, Quantity quantity, Str name, const Value *domain) {
  Point *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->quantity = quantity;
  boxed->name = name;
  boxed->domain = domain;
  return budget_pure(boxed);
}

Comp check_engine_view_point(Heap *heap, Shape shape) {
  switch (shape.kind) {
  case SHAPE_PI:
    return pure_point(heap, shape.quantity, shape.name, shape.domain);
  case SHAPE_ZK:
    return pure_point(heap, shape.quantity, shape.name, shape.domain);
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    break;
  }
  return budget_failed(error_text(ERROR_MISMATCH, "the rule pack does not match the shape of the term"));
}

Quantity check_engine_point_q(Point point) { return point.quantity; }

Str check_engine_point_name(Point point) { return point.name; }

const Value *check_engine_point_dom(Point point) { return point.domain; }

Comp check_engine_view_coll(Heap *heap, Shape shape) {
  switch (shape.kind) {
  case SHAPE_COLL:
    return pure_big(heap, shape.count);
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    break;
  }
  return budget_failed(error_text(ERROR_MISMATCH, "the rule pack does not match the shape of the term"));
}

Comp check_engine_one(const Vec *items, Error error) {
  VecGet first = vec_get(items, 0);
  if (items->len != 1) return budget_failed(error);
  switch (first.status) {
  case VEC_GET_OK:
    return budget_pure(first.value);
  case VEC_GET_OUT_OF_RANGE:
    break;
  }
  return budget_failed(error);
}

static Error binder_quantity_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, str_lit("the "), f->name, str_lit(" "));
  Str marked = cat3(heap, f->message, str_lit(" is marked "), quantity_to_string(f->mode));
  Str source = cat3(heap, str_lit(" and the "), f->value == NULL ? str_empty() : *(const Str *)f->value,
                    str_lit(" marks it "));
  return error_make(ERROR_QUANTITY,
                    heap_str_cat(heap, cat3(heap, head, marked, source), quantity_to_string(f->shape.quantity)));
}

Comp check_engine_binder_quantity(Heap *heap, Str subject, Str source, Str name, Quantity marked, Quantity declared) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(message));
  Str *source_box = heap_alloc(heap, sizeof *source_box);
  ErrorThunk thunk;
  if (f == NULL || source_box == NULL) return comp_oom();
  *source_box = source;
  f->name = subject;
  f->message = name;
  f->mode = marked;
  f->shape.quantity = declared;
  f->value = (const Value *)source_box;
  thunk.fn = binder_quantity_error;
  thunk.ctx = f;
  return check_engine_require_by(heap, quantity_equal(marked, declared), thunk);
}

const Term *check_engine_leg_body(Leg leg) { return leg.body; }

Vec check_engine_leg_binders(Leg leg) { return leg.binders; }

Quantity check_engine_pair_q(Binder pair) { return pair.quantity; }

Str check_engine_pair_name(Binder pair) { return pair.name; }

Comp check_engine_point_arg(Addr addr, Str message) {
  switch (addr.kind) {
  case ADDR_PT:
    return budget_pure((void *)addr.arg);
  case ADDR_LEG:
  case ADDR_CTOR:
    break;
  }
  return budget_failed(error_make(ERROR_WRONG_LEG, message));
}

Comp check_engine_leg_index(Heap *heap, Addr addr, Str message) {
  MaybeBig index = term_as_aleg(addr);
  Big *boxed;
  if (!index.some) return budget_failed(error_make(ERROR_WRONG_LEG, message));
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = index.value;
  return check_engine_optional(true, boxed, error_make(ERROR_WRONG_LEG, message));
}

/* ---- Bend lines 601-845: the point and collection laws, the Mu views (S1.9 hop 23) ---- */

/* Result<Error.T, List<A>> lifted into Comp: the ready value is a heap boxed Vec. */
static Comp lift_vec(Heap *heap, VecResult result) {
  Vec *boxed;
  switch (result.kind) {
  case RESULT_DONE:
    boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) return comp_oom();
    *boxed = result.value;
    return budget_pure(boxed);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return budget_failed(result.error);
}

/* [item] */
static Vec singleton(Heap *heap, void *item) {
  Vec out = vec_new(heap->arena);
  if (!vec_push(&out, item)) heap->oom = true;
  return out;
}

/* item <> rest */
static Vec cons(Heap *heap, void *item, const Vec *rest) {
  Vec head = singleton(heap, item);
  return vec_concat(heap->arena, &head, rest);
}

/* The tail of a non-empty list view (no copy). */
static Vec list_tail(Vec xs) {
  xs.items = xs.items + 1;
  xs.len = xs.len - 1;
  xs.cap = xs.cap > 0 ? xs.cap - 1 : 0;
  return xs;
}

static Nat nat_len(Heap *heap, const Vec *xs) { return heap_nat_of_u64(heap, (uint64_t)xs->len); }

static bool nat_is_eq(Nat a, Nat b) { return nat_cmp(a, b) == 0; }

static ErrorThunk thunk(Error (*fn)(Heap *heap, void *ctx), void *ctx) {
  ErrorThunk out;
  out.fn = fn;
  out.ctx = ctx;
  return out;
}

static Comp pure_level(const LevelVar *level) { return budget_pure((void *)level); }

static Comp pure_value(const Value *value) { return budget_pure((void *)value); }

/* point_form */
static Comp point_form_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const LevelVar *cod_level = value;
  if (f->flag) return pure_level(level_max(heap, f->level, cod_level));
  return pure_level(level_imax(heap, f->level, cod_level));
}

static Comp point_form_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_infer_univ(heap, context_bind(heap, f->name, f->mode, value, f->ctx), f->term2),
                    point_form_after_cod, f);
}

static Comp point_form_after_dom(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->level = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), point_form_after_eval, f);
}

Comp check_engine_point_form(Heap *heap, Ctx ctx, bool left, Quantity q, Str name, const Term *dom,
                             const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(level));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->flag = left;
  f->mode = q;
  f->name = name;
  f->term = dom;
  f->term2 = diagram;
  return bind_frame(heap, check_engine_infer_univ(heap, ctx, dom), point_form_after_dom, f);
}

/* point_intro_sec */
static Comp point_intro_sec_after_close(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *free = value;
  return pure_usage(heap, quantity_scale(heap, f->mode, quantity_captures(heap, *free)));
}

static Comp point_intro_sec_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return bind_frame(heap, check_engine_close(heap, f->inner, context_size_of(f->ctx), f->mode, *uses, false),
                    point_intro_sec_after_close, f);
}

static Comp point_intro_sec_after_open(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->inner = context_bind(heap, f->binder.name, f->point.quantity, f->point.domain, f->ctx);
  return bind_frame(heap, check_engine_check_uses(heap, f->inner, quantity_runtime(f->mode), f->leg.body, value),
                    point_intro_sec_after_check, f);
}

static Comp point_intro_sec_after_quantity(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    check_engine_open(heap, f->ctx, f->former.diagram,
                                      singleton(heap, (void *)value_var(heap, context_size_of(f->ctx)))),
                    point_intro_sec_after_open, f);
}

static Comp point_intro_sec_after_binder(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Binder *binder = value;
  f->binder = *binder;
  return bind_frame(heap,
                    check_engine_binder_quantity(heap, str_lit("binder"), str_lit("type"), binder->name,
                                                 binder->quantity, f->point.quantity),
                    point_intro_sec_after_quantity, f);
}

static Comp point_intro_sec_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Leg *leg = value;
  f->leg = *leg;
  return bind_frame(heap, check_engine_one(&f->leg.binders, error_text(ERROR_MISMATCH, "the leg binds the point once")),
                    point_intro_sec_after_binder, f);
}

static Comp point_intro_sec_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap,
                    check_engine_one(f->items, error_text(ERROR_MISMATCH, "a section at the point shape has one leg")),
                    point_intro_sec_after_leg, f);
}

static Comp point_intro_sec_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, former->shape), point_intro_sec_after_point, f);
}

Comp check_engine_point_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(binder));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->items = legs;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, false, expected,
                                             str_lit("a section needs a right former as its expected type")),
                    point_intro_sec_after_former, f);
}

/* point_intro_in */
static Comp point_intro_in_after_fibre(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *fibre_uses = value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, *fibre_uses));
}

static Comp point_intro_in_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, f->mode, f->term2, value), point_intro_in_after_fibre,
                    f);
}

static Comp point_intro_in_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_open(heap, f->ctx, f->former.diagram, singleton(heap, value)),
                    point_intro_in_after_cod, f);
}

static Comp point_intro_in_after_point_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), point_intro_in_after_eval, f);
}

static Comp point_intro_in_after_fibre_item(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term2 = value;
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, f->point.quantity), f->term,
                                            f->point.domain),
                    point_intro_in_after_point_uses, f);
}

static Comp point_intro_in_after_arg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term = value;
  return bind_frame(heap, check_engine_one(f->items, error_text(ERROR_MISMATCH, "a pair carries one fibre element")),
                    point_intro_in_after_fibre_item, f);
}

static Comp point_intro_in_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap, check_engine_point_arg(f->addr, str_lit("a pair takes the point address")),
                    point_intro_in_after_arg, f);
}

static Comp point_intro_in_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, former->shape), point_intro_in_after_point, f);
}

Comp check_engine_point_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args,
                                 const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(addr));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->addr = addr;
  f->items = args;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, true, expected,
                                             str_lit("a pair needs a left former as its expected type")),
                    point_intro_in_after_former, f);
}

/* point_elim_out */
static Comp point_elim_out_after_open(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_inferred(heap, value, quantity_sequence(heap, f->inferred.uses, f->uses));
}

static Comp point_elim_out_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_open(heap, f->ctx, f->former.diagram, singleton(heap, value)),
                    point_elim_out_after_open, f);
}

static Comp point_elim_out_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), point_elim_out_after_eval, f);
}

static Comp point_elim_out_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, point->quantity), f->term,
                                            point->domain),
                    point_elim_out_after_uses, f);
}

static Comp point_elim_out_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, former->shape), point_elim_out_after_point, f);
}

static Comp point_elim_out_after_infer(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, got->ty,
                                             str_lit("the head of an application is not a function")),
                    point_elim_out_after_former, f);
}

static Comp point_elim_out_after_arg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term = value;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, f->mode, f->term2), point_elim_out_after_infer, f);
}

Comp check_engine_point_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(point));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->term2 = head;
  return bind_frame(heap, check_engine_point_arg(addr, str_lit("an application takes the point address")),
                    point_elim_out_after_arg, f);
}

Comp check_engine_elim_result(Heap *heap, MaybeMotive motive, ExpectedValue expected, Ctx ctx, const Value *self) {
  Vec env;
  if (!motive.some) {
    return check_engine_optional(expected.some, (void *)expected.value,
                                 error_text(ERROR_CANNOT_INFER, "an elimination without a motive needs an expected type"));
  }
  env = context_env_of(ctx);
  return budget_lift(eval_eval(heap, context_globals_of(ctx), cons(heap, (void *)self, &env), motive.value.body));
}

static Comp check_motive_after_univ(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

Comp check_engine_check_motive(Heap *heap, MaybeMotive motive, Ctx ctx, const Value *scrut_ty) {
  if (!motive.some) return pure_unit();
  return bind_frame(heap,
                    check_engine_infer_univ(heap, context_bind(heap, motive.value.self, QUANTITY_ZERO, scrut_ty, ctx),
                                            motive.value.body),
                    check_motive_after_univ, frame_new(heap));
}

MaybeMotive check_engine_elim_motive(const ElimData *data) { return data->motive; }

const Term *check_engine_elim_scrut(const ElimData *data) { return data->scrut; }

/* Branch pointers. */
const Vec *check_engine_elim_branches(const ElimData *data) { return &data->branches; }

static Comp infer_scrutinee_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_infer_uses(heap, f->ctx, quantity_mul(f->mode, f->data->scrut_q), f->data->scrut);
}

Comp check_engine_infer_scrutinee(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(data));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  return bind_frame(heap,
                    check_engine_require(quantity_equal(mode, QUANTITY_ZERO) ||
                                             !quantity_equal(data->scrut_q, QUANTITY_ZERO),
                                         error_text(ERROR_QUANTITY, "an erased scrutinee cannot be eliminated at runtime")),
                    infer_scrutinee_after_require, f);
}

static Comp first_branch_pair_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)heap;
  (void)value;
  return budget_pure((void *)f->items);
}

static Comp first_branch_pair_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *index = value;
  return bind_frame(heap,
                    check_engine_require(big_equal(*index, big_zero()),
                                         error_text(ERROR_WRONG_LEG, "the leg address is outside the collection")),
                    first_branch_pair_leg, f);
}

/* The ready value is the Leg pointer of the branch. */
Comp check_engine_first_branch_pair(Heap *heap, const Branch *branch) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(items));
  if (f == NULL) return comp_oom();
  f->items = (const Vec *)&branch->leg;
  return bind_frame(heap, check_engine_leg_index(heap, branch->addr, str_lit("the branch takes the leg address")),
                    first_branch_pair_after_index, f);
}

/* point_branch */
static Comp point_branch_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return check_engine_close(heap, f->inner, context_size_of(f->ctx), f->mode, *uses, false);
}

static Comp point_branch_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->inner, quantity_runtime(f->mode), f->term, value),
                    point_branch_after_check, f);
}

static Comp point_branch_after_open(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->inner = context_bind(heap, f->binder2.name, f->binder2.quantity, value,
                          context_bind(heap, f->binder.name, f->binder.quantity, f->point.domain, f->ctx));
  return bind_frame(heap,
                    check_engine_elim_result(heap, f->motive, f->expected, f->ctx,
                                             value_vin(heap, f->former.shape,
                                                       value_vaddr_pt(f->point.quantity, f->value),
                                                       singleton(heap, (void *)f->value2))),
                    point_branch_after_target, f);
}

static Comp point_branch_after_quantity(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_open(heap, f->ctx, f->former.diagram, singleton(heap, (void *)f->value)),
                    point_branch_after_open, f);
}

Comp check_engine_point_branch(Heap *heap, Leg leg, Ctx ctx, Quantity mode, Former former, Point point,
                               MaybeMotive motive, ExpectedValue expected) {
  Frame *f;
  VecGet first;
  VecGet second;
  Nat size;
  if (leg.binders.len != 2) {
    return budget_failed(
        error_text(ERROR_MISSING_BRANCH, "the branch binds the point and the fibre element (D-M0-3)"));
  }
  first = vec_get(&leg.binders, 0);
  second = vec_get(&leg.binders, 1);
  if (first.status != VEC_GET_OK || second.status != VEC_GET_OK) {
    return budget_failed(
        error_text(ERROR_MISSING_BRANCH, "the branch binds the point and the fibre element (D-M0-3)"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(binder2));
  if (f == NULL) return comp_oom();
  size = context_size_of(ctx);
  f->ctx = ctx;
  f->mode = mode;
  f->former = former;
  f->point = point;
  f->motive = motive;
  f->expected = expected;
  f->binder = *(const Binder *)first.value;
  f->binder2 = *(const Binder *)second.value;
  f->term = leg.body;
  f->value = value_var(heap, size);
  f->value2 = value_var(heap, heap_nat_succ(heap, size));
  return bind_frame(heap,
                    check_engine_binder_quantity(heap, str_lit("branch binder"), str_lit("type"), f->binder.name,
                                                 f->binder.quantity, point.quantity),
                    point_branch_after_quantity, f);
}

/* point_elim_elim */
static Comp point_elim_elim_after_branch(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_inferred(heap, f->value,
                       quantity_sequence(heap, f->inferred.uses, quantity_scale(heap, f->mode, *uses)));
}

static Comp point_elim_elim_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Leg *leg = value;
  return bind_frame(heap,
                    check_engine_point_branch(heap, *leg, f->ctx, f->mode, f->former, f->point, f->data->motive,
                                              f->expected),
                    point_elim_elim_after_branch, f);
}

static Comp point_elim_elim_after_one(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_first_branch_pair(heap, value), point_elim_elim_after_leg, f);
}

static Comp point_elim_elim_after_result(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap,
                    check_engine_one(&f->data->branches,
                                     error_text(ERROR_MISSING_BRANCH, "an elimination at the point shape has one branch")),
                    point_elim_elim_after_one, f);
}

static Comp point_elim_elim_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_elim_result(heap, f->data->motive, f->expected, f->ctx, value),
                    point_elim_elim_after_result, f);
}

static Comp point_elim_elim_after_motive(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->data->scrut), point_elim_elim_after_scrut, f);
}

static Comp point_elim_elim_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap, check_engine_check_motive(heap, f->data->motive, f->ctx, f->former.whole),
                    point_elim_elim_after_motive, f);
}

static Comp point_elim_elim_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, former->shape), point_elim_elim_after_point, f);
}

static Comp point_elim_elim_after_infer(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, true, got->ty, str_lit("the scrutinee is not a left former")),
                    point_elim_elim_after_former, f);
}

Comp check_engine_point_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(data));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  f->expected = expected;
  return bind_frame(heap, check_engine_infer_scrutinee(heap, ctx, mode, data), point_elim_elim_after_infer, f);
}

/* coll_levels: legs are Leg pointers. */
static Comp coll_levels_after_univ(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_coll_levels(heap, list_tail(f->vec), f->ctx, level_max(heap, f->level, value));
}

static Comp coll_levels_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_univ(heap, f->ctx, f->term), coll_levels_after_univ, f);
}

Comp check_engine_coll_levels(Heap *heap, Vec legs, Ctx ctx, const LevelVar *level) {
  Frame *f;
  VecGet first;
  const Leg *leg;
  if (legs.len == 0) return pure_level(level);
  first = vec_get(&legs, 0);
  if (first.status != VEC_GET_OK) return pure_level(level);
  leg = first.value;
  f = frame_new_upto(heap, FRAME_UPTO(level));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = legs;
  f->level = level;
  f->term = leg->body;
  return bind_frame(heap,
                    check_engine_require(leg->binders.len == 0, error_text(ERROR_MISMATCH, "a diagram leg binds nothing")),
                    coll_levels_after_require, f);
}

const LevelVar *check_engine_expected_level(MaybeLevelVar expected) {
  if (expected.some) return expected.value;
  return level_zero();
}

static Comp coll_form_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const LevelVar *start = big_equal(f->big, big_zero()) ? check_engine_expected_level(f->expected_level) : level_zero();
  (void)value;
  return check_engine_coll_levels(heap, f->vec, f->ctx, start);
}

Comp check_engine_coll_form(Heap *heap, Ctx ctx, Big count, const Term *diagram, MaybeLevelVar expected) {
  Frame *f;
  if (diagram->kind != TERM_SEC || diagram->shape.kind != SHAPE_COLL) {
    return budget_failed(error_text(ERROR_MISMATCH, "the diagram is not a section of the declared width"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(expected_level));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->big = count;
  f->vec = diagram->legs;
  f->expected_level = expected;
  return bind_frame(heap,
                    check_engine_require(big_equal(diagram->shape.count, count) &&
                                             big_equal(big_of_nat(heap, nat_len(heap, &diagram->legs)), count),
                                         error_text(ERROR_MISMATCH, "the diagram is not a section of the declared width")),
                    coll_form_after_require, f);
}

/* coll_check_legs: legs are Leg pointers, dlegs are VLeg pointers. */
static Comp coll_check_legs_after_others(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *others = value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, *others));
}

static Comp coll_check_legs_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_coll_check_legs(heap, list_tail(f->vec), list_tail(f->vec2), f->ctx, f->mode),
                    coll_check_legs_after_others, f);
}

static Comp coll_check_legs_after_open(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, f->mode, f->term, value), coll_check_legs_after_check,
                    f);
}

Comp check_engine_coll_check_legs(Heap *heap, Vec legs, Vec dlegs, Ctx ctx, Quantity mode) {
  Frame *f;
  VecGet first;
  VecGet dfirst;
  const Leg *leg;
  const VLeg *dleg;
  if (legs.len == 0 && dlegs.len == 0) return pure_usage(heap, quantity_empty(heap));
  if (legs.len == 0 || dlegs.len == 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "the diagram is not a section of the declared width"));
  }
  first = vec_get(&legs, 0);
  dfirst = vec_get(&dlegs, 0);
  if (first.status != VEC_GET_OK || dfirst.status != VEC_GET_OK) {
    return budget_failed(error_text(ERROR_MISMATCH, "the diagram is not a section of the declared width"));
  }
  leg = first.value;
  dleg = dfirst.value;
  f = frame_new_upto(heap, FRAME_UPTO(vec2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->vec = legs;
  f->vec2 = dlegs;
  f->term = leg->body;
  return bind_frame(heap, check_engine_open(heap, ctx, dleg->clo, vec_new(heap->arena)), coll_check_legs_after_open, f);
}

/* coll_intro_sec */
static Comp coll_intro_sec_after_dlegs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *dlegs = value;
  return check_engine_coll_check_legs(heap, *f->items, *dlegs, f->ctx, f->mode);
}

static Comp coll_intro_sec_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, f->ctx, f->former.diagram)),
                    coll_intro_sec_after_dlegs, f);
}

static Comp coll_intro_sec_after_width(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *width = value;
  return bind_frame(heap,
                    check_engine_require(big_equal(*width, f->big) &&
                                             big_equal(big_of_nat(heap, nat_len(heap, f->items)), f->big),
                                         error_text(ERROR_MISMATCH, "the tuple width and the type width differ")),
                    coll_intro_sec_after_require, f);
}

static Comp coll_intro_sec_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_coll(heap, former->shape), coll_intro_sec_after_width, f);
}

Comp check_engine_coll_intro_sec(Heap *heap, Ctx ctx, Quantity mode, Big count, const Vec *legs,
                                 const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(big));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->big = count;
  f->items = legs;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, false, expected,
                                             str_lit("a tuple needs a right former as its expected type")),
                    coll_intro_sec_after_former, f);
}

/* coll_intro_in */
static Comp coll_intro_in_after_payload(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_check_uses(heap, f->ctx, f->mode, value, f->value);
}

static Comp coll_intro_in_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_one(f->items, error_text(ERROR_MISMATCH, "an injection carries one payload")),
                    coll_intro_in_after_payload, f);
}

static Comp coll_intro_in_after_dlegs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *dlegs = value;
  return bind_frame(heap, budget_lift(rules_coll_leg_ty(heap, f->ctx, dlegs, f->big)), coll_intro_in_after_ty, f);
}

static Comp coll_intro_in_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, f->ctx, f->former.diagram)),
                    coll_intro_in_after_dlegs, f);
}

static Comp coll_intro_in_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *index = value;
  f->big = *index;
  return bind_frame(heap,
                    check_engine_require(big_le(big_zero(), *index) && !big_le(f->big2, *index),
                                         error_text(ERROR_WRONG_LEG, "the leg address is outside the collection")),
                    coll_intro_in_after_require, f);
}

static Comp coll_intro_in_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  f->big2 = *count;
  return bind_frame(heap, check_engine_leg_index(heap, f->addr, str_lit("an injection takes the leg address")),
                    coll_intro_in_after_index, f);
}

static Comp coll_intro_in_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_coll(heap, former->shape), coll_intro_in_after_count, f);
}

Comp check_engine_coll_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(big2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->addr = addr;
  f->items = args;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, true, expected,
                                             str_lit("an injection needs a left former as its expected type")),
                    coll_intro_in_after_former, f);
}

/* coll_elim_out */
static Comp coll_elim_out_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_inferred(heap, value, f->inferred.uses);
}

static Comp coll_elim_out_after_dlegs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *dlegs = value;
  return bind_frame(heap, budget_lift(rules_coll_leg_ty(heap, f->ctx, dlegs, f->big)), coll_elim_out_after_ty, f);
}

static Comp coll_elim_out_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, f->ctx, f->former.diagram)),
                    coll_elim_out_after_dlegs, f);
}

static Comp coll_elim_out_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  return bind_frame(heap,
                    check_engine_require(big_le(big_zero(), f->big) && !big_le(*count, f->big),
                                         error_text(ERROR_WRONG_LEG, "the leg address is outside the collection")),
                    coll_elim_out_after_require, f);
}

static Comp coll_elim_out_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_coll(heap, former->shape), coll_elim_out_after_count, f);
}

static Comp coll_elim_out_after_infer(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, got->ty, str_lit("a projection needs a right former")),
                    coll_elim_out_after_former, f);
}

static Comp coll_elim_out_after_index(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *index = value;
  f->big = *index;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, f->mode, f->term), coll_elim_out_after_infer, f);
}

Comp check_engine_coll_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(inferred));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->term = head;
  return bind_frame(heap, check_engine_leg_index(heap, addr, str_lit("a projection takes the leg address")),
                    coll_elim_out_after_index, f);
}

/* coll_branch */
static Error coll_branch_missing(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_MISSING_BRANCH, heap_str_cat(heap, str_lit("no branch at leg "), heap_nat_show(heap, f->nat)));
}

static Comp coll_branch_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return check_engine_close(heap, f->inner, context_size_of(f->ctx), f->mode, *uses, false);
}

static Comp coll_branch_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->inner = context_bind(heap, f->binder.name, f->binder.quantity, f->value, f->ctx);
  return bind_frame(heap, check_engine_check_uses(heap, f->inner, quantity_runtime(f->mode), f->leg.body, value),
                    coll_branch_after_check, f);
}

static Comp coll_branch_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap,
                    check_engine_elim_result(heap, f->data->motive, f->expected, f->ctx,
                                             value_vin(heap, f->shape, value_valeg(heap, f->nat),
                                                       singleton(heap, (void *)value_var(heap, context_size_of(f->ctx))))),
                    coll_branch_after_target, f);
}

static Comp coll_branch_after_binder(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Binder *binder = value;
  f->binder = *binder;
  return bind_frame(heap, budget_lift(rules_coll_leg_ty(heap, f->ctx, &f->vec2, big_of_nat(heap, f->nat))),
                    coll_branch_after_ty, f);
}

static Comp coll_branch_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Leg *leg = value;
  f->leg = *leg;
  return bind_frame(heap,
                    check_engine_one(&f->leg.binders,
                                     error_text(ERROR_MISSING_BRANCH, "each branch binds its payload once")),
                    coll_branch_after_binder, f);
}

Comp check_engine_coll_branch(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, Shape shape, Vec dlegs,
                              ExpectedValue expected, Nat index) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(binder));
  MaybeLegPtr found;
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  f->shape = shape;
  f->vec2 = dlegs;
  f->expected = expected;
  f->nat = index;
  found = eval_find_branch(&data->branches, value_valeg(heap, index));
  return bind_frame(heap, check_engine_optional_by(heap, found.some, (void *)found.value, thunk(coll_branch_missing, f)),
                    coll_branch_after_leg, f);
}

static Comp coll_branches_after_branch(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *branch = value;
  return check_engine_coll_branches(heap, pred(heap, f->nat2), heap_nat_succ(heap, f->nat), f->ctx, f->mode, f->data,
                                    f->shape, f->vec2, f->expected, quantity_alternative(heap, f->uses, *branch));
}

Comp check_engine_coll_branches(Heap *heap, Nat count, Nat index, Ctx ctx, Quantity mode, const ElimData *data,
                                Shape shape, Vec dlegs, ExpectedValue expected, Usage uses) {
  Frame *f;
  if (nat_is_zero(count)) return pure_usage(heap, uses);
  f = frame_new_upto(heap, FRAME_UPTO(data));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  f->shape = shape;
  f->vec2 = dlegs;
  f->expected = expected;
  f->nat = index;
  f->nat2 = count;
  f->uses = uses;
  return bind_frame(heap, check_engine_coll_branch(heap, ctx, mode, data, shape, dlegs, expected, index),
                    coll_branches_after_branch, f);
}

/* coll_elim_elim */
static Error coll_elim_elim_count_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str text = cat3(heap, str_lit("the case has "), heap_nat_show(heap, f->nat), str_lit(" branches and the type has "));
  return error_make(ERROR_MISSING_BRANCH, cat3(heap, text, big_to_string(heap, f->big), str_lit(" legs")));
}

static Comp coll_elim_elim_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_inferred(heap, f->value,
                       quantity_sequence(heap, f->inferred.uses, quantity_scale(heap, f->mode, *uses)));
}

static Comp coll_elim_elim_after_dlegs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *dlegs = value;
  return bind_frame(heap,
                    check_engine_coll_branches(heap, f->nat, nat_zero(), f->ctx, f->mode, f->data, f->former.shape,
                                               *dlegs, f->expected, quantity_unreachable(heap)),
                    coll_elim_elim_after_uses, f);
}

static Comp coll_elim_elim_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, f->ctx, f->former.diagram)),
                    coll_elim_elim_after_dlegs, f);
}

static Comp coll_elim_elim_after_result(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  f->nat = nat_len(heap, &f->data->branches);
  return bind_frame(heap,
                    check_engine_require_by(heap, big_equal(big_of_nat(heap, f->nat), f->big),
                                            thunk(coll_elim_elim_count_error, f)),
                    coll_elim_elim_after_require, f);
}

static Comp coll_elim_elim_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_elim_result(heap, f->data->motive, f->expected, f->ctx, value),
                    coll_elim_elim_after_result, f);
}

static Comp coll_elim_elim_after_motive(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->data->scrut), coll_elim_elim_after_scrut, f);
}

static Comp coll_elim_elim_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  f->big = *count;
  return bind_frame(heap, check_engine_check_motive(heap, f->data->motive, f->ctx, f->former.whole),
                    coll_elim_elim_after_motive, f);
}

static Comp coll_elim_elim_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_coll(heap, former->shape), coll_elim_elim_after_count, f);
}

static Comp coll_elim_elim_after_infer(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, true, got->ty, str_lit("the scrutinee is not a left former")),
                    coll_elim_elim_after_former, f);
}

Comp check_engine_coll_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(data));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  f->expected = expected;
  return bind_frame(heap, check_engine_infer_scrutinee(heap, ctx, mode, data), coll_elim_elim_after_infer, f);
}

static Comp pure_mu(Heap *heap, Str name, Vec indices) {
  Mu *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->name = name;
  boxed->indices = indices;
  return budget_pure(boxed);
}

Comp check_engine_mu_view(Heap *heap, Shape shape) {
  switch (shape.kind) {
  case SHAPE_MU:
    return pure_mu(heap, shape.name, shape.args);
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    break;
  }
  return budget_failed(error_text(ERROR_MISMATCH, "the rule pack does not match the shape of the term"));
}

Str check_engine_mu_name(Mu value) { return value.name; }

Vec check_engine_mu_index_values(Mu value) { return value.indices; }

/* ---- Bend lines 846-1111 (S1.9 hop 24): the Positivity accessors and the mu laws up to mu_cover ---- */

static Comp pure_env_uses(Heap *heap, Vec env, Usage uses) {
  EnvUses *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->env = env;
  boxed->uses = uses;
  return budget_pure(boxed);
}

static Comp pure_fields(Heap *heap, Ctx ctx, Vec env, Vec values) {
  Fields *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->ctx = ctx;
  boxed->env = env;
  boxed->values = values;
  return budget_pure(boxed);
}

static Comp pure_vec(Heap *heap, Vec items) {
  Vec *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = items;
  return budget_pure(boxed);
}

static Comp pure_motive(Heap *heap, Motive motive) {
  Motive *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = motive;
  return budget_pure(boxed);
}

/* Result<Error.T, Positivity.Family> lifted into Comp: the ready value is a heap boxed Family. */
static Comp lift_family(Heap *heap, FamilyResult result) {
  Family *boxed;
  switch (result.kind) {
  case RESULT_DONE:
    boxed = heap_alloc(heap, sizeof *boxed);
    if (boxed == NULL) return comp_oom();
    *boxed = result.value;
    return budget_pure(boxed);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return comp_oom();
}

Vec check_engine_fam_params(Family f) { return f.params; }

Vec check_engine_fam_indices(Family f) { return f.indices; }

const LevelVar *check_engine_fam_level(Family f) { return f.level; }

Vec check_engine_ctor_args(Ctor c) { return c.args; }

Vec check_engine_ctor_indices(Ctor c) { return c.res_idx; }

Vec check_engine_env_values(EnvUses value) { return value.env; }

Usage check_engine_env_uses(EnvUses value) { return value.uses; }

Ctx check_engine_fields_ctx(Fields value) { return value.ctx; }

Vec check_engine_fields_env(Fields value) { return value.env; }

Vec check_engine_fields_values(Fields value) { return value.values; }

Comp check_engine_params_of(Heap *heap, const Term *diagram) {
  if (diagram->kind != TERM_SEC) {
    return budget_failed(error_text(ERROR_MISMATCH, "the diagram is not a section of the declared width"));
  }
  return pure_vec(heap, term_leg_children(heap, &diagram->legs));
}

static Comp telescope_loop_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_telescope_loop(heap, list_tail(f->vec), list_tail(f->vec2), f->ctx, f->mode, cons(heap, value, &f->vec3),
                                     quantity_sequence(heap, f->uses, f->uses2));
}

static Comp telescope_loop_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *next = value;
  f->uses2 = *next;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), telescope_loop_after_value, f);
}

static Comp telescope_loop_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *tyv = value;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, f->q, f->term, tyv), telescope_loop_after_uses, f);
}

/* tele: TeleBinder pointers; args: Term pointers; env: Value pointers (list views by value). */
Comp check_engine_telescope_loop(Heap *heap, Vec tele, Vec args, Ctx ctx, Quantity mode, Vec env, Usage uses) {
  Frame *f;
  VecGet first;
  VecGet arg;
  const TeleBinder *binder;
  if (tele.len == 0 && args.len == 0) return pure_env_uses(heap, env, uses);
  if (tele.len == 0 || args.len == 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "the telescope has the wrong arity"));
  }
  first = vec_get(&tele, 0);
  arg = vec_get(&args, 0);
  if (first.status != VEC_GET_OK || arg.status != VEC_GET_OK) {
    return budget_failed(error_text(ERROR_MISMATCH, "the telescope has the wrong arity"));
  }
  binder = first.value;
  f = frame_new_upto(heap, FRAME_UPTO(uses2));
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->vec2 = args;
  f->ctx = ctx;
  f->mode = mode;
  f->q = quantity_mul(mode, binder->quantity);
  f->vec3 = env;
  f->uses = uses;
  f->term = arg.value;
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(ctx), env, binder->ty)), telescope_loop_after_ty, f);
}

static Error telescope_uses_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, f->message, str_lit(" takes "), heap_nat_show(heap, f->nat));
  return error_make(ERROR_MISMATCH,
                    cat3(heap, head, str_lit(" arguments and the term gives "), heap_nat_show(heap, f->nat2)));
}

static Comp telescope_uses_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_telescope_loop(heap, f->vec, f->vec2, f->ctx, f->mode, f->vec3, quantity_empty(heap));
}

Comp check_engine_telescope_uses(Heap *heap, Ctx ctx, Quantity mode, Str what, const Vec *tele, const Vec *args,
                                 Vec env) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(message));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->message = what;
  f->vec = *tele;
  f->vec2 = *args;
  f->vec3 = env;
  f->nat = nat_len(heap, tele);
  f->nat2 = nat_len(heap, args);
  return bind_frame(heap, check_engine_require_by(heap, nat_is_eq(f->nat, f->nat2), thunk(telescope_uses_error, f)),
                    telescope_uses_after_require, f);
}

static Comp mu_form_after_indices(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)heap;
  (void)value;
  return pure_level(f->family.level);
}

static Comp mu_form_after_params_env(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EnvUses *penv = value;
  Str what = heap_str_cat(heap, str_lit("the indices of "), f->name);
  return bind_frame(heap,
                    check_engine_telescope_uses(heap, f->ctx, QUANTITY_ZERO, what, &f->family.indices, f->items, penv->env),
                    mu_form_after_indices, f);
}

static Comp mu_form_after_params(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *params = value;
  return bind_frame(heap,
                    check_engine_telescope_uses(heap, f->ctx, QUANTITY_ZERO, f->name, &f->family.params, params,
                                                vec_new(heap->arena)),
                    mu_form_after_params_env, f);
}

static Comp mu_form_after_family(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Family *fam = value;
  f->family = *fam;
  return bind_frame(heap, check_engine_params_of(heap, f->term), mu_form_after_params, f);
}

/* indices: Term pointers. */
Comp check_engine_mu_form(Heap *heap, Ctx ctx, Str name, const Vec *indices, const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(family));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->name = name;
  f->items = indices;
  f->term = diagram;
  return bind_frame(heap, lift_family(heap, rules_mu_family(heap, ctx, name)), mu_form_after_family, f);
}

Comp check_engine_index_equal_result(Heap *heap, bool same, Ctx ctx, Str ctor, Str name, const Value *got,
                                     const Value *want) {
  Str head;
  Str middle;
  if (same) return pure_unit();
  head = cat3(heap, str_lit("the constructor "), ctor, str_lit(" of "));
  middle = cat3(heap, head, name, str_lit(" gives the index "));
  return budget_failed(error_make(
      ERROR_MISMATCH, cat3(heap, cat3(heap, middle, check_engine_pp_value(heap, ctx, got), str_lit(" and the type asks for ")),
                           check_engine_pp_value(heap, ctx, want), str_empty())));
}

static Comp index_loop_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_index_loop(heap, list_tail(f->vec), list_tail(f->vec2), list_tail(f->vec3), f->ctx, f->name, f->name2,
                                 f->vec4, cons(heap, (void *)f->value, &f->vec5));
}

static Comp index_loop_after_conv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_index_equal_result(heap, kernel_bool_unbox(value), f->ctx, f->name2, f->name, f->value2,
                                                    f->value),
                    index_loop_after_ok, f);
}

static Comp index_loop_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *got = value;
  f->value2 = got;
  return bind_frame(heap, check_engine_conv(heap, f->ctx, f->value3, got, f->value), index_loop_after_conv, f);
}

static Comp index_loop_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *tyv = value;
  f->value3 = tyv;
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(f->ctx), f->vec4, f->term)), index_loop_after_got,
                    f);
}

/* tele: TeleBinder pointers; results: Term pointers; wants, env, index_env: Value pointers (list views by value). */
Comp check_engine_index_loop(Heap *heap, Vec tele, Vec results, Vec wants, Ctx ctx, Str name, Str ctor, Vec env,
                             Vec index_env) {
  Frame *f;
  VecGet first;
  VecGet result;
  VecGet want;
  const TeleBinder *binder;
  if (tele.len == 0 && results.len == 0 && wants.len == 0) return pure_unit();
  if (tele.len == 0 || results.len == 0 || wants.len == 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "the family index telescope has the wrong arity"));
  }
  first = vec_get(&tele, 0);
  result = vec_get(&results, 0);
  want = vec_get(&wants, 0);
  if (first.status != VEC_GET_OK || result.status != VEC_GET_OK || want.status != VEC_GET_OK) {
    return budget_failed(error_text(ERROR_MISMATCH, "the family index telescope has the wrong arity"));
  }
  binder = first.value;
  f = frame_new_upto(heap, FRAME_UPTO(vec5));
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->vec2 = results;
  f->vec3 = wants;
  f->ctx = ctx;
  f->name = name;
  f->name2 = ctor;
  f->vec4 = env;
  f->vec5 = index_env;
  f->term = result.value;
  f->value = want.value;
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(ctx), index_env, binder->ty)), index_loop_after_ty,
                    f);
}

static Error mu_indices_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, f->name2, str_lit(" gives "), heap_nat_show(heap, f->nat));
  Str middle = cat3(heap, head, str_lit(" result indices and "), f->name);
  return error_make(ERROR_MISMATCH, cat3(heap, middle, str_lit(" takes "), heap_nat_show(heap, f->nat2)));
}

static Comp mu_indices_after_require(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_index_loop(heap, f->vec, f->vec2, f->vec4, f->ctx, f->name, f->name2, f->vec5, f->vec3);
}

/* tele: TeleBinder pointers; results: Term pointers; penv, wants, env: Value pointers. */
Comp check_engine_mu_indices(Heap *heap, Ctx ctx, Str name, Str ctor, const Vec *tele, const Vec *results, Vec penv,
                             Vec wants, Vec env) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(vec5));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->name = name;
  f->name2 = ctor;
  f->vec = *tele;
  f->vec2 = *results;
  f->vec3 = penv;
  f->vec4 = wants;
  f->vec5 = env;
  f->nat = nat_len(heap, results);
  f->nat2 = nat_len(heap, &wants);
  return bind_frame(heap, check_engine_require_by(heap, nat_is_eq(f->nat, f->nat2), thunk(mu_indices_error, f)),
                    mu_indices_after_require, f);
}

static Comp mu_intro_in_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_usage(heap, f->uses);
}

static Comp mu_intro_in_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const EnvUses *got = value;
  f->uses = got->uses;
  return bind_frame(heap,
                    check_engine_mu_indices(heap, f->ctx, f->mu.name, f->name, &f->family.indices, &f->ctor.res_idx, f->vec3,
                                            f->mu.indices, got->env),
                    mu_intro_in_after_ok, f);
}

static Comp mu_intro_in_after_penv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *penv = value;
  f->vec3 = *penv;
  return bind_frame(heap, check_engine_telescope_uses(heap, f->ctx, f->mode, f->name, &f->ctor.args, f->items, f->vec3),
                    mu_intro_in_after_got, f);
}

static Comp mu_intro_in_after_ctor(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctor *ct = value;
  f->ctor = *ct;
  return bind_frame(heap, lift_vec(heap, rules_mu_param_env(heap, f->ctx, f->former.diagram)), mu_intro_in_after_penv, f);
}

static Error mu_intro_in_ctor_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_UNBOUND, cat3(heap, f->name, str_lit(" is not a constructor of "), f->mu.name));
}

static Comp mu_intro_in_after_family(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Family *fam = value;
  MaybeCtor ct;
  Ctor *boxed;
  f->family = *fam;
  ct = positivity_ctor_of(f->name, f->family);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ct.value;
  return bind_frame(heap, check_engine_optional_by(heap, ct.some, boxed, thunk(mu_intro_in_ctor_error, f)),
                    mu_intro_in_after_ctor, f);
}

static Comp mu_intro_in_after_actor(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Str *ctor = value;
  f->name = *ctor;
  return bind_frame(heap, lift_family(heap, rules_mu_family(heap, f->ctx, f->mu.name)), mu_intro_in_after_family, f);
}

static Comp mu_intro_in_after_mu(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Mu *mu = value;
  MaybeStr actor;
  Str *boxed;
  f->mu = *mu;
  actor = term_as_actor(f->addr);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = actor.value;
  return bind_frame(heap,
                    check_engine_optional(actor.some, boxed,
                                          error_text(ERROR_WRONG_LEG, "a constructor takes a constructor address")),
                    mu_intro_in_after_actor, f);
}

static Comp mu_intro_in_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_mu_view(heap, former->shape), mu_intro_in_after_mu, f);
}

/* args: Term pointers. */
Comp check_engine_mu_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(mu));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->addr = addr;
  f->items = args;
  return bind_frame(heap, check_engine_former_view(heap, ctx, true, expected, str_lit("a constructor needs a left former")),
                    mu_intro_in_after_former, f);
}

Vec check_engine_motive_indices(Motive motive) { return motive.indices; }

const Term *check_engine_motive_body(Motive motive) { return motive.body; }

Str check_engine_motive_self(Motive motive) { return motive.self; }

MaybeStr check_engine_motive_ind(Motive motive) { return motive.ind; }

static Comp mu_motive_of_after_ok3(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_motive(heap, f->mot);
}

static Error mu_motive_of_actual_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, f->name, str_lit(" is applied to "), heap_nat_show(heap, f->nat3));
  return error_make(ERROR_MISMATCH, cat3(heap, head, str_lit(" indices and the family has "), heap_nat_show(heap, f->nat)));
}

static Comp mu_motive_of_after_ok2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  f->nat3 = nat_len(heap, f->items);
  return bind_frame(heap, check_engine_require_by(heap, nat_is_eq(f->nat3, f->nat), thunk(mu_motive_of_actual_error, f)),
                    mu_motive_of_after_ok3, f);
}

static Error mu_motive_of_got_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, str_lit("the motive of "), f->name, str_lit(" binds "));
  Str middle = cat3(heap, head, heap_nat_show(heap, f->nat2), str_lit(" indices and the family has "));
  return error_make(ERROR_MISMATCH, heap_str_cat(heap, middle, heap_nat_show(heap, f->nat)));
}

static Comp mu_motive_of_after_ok1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  f->nat2 = nat_len(heap, &f->mot.indices);
  return bind_frame(heap, check_engine_require_by(heap, nat_is_eq(f->nat2, f->nat), thunk(mu_motive_of_got_error, f)),
                    mu_motive_of_after_ok2, f);
}

static Error mu_motive_of_name_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, str_lit("the motive is built for "), f->name2, str_lit(" and the scrutinee is at "));
  return error_make(ERROR_MISMATCH, heap_str_cat(heap, head, f->name));
}

static Comp mu_motive_of_after_ind(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Str *mn = value;
  f->name2 = *mn;
  return bind_frame(heap, check_engine_require_by(heap, str_eq(f->name2, f->name), thunk(mu_motive_of_name_error, f)),
                    mu_motive_of_after_ok1, f);
}

static Error mu_motive_of_ind_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_MISMATCH,
                    cat3(heap, str_lit("the motive of an elimination at "), f->name, str_lit(" names no family")));
}

static Comp mu_motive_of_after_motive(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Motive *motive = value;
  Str *boxed;
  f->mot = *motive;
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = motive->ind.value;
  return bind_frame(heap, check_engine_optional_by(heap, motive->ind.some, boxed, thunk(mu_motive_of_ind_error, f)),
                    mu_motive_of_after_ind, f);
}

/* indices: Value pointers. The ready value is a heap boxed Motive. */
Comp check_engine_mu_motive_of(Heap *heap, Str name, Family fam, const Vec *indices, const ElimData *data) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(nat3));
  MaybeMotive motive;
  Motive *boxed;
  if (f == NULL) return comp_oom();
  f->name = name;
  f->family = fam;
  f->items = indices;
  f->data = data;
  f->nat = nat_len(heap, &fam.indices);
  motive = check_engine_elim_motive(data);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = motive.value;
  return bind_frame(heap,
                    check_engine_optional(motive.some, boxed,
                                          error_text(ERROR_CANNOT_INFER, "an elimination at a mu shape needs a motive")),
                    mu_motive_of_after_motive, f);
}

/* indices: Value pointers. */
Comp check_engine_mu_result(Heap *heap, Ctx ctx, Motive motive, Vec indices, const Value *self) {
  Vec reversed = vec_reverse(heap->arena, &indices);
  Vec env = context_env_of(ctx);
  Vec rest = vec_concat(heap->arena, &reversed, &env);
  return budget_lift(eval_eval(heap, context_globals_of(ctx), cons(heap, (void *)self, &rest), motive.body));
}

static Comp motive_bind_indices_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *tyv = value;
  return check_engine_motive_bind_indices(heap, list_tail(f->vec), list_tail(f->vec2),
                                          context_bind(heap, f->name2, QUANTITY_ZERO, tyv, f->ctx),
                                          cons(heap, (void *)f->value, &f->vec3), cons(heap, (void *)f->value, &f->vec4),
                                          f->name);
}

/* tele: TeleBinder pointers; names: Str pointers; env, values: Value pointers (list views by value). */
Comp check_engine_motive_bind_indices(Heap *heap, Vec tele, Vec names, Ctx ctx, Vec env, Vec values, Str family) {
  Frame *f;
  VecGet first;
  VecGet name;
  const TeleBinder *binder;
  if (tele.len == 0 && names.len == 0) return pure_fields(heap, ctx, env, values);
  if (tele.len == 0 || names.len == 0) {
    return budget_failed(
        error_make(ERROR_MISMATCH, cat3(heap, str_lit("the motive of "), family, str_lit(" does not bind the indices"))));
  }
  first = vec_get(&tele, 0);
  name = vec_get(&names, 0);
  if (first.status != VEC_GET_OK || name.status != VEC_GET_OK) {
    return budget_failed(
        error_make(ERROR_MISMATCH, cat3(heap, str_lit("the motive of "), family, str_lit(" does not bind the indices"))));
  }
  binder = first.value;
  f = frame_new_upto(heap, FRAME_UPTO(vec4));
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->vec2 = names;
  f->ctx = ctx;
  f->vec3 = env;
  f->vec4 = values;
  f->name = family;
  f->name2 = *(const Str *)name.value;
  f->value = value_var(heap, context_size_of(ctx));
  if (f->value == NULL) return comp_oom();
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(ctx), env, binder->ty)), motive_bind_indices_after_ty,
                    f);
}

MaybeLevelVar check_engine_former_level(Former f) { return f.level; }

static Comp mu_motive_level_after_fields(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Fields *fields = value;
  Vec reversed = vec_reverse(heap->arena, &fields->values);
  const Value *self = value_vlan(heap, shape_smu(f->name, reversed), f->former.diagram, f->former.level);
  if (self == NULL) return comp_oom();
  return check_engine_infer_univ(heap, context_bind(heap, f->mot.self, QUANTITY_ZERO, self, fields->ctx), f->mot.body);
}

/* penv: Value pointers. */
Comp check_engine_mu_motive_level(Heap *heap, Ctx ctx, Str name, Family fam, Motive motive, Former former, Vec penv) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(mot));
  if (f == NULL) return comp_oom();
  f->name = name;
  f->mot = motive;
  f->former = former;
  return bind_frame(heap,
                    check_engine_motive_bind_indices(heap, fam.indices, motive.indices, ctx, penv, vec_new(heap->arena), name),
                    mu_motive_level_after_fields, f);
}

/* tele: TeleBinder pointers (list view by value). */
bool check_engine_erased_fields(Vec tele) {
  VecGet first;
  const TeleBinder *binder;
  if (tele.len == 0) return true;
  first = vec_get(&tele, 0);
  if (first.status != VEC_GET_OK) return true;
  binder = first.value;
  return quantity_equal(binder->quantity, QUANTITY_ZERO) && check_engine_erased_fields(list_tail(tele));
}

bool check_engine_zero_ctor(MaybeCtor ctor) {
  if (ctor.some) return check_engine_erased_fields(ctor.value.args) && !ctor.value.self_rec;
  return false;
}

bool check_engine_mu_zero_eliminable(Family fam) {
  VecGet first;
  switch (fam.status.kind) {
  case CTOR_STATUS_COMPLETE:
    if (fam.status.names.len == 0) return true;
    if (fam.status.names.len != 1) return false;
    first = vec_get(&fam.status.names, 0);
    if (first.status != VEC_GET_OK) return false;
    return check_engine_zero_ctor(positivity_find_ctor(&fam.ctors, *(const Str *)first.value));
  case CTOR_STATUS_PROVISIONAL:
  case CTOR_STATUS_BUILTIN:
    return false;
  }
  return false;
}

static Error mu_large_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_UNIVERSE,
                    heap_str_cat(heap, str_lit("a large elimination out of a proposition needs a subsingleton family at "),
                                 f->name));
}

Comp check_engine_mu_large(Heap *heap, Str name, Family fam, const LevelVar *level) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(name));
  bool valid;
  if (f == NULL) return comp_oom();
  f->name = name;
  valid = level_always_positive(fam.level) || level_always_zero(level) || check_engine_mu_zero_eliminable(fam);
  return check_engine_require_by(heap, valid, thunk(mu_large_error, f));
}

/* The ready value is a heap boxed Vec of Str pointers. */
Comp check_engine_mu_ctor_names(Heap *heap, Family fam, Str name) {
  switch (fam.status.kind) {
  case CTOR_STATUS_COMPLETE:
    return pure_vec(heap, fam.status.names);
  case CTOR_STATUS_PROVISIONAL:
    return budget_failed(
        error_make(ERROR_UNBOUND, cat3(heap, str_lit("the family "), name, str_lit(" is still under declaration"))));
  case CTOR_STATUS_BUILTIN:
    return budget_failed(error_make(
        ERROR_MISMATCH, cat3(heap, str_lit("the family "), name, str_lit(" is a kernel family and has no branches"))));
  }
  return comp_oom();
}

static Comp branch_keys_after_rest(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *keys = value;
  return pure_vec(heap, cons(heap, (void *)f->value, keys));
}

/* branches: Branch pointers (list view by value). The ready value is a heap boxed Vec of Str pointers. */
Comp check_engine_branch_keys(Heap *heap, Vec branches, Str name) {
  Frame *f;
  VecGet first;
  const Branch *branch;
  if (branches.len == 0) return pure_vec(heap, vec_new(heap->arena));
  first = vec_get(&branches, 0);
  if (first.status != VEC_GET_OK) return pure_vec(heap, vec_new(heap->arena));
  branch = first.value;
  if (branch->addr.kind != ADDR_CTOR) {
    return budget_failed(
        error_make(ERROR_WRONG_LEG, cat3(heap, str_lit("a branch of "), name, str_lit(" takes the constructor address"))));
  }
  f = frame_new_upto(heap, FRAME_UPTO(value));
  if (f == NULL) return comp_oom();
  f->value = (const Value *)&branch->addr.name;
  return bind_frame(heap, check_engine_branch_keys(heap, list_tail(branches), name), branch_keys_after_rest, f);
}

/* keys: Str pointers (list view by value). */
Nat check_engine_count_name(Heap *heap, Vec keys, Str name) {
  VecGet first;
  Nat here;
  if (keys.len == 0) return nat_zero();
  first = vec_get(&keys, 0);
  if (first.status != VEC_GET_OK) return nat_zero();
  here = str_eq(*(const Str *)first.value, name) ? heap_nat_of_u64(heap, 1) : nat_zero();
  return heap_nat_add(heap, here, check_engine_count_name(heap, list_tail(keys), name));
}

Comp check_engine_cover_count(Heap *heap, Nat count, Str name, Str ctor) {
  if (nat_is_zero(count)) {
    return budget_failed(error_make(
        ERROR_MISSING_BRANCH, cat3(heap, cat3(heap, str_lit("the elimination of "), name, str_lit(" has no branch at ")), ctor,
                                   str_empty())));
  }
  if (nat_is_eq(count, heap_nat_of_u64(heap, 1))) return pure_unit();
  return budget_failed(error_make(
      ERROR_WRONG_LEG, cat3(heap, cat3(heap, str_lit("the elimination of "), name, str_lit(" repeats the branch at ")), ctor,
                            str_empty())));
}

static Comp cover_names_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_cover_names(heap, list_tail(f->vec), f->items, f->name);
}

/* names: Str pointers (list view by value); keys: Str pointers. */
Comp check_engine_cover_names(Heap *heap, Vec names, const Vec *keys, Str family) {
  Frame *f;
  VecGet first;
  Str name;
  if (names.len == 0) return pure_unit();
  first = vec_get(&names, 0);
  if (first.status != VEC_GET_OK) return pure_unit();
  name = *(const Str *)first.value;
  f = frame_new_upto(heap, FRAME_UPTO(items));
  if (f == NULL) return comp_oom();
  f->vec = names;
  f->items = keys;
  f->name = family;
  return bind_frame(heap, check_engine_cover_count(heap, check_engine_count_name(heap, *keys, name), family, name),
                    cover_names_after_ok, f);
}

static Error cover_known_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_UNBOUND, cat3(heap, f->name2, str_lit(" is not a constructor of "), f->name));
}

static Comp cover_known_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_cover_known(heap, list_tail(f->vec), f->items, f->name);
}

/* keys: Str pointers (list view by value); names: Str pointers. */
Comp check_engine_cover_known(Heap *heap, Vec keys, const Vec *names, Str family) {
  Frame *f;
  VecGet first;
  if (keys.len == 0) return pure_unit();
  first = vec_get(&keys, 0);
  if (first.status != VEC_GET_OK) return pure_unit();
  f = frame_new_upto(heap, FRAME_UPTO(name2));
  if (f == NULL) return comp_oom();
  f->vec = keys;
  f->items = names;
  f->name = family;
  f->name2 = *(const Str *)first.value;
  return bind_frame(heap, check_engine_require_by(heap, term_member(names, f->name2), thunk(cover_known_error, f)),
                    cover_known_after_ok, f);
}

static Comp mu_cover_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_cover_known(heap, f->vec, f->items, f->name);
}

static Comp mu_cover_after_keys(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *keys = value;
  f->vec = *keys;
  return bind_frame(heap, check_engine_cover_names(heap, *f->items, &f->vec, f->name), mu_cover_after_ok, f);
}

/* names: Str pointers; branches: Branch pointers. */
Comp check_engine_mu_cover(Heap *heap, Str name, const Vec *names, const Vec *branches) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(items));
  if (f == NULL) return comp_oom();
  f->name = name;
  f->items = names;
  return bind_frame(heap, check_engine_branch_keys(heap, *branches, name), mu_cover_after_keys, f);
}

/* ---- Bend lines 1113-1248 (S1.9 hop 26): the mu eliminator, the circuit bridge, the zk laws, the fhc views ---- */

static Comp pure_proof(Heap *heap, const Term *fibre, Usage uses) {
  ProofArgs *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->fibre = fibre;
  boxed->uses = uses;
  return budget_pure(boxed);
}

static Comp branch_bind_fields_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_branch_bind_fields(heap, list_tail(f->vec), list_tail(f->vec2),
                                         context_bind(heap, f->name, f->q, f->value2, f->ctx),
                                         cons(heap, (void *)f->value, &f->vec3), cons(heap, (void *)f->value, &f->vec4));
}

static Comp branch_bind_fields_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value2 = value;
  return bind_frame(heap,
                    check_engine_binder_quantity(heap, str_lit("branch binder"), str_lit("field"), f->name, f->q2, f->q),
                    branch_bind_fields_after_ok, f);
}

/* tele: TeleBinder pointers; binders: Binder pointers; env, values: Value pointers. */
Comp check_engine_branch_bind_fields(Heap *heap, Vec tele, Vec binders, Ctx ctx, Vec env, Vec values) {
  Frame *f;
  VecGet first;
  VecGet second;
  const TeleBinder *binder;
  const Binder *pair;
  if (tele.len == 0 && binders.len == 0) return pure_fields(heap, ctx, env, values);
  if (tele.len == 0 || binders.len == 0) {
    return budget_failed(error_make(ERROR_MISSING_BRANCH, str_lit("the branch binds the wrong number of fields")));
  }
  first = vec_get(&tele, 0);
  second = vec_get(&binders, 0);
  if (first.status != VEC_GET_OK || second.status != VEC_GET_OK) return comp_oom();
  binder = first.value;
  pair = second.value;
  f = frame_new_upto(heap, FRAME_UPTO(q2));
  if (f == NULL) return comp_oom();
  f->vec = tele;
  f->vec2 = binders;
  f->ctx = ctx;
  f->vec3 = env;
  f->vec4 = values;
  f->q = binder->quantity;
  f->q2 = check_engine_pair_q(*pair);
  f->name = check_engine_pair_name(*pair);
  f->value = value_var(heap, context_size_of(ctx));
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(ctx), env, binder->ty)),
                    branch_bind_fields_after_ty, f);
}

static Error mu_branch_ctor_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  return error_make(ERROR_UNBOUND, cat3(heap, f->name2, str_lit(" is not a constructor of "), f->name));
}

static Error mu_branch_leg_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, str_lit("the elimination of "), f->name, str_lit(" has no branch at "));
  return error_make(ERROR_MISSING_BRANCH, heap_str_cat(heap, head, f->name2));
}

static Error mu_branch_arity_error(Heap *heap, void *ctx) {
  Frame *f = ctx;
  Str head = cat3(heap, str_lit("the branch at "), f->name2, str_lit(" binds "));
  Str middle = cat3(heap, heap_nat_show(heap, f->nat2), str_lit(" fields and "), f->name2);
  Str tail = heap_str_cat(heap, str_lit(" takes "), heap_nat_show(heap, f->nat));
  return error_make(ERROR_MISSING_BRANCH, cat3(heap, head, middle, tail));
}

static Comp mu_branch_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return check_engine_close(heap, check_engine_fields_ctx(f->fields), context_size_of(f->ctx), f->mode, *uses, false);
}

static Comp mu_branch_after_target(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_check_uses(heap, check_engine_fields_ctx(f->fields), quantity_runtime(f->mode),
                                            check_engine_leg_body(f->leg), value),
                    mu_branch_after_uses, f);
}

static Comp mu_branch_after_indices(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *indices = value;
  Vec values = check_engine_fields_values(f->fields);
  f->vec2 = *indices;
  return bind_frame(heap,
                    check_engine_mu_result(heap, f->ctx, f->mot, f->vec2,
                                           value_vin(heap, shape_smu(f->name, f->vec2), value_vaddr_ctor(f->name2),
                                                     vec_reverse(heap->arena, &values))),
                    mu_branch_after_target, f);
}

static Comp mu_branch_after_fields(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Fields *fields = value;
  f->fields = *fields;
  f->vec3 = check_engine_ctor_indices(f->ctor);
  return bind_frame(heap,
                    lift_vec(heap, eval_eval_list(heap, &f->vec3, context_globals_of(f->ctx),
                                                  check_engine_fields_env(f->fields))),
                    mu_branch_after_indices, f);
}

static Comp mu_branch_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    check_engine_branch_bind_fields(heap, check_engine_ctor_args(f->ctor),
                                                    check_engine_leg_binders(f->leg), f->ctx, f->vec,
                                                    vec_new(heap->arena)),
                    mu_branch_after_fields, f);
}

static Comp mu_branch_after_leg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Leg *leg = value;
  Vec args = check_engine_ctor_args(f->ctor);
  Vec binders;
  f->leg = *leg;
  binders = check_engine_leg_binders(f->leg);
  f->nat = nat_len(heap, &args);
  f->nat2 = nat_len(heap, &binders);
  return bind_frame(heap, check_engine_require_by(heap, nat_is_eq(f->nat, f->nat2), thunk(mu_branch_arity_error, f)),
                    mu_branch_after_ok, f);
}

static Comp mu_branch_after_ctor(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Ctor *ct = value;
  MaybeLegPtr found;
  f->ctor = *ct;
  found = eval_find_branch(f->items, value_vaddr_ctor(f->name2));
  return bind_frame(heap,
                    check_engine_optional_by(heap, found.some, (void *)found.value, thunk(mu_branch_leg_error, f)),
                    mu_branch_after_leg, f);
}

/* penv: Value pointers; branches: Branch pointers. */
Comp check_engine_mu_branch(Heap *heap, Ctx ctx, Quantity mode, Str name, Family fam, Motive motive, Vec penv,
                            const Vec *branches, Str ctor) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(fields));
  MaybeCtor ct;
  Ctor *boxed;
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->name = name;
  f->family = fam;
  f->mot = motive;
  f->vec = penv;
  f->items = branches;
  f->name2 = ctor;
  ct = positivity_ctor_of(ctor, fam);
  boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = ct.value;
  return bind_frame(heap, check_engine_optional_by(heap, ct.some, boxed, thunk(mu_branch_ctor_error, f)),
                    mu_branch_after_ctor, f);
}

static Comp mu_branches_after_next(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *next = value;
  return check_engine_mu_branches(heap, list_tail(f->vec), f->ctx, f->mode, f->name, f->family, f->mot, f->vec2,
                                  f->items, quantity_alternative(heap, f->uses, *next));
}

/* names: Str pointers; penv: Value pointers; branches: Branch pointers. */
Comp check_engine_mu_branches(Heap *heap, Vec names, Ctx ctx, Quantity mode, Str name, Family fam, Motive motive,
                              Vec penv, const Vec *branches, Usage uses) {
  Frame *f;
  VecGet first;
  if (names.len == 0) return pure_usage(heap, uses);
  first = vec_get(&names, 0);
  if (first.status != VEC_GET_OK) return comp_oom();
  f = frame_new_upto(heap, FRAME_UPTO(mot));
  if (f == NULL) return comp_oom();
  f->vec = names;
  f->ctx = ctx;
  f->mode = mode;
  f->name = name;
  f->family = fam;
  f->mot = motive;
  f->vec2 = penv;
  f->items = branches;
  f->uses = uses;
  return bind_frame(heap,
                    check_engine_mu_branch(heap, ctx, mode, name, fam, motive, penv, branches,
                                           *(const Str *)first.value),
                    mu_branches_after_next, f);
}

static Comp mu_elim_elim_after_result(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_inferred(heap, value,
                       quantity_sequence(heap, check_engine_inferred_uses(f->inferred),
                                         quantity_scale(heap, f->mode, f->uses)));
}

static Comp mu_elim_elim_after_scrut(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_mu_result(heap, f->ctx, f->mot, check_engine_mu_index_values(f->mu), value),
                    mu_elim_elim_after_result, f);
}

static Comp mu_elim_elim_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, check_engine_elim_scrut(f->data)), mu_elim_elim_after_scrut,
                    f);
}

static Comp mu_elim_elim_after_covered(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap,
                    check_engine_mu_branches(heap, f->vec2, f->ctx, f->mode, check_engine_mu_name(f->mu), f->family,
                                             f->mot, f->vec, check_engine_elim_branches(f->data),
                                             quantity_unreachable(heap)),
                    mu_elim_elim_after_uses, f);
}

static Comp mu_elim_elim_after_names(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *names = value;
  f->vec2 = *names;
  return bind_frame(heap,
                    check_engine_mu_cover(heap, check_engine_mu_name(f->mu), &f->vec2,
                                          check_engine_elim_branches(f->data)),
                    mu_elim_elim_after_covered, f);
}

static Comp mu_elim_elim_after_large(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_mu_ctor_names(heap, f->family, check_engine_mu_name(f->mu)),
                    mu_elim_elim_after_names, f);
}

static Comp mu_elim_elim_after_level(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_mu_large(heap, check_engine_mu_name(f->mu), f->family, value),
                    mu_elim_elim_after_large, f);
}

static Comp mu_elim_elim_after_penv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *penv = value;
  f->vec = *penv;
  return bind_frame(heap,
                    check_engine_mu_motive_level(heap, f->ctx, check_engine_mu_name(f->mu), f->family, f->mot,
                                                 f->former, f->vec),
                    mu_elim_elim_after_level, f);
}

static Comp mu_elim_elim_after_motive(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Motive *motive = value;
  f->mot = *motive;
  return bind_frame(heap, lift_vec(heap, rules_mu_param_env(heap, f->ctx, check_engine_former_clo(f->former))),
                    mu_elim_elim_after_penv, f);
}

static Comp mu_elim_elim_after_family(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Family *fam = value;
  f->family = *fam;
  f->vec3 = check_engine_mu_index_values(f->mu);
  return bind_frame(heap,
                    check_engine_mu_motive_of(heap, check_engine_mu_name(f->mu), f->family, &f->vec3, f->data),
                    mu_elim_elim_after_motive, f);
}

static Comp mu_elim_elim_after_mu(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Mu *mu = value;
  f->mu = *mu;
  return bind_frame(heap, lift_family(heap, rules_mu_family(heap, f->ctx, check_engine_mu_name(f->mu))),
                    mu_elim_elim_after_family, f);
}

static Comp mu_elim_elim_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_mu_view(heap, check_engine_former_shape(f->former)), mu_elim_elim_after_mu, f);
}

static Comp mu_elim_elim_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, true, check_engine_inferred_ty(f->inferred),
                                             str_lit("the scrutinee is not a left former")),
                    mu_elim_elim_after_former, f);
}

Comp check_engine_mu_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(mu));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->data = data;
  return bind_frame(heap, check_engine_infer_scrutinee(heap, ctx, mode, data), mu_elim_elim_after_got, f);
}

/* The ready value is a Vec of CircuitGlobal pointers (Pair<String, Term.T> rows of the definitions only). */
Vec check_engine_definition_rows(Heap *heap, const EntryRows *entries) {
  CircuitGlobal *row;
  Vec rest;
  if (entries == NULL) return vec_new(heap->arena);
  rest = check_engine_definition_rows(heap, entries->rest);
  if (entries->entry.kind != ENTRY_DEF) return rest;
  row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return rest;
  row->name = entries->name;
  row->term = entries->entry.def.body;
  return cons(heap, row, &rest);
}

Vec check_engine_globals_rows(Heap *heap, Globals globals) {
  return check_engine_definition_rows(heap, globals.entries);
}

Comp check_engine_circuit_result(Heap *heap, CircuitBigResult result) {
  switch (result.kind) {
  case RESULT_DONE: return pure_big(heap, result.value);
  case RESULT_FAIL: return budget_failed(error_make(ERROR_NOT_YET, circuit_word(heap, result.error)));
  }
  return comp_oom();
}

Comp check_engine_circuit(Heap *heap, Ctx ctx, const Term *term) {
  Vec globals = check_engine_globals_rows(heap, context_globals_of(ctx));
  return check_engine_circuit_result(heap, circuit_depth(heap, &globals, term));
}

static Comp zk_form_after_depth(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_level(level_max(heap, f->level, f->level2));
}

static Comp zk_form_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_circuit(heap, f->inner, f->term2), zk_form_after_depth, f);
}

static Comp zk_form_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->level2 = value;
  return bind_frame(heap,
                    check_engine_require(level_equal(heap, f->level2, level_zero()),
                                         error_make(ERROR_MISMATCH,
                                                    str_lit("a zk type hides the witness of a proposition"))),
                    zk_form_after_ok, f);
}

static Comp zk_form_after_dom_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->inner = context_bind(heap, f->name, f->q, value, f->ctx);
  return bind_frame(heap, check_engine_infer_univ(heap, f->inner, f->term2), zk_form_after_cod, f);
}

static Comp zk_form_after_dom_level(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->level = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), zk_form_after_dom_value, f);
}

Comp check_engine_zk_form(Heap *heap, Ctx ctx, Quantity q, Str name, const Term *dom, const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(level2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->q = q;
  f->name = name;
  f->term = dom;
  f->term2 = diagram;
  return bind_frame(heap, check_engine_infer_univ(heap, ctx, dom), zk_form_after_dom_level, f);
}

static Comp proof_args_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  return pure_proof(heap, f->term, check_engine_inferred_uses(*got));
}

/* args: Term pointers. The ready value is a heap boxed ProofArgs. */
Comp check_engine_proof_args(Heap *heap, Vec args, Ctx ctx, Quantity mode) {
  Frame *f;
  VecGet first = vec_get(&args, 0);
  VecGet second = vec_get(&args, 1);
  if (args.len == 1 && first.status == VEC_GET_OK) return pure_proof(heap, first.value, quantity_empty(heap));
  if (args.len != 2 || first.status != VEC_GET_OK || second.status != VEC_GET_OK) {
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a proof carries one relation leg")));
  }
  f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->term = first.value;
  return bind_frame(heap, check_engine_infer_uses(heap, ctx, mode, second.value), proof_args_after_got, f);
}

static Comp zk_prove_after_fibre(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *fibre_uses = value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, quantity_sequence(heap, *fibre_uses, f->proof.uses)));
}

static Comp zk_prove_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, f->mode, f->proof.fibre, value), zk_prove_after_fibre,
                    f);
}

static Comp zk_prove_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_open(heap, f->ctx, f->clo, singleton(heap, value)), zk_prove_after_cod, f);
}

static Comp zk_prove_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *point_uses = value;
  f->uses = *point_uses;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term), zk_prove_after_value, f);
}

Comp check_engine_zk_prove(Heap *heap, ProofArgs args, Ctx ctx, Quantity mode, const Value *dom, Closure clo,
                           const Term *point) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(proof));
  if (f == NULL) return comp_oom();
  f->proof = args;
  f->ctx = ctx;
  f->mode = mode;
  f->clo = clo;
  f->term = point;
  return bind_frame(heap, check_engine_check_uses(heap, ctx, quantity_mul(mode, QUANTITY_ONE), point, dom),
                    zk_prove_after_point, f);
}

static Comp zk_intro_in_after_proof(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const ProofArgs *proof = value;
  return check_engine_zk_prove(heap, *proof, f->ctx, f->mode, check_engine_point_dom(f->point),
                               check_engine_former_clo(f->former), f->term);
}

static Comp zk_intro_in_after_arg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term = value;
  return bind_frame(heap, check_engine_proof_args(heap, *f->items, f->ctx, f->mode), zk_intro_in_after_proof, f);
}

static Comp zk_intro_in_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap, check_engine_point_arg(f->addr, str_lit("a proof takes the point address")),
                    zk_intro_in_after_arg, f);
}

static Comp zk_intro_in_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, check_engine_former_shape(f->former)),
                    zk_intro_in_after_point, f);
}

/* args: Term pointers. */
Comp check_engine_zk_intro_in(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Vec *args, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(addr));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->addr = addr;
  f->items = args;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, true, expected,
                                             str_lit("a proof needs a left former as its expected type")),
                    zk_intro_in_after_former, f);
}

Comp check_engine_fhc_shape(Shape shape, Str message) {
  if (shape.kind == SHAPE_FHC) return pure_value(shape.level);
  return budget_failed(error_make(ERROR_MISMATCH, message));
}

Comp check_engine_literal_level(Heap *heap, const Value *value) {
  if (value->kind == VALUE_LIT && value->lit.kind == LITERAL_INT) return pure_big(heap, value->lit.integer);
  return budget_failed(error_make(ERROR_MISMATCH, str_lit("the level of a ciphertext type is a Nat literal")));
}

static Comp fhc_level_after_reduced(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return check_engine_literal_level(heap, value);
}

Comp check_engine_fhc_level(Heap *heap, Ctx ctx, const Value *value) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  return bind_frame(heap, check_engine_whnf(heap, ctx, value), fhc_level_after_reduced, f);
}

/* ---- Bend lines 1250-1392 (S1.9 hop 27): the fhc laws (fragment, form, eval, enc, dec) ---- */

static Comp fragment_legs_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_fragment_legs(heap, list_tail(f->vec), f->ctx);
}

static Comp fragment_legs_after_open(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_fhc_fragment(heap, f->ctx, value), fragment_legs_after_ok, f);
}

/* legs: VLeg pointers (list view by value). */
Comp check_engine_fragment_legs(Heap *heap, Vec legs, Ctx ctx) {
  Frame *f;
  VecGet first;
  const VLeg *leg;
  if (legs.len == 0) return pure_unit();
  first = vec_get(&legs, 0);
  if (first.status != VEC_GET_OK) return comp_oom();
  leg = first.value;
  f = frame_new_upto(heap, FRAME_UPTO(vec));
  if (f == NULL) return comp_oom();
  f->vec = legs;
  f->ctx = ctx;
  return bind_frame(heap, check_engine_open(heap, ctx, leg->clo, vec_new(heap->arena)), fragment_legs_after_open, f);
}

static Comp fragment_former_after_legs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *legs = value;
  return check_engine_fragment_legs(heap, *legs, f->ctx);
}

Comp check_engine_fragment_former(Heap *heap, Shape shape, Closure clo, Ctx ctx) {
  Frame *f;
  if (shape.kind != SHAPE_COLL) {
    return budget_failed(error_text(ERROR_MISMATCH, "a ciphertext carries a fragment type"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, ctx, clo)), fragment_former_after_legs, f);
}

Comp check_engine_fragment_value(Heap *heap, const Value *value, Ctx ctx) {
  switch (value->kind) {
  case VALUE_NEUTRAL:
    if (value->head.kind == HEAD_GLOBAL && value->spine.len == 0) {
      return check_engine_require(str_eq(value->head.name, str_lit("Nat")),
                                  error_text(ERROR_MISMATCH, "a ciphertext carries a fragment type"));
    }
    return budget_failed(error_text(ERROR_MISMATCH, "a ciphertext carries a fragment type"));
  case VALUE_LAN:
  case VALUE_RAN:
    return check_engine_fragment_former(heap, value->shape, value->diagram, ctx);
  case VALUE_UNIV:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
    return budget_failed(error_text(ERROR_MISMATCH, "a ciphertext carries a fragment type"));
  }
  return comp_oom();
}

static Comp fhc_fragment_after_whnf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_fragment_value(heap, value, f->ctx);
}

Comp check_engine_fhc_fragment(Heap *heap, Ctx ctx, const Value *ty) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, check_engine_whnf(heap, ctx, ty), fhc_fragment_after_whnf, f);
}

static Comp fhc_form_after_fragment(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)heap;
  (void)value;
  return pure_level(f->level);
}

static Comp fhc_form_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_fhc_fragment(heap, f->ctx, value), fhc_form_after_fragment, f);
}

static Comp fhc_form_after_universe(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->level = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term2), fhc_form_after_ty, f);
}

static Comp fhc_form_after_number(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_univ(heap, f->ctx, f->term2), fhc_form_after_universe, f);
}

static Comp fhc_form_after_value(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_fhc_level(heap, f->ctx, value), fhc_form_after_number, f);
}

Comp check_engine_fhc_form(Heap *heap, Ctx ctx, const Term *level, const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(level));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->term = level;
  f->term2 = diagram;
  return bind_frame(heap, check_engine_eval(heap, ctx, level), fhc_form_after_value, f);
}

/* Frame: big = out_level, value = out_ty, value2 = fty, uses = f_uses, term = function, term2 = cipher,
   big2 = depth, former = ff, point, inferred = got, former2 = cf, value3 = lv, big3 = in_level,
   value4 = in_ty, flag = dom_ok, value5 = cod. */
static Comp fhc_eval_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, check_engine_inferred_uses(f->inferred)));
}

static Comp fhc_eval_after_cod_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_require(f->flag && kernel_bool_unbox(value),
                                         error_text(ERROR_MISMATCH,
                                                    "eval takes a function from the plaintext type to the result type")),
                    fhc_eval_after_ok, f);
}

static Comp fhc_eval_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value5 = value;
  return bind_frame(heap,
                    check_engine_conv_type(heap,
                                           context_bind(heap, check_engine_point_name(f->point),
                                                        check_engine_point_q(f->point),
                                                        check_engine_point_dom(f->point), f->ctx),
                                           f->value5, f->value),
                    fhc_eval_after_cod_ok, f);
}

static Comp fhc_eval_after_dom_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->flag = kernel_bool_unbox(value);
  return bind_frame(heap,
                    check_engine_open(heap, f->ctx, check_engine_former_clo(f->former),
                                      singleton(heap, (void *)value_var(heap, context_size_of(f->ctx)))),
                    fhc_eval_after_cod, f);
}

static Comp fhc_eval_after_grow(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_conv_type(heap, f->ctx, check_engine_point_dom(f->point), f->value4),
                    fhc_eval_after_dom_ok, f);
}

static Comp fhc_eval_after_in_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(
      heap,
      check_engine_require(big_equal(f->big, big_add(heap, f->big3, f->big2)),
                           error_text(ERROR_MISMATCH,
                                      "the level of the result is the level of the argument plus the depth of the function")),
      fhc_eval_after_grow, f);
}

static Comp fhc_eval_after_in_level(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *in_level = value;
  f->big3 = *in_level;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former2), vec_new(heap->arena)),
                    fhc_eval_after_in_ty, f);
}

static Comp fhc_eval_after_lv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value3 = value;
  return bind_frame(heap, check_engine_fhc_level(heap, f->ctx, f->value3), fhc_eval_after_in_level, f);
}

static Comp fhc_eval_after_cf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former2 = *former;
  return bind_frame(heap,
                    check_engine_fhc_shape(check_engine_former_shape(f->former2),
                                           str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_eval_after_lv, f);
}

static Comp fhc_eval_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, check_engine_inferred_ty(f->inferred),
                                             str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_eval_after_cf, f);
}

static Comp fhc_eval_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, f->mode, f->term2), fhc_eval_after_got, f);
}

static Comp fhc_eval_after_ff(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, check_engine_former_shape(f->former)), fhc_eval_after_point,
                    f);
}

static Comp fhc_eval_after_depth(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *depth = value;
  f->big2 = *depth;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, f->value2,
                                             str_lit("eval takes a function from the plaintext type to the result type")),
                    fhc_eval_after_ff, f);
}

Comp check_engine_fhc_eval(Heap *heap, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty, const Value *fty,
                           Usage f_uses, const Term *function, const Term *cipher) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(former2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->big = out_level;
  f->value = out_ty;
  f->value2 = fty;
  f->uses = f_uses;
  f->term = function;
  f->term2 = cipher;
  return bind_frame(heap, check_engine_circuit(heap, ctx, function), fhc_eval_after_depth, f);
}

static Comp fhc_const_pi_after_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Term *term = value;
  return pure_value(value_vran(heap, shape_spi(QUANTITY_MANY, str_lit("_"), (void *)f->value),
                               value_close(context_env_of(f->ctx), term), level_none()));
}

Comp check_engine_fhc_const_pi(Heap *heap, Ctx ctx, const Value *dom, const Value *cod) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = dom;
  return bind_frame(heap, check_engine_quote(heap, context_bind(heap, str_lit("_"), QUANTITY_MANY, dom, ctx), cod),
                    fhc_const_pi_after_term, f);
}

/* Frame: big = out_level, value = out_ty, term = function, term2 = cipher, big2 = depth, inferred = got,
   former = cf, value2 = lv, big3 = in_level, value3 = in_ty, value4 = pi. */
static Comp fhc_cipher_first_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_usage(heap, quantity_sequence(heap, *uses, check_engine_inferred_uses(f->inferred)));
}

static Comp fhc_cipher_first_after_pi(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_MANY), f->term, f->value4),
                    fhc_cipher_first_after_uses, f);
}

static Comp fhc_cipher_first_after_grow(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_fhc_const_pi(heap, f->ctx, f->value3, f->value), fhc_cipher_first_after_pi, f);
}

static Comp fhc_cipher_first_after_in_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value3 = value;
  return bind_frame(
      heap,
      check_engine_require(big_equal(f->big, big_add(heap, f->big3, f->big2)),
                           error_text(ERROR_MISMATCH,
                                      "the level of the result is the level of the argument plus the depth of the function")),
      fhc_cipher_first_after_grow, f);
}

static Comp fhc_cipher_first_after_in_level(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *in_level = value;
  f->big3 = *in_level;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former), vec_new(heap->arena)),
                    fhc_cipher_first_after_in_ty, f);
}

static Comp fhc_cipher_first_after_lv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value2 = value;
  return bind_frame(heap, check_engine_fhc_level(heap, f->ctx, f->value2), fhc_cipher_first_after_in_level, f);
}

static Comp fhc_cipher_first_after_cf(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap,
                    check_engine_fhc_shape(check_engine_former_shape(f->former),
                                           str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_cipher_first_after_lv, f);
}

static Comp fhc_cipher_first_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, check_engine_inferred_ty(f->inferred),
                                             str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_cipher_first_after_cf, f);
}

static Comp fhc_cipher_first_after_depth(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *depth = value;
  f->big2 = *depth;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_MANY), f->term2),
                    fhc_cipher_first_after_got, f);
}

Comp check_engine_fhc_cipher_first(Heap *heap, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty,
                                   const Term *function, const Term *cipher) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(big3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->big = out_level;
  f->value = out_ty;
  f->term = function;
  f->term2 = cipher;
  return bind_frame(heap, check_engine_circuit(heap, ctx, function), fhc_cipher_first_after_depth, f);
}

bool check_engine_is_function(const Value *value) {
  return value->kind == VALUE_RAN && (value->shape.kind == SHAPE_PI || value->shape.kind == SHAPE_ZK);
}

static Comp fhc_head_function_after_tail(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *tail = value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, *tail));
}

static Comp fhc_head_function_after_zero(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, f->mode, f->term2, f->value),
                    fhc_head_function_after_tail, f);
}

Comp check_engine_fhc_head_function(Heap *heap, bool is_fun, Ctx ctx, Quantity mode, Big out_level,
                                    const Value *out_ty, const Value *fw, Usage uses, const Term *first,
                                    const Term *second) {
  Frame *f;
  if (is_fun) {
    return check_engine_fhc_eval(heap, ctx, mode, out_level, out_ty, fw, uses, first, second);
  } else {
    f = frame_new_upto(heap, FRAME_UPTO(term2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->value = out_ty;
    f->uses = uses;
    f->term2 = second;
    return bind_frame(heap,
                      check_engine_require(big_equal(out_level, big_zero()),
                                           error_text(ERROR_MISMATCH, "enc starts at level 0")),
                      fhc_head_function_after_zero, f);
  }
}

static Comp fhc_head_result_after_fw(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Value *fw = value;
  return check_engine_fhc_head_function(heap, check_engine_is_function(fw), f->ctx, f->mode, f->big, f->value, fw,
                                        f->uses, f->term, f->term2);
}

/* result: the boxed Result of a budget_attempt over infer_uses (an Inferred pointer when done). */
Comp check_engine_fhc_head_result(Heap *heap, const Result *result, Ctx ctx, Quantity mode, Big out_level,
                                  const Value *out_ty, const Term *first, const Term *second) {
  Frame *f;
  const Inferred *got;
  switch (result->kind) {
  case RESULT_FAIL:
    return check_engine_fhc_cipher_first(heap, ctx, mode, out_level, out_ty, first, second);
  case RESULT_DONE:
    got = result->value;
    f = frame_new_upto(heap, FRAME_UPTO(big));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->big = out_level;
    f->value = out_ty;
    f->uses = check_engine_inferred_uses(*got);
    f->term = first;
    f->term2 = second;
    return bind_frame(heap, check_engine_whnf(heap, ctx, check_engine_inferred_ty(*got)), fhc_head_result_after_fw, f);
  }
  return comp_oom();
}

static Comp fhc_legs_after_attempt(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_fhc_head_result(heap, value, f->ctx, f->mode, f->big, f->value, f->term, f->term2);
}

/* legs: Leg pointers. */
Comp check_engine_fhc_legs(Heap *heap, const Vec *legs, Ctx ctx, Quantity mode, Big out_level, const Value *out_ty) {
  Frame *f;
  VecGet first;
  VecGet second;
  const Leg *head;
  const Leg *tail;
  if (legs->len != 2) {
    return budget_failed(error_text(ERROR_MISMATCH, "a ciphertext section takes two arguments"));
  }
  first = vec_get(legs, 0);
  second = vec_get(legs, 1);
  if (first.status != VEC_GET_OK || second.status != VEC_GET_OK) return comp_oom();
  head = first.value;
  tail = second.value;
  if (head->binders.len != 0 || tail->binders.len != 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "a ciphertext section takes two arguments"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(big));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->big = out_level;
  f->value = out_ty;
  f->term = head->body;
  f->term2 = tail->body;
  return bind_frame(
      heap,
      budget_attempt(heap, check_engine_infer_uses(heap, ctx, quantity_mul(mode, QUANTITY_MANY), head->body)),
      fhc_legs_after_attempt, f);
}

static Comp fhc_intro_sec_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_fhc_legs(heap, f->items, f->ctx, f->mode, f->big, value);
}

static Comp fhc_intro_sec_after_level(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *level = value;
  f->big = *level;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former), vec_new(heap->arena)),
                    fhc_intro_sec_after_ty, f);
}

static Comp fhc_intro_sec_after_lv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_fhc_level(heap, f->ctx, f->value), fhc_intro_sec_after_level, f);
}

static Comp fhc_intro_sec_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap,
                    check_engine_fhc_shape(check_engine_former_shape(f->former),
                                           str_lit("the rule pack does not match the shape of the term")),
                    fhc_intro_sec_after_lv, f);
}

Comp check_engine_fhc_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(big));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->items = legs;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, false, expected,
                                             str_lit("a ciphertext needs a right former as its expected type")),
                    fhc_intro_sec_after_former, f);
}

/* Frame: term = key, term2 = head, inferred = got, former = f, value = lv, inferred2 = key_got. */
static Comp fhc_elim_out_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_inferred(heap, value,
                       quantity_sequence(heap, check_engine_inferred_uses(f->inferred),
                                         check_engine_inferred_uses(f->inferred2)));
}

static Comp fhc_elim_out_after_key(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *key_got = value;
  f->inferred2 = *key_got;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former), vec_new(heap->arena)),
                    fhc_elim_out_after_ty, f);
}

static Comp fhc_elim_out_after_n(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_ONE), f->term),
                    fhc_elim_out_after_key, f);
}

static Comp fhc_elim_out_after_lv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_fhc_level(heap, f->ctx, f->value), fhc_elim_out_after_n, f);
}

static Comp fhc_elim_out_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap,
                    check_engine_fhc_shape(check_engine_former_shape(f->former),
                                           str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_elim_out_after_lv, f);
}

static Comp fhc_elim_out_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, check_engine_inferred_ty(f->inferred),
                                             str_lit("the argument of eval or dec is not a ciphertext")),
                    fhc_elim_out_after_former, f);
}

static Comp fhc_elim_out_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, f->mode, f->term2), fhc_elim_out_after_got, f);
}

Comp check_engine_fhc_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head) {
  Frame *f;
  switch (addr.kind) {
  case ADDR_PT:
    f = frame_new_upto(heap, FRAME_UPTO(inferred2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->term = addr.arg;
    f->term2 = head;
    return bind_frame(heap,
                      check_engine_require(quantity_equal(addr.quantity, QUANTITY_ONE),
                                           error_text(ERROR_QUANTITY, "the key of dec is consumed once")),
                      fhc_elim_out_after_ok, f);
  case ADDR_LEG:
  case ADDR_CTOR:
    return budget_failed(error_text(ERROR_WRONG_LEG, "dec takes the key address"));
  }
  return comp_oom();
}

/* Bend lines 1394-1642 (S1.9 hop 28): the mpc laws and the shape dispatch. */
static Comp pure_mpc(Heap *heap, const Value *parties, const Value *access) {
  Mpc *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->parties = parties;
  boxed->access = access;
  return budget_pure(boxed);
}

static Comp lift_big(Heap *heap, BigResult result) {
  switch (result.kind) {
  case RESULT_DONE:
    return pure_big(heap, result.value);
  case RESULT_FAIL:
    return budget_failed(result.error);
  }
  return comp_oom();
}

static Comp pure_pair(Heap *heap, void *first, void *second) {
  Pair *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  boxed->first = first;
  boxed->second = second;
  return budget_pure(boxed);
}

Comp check_engine_mpc_view(Heap *heap, Shape shape, Str message) {
  switch (shape.kind) {
  case SHAPE_MPC:
    return pure_mpc(heap, shape.parties, shape.access);
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
    return budget_failed(error_make(ERROR_MISMATCH, message));
  }
  return comp_oom();
}

const Value *check_engine_mpc_parties(Mpc value) { return value.parties; }

const Value *check_engine_mpc_access(Mpc value) { return value.access; }

/* Frame: term = access, term2 = diagram, big = count, term3 = pred_term. */
static Comp mpc_form_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_infer_univ(heap, f->ctx, f->term2);
}

static Comp mpc_form_after_pred(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, QUANTITY_ZERO, f->term, value), mpc_form_after_uses,
                    f);
}

static Comp mpc_form_after_pred_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term3 = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term3), mpc_form_after_pred, f);
}

static Comp mpc_form_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  f->big = *count;
  return bind_frame(heap, budget_lift(rules_mpc_pred_ty(heap, f->big)), mpc_form_after_pred_term, f);
}

static Comp mpc_form_after_pv(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, lift_big(heap, rules_mpc_parties(heap, f->ctx, value)), mpc_form_after_count, f);
}

Comp check_engine_mpc_form(Heap *heap, Ctx ctx, const Term *parties, const Term *access, const Term *diagram) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->term = access;
  f->term2 = diagram;
  return bind_frame(heap, check_engine_eval(heap, ctx, parties), mpc_form_after_pv, f);
}

/* Frame: value = pv, value2 = av. */
static Comp mpc_share_ty_after_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_value(value_vran(heap, shape_smpc((void *)f->value, (void *)f->value2),
                               value_close(context_env_of(f->ctx), value), level_none()));
}

Comp check_engine_mpc_share_ty(Heap *heap, Ctx ctx, const Value *pv, const Value *av, const Value *ty) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = pv;
  f->value2 = av;
  return bind_frame(heap, check_engine_quote(heap, ctx, ty), mpc_share_ty_after_term, f);
}

/* Frame: vec = shares, term = share, mpc, former = f, point, uses, inferred = tail. */
static Comp mpc_shares_after_tail(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *tail = value;
  f->inferred = *tail;
  return pure_inferred(heap, check_engine_inferred_ty(f->inferred),
                       quantity_sequence(heap, f->uses, check_engine_inferred_uses(f->inferred)));
}

static Comp mpc_shares_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_mpc_shares(heap, list_tail(f->vec), f->ctx, f->mode, f->mpc, value),
                    mpc_shares_after_tail, f);
}

static Comp mpc_shares_after_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap,
                    check_engine_open(heap, f->ctx, check_engine_former_clo(f->former),
                                      singleton(heap, (void *)value_var(heap, context_size_of(f->ctx)))),
                    mpc_shares_after_cod, f);
}

static Comp mpc_shares_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_ONE), f->term, value),
                    mpc_shares_after_uses, f);
}

static Comp mpc_shares_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Point *point = value;
  f->point = *point;
  return bind_frame(heap,
                    check_engine_mpc_share_ty(heap, f->ctx, check_engine_mpc_parties(f->mpc),
                                              check_engine_mpc_access(f->mpc), check_engine_point_dom(f->point)),
                    mpc_shares_after_ty, f);
}

static Comp mpc_shares_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap, check_engine_view_point(heap, check_engine_former_shape(f->former)), mpc_shares_after_point,
                    f);
}

Comp check_engine_mpc_shares(Heap *heap, Vec shares, Ctx ctx, Quantity mode, Mpc mpc, const Value *fty) {
  Frame *f;
  VecGet first;
  if (shares.len == 0) return pure_inferred(heap, fty, quantity_empty(heap));
  first = vec_get(&shares, 0);
  if (first.status != VEC_GET_OK) return comp_oom();
  f = frame_new_upto(heap, FRAME_UPTO(mpc));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->mpc = mpc;
  f->vec = shares;
  f->term = first.value;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, false, fty,
                                             str_lit("mpc takes a function from the share types to the result type")),
                    mpc_shares_after_former, f);
}

/* Frame: value = out_ty, term = ps, term2 = function, vec = shares, big = count, term3 = sub_term, uses = set_uses,
   inferred = got, inferred2 = rest. */
static Comp mpc_join_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return pure_usage(heap, quantity_sequence(heap, f->uses,
                                            quantity_sequence(heap, check_engine_inferred_uses(f->inferred),
                                                              check_engine_inferred_uses(f->inferred2))));
}

static Comp mpc_join_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(
      heap,
      check_engine_require(kernel_bool_unbox(value),
                           error_text(ERROR_MISMATCH,
                                      "the result of mpc is the type that remains after the last share")),
      mpc_join_after_ok, f);
}

static Comp mpc_join_after_rest(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *rest = value;
  f->inferred2 = *rest;
  return bind_frame(heap, check_engine_conv_type(heap, f->ctx, check_engine_inferred_ty(f->inferred2), f->value),
                    mpc_join_after_same, f);
}

static Comp mpc_join_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_mpc_shares(heap, f->vec, f->ctx, f->mode, f->mpc,
                                            check_engine_inferred_ty(f->inferred)),
                    mpc_join_after_rest, f);
}

static Comp mpc_join_after_depth(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_infer_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_MANY), f->term2),
                    mpc_join_after_got, f);
}

static Comp mpc_join_after_set_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_circuit(heap, f->ctx, f->term2), mpc_join_after_depth, f);
}

static Comp mpc_join_after_sub(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_MANY), f->term, value),
                    mpc_join_after_set_uses, f);
}

static Comp mpc_join_after_sub_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term3 = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term3), mpc_join_after_sub, f);
}

static Comp mpc_join_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  f->big = *count;
  return bind_frame(heap, budget_lift(rules_mpc_sub_ty(heap, f->big)), mpc_join_after_sub_term, f);
}

Comp check_engine_mpc_join(Heap *heap, Ctx ctx, Quantity mode, Mpc mpc, const Value *out_ty, const Term *ps,
                           const Term *function, Vec shares) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(inferred2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->mpc = mpc;
  f->value = out_ty;
  f->term = ps;
  f->term2 = function;
  f->vec = shares;
  return bind_frame(heap, lift_big(heap, rules_mpc_parties(heap, ctx, check_engine_mpc_parties(mpc))),
                    mpc_join_after_count, f);
}

/* Frame: term = body. */
static Comp mpc_args_after_others(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_vec(heap, cons(heap, (void *)f->term, value));
}

Comp check_engine_mpc_args(Heap *heap, Vec legs) {
  Frame *f;
  VecGet first;
  const Leg *leg;
  if (legs.len == 0) return pure_vec(heap, vec_new(heap->arena));
  first = vec_get(&legs, 0);
  if (first.status != VEC_GET_OK) return comp_oom();
  leg = first.value;
  if (leg->binders.len != 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "an mpc section leg binds nothing"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->term = leg->body;
  return bind_frame(heap, check_engine_mpc_args(heap, list_tail(legs)), mpc_args_after_others, f);
}

/* Frame: value = out_ty, term = x, uses = party_uses. */
static Comp mpc_intro_args_after_x(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  return pure_usage(heap, quantity_sequence(heap, f->uses, *uses));
}

static Comp mpc_intro_args_after_party(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap,
                    check_engine_check_uses(heap, f->ctx, quantity_mul(f->mode, QUANTITY_ONE), f->term, f->value),
                    mpc_intro_args_after_x, f);
}

Comp check_engine_mpc_intro_args(Heap *heap, Vec args, Ctx ctx, Quantity mode, Mpc mpc, const Value *out_ty) {
  Frame *f;
  VecGet first;
  VecGet second;
  if (args.len == 0) {
    return budget_failed(error_text(ERROR_MISMATCH, "an mpc section takes one argument or more"));
  }
  first = vec_get(&args, 0);
  if (first.status != VEC_GET_OK) return comp_oom();
  if (args.len == 1) {
    return check_engine_check_uses(heap, ctx, quantity_mul(mode, QUANTITY_ONE), first.value, out_ty);
  }
  second = vec_get(&args, 1);
  if (second.status != VEC_GET_OK) return comp_oom();
  if (args.len == 2) {
    f = frame_new_upto(heap, FRAME_UPTO(uses));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->value = out_ty;
    f->term = second.value;
    return bind_frame(heap,
                      check_engine_check_uses(heap, ctx, quantity_mul(mode, QUANTITY_MANY), first.value,
                                              check_engine_mpc_parties(mpc)),
                      mpc_intro_args_after_party, f);
  }
  return check_engine_mpc_join(heap, ctx, mode, mpc, out_ty, first.value, second.value, list_tail(list_tail(args)));
}

/* Frame: items = legs, former = f, mpc, value = ty. */
static Comp mpc_intro_sec_after_args(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *args = value;
  return check_engine_mpc_intro_args(heap, *args, f->ctx, f->mode, f->mpc, f->value);
}

static Comp mpc_intro_sec_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_mpc_args(heap, *f->items), mpc_intro_sec_after_args, f);
}

static Comp mpc_intro_sec_after_mpc(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Mpc *mpc = value;
  f->mpc = *mpc;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former), vec_new(heap->arena)),
                    mpc_intro_sec_after_ty, f);
}

static Comp mpc_intro_sec_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap,
                    check_engine_mpc_view(heap, check_engine_former_shape(f->former),
                                          str_lit("the rule pack does not match the shape of the term")),
                    mpc_intro_sec_after_mpc, f);
}

Comp check_engine_mpc_intro_sec(Heap *heap, Ctx ctx, Quantity mode, const Vec *legs, const Value *expected) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(mpc));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->items = legs;
  return bind_frame(heap,
                    check_engine_former_view(heap, ctx, false, expected,
                                             str_lit("a share needs a right former as its expected type")),
                    mpc_intro_sec_after_former, f);
}

/* Frame: term = qset. */
static Comp mpc_address_after_proof(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_pair(heap, (void *)f->term, value);
}

Comp check_engine_mpc_address(Heap *heap, const Term *point) {
  Frame *f;
  bool pointed = point->kind == TERM_IN && (point->shape.kind == SHAPE_PI || point->shape.kind == SHAPE_ZK) &&
                 point->addr.kind == ADDR_PT;
  if (!pointed) {
    return budget_failed(
        error_text(ERROR_WRONG_LEG, "open takes the party set address with the authorization proof"));
  }
  f = frame_new_upto(heap, FRAME_UPTO(term));
  if (f == NULL) return comp_oom();
  f->term = point->addr.arg;
  return bind_frame(heap,
                    check_engine_one(&point->args, error_text(ERROR_MISMATCH, "open carries one authorization proof")),
                    mpc_address_after_proof, f);
}

/* Frame: term = qset, term2 = proof, inferred = got, former = f, mpc, big = count, term3 = sub_term, value = sub,
   uses = set_uses, term4 = access, uses2 = proof_uses. */
static Comp mpc_open_pair_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return pure_inferred(heap, value,
                       quantity_sequence(heap, check_engine_inferred_uses(f->inferred),
                                         quantity_sequence(heap, f->uses, f->uses2)));
}

static Comp mpc_open_pair_after_proof_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses2 = *uses;
  return bind_frame(heap, check_engine_open(heap, f->ctx, check_engine_former_clo(f->former), vec_new(heap->arena)),
                    mpc_open_pair_after_ty, f);
}

static Comp mpc_open_pair_after_prop(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, QUANTITY_ZERO, f->term2, value),
                    mpc_open_pair_after_proof_uses, f);
}

static Comp mpc_open_pair_after_access(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term4 = value;
  return bind_frame(heap,
                    check_engine_eval(heap, f->ctx,
                                      term_out(heap, shape_spi(QUANTITY_ZERO, str_lit("q"), (void *)f->term3),
                                               term_addr_pt(QUANTITY_ZERO, f->term), f->term4)),
                    mpc_open_pair_after_prop, f);
}

static Comp mpc_open_pair_after_set_uses(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Usage *uses = value;
  f->uses = *uses;
  return bind_frame(heap, check_engine_quote(heap, f->ctx, check_engine_mpc_access(f->mpc)),
                    mpc_open_pair_after_access, f);
}

static Comp mpc_open_pair_after_sub(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_check_uses(heap, f->ctx, QUANTITY_ZERO, f->term, f->value),
                    mpc_open_pair_after_set_uses, f);
}

static Comp mpc_open_pair_after_sub_term(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->term3 = value;
  return bind_frame(heap, check_engine_eval(heap, f->ctx, f->term3), mpc_open_pair_after_sub, f);
}

static Comp mpc_open_pair_after_count(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Big *count = value;
  f->big = *count;
  return bind_frame(heap, budget_lift(rules_mpc_sub_ty(heap, f->big)), mpc_open_pair_after_sub_term, f);
}

static Comp mpc_open_pair_after_mpc(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Mpc *mpc = value;
  f->mpc = *mpc;
  return bind_frame(heap, lift_big(heap, rules_mpc_parties(heap, f->ctx, check_engine_mpc_parties(f->mpc))),
                    mpc_open_pair_after_count, f);
}

static Comp mpc_open_pair_after_former(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Former *former = value;
  f->former = *former;
  return bind_frame(heap,
                    check_engine_mpc_view(heap, check_engine_former_shape(f->former),
                                          str_lit("the argument of open is not a share")),
                    mpc_open_pair_after_mpc, f);
}

static Comp mpc_open_pair_after_got(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Inferred *got = value;
  f->inferred = *got;
  return bind_frame(heap,
                    check_engine_former_view(heap, f->ctx, false, check_engine_inferred_ty(f->inferred),
                                             str_lit("the argument of open is not a share")),
                    mpc_open_pair_after_former, f);
}

Comp check_engine_mpc_open_pair(Heap *heap, Pair pair, Ctx ctx, Quantity mode, const Term *head) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(term4));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->mode = mode;
  f->term = pair.first;
  f->term2 = pair.second;
  return bind_frame(heap, check_engine_infer_uses(heap, ctx, mode, head), mpc_open_pair_after_got, f);
}

/* Frame: term = point, term2 = head. */
static Comp mpc_elim_out_after_pair(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Pair *pair = value;
  return check_engine_mpc_open_pair(heap, *pair, f->ctx, f->mode, f->term2);
}

static Comp mpc_elim_out_after_ok(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_mpc_address(heap, f->term), mpc_elim_out_after_pair, f);
}

Comp check_engine_mpc_elim_out(Heap *heap, Ctx ctx, Quantity mode, Addr addr, const Term *head) {
  Frame *f;
  switch (addr.kind) {
  case ADDR_PT:
    f = frame_new_upto(heap, FRAME_UPTO(term2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->mode = mode;
    f->term = addr.arg;
    f->term2 = head;
    return bind_frame(heap,
                      check_engine_require(quantity_equal(addr.quantity, QUANTITY_ZERO),
                                           error_text(ERROR_QUANTITY, "the authorization proof of open is erased")),
                      mpc_elim_out_after_ok, f);
  case ADDR_LEG:
  case ADDR_CTOR:
    return budget_failed(
        error_text(ERROR_WRONG_LEG, "open takes the party set address with the authorization proof"));
  }
  return comp_oom();
}

Comp check_engine_form_dispatch(Heap *heap, Shape shape, bool left, Ctx ctx, const Term *diagram,
                                MaybeLevelVar expected) {
  Vec *indices;
  switch (shape.kind) {
  case SHAPE_PI:
    return check_engine_point_form(heap, ctx, left, shape.quantity, shape.name, shape.domain, diagram);
  case SHAPE_ARG:
    return budget_failed(error_text(ERROR_MISMATCH, "an application frame shape is not a type former"));
  case SHAPE_COLL:
    return check_engine_coll_form(heap, ctx, shape.count, diagram, expected);
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    if (left) {
      indices = heap_alloc(heap, sizeof *indices);
      if (indices == NULL) return comp_oom();
      *indices = shape.args;
      return check_engine_mu_form(heap, ctx, shape.name, indices, diagram);
    } else {
      return budget_failed(error_text(ERROR_NOT_YET, "a right former at a mu shape arrives at M2"));
    }
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    if (left) {
      return check_engine_zk_form(heap, ctx, shape.quantity, shape.name, shape.domain, diagram);
    } else {
      return budget_failed(error_text(ERROR_NOT_YET, "Ran SZk arrives at V5"));
    }
  case SHAPE_FHC:
    if (left) {
      return budget_failed(error_text(ERROR_NOT_YET, "Lan SFhc arrives at V5"));
    } else {
      return check_engine_fhc_form(heap, ctx, shape.level, diagram);
    }
  case SHAPE_MPC:
    if (left) {
      return budget_failed(error_text(ERROR_NOT_YET, "Session arrives at V5"));
    } else {
      return check_engine_mpc_form(heap, ctx, shape.parties, shape.access, diagram);
    }
  }
  return comp_oom();
}

Comp check_engine_form(Heap *heap, Ctx ctx, bool left, Shape shape, const Term *diagram, MaybeLevelVar expected) {
  return check_engine_form_dispatch(heap, shape, left, ctx, diagram, expected);
}

Comp check_engine_intro_sec(Heap *heap, Ctx ctx, Quantity mode, Shape shape, const Vec *legs, const Value *expected) {
  switch (shape.kind) {
  case SHAPE_PI:
    return check_engine_point_intro_sec(heap, ctx, mode, legs, expected);
  case SHAPE_ARG:
    return budget_failed(error_text(ERROR_MISMATCH, "an application frame shape has no introduction"));
  case SHAPE_COLL:
    return check_engine_coll_intro_sec(heap, ctx, mode, shape.count, legs, expected);
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    return budget_failed(error_text(ERROR_NOT_YET, "a right former at a mu shape arrives at M2"));
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    return budget_failed(error_text(ERROR_NOT_YET, "Ran SZk arrives at V5"));
  case SHAPE_FHC:
    return check_engine_fhc_intro_sec(heap, ctx, mode, legs, expected);
  case SHAPE_MPC:
    return check_engine_mpc_intro_sec(heap, ctx, mode, legs, expected);
  }
  return comp_oom();
}

Comp check_engine_intro_in(Heap *heap, Ctx ctx, Quantity mode, Shape shape, Addr addr, const Vec *args,
                           const Value *expected) {
  switch (shape.kind) {
  case SHAPE_PI:
    return check_engine_point_intro_in(heap, ctx, mode, addr, args, expected);
  case SHAPE_ARG:
    return budget_failed(error_text(ERROR_MISMATCH, "an application frame shape has no introduction"));
  case SHAPE_COLL:
    return check_engine_coll_intro_in(heap, ctx, mode, addr, args, expected);
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    return check_engine_mu_intro_in(heap, ctx, mode, addr, args, expected);
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    return check_engine_zk_intro_in(heap, ctx, mode, addr, args, expected);
  case SHAPE_FHC:
    return budget_failed(error_text(ERROR_NOT_YET, "Lan SFhc arrives at V5"));
  case SHAPE_MPC:
    return budget_failed(error_text(ERROR_NOT_YET, "Session arrives at V5"));
  }
  return comp_oom();
}

Comp check_engine_elim_out(Heap *heap, Ctx ctx, Quantity mode, Shape shape, Addr addr, const Term *head) {
  switch (shape.kind) {
  case SHAPE_PI:
  case SHAPE_ARG:
    return check_engine_point_elim_out(heap, ctx, mode, addr, head);
  case SHAPE_COLL:
    return check_engine_coll_elim_out(heap, ctx, mode, addr, head);
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    return budget_failed(error_text(ERROR_NOT_YET, "a right former at a mu shape arrives at M2"));
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    return budget_failed(error_text(ERROR_NOT_YET, "Ran SZk arrives at V5"));
  case SHAPE_FHC:
    return check_engine_fhc_elim_out(heap, ctx, mode, addr, head);
  case SHAPE_MPC:
    return check_engine_mpc_elim_out(heap, ctx, mode, addr, head);
  }
  return comp_oom();
}

Comp check_engine_elim_elim(Heap *heap, Ctx ctx, Quantity mode, const ElimData *data, ExpectedValue expected) {
  switch (data->shape.kind) {
  case SHAPE_PI:
    return check_engine_point_elim_elim(heap, ctx, mode, data, expected);
  case SHAPE_ARG:
    return budget_failed(error_text(ERROR_MISMATCH, "an application frame shape has no eliminator"));
  case SHAPE_COLL:
    return check_engine_coll_elim_elim(heap, ctx, mode, data, expected);
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    return check_engine_mu_elim_elim(heap, ctx, mode, data);
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    return check_engine_point_elim_elim(heap, ctx, mode, data, expected);
  case SHAPE_FHC:
    return budget_failed(error_text(ERROR_NOT_YET, "Lan SFhc arrives at V5"));
  case SHAPE_MPC:
    return budget_failed(error_text(ERROR_NOT_YET, "Session arrives at V5"));
  }
  return comp_oom();
}

/* ---- conv (Bend 1645-2115): the conversion checker ---- */

static Comp pure_bool(bool value) { return budget_pure(kernel_bool_box(value)); }

static Comp pure_step(Heap *heap, ConvStep step) {
  ConvStep *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = step;
  return budget_pure(boxed);
}

static Comp pure_maybe_motive(Heap *heap, MaybeMotive motive) {
  MaybeMotive *boxed = heap_alloc(heap, sizeof *boxed);
  if (boxed == NULL) return comp_oom();
  *boxed = motive;
  return budget_pure(boxed);
}

/* The head of a list view; NULL on an empty list. */
static void *list_head(Vec xs) {
  VecGet got = vec_get(&xs, 0);
  switch (got.status) {
  case VEC_GET_OK:
    return got.value;
  case VEC_GET_OUT_OF_RANGE:
    return NULL;
  }
  return NULL;
}

static ExpectedValue expected_of_ptr(MaybePtr maybe) {
  if (maybe.some) return expected_some(maybe.value);
  return expected_none();
}

const Value *check_engine_conv_placeholder(Heap *heap) { return value_vuniv(heap, level_zero()); }

Comp check_engine_conv_and(bool flag, Comp next) {
  if (flag) return next;
  return pure_bool(false);
}

Comp check_engine_conv_supported(Shape s) {
  switch (s.kind) {
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_PI:
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_MU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return pure_unit();
  }
  return pure_unit();
}

Ctx check_engine_conv_grow(Heap *heap, Nat n, Ctx ctx) {
  if (nat_is_zero(n)) return ctx;
  return check_engine_conv_grow(heap, pred(heap, n),
                                context_bind(heap, str_lit("_"), QUANTITY_MANY, check_engine_conv_placeholder(heap), ctx));
}

/* Value.var(size + p) <> conv_fresh(p, size), pushed in list order. */
static Vec conv_fresh_into(Heap *heap, Nat n, Nat size, Vec out) {
  Nat p;
  const Value *var;
  if (nat_is_zero(n)) return out;
  p = pred(heap, n);
  var = value_var(heap, heap_nat_add(heap, size, p));
  if (var == NULL || !vec_push(&out, (void *)var)) {
    heap->oom = true;
    return out;
  }
  return conv_fresh_into(heap, p, size, out);
}

Vec check_engine_conv_fresh(Heap *heap, Nat n, Nat size) { return conv_fresh_into(heap, n, size, vec_new(heap->arena)); }

/* Frame: ctx, vec = xs, vec2 = ys. */
static Comp conv_list_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_list(heap, f->ctx, list_tail(f->vec), list_tail(f->vec2)));
}

Comp check_engine_conv_list(Heap *heap, Ctx ctx, Vec xs, Vec ys) {
  Frame *f;
  if (xs.len == 0 && ys.len == 0) return pure_bool(true);
  if (xs.len == 0 || ys.len == 0) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(vec2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = xs;
  f->vec2 = ys;
  return bind_frame(heap, check_engine_conv_type(heap, ctx, list_head(xs), list_head(ys)), conv_list_after_same, f);
}

/* Frame: ctx, value = a1, value2 = a2 (the SMpc access values). */
static Comp conv_shape_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value), check_engine_conv_type(heap, f->ctx, f->value, f->value2));
}

Comp check_engine_conv_shape(Heap *heap, Ctx ctx, Shape a, Shape b) {
  Frame *f;
  if (a.kind != b.kind) return pure_bool(false);
  switch (a.kind) {
  case SHAPE_PI:
    return check_engine_conv_and(quantity_equal(a.quantity, b.quantity),
                                 check_engine_conv_type(heap, ctx, a.domain, b.domain));
  case SHAPE_ARG:
    return pure_bool(false);
  case SHAPE_COLL:
    return pure_bool(big_equal(a.count, b.count));
  case SHAPE_PAR:
    return budget_failed(error_text(ERROR_NOT_YET, "SPar arrives at M1"));
  case SHAPE_MU:
    return check_engine_conv_and(str_eq(a.name, b.name), check_engine_conv_list(heap, ctx, a.args, b.args));
  case SHAPE_NU:
    return budget_failed(error_text(ERROR_NOT_YET, "SNu arrives at M2"));
  case SHAPE_ZK:
    return check_engine_conv_and(quantity_equal(a.quantity, b.quantity),
                                 check_engine_conv_type(heap, ctx, a.domain, b.domain));
  case SHAPE_FHC:
    return check_engine_conv_type(heap, ctx, a.level, b.level);
  case SHAPE_MPC:
    f = frame_new_upto(heap, FRAME_UPTO(value2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->value = a.access;
    f->value2 = b.access;
    return bind_frame(heap, check_engine_conv_type(heap, ctx, a.parties, b.parties), conv_shape_after_same, f);
  }
  return pure_bool(false);
}

Comp check_engine_conv_at_type(Heap *heap, Ctx ctx, ExpectedValue ty, const Value *a, const Value *b) {
  if (ty.some) return check_engine_conv(heap, ctx, ty.value, a, b);
  return check_engine_conv_type(heap, ctx, a, b);
}

Comp check_engine_conv_addr(Heap *heap, Ctx ctx, ExpectedValue ty, VAddr a, VAddr b) {
  if (a.kind != b.kind) return pure_bool(false);
  switch (a.kind) {
  case VADDR_PT:
    return check_engine_conv_at_type(heap, ctx, ty, a.arg, b.arg);
  case VADDR_LEG:
    return pure_bool(big_equal(a.index, b.index));
  case VADDR_CTOR:
    return pure_bool(str_eq(a.name, b.name));
  }
  return pure_bool(false);
}

/* Frame: ctx, nat = arity, vec = fresh, clo = y, value = v1. */
static Comp conv_closure_same_after_v2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_type(heap, check_engine_conv_grow(heap, f->nat, f->ctx), f->value, value);
}

static Comp conv_closure_same_after_v1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap,
                    budget_lift(eval_eval(heap, context_globals_of(f->ctx), vec_concat(heap->arena, &f->vec, &f->clo.env),
                                          f->clo.body)),
                    conv_closure_same_after_v2, f);
}

Comp check_engine_conv_closure_same(Heap *heap, bool same, Ctx ctx, Nat arity, Closure x, Closure y) {
  Frame *f;
  Vec fresh;
  if (!same) return pure_bool(false);
  fresh = check_engine_conv_fresh(heap, arity, context_size_of(ctx));
  f = frame_new_upto(heap, FRAME_UPTO(clo));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->nat = arity;
  f->vec = fresh;
  f->clo = y;
  return bind_frame(heap,
                    budget_lift(eval_eval(heap, context_globals_of(ctx), vec_concat(heap->arena, &fresh, &x.env), x.body)),
                    conv_closure_same_after_v1, f);
}

Comp check_engine_conv_closures(Heap *heap, Ctx ctx, Nat arity, Closure x, Nat other, Closure y) {
  return check_engine_conv_closure_same(heap, nat_cmp(arity, other) == 0, ctx, arity, x, y);
}

/* Frame: ctx, vec = xs, vec2 = ys. */
static Comp conv_legs_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_legs(heap, f->ctx, list_tail(f->vec), list_tail(f->vec2)));
}

Comp check_engine_conv_legs(Heap *heap, Ctx ctx, Vec xs, Vec ys) {
  Frame *f;
  const VLeg *x;
  const VLeg *y;
  if (xs.len == 0 && ys.len == 0) return pure_bool(true);
  if (xs.len == 0 || ys.len == 0) return pure_bool(false);
  x = list_head(xs);
  y = list_head(ys);
  f = frame_new_upto(heap, FRAME_UPTO(vec2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = xs;
  f->vec2 = ys;
  return bind_frame(heap,
                    check_engine_conv_closures(heap, ctx, nat_len(heap, &x->binders), x->clo, nat_len(heap, &y->binders),
                                               y->clo),
                    conv_legs_after_same, f);
}

/* Frame: ctx, vec2 = env2, term = t2, value = v1. */
static Comp conv_branch_addr_after_v2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_type(heap, f->ctx, f->value, value);
}

static Comp conv_branch_addr_after_v1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(f->ctx), f->vec2, f->term)),
                    conv_branch_addr_after_v2, f);
}

Comp check_engine_conv_branch_addr(Heap *heap, Ctx ctx, Vec env1, Vec env2, Addr a, Addr b) {
  Frame *f;
  if (a.kind != b.kind) return pure_bool(false);
  switch (a.kind) {
  case ADDR_PT:
    f = frame_new_upto(heap, FRAME_UPTO(vec2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->vec2 = env2;
    f->term = b.arg;
    return bind_frame(heap, budget_lift(eval_eval(heap, context_globals_of(ctx), env1, a.arg)),
                      conv_branch_addr_after_v1, f);
  case ADDR_LEG:
    return pure_bool(big_equal(a.index, b.index));
  case ADDR_CTOR:
    return pure_bool(str_eq(a.name, b.name));
  }
  return pure_bool(false);
}

/* Frame: ctx, vec = env1, vec2 = env2, vec3 = bs1, vec4 = bs2, leg = leg1, leg2 = leg2. */
static Comp conv_branches_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_branches(heap, f->ctx, f->vec, f->vec2, list_tail(f->vec3),
                                                          list_tail(f->vec4)));
}

static Comp conv_branches_after_addr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_conv_and(kernel_bool_unbox(value),
                                          check_engine_conv_closures(heap, f->ctx, nat_len(heap, &f->leg.binders),
                                                                     value_closure(f->vec, f->leg.body),
                                                                     nat_len(heap, &f->leg2.binders),
                                                                     value_closure(f->vec2, f->leg2.body))),
                    conv_branches_after_same, f);
}

Comp check_engine_conv_branches(Heap *heap, Ctx ctx, Vec env1, Vec env2, Vec bs1, Vec bs2) {
  Frame *f;
  const Branch *b1;
  const Branch *b2;
  if (bs1.len == 0 && bs2.len == 0) return pure_bool(true);
  if (bs1.len == 0 || bs2.len == 0) return pure_bool(false);
  b1 = list_head(bs1);
  b2 = list_head(bs2);
  f = frame_new_upto(heap, FRAME_UPTO(leg2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = env1;
  f->vec2 = env2;
  f->vec3 = bs1;
  f->vec4 = bs2;
  f->leg = b1->leg;
  f->leg2 = b2->leg;
  return bind_frame(heap, check_engine_conv_branch_addr(heap, ctx, env1, env2, b1->addr, b2->addr),
                    conv_branches_after_addr, f);
}

Comp check_engine_conv_motives(Heap *heap, Ctx ctx, Vec env1, Vec env2, MaybeMotive m1, MaybeMotive m2) {
  if (!m1.some && !m2.some) return pure_bool(true);
  if (m1.some && m2.some)
    return check_engine_conv_closures(heap, ctx, heap_nat_of_u64(heap, (uint64_t)m1.value.indices.len + 1),
                                      value_closure(env1, m1.value.body),
                                      heap_nat_of_u64(heap, (uint64_t)m2.value.indices.len + 1),
                                      value_closure(env2, m2.value.body));
  return pure_bool(false);
}

/* Frame: ctx, stuck = m1, stuck2 = m2, flag = same. */
static Comp conv_stuck_after_mot(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(f->flag && quantity_equal(f->stuck.scrut_q, f->stuck2.scrut_q) && kernel_bool_unbox(value),
                               check_engine_conv_branches(heap, f->ctx, f->stuck.env, f->stuck2.env, f->stuck.branches,
                                                          f->stuck2.branches));
}

static Comp conv_stuck_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->flag = kernel_bool_unbox(value);
  return bind_frame(heap,
                    check_engine_conv_motives(heap, f->ctx, f->stuck.env, f->stuck2.env, f->stuck.motive, f->stuck2.motive),
                    conv_stuck_after_mot, f);
}

Comp check_engine_conv_stuck(Heap *heap, Ctx ctx, StuckElim m1, StuckElim m2) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(stuck2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->stuck = m1;
  f->stuck2 = m2;
  return bind_frame(heap, check_engine_conv_shape(heap, ctx, m1.shape, m2.shape), conv_stuck_after_same, f);
}

ConvStep check_engine_conv_step_default(Heap *heap) {
  ConvStep step;
  step.argument = expected_none();
  step.next = check_engine_conv_placeholder(heap);
  return step;
}

ExpectedValue check_engine_conv_step_arg(ConvStep s) { return s.argument; }

const Value *check_engine_conv_step_next(ConvStep s) { return s.next; }

static Comp conv_spine_leg_after_t(Heap *heap, void *ctx, void *value) {
  ConvStep step;
  (void)ctx;
  step.argument = expected_none();
  step.next = value;
  return pure_step(heap, step);
}

/* Maybe<VLeg> as (some, leg). */
Comp check_engine_conv_spine_leg(Heap *heap, Ctx ctx, bool some, const VLeg *leg) {
  Frame *f;
  if (!some) return pure_step(heap, check_engine_conv_step_default(heap));
  f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  return bind_frame(heap, check_engine_open(heap, ctx, leg->clo, vec_new(heap->arena)), conv_spine_leg_after_t, f);
}

const Value *check_engine_conv_head_dom(Shape ws, const Value *dom) {
  if (ws.kind == SHAPE_PI) return ws.domain;
  return dom;
}

/* Frame: ctx, value = the head domain (SPi), shape = ws (SArg), big = k (SColl). */
static Comp conv_spine_former_after_cod_pi(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ConvStep step;
  step.argument = expected_some(f->value);
  step.next = value;
  return pure_step(heap, step);
}

static Comp conv_spine_former_after_cod_arg(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ConvStep step;
  step.argument = expected_of_ptr(shape_point_dom(f->shape));
  step.next = value;
  return pure_step(heap, step);
}

static Comp conv_spine_former_after_legs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *ls = value;
  VecGet got = big_at(heap, ls, f->big);
  switch (got.status) {
  case VEC_GET_OK:
    return check_engine_conv_spine_leg(heap, f->ctx, true, got.value);
  case VEC_GET_OUT_OF_RANGE:
    return check_engine_conv_spine_leg(heap, f->ctx, false, NULL);
  }
  return check_engine_conv_spine_leg(heap, f->ctx, false, NULL);
}

static Comp conv_spine_former_after_supported(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  (void)value;
  return pure_step(heap, check_engine_conv_step_default(heap));
}

Comp check_engine_conv_spine_former(Heap *heap, Ctx ctx, const Value *w, Shape s, VAddr addr) {
  Frame *f;
  if (w->kind != VALUE_RAN) return pure_step(heap, check_engine_conv_step_default(heap));
  f = frame_new_upto(heap, FRAME_UPTO(big));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  switch (s.kind) {
  case SHAPE_PI:
    if (addr.kind == VADDR_PT) {
      f->value = check_engine_conv_head_dom(w->shape, s.domain);
      return bind_frame(heap, check_engine_open(heap, ctx, w->diagram, singleton(heap, (void *)addr.arg)),
                        conv_spine_former_after_cod_pi, f);
    }
    break;
  case SHAPE_ARG:
    if (addr.kind == VADDR_PT) {
      f->shape = w->shape;
      return bind_frame(heap, check_engine_open(heap, ctx, w->diagram, singleton(heap, (void *)addr.arg)),
                        conv_spine_former_after_cod_arg, f);
    }
    break;
  case SHAPE_COLL:
    if (addr.kind == VADDR_LEG) {
      f->big = addr.index;
      return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, ctx, w->diagram)),
                        conv_spine_former_after_legs, f);
    }
    break;
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    break;
  }
  return bind_frame(heap, check_engine_conv_supported(s), conv_spine_former_after_supported, f);
}

/* Frame: ctx, shape = s, vaddr = a. */
static Comp conv_spine_step_after_w(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_spine_former(heap, f->ctx, value, f->shape, f->vaddr);
}

Comp check_engine_conv_spine_step(Heap *heap, Ctx ctx, const Value *hty, Shape s, VAddr a) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(vaddr));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->vaddr = a;
  return bind_frame(heap, check_engine_whnf(heap, ctx, hty), conv_spine_step_after_w, f);
}

static bool conv_point_shape(Shape s) { return s.kind == SHAPE_PI || s.kind == SHAPE_ARG; }

Comp check_engine_conv_frame_shape(Heap *heap, Ctx ctx, Shape a, Shape b) {
  if (conv_point_shape(a) && conv_point_shape(b)) return pure_bool(quantity_equal(a.quantity, b.quantity));
  return check_engine_conv_shape(heap, ctx, a, b);
}

/* Frame: ctx, value = hty, shape = s1, shape2 = s2, vaddr = a1, vaddr2 = a2, vec = xs, vec2 = ys, flag = same,
 * value2 = step.next. */
static Comp conv_spines_after_addr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(f->flag && kernel_bool_unbox(value),
                               check_engine_conv_spines(heap, f->ctx, f->value2, list_tail(f->vec), list_tail(f->vec2)));
}

static Comp conv_spines_after_step(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const ConvStep *step = value;
  f->value2 = check_engine_conv_step_next(*step);
  return bind_frame(heap,
                    check_engine_conv_addr(heap, f->ctx, check_engine_conv_step_arg(*step), f->vaddr, f->vaddr2),
                    conv_spines_after_addr, f);
}

static Comp conv_spines_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->flag = kernel_bool_unbox(value);
  return bind_frame(heap, check_engine_conv_spine_step(heap, f->ctx, f->value, f->shape, f->vaddr),
                    conv_spines_after_step, f);
}

static Comp conv_spines_after_stuck(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_spines(heap, f->ctx, check_engine_conv_placeholder(heap),
                                                        list_tail(f->vec), list_tail(f->vec2)));
}

Comp check_engine_conv_spines(Heap *heap, Ctx ctx, const Value *hty, Vec xs, Vec ys) {
  Frame *f;
  const Spine *s1;
  const Spine *s2;
  if (xs.len == 0 && ys.len == 0) return pure_bool(true);
  if (xs.len == 0 || ys.len == 0) return pure_bool(false);
  s1 = list_head(xs);
  s2 = list_head(ys);
  if (s1->kind != s2->kind) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(shape2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = xs;
  f->vec2 = ys;
  switch (s1->kind) {
  case SPINE_OUT:
    f->value = hty;
    f->shape = s1->shape;
    f->shape2 = s2->shape;
    f->vaddr = s1->addr;
    f->vaddr2 = s2->addr;
    return bind_frame(heap, check_engine_conv_frame_shape(heap, ctx, s1->shape, s2->shape), conv_spines_after_same, f);
  case SPINE_ELIM:
    return bind_frame(heap, check_engine_conv_stuck(heap, ctx, s1->data, s2->data), conv_spines_after_stuck, f);
  }
  return pure_bool(false);
}

const LevelVar *check_engine_conv_level_default(MaybeLevelVar u) {
  if (u.some) return u.value;
  return level_zero();
}

Comp check_engine_conv_annotation_width(Heap *heap, bool zero, MaybeLevelVar u1, MaybeLevelVar u2) {
  if (zero)
    return level_eq_decide_m(heap, check_engine_conv_level_default(u1), check_engine_conv_level_default(u2), true);
  return pure_bool(true);
}

Comp check_engine_conv_annotations(Heap *heap, Shape s, MaybeLevelVar u1, MaybeLevelVar u2) {
  if (s.kind == SHAPE_COLL) return check_engine_conv_annotation_width(heap, big_equal(s.count, big_zero()), u1, u2);
  return pure_bool(true);
}

/* Frame: ctx, shape = s, clo = d2, value = v (the fresh variable), flag = bind the binder, value2 = a. */
static Comp conv_diagram_after_b_bound(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_type(heap, context_bind(heap, f->shape.name, f->shape.quantity, f->shape.domain, f->ctx),
                                f->value2, value);
}

static Comp conv_diagram_after_b_plain(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_type(heap, f->ctx, f->value2, value);
}

static Comp conv_diagram_after_a(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value2 = value;
  if (f->flag)
    return bind_frame(heap, check_engine_open(heap, f->ctx, f->clo, singleton(heap, (void *)f->value)),
                      conv_diagram_after_b_bound, f);
  return bind_frame(heap, check_engine_open(heap, f->ctx, f->clo, vec_new(heap->arena)), conv_diagram_after_b_plain, f);
}

static bool conv_diagram_binds(Shape s) {
  switch (s.kind) {
  case SHAPE_PI:
  case SHAPE_ZK:
    return true;
  case SHAPE_ARG:
  case SHAPE_COLL:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return false;
  }
  return false;
}

Comp check_engine_conv_diagram(Heap *heap, Ctx ctx, Shape s, Closure d1, Closure d2) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(clo));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->clo = d2;
  f->flag = conv_diagram_binds(s);
  if (f->flag) {
    f->value = value_var(heap, context_size_of(ctx));
    return bind_frame(heap, check_engine_open(heap, ctx, d1, singleton(heap, (void *)f->value)), conv_diagram_after_a, f);
  }
  return bind_frame(heap, check_engine_open(heap, ctx, d1, vec_new(heap->arena)), conv_diagram_after_a, f);
}

/* Frame: ctx, shape = s, clo = d1, clo2 = d2. */
static Comp conv_former_annotations_after_levels(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_diagram(heap, f->ctx, f->shape, f->clo, f->clo2));
}

Comp check_engine_conv_former_annotations(Heap *heap, bool same, Ctx ctx, Shape s, Closure d1, MaybeLevelVar u1,
                                          Closure d2, MaybeLevelVar u2) {
  Frame *f;
  if (!same) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(clo2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->clo = d1;
  f->clo2 = d2;
  return bind_frame(heap, check_engine_conv_annotations(heap, s, u1, u2), conv_former_annotations_after_levels, f);
}

/* Frame: ctx, shape = s1, shape2 = s2, clo = d1, clo2 = d2, expected_level = u1, expected_level2 = u2. */
static Comp conv_formers_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_former_annotations(heap, kernel_bool_unbox(value), f->ctx, f->shape, f->clo,
                                              f->expected_level, f->clo2, f->expected_level2);
}

static Comp conv_formers_after_supported(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return bind_frame(heap, check_engine_conv_shape(heap, f->ctx, f->shape, f->shape2), conv_formers_after_same, f);
}

Comp check_engine_conv_formers(Heap *heap, Ctx ctx, Shape s1, Closure d1, MaybeLevelVar u1, Shape s2, Closure d2,
                               MaybeLevelVar u2) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(expected_level2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s1;
  f->shape2 = s2;
  f->clo = d1;
  f->clo2 = d2;
  f->expected_level = u1;
  f->expected_level2 = u2;
  return bind_frame(heap, check_engine_conv_supported(s1), conv_formers_after_supported, f);
}

/* Frame: ctx, vec = sp1, vec2 = sp2. */
static Comp conv_neutrals_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_spines(heap, f->ctx, value, vec_reverse(heap->arena, &f->vec),
                                  vec_reverse(heap->arena, &f->vec2));
}

Comp check_engine_conv_neutrals(Heap *heap, bool same, Ctx ctx, Head h, Vec sp1, Vec sp2) {
  Frame *f;
  if (!same) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(vec2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->vec = sp1;
  f->vec2 = sp2;
  return bind_frame(heap, check_engine_head_ty(heap, ctx, h), conv_neutrals_after_ty, f);
}

/* Frame: ctx, vaddr = a1, vaddr2 = a2, vec = args1 or legs1, vec2 = args2 or legs2, flag = same. */
static Comp conv_structural_whnf_after_addr(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(f->flag && kernel_bool_unbox(value),
                               check_engine_conv_list(heap, f->ctx, f->vec, f->vec2));
}

static Comp conv_structural_whnf_after_same_in(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->flag = kernel_bool_unbox(value);
  return bind_frame(heap, check_engine_conv_addr(heap, f->ctx, expected_none(), f->vaddr, f->vaddr2),
                    conv_structural_whnf_after_addr, f);
}

static Comp conv_structural_whnf_after_same_sec(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_and(kernel_bool_unbox(value), check_engine_conv_legs(heap, f->ctx, f->vec, f->vec2));
}

Comp check_engine_conv_structural_whnf(Heap *heap, Ctx ctx, const Value *a, const Value *b) {
  Frame *f;
  if (a->kind != b->kind) return pure_bool(false);
  switch (a->kind) {
  case VALUE_UNIV:
    return level_eq_decide_m(heap, a->level, b->level, true);
  case VALUE_LIT:
    return pure_bool(literal_equal(a->lit, b->lit));
  case VALUE_RAN:
  case VALUE_LAN:
    return check_engine_conv_formers(heap, ctx, a->shape, a->diagram, a->former_level, b->shape, b->diagram,
                                     b->former_level);
  case VALUE_IN:
    f = frame_new_upto(heap, FRAME_UPTO(vaddr2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->vaddr = a->addr;
    f->vaddr2 = b->addr;
    f->vec = a->args;
    f->vec2 = b->args;
    return bind_frame(heap, check_engine_conv_shape(heap, ctx, a->shape, b->shape), conv_structural_whnf_after_same_in,
                      f);
  case VALUE_SEC:
    f = frame_new_upto(heap, FRAME_UPTO(vaddr2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->vec = a->legs;
    f->vec2 = b->legs;
    return bind_frame(heap, check_engine_conv_shape(heap, ctx, a->shape, b->shape), conv_structural_whnf_after_same_sec,
                      f);
  case VALUE_NEUTRAL:
    return check_engine_conv_neutrals(heap, value_head_equal(a->head, b->head), ctx, a->head, a->spine, b->spine);
  }
  return pure_bool(false);
}

const Value *check_engine_unfolded_or(ExpectedValue step, const Value *fallback) {
  if (step.some) return step.value;
  return fallback;
}

/* Frame: ctx, value = unfolded a, value2 = unfolded b. */
static Comp conv_delta_unfold_after_tick(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_conv_type(heap, f->ctx, f->value, f->value2);
}

Comp check_engine_conv_delta_unfold(Heap *heap, Ctx ctx, ExpectedValue ua, ExpectedValue ub, const Value *a,
                                    const Value *b) {
  Frame *f;
  if (!ua.some && !ub.some) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = check_engine_unfolded_or(ua, a);
  f->value2 = check_engine_unfolded_or(ub, b);
  return bind_frame(heap, budget_tick(heap), conv_delta_unfold_after_tick, f);
}

/* Frame: ctx, value = a, value2 = b, expected = ua. */
static Comp conv_delta_retry_after_ub(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const ExpectedValue *ub = value;
  return check_engine_conv_delta_unfold(heap, f->ctx, f->expected, *ub, f->value, f->value2);
}

static Comp conv_delta_retry_after_ua(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const ExpectedValue *ua = value;
  f->expected = *ua;
  return bind_frame(heap, check_engine_unfold_step(heap, f->ctx, f->value2), conv_delta_retry_after_ub, f);
}

Comp check_engine_conv_delta_retry(Heap *heap, bool same, Ctx ctx, const Value *a, const Value *b) {
  Frame *f;
  if (same) return pure_bool(true);
  f = frame_new_upto(heap, FRAME_UPTO(expected));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = a;
  f->value2 = b;
  return bind_frame(heap, check_engine_unfold_step(heap, ctx, a), conv_delta_retry_after_ua, f);
}

/* Frame: ctx, value = wa, value2 = b then wb. */
static Comp conv_structural_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_delta_retry(heap, kernel_bool_unbox(value), f->ctx, f->value, f->value2);
}

static Comp conv_structural_after_wb(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value2 = value;
  return bind_frame(heap, check_engine_conv_structural_whnf(heap, f->ctx, f->value, f->value2),
                    conv_structural_after_same, f);
}

static Comp conv_structural_after_wa(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_whnf_core(heap, f->ctx, f->value2), conv_structural_after_wb, f);
}

Comp check_engine_conv_structural(Heap *heap, Ctx ctx, const Value *a, const Value *b) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value2 = b;
  return bind_frame(heap, check_engine_whnf_core(heap, ctx, a), conv_structural_after_wa, f);
}

/* Frame: ctx. */
static Comp conv_prop_probe_after_l(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  return pure_bool(level_always_zero(value));
}

static Comp conv_prop_probe_after_t(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_infer_univ(heap, f->ctx, value), conv_prop_probe_after_l, f);
}

Comp check_engine_conv_prop_probe(Heap *heap, Ctx ctx, const Value *ty) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  return bind_frame(heap, check_engine_quote(heap, ctx, ty), conv_prop_probe_after_t, f);
}

Comp check_engine_conv_prop_failure(Error error) {
  if (error.kind == ERROR_BUDGET_EXHAUSTED) return budget_failed(error_make(ERROR_BUDGET_EXHAUSTED, error.message));
  return pure_bool(false);
}

static Comp conv_prop_failure_handler(Heap *heap, void *ctx, Error error) {
  (void)heap;
  (void)ctx;
  return check_engine_conv_prop_failure(error);
}

Comp check_engine_conv_is_prop(Heap *heap, Ctx ctx, const Value *ty) {
  HandlerK handler;
  if (ty->kind == VALUE_UNIV) return pure_bool(false);
  handler.fn = conv_prop_failure_handler;
  handler.ctx = NULL;
  return budget_recover(heap, check_engine_conv_prop_probe(heap, ctx, ty), handler);
}

/* Telescope = a Vec of TeleBinder pointers. */
bool check_engine_conv_fields_zero(Vec xs) {
  const TeleBinder *binder;
  if (xs.len == 0) return true;
  binder = list_head(xs);
  return quantity_equal(binder->quantity, QUANTITY_ZERO) && check_engine_conv_fields_zero(list_tail(xs));
}

bool check_engine_conv_ctor_zero(MaybeCtor c) {
  if (!c.some) return false;
  return check_engine_conv_fields_zero(c.value.args) && !c.value.self_rec;
}

bool check_engine_conv_family_zero(Family f) {
  const Str *name;
  switch (f.status.kind) {
  case CTOR_STATUS_COMPLETE:
    if (f.status.names.len == 0) return level_always_zero(f.level);
    if (f.status.names.len == 1) {
      name = list_head(f.status.names);
      return level_always_zero(f.level) && check_engine_conv_ctor_zero(positivity_ctor_of(*name, f));
    }
    return false;
  case CTOR_STATUS_PROVISIONAL:
  case CTOR_STATUS_BUILTIN:
    return false;
  }
  return false;
}

bool check_engine_conv_family_probe(FamilyResult r) {
  switch (r.kind) {
  case RESULT_FAIL:
    return false;
  case RESULT_DONE:
    return check_engine_conv_family_zero(r.value);
  }
  return false;
}

static Comp conv_subsingleton_after_supported(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_bool(false);
}

Comp check_engine_conv_subsingleton(Heap *heap, Ctx ctx, const Value *w) {
  Frame *f;
  if (w->kind != VALUE_LAN) return pure_bool(false);
  if (w->shape.kind == SHAPE_MU)
    return pure_bool(check_engine_conv_family_probe(rules_mu_family(heap, ctx, w->shape.name)));
  f = frame_new_upto(heap, FRAME_UPTO(ctx));
  if (f == NULL) return comp_oom();
  return bind_frame(heap, check_engine_conv_supported(w->shape), conv_subsingleton_after_supported, f);
}

Comp check_engine_conv_out(Heap *heap, Ctx ctx, Shape s, VAddr a, const Value *v) {
  return budget_lift(eval_out_value(heap, context_globals_of(ctx), s, a, v));
}

/* [Pair{aleg 0, Leg{[(q, "x"), (many, "y")], Var{which == 0 ? 1 : 0}}}]; empty with heap->oom set on failure. */
Vec check_engine_conv_proj_branches(Heap *heap, Quantity q, Nat which) {
  Vec binders = vec_new(heap->arena);
  Vec out = vec_new(heap->arena);
  Binder *x = term_binder_new(heap, q, str_lit("x"));
  Binder *y = term_binder_new(heap, QUANTITY_MANY, str_lit("y"));
  const Term *body = term_var(heap, heap_nat_of_u64(heap, nat_is_zero(which) ? 1 : 0));
  Branch *branch;
  if (x == NULL || y == NULL || body == NULL) return out;
  if (!vec_push(&binders, x) || !vec_push(&binders, y)) {
    heap->oom = true;
    return out;
  }
  branch = term_branch_new(heap, term_aleg(heap, nat_zero()), term_leg(binders, body));
  if (branch == NULL) return out;
  if (!vec_push(&out, branch)) heap->oom = true;
  return out;
}

Comp check_engine_conv_project(Heap *heap, Ctx ctx, Shape s, Quantity q, Quantity scrut_q, MaybeMotive mo, Nat which,
                               const Value *v) {
  return budget_lift(eval_elim_value(heap, context_globals_of(ctx),
                                     value_stuck_elim(s, scrut_q, mo, check_engine_conv_proj_branches(heap, q, which),
                                                      context_env_of(ctx)),
                                     v));
}

/* Some{Term.Motive{None, [], "self", body}} */
static MaybeMotive conv_self_motive(Heap *heap, const Term *body) {
  MaybeStr ind;
  ind.some = false;
  ind.value = str_lit("");
  return term_motive_some(term_motive(ind, vec_new(heap->arena), str_lit("self"), body));
}

/* Frame: ctx, shape = s, clo = d, q, value = dom. */
static Comp conv_proj_motive_body_after_point(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_open(heap, f->ctx, f->clo, singleton(heap, value));
}

static Comp conv_proj_motive_body_after_domain(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, conv_self_motive(heap, value),
                                              nat_zero(), value_var(heap, context_size_of(f->ctx))),
                    conv_proj_motive_body_after_point, f);
}

Comp check_engine_conv_proj_motive_body(Heap *heap, Nat which, Ctx ctx, Shape s, Closure d, Quantity q,
                                        const Value *dom) {
  Frame *f;
  if (nat_is_zero(which)) return pure_value(dom);
  f = frame_new_upto(heap, FRAME_UPTO(q));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->clo = d;
  f->q = q;
  f->value = dom;
  return bind_frame(heap, check_engine_quote(heap, context_bind(heap, str_lit("self"), q, dom, ctx), dom),
                    conv_proj_motive_body_after_domain, f);
}

/* Frame: ctx, name = n, q, value = dom. */
static Comp conv_proj_motive_after_body(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_maybe_motive(heap, conv_self_motive(heap, value));
}

static Comp conv_proj_motive_after_v(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_quote(heap, context_bind(heap, f->name, f->q, f->value, f->ctx), value),
                    conv_proj_motive_after_body, f);
}

Comp check_engine_conv_proj_motive(Heap *heap, Ctx ctx, Shape s, Closure d, Quantity q, Str n, const Value *dom,
                                   Nat which) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(q));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->name = n;
  f->q = q;
  f->value = dom;
  return bind_frame(heap, check_engine_conv_proj_motive_body(heap, which, ctx, s, d, q, dom), conv_proj_motive_after_v,
                    f);
}

/* Frame: ctx, shape = s, clo = d, q, name = n, value = dom, value2 = a, value3 = b, value4 = cod, motive = mo,
 * value5 = a2. */
static Comp conv_eta_pair_second_after_b2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv(heap, f->ctx, f->value4, f->value5, value);
}

static Comp conv_eta_pair_second_after_a2(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value5 = value;
  return bind_frame(heap,
                    check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, f->motive,
                                              heap_nat_of_u64(heap, 1), f->value3),
                    conv_eta_pair_second_after_b2, f);
}

static Comp conv_eta_pair_second_after_mo(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const MaybeMotive *mo = value;
  f->motive = *mo;
  return bind_frame(heap,
                    check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, f->motive,
                                              heap_nat_of_u64(heap, 1), f->value2),
                    conv_eta_pair_second_after_a2, f);
}

static Comp conv_eta_pair_second_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(heap,
                    check_engine_conv_proj_motive(heap, f->ctx, f->shape, f->clo, f->q, f->name, f->value,
                                                  heap_nat_of_u64(heap, 1)),
                    conv_eta_pair_second_after_mo, f);
}

Comp check_engine_conv_eta_pair_second(Heap *heap, bool first, Ctx ctx, Shape s, Closure d, Quantity q, Str n,
                                       const Value *dom, const Value *a1, const Value *a, const Value *b) {
  Frame *f;
  if (!first) return pure_bool(false);
  f = frame_new_upto(heap, FRAME_UPTO(value5));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->clo = d;
  f->q = q;
  f->name = n;
  f->value = dom;
  f->value2 = a;
  f->value3 = b;
  return bind_frame(heap, check_engine_open(heap, ctx, d, singleton(heap, (void *)a1)), conv_eta_pair_second_after_cod,
                    f);
}

/* Frame: ctx, shape = s, clo = d, q, name = n, value = dom, value2 = a, value3 = b, motive = mo, value4 = a1. */
static Comp conv_eta_pair_after_first(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_eta_pair_second(heap, kernel_bool_unbox(value), f->ctx, f->shape, f->clo, f->q, f->name,
                                           f->value, f->value4, f->value2, f->value3);
}

static Comp conv_eta_pair_after_b1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_conv(heap, f->ctx, f->value, f->value4, value), conv_eta_pair_after_first, f);
}

static Comp conv_eta_pair_after_a1(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(heap,
                    check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, f->motive, nat_zero(),
                                              f->value3),
                    conv_eta_pair_after_b1, f);
}

static Comp conv_eta_pair_after_mo(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const MaybeMotive *mo = value;
  f->motive = *mo;
  return bind_frame(heap,
                    check_engine_conv_project(heap, f->ctx, f->shape, f->q, QUANTITY_ONE, f->motive, nat_zero(),
                                              f->value2),
                    conv_eta_pair_after_a1, f);
}

Comp check_engine_conv_eta_pair(Heap *heap, Ctx ctx, Shape s, Closure d, Quantity q, Str n, const Value *dom,
                                const Value *a, const Value *b) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(motive));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->shape = s;
  f->clo = d;
  f->q = q;
  f->name = n;
  f->value = dom;
  f->value2 = a;
  f->value3 = b;
  return bind_frame(heap, check_engine_conv_proj_motive(heap, ctx, s, d, q, n, dom, nat_zero()), conv_eta_pair_after_mo,
                    f);
}

/* Frame: ctx, big = n, big2 = index, shape = s, vec = legs, value = a, value2 = b, value3 = ty, value4 = x. */
static Comp conv_eta_coll_after_same(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  Big next = big_sub(heap, f->big, big_one());
  return check_engine_conv_and(kernel_bool_unbox(value),
                               check_engine_conv_eta_coll(heap, big_sign(next), next, big_add(heap, f->big2, big_one()),
                                                          f->ctx, f->shape, f->vec, f->value, f->value2));
}

static Comp conv_eta_coll_after_y(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap, check_engine_conv(heap, f->ctx, f->value3, f->value4, value), conv_eta_coll_after_same, f);
}

static Comp conv_eta_coll_after_x(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(heap, check_engine_conv_out(heap, f->ctx, f->shape, value_vaddr_leg(f->big2), f->value2),
                    conv_eta_coll_after_y, f);
}

static Comp conv_eta_coll_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value3 = value;
  return bind_frame(heap, check_engine_conv_out(heap, f->ctx, f->shape, value_vaddr_leg(f->big2), f->value),
                    conv_eta_coll_after_x, f);
}

Comp check_engine_conv_eta_coll(Heap *heap, Cmp sign, Big n, Big index, Ctx ctx, Shape s, Vec legs, const Value *a,
                                const Value *b) {
  Frame *f;
  switch (sign) {
  case CMP_EQ:
    return pure_bool(true);
  case CMP_LT:
    return budget_failed(error_text(ERROR_MISMATCH, "a collection width must be nonnegative"));
  case CMP_GT:
    f = frame_new_upto(heap, FRAME_UPTO(big2));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->big = n;
    f->big2 = index;
    f->shape = s;
    f->vec = legs;
    f->value = a;
    f->value2 = b;
    return bind_frame(heap, budget_lift(rules_coll_leg_ty(heap, ctx, &f->vec, index)), conv_eta_coll_after_ty, f);
  }
  return pure_bool(true);
}

/* Frame: ctx, shape = the SPi shape, value = v, value2 = a, value3 = b, value4 = cod, value5 = fa. */
static Comp conv_eta_or_structural_after_fb(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv(heap, context_bind(heap, f->shape.name, f->shape.quantity, f->shape.domain, f->ctx),
                           f->value4, f->value5, value);
}

static Comp conv_eta_or_structural_after_fa(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value5 = value;
  return bind_frame(heap,
                    check_engine_conv_out(heap, f->ctx, f->shape, value_vaddr_pt(f->shape.quantity, f->value),
                                          f->value3),
                    conv_eta_or_structural_after_fb, f);
}

static Comp conv_eta_or_structural_after_cod(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value4 = value;
  return bind_frame(heap,
                    check_engine_conv_out(heap, f->ctx, f->shape, value_vaddr_pt(f->shape.quantity, f->value),
                                          f->value2),
                    conv_eta_or_structural_after_fa, f);
}

/* Frame: ctx, shape = SColl{n}, value = a, value2 = b. */
static Comp conv_eta_or_structural_after_legs(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *legs = value;
  return check_engine_conv_eta_coll(heap, big_sign(f->shape.count), f->shape.count, big_zero(), f->ctx, f->shape, *legs,
                                    f->value, f->value2);
}

/* Frame: ctx, value = a, value2 = b. */
static Comp conv_eta_or_structural_after_supported(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  (void)value;
  return check_engine_conv_structural(heap, f->ctx, f->value, f->value2);
}

static Comp conv_eta_or_structural_supported(Heap *heap, Ctx ctx, Shape s, const Value *a, const Value *b) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = a;
  f->value2 = b;
  return bind_frame(heap, check_engine_conv_supported(s), conv_eta_or_structural_after_supported, f);
}

static Comp conv_eta_or_structural_ran(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  Frame *f;
  switch (w->shape.kind) {
  case SHAPE_PI:
    f = frame_new_upto(heap, FRAME_UPTO(value5));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->shape = w->shape;
    f->value = value_var(heap, context_size_of(ctx));
    f->value2 = a;
    f->value3 = b;
    return bind_frame(heap, check_engine_open(heap, ctx, w->diagram, singleton(heap, (void *)f->value)),
                      conv_eta_or_structural_after_cod, f);
  case SHAPE_COLL:
    f = frame_new_upto(heap, FRAME_UPTO(value5));
    if (f == NULL) return comp_oom();
    f->ctx = ctx;
    f->shape = w->shape;
    f->value = a;
    f->value2 = b;
    return bind_frame(heap, lift_vec(heap, rules_coll_legs_of(heap, ctx, w->diagram)),
                      conv_eta_or_structural_after_legs, f);
  case SHAPE_ARG:
  case SHAPE_PAR:
  case SHAPE_MU:
  case SHAPE_NU:
  case SHAPE_ZK:
  case SHAPE_FHC:
  case SHAPE_MPC:
    return conv_eta_or_structural_supported(heap, ctx, w->shape, a, b);
  }
  return conv_eta_or_structural_supported(heap, ctx, w->shape, a, b);
}

static Comp conv_eta_or_structural_lan(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  if (w->shape.kind == SHAPE_PI)
    return check_engine_conv_eta_pair(heap, ctx, w->shape, w->diagram, w->shape.quantity, w->shape.name,
                                      w->shape.domain, a, b);
  return conv_eta_or_structural_supported(heap, ctx, w->shape, a, b);
}

Comp check_engine_conv_eta_or_structural(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  switch (w->kind) {
  case VALUE_RAN:
    return conv_eta_or_structural_ran(heap, ctx, w, a, b);
  case VALUE_LAN:
    return conv_eta_or_structural_lan(heap, ctx, w, a, b);
  case VALUE_UNIV:
  case VALUE_IN:
  case VALUE_SEC:
  case VALUE_LIT:
  case VALUE_NEUTRAL:
    return check_engine_conv_structural(heap, ctx, a, b);
  }
  return check_engine_conv_structural(heap, ctx, a, b);
}

Comp check_engine_conv_after_sub(Heap *heap, bool sub, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  if (sub) return pure_bool(true);
  return check_engine_conv_eta_or_structural(heap, ctx, w, a, b);
}

/* Frame: ctx, value = a, value2 = b, value3 = w. */
static Comp conv_after_prop_after_sub(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_after_sub(heap, kernel_bool_unbox(value), f->ctx, f->value3, f->value, f->value2);
}

static Comp conv_after_prop_after_w(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value3 = value;
  return bind_frame(heap, check_engine_conv_subsingleton(heap, f->ctx, f->value3), conv_after_prop_after_sub, f);
}

Comp check_engine_conv_after_prop(Heap *heap, bool prop, Ctx ctx, const Value *ty, const Value *a, const Value *b) {
  Frame *f;
  if (prop) return pure_bool(true);
  f = frame_new_upto(heap, FRAME_UPTO(value3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = a;
  f->value2 = b;
  return bind_frame(heap, check_engine_whnf(heap, ctx, ty), conv_after_prop_after_w, f);
}

/* Frame: ctx, value = wa, value2 = b. */
static Comp conv_quick_after_wb(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_structural_whnf(heap, f->ctx, f->value, value);
}

static Comp conv_quick_after_wa(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->value = value;
  return bind_frame(heap, check_engine_whnf_core(heap, f->ctx, f->value2), conv_quick_after_wb, f);
}

Comp check_engine_conv_quick(Heap *heap, Ctx ctx, const Value *a, const Value *b) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value2 = b;
  return bind_frame(heap, check_engine_whnf_core(heap, ctx, a), conv_quick_after_wa, f);
}

/* Frame: ctx, value = w, value2 = a, value3 = b. */
static Comp conv_after_quick_after_prop(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_after_prop(heap, kernel_bool_unbox(value), f->ctx, f->value, f->value2, f->value3);
}

Comp check_engine_conv_after_quick(Heap *heap, bool quick, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  Frame *f;
  if (quick) return pure_bool(true);
  f = frame_new_upto(heap, FRAME_UPTO(value3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = w;
  f->value2 = a;
  f->value3 = b;
  return bind_frame(heap, check_engine_conv_is_prop(heap, ctx, w), conv_after_quick_after_prop, f);
}

/* Frame: ctx, value = w, value2 = a, value3 = b. */
static Comp conv_whnf_ty_after_quick(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_after_quick(heap, kernel_bool_unbox(value), f->ctx, f->value, f->value2, f->value3);
}

Comp check_engine_conv_whnf_ty(Heap *heap, Ctx ctx, const Value *w, const Value *a, const Value *b) {
  Frame *f;
  if (w->kind == VALUE_UNIV) return check_engine_conv_structural(heap, ctx, a, b);
  f = frame_new_upto(heap, FRAME_UPTO(value3));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = w;
  f->value2 = a;
  f->value3 = b;
  return bind_frame(heap, check_engine_conv_quick(heap, ctx, a, b), conv_whnf_ty_after_quick, f);
}

/* Frame: ctx, value = a, value2 = b. */
static Comp conv_after_w(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return check_engine_conv_whnf_ty(heap, f->ctx, value, f->value, f->value2);
}

Comp check_engine_conv(Heap *heap, Ctx ctx, const Value *ty, const Value *a, const Value *b) {
  Frame *f = frame_new_upto(heap, FRAME_UPTO(value2));
  if (f == NULL) return comp_oom();
  f->ctx = ctx;
  f->value = a;
  f->value2 = b;
  return bind_frame(heap, check_engine_whnf(heap, ctx, ty), conv_after_w, f);
}

Comp check_engine_conv_type(Heap *heap, Ctx ctx, const Value *a, const Value *b) {
  return check_engine_conv_structural(heap, ctx, a, b);
}
