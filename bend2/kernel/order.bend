import Base
import ./term.bend as Term
import ./shape.bend as Shape
import ./quantity.bend as Quantity
import ./common.bend as Common
import ./error.bend as Error

type Rule is Data:
  Structural{}

type Status is Data:
  Principal{}
  Smaller{}
  Other{}

type Step is Data:
  Step{scrut:Nat,ctor:Maybe<&2,String>}

type Call is Data:
  Call{callee:String,chain:List<&2,Step>}

type Row is Data:
  Row{member:String,calls:List<&2,Call>}

type T is Data:
  Certificate{group:List<&2,String>,arg:Nat,rows:List<&2,Row>}

type Peeled is Data:
  Peeled{name:String,formals:Nat,inner:Term.T}

type Spine is Data:
  Spine{head:Term.T,args:List<&2,Term.T>,shapes:List<&2,Shape.T<Term.T>>}

def choose(-A:Data,flag:Bool,a:A,b:A) -> A:
  match flag:
    case True{}: a
    case False{}: b

def member(xs:List<&2,String>,+n:String) -> Bool:
  match xs:
    case Nil{}: False{}
    case Con{x,rest}: String.eq(x,n) || member(rest,n)

def mentions(group:List<&2,String>,t:Term.T) -> Bool:
  Term.exists_name(False{},group,t)

def peel(t:Term.T,n:Nat,name:String) -> Peeled:
  match t:
    case Term.Sec{Shape.SPi{q,x,d},Con{Term.Leg{Con{binder,Nil{}},body},Nil{}}}: peel(body,1n+n,name)
    case Term.Sec{Shape.SZk{q,x,d},Con{Term.Leg{Con{binder,Nil{}},body},Nil{}}}: peel(body,1n+n,name)
    case _: Peeled{name,n,t}

def spine(t:Term.T,args:List<&2,Term.T>,shapes:List<&2,Shape.T<Term.T>>) -> Spine:
  match t:
    case Term.Out{s,Term.APt{q,a},head}: spine(head,a <> args,s <> shapes)
    case _: Spine{t,args,shapes}

def status_at(st:List<&2,Status>,ix:Nat) -> Maybe<&2,Status>:
  List.get(&2,Status,st,ix)

def is_smaller(s:Maybe<&2,Status>) -> Bool:
  match s:
    case Some{Smaller{}}: True{}
    case _: False{}

def is_principal_or_smaller(s:Maybe<&2,Status>) -> Bool:
  match s:
    case Some{Principal{}}: True{}
    case Some{Smaller{}}: True{}
    case _: False{}

def guarded_arg(arg:Maybe<&2,Term.T>,st:List<&2,Status>) -> Bool:
  match arg:
    case Some{Term.Var{i}}: is_smaller(status_at(st,i))
    case _: False{}

def guarded_call(st:List<&2,Status>,args:List<&2,Term.T>,k:Nat) -> Bool:
  guarded_arg(List.get(&2,Term.T,args,k),st)

def under(n:Nat,+status:Status,st:List<&2,Status>) -> List<&2,Status>:
  match n:
    case 0n: st
    case 1n+p: status <> under(p,status,st)

def under_point_result(dom:Maybe<&2,Term.T>,st:List<&2,Status>) -> List<&2,Status>:
  match dom:
    case Some{d}: Other{} <> st
    case None{}: st

def under_point(s:Shape.T<Term.T>,st:List<&2,Status>) -> List<&2,Status>:
  under_point_result(Shape.point_dom(Term.T,s),st)

type Task is Data:
  Visit{status:List<&2,Status>,chain:List<&2,Step>,term:Term.T}
  CheckCall{valid:Bool,callee:String,chain:List<&2,Step>}
  Refuse{}

def term_tasks(ts:List<&2,Term.T>,+st:List<&2,Status>,+chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  match ts:
    case Nil{}: rest
    case Con{t,ts}: Visit{st,chain,t} <> term_tasks(ts,st,chain,rest)

def shape_tasks(s:Shape.T<Term.T>,st:List<&2,Status>,chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  term_tasks(Shape.payload(Term.T,s),st,chain,rest)

def shapes_tasks(ss:List<&2,Shape.T<Term.T>>,+st:List<&2,Status>,+chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  match ss:
    case Nil{}: rest
    case Con{s,ss}: shape_tasks(s,st,chain,shapes_tasks(ss,st,chain,rest))

def addr_tasks(a:Term.Addr<Term.T>,st:List<&2,Status>,chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  term_tasks(Term.addr_children(a),st,chain,rest)

def leg_tasks(ls:List<&2,Term.Leg<Term.T>>,+st:List<&2,Status>,+chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  match ls:
    case Nil{}: rest
    case Con{Term.Leg{bs,t},ls}: Visit{under(List.length(&2,Common.Pair<Quantity.T,String>,bs),Other{},st),chain,t} <> leg_tasks(ls,st,chain,rest)

def spine_tasks(sp:Spine,+st:List<&2,Status>,+chain:List<&2,Step>,group:List<&2,String>,k:Nat,rest:List<&2,Task>) -> List<&2,Task>:
  match sp:
    case Spine{head,+args,shapes}:
      match head:
        case Term.Global{+g}:
          +after = shapes_tasks(shapes,st,chain,term_tasks(args,st,chain,rest))
          choose(List<&2,Task>,member(group,g),CheckCall{guarded_call(st,args,k),g,chain} <> after,after)
        case Term.Out{s,a,h}: shape_tasks(s,st,chain,addr_tasks(a,st,chain,Visit{st,chain,h} <> shapes_tasks(shapes,st,chain,term_tasks(args,st,chain,rest))))
        case _: Visit{st,chain,head} <> shapes_tasks(shapes,st,chain,term_tasks(args,st,chain,rest))

def scrut_var(t:Term.T) -> Maybe<&2,Nat>:
  match t:
    case Term.Var{i}: Some{i}
    case _: None{}

def special_scrut(v:Maybe<&2,Nat>,st:List<&2,Status>) -> Bool:
  match v:
    case Some{i}: is_principal_or_smaller(status_at(st,i))
    case None{}: False{}

def branch_chain(v:Maybe<&2,Nat>,special:Bool,a:Term.Addr<Term.T>,+chain:List<&2,Step>) -> List<&2,Step>:
  match v:
    case Some{i}: choose(List<&2,Step>,special,Step{i,Term.as_actor(Term.T,a)} <> chain,chain)
    case None{}: chain

def branch_tasks(bs:List<&2,Common.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>,+st:List<&2,Status>,+chain:List<&2,Step>,+v:Maybe<&2,Nat>,+special:Bool,rest:List<&2,Task>) -> List<&2,Task>:
  match bs:
    case Nil{}: rest
    case Con{Common.Pair{+a,Term.Leg{bindings,t}},bs}:
      inner = under(List.length(&2,Common.Pair<Quantity.T,String>,bindings),choose(Status,special,Smaller{},Other{}),st)
      addr_tasks(a,st,chain,Visit{inner,branch_chain(v,special,a,chain),t} <> branch_tasks(bs,st,chain,v,special,rest))

def motive_tasks(m:Maybe<&2,Term.Motive<Term.T>>,st:List<&2,Status>,chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  match m:
    case None{}: rest
    case Some{Term.Motive{ind,indices,self,t}}: Visit{under(1n+List.length(&2,String,indices),Other{},st),chain,t} <> rest

def elim_tasks(e:Term.ElimData<Term.T>,+st:List<&2,Status>,+chain:List<&2,Step>,rest:List<&2,Task>) -> List<&2,Task>:
  match e:
    case Term.ElimData{s,+scrut,q,m,bs}:
      +v = scrut_var(scrut)
      special = special_scrut(v,st)
      Visit{st,chain,scrut} <> shape_tasks(s,st,chain,motive_tasks(m,st,chain,branch_tasks(bs,st,chain,v,special,rest)))

@unsafe def walk(tasks:List<&2,Task>,+group:List<&2,String>,+k:Nat,calls:List<&2,Call>) -> Maybe<&2,List<&2,Call>>:
  match tasks:
    case Nil{}: Some{List.reverse(&2,Call,calls)}
    case Con{Refuse{},rest}: None{}
    case Con{CheckCall{valid,n,chain},rest}:
      match valid:
        case False{}: None{}
        case True{}: walk(rest,group,k,Call{n,chain} <> calls)
    case Con{Visit{+st,+chain,t},+rest}:
      match t:
        case Term.Global{g}: walk(choose(List<&2,Task>,member(group,g),Refuse{} <> rest,rest),group,k,calls)
        case Term.Lan{+s,d}: walk(shape_tasks(s,st,chain,Visit{under_point(s,st),chain,d} <> rest),group,k,calls)
        case Term.Ran{+s,d}: walk(shape_tasks(s,st,chain,Visit{under_point(s,st),chain,d} <> rest),group,k,calls)
        case Term.In{s,a,args}: walk(shape_tasks(s,st,chain,addr_tasks(a,st,chain,term_tasks(args,st,chain,rest))),group,k,calls)
        case Term.Sec{s,legs}: walk(shape_tasks(s,st,chain,leg_tasks(legs,st,chain,rest)),group,k,calls)
        case Term.Out{s,a,h}: walk(spine_tasks(spine(Term.Out{s,a,h},[],[]),st,chain,group,k,rest),group,k,calls)
        case Term.Elim{e}: walk(elim_tasks(e,st,chain,rest),group,k,calls)
        case Term.Let{x,ty,v,b}: walk(Visit{st,chain,ty} <> Visit{st,chain,v} <> Visit{Other{} <> st,chain,b} <> rest,group,k,calls)
        case Term.Ann{tm,ty}: walk(Visit{st,chain,tm} <> Visit{st,chain,ty} <> rest,group,k,calls)
        case _: walk(rest,group,k,calls)

def seed(n:Nat,+index:Nat,+formals:Nat,+k:Nat) -> List<&2,Status>:
  match n:
    case 0n: []
    case 1n+p: choose(Status,Nat.is_eq(1n+Nat.add(index,k),formals),Principal{},Other{}) <> seed(p,1n+index,formals,k)

def passes(rule:Rule,group:List<&2,String>,+k:Nat,+formals:Nat,body:Term.T) -> Maybe<&2,List<&2,Call>>:
  match rule:
    case Structural{}: walk([Visit{seed(formals,0n,formals,k),[],body}],group,k,[])

def names(ms:List<&2,Common.Pair<String,Term.T>>) -> List<&2,String>:
  match ms:
    case Nil{}: []
    case Con{Common.Pair{n,b},rest}: n <> names(rest)

def peeled(ms:List<&2,Common.Pair<String,Term.T>>) -> List<&2,Peeled>:
  match ms:
    case Nil{}: []
    case Con{Common.Pair{n,b},rest}: peel(b,0n,n) <> peeled(rest)

def any_mentions(ms:List<&2,Common.Pair<String,Term.T>>,+group:List<&2,String>) -> Bool:
  match ms:
    case Nil{}: False{}
    case Con{Common.Pair{n,b},rest}: mentions(group,b) || any_mentions(rest,group)

def min_bound(a:Maybe<&2,Nat>,n:Nat) -> Maybe<&2,Nat>:
  match a:
    case None{}: Some{n}
    case Some{m}: Some{Nat.min(m,n)}

def bound(ms:List<&2,Peeled>,+group:List<&2,String>,+acc:Maybe<&2,Nat>) -> Maybe<&2,Nat>:
  match ms:
    case Nil{}: acc
    case Con{Peeled{n,f,b},rest}: bound(rest,group,choose(Maybe<&2,Nat>,mentions(group,b),min_bound(acc,f),acc))

def bound_default(b:Maybe<&2,Nat>) -> Nat:
  match b:
    case None{}: 0n
    case Some{n}: n

def row_join(cs:Maybe<&2,List<&2,Call>>,rs:Maybe<&2,List<&2,Row>>,n:String) -> Maybe<&2,List<&2,Row>>:
  match cs rs:
    case Some{calls} Some{rows}: Some{Row{n,calls} <> rows}
    case _ _: None{}

def rows_at(ms:List<&2,Peeled>,+group:List<&2,String>,+k:Nat) -> Maybe<&2,List<&2,Row>>:
  match ms:
    case Nil{}: Some{[]}
    case Con{Peeled{n,f,b},rest}: row_join(passes(Structural{},group,k,f,b),rows_at(rest,group,k),n)

def refusal(ms:List<&2,Common.Pair<String,Term.T>>) -> Error.T:
  match ms:
    case Nil{}: Error.Termination{""}
    case Con{Common.Pair{n,b},rest}: Error.Termination{n}

type Search is Data:
  Search{remaining:Nat,index:Nat}
  Candidate{rows:Maybe<&2,List<&2,Row>>,remaining:Nat,index:Nat}

@unsafe def first_fit(s:Search,+ps:List<&2,Peeled>,+group:List<&2,String>,+error:Error.T) -> Result<&2,&2,Error.T,Maybe<&2,T>>:
  match s:
    case Search{0n,k}: Fail{error}
    case Search{1n+remaining,+k}: first_fit(Candidate{rows_at(ps,group,k),remaining,k},ps,group,error)
    case Candidate{Some{rows},remaining,k}: Done{Some{Certificate{group,k,rows}}}
    case Candidate{None{},remaining,k}: first_fit(Search{remaining,1n+k},ps,group,error)

def certify_used(used:Bool,+ms:List<&2,Common.Pair<String,Term.T>>) -> Result<&2,&2,Error.T,Maybe<&2,T>>:
  match used:
    case False{}: Done{None{}}
    case True{}:
      +group = names(ms)
      +ps = peeled(ms)
      n = bound_default(bound(ps,group,None{}))
      first_fit(Search{n,0n},ps,group,refusal(ms))

def certify(+members:List<&2,Common.Pair<String,Term.T>>) -> Result<&2,&2,Error.T,Maybe<&2,T>>:
  certify_used(any_mentions(members,names(members)),members)

def argument(cert:T) -> Nat:
  match cert:
    case Certificate{group,arg,rows}: arg

def one_elim_msg() -> String:
  "a structurally recursive definition eliminates its recursive argument at the head of its body"

def motive_msg() -> String:
  "the elimination of a structurally recursive definition carries its motive"

def ctor_key_msg() -> String:
  "every branch of the elimination of a structurally recursive definition keys a constructor"

def valid_calls(cs:List<&2,Call>,+name:String) -> Result<&2,&2,Error.T,Unit>:
  match cs:
    case Nil{}: Done{Unit{}}
    case Con{Call{callee,Con{Step{i,Some{ctor}},chain}},rest}: valid_calls(rest,name)
    case _: Fail{Error.Termination{name}}

def valid_rows(rs:List<&2,Row>) -> Result<&2,&2,Error.T,Unit>:
  match rs:
    case Nil{}: Done{Unit{}}
    case Con{Row{n,cs},rest}:
      do Result<&2,&2,Error.T,Unit>:
        valid_calls(cs,n)
        valid_rows(rest)

def valid_branches(bs:List<&2,Common.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>) -> Result<&2,&2,Error.T,Unit>:
  match bs:
    case Nil{}: Done{Unit{}}
    case Con{Common.Pair{Term.ACtor{n},lg},rest}: valid_branches(rest)
    case _: Fail{Error.Wrong_leg{ctor_key_msg()}}

def valid_motive(m:Maybe<&2,Term.Motive<Term.T>>,bs:List<&2,Common.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>,rows:List<&2,Row>) -> Result<&2,&2,Error.T,Unit>:
  match m:
    case None{}: Fail{Error.Missing_branch{motive_msg()}}
    case Some{mo}:
      do Result<&2,&2,Error.T,Unit>:
        valid_branches(bs)
        valid_rows(rows)

def valid_elim(e:Term.ElimData<Term.T>,formals:Nat,k:Nat,rows:List<&2,Row>) -> Result<&2,&2,Error.T,Unit>:
  match e:
    case Term.ElimData{s,Term.Var{i},q,m,bs}:
      do Result<&2,&2,Error.T,Unit>:
        choose(Result<&2,&2,Error.T,Unit>,Nat.is_eq(1n+Nat.add(i,k),formals),Done{Unit{}},Fail{Error.Mismatch{one_elim_msg()}})
        valid_motive(m,bs,rows)
    case _: Fail{Error.Mismatch{one_elim_msg()}}

def translate_inner(p:Peeled,k:Nat,rows:List<&2,Row>) -> Result<&2,&2,Error.T,Unit>:
  match p:
    case Peeled{name,n,Term.Elim{e}}: valid_elim(e,n,k,rows)
    case _: Fail{Error.Mismatch{one_elim_msg()}}

def translate_used(used:Bool,cert:T,+body:Term.T) -> Result<&2,&2,Error.T,Term.T>:
  match used cert:
    case False{} _: Done{body}
    case True{} Certificate{group,k,rows}:
      do Result<&2,&2,Error.T,Term.T>:
        translate_inner(peel(body,0n,""),k,rows)
        return body

def translate(cert:T,+body:Term.T) -> Result<&2,&2,Error.T,Term.T>:
  match cert:
    case Certificate{+group,+k,+rows}: translate_used(mentions(group,body),Certificate{group,k,rows},body)
