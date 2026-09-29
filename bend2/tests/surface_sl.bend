import Base
import ./relational_common.bend as R
import ./relational_syntax_equal.bend as Equal
import ./surface_sl_data.bend as Data
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def refusal(-A: Data, result: Result<&2, &2, E.T, A>, expected: String, message: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{message}
    case Fail{+error}: R.require(String.eq(E.message(error), expected), E.to_string(error))

def same_tree(first: String, second: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    a : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(first))
    b : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(second))
    R.require(Equal.declarations(a, b), "sugar changed the family tree")

def same_checked(first: String, second: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    a : Elab.Rows <- R.kernel(Elab.Rows, Elab.check_text(G.initial(), B.unlimited(), first))
    b : Elab.Rows <- R.kernel(Elab.Rows, Elab.check_text(G.initial(), B.unlimited(), second))
    R.require(String.eq(Elab.checked_form(a), Elab.checked_form(b)), "match changed checked elimination")

def action(action: Data.Action) -> Result<&2, &2, String, Unit>:
  match action:
    case Data.RoundTrip{source}:
      do Result<&2, &2, String, Unit>:
        parsed : List<&2, Syntax.Decl> <- R.parse_roundtrip(source)
        Done{Unit{}}
    case Data.ParseRefusal{source, expected}:
      refusal(List<&2, Syntax.Decl>, Parser.program(source), expected, "invalid source parsed")
    case Data.SameTree{first, second}: same_tree(first, second)
    case Data.SameChecked{first, second}: same_checked(first, second)
    case Data.CheckedRefusal{source, expected}:
      refusal(Elab.Rows, Elab.check_text(G.initial(), B.unlimited(), source), expected, "invalid source elaborated")

def report(name: String, result: Result<&2, &2, String, Unit>) -> IO(Nat):
  match result:
    case Done{unit}:
      do IO<Nat>:
        printed : Unit <- IO.print("SL-SURFACE " ++ name ++ " OK")
        return 1n
    case Fail{message}:
      do IO<Nat>:
        printed : Unit <- IO.print("SL-SURFACE " ++ name ++ " FAIL: " ++ message)
        return 0n

def outcome(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("SL-SURFACE OK")
    case False{}: IO.die(Unit, 1, "SL-SURFACE FAIL")

def finish(+passed: Nat) -> IO(Unit):
  do IO<Unit>:
    printed : Unit <- IO.print("SL-SURFACE-OK " ++ Nat.show(passed) ++ "/36")
    outcome(Nat.is_eq(passed, 36n))

def run(cases: List<&2, Data.Case>, count: Nat) -> IO(Unit):
  match cases:
    case Nil{}: finish(count)
    case Con{Data.Case{name, test}, rest}:
      do IO<Unit>:
        passed : Nat <- report(name, action(test))
        run(rest, (count + passed : Nat))

def arguments(args: List<String>) -> IO(Unit):
  run(Data.cases(), 0n)

def main() -> IO(Unit):
  run(Data.cases(), 0n)
