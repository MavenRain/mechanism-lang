/* Port of bend2/surface/family_poly.bend: universe-polymorphic family schemas. Chunk 1 (Bend lines
 * 1-288): raw traversals over family declarations, the scheme catalog, instance renaming and the
 * export validation that computes instance names.
 * Lists: a Telescope is a Vec of TeleBinder pointers, ctors are CtorDecl pointers, members are Decl
 * pointers, companions are FamilyRaw pointers, names are Str pointers and bindings (Bindings =
 * List<Common.Pair<String, String>>) are PolyRename pointers.
 * Comp values: Vec * (telescopes, ctor lists, bindings), CtorDecl *, FamilyRaw *, Decl * and
 * kernel_unit(). */
#include <string.h>

#include "surface.h"

/* ---- host helpers: boxing, frames, continuations ---- */

typedef Comp (*ItemMapper)(Heap *heap, PolyAction action, void *item);

typedef struct {
  PolyAction action;
  TeleBinder binder;   /* telescope item input */
  CtorDecl ctor;       /* map_ctor input */
  FamilyDecl family;   /* map_family input */
  Vec ctors;           /* map_family input ctors */
  Decl decl;           /* map_member input */
  Str name;            /* the mapped name */
  const LevelVar *level;
  const Term *first;   /* the mapped type */
  Vec args;            /* mapped telescopes and term lists */
  Vec res_params;
  Vec params;
  Vec indices;
  const Vec *items;    /* list stepping: input, cursor, output, item mapper */
  size_t index;
  Vec acc;
  ItemMapper mapper;
  Str family_name;     /* unique_ctors */
  Vec seen;
  Vec names;           /* export_loop */
  Vec reserved;
  Vec sources;
  Vec targets;
  Vec bindings;        /* check_exports */
  Globals globals;     /* chunk 2: the table being extended */
  Globals symbolic;    /* declare_nonempty: the table after the symbolic imports */
  FamilyCatalog catalog;
  Nat arity;
  Vec members;         /* Decl pointers */
  FamilyRaw raw;
  Vec companions;      /* FamilyRaw pointers */
  Family actual;       /* check_reuse */
  Family expected;
  Str local;           /* check_bindings */
  Str existing;
  Vec bound;
  Vec families;
  Vec available;
  Vec reuse;           /* validate_exports */
  Vec exports;
  FamilyScheme scheme;
  Str as_name;
  FamilyImported imported; /* install_symbolic */
  Vec imports;
  Vec trusted;         /* declare_nonempty */
  Vec elaborators;     /* FamilyMemberElaborator pointers */
  FamilyMemberElaborator elaborator;
  Vec reversed;
  Decl *member;
  FamilyDependency dependency; /* chunk 3: compose_dependency */
  FamilyComposed state;
  Str root;
  Vec definitions;     /* Decl pointers */
  Vec fresh;           /* Str pointers */
  Vec generated;
  bool renamed;
  Vec levels;          /* LevelVar pointers (instantiate) */
  Vec supplied;        /* FamilyMemberElaborator pointers */
  FamilyMaybeVec maybe_exports;
  const FamilyGroupState *group; /* group member split (host only) */
  const FamilyComposeState *compose; /* composition member split (host only) */
  bool split;          /* instantiate: stop before map_members (host only, D-264a) */
  bool head_only;      /* compose_dependency: stop before the k8 schema check (host only, D-256a) */
} Frame;

static Comp comp_oom(void) { return budget_failed(error_host_oom()); }

static void *box(Heap *heap, const void *source, size_t size) {
  void *copy = heap_alloc(heap, size);
  if (copy != NULL) memcpy(copy, source, size);
  return copy;
}

static Frame *frame_new(Heap *heap) {
  Frame *frame = heap_alloc(heap, sizeof(Frame));
  if (frame == NULL) return NULL;
  memset(frame, 0, sizeof(Frame));
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

static Comp pure_vec(Heap *heap, Vec vec) { return pure_box(heap, &vec, sizeof vec); }
static Comp pure_unit(void) { return budget_pure(kernel_unit()); }
static Result done_unit(void) { return budget_result_done(kernel_unit()); }
static Result fail_text(ErrorKind kind, Str text) { return budget_result_fail(error_make(kind, text)); }

/* ---- name lists: Vec of Str pointers. A failed push marks the heap, so the run's export boundary
 * reports Error.HostOom (poly.c export_result precedent). ---- */

static Vec names_new(Heap *heap) { return vec_new(heap->arena); }

static void names_push(Heap *heap, Vec *names, Str *name) {
  if (name == NULL || !vec_push(names, name)) heap->oom = true;
}

static Str *str_box(Heap *heap, Str value) { return box(heap, &value, sizeof value); }

static void names_push_copy(Heap *heap, Vec *names, Str value) { names_push(heap, names, str_box(heap, value)); }

static void names_extend(Heap *heap, Vec *out, const Vec *source) {
  size_t i = 0;
  for (; i < source->len; i = i + 1) {
    VecGet got = vec_get(source, i);
    if (got.status == VEC_GET_OK) names_push(heap, out, got.value);
  }
}

static Vec names_copy(Heap *heap, const Vec *source) {
  Vec out = names_new(heap);
  names_extend(heap, &out, source);
  return out;
}

/* head <> tail */
static Vec names_cons(Heap *heap, Str head, const Vec *tail) {
  Vec out = names_new(heap);
  names_push_copy(heap, &out, head);
  names_extend(heap, &out, tail);
  return out;
}

/* List.append(a, b) */
static Vec names_append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = names_new(heap);
  names_extend(heap, &out, a);
  names_extend(heap, &out, b);
  return out;
}

/* ---- list mapping: one stepper shared by map_telescope and map_ctors (poly.c map_list) ---- */

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
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->items = items;
  frame->index = 0;
  frame->acc = vec_new(heap->arena);
  frame->mapper = mapper;
  return map_list_step(heap, frame);
}

/* ---- map_telescope .. map_family: raw family traversals precede trusted checking ---- */

static Comp telescope_item_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  TeleBinder binder;
  binder.quantity = frame->binder.quantity;
  binder.name = frame->binder.name;
  binder.ty = value;
  return pure_box(heap, &binder, sizeof binder);
}

/* The Con step of map_telescope: Binder{quantity, name, ty} with ty mapped. */
static Comp telescope_item(Heap *heap, PolyAction action, void *item) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->binder = *(const TeleBinder *)item;
  return bind_frame(heap, poly_map_term(heap, action, frame->binder.ty), telescope_item_k, frame);
}

/* map_telescope(telescope, action): the Done value is a boxed Vec of TeleBinder pointers. */
Comp family_map_telescope(Heap *heap, const Vec *telescope, PolyAction action) {
  return map_list(heap, action, telescope, telescope_item);
}

static Comp map_ctor_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  CtorDecl ctor;
  ctor.name = frame->ctor.name;
  ctor.args = frame->args;
  ctor.res_params = frame->res_params;
  ctor.res_idx = *(const Vec *)value;
  return pure_box(heap, &ctor, sizeof ctor);
}

static Comp map_ctor_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->res_params = *(const Vec *)value;
  return bind_frame(heap, poly_map_terms(heap, &frame->ctor.res_idx, frame->action), map_ctor_k4, frame);
}

static Comp map_ctor_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->args = *(const Vec *)value;
  return bind_frame(heap, poly_map_terms(heap, &frame->ctor.res_params, frame->action), map_ctor_k3, frame);
}

static Comp map_ctor_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_map_telescope(heap, &frame->ctor.args, frame->action), map_ctor_k2, frame);
}

/* map_ctor(ctor, action): one tick, then args, res_params and res_idx mapped; the Done value is a
 * boxed CtorDecl. */
Comp family_map_ctor(Heap *heap, CtorDecl ctor, PolyAction action) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->ctor = ctor;
  return bind_frame(heap, budget_tick(heap), map_ctor_k1, frame);
}

static Comp ctor_item(Heap *heap, PolyAction action, void *item) {
  return family_map_ctor(heap, *(const CtorDecl *)item, action);
}

/* map_ctors(ctors, action): the Done value is a boxed Vec of CtorDecl pointers. */
Comp family_map_ctors(Heap *heap, const Vec *ctors, PolyAction action) {
  return map_list(heap, action, ctors, ctor_item);
}

static Comp map_family_k6(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyRaw raw;
  raw.family.name = frame->name;
  raw.family.params = frame->params;
  raw.family.indices = frame->indices;
  raw.family.level = frame->level;
  raw.ctors = *(const Vec *)value;
  return pure_box(heap, &raw, sizeof raw);
}

static Comp map_family_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->indices = *(const Vec *)value;
  return bind_frame(heap, family_map_ctors(heap, &frame->ctors, frame->action), map_family_k6, frame);
}

static Comp map_family_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->params = *(const Vec *)value;
  return bind_frame(heap, family_map_telescope(heap, &frame->family.indices, frame->action), map_family_k5, frame);
}

static Comp map_family_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->level = value;
  return bind_frame(heap, family_map_telescope(heap, &frame->family.params, frame->action), map_family_k4, frame);
}

static Comp map_family_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->name = *(const Str *)value;
  return bind_frame(heap, budget_lift(poly_map_level(heap, frame->action, frame->family.level)), map_family_k3, frame);
}

static Comp map_family_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_lift(poly_map_name(heap, frame->action, frame->family.name)), map_family_k2, frame);
}

/* map_family(family, ctors, action): one tick, then name, level, params, indices and ctors mapped;
 * the Done value is a boxed FamilyRaw. */
Comp family_map_family(Heap *heap, FamilyDecl family, const Vec *ctors, PolyAction action) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->family = family;
  frame->ctors = *ctors;
  return bind_frame(heap, budget_tick(heap), map_family_k1, frame);
}

/* ---- member_kind .. unique_ctors ---- */

/* member_kind(kind): Done carries kernel_unit(). */
Result family_member_kind(DeclKind kind) {
  switch (kind) {
    case DECL_DEFINITION:
      return done_unit();
    case DECL_POSTULATE:
      return fail_text(ERROR_NOT_YET, str_lit("family schema members must be definitions"));
  }
  return done_unit();
}

/* member_body(body, name): Done carries the const Term *. */
Result family_member_body(Heap *heap, MaybeTermPtr body, Str name) {
  if (body.some) return budget_result_done((void *)body.value);
  return fail_text(ERROR_CANNOT_INFER, SURFACE_CAT(heap, str_lit("the definition "), name, str_lit(" has no body")));
}

static Comp map_member_k6(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Decl decl;
  decl.name = frame->name;
  decl.kind = frame->decl.kind;
  decl.ty = frame->first;
  decl.body.some = true;
  decl.body.value = value;
  return pure_box(heap, &decl, sizeof decl);
}

static Comp map_member_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return bind_frame(heap, poly_map_term(heap, frame->action, value), map_member_k6, frame);
}

static Comp map_member_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->first = value;
  return bind_frame(heap, budget_lift(family_member_body(heap, frame->decl.body, frame->decl.name)), map_member_k5, frame);
}

static Comp map_member_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->name = *(const Str *)value;
  return bind_frame(heap, poly_map_term(heap, frame->action, frame->decl.ty), map_member_k4, frame);
}

static Comp map_member_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_lift(poly_map_name(heap, frame->action, frame->decl.name)), map_member_k3, frame);
}

static Comp map_member_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_lift(family_member_kind(frame->decl.kind)), map_member_k2, frame);
}

/* map_member(decl, action): one tick, the kind checked, the name, type and body mapped; the Done
 * value is a boxed Decl whose body is always Some. */
Comp family_map_member(Heap *heap, Decl decl, PolyAction action) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->action = action;
  frame->decl = decl;
  return bind_frame(heap, budget_tick(heap), map_member_k1, frame);
}

static Comp unique_ctors_step(Heap *heap, Frame *frame);

static Comp unique_ctors_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  VecGet got = vec_get(frame->items, frame->index);
  CtorDecl *ctor;
  (void)value;
  if (got.status != VEC_GET_OK) return pure_unit();
  ctor = got.value;
  if (poly_member(&frame->seen, ctor->name))
    return budget_lift(fail_text(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("the constructor "), ctor->name,
                                                            str_lit(" is already declared in "), frame->family_name)));
  names_push(heap, &frame->seen, &ctor->name);
  frame->index = frame->index + 1;
  return unique_ctors_step(heap, frame);
}

static Comp unique_ctors_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_unit();
  return bind_frame(heap, budget_tick(heap), unique_ctors_k, frame);
}

/* unique_ctors(ctors, family, seen): one tick per constructor; Done carries kernel_unit(). */
Comp family_unique_ctors(Heap *heap, const Vec *ctors, Str family, const Vec *seen) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = ctors;
  frame->index = 0;
  frame->family_name = family;
  frame->seen = names_copy(heap, seen);
  return unique_ctors_step(heap, frame);
}

/* ---- Scheme, T, empty .. constructors: the scheme catalog ---- */

static MaybeFamilyScheme no_scheme(void) {
  MaybeFamilyScheme none;
  memset(&none, 0, sizeof none);
  none.some = false;
  return none;
}

static FamilyMaybeVec no_names(void) {
  FamilyMaybeVec none;
  memset(&none, 0, sizeof none);
  none.some = false;
  return none;
}

static FamilyMaybeVec some_names(Vec names) {
  FamilyMaybeVec some;
  some.some = true;
  some.value = names;
  return some;
}

/* empty() */
FamilyCatalog family_empty(void) { return NULL; }

/* lookup(catalog, name): the first row with the name wins. */
MaybeFamilyScheme family_lookup(FamilyCatalog catalog, Str name) {
  const FamilyRow *row = catalog;
  for (; row != NULL; row = row->rest) {
    if (str_eq(row->name, name)) {
      MaybeFamilyScheme found;
      found.some = true;
      found.value = row->scheme;
      return found;
    }
  }
  return no_scheme();
}

/* scheme_arity(scheme) */
MaybeNat family_scheme_arity(MaybeFamilyScheme scheme) {
  MaybeNat arity;
  arity.some = scheme.some;
  arity.value = scheme.some ? scheme.value.arity : nat_zero();
  return arity;
}

/* arity(catalog, name) */
MaybeNat family_arity(FamilyCatalog catalog, Str name) { return family_scheme_arity(family_lookup(catalog, name)); }

/* member_names(members): members is a Vec of Decl pointers. */
Vec family_member_names(Heap *heap, const Vec *members) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < members->len; i = i + 1) {
    VecGet got = vec_get(members, i);
    if (got.status == VEC_GET_OK) names_push(heap, &out, &((Decl *)got.value)->name);
  }
  return out;
}

/* family_name(family) */
Str family_family_name(FamilyDecl family) { return family.name; }

/* companion_names(companions): companions is a Vec of FamilyRaw pointers. */
Vec family_companion_names(Heap *heap, const Vec *companions) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < companions->len; i = i + 1) {
    VecGet got = vec_get(companions, i);
    if (got.status == VEC_GET_OK) names_push(heap, &out, &((FamilyRaw *)got.value)->family.name);
  }
  return out;
}

/* ctor_names(ctors): ctors is a Vec of CtorDecl pointers. */
Vec family_ctor_names(Heap *heap, const Vec *ctors) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < ctors->len; i = i + 1) {
    VecGet got = vec_get(ctors, i);
    if (got.status == VEC_GET_OK) names_push(heap, &out, &((CtorDecl *)got.value)->name);
  }
  return out;
}

/* companion_ctors(companions): the constructor names of every companion, in order. */
Vec family_companion_ctors(Heap *heap, const Vec *companions) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < companions->len; i = i + 1) {
    VecGet got = vec_get(companions, i);
    if (got.status == VEC_GET_OK) {
      Vec names = family_ctor_names(heap, &((FamilyRaw *)got.value)->ctors);
      names_extend(heap, &out, &names);
    }
  }
  return out;
}

/* scheme_members(scheme) */
FamilyMaybeVec family_scheme_members(Heap *heap, MaybeFamilyScheme scheme) {
  if (!scheme.some) return no_names();
  return some_names(family_member_names(heap, &scheme.value.members));
}

/* members(catalog, name) */
FamilyMaybeVec family_members(Heap *heap, FamilyCatalog catalog, Str name) {
  return family_scheme_members(heap, family_lookup(catalog, name));
}

/* scheme_companions(scheme) */
FamilyMaybeVec family_scheme_companions(Heap *heap, MaybeFamilyScheme scheme) {
  if (!scheme.some) return no_names();
  return some_names(family_companion_names(heap, &scheme.value.companions));
}

/* companions(catalog, name) */
FamilyMaybeVec family_companions(Heap *heap, FamilyCatalog catalog, Str name) {
  return family_scheme_companions(heap, family_lookup(catalog, name));
}

/* scheme_constructors(scheme): the scheme's own constructors, then the companions' constructors. */
FamilyMaybeVec family_scheme_constructors(Heap *heap, MaybeFamilyScheme scheme) {
  Vec own;
  Vec companions;
  if (!scheme.some) return no_names();
  own = family_ctor_names(heap, &scheme.value.ctors);
  companions = family_companion_ctors(heap, &scheme.value.companions);
  return some_names(names_append(heap, &own, &companions));
}

/* constructors(catalog, name) */
FamilyMaybeVec family_constructors(Heap *heap, FamilyCatalog catalog, Str name) {
  return family_scheme_constructors(heap, family_lookup(catalog, name));
}

/* ---- rename_instance .. instance_names: instance renaming and export validation ---- */

/* rename_instance(scheme, name, as_name, value): the scheme name becomes as_name; its family,
 * companions and members get the as_name prefix; every other name is kept. */
Str family_rename_instance(Heap *heap, FamilyScheme scheme, Str name, Str as_name, Str value) {
  Vec companions;
  Vec members;
  if (str_eq(value, name)) return as_name;
  companions = family_companion_names(heap, &scheme.companions);
  members = family_member_names(heap, &scheme.members);
  if (str_eq(value, family_family_name(scheme.family)) || poly_member(&companions, value) || poly_member(&members, value))
    return SURFACE_CAT(heap, as_name, str_lit("_"), value);
  return value;
}

/* mapping_find(mapping, name): mapping is a Vec of PolyRename pointers; the first source match wins. */
MaybeStr family_mapping_find(const Vec *mapping, Str name) {
  MaybeStr found;
  size_t i = 0;
  for (; i < mapping->len; i = i + 1) {
    VecGet got = vec_get(mapping, i);
    if (got.status == VEC_GET_OK && str_eq(((const PolyRename *)got.value)->source, name)) {
      found.some = true;
      found.value = ((const PolyRename *)got.value)->target;
      return found;
    }
  }
  found.some = false;
  found.value = str_empty();
  return found;
}

/* mapped_name(value, fallback) */
Str family_mapped_name(MaybeStr value, Str fallback) {
  if (value.some) return value.value;
  return fallback;
}

/* rename_export(scheme, name, as_name, exports, value) */
Str family_rename_export(Heap *heap, FamilyScheme scheme, Str name, Str as_name, const Vec *exports, Str value) {
  return family_mapped_name(family_mapping_find(exports, value), family_rename_instance(heap, scheme, name, as_name, value));
}

/* planned_families(families, scheme, name, as_name, reuse): the instance name of every family, a
 * reuse binding winning over the rename. */
Vec family_planned_families(Heap *heap, const Vec *families, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < families->len; i = i + 1) {
    VecGet got = vec_get(families, i);
    if (got.status == VEC_GET_OK) {
      Str family = *(const Str *)got.value;
      names_push_copy(heap, &out,
                      family_mapped_name(family_mapping_find(reuse, family),
                                         family_rename_instance(heap, scheme, name, as_name, family)));
    }
  }
  return out;
}

/* collision(name) */
Error family_collision(Heap *heap, Str name) {
  return error_make(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("the name "), name, str_lit(" is already declared")));
}

/* validate_export_binding(source, target, names, sources, reserved, targets): Done carries kernel_unit(). */
Result family_validate_export_binding(Heap *heap, Str source, Str target, const Vec *names, const Vec *sources,
                                      const Vec *reserved, const Vec *targets) {
  if (!poly_member(names, source) || poly_member(sources, source))
    return fail_text(ERROR_MISMATCH, str_lit("member exports must name each member once"));
  if (poly_member(reserved, target)) return budget_result_fail(family_collision(heap, target));
  if (poly_member(targets, target))
    return fail_text(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("member exports repeat the target "), target));
  return done_unit();
}

static Comp export_loop_step(Heap *heap, Frame *frame);

static Comp export_loop_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  VecGet got = vec_get(frame->items, frame->index);
  PolyRename *binding;
  Result checked;
  (void)value;
  if (got.status != VEC_GET_OK) return export_loop_step(heap, frame);
  binding = got.value;
  checked = family_validate_export_binding(heap, binding->source, binding->target, &frame->names, &frame->sources,
                                           &frame->reserved, &frame->targets);
  if (checked.kind == RESULT_FAIL) return budget_lift(checked);
  names_push(heap, &frame->sources, &binding->source);
  names_push(heap, &frame->targets, &binding->target);
  frame->index = frame->index + 1;
  return export_loop_step(heap, frame);
}

static Comp export_loop_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) {
    if (frame->sources.len == frame->names.len) return budget_lift(done_unit());
    return budget_lift(fail_text(ERROR_MISMATCH, str_lit("every family member needs an export name")));
  }
  return bind_frame(heap, budget_tick(heap), export_loop_k, frame);
}

/* export_loop(bindings, names, reserved, sources, targets): one tick per binding, each validated
 * against the bindings before it; at the end every member must have been named. Done carries
 * kernel_unit(). */
Comp family_export_loop(Heap *heap, const Vec *bindings, const Vec *names, const Vec *reserved, const Vec *sources,
                        const Vec *targets) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = bindings;
  frame->index = 0;
  frame->names = *names;
  frame->reserved = *reserved;
  frame->sources = names_copy(heap, sources);
  frame->targets = names_copy(heap, targets);
  return export_loop_step(heap, frame);
}

static Comp check_exports_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return pure_vec(heap, frame->bindings);
}

/* check_exports(scheme, name, as_name, reuse, exports): no exports validate as the empty binding
 * list; otherwise the bindings are checked against the planned family names and constructor labels
 * and returned. The Done value is a boxed Vec of PolyRename pointers. */
Comp family_check_exports(Heap *heap, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse, FamilyMaybeVec exports) {
  Frame *frame;
  Vec companion_names;
  Vec family_names;
  Vec families;
  Vec own_ctors;
  Vec companion_ctors;
  Vec labels;
  Vec planned;
  Vec reserved;
  Vec members;
  if (!exports.some) return pure_vec(heap, names_new(heap));
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  companion_names = family_companion_names(heap, &scheme.companions);
  family_names = names_cons(heap, family_family_name(scheme.family), &companion_names);
  families = family_planned_families(heap, &family_names, scheme, name, as_name, reuse);
  own_ctors = family_ctor_names(heap, &scheme.ctors);
  companion_ctors = family_companion_ctors(heap, &scheme.companions);
  labels = names_append(heap, &own_ctors, &companion_ctors);
  planned = names_append(heap, &families, &labels);
  reserved = names_cons(heap, as_name, &planned);
  members = family_member_names(heap, &scheme.members);
  frame->bindings = exports.value;
  frame->sources = names_new(heap);
  frame->targets = names_new(heap);
  return bind_frame(heap, family_export_loop(heap, &frame->bindings, &members, &reserved, &frame->sources, &frame->targets),
                    check_exports_k, frame);
}

/* keep_fresh(reused, name, tail): a reused family is not fresh; the Con case pushes the name. */
static void keep_fresh(Heap *heap, MaybeStr reused, Str name, Vec *out) {
  if (reused.some) return;
  names_push_copy(heap, out, name);
}

/* fresh_family_names(families, scheme, name, as_name, reuse, exports): the export names of the
 * families that are not reused. */
Vec family_fresh_family_names(Heap *heap, const Vec *families, FamilyScheme scheme, Str name, Str as_name, const Vec *reuse,
                              const Vec *exports) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < families->len; i = i + 1) {
    VecGet got = vec_get(families, i);
    if (got.status == VEC_GET_OK) {
      Str family = *(const Str *)got.value;
      keep_fresh(heap, family_mapping_find(reuse, family), family_rename_export(heap, scheme, name, as_name, exports, family),
                 &out);
    }
  }
  return out;
}

/* exported_member_names(names, scheme, name, as_name, exports) */
Vec family_exported_member_names(Heap *heap, const Vec *names, FamilyScheme scheme, Str name, Str as_name, const Vec *exports) {
  Vec out = names_new(heap);
  size_t i = 0;
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OK)
      names_push_copy(heap, &out, family_rename_export(heap, scheme, name, as_name, exports, *(const Str *)got.value));
  }
  return out;
}

static MaybeFamilyInstanceNames no_instance_names(void) {
  MaybeFamilyInstanceNames none;
  memset(&none, 0, sizeof none);
  none.some = false;
  return none;
}

/* instance_names_result(result, scheme, name, as_name, reuse): a failed export check gives None. */
MaybeFamilyInstanceNames family_instance_names_result(Heap *heap, Result result, FamilyScheme scheme, Str name, Str as_name,
                                                      const Vec *reuse) {
  MaybeFamilyInstanceNames out;
  Vec exports;
  Vec companion_names;
  Vec family_names;
  Vec members;
  if (result.kind == RESULT_FAIL) return no_instance_names();
  exports = *(const Vec *)result.value;
  companion_names = family_companion_names(heap, &scheme.companions);
  family_names = names_cons(heap, family_family_name(scheme.family), &companion_names);
  members = family_member_names(heap, &scheme.members);
  out.some = true;
  out.value.families = family_fresh_family_names(heap, &family_names, scheme, name, as_name, reuse, &exports);
  out.value.members = family_exported_member_names(heap, &members, scheme, name, as_name, &exports);
  return out;
}

/* instance_names_scheme(scheme, name, as_name, reuse, exports): check_exports under an unlimited budget. */
MaybeFamilyInstanceNames family_instance_names_scheme(Heap *heap, MaybeFamilyScheme scheme, Str name, Str as_name,
                                                      const Vec *reuse, FamilyMaybeVec exports) {
  Result checked;
  if (!scheme.some) return no_instance_names();
  checked = budget_run(heap, family_check_exports(heap, scheme.value, name, as_name, reuse, exports), budget_unlimited());
  if (heap->oom) return no_instance_names();
  return family_instance_names_result(heap, checked, scheme.value, name, as_name, reuse);
}

/* instance_names(catalog, name, as_name, reuse, exports) */
MaybeFamilyInstanceNames family_instance_names(Heap *heap, FamilyCatalog catalog, Str name, Str as_name, const Vec *reuse,
                                               FamilyMaybeVec exports) {
  return family_instance_names_scheme(heap, family_lookup(catalog, name), name, as_name, reuse, exports);
}

/* ---- chunk 2 (Bend lines 289-600): symbolic declaration of family schema groups ---- */

/* Pointer vectors shared by every list in this chunk: FamilyRaw *, FamilyImported *, Decl *,
 * FamilyMemberElaborator * and Ctor * items. A failed push marks the heap (names_push). */
static void items_push(Heap *heap, Vec *out, void *item) { names_push(heap, out, item); }

static void items_push_box(Heap *heap, Vec *out, const void *source, size_t size) {
  items_push(heap, out, box(heap, source, size));
}

/* head <> tail over pointer items */
static Vec items_cons(Heap *heap, void *head, const Vec *tail) {
  Vec out = vec_new(heap->arena);
  items_push(heap, &out, head);
  names_extend(heap, &out, tail);
  return out;
}

/* the list without its head */
static Vec items_tail(Heap *heap, const Vec *source) {
  Vec out = vec_new(heap->arena);
  size_t i = 1;
  for (; i < source->len; i = i + 1) {
    VecGet got = vec_get(source, i);
    if (got.status == VEC_GET_OK) items_push(heap, &out, got.value);
  }
  return out;
}

static Comp pure_globals(Heap *heap, Globals globals) { return pure_box(heap, &globals, sizeof globals); }

static FamilyScheme scheme_make(Nat arity, FamilyDecl family, Vec ctors, Vec companions, Vec members, Globals origin) {
  FamilyScheme scheme;
  scheme.arity = arity;
  scheme.family = family;
  scheme.ctors = ctors;
  scheme.companions = companions;
  scheme.members = members;
  scheme.origin = origin;
  return scheme;
}

/* Common.Pair{name, scheme} <> catalog: the Done value is the new FamilyRow * (poly.c declare precedent). */
static Comp row_pure(Heap *heap, Str name, FamilyScheme scheme, FamilyCatalog rest) {
  FamilyRow *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return comp_oom();
  row->name = name;
  row->scheme = scheme;
  row->rest = rest;
  return budget_pure(row);
}

/* [name] as a Vec of one Str pointer kept in the frame; false when the allocation failed */
static bool frame_group(Heap *heap, Frame *frame, Str name) {
  frame->names = names_new(heap);
  names_push_copy(heap, &frame->names, name);
  return !heap->oom;
}

static PolyAction scope_action(Heap *heap, FamilyCatalog catalog, Nat arity) {
  return poly_action(poly_scope_levels(arity), names_new(heap), family_catalog_names(heap, catalog));
}

Vec family_catalog_names(Heap *heap, FamilyCatalog catalog) {
  Vec out = names_new(heap);
  const FamilyRow *row = catalog;
  for (; row != NULL; row = row->rest) names_push(heap, &out, (Str *)&row->name);
  return out;
}

bool family_has_ctor(const FamilyRows *families, Str name) {
  const FamilyRows *row = families;
  for (; row != NULL; row = row->rest) {
    if (positivity_find_ctor(&row->family.ctors, name).some) return true;
  }
  return false;
}

bool family_ctor_declared(Globals globals, Str name) { return family_has_ctor(globals.families, name); }

bool family_occupied(Globals globals, FamilyCatalog catalog, Str name) {
  return global_find(name, globals).some || global_find_family(name, globals).some || family_ctor_declared(globals, name) ||
         family_lookup(catalog, name).some;
}

Comp family_require_fresh(Heap *heap, bool used, Str name) {
  if (used) return budget_lift(budget_result_fail(family_collision(heap, name)));
  return budget_lift(done_unit());
}

/* ---- map_members ---- */

static Comp member_item(Heap *heap, PolyAction action, void *item) {
  return family_map_member(heap, *(const Decl *)item, action);
}

Comp family_map_members(Heap *heap, const Vec *members, PolyAction action) {
  return map_list(heap, action, members, member_item);
}

/* ---- check_members: each member is fresh, kernel checked and added to the table ---- */

static Comp check_members_step(Heap *heap, Frame *frame);

static Comp check_members_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = global_add(heap, frame->name, *(const Entry *)value, frame->globals);
  frame->index = frame->index + 1;
  return check_members_step(heap, frame);
}

static Comp check_members_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, frame->arity, frame->decl), check_members_k3, frame);
}

static Comp check_members_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->name = poly_decl_name(frame->decl);
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->name), frame->name),
                    check_members_k2, frame);
}

static Comp check_members_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_globals(heap, frame->globals);
  frame->decl = *(const Decl *)got.value;
  return bind_frame(heap, budget_tick(heap), check_members_k1, frame);
}

Comp family_check_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Nat arity, Globals globals) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->index = 0;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->globals = globals;
  return check_members_step(heap, frame);
}

/* One check_members item on its own (host only, no Bend counterpart): the same tick, require_fresh, kernel
 * check and global_add as one check_members_step, so the compact elaboration path can reset its arena
 * between two members of an instantiate entry (USER ruling 2026-10-12 (e)). Done = Globals *. */
static Comp check_member_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return pure_globals(heap, global_add(heap, frame->name, *(const Entry *)value, frame->globals));
}

static Comp check_member_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, check_check_decl_comp(heap, frame->globals, frame->arity, frame->decl), check_member_k3, frame);
}

static Comp check_member_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->name = poly_decl_name(frame->decl);
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->name), frame->name),
                    check_member_k2, frame);
}

Comp family_check_member(Heap *heap, Decl decl, FamilyCatalog catalog, Nat arity, Globals globals) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->decl = decl;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->globals = globals;
  return bind_frame(heap, budget_tick(heap), check_member_k1, frame);
}

/* ---- trusted_entry, add_members: composition members enter the symbolic table without a re-check ---- */

static Comp trusted_entry_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Entry entry = check_checked_entry(frame->decl.ty, value);
  return pure_box(heap, &entry, sizeof entry);
}

static Comp trusted_entry_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_lift(family_member_body(heap, frame->decl.body, frame->decl.name)), trusted_entry_k2,
                    frame);
}

Comp family_trusted_entry(Heap *heap, Decl decl) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->decl = decl;
  return bind_frame(heap, budget_lift(family_member_kind(decl.kind)), trusted_entry_k1, frame);
}

static Comp add_members_step(Heap *heap, Frame *frame);

static Comp add_members_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = global_add(heap, frame->name, *(const Entry *)value, frame->globals);
  frame->index = frame->index + 1;
  return add_members_step(heap, frame);
}

static Comp add_members_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_trusted_entry(heap, frame->decl), add_members_k3, frame);
}

static Comp add_members_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->name = poly_decl_name(frame->decl);
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->name), frame->name),
                    add_members_k2, frame);
}

static Comp add_members_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_globals(heap, frame->globals);
  frame->decl = *(const Decl *)got.value;
  return bind_frame(heap, budget_tick(heap), add_members_k1, frame);
}

Comp family_add_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Globals globals) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->index = 0;
  frame->catalog = catalog;
  frame->globals = globals;
  return add_members_step(heap, frame);
}

Comp family_scope_members(Heap *heap, const Vec *members, FamilyCatalog catalog, Nat arity) {
  return family_map_members(heap, members, scope_action(heap, catalog, arity));
}

/* ---- reserve_members ---- */

static Comp reserve_members_step(Heap *heap, Frame *frame);

static Comp reserve_members_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->index = frame->index + 1;
  return reserve_members_step(heap, frame);
}

static Comp reserve_members_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  const Decl *decl;
  if (got.status != VEC_GET_OK) return pure_unit();
  decl = got.value;
  return bind_frame(heap, family_require_fresh(heap, family_lookup(frame->catalog, decl->name).some, decl->name),
                    reserve_members_k, frame);
}

Comp family_reserve_members(Heap *heap, const Vec *members, FamilyCatalog catalog) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->index = 0;
  frame->catalog = catalog;
  return reserve_members_step(heap, frame);
}

/* ---- family_members: the members are checked against the provisional family and its constructors ---- */

static Comp family_members_k3(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

static Comp family_members_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *symbolic = value;
  return bind_frame(heap, family_check_members(heap, frame->items, frame->catalog, frame->arity, *symbolic),
                    family_members_k3, frame);
}

static Comp family_members_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *provisional = value;
  Str name = family_family_name(frame->family);
  if (!frame_group(heap, frame, name)) return comp_oom();
  return bind_frame(heap, check_define_ctors_comp(heap, *provisional, frame->arity, &frame->names, name, frame->ctors),
                    family_members_k2, frame);
}

Comp family_family_members(Heap *heap, const Vec *members, Globals globals, FamilyCatalog catalog, Nat arity,
                           FamilyDecl family, Vec ctors) {
  Frame *frame;
  if (members->len == 0) return pure_unit();
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->family = family;
  frame->ctors = ctors;
  return bind_frame(heap, check_declare_family_comp(heap, globals, arity, family), family_members_k1, frame);
}

/* ---- declared_family, declare_comp, declare ---- */

static Comp declared_family_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return row_pure(heap, family_family_name(frame->raw.family),
                  scheme_make(frame->arity, frame->raw.family, frame->raw.ctors, vec_new(heap->arena), frame->members,
                              frame->globals),
                  frame->catalog);
}

static Comp declared_family_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->members = *(const Vec *)value;
  return bind_frame(heap,
                    family_family_members(heap, &frame->members, frame->globals, frame->catalog, frame->arity,
                                          frame->raw.family, frame->raw.ctors),
                    declared_family_k4, frame);
}

static Comp declared_family_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_map_members(heap, frame->items, frame->action), declared_family_k3, frame);
}

static Comp declared_family_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_reserve_members(heap, frame->items, frame->catalog), declared_family_k2, frame);
}

Comp family_declared_family(Heap *heap, FamilyRaw raw, Globals globals, FamilyCatalog catalog, Nat arity,
                            const Vec *members, PolyAction action) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->raw = raw;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->items = members;
  frame->action = action;
  return bind_frame(heap, check_check_family_scheme_comp(heap, globals, arity, raw.family, raw.ctors), declared_family_k1,
                    frame);
}

static Comp declare_comp_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const FamilyRaw *raw = value;
  return family_declared_family(heap, *raw, frame->globals, frame->catalog, frame->arity, frame->items, frame->action);
}

static Comp declare_comp_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->action = scope_action(heap, frame->catalog, frame->arity);
  return bind_frame(heap, family_map_family(heap, frame->family, &frame->ctors, frame->action), declare_comp_k3, frame);
}

static Comp declare_comp_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->seen = names_new(heap);
  return bind_frame(heap, family_unique_ctors(heap, &frame->ctors, family_family_name(frame->family), &frame->seen),
                    declare_comp_k2, frame);
}

Comp family_declare_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, FamilyDecl family, Vec ctors,
                         const Vec *members) {
  Str name = family_family_name(family);
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->family = family;
  frame->ctors = ctors;
  frame->items = members;
  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name), declare_comp_k1,
                    frame);
}

Result family_declare(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity, FamilyDecl family,
                      Vec ctors, const Vec *members) {
  return budget_run(heap, family_declare_comp(heap, globals, catalog, arity, family, ctors, members), budget);
}

/* ---- family reuse: the existing family must match the template up to its level ---- */

Comp family_family_option(Heap *heap, MaybeFamily value, Error error) {
  if (value.some) return pure_box(heap, &value.value, sizeof value.value);
  return budget_failed(error);
}

Globals family_drop_family(Heap *heap, Globals globals, Str name) { return global_without_family(heap, name, globals); }

Family family_replace_level(Family family, const LevelVar *level) {
  family.level = level;
  return family;
}

const LevelVar *family_kernel_level(Family family) { return family.level; }

Vec family_erase_rec(Heap *heap, const Vec *ctors) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < ctors->len; i = i + 1) {
    VecGet got = vec_get(ctors, i);
    if (got.status == VEC_GET_OK) {
      Ctor ctor = *(const Ctor *)got.value;
      ctor.self_rec = false;
      items_push_box(heap, &out, &ctor, sizeof ctor);
    }
  }
  return out;
}

Family family_erase_self_rec(Heap *heap, Family family) {
  family.ctors = family_erase_rec(heap, &family.ctors);
  return family;
}

static Comp reuse_mismatch(Heap *heap, Str name, const char *tail) {
  return budget_lift(fail_text(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("the reused family "), name, str_lit(tail))));
}

Comp family_reuse_agrees(Heap *heap, bool level_ok, Family actual, Family expected, Str name, Globals globals) {
  if (level_ok && positivity_same_family(actual, expected)) return pure_globals(heap, globals);
  if (level_ok && positivity_same_family(family_erase_self_rec(heap, actual), family_erase_self_rec(heap, expected)))
    return reuse_mismatch(heap, name, " is a member of a mutual group, which family reuse does not support");
  return reuse_mismatch(heap, name, " does not match the template");
}

static Comp check_reuse_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return family_reuse_agrees(heap, kernel_bool_unbox(value), frame->actual,
                             family_replace_level(frame->expected, family_kernel_level(frame->actual)), frame->name,
                             frame->globals);
}

static Comp check_reuse_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    level_eq_decide_m(heap, family_kernel_level(frame->actual), family_kernel_level(frame->expected), true),
                    check_reuse_k5, frame);
}

static Comp check_reuse_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->expected = *(const Family *)value;
  return bind_frame(heap, budget_tick(heap), check_reuse_k4, frame);
}

static Comp check_reuse_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *checked = value;
  return bind_frame(heap,
                    family_family_option(heap, global_find_family(frame->name, *checked),
                                         error_make(ERROR_CANNOT_INFER, str_lit("reuse rechecking returned no family"))),
                    check_reuse_k3, frame);
}

static Comp check_reuse_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *provisional = value;
  if (!frame_group(heap, frame, frame->name)) return comp_oom();
  return bind_frame(heap,
                    check_define_ctors_comp(heap, *provisional, frame->arity, &frame->names, frame->name, frame->ctors),
                    check_reuse_k2, frame);
}

static Comp check_reuse_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->actual = *(const Family *)value;
  return bind_frame(heap,
                    check_declare_family_comp(heap, family_drop_family(heap, frame->globals, frame->name), frame->arity,
                                              frame->family),
                    check_reuse_k1, frame);
}

Comp family_check_reuse(Heap *heap, Globals globals, Nat arity, FamilyDecl family, Vec ctors) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->arity = arity;
  frame->family = family;
  frame->ctors = ctors;
  frame->name = family_family_name(family);
  return bind_frame(heap,
                    family_family_option(heap, global_find_family(frame->name, globals),
                                         error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("unknown reused family "),
                                                                           frame->name))),
                    check_reuse_k0, frame);
}

/* ---- reuse bindings ---- */

Result family_check_binding(Heap *heap, Str local, Str existing, const Vec *bound, const Vec *families,
                            const Vec *available, Globals globals) {
  if (poly_member(bound, local))
    return fail_text(ERROR_MISMATCH, SURFACE_CAT(heap, str_lit("the family "), local, str_lit(" is bound more than once")));
  if (!poly_member(families, local))
    return fail_text(ERROR_UNBOUND, str_cat(heap->arena, str_lit("unknown template family "), local));
  if (poly_member(available, existing) || global_find_family(existing, globals).some) return done_unit();
  return fail_text(ERROR_UNBOUND, str_cat(heap->arena, str_lit("unknown reused family "), existing));
}

static Comp check_bindings_step(Heap *heap, Frame *frame);

static Comp check_bindings_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->bound = names_cons(heap, frame->local, &frame->bound);
  frame->index = frame->index + 1;
  return check_bindings_step(heap, frame);
}

static Comp check_bindings_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    budget_lift(family_check_binding(heap, frame->local, frame->existing, &frame->bound, &frame->families,
                                                     &frame->available, frame->globals)),
                    check_bindings_k2, frame);
}

static Comp check_bindings_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  const PolyRename *pair;
  if (got.status != VEC_GET_OK) return pure_unit();
  pair = got.value;
  frame->local = pair->source;
  frame->existing = pair->target;
  return bind_frame(heap, budget_tick(heap), check_bindings_k1, frame);
}

Comp family_check_bindings(Heap *heap, const Vec *reuse, const Vec *families, const Vec *available, Globals globals,
                           const Vec *bound) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = reuse;
  frame->index = 0;
  frame->families = *families;
  frame->available = *available;
  frame->globals = globals;
  frame->bound = *bound;
  return check_bindings_step(heap, frame);
}

/* ---- scheme projections ---- */

Vec family_raw_families(Heap *heap, FamilyScheme scheme) {
  FamilyRaw head;
  Vec out = vec_new(heap->arena);
  head.family = scheme.family;
  head.ctors = scheme.ctors;
  items_push_box(heap, &out, &head, sizeof head);
  names_extend(heap, &out, &scheme.companions);
  return out;
}

Vec family_schema_members(FamilyScheme scheme) { return scheme.members; }

Nat family_schema_arity(FamilyScheme scheme) { return scheme.arity; }

Globals family_schema_table(FamilyScheme scheme) { return scheme.origin; }

Comp family_require_schema(Heap *heap, MaybeFamilyScheme scheme, Str name) {
  if (!scheme.some)
    return budget_failed(error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("unknown family universe schema "), name)));
  return pure_box(heap, &scheme.value, sizeof scheme.value);
}

/* ---- validate_exports ---- */

static Comp validate_exports_k3(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

static Comp validate_exports_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyMaybeVec exports;
  (void)value;
  exports.some = true;
  exports.value = frame->exports;
  return bind_frame(heap, family_check_exports(heap, frame->scheme, frame->name, frame->as_name, &frame->reuse, exports),
                    validate_exports_k3, frame);
}

static Comp validate_exports_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->scheme = *(const FamilyScheme *)value;
  frame->companions = family_raw_families(heap, frame->scheme);
  frame->families = family_companion_names(heap, &frame->companions);
  frame->bound = names_new(heap);
  return bind_frame(heap,
                    family_check_bindings(heap, &frame->reuse, &frame->families, &frame->available, frame->globals,
                                          &frame->bound),
                    validate_exports_k2, frame);
}

static Comp validate_exports_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_require_schema(heap, family_lookup(frame->catalog, frame->name), frame->name),
                    validate_exports_k1, frame);
}

Comp family_validate_exports_comp(Heap *heap, Globals globals, const Vec *available, FamilyCatalog catalog, Str name,
                                  Str as_name, const Vec *reuse, const Vec *exports) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->available = *available;
  frame->catalog = catalog;
  frame->name = name;
  frame->as_name = as_name;
  frame->reuse = *reuse;
  frame->exports = *exports;
  return bind_frame(heap, budget_tick(heap), validate_exports_k0, frame);
}

Result family_validate_exports(Heap *heap, Globals globals, const Vec *available, FamilyCatalog catalog, Budget budget,
                               Str name, Str as_name, const Vec *reuse, const Vec *exports) {
  return budget_run(heap, family_validate_exports_comp(heap, globals, available, catalog, name, as_name, reuse, exports),
                    budget);
}

/* ---- symbolic imports ---- */

Vec family_fresh_imports(Heap *heap, const Vec *imports) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < imports->len; i = i + 1) {
    VecGet got = vec_get(imports, i);
    if (got.status == VEC_GET_OK && !((const FamilyImported *)got.value)->reused) {
      FamilyRaw raw;
      raw.family = ((const FamilyImported *)got.value)->family;
      raw.ctors = ((const FamilyImported *)got.value)->ctors;
      items_push_box(heap, &out, &raw, sizeof raw);
    }
  }
  return out;
}

static Comp install_import_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *provisional = value;
  Str name = family_family_name(frame->family);
  if (!frame_group(heap, frame, name)) return comp_oom();
  return check_define_ctors_comp(heap, *provisional, frame->arity, &frame->names, name, frame->ctors);
}

static Comp install_import_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, check_declare_family_comp(heap, frame->globals, frame->arity, frame->family), install_import_k2,
                    frame);
}

Comp family_install_symbolic_import(Heap *heap, FamilyImported imported, Globals globals, FamilyCatalog catalog, Nat arity) {
  Frame *frame;
  if (imported.reused) return family_check_reuse(heap, globals, arity, imported.family, imported.ctors);
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->family = imported.family;
  frame->ctors = imported.ctors;
  frame->members = vec_new(heap->arena);
  return bind_frame(heap,
                    family_declare_comp(heap, globals, catalog, arity, imported.family, imported.ctors, &frame->members),
                    install_import_k1, frame);
}

static Comp install_symbolic_step(Heap *heap, Frame *frame);

static Comp install_symbolic_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = *(const Globals *)value;
  frame->index = frame->index + 1;
  return install_symbolic_step(heap, frame);
}

static Comp install_symbolic_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_install_symbolic_import(heap, frame->imported, frame->globals, frame->catalog, frame->arity),
                    install_symbolic_k2, frame);
}

static Comp install_symbolic_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_globals(heap, frame->globals);
  frame->imported = *(const FamilyImported *)got.value;
  return bind_frame(heap, budget_tick(heap), install_symbolic_k1, frame);
}

Comp family_install_symbolic(Heap *heap, const Vec *imports, Globals globals, FamilyCatalog catalog, Nat arity) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = imports;
  frame->index = 0;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  return install_symbolic_step(heap, frame);
}

/* ---- elaborate_members: each callback sees the table extended by the members before it ---- */

static Comp elaborate_members_step(Heap *heap, Frame *frame);

static Comp elaborate_members_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = *(const Globals *)value;
  frame->reversed = items_cons(heap, frame->member, &frame->reversed);
  frame->index = frame->index + 1;
  return elaborate_members_step(heap, frame);
}

static Comp elaborate_members_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->member = value;
  frame->members = vec_new(heap->arena);
  if (!vec_push(&frame->members, frame->member)) return comp_oom();
  return bind_frame(heap, family_check_members(heap, &frame->members, frame->catalog, frame->arity, frame->globals),
                    elaborate_members_k4, frame);
}

static Comp elaborate_members_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Decl *member = value;
  return bind_frame(heap, family_map_member(heap, *member, scope_action(heap, frame->catalog, frame->arity)),
                    elaborate_members_k3, frame);
}

static Comp elaborate_members_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, frame->elaborator.fn(heap, frame->elaborator.ctx, frame->globals), elaborate_members_k2, frame);
}

static Comp elaborate_members_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_vec(heap, vec_reverse(heap->arena, &frame->reversed));
  frame->elaborator = *(const FamilyMemberElaborator *)got.value;
  return bind_frame(heap, budget_tick(heap), elaborate_members_k1, frame);
}

Comp family_elaborate_members(Heap *heap, const Vec *members, Globals globals, FamilyCatalog catalog, Nat arity,
                              const Vec *reversed) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->index = 0;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->reversed = *reversed;
  return elaborate_members_step(heap, frame);
}

/* ---- declare_nonempty .. declare_group ---- */

static Comp declare_nonempty_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->members = *(const Vec *)value;
  return row_pure(heap, family_family_name(frame->raw.family),
                  scheme_make(frame->arity, frame->raw.family, frame->raw.ctors, frame->companions, frame->members,
                              frame->globals),
                  frame->catalog);
}

static Comp declare_nonempty_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  frame->reversed = vec_reverse(heap->arena, &frame->trusted);
  return bind_frame(heap,
                    family_elaborate_members(heap, &frame->elaborators, frame->symbolic, frame->catalog, frame->arity,
                                             &frame->reversed),
                    declare_nonempty_k4, frame);
}

static Comp declare_nonempty_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->trusted = *(const Vec *)value;
  return bind_frame(heap, family_add_members(heap, &frame->trusted, frame->catalog, frame->symbolic), declare_nonempty_k3,
                    frame);
}

static Comp declare_nonempty_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  return bind_frame(heap, family_scope_members(heap, &frame->trusted, frame->catalog, frame->arity), declare_nonempty_k2,
                    frame);
}

Comp family_declare_nonempty(Heap *heap, const Vec *families, const Vec *imports, const Vec *trusted, const Vec *members,
                             Globals globals, FamilyCatalog catalog, Nat arity) {
  VecGet head = vec_get(families, 0);
  Frame *frame;
  if (head.status != VEC_GET_OK)
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")));
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->raw = *(const FamilyRaw *)head.value;
  frame->companions = items_tail(heap, families);
  frame->imports = *imports;
  frame->trusted = *trusted;
  frame->elaborators = *members;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  return bind_frame(heap, family_install_symbolic(heap, &frame->imports, globals, catalog, arity), declare_nonempty_k1,
                    frame);
}

Comp family_declare_group_with_reuse(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, const Vec *imports,
                                     const Vec *trusted, const Vec *members) {
  Vec families = family_fresh_imports(heap, imports);
  return family_declare_nonempty(heap, &families, imports, trusted, members, globals, catalog, arity);
}

Vec family_fresh_raw(Heap *heap, const Vec *families) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < families->len; i = i + 1) {
    VecGet got = vec_get(families, i);
    if (got.status == VEC_GET_OK) {
      FamilyImported imported;
      imported.reused = false;
      imported.family = ((const FamilyRaw *)got.value)->family;
      imported.ctors = ((const FamilyRaw *)got.value)->ctors;
      items_push_box(heap, &out, &imported, sizeof imported);
    }
  }
  return out;
}

/* globals => Budget.pure(head): the context is the Decl * itself */
static Comp raw_callback(Heap *heap, void *ctx, Globals globals) {
  (void)heap;
  (void)globals;
  return budget_pure(ctx);
}

Vec family_raw_callbacks(Heap *heap, const Vec *members) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < members->len; i = i + 1) {
    VecGet got = vec_get(members, i);
    if (got.status == VEC_GET_OK) {
      FamilyMemberElaborator callback;
      callback.fn = raw_callback;
      callback.ctx = got.value;
      items_push_box(heap, &out, &callback, sizeof callback);
    }
  }
  return out;
}

FamilyMemberElaborator family_raw_elaborator(void *member) {
  FamilyMemberElaborator callback;
  callback.fn = raw_callback;
  callback.ctx = member;
  return callback;
}

Comp family_declare_group_elaborated_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity,
                                          const Vec *families, const Vec *members) {
  Vec imports = family_fresh_raw(heap, families);
  Vec trusted = vec_new(heap->arena);
  return family_declare_group_with_reuse(heap, globals, catalog, arity, &imports, &trusted, members);
}

/* ---- group member split (host only, no Bend counterpart; USER ruling 2026-10-12 (b)) ----
 * declare_group_elaborated_comp cut at the member boundary so the compact elaboration path can reset its
 * arena between the members of one `poly ... group ... end` entry: open = fresh_raw, fresh_imports,
 * install_symbolic, scope_members and add_members (the declare_nonempty prefix); member = one
 * elaborate_members step (tick, elaborate, map_member, check_members); close = the row of
 * declare_nonempty_k4. Under budget_unlimited the three runs give the Done value of
 * declare_group_elaborated_comp: the chain is cut only where a pure value crosses, and the member list is
 * kept in declaration order (trusted first) instead of reversed. */
static Comp group_open_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyGroupState *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return comp_oom();
  out->globals = frame->globals;
  out->symbolic = *(const Globals *)value;
  out->catalog = frame->catalog;
  out->arity = frame->arity;
  out->raw = frame->raw;
  out->companions = frame->companions;
  out->members = frame->trusted;
  return budget_pure(out);
}

static Comp group_open_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->trusted = *(const Vec *)value;
  return bind_frame(heap, family_add_members(heap, &frame->trusted, frame->catalog, frame->symbolic), group_open_k3,
                    frame);
}

static Comp group_open_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  return bind_frame(heap, family_scope_members(heap, &frame->trusted, frame->catalog, frame->arity), group_open_k2,
                    frame);
}

Comp family_group_open(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, const Vec *families) {
  Vec imports = family_fresh_raw(heap, families);
  Vec fresh = family_fresh_imports(heap, &imports);
  VecGet head = vec_get(&fresh, 0);
  Frame *frame;
  if (head.status != VEC_GET_OK)
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")));
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->raw = *(const FamilyRaw *)head.value;
  frame->companions = items_tail(heap, &fresh);
  frame->imports = imports;
  frame->trusted = vec_new(heap->arena);
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  return bind_frame(heap, family_install_symbolic(heap, &frame->imports, globals, catalog, arity), group_open_k1,
                    frame);
}

static Comp group_member_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyGroupState *out = heap_alloc(heap, sizeof *out);
  size_t i = 0;
  if (out == NULL) return comp_oom();
  *out = *frame->group;
  out->symbolic = *(const Globals *)value;
  out->members = vec_new(heap->arena);
  for (; i < frame->group->members.len; i = i + 1) {
    VecGet got = vec_get(&frame->group->members, i);
    if (got.status == VEC_GET_OK && !vec_push(&out->members, got.value)) return comp_oom();
  }
  if (!vec_push(&out->members, frame->member)) return comp_oom();
  return budget_pure(out);
}

static Comp group_member_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->member = value;
  frame->members = vec_new(heap->arena);
  if (!vec_push(&frame->members, frame->member)) return comp_oom();
  return bind_frame(heap,
                    family_check_members(heap, &frame->members, frame->group->catalog, frame->group->arity,
                                         frame->group->symbolic),
                    group_member_k3, frame);
}

static Comp group_member_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Decl *member = value;
  return bind_frame(heap,
                    family_map_member(heap, *member, scope_action(heap, frame->group->catalog, frame->group->arity)),
                    group_member_k2, frame);
}

static Comp group_member_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, frame->elaborator.fn(heap, frame->elaborator.ctx, frame->group->symbolic), group_member_k1,
                    frame);
}

Comp family_group_member(Heap *heap, const FamilyGroupState *group, FamilyMemberElaborator elaborator) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->group = group;
  frame->elaborator = elaborator;
  return bind_frame(heap, budget_tick(heap), group_member_k0, frame);
}

Comp family_group_close(Heap *heap, const FamilyGroupState *group) {
  return row_pure(heap, family_family_name(group->raw.family),
                  scheme_make(group->arity, group->raw.family, group->raw.ctors, group->companions, group->members,
                              group->globals),
                  group->catalog);
}

Result family_declare_group_elaborated(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity,
                                       const Vec *families, const Vec *members) {
  return budget_run(heap, family_declare_group_elaborated_comp(heap, globals, catalog, arity, families, members), budget);
}

Result family_declare_group(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity,
                            const Vec *families, const Vec *members) {
  Vec callbacks = family_raw_callbacks(heap, members);
  return family_declare_group_elaborated(heap, globals, catalog, budget, arity, families, &callbacks);
}

/* ---- chunk 3 (Bend lines 603-819): instance renaming, composition and closed instantiation ---- */

/* rename_bindings(names, scheme, source, alias, reuse, exports): Bindings as PolyRename pointers */
Vec family_rename_bindings(Heap *heap, const Vec *names, FamilyScheme scheme, Str source, Str alias, const Vec *reuse,
                           const Vec *exports) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OK) {
      Str name = *(const Str *)got.value;
      PolyRename rename;
      rename.source = name;
      rename.target = family_mapped_name(family_mapping_find(reuse, name),
                                         family_rename_export(heap, scheme, source, alias, exports, name));
      items_push_box(heap, &out, &rename, sizeof rename);
    }
  }
  return out;
}

PolyAction family_instance_action(Heap *heap, FamilyScheme scheme, Str source, Str alias, Vec levels, const Vec *reuse,
                                  const Vec *exports) {
  Vec raw = family_raw_families(heap, scheme);
  Vec companions = family_companion_names(heap, &raw);
  Vec members = family_schema_members(scheme);
  Vec member_names = family_member_names(heap, &members);
  Vec names = names_append(heap, &companions, &member_names);
  Vec with_source = names_cons(heap, source, &names);
  return poly_action(poly_substitute_levels(levels),
                     family_rename_bindings(heap, &with_source, scheme, source, alias, reuse, exports), names_new(heap));
}

FamilyImported family_mapped_import(FamilyRaw raw, bool reused) {
  FamilyImported imported;
  imported.reused = reused;
  imported.family = raw.family;
  imported.ctors = raw.ctors;
  return imported;
}

/* ---- map_imports: the Done value is a boxed Vec of FamilyImported pointers ---- */

static Comp map_imports_step(Heap *heap, Frame *frame);

static Comp map_imports_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const FamilyRaw *mapped = value;
  FamilyImported imported =
      family_mapped_import(*mapped, family_mapping_find(&frame->reuse, family_family_name(frame->raw.family)).some);
  items_push_box(heap, &frame->acc, &imported, sizeof imported);
  if (heap->oom) return comp_oom();
  frame->index = frame->index + 1;
  return map_imports_step(heap, frame);
}

static Comp map_imports_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_vec(heap, frame->acc);
  frame->raw = *(const FamilyRaw *)got.value;
  return bind_frame(heap, family_map_family(heap, frame->raw.family, &frame->raw.ctors, frame->action), map_imports_k,
                    frame);
}

Comp family_map_imports(Heap *heap, const Vec *families, PolyAction action, const Vec *reuse) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = families;
  frame->index = 0;
  frame->acc = vec_new(heap->arena);
  frame->action = action;
  frame->reuse = *reuse;
  return map_imports_step(heap, frame);
}

/* ---- check_schema: a public scheme is re-checked in its origin table ---- */

static Comp check_schema_k4(Heap *heap, void *ctx, void *value) {
  (void)heap;
  (void)ctx;
  (void)value;
  return pure_unit();
}

static Comp check_schema_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->members = *(const Vec *)value;
  return bind_frame(heap, family_check_members(heap, &frame->members, family_empty(), frame->arity, frame->symbolic),
                    check_schema_k4, frame);
}

static Comp check_schema_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  frame->members = family_schema_members(frame->scheme);
  return bind_frame(heap, family_scope_members(heap, &frame->members, family_empty(), frame->arity), check_schema_k3,
                    frame);
}

static Comp check_schema_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->imports = *(const Vec *)value;
  return bind_frame(heap,
                    family_install_symbolic(heap, &frame->imports, family_schema_table(frame->scheme), family_empty(),
                                            frame->arity),
                    check_schema_k2, frame);
}

Comp family_check_schema(Heap *heap, FamilyScheme scheme) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->scheme = scheme;
  frame->arity = family_schema_arity(scheme);
  frame->families = family_raw_families(heap, scheme);
  frame->reuse = names_new(heap);
  return bind_frame(heap,
                    family_map_imports(heap, &frame->families,
                                       poly_action(poly_scope_levels(frame->arity), names_new(heap), names_new(heap)),
                                       &frame->reuse),
                    check_schema_k1, frame);
}

/* check_schema up to scope_members (host only, no Bend counterpart; D-256a): the same map_imports,
 * install_symbolic and scope_members as check_schema_k0..k2; Done = boxed FamilySchemaOpen (the symbolic
 * table, the scoped members and the arity). The caller then runs family_check_member per member with the
 * empty catalog, threading the table: the check_members fold of check_schema_k3. */
static Comp check_schema_open_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilySchemaOpen open;
  open.symbolic = frame->symbolic;
  open.members = *(const Vec *)value;
  open.arity = frame->arity;
  return pure_box(heap, &open, sizeof open);
}

static Comp check_schema_open_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  frame->members = family_schema_members(frame->scheme);
  return bind_frame(heap, family_scope_members(heap, &frame->members, family_empty(), frame->arity),
                    check_schema_open_k3, frame);
}

static Comp check_schema_open_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->imports = *(const Vec *)value;
  return bind_frame(heap,
                    family_install_symbolic(heap, &frame->imports, family_schema_table(frame->scheme), family_empty(),
                                            frame->arity),
                    check_schema_open_k2, frame);
}

Comp family_check_schema_open(Heap *heap, FamilyScheme scheme) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->scheme = scheme;
  frame->arity = family_schema_arity(scheme);
  frame->families = family_raw_families(heap, scheme);
  frame->reuse = names_new(heap);
  return bind_frame(heap,
                    family_map_imports(heap, &frame->families,
                                       poly_action(poly_scope_levels(frame->arity), names_new(heap), names_new(heap)),
                                       &frame->reuse),
                    check_schema_open_k1, frame);
}

/* ---- universe arguments ---- */

bool family_scopes(Nat arity, const Vec *levels) {
  size_t i = 0;
  for (; i < levels->len; i = i + 1) {
    VecGet got = vec_get(levels, i);
    if (got.status == VEC_GET_OK && !level_in_scope(arity, got.value)) return false;
  }
  return true;
}

Error family_level_count_error(Heap *heap, Str source, Nat want, Nat got) {
  Arena *arena = heap->arena;
  Str text = str_cat(arena, str_lit("the family schema "), source);
  text = str_cat(arena, text, str_lit(" expects "));
  text = str_cat(arena, text, heap_nat_show(heap, want));
  text = str_cat(arena, text, str_lit(" universe arguments, got "));
  text = str_cat(arena, text, heap_nat_show(heap, got));
  return error_make(ERROR_UNIVERSE, text);
}

Comp family_check_levels(Heap *heap, const Vec *levels, Nat want, Str source, Nat arity, bool closed) {
  Nat got = nat_of_u64(heap->arena, levels->len);
  if (nat_cmp(want, got) != 0) return budget_lift(budget_result_fail(family_level_count_error(heap, source, want, got)));
  if (family_scopes(arity, levels)) return budget_lift(done_unit());
  if (closed) return budget_lift(fail_text(ERROR_UNIVERSE, str_lit("universe arguments must be closed")));
  return budget_lift(fail_text(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope")));
}

/* ---- collision_list: every generated name is fresh against the occupied list ---- */

static Comp collision_list_step(Heap *heap, Frame *frame);

static Comp collision_list_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->index = frame->index + 1;
  return collision_list_step(heap, frame);
}

static Comp collision_list_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  Str name;
  if (got.status != VEC_GET_OK) return pure_unit();
  name = *(const Str *)got.value;
  return bind_frame(heap, family_require_fresh(heap, poly_member(&frame->names, name), name), collision_list_k, frame);
}

Comp family_collision_list(Heap *heap, const Vec *names, const Vec *occupied) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = names;
  frame->index = 0;
  frame->names = *occupied;
  return collision_list_step(heap, frame);
}

/* ---- compose_dependency: one dependency joins the composed state; Done = boxed FamilyComposed * ---- */

static Comp compose_dependency_k9(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposed next;
  Vec generated_aliases = names_append(heap, &frame->generated, &frame->state.aliases);
  (void)value;
  next.aliases = names_cons(heap, frame->dependency.alias, &generated_aliases);
  next.available = names_append(heap, &frame->fresh, &frame->state.available);
  next.imports = names_append(heap, &frame->state.imports, &frame->imports);
  next.members = frame->renamed ? frame->state.members : names_append(heap, &frame->state.members, &frame->definitions);
  next.trusted = frame->renamed ? names_append(heap, &frame->state.trusted, &frame->definitions) : frame->state.trusted;
  if (heap->oom) return comp_oom();
  return pure_box(heap, &next, sizeof next);
}

/* checked <- collision_list(generated, aliases) */
static Comp compose_dependency_k8(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->renamed = level_distinct_vars(heap, &frame->dependency.levels) &&
                   global_extended_by(family_schema_table(frame->scheme), frame->globals);
  return bind_frame(heap, frame->renamed ? family_check_schema(heap, frame->scheme) : pure_unit(),
                    compose_dependency_k9, frame);
}

/* The k8 of family_compose_dependency_head (host only, D-256a): the same renamed test, then stop with the
 * frame instead of running the schema check; Done = boxed FamilyDependencyHead. */
static Comp compose_dependency_head_k8(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyDependencyHead head;
  (void)value;
  frame->renamed = level_distinct_vars(heap, &frame->dependency.levels) &&
                   global_extended_by(family_schema_table(frame->scheme), frame->globals);
  head.frame = frame;
  head.renamed = frame->renamed;
  head.scheme = frame->scheme;
  head.members = frame->members;
  head.action = frame->action;
  if (heap->oom) return comp_oom();
  return pure_box(heap, &head, sizeof head);
}

/* definitions <- map_members(schema_members(scheme), action) */
static Comp compose_dependency_k7(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Vec fresh_imports;
  Vec member_names;
  frame->definitions = *(const Vec *)value;
  fresh_imports = family_fresh_imports(heap, &frame->imports);
  frame->fresh = family_companion_names(heap, &fresh_imports);
  member_names = family_member_names(heap, &frame->definitions);
  frame->generated = names_append(heap, &frame->fresh, &member_names);
  return bind_frame(heap, family_collision_list(heap, &frame->generated, &frame->state.aliases),
                    frame->head_only ? compose_dependency_head_k8 : compose_dependency_k8, frame);
}

/* families <- map_imports(raw_families(scheme), action, reuse). The head of the split (host only, D-267a)
 * stops here with the frame, the schema members and the action instead of running map_members. */
static Comp compose_dependency_k6(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->imports = *(const Vec *)value;
  frame->members = family_schema_members(frame->scheme);
  if (frame->head_only) {
    FamilyDependencyHead head;
    head.frame = frame;
    head.renamed = false;
    head.scheme = frame->scheme;
    head.members = frame->members;
    head.action = frame->action;
    if (heap->oom) return comp_oom();
    return pure_box(heap, &head, sizeof head);
  }
  return bind_frame(heap, family_map_members(heap, &frame->members, frame->action), compose_dependency_k7, frame);
}

/* exports <- check_exports(scheme, source, alias, reuse, exports) */
static Comp compose_dependency_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->exports = *(const Vec *)value;
  frame->action = family_instance_action(heap, frame->scheme, frame->dependency.source, frame->dependency.alias,
                                         frame->dependency.levels, &frame->dependency.reuse, &frame->exports);
  frame->families = family_raw_families(heap, frame->scheme);
  return bind_frame(heap, family_map_imports(heap, &frame->families, frame->action, &frame->dependency.reuse),
                    compose_dependency_k6, frame);
}

/* checked <- check_bindings(reuse, companion_names(raw_families(scheme)), available, globals, []) */
static Comp compose_dependency_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_check_exports(heap, frame->scheme, frame->dependency.source, frame->dependency.alias,
                                         &frame->dependency.reuse, frame->dependency.exports),
                    compose_dependency_k5, frame);
}

/* checked <- check_levels(levels, schema_arity(scheme), source, arity, False) */
static Comp compose_dependency_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->families = family_raw_families(heap, frame->scheme);
  frame->names = family_companion_names(heap, &frame->families);
  frame->bound = names_new(heap);
  return bind_frame(heap,
                    family_check_bindings(heap, &frame->dependency.reuse, &frame->names, &frame->state.available,
                                          frame->globals, &frame->bound),
                    compose_dependency_k4, frame);
}

/* checked <- require_fresh(alias used, alias) */
static Comp compose_dependency_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_check_levels(heap, &frame->dependency.levels, family_schema_arity(frame->scheme),
                                        frame->dependency.source, frame->arity, false),
                    compose_dependency_k3, frame);
}

/* scheme <- require_schema(lookup(catalog, source), source) */
static Comp compose_dependency_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Str alias = frame->dependency.alias;
  bool used;
  frame->scheme = *(const FamilyScheme *)value;
  used = str_eq(alias, frame->root) || poly_member(&frame->state.aliases, alias) ||
         family_occupied(frame->globals, frame->catalog, alias);
  return bind_frame(heap, family_require_fresh(heap, used, alias), compose_dependency_k2, frame);
}

/* tick */
static Comp compose_dependency_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_require_schema(heap, family_lookup(frame->catalog, frame->dependency.source),
                                          frame->dependency.source),
                    compose_dependency_k1, frame);
}

Comp family_compose_dependency(Heap *heap, FamilyDependency dependency, FamilyComposed state, Globals globals,
                               FamilyCatalog catalog, Nat arity, Str root) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->dependency = dependency;
  frame->state = state;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = root;
  return bind_frame(heap, budget_tick(heap), compose_dependency_k0, frame);
}

/* ---- compose_dependency split (host only, no Bend counterpart; D-256a, D-267a, decision 156) ----
 * head = k0..k6 up to map_members; Done = boxed FamilyDependencyHead (the frame stays in the head's arena,
 * members and action = the map_members input). The compact path then maps each member with
 * family_map_member on its own scratch arena (D-267a: one map_members run held about 3.76 GB of poly frames
 * on MechHorizontalLaws, hop 267). mapped = k7 (the mapped definitions in order) and the k8 renamed test;
 * Done = boxed FamilyDependencyHead with renamed set. tail = the k9 body over that frame. Between mapped
 * and tail the compact path runs the schema re-check of a renamed line on scratch arenas
 * (family_check_schema_open, then family_check_member per member), so its garbage is freed member by member.
 * check_schema_k4 drops the check's value, so the check is a pure gate, and map_members maps each member on
 * its own and in order: under budget_unlimited the Done values and the error order equal
 * family_compose_dependency. */
Comp family_compose_dependency_head(Heap *heap, FamilyDependency dependency, FamilyComposed state, Globals globals,
                                    FamilyCatalog catalog, Nat arity, Str root) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->dependency = dependency;
  frame->state = state;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = root;
  frame->head_only = true;
  return bind_frame(heap, budget_tick(heap), compose_dependency_k0, frame);
}

Comp family_compose_dependency_mapped(Heap *heap, const FamilyDependencyHead *head, const Vec *mapped) {
  return compose_dependency_k7(heap, head->frame, (void *)mapped);
}

Comp family_compose_dependency_tail(Heap *heap, const FamilyDependencyHead *head) {
  return compose_dependency_k9(heap, head->frame, NULL);
}

/* ---- compose_dependencies: fold over FamilyDependency pointers; Done = boxed FamilyComposed * ---- */

static Comp compose_dependencies_step(Heap *heap, Frame *frame);

static Comp compose_dependencies_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->state = *(const FamilyComposed *)value;
  frame->index = frame->index + 1;
  return compose_dependencies_step(heap, frame);
}

static Comp compose_dependencies_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_box(heap, &frame->state, sizeof frame->state);
  return bind_frame(heap,
                    family_compose_dependency(heap, *(const FamilyDependency *)got.value, frame->state, frame->globals,
                                              frame->catalog, frame->arity, frame->root),
                    compose_dependencies_k, frame);
}

Comp family_compose_dependencies(Heap *heap, const Vec *dependencies, FamilyComposed state, Globals globals,
                                 FamilyCatalog catalog, Nat arity, Str root) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = dependencies;
  frame->index = 0;
  frame->state = state;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = root;
  return compose_dependencies_step(heap, frame);
}

/* ---- root guards: no member may take the root name ---- */

static Comp check_root_member_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return pure_box(heap, &frame->decl, sizeof frame->decl);
}

/* Done = boxed Decl * */
Comp family_check_root_member(Heap *heap, Decl decl, Str root) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->decl = decl;
  return bind_frame(heap, family_require_fresh(heap, str_eq(poly_decl_name(decl), root), root), check_root_member_k,
                    frame);
}

/* guarded_callback(elaborate, root): the closure over elaborate and root */
typedef struct {
  FamilyMemberElaborator inner;
  Str root;
} GuardedCallback;

static Comp guarded_callback_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return family_check_root_member(heap, *(const Decl *)value, frame->root);
}

static Comp guarded_callback_fn(Heap *heap, void *ctx, Globals symbolic) {
  const GuardedCallback *guard = ctx;
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->root = guard->root;
  return bind_frame(heap, guard->inner.fn(heap, guard->inner.ctx, symbolic), guarded_callback_k, frame);
}

/* the callback installed when the closure record could not be allocated */
static Comp oom_callback_fn(Heap *heap, void *ctx, Globals symbolic) {
  (void)heap;
  (void)ctx;
  (void)symbolic;
  return comp_oom();
}

FamilyMemberElaborator family_guarded_callback(Heap *heap, FamilyMemberElaborator elaborate, Str root) {
  FamilyMemberElaborator out;
  GuardedCallback guard;
  guard.inner = elaborate;
  guard.root = root;
  out.ctx = box(heap, &guard, sizeof guard);
  out.fn = out.ctx == NULL ? oom_callback_fn : guarded_callback_fn;
  if (out.ctx == NULL) heap->oom = true;
  return out;
}

static Comp guard_trusted_step(Heap *heap, Frame *frame);

static Comp guard_trusted_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->index = frame->index + 1;
  return guard_trusted_step(heap, frame);
}

static Comp guard_trusted_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_unit();
  return bind_frame(heap, family_check_root_member(heap, *(const Decl *)got.value, frame->root), guard_trusted_k, frame);
}

Comp family_guard_trusted(Heap *heap, const Vec *members, Str root) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = members;
  frame->index = 0;
  frame->root = root;
  return guard_trusted_step(heap, frame);
}

Vec family_guard_callbacks(Heap *heap, const Vec *members, Str root) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < members->len; i = i + 1) {
    VecGet got = vec_get(members, i);
    if (got.status == VEC_GET_OK) {
      FamilyMemberElaborator guarded =
          family_guarded_callback(heap, *(const FamilyMemberElaborator *)got.value, root);
      items_push_box(heap, &out, &guarded, sizeof guarded);
    }
  }
  return out;
}

/* ---- composition_schema .. compose ---- */

Comp family_composition_schema(Heap *heap, MaybeFamilyScheme scheme, Str name, FamilyCatalog catalog) {
  if (!scheme.some) return budget_failed(error_make(ERROR_CANNOT_INFER, str_lit("composition returned no schema")));
  return row_pure(heap, name, scheme.value, catalog);
}

static Comp finish_composition_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyCatalog checked = value;
  return family_composition_schema(heap, family_lookup(checked, family_family_name(frame->raw.family)), frame->name,
                                   frame->catalog);
}

Comp family_finish_composition(Heap *heap, const Vec *fresh, const Vec *imports, const Vec *trusted, const Vec *members,
                               Globals globals, FamilyCatalog catalog, Nat arity, Str name) {
  VecGet head = vec_get(fresh, 0);
  Frame *frame;
  if (head.status != VEC_GET_OK)
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")));
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->raw = *(const FamilyRaw *)head.value;
  frame->catalog = catalog;
  frame->name = name;
  return bind_frame(heap, family_declare_group_with_reuse(heap, globals, catalog, arity, imports, trusted, members),
                    finish_composition_k, frame);
}

Vec family_import_families(Heap *heap, const Vec *imports) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < imports->len; i = i + 1) {
    VecGet got = vec_get(imports, i);
    if (got.status == VEC_GET_OK) {
      FamilyRaw raw;
      raw.family = ((const FamilyImported *)got.value)->family;
      raw.ctors = ((const FamilyImported *)got.value)->ctors;
      items_push_box(heap, &out, &raw, sizeof raw);
    }
  }
  return out;
}

/* checked <- guard_trusted(trusted, root) */
static Comp composed_group_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  Vec raw = family_raw_callbacks(heap, &frame->state.members);
  Vec members = names_append(heap, &raw, &frame->supplied);
  (void)value;
  frame->elaborators = family_guard_callbacks(heap, &members, frame->root);
  frame->families = family_fresh_imports(heap, &frame->state.imports);
  return family_finish_composition(heap, &frame->families, &frame->state.imports, &frame->state.trusted,
                                   &frame->elaborators, frame->globals, frame->catalog, frame->arity, frame->root);
}

/* checked <- collision_list(companion_names(import_families(imports)), [root]) */
static Comp composed_group_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_guard_trusted(heap, &frame->state.trusted, frame->root), composed_group_k2, frame);
}

Comp family_composed_group(Heap *heap, FamilyComposed state, const Vec *supplied, Globals globals, FamilyCatalog catalog,
                           Nat arity, Str root) {
  Frame *frame = frame_new(heap);
  Vec imported;
  if (frame == NULL) return comp_oom();
  frame->state = state;
  frame->supplied = *supplied;
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = root;
  if (!frame_group(heap, frame, root)) return comp_oom();
  imported = family_import_families(heap, &frame->state.imports);
  frame->generated = family_companion_names(heap, &imported);
  return bind_frame(heap, family_collision_list(heap, &frame->generated, &frame->names), composed_group_k1, frame);
}

/* state <- compose_dependencies(dependencies, Composed{[], [], [], [], []}, ...) */
static Comp compose_comp_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  return family_composed_group(heap, *(const FamilyComposed *)value, &frame->supplied, frame->globals, frame->catalog,
                               frame->arity, frame->name);
}

/* checked <- require_fresh(occupied(globals, catalog, name), name) */
static Comp compose_comp_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposed empty;
  (void)value;
  empty.aliases = names_new(heap);
  empty.available = names_new(heap);
  empty.imports = names_new(heap);
  empty.members = names_new(heap);
  empty.trusted = names_new(heap);
  return bind_frame(heap,
                    family_compose_dependencies(heap, frame->items, empty, frame->globals, frame->catalog, frame->arity,
                                                frame->name),
                    compose_comp_k2, frame);
}

/* tick */
static Comp compose_comp_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->name), frame->name),
                    compose_comp_k1, frame);
}

Comp family_compose_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name, const Vec *dependencies,
                         const Vec *members) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->name = name;
  frame->items = dependencies;
  frame->supplied = *members;
  return bind_frame(heap, budget_tick(heap), compose_comp_k0, frame);
}

Result family_compose(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Nat arity, Str name,
                      const Vec *dependencies, const Vec *members) {
  return budget_run(heap, family_compose_comp(heap, globals, catalog, arity, name, dependencies, members), budget);
}

/* ---- composition member split (host only, no Bend counterpart; USER ruling 2026-10-12 (b)) ----
 * compose_comp cut at the member boundary so the compact elaboration path can reset its arena between
 * the specialize lines and the members of one `poly ... group X where specialize ...` entry: begin = tick,
 * require_fresh (Done = the dependencies with the empty Composed); one specialize line = family_compose_dependency
 * from the Composed value so far (decision 173); open = the composed_group guards (collision_list, guard_trusted)
 * and the declare_nonempty prefix of finish_composition (install_symbolic, scope_members, add_members over the
 * trusted members); member = one elaborate_members step through family_group_member with the callback
 * guarded against the root (guard_callbacks); close = the row of declare_nonempty_k4 followed by
 * finish_composition_k. Under budget_unlimited the runs give the Done value of compose_comp: the chain is
 * cut only where a pure value crosses, and the member order is raw_callbacks(Composed.members) then the
 * supplied definitions, as in composed_group_k2. */
static Comp compose_open_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposeState *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return comp_oom();
  out->group.globals = frame->globals;
  out->group.symbolic = *(const Globals *)value;
  out->group.catalog = frame->catalog;
  out->group.arity = frame->arity;
  out->group.raw = frame->raw;
  out->group.companions = frame->companions;
  out->group.members = frame->trusted;
  out->name = frame->root;
  out->pending = frame->state.members;
  return budget_pure(out);
}

/* trusted <- scope_members(trusted) */
static Comp compose_open_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->trusted = *(const Vec *)value;
  return bind_frame(heap, family_add_members(heap, &frame->trusted, frame->catalog, frame->symbolic), compose_open_k5,
                    frame);
}

/* symbolic <- install_symbolic(imports) */
static Comp compose_open_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->symbolic = *(const Globals *)value;
  return bind_frame(heap, family_scope_members(heap, &frame->trusted, frame->catalog, frame->arity), compose_open_k4,
                    frame);
}

/* finish_composition and declare_nonempty up to install_symbolic; next receives the symbolic table
 * (compose_open_k3, or compose_open_head_k3 on the split path of D-259a) */
static Comp compose_open_install(Heap *heap, Frame *frame, Comp (*next)(Heap *heap, void *ctx, void *value)) {
  VecGet head;
  frame->families = family_fresh_imports(heap, &frame->state.imports);
  head = vec_get(&frame->families, 0);
  if (head.status != VEC_GET_OK)
    return budget_failed(error_make(ERROR_MISMATCH, str_lit("a family schema group must be nonempty")));
  frame->raw = *(const FamilyRaw *)head.value;
  frame->companions = items_tail(heap, &frame->families);
  frame->imports = frame->state.imports;
  frame->trusted = frame->state.trusted;
  return bind_frame(heap, family_install_symbolic(heap, &frame->imports, frame->globals, frame->catalog, frame->arity),
                    next, frame);
}

/* checked <- guard_trusted(trusted, root): finish_composition and declare_nonempty up to install_symbolic */
static Comp compose_open_k2(Heap *heap, void *ctx, void *value) {
  (void)value;
  return compose_open_install(heap, ctx, compose_open_k3);
}

/* checked <- collision_list(companion_names(import_families(imports)), [root]) */
static Comp compose_open_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_guard_trusted(heap, &frame->state.trusted, frame->root), compose_open_k2, frame);
}

/* composed_group up to its guards, from the Composed value the specialize lines folded (frame->state);
 * next = compose_open_k1, or compose_open_head_k1 on the split path of D-259a */
static Comp compose_open_from(Heap *heap, Frame *frame, Comp (*next)(Heap *heap, void *ctx, void *value)) {
  Vec imported;
  if (!frame_group(heap, frame, frame->root)) return comp_oom();
  imported = family_import_families(heap, &frame->state.imports);
  frame->generated = family_companion_names(heap, &imported);
  return bind_frame(heap, family_collision_list(heap, &frame->generated, &frame->names), next, frame);
}

static Comp compose_open_frame(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                               const FamilyComposed *composed, Comp (*next)(Heap *heap, void *ctx, void *value)) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = name;
  frame->state = *composed;
  return compose_open_from(heap, frame, next);
}

Comp family_compose_open(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                         const FamilyComposed *composed) {
  return compose_open_frame(heap, globals, catalog, arity, name, composed, compose_open_k1);
}

/* ---- composition open split (host only, no Bend counterpart; D-259a, decision 156) ----
 * family_compose_open cut around scope_members, so the compact path can scope each trusted member on its
 * own scratch arena: on MechHorizontalAssociativity the one scope_members map over 752 trusted members
 * grew past the 3800 MB cap (hop 259). head = collision_list, guard_trusted, the nonempty guard and
 * install_symbolic (Done = a FamilyComposeState whose group.members are the trusted members NOT yet
 * scoped and whose group.symbolic is the install_symbolic table); each member is then scoped by
 * family_map_member with ONE family_scope_action, in order, as map_list does; tail = add_members over the
 * scoped members and the compose_open_k5 value. Under budget_unlimited the three runs give the Done value
 * and the error order of family_compose_open. */
static Comp compose_open_head_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposeState *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return comp_oom();
  out->group.globals = frame->globals;
  out->group.symbolic = *(const Globals *)value;
  out->group.catalog = frame->catalog;
  out->group.arity = frame->arity;
  out->group.raw = frame->raw;
  out->group.companions = frame->companions;
  out->group.members = frame->trusted;
  out->name = frame->root;
  out->pending = frame->state.members;
  return budget_pure(out);
}

static Comp compose_open_head_k2(Heap *heap, void *ctx, void *value) {
  (void)value;
  return compose_open_install(heap, ctx, compose_open_head_k3);
}

static Comp compose_open_head_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_guard_trusted(heap, &frame->state.trusted, frame->root), compose_open_head_k2, frame);
}

Comp family_compose_open_head(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                              const FamilyComposed *composed) {
  return compose_open_frame(heap, globals, catalog, arity, name, composed, compose_open_head_k1);
}

PolyAction family_scope_action(Heap *heap, FamilyCatalog catalog, Nat arity) {
  return scope_action(heap, catalog, arity);
}

static Comp compose_open_tail_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposeState *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return comp_oom();
  *out = *frame->compose;
  out->group.symbolic = *(const Globals *)value;
  out->group.members = frame->trusted;
  return budget_pure(out);
}

Comp family_compose_open_tail(Heap *heap, const FamilyComposeState *head, const Vec *scoped) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->compose = head;
  frame->trusted = *scoped;
  return bind_frame(heap, family_add_members(heap, &frame->trusted, head->group.catalog, head->group.symbolic),
                    compose_open_tail_k, frame);
}

/* checked <- require_fresh(occupied(globals, catalog, name), name): the Done value is the start state, the
 * dependencies with Composed{[], [], [], [], []}; the compact path folds family_compose_dependency over it one
 * specialize line at a time (decision 173), where compose_comp_k1 runs compose_dependencies in one go */
static Comp compose_begin_fresh(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposeStart *out = heap_alloc(heap, sizeof *out);
  (void)value;
  if (out == NULL) return comp_oom();
  out->dependencies = *frame->items;
  out->composed.aliases = names_new(heap);
  out->composed.available = names_new(heap);
  out->composed.imports = names_new(heap);
  out->composed.members = names_new(heap);
  out->composed.trusted = names_new(heap);
  return budget_pure(out);
}

/* tick */
static Comp compose_begin_tick(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->root), frame->root),
                    compose_begin_fresh, frame);
}

Comp family_compose_begin(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, Str name,
                          const Vec *dependencies) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->arity = arity;
  frame->root = name;
  frame->items = dependencies;
  return bind_frame(heap, budget_tick(heap), compose_begin_tick, frame);
}

static Comp compose_member_k(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyComposeState *out = heap_alloc(heap, sizeof *out);
  if (out == NULL) return comp_oom();
  out->group = *(const FamilyGroupState *)value;
  out->name = frame->compose->name;
  out->pending = frame->compose->pending;
  return budget_pure(out);
}

Comp family_compose_member(Heap *heap, const FamilyComposeState *state, FamilyMemberElaborator elaborator) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->compose = state;
  return bind_frame(heap,
                    family_group_member(heap, &state->group, family_guarded_callback(heap, elaborator, state->name)),
                    compose_member_k, frame);
}

Comp family_compose_close(Heap *heap, const FamilyComposeState *state) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->raw = state->group.raw;
  frame->catalog = state->group.catalog;
  frame->name = state->name;
  return bind_frame(heap, family_group_close(heap, &state->group), finish_composition_k, frame);
}

/* ---- normal levels: a closed level equal to zero + n for n < 64 is replaced by that literal ---- */

const LevelVar *family_normalize_from(Heap *heap, size_t remaining, const LevelVar *candidate, const LevelVar *level) {
  if (remaining == 0 || candidate == NULL) return level;
  if (level_equal(heap, candidate, level)) return candidate;
  return family_normalize_from(heap, remaining - 1, level_succ(heap, candidate), level);
}

const LevelVar *family_normal_level(Heap *heap, const LevelVar *level) {
  return family_normalize_from(heap, 64, level_zero(), level);
}

Vec family_normal_levels(Heap *heap, const Vec *levels) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < levels->len; i = i + 1) {
    VecGet got = vec_get(levels, i);
    if (got.status == VEC_GET_OK) items_push(heap, &out, (LevelVar *)family_normal_level(heap, got.value));
  }
  return out;
}

/* ---- install_import, install_imports: closed instances enter the table at arity 0 ---- */

static Comp install_import_closed_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  const Globals *provisional = value;
  Str name = family_family_name(frame->family);
  if (!frame_group(heap, frame, name)) return comp_oom();
  return check_define_ctors_comp(heap, *provisional, nat_zero(), &frame->names, name, frame->ctors);
}

static Comp install_import_closed_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, check_declare_family_comp(heap, frame->globals, nat_zero(), frame->family),
                    install_import_closed_k2, frame);
}

Comp family_install_import(Heap *heap, FamilyImported imported, Globals globals, FamilyCatalog catalog) {
  Frame *frame;
  Str name;
  if (imported.reused) return family_check_reuse(heap, globals, nat_zero(), imported.family, imported.ctors);
  frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->family = imported.family;
  frame->ctors = imported.ctors;
  name = family_family_name(imported.family);
  return bind_frame(heap, family_require_fresh(heap, family_occupied(globals, catalog, name), name),
                    install_import_closed_k1, frame);
}

static Comp install_imports_step(Heap *heap, Frame *frame);

static Comp install_imports_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = *(const Globals *)value;
  frame->index = frame->index + 1;
  return install_imports_step(heap, frame);
}

static Comp install_imports_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, family_install_import(heap, frame->imported, frame->globals, frame->catalog),
                    install_imports_k2, frame);
}

static Comp install_imports_step(Heap *heap, Frame *frame) {
  VecGet got = vec_get(frame->items, frame->index);
  if (got.status != VEC_GET_OK) return pure_globals(heap, frame->globals);
  frame->imported = *(const FamilyImported *)got.value;
  return bind_frame(heap, budget_tick(heap), install_imports_k1, frame);
}

Comp family_install_imports(Heap *heap, const Vec *imports, Globals globals, FamilyCatalog catalog) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->items = imports;
  frame->index = 0;
  frame->globals = globals;
  frame->catalog = catalog;
  return install_imports_step(heap, frame);
}

/* ---- instantiate: a closed instance of a scheme, every member kernel checked; Done = boxed Globals * ---- */

/* installed <- install_imports(families, globals, catalog) */
static Comp instantiate_k8(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->globals = *(const Globals *)value;
  return family_check_members(heap, &frame->members, frame->catalog, nat_zero(), frame->globals);
}

/* The end of the instantiate open tail (host only, D-264a): the installed table and the mapped members still
 * to check, for the compact path's per-member arena reset (USER ruling 2026-10-12 (e)). */
static Comp instantiate_begin_k8(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  FamilyInstantiateStart start;
  start.installed = *(const Globals *)value;
  start.members = frame->members;
  return pure_box(heap, &start, sizeof start);
}

/* members <- map_members(schema_members(scheme), action) */
static Comp instantiate_k7(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->members = *(const Vec *)value;
  return bind_frame(heap, family_install_imports(heap, &frame->imports, frame->globals, frame->catalog), instantiate_k8,
                    frame);
}

/* families <- map_imports(raw_families(scheme), action, reuse); the split head (D-264a) stops here with the
 * table, the catalog, the action, the mapped imports and the schema members still to map. */
static Comp instantiate_k6(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->imports = *(const Vec *)value;
  frame->members = family_schema_members(frame->scheme);
  if (frame->split) {
    FamilyInstantiateHead head;
    head.globals = frame->globals;
    head.catalog = frame->catalog;
    head.action = frame->action;
    head.imports = frame->imports;
    head.members = frame->members;
    return pure_box(heap, &head, sizeof head);
  }
  return bind_frame(heap, family_map_members(heap, &frame->members, frame->action), instantiate_k7, frame);
}

/* exports <- check_exports(scheme, name, as_name, reuse, exports) */
static Comp instantiate_k5(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->exports = *(const Vec *)value;
  frame->action = family_instance_action(heap, frame->scheme, frame->name, frame->as_name,
                                         family_normal_levels(heap, &frame->levels), &frame->reuse, &frame->exports);
  frame->families = family_raw_families(heap, frame->scheme);
  return bind_frame(heap, family_map_imports(heap, &frame->families, frame->action, &frame->reuse), instantiate_k6,
                    frame);
}

/* checked <- check_bindings(reuse, companion_names(raw_families(scheme)), [], globals, []) */
static Comp instantiate_k4(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_check_exports(heap, frame->scheme, frame->name, frame->as_name, &frame->reuse,
                                         frame->maybe_exports),
                    instantiate_k5, frame);
}

/* tick */
static Comp instantiate_k3(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  frame->families = family_raw_families(heap, frame->scheme);
  frame->names = family_companion_names(heap, &frame->families);
  frame->available = names_new(heap);
  frame->bound = names_new(heap);
  return bind_frame(heap,
                    family_check_bindings(heap, &frame->reuse, &frame->names, &frame->available, frame->globals,
                                          &frame->bound),
                    instantiate_k4, frame);
}

/* checked <- check_levels(levels, schema_arity(scheme), name, 0, True) */
static Comp instantiate_k2(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap, budget_tick(heap), instantiate_k3, frame);
}

/* checked <- require_fresh(occupied(globals, catalog, as_name), as_name) */
static Comp instantiate_k1(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  (void)value;
  return bind_frame(heap,
                    family_check_levels(heap, &frame->levels, family_schema_arity(frame->scheme), frame->name, nat_zero(),
                                        true),
                    instantiate_k2, frame);
}

/* scheme <- require_schema(lookup(catalog, name), name) */
static Comp instantiate_k0(Heap *heap, void *ctx, void *value) {
  Frame *frame = ctx;
  frame->scheme = *(const FamilyScheme *)value;
  return bind_frame(heap,
                    family_require_fresh(heap, family_occupied(frame->globals, frame->catalog, frame->as_name),
                                         frame->as_name),
                    instantiate_k1, frame);
}

static Comp instantiate_start(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, const Vec *levels,
                              Str as_name, const Vec *reuse, FamilyMaybeVec exports, bool split) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->globals = globals;
  frame->catalog = catalog;
  frame->name = name;
  frame->levels = *levels;
  frame->as_name = as_name;
  frame->reuse = *reuse;
  frame->maybe_exports = exports;
  frame->split = split;
  return bind_frame(heap, family_require_schema(heap, family_lookup(catalog, name), name), instantiate_k0, frame);
}

Comp family_instantiate_comp(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, const Vec *levels, Str as_name,
                             const Vec *reuse, FamilyMaybeVec exports) {
  return instantiate_start(heap, globals, catalog, name, levels, as_name, reuse, exports, false);
}

/* The instantiate open split (host only, D-264a): the head stops before map_members (instantiate_k6); the
 * compact path maps each schema member with family_map_member and head.action on its own arena, in order,
 * then runs the tail. map_members maps each member on its own, so under budget_unlimited the head, the
 * member maps and the tail give the Done value and the error order of instantiate up to install_imports. */
Comp family_instantiate_open_head(Heap *heap, Globals globals, FamilyCatalog catalog, Str name, const Vec *levels,
                                  Str as_name, const Vec *reuse, FamilyMaybeVec exports) {
  return instantiate_start(heap, globals, catalog, name, levels, as_name, reuse, exports, true);
}

/* install_imports(families, globals, catalog) over the head, then instantiate_begin_k8 with the mapped members. */
Comp family_instantiate_open_tail(Heap *heap, const FamilyInstantiateHead *head, const Vec *mapped) {
  Frame *frame = frame_new(heap);
  if (frame == NULL) return comp_oom();
  frame->members = *mapped;
  return bind_frame(heap, family_install_imports(heap, &head->imports, head->globals, head->catalog),
                    instantiate_begin_k8, frame);
}

Result family_instantiate(Heap *heap, Globals globals, FamilyCatalog catalog, Budget budget, Str name, const Vec *levels,
                          Str as_name, const Vec *reuse, FamilyMaybeVec exports) {
  return budget_run(heap, family_instantiate_comp(heap, globals, catalog, name, levels, as_name, reuse, exports), budget);
}
