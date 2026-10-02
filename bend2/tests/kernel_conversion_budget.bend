import Base
import ../kernel/bignum.bend as Width
import ./levels_protocol.bend as Test
import ../kernel/budget.bend as B
import ../kernel/common.bend as C
import ../kernel/check_engine.bend as Engine
import ../kernel/context.bend as Context
import ../kernel/global.bend as G
import ../kernel/level.bend as L
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/error.bend as E

def left() -> L.T:
  L.imax(L.var(0n),L.max(L.var(1n),L.var(2n)))

def right() -> L.T:
  L.max(L.imax(L.var(0n),L.var(1n)),L.imax(L.var(0n),L.var(2n)))

def action(collection: Bool) -> B.Comp(Bool):
  match collection:
    case False{}: Engine.conv_structural_whnf(Context.make(G.initial(),B.unlimited(),3n),V.VUniv{left()},V.VUniv{right()})
    case True{}: Engine.conv_annotations(S.SColl{Width.of_nat(0n)},Some{left()},Some{right()})

def failed(measured: C.Pair<U32,Result<&2,&2,E.T,Bool>>, complete: U32) -> Test.R(Unit):
  match measured:
    case C.Pair{count,result}: do Result<&2,&2,String,Unit>:
      ok : Unit <- Test.expect(Bool,result,"budget",Some{"the check budget is exhausted"})
      Test.require(U32.is_eq(count,complete),"conversion did not refuse the final measured poll")

def succeeded(measured: C.Pair<U32,Bool>, complete: U32) -> Test.R(Unit):
  match measured:
    case C.Pair{count,same}: Test.require(Bool.and(same,U32.is_eq(count,complete)),"one-higher conversion budget did not succeed at the same count")

def boundaries(measured: C.Pair<U32,Bool>, +collection: Bool) -> Test.R(Unit):
  match measured:
    case C.Pair{+complete,same}: do Result<&2,&2,String,Unit>:
      ok : Unit <- Test.require(Bool.and(same,U32.is_gt(complete,64)),"conversion omitted symbolic comparison polls")
      refused : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,action(collection),Some{U32.sub(complete,1)})
      ok : Unit <- failed(refused,complete)
      accepted : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,action(collection),Some{complete})
      done : C.Pair<U32,Bool> <- Test.completed(Bool,accepted)
      succeeded(done,complete)

def check(+collection: Bool) -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    measured : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,action(collection),None{})
    done : C.Pair<U32,Bool> <- Test.completed(Bool,measured)
    boundaries(done,collection)

def short_result(measured: C.Pair<U32,Bool>) -> Test.R(Unit):
  match measured:
    case C.Pair{count,same}: Test.require(Bool.and(U32.is_eq(count,0),Bool.not(same)),"unequal shapes polled their ignored annotations")

def short_circuit() -> Test.R(Unit):
  +diagram : V.Closure<V.T> = V.close([],T.Sec{S.SColl{Width.of_nat(0n)},[]})
  do Result<&2,&2,String,Unit>:
    measured : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,Engine.conv_formers(Context.make(G.initial(),B.unlimited(),3n),S.SColl{Width.of_nat(0n)},diagram,Some{left()},S.SColl{Width.of_nat(1n)},diagram,Some{right()}),Some{0})
    done : C.Pair<U32,Bool> <- Test.completed(Bool,measured)
    short_result(done)

def finish(result: Test.R(Unit)) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit,1,message)
    case Done{unit}: IO.print("PASS conversion level budgets: universes, annotations, exact boundaries, shape short circuit")

def universe_action() -> B.Comp(Bool):
  Engine.conv(Context.make(G.initial(),B.unlimited(),3n),V.VUniv{L.succ(L.max(left(),right()))},V.VUniv{left()},V.VUniv{right()})

def universe_counts(measured: C.Pair<U32,Bool>, reference: C.Pair<U32,Bool>) -> Test.R(U32):
  match measured reference:
    case C.Pair{+count,same} C.Pair{expected,structural}:
      do Result<&2,&2,String,U32>:
        ok : Unit <- Test.require(Bool.and(Bool.and(same,structural),U32.is_eq(count,expected)),"universe value conversion changed comparison polls")
        ok : Unit <- Test.require(U32.is_gt(count,0),"universe conversion omitted comparison polls")
        return count

def universe_dispatch() -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    measured : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,universe_action(),None{})
    done : C.Pair<U32,Bool> <- Test.completed(Bool,measured)
    reference : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,Engine.conv_type(Context.make(G.initial(),B.unlimited(),3n),V.VUniv{left()},V.VUniv{right()}),None{})
    structural : C.Pair<U32,Bool> <- Test.completed(Bool,reference)
    +complete : U32 <- universe_counts(done,structural)
    refused : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,universe_action(),Some{U32.sub(complete,1)})
    ok : Unit <- failed(refused,complete)
    accepted : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- Test.measure(Bool,universe_action(),Some{complete})
    done : C.Pair<U32,Bool> <- Test.completed(Bool,accepted)
    succeeded(done,complete)

def run() -> Test.R(Unit):
  do Result<&2,&2,String,Unit>:
    ok : Unit <- check(False{})
    ok : Unit <- check(True{})
    ok : Unit <- short_circuit()
    universe_dispatch()

def arguments(args: List<String>) -> IO(Unit):
  finish(run())

def main() -> IO(Unit):
  finish(run())
