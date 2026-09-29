import Base
import ./prelude_category.bend as P0
import ./prelude_functor.bend as P1
import ./prelude_nattrans.bend as P2
import ./prelude_left_kan.bend as P3
import ./prelude_heterogeneous_functor.bend as P4
import ./prelude_composable_functors.bend as P5
import ./prelude_heterogeneous_nattrans.bend as P6
import ./prelude_heterogeneous_left_kan.bend as P7
import ./prelude_heterogeneous_whiskering.bend as P8
import ./prelude_left_kan_laws.bend as P9
import ./prelude_shared_left_kan.bend as P10
import ./prelude_shared_nattrans.bend as P11
import ./prelude_nattrans_laws.bend as P12
import ./prelude_whiskering_laws.bend as P13
import ./prelude_horizontal_laws.bend as P14
import ./prelude_iterated_whiskering.bend as P15
import ./prelude_horizontal_associativity.bend as P16
import ./prelude_nattrans_units.bend as P17
import ./surface_check_driver.bend as Surface

def surface_mode(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: Surface.run(args)
    case False{}: IO.die(Unit, 1, "unknown relational protocol: " ++ mode)

def choose_18(+mode: String, args: List<String>) -> IO(Unit):
  surface_mode(String.eq(mode,"surface-check"),mode,args)

def selected_17(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P17.arguments(args)
    case False{}: choose_18(mode, args)

def choose_17(+mode: String, args: List<String>) -> IO(Unit):
  selected_17(String.eq(mode, "nattrans-units"), mode, args)

def selected_16(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P16.arguments(args)
    case False{}: choose_17(mode, args)

def choose_16(+mode: String, args: List<String>) -> IO(Unit):
  selected_16(String.eq(mode, "horizontal-associativity"), mode, args)

def selected_15(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P15.arguments(args)
    case False{}: choose_16(mode, args)

def choose_15(+mode: String, args: List<String>) -> IO(Unit):
  selected_15(String.eq(mode, "iterated-whiskering"), mode, args)

def selected_14(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P14.arguments(args)
    case False{}: choose_15(mode, args)

def choose_14(+mode: String, args: List<String>) -> IO(Unit):
  selected_14(String.eq(mode, "horizontal-laws"), mode, args)

def selected_13(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P13.arguments(args)
    case False{}: choose_14(mode, args)

def choose_13(+mode: String, args: List<String>) -> IO(Unit):
  selected_13(String.eq(mode, "whiskering-laws"), mode, args)

def selected_12(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P12.arguments(args)
    case False{}: choose_13(mode, args)

def choose_12(+mode: String, args: List<String>) -> IO(Unit):
  selected_12(String.eq(mode, "nattrans-laws"), mode, args)

def selected_11(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P11.arguments(args)
    case False{}: choose_12(mode, args)

def choose_11(+mode: String, args: List<String>) -> IO(Unit):
  selected_11(String.eq(mode, "shared-nattrans"), mode, args)

def selected_10(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P10.arguments(args)
    case False{}: choose_11(mode, args)

def choose_10(+mode: String, args: List<String>) -> IO(Unit):
  selected_10(String.eq(mode, "shared-left-kan"), mode, args)

def selected_9(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P9.arguments(args)
    case False{}: choose_10(mode, args)

def choose_9(+mode: String, args: List<String>) -> IO(Unit):
  selected_9(String.eq(mode, "left-kan-laws"), mode, args)

def selected_8(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P8.arguments(args)
    case False{}: choose_9(mode, args)

def choose_8(+mode: String, args: List<String>) -> IO(Unit):
  selected_8(String.eq(mode, "heterogeneous-whiskering"), mode, args)

def selected_7(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P7.arguments(args)
    case False{}: choose_8(mode, args)

def choose_7(+mode: String, args: List<String>) -> IO(Unit):
  selected_7(String.eq(mode, "heterogeneous-left-kan"), mode, args)

def selected_6(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P6.arguments(args)
    case False{}: choose_7(mode, args)

def choose_6(+mode: String, args: List<String>) -> IO(Unit):
  selected_6(String.eq(mode, "heterogeneous-nattrans"), mode, args)

def selected_5(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P5.arguments(args)
    case False{}: choose_6(mode, args)

def choose_5(+mode: String, args: List<String>) -> IO(Unit):
  selected_5(String.eq(mode, "composable-functors"), mode, args)

def selected_4(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P4.arguments(args)
    case False{}: choose_5(mode, args)

def choose_4(+mode: String, args: List<String>) -> IO(Unit):
  selected_4(String.eq(mode, "heterogeneous-functor"), mode, args)

def selected_3(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P3.arguments(args)
    case False{}: choose_4(mode, args)

def choose_3(+mode: String, args: List<String>) -> IO(Unit):
  selected_3(String.eq(mode, "left-kan"), mode, args)

def selected_2(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P2.arguments(args)
    case False{}: choose_3(mode, args)

def choose_2(+mode: String, args: List<String>) -> IO(Unit):
  selected_2(String.eq(mode, "nattrans"), mode, args)

def selected_1(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P1.arguments(args)
    case False{}: choose_2(mode, args)

def choose_1(+mode: String, args: List<String>) -> IO(Unit):
  selected_1(String.eq(mode, "functor"), mode, args)

def selected_0(yes: Bool, mode: String, args: List<String>) -> IO(Unit):
  match yes:
    case True{}: P0.arguments(args)
    case False{}: choose_1(mode, args)

def choose_0(+mode: String, args: List<String>) -> IO(Unit):
  selected_0(String.eq(mode, "category"), mode, args)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}: IO.die(Unit, 1, "expected relational protocol mode and root")
    case Con{mode, rest}: choose_0(mode, rest)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
