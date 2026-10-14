/* Port of bend2/surface/elab.bend: the program pass. Chunk 1 (Bend lines 1-194): the run wrappers,
 * the name traversals, the mu group (declare headers, define families) and the rec group (types,
 * postulates, bodies, totality certificate, guarded or plain checking).
 * Lists: families are Vecs of SFam pointers, definitions Vecs of SRecDef pointers, names Vecs of
 * Str pointers (pointing into the syntax records, no copies), Rows = List<Common.Pair<String,
 * Global.Entry>> is a Vec of CheckRow pointers (check.c check_check_decls_comp), decls are Vecs of
 * Decl pointers, typed lists Vecs of ElabRecTyped pointers, body lists Vecs of ElabRecBody
 * pointers and totality members Vecs of OrderMember pointers {name, body} (order.c, totality.c).
 * Comp values: Globals * (declare_headers, define_families, elab_mu_group_comp), Vec * (rec_types,
 * rec_bodies, guarded_bodies, check_rec_bodies) and ElabChecked * (elab_rec_group_comp). */
#include <string.h>

#include "surface.h"

/* One frame per defunctionalized do block; each stepper reads only the fields it set. */
typedef struct {
  Globals globals;
  Globals provisional;
  const Vec *families;
  const Vec *definitions;
  const Vec *bodies_list;
  size_t index;
  Vec group;
  Vec out;
  const SFam *fam;
  const SRecDef *def;
  const ElabRecTyped *typed_head;
  const ElabRecBody *rec_body;
  Ctx ctx;
  const Value *expected;
  const Term *term;
  Vec typed;
  Vec bodies;
  Vec pairs;
  OrderCert cert;
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

/* A failed push marks the heap; the run's export boundary reports Error.HostOom (poly.c
 * export_result precedent). */
static bool push(Heap *heap, Vec *vec, void *item) {
  if (item == NULL || !vec_push(vec, item)) {
    heap->oom = true;
    return false;
  }
  return true;
}

static MaybeValue no_value(void) {
  MaybeValue none;
  none.some = false;
  none.value = NULL;
  return none;
}

static MaybeValue some_value(const Value *value) {
  MaybeValue some;
  some.some = true;
  some.value = value;
  return some;
}

/* Rows (25-26) and Checked (28-29) are ElabChecked in surface.h. */

/* elab (31-32) */
Result elab_elab(Heap *heap, Ctx ctx, MaybeValue expected, const STerm *syntax) {
  return budget_run(heap, elab_term_elab(heap, ctx, expected, syntax), check_context_budget(ctx));
}

/* elab_decl (34-35) */
Result elab_elab_decl(Heap *heap, const SDecl *decl, Ctx ctx) {
  return budget_run(heap, elab_term_elab_decl(heap, decl, ctx), check_context_budget(ctx));
}

/* elab_fam_decl (37-38) */
Result elab_elab_fam_decl(Heap *heap, SFam family, Ctx ctx) {
  return budget_run(heap, elab_term_elab_fam_decl(heap, family, ctx), check_context_budget(ctx));
}

/* elab_ctor_decl (40-41) */
Result elab_elab_ctor_decl(Heap *heap, SFamCtor ctor, Ctx ctx, Str name) {
  return budget_run(heap, elab_term_elab_ctor_decl(heap, ctor, ctx, name), check_context_budget(ctx));
}

/* context (43-44) */
Ctx elab_context(Heap *heap, Globals globals, Nat arity) {
  return check_make(heap, globals, budget_unlimited(), arity);
}

/* family_names (46-51) */
Vec elab_family_names(Heap *heap, const Vec *families) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < families->len; i = i + 1) {
    VecGet got = vec_get(families, i);
    if (got.status == VEC_GET_OK) {
      SFam *fam = got.value;
      push(heap, &out, &fam->name);
    }
  }
  return out;
}

/* rec_names (53-58) */
Vec elab_rec_names(Heap *heap, const Vec *definitions) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < definitions->len; i = i + 1) {
    VecGet got = vec_get(definitions, i);
    if (got.status == VEC_GET_OK) {
      SRecDef *def = got.value;
      push(heap, &out, &def->name);
    }
  }
  return out;
}

/* syntax_ctor_names (60-65) */
Vec elab_syntax_ctor_names(Heap *heap, const Vec *ctors) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < ctors->len; i = i + 1) {
    VecGet got = vec_get(ctors, i);
    if (got.status == VEC_GET_OK) {
      SFamCtor *ctor = got.value;
      push(heap, &out, &ctor->name);
    }
  }
  return out;
}

/* all_ctor_names (67-72) */
Vec elab_all_ctor_names(Heap *heap, const Vec *families) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < families->len; i = i + 1) {
    VecGet got = vec_get(families, i);
    if (got.status == VEC_GET_OK) {
      SFam *fam = got.value;
      size_t j = 0;
      for (; j < fam->ctors.len; j = j + 1) {
        VecGet ctor = vec_get(&fam->ctors, j);
        if (ctor.status == VEC_GET_OK) {
          SFamCtor *head = ctor.value;
          push(heap, &out, &head->name);
        }
      }
    }
  }
  return out;
}

/* declare_headers (74-82): the Comp value is a Globals *. */
static Comp declare_headers_step(Heap *heap, Frame *f);

static Comp declare_headers_after_declare(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->globals = *(const Globals *)value;
  f->index = f->index + 1;
  return declare_headers_step(heap, f);
}

static Comp declare_headers_after_family(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const FamilyDecl *family = value;
  return bind_frame(heap, check_declare_family_comp(heap, f->globals, nat_zero(), *family),
                    declare_headers_after_declare, f);
}

static Comp declare_headers_step(Heap *heap, Frame *f) {
  VecGet got = vec_get(f->families, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_box(heap, &f->globals, sizeof f->globals);
  f->fam = got.value;
  return bind_frame(heap, elab_term_elab_fam_decl(heap, *f->fam, elab_context(heap, f->globals, nat_zero())),
                    declare_headers_after_family, f);
}

Comp elab_declare_headers(Heap *heap, const Vec *families, Globals globals) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->families = families;
  f->globals = globals;
  return declare_headers_step(heap, f);
}

/* define_families (84-93): the Comp value is a Globals *. */
static Comp define_families_step(Heap *heap, Frame *f);

static Comp define_families_after_define(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->globals = *(const Globals *)value;
  f->index = f->index + 1;
  return define_families_step(heap, f);
}

static Comp define_families_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *ctors = value;
  return bind_frame(heap, check_define_ctors_comp(heap, f->globals, nat_zero(), &f->group, f->fam->name, *ctors),
                    define_families_after_define, f);
}

static Comp define_families_after_telescope(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return bind_frame(heap,
                    elab_term_elab_ctor_decls(heap, &f->fam->ctors, elab_term_telescope_context(value), f->fam->name),
                    define_families_after_ctors, f);
}

static Comp define_families_step(Heap *heap, Frame *f) {
  VecGet got = vec_get(f->families, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_box(heap, &f->globals, sizeof f->globals);
  f->fam = got.value;
  return bind_frame(heap,
                    elab_term_elab_telescope(heap, &f->fam->params, elab_context(heap, f->globals, nat_zero()),
                                             vec_new(heap->arena)),
                    define_families_after_telescope, f);
}

Comp elab_define_families(Heap *heap, const Vec *families, Globals globals, Vec group) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->families = families;
  f->globals = globals;
  f->group = group;
  return define_families_step(heap, f);
}

/* elab_mu_group_comp (95-98): the Comp value is a Globals *. */
static Comp mu_group_after_headers(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  return elab_define_families(heap, f->families, *(const Globals *)value, elab_family_names(heap, f->families));
}

Comp elab_elab_mu_group_comp(Heap *heap, Globals globals, const Vec *families) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->families = families;
  return bind_frame(heap, elab_declare_headers(heap, families, globals), mu_group_after_headers, f);
}

/* elab_mu_group (100-101): the Result value is a Globals *. */
Result elab_elab_mu_group(Heap *heap, Globals globals, Budget budget, const Vec *families) {
  return budget_run(heap, elab_elab_mu_group_comp(heap, globals, families), budget);
}

/* add_rows (103-108) */
Globals elab_add_rows(Heap *heap, const Vec *rows, Globals globals) {
  size_t i = 0;
  for (; i < rows->len; i = i + 1) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK) {
      const CheckRow *row = got.value;
      globals = global_add(heap, row->name, row->entry, globals);
    }
  }
  return globals;
}

/* RecTyped (110-111) and RecBody (113-114) are ElabRecTyped and ElabRecBody in surface.h. */

/* rec_types (116-124): the Comp value is a Vec * of ElabRecTyped pointers. */
static Comp rec_types_step(Heap *heap, Frame *f);

static Comp rec_types_after_ty(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ElabRecTyped typed;
  typed.name = f->def->name;
  typed.ty = value;
  typed.syntax = f->def->body;
  if (!push(heap, &f->out, box(heap, &typed, sizeof typed))) return comp_oom();
  f->index = f->index + 1;
  return rec_types_step(heap, f);
}

static Comp rec_types_step(Heap *heap, Frame *f) {
  VecGet got = vec_get(f->definitions, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_vec(heap, f->out);
  f->def = got.value;
  return bind_frame(heap, elab_term_elab(heap, elab_context(heap, f->globals, nat_zero()), no_value(), f->def->ty),
                    rec_types_after_ty, f);
}

Comp elab_rec_types(Heap *heap, const Vec *definitions, Globals globals) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->definitions = definitions;
  f->globals = globals;
  f->out = vec_new(heap->arena);
  return rec_types_step(heap, f);
}

/* rec_postulates (126-131): a Vec of Decl pointers. */
Vec elab_rec_postulates(Heap *heap, const Vec *definitions) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < definitions->len; i = i + 1) {
    VecGet got = vec_get(definitions, i);
    if (got.status == VEC_GET_OK) {
      const ElabRecTyped *typed = got.value;
      Decl decl;
      decl.name = typed->name;
      decl.kind = DECL_POSTULATE;
      decl.ty = typed->ty;
      decl.body.some = false;
      decl.body.value = NULL;
      push(heap, &out, box(heap, &decl, sizeof decl));
    }
  }
  return out;
}

/* rec_bodies (133-143): the Comp value is a Vec * of ElabRecBody pointers. */
static Comp rec_bodies_step(Heap *heap, Frame *f);

static Comp rec_bodies_after_body(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  ElabRecBody body;
  body.name = f->typed_head->name;
  body.ty = f->typed_head->ty;
  body.expected = f->expected;
  body.body = value;
  if (!push(heap, &f->out, box(heap, &body, sizeof body))) return comp_oom();
  f->index = f->index + 1;
  return rec_bodies_step(heap, f);
}

static Comp rec_bodies_after_eval(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->expected = value;
  return bind_frame(heap, elab_term_elab(heap, f->ctx, some_value(f->expected), f->typed_head->syntax),
                    rec_bodies_after_body, f);
}

static Comp rec_bodies_step(Heap *heap, Frame *f) {
  VecGet got = vec_get(f->definitions, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_vec(heap, f->out);
  f->typed_head = got.value;
  f->ctx = elab_context(heap, f->globals, nat_zero());
  return bind_frame(heap, elab_term_eval_in(heap, f->ctx, f->typed_head->ty), rec_bodies_after_eval, f);
}

Comp elab_rec_bodies(Heap *heap, const Vec *definitions, Globals globals) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->definitions = definitions;
  f->globals = globals;
  f->out = vec_new(heap->arena);
  return rec_bodies_step(heap, f);
}

/* rec_pairs (145-150): a Vec of OrderMember pointers {name, body}, the totality member list
 * (totality_guard_group_comp and order_certify read OrderMember, not Pair). */
Vec elab_rec_pairs(Heap *heap, const Vec *bodies) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < bodies->len; i = i + 1) {
    VecGet got = vec_get(bodies, i);
    if (got.status == VEC_GET_OK) {
      const ElabRecBody *body = got.value;
      OrderMember *member = order_member_new(heap, body->name, body->body);
      if (member != NULL) push(heap, &out, member);
    }
  }
  return out;
}

/* rec_declarations (152-157): a Vec of Decl pointers. */
Vec elab_rec_declarations(Heap *heap, const Vec *bodies) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < bodies->len; i = i + 1) {
    VecGet got = vec_get(bodies, i);
    if (got.status == VEC_GET_OK) {
      const ElabRecBody *body = got.value;
      Decl decl;
      decl.name = body->name;
      decl.kind = DECL_DEFINITION;
      decl.ty = body->ty;
      decl.body.some = true;
      decl.body.value = body->body;
      push(heap, &out, box(heap, &decl, sizeof decl));
    }
  }
  return out;
}

/* rec_arg (159-162) */
MaybeNat elab_rec_arg(Heap *heap, OrderCert cert, const Term *body) {
  MaybeNat out;
  out.some = order_mentions(heap, &cert.group, body);
  out.value = out.some ? cert.arg : nat_zero();
  return out;
}

/* guarded_bodies (164-173): the Comp value is a Vec * of CheckRow pointers. */
static Comp guarded_bodies_step(Heap *heap, Frame *f);

static Comp guarded_bodies_after_check(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  CheckRow row;
  (void)value;
  memset(&row, 0, sizeof row);
  row.name = f->rec_body->name;
  row.entry.kind = ENTRY_DEF;
  row.entry.def.ty = f->rec_body->ty;
  row.entry.def.body = f->term;
  row.entry.def.reducible = true;
  row.entry.def.rec_arg = elab_rec_arg(heap, f->cert, f->rec_body->body);
  row.entry.def.partial = false;
  if (!push(heap, &f->out, box(heap, &row, sizeof row))) return comp_oom();
  f->index = f->index + 1;
  return guarded_bodies_step(heap, f);
}

static Comp guarded_bodies_step(Heap *heap, Frame *f) {
  VecGet got = vec_get(f->bodies_list, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_vec(heap, f->out);
  f->rec_body = got.value;
  {
    /* Budget.lift(Order.translate(cert, body)) */
    OrderTermResult translated = order_translate(heap, f->cert, f->rec_body->body);
    switch (translated.kind) {
      case RESULT_FAIL: return budget_failed(translated.error);
      case RESULT_DONE: break;
    }
    f->term = translated.value;
  }
  return bind_frame(heap,
                    check_engine_check_uses(heap, elab_context(heap, f->globals, nat_zero()), QUANTITY_MANY, f->term,
                                            f->rec_body->expected),
                    guarded_bodies_after_check, f);
}

Comp elab_guarded_bodies(Heap *heap, const Vec *bodies, Globals globals, OrderCert cert) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->bodies_list = bodies;
  f->globals = globals;
  f->cert = cert;
  f->out = vec_new(heap->arena);
  return guarded_bodies_step(heap, f);
}

/* check_rec_bodies (175-180): the Comp value is a Vec * of CheckRow pointers. */
Comp elab_check_rec_bodies(Heap *heap, MaybeOrderCert cert, const Vec *bodies, Globals globals, Globals provisional) {
  if (cert.some) return elab_guarded_bodies(heap, bodies, provisional, cert.value);
  return check_check_decls_comp(heap, globals, elab_rec_declarations(heap, bodies));
}

/* elab_rec_group_comp (182-190): the Comp value is an ElabChecked *. */
static Comp rec_group_after_rows(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *rows = value;
  ElabChecked checked;
  checked.globals = elab_add_rows(heap, rows, f->globals);
  checked.rows = *rows;
  return pure_box(heap, &checked, sizeof checked);
}

static Comp rec_group_after_cert(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const MaybeOrderCert *cert = value;
  return bind_frame(heap, elab_check_rec_bodies(heap, *cert, &f->bodies, f->globals, f->provisional),
                    rec_group_after_rows, f);
}

static Comp rec_group_after_bodies(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->bodies = *(const Vec *)value;
  f->pairs = elab_rec_pairs(heap, &f->bodies);
  return bind_frame(heap, totality_guard_group_comp(heap, f->provisional, &f->pairs), rec_group_after_cert, f);
}

static Comp rec_group_after_declared(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  const Vec *declared = value;
  f->provisional = elab_add_rows(heap, declared, f->globals);
  return bind_frame(heap, elab_rec_bodies(heap, &f->typed, f->provisional), rec_group_after_bodies, f);
}

static Comp rec_group_after_typed(Heap *heap, void *ctx, void *value) {
  Frame *f = ctx;
  f->typed = *(const Vec *)value;
  return bind_frame(heap, check_check_decls_comp(heap, f->globals, elab_rec_postulates(heap, &f->typed)),
                    rec_group_after_declared, f);
}

Comp elab_elab_rec_group_comp(Heap *heap, Globals globals, const Vec *definitions) {
  Frame *f = frame_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  return bind_frame(heap, elab_rec_types(heap, definitions, globals), rec_group_after_typed, f);
}

/* elab_rec_group (192-193): the Result value is an ElabChecked *. */
Result elab_elab_rec_group(Heap *heap, Globals globals, Budget budget, const Vec *definitions) {
  return budget_run(heap, elab_elab_rec_group_comp(heap, globals, definitions), budget);
}

/* ---- chunk 2 (Bend 195-379): family templates, schema groups and compositions ----
 * Lists: dependencies are Vecs of FamilyDependency pointers, lowered levels Vecs of Level pointers,
 * member elaborators Vecs of FamilyMemberElaborator pointers, raw families Vecs of FamilyRaw
 * pointers. Comp values: ElabFamilyParts * (elab_family_parts), FamilyCatalog (declare_template,
 * elab_family_template, elab_family_group, elab_composition), Vec * (symbolic_families,
 * lower_levels, lower_dependencies) and kernel_unit() for every Unit Comp. */

/* FamilyParts (195-196) is ElabFamilyParts in surface.h. */

typedef struct {
  SFam fam;
  Globals globals;
  Globals provisional;
  Nat arity;
  FamilyDecl family;
  FamilyCatalog catalog;
  PolyCatalog definitions;
  const Vec *names;
  const Vec *items;
  const Vec *members;
  const FamilyDependency *dep;
  size_t index;
  Vec out;
  Vec store;
  Vec available;
  Vec occupied;
  Vec labels;
  Vec deps;
  Str root;
  Str name;
  bool split; /* composition member split (host only): hand over to family_compose_open after the labels */
} Frame2;

static Frame2 *frame2_new(Heap *heap) {
  Frame2 *frame = heap_alloc(heap, sizeof(Frame2));
  if (frame == NULL) return NULL;
  memset(frame, 0, sizeof(Frame2));
  return frame;
}

static Comp bind_ctx(Heap *heap, Comp first, Comp (*fn)(Heap *heap, void *ctx, void *value), void *ctx) {
  NextK next;
  if (ctx == NULL) return comp_oom();
  next.fn = fn;
  next.ctx = ctx;
  return budget_bind(heap, first, next);
}

static Comp pure_unit(void) { return budget_pure(kernel_unit()); }

/* names_extend appends the Str pointers of src to out (no copies). */
static void names_extend(Heap *heap, Vec *out, const Vec *src) {
  size_t i = 0;
  for (; i < src->len; i = i + 1) {
    VecGet got = vec_get(src, i);
    if (got.status == VEC_GET_OK) push(heap, out, got.value);
  }
}

static Vec names_append(Heap *heap, const Vec *a, const Vec *b) {
  Vec out = vec_new(heap->arena);
  names_extend(heap, &out, a);
  names_extend(heap, &out, b);
  return out;
}

static Vec names_one(Heap *heap, Str name) {
  Vec out = vec_new(heap->arena);
  push(heap, &out, box(heap, &name, sizeof name));
  return out;
}

static Vec *vec_box(Heap *heap, Vec vec) { return box(heap, &vec, sizeof vec); }

/* ctor_distinct (198-206): Budget.lift of a pure check per constructor, no ticks. */
Comp elab_ctor_distinct(Heap *heap, const Vec *ctors, Str family) {
  size_t i = 0;
  for (; i < ctors->len; i = i + 1) {
    VecGet got = vec_get(ctors, i);
    if (got.status == VEC_GET_OK) {
      const SFamCtor *ctor = got.value;
      if (str_eq(ctor->name, family)) {
        Str head = str_cat(heap->arena, str_lit("the constructor "), ctor->name);
        Str text = str_cat(heap->arena, str_cat(heap->arena, head, str_lit(" repeats the family name ")), family);
        return budget_failed(error_make(ERROR_MISMATCH, text));
      }
    }
  }
  return pure_unit();
}

/* elab_family_parts (208-217): the Comp value is an ElabFamilyParts *. */
static Comp family_parts_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  const Vec *ctors = value;
  ElabFamilyParts parts;
  parts.family = f->family;
  parts.ctors = *ctors;
  parts.provisional = f->provisional;
  return pure_box(heap, &parts, sizeof parts);
}

static Comp family_parts_after_telescope(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  return bind_ctx(heap, elab_term_elab_ctor_decls(heap, &f->fam.ctors, elab_term_telescope_context(value), f->fam.name),
                  family_parts_after_ctors, f);
}

static Comp family_parts_after_declare(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  f->provisional = *(const Globals *)value;
  return bind_ctx(heap,
                  elab_term_elab_telescope(heap, &f->fam.params, elab_context(heap, f->provisional, f->arity),
                                           vec_new(heap->arena)),
                  family_parts_after_telescope, f);
}

static Comp family_parts_after_family(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  f->family = *(const FamilyDecl *)value;
  return bind_ctx(heap, check_declare_family_comp(heap, f->globals, f->arity, f->family), family_parts_after_declare, f);
}

static Comp family_parts_after_distinct(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_term_elab_fam_decl(heap, f->fam, elab_context(heap, f->globals, f->arity)),
                  family_parts_after_family, f);
}

Comp elab_elab_family_parts(Heap *heap, SFam syntax, Globals globals, Nat arity) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->fam = syntax;
  f->globals = globals;
  f->arity = arity;
  return bind_ctx(heap, elab_ctor_distinct(heap, &f->fam.ctors, f->fam.name), family_parts_after_distinct, f);
}

/* declare_template (219-222): the Comp value is the new FamilyRow, a FamilyCatalog. */
Comp elab_declare_template(Heap *heap, const ElabFamilyParts *parts, Globals globals, FamilyCatalog catalog, Nat arity) {
  Vec *members = vec_box(heap, vec_new(heap->arena));
  if (members == NULL) return comp_oom();
  return family_declare_comp(heap, globals, catalog, arity, parts->family, parts->ctors, members);
}

/* elab_family_template (224-227) */
static Comp family_template_after_parts(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  return elab_declare_template(heap, value, f->globals, f->catalog, f->arity);
}

Comp elab_elab_family_template(Heap *heap, Globals globals, FamilyCatalog catalog, Nat arity, SFam family) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->catalog = catalog;
  f->arity = arity;
  return bind_ctx(heap, elab_elab_family_parts(heap, family, globals, arity), family_template_after_parts, f);
}

/* template_reservations (229-236) */
static Comp template_reservations_step(Heap *heap, Frame2 *f);

static Comp template_reservations_after_fresh(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  f->index = f->index + 1;
  return template_reservations_step(heap, f);
}

static Comp template_reservations_step(Heap *heap, Frame2 *f) {
  VecGet got = vec_get(f->names, f->index);
  Str name;
  bool used;
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_unit();
  name = *(const Str *)got.value;
  used = poly_arity(f->definitions, name).some || family_arity(f->catalog, name).some ||
         family_ctor_declared(f->globals, name);
  return bind_ctx(heap, family_require_fresh(heap, used, name), template_reservations_after_fresh, f);
}

Comp elab_template_reservations(Heap *heap, const Vec *names, Globals globals, PolyCatalog definitions,
                                FamilyCatalog catalog) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->names = names;
  f->globals = globals;
  f->definitions = definitions;
  f->catalog = catalog;
  return template_reservations_step(heap, f);
}

/* member_callback (238-245): ctx = {member, arity}; each run gets its own frame with the symbolic
 * globals. The Comp value is the Decl * of elab_term_elab_decl. */
typedef struct {
  const SRecDef *member;
  Nat arity;
} MemberCtx;

typedef struct {
  const SRecDef *member;
  Nat arity;
  Globals symbolic;
} MemberFrame;

static Comp member_after_fresh(Heap *heap, void *ctx, void *value) {
  MemberFrame *f = ctx;
  SDecl *decl = heap_alloc(heap, sizeof(SDecl));
  (void)value;
  if (decl == NULL) return comp_oom();
  memset(decl, 0, sizeof(SDecl));
  decl->kind = SDECL_DEF;
  decl->name = f->member->name;
  decl->ty = f->member->ty;
  decl->body = f->member->body;
  return elab_term_elab_decl(heap, decl, elab_context(heap, f->symbolic, f->arity));
}

static Comp member_elaborate(Heap *heap, void *ctx, Globals symbolic) {
  const MemberCtx *m = ctx;
  MemberFrame *f;
  bool used;
  if (m == NULL) return comp_oom();
  f = heap_alloc(heap, sizeof(MemberFrame));
  if (f == NULL) return comp_oom();
  f->member = m->member;
  f->arity = m->arity;
  f->symbolic = symbolic;
  used = poly_occupied(symbolic, poly_empty(), f->member->name) || family_ctor_declared(symbolic, f->member->name);
  return bind_ctx(heap, family_require_fresh(heap, used, f->member->name), member_after_fresh, f);
}

FamilyMemberElaborator elab_member_callback(Heap *heap, const SRecDef *member, Nat arity) {
  FamilyMemberElaborator out;
  MemberCtx m;
  m.member = member;
  m.arity = arity;
  out.fn = member_elaborate;
  out.ctx = box(heap, &m, sizeof m);
  if (out.ctx == NULL) heap->oom = true;
  return out;
}

/* member_callbacks (247-252) */
Vec elab_member_callbacks(Heap *heap, const Vec *members, Nat arity) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < members->len; i = i + 1) {
    VecGet got = vec_get(members, i);
    if (got.status == VEC_GET_OK) {
      FamilyMemberElaborator callback = elab_member_callback(heap, got.value, arity);
      push(heap, &out, box(heap, &callback, sizeof callback));
    }
  }
  return out;
}

/* symbolic_families (254-277): the Comp value is a Vec * of FamilyRaw pointers, in source order. */
static Comp symbolic_families_step(Heap *heap, Frame2 *f);

static Comp symbolic_family_after_define(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  f->globals = *(const Globals *)value;
  f->index = f->index + 1;
  return symbolic_families_step(heap, f);
}

/* symbolic_family_rest (260-267) */
static Comp symbolic_family_rest(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  const ElabFamilyParts *parts = value;
  Str name = family_family_name(parts->family);
  Vec *group = vec_box(heap, names_one(heap, name));
  FamilyRaw raw;
  raw.family = parts->family;
  raw.ctors = parts->ctors;
  if (group == NULL || !push(heap, &f->out, box(heap, &raw, sizeof raw))) return comp_oom();
  return bind_ctx(heap, check_define_ctors_comp(heap, parts->provisional, f->arity, group, name, parts->ctors),
                  symbolic_family_after_define, f);
}

static Comp symbolic_families_step(Heap *heap, Frame2 *f) {
  VecGet got = vec_get(f->items, f->index);
  const SFam *fam;
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_vec(heap, f->out);
  fam = got.value;
  return bind_ctx(heap, elab_elab_family_parts(heap, *fam, f->globals, f->arity), symbolic_family_rest, f);
}

Comp elab_symbolic_families(Heap *heap, const Vec *families, Globals globals, Nat arity) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->items = families;
  f->globals = globals;
  f->arity = arity;
  f->out = vec_new(heap->arena);
  return symbolic_families_step(heap, f);
}

/* elab_family_group (279-283): the Comp value is the new FamilyRow, a FamilyCatalog. */
static Comp family_group_after_families(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  Vec *callbacks = vec_box(heap, elab_member_callbacks(heap, f->members, f->arity));
  if (callbacks == NULL) return comp_oom();
  return family_declare_group_elaborated_comp(heap, f->globals, f->catalog, f->arity, value, callbacks);
}

static Comp family_group_after_reservations(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_symbolic_families(heap, f->items, f->globals, f->arity), family_group_after_families, f);
}

Comp elab_elab_family_group(Heap *heap, Globals globals, FamilyCatalog catalog, PolyCatalog definitions, Nat arity,
                            const Vec *group, const Vec *members) {
  Frame2 *f = frame2_new(heap);
  Vec family_names;
  Vec member_names;
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->catalog = catalog;
  f->definitions = definitions;
  f->arity = arity;
  f->items = group;
  f->members = members;
  family_names = elab_family_names(heap, group);
  member_names = elab_rec_names(heap, members);
  f->store = names_append(heap, &family_names, &member_names);
  return bind_ctx(heap, elab_template_reservations(heap, &f->store, globals, definitions, catalog),
                  family_group_after_reservations, f);
}

/* lower_levels (285-293) and lower_dependencies (295-303): Budget.lift chains of pure Results, so the
 * first failure wins and no tick is spent; run eagerly and lift once. */
typedef struct {
  bool ok;
  Vec items;
  Error error;
} Lowered;

static Lowered lowered_new(Heap *heap) {
  Lowered out;
  memset(&out, 0, sizeof out);
  out.ok = true;
  out.items = vec_new(heap->arena);
  return out;
}

static Lowered lower_levels_run(Heap *heap, const Vec *levels) {
  Lowered out = lowered_new(heap);
  size_t i = 0;
  for (; i < levels->len; i = i + 1) {
    VecGet got = vec_get(levels, i);
    if (got.status == VEC_GET_OK) {
      Result lowered = universe_lower(heap, got.value);
      switch (lowered.kind) {
        case RESULT_FAIL:
          out.ok = false;
          out.error = lowered.error;
          return out;
        case RESULT_DONE:
          push(heap, &out.items, lowered.value);
          break;
      }
    }
  }
  return out;
}

static Comp lowered_comp(Heap *heap, Lowered lowered) {
  if (lowered.ok) return pure_vec(heap, lowered.items);
  return budget_failed(lowered.error);
}

Comp elab_lower_levels(Heap *heap, const Vec *levels) { return lowered_comp(heap, lower_levels_run(heap, levels)); }

static Lowered lower_dependencies_run(Heap *heap, const Vec *dependencies) {
  Lowered out = lowered_new(heap);
  size_t i = 0;
  for (; i < dependencies->len; i = i + 1) {
    VecGet got = vec_get(dependencies, i);
    if (got.status == VEC_GET_OK) {
      const SSpecialization *spec = got.value;
      Lowered levels = lower_levels_run(heap, &spec->levels);
      FamilyDependency dep;
      if (!levels.ok) {
        out.ok = false;
        out.error = levels.error;
        return out;
      }
      dep.source = spec->template;
      dep.levels = levels.items;
      dep.alias = spec->name;
      dep.reuse = spec->names;
      dep.exports.some = spec->helpers.some;
      dep.exports.value = spec->helpers.value;
      push(heap, &out.items, box(heap, &dep, sizeof dep));
    }
  }
  return out;
}

Comp elab_lower_dependencies(Heap *heap, const Vec *dependencies) {
  return lowered_comp(heap, lower_dependencies_run(heap, dependencies));
}

/* planned_names (305-310) */
Vec elab_planned_names(Heap *heap, MaybeFamilyInstanceNames value) {
  if (!value.some) return vec_new(heap->arena);
  return names_append(heap, &value.value.families, &value.value.members);
}

/* planned_families (312-317) */
Vec elab_planned_families(Heap *heap, MaybeFamilyInstanceNames value) {
  if (!value.some) return vec_new(heap->arena);
  return value.value.families;
}

/* validate_dependency_exports (319-324) */
Comp elab_validate_dependency_exports(Heap *heap, FamilyMaybeVec exports, Globals globals, const Vec *available,
                                      FamilyCatalog catalog, Str source, Str alias, const Vec *reuse) {
  Vec *boxed;
  if (!exports.some) return pure_unit();
  boxed = vec_box(heap, exports.value);
  if (boxed == NULL) return comp_oom();
  return family_validate_exports_comp(heap, globals, available, catalog, source, alias, reuse, boxed);
}

/* validate_dependencies (326-334) */
static Comp validate_dependencies_step(Heap *heap, Frame2 *f);

static Comp validate_dependencies_after_exports(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  const FamilyDependency *dep = f->dep;
  Vec fresh;
  (void)value;
  fresh = elab_planned_families(
      heap, family_instance_names(heap, f->catalog, dep->source, dep->alias, &dep->reuse, dep->exports));
  f->available = names_append(heap, &fresh, &f->available);
  f->index = f->index + 1;
  return validate_dependencies_step(heap, f);
}

static Comp validate_dependencies_step(Heap *heap, Frame2 *f) {
  VecGet got = vec_get(f->items, f->index);
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_unit();
  f->dep = got.value;
  return bind_ctx(heap,
                  elab_validate_dependency_exports(heap, f->dep->exports, f->globals, &f->available, f->catalog,
                                                   f->dep->source, f->dep->alias, &f->dep->reuse),
                  validate_dependencies_after_exports, f);
}

Comp elab_validate_dependencies(Heap *heap, const Vec *dependencies, Globals globals, FamilyCatalog catalog,
                                Vec available) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->items = dependencies;
  f->globals = globals;
  f->catalog = catalog;
  f->available = available;
  return validate_dependencies_step(heap, f);
}

/* imported_names (336-341) */
Vec elab_imported_names(Heap *heap, const Vec *dependencies, FamilyCatalog catalog) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < dependencies->len; i = i + 1) {
    VecGet got = vec_get(dependencies, i);
    if (got.status == VEC_GET_OK) {
      FamilyDependency *dep = got.value;
      Vec planned = elab_planned_names(
          heap, family_instance_names(heap, catalog, dep->source, dep->alias, &dep->reuse, dep->exports));
      push(heap, &out, &dep->alias);
      names_extend(heap, &out, &planned);
    }
  }
  return out;
}

/* option_names (343-348) */
Vec elab_option_names(Heap *heap, FamilyMaybeVec value) {
  if (!value.some) return vec_new(heap->arena);
  return value.value;
}

/* imported_labels (350-355) */
Vec elab_imported_labels(Heap *heap, const Vec *dependencies, FamilyCatalog catalog) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < dependencies->len; i = i + 1) {
    VecGet got = vec_get(dependencies, i);
    if (got.status == VEC_GET_OK) {
      const FamilyDependency *dep = got.value;
      Vec labels = elab_option_names(heap, family_constructors(heap, catalog, dep->source));
      names_extend(heap, &out, &labels);
    }
  }
  return out;
}

/* label_reservations (357-367) */
static Comp label_reservations_step(Heap *heap, Frame2 *f);

static Comp label_reservations_after_fresh(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  f->index = f->index + 1;
  return label_reservations_step(heap, f);
}

static Comp label_reservations_step(Heap *heap, Frame2 *f) {
  VecGet got = vec_get(f->items, f->index);
  Str label;
  bool used;
  if (got.status == VEC_GET_OUT_OF_RANGE) return pure_unit();
  label = *(const Str *)got.value;
  used = str_eq(label, f->root) || poly_member(f->names, label) || poly_arity(f->definitions, label).some ||
         family_arity(f->catalog, label).some || global_find_family(label, f->globals).some ||
         (global_find(label, f->globals).some && !family_ctor_declared(f->globals, label));
  return bind_ctx(heap, family_require_fresh(heap, used, label), label_reservations_after_fresh, f);
}

Comp elab_label_reservations(Heap *heap, const Vec *labels, const Vec *names, Str root, Globals globals,
                             PolyCatalog definitions, FamilyCatalog catalog) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->items = labels;
  f->names = names;
  f->root = root;
  f->globals = globals;
  f->definitions = definitions;
  f->catalog = catalog;
  return label_reservations_step(heap, f);
}

/* elab_composition (369-378): the Comp value is the new FamilyRow, a FamilyCatalog. */
static Comp composition_after_labels(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  Vec *callbacks;
  (void)value;
  if (f->split) return family_compose_begin(heap, f->globals, f->catalog, f->arity, f->name, &f->deps);
  callbacks = vec_box(heap, elab_member_callbacks(heap, f->members, f->arity));
  if (callbacks == NULL) return comp_oom();
  return family_compose_comp(heap, f->globals, f->catalog, f->arity, f->name, &f->deps, callbacks);
}

static Comp composition_after_template(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  f->labels = elab_imported_labels(heap, &f->deps, f->catalog);
  return bind_ctx(heap,
                  elab_label_reservations(heap, &f->labels, &f->store, f->name, f->globals, f->definitions, f->catalog),
                  composition_after_labels, f);
}

static Comp composition_after_collision(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_template_reservations(heap, &f->store, f->globals, f->definitions, f->catalog),
                  composition_after_template, f);
}

static Comp composition_after_tick(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  f->occupied = names_one(heap, f->name);
  return bind_ctx(heap, family_collision_list(heap, &f->store, &f->occupied), composition_after_collision, f);
}

static Comp composition_after_validate(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  Vec imported;
  Vec recs;
  (void)value;
  imported = elab_imported_names(heap, &f->deps, f->catalog);
  recs = elab_rec_names(heap, f->members);
  f->store = names_append(heap, &imported, &recs);
  return bind_ctx(heap, budget_tick(heap), composition_after_tick, f);
}

static Comp composition_after_lower(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  f->deps = *(const Vec *)value;
  return bind_ctx(heap, elab_validate_dependencies(heap, &f->deps, f->globals, f->catalog, vec_new(heap->arena)),
                  composition_after_validate, f);
}

Comp elab_elab_composition(Heap *heap, Globals globals, FamilyCatalog catalog, PolyCatalog definitions, Nat arity,
                           Str name, const Vec *dependencies, const Vec *members) {
  Frame2 *f = frame2_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->catalog = catalog;
  f->definitions = definitions;
  f->arity = arity;
  f->name = name;
  f->members = members;
  return bind_ctx(heap, elab_lower_dependencies(heap, dependencies), composition_after_lower, f);
}

/* ---- chunk 3 (Bend 380-608): programs, declaration steps and the text entry points ----
 * Program{globals, definitions, families, reversed} is ElabProgram. Its rows Vec (CheckRow pointers) is
 * kept in declaration order: Bend's `row <> reversed` is a push at the end, `List.append(List.reverse(new),
 * reversed)` extends with new in order, and finish_program hands the Vec over as is. Comp values:
 * ElabProgram * (specialize_*, append_one, elab_declaration, program_step, program_loop), ElabChecked *
 * (elab_program_comp), Entry * (global_entry), Vec * (instance_members, instance_labels) and kernel_unit()
 * for the Unit Comps. Results: ElabChecked * (elab_program_in, check_in), Vec * of CheckRow pointers
 * (elab_program, check_text). */

typedef struct {
  const Vec *names;
  const Vec *decls;
  const Vec *reuse;
  const SDecl *decl;
  size_t index;
  Globals globals;
  Globals installed;
  PolyCatalog definitions;
  FamilyCatalog families;
  Vec rows;
  Vec out;
  Vec family_names;
  Vec members;
  Vec generated;
  Vec levels;
  Str source;
  Str alias;
  Str name;
  Nat arity;
  FamilyMaybeVec exports;
  ElabProgram state;
} Frame3;

static Frame3 *frame3_new(Heap *heap) {
  Frame3 *frame = heap_alloc(heap, sizeof(Frame3));
  if (frame == NULL) return NULL;
  memset(frame, 0, sizeof(Frame3));
  return frame;
}

static ElabProgram program_make(Globals globals, PolyCatalog definitions, FamilyCatalog families, Vec rows) {
  ElabProgram program;
  program.globals = globals;
  program.definitions = definitions;
  program.families = families;
  program.rows = rows;
  return program;
}

static Comp pure_program(Heap *heap, ElabProgram program) { return pure_box(heap, &program, sizeof program); }

/* rows_push copies rows and appends row (Bend `row <> reversed`). */
static Vec rows_push(Heap *heap, const Vec *rows, void *row) {
  Vec one = vec_new(heap->arena);
  push(heap, &one, row);
  return names_append(heap, rows, &one);
}

static CheckRow *row_new(Heap *heap, Str name, Entry entry) {
  CheckRow row;
  row.name = name;
  row.entry = entry;
  return box(heap, &row, sizeof row);
}

/* declared_names (383-402) */
Vec elab_declared_names(Heap *heap, const SDecl *decl) {
  Vec families;
  Vec ctors;
  switch (decl->kind) {
  case SDECL_DEF:
  case SDECL_AXIOM:
  case SDECL_POLY:
  case SDECL_POLY_COMPOSE:
    return names_one(heap, decl->name);
  case SDECL_SPECIALIZE:
    return names_one(heap, decl->specialization.name);
  case SDECL_POLY_MU:
  case SDECL_POLY_GROUP:
    return names_one(heap, decl->family.name);
  case SDECL_MU:
    families = elab_family_names(heap, &decl->families);
    ctors = elab_all_ctor_names(heap, &decl->families);
    return names_append(heap, &families, &ctors);
  case SDECL_REC:
    return elab_rec_names(heap, &decl->definitions);
  }
  return vec_new(heap->arena);
}

/* ctor_reserved (404-417) */
Vec elab_ctor_reserved(Heap *heap, const SDecl *decl) {
  switch (decl->kind) {
  case SDECL_POLY:
  case SDECL_POLY_COMPOSE:
    return names_one(heap, decl->name);
  case SDECL_SPECIALIZE:
    return names_one(heap, decl->specialization.name);
  case SDECL_POLY_MU:
  case SDECL_POLY_GROUP:
    return names_one(heap, decl->family.name);
  case SDECL_DEF:
  case SDECL_AXIOM:
  case SDECL_MU:
  case SDECL_REC:
    return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

/* reserve_catalogs (419-426) */
static Comp reserve_catalogs_step(Heap *heap, Frame3 *f);

static Comp reserve_catalogs_after_fresh(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  f->index = f->index + 1;
  return reserve_catalogs_step(heap, f);
}

static Comp reserve_catalogs_step(Heap *heap, Frame3 *f) {
  VecGet got = vec_get(f->names, f->index);
  Str name;
  bool used;
  if (got.status != VEC_GET_OK) return pure_unit();
  name = *(const Str *)got.value;
  used = poly_arity(f->definitions, name).some || family_arity(f->families, name).some;
  return bind_ctx(heap, family_require_fresh(heap, used, name), reserve_catalogs_after_fresh, f);
}

Comp elab_reserve_catalogs(Heap *heap, const Vec *names, PolyCatalog definitions, FamilyCatalog families) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->names = names;
  f->definitions = definitions;
  f->families = families;
  return reserve_catalogs_step(heap, f);
}

/* reserve_ctors (428-435) */
static Comp reserve_ctors_step(Heap *heap, Frame3 *f);

static Comp reserve_ctors_after_fresh(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  f->index = f->index + 1;
  return reserve_ctors_step(heap, f);
}

static Comp reserve_ctors_step(Heap *heap, Frame3 *f) {
  VecGet got = vec_get(f->names, f->index);
  Str name;
  if (got.status != VEC_GET_OK) return pure_unit();
  name = *(const Str *)got.value;
  return bind_ctx(heap, family_require_fresh(heap, family_ctor_declared(f->globals, name), name),
                  reserve_ctors_after_fresh, f);
}

Comp elab_reserve_ctors(Heap *heap, const Vec *names, Globals globals) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->names = names;
  f->globals = globals;
  return reserve_ctors_step(heap, f);
}

/* global_entry (437-442): the Done value is a boxed Entry *. */
Comp elab_global_entry(Heap *heap, MaybeEntry value, Str message) {
  if (!value.some) return budget_failed(error_make(ERROR_CANNOT_INFER, message));
  return pure_box(heap, &value.value, sizeof value.value);
}

/* instance_members (444-452): rows in names order. */
static Comp instance_members_step(Heap *heap, Frame3 *f);

static Comp instance_members_after_entry(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Entry *entry = value;
  CheckRow *row = row_new(heap, f->name, *entry);
  if (row == NULL) return comp_oom();
  if (!push(heap, &f->out, row)) return comp_oom();
  f->index = f->index + 1;
  return instance_members_step(heap, f);
}

static Comp instance_members_step(Heap *heap, Frame3 *f) {
  VecGet got = vec_get(f->names, f->index);
  if (got.status != VEC_GET_OK) return pure_vec(heap, f->out);
  f->name = *(const Str *)got.value;
  return bind_ctx(heap,
                  elab_global_entry(heap, global_find(f->name, f->installed),
                                    str_lit("specialization returned no member")),
                  instance_members_after_entry, f);
}

Comp elab_instance_members(Heap *heap, const Vec *names, Globals globals) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->names = names;
  f->installed = globals;
  f->out = vec_new(heap->arena);
  return instance_members_step(heap, f);
}

/* kernel_ctor_names (454-457) */
Vec elab_kernel_ctor_names(Heap *heap, Family family) { return check_ctor_names(heap, &family.ctors); }

/* instance_labels (459-467): constructor labels in names order. */
static Comp instance_labels_step(Heap *heap, Frame3 *f);

static Comp instance_labels_after_family(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Family *family = value;
  Vec labels = elab_kernel_ctor_names(heap, *family);
  names_extend(heap, &f->out, &labels);
  f->index = f->index + 1;
  return instance_labels_step(heap, f);
}

static Comp instance_labels_step(Heap *heap, Frame3 *f) {
  VecGet got = vec_get(f->names, f->index);
  Str name;
  if (got.status != VEC_GET_OK) return pure_vec(heap, f->out);
  name = *(const Str *)got.value;
  return bind_ctx(heap,
                  family_family_option(heap, global_find_family(name, f->installed),
                                       error_make(ERROR_CANNOT_INFER, str_lit("specialization returned no family"))),
                  instance_labels_after_family, f);
}

Comp elab_instance_labels(Heap *heap, const Vec *names, Globals globals) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->names = names;
  f->installed = globals;
  f->out = vec_new(heap->arena);
  return instance_labels_step(heap, f);
}

/* specialize_family_names (469-481) */
static Comp specialize_family_after_members(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Vec *member_rows = value;
  return pure_program(heap, program_make(f->installed, f->definitions, f->families,
                                         names_append(heap, &f->rows, member_rows)));
}

static Comp specialize_family_after_labels_checked(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_instance_members(heap, &f->members, f->installed), specialize_family_after_members, f);
}

static Comp specialize_family_after_labels(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Vec *labels = value;
  return bind_ctx(heap,
                  elab_label_reservations(heap, labels, &f->generated, f->alias, f->globals, f->definitions,
                                          f->families),
                  specialize_family_after_labels_checked, f);
}

static Comp specialize_family_after_reserved(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_instance_labels(heap, &f->family_names, f->installed), specialize_family_after_labels,
                  f);
}

Comp elab_specialize_family_names(Heap *heap, MaybeFamilyInstanceNames names, Globals installed, Globals globals,
                                  PolyCatalog definitions, FamilyCatalog families, Vec rows, Str source,
                                  Str alias) {
  Frame3 *f;
  if (!names.some) {
    return budget_failed(
        error_make(ERROR_UNBOUND, str_cat(heap->arena, str_lit("unknown family universe schema "), source)));
  }
  f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->installed = installed;
  f->globals = globals;
  f->definitions = definitions;
  f->families = families;
  f->rows = rows;
  f->source = source;
  f->alias = alias;
  f->family_names = names.value.families;
  f->members = names.value.members;
  f->generated = names_append(heap, &f->family_names, &f->members);
  f->out = names_one(heap, alias);
  names_extend(heap, &f->out, &f->generated);
  return bind_ctx(heap, elab_template_reservations(heap, &f->out, globals, definitions, family_empty()),
                  specialize_family_after_reserved, f);
}

/* definition_clauses (483-492): the four match arms in order. */
Comp elab_definition_clauses(Heap *heap, const Vec *reuse, FamilyMaybeVec exports, bool known) {
  (void)heap;
  if (reuse->len == 0 && !exports.some) return pure_unit();
  if (!known) return pure_unit();
  if (reuse->len > 0) {
    return budget_failed(error_make(ERROR_NOT_YET, str_lit("family reuse requires a family template")));
  }
  return budget_failed(error_make(ERROR_NOT_YET, str_lit("member exports require a family template")));
}

/* specialize_definition (494-499) */
static Comp specialize_definition_after_entry(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Entry *entry = value;
  CheckRow *row = row_new(heap, f->alias, *entry);
  if (row == NULL) return comp_oom();
  return pure_program(heap, program_make(f->globals, f->definitions, f->families, rows_push(heap, &f->rows, row)));
}

Comp elab_specialize_definition(Heap *heap, PolyInstance result, PolyCatalog definitions, FamilyCatalog families,
                                Vec rows, Str alias) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = result.globals;
  f->definitions = definitions;
  f->families = families;
  f->rows = rows;
  f->alias = alias;
  return bind_ctx(heap,
                  elab_global_entry(heap, global_find(alias, result.globals),
                                    str_lit("specialization returned no global entry")),
                  specialize_definition_after_entry, f);
}

/* specialize_kind (501-511) */
static Comp specialize_kind_after_installed(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Globals *installed = value;
  return elab_specialize_family_names(
      heap, family_instance_names(heap, f->families, f->source, f->alias, f->reuse, f->exports), *installed,
      f->globals, f->definitions, f->families, f->rows, f->source, f->alias);
}

static Comp specialize_kind_after_instance(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const PolyInstance *result = value;
  return elab_specialize_definition(heap, *result, f->definitions, f->families, f->rows, f->alias);
}

static Comp specialize_kind_after_clauses(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  return bind_ctx(heap, poly_instantiate_comp(heap, f->globals, f->definitions, f->source, f->levels, f->alias),
                  specialize_kind_after_instance, f);
}

Comp elab_specialize_kind(Heap *heap, bool is_family, Globals globals, PolyCatalog definitions,
                          FamilyCatalog families, Vec rows, Str source, const Vec *levels, Str alias,
                          const Vec *reuse, FamilyMaybeVec exports) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->globals = globals;
  f->definitions = definitions;
  f->families = families;
  f->rows = rows;
  f->source = source;
  f->alias = alias;
  f->reuse = reuse;
  f->exports = exports;
  f->levels = *levels;
  if (is_family) {
    return bind_ctx(heap, family_instantiate_comp(heap, globals, families, source, levels, alias, reuse, exports),
                    specialize_kind_after_installed, f);
  }
  return bind_ctx(heap, elab_definition_clauses(heap, reuse, exports, poly_arity(definitions, source).some),
                  specialize_kind_after_clauses, f);
}

/* append_checked (513-516) */
ElabProgram elab_append_checked(Heap *heap, ElabChecked result, PolyCatalog definitions, FamilyCatalog families,
                               Vec rows) {
  return program_make(result.globals, definitions, families, names_append(heap, &rows, &result.rows));
}

/* append_one (518-523) */
Comp elab_append_one(Heap *heap, const Vec *checked, Globals globals, PolyCatalog definitions,
                     FamilyCatalog families, Vec rows) {
  VecGet got = vec_get(checked, 0);
  const CheckRow *row;
  if (checked->len != 1 || got.status != VEC_GET_OK) {
    return budget_failed(
        error_make(ERROR_CANNOT_INFER, str_lit("the checker answered no entry for a declaration")));
  }
  row = got.value;
  return pure_program(heap, program_make(global_add(heap, row->name, row->entry, globals), definitions, families,
                                         rows_push(heap, &rows, got.value)));
}

/* elab_declaration (525-560) */
static Comp declaration_after_definitions(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  return pure_program(heap, program_make(f->globals, (PolyCatalog)value, f->families, f->rows));
}

static Comp declaration_after_raw_poly(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Decl *raw = value;
  return bind_ctx(heap, poly_declare_comp(heap, f->globals, f->definitions, f->arity, *raw),
                  declaration_after_definitions, f);
}

static Comp declaration_after_families(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  /* family_declare_comp, family_declare_group_elaborated_comp and family_compose_comp all end in row_pure, whose
   * Done value is the new FamilyRow itself (FamilyCatalog is `const FamilyRow *`), not a boxed catalog pointer. */
  return pure_program(heap, program_make(f->globals, f->definitions, (FamilyCatalog)value, f->rows));
}

static Comp declaration_after_levels(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Vec *levels = value;
  const SSpecialization *spec = &f->decl->specialization;
  FamilyMaybeVec exports;
  exports.some = spec->helpers.some;
  exports.value = spec->helpers.value;
  return elab_specialize_kind(heap, family_arity(f->families, spec->template).some, f->globals, f->definitions,
                              f->families, f->rows, spec->template, levels, spec->name, &spec->names, exports);
}

static Comp declaration_after_globals(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Globals *globals = value;
  return pure_program(heap, program_make(*globals, f->definitions, f->families, f->rows));
}

static Comp declaration_after_rec(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const ElabChecked *result = value;
  return pure_program(heap, elab_append_checked(heap, *result, f->definitions, f->families, f->rows));
}

static Comp declaration_after_checked(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  return elab_append_one(heap, value, f->globals, f->definitions, f->families, f->rows);
}

static Comp declaration_after_raw(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Vec decls = vec_new(heap->arena);
  if (!push(heap, &decls, value)) return comp_oom();
  return bind_ctx(heap, check_check_decls_comp(heap, f->globals, decls), declaration_after_checked, f);
}

/* [decl.family] ++ decl.families as a boxed Vec of SFam pointers (the POLY_GROUP head and companions); NULL on
 * allocation failure. */
static Vec *group_families_box(Heap *heap, const SDecl *decl) {
  SFam *head = box(heap, &decl->family, sizeof decl->family);
  Vec group;
  if (head == NULL) return NULL;
  group = vec_new(heap->arena);
  if (!push(heap, &group, head)) return NULL;
  names_extend(heap, &group, &decl->families);
  return vec_box(heap, group);
}

Comp elab_elab_declaration(Heap *heap, const SDecl *decl, Globals globals, PolyCatalog definitions,
                           FamilyCatalog families, Vec rows) {
  Frame3 *f = frame3_new(heap);
  SDecl def;
  SDecl *boxed;
  Vec *group_box;
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->globals = globals;
  f->definitions = definitions;
  f->families = families;
  f->rows = rows;
  f->arity = decl->universes;
  switch (decl->kind) {
  case SDECL_POLY:
    memset(&def, 0, sizeof def);
    def.kind = SDECL_DEF;
    def.name = decl->name;
    def.ty = decl->ty;
    def.body = decl->body;
    boxed = box(heap, &def, sizeof def);
    if (boxed == NULL) return comp_oom();
    return bind_ctx(heap, elab_term_elab_decl(heap, boxed, elab_context(heap, globals, decl->universes)),
                    declaration_after_raw_poly, f);
  case SDECL_POLY_MU:
    return bind_ctx(heap, elab_elab_family_template(heap, globals, families, decl->universes, decl->family),
                    declaration_after_families, f);
  case SDECL_POLY_GROUP:
    group_box = group_families_box(heap, decl);
    if (group_box == NULL) return comp_oom();
    return bind_ctx(heap,
                    elab_elab_family_group(heap, globals, families, definitions, decl->universes, group_box,
                                           &decl->helpers),
                    declaration_after_families, f);
  case SDECL_POLY_COMPOSE:
    return bind_ctx(heap,
                    elab_elab_composition(heap, globals, families, definitions, decl->universes, decl->name,
                                          &decl->specializations, &decl->helpers),
                    declaration_after_families, f);
  case SDECL_SPECIALIZE:
    return bind_ctx(heap, elab_lower_levels(heap, &decl->specialization.levels), declaration_after_levels, f);
  case SDECL_MU:
    return bind_ctx(heap, elab_elab_mu_group_comp(heap, globals, &decl->families), declaration_after_globals, f);
  case SDECL_REC:
    return bind_ctx(heap, elab_elab_rec_group_comp(heap, globals, &decl->definitions), declaration_after_rec, f);
  case SDECL_DEF:
  case SDECL_AXIOM:
    return bind_ctx(heap, elab_term_elab_decl(heap, decl, elab_context(heap, globals, nat_zero())),
                    declaration_after_raw, f);
  }
  return comp_oom();
}

/* program_step (562-568) */
static Comp program_step_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  return elab_elab_declaration(heap, f->decl, f->globals, f->definitions, f->families, f->rows);
}

static Comp program_step_after_catalogs(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Vec *names;
  (void)value;
  names = vec_box(heap, elab_ctor_reserved(heap, f->decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_ctors(heap, names, f->globals), program_step_after_ctors, f);
}

Comp elab_program_step(Heap *heap, ElabProgram state, const SDecl *decl) {
  Frame3 *f = frame3_new(heap);
  Vec *names;
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->globals = state.globals;
  f->definitions = state.definitions;
  f->families = state.families;
  f->rows = state.rows;
  names = vec_box(heap, elab_declared_names(heap, decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_catalogs(heap, names, state.definitions, state.families),
                  program_step_after_catalogs, f);
}

/* program_loop (570-577) */
static Comp program_loop_step(Heap *heap, Frame3 *f);

static Comp program_loop_after_step(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const ElabProgram *state = value;
  f->state = *state;
  f->index = f->index + 1;
  return program_loop_step(heap, f);
}

static Comp program_loop_step(Heap *heap, Frame3 *f) {
  VecGet got = vec_get(f->decls, f->index);
  if (got.status != VEC_GET_OK) return pure_program(heap, f->state);
  return bind_ctx(heap, elab_program_step(heap, f->state, got.value), program_loop_after_step, f);
}

Comp elab_program_loop(Heap *heap, const Vec *decls, ElabProgram state) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  f->decls = decls;
  f->state = state;
  return program_loop_step(heap, f);
}

/* finish_program (579-582): rows are already in declaration order. */
ElabChecked elab_finish_program(ElabProgram state) {
  ElabChecked checked;
  checked.globals = state.globals;
  checked.rows = state.rows;
  return checked;
}

/* elab_program_comp (584-587) */
static Comp program_comp_after_loop(Heap *heap, void *ctx, void *value) {
  const ElabProgram *state = value;
  ElabChecked checked = elab_finish_program(*state);
  (void)ctx;
  return pure_box(heap, &checked, sizeof checked);
}

Comp elab_elab_program_comp(Heap *heap, Globals globals, const Vec *decls) {
  Frame3 *f = frame3_new(heap);
  if (f == NULL) return comp_oom();
  return bind_ctx(heap,
                  elab_program_loop(heap, decls,
                                    program_make(globals, poly_empty(), family_empty(), vec_new(heap->arena))),
                  program_comp_after_loop, f);
}

/* elab_program_in (589-590): the Result value is an ElabChecked *. */
Result elab_elab_program_in(Heap *heap, Globals globals, Budget budget, const Vec *decls) {
  return budget_run(heap, elab_elab_program_comp(heap, globals, decls), budget);
}

/* result_rows (592-597): Done carries a Vec * of CheckRow pointers. */
Result elab_result_rows(Result result) {
  ElabChecked *checked;
  switch (result.kind) {
  case RESULT_FAIL:
    return result;
  case RESULT_DONE:
    checked = result.value;
    return budget_result_done(&checked->rows);
  }
  return result;
}

/* elab_program (599-600) */
Result elab_elab_program(Heap *heap, Globals globals, Budget budget, const Vec *decls) {
  return elab_result_rows(elab_elab_program_in(heap, globals, budget, decls));
}

/* check_in (602-605): Parser.program then elab_program_in. */
Result elab_check_in(Heap *heap, Globals globals, Budget budget, Str source) {
  Result parsed = parser_program(heap, source);
  switch (parsed.kind) {
  case RESULT_FAIL:
    return parsed;
  case RESULT_DONE:
    return elab_elab_program_in(heap, globals, budget, parsed.value);
  }
  return parsed;
}

/* check_in_compact (host only, no Bend counterpart; decision 156): check_in with one fresh arena per
 * declaration. The syntax stays in the caller's arena; each declaration step runs on a heap over its
 * own arena, the surviving program state is deep-copied into a new arena (ping-pong) and the step's
 * arena is freed, so the per-declaration garbage of the checker never accumulates (the Bend bundle
 * reclaimed it by garbage collection). Only the prelude drivers call it. The Done value is an
 * ElabChecked * in the caller's arena, as for check_in; a failure is copied into the caller's arena. */
#define COMPACT_CHUNK (1 << 20)

static Result compact_host_oom(Heap *heap, Arena *live) {
  arena_free(live);
  heap->oom = true;
  return budget_result_fail(error_host_oom());
}

static Result compact_failure(Heap *heap, Arena *live, Error error) {
  Error copied = compact_error(heap, error);
  arena_free(live);
  return budget_result_fail(copied);
}

static Result compact_finish(Heap *heap, Arena *live, ElabProgram state) {
  ElabChecked checked = elab_finish_program(compact_program(heap, &state));
  void *boxed;
  arena_free(live);
  if (heap->oom) return budget_result_fail(error_host_oom());
  boxed = box(heap, &checked, sizeof checked);
  if (boxed == NULL) return budget_result_fail(error_host_oom());
  return budget_result_done(boxed);
}

/* ---- group member split (host only; USER ruling 2026-10-12 (b)) ----
 * A `poly ... group ... end` entry of the compact path runs as open, one step per member and close, each
 * in its own arena with compact_group_swap between them. open = the two elab_program_step reservations,
 * the template reservations, the symbolic families and family_group_open; member = family_group_member
 * with the member's elab_member_callback; close = family_group_close and the program_make of
 * declaration_after_families. Under budget_unlimited the Done values equal those of elab_program_step
 * (decision 156: the chain is cut at pure data only, and the copies are deep). */
typedef struct {
  ElabProgram state;
  FamilyGroupState group;
} CompactGroup;

static Comp group_open_after_families(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  return family_group_open(heap, f->globals, f->catalog, f->arity, value);
}

static Comp group_open_after_reservations(Heap *heap, void *ctx, void *value) {
  Frame2 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_symbolic_families(heap, f->items, f->globals, f->arity), group_open_after_families, f);
}

static Comp group_open_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Frame2 *g = frame2_new(heap);
  Vec *group = group_families_box(heap, f->decl);
  Vec family_names;
  Vec member_names;
  (void)value;
  if (g == NULL || group == NULL) return comp_oom();
  g->globals = f->globals;
  g->catalog = f->families;
  g->definitions = f->definitions;
  g->arity = f->decl->universes;
  g->items = group;
  g->members = &f->decl->helpers;
  family_names = elab_family_names(heap, group);
  member_names = elab_rec_names(heap, &f->decl->helpers);
  g->store = names_append(heap, &family_names, &member_names);
  return bind_ctx(heap, elab_template_reservations(heap, &g->store, g->globals, g->definitions, g->catalog),
                  group_open_after_reservations, g);
}

static Comp group_open_after_catalogs(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Vec *names;
  (void)value;
  names = vec_box(heap, elab_ctor_reserved(heap, f->decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_ctors(heap, names, f->globals), group_open_after_ctors, f);
}

/* elab_program_step for a POLY_GROUP decl up to the first member: the Done value is a FamilyGroupState *. */
static Comp group_open_comp(Heap *heap, ElabProgram state, const SDecl *decl) {
  Frame3 *f = frame3_new(heap);
  Vec *names;
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->globals = state.globals;
  f->definitions = state.definitions;
  f->families = state.families;
  f->rows = state.rows;
  names = vec_box(heap, elab_declared_names(heap, decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_catalogs(heap, names, state.definitions, state.families),
                  group_open_after_catalogs, f);
}

/* Copies the survivors of a group step into a fresh arena and frees the old one. The origin table and the
 * catalog are shared between the program state and the group state, so they are copied once. */
static bool compact_group_swap(Arena *live, CompactGroup *cg) {
  Arena next = arena_new(COMPACT_CHUNK);
  Heap dst = heap_new(&next);
  bool memo = compact_memo_open(&dst);
  CompactGroup copied;
  copied.group = compact_family_group_state(&dst, &cg->group);
  copied.state.globals = copied.group.globals;
  copied.state.families = copied.group.catalog;
  copied.state.definitions = compact_poly_catalog(&dst, cg->state.definitions);
  copied.state.rows = compact_check_rows(&dst, &cg->state.rows);
  compact_memo_close(&dst, memo);
  arena_free(live);
  *live = next;
  *cg = copied;
  return !dst.oom;
}

/* One POLY_GROUP entry at member granularity. Done = *state holds the stepped program (not yet compacted,
 * like the Done value of elab_program_step); a failure has already freed the live arena. */
static Result compact_group_entry(Heap *heap, Arena *live, ElabProgram *state, const SDecl *decl) {
  CompactGroup cg;
  Heap work = heap_new(live);
  Result run = budget_run(&work, group_open_comp(&work, *state, decl), budget_unlimited());
  size_t i = 0;
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  cg.state = *state;
  cg.group = *(const FamilyGroupState *)run.value;
  if (!compact_group_swap(live, &cg)) return compact_host_oom(heap, live);
  for (;;) {
    VecGet got = vec_get(&decl->helpers, i);
    FamilyMemberElaborator callback;
    if (got.status != VEC_GET_OK) break;
    work = heap_new(live);
    callback = elab_member_callback(&work, got.value, decl->universes);
    run = budget_run(&work, family_group_member(&work, &cg.group, callback), budget_unlimited());
    if (work.oom) return compact_host_oom(heap, live);
    switch (run.kind) {
    case RESULT_FAIL:
      return compact_failure(heap, live, run.error);
    case RESULT_DONE:
      break;
    }
    cg.group = *(const FamilyGroupState *)run.value;
    if (!compact_group_swap(live, &cg)) return compact_host_oom(heap, live);
    i = i + 1;
  }
  work = heap_new(live);
  run = budget_run(&work, family_group_close(&work, &cg.group), budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  *state = program_make(cg.group.globals, cg.state.definitions, (FamilyCatalog)run.value, cg.state.rows);
  return budget_result_done(NULL);
}

/* ---- composition member split (host only; USER ruling 2026-10-12 (b)) ----
 * A `poly ... group X where specialize ...` entry (SDECL_POLY_COMPOSE) of the compact path runs as begin,
 * one step per specialize line, open, one step per member and close, each in its own arena with
 * compact_compose_swap_start or compact_compose_swap between them (decision 173). begin = the two
 * elab_program_step reservations, the elab_composition chain up to the label reservations and
 * family_compose_begin (tick, require_fresh); specialize line = family_compose_dependency from the Composed
 * value so far; open = family_compose_open (guards, the trusted members); member = family_compose_member
 * with the raw elaborator of one pending specialization member, then with each supplied definition's
 * elab_member_callback (the composed_group_k2 order); close = family_compose_close and the program_make
 * of declaration_after_families. */
typedef struct {
  ElabProgram state;
  FamilyComposeStart start;
  FamilyComposeState compose;
} CompactCompose;

static Comp compose_open_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Frame2 *g = frame2_new(heap);
  (void)value;
  if (g == NULL) return comp_oom();
  g->globals = f->globals;
  g->catalog = f->families;
  g->definitions = f->definitions;
  g->arity = f->decl->universes;
  g->name = f->decl->name;
  g->members = &f->decl->helpers;
  g->split = true;
  return bind_ctx(heap, elab_lower_dependencies(heap, &f->decl->specializations), composition_after_lower, g);
}

static Comp compose_open_after_catalogs(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Vec *names;
  (void)value;
  names = vec_box(heap, elab_ctor_reserved(heap, f->decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_ctors(heap, names, f->globals), compose_open_after_ctors, f);
}

/* elab_program_step for a POLY_COMPOSE decl up to the first specialize line (the reservations, the lowered
 * dependencies, tick and require_fresh): the Done value is a FamilyComposeStart *. */
static Comp compose_open_comp(Heap *heap, ElabProgram state, const SDecl *decl) {
  Frame3 *f = frame3_new(heap);
  Vec *names;
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->globals = state.globals;
  f->definitions = state.definitions;
  f->families = state.families;
  f->rows = state.rows;
  names = vec_box(heap, elab_declared_names(heap, decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_catalogs(heap, names, state.definitions, state.families),
                  compose_open_after_catalogs, f);
}

static bool compact_compose_swap(Arena *live, CompactCompose *cc) {
  Arena next = arena_new(COMPACT_CHUNK);
  Heap dst = heap_new(&next);
  bool memo = compact_memo_open(&dst);
  CompactCompose copied;
  copied.compose = compact_family_compose_state(&dst, &cc->compose);
  copied.state.globals = copied.compose.group.globals;
  copied.state.families = copied.compose.group.catalog;
  copied.state.definitions = compact_poly_catalog(&dst, cc->state.definitions);
  copied.state.rows = compact_check_rows(&dst, &cc->state.rows);
  compact_memo_close(&dst, memo);
  arena_free(live);
  *live = next;
  *cc = copied;
  return !dst.oom;
}

/* The survivor between two specialize lines (decision 173): the start state, the program tables and the
 * rows; cc->compose is not in use yet. */
static bool compact_compose_swap_start(Arena *live, CompactCompose *cc) {
  Arena next = arena_new(COMPACT_CHUNK);
  Heap dst = heap_new(&next);
  bool memo = compact_memo_open(&dst);
  FamilyComposeStart start = compact_family_compose_start(&dst, &cc->start);
  ElabProgram state;
  state.globals = compact_globals(&dst, cc->state.globals);
  state.families = compact_family_catalog(&dst, cc->state.families);
  state.definitions = compact_poly_catalog(&dst, cc->state.definitions);
  state.rows = compact_check_rows(&dst, &cc->state.rows);
  compact_memo_close(&dst, memo);
  arena_free(live);
  *live = next;
  cc->start = start;
  cc->state = state;
  return !dst.oom;
}

/* One specialize line of a composition entry; Done = cc->start.composed holds the folded and compacted value. */
static Result compact_compose_dependency(Heap *heap, Arena *live, CompactCompose *cc, FamilyDependency dependency,
                                         const SDecl *decl, Heap *work) {
  Result run = budget_run(work,
                          family_compose_dependency(work, dependency, cc->start.composed, cc->state.globals,
                                                    cc->state.families, decl->universes, decl->name),
                          budget_unlimited());
  if (work->oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  cc->start.composed = *(const FamilyComposed *)run.value;
  if (!compact_compose_swap_start(live, cc)) return compact_host_oom(heap, live);
  return budget_result_done(NULL);
}

/* One member step of a composition entry; Done = *cc holds the stepped and compacted state. */
static Result compact_compose_member(Heap *heap, Arena *live, CompactCompose *cc, FamilyMemberElaborator callback,
                                     Heap *work) {
  Result run = budget_run(work, family_compose_member(work, &cc->compose, callback), budget_unlimited());
  if (work->oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  cc->compose = *(const FamilyComposeState *)run.value;
  if (!compact_compose_swap(live, cc)) return compact_host_oom(heap, live);
  return budget_result_done(NULL);
}

/* One POLY_COMPOSE entry at specialization and member granularity (compact_group_entry precedent): begin,
 * one swap per specialize line, open, one swap per member, close. */
static Result compact_compose_entry(Heap *heap, Arena *live, ElabProgram *state, const SDecl *decl) {
  CompactCompose cc;
  Heap work = heap_new(live);
  Result run = budget_run(&work, compose_open_comp(&work, *state, decl), budget_unlimited());
  size_t i = 0;
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  cc.state = *state;
  cc.start = *(const FamilyComposeStart *)run.value;
  if (!compact_compose_swap_start(live, &cc)) return compact_host_oom(heap, live);
  for (;;) {
    VecGet got = vec_get(&cc.start.dependencies, i);
    if (got.status != VEC_GET_OK) break;
    work = heap_new(live);
    run = compact_compose_dependency(heap, live, &cc, *(const FamilyDependency *)got.value, decl, &work);
    switch (run.kind) {
    case RESULT_FAIL:
      return run;
    case RESULT_DONE:
      break;
    }
    i = i + 1;
  }
  work = heap_new(live);
  run = budget_run(&work,
                   family_compose_open(&work, cc.state.globals, cc.state.families, decl->universes, decl->name,
                                       &cc.start.composed),
                   budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  cc.compose = *(const FamilyComposeState *)run.value;
  if (!compact_compose_swap(live, &cc)) return compact_host_oom(heap, live);
  i = 0;
  for (;;) {
    VecGet got = vec_get(&cc.compose.pending, i);
    if (got.status != VEC_GET_OK) break;
    work = heap_new(live);
    run = compact_compose_member(heap, live, &cc, family_raw_elaborator(got.value), &work);
    switch (run.kind) {
    case RESULT_FAIL:
      return run;
    case RESULT_DONE:
      break;
    }
    i = i + 1;
  }
  i = 0;
  for (;;) {
    VecGet got = vec_get(&decl->helpers, i);
    if (got.status != VEC_GET_OK) break;
    work = heap_new(live);
    run = compact_compose_member(heap, live, &cc, elab_member_callback(&work, got.value, decl->universes), &work);
    switch (run.kind) {
    case RESULT_FAIL:
      return run;
    case RESULT_DONE:
      break;
    }
    i = i + 1;
  }
  work = heap_new(live);
  run = budget_run(&work, family_compose_close(&work, &cc.compose), budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  *state = program_make(cc.compose.group.globals, cc.state.definitions, (FamilyCatalog)run.value, cc.state.rows);
  return budget_result_done(NULL);
}

/* ---- instantiate member split (host only; USER ruling 2026-10-12 (e)) ----
 * A `specialize F (...) as X` entry whose template is a family (SDECL_SPECIALIZE on the family branch of
 * specialize_kind) runs as begin, one step per schema member and close, each in its own arena with
 * compact_instantiate_swap between them. begin = the two elab_program_step reservations, lower_levels and
 * family_instantiate_begin (instantiate up to install_imports); member = family_check_member on the head of
 * the remaining members; close = specialize_kind_after_installed (the instance names and the rows). A
 * `specialize` of a poly definition stays a plain entry. */
typedef struct {
  ElabProgram state;
  FamilyInstantiateStart start;
} CompactInstantiate;

static FamilyMaybeVec specialization_exports(const SDecl *decl) {
  FamilyMaybeVec exports;
  exports.some = decl->specialization.helpers.some;
  exports.value = decl->specialization.helpers.value;
  return exports;
}

static Comp instantiate_open_after_levels(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  const Vec *levels = value;
  const SSpecialization *spec = &f->decl->specialization;
  return family_instantiate_begin(heap, f->globals, f->families, spec->template, levels, spec->name, &spec->names,
                                  specialization_exports(f->decl));
}

static Comp instantiate_open_after_ctors(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  (void)value;
  return bind_ctx(heap, elab_lower_levels(heap, &f->decl->specialization.levels), instantiate_open_after_levels, f);
}

static Comp instantiate_open_after_catalogs(Heap *heap, void *ctx, void *value) {
  Frame3 *f = ctx;
  Vec *names;
  (void)value;
  names = vec_box(heap, elab_ctor_reserved(heap, f->decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_ctors(heap, names, f->globals), instantiate_open_after_ctors, f);
}

/* elab_program_step for a family SPECIALIZE decl up to install_imports (the reservations, the lowered levels
 * and family_instantiate_begin): the Done value is a FamilyInstantiateStart *. */
static Comp instantiate_open_comp(Heap *heap, ElabProgram state, const SDecl *decl) {
  Frame3 *f = frame3_new(heap);
  Vec *names;
  if (f == NULL) return comp_oom();
  f->decl = decl;
  f->globals = state.globals;
  f->definitions = state.definitions;
  f->families = state.families;
  f->rows = state.rows;
  names = vec_box(heap, elab_declared_names(heap, decl));
  if (names == NULL) return comp_oom();
  return bind_ctx(heap, elab_reserve_catalogs(heap, names, state.definitions, state.families),
                  instantiate_open_after_catalogs, f);
}

/* The close of an instantiate entry = specialize_kind_after_installed over the fully checked table: the
 * Done value is the stepped ElabProgram *. */
static Comp instantiate_close_comp(Heap *heap, ElabProgram state, const SDecl *decl, Globals installed) {
  const SSpecialization *spec = &decl->specialization;
  return elab_specialize_family_names(
      heap, family_instance_names(heap, state.families, spec->template, spec->name, &spec->names,
                                  specialization_exports(decl)),
      installed, state.globals, state.definitions, state.families, state.rows, spec->template, spec->name);
}

/* Copies the survivors of an instantiate step into a fresh arena and frees the old one: the program tables
 * (the catalog check_member reads is state.families) and the start value from member `from` on. */
static bool compact_instantiate_swap(Arena *live, CompactInstantiate *ci, size_t from) {
  Arena next = arena_new(COMPACT_CHUNK);
  Heap dst = heap_new(&next);
  bool memo = compact_memo_open(&dst);
  CompactInstantiate copied;
  copied.state = compact_program(&dst, &ci->state);
  copied.start = compact_family_instantiate_start(&dst, &ci->start, from);
  compact_memo_close(&dst, memo);
  arena_free(live);
  *live = next;
  *ci = copied;
  return !dst.oom;
}

/* One family SPECIALIZE entry at member granularity (compact_group_entry precedent): begin, one swap per
 * schema member (the head of the remaining list each time), close. */
static Result compact_instantiate_entry(Heap *heap, Arena *live, ElabProgram *state, const SDecl *decl) {
  CompactInstantiate ci;
  Heap work = heap_new(live);
  Result run = budget_run(&work, instantiate_open_comp(&work, *state, decl), budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  ci.state = *state;
  ci.start = *(const FamilyInstantiateStart *)run.value;
  if (!compact_instantiate_swap(live, &ci, 0)) return compact_host_oom(heap, live);
  for (;;) {
    VecGet got = vec_get(&ci.start.members, 0);
    if (got.status != VEC_GET_OK) break;
    work = heap_new(live);
    run = budget_run(&work,
                     family_check_member(&work, *(const Decl *)got.value, ci.state.families, nat_zero(),
                                         ci.start.installed),
                     budget_unlimited());
    if (work.oom) return compact_host_oom(heap, live);
    switch (run.kind) {
    case RESULT_FAIL:
      return compact_failure(heap, live, run.error);
    case RESULT_DONE:
      break;
    }
    ci.start.installed = *(const Globals *)run.value;
    if (!compact_instantiate_swap(live, &ci, 1)) return compact_host_oom(heap, live);
  }
  work = heap_new(live);
  run = budget_run(&work, instantiate_close_comp(&work, ci.state, decl, ci.start.installed), budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (run.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, run.error);
  case RESULT_DONE:
    break;
  }
  *state = *(const ElabProgram *)run.value;
  return budget_result_done(NULL);
}

/* Every other declaration kind: one elab_program_step in the live arena. */
static Result compact_plain_entry(Heap *heap, Arena *live, ElabProgram *state, const SDecl *decl) {
  Heap work = heap_new(live);
  Result stepped = budget_run(&work, elab_program_step(&work, *state, decl), budget_unlimited());
  if (work.oom) return compact_host_oom(heap, live);
  switch (stepped.kind) {
  case RESULT_FAIL:
    return compact_failure(heap, live, stepped.error);
  case RESULT_DONE:
    break;
  }
  *state = *(const ElabProgram *)stepped.value;
  return budget_result_done(NULL);
}

static Result compact_decl_entry(Heap *heap, Arena *live, ElabProgram *state, const SDecl *decl) {
  switch (decl->kind) {
  case SDECL_POLY_GROUP:
    return compact_group_entry(heap, live, state, decl);
  case SDECL_POLY_COMPOSE:
    return compact_compose_entry(heap, live, state, decl);
  case SDECL_SPECIALIZE:
    /* the family branch of specialize_kind (declaration_after_levels): the template names a family */
    if (family_arity(state->families, decl->specialization.template).some) {
      return compact_instantiate_entry(heap, live, state, decl);
    }
    return compact_plain_entry(heap, live, state, decl);
  case SDECL_POLY:
  case SDECL_POLY_MU:
  case SDECL_MU:
  case SDECL_REC:
  case SDECL_DEF:
  case SDECL_AXIOM:
    return compact_plain_entry(heap, live, state, decl);
  }
  return compact_plain_entry(heap, live, state, decl);
}

static Result compact_decls(Heap *heap, Globals globals, const Vec *decls) {
  ElabProgram state = program_make(globals, poly_empty(), family_empty(), vec_new(heap->arena));
  Arena live = arena_new(COMPACT_CHUNK);
  size_t i = 0;
  for (;;) {
    VecGet got = vec_get(decls, i);
    const SDecl *decl = got.value;
    Arena next;
    Heap dst;
    Result entry;
    if (got.status != VEC_GET_OK) break;
    entry = compact_decl_entry(heap, &live, &state, decl);
    switch (entry.kind) {
    case RESULT_FAIL:
      return entry;
    case RESULT_DONE:
      break;
    }
    next = arena_new(COMPACT_CHUNK);
    dst = heap_new(&next);
    state = compact_program(&dst, &state);
    arena_free(&live);
    live = next;
    if (dst.oom) return compact_host_oom(heap, &live);
    i = i + 1;
  }
  return compact_finish(heap, &live, state);
}

Result elab_check_in_compact(Heap *heap, Globals globals, Str source) {
  Result parsed = parser_program(heap, source);
  switch (parsed.kind) {
  case RESULT_FAIL:
    return parsed;
  case RESULT_DONE:
    return compact_decls(heap, globals, parsed.value);
  }
  return parsed;
}

/* elab_program_in_compact (host only, no Bend counterpart): elab_program_in with budget_unlimited over
 * already parsed declarations, with the same per-declaration arena reset as check_in_compact. The
 * relational `elaborated` helper uses it so a prelude driver never keeps a whole elaboration's garbage
 * in its arena. */
Result elab_elab_program_in_compact(Heap *heap, Globals globals, const Vec *decls) {
  return compact_decls(heap, globals, decls);
}

/* check_text (607-608) */
Result elab_check_text(Heap *heap, Globals globals, Budget budget, Str source) {
  return elab_result_rows(elab_check_in(heap, globals, budget, source));
}

/* ---- chunk 4 (Bend 609-779): checked form, axiom names and disclosures ---- */

/* entry_text (610-617) */
Str elab_entry_text(Heap *heap, const CheckRow *row) {
  Vec names = vec_new(heap->arena);
  Str text = str_empty();
  switch (row->entry.kind) {
    case ENTRY_DEF:
      text = str_cat(heap->arena, str_lit("def "), row->name);
      text = str_cat(heap->arena, text, str_lit(" : "));
      text = str_cat(heap->arena, text, pp_term(heap, &names, row->entry.def.ty));
      text = str_cat(heap->arena, text, str_lit(" := "));
      text = str_cat(heap->arena, text, pp_term(heap, &names, row->entry.def.body));
      return str_cat(heap->arena, text, str_lit("\n"));
    case ENTRY_AXIOM:
      text = str_cat(heap->arena, str_lit("axiom "), row->name);
      text = str_cat(heap->arena, text, str_lit(" : "));
      text = str_cat(heap->arena, text, pp_term(heap, &names, row->entry.ty));
      return str_cat(heap->arena, text, str_lit("\n"));
    case ENTRY_PRIM:
      text = str_cat(heap->arena, str_lit("prim "), row->name);
      text = str_cat(heap->arena, text, str_lit(" : "));
      text = str_cat(heap->arena, text, pp_term(heap, &names, row->entry.ty));
      return str_cat(heap->arena, text, str_lit("\n"));
  }
  return text;
}

/* checked_form (619-624): the entry texts in row order. */
Str elab_checked_form(Heap *heap, const Vec *rows) {
  Str text = str_empty();
  size_t i = 0;
  for (; i < rows->len; i = i + 1) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK) text = str_cat(heap->arena, text, elab_entry_text(heap, got.value));
  }
  return text;
}

/* axiom_names (626-633): the names of the axiom rows in row order. */
Vec elab_axiom_names(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < rows->len; i = i + 1) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK) {
      const CheckRow *row = got.value;
      Str name = row->name;
      if (row->entry.kind == ENTRY_AXIOM) push(heap, &out, box(heap, &name, sizeof name));
    }
  }
  return out;
}

/* disclosure_of_shape (635-654) */
MaybeStr elab_disclosure_of_shape(Shape shape) {
  MaybeStr out;
  out.some = false;
  out.value = str_empty();
  switch (shape.kind) {
    case SHAPE_ZK:
      out.some = true;
      out.value = str_lit("zk-soundness");
      return out;
    case SHAPE_FHC:
      out.some = true;
      out.value = str_lit("fhc-correctness");
      return out;
    case SHAPE_MPC:
      out.some = true;
      out.value = str_lit("mpc-simulatability");
      return out;
    case SHAPE_PI: return out;
    case SHAPE_ARG: return out;
    case SHAPE_COLL: return out;
    case SHAPE_PAR: return out;
    case SHAPE_MU: return out;
    case SHAPE_NU: return out;
  }
  return out;
}

/* prepend_disclosure (656-661): a fresh Vec, so the tail is never aliased. */
Vec elab_prepend_disclosure(Heap *heap, MaybeStr value, const Vec *tail) {
  Vec out = vec_new(heap->arena);
  if (value.some) {
    Str name = value.value;
    push(heap, &out, box(heap, &name, sizeof name));
  }
  names_extend(heap, &out, tail);
  return out;
}

/* term_list_disclosures (667-673) */
Vec elab_term_list_disclosures(Heap *heap, const Vec *terms) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < terms->len; i = i + 1) {
    VecGet got = vec_get(terms, i);
    if (got.status == VEC_GET_OK) {
      Vec part = elab_term_disclosures(heap, got.value);
      names_extend(heap, &out, &part);
    }
  }
  return out;
}

/* shape_disclosures (675-677) */
Vec elab_shape_disclosures(Heap *heap, Shape shape) {
  Vec payload = shape_payload(heap, shape);
  Vec rest = elab_term_list_disclosures(heap, &payload);
  return elab_prepend_disclosure(heap, elab_disclosure_of_shape(shape), &rest);
}

/* addr_disclosures (679-687) */
Vec elab_addr_disclosures(Heap *heap, Addr addr) {
  switch (addr.kind) {
    case ADDR_PT: return elab_term_disclosures(heap, addr.arg);
    case ADDR_LEG: return vec_new(heap->arena);
    case ADDR_CTOR: return vec_new(heap->arena);
  }
  return vec_new(heap->arena);
}

/* leg_disclosures (689-693) */
Vec elab_leg_disclosures(Heap *heap, const Leg *leg) { return elab_term_disclosures(heap, leg->body); }

/* legs_disclosures (695-701) */
Vec elab_legs_disclosures(Heap *heap, const Vec *legs) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < legs->len; i = i + 1) {
    VecGet got = vec_get(legs, i);
    if (got.status == VEC_GET_OK) {
      Vec part = elab_leg_disclosures(heap, got.value);
      names_extend(heap, &out, &part);
    }
  }
  return out;
}

/* motive_disclosures (703-709) */
Vec elab_motive_disclosures(Heap *heap, MaybeMotive motive) {
  if (!motive.some) return vec_new(heap->arena);
  return elab_term_disclosures(heap, motive.value.body);
}

/* branches_disclosures (711-717): addr then leg per branch, in branch order. */
Vec elab_branches_disclosures(Heap *heap, const Vec *branches) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < branches->len; i = i + 1) {
    VecGet got = vec_get(branches, i);
    if (got.status == VEC_GET_OK) {
      const Branch *branch = got.value;
      Vec from_addr = elab_addr_disclosures(heap, branch->addr);
      Vec from_leg = elab_leg_disclosures(heap, &branch->leg);
      names_extend(heap, &out, &from_addr);
      names_extend(heap, &out, &from_leg);
    }
  }
  return out;
}

/* elim_disclosures (719-723) */
Vec elab_elim_disclosures(Heap *heap, const ElimData *elim) {
  Vec out = elab_shape_disclosures(heap, elim->shape);
  Vec from_scrut = elab_term_disclosures(heap, elim->scrut);
  Vec from_motive = elab_motive_disclosures(heap, elim->motive);
  Vec from_branches = elab_branches_disclosures(heap, &elim->branches);
  names_extend(heap, &out, &from_scrut);
  names_extend(heap, &out, &from_motive);
  names_extend(heap, &out, &from_branches);
  return out;
}

/* term_disclosures (725-753) */
Vec elab_term_disclosures(Heap *heap, const Term *term) {
  Vec out = vec_new(heap->arena);
  Vec part = vec_new(heap->arena);
  switch (term->kind) {
    case TERM_VAR: return out;
    case TERM_UNIV: return out;
    case TERM_GLOBAL: return out;
    case TERM_LIT: return out;
    case TERM_AUTO: return out;
    case TERM_LAN:
      out = elab_shape_disclosures(heap, term->shape);
      part = elab_term_disclosures(heap, term->body);
      names_extend(heap, &out, &part);
      return out;
    case TERM_RAN:
      out = elab_shape_disclosures(heap, term->shape);
      part = elab_term_disclosures(heap, term->body);
      names_extend(heap, &out, &part);
      return out;
    case TERM_IN:
      out = elab_shape_disclosures(heap, term->shape);
      part = elab_addr_disclosures(heap, term->addr);
      names_extend(heap, &out, &part);
      part = elab_term_list_disclosures(heap, &term->args);
      names_extend(heap, &out, &part);
      return out;
    case TERM_ELIM: return elab_elim_disclosures(heap, term->data);
    case TERM_SEC:
      out = elab_shape_disclosures(heap, term->shape);
      part = elab_legs_disclosures(heap, &term->legs);
      names_extend(heap, &out, &part);
      return out;
    case TERM_OUT:
      out = elab_shape_disclosures(heap, term->shape);
      part = elab_addr_disclosures(heap, term->addr);
      names_extend(heap, &out, &part);
      part = elab_term_disclosures(heap, term->head);
      names_extend(heap, &out, &part);
      return out;
    case TERM_LET:
      out = elab_term_disclosures(heap, term->ty);
      part = elab_term_disclosures(heap, term->value);
      names_extend(heap, &out, &part);
      part = elab_term_disclosures(heap, term->body);
      names_extend(heap, &out, &part);
      return out;
    case TERM_ANN:
      out = elab_term_disclosures(heap, term->body);
      part = elab_term_disclosures(heap, term->ty);
      names_extend(heap, &out, &part);
      return out;
  }
  return out;
}

/* entry_disclosures (755-762) */
Vec elab_entry_disclosures(Heap *heap, Entry entry) {
  Vec out = vec_new(heap->arena);
  Vec part = vec_new(heap->arena);
  switch (entry.kind) {
    case ENTRY_DEF:
      out = elab_term_disclosures(heap, entry.def.ty);
      part = elab_term_disclosures(heap, entry.def.body);
      names_extend(heap, &out, &part);
      return out;
    case ENTRY_AXIOM: return elab_term_disclosures(heap, entry.ty);
    case ENTRY_PRIM: return elab_term_disclosures(heap, entry.ty);
  }
  return out;
}

/* row_disclosures (764-769) */
Vec elab_row_disclosures(Heap *heap, const Vec *rows) {
  Vec out = vec_new(heap->arena);
  size_t i = 0;
  for (; i < rows->len; i = i + 1) {
    VecGet got = vec_get(rows, i);
    if (got.status == VEC_GET_OK) {
      const CheckRow *row = got.value;
      Vec part = elab_entry_disclosures(heap, row->entry);
      names_extend(heap, &out, &part);
    }
  }
  return out;
}

/* unique_disclosures (771-776): keep the first occurrence of each name not already in seen. */
Vec elab_unique_disclosures(Heap *heap, const Vec *names, const Vec *seen) {
  Vec out = vec_new(heap->arena);
  Vec known = vec_new(heap->arena);
  size_t i = 0;
  names_extend(heap, &known, seen);
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    if (got.status == VEC_GET_OK) {
      const Str *name = got.value;
      if (!poly_member(&known, *name)) {
        push(heap, &out, got.value);
        push(heap, &known, got.value);
      }
    }
  }
  return out;
}

/* disclosure_names (778-779) */
Vec elab_disclosure_names(Heap *heap, const Vec *rows) {
  Vec axioms = elab_axiom_names(heap, rows);
  Vec found = elab_row_disclosures(heap, rows);
  Vec seen = vec_new(heap->arena);
  Vec unique = elab_unique_disclosures(heap, &found, &seen);
  return names_append(heap, &axioms, &unique);
}
