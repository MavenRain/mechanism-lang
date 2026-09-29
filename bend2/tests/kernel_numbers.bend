import Base
import ../kernel/bignum.bend as B

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS arbitrary precision arithmetic")
    case False{}:
      IO.die(Unit, 1, "FAIL arbitrary precision arithmetic")

def run(a: Maybe<&2, B.T>, b: Maybe<&2, B.T>) -> IO(Unit):
  match a b:
    case Some{+x} Some{+y}:
      verify(Bool.and(String.eq(B.to_string(B.add(x, y)), "123456789012345678910000000000"), Bool.and(String.eq(B.to_string(B.mul(x, y)), "1524157875323758556885594913688591961096125479"), Bool.and(String.eq(B.to_string(B.sub(x, y)), "123456789012320987552197530422"), B.equal(B.sub(y, x), B.zero())))))
    case None{} _:
      IO.die(Unit, 1, "FAIL parsing first integer")
    case Some{x} None{}:
      IO.die(Unit, 1, "FAIL parsing second integer")

def main() -> IO(Unit):
  run(B.of_decimal("123456789012333333231098765211"), B.of_decimal("12345678901234789"))
