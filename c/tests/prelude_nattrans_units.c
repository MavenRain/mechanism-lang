/* Port of tests/prelude_nattrans_units.bend (122 lines): the nattrans units prelude suite. Reads the prelude
 * layers and the unit fixture clients under argv[1] (default "."), checks the parse roundtrip, the unit heads, the case
 * bindings and the two whisker law statements on the parsed declarations, then the entry inventory, the probes and the
 * negatives, and prints PRELUDE-NATTRANS-UNITS-OK fields or PRELUDE-NATTRANS-UNITS-FAIL message. Same definitions in
 * the same order. */
#include "driver.h"
#include "relational_common.h"
#include "relational_negatives.h"
#include "relational_program.h"
#include "relational_text.h"

static Str cat(Heap *heap, Str a, Str b) { return str_cat(heap->arena, a, b); }

/* A List<String> literal as a Vec of boxed Str. */
static Vec strings(Heap *heap, const char *const *items, size_t count) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < count; i++) vec_push(&out, relational_str(heap, str_lit(items[i])));
  return out;
}

#define STRINGS(heap, items) strings(heap, items, sizeof items / sizeof items[0])

/* C.Pair<String, S.T>: one helper name and its declared type. */
typedef struct {
  Str name;
  const STerm *ty;
} TypeRow;

/* C.Pair<String, String> literal rows of the head_pins and case_pins calls in suite(). */
typedef struct {
  const char *name;
  const char *expected;
} PinRow;

/* law_at(level, term) */
static Str law_at(Heap *heap, uint32_t level, const STerm *term) {
  return syntax_at(heap, big_of_u32(heap, level), term);
}

/* law_lookup(types, name): the first row named name, or NULL for None. */
static const STerm *law_lookup(const Vec *types, Str name) {
  for (size_t i = 0; i < types->len; i++) {
    VecGet got = vec_get(types, i);
    if (got.status != VEC_GET_OK || got.value == NULL) continue;
    const TypeRow *row = got.value;
    if (str_eq(row->name, name)) return row->ty;
  }
  return NULL;
}

/* law_found(found, name, expected) */
static Task law_found(Heap *heap, const STerm *found, Str name, Str expected) {
  if (found == NULL) return task_fail(cat(heap, str_lit("missing whisker law: "), name));
  Str actual = law_at(heap, 0, found);
  return relational_require(str_eq(text_normalise(heap, actual), expected),
                            cat(heap, str_lit("whisker law restated: "), name));
}

/* law_statement(types, name, expected) */
static Task law_statement(Heap *heap, const Vec *types, Str name, Str expected) {
  return law_found(heap, law_lookup(types, name), name, expected);
}

/* definition(declaration), appended to out: one row per S.DDef. */
static void definition(Heap *heap, Vec *out, const SDecl *declaration) {
  TextRename *row = NULL;
  switch (declaration->kind) {
    case SDECL_DEF:
      row = heap_alloc(heap, sizeof *row);
      if (row == NULL) return;
      row->name = declaration->name;
      row->value = syntax_show(heap, declaration->body);
      vec_push(out, row);
      return;
    case SDECL_POLY:
    case SDECL_POLY_MU:
    case SDECL_POLY_GROUP:
    case SDECL_POLY_COMPOSE:
    case SDECL_SPECIALIZE:
    case SDECL_AXIOM:
    case SDECL_MU:
    case SDECL_REC:
      return;
  }
}

/* definitions(declarations) */
static Vec definitions(Heap *heap, const Vec *declarations) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < declarations->len; i++) {
    VecGet got = vec_get(declarations, i);
    if (got.status != VEC_GET_OK || got.value == NULL) continue;
    definition(heap, &out, got.value);
  }
  return out;
}

/* helper_types(helpers), appended to out */
static void helper_types(Heap *heap, Vec *out, const Vec *helpers) {
  for (size_t i = 0; i < helpers->len; i++) {
    VecGet got = vec_get(helpers, i);
    if (got.status != VEC_GET_OK || got.value == NULL) continue;
    const SRecDef *helper = got.value;
    TypeRow *row = heap_alloc(heap, sizeof *row);
    if (row == NULL) return;
    row->name = helper->name;
    row->ty = helper->ty;
    vec_push(out, row);
  }
}

/* selected_types(name, helpers), appended to out */
static void selected_types(Heap *heap, Vec *out, Str name, const Vec *helpers) {
  if (str_eq(name, str_lit("MechNatTransUnits"))) helper_types(heap, out, helpers);
}

/* template(declaration), appended to out */
static void template(Heap *heap, Vec *out, const SDecl *declaration) {
  switch (declaration->kind) {
    case SDECL_POLY_COMPOSE:
      selected_types(heap, out, declaration->name, &declaration->helpers);
      return;
    case SDECL_POLY_GROUP:
      selected_types(heap, out, declaration->family.name, &declaration->helpers);
      return;
    case SDECL_POLY:
    case SDECL_POLY_MU:
    case SDECL_SPECIALIZE:
    case SDECL_DEF:
    case SDECL_AXIOM:
    case SDECL_MU:
    case SDECL_REC:
      return;
  }
}

/* templates(declarations) */
static Vec templates(Heap *heap, const Vec *declarations) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < declarations->len; i++) {
    VecGet got = vec_get(declarations, i);
    if (got.status != VEC_GET_OK || got.value == NULL) continue;
    template(heap, &out, got.value);
  }
  return out;
}

/* first_is(tokens, expected) */
static bool first_is(const Vec *tokens, Str expected) {
  VecGet got = vec_get(tokens, 0);
  if (got.status != VEC_GET_OK || got.value == NULL) return false;
  return str_eq(*(const Str *)got.value, expected);
}

/* head_pins(pins, bodies) */
static Task head_pins(Heap *heap, const PinRow *pins, size_t count, const Vec *bodies) {
  for (size_t i = 0; i < count; i++) {
    Str name = str_lit(pins[i].name);
    Vec identifiers = text_identifiers(heap, text_lookup(bodies, name));
    RELATIONAL_TRY(relational_require(first_is(&identifiers, str_lit(pins[i].expected)),
                                      cat(heap, str_lit("unit head changed: "), name)));
  }
  return task_pure();
}

/* case_pins(pins, bodies) */
static Task case_pins(Heap *heap, const PinRow *pins, size_t count, const Vec *bodies) {
  for (size_t i = 0; i < count; i++) {
    Str name = str_lit(pins[i].name);
    Str at = cat(heap, name, str_lit("UnitAt"));
    Vec identifiers = text_identifiers(heap, text_lookup(bodies, at));
    bool expected = relational_contains(&identifiers, str_lit(pins[i].expected));
    bool law = relational_contains(&identifiers, cat(heap, name, str_lit("UnitLaw")));
    RELATIONAL_TRY(relational_require(expected && law, cat(heap, str_lit("case rebound: "), at)));
  }
  return task_pure();
}

/* expected_entries() (tests/prelude_nattrans_units.bend line 81, 1281 names) */
static const char *const EXPECTED_ENTRIES[] = {
    "Alpha", "Beta", "BetaComponents", "Components", "Constant", "DoubleEnd", "IndexedEnd", "LeftUnit", "PostUnit",
    "PreUnit", "RightUnit", "Run_Base_Functor", "Run_Base_Source_Category", "Run_Base_Source_Hom",
    "Run_Base_Source_assoc", "Run_Base_Source_comp", "Run_Base_Source_compId", "Run_Base_Source_eqCongr",
    "Run_Base_Source_eqTrans", "Run_Base_Source_id", "Run_Base_Source_idComp", "Run_Base_Target_Category",
    "Run_Base_Target_Hom", "Run_Base_Target_assoc", "Run_Base_Target_comp", "Run_Base_Target_compId",
    "Run_Base_Target_eqCongr", "Run_Base_Target_eqTrans", "Run_Base_Target_id", "Run_Base_Target_idComp",
    "Run_Base_eqCongr", "Run_Base_functorMap", "Run_Base_functorMapComp", "Run_Base_functorMapId",
    "Run_Base_functorObj", "Run_NatTrans", "Run_idNat", "Run_natApp", "Run_naturality", "Run_targetEqSymm",
    "Run_vcomp", "Run_vcompLaw", "SeparateTarget_Category", "SeparateTarget_Hom", "SeparateTarget_assoc",
    "SeparateTarget_comp", "SeparateTarget_compId", "SeparateTarget_eqCongr", "SeparateTarget_eqTrans",
    "SeparateTarget_id", "SeparateTarget_idComp", "UnitsMixed_After_Compose_Base_Composite_Functor",
    "UnitsMixed_After_Compose_Base_Composite_eqCongr", "UnitsMixed_After_Compose_Base_Composite_functorMap",
    "UnitsMixed_After_Compose_Base_Composite_functorMapComp", "UnitsMixed_After_Compose_Base_Composite_functorMapId",
    "UnitsMixed_After_Compose_Base_Composite_functorObj", "UnitsMixed_After_Compose_Base_First_Functor",
    "UnitsMixed_After_Compose_Base_First_eqCongr", "UnitsMixed_After_Compose_Base_First_functorMap",
    "UnitsMixed_After_Compose_Base_First_functorMapComp", "UnitsMixed_After_Compose_Base_First_functorMapId",
    "UnitsMixed_After_Compose_Base_First_functorObj", "UnitsMixed_After_Compose_Base_Middle_Category",
    "UnitsMixed_After_Compose_Base_Middle_Hom", "UnitsMixed_After_Compose_Base_Middle_assoc",
    "UnitsMixed_After_Compose_Base_Middle_comp", "UnitsMixed_After_Compose_Base_Middle_compId",
    "UnitsMixed_After_Compose_Base_Middle_eqCongr", "UnitsMixed_After_Compose_Base_Middle_eqTrans",
    "UnitsMixed_After_Compose_Base_Middle_id", "UnitsMixed_After_Compose_Base_Middle_idComp",
    "UnitsMixed_After_Compose_Base_Second_Functor", "UnitsMixed_After_Compose_Base_Second_eqCongr",
    "UnitsMixed_After_Compose_Base_Second_functorMap", "UnitsMixed_After_Compose_Base_Second_functorMapComp",
    "UnitsMixed_After_Compose_Base_Second_functorMapId", "UnitsMixed_After_Compose_Base_Second_functorObj",
    "UnitsMixed_After_Compose_Base_Source_Category", "UnitsMixed_After_Compose_Base_Source_Hom",
    "UnitsMixed_After_Compose_Base_Source_assoc", "UnitsMixed_After_Compose_Base_Source_comp",
    "UnitsMixed_After_Compose_Base_Source_compId", "UnitsMixed_After_Compose_Base_Source_eqCongr",
    "UnitsMixed_After_Compose_Base_Source_eqTrans", "UnitsMixed_After_Compose_Base_Source_id",
    "UnitsMixed_After_Compose_Base_Source_idComp", "UnitsMixed_After_Compose_Base_Target_Category",
    "UnitsMixed_After_Compose_Base_Target_Hom", "UnitsMixed_After_Compose_Base_Target_assoc",
    "UnitsMixed_After_Compose_Base_Target_comp", "UnitsMixed_After_Compose_Base_Target_compId",
    "UnitsMixed_After_Compose_Base_Target_eqCongr", "UnitsMixed_After_Compose_Base_Target_eqTrans",
    "UnitsMixed_After_Compose_Base_Target_id", "UnitsMixed_After_Compose_Base_Target_idComp",
    "UnitsMixed_After_Compose_Base_compFunctor", "UnitsMixed_After_Compose_Composite_NatTrans",
    "UnitsMixed_After_Compose_Composite_natApp", "UnitsMixed_After_Compose_Composite_naturality",
    "UnitsMixed_After_Compose_First_NatTrans", "UnitsMixed_After_Compose_First_natApp",
    "UnitsMixed_After_Compose_First_naturality", "UnitsMixed_After_Compose_Second_NatTrans",
    "UnitsMixed_After_Compose_Second_natApp", "UnitsMixed_After_Compose_Second_naturality",
    "UnitsMixed_After_Compose_targetEqSymm", "UnitsMixed_After_Compose_whiskerLeft",
    "UnitsMixed_After_Compose_whiskerRight", "UnitsMixed_After_Composite_Base_Functor",
    "UnitsMixed_After_Composite_Base_Source_Category", "UnitsMixed_After_Composite_Base_Source_Hom",
    "UnitsMixed_After_Composite_Base_Source_assoc", "UnitsMixed_After_Composite_Base_Source_comp",
    "UnitsMixed_After_Composite_Base_Source_compId", "UnitsMixed_After_Composite_Base_Source_eqCongr",
    "UnitsMixed_After_Composite_Base_Source_eqTrans", "UnitsMixed_After_Composite_Base_Source_id",
    "UnitsMixed_After_Composite_Base_Source_idComp", "UnitsMixed_After_Composite_Base_Target_Category",
    "UnitsMixed_After_Composite_Base_Target_Hom", "UnitsMixed_After_Composite_Base_Target_assoc",
    "UnitsMixed_After_Composite_Base_Target_comp", "UnitsMixed_After_Composite_Base_Target_compId",
    "UnitsMixed_After_Composite_Base_Target_eqCongr", "UnitsMixed_After_Composite_Base_Target_eqTrans",
    "UnitsMixed_After_Composite_Base_Target_id", "UnitsMixed_After_Composite_Base_Target_idComp",
    "UnitsMixed_After_Composite_Base_eqCongr", "UnitsMixed_After_Composite_Base_functorMap",
    "UnitsMixed_After_Composite_Base_functorMapComp", "UnitsMixed_After_Composite_Base_functorMapId",
    "UnitsMixed_After_Composite_Base_functorObj", "UnitsMixed_After_Composite_NatTrans",
    "UnitsMixed_After_Composite_idNat", "UnitsMixed_After_Composite_natApp", "UnitsMixed_After_Composite_naturality",
    "UnitsMixed_After_Composite_targetEqSymm", "UnitsMixed_After_Composite_vcomp",
    "UnitsMixed_After_Composite_vcompLaw", "UnitsMixed_After_First_Base_Functor",
    "UnitsMixed_After_First_Base_Source_Category", "UnitsMixed_After_First_Base_Source_Hom",
    "UnitsMixed_After_First_Base_Source_assoc", "UnitsMixed_After_First_Base_Source_comp",
    "UnitsMixed_After_First_Base_Source_compId", "UnitsMixed_After_First_Base_Source_eqCongr",
    "UnitsMixed_After_First_Base_Source_eqTrans", "UnitsMixed_After_First_Base_Source_id",
    "UnitsMixed_After_First_Base_Source_idComp", "UnitsMixed_After_First_Base_Target_Category",
    "UnitsMixed_After_First_Base_Target_Hom", "UnitsMixed_After_First_Base_Target_assoc",
    "UnitsMixed_After_First_Base_Target_comp", "UnitsMixed_After_First_Base_Target_compId",
    "UnitsMixed_After_First_Base_Target_eqCongr", "UnitsMixed_After_First_Base_Target_eqTrans",
    "UnitsMixed_After_First_Base_Target_id", "UnitsMixed_After_First_Base_Target_idComp",
    "UnitsMixed_After_First_Base_eqCongr", "UnitsMixed_After_First_Base_functorMap",
    "UnitsMixed_After_First_Base_functorMapComp", "UnitsMixed_After_First_Base_functorMapId",
    "UnitsMixed_After_First_Base_functorObj", "UnitsMixed_After_First_NatTrans", "UnitsMixed_After_First_idNat",
    "UnitsMixed_After_First_natApp", "UnitsMixed_After_First_naturality", "UnitsMixed_After_First_targetEqSymm",
    "UnitsMixed_After_First_vcomp", "UnitsMixed_After_First_vcompLaw", "UnitsMixed_After_Middle_Category",
    "UnitsMixed_After_Middle_Hom", "UnitsMixed_After_Middle_assoc", "UnitsMixed_After_Middle_comp",
    "UnitsMixed_After_Middle_compId", "UnitsMixed_After_Middle_eqCongr", "UnitsMixed_After_Middle_eqTrans",
    "UnitsMixed_After_Middle_id", "UnitsMixed_After_Middle_idComp", "UnitsMixed_After_Second_Base_Functor",
    "UnitsMixed_After_Second_Base_Source_Category", "UnitsMixed_After_Second_Base_Source_Hom",
    "UnitsMixed_After_Second_Base_Source_assoc", "UnitsMixed_After_Second_Base_Source_comp",
    "UnitsMixed_After_Second_Base_Source_compId", "UnitsMixed_After_Second_Base_Source_eqCongr",
    "UnitsMixed_After_Second_Base_Source_eqTrans", "UnitsMixed_After_Second_Base_Source_id",
    "UnitsMixed_After_Second_Base_Source_idComp", "UnitsMixed_After_Second_Base_Target_Category",
    "UnitsMixed_After_Second_Base_Target_Hom", "UnitsMixed_After_Second_Base_Target_assoc",
    "UnitsMixed_After_Second_Base_Target_comp", "UnitsMixed_After_Second_Base_Target_compId",
    "UnitsMixed_After_Second_Base_Target_eqCongr", "UnitsMixed_After_Second_Base_Target_eqTrans",
    "UnitsMixed_After_Second_Base_Target_id", "UnitsMixed_After_Second_Base_Target_idComp",
    "UnitsMixed_After_Second_Base_eqCongr", "UnitsMixed_After_Second_Base_functorMap",
    "UnitsMixed_After_Second_Base_functorMapComp", "UnitsMixed_After_Second_Base_functorMapId",
    "UnitsMixed_After_Second_Base_functorObj", "UnitsMixed_After_Second_NatTrans", "UnitsMixed_After_Second_idNat",
    "UnitsMixed_After_Second_natApp", "UnitsMixed_After_Second_naturality", "UnitsMixed_After_Second_targetEqSymm",
    "UnitsMixed_After_Second_vcomp", "UnitsMixed_After_Second_vcompLaw", "UnitsMixed_After_Source_Category",
    "UnitsMixed_After_Source_Hom", "UnitsMixed_After_Source_assoc", "UnitsMixed_After_Source_comp",
    "UnitsMixed_After_Source_compId", "UnitsMixed_After_Source_eqCongr", "UnitsMixed_After_Source_eqTrans",
    "UnitsMixed_After_Source_id", "UnitsMixed_After_Source_idComp", "UnitsMixed_After_Target_Category",
    "UnitsMixed_After_Target_Hom", "UnitsMixed_After_Target_assoc", "UnitsMixed_After_Target_comp",
    "UnitsMixed_After_Target_compId", "UnitsMixed_After_Target_eqCongr", "UnitsMixed_After_Target_eqTrans",
    "UnitsMixed_After_Target_id", "UnitsMixed_After_Target_idComp", "UnitsMixed_After_hcomp",
    "UnitsMixed_Before_Compose_Base_Composite_Functor", "UnitsMixed_Before_Compose_Base_Composite_eqCongr",
    "UnitsMixed_Before_Compose_Base_Composite_functorMap", "UnitsMixed_Before_Compose_Base_Composite_functorMapComp",
    "UnitsMixed_Before_Compose_Base_Composite_functorMapId", "UnitsMixed_Before_Compose_Base_Composite_functorObj",
    "UnitsMixed_Before_Compose_Base_First_Functor", "UnitsMixed_Before_Compose_Base_First_eqCongr",
    "UnitsMixed_Before_Compose_Base_First_functorMap", "UnitsMixed_Before_Compose_Base_First_functorMapComp",
    "UnitsMixed_Before_Compose_Base_First_functorMapId", "UnitsMixed_Before_Compose_Base_First_functorObj",
    "UnitsMixed_Before_Compose_Base_Middle_Category", "UnitsMixed_Before_Compose_Base_Middle_Hom",
    "UnitsMixed_Before_Compose_Base_Middle_assoc", "UnitsMixed_Before_Compose_Base_Middle_comp",
    "UnitsMixed_Before_Compose_Base_Middle_compId", "UnitsMixed_Before_Compose_Base_Middle_eqCongr",
    "UnitsMixed_Before_Compose_Base_Middle_eqTrans", "UnitsMixed_Before_Compose_Base_Middle_id",
    "UnitsMixed_Before_Compose_Base_Middle_idComp", "UnitsMixed_Before_Compose_Base_Second_Functor",
    "UnitsMixed_Before_Compose_Base_Second_eqCongr", "UnitsMixed_Before_Compose_Base_Second_functorMap",
    "UnitsMixed_Before_Compose_Base_Second_functorMapComp", "UnitsMixed_Before_Compose_Base_Second_functorMapId",
    "UnitsMixed_Before_Compose_Base_Second_functorObj", "UnitsMixed_Before_Compose_Base_Source_Category",
    "UnitsMixed_Before_Compose_Base_Source_Hom", "UnitsMixed_Before_Compose_Base_Source_assoc",
    "UnitsMixed_Before_Compose_Base_Source_comp", "UnitsMixed_Before_Compose_Base_Source_compId",
    "UnitsMixed_Before_Compose_Base_Source_eqCongr", "UnitsMixed_Before_Compose_Base_Source_eqTrans",
    "UnitsMixed_Before_Compose_Base_Source_id", "UnitsMixed_Before_Compose_Base_Source_idComp",
    "UnitsMixed_Before_Compose_Base_Target_Category", "UnitsMixed_Before_Compose_Base_Target_Hom",
    "UnitsMixed_Before_Compose_Base_Target_assoc", "UnitsMixed_Before_Compose_Base_Target_comp",
    "UnitsMixed_Before_Compose_Base_Target_compId", "UnitsMixed_Before_Compose_Base_Target_eqCongr",
    "UnitsMixed_Before_Compose_Base_Target_eqTrans", "UnitsMixed_Before_Compose_Base_Target_id",
    "UnitsMixed_Before_Compose_Base_Target_idComp", "UnitsMixed_Before_Compose_Base_compFunctor",
    "UnitsMixed_Before_Compose_Composite_NatTrans", "UnitsMixed_Before_Compose_Composite_natApp",
    "UnitsMixed_Before_Compose_Composite_naturality", "UnitsMixed_Before_Compose_First_NatTrans",
    "UnitsMixed_Before_Compose_First_natApp", "UnitsMixed_Before_Compose_First_naturality",
    "UnitsMixed_Before_Compose_Second_NatTrans", "UnitsMixed_Before_Compose_Second_natApp",
    "UnitsMixed_Before_Compose_Second_naturality", "UnitsMixed_Before_Compose_targetEqSymm",
    "UnitsMixed_Before_Compose_whiskerLeft", "UnitsMixed_Before_Compose_whiskerRight",
    "UnitsMixed_Before_Composite_Base_Functor", "UnitsMixed_Before_Composite_Base_Source_Category",
    "UnitsMixed_Before_Composite_Base_Source_Hom", "UnitsMixed_Before_Composite_Base_Source_assoc",
    "UnitsMixed_Before_Composite_Base_Source_comp", "UnitsMixed_Before_Composite_Base_Source_compId",
    "UnitsMixed_Before_Composite_Base_Source_eqCongr", "UnitsMixed_Before_Composite_Base_Source_eqTrans",
    "UnitsMixed_Before_Composite_Base_Source_id", "UnitsMixed_Before_Composite_Base_Source_idComp",
    "UnitsMixed_Before_Composite_Base_Target_Category", "UnitsMixed_Before_Composite_Base_Target_Hom",
    "UnitsMixed_Before_Composite_Base_Target_assoc", "UnitsMixed_Before_Composite_Base_Target_comp",
    "UnitsMixed_Before_Composite_Base_Target_compId", "UnitsMixed_Before_Composite_Base_Target_eqCongr",
    "UnitsMixed_Before_Composite_Base_Target_eqTrans", "UnitsMixed_Before_Composite_Base_Target_id",
    "UnitsMixed_Before_Composite_Base_Target_idComp", "UnitsMixed_Before_Composite_Base_eqCongr",
    "UnitsMixed_Before_Composite_Base_functorMap", "UnitsMixed_Before_Composite_Base_functorMapComp",
    "UnitsMixed_Before_Composite_Base_functorMapId", "UnitsMixed_Before_Composite_Base_functorObj",
    "UnitsMixed_Before_Composite_NatTrans", "UnitsMixed_Before_Composite_idNat",
    "UnitsMixed_Before_Composite_natApp", "UnitsMixed_Before_Composite_naturality",
    "UnitsMixed_Before_Composite_targetEqSymm", "UnitsMixed_Before_Composite_vcomp",
    "UnitsMixed_Before_Composite_vcompLaw", "UnitsMixed_Before_First_Base_Functor",
    "UnitsMixed_Before_First_Base_Source_Category", "UnitsMixed_Before_First_Base_Source_Hom",
    "UnitsMixed_Before_First_Base_Source_assoc", "UnitsMixed_Before_First_Base_Source_comp",
    "UnitsMixed_Before_First_Base_Source_compId", "UnitsMixed_Before_First_Base_Source_eqCongr",
    "UnitsMixed_Before_First_Base_Source_eqTrans", "UnitsMixed_Before_First_Base_Source_id",
    "UnitsMixed_Before_First_Base_Source_idComp", "UnitsMixed_Before_First_Base_Target_Category",
    "UnitsMixed_Before_First_Base_Target_Hom", "UnitsMixed_Before_First_Base_Target_assoc",
    "UnitsMixed_Before_First_Base_Target_comp", "UnitsMixed_Before_First_Base_Target_compId",
    "UnitsMixed_Before_First_Base_Target_eqCongr", "UnitsMixed_Before_First_Base_Target_eqTrans",
    "UnitsMixed_Before_First_Base_Target_id", "UnitsMixed_Before_First_Base_Target_idComp",
    "UnitsMixed_Before_First_Base_eqCongr", "UnitsMixed_Before_First_Base_functorMap",
    "UnitsMixed_Before_First_Base_functorMapComp", "UnitsMixed_Before_First_Base_functorMapId",
    "UnitsMixed_Before_First_Base_functorObj", "UnitsMixed_Before_First_NatTrans", "UnitsMixed_Before_First_idNat",
    "UnitsMixed_Before_First_natApp", "UnitsMixed_Before_First_naturality", "UnitsMixed_Before_First_targetEqSymm",
    "UnitsMixed_Before_First_vcomp", "UnitsMixed_Before_First_vcompLaw", "UnitsMixed_Before_Middle_Category",
    "UnitsMixed_Before_Middle_Hom", "UnitsMixed_Before_Middle_assoc", "UnitsMixed_Before_Middle_comp",
    "UnitsMixed_Before_Middle_compId", "UnitsMixed_Before_Middle_eqCongr", "UnitsMixed_Before_Middle_eqTrans",
    "UnitsMixed_Before_Middle_id", "UnitsMixed_Before_Middle_idComp", "UnitsMixed_Before_Second_Base_Functor",
    "UnitsMixed_Before_Second_Base_Source_Category", "UnitsMixed_Before_Second_Base_Source_Hom",
    "UnitsMixed_Before_Second_Base_Source_assoc", "UnitsMixed_Before_Second_Base_Source_comp",
    "UnitsMixed_Before_Second_Base_Source_compId", "UnitsMixed_Before_Second_Base_Source_eqCongr",
    "UnitsMixed_Before_Second_Base_Source_eqTrans", "UnitsMixed_Before_Second_Base_Source_id",
    "UnitsMixed_Before_Second_Base_Source_idComp", "UnitsMixed_Before_Second_Base_Target_Category",
    "UnitsMixed_Before_Second_Base_Target_Hom", "UnitsMixed_Before_Second_Base_Target_assoc",
    "UnitsMixed_Before_Second_Base_Target_comp", "UnitsMixed_Before_Second_Base_Target_compId",
    "UnitsMixed_Before_Second_Base_Target_eqCongr", "UnitsMixed_Before_Second_Base_Target_eqTrans",
    "UnitsMixed_Before_Second_Base_Target_id", "UnitsMixed_Before_Second_Base_Target_idComp",
    "UnitsMixed_Before_Second_Base_eqCongr", "UnitsMixed_Before_Second_Base_functorMap",
    "UnitsMixed_Before_Second_Base_functorMapComp", "UnitsMixed_Before_Second_Base_functorMapId",
    "UnitsMixed_Before_Second_Base_functorObj", "UnitsMixed_Before_Second_NatTrans",
    "UnitsMixed_Before_Second_idNat", "UnitsMixed_Before_Second_natApp", "UnitsMixed_Before_Second_naturality",
    "UnitsMixed_Before_Second_targetEqSymm", "UnitsMixed_Before_Second_vcomp", "UnitsMixed_Before_Second_vcompLaw",
    "UnitsMixed_Before_Source_Category", "UnitsMixed_Before_Source_Hom", "UnitsMixed_Before_Source_assoc",
    "UnitsMixed_Before_Source_comp", "UnitsMixed_Before_Source_compId", "UnitsMixed_Before_Source_eqCongr",
    "UnitsMixed_Before_Source_eqTrans", "UnitsMixed_Before_Source_id", "UnitsMixed_Before_Source_idComp",
    "UnitsMixed_Before_Target_Category", "UnitsMixed_Before_Target_Hom", "UnitsMixed_Before_Target_assoc",
    "UnitsMixed_Before_Target_comp", "UnitsMixed_Before_Target_compId", "UnitsMixed_Before_Target_eqCongr",
    "UnitsMixed_Before_Target_eqTrans", "UnitsMixed_Before_Target_id", "UnitsMixed_Before_Target_idComp",
    "UnitsMixed_Before_hcomp", "UnitsMixed_SourceIdentity_Base_Category", "UnitsMixed_SourceIdentity_Base_Hom",
    "UnitsMixed_SourceIdentity_Base_assoc", "UnitsMixed_SourceIdentity_Base_comp",
    "UnitsMixed_SourceIdentity_Base_compId", "UnitsMixed_SourceIdentity_Base_eqCongr",
    "UnitsMixed_SourceIdentity_Base_eqTrans", "UnitsMixed_SourceIdentity_Base_id",
    "UnitsMixed_SourceIdentity_Base_idComp", "UnitsMixed_SourceIdentity_Functor",
    "UnitsMixed_SourceIdentity_idFunctor", "UnitsMixed_Source_Category", "UnitsMixed_Source_Hom",
    "UnitsMixed_Source_assoc", "UnitsMixed_Source_comp", "UnitsMixed_Source_compId", "UnitsMixed_Source_eqCongr",
    "UnitsMixed_Source_eqTrans", "UnitsMixed_Source_id", "UnitsMixed_Source_idComp",
    "UnitsMixed_TargetIdentity_Base_Category", "UnitsMixed_TargetIdentity_Base_Hom",
    "UnitsMixed_TargetIdentity_Base_assoc", "UnitsMixed_TargetIdentity_Base_comp",
    "UnitsMixed_TargetIdentity_Base_compId", "UnitsMixed_TargetIdentity_Base_eqCongr",
    "UnitsMixed_TargetIdentity_Base_eqTrans", "UnitsMixed_TargetIdentity_Base_id",
    "UnitsMixed_TargetIdentity_Base_idComp", "UnitsMixed_TargetIdentity_Functor",
    "UnitsMixed_TargetIdentity_idFunctor", "UnitsMixed_Target_Category", "UnitsMixed_Target_Hom",
    "UnitsMixed_Target_assoc", "UnitsMixed_Target_comp", "UnitsMixed_Target_compId", "UnitsMixed_Target_eqCongr",
    "UnitsMixed_Target_eqTrans", "UnitsMixed_Target_id", "UnitsMixed_Target_idComp", "UnitsMixed_hcompIdFunctorLeft",
    "UnitsMixed_hcompIdFunctorRight", "UnitsMixed_whiskerLeftIdFunctor", "UnitsMixed_whiskerRightIdFunctor",
    "UnitsWide_After_Compose_Base_Composite_Functor", "UnitsWide_After_Compose_Base_Composite_eqCongr",
    "UnitsWide_After_Compose_Base_Composite_functorMap", "UnitsWide_After_Compose_Base_Composite_functorMapComp",
    "UnitsWide_After_Compose_Base_Composite_functorMapId", "UnitsWide_After_Compose_Base_Composite_functorObj",
    "UnitsWide_After_Compose_Base_First_Functor", "UnitsWide_After_Compose_Base_First_eqCongr",
    "UnitsWide_After_Compose_Base_First_functorMap", "UnitsWide_After_Compose_Base_First_functorMapComp",
    "UnitsWide_After_Compose_Base_First_functorMapId", "UnitsWide_After_Compose_Base_First_functorObj",
    "UnitsWide_After_Compose_Base_Middle_Category", "UnitsWide_After_Compose_Base_Middle_Hom",
    "UnitsWide_After_Compose_Base_Middle_assoc", "UnitsWide_After_Compose_Base_Middle_comp",
    "UnitsWide_After_Compose_Base_Middle_compId", "UnitsWide_After_Compose_Base_Middle_eqCongr",
    "UnitsWide_After_Compose_Base_Middle_eqTrans", "UnitsWide_After_Compose_Base_Middle_id",
    "UnitsWide_After_Compose_Base_Middle_idComp", "UnitsWide_After_Compose_Base_Second_Functor",
    "UnitsWide_After_Compose_Base_Second_eqCongr", "UnitsWide_After_Compose_Base_Second_functorMap",
    "UnitsWide_After_Compose_Base_Second_functorMapComp", "UnitsWide_After_Compose_Base_Second_functorMapId",
    "UnitsWide_After_Compose_Base_Second_functorObj", "UnitsWide_After_Compose_Base_Source_Category",
    "UnitsWide_After_Compose_Base_Source_Hom", "UnitsWide_After_Compose_Base_Source_assoc",
    "UnitsWide_After_Compose_Base_Source_comp", "UnitsWide_After_Compose_Base_Source_compId",
    "UnitsWide_After_Compose_Base_Source_eqCongr", "UnitsWide_After_Compose_Base_Source_eqTrans",
    "UnitsWide_After_Compose_Base_Source_id", "UnitsWide_After_Compose_Base_Source_idComp",
    "UnitsWide_After_Compose_Base_Target_Category", "UnitsWide_After_Compose_Base_Target_Hom",
    "UnitsWide_After_Compose_Base_Target_assoc", "UnitsWide_After_Compose_Base_Target_comp",
    "UnitsWide_After_Compose_Base_Target_compId", "UnitsWide_After_Compose_Base_Target_eqCongr",
    "UnitsWide_After_Compose_Base_Target_eqTrans", "UnitsWide_After_Compose_Base_Target_id",
    "UnitsWide_After_Compose_Base_Target_idComp", "UnitsWide_After_Compose_Base_compFunctor",
    "UnitsWide_After_Compose_Composite_NatTrans", "UnitsWide_After_Compose_Composite_natApp",
    "UnitsWide_After_Compose_Composite_naturality", "UnitsWide_After_Compose_First_NatTrans",
    "UnitsWide_After_Compose_First_natApp", "UnitsWide_After_Compose_First_naturality",
    "UnitsWide_After_Compose_Second_NatTrans", "UnitsWide_After_Compose_Second_natApp",
    "UnitsWide_After_Compose_Second_naturality", "UnitsWide_After_Compose_targetEqSymm",
    "UnitsWide_After_Compose_whiskerLeft", "UnitsWide_After_Compose_whiskerRight",
    "UnitsWide_After_Composite_Base_Functor", "UnitsWide_After_Composite_Base_Source_Category",
    "UnitsWide_After_Composite_Base_Source_Hom", "UnitsWide_After_Composite_Base_Source_assoc",
    "UnitsWide_After_Composite_Base_Source_comp", "UnitsWide_After_Composite_Base_Source_compId",
    "UnitsWide_After_Composite_Base_Source_eqCongr", "UnitsWide_After_Composite_Base_Source_eqTrans",
    "UnitsWide_After_Composite_Base_Source_id", "UnitsWide_After_Composite_Base_Source_idComp",
    "UnitsWide_After_Composite_Base_Target_Category", "UnitsWide_After_Composite_Base_Target_Hom",
    "UnitsWide_After_Composite_Base_Target_assoc", "UnitsWide_After_Composite_Base_Target_comp",
    "UnitsWide_After_Composite_Base_Target_compId", "UnitsWide_After_Composite_Base_Target_eqCongr",
    "UnitsWide_After_Composite_Base_Target_eqTrans", "UnitsWide_After_Composite_Base_Target_id",
    "UnitsWide_After_Composite_Base_Target_idComp", "UnitsWide_After_Composite_Base_eqCongr",
    "UnitsWide_After_Composite_Base_functorMap", "UnitsWide_After_Composite_Base_functorMapComp",
    "UnitsWide_After_Composite_Base_functorMapId", "UnitsWide_After_Composite_Base_functorObj",
    "UnitsWide_After_Composite_NatTrans", "UnitsWide_After_Composite_idNat", "UnitsWide_After_Composite_natApp",
    "UnitsWide_After_Composite_naturality", "UnitsWide_After_Composite_targetEqSymm",
    "UnitsWide_After_Composite_vcomp", "UnitsWide_After_Composite_vcompLaw", "UnitsWide_After_First_Base_Functor",
    "UnitsWide_After_First_Base_Source_Category", "UnitsWide_After_First_Base_Source_Hom",
    "UnitsWide_After_First_Base_Source_assoc", "UnitsWide_After_First_Base_Source_comp",
    "UnitsWide_After_First_Base_Source_compId", "UnitsWide_After_First_Base_Source_eqCongr",
    "UnitsWide_After_First_Base_Source_eqTrans", "UnitsWide_After_First_Base_Source_id",
    "UnitsWide_After_First_Base_Source_idComp", "UnitsWide_After_First_Base_Target_Category",
    "UnitsWide_After_First_Base_Target_Hom", "UnitsWide_After_First_Base_Target_assoc",
    "UnitsWide_After_First_Base_Target_comp", "UnitsWide_After_First_Base_Target_compId",
    "UnitsWide_After_First_Base_Target_eqCongr", "UnitsWide_After_First_Base_Target_eqTrans",
    "UnitsWide_After_First_Base_Target_id", "UnitsWide_After_First_Base_Target_idComp",
    "UnitsWide_After_First_Base_eqCongr", "UnitsWide_After_First_Base_functorMap",
    "UnitsWide_After_First_Base_functorMapComp", "UnitsWide_After_First_Base_functorMapId",
    "UnitsWide_After_First_Base_functorObj", "UnitsWide_After_First_NatTrans", "UnitsWide_After_First_idNat",
    "UnitsWide_After_First_natApp", "UnitsWide_After_First_naturality", "UnitsWide_After_First_targetEqSymm",
    "UnitsWide_After_First_vcomp", "UnitsWide_After_First_vcompLaw", "UnitsWide_After_Middle_Category",
    "UnitsWide_After_Middle_Hom", "UnitsWide_After_Middle_assoc", "UnitsWide_After_Middle_comp",
    "UnitsWide_After_Middle_compId", "UnitsWide_After_Middle_eqCongr", "UnitsWide_After_Middle_eqTrans",
    "UnitsWide_After_Middle_id", "UnitsWide_After_Middle_idComp", "UnitsWide_After_Second_Base_Functor",
    "UnitsWide_After_Second_Base_Source_Category", "UnitsWide_After_Second_Base_Source_Hom",
    "UnitsWide_After_Second_Base_Source_assoc", "UnitsWide_After_Second_Base_Source_comp",
    "UnitsWide_After_Second_Base_Source_compId", "UnitsWide_After_Second_Base_Source_eqCongr",
    "UnitsWide_After_Second_Base_Source_eqTrans", "UnitsWide_After_Second_Base_Source_id",
    "UnitsWide_After_Second_Base_Source_idComp", "UnitsWide_After_Second_Base_Target_Category",
    "UnitsWide_After_Second_Base_Target_Hom", "UnitsWide_After_Second_Base_Target_assoc",
    "UnitsWide_After_Second_Base_Target_comp", "UnitsWide_After_Second_Base_Target_compId",
    "UnitsWide_After_Second_Base_Target_eqCongr", "UnitsWide_After_Second_Base_Target_eqTrans",
    "UnitsWide_After_Second_Base_Target_id", "UnitsWide_After_Second_Base_Target_idComp",
    "UnitsWide_After_Second_Base_eqCongr", "UnitsWide_After_Second_Base_functorMap",
    "UnitsWide_After_Second_Base_functorMapComp", "UnitsWide_After_Second_Base_functorMapId",
    "UnitsWide_After_Second_Base_functorObj", "UnitsWide_After_Second_NatTrans", "UnitsWide_After_Second_idNat",
    "UnitsWide_After_Second_natApp", "UnitsWide_After_Second_naturality", "UnitsWide_After_Second_targetEqSymm",
    "UnitsWide_After_Second_vcomp", "UnitsWide_After_Second_vcompLaw", "UnitsWide_After_Source_Category",
    "UnitsWide_After_Source_Hom", "UnitsWide_After_Source_assoc", "UnitsWide_After_Source_comp",
    "UnitsWide_After_Source_compId", "UnitsWide_After_Source_eqCongr", "UnitsWide_After_Source_eqTrans",
    "UnitsWide_After_Source_id", "UnitsWide_After_Source_idComp", "UnitsWide_After_Target_Category",
    "UnitsWide_After_Target_Hom", "UnitsWide_After_Target_assoc", "UnitsWide_After_Target_comp",
    "UnitsWide_After_Target_compId", "UnitsWide_After_Target_eqCongr", "UnitsWide_After_Target_eqTrans",
    "UnitsWide_After_Target_id", "UnitsWide_After_Target_idComp", "UnitsWide_After_hcomp",
    "UnitsWide_Before_Compose_Base_Composite_Functor", "UnitsWide_Before_Compose_Base_Composite_eqCongr",
    "UnitsWide_Before_Compose_Base_Composite_functorMap", "UnitsWide_Before_Compose_Base_Composite_functorMapComp",
    "UnitsWide_Before_Compose_Base_Composite_functorMapId", "UnitsWide_Before_Compose_Base_Composite_functorObj",
    "UnitsWide_Before_Compose_Base_First_Functor", "UnitsWide_Before_Compose_Base_First_eqCongr",
    "UnitsWide_Before_Compose_Base_First_functorMap", "UnitsWide_Before_Compose_Base_First_functorMapComp",
    "UnitsWide_Before_Compose_Base_First_functorMapId", "UnitsWide_Before_Compose_Base_First_functorObj",
    "UnitsWide_Before_Compose_Base_Middle_Category", "UnitsWide_Before_Compose_Base_Middle_Hom",
    "UnitsWide_Before_Compose_Base_Middle_assoc", "UnitsWide_Before_Compose_Base_Middle_comp",
    "UnitsWide_Before_Compose_Base_Middle_compId", "UnitsWide_Before_Compose_Base_Middle_eqCongr",
    "UnitsWide_Before_Compose_Base_Middle_eqTrans", "UnitsWide_Before_Compose_Base_Middle_id",
    "UnitsWide_Before_Compose_Base_Middle_idComp", "UnitsWide_Before_Compose_Base_Second_Functor",
    "UnitsWide_Before_Compose_Base_Second_eqCongr", "UnitsWide_Before_Compose_Base_Second_functorMap",
    "UnitsWide_Before_Compose_Base_Second_functorMapComp", "UnitsWide_Before_Compose_Base_Second_functorMapId",
    "UnitsWide_Before_Compose_Base_Second_functorObj", "UnitsWide_Before_Compose_Base_Source_Category",
    "UnitsWide_Before_Compose_Base_Source_Hom", "UnitsWide_Before_Compose_Base_Source_assoc",
    "UnitsWide_Before_Compose_Base_Source_comp", "UnitsWide_Before_Compose_Base_Source_compId",
    "UnitsWide_Before_Compose_Base_Source_eqCongr", "UnitsWide_Before_Compose_Base_Source_eqTrans",
    "UnitsWide_Before_Compose_Base_Source_id", "UnitsWide_Before_Compose_Base_Source_idComp",
    "UnitsWide_Before_Compose_Base_Target_Category", "UnitsWide_Before_Compose_Base_Target_Hom",
    "UnitsWide_Before_Compose_Base_Target_assoc", "UnitsWide_Before_Compose_Base_Target_comp",
    "UnitsWide_Before_Compose_Base_Target_compId", "UnitsWide_Before_Compose_Base_Target_eqCongr",
    "UnitsWide_Before_Compose_Base_Target_eqTrans", "UnitsWide_Before_Compose_Base_Target_id",
    "UnitsWide_Before_Compose_Base_Target_idComp", "UnitsWide_Before_Compose_Base_compFunctor",
    "UnitsWide_Before_Compose_Composite_NatTrans", "UnitsWide_Before_Compose_Composite_natApp",
    "UnitsWide_Before_Compose_Composite_naturality", "UnitsWide_Before_Compose_First_NatTrans",
    "UnitsWide_Before_Compose_First_natApp", "UnitsWide_Before_Compose_First_naturality",
    "UnitsWide_Before_Compose_Second_NatTrans", "UnitsWide_Before_Compose_Second_natApp",
    "UnitsWide_Before_Compose_Second_naturality", "UnitsWide_Before_Compose_targetEqSymm",
    "UnitsWide_Before_Compose_whiskerLeft", "UnitsWide_Before_Compose_whiskerRight",
    "UnitsWide_Before_Composite_Base_Functor", "UnitsWide_Before_Composite_Base_Source_Category",
    "UnitsWide_Before_Composite_Base_Source_Hom", "UnitsWide_Before_Composite_Base_Source_assoc",
    "UnitsWide_Before_Composite_Base_Source_comp", "UnitsWide_Before_Composite_Base_Source_compId",
    "UnitsWide_Before_Composite_Base_Source_eqCongr", "UnitsWide_Before_Composite_Base_Source_eqTrans",
    "UnitsWide_Before_Composite_Base_Source_id", "UnitsWide_Before_Composite_Base_Source_idComp",
    "UnitsWide_Before_Composite_Base_Target_Category", "UnitsWide_Before_Composite_Base_Target_Hom",
    "UnitsWide_Before_Composite_Base_Target_assoc", "UnitsWide_Before_Composite_Base_Target_comp",
    "UnitsWide_Before_Composite_Base_Target_compId", "UnitsWide_Before_Composite_Base_Target_eqCongr",
    "UnitsWide_Before_Composite_Base_Target_eqTrans", "UnitsWide_Before_Composite_Base_Target_id",
    "UnitsWide_Before_Composite_Base_Target_idComp", "UnitsWide_Before_Composite_Base_eqCongr",
    "UnitsWide_Before_Composite_Base_functorMap", "UnitsWide_Before_Composite_Base_functorMapComp",
    "UnitsWide_Before_Composite_Base_functorMapId", "UnitsWide_Before_Composite_Base_functorObj",
    "UnitsWide_Before_Composite_NatTrans", "UnitsWide_Before_Composite_idNat", "UnitsWide_Before_Composite_natApp",
    "UnitsWide_Before_Composite_naturality", "UnitsWide_Before_Composite_targetEqSymm",
    "UnitsWide_Before_Composite_vcomp", "UnitsWide_Before_Composite_vcompLaw", "UnitsWide_Before_First_Base_Functor",
    "UnitsWide_Before_First_Base_Source_Category", "UnitsWide_Before_First_Base_Source_Hom",
    "UnitsWide_Before_First_Base_Source_assoc", "UnitsWide_Before_First_Base_Source_comp",
    "UnitsWide_Before_First_Base_Source_compId", "UnitsWide_Before_First_Base_Source_eqCongr",
    "UnitsWide_Before_First_Base_Source_eqTrans", "UnitsWide_Before_First_Base_Source_id",
    "UnitsWide_Before_First_Base_Source_idComp", "UnitsWide_Before_First_Base_Target_Category",
    "UnitsWide_Before_First_Base_Target_Hom", "UnitsWide_Before_First_Base_Target_assoc",
    "UnitsWide_Before_First_Base_Target_comp", "UnitsWide_Before_First_Base_Target_compId",
    "UnitsWide_Before_First_Base_Target_eqCongr", "UnitsWide_Before_First_Base_Target_eqTrans",
    "UnitsWide_Before_First_Base_Target_id", "UnitsWide_Before_First_Base_Target_idComp",
    "UnitsWide_Before_First_Base_eqCongr", "UnitsWide_Before_First_Base_functorMap",
    "UnitsWide_Before_First_Base_functorMapComp", "UnitsWide_Before_First_Base_functorMapId",
    "UnitsWide_Before_First_Base_functorObj", "UnitsWide_Before_First_NatTrans", "UnitsWide_Before_First_idNat",
    "UnitsWide_Before_First_natApp", "UnitsWide_Before_First_naturality", "UnitsWide_Before_First_targetEqSymm",
    "UnitsWide_Before_First_vcomp", "UnitsWide_Before_First_vcompLaw", "UnitsWide_Before_Middle_Category",
    "UnitsWide_Before_Middle_Hom", "UnitsWide_Before_Middle_assoc", "UnitsWide_Before_Middle_comp",
    "UnitsWide_Before_Middle_compId", "UnitsWide_Before_Middle_eqCongr", "UnitsWide_Before_Middle_eqTrans",
    "UnitsWide_Before_Middle_id", "UnitsWide_Before_Middle_idComp", "UnitsWide_Before_Second_Base_Functor",
    "UnitsWide_Before_Second_Base_Source_Category", "UnitsWide_Before_Second_Base_Source_Hom",
    "UnitsWide_Before_Second_Base_Source_assoc", "UnitsWide_Before_Second_Base_Source_comp",
    "UnitsWide_Before_Second_Base_Source_compId", "UnitsWide_Before_Second_Base_Source_eqCongr",
    "UnitsWide_Before_Second_Base_Source_eqTrans", "UnitsWide_Before_Second_Base_Source_id",
    "UnitsWide_Before_Second_Base_Source_idComp", "UnitsWide_Before_Second_Base_Target_Category",
    "UnitsWide_Before_Second_Base_Target_Hom", "UnitsWide_Before_Second_Base_Target_assoc",
    "UnitsWide_Before_Second_Base_Target_comp", "UnitsWide_Before_Second_Base_Target_compId",
    "UnitsWide_Before_Second_Base_Target_eqCongr", "UnitsWide_Before_Second_Base_Target_eqTrans",
    "UnitsWide_Before_Second_Base_Target_id", "UnitsWide_Before_Second_Base_Target_idComp",
    "UnitsWide_Before_Second_Base_eqCongr", "UnitsWide_Before_Second_Base_functorMap",
    "UnitsWide_Before_Second_Base_functorMapComp", "UnitsWide_Before_Second_Base_functorMapId",
    "UnitsWide_Before_Second_Base_functorObj", "UnitsWide_Before_Second_NatTrans", "UnitsWide_Before_Second_idNat",
    "UnitsWide_Before_Second_natApp", "UnitsWide_Before_Second_naturality", "UnitsWide_Before_Second_targetEqSymm",
    "UnitsWide_Before_Second_vcomp", "UnitsWide_Before_Second_vcompLaw", "UnitsWide_Before_Source_Category",
    "UnitsWide_Before_Source_Hom", "UnitsWide_Before_Source_assoc", "UnitsWide_Before_Source_comp",
    "UnitsWide_Before_Source_compId", "UnitsWide_Before_Source_eqCongr", "UnitsWide_Before_Source_eqTrans",
    "UnitsWide_Before_Source_id", "UnitsWide_Before_Source_idComp", "UnitsWide_Before_Target_Category",
    "UnitsWide_Before_Target_Hom", "UnitsWide_Before_Target_assoc", "UnitsWide_Before_Target_comp",
    "UnitsWide_Before_Target_compId", "UnitsWide_Before_Target_eqCongr", "UnitsWide_Before_Target_eqTrans",
    "UnitsWide_Before_Target_id", "UnitsWide_Before_Target_idComp", "UnitsWide_Before_hcomp",
    "UnitsWide_SourceIdentity_Base_Category", "UnitsWide_SourceIdentity_Base_Hom",
    "UnitsWide_SourceIdentity_Base_assoc", "UnitsWide_SourceIdentity_Base_comp",
    "UnitsWide_SourceIdentity_Base_compId", "UnitsWide_SourceIdentity_Base_eqCongr",
    "UnitsWide_SourceIdentity_Base_eqTrans", "UnitsWide_SourceIdentity_Base_id",
    "UnitsWide_SourceIdentity_Base_idComp", "UnitsWide_SourceIdentity_Functor", "UnitsWide_SourceIdentity_idFunctor",
    "UnitsWide_Source_Category", "UnitsWide_Source_Hom", "UnitsWide_Source_assoc", "UnitsWide_Source_comp",
    "UnitsWide_Source_compId", "UnitsWide_Source_eqCongr", "UnitsWide_Source_eqTrans", "UnitsWide_Source_id",
    "UnitsWide_Source_idComp", "UnitsWide_TargetIdentity_Base_Category", "UnitsWide_TargetIdentity_Base_Hom",
    "UnitsWide_TargetIdentity_Base_assoc", "UnitsWide_TargetIdentity_Base_comp",
    "UnitsWide_TargetIdentity_Base_compId", "UnitsWide_TargetIdentity_Base_eqCongr",
    "UnitsWide_TargetIdentity_Base_eqTrans", "UnitsWide_TargetIdentity_Base_id",
    "UnitsWide_TargetIdentity_Base_idComp", "UnitsWide_TargetIdentity_Functor", "UnitsWide_TargetIdentity_idFunctor",
    "UnitsWide_Target_Category", "UnitsWide_Target_Hom", "UnitsWide_Target_assoc", "UnitsWide_Target_comp",
    "UnitsWide_Target_compId", "UnitsWide_Target_eqCongr", "UnitsWide_Target_eqTrans", "UnitsWide_Target_id",
    "UnitsWide_Target_idComp", "UnitsWide_hcompIdFunctorLeft", "UnitsWide_hcompIdFunctorRight",
    "UnitsWide_whiskerLeftIdFunctor", "UnitsWide_whiskerRightIdFunctor",
    "Units_After_Compose_Base_Composite_Functor", "Units_After_Compose_Base_Composite_eqCongr",
    "Units_After_Compose_Base_Composite_functorMap", "Units_After_Compose_Base_Composite_functorMapComp",
    "Units_After_Compose_Base_Composite_functorMapId", "Units_After_Compose_Base_Composite_functorObj",
    "Units_After_Compose_Base_First_Functor", "Units_After_Compose_Base_First_eqCongr",
    "Units_After_Compose_Base_First_functorMap", "Units_After_Compose_Base_First_functorMapComp",
    "Units_After_Compose_Base_First_functorMapId", "Units_After_Compose_Base_First_functorObj",
    "Units_After_Compose_Base_Middle_Category", "Units_After_Compose_Base_Middle_Hom",
    "Units_After_Compose_Base_Middle_assoc", "Units_After_Compose_Base_Middle_comp",
    "Units_After_Compose_Base_Middle_compId", "Units_After_Compose_Base_Middle_eqCongr",
    "Units_After_Compose_Base_Middle_eqTrans", "Units_After_Compose_Base_Middle_id",
    "Units_After_Compose_Base_Middle_idComp", "Units_After_Compose_Base_Second_Functor",
    "Units_After_Compose_Base_Second_eqCongr", "Units_After_Compose_Base_Second_functorMap",
    "Units_After_Compose_Base_Second_functorMapComp", "Units_After_Compose_Base_Second_functorMapId",
    "Units_After_Compose_Base_Second_functorObj", "Units_After_Compose_Base_Source_Category",
    "Units_After_Compose_Base_Source_Hom", "Units_After_Compose_Base_Source_assoc",
    "Units_After_Compose_Base_Source_comp", "Units_After_Compose_Base_Source_compId",
    "Units_After_Compose_Base_Source_eqCongr", "Units_After_Compose_Base_Source_eqTrans",
    "Units_After_Compose_Base_Source_id", "Units_After_Compose_Base_Source_idComp",
    "Units_After_Compose_Base_Target_Category", "Units_After_Compose_Base_Target_Hom",
    "Units_After_Compose_Base_Target_assoc", "Units_After_Compose_Base_Target_comp",
    "Units_After_Compose_Base_Target_compId", "Units_After_Compose_Base_Target_eqCongr",
    "Units_After_Compose_Base_Target_eqTrans", "Units_After_Compose_Base_Target_id",
    "Units_After_Compose_Base_Target_idComp", "Units_After_Compose_Base_compFunctor",
    "Units_After_Compose_Composite_NatTrans", "Units_After_Compose_Composite_natApp",
    "Units_After_Compose_Composite_naturality", "Units_After_Compose_First_NatTrans",
    "Units_After_Compose_First_natApp", "Units_After_Compose_First_naturality",
    "Units_After_Compose_Second_NatTrans", "Units_After_Compose_Second_natApp",
    "Units_After_Compose_Second_naturality", "Units_After_Compose_targetEqSymm", "Units_After_Compose_whiskerLeft",
    "Units_After_Compose_whiskerRight", "Units_After_Composite_Base_Functor",
    "Units_After_Composite_Base_Source_Category", "Units_After_Composite_Base_Source_Hom",
    "Units_After_Composite_Base_Source_assoc", "Units_After_Composite_Base_Source_comp",
    "Units_After_Composite_Base_Source_compId", "Units_After_Composite_Base_Source_eqCongr",
    "Units_After_Composite_Base_Source_eqTrans", "Units_After_Composite_Base_Source_id",
    "Units_After_Composite_Base_Source_idComp", "Units_After_Composite_Base_Target_Category",
    "Units_After_Composite_Base_Target_Hom", "Units_After_Composite_Base_Target_assoc",
    "Units_After_Composite_Base_Target_comp", "Units_After_Composite_Base_Target_compId",
    "Units_After_Composite_Base_Target_eqCongr", "Units_After_Composite_Base_Target_eqTrans",
    "Units_After_Composite_Base_Target_id", "Units_After_Composite_Base_Target_idComp",
    "Units_After_Composite_Base_eqCongr", "Units_After_Composite_Base_functorMap",
    "Units_After_Composite_Base_functorMapComp", "Units_After_Composite_Base_functorMapId",
    "Units_After_Composite_Base_functorObj", "Units_After_Composite_NatTrans", "Units_After_Composite_idNat",
    "Units_After_Composite_natApp", "Units_After_Composite_naturality", "Units_After_Composite_targetEqSymm",
    "Units_After_Composite_vcomp", "Units_After_Composite_vcompLaw", "Units_After_First_Base_Functor",
    "Units_After_First_Base_Source_Category", "Units_After_First_Base_Source_Hom",
    "Units_After_First_Base_Source_assoc", "Units_After_First_Base_Source_comp",
    "Units_After_First_Base_Source_compId", "Units_After_First_Base_Source_eqCongr",
    "Units_After_First_Base_Source_eqTrans", "Units_After_First_Base_Source_id",
    "Units_After_First_Base_Source_idComp", "Units_After_First_Base_Target_Category",
    "Units_After_First_Base_Target_Hom", "Units_After_First_Base_Target_assoc", "Units_After_First_Base_Target_comp",
    "Units_After_First_Base_Target_compId", "Units_After_First_Base_Target_eqCongr",
    "Units_After_First_Base_Target_eqTrans", "Units_After_First_Base_Target_id",
    "Units_After_First_Base_Target_idComp", "Units_After_First_Base_eqCongr", "Units_After_First_Base_functorMap",
    "Units_After_First_Base_functorMapComp", "Units_After_First_Base_functorMapId",
    "Units_After_First_Base_functorObj", "Units_After_First_NatTrans", "Units_After_First_idNat",
    "Units_After_First_natApp", "Units_After_First_naturality", "Units_After_First_targetEqSymm",
    "Units_After_First_vcomp", "Units_After_First_vcompLaw", "Units_After_Middle_Category", "Units_After_Middle_Hom",
    "Units_After_Middle_assoc", "Units_After_Middle_comp", "Units_After_Middle_compId", "Units_After_Middle_eqCongr",
    "Units_After_Middle_eqTrans", "Units_After_Middle_id", "Units_After_Middle_idComp",
    "Units_After_Second_Base_Functor", "Units_After_Second_Base_Source_Category",
    "Units_After_Second_Base_Source_Hom", "Units_After_Second_Base_Source_assoc",
    "Units_After_Second_Base_Source_comp", "Units_After_Second_Base_Source_compId",
    "Units_After_Second_Base_Source_eqCongr", "Units_After_Second_Base_Source_eqTrans",
    "Units_After_Second_Base_Source_id", "Units_After_Second_Base_Source_idComp",
    "Units_After_Second_Base_Target_Category", "Units_After_Second_Base_Target_Hom",
    "Units_After_Second_Base_Target_assoc", "Units_After_Second_Base_Target_comp",
    "Units_After_Second_Base_Target_compId", "Units_After_Second_Base_Target_eqCongr",
    "Units_After_Second_Base_Target_eqTrans", "Units_After_Second_Base_Target_id",
    "Units_After_Second_Base_Target_idComp", "Units_After_Second_Base_eqCongr", "Units_After_Second_Base_functorMap",
    "Units_After_Second_Base_functorMapComp", "Units_After_Second_Base_functorMapId",
    "Units_After_Second_Base_functorObj", "Units_After_Second_NatTrans", "Units_After_Second_idNat",
    "Units_After_Second_natApp", "Units_After_Second_naturality", "Units_After_Second_targetEqSymm",
    "Units_After_Second_vcomp", "Units_After_Second_vcompLaw", "Units_After_Source_Category",
    "Units_After_Source_Hom", "Units_After_Source_assoc", "Units_After_Source_comp", "Units_After_Source_compId",
    "Units_After_Source_eqCongr", "Units_After_Source_eqTrans", "Units_After_Source_id", "Units_After_Source_idComp",
    "Units_After_Target_Category", "Units_After_Target_Hom", "Units_After_Target_assoc", "Units_After_Target_comp",
    "Units_After_Target_compId", "Units_After_Target_eqCongr", "Units_After_Target_eqTrans", "Units_After_Target_id",
    "Units_After_Target_idComp", "Units_After_hcomp", "Units_Before_Compose_Base_Composite_Functor",
    "Units_Before_Compose_Base_Composite_eqCongr", "Units_Before_Compose_Base_Composite_functorMap",
    "Units_Before_Compose_Base_Composite_functorMapComp", "Units_Before_Compose_Base_Composite_functorMapId",
    "Units_Before_Compose_Base_Composite_functorObj", "Units_Before_Compose_Base_First_Functor",
    "Units_Before_Compose_Base_First_eqCongr", "Units_Before_Compose_Base_First_functorMap",
    "Units_Before_Compose_Base_First_functorMapComp", "Units_Before_Compose_Base_First_functorMapId",
    "Units_Before_Compose_Base_First_functorObj", "Units_Before_Compose_Base_Middle_Category",
    "Units_Before_Compose_Base_Middle_Hom", "Units_Before_Compose_Base_Middle_assoc",
    "Units_Before_Compose_Base_Middle_comp", "Units_Before_Compose_Base_Middle_compId",
    "Units_Before_Compose_Base_Middle_eqCongr", "Units_Before_Compose_Base_Middle_eqTrans",
    "Units_Before_Compose_Base_Middle_id", "Units_Before_Compose_Base_Middle_idComp",
    "Units_Before_Compose_Base_Second_Functor", "Units_Before_Compose_Base_Second_eqCongr",
    "Units_Before_Compose_Base_Second_functorMap", "Units_Before_Compose_Base_Second_functorMapComp",
    "Units_Before_Compose_Base_Second_functorMapId", "Units_Before_Compose_Base_Second_functorObj",
    "Units_Before_Compose_Base_Source_Category", "Units_Before_Compose_Base_Source_Hom",
    "Units_Before_Compose_Base_Source_assoc", "Units_Before_Compose_Base_Source_comp",
    "Units_Before_Compose_Base_Source_compId", "Units_Before_Compose_Base_Source_eqCongr",
    "Units_Before_Compose_Base_Source_eqTrans", "Units_Before_Compose_Base_Source_id",
    "Units_Before_Compose_Base_Source_idComp", "Units_Before_Compose_Base_Target_Category",
    "Units_Before_Compose_Base_Target_Hom", "Units_Before_Compose_Base_Target_assoc",
    "Units_Before_Compose_Base_Target_comp", "Units_Before_Compose_Base_Target_compId",
    "Units_Before_Compose_Base_Target_eqCongr", "Units_Before_Compose_Base_Target_eqTrans",
    "Units_Before_Compose_Base_Target_id", "Units_Before_Compose_Base_Target_idComp",
    "Units_Before_Compose_Base_compFunctor", "Units_Before_Compose_Composite_NatTrans",
    "Units_Before_Compose_Composite_natApp", "Units_Before_Compose_Composite_naturality",
    "Units_Before_Compose_First_NatTrans", "Units_Before_Compose_First_natApp",
    "Units_Before_Compose_First_naturality", "Units_Before_Compose_Second_NatTrans",
    "Units_Before_Compose_Second_natApp", "Units_Before_Compose_Second_naturality",
    "Units_Before_Compose_targetEqSymm", "Units_Before_Compose_whiskerLeft", "Units_Before_Compose_whiskerRight",
    "Units_Before_Composite_Base_Functor", "Units_Before_Composite_Base_Source_Category",
    "Units_Before_Composite_Base_Source_Hom", "Units_Before_Composite_Base_Source_assoc",
    "Units_Before_Composite_Base_Source_comp", "Units_Before_Composite_Base_Source_compId",
    "Units_Before_Composite_Base_Source_eqCongr", "Units_Before_Composite_Base_Source_eqTrans",
    "Units_Before_Composite_Base_Source_id", "Units_Before_Composite_Base_Source_idComp",
    "Units_Before_Composite_Base_Target_Category", "Units_Before_Composite_Base_Target_Hom",
    "Units_Before_Composite_Base_Target_assoc", "Units_Before_Composite_Base_Target_comp",
    "Units_Before_Composite_Base_Target_compId", "Units_Before_Composite_Base_Target_eqCongr",
    "Units_Before_Composite_Base_Target_eqTrans", "Units_Before_Composite_Base_Target_id",
    "Units_Before_Composite_Base_Target_idComp", "Units_Before_Composite_Base_eqCongr",
    "Units_Before_Composite_Base_functorMap", "Units_Before_Composite_Base_functorMapComp",
    "Units_Before_Composite_Base_functorMapId", "Units_Before_Composite_Base_functorObj",
    "Units_Before_Composite_NatTrans", "Units_Before_Composite_idNat", "Units_Before_Composite_natApp",
    "Units_Before_Composite_naturality", "Units_Before_Composite_targetEqSymm", "Units_Before_Composite_vcomp",
    "Units_Before_Composite_vcompLaw", "Units_Before_First_Base_Functor", "Units_Before_First_Base_Source_Category",
    "Units_Before_First_Base_Source_Hom", "Units_Before_First_Base_Source_assoc",
    "Units_Before_First_Base_Source_comp", "Units_Before_First_Base_Source_compId",
    "Units_Before_First_Base_Source_eqCongr", "Units_Before_First_Base_Source_eqTrans",
    "Units_Before_First_Base_Source_id", "Units_Before_First_Base_Source_idComp",
    "Units_Before_First_Base_Target_Category", "Units_Before_First_Base_Target_Hom",
    "Units_Before_First_Base_Target_assoc", "Units_Before_First_Base_Target_comp",
    "Units_Before_First_Base_Target_compId", "Units_Before_First_Base_Target_eqCongr",
    "Units_Before_First_Base_Target_eqTrans", "Units_Before_First_Base_Target_id",
    "Units_Before_First_Base_Target_idComp", "Units_Before_First_Base_eqCongr", "Units_Before_First_Base_functorMap",
    "Units_Before_First_Base_functorMapComp", "Units_Before_First_Base_functorMapId",
    "Units_Before_First_Base_functorObj", "Units_Before_First_NatTrans", "Units_Before_First_idNat",
    "Units_Before_First_natApp", "Units_Before_First_naturality", "Units_Before_First_targetEqSymm",
    "Units_Before_First_vcomp", "Units_Before_First_vcompLaw", "Units_Before_Middle_Category",
    "Units_Before_Middle_Hom", "Units_Before_Middle_assoc", "Units_Before_Middle_comp", "Units_Before_Middle_compId",
    "Units_Before_Middle_eqCongr", "Units_Before_Middle_eqTrans", "Units_Before_Middle_id",
    "Units_Before_Middle_idComp", "Units_Before_Second_Base_Functor", "Units_Before_Second_Base_Source_Category",
    "Units_Before_Second_Base_Source_Hom", "Units_Before_Second_Base_Source_assoc",
    "Units_Before_Second_Base_Source_comp", "Units_Before_Second_Base_Source_compId",
    "Units_Before_Second_Base_Source_eqCongr", "Units_Before_Second_Base_Source_eqTrans",
    "Units_Before_Second_Base_Source_id", "Units_Before_Second_Base_Source_idComp",
    "Units_Before_Second_Base_Target_Category", "Units_Before_Second_Base_Target_Hom",
    "Units_Before_Second_Base_Target_assoc", "Units_Before_Second_Base_Target_comp",
    "Units_Before_Second_Base_Target_compId", "Units_Before_Second_Base_Target_eqCongr",
    "Units_Before_Second_Base_Target_eqTrans", "Units_Before_Second_Base_Target_id",
    "Units_Before_Second_Base_Target_idComp", "Units_Before_Second_Base_eqCongr",
    "Units_Before_Second_Base_functorMap", "Units_Before_Second_Base_functorMapComp",
    "Units_Before_Second_Base_functorMapId", "Units_Before_Second_Base_functorObj", "Units_Before_Second_NatTrans",
    "Units_Before_Second_idNat", "Units_Before_Second_natApp", "Units_Before_Second_naturality",
    "Units_Before_Second_targetEqSymm", "Units_Before_Second_vcomp", "Units_Before_Second_vcompLaw",
    "Units_Before_Source_Category", "Units_Before_Source_Hom", "Units_Before_Source_assoc",
    "Units_Before_Source_comp", "Units_Before_Source_compId", "Units_Before_Source_eqCongr",
    "Units_Before_Source_eqTrans", "Units_Before_Source_id", "Units_Before_Source_idComp",
    "Units_Before_Target_Category", "Units_Before_Target_Hom", "Units_Before_Target_assoc",
    "Units_Before_Target_comp", "Units_Before_Target_compId", "Units_Before_Target_eqCongr",
    "Units_Before_Target_eqTrans", "Units_Before_Target_id", "Units_Before_Target_idComp", "Units_Before_hcomp",
    "Units_SourceIdentity_Base_Category", "Units_SourceIdentity_Base_Hom", "Units_SourceIdentity_Base_assoc",
    "Units_SourceIdentity_Base_comp", "Units_SourceIdentity_Base_compId", "Units_SourceIdentity_Base_eqCongr",
    "Units_SourceIdentity_Base_eqTrans", "Units_SourceIdentity_Base_id", "Units_SourceIdentity_Base_idComp",
    "Units_SourceIdentity_Functor", "Units_SourceIdentity_idFunctor", "Units_Source_Category", "Units_Source_Hom",
    "Units_Source_assoc", "Units_Source_comp", "Units_Source_compId", "Units_Source_eqCongr", "Units_Source_eqTrans",
    "Units_Source_id", "Units_Source_idComp", "Units_TargetIdentity_Base_Category", "Units_TargetIdentity_Base_Hom",
    "Units_TargetIdentity_Base_assoc", "Units_TargetIdentity_Base_comp", "Units_TargetIdentity_Base_compId",
    "Units_TargetIdentity_Base_eqCongr", "Units_TargetIdentity_Base_eqTrans", "Units_TargetIdentity_Base_id",
    "Units_TargetIdentity_Base_idComp", "Units_TargetIdentity_Functor", "Units_TargetIdentity_idFunctor",
    "Units_Target_Category", "Units_Target_Hom", "Units_Target_assoc", "Units_Target_comp", "Units_Target_compId",
    "Units_Target_eqCongr", "Units_Target_eqTrans", "Units_Target_id", "Units_Target_idComp",
    "Units_hcompIdFunctorLeft", "Units_hcompIdFunctorRight", "Units_whiskerLeftIdFunctor",
    "Units_whiskerRightIdFunctor", "betaComponentLaw", "componentLaw", "componentValue", "heterogeneousNatInput",
    "identityValue", "leftUnitAt", "leftUnitLaw", "leftUnitLawAbstract", "naturalityWitness", "pathTrans",
    "postUnitAt", "postUnitLaw", "postUnitLawAbstract", "preUnitAt", "preUnitLaw", "preUnitLawAbstract",
    "rightUnitAt", "rightUnitLaw", "rightUnitLawAbstract", "unitCertified", "unitReferenceAt", "verticalForward",
    "verticalNested", "verticalReverse"
};

/* expected_families() */
static const char *const EXPECTED_FAMILIES[] = {
    "Path", "Point", "Run_Base_Source", "Run_Base_Target", "SeparateTarget", "UnitsMixed_Source",
    "UnitsMixed_Target", "UnitsWide_Source", "UnitsWide_Target"
};

/* negatives() */
static const char *const NEGATIVES[] = {
    "false-components", "wrong-object", "nominal-target", "incompatible-endpoints", "left-reflexivity",
    "right-reflexivity"
};

/* right_statement() */
#define RIGHT_STATEMENT "(0 C : Sort (succ u0)) -> (0 D : Sort (succ u2)) -> (c : Source_Category C) -> (d : Target_Category D) -> (F : Before_Second_Base_Functor C D c d) -> (G : Before_Second_Base_Functor C D c d) -> (alpha : Before_Second_NatTrans C D c d F G) -> (x : C) -> Target (Target_Hom D d (F.1 x) (G.1 x)) ((Before_Compose_whiskerRight C C D c c d F G alpha (SourceIdentity_idFunctor C c)).1 x) (alpha.1 x)"

/* left_statement() */
#define LEFT_STATEMENT "(0 C : Sort (succ u0)) -> (0 D : Sort (succ u2)) -> (c : Source_Category C) -> (d : Target_Category D) -> (F : After_First_Base_Functor C D c d) -> (G : After_First_Base_Functor C D c d) -> (alpha : After_First_NatTrans C D c d F G) -> (x : C) -> Target (Target_Hom D d (F.1 x) (G.1 x)) ((After_Compose_whiskerLeft C D D c d d (TargetIdentity_idFunctor D d) F G alpha).1 x) (alpha.1 x)"

/* One Program.Probe{name, payload, expected} row of values(). */
static void probe(Heap *heap, Vec *out, const char *name, uint32_t payload, uint32_t expected) {
  Probe *row = heap_alloc(heap, sizeof *row);
  if (row == NULL) return;
  row->name = str_lit(name);
  row->payload = payload;
  row->expected = expected;
  vec_push(out, row);
}

/* values() (15 Program.Probe rows) */
static Vec values(Heap *heap) {
  Vec out = vec_new(heap->arena);
  probe(heap, &out, "preUnitAt", 0, 707);
  probe(heap, &out, "postUnitAt", 0, 707);
  probe(heap, &out, "leftUnitAt", 0, 707);
  probe(heap, &out, "rightUnitAt", 0, 707);
  probe(heap, &out, "unitReferenceAt", 0, 707);
  probe(heap, &out, "preUnitAt", 37, 8218);
  probe(heap, &out, "postUnitAt", 37, 8218);
  probe(heap, &out, "leftUnitAt", 37, 8218);
  probe(heap, &out, "rightUnitAt", 37, 8218);
  probe(heap, &out, "unitReferenceAt", 37, 8218);
  probe(heap, &out, "preUnitAt", 41, 9030);
  probe(heap, &out, "postUnitAt", 41, 9030);
  probe(heap, &out, "leftUnitAt", 41, 9030);
  probe(heap, &out, "rightUnitAt", 41, 9030);
  probe(heap, &out, "unitReferenceAt", 41, 9030);
  return out;
}

/* The R.read_all path list of source in suite() */
static const char *const SOURCE_PATHS[] = {
    "prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
    "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
    "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech",
    "prelude/cat/identity-functor.mech", "prelude/cat/nattrans-units.mech",
    "test/fixtures/prelude/nattrans-units.mech"
};

/* The R.read_all path list of fixture in suite() */
static const char *const FIXTURE_PATHS[] = {
    "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech", "test/fixtures/prelude/nattrans-units-runtime.mech"
};

/* The head_pins rows of suite() */
static const PinRow HEAD_PINS[] = {
    {"PreUnit", "Units_Before_Compose_whiskerRight"}, {"PostUnit", "Units_After_Compose_whiskerLeft"},
    {"LeftUnit", "Units_Before_hcomp"}, {"RightUnit", "Units_After_hcomp"}
};

/* The case_pins rows of suite() */
static const PinRow CASE_PINS[] = {
    {"pre", "PreUnit"}, {"post", "PostUnit"}, {"left", "LeftUnit"}, {"right", "RightUnit"}
};

/* The full list of the Neg.Digest policy of suite() */
static const char *const DIGEST_FULL[] = {
    "left-reflexivity", "right-reflexivity"
};

/* suite(root) */
static Task suite(Heap *heap, void *ctx, Str *fields) {
  Str root = *(const Str *)ctx;
  Str source = str_empty();
  Vec source_paths = STRINGS(heap, SOURCE_PATHS);
  Str fixture = str_empty();
  Vec fixture_paths = STRINGS(heap, FIXTURE_PATHS);
  const Vec *parsed = NULL;
  Vec bodies;
  Vec types;
  const ElabChecked *checked = NULL;
  Vec entries;
  Vec families;
  Globals globals;
  Vec rows;
  Vec negatives;
  Vec full;
  RELATIONAL_TRY(relational_read_all(heap, root, &source_paths, &source));
  RELATIONAL_TRY(relational_read_all(heap, root, &fixture_paths, &fixture));
  RELATIONAL_TRY(relational_parse_roundtrip(heap, cat(heap, cat(heap, source, str_lit("\n")), fixture), &parsed));
  bodies = definitions(heap, parsed);
  RELATIONAL_TRY(head_pins(heap, HEAD_PINS, sizeof HEAD_PINS / sizeof HEAD_PINS[0], &bodies));
  RELATIONAL_TRY(case_pins(heap, CASE_PINS, sizeof CASE_PINS / sizeof CASE_PINS[0], &bodies));
  types = templates(heap, parsed);
  RELATIONAL_TRY(law_statement(heap, &types, str_lit("whiskerRightIdFunctor"), str_lit(RIGHT_STATEMENT)));
  RELATIONAL_TRY(law_statement(heap, &types, str_lit("whiskerLeftIdFunctor"), str_lit(LEFT_STATEMENT)));
  RELATIONAL_TRY(relational_elaborated(heap, global_initial(heap), parsed, &checked));
  entries = STRINGS(heap, EXPECTED_ENTRIES);
  families = STRINGS(heap, EXPECTED_FAMILIES);
  RELATIONAL_TRY(program_inventory(heap, checked, &entries, &families));
  globals = relational_globals(checked);
  rows = values(heap);
  RELATIONAL_TRY(program_probes(heap, &rows, globals));
  negatives = STRINGS(heap, NEGATIVES);
  full = STRINGS(heap, DIGEST_FULL);
  RELATIONAL_TRY(negatives_run(heap, root, str_lit("test/neg/nattrans-units"), &negatives, globals,
                               negatives_digest(0, 256, &full, true, true)));
  *fields = str_lit("entries=1281 families=9 computations=15 negatives=6");
  return task_pure();
}

/* arguments(args) = R.run("PRELUDE-NATTRANS-UNITS", suite(R.root(args))) */
static int program(int argc, char **argv) {
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  Str root = relational_root(argc, argv);
  int code = relational_run(&heap, str_lit("PRELUDE-NATTRANS-UNITS"), suite, &root);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
