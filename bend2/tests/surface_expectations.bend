import ../kernel/bignum.bend as Big
import ../kernel/budget.bend as B
import ../kernel/check.bend as C
import ../kernel/error.bend as E
import ../kernel/global.bend as G
import ../kernel/shape.bend as Shape
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../surface/elab_term.bend as ET
import ../surface/syntax.bend as S
import ./relational_common.bend as R

def good() -> T.Leg<T.T>:
  T.Leg{[], T.Global{"Nat"}}

def bad(index: Nat) -> T.Leg<T.T>:
  T.Leg{[], T.Var{index}}

def expected(count: Big.T, +legs: List<&2, T.Leg<T.T>>) -> V.T:
  V.VLan{Shape.SColl{count}, V.Closure{[], T.Sec{Shape.coll(T.T, List.length(&2, T.Leg<T.T>, legs)), legs}}, None{}}

def helper_result(result: Result<&2, &2, E.T, List<&2, Maybe<&2, V.T>>>) -> String:
  match result:
    case Fail{error}: E.to_string(error)
    case Done{values}: "OK:" ++ Nat.show(List.length(&2, Maybe<&2, V.T>, values))

def elab_result(result: Result<&2, &2, E.T, T.T>) -> String:
  match result:
    case Fail{error}: E.to_string(error)
    case Done{term}: "OK"

def probe(+name: String, +count: Big.T, +ty: Maybe<&2, V.T>, helper: String, elaborated: String) -> Result<&2, &2, String, Unit>:
  +ctx = C.make(G.initial(), B.unlimited(), 0n)
  actual_helper = helper_result(B.run(List<&2, Maybe<&2, V.T>>, ET.injection_expectations(ctx, ty, count), B.Polls{0n}))
  actual_elab = elab_result(B.run(T.T, ET.elab(ctx, ty, S.SInj{Big.zero(), count, S.SProp{}}), B.Polls{0n}))
  do Result<&2, &2, String, Unit>:
    checked : Unit <- R.require(String.eq(actual_helper, helper), name ++ ": helper result changed")
    R.require(String.eq(actual_elab, elaborated), name ++ ": public elaboration result changed")

def finite(name: String, +count: Nat, legs: List<&2, T.Leg<T.T>>, helper: String, elaborated: String) -> Result<&2, &2, String, Unit>:
  probe(name, Big.of_nat(count), Some{expected(Big.of_nat(count), legs)}, helper, elaborated)

def suite() -> Result<&2, &2, String, Unit>:
  +wrong : String = "wrong leg: the leg address is outside the collection"
  +unbound : String = "unbound: de Bruijn index 0 is outside the environment"
  +negative : String = "mismatch: a collection width must be nonnegative"
  +minus_one : Big.T = Big.Big{True{}, [1]}
  +huge = Big.mul(Big.of_nat(999999999n), Big.of_nat(1000000000n))
  do Result<&2, &2, String, Unit>:
    checked : Unit <- finite("valid-one", 1n, [good()], "OK:1", "OK")
    checked : Unit <- finite("valid-two", 2n, [good(), good()], "OK:2", "OK")
    checked : Unit <- finite("extra-good", 1n, [good(), good()], "OK:1", "OK")
    checked : Unit <- finite("extra-bad", 1n, [good(), bad(0n)], "OK:1", "OK")
    checked : Unit <- finite("missing-leg", 2n, [good()], wrong, wrong)
    checked : Unit <- finite("zero-extra-bad", 0n, [bad(0n)], "OK:0", "OK")
    checked : Unit <- finite("bad-before-missing", 2n, [bad(0n)], unbound, unbound)
    checked : Unit <- finite("first-of-two-errors", 2n, [bad(0n), bad(1n)], unbound, unbound)
    checked : Unit <- finite("zero-empty", 0n, [], "OK:0", "OK")
    checked : Unit <- probe("mismatch-width", Big.one(), Some{expected(Big.of_nat(2n), [bad(0n)])}, "OK:0", "OK")
    checked : Unit <- probe("huge-count", huge, Some{expected(huge, [good()])}, wrong, wrong)
    checked : Unit <- probe("negative-count", minus_one, Some{expected(minus_one, [good()])}, negative, negative)
    checked : Unit <- probe("negative-none", minus_one, None{}, negative, negative)
    probe("negative-mismatch", minus_one, Some{expected(Big.one(), [bad(0n)])}, negative, negative)

def arguments(args: List<String>) -> IO(Unit):
  result = do Result<&2, &2, String, String>:
    checked : Unit <- suite()
    return "cases=14 zero-poll=14"
  R.finish("SURFACE-EXPECTATIONS", result)

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
