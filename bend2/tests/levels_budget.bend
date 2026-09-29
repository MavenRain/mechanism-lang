import Base
import ../kernel/budget.bend as Budget
import ../kernel/level.bend as L
import ../kernel/level_eq.bend as Eq
import ../kernel/error.bend as Error

def exhausted(result: Result<&2, &2, Error.T, Bool>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      True{}
    case _:
      False{}

def accepted(result: Result<&2, &2, Error.T, Bool>) -> Bool:
  match result:
    case Done{True{}}:
      True{}
    case _:
      False{}

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS threaded universe budget and late exhaustion")
    case False{}:
      IO.die(Unit, 1, "FAIL threaded universe budget and late exhaustion")

def main() -> IO(Unit):
  +a = L.imax(L.var(0n), L.succ(L.var(1n)))
  +b = L.max(L.var(0n), L.succ(L.var(1n)))
  verify(Bool.and(exhausted(Eq.equal_budget(Budget.of_polls(0n), a, b)), Bool.and(exhausted(Eq.equal_budget(Budget.of_polls(20n), a, b)), accepted(Eq.equal_budget(Budget.of_polls(1000n), a, b)))))
