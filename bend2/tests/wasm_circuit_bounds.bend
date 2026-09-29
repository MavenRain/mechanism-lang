import Base
import ../kernel/circuit.bend as C
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/literal.bend as L
import ../kernel/bignum.bend as B
import ../kernel/common.bend as P
import ../cli/output.bend as Out

def shape() -> S.T<T.T>:
  S.SMu{"Bound",[]}

def ctor(args:List<&2,T.T>) -> T.T:
  T.In{shape(),T.ACtor{"node"},args}

def leaf() -> T.T:
  ctor([])

def three() -> T.T:
  ctor([ctor([leaf()])])

def zero() -> T.T:
  T.Lit{L.LInt{B.zero()}}

def other(args:List<&2,T.T>) -> T.T:
  T.In{S.SMu{"Other",[]},T.ACtor{"o"},args}

def pair(a:T.T,b:T.T) -> T.T:
  T.In{S.coll(T.T,2n),T.aleg(T.T,0n),[a,b]}

def globals() -> List<&2,P.Pair<String,T.T>>:
  [P.Pair{"alias",T.Global{"tree"}},P.Pair{"tree",three()},
   P.Pair{"cycleA",T.Global{"cycleB"}},P.Pair{"cycleB",ctor([T.Global{"cycleA"}])}]

def branch(scrut:T.T,body:T.T) -> T.T:
  T.Elim{T.ElimData{shape(),scrut,Q.QMany{},None{},
    [P.Pair{T.ACtor{"node"},T.Leg{[],body}}]}}

def fold(scrut:T.T,body:T.T) -> T.T:
  T.Elim{T.ElimData{shape(),scrut,Q.QMany{},None{},
    [P.Pair{T.ACtor{"node"},T.Leg{[P.Pair{Q.QMany{},"x"}],pair(T.Var{0n},body)}}]}}

def scaled(n:Nat) -> T.T:
  match n:
    case 0n: T.Global{"natMul"}
    case 1n+p: fold(three(),scaled(p))

def scaled_once(n:Nat) -> T.T:
  match n:
    case 0n: T.Global{"natMul"}
    case 1n+p: branch(three(),scaled_once(p))

def apply(head:T.T,arg:T.T) -> T.T:
  T.Out{S.SPi{Q.QMany{},"x",T.Global{"Nat"}},T.APt{Q.QMany{},arg},head}

def increment(body:T.T) -> T.T:
  apply(apply(T.Global{"natMul"},body),zero())

def near_limit(n:Nat) -> T.T:
  match n:
    case 0n: T.Global{"natMul"}
    case 1n+p: increment(fold(ctor([leaf()]),near_limit(p)))

# Preserve the source oracle's 63-bit machine-integer depth bound.
def limit() -> T.T:
  near_limit(61n)

def case_limit() -> T.T:
  T.Elim{T.ElimData{S.coll(T.T,1n),zero(),Q.QMany{},None{},
    [P.Pair{T.aleg(T.T,0n),T.Leg{[],limit()}}]}}

type Case is Data:
  Case{name:String,term:T.T,expected:Result<&2,&2,String,B.T>}

def good(n:U32) -> Result<&2,&2,String,B.T>:
  Done{B.of_u32(n)}

def cases() -> List<&2,Case>:
  [Case{"alias",fold(T.Global{"alias"},T.Global{"natMul"}),good(3)},
   Case{"alias-once",branch(T.Global{"alias"},T.Global{"natMul"}),good(1)},
   Case{"leaf-work",branch(leaf(),T.Global{"natMul"}),good(1)},
   Case{"literal-field",fold(ctor([zero()]),T.Global{"natMul"}),good(1)},
   Case{"literal-scrutinee",branch(zero(),T.Global{"natMul"}),Fail{"unbounded iteration"}},
   Case{"foreign-payload",fold(ctor([other([other([other([])])])]),T.Global{"natMul"}),good(1)},
   Case{"opaque-sibling",branch(ctor([leaf(),T.Var{0n}]),zero()),Fail{"unbounded iteration"}},
   Case{"coll-field-value",ctor([pair(zero(),zero())]),good(0)},
   Case{"recursive-field-uncertified",ctor([ctor([pair(zero(),zero())])]),Fail{"mu"}},
   Case{"unknown-field-global",ctor([T.Global{"nosuch"}]),Fail{"unknown global nosuch"}},
   Case{"cycle-bound",branch(T.Global{"cycleA"},T.Global{"natMul"}),Fail{"unbounded iteration"}},
   Case{"cycle-value",T.Global{"cycleA"},Fail{"mu"}},
   Case{"representable-depth",scaled(10n),good(59049)},
   Case{"no-fold-depth",scaled_once(10n),good(1)},
   Case{"overflow",scaled(63n),Fail{"unbounded iteration"}},
   Case{"integer-limit",limit(),Done{B.Big{False{},[32767,32767,32767,32767,3]}}},
   Case{"primitive-overflow",increment(limit()),Fail{"unbounded iteration"}},
   Case{"case-overflow",case_limit(),Fail{"unbounded iteration"}}]

def same(a:Result<&2,&2,String,B.T>,b:Result<&2,&2,String,B.T>) -> Bool:
  match a b:
    case Done{x} Done{y}: String.eq(B.to_string(x),B.to_string(y))
    case Fail{x} Fail{y}: String.eq(x,y)
    case _ _: False{}

def report(ok:Bool,name:String) -> IO(Nat):
  match ok:
    case True{}: do IO<Nat>:
      IO.print("PASS " ++ name)
      return 1n
    case False{}: do IO<Nat>:
      IO.print("FAIL " ++ name)
      return 0n

def finish(ok:Bool) -> IO(Unit):
  match ok:
    case True{}: IO.pure(Unit,Unit{})
    case False{}: Out.cli_exit(1)

def run(rows:List<&2,Case>,+passed:Nat,+total:Nat) -> IO(Unit):
  match rows:
    case Nil{}: do IO<Unit>:
      IO.print("CIRCUIT-BOUNDS " ++ Nat.show(passed) ++ "/" ++ Nat.show(total))
      finish(Nat.is_eq(passed,18n) && Nat.is_eq(total,18n))
    case Con{Case{name,term,expected},rest}: do IO<Unit>:
      count : Nat <- report(same(C.depth(globals(),term),expected),name)
      run(rest,Nat.add(passed,count),Nat.add(total,1n))

def arguments(args:List<String>) -> IO(Unit):
  run(cases(),0n,0n)

def main() -> IO(Unit):
  arguments([])
