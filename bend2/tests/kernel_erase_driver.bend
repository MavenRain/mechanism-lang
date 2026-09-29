import Base
import ../surface/elab.bend as Elab
import ../kernel/erase.bend as Erase
import ../kernel/common.bend as P
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/error.bend as Error

def erased(checked: Elab.Checked) -> Result<&2,&2,Error.T,List<&2,P.Pair<String,Erase.Entry>>>:
  match checked:
    case P.Pair{globals,rows}: Erase.program(globals,rows)

def process(source: String) -> Result<&2,&2,Error.T,String>:
  do Result<&2,&2,Error.T,String>:
    checked : Elab.Checked <- Elab.check_in(G.initial(),B.unlimited(),source)
    rows : List<&2,P.Pair<String,Erase.Entry>> <- erased(checked)
    return Erase.print(rows)

def respond(result: Result<&2,&2,Error.T,String>) -> IO(Unit):
  match result:
    case Done{text}: IO.print(text)
    case Fail{error}: IO.die(Unit,1,Error.to_string(error))

def run(args: List<String>) -> IO(Unit):
  match args:
    case [source]: respond(process(source))
    case _: IO.die(Unit,2,"expected SOURCE")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    run(args)
