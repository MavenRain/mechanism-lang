import Base
import ./eterm.bend as E
import ./term.bend as T
import ./value.bend as V
import ./shape.bend as S
import ./quantity.bend as Q
import ./context.bend as C
import ./global.bend as G
import ./check_engine.bend as K
import ./eval.bend as Eval
import ./rules.bend as R
import ./positivity.bend as Pos
import ./level.bend as L
import ./budget.bend as B
import ./common.bend as P
import ./error.bend as Error
import ./bignum.bend as Big

type Slot is Data:
  SDrop{}
  SKeep{}
  SExtra{}

type Ctx is Data:
  Ctx{check: C.Ctx, slots: List<&2, Slot>, self: String}

def checker(ec: Ctx) -> C.Ctx:
  match ec:
    case Ctx{c, slots, self}: c

def slots(ec: Ctx) -> List<&2, Slot>:
  match ec:
    case Ctx{c, slots, self}: slots

def self(ec: Ctx) -> String:
  match ec:
    case Ctx{c, slots, self}: self

def globals(ec: Ctx) -> G.T:
  C.globals_of(checker(ec))

def size(ec: Ctx) -> Nat:
  C.size_of(checker(ec))

def env(ec: Ctx) -> List<&2, V.T>:
  C.env_of(checker(ec))

def bind(ec: Ctx, name: String, q: Q.T, ty: V.T, slot: Slot) -> Ctx:
  match ec:
    case Ctx{c, slots, self}: Ctx{C.bind(name,q,ty,c), slot <> slots, self}

def define(ec: Ctx, name: String, q: Q.T, ty: V.T, value: V.T, slot: Slot) -> Ctx:
  match ec:
    case Ctx{c, slots, self}: Ctx{C.define(name,q,ty,value,c), slot <> slots, self}

def extra(ec: Ctx) -> Ctx:
  match ec:
    case Ctx{c, slots, self}: Ctx{c, SExtra{} <> slots, self}

def replace_slots(ec: Ctx, slots: List<&2, Slot>) -> Ctx:
  match ec:
    case Ctx{c, old, self}: Ctx{c, slots, self}

def fresh(ec: Ctx) -> Ctx:
  match ec:
    case Ctx{c, slots, self}: Ctx{C.make(C.globals_of(c), B.unlimited(), 0n), [], self}

def keep_slot(keep: Bool) -> Slot:
  Bool.pick(Slot, keep, SKeep{}, SDrop{})

def eval(ec: Ctx, term: T.T) -> B.Comp(V.T):
  K.eval(checker(ec),term)

def eval_env(ec: Ctx, env: List<&2,V.T>, term: T.T) -> B.Comp(V.T):
  B.lift(V.T, Eval.eval(globals(ec),env,term))

def whnf(ec: Ctx, value: V.T) -> B.Comp(V.T):
  K.whnf(checker(ec),value)

def open(ec: Ctx, clo: V.Closure<V.T>, args: List<&2,V.T>) -> B.Comp(V.T):
  K.open(checker(ec),clo,args)

def infer(ec: Ctx, term: T.T) -> B.Comp(V.T):
  do B.Comp<V.T>:
    result : K.Inferred <- K.infer_uses(checker(ec),Q.QOne{},term)
    return K.inferred_ty(result)

def proof_free(+ec: Ctx, value: V.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    term : T.T <- K.quote(checker(ec),value)
    level : L.T <- K.infer_univ(checker(ec),term)
    return Bool.not(L.equal(level,L.zero()))

def runtime_collection(empty: Bool, ec: Ctx, value: V.T) -> B.Comp(Bool):
  match empty:
    case True{}: B.pure(Bool,False{})
    case False{}: proof_free(ec,value)

def runtime_node(ec: Ctx, value: V.T) -> B.Comp(Bool):
  match value:
    case V.VUniv{level}: B.pure(Bool,False{})
    case V.VLan{S.SColl{+n},d,l}: runtime_collection(Big.equal(n,Big.zero()),ec,V.VLan{S.SColl{n},d,l})
    case V.VRan{S.SColl{+n},d,l}: runtime_collection(Big.equal(n,Big.zero()),ec,V.VRan{S.SColl{n},d,l})
    case _: proof_free(ec,value)

def runtime_ty(+ec: Ctx, ty: V.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    value : V.T <- whnf(ec,ty)
    runtime_node(ec,value)

def point_runtime(ec: Ctx, q: Q.T, dom: V.T) -> B.Comp(Bool):
  match q:
    case Q.QZero{}: B.pure(Bool,False{})
    case _: runtime_ty(ec,dom)

def any_repr() -> E.Repr:
  E.RUnion{E.Tid{"any"}}

def mu_tid(name: String) -> E.Tid:
  E.Tid{"mu<" ++ name ++ ">"}

def repr_commas(rs: List<&2,E.Repr>) -> String:
  match rs:
    case Nil{}: ""
    case Con{r,rest}: "," ++ E.print_repr(r) ++ repr_commas(rest)

def mu_leg_tid(name: String, index: Nat, rs: List<&2,E.Repr>) -> E.Tid:
  E.Tid{"leg<" ++ E.tid_text(mu_tid(name)) ++ "," ++ Nat.show(index) ++ repr_commas(rs) ++ ">"}

def join(texts: List<&2,String>, +sep: String) -> String:
  match texts:
    case Nil{}: ""
    case Con{x,Nil{}}: x
    case Con{x,xs}: x ++ sep ++ join(xs,sep)

law repr_of:
  for +ec: Ctx
  for value: V.T
  B.Comp(E.Repr)

law arity_of:
  for +ec: Ctx
  for n: Nat
  for shape: S.T<V.T>
  for clo: V.Closure<V.T>
  B.Comp(Nat)

def under_point(+ec: Ctx, name: String, q: Q.T, dom: V.T, clo: V.Closure<V.T>) -> B.Comp(P.Pair<Ctx,V.T>):
  do B.Comp<P.Pair<Ctx,V.T>>:
    cod : V.T <- open(ec,clo,[V.var(size(ec))])
    return P.Pair{bind(ec,name,q,dom,SDrop{}),cod}

@unsafe def repr_text(ec: Ctx, keep: Bool, ty: V.T) -> B.Comp(List<&2,String>):
  match keep:
    case False{}: B.pure(List<&2,String>,[])
    case True{}:
      do B.Comp<List<&2,String>>:
        r : E.Repr <- repr_of(ec,ty)
        return [E.print_repr(r)]

@unsafe def arity_rest(ec: Ctx, n: Nat, value: V.T) -> B.Comp(Nat):
  match value:
    case V.VRan{s,d,l}: arity_of(ec,n,s,d)
    case _: B.pure(Nat,n)

@unsafe def arity_open(pair: P.Pair<Ctx,V.T>, n: Nat, keep: Bool) -> B.Comp(Nat):
  match pair:
    case P.Pair{+ec,cod}:
      do B.Comp<Nat>:
        value : V.T <- whnf(ec,cod)
        arity_rest(ec,Nat.add(n,Bool.pick(Nat,keep,1n,0n)),value)

@unsafe def arity_point(+ec: Ctx, n: Nat, +q: Q.T, name: String, +dom: V.T, clo: V.Closure<V.T>) -> B.Comp(Nat):
  do B.Comp<Nat>:
    keep : Bool <- point_runtime(ec,q,dom)
    pair : P.Pair<Ctx,V.T> <- under_point(ec,name,q,dom,clo)
    arity_open(pair,n,keep)

@unsafe def arity_of(ec,n,shape,clo):
  match shape:
    case S.SPi{q,x,dom}: arity_point(ec,n,q,x,dom,clo)
    case S.SZk{q,x,dom}: arity_point(ec,n,q,x,dom,clo)
    case _: B.pure(Nat,n)

def pair_second(pair: P.Pair<Ctx,V.T>, first: List<&2,String>) -> B.Comp(List<&2,String>):
  match pair:
    case P.Pair{+ec,+cod}:
      do B.Comp<List<&2,String>>:
        keep : Bool <- runtime_ty(ec,cod)
        second : List<&2,String> <- repr_text(ec,keep,cod)
        return List.append(&2,String,first,second)

def pair_texts(+ec: Ctx, +q: Q.T, name: String, +dom: V.T, clo: V.Closure<V.T>) -> B.Comp(List<&2,String>):
  do B.Comp<List<&2,String>>:
    keep : Bool <- point_runtime(ec,q,dom)
    first : List<&2,String> <- repr_text(ec,keep,dom)
    pair : P.Pair<Ctx,V.T> <- under_point(ec,name,q,dom,clo)
    pair_second(pair,first)

def leg_types_loop(n: Nat, +index: Nat, +ec: Ctx, +legs: List<&2,V.VLeg<V.T>>) -> B.Comp(List<&2,V.T>):
  match n:
    case 0n: B.pure(List<&2,V.T>,[])
    case 1n+rest:
      do B.Comp<List<&2,V.T>>:
        ty : V.T <- B.lift(V.T,R.coll_leg_ty(checker(ec),legs,Big.of_nat(index)))
        more : List<&2,V.T> <- leg_types_loop(rest,1n+index,ec,legs)
        return ty <> more

def bounded_leg_types(count: Maybe<&2,Nat>, ec: Ctx, +legs: List<&2,V.VLeg<V.T>>) -> B.Comp(List<&2,V.T>):
  match count:
    case Some{n}: leg_types_loop(n,0n,ec,legs)
    # Visit at most the actual list plus its first missing leg, preserving Wrong_leg.
    case None{}: leg_types_loop(Succ{List.length(&2,V.VLeg<V.T>,legs)},0n,ec,legs)

def leg_types(+ec: Ctx, count: Big.T, clo: V.Closure<V.T>) -> B.Comp(List<&2,V.T>):
  do B.Comp<List<&2,V.T>>:
    +legs : List<&2,V.VLeg<V.T>> <- B.lift(List<&2,V.VLeg<V.T>>,R.coll_legs_of(checker(ec),clo))
    bounded_leg_types(Big.to_nat_bounded(count,List.length(&2,V.VLeg<V.T>,legs)),ec,legs)

def flags(types: List<&2,V.T>, +ec: Ctx) -> B.Comp(List<&2,Bool>):
  match types:
    case Nil{}: B.pure(List<&2,Bool>,[])
    case Con{ty,rest}:
      do B.Comp<List<&2,Bool>>:
        keep : Bool <- runtime_ty(ec,ty)
        more : List<&2,Bool> <- flags(rest,ec)
        return keep <> more

def leg_flags(+ec: Ctx, count: Big.T, clo: V.Closure<V.T>) -> B.Comp(List<&2,Bool>):
  do B.Comp<List<&2,Bool>>:
    tys : List<&2,V.T> <- leg_types(ec,count,clo)
    flags(tys,ec)

@unsafe def sum_text(ec: Ctx, ty: V.T, keep: Bool) -> B.Comp(String):
  match keep:
    case False{}: B.pure(String,"unit")
    case True{}:
      do B.Comp<String>:
        r : E.Repr <- repr_of(ec,ty)
        return E.print_repr(r)

def sum_texts(types: List<&2,V.T>, +ec: Ctx) -> B.Comp(List<&2,String>):
  match types:
    case Nil{}: B.pure(List<&2,String>,[])
    case Con{+ty,rest}:
      do B.Comp<List<&2,String>>:
        keep : Bool <- runtime_ty(ec,ty)
        here : String <- sum_text(ec,ty,keep)
        more : List<&2,String> <- sum_texts(rest,ec)
        return here <> more

def tuple_texts(types: List<&2,V.T>, +ec: Ctx) -> B.Comp(List<&2,String>):
  match types:
    case Nil{}: B.pure(List<&2,String>,[])
    case Con{+ty,rest}:
      do B.Comp<List<&2,String>>:
        keep : Bool <- runtime_ty(ec,ty)
        here : List<&2,String> <- repr_text(ec,keep,ty)
        more : List<&2,String> <- tuple_texts(rest,ec)
        return List.append(&2,String,here,more)

def ran_collection(empty: Bool, +ec: Ctx, count: Big.T, clo: V.Closure<V.T>) -> B.Comp(E.Repr):
  match empty:
    case True{}: B.pure(E.Repr,any_repr())
    case False{}:
      do B.Comp<E.Repr>:
        types : List<&2,V.T> <- leg_types(ec,count,clo)
        texts : List<&2,String> <- tuple_texts(types,ec)
        return E.RStruct{E.Tid{"tuple<" ++ join(texts,",") ++ ">"}}

def lan_collection(empty: Bool, +ec: Ctx, count: Big.T, clo: V.Closure<V.T>) -> B.Comp(E.Repr):
  match empty:
    case True{}: B.pure(E.Repr,any_repr())
    case False{}:
      do B.Comp<E.Repr>:
        types : List<&2,V.T> <- leg_types(ec,count,clo)
        texts : List<&2,String> <- sum_texts(types,ec)
        return E.RUnion{E.Tid{"sum<" ++ join(texts,"|") ++ ">"}}

@unsafe def ran_repr(+ec: Ctx, shape: S.T<V.T>, clo: V.Closure<V.T>) -> B.Comp(E.Repr):
  match shape:
    case S.SPi{q,x,dom}:
      do B.Comp<E.Repr>:
        count : Nat <- arity_of(ec,0n,S.SPi{q,x,dom},clo)
        return E.RFunc{E.Tid{"fn<" ++ Nat.show(count) ++ ">"}}
    case S.SZk{q,x,dom}:
      do B.Comp<E.Repr>:
        count : Nat <- arity_of(ec,0n,S.SZk{q,x,dom},clo)
        return E.RFunc{E.Tid{"fn<" ++ Nat.show(count) ++ ">"}}
    case S.SColl{+n}: ran_collection(Big.equal(n,Big.zero()),ec,n,clo)
    case _: B.pure(E.Repr,any_repr())

def lan_repr(+ec: Ctx, shape: S.T<V.T>, clo: V.Closure<V.T>) -> B.Comp(E.Repr):
  match shape:
    case S.SMu{name,args}: B.pure(E.Repr,E.RUnion{mu_tid(name)})
    case S.SPi{q,x,dom}:
      do B.Comp<E.Repr>:
        texts : List<&2,String> <- pair_texts(ec,q,x,dom,clo)
        return E.RStruct{E.Tid{"pair<" ++ join(texts,",") ++ ">"}}
    case S.SColl{+n}: lan_collection(Big.equal(n,Big.zero()),ec,n,clo)
    case _: B.pure(E.Repr,any_repr())

def repr_node(ec: Ctx, value: V.T) -> B.Comp(E.Repr):
  match value:
    case V.VNeutral{V.HGlobal{name},Nil{}}:
      B.pure(E.Repr,Bool.pick(E.Repr,String.eq(name,"Nat"),E.RUnion{E.Tid{"nat"}},any_repr()))
    case V.VRan{s,d,l}: ran_repr(ec,s,d)
    case V.VLan{s,d,l}: lan_repr(ec,s,d)
    case _: B.pure(E.Repr,any_repr())

@unsafe def repr_of(ec,value):
  do B.Comp<E.Repr>:
    value : V.T <- whnf(ec,value)
    repr_node(ec,value)

def repr_tid(repr: E.Repr) -> B.Comp(E.Tid):
  match repr:
    case E.RI31{}: B.failed(E.Tid,Error.Mismatch{"a structured value stands at a type whose repr is i31"})
    case E.RStruct{tid}: B.pure(E.Tid,tid)
    case E.RUnion{tid}: B.pure(E.Tid,tid)
    case E.RFunc{tid}: B.pure(E.Tid,tid)
    case E.RThunk{tid}: B.pure(E.Tid,tid)

def tid_of(ec: Ctx, value: V.T) -> B.Comp(E.Tid):
  do B.Comp<E.Tid>:
    repr : E.Repr <- repr_of(ec,value)
    repr_tid(repr)

def optional_repr(ec: Ctx, keep: Bool, ty: V.T) -> B.Comp(Maybe<&2,E.Repr>):
  match keep:
    case False{}: B.pure(Maybe<&2,E.Repr>,None{})
    case True{}:
      do B.Comp<Maybe<&2,E.Repr>>:
        r : E.Repr <- repr_of(ec,ty)
        return Some{r}

def mu_leg_reprs(tele: Pos.Telescope, +ec: Ctx, +env: List<&2,V.T>) -> B.Comp(List<&2,Maybe<&2,E.Repr>>):
  match tele:
    case Nil{}: B.pure(List<&2,Maybe<&2,E.Repr>>,[])
    case Con{Pos.Binder{+q,x,ty},rest}:
      do B.Comp<List<&2,Maybe<&2,E.Repr>>>:
        +tyv : V.T <- eval_env(ec,env,ty)
        keep : Bool <- point_runtime(ec,q,tyv)
        here : Maybe<&2,E.Repr> <- optional_repr(ec,keep,tyv)
        more : List<&2,Maybe<&2,E.Repr>> <- mu_leg_reprs(rest,bind(ec,x,q,tyv,SDrop{}),V.var(size(ec)) <> env)
        return here <> more

def mu_parameters(tele: Pos.Telescope, +ec: Ctx) -> B.Comp(Ctx):
  match tele:
    case Nil{}: B.pure(Ctx,ec)
    case Con{Pos.Binder{q,x,ty},rest}:
      do B.Comp<Ctx>:
        tyv : V.T <- eval(ec,ty)
        mu_parameters(rest,bind(ec,x,q,tyv,SDrop{}))

def mu_layout(ec: Ctx, family: Pos.Family, ctor: Pos.Ctor) -> B.Comp(List<&2,Maybe<&2,E.Repr>>):
  match family ctor:
    case Pos.Family{name,params,idx,l,s,cs,p} Pos.Ctor{n,args,idx2,arity,rec}:
      do B.Comp<List<&2,Maybe<&2,E.Repr>>>:
        +params : Ctx <- mu_parameters(params,fresh(ec))
        mu_leg_reprs(args,params,env(params))

def kept_reprs(rs: List<&2,Maybe<&2,E.Repr>>) -> List<&2,E.Repr>:
  match rs:
    case Nil{}: []
    case Con{None{},rest}: kept_reprs(rest)
    case Con{Some{r},rest}: r <> kept_reprs(rest)

def ctor_names(family: Pos.Family, name: String) -> B.Comp(List<&2,String>):
  match family:
    case Pos.Family{n,p,i,l,Pos.Complete{names},cs,pos}: B.pure(List<&2,String>,names)
    case Pos.Family{n,p,i,l,Pos.Provisional{},cs,pos}: B.failed(List<&2,String>,Error.Unbound{"the family " ++ name ++ " is still under declaration"})
    case Pos.Family{n,p,i,l,Pos.Builtin{},cs,pos}: B.failed(List<&2,String>,Error.Mismatch{"the family " ++ name ++ " is a kernel family and has no branches"})

def group_tids(names: List<&2,String>, +index: Nat, +ec: Ctx, +name: String, +family: Pos.Family) -> B.Comp(List<&2,E.Tid>):
  match names:
    case Nil{}: B.pure(List<&2,E.Tid>,[])
    case Con{+ctor,rest}:
      do B.Comp<List<&2,E.Tid>>:
        ct : Pos.Ctor <- K.optional(Pos.Ctor,Pos.ctor_of(ctor,family),Error.Unbound{ctor ++ " is not a constructor of " ++ name})
        rs : List<&2,Maybe<&2,E.Repr>> <- mu_layout(ec,family,ct)
        more : List<&2,E.Tid> <- group_tids(rest,1n+index,ec,name,family)
        return mu_leg_tid(name,index,kept_reprs(rs)) <> more

def mu_group_tids(+ec: Ctx, +name: String, +family: Pos.Family) -> B.Comp(List<&2,E.Tid>):
  do B.Comp<List<&2,E.Tid>>:
    names : List<&2,String> <- ctor_names(family,name)
    group_tids(names,0n,ec,name,family)
