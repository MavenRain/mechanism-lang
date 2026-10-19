/* Port of import/levels.bend: the universe level entries of a lean4export NDJSON stream
 * (LZero, LSucc{index}, LMax{left, right}, LImax{left, right}, LParam{index}); indices stay text. */
#include "import.h"

/* LevelPair{left, right} */
typedef struct {
  Str left;
  Str right;
} LevelPair;

/* Result<String, LevelPair> */
typedef struct {
  JsonResultKind kind;
  LevelPair value;
  Str message;
} LevelPairResult;

static Level level_blank(LevelKind kind) {
  Level value;
  value.kind = kind;
  value.index = str_empty();
  value.left = str_empty();
  value.right = str_empty();
  return value;
}

static LevelResult level_done(Level value) {
  LevelResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static LevelResult level_fail(Str message) {
  LevelResult result;
  result.kind = JSON_FAIL;
  result.value = level_blank(LEVEL_ZERO);
  result.message = message;
  return result;
}

static LevelPairResult pair_done(Str left, Str right) {
  LevelPairResult result;
  result.kind = JSON_DONE;
  result.value.left = left;
  result.value.right = right;
  result.message = str_empty();
  return result;
}

static LevelPairResult pair_fail(Str message) {
  LevelPairResult result;
  result.kind = JSON_FAIL;
  result.value.left = str_empty();
  result.value.right = str_empty();
  result.message = message;
  return result;
}

/* pair_items(values): exactly two Str* items. */
static LevelPairResult pair_items(const Vec *values) {
  VecGet left = vec_get(values, 0);
  VecGet right = vec_get(values, 1);
  if (values->len == 2 && left.status == VEC_GET_OK && right.status == VEC_GET_OK)
    return pair_done(*(const Str *)left.value, *(const Str *)right.value);
  return pair_fail(str_lit("expected exactly two level indices"));
}

/* pair(value) */
static LevelPairResult pair(Heap *heap, const Json *value) {
  JsonListResult items = json_nat_list(heap, value);
  if (items.kind == JSON_FAIL) return pair_fail(items.message);
  return pair_items(&items.value);
}

/* maximum(value, imax) */
static Level maximum(LevelPair value, bool imax) {
  Level level = level_blank(imax ? LEVEL_IMAX : LEVEL_MAX);
  level.left = value.left;
  level.right = value.right;
  return level;
}

/* LSucc{index} or LParam{index} from Json.nat(value) */
static LevelResult indexed(LevelKind kind, const Json *value) {
  JsonTextResult index = json_nat(value);
  if (index.kind == JSON_FAIL) return level_fail(index.message);
  Level level = level_blank(kind);
  level.index = index.value;
  return level_done(level);
}

/* LMax or LImax from pair(value) */
static LevelResult paired(Heap *heap, const Json *value, bool imax) {
  LevelPairResult both = pair(heap, value);
  if (both.kind == JSON_FAIL) return level_fail(both.message);
  return level_done(maximum(both.value, imax));
}

/* parse(tag, value) */
LevelResult level_parse(Heap *heap, Str tag, const Json *value) {
  if (str_eq(tag, str_lit("succ"))) return indexed(LEVEL_SUCC, value);
  if (str_eq(tag, str_lit("param"))) return indexed(LEVEL_PARAM, value);
  if (str_eq(tag, str_lit("max"))) return paired(heap, value, false);
  if (str_eq(tag, str_lit("imax"))) return paired(heap, value, true);
  return level_fail(heap_str_cat(heap, str_lit("unknown level tag "), tag));
}

/* [item] appended to out, as a Str* */
static Vec with_text(Heap *heap, Vec out, Str item) {
  Str *box = json_boxed_str(heap, item);
  if (box == NULL) return out;
  return json_vec_with(heap, out, box);
}

/* name_references(value): the name indices the level refers to. */
Vec level_name_references(Heap *heap, Level value) {
  Vec out = vec_new(heap->arena);
  switch (value.kind) {
    case LEVEL_ZERO: return out;
    case LEVEL_SUCC: return out;
    case LEVEL_MAX: return out;
    case LEVEL_IMAX: return out;
    case LEVEL_PARAM: return with_text(heap, out, value.index);
  }
  return out;
}

/* level_references(value): the level indices the level refers to. */
Vec level_level_references(Heap *heap, Level value) {
  Vec out = vec_new(heap->arena);
  switch (value.kind) {
    case LEVEL_ZERO: return out;
    case LEVEL_SUCC: return with_text(heap, out, value.index);
    case LEVEL_MAX: return with_text(heap, with_text(heap, out, value.left), value.right);
    case LEVEL_IMAX: return with_text(heap, with_text(heap, out, value.left), value.right);
    case LEVEL_PARAM: return out;
  }
  return out;
}
