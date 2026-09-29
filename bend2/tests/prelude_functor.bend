import Base
import ./relational_common.bend as R
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../kernel/budget.bend as B
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as S

def expected_families() -> List<&2, String>:
  ["Higher", "Point", "Small", "Tiny", "Types", "Up"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"objectIdentity", R.tiny(1n)}, C.Pair{"objectForward", R.tiny(0n)}, C.Pair{"objectReverse", R.tiny(1n)}, C.Pair{"mapIdentity", R.tiny(1n)}, C.Pair{"mapForward", R.tiny(1n)}, C.Pair{"mapReverse", R.tiny(0n)}, C.Pair{"mapNested", R.tiny(2n)}]

def negatives() -> List<&2, String>:
  ["wrong-object", "wrong-endpoint", "wrong-identity-law", "wrong-composition-law", "missing-law", "erased-object", "wrong-level-arity"]

def negative_selected(template: Bool, source: String, globals: G.T, text: String, expected: String, name: String) -> R.Task(Unit):
  match template:
    case True{}: R.lift(Unit, R.refuse_exact(Elab.check_in(G.empty(), B.unlimited(), source ++ text), expected, name))
    case False{}: R.lift(Unit, R.refuse_exact(Elab.check_in(globals, B.unlimited(), text), expected, name))

def negative(+root: String, source: String, globals: G.T, +name: String) -> R.Task(Unit):
  do R.Task<Unit>:
    text : String <- R.read(root, "test/neg/functor/" ++ name ++ ".mech")
    expected : String <- R.read(root, "test/neg/functor/" ++ name ++ ".err")
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
    +client : String <- R.read(root, "test/fixtures/prelude/functor.mech")
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.parse_roundtrip(source ++ client))
    template : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    ok : Unit <- R.assert(R.empty_symbolic(template), "symbolic functor escaped")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source ++ client))
    ok : Unit <- R.lift(Unit, R.counted_inventory(checked, 177n, expected_families()))
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, R.required(R.prefixes(["Small", "Types", "Up", "Higher"], ["eqTrans", "eqCongr", "Functor", "functorObj", "functorMap", "functorMapId", "functorMapComp", "idFunctor", "compFunctor"]), globals))
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- R.negative_inventory(root, "test/neg/functor", negatives())
    ok : Unit <- negative_loop(negatives(), root, source, globals)
    ok : Unit <- R.lift(Unit, R.budget_refusal(source))
    return "entries=177 instances=4 computations=7 negatives=8"

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRELUDE-FUNCTOR", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
