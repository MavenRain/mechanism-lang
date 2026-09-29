import Base
import ./term.bend as Term
import ./common.bend as Common
import ./global.bend as Global
import ./budget.bend as Budget
import ./error.bend as Error
import ./order.bend as Order

def budget_msg() -> String:
  "the totality budget is exhausted"

@unsafe def spend(state:Maybe<&2,Budget.T>,terms:List<&2,Term.T>) -> Result<&2,&2,Error.T,Budget.T>:
  match state:
    case None{}: Fail{Error.Budget_exhausted{budget_msg()}}
    case Some{budget}:
      match terms:
        case Nil{}: Done{budget}
        case Con{t,rest}: spend(Budget.poll(budget),List.append(&2,Term.T,Term.children(t),rest))

def bodies(ms:List<&2,Common.Pair<String,Term.T>>) -> List<&2,Term.T>:
  match ms:
    case Nil{}: []
    case Con{Common.Pair{n,b},rest}: b <> bodies(rest)

def guard_group(budget:Budget.T,globals:Global.T,+members:List<&2,Common.Pair<String,Term.T>>) -> Result<&2,&2,Error.T,Maybe<&2,Order.T>>:
  do Result<&2,&2,Error.T,Maybe<&2,Order.T>>:
    remaining : Budget.T <- spend(Some{budget},bodies(members))
    Order.certify(members)

def guarded_argument(cert:Maybe<&2,Order.T>) -> Maybe<&2,Nat>:
  match cert:
    case None{}: None{}
    case Some{c}: Some{Order.argument(c)}

def guard(budget:Budget.T,globals:Global.T,name:String,ty:Term.T,body:Term.T) -> Result<&2,&2,Error.T,Maybe<&2,Nat>>:
  do Result<&2,&2,Error.T,Maybe<&2,Nat>>:
    c : Maybe<&2,Order.T> <- guard_group(budget,globals,[Common.Pair{name,body}])
    return guarded_argument(c)


def tick_comp() -> Budget.Comp(Unit):
  Budget.tick_with(budget_msg())

@unsafe def spend_comp(terms: List<&2, Term.T>) -> Budget.Comp(Unit):
  match terms:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{term, rest}:
      do Budget.Comp<Unit>:
        ok : Unit <- tick_comp()
        spend_comp(List.append(&2, Term.T, Term.children(term), rest))

def guard_group_comp(globals: Global.T, +members: List<&2, Common.Pair<String, Term.T>>) -> Budget.Comp(Maybe<&2, Order.T>):
  do Budget.Comp<Maybe<&2, Order.T>>:
    ok : Unit <- spend_comp(bodies(members))
    Budget.lift(Maybe<&2, Order.T>, Order.certify(members))

def guard_comp(globals: Global.T, name: String, ty: Term.T, body: Term.T) -> Budget.Comp(Maybe<&2, Nat>):
  do Budget.Comp<Maybe<&2, Nat>>:
    cert : Maybe<&2, Order.T> <- guard_group_comp(globals, [Common.Pair{name, body}])
    return guarded_argument(cert)
