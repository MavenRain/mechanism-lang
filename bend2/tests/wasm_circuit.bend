import Base
import ../kernel/circuit.bend as C
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/literal.bend as L
import ../kernel/bignum.bend as B
import ../kernel/common.bend as P
import ../kernel/level_var.bend as U

def nat() -> T.T:
  T.Global{"Nat"}

def lit() -> T.T:
  T.Lit{L.LInt{B.of_u32(7)}}

def app(head:T.T,arg:T.T) -> T.T:
  T.Out{S.SPi{Q.QMany{},"x",nat()},T.APt{Q.QMany{},arg},head}

def add(a:T.T,b:T.T) -> T.T:
  app(app(T.Global{"natAdd"},a),b)

def identity() -> T.T:
  T.Sec{S.SPi{Q.QMany{},"x",nat()},[T.Leg{[P.Pair{Q.QMany{},"x"}],T.Var{0n}}]}

def leaf() -> T.T:
  T.In{S.SMu{"Tree",[]},T.ACtor{"leaf"},[]}

def tree() -> T.T:
  T.In{S.SMu{"Tree",[]},T.ACtor{"node"},[leaf()]}

def fold_tree() -> T.T:
  T.Elim{T.ElimData{S.SMu{"Tree",[]},tree(),Q.QMany{},None{},[
    P.Pair{T.ACtor{"leaf"},T.Leg{[],lit()}},
    P.Pair{T.ACtor{"node"},T.Leg{[P.Pair{Q.QMany{},"child"}],add(T.Var{0n},lit())}}]}}

def result(r:Result<&2,&2,String,B.T>) -> String:
  match r:
    case Done{n}: B.to_string(n)
    case Fail{e}: e

def report(ok:Bool,name:String,expected:String,actual:String) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS " ++ name)
    case False{}: IO.die(Unit,1,name ++ ": expected " ++ expected ++ ", observed " ++ actual)

def check(name:String, +expected:String, +actual:String) -> IO(Unit):
  report(String.eq(expected,actual),name,expected,actual)

def main() -> IO(Unit):
  do IO<Unit>:
    check("primitive","1",result(C.depth([],add(lit(),lit()))))
    check("nested","2",result(C.depth([],add(add(lit(),lit()),lit()))))
    check("closure","2",result(C.depth([],app(identity(),add(add(lit(),lit()),lit())))))
    check("unapplied","0",result(C.depth([],identity())))
    check("fold","2",result(C.depth([],fold_tree())))
    check("unknown","unknown global missing",result(C.depth([],T.Global{"missing"})))
    check("cycle","mu",result(C.depth([P.Pair{"loop",T.Global{"loop"}}],T.Global{"loop"})))
    check("shape","SPar",result(C.depth([],T.Lan{S.SPar{nat(),nat()},nat()})))
    check("opaque","opaque callee",result(C.depth([],app(lit(),lit()))))
    check("type","0",result(C.depth([],T.Univ{U.LZero{}})))
    check("captured cycle","mu",result(C.depth([P.Pair{"f",T.Sec{S.SPi{Q.QMany{},"x",nat()},[T.Leg{[P.Pair{Q.QMany{},"x"}],T.Global{"f"}}]}}],T.Global{"f"})))
    check("ordinary field","0",result(C.depth([],T.Let{"x",nat(),lit(),T.In{S.SMu{"Tree",[]},T.ACtor{"node"},[T.Var{0n}]}})))
    check("uncertified recursive field","mu",result(C.depth([],T.In{S.SMu{"Tree",[]},T.ACtor{"node"},[T.Lan{S.SMu{"Tree",[]},nat()}]})))
    check("overflow","unbounded iteration",result(C.bounded(B.Big{False{},[0,0,0,0,4]})))
