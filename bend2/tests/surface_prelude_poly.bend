import Base
import ./relational_common.bend as R
import ../prelude/families.bend as Families
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/level.bend as L
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

type Instance is Data:
  Instance{name: String, levels: List<&2, L.T>, alias: String}

def arity(value: Maybe<&2, Nat>, expected: Nat) -> Bool:
  match value:
    case Some{actual}: Nat.is_eq(actual, expected)
    case None{}: False{}

def globals(checked: Elab.Checked) -> G.T:
  match checked:
    case C.Pair{globals, rows}: globals

def audit(globals: G.T) -> Result<&2, &2, String, Unit>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- R.audit_entries(entries, G.empty())
        R.audit_families(families)

def install(instances: List<&2, Instance>, +catalog: Family.T, current: G.T) -> Result<&2, &2, String, G.T>:
  match instances:
    case Nil{}: Done{current}
    case Con{Instance{name, levels, alias}, tail}:
      do Result<&2, &2, String, G.T>:
        next : G.T <- R.kernel(G.T, Family.instantiate(current, catalog, B.unlimited(), name, levels, alias, [], None{}))
        install(tail, catalog, next)

def starts(expected: String, actual: String) -> Bool:
  match expected actual:
    case SNil{} _: True{}
    case SCon{x, xs} SCon{y, ys}: Char.is_eq(x, y) && starts(xs, ys)
    case _ _: False{}

def refusal(result: Result<&2, &2, E.T, Elab.Checked>, expected: String, message: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{message}
    case Fail{+error}: R.require(starts(expected, E.to_string(error)), "wrong refusal: " ++ E.to_string(error))

def endpoint_text() -> String:
  "mismatch: the constructor mechReflCtor of PolyEqData gives the index " ++
  "(In SMu MechNat [] (ACtor mechZero) []) and the type asks for " ++
  "(In SMu MechNat [] (ACtor mechSucc) [(In SMu MechNat [] (ACtor mechZero) [])])"

def instance_text() -> String:
  +arguments : String = "(Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => (Lan SMu MechUnit [] (Sec SColl 0 []))])"
  "mismatch: the term has type (Lan SMu PolySumLow [] " ++ arguments ++
  ") and the expected type is (Lan SMu MechSum [] " ++ arguments ++ ")"

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +catalog : Family.T <- R.lift(Family.T, R.kernel(Family.T, Families.catalog(G.empty(), B.unlimited())))
    checked : Unit <- R.assert(arity(Family.arity(catalog, "MechEq"), 1n) && arity(Family.arity(catalog, "MechSum"), 2n), "prelude catalog arities changed")
    source : String <- R.read(root, "prelude/init.mech")
    initial : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +installed : G.T <- R.lift(G.T, install([
      Instance{"MechEq", [L.one()], "PolyEqData"},
      Instance{"MechEq", [L.succ(L.one())], "PolyEqTypes"},
      Instance{"MechEq", [L.succ(L.succ(L.one()))], "PolyEqHigher"},
      Instance{"MechSum", [L.zero(), L.zero()], "PolySumLow"},
      Instance{"MechSum", [L.zero(), L.one()], "PolySumMixed"}], catalog, globals(initial)))
    checked : Unit <- R.lift(Unit, audit(installed))
    client : String <- R.read(root, "test/fixtures/prelude/polymorphic.mech")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(installed, client))
    +checked_globals : G.T = globals(checked)
    checked : Unit <- R.lift(Unit, audit(checked_globals))
    checked : Unit <- R.lift(Unit, refusal(Elab.check_in(installed, B.unlimited(),
      "def polyWrong : PolyEqData MechNat mechZero (mechSucc mechZero) := mechReflCtor\n"),
      endpoint_text(), "unequal endpoints accepted in an instantiated family"))
    checked : Unit <- R.lift(Unit, refusal(Elab.check_in(checked_globals, B.unlimited(),
      "def polyWrongInstance : MechSum MechNat MechUnit := polyLowLeft\n"),
      instance_text(), "distinct family instances became convertible"))
    return "templates=2 instances=5 negatives=2"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-POLY", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
