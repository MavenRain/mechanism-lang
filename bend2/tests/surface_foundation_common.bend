import Base
import ./relational_common.bend as R
import ./surface_foundation_data.bend as Data
import ./surface_prelude_poly.bend as Audit
import ../surface/elab.bend as Elab
import ../surface/poly.bend as Poly
import ../surface/family_poly.bend as Family
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/level.bend as L
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

type Instance is Data:
  Instance{name: String, levels: List<&2, L.T>, alias: String, contract: String}

def level(index: Nat) -> L.T:
  match index:
    case 0n: L.zero()
    case 1n+p: L.succ(level(p))

def contract(contracts: List<&2, Data.Contract>, +name: String) -> Result<&2, &2, String, String>:
  match contracts:
    case Nil{}: Fail{"missing frozen contract: " ++ name}
    case Con{Data.Contract{key, source}, rest}:
      choose = Bool.pick(Unit -> Result<&2, &2, String, String>, String.eq(key, name),
        unit => Done{source}, unit => contract(rest, name))
      choose(Unit{})

def check_contract(globals: G.T, name: String, contracts: List<&2, Data.Contract>) -> Result<&2, &2, String, G.T>:
  match name:
    case SNil{}: Done{globals}
    case SCon{head, tail}:
      do Result<&2, &2, String, G.T>:
        source : String <- contract(contracts, SCon{head, tail})
        checked : Elab.Checked <- R.checked(globals, source)
        Done{R.globals(checked)}

def install_families(instances: List<&2, Instance>, +catalog: Family.T, +contracts: List<&2, Data.Contract>, globals: G.T) -> Result<&2, &2, String, G.T>:
  match instances:
    case Nil{}: Done{globals}
    case Con{Instance{name, levels, alias, contract_name}, rest}:
      do Result<&2, &2, String, G.T>:
        installed : G.T <- R.kernel(G.T, Family.instantiate(globals, catalog, B.unlimited(), name, levels, alias, [], None{}))
        checked : G.T <- check_contract(installed, contract_name, contracts)
        install_families(rest, catalog, contracts, checked)

def entry_count(globals: G.T) -> Nat:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: R.length(C.Pair<String, G.Entry>, entries)

def instance_globals(instance: C.Pair<G.T, T.T>) -> G.T:
  match instance:
    case C.Pair{globals, term}: globals

def install_terms(instances: List<&2, Instance>, +catalog: Poly.T, +contracts: List<&2, Data.Contract>, +globals: G.T, added: Nat) -> Result<&2, &2, String, C.Pair<G.T, Nat>>:
  match instances:
    case Nil{}: Done{C.Pair{globals, added}}
    case Con{Instance{name, levels, alias, contract_name}, rest}:
      do Result<&2, &2, String, C.Pair<G.T, Nat>>:
        instance : C.Pair<G.T, T.T> <- R.kernel(C.Pair<G.T, T.T>, Poly.instantiate(globals, catalog, B.unlimited(), name, levels, alias))
        +installed : G.T = instance_globals(instance)
        count : Nat = (added + (entry_count(installed) - entry_count(globals) : Nat) : Nat)
        checked : G.T <- check_contract(installed, contract_name, contracts)
        install_terms(rest, catalog, contracts, checked, count)

def negatives(cases: List<&2, Data.Negative>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Negative{name, +expected, control, source}, rest}:
      do Result<&2, &2, String, Unit>:
        accepted : Elab.Checked <- R.checked(globals, control)
        checked : Unit <- Audit.refusal(Elab.check_in(globals, B.unlimited(), source), expected, name ++ ": expected refusal: " ++ expected)
        negatives(rest, globals)

@unsafe def exhausted() -> B.Callback:
  B.Callback{unit => (exhausted(), True{})}

@unsafe def countdown(remaining: Nat) -> B.Callback:
  match remaining:
    case 0n: exhausted()
    case 1n+rest: B.Callback{unit => (countdown(rest), False{})}

def budget_refusal(-A: Data, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{"budget: expected refusal"}
    case Fail{+error}: R.require(Audit.starts("budget: the check budget is exhausted", E.to_string(error)), "budget: wrong refusal: " ++ E.to_string(error))

def zero() -> T.T:
  T.In{S.SMu{"MechNat", []}, T.ACtor{"mechZero"}, []}

def one() -> T.T:
  T.In{S.SMu{"MechNat", []}, T.ACtor{"mechSucc"}, [zero()]}

def ones(names: List<&2, String>) -> List<&2, C.Pair<String, T.T>>:
  match names:
    case Nil{}: []
    case Con{name, rest}: C.Pair{name, one()} <> ones(rest)

def audit(globals: G.T) -> Result<&2, &2, String, Unit>:
  Audit.audit(globals)
