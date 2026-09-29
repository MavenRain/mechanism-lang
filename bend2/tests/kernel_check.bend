import Base
import ../kernel/check.bend as Check
import ../kernel/common.bend as Pair
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/prim.bend as Prim
import ../kernel/bignum.bend as Big
import ../kernel/literal.bend as Lit
import ../kernel/eval.bend as Eval
import ../kernel/error.bend as Error

def success(result: Result<&2, &2, Error.T, Unit>) -> Bool:
  match result:
    case Done{Unit{}}:
      True{}
    case Fail{error}:
      False{}

def quantity_error(result: Result<&2, &2, Error.T, Unit>) -> Bool:
  match result:
    case Fail{Error.Quantity{message}}:
      True{}
    case _:
      False{}

def universe_error(result: Result<&2, &2, Error.T, Unit>) -> Bool:
  match result:
    case Fail{Error.Universe{message}}:
      True{}
    case _:
      False{}

def check_typed(ty: Result<&2, &2, Error.T, Value.T>, +ctx: Check.Ctx) -> Bool:
  match ty:
    case Fail{error}:
      False{}
    case Done{+expected}:
      Bool.and(success(Check.check(ctx, Q.QMany{}, Term.Sec{Shape.SPi{Q.QOne{}, "x", Prim.nat_ty()}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}], Term.Var{0n}}]}, expected)), quantity_error(Check.check(ctx, Q.QMany{}, Term.Sec{Shape.SPi{Q.QOne{}, "x", Prim.nat_ty()}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}], Term.Lit{Lit.LInt{Big.zero()}}}]}, expected)))

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS checker linearity and raw universe scope")
    case False{}:
      IO.die(Unit, 1, "FAIL checker linearity and raw universe scope")

def main() -> IO(Unit):
  +globals = Global.initial()
  +ctx = Check.make(globals, Budget.unlimited(), 0n)
  verify(Bool.and(check_typed(Eval.eval(globals, [], Prim.arrow(Q.QOne{}, "x", Prim.nat_ty(), Prim.nat_ty())), ctx), universe_error(Check.check(ctx, Q.QMany{}, Term.Sec{Shape.SPi{Q.QOne{}, "x", Term.Univ{Level.var(0n)}}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}], Term.Var{0n}}]}, Value.VUniv{Level.one()}))))
