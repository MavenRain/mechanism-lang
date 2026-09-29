import Base
import ./prelude_category.bend as Category
import ./prelude_heterogeneous_functor.bend as Heterogeneous
import ./surface_check_driver.bend as Surface
import ./template_cost.bend as Template

def surface(yes: Bool, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: Surface.run(args)
    case False{}: IO.die(Unit,2,"expected category, heterogeneous-functor, template-cost or surface-check mode")

def template(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: Template.arguments(args)
    case False{}: surface(String.eq(mode,"surface-check"),args)

def heterogeneous(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: Heterogeneous.arguments(args)
    case False{}: template(String.eq(mode,"template-cost"),mode,args)

def category(yes: Bool, +mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: Category.arguments(args)
    case False{}: heterogeneous(String.eq(mode,"heterogeneous-functor"),mode,args)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{+mode,rest}: category(String.eq(mode,"category"),mode,rest)
    case Nil{}: IO.die(Unit,2,"expected protocol mode and arguments")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
