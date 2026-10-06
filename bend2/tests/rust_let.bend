import Base
import ../rust/rir.bend as R
import ../rust/infer.bend as In
import ../rust/lower.bend as Lo
import ../surface/syntax.bend as Syntax

# Direct RIR regressions: the E1 emitter slice precedes the let lifter.
def env() -> In.Env:
  In.Env{[In.Sig{"flip", [], [R.TBool{}], R.TBool{}}, In.Sig{"constant", [], [], R.TBool{}}, In.Sig{"convert", [], [R.TBool{}], R.TCon{"MechNat", []}}], [], []}

def lowered(t: R.Ty, body: R.Ex) -> String:
  Syntax.print([Lo.fn_decl(env(), ["flip", "constant", "convert"], [], "probe", [], [R.Param{"x", R.TBool{}}], t, body)])

def check(+label: String, +t: R.Ty, +value: R.Ex) -> String:
  Bool.pick(String,
    String.eq(lowered(t, R.XLet{"y", None{}, value, R.XVar{"y"}}), lowered(t, R.XLet{"y", Some{t}, value, R.XVar{"y"}})),
    "PASS " ++ label ++ "\n", "FAIL " ++ label ++ "\n")

def rejected(r: Result<&2, &2, String, List<&2, In.Note>>) -> String:
  match r:
    case Fail{message}:
      "PASS initializer type checking\n"
    case Done{notes}:
      "FAIL initializer type checking\n"

def report() -> String:
  check("variable initializer", R.TBool{}, R.XVar{"x"}) ++
  check("call initializer", R.TBool{}, R.XCall{"flip", [], [R.XVar{"x"}]}) ++
  check("nullary initializer", R.TBool{}, R.XCall{"constant", [], []}) ++
  check("different result type", R.TCon{"MechNat", []}, R.XCall{"convert", [], [R.XVar{"x"}]}) ++
  rejected(In.walk(env(), Done{In.St{[In.JChk{[], R.XLet{"y", None{}, R.XCall{"flip", [], [R.XInt{"0"}]}, R.XVar{"y"}}, R.TBool{}}], []}}))

def main() -> IO(Unit):
  IO.write(report())
