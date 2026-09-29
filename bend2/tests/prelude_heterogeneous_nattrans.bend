import Base
import ./relational_common.bend as R
import ./relational_negatives.bend as Neg
import ./relational_program.bend as Program
import ./relational_text.bend as Text
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../kernel/error.bend as E
import ../kernel/budget.bend as B
import ../surface/syntax.bend as S
import ../surface/elab.bend as Elab

def expected_families() -> List<&2, String>:
  ["Equal_Base_Source", "Equal_Base_Target", "Mixed_Base_Source", "Mixed_Base_Target", "Path", "Point", "Run_Base_Source", "Run_Base_Target", "Wide_Base_Source", "Wide_Base_Target"]

def negatives() -> List<&2, String>:
  ["erased-object", "missing-law", "nominal-category", "nominal-equality", "target-record", "wrong-endpoint", "wrong-middle", "wrong-nat-sort", "wrong-naturality", "wrong-source-level", "wrong-target-level"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"componentValue", R.natural(37)}, C.Pair{"identityValue", R.natural(42)}, C.Pair{"verticalForward", R.natural(74)}, C.Pair{"verticalReverse", R.natural(37)}, C.Pair{"verticalNested", R.natural(111)}]

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech", "prelude/cat/heterogeneous-nattrans.mech"])
    fixture : String <- R.read_all(root, ["test/fixtures/prelude/heterogeneous-nattrans.mech", "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech"])
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source ++ fixture))
    symbolic : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.assert(R.empty_symbolic(symbolic), "symbolic definitions escaped")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.elaborated(G.initial(), parsed))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, Program.count_initial(checked, 150n, expected_families()))
    ok : Unit <- R.lift(Unit, R.required(R.prefixes(["Mixed", "Wide", "Equal", "Run"], ["NatTrans", "natApp", "naturality", "idNat", "vcomp", "vcompLaw", "targetEqSymm", "Base_Functor", "Base_functorObj", "Base_functorMap", "Base_functorMapId", "Base_functorMapComp", "Base_eqCongr", "Base_Source_Category", "Base_Source_Hom", "Base_Source_id", "Base_Source_comp", "Base_Source_idComp", "Base_Source_compId", "Base_Source_assoc", "Base_Source_eqTrans", "Base_Source_eqCongr", "Base_Target_Category", "Base_Target_Hom", "Base_Target_id", "Base_Target_comp", "Base_Target_idComp", "Base_Target_compId", "Base_Target_assoc", "Base_Target_eqTrans", "Base_Target_eqCongr"]), globals))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- Neg.run(root, "test/neg/heterogeneous-nattrans", negatives(), globals, Neg.Prefix{9n, True{}, False{}})
    ok : Unit <- R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ "specialize MechHeterogeneousNatTrans (0, 1, 2) as Bad\n"), "universe: the family schema MechHeterogeneousNatTrans expects 4 universe arguments, got 3\n", "universe arity"))
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=150 instances=4 computations=5 negatives=13"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-HETEROGENEOUS-NATTRANS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
