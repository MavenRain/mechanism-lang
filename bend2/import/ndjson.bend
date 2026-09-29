import Base

type Field<-A: Data> is Data:
  Field{key: String, value: A}

type T is Data:
  Null{}
  Boolean{value: Bool}
  Number{value: String}
  Text{value: String}
  Items{values: List<&2, T>}
  Object{fields: List<&2, Field<T>>}

type Cursor is Data:
  At{rest: String, column: U32}

type Frame is Data:
  ArrayFrame{items: List<&2, T>}
  ObjectFrame{fields: List<&2, Field<T>>, key: String}

type Target is Data:
  ValueText{}
  KeyText{fields: List<&2, Field<T>>}

type NumberPhase is Data:
  Minus{}
  ZeroInt{}
  Integer{}
  Dot{}
  Fraction{}
  Exponent{}
  ExpSign{}
  ExpDigits{}

type Mode is Data:
  Value{}
  ArrayStart{}
  ArrayAfter{items: List<&2, T>}
  ObjectStart{fields: List<&2, Field<T>>, empty_ok: Bool}
  ObjectAfter{fields: List<&2, Field<T>>}
  Colon{fields: List<&2, Field<T>>, key: String}
  StringChars{target: Target, reversed: String}
  Escape{target: Target, reversed: String}
  Unicode{target: Target, reversed: String, remaining: Nat, scalar: U32, high: Maybe<&2, U32>}
  LowSlash{target: Target, reversed: String, high: U32}
  LowU{target: Target, reversed: String, high: U32}
  Numeric{phase: NumberPhase, reversed: String}
  Literal{expected: String, value: T}
  Ready{value: T}

type State is Data:
  Continue{mode: Mode, cursor: Cursor, frames: List<&2, Frame>, depth: U32}
  Complete{value: T}
  Invalid{message: String}

def str_eq(a: String, b: String) -> Bool:
  match a:
    case SNil{}:
      match b:
        case SNil{}:
          True{}
        case SCon{h, t}:
          False{}
    case SCon{Chr{x}, xs}:
      match b:
        case SNil{}:
          False{}
        case SCon{Chr{y}, ys}:
          Bool.and(U32.is_eq(x, y), str_eq(xs, ys))

def reverse(s: String, acc: String) -> String:
  match s:
    case SNil{}:
      acc
    case SCon{h, t}:
      reverse(t, SCon{h, acc})

def byte_width.pick(non_ascii: Bool, two: Bool, three: Bool) -> U32:
  match non_ascii:
    case False{}:
      1
    case True{}:
      Bool.pick(U32, two, 2, Bool.pick(U32, three, 3, 4))

def byte_width(ch: Char) -> U32:
  match ch:
    case Chr{+code}:
      byte_width.pick(U32.is_ge(code, 128), U32.is_lt(code, 2048), U32.is_lt(code, 65536))

def fail(column: U32, message: String) -> State:
  Invalid{"JSON column " ++ U32.show(column) ++ ": " ++ message}

def whitespace.go(s: String, column: U32) -> Cursor:
  match s:
    case SCon{' ', rest}:
      whitespace.go(rest, (column + 1 : U32))
    case SCon{'\t', rest}:
      whitespace.go(rest, (column + 1 : U32))
    case SCon{'\r', rest}:
      whitespace.go(rest, (column + 1 : U32))
    case SCon{'\n', rest}:
      whitespace.go(rest, (column + 1 : U32))
    case SNil{}:
      At{"", column}
    case SCon{ch, rest}:
      At{SCon{ch, rest}, column}

def whitespace(c: Cursor) -> Cursor:
  match c:
    case At{s, column}:
      whitespace.go(s, column)

def key_exists(fields: List<&2, Field<T>>, +key: String) -> Bool:
  match fields:
    case Nil{}:
      False{}
    case Con{Field{name, value}, rest}:
      Bool.or(str_eq(name, key), key_exists(rest, key))

def reversed_items(xs: List<&2, T>) -> T:
  Items{List.reverse(&2, T, xs)}

def reversed_fields(xs: List<&2, Field<T>>) -> T:
  Object{List.reverse(&2, Field<T>, xs)}

def finish_text(target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match target:
    case ValueText{}:
      Continue{Ready{Text{reverse(reversed, "")}}, cursor, frames, depth}
    case KeyText{fields}:
      Continue{Colon{fields, reverse(reversed, "")}, cursor, frames, depth}

def digit(ch: Char) -> Bool:
  match ch:
    case Chr{+code}:
      Bool.and(U32.is_ge(code, 48), U32.is_le(code, 57))

def value_digit(is_digit: Bool, ch: Char, rest: String, column: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match is_digit:
    case True{}:
      Continue{Numeric{Integer{}, SCon{ch, ""}}, At{rest, (column + 1 : U32)}, frames, depth}
    case False{}:
      fail(column, "unexpected character")

def value_start(cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "expected value")
    case At{SCon{'"', rest}, column}:
      Continue{StringChars{ValueText{}, ""}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{'[', rest}, column}:
      Continue{ArrayStart{}, At{rest, (column + 1 : U32)}, frames, (depth + 1 : U32)}
    case At{SCon{'{', rest}, column}:
      Continue{ObjectStart{Nil{}, True{}}, At{rest, (column + 1 : U32)}, frames, (depth + 1 : U32)}
    case At{SCon{'t', rest}, column}:
      Continue{Literal{"rue", Boolean{True{}}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{'f', rest}, column}:
      Continue{Literal{"alse", Boolean{False{}}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{'n', rest}, column}:
      Continue{Literal{"ull", Null{}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{'-', rest}, column}:
      Continue{Numeric{Minus{}, "-"}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{'0', rest}, column}:
      Continue{Numeric{ZeroInt{}, "0"}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{+ch, rest}, column}:
      value_digit(digit(ch), ch, rest, column, frames, depth)

def value_limit(exceeded: Bool, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match exceeded:
    case True{}:
      match cursor:
        case At{rest, column}:
          fail(column, "nesting exceeds 512 levels")
    case False{}:
      value_start(cursor, frames, depth)

def array_start(cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "unterminated array")
    case At{SCon{']', rest}, column}:
      Continue{Ready{Items{Nil{}}}, At{rest, (column + 1 : U32)}, frames, (depth - 1 : U32)}
    case At{rest, column}:
      Continue{Value{}, At{rest, column}, Con{ArrayFrame{Nil{}}, frames}, depth}

def array_after(cursor: Cursor, items: List<&2, T>, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "unterminated array")
    case At{SCon{']', rest}, column}:
      Continue{Ready{reversed_items(items)}, At{rest, (column + 1 : U32)}, frames, (depth - 1 : U32)}
    case At{SCon{',', rest}, column}:
      Continue{Value{}, At{rest, (column + 1 : U32)}, Con{ArrayFrame{items}, frames}, depth}
    case At{rest, column}:
      fail(column, "expected ',' or ']'")

def object_empty(allowed: Bool, fields: List<&2, Field<T>>, rest: String, column: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match allowed:
    case True{}:
      Continue{Ready{reversed_fields(fields)}, At{rest, (column + 1 : U32)}, frames, (depth - 1 : U32)}
    case False{}:
      fail(column, "expected '\"'")

def object_start(cursor: Cursor, fields: List<&2, Field<T>>, empty_ok: Bool, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "unterminated object")
    case At{SCon{'}', rest}, column}:
      object_empty(empty_ok, fields, rest, column, frames, depth)
    case At{SCon{'"', rest}, column}:
      Continue{StringChars{KeyText{fields}, ""}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{rest, column}:
      fail(column, "expected '\"'")

def object_after(cursor: Cursor, fields: List<&2, Field<T>>, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "unterminated object")
    case At{SCon{'}', rest}, column}:
      Continue{Ready{reversed_fields(fields)}, At{rest, (column + 1 : U32)}, frames, (depth - 1 : U32)}
    case At{SCon{',', rest}, column}:
      Continue{ObjectStart{fields, False{}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{rest, column}:
      fail(column, "expected ',' or '}'")

def colon(cursor: Cursor, fields: List<&2, Field<T>>, key: String, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SCon{':', rest}, column}:
      Continue{Value{}, At{rest, (column + 1 : U32)}, Con{ObjectFrame{fields, key}, frames}, depth}
    case At{rest, column}:
      fail(column, "expected ':'")

def unique_colon(duplicate: Bool, cursor: Cursor, fields: List<&2, Field<T>>, key: String, frames: List<&2, Frame>, depth: U32) -> State:
  match duplicate:
    case True{}:
      match cursor:
        case At{rest, column}:
          fail(column, "duplicate object key " ++ key)
    case False{}:
      colon(whitespace(cursor), fields, key, frames, depth)

def ready_end_trimmed(cursor: Cursor, value: T, initial_column: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      Complete{value}
    case At{rest, column}:
      fail(initial_column, "trailing content")

def ready_end(cursor: Cursor, value: T) -> State:
  match cursor:
    case At{rest, +column}:
      ready_end_trimmed(whitespace(At{rest, column}), value, column)

def ready(frames: List<&2, Frame>, cursor: Cursor, value: T, depth: U32) -> State:
  match frames:
    case Nil{}:
      ready_end(cursor, value)
    case Con{ArrayFrame{items}, rest}:
      Continue{ArrayAfter{Con{value, items}}, cursor, rest, depth}
    case Con{ObjectFrame{fields, key}, rest}:
      Continue{ObjectAfter{Con{Field{key, value}, fields}}, cursor, rest, depth}

def literal_char(same: Bool, expected: String, value: T, cursor: Cursor, frames: List<&2, Frame>, depth: U32, ch: Char, column: U32) -> State:
  match same:
    case True{}:
      Continue{Literal{expected, value}, cursor, frames, depth}
    case False{}:
      fail(column, "expected '" ++ SCon{ch, "'"})

def literal(expected: String, cursor: Cursor, value: T, frames: List<&2, Frame>, depth: U32) -> State:
  match expected:
    case SNil{}:
      Continue{Ready{value}, cursor, frames, depth}
    case SCon{+wanted, tail}:
      match cursor:
        case At{SNil{}, column}:
          fail(column, "expected '" ++ SCon{wanted, "'"})
        case At{SCon{ch, rest}, +column}:
          literal_char(str_eq(SCon{wanted, ""}, SCon{ch, ""}), tail, value,
            At{rest, (column + 1 : U32)}, frames, depth, wanted, column)

def raw_char(valid: Bool, ch: Char, target: Target, reversed: String, rest: String, column: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match valid:
    case False{}:
      fail(column, "unescaped control character")
    case True{}:
      +c = ch
      Continue{StringChars{target, SCon{c, reversed}}, At{rest, (column + byte_width(c) : U32)}, frames, depth}

def raw_valid(ch: Char) -> Bool:
  match ch:
    case Chr{+code}:
      Bool.and(Bool.and(U32.is_ge(code, 32), U32.is_le(code, 1114111)),
        Bool.or(U32.is_lt(code, 55296), U32.is_gt(code, 57343)))

def string_chars(cursor: Cursor, target: Target, reversed: String, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "unterminated string")
    case At{SCon{'"', rest}, column}:
      finish_text(target, reversed, At{rest, (column + 1 : U32)}, frames, depth)
    case At{SCon{'\\', rest}, column}:
      Continue{Escape{target, reversed}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{+ch, rest}, column}:
      raw_char(raw_valid(ch), ch, target, reversed, rest, column, frames, depth)

def decoded(ch: Char) -> Maybe<&2, Char>:
  match ch:
    case '"':
      Some{'"'}
    case '\\':
      Some{'\\'}
    case '/':
      Some{'/'}
    case 'b':
      Some{Chr{8}}
    case 'f':
      Some{Chr{12}}
    case 'n':
      Some{Chr{10}}
    case 'r':
      Some{Chr{13}}
    case 't':
      Some{Chr{9}}
    case Chr{code}:
      None{}

def escape_decoded(value: Maybe<&2, Char>, target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match value:
    case None{}:
      Invalid{"invalid JSON escape"}
    case Some{ch}:
      Continue{StringChars{target, SCon{ch, reversed}}, cursor, frames, depth}

def escape(cursor: Cursor, target: Target, reversed: String, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      fail(column, "incomplete escape")
    case At{SCon{'u', rest}, column}:
      Continue{Unicode{target, reversed, 4n, 0, None{}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{SCon{ch, rest}, column}:
      escape_decoded(decoded(ch), target, reversed, At{rest, (column + 1 : U32)}, frames, depth)

def hex.pick(decimal: Bool, lower: Bool, upper: Bool, code: U32) -> Maybe<&2, U32>:
  match decimal:
    case True{}:
      Some{(code - 48 : U32)}
    case False{}:
      match lower:
        case True{}:
          Some{(code - 87 : U32)}
        case False{}:
          match upper:
            case True{}:
              Some{(code - 55 : U32)}
            case False{}:
              None{}

def hex(ch: Char) -> Maybe<&2, U32>:
  match ch:
    case Chr{+code}:
      hex.pick(Bool.and(U32.is_ge(code, 48), U32.is_le(code, 57)),
        Bool.and(U32.is_ge(code, 97), U32.is_le(code, 102)),
        Bool.and(U32.is_ge(code, 65), U32.is_le(code, 70)), code)

def unicode_add(scalar: U32, target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  Continue{StringChars{target, SCon{Chr{scalar}, reversed}}, cursor, frames, depth}

def unicode_low(valid: Bool, scalar: U32, high: U32, target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match valid:
    case True{}:
      unicode_add((65536 + (high - 55296) * 1024 + scalar - 56320 : U32),
        target, reversed, cursor, frames, depth)
    case False{}:
      match cursor:
        case At{rest, column}:
          fail(column, "invalid low surrogate")

def unicode_first(high: Bool, low: Bool, scalar: U32, target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match high:
    case True{}:
      Continue{LowSlash{target, reversed, scalar}, cursor, frames, depth}
    case False{}:
      match low:
        case True{}:
          match cursor:
            case At{rest, column}:
              fail(column, "unpaired low surrogate")
        case False{}:
          unicode_add(scalar, target, reversed, cursor, frames, depth)

def unicode_done(high: Maybe<&2, U32>, +scalar: U32, target: Target, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match high:
    case None{}:
      unicode_first(Bool.and(U32.is_ge(scalar, 55296), U32.is_le(scalar, 56319)),
        Bool.and(U32.is_ge(scalar, 56320), U32.is_le(scalar, 57343)),
        scalar, target, reversed, cursor, frames, depth)
    case Some{value}:
      unicode_low(Bool.and(U32.is_ge(scalar, 56320), U32.is_le(scalar, 57343)),
        scalar, value, target, reversed, cursor, frames, depth)

def unicode_digit(value: Maybe<&2, U32>, target: Target, reversed: String, remaining: Nat, scalar: U32, high: Maybe<&2, U32>, rest: String, column: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match value:
    case None{}:
      fail(column, "invalid Unicode escape")
    case Some{n}:
      Continue{Unicode{target, reversed, remaining, (scalar * 16 + n : U32), high},
        At{rest, (column + 1 : U32)}, frames, depth}

def unicode(remaining: Nat, cursor: Cursor, target: Target, reversed: String, scalar: U32, high: Maybe<&2, U32>, frames: List<&2, Frame>, depth: U32) -> State:
  match remaining:
    case 0n:
      unicode_done(high, scalar, target, reversed, cursor, frames, depth)
    case 1n+p:
      match cursor:
        case At{SNil{}, column}:
          fail(column, "incomplete Unicode escape")
        case At{SCon{ch, rest}, column}:
          unicode_digit(hex(ch), target, reversed, p, scalar, high, rest, column, frames, depth)

def low_slash(cursor: Cursor, target: Target, reversed: String, high: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SCon{'\\', rest}, column}:
      Continue{LowU{target, reversed, high}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{rest, column}:
      fail(column, "expected '\\'")

def low_u(cursor: Cursor, target: Target, reversed: String, high: U32, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SCon{'u', rest}, column}:
      Continue{Unicode{target, reversed, 4n, 0, Some{high}}, At{rest, (column + 1 : U32)}, frames, depth}
    case At{rest, column}:
      fail(column, "expected 'u'")

type NumericAction is Data:
  TakeDigit{phase: NumberPhase}
  EndNumber{}
  BadNumber{message: String}

def number_digit(yes: Bool, phase: NumberPhase, other: NumericAction) -> NumericAction:
  match yes:
    case True{}:
      TakeDigit{phase}
    case False{}:
      other

def integer_action(ch: Char, phase: NumberPhase, digits: Bool) -> NumericAction:
  match ch:
    case '.':
      TakeDigit{Dot{}}
    case 'e':
      TakeDigit{Exponent{}}
    case 'E':
      TakeDigit{Exponent{}}
    case +value:
      number_digit(Bool.and(digits, digit(value)), phase, EndNumber{})

def numeric_action(phase: NumberPhase, ch: Char) -> NumericAction:
  match phase:
    case Minus{}:
      match ch:
        case '0':
          TakeDigit{ZeroInt{}}
        case value:
          number_digit(digit(value), Integer{}, BadNumber{"expected number"})
    case ZeroInt{}:
      integer_action(ch, ZeroInt{}, False{})
    case Integer{}:
      integer_action(ch, Integer{}, True{})
    case Dot{}:
      number_digit(digit(ch), Fraction{}, BadNumber{"expected digit"})
    case Fraction{}:
      match ch:
        case 'e':
          TakeDigit{Exponent{}}
        case 'E':
          TakeDigit{Exponent{}}
        case value:
          number_digit(digit(value), Fraction{}, EndNumber{})
    case Exponent{}:
      match ch:
        case '+':
          TakeDigit{ExpSign{}}
        case '-':
          TakeDigit{ExpSign{}}
        case value:
          number_digit(digit(value), ExpDigits{}, BadNumber{"expected digit"})
    case ExpSign{}:
      number_digit(digit(ch), ExpDigits{}, BadNumber{"expected digit"})
    case ExpDigits{}:
      number_digit(digit(ch), ExpDigits{}, EndNumber{})

def numeric_eof(phase: NumberPhase) -> NumericAction:
  match phase:
    case Minus{}:
      BadNumber{"expected number"}
    case Dot{}:
      BadNumber{"expected digit"}
    case Exponent{}:
      BadNumber{"expected digit"}
    case ExpSign{}:
      BadNumber{"expected digit"}
    case ZeroInt{}:
      EndNumber{}
    case Integer{}:
      EndNumber{}
    case Fraction{}:
      EndNumber{}
    case ExpDigits{}:
      EndNumber{}

def numeric_apply(action: NumericAction, reversed: String, cursor: Cursor, frames: List<&2, Frame>, depth: U32) -> State:
  match action:
    case BadNumber{message}:
      match cursor:
        case At{rest, column}:
          fail(column, message)
    case EndNumber{}:
      Continue{Ready{Number{reverse(reversed, "")}}, cursor, frames, depth}
    case TakeDigit{phase}:
      match cursor:
        case At{SNil{}, column}:
          fail(column, "expected digit")
        case At{SCon{ch, rest}, column}:
          Continue{Numeric{phase, SCon{ch, reversed}}, At{rest, (column + 1 : U32)}, frames, depth}

def numeric(cursor: Cursor, phase: NumberPhase, reversed: String, frames: List<&2, Frame>, depth: U32) -> State:
  match cursor:
    case At{SNil{}, column}:
      numeric_apply(numeric_eof(phase), reversed, At{"", column}, frames, depth)
    case At{SCon{+ch, rest}, column}:
      numeric_apply(numeric_action(phase, ch), reversed, At{SCon{ch, rest}, column}, frames, depth)

def step(mode: Mode, cursor: Cursor, frames: List<&2, Frame>, +depth: U32) -> State:
  match mode:
    case Value{}:
      value_limit(U32.is_gt(depth, 512), whitespace(cursor), frames, depth)
    case ArrayStart{}:
      array_start(whitespace(cursor), frames, depth)
    case ArrayAfter{items}:
      array_after(whitespace(cursor), items, frames, depth)
    case ObjectStart{fields, empty_ok}:
      object_start(whitespace(cursor), fields, empty_ok, frames, depth)
    case ObjectAfter{fields}:
      object_after(whitespace(cursor), fields, frames, depth)
    case Colon{+fields, +key}:
      unique_colon(key_exists(fields, key), cursor, fields, key, frames, depth)
    case StringChars{target, reversed}:
      string_chars(cursor, target, reversed, frames, depth)
    case Escape{target, reversed}:
      escape(cursor, target, reversed, frames, depth)
    case Unicode{target, reversed, remaining, scalar, high}:
      unicode(remaining, cursor, target, reversed, scalar, high, frames, depth)
    case LowSlash{target, reversed, high}:
      low_slash(cursor, target, reversed, high, frames, depth)
    case LowU{target, reversed, high}:
      low_u(cursor, target, reversed, high, frames, depth)
    case Numeric{phase, reversed}:
      numeric(cursor, phase, reversed, frames, depth)
    case Literal{expected, value}:
      literal(expected, cursor, value, frames, depth)
    case Ready{value}:
      ready(frames, cursor, value, depth)

# Each nonterminal step consumes input or reduces a finite continuation.
# The depth check matches the original reader's 512-level bound.
@unsafe def run(state: State) -> Result<&2, &2, String, T>:
  match state:
    case Invalid{message}:
      Fail{message}
    case Complete{value}:
      Done{value}
    case Continue{mode, cursor, frames, depth}:
      run(step(mode, cursor, frames, depth))

def parse(source: String) -> Result<&2, &2, String, T>:
  run(Continue{Value{}, At{source, 1}, Nil{}, 0})

def hex_char.pick(decimal: Bool, n: U32) -> Char:
  match decimal:
    case True{}:
      Chr{(48 + n : U32)}
    case False{}:
      Chr{(87 + n : U32)}

def hex_char(+n: U32) -> Char:
  hex_char.pick(U32.is_lt(n, 10), n)

def quoted_scalar(control: Bool, +code: U32) -> String:
  match control:
    case True{}:
      "\\u00" ++ SCon{hex_char(U32.shrn(code, 4n)), SCon{hex_char(U32.and(code, 15)), ""}}
    case False{}:
      SCon{Chr{code}, ""}

def quoted_char(ch: Char) -> String:
  match ch:
    case '"':
      "\\\""
    case '\\':
      "\\\\"
    case Chr{+code}:
      quoted_scalar(U32.is_lt(code, 32), code)

def quoted_body(text: String, reversed: String) -> String:
  match text:
    case SNil{}:
      reverse(SCon{'"', reversed}, "")
    case SCon{ch, rest}:
      quoted_body(rest, reverse(quoted_char(ch), reversed))

def quoted(text: String) -> String:
  quoted_body(text, "\"")

type Render is Data:
  RenderValue{value: T}
  RenderItems{items: List<&2, T>, separator: String}
  RenderFields{fields: List<&2, Field<T>>, separator: String}
  RenderText{text: String}

@unsafe def render(work: List<Render>, reversed: String) -> String:
  match work:
    case Nil{}:
      reverse(reversed, "")
    case Con{RenderText{text}, rest}:
      render(rest, reverse(text, reversed))
    case Con{RenderValue{Null{}}, rest}:
      render(rest, reverse("null", reversed))
    case Con{RenderValue{Boolean{value}}, rest}:
      render(rest, reverse(Bool.pick(String, value, "true", "false"), reversed))
    case Con{RenderValue{Number{value}}, rest}:
      render(rest, reverse(value, reversed))
    case Con{RenderValue{Text{value}}, rest}:
      render(rest, reverse(quoted(value), reversed))
    case Con{RenderValue{Items{items}}, rest}:
      render(Con{RenderItems{items, ""}, rest}, SCon{'[', reversed})
    case Con{RenderValue{Object{fields}}, rest}:
      render(Con{RenderFields{fields, ""}, rest}, SCon{'{', reversed})
    case Con{RenderItems{Nil{}, separator}, rest}:
      render(rest, SCon{']', reversed})
    case Con{RenderItems{Con{item, tail}, separator}, rest}:
      render(Con{RenderValue{item}, Con{RenderItems{tail, ","}, rest}},
        reverse(separator, reversed))
    case Con{RenderFields{Nil{}, separator}, rest}:
      render(rest, SCon{'}', reversed})
    case Con{RenderFields{Con{Field{key, value}, tail}, separator}, rest}:
      render(Con{RenderValue{value}, Con{RenderFields{tail, ","}, rest}},
        reverse(separator ++ quoted(key) ++ ":", reversed))

def to_string(value: T) -> String:
  render(Con{RenderValue{value}, Nil{}}, "")

def object_fields(value: T) -> Result<&2, &2, String, List<&2, Field<T>>>:
  match value:
    case Object{fields}:
      Done{fields}
    case other:
      Fail{"expected object"}

def string(value: T) -> Result<&2, &2, String, String>:
  match value:
    case Text{text}:
      Done{text}
    case other:
      Fail{"expected string"}

def bool(value: T) -> Result<&2, &2, String, Bool>:
  match value:
    case Boolean{b}:
      Done{b}
    case other:
      Fail{"expected boolean"}

def array(value: T) -> Result<&2, &2, String, List<&2, T>>:
  match value:
    case Items{items}:
      Done{items}
    case other:
      Fail{"expected array"}

def field_pick(same: Bool, value: T, rest: Result<&2, &2, String, T>) -> Result<&2, &2, String, T>:
  match same:
    case True{}:
      Done{value}
    case False{}:
      rest

def field(fields: List<&2, Field<T>>, +key: String) -> Result<&2, &2, String, T>:
  match fields:
    case Nil{}:
      Fail{"missing field " ++ key}
    case Con{Field{name, value}, rest}:
      field_pick(str_eq(name, key), value, field(rest, key))

def required(expected: List<&2, String>, +fields: List<&2, Field<T>>) -> Bool:
  match expected:
    case Nil{}:
      True{}
    case Con{key, rest}:
      Bool.and(key_exists(fields, key), required(rest, fields))

def join_strings(xs: List<&2, String>, separator: String, +between: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{head, rest}:
      separator ++ head ++ join_strings(rest, between, between)

def exact_fields(valid: Bool, fields: List<&2, Field<T>>, expected: List<&2, String>) -> Result<&2, &2, String, List<&2, Field<T>>>:
  match valid:
    case True{}:
      Done{fields}
    case False{}:
      Fail{"expected exactly fields " ++ join_strings(expected, "", ",")}

def exact_object(value: T, +expected: List<&2, String>) -> Result<&2, &2, String, List<&2, Field<T>>>:
  match value:
    case Object{+fields}:
      exact_fields(Bool.and(
        Nat.is_eq(List.length(&2, Field<T>, fields), List.length(&2, String, expected)),
        required(expected, fields)), fields, expected)
    case other:
      Fail{"expected object"}

def decimal_digits(s: String) -> Bool:
  match s:
    case SNil{}:
      True{}
    case SCon{ch, rest}:
      Bool.and(digit(ch), decimal_digits(rest))

def trim_zero(s: String) -> String:
  match s:
    case SNil{}:
      "0"
    case SCon{'0', rest}:
      trim_zero(rest)
    case SCon{ch, rest}:
      SCon{ch, rest}

def lexical_pick(order: Cmp, rest: Cmp) -> Cmp:
  match order:
    case LT{}:
      LT{}
    case GT{}:
      GT{}
    case EQ{}:
      rest

def lexical(a: String, b: String) -> Cmp:
  match a:
    case SNil{}:
      match b:
        case SNil{}:
          EQ{}
        case SCon{h, t}:
          LT{}
    case SCon{Chr{x}, xs}:
      match b:
        case SNil{}:
          GT{}
        case SCon{Chr{y}, ys}:
          lexical_pick(U32.cmp(x, y), lexical(xs, ys))

def string_length(s: String) -> Nat:
  match s:
    case SNil{}:
      0n
    case SCon{h, rest}:
      1n+string_length(rest)

def natural_range(order: Cmp, text: String) -> Result<&2, &2, String, String>:
  match order:
    case GT{}:
      Fail{"natural index exceeds host integer range"}
    case EQ{}:
      Done{text}
    case LT{}:
      Done{text}

def natural_length(order: Cmp, +text: String) -> Result<&2, &2, String, String>:
  match order:
    case LT{}:
      Done{text}
    case GT{}:
      Fail{"natural index exceeds host integer range"}
    case EQ{}:
      natural_range(lexical(text, "4611686018427387903"), text)

def natural_valid(valid: Bool, text: String) -> Result<&2, &2, String, String>:
  match valid:
    case False{}:
      Fail{"expected natural integer"}
    case True{}:
      +normalized = trim_zero(text)
      natural_length(Nat.cmp(string_length(normalized), 19n), normalized)

# Source table identifiers are canonical decimal strings. This retains the
# original host-integer range without allocating unary naturals for identifiers.
def nat(value: T) -> Result<&2, &2, String, String>:
  match value:
    case Number{SNil{}}:
      Fail{"expected natural integer"}
    case Number{SCon{+ch, +rest}}:
      natural_valid(decimal_digits(SCon{ch, rest}), SCon{ch, rest})
    case other:
      Fail{"expected natural integer"}

def string_field(fields: List<&2, Field<T>>, key: String) -> Result<&2, &2, String, String>:
  do Result<&2, &2, String, String>:
    value : T <- field(fields, key)
    string(value)

def nat_field(fields: List<&2, Field<T>>, key: String) -> Result<&2, &2, String, String>:
  do Result<&2, &2, String, String>:
    value : T <- field(fields, key)
    nat(value)

def bool_field(fields: List<&2, Field<T>>, key: String) -> Result<&2, &2, String, Bool>:
  do Result<&2, &2, String, Bool>:
    value : T <- field(fields, key)
    bool(value)

def nats(values: List<&2, T>) -> Result<&2, &2, String, List<&2, String>>:
  match values:
    case Nil{}:
      Done{Nil{}}
    case Con{head, tail}:
      do Result<&2, &2, String, List<&2, String>>:
        item : String <- nat(head)
        rest : List<&2, String> <- nats(tail)
        return Con{item, rest}

def nat_list(value: T) -> Result<&2, &2, String, List<&2, String>>:
  do Result<&2, &2, String, List<&2, String>>:
    values : List<&2, T> <- array(value)
    nats(values)

def nats_field(fields: List<&2, Field<T>>, key: String) -> Result<&2, &2, String, List<&2, String>>:
  do Result<&2, &2, String, List<&2, String>>:
    value : T <- field(fields, key)
    nat_list(value)
