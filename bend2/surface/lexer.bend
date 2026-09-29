import Base
import ./token.bend as Token
import ../kernel/error.bend as Error

# Decimal token payloads stay lossless until the parser constructs Bignum.T.
type Mode is Data:
  Scan{}
  Word{}
  Number{}
  DotNumber{}
  Comment{}
  ByteString{}
  Escape{}
  HexFirst{}
  HexSecond{high: U32}

type State is Data:
  Running{mode: Mode, source: String, loc: Token.Loc, start: Token.Loc,
    tokens: List<&2, Token.T>, text: String, bytes: List<&2, U32>}
  Finished{tokens: List<&2, Token.T>}
  Failed{error: Error.T}

def fail(loc: Token.Loc, message: String) -> State:
  match loc:
    case Token.Loc{line, col}:
      Failed{Error.Parse{message, line, col}}

def emit(kind: Token.KindT, source: String, loc: Token.Loc,
    +start: Token.Loc, tokens: List<&2, Token.T>) -> State:
  Running{Scan{}, source, loc, start, Token.Tok{kind, start} <> tokens, "", []}

def is_digit(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 48) && U32.is_le(Char.to_u32(c), 57)

def is_ident_start(+c: Char) -> Bool:
  Char.is_alpha(c) || Char.is_eq(c, '_')

def is_ident_char(+c: Char) -> Bool:
  is_ident_start(c) || is_digit(c) || Char.is_eq(c, '\'')

def keyword_pick(same: Bool, value: Token.KindT, other: Token.KindT) -> Token.KindT:
  Bool.pick(Token.KindT, same, value, other)

def ident_kind(name: String) -> Token.KindT:
  match name:
    case SCon{'d', +tail}:
      k : Token.KindT = Token.Ident{SCon{'d', tail}}
      k = keyword_pick(String.eq(tail, "ef"), Token.KDef{}, k)
      k = keyword_pick(String.eq(tail, "ec"), Token.KDec{}, k)
      k
    case SCon{'a', +tail}:
      k : Token.KindT = Token.Ident{SCon{'a', tail}}
      k = keyword_pick(String.eq(tail, "xiom"), Token.KAxiom{}, k)
      k = keyword_pick(String.eq(tail, "s"), Token.KAs{}, k)
      k = keyword_pick(String.eq(tail, "bsurd"), Token.KAbsurd{}, k)
      k = keyword_pick(String.eq(tail, "uto"), Token.KAuto{}, k)
      k = keyword_pick(String.eq(tail, "nd"), Token.KAnd{}, k)
      k
    case SCon{'f', +tail}:
      k : Token.KindT = Token.Ident{SCon{'f', tail}}
      k = keyword_pick(String.eq(tail, "un"), Token.KFun{}, k)
      k = keyword_pick(String.eq(tail, "hc"), Token.KFhc{}, k)
      k
    case SCon{'i', +tail}:
      k : Token.KindT = Token.Ident{SCon{'i', tail}}
      k = keyword_pick(String.eq(tail, "nj"), Token.KInj{}, k)
      k = keyword_pick(String.eq(tail, "n"), Token.KIn{}, k)
      k = keyword_pick(String.eq(tail, "nput"), Token.KInput{}, k)
      k
    case SCon{'o', +tail}:
      k : Token.KindT = Token.Ident{SCon{'o', tail}}
      k = keyword_pick(String.eq(tail, "f"), Token.KOf{}, k)
      k = keyword_pick(String.eq(tail, "pen"), Token.KOpen{}, k)
      k
    case SCon{'c', +tail}:
      k : Token.KindT = Token.Ident{SCon{'c', tail}}
      k = keyword_pick(String.eq(tail, "ase"), Token.KCase{}, k)
      k
    case SCon{'m', +tail}:
      k : Token.KindT = Token.Ident{SCon{'m', tail}}
      k = keyword_pick(String.eq(tail, "atch"), Token.KMatch{}, k)
      k = keyword_pick(String.eq(tail, "u"), Token.KMu{}, k)
      k = keyword_pick(String.eq(tail, "utual"), Token.KMutual{}, k)
      k = keyword_pick(String.eq(tail, "pc"), Token.KMpc{}, k)
      k
    case SCon{'r', +tail}:
      k : Token.KindT = Token.Ident{SCon{'r', tail}}
      k = keyword_pick(String.eq(tail, "eturn"), Token.KReturn{}, k)
      k = keyword_pick(String.eq(tail, "ec"), Token.KRec{}, k)
      k
    case SCon{'w', +tail}:
      k : Token.KindT = Token.Ident{SCon{'w', tail}}
      k = keyword_pick(String.eq(tail, "ith"), Token.KWith{}, k)
      k = keyword_pick(String.eq(tail, "here"), Token.KWhere{}, k)
      k
    case SCon{'t', +tail}:
      k : Token.KindT = Token.Ident{SCon{'t', tail}}
      k = keyword_pick(String.eq(tail, "uple"), Token.KTuple{}, k)
      k
    case SCon{'s', +tail}:
      k : Token.KindT = Token.Ident{SCon{'s', tail}}
      k = keyword_pick(String.eq(tail, "um"), Token.KSum{}, k)
      k = keyword_pick(String.eq(tail, "pecialize"), Token.KSpecialize{}, k)
      k = keyword_pick(String.eq(tail, "hare"), Token.KShare{}, k)
      k
    case SCon{'p', +tail}:
      k : Token.KindT = Token.Ident{SCon{'p', tail}}
      k = keyword_pick(String.eq(tail, "rod"), Token.KProd{}, k)
      k = keyword_pick(String.eq(tail, "oly"), Token.KPoly{}, k)
      k = keyword_pick(String.eq(tail, "rove"), Token.KProve{}, k)
      k
    case SCon{'P', +tail}:
      k : Token.KindT = Token.Ident{SCon{'P', tail}}
      k = keyword_pick(String.eq(tail, "rop"), Token.KProp{}, k)
      k
    case SCon{'T', +tail}:
      k : Token.KindT = Token.Ident{SCon{'T', tail}}
      k = keyword_pick(String.eq(tail, "ype"), Token.KType{}, k)
      k
    case SCon{'S', +tail}:
      k : Token.KindT = Token.Ident{SCon{'S', tail}}
      k = keyword_pick(String.eq(tail, "ort"), Token.KSort{}, k)
      k
    case SCon{'g', +tail}:
      k : Token.KindT = Token.Ident{SCon{'g', tail}}
      k = keyword_pick(String.eq(tail, "roup"), Token.KGroup{}, k)
      k
    case SCon{'l', +tail}:
      k : Token.KindT = Token.Ident{SCon{'l', tail}}
      k = keyword_pick(String.eq(tail, "et"), Token.KLet{}, k)
      k
    case SCon{'n', +tail}:
      k : Token.KindT = Token.Ident{SCon{'n', tail}}
      k = keyword_pick(String.eq(tail, "atAdd"), Token.KNatAdd{}, k)
      k = keyword_pick(String.eq(tail, "atSub"), Token.KNatSub{}, k)
      k = keyword_pick(String.eq(tail, "atMul"), Token.KNatMul{}, k)
      k = keyword_pick(String.eq(tail, "atEq"), Token.KNatEq{}, k)
      k = keyword_pick(String.eq(tail, "atLt"), Token.KNatLt{}, k)
      k = keyword_pick(String.eq(tail, "u"), Token.KNu{}, k)
      k
    case SCon{'e', +tail}:
      k : Token.KindT = Token.Ident{SCon{'e', tail}}
      k = keyword_pick(String.eq(tail, "nd"), Token.KEnd{}, k)
      k = keyword_pick(String.eq(tail, "nc"), Token.KEnc{}, k)
      k = keyword_pick(String.eq(tail, "val"), Token.KEval{}, k)
      k
    case SCon{'z', +tail}:
      k : Token.KindT = Token.Ident{SCon{'z', tail}}
      k = keyword_pick(String.eq(tail, "k"), Token.KZk{}, k)
      k
    case SCon{'v', +tail}:
      k : Token.KindT = Token.Ident{SCon{'v', tail}}
      k = keyword_pick(String.eq(tail, "erify"), Token.KVerify{}, k)
      k
    case name:
      Token.Ident{name}


def normalize_digits(digits: String) -> String:
  match digits:
    case SNil{}:
      "0"
    case SCon{'0', tail}:
      normalize_digits(tail)
    case other:
      other

def dot_kind(+digits: String) -> Maybe<&2, Token.KindT>:
  Bool.pick(Maybe<&2, Token.KindT>, String.eq(digits, "1"), Some{Token.Dot1{}},
    Bool.pick(Maybe<&2, Token.KindT>, String.eq(digits, "2"), Some{Token.Dot2{}}, None{}))

def finish_dot(kind: Maybe<&2, Token.KindT>, digits: String, source: String,
    loc: Token.Loc, +start: Token.Loc, tokens: List<&2, Token.T>) -> State:
  match kind:
    case Some{k}:
      emit(k, source, loc, start, tokens)
    case None{}:
      emit(Token.NatTok{normalize_digits(digits)}, source, loc, Token.advance(start, 1n), Token.Tok{Token.Dot{}, start} <> tokens)

def finish_number(mode: Mode, text: String, source: String, loc: Token.Loc,
    +start: Token.Loc, tokens: List<&2, Token.T>) -> State:
  match mode:
    case DotNumber{}:
      +digits = String.reverse(text)
      finish_dot(dot_kind(digits), digits, source, loc, start, tokens)
    case other:
      emit(Token.NatTok{normalize_digits(String.reverse(text))}, source, loc, start, tokens)

def hex_digit(+c: Char) -> Maybe<&2, U32>:
  +n = Char.to_u32(c)
  Bool.pick(Maybe<&2, U32>, is_digit(c), Some{(n - 48 : U32)},
    Bool.pick(Maybe<&2, U32>, U32.is_ge(n, 97) && U32.is_le(n, 102), Some{(n - 87 : U32)},
      Bool.pick(Maybe<&2, U32>, U32.is_ge(n, 65) && U32.is_le(n, 70), Some{(n - 55 : U32)}, None{})))

def hex_step(mode: Mode, digit: Maybe<&2, U32>, source: String, +loc: Token.Loc,
    start: Token.Loc, tokens: List<&2, Token.T>, bytes: List<&2, U32>) -> State:
  match mode digit:
    case HexFirst{} Some{n}:
      Running{HexSecond{n}, source, loc, start, tokens, "", bytes}
    case HexSecond{high} Some{n}:
      Running{ByteString{}, source, Token.advance(loc, 4n), start, tokens, "", ((high * 16 + n : U32) <> bytes)}
    case other None{}:
      fail(loc, "byte escape requires two hexadecimal digits")
    case other Some{n}:
      fail(loc, "invalid scanner state")

# Convert source codepoints to their UTF-8 bytes before scanning. OCaml's scanner
# counts bytes and byte literals retain precisely those source bytes.
def utf8_four(+n: U32, tail: String) -> String:
  SCon{Char.from_u32((240 + (n >> 18n) : U32)),
    SCon{Char.from_u32((128 + ((n >> 12n) .&. 63) : U32)),
      SCon{Char.from_u32((128 + ((n >> 6n) .&. 63) : U32)),
        SCon{Char.from_u32((128 + (n .&. 63) : U32)), tail}}}}

def utf8_char(+n: U32, +tail: String) -> String:
  Bool.pick(String, U32.is_lt(n, 128), SCon{Char.from_u32(n), tail},
    Bool.pick(String, U32.is_lt(n, 2048),
      SCon{Char.from_u32((192 + (n >> 6n) : U32)), SCon{Char.from_u32((128 + (n .&. 63) : U32)), tail}},
      Bool.pick(String, U32.is_lt(n, 65536),
        SCon{Char.from_u32((224 + (n >> 12n) : U32)), SCon{Char.from_u32((128 + ((n >> 6n) .&. 63) : U32)), SCon{Char.from_u32((128 + (n .&. 63) : U32)), tail}}},
        utf8_four(n, tail))))

def reverse_chars(source: String, acc: String) -> String:
  match source:
    case SNil{}:
      acc
    case SCon{c, tail}:
      reverse_chars(tail, SCon{c, acc})

def utf8_reversed(source: String, acc: String) -> String:
  match source:
    case SNil{}:
      acc
    case SCon{c, tail}:
      utf8_reversed(tail, utf8_char(Char.to_u32(c), acc))

def utf8(source: String) -> String:
  utf8_reversed(reverse_chars(source, ""), "")

def scan(source: String, +loc: Token.Loc, +tokens: List<&2, Token.T>) -> State:
  match source:
    case SNil{}:
      Finished{List.reverse(&2, Token.T, Token.Tok{Token.Eof{}, loc} <> tokens)}
    case SCon{'b', SCon{'"', tail}}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), loc, tokens, "", []}
    case SCon{' ', tail}:
      Running{Scan{}, tail, Token.advance(loc, 1n), loc, tokens, "", []}
    case SCon{'\t', tail}:
      Running{Scan{}, tail, Token.advance(loc, 1n), loc, tokens, "", []}
    case SCon{'\r', tail}:
      Running{Scan{}, tail, Token.advance(loc, 1n), loc, tokens, "", []}
    case SCon{'\n', tail}:
      Running{Scan{}, tail, Token.next_line(loc), loc, tokens, "", []}
    case SCon{'-', SCon{'-', tail}}:
      Running{Comment{}, tail, Token.advance(loc, 2n), loc, tokens, "", []}
    case SCon{'-', SCon{'>', tail}}:
      emit(Token.Arrow{}, tail, Token.advance(loc, 2n), loc, tokens)
    case SCon{'=', SCon{'>', tail}}:
      emit(Token.DArrow{}, tail, Token.advance(loc, 2n), loc, tokens)
    case SCon{':', SCon{'=', tail}}:
      emit(Token.ColonEq{}, tail, Token.advance(loc, 2n), loc, tokens)
    case SCon{'(', SCon{')', tail}}:
      emit(Token.UnitTok{}, tail, Token.advance(loc, 2n), loc, tokens)
    case SCon{':', tail}:
      emit(Token.Colon{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{'(', tail}:
      emit(Token.LParen{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{')', tail}:
      emit(Token.RParen{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{'*', tail}:
      emit(Token.Star{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{',', tail}:
      emit(Token.Comma{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{'|', tail}:
      emit(Token.Pipe{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{'[', tail}:
      emit(Token.LBracket{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{']', tail}:
      emit(Token.RBracket{}, tail, Token.advance(loc, 1n), loc, tokens)
    case SCon{'.', tail}:
      Running{DotNumber{}, tail, Token.advance(loc, 1n), loc, tokens, "", []}
    case SCon{+c, +tail}:
      Bool.pick(State, is_digit(c), Running{Number{}, tail, Token.advance(loc, 1n), loc, tokens, SCon{c, ""}, []},
        Bool.pick(State, is_ident_start(c), Running{Word{}, tail, Token.advance(loc, 1n), loc, tokens, SCon{c, ""}, []},
          fail(loc, "unexpected character '" ++ SCon{c, "'"})))

def advance_state(mode: Mode, source: String, +loc: Token.Loc, +start: Token.Loc,
    +tokens: List<&2, Token.T>, +text: String, +bytes: List<&2, U32>) -> State:
  match mode source:
    case Scan{} source:
      scan(source, loc, tokens)
    case Word{} SCon{+c, +tail}:
      Bool.pick(State, is_ident_char(c), Running{Word{}, tail, Token.advance(loc, 1n), start, tokens, SCon{c, text}, []},
        emit(ident_kind(String.reverse(text)), SCon{c, tail}, loc, start, tokens))
    case Word{} SNil{}:
      emit(ident_kind(String.reverse(text)), "", loc, start, tokens)
    case Number{} SCon{+c, +tail}:
      Bool.pick(State, is_digit(c), Running{Number{}, tail, Token.advance(loc, 1n), start, tokens, SCon{c, text}, []},
        finish_number(Number{}, text, SCon{c, tail}, loc, start, tokens))
    case Number{} SNil{}:
      finish_number(Number{}, text, "", loc, start, tokens)
    case DotNumber{} SCon{+c, +tail}:
      Bool.pick(State, is_digit(c), Running{DotNumber{}, tail, Token.advance(loc, 1n), start, tokens, SCon{c, text}, []},
        Bool.pick(State, String.is_empty(text), emit(Token.Dot{}, SCon{c, tail}, loc, start, tokens),
          finish_number(DotNumber{}, text, SCon{c, tail}, loc, start, tokens)))
    case DotNumber{} SNil{}:
      Bool.pick(State, String.is_empty(text), emit(Token.Dot{}, "", loc, start, tokens),
        finish_number(DotNumber{}, text, "", loc, start, tokens))
    case Comment{} SCon{'\n', tail}:
      Running{Scan{}, tail, Token.next_line(loc), start, tokens, "", []}
    case Comment{} SCon{c, tail}:
      Running{Comment{}, tail, Token.advance(loc, 1n), start, tokens, "", []}
    case Comment{} SNil{}:
      scan("", loc, tokens)
    case ByteString{} SCon{'"', tail}:
      emit(Token.Bytes{List.reverse(&2, U32, bytes)}, tail, Token.advance(loc, 1n), start, tokens)
    case ByteString{} SCon{'\n', tail}:
      fail(loc, "unescaped newline in byte literal")
    case ByteString{} SCon{'\r', tail}:
      fail(loc, "unescaped newline in byte literal")
    case ByteString{} SCon{'\\', tail}:
      Running{Escape{}, tail, loc, start, tokens, "", bytes}
    case ByteString{} SCon{c, tail}:
      Running{ByteString{}, tail, Token.advance(loc, 1n), start, tokens, "", Char.to_u32(c) <> bytes}
    case ByteString{} SNil{}:
      fail(loc, "unterminated byte literal")
    case Escape{} SCon{'x', SCon{a, SCon{b, tail}}}:
      Running{HexFirst{}, SCon{a, SCon{b, tail}}, loc, start, tokens, "", bytes}
    case Escape{} SCon{'x', tail}:
      fail(loc, "invalid byte escape \\x")
    case Escape{} SCon{'n', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 10 <> bytes}
    case Escape{} SCon{'r', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 13 <> bytes}
    case Escape{} SCon{'t', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 9 <> bytes}
    case Escape{} SCon{'0', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 0 <> bytes}
    case Escape{} SCon{'\\', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 92 <> bytes}
    case Escape{} SCon{'"', tail}:
      Running{ByteString{}, tail, Token.advance(loc, 2n), start, tokens, "", 34 <> bytes}
    case Escape{} SCon{c, tail}:
      fail(loc, "invalid byte escape \\" ++ SCon{c, ""})
    case Escape{} SNil{}:
      fail(loc, "unterminated byte escape")
    case HexFirst{} SCon{c, tail}:
      hex_step(HexFirst{}, hex_digit(c), tail, loc, start, tokens, bytes)
    case HexSecond{high} SCon{c, tail}:
      hex_step(HexSecond{high}, hex_digit(c), tail, loc, start, tokens, bytes)
    case HexFirst{} SNil{}:
      fail(loc, "invalid byte escape \\x")
    case HexSecond{high} SNil{}:
      fail(loc, "invalid byte escape \\x")

def step(state: State) -> State:
  match state:
    case Running{mode, source, loc, start, tokens, text, bytes}:
      advance_state(mode, source, loc, start, tokens, text, bytes)
    case other:
      other

@unsafe
def run(state: State) -> Result<&2, &2, Error.T, List<&2, Token.T>>:
  match state:
    case Finished{tokens}:
      Done{tokens}
    case Failed{error}:
      Fail{error}
    case Running{mode, source, loc, start, tokens, text, bytes}:
      run(step(Running{mode, source, loc, start, tokens, text, bytes}))

def lex(source: String) -> Result<&2, &2, Error.T, List<&2, Token.T>>:
  run(Running{Scan{}, utf8(source), Token.start(), Token.start(), [], "", []})
