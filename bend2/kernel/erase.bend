import Base
import ./eterm.bend as E
import ./quantity.bend as Q

import ./term.bend as T
import ./value.bend as V
import ./shape.bend as S
import ./global.bend as G
import ./context.bend as C
import ./check_engine.bend as K
import ./rules.bend as Rules
import ./eval.bend as Eval
import ./positivity.bend as Pos
import ./budget.bend as B
import ./common.bend as P
import ./error.bend as Error
import ./erase_repr.bend as R
import ./erase_util.bend as U
import ./bignum.bend as Big

type Entry is Data:
  Dropped{}
  Postulate{repr:E.Repr}
  Code{decls:List<&2,E.Kdecl>}

type Slot is Data:
  SDrop{}
  SKeep{}
  SExtra{}

type Found is Data:
  LKeep{index:Nat}
  LDrop{}
  LOut{}

type Acc is Data:
  Acc{next:Nat, lifted:List<&2,E.Kdecl>, groups:List<&2,E.Tid>}

def lookup(slots:List<&2,Slot>, ix:Nat, seen:Nat) -> Found:
  match slots:
    case Nil{}: LOut{}
    case Con{SExtra{},rest}: lookup(rest,ix,1n+seen)
    case Con{SKeep{},rest}:
      match ix:
        case 0n: LKeep{seen}
        case 1n+p: lookup(rest,p,1n+seen)
    case Con{SDrop{},rest}:
      match ix:
        case 0n: LDrop{}
        case 1n+p: lookup(rest,p,seen)

def quantity_runtime(q:Q.T) -> Bool:
  match q:
    case Q.QZero{}: False{}
    case Q.QOne{}: True{}
    case Q.QMany{}: True{}


type Out is Data:
  Out{value: E.Ktm, acc: Acc}

type Outs is Data:
  Outs{values: List<&2,E.Ktm>, acc: Acc}

type Chain is Data:
  Chain{params: List<&2,E.Repr>, result: E.Repr, body: E.Ktm, acc: Acc}

type Signature is Data:
  Signature{params: List<&2,E.Repr>, result: E.Repr}

law term:
  for +ec: R.Ctx
  for ac: Acc
  for tail: Bool
  for expected: Maybe<&2,V.T>
  for +t: T.T
  B.Comp(Out)

law chain:
  for +ec: R.Ctx
  for ac: Acc
  for params: List<&2,E.Repr>
  for +ty: V.T
  for +t: T.T
  B.Comp(Chain)

law remaining_signature:
  for +ec: R.Ctx
  for shape: S.T<V.T>
  for clo: V.Closure<V.T>
  B.Comp(Signature)

def out_value(out: Out) -> E.Ktm:
  match out:
    case Out{value,ac}: value

def out_acc(out: Out) -> Acc:
  match out:
    case Out{value,ac}: ac

def outs_values(out: Outs) -> List<&2,E.Ktm>:
  match out:
    case Outs{values,ac}: values

def outs_acc(out: Outs) -> Acc:
  match out:
    case Outs{values,ac}: ac

def add_lift(ac: Acc, decl: E.Kdecl) -> Acc:
  match ac:
    case Acc{n,ls,gs}: Acc{n,List.append(&2,E.Kdecl,ls,[decl]),gs}

def add_groups(ac: Acc, tids: List<&2,E.Tid>) -> Acc:
  match ac:
    case Acc{n,ls,gs}: Acc{n,ls,List.append(&2,E.Tid,gs,tids)}

def next(ac: Acc) -> Nat:
  match ac:
    case Acc{n,ls,gs}: n

def advance(ac: Acc) -> Acc:
  match ac:
    case Acc{n,ls,gs}: Acc{1n+n,ls,gs}

def position_ty(ec: R.Ctx, expected: Maybe<&2,V.T>, t: T.T) -> B.Comp(V.T):
  match expected:
    case Some{ty}: B.pure(V.T,ty)
    case None{}: R.infer(ec,t)

def var_found(found: Maybe<&2,Nat>, ac: Acc) -> B.Comp(Out):
  match found:
    case Some{index}: B.pure(Out,Out{E.KVar{index},ac})
    case None{}: B.pure(Out,Out{E.KErased{},ac})

def kernel_slots(slots: List<&2,R.Slot>) -> Nat:
  match slots:
    case Nil{}: 0n
    case Con{R.SExtra{},rest}: kernel_slots(rest)
    case Con{slot,rest}: 1n+kernel_slots(rest)

def var_arm(+ec: R.Ctx, ac: Acc, +ix: Nat) -> B.Comp(Out):
  do B.Comp<Out>:
    present : Unit <- K.require(Nat.is_lt(ix,kernel_slots(R.slots(ec))),Error.Unbound{"the erasure context has no binder at index " ++ Nat.show(ix)})
    var_found(U.lookup(R.slots(ec),ix,0n),ac)

@unsafe def erase_fold(jobs: List<&2,P.Pair<Maybe<&2,V.T>,T.T>>, +ec: R.Ctx, ac: Acc) -> B.Comp(Outs):
  match jobs:
    case Nil{}: B.pure(Outs,Outs{[],ac})
    case Con{P.Pair{expected,t},rest}:
      do B.Comp<Outs>:
        +one : Out <- term(ec,ac,False{},expected,t)
        +more : Outs <- erase_fold(rest,ec,out_acc(one))
        return Outs{out_value(one) <> outs_values(more),outs_acc(more)}

def as_lam(t: T.T) -> Maybe<&2,T.Leg<T.T>>:
  match t:
    case T.Sec{S.SPi{q,x,dom},[leg]}: Some{leg}
    case _: None{}

def function_node(value: V.T) -> Bool:
  match value:
    case V.VRan{S.SPi{q,x,dom},d,l}: True{}
    case V.VRan{S.SZk{q,x,dom},d,l}: True{}
    case _: False{}

def function_result(ec: R.Ctx, ty: V.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    w : V.T <- R.whnf(ec,ty)
    return function_node(w)

@unsafe def host_arg_keep(keep: Bool, ec: R.Ctx, ac: Acc, ty: V.T, t: T.T) -> B.Comp(Outs):
  match keep:
    case False{}: B.pure(Outs,Outs{[],ac})
    case True{}:
      do B.Comp<Outs>:
        +out : Out <- term(ec,ac,False{},Some{ty},t)
        return Outs{[out_value(out)],out_acc(out)}

def host_arg(+ec: R.Ctx, ac: Acc, +t: T.T) -> B.Comp(Outs):
  do B.Comp<Outs>:
    +ty : V.T <- R.infer(ec,t)
    keep : Bool <- R.runtime_ty(ec,ty)
    host_arg_keep(keep,ec,ac,ty,t)

def host_args(ts: List<&2,T.T>, +ec: R.Ctx, ac: Acc) -> B.Comp(Outs):
  match ts:
    case Nil{}: B.pure(Outs,Outs{[],ac})
    case Con{t,rest}:
      do B.Comp<Outs>:
        +out : Outs <- host_arg(ec,ac,t)
        +more : Outs <- host_args(rest,ec,outs_acc(out))
        return Outs{List.append(&2,E.Ktm,outs_values(out),outs_values(more)),outs_acc(more)}

def host_call(ec: R.Ctx, ac: Acc, name: String, ts: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +args : Outs <- host_args(ts,ec,ac)
    return Out{E.KApp{E.KGlobal{name},outs_values(args)},outs_acc(args)}

def leg_arg(leg: T.Leg<T.T>, message: String) -> B.Comp(T.T):
  match leg:
    case T.Leg{[],body}: B.pure(T.T,body)
    case _: B.failed(T.T,Error.Mismatch{message})

def fhc_pick(is_eval: Bool, ec: R.Ctx, ac: Acc, level: T.T, first: T.T, second: T.T) -> B.Comp(Out):
  match is_eval:
    case True{}: host_call(ec,ac,"fhcEval",[level,first,second])
    case False{}: host_call(ec,ac,"fhcEnc",[level,second])

def fhc_sec(+ec: R.Ctx, ac: Acc, level: T.T, legs: List<&2,T.Leg<T.T>>) -> B.Comp(Out):
  match legs:
    case [a,b]:
      do B.Comp<Out>:
        +first : T.T <- leg_arg(a,"a ciphertext section takes two arguments")
        second : T.T <- leg_arg(b,"a ciphertext section takes two arguments")
        ty : V.T <- R.infer(ec,first)
        is_eval : Bool <- function_result(ec,ty)
        fhc_pick(is_eval,ec,ac,level,first,second)
    case _: B.failed(Out,Error.Mismatch{"a ciphertext section takes two arguments"})

def leg_args(legs: List<&2,T.Leg<T.T>>, +message: String) -> B.Comp(List<&2,T.T>):
  match legs:
    case Nil{}: B.pure(List<&2,T.T>,[])
    case Con{leg,rest}:
      do B.Comp<List<&2,T.T>>:
        t : T.T <- leg_arg(leg,message)
        more : List<&2,T.T> <- leg_args(rest,message)
        return t <> more

def mpc_args(args: List<&2,T.T>, ec: R.Ctx, ac: Acc) -> B.Comp(Out):
  match args:
    case Nil{}: B.failed(Out,Error.Mismatch{"an mpc section takes one argument or more"})
    case [x]: host_call(ec,ac,"mpcInput",[x])
    case [p,x]: host_call(ec,ac,"mpcInput",[p,x])
    case Con{p,Con{f,Con{c,cs}}}: host_call(ec,ac,"mpcShare",p <> f <> c <> cs)

def mpc_sec(ec: R.Ctx, ac: Acc, legs: List<&2,T.Leg<T.T>>) -> B.Comp(Out):
  do B.Comp<Out>:
    args : List<&2,T.T> <- leg_args(legs,"an mpc section leg binds nothing")
    mpc_args(args,ec,ac)

def zk_args(args: List<&2,T.T>, ec: R.Ctx, ac: Acc, point: T.T) -> B.Comp(Out):
  match args:
    case [relation,inst]: host_call(ec,ac,"zkProve",[inst,point])
    case [relation]: host_call(ec,ac,"zkProve",[point])
    case _: B.failed(Out,Error.Mismatch{"a proof carries one relation leg"})

def zk_in(ec: R.Ctx, ac: Acc, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  match addr:
    case T.APt{q,point}: zk_args(args,ec,ac,point)
    case _: B.failed(Out,Error.Wrong_leg{"a proof takes the point address"})

def tuple_jobs(legs: List<&2,T.Leg<T.T>>, types: List<&2,V.T>, flags: List<&2,Bool>) -> List<&2,P.Pair<Maybe<&2,V.T>,T.T>>:
  match legs types flags:
    case Con{T.Leg{bs,body},ls} Con{ty,tys} Con{True{},fs}: P.Pair{Some{ty},body} <> tuple_jobs(ls,tys,fs)
    case Con{leg,ls} Con{ty,tys} Con{False{},fs}: tuple_jobs(ls,tys,fs)
    case _ _ _: []

def tuple_finish(fields: List<&2,E.Ktm>, ac: Acc, ec: R.Ctx, ty: V.T) -> B.Comp(Out):
  match fields:
    case Nil{}: B.pure(Out,Out{E.KErased{},ac})
    case Con{x,xs}:
      do B.Comp<Out>:
        tid : E.Tid <- R.tid_of(ec,ty)
        return Out{E.KStruct{tid,x <> xs},ac}

def tuple_type(value: V.T, +ec: R.Ctx, ac: Acc, ty: V.T, count: Big.T, legs: List<&2,T.Leg<T.T>>) -> B.Comp(Out):
  match value:
    case V.VRan{s,clo,l}:
      do B.Comp<Out>:
        +tys : List<&2,V.T> <- R.leg_types(ec,count,clo)
        flags : List<&2,Bool> <- R.flags(tys,ec)
        +out : Outs <- erase_fold(tuple_jobs(legs,tys,flags),ec,ac)
        tuple_finish(outs_values(out),outs_acc(out),ec,ty)
    case _: B.failed(Out,Error.Mismatch{"a tuple stands at a type that is not a right former"})

def tuple_arm(+ec: R.Ctx, ac: Acc, +ty: V.T, count: Big.T, legs: List<&2,T.Leg<T.T>>) -> B.Comp(Out):
  do B.Comp<Out>:
    value : V.T <- R.whnf(ec,ty)
    tuple_type(value,ec,ac,ty,count,legs)

def capture_reprs(caps: List<&2,Nat>, +ec: R.Ctx) -> B.Comp(List<&2,E.Repr>):
  match caps:
    case Nil{}: B.pure(List<&2,E.Repr>,[])
    case Con{ix,rest}:
      do B.Comp<List<&2,E.Repr>>:
        ty : V.T <- R.infer(ec,T.Var{ix})
        repr : E.Repr <- R.repr_of(ec,ty)
        more : List<&2,E.Repr> <- capture_reprs(rest,ec)
        return repr <> more

def capture_args(caps: List<&2,Nat>, +ec: R.Ctx, +ac: Acc) -> B.Comp(List<&2,E.Ktm>):
  match caps:
    case Nil{}: B.pure(List<&2,E.Ktm>,[])
    case Con{ix,rest}:
      do B.Comp<List<&2,E.Ktm>>:
        out : Out <- var_arm(ec,ac,ix)
        more : List<&2,E.Ktm> <- capture_args(rest,ec,ac)
        return out_value(out) <> more

def lift_pruned(pruned: U.Pruned, +fid: E.Fid, +params: List<&2,E.Repr>, ret: E.Repr, ac: Acc) -> Out:
  match pruned:
    case U.Pruned{reprs,args,body}:
      Out{E.KClos{fid,List.length(&2,E.Repr,params),args},add_lift(ac,E.KFun{fid,List.append(&2,E.Repr,reprs,params),ret,body})}

def lift_finish(result: Chain, reprs: List<&2,E.Repr>, args: List<&2,E.Ktm>, fid: E.Fid) -> Out:
  match result:
    case Chain{+params,ret,body,ac}:
      lift_pruned(U.prune_captures(reprs,args,List.length(&2,E.Repr,params),body),fid,params,ret,ac)

@unsafe def lift_arm(+ec: R.Ctx, +ac: Acc, ty: V.T, +t: T.T) -> B.Comp(Out):
  do B.Comp<Out>:
    +caps : List<&2,Nat> = U.captures_of(ec,t)
    +outer : List<&2,Nat> = List.reverse(&2,Nat,caps)
    reprs : List<&2,E.Repr> <- capture_reprs(outer,ec)
    args : List<&2,E.Ktm> <- capture_args(outer,ec,ac)
    fid : E.Fid = E.Fid{R.self(ec) ++ "$" ++ Nat.show(next(ac))}
    result : Chain <- chain(R.replace_slots(ec,U.frame(R.slots(ec),caps,0n)),advance(ac),[],ty,t)
    return lift_finish(result,reprs,args,fid)

def add_param(ec: R.Ctx, keep: Bool, ty: V.T, params: List<&2,E.Repr>) -> B.Comp(List<&2,E.Repr>):
  match keep:
    case False{}: B.pure(List<&2,E.Repr>,params)
    case True{}:
      do B.Comp<List<&2,E.Repr>>:
        repr : E.Repr <- R.repr_of(ec,ty)
        return List.append(&2,E.Repr,params,[repr])

@unsafe def chain_stop(+ec: R.Ctx, ac: Acc, params: List<&2,E.Repr>, +ty: V.T, t: T.T) -> B.Comp(Chain):
  do B.Comp<Chain>:
    ret : E.Repr <- R.repr_of(ec,ty)
    +out : Out <- term(ec,ac,True{},Some{ty},t)
    return Chain{params,ret,out_value(out),out_acc(out)}

@unsafe def chain_point(+ec: R.Ctx, ac: Acc, params: List<&2,E.Repr>, +q: Q.T, name: String, +dom: V.T, clo: V.Closure<V.T>, leg: T.Leg<T.T>) -> B.Comp(Chain):
  match leg:
    case T.Leg{binders,body}:
      do B.Comp<Chain>:
        +keep : Bool <- R.point_runtime(ec,q,dom)
        params : List<&2,E.Repr> <- add_param(ec,keep,dom,params)
        cod : V.T <- R.open(ec,clo,[V.var(R.size(ec))])
        chain(R.bind(ec,name,q,dom,R.keep_slot(keep)),ac,params,cod,body)

law signature_value:
  for value: V.T
  for ec: R.Ctx
  for cod: V.T
  for here: List<&2,E.Repr>
  B.Comp(Signature)

@unsafe def signature_rest(pair: P.Pair<R.Ctx,V.T>, here: List<&2,E.Repr>) -> B.Comp(Signature):
  match pair:
    case P.Pair{+ec,+cod}:
      do B.Comp<Signature>:
        value : V.T <- R.whnf(ec,cod)
        signature_value(value,ec,cod,here)

def append_signature(sig: Signature, here: List<&2,E.Repr>) -> Signature:
  match sig:
    case Signature{params,ret}: Signature{List.append(&2,E.Repr,here,params),ret}

@unsafe def signature_value(value,ec,cod,here):
  match value:
    case V.VRan{s,d,l}:
      do B.Comp<Signature>:
        sig : Signature <- remaining_signature(ec,s,d)
        return append_signature(sig,here)
    case _:
      do B.Comp<Signature>:
        ret : E.Repr <- R.repr_of(ec,cod)
        return Signature{here,ret}

@unsafe def signature_point(+ec: R.Ctx, +q: Q.T, name: String, +dom: V.T, clo: V.Closure<V.T>) -> B.Comp(Signature):
  do B.Comp<Signature>:
    keep : Bool <- R.point_runtime(ec,q,dom)
    here : List<&2,E.Repr> <- add_param(ec,keep,dom,[])
    pair : P.Pair<R.Ctx,V.T> <- R.under_point(ec,name,q,dom,clo)
    signature_rest(pair,here)

@unsafe def remaining_signature(ec,shape,clo):
  match shape:
    case S.SPi{q,x,dom}: signature_point(ec,q,x,dom,clo)
    case S.SZk{q,x,dom}: signature_point(ec,q,x,dom,clo)
    case _:
      do B.Comp<Signature>:
        repr : E.Repr <- R.ran_repr(ec,shape,clo)
        return Signature{[],repr}

def eta_args(n: Nat) -> List<&2,E.Ktm>:
  match n:
    case 0n: []
    case 1n+ +p: E.KVar{p} <> eta_args(p)

def eta_finish(out: Out, sig: Signature, params: List<&2,E.Repr>) -> Chain:
  match out sig:
    case Out{head,ac} Signature{+extra,ret}:
      +n = List.length(&2,E.Repr,extra)
      Chain{List.append(&2,E.Repr,params,extra),ret,E.KTail{U.shift_runtime(n,0n,head),eta_args(n)},ac}

@unsafe def eta_chain(+ec: R.Ctx, ac: Acc, params: List<&2,E.Repr>, ty: V.T, t: T.T, s: S.T<V.T>, d: V.Closure<V.T>) -> B.Comp(Chain):
  do B.Comp<Chain>:
    out : Out <- term(ec,ac,False{},Some{ty},t)
    sig : Signature <- remaining_signature(ec,s,d)
    return eta_finish(out,sig,params)

@unsafe def chain_shape(leg: Maybe<&2,T.Leg<T.T>>, shape: S.T<V.T>, ec: R.Ctx, ac: Acc, params: List<&2,E.Repr>, ty: V.T, t: T.T, clo: V.Closure<V.T>) -> B.Comp(Chain):
  match leg shape:
    case Some{lg} S.SPi{q,x,dom}: chain_point(ec,ac,params,q,x,dom,clo,lg)
    case Some{lg} S.SZk{q,x,dom}: chain_point(ec,ac,params,q,x,dom,clo,lg)
    case None{} S.SPi{q,x,dom}: eta_chain(ec,ac,params,ty,t,S.SPi{q,x,dom},clo)
    case None{} S.SZk{q,x,dom}: eta_chain(ec,ac,params,ty,t,S.SZk{q,x,dom},clo)
    case _ _: chain_stop(ec,ac,params,ty,t)

def chain_value(value: V.T, ec: R.Ctx, ac: Acc, params: List<&2,E.Repr>, ty: V.T, +t: T.T) -> B.Comp(Chain):
  match value:
    case V.VRan{s,d,l}: chain_shape(as_lam(t),s,ec,ac,params,ty,t,d)
    case _: chain_stop(ec,ac,params,ty,t)

@unsafe def chain(ec,ac,params,ty,t):
  do B.Comp<Chain>:
    value : V.T <- R.whnf(ec,ty)
    chain_value(value,ec,ac,params,ty,t)

def one_term(args: List<&2,T.T>, message: String) -> B.Comp(T.T):
  match args:
    case [arg]: B.pure(T.T,arg)
    case _: B.failed(T.T,Error.Mismatch{message})

def point_term(addr: T.Addr<T.T>, message: String) -> B.Comp(T.T):
  match addr:
    case T.APt{q,arg}: B.pure(T.T,arg)
    case _: B.failed(T.T,Error.Mismatch{message})

def under_runtime(pair: P.Pair<R.Ctx,V.T>) -> B.Comp(Bool):
  match pair:
    case P.Pair{ec,ty}: R.runtime_ty(ec,ty)

def job(keep: Bool, ty: V.T, t: T.T) -> List<&2,P.Pair<Maybe<&2,V.T>,T.T>>:
  Bool.pick(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>,keep,[P.Pair{Some{ty},t}],[])

def pair_intro(+ec: R.Ctx, ac: Acc, ty: V.T, +q: Q.T, x: String, +dom: V.T, +clo: V.Closure<V.T>, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +point : T.T <- point_term(addr,"a pair takes a point address")
    fibre : T.T <- one_term(args,"a pair takes one fibre")
    keep1 : Bool <- R.point_runtime(ec,q,dom)
    pair : P.Pair<R.Ctx,V.T> <- R.under_point(ec,x,q,dom,clo)
    keep2 : Bool <- under_runtime(pair)
    point_v : V.T <- R.eval(ec,point)
    fibre_ty : V.T <- R.open(ec,clo,[point_v])
    +out : Outs <- erase_fold(List.append(&2,P.Pair<Maybe<&2,V.T>,T.T>,job(keep1,dom,point),job(keep2,fibre_ty,fibre)),ec,ac)
    tuple_finish(outs_values(out),outs_acc(out),ec,ty)

def leg_index(addr: T.Addr<T.T>, message: String) -> B.Comp(Big.T):
  match addr:
    case T.ALeg{index}: B.pure(Big.T,index)
    case _: B.failed(Big.T,Error.Mismatch{message})

def tag_intro(+ec: R.Ctx, ac: Acc, ty: V.T, count: Big.T, clo: V.Closure<V.T>, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +raw_index : Big.T <- leg_index(addr,"an injection takes a leg address")
    +tys : List<&2,V.T> <- R.leg_types(ec,count,clo)
    +lty : V.T <- K.optional(V.T,Big.at(V.T,tys,raw_index),Error.Mismatch{"the leg address is outside the collection"})
    index : Nat <- K.optional(Nat,Big.to_nat_bounded(raw_index,List.length(&2,V.T,tys)),Error.Mismatch{"the leg address is outside the collection"})
    keep : Bool <- R.runtime_ty(ec,lty)
    payload : T.T <- one_term(args,"an injection takes one payload")
    +out : Outs <- erase_fold(job(keep,lty,payload),ec,ac)
    tid : E.Tid <- R.tid_of(ec,ty)
    return Out{E.KTag{tid,index,outs_values(out)},outs_acc(out)}

def in_type(value: V.T, ec: R.Ctx, ac: Acc, ty: V.T, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  match value:
    case V.VLan{S.SPi{q,x,dom},clo,l}: pair_intro(ec,ac,ty,q,x,dom,clo,addr,args)
    case V.VLan{S.SZk{q,x,dom},clo,l}: pair_intro(ec,ac,ty,q,x,dom,clo,addr,args)
    case V.VLan{S.SColl{count},clo,l}: tag_intro(ec,ac,ty,count,clo,addr,args)
    case V.VLan{s,clo,l}: B.failed(Out,Error.Mismatch{"an introduction at a shape past M0"})
    case _: B.failed(Out,Error.Mismatch{"an introduction stands at a type that is not a left former"})

def in_typed(+ec: R.Ctx, ac: Acc, +ty: V.T, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    value : V.T <- R.whnf(ec,ty)
    in_type(value,ec,ac,ty,addr,args)

def position(names: List<&2,String>, +name: String, +index: Nat) -> Maybe<&2,Nat>:
  match names:
    case Nil{}: None{}
    case Con{x,rest}: Bool.pick(Maybe<&2,Nat>,String.eq(x,name),Some{index},position(rest,name,1n+index))

def constructor(addr: T.Addr<T.T>) -> B.Comp(String):
  match addr:
    case T.ACtor{name}: B.pure(String,name)
    case _: B.failed(String,Error.Mismatch{"a constructor takes a constructor address"})

def field_kept(field: Maybe<&2,E.Repr>) -> Bool:
  match field:
    case None{}: False{}
    case Some{r}: True{}

@unsafe def optional_field(keep: Bool, ec: R.Ctx, ac: Acc, ty: V.T, t: T.T) -> B.Comp(Outs):
  match keep:
    case False{}: B.pure(Outs,Outs{[],ac})
    case True{}:
      do B.Comp<Outs>:
        +out : Out <- term(ec,ac,False{},Some{ty},t)
        return Outs{[out_value(out)],out_acc(out)}

@unsafe def mu_fields(tele: Pos.Telescope, args: List<&2,T.T>, layout: List<&2,Maybe<&2,E.Repr>>, +ec: R.Ctx, ac: Acc, +env: List<&2,V.T>) -> B.Comp(Outs):
  match tele args layout:
    case Nil{} Nil{} Nil{}: B.pure(Outs,Outs{[],ac})
    case Con{Pos.Binder{q,x,ty},rest} Con{+arg,more} Con{field,fields}:
      do B.Comp<Outs>:
        tyv : V.T <- R.eval_env(ec,env,ty)
        +here : Outs <- optional_field(field_kept(field),ec,ac,tyv,arg)
        value : V.T <- R.eval(ec,arg)
        +tail : Outs <- mu_fields(rest,more,fields,ec,outs_acc(here),value <> env)
        return Outs{List.append(&2,E.Ktm,outs_values(here),outs_values(tail)),outs_acc(tail)}
    case _ _ _: B.failed(Outs,Error.Mismatch{"a branch binds a different number of fields than its constructor takes"})

def ctor_args(ctor: Pos.Ctor) -> Pos.Telescope:
  match ctor:
    case Pos.Ctor{name,args,idx,arity,rec}: args

def ctor_indices(ctor: Pos.Ctor) -> List<&2,T.T>:
  match ctor:
    case Pos.Ctor{name,args,idx,arity,rec}: idx

def mu_tag(+ec: R.Ctx, ac: Acc, +name: String, clo: V.Closure<V.T>, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +ctor : String <- constructor(addr)
    +family : Pos.Family <- B.lift(Pos.Family,Rules.mu_family(R.checker(ec),name))
    names : List<&2,String> <- R.ctor_names(family,name)
    index : Nat <- K.optional(Nat,position(names,ctor,0n),Error.Unbound{ctor ++ " is not a constructor of " ++ name})
    +ct : Pos.Ctor <- K.optional(Pos.Ctor,Pos.ctor_of(ctor,family),Error.Unbound{ctor ++ " is not a constructor of " ++ name})
    penv : List<&2,V.T> <- B.lift(List<&2,V.T>,Rules.mu_param_env(R.checker(ec),clo))
    layout : List<&2,Maybe<&2,E.Repr>> <- R.mu_layout(ec,family,ct)
    +out : Outs <- mu_fields(ctor_args(ct),args,layout,ec,ac,penv)
    group : List<&2,E.Tid> <- R.mu_group_tids(ec,name,family)
    return Out{E.KTag{R.mu_tid(name),index,outs_values(out)},add_groups(outs_acc(out),group)}

def mu_intro_type(value: V.T, ec: R.Ctx, ac: Acc, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  match value:
    case V.VLan{S.SMu{name,idx},clo,l}: mu_tag(ec,ac,name,clo,addr,args)
    case V.VLan{s,clo,l}: B.failed(Out,Error.Mismatch{"an introduction at a shape past M0"})
    case _: B.failed(Out,Error.Mismatch{"an introduction stands at a type that is not a left former"})

def mu_intro(+ec: R.Ctx, ac: Acc, ty: V.T, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    value : V.T <- R.whnf(ec,ty)
    mu_intro_type(value,ec,ac,addr,args)

def spine(t: T.T, args: List<&2,T.T>) -> P.Pair<T.T,List<&2,T.T>>:
  match t:
    case T.Out{S.SPi{q,x,dom},T.APt{qa,arg},head}: spine(head,arg <> args)
    case _: P.Pair{t,args}

law app_jobs:
  for args: List<&2,T.T>
  for +ec: R.Ctx
  for ty: V.T
  B.Comp(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>)

@unsafe def app_point(+ec: R.Ctx, q: Q.T, +dom: V.T, clo: V.Closure<V.T>, +arg: T.T, rest: List<&2,T.T>) -> B.Comp(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>):
  do B.Comp<List<&2,P.Pair<Maybe<&2,V.T>,T.T>>>:
    keep : Bool <- R.point_runtime(ec,q,dom)
    value : V.T <- R.eval(ec,arg)
    cod : V.T <- R.open(ec,clo,[value])
    more : List<&2,P.Pair<Maybe<&2,V.T>,T.T>> <- app_jobs(rest,ec,cod)
    return List.append(&2,P.Pair<Maybe<&2,V.T>,T.T>,job(keep,dom,arg),more)

@unsafe def app_type(value: V.T, ec: R.Ctx, arg: T.T, rest: List<&2,T.T>) -> B.Comp(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>):
  match value:
    case V.VRan{S.SPi{q,x,dom},clo,l}: app_point(ec,q,dom,clo,arg,rest)
    case V.VRan{S.SZk{q,x,dom},clo,l}: app_point(ec,q,dom,clo,arg,rest)
    case _: B.failed(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>,Error.Mismatch{"an application needs a point function type"})

@unsafe def app_jobs(args,ec,ty):
  match args:
    case Nil{}: B.pure(List<&2,P.Pair<Maybe<&2,V.T>,T.T>>,[])
    case Con{arg,rest}:
      do B.Comp<List<&2,P.Pair<Maybe<&2,V.T>,T.T>>>:
        value : V.T <- R.whnf(ec,ty)
        app_type(value,ec,arg,rest)

def app_bare(args: List<&2,E.Ktm>, ec: R.Ctx, ty: V.T) -> B.Comp(Bool):
  match args:
    case Nil{}: function_result(ec,ty)
    case Con{x,xs}: B.pure(Bool,False{})

def app_result(bare: Bool, tail: Bool, head: E.Ktm, args: List<&2,E.Ktm>, ac: Acc) -> Out:
  match bare tail:
    case True{} _: Out{head,ac}
    case False{} True{}: Out{E.KTail{head,args},ac}
    case False{} False{}: Out{E.KApp{head,args},ac}

@unsafe def app_spine(parts: P.Pair<T.T,List<&2,T.T>>, +ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T) -> B.Comp(Out):
  match parts:
    case P.Pair{head,Nil{}}: B.failed(Out,Error.Mismatch{"an application at a point shape with no point address"})
    case P.Pair{+head,Con{arg,args}}:
      do B.Comp<Out>:
        +hty : V.T <- R.infer(ec,head)
        jobs : List<&2,P.Pair<Maybe<&2,V.T>,T.T>> <- app_jobs(arg <> args,ec,hty)
        +head : Out <- term(ec,ac,False{},Some{hty},head)
        +args : Outs <- erase_fold(jobs,ec,out_acc(head))
        bare : Bool <- app_bare(outs_values(args),ec,ty)
        return app_result(bare,tail,out_value(head),outs_values(args),outs_acc(args))

def app_arm(ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, t: T.T) -> B.Comp(Out):
  app_spine(spine(t,[]),ec,ac,tail,ty)

def flags_prefix(flags: List<&2,Bool>, +limit: Big.T, +index: Nat) -> Nat:
  match flags:
    case Nil{}: 0n
    case Con{flag,rest}:
      Nat.add(Bool.pick(Nat,Bool.and(flag,Bool.not(Big.cmp_le(Big.compare(limit,Big.of_nat(index))))),1n,0n),flags_prefix(rest,limit,Succ{index}))

def flags_before(flags: List<&2,Bool>, count: Big.T) -> Nat:
  flags_prefix(flags,count,0n)

@unsafe def proj_type(value: V.T, +ec: R.Ctx, ac: Acc, +sty: V.T, n: Big.T, index: Big.T, head: T.T) -> B.Comp(Out):
  match value:
    case V.VRan{s,clo,l}:
      do B.Comp<Out>:
        flags : List<&2,Bool> <- R.leg_flags(ec,n,clo)
        +out : Out <- term(ec,ac,False{},Some{sty},head)
        tid : E.Tid <- R.tid_of(ec,sty)
        return Out{E.KProj{tid,flags_before(flags,index),out_value(out)},out_acc(out)}
    case _: B.failed(Out,Error.Mismatch{"a projection stands on a value that is not a right former"})

def proj_arm(+ec: R.Ctx, ac: Acc, n: Big.T, addr: T.Addr<T.T>, +head: T.T) -> B.Comp(Out):
  do B.Comp<Out>:
    index : Big.T <- leg_index(addr,"a projection takes a leg address")
    +sty : V.T <- R.infer(ec,head)
    value : V.T <- R.whnf(ec,sty)
    proj_type(value,ec,ac,sty,n,index,head)

@unsafe def let_runtime(keep: Bool, ec: R.Ctx, next: R.Ctx, ac: Acc, tail: Bool, ty: V.T, name: String, lty: V.T, value: T.T, body: T.T) -> B.Comp(Out):
  match keep:
    case False{}: term(next,ac,tail,Some{ty},body)
    case True{}:
      do B.Comp<Out>:
        +value : Out <- term(ec,ac,False{},Some{lty},value)
        +body : Out <- term(next,out_acc(value),tail,Some{ty},body)
        return Out{E.KLet{name,out_value(value),out_value(body)},out_acc(body)}

def let_arm(+ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, +name: String, lty: T.T, +value: T.T, body: T.T) -> B.Comp(Out):
  do B.Comp<Out>:
    +lty : V.T <- R.eval(ec,lty)
    +keep : Bool <- R.runtime_ty(ec,lty)
    vv : V.T <- R.eval(ec,value)
    next : R.Ctx = R.define(ec,name,Q.QMany{},lty,vv,R.keep_slot(keep))
    let_runtime(keep,ec,next,ac,tail,ty,name,lty,value,body)

def elim_shape(data: T.ElimData<T.T>) -> S.T<T.T>:
  match data:
    case T.ElimData{s,t,q,m,bs}: s

def pair_branch(branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>) -> B.Comp(T.Leg<T.T>):
  match branches:
    case [P.Pair{addr,leg}]: B.pure(T.Leg<T.T>,leg)
    case _: B.failed(T.Leg<T.T>,Error.Mismatch{"a pair elimination takes one branch"})

def second_binder(leg: T.Leg<T.T>) -> B.Comp(String):
  match leg:
    case T.Leg{[P.Pair{q,x},P.Pair{r,y}],body}: B.pure(String,y)
    case _: B.failed(String,Error.Mismatch{"a pair branch takes two binders"})

def leg_body(leg: T.Leg<T.T>) -> T.T:
  match leg:
    case T.Leg{bs,body}: body

def project_let(keep: Bool, name: String, tid: E.Tid, +index: Nat, body: E.Ktm) -> E.Ktm:
  match keep:
    case False{}: body
    case True{}: E.KLet{name,E.KProj{tid,index,E.KVar{index}},body}

@unsafe def pair_elim(+ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, +sty: V.T, +data: T.ElimData<T.T>, +q: Q.T, +name: String, +dom: V.T, clo: V.Closure<V.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +leg : T.Leg<T.T> <- pair_branch(K.elim_branches(data))
    +name2 : String <- second_binder(leg)
    +keep1 : Bool <- R.point_runtime(ec,q,dom)
    +cod : V.T <- R.open(ec,clo,[V.var(R.size(ec))])
    +ec1 : R.Ctx = R.bind(R.extra(ec),name,q,dom,R.keep_slot(keep1))
    +keep2 : Bool <- R.runtime_ty(ec1,cod)
    ec2 : R.Ctx = R.bind(ec1,name2,Q.QMany{},cod,R.keep_slot(keep2))
    +scrut : Out <- term(ec,ac,False{},Some{sty},K.elim_scrut(data))
    shape : S.T<V.T> <- B.lift(S.T<V.T>,Eval.eval_shape(elim_shape(data),R.globals(ec),R.env(ec)))
    self : V.T = V.VIn{shape,V.VAPt{q,V.var(R.size(ec))},[V.var(1n+R.size(ec))]}
    target : V.T <- K.elim_result(K.elim_motive(data),Some{ty},R.checker(ec),self)
    +body : Out <- term(ec2,out_acc(scrut),tail,Some{target},leg_body(leg))
    +tid : E.Tid <- R.tid_of(ec,sty)
    inner2 : E.Ktm = project_let(keep2,name2,tid,Bool.pick(Nat,keep1,1n,0n),out_value(body))
    inner1 : E.Ktm = project_let(keep1,name,tid,0n,inner2)
    return Out{E.KLet{"scrut",out_value(scrut),inner1},out_acc(body)}

type BranchOut is Data:
  BranchOut{branch: E.Branch<E.Ktm>, acc: Acc}

type Branches is Data:
  Branches{branches: List<&2,E.Branch<E.Ktm>>, acc: Acc}

def branch_value(out: BranchOut) -> E.Branch<E.Ktm>:
  match out:
    case BranchOut{branch,ac}: branch

def branch_acc(out: BranchOut) -> Acc:
  match out:
    case BranchOut{branch,ac}: ac

def branches_values(out: Branches) -> List<&2,E.Branch<E.Ktm>>:
  match out:
    case Branches{branches,ac}: branches

def branches_acc(out: Branches) -> Acc:
  match out:
    case Branches{branches,ac}: ac

def find_leg(branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>, +index: Nat) -> Maybe<&2,T.Leg<T.T>>:
  match branches:
    case Nil{}: None{}
    case Con{P.Pair{T.ALeg{k},leg},rest}: Bool.pick(Maybe<&2,T.Leg<T.T>>,Big.equal(k,Big.of_nat(index)),Some{leg},find_leg(rest,index))
    case Con{row,rest}: find_leg(rest,index)

@unsafe def case_leg(leg: T.Leg<T.T>, +ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, data: T.ElimData<T.T>, count: Big.T, +index: Nat, +lty: V.T) -> B.Comp(BranchOut):
  match leg:
    case T.Leg{[P.Pair{+q,name}],body}:
      do B.Comp<BranchOut>:
        +keep : Bool <- R.point_runtime(ec,q,lty)
        next : R.Ctx = R.bind(ec,name,q,lty,R.keep_slot(keep))
        self : V.T = V.VIn{S.SColl{count},V.valeg(V.T,index),[V.var(R.size(ec))]}
        target : V.T <- K.elim_result(K.elim_motive(data),Some{ty},R.checker(ec),self)
        +out : Out <- term(next,ac,tail,Some{target},body)
        return BranchOut{E.Branch{index,Bool.pick(Nat,keep,1n,0n),out_value(out)},out_acc(out)}
    case _: B.failed(BranchOut,Error.Mismatch{"a case branch takes one binder"})

def case_branches(types: List<&2,V.T>, +ec: R.Ctx, ac: Acc, +tail: Bool, +ty: V.T, +data: T.ElimData<T.T>, +count: Big.T, +index: Nat) -> B.Comp(Branches):
  match types:
    case Nil{}: B.pure(Branches,Branches{[],ac})
    case Con{lty,rest}:
      do B.Comp<Branches>:
        leg : T.Leg<T.T> <- K.optional(T.Leg<T.T>,find_leg(K.elim_branches(data),index),Error.Mismatch{"a case is missing a branch"})
        +out : BranchOut <- case_leg(leg,ec,ac,tail,ty,data,count,index,lty)
        +more : Branches <- case_branches(rest,ec,branch_acc(out),tail,ty,data,count,1n+index)
        return Branches{branch_value(out) <> branches_values(more),branches_acc(more)}

@unsafe def case_elim(+ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, +sty: V.T, +data: T.ElimData<T.T>, +count: Big.T, clo: V.Closure<V.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +scrut : Out <- term(ec,ac,False{},Some{sty},K.elim_scrut(data))
    tys : List<&2,V.T> <- R.leg_types(ec,count,clo)
    +branches : Branches <- case_branches(tys,ec,out_acc(scrut),tail,ty,data,count,0n)
    tid : E.Tid <- R.tid_of(ec,sty)
    return Out{E.KCase{tid,out_value(scrut),branches_values(branches)},branches_acc(branches)}

def elim_type(value: V.T, ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, sty: V.T, data: T.ElimData<T.T>) -> B.Comp(Out):
  match value:
    case V.VLan{S.SPi{q,x,dom},clo,l}: pair_elim(ec,ac,tail,ty,sty,data,q,x,dom,clo)
    case V.VLan{S.SZk{q,x,dom},clo,l}: pair_elim(ec,ac,tail,ty,sty,data,q,x,dom,clo)
    case V.VLan{S.SColl{n},clo,l}: case_elim(ec,ac,tail,ty,sty,data,n,clo)
    case V.VLan{s,clo,l}: B.failed(Out,Error.Mismatch{"an elimination at a shape past M0"})
    case _: B.failed(Out,Error.Mismatch{"an elimination stands on a value that is not a left former"})

def elim_typed(+ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, +data: T.ElimData<T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +sty : V.T <- R.infer(ec,K.elim_scrut(data))
    value : V.T <- R.whnf(ec,sty)
    elim_type(value,ec,ac,tail,ty,sty,data)

type MuBinders is Data:
  MuBinders{ctx: R.Ctx, env: List<&2,V.T>, values: List<&2,V.T>, arity: Nat}

def mu_binders(tele: Pos.Telescope, binders: List<&2,P.Pair<Q.T,String>>, layout: List<&2,Maybe<&2,E.Repr>>, +ec: R.Ctx, +env: List<&2,V.T>, values: List<&2,V.T>, arity: Nat) -> B.Comp(MuBinders):
  match tele binders layout:
    case Nil{} Nil{} Nil{}: B.pure(MuBinders,MuBinders{ec,env,values,arity})
    case Con{Pos.Binder{q,x,ty},ts} Con{P.Pair{bq,bx},bs} Con{field,fs}:
      do B.Comp<MuBinders>:
        tyv : V.T <- R.eval_env(ec,env,ty)
        +keep : Bool = field_kept(field)
        +value : V.T = V.var(R.size(ec))
        mu_binders(ts,bs,fs,R.bind(ec,bx,q,tyv,R.keep_slot(keep)),value <> env,value <> values,Nat.add(arity,Bool.pick(Nat,keep,1n,0n)))
    case _ _ _: B.failed(MuBinders,Error.Missing_branch{"a branch binds a different number of fields than its constructor takes"})

def find_ctor_branch(branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>, +name: String) -> Maybe<&2,T.Leg<T.T>>:
  match branches:
    case Nil{}: None{}
    case Con{P.Pair{T.ACtor{n},leg},rest}: Bool.pick(Maybe<&2,T.Leg<T.T>>,String.eq(n,name),Some{leg},find_ctor_branch(rest,name))
    case Con{row,rest}: find_ctor_branch(rest,name)

def leg_binders(leg: T.Leg<T.T>) -> List<&2,P.Pair<Q.T,String>>:
  match leg:
    case T.Leg{binders,body}: binders

def eval_terms(terms: List<&2,T.T>, +ec: R.Ctx, +env: List<&2,V.T>) -> B.Comp(List<&2,V.T>):
  match terms:
    case Nil{}: B.pure(List<&2,V.T>,[])
    case Con{t,rest}:
      do B.Comp<List<&2,V.T>>:
        value : V.T <- R.eval_env(ec,env,t)
        more : List<&2,V.T> <- eval_terms(rest,ec,env)
        return value <> more

@unsafe def mu_branch_body(bound: MuBinders, +ec: R.Ctx, ac: Acc, tail: Bool, name: String, motive: T.Motive<T.T>, index: Nat, ctor: String, indices: List<&2,T.T>, body: T.T) -> B.Comp(BranchOut):
  match bound:
    case MuBinders{next,env,values,arity}:
      do B.Comp<BranchOut>:
        +idx : List<&2,V.T> <- eval_terms(indices,ec,env)
        self : V.T = V.VIn{S.SMu{name,idx},V.VACtor{ctor},List.reverse(&2,V.T,values)}
        target : V.T <- K.mu_result(R.checker(ec),motive,idx,self)
        +out : Out <- term(next,ac,tail,Some{target},body)
        return BranchOut{E.Branch{index,arity,out_value(out)},out_acc(out)}

def mu_branch_of(+ec: R.Ctx, ac: Acc, tail: Bool, +name: String, +family: Pos.Family, motive: T.Motive<T.T>, penv: List<&2,V.T>, branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>, index: Nat, +ctor: String) -> B.Comp(BranchOut):
  do B.Comp<BranchOut>:
    +ct : Pos.Ctor <- K.optional(Pos.Ctor,Pos.ctor_of(ctor,family),Error.Unbound{ctor ++ " is not a constructor of " ++ name})
    +leg : T.Leg<T.T> <- K.optional(T.Leg<T.T>,find_ctor_branch(branches,ctor),Error.Missing_branch{"the elimination of " ++ name ++ " has no branch at " ++ ctor})
    layout : List<&2,Maybe<&2,E.Repr>> <- R.mu_layout(ec,family,ct)
    bound : MuBinders <- mu_binders(ctor_args(ct),leg_binders(leg),layout,ec,penv,[],0n)
    mu_branch_body(bound,ec,ac,tail,name,motive,index,ctor,ctor_indices(ct),leg_body(leg))

def mu_branches(names: List<&2,String>, +index: Nat, +ec: R.Ctx, ac: Acc, +tail: Bool, +name: String, +family: Pos.Family, +motive: T.Motive<T.T>, +penv: List<&2,V.T>, +branches: List<&2,P.Pair<T.Addr<T.T>,T.Leg<T.T>>>) -> B.Comp(Branches):
  match names:
    case Nil{}: B.pure(Branches,Branches{[],ac})
    case Con{ctor,rest}:
      do B.Comp<Branches>:
        +one : BranchOut <- mu_branch_of(ec,ac,tail,name,family,motive,penv,branches,index,ctor)
        +more : Branches <- mu_branches(rest,1n+index,ec,branch_acc(one),tail,name,family,motive,penv,branches)
        return Branches{branch_value(one) <> branches_values(more),branches_acc(more)}

@unsafe def mu_case(+ec: R.Ctx, ac: Acc, tail: Bool, sty: V.T, +data: T.ElimData<T.T>, +name: String, idx: List<&2,V.T>, clo: V.Closure<V.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +family : Pos.Family <- B.lift(Pos.Family,Rules.mu_family(R.checker(ec),name))
    motive : T.Motive<T.T> <- K.mu_motive_of(name,family,idx,data)
    names : List<&2,String> <- R.ctor_names(family,name)
    penv : List<&2,V.T> <- B.lift(List<&2,V.T>,Rules.mu_param_env(R.checker(ec),clo))
    +scrut : Out <- term(ec,ac,False{},Some{sty},K.elim_scrut(data))
    +brs : Branches <- mu_branches(names,0n,ec,out_acc(scrut),tail,name,family,motive,penv,K.elim_branches(data))
    group : List<&2,E.Tid> <- R.mu_group_tids(ec,name,family)
    return Out{E.KCase{R.mu_tid(name),out_value(scrut),branches_values(brs)},add_groups(branches_acc(brs),group)}

def mu_elim_type(value: V.T, ec: R.Ctx, ac: Acc, tail: Bool, sty: V.T, data: T.ElimData<T.T>) -> B.Comp(Out):
  match value:
    case V.VLan{S.SMu{name,idx},clo,l}: mu_case(ec,ac,tail,sty,data,name,idx,clo)
    case V.VLan{s,clo,l}: B.failed(Out,Error.Mismatch{"an elimination at a shape past M0"})
    case _: B.failed(Out,Error.Mismatch{"an elimination stands on a value that is not a left former"})

def mu_elim(+ec: R.Ctx, ac: Acc, tail: Bool, +data: T.ElimData<T.T>) -> B.Comp(Out):
  do B.Comp<Out>:
    +sty : V.T <- R.infer(ec,K.elim_scrut(data))
    value : V.T <- R.whnf(ec,sty)
    mu_elim_type(value,ec,ac,tail,sty,data)

def sec_arm(shape: S.T<T.T>, ec: R.Ctx, ac: Acc, ty: V.T, legs: List<&2,T.Leg<T.T>>, t: T.T) -> B.Comp(Out):
  match shape:
    case S.SPi{q,x,dom}: lift_arm(ec,ac,ty,t)
    case S.SColl{n}: tuple_arm(ec,ac,ty,n,legs)
    case S.SPar{a,b}: B.failed(Out,Error.Not_yet{"SPar arrives at M1"})
    case S.SMu{n,args}: B.failed(Out,Error.Not_yet{"a right former at a mu shape arrives at M2"})
    case S.SNu{n,args}: B.failed(Out,Error.Not_yet{"SNu arrives at M2"})
    case S.SZk{q,x,dom}: B.failed(Out,Error.Not_yet{"Ran SZk arrives at V5"})
    case S.SFhc{level}: fhc_sec(ec,ac,level,legs)
    case S.SMpc{p,a}: mpc_sec(ec,ac,legs)

def in_arm(shape: S.T<T.T>, ec: R.Ctx, ac: Acc, ty: V.T, addr: T.Addr<T.T>, args: List<&2,T.T>) -> B.Comp(Out):
  match shape:
    case S.SPi{q,x,dom}: in_typed(ec,ac,ty,addr,args)
    case S.SColl{n}: in_typed(ec,ac,ty,addr,args)
    case S.SPar{a,b}: B.failed(Out,Error.Not_yet{"SPar arrives at M1"})
    case S.SMu{n,ix}: mu_intro(ec,ac,ty,addr,args)
    case S.SNu{n,ix}: B.failed(Out,Error.Not_yet{"SNu arrives at M2"})
    case S.SZk{q,x,dom}: zk_in(ec,ac,addr,args)
    case S.SFhc{level}: B.failed(Out,Error.Not_yet{"Lan SFhc arrives at V5"})
    case S.SMpc{p,a}: B.failed(Out,Error.Not_yet{"Session arrives at V5"})

def out_arm(shape: S.T<T.T>, ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, addr: T.Addr<T.T>, head: T.T, t: T.T) -> B.Comp(Out):
  match shape:
    case S.SPi{q,x,dom}: app_arm(ec,ac,tail,ty,t)
    case S.SColl{n}: proj_arm(ec,ac,n,addr,head)
    case S.SPar{a,b}: B.failed(Out,Error.Not_yet{"SPar arrives at M1"})
    case S.SMu{n,ix}: B.failed(Out,Error.Not_yet{"a right former at a mu shape arrives at M2"})
    case S.SNu{n,ix}: B.failed(Out,Error.Not_yet{"SNu arrives at M2"})
    case S.SZk{q,x,dom}: B.failed(Out,Error.Not_yet{"Ran SZk arrives at V5"})
    case S.SFhc{level}: host_call(ec,ac,"fhcDec",[head])
    case S.SMpc{p,a}: host_call(ec,ac,"mpcOpen",[head])

def elim_arm(shape: S.T<T.T>, ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, data: T.ElimData<T.T>) -> B.Comp(Out):
  match shape:
    case S.SPi{q,x,dom}: elim_typed(ec,ac,tail,ty,data)
    case S.SColl{n}: elim_typed(ec,ac,tail,ty,data)
    case S.SPar{a,b}: B.failed(Out,Error.Not_yet{"SPar arrives at M1"})
    case S.SMu{n,ix}: mu_elim(ec,ac,tail,data)
    case S.SNu{n,ix}: B.failed(Out,Error.Not_yet{"SNu arrives at M2"})
    case S.SZk{q,x,dom}: elim_typed(ec,ac,tail,ty,data)
    case S.SFhc{level}: B.failed(Out,Error.Not_yet{"Lan SFhc arrives at V5"})
    case S.SMpc{p,a}: B.failed(Out,Error.Not_yet{"Session arrives at V5"})

@unsafe def node(t: T.T, ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T) -> B.Comp(Out):
  match t:
    case T.Var{ix}: var_arm(ec,ac,ix)
    case T.Univ{l}: B.pure(Out,Out{E.KErased{},ac})
    case T.Lan{s,d}: B.pure(Out,Out{E.KErased{},ac})
    case T.Ran{s,d}: B.pure(Out,Out{E.KErased{},ac})
    case T.Sec{+s,+legs}: sec_arm(s,ec,ac,ty,legs,T.Sec{s,legs})
    case T.In{s,a,args}: in_arm(s,ec,ac,ty,a,args)
    case T.Out{+s,+a,+head}: out_arm(s,ec,ac,tail,ty,a,head,T.Out{s,a,head})
    case T.Elim{+data}: elim_arm(elim_shape(data),ec,ac,tail,ty,data)
    case T.Let{x,lty,value,body}: let_arm(ec,ac,tail,ty,x,lty,value,body)
    case T.Ann{body,aty}: term(ec,ac,tail,Some{ty},body)
    case T.Global{name}: B.pure(Out,Out{E.KGlobal{name},ac})
    case T.Lit{literal}: B.pure(Out,Out{E.KLit{literal},ac})
    case T.Auto{}: B.failed(Out,Error.Not_yet{"instances arrive at M2"})

def term_runtime(keep: Bool, ec: R.Ctx, ac: Acc, tail: Bool, ty: V.T, t: T.T) -> B.Comp(Out):
  match keep:
    case False{}: B.pure(Out,Out{E.KErased{},ac})
    case True{}: node(t,ec,ac,tail,ty)

@unsafe def term(ec,ac,tail,expected,t):
  do B.Comp<Out>:
    +ty : V.T <- position_ty(ec,expected,t)
    keep : Bool <- R.runtime_ty(ec,ty)
    term_runtime(keep,ec,ac,tail,ty,t)

def tids_repr(repr: E.Repr) -> List<&2,E.Tid>:
  match repr:
    case E.RI31{}: []
    case E.RStruct{tid}: [tid]
    case E.RUnion{tid}: [tid]
    case E.RFunc{tid}: [tid]
    case E.RThunk{tid}: [tid]

def tids_reprs(reprs: List<&2,E.Repr>) -> List<&2,E.Tid>:
  match reprs:
    case Nil{}: []
    case Con{r,rest}: List.append(&2,E.Tid,tids_repr(r),tids_reprs(rest))

def node_tid(term: E.Ktm) -> List<&2,E.Tid>:
  match term:
    case E.KStruct{tid,args}: [tid]
    case E.KTag{tid,k,args}: [tid]
    case E.KProj{tid,k,value}: [tid]
    case E.KCase{tid,value,branches}: [tid]
    case _: []

@unsafe def tids_walk(work: List<&2,U.RuntimeWork>) -> List<&2,E.Tid>:
  match work:
    case Nil{}: []
    case Con{U.RuntimeWork{depth,+t},rest}: List.append(&2,E.Tid,node_tid(t),tids_walk(List.append(&2,U.RuntimeWork,U.runtime_children(t,0n),rest)))

def tids_ktm(t: E.Ktm) -> List<&2,E.Tid>:
  tids_walk([U.RuntimeWork{0n,t}])

def tids_decl(decl: E.Kdecl) -> List<&2,E.Tid>:
  match decl:
    case E.KRec{tids}: tids
    case E.KFun{f,ps,ret,body}: List.append(&2,E.Tid,tids_reprs(ps),List.append(&2,E.Tid,tids_repr(ret),tids_ktm(body)))

def tids_decls(decls: List<&2,E.Kdecl>) -> List<&2,E.Tid>:
  match decls:
    case Nil{}: []
    case Con{d,rest}: List.append(&2,E.Tid,tids_decl(d),tids_decls(rest))

def member_tid(tids: List<&2,E.Tid>, +tid: E.Tid) -> Bool:
  match tids:
    case Nil{}: False{}
    case Con{t,rest}: Bool.or(String.eq(E.tid_text(t),E.tid_text(tid)),member_tid(rest,tid))

def dedup_tids(tids: List<&2,E.Tid>, +seen: List<&2,E.Tid>) -> List<&2,E.Tid>:
  match tids:
    case Nil{}: []
    case Con{+tid,rest}: List.append(&2,E.Tid,Bool.pick(List<&2,E.Tid>,member_tid(seen,tid),[],[tid]),dedup_tids(rest,tid <> seen))

def def_finish(result: Chain, name: String) -> Entry:
  match result:
    case Chain{params,ret,body,Acc{next,lifted,groups}}:
      +ds = List.append(&2,E.Kdecl,lifted,[E.KFun{E.Fid{name},params,ret,body}])
      Code{E.KRec{dedup_tids(List.append(&2,E.Tid,tids_decls(ds),groups),[])} <> ds}

def def_code(ec: R.Ctx, name: String, ty: V.T, body: T.T) -> B.Comp(Entry):
  do B.Comp<Entry>:
    result : Chain <- chain(ec,Acc{0n,[],[]},[],ty,body)
    return def_finish(result,name)

def axiom_runtime(keep: Bool, ec: R.Ctx, ty: V.T) -> B.Comp(Entry):
  match keep:
    case False{}: B.pure(Entry,Dropped{})
    case True{}:
      do B.Comp<Entry>:
        repr : E.Repr <- R.repr_of(ec,ty)
        return Postulate{repr}

def def_runtime(keep: Bool, ec: R.Ctx, name: String, ty: V.T, body: T.T) -> B.Comp(Entry):
  match keep:
    case False{}: B.pure(Entry,Dropped{})
    case True{}: def_code(ec,name,ty,body)

def entry_kind(entry: G.Entry, +ec: R.Ctx, name: String) -> B.Comp(Entry):
  match entry:
    case G.Prim{ty,prim}: B.pure(Entry,Dropped{})
    case G.Axiom{ty}:
      do B.Comp<Entry>:
        +ty : V.T <- R.eval(ec,ty)
        keep : Bool <- R.runtime_ty(ec,ty)
        axiom_runtime(keep,ec,ty)
    case G.Def{G.DefEntry{ty,body,reducible,rec_arg,partial}}:
      do B.Comp<Entry>:
        +ty : V.T <- R.eval(ec,ty)
        keep : Bool <- R.runtime_ty(ec,ty)
        def_runtime(keep,ec,name,ty,body)

def entry_comp(globals: G.T, +name: String, entry: G.Entry) -> B.Comp(Entry):
  entry_kind(entry,R.Ctx{C.make(globals,B.unlimited(),0n),[],name},name)

def decl(globals: G.T, budget: B.T, name: String, entry: G.Entry) -> Result<&2,&2,Error.T,Entry>:
  B.run(Entry,entry_comp(globals,name,entry),budget)

def entry(globals: G.T, name: String, entry: G.Entry) -> Result<&2,&2,Error.T,Entry>:
  decl(globals,B.unlimited(),name,entry)

def program_comp(rows: List<&2,P.Pair<String,G.Entry>>, +globals: G.T) -> B.Comp(List<&2,P.Pair<String,Entry>>):
  match rows:
    case Nil{}: B.pure(List<&2,P.Pair<String,Entry>>,[])
    case Con{P.Pair{+name,+en},rest}:
      do B.Comp<List<&2,P.Pair<String,Entry>>>:
        erased : Entry <- entry_comp(globals,name,en)
        more : List<&2,P.Pair<String,Entry>> <- program_comp(rest,G.add(name,en,globals))
        return P.Pair{name,erased} <> more

def program_budget(globals: G.T, budget: B.T, rows: List<&2,P.Pair<String,G.Entry>>) -> Result<&2,&2,Error.T,List<&2,P.Pair<String,Entry>>>:
  B.run(List<&2,P.Pair<String,Entry>>,program_comp(rows,globals),budget)

def program(globals: G.T, rows: List<&2,P.Pair<String,G.Entry>>) -> Result<&2,&2,Error.T,List<&2,P.Pair<String,Entry>>>:
  program_budget(globals,B.unlimited(),rows)

def print_decls(decls: List<&2,E.Kdecl>) -> String:
  match decls:
    case Nil{}: ""
    case Con{decl,rest}: E.print_decl(decl) ++ "\n" ++ print_decls(rest)

def print_entry(name: String, entry: Entry) -> String:
  match entry:
    case Dropped{}: "erased " ++ name ++ "\n"
    case Postulate{repr}: "axiom " ++ name ++ " : " ++ E.print_repr(repr) ++ "\n"
    case Code{decls}: print_decls(decls)

def print(rows: List<&2,P.Pair<String,Entry>>) -> String:
  match rows:
    case Nil{}: ""
    case Con{P.Pair{name,entry},rest}: print_entry(name,entry) ++ print(rest)
