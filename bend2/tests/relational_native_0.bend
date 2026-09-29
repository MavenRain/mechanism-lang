import Base
import ./prelude_category.bend as P0
import ./prelude_functor.bend as P1
import ./prelude_nattrans.bend as P2
import ./prelude_left_kan.bend as P3

def select_3(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P3.arguments(args)
    case False{}: IO.die(Unit,2,"unknown relational shard mode")

def select_2(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P2.arguments(args)
    case False{}: select_3(String.eq(mode,"left-kan"),mode,args)

def select_1(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P1.arguments(args)
    case False{}: select_2(String.eq(mode,"nattrans"),mode,args)

def select_0(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P0.arguments(args)
    case False{}: select_1(String.eq(mode,"functor"),mode,args)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{+mode,rest}: select_0(String.eq(mode,"category"),mode,rest)
    case Nil{}: IO.die(Unit,2,"expected relational shard mode")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
