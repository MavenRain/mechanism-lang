import Base
import ./relational_common.bend as R
import ./surface_foundation_common.bend as F
import ./surface_foundation_data.bend as D
import ../prelude/equality.bend as Prelude
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/common.bend as C
import ../kernel/shape.bend as S
import ../kernel/term.bend as T

def equality(+alias: String, u: Nat, v: Nat) -> F.Instance:
  F.Instance{"MechEq", [F.level(u), F.level(v)], alias, alias}

def cast(+alias: String, u: Nat) -> F.Instance:
  F.Instance{"MechTypeEq", [F.level(u)], alias, alias}

def computations() -> List<&2, C.Pair<String, T.T>>:
  List.append(&2, C.Pair<String, T.T>,
  F.ones(["opsSymm", "opsTrans", "opsCongr", "opsJ", "opsProofDataJ",
    "opsCastSymm", "opsCastTrans", "opsHigherValue", "opsTypesValue", "opsCastHigherValue"]),
  [C.Pair{"opsDependent", T.In{S.SMu{"OpsFiber", [F.zero(),
    T.In{S.SMu{"OpsData", [F.zero()]}, T.ACtor{"mechReflCtor"}, []}]},
    T.ACtor{"opsFiber"}, []}}])

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    catalog : Family.T <- R.lift(Family.T, R.kernel(Family.T, Prelude.catalog(G.empty(), B.unlimited())))
    source : String <- R.read(root, "prelude/init.mech")
    initial : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    installed : G.T <- R.lift(G.T, F.install_families([
      equality("OpsData", 1n, 1n), equality("OpsProof", 0n, 0n),
      equality("OpsProofData", 0n, 1n), equality("OpsToProof", 1n, 0n),
      equality("OpsHigher", 1n, 2n), equality("OpsTypes", 2n, 2n),
      equality("OpsTall", 3n, 1n), cast("OpsCast", 0n),
      cast("OpsCastHigher", 1n), cast("OpsCastTall", 2n)],
      catalog, D.equality_ops_contracts(), R.globals(initial)))
    client : String <- R.read(root, "test/fixtures/prelude/equality-ops.mech")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(installed, client))
    +globals : G.T = R.globals(checked)
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.lift(Unit, R.computations(computations(), globals))
    checked : Unit <- R.lift(Unit, F.negatives(D.equality_ops_negatives(), globals))
    return "instances=10 computations=11 negatives=8"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-EQUALITY-OPS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
