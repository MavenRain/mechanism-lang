import Base
import ../surface/elab.bend as Elab
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def respond(result: Result<&2, &2, Error.T, Elab.Rows>) -> IO(Unit):
  match result:
    case Done{rows}:
      IO.print(Elab.checked_form(rows))
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def run_empty(valid: Bool, source: String) -> IO(Unit):
  match valid:
    case True{}:
      respond(Elab.check_text(Global.empty(), Budget.unlimited(), source))
    case False{}:
      IO.die(Unit, 2, "expected SOURCE or --empty SOURCE")

def run(args: List<String>) -> IO(Unit):
  match args:
    case Con{source, Nil{}}:
      respond(Elab.check_text(Global.initial(), Budget.unlimited(), source))
    case Con{flag, Con{source, Nil{}}}:
      run_empty(String.eq(flag, "--empty"), source)
    case other:
      IO.die(Unit, 2, "expected SOURCE or --empty SOURCE")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    run(args)
