import Base
import ./relational_common.bend as R
import ./relational_syntax_equal.bend as Equal
import ./surface_prenex_common.bend as Test
import ./surface_prenex_data.bend as Data
import ./surface_raw_common.bend as Raw
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/budget.bend as B

def roundtrip(+source: String, message: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +parsed : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(source))
    reparsed : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(Syntax.print(parsed)))
    R.require(Equal.declarations(parsed, reparsed), message)

def positives(cases: List<&2, Data.Positive>) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Positive{+source, rows}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- roundtrip(source, "parse/print changed the export mapping")
        checked : Elab.Checked <- R.checked(G.empty(), source)
        positives(rest)

def names() -> List<&2, String>:
  ["Shared_unbox", "Shared_pack", "Shared_unpack", "freshUnbox", "freshPack", "freshUnpack", "sharedUnbox", "sharedPack", "sharedUnpack", "exportInput", "namedPayload", "reusedPayload"]

def budgets() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +declaration : Nat <- Test.polls(Data.prenex_exports_group())
    checked : Unit <- Test.exhausted("instance-budget", Data.prenex_exports_instance(), declaration)
    +export_polls : Nat <- Test.polls(Data.prenex_exports_instance())
    checked : Unit <- R.require(Nat.is_lt(declaration, export_polls), "the export specialization consumed no budget")
    checked : Unit <- Test.exhausted("late-budget", Data.prenex_exports_instance(), (export_polls - 1n : Nat))
    +reservation : String = "poly (v) def chosenWitness : Sort (succ v) := Sort v "
    reserved : Nat <- Test.polls(reservation ++ Data.prenex_exports_group())
    Test.exhausted("collision-budget", reservation ++ Data.prenex_exports_instance(), reserved)

def composed() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +program : Elab.Checked <- R.checked(G.empty(), Data.prenex_exports_composed_instance())
    checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), ["chosenWitness", "chosenLocalAlias", "chosenPick"]), "composed export rows or declaration order changed")
    R.require(Test.absent_entries(["Inst_Local_witness", "Inst_Local_alias", "Inst_pick"], R.globals(program)), "composed default member names leaked")

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read(root, "test/fixtures/prelude/prenex-exports-runtime.mech")
    checked : Unit <- R.lift(Unit, roundtrip(source, "parse/print changed the export mapping"))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.initial(), source))
    +globals : G.T = R.globals(program)
    checked : Unit <- R.assert(R.strings(R.entry_names(R.rows(program)), names()), "export row names or declaration order changed")
    checked : Unit <- R.assert(Test.absent_entries(["Named_unbox", "Named_pack", "Named_unpack", "Reused_unbox", "Reused_pack", "Reused_unpack"], globals), "default member names leaked")
    checked : Unit <- R.assert(Raw.absent(P.Family, G.find_family("Reused", globals)) && Raw.absent(P.Family, G.find_family("Reused_Wrapped", globals)) && Nat.is_eq(Test.family_count(globals), (Test.family_count(G.initial()) + 4n : Nat)), "reuse installed a fresh family")
    checked : Unit <- R.assert(R.strings(Elab.disclosure_names(R.rows(program)), []), "exported members introduced an axiom")
    checked : Unit <- R.lift(Unit, R.computations([C.Pair{"namedPayload", R.natural(37)}, C.Pair{"reusedPayload", R.natural(37)}], globals))
    checked : Unit <- R.lift(Unit, composed())
    checked : Unit <- R.lift(Unit, positives(Data.prenex_exports_positives()))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.prenex_exports_negatives(), G.empty()))
    checked : Unit <- R.lift(Unit, Test.parsers(Data.prenex_exports_parsers(), True{}))
    checked : Unit <- R.lift(Unit, budgets())
    return "entries=" ++ Nat.show(R.length(String, R.entry_names(R.rows(program)))) ++ " positives=" ++ Nat.show(R.length(Data.Positive, Data.prenex_exports_positives())) ++ " negatives=" ++ Nat.show(R.length(Data.Negative, Data.prenex_exports_negatives())) ++ " parser=" ++ Nat.show(R.length(Data.ParseCase, Data.prenex_exports_parsers())) ++ " budget=3"

def finish(result: Result<&2, &2, String, String>) -> IO(Unit):
  match result:
    case Done{details}: IO.print("PRENEX-EXPORTS-OK " ++ details)
    case Fail{message}: IO.die(Unit, 1, "PRENEX-EXPORTS FAIL " ++ message)

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(R.root(args))
    finish(result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
