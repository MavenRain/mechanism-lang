import Base
import ./token.bend as Tk

# Line{kind}: 0 plain comment, 1 outer doc (///), 2 inner doc (//!).
type Mode is Data:
  Scan{}
  Word{}
  Raw{}
  Number{}
  Str{}
  Life{}
  Line{kind: Nat}
  Block{depth: Nat}

# `nl` counts newlines since the previous token; two or more mark a gap.
type State is Data:
  Running{mode: Mode, source: String, loc: Tk.Loc, start: Tk.Loc,
    tokens: List<&2, Tk.T>, text: String, nl: Nat}
  Finished{tokens: List<&2, Tk.T>}
  Failed{message: String}

def fail(loc: Tk.Loc, message: String) -> State:
  Failed{Tk.show_loc(loc) ++ " " ++ message}

def gap(nl: Nat) -> Bool:
  Nat.is_ge(nl, 2n)

def emit(kind: Tk.KindT, source: String, loc: Tk.Loc,
    +start: Tk.Loc, tokens: List<&2, Tk.T>, nl: Nat) -> State:
  Running{Scan{}, source, loc, start, Tk.Tok{kind, start, gap(nl)} <> tokens, "", 0n}

def punct(p: String, n: Nat, source: String, +loc: Tk.Loc,
    tokens: List<&2, Tk.T>, nl: Nat) -> State:
  emit(Tk.Punct{p}, source, Tk.advance(loc, n), loc, tokens, nl)

def is_digit(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 48) && U32.is_le(Char.to_u32(c), 57)

def is_ident_start(+c: Char) -> Bool:
  Char.is_alpha(c) || Char.is_eq(c, '_')

def is_ident_char(+c: Char) -> Bool:
  is_ident_start(c) || is_digit(c)

def is_punct(+c: Char) -> Bool:
  String.contains("()[]{},;:.=<>+-*/%!&|^?#@$~", SCon{c, ""})

def line_kind(kind: Nat, text: String) -> Tk.KindT:
  match kind:
    case 0n:
      Tk.Comment{text}
    case 1n:
      Tk.DocOuter{text}
    case other:
      Tk.DocInner{text}

def block_close(p: Nat, source: String, loc: Tk.Loc, +start: Tk.Loc,
    tokens: List<&2, Tk.T>, nl: Nat) -> State:
  match p:
    case 0n:
      emit(Tk.Comment{""}, source, loc, start, tokens, nl)
    case 1n+q:
      Running{Block{1n+q}, source, loc, start, tokens, "", nl}

def block_end(depth: Nat, source: String, loc: Tk.Loc, +start: Tk.Loc,
    tokens: List<&2, Tk.T>, nl: Nat) -> State:
  match depth:
    case 0n:
      fail(start, "invalid block comment depth")
    case 1n+p:
      block_close(p, source, loc, start, tokens, nl)

def scan(source: String, +loc: Tk.Loc, +tokens: List<&2, Tk.T>, +nl: Nat) -> State:
  match source:
    case SNil{}:
      Finished{List.reverse(&2, Tk.T, Tk.Tok{Tk.Eof{}, loc, gap(nl)} <> tokens)}
    case SCon{' ', tail}:
      Running{Scan{}, tail, Tk.advance(loc, 1n), loc, tokens, "", nl}
    case SCon{'\t', tail}:
      Running{Scan{}, tail, Tk.advance(loc, 1n), loc, tokens, "", nl}
    case SCon{'\r', tail}:
      Running{Scan{}, tail, Tk.advance(loc, 1n), loc, tokens, "", nl}
    case SCon{'\n', tail}:
      Running{Scan{}, tail, Tk.next_line(loc), loc, tokens, "", 1n+nl}
    case SCon{'/', SCon{'/', SCon{'/', SCon{'/', tail}}}}:
      Running{Line{0n}, tail, Tk.advance(loc, 4n), loc, tokens, "", nl}
    case SCon{'/', SCon{'/', SCon{'/', tail}}}:
      Running{Line{1n}, tail, Tk.advance(loc, 3n), loc, tokens, "", nl}
    case SCon{'/', SCon{'/', SCon{'!', tail}}}:
      Running{Line{2n}, tail, Tk.advance(loc, 3n), loc, tokens, "", nl}
    case SCon{'/', SCon{'/', tail}}:
      Running{Line{0n}, tail, Tk.advance(loc, 2n), loc, tokens, "", nl}
    case SCon{'/', SCon{'*', tail}}:
      Running{Block{1n}, tail, Tk.advance(loc, 2n), loc, tokens, "", nl}
    case SCon{'r', SCon{'#', tail}}:
      Running{Raw{}, tail, Tk.advance(loc, 2n), loc, tokens, "", nl}
    case SCon{'"', tail}:
      Running{Str{}, tail, Tk.advance(loc, 1n), loc, tokens, "", nl}
    case SCon{'\'', SCon{'\\', SCon{c, SCon{'\'', tail}}}}:
      emit(Tk.ChrLit{SCon{'\\', SCon{c, ""}}}, tail, Tk.advance(loc, 4n), loc, tokens, nl)
    case SCon{'\'', SCon{c, SCon{'\'', tail}}}:
      emit(Tk.ChrLit{SCon{c, ""}}, tail, Tk.advance(loc, 3n), loc, tokens, nl)
    case SCon{'\'', tail}:
      Running{Life{}, tail, Tk.advance(loc, 1n), loc, tokens, "", nl}
    case SCon{':', SCon{':', tail}}:
      punct("::", 2n, tail, loc, tokens, nl)
    case SCon{'-', SCon{'>', tail}}:
      punct("->", 2n, tail, loc, tokens, nl)
    case SCon{'=', SCon{'>', tail}}:
      punct("=>", 2n, tail, loc, tokens, nl)
    case SCon{'=', SCon{'=', tail}}:
      punct("==", 2n, tail, loc, tokens, nl)
    case SCon{'!', SCon{'=', tail}}:
      punct("!=", 2n, tail, loc, tokens, nl)
    case SCon{'<', SCon{'=', tail}}:
      punct("<=", 2n, tail, loc, tokens, nl)
    case SCon{'>', SCon{'=', tail}}:
      punct(">=", 2n, tail, loc, tokens, nl)
    case SCon{'&', SCon{'&', tail}}:
      punct("&&", 2n, tail, loc, tokens, nl)
    case SCon{'|', SCon{'|', tail}}:
      punct("||", 2n, tail, loc, tokens, nl)
    case SCon{'.', SCon{'.', SCon{'=', tail}}}:
      punct("..=", 3n, tail, loc, tokens, nl)
    case SCon{'.', SCon{'.', tail}}:
      punct("..", 2n, tail, loc, tokens, nl)
    case SCon{'+', SCon{'=', tail}}:
      punct("+=", 2n, tail, loc, tokens, nl)
    case SCon{'-', SCon{'=', tail}}:
      punct("-=", 2n, tail, loc, tokens, nl)
    case SCon{'*', SCon{'=', tail}}:
      punct("*=", 2n, tail, loc, tokens, nl)
    case SCon{+c, +tail}:
      Bool.pick(State, is_digit(c), Running{Number{}, tail, Tk.advance(loc, 1n), loc, tokens, SCon{c, ""}, nl},
        Bool.pick(State, is_ident_start(c), Running{Word{}, tail, Tk.advance(loc, 1n), loc, tokens, SCon{c, ""}, nl},
          Bool.pick(State, is_punct(c), punct(SCon{c, ""}, 1n, tail, loc, tokens, nl),
            fail(loc, "unexpected character '" ++ SCon{c, "'"}))))

def advance_state(mode: Mode, source: String, +loc: Tk.Loc, +start: Tk.Loc,
    +tokens: List<&2, Tk.T>, +text: String, +nl: Nat) -> State:
  match mode source:
    case Scan{} source:
      scan(source, loc, tokens, nl)
    case Word{} SCon{+c, +tail}:
      Bool.pick(State, is_ident_char(c), Running{Word{}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl},
        emit(Tk.Ident{String.reverse(text)}, SCon{c, tail}, loc, start, tokens, nl))
    case Word{} SNil{}:
      emit(Tk.Ident{String.reverse(text)}, "", loc, start, tokens, nl)
    case Raw{} SCon{+c, +tail}:
      Bool.pick(State, is_ident_char(c), Running{Raw{}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl},
        emit(Tk.RawIdent{String.reverse(text)}, SCon{c, tail}, loc, start, tokens, nl))
    case Raw{} SNil{}:
      emit(Tk.RawIdent{String.reverse(text)}, "", loc, start, tokens, nl)
    case Number{} SCon{+c, +tail}:
      Bool.pick(State, is_ident_char(c), Running{Number{}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl},
        emit(Tk.Int{String.reverse(text)}, SCon{c, tail}, loc, start, tokens, nl))
    case Number{} SNil{}:
      emit(Tk.Int{String.reverse(text)}, "", loc, start, tokens, nl)
    case Life{} SCon{+c, +tail}:
      Bool.pick(State, is_ident_char(c), Running{Life{}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl},
        emit(Tk.Lifetime{String.reverse(text)}, SCon{c, tail}, loc, start, tokens, nl))
    case Life{} SNil{}:
      emit(Tk.Lifetime{String.reverse(text)}, "", loc, start, tokens, nl)
    case Str{} SCon{'\\', SCon{c, tail}}:
      Running{Str{}, tail, Tk.advance(loc, 2n), start, tokens, SCon{c, SCon{'\\', text}}, nl}
    case Str{} SCon{'"', tail}:
      emit(Tk.Str{String.reverse(text)}, tail, Tk.advance(loc, 1n), start, tokens, nl)
    case Str{} SCon{'\n', tail}:
      Running{Str{}, tail, Tk.next_line(loc), start, tokens, SCon{'\n', text}, nl}
    case Str{} SCon{c, tail}:
      Running{Str{}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl}
    case Str{} SNil{}:
      fail(start, "unterminated string literal")
    case Line{kind} SCon{'\n', tail}:
      Running{Scan{}, tail, Tk.next_line(loc), loc, Tk.Tok{line_kind(kind, String.reverse(text)), start, gap(nl)} <> tokens, "", 1n}
    case Line{kind} SCon{c, tail}:
      Running{Line{kind}, tail, Tk.advance(loc, 1n), start, tokens, SCon{c, text}, nl}
    case Line{kind} SNil{}:
      emit(line_kind(kind, String.reverse(text)), "", loc, start, tokens, nl)
    case Block{depth} SCon{'*', SCon{'/', tail}}:
      block_end(depth, tail, Tk.advance(loc, 2n), start, tokens, nl)
    case Block{depth} SCon{'/', SCon{'*', tail}}:
      Running{Block{1n+depth}, tail, Tk.advance(loc, 2n), start, tokens, "", nl}
    case Block{depth} SCon{'\n', tail}:
      Running{Block{depth}, tail, Tk.next_line(loc), start, tokens, "", nl}
    case Block{depth} SCon{c, tail}:
      Running{Block{depth}, tail, Tk.advance(loc, 1n), start, tokens, "", nl}
    case Block{depth} SNil{}:
      fail(start, "unterminated block comment")

def step(state: State) -> State:
  match state:
    case Running{mode, source, loc, start, tokens, text, nl}:
      advance_state(mode, source, loc, start, tokens, text, nl)
    case other:
      other

@unsafe
def run(state: State) -> Result<&2, &2, String, List<&2, Tk.T>>:
  match state:
    case Finished{tokens}:
      Done{tokens}
    case Failed{message}:
      Fail{message}
    case Running{mode, source, loc, start, tokens, text, nl}:
      run(step(Running{mode, source, loc, start, tokens, text, nl}))

def lex(source: String) -> Result<&2, &2, String, List<&2, Tk.T>>:
  run(Running{Scan{}, source, Tk.start(), Tk.start(), [], "", 0n})
