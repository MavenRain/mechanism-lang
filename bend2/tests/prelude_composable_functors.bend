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
  ["Equal_Middle", "Equal_Source", "Equal_Target", "High", "Higher", "Mixed_Middle", "Mixed_Source", "Mixed_Target", "Run_Middle", "Run_Source", "Run_Target"]

def negatives() -> List<&2, String>:
  ["erased-object", "middle-record", "missing-law", "nominal-category", "nominal-equality", "reversed-functors", "wrong-identity-law", "wrong-middle-level", "wrong-source-level", "wrong-target-level"]

def expected_entries() -> List<&2, String>:
  ["Composed", "Equal_Composite_Functor", "Equal_Composite_eqCongr", "Equal_Composite_functorMap", "Equal_Composite_functorMapComp", "Equal_Composite_functorMapId", "Equal_Composite_functorObj", "Equal_First_Functor", "Equal_First_eqCongr", "Equal_First_functorMap", "Equal_First_functorMapComp", "Equal_First_functorMapId", "Equal_First_functorObj", "Equal_Middle_Category", "Equal_Middle_Hom", "Equal_Middle_assoc", "Equal_Middle_comp", "Equal_Middle_compId", "Equal_Middle_eqCongr", "Equal_Middle_eqTrans", "Equal_Middle_id", "Equal_Middle_idComp", "Equal_Second_Functor", "Equal_Second_eqCongr", "Equal_Second_functorMap", "Equal_Second_functorMapComp", "Equal_Second_functorMapId", "Equal_Second_functorObj", "Equal_Source_Category", "Equal_Source_Hom", "Equal_Source_assoc", "Equal_Source_comp", "Equal_Source_compId", "Equal_Source_eqCongr", "Equal_Source_eqTrans", "Equal_Source_id", "Equal_Source_idComp", "Equal_Target_Category", "Equal_Target_Hom", "Equal_Target_assoc", "Equal_Target_comp", "Equal_Target_compId", "Equal_Target_eqCongr", "Equal_Target_eqTrans", "Equal_Target_id", "Equal_Target_idComp", "Equal_compFunctor", "Lift", "MiddleCategory", "Mixed_Composite_Functor", "Mixed_Composite_eqCongr", "Mixed_Composite_functorMap", "Mixed_Composite_functorMapComp", "Mixed_Composite_functorMapId", "Mixed_Composite_functorObj", "Mixed_First_Functor", "Mixed_First_eqCongr", "Mixed_First_functorMap", "Mixed_First_functorMapComp", "Mixed_First_functorMapId", "Mixed_First_functorObj", "Mixed_Middle_Category", "Mixed_Middle_Hom", "Mixed_Middle_assoc", "Mixed_Middle_comp", "Mixed_Middle_compId", "Mixed_Middle_eqCongr", "Mixed_Middle_eqTrans", "Mixed_Middle_id", "Mixed_Middle_idComp", "Mixed_Second_Functor", "Mixed_Second_eqCongr", "Mixed_Second_functorMap", "Mixed_Second_functorMapComp", "Mixed_Second_functorMapId", "Mixed_Second_functorObj", "Mixed_Source_Category", "Mixed_Source_Hom", "Mixed_Source_assoc", "Mixed_Source_comp", "Mixed_Source_compId", "Mixed_Source_eqCongr", "Mixed_Source_eqTrans", "Mixed_Source_id", "Mixed_Source_idComp", "Mixed_Target_Category", "Mixed_Target_Hom", "Mixed_Target_assoc", "Mixed_Target_comp", "Mixed_Target_compId", "Mixed_Target_eqCongr", "Mixed_Target_eqTrans", "Mixed_Target_id", "Mixed_Target_idComp", "Mixed_compFunctor", "Raise", "Run_Composite_Functor", "Run_Composite_eqCongr", "Run_Composite_functorMap", "Run_Composite_functorMapComp", "Run_Composite_functorMapId", "Run_Composite_functorObj", "Run_First_Functor", "Run_First_eqCongr", "Run_First_functorMap", "Run_First_functorMapComp", "Run_First_functorMapId", "Run_First_functorObj", "Run_Middle_Category", "Run_Middle_Hom", "Run_Middle_assoc", "Run_Middle_comp", "Run_Middle_compId", "Run_Middle_eqCongr", "Run_Middle_eqTrans", "Run_Middle_id", "Run_Middle_idComp", "Run_Second_Functor", "Run_Second_eqCongr", "Run_Second_functorMap", "Run_Second_functorMapComp", "Run_Second_functorMapId", "Run_Second_functorObj", "Run_Source_Category", "Run_Source_Hom", "Run_Source_assoc", "Run_Source_comp", "Run_Source_compId", "Run_Source_eqCongr", "Run_Source_eqTrans", "Run_Source_id", "Run_Source_idComp", "Run_Target_Category", "Run_Target_Hom", "Run_Target_assoc", "Run_Target_comp", "Run_Target_compId", "Run_Target_eqCongr", "Run_Target_eqTrans", "Run_Target_id", "Run_Target_idComp", "Run_compFunctor", "SourceCategory", "TargetCategory", "composableInput", "compositeCompositionLaw", "compositeIdentityLaw", "compositionValue", "identityValue", "mapValue", "mixedCompositeSort", "mixedComposition", "mixedFirstSort", "mixedMiddleHom", "mixedSecondSort", "mixedSourceHom", "mixedTargetHom", "objectValue"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"objectValue", R.natural(78)}, C.Pair{"mapValue", R.natural(40)}, C.Pair{"compositionValue", R.natural(80)}, C.Pair{"identityValue", R.natural(37)}]

def mismatch_pins() -> List<&2, C.Pair<String, String>>:
  [C.Pair{"middle-record", "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D (Ran SPi 0 y D (Ran SPi w _ (Out SPi 0 y D (APt 0 y) (Out SPi 0 x D (APt 0 x) (Elim SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 4)) other as self"}, C.Pair{"missing-law", "expected type is (Lan SPi w map "}, C.Pair{"nominal-category", "Equal_Source"}, C.Pair{"nominal-equality", "Equal_Middle"}, C.Pair{"reversed-functors", "the term has type (Lan SPi w obj (Ran SPi w _ D E) (Lan SPi w map (Ran SPi 0 x D (Ran SPi 0 y D (Ran SPi w _ (Out SPi 0 y D (APt 0 y) (Out SPi 0 x D (APt 0 x) (Elim SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type 1)) d as self return (Ran SPi 0 x D (Ran SPi 0 y D Type 1)) with | (ALeg 0) x y => x))) (Out SPi 0 y E (APt 0 (Out SPi w _ D (APt w y) obj)) (Out SPi 0 x E (APt 0 (Out SPi w _ D (APt w x) obj)) (Elim SPi w hom (Ran SPi 0 x E (Ran SPi 0 y E Type 1)) e as self return (Ran SPi 0 x E (Ran SPi 0 y E Type 1)) with | (ALeg 0) x y => x)))))) (Lan SPi w mapId (Ran SPi 0 x D (Lan SMu Equal_Target ["}, C.Pair{"wrong-identity-law", "the constructor categoryRefl of Run_Target"}, C.Pair{"wrong-middle-level", "the term has type (Ran SPi 0 Obj Type 3"}, C.Pair{"wrong-source-level", "the term has type (Ran SPi 0 Obj Type 1"}, C.Pair{"wrong-target-level", "the term has type (Ran SPi 0 Obj Type 5"}]

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
      source : String <- R.read(root, "test/neg/composable-functors/" ++ name ++ ".mech")
      ok : Unit <- R.lift(Unit, refusal(Elab.check_in(globals, B.unlimited(), source), name))
      negative_loop(rest, root, globals)

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    canonical : String <- R.read(root, "prelude/cat/heterogeneous-functor.mech")
    +target : String <- R.read(root, "prelude/cat/composable-functors.mech")
    +body : String = Text.member_body(canonical, "def Functor ", "end")
    ok : Unit <- R.assert(Nat.is_gt(String.length(body), 1n), "missing canonical functor members")
    ok : Unit <- R.assert(String.contains(target, Text.rename_words(body, [C.Pair{"u", "u"}, C.Pair{"v", "v"}, C.Pair{"w", "w"}, C.Pair{"z", "z"}], [C.Pair{"Functor", "First_Functor"}, C.Pair{"functorObj", "First_functorObj"}, C.Pair{"functorMap", "First_functorMap"}, C.Pair{"functorMapId", "First_functorMapId"}, C.Pair{"functorMapComp", "First_functorMapComp"}, C.Pair{"eqCongr", "First_eqCongr"}, C.Pair{"Source", "Source"}, C.Pair{"Source_Category", "Source_Category"}, C.Pair{"Source_Hom", "Source_Hom"}, C.Pair{"Source_id", "Source_id"}, C.Pair{"Source_comp", "Source_comp"}, C.Pair{"Source_idComp", "Source_idComp"}, C.Pair{"Source_compId", "Source_compId"}, C.Pair{"Source_assoc", "Source_assoc"}, C.Pair{"Source_eqTrans", "Source_eqTrans"}, C.Pair{"Source_eqCongr", "Source_eqCongr"}, C.Pair{"Target", "Middle"}, C.Pair{"Target_Category", "Middle_Category"}, C.Pair{"Target_Hom", "Middle_Hom"}, C.Pair{"Target_id", "Middle_id"}, C.Pair{"Target_comp", "Middle_comp"}, C.Pair{"Target_idComp", "Middle_idComp"}, C.Pair{"Target_compId", "Middle_compId"}, C.Pair{"Target_assoc", "Middle_assoc"}, C.Pair{"Target_eqTrans", "Middle_eqTrans"}, C.Pair{"Target_eqCongr", "Middle_eqCongr"}])), "functor mirror changed: First")
    ok : Unit <- R.assert(String.contains(target, Text.rename_words(body, [C.Pair{"u", "w"}, C.Pair{"v", "z"}, C.Pair{"w", "p"}, C.Pair{"z", "q"}], [C.Pair{"Functor", "Second_Functor"}, C.Pair{"functorObj", "Second_functorObj"}, C.Pair{"functorMap", "Second_functorMap"}, C.Pair{"functorMapId", "Second_functorMapId"}, C.Pair{"functorMapComp", "Second_functorMapComp"}, C.Pair{"eqCongr", "Second_eqCongr"}, C.Pair{"Source", "Middle"}, C.Pair{"Source_Category", "Middle_Category"}, C.Pair{"Source_Hom", "Middle_Hom"}, C.Pair{"Source_id", "Middle_id"}, C.Pair{"Source_comp", "Middle_comp"}, C.Pair{"Source_idComp", "Middle_idComp"}, C.Pair{"Source_compId", "Middle_compId"}, C.Pair{"Source_assoc", "Middle_assoc"}, C.Pair{"Source_eqTrans", "Middle_eqTrans"}, C.Pair{"Source_eqCongr", "Middle_eqCongr"}, C.Pair{"Target", "Target"}, C.Pair{"Target_Category", "Target_Category"}, C.Pair{"Target_Hom", "Target_Hom"}, C.Pair{"Target_id", "Target_id"}, C.Pair{"Target_comp", "Target_comp"}, C.Pair{"Target_idComp", "Target_idComp"}, C.Pair{"Target_compId", "Target_compId"}, C.Pair{"Target_assoc", "Target_assoc"}, C.Pair{"Target_eqTrans", "Target_eqTrans"}, C.Pair{"Target_eqCongr", "Target_eqCongr"}])), "functor mirror changed: Second")
    ok : Unit <- R.assert(String.contains(target, Text.rename_words(body, [C.Pair{"u", "u"}, C.Pair{"v", "v"}, C.Pair{"w", "p"}, C.Pair{"z", "q"}], [C.Pair{"Functor", "Composite_Functor"}, C.Pair{"functorObj", "Composite_functorObj"}, C.Pair{"functorMap", "Composite_functorMap"}, C.Pair{"functorMapId", "Composite_functorMapId"}, C.Pair{"functorMapComp", "Composite_functorMapComp"}, C.Pair{"eqCongr", "Composite_eqCongr"}, C.Pair{"Source", "Source"}, C.Pair{"Source_Category", "Source_Category"}, C.Pair{"Source_Hom", "Source_Hom"}, C.Pair{"Source_id", "Source_id"}, C.Pair{"Source_comp", "Source_comp"}, C.Pair{"Source_idComp", "Source_idComp"}, C.Pair{"Source_compId", "Source_compId"}, C.Pair{"Source_assoc", "Source_assoc"}, C.Pair{"Source_eqTrans", "Source_eqTrans"}, C.Pair{"Source_eqCongr", "Source_eqCongr"}, C.Pair{"Target", "Target"}, C.Pair{"Target_Category", "Target_Category"}, C.Pair{"Target_Hom", "Target_Hom"}, C.Pair{"Target_id", "Target_id"}, C.Pair{"Target_comp", "Target_comp"}, C.Pair{"Target_idComp", "Target_idComp"}, C.Pair{"Target_compId", "Target_compId"}, C.Pair{"Target_assoc", "Target_assoc"}, C.Pair{"Target_eqTrans", "Target_eqTrans"}, C.Pair{"Target_eqCongr", "Target_eqCongr"}])), "functor mirror changed: Composite")
    +source : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/composable-functors.mech"])
    fixture : String <- R.read_all(root, ["test/fixtures/prelude/composable-functors.mech", "test/fixtures/prelude/composable-functors-runtime.mech"])
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source ++ fixture))
    symbolic : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.assert(R.empty_symbolic(symbolic), "symbolic definitions escaped")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.elaborated(G.initial(), parsed))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, Program.inventory(checked, expected_entries(), expected_families()))
    ok : Unit <- R.lift(Unit, Program.global_inventory(globals, expected_entries()))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- R.negative_inventory(root, "test/neg/composable-functors", negatives())
    ok : Unit <- negative_loop(negatives(), root, globals)
    ok : Unit <- R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ "specialize MechComposableFunctors (0, 1, 2, 3, 4) as Bad\n"), "universe: the family schema MechComposableFunctors expects 6 universe arguments, got 5\n", "universe arity"))
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=158 instances=3 computations=4 negatives=12"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-COMPOSABLE-FUNCTORS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
