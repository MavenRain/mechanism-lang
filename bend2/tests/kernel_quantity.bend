import Base
import ../kernel/quantity.bend as Q

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS linear path usage")
    case False{}:
      IO.die(Unit, 1, "FAIL linear path usage")

def main() -> IO(Unit):
  +once = Q.occurrence(0n, Q.QOne{})
  +dead = Q.sequence(once, Q.unreachable())
  verify(Bool.and(Q.exactly_once(0n, Q.alternative(once, once)), Bool.and(Bool.not(Q.exactly_once(0n, Q.alternative(once, Q.empty()))), Bool.and(Bool.not(Q.at_most_once(0n, Q.sequence(once, once))), Bool.and(Q.exactly_once(0n, Q.alternative(once, dead)), Bool.and(Q.used(0n, Q.captures(dead)), Bool.not(Q.used(0n, Q.scale(Q.QZero{}, once)))))))))
