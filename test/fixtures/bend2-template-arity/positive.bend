import Base
import ../../../bend2/surface/family_poly.bend as Family
import ../../../bend2/kernel/global.bend as G
import ../../../bend2/kernel/budget.bend as B

def probe() -> B.Comp(Family.T):
  Family.compose_comp(G.empty(), Family.empty(), 1n, "Boundary", [], [])

def main() -> IO(Unit):
  IO.pure(Unit, Unit{})
