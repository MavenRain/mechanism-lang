import Base
import ./surface_catalog.bend as Test
import ../surface/family_poly.bend as Family
import ../kernel/check.bend as Check
import ../kernel/common.bend as P
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def payload(name: String, level: Level.T) -> Term.T:
  Term.Ann{Term.Global{name}, Term.Univ{level}}

def shape(mode: Nat, name: String, level: Level.T) -> Shape.T<Term.T>:
  match mode:
    case 0n: Shape.SZk{Q.QZero{}, "w", payload(name, level)}
    case 1n: Shape.SFhc{payload(name, level)}
    case 2n: Shape.SMpc{payload(name, level), Term.Univ{Level.zero()}}
    case other: Shape.SMpc{Term.Univ{Level.zero()}, payload(name, level)}

def hidden_body(shape: Shape.T<Term.T>) -> Term.T:
  +pi : Shape.T<Term.T> = Shape.SPi{Q.QMany{}, "A", Term.Univ{Level.one()}}
  identity : Term.T = Term.Ann{Term.Sec{pi, [Term.Leg{[P.Pair{Q.QMany{}, "A"}], Term.Var{0n}}]}, Term.Ran{pi, Term.Univ{Level.one()}}}
  Term.Out{Shape.SPi{Q.QMany{}, "ignored", Term.Lan{shape, Term.Univ{Level.zero()}}}, Term.APt{Q.QMany{}, Term.Univ{Level.zero()}}, identity}

def hidden(shape: Shape.T<Term.T>) -> Check.Decl:
  Check.Decl{"hidden", Check.Definition{}, Term.Univ{Level.one()}, Some{hidden_body(shape)}}

def declare(shape: Shape.T<Term.T>) -> Result<&2, &2, Error.T, Family.T>:
  Family.declare(Global.empty(), [], Budget.unlimited(), 1n, Test.marker(), Test.ctors(), [hidden(shape)])

def renamed(entry: Maybe<&2, Global.Entry>, expected: Term.T) -> Bool:
  match entry:
    case Some{Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}}: Term.equal(body, expected)
    case other: False{}

def check_shape(+mode: Nat) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    catalog : Family.T <- declare(shape(mode, "Marker", Level.var(0n)))
    globals : Global.T <- Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "Marker", [Level.one()], "Closed", [], None{})
    checked : Unit <- Test.ensure(renamed(Global.find("Closed_hidden", globals), hidden_body(shape(mode, "Closed", Level.one()))), "crypto shape payload did not specialize names and levels")
    Test.ensure(Test.scope_refused(Family.T, declare(shape(mode, "Marker", Level.var(1n)))), "crypto shape payload bypassed universe scope")

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- check_shape(0n)
    checked : Unit <- check_shape(1n)
    checked : Unit <- check_shape(2n)
    check_shape(3n)

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}: IO.print("VEIL-TEMPLATES OK shapes=4")
    case Fail{error}: IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())

def arguments(args: List<String>) -> IO(Unit):
  main()
