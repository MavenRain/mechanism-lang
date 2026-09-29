import Base
import ../../../kernel/level.bend as L
import ../../../kernel/check.bend as Check
import ../../../kernel/global.bend as G
import ../../../kernel/budget.bend as B
import ../../../kernel/error.bend as E
import ../../../kernel/term.bend as T

def main() -> Result<&2,&2,E.T,Unit>:
  Check.check_scheme(G.initial(),B.unlimited(),0n,Check.Decl{"negative",Check.Definition{},T.Univ{L.one()},Some{T.Univ{L.zero()}}})
