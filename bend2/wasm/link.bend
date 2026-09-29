import Base
import ./gc_encode.bend as G
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as Erase
import ../kernel/error.bend as Err
import ../kernel/common.bend as C

type Leg is Data:
  LUnit{}
  LRepr{repr:E.Repr}
  LMu{tid:String, fields:List<&2,E.Repr>}

type Form is Data:
  FI31{}
  FUnit{}
  FAny{}
  FFields{fields:List<&2,E.Repr>}
  FSum{legs:List<&2,Leg>}
  FFn{arity:Nat}

type Fn is Data:
  Fn{params:List<&2,E.Repr>, result:E.Repr, body:E.Ktm}

type MuLeg is Data:
  MuLeg{tid:String, family:String, tag:Nat, fields:List<&2,E.Repr>}

type Prog is Data:
  Prog{funs:List<&2,C.Pair<String,Fn>>, posts:List<&2,C.Pair<String,E.Repr>>, mus:List<&2,MuLeg>}

def choose(-A:Data, b:Bool, yes:A, no:A) -> A:
  match b:
    case True{}: yes
    case False{}: no

def inside(+prefix:String,+s:String) -> Maybe<&2,String>:
  choose(Maybe<&2,String>,String.starts_with(s,prefix) && String.ends_with(s,">"),
    Some{String.take(String.drop(s,String.length(prefix)),Nat.sub(Nat.sub(String.length(s),String.length(prefix)),1n))},None{})

def split_push(c:Char, rest:List<&2,String>) -> List<&2,String>:
  match rest:
    case Nil{}: [String.from_list([c])]
    case Con{h,t}: (String.from_list([c]) ++ h) <> t

def split_cut(cut:Bool, c:Char, rest:List<&2,String>) -> List<&2,String>:
  match cut:
    case True{}: "" <> rest
    case False{}: split_push(c,rest)

def split_chars(cs:List<&2,Char>, +sep:Char, +depth:Nat) -> List<&2,String>:
  match cs:
    case Nil{}: [""]
    case Con{'<',rest}: split_push('<',split_chars(rest,sep,1n+depth))
    case Con{'>',rest}: split_push('>',split_chars(rest,sep,Nat.sub(depth,1n)))
    case Con{+c,rest}: split_cut(Char.is_eq(c,sep) && Nat.is_eq(depth,0n),c,split_chars(rest,sep,depth))

def split_top(sep:Char,s:String) -> List<&2,String>:
  split_chars(String.to_list(s),sep,0n)

def repr_of_text(+s:String) -> Result<&2,&2,Err.T,E.Repr>:
  choose(Result<&2,&2,Err.T,E.Repr>,String.eq(s,"i31"),Done{E.RI31{}},
  choose(Result<&2,&2,Err.T,E.Repr>,String.starts_with(s,"struct "),Done{E.RStruct{E.Tid{String.drop(s,7n)}}},
  choose(Result<&2,&2,Err.T,E.Repr>,String.starts_with(s,"union "),Done{E.RUnion{E.Tid{String.drop(s,6n)}}},
  choose(Result<&2,&2,Err.T,E.Repr>,String.starts_with(s,"func "),Done{E.RFunc{E.Tid{String.drop(s,5n)}}},
  choose(Result<&2,&2,Err.T,E.Repr>,String.starts_with(s,"thunk "),Done{E.RThunk{E.Tid{String.drop(s,6n)}}},
    Fail{Err.Mismatch{"a repr does not parse:  " ++ s}})))))

def parse_reprs(xs:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match xs:
    case Nil{}: Done{[]}
    case Con{s,rest}:
      do Result<&2,&2,Err.T,List<&2,E.Repr>>:
        r : E.Repr <- repr_of_text(s)
        rs : List<&2,E.Repr> <- parse_reprs(rest)
        return r <> rs

def parse_fields_case(empty:Bool,body:String) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match empty:
    case True{}: Done{[]}
    case False{}: parse_reprs(split_top(',',body))

def parse_fields(+body:String) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  parse_fields_case(String.eq(body,""),body)

def parse_leg(unit:Bool,s:String) -> Result<&2,&2,Err.T,Leg>:
  match unit:
    case True{}: Done{LUnit{}}
    case False{}:
      do Result<&2,&2,Err.T,Leg>:
        r : E.Repr <- repr_of_text(s)
        return LRepr{r}

def parse_legs(xs:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,Leg>>:
  match xs:
    case Nil{}: Done{[]}
    case Con{+s,rest}:
      do Result<&2,&2,Err.T,List<&2,Leg>>:
        l : Leg <- parse_leg(String.eq(s,"unit"),s)
        ls : List<&2,Leg> <- parse_legs(rest)
        return l <> ls

def parse_sum(empty:Bool,s:String) -> Result<&2,&2,Err.T,Form>:
  match empty:
    case True{}: Done{FSum{[]}}
    case False{}:
      do Result<&2,&2,Err.T,Form>:
        ls : List<&2,Leg> <- parse_legs(split_top('|',s))
        return FSum{ls}

def parse_arity(n:Maybe<&2,Nat>, t:String) -> Result<&2,&2,Err.T,Form>:
  match n:
    case Some{k}: Done{FFn{k}}
    case None{}: Fail{Err.Mismatch{"an arity does not parse:  " ++ t}}

def parse_wrapped(fn:Maybe<&2,String>, pair:Maybe<&2,String>, tuple:Maybe<&2,String>, sum:Maybe<&2,String>, t:String) -> Result<&2,&2,Err.T,Form>:
  match fn pair tuple sum:
    case Some{n} _ _ _: parse_arity(Nat.read(n),t)
    case None{} Some{body} _ _:
      do Result<&2,&2,Err.T,Form>:
        rs : List<&2,E.Repr> <- parse_fields(body)
        return FFields{rs}
    case None{} None{} Some{body} _:
      do Result<&2,&2,Err.T,Form>:
        rs : List<&2,E.Repr> <- parse_fields(body)
        return FFields{rs}
    case None{} None{} None{} Some{+body}: parse_sum(String.eq(body,""),body)
    case None{} None{} None{} None{}: Fail{Err.Mismatch{"a tid does not parse:  " ++ t}}

def parse_tid(+t:String) -> Result<&2,&2,Err.T,Form>:
  choose(Result<&2,&2,Err.T,Form>,String.eq(t,"i31"),Done{FI31{}},
  choose(Result<&2,&2,Err.T,Form>,String.eq(t,"unit"),Done{FUnit{}},
  choose(Result<&2,&2,Err.T,Form>,String.eq(t,"any"),Done{FAny{}},
    parse_wrapped(inside("fn<",t),inside("pair<",t),inside("tuple<",t),inside("sum<",t),t))))

def mu_fields(parsed:Result<&2,&2,Err.T,List<&2,E.Repr>>, t:String, fam:String, k:Nat) -> Maybe<&2,MuLeg>:
  match parsed:
    case Fail{e}: None{}
    case Done{fs}: Some{MuLeg{t,fam,k,fs}}

def mu_index(family:Maybe<&2,String>, index:Maybe<&2,Nat>, t:String, fam:String, fields:List<&2,String>) -> Maybe<&2,MuLeg>:
  match family index:
    case Some{f} Some{k}: mu_fields(parse_reprs(fields),t,fam,k)
    case _ _: None{}

def mu_parts(parts:List<&2,String>,t:String) -> Maybe<&2,MuLeg>:
  match parts:
    case Con{+fam,Con{k,fs}}: mu_index(inside("mu<",fam),Nat.read(k),t,fam,fs)
    case _: None{}

def mu_inside(body:Maybe<&2,String>,t:String) -> Maybe<&2,MuLeg>:
  match body:
    case None{}: None{}
    case Some{b}: mu_parts(split_top(',',b),t)

def parse_mu_leg(+t:String) -> Maybe<&2,MuLeg>:
  mu_inside(inside("leg<",t),t)

def fields_form(f:Form,t:String) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match f:
    case FFields{rs}: Done{rs}
    case _: Fail{Err.Mismatch{"a struct tid is not a pair or a tuple:  " ++ t}}

def fields_of(+t:String) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  do Result<&2,&2,Err.T,List<&2,E.Repr>>:
    f : Form <- parse_tid(t)
    fields_form(f,t)

def arity_form(f:Form,t:String) -> Result<&2,&2,Err.T,Nat>:
  match f:
    case FFn{n}: Done{n}
    case _: Fail{Err.Mismatch{"a function repr is not fn<n>:  " ++ t}}

def arity_of_fn(+t:String) -> Result<&2,&2,Err.T,Nat>:
  do Result<&2,&2,Err.T,Nat>:
    f : Form <- parse_tid(t)
    arity_form(f,t)

def sum_form(f:Form,t:String) -> Result<&2,&2,Err.T,List<&2,Leg>>:
  match f:
    case FSum{ls}: Done{ls}
    case _: Fail{Err.Mismatch{"a case scrutinee is not a sum:  " ++ t}}

def sum_legs(+t:String) -> Result<&2,&2,Err.T,List<&2,Leg>>:
  do Result<&2,&2,Err.T,List<&2,Leg>>:
    f : Form <- parse_tid(t)
    sum_form(f,t)

def fn_key(n:Nat) -> String:
  "fn<" ++ Nat.show(n) ++ ">"

def nat_repr() -> E.Repr:
  E.RUnion{E.Tid{"nat"}}

def any_repr() -> E.Repr:
  E.RUnion{E.Tid{"any"}}

def tid_text(r:E.Repr) -> Maybe<&2,String>:
  match r:
    case E.RI31{}: None{}
    case E.RStruct{E.Tid{t}}: Some{t}
    case E.RUnion{E.Tid{t}}: Some{t}
    case E.RFunc{E.Tid{t}}: Some{t}
    case E.RThunk{E.Tid{t}}: Some{t}

def decls_of_entry(e:Erase.Entry) -> List<&2,E.Kdecl>:
  match e:
    case Erase.Code{ds}: ds
    case _: []

def funs_of_decls(ds:List<&2,E.Kdecl>) -> List<&2,C.Pair<String,Fn>>:
  match ds:
    case Nil{}: []
    case Con{d,rest}:
      match d:
        case E.KFun{E.Fid{f},ps,r,b}: C.Pair{f,Fn{ps,r,b}} <> funs_of_decls(rest)
        case E.KRec{ts}: funs_of_decls(rest)

def funs_of_rows(rows:List<&2,C.Pair<String,Erase.Entry>>) -> List<&2,C.Pair<String,Fn>>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{n,e},rest}: List.append(&2,C.Pair<String,Fn>,funs_of_decls(decls_of_entry(e)),funs_of_rows(rest))

def posts_of_rows(rows:List<&2,C.Pair<String,Erase.Entry>>) -> List<&2,C.Pair<String,E.Repr>>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{n,e},rest}:
      match e:
        case Erase.Postulate{r}: C.Pair{n,r} <> posts_of_rows(rest)
        case _: posts_of_rows(rest)

def mu_cons(m:Maybe<&2,MuLeg>,rest:List<&2,MuLeg>) -> List<&2,MuLeg>:
  match m:
    case None{}: rest
    case Some{x}: x <> rest

def mus_of_tids(ts:List<&2,E.Tid>) -> List<&2,MuLeg>:
  match ts:
    case Nil{}: []
    case Con{E.Tid{t},rest}: mu_cons(parse_mu_leg(t),mus_of_tids(rest))

def mus_of_decls(ds:List<&2,E.Kdecl>) -> List<&2,MuLeg>:
  match ds:
    case Nil{}: []
    case Con{d,rest}:
      match d:
        case E.KRec{ts}: List.append(&2,MuLeg,mus_of_tids(ts),mus_of_decls(rest))
        case _: mus_of_decls(rest)

def mus_of_rows(rows:List<&2,C.Pair<String,Erase.Entry>>) -> List<&2,MuLeg>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{n,e},rest}: List.append(&2,MuLeg,mus_of_decls(decls_of_entry(e)),mus_of_rows(rest))

def program_table(+rows:List<&2,C.Pair<String,Erase.Entry>>) -> Prog:
  Prog{funs_of_rows(rows),posts_of_rows(rows),mus_of_rows(rows)}

def lookup(-A:Data,rows:List<&2,C.Pair<String,A>>,+name:String) -> Maybe<&2,A>:
  match rows:
    case Nil{}: None{}
    case Con{C.Pair{k,v},rest}: choose(Maybe<&2,A>,String.eq(k,name),Some{v},lookup(A,rest,name))

def mu_family(ms:List<&2,MuLeg>,+family:String) -> List<&2,MuLeg>:
  match ms:
    case Nil{}: []
    case Con{+m,rest}:
      MuLeg{t,f,k,fs} = m
      +tail = mu_family(rest,family)
      choose(List<&2,MuLeg>,String.eq(f,family),m <> tail,tail)

def mu_top(ms:List<&2,MuLeg>) -> Nat:
  match ms:
    case Nil{}: 0n
    case Con{MuLeg{t,f,k,fs},rest}: Nat.max(1n+k,mu_top(rest))

def mu_at(ms:List<&2,MuLeg>,+index:Nat) -> Leg:
  match ms:
    case Nil{}: LUnit{}
    case Con{MuLeg{t,f,k,+fs},rest}:
      choose(Leg,Nat.is_eq(k,index),choose(Leg,Nat.is_eq(List.length(&2,E.Repr,fs),0n),LUnit{},LMu{t,fs}),mu_at(rest,index))

def mu_order(n:Nat, +index:Nat,+ms:List<&2,MuLeg>) -> List<&2,Leg>:
  match n:
    case 0n: []
    case 1n+p: mu_at(ms,index) <> mu_order(p,1n+index,ms)

def sum_mu(ms:List<&2,MuLeg>,t:String) -> Result<&2,&2,Err.T,List<&2,Leg>>:
  match ms:
    case Nil{}: sum_legs(t)
    case Con{+m,+rest}: Done{mu_order(mu_top(m <> rest),0n,m <> rest)}

def sum_legs_p(p:Prog,+t:String) -> Result<&2,&2,Err.T,List<&2,Leg>>:
  match p:
    case Prog{fs,ps,ms}: sum_mu(mu_family(ms,t),t)

def leg_fields(l:Leg) -> List<&2,E.Repr>:
  match l:
    case LUnit{}: []
    case LRepr{r}: [r]
    case LMu{t,rs}: rs

def repr_at(r:Maybe<&2,E.Repr>,error:Err.T) -> Result<&2,&2,Err.T,E.Repr>:
  match r:
    case Some{x}: Done{x}
    case None{}: Fail{error}

def branch_at(l:Maybe<&2,Leg>,t:String,k:Nat) -> Result<&2,&2,Err.T,Leg>:
  match l:
    case Some{x}: Done{x}
    case None{}: Fail{Err.Wrong_leg{"leg " ++ Nat.show(k) ++ " of " ++ t}}

def branch_tid(t:Maybe<&2,String>,p:Prog,+k:Nat) -> Result<&2,&2,Err.T,Leg>:
  match t:
    case None{}: Fail{Err.Mismatch{"a case scrutinee has no tid"}}
    case Some{+s}:
      do Result<&2,&2,Err.T,Leg>:
        ls : List<&2,Leg> <- sum_legs_p(p,s)
        branch_at(List.get(&2,Leg,ls,k),s,k)

def branch_leg(p:Prog,scrut:E.Repr,b:E.Branch<E.Ktm>) -> Result<&2,&2,Err.T,Leg>:
  match b:
    case E.Branch{k,n,body}: branch_tid(tid_text(scrut),p,k)

def branch_fields(ok:Bool,fs:List<&2,E.Repr>,k:Nat,n:Nat,count:Nat) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match ok:
    case True{}: Done{List.reverse(&2,E.Repr,fs)}
    case False{}: Fail{Err.Mismatch{"leg " ++ Nat.show(k) ++ " binds " ++ Nat.show(count) ++ " fields and the branch binds " ++ Nat.show(n)}}

def branch_binders(p:Prog,scrut:E.Repr,b:E.Branch<E.Ktm>) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match b:
    case E.Branch{k,0n,body}: Done{[]}
    case E.Branch{+k,1n+ +n,body}:
      do Result<&2,&2,Err.T,List<&2,E.Repr>>:
        l : Leg <- branch_tid(tid_text(scrut),p,k)
        +fs : List<&2,E.Repr> = leg_fields(l)
        +count : Nat = List.length(&2,E.Repr,fs)
        branch_fields(Nat.is_eq(count,1n+n),fs,k,1n+n,count)

type HeadKind is Data:
  HFun{name:String, fn:Fn}
  HPrim{name:String}
  HValue{}

def is_prim(+n:String) -> Bool:
  String.eq(n,"natAdd") || String.eq(n,"natSub") || String.eq(n,"natMul") || String.eq(n,"natEq") || String.eq(n,"natLt")

def prim_result(+n:String) -> E.Repr:
  choose(E.Repr,String.eq(n,"natEq") || String.eq(n,"natLt"),E.RUnion{E.Tid{"sum<unit|unit>"}},nat_repr())

def head_lookup(f:Maybe<&2,Fn>,+name:String) -> HeadKind:
  match f:
    case Some{x}: HFun{name,x}
    case None{}: choose(HeadKind,is_prim(name),HPrim{name},HValue{})

def head_kind(p:Prog,h:E.Ktm) -> HeadKind:
  match p h:
    case Prog{fs,ps,ms} E.KGlobal{+n}: head_lookup(lookup(Fn,fs,n),n)
    case _ _: HValue{}

def nullary_closure(r:E.Repr) -> Bool:
  match r:
    case E.RFunc{E.Tid{t}}: String.eq(t,fn_key(0n))
    case _: False{}

def steps_arity(+m:Nat,+k:Nat) -> E.Repr:
  choose(E.Repr,Nat.is_gt(m,k),E.RFunc{E.Tid{fn_key(Nat.sub(m,k))}},any_repr())

def steps_result(r:E.Repr,k:Nat) -> Result<&2,&2,Err.T,E.Repr>:
  match r k:
    case E.RFunc{E.Tid{+t}} 0n: Done{choose(E.Repr,String.eq(t,fn_key(0n)),any_repr(),E.RFunc{E.Tid{t}})}
    case _ 0n: Done{r}
    case E.RFunc{E.Tid{t}} 1n+k:
      do Result<&2,&2,Err.T,E.Repr>:
        m : Nat <- arity_of_fn(t)
        return steps_arity(m,1n+k)
    case E.RUnion{t} 1n+k: Done{any_repr()}
    case E.RThunk{t} 1n+k: Fail{Err.Not_yet{"KForce arrives at M2"}}
    case _ 1n+k: Fail{Err.Mismatch{"an application head is not a function"}}

def after_compare(c:Cmp,+m:Nat,res:E.Repr,+k:Nat) -> Result<&2,&2,Err.T,E.Repr>:
  match c:
    case EQ{}: Done{res}
    case GT{}: steps_result(res,Nat.sub(k,m))
    case LT{}: steps_result(E.RFunc{E.Tid{fn_key(m)}},k)

def after_head(+m:Nat,res:E.Repr,+k:Nat) -> Result<&2,&2,Err.T,E.Repr>:
  after_compare(Nat.cmp(k,m),m,res,k)

def global_value(post:Maybe<&2,E.Repr>,name:String) -> Result<&2,&2,Err.T,E.Repr>:
  match post:
    case None{}: Fail{Err.Unbound{"no runtime declaration for " ++ name}}
    case Some{r}: Fail{Err.Unbound{"axiom " ++ name ++ " has no body"}}

def global_head(h:HeadKind,p:Prog,+name:String) -> Result<&2,&2,Err.T,E.Repr>:
  match h p:
    case HFun{n,Fn{+ps,r,b}} _: Done{choose(E.Repr,Nat.is_eq(List.length(&2,E.Repr,ps),0n),r,E.RFunc{E.Tid{fn_key(List.length(&2,E.Repr,ps))}})}
    case HPrim{n} _: Done{E.RFunc{E.Tid{fn_key(2n)}}}
    case HValue{} Prog{fs,ps,ms}: global_value(lookup(E.Repr,ps,name),name)

def global_repr(+p:Prog,+name:String) -> Result<&2,&2,Err.T,E.Repr>:
  global_head(head_kind(p,E.KGlobal{name}),p,name)

type InferQuery is Data:
  Infer{term:E.Ktm,env:List<&2,E.Repr>}
  InferApp{kind:HeadKind,head:E.Ktm,args:List<&2,E.Ktm>,env:List<&2,E.Repr>}

@unsafe def infer_query(q:InferQuery,+p:Prog) -> Result<&2,&2,Err.T,E.Repr>:
  match q:
    case Infer{tm,+env}:
      match tm:
        case E.KVar{+i}: repr_at(List.get(&2,E.Repr,env,i),Err.Unbound{"KVar " ++ Nat.show(i) ++ " is out of scope"})
        case E.KLit{l}: Done{nat_repr()}
        case E.KGlobal{n}: global_repr(p,n)
        case E.KErased{}: Done{any_repr()}
        case E.KLet{x,v,b}:
          do Result<&2,&2,Err.T,E.Repr>:
            r : E.Repr <- infer_query(Infer{v,env},p)
            infer_query(Infer{b,r <> env},p)
        case E.KClos{f,n,cs}: Done{E.RFunc{E.Tid{fn_key(n)}}}
        case E.KApp{+h,args}: infer_query(InferApp{head_kind(p,h),h,args,env},p)
        case E.KTail{+h,args}: infer_query(InferApp{head_kind(p,h),h,args,env},p)
        case E.KStruct{t,fs}: Done{E.RStruct{t}}
        case E.KTag{t,k,ps}: Done{E.RUnion{t}}
        case E.KProj{E.Tid{+t},+k,x}:
          do Result<&2,&2,Err.T,E.Repr>:
            rs : List<&2,E.Repr> <- fields_of(t)
            repr_at(List.get(&2,E.Repr,rs,k),Err.Wrong_leg{"field " ++ Nat.show(k) ++ " of " ++ t})
        case E.KCase{t,s,Nil{}}: Done{any_repr()}
        case E.KCase{t,s,Con{E.Branch{+k,+n,+body},bs}}:
          do Result<&2,&2,Err.T,E.Repr>:
            bindings : List<&2,E.Repr> <- branch_binders(p,E.RUnion{t},E.Branch{k,n,body})
            infer_query(Infer{body,List.append(&2,E.Repr,bindings,env)},p)
        case E.KDelay{f,cs}: Fail{Err.Not_yet{"KDelay arrives at M2"}}
        case E.KForce{x}: Fail{Err.Not_yet{"KForce arrives at M2"}}
    case InferApp{h,head,args,env}:
      match h:
        case HFun{n,Fn{ps,r,b}}: after_head(List.length(&2,E.Repr,ps),r,List.length(&2,E.Ktm,args))
        case HPrim{n}: after_head(2n,prim_result(n),List.length(&2,E.Ktm,args))
        case HValue{}:
          do Result<&2,&2,Err.T,E.Repr>:
            r : E.Repr <- infer_query(Infer{head,env},p)
            steps_result(r,List.length(&2,E.Ktm,args))

def infer(p:Prog,env:List<&2,E.Repr>,tm:E.Ktm) -> Result<&2,&2,Err.T,E.Repr>:
  infer_query(Infer{tm,env},p)

def capture_found(f:Maybe<&2,Fn>,+name:String,+count:Nat) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match f:
    case None{}: Fail{Err.Unbound{"no function named " ++ name}}
    case Some{Fn{+ps,r,b}}:
      choose(Result<&2,&2,Err.T,List<&2,E.Repr>>,Nat.is_le(count,List.length(&2,E.Repr,ps)),Done{List.take(&2,E.Repr,ps,count)},Fail{Err.Mismatch{"invalid capture count for " ++ name}})

def capture_reprs(p:Prog,+name:String,count:Nat) -> Result<&2,&2,Err.T,List<&2,E.Repr>>:
  match p:
    case Prog{fs,ps,ms}: capture_found(lookup(Fn,fs,name),name,count)

type Tspec is Data:
  TSLimbs{}
  TSBigNat{}
  TSRuntime{params:List<&2,String>,result:String}
  TSClos{}
  TSFn{arity:Nat}
  TSStruct{fields:List<&2,E.Repr>}
  TSLeg{fields:List<&2,E.Repr>}
  TSPap{count:Nat}
  TSSig{params:List<&2,E.Repr>,result:E.Repr}
  TSApply{arity:Nat}
  TSEntry{}

type Fspec is Data:
  FSRuntime{name:String}
  FSProg{name:String}
  FSWrap{name:String,captures:Nat}
  FSApply{arity:Nat}
  FSPapw{arity:Nat,applied:Nat}
  FSEntry{}

type Keys is Data:
  Keys{types:List<&2,C.Pair<String,Tspec>>,funcs:List<&2,C.Pair<String,Fspec>>}

def no_keys() -> Keys:
  Keys{[],[]}

def merge(a:Keys,b:Keys) -> Keys:
  match a b:
    case Keys{at,af} Keys{bt,bf}: Keys{List.append(&2,C.Pair<String,Tspec>,at,bt),List.append(&2,C.Pair<String,Fspec>,af,bf)}

def merge_all(ks:List<&2,Keys>) -> Keys:
  match ks:
    case Nil{}: no_keys()
    case Con{k,rest}: merge(k,merge_all(rest))

def ty_key(k:String,s:Tspec) -> Keys:
  Keys{[C.Pair{k,s}],[]}

def fn_key_of(k:String,s:Fspec) -> Keys:
  Keys{[],[C.Pair{k,s}]}

def fnty_keys(+n:Nat) -> Keys:
  merge(ty_key("clos",TSClos{}),ty_key(fn_key(n),TSFn{n}))

def nat_data_keys() -> Keys:
  merge(ty_key("nat-limbs",TSLimbs{}),ty_key("nat-big",TSBigNat{}))

def runtime_key(+n:String,ps:List<&2,String>,r:String) -> Keys:
  merge(ty_key("nat-sig:" ++ n,TSRuntime{ps,r}),fn_key_of("nat-runtime:" ++ n,FSRuntime{n}))

def nat_runtime_keys() -> Keys:
  merge_all([nat_data_keys(),
    runtime_key("length",["n"],"i"),
    runtime_key("digit",["n","i"],"i"),
    runtime_key("copy",["a","a","i","i"],"a"),
    runtime_key("normal",["a","i"],"n"),
    runtime_key("compareDigits",["n","n","i"],"i"),
    runtime_key("compare",["n","n"],"i"),
    runtime_key("addLoop",["n","n","a","i","i","i"],"n"),
    runtime_key("subLoop",["n","n","a","i","i","i"],"n"),
    runtime_key("mulLoop",["n","n","a","i","i","i"],"n"),
    runtime_key("slowAdd",["n","n"],"n"),
    runtime_key("slowSub",["n","n"],"n"),
    runtime_key("slowMul",["n","n"],"n"),
    runtime_key("natAdd",["n","n"],"n"),
    runtime_key("natSub",["n","n"],"n"),
    runtime_key("natMul",["n","n"],"n"),
    runtime_key("natEq",["n","n"],"n"),
    runtime_key("natLt",["n","n"],"n")])

def repr_texts(rs:List<&2,E.Repr>) -> String:
  match rs:
    case Nil{}: ""
    case Con{r,Nil{}}: E.print_repr(r)
    case Con{r,rest}: E.print_repr(r) ++ "," ++ repr_texts(rest)

def leg_key(rs:List<&2,E.Repr>) -> String:
  "leg<" ++ repr_texts(rs) ++ ">"

def sig_key(ps:List<&2,E.Repr>,r:E.Repr) -> String:
  "sig<" ++ repr_texts(ps) ++ "->" ++ E.print_repr(r) ++ ">"

def env_tid(rs:List<&2,E.Repr>) -> String:
  "tuple<" ++ repr_texts(rs) ++ ">"

def wrap_fkey(n:String,c:Nat) -> String:
  "wrap:" ++ n ++ ":" ++ Nat.show(c)

def apply_fkey(n:Nat) -> String:
  "apply:" ++ Nat.show(n)

def apply_ty_key(n:Nat) -> String:
  "applyfn<" ++ Nat.show(n) ++ ">"

def pap_key(m:Nat,k:Nat) -> String:
  "pap<" ++ Nat.show(m) ++ "," ++ Nat.show(k) ++ ">"

def papw_fkey(m:Nat,k:Nat) -> String:
  "papw:" ++ Nat.show(m) ++ ":" ++ Nat.show(k)

def leg_type(l:Leg) -> Maybe<&2,C.Pair<String,List<&2,E.Repr>>>:
  match l:
    case LUnit{}: None{}
    case LRepr{+r}: Some{C.Pair{leg_key([r]),[r]}}
    case LMu{t,rs}: Some{C.Pair{t,rs}}

type Need is Data:
  NRepr{repr:E.Repr}
  NReprs{reprs:List<&2,E.Repr>}
  NStruct{tid:String}
  NSum{tid:String}
  NLegs{legs:List<&2,Leg>}
  NWrap{name:String,kind:HeadKind}
  NWalk{term:E.Ktm,env:List<&2,E.Repr>}
  NTerms{terms:List<&2,E.Ktm>,env:List<&2,E.Repr>}
  NBranches{branches:List<&2,E.Branch<E.Ktm>>,scrut:E.Repr,env:List<&2,E.Repr>}
  NApp{head:E.Ktm,args:List<&2,E.Ktm>,env:List<&2,E.Repr>}
  NApply{kind:HeadKind,head:E.Ktm,count:Nat,env:List<&2,E.Repr>}
  NSaturated{name:String,arity:Nat,result:E.Repr,count:Nat,saturated:Bool}
  NGlobal{name:String,kind:HeadKind}
  NFun{name:String,fn:Fn}
  NFuns{funs:List<&2,C.Pair<String,Fn>>}

def apply_keys(+k:Nat) -> Keys:
  merge_all([ty_key("clos",TSClos{}),ty_key(apply_ty_key(k),TSApply{k}),fn_key_of(apply_fkey(k),FSApply{k})])

def need_apply_positive(r:E.Repr,k:Nat) -> Result<&2,&2,Err.T,Keys>:
  match r:
    case E.RFunc{E.Tid{t}}:
      do Result<&2,&2,Err.T,Keys>:
        m : Nat <- arity_of_fn(t)
        return apply_keys(k)
    case E.RUnion{t}: Done{apply_keys(k)}
    case E.RThunk{t}: Fail{Err.Not_yet{"KForce arrives at M2"}}
    case _: Fail{Err.Mismatch{"an application head is not a function"}}

def need_apply(r:E.Repr,k:Nat) -> Result<&2,&2,Err.T,Keys>:
  match k:
    case 0n: Done{choose(Keys,nullary_closure(r),apply_keys(0n),no_keys())}
    case 1n+p: need_apply_positive(r,1n+p)

@unsafe def collect(q:Need,+p:Prog) -> Result<&2,&2,Err.T,Keys>:
  match q:
    case NRepr{r}:
      match r:
        case E.RI31{}: Done{no_keys()}
        case E.RUnion{E.Tid{t}}: Done{choose(Keys,String.eq(t,"nat"),nat_data_keys(),no_keys())}
        case E.RStruct{E.Tid{t}}: collect(NStruct{t},p)
        case E.RFunc{E.Tid{t}}:
          do Result<&2,&2,Err.T,Keys>:
            n : Nat <- arity_of_fn(t)
            return fnty_keys(n)
        case E.RThunk{t}: Fail{Err.Not_yet{"RThunk arrives at M2"}}
    case NReprs{rs}:
      match rs:
        case Nil{}: Done{no_keys()}
        case Con{r,rest}:
          do Result<&2,&2,Err.T,Keys>:
            a : Keys <- collect(NRepr{r},p)
            b : Keys <- collect(NReprs{rest},p)
            return merge(a,b)
    case NStruct{+t}:
      do Result<&2,&2,Err.T,Keys>:
        +rs : List<&2,E.Repr> <- fields_of(t)
        k : Keys <- collect(NReprs{rs},p)
        return merge(ty_key(t,TSStruct{rs}),k)
    case NSum{t}:
      do Result<&2,&2,Err.T,Keys>:
        ls : List<&2,Leg> <- sum_legs_p(p,t)
        collect(NLegs{ls},p)
    case NLegs{ls}:
      match ls:
        case Nil{}: Done{no_keys()}
        case Con{LUnit{},rest}: collect(NLegs{rest},p)
        case Con{LRepr{+r},rest}: collect(NLegs{LMu{leg_key([r]),[r]} <> rest},p)
        case Con{LMu{t,+rs},rest}:
          do Result<&2,&2,Err.T,Keys>:
            k : Keys <- collect(NReprs{rs},p)
            ks : Keys <- collect(NLegs{rest},p)
            return merge_all([k,ty_key(t,TSLeg{rs}),ks])
    case NWrap{+n,h}:
      match h:
        case HFun{x,Fn{+ps,+r,b}}:
          do Result<&2,&2,Err.T,Keys>:
            kp : Keys <- collect(NReprs{ps},p)
            kr : Keys <- collect(NRepr{r},p)
            return merge_all([fnty_keys(List.length(&2,E.Repr,ps)),fn_key_of(wrap_fkey(n,0n),FSWrap{n,0n}),kp,kr])
        case HPrim{x}: Done{merge_all([nat_runtime_keys(),fnty_keys(2n),fn_key_of(wrap_fkey(n,0n),FSWrap{n,0n})])}
        case HValue{}:
          do Result<&2,&2,Err.T,Keys>:
            r : E.Repr <- global_repr(p,n)
            return no_keys()
    case NGlobal{+n,h}:
      match h:
        case HFun{x,Fn{Nil{},r,b}}: Done{no_keys()}
        case _: collect(NWrap{n,h},p)
    case NWalk{tm,+env}:
      match tm:
        case E.KVar{i}: Done{no_keys()}
        case E.KLit{l}: Done{nat_data_keys()}
        case E.KErased{}: Done{no_keys()}
        case E.KGlobal{+n}: collect(NGlobal{n,head_kind(p,E.KGlobal{n})},p)
        case E.KLet{x,+v,b}:
          do Result<&2,&2,Err.T,Keys>:
            kv : Keys <- collect(NWalk{v,env},p)
            +r : E.Repr <- infer(p,env,v)
            kr : Keys <- collect(NRepr{r},p)
            kb : Keys <- collect(NWalk{b,r <> env},p)
            return merge_all([kv,kr,kb])
        case E.KClos{E.Fid{+f},n,+caps}:
          do Result<&2,&2,Err.T,Keys>:
            kc : Keys <- collect(NTerms{caps,env},p)
            +rs : List<&2,E.Repr> <- capture_reprs(p,f,List.length(&2,E.Ktm,caps))
            kr : Keys <- collect(NReprs{rs},p)
            ke : Keys <- collect(NReprs{choose(List<&2,E.Repr>,Nat.is_eq(List.length(&2,E.Repr,rs),0n),[],[E.RStruct{E.Tid{env_tid(rs)}}])},p)
            +count : Nat = List.length(&2,E.Ktm,caps)
            return merge_all([fnty_keys(n),fn_key_of(wrap_fkey(f,count),FSWrap{f,count}),ke,kc,kr])
        case E.KApp{h,args}: collect(NApp{h,args,env},p)
        case E.KTail{h,args}: collect(NApp{h,args,env},p)
        case E.KStruct{E.Tid{t},fs}:
          do Result<&2,&2,Err.T,Keys>:
            kt : Keys <- collect(NStruct{t},p)
            kf : Keys <- collect(NTerms{fs,env},p)
            return merge(kt,kf)
        case E.KProj{E.Tid{t},k,x}:
          do Result<&2,&2,Err.T,Keys>:
            kt : Keys <- collect(NStruct{t},p)
            kx : Keys <- collect(NWalk{x,env},p)
            return merge(kt,kx)
        case E.KTag{E.Tid{t},k,ps}:
          do Result<&2,&2,Err.T,Keys>:
            kt : Keys <- collect(NSum{t},p)
            kp : Keys <- collect(NTerms{ps,env},p)
            return merge(kt,kp)
        case E.KCase{+t,+s,+bs}:
          do Result<&2,&2,Err.T,Keys>:
            ks : Keys <- collect(NWalk{s,env},p)
            kt : Keys <- collect(choose(Need,Nat.is_eq(List.length(&2,E.Branch<E.Ktm>,bs),0n),NLegs{[]},NSum{E.tid_text(t)}),p)
            r : E.Repr <- infer(p,env,E.KCase{t,s,bs})
            kr : Keys <- collect(NRepr{r},p)
            kb : Keys <- collect(NBranches{bs,E.RUnion{t},env},p)
            return merge_all([ks,kt,kr,kb])
        case E.KDelay{f,cs}: Fail{Err.Not_yet{"KDelay arrives at M2"}}
        case E.KForce{x}: Fail{Err.Not_yet{"KForce arrives at M2"}}
    case NTerms{ts,+env}:
      match ts:
        case Nil{}: Done{no_keys()}
        case Con{t,rest}:
          do Result<&2,&2,Err.T,Keys>:
            k : Keys <- collect(NWalk{t,env},p)
            ks : Keys <- collect(NTerms{rest,env},p)
            return merge(k,ks)
    case NBranches{bs,+scrut,+env}:
      match bs:
        case Nil{}: Done{no_keys()}
        case Con{E.Branch{+k,+n,+body},rest}:
          do Result<&2,&2,Err.T,Keys>:
            +bindings : List<&2,E.Repr> <- branch_binders(p,scrut,E.Branch{k,n,body})
            kb : Keys <- collect(NWalk{body,List.append(&2,E.Repr,bindings,env)},p)
            kr : Keys <- collect(NReprs{bindings},p)
            ks : Keys <- collect(NBranches{rest,scrut,env},p)
            return merge_all([kb,kr,ks])
    case NApp{+h,+args,+env}:
      do Result<&2,&2,Err.T,Keys>:
        ka : Keys <- collect(NTerms{args,env},p)
        kh : Keys <- collect(NApply{head_kind(p,h),h,List.length(&2,E.Ktm,args),env},p)
        return merge(ka,kh)
    case NApply{kind,+head,k,+env}:
      match kind k:
        case HFun{n,f} 0n: collect(NWalk{head,env},p)
        case HPrim{n} 0n: collect(NWalk{head,env},p)
        case HValue{} _:
          do Result<&2,&2,Err.T,Keys>:
            kh : Keys <- collect(NWalk{head,env},p)
            r : E.Repr <- infer(p,env,head)
            ka : Keys <- need_apply(r,k)
            return merge(kh,ka)
        case HFun{n,Fn{ps,r,b}} 1n+ +k:
          +m = List.length(&2,E.Repr,ps)
          collect(NSaturated{n,m,r,1n+k,Nat.is_ge(1n+k,m)},p)
        case HPrim{+n} 1n+ +k:
          do Result<&2,&2,Err.T,Keys>:
            ks : Keys <- collect(NSaturated{n,2n,prim_result(n),1n+k,Nat.is_ge(1n+k,2n)},p)
            return merge(nat_runtime_keys(),ks)
    case NSaturated{+n,m,r,k,saturated}:
      match saturated:
        case True{}: need_apply(r,Nat.sub(k,m))
        case False{}:
          do Result<&2,&2,Err.T,Keys>:
            kw : Keys <- collect(NWrap{n,head_kind(p,E.KGlobal{n})},p)
            ka : Keys <- need_apply(E.RFunc{E.Tid{fn_key(m)}},k)
            return merge(kw,ka)
    case NFun{+n,Fn{+ps,+r,b}}:
      do Result<&2,&2,Err.T,Keys>:
        kp : Keys <- collect(NReprs{ps},p)
        kr : Keys <- collect(NRepr{r},p)
        kb : Keys <- collect(NWalk{b,List.reverse(&2,E.Repr,ps)},p)
        return merge_all([ty_key(sig_key(ps,r),TSSig{ps,r}),fn_key_of("fun:" ++ n,FSProg{n}),kp,kr,kb])
    case NFuns{fs}:
      match fs:
        case Nil{}: Done{no_keys()}
        case Con{C.Pair{n,f},rest}:
          do Result<&2,&2,Err.T,Keys>:
            k : Keys <- collect(NFun{n,f},p)
            ks : Keys <- collect(NFuns{rest},p)
            return merge(k,ks)

def first(-A:Data,-B:Data,p:C.Pair<A,B>) -> A:
  match p:
    case C.Pair{a,b}: a

def second(-A:Data,-B:Data,p:C.Pair<A,B>) -> B:
  match p:
    case C.Pair{a,b}: b

def keys_types(k:Keys) -> List<&2,C.Pair<String,Tspec>>:
  match k:
    case Keys{ts,fs}: ts

def keys_funcs(k:Keys) -> List<&2,C.Pair<String,Fspec>>:
  match k:
    case Keys{ts,fs}: fs

def prog_funcs(p:Prog) -> List<&2,C.Pair<String,Fn>>:
  match p:
    case Prog{fs,ps,ms}: fs

def prog_mus(p:Prog) -> List<&2,MuLeg>:
  match p:
    case Prog{fs,ps,ms}: ms

def contains_name(xs:List<&2,String>,+n:String) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{x,rest}: String.eq(x,n) || contains_name(rest,n)

def contains_nat(xs:List<&2,Nat>,+n:Nat) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{x,rest}: Nat.is_eq(x,n) || contains_nat(rest,n)

def dedup_rows(-A:Data,xs:List<&2,C.Pair<String,A>>,+seen:List<&2,String>) -> List<&2,C.Pair<String,A>>:
  match xs:
    case Nil{}: []
    case Con{C.Pair{+k,+v},rest}:
      +tail = dedup_rows(A,rest,k <> seen)
      choose(List<&2,C.Pair<String,A>>,contains_name(seen,k),tail,C.Pair{k,v} <> tail)

def dedup_nat(xs:List<&2,Nat>,+seen:List<&2,Nat>) -> List<&2,Nat>:
  match xs:
    case Nil{}: []
    case Con{+n,rest}:
      +tail = dedup_nat(rest,n <> seen)
      choose(List<&2,Nat>,contains_nat(seen,n),tail,n <> tail)

def arity_keys(ts:List<&2,C.Pair<String,Tspec>>) -> List<&2,Nat>:
  match ts:
    case Nil{}: []
    case Con{C.Pair{k,TSFn{n}},rest}: n <> arity_keys(rest)
    case Con{x,rest}: arity_keys(rest)

def call_keys(ts:List<&2,C.Pair<String,Tspec>>) -> List<&2,Nat>:
  match ts:
    case Nil{}: []
    case Con{C.Pair{k,TSApply{n}},rest}: n <> call_keys(rest)
    case Con{x,rest}: call_keys(rest)

def arity_diffs(ks:List<&2,Nat>,+m:Nat) -> List<&2,Nat>:
  match ks:
    case Nil{}: []
    case Con{+k,rest}:
      +tail = arity_diffs(rest,m)
      choose(List<&2,Nat>,Nat.is_gt(m,k),Nat.sub(m,k) <> tail,tail)

def call_diffs(ms:List<&2,Nat>,+k:Nat) -> List<&2,Nat>:
  match ms:
    case Nil{}: []
    case Con{+m,rest}:
      +tail = call_diffs(rest,k)
      choose(List<&2,Nat>,Nat.is_gt(k,m) && Nat.is_gt(m,0n),Nat.sub(k,m) <> tail,tail)

def all_arity_diffs(ms:List<&2,Nat>,+ks:List<&2,Nat>) -> List<&2,Nat>:
  match ms:
    case Nil{}: []
    case Con{m,rest}: List.append(&2,Nat,arity_diffs(ks,m),all_arity_diffs(rest,ks))

def all_call_diffs(ks:List<&2,Nat>,+ms:List<&2,Nat>) -> List<&2,Nat>:
  match ks:
    case Nil{}: []
    case Con{k,rest}: List.append(&2,Nat,call_diffs(ms,k),all_call_diffs(rest,ms))

def max_nat(xs:List<&2,Nat>) -> Nat:
  match xs:
    case Nil{}: 0n
    case Con{n,rest}: Nat.max(n,max_nat(rest))

@unsafe def close_arities(fuel:Nat,+a:List<&2,Nat>,+k:List<&2,Nat>) -> C.Pair<List<&2,Nat>,List<&2,Nat>>:
  match fuel:
    case 0n: C.Pair{a,k}
    case 1n+f:
      +a2 = dedup_nat(List.append(&2,Nat,a,all_arity_diffs(a,k)),[])
      +k2 = dedup_nat(List.append(&2,Nat,k,all_call_diffs(k,a)),[])
      close_arities(choose(Nat,Nat.is_eq(List.length(&2,Nat,a),List.length(&2,Nat,a2)) && Nat.is_eq(List.length(&2,Nat,k),List.length(&2,Nat,k2)),0n,f),a2,k2)

def helper_pap(ks:List<&2,Nat>,+m:Nat) -> Keys:
  match ks:
    case Nil{}: no_keys()
    case Con{+k,rest}:
      here = choose(Keys,Nat.is_gt(m,k),merge_all([
        ty_key(pap_key(m,k),TSPap{k}),
        fn_key_of(papw_fkey(m,k),FSPapw{m,k}),
        ty_key(fn_key(Nat.sub(m,k)),TSFn{Nat.sub(m,k)})]),no_keys())
      merge(here,helper_pap(rest,m))

def helper_arities(ms:List<&2,Nat>,+ks:List<&2,Nat>) -> Keys:
  match ms:
    case Nil{}: no_keys()
    case Con{+m,rest}: merge_all([ty_key(fn_key(m),TSFn{m}),helper_pap(ks,m),helper_arities(rest,ks)])

def helper_calls(ks:List<&2,Nat>) -> Keys:
  match ks:
    case Nil{}: no_keys()
    case Con{+k,rest}: merge_all([ty_key(apply_ty_key(k),TSApply{k}),fn_key_of(apply_fkey(k),FSApply{k}),helper_calls(rest)])

def helper_keys(a:List<&2,Nat>,+k:List<&2,Nat>) -> Keys:
  merge(helper_calls(k),helper_arities(a,k))

def trank(t:Tspec) -> Nat:
  match t:
    case TSLimbs{}: 0n
    case TSBigNat{}: 1n
    case TSClos{}: 2n
    case TSFn{n}: 3n
    case TSStruct{rs}: 4n
    case TSLeg{rs}: 5n
    case TSPap{k}: 6n
    case TSRuntime{ps,r}: 7n
    case TSSig{ps,r}: 7n
    case TSApply{k}: 8n
    case TSEntry{}: 9n

def frank(f:Fspec) -> Nat:
  match f:
    case FSProg{n}: 0n
    case FSWrap{n,c}: 1n
    case FSApply{k}: 2n
    case FSPapw{m,k}: 3n
    case FSRuntime{n}: 4n
    case FSEntry{}: 4n

def text_depth(cs:List<&2,Char>) -> Nat:
  match cs:
    case Nil{}: 0n
    case Con{'<',rest}: 1n+text_depth(rest)
    case Con{c,rest}: text_depth(rest)

def type_before(a:C.Pair<String,Tspec>,b:C.Pair<String,Tspec>) -> Bool:
  match a b:
    case C.Pair{ka,sa} C.Pair{kb,sb}:
      +ra = trank(sa)
      +rb = trank(sb)
      Nat.is_lt(ra,rb) || (Nat.is_eq(ra,rb) && Nat.is_le(text_depth(String.to_list(ka)),text_depth(String.to_list(kb))))

def insert_type(xs:List<&2,C.Pair<String,Tspec>>,+x:C.Pair<String,Tspec>) -> List<&2,C.Pair<String,Tspec>>:
  match xs:
    case Nil{}: [x]
    case Con{+y,+rest}: choose(List<&2,C.Pair<String,Tspec>>,type_before(x,y),x <> y <> rest,y <> insert_type(rest,x))

def order_types(xs:List<&2,C.Pair<String,Tspec>>) -> List<&2,C.Pair<String,Tspec>>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: insert_type(order_types(rest),x)

def insert_func(xs:List<&2,C.Pair<String,Fspec>>,+x:C.Pair<String,Fspec>) -> List<&2,C.Pair<String,Fspec>>:
  match xs:
    case Nil{}: [x]
    case Con{C.Pair{+kn,+yn},+rest}:
      C.Pair{k,+xn} = x
      choose(List<&2,C.Pair<String,Fspec>>,Nat.is_le(frank(xn),frank(yn)),x <> C.Pair{kn,yn} <> rest,C.Pair{kn,yn} <> insert_func(rest,x))

def order_funcs(xs:List<&2,C.Pair<String,Fspec>>) -> List<&2,C.Pair<String,Fspec>>:
  match xs:
    case Nil{}: []
    case Con{x,rest}: insert_func(order_funcs(rest),x)

def family_names(ms:List<&2,MuLeg>) -> List<&2,String>:
  match ms:
    case Nil{}: []
    case Con{MuLeg{t,f,k,rs},rest}: f <> family_names(rest)

def edges_fields(rs:List<&2,E.Repr>,+fam:String,+families:List<&2,String>) -> List<&2,C.Pair<String,String>>:
  match rs:
    case Nil{}: []
    case Con{E.RUnion{E.Tid{+g}},rest}:
      +tail = edges_fields(rest,fam,families)
      choose(List<&2,C.Pair<String,String>>,contains_name(families,g),C.Pair{fam,g} <> tail,tail)
    case Con{r,rest}: edges_fields(rest,fam,families)

def mu_edges(ms:List<&2,MuLeg>,+families:List<&2,String>) -> List<&2,C.Pair<String,String>>:
  match ms:
    case Nil{}: []
    case Con{MuLeg{t,f,k,rs},rest}: List.append(&2,C.Pair<String,String>,edges_fields(rs,f,families),mu_edges(rest,families))

def edge_targets(es:List<&2,C.Pair<String,String>>,+name:String) -> List<&2,String>:
  match es:
    case Nil{}: []
    case Con{C.Pair{a,b},rest}:
      +tail = edge_targets(rest,name)
      choose(List<&2,String>,String.eq(a,name),b <> tail,tail)

@unsafe def reach_go(todo:List<&2,String>,+seen:List<&2,String>,+es:List<&2,C.Pair<String,String>>) -> List<&2,String>:
  match todo:
    case Nil{}: seen
    case Con{+n,+rest}:
      +hit = contains_name(seen,n)
      reach_go(choose(List<&2,String>,hit,rest,List.append(&2,String,edge_targets(es,n),rest)),choose(List<&2,String>,hit,seen,n <> seen),es)

def reach(+es:List<&2,C.Pair<String,String>>,f:String) -> List<&2,String>:
  reach_go(edge_targets(es,f),[],es)

def min_compare(c:Cmp,a:String,b:String) -> String:
  match c:
    case GT{}: b
    case _: a

def min_result(pair:(String & String) & Cmp) -> String:
  ((aa,bb),c) = pair
  min_compare(c,aa,bb)

def string_min(a:String,b:String) -> String:
  min_result(String.cmp(a,b))

def component_min(fams:List<&2,String>,+es:List<&2,C.Pair<String,String>>,+f:String,+best:String) -> String:
  match fams:
    case Nil{}: best
    case Con{+g,rest}:
      mutual = contains_name(reach(es,f),g) && contains_name(reach(es,g),f)
      component_min(rest,es,f,choose(String,mutual,string_min(g,best),best))

def group_mu(m:Maybe<&2,MuLeg>,key:String,+fams:List<&2,String>,+es:List<&2,C.Pair<String,String>>) -> String:
  match m:
    case None{}: key
    case Some{MuLeg{t,+f,k,rs}}:
      choose(String,contains_name(reach(es,f),f),"group:" ++ component_min(fams,es,f,f),key)

def group_key(+key:String,fams:List<&2,String>,es:List<&2,C.Pair<String,String>>) -> String:
  group_mu(parse_mu_leg(key),key,fams,es)

def take_group(xs:List<&2,C.Pair<String,Tspec>>,+g:String,+fams:List<&2,String>,+es:List<&2,C.Pair<String,String>>) -> C.Pair<List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>>:
  match xs:
    case Nil{}: C.Pair{[],[]}
    case Con{C.Pair{+k,+s},rest}:
      +parts = take_group(rest,g,fams,es)
      +yes = first(List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>,parts)
      +no = second(List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>,parts)
      choose(C.Pair<List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>>,String.eq(group_key(k,fams,es),g),C.Pair{C.Pair{k,s} <> yes,no},C.Pair{yes,C.Pair{k,s} <> no})

@unsafe def group_types(xs:List<&2,C.Pair<String,Tspec>>,+fams:List<&2,String>,+es:List<&2,C.Pair<String,String>>) -> List<&2,List<&2,C.Pair<String,Tspec>>>:
  match xs:
    case Nil{}: []
    case Con{C.Pair{+k,s},rest}:
      +parts = take_group(rest,group_key(k,fams,es),fams,es)
      group = first(List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>,parts)
      other = second(List<&2,C.Pair<String,Tspec>>,List<&2,C.Pair<String,Tspec>>,parts)
      (C.Pair{k,s} <> group) <> group_types(other,fams,es)

def flatten_types(gs:List<&2,List<&2,C.Pair<String,Tspec>>>) -> List<&2,C.Pair<String,Tspec>>:
  match gs:
    case Nil{}: []
    case Con{g,rest}: List.append(&2,C.Pair<String,Tspec>,g,flatten_types(rest))

def indices(-A:Data,xs:List<&2,C.Pair<String,A>>,+index:Nat) -> List<&2,C.Pair<String,Nat>>:
  match xs:
    case Nil{}: []
    case Con{C.Pair{k,v},rest}: C.Pair{k,index} <> indices(A,rest,1n+index)

def declared(fs:List<&2,C.Pair<String,Fspec>>,+index:Nat) -> List<&2,U32>:
  match fs:
    case Nil{}: []
    case Con{C.Pair{k,s},rest}:
      match s:
        case FSWrap{n,c}: U32.from_nat(index) <> declared(rest,1n+index)
        case FSPapw{m,k}: U32.from_nat(index) <> declared(rest,1n+index)
        case _: declared(rest,1n+index)

type T is Data:
  Linked{prog:Prog,types:List<&2,C.Pair<String,Tspec>>,groups:List<&2,List<&2,C.Pair<String,Tspec>>>,tmap:List<&2,C.Pair<String,Nat>>,funcs:List<&2,C.Pair<String,Fspec>>,fmap:List<&2,C.Pair<String,Nat>>,arities:List<&2,Nat>,calls:List<&2,Nat>,declared:List<&2,U32>}

def finish_layout(+p:Prog,base:Keys) -> T:
  match base:
    case Keys{bt,bf}:
      +tys0 = dedup_rows(Tspec,bt,[])
      +fns0 = dedup_rows(Fspec,bf,[])
      +ar = dedup_nat(arity_keys(tys0),[])
      +ca = dedup_nat(call_keys(tys0),[])
      +closed = close_arities(1n+Nat.add(max_nat(ar),max_nat(ca)),ar,ca)
      +a = first(List<&2,Nat>,List<&2,Nat>,closed)
      +k = second(List<&2,Nat>,List<&2,Nat>,closed)
      +extra = helper_keys(a,k)
      et = keys_types(extra)
      ef = keys_funcs(extra)
      ordered = order_types(dedup_rows(Tspec,List.append(&2,C.Pair<String,Tspec>,tys0,et),[]))
      +mus = prog_mus(p)
      +families = family_names(mus)
      +groups = group_types(ordered,families,mu_edges(mus,families))
      +ts = flatten_types(groups)
      +funcs = order_funcs(dedup_rows(Fspec,List.append(&2,C.Pair<String,Fspec>,fns0,ef),[]))
      Linked{p,ts,groups,indices(Tspec,ts,0n),funcs,indices(Fspec,funcs,0n),a,k,declared(funcs,0n)}

def build(rows:List<&2,C.Pair<String,Erase.Entry>>) -> Result<&2,&2,Err.T,T>:
  +p = program_table(rows)
  fs = prog_funcs(p)
  do Result<&2,&2,Err.T,T>:
    k : Keys <- collect(NFuns{fs},p)
    return finish_layout(p,merge_all([k,ty_key("entryfn",TSEntry{}),fn_key_of("entry",FSEntry{})]))

def index_result(found:Maybe<&2,Nat>,msg:String) -> Result<&2,&2,Err.T,U32>:
  match found:
    case Some{i}: Done{U32.from_nat(i)}
    case None{}: Fail{Err.Unbound{msg}}

def type_index(l:T,+key:String) -> Result<&2,&2,Err.T,U32>:
  match l:
    case Linked{p,ts,gs,tm,fs,fm,a,k,d}: index_result(lookup(Nat,tm,key),"no type index for " ++ key)

def func_index(l:T,+key:String) -> Result<&2,&2,Err.T,U32>:
  match l:
    case Linked{p,ts,gs,tm,fs,fm,a,k,d}: index_result(lookup(Nat,fm,key),"no function index for " ++ key)

def valtype_of(l:T,r:E.Repr) -> Result<&2,&2,Err.T,G.Valtype>:
  match r:
    case E.RI31{}: Done{G.Ref{G.HI31{}}}
    case E.RUnion{t}: Done{G.Ref{G.HEq{}}}
    case E.RStruct{E.Tid{t}}:
      do Result<&2,&2,Err.T,G.Valtype>:
        i : U32 <- type_index(l,t)
        return G.Ref{G.HType{i}}
    case E.RFunc{t}:
      do Result<&2,&2,Err.T,G.Valtype>:
        i : U32 <- type_index(l,"clos")
        return G.Ref{G.HType{i}}
    case E.RThunk{t}: Fail{Err.Not_yet{"RThunk arrives at M2"}}

def valtypes_of(rs:List<&2,E.Repr>,+l:T) -> Result<&2,&2,Err.T,List<&2,G.Valtype>>:
  match rs:
    case Nil{}: Done{[]}
    case Con{r,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Valtype>>:
        v : G.Valtype <- valtype_of(l,r)
        vs : List<&2,G.Valtype> <- valtypes_of(rest,l)
        return v <> vs

def eqs(n:Nat) -> List<&2,G.Valtype>:
  match n:
    case 0n: []
    case 1n+p: G.Ref{G.HEq{}} <> eqs(p)

def runtime_vtype(other:Bool,l:T,s:String) -> Result<&2,&2,Err.T,G.Valtype>:
  match other:
    case True{}: Fail{Err.Mismatch{"unknown natural runtime value type: " ++ s}}
    case False{}:
      do Result<&2,&2,Err.T,G.Valtype>:
        i : U32 <- type_index(l,"nat-limbs")
        return G.Ref{G.HType{i}}

def runtime_valtype(l:T,+s:String) -> Result<&2,&2,Err.T,G.Valtype>:
  choose(Result<&2,&2,Err.T,G.Valtype>,String.eq(s,"i"),Done{G.I32{}},
    choose(Result<&2,&2,Err.T,G.Valtype>,String.eq(s,"n"),Done{G.Ref{G.HEq{}}},
      runtime_vtype(Bool.not(String.eq(s,"a")),l,s)))

def runtime_vtypes(ss:List<&2,String>,+l:T) -> Result<&2,&2,Err.T,List<&2,G.Valtype>>:
  match ss:
    case Nil{}: Done{[]}
    case Con{s,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Valtype>>:
        v : G.Valtype <- runtime_valtype(l,s)
        vs : List<&2,G.Valtype> <- runtime_vtypes(rest,l)
        return v <> vs

def comptype_of(s:Tspec,+l:T) -> Result<&2,&2,Err.T,G.Comptype>:
  match s:
    case TSLimbs{}: Done{G.CArray{G.I32{}}}
    case TSBigNat{}:
      do Result<&2,&2,Err.T,G.Comptype>:
        i : U32 <- type_index(l,"nat-limbs")
        return G.CStruct{[G.I32{},G.Ref{G.HType{i}}]}
    case TSRuntime{ps,r}:
      do Result<&2,&2,Err.T,G.Comptype>:
        vs : List<&2,G.Valtype> <- runtime_vtypes(ps,l)
        v : G.Valtype <- runtime_valtype(l,r)
        return G.CFunc{vs,[v]}
    case TSClos{}: Done{G.CStruct{[G.I32{},G.Ref{G.HFunc{}},G.Ref{G.HEq{}}]}}
    case TSFn{n}: Done{G.CFunc{eqs(1n+n),[G.Ref{G.HEq{}}]}}
    case TSStruct{rs}: Done{G.CStruct{eqs(List.length(&2,E.Repr,rs))}}
    case TSLeg{rs}: Done{G.CStruct{G.Ref{G.HI31{}} <> eqs(List.length(&2,E.Repr,rs))}}
    case TSPap{k}: Done{G.CStruct{eqs(1n+k)}}
    case TSSig{ps,r}:
      do Result<&2,&2,Err.T,G.Comptype>:
        vs : List<&2,G.Valtype> <- valtypes_of(ps,l)
        v : G.Valtype <- valtype_of(l,r)
        return G.CFunc{vs,[v]}
    case TSApply{k}: Done{G.CFunc{eqs(1n+k),[G.Ref{G.HEq{}}]}}
    case TSEntry{}: Done{G.CFunc{[],[G.I32{}]}}

def comptypes(xs:List<&2,C.Pair<String,Tspec>>,+l:T) -> Result<&2,&2,Err.T,List<&2,G.Comptype>>:
  match xs:
    case Nil{}: Done{[]}
    case Con{C.Pair{k,s},rest}:
      do Result<&2,&2,Err.T,List<&2,G.Comptype>>:
        c : G.Comptype <- comptype_of(s,l)
        cs : List<&2,G.Comptype> <- comptypes(rest,l)
        return c <> cs

def comptypes_groups(gs:List<&2,List<&2,C.Pair<String,Tspec>>>,+l:T) -> Result<&2,&2,Err.T,List<&2,List<&2,G.Comptype>>>:
  match gs:
    case Nil{}: Done{[]}
    case Con{g,rest}:
      do Result<&2,&2,Err.T,List<&2,List<&2,G.Comptype>>>:
        cs : List<&2,G.Comptype> <- comptypes(g,l)
        rest : List<&2,List<&2,G.Comptype>> <- comptypes_groups(rest,l)
        return cs <> rest

def comptype_groups(l:T) -> Result<&2,&2,Err.T,List<&2,List<&2,G.Comptype>>>:
  match l:
    case Linked{+p,+ts,+gs,+tm,+fs,+fm,+a,+k,+d}: comptypes_groups(gs,Linked{p,ts,gs,tm,fs,fm,a,k,d})
