import Base
import ./relational_common.bend as R
import ./relational_negatives.bend as Neg
import ./relational_program.bend as Program
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../surface/syntax.bend as S
import ../surface/elab.bend as Elab

def expected_entries() -> List<&2, String>:
  ["Alpha", "AssocLeft", "AssocRight", "Beta", "BetaComponents", "Components", "CongrLeft", "CongrRight", "Constant", "DoubleEnd", "IndexedEnd", "Laws_NatTransEq", "Laws_Ops_Base_Functor", "Laws_Ops_Base_Source_Category", "Laws_Ops_Base_Source_Hom", "Laws_Ops_Base_Source_assoc", "Laws_Ops_Base_Source_comp", "Laws_Ops_Base_Source_compId", "Laws_Ops_Base_Source_eqCongr", "Laws_Ops_Base_Source_eqTrans", "Laws_Ops_Base_Source_id", "Laws_Ops_Base_Source_idComp", "Laws_Ops_Base_Target_Category", "Laws_Ops_Base_Target_Hom", "Laws_Ops_Base_Target_assoc", "Laws_Ops_Base_Target_comp", "Laws_Ops_Base_Target_compId", "Laws_Ops_Base_Target_eqCongr", "Laws_Ops_Base_Target_eqTrans", "Laws_Ops_Base_Target_id", "Laws_Ops_Base_Target_idComp", "Laws_Ops_Base_eqCongr", "Laws_Ops_Base_functorMap", "Laws_Ops_Base_functorMapComp", "Laws_Ops_Base_functorMapId", "Laws_Ops_Base_functorObj", "Laws_Ops_NatTrans", "Laws_Ops_idNat", "Laws_Ops_natApp", "Laws_Ops_naturality", "Laws_Ops_targetEqSymm", "Laws_Ops_vcomp", "Laws_Ops_vcompLaw", "Laws_eqRefl", "Laws_eqSymm", "Laws_eqTrans", "Laws_idVcomp", "Laws_vcompAssoc", "Laws_vcompCongr", "Laws_vcompId", "LeftIdentity", "Mixed_NatTransEq", "Mixed_Ops_Base_Functor", "Mixed_Ops_Base_Source_Category", "Mixed_Ops_Base_Source_Hom", "Mixed_Ops_Base_Source_assoc", "Mixed_Ops_Base_Source_comp", "Mixed_Ops_Base_Source_compId", "Mixed_Ops_Base_Source_eqCongr", "Mixed_Ops_Base_Source_eqTrans", "Mixed_Ops_Base_Source_id", "Mixed_Ops_Base_Source_idComp", "Mixed_Ops_Base_Target_Category", "Mixed_Ops_Base_Target_Hom", "Mixed_Ops_Base_Target_assoc", "Mixed_Ops_Base_Target_comp", "Mixed_Ops_Base_Target_compId", "Mixed_Ops_Base_Target_eqCongr", "Mixed_Ops_Base_Target_eqTrans", "Mixed_Ops_Base_Target_id", "Mixed_Ops_Base_Target_idComp", "Mixed_Ops_Base_eqCongr", "Mixed_Ops_Base_functorMap", "Mixed_Ops_Base_functorMapComp", "Mixed_Ops_Base_functorMapId", "Mixed_Ops_Base_functorObj", "Mixed_Ops_NatTrans", "Mixed_Ops_idNat", "Mixed_Ops_natApp", "Mixed_Ops_naturality", "Mixed_Ops_targetEqSymm", "Mixed_Ops_vcomp", "Mixed_Ops_vcompLaw", "Mixed_eqRefl", "Mixed_eqSymm", "Mixed_eqTrans", "Mixed_idVcomp", "Mixed_vcompAssoc", "Mixed_vcompCongr", "Mixed_vcompId", "RightIdentity", "Run_Base_Functor", "Run_Base_Source_Category", "Run_Base_Source_Hom", "Run_Base_Source_assoc", "Run_Base_Source_comp", "Run_Base_Source_compId", "Run_Base_Source_eqCongr", "Run_Base_Source_eqTrans", "Run_Base_Source_id", "Run_Base_Source_idComp", "Run_Base_Target_Category", "Run_Base_Target_Hom", "Run_Base_Target_assoc", "Run_Base_Target_comp", "Run_Base_Target_compId", "Run_Base_Target_eqCongr", "Run_Base_Target_eqTrans", "Run_Base_Target_id", "Run_Base_Target_idComp", "Run_Base_eqCongr", "Run_Base_functorMap", "Run_Base_functorMapComp", "Run_Base_functorMapId", "Run_Base_functorObj", "Run_NatTrans", "Run_idNat", "Run_natApp", "Run_naturality", "Run_targetEqSymm", "Run_vcomp", "Run_vcompLaw", "Separate_NatTransEq", "Separate_Ops_Base_Functor", "Separate_Ops_Base_Source_Category", "Separate_Ops_Base_Source_Hom", "Separate_Ops_Base_Source_assoc", "Separate_Ops_Base_Source_comp", "Separate_Ops_Base_Source_compId", "Separate_Ops_Base_Source_eqCongr", "Separate_Ops_Base_Source_eqTrans", "Separate_Ops_Base_Source_id", "Separate_Ops_Base_Source_idComp", "Separate_Ops_Base_Target_Category", "Separate_Ops_Base_Target_Hom", "Separate_Ops_Base_Target_assoc", "Separate_Ops_Base_Target_comp", "Separate_Ops_Base_Target_compId", "Separate_Ops_Base_Target_eqCongr", "Separate_Ops_Base_Target_eqTrans", "Separate_Ops_Base_Target_id", "Separate_Ops_Base_Target_idComp", "Separate_Ops_Base_eqCongr", "Separate_Ops_Base_functorMap", "Separate_Ops_Base_functorMapComp", "Separate_Ops_Base_functorMapId", "Separate_Ops_Base_functorObj", "Separate_Ops_NatTrans", "Separate_Ops_idNat", "Separate_Ops_natApp", "Separate_Ops_naturality", "Separate_Ops_targetEqSymm", "Separate_Ops_vcomp", "Separate_Ops_vcompLaw", "Separate_eqRefl", "Separate_eqSymm", "Separate_eqTrans", "Separate_idVcomp", "Separate_vcompAssoc", "Separate_vcompCongr", "Separate_vcompId", "Wide_NatTransEq", "Wide_Ops_Base_Functor", "Wide_Ops_Base_Source_Category", "Wide_Ops_Base_Source_Hom", "Wide_Ops_Base_Source_assoc", "Wide_Ops_Base_Source_comp", "Wide_Ops_Base_Source_compId", "Wide_Ops_Base_Source_eqCongr", "Wide_Ops_Base_Source_eqTrans", "Wide_Ops_Base_Source_id", "Wide_Ops_Base_Source_idComp", "Wide_Ops_Base_Target_Category", "Wide_Ops_Base_Target_Hom", "Wide_Ops_Base_Target_assoc", "Wide_Ops_Base_Target_comp", "Wide_Ops_Base_Target_compId", "Wide_Ops_Base_Target_eqCongr", "Wide_Ops_Base_Target_eqTrans", "Wide_Ops_Base_Target_id", "Wide_Ops_Base_Target_idComp", "Wide_Ops_Base_eqCongr", "Wide_Ops_Base_functorMap", "Wide_Ops_Base_functorMapComp", "Wide_Ops_Base_functorMapId", "Wide_Ops_Base_functorObj", "Wide_Ops_NatTrans", "Wide_Ops_idNat", "Wide_Ops_natApp", "Wide_Ops_naturality", "Wide_Ops_targetEqSymm", "Wide_Ops_vcomp", "Wide_Ops_vcompLaw", "Wide_eqRefl", "Wide_eqSymm", "Wide_eqTrans", "Wide_idVcomp", "Wide_vcompAssoc", "Wide_vcompCongr", "Wide_vcompId", "assocLaw", "assocLeftAt", "assocRightAt", "betaComponentLaw", "certifiedComponent", "componentLaw", "componentValue", "congrLaw", "congrLeftAt", "congrRightAt", "heterogeneousNatInput", "identityValue", "leftIdentityAt", "leftIdentityLaw", "mixedAssoc", "mixedComponents", "mixedCongr", "mixedLeftId", "mixedRefl", "mixedRightId", "mixedSymm", "mixedTrans", "naturalityWitness", "pathTrans", "reflLaw", "rightIdentityAt", "rightIdentityLaw", "symmLaw", "transLaw", "verticalForward", "verticalNested", "verticalReverse", "wideAssoc", "wideRelation"]

def expected_families() -> List<&2, String>:
  ["Mixed_Ops_Base_Source", "Mixed_Ops_Base_Target", "Path", "Point", "Run_Base_Source", "Run_Base_Target", "Separate_Ops_Base_Source", "Separate_Ops_Base_Target", "Wide_Ops_Base_Source", "Wide_Ops_Base_Target"]

def negatives() -> List<&2, String>:
  ["false-components", "one-component", "nominal-equality", "wrong-endpoint", "trans-middle", "missing-congruence"]

def values() -> List<&2, Program.Probe>:
  [Program.Probe{"leftIdentityAt", 37, 37}, Program.Probe{"rightIdentityAt", 37, 37}, Program.Probe{"assocLeftAt", 37, 111}, Program.Probe{"assocRightAt", 37, 111}, Program.Probe{"congrLeftAt", 37, 74}, Program.Probe{"congrRightAt", 37, 74}, Program.Probe{"leftIdentityAt", 41, 41}, Program.Probe{"rightIdentityAt", 41, 41}, Program.Probe{"assocLeftAt", 41, 123}, Program.Probe{"assocRightAt", 41, 123}, Program.Probe{"congrLeftAt", 41, 82}, Program.Probe{"congrRightAt", 41, 82}]

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +templates : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech", "prelude/cat/heterogeneous-nattrans.mech", "prelude/cat/nattrans-laws.mech"])
    +fixture : String <- R.read_all(root, ["test/fixtures/prelude/nattrans-laws.mech", "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech", "test/fixtures/prelude/nattrans-laws-runtime.mech"])
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(templates ++ "\n" ++ fixture))
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.elaborated(G.initial(), parsed))
    ok : Unit <- R.lift(Unit, Program.inventory(checked, expected_entries(), expected_families()))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, Program.probes(values(), globals))
    ok : Unit <- Neg.run(root, "test/neg/nattrans-laws", negatives(), globals, Neg.Prefix{40n, False{}, False{}})
    return "entries=234 families=10 computations=12 negatives=6"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-NATTRANS-LAWS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
