import Base
import ../rust/rir.bend as R
import ../rust/emit.bend as Em

# Direct RIR coverage, independent of source family classification.
def con(name: String) -> R.Ty:
  R.TCon{name, []}

def param(name: String, t: R.Ty) -> R.Param:
  R.Param{name, t}

def value(name: String) -> R.Ex:
  R.XVar{name}

def named() -> R.Item:
  R.IStruct{"Named", "named", True{},
    [param("enabled", R.TBool{}), param("payload", con("String")), param("spare", R.TBool{})],
    [param("enabled", R.TBool{}), param("payload", R.TRef{con("String")})]}

def tuple() -> R.Item:
  R.IStruct{"Tuple", "tuple", False{},
    [param("first", R.TBool{}), param("second", con("String")), param("third", R.TBool{})], []}

def read_named(+name: String, +borrowed: Bool, +renamed: Bool) -> R.Item:
  R.IFn{name, [], [param("record", Bool.pick(R.Ty, borrowed, R.TRef{con("Named")}, con("Named")))], con("String"),
    R.XMatch{value("record"), [R.Arm{
      R.PStruct{"Named", True{}, ["enabled", "payload", "spare"],
        [R.BSkip{}, R.BVar{Bool.pick(String, renamed, "text", "payload")}, R.BSkip{}]},
      Bool.pick(R.Ex, borrowed,
        R.XClone{value(Bool.pick(String, renamed, "text", "payload"))},
        value(Bool.pick(String, renamed, "text", "payload")))
    }]}}

def read_tuple(+name: String, +borrowed: Bool) -> R.Item:
  R.IFn{name, [], [param("record", Bool.pick(R.Ty, borrowed, R.TRef{con("Tuple")}, con("Tuple")))], con("String"),
    R.XMatch{value("record"), [R.Arm{
      R.PStruct{"Tuple", False{}, [], [R.BSkip{}, R.BVar{"text"}, R.BSkip{}]},
      Bool.pick(R.Ex, borrowed, R.XClone{value("text")}, value("text"))
    }]}}

def empty_match(+name: String, +family: String, named: Bool) -> R.Item:
  R.IFn{name, [], [param("record", con(family))], R.TBool{},
    R.XMatch{value("record"), [R.Arm{R.PStruct{family, named, [], []}, R.XBool{True{}}}]}}

def sample() -> R.Module:
  R.Module{"struct-regression.mech", [
    named(), tuple(),
    read_named("named_renamed", True{}, True{}),
    read_named("named_shorthand", True{}, False{}),
    read_named("named_owned", False{}, True{}),
    read_tuple("tuple_borrowed", True{}),
    read_tuple("tuple_owned", False{}),
    R.IStruct{"EmptyNamed", "empty_named", True{}, [], []},
    R.IStruct{"EmptyTuple", "empty_tuple", False{}, [], []},
    empty_match("empty_named_match", "EmptyNamed", True{}),
    empty_match("empty_tuple_match", "EmptyTuple", False{})]}

def main() -> IO(Unit):
  IO.write(Em.text(sample()))
