import Base
import ./relational_common.bend as R
import ./surface_prelude_poly.bend as Audit
import ../kernel/error.bend as E
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/budget.bend as B

type Case is Type:
  Case{name: String, test: Unit -> Result<&2, &2, String, Unit>}

def refusal(-A: Data, +expected: E.T, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{"expected refusal: " ++ E.to_string(expected)}
    case Fail{+actual}: R.require(Audit.starts(E.to_string(expected), E.to_string(actual)), "wrong refusal: " ++ E.to_string(actual))

def exact(-A: Data, +expected: E.T, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{"expected refusal: " ++ E.to_string(expected)}
    case Fail{+actual}: R.require(String.eq(E.to_string(expected), E.to_string(actual)), "wrong refusal: " ++ E.to_string(actual))

def collision(name: String) -> E.T:
  E.Mismatch{"the name " ++ name ++ " is already declared"}

def absent(-A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case None{}: True{}
    case Some{entry}: False{}

def present(-A: Data, value: Maybe<&2, A>) -> Bool:
  Bool.not(absent(A, value))

def matched_entry(expected: G.Entry, actual: Maybe<&2, G.Entry>) -> Bool:
  match actual:
    case None{}: False{}
    case Some{actual}: R.same_entry(expected, actual)

def matched_family(expected: P.Family, actual: Maybe<&2, P.Family>) -> Bool:
  match actual:
    case None{}: False{}
    case Some{actual}: P.same_family(expected, actual)

def entries(rows: List<&2, C.Pair<String, G.Entry>>, +globals: G.T) -> Bool:
  match rows:
    case Nil{}: True{}
    case Con{C.Pair{name, value}, rest}: matched_entry(value, G.find(name, globals)) && entries(rest, globals)

def families(rows: List<&2, C.Pair<String, P.Family>>, +globals: G.T) -> Bool:
  match rows:
    case Nil{}: True{}
    case Con{C.Pair{name, value}, rest}: matched_family(value, G.find_family(name, globals)) && families(rest, globals)

def same_globals(actual: G.T, +expected: G.T) -> Bool:
  match actual expected:
    case G.Globals{+a, +af, entry_index, family_index} G.Globals{b, bf, other_entry_index, other_family_index}:
      Nat.is_eq(R.length(C.Pair<String, G.Entry>, a), R.length(C.Pair<String, G.Entry>, b)) &&
      Nat.is_eq(R.length(C.Pair<String, P.Family>, af), R.length(C.Pair<String, P.Family>, bf)) &&
      entries(a, expected) && families(af, expected)

def labeled(name: String, result: Result<&2, &2, String, Unit>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{unit}: Done{unit}
    case Fail{message}: Fail{name ++ ": " ++ message}

def run(cases: List<&1, Case>, count: Nat) -> Result<&2, &2, String, String>:
  match cases:
    case Nil{}: Done{"cases=" ++ Nat.show(count)}
    case Con{Case{name, test}, rest}:
      do Result<&2, &2, String, String>:
        checked : Unit <- labeled(name, test(Unit{}))
        run(rest, 1n+count)

def budget_error() -> E.T:
  E.Budget_exhausted{"the check budget is exhausted"}

def scope_error() -> E.T:
  E.Universe{"universe level is outside the global parameter scope"}

def measure(-A: Data, result: C.Pair<B.T, Result<&2, &2, E.T, A>>) -> Result<&2, &2, String, Nat>:
  match result:
    case C.Pair{B.Polls{remaining}, Done{value}}: Done{(100000n - remaining : Nat)}
    case C.Pair{budget, Fail{error}}: Fail{E.to_string(error)}
    case other: Fail{"finite poll measurement disappeared"}
