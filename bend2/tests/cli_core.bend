import Base
import ../cli/kanon.bend as K

def start(args:List<String>) -> IO(Unit):
  match args:
    case Nil{}: K.usage()
    case Con{cmd,rest}: K.dispatch(cmd,rest)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    start(args)
