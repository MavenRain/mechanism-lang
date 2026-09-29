import Base
import ./relational_common.bend as R
import ./surface_foundation_common.bend as F
import ./surface_foundation_data.bend as D
import ../prelude/congruence.bend as Prelude
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/positivity.bend as P

def members(value: Maybe<&2, List<&2, String>>) -> Bool:
  match value:
    case Some{names}: R.strings(names, ["congr"])
    case None{}: False{}

def absent(-A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case None{}: True{}
    case Some{entry}: False{}

def instance(+alias: String, u: Nat, v: Nat) -> F.Instance:
  F.Instance{"MechCongr", [F.level(u), F.level(v)], alias, alias}

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +catalog : Family.T <- R.lift(Family.T, R.kernel(Family.T, Prelude.catalog(G.empty(), B.unlimited())))
    checked : Unit <- R.assert(R.optional_nat(Family.arity(catalog, "MechCongr"), Some{2n}) && members(Family.members(catalog, "MechCongr")) && absent(Nat, Family.arity(catalog, "Result")), "catalog inventory differs")
    checked : Unit <- R.lift(Unit, F.budget_refusal(Family.T, B.run_poll(Family.T, Prelude.catalog_comp(G.empty()), F.exhausted())))
    source : String <- R.read(root, "prelude/init.mech")
    initial : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +installed : G.T <- R.lift(G.T, F.install_families([
      instance("Same", 1n, 1n), instance("Again", 1n, 1n),
      instance("Up", 1n, 2n), instance("Down", 2n, 1n),
      instance("Proof", 0n, 0n), instance("ProofData", 0n, 1n),
      instance("DataProof", 1n, 0n), instance("Tall", 3n, 0n)],
      catalog, D.congruence_contracts(), R.globals(initial)))
    checked : Unit <- R.assert(absent(G.Entry, G.find("congr", installed)) && absent(P.Family, G.find_family("MechCongr", installed)) && absent(P.Family, G.find_family("Result", installed)), "symbolic families escaped")
    client : String <- R.read(root, "test/fixtures/prelude/congruence.mech")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(installed, client))
    +globals : G.T = R.globals(checked)
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.lift(Unit, R.computations(F.ones(["sameValue", "upValue", "downValue", "proofDataValue"]), globals))
    checked : Unit <- R.lift(Unit, F.negatives(D.congruence_negatives(), globals))
    return "instances=8 computations=4 negatives=5"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-CONGRUENCE", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
