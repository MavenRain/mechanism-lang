import Base
import ../../../bend2/surface/family_poly.bend as Family
import ../../../bend2/kernel/global.bend as G
import ../../../bend2/kernel/budget.bend as B
import ../../../bend2/kernel/bignum.bend as Big

def probe() -> B.Comp(Family.T):
  Family.compose_comp(G.empty(), Family.empty(), Big.neg(Big.one()), "Boundary", [], [])

def main() -> IO(Unit):
  IO.pure(Unit, Unit{})
