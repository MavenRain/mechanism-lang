import Base
import ../../../surface/family_poly.bend as Family
import ../../../kernel/check.bend as Check
import ../../../kernel/level.bend as L
import ../../../kernel/global.bend as G
import ../../../kernel/budget.bend as B
import ../../../kernel/error.bend as E

def main() -> Result<&2, &2, E.T, Family.T>:
  Family.declare(G.empty(), [], B.unlimited(), 0n, Check.FamilyDecl{"Negative", [], [], L.one()}, [], [])
