/* Port of bend2/tests/relational_text.bend (110 lines), same definitions in the same order.
 * Strings are byte strings: every separator and word character tested here is ASCII, so a
 * multi-byte character passes through unchanged exactly as the one Bend Char it was. */
#include "relational_text.h"

#include <string.h>

static Str at_str(const Vec *values, size_t index) {
  VecGet got = vec_get(values, index);
  if (got.status == VEC_GET_OK && got.value != NULL) return *(Str *)got.value;
  return str_empty();
}

static Str slice(Str s, size_t start, size_t end) {
  StrSlice got = str_slice(s, start, end);
  if (got.status == STR_INDEX_OK) return got.value;
  return str_empty();
}

static bool push_str(Heap *heap, Vec *out, Str value) {
  Str *boxed = relational_str(heap, value);
  if (boxed == NULL || !vec_push(out, boxed)) {
    heap->oom = true;
    return false;
  }
  return true;
}

/* Map every byte of text through f (a Char -> Char function on ASCII code points). */
static Str map_chars(Heap *heap, Str text, uint32_t (*f)(uint32_t)) {
  char *buffer = heap_alloc(heap, text.len == 0 ? 1 : text.len);
  if (buffer == NULL) {
    heap->oom = true;
    return str_empty();
  }
  for (size_t i = 0; i < text.len; i++) buffer[i] = (char)f((unsigned char)text.data[i]);
  Str out;
  out.data = buffer;
  out.len = text.len;
  return out;
}

Vec text_split(Heap *heap, Str text, char separator) {
  Vec out = vec_new(heap->arena);
  size_t start = 0;
  for (size_t i = 0; i < text.len; i++) {
    if (text.data[i] != separator) continue;
    if (!push_str(heap, &out, slice(text, start, i))) return out;
    start = i + 1;
  }
  push_str(heap, &out, slice(text, start, text.len));
  return out;
}

Str text_join(Heap *heap, const Vec *parts, Str separator) {
  size_t total = 0;
  for (size_t i = 0; i < parts->len; i++) total += at_str(parts, i).len + (i == 0 ? 0 : separator.len);
  char *buffer = heap_alloc(heap, total == 0 ? 1 : total);
  if (buffer == NULL) {
    heap->oom = true;
    return str_empty();
  }
  size_t at = 0;
  for (size_t i = 0; i < parts->len; i++) {
    Str part = at_str(parts, i);
    if (i != 0 && separator.len != 0) memcpy(buffer + at, separator.data, separator.len);
    at += (i == 0 ? 0 : separator.len);
    if (part.len != 0) memcpy(buffer + at, part.data, part.len);
    at += part.len;
  }
  Str out;
  out.data = buffer;
  out.len = total;
  return out;
}

/* word: Char.is_alpha || Char.is_digit || '_' (ASCII letters and digits). */
bool text_word(uint32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

Str text_lookup(const Vec *rows, Str key) {
  for (size_t i = 0; i < rows->len; i++) {
    VecGet got = vec_get(rows, i);
    if (got.status != VEC_GET_OK || got.value == NULL) continue;
    const TextRename *row = got.value;
    if (str_eq(row->name, key)) return row->value;
  }
  return key;
}

Str text_rename_token(Str previous, Str token, const Vec *levels, const Vec *mapping) {
  return text_lookup(str_eq(previous, str_lit("succ")) ? levels : mapping, token);
}

/* rename_go / rename_step: a word run is one token; the token before it names the table. */
static Str text_rename_go(Heap *heap, Str text, const Vec *levels, const Vec *mapping) {
  Vec pieces = vec_new(heap->arena);
  Str previous = str_empty();
  size_t token_start = 0;
  for (size_t i = 0; i < text.len; i++) {
    if (text_word((unsigned char)text.data[i])) continue;
    Str token = slice(text, token_start, i);
    if (!push_str(heap, &pieces, text_rename_token(previous, token, levels, mapping))) return str_empty();
    if (!push_str(heap, &pieces, slice(text, i, i + 1))) return str_empty();
    if (token.len != 0) previous = token;
    token_start = i + 1;
  }
  Str token = slice(text, token_start, text.len);
  if (!push_str(heap, &pieces, text_rename_token(previous, token, levels, mapping))) return str_empty();
  return text_join(heap, &pieces, str_empty());
}

Str text_rename_words(Heap *heap, Str text, const Vec *levels, const Vec *mapping) {
  return text_rename_go(heap, text, levels, mapping);
}

Str text_core_word(Str value) {
  if (str_eq(value, str_lit("MechCategoryCore"))) return str_lit("MechCategory");
  return value;
}

Vec text_core_words(Heap *heap, const Vec *words) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < words->len; i++)
    if (!push_str(heap, &out, text_core_word(at_str(words, i)))) return out;
  return out;
}

Str text_core_line(Heap *heap, Str line) {
  Vec words = text_split(heap, line, ' ');
  Vec renamed = text_core_words(heap, &words);
  return text_join(heap, &renamed, str_lit(" "));
}

Vec text_core_lines(Heap *heap, const Vec *lines) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < lines->len; i++)
    if (!push_str(heap, &out, text_core_line(heap, at_str(lines, i)))) return out;
  return out;
}

Str text_rename_core(Heap *heap, Str text) {
  Vec lines = text_split(heap, text, '\n');
  Vec renamed = text_core_lines(heap, &lines);
  return text_join(heap, &renamed, str_lit("\n"));
}

Str text_first_lines(Heap *heap, Str text, size_t count) {
  Vec lines = text_split(heap, text, '\n');
  Vec taken = vec_new(heap->arena);
  for (size_t i = 0; i < lines.len && i < count; i++)
    if (!push_str(heap, &taken, at_str(&lines, i))) break;
  return text_join(heap, &taken, str_lit("\n"));
}

Vec text_member_lines(Heap *heap, const Vec *lines, Str stop) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < lines->len; i++) {
    Str line = at_str(lines, i);
    if (str_starts_with(line, stop)) return out;
    if (!push_str(heap, &out, line)) return out;
  }
  return out;
}

Vec text_skip_header(Heap *heap, const Vec *lines, Str prefix, Str stop) {
  for (size_t i = 0; i < lines->len; i++) {
    if (!str_starts_with(at_str(lines, i), prefix)) continue;
    Vec rest = vec_new(heap->arena);
    for (size_t j = i; j < lines->len; j++)
      if (!push_str(heap, &rest, at_str(lines, j))) return rest;
    return text_member_lines(heap, &rest, stop);
  }
  return vec_new(heap->arena);
}

Str text_member_body(Heap *heap, Str text, Str prefix, Str stop) {
  Vec lines = text_split(heap, text, '\n');
  Vec body = text_skip_header(heap, &lines, prefix, stop);
  return str_cat(heap->arena, text_join(heap, &body, str_lit("\n")), str_lit("\n"));
}

Vec text_nonempty(Heap *heap, const Vec *parts) {
  Vec out = vec_new(heap->arena);
  for (size_t i = 0; i < parts->len; i++) {
    Str part = at_str(parts, i);
    if (part.len == 0) continue;
    if (!push_str(heap, &out, part)) return out;
  }
  return out;
}

uint32_t text_identifier_char(uint32_t c) { return text_word(c) ? c : ' '; }

Str text_identifier_chars(Heap *heap, Str text) { return map_chars(heap, text, text_identifier_char); }

Vec text_identifiers(Heap *heap, Str text) {
  Vec parts = text_split(heap, text_identifier_chars(heap, text), ' ');
  return text_nonempty(heap, &parts);
}

uint32_t text_whitespace(uint32_t c) { return (c == '\t' || c == '\n') ? ' ' : c; }

Str text_whitespace_chars(Heap *heap, Str text) { return map_chars(heap, text, text_whitespace); }

Str text_normalise(Heap *heap, Str text) {
  Vec parts = text_split(heap, text_whitespace_chars(heap, text), ' ');
  Vec words = text_nonempty(heap, &parts);
  return text_join(heap, &words, str_lit(" "));
}
