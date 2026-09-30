import Base
import ./prelude_horizontal_associativity.bend as P0

def select_0(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P0.arguments(args)
    case False{}: IO.die(Unit,2,"unknown relational shard mode")

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{+mode,rest}: select_0(String.eq(mode,"horizontal-associativity"),mode,rest)
    case Nil{}: IO.die(Unit,2,"expected relational shard mode")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
