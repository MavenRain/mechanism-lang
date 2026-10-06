import Base
import ../rust/ast.bend as A
import ../rust/rir.bend as R
import ../rust/resolve.bend as Rs
import ../rust/infer.bend as In
import ../rust/lower.bend as Lo

def item(named: Bool) -> R.Item:
  R.IStruct{"Data", "data", named, [R.Param{"value", R.TBool{}}], []}

def pat(+named: Bool) -> R.Pat:
  R.PStruct{"Data", named, Bool.pick(List<&2, String>, named, ["value"], []), [R.BVar{"x"}]}

def bad(module: String, pos: A.Pos, what: String) -> Bool:
  String.eq(module, "sample") && String.eq(A.show_pos(pos), "3:5") && String.eq(what, "struct import is not supported")

def rows_failed(r: Result<&2, &2, Rs.Bad, List<&2, Rs.Row>>) -> Bool:
  match r:
    case Fail{Rs.Bad{module, pos, what}}:
      bad(module, pos, what)
    case Done{rows}:
      False{}

def rows_refused(named: Bool) -> Bool:
  rows_failed(Rs.item_rows("sample", Rs.Placed{A.Pos{3n, 5n}, item(named)}))

def typed_failed(r: Result<&2, &2, Rs.Bad, Maybe<&2, In.Typed>>) -> Bool:
  match r:
    case Fail{Rs.Bad{module, pos, what}}:
      bad(module, pos, what)
    case Done{typed}:
      False{}

def item_refused(named: Bool) -> Bool:
  typed_failed(In.item_typed(In.Env{[], [], []}, [], "sample", Rs.Placed{A.Pos{3n, 5n}, item(named)}))

def pattern_failed(r: Maybe<&2, String>) -> Bool:
  match r:
    case Some{what}:
      String.eq(what, "struct patterns are outside the import subset")
    case None{}:
      False{}

def resolve_pattern_refused(named: Bool) -> Bool:
  pattern_failed(Rs.walk(Rs.Scope{[], [], []}, [Rs.JArm{[], [], R.Arm{pat(named), R.XBool{True{}}}}]))

def step_failed(r: Result<&2, &2, String, In.St>) -> Bool:
  match r:
    case Fail{what}:
      String.eq(what, "struct patterns are outside the import subset")
    case Done{st}:
      False{}

def infer_pattern_refused(named: Bool) -> Bool:
  step_failed(In.step(In.Env{[], [], []}, In.JArm{[], In.Fam{"Data", [], [R.Variant{"data", [R.TBool{}]}]}, [], R.Arm{pat(named), R.XBool{True{}}}, R.TBool{}}, In.St{[], []}))

def not_covered(named: Bool) -> Bool:
  Bool.pick(Bool, In.covers([R.Arm{pat(named), R.XBool{True{}}}], [R.Variant{"data", [R.TBool{}]}], "Data"), False{}, True{})

def bound_names(ns: List<&2, String>) -> Bool:
  match ns:
    case Con{bound, Nil{}}:
      String.eq(bound, "x")
    case other:
      False{}

def binds_lowered(named: Bool) -> Bool:
  bound_names(Lo.arm_names([R.Arm{pat(named), R.XBool{True{}}}]))

def check(+name: String, ok: Bool) -> String:
  Bool.pick(String, ok, "PASS " ++ name ++ "\n", "FAIL " ++ name ++ "\n")

def report() -> String:
  check("resolve named struct refusal", rows_refused(True{})) ++
  check("resolve tuple struct refusal", rows_refused(False{})) ++
  check("infer named struct refusal", item_refused(True{})) ++
  check("infer tuple struct refusal", item_refused(False{})) ++
  check("resolve named struct pattern refusal", resolve_pattern_refused(True{})) ++
  check("resolve tuple struct pattern refusal", resolve_pattern_refused(False{})) ++
  check("infer named struct pattern refusal", infer_pattern_refused(True{})) ++
  check("infer tuple struct pattern refusal", infer_pattern_refused(False{})) ++
  check("named struct pattern has no enum coverage", not_covered(True{})) ++
  check("tuple struct pattern has no enum coverage", not_covered(False{})) ++
  check("named struct binding walker", binds_lowered(True{})) ++
  check("tuple struct binding walker", binds_lowered(False{}))

def main() -> IO(Unit):
  IO.write(report())
