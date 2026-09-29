import Base
import ./relational_common.bend as R
import ./surface_prenex_common.bend as Test
import ./surface_prenex_exports.bend as Exports
import ./surface_prenex_data.bend as Data
import ./surface_raw_common.bend as Raw
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def exact(label: String, +expected: String, result: Result<&2, &2, E.T, Elab.Checked>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{label ++ ": expected refusal: " ++ expected}
    case Fail{+error}: R.require(String.eq(E.to_string(error), expected), label ++ ": wrong refusal: " ++ E.to_string(error))

def negatives(cases: List<&2, Data.Negative>) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Negative{name, kind, expected, good, source}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- exact(name, expected, Elab.check_in(G.initial(), B.unlimited(), source))
        negatives(rest)

def roundtrip(source: String) -> Result<&2, &2, String, Unit>:
  Exports.roundtrip(source, "parse/print changed a dependency export mapping")

def positives(cases: List<&2, Data.Positive>) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Positive{+source, rows}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- roundtrip(source)
        program : Elab.Checked <- R.checked(G.empty(), source)
        checked : Unit <- R.require(R.empty_symbolic(program), "symbolic declarations escaped composition")
        program : Elab.Checked <- R.checked(G.empty(), source ++ "specialize Pair (0) as Closed")
        checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), rows), "the closed instance rows differ from the chosen names in declaration order")
        positives(rest)

def instance() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +program : Elab.Checked <- R.checked(G.empty(), Data.prenex_dependency_exports_instance())
    +globals : G.T = R.globals(program)
    checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), ["Run_w", "Run_a", "Run_pick"]), "dependency exports changed declaration order")
    checked : Unit <- R.require(Raw.present(P.Family, G.find_family("Run_Local", globals)), "the closed instance lost the prefixed dependency family")
    R.require(Test.absent_entries(["w", "a", "Local_witness", "Local_alias", "Run_Local_witness", "Run_Local_alias"], globals), "default or local export names escaped")

def measured(label: String, expected: Maybe<&2, String>, state: C.Pair<B.T, Result<&2, &2, E.T, Elab.Checked>>) -> Result<&2, &2, String, Nat>:
  match expected state:
    case Some{expected} C.Pair{B.Polls{remaining}, result}:
      do Result<&2, &2, String, Nat>:
        checked : Unit <- exact(label ++ " baseline", expected, result)
        Done{(100000n - remaining : Nat)}
    case None{} C.Pair{B.Polls{remaining}, result}:
      do Result<&2, &2, String, Nat>:
        checked : Elab.Checked <- R.kernel(Elab.Checked, result)
        Done{(100000n - remaining : Nat)}
    case _ _: Fail{"finite poll measurement disappeared"}

def group(body: String) -> String:
  "poly (v) group Pair where " ++ body ++ " end "

def budgets() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +reservation : String = "poly (u) def w : Sort (succ u) := Sort u "
    ceiling : Nat <- Test.polls(reservation ++ Data.prenex_dependency_exports_seed())
    checked : Unit <- Test.exhausted("collision-budget", reservation ++ Data.prenex_dependency_exports_complete(), (ceiling + 3n : Nat))
    baseline : String = Data.prenex_dependency_exports_seed() ++ group(Data.prenex_dependency_exports_dependency() ++ "export ()")
    ceiling : Nat <- measured("composition-budget", Some{"mismatch: every family member needs an export name"}, B.run_state(Elab.Checked, Test.action(G.empty(), baseline), B.of_polls(100000n)))
    Test.exhausted("composition-budget", Data.prenex_dependency_exports_seed() ++ group(Data.prenex_dependency_exports_dependency() ++ "export (witness := Local, alias := a)"), ceiling)

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    checked : Unit <- R.lift(Unit, positives(Data.prenex_dependency_exports_positives()))
    checked : Unit <- R.lift(Unit, instance())
    checked : Unit <- R.lift(Unit, negatives(Data.prenex_dependency_exports_negatives()))
    checked : Unit <- R.lift(Unit, Test.parsers(Data.prenex_dependency_exports_parsers(), True{}))
    checked : Unit <- R.lift(Unit, budgets())
    base : String <- R.read(root, "test/fixtures/prelude/prenex-exports-runtime.mech")
    extra : String <- R.read(root, "test/fixtures/prelude/dependency-exports-runtime.mech")
    +source : String = base ++ extra
    checked : Unit <- R.lift(Unit, roundtrip(source))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.initial(), source))
    +globals : G.T = R.globals(program)
    checked : Unit <- R.assert(R.strings(Elab.disclosure_names(R.rows(program)), []), "dependency exports introduced an axiom")
    checked : Unit <- R.assert(Raw.absent(P.Family, G.find_family("NestedRun_Again", globals)) && Nat.is_eq(Test.family_count(globals), (Test.family_count(G.initial()) + 8n : Nat)), "nested reuse installed fresh families")
    checked : Unit <- R.lift(Unit, R.computations([C.Pair{"dependencyPayload", R.natural(37)}, C.Pair{"nestedPayload", R.natural(37)}], globals))
    return "positives=" ++ Nat.show(R.length(Data.Positive, Data.prenex_dependency_exports_positives())) ++ " negatives=" ++ Nat.show(R.length(Data.Negative, Data.prenex_dependency_exports_negatives())) ++ " parser=" ++ Nat.show(R.length(Data.ParseCase, Data.prenex_dependency_exports_parsers())) ++ " budget=2"

def finish(result: Result<&2, &2, String, String>) -> IO(Unit):
  match result:
    case Done{details}: IO.print("PRENEX-DEPENDENCY-EXPORTS-OK " ++ details)
    case Fail{message}: IO.die(Unit, 1, "PRENEX-DEPENDENCY-EXPORTS FAIL " ++ message)

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(R.root(args))
    finish(result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
