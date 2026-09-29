import Base
import ./eterm.bend as E
import ./term.bend as T
import ./common.bend as P
import ./erase_repr.bend as R
import ./quantity.bend as Q

def member(+n: Nat, xs: List<&2,Nat>) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{x,rest}: Bool.or(Nat.is_eq(n,x),member(n,rest))

law insert_order:
  for before: Bool
  for same: Bool
  for +n: Nat
  for x: Nat
  for rest: List<&2,Nat>
  List<&2,Nat>

@unsafe def insert(+n: Nat, xs: List<&2,Nat>) -> List<&2,Nat>:
  match xs:
    case Nil{}: [n]
    case Con{+x,rest}:
      insert_order(Nat.is_lt(n,x),Nat.is_eq(n,x),n,x,rest)

@unsafe def insert_order(before,same,n,x,rest):
  match before same:
    case True{} _: n <> x <> rest
    case False{} True{}: x <> rest
    case False{} False{}: x <> insert(n,rest)

def sorted(xs: List<&2,Nat>) -> List<&2,Nat>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: insert(x,sorted(rest))

type FreeWork is Data:
  FreeWork{depth: Nat, term: T.T}

def term_work(terms: List<&2,T.T>, +depth: Nat) -> List<&2,FreeWork>:
  match terms:
    case Nil{}: []
    case Con{term,rest}: FreeWork{depth,term} <> term_work(rest,depth)

def leg_work(legs: List<&2,T.Leg<T.T>>, +depth: Nat) -> List<&2,FreeWork>:
  match legs:
    case Nil{}: []
    case Con{T.Leg{binders,body},rest}: FreeWork{Nat.add(depth,List.length(&2,P.Pair<Q.T,String>,binders)),body} <> leg_work(rest,depth)

def branch_work(branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>, +depth: Nat) -> List<&2,FreeWork>:
  match branches:
    case Nil{}: []
    case Con{P.Pair{a,T.Leg{binders,body}},rest}: FreeWork{Nat.add(depth,List.length(&2,P.Pair<Q.T,String>,binders)),body} <> branch_work(rest,depth)

def addr_work(addr: T.Addr<T.T>, depth: Nat) -> List<&2,FreeWork>:
  match addr:
    case T.APt{q,arg}: [FreeWork{depth,arg}]
    case _: []

def free_children(term: T.T, +depth: Nat) -> List<&2,FreeWork>:
  match term:
    case T.In{s,a,args}: List.append(&2,FreeWork,addr_work(a,depth),term_work(args,depth))
    case T.Out{s,a,head}: List.append(&2,FreeWork,addr_work(a,depth),[FreeWork{depth,head}])
    case T.Sec{s,legs}: leg_work(legs,depth)
    case T.Elim{T.ElimData{s,scrut,q,m,branches}}: FreeWork{depth,scrut} <> branch_work(branches,depth)
    case T.Let{x,ty,value,body}: [FreeWork{depth,value},FreeWork{1n+depth,body}]
    case T.Ann{body,ty}: [FreeWork{depth,body}]
    case _: []

@unsafe def free_walk(work: List<&2,FreeWork>) -> List<&2,Nat>:
  match work:
    case Nil{}: []
    case Con{FreeWork{+depth,T.Var{+index}},rest}:
      List.append(&2,Nat,Bool.pick(List<&2,Nat>,Nat.is_ge(index,depth),[Nat.sub(index,depth)],[]),free_walk(rest))
    case Con{FreeWork{depth,term},rest}: free_walk(List.append(&2,FreeWork,free_children(term,depth),rest))

def free_vars(depth: Nat, term: T.T) -> List<&2,Nat>:
  free_walk([FreeWork{depth,term}])

def lookup(slots: List<&2,R.Slot>, index: Nat, +seen: Nat) -> Maybe<&2,Nat>:
  match slots:
    case Nil{}: None{}
    case Con{R.SExtra{},rest}: lookup(rest,index,1n+seen)
    case Con{R.SKeep{},rest}:
      match index:
        case 0n: Some{seen}
        case 1n+p: lookup(rest,p,1n+seen)
    case Con{R.SDrop{},rest}:
      match index:
        case 0n: None{}
        case 1n+p: lookup(rest,p,seen)

def is_some(value: Maybe<&2,Nat>) -> Bool:
  match value:
    case None{}: False{}
    case Some{x}: True{}

def filter_caps(xs: List<&2,Nat>, +slots: List<&2,R.Slot>) -> List<&2,Nat>:
  match xs:
    case Nil{}: []
    case Con{+x,rest}: List.append(&2,Nat,Bool.pick(List<&2,Nat>,is_some(lookup(slots,x,0n)),[x],[]),filter_caps(rest,slots))

def captures_of(ec: R.Ctx, term: T.T) -> List<&2,Nat>:
  sorted(filter_caps(free_vars(0n,term),R.slots(ec)))

def frame(slots: List<&2,R.Slot>, +caps: List<&2,Nat>, +index: Nat) -> List<&2,R.Slot>:
  match slots:
    case Nil{}: []
    case Con{R.SExtra{},rest}: frame(rest,caps,index)
    case Con{R.SDrop{},rest}: R.SDrop{} <> frame(rest,caps,1n+index)
    case Con{R.SKeep{},rest}: R.keep_slot(member(index,caps)) <> frame(rest,caps,1n+index)

type RuntimeWork is Data:
  RuntimeWork{depth: Nat, term: E.Ktm}

def runtime_work(terms: List<&2,E.Ktm>, +depth: Nat) -> List<&2,RuntimeWork>:
  match terms:
    case Nil{}: []
    case Con{t,rest}: RuntimeWork{depth,t} <> runtime_work(rest,depth)

def runtime_branch_work(branches: List<&2,E.Branch<E.Ktm>>, +depth: Nat) -> List<&2,RuntimeWork>:
  match branches:
    case Nil{}: []
    case Con{E.Branch{tag,arity,body},rest}: RuntimeWork{Nat.add(depth,arity),body} <> runtime_branch_work(rest,depth)

def runtime_children(term: E.Ktm, +depth: Nat) -> List<&2,RuntimeWork>:
  match term:
    case E.KLet{x,v,b}: [RuntimeWork{depth,v},RuntimeWork{1n+depth,b}]
    case E.KClos{f,n,args}: runtime_work(args,depth)
    case E.KDelay{f,args}: runtime_work(args,depth)
    case E.KApp{h,args}: RuntimeWork{depth,h} <> runtime_work(args,depth)
    case E.KTail{h,args}: RuntimeWork{depth,h} <> runtime_work(args,depth)
    case E.KStruct{tid,args}: runtime_work(args,depth)
    case E.KTag{tid,tag,args}: runtime_work(args,depth)
    case E.KProj{tid,index,value}: [RuntimeWork{depth,value}]
    case E.KForce{value}: [RuntimeWork{depth,value}]
    case E.KCase{tid,value,branches}: RuntimeWork{depth,value} <> runtime_branch_work(branches,depth)
    case _: []

@unsafe def runtime_walk(work: List<&2,RuntimeWork>) -> List<&2,Nat>:
  match work:
    case Nil{}: []
    case Con{RuntimeWork{+depth,E.KVar{+index}},rest}:
      List.append(&2,Nat,Bool.pick(List<&2,Nat>,Nat.is_ge(index,depth),[Nat.sub(index,depth)],[]),runtime_walk(rest))
    case Con{RuntimeWork{depth,term},rest}: runtime_walk(List.append(&2,RuntimeWork,runtime_children(term,depth),rest))

def runtime_vars(depth: Nat, term: E.Ktm) -> List<&2,Nat>:
  runtime_walk([RuntimeWork{depth,term}])

type Reindex is Data:
  Shift{amount: Nat}
  Prune{params: Nat, live: List<&2,Nat>}

def count_before(xs: List<&2,Nat>, +index: Nat) -> Nat:
  match xs:
    case Nil{}: 0n
    case Con{x,rest}: Nat.add(Bool.pick(Nat,Nat.is_lt(x,index),1n,0n),count_before(rest,index))

def reindex(index: Reindex, +i: Nat) -> Nat:
  match index:
    case Shift{amount}: Nat.add(i,amount)
    case Prune{+params,live}: Bool.pick(Nat,Nat.is_lt(i,params),i,Nat.add(params,count_before(live,Nat.sub(i,params))))

law reindex_runtime:
  for +index: Reindex
  for +depth: Nat
  for term: E.Ktm
  E.Ktm

@unsafe def reindex_list(terms: List<&2,E.Ktm>, +index: Reindex, +depth: Nat) -> List<&2,E.Ktm>:
  match terms:
    case Nil{}: []
    case Con{t,rest}: reindex_runtime(index,depth,t) <> reindex_list(rest,index,depth)

@unsafe def reindex_branches(branches: List<&2,E.Branch<E.Ktm>>, +index: Reindex, +depth: Nat) -> List<&2,E.Branch<E.Ktm>>:
  match branches:
    case Nil{}: []
    case Con{E.Branch{tag,+arity,body},rest}: E.Branch{tag,arity,reindex_runtime(index,Nat.add(depth,arity),body)} <> reindex_branches(rest,index,depth)

@unsafe def reindex_runtime(index,depth,term):
  match term:
    case E.KVar{+i}: E.KVar{Bool.pick(Nat,Nat.is_ge(i,depth),Nat.add(depth,reindex(index,Nat.sub(i,depth))),i)}
    case E.KLet{x,v,b}: E.KLet{x,reindex_runtime(index,depth,v),reindex_runtime(index,1n+depth,b)}
    case E.KClos{f,n,args}: E.KClos{f,n,reindex_list(args,index,depth)}
    case E.KDelay{f,args}: E.KDelay{f,reindex_list(args,index,depth)}
    case E.KApp{h,args}: E.KApp{reindex_runtime(index,depth,h),reindex_list(args,index,depth)}
    case E.KTail{h,args}: E.KTail{reindex_runtime(index,depth,h),reindex_list(args,index,depth)}
    case E.KStruct{tid,args}: E.KStruct{tid,reindex_list(args,index,depth)}
    case E.KTag{tid,tag,args}: E.KTag{tid,tag,reindex_list(args,index,depth)}
    case E.KProj{tid,k,value}: E.KProj{tid,k,reindex_runtime(index,depth,value)}
    case E.KForce{value}: E.KForce{reindex_runtime(index,depth,value)}
    case E.KCase{tid,value,branches}: E.KCase{tid,reindex_runtime(index,depth,value),reindex_branches(branches,index,depth)}
    case _: term

def shift_runtime(by: Nat, depth: Nat, term: E.Ktm) -> E.Ktm:
  reindex_runtime(Shift{by},depth,term)

def live_captures(xs: List<&2,Nat>, +params: Nat, +count: Nat) -> List<&2,Nat>:
  match xs:
    case Nil{}: []
    case Con{+i,rest}: List.append(&2,Nat,Bool.pick(List<&2,Nat>,Bool.and(Nat.is_ge(i,params),Nat.is_lt(i,Nat.add(params,count))),[Nat.sub(i,params)],[]),live_captures(rest,params,count))

def filter_live(-A: Data, xs: List<&2,A>, +live: List<&2,Nat>, +count: Nat, +index: Nat) -> List<&2,A>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: List.append(&2,A,Bool.pick(List<&2,A>,member(Nat.sub(count,1n+index),live),[x],[]),filter_live(A,rest,live,count,1n+index))

type Pruned is Data:
  Pruned{reprs: List<&2,E.Repr>, args: List<&2,E.Ktm>, body: E.Ktm}

def prune_captures(+ps: List<&2,E.Repr>, args: List<&2,E.Ktm>, +params: Nat, +body: E.Ktm) -> Pruned:
  +count = List.length(&2,E.Repr,ps)
  +live = sorted(live_captures(runtime_vars(0n,body),params,count))
  Pruned{filter_live(E.Repr,ps,live,count,0n),filter_live(E.Ktm,args,live,count,0n),reindex_runtime(Prune{params,live},0n,body)}
