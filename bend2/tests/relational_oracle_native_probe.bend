import Base
import ./relational_negatives.bend as Neg

def show(label: String, value: Bool) -> IO(Unit):
  IO.print(label ++ ": " ++ Bool.pick(String, value, "true", "false"))

def probe(+prefix: String) -> IO(Unit):
  +pin : Neg.Pin = Neg.Pin{"erased-object", "", prefix, ""}
  +all : List<&2, Neg.Pin> = [pin, Neg.Pin{"other", "", "mismatch: other", ""}]
  do IO<Unit>:
    ok : Unit <- show("length", Nat.is_gt(String.length(prefix), 9n))
    ok : Unit <- show("distinct", Neg.distinct_from(pin, all, False{}))
    ok : Unit <- show("not common", Bool.not(Neg.prefix_of_all(prefix, all)))
    ok : Unit <- show("oracle", Neg.oracle_valid(pin, all, Neg.Prefix{9n, True{}, False{}}))
    return Unit{}

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{prefix, Nil{}}: probe(prefix)
    case other: IO.die(Unit, 2, "expected prefix")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
