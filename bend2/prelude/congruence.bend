import Base
import ./dependent.bend as D
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/common.bend as Common
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/level.bend as Level
import ../kernel/global.bend as Global
import ../kernel/rules.bend as Rules
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../surface/family_poly.bend as Family

def variable(index: Nat) -> Result<&2, &2, Error.T, Level.T>:
  D.variable(index)

def eq(name: String, carrier: Term.T, left: Term.T, right: Term.T) -> Term.T:
  Term.Lan{Shape.SMu{name, [right]}, Rules.diagram_of([carrier, left])}

def equality(name: String, level: Level.T) -> Family.RawFamily:
  Common.Pair{Check.FamilyDecl{name,
    [P.Binder{Q.QZero{}, "A", Term.Univ{level}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}}],
    [P.Binder{Q.QZero{}, "y", Term.Var{1n}}], Level.zero()},
    [Check.CtorDecl{"mechReflCtor", [], [Term.Var{1n}, Term.Var{0n}], [Term.Var{0n}]}]}

def congr(u: Level.T, v: Level.T) -> Check.Decl:
  D.definition("congr",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "B", Term.Univ{v}},
     P.Binder{Q.QZero{}, "f", D.arrow(P.Binder{Q.QMany{}, "point", Term.Var{1n}}, Term.Var{1n})},
     P.Binder{Q.QZero{}, "x", Term.Var{2n}}, P.Binder{Q.QZero{}, "y", Term.Var{3n}},
     P.Binder{Q.QMany{}, "proof", eq("MechCongr", Term.Var{4n}, Term.Var{1n}, Term.Var{0n})}],
    eq("Result", Term.Var{4n}, D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}), D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{1n})),
    Term.Elim{Term.ElimData{Shape.SMu{"MechCongr", [Term.Var{1n}]}, Term.Var{0n}, Q.QOne{},
      Some{Term.Motive{Some{"MechCongr"}, ["right"], "self",
        eq("Result", Term.Var{6n}, D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{4n}), D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{1n}))}},
      [Common.Pair{Term.ACtor{"mechReflCtor"}, Term.Leg{[],
        Term.In{Shape.SMu{"Result", [D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n})]}, Term.ACtor{"mechReflCtor"}, []}}}]}})

def catalog_comp(globals: Global.T) -> Budget.Comp(Family.T):
  +u = Level.var(0n)
  +v = Level.var(1n)
  Family.declare_group_elaborated_comp(globals, [], 2n, [equality("MechCongr", u), equality("Result", v)], Family.raw_callbacks([congr(u, v)]))

def catalog(globals: Global.T, budget: Budget.T) -> Result<&2, &2, Error.T, Family.T>:
  Budget.run(Family.T, catalog_comp(globals), budget)
