import Base
import ./common.bend as C
import ./term.bend as T
import ./shape.bend as S
import ./quantity.bend as Q
import ./bignum.bend as B

def word(head: String) -> String:
  "circuit fragment arrives at V5: " ++ head

def cost_case(yes: Bool) -> Maybe<&2, B.T>:
  match yes:
    case True{}: Some{B.one()}
    case False{}: None{}

def cost(+name: String) -> Maybe<&2, B.T>:
  cost_case(String.eq(name, "natAdd") || String.eq(name, "natSub") || String.eq(name, "natMul") || String.eq(name, "natEq") || String.eq(name, "natLt"))

def choose(-A: Data, yes: Bool, a: A, b: A) -> A:
  match yes:
    case True{}: a
    case False{}: b

def max(+a: B.T, +b: B.T) -> B.T:
  choose(B.T, B.le(a,b), b,a)

def bound_result(ok: Bool, n: B.T) -> Result<&2,&2,String,B.T>:
  match ok:
    case True{}: Done{n}
    case False{}: Fail{"unbounded iteration"}

def bounded(+n: B.T) -> Result<&2,&2,String,B.T>:
  # OCaml's positive machine integer ceiling is 2^62 - 1.
  bound_result(B.le(n, B.Big{False{}, [32767,32767,32767,32767,3]}), n)

def lookup_hit(hit: Bool, value: T.T, rest: Maybe<&2,T.T>) -> Maybe<&2,T.T>:
  match hit:
    case True{}: Some{value}
    case False{}: rest

def lookup(rows: List<&2,C.Pair<String,T.T>>, +name: String) -> Maybe<&2,T.T>:
  match rows:
    case Nil{}: None{}
    case Con{C.Pair{key,value},rest}: lookup_hit(String.eq(key,name),value,lookup(rest,name))

def member(xs: List<&2,String>, +name: String) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{x,rest}: String.eq(x,name) || member(rest,name)

type Value is Data:
  VD{depth: B.T}
  VClo{leg: T.Leg<T.T>, env: List<&2,Value>, seen: List<&2,String>}

def vd_of(v: Value) -> B.T:
  match v:
    case VD{n}: n
    case VClo{leg,env,seen}: B.zero()

def nth_default(v: Maybe<&2,Value>) -> Value:
  match v:
    case None{}: VD{B.zero()}
    case Some{x}: x

def push(n: Nat, +v: Value, env: List<&2,Value>) -> List<&2,Value>:
  match n:
    case 0n: env
    case 1n+p: v <> push(p,v,env)

def shape_error(s: S.T<T.T>) -> Maybe<&2,String>:
  match s:
    case S.SPi{q,n,d}: None{}
    case S.SArg{q,n}: None{}
    case S.SColl{n}: None{}
    case S.SMu{n,args}: None{}
    case S.SPar{a,b}: Some{"SPar"}
    case S.SNu{n,args}: Some{"SNu"}
    case S.SZk{q,n,d}: Some{"SZk"}
    case S.SFhc{l}: Some{"SFhc"}
    case S.SMpc{p,a}: Some{"SMpc"}

def type_result(e: Maybe<&2,String>) -> Result<&2,&2,String,Value>:
  match e:
    case None{}: Done{VD{B.zero()}}
    case Some{s}: Fail{s}

type BoundAnswer is Data:
  BoundAnswer{value:B.T, memo:List<&2,C.Pair<String,B.T>>}

def memo_lookup(rows:List<&2,C.Pair<String,B.T>>,+name:String) -> Maybe<&2,B.T>:
  match rows:
    case Nil{}: None{}
    case Con{C.Pair{k,v},rest}: choose(Maybe<&2,B.T>,String.eq(k,name),Some{v},memo_lookup(rest,name))

def bound_value(a:BoundAnswer) -> B.T:
  match a:
    case BoundAnswer{v,m}: v

def bound_memo(a:BoundAnswer) -> List<&2,C.Pair<String,B.T>>:
  match a:
    case BoundAnswer{v,m}: m

type BoundTask is Data:
  BTerm{term:T.T, seen:List<&2,String>}
  BArgs{args:List<&2,T.T>, seen:List<&2,String>}
  BGlobal{cycle:Bool, cached:Maybe<&2,B.T>, body:Maybe<&2,T.T>, name:String, seen:List<&2,String>}

@unsafe def literal_bound_go(task: BoundTask, +globals: List<&2,C.Pair<String,T.T>>, +family: String, +memo:List<&2,C.Pair<String,B.T>>) -> Result<&2,&2,String,BoundAnswer>:
  match task:
    case BTerm{t,+seen}:
      match t:
        case T.Ann{tm,ty}: literal_bound_go(BTerm{tm,seen},globals,family,memo)
        case T.Lit{l}: Done{BoundAnswer{B.zero(),memo}}
        case T.Global{+name}:
          literal_bound_go(BGlobal{member(seen,name),memo_lookup(memo,name),lookup(globals,name),name,name <> seen},globals,family,memo)
        case T.In{S.SMu{name,args},a,fields}:
          do Result<&2,&2,String,BoundAnswer>:
            +answer : BoundAnswer <- literal_bound_go(BArgs{fields,seen},globals,family,memo)
            return BoundAnswer{B.add(bound_value(answer),choose(B.T,String.eq(name,family),B.one(),B.zero())),bound_memo(answer)}
        case _: Fail{"unbounded iteration"}
    case BArgs{args,+seen}:
      match args:
        case Nil{}: Done{BoundAnswer{B.zero(),memo}}
        case Con{t,rest}:
          do Result<&2,&2,String,BoundAnswer>:
            +a : BoundAnswer <- literal_bound_go(BTerm{t,seen},globals,family,memo)
            +b : BoundAnswer <- literal_bound_go(BArgs{rest,seen},globals,family,bound_memo(a))
            return BoundAnswer{max(bound_value(a),bound_value(b)),bound_memo(b)}
    case BGlobal{cycle,cached,body,+name,seen}:
      match cycle cached body:
        case True{} _ _: Fail{"unbounded iteration"}
        case False{} Some{n} _: Done{BoundAnswer{n,memo}}
        case False{} None{} None{}: Fail{"unbounded iteration"}
        case False{} None{} Some{t}:
          do Result<&2,&2,String,BoundAnswer>:
            +a : BoundAnswer <- literal_bound_go(BTerm{t,seen},globals,family,memo)
            +v : B.T = bound_value(a)
            return BoundAnswer{v,C.Pair{name,v} <> bound_memo(a)}

def literal_bound(task:BoundTask,globals:List<&2,C.Pair<String,T.T>>,family:String) -> Result<&2,&2,String,B.T>:
  do Result<&2,&2,String,B.T>:
    a : BoundAnswer <- literal_bound_go(task,globals,family,[])
    return bound_value(a)


type Occur is Data:
  Occur{depth:Nat, term:T.T}

def addr_terms(a:T.Addr<T.T>, rest:List<&2,T.T>) -> List<&2,T.T>:
  match a:
    case T.APt{q,t}: t <> rest
    case T.ALeg{i}: rest
    case T.ACtor{n}: rest

def addr_occurs(a:T.Addr<T.T>, depth:Nat, rest:List<&2,Occur>) -> List<&2,Occur>:
  match a:
    case T.APt{q,t}: Occur{depth,t} <> rest
    case T.ALeg{i}: rest
    case T.ACtor{n}: rest

def terms_occurs(ts:List<&2,T.T>, +depth:Nat, rest:List<&2,Occur>) -> List<&2,Occur>:
  match ts:
    case Nil{}: rest
    case Con{t,ts}: Occur{depth,t} <> terms_occurs(ts,depth,rest)

def legs_occurs(ls:List<&2,T.Leg<T.T>>, +depth:Nat, rest:List<&2,Occur>) -> List<&2,Occur>:
  match ls:
    case Nil{}: rest
    case Con{T.Leg{bs,t},ls}: Occur{Nat.add(depth,List.length(&2,C.Pair<Q.T,String>,bs)),t} <> legs_occurs(ls,depth,rest)

def branches_occurs(bs:List<&2,C.Pair<T.Addr<T.T>,T.Leg<T.T>>>, +depth:Nat, rest:List<&2,Occur>) -> List<&2,Occur>:
  match bs:
    case Nil{}: rest
    case Con{C.Pair{a,T.Leg{bindings,t}},bs}:
      addr_occurs(a,depth,Occur{Nat.add(depth,List.length(&2,C.Pair<Q.T,String>,bindings)),t} <> branches_occurs(bs,depth,rest))

@unsafe def reads_fields(tasks:List<&2,Occur>, +count:Nat) -> Bool:
  match tasks:
    case Nil{}: False{}
    case Con{Occur{+d,t},rest}:
      match t:
        case T.Var{+i}: (Nat.is_ge(i,d) && Nat.is_lt(i,Nat.add(d,count))) || reads_fields(rest,count)
        case T.In{s,a,args}: reads_fields(addr_occurs(a,d,terms_occurs(args,d,rest)),count)
        case T.Elim{T.ElimData{s,scrut,q,m,bs}}: reads_fields(Occur{d,scrut} <> branches_occurs(bs,d,rest),count)
        case T.Sec{s,legs}: reads_fields(legs_occurs(legs,d,rest),count)
        case T.Out{s,a,h}: reads_fields(addr_occurs(a,d,Occur{d,h} <> rest),count)
        case T.Let{n,ty,v,b}: reads_fields(Occur{d,v} <> Occur{1n+d,b} <> rest,count)
        case T.Ann{tm,ty}: reads_fields(Occur{d,tm} <> rest,count)
        case _: reads_fields(rest,count)

def branches_read(bs:List<&2,C.Pair<T.Addr<T.T>,T.Leg<T.T>>>) -> Bool:
  match bs:
    case Nil{}: False{}
    case Con{C.Pair{a,T.Leg{bindings,t}},rest}: reads_fields([Occur{0n,t}],List.length(&2,C.Pair<Q.T,String>,bindings)) || branches_read(rest)

def spine(t:T.T, args:List<&2,T.T>) -> C.Pair<T.T,List<&2,T.T>>:
  match t:
    case T.Out{S.SPi{q,n,d},T.APt{aq,arg},head}: spine(head,arg <> args)
    case T.Out{S.SPi{q,n,d},a,head}: spine(head,args)
    case T.Out{S.SArg{q,n},T.APt{aq,arg},head}: spine(head,arg <> args)
    case T.Out{S.SArg{q,n},a,head}: spine(head,args)
    case _: C.Pair{t,args}

def shape_terms(s:S.T<T.T>, rest:List<&2,T.T>) -> List<&2,T.T>:
  match s:
    case S.SPi{q,n,d}: d <> rest
    case S.SArg{q,n}: rest
    case S.SColl{n}: rest
    case S.SPar{a,b}: a <> b <> rest
    case S.SMu{n,args}: T.Global{n} <> List.append(&2,T.T,args,rest)
    case S.SNu{n,args}: T.Global{n} <> List.append(&2,T.T,args,rest)
    case S.SZk{q,n,d}: d <> rest
    case S.SFhc{l}: l <> rest
    case S.SMpc{p,a}: p <> a <> rest

def legs_terms(ls:List<&2,T.Leg<T.T>>, rest:List<&2,T.T>) -> List<&2,T.T>:
  match ls:
    case Nil{}: rest
    case Con{T.Leg{bs,t},ls}: t <> legs_terms(ls,rest)

def branches_terms(bs:List<&2,C.Pair<T.Addr<T.T>,T.Leg<T.T>>>, rest:List<&2,T.T>) -> List<&2,T.T>:
  match bs:
    case Nil{}: rest
    case Con{C.Pair{a,T.Leg{bindings,t}},bs}: addr_terms(a,t <> branches_terms(bs,rest))

def motive_terms(m:Maybe<&2,T.Motive<T.T>>, rest:List<&2,T.T>) -> List<&2,T.T>:
  match m:
    case None{}: rest
    case Some{T.Motive{ind,indices,self,t}}: t <> rest

@unsafe def names_family(ts:List<&2,T.T>, +fam:String) -> Bool:
  match ts:
    case Nil{}: False{}
    case Con{t,rest}:
      match t:
        case T.Global{n}: String.eq(n,fam) || names_family(rest,fam)
        case T.Lan{s,d}: names_family(shape_terms(s,d <> rest),fam)
        case T.Ran{s,d}: names_family(shape_terms(s,d <> rest),fam)
        case T.In{s,a,args}: names_family(shape_terms(s,addr_terms(a,List.append(&2,T.T,args,rest))),fam)
        case T.Sec{s,ls}: names_family(shape_terms(s,legs_terms(ls,rest)),fam)
        case T.Out{s,a,h}: names_family(shape_terms(s,addr_terms(a,h <> rest)),fam)
        case T.Elim{T.ElimData{s,scrut,q,m,bs}}: names_family(shape_terms(s,scrut <> motive_terms(m,branches_terms(bs,rest))),fam)
        case T.Let{n,ty,v,b}: names_family(ty <> v <> b <> rest,fam)
        case T.Ann{tm,ty}: names_family(tm <> ty <> rest,fam)
        case _: names_family(rest,fam)

def certificate_result(cert:Result<&2,&2,String,B.T>, recursive:Bool, v:Value) -> Result<&2,&2,String,Value>:
  match cert recursive:
    case Done{n} _: Done{v}
    case Fail{e} True{}: Fail{"mu"}
    case Fail{e} False{}: Done{v}

type Query is Data:
  Eval{term:T.T, env:List<&2,Value>, seen:List<&2,String>}
  Args{args:List<&2,T.T>, env:List<&2,Value>, seen:List<&2,String>}
  Legs{legs:List<&2,T.Leg<T.T>>, env:List<&2,Value>, seen:List<&2,String>}
  Branches{branches:List<&2,C.Pair<T.Addr<T.T>,T.Leg<T.T>>>, env:List<&2,Value>, seen:List<&2,String>}
  Address{addr:T.Addr<T.T>, env:List<&2,Value>, seen:List<&2,String>}
  Apply{value:Value, args:List<&2,T.T>, env:List<&2,Value>, seen:List<&2,String>}
  CApp{spine:C.Pair<T.T,List<&2,T.T>>, env:List<&2,Value>, seen:List<&2,String>}
  Global{cycle:Bool, name:String, body:Maybe<&2,T.T>, seen:List<&2,String>}
  Primitive{cost:Maybe<&2,B.T>, head:T.T, args:List<&2,T.T>, env:List<&2,Value>, seen:List<&2,String>}
  Unwrap{value:Value}

def global_query(c:Maybe<&2,B.T>, +name:String, +seen:List<&2,String>, body:Maybe<&2,T.T>) -> Query:
  match c:
    case Some{n}: Unwrap{VD{n}}
    case None{}: Global{member(seen,name),name,body,name <> seen}

def mu_depth(read:Bool, n:B.T, best:B.T) -> B.T:
  match read:
    case True{}: B.mul(n,best)
    case False{}: best

@unsafe def eval(query:Query, +globals:List<&2,C.Pair<String,T.T>>) -> Result<&2,&2,String,Value>:
  match query:
    case Eval{t,+env,+seen}:
      match t:
        case T.Var{i}: Done{nth_default(List.get(&2,Value,env,i))}
        case T.Univ{u}: Done{VD{B.zero()}}
        case T.Lan{s,d}: type_result(shape_error(s))
        case T.Ran{s,d}: type_result(shape_error(s))
        case T.Lit{l}: Done{VD{B.zero()}}
        case T.Auto{}: Fail{"auto"}
        case T.Ann{tm,ty}: eval(Eval{tm,env,seen},globals)
        case T.Let{n,ty,v,b}:
          do Result<&2,&2,String,Value>:
            val : Value <- eval(Eval{v,env,seen},globals)
            eval(Eval{b,val <> env,seen},globals)
        case T.Global{+name}: eval(global_query(cost(name),name,seen,lookup(globals,name)),globals)
        case T.In{s,a,+args}:
          match s:
            case S.SPi{q,n,d}:
              do Result<&2,&2,String,Value>:
                av : Value <- eval(Address{a,env,seen},globals)
                vs : Value <- eval(Args{args,env,seen},globals)
                return VD{max(vd_of(av),vd_of(vs))}
            case S.SColl{n}: eval(Args{args,env,seen},globals)
            case S.SMu{+fam,ps}:
              do Result<&2,&2,String,Value>:
                vs : Value <- eval(Args{args,env,seen},globals)
                certificate_result(literal_bound(BArgs{args,seen},globals,fam),names_family(args,fam),vs)
            case _: type_result(shape_error(s))
        case T.Sec{s,legs}:
          match s legs:
            case S.SPi{q,n,d} Con{leg,Nil{}}: Done{VClo{leg,env,seen}}
            case S.SPi{q,n,d} _: Fail{"auto"}
            case S.SColl{n} _: eval(Legs{legs,env,seen},globals)
            case S.SMu{n,ps} _: Fail{"mu"}
            case _ _: type_result(shape_error(s))
        case T.Out{+s,+a,+head}:
          match s:
            case S.SPi{q,n,d}: eval(CApp{spine(T.Out{S.SPi{q,n,d},a,head},[]),env,seen},globals)
            case S.SArg{q,n}: eval(CApp{spine(T.Out{S.SArg{q,n},a,head},[]),env,seen},globals)
            case S.SColl{n}:
              do Result<&2,&2,String,Value>:
                v : Value <- eval(Eval{head,env,seen},globals)
                return VD{vd_of(v)}
            case S.SMu{n,ps}: Fail{"mu"}
            case _: type_result(shape_error(s))
        case T.Elim{T.ElimData{s,+scrut,q,m,+bs}}:
          match s:
            case S.SMu{+fam,ps}:
              do Result<&2,&2,String,Value>:
                b : Value <- eval(Branches{bs,env,seen},globals)
                +n : B.T <- literal_bound(BTerm{scrut,seen},globals,fam)
                nonzero : B.T <- bound_result(B.positive(n),n)
                depth : B.T <- bounded(mu_depth(branches_read(bs),nonzero,vd_of(b)))
                return VD{depth}
            case _:
              do Result<&2,&2,String,Value>:
                allowed : Value <- type_result(shape_error(s))
                sv : Value <- eval(Eval{scrut,env,seen},globals)
                bv : Value <- eval(Branches{bs,env,seen},globals)
                n : B.T <- bounded(B.add(B.one(),vd_of(bv)))
                return VD{max(vd_of(sv),n)}
    case Args{args,+env,+seen}:
      match args:
        case Nil{}: Done{VD{B.zero()}}
        case Con{a,rest}:
          do Result<&2,&2,String,Value>:
            av : Value <- eval(Eval{a,env,seen},globals)
            rv : Value <- eval(Args{rest,env,seen},globals)
            return VD{max(vd_of(av),vd_of(rv))}
    case Legs{legs,+env,+seen}:
      match legs:
        case Nil{}: Done{VD{B.zero()}}
        case Con{T.Leg{bindings,t},rest}:
          do Result<&2,&2,String,Value>:
            v : Value <- eval(Eval{t,push(List.length(&2,C.Pair<Q.T,String>,bindings),VD{B.zero()},env),seen},globals)
            r : Value <- eval(Legs{rest,env,seen},globals)
            return VD{max(vd_of(v),vd_of(r))}
    case Address{addr,env,seen}:
      match addr:
        case T.APt{q,t}: eval(Eval{t,env,seen},globals)
        case _: Done{VD{B.zero()}}
    case Branches{bs,+env,+seen}:
      match bs:
        case Nil{}: Done{VD{B.zero()}}
        case Con{C.Pair{a,T.Leg{bindings,t}},rest}:
          do Result<&2,&2,String,Value>:
            av : Value <- eval(Address{a,env,seen},globals)
            v : Value <- eval(Eval{t,push(List.length(&2,C.Pair<Q.T,String>,bindings),VD{vd_of(av)},env),seen},globals)
            r : Value <- eval(Branches{rest,env,seen},globals)
            return VD{max(vd_of(v),vd_of(r))}
    case CApp{C.Pair{head,args},+env,+seen}:
      match head:
        case T.Global{+name}: eval(Primitive{cost(name),T.Global{name},args,env,seen},globals)
        case _:
          do Result<&2,&2,String,Value>:
            v : Value <- eval(Eval{head,env,seen},globals)
            eval(Apply{v,args,env,seen},globals)
    case Primitive{c,head,args,+env,+seen}:
      match c:
        case Some{n}:
          do Result<&2,&2,String,Value>:
            v : Value <- eval(Args{args,env,seen},globals)
            d : B.T <- bounded(B.add(n,vd_of(v)))
            return VD{d}
        case None{}:
          do Result<&2,&2,String,Value>:
            v : Value <- eval(Eval{head,env,seen},globals)
            eval(Apply{v,args,env,seen},globals)
    case Apply{v,args,+env,+seen}:
      match v args:
        case _ Nil{}: Done{v}
        case VD{d} Con{a,rest}: Fail{"opaque callee"}
        case VClo{T.Leg{bindings,body},captured,open} Con{a,rest}:
          do Result<&2,&2,String,Value>:
            av : Value <- eval(Eval{a,env,seen},globals)
            rv : Value <- eval(Eval{body,av <> captured,List.append(&2,String,open,seen)},globals)
            eval(Apply{rv,rest,env,seen},globals)
    case Global{cycle,name,body,seen}:
      match cycle body:
        case True{} _: Fail{"mu"}
        case False{} None{}: Fail{"unknown global " ++ name}
        case False{} Some{t}: eval(Eval{t,[],seen},globals)
    case Unwrap{v}:
      match v:
        case VD{n}: Done{VD{n}}
        case VClo{T.Leg{bs,t},env,seen}:
          do Result<&2,&2,String,Value>:
            result : Value <- eval(Eval{t,VD{B.zero()} <> env,seen},globals)
            eval(Unwrap{result},globals)

def depth(+globals:List<&2,C.Pair<String,T.T>>, t:T.T) -> Result<&2,&2,String,B.T>:
  do Result<&2,&2,String,B.T>:
    v : Value <- eval(Eval{t,[],[]},globals)
    r : Value <- eval(Unwrap{v},globals)
    return vd_of(r)
