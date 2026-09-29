import Base
import ../kernel/check.bend as Check
import ../kernel/eval.bend as Eval
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/quantity.bend as Q
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/common.bend as C
import ../kernel/error.bend as E
import ../kernel/value.bend as V

def result(value: Result<&2,&2,E.T,Unit>) -> IO(Unit):
  match value:
    case Done{_}: IO.print("linear raw check PASS")
    case Fail{e}: IO.print("linear raw check FAIL " ++ E.to_string(e))

def main() -> IO(Unit):
  +shape : S.T<T.T> = S.SPi{Q.QOne{},"x",T.Global{"Nat"}}
  +globals = Global.initial()
  body : T.T = T.Sec{shape,[T.Leg{[C.Pair{Q.QOne{},"x"}],T.Var{0n}}]}
  result(do Result<&2,&2,E.T,Unit>:
    ty : V.T <- Eval.eval(globals,[],T.Ran{shape,T.Global{"Nat"}})
    Check.check_term(globals,Budget.Unlimited{},body,ty))
