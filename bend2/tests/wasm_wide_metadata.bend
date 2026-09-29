import Base
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/shape.bend as S
import ../kernel/bignum.bend as Big
import ../kernel/literal.bend as Lit
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/global.bend as G
import ../kernel/common.bend as P
import ../kernel/rules.bend as Rules
import ../kernel/prim.bend as Prim
import ../kernel/budget.bend as B
import ../kernel/check.bend as Check
import ../kernel/eval.bend as Eval
import ../kernel/error.bend as Error
import ../kernel/erase.bend as Erase
import ../kernel/erase_repr.bend as Repr
import ../kernel/circuit.bend as Circuit
import ../wasm/emit.bend as Emit

# Raw metadata probes mirror build/large-index-audit/core.ml from the original oracle.
def huge() -> Big.T:
  Big.Big{False{},[0,0,0,8]}

def negative() -> Big.T:
  Big.Big{True{},[1]}

def zero() -> T.T:
  T.Lit{Lit.LInt{Big.zero()}}

def injection(width:Big.T,index:Big.T) -> T.T:
  T.In{S.SColl{width},T.ALeg{index},[zero()]}

def projection(width:Big.T,index:Big.T) -> T.T:
  T.Out{S.SColl{width},T.ALeg{index},T.Ann{T.Sec{S.coll(T.T,1n),[T.Leg{[],zero()}]},Rules.prod_ty([Prim.nat_ty()])}}

def require(ok:Bool,message:String) -> Result<&2,&2,String,Unit>:
  match ok:
    case True{}: Done{Unit{}}
    case False{}: Fail{message}

def kernel(-A:Data,value:Result<&2,&2,Error.T,A>) -> Result<&2,&2,String,A>:
  match value:
    case Done{x}: Done{x}
    case Fail{error}: Fail{Error.to_string(error)}

def accepted_in(width:Big.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    expected : V.T <- kernel(V.T,Eval.eval(G.initial(),[],Rules.sum_ty([Prim.nat_ty()])))
    kernel(Unit,Check.check(Check.make(G.initial(),B.unlimited(),0n),Q.QMany{},injection(width,Big.zero()),expected))

def accepted_out(width:Big.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    ty : V.T <- kernel(V.T,Check.infer_term(G.initial(),B.unlimited(),projection(width,Big.zero())))
    return Unit{}

def is_zero(value:V.T) -> Bool:
  match value:
    case V.VLit{Lit.LInt{n}}: Big.equal(n,Big.zero())
    case _: False{}

def evaluated_out(width:Big.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    value : V.T <- kernel(V.T,Eval.eval(G.initial(),[],projection(width,Big.zero())))
    require(is_zero(value),"projection did not evaluate to zero")

def wrong_leg(-A:Data,value:Result<&2,&2,Error.T,A>) -> Result<&2,&2,String,Unit>:
  match value:
    case Fail{Error.Wrong_leg{message}}: Done{Unit{}}
    case Fail{error}: Fail{"wrong refusal: " ++ Error.to_string(error)}
    case Done{x}: Fail{"out-of-range leg was accepted"}

def rejected_in(index:Big.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    expected : V.T <- kernel(V.T,Eval.eval(G.initial(),[],Rules.sum_ty([Prim.nat_ty()])))
    wrong_leg(Unit,Check.check(Check.make(G.initial(),B.unlimited(),0n),Q.QMany{},injection(Big.one(),index),expected))

def rejected_out(index:Big.T) -> Result<&2,&2,String,Unit>:
  wrong_leg(V.T,Check.infer_term(G.initial(),B.unlimited(),projection(Big.one(),index)))

def entry(ty:T.T,body:T.T) -> G.Entry:
  G.Def{G.DefEntry{ty,body,True{},None{},False{}}}

def rows(width:Big.T) -> Result<&2,&2,String,List<&2,P.Pair<String,Erase.Entry>>>:
  tag={P.Pair{"tag",entry(Rules.sum_ty([Prim.nat_ty()]),injection(width,Big.zero()))}:P.Pair<String,G.Entry>}
  body={T.Elim{T.ElimData{S.coll(T.T,1n),T.Global{"tag"},Q.QMany{},None{},[P.Pair{T.aleg(T.T,0n),T.Leg{[P.Pair{Q.QMany{},"value"}],T.Var{0n}}}]}}:T.T}
  kernel(List<&2,P.Pair<String,Erase.Entry>>,Erase.program(G.initial(),[tag,P.Pair{"main",entry(Prim.nat_ty(),body)}]))

def bytes_same(a:List<&2,U32>,b:List<&2,U32>) -> Bool:
  match a b:
    case Nil{} Nil{}: True{}
    case Con{x,xs} Con{y,ys}: Bool.and(U32.is_eq(x,y),bytes_same(xs,ys))
    case _ _: False{}

def emitted_in(width:Big.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    +wide : List<&2,P.Pair<String,Erase.Entry>> <- rows(width)
    +small : List<&2,P.Pair<String,Erase.Entry>> <- rows(Big.one())
    require(String.eq(Erase.print(wide),Erase.print(small)),"ignored injection metadata changed erasure")
    actual : List<&2,U32> <- kernel(List<&2,U32>,Emit.program(wide,"main"))
    expected : List<&2,U32> <- kernel(List<&2,U32>,Emit.program(small,"main"))
    require(bytes_same(actual,expected),"ignored injection metadata changed Wasm bytes")

def erased_out(width:Big.T) -> Result<&2,&2,String,Unit>:
  wrong_leg(Erase.Entry,Erase.entry(G.initial(),"main",entry(Prim.nat_ty(),projection(width,Big.zero()))))

def diagram() -> V.Closure<V.T>:
  V.Closure{[],T.Sec{S.coll(T.T,1n),[T.Leg{[],Prim.nat_ty()}]}}

def ec() -> Repr.Ctx:
  Repr.Ctx{Check.make(G.initial(),B.unlimited(),0n),[],"wide"}

def width_refused(width:Big.T) -> Result<&2,&2,String,Unit>:
  wrong_leg(List<&2,V.T>,B.run(List<&2,V.T>,Repr.leg_types(ec(),width,diagram()),B.unlimited()))

def finite_diagram() -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    types : List<&2,V.T> <- kernel(List<&2,V.T>,B.run(List<&2,V.T>,Repr.leg_types(ec(),Big.one(),diagram()),B.unlimited()))
    require(Nat.is_eq(List.length(&2,V.T,types),1n),"finite diagram lost a leg")

def circuit_zero(term:T.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    depth : Big.T <- Circuit.depth([],term)
    require(Big.equal(depth,Big.zero()),"ignored metadata changed circuit depth")

def flags() -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    require(String.eq(Big.to_string(huge()),"281474976710656"),"wide fixture no longer exceeds the Nat payload limit")
    require(String.eq(Big.to_string(negative()),"-1"),"signed fixture lost its sign")
    require(Nat.is_eq(Erase.flags_before([True{},False{},True{}],huge()),2n),"large index wrapped in projection offset")
    require(Nat.is_eq(Erase.flags_before([True{},False{},True{}],negative()),0n),"negative index became a runtime offset")
    require(Nat.is_eq(Erase.flags_before([True{},False{},True{}],Big.one()),1n),"finite projection offset changed")

def report(+name:String,value:Result<&2,&2,String,Unit>) -> IO(Nat):
  match value:
    case Done{x}: do IO<Nat>:
      IO.print("PASS wide-backend " ++ name)
      return 0n
    case Fail{message}: do IO<Nat>:
      IO.print("FAIL wide-backend " ++ name ++ ": " ++ message)
      return 1n

def finish(failed:Nat) -> IO(Unit):
  match failed:
    case Zero{}: IO.print("WIDE-BACKEND 19/19")
    case Succ{n}: IO.die(Unit,1,"WIDE-BACKEND FAIL " ++ Nat.show(Succ{n}))

def arguments(args:List<String>) -> IO(Unit):
  do IO<Unit>:
    a : Nat <- report("huge-ignored-injection",accepted_in(huge()))
    b : Nat <- report("negative-ignored-injection",accepted_in(negative()))
    c : Nat <- report("huge-ignored-projection",accepted_out(huge()))
    d : Nat <- report("negative-ignored-projection",accepted_out(negative()))
    e : Nat <- report("huge-projection-evaluation",evaluated_out(huge()))
    f : Nat <- report("negative-projection-evaluation",evaluated_out(negative()))
    g : Nat <- report("huge-injection-address",rejected_in(huge()))
    h : Nat <- report("negative-injection-address",rejected_in(negative()))
    i : Nat <- report("huge-projection-address",rejected_out(huge()))
    j : Nat <- report("negative-projection-address",rejected_out(negative()))
    k : Nat <- report("huge-injection-exact-wasm",emitted_in(huge()))
    l : Nat <- report("negative-injection-exact-wasm",emitted_in(negative()))
    m : Nat <- report("huge-projection-backend-refusal",erased_out(huge()))
    n : Nat <- report("negative-projection-backend-refusal",erased_out(negative()))
    o : Nat <- report("finite-diagram",finite_diagram())
    p : Nat <- report("huge-diagram-refusal",width_refused(huge()))
    q : Nat <- report("negative-diagram-refusal",width_refused(negative()))
    r : Nat <- report("wide-projection-offsets",flags())
    s : Nat <- report("ignored-circuit-metadata",do Result<&2,&2,String,Unit>:
      circuit_zero(injection(huge(),negative()))
      circuit_zero(injection(negative(),huge()))
      circuit_zero(projection(huge(),Big.zero()))
      circuit_zero(projection(negative(),Big.zero())))
    finish((a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r + s : Nat))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
