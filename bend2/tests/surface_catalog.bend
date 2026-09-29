import Base
import ../surface/family_poly.bend as Family
import ../surface/poly.bend as Poly
import ../kernel/check.bend as Check
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/rules.bend as Rules
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def ensure(ok: Bool, message: String) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, ok, Done{Unit{}}, Fail{Error.Mismatch{message}})

def marker() -> Check.FamilyDecl:
  Check.FamilyDecl{"Marker", [], [], Level.one()}

def ctors() -> List<&2, Check.CtorDecl>:
  [Check.CtorDecl{"mark", [], [], []}]

def marker_ty(name: String) -> Term.T:
  Term.Lan{Shape.SMu{name, []}, Rules.diagram_of([])}

def witness() -> Check.Decl:
  Check.Decl{"witness", Check.Definition{}, marker_ty("Marker"), Some{Term.In{Shape.SMu{"Marker", []}, Term.ACtor{"mark"}, []}}}

def alias() -> Check.Decl:
  Check.Decl{"alias", Check.Definition{}, marker_ty("Marker"), Some{Term.Global{"witness"}}}

def member_alias(value: Maybe<&2, Global.Entry>) -> Bool:
  match value:
    case Some{Global.Def{Global.DefEntry{ty, Term.Global{name}, reducible, rec_arg, partial}}}:
      Term.equal(ty, marker_ty("Small")) && String.eq(name, "Small_witness")
    case other:
      False{}

def refused(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{error}:
      True{}
    case Done{value}:
      False{}

def scope_refused(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{Error.Universe{message}}:
      String.eq(message, "universe level is outside the global parameter scope")
    case other:
      False{}

def budget_refused(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      True{}
    case other:
      False{}

def forged(members: List<&2, Check.Decl>) -> Family.T:
  [Common.Pair{"Marker", Family.Scheme{0n, marker(), ctors(), [], members}}]

def first_callback(globals: Global.T) -> Budget.Comp(Check.Decl):
  do Budget.Comp<Check.Decl>:
    checked : Unit <- Budget.lift(Unit, ensure(Bool.not(Poly.is_some(Global.Entry, Global.find("witness", globals))), "first callback saw a future member"))
    return witness()

def second_callback(globals: Global.T) -> Budget.Comp(Check.Decl):
  do Budget.Comp<Check.Decl>:
    checked : Unit <- Budget.lift(Unit, ensure(Poly.is_some(Global.Entry, Global.find("witness", globals)), "second callback missed prior member"))
    return alias()

def failed_callback(globals: Global.T) -> Budget.Comp(Check.Decl):
  Budget.failed(Check.Decl, Error.Mismatch{"first callback failure"})

def callback_error(result: Result<&2, &2, Error.T, Family.T>) -> Bool:
  match result:
    case Fail{Error.Mismatch{message}}:
      String.eq(message, "first callback failure")
    case other:
      False{}

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    grouped : Family.T <- Family.declare_group_elaborated(Global.empty(), [], Budget.unlimited(), 0n, [Common.Pair{marker(), ctors()}], [first_callback, second_callback])
    checked : Unit <- ensure(callback_error(Family.declare_group_elaborated(Global.empty(), [], Budget.unlimited(), 0n, [Common.Pair{marker(), ctors()}], [failed_callback, second_callback])), "later callback replaced first error")
    +catalog : Family.T <- Family.declare(Global.empty(), [], Budget.unlimited(), 1n, marker(), ctors(), [witness(), alias()])
    +globals : Global.T <- Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "Marker", [Level.one()], "Small", [], None{})
    checked : Unit <- ensure(member_alias(Global.find("Small_alias", globals)), "member global reference was not renamed")
    checked : Unit <- ensure(Bool.not(Poly.is_some(Global.Entry, Global.find("witness", globals))), "symbolic member escaped")
    checked : Unit <- ensure(Bool.not(Poly.is_some(Family.Scheme, Family.lookup(catalog, "Small"))), "specialization changed catalog")
    again : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "Marker", [Level.one()], "Again", [], None{})
    exported : Global.T <- Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "Marker", [Level.one()], "Exported", [], Some{[Common.Pair{"witness", "make"}, Common.Pair{"alias", "get"}]})
    checked : Unit <- ensure(Poly.is_some(Global.Entry, Global.find("get", exported)), "export target missing")
    reused : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "Marker", [Level.one()], "Reused", [Common.Pair{"Marker", "Small"}], None{})
    checked : Unit <- ensure(Bool.not(Poly.is_some(Global.Entry, Global.find("Marker", reused))), "reused symbolic family escaped")
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.empty(), catalog, Budget.unlimited(), "Marker", [Level.one()], "Bad", [], Some{[]})), "incomplete export accepted")
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.empty(), forged([Check.Decl{"postulate", Check.Postulate{}, marker_ty("Marker"), None{}}]), Budget.unlimited(), "Marker", [], "Bad", [], None{})), "forged postulate member accepted")
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.empty(), forged([Check.Decl{"missing", Check.Definition{}, marker_ty("Marker"), None{}}]), Budget.unlimited(), "Marker", [], "Bad", [], None{})), "forged missing member body accepted")
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.empty(), forged([Check.Decl{"wrong", Check.Definition{}, marker_ty("Marker"), Some{Term.Univ{Level.one()}}}]), Budget.unlimited(), "Marker", [], "Bad", [], None{})), "forged ill-typed member accepted")
    checked : Unit <- ensure(scope_refused(Family.T, Family.declare(Global.empty(), [], Budget.unlimited(), 0n, marker(), ctors(), [Check.Decl{"hidden", Check.Definition{}, marker_ty("Marker"), Some{Term.Ann{Term.Auto{}, Term.Univ{Level.var(0n)}}}}])), "hidden annotation escaped universe scope")
    checked : Unit <- ensure(refused(Family.T, Family.declare(Global.empty(), [], Budget.unlimited(), 0n, marker(), List.append(&2, Check.CtorDecl, ctors(), ctors()), [])), "duplicate constructor accepted")
    checked : Unit <- ensure(budget_refused(Family.T, Family.declare(Global.empty(), [], Budget.of_polls(0n), 0n, marker(), ctors(), [])), "declaration ignored exhausted budget")
    ensure(budget_refused(Global.T, Family.instantiate(Global.empty(), catalog, Budget.of_polls(0n), "Marker", [Level.one()], "Bad", [], None{})), "specialization ignored exhausted budget")

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.print("PASS raw family catalog trust boundaries")
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
