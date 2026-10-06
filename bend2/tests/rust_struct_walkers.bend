import Base
import ../rust/rir.bend as R
import ../rust/erase_typed.bend as Et
import ../cli/rust_out.bend as Ro

# Direct RIR checks while source-level struct classification remains deferred.
def structure(named: Bool) -> R.Item:
  R.IStruct{"Holder", "make_holder", named,
    [R.Param{"value", R.TCon{"Foreign", []}}],
    [R.Param{"value", R.TRef{R.TCon{"Foreign", []}}}]}

def pattern(named: Bool) -> R.Pat:
  R.PStruct{"Holder", named, ["left", "right"], [R.BVar{"unused"}, R.BVar{"used"}]}

def row(label: String, ok: Bool) -> String:
  Bool.pick(String, ok, "PASS ", "FAIL ") ++ label ++ "\n"

def names(xs: List<&2, String>, expected: String) -> Bool:
  String.eq(Et.join(",", xs), expected)

def skipped(p: R.Pat) -> Bool:
  match p:
    case R.PStruct{family, named, fields, Con{R.BSkip{}, Con{R.BVar{name}, Nil{}}}}:
      String.eq(family, "Holder") && named && names(fields, "left,right") && String.eq(name, "used")
    case other:
      False{}

def kept_match(ex: R.Ex) -> Bool:
  match ex:
    case R.XMatch{scrut, arms}:
      True{}
    case other:
      False{}

def report() -> String:
  row("struct field dependencies", names(Ro.item_refs(structure(True{})), "Foreign,Foreign")) ++
  row("struct pattern dependencies", names(Ro.arm_fams([R.Arm{pattern(True{}), R.XBool{True{}}}]), "Holder")) ++
  row("struct item collision names", names(Ro.item_names(structure(True{})), "Holder,make_holder")) ++
  row("struct class collision names", names(Et.item_names(Et.CData{structure(True{})}), "Holder,make_holder")) ++
  row("struct is not Copy", names(Et.copy_names(Et.CData{structure(True{})}), "")) ++
  row("unused struct bindings", skipped(Et.skip_pat(pattern(True{}), R.XVar{"used"}))) ++
  row("struct match is preserved", kept_match(Et.match_ex(R.XVar{"holder"}, [R.Arm{pattern(True{}), R.XVar{"used"}}]))) ++
  row("named struct description", String.eq(Et.show_item(structure(True{})), "struct Holder { value: Foreign }" ++ "; fn make_holder")) ++
  row("tuple struct description", String.eq(Et.show_item(structure(False{})), "struct Holder(Foreign); fn make_holder"))

def main() -> IO(Unit):
  IO.write(report())
