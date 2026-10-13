/* Port of bend2/surface/syntax.bend: the surface syntax tree and its two printers (raw with explicit
 * parentheses, precedence with the grammar levels). Same definitions in the same order; the type
 * family comes first as its constructors. Lists are Vec values of pointers to the element type; a
 * NULL item prints as "" (totality only, no constructor produces one). */
#include "surface.h"

/* ---- text accumulation (Bend `++` chains over a list) ---- */
typedef struct {
  StrBuf buf;
  bool ok;
} Text;

static Text text_new(Heap *heap) {
  Text text = {strbuf_new(heap->arena), true};
  return text;
}

static void text_add(Text *text, Str part) {
  if (part.len > 0) text->ok = strbuf_push(&text->buf, part) && text->ok;
}

static Str text_done(Heap *heap, Text *text) {
  if (!text->ok) heap->oom = true;
  if (text->buf.len == 0) return str_empty();
  return strbuf_str(&text->buf);
}

static const void *item_at(const Vec *vec, size_t index) {
  VecGet got = vec_get(vec, index);
  if (got.status != VEC_GET_OK) return NULL;
  return got.value;
}

static Str str_or_empty(const Str *item) {
  if (item == NULL) return str_empty();
  return *item;
}

/* ---- constructors ---- */
static const STerm sterm_placeholder = {.kind = STERM_UNIT};
static const STerm sterm_prop_node = {.kind = STERM_PROP};
static const STerm sterm_unit_node = {.kind = STERM_UNIT};
static const STerm sterm_auto_node = {.kind = STERM_AUTO};

static STerm *sterm_alloc(Heap *heap, STermKind kind) {
  STerm *node = heap_alloc(heap, sizeof(STerm));
  if (node) {
    *node = sterm_placeholder;
    node->kind = kind;
  }
  return node;
}

const STerm *sterm_var(Heap *heap, Str name) {
  STerm *node = sterm_alloc(heap, STERM_VAR);
  if (!node) return &sterm_placeholder;
  node->name = name;
  return node;
}

const STerm *sterm_nat(Heap *heap, Big value) {
  STerm *node = sterm_alloc(heap, STERM_NAT);
  if (!node) return &sterm_placeholder;
  node->nat = value;
  return node;
}

const STerm *sterm_prop(void) { return &sterm_prop_node; }

const STerm *sterm_type(Heap *heap, Big level) {
  STerm *node = sterm_alloc(heap, STERM_TYPE);
  if (!node) return &sterm_placeholder;
  node->nat = level;
  return node;
}

const STerm *sterm_sort(Heap *heap, const Universe *level) {
  STerm *node = sterm_alloc(heap, STERM_SORT);
  if (!node) return &sterm_placeholder;
  node->sort = level;
  return node;
}

const STerm *sterm_prim(Heap *heap, SPrim prim) {
  STerm *node = sterm_alloc(heap, STERM_PRIM);
  if (!node) return &sterm_placeholder;
  node->prim = prim;
  return node;
}

const STerm *sterm_unit(void) { return &sterm_unit_node; }

const STerm *sterm_auto(void) { return &sterm_auto_node; }

const STerm *sterm_pair(Heap *heap, const STerm *left, const STerm *right) {
  STerm *node = sterm_alloc(heap, STERM_PAIR);
  if (!node) return &sterm_placeholder;
  node->left = left;
  node->right = right;
  return node;
}

static const STerm *sterm_items(Heap *heap, STermKind kind, Vec items) {
  STerm *node = sterm_alloc(heap, kind);
  if (!node) return &sterm_placeholder;
  node->items = items;
  return node;
}

const STerm *sterm_tuple(Heap *heap, Vec items) { return sterm_items(heap, STERM_TUPLE, items); }

const STerm *sterm_sum(Heap *heap, Vec items) { return sterm_items(heap, STERM_SUM, items); }

const STerm *sterm_prod(Heap *heap, Vec items) { return sterm_items(heap, STERM_PROD, items); }

const STerm *sterm_proj(Heap *heap, const STerm *head, Big index) {
  STerm *node = sterm_alloc(heap, STERM_PROJ);
  if (!node) return &sterm_placeholder;
  node->head = head;
  node->index = index;
  return node;
}

const STerm *sterm_inj(Heap *heap, Big index, Big count, const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_INJ);
  if (!node) return &sterm_placeholder;
  node->index = index;
  node->count = count;
  node->value = value;
  return node;
}

const STerm *sterm_absurd(Heap *heap, const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_ABSURD);
  if (!node) return &sterm_placeholder;
  node->value = value;
  return node;
}

const STerm *sterm_app(Heap *heap, const STerm *head, const STerm *arg) {
  STerm *node = sterm_alloc(heap, STERM_APP);
  if (!node) return &sterm_placeholder;
  node->head = head;
  node->arg = arg;
  return node;
}

const STerm *sterm_fun(Heap *heap, Vec binders, const STerm *body) {
  STerm *node = sterm_alloc(heap, STERM_FUN);
  if (!node) return &sterm_placeholder;
  node->binders = binders;
  node->body = body;
  return node;
}

static const STerm *sterm_bound(Heap *heap, STermKind kind, SBinder binder, const STerm *body) {
  STerm *node = sterm_alloc(heap, kind);
  if (!node) return &sterm_placeholder;
  node->binder = binder;
  node->body = body;
  return node;
}

const STerm *sterm_arrow(Heap *heap, SBinder binder, const STerm *body) {
  return sterm_bound(heap, STERM_ARROW, binder, body);
}

const STerm *sterm_star(Heap *heap, SBinder binder, const STerm *body) {
  return sterm_bound(heap, STERM_STAR, binder, body);
}

const STerm *sterm_zk_ty(Heap *heap, SBinder binder, const STerm *body) {
  return sterm_bound(heap, STERM_ZK_TY, binder, body);
}

const STerm *sterm_prove(Heap *heap, const STerm *key, const STerm *witness, const STerm *relation) {
  STerm *node = sterm_alloc(heap, STERM_PROVE);
  if (!node) return &sterm_placeholder;
  node->key = key;
  node->witness = witness;
  node->relation = relation;
  return node;
}

const STerm *sterm_verify(Heap *heap, const STerm *key, const STerm *proof) {
  STerm *node = sterm_alloc(heap, STERM_VERIFY);
  if (!node) return &sterm_placeholder;
  node->key = key;
  node->proof = proof;
  return node;
}

const STerm *sterm_fhc_ty(Heap *heap, const STerm *level, const STerm *ty) {
  STerm *node = sterm_alloc(heap, STERM_FHC_TY);
  if (!node) return &sterm_placeholder;
  node->level = level;
  node->ty = ty;
  return node;
}

static const STerm *sterm_keyed(Heap *heap, STermKind kind, const STerm *key, const STerm *value) {
  STerm *node = sterm_alloc(heap, kind);
  if (!node) return &sterm_placeholder;
  node->key = key;
  node->value = value;
  return node;
}

const STerm *sterm_enc(Heap *heap, const STerm *key, const STerm *value) {
  return sterm_keyed(heap, STERM_ENC, key, value);
}

const STerm *sterm_eval(Heap *heap, const STerm *function, const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_EVAL);
  if (!node) return &sterm_placeholder;
  node->function = function;
  node->value = value;
  return node;
}

const STerm *sterm_dec(Heap *heap, const STerm *key, const STerm *value) {
  return sterm_keyed(heap, STERM_DEC, key, value);
}

const STerm *sterm_mpc_ty(Heap *heap, const STerm *parties, const STerm *access, const STerm *ty) {
  STerm *node = sterm_alloc(heap, STERM_MPC_TY);
  if (!node) return &sterm_placeholder;
  node->parties = parties;
  node->access = access;
  node->ty = ty;
  return node;
}

const STerm *sterm_share(Heap *heap, const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_SHARE);
  if (!node) return &sterm_placeholder;
  node->value = value;
  return node;
}

const STerm *sterm_input(Heap *heap, const STerm *party, const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_INPUT);
  if (!node) return &sterm_placeholder;
  node->party = party;
  node->value = value;
  return node;
}

const STerm *sterm_join(Heap *heap, const STerm *parties, const STerm *function, Vec values) {
  STerm *node = sterm_alloc(heap, STERM_JOIN);
  if (!node) return &sterm_placeholder;
  node->parties = parties;
  node->function = function;
  node->items = values;
  return node;
}

const STerm *sterm_open(Heap *heap, MaybeQuantity quantity, const STerm *parties, const STerm *proof,
                        const STerm *value) {
  STerm *node = sterm_alloc(heap, STERM_OPEN);
  if (!node) return &sterm_placeholder;
  node->quantity = quantity;
  node->parties = parties;
  node->proof = proof;
  node->value = value;
  return node;
}

const STerm *sterm_let(Heap *heap, Str name, const STerm *ty, const STerm *value, const STerm *body) {
  STerm *node = sterm_alloc(heap, STERM_LET);
  if (!node) return &sterm_placeholder;
  node->name = name;
  node->ty = ty;
  node->value = value;
  node->body = body;
  return node;
}

const STerm *sterm_ann(Heap *heap, const STerm *value, const STerm *ty) {
  STerm *node = sterm_alloc(heap, STERM_ANN);
  if (!node) return &sterm_placeholder;
  node->value = value;
  node->ty = ty;
  return node;
}

static const STerm *sterm_eliminator(Heap *heap, STermKind kind, const STerm *scrut, MaybeSMotive motive,
                                     Vec branches) {
  STerm *node = sterm_alloc(heap, kind);
  if (!node) return &sterm_placeholder;
  node->scrut = scrut;
  node->motive = motive;
  node->branches = branches;
  return node;
}

const STerm *sterm_case(Heap *heap, const STerm *scrut, MaybeSMotive motive, Vec branches) {
  return sterm_eliminator(heap, STERM_CASE, scrut, motive, branches);
}

const STerm *sterm_match(Heap *heap, const STerm *scrut, MaybeSMotive motive, Vec branches) {
  return sterm_eliminator(heap, STERM_MATCH, scrut, motive, branches);
}

SBinder sbinder(Quantity quantity, Str name, const STerm *ty) {
  SBinder binder = {quantity, name, ty};
  return binder;
}

static const SBinder sbinder_placeholder = {.quantity = QUANTITY_MANY, .ty = &sterm_placeholder};

const SBinder *sbinder_new(Heap *heap, SBinder binder) {
  SBinder *node = heap_alloc(heap, sizeof(SBinder));
  if (!node) return &sbinder_placeholder;
  *node = binder;
  return node;
}

SMotive smotive(Str self, MaybeStr ind, Vec indices, const STerm *body) {
  SMotive motive = {self, ind, indices, body};
  return motive;
}

SField sfield(Quantity quantity, Str name, MaybeSTerm ty) {
  SField field = {quantity, name, ty};
  return field;
}

static const SField sfield_placeholder = {.quantity = QUANTITY_MANY};

const SField *sfield_new(Heap *heap, SField field) {
  SField *node = heap_alloc(heap, sizeof(SField));
  if (!node) return &sfield_placeholder;
  *node = field;
  return node;
}

static const SBranch sbranch_placeholder = {.kind = SBRANCH_LEG, .body = &sterm_placeholder};

static SBranch *sbranch_alloc(Heap *heap, SBranchKind kind, const STerm *body) {
  SBranch *node = heap_alloc(heap, sizeof(SBranch));
  if (node) {
    *node = sbranch_placeholder;
    node->kind = kind;
    node->body = body;
  }
  return node;
}

const SBranch *sbranch_leg_new(Heap *heap, Big index, Vec binders, const STerm *body) {
  SBranch *node = sbranch_alloc(heap, SBRANCH_LEG, body);
  if (!node) return &sbranch_placeholder;
  node->index = index;
  node->binders = binders;
  return node;
}

const SBranch *sbranch_ctor_new(Heap *heap, Str name, Vec fields, const STerm *body) {
  SBranch *node = sbranch_alloc(heap, SBRANCH_CTOR, body);
  if (!node) return &sbranch_placeholder;
  node->name = name;
  node->fields = fields;
  return node;
}

static const SFamCtor sfam_ctor_placeholder = {.ty = &sterm_placeholder};

const SFamCtor *sfam_ctor_new(Heap *heap, Str name, const STerm *ty) {
  SFamCtor *node = heap_alloc(heap, sizeof(SFamCtor));
  if (!node) return &sfam_ctor_placeholder;
  node->name = name;
  node->ty = ty;
  return node;
}

SFam sfam(Str name, Vec params, const STerm *ty, Vec ctors) {
  SFam family = {name, params, ty, ctors};
  return family;
}

static const SFam sfam_placeholder = {.ty = &sterm_placeholder};

const SFam *sfam_new(Heap *heap, SFam family) {
  SFam *node = heap_alloc(heap, sizeof(SFam));
  if (!node) return &sfam_placeholder;
  *node = family;
  return node;
}

static const SRecDef srec_def_placeholder = {.ty = &sterm_placeholder, .body = &sterm_placeholder};

const SRecDef *srec_def_new(Heap *heap, Str name, const STerm *ty, const STerm *body) {
  SRecDef *node = heap_alloc(heap, sizeof(SRecDef));
  if (!node) return &srec_def_placeholder;
  node->name = name;
  node->ty = ty;
  node->body = body;
  return node;
}

SSpecialization sspecialization(Str template, Vec levels, Str name, Vec names, MaybeVec helpers) {
  SSpecialization specialization = {template, levels, name, names, helpers};
  return specialization;
}

static const SSpecialization sspecialization_placeholder = {.helpers = {false}};

const SSpecialization *sspecialization_new(Heap *heap, SSpecialization specialization) {
  SSpecialization *node = heap_alloc(heap, sizeof(SSpecialization));
  if (!node) return &sspecialization_placeholder;
  *node = specialization;
  return node;
}

static const SDecl sdecl_placeholder = {.kind = SDECL_AXIOM, .ty = &sterm_placeholder};

static SDecl *sdecl_alloc(Heap *heap, SDeclKind kind) {
  SDecl *node = heap_alloc(heap, sizeof(SDecl));
  if (node) {
    *node = sdecl_placeholder;
    node->kind = kind;
  }
  return node;
}

const SDecl *sdecl_poly_new(Heap *heap, Nat universes, Str name, const STerm *ty, const STerm *body) {
  SDecl *node = sdecl_alloc(heap, SDECL_POLY);
  if (!node) return &sdecl_placeholder;
  node->universes = universes;
  node->name = name;
  node->ty = ty;
  node->body = body;
  return node;
}

const SDecl *sdecl_poly_mu_new(Heap *heap, Nat universes, SFam family) {
  SDecl *node = sdecl_alloc(heap, SDECL_POLY_MU);
  if (!node) return &sdecl_placeholder;
  node->universes = universes;
  node->family = family;
  return node;
}

const SDecl *sdecl_poly_group_new(Heap *heap, Nat universes, SFam family, Vec families, Vec helpers) {
  SDecl *node = sdecl_alloc(heap, SDECL_POLY_GROUP);
  if (!node) return &sdecl_placeholder;
  node->universes = universes;
  node->family = family;
  node->families = families;
  node->helpers = helpers;
  return node;
}

const SDecl *sdecl_poly_compose_new(Heap *heap, Nat universes, Str name, Vec specializations, Vec helpers) {
  SDecl *node = sdecl_alloc(heap, SDECL_POLY_COMPOSE);
  if (!node) return &sdecl_placeholder;
  node->universes = universes;
  node->name = name;
  node->specializations = specializations;
  node->helpers = helpers;
  return node;
}

const SDecl *sdecl_specialize_new(Heap *heap, SSpecialization specialization) {
  SDecl *node = sdecl_alloc(heap, SDECL_SPECIALIZE);
  if (!node) return &sdecl_placeholder;
  node->specialization = specialization;
  return node;
}

const SDecl *sdecl_def_new(Heap *heap, Str name, const STerm *ty, const STerm *body) {
  SDecl *node = sdecl_alloc(heap, SDECL_DEF);
  if (!node) return &sdecl_placeholder;
  node->name = name;
  node->ty = ty;
  node->body = body;
  return node;
}

const SDecl *sdecl_axiom_new(Heap *heap, Str name, const STerm *ty) {
  SDecl *node = sdecl_alloc(heap, SDECL_AXIOM);
  if (!node) return &sdecl_placeholder;
  node->name = name;
  node->ty = ty;
  return node;
}

const SDecl *sdecl_mu_new(Heap *heap, Vec families) {
  SDecl *node = sdecl_alloc(heap, SDECL_MU);
  if (!node) return &sdecl_placeholder;
  node->families = families;
  return node;
}

const SDecl *sdecl_rec_new(Heap *heap, Vec definitions) {
  SDecl *node = sdecl_alloc(heap, SDECL_REC);
  if (!node) return &sdecl_placeholder;
  node->definitions = definitions;
  return node;
}

/* ---- printers (syntax.bend order) ---- */
Str syntax_prim_name(SPrim prim) {
  switch (prim) {
    case SPRIM_ADD:
      return str_lit("natAdd");
    case SPRIM_SUB:
      return str_lit("natSub");
    case SPRIM_MUL:
      return str_lit("natMul");
    case SPRIM_EQ:
      return str_lit("natEq");
    case SPRIM_LT:
      return str_lit("natLt");
  }
  return str_empty();
}

/* Printing uses explicit parentheses to preserve precedence and branch scope. */
Str syntax_mark(Quantity quantity) {
  switch (quantity) {
    case QUANTITY_ZERO:
      return str_lit("0 ");
    case QUANTITY_ONE:
      return str_lit("1 ");
    case QUANTITY_MANY:
      return str_empty();
  }
  return str_empty();
}

Str syntax_binder_text(Heap *heap, SBinder binder) {
  return SURFACE_CAT(heap, str_lit("("), syntax_mark(binder.quantity), binder.name, str_lit(" : "),
                     syntax_raw(heap, binder.ty), str_lit(")"));
}

Str syntax_binders_text(Heap *heap, const Vec *binders) {
  Text text = text_new(heap);
  for (size_t i = 0; i < binders->len; i++) {
    const SBinder *binder = item_at(binders, i);
    text_add(&text, str_lit(" "));
    if (binder) text_add(&text, syntax_binder_text(heap, *binder));
  }
  return text_done(heap, &text);
}

Str syntax_fields_text(Heap *heap, const Vec *fields) {
  Text text = text_new(heap);
  for (size_t i = 0; i < fields->len; i++) {
    const SField *field = item_at(fields, i);
    text_add(&text, str_lit(" "));
    if (field == NULL) continue;
    if (field->ty.some) {
      text_add(&text, syntax_binder_text(heap, sbinder(field->quantity, field->name, field->ty.value)));
    } else {
      text_add(&text, syntax_mark(field->quantity));
      text_add(&text, field->name);
    }
  }
  return text_done(heap, &text);
}

Str syntax_names_text(Heap *heap, const Vec *names) {
  Text text = text_new(heap);
  for (size_t i = 0; i < names->len; i++) {
    text_add(&text, str_lit(" "));
    text_add(&text, str_or_empty(item_at(names, i)));
  }
  return text_done(heap, &text);
}

Str syntax_index_text(Heap *heap, MaybeStr ind, const Vec *indices) {
  if (!ind.some) return str_empty();
  return SURFACE_CAT(heap, str_lit(" in "), ind.value, syntax_names_text(heap, indices));
}

Str syntax_motive_text(Heap *heap, MaybeSMotive motive) {
  if (!motive.some) return str_empty();
  return SURFACE_CAT(heap, str_lit(" as "), motive.value.self,
                     syntax_index_text(heap, motive.value.ind, &motive.value.indices), str_lit(" return "),
                     syntax_raw(heap, motive.value.body));
}

Str syntax_branches_text(Heap *heap, const Vec *branches) {
  Text text = text_new(heap);
  for (size_t i = 0; i < branches->len; i++) {
    const SBranch *branch = item_at(branches, i);
    if (branch == NULL) continue;
    switch (branch->kind) {
      case SBRANCH_LEG:
        text_add(&text, SURFACE_CAT(heap, str_lit(" | "), big_to_string(heap, branch->index),
                                    syntax_binders_text(heap, &branch->binders), str_lit(" => "),
                                    syntax_raw(heap, branch->body)));
        break;
      case SBRANCH_CTOR:
        text_add(&text, SURFACE_CAT(heap, str_lit(" | "), branch->name, syntax_fields_text(heap, &branch->fields),
                                    str_lit(" => "), syntax_raw(heap, branch->body)));
        break;
    }
  }
  return text_done(heap, &text);
}

Str syntax_terms_text(Heap *heap, const Vec *terms, Str separator) {
  Text text = text_new(heap);
  for (size_t i = 0; i < terms->len; i++) {
    const STerm *term = item_at(terms, i);
    if (i > 0) text_add(&text, separator);
    if (term) text_add(&text, syntax_raw(heap, term));
  }
  return text_done(heap, &text);
}

Str syntax_optional_mark(MaybeQuantity quantity) {
  if (!quantity.some) return str_empty();
  return syntax_mark(quantity.value);
}

Str syntax_raw(Heap *heap, const STerm *term) {
  switch (term->kind) {
    case STERM_VAR:
      return term->name;
    case STERM_NAT:
      return big_to_string(heap, term->nat);
    case STERM_PROP:
      return str_lit("Prop");
    case STERM_TYPE:
      return heap_str_cat(heap, str_lit("Type "), big_to_string(heap, term->nat));
    case STERM_SORT:
      return heap_str_cat(heap, str_lit("Sort "), universe_text(heap, term->sort));
    case STERM_PRIM:
      return syntax_prim_name(term->prim);
    case STERM_UNIT:
      return str_lit("()");
    case STERM_AUTO:
      return str_lit("auto");
    case STERM_PAIR:
      return SURFACE_CAT(heap, str_lit("("), syntax_raw(heap, term->left), str_lit(", "),
                         syntax_raw(heap, term->right), str_lit(")"));
    case STERM_PROJ:
      return SURFACE_CAT(heap, str_lit("("), syntax_raw(heap, term->head), str_lit(")."),
                         big_to_string(heap, term->index));
    case STERM_INJ:
      return SURFACE_CAT(heap, str_lit("(inj "), big_to_string(heap, term->index), str_lit(" of "),
                         big_to_string(heap, term->count), str_lit(" "), syntax_raw(heap, term->value), str_lit(")"));
    case STERM_ABSURD:
      return SURFACE_CAT(heap, str_lit("(absurd "), syntax_raw(heap, term->value), str_lit(")"));
    case STERM_APP:
      return SURFACE_CAT(heap, str_lit("("), syntax_raw(heap, term->head), str_lit(" "), syntax_raw(heap, term->arg),
                         str_lit(")"));
    case STERM_FUN:
      return SURFACE_CAT(heap, str_lit("(fun"), syntax_binders_text(heap, &term->binders), str_lit(" => "),
                         syntax_raw(heap, term->body), str_lit(")"));
    case STERM_ARROW:
      return SURFACE_CAT(heap, str_lit("("), syntax_binder_text(heap, term->binder), str_lit(" -> "),
                         syntax_raw(heap, term->body), str_lit(")"));
    case STERM_STAR:
      return SURFACE_CAT(heap, str_lit("("), syntax_binder_text(heap, term->binder), str_lit(" * "),
                         syntax_raw(heap, term->body), str_lit(")"));
    case STERM_ZK_TY:
      return SURFACE_CAT(heap, str_lit("(zk "), syntax_binder_text(heap, term->binder), str_lit(" * "),
                         syntax_raw(heap, term->body), str_lit(")"));
    case STERM_MPC_TY:
      return SURFACE_CAT(heap, str_lit("(mpc["), syntax_raw(heap, term->parties), str_lit(", "),
                         syntax_raw(heap, term->access), str_lit("] "), syntax_raw(heap, term->ty), str_lit(")"));
    case STERM_JOIN:
      return SURFACE_CAT(heap, str_lit("(mpc "), syntax_raw(heap, term->parties), str_lit(" "),
                         syntax_raw(heap, term->function), str_lit(" "), syntax_terms_text(heap, &term->items, str_lit(" ")),
                         str_lit(")"));
    case STERM_OPEN:
      return SURFACE_CAT(heap, str_lit("(open "), syntax_raw(heap, term->parties), str_lit(" "),
                         syntax_optional_mark(term->quantity), syntax_raw(heap, term->proof), str_lit(" "),
                         syntax_raw(heap, term->value), str_lit(")"));
    case STERM_LET:
      return SURFACE_CAT(heap, str_lit("(let "), term->name, str_lit(" : "), syntax_raw(heap, term->ty),
                         str_lit(" := "), syntax_raw(heap, term->value), str_lit(" in "), syntax_raw(heap, term->body),
                         str_lit(")"));
    case STERM_ANN:
      return SURFACE_CAT(heap, str_lit("("), syntax_raw(heap, term->value), str_lit(" : "), syntax_raw(heap, term->ty),
                         str_lit(")"));
    case STERM_TUPLE:
      return SURFACE_CAT(heap, str_lit("tuple ("), syntax_terms_text(heap, &term->items, str_lit(", ")), str_lit(")"));
    case STERM_SUM:
      return SURFACE_CAT(heap, str_lit("sum ("), syntax_terms_text(heap, &term->items, str_lit(", ")), str_lit(")"));
    case STERM_PROD:
      return SURFACE_CAT(heap, str_lit("prod ("), syntax_terms_text(heap, &term->items, str_lit(", ")), str_lit(")"));
    case STERM_PROVE:
      return SURFACE_CAT(heap, str_lit("(prove "), syntax_raw(heap, term->key), str_lit(" "),
                         syntax_raw(heap, term->witness), str_lit(" "), syntax_raw(heap, term->relation), str_lit(")"));
    case STERM_VERIFY:
      return SURFACE_CAT(heap, str_lit("(verify "), syntax_raw(heap, term->key), str_lit(" "),
                         syntax_raw(heap, term->proof), str_lit(")"));
    case STERM_FHC_TY:
      return SURFACE_CAT(heap, str_lit("(fhc "), syntax_raw(heap, term->level), str_lit(" "), syntax_raw(heap, term->ty),
                         str_lit(")"));
    case STERM_ENC:
      return SURFACE_CAT(heap, str_lit("(enc "), syntax_raw(heap, term->key), str_lit(" "), syntax_raw(heap, term->value),
                         str_lit(")"));
    case STERM_EVAL:
      return SURFACE_CAT(heap, str_lit("(eval "), syntax_raw(heap, term->function), str_lit(" "),
                         syntax_raw(heap, term->value), str_lit(")"));
    case STERM_DEC:
      return SURFACE_CAT(heap, str_lit("(dec "), syntax_raw(heap, term->key), str_lit(" "), syntax_raw(heap, term->value),
                         str_lit(")"));
    case STERM_SHARE:
      return SURFACE_CAT(heap, str_lit("(share "), syntax_raw(heap, term->value), str_lit(")"));
    case STERM_INPUT:
      return SURFACE_CAT(heap, str_lit("(input "), syntax_raw(heap, term->party), str_lit(" "),
                         syntax_raw(heap, term->value), str_lit(")"));
    case STERM_CASE:
      return SURFACE_CAT(heap, str_lit("(case "), syntax_raw(heap, term->scrut), syntax_motive_text(heap, term->motive),
                         str_lit(" with"), syntax_branches_text(heap, &term->branches), str_lit(")"));
    case STERM_MATCH:
      return SURFACE_CAT(heap, str_lit("(match "), syntax_raw(heap, term->scrut), syntax_motive_text(heap, term->motive),
                         str_lit(" with"), syntax_branches_text(heap, &term->branches), str_lit(")"));
  }
  return str_empty();
}

Str syntax_show(Heap *heap, const STerm *term) { return syntax_raw(heap, term); }

Str syntax_fam_ctor_text(Heap *heap, SFamCtor ctor) {
  return SURFACE_CAT(heap, str_lit("| "), ctor.name, str_lit(" : "), syntax_raw(heap, ctor.ty), str_lit("\n"));
}

Str syntax_fam_ctors_text(Heap *heap, const Vec *ctors) {
  Text text = text_new(heap);
  for (size_t i = 0; i < ctors->len; i++) {
    const SFamCtor *ctor = item_at(ctors, i);
    if (ctor) text_add(&text, syntax_fam_ctor_text(heap, *ctor));
  }
  return text_done(heap, &text);
}

Str syntax_fam_text(Heap *heap, Str word, SFam family) {
  return SURFACE_CAT(heap, word, str_lit(" "), family.name, syntax_binders_text(heap, &family.params), str_lit(" : "),
                     syntax_raw(heap, family.ty), str_lit(" :=\n"), syntax_fam_ctors_text(heap, &family.ctors));
}

Str syntax_families_text(Heap *heap, const Vec *families, Str word) {
  Text text = text_new(heap);
  for (size_t i = 0; i < families->len; i++) {
    const SFam *family = item_at(families, i);
    if (family) text_add(&text, syntax_fam_text(heap, word, *family));
  }
  return text_done(heap, &text);
}

Str syntax_bindings_text(Heap *heap, const Vec *bindings) {
  Text text = text_new(heap);
  for (size_t i = 0; i < bindings->len; i++) {
    const PolyRename *binding = item_at(bindings, i);
    if (i > 0) text_add(&text, str_lit(", "));
    if (binding == NULL) continue;
    text_add(&text, binding->source);
    text_add(&text, str_lit(" := "));
    text_add(&text, binding->target);
  }
  return text_done(heap, &text);
}

Str syntax_reuse_text(Heap *heap, const Vec *bindings) {
  if (bindings->len == 0) return str_empty();
  return SURFACE_CAT(heap, str_lit(" with ("), syntax_bindings_text(heap, bindings), str_lit(")"));
}

Str syntax_exports_text(Heap *heap, MaybeVec exports) {
  if (!exports.some) return str_empty();
  return SURFACE_CAT(heap, str_lit(" export ("), syntax_bindings_text(heap, &exports.value), str_lit(")"));
}

Str syntax_universe_names(Heap *heap, Nat count, Nat index) {
  NatToU64 total = nat_to_u64(count);
  if (total.status != NAT_TO_U64_OK) {
    heap->oom = true;
    return str_empty();
  }
  Text text = text_new(heap);
  Nat current = index;
  for (uint64_t i = 0; i < total.value; i++) {
    if (i > 0) text_add(&text, str_lit(", "));
    text_add(&text, str_lit("u"));
    text_add(&text, heap_nat_show(heap, current));
    if (i + 1 < total.value) current = heap_nat_succ(heap, current);
  }
  return text_done(heap, &text);
}

Str syntax_universe_arguments(Heap *heap, const Vec *levels) {
  Text text = text_new(heap);
  for (size_t i = 0; i < levels->len; i++) {
    const Universe *level = item_at(levels, i);
    if (i > 0) text_add(&text, str_lit(", "));
    if (level) text_add(&text, universe_text(heap, level));
  }
  return text_done(heap, &text);
}

Str syntax_specialization_text(Heap *heap, SSpecialization specialization) {
  return SURFACE_CAT(heap, str_lit("specialize "), specialization.template, str_lit(" ("),
                     syntax_universe_arguments(heap, &specialization.levels), str_lit(") as "), specialization.name,
                     syntax_reuse_text(heap, &specialization.names), syntax_exports_text(heap, specialization.helpers),
                     str_lit("\n"));
}

Str syntax_specializations_text(Heap *heap, const Vec *specializations) {
  Text text = text_new(heap);
  for (size_t i = 0; i < specializations->len; i++) {
    const SSpecialization *specialization = item_at(specializations, i);
    if (specialization) text_add(&text, syntax_specialization_text(heap, *specialization));
  }
  return text_done(heap, &text);
}

Str syntax_rec_defs_text(Heap *heap, const Vec *definitions, Str word, Str next) {
  Text text = text_new(heap);
  for (size_t i = 0; i < definitions->len; i++) {
    const SRecDef *definition = item_at(definitions, i);
    Str keyword = i == 0 ? word : next;
    if (definition == NULL) continue;
    text_add(&text, SURFACE_CAT(heap, keyword, str_lit(" "), definition->name, str_lit(" : "),
                                syntax_raw(heap, definition->ty), str_lit(" := "), syntax_raw(heap, definition->body),
                                str_lit("\n")));
  }
  return text_done(heap, &text);
}

Str syntax_members_text(Heap *heap, const Vec *members) {
  if (members->len == 0) return str_empty();
  return SURFACE_CAT(heap, str_lit("where\n"), syntax_rec_defs_text(heap, members, str_lit("def"), str_lit("def")),
                     str_lit("end\n"));
}

Str syntax_decl_text(Heap *heap, const SDecl *decl) {
  switch (decl->kind) {
    case SDECL_POLY:
      return SURFACE_CAT(heap, str_lit("poly ("), syntax_universe_names(heap, decl->universes, nat_zero()),
                         str_lit(") def "), decl->name, str_lit(" : "), syntax_raw(heap, decl->ty), str_lit(" := "),
                         syntax_raw(heap, decl->body), str_lit("\n"));
    case SDECL_POLY_MU:
      return SURFACE_CAT(heap, str_lit("poly ("), syntax_universe_names(heap, decl->universes, nat_zero()),
                         str_lit(") "), syntax_fam_text(heap, str_lit("mu"), decl->family));
    case SDECL_POLY_GROUP:
      return SURFACE_CAT(heap, str_lit("poly ("), syntax_universe_names(heap, decl->universes, nat_zero()),
                         str_lit(") "), syntax_fam_text(heap, str_lit("mu"), decl->family),
                         syntax_families_text(heap, &decl->families, str_lit("and")),
                         syntax_members_text(heap, &decl->helpers));
    case SDECL_POLY_COMPOSE:
      return SURFACE_CAT(heap, str_lit("poly ("), syntax_universe_names(heap, decl->universes, nat_zero()),
                         str_lit(") group "), decl->name, str_lit(" where\n"),
                         syntax_specializations_text(heap, &decl->specializations),
                         syntax_rec_defs_text(heap, &decl->helpers, str_lit("def"), str_lit("def")), str_lit("end\n"));
    case SDECL_SPECIALIZE:
      return syntax_specialization_text(heap, decl->specialization);
    case SDECL_DEF:
      return SURFACE_CAT(heap, str_lit("def "), decl->name, str_lit(" : "), syntax_raw(heap, decl->ty), str_lit(" := "),
                         syntax_raw(heap, decl->body), str_lit("\n"));
    case SDECL_AXIOM:
      return SURFACE_CAT(heap, str_lit("axiom "), decl->name, str_lit(" : "), syntax_raw(heap, decl->ty), str_lit("\n"));
    case SDECL_MU: {
      if (decl->families.len == 0) return str_empty();
      if (decl->families.len == 1) {
        const SFam *family = item_at(&decl->families, 0);
        if (family == NULL) return str_empty();
        return syntax_fam_text(heap, str_lit("mu"), *family);
      }
      return SURFACE_CAT(heap, str_lit("mutual\n"), syntax_families_text(heap, &decl->families, str_lit("mu")),
                         str_lit("end\n"));
    }
    case SDECL_REC:
      return syntax_rec_defs_text(heap, &decl->definitions, str_lit("def rec"), str_lit("and"));
  }
  return str_empty();
}

Str syntax_print(Heap *heap, const Vec *decls) {
  Text text = text_new(heap);
  for (size_t i = 0; i < decls->len; i++) {
    const SDecl *decl = item_at(decls, i);
    if (decl) text_add(&text, syntax_decl_text(heap, decl));
  }
  return text_done(heap, &text);
}

/* Original Syntax.at accepts a signed OCaml int. Big preserves that domain,
 * including negative precedence and values beyond the three grammar levels. */
uint32_t syntax_level_of(const STerm *term) {
  switch (term->kind) {
    case STERM_VAR:
    case STERM_NAT:
    case STERM_PROP:
    case STERM_TYPE:
    case STERM_SORT:
    case STERM_PRIM:
    case STERM_UNIT:
    case STERM_AUTO:
    case STERM_PAIR:
    case STERM_TUPLE:
    case STERM_SUM:
    case STERM_PROD:
    case STERM_PROJ:
    case STERM_ANN:
      return 2;
    case STERM_INJ:
    case STERM_ABSURD:
    case STERM_APP:
      return 1;
    case STERM_FUN:
    case STERM_ARROW:
    case STERM_STAR:
    case STERM_ZK_TY:
      return 0;
    case STERM_PROVE:
    case STERM_VERIFY:
    case STERM_FHC_TY:
    case STERM_ENC:
    case STERM_EVAL:
    case STERM_DEC:
    case STERM_MPC_TY:
    case STERM_SHARE:
    case STERM_INPUT:
    case STERM_JOIN:
    case STERM_OPEN:
      return 1;
    case STERM_LET:
    case STERM_CASE:
    case STERM_MATCH:
      return 0;
  }
  return 0;
}

Str syntax_precedence_at_level(Heap *heap, uint32_t level, const STerm *term) {
  return syntax_at(heap, big_of_u32(heap, level), term);
}

Str syntax_precedence_binder_text(Heap *heap, SBinder binder) {
  return SURFACE_CAT(heap, str_lit("("), syntax_mark(binder.quantity), binder.name, str_lit(" : "),
                     syntax_precedence_at_level(heap, 0, binder.ty), str_lit(")"));
}

Str syntax_precedence_binders_text(Heap *heap, const Vec *binders) {
  Text text = text_new(heap);
  for (size_t i = 0; i < binders->len; i++) {
    const SBinder *binder = item_at(binders, i);
    if (i > 0) text_add(&text, str_lit(" "));
    if (binder) text_add(&text, syntax_precedence_binder_text(heap, *binder));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_branch_binders(Heap *heap, const Vec *binders) {
  Text text = text_new(heap);
  for (size_t i = 0; i < binders->len; i++) {
    const SBinder *binder = item_at(binders, i);
    text_add(&text, str_lit(" "));
    if (binder) text_add(&text, syntax_precedence_binder_text(heap, *binder));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_field_text(Heap *heap, SField field) {
  if (field.ty.some) {
    return heap_str_cat(heap, str_lit(" "),
                        syntax_precedence_binder_text(heap, sbinder(field.quantity, field.name, field.ty.value)));
  }
  return SURFACE_CAT(heap, str_lit(" "), syntax_mark(field.quantity), field.name);
}

Str syntax_precedence_fields_text(Heap *heap, const Vec *fields) {
  Text text = text_new(heap);
  for (size_t i = 0; i < fields->len; i++) {
    const SField *field = item_at(fields, i);
    if (field) text_add(&text, syntax_precedence_field_text(heap, *field));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_ind_text(Heap *heap, MaybeStr ind, const Vec *indices) {
  if (!ind.some) return str_empty();
  return SURFACE_CAT(heap, str_lit(" in "), ind.value, syntax_names_text(heap, indices));
}

Str syntax_precedence_motive_text(Heap *heap, SMotive motive) {
  return SURFACE_CAT(heap, str_lit(" as "), motive.self, syntax_precedence_ind_text(heap, motive.ind, &motive.indices),
                     str_lit(" return "), syntax_precedence_at_level(heap, 1, motive.body));
}

Str syntax_precedence_optional_motive(Heap *heap, MaybeSMotive motive) {
  if (!motive.some) return str_empty();
  return syntax_precedence_motive_text(heap, motive.value);
}

Str syntax_precedence_branch_text(Heap *heap, const SBranch *branch) {
  switch (branch->kind) {
    case SBRANCH_LEG:
      return SURFACE_CAT(heap, str_lit(" | "), big_to_string(heap, branch->index),
                         syntax_precedence_branch_binders(heap, &branch->binders), str_lit(" => "),
                         syntax_precedence_at_level(heap, 1, branch->body));
    case SBRANCH_CTOR:
      return SURFACE_CAT(heap, str_lit(" | "), branch->name, syntax_precedence_fields_text(heap, &branch->fields),
                         str_lit(" => "), syntax_precedence_at_level(heap, 1, branch->body));
  }
  return str_empty();
}

Str syntax_precedence_branches_text(Heap *heap, const Vec *branches) {
  Text text = text_new(heap);
  for (size_t i = 0; i < branches->len; i++) {
    const SBranch *branch = item_at(branches, i);
    if (branch) text_add(&text, syntax_precedence_branch_text(heap, branch));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_terms_text(Heap *heap, const Vec *terms) {
  Text text = text_new(heap);
  for (size_t i = 0; i < terms->len; i++) {
    const STerm *term = item_at(terms, i);
    if (i > 0) text_add(&text, str_lit(", "));
    if (term) text_add(&text, syntax_precedence_at_level(heap, 0, term));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_join_values(Heap *heap, const Vec *terms) {
  Text text = text_new(heap);
  for (size_t i = 0; i < terms->len; i++) {
    const STerm *term = item_at(terms, i);
    text_add(&text, str_lit(" "));
    if (term) text_add(&text, syntax_precedence_at_level(heap, 2, term));
  }
  return text_done(heap, &text);
}

Str syntax_precedence_raw(Heap *heap, const STerm *term) {
  switch (term->kind) {
    case STERM_VAR:
      return term->name;
    case STERM_NAT:
      return big_to_string(heap, term->nat);
    case STERM_PROP:
      return str_lit("Prop");
    case STERM_TYPE:
      return heap_str_cat(heap, str_lit("Type "), big_to_string(heap, term->nat));
    case STERM_SORT:
      return heap_str_cat(heap, str_lit("Sort "), universe_text(heap, term->sort));
    case STERM_PRIM:
      return syntax_prim_name(term->prim);
    case STERM_UNIT:
      return str_lit("()");
    case STERM_AUTO:
      return str_lit("auto");
    case STERM_PAIR:
      return SURFACE_CAT(heap, str_lit("("), syntax_precedence_at_level(heap, 0, term->left), str_lit(", "),
                         syntax_precedence_at_level(heap, 0, term->right), str_lit(")"));
    case STERM_TUPLE:
      return SURFACE_CAT(heap, str_lit("tuple ("), syntax_precedence_terms_text(heap, &term->items), str_lit(")"));
    case STERM_SUM:
      return SURFACE_CAT(heap, str_lit("sum ("), syntax_precedence_terms_text(heap, &term->items), str_lit(")"));
    case STERM_PROD:
      return SURFACE_CAT(heap, str_lit("prod ("), syntax_precedence_terms_text(heap, &term->items), str_lit(")"));
    case STERM_PROJ:
      return SURFACE_CAT(heap, syntax_precedence_at_level(heap, 2, term->head), str_lit("."),
                         big_to_string(heap, term->index));
    case STERM_INJ:
      return SURFACE_CAT(heap, str_lit("inj "), big_to_string(heap, term->index), str_lit(" of "),
                         big_to_string(heap, term->count), str_lit(" "), syntax_precedence_at_level(heap, 1, term->value));
    case STERM_ABSURD:
      return heap_str_cat(heap, str_lit("absurd "), syntax_precedence_at_level(heap, 1, term->value));
    case STERM_APP:
      return SURFACE_CAT(heap, syntax_precedence_at_level(heap, 1, term->head), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->arg));
    case STERM_FUN:
      return SURFACE_CAT(heap, str_lit("fun "), syntax_precedence_binders_text(heap, &term->binders), str_lit(" => "),
                         syntax_precedence_at_level(heap, 0, term->body));
    case STERM_ARROW:
      return SURFACE_CAT(heap, syntax_precedence_binder_text(heap, term->binder), str_lit(" -> "),
                         syntax_precedence_at_level(heap, 0, term->body));
    case STERM_STAR:
      return SURFACE_CAT(heap, syntax_precedence_binder_text(heap, term->binder), str_lit(" * "),
                         syntax_precedence_at_level(heap, 0, term->body));
    case STERM_ZK_TY:
      return SURFACE_CAT(heap, str_lit("zk "), syntax_precedence_binder_text(heap, term->binder), str_lit(" * "),
                         syntax_precedence_at_level(heap, 0, term->body));
    case STERM_PROVE:
      return SURFACE_CAT(heap, str_lit("prove "), syntax_precedence_at_level(heap, 2, term->key), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->witness), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->relation));
    case STERM_VERIFY:
      return SURFACE_CAT(heap, str_lit("verify "), syntax_precedence_at_level(heap, 2, term->key), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->proof));
    case STERM_FHC_TY:
      return SURFACE_CAT(heap, str_lit("fhc "), syntax_precedence_at_level(heap, 2, term->level), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->ty));
    case STERM_ENC:
      return SURFACE_CAT(heap, str_lit("enc "), syntax_precedence_at_level(heap, 2, term->key), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->value));
    case STERM_EVAL:
      return SURFACE_CAT(heap, str_lit("eval "), syntax_precedence_at_level(heap, 2, term->function), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->value));
    case STERM_DEC:
      return SURFACE_CAT(heap, str_lit("dec "), syntax_precedence_at_level(heap, 2, term->key), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->value));
    case STERM_MPC_TY:
      return SURFACE_CAT(heap, str_lit("mpc["), syntax_precedence_at_level(heap, 0, term->parties), str_lit(", "),
                         syntax_precedence_at_level(heap, 0, term->access), str_lit("] "),
                         syntax_precedence_at_level(heap, 2, term->ty));
    case STERM_SHARE:
      return heap_str_cat(heap, str_lit("share "), syntax_precedence_at_level(heap, 2, term->value));
    case STERM_INPUT:
      return SURFACE_CAT(heap, str_lit("input "), syntax_precedence_at_level(heap, 2, term->party), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->value));
    case STERM_JOIN:
      return SURFACE_CAT(heap, str_lit("mpc "), syntax_precedence_at_level(heap, 2, term->parties), str_lit(" "),
                         syntax_precedence_at_level(heap, 2, term->function),
                         syntax_precedence_join_values(heap, &term->items));
    case STERM_OPEN:
      return SURFACE_CAT(heap, str_lit("open "), syntax_precedence_at_level(heap, 2, term->parties), str_lit(" "),
                         syntax_optional_mark(term->quantity), syntax_precedence_at_level(heap, 2, term->proof),
                         str_lit(" "), syntax_precedence_at_level(heap, 2, term->value));
    case STERM_LET:
      return SURFACE_CAT(heap, str_lit("let "), term->name, str_lit(" : "), syntax_precedence_at_level(heap, 1, term->ty),
                         str_lit(" := "), syntax_precedence_at_level(heap, 1, term->value), str_lit(" in "),
                         syntax_precedence_at_level(heap, 0, term->body));
    case STERM_ANN:
      return SURFACE_CAT(heap, str_lit("("), syntax_precedence_at_level(heap, 0, term->value), str_lit(" : "),
                         syntax_precedence_at_level(heap, 0, term->ty), str_lit(")"));
    case STERM_CASE:
      return SURFACE_CAT(heap, str_lit("case "), syntax_precedence_at_level(heap, 1, term->scrut),
                         syntax_precedence_optional_motive(heap, term->motive), str_lit(" with"),
                         syntax_precedence_branches_text(heap, &term->branches));
    case STERM_MATCH:
      return SURFACE_CAT(heap, str_lit("match "), syntax_precedence_at_level(heap, 1, term->scrut),
                         syntax_precedence_optional_motive(heap, term->motive), str_lit(" with"),
                         syntax_precedence_branches_text(heap, &term->branches));
  }
  return str_empty();
}

Str syntax_precedence_wrap(Heap *heap, bool enough, Str text) {
  if (enough) return text;
  return SURFACE_CAT(heap, str_lit("("), text, str_lit(")"));
}

Str syntax_at(Heap *heap, Big level, const STerm *term) {
  return syntax_precedence_wrap(heap, big_le(level, big_of_u32(heap, syntax_level_of(term))),
                                syntax_precedence_raw(heap, term));
}
