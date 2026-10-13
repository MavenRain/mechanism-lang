/* Port of bend2/tests/relational_syntax_equal.bend: exact field-by-field syntax equality for the
 * parse/print protocol assertions. Every constructor and field in Syntax and Universe participates.
 * A library module (no main): c-build compiles it with -c and the S2.4 surface drivers link it. */
#include "relational_syntax_equal.h"

/* One list walk for every same_list_2_* law: equal length and pairwise equal items; two NULL items
 * are equal, one NULL item is not (totality only, no constructor produces one). */
typedef bool (*ItemEqual)(const void *a, const void *b);

static bool same_list(const Vec *a, const Vec *b, ItemEqual item) {
  if (a->len != b->len) return false;
  for (size_t i = 0; i < a->len; i++) {
    VecGet x = vec_get(a, i);
    VecGet y = vec_get(b, i);
    if (x.status != VEC_GET_OK || y.status != VEC_GET_OK) return false;
    if (x.value == NULL && y.value == NULL) continue;
    if (x.value == NULL || y.value == NULL) return false;
    if (!item(x.value, y.value)) return false;
  }
  return true;
}

static bool same_nat(Nat a, Nat b) { return nat_cmp(a, b) == 0; }

static bool item_s_decl(const void *a, const void *b) { return syntax_equal_same_s_decl(a, b); }
static bool item_s_t(const void *a, const void *b) { return syntax_equal_same_s_t(a, b); }
static bool item_s_binder_s_t(const void *a, const void *b) {
  return syntax_equal_same_s_binder_s_t(*(const SBinder *)a, *(const SBinder *)b);
}
static bool item_string(const void *a, const void *b) { return str_eq(*(const Str *)a, *(const Str *)b); }
static bool item_s_branch_s_t(const void *a, const void *b) { return syntax_equal_same_s_branch_s_t(a, b); }
static bool item_s_field_s_t(const void *a, const void *b) { return syntax_equal_same_s_field_s_t(a, b); }
static bool item_s_famctor_s_t(const void *a, const void *b) { return syntax_equal_same_s_famctor_s_t(a, b); }
static bool item_s_fam_s_t(const void *a, const void *b) { return syntax_equal_same_s_fam_s_t(a, b); }
static bool item_s_recdef_s_t(const void *a, const void *b) { return syntax_equal_same_s_recdef_s_t(a, b); }
static bool item_s_specialization(const void *a, const void *b) { return syntax_equal_same_s_specialization(a, b); }
static bool item_u_t(const void *a, const void *b) { return syntax_equal_same_u_t(a, b); }
static bool item_c_pair_string_string(const void *a, const void *b) {
  return syntax_equal_same_c_pair_string_string(a, b);
}

bool syntax_equal_same_list_2_s_decl(const Vec *a, const Vec *b) { return same_list(a, b, item_s_decl); }

bool syntax_equal_same_s_decl(const SDecl *a, const SDecl *b) {
  if (a == NULL || b == NULL) return a == b;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case SDECL_POLY:
      return same_nat(a->universes, b->universes) && str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty) &&
             syntax_equal_same_s_t(a->body, b->body);
    case SDECL_POLY_MU:
      return same_nat(a->universes, b->universes) && syntax_equal_same_s_fam_s_t(&a->family, &b->family);
    case SDECL_POLY_GROUP:
      return same_nat(a->universes, b->universes) && syntax_equal_same_s_fam_s_t(&a->family, &b->family) &&
             syntax_equal_same_list_2_s_fam_s_t(&a->families, &b->families) &&
             syntax_equal_same_list_2_s_recdef_s_t(&a->helpers, &b->helpers);
    case SDECL_POLY_COMPOSE:
      return same_nat(a->universes, b->universes) && str_eq(a->name, b->name) &&
             syntax_equal_same_list_2_s_specialization(&a->specializations, &b->specializations) &&
             syntax_equal_same_list_2_s_recdef_s_t(&a->helpers, &b->helpers);
    case SDECL_SPECIALIZE:
      return syntax_equal_same_s_specialization(&a->specialization, &b->specialization);
    case SDECL_DEF:
      return str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty) && syntax_equal_same_s_t(a->body, b->body);
    case SDECL_AXIOM:
      return str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty);
    case SDECL_MU:
      return syntax_equal_same_list_2_s_fam_s_t(&a->families, &b->families);
    case SDECL_REC:
      return syntax_equal_same_list_2_s_recdef_s_t(&a->definitions, &b->definitions);
  }
  return false;
}

bool syntax_equal_same_s_t(const STerm *a, const STerm *b) {
  if (a == NULL || b == NULL) return a == b;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case STERM_VAR:
      return str_eq(a->name, b->name);
    case STERM_NAT:
      return big_equal(a->nat, b->nat);
    case STERM_PROP:
      return true;
    case STERM_TYPE:
      return big_equal(a->nat, b->nat);
    case STERM_SORT:
      return syntax_equal_same_u_t(a->sort, b->sort);
    case STERM_PRIM:
      return syntax_equal_same_s_prim(a->prim, b->prim);
    case STERM_UNIT:
      return true;
    case STERM_AUTO:
      return true;
    case STERM_PAIR:
      return syntax_equal_same_s_t(a->left, b->left) && syntax_equal_same_s_t(a->right, b->right);
    case STERM_TUPLE:
    case STERM_SUM:
    case STERM_PROD:
      return syntax_equal_same_list_2_s_t(&a->items, &b->items);
    case STERM_PROJ:
      return syntax_equal_same_s_t(a->head, b->head) && big_equal(a->index, b->index);
    case STERM_INJ:
      return big_equal(a->index, b->index) && big_equal(a->count, b->count) && syntax_equal_same_s_t(a->value, b->value);
    case STERM_ABSURD:
      return syntax_equal_same_s_t(a->value, b->value);
    case STERM_APP:
      return syntax_equal_same_s_t(a->head, b->head) && syntax_equal_same_s_t(a->arg, b->arg);
    case STERM_FUN:
      return syntax_equal_same_list_2_s_binder_s_t(&a->binders, &b->binders) && syntax_equal_same_s_t(a->body, b->body);
    case STERM_ARROW:
    case STERM_STAR:
    case STERM_ZK_TY:
      return syntax_equal_same_s_binder_s_t(a->binder, b->binder) && syntax_equal_same_s_t(a->body, b->body);
    case STERM_PROVE:
      return syntax_equal_same_s_t(a->key, b->key) && syntax_equal_same_s_t(a->witness, b->witness) &&
             syntax_equal_same_s_t(a->relation, b->relation);
    case STERM_VERIFY:
      return syntax_equal_same_s_t(a->key, b->key) && syntax_equal_same_s_t(a->proof, b->proof);
    case STERM_FHC_TY:
      return syntax_equal_same_s_t(a->level, b->level) && syntax_equal_same_s_t(a->ty, b->ty);
    case STERM_ENC:
    case STERM_DEC:
      return syntax_equal_same_s_t(a->key, b->key) && syntax_equal_same_s_t(a->value, b->value);
    case STERM_EVAL:
      return syntax_equal_same_s_t(a->function, b->function) && syntax_equal_same_s_t(a->value, b->value);
    case STERM_MPC_TY:
      return syntax_equal_same_s_t(a->parties, b->parties) && syntax_equal_same_s_t(a->access, b->access) &&
             syntax_equal_same_s_t(a->ty, b->ty);
    case STERM_SHARE:
      return syntax_equal_same_s_t(a->value, b->value);
    case STERM_INPUT:
      return syntax_equal_same_s_t(a->party, b->party) && syntax_equal_same_s_t(a->value, b->value);
    case STERM_JOIN:
      return syntax_equal_same_s_t(a->parties, b->parties) && syntax_equal_same_s_t(a->function, b->function) &&
             syntax_equal_same_list_2_s_t(&a->items, &b->items);
    case STERM_OPEN:
      return syntax_equal_same_maybe_2_q_t(a->quantity, b->quantity) && syntax_equal_same_s_t(a->parties, b->parties) &&
             syntax_equal_same_s_t(a->proof, b->proof) && syntax_equal_same_s_t(a->value, b->value);
    case STERM_LET:
      return str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty) && syntax_equal_same_s_t(a->value, b->value) &&
             syntax_equal_same_s_t(a->body, b->body);
    case STERM_ANN:
      return syntax_equal_same_s_t(a->value, b->value) && syntax_equal_same_s_t(a->ty, b->ty);
    case STERM_CASE:
    case STERM_MATCH:
      return syntax_equal_same_s_t(a->scrut, b->scrut) && syntax_equal_same_maybe_2_s_motive_s_t(a->motive, b->motive) &&
             syntax_equal_same_list_2_s_branch_s_t(&a->branches, &b->branches);
  }
  return false;
}

bool syntax_equal_same_u_t(const Universe *a, const Universe *b) {
  if (a == NULL || b == NULL) return a == b;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case UNIVERSE_NAT_LEVEL:
      return big_equal(a->value, b->value);
    case UNIVERSE_VAR_LEVEL:
      return same_nat(a->index, b->index);
    case UNIVERSE_SUCC_LEVEL:
      return syntax_equal_same_u_t(a->left, b->left);
    case UNIVERSE_MAX_LEVEL:
    case UNIVERSE_IMAX_LEVEL:
      return syntax_equal_same_u_t(a->left, b->left) && syntax_equal_same_u_t(a->right, b->right);
  }
  return false;
}

bool syntax_equal_same_s_prim(SPrim a, SPrim b) {
  switch (a) {
    case SPRIM_ADD:
      return b == SPRIM_ADD;
    case SPRIM_SUB:
      return b == SPRIM_SUB;
    case SPRIM_MUL:
      return b == SPRIM_MUL;
    case SPRIM_EQ:
      return b == SPRIM_EQ;
    case SPRIM_LT:
      return b == SPRIM_LT;
  }
  return false;
}

bool syntax_equal_same_list_2_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_t); }

bool syntax_equal_same_list_2_s_binder_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_binder_s_t); }

bool syntax_equal_same_s_binder_s_t(SBinder a, SBinder b) {
  return quantity_equal(a.quantity, b.quantity) && str_eq(a.name, b.name) && syntax_equal_same_s_t(a.ty, b.ty);
}

bool syntax_equal_same_maybe_2_q_t(MaybeQuantity a, MaybeQuantity b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return quantity_equal(a.value, b.value);
}

bool syntax_equal_same_maybe_2_s_motive_s_t(MaybeSMotive a, MaybeSMotive b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return syntax_equal_same_s_motive_s_t(a.value, b.value);
}

bool syntax_equal_same_s_motive_s_t(SMotive a, SMotive b) {
  return str_eq(a.self, b.self) && syntax_equal_same_maybe_2_string(a.ind, b.ind) &&
         syntax_equal_same_list_2_string(&a.indices, &b.indices) && syntax_equal_same_s_t(a.body, b.body);
}

bool syntax_equal_same_maybe_2_string(MaybeStr a, MaybeStr b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return str_eq(a.value, b.value);
}

bool syntax_equal_same_list_2_string(const Vec *a, const Vec *b) { return same_list(a, b, item_string); }

bool syntax_equal_same_list_2_s_branch_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_branch_s_t); }

bool syntax_equal_same_s_branch_s_t(const SBranch *a, const SBranch *b) {
  if (a == NULL || b == NULL) return a == b;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case SBRANCH_LEG:
      return big_equal(a->index, b->index) && syntax_equal_same_list_2_s_binder_s_t(&a->binders, &b->binders) &&
             syntax_equal_same_s_t(a->body, b->body);
    case SBRANCH_CTOR:
      return str_eq(a->name, b->name) && syntax_equal_same_list_2_s_field_s_t(&a->fields, &b->fields) &&
             syntax_equal_same_s_t(a->body, b->body);
  }
  return false;
}

bool syntax_equal_same_list_2_s_field_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_field_s_t); }

bool syntax_equal_same_s_field_s_t(const SField *a, const SField *b) {
  if (a == NULL || b == NULL) return a == b;
  return quantity_equal(a->quantity, b->quantity) && str_eq(a->name, b->name) &&
         syntax_equal_same_maybe_2_s_t(a->ty, b->ty);
}

bool syntax_equal_same_maybe_2_s_t(MaybeSTerm a, MaybeSTerm b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return syntax_equal_same_s_t(a.value, b.value);
}

bool syntax_equal_same_s_fam_s_t(const SFam *a, const SFam *b) {
  if (a == NULL || b == NULL) return a == b;
  return str_eq(a->name, b->name) && syntax_equal_same_list_2_s_binder_s_t(&a->params, &b->params) &&
         syntax_equal_same_s_t(a->ty, b->ty) && syntax_equal_same_list_2_s_famctor_s_t(&a->ctors, &b->ctors);
}

bool syntax_equal_same_list_2_s_famctor_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_famctor_s_t); }

bool syntax_equal_same_s_famctor_s_t(const SFamCtor *a, const SFamCtor *b) {
  if (a == NULL || b == NULL) return a == b;
  return str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty);
}

bool syntax_equal_same_list_2_s_fam_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_fam_s_t); }

bool syntax_equal_same_list_2_s_recdef_s_t(const Vec *a, const Vec *b) { return same_list(a, b, item_s_recdef_s_t); }

bool syntax_equal_same_s_recdef_s_t(const SRecDef *a, const SRecDef *b) {
  if (a == NULL || b == NULL) return a == b;
  return str_eq(a->name, b->name) && syntax_equal_same_s_t(a->ty, b->ty) && syntax_equal_same_s_t(a->body, b->body);
}

bool syntax_equal_same_list_2_s_specialization(const Vec *a, const Vec *b) {
  return same_list(a, b, item_s_specialization);
}

bool syntax_equal_same_s_specialization(const SSpecialization *a, const SSpecialization *b) {
  if (a == NULL || b == NULL) return a == b;
  return str_eq(a->template, b->template) && syntax_equal_same_list_2_u_t(&a->levels, &b->levels) &&
         str_eq(a->name, b->name) && syntax_equal_same_list_2_c_pair_string_string(&a->names, &b->names) &&
         syntax_equal_same_maybe_2_list_2_c_pair_string_string(a->helpers, b->helpers);
}

bool syntax_equal_same_list_2_u_t(const Vec *a, const Vec *b) { return same_list(a, b, item_u_t); }

bool syntax_equal_same_list_2_c_pair_string_string(const Vec *a, const Vec *b) {
  return same_list(a, b, item_c_pair_string_string);
}

bool syntax_equal_same_c_pair_string_string(const PolyRename *a, const PolyRename *b) {
  if (a == NULL || b == NULL) return a == b;
  return str_eq(a->source, b->source) && str_eq(a->target, b->target);
}

bool syntax_equal_same_maybe_2_list_2_c_pair_string_string(MaybeVec a, MaybeVec b) {
  if (a.some != b.some) return false;
  if (!a.some) return true;
  return syntax_equal_same_list_2_c_pair_string_string(&a.value, &b.value);
}

bool syntax_equal_declarations(const Vec *a, const Vec *b) { return syntax_equal_same_list_2_s_decl(a, b); }
