import Base
import ../rust/token.bend as Tk
import ../rust/lexer.bend as Lexer
import ../rust/ast.bend as A
import ../rust/parser.bend as Parser
import ../rust/print.bend as Pr
import ../rust/fragment.bend as Fr

# Driver: `bend bend2/tests/rust_frontend.bend <mode> <source text>`.
def show_tokens(ts: List<&2, Tk.T>) -> String:
  match ts:
    case Nil{}:
      ""
    case Con{t, rest}:
      Tk.show(t) ++ "\n" ++ show_tokens(rest)

def lex_report(r: Result<&2, &2, String, List<&2, Tk.T>>) -> String:
  match r:
    case Done{ts}:
      show_tokens(ts)
    case Fail{msg}:
      "LEX-FAIL " ++ msg ++ "\n"

def item_lines(ns: List<&2, A.Node>) -> String:
  match ns:
    case Nil{}:
      ""
    case Con{n, rest}:
      A.item_line(n) ++ "\n" ++ item_lines(rest)

def parse_report(r: Result<&2, &2, String, A.SrcFile>) -> String:
  match r:
    case Done{A.SrcFile{inner, items}}:
      "PARSE-OK\n" ++ item_lines(items)
    case Fail{msg}:
      "PARSE-FAIL " ++ msg ++ "\n"

# One `<line>:<col> <kind> <name>` line for each item of the file.
def pos_lines(ns: List<&2, A.Node>) -> String:
  match ns:
    case Nil{}:
      ""
    case Con{+n, rest}:
      A.show_pos(A.item_pos(n)) ++ " " ++ A.item_line(n) ++ "\n" ++ pos_lines(rest)

def pos_report(r: Result<&2, &2, String, A.SrcFile>) -> String:
  match r:
    case Done{A.SrcFile{inner, items}}:
      pos_lines(items)
    case Fail{msg}:
      "PARSE-FAIL " ++ msg ++ "\n"

def print_report(r: Result<&2, &2, String, A.SrcFile>) -> String:
  match r:
    case Done{f}:
      Pr.file(f)
    case Fail{msg}:
      "PARSE-FAIL " ++ msg ++ "\n"

def refused_lines(rs: List<&2, Fr.Refusal>) -> String:
  match rs:
    case Nil{}:
      ""
    case Con{r, rest}:
      "REFUSED " ++ Fr.show(r) ++ "\n" ++ refused_lines(rest)

# `check` is the full frontend: lex, fragment pass, parse, print. A refused
# source gives one `REFUSED <line>:<col> <construct>` line per refusal.
def check_tokens(rs: List<&2, Fr.Refusal>, ts: List<&2, Tk.T>) -> String:
  match rs:
    case Nil{}:
      print_report(Parser.from_tokens(ts))
    case Con{r, rest}:
      refused_lines(r <> rest)

def check_report(r: Result<&2, &2, String, List<&2, Tk.T>>) -> String:
  match r:
    case Done{+ts}:
      check_tokens(Fr.refusals(ts), ts)
    case Fail{msg}:
      "LEX-FAIL " ++ msg ++ "\n"

# Bool.pick is eager, so it picks a mode tag and `run_mode` matches on the
# tag: one run does only the chosen work.
type Md is Data:
  MLex{}
  MParse{}
  MPrint{}
  MCheck{}
  MPos{}
  MNone{}

def mode_of(+mode: String) -> Md:
  Bool.pick(Md, String.eq(mode, "lex"), MLex{},
    Bool.pick(Md, String.eq(mode, "parse"), MParse{},
      Bool.pick(Md, String.eq(mode, "print"), MPrint{},
        Bool.pick(Md, String.eq(mode, "check"), MCheck{}, Bool.pick(Md, String.eq(mode, "pos"), MPos{}, MNone{})))))

def run_mode(m: Md, src: String) -> String:
  match m:
    case MLex{}:
      lex_report(Lexer.lex(src))
    case MParse{}:
      parse_report(Parser.file(src))
    case MPrint{}:
      print_report(Parser.file(src))
    case MCheck{}:
      check_report(Lexer.lex(src))
    case MPos{}:
      pos_report(Parser.file(src))
    case MNone{}:
      "unknown mode\n"

def dispatch(+mode: String, +src: String) -> String:
  run_mode(mode_of(mode), src)

def run_args(args: List<String>) -> String:
  match args:
    case Con{mode, Con{src, rest}}:
      dispatch(mode, src)
    case other:
      "usage: rust_frontend.bend <lex|parse|print|check|pos> <source text>\n"

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    IO.write(run_args(args))
