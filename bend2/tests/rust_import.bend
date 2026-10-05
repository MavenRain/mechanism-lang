import Base
import ../rust/ast.bend as A
import ../rust/rir.bend as R
import ../rust/parser.bend as Parser
import ../rust/emit.bend as Em
import ../rust/lift.bend as Li
import ../rust/resolve.bend as Rs
import ../rust/infer.bend as In

# Driver: `bend bend2/tests/rust_import.bend <mode> <source text>`.
# Mode `lift`: parse, lift to the erased IR, print with the emitter. The
# text is the input text for each file that the emitter writes; a refused
# file gives one `REFUSED <line>:<col>: <construct>` line.
# Mode `resolve`: the text of src/lib.rs, then a module name and the text of
# its file for each module (`nat` too). The output is the item table, one
# `<module> <kind> <rust name> -> <mech name>` line for each item, then one
# `manifest <module> <file>` line for each module in manifest order (CD3); a
# refused crate gives one `REFUSED src/<module>.rs:<line>:<col>: ...` line.
def lifted(r: Result<&2, &2, Li.Refusal, R.Module>) -> String:
  match r:
    case Done{m}:
      Em.text(m)
    case Fail{e}:
      "REFUSED " ++ Li.show(e) ++ "\n"

def lift_report(r: Result<&2, &2, String, A.SrcFile>) -> String:
  match r:
    case Done{f}:
      lifted(Li.lift(f))
    case Fail{msg}:
      "PARSE-FAIL " ++ msg ++ "\n"

# Bool.pick is eager, so it picks a mode tag and `run_mode` matches on the
# tag: one run does only the chosen work.
def resolved(r: Result<&2, &2, Rs.Bad, Rs.Out>) -> String:
  match r:
    case Done{o}:
      Rs.table_text(o)
    case Fail{b}:
      "REFUSED " ++ Rs.show(b) ++ "\n"

# `resolve`: the arguments after the text of lib.rs are pairs of a module
# name and the text of its file.
def srcs_of(xs: List<String>) -> List<&2, Rs.Src>:
  match xs:
    case Con{n, Con{t, rest}}:
      Rs.Src{n, t} <> srcs_of(rest)
    case other:
      []

# Mode `types`: the arguments of `resolve`. The output is one
# `<module> <mech fn name> : <telescope> annotated <n>` line for each fn
# (C3), or one REFUSED line.
def typed(r: Result<&2, &2, Rs.Bad, List<&2, In.Typed>>) -> String:
  match r:
    case Done{ts}:
      In.types_text(ts)
    case Fail{b}:
      "REFUSED " ++ Rs.show(b) ++ "\n"

def inferred(r: Result<&2, &2, Rs.Bad, Rs.Out>) -> String:
  match r:
    case Done{o}:
      typed(In.infer(o))
    case Fail{b}:
      "REFUSED " ++ Rs.show(b) ++ "\n"

type Md is Data:
  MLift{}
  MResolve{}
  MTypes{}
  MNone{}

def mode_of(+mode: String) -> Md:
  Bool.pick(Md, String.eq(mode, "lift"), MLift{}, Bool.pick(Md, String.eq(mode, "resolve"), MResolve{}, Bool.pick(Md, String.eq(mode, "types"), MTypes{}, MNone{})))

def run_mode(m: Md, src: String, more: List<String>) -> String:
  match m:
    case MLift{}:
      lift_report(Parser.file(src))
    case MResolve{}:
      resolved(Rs.resolve(src, srcs_of(more)))
    case MTypes{}:
      inferred(Rs.resolve(src, srcs_of(more)))
    case MNone{}:
      "unknown mode\n"

def dispatch(+mode: String, +src: String, more: List<String>) -> String:
  run_mode(mode_of(mode), src, more)

def run_args(args: List<String>) -> String:
  match args:
    case Con{mode, Con{src, rest}}:
      dispatch(mode, src, rest)
    case other:
      "usage: rust_import.bend lift <source text> | resolve <lib.rs text> (<module> <text>)*\n"

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    IO.write(run_args(args))
