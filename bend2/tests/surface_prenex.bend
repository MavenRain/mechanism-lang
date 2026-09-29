import Base
import ./relational_common.bend as R
import ./surface_prenex_common.bend as Test
import ./surface_prenex_data.bend as Data
import ./surface_foundation_common.bend as F
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/rules.bend as Rules
import ../kernel/budget.bend as B

def rows() -> List<&2, String>:
  ["proofIdentity", "dataIdentity", "typeIdentity", "againIdentity", "dataValue", "againValue", "proofValue", "typeValue", "predicateType", "higherType", "predicateWitness", "higherWitness", "joined", "joinedWitness", "applied", "appliedValue"]

def zero() -> T.T:
  T.In{S.SMu{"Tiny", []}, T.ACtor{"tinyZero"}, []}

def succ(value: T.T) -> T.T:
  T.In{S.SMu{"Tiny", []}, T.ACtor{"tinySucc"}, [value]}

def tiny() -> T.T:
  T.Lan{S.SMu{"Tiny", []}, Rules.diagram_of([])}

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read(root, "test/fixtures/prelude/prenex.mech")
    tree : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.parse_roundtrip(source))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +globals : G.T = R.globals(program)
    checked : Unit <- R.assert(Test.absent_entries(["identity", "functionType", "joinedSort", "appliedIdentity"], globals), "symbolic template escaped")
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.assert(R.strings(R.entry_names(R.rows(program)), rows()), "row order changed")
    checked : Unit <- R.lift(Unit, R.computations([C.Pair{"dataValue", succ(zero())}, C.Pair{"againValue", succ(succ(zero()))}, C.Pair{"typeValue", tiny()}, C.Pair{"appliedValue", succ(zero())}], globals))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.prenex_negatives(), G.empty()))
    checked : Unit <- R.lift(Unit, Test.exhausted("budget", Data.prenex_identity(), 0n))
    checked : Unit <- R.lift(Unit, Test.refusal(Elab.Checked, "catalog-lifetime", "unbound", "unknown universe schema identity", Elab.check_in(globals, B.unlimited(), "specialize identity (1) as later")))
    +large : String = "def p : Sort (succ (succ 999999999999999999)) := Sort (succ 999999999999999999)"
    tree : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.parse_roundtrip(large))
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), large))
    return "entries=" ++ Nat.show(F.entry_count(globals)) ++ " computations=4 negatives=" ++ Nat.show((R.length(Data.Negative, Data.prenex_negatives()) + 2n : Nat))

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRENEX", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
