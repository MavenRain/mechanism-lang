import Base
import ./relational_common.bend as R
import ./surface_foundation_common.bend as F
import ./surface_foundation_data.bend as D
import ../prelude/dependent.bend as Prelude
import ../surface/poly.bend as Poly
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/common.bend as C
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/rules.bend as Rules

def arities(rows: List<&2, C.Pair<String, Nat>>, +catalog: Poly.T) -> Result<&2, &2, String, Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{+name, arity}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- R.require(R.optional_nat(Poly.arity(catalog, name), Some{arity}), "wrong template arity: " ++ name)
        arities(rest, catalog)

def group(+prefix: String, +u: Nat, +v: Nat, w: Nat) -> List<&2, F.Instance>:
  [F.Instance{"MechPi", [F.level(1n+u), F.level(1n+v)], prefix ++ "Pi", ""},
   F.Instance{"MechSigma", [F.level(u), F.level(v)], prefix ++ "Pair", ""},
   F.Instance{"mechSigmaMk", [F.level(u), F.level(v)], prefix ++ "Mk", ""},
   F.Instance{"mechSigmaFst", [F.level(u), F.level(v)], prefix ++ "Fst", ""},
   F.Instance{"mechSigmaSnd", [F.level(u), F.level(v)], prefix ++ "Snd", ""},
   F.Instance{"mechSigmaRec", [F.level(u), F.level(v), F.level(w)], prefix ++ "Rec", prefix}]

def pi(+name: String, u: Nat, v: Nat) -> F.Instance:
  F.Instance{"MechPi", [F.level(u), F.level(v)], name, name}

def instances() -> List<&2, F.Instance>:
  List.append(&2, F.Instance, group("Small", 0n, 0n, 1n),
  List.append(&2, F.Instance, group("Up", 0n, 1n, 2n),
  List.append(&2, F.Instance, group("Down", 1n, 0n, 0n),
  List.append(&2, F.Instance, group("Tall", 1n, 2n, 3n),
  [pi("ProofPi", 0n, 0n), pi("ProofDataPi", 0n, 1n), pi("DataProofPi", 1n, 0n),
   pi("HigherProofPi", 2n, 0n), pi("HigherPi", 2n, 1n)]))))

def check_count(state: C.Pair<G.T, Nat>) -> Result<&2, &2, String, G.T>:
  match state:
    case C.Pair{globals, count}:
      do Result<&2, &2, String, G.T>:
        checked : Unit <- R.require(Nat.is_eq(count, 29n), "wrong instance count")
        Done{globals}

def computations() -> List<&2, C.Pair<String, T.T>>:
  +nat : T.T = T.Lan{S.SMu{"MechNat", []}, Rules.diagram_of([])}
  +trivial : T.T = T.In{S.SMu{"MechTrue", []}, T.ACtor{"mechTrivial"}, []}
  [C.Pair{"pairFirst", F.zero()}, C.Pair{"pairSecond", F.one()},
   C.Pair{"pairRec", T.In{S.SMu{"MechNat", []}, T.ACtor{"mechSucc"}, [F.one()]}},
   C.Pair{"depSecond", F.one()}, C.Pair{"depRec", F.one()},
   C.Pair{"downFirst", nat}, C.Pair{"downSecond", F.one()},
   C.Pair{"upSecond", nat}, C.Pair{"upRec", nat}, C.Pair{"piValue", F.one()},
   C.Pair{"downRec", trivial}, C.Pair{"proofPiValue", trivial}]

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +catalog : Poly.T <- R.lift(Poly.T, R.kernel(Poly.T, Prelude.catalog(G.empty(), B.unlimited())))
    checked : Unit <- R.lift(Unit, F.budget_refusal(Poly.T, B.run_poll(Poly.T, Prelude.catalog_comp(G.empty()), F.exhausted())))
    checked : Unit <- R.lift(Unit, arities([C.Pair{"MechPi", 2n}, C.Pair{"MechSigma", 2n},
      C.Pair{"mechSigmaMk", 2n}, C.Pair{"mechSigmaFst", 2n}, C.Pair{"mechSigmaSnd", 2n}, C.Pair{"mechSigmaRec", 3n}], catalog))
    source : String <- R.read(root, "prelude/init.mech")
    initial : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    state : C.Pair<G.T, Nat> <- R.lift(C.Pair<G.T, Nat>, F.install_terms(instances(), catalog, D.dependent_contracts(), R.globals(initial), 0n))
    installed : G.T <- R.lift(G.T, check_count(state))
    client : String <- R.read(root, "test/fixtures/prelude/dependent.mech")
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(installed, client))
    +globals : G.T = R.globals(checked)
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.lift(Unit, R.computations(computations(), globals))
    checked : Unit <- R.lift(Unit, F.negatives(D.dependent_negatives(), globals))
    return "templates=6 instances=29 computations=12 negatives=6"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-DEPENDENT", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
