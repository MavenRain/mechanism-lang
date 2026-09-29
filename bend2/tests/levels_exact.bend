import Base
import ../kernel/level.bend as L
import ../kernel/level_var.bend as V
import ../kernel/bignum.bend as B

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS exact universe comparison")
    case False{}:
      IO.die(Unit, 1, "FAIL exact universe comparison")

def test_big(n: Maybe<&2, B.T>) -> IO(Unit):
  match n:
    case None{}:
      IO.die(Unit, 1, "FAIL universe offset parsing")
    case Some{+amount}:
      +u = L.var(0n)
      +v = L.var(1n)
      +big = V.offset(amount, u)
      verify(Bool.and(L.equal(L.imax(u, L.zero()), L.zero()), Bool.and(L.equal(L.imax(u, L.succ(v)), L.max(u, L.succ(v))), Bool.and(Bool.not(L.le(L.succ(u), u)), Bool.and(L.le(u, L.max(u, v)), Bool.and(L.equal(V.offset(B.one(), big), V.offset(B.add(amount, B.one()), u)), Bool.not(L.le(big, L.succ(u)))))))))

def main() -> IO(Unit):
  test_big(B.of_decimal("1234567890123456789012345678901234567890"))
