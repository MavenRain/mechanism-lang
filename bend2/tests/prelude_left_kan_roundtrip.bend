import Base
import ./relational_common.bend as R
import ./relational_negatives.bend as Neg
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../kernel/budget.bend as Budget
import ../surface/elab.bend as Elab

def required() -> List<&2, String>:
  ["Laws_postCoconeDesc", "Laws_descPostCocone", "WidePostCoconeDescContract", "WideDescPostCoconeContract"]

def require_rows(names: List<&2, String>, +rows: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{+name, rest}: do Result<&2, &2, String, Unit>:
      ok : Unit <- R.require(R.contains(rows, name), "missing round-trip contract: " ++ name)
      require_rows(rest, rows)

def negatives() -> List<&2, String>:
  ["cocone-missing-solution", "cocone-wrong-solution", "desc-wrong-transformation", "desc-nominal-equality"]

def values() -> List<&2, C.Pair<String, T.T>>:
  [C.Pair{"lanRoundtripCocone", R.natural(3822)}, C.Pair{"lanRoundtripCoconeReference", R.natural(3822)}, C.Pair{"lanRoundtripOtherCocone", R.natural(4937)}, C.Pair{"lanRoundtripOtherCoconeReference", R.natural(4937)}, C.Pair{"lanRoundtripDesc", R.natural(3822)}, C.Pair{"lanRoundtripDescReference", R.natural(3822)}, C.Pair{"lanRoundtripOtherDesc", R.natural(4937)}, C.Pair{"lanRoundtripOtherDescReference", R.natural(4937)}]

def checked(+root: String) -> R.Task(Elab.Checked):
  do R.Task<Elab.Checked>:
    +source : String <- R.read(root, "_bend2/left-kan-roundtrip-contracts/focused-source.mech")
    +checked : Elab.Checked <- R.lift(Elab.Checked, R.kernel(Elab.Checked, Elab.check_in(G.initial(), Budget.unlimited(), source)))
    ok : Unit <- R.lift(Unit, require_rows(required(), R.entry_names(R.rows(checked))))
    ok : Unit <- R.lift(Unit, R.require(R.strings(Elab.axiom_names(R.rows(checked)), []), "round-trip laws introduced an axiom"))
    return checked

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    +checked : Elab.Checked <- checked(root)
    +globals : G.T = R.globals(checked)
    ok : Unit <- R.lift(Unit, R.computations(values(), globals))
    ok : Unit <- Neg.run(root, "test/neg/left-kan-roundtrip", negatives(), globals, Neg.Prefix{64n, True{}, False{}})
    return "laws=2 contracts=2 computations=8 negatives=4"

def oracles(+root: String, names: List<&2, String>, +globals: G.T) -> R.Task(String):
  match names:
    case Nil{}: R.Task.pure(String, "")
    case Con{+name, rest}: do R.Task<String>:
      +source : String <- R.read(root, "test/neg/left-kan-roundtrip/" ++ name ++ ".mech")
      +message : String <- R.lift(String, Neg.message(Elab.check_in(globals, Budget.unlimited(), source), name))
      +tail : String <- oracles(root, rest, globals)
      return name ++ "\t" ++ message ++ "\n" ++ tail

def record(+root: String) -> R.Task(String):
  do R.Task<String>:
    +checked : Elab.Checked <- checked(root)
    oracles(root, negatives(), R.globals(checked))

def selected(is_record: Bool, +root: String) -> IO(Unit):
  match is_record:
    case True{}: R.run("ROUNDTRIP-ORACLES", record(root))
    case False{}: IO.die(Unit, 64, "expected ROOT [--record-oracles]")

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{root, Con{+flag, Nil{}}}: selected(String.eq(flag, "--record-oracles"), root)
    case other: R.run("PRELUDE-LEFT-KAN-ROUNDTRIP", suite(R.root(other)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
