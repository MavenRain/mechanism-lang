import Base
import ./dependent.bend as D
import ./congruence.bend as Congruence
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

def family(name: String, params: List<&2, Term.T>, indices: List<&2, Term.T>) -> Term.T:
  Term.Lan{Shape.SMu{name, indices}, Rules.diagram_of(params)}

def erased_apply(domain: Term.T, head: Term.T, argument: Term.T) -> Term.T:
  Term.Out{Shape.SPi{Q.QZero{}, "point", domain}, Term.APt{Q.QZero{}, argument}, head}

def reflexive(name: String, index: Term.T) -> Term.T:
  Term.In{Shape.SMu{name, [index]}, Term.ACtor{"mechReflCtor"}, []}

def eliminate_at(+name: String, index: Term.T, motive: Term.T, scrut: Term.T, branch: Term.T) -> Term.T:
  Term.Elim{Term.ElimData{Shape.SMu{name, [index]}, scrut, Q.QOne{},
    Some{Term.Motive{Some{name}, ["right"], "self", motive}},
    [Common.Pair{Term.ACtor{"mechReflCtor"}, Term.Leg{[], branch}}]}}

def eliminate(name: String, index: Term.T, motive: Term.T) -> Term.T:
  eliminate_at(name, index, motive, Term.Var{1n}, Term.Var{0n})

def eq(carrier: Term.T, left: Term.T, right: Term.T) -> Term.T:
  family("MechEq", [carrier, left], [right])

def at_motive(+carrier: Term.T, left: Term.T, motive: Term.T, +right: Term.T, proof: Term.T) -> Term.T:
  erased_apply(eq(carrier, left, right), erased_apply(carrier, motive, right), proof)

def refl(u: Level.T) -> Check.Decl:
  D.definition("refl", [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}}],
    eq(Term.Var{1n}, Term.Var{0n}, Term.Var{0n}), reflexive("MechEq", Term.Var{0n}))

def transport(u: Level.T, v: Level.T) -> Check.Decl:
  D.definition("transport",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}},
     P.Binder{Q.QZero{}, "B", D.arrow(P.Binder{Q.QMany{}, "point", Term.Var{0n}}, Term.Univ{v})},
     P.Binder{Q.QZero{}, "x", Term.Var{1n}}, P.Binder{Q.QZero{}, "y", Term.Var{2n}},
     P.Binder{Q.QMany{}, "proof", eq(Term.Var{3n}, Term.Var{1n}, Term.Var{0n})},
     P.Binder{Q.QMany{}, "value", D.apply(Term.Var{4n}, Term.Var{3n}, Term.Var{2n})}],
    D.apply(Term.Var{5n}, Term.Var{4n}, Term.Var{2n}),
    eliminate("MechEq", Term.Var{2n}, D.apply(Term.Var{7n}, Term.Var{6n}, Term.Var{1n})))

def j(u: Level.T, v: Level.T) -> Check.Decl:
  D.definition("j",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}},
     P.Binder{Q.QZero{}, "P", D.arrow(P.Binder{Q.QZero{}, "right", Term.Var{1n}},
       D.arrow(P.Binder{Q.QZero{}, "proof", eq(Term.Var{2n}, Term.Var{1n}, Term.Var{0n})}, Term.Univ{v}))},
     P.Binder{Q.QMany{}, "reflCase", at_motive(Term.Var{2n}, Term.Var{1n}, Term.Var{0n}, Term.Var{1n}, reflexive("MechEq", Term.Var{1n}))},
     P.Binder{Q.QZero{}, "y", Term.Var{3n}}, P.Binder{Q.QMany{}, "proof", eq(Term.Var{4n}, Term.Var{3n}, Term.Var{0n})}],
    at_motive(Term.Var{5n}, Term.Var{4n}, Term.Var{3n}, Term.Var{1n}, Term.Var{0n}),
    eliminate_at("MechEq", Term.Var{1n}, at_motive(Term.Var{7n}, Term.Var{6n}, Term.Var{5n}, Term.Var{1n}, Term.Var{0n}), Term.Var{0n}, Term.Var{2n}))

def symm(u: Level.T) -> Check.Decl:
  D.definition("symm",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}},
     P.Binder{Q.QZero{}, "y", Term.Var{1n}}, P.Binder{Q.QMany{}, "proof", eq(Term.Var{2n}, Term.Var{1n}, Term.Var{0n})}],
    eq(Term.Var{3n}, Term.Var{1n}, Term.Var{2n}),
    eliminate_at("MechEq", Term.Var{1n}, eq(Term.Var{5n}, Term.Var{1n}, Term.Var{4n}), Term.Var{0n}, reflexive("MechEq", Term.Var{2n})))

def trans(u: Level.T) -> Check.Decl:
  D.definition("trans",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}},
     P.Binder{Q.QZero{}, "y", Term.Var{1n}}, P.Binder{Q.QZero{}, "z", Term.Var{2n}},
     P.Binder{Q.QMany{}, "first", eq(Term.Var{3n}, Term.Var{2n}, Term.Var{1n})},
     P.Binder{Q.QMany{}, "second", eq(Term.Var{4n}, Term.Var{2n}, Term.Var{1n})}],
    eq(Term.Var{5n}, Term.Var{4n}, Term.Var{2n}),
    eliminate_at("MechEq", Term.Var{2n}, eq(Term.Var{7n}, Term.Var{6n}, Term.Var{1n}), Term.Var{0n}, Term.Var{1n}))

def congr(+u: Level.T) -> Check.Decl:
  D.definition("congr",
    [P.Binder{Q.QZero{}, "A", Term.Univ{u}}, P.Binder{Q.QZero{}, "B", Term.Univ{u}},
     P.Binder{Q.QZero{}, "f", D.arrow(P.Binder{Q.QMany{}, "point", Term.Var{1n}}, Term.Var{1n})},
     P.Binder{Q.QZero{}, "x", Term.Var{2n}}, P.Binder{Q.QZero{}, "y", Term.Var{3n}},
     P.Binder{Q.QMany{}, "proof", eq(Term.Var{4n}, Term.Var{1n}, Term.Var{0n})}],
    eq(Term.Var{4n}, D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}), D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{1n})),
    eliminate_at("MechEq", Term.Var{1n},
      eq(Term.Var{6n}, D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{4n}), D.apply(Term.Var{7n}, Term.Var{5n}, Term.Var{1n})),
      Term.Var{0n}, reflexive("MechEq", D.apply(Term.Var{5n}, Term.Var{3n}, Term.Var{2n}))))

def type_eq(left: Term.T, right: Term.T) -> Term.T:
  family("MechTypeEq", [left], [right])

def type_refl(u: Level.T) -> Check.Decl:
  D.definition("refl", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}}],
    type_eq(Term.Var{0n}, Term.Var{0n}), reflexive("MechTypeEq", Term.Var{0n}))

def cast(+u: Level.T) -> Check.Decl:
  D.definition("cast", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}},
    P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(u)}},
    P.Binder{Q.QMany{}, "proof", type_eq(Term.Var{1n}, Term.Var{0n})}, P.Binder{Q.QMany{}, "value", Term.Var{2n}}],
    Term.Var{2n}, eliminate("MechTypeEq", Term.Var{2n}, Term.Var{1n}))

def type_symm(+u: Level.T) -> Check.Decl:
  D.definition("symm", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}},
    P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(u)}}, P.Binder{Q.QMany{}, "proof", type_eq(Term.Var{1n}, Term.Var{0n})}],
    type_eq(Term.Var{1n}, Term.Var{2n}),
    eliminate_at("MechTypeEq", Term.Var{1n}, type_eq(Term.Var{1n}, Term.Var{4n}), Term.Var{0n}, reflexive("MechTypeEq", Term.Var{2n})))

def type_trans(+u: Level.T) -> Check.Decl:
  D.definition("trans", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}},
    P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(u)}}, P.Binder{Q.QZero{}, "C", Term.Univ{Level.succ(u)}},
    P.Binder{Q.QMany{}, "first", type_eq(Term.Var{2n}, Term.Var{1n})}, P.Binder{Q.QMany{}, "second", type_eq(Term.Var{2n}, Term.Var{1n})}],
    type_eq(Term.Var{4n}, Term.Var{2n}), eliminate_at("MechTypeEq", Term.Var{2n}, type_eq(Term.Var{6n}, Term.Var{1n}), Term.Var{0n}, Term.Var{1n}))

def declare_equality(raw: Family.RawFamily, globals: Global.T, members: List<&2, Check.Decl>) -> Budget.Comp(Family.T):
  match raw:
    case Common.Pair{family, ctors}:
      Family.declare_comp(globals, [], 2n, family, ctors, members)

def catalog_comp(+globals: Global.T) -> Budget.Comp(Family.T):
  +u = Level.var(0n)
  +v = Level.var(1n)
  do Budget.Comp<Family.T>:
    catalog : Family.T <- declare_equality(Congruence.equality("MechEq", u), globals,
      [refl(u), transport(u, v), j(u, v), symm(u), trans(u), congr(u)])
    Family.declare_comp(globals, catalog, 1n,
      Check.FamilyDecl{"MechTypeEq", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}}], [P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(u)}}], Level.zero()},
      [Check.CtorDecl{"mechReflCtor", [], [Term.Var{0n}], [Term.Var{0n}]}],
      [type_refl(u), cast(u), type_symm(u), type_trans(u)])

def catalog(globals: Global.T, budget: Budget.T) -> Result<&2, &2, Error.T, Family.T>:
  Budget.run(Family.T, catalog_comp(globals), budget)
