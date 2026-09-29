import Base
import ./levels_protocol.bend as Test
import ../kernel/bignum.bend as Big
import ../kernel/budget.bend as B
import ../kernel/check.bend as Check
import ../kernel/context.bend as Context
import ../kernel/error.bend as E
import ../kernel/eval.bend as Eval
import ../kernel/global.bend as G
import ../kernel/literal.bend as Lit
import ../kernel/quantity.bend as Q
import ../kernel/rules.bend as R
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/common.bend as C

def zero() -> T.T:
  T.Lit{Lit.LInt{Big.zero()}}

def nat() -> T.T:
  T.Global{"Nat"}

def injection(width: Big.T, index: Big.T) -> T.T:
  T.In{S.SColl{width},T.ALeg{index},[zero()]}

def tuple() -> T.T:
  T.Ann{T.Sec{S.coll(T.T,1n),[T.Leg{[],zero()}]},R.prod_ty([nat()])}

def projection(width: Big.T, index: Big.T) -> T.T:
  T.Out{S.SColl{width},T.ALeg{index},tuple()}

def check_injection(width: Big.T, index: Big.T) -> Result<&2,&2,E.T,Unit>:
  do Result<&2,&2,E.T,Unit>:
    ty : V.T <- Eval.eval(G.initial(),[],R.sum_ty([nat()]))
    Check.check_term(G.initial(),B.unlimited(),injection(width,index),ty)

def infer_projection(width: Big.T, index: Big.T) -> Result<&2,&2,E.T,V.T>:
  Check.infer_term(G.initial(),B.unlimited(),projection(width,index))

def roundtrip(width: Big.T) -> Test.R(Unit):
  +term = injection(width,Big.zero())
  do Result<&2,&2,String,Unit>:
    value : V.T <- Test.kernel(V.T,Eval.eval(G.initial(),[],term))
    quoted : T.T <- Test.kernel(T.T,Eval.quote(G.initial(),0n,value))
    Test.require(T.equal(term,quoted),"quote changed ignored signed or huge width")

def accepted(+width: Big.T) -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    ok : Unit <- Test.kernel(Unit,check_injection(width,Big.zero()))
    ty : V.T <- Test.kernel(V.T,infer_projection(width,Big.zero()))
    roundtrip(width)

def refused(+index: Big.T) -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    ok : Unit <- Test.expect(Unit,check_injection(Big.one(),index),"wrong leg",Some{"the leg address is outside the collection"})
    Test.expect(V.T,infer_projection(Big.one(),index),"wrong leg",Some{"the leg address is outside the collection"})

def former_refused(+width: Big.T) -> Test.R(Unit):
  Test.expect(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Lan{S.SColl{width},T.Sec{S.SColl{width},[]}}),"mismatch",Some{"the diagram is not a section of the declared width"})

def party_width(+width: Big.T) -> Test.R(Unit):
  +ctx = Context.make(G.initial(),B.unlimited(),0n)
  +value : V.T = V.VLan{S.SColl{width},V.close([],R.diagram_of([])),None{}}
  do Result<&2,&2,String,Unit>:
    got : Big.T <- Test.kernel(Big.T,R.mpc_parties(ctx,value))
    Test.require(Big.equal(got,width),"MPC party inspection changed the declared raw width")

def some_nat(actual: Maybe<&2,Nat>, expected: Nat) -> Bool:
  match actual:
    case None{}: False{}
    case Some{n}: Nat.is_eq(n,expected)

def absent(-A: Data, actual: Maybe<&2,A>) -> Bool:
  match actual:
    case None{}: True{}
    case Some{value}: False{}

def bounded(+huge: Big.T) -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    ok : Unit <- Test.require(absent(Nat,Big.to_nat_bounded(huge,2n)),"huge address narrowed into Nat")
    ok : Unit <- Test.require(absent(Nat,Big.to_nat_bounded(Big.neg(Big.one()),2n)),"negative address narrowed into Nat")
    ok : Unit <- Test.require(some_nat(Big.to_nat_bounded(Big.of_u32(2),2n),2n),"inclusive concrete bound refused")
    ok : Unit <- Test.require(absent(Nat,Big.to_nat_bounded(Big.of_u32(3),2n)),"address exceeded concrete bound")
    ok : Unit <- Test.require(absent(U32,Big.at(U32,[7,8],huge)),"huge list lookup wrapped")
    Test.require(absent(U32,Big.at(U32,[7,8],Big.neg(Big.one()))),"negative list lookup wrapped")

def native_max(value: Maybe<&2,Nat>) -> Test.R(Unit):
  match value:
    case None{}: Fail{"native Nat maximum did not parse"}
    case Some{+n}: Test.require(String.eq(Big.to_string(Big.of_nat(n)),"281474976710655"),"Nat to Big lost high bits")

def mpc_constructors() -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    empty : T.T <- Test.kernel(T.T,R.mpc_sub_ty(Big.zero()))
    ok : Unit <- Test.require(T.equal(empty,R.prod_ty([])),"empty subset type changed")
    sub : T.T <- Test.kernel(T.T,R.mpc_sub_ty(Big.of_u32(3)))
    ok : Unit <- Test.require(T.equal(sub,R.prod_ty([nat(),nat(),nat()])),"subset field count changed")
    ok : Unit <- Test.expect(T.T,R.mpc_sub_ty(Big.neg(Big.one())),"mismatch",Some{"an mpc subset width must be nonnegative"})
    Test.expect(T.T,R.mpc_pred_ty(Big.neg(Big.one())),"mismatch",Some{"an mpc subset width must be nonnegative"})

def run_big(+huge: Big.T) -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    ok : Unit <- accepted(huge)
    ok : Unit <- accepted(Big.neg(Big.one()))
    ok : Unit <- accepted(Big.zero())
    ok : Unit <- refused(huge)
    ok : Unit <- refused(Big.neg(Big.one()))
    ok : Unit <- former_refused(huge)
    ok : Unit <- former_refused(Big.neg(Big.one()))
    ok : Unit <- party_width(huge)
    ok : Unit <- party_width(Big.neg(Big.one()))
    ok : Unit <- bounded(huge)
    ok : Unit <- native_max(Nat.read("281474976710655"))
    mpc_constructors()

def parsed(value: Maybe<&2,Big.T>) -> Test.R(Unit):
  match value:
    case None{}: Fail{"huge signed-width fixture did not parse"}
    case Some{huge}: run_big(huge)

def run() -> Test.R(Unit):
  parsed(Big.of_decimal("281474976710656"))

def finish(result: Test.R(Unit)) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit,1,message)
    case Done{unit}: IO.print("PASS signed large kernel widths and addresses")

def arguments(args: List<String>) -> IO(Unit):
  finish(run())

def main() -> IO(Unit):
  finish(run())
