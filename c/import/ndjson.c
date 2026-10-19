/* Port of import/ndjson.bend: a JSON reader driven by an explicit state machine (one step per
 * input character or finite reduction, the 512-level nesting bound of the original reader), a compact
 * renderer and the typed field accessors. The Bend version walks a String of codepoints; this version
 * walks the UTF-8 bytes of the Str: every structural character is one ASCII byte, a raw string
 * character is decoded to check it and advances the column by its byte width (as byte_width does in
 * the Bend source), and escapes are encoded as UTF-8 when they enter the text. */
#include "import.h"

/* ---- placeholders returned after a failed allocation ---- */
static Json json_null_placeholder = {JSON_NULL, false, {"", 0}, {0}, {0}};
static JsonField json_field_placeholder = {{"", 0}, &json_null_placeholder};

static Json *json_node(Heap *heap, JsonKind kind) {
  Json *node = heap_alloc(heap, sizeof(Json));
  if (node == NULL) return &json_null_placeholder;
  node->kind = kind;
  node->values = vec_new(heap->arena);
  node->fields = vec_new(heap->arena);
  return node;
}

Json *json_null(Heap *heap) { return json_node(heap, JSON_NULL); }

Json *json_boolean(Heap *heap, bool value) {
  Json *node = json_node(heap, JSON_BOOLEAN);
  if (node == &json_null_placeholder) return node;
  node->boolean = value;
  return node;
}

Json *json_number(Heap *heap, Str value) {
  Json *node = json_node(heap, JSON_NUMBER);
  if (node == &json_null_placeholder) return node;
  node->text = value;
  return node;
}

Json *json_text(Heap *heap, Str value) {
  Json *node = json_node(heap, JSON_TEXT);
  if (node == &json_null_placeholder) return node;
  node->text = value;
  return node;
}

Json *json_items(Heap *heap, Vec values) {
  Json *node = json_node(heap, JSON_ITEMS);
  if (node == &json_null_placeholder) return node;
  node->values = values;
  return node;
}

Json *json_object(Heap *heap, Vec fields) {
  Json *node = json_node(heap, JSON_OBJECT);
  if (node == &json_null_placeholder) return node;
  node->fields = fields;
  return node;
}

JsonField *json_field_new(Heap *heap, Str key, Json *value) {
  JsonField *field = heap_alloc(heap, sizeof(JsonField));
  if (field == NULL) return &json_field_placeholder;
  field->key = key;
  field->value = value;
  return field;
}

/* ---- parser state (Cursor, Frame, Target, NumberPhase, Mode, State) ---- */

/* Cursor: At{rest, column} */
typedef struct {
  Str rest;
  uint32_t column;
} Cursor;

typedef enum { FRAME_ARRAY, FRAME_OBJECT } FrameKind;
/* Frame: ArrayFrame{items} or ObjectFrame{fields, key}; the frame list is a cons list as in Bend. */
typedef struct FrameList FrameList;
struct FrameList {
  FrameKind kind;
  Vec items;  /* FRAME_ARRAY: Json* so far */
  Vec fields; /* FRAME_OBJECT: JsonField* so far */
  Str key;    /* FRAME_OBJECT */
  FrameList *rest;
};

typedef enum { TARGET_VALUE_TEXT, TARGET_KEY_TEXT } TargetKind;
/* Target: ValueText{} or KeyText{fields} */
typedef struct {
  TargetKind kind;
  Vec fields; /* TARGET_KEY_TEXT */
} Target;

typedef enum {
  PHASE_MINUS,
  PHASE_ZERO_INT,
  PHASE_INTEGER,
  PHASE_DOT,
  PHASE_FRACTION,
  PHASE_EXPONENT,
  PHASE_EXP_SIGN,
  PHASE_EXP_DIGITS
} NumberPhase;

typedef enum {
  MODE_VALUE,
  MODE_ARRAY_START,
  MODE_ARRAY_AFTER,
  MODE_OBJECT_START,
  MODE_OBJECT_AFTER,
  MODE_COLON,
  MODE_STRING_CHARS,
  MODE_ESCAPE,
  MODE_UNICODE,
  MODE_LOW_SLASH,
  MODE_LOW_U,
  MODE_NUMERIC,
  MODE_LITERAL,
  MODE_READY
} ModeKind;

/* Mode: the Bend `reversed` accumulators are StrBuf values in source order. */
typedef struct {
  ModeKind kind;
  Vec items;          /* ArrayAfter */
  Vec fields;         /* ObjectStart, ObjectAfter, Colon */
  bool empty_ok;      /* ObjectStart */
  Str key;            /* Colon */
  Target target;      /* StringChars, Escape, Unicode, LowSlash, LowU */
  StrBuf text;        /* StringChars, Escape, Unicode, LowSlash, LowU, Numeric */
  uint32_t remaining; /* Unicode */
  uint32_t scalar;    /* Unicode */
  bool has_high;      /* Unicode: high is Some */
  uint32_t high;      /* Unicode (when has_high), LowSlash, LowU */
  NumberPhase phase;  /* Numeric */
  Str expected;       /* Literal */
  Json *value;        /* Literal, Ready */
} Mode;

typedef enum { STATE_CONTINUE, STATE_COMPLETE, STATE_INVALID } StateKind;
/* State: Continue{mode, cursor, frames, depth}, Complete{value} or Invalid{message} */
typedef struct {
  StateKind kind;
  Mode mode;
  Cursor cursor;
  FrameList *frames;
  uint32_t depth;
  Json *value;
  Str message;
} State;

/* A decoded UTF-8 character: the codepoint and the bytes it spans. */
typedef struct {
  uint32_t code;
  size_t width;
} Utf8Char;

/* NumericAction: TakeDigit{phase}, EndNumber{} or BadNumber{message} */
typedef enum { ACTION_TAKE_DIGIT, ACTION_END_NUMBER, ACTION_BAD_NUMBER } NumericActionKind;
typedef struct {
  NumericActionKind kind;
  NumberPhase phase; /* ACTION_TAKE_DIGIT */
  Str message;       /* ACTION_BAD_NUMBER */
} NumericAction;

/* ---- helpers ---- */

static Cursor at(Str rest, uint32_t column) {
  Cursor cursor;
  cursor.rest = rest;
  cursor.column = column;
  return cursor;
}

/* The byte after the first one: the Bend `rest` of SCon{ch, rest}. */
static Str tail(Str s) {
  StrSlice slice = str_slice(s, 1, s.len);
  return slice.status == STR_INDEX_OK ? slice.value : str_empty();
}

static Str drop(Str s, size_t count) {
  StrSlice slice = str_slice(s, count, s.len);
  return slice.status == STR_INDEX_OK ? slice.value : str_empty();
}

/* The first byte, or 256 for an empty string. */
static uint32_t head(Str s) {
  StrAt first = str_at(s, 0);
  return first.status == STR_INDEX_OK ? first.value : 256u;
}

static Str cat(Heap *heap, Str a, Str b) { return heap_str_cat(heap, a, b); }

static void note(Heap *heap, bool ok) {
  if (!ok) heap->oom = true;
}

static Mode mode_of(ModeKind kind) {
  Mode mode;
  mode.kind = kind;
  mode.items.arena = NULL;
  mode.items.items = NULL;
  mode.items.len = 0;
  mode.items.cap = 0;
  mode.fields = mode.items;
  mode.empty_ok = false;
  mode.key = str_empty();
  mode.target.kind = TARGET_VALUE_TEXT;
  mode.target.fields = mode.items;
  mode.text.arena = NULL;
  mode.text.data = NULL;
  mode.text.len = 0;
  mode.text.cap = 0;
  mode.remaining = 0;
  mode.scalar = 0;
  mode.has_high = false;
  mode.high = 0;
  mode.phase = PHASE_MINUS;
  mode.expected = str_empty();
  mode.value = &json_null_placeholder;
  return mode;
}

static State continue_with(Mode mode, Cursor cursor, FrameList *frames, uint32_t depth) {
  State state;
  state.kind = STATE_CONTINUE;
  state.mode = mode;
  state.cursor = cursor;
  state.frames = frames;
  state.depth = depth;
  state.value = &json_null_placeholder;
  state.message = str_empty();
  return state;
}

static State complete(Json *value) {
  State state = continue_with(mode_of(MODE_READY), at(str_empty(), 0), NULL, 0);
  state.kind = STATE_COMPLETE;
  state.value = value;
  return state;
}

static State invalid(Str message) {
  State state = continue_with(mode_of(MODE_READY), at(str_empty(), 0), NULL, 0);
  state.kind = STATE_INVALID;
  state.message = message;
  return state;
}

/* Pushes the UTF-8 encoding of the codepoint. */
static bool push_scalar(StrBuf *buf, uint32_t code) {
  if (code < 0x80) return strbuf_push_byte(buf, (char)code);
  if (code < 0x800)
    return strbuf_push_byte(buf, (char)(0xC0 | (code >> 6))) && strbuf_push_byte(buf, (char)(0x80 | (code & 63)));
  if (code < 0x10000)
    return strbuf_push_byte(buf, (char)(0xE0 | (code >> 12))) && strbuf_push_byte(buf, (char)(0x80 | ((code >> 6) & 63))) &&
           strbuf_push_byte(buf, (char)(0x80 | (code & 63)));
  return strbuf_push_byte(buf, (char)(0xF0 | (code >> 18))) && strbuf_push_byte(buf, (char)(0x80 | ((code >> 12) & 63))) &&
         strbuf_push_byte(buf, (char)(0x80 | ((code >> 6) & 63))) && strbuf_push_byte(buf, (char)(0x80 | (code & 63)));
}

static bool continuation(Str s, size_t index) {
  StrAt byte = str_at(s, index);
  return byte.status == STR_INDEX_OK && (byte.value & 0xC0) == 0x80;
}

static uint32_t low_bits(Str s, size_t index) {
  StrAt byte = str_at(s, index);
  return byte.status == STR_INDEX_OK ? (uint32_t)(byte.value & 63) : 0;
}

static Utf8Char utf8_char(uint32_t code, size_t width) {
  Utf8Char ch;
  ch.code = code;
  ch.width = width;
  return ch;
}

/* Decodes the first character of the non-empty string; width zero marks invalid UTF-8. */
static Utf8Char decode_char(Str s) {
  uint32_t lead = head(s);
  if (lead < 0x80) return utf8_char(lead, 1);
  if (lead >= 0xC2 && lead < 0xE0 && continuation(s, 1)) return utf8_char(((lead & 31) << 6) | low_bits(s, 1), 2);
  if (lead >= 0xE0 && lead < 0xF0 && continuation(s, 1) && continuation(s, 2)) {
    uint32_t code = ((lead & 15) << 12) | (low_bits(s, 1) << 6) | low_bits(s, 2);
    if (code >= 0x800 && (code < 0xD800 || code > 0xDFFF)) return utf8_char(code, 3);
  }
  if (lead >= 0xF0 && lead < 0xF5 && continuation(s, 1) && continuation(s, 2) && continuation(s, 3)) {
    uint32_t code = ((lead & 7) << 18) | (low_bits(s, 1) << 12) | (low_bits(s, 2) << 6) | low_bits(s, 3);
    if (code >= 0x10000 && code <= 0x10FFFF) return utf8_char(code, 4);
  }
  return utf8_char(0, 0);
}

static StrBuf text_new(Heap *heap) { return strbuf_new(heap->arena); }

static StrBuf text_with(Heap *heap, StrBuf text, Str more) {
  note(heap, strbuf_push(&text, more));
  return text;
}

static StrBuf text_with_scalar(Heap *heap, StrBuf text, uint32_t code) {
  note(heap, push_scalar(&text, code));
  return text;
}

Vec json_vec_with(Heap *heap, Vec vec, void *item) {
  note(heap, vec_push(&vec, item));
  return vec;
}

static FrameList *frame_push(Heap *heap, FrameKind kind, Vec items, Vec fields, Str key, FrameList *rest) {
  FrameList *frame = heap_alloc(heap, sizeof(FrameList));
  if (frame == NULL) return rest;
  frame->kind = kind;
  frame->items = items;
  frame->fields = fields;
  frame->key = key;
  frame->rest = rest;
  return frame;
}

static Target value_text(void) {
  Target target;
  target.kind = TARGET_VALUE_TEXT;
  target.fields = vec_new(NULL);
  return target;
}

static Target key_text(Vec fields) {
  Target target;
  target.kind = TARGET_KEY_TEXT;
  target.fields = fields;
  return target;
}

/* ---- the Bend definitions, in source order ---- */

/* byte_width.pick, byte_width */
static uint32_t byte_width_pick(bool non_ascii, bool two, bool three) {
  if (!non_ascii) return 1;
  return two ? 2 : (three ? 3 : 4);
}

uint32_t json_byte_width(uint32_t code) { return byte_width_pick(code >= 128, code < 2048, code < 65536); }

/* fail */
static State fail(Heap *heap, uint32_t column, Str message) {
  StrBuf buf = text_new(heap);
  bool ok = strbuf_push(&buf, str_lit("JSON column ")) && strbuf_push_u64(&buf, column) && strbuf_push(&buf, str_lit(": ")) &&
            strbuf_push(&buf, message);
  note(heap, ok);
  return invalid(strbuf_str(&buf));
}

/* whitespace.go, whitespace */
static Cursor whitespace_go(Str s, uint32_t column) {
  uint32_t ch = head(s);
  if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') return whitespace_go(tail(s), column + 1);
  return at(s, column);
}

static Cursor whitespace(Cursor c) { return whitespace_go(c.rest, c.column); }

/* key_exists */
static bool key_exists_from(const Vec *fields, Str key, size_t index) {
  VecGet got = vec_get(fields, index);
  if (got.status != VEC_GET_OK) return false;
  return str_eq(((const JsonField *)got.value)->key, key) || key_exists_from(fields, key, index + 1);
}

bool json_key_exists(const Vec *fields, Str key) { return key_exists_from(fields, key, 0); }

/* reversed_items, reversed_fields: the Vec already holds the items in source order. */
static Json *reversed_items(Heap *heap, Vec xs) { return json_items(heap, xs); }

static Json *reversed_fields(Heap *heap, Vec xs) { return json_object(heap, xs); }

/* finish_text */
static State finish_text(Heap *heap, Target target, StrBuf text, Cursor cursor, FrameList *frames, uint32_t depth) {
  switch (target.kind) {
  case TARGET_VALUE_TEXT: {
    Mode mode = mode_of(MODE_READY);
    mode.value = json_text(heap, strbuf_str(&text));
    return continue_with(mode, cursor, frames, depth);
  }
  case TARGET_KEY_TEXT: {
    Mode mode = mode_of(MODE_COLON);
    mode.fields = target.fields;
    mode.key = strbuf_str(&text);
    return continue_with(mode, cursor, frames, depth);
  }
  }
  return invalid(str_lit("unreachable"));
}

/* digit */
static bool digit(uint32_t code) { return code >= 48 && code <= 57; }

/* value_digit */
static State value_digit(Heap *heap, bool is_digit, uint32_t ch, Str rest, uint32_t column, FrameList *frames, uint32_t depth) {
  if (is_digit) {
    Mode mode = mode_of(MODE_NUMERIC);
    mode.phase = PHASE_INTEGER;
    mode.text = text_with_scalar(heap, text_new(heap), ch);
    return continue_with(mode, at(rest, column + 1), frames, depth);
  }
  return fail(heap, column, str_lit("unexpected character"));
}

static State string_start(Heap *heap, Target target, Cursor next, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_STRING_CHARS);
  mode.target = target;
  mode.text = text_new(heap);
  return continue_with(mode, next, frames, depth);
}

static State literal_start(Heap *heap, Str expected, Json *value, Cursor next, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_LITERAL);
  mode.expected = expected;
  mode.value = value;
  (void)heap;
  return continue_with(mode, next, frames, depth);
}

static State numeric_start(Heap *heap, NumberPhase phase, Str text, Cursor next, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_NUMERIC);
  mode.phase = phase;
  mode.text = text_with(heap, text_new(heap), text);
  return continue_with(mode, next, frames, depth);
}

/* value_start */
static State value_start(Heap *heap, Cursor cursor, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  Str rest = tail(cursor.rest);
  uint32_t column = cursor.column;
  if (cursor.rest.len == 0) return fail(heap, column, str_lit("expected value"));
  if (ch == '"') return string_start(heap, value_text(), at(rest, column + 1), frames, depth);
  if (ch == '[') return continue_with(mode_of(MODE_ARRAY_START), at(rest, column + 1), frames, depth + 1);
  if (ch == '{') {
    Mode mode = mode_of(MODE_OBJECT_START);
    mode.fields = vec_new(heap->arena);
    mode.empty_ok = true;
    return continue_with(mode, at(rest, column + 1), frames, depth + 1);
  }
  if (ch == 't') return literal_start(heap, str_lit("rue"), json_boolean(heap, true), at(rest, column + 1), frames, depth);
  if (ch == 'f') return literal_start(heap, str_lit("alse"), json_boolean(heap, false), at(rest, column + 1), frames, depth);
  if (ch == 'n') return literal_start(heap, str_lit("ull"), json_null(heap), at(rest, column + 1), frames, depth);
  if (ch == '-') return numeric_start(heap, PHASE_MINUS, str_lit("-"), at(rest, column + 1), frames, depth);
  if (ch == '0') return numeric_start(heap, PHASE_ZERO_INT, str_lit("0"), at(rest, column + 1), frames, depth);
  return value_digit(heap, digit(ch), ch, rest, column, frames, depth);
}

/* value_limit */
static State value_limit(Heap *heap, bool exceeded, Cursor cursor, FrameList *frames, uint32_t depth) {
  if (exceeded) return fail(heap, cursor.column, str_lit("nesting exceeds 512 levels"));
  return value_start(heap, cursor, frames, depth);
}

static State ready_with(Json *value, Cursor cursor, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_READY);
  mode.value = value;
  return continue_with(mode, cursor, frames, depth);
}

/* array_start */
static State array_start(Heap *heap, Cursor cursor, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("unterminated array"));
  if (ch == ']') return ready_with(json_items(heap, vec_new(heap->arena)), at(tail(cursor.rest), cursor.column + 1), frames, depth - 1);
  return continue_with(mode_of(MODE_VALUE), cursor, frame_push(heap, FRAME_ARRAY, vec_new(heap->arena), vec_new(NULL), str_empty(), frames), depth);
}

/* array_after */
static State array_after(Heap *heap, Cursor cursor, Vec items, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("unterminated array"));
  if (ch == ']') return ready_with(reversed_items(heap, items), at(tail(cursor.rest), cursor.column + 1), frames, depth - 1);
  if (ch == ',')
    return continue_with(mode_of(MODE_VALUE), at(tail(cursor.rest), cursor.column + 1),
                         frame_push(heap, FRAME_ARRAY, items, vec_new(NULL), str_empty(), frames), depth);
  return fail(heap, cursor.column, str_lit("expected ',' or ']'"));
}

/* object_empty */
static State object_empty(Heap *heap, bool allowed, Vec fields, Str rest, uint32_t column, FrameList *frames, uint32_t depth) {
  if (allowed) return ready_with(reversed_fields(heap, fields), at(rest, column + 1), frames, depth - 1);
  return fail(heap, column, str_lit("expected '\"'"));
}

/* object_start */
static State object_start(Heap *heap, Cursor cursor, Vec fields, bool empty_ok, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("unterminated object"));
  if (ch == '}') return object_empty(heap, empty_ok, fields, tail(cursor.rest), cursor.column, frames, depth);
  if (ch == '"') return string_start(heap, key_text(fields), at(tail(cursor.rest), cursor.column + 1), frames, depth);
  return fail(heap, cursor.column, str_lit("expected '\"'"));
}

/* object_after */
static State object_after(Heap *heap, Cursor cursor, Vec fields, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("unterminated object"));
  if (ch == '}') return ready_with(reversed_fields(heap, fields), at(tail(cursor.rest), cursor.column + 1), frames, depth - 1);
  if (ch == ',') {
    Mode mode = mode_of(MODE_OBJECT_START);
    mode.fields = fields;
    mode.empty_ok = false;
    return continue_with(mode, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  }
  return fail(heap, cursor.column, str_lit("expected ',' or '}'"));
}

/* colon */
static State colon(Heap *heap, Cursor cursor, Vec fields, Str key, FrameList *frames, uint32_t depth) {
  if (head(cursor.rest) == ':')
    return continue_with(mode_of(MODE_VALUE), at(tail(cursor.rest), cursor.column + 1),
                         frame_push(heap, FRAME_OBJECT, vec_new(NULL), fields, key, frames), depth);
  return fail(heap, cursor.column, str_lit("expected ':'"));
}

/* unique_colon */
static State unique_colon(Heap *heap, bool duplicate, Cursor cursor, Vec fields, Str key, FrameList *frames, uint32_t depth) {
  if (duplicate) return fail(heap, cursor.column, cat(heap, str_lit("duplicate object key "), key));
  return colon(heap, whitespace(cursor), fields, key, frames, depth);
}

/* ready_end_trimmed */
static State ready_end_trimmed(Heap *heap, Cursor cursor, Json *value, uint32_t initial_column) {
  if (cursor.rest.len == 0) return complete(value);
  return fail(heap, initial_column, str_lit("trailing content"));
}

/* ready_end */
static State ready_end(Heap *heap, Cursor cursor, Json *value) {
  return ready_end_trimmed(heap, whitespace(cursor), value, cursor.column);
}

/* ready */
static State ready(Heap *heap, FrameList *frames, Cursor cursor, Json *value, uint32_t depth) {
  if (frames == NULL) return ready_end(heap, cursor, value);
  switch (frames->kind) {
  case FRAME_ARRAY: {
    Mode mode = mode_of(MODE_ARRAY_AFTER);
    mode.items = json_vec_with(heap, frames->items, value);
    return continue_with(mode, cursor, frames->rest, depth);
  }
  case FRAME_OBJECT: {
    Mode mode = mode_of(MODE_OBJECT_AFTER);
    mode.fields = json_vec_with(heap, frames->fields, json_field_new(heap, frames->key, value));
    return continue_with(mode, cursor, frames->rest, depth);
  }
  }
  return invalid(str_lit("unreachable"));
}

/* "expected '" ++ SCon{ch, "'"} for an ASCII ch */
static Str expected_char(Heap *heap, uint32_t ch) {
  StrBuf buf = text_new(heap);
  note(heap, strbuf_push(&buf, str_lit("expected '")) && push_scalar(&buf, ch) && strbuf_push_byte(&buf, '\''));
  return strbuf_str(&buf);
}

/* literal_char */
static State literal_char(Heap *heap, bool same, Str expected, Json *value, Cursor cursor, FrameList *frames, uint32_t depth, uint32_t ch,
                          uint32_t column) {
  if (same) return literal_start(heap, expected, value, cursor, frames, depth);
  return fail(heap, column, expected_char(heap, ch));
}

/* literal */
static State literal(Heap *heap, Str expected, Cursor cursor, Json *value, FrameList *frames, uint32_t depth) {
  if (expected.len == 0) return ready_with(value, cursor, frames, depth);
  uint32_t wanted = head(expected);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, expected_char(heap, wanted));
  return literal_char(heap, head(cursor.rest) == wanted, tail(expected), value, at(tail(cursor.rest), cursor.column + 1), frames, depth, wanted,
                      cursor.column);
}

/* raw_char: the valid character enters the text as its source bytes and advances the column by its
 * byte width. */
static State raw_char(Heap *heap, bool valid, Utf8Char ch, Target target, StrBuf text, Str source, uint32_t column, FrameList *frames,
                      uint32_t depth) {
  if (!valid) return fail(heap, column, str_lit("unescaped control character"));
  StrSlice bytes = str_slice(source, 0, ch.width);
  Mode mode = mode_of(MODE_STRING_CHARS);
  mode.target = target;
  mode.text = text_with(heap, text, bytes.status == STR_INDEX_OK ? bytes.value : str_empty());
  return continue_with(mode, at(drop(source, ch.width), column + json_byte_width(ch.code)), frames, depth);
}

/* raw_valid */
static bool raw_valid(uint32_t code) { return code >= 32 && code <= 1114111 && (code < 55296 || code > 57343); }

/* string_chars */
static State string_chars(Heap *heap, Cursor cursor, Target target, StrBuf text, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("unterminated string"));
  if (ch == '"') return finish_text(heap, target, text, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  if (ch == '\\') {
    Mode mode = mode_of(MODE_ESCAPE);
    mode.target = target;
    mode.text = text;
    return continue_with(mode, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  }
  Utf8Char decoded_ch = decode_char(cursor.rest);
  if (decoded_ch.width == 0) return fail(heap, cursor.column, str_lit("invalid UTF-8 in string"));
  return raw_char(heap, raw_valid(decoded_ch.code), decoded_ch, target, text, cursor.rest, cursor.column, frames, depth);
}

/* decoded: Maybe<Char> as a flag and the character. */
typedef struct {
  bool some;
  uint32_t ch;
} MaybeChar;

static MaybeChar some_char(uint32_t ch) {
  MaybeChar value;
  value.some = true;
  value.ch = ch;
  return value;
}

static MaybeChar no_char(void) {
  MaybeChar value;
  value.some = false;
  value.ch = 0;
  return value;
}

static MaybeChar decoded(uint32_t ch) {
  if (ch == '"') return some_char('"');
  if (ch == '\\') return some_char('\\');
  if (ch == '/') return some_char('/');
  if (ch == 'b') return some_char(8);
  if (ch == 'f') return some_char(12);
  if (ch == 'n') return some_char(10);
  if (ch == 'r') return some_char(13);
  if (ch == 't') return some_char(9);
  return no_char();
}

/* escape_decoded */
static State escape_decoded(Heap *heap, MaybeChar value, Target target, StrBuf text, Cursor cursor, FrameList *frames, uint32_t depth) {
  if (!value.some) return invalid(str_lit("invalid JSON escape"));
  Mode mode = mode_of(MODE_STRING_CHARS);
  mode.target = target;
  mode.text = text_with_scalar(heap, text, value.ch);
  return continue_with(mode, cursor, frames, depth);
}

static State unicode_mode(Target target, StrBuf text, bool has_high, uint32_t high, Cursor cursor, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_UNICODE);
  mode.target = target;
  mode.text = text;
  mode.remaining = 4;
  mode.scalar = 0;
  mode.has_high = has_high;
  mode.high = high;
  return continue_with(mode, cursor, frames, depth);
}

/* escape */
static State escape(Heap *heap, Cursor cursor, Target target, StrBuf text, FrameList *frames, uint32_t depth) {
  uint32_t ch = head(cursor.rest);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("incomplete escape"));
  if (ch == 'u') return unicode_mode(target, text, false, 0, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  return escape_decoded(heap, decoded(ch), target, text, at(tail(cursor.rest), cursor.column + 1), frames, depth);
}

/* hex.pick, hex: Maybe<U32> as a flag and the value (MaybeU32 from kernel.h). */

static MaybeU32 some_u32(uint32_t value) {
  MaybeU32 result;
  result.some = true;
  result.value = value;
  return result;
}

static MaybeU32 no_u32(void) {
  MaybeU32 result;
  result.some = false;
  result.value = 0;
  return result;
}

static MaybeU32 hex_pick(bool decimal, bool lower, bool upper, uint32_t code) {
  if (decimal) return some_u32(code - 48);
  if (lower) return some_u32(code - 87);
  if (upper) return some_u32(code - 55);
  return no_u32();
}

static MaybeU32 hex(uint32_t code) {
  return hex_pick(code >= 48 && code <= 57, code >= 97 && code <= 102, code >= 65 && code <= 70, code);
}

/* unicode_add */
static State unicode_add(Heap *heap, uint32_t scalar, Target target, StrBuf text, Cursor cursor, FrameList *frames, uint32_t depth) {
  Mode mode = mode_of(MODE_STRING_CHARS);
  mode.target = target;
  mode.text = text_with_scalar(heap, text, scalar);
  return continue_with(mode, cursor, frames, depth);
}

/* unicode_low */
static State unicode_low(Heap *heap, bool valid, uint32_t scalar, uint32_t high, Target target, StrBuf text, Cursor cursor, FrameList *frames,
                         uint32_t depth) {
  if (valid) return unicode_add(heap, 65536 + (high - 55296) * 1024 + scalar - 56320, target, text, cursor, frames, depth);
  return fail(heap, cursor.column, str_lit("invalid low surrogate"));
}

/* unicode_first */
static State unicode_first(Heap *heap, bool high, bool low, uint32_t scalar, Target target, StrBuf text, Cursor cursor, FrameList *frames,
                           uint32_t depth) {
  if (high) {
    Mode mode = mode_of(MODE_LOW_SLASH);
    mode.target = target;
    mode.text = text;
    mode.high = scalar;
    return continue_with(mode, cursor, frames, depth);
  }
  if (low) return fail(heap, cursor.column, str_lit("unpaired low surrogate"));
  return unicode_add(heap, scalar, target, text, cursor, frames, depth);
}

/* unicode_done */
static State unicode_done(Heap *heap, bool has_high, uint32_t high, uint32_t scalar, Target target, StrBuf text, Cursor cursor,
                          FrameList *frames, uint32_t depth) {
  if (!has_high)
    return unicode_first(heap, scalar >= 55296 && scalar <= 56319, scalar >= 56320 && scalar <= 57343, scalar, target, text, cursor, frames,
                         depth);
  return unicode_low(heap, scalar >= 56320 && scalar <= 57343, scalar, high, target, text, cursor, frames, depth);
}

/* unicode_digit */
static State unicode_digit(Heap *heap, MaybeU32 value, Target target, StrBuf text, uint32_t remaining, uint32_t scalar, bool has_high,
                           uint32_t high, Str rest, uint32_t column, FrameList *frames, uint32_t depth) {
  if (!value.some) return fail(heap, column, str_lit("invalid Unicode escape"));
  Mode mode = mode_of(MODE_UNICODE);
  mode.target = target;
  mode.text = text;
  mode.remaining = remaining;
  mode.scalar = scalar * 16 + value.value;
  mode.has_high = has_high;
  mode.high = high;
  return continue_with(mode, at(rest, column + 1), frames, depth);
}

/* unicode */
static State unicode(Heap *heap, uint32_t remaining, Cursor cursor, Target target, StrBuf text, uint32_t scalar, bool has_high, uint32_t high,
                     FrameList *frames, uint32_t depth) {
  if (remaining == 0) return unicode_done(heap, has_high, high, scalar, target, text, cursor, frames, depth);
  if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("incomplete Unicode escape"));
  return unicode_digit(heap, hex(head(cursor.rest)), target, text, remaining - 1, scalar, has_high, high, tail(cursor.rest), cursor.column,
                       frames, depth);
}

/* low_slash */
static State low_slash(Heap *heap, Cursor cursor, Target target, StrBuf text, uint32_t high, FrameList *frames, uint32_t depth) {
  if (head(cursor.rest) == '\\') {
    Mode mode = mode_of(MODE_LOW_U);
    mode.target = target;
    mode.text = text;
    mode.high = high;
    return continue_with(mode, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  }
  return fail(heap, cursor.column, str_lit("expected '\\'"));
}

/* low_u */
static State low_u(Heap *heap, Cursor cursor, Target target, StrBuf text, uint32_t high, FrameList *frames, uint32_t depth) {
  if (head(cursor.rest) == 'u') return unicode_mode(target, text, true, high, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  return fail(heap, cursor.column, str_lit("expected 'u'"));
}

/* NumericAction constructors */
static NumericAction take_digit(NumberPhase phase) {
  NumericAction action;
  action.kind = ACTION_TAKE_DIGIT;
  action.phase = phase;
  action.message = str_empty();
  return action;
}

static NumericAction end_number(void) {
  NumericAction action;
  action.kind = ACTION_END_NUMBER;
  action.phase = PHASE_MINUS;
  action.message = str_empty();
  return action;
}

static NumericAction bad_number(Str message) {
  NumericAction action;
  action.kind = ACTION_BAD_NUMBER;
  action.phase = PHASE_MINUS;
  action.message = message;
  return action;
}

/* number_digit */
static NumericAction number_digit(bool yes, NumberPhase phase, NumericAction other) { return yes ? take_digit(phase) : other; }

/* integer_action */
static NumericAction integer_action(uint32_t ch, NumberPhase phase, bool digits) {
  if (ch == '.') return take_digit(PHASE_DOT);
  if (ch == 'e' || ch == 'E') return take_digit(PHASE_EXPONENT);
  return number_digit(digits && digit(ch), phase, end_number());
}

/* numeric_action */
static NumericAction numeric_action(NumberPhase phase, uint32_t ch) {
  switch (phase) {
  case PHASE_MINUS:
    if (ch == '0') return take_digit(PHASE_ZERO_INT);
    return number_digit(digit(ch), PHASE_INTEGER, bad_number(str_lit("expected number")));
  case PHASE_ZERO_INT:
    return integer_action(ch, PHASE_ZERO_INT, false);
  case PHASE_INTEGER:
    return integer_action(ch, PHASE_INTEGER, true);
  case PHASE_DOT:
    return number_digit(digit(ch), PHASE_FRACTION, bad_number(str_lit("expected digit")));
  case PHASE_FRACTION:
    if (ch == 'e' || ch == 'E') return take_digit(PHASE_EXPONENT);
    return number_digit(digit(ch), PHASE_FRACTION, end_number());
  case PHASE_EXPONENT:
    if (ch == '+' || ch == '-') return take_digit(PHASE_EXP_SIGN);
    return number_digit(digit(ch), PHASE_EXP_DIGITS, bad_number(str_lit("expected digit")));
  case PHASE_EXP_SIGN:
    return number_digit(digit(ch), PHASE_EXP_DIGITS, bad_number(str_lit("expected digit")));
  case PHASE_EXP_DIGITS:
    return number_digit(digit(ch), PHASE_EXP_DIGITS, end_number());
  }
  return end_number();
}

/* numeric_eof */
static NumericAction numeric_eof(NumberPhase phase) {
  switch (phase) {
  case PHASE_MINUS:
    return bad_number(str_lit("expected number"));
  case PHASE_DOT:
    return bad_number(str_lit("expected digit"));
  case PHASE_EXPONENT:
    return bad_number(str_lit("expected digit"));
  case PHASE_EXP_SIGN:
    return bad_number(str_lit("expected digit"));
  case PHASE_ZERO_INT:
    return end_number();
  case PHASE_INTEGER:
    return end_number();
  case PHASE_FRACTION:
    return end_number();
  case PHASE_EXP_DIGITS:
    return end_number();
  }
  return end_number();
}

/* numeric_apply */
static State numeric_apply(Heap *heap, NumericAction action, StrBuf text, Cursor cursor, FrameList *frames, uint32_t depth) {
  switch (action.kind) {
  case ACTION_BAD_NUMBER:
    return fail(heap, cursor.column, action.message);
  case ACTION_END_NUMBER:
    return ready_with(json_number(heap, strbuf_str(&text)), cursor, frames, depth);
  case ACTION_TAKE_DIGIT: {
    if (cursor.rest.len == 0) return fail(heap, cursor.column, str_lit("expected digit"));
    Mode mode = mode_of(MODE_NUMERIC);
    mode.phase = action.phase;
    mode.text = text_with_scalar(heap, text, head(cursor.rest));
    return continue_with(mode, at(tail(cursor.rest), cursor.column + 1), frames, depth);
  }
  }
  return invalid(str_lit("unreachable"));
}

/* numeric */
static State numeric(Heap *heap, Cursor cursor, NumberPhase phase, StrBuf text, FrameList *frames, uint32_t depth) {
  if (cursor.rest.len == 0) return numeric_apply(heap, numeric_eof(phase), text, at(str_empty(), cursor.column), frames, depth);
  return numeric_apply(heap, numeric_action(phase, head(cursor.rest)), text, cursor, frames, depth);
}

/* step */
static State step(Heap *heap, Mode mode, Cursor cursor, FrameList *frames, uint32_t depth) {
  switch (mode.kind) {
  case MODE_VALUE:
    return value_limit(heap, depth > 512, whitespace(cursor), frames, depth);
  case MODE_ARRAY_START:
    return array_start(heap, whitespace(cursor), frames, depth);
  case MODE_ARRAY_AFTER:
    return array_after(heap, whitespace(cursor), mode.items, frames, depth);
  case MODE_OBJECT_START:
    return object_start(heap, whitespace(cursor), mode.fields, mode.empty_ok, frames, depth);
  case MODE_OBJECT_AFTER:
    return object_after(heap, whitespace(cursor), mode.fields, frames, depth);
  case MODE_COLON:
    return unique_colon(heap, json_key_exists(&mode.fields, mode.key), cursor, mode.fields, mode.key, frames, depth);
  case MODE_STRING_CHARS:
    return string_chars(heap, cursor, mode.target, mode.text, frames, depth);
  case MODE_ESCAPE:
    return escape(heap, cursor, mode.target, mode.text, frames, depth);
  case MODE_UNICODE:
    return unicode(heap, mode.remaining, cursor, mode.target, mode.text, mode.scalar, mode.has_high, mode.high, frames, depth);
  case MODE_LOW_SLASH:
    return low_slash(heap, cursor, mode.target, mode.text, mode.high, frames, depth);
  case MODE_LOW_U:
    return low_u(heap, cursor, mode.target, mode.text, mode.high, frames, depth);
  case MODE_NUMERIC:
    return numeric(heap, cursor, mode.phase, mode.text, frames, depth);
  case MODE_LITERAL:
    return literal(heap, mode.expected, cursor, mode.value, frames, depth);
  case MODE_READY:
    return ready(heap, frames, cursor, mode.value, depth);
  }
  return invalid(str_lit("unreachable"));
}

static JsonResult json_done(Json *value) {
  JsonResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static JsonResult json_fail(Str message) {
  JsonResult result;
  result.kind = JSON_FAIL;
  result.value = &json_null_placeholder;
  result.message = message;
  return result;
}

/* run: each nonterminal step consumes input or reduces a finite continuation (the Bend @unsafe tail
 * loop), so the iteration terminates; after an allocation failure the loop stops at once. */
static JsonResult run(Heap *heap, State state) {
  while (state.kind == STATE_CONTINUE && !heap->oom) state = step(heap, state.mode, state.cursor, state.frames, state.depth);
  switch (state.kind) {
  case STATE_INVALID:
    return json_fail(state.message);
  case STATE_COMPLETE:
    return json_done(state.value);
  case STATE_CONTINUE:
    return json_fail(str_lit("host out of memory"));
  }
  return json_fail(str_lit("unreachable"));
}

/* parse */
JsonResult json_parse(Heap *heap, Str source) { return run(heap, continue_with(mode_of(MODE_VALUE), at(source, 1), NULL, 0)); }

/* hex_char.pick, hex_char */
static char hex_char_pick(bool decimal, uint32_t n) { return decimal ? (char)(48 + n) : (char)(87 + n); }

static char hex_char(uint32_t n) { return hex_char_pick(n < 10, n); }

/* quoted_scalar: a control character as \u00XX, every other one as itself. */
static bool quoted_scalar(StrBuf *buf, bool control, uint32_t code) {
  if (control)
    return strbuf_push(buf, str_lit("\\u00")) && strbuf_push_byte(buf, hex_char(code >> 4)) && strbuf_push_byte(buf, hex_char(code & 15));
  return strbuf_push_byte(buf, (char)code);
}

/* quoted_char: a byte of the UTF-8 text (bytes of multi-byte characters pass through unchanged). */
static bool quoted_char(StrBuf *buf, uint32_t code) {
  if (code == '"') return strbuf_push(buf, str_lit("\\\""));
  if (code == '\\') return strbuf_push(buf, str_lit("\\\\"));
  return quoted_scalar(buf, code < 32, code);
}

/* quoted_body */
static bool quoted_body(StrBuf *buf, Str text, size_t index) {
  StrAt byte = str_at(text, index);
  if (byte.status != STR_INDEX_OK) return strbuf_push_byte(buf, '"');
  return quoted_char(buf, byte.value) && quoted_body(buf, text, index + 1);
}

static bool quoted_into(StrBuf *buf, Str text) { return strbuf_push_byte(buf, '"') && quoted_body(buf, text, 0); }

/* quoted */
Str json_quoted(Heap *heap, Str text) {
  StrBuf buf = text_new(heap);
  note(heap, quoted_into(&buf, text));
  return strbuf_str(&buf);
}

/* render: RenderValue, RenderItems, RenderFields and RenderText as recursive writers into one buffer. */
static bool render_value(StrBuf *buf, const Json *value);

static bool render_items(StrBuf *buf, const Vec *items, size_t index, Str separator) {
  VecGet got = vec_get(items, index);
  if (got.status != VEC_GET_OK) return strbuf_push_byte(buf, ']');
  return strbuf_push(buf, separator) && render_value(buf, got.value) && render_items(buf, items, index + 1, str_lit(","));
}

static bool render_fields(StrBuf *buf, const Vec *fields, size_t index, Str separator) {
  VecGet got = vec_get(fields, index);
  if (got.status != VEC_GET_OK) return strbuf_push_byte(buf, '}');
  const JsonField *field = got.value;
  return strbuf_push(buf, separator) && quoted_into(buf, field->key) && strbuf_push_byte(buf, ':') && render_value(buf, field->value) &&
         render_fields(buf, fields, index + 1, str_lit(","));
}

static bool render_value(StrBuf *buf, const Json *value) {
  switch (value->kind) {
  case JSON_NULL:
    return strbuf_push(buf, str_lit("null"));
  case JSON_BOOLEAN:
    return strbuf_push(buf, value->boolean ? str_lit("true") : str_lit("false"));
  case JSON_NUMBER:
    return strbuf_push(buf, value->text);
  case JSON_TEXT:
    return quoted_into(buf, value->text);
  case JSON_ITEMS:
    return strbuf_push_byte(buf, '[') && render_items(buf, &value->values, 0, str_empty());
  case JSON_OBJECT:
    return strbuf_push_byte(buf, '{') && render_fields(buf, &value->fields, 0, str_empty());
  }
  return false;
}

/* to_string */
Str json_to_string(Heap *heap, const Json *value) {
  StrBuf buf = text_new(heap);
  note(heap, render_value(&buf, value));
  return strbuf_str(&buf);
}

/* ---- typed results ---- */
static JsonListResult list_done(Vec value) {
  JsonListResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static JsonListResult list_fail(Str message) {
  JsonListResult result;
  result.kind = JSON_FAIL;
  result.value = vec_new(NULL);
  result.message = message;
  return result;
}

static JsonTextResult text_done(Str value) {
  JsonTextResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static JsonTextResult text_fail(Str message) {
  JsonTextResult result;
  result.kind = JSON_FAIL;
  result.value = str_empty();
  result.message = message;
  return result;
}

static JsonBoolResult bool_done(bool value) {
  JsonBoolResult result;
  result.kind = JSON_DONE;
  result.value = value;
  result.message = str_empty();
  return result;
}

static JsonBoolResult bool_fail(Str message) {
  JsonBoolResult result;
  result.kind = JSON_FAIL;
  result.value = false;
  result.message = message;
  return result;
}

/* object_fields */
JsonListResult json_object_fields(const Json *value) {
  if (value->kind == JSON_OBJECT) return list_done(value->fields);
  return list_fail(str_lit("expected object"));
}

/* string */
JsonTextResult json_string(const Json *value) {
  if (value->kind == JSON_TEXT) return text_done(value->text);
  return text_fail(str_lit("expected string"));
}

/* bool */
JsonBoolResult json_bool(const Json *value) {
  if (value->kind == JSON_BOOLEAN) return bool_done(value->boolean);
  return bool_fail(str_lit("expected boolean"));
}

/* array */
JsonListResult json_array(const Json *value) {
  if (value->kind == JSON_ITEMS) return list_done(value->values);
  return list_fail(str_lit("expected array"));
}

/* field_pick, field */
static JsonResult field_from(Heap *heap, const Vec *fields, Str key, size_t index) {
  VecGet got = vec_get(fields, index);
  if (got.status != VEC_GET_OK) return json_fail(cat(heap, str_lit("missing field "), key));
  const JsonField *field = got.value;
  if (str_eq(field->key, key)) return json_done(field->value);
  return field_from(heap, fields, key, index + 1);
}

JsonResult json_field(Heap *heap, const Vec *fields, Str key) { return field_from(heap, fields, key, 0); }

/* required */
static bool required_from(const Vec *expected, const Vec *fields, size_t index) {
  VecGet got = vec_get(expected, index);
  if (got.status != VEC_GET_OK) return true;
  return json_key_exists(fields, *(const Str *)got.value) && required_from(expected, fields, index + 1);
}

bool json_required(const Vec *expected, const Vec *fields) { return required_from(expected, fields, 0); }

/* join_strings */
static bool join_from(StrBuf *buf, const Vec *xs, size_t index, Str separator, Str between) {
  VecGet got = vec_get(xs, index);
  if (got.status != VEC_GET_OK) return true;
  return strbuf_push(buf, separator) && strbuf_push(buf, *(const Str *)got.value) && join_from(buf, xs, index + 1, between, between);
}

Str json_join_strings(Heap *heap, const Vec *xs, Str separator, Str between) {
  StrBuf buf = text_new(heap);
  note(heap, join_from(&buf, xs, 0, separator, between));
  return strbuf_str(&buf);
}

/* exact_fields */
static JsonListResult exact_fields(Heap *heap, bool valid, Vec fields, const Vec *expected) {
  if (valid) return list_done(fields);
  return list_fail(cat(heap, str_lit("expected exactly fields "), json_join_strings(heap, expected, str_empty(), str_lit(","))));
}

/* exact_object */
JsonListResult json_exact_object(Heap *heap, const Json *value, const Vec *expected) {
  if (value->kind == JSON_OBJECT)
    return exact_fields(heap, value->fields.len == expected->len && json_required(expected, &value->fields), value->fields, expected);
  return list_fail(str_lit("expected object"));
}

/* decimal_digits */
static bool decimal_digits(Str s, size_t index) {
  StrAt byte = str_at(s, index);
  if (byte.status != STR_INDEX_OK) return true;
  return digit(byte.value) && decimal_digits(s, index + 1);
}

/* trim_zero */
static Str trim_zero(Str s) {
  if (s.len == 0) return str_lit("0");
  if (head(s) == '0') return trim_zero(tail(s));
  return s;
}

/* lexical_pick, lexical: byte order equals codepoint order for UTF-8. */
static Cmp lexical_pick(Cmp order, Cmp rest) {
  switch (order) {
  case CMP_LT:
    return CMP_LT;
  case CMP_GT:
    return CMP_GT;
  case CMP_EQ:
    return rest;
  }
  return rest;
}

static Cmp cmp_u32(uint32_t x, uint32_t y) { return x < y ? CMP_LT : (x > y ? CMP_GT : CMP_EQ); }

Cmp json_lexical(Str a, Str b) {
  if (a.len == 0) return b.len == 0 ? CMP_EQ : CMP_LT;
  if (b.len == 0) return CMP_GT;
  return lexical_pick(cmp_u32(head(a), head(b)), json_lexical(tail(a), tail(b)));
}

/* string_length: the digit strings here are ASCII, one byte per character. */
static size_t string_length(Str s) { return s.len; }

/* natural_range */
static JsonTextResult natural_range(Cmp order, Str text) {
  switch (order) {
  case CMP_GT:
    return text_fail(str_lit("natural index exceeds host integer range"));
  case CMP_EQ:
    return text_done(text);
  case CMP_LT:
    return text_done(text);
  }
  return text_done(text);
}

/* natural_length */
static JsonTextResult natural_length(Cmp order, Str text) {
  switch (order) {
  case CMP_LT:
    return text_done(text);
  case CMP_GT:
    return text_fail(str_lit("natural index exceeds host integer range"));
  case CMP_EQ:
    return natural_range(json_lexical(text, str_lit("4611686018427387903")), text);
  }
  return text_done(text);
}

/* natural_valid */
static JsonTextResult natural_valid(bool valid, Str text) {
  if (!valid) return text_fail(str_lit("expected natural integer"));
  Str normalized = trim_zero(text);
  return natural_length(cmp_u32((uint32_t)(string_length(normalized) < 19 ? 0 : (string_length(normalized) == 19 ? 1 : 2)), 1), normalized);
}

/* nat: source table identifiers are canonical decimal strings within the host integer range. */
JsonTextResult json_nat(const Json *value) {
  if (value->kind != JSON_NUMBER) return text_fail(str_lit("expected natural integer"));
  if (value->text.len == 0) return text_fail(str_lit("expected natural integer"));
  return natural_valid(decimal_digits(value->text, 0), value->text);
}

/* string_field */
JsonTextResult json_string_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return text_fail(value.message);
  return json_string(value.value);
}

/* nat_field */
JsonTextResult json_nat_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return text_fail(value.message);
  return json_nat(value.value);
}

/* bool_field */
JsonBoolResult json_bool_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return bool_fail(value.message);
  return json_bool(value.value);
}

/* nats */
Str *json_boxed_str(Heap *heap, Str value) {
  Str *box = heap_alloc(heap, sizeof(Str));
  if (box == NULL) return NULL;
  *box = value;
  return box;
}

static JsonListResult nats_from(Heap *heap, const Vec *values, size_t index, Vec acc) {
  VecGet got = vec_get(values, index);
  if (got.status != VEC_GET_OK) return list_done(acc);
  JsonTextResult item = json_nat(got.value);
  if (item.kind == JSON_FAIL) return list_fail(item.message);
  Str *box = json_boxed_str(heap, item.value);
  if (box == NULL) return list_done(acc);
  return nats_from(heap, values, index + 1, json_vec_with(heap, acc, box));
}

JsonListResult json_nats(Heap *heap, const Vec *values) { return nats_from(heap, values, 0, vec_new(heap->arena)); }

/* nat_list */
JsonListResult json_nat_list(Heap *heap, const Json *value) {
  JsonListResult values = json_array(value);
  if (values.kind == JSON_FAIL) return values;
  return json_nats(heap, &values.value);
}

/* nats_field */
JsonListResult json_nats_field(Heap *heap, const Vec *fields, Str key) {
  JsonResult value = json_field(heap, fields, key);
  if (value.kind == JSON_FAIL) return list_fail(value.message);
  return json_nat_list(heap, value.value);
}

/* decimal_digits(text): every character a decimal digit (true of the empty text). */
bool json_decimal_digits(Str text) { return decimal_digits(text, 0); }

/* The expected key list of exact_object from C string literals, as heap-boxed Str items. */
static Vec expected_keys_from(Heap *heap, Vec acc, const char *const *names, size_t index, size_t count) {
  if (index >= count) return acc;
  Str *box = json_boxed_str(heap, str_lit(names[index]));
  if (box == NULL) return acc;
  return expected_keys_from(heap, json_vec_with(heap, acc, box), names, index + 1, count);
}

Vec json_expected_keys(Heap *heap, const char *const *names, size_t count) {
  return expected_keys_from(heap, vec_new(heap->arena), names, 0, count);
}
