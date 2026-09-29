import Base
import ./relational_common.bend as R
import ../kernel/common.bend as C

def word(+char: Char) -> Bool:
  Bool.or(Char.is_alpha(char), Bool.or(Char.is_digit(char), Char.is_eq(char, '_')))

def lookup(rows: List<&2, C.Pair<String, String>>, +key: String) -> String:
  match rows:
    case Nil{}: key
    case Con{C.Pair{name, value}, rest}:
      selected = Bool.pick(Unit -> String, String.eq(name, key), unit => value, unit => lookup(rest, key))
      selected(Unit{})

def rename_token(previous: String, token: String, levels: List<&2, C.Pair<String, String>>, mapping: List<&2, C.Pair<String, String>>) -> String:
  lookup(Bool.pick(List<&2, C.Pair<String, String>>, String.eq(previous, "succ"), levels, mapping), token)

law rename_go:
  for text: String
  for token: String
  for previous: String
  for pieces: List<&2, String>
  for levels: List<&2, C.Pair<String, String>>
  for mapping: List<&2, C.Pair<String, String>>
  String

@unsafe
def rename_step(is_word: Bool, char: Char, tail: String, +token: String, +previous: String, pieces: List<&2, String>, +levels: List<&2, C.Pair<String, String>>, +mapping: List<&2, C.Pair<String, String>>) -> String:
  match is_word:
    case True{}: rename_go(tail, token ++ SCon{char, SNil{}}, previous, pieces, levels, mapping)
    case False{}:
      +renamed = rename_token(previous, token, levels, mapping)
      rename_go(tail, "", Bool.pick(String, String.eq(token, ""), previous, token), SCon{char, SNil{}} <> renamed <> pieces, levels, mapping)

@unsafe
def rename_go(text, token, previous, pieces, levels, mapping):
  match text:
    case SNil{}: String.concat(List.reverse(&2, String, rename_token(previous, token, levels, mapping) <> pieces))
    case SCon{+char, tail}: rename_step(word(char), char, tail, token, previous, pieces, levels, mapping)

def rename_words(text: String, levels: List<&2, C.Pair<String, String>>, mapping: List<&2, C.Pair<String, String>>) -> String:
  rename_go(text, "", "", [], levels, mapping)

def core_word(+value: String) -> String:
  Bool.pick(String, String.eq(value, "MechCategoryCore"), "MechCategory", value)

def core_words(words: List<&2, String>) -> List<&2, String>:
  match words:
    case Nil{}: []
    case Con{word, rest}: core_word(word) <> core_words(rest)

def core_line(line: String) -> String:
  String.join(core_words(String.split(line, ' ')), " ")

def core_lines(lines: List<&2, String>) -> List<&2, String>:
  match lines:
    case Nil{}: []
    case Con{line, rest}: core_line(line) <> core_lines(rest)

def rename_core(text: String) -> String:
  String.join(core_lines(String.split(text, '\n')), "\n")

def first_lines(text: String, count: Nat) -> String:
  String.join(List.take(&2, String, String.split(text, '\n'), count), "\n")

def member_lines(lines: List<&2, String>, +stop: String) -> List<&2, String>:
  match lines:
    case Nil{}: []
    case Con{+line, rest}:
      selected = Bool.pick(Unit -> List<&2, String>, String.starts_with(line, stop), unit => [], unit => line <> member_lines(rest, stop))
      selected(Unit{})

def skip_header(lines: List<&2, String>, +prefix: String, +stop: String) -> List<&2, String>:
  match lines:
    case Nil{}: []
    case Con{+line, +rest}:
      selected = Bool.pick(Unit -> List<&2, String>, String.starts_with(line, prefix), unit => member_lines(line <> rest, stop), unit => skip_header(rest, prefix, stop))
      selected(Unit{})

def member_body(text: String, prefix: String, stop: String) -> String:
  String.join(skip_header(String.split(text, '\n'), prefix, stop), "\n") ++ "\n"

def nonempty(parts: List<&2, String>) -> List<&2, String>:
  match parts:
    case Nil{}: []
    case Con{+head, rest}:
      +tail = nonempty(rest)
      Bool.pick(List<&2, String>, String.eq(head, ""), tail, head <> tail)

def identifier_char(+char: Char) -> Char:
  Bool.pick(Char, word(char), char, ' ')

def identifier_chars(text: String) -> String:
  match text:
    case SNil{}: SNil{}
    case SCon{char, tail}: SCon{identifier_char(char), identifier_chars(tail)}

def identifiers(text: String) -> List<&2, String>:
  nonempty(String.split(identifier_chars(text), ' '))

def whitespace(+char: Char) -> Char:
  Bool.pick(Char, Bool.or(Char.is_eq(char, '\t'), Char.is_eq(char, '\n')), ' ', char)

def whitespace_chars(text: String) -> String:
  match text:
    case SNil{}: SNil{}
    case SCon{char, tail}: SCon{whitespace(char), whitespace_chars(tail)}

def normalise(text: String) -> String:
  String.join(nonempty(String.split(whitespace_chars(text), ' ')), " ")
