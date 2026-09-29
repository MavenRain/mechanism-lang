import Base
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/common.bend as Common
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/level.bend as Level
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../surface/poly.bend as Poly

# Nonnegative universe indices are enforced by Nat in the native API.
def variable(index: Nat) -> Result<&2, &2, Error.T, Level.T>:
  Done{Level.var(index)}

def arrow(binder: P.Binder, body: Term.T) -> Term.T:
  match binder:
    case P.Binder{quantity, name, domain}:
      Term.Ran{Shape.SPi{quantity, name, domain}, body}

def lambda(binder: P.Binder, body: Term.T) -> Term.T:
  match binder:
    case P.Binder{+quantity, +name, domain}:
      Term.Sec{Shape.SPi{quantity, name, domain}, [Term.Leg{[Common.Pair{quantity, name}], body}]}

def arrows(binders: P.Telescope, body: Term.T) -> Term.T:
  match binders:
    case Nil{}:
      body
    case Con{head, tail}:
      arrow(head, arrows(tail, body))

def lambdas(binders: P.Telescope, body: Term.T) -> Term.T:
  match binders:
    case Nil{}:
      body
    case Con{head, tail}:
      lambda(head, lambdas(tail, body))

def definition(name: String, +binders: P.Telescope, result: Term.T, body: Term.T) -> Check.Decl:
  Check.Decl{name, Check.Definition{}, arrows(binders, result), Some{lambdas(binders, body)}}

def point(domain: Term.T) -> Shape.T<Term.T>:
  Shape.SPi{Q.QMany{}, "point", domain}

def apply(domain: Term.T, head: Term.T, argument: Term.T) -> Term.T:
  Term.Out{point(domain), Term.APt{Q.QMany{}, argument}, head}

def sigma(+a: Nat, b: Nat) -> Term.T:
  Term.Lan{point(Term.Var{a}), apply(Term.Var{1n+a}, Term.Var{1n+b}, Term.Var{0n})}

def pair(a: Nat, x: Term.T, y: Term.T) -> Term.T:
  Term.In{point(Term.Var{a}), Term.APt{Q.QMany{}, x}, [y]}

def eliminate(a: Nat, scrut: Term.T, motive: Term.T, branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> Term.T:
  Term.Elim{Term.ElimData{point(Term.Var{a}), scrut, Q.QOne{}, Some{Term.Motive{None{}, [], "self", motive}}, branches}}

def proj_branch(which: Nat) -> List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>:
  [Common.Pair{Term.aleg(Term.T, 0n), Term.Leg{[Common.Pair{Q.QMany{}, "x"}, Common.Pair{Q.QMany{}, "y"}], Term.Var{Nat.sub(1n, which)}}}]

def first(+a: Nat, scrut: Term.T) -> Term.T:
  eliminate(a, scrut, Term.Var{1n+a}, proj_branch(0n))

def pi(+u: Level.T, +v: Level.T) -> Check.Decl:
  definition("MechPi", [P.Binder{Q.QZero{}, "A", Term.Univ{u}},
    P.Binder{Q.QZero{}, "B", arrow(P.Binder{Q.QMany{}, "x", Term.Var{0n}}, Term.Univ{v})}],
    Term.Univ{Level.imax(u, v)}, arrow(P.Binder{Q.QMany{}, "x", Term.Var{1n}}, apply(Term.Var{2n}, Term.Var{1n}, Term.Var{0n})))

def parameters(u: Level.T, v: Level.T) -> P.Telescope:
  [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(u)}},
   P.Binder{Q.QZero{}, "B", arrow(P.Binder{Q.QMany{}, "x", Term.Var{0n}}, Term.Univ{Level.succ(v)})}]

def sigma_type(+u: Level.T, +v: Level.T) -> Check.Decl:
  definition("MechSigma", parameters(u, v), Term.Univ{Level.max(Level.succ(u), Level.succ(v))}, sigma(1n, 0n))

def make(u: Level.T, v: Level.T) -> Check.Decl:
  definition("mechSigmaMk", List.append(&2, P.Binder, parameters(u, v),
    [P.Binder{Q.QMany{}, "x", Term.Var{1n}}, P.Binder{Q.QMany{}, "y", apply(Term.Var{2n}, Term.Var{1n}, Term.Var{0n})}]),
    sigma(3n, 2n), pair(3n, Term.Var{1n}, Term.Var{0n}))

def projections(u: Level.T, v: Level.T) -> P.Telescope:
  List.append(&2, P.Binder, parameters(u, v), [P.Binder{Q.QMany{}, "pair", sigma(1n, 0n)}])

def fst(u: Level.T, v: Level.T) -> Check.Decl:
  definition("mechSigmaFst", projections(u, v), Term.Var{2n}, first(2n, Term.Var{0n}))

def snd(u: Level.T, v: Level.T) -> Check.Decl:
  definition("mechSigmaSnd", projections(u, v), apply(Term.Var{2n}, Term.Var{1n}, first(2n, Term.Var{0n})),
    eliminate(2n, Term.Var{0n}, apply(Term.Var{3n}, Term.Var{2n}, first(3n, Term.Var{0n})), proj_branch(1n)))

def recursor(u: Level.T, v: Level.T, w: Level.T) -> Check.Decl:
  definition("mechSigmaRec", List.append(&2, P.Binder, parameters(u, v),
    [P.Binder{Q.QZero{}, "P", arrow(P.Binder{Q.QMany{}, "pair", sigma(1n, 0n)}, Term.Univ{w})},
     P.Binder{Q.QMany{}, "step", arrow(P.Binder{Q.QMany{}, "x", Term.Var{2n}},
       arrow(P.Binder{Q.QMany{}, "y", apply(Term.Var{3n}, Term.Var{2n}, Term.Var{0n})},
         apply(sigma(4n, 3n), Term.Var{2n}, pair(4n, Term.Var{1n}, Term.Var{0n}))))},
     P.Binder{Q.QMany{}, "pair", sigma(3n, 2n)}]),
    apply(sigma(4n, 3n), Term.Var{2n}, Term.Var{0n}),
    eliminate(4n, Term.Var{0n}, apply(sigma(5n, 4n), Term.Var{3n}, Term.Var{0n}),
      [Common.Pair{Term.aleg(Term.T, 0n), Term.Leg{[Common.Pair{Q.QMany{}, "x"}, Common.Pair{Q.QMany{}, "y"}],
        apply(apply(Term.Var{6n}, Term.Var{5n}, Term.Var{1n}), apply(Term.Var{6n}, Term.Var{3n}, Term.Var{1n}), Term.Var{0n})}}]))

def catalog_comp(+globals: Global.T) -> Budget.Comp(Poly.T):
  +u = Level.var(0n)
  +v = Level.var(1n)
  w = Level.var(2n)
  do Budget.Comp<Poly.T>:
    catalog : Poly.T <- Poly.declare_comp(globals, [], 2n, pi(u, v))
    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, sigma_type(u, v))
    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, make(u, v))
    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, fst(u, v))
    catalog : Poly.T <- Poly.declare_comp(globals, catalog, 2n, snd(u, v))
    Poly.declare_comp(globals, catalog, 3n, recursor(u, v, w))

def catalog(globals: Global.T, budget: Budget.T) -> Result<&2, &2, Error.T, Poly.T>:
  Budget.run(Poly.T, catalog_comp(globals), budget)
