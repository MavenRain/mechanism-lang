import Base

# One-based source positions, as in bend2/surface/token.bend.
type Loc is Data:
  Loc{line: Nat, col: Nat}

# Keywords stay `Ident`; the parser compares names. `Comment` is a non-doc
# comment, kept so the fragment pass can refuse it at its position.
type KindT is Data:
  Ident{name: String}
  RawIdent{name: String}
  Lifetime{name: String}
  Int{text: String}
  Str{text: String}
  ChrLit{text: String}
  DocOuter{text: String}
  DocInner{text: String}
  Comment{text: String}
  Punct{text: String}
  Eof{}

# `gap` is True when one or more blank lines precede the token.
type T is Data:
  Tok{kind: KindT, loc: Loc, gap: Bool}

def start() -> Loc:
  Loc{1n, 1n}

def advance(l: Loc, n: Nat) -> Loc:
  match l:
    case Loc{line, col}:
      Loc{line, Nat.add(col, n)}

def next_line(l: Loc) -> Loc:
  match l:
    case Loc{line, col}:
      Loc{1n+line, 1n}

def show_loc(l: Loc) -> String:
  match l:
    case Loc{line, col}:
      Nat.show(line) ++ ":" ++ Nat.show(col)

def show_kind(k: KindT) -> String:
  match k:
    case Ident{name}:
      "ident " ++ name
    case RawIdent{name}:
      "raw " ++ name
    case Lifetime{name}:
      "lifetime " ++ name
    case Int{text}:
      "int " ++ text
    case Str{text}:
      "str " ++ text
    case ChrLit{text}:
      "char " ++ text
    case DocOuter{text}:
      "doc " ++ text
    case DocInner{text}:
      "innerdoc " ++ text
    case Comment{text}:
      "comment"
    case Punct{text}:
      "punct " ++ text
    case Eof{}:
      "eof"

def show_gap(g: Bool) -> String:
  match g:
    case True{}:
      " +gap"
    case False{}:
      ""

def show(t: T) -> String:
  match t:
    case Tok{kind, loc, gap}:
      show_loc(loc) ++ " " ++ show_kind(kind) ++ show_gap(gap)
