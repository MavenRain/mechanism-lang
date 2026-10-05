import Base
import ../rust/ast.bend as A
import ../rust/rir.bend as R
import ../rust/parser.bend as Parser
import ../rust/emit.bend as Em
import ../rust/lift.bend as Li
import ../rust/resolve.bend as Rs
import ../rust/infer.bend as In
import ../rust/lower.bend as Lo
import ../surface/elab.bend as Elab
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as E

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

# Modes `lower` and `check` (C4): the arguments of `resolve`. `lower` prints
# the `.mech` text of each module after a `-- file <name>` line; `check`
# joins the files in manifest order, runs the kernel check and prints
# `MECH-CHECK-OK <n> rows axioms=<k>` or `MECH-CHECK-FAIL <error>`.
# Regression gate: python3 dev/rust-lower-gate.py (optional --driver/--emitter).
def lower_if(+o: Rs.Out, r: Result<&2, &2, Rs.Bad, List<&2, In.Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>:
  match r:
    case Done{ts}:
      Done{Lo.lower(o)}
    case Fail{b}:
      Fail{b}

def lows_of(r: Result<&2, &2, Rs.Bad, Rs.Out>) -> Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>:
  match r:
    case Done{+o}:
      lower_if(o, In.infer(o))
    case Fail{b}:
      Fail{b}

def lower_report(r: Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>) -> String:
  match r:
    case Done{ls}:
      Lo.files_text(ls)
    case Fail{b}:
      "REFUSED " ++ Rs.show(b) ++ "\n"

def axiom_count(+rows: Elab.Rows) -> String:
  Nat.show(List.length(&2, String, Elab.axiom_names(rows)))

# One mark for each row, to count the rows.
def row_marks(rows: Elab.Rows) -> List<&2, String>:
  match rows:
    case Nil{}:
      []
    case Con{r, rest}:
      "r" <> row_marks(rest)

def check_rows(r: Result<&2, &2, E.T, Elab.Rows>) -> String:
  match r:
    case Done{+rows}:
      "MECH-CHECK-OK " ++ Nat.show(List.length(&2, String, row_marks(rows))) ++ " rows axioms=" ++ axiom_count(rows) ++ "\n"
    case Fail{e}:
      "MECH-CHECK-FAIL " ++ E.message(e) ++ "\n"

def check_report(r: Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>) -> String:
  match r:
    case Done{ls}:
      check_rows(Elab.check_text(Global.initial(), Budget.unlimited(), Lo.source(ls)))
    case Fail{b}:
      "REFUSED " ++ Rs.show(b) ++ "\n"

type Md is Data:
  MLift{}
  MResolve{}
  MTypes{}
  MLower{}
  MCheck{}
  MNone{}

def mode_of(+mode: String) -> Md:
  Bool.pick(Md, String.eq(mode, "lift"), MLift{}, Bool.pick(Md, String.eq(mode, "resolve"), MResolve{}, Bool.pick(Md, String.eq(mode, "types"), MTypes{}, Bool.pick(Md, String.eq(mode, "lower"), MLower{}, Bool.pick(Md, String.eq(mode, "check"), MCheck{}, MNone{})))))

def run_mode(m: Md, src: String, more: List<String>) -> String:
  match m:
    case MLift{}:
      lift_report(Parser.file(src))
    case MResolve{}:
      resolved(Rs.resolve(src, srcs_of(more)))
    case MTypes{}:
      inferred(Rs.resolve(src, srcs_of(more)))
    case MLower{}:
      lower_report(lows_of(Rs.resolve(src, srcs_of(more))))
    case MCheck{}:
      check_report(lows_of(Rs.resolve(src, srcs_of(more))))
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
