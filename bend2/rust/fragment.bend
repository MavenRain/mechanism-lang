import Base
import ./token.bend as Tk

# Fragment pass: a walk over the lexer tokens that runs before the parser.
# It refuses each construct outside the house-convention Rust subset and gives
# the position and the construct name. See FRONTEND.md for the rule table.
type Refusal is Data:
  Refusal{loc: Tk.Loc, what: String}

def show(r: Refusal) -> String:
  match r:
    case Refusal{loc, what}:
      Tk.show_loc(loc) ++ " " ++ what

# Token classes. `CKw` is a keyword that cannot end an expression. `CWord` is
# any other identifier (`self`, `true` and raw identifiers included).
# `CClose` is `)`, `]` or `?`.
type C is Data:
  CLoop{}
  CWhile{}
  CFor{}
  CMut{}
  CUnsafe{}
  CAs{}
  CDyn{}
  CAsync{}
  CAwait{}
  CImpl{}
  CUse{}
  CKw{}
  CWord{}
  CLife{}
  CCmt{}
  CLit{}
  CAmp{}
  CAmpAmp{}
  CDot{}
  CBang{}
  CLBrack{}
  CClose{}
  CLParen{}
  CPathSep{}
  CLBrace{}
  CSemi{}
  COther{}

type CT is Data:
  CT{c: C, text: String, loc: Tk.Loc}

type Entry is Data:
  Entry{text: String, c: C}

def keywords() -> List<&2, Entry>:
  Entry{"loop", CLoop{}} <> Entry{"while", CWhile{}} <> Entry{"for", CFor{}} <> Entry{"mut", CMut{}} <>
  Entry{"unsafe", CUnsafe{}} <> Entry{"as", CAs{}} <> Entry{"dyn", CDyn{}} <> Entry{"async", CAsync{}} <>
  Entry{"await", CAwait{}} <> Entry{"impl", CImpl{}} <> Entry{"use", CUse{}} <> Entry{"break", CKw{}} <>
  Entry{"const", CKw{}} <> Entry{"continue", CKw{}} <> Entry{"crate", CKw{}} <> Entry{"else", CKw{}} <>
  Entry{"enum", CKw{}} <> Entry{"extern", CKw{}} <> Entry{"fn", CKw{}} <> Entry{"if", CKw{}} <>
  Entry{"in", CKw{}} <> Entry{"let", CKw{}} <> Entry{"match", CKw{}} <> Entry{"mod", CKw{}} <>
  Entry{"move", CKw{}} <> Entry{"pub", CKw{}} <> Entry{"ref", CKw{}} <> Entry{"return", CKw{}} <>
  Entry{"static", CKw{}} <> Entry{"struct", CKw{}} <> Entry{"super", CKw{}} <> Entry{"trait", CKw{}} <>
  Entry{"type", CKw{}} <> Entry{"where", CKw{}} <> Entry{"Self", CKw{}} <> []

def puncts() -> List<&2, Entry>:
  Entry{"&", CAmp{}} <> Entry{"&&", CAmpAmp{}} <> Entry{".", CDot{}} <> Entry{"!", CBang{}} <>
  Entry{"[", CLBrack{}} <> Entry{")", CClose{}} <> Entry{"]", CClose{}} <> Entry{"?", CClose{}} <>
  Entry{"(", CLParen{}} <> Entry{"::", CPathSep{}} <> Entry{"{", CLBrace{}} <> Entry{";", CSemi{}} <> []

def lookup(+text: String, table: List<&2, Entry>, fallback: C) -> C:
  match table:
    case Nil{}:
      fallback
    case Con{Entry{s, c}, rest}:
      Bool.pick(C, String.eq(text, s), c, lookup(text, rest, fallback))

def classify_kind(kind: Tk.KindT, loc: Tk.Loc) -> CT:
  match kind:
    case Tk.Ident{+name}:
      CT{lookup(name, keywords(), CWord{}), name, loc}
    case Tk.RawIdent{name}:
      CT{CWord{}, name, loc}
    case Tk.Lifetime{name}:
      CT{CLife{}, name, loc}
    case Tk.Int{text}:
      CT{CLit{}, text, loc}
    case Tk.Str{text}:
      CT{CLit{}, text, loc}
    case Tk.ChrLit{text}:
      CT{CLit{}, text, loc}
    case Tk.DocOuter{text}:
      CT{COther{}, text, loc}
    case Tk.DocInner{text}:
      CT{COther{}, text, loc}
    case Tk.Comment{text}:
      CT{CCmt{}, text, loc}
    case Tk.Punct{+text}:
      CT{lookup(text, puncts(), COther{}), text, loc}
    case Tk.Eof{}:
      CT{COther{}, "", loc}

def classify(ts: List<&2, Tk.T>) -> List<&2, CT>:
  match ts:
    case Nil{}:
      []
    case Con{Tk.Tok{kind, loc, gap}, rest}:
      classify_kind(kind, loc) <> classify(rest)

# True when a token of this class can end an expression. A `[` that follows
# such a token is an index operation.
def ender(c: C) -> Bool:
  match c:
    case CWord{}:
      True{}
    case CLit{}:
      True{}
    case CClose{}:
      True{}
    case other:
      False{}

# `hdr` is True from `impl` to the next `{` or `;`. There, `for` is the
# `impl Trait for Type` keyword and not a loop.
def hdr_next(c: C, hdr: Bool) -> Bool:
  match c:
    case CImpl{}:
      True{}
    case CLBrace{}:
      False{}
    case CSemi{}:
      False{}
    case other:
      hdr

# `usei` is True from `use` to the next `;`. There, `as` is a rename and not
# a cast.
def use_next(c: C, usei: Bool) -> Bool:
  match c:
    case CUse{}:
      True{}
    case CSemi{}:
      False{}
    case other:
      usei

# The refusal for one token, from its class and the walk state.
def one(c: C, l: Tk.Loc, e: Bool, hdr: Bool, usei: Bool) -> Maybe<&2, Refusal>:
  match c:
    case CLoop{}:
      Some{Refusal{l, "loop"}}
    case CWhile{}:
      Some{Refusal{l, "while"}}
    case CFor{}:
      Bool.pick(Maybe<&2, Refusal>, hdr, None{}, Some{Refusal{l, "for"}})
    case CMut{}:
      Some{Refusal{l, "mut binding"}}
    case CUnsafe{}:
      Some{Refusal{l, "unsafe"}}
    case CAs{}:
      Bool.pick(Maybe<&2, Refusal>, usei, None{}, Some{Refusal{l, "as"}})
    case CDyn{}:
      Some{Refusal{l, "dyn Trait"}}
    case CAsync{}:
      Some{Refusal{l, "async"}}
    case CAwait{}:
      Some{Refusal{l, ".await"}}
    case CLife{}:
      Some{Refusal{l, "lifetime"}}
    case CCmt{}:
      Some{Refusal{l, "comment"}}
    case CLBrack{}:
      Bool.pick(Maybe<&2, Refusal>, e, Some{Refusal{l, "indexing"}}, None{})
    case other:
      None{}

# The construct name for a refused method call `.name(`.
def method_what(+name: String) -> Maybe<&2, String>:
  Bool.pick(Maybe<&2, String>, String.eq(name, "unwrap"), Some{".unwrap()"},
    Bool.pick(Maybe<&2, String>, String.eq(name, "expect"), Some{".expect("},
      Bool.pick(Maybe<&2, String>, String.eq(name, "scan"), Some{".scan("}, None{})))

# The construct name for a macro call `name!`. Only `vec!` is permitted.
def macro_what(+name: String) -> Maybe<&2, String>:
  Bool.pick(Maybe<&2, String>, String.eq(name, "vec"), None{},
    Bool.pick(Maybe<&2, String>,
      String.eq(name, "panic") || String.eq(name, "assert") || String.eq(name, "assert_eq"),
      Some{name ++ "!"}, Some{"macro " ++ name ++ "!"}))

def at(l: Tk.Loc, m: Maybe<&2, String>) -> Maybe<&2, Refusal>:
  match m:
    case None{}:
      None{}
    case Some{what}:
      Some{Refusal{l, what}}

def push(m: Maybe<&2, Refusal>, tail: List<&2, Refusal>) -> List<&2, Refusal>:
  match m:
    case None{}:
      tail
    case Some{r}:
      r <> tail

# `e` is True when the previous token can end an expression. The cases with
# two or three tokens come first; each one consumes all of its tokens, so one
# construct gives one refusal.
def walk(ts: List<&2, CT>, e: Bool, +hdr: Bool, +usei: Bool) -> List<&2, Refusal>:
  match ts:
    case Nil{}:
      []
    case Con{CT{CAmp{}, a, l}, Con{CT{CLife{}, b, m}, Con{CT{CMut{}, c, n}, rest}}}:
      Refusal{l, "&mut"} <> Refusal{m, "lifetime"} <> walk(rest, False{}, hdr, usei)
    case Con{CT{CAmp{}, a, l}, Con{CT{CMut{}, b, m}, rest}}:
      Refusal{l, "&mut"} <> walk(rest, False{}, hdr, usei)
    case Con{CT{CAmpAmp{}, a, l}, Con{CT{CMut{}, b, m}, rest}}:
      Refusal{Tk.advance(l, 1n), "&mut"} <> walk(rest, False{}, hdr, usei)
    case Con{CT{CDot{}, a, l}, Con{CT{CAwait{}, b, m}, rest}}:
      Refusal{l, ".await"} <> walk(rest, True{}, hdr, usei)
    case Con{CT{CDot{}, a, l}, Con{CT{CWord{}, name, m}, Con{CT{CLParen{}, c, n}, rest}}}:
      push(at(l, method_what(name)), walk(rest, False{}, hdr, usei))
    case Con{CT{CDot{}, a, l}, Con{CT{CWord{}, name, m}, Con{CT{CPathSep{}, c, n}, rest}}}:
      push(at(l, method_what(name)), walk(rest, False{}, hdr, usei))
    case Con{CT{CWord{}, name, l}, Con{CT{CBang{}, b, m}, rest}}:
      push(at(l, macro_what(name)), walk(rest, False{}, hdr, usei))
    case Con{CT{+c, a, l}, rest}:
      push(one(c, l, e, hdr, usei), walk(rest, ender(c), hdr_next(c, hdr), use_next(c, usei)))

# All refusals for a token list, in source order. An empty list means that
# the source is in the fragment.
def refusals(ts: List<&2, Tk.T>) -> List<&2, Refusal>:
  walk(classify(ts), False{}, False{}, False{})
