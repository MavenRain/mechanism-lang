import Base
import ../kernel/order.bend as O
import ../kernel/totality.bend as Tot
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/literal.bend as L
import ../kernel/bignum.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as Err
import ../kernel/budget.bend as Budget
import ../kernel/global.bend as Global

def literal() -> T.T:
  T.Lit{L.LInt{B.of_u32(0)}}

def nat() -> T.T:
  T.Global{"Nat"}

def lam(name:String,body:T.T) -> T.T:
  T.Sec{S.SPi{Q.QMany{},"x",nat()},[T.Leg{[C.Pair{Q.QMany{},name}],body}]}

def app(head:T.T,arg:T.T) -> T.T:
  T.Out{S.SPi{Q.QMany{},"x",nat()},T.APt{Q.QMany{},arg},head}

def elim(scrut:T.T,body:T.T,motive:Bool) -> T.T:
  T.Elim{T.ElimData{S.SMu{"Tree",[]},scrut,Q.QMany{},
    O.choose(Maybe<&2,T.Motive<T.T>>,motive,Some{T.Motive{None{},[],"self",nat()}},None{}),
    [C.Pair{T.ACtor{"leaf"},T.Leg{[],literal()}},C.Pair{T.ACtor{"node"},T.Leg{[C.Pair{Q.QMany{},"child"}],body}}]}}

def recursive(name:String,index:Nat) -> T.T:
  lam("x",elim(T.Var{0n},app(T.Global{name},T.Var{index}),True{}))

def f2() -> T.T:
  lam("x",lam("y",elim(T.Var{1n},app(app(T.Global{"g"},T.Var{0n}),T.Var{1n}),True{})))

def g2() -> T.T:
  lam("x",lam("y",elim(T.Var{0n},app(app(T.Global{"f"},T.Var{2n}),T.Var{0n}),True{})))

def certificate(r:Result<&2,&2,Err.T,Maybe<&2,O.T>>) -> String:
  match r:
    case Fail{e}: Err.to_string(e)
    case Done{None{}}: "none"
    case Done{Some{c}}: Nat.show(O.argument(c))

def guarded(r:Result<&2,&2,Err.T,Maybe<&2,Nat>>) -> String:
  match r:
    case Fail{e}: Err.to_string(e)
    case Done{None{}}: "none"
    case Done{Some{k}}: Nat.show(k)

def translated_result(r:Result<&2,&2,Err.T,T.T>) -> String:
  match r:
    case Fail{e}: Err.to_string(e)
    case Done{t}: "ok"

def translated(c:Result<&2,&2,Err.T,Maybe<&2,O.T>>,body:T.T) -> String:
  match c:
    case Fail{e}: Err.to_string(e)
    case Done{None{}}: "none"
    case Done{Some{cert}}: translated_result(O.translate(cert,body))

def report(ok:Bool,name:String,expected:String,actual:String) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS " ++ name)
    case False{}: IO.die(Unit,1,name ++ ": expected " ++ expected ++ ", observed " ++ actual)

def check(name:String,+expected:String,+actual:String) -> IO(Unit):
  report(String.eq(expected,actual),name,expected,actual)

def main() -> IO(Unit):
  do IO<Unit>:
    check("structural","0",certificate(O.certify([C.Pair{"f",recursive("f",0n)}])))
    check("nondecreasing","termination: recursive definition f failed the structural termination guard",certificate(O.certify([C.Pair{"f",recursive("f",1n)}])))
    check("bare self","termination: recursive definition f failed the structural termination guard",certificate(O.certify([C.Pair{"f",lam("x",T.Global{"f"})}])))
    check("nonrecursive","none",certificate(O.certify([C.Pair{"f",lam("x",literal())}])))
    check("mutual","0",certificate(O.certify([C.Pair{"f",recursive("g",0n)},C.Pair{"g",recursive("f",0n)}])))
    check("inconsistent positions","termination: recursive definition f failed the structural termination guard",certificate(O.certify([C.Pair{"f",f2()},C.Pair{"g",g2()}])))
    check("translation","ok",translated(O.certify([C.Pair{"f",recursive("f",0n)}]),recursive("f",0n)))
    check("missing motive","missing branch: the elimination of a structurally recursive definition carries its motive",translated(O.certify([C.Pair{"f",recursive("f",0n)}]),lam("x",elim(T.Var{0n},app(T.Global{"f"},T.Var{0n}),False{}))))
    check("budget zero","budget: the totality budget is exhausted",guarded(Tot.guard(Budget.of_polls(0n),Global.initial(),"f",nat(),literal())))
    check("budget one","none",guarded(Tot.guard(Budget.of_polls(1n),Global.initial(),"f",nat(),literal())))
    check("budget nested fail","budget: the totality budget is exhausted",guarded(Tot.guard(Budget.of_polls(3n),Global.initial(),"f",nat(),T.Let{"x",literal(),literal(),literal()})))
    check("budget nested exact","none",guarded(Tot.guard(Budget.of_polls(4n),Global.initial(),"f",nat(),T.Let{"x",literal(),literal(),literal()})))

