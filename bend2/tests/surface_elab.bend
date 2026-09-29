import Base
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/check.bend as Check
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/quantity.bend as Quantity
import ../kernel/eval.bend as Eval

def elaborate(source: String) -> Result<&2, &2, Error.T, Term.T>:
  +ctx = Check.make(Global.initial(), Budget.unlimited(), 0n)
  do Result<&2, &2, Error.T, Term.T>:
    syntax : Syntax.T <- Parser.term(source)
    +term : Term.T <- Elab.elab(ctx, None{}, syntax)
    ty : Value.T <- Check.infer(ctx, Quantity.QMany{}, term)
    return term

def accept_result(result: Result<&2, &2, Error.T, Term.T>, source: String) -> IO(Unit):
  match result:
    case Done{term}:
      IO.pure(Unit, Unit{})
    case Fail{error}:
      IO.die(Unit, 1, source ++ ": " ++ Error.to_string(error))

def accepts(+source: String) -> IO(Unit):
  accept_result(elaborate(source), source)

def reject_result(result: Result<&2, &2, Error.T, Term.T>, source: String) -> IO(Unit):
  match result:
    case Fail{error}:
      IO.pure(Unit, Unit{})
    case Done{term}:
      IO.die(Unit, 1, "unexpected acceptance: " ++ source)

def rejects(+source: String) -> IO(Unit):
  reject_result(elaborate(source), source)

def main() -> IO(Unit):
  do IO<Unit>:
    accepts("Type")
    accepts("1234567890123456789012345678901234567890")
    accepts("(fun (0 A : Type) (1 x : A) => x : (0 A : Type) -> (1 x : A) -> A)")
    accepts("let x : Nat := 7 in x")
    accepts("((1, 2) : Nat * Nat).1")
    accepts("((1, 2) : Nat * Nat).2")
    accepts("Nat * Nat")
    rejects("unknown")
    rejects("(fun (1 x : Nat) => 7 : (1 x : Nat) -> Nat)")
    IO.print("PASS surface elaboration and trusted checking")
