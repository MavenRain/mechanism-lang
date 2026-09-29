import Base
import ../io.bend as Files
import ../kernel/bignum.bend as Bignum
import ../kernel/budget.bend as Budget
import ../kernel/common.bend as Common
import ../kernel/error.bend as Error
import ../kernel/eval.bend as Eval
import ../kernel/global.bend as Global
import ../kernel/literal.bend as Literal
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../surface/elab.bend as Elab

type Binding is Data:
  Binding{name: String, value: String}

def natural(value: Value.T, +name: String) -> Result<&2, &2, String, Binding>:
  match value:
    case Value.VLit{Literal.LInt{number}}:
      Done{Binding{name, Bignum.to_string(number)}}
    case Value.VLit{Literal.LString{text}}:
      Fail{"not a natural number: " ++ name}
    case other:
      Fail{"not a literal: " ++ name}

def normal(result: Result<&2, &2, Error.T, Value.T>, name: String) -> Result<&2, &2, String, Binding>:
  match result:
    case Done{value}:
      natural(value, name)
    case Fail{error}:
      Fail{Error.to_string(error)}

def evaluated(result: Result<&2, &2, Error.T, Value.T>, globals: Global.T, name: String) -> Result<&2, &2, String, Binding>:
  match result:
    case Done{value}:
      normal(Eval.whnf(globals, value), name)
    case Fail{error}:
      Fail{Error.to_string(error)}

def prepend(result: Result<&2, &2, String, List<&2, Binding>>, binding: Binding) -> Result<&2, &2, String, List<&2, Binding>>:
  match result:
    case Done{rest}:
      Done{Con{binding, rest}}
    case Fail{message}:
      Fail{message}

law values:
  for +globals: Global.T
  for exports: List<String>
  Result<&2, &2, String, List<&2, Binding>>

@unsafe def next(result: Result<&2, &2, String, Binding>, globals: Global.T, rest: List<String>) -> Result<&2, &2, String, List<&2, Binding>>:
  match result:
    case Fail{message}:
      Fail{message}
    case Done{binding}:
      prepend(values(globals, rest), binding)

@unsafe def values(globals, exports):
  match exports:
    case Nil{}:
      Done{[]}
    case Con{+name, rest}:
      next(evaluated(Eval.eval(globals, [], Term.Global{name}), globals, name), globals, rest)

def allowed(axioms: List<&2, String>, globals: Global.T, exports: List<String>) -> Result<&2, &2, String, List<&2, Binding>>:
  match axioms:
    case Nil{}:
      values(globals, exports)
    case Con{first, rest}:
      Fail{"certificate declares axioms: " ++ String.join(Con{first, rest}, ", ")}

def checked(result: Result<&2, &2, Error.T, Common.Pair<Global.T, Elab.Rows>>, exports: List<String>) -> Result<&2, &2, String, List<&2, Binding>>:
  match result:
    case Fail{error}:
      Fail{Error.to_string(error)}
    case Done{Common.Pair{globals, rows}}:
      allowed(Elab.axiom_names(rows), globals, exports)

def show(bindings: List<&2, Binding>) -> IO(Unit):
  match bindings:
    case Nil{}:
      IO.pure(Unit, Unit{})
    case Con{Binding{name, value}, rest}:
      do IO<Unit>:
        unit : Unit <- IO.print(name ++ "\t" ++ value)
        show(rest)

def report(result: Result<&2, &2, String, List<&2, Binding>>) -> IO(Unit):
  match result:
    case Fail{message}:
      IO.die(Unit, 1, message)
    case Done{bindings}:
      do IO<Unit>:
        unit : Unit <- IO.print("CHECKED axioms=0")
        show(bindings)

def read_result(result: Result<&2, &2, String, String>, exports: List<String>) -> IO(Unit):
  match result:
    case Fail{message}:
      IO.die(Unit, 1, message)
    case Done{source}:
      report(checked(Elab.check_in(Global.initial(), Budget.unlimited(), source), exports))

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}:
      IO.die(Unit, 1, "usage: mech_cert SOURCE [NAT_EXPORT ...]")
    case Con{source, exports}:
      do IO<Unit>:
        result : Result<&2, &2, String, String> <- Files.read_file(source)
        read_result(result, exports)
