import Base
import ./surface_catalog.bend as Test
import ../prelude/families.bend as Families
import ../prelude/dependent.bend as D
import ../surface/family_poly.bend as Family
import ../surface/poly.bend as Poly
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as P
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/level_var.bend as LevelVar
import ../kernel/bignum.bend as Bignum
import ../kernel/rules.bend as Rules
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def high_value(value: Maybe<&2, Bignum.T>) -> Result<&2, &2, Error.T, Level.T>:
  match value:
    case Some{number}: Done{LevelVar.LSucc{number, Level.zero()}}
    case None{}: Fail{Error.Mismatch{"high universe literal did not parse"}}

def member(name: String, params: List<&2, Term.T>, indices: List<&2, Term.T>) -> Term.T:
  Term.Lan{Shape.SMu{name, indices}, Rules.diagram_of(params)}

def reflexive(+name: String, carrier: Level.T) -> Check.Decl:
  D.definition("reflClient", [P.Binder{Q.QZero{}, "A", Term.Univ{carrier}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}}], member(name, [Term.Var{1n}, Term.Var{0n}], [Term.Var{0n}]), Term.In{Shape.SMu{name, [Term.Var{0n}]}, Term.ACtor{"mechReflCtor"}, []})

def eq_metadata(family: Maybe<&2, P.Family>) -> Bool:
  match family:
    case Some{+family}:
      Level.equal(Family.kernel_level(family), Level.zero()) && Engine.mu_zero_eliminable(family)
    case None{}: False{}

def equalities(+catalog: Family.T) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +high : Level.T <- high_value(Bignum.of_decimal("4611686018427387903"))
    globals : Global.T <- Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "MechEq", [Level.zero()], "EqProp", [], None{})
    globals : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "MechEq", [Level.one()], "EqData", [], None{})
    +globals : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "MechEq", [high], "EqHigh", [], None{})
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), reflexive("EqProp", Level.zero()))
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), reflexive("EqData", Level.one()))
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), reflexive("EqHigh", high))
    checked : Unit <- Test.ensure(eq_metadata(Global.find_family("EqHigh", globals)), "equality lost Prop or singleton elimination")
    Test.ensure(Bool.not(Poly.is_some(P.Family, Global.find_family("MechEq", globals))), "equality specialization installed the symbolic family")

def sum_client(+name: String, left: Level.T, right: Level.T, ctor: String, field: Term.T) -> Check.Decl:
  D.definition("sumClient", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(left)}}, P.Binder{Q.QZero{}, "B", Term.Univ{Level.succ(right)}}, P.Binder{Q.QMany{}, "value", field}], member(name, [Term.Var{2n}, Term.Var{1n}], []), Term.In{Shape.SMu{name, []}, Term.ACtor{ctor}, [Term.Var{0n}]})

def sum_level(family: Maybe<&2, P.Family>) -> Bool:
  match family:
    case Some{family}: Level.equal(Family.kernel_level(family), Level.of_u32(3))
    case None{}: False{}

def sums(+catalog: Family.T) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    globals : Global.T <- Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "MechSum", [Level.zero(), Level.of_u32(2)], "SumLowHigh", [], None{})
    +globals : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "MechSum", [Level.of_u32(2), Level.zero()], "SumHighLow", [], None{})
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), sum_client("SumLowHigh", Level.zero(), Level.of_u32(2), "mechInl", Term.Var{1n}))
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), sum_client("SumLowHigh", Level.zero(), Level.of_u32(2), "mechInr", Term.Var{0n}))
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), sum_client("SumHighLow", Level.of_u32(2), Level.zero(), "mechInl", Term.Var{1n}))
    entry : Global.Entry <- Check.check_decl(globals, Budget.unlimited(), sum_client("SumHighLow", Level.of_u32(2), Level.zero(), "mechInr", Term.Var{0n}))
    Test.ensure(sum_level(Global.find_family("SumLowHigh", globals)) && sum_level(Global.find_family("SumHighLow", globals)), "sum maximum universe did not specialize in both directions")

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Families.catalog(Global.empty(), Budget.unlimited())
    checked : Unit <- equalities(catalog)
    sums(catalog)

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}: IO.print("PASS Eq Prop/data/high universes and Sum unequal universes in both directions")
    case Fail{error}: IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
