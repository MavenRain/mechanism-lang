import Base
import ./dependent.bend as D
import ./congruence.bend as Congruence
import ../kernel/term.bend as Term
import ../kernel/quantity.bend as Q
import ../kernel/common.bend as Common
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/level.bend as Level
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../surface/family_poly.bend as Family

def variable(index: Nat) -> Result<&2, &2, Error.T, Level.T>:
  D.variable(index)

def declare_equality(raw: Family.RawFamily, globals: Global.T) -> Budget.Comp(Family.T):
  match raw:
    case Common.Pair{family, ctors}:
      Family.declare_comp(globals, [], 1n, family, ctors, [])

def catalog_comp(+globals: Global.T) -> Budget.Comp(Family.T):
  +u = Level.var(0n)
  +v = Level.var(1n)
  do Budget.Comp<Family.T>:
    catalog : Family.T <- declare_equality(Congruence.equality("MechEq", u), globals)
    Family.declare_comp(globals, catalog, 2n,
      Check.FamilyDecl{"MechSum", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}}, P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(v)}}], [], Level.max(Level.succ(u), Level.succ(v))},
      [Check.CtorDecl{"mechInl", [P.Binder{Q.QMany{}, "left", Term.Var{1n}}], [Term.Var{2n}, Term.Var{1n}], []},
       Check.CtorDecl{"mechInr", [P.Binder{Q.QMany{}, "right", Term.Var{0n}}], [Term.Var{2n}, Term.Var{1n}], []}], [])

def catalog(globals: Global.T, budget: Budget.T) -> Result<&2, &2, Error.T, Family.T>:
  Budget.run(Family.T, catalog_comp(globals), budget)
