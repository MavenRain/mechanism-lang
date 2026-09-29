import Base
import ./bignum.bend as Bignum
import ./common.bend as Common
import ./context.bend as Context
import ./term.bend as Term
import ./value.bend as Value
import ./eval.bend as Eval
import ./global.bend as Global
import ./prim.bend as Prim
import ./positivity.bend as Positivity
import ./quantity.bend as Quantity
import ./shape.bend as Shape
import ./level.bend as Level
import ./error.bend as Error

def arrow(q: Quantity.T, name: String, dom: Term.T, cod: Term.T) -> Term.T:
  Term.Ran{Shape.SPi{q, name, dom}, cod}

def term_legs(items: List<&2, Term.T>) -> List<&2, Term.Leg<Term.T>>:
  match items:
    case Nil{}:
      []
    case Con{x, rest}:
      Term.Leg{[], x} <> term_legs(rest)

def diagram_of(+items: List<&2, Term.T>) -> Term.T:
  Term.Sec{Shape.coll(Term.T,List.length(&2, Term.T, items)), term_legs(items)}

def sum_ty(+items: List<&2, Term.T>) -> Term.T:
  Term.Lan{Shape.coll(Term.T,List.length(&2, Term.T, items)), diagram_of(items)}

def prod_ty(+items: List<&2, Term.T>) -> Term.T:
  Term.Ran{Shape.coll(Term.T,List.length(&2, Term.T, items)), diagram_of(items)}

def unit_ty(level: Level.T) -> Term.T:
  Term.Ann{prod_ty([]), Term.Univ{level}}

def unit_val() -> Term.T:
  Term.Sec{Shape.coll(Term.T,0n), []}

def bool_ty() -> Term.T:
  Prim.bool_ty()

def bool_value(value: Bool) -> Value.T:
  Prim.bool_value(value)

def coll_legs(value: Value.T) -> Result<&2, &2, Error.T, List<&2, Value.VLeg<Value.T>>>:
  match value:
    case Value.VSec{shape, legs}:
      Done{legs}
    case _:
      Fail{Error.Mismatch{"the diagram is not a section of the declared width"}}

def coll_legs_of(ctx: Context.Ctx, clo: Value.Closure<Value.T>) -> Result<&2, &2, Error.T, List<&2, Value.VLeg<Value.T>>>:
  do Result<&2, &2, Error.T, List<&2, Value.VLeg<Value.T>>>:
    value : Value.T <- Eval.open_closure(Context.globals_of(ctx), clo, [])
    coll_legs(value)

def coll_leg_ty_found(leg: Maybe<&2, Value.VLeg<Value.T>>, ctx: Context.Ctx) -> Result<&2, &2, Error.T, Value.T>:
  match leg:
    case None{}:
      Fail{Error.Wrong_leg{"the leg address is outside the collection"}}
    case Some{Value.VLeg{binders, clo}}:
      Eval.open_closure(Context.globals_of(ctx), clo, [])

def coll_leg_ty(ctx: Context.Ctx, legs: List<&2, Value.VLeg<Value.T>>, index: Bignum.T) -> Result<&2, &2, Error.T, Value.T>:
  coll_leg_ty_found(Bignum.at(Value.VLeg<Value.T>, legs, index), ctx)

def force_legs(legs: List<&2, Value.VLeg<Value.T>>, +ctx: Context.Ctx) -> Result<&2, &2, Error.T, List<&2, Value.T>>:
  match legs:
    case Nil{}:
      Done{[]}
    case Con{Value.VLeg{binders, clo}, rest}:
      do Result<&2, &2, Error.T, List<&2, Value.T>>:
        value : Value.T <- Eval.open_closure(Context.globals_of(ctx), clo, [])
        values : List<&2, Value.T> <- force_legs(rest, ctx)
        return value <> values

def mu_param_env(+ctx: Context.Ctx, clo: Value.Closure<Value.T>) -> Result<&2, &2, Error.T, List<&2, Value.T>>:
  do Result<&2, &2, Error.T, List<&2, Value.T>>:
    legs : List<&2, Value.VLeg<Value.T>> <- coll_legs_of(ctx, clo)
    values : List<&2, Value.T> <- force_legs(legs, ctx)
    return List.reverse(&2, Value.T, values)

def family_ready(family: Positivity.Family) -> Result<&2, &2, Error.T, Positivity.Family>:
  match family:
    case Positivity.Family{name, params, indices, level, Positivity.Complete{names}, ctors, False{}}:
      Fail{Error.Not_yet{"a family that is not strictly positive arrives at M2"}}
    case other:
      Done{other}

def family_found(family: Maybe<&2, Positivity.Family>, name: String) -> Result<&2, &2, Error.T, Positivity.Family>:
  match family:
    case None{}:
      Fail{Error.Unbound{"the family " ++ name ++ " is not declared"}}
    case Some{value}:
      family_ready(value)

def mu_family(ctx: Context.Ctx, +name: String) -> Result<&2, &2, Error.T, Positivity.Family>:
  family_found(Global.find_family(name, Context.globals_of(ctx)), name)

@unsafe def mpc_fields(sign: Cmp, count: Bignum.T, acc: List<&2,Term.T>) -> List<&2,Term.T>:
  match sign:
    case GT{}:
      +next = Bignum.sub(count,Bignum.one())
      mpc_fields(Bignum.sign(next),next,Term.Global{"Nat"} <> acc)
    case _: acc

def mpc_sub_count(+sign: Cmp, count: Bignum.T) -> Result<&2,&2,Error.T,Term.T>:
  match sign:
    case LT{}: Fail{Error.Mismatch{"an mpc subset width must be nonnegative"}}
    case _: Done{prod_ty(mpc_fields(sign,count,[]))}

def mpc_sub_ty(+count: Bignum.T) -> Result<&2,&2,Error.T,Term.T>:
  mpc_sub_count(Bignum.sign(count),count)

def mpc_pred_ty(count: Bignum.T) -> Result<&2,&2,Error.T,Term.T>:
  do Result<&2,&2,Error.T,Term.T>:
    subset : Term.T <- mpc_sub_ty(count)
    return arrow(Quantity.QZero{}, "q", subset, Term.Univ{Level.zero()})

def mpc_party_count(count: Bignum.T) -> Result<&2,&2,Error.T,Unit>:
  Bool.pick(Result<&2,&2,Error.T,Unit>,Bignum.equal(count,Bignum.zero()),Done{Unit{}},Fail{Error.Mismatch{"the party set of an mpc type is a finite enumeration"}})

def mpc_party_value(value: Value.T) -> Result<&2, &2, Error.T, Unit>:
  match value:
    case Value.VRan{Shape.SColl{count}, clo, level}: mpc_party_count(count)
    case _: Fail{Error.Mismatch{"the party set of an mpc type is a finite enumeration"}}

def mpc_party(ctx: Context.Ctx, value: Value.T) -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    reduced : Value.T <- Eval.whnf(Context.globals_of(ctx), value)
    mpc_party_value(reduced)

def mpc_party_legs(legs: List<&2, Value.VLeg<Value.T>>, +ctx: Context.Ctx) -> Result<&2, &2, Error.T, Unit>:
  match legs:
    case Nil{}:
      Done{Unit{}}
    case Con{Value.VLeg{binders, clo}, rest}:
      do Result<&2, &2, Error.T, Unit>:
        value : Value.T <- Eval.open_closure(Context.globals_of(ctx), clo, [])
        ok : Unit <- mpc_party(ctx, value)
        mpc_party_legs(rest, ctx)

def mpc_parties_value(value: Value.T, +ctx: Context.Ctx) -> Result<&2, &2, Error.T, Bignum.T>:
  match value:
    case Value.VLan{Shape.SColl{count}, clo, level}:
      do Result<&2, &2, Error.T, Bignum.T>:
        legs : List<&2, Value.VLeg<Value.T>> <- coll_legs_of(ctx, clo)
        ok : Unit <- mpc_party_legs(legs, ctx)
        return count
    case _:
      Fail{Error.Mismatch{"the party set of an mpc type is a finite enumeration"}}

def mpc_parties(+ctx: Context.Ctx, parties: Value.T) -> Result<&2, &2, Error.T, Bignum.T>:
  do Result<&2, &2, Error.T, Bignum.T>:
    value : Value.T <- Eval.whnf(Context.globals_of(ctx), parties)
    mpc_parties_value(value, ctx)

def mu_result(+ctx: Context.Ctx, motive: Term.Motive<Term.T>, indices: List<&2, Value.T>, self: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match motive:
    case Term.Motive{ind, names, name, body}:
      Eval.eval(Context.globals_of(ctx), self <> List.append(&2, Value.T, List.reverse(&2, Value.T, indices), Context.env_of(ctx)), body)

def elim_result(+ctx: Context.Ctx, motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>, self: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match motive expected:
    case None{} None{}:
      Fail{Error.Cannot_infer{"an elimination without a motive needs an expected type"}}
    case None{} Some{value}:
      Done{value}
    case Some{Term.Motive{ind, indices, name, body}} _:
      Eval.eval(Context.globals_of(ctx), self <> Context.env_of(ctx), body)

type EtaRow is Data:
  EtaRow{ran: Bool, lan: Bool}

def eta_of(-A: Data, shape: Shape.T<A>) -> Maybe<&2,EtaRow>:
  match shape:
    case Shape.SPi{q,x,dom}: Some{EtaRow{True{},True{}}}
    case Shape.SColl{n}: Some{EtaRow{True{},False{}}}
    case Shape.SPar{a,b}: None{}
    case Shape.SNu{name,args}: None{}
    case Shape.SMu{name,args}: Some{EtaRow{False{},False{}}}
    case Shape.SZk{q,x,dom}: Some{EtaRow{False{},False{}}}
    case Shape.SFhc{l}: Some{EtaRow{False{},False{}}}
    case Shape.SMpc{p,a}: Some{EtaRow{False{},False{}}}

def samples() -> List<&2,Shape.T<Term.T>>:
  [Shape.SPi{Quantity.QMany{},"x",Term.Univ{Level.zero()}},Shape.coll(Term.T,0n),Shape.SPar{Term.Univ{Level.zero()},Term.Univ{Level.zero()}},Shape.SMu{"x",[]},Shape.SNu{"x",[]},Shape.SZk{Quantity.QZero{},"w",Term.Univ{Level.zero()}},Shape.SFhc{Term.Univ{Level.zero()}},Shape.SMpc{Term.Univ{Level.zero()},Term.Univ{Level.zero()}}]

def eta_entry(row: Maybe<&2,EtaRow>, name: String) -> List<&2,Common.Pair<String,EtaRow>>:
  match row:
    case None{}: []
    case Some{row}: [Common.Pair{name,row}]

def eta_rows(shapes: List<&2,Shape.T<Term.T>>) -> List<&2,Common.Pair<String,EtaRow>>:
  match shapes:
    case Nil{}: []
    case Con{+s,rest}: List.append(&2,Common.Pair<String,EtaRow>,eta_entry(eta_of(Term.T,s),Shape.name(Term.T,s)),eta_rows(rest))

def eta_table() -> List<&2,Common.Pair<String,EtaRow>>:
  eta_rows(samples())

def admitted_names(rows: List<&2,Common.Pair<String,EtaRow>>) -> List<&2,String>:
  match rows:
    case Nil{}: []
    case Con{Common.Pair{name,eta},rest}: name <> admitted_names(rest)

def admitted() -> List<&2,String>:
  admitted_names(eta_table())

def named_declared() -> List<&2,String>:
  ["proof-irrelevance","subsingleton-large-elimination","literal-fast-path","zk-fhc","zk-mpc","fhc-mpc"]

def named_present() -> List<&2,String>:
  ["proof-irrelevance","subsingleton-large-elimination","literal-fast-path"]
