import Base
import ../rust/rir.bend as R
import ../rust/emit.bend as Em
import ../rust/erase_typed.bend as Et
import ../io.bend as F
import ../cli/rust_out.bend as Ro
import ./rust_emit_oracle.bend as Or
import ./rust_struct_emit.bend as St

# Driver: `bend bend2/tests/rust_emit.bend sample <init|second>` or
# `classify <file.mech> [<file.mech>]`.
# `sample` prints a module from a hand-built IR. Each item of the text must
# be equal to the same item of the hand-written target crate.
# `classify` checks the files in order and prints one line for each
# declaration: its class and, for a runtime declaration, its Rust form.

def con(name: String) -> R.Ty:
  R.TCon{name, []}

def nat() -> R.Ty:
  con("MechNat")

def rnat() -> R.Ty:
  R.TRef{nat()}

def v(name: String) -> R.Ex:
  R.XVar{name}

def p(name: String, t: R.Ty) -> R.Param:
  R.Param{name, t}

def unit(family: String, ctor: String) -> R.Pat:
  R.PCtor{family, ctor, []}

def small(n: String) -> R.Ex:
  R.XCall{"nat_small", [], [R.XInt{n}]}

# `&impl Fn(&A) -> C`
def branch(a: String) -> R.Ty:
  R.TRef{R.TFn{[R.TRef{con(a)}], con("C")}}

def mech_nat() -> R.Item:
  R.IEnum{"MechNat", [], False{}, [R.Variant{"MechZero", []}, R.Variant{"MechSucc", [R.TBox{nat()}]}]}

def mech_sum() -> R.Item:
  R.IEnum{"MechSum", ["A", "B"], False{}, [R.Variant{"MechInl", [con("A")]}, R.Variant{"MechInr", [con("B")]}]}

def mech_empty_elim() -> R.Item:
  R.IFn{"mech_empty_elim", ["A"], [p("e", con("MechEmpty"))], con("A"), R.XMatch{v("e"), []}}

def sum_arm(ctor: String, +x: String, f: String) -> R.Arm<R.Ex>:
  R.Arm{R.PCtor{"MechSum", ctor, [R.BVar{x}]}, R.XCall{f, [], [v(x)]}}

def mech_sum_rec() -> R.Item:
  R.IFn{"mech_sum_rec", ["A", "B", "C"],
    [p("left", branch("A")), p("right", branch("B")), p("s", R.TRef{R.TCon{"MechSum", [con("A"), con("B")]}})],
    con("C"), R.XMatch{v("s"), [sum_arm("MechInl", "a", "left"), sum_arm("MechInr", "b", "right")]}}

def sample_init() -> R.Module:
  R.Module{"init.mech",
    [mech_nat(), R.IUnit{"MechUnit", "mech_unit"}, R.IEnum{"MechEmpty", [], True{}, []}, mech_sum(), mech_empty_elim(),
      mech_sum_rec()]}

def auction_choice() -> R.Item:
  R.IEnum{"AuctionChoice", [], True{}, [R.Variant{"AuctionLose", []}, R.Variant{"AuctionWin", []}]}

def auction_payment() -> R.Item:
  R.IFn{"auction_payment", [], [p("price", rnat()), p("choice", con("AuctionChoice"))], nat(),
    R.XMatch{v("choice"),
      [R.Arm{unit("AuctionChoice", "AuctionLose"), R.XCtor{"MechNat", "MechZero", []}},
        R.Arm{unit("AuctionChoice", "AuctionWin"), R.XClone{v("price")}}]}}

def add_step() -> R.Ex:
  R.XCtor{"MechNat", "MechSucc", [R.XBox{R.XCall{"auction_add", [], [v("previous"), v("right")]}}]}

def auction_add() -> R.Item:
  R.IFn{"auction_add", [], [p("left", rnat()), p("right", rnat())], nat(),
    R.XMatch{v("left"),
      [R.Arm{unit("MechNat", "MechZero"), R.XClone{v("right")}},
        R.Arm{R.PCtor{"MechNat", "MechSucc", [R.BVar{"previous"}]}, add_step()}]}}

def native_step() -> R.Ex:
  R.XClosure{[p("_value", rnat()), p("previous", R.TRef{con("Nat")})],
    R.XCall{"nat_add", [], [R.XBorrow{small("1")}, v("previous")]}}

def auction_to_native() -> R.Item:
  R.IFn{"auction_to_native", [], [p("n", rnat())], con("Nat"),
    R.XCall{"mech_nat_rec", [con("Nat")], [R.XBorrow{small("0")}, R.XBorrow{native_step()}, v("n")]}}

def sample_second() -> R.Module:
  R.Module{"second-price.mech",
    [R.IUse{"init"}, R.IUse{"nat"}, auction_choice(), auction_payment(), auction_add(), auction_to_native()]}

# Bool.pick is eager, so it picks a mode tag and `run_args` matches on the
# tag: one run prints only the chosen module.
type Md is Data:
  MInit{}
  MSecond{}
  MStruct{}
  MClassify{}
  MEmit{}
  MCrate{}
  MValues{}
  MProbes{}
  MBin{}
  MNone{}

def usage() -> String:
  "usage: rust_emit.bend sample <init|second|struct> | classify <file.mech> [<file.mech>] | emit <file.mech> [<file.mech>] | crate <file.mech>... <out dir> | values <file.mech>... | probes <file.mech>... | bin <file.mech>...\n"

def oracle_of(+mode: String) -> Md:
  Bool.pick(Md, String.eq(mode, "values"), MValues{}, Bool.pick(Md, String.eq(mode, "probes"), MProbes{}, Bool.pick(Md, String.eq(mode, "bin"), MBin{}, MNone{})))

def which_of(+which: String) -> Md:
  Bool.pick(Md, String.eq(which, "init"), MInit{}, Bool.pick(Md, String.eq(which, "second"), MSecond{}, Bool.pick(Md, String.eq(which, "struct"), MStruct{}, MNone{})))

def mode_of(+mode: String, which: String) -> Md:
  Bool.pick(Md, String.eq(mode, "sample"), which_of(which), Bool.pick(Md, String.eq(mode, "classify"), MClassify{}, Bool.pick(Md, String.eq(mode, "emit"), MEmit{}, Bool.pick(Md, String.eq(mode, "crate"), MCrate{}, oracle_of(mode)))))

def read_result(result: Result<&2, &2, String, String>, path: String) -> IO(String):
  match result:
    case Done{text}:
      IO.pure(String, text)
    case Fail{e}:
      IO.die(String, 64, "cannot read " ++ path)

def read_file(+path: String) -> IO(String):
  do IO<String>:
    result : Result<&2, &2, String, String> <- F.read_file(path)
    read_result(result, path)

def classify1_io(path: String) -> IO(Unit):
  do IO<Unit>:
    source : String <- read_file(path)
    IO.write(Et.classify1(source))

def classify2_io(first: String, second: String) -> IO(Unit):
  do IO<Unit>:
    a : String <- read_file(first)
    b : String <- read_file(second)
    IO.write(Et.classify2(a, b))

def classify_io(first: String, rest: List<String>) -> IO(Unit):
  match rest:
    case Nil{}:
      classify1_io(first)
    case Con{second, more}:
      classify2_io(first, second)

def emit_out(r: Result<&2, &2, String, R.Module>) -> IO(Unit):
  match r:
    case Done{m}:
      IO.write(Em.text(m))
    case Fail{why}:
      IO.die(Unit, 65, why)

def emit1_io(+path: String) -> IO(Unit):
  do IO<Unit>:
    source : String <- read_file(path)
    emit_out(Et.emit1(path, source))

def emit2_io(+first: String, +second: String) -> IO(Unit):
  do IO<Unit>:
    a : String <- read_file(first)
    b : String <- read_file(second)
    emit_out(Et.emit2(first, a, second, b))

def emit_io(first: String, rest: List<String>) -> IO(Unit):
  match rest:
    case Nil{}:
      emit1_io(first)
    case Con{second, more}:
      emit2_io(first, second)

def run_md(m: Md, which: String, rest: List<String>) -> IO(Unit):
  match m:
    case MInit{}:
      IO.write(Em.text(sample_init()))
    case MSecond{}:
      IO.write(Em.text(sample_second()))
    case MStruct{}:
      IO.write(Em.text(St.sample()))
    case MClassify{}:
      classify_io(which, rest)
    case MEmit{}:
      emit_io(which, rest)
    case MCrate{}:
      Ro.run(which <> rest)
    case MValues{}:
      Or.values(which <> rest)
    case MProbes{}:
      Or.probes(which <> rest)
    case MBin{}:
      Or.bin(which <> rest)
    case MNone{}:
      IO.write(usage())

def run_args(args: List<String>) -> IO(Unit):
  match args:
    case Con{mode, Con{+which, rest}}:
      run_md(mode_of(mode, which), which, rest)
    case other:
      IO.write(usage())

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    run_args(args)
