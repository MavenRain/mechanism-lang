import Base
import ./kernel_budget_callbacks.bend as Test
import ./kernel_budget_dag.bend as Dag
import ../kernel/budget.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as E
import ../kernel/level.bend as L
import ../kernel/level_eq.bend as Eq
import ../kernel/check_engine.bend as K
import ../kernel/context.bend as Context
import ../kernel/global.bend as G
import ../kernel/value.bend as V
import ../kernel/quantity.bend as Q
import ../kernel/totality.bend as Totality
import ../kernel/term.bend as T

@unsafe def cancel_after(remaining: Nat) -> B.Callback:
  match remaining:
    case 0n:
      B.of_poll(unit => (cancel_after(0n), True{}))
    case 1n+rest:
      B.of_poll(unit => (cancel_after(rest), False{}))

def comparison() -> B.Comp(Bool):
  Eq.decide_m(L.imax(L.var(0n), L.succ(L.var(1n))), L.max(L.var(0n), L.succ(L.var(1n))), True{})

def accepted(result: Result<&2, &2, E.T, Bool>) -> Bool:
  match result:
    case Done{True{}}: True{}
    case other: False{}

def comparison_boundary(state: C.Pair<B.T, Result<&2, &2, E.T, Bool>>) -> Bool:
  match state:
    case C.Pair{B.Polls{remaining}, Done{True{}}}:
      +used = Nat.sub(10000n, remaining)
      Nat.is_lt(20n, used) &&
      accepted(B.run_poll(Bool, comparison(), cancel_after(used))) &&
      Test.refused(Bool, B.run_poll(Bool, comparison(), cancel_after(Nat.sub(used, 1n)))) &&
      Test.refused(Bool, B.run_poll(Bool, comparison(), cancel_after(20n)))
    case other: False{}

def proof_probe() -> B.Comp(Bool):
  K.conv_is_prop(Context.make(G.initial(), B.unlimited(), 0n), V.VNeutral{V.HGlobal{"absent"}, []})

def proof_false(result: Result<&2, &2, E.T, Bool>) -> Bool:
  match result:
    case Done{False{}}: True{}
    case other: False{}

def proof_state(state: B.Callback & Result<&2, &2, E.T, Bool>) -> Bool:
  (next, result) = state
  proof_false(result) && Test.next_answer(next)

def proof_remainder(state: C.Pair<B.T, Result<&2, &2, E.T, Bool>>) -> Bool:
  match state:
    case C.Pair{B.Polls{0n}, Done{False{}}}: True{}
    case other: False{}

def after_probe() -> B.Comp(Bool):
  do B.Comp<Bool>:
    result : Bool <- proof_probe()
    tick : Unit <- B.tick()
    return result

def totality_message(result: Result<&2, &2, E.T, Maybe<&2, Nat>>) -> Bool:
  match result:
    case Fail{E.Budget_exhausted{message}}:
      String.eq(message, "the totality budget is exhausted")
    case other: False{}

def suite() -> Bool:
  comparison_boundary(B.run_state(Bool, comparison(), B.of_polls(10000n))) &&
  proof_state(B.run_poll_state(Bool, proof_probe(), Test.scripted([False{}, False{}, True{}, False{}]))) &&
  Test.refused(Bool, B.run_poll(Bool, proof_probe(), Test.scripted([True{}, False{}, False{}]))) &&
  Test.refused(Bool, B.run_poll(Bool, proof_probe(), Test.scripted([False{}, True{}, False{}]))) &&
  proof_remainder(B.run_state(Bool, proof_probe(), B.of_polls(2n))) &&
  Test.refused(Bool, B.run_poll(Bool, after_probe(), Test.scripted([False{}, False{}, True{}]))) &&
  Test.refused(K.Inferred, B.run_poll(K.Inferred, K.infer_uses(Context.make(G.initial(), B.unlimited(), 0n), Q.QZero{}, Dag.term_dag(40n)), cancel_after(100n))) &&
  totality_message(B.run_poll(Maybe<&2, Nat>, Totality.guard_comp(G.initial(), "constant", T.Global{"Nat"}, T.Global{"Nat"}), cancel_after(0n)))

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS callback kernel comparison, conversion recovery, scope and totality")
    case False{}: IO.die(Unit, 1, "FAIL kernel callback integration")

def main() -> IO(Unit):
  do IO<Unit>:
    +checks : Bool = suite()
    expires : Nat <- IO.now()
    expired : Result<&2, &2, E.T, Bool> <- B.run_io(Bool, comparison(), Test.deadline(expires))
    verify(checks && Test.refused(Bool, expired))
