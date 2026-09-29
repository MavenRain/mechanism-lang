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
  ["Equal_Source", "Equal_Target", "High", "Mixed_Source", "Mixed_Target", "Run_Source", "Run_Target"]

def negatives() -> List<&2, String>:
  ["erased-object", "missing-law", "nominal-category", "nominal-equality", "wrong-composition-law", "wrong-endpoint", "wrong-identity-law", "wrong-object", "wrong-source-level", "wrong-target-level"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"objectValue", R.natural(39)}, C.Pair{"mapValue", R.natural(40)}, C.Pair{"compositionValue", R.natural(80)}]

def mismatch_pins() -> List<&2, C.Pair<String, String>>:
  [C.Pair{"missing-law", "expected type is (Lan SPi w mapId"}, C.Pair{"nominal-category", "hom (Ran SPi 0 x C (Ran SPi 0 y C Type 1))"}, C.Pair{"nominal-equality", "the term has type (Lan SMu Equal_Source"}, C.Pair{"wrong-composition-law", "the constructor categoryRefl of Mixed_Target"}, C.Pair{"wrong-endpoint", "the term has type (Ran SPi 0 C Type 1 (Ran SPi 0 D Type 3"}, C.Pair{"wrong-identity-law", "the constructor categoryRefl of Run_Target"}, C.Pair{"wrong-object", "the term has type Nat and the expected type is (Lan SMu High"}, C.Pair{"wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1"}, C.Pair{"wrong-target-level", "the term has type (Ran SPi 0 Obj Type 3"}]

def refusal_error(error: E.T, +name: String) -> Result<&2, &2, String, Unit>:
  match error:
    case E.Mismatch{message}: R.require(Bool.and(Bool.not(String.eq(name, "erased-object")), String.contains(message, Text.lookup(mismatch_pins(), name))), name ++ ": wrong mismatch refusal")
    case E.Quantity{message}: R.require(Bool.and(String.eq(name, "erased-object"), String.eq(message, "the erased binder x is read in a runtime position")), name ++ ": wrong quantity refusal")
    case _: Fail{name ++ ": wrong refusal"}

def refusal(result: Result<&2, &2, E.T, Elab.Checked>, name: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{checked}: Fail{name ++ ": expected refusal"}
    case Fail{error}: refusal_error(error, name)

def negative_loop(names: List<&2, String>, +root: String, +globals: G.T) -> R.Task(Unit):
  match names:
    case Nil{}: R.Task.pure(Unit, Unit{})
    case Con{+name, rest}: do R.Task<Unit>:
      source : String <- R.read(root, "test/neg/heterogeneous-functor/" ++ name ++ ".mech")
      ok : Unit <- R.lift(Unit, refusal(Elab.check_in(globals, B.unlimited(), source), name))
      negative_loop(rest, root, globals)

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    core : String <- R.read(root, "prelude/cat/category-core.mech")
    category : String <- R.read(root, "prelude/cat/category.mech")
    ok : Unit <- R.assert(String.eq(Text.first_lines(Text.rename_core(core), 70n), Text.first_lines(category, 70n)), "category-core mirror changed")
    +source : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech"])
    fixture : String <- R.read_all(root, ["test/fixtures/prelude/heterogeneous-functor.mech", "test/fixtures/prelude/heterogeneous-functor-runtime.mech"])
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source ++ fixture))
    symbolic : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.assert(R.empty_symbolic(symbolic), "symbolic definitions escaped")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.elaborated(G.initial(), parsed))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, Program.count_initial(checked, 90n, expected_families()))
    ok : Unit <- R.lift(Unit, R.required(R.prefixes(["Mixed", "Equal", "Run"], ["Functor", "functorObj", "functorMap", "functorMapId", "functorMapComp", "eqCongr", "Source_Category", "Source_Hom", "Source_id", "Source_comp", "Source_idComp", "Source_compId", "Source_assoc", "Source_eqTrans", "Source_eqCongr", "Target_Category", "Target_Hom", "Target_id", "Target_comp", "Target_idComp", "Target_compId", "Target_assoc", "Target_eqTrans", "Target_eqCongr"]), globals))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- R.negative_inventory(root, "test/neg/heterogeneous-functor", negatives())
    ok : Unit <- negative_loop(negatives(), root, globals)
    ok : Unit <- R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ "specialize MechHeterogeneousFunctor (0, 1, 2) as Bad\n"), "universe: the family schema MechHeterogeneousFunctor expects 4 universe arguments, got 3\n", "universe arity"))
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=90 instances=3 computations=3 negatives=12"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-HETEROGENEOUS-FUNCTOR", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
