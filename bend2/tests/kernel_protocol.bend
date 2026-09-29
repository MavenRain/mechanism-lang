import Base
import ./relational_common.bend as R
import ./relational_syntax_equal.bend as Same
import ./kernel_recursive.bend as Recursive
import ./kernel_erase_driver.bend as Erasure
import ../io.bend as FileIO
import ../cli/output.bend as Output
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../surface/elab.bend as Elab
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E
import ../kernel/global.bend as G
import ../kernel/check.bend as Check
import ../kernel/erase.bend as Erase
import ../kernel/term.bend as T
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as L
import ../kernel/value.bend as V
import ../kernel/totality.bend as Totality

# Preserve the fixture groups and raw assertions of Veil test/main.ml at PIN.
type Mode is Data:
  Parse{}
  Checked{}
  Erased{}
  Negative{}
  ErasureNegative{}
  KernelNegative{}
  RecursiveValues{}
  Migrated{}

type Source is Data:
  Source{name: String, path: String}

def sources(+directory: String, names: List<&2, String>) -> List<&2, Source>:
  match names:
    case Nil{}: []
    case Con{+name, rest}: Source{name, directory ++ "/" ++ name ++ ".kan"} <> sources(directory, rest)

def kan_name(+name: String, rest: List<&2, String>, accepted: Bool) -> List<&2, String>:
  match accepted:
    case True{}: String.take(name, (String.length(name) - 4n : Nat)) <> rest
    case False{}: rest

@unsafe def kan_names(names: List<&2, String>) -> List<&2, String>:
  match names:
    case Nil{}: []
    case Con{+name, rest}: kan_name(name, kan_names(rest), String.ends_with(name, ".kan"))

def listed(root: String, path: String) -> R.Task(List<&2, String>):
  do R.Task<List<&2, String>>:
    names : List<&2, String> <- R.directory(root, path)
    return R.sorted(kan_names(names))

def optional_listed(present: Bool, root: String, path: String) -> R.Task(List<&2, String>):
  match present:
    case True{}: listed(root, path)
    case False{}: R.Task.pure(List<&2, String>, [])

def message_result(-A: Data, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, A>:
  match result:
    case Fail{error}: Fail{E.message(error)}
    case Done{value}: Done{value}

def round_trip(source: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +first : List<&2, Syntax.Decl> <- message_result(List<&2, Syntax.Decl>, Parser.program(source))
    second : List<&2, Syntax.Decl> <- message_result(List<&2, Syntax.Decl>, Parser.program(Syntax.print(first)))
    R.require(Same.declarations(first, second), "the printed text parses to a different tree")

def refusal(-A: Data, result: Result<&2, &2, E.T, A>, want: String, missing: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{missing}
    case Fail{error}:
      +got : String = E.message(error)
      R.require(String.eq(got, String.trim(want)), "the message is \"" ++ got ++ "\"")

def guarded_self() -> T.T:
  +arrow : Shape.T<T.T> = Shape.SPi{Q.QMany{}, "n", T.Global{"N"}}
  call : T.T = T.Out{arrow, T.APt{Q.QMany{}, T.Var{0n}}, T.Global{"self"}}
  branch : C.Pair<T.Addr<T.T>, T.Leg<T.T>> = C.Pair{T.ACtor{"succ"}, T.Leg{[C.Pair{Q.QMany{}, "m"}], call}}
  motive : T.Motive<T.T> = T.Motive{Some{"N"}, [], "x", T.Global{"N"}}
  inner : T.T = T.Elim{T.ElimData{Shape.SMu{"N", []}, T.Var{0n}, Q.QMany{}, Some{motive}, [branch]}}
  T.Sec{arrow, [T.Leg{[C.Pair{Q.QMany{}, "n"}], inner}]}

def guarded_position(position: Maybe<&2, Nat>) -> Result<&2, &2, String, Unit>:
  match position:
    case None{}: Fail{"the guarded self reference answers no position"}
    case Some{+index}: R.require(Nat.is_eq(index, 0n), "the guarded position is " ++ Nat.show(index))

def kernel_negative(name: String) -> Result<&2, &2, String, Unit>:
  match name:
    case "smu":
      refusal(V.T, Check.infer_term(G.initial(), B.unlimited(), T.Ran{Shape.SMu{"F", [T.Univ{L.zero()}]}, T.Sec{Shape.coll(T.T, 0n), []}}), "a right former at a mu shape arrives at M2", "the right former at a mu shape is admitted at M1")
    case "self":
      do Result<&2, &2, String, Unit>:
        ok : Unit <- refusal(Maybe<&2, Nat>, Totality.guard(B.unlimited(), G.initial(), "self", T.Global{"Nat"}, T.Global{"self"}), "recursive definition self failed the structural termination guard", "the unguarded self reference is admitted at M1")
        position : Maybe<&2, Nat> <- R.kernel(Maybe<&2, Nat>, Totality.guard(B.unlimited(), G.initial(), "self", T.Global{"Nat"}, guarded_self()))
        guarded_position(position)
    case other: Fail{"no kernel negative is named \"" ++ other ++ "\""}

def recursive_values() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Elab.Checked <- R.checked(G.initial(), Recursive.source())
    R.kernel(Unit, Recursive.validate(checked))

def run_case(mode: Mode, +root: String, +name: String, path: String, fixtures: List<&2, String>) -> R.Task(Unit):
  match mode:
    case Parse{}:
      do R.Task<Unit>:
        source : String <- R.read(root, path)
        R.lift(Unit, round_trip(source))
    case Checked{}:
      do R.Task<Unit>:
        source : String <- R.read(root, path)
        golden : String <- R.read(root, "golden/" ++ name ++ ".checked")
        rows : Elab.Rows <- R.lift(Elab.Rows, R.kernel(Elab.Rows, Elab.check_text(G.initial(), B.unlimited(), source)))
        R.assert(String.eq(Elab.checked_form(rows), golden), "the checked form is not the golden text")
    case Erased{}:
      do R.Task<Unit>:
        source : String <- R.read(root, path)
        golden : String <- R.read(root, "golden/" ++ name ++ ".erased")
        printed : String <- R.lift(String, R.kernel(String, Erasure.process(source)))
        R.assert(String.eq(printed, golden), "the erased form is not the golden text")
    case Negative{}:
      do R.Task<Unit>:
        source : String <- R.read(root, path)
        want : String <- R.read(root, "neg/" ++ name ++ ".err")
        R.lift(Unit, refusal(Elab.Rows, Elab.check_text(G.initial(), B.unlimited(), source), want, "the file checks and the negative expects it to fail"))
    case ErasureNegative{}:
      do R.Task<Unit>:
        source : String <- R.read(root, path)
        want : String <- R.read(root, "erase-neg/" ++ name ++ ".err")
        checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.initial(), source))
        R.lift(Unit, refusal(List<&2, C.Pair<String, Erase.Entry>>, Erasure.erased(checked), want, "the file erases and the negative expects the erasure to fail"))
    case KernelNegative{}: R.lift(Unit, kernel_negative(name))
    case RecursiveValues{}: R.lift(Unit, recursive_values())
    case Migrated{}: R.assert(R.contains(fixtures, name), "the migrated erasure fixture is missing")

def passed_case(quiet: Bool, kind: String, name: String) -> IO(Bool):
  match quiet:
    case True{}: IO.pure(Bool, True{})
    case False{}:
      do IO<Bool>:
        printed : Unit <- IO.print(kind ++ " " ++ name ++ " OK")
        return True{}

def report(quiet: Bool, kind: String, name: String, result: Result<&2, &2, String, Unit>) -> IO(Bool):
  match result:
    case Done{ok}: passed_case(quiet, kind, name)
    case Fail{message}:
      do IO<Bool>:
        printed : Unit <- IO.print(kind ++ " " ++ name ++ " FAIL: " ++ message)
        return False{}

@unsafe def group_loop(+mode: Mode, +quiet: Bool, +kind: String, +label: String, +root: String, cases: List<&2, Source>, +fixtures: List<&2, String>, +passed: Nat, +total: Nat) -> IO(C.Pair<Nat, Nat>):
  match cases:
    case Nil{}:
      do IO<C.Pair<Nat, Nat>>:
        printed : Unit <- IO.print(label ++ " " ++ Nat.show(passed) ++ "/" ++ Nat.show(total))
        return C.Pair{passed, total}
    case Con{Source{+name, path}, rest}:
      do IO<C.Pair<Nat, Nat>>:
        result : Result<&2, &2, String, Unit> <- run_case(mode, root, name, path, fixtures)
        ok : Bool <- report(quiet, kind, name, result)
        group_loop(mode, quiet, kind, label, root, rest, fixtures, Nat.add(passed, Bool.pick(Nat, ok, 1n, 0n)), total)

def group(mode: Mode, quiet: Bool, kind: String, label: String, root: String, +cases: List<&2, Source>, fixtures: List<&2, String>) -> IO(C.Pair<Nat, Nat>):
  group_loop(mode, quiet, kind, label, root, cases, fixtures, 0n, R.length(Source, cases))

def sound(count: C.Pair<Nat, Nat>) -> Bool:
  match count:
    case C.Pair{passed, total}: Nat.is_eq(passed, total)

def full(count: C.Pair<Nat, Nat>) -> Bool:
  match count:
    case C.Pair{passed, +total}: Nat.is_eq(passed, total) && Bool.not(Nat.is_eq(total, 0n))

def all_full(counts: List<&2, C.Pair<Nat, Nat>>) -> Bool:
  match counts:
    case Nil{}: True{}
    case Con{count, rest}: full(count) && all_full(rest)

def verdict(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("SUITE-KERNEL OK")
    case False{}:
      do IO<Unit>:
        printed : Unit <- IO.print("SUITE-KERNEL FAIL")
        Output.cli_exit(1)

def fail_out(message: String) -> IO(Unit):
  do IO<Unit>:
    printed : Unit <- IO.print("SUITE " ++ message)
    verdict(False{})

def run(+root: String, +fixtures: List<&2, String>, +negatives: List<&2, String>, +erase_negatives: List<&2, String>) -> IO(Unit):
  +positive : List<&2, Source> = sources("fixtures", fixtures)
  +negative : List<&2, Source> = sources("neg", negatives)
  +erase_negative : List<&2, Source> = sources("erase-neg", erase_negatives)
  parse_cases : List<&2, Source> = List.append(&2, Source, positive, List.append(&2, Source, negative, erase_negative))
  do IO<Unit>:
    parsed : C.Pair<Nat, Nat> <- group(Parse{}, True{}, "PARSE", "PARSE-OK", root, parse_cases, [])
    checked : C.Pair<Nat, Nat> <- group(Checked{}, False{}, "CHECK", "CHECK-OK", root, positive, [])
    erased : C.Pair<Nat, Nat> <- group(Erased{}, False{}, "ERASE", "ERASE-OK", root, positive, [])
    refused : C.Pair<Nat, Nat> <- group(Negative{}, False{}, "NEG", "NEG-OK", root, negative, [])
    unerased : C.Pair<Nat, Nat> <- group(ErasureNegative{}, False{}, "ERASE-NEG", "ERASE-NEG-OK", root, erase_negative, [])
    closed : C.Pair<Nat, Nat> <- group(KernelNegative{}, False{}, "KNEG", "KNEG-OK", root, sources("", ["smu", "self"]), [])
    recursive : C.Pair<Nat, Nat> <- group(RecursiveValues{}, False{}, "REC", "REC-OK", root, sources("", ["values"]), [])
    migration : C.Pair<Nat, Nat> <- group(Migrated{}, False{}, "MIGRATED", "MIGRATED-OK", root, sources("", ["mu-erase", "mu-rec-direct", "mu-rec-indexed", "mu-rec-mutual"]), fixtures)
    verdict(all_full([parsed, checked, erased, refused, closed, recursive, migration]) && sound(unerased))

def list_optional(+root: String) -> R.Task(List<&2, String>):
  do IO<Result<&2, &2, String, List<&2, String>>>:
    present : Bool <- FileIO.exists(root ++ "/erase-neg")
    optional_listed(present, root, "erase-neg")

def list_inputs(+root: String) -> R.Task(C.Pair<List<&2, String>, C.Pair<List<&2, String>, List<&2, String>>>):
  do R.Task<C.Pair<List<&2, String>, C.Pair<List<&2, String>, List<&2, String>>>>:
    fixtures : List<&2, String> <- listed(root, "fixtures")
    negatives : List<&2, String> <- listed(root, "neg")
    erase_negatives : List<&2, String> <- list_optional(root)
    return C.Pair{fixtures, C.Pair{negatives, erase_negatives}}

def listed_run(root: String, result: Result<&2, &2, String, C.Pair<List<&2, String>, C.Pair<List<&2, String>, List<&2, String>>>>) -> IO(Unit):
  match result:
    case Fail{message}: fail_out(message)
    case Done{C.Pair{fixtures, C.Pair{negatives, erase_negatives}}}: run(root, fixtures, negatives, erase_negatives)

def root_argument(args: List<String>) -> String:
  match args:
    case Nil{}: "test/veil/test"
    case Con{root, rest}: root

def arguments(args: List<String>) -> IO(Unit):
  +root : String = root_argument(args)
  do IO<Unit>:
    result : Result<&2, &2, String, C.Pair<List<&2, String>, C.Pair<List<&2, String>, List<&2, String>>>> <- list_inputs(root)
    listed_run(root, result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
