/* Port of surface/poly.bend: universe-polymorphic schemes. Defunctionalized level and name
 * callbacks (PolyAction) drive one budgeted traversal over kernel terms; a catalog of schemes
 * (PolyCatalog) is declared against the globals and instantiated by level substitution.
 * Comp values: const Term *, Shape *, Addr *, Leg *, Branch *, MaybeMotive *, Vec *, Decl *,
 * MaybeTermPtr *, Str *, MaybeStr *, PolyRow * (a catalog), PolyInstance * and kernel_unit(). */
#include <string.h>

#include "surface.h"

/* ---- action constructors ---- */

static Vec no_vec(void) {
  Vec v;
  memset(&v, 0, sizeof v);
  return v;
}

PolyLevelAction poly_preserve_levels(void) {
  PolyLevelAction a;
  a.kind = POLY_PRESERVE_LEVELS;
  a.arity = nat_zero();
  a.levels = no_vec();
  return a;
}

PolyLevelAction poly_scope_levels(Nat arity) {
  PolyLevelAction a;
  a.kind = POLY_SCOPE_LEVELS;
  a.arity = arity;
  a.levels = no_vec();
  return a;
}

PolyLevelAction poly_substitute_levels(Vec levels) {
  PolyLevelAction a;
  a.kind = POLY_SUBSTITUTE_LEVELS;
  a.arity = nat_zero();
  a.levels = levels;
  return a;
}

PolyAction poly_action(PolyLevelAction levels, Vec names, Vec forbidden) {
  PolyAction a;
  a.levels = levels;
  a.names = names;
  a.forbidden = forbidden;
  return a;
}

/* ---- host helpers: boxing, frames, continuations ---- */

typedef Comp (*ItemMapper)(Heap *heap, PolyAction action, void *item);

typedef struct {
  PolyAction action;
  const Term *term;     /* the input term */
  Shape shape;          /* the input shape, then the mapped shape */
  Addr addr;            /* the mapped addr */
  const Term *first;    /* mapped intermediate 1 */
  const Term *second;   /* mapped intermediate 2 */
  MaybeMotive motive;   /* the mapped motive */
  Motive source_motive; /* the input motive */
  Leg leg;              /* the input leg */
  Branch branch;        /* the input branch */
  Decl decl;            /* the input decl */
  MaybeStr ind;
  Str name;
  const Vec *items; /* list mapping: input, cursor, output, item mapper */
  size_t index;
  Vec acc;
  ItemMapper mapper;
  const Term *(*maker)(Heap *heap, Shape shape, const Term *body); /* term_lan or term_ran */
  Globals globals; /* catalog operations */
  PolyCatalog catalog;
  Nat arity;
  Vec levels;
  Str as_name;
  PolyScheme scheme;
} Frame;

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static void *box(Heap *heap, const void *source, size_t size) {
  void *copy = heap_alloc(heap, size);
  if (copy != NULL) memcpy(copy, source, size);
  return copy;
}

static Frame *frame_new(Heap *heap, PolyAction action) {
  Frame *frame = heap_alloc(heap, sizeof(Frame));
  if (frame == NULL) return NULL;
  memset(frame, 0, sizeof(Frame));
  frame->action = action;
  return frame;
}

static Comp bind_frame(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), Frame *frame) {
  NextK next;
  if (frame == NULL) return comp_oom();
  next.fn = fn;
  next.ctx = frame;
  return budget_bind(heap, first, next);
}

static Comp pure_box(Heap *heap, const void *source, size_t size) {
  void *copy = box(heap, source, size);
  if (copy == NULL) return comp_oom();
  return budget_pure(copy);
}

static Comp pure_term(const Term *term) { return budget_pure((void *)term); }
static Comp pure_shape(Heap *heap, Shape shape) { return pure_box(heap, &shape, sizeof shape); }
static Comp pure_addr(Heap *heap, Addr addr) { return pure_box(heap, &addr, sizeof addr); }
static Comp pure_vec(Heap *heap, Vec vec) { return pure_box(heap, &vec, sizeof vec); }
static Comp pure_maybe_motive(Heap *heap, MaybeMotive motive) { return pure_box(heap, &motive, sizeof motive); }
static Comp pure_maybe_term(Heap *heap, MaybeTermPtr body) { return pure_box(heap, &body, sizeof body); }
static Comp pure_decl(Heap *heap, Decl decl) { return pure_box(heap, &decl, sizeof decl); }

static Result done_str(Heap *heap, Str value) {
  Str *copy = box(heap, &value, sizeof value);
  if (copy == NULL) return budget_result_fail(error_host_oom());
  return budget_result_done(copy);
}

static Result done_maybe_str(Heap *heap, MaybeStr value) {
  MaybeStr *copy = box(heap, &value, sizeof value);
  if (copy == NULL) return budget_result_fail(error_host_oom());
  return budget_result_done(copy);
}

static Result fail_text(ErrorKind kind, Str text) { return budget_result_fail(error_make(kind, text)); }

/* A host allocation failure anywhere in a run surfaces as Error.HostOom (parser export precedent). */
static Result export_result(Heap *heap, Result result) {
  if (heap->oom) return budget_result_fail(error_host_oom());
  return result;
}

/* ---- substitution .. map_name ---- */

/* substitution(value) */
static Result substitution(MaybeLevelVar value) {
  if (value.some) return budget_result_done((void *)value.value);
  return fail_text(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope"));
}

/* level_action(action, level): the Done value is the const LevelVar *. */
static Result level_action(Heap *heap, PolyLevelAction action, const LevelVar *level) {
  switch (action.kind) {
    case POLY_PRESERVE_LEVELS:
      return budget_result_done((void *)level);
    case POLY_SCOPE_LEVELS:
      if (level_in_scope(action.arity, level)) return budget_result_done((void *)level);
      return fail_text(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope"));
    case POLY_SUBSTITUTE_LEVELS:
      return substitution(level_subst(heap, &action.levels, level));
  }
  return fail_text(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope"));
}

/* map_level(action, level) */
static Result map_level(Heap *heap, PolyAction action, const LevelVar *level) {
  return level_action(heap, action.levels, level);
}

/* member(names, name): names is a Vec of Str pointers. */
static bool member(const Vec *names, Str name) {
  size_t i = 0;
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OK && str_eq(*(const Str *)got.value, name)) return true;
  }
  return false;
}

/* find_name(names, name): names is a Vec of PolyRename pointers; the first source match wins. */
static Str find_name(const Vec *names, Str name) {
  size_t i = 0;
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OK && str_eq(((const PolyRename *)got.value)->source, name))
      return ((const PolyRename *)got.value)->target;
  }
  return name;
}

/* map_name(action, name): the Done value is a boxed Str. */
static Result map_name(Heap *heap, PolyAction action, Str name) {
  if (member(&action.forbidden, name))
    return fail_text(ERROR_NOT_YET, str_lit("references between family schemas are not supported"));
  return done_str(heap, find_name(&action.names, name));
}

/* ---- list mapping (map_terms, map_legs, map_branches share one stepper) ---- */

static Comp map_list_step(Heap *heap, Frame *frame);

static Comp map_list_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  if (!vec_push(&frame->acc, value)) return comp_oom();
  frame->index = frame->index + 1;
  return map_list_step(heap, frame);
}

static Comp map_list_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_vec(heap, frame->acc);
  return bind_frame(heap, frame->mapper(heap, frame->action, got.value), map_list_k, frame);
}

/* The Done value is a boxed Vec of the mapped items, in source order. */
static Comp map_list(Heap *heap, PolyAction action, const Vec *items, ItemMapper mapper) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->items = items;
  frame->index = 0;
  frame->acc = vec_new(heap->arena);
  frame->mapper = mapper;
  return map_list_step(heap, frame);
}

static Comp term_item(Heap *heap, PolyAction action, void *item) {
  return poly_map_term(heap, action, (const Term *)item);
}

/* map_terms(terms, action) */
static Comp map_terms(Heap *heap, const Vec *terms, PolyAction action) {
  return map_list(heap, action, terms, term_item);
}

/* ---- shape_body, map_shape ---- */

static Comp shape_body_domain_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  if (frame->shape.kind == SHAPE_ZK) return pure_shape(heap, shape_szk(frame->shape.quantity, frame->shape.name, value));
  return pure_shape(heap, shape_spi(frame->shape.quantity, frame->shape.name, value));
}

static Comp shape_body_par_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_shape(heap, shape_spar((void *)frame->first, value));
}

static Comp shape_body_par_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->shape.right), shape_body_par_k2, frame);
}

static Comp shape_body_family_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Vec args = *(const Vec *)value;
  if (frame->shape.kind == SHAPE_NU) return pure_shape(heap, shape_snu(frame->name, args));
  return pure_shape(heap, shape_smu(frame->name, args));
}

static Comp shape_body_family_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->name = *(const Str *)value;
  return bind_frame(heap, map_terms(heap, &frame->shape.args, frame->action), shape_body_family_k2, frame);
}

static Comp shape_body_fhc_k(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_shape(heap, shape_sfhc(value));
}

static Comp shape_body_mpc_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_shape(heap, shape_smpc((void *)frame->first, value));
}

static Comp shape_body_mpc_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->shape.access), shape_body_mpc_k2, frame);
}

/* shape_body(shape, action): the Done value is a boxed Shape. */
static Comp shape_body(Heap *heap, Shape shape, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->shape = shape;
  switch (shape.kind) {
    case SHAPE_PI:
      return bind_frame(heap, poly_map_term(heap, action, shape.domain), shape_body_domain_k, frame);
    case SHAPE_ZK:
      return bind_frame(heap, poly_map_term(heap, action, shape.domain), shape_body_domain_k, frame);
    case SHAPE_ARG:
      return pure_shape(heap, shape);
    case SHAPE_COLL:
      return pure_shape(heap, shape);
    case SHAPE_PAR:
      return bind_frame(heap, poly_map_term(heap, action, shape.left), shape_body_par_k1, frame);
    case SHAPE_MU:
      return bind_frame(heap, budget_lift(map_name(heap, action, shape.name)), shape_body_family_k1, frame);
    case SHAPE_NU:
      return bind_frame(heap, budget_lift(map_name(heap, action, shape.name)), shape_body_family_k1, frame);
    case SHAPE_FHC:
      return bind_frame(heap, poly_map_term(heap, action, shape.level), shape_body_fhc_k, frame);
    case SHAPE_MPC:
      return bind_frame(heap, poly_map_term(heap, action, shape.parties), shape_body_mpc_k1, frame);
  }
  return pure_shape(heap, shape);
}

static Comp map_shape_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return shape_body(heap, frame->shape, frame->action);
}

/* map_shape(shape, action): one tick, then shape_body. */
static Comp map_shape(Heap *heap, Shape shape, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->shape = shape;
  return bind_frame(heap, budget_tick(heap), map_shape_k, frame);
}

/* ---- addr_body, map_addr ---- */

static Comp addr_body_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_addr(heap, term_addr_pt(frame->addr.quantity, value));
}

/* addr_body(addr, action): the Done value is a boxed Addr. */
static Comp addr_body(Heap *heap, Addr addr, PolyAction action) {
  Frame *frame;
  switch (addr.kind) {
    case ADDR_PT:
      frame = frame_new(heap, action);
      if (frame == NULL) return comp_oom();
      frame->addr = addr;
      return bind_frame(heap, poly_map_term(heap, action, addr.arg), addr_body_k, frame);
    case ADDR_LEG:
      return pure_addr(heap, addr);
    case ADDR_CTOR:
      return pure_addr(heap, addr);
  }
  return pure_addr(heap, addr);
}

static Comp map_addr_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return addr_body(heap, frame->addr, frame->action);
}

/* map_addr(addr, action): one tick, then addr_body. */
static Comp map_addr(Heap *heap, Addr addr, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->addr = addr;
  return bind_frame(heap, budget_tick(heap), map_addr_k, frame);
}

/* ---- map_leg, map_legs ---- */

static Comp map_leg_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Leg *leg = term_leg_new(heap, frame->leg.binders, value);
  if (leg == NULL) return comp_oom();
  return budget_pure(leg);
}

static Comp map_leg_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->leg.body), map_leg_k2, frame);
}

/* map_leg(leg, action): one tick, the body mapped; the Done value is a Leg pointer. */
static Comp map_leg(Heap *heap, Leg leg, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->leg = leg;
  return bind_frame(heap, budget_tick(heap), map_leg_k1, frame);
}

static Comp leg_item(Heap *heap, PolyAction action, void *item) {
  return map_leg(heap, *(const Leg *)item, action);
}

/* map_legs(legs, action) */
static Comp map_legs(Heap *heap, const Vec *legs, PolyAction action) {
  return map_list(heap, action, legs, leg_item);
}

/* ---- map_optional_name, map_motive ---- */

/* map_optional_name(name, action): the Done value is a boxed MaybeStr. */
static Result map_optional_name(Heap *heap, MaybeStr name, PolyAction action) {
  MaybeStr none;
  MaybeStr some;
  Result mapped;
  none.some = false;
  none.value = str_lit("");
  if (!name.some) return done_maybe_str(heap, none);
  mapped = map_name(heap, action, name.value);
  switch (mapped.kind) {
    case RESULT_FAIL:
      return mapped;
    case RESULT_DONE:
      some.some = true;
      some.value = *(const Str *)mapped.value;
      return done_maybe_str(heap, some);
  }
  return mapped;
}

static Comp map_motive_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  MaybeMotive motive;
  motive.some = true;
  motive.value.ind = frame->ind;
  motive.value.indices = frame->source_motive.indices;
  motive.value.self = frame->source_motive.self;
  motive.value.body = value;
  return pure_maybe_motive(heap, motive);
}

static Comp map_motive_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->ind = *(const MaybeStr *)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->source_motive.body), map_motive_k2, frame);
}

/* map_motive(motive, action): the Done value is a boxed MaybeMotive. */
static Comp map_motive(Heap *heap, MaybeMotive motive, PolyAction action) {
  Frame *frame;
  if (!motive.some) return pure_maybe_motive(heap, motive);
  frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->source_motive = motive.value;
  return bind_frame(heap, budget_lift(map_optional_name(heap, motive.value.ind, action)), map_motive_k1, frame);
}

/* ---- map_branches ---- */

static Comp map_branch_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Branch *branch = term_branch_new(heap, frame->addr, *(const Leg *)value);
  if (branch == NULL) return comp_oom();
  return budget_pure(branch);
}

static Comp map_branch_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->addr = *(const Addr *)value;
  return bind_frame(heap, map_leg(heap, frame->branch.leg, frame->action), map_branch_k2, frame);
}

/* One Common.Pair{addr, leg} of map_branches: addr mapped, then leg; the Done value is a Branch pointer. */
static Comp map_branch(Heap *heap, Branch branch, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->branch = branch;
  return bind_frame(heap, map_addr(heap, branch.addr, action), map_branch_k1, frame);
}

static Comp branch_item(Heap *heap, PolyAction action, void *item) {
  return map_branch(heap, *(const Branch *)item, action);
}

/* map_branches(branches, action) */
static Comp map_branches(Heap *heap, const Vec *branches, PolyAction action) {
  return map_list(heap, action, branches, branch_item);
}

/* ---- term_body, map_term ---- */

static Comp term_body_global_k(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(term_global(heap, *(const Str *)value));
}

static Comp term_body_univ_k(Heap *heap, void *ctx, void *value) {
  (void)ctx;
  return pure_term(term_univ(heap, (const LevelVar *)value));
}

/* Lan and Ran: shape, then body. */
static Comp term_body_kan_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(frame->maker(heap, frame->shape, value));
}

static Comp term_body_kan_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->shape = *(const Shape *)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->body), term_body_kan_k2, frame);
}

/* In: shape, addr, args. */
static Comp term_body_in_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(term_in(heap, frame->shape, frame->addr, *(const Vec *)value));
}

static Comp term_body_in_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->addr = *(const Addr *)value;
  return bind_frame(heap, map_terms(heap, &frame->term->args, frame->action), term_body_in_k3, frame);
}

static Comp term_body_in_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->shape = *(const Shape *)value;
  return bind_frame(heap, map_addr(heap, frame->term->addr, frame->action), term_body_in_k2, frame);
}

/* Sec: shape, legs. */
static Comp term_body_sec_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(term_sec(heap, frame->shape, *(const Vec *)value));
}

static Comp term_body_sec_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->shape = *(const Shape *)value;
  return bind_frame(heap, map_legs(heap, &frame->term->legs, frame->action), term_body_sec_k2, frame);
}

/* Out: shape, addr, head. */
static Comp term_body_out_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(term_out(heap, frame->shape, frame->addr, value));
}

static Comp term_body_out_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->addr = *(const Addr *)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->head), term_body_out_k3, frame);
}

static Comp term_body_out_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->shape = *(const Shape *)value;
  return bind_frame(heap, map_addr(heap, frame->term->addr, frame->action), term_body_out_k2, frame);
}

/* Elim: shape, scrut, motive, branches. */
static Comp term_body_elim_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  ElimData data = term_elim_data(frame->shape, frame->first, frame->term->data->scrut_q, frame->motive, *(const Vec *)value);
  return pure_term(term_elim(heap, data));
}

static Comp term_body_elim_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->motive = *(const MaybeMotive *)value;
  return bind_frame(heap, map_branches(heap, &frame->term->data->branches, frame->action), term_body_elim_k4, frame);
}

static Comp term_body_elim_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, map_motive(heap, frame->term->data->motive, frame->action), term_body_elim_k3, frame);
}

static Comp term_body_elim_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->shape = *(const Shape *)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->data->scrut), term_body_elim_k2, frame);
}

/* Let: ty, value, body. */
static Comp term_body_let_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(term_let(heap, frame->term->name, frame->first, frame->second, value));
}

static Comp term_body_let_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->second = value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->body), term_body_let_k3, frame);
}

static Comp term_body_let_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->value), term_body_let_k2, frame);
}

/* Ann: body, ty. */
static Comp term_body_ann_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_term(term_ann(heap, frame->first, value));
}

static Comp term_body_ann_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->term->ty), term_body_ann_k2, frame);
}

/* term_body(term, action): the Done value is the mapped const Term *. */
static Comp term_body(Heap *heap, const Term *term, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->term = term;
  switch (term->kind) {
    case TERM_VAR:
      return pure_term(term);
    case TERM_LIT:
      return pure_term(term);
    case TERM_AUTO:
      return pure_term(term);
    case TERM_GLOBAL:
      return bind_frame(heap, budget_lift(map_name(heap, action, term->name)), term_body_global_k, frame);
    case TERM_UNIV:
      return bind_frame(heap, budget_lift(map_level(heap, action, term->level)), term_body_univ_k, frame);
    case TERM_LAN:
      frame->maker = term_lan;
      return bind_frame(heap, map_shape(heap, term->shape, action), term_body_kan_k1, frame);
    case TERM_RAN:
      frame->maker = term_ran;
      return bind_frame(heap, map_shape(heap, term->shape, action), term_body_kan_k1, frame);
    case TERM_IN:
      return bind_frame(heap, map_shape(heap, term->shape, action), term_body_in_k1, frame);
    case TERM_SEC:
      return bind_frame(heap, map_shape(heap, term->shape, action), term_body_sec_k1, frame);
    case TERM_OUT:
      return bind_frame(heap, map_shape(heap, term->shape, action), term_body_out_k1, frame);
    case TERM_ELIM:
      return bind_frame(heap, map_shape(heap, term->data->shape, action), term_body_elim_k1, frame);
    case TERM_LET:
      return bind_frame(heap, poly_map_term(heap, action, term->ty), term_body_let_k1, frame);
    case TERM_ANN:
      return bind_frame(heap, poly_map_term(heap, action, term->body), term_body_ann_k1, frame);
  }
  return pure_term(term);
}

/* The tick before term_body carries only the action and the term: a full Frame here would be a
 * second 960 B record per term node (hop 101, prelude footprint). */
typedef struct {
  PolyAction action;
  const Term *term;
} TickFrame;

static Comp map_term_k(Heap *heap, void *ctx, void *value) {
  TickFrame *frame = ctx;
  (void)value;
  return term_body(heap, frame->term, frame->action);
}

/* map_term(action, term): one tick, then term_body. */
Comp poly_map_term(Heap *heap, PolyAction action, const Term *term) {
  TickFrame *frame = heap_alloc(heap, sizeof(TickFrame));
  NextK next;
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->term = term;
  next.fn = map_term_k;
  next.ctx = frame;
  return budget_bind(heap, budget_tick(heap), next);
}

/* map_levels(action, term) */
Result poly_map_levels(Heap *heap, PolyLevelAction action, const Term *term) {
  PolyAction full = poly_action(action, vec_new(heap->arena), vec_new(heap->arena));
  return export_result(heap, budget_run(heap, poly_map_term(heap, full, term), budget_unlimited()));
}

/* ---- map_optional_term ---- */

static Comp map_optional_term_k(Heap *heap, void *ctx, void *value) {
  MaybeTermPtr some;
  (void)ctx;
  some.some = true;
  some.value = value;
  return pure_maybe_term(heap, some);
}

/* map_optional_term(body, action): the Done value is a boxed MaybeTermPtr. */
static Comp map_optional_term(Heap *heap, MaybeTermPtr body, PolyAction action) {
  Frame *frame;
  if (!body.some) return pure_maybe_term(heap, body);
  frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  return bind_frame(heap, poly_map_term(heap, action, body.value), map_optional_term_k, frame);
}

/* ---- the catalog: empty .. arity ---- */

/* empty() */
PolyCatalog poly_empty(void) { return NULL; }

/* lookup(catalog, name) */
MaybePolyScheme poly_lookup(PolyCatalog catalog, Str name) {
  MaybePolyScheme found;
  const PolyRow *row = catalog;
  for (; row != NULL; row = row->rest) {
    if (str_eq(row->name, name)) {
      found.some = true;
      found.value = row->scheme;
      return found;
    }
  }
  found.some = false;
  found.value.arity = nat_zero();
  found.value.decl.name = str_lit("");
  found.value.decl.kind = DECL_DEFINITION;
  found.value.decl.ty = term_auto();
  found.value.decl.body.some = false;
  found.value.decl.body.value = term_auto();
  return found;
}

/* occupied(globals, catalog, name) */
bool poly_occupied(Globals globals, PolyCatalog catalog, Str name) {
  return global_find(name, globals).some || global_find_family(name, globals).some || poly_lookup(catalog, name).some;
}

/* collision(name) */
Error poly_collision(Heap *heap, Str name) {
  return error_make(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("the name "), name, str_lit(" is already declared")));
}

/* decl_name(decl) */
static Str decl_name(Decl decl) { return decl.name; }

static Comp map_decl_action_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Decl decl;
  decl.name = frame->decl.name;
  decl.kind = frame->decl.kind;
  decl.ty = frame->first;
  decl.body = *(const MaybeTermPtr *)value;
  return pure_decl(heap, decl);
}

static Comp map_decl_action_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, map_optional_term(heap, frame->decl.body, frame->action), map_decl_action_k2, frame);
}

/* map_decl_action(decl, action): the Done value is a boxed Decl. */
Comp poly_map_decl_action(Heap *heap, Decl decl, PolyAction action) {
  Frame *frame = frame_new(heap, action);
  if (frame == NULL) return comp_oom();
  frame->decl = decl;
  return bind_frame(heap, poly_map_term(heap, action, decl.ty), map_decl_action_k1, frame);
}

/* map_decl(levels, decl): the Done value is a boxed Decl. */
Result poly_map_decl(Heap *heap, PolyLevelAction levels, Decl decl) {
  PolyAction action = poly_action(levels, vec_new(heap->arena), vec_new(heap->arena));
  return export_result(heap, budget_run(heap, poly_map_decl_action(heap, decl, action), budget_unlimited()));
}

/* scheme_arity(scheme) */
static MaybeNat scheme_arity(MaybePolyScheme scheme) {
  MaybeNat arity;
  arity.some = scheme.some;
  arity.value = scheme.some ? scheme.value.arity : nat_zero();
  return arity;
}

/* arity(catalog, name) */
MaybeNat poly_arity(PolyCatalog catalog, Str name) { return scheme_arity(poly_lookup(catalog, name)); }

/* closed_levels(levels): levels is a Vec of const LevelVar pointers. */
static bool closed_levels(const Vec *levels) {
  size_t i = 0;
  for (; i < levels->len; i = i + 1) {
    VecGet got = vec_get(levels, i);
    if (got.status != VEC_GET_OK || !level_in_scope(nat_zero(), (const LevelVar *)got.value)) return false;
  }
  return true;
}

/* rename_decl(decl, name) */
static Decl rename_decl(Decl decl, Str name) {
  Decl renamed = decl;
  renamed.name = name;
  return renamed;
}

/* validate_instance(fresh, name, as_name, arity, levels): Done carries kernel_unit(). */
static Result validate_instance(Heap *heap, bool fresh, Str name, Str as_name, Nat arity, const Vec *levels) {
  Nat given = heap_nat_of_u64(heap, (uint64_t)levels->len);
  if (!fresh) return budget_result_fail(poly_collision(heap, as_name));
  if (nat_cmp(arity, given) != 0)
    return fail_text(ERROR_UNIVERSE, SURFACE_CAT(heap, str_lit("the schema "), name, str_lit(" expects "), heap_nat_show(heap, arity),
                                                 str_lit(" universe arguments, got "), heap_nat_show(heap, given)));
  if (closed_levels(levels)) return budget_result_done(kernel_unit());
  return fail_text(ERROR_UNIVERSE, str_lit("universe arguments must be closed"));
}

/* ---- declare ---- */

static Comp declare_comp_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  PolyRow *row = heap_alloc(heap, sizeof(PolyRow));
  (void)value;
  if (row == NULL) return comp_oom();
  row->name = decl_name(frame->decl);
  row->scheme.arity = frame->arity;
  row->scheme.decl = frame->decl;
  row->rest = frame->catalog;
  return budget_pure(row);
}

static Comp declare_comp_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, check_check_scheme_comp(heap, frame->globals, frame->arity, frame->decl), declare_comp_k2, frame);
}

/* declare_comp(globals, catalog, arity, decl): the Done value is the extended catalog (a PolyRow *). */
Comp poly_declare_comp(Heap *heap, Globals globals, PolyCatalog catalog, Nat arity, Decl decl) {
  Frame *frame = frame_new(heap, poly_action(poly_preserve_levels(), no_vec(), no_vec()));
  Result checked;
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->decl = decl;
  if (poly_occupied(globals, catalog, decl_name(decl))) checked = budget_result_fail(poly_collision(heap, decl_name(decl)));
  else checked = budget_result_done(kernel_unit());
  return bind_frame(heap, budget_lift(checked), declare_comp_k1, frame);
}

/* declare(globals, catalog, budget, arity, decl) */
Result poly_declare(Heap *heap, Globals globals, PolyCatalog catalog, Budget budget, Nat arity, Decl decl) {
  return export_result(heap, budget_run(heap, poly_declare_comp(heap, globals, catalog, arity, decl), budget));
}

/* ---- instantiate ---- */

static Comp instantiate_scheme_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  PolyInstance instance;
  instance.globals = global_add(heap, frame->as_name, *(const Entry *)value, frame->globals);
  instance.term = term_global(heap, frame->as_name);
  return pure_box(heap, &instance, sizeof instance);
}

static Comp instantiate_scheme_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->decl = rename_decl(*(const Decl *)value, frame->as_name);
  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, nat_zero(), frame->decl), instantiate_scheme_k3, frame);
}

static Comp instantiate_scheme_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_lift(poly_map_decl(heap, poly_substitute_levels(frame->levels), frame->scheme.decl)),
                    instantiate_scheme_k2, frame);
}

/* instantiate_scheme(scheme, globals, catalog, name, levels, as_name): the Done value is a PolyInstance *. */
static Comp instantiate_scheme(Heap *heap, MaybePolyScheme scheme, Globals globals, PolyCatalog catalog, Str name, Vec levels,
                               Str as_name) {
  Frame *frame;
  if (!scheme.some)
    return budget_failed(error_make(ERROR_UNBOUND, SURFACE_CAT(heap, str_lit("unknown universe schema "), name)));
  frame = frame_new(heap, poly_action(poly_preserve_levels(), no_vec(), no_vec()));
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->name = name;
  frame->levels = levels;
  frame->as_name = as_name;
  frame->scheme = scheme.value;
  return bind_frame(heap,
                    budget_lift(validate_instance(heap, !poly_occupied(globals, catalog, as_name), name, as_name,
                                                  scheme.value.arity, &frame->levels)),
                    instantiate_scheme_k1, frame);
}

/* instantiate_comp(globals, catalog, name, levels, as_name) */
Comp poly_instantiate_comp(Heap *heap, Globals globals, PolyCatalog catalog, Str name, Vec levels, Str as_name) {
  return instantiate_scheme(heap, poly_lookup(catalog, name), globals, catalog, name, levels, as_name);
}

/* instantiate(globals, catalog, budget, name, levels, as_name) */
Result poly_instantiate(Heap *heap, Globals globals, PolyCatalog catalog, Budget budget, Str name, Vec levels, Str as_name) {
  return export_result(heap, budget_run(heap, poly_instantiate_comp(heap, globals, catalog, name, levels, as_name), budget));
}

/* ---- exports for family_poly (the Bend module exports these definitions) ---- */

bool poly_member(const Vec *names, Str name) { return member(names, name); }

Result poly_map_name(Heap *heap, PolyAction action, Str name) { return map_name(heap, action, name); }

Result poly_map_level(Heap *heap, PolyAction action, const LevelVar *level) { return map_level(heap, action, level); }

Comp poly_map_terms(Heap *heap, const Vec *terms, PolyAction action) { return map_terms(heap, terms, action); }

Str poly_decl_name(Decl decl) { return decl_name(decl); }
