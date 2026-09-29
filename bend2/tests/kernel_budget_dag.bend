import Base
import ../kernel/check_engine.bend as K
import ../kernel/context.bend as C
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/level.bend as L
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/error.bend as E

def level_dag(depth: Nat) -> L.T:
  match depth:
    case 0n: L.zero()
    case 1n+n:
      +prior = level_dag(n)
      L.max(prior,prior)

def term_dag(depth: Nat) -> T.T:
  match depth:
    case 0n: T.Univ{L.zero()}
    case 1n+n:
      +prior = term_dag(n)
      T.Ran{S.SPi{Q.QZero{},"x",prior},prior}

def extracted(result: Result<&2,&2,E.T,L.T>) -> Bool:
  match result:
    case Done{level}: True{}
    case Fail{error}: False{}

def exhausted(result: Result<&2,&2,E.T,K.Inferred>) -> Bool:
  match result:
    case Fail{E.Budget_exhausted{message}}: True{}
    case _: False{}

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS successful universe extraction skips rendering; shared term DAG exhausts bounded polls")
    case False{}: IO.die(Unit,1,"FAIL DAG budget regression")

def main() -> IO(Unit):
  +ctx = C.make(G.initial(),B.unlimited(),0n)
  verify(Bool.and(extracted(B.run(L.T,K.universe_value(V.VUniv{level_dag(60n)},ctx),B.of_polls(0n))),exhausted(B.run(K.Inferred,K.infer_uses(ctx,Q.QZero{},term_dag(40n)),B.of_polls(100n)))))
