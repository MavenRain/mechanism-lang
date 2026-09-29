import Base
import ../../../surface/family_poly.bend as Family
import ../../../kernel/global.bend as G
import ../../../kernel/budget.bend as B
import ../../../kernel/error.bend as E

def main() -> Result<&2, &2, E.T, Family.T>:
  Family.compose(G.empty(), [], B.unlimited(), -1n, "Composed", [], [])
