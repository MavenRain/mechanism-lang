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
  [C.Pair{"componentValue", R.tiny(1n)}, C.Pair{"identityValue", R.tiny(1n)}, C.Pair{"verticalForward", R.tiny(1n)}, C.Pair{"verticalReverse", R.tiny(0n)}, C.Pair{"verticalNested", R.tiny(2n)}, C.Pair{"rightValue", R.tiny(1n)}, C.Pair{"leftValue", R.tiny(1n)}, C.Pair{"leftOther", R.tiny(0n)}]

def negatives() -> List<&2, String>:
  ["wrong-endpoint", "wrong-naturality", "missing-law", "erased-object", "wrong-middle", "wrong-whisker-endpoint"]

def negative_selected(template: Bool, source: String, globals: G.T, text: String, expected: String, name: String) -> R.Task(Unit):
  match template:
    case True{}: R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ text), expected, name))
    case False{}: R.lift(Unit, R.refuse_exact(Elab.check_in(globals, B.unlimited(), text), expected, name))

def negative(+root: String, source: String, globals: G.T, +name: String) -> R.Task(Unit):
  do R.Task<Unit>:
    text : String <- R.read(root, "test/neg/nattrans/" ++ name ++ ".mech")
    expected : String <- R.read(root, "test/neg/nattrans/" ++ name ++ ".err")
    negative_selected(String.eq(name, "wrong-level-arity"), source, globals, text, expected, name)

def negative_loop(names: List<&2, String>, +root: String, +source: String, +globals: G.T) -> R.Task(Unit):
  match names:
    case Nil{}: R.Task.pure(Unit, Unit{})
    case Con{name, rest}: do R.Task<Unit>:
      ok : Unit <- negative(root, source, globals, name)
      negative_loop(rest, root, source, globals)

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read(root, "prelude/cat/category.mech")
    +client : String <- R.read(root, "test/fixtures/prelude/nattrans.mech")
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source ++ client))
    template : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.assert(R.empty_symbolic(template), "symbolic nattrans escaped")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source ++ client))
    ok : Unit <- R.lift(Unit, R.counted_inventory(checked, 183n, expected_families()))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, R.required(R.prefixes(["Small", "Types", "Up", "Higher"], ["eqSymm", "NatTrans", "natApp", "naturality", "idNat", "vcompLaw", "vcomp", "whiskerRight", "whiskerLeft"]), globals))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- R.negative_inventory(root, "test/neg/nattrans", negatives())
    ok : Unit <- negative_loop(negatives(), root, source, globals)
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=183 instances=4 computations=8 negatives=7"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-NATTRANS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
