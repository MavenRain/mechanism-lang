import Base
import ./term.bend as Term
import ./level.bend as Level
import ./error.bend as Error

def valid_node(term: Term.T, arity: Nat) -> Bool:
  match term:
    case Term.Univ{level}:
      Level.in_scope(arity, level)
    case Term.Var{index}:
      True{}
    case Term.Lan{shape, body}:
      True{}
    case Term.Ran{shape, body}:
      True{}
    case Term.In{shape, addr, args}:
      True{}
    case Term.Elim{data}:
      True{}
    case Term.Sec{shape, legs}:
      True{}
    case Term.Out{shape, addr, head}:
      True{}
    case Term.Let{name, ty, value, body}:
      True{}
    case Term.Ann{body, ty}:
      True{}
    case Term.Global{name}:
      True{}
    case Term.Lit{value}:
      True{}
    case Term.Auto{}:
      True{}

@unsafe def valid_walk(todo: List<&2, Term.T>, +arity: Nat) -> Bool:
  match todo:
    case Nil{}:
      True{}
    case Con{+term, rest}:
      Bool.and(valid_node(term, arity), valid_walk(List.append(&2, Term.T, Term.children(term), rest), arity))

def check(arity: Nat, term: Term.T) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, valid_walk([term], arity), Done{Unit{}}, Fail{Error.Universe{"universe level is outside the global parameter scope"}})
