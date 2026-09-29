import Base
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../kernel/error.bend as Error

def respond(result: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> IO(Unit):
  match result:
    case Done{decls}:
      IO.print("OK")
    case Fail{error}:
      IO.die(Unit, 1, Error.to_string(error))

def run(args: List<String>) -> IO(Unit):
  match args:
    case Con{source, Nil{}}:
      respond(Parser.program(source))
    case other:
      IO.die(Unit, 2, "expected exactly one program source argument")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    run(args)
