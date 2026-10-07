import Base
import ../prelude/dependent.bend as D
import ../prelude/equality.bend as Equality
import ../prelude/congruence.bend as Congruence
import ../prelude/families.bend as Families
import ../surface/poly.bend as Poly
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Bignum
import ../kernel/eval.bend as Eval
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def ensure(ok: Bool, message: String) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, ok, Done{Unit{}}, Fail{Error.Mismatch{message}})

def installed(pair: Common.Pair<Global.T, Term.T>) -> Global.T:
  match pair:
    case Common.Pair{globals, term}:
      globals

def install(globals: Global.T, catalog: Poly.T, name: String, levels: List<&2, Level.T>, alias: String) -> Result<&2, &2, Error.T, Global.T>:
  do Result<&2, &2, Error.T, Global.T>:
    result : Common.Pair<Global.T, Term.T> <- Poly.instantiate(globals, catalog, Budget.unlimited(), name, levels, alias)
    return installed(result)

def checked_globals(result: Elab.Checked) -> Global.T:
  match result:
    case Common.Pair{globals, rows}:
      globals

def check_source(globals: Global.T, source: String) -> Result<&2, &2, Error.T, Global.T>:
  do Result<&2, &2, Error.T, Global.T>:
    result : Elab.Checked <- Elab.check_in(globals, Budget.unlimited(), source)
    return checked_globals(result)

def literal_nat(term: Term.T, expected: String) -> Result<&2, &2, Error.T, Unit>:
  match term:
    case Term.Lit{Literal.LInt{number}}:
      ensure(String.eq(Bignum.to_string(number), expected), "wrong computed prelude result")
    case other:
      Fail{Error.Mismatch{"prelude result did not compute to a natural"}}

def evaluates(+globals: Global.T, name: String, expected: String) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    value : Value.T <- Eval.eval(globals, [], Term.Global{name})
    term : Term.T <- Eval.normalize(globals, 0n, value)
    literal_nat(term, expected)

def dependent() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Poly.T <- D.catalog(Global.initial(), Budget.unlimited())
    globals : Global.T <- install(Global.initial(), catalog, "MechPi", [Level.one(), Level.one()], "Pi")
    globals : Global.T <- install(globals, catalog, "MechSigma", [Level.zero(), Level.zero()], "Sigma")
    globals : Global.T <- install(globals, catalog, "mechSigmaMk", [Level.zero(), Level.zero()], "make")
    globals : Global.T <- install(globals, catalog, "mechSigmaFst", [Level.zero(), Level.zero()], "first")
    globals : Global.T <- install(globals, catalog, "mechSigmaSnd", [Level.zero(), Level.zero()], "second")
    globals : Global.T <- install(globals, catalog, "mechSigmaRec", [Level.zero(), Level.zero(), Level.one()], "fold")
    +globals : Global.T <- check_source(globals,
      "def B : Nat -> Type 0 := fun (x : Nat) => Nat " ++
      "def p : Sigma Nat B := make Nat B 3 7 " ++
      "def x : Nat := first Nat B p def y : Nat := second Nat B p " ++
      "def combined : Nat := fold Nat B (fun (p : Sigma Nat B) => Nat) (fun (x : Nat) (y : Nat) => natAdd x y) p " ++
      "def id : Pi Nat B := fun (x : Nat) => x def piValue : Nat := id 13")
    checked : Unit <- evaluates(globals, "x", "3")
    checked : Unit <- evaluates(globals, "y", "7")
    checked : Unit <- evaluates(globals, "combined", "10")
    evaluates(globals, "piValue", "13")

def equality() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Equality.catalog(Global.initial(), Budget.unlimited())
    globals : Global.T <- Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "MechEq", [Level.one(), Level.one()], "Eq", [], None{})
    globals : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "MechTypeEq", [Level.zero()], "TEq", [], None{})
    +globals : Global.T <- check_source(globals,
      "def proof : Eq Nat 3 3 := Eq_refl Nat 3 " ++
      "def symmetric : Eq Nat 3 3 := Eq_symm Nat 3 3 proof " ++
      "def transitive : Eq Nat 3 3 := Eq_trans Nat 3 3 3 proof symmetric " ++
      "def transported : Nat := Eq_transport Nat (fun (x : Nat) => Nat) 3 3 proof 11 " ++
      "def casted : Nat := TEq_cast Nat Nat (TEq_refl Nat) 7")
    checked : Unit <- evaluates(globals, "transported", "11")
    evaluates(globals, "casted", "7")

def congruence() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    catalog : Family.T <- Congruence.catalog(Global.initial(), Budget.unlimited())
    globals : Global.T <- Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "MechCongr", [Level.one(), Level.one()], "Cong", [], None{})
    globals : Global.T <- check_source(globals,
      "def proof : Cong_Result Nat (natAdd 3 1) (natAdd 3 1) := Cong_congr Nat Nat (fun (x : Nat) => natAdd x 1) 3 3 mechReflCtor")
    return Unit{}

def families() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Families.catalog(Global.initial(), Budget.unlimited())
    globals : Global.T <- Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "MechEq", [Level.one()], "BaseEq", [], None{})
    globals : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "MechSum", [Level.zero(), Level.zero()], "Sum", [], None{})
    globals : Global.T <- check_source(globals,
      "def proof : BaseEq Nat 3 3 := mechReflCtor " ++
      "def right : Sum Nat Nat := mechInr 9 " ++
      "def result : Nat := case right as self in Sum return Nat with | mechInl (x : Nat) => x | mechInr (y : Nat) => y")
    evaluates(globals, "result", "9")

def exhausted(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      True{}
    case other:
      False{}

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- dependent()
    checked : Unit <- equality()
    checked : Unit <- congruence()
    checked : Unit <- families()
    checked : Unit <- ensure(exhausted(Poly.T, D.catalog(Global.initial(), Budget.of_polls(0n))), "dependent catalog ignored budget")
    checked : Unit <- ensure(exhausted(Family.T, Equality.catalog(Global.initial(), Budget.of_polls(0n))), "equality catalog ignored budget")
    checked : Unit <- ensure(exhausted(Family.T, Congruence.catalog(Global.initial(), Budget.of_polls(0n))), "congruence catalog ignored budget")
    ensure(exhausted(Family.T, Families.catalog(Global.initial(), Budget.of_polls(0n))), "family catalog ignored budget")

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.print("PASS four trusted prelude catalogs and computed specializations")
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
