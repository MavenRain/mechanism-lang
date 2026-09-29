import Base
import ./gc_encode.bend as G
import ./link.bend as L
import ./nat_runtime.bend as N
import ./state.bend as S
import ../kernel/common.bend as C
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as Erase
import ../kernel/error.bend as Err
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as Big

type Ctx is Data:
  Ctx{layout:L.T,result:E.Repr}

type Binding is Data:
  Binding{index:U32,repr:E.Repr}

def layout(c:Ctx) -> L.T:
  match c:
    case Ctx{l,r}: l

def prog(l:L.T) -> L.Prog:
  match l:
    case L.Linked{p,ts,gs,tm,fs,fm,a,k,d}: p

def cat(a:List<&2,G.Instr>,b:List<&2,G.Instr>) -> List<&2,G.Instr>:
  List.append(&2,G.Instr,a,b)

def cats(xs:List<&2,List<&2,G.Instr>>) -> List<&2,G.Instr>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: cat(x,cats(rest))

def same(a:E.Repr,b:E.Repr) -> Bool:
  String.eq(E.print_repr(a),E.print_repr(b))

def coerce_diff(+l:L.T,src:E.Repr,dst:E.Repr) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match src dst:
    case E.RStruct{E.Tid{a}} E.RStruct{E.Tid{b}}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        af : List<&2,E.Repr> <- L.fields_of(a)
        bf : List<&2,E.Repr> <- L.fields_of(b)
        Bool.pick(Result<&2,&2,Err.T,List<&2,G.Instr>>,Nat.is_eq(List.length(&2,E.Repr,af),List.length(&2,E.Repr,bf)),Done{[]},Fail{Err.Mismatch{"aggregate representations have different widths"}})
    case _ E.RUnion{t}: Done{[]}
    case _ E.RI31{}: Done{[G.Ref_cast{G.HI31{}}]}
    case _ E.RStruct{E.Tid{t}}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        i : U32 <- L.type_index(l,t)
        return [G.Ref_cast{G.HType{i}}]
    case _ E.RFunc{t}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        i : U32 <- L.type_index(l,"clos")
        return [G.Ref_cast{G.HType{i}}]
    case _ E.RThunk{t}: Fail{Err.Not_yet{"RThunk arrives at M2"}}

def coerce(l:L.T,+src:E.Repr,+dst:E.Repr) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  Bool.pick(Result<&2,&2,Err.T,List<&2,G.Instr>>,same(src,dst),Done{[]},coerce_diff(l,src,dst))

def terminal(rev:List<&2,G.Instr>) -> Bool:
  match rev:
    case Con{G.Unreachable{},rest}: True{}
    case _: False{}

def with_coercion(+body:List<&2,G.Instr>,cast:List<&2,G.Instr>) -> List<&2,G.Instr>:
  Bool.pick(List<&2,G.Instr>,terminal(List.reverse(&2,G.Instr,body)),body,cat(body,cast))

def empty_cast(r:Result<&2,&2,Err.T,List<&2,G.Instr>>) -> Bool:
  match r:
    case Done{Nil{}}: True{}
    case _: False{}

def tail_ok(c:Ctx,tail:Bool,r:E.Repr) -> Bool:
  match c:
    case Ctx{l,res}: tail && empty_cast(coerce(l,r,res))

def reprs(env:List<&2,Binding>) -> List<&2,E.Repr>:
  match env:
    case Nil{}: []
    case Con{Binding{i,r},rest}: r <> reprs(rest)

def rep(c:Ctx,env:List<&2,Binding>,tm:E.Ktm) -> Result<&2,&2,Err.T,E.Repr>:
  L.infer(prog(layout(c)),reprs(env),tm)

def stores(xs:List<&2,U32>,+arr:U32,+local:U32,+i:U32) -> List<&2,G.Instr>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: cat([G.Local_get{local},G.I32_const{i},G.I32_const{x},G.Array_set{arr}],stores(rest,arr,local,U32.add(i,1)))

def literal_big(+l:L.T,digits:Maybe<&2,List<&2,U32>>) -> S.Comp(List<&2,G.Instr>):
  match digits:
    case None{}: S.fail(List<&2,G.Instr>,Err.Mismatch{"a Nat literal is negative"})
    case Some{+ds}:
      do S.Comp<List<&2,G.Instr>>:
        +ai : U32 <- S.lift(U32,L.type_index(l,"nat-limbs"))
        bi : U32 <- S.lift(U32,L.type_index(l,"nat-big"))
        +idx : U32 <- S.alloc(G.Ref{G.HType{ai}})
        return cats([[G.I32_const{0},G.I32_const{U32.from_nat(List.length(&2,U32,ds))},G.Array_new{ai},G.Local_set{idx}],stores(ds,ai,idx,0),[G.I32_const{1},G.Local_get{idx},G.Struct_new{bi}]])

def literal_small(l:L.T,n:Big.T,small:Maybe<&2,U32>) -> S.Comp(List<&2,G.Instr>):
  match small:
    case Some{x}: S.pure(List<&2,G.Instr>,[G.I32_const{x},G.Ref_i31{}])
    case None{}: literal_big(l,Big.limbs15(n))

def literal(l:L.T,lit:Lit.T) -> S.Comp(List<&2,G.Instr>):
  match lit:
    case Lit.LInt{+n}: literal_small(l,n,Big.to_i31(n))
    case Lit.LString{s}: S.fail(List<&2,G.Instr>,Err.Not_yet{"a string literal has no wasm form at M0"})

def closure_value(+c:Ctx,n:String,m:Nat) -> S.Comp(List<&2,G.Instr>):
  do S.Comp<List<&2,G.Instr>>:
    wi : U32 <- S.lift(U32,L.func_index(layout(c),L.wrap_fkey(n,0n)))
    ci : U32 <- S.lift(U32,L.type_index(layout(c),"clos"))
    return [G.I32_const{U32.from_nat(m)},G.Ref_func{wi},G.I32_const{0},G.Ref_i31{},G.Struct_new{ci}]

def optional(-A:Data,m:Maybe<&2,A>,e:Err.T) -> Result<&2,&2,Err.T,A>:
  match m:
    case None{}: Fail{e}
    case Some{a}: Done{a}

def nth(-A:Data,xs:List<&2,A>,i:Nat) -> Maybe<&2,A>:
  match xs i:
    case Nil{} _: None{}
    case Con{x,rest} 0n: Some{x}
    case Con{x,rest} 1n+i: nth(A,rest,i)

def var(b:Maybe<&2,Binding>,i:Nat) -> S.Comp(List<&2,G.Instr>):
  match b:
    case Some{Binding{idx,r}}: S.pure(List<&2,G.Instr>,[G.Local_get{idx}])
    case None{}: S.fail(List<&2,G.Instr>,Err.Unbound{"KVar " ++ Nat.show(i) ++ " is out of scope"})

law go:
  for +c:Ctx
  for +env:List<&2,Binding>
  for +tail:Bool
  for +tm:E.Ktm
  S.Comp(List<&2,G.Instr>)
law arg_at:
  for +c:Ctx
  for +env:List<&2,Binding>
  for +tm:E.Ktm
  for +r:E.Repr
  S.Comp(List<&2,G.Instr>)
law apply:
  for +c:Ctx
  for +env:List<&2,Binding>
  for +tail:Bool
  for +h:E.Ktm
  for +args:List<&2,E.Ktm>
  S.Comp(List<&2,G.Instr>)
law steps:
  for +c:Ctx
  for +env:List<&2,Binding>
  for +tail:Bool
  for +head:List<&2,G.Instr>
  for +r:E.Repr
  for +args:List<&2,E.Ktm>
  S.Comp(List<&2,G.Instr>)
law case_term:
  for +c:Ctx
  for +env:List<&2,Binding>
  for +tail:Bool
  for +tid:E.Tid
  for +sc:E.Ktm
  for +code:List<&2,G.Instr>
  for +bs:List<&2,E.Branch<E.Ktm>>
  S.Comp(List<&2,G.Instr>)

@unsafe def each(+c:Ctx,+env:List<&2,Binding>,xs:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match xs:
    case Nil{}: S.pure(List<&2,G.Instr>,[])
    case Con{x,rest}:
      do S.Comp<List<&2,G.Instr>>:
        a : List<&2,G.Instr> <- go(c,env,False{},x)
        b : List<&2,G.Instr> <- each(c,env,rest)
        return cat(a,b)

@unsafe def each2(+c:Ctx,+env:List<&2,Binding>,xs:List<&2,E.Ktm>,rs:List<&2,E.Repr>) -> S.Comp(List<&2,G.Instr>):
  match xs rs:
    case Nil{} Nil{}: S.pure(List<&2,G.Instr>,[])
    case Con{x,xt} Con{r,rt}:
      do S.Comp<List<&2,G.Instr>>:
        a : List<&2,G.Instr> <- arg_at(c,env,x,r)
        b : List<&2,G.Instr> <- each2(c,env,xt,rt)
        return cat(a,b)
    case Nil{} Con{r,rt}: S.fail(List<&2,G.Instr>,Err.Mismatch{"an argument list is shorter than its signature"})
    case Con{x,xt} Nil{}: S.fail(List<&2,G.Instr>,Err.Mismatch{"an argument list is longer than its signature"})

def global_fun(m:Nat,+c:Ctx,name:String,tail:Bool,r:E.Repr) -> S.Comp(List<&2,G.Instr>):
  match m:
    case 0n:
      do S.Comp<List<&2,G.Instr>>:
        +fi : U32 <- S.lift(U32,L.func_index(layout(c),"fun:" ++ name))
        return [Bool.pick(G.Instr,tail_ok(c,tail,r),G.Return_call{fi},G.Call{fi})]
    case 1n+p: closure_value(c,name,1n+p)

def global_kind(k:L.HeadKind,+c:Ctx,+name:String,tail:Bool) -> S.Comp(List<&2,G.Instr>):
  match k:
    case L.HFun{n,L.Fn{ps,r,b}}:
      global_fun(List.length(&2,E.Repr,ps),c,name,tail,r)
    case L.HPrim{n}: closure_value(c,name,2n)
    case L.HValue{}:
      do S.Comp<List<&2,G.Instr>>:
        r : E.Repr <- S.lift(E.Repr,L.global_repr(prog(layout(c)),name))
        S.fail(List<&2,G.Instr>,Err.Unbound{"no runtime value for " ++ name})

def global(+c:Ctx,+name:String,tail:Bool) -> S.Comp(List<&2,G.Instr>):
  global_kind(L.head_kind(prog(layout(c)),E.KGlobal{name}),c,name,tail)

@unsafe def direct_last(+c:Ctx,env:List<&2,Binding>,tail:Bool,r:E.Repr,ia:List<&2,G.Instr>,+fi:U32,rest:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match rest:
    case Nil{}: S.pure(List<&2,G.Instr>,cat(ia,[Bool.pick(G.Instr,tail_ok(c,tail,r),G.Return_call{fi},G.Call{fi})]))
    case Con{x,xs}: steps(c,env,tail,cat(ia,[G.Call{fi}]),r,x <> xs)

@unsafe def direct_short(short:Bool,+c:Ctx,+env:List<&2,Binding>,+tail:Bool,+name:String,+params:List<&2,E.Repr>,result:E.Repr,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match short:
    case True{}:
      +m = List.length(&2,E.Repr,params)
      do S.Comp<List<&2,G.Instr>>:
        ih : List<&2,G.Instr> <- closure_value(c,name,m)
        steps(c,env,tail,ih,E.RFunc{E.Tid{L.fn_key(m)}},args)
    case False{}:
      +m = List.length(&2,E.Repr,params)
      do S.Comp<List<&2,G.Instr>>:
        ia : List<&2,G.Instr> <- each2(c,env,List.take(&2,E.Ktm,args,m),params)
        fi : U32 <- S.lift(U32,L.func_index(layout(c),"fun:" ++ name))
        direct_last(c,env,tail,result,ia,fi,List.drop(&2,E.Ktm,args,m))

@unsafe def direct(c:Ctx,env:List<&2,Binding>,tail:Bool,name:String,+params:List<&2,E.Repr>,result:E.Repr,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  direct_short(Nat.is_lt(List.length(&2,E.Ktm,args),List.length(&2,E.Repr,params)),c,env,tail,name,params,result,args)

@unsafe def primitive_short(short:Bool,+c:Ctx,+env:List<&2,Binding>,tail:Bool,+name:String,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match short:
    case True{}:
      do S.Comp<List<&2,G.Instr>>:
        ih : List<&2,G.Instr> <- closure_value(c,name,2n)
        steps(c,env,tail,ih,E.RFunc{E.Tid{L.fn_key(2n)}},args)
    case False{}:
      do S.Comp<List<&2,G.Instr>>:
        ia : List<&2,G.Instr> <- each(c,env,List.take(&2,E.Ktm,args,2n))
        fi : U32 <- S.lift(U32,L.func_index(layout(c),"nat-runtime:" ++ name))
        steps(c,env,tail,cat(ia,[G.Call{fi}]),L.prim_result(name),List.drop(&2,E.Ktm,args,2n))

@unsafe def apply_kind(k:L.HeadKind,+c:Ctx,+env:List<&2,Binding>,+tail:Bool,+head:E.Ktm,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match k args:
    case L.HFun{n,f} Nil{}: go(c,env,tail,head)
    case L.HFun{n,L.Fn{ps,r,b}} Con{x,xs}: direct(c,env,tail,n,ps,r,args)
    case L.HPrim{n} Nil{}: go(c,env,tail,head)
    case L.HPrim{n} Con{x,xs}: primitive_short(Nat.is_lt(List.length(&2,E.Ktm,args),2n),c,env,tail,n,args)
    case L.HValue{} _:
      do S.Comp<List<&2,G.Instr>>:
        ih : List<&2,G.Instr> <- go(c,env,False{},head)
        hr : E.Repr <- S.lift(E.Repr,rep(c,env,head))
        steps(c,env,tail,ih,hr,args)

@unsafe def apply(c,env,tail,h,args):
  apply_kind(L.head_kind(prog(layout(c)),h),c,env,tail,h,args)

@unsafe def arg_at(c,env,tm,r):
  do S.Comp<List<&2,G.Instr>>:
    code : List<&2,G.Instr> <- go(c,env,False{},tm)
    tr : E.Repr <- S.lift(E.Repr,rep(c,env,tm))
    cz : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),tr,r))
    return cat(code,cz)

@unsafe def steps_apply(+c:Ctx,+env:List<&2,Binding>,tail:Bool,head:List<&2,G.Instr>,+r:E.Repr,+args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  +k = List.length(&2,E.Ktm,args)
  do S.Comp<List<&2,G.Instr>>:
    ia : List<&2,G.Instr> <- each(c,env,args)
    +fi : U32 <- S.lift(U32,L.func_index(layout(c),L.apply_fkey(k)))
    want : E.Repr <- S.lift(E.Repr,L.steps_result(r,k))
    cz : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),L.any_repr(),want))
    return cats([head,ia,[Bool.pick(G.Instr,tail_ok(c,tail,L.any_repr()),G.Return_call{fi},G.Call{fi})],cz])

@unsafe def steps_type(+c:Ctx,+env:List<&2,Binding>,tail:Bool,head:List<&2,G.Instr>,+r:E.Repr,args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match r:
    case E.RFunc{E.Tid{t}}:
      do S.Comp<List<&2,G.Instr>>:
        m : Nat <- S.lift(Nat,L.arity_of_fn(t))
        steps_apply(c,env,tail,head,r,args)
    case E.RUnion{t}: steps_apply(c,env,tail,head,r,args)
    case E.RThunk{t}: S.fail(List<&2,G.Instr>,Err.Not_yet{"KForce arrives at M2"})
    case _: S.fail(List<&2,G.Instr>,Err.Mismatch{"an application head is not a function"})

@unsafe def steps(c,env,tail,head,r,args):
  Bool.pick(S.Comp(List<&2,G.Instr>),Nat.is_eq(List.length(&2,E.Ktm,args),0n) && Bool.not(L.nullary_closure(r)),S.pure(List<&2,G.Instr>,head),steps_type(c,env,tail,head,r,args))

def capture_env(+l:L.T,n:Nat,rs:List<&2,E.Repr>,code:List<&2,G.Instr>) -> S.Comp(List<&2,G.Instr>):
  match n:
    case 0n: S.pure(List<&2,G.Instr>,[G.I32_const{0},G.Ref_i31{}])
    case 1n+p:
      do S.Comp<List<&2,G.Instr>>:
        i : U32 <- S.lift(U32,L.type_index(l,L.env_tid(rs)))
        return cat(code,[G.Struct_new{i}])

@unsafe def tag_payload(p:Maybe<&2,C.Pair<String,List<&2,E.Repr>>>,+c:Ctx,env:List<&2,Binding>,head:List<&2,G.Instr>,args:List<&2,E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match p:
    case None{}: S.pure(List<&2,G.Instr>,head)
    case Some{C.Pair{key,rs}}:
      do S.Comp<List<&2,G.Instr>>:
        li : U32 <- S.lift(U32,L.type_index(layout(c),key))
        code : List<&2,G.Instr> <- each2(c,env,args,rs)
        return cats([head,code,[G.Struct_new{li}]])

@unsafe def case_nonempty(c:Ctx,env:List<&2,Binding>,tail:Bool,tid:E.Tid,sc:E.Ktm,code:List<&2,G.Instr>,bs:List<&2,E.Branch<E.Ktm>>) -> S.Comp(List<&2,G.Instr>):
  match bs:
    case Nil{}: S.pure(List<&2,G.Instr>,cat(code,[G.Unreachable{}]))
    case Con{b,rest}: case_term(c,env,tail,tid,sc,code,b <> rest)

@unsafe def go(c,env,tail,tm):
  match tm:
    case E.KVar{i}: var(nth(Binding,env,i),i)
    case E.KLit{lit}: literal(layout(c),lit)
    case E.KGlobal{n}: global(c,n,tail)
    case E.KErased{}: S.pure(List<&2,G.Instr>,[G.I32_const{0},G.Ref_i31{}])
    case E.KLet{name,v,b}:
      do S.Comp<List<&2,G.Instr>>:
        iv : List<&2,G.Instr> <- go(c,env,False{},v)
        +vr : E.Repr <- S.lift(E.Repr,rep(c,env,v))
        vt : G.Valtype <- S.lift(G.Valtype,L.valtype_of(layout(c),vr))
        +idx : U32 <- S.alloc(vt)
        ib : List<&2,G.Instr> <- go(c,Binding{idx,vr} <> env,tail,b)
        return cats([iv,[G.Local_set{idx}],ib])
    case E.KClos{E.Fid{f},n,caps}:
      do S.Comp<List<&2,G.Instr>>:
        +rs : List<&2,E.Repr> <- S.lift(List<&2,E.Repr>,L.capture_reprs(prog(layout(c)),f,List.length(&2,E.Ktm,caps)))
        ic : List<&2,G.Instr> <- each2(c,env,caps,rs)
        wi : U32 <- S.lift(U32,L.func_index(layout(c),L.wrap_fkey(f,List.length(&2,E.Ktm,caps))))
        ci : U32 <- S.lift(U32,L.type_index(layout(c),"clos"))
        ie : List<&2,G.Instr> <- capture_env(layout(c),List.length(&2,E.Ktm,caps),rs,ic)
        return cats([[G.I32_const{U32.from_nat(n)},G.Ref_func{wi}],ie,[G.Struct_new{ci}]])
    case E.KApp{h,args}: apply(c,env,False{},h,args)
    case E.KTail{h,args}: apply(c,env,tail,h,args)
    case E.KStruct{E.Tid{t},fs}:
      do S.Comp<List<&2,G.Instr>>:
        rs : List<&2,E.Repr> <- S.lift(List<&2,E.Repr>,L.fields_of(t))
        ti : U32 <- S.lift(U32,L.type_index(layout(c),t))
        code : List<&2,G.Instr> <- each2(c,env,fs,rs)
        return cat(code,[G.Struct_new{ti}])
    case E.KProj{E.Tid{t},k,x}:
      do S.Comp<List<&2,G.Instr>>:
        code : List<&2,G.Instr> <- arg_at(c,env,x,E.RStruct{E.Tid{t}})
        ti : U32 <- S.lift(U32,L.type_index(layout(c),t))
        r : E.Repr <- S.lift(E.Repr,rep(c,env,tm))
        cz : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),L.any_repr(),r))
        return cats([code,[G.Struct_get{ti,U32.from_nat(k)}],cz])
    case E.KTag{E.Tid{t},k,ps}:
      do S.Comp<List<&2,G.Instr>>:
        legs : List<&2,L.Leg> <- S.lift(List<&2,L.Leg>,L.sum_legs_p(prog(layout(c)),t))
        leg : L.Leg <- S.lift(L.Leg,optional(L.Leg,nth(L.Leg,legs,k),Err.Wrong_leg{"leg " ++ Nat.show(k) ++ " of " ++ t}))
        tag_payload(L.leg_type(leg),c,env,[G.I32_const{U32.from_nat(k)},G.Ref_i31{}],ps)
    case E.KCase{tid,sc,bs}:
      do S.Comp<List<&2,G.Instr>>:
        code : List<&2,G.Instr> <- go(c,env,False{},sc)
        case_nonempty(c,env,tail,tid,sc,code,bs)
    case E.KDelay{f,cs}: S.fail(List<&2,G.Instr>,Err.Not_yet{"KDelay arrives at M2"})
    case E.KForce{x}: S.fail(List<&2,G.Instr>,Err.Not_yet{"KForce arrives at M2"})

def add_leg_key(p:Maybe<&2,C.Pair<String,List<&2,E.Repr>>>,keys:List<&2,String>) -> List<&2,String>:
  match p:
    case None{}: keys
    case Some{C.Pair{key,rs}}: key <> keys

def leg_keys(ls:List<&2,L.Leg>) -> List<&2,String>:
  match ls:
    case Nil{}: []
    case Con{l,rest}: add_leg_key(L.leg_type(l),leg_keys(rest))

def has_string(xs:List<&2,String>,+x:String) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{h,t}: String.eq(h,x) || has_string(t,x)

def unique(xs:List<&2,String>,+seen:List<&2,String>) -> List<&2,String>:
  match xs:
    case Nil{}: []
    case Con{+x,rest}:
      +tail = unique(rest,x <> seen)
      Bool.pick(List<&2,String>,has_string(seen,x),tail,x <> tail)

def indices(+l:L.T,keys:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,U32>>:
  match keys:
    case Nil{}: Done{[]}
    case Con{k,rest}:
      do Result<&2,&2,Err.T,List<&2,U32>>:
        i : U32 <- L.type_index(l,k)
        tail_indices : List<&2,U32> <- indices(l,rest)
        return i <> tail_indices

def cast_tags(shapes:List<&2,U32>,+depth:U32) -> List<&2,G.Instr>:
  match shapes:
    case Nil{}: [G.Unreachable{}]
    case Con{i,rest}: G.Br_on_cast{depth,G.HEq{},G.HType{i}} <> cast_tags(rest,U32.add(depth,1))

def tag_layers(shapes:List<&2,U32>,code:List<&2,G.Instr>) -> List<&2,G.Instr>:
  match shapes:
    case Nil{}: code
    case Con{+i,+rest}:
      +n = U32.from_nat(List.length(&2,U32,rest))
      post = Bool.pick(List<&2,G.Instr>,U32.is_eq(n,0),[],[G.Br{n}])
      tag_layers(rest,cat([G.Block{Some{G.Ref{G.HType{i}}},code},G.Struct_get{i,0},G.I31_get_u{}],post))

def tag_block(sv:U32,+shapes:List<&2,U32>) -> G.Instr:
  +p = U32.from_nat(List.length(&2,U32,shapes))
  core = cat([G.Local_get{sv},G.Br_on_cast{0,G.HEq{},G.HI31{}}],cast_tags(shapes,1))
  start = cat([G.Block{Some{G.Ref{G.HI31{}}},core},G.I31_get_u{}],Bool.pick(List<&2,G.Instr>,U32.is_eq(p,0),[],[G.Br{p}]))
  G.Block{Some{G.I32{}},tag_layers(shapes,start)}

type Reads is Data:
  Reads{code:List<&2,G.Instr>,env:List<&2,Binding>}

def reads_code(r:Reads) -> List<&2,G.Instr>:
  match r:
    case Reads{code,env}: code

def reads_env(r:Reads) -> List<&2,Binding>:
  match r:
    case Reads{code,env}: env

@unsafe def leg_reads(+c:Ctx,+sv:U32,+li:U32,+i:U32,rs:List<&2,E.Repr>) -> S.Comp(Reads):
  match rs:
    case Nil{}: S.pure(Reads,Reads{[],[]})
    case Con{+r,rest}:
      do S.Comp<Reads>:
        vt : G.Valtype <- S.lift(G.Valtype,L.valtype_of(layout(c),r))
        +idx : U32 <- S.alloc(vt)
        cz : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),L.any_repr(),r))
        +more : Reads <- leg_reads(c,sv,li,U32.add(i,1),rest)
        return Reads{cats([[G.Local_get{sv},G.Ref_cast{G.HType{li}},G.Struct_get{li,U32.add(i,1)}],cz,[G.Local_set{idx}],reads_code(more)]),List.append(&2,Binding,reads_env(more),[Binding{idx,r}])}

@unsafe def branch_code(+c:Ctx,+env:List<&2,Binding>,tail:Bool,cr:E.Repr,+body:E.Ktm) -> S.Comp(List<&2,G.Instr>):
  do S.Comp<List<&2,G.Instr>>:
    ib : List<&2,G.Instr> <- go(c,env,tail,body)
    br : E.Repr <- S.lift(E.Repr,rep(c,env,body))
    cz : List<&2,G.Instr> <- S.lift(List<&2,G.Instr>,coerce(layout(c),br,cr))
    return with_coercion(ib,cz)

@unsafe def branch_payload(p:C.Pair<String,List<&2,E.Repr>>,+c:Ctx,env:List<&2,Binding>,tail:Bool,sv:U32,cr:E.Repr,body:E.Ktm) -> S.Comp(List<&2,G.Instr>):
  match p:
    case C.Pair{key,rs}:
      do S.Comp<List<&2,G.Instr>>:
        li : U32 <- S.lift(U32,L.type_index(layout(c),key))
        +reads : Reads <- leg_reads(c,sv,li,0,rs)
        ib : List<&2,G.Instr> <- branch_code(c,List.append(&2,Binding,reads_env(reads),env),tail,cr,body)
        return cat(reads_code(reads),ib)

@unsafe def branch_bindings(rs:List<&2,E.Repr>,+c:Ctx,env:List<&2,Binding>,tail:Bool,sr:E.Repr,sv:U32,cr:E.Repr,+b:E.Branch<E.Ktm>) -> S.Comp(List<&2,G.Instr>):
  match rs b:
    case Nil{} E.Branch{k,n,body}: branch_code(c,env,tail,cr,body)
    case Con{r,rest} E.Branch{k,n,body}:
      do S.Comp<List<&2,G.Instr>>:
        leg : L.Leg <- S.lift(L.Leg,L.branch_tid(L.tid_text(sr),prog(layout(c)),k))
        p : C.Pair<String,List<&2,E.Repr>> <- S.lift(C.Pair<String,List<&2,E.Repr>>,optional(C.Pair<String,List<&2,E.Repr>>,L.leg_type(leg),Err.Mismatch{"a branch binds a field of a payload free leg"}))
        branch_payload(p,c,env,tail,sv,cr,body)

@unsafe def branches(+c:Ctx,+env:List<&2,Binding>,+tail:Bool,+sr:E.Repr,+sv:U32,+cvt:G.Valtype,+cr:E.Repr,+tg:U32,bs:List<&2,E.Branch<E.Ktm>>) -> S.Comp(List<&2,G.Instr>):
  match bs:
    case Nil{}: S.pure(List<&2,G.Instr>,[G.Unreachable{}])
    case Con{+b,rest}:
      E.Branch{k,n,body} = b
      do S.Comp<List<&2,G.Instr>>:
        rs : List<&2,E.Repr> <- S.lift(List<&2,E.Repr>,L.branch_binders(prog(layout(c)),sr,b))
        ib : List<&2,G.Instr> <- branch_bindings(rs,c,env,tail,sr,sv,cr,b)
        others : List<&2,G.Instr> <- branches(c,env,tail,sr,sv,cvt,cr,tg,rest)
        return [G.Local_get{tg},G.I32_const{U32.from_nat(k)},G.I32_eq{},G.If{Some{cvt},ib,others}]

@unsafe def case_term(c,env,tail,tid,sc,code,bs):
  do S.Comp<List<&2,G.Instr>>:
    stid : String <- S.lift(String,optional(String,L.tid_text(E.RUnion{tid}),Err.Mismatch{"a case scrutinee has no tid"}))
    legs : List<&2,L.Leg> <- S.lift(List<&2,L.Leg>,L.sum_legs_p(prog(layout(c)),stid))
    shapes : List<&2,U32> <- S.lift(List<&2,U32>,indices(layout(c),unique(leg_keys(legs),[])))
    +sv : U32 <- S.alloc(G.Ref{G.HEq{}})
    +tg : U32 <- S.alloc(G.I32{})
    +cr : E.Repr <- S.lift(E.Repr,rep(c,env,E.KCase{tid,sc,bs}))
    cvt : G.Valtype <- S.lift(G.Valtype,L.valtype_of(layout(c),cr))
    dis : List<&2,G.Instr> <- branches(c,env,tail,E.RUnion{tid},sv,cvt,cr,tg,bs)
    return cats([code,[G.Local_set{sv},tag_block(sv,shapes),G.Local_set{tg}],dis])

def param_bindings(ps:List<&2,E.Repr>,+i:U32) -> List<&2,Binding>:
  match ps:
    case Nil{}: []
    case Con{p,rest}: Binding{i,p} <> param_bindings(rest,U32.add(i,1))

def param_env(ps:List<&2,E.Repr>) -> List<&2,Binding>:
  List.reverse(&2,Binding,param_bindings(ps,0))

def prog_func(+l:L.T,f:L.Fn) -> Result<&2,&2,Err.T,G.Func>:
  match f:
    case L.Fn{+ps,+r,+body}:
      do Result<&2,&2,Err.T,G.Func>:
        ft : U32 <- L.type_index(l,L.sig_key(ps,r))
        br : E.Repr <- L.infer(prog(l),List.reverse(&2,E.Repr,ps),body)
        cz : List<&2,G.Instr> <- coerce(l,br,r)
        action : S.Comp(List<&2,G.Instr>) = do S.Comp<List<&2,G.Instr>>:
          code : List<&2,G.Instr> <- go(Ctx{l,r},param_env(ps),True{},body)
          return with_coercion(code,cz)
        S.finish(ft,action(S.State{List.length(&2,E.Repr,ps),[]}))

def wrapper_args(ps:List<&2,E.Repr>,+l:L.T,+i:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match ps:
    case Nil{}: Done{[]}
    case Con{p,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        cz : List<&2,G.Instr> <- coerce(l,L.any_repr(),p)
        more : List<&2,G.Instr> <- wrapper_args(rest,l,U32.add(i,1))
        return cats([[G.Local_get{i}],cz,more])

def wrapper_captures(rs:List<&2,E.Repr>,+l:L.T,+ei:U32,+i:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match rs:
    case Nil{}: Done{[]}
    case Con{r,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        cz : List<&2,G.Instr> <- coerce(l,L.any_repr(),r)
        more : List<&2,G.Instr> <- wrapper_captures(rest,l,ei,U32.add(i,1))
        return cats([[G.Local_get{0},G.Ref_cast{G.HType{ei}},G.Struct_get{ei,i}],cz,more])

def wrapper_env(n:Nat,+l:L.T,+rs:List<&2,E.Repr>) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match n:
    case 0n: Done{[]}
    case 1n+p:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        ei : U32 <- L.type_index(l,L.env_tid(rs))
        wrapper_captures(rs,l,ei,0)

def wrapper_kind(k:L.HeadKind,+l:L.T,+name:String,+caps:Nat) -> Result<&2,&2,Err.T,G.Func>:
  match k:
    case L.HFun{n,L.Fn{+ps,r,b}}:
      do Result<&2,&2,Err.T,G.Func>:
        ft : U32 <- L.type_index(l,L.fn_key(Nat.sub(List.length(&2,E.Repr,ps),caps)))
        fi : U32 <- L.func_index(l,"fun:" ++ name)
        rs : List<&2,E.Repr> <- L.capture_reprs(prog(l),name,caps)
        cap_ins : List<&2,G.Instr> <- wrapper_env(caps,l,rs)
        arg_ins : List<&2,G.Instr> <- wrapper_args(List.drop(&2,E.Repr,ps,caps),l,1)
        return G.Func{ft,[],cats([cap_ins,arg_ins,[G.Return_call{fi}]])}
    case L.HPrim{n}:
      do Result<&2,&2,Err.T,G.Func>:
        ft : U32 <- L.type_index(l,L.fn_key(2n))
        fi : U32 <- L.func_index(l,"nat-runtime:" ++ n)
        return G.Func{ft,[],[G.Local_get{1},G.Local_get{2},G.Return_call{fi}]}
    case L.HValue{}: Fail{Err.Unbound{"no runtime value for " ++ name}}

def wrapper(+l:L.T,+name:String,caps:Nat) -> Result<&2,&2,Err.T,G.Func>:
  wrapper_kind(L.head_kind(prog(l),E.KGlobal{name}),l,name,caps)

def gets(n:Nat,+start:U32) -> List<&2,G.Instr>:
  match n:
    case 0n: []
    case 1n+p: G.Local_get{start} <> gets(p,U32.add(start,1))

def apply_arm_kind(order:Cmp,+l:L.T,+k:Nat,+m:Nat,+cl:U32,+ci:U32,+fm:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match order:
    case EQ{}: Done{cats([[G.Local_get{cl},G.Struct_get{ci,2}],gets(k,1),[G.Local_get{cl},G.Struct_get{ci,1},G.Ref_cast{G.HType{fm}},G.Return_call_ref{fm}]])}
    case LT{}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        rest : U32 <- L.func_index(l,L.apply_fkey(Nat.sub(k,m)))
        return cats([[G.Local_get{cl},G.Struct_get{ci,2}],gets(m,1),[G.Local_get{cl},G.Struct_get{ci,1},G.Ref_cast{G.HType{fm}},G.Call_ref{fm}],gets(Nat.sub(k,m),U32.from_nat(1n+m)),[G.Return_call{rest}]])
    case GT{}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        pi : U32 <- L.type_index(l,L.pap_key(m,k))
        fw : U32 <- L.func_index(l,L.papw_fkey(m,k))
        return cats([[G.I32_const{U32.from_nat(Nat.sub(m,k))},G.Ref_func{fw},G.Local_get{0}],gets(k,1),[G.Struct_new{pi},G.Struct_new{ci}]])

def apply_arm(+l:L.T,+k:Nat,+m:Nat,cl:U32,ci:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  do Result<&2,&2,Err.T,List<&2,G.Instr>>:
    fm : U32 <- L.type_index(l,L.fn_key(m))
    apply_arm_kind(Nat.cmp(m,k),l,k,m,cl,ci,fm)

def apply_chain(ms:List<&2,Nat>,+l:L.T,+k:Nat,+cl:U32,+ci:U32,+ar:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match ms:
    case Nil{}: Done{[G.Unreachable{}]}
    case Con{+m,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        b : List<&2,G.Instr> <- apply_arm(l,k,m,cl,ci)
        more : List<&2,G.Instr> <- apply_chain(rest,l,k,cl,ci,ar)
        return [G.Local_get{ar},G.I32_const{U32.from_nat(m)},G.I32_eq{},G.If{Some{G.Ref{G.HEq{}}},b,more}]

def arities(l:L.T) -> List<&2,Nat>:
  match l:
    case L.Linked{p,ts,gs,tm,fs,fm,a,k,d}: a

def apply_func(+l:L.T,+k:Nat) -> Result<&2,&2,Err.T,G.Func>:
  +cl = U32.from_nat(1n+k)
  +ar = U32.from_nat(2n+k)
  do Result<&2,&2,Err.T,G.Func>:
    ft : U32 <- L.type_index(l,L.apply_ty_key(k))
    +ci : U32 <- L.type_index(l,"clos")
    chain : List<&2,G.Instr> <- apply_chain(arities(l),l,k,cl,ci,ar)
    return G.Func{ft,[G.Ref{G.HType{ci}},G.I32{}],cat([G.Local_get{0},G.Ref_cast{G.HType{ci}},G.Local_set{cl},G.Local_get{cl},G.Struct_get{ci,0},G.Local_set{ar}],chain)}

def saved_args(n:Nat,+pv:U32,+pi:U32,+i:U32) -> List<&2,G.Instr>:
  match n:
    case 0n: []
    case 1n+p: cat([G.Local_get{pv},G.Struct_get{pi,i}],saved_args(p,pv,pi,U32.add(i,1)))

def papw_func(+l:L.T,+m:Nat,+k:Nat) -> Result<&2,&2,Err.T,G.Func>:
  +n = Nat.sub(m,k)
  +pv = U32.from_nat(1n+n)
  +cv = U32.from_nat(2n+n)
  do Result<&2,&2,Err.T,G.Func>:
    ft : U32 <- L.type_index(l,L.fn_key(n))
    +ci : U32 <- L.type_index(l,"clos")
    +pi : U32 <- L.type_index(l,L.pap_key(m,k))
    +fm : U32 <- L.type_index(l,L.fn_key(m))
    return G.Func{ft,[G.Ref{G.HType{pi}},G.Ref{G.HType{ci}}],cats([[G.Local_get{0},G.Ref_cast{G.HType{pi}},G.Local_set{pv},G.Local_get{pv},G.Struct_get{pi,0},G.Ref_cast{G.HType{ci}},G.Local_set{cv},G.Local_get{cv},G.Struct_get{ci,2}],saved_args(k,pv,pi,1),gets(n,1),[G.Local_get{cv},G.Struct_get{ci,1},G.Ref_cast{G.HType{fm}},G.Return_call_ref{fm}]])}

def entry_func(+l:L.T,export:String) -> Result<&2,&2,Err.T,G.Func>:
  do Result<&2,&2,Err.T,G.Func>:
    ft : U32 <- L.type_index(l,"entryfn")
    fi : U32 <- L.func_index(l,"fun:" ++ export)
    return G.Func{ft,[],[G.Call{fi},G.Ref_cast{G.HI31{}},G.I31_get_s{}]}

def one_func(+l:L.T,export:String,spec:L.Fspec) -> Result<&2,&2,Err.T,G.Func>:
  match spec:
    case L.FSRuntime{n}: N.runtime_func(l,n)
    case L.FSProg{+n}:
      do Result<&2,&2,Err.T,G.Func>:
        f : L.Fn <- optional(L.Fn,L.lookup(L.Fn,L.prog_funcs(prog(l)),n),Err.Unbound{"no function named " ++ n})
        prog_func(l,f)
    case L.FSWrap{n,caps}: wrapper(l,n,caps)
    case L.FSApply{k}: apply_func(l,k)
    case L.FSPapw{m,k}: papw_func(l,m,k)
    case L.FSEntry{}: entry_func(l,export)

def export_name(e:Maybe<&2,String>) -> String:
  match e:
    case None{}: ""
    case Some{s}: s

def funcs(fs:List<&2,C.Pair<String,L.Fspec>>,+l:L.T,+export:Maybe<&2,String>) -> Result<&2,&2,Err.T,List<&2,G.Func>>:
  match fs export:
    case Nil{} _: Done{[]}
    case Con{C.Pair{k,L.FSEntry{}},rest} None{}:
      do Result<&2,&2,Err.T,List<&2,G.Func>>:
        ft : U32 <- L.type_index(l,"entryfn")
        tail : List<&2,G.Func> <- funcs(rest,l,export)
        return G.Func{ft,[],[G.Unreachable{}]} <> tail
    case Con{C.Pair{k,s},rest} _:
      do Result<&2,&2,Err.T,List<&2,G.Func>>:
        f : G.Func <- one_func(l,export_name(export),s)
        tail : List<&2,G.Func> <- funcs(rest,l,export)
        return f <> tail

def check_export(f:Maybe<&2,L.Fn>,+export:String) -> Result<&2,&2,Err.T,Unit>:
  match f:
    case None{}: Fail{Err.Unbound{"no definition named " ++ export}}
    case Some{L.Fn{ps,r,b}}:
      Bool.pick(Result<&2,&2,Err.T,Unit>,Nat.is_eq(List.length(&2,E.Repr,ps),0n) && same(r,L.nat_repr()),Done{Unit{}},Fail{Err.Mismatch{export ++ " is not a Nat definition of arity 0"}})

def program_linked(+l:L.T,+export:String) -> Result<&2,&2,Err.T,List<&2,U32>>:
  match l:
    case L.Linked{p,ts,gs,tm,fs,fm,a,k,d}:
      do Result<&2,&2,Err.T,List<&2,U32>>:
        types : List<&2,List<&2,G.Comptype>> <- L.comptype_groups(l)
        functions : List<&2,G.Func> <- funcs(fs,l,Some{export})
        ei : U32 <- L.func_index(l,"entry")
        return G.encode(G.Module{types,[],functions,[G.Export{export,ei}],d})

def program(+rows:List<&2,C.Pair<String,Erase.Entry>>,+export:String) -> Result<&2,&2,Err.T,List<&2,U32>>:
  do Result<&2,&2,Err.T,List<&2,U32>>:
    u : Unit <- check_export(L.lookup(L.Fn,L.prog_funcs(L.program_table(rows)),export),export)
    l : L.T <- L.build(rows)
    program_linked(l,export)

type HostHandle is Data:
  HostHandle{repr:E.Repr,index:U32,padding:Nat,payload:G.Valtype}

def host_find(hs:List<&2,HostHandle>,+r:E.Repr) -> Result<&2,&2,Err.T,HostHandle>:
  match hs:
    case Nil{}: Fail{Err.Mismatch{"unsupported host representation " ++ E.print_repr(r)}}
    case Con{+h,rest}:
      HostHandle{hr,i,p,v} = h
      Bool.pick(Result<&2,&2,Err.T,HostHandle>,same(hr,r),Done{h},host_find(rest,r))

def handle_type(h:HostHandle) -> G.Valtype:
  match h:
    case HostHandle{r,i,p,v}: G.Ref{G.HType{i}}

def host_type(hs:List<&2,HostHandle>,+r:E.Repr) -> Result<&2,&2,Err.T,G.Valtype>:
  Bool.pick(Result<&2,&2,Err.T,G.Valtype>,same(r,L.nat_repr()),Done{G.I32{}},do Result<&2,&2,Err.T,G.Valtype>:
    h : HostHandle <- host_find(hs,r)
    return handle_type(h))

def host_types(rs:List<&2,E.Repr>,+hs:List<&2,HostHandle>) -> Result<&2,&2,Err.T,List<&2,G.Valtype>>:
  match rs:
    case Nil{}: Done{[]}
    case Con{r,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Valtype>>:
        t : G.Valtype <- host_type(hs,r)
        tail : List<&2,G.Valtype> <- host_types(rest,hs)
        return t <> tail

def host_read(h:HostHandle,i:U32) -> List<&2,G.Instr>:
  match h:
    case HostHandle{r,+index,p,v}: [G.Local_get{i},G.Ref_cast{G.HType{index}},G.Struct_get{index,0}]

def host_argument(hs:List<&2,HostHandle>,+i:U32,+r:E.Repr) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  Bool.pick(Result<&2,&2,Err.T,List<&2,G.Instr>>,same(r,L.nat_repr()),Done{[G.Local_get{i},G.I32_const{1073741823},G.I32_gt_u{},G.If{None{},[G.Unreachable{}],[]},G.Local_get{i},G.Ref_i31{}]},do Result<&2,&2,Err.T,List<&2,G.Instr>>:
    h : HostHandle <- host_find(hs,r)
    return host_read(h,i))

def zeros(n:Nat) -> List<&2,G.Instr>:
  match n:
    case 0n: []
    case 1n+p: G.I32_const{0} <> zeros(p)

def host_wrap(h:HostHandle) -> N.Body:
  match h:
    case HostHandle{r,index,p,v}: N.Body{[],cat(zeros(p),[G.Struct_new{index}])}

def host_result(hs:List<&2,HostHandle>,+local:U32,+r:E.Repr) -> Result<&2,&2,Err.T,N.Body>:
  Bool.pick(Result<&2,&2,Err.T,N.Body>,same(r,L.nat_repr()),Done{N.Body{[G.I32{}],[G.Ref_cast{G.HI31{}},G.I31_get_s{},G.Local_set{local},G.Local_get{local},G.I32_const{1073741823},G.I32_gt_u{},G.If{None{},[G.Unreachable{}],[]},G.Local_get{local}]}},do Result<&2,&2,Err.T,N.Body>:
    h : HostHandle <- host_find(hs,r)
    return host_wrap(h))

def host_arguments(ps:List<&2,E.Repr>,+hs:List<&2,HostHandle>,+i:U32) -> Result<&2,&2,Err.T,List<&2,G.Instr>>:
  match ps:
    case Nil{}: Done{[]}
    case Con{p,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Instr>>:
        a : List<&2,G.Instr> <- host_argument(hs,i,p)
        tail : List<&2,G.Instr> <- host_arguments(rest,hs,U32.add(i,1))
        return cat(a,tail)

def host_func(ft:U32,fi:U32,args:List<&2,G.Instr>,body:N.Body) -> G.Func:
  match body:
    case N.Body{ls,code}: G.Func{ft,ls,cats([args,[G.Call{fi}],code])}

def host_wrapper(+l:L.T,+hs:List<&2,HostHandle>,ft:U32,name:String,f:L.Fn) -> Result<&2,&2,Err.T,G.Func>:
  match f:
    case L.Fn{+ps,r,b}:
      do Result<&2,&2,Err.T,G.Func>:
        fi : U32 <- L.func_index(l,"fun:" ++ name)
        args : List<&2,G.Instr> <- host_arguments(ps,hs,0)
        result : N.Body <- host_result(hs,U32.from_nat(List.length(&2,E.Repr,ps)),r)
        return host_func(ft,fi,args,result)

def host_definitions(ns:List<&2,String>,+p:L.Prog) -> Result<&2,&2,Err.T,List<&2,C.Pair<String,L.Fn>>>:
  match ns:
    case Nil{}: Done{[]}
    case Con{+n,rest}:
      do Result<&2,&2,Err.T,List<&2,C.Pair<String,L.Fn>>>:
        f : L.Fn <- optional(L.Fn,L.lookup(L.Fn,L.prog_funcs(p),n),Err.Unbound{"no definition named " ++ n})
        tail : List<&2,C.Pair<String,L.Fn>> <- host_definitions(rest,p)
        return C.Pair{n,f} <> tail

def host_reprs(ds:List<&2,C.Pair<String,L.Fn>>) -> List<&2,E.Repr>:
  match ds:
    case Nil{}: []
    case Con{C.Pair{n,L.Fn{ps,r,b}},rest}: r <> List.append(&2,E.Repr,ps,host_reprs(rest))

def text_order(pair:(String & String) & Cmp) -> Cmp:
  (texts,order) = pair
  order

law repr_insert:
  for xs:List<&2,E.Repr>
  for +x:E.Repr
  List<&2,E.Repr>

@unsafe def repr_insert_order(order:Cmp,+x:E.Repr,+y:E.Repr,+rest:List<&2,E.Repr>) -> List<&2,E.Repr>:
  match order:
    case EQ{}: y <> rest
    case LT{}: x <> y <> rest
    case GT{}: y <> repr_insert(rest,x)

@unsafe def repr_insert(xs,x):
  match xs:
    case Nil{}: [x]
    case Con{+y,rest}: repr_insert_order(text_order(String.cmp(E.print_repr(x),E.print_repr(y))),x,y,rest)

def repr_unique(xs:List<&2,E.Repr>) -> List<&2,E.Repr>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: repr_insert(repr_unique(rest),x)

def supported(rs:List<&2,E.Repr>) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match rs:
    case Nil{}: Done{[]}
    case Con{E.RFunc{t},rest}: Fail{Err.Not_yet{"host function and thunk values are not supported"}}
    case Con{E.RThunk{t},rest}: Fail{Err.Not_yet{"host function and thunk values are not supported"}}
    case Con{+r,rest}:
      do Result<&2,&2,Err.T,List<&2,E.Repr>>:
        +tail : List<&2,E.Repr> <- supported(rest)
        return Bool.pick(List<&2,E.Repr>,same(r,L.nat_repr()),tail,r <> tail)

def handles(rs:List<&2,E.Repr>,+l:L.T,+base:Nat,+padding:Nat) -> Result<&2,&2,Err.T,List<&2,HostHandle>>:
  match rs:
    case Nil{}: Done{[]}
    case Con{+r,rest}:
      do Result<&2,&2,Err.T,List<&2,HostHandle>>:
        payload : G.Valtype <- L.valtype_of(l,r)
        tail : List<&2,HostHandle> <- handles(rest,l,base,1n+padding)
        return HostHandle{r,U32.from_nat(Nat.add(base,padding)),padding,payload} <> tail

def i32s(n:Nat) -> List<&2,G.Valtype>:
  match n:
    case 0n: []
    case 1n+p: G.I32{} <> i32s(p)

def handle_types(hs:List<&2,HostHandle>) -> List<&2,List<&2,G.Comptype>>:
  match hs:
    case Nil{}: []
    case Con{HostHandle{r,i,p,v},rest}: [G.CStruct{v <> i32s(p)}] <> handle_types(rest)

def host_signatures(ds:List<&2,C.Pair<String,L.Fn>>,+hs:List<&2,HostHandle>) -> Result<&2,&2,Err.T,List<&2,List<&2,G.Comptype>>>:
  match ds:
    case Nil{}: Done{[]}
    case Con{C.Pair{n,L.Fn{ps,r,b}},rest}:
      do Result<&2,&2,Err.T,List<&2,List<&2,G.Comptype>>>:
        params : List<&2,G.Valtype> <- host_types(ps,hs)
        result : G.Valtype <- host_type(hs,r)
        tail : List<&2,List<&2,G.Comptype>> <- host_signatures(rest,hs)
        return [G.CFunc{params,[result]}] <> tail

def host_wrappers(ds:List<&2,C.Pair<String,L.Fn>>,+l:L.T,+hs:List<&2,HostHandle>,+ft:U32) -> Result<&2,&2,Err.T,List<&2,G.Func>>:
  match ds:
    case Nil{}: Done{[]}
    case Con{C.Pair{n,f},rest}:
      do Result<&2,&2,Err.T,List<&2,G.Func>>:
        w : G.Func <- host_wrapper(l,hs,ft,n,f)
        tail : List<&2,G.Func> <- host_wrappers(rest,l,hs,U32.add(ft,1))
        return w <> tail

def host_exports(ds:List<&2,C.Pair<String,L.Fn>>,+base:U32) -> List<&2,G.Export>:
  match ds:
    case Nil{}: []
    case Con{C.Pair{n,f},rest}: G.Export{n,base} <> host_exports(rest,U32.add(base,1))

def reactor_linked(+l:L.T,ns:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,U32>>:
  match l:
    case L.Linked{+p,+ts,gs,tm,+fs,fm,a,k,d}:
      do Result<&2,&2,Err.T,List<&2,U32>>:
        +ds : List<&2,C.Pair<String,L.Fn>> <- host_definitions(ns,p)
        rs : List<&2,E.Repr> <- supported(repr_unique(host_reprs(ds)))
        +hs : List<&2,HostHandle> <- handles(rs,l,List.length(&2,C.Pair<String,L.Tspec>,ts),0n)
        sigs : List<&2,List<&2,G.Comptype>> <- host_signatures(ds,hs)
        types : List<&2,List<&2,G.Comptype>> <- L.comptype_groups(l)
        +functions : List<&2,G.Func> <- funcs(fs,l,None{})
        wrappers : List<&2,G.Func> <- host_wrappers(ds,l,hs,U32.from_nat(Nat.add(List.length(&2,C.Pair<String,L.Tspec>,ts),List.length(&2,HostHandle,hs))))
        return G.encode(G.Module{List.append(&2,List<&2,G.Comptype>,types,List.append(&2,List<&2,G.Comptype>,handle_types(hs),sigs)),[],List.append(&2,G.Func,functions,wrappers),host_exports(ds,U32.from_nat(List.length(&2,G.Func,functions))),d})

def valid_exports(+ns:List<&2,String>) -> Result<&2,&2,Err.T,Unit>:
  match ns:
    case Nil{}: Fail{Err.Mismatch{"build needs at least one export"}}
    case Con{n,rest}:
      Bool.pick(Result<&2,&2,Err.T,Unit>,Nat.is_eq(List.length(&2,String,ns),List.length(&2,String,unique(ns,[]))),Done{Unit{}},Fail{Err.Mismatch{"duplicate export name"}})

def reactor(rows:List<&2,C.Pair<String,Erase.Entry>>,+exports:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,U32>>:
  do Result<&2,&2,Err.T,List<&2,U32>>:
    u : Unit <- valid_exports(exports)
    l : L.T <- L.build(rows)
    reactor_linked(l,exports)
