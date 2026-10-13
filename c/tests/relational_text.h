/* Port of bend2/tests/relational_text.bend: the text helpers of the relational test drivers
 * (word renaming, core renaming, line slicing, identifier and whitespace normalisation).
 * A library module (no main). String lists are Vec of Str * (boxed with relational_str). */
#ifndef MECH_RELATIONAL_TEXT_H
#define MECH_RELATIONAL_TEXT_H

#include "relational_common.h"

/* C.Pair<String, String>: one rename row. */
typedef struct {
  Str name;
  Str value;
} TextRename;

/* Base String.split / String.join on byte strings (the separators are ASCII). */
Vec text_split(Heap *heap, Str text, char separator);
Str text_join(Heap *heap, const Vec *parts, Str separator);

bool text_word(uint32_t c);
Str text_lookup(const Vec *rows, Str key);
Str text_rename_token(Str previous, Str token, const Vec *levels, const Vec *mapping);
Str text_rename_words(Heap *heap, Str text, const Vec *levels, const Vec *mapping);
Str text_core_word(Str value);
Vec text_core_words(Heap *heap, const Vec *words);
Str text_core_line(Heap *heap, Str line);
Vec text_core_lines(Heap *heap, const Vec *lines);
Str text_rename_core(Heap *heap, Str text);
Str text_first_lines(Heap *heap, Str text, size_t count);
Vec text_member_lines(Heap *heap, const Vec *lines, Str stop);
Vec text_skip_header(Heap *heap, const Vec *lines, Str prefix, Str stop);
Str text_member_body(Heap *heap, Str text, Str prefix, Str stop);
Vec text_nonempty(Heap *heap, const Vec *parts);
uint32_t text_identifier_char(uint32_t c);
Str text_identifier_chars(Heap *heap, Str text);
Vec text_identifiers(Heap *heap, Str text);
uint32_t text_whitespace(uint32_t c);
Str text_whitespace_chars(Heap *heap, Str text);
Str text_normalise(Heap *heap, Str text);

#endif
