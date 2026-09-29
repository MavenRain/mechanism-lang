import Base
import ./literal.bend as Literal
import ./bignum.bend as Bignum

type Tid is Data:
  Tid{name: String}

type Fid is Data:
  Fid{name: String}

type Repr is Data:
  RI31{}
  RStruct{tid: Tid}
  RUnion{tid: Tid}
  RFunc{tid: Tid}
  RThunk{tid: Tid}

type Branch<-A: Data> is Data:
  Branch{tag: Nat, arity: Nat, body: A}

type Ktm is Data:
  KVar{index: Nat}
  KLit{value: Literal.T}
  KGlobal{name: String}
  KErased{}
  KLet{name: String, value: Ktm, body: Ktm}
  KClos{fid: Fid, arity: Nat, captures: List<&2, Ktm>}
  KApp{head: Ktm, args: List<&2, Ktm>}
  KTail{head: Ktm, args: List<&2, Ktm>}
  KStruct{tid: Tid, fields: List<&2, Ktm>}
  KProj{tid: Tid, index: Nat, value: Ktm}
  KTag{tid: Tid, tag: Nat, fields: List<&2, Ktm>}
  KCase{tid: Tid, scrut: Ktm, branches: List<&2, Branch<Ktm>>}
  KDelay{fid: Fid, captures: List<&2, Ktm>}
  KForce{value: Ktm}

type Kdecl is Data:
  KFun{fid: Fid, params: List<&2, Repr>, result: Repr, body: Ktm}
  KRec{tids: List<&2, Tid>}

def tid_text(t: Tid) -> String:
  match t:
    case Tid{name}: name

def fid_text(f: Fid) -> String:
  match f:
    case Fid{name}: name

def decimal_escape(+n:U32) -> String:
  "\\" ++ U32.show(U32.div(n,100)) ++ U32.show(U32.mod(U32.div(n,10),10)) ++ U32.show(U32.mod(n,10))

def ascii_escape(printable:Bool,n:U32) -> String:
  match printable:
    case True{}: String.from_list([Char.from_u32(n)])
    case False{}: decimal_escape(n)

def unicode_escape_four(four:Bool,+n:U32) -> String:
  match four:
    case False{}:
      decimal_escape(U32.or(224,U32.shrn(n,12n))) ++ decimal_escape(U32.or(128,U32.and(U32.shrn(n,6n),63))) ++ decimal_escape(U32.or(128,U32.and(n,63)))
    case True{}:
      decimal_escape(U32.or(240,U32.shrn(n,18n))) ++ decimal_escape(U32.or(128,U32.and(U32.shrn(n,12n),63))) ++ decimal_escape(U32.or(128,U32.and(U32.shrn(n,6n),63))) ++ decimal_escape(U32.or(128,U32.and(n,63)))

def unicode_escape_two(two:Bool,+n:U32) -> String:
  match two:
    case True{}: decimal_escape(U32.or(192,U32.shrn(n,6n))) ++ decimal_escape(U32.or(128,U32.and(n,63)))
    case False{}: unicode_escape_four(U32.is_ge(n,65536),n)

def unicode_escape(ascii:Bool,+n:U32) -> String:
  match ascii:
    case True{}: ascii_escape(U32.is_ge(n,32) && U32.is_lt(n,127),n)
    case False{}: unicode_escape_two(U32.is_lt(n,2048),n)

def escape_char(c: Char) -> String:
  match c:
    case '\n': "\\n"
    case '\r': "\\r"
    case '\t': "\\t"
    case '\u{8}': "\\b"
    case '"': "\\\""
    case '\\': "\\\\"
    case _:
      +n = Char.to_u32(c)
      unicode_escape(U32.is_lt(n,128),n)

def escape(cs: List<&2, Char>) -> String:
  match cs:
    case Nil{}: ""
    case Con{c, rest}: escape_char(c) ++ escape(rest)

def literal_text(l: Literal.T) -> String:
  match l:
    case Literal.LString{s}: "\"" ++ escape(String.to_list(s)) ++ "\""
    case Literal.LInt{n}: Bignum.to_string(n)

def print_repr(r: Repr) -> String:
  match r:
    case RI31{}: "i31"
    case RStruct{t}: "struct " ++ tid_text(t)
    case RUnion{t}: "union " ++ tid_text(t)
    case RFunc{t}: "func " ++ tid_text(t)
    case RThunk{t}: "thunk " ++ tid_text(t)

type PrintTask is Data:
  Text{text: String}
  Term{term: Ktm}
  Terms{terms: List<&2, Ktm>}
  Branches{branches: List<&2, Branch<Ktm>>}

def expand(t: Ktm) -> List<&2, PrintTask>:
  match t:
    case KVar{i}: [Text{"KVar " ++ Nat.show(i)}]
    case KLit{l}: [Text{"KLit " ++ literal_text(l)}]
    case KGlobal{n}: [Text{"KGlobal " ++ n}]
    case KErased{}: [Text{"KErased"}]
    case KLet{n,v,b}: [Text{"KLet " ++ n ++ " ("}, Term{v}, Text{") ("}, Term{b}, Text{")"}]
    case KClos{f,n,cs}: [Text{"KClos " ++ fid_text(f) ++ " " ++ Nat.show(n) ++ " ["}, Terms{cs}, Text{"]"}]
    case KApp{h,args}: [Text{"KApp ("}, Term{h}, Text{") ["}, Terms{args}, Text{"]"}]
    case KTail{h,args}: [Text{"KTail ("}, Term{h}, Text{") ["}, Terms{args}, Text{"]"}]
    case KStruct{t,fs}: [Text{"KStruct " ++ tid_text(t) ++ " ["}, Terms{fs}, Text{"]"}]
    case KProj{t,k,x}: [Text{"KProj " ++ tid_text(t) ++ " " ++ Nat.show(k) ++ " ("}, Term{x}, Text{")"}]
    case KTag{t,k,ps}: [Text{"KTag " ++ tid_text(t) ++ " " ++ Nat.show(k) ++ " ["}, Terms{ps}, Text{"]"}]
    case KCase{t,s,bs}: [Text{"KCase " ++ tid_text(t) ++ " ("}, Term{s}, Text{") ["}, Branches{bs}, Text{"]"}]
    case KDelay{f,cs}: [Text{"KDelay " ++ fid_text(f) ++ " ["}, Terms{cs}, Text{"]"}]
    case KForce{x}: [Text{"KForce ("}, Term{x}, Text{")"}]

@unsafe def print_tasks(tasks: List<&2, PrintTask>) -> String:
  match tasks:
    case Nil{}: ""
    case Con{task, rest}:
      match task:
        case Text{s}: s ++ print_tasks(rest)
        case Term{t}: print_tasks(List.append(&2, PrintTask, expand(t), rest))
        case Terms{Nil{}}: print_tasks(rest)
        case Terms{Con{t, Nil{}}}: print_tasks(Term{t} <> rest)
        case Terms{Con{t, ts}}: print_tasks(Term{t} <> Text{"; "} <> Terms{ts} <> rest)
        case Branches{Nil{}}: print_tasks(rest)
        case Branches{Con{Branch{tag,arity,body}, Nil{}}}:
          print_tasks(Text{"{" ++ Nat.show(tag) ++ " " ++ Nat.show(arity) ++ " ("} <> Term{body} <> Text{")}"} <> rest)
        case Branches{Con{Branch{tag,arity,body}, bs}}:
          print_tasks(Text{"{" ++ Nat.show(tag) ++ " " ++ Nat.show(arity) ++ " ("} <> Term{body} <> Text{")}; "} <> Branches{bs} <> rest)

def print_ktm(t: Ktm) -> String:
  print_tasks([Term{t}])

def repr_list(rs: List<&2, Repr>) -> String:
  match rs:
    case Nil{}: ""
    case Con{r, Nil{}}: print_repr(r)
    case Con{r, rest}: print_repr(r) ++ ", " ++ repr_list(rest)

def tid_list(ts: List<&2, Tid>) -> String:
  match ts:
    case Nil{}: ""
    case Con{t, Nil{}}: tid_text(t)
    case Con{t, rest}: tid_text(t) ++ "; " ++ tid_list(rest)

def print_decl(d: Kdecl) -> String:
  match d:
    case KFun{f,ps,r,b}:
      "fun " ++ fid_text(f) ++ " (" ++ repr_list(ps) ++ ") : " ++ print_repr(r) ++ " := " ++ print_ktm(b)
    case KRec{ts}: "rec [" ++ tid_list(ts) ++ "]"
