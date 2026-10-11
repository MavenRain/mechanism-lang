/* bend2/kernel/eval.bend: evaluation, weak head normalisation and read back of kernel terms.
 * Bend lists are Vecs of pointers in the Bend order (index 0 = the list head); every allocation
 * goes through the arena and a failed one sets heap->oom while the code carries on with the
 * placeholders the constructors return. */
#include "kernel.h"

static const char *CANNOT_ELIMINATE = "an elimination met a value it cannot eliminate";

static void push(Heap *heap, Vec *xs, const void *item) {
  if (!vec_push(xs, (void *)item)) heap->oom = true;
}

static void *at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  switch (got.status) {
    case VEC_GET_OK: return got.value;
    case VEC_GET_OUT_OF_RANGE: return NULL;
  }
  return NULL;
}

/* item <> rest */
static Vec cons(Heap *heap, const void *item, const Vec *rest) {
  Vec out = vec_new(heap->arena);
  push(heap, &out, item);
  for (size_t i = 0; i < rest->len; i++) push(heap, &out, at(rest, i));
  return out;
}

/* List.append(a, b) */
static Vec append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < a->len; i++) push(heap, &out, at(a, i));
  for (size_t i = 0; i < b->len; i++) push(heap, &out, at(b, i));
  return out;
}

/* List.reverse(xs) */
static Vec reverse(Heap *heap, const Vec *xs) {
  Vec out = vec_new(heap->arena);
  for (size_t i = xs->len; i > 0; i--) push(heap, &out, at(xs, i - 1));
  return out;
}

static Result done(const void *value) { return budget_result_done((void *)value); }

static Result fail(ErrorKind kind, const char *message) {
  return budget_result_fail(error_make(kind, str_lit(message)));
}

static ShapeResult shape_done(Shape value) {
  ShapeResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static ShapeResult shape_fail(Error error) {
  ShapeResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static VAddrResult vaddr_done(VAddr value) {
  VAddrResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static VAddrResult vaddr_fail(Error error) {
  VAddrResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static AddrResult addr_done(Addr value) {
  AddrResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static AddrResult addr_fail(Error error) {
  AddrResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static VecResult vec_done(Vec value) {
  VecResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static VecResult vec_fail(Error error) {
  VecResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static LegResult leg_done(Leg value) {
  LegResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static LegResult leg_fail(Error error) {
  LegResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static MaybeMotiveResult motive_done(MaybeMotive value) {
  MaybeMotiveResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static MaybeMotiveResult motive_fail(Error error) {
  MaybeMotiveResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static MaybeValueResult maybe_value_done(MaybeValue value) {
  MaybeValueResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static MaybeValueResult maybe_value_fail(Error error) {
  MaybeValueResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static MaybeVecResult maybe_vec_done(MaybeVec value) {
  MaybeVecResult r = {0};
  r.kind = RESULT_DONE;
  r.value = value;
  return r;
}

static MaybeVecResult maybe_vec_fail(Error error) {
  MaybeVecResult r = {0};
  r.kind = RESULT_FAIL;
  r.error = error;
  return r;
}

static MaybeVec vec_some(Vec value) {
  MaybeVec m = {0};
  m.some = true;
  m.value = value;
  return m;
}

static MaybeVec vec_none(void) {
  MaybeVec m = {0};
  return m;
}

static MaybeValue value_some(const Value *value) {
  MaybeValue m = {0};
  m.some = true;
  m.value = value;
  return m;
}

static MaybeValue value_none(void) {
  MaybeValue m = {0};
  return m;
}

static MaybeLevelVar no_level(void) {
  MaybeLevelVar m = {0};
  return m;
}

static const Value *value_of(Result r) { return (const Value *)r.value; }

static const Term *term_of(Result r) { return (const Term *)r.value; }

/* SOut{s, VAPt{q, value}}: the one frame shape the guards and literal spines look at. */
static bool point_frame(const Spine *frame) {
  return frame != NULL && frame->kind == SPINE_OUT && frame->addr.kind == VADDR_PT;
}

Result eval_local_value(Heap *heap, MaybeValue value, Nat index) {
  if (value.some) return done(value.value);
  Str head = heap_str_cat(heap, str_lit("de Bruijn index "), heap_nat_show(heap, index));
  Str message = heap_str_cat(heap, head, str_lit(" is outside the environment"));
  return budget_result_fail(error_make(ERROR_UNBOUND, message));
}

VecResult eval_eval_list(Heap *heap, const Vec *terms, Globals globals, Vec env) {
  Vec values = vec_new(heap->arena);
  for (size_t i = 0; i < terms->len; i++) {
    Result value = eval_eval(heap, globals, env, (const Term *)at(terms, i));
    if (value.kind == RESULT_FAIL) return vec_fail(value.error);
    push(heap, &values, value.value);
  }
  return vec_done(values);
}

ShapeResult eval_eval_shape(Heap *heap, Shape shape, Globals globals, Vec env) {
  switch (shape.kind) {
    case SHAPE_PI: {
      Result value = eval_eval(heap, globals, env, (const Term *)shape.domain);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_spi(shape.quantity, shape.name, value.value));
    }
    case SHAPE_ARG: return shape_done(shape_sarg(shape.quantity, shape.name));
    case SHAPE_COLL: return shape_done(shape_scoll(shape.count));
    case SHAPE_PAR: return shape_fail(error_make(ERROR_NOT_YET, str_lit("SPar arrives at M1")));
    case SHAPE_MU: {
      VecResult values = eval_eval_list(heap, &shape.args, globals, env);
      if (values.kind == RESULT_FAIL) return shape_fail(values.error);
      return shape_done(shape_smu(shape.name, values.value));
    }
    case SHAPE_NU: return shape_fail(error_make(ERROR_NOT_YET, str_lit("SNu arrives at M2")));
    case SHAPE_ZK: {
      Result value = eval_eval(heap, globals, env, (const Term *)shape.domain);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_szk(shape.quantity, shape.name, value.value));
    }
    case SHAPE_FHC: {
      Result value = eval_eval(heap, globals, env, (const Term *)shape.level);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_sfhc(value.value));
    }
    case SHAPE_MPC: {
      Result p = eval_eval(heap, globals, env, (const Term *)shape.parties);
      if (p.kind == RESULT_FAIL) return shape_fail(p.error);
      Result a = eval_eval(heap, globals, env, (const Term *)shape.access);
      if (a.kind == RESULT_FAIL) return shape_fail(a.error);
      return shape_done(shape_smpc(p.value, a.value));
    }
  }
  return shape_fail(error_host_oom());
}

VAddrResult eval_eval_addr(Heap *heap, Globals globals, Vec env, Addr addr) {
  switch (addr.kind) {
    case ADDR_PT: {
      Result value = eval_eval(heap, globals, env, addr.arg);
      if (value.kind == RESULT_FAIL) return vaddr_fail(value.error);
      return vaddr_done(value_vaddr_pt(addr.quantity, value_of(value)));
    }
    case ADDR_LEG: return vaddr_done(value_vaddr_leg(addr.index));
    case ADDR_CTOR: return vaddr_done(value_vaddr_ctor(addr.name));
  }
  return vaddr_fail(error_host_oom());
}

Vec eval_close_legs(Heap *heap, const Vec *legs, Vec env) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < legs->len; i++) {
    const Leg *leg = (const Leg *)at(legs, i);
    if (leg == NULL) continue;
    VLeg *closed = value_vleg_new(heap, leg->binders, value_close(env, leg->body));
    if (closed != NULL) push(heap, &out, closed);
  }
  return out;
}

Result eval_open_closure(Heap *heap, Globals globals, Closure closure, Vec args) {
  Vec reversed = reverse(heap, &args);
  return eval_eval(heap, globals, append(heap, &reversed, &closure.env), closure.body);
}

Result eval_eval_global_entry(Heap *heap, MaybeEntry entry, Str name) {
  if (!entry.some) return budget_result_fail(error_make(ERROR_UNBOUND, name));
  return done(value_vneutral(heap, value_hglobal(name), vec_new(heap->arena)));
}

MaybeLevelVar eval_annotation_level(Result result) {
  switch (result.kind) {
    case RESULT_DONE: return value_as_univ(value_of(result));
    case RESULT_FAIL: return no_level();
  }
  return no_level();
}

Result eval_eval_whnf(Heap *heap, Globals globals, Vec env, const Term *term) {
  Result value = eval_eval(heap, globals, env, term);
  if (value.kind == RESULT_FAIL) return value;
  return eval_whnf(heap, globals, value_of(value));
}

MaybeLevelVar eval_fill_level(Heap *heap, MaybeLevelVar level, Globals globals, Vec env, const Term *ty) {
  if (level.some) return level;
  return eval_annotation_level(eval_eval_whnf(heap, globals, env, ty));
}

const Value *eval_annotate(Heap *heap, const Value *value, Globals globals, Vec env, const Term *ty) {
  switch (value->kind) {
    case VALUE_LAN:
      return value_vlan(heap, value->shape, value->diagram,
                        eval_fill_level(heap, value->former_level, globals, env, ty));
    case VALUE_RAN:
      return value_vran(heap, value->shape, value->diagram,
                        eval_fill_level(heap, value->former_level, globals, env, ty));
    case VALUE_UNIV:
    case VALUE_IN:
    case VALUE_SEC:
    case VALUE_LIT:
    case VALUE_NEUTRAL:
      return value;
  }
  return value;
}

Result eval_eval(Heap *heap, Globals globals, Vec env, const Term *term) {
  switch (term->kind) {
    case TERM_VAR: {
      VecGet got = common_at(&env, term->index);
      MaybeValue local = value_none();
      switch (got.status) {
        case VEC_GET_OK: local = value_some((const Value *)got.value); break;
        case VEC_GET_OUT_OF_RANGE: break;
      }
      return eval_local_value(heap, local, term->index);
    }
    case TERM_UNIV: return done(value_vuniv(heap, term->level));
    case TERM_LIT: return done(value_vlit(heap, term->lit));
    case TERM_LAN: {
      ShapeResult shape = eval_eval_shape(heap, term->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      return done(value_vlan(heap, shape.value, value_close(env, term->body), no_level()));
    }
    case TERM_RAN: {
      ShapeResult shape = eval_eval_shape(heap, term->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      return done(value_vran(heap, shape.value, value_close(env, term->body), no_level()));
    }
    case TERM_IN: {
      ShapeResult shape = eval_eval_shape(heap, term->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      VAddrResult addr = eval_eval_addr(heap, globals, env, term->addr);
      if (addr.kind == RESULT_FAIL) return budget_result_fail(addr.error);
      VecResult values = eval_eval_list(heap, &term->args, globals, env);
      if (values.kind == RESULT_FAIL) return budget_result_fail(values.error);
      return done(value_vin(heap, shape.value, addr.value, values.value));
    }
    case TERM_SEC: {
      ShapeResult shape = eval_eval_shape(heap, term->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      return done(value_vsec(heap, shape.value, eval_close_legs(heap, &term->legs, env)));
    }
    case TERM_OUT: {
      ShapeResult shape = eval_eval_shape(heap, term->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      VAddrResult addr = eval_eval_addr(heap, globals, env, term->addr);
      if (addr.kind == RESULT_FAIL) return budget_result_fail(addr.error);
      Result value = eval_eval(heap, globals, env, term->head);
      if (value.kind == RESULT_FAIL) return value;
      Result applied = eval_out_value(heap, globals, shape.value, addr.value, value_of(value));
      if (applied.kind == RESULT_FAIL) return applied;
      return eval_whnf_core(heap, globals, value_of(applied));
    }
    case TERM_ELIM: {
      const ElimData *data = term->data;
      ShapeResult shape = eval_eval_shape(heap, data->shape, globals, env);
      if (shape.kind == RESULT_FAIL) return budget_result_fail(shape.error);
      Result value = eval_eval(heap, globals, env, data->scrut);
      if (value.kind == RESULT_FAIL) return value;
      StuckElim stuck = value_stuck_elim(shape.value, data->scrut_q, data->motive, data->branches, env);
      return eval_elim_value(heap, globals, stuck, value_of(value));
    }
    case TERM_LET: {
      Result v = eval_eval(heap, globals, env, term->value);
      if (v.kind == RESULT_FAIL) return v;
      return eval_eval(heap, globals, cons(heap, v.value, &env), term->body);
    }
    case TERM_ANN: {
      Result v = eval_eval(heap, globals, env, term->body);
      if (v.kind == RESULT_FAIL) return v;
      return done(eval_annotate(heap, value_of(v), globals, env, term->ty));
    }
    case TERM_GLOBAL: return eval_eval_global_entry(heap, global_find(term->name, globals), term->name);
    case TERM_AUTO: return fail(ERROR_NOT_YET, "instances arrive at M2");
  }
  return budget_result_fail(error_host_oom());
}

Result eval_stuck_out(Heap *heap, const Value *value, Shape shape, VAddr addr) {
  if (value->kind != VALUE_NEUTRAL) return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
  Spine *frame = value_spine_new(heap, value_sout(shape, addr));
  Vec spine = frame == NULL ? value->spine : cons(heap, frame, &value->spine);
  return done(value_vneutral(heap, value->head, spine));
}

/* leg: Maybe<VLeg> as the VecGet of Bignum.at over the legs. */
Result eval_selected_leg(Heap *heap, VecGet leg, Globals globals) {
  switch (leg.status) {
    case VEC_GET_OUT_OF_RANGE: return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
    case VEC_GET_OK: {
      const VLeg *selected = (const VLeg *)leg.value;
      if (selected == NULL) return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
      return eval_open_closure(heap, globals, selected->clo, vec_new(heap->arena));
    }
  }
  return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
}

Result eval_out_point(Heap *heap, const Value *value, Globals globals, Shape shape, VAddr addr) {
  bool single = value->kind == VALUE_SEC && value->legs.len == 1 && addr.kind == VADDR_PT;
  const VLeg *leg = single ? (const VLeg *)at(&value->legs, 0) : NULL;
  if (leg != NULL) {
    Vec args = vec_new(heap->arena);
    push(heap, &args, addr.arg);
    return eval_open_closure(heap, globals, leg->clo, args);
  }
  return eval_stuck_out(heap, value, shape, addr);
}

Result eval_out_coll(Heap *heap, const Value *value, Globals globals, Shape shape, VAddr addr) {
  if (value->kind == VALUE_SEC && addr.kind == VADDR_LEG) {
    return eval_selected_leg(heap, big_at(heap, &value->legs, addr.index), globals);
  }
  return eval_stuck_out(heap, value, shape, addr);
}

Result eval_out_value(Heap *heap, Globals globals, Shape shape, VAddr addr, const Value *value) {
  switch (shape.kind) {
    case SHAPE_PI: return eval_out_point(heap, value, globals, shape, addr);
    case SHAPE_ARG: return eval_out_point(heap, value, globals, shape, addr);
    case SHAPE_COLL: return eval_out_coll(heap, value, globals, shape, addr);
    case SHAPE_PAR: return fail(ERROR_NOT_YET, "SPar arrives at M1");
    case SHAPE_MU: return fail(ERROR_NOT_YET, "a right former at a mu shape arrives at M2");
    case SHAPE_NU: return fail(ERROR_NOT_YET, "SNu arrives at M2");
    case SHAPE_ZK: return eval_out_point(heap, value, globals, shape, addr);
    case SHAPE_FHC: return eval_stuck_out(heap, value, shape, addr);
    case SHAPE_MPC: return eval_stuck_out(heap, value, shape, addr);
  }
  return budget_result_fail(error_host_oom());
}

bool eval_branch_match(Addr addr, VAddr target) {
  switch (addr.kind) {
    case ADDR_LEG: return target.kind == VADDR_LEG && big_equal(addr.index, target.index);
    case ADDR_CTOR: return target.kind == VADDR_CTOR && str_eq(addr.name, target.name);
    case ADDR_PT: return false;
  }
  return false;
}

MaybeLegPtr eval_find_branch(const Vec *branches, VAddr addr) {
  MaybeLegPtr out = {0};
  for (size_t i = 0; i < branches->len; i++) {
    const Branch *branch = (const Branch *)at(branches, i);
    if (branch != NULL && eval_branch_match(branch->addr, addr)) {
      out.some = true;
      out.value = &branch->leg;
      return out;
    }
  }
  return out;
}

Result eval_stuck_elim(Heap *heap, const Value *value, StuckElim data) {
  if (value->kind != VALUE_NEUTRAL) return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
  Spine *frame = value_spine_new(heap, value_selim(data));
  Vec spine = frame == NULL ? value->spine : cons(heap, frame, &value->spine);
  return done(value_vneutral(heap, value->head, spine));
}

Result eval_selected_branch(Heap *heap, MaybeLegPtr branch, Globals globals, Vec env, Vec args) {
  if (!branch.some) return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
  Vec reversed = reverse(heap, &args);
  return eval_eval(heap, globals, append(heap, &reversed, &env), branch.value->body);
}

Result eval_elim_zk(Heap *heap, bool zk, Globals globals, Vec env, const Value *fibre, const Value *arg,
                    const Term *body) {
  if (zk) {
    Vec with_arg = cons(heap, arg, &env);
    return eval_eval(heap, globals, cons(heap, fibre, &with_arg), body);
  }
  return fail(ERROR_MISMATCH, CANNOT_ELIMINATE);
}

Result eval_elim_point(Heap *heap, const Value *value, const Vec *branches, Globals globals, Vec env,
                       StuckElim data, bool zk) {
  bool point = value->kind == VALUE_IN && value->addr.kind == VADDR_PT && branches->len == 1;
  const Branch *branch = point ? (const Branch *)at(branches, 0) : NULL;
  const Value *fibre = branch != NULL ? (const Value *)at(&value->args, 0) : NULL;
  if (fibre != NULL && value->args.len == 1) {
    Vec with_arg = cons(heap, value->addr.arg, &env);
    return eval_eval(heap, globals, cons(heap, fibre, &with_arg), branch->leg.body);
  }
  if (fibre != NULL && value->args.len == 2) {
    return eval_elim_zk(heap, zk, globals, env, fibre, value->addr.arg, branch->leg.body);
  }
  return eval_stuck_elim(heap, value, data);
}

Result eval_elim_named(Heap *heap, const Value *value, const Vec *branches, Globals globals, Vec env,
                       StuckElim data) {
  if (value->kind == VALUE_IN) {
    return eval_selected_branch(heap, eval_find_branch(branches, value->addr), globals, env, value->args);
  }
  return eval_stuck_elim(heap, value, data);
}

Result eval_elim_value(Heap *heap, Globals globals, StuckElim data, const Value *value) {
  switch (data.shape.kind) {
    case SHAPE_PAR: return fail(ERROR_NOT_YET, "SPar arrives at M1");
    case SHAPE_NU: return fail(ERROR_NOT_YET, "SNu arrives at M2");
    case SHAPE_PI: return eval_elim_point(heap, value, &data.branches, globals, data.env, data, false);
    case SHAPE_ZK: return eval_elim_point(heap, value, &data.branches, globals, data.env, data, true);
    case SHAPE_COLL: return eval_elim_named(heap, value, &data.branches, globals, data.env, data);
    case SHAPE_MU: return eval_elim_named(heap, value, &data.branches, globals, data.env, data);
    case SHAPE_ARG:
    case SHAPE_FHC:
    case SHAPE_MPC:
      return eval_stuck_elim(heap, value, data);
  }
  return budget_result_fail(error_host_oom());
}

/* Each frame SOut{s, VAPt{q, VLit{lit}}} prepends its literal to acc, so the literals of the
 * spine come out reversed in front of acc; the Literal pointers point into the VLit values. */
MaybeVec eval_spine_literals(Heap *heap, const Vec *spine, Vec acc) {
  Vec lits = vec_new(heap->arena);
  for (size_t i = 0; i < spine->len; i++) {
    const Spine *frame = (const Spine *)at(spine, i);
    bool literal = point_frame(frame) && frame->addr.arg->kind == VALUE_LIT;
    if (!literal) return vec_none();
    push(heap, &lits, &frame->addr.arg->lit);
  }
  Vec reversed = reverse(heap, &lits);
  return vec_some(append(heap, &reversed, &acc));
}

bool eval_canonical_ctor(const Value *value) {
  return value->kind == VALUE_IN && value->addr.kind == VADDR_CTOR;
}

MaybeVec eval_prepend_frame(Heap *heap, const Spine *frame, MaybeVec tail) {
  if (!tail.some) return vec_none();
  return vec_some(cons(heap, frame, &tail.value));
}

/* The Bend recursion walks the frames before index, each of which must be an SOut point frame,
 * and forces the point value at index: a canonical constructor there keeps the frames with the
 * forced value in place, anything else (or a short list) is None. */
MaybeVecResult eval_guarded_frames(Heap *heap, const Vec *frames, Nat index, Globals globals) {
  NatToU64 position = nat_to_u64(index);
  switch (position.status) {
    case NAT_TO_U64_OK: break;
    case NAT_TO_U64_OVERFLOW: return maybe_vec_done(vec_none());
  }
  if (position.value >= frames->len) return maybe_vec_done(vec_none());
  size_t target = (size_t)position.value;
  for (size_t i = 0; i <= target; i++) {
    if (!point_frame((const Spine *)at(frames, i))) return maybe_vec_done(vec_none());
  }
  const Spine *frame = (const Spine *)at(frames, target);
  Result forced = eval_whnf(heap, globals, frame->addr.arg);
  if (forced.kind == RESULT_FAIL) return maybe_vec_fail(forced.error);
  if (!eval_canonical_ctor(value_of(forced))) return maybe_vec_done(vec_none());
  VAddr addr = value_vaddr_pt(frame->addr.quantity, value_of(forced));
  Spine *replaced = value_spine_new(heap, value_sout(frame->shape, addr));
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < frames->len; i++) {
    const void *item = i == target && replaced != NULL ? (const void *)replaced : at(frames, i);
    push(heap, &out, item);
  }
  return maybe_vec_done(vec_some(out));
}

VecResult eval_force_frames(Heap *heap, const Vec *frames, Globals globals) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < frames->len; i++) {
    const Spine *frame = (const Spine *)at(frames, i);
    if (!point_frame(frame)) {
      if (frame != NULL) push(heap, &out, frame);
      continue;
    }
    Result forced = eval_whnf(heap, globals, frame->addr.arg);
    if (forced.kind == RESULT_FAIL) return vec_fail(forced.error);
    VAddr addr = value_vaddr_pt(frame->addr.quantity, value_of(forced));
    Spine *replaced = value_spine_new(heap, value_sout(frame->shape, addr));
    push(heap, &out, replaced != NULL ? (const void *)replaced : (const void *)frame);
  }
  return vec_done(out);
}

Result eval_unfold_guarded(Heap *heap, MaybeVec ready, Globals globals, const Term *body, const Value *value) {
  if (!ready.some) return done(value);
  Result head = eval_eval(heap, globals, vec_new(heap->arena), body);
  if (head.kind == RESULT_FAIL) return head;
  return eval_replay(heap, globals, value_of(head), &ready.value);
}

Result eval_unfold_entry(Heap *heap, MaybeDefEntry entry, Globals globals, const Vec *frames, const Value *value) {
  bool guarded = entry.some && entry.value.reducible && entry.value.rec_arg.some;
  if (!guarded) return done(value);
  MaybeVecResult ready = eval_guarded_frames(heap, frames, entry.value.rec_arg.value, globals);
  if (ready.kind == RESULT_FAIL) return budget_result_fail(ready.error);
  return eval_unfold_guarded(heap, ready.value, globals, entry.value.body, value);
}

Result eval_whnf_neutral(Heap *heap, const Value *value, Globals globals) {
  bool global = value->kind == VALUE_NEUTRAL && value->head.kind == HEAD_GLOBAL;
  if (!global) return done(value);
  Vec frames = reverse(heap, &value->spine);
  return eval_unfold_entry(heap, global_find_def(value->head.name, globals), globals, &frames, value);
}

Result eval_prim_after(Heap *heap, MaybeValue result, Globals globals, const Value *fallback) {
  if (!result.some) return done(fallback);
  return eval_whnf_neutral(heap, result.value, globals);
}

Result eval_prim_args(Heap *heap, MaybeVec args, Prim p, Globals globals, const Value *fallback) {
  if (!args.some) return done(fallback);
  return eval_prim_after(heap, prim_apply(heap, p, &args.value), globals, fallback);
}

Result eval_whnf_global(Heap *heap, MaybeEntry entry, Globals globals, Str name, Vec spine) {
  const Value *neutral = value_vneutral(heap, value_hglobal(name), spine);
  if (!entry.some) return done(neutral);
  switch (entry.value.kind) {
    case ENTRY_PRIM: {
      VecResult forced = eval_force_frames(heap, &spine, globals);
      if (forced.kind == RESULT_FAIL) return budget_result_fail(forced.error);
      MaybeVec lits = eval_spine_literals(heap, &forced.value, vec_new(heap->arena));
      return eval_prim_args(heap, lits, entry.value.prim, globals, neutral);
    }
    case ENTRY_DEF: {
      DefEntry def = entry.value.def;
      if (!(def.reducible && def.rec_arg.some)) return done(neutral);
      Vec frames = reverse(heap, &spine);
      MaybeVecResult ready = eval_guarded_frames(heap, &frames, def.rec_arg.value, globals);
      if (ready.kind == RESULT_FAIL) return budget_result_fail(ready.error);
      return eval_unfold_guarded(heap, ready.value, globals, def.body, neutral);
    }
    case ENTRY_AXIOM: return done(neutral);
  }
  return done(neutral);
}

Result eval_whnf_core(Heap *heap, Globals globals, const Value *value) {
  bool global = value->kind == VALUE_NEUTRAL && value->head.kind == HEAD_GLOBAL;
  if (!global) return done(value);
  return eval_whnf_global(heap, global_find(value->head.name, globals), globals, value->head.name, value->spine);
}

/* The tail recursion over the frames runs as a loop. */
Result eval_replay(Heap *heap, Globals globals, const Value *value, const Vec *frames) {
  const Value *current = value;
  for (size_t i = 0; i < frames->len; i++) {
    const Spine *frame = (const Spine *)at(frames, i);
    if (frame == NULL) continue;
    switch (frame->kind) {
      case SPINE_OUT: {
        Result applied = eval_out_value(heap, globals, frame->shape, frame->addr, current);
        if (applied.kind == RESULT_FAIL) return applied;
        Result reduced = eval_whnf_core(heap, globals, value_of(applied));
        if (reduced.kind == RESULT_FAIL) return reduced;
        current = value_of(reduced);
        break;
      }
      case SPINE_ELIM: {
        Result reduced = eval_elim_value(heap, globals, frame->data, current);
        if (reduced.kind == RESULT_FAIL) return reduced;
        current = value_of(reduced);
        break;
      }
    }
  }
  return done(current);
}

MaybeValueResult eval_unfold_step_entry(Heap *heap, MaybeDefEntry entry, Globals globals, const Vec *spine) {
  bool plain = entry.some && entry.value.reducible && !entry.value.rec_arg.some;
  if (!plain) return maybe_value_done(value_none());
  Result head = eval_eval(heap, globals, vec_new(heap->arena), entry.value.body);
  if (head.kind == RESULT_FAIL) return maybe_value_fail(head.error);
  Vec frames = reverse(heap, spine);
  Result unfolded = eval_replay(heap, globals, value_of(head), &frames);
  if (unfolded.kind == RESULT_FAIL) return maybe_value_fail(unfolded.error);
  return maybe_value_done(value_some(value_of(unfolded)));
}

MaybeValueResult eval_unfold_step(Heap *heap, Globals globals, const Value *value) {
  bool global = value->kind == VALUE_NEUTRAL && value->head.kind == HEAD_GLOBAL;
  if (!global) return maybe_value_done(value_none());
  return eval_unfold_step_entry(heap, global_find_def(value->head.name, globals), globals, &value->spine);
}

Result eval_whnf_unfolded(Heap *heap, MaybeValue step, Globals globals, const Value *core) {
  if (!step.some) return done(core);
  return eval_whnf(heap, globals, step.value);
}

/* whnf_core, then unfold_step, then whnf again on the unfolded value: the tail call through
 * whnf_unfolded runs as a loop. */
Result eval_whnf(Heap *heap, Globals globals, const Value *value) {
  const Value *current = value;
  for (;;) {
    Result core = eval_whnf_core(heap, globals, current);
    if (core.kind == RESULT_FAIL) return core;
    MaybeValueResult step = eval_unfold_step(heap, globals, value_of(core));
    if (step.kind == RESULT_FAIL) return budget_result_fail(step.error);
    if (!step.value.some) return core;
    current = step.value.value;
  }
}

VecResult eval_quote_list(Heap *heap, Readback mode, const Vec *values, Globals globals, Nat size) {
  Vec terms = vec_new(heap->arena);
  for (size_t i = 0; i < values->len; i++) {
    Result term = eval_read_back(heap, mode, globals, size, (const Value *)at(values, i));
    if (term.kind == RESULT_FAIL) return vec_fail(term.error);
    push(heap, &terms, term.value);
  }
  return vec_done(terms);
}

ShapeResult eval_quote_shape(Heap *heap, Readback mode, Shape shape, Globals globals, Nat size) {
  switch (shape.kind) {
    case SHAPE_PI: {
      Result value = eval_read_back(heap, mode, globals, size, (const Value *)shape.domain);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_spi(shape.quantity, shape.name, value.value));
    }
    case SHAPE_ARG: return shape_done(shape_sarg(shape.quantity, shape.name));
    case SHAPE_COLL: return shape_done(shape_scoll(shape.count));
    case SHAPE_PAR: return shape_fail(error_make(ERROR_NOT_YET, str_lit("SPar arrives at M1")));
    case SHAPE_MU: {
      VecResult values = eval_quote_list(heap, mode, &shape.args, globals, size);
      if (values.kind == RESULT_FAIL) return shape_fail(values.error);
      return shape_done(shape_smu(shape.name, values.value));
    }
    case SHAPE_NU: return shape_fail(error_make(ERROR_NOT_YET, str_lit("SNu arrives at M2")));
    case SHAPE_ZK: {
      Result value = eval_read_back(heap, mode, globals, size, (const Value *)shape.domain);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_szk(shape.quantity, shape.name, value.value));
    }
    case SHAPE_FHC: {
      Result value = eval_read_back(heap, mode, globals, size, (const Value *)shape.level);
      if (value.kind == RESULT_FAIL) return shape_fail(value.error);
      return shape_done(shape_sfhc(value.value));
    }
    case SHAPE_MPC: {
      Result p = eval_read_back(heap, mode, globals, size, (const Value *)shape.parties);
      if (p.kind == RESULT_FAIL) return shape_fail(p.error);
      Result a = eval_read_back(heap, mode, globals, size, (const Value *)shape.access);
      if (a.kind == RESULT_FAIL) return shape_fail(a.error);
      return shape_done(shape_smpc(p.value, a.value));
    }
  }
  return shape_fail(error_host_oom());
}

/* [Value.var(size), Value.var(size + 1), ...] with count items; the Nat recursion runs as a loop. */
Vec eval_fresh_vars(Heap *heap, Nat count, Nat size) {
  Vec out = vec_new(heap->arena);
  Nat left = count;
  Nat level = size;
  while (!nat_is_zero(left)) {
    push(heap, &out, value_var(heap, level));
    level = heap_nat_succ(heap, level);
    NatSub next = nat_sub(heap->arena, left, heap_nat_of_u64(heap, 1));
    switch (next.status) {
      case NAT_SUB_OK: left = next.value; break;
      case NAT_SUB_UNDERFLOW: left = nat_zero(); break;
      case NAT_SUB_OOM: heap->oom = true; left = nat_zero(); break;
    }
  }
  return out;
}

AddrResult eval_quote_addr(Heap *heap, Readback mode, Globals globals, Nat size, VAddr addr) {
  switch (addr.kind) {
    case VADDR_PT: {
      Result term = eval_read_back(heap, mode, globals, size, addr.arg);
      if (term.kind == RESULT_FAIL) return addr_fail(term.error);
      return addr_done(term_addr_pt(addr.quantity, term_of(term)));
    }
    case VADDR_LEG: return addr_done(term_addr_leg(addr.index));
    case VADDR_CTOR: return addr_done(term_addr_ctor(addr.name));
  }
  return addr_fail(error_host_oom());
}

LegResult eval_quote_leg(Heap *heap, Readback mode, Globals globals, Nat size, VLeg leg) {
  Nat arity = heap_nat_of_u64(heap, (uint64_t)leg.binders.len);
  Result value = eval_open_closure(heap, globals, leg.clo, eval_fresh_vars(heap, arity, size));
  if (value.kind == RESULT_FAIL) return leg_fail(value.error);
  Result body = eval_read_back(heap, mode, globals, heap_nat_add(heap, size, arity), value_of(value));
  if (body.kind == RESULT_FAIL) return leg_fail(body.error);
  return leg_done(term_leg(leg.binders, term_of(body)));
}

VecResult eval_quote_legs(Heap *heap, Readback mode, const Vec *legs, Globals globals, Nat size) {
  Vec items = vec_new(heap->arena);
  for (size_t i = 0; i < legs->len; i++) {
    const VLeg *leg = (const VLeg *)at(legs, i);
    if (leg == NULL) continue;
    LegResult item = eval_quote_leg(heap, mode, globals, size, *leg);
    if (item.kind == RESULT_FAIL) return vec_fail(item.error);
    Leg *boxed = term_leg_new(heap, item.value.binders, item.value.body);
    if (boxed != NULL) push(heap, &items, boxed);
  }
  return vec_done(items);
}

Nat eval_diagram_arity(Heap *heap, Shape shape) {
  switch (shape.kind) {
    case SHAPE_PI: return heap_nat_of_u64(heap, 1);
    case SHAPE_ZK: return heap_nat_of_u64(heap, 1);
    case SHAPE_ARG:
    case SHAPE_COLL:
    case SHAPE_PAR:
    case SHAPE_MU:
    case SHAPE_NU:
    case SHAPE_FHC:
    case SHAPE_MPC:
      return nat_zero();
  }
  return nat_zero();
}

const Term *eval_quote_annotate(Heap *heap, MaybeLevelVar level, const Term *body) {
  if (!level.some) return body;
  return term_ann(heap, body, term_univ(heap, level.value));
}

const Term *eval_build_former(Heap *heap, bool lan, Shape shape, const Term *body) {
  if (lan) return term_lan(heap, shape, body);
  return term_ran(heap, shape, body);
}

Result eval_quote_former(Heap *heap, Readback mode, Globals globals, Nat size, Shape shape, Closure clo,
                         MaybeLevelVar level, bool lan) {
  Nat arity = eval_diagram_arity(heap, shape);
  ShapeResult s = eval_quote_shape(heap, mode, shape, globals, size);
  if (s.kind == RESULT_FAIL) return budget_result_fail(s.error);
  Result value = eval_open_closure(heap, globals, clo, eval_fresh_vars(heap, arity, size));
  if (value.kind == RESULT_FAIL) return value;
  Result body = eval_read_back(heap, mode, globals, heap_nat_add(heap, size, arity), value_of(value));
  if (body.kind == RESULT_FAIL) return body;
  return done(eval_quote_annotate(heap, level, eval_build_former(heap, lan, s.value, term_of(body))));
}

MaybeMotiveResult eval_quote_motive(Heap *heap, Readback mode, MaybeMotive motive, Globals globals, Vec env,
                                    Nat size) {
  if (!motive.some) return motive_done(term_motive_none());
  Motive m = motive.value;
  Nat arity = heap_nat_of_u64(heap, (uint64_t)m.indices.len + 1);
  Closure clo = value_close(env, m.body);
  Result value = eval_open_closure(heap, globals, clo, eval_fresh_vars(heap, arity, size));
  if (value.kind == RESULT_FAIL) return motive_fail(value.error);
  Result term = eval_read_back(heap, mode, globals, heap_nat_add(heap, size, arity), value_of(value));
  if (term.kind == RESULT_FAIL) return motive_fail(term.error);
  return motive_done(term_motive_some(term_motive(m.ind, m.indices, m.self, term_of(term))));
}

VecResult eval_quote_branches(Heap *heap, Readback mode, const Vec *branches, Globals globals, Vec env,
                              Nat size) {
  Vec items = vec_new(heap->arena);
  for (size_t i = 0; i < branches->len; i++) {
    const Branch *branch = (const Branch *)at(branches, i);
    if (branch == NULL) continue;
    VAddrResult va = eval_eval_addr(heap, globals, env, branch->addr);
    if (va.kind == RESULT_FAIL) return vec_fail(va.error);
    AddrResult a = eval_quote_addr(heap, mode, globals, size, va.value);
    if (a.kind == RESULT_FAIL) return vec_fail(a.error);
    VLeg vleg = value_vleg(branch->leg.binders, value_close(env, branch->leg.body));
    LegResult leg = eval_quote_leg(heap, mode, globals, size, vleg);
    if (leg.kind == RESULT_FAIL) return vec_fail(leg.error);
    Branch *boxed = term_branch_new(heap, a.value, leg.value);
    if (boxed != NULL) push(heap, &items, boxed);
  }
  return vec_done(items);
}

/* The tail recursion over the frames runs as a loop that rebuilds the head. */
Result eval_quote_frames(Heap *heap, Readback mode, const Vec *frames, Globals globals, Nat size,
                         const Term *head) {
  const Term *current = head;
  for (size_t i = 0; i < frames->len; i++) {
    const Spine *frame = (const Spine *)at(frames, i);
    if (frame == NULL) continue;
    switch (frame->kind) {
      case SPINE_OUT: {
        ShapeResult s = eval_quote_shape(heap, mode, frame->shape, globals, size);
        if (s.kind == RESULT_FAIL) return budget_result_fail(s.error);
        AddrResult a = eval_quote_addr(heap, mode, globals, size, frame->addr);
        if (a.kind == RESULT_FAIL) return budget_result_fail(a.error);
        current = term_out(heap, s.value, a.value, current);
        break;
      }
      case SPINE_ELIM: {
        StuckElim data = frame->data;
        ShapeResult s = eval_quote_shape(heap, mode, data.shape, globals, size);
        if (s.kind == RESULT_FAIL) return budget_result_fail(s.error);
        MaybeMotiveResult mo = eval_quote_motive(heap, mode, data.motive, globals, data.env, size);
        if (mo.kind == RESULT_FAIL) return budget_result_fail(mo.error);
        VecResult bs = eval_quote_branches(heap, mode, &data.branches, globals, data.env, size);
        if (bs.kind == RESULT_FAIL) return budget_result_fail(bs.error);
        current = term_elim(heap, term_elim_data(s.value, current, data.scrut_q, mo.value, bs.value));
        break;
      }
    }
  }
  return done(current);
}

const Term *eval_quote_head(Heap *heap, Head head, Nat size) {
  switch (head.kind) {
    case HEAD_LOCAL: {
      NatSub index = nat_sub(heap->arena, size, heap_nat_succ(heap, head.level));
      switch (index.status) {
        case NAT_SUB_OK: return term_var(heap, index.value);
        case NAT_SUB_UNDERFLOW: return term_var(heap, nat_zero());
        case NAT_SUB_OOM: heap->oom = true; return term_auto();
      }
      return term_auto();
    }
    case HEAD_GLOBAL: return term_global(heap, head.name);
  }
  return term_auto();
}

Result eval_read_back(Heap *heap, Readback mode, Globals globals, Nat size, const Value *value) {
  Result reduced = eval_read_back_reduce(heap, mode, globals, value);
  if (reduced.kind == RESULT_FAIL) return reduced;
  return eval_read_back_value(heap, mode, globals, size, value_of(reduced));
}

Result eval_read_back_reduce(Heap *heap, Readback mode, Globals globals, const Value *value) {
  switch (mode) {
    case READBACK_QUOTE: return done(value);
    case READBACK_NORMAL: return eval_whnf(heap, globals, value);
  }
  return done(value);
}

Result eval_read_back_value(Heap *heap, Readback mode, Globals globals, Nat size, const Value *value) {
  switch (value->kind) {
    case VALUE_UNIV: return done(term_univ(heap, value->level));
    case VALUE_LIT: return done(term_lit(heap, value->lit));
    case VALUE_LAN:
      return eval_quote_former(heap, mode, globals, size, value->shape, value->diagram, value->former_level, true);
    case VALUE_RAN:
      return eval_quote_former(heap, mode, globals, size, value->shape, value->diagram, value->former_level, false);
    case VALUE_IN: {
      ShapeResult s = eval_quote_shape(heap, mode, value->shape, globals, size);
      if (s.kind == RESULT_FAIL) return budget_result_fail(s.error);
      AddrResult a = eval_quote_addr(heap, mode, globals, size, value->addr);
      if (a.kind == RESULT_FAIL) return budget_result_fail(a.error);
      VecResult xs = eval_quote_list(heap, mode, &value->args, globals, size);
      if (xs.kind == RESULT_FAIL) return budget_result_fail(xs.error);
      return done(term_in(heap, s.value, a.value, xs.value));
    }
    case VALUE_SEC: {
      ShapeResult s = eval_quote_shape(heap, mode, value->shape, globals, size);
      if (s.kind == RESULT_FAIL) return budget_result_fail(s.error);
      VecResult ls = eval_quote_legs(heap, mode, &value->legs, globals, size);
      if (ls.kind == RESULT_FAIL) return budget_result_fail(ls.error);
      return done(term_sec(heap, s.value, ls.value));
    }
    case VALUE_NEUTRAL: {
      Vec frames = reverse(heap, &value->spine);
      return eval_quote_frames(heap, mode, &frames, globals, size, eval_quote_head(heap, value->head, size));
    }
  }
  return budget_result_fail(error_host_oom());
}

Result eval_quote(Heap *heap, Globals globals, Nat size, const Value *value) {
  return eval_read_back(heap, READBACK_QUOTE, globals, size, value);
}

Result eval_normalize(Heap *heap, Globals globals, Nat size, const Value *value) {
  return eval_read_back(heap, READBACK_NORMAL, globals, size, value);
}
