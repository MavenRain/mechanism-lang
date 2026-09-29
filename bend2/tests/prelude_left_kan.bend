import Base
import ./relational_common.bend as R
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../kernel/budget.bend as B
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as S

def expected_families() -> List<&2, String>:
  ["Higher", "Path", "Point", "Small", "Tiny", "Types", "Up"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"lanUnitValue", R.tiny(1n)}, C.Pair{"lanMapValue", R.tiny(2n)}, C.Pair{"lanDescValue", R.tiny(1n)}, C.Pair{"lanDescOther", R.tiny(2n)}, C.Pair{"lanDescObject", R.tiny(0n)}, C.Pair{"lanDescSecond", R.tiny(1n)}]

def negatives() -> List<&2, String>:
  ["missing-uniqueness", "wrong-factor", "wrong-uniqueness", "reversed-unit", "erased-cocone", "wrong-order"]

def negative_selected(template: Bool, source: String, globals: G.T, text: String, expected: String, name: String) -> R.Task(Unit):
  match template:
    case True{}: R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ text), expected, name))
    case False{}: R.lift(Unit, R.refuse_exact(Elab.check_in(globals, B.unlimited(), text), expected, name))

def negative(+root: String, source: String, globals: G.T, +name: String) -> R.Task(Unit):
  do R.Task<Unit>:
    text : String <- R.read(root, "test/neg/left-kan/" ++ name ++ ".mech")
    expected : String <- R.read(root, "test/neg/left-kan/" ++ name ++ ".err")
    negative_selected(String.eq(name, "wrong-level-arity"), source, globals, text, expected, name)

def negative_loop(names: List<&2, String>, +root: String, +source: String, +globals: G.T) -> R.Task(Unit):
  match names:
    case Nil{}: R.Task.pure(Unit, Unit{})
    case Con{name, rest}: do R.Task<Unit>:
      ok : Unit <- negative(root, source, globals, name)
      negative_loop(rest, root, source, globals)

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read_all(root, ["prelude/cat/category.mech", "test/fixtures/prelude/nattrans.mech", "test/fixtures/prelude/left-kan-identity.mech", "test/fixtures/prelude/left-kan-common.mech", "test/fixtures/prelude/left-kan.mech"])
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source))
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.lift(Unit, R.counted_inventory(checked, 203n, expected_families()))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, R.required(R.prefixes(["Small", "Types", "Up", "Higher"], ["recordFirst", "recordSecond", "LanCocone", "LanFactor", "LanSolution", "LanTail", "LeftKanExtension", "lanFunctor", "lanTail", "lanUnit", "lanSolve", "lanDesc", "lanFac", "lanUniq", "desc_unique"]), globals))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- R.negative_inventory(root, "test/neg/left-kan", negatives())
    ok : Unit <- negative_loop(negatives(), root, source, globals)
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=203 instances=4 computations=6 negatives=7"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-LEFT-KAN", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
