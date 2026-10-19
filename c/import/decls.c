/* Port of import/decls.bend: the declaration entries of a lean4export NDJSON stream
 * (axiom, def, thm, opaque, quot and the inductive groups of types, ctors and recs) with the
 * group consistency checks; every index stays decimal text. */
#include "import.h"

/* Result<String, Hints> */
typedef struct {
  JsonResultKind kind;
  DeclHints value;
  Str message;
} HintsResult;

/* Result<String, Safety> */
typedef struct {
  JsonResultKind kind;
  DeclSafety value;
  Str message;
} SafetyResult;

/* Result<String, QuotKind> */
typedef struct {
  JsonResultKind kind;
  DeclQuotKind value;
  Str message;
} QuotResult;

/* Result<String, Rule> */
typedef struct {
  JsonResultKind kind;
  DeclRule value;
  Str message;
} RuleResult;

/* Result<String, T> */
typedef struct {
  JsonResultKind kind;
  DeclEntry value;
  Str message;
} DeclResult;

/* Result<String, Unit> */
typedef struct {
  JsonResultKind kind;
  Str message;
} UnitResult;

/* GroupPart: GroupType{}, GroupCtor{}, GroupRec{} */
typedef enum { GROUP_TYPE, GROUP_CTOR, GROUP_REC } GroupPart;

/* Position{ctor, owner, index} */
typedef struct {
  Str ctor;
  Str owner;
  Str index;
} Position;

/* Maybe<Position> */
typedef struct {
  bool some;
  Position value;
} MaybePosition;

/* Maybe<DeclKind> */
typedef struct {
  bool some;
  DeclKindData value;
} MaybeKind;

/* ---- blanks ---- */

static DeclHints hints_blank(DeclHintsKind kind) {
  DeclHints value;
  value.kind = kind;
  value.height = str_empty();
  return value;
}

static DeclRule rule_blank(void) {
  DeclRule value;
  value.ctor = str_empty();
  value.nfields = str_empty();
  value.rhs = str_empty();
  return value;
}

static DeclKindData kind_blank(Heap *heap, DeclKindTag tag) {
  DeclKindData kind;
  kind.tag = tag;
  kind.unsafe = false;
  kind.value = str_empty();
  kind.hint = hints_blank(DECL_HINT_OPAQUE);
  kind.safe = DECL_SAFE;
  kind.all = vec_new(heap->arena);
  kind.quot = DECL_QUOT_TYPE;
  kind.num_params = str_empty();
  kind.num_indices = str_empty();
  kind.ctors = vec_new(heap->arena);
  kind.num_nested = str_empty();
  kind.recursive = false;
  kind.reflexive = false;
  kind.induct = str_empty();
  kind.cidx = str_empty();
  kind.num_fields = str_empty();
  kind.num_motives = str_empty();
  kind.num_minors = str_empty();
  kind.rules = vec_new(heap->arena);
  kind.k = false;
  return kind;
}

static DeclEntry decl_blank(Heap *heap) {
  DeclEntry value;
  value.name = str_empty();
  value.level_params = vec_new(heap->arena);
  value.typ = str_empty();
  value.kind = kind_blank(heap, DECL_KIND_AXIOM);
  value.line = 0;
  return value;
}

static DeclGroup group_blank(Heap *heap) {
  DeclGroup group;
  group.types = vec_new(heap->arena);
  group.ctors = vec_new(heap->arena);
  group.recs = vec_new(heap->arena);
  group.line = 0;
  return group;
}

static DeclParsed parsed_blank(Heap *heap) {
  DeclParsed parsed;
  parsed.declarations = vec_new(heap->arena);
  parsed.has_group = false;
  parsed.group = group_blank(heap);
  return parsed;
}

static Position position_blank(void) {
  Position value;
  value.ctor = str_empty();
  value.owner = str_empty();
  value.index = str_empty();
  return value;
}

/* ---- result constructors ---- */

static HintsResult hints_done(DeclHints value) {
  HintsResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static HintsResult hints_fail(Str message) {
  HintsResult result;
  result.kind = JSON_FAIL;
  result.value = hints_blank(DECL_HINT_OPAQUE);
  result.message = message;
  return result;
}

static SafetyResult safety_done(DeclSafety value) {
  SafetyResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static SafetyResult safety_fail(Str message) {
  SafetyResult result;
  result.kind = JSON_FAIL;
  result.value = DECL_SAFE;
  result.message = message;
  return result;
}

static QuotResult quot_done(DeclQuotKind value) {
  QuotResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static QuotResult quot_fail(Str message) {
  QuotResult result;
  result.kind = JSON_FAIL;
  result.value = DECL_QUOT_TYPE;
  result.message = message;
  return result;
}

static RuleResult rule_done(DeclRule value) {
  RuleResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static RuleResult rule_fail(Str message) {
  RuleResult result;
  result.kind = JSON_FAIL;
  result.value = rule_blank();
  result.message = message;
  return result;
}

static DeclResult decl_done(DeclEntry value) {
  DeclResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static DeclResult decl_fail(Heap *heap, Str message) {
  DeclResult result;
  result.kind = JSON_FAIL;
  result.value = decl_blank(heap);
  result.message = message;
  return result;
}

static DeclParsedResult parsed_done(DeclParsed value) {
  DeclParsedResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static DeclParsedResult parsed_fail(Heap *heap, Str message) {
  DeclParsedResult result;
  result.kind = JSON_FAIL;
  result.value = parsed_blank(heap);
  result.message = message;
  return result;
}

static JsonListResult list_done(Vec value) {
  JsonListResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static JsonListResult list_fail(Heap *heap, Str message) {
  JsonListResult result;
  result.kind = JSON_FAIL;
  result.value = vec_new(heap->arena);
  result.message = message;
  return result;
}

static UnitResult unit_done(void) {
  UnitResult result;
  result.kind = JSON_DONE;
  result.message = str_empty();
  return result;
}

static UnitResult unit_fail(Str message) {
  UnitResult result;
  result.kind = JSON_FAIL;
  result.message = message;
  return result;
}

/* a ++ b ++ c */
static Str cat3(Heap *heap, Str a, Str b, Str c) { return heap_str_cat(heap, heap_str_cat(heap, a, b), c); }

/* [item] appended to out, as a Str* */
static Vec with_text(Heap *heap, Vec out, Str item) {
  Str *box = json_boxed_str(heap, item);
  if (box == NULL) return out;
  return json_vec_with(heap, out, box);
}

/* a heap-boxed DeclEntry (NULL on allocation failure, heap->oom set) */
static DeclEntry *box_decl(Heap *heap, DeclEntry value) {
  DeclEntry *box = heap_alloc(heap, sizeof(DeclEntry));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

/* a heap-boxed Rule */
static DeclRule *box_rule(Heap *heap, DeclRule value) {
  DeclRule *box = heap_alloc(heap, sizeof(DeclRule));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

/* append(xs, ys): the items of xs then the items of ys, in a new Vec. */
static Vec append_from(Heap *heap, Vec acc, const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  if (got.status != VEC_GET_OK) return acc;
  return append_from(heap, json_vec_with(heap, acc, got.value), xs, index + 1);
}

static Vec list_append(Heap *heap, const Vec *xs, const Vec *ys) {
  return append_from(heap, append_from(heap, vec_new(heap->arena), xs, 0), ys, 0);
}

/* safety(text) */
static SafetyResult safety(Heap *heap, Str text) {
  if (str_eq(text, str_lit("unsafe"))) return safety_done(DECL_UNSAFE);
  if (str_eq(text, str_lit("safe"))) return safety_done(DECL_SAFE);
  if (str_eq(text, str_lit("partial"))) return safety_done(DECL_PARTIAL);
  return safety_fail(heap_str_cat(heap, str_lit("invalid enum "), text));
}

/* quot_kind(text) */
static QuotResult quot_kind(Heap *heap, Str text) {
  if (str_eq(text, str_lit("type"))) return quot_done(DECL_QUOT_TYPE);
  if (str_eq(text, str_lit("ctor"))) return quot_done(DECL_QUOT_CTOR);
  if (str_eq(text, str_lit("lift"))) return quot_done(DECL_QUOT_LIFT);
  if (str_eq(text, str_lit("ind"))) return quot_done(DECL_QUOT_IND);
  return quot_fail(heap_str_cat(heap, str_lit("invalid enum "), text));
}

/* hints_text(text) */
static HintsResult hints_text(Heap *heap, Str text) {
  if (str_eq(text, str_lit("opaque"))) return hints_done(hints_blank(DECL_HINT_OPAQUE));
  if (str_eq(text, str_lit("abbrev"))) return hints_done(hints_blank(DECL_HINT_ABBREV));
  return hints_fail(heap_str_cat(heap, str_lit("invalid enum "), text));
}

/* Regular{height} */
static DeclHints regular(Str height) {
  DeclHints value = hints_blank(DECL_HINT_REGULAR);
  value.height = height;
  return value;
}

/* regular_range(order, height) */
static HintsResult regular_range(Cmp order, Str height) {
  switch (order) {
    case CMP_GT: return hints_fail(str_lit("regular reducibility hint exceeds UInt32"));
    case CMP_LT: return hints_done(regular(height));
    case CMP_EQ: return hints_done(regular(height));
  }
  return hints_done(regular(height));
}

/* regular_length(order, height) */
static HintsResult regular_length(Cmp order, Str height) {
  switch (order) {
    case CMP_LT: return hints_done(regular(height));
    case CMP_GT: return hints_fail(str_lit("regular reducibility hint exceeds UInt32"));
    case CMP_EQ: return regular_range(json_lexical(height, str_lit("4294967295")), height);
  }
  return regular_range(json_lexical(height, str_lit("4294967295")), height);
}

/* Nat.cmp(a, b) */
static Cmp cmp_size(size_t a, size_t b) { return a < b ? CMP_LT : (a > b ? CMP_GT : CMP_EQ); }

/* hints, case Json.Object{fields} */
static HintsResult hints_object(Heap *heap, const Json *value) {
  static const char *const names[] = {"regular"};
  Vec expected = json_expected_keys(heap, names, 1);
  JsonListResult exact = json_exact_object(heap, value, &expected);
  if (exact.kind == JSON_FAIL) return hints_fail(exact.message);
  JsonTextResult height = json_nat_field(heap, &exact.value, str_lit("regular"));
  if (height.kind == JSON_FAIL) return hints_fail(height.message);
  return regular_length(cmp_size(height.value.len, 10), height.value);
}

/* hints(value) */
static HintsResult hints(Heap *heap, const Json *value) {
  switch (value->kind) {
    case JSON_NULL: return hints_fail(str_lit("invalid reducibility hints"));
    case JSON_BOOLEAN: return hints_fail(str_lit("invalid reducibility hints"));
    case JSON_NUMBER: return hints_fail(str_lit("invalid reducibility hints"));
    case JSON_TEXT: return hints_text(heap, value->text);
    case JSON_ITEMS: return hints_fail(str_lit("invalid reducibility hints"));
    case JSON_OBJECT: return hints_object(heap, value);
  }
  return hints_fail(str_lit("invalid reducibility hints"));
}

/* hints_field(fields, key) */
static HintsResult hints_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return hints_fail(value.message);
  return hints(heap, value.value);
}

/* safety_field(fields, key) */
static SafetyResult safety_field(Heap *heap, const Vec *fields, Str key) {
  JsonTextResult value = json_string_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return safety_fail(value.message);
  return safety(heap, value.value);
}

/* quot_field(fields, key) */
static QuotResult quot_field(Heap *heap, const Vec *fields, Str key) {
  JsonTextResult value = json_string_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return quot_fail(value.message);
  return quot_kind(heap, value.value);
}

/* rule(value) */
static RuleResult rule(Heap *heap, const Json *value) {
  static const char *const names[] = {"ctor", "nfields", "rhs"};
  Vec expected = json_expected_keys(heap, names, 3);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return rule_fail(fields.message);
  JsonTextResult ctor = json_nat_field(heap, &fields.value, str_lit("ctor"));
  if (ctor.kind == JSON_FAIL) return rule_fail(ctor.message);
  JsonTextResult nfields = json_nat_field(heap, &fields.value, str_lit("nfields"));
  if (nfields.kind == JSON_FAIL) return rule_fail(nfields.message);
  JsonTextResult rhs = json_nat_field(heap, &fields.value, str_lit("rhs"));
  if (rhs.kind == JSON_FAIL) return rule_fail(rhs.message);
  DeclRule out = rule_blank();
  out.ctor = ctor.value;
  out.nfields = nfields.value;
  out.rhs = rhs.value;
  return rule_done(out);
}

/* rules(values): Result<String, List<Rule>> of DeclRule*, the first failure in order. */
static JsonListResult rules_from(Heap *heap, const Vec *values, size_t index, Vec acc) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return list_done(acc);
  RuleResult item = rule(heap, got.value);
  if (item.kind == JSON_FAIL) return list_fail(heap, item.message);
  DeclRule *box = box_rule(heap, item.value);
  if (box == NULL) return list_done(acc);
  return rules_from(heap, values, index + 1, json_vec_with(heap, acc, box));
}

static JsonListResult rules(Heap *heap, const Vec *values) { return rules_from(heap, values, 0, vec_new(heap->arena)); }

/* rules_field(fields, key) */
static JsonListResult rules_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return list_fail(heap, value.message);
  JsonListResult values = json_array(value.value);
  if (values.kind == JSON_FAIL) return list_fail(heap, values.message);
  return rules(heap, &values.value);
}

/* base(fields, kind, line) */
static DeclResult base(Heap *heap, const Vec *fields, DeclKindData kind, uint32_t line) {
  JsonTextResult name = json_nat_field(heap, fields, str_lit("name"));
  if (name.kind == JSON_FAIL) return decl_fail(heap, name.message);
  JsonListResult levels = json_nats_field(heap, fields, str_lit("levelParams"));
  if (levels.kind == JSON_FAIL) return decl_fail(heap, levels.message);
  JsonTextResult typ = json_nat_field(heap, fields, str_lit("type"));
  if (typ.kind == JSON_FAIL) return decl_fail(heap, typ.message);
  DeclEntry out = decl_blank(heap);
  out.name = name.value;
  out.level_params = levels.value;
  out.typ = typ.value;
  out.kind = kind;
  out.line = line;
  return decl_done(out);
}

/* axiom(value, line) */
static DeclResult axiom(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type", "isUnsafe"};
  Vec expected = json_expected_keys(heap, names, 4);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonBoolResult unsafe = json_bool_field(heap, &fields.value, str_lit("isUnsafe"));
  if (unsafe.kind == JSON_FAIL) return decl_fail(heap, unsafe.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_AXIOM);
  kind.unsafe = unsafe.value;
  return base(heap, &fields.value, kind, line);
}

/* definition(value, line) */
static DeclResult definition(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type", "value", "hints", "safety", "all"};
  Vec expected = json_expected_keys(heap, names, 7);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonTextResult val = json_nat_field(heap, &fields.value, str_lit("value"));
  if (val.kind == JSON_FAIL) return decl_fail(heap, val.message);
  HintsResult hint = hints_field(heap, &fields.value, str_lit("hints"));
  if (hint.kind == JSON_FAIL) return decl_fail(heap, hint.message);
  SafetyResult safe = safety_field(heap, &fields.value, str_lit("safety"));
  if (safe.kind == JSON_FAIL) return decl_fail(heap, safe.message);
  JsonListResult all = json_nats_field(heap, &fields.value, str_lit("all"));
  if (all.kind == JSON_FAIL) return decl_fail(heap, all.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_DEFINITION);
  kind.value = val.value;
  kind.hint = hint.value;
  kind.safe = safe.value;
  kind.all = all.value;
  return base(heap, &fields.value, kind, line);
}

/* theorem(value, line) */
static DeclResult theorem(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type", "value", "all"};
  Vec expected = json_expected_keys(heap, names, 5);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonTextResult val = json_nat_field(heap, &fields.value, str_lit("value"));
  if (val.kind == JSON_FAIL) return decl_fail(heap, val.message);
  JsonListResult all = json_nats_field(heap, &fields.value, str_lit("all"));
  if (all.kind == JSON_FAIL) return decl_fail(heap, all.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_THEOREM);
  kind.value = val.value;
  kind.all = all.value;
  return base(heap, &fields.value, kind, line);
}

/* opaque(value, line) */
static DeclResult opaque(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type", "value", "isUnsafe", "all"};
  Vec expected = json_expected_keys(heap, names, 6);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonTextResult val = json_nat_field(heap, &fields.value, str_lit("value"));
  if (val.kind == JSON_FAIL) return decl_fail(heap, val.message);
  JsonBoolResult unsafe = json_bool_field(heap, &fields.value, str_lit("isUnsafe"));
  if (unsafe.kind == JSON_FAIL) return decl_fail(heap, unsafe.message);
  JsonListResult all = json_nats_field(heap, &fields.value, str_lit("all"));
  if (all.kind == JSON_FAIL) return decl_fail(heap, all.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_OPAQUE);
  kind.value = val.value;
  kind.unsafe = unsafe.value;
  kind.all = all.value;
  return base(heap, &fields.value, kind, line);
}

/* quotient(value, line) */
static DeclResult quotient(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type", "kind"};
  Vec expected = json_expected_keys(heap, names, 4);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  QuotResult quot = quot_field(heap, &fields.value, str_lit("kind"));
  if (quot.kind == JSON_FAIL) return decl_fail(heap, quot.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_QUOTIENT);
  kind.quot = quot.value;
  return base(heap, &fields.value, kind, line);
}

/* inductive(value, line) */
static DeclResult inductive(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name",  "levelParams", "type",      "numParams", "numIndices", "all",
                                      "ctors", "numNested",   "isRec",     "isUnsafe",  "isReflexive"};
  Vec expected = json_expected_keys(heap, names, 11);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonTextResult num_params = json_nat_field(heap, &fields.value, str_lit("numParams"));
  if (num_params.kind == JSON_FAIL) return decl_fail(heap, num_params.message);
  JsonTextResult num_indices = json_nat_field(heap, &fields.value, str_lit("numIndices"));
  if (num_indices.kind == JSON_FAIL) return decl_fail(heap, num_indices.message);
  JsonListResult all = json_nats_field(heap, &fields.value, str_lit("all"));
  if (all.kind == JSON_FAIL) return decl_fail(heap, all.message);
  JsonListResult ctors = json_nats_field(heap, &fields.value, str_lit("ctors"));
  if (ctors.kind == JSON_FAIL) return decl_fail(heap, ctors.message);
  JsonTextResult num_nested = json_nat_field(heap, &fields.value, str_lit("numNested"));
  if (num_nested.kind == JSON_FAIL) return decl_fail(heap, num_nested.message);
  JsonBoolResult recursive = json_bool_field(heap, &fields.value, str_lit("isRec"));
  if (recursive.kind == JSON_FAIL) return decl_fail(heap, recursive.message);
  JsonBoolResult unsafe = json_bool_field(heap, &fields.value, str_lit("isUnsafe"));
  if (unsafe.kind == JSON_FAIL) return decl_fail(heap, unsafe.message);
  JsonBoolResult reflexive = json_bool_field(heap, &fields.value, str_lit("isReflexive"));
  if (reflexive.kind == JSON_FAIL) return decl_fail(heap, reflexive.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_INDUCTIVE);
  kind.num_params = num_params.value;
  kind.num_indices = num_indices.value;
  kind.all = all.value;
  kind.ctors = ctors.value;
  kind.num_nested = num_nested.value;
  kind.recursive = recursive.value;
  kind.unsafe = unsafe.value;
  kind.reflexive = reflexive.value;
  return base(heap, &fields.value, kind, line);
}

/* constructor(value, line) */
static DeclResult constructor(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name", "levelParams", "type",      "induct",
                                      "cidx", "numParams",   "numFields", "isUnsafe"};
  Vec expected = json_expected_keys(heap, names, 8);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonTextResult induct = json_nat_field(heap, &fields.value, str_lit("induct"));
  if (induct.kind == JSON_FAIL) return decl_fail(heap, induct.message);
  JsonTextResult cidx = json_nat_field(heap, &fields.value, str_lit("cidx"));
  if (cidx.kind == JSON_FAIL) return decl_fail(heap, cidx.message);
  JsonTextResult num_params = json_nat_field(heap, &fields.value, str_lit("numParams"));
  if (num_params.kind == JSON_FAIL) return decl_fail(heap, num_params.message);
  JsonTextResult num_fields = json_nat_field(heap, &fields.value, str_lit("numFields"));
  if (num_fields.kind == JSON_FAIL) return decl_fail(heap, num_fields.message);
  JsonBoolResult unsafe = json_bool_field(heap, &fields.value, str_lit("isUnsafe"));
  if (unsafe.kind == JSON_FAIL) return decl_fail(heap, unsafe.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_CONSTRUCTOR);
  kind.induct = induct.value;
  kind.cidx = cidx.value;
  kind.num_params = num_params.value;
  kind.num_fields = num_fields.value;
  kind.unsafe = unsafe.value;
  return base(heap, &fields.value, kind, line);
}

/* recursor(value, line) */
static DeclResult recursor(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"name",       "levelParams", "type",  "all", "numParams", "numIndices",
                                      "numMotives", "numMinors",   "rules", "k",   "isUnsafe"};
  Vec expected = json_expected_keys(heap, names, 11);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return decl_fail(heap, fields.message);
  JsonListResult all = json_nats_field(heap, &fields.value, str_lit("all"));
  if (all.kind == JSON_FAIL) return decl_fail(heap, all.message);
  JsonTextResult num_params = json_nat_field(heap, &fields.value, str_lit("numParams"));
  if (num_params.kind == JSON_FAIL) return decl_fail(heap, num_params.message);
  JsonTextResult num_indices = json_nat_field(heap, &fields.value, str_lit("numIndices"));
  if (num_indices.kind == JSON_FAIL) return decl_fail(heap, num_indices.message);
  JsonTextResult num_motives = json_nat_field(heap, &fields.value, str_lit("numMotives"));
  if (num_motives.kind == JSON_FAIL) return decl_fail(heap, num_motives.message);
  JsonTextResult num_minors = json_nat_field(heap, &fields.value, str_lit("numMinors"));
  if (num_minors.kind == JSON_FAIL) return decl_fail(heap, num_minors.message);
  JsonListResult rule_list = rules_field(heap, &fields.value, str_lit("rules"));
  if (rule_list.kind == JSON_FAIL) return decl_fail(heap, rule_list.message);
  JsonBoolResult k = json_bool_field(heap, &fields.value, str_lit("k"));
  if (k.kind == JSON_FAIL) return decl_fail(heap, k.message);
  JsonBoolResult unsafe = json_bool_field(heap, &fields.value, str_lit("isUnsafe"));
  if (unsafe.kind == JSON_FAIL) return decl_fail(heap, unsafe.message);
  DeclKindData kind = kind_blank(heap, DECL_KIND_RECURSOR);
  kind.all = all.value;
  kind.num_params = num_params.value;
  kind.num_indices = num_indices.value;
  kind.num_motives = num_motives.value;
  kind.num_minors = num_minors.value;
  kind.rules = rule_list.value;
  kind.k = k.value;
  kind.unsafe = unsafe.value;
  return base(heap, &fields.value, kind, line);
}

/* group_item(part, value, line) */
static DeclResult group_item(Heap *heap, GroupPart part, const Json *value, uint32_t line) {
  switch (part) {
    case GROUP_TYPE: return inductive(heap, value, line);
    case GROUP_CTOR: return constructor(heap, value, line);
    case GROUP_REC: return recursor(heap, value, line);
  }
  return recursor(heap, value, line);
}

/* group_items(items, part, line): Result<String, List<T>> of DeclEntry*, the first failure in order. */
static JsonListResult group_items_from(Heap *heap, const Vec *items, GroupPart part, uint32_t line, size_t index,
                                       Vec acc) {
  VecGet got = vec_get(items, index);
  if (got.status != VEC_GET_OK) return list_done(acc);
  DeclResult item = group_item(heap, part, got.value, line);
  if (item.kind == JSON_FAIL) return list_fail(heap, item.message);
  DeclEntry *box = box_decl(heap, item.value);
  if (box == NULL) return list_done(acc);
  return group_items_from(heap, items, part, line, index + 1, json_vec_with(heap, acc, box));
}

static JsonListResult group_items(Heap *heap, const Vec *items, GroupPart part, uint32_t line) {
  return group_items_from(heap, items, part, line, 0, vec_new(heap->arena));
}

/* group_field(fields, key, part, line) */
static JsonListResult group_field(Heap *heap, const Vec *fields, Str key, GroupPart part, uint32_t line) {
  JsonResult raw = json_field(heap, fields, key);
  if (raw.kind == JSON_FAIL) return list_fail(heap, raw.message);
  JsonListResult items = json_array(raw.value);
  if (items.kind == JSON_FAIL) return list_fail(heap, items.message);
  return group_items(heap, &items.value, part, line);
}

/* positions(ctors, owner, index): one Position* per constructor name, index shown as decimal text. */
static Vec positions_from(Heap *heap, const Vec *ctors, Str owner, size_t index, Vec acc) {
  VecGet got = vec_get(ctors, index);
  if (got.status != VEC_GET_OK) return acc;
  Position *box = heap_alloc(heap, sizeof(Position));
  if (box == NULL) return acc;
  *box = position_blank();
  box->ctor = *(const Str *)got.value;
  box->owner = owner;
  box->index = heap_nat_show(heap, heap_nat_of_u64(heap, (uint64_t)index));
  return positions_from(heap, ctors, owner, index + 1, json_vec_with(heap, acc, box));
}

/* type_positions(types): the positions of every inductive type of the list, in order. */
static Vec type_positions_from(Heap *heap, const Vec *types, size_t index, Vec acc) {
  VecGet got = vec_get(types, index);
  if (got.status != VEC_GET_OK) return acc;
  const DeclEntry *item = got.value;
  if (item->kind.tag != DECL_KIND_INDUCTIVE) return type_positions_from(heap, types, index + 1, acc);
  return type_positions_from(heap, types, index + 1, positions_from(heap, &item->kind.ctors, item->name, 0, acc));
}

static Vec type_positions(Heap *heap, const Vec *types) {
  return type_positions_from(heap, types, 0, vec_new(heap->arena));
}

static MaybePosition no_position(void) {
  MaybePosition result;
  result.some = false;
  result.value = position_blank();
  return result;
}

static MaybePosition some_position(Position value) {
  MaybePosition result;
  result.some = true;
  result.value = value;
  return result;
}

/* find_position(positions, name): the first position whose ctor is name (position_choice). */
static MaybePosition find_position_from(const Vec *positions, Str name, size_t index) {
  VecGet got = vec_get(positions, index);
  if (got.status != VEC_GET_OK) return no_position();
  const Position *item = got.value;
  if (str_eq(item->ctor, name)) return some_position(*item);
  return find_position_from(positions, name, index + 1);
}

static MaybePosition find_position(const Vec *positions, Str name) { return find_position_from(positions, name, 0); }

/* constructor_matches(valid, name, owner) */
static UnitResult constructor_matches(Heap *heap, bool valid, Str name, Str owner) {
  if (valid) return unit_done();
  return unit_fail(cat3(heap, heap_str_cat(heap, str_lit("constructor "), name),
                        str_lit(" disagrees with the ctors list of inductive type "), owner));
}

/* constructor_position(found, name, induct, cidx) */
static UnitResult constructor_position(Heap *heap, MaybePosition found, Str name, Str induct, Str cidx) {
  if (!found.some)
    return unit_fail(cat3(heap, str_lit("constructor "), name,
                          str_lit(" is not listed by an inductive type of its group")));
  return constructor_matches(heap, str_eq(induct, found.value.owner) && str_eq(cidx, found.value.index), name,
                             found.value.owner);
}

/* check_constructor(value, positions) */
static UnitResult check_constructor(Heap *heap, const DeclEntry *value, const Vec *positions) {
  if (value->kind.tag != DECL_KIND_CONSTRUCTOR) return unit_done();
  return constructor_position(heap, find_position(positions, value->name), value->name, value->kind.induct,
                              value->kind.cidx);
}

/* check_constructors(values, positions) */
static UnitResult check_constructors(Heap *heap, const Vec *values, const Vec *positions, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return unit_done();
  UnitResult checked = check_constructor(heap, got.value, positions);
  if (checked.kind == JSON_FAIL) return checked;
  return check_constructors(heap, values, positions, index + 1);
}

static MaybeKind no_kind(Heap *heap) {
  MaybeKind result;
  result.some = false;
  result.value = kind_blank(heap, DECL_KIND_AXIOM);
  return result;
}

static MaybeKind some_kind(DeclKindData value) {
  MaybeKind result;
  result.some = true;
  result.value = value;
  return result;
}

/* member(values, name): the kind of the first declaration named name (member_choice). */
static MaybeKind member_from(Heap *heap, const Vec *values, Str name, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return no_kind(heap);
  const DeclEntry *item = got.value;
  if (str_eq(item->name, name)) return some_kind(item->kind);
  return member_from(heap, values, name, index + 1);
}

static MaybeKind member(Heap *heap, const Vec *values, Str name) { return member_from(heap, values, name, 0); }

/* rule_matches(valid, name, expected, actual) */
static UnitResult rule_matches(Heap *heap, bool valid, Str name, Str expected, Str actual) {
  if (valid) return unit_done();
  Str head = cat3(heap, str_lit("recursor rule for constructor "), name, str_lit(" declares "));
  return unit_fail(cat3(heap, heap_str_cat(heap, head, expected), str_lit(" fields but the constructor has "),
                        actual));
}

/* rule_member(found, name, nfields) */
static UnitResult rule_member(Heap *heap, MaybeKind found, Str name, Str nfields) {
  if (!found.some) return unit_done();
  if (found.value.tag == DECL_KIND_CONSTRUCTOR)
    return rule_matches(heap, str_eq(found.value.num_fields, nfields), name, nfields, found.value.num_fields);
  return unit_fail(cat3(heap, str_lit("recursor rule names "), name,
                        str_lit(", a declaration of its group that is not a constructor")));
}

/* check_rule(rule, members) */
static UnitResult check_rule(Heap *heap, const DeclRule *item, const Vec *members) {
  return rule_member(heap, member(heap, members, item->ctor), item->ctor, item->nfields);
}

/* check_rules(rules, members) */
static UnitResult check_rules(Heap *heap, const Vec *rule_list, const Vec *members, size_t index) {
  VecGet got = vec_get(rule_list, index);
  if (got.status != VEC_GET_OK) return unit_done();
  UnitResult checked = check_rule(heap, got.value, members);
  if (checked.kind == JSON_FAIL) return checked;
  return check_rules(heap, rule_list, members, index + 1);
}

/* recursor_rules(recs): the rules of every recursor of the list, in order. */
static Vec recursor_rules_from(Heap *heap, const Vec *recs, size_t index, Vec acc) {
  VecGet got = vec_get(recs, index);
  if (got.status != VEC_GET_OK) return acc;
  const DeclEntry *item = got.value;
  if (item->kind.tag != DECL_KIND_RECURSOR) return recursor_rules_from(heap, recs, index + 1, acc);
  return recursor_rules_from(heap, recs, index + 1, append_from(heap, acc, &item->kind.rules, 0));
}

static Vec recursor_rules(Heap *heap, const Vec *recs) {
  return recursor_rules_from(heap, recs, 0, vec_new(heap->arena));
}

/* group_checks(types, ctors, recs) */
static UnitResult group_checks(Heap *heap, const Vec *types, const Vec *ctors, const Vec *recs) {
  Vec places = type_positions(heap, types);
  UnitResult checked = check_constructors(heap, ctors, &places, 0);
  if (checked.kind == JSON_FAIL) return checked;
  Vec rule_list = recursor_rules(heap, recs);
  Vec tail = list_append(heap, ctors, recs);
  Vec members = list_append(heap, types, &tail);
  return check_rules(heap, &rule_list, &members, 0);
}

/* ordinary(tag, value, line) */
static DeclResult ordinary(Heap *heap, Str tag, const Json *value, uint32_t line) {
  if (str_eq(tag, str_lit("axiom"))) return axiom(heap, value, line);
  if (str_eq(tag, str_lit("def"))) return definition(heap, value, line);
  if (str_eq(tag, str_lit("thm"))) return theorem(heap, value, line);
  if (str_eq(tag, str_lit("opaque"))) return opaque(heap, value, line);
  if (str_eq(tag, str_lit("quot"))) return quotient(heap, value, line);
  return decl_fail(heap, heap_str_cat(heap, str_lit("unknown declaration tag "), tag));
}

/* parse, case "inductive" */
static DeclParsedResult parse_group(Heap *heap, const Json *value, uint32_t line) {
  static const char *const names[] = {"types", "ctors", "recs"};
  Vec expected = json_expected_keys(heap, names, 3);
  JsonListResult fields = json_exact_object(heap, value, &expected);
  if (fields.kind == JSON_FAIL) return parsed_fail(heap, fields.message);
  JsonListResult types = group_field(heap, &fields.value, str_lit("types"), GROUP_TYPE, line);
  if (types.kind == JSON_FAIL) return parsed_fail(heap, types.message);
  JsonListResult ctors = group_field(heap, &fields.value, str_lit("ctors"), GROUP_CTOR, line);
  if (ctors.kind == JSON_FAIL) return parsed_fail(heap, ctors.message);
  JsonListResult recs = group_field(heap, &fields.value, str_lit("recs"), GROUP_REC, line);
  if (recs.kind == JSON_FAIL) return parsed_fail(heap, recs.message);
  UnitResult checked = group_checks(heap, &types.value, &ctors.value, &recs.value);
  if (checked.kind == JSON_FAIL) return parsed_fail(heap, checked.message);
  Vec tail = list_append(heap, &ctors.value, &recs.value);
  DeclParsed out = parsed_blank(heap);
  out.declarations = list_append(heap, &types.value, &tail);
  out.has_group = true;
  out.group.types = types.value;
  out.group.ctors = ctors.value;
  out.group.recs = recs.value;
  out.group.line = line;
  return parsed_done(out);
}

/* parse(tag, value, line) */
DeclParsedResult decl_parse(Heap *heap, Str tag, const Json *value, uint32_t line) {
  if (str_eq(tag, str_lit("inductive"))) return parse_group(heap, value, line);
  DeclResult declaration = ordinary(heap, tag, value, line);
  if (declaration.kind == JSON_FAIL) return parsed_fail(heap, declaration.message);
  DeclParsed out = parsed_blank(heap);
  DeclEntry *box = box_decl(heap, declaration.value);
  if (box == NULL) return parsed_done(out);
  out.declarations = json_vec_with(heap, out.declarations, box);
  return parsed_done(out);
}

/* rule_names(rules): the ctor of every rule. */
static Vec rule_names_from(Heap *heap, const Vec *rule_list, size_t index, Vec acc) {
  VecGet got = vec_get(rule_list, index);
  if (got.status != VEC_GET_OK) return acc;
  const DeclRule *item = got.value;
  return rule_names_from(heap, rule_list, index + 1, with_text(heap, acc, item->ctor));
}

static Vec rule_names(Heap *heap, const Vec *rule_list) {
  return rule_names_from(heap, rule_list, 0, vec_new(heap->arena));
}

/* rule_exprs(rules): the rhs of every rule. */
static Vec rule_exprs_from(Heap *heap, const Vec *rule_list, size_t index, Vec acc) {
  VecGet got = vec_get(rule_list, index);
  if (got.status != VEC_GET_OK) return acc;
  const DeclRule *item = got.value;
  return rule_exprs_from(heap, rule_list, index + 1, with_text(heap, acc, item->rhs));
}

static Vec rule_exprs(Heap *heap, const Vec *rule_list) {
  return rule_exprs_from(heap, rule_list, 0, vec_new(heap->arena));
}

/* extra_names(kind) */
static Vec extra_names(Heap *heap, const DeclKindData *kind) {
  switch (kind->tag) {
    case DECL_KIND_AXIOM: return vec_new(heap->arena);
    case DECL_KIND_DEFINITION: return kind->all;
    case DECL_KIND_THEOREM: return kind->all;
    case DECL_KIND_OPAQUE: return kind->all;
    case DECL_KIND_QUOTIENT: return vec_new(heap->arena);
    case DECL_KIND_INDUCTIVE: return list_append(heap, &kind->all, &kind->ctors);
    case DECL_KIND_CONSTRUCTOR: return with_text(heap, vec_new(heap->arena), kind->induct);
    case DECL_KIND_RECURSOR: {
      Vec names = rule_names(heap, &kind->rules);
      return list_append(heap, &kind->all, &names);
    }
  }
  return vec_new(heap->arena);
}

/* name_references(declaration): the name, the level parameter names and the extra names of the kind. */
Vec decl_name_references(Heap *heap, const DeclEntry *declaration) {
  Vec extra = extra_names(heap, &declaration->kind);
  Vec rest = list_append(heap, &declaration->level_params, &extra);
  return append_from(heap, with_text(heap, vec_new(heap->arena), declaration->name), &rest, 0);
}

/* extra_exprs(kind) */
static Vec extra_exprs(Heap *heap, const DeclKindData *kind) {
  switch (kind->tag) {
    case DECL_KIND_AXIOM: return vec_new(heap->arena);
    case DECL_KIND_DEFINITION: return with_text(heap, vec_new(heap->arena), kind->value);
    case DECL_KIND_THEOREM: return with_text(heap, vec_new(heap->arena), kind->value);
    case DECL_KIND_OPAQUE: return with_text(heap, vec_new(heap->arena), kind->value);
    case DECL_KIND_QUOTIENT: return vec_new(heap->arena);
    case DECL_KIND_INDUCTIVE: return vec_new(heap->arena);
    case DECL_KIND_CONSTRUCTOR: return vec_new(heap->arena);
    case DECL_KIND_RECURSOR: return rule_exprs(heap, &kind->rules);
  }
  return vec_new(heap->arena);
}

/* expr_references(declaration): the type and the extra expressions of the kind. */
Vec decl_expr_references(Heap *heap, const DeclEntry *declaration) {
  Vec extra = extra_exprs(heap, &declaration->kind);
  return append_from(heap, with_text(heap, vec_new(heap->arena), declaration->typ), &extra, 0);
}

/* kind_name(kind) */
Str decl_kind_name(const DeclKindData *kind) {
  switch (kind->tag) {
    case DECL_KIND_AXIOM: return str_lit("axiom");
    case DECL_KIND_DEFINITION: return str_lit("def");
    case DECL_KIND_THEOREM: return str_lit("thm");
    case DECL_KIND_OPAQUE: return str_lit("opaque");
    case DECL_KIND_QUOTIENT: return str_lit("quot");
    case DECL_KIND_INDUCTIVE: return str_lit("inductive");
    case DECL_KIND_CONSTRUCTOR: return str_lit("constructor");
    case DECL_KIND_RECURSOR: return str_lit("recursor");
  }
  return str_lit("axiom");
}
