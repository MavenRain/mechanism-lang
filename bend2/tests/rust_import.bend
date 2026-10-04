import Base
import ../rust/ast.bend as A
import ../rust/rir.bend as R
import ../rust/parser.bend as Parser
import ../rust/emit.bend as Em
import ../rust/lift.bend as Li

# Driver: `bend bend2/tests/rust_import.bend <mode> <source text>`.
# Mode `lift`: parse, lift to the erased IR, print with the emitter. The
# text is the input text for each file that the emitter writes; a refused
# file gives one `REFUSED <line>:<col>: <construct>` line.
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
type Md is Data:
  MLift{}
  MNone{}

def mode_of(+mode: String) -> Md:
  Bool.pick(Md, String.eq(mode, "lift"), MLift{}, MNone{})

def run_mode(m: Md, src: String) -> String:
  match m:
    case MLift{}:
      lift_report(Parser.file(src))
    case MNone{}:
      "unknown mode\n"

def dispatch(+mode: String, +src: String) -> String:
  run_mode(mode_of(mode), src)

def run_args(args: List<String>) -> String:
  match args:
    case Con{mode, Con{src, rest}}:
      dispatch(mode, src)
    case other:
      "usage: rust_import.bend lift <source text>\n"

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    IO.write(run_args(args))
