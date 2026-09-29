import Base
import ./relational_common.bend as R
import ./surface_prenex_data.bend as Data
import ./surface_prenex_common.bend as Test
import ./surface_raw_common.bend as Raw
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/budget.bend as B
import ../kernel/error.bend as E
import ../kernel/common.bend as C

def exact(label: String, +expected: String, result: Result<&2, &2, E.T, Elab.Checked>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{label ++ ": expected refusal: " ++ expected}
    case Fail{+error}: R.require(String.eq(expected, E.to_string(error)), label ++ ": wrong refusal: " ++ E.to_string(error))

def negatives(cases: List<&2, Data.Negative>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Negative{name, kind, detail, good, source}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- exact(name, detail, Elab.check_in(globals, B.unlimited(), source))
        negatives(rest, globals)

def required_family(value: Maybe<&2, P.Family>) -> Result<&2, &2, String, P.Family>:
  match value:
    case Some{value}: Done{value}
    case None{}: Fail{"missing family"}

def family(name: String, globals: G.T) -> Result<&2, &2, String, P.Family>:
  required_family(G.find_family(name, globals))

def family_rows(globals: G.T) -> List<&2, C.Pair<String, P.Family>>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: families

def entries(globals: G.T) -> Elab.Rows:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: entries

def run(cases: List<&1, Raw.Case>, count: Nat) -> Result<&2, &2, String, Nat>:
  match cases:
    case Nil{}: Done{count}
    case Con{Raw.Case{name, test}, rest}:
      do Result<&2, &2, String, Nat>:
        checked : Unit <- Raw.labeled(name, test(Unit{}))
        run(rest, 1n+count)

def same_families(+a: G.T, +b: G.T) -> Bool:
  Nat.is_eq(Test.family_count(a), Test.family_count(b)) && Raw.families(family_rows(a), b)

def new_family_names(globals: G.T, initial: G.T) -> List<&2, String>:
  R.family_names(R.new_families(family_rows(globals), initial))

def no_axioms(rows: Elab.Rows) -> Bool:
  R.strings(Elab.disclosure_names(rows), [])

def altered(family: P.Family, mode: Nat) -> P.Family:
  match family mode:
    case P.Family{name, params, indices, level, status, ctors, positive} Zero{}:
      P.Family{name, params, indices, level, status, ctors, False{}}
    case P.Family{name, params, indices, level, status, ctors, positive} Succ{Zero{}}:
      P.Family{name, params, indices, level, P.Provisional{}, ctors, positive}
    case P.Family{name, params, indices, level, status, ctors, positive} Succ{Succ{rest}}:
      P.Family{name, params, indices, level, status, [], positive}

def mismatch(name: String) -> E.T:
  E.Mismatch{"the reused family " ++ name ++ " does not match the template"}
