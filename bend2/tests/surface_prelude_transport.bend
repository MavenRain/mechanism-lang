import Base
import ./relational_common.bend as R
import ./surface_foundation_common.bend as F
import ./surface_prelude_poly.bend as Audit
import ./surface_prelude_congruence.bend as Congruence
import ../prelude/equality.bend as Prelude
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/budget.bend as B

def members(value: Maybe<&2, List<&2, String>>, expected: List<&2, String>) -> Bool:
  match value:
    case Some{names}: R.strings(names, expected)
    case None{}: False{}

def inventory(+catalog: Family.T) -> Bool:
  R.optional_nat(Family.arity(catalog, "MechEq"), Some{2n}) &&
  members(Family.members(catalog, "MechEq"), ["refl", "transport", "j", "symm", "trans", "congr"]) &&
  R.optional_nat(Family.arity(catalog, "MechTypeEq"), Some{1n}) &&
  members(Family.members(catalog, "MechTypeEq"), ["refl", "cast", "symm", "trans"])

def library_client() -> String:
  "def libraryValue : MechNat := libraryCast MechNat MechNat (CastData_refl MechNat) (mechSucc mechZero)\n" ++
  "def librarySelf : MechNat := libraryCast MechNat MechNat (libraryRefl MechNat) mechZero\n" ++
  "mu LibraryIsOne : (0 n : MechNat) -> Type 0 with\n" ++
  "| libraryIsOne : LibraryIsOne (mechSucc mechZero)\n" ++
  "def libraryComputes : LibraryIsOne libraryValue := libraryIsOne\n"

def refusal(globals: G.T, source: String, +expected: String) -> Result<&2, &2, String, Unit>:
  Audit.refusal(Elab.check_in(globals, B.unlimited(), source), expected, "expected refusal: " ++ expected)

def negatives(+installed: G.T, initial: G.T) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- refusal(installed,
      "def wrongEndpoint : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) mechZero (mechSucc mechZero) (TransportData_refl MechNat mechZero) mechZero\n",
      "mismatch: the term has type (Lan SMu TransportData")
    checked : Unit <- refusal(installed,
      "def wrongTypes : MechUnit := CastData_cast MechNat MechUnit (CastData_refl MechNat) mechZero\n",
      "mismatch: the term has type (Lan SMu CastData")
    checked : Unit <- refusal(installed,
      "def wrongUniverse : Type 0 := CastData_cast (Type 0) (Type 0) (CastData_refl (Type 0)) MechNat\n",
      "mismatch: the term has type Type 2 and the expected type is Type 1")
    checked : Unit <- refusal(installed,
      "def mixedInstance : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) mechZero mechZero (TransportHigher_refl MechNat mechZero) mechZero\n",
      "mismatch: the term has type (Lan SMu TransportHigher")
    refusal(initial,
      "def unavailable : MechNat := CastData_cast MechNat MechNat (CastData_refl MechNat) mechZero\n",
      "unbound: CastData_cast")

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +catalog : Family.T <- R.lift(Family.T, R.kernel(Family.T, Prelude.catalog(G.empty(), B.unlimited())))
    checked : Unit <- R.assert(inventory(catalog), "polymorphic equality inventory differs")
    source : String <- R.read(root, "prelude/init.mech")
    initial : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +initial : G.T = R.globals(initial)
    +installed : G.T <- R.lift(G.T, F.install_families([
      F.Instance{"MechEq", [F.level(1n), F.level(1n)], "TransportData", ""},
      F.Instance{"MechEq", [F.level(1n), F.level(2n)], "TransportHigher", ""},
      F.Instance{"MechEq", [F.level(0n), F.level(0n)], "TransportProof", ""},
      F.Instance{"MechEq", [F.level(0n), F.level(1n)], "TransportProofData", ""},
      F.Instance{"MechEq", [F.level(1n), F.level(0n)], "TransportToProof", ""},
      F.Instance{"MechTypeEq", [F.level(0n)], "CastData", ""},
      F.Instance{"MechTypeEq", [F.level(1n)], "CastTypes", ""}], catalog, [], initial))
    client : String <- R.read(root, "test/fixtures/prelude/transport.mech")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(installed, client))
    +named : G.T <- R.lift(G.T, R.kernel(G.T, Family.instantiate(R.globals(checked), catalog, B.unlimited(), "MechTypeEq", [F.level(0n)], "NamedCast",
      [C.Pair{"MechTypeEq", "CastData"}], Some{[C.Pair{"refl", "libraryRefl"}, C.Pair{"cast", "libraryCast"}, C.Pair{"symm", "librarySymm"}, C.Pair{"trans", "libraryTrans"}]})))
    checked : Unit <- R.assert(Congruence.absent(P.Family, G.find_family("NamedCast", named)) && Congruence.absent(G.Entry, G.find("NamedCast_cast", named)), "reuse installed a fresh family or a default member name")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(named, library_client()))
    checked : Unit <- R.lift(Unit, F.audit(R.globals(checked)))
    checked : Unit <- R.lift(Unit, negatives(installed, initial))
    checked : Unit <- R.assert(Congruence.absent(G.Entry, G.find("cast", initial)) && Congruence.absent(P.Family, G.find_family("MechTypeEq", initial)), "template checking changed ordinary globals")
    return "templates=2 instances=7 negatives=5"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-TRANSPORT", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
