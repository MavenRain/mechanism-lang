import Base
import ./runtime_slice.bend as Slice
import ./runtime_reachable.bend as Reach
import ../io.bend as Fs
import ../surface/elab.bend as Elab
import ../kernel/common.bend as P
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Bignum
import ../kernel/eval.bend as Eval
import ../kernel/erase.bend as Erase
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../wasm/emit.bend as Emit

type Prepared is Data:
  Prepared{globals: Global.T, rows: Slice.Rows}

def kernel(-A: Data, result: Result<&2, &2, Error.T, A>) -> Result<&2, &2, String, A>:
  match result:
    case Done{value}: Done{value}
    case Fail{error}: Fail{Error.to_string(error)}

def no_axioms(names: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{name, rest}: Fail{"runtime fixture declares axioms"}

def final_component(reversed: String) -> String:
  match reversed:
    case SNil{}: ""
    case SCon{Chr{47}, rest}: ""
    case SCon{char, rest}: SCon{char, final_component(rest)}

def basename(normalized: String) -> String:
  match normalized:
    case "/": "/"
    case other: String.reverse(final_component(String.reverse(other)))

def valid_exports(names: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{+name, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- Slice.require(String.eq(basename(Fs.normalize_target(name)), name), "invalid export path")
        valid_exports(rest)

def prepare_mode(reachable: Bool, +globals: Global.T, rows: Reach.SourceRows, names: List<&2, String>) -> Result<&2, &2, String, Prepared>:
  match reachable:
    case True{}:
      do Result<&2, &2, String, Prepared>:
        erased : Slice.Rows <- Reach.select_many(globals, rows, names)
        return Prepared{globals, erased}
    case False{}:
      do Result<&2, &2, String, Prepared>:
        erased : Slice.Rows <- kernel(Slice.Rows, Erase.program(globals, rows))
        return Prepared{globals, erased}

def prepare_rows(checked: Elab.Checked, reachable: Bool, +names: List<&2, String>) -> Result<&2, &2, String, Prepared>:
  match checked:
    case P.Pair{+globals, +rows}:
      do Result<&2, &2, String, Prepared>:
        checked : Unit <- no_axioms(Elab.axiom_names(rows))
        checked : Unit <- valid_exports(names)
        prepare_mode(reachable, globals, rows, names)

def prepare(text: String, reachable: Bool, names: List<&2, String>) -> Result<&2, &2, String, Prepared>:
  do Result<&2, &2, String, Prepared>:
    checked : Elab.Checked <- kernel(Elab.Checked, Elab.check_in(Global.initial(), Budget.unlimited(), text))
    prepare_rows(checked, reachable, names)

def natural(value: Value.T, +export: String) -> Result<&2, &2, String, String>:
  match value:
    case Value.VLit{Literal.LInt{number}}: Done{Bignum.to_string(number)}
    case Value.VLit{Literal.LString{text}}: Fail{"not a natural number: " ++ export}
    case other: Fail{"not a literal: " ++ export}

def selected(reachable: Bool, rows: Slice.Rows, export: String) -> Result<&2, &2, String, Slice.Rows>:
  match reachable:
    case False{}: Done{rows}
    case True{}: Slice.select(rows, export)

def prepare_export(+globals: Global.T, rows: Slice.Rows, +export: String, reachable: Bool) -> Result<&2, &2, String, P.Pair<String, List<&2, U32>>>:
  do Result<&2, &2, String, P.Pair<String, List<&2, U32>>>:
    checked : Unit <- Slice.require(String.eq(basename(Fs.normalize_target(export)), export), "invalid export path")
    value : Value.T <- kernel(Value.T, Eval.eval(globals, [], Term.Global{export}))
    value : Value.T <- kernel(Value.T, Eval.whnf(globals, value))
    answer : String <- natural(value, export)
    rows : Slice.Rows <- selected(reachable, rows, export)
    bytes : List<&2, U32> <- kernel(List<&2, U32>, Emit.program(rows, export))
    return P.Pair{answer, bytes}

law exports:
  for +globals: Global.T
  for +rows: Slice.Rows
  for +directory: String
  for +reachable: Bool
  for names: List<&2, String>
  IO(Unit)

@unsafe def written(result: Result<&2, &2, String, Unit>, answer: String, export: String, globals: Global.T, rows: Slice.Rows, directory: String, reachable: Bool, rest: List<&2, String>) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit, 1, message)
    case Done{unit}:
      do IO<Unit>:
        unit : Unit <- IO.print(export ++ "\t" ++ answer)
        exports(globals, rows, directory, reachable, rest)

@unsafe def write_export(result: Result<&2, &2, String, P.Pair<String, List<&2, U32>>>, +export: String, globals: Global.T, rows: Slice.Rows, +directory: String, reachable: Bool, rest: List<&2, String>) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit, 1, message)
    case Done{P.Pair{answer, bytes}}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Fs.write_bytes(directory ++ "/" ++ export ++ ".wasm", bytes)
        written(result, answer, export, globals, rows, directory, reachable, rest)

@unsafe def exports(globals, rows, directory, reachable, names):
  match names:
    case Nil{}: IO.pure(Unit, Unit{})
    case Con{+name, rest}: write_export(prepare_export(globals, rows, name, reachable), name, globals, rows, directory, reachable, rest)

def run_prepared(result: Result<&2, &2, String, Prepared>, directory: String, reachable: Bool, names: List<&2, String>) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit, 1, message)
    case Done{Prepared{globals, rows}}: exports(globals, rows, directory, reachable, names)

def run_read(result: Result<&2, &2, String, String>, directory: String, +reachable: Bool, +names: List<&2, String>) -> IO(Unit):
  match result:
    case Fail{message}: IO.die(Unit, 1, message)
    case Done{text}: run_prepared(prepare(text, reachable, names), directory, reachable, names)

def run(source: String, directory: String, reachable: Bool, names: List<&2, String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- Fs.read_file(source)
    run_read(result, directory, reachable, names)

def self_test(result: Result<&2, &2, String, Unit>) -> IO(Unit):
  match result:
    case Done{unit}: IO.print("RUNTIME-SLICE-OK")
    case Fail{message}: IO.die(Unit, 1, message)

def refused(result: Result<&2, &2, String, Prepared>, message: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Fail{error}: Done{Unit{}}
    case Done{prepared}: Fail{message}

def axiom_refused(result: Result<&2, &2, String, Prepared>) -> Result<&2, &2, String, Unit>:
  match result:
    case Fail{message}: Slice.require(String.eq(message, "runtime fixture declares axioms"), "unreachable axiom diagnostic")
    case Done{prepared}: Fail{"unreachable axiom accepted"}

def reachable_test(checked: Elab.Checked) -> Result<&2, &2, String, Unit>:
  match checked:
    case P.Pair{globals, rows}: Reach.self_test(globals, rows)

def runtime_self_test() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Slice.self_test()
    source : Elab.Checked <- kernel(Elab.Checked, Elab.check_in(Global.initial(), Budget.unlimited(), "def hidden : Type := Nat\ndef helper : Nat -> Nat := fun (n : Nat) => n\ndef unused : Nat := 99\ndef alias : Nat -> Nat := helper\ndef root : Nat := alias 7\n"))
    checked : Unit <- reachable_test(source)
    checked : Unit <- axiom_refused(prepare("axiom unused : Nat\ndef root : Nat := 7\n", True{}, ["root"]))
    checked : Unit <- refused(prepare("def unused : Nat := Type\ndef root : Nat := 7\n", True{}, ["root"]), "unchecked unreachable declaration")
    return Unit{}

def usage() -> IO(Unit):
  IO.die(Unit, 1, "usage: prelude_runtime [--reachable] SOURCE DIRECTORY EXPORT...")

def export_names(names: List<String>) -> List<&2, String>:
  match names:
    case Nil{}: []
    case Con{name, rest}: Con{name, export_names(rest)}

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case ["--slice-self-test"]: self_test(runtime_self_test())
    case Con{"--reachable", Con{source, Con{directory, Con{first, rest}}}}: run(source, directory, True{}, export_names(Con{first, rest}))
    case Con{"--reachable", rest}: usage()
    case Con{source, Con{directory, Con{first, rest}}}: run(source, directory, False{}, export_names(Con{first, rest}))
    case other: usage()

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
