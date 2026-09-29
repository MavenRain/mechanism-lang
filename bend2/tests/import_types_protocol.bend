import Base
import ./import_types_protocol_data.bend as D
import ../import/export.bend as E
import ../import/translate.bend as T
import ../import/lower.bend as L
import ../import/table.bend as Table
import ../import/ndjson.bend as Json
import ../kernel/global.bend as G
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/rules.bend as Rules
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/budget.bend as B
import ../kernel/check.bend as Check
import ../kernel/eval.bend as Eval
import ../kernel/conv.bend as Conv
import ../kernel/error.bend as Error
import ../kernel/prim.bend as Prim

def require(ok:Bool,message:String) -> Result<&2,&2,String,Unit>:
  match ok:
    case True{}: Done{Unit{}}
    case False{}: Fail{message}

def imported(-A:Data,value:Result<&2,&2,E.Error,A>) -> Result<&2,&2,String,A>:
  match value:
    case Done{x}: Done{x}
    case Fail{E.ImportError{line,message}}: Fail{message}

def kernel(-A:Data,value:Result<&2,&2,Error.T,A>) -> Result<&2,&2,String,A>:
  match value:
    case Done{x}: Done{x}
    case Fail{e}: Fail{Error.to_string(e)}

def successful(value:Result<&2,&2,L.Error,Term.T>) -> Result<&2,&2,String,Term.T>:
  match value:
    case Done{x}: Done{x}
    case Fail{e}: Fail{L.message(e)}

law row:
  for rows:List<&2,T.Row>
  Result<&2,&2,String,T.Row>

@unsafe def row_choice(found:Bool,value:T.Row,rest:List<&2,T.Row>) -> Result<&2,&2,String,T.Row>:
  match found:
    case True{}: Done{value}
    case False{}: row(rest)

@unsafe def row(rows):
  match rows:
    case Nil{}: Fail{"missing test declaration"}
    case Con{T.Row{+declaration,+name,+parameters,+root,+dependencies},rest}:
      row_choice(String.eq(name,"test"),T.Row{declaration,name,parameters,root,dependencies},rest)

def lower_found(found:Result<&2,&2,String,T.Row>,table:T.T,resolver:L.Resolver) -> Result<&2,&2,L.Error,Term.T>:
  match found:
    case Fail{message}: Fail{L.Unsupported{message}}
    case Done{declaration}: L.lower_default(resolver,G.initial(),table,declaration)

def lower(+table:T.T,resolver:L.Resolver) -> Result<&2,&2,L.Error,Term.T>:
  lower_found(row(T.rows(table)),table,resolver)

def missing() -> L.Resolver:
  L.missing("no checked mapping for ")

def equivalent(expected:Term.T,actual:Term.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    left : Value.T <- kernel(Value.T,Eval.eval(G.initial(),[],expected))
    right : Value.T <- kernel(Value.T,Eval.eval(G.initial(),[],actual))
    same : Bool <- kernel(Bool,Conv.conv_type(Check.make(G.initial(),B.unlimited(),0n),left,right))
    require(same,"types are not convertible")

def refusal(kind:String,value:Result<&2,&2,L.Error,Term.T>) -> Result<&2,&2,String,Unit>:
  match value:
    case Done{x}: Fail{"expected " ++ kind}
    case Fail{L.Deferred{message}}: require(String.eq(kind,"deferred"),"wrong lowering refusal: " ++ message)
    case Fail{L.Unsupported{message}}: require(String.eq(kind,"unsupported"),"wrong lowering refusal: " ++ message)
    case Fail{L.KernelError{error}}: require(String.eq(kind,"kernel"),"wrong lowering refusal: " ++ Error.to_string(error))

def semantic(prefix:String,value:Result<&2,&2,E.Error,T.T>) -> Result<&2,&2,String,Unit>:
  match value:
    case Done{x}: Fail{"expected semantic refusal: " ++ prefix}
    case Fail{E.ImportError{line,+message}}:
      require(Bool.and(U32.is_gt(line,0),String.starts_with(message,prefix)),"wrong semantic refusal: " ++ message)

def retained_string(value:Maybe<&2,T.Node>) -> Bool:
  match value:
    case Some{T.StringLiteral{text}}: String.eq(text,"retained")
    case _: False{}

def retained_metadata(value:Maybe<&2,T.Node>) -> Bool:
  match value:
    case Some{T.Metadata{expr,data}}: Bool.and(String.eq(expr,"1"),String.eq(Json.to_string(Json.Object{data}),"{\"opaque\":{\"array\":[\"x\",true]}}"))
    case _: False{}

def canonical_parameter(declaration:T.Row) -> Bool:
  match declaration:
    case T.Row{d,n,Con{T.Parameter{id,name},Nil{}},r,deps}: Bool.and(String.eq(id,"1"),String.eq(name,"u"))
    case _: False{}

def levels_match(levels:List<&2,Level.T>) -> Bool:
  match levels:
    case Con{first,Con{second,Nil{}}}:
      Bool.and(Level.equal(first,Level.succ(Level.imax(Level.var(1n),Level.var(0n)))),Level.equal(second,Level.max(Level.var(1n),Level.var(0n))))
    case _: False{}

law count_resolver:
  for count:Nat
  L.Resolver

@unsafe def count_call(probe:Bool,+count:Nat,name:String,levels:List<&2,Level.T>) -> L.Resolver & Result<&2,&2,String,Term.T>:
  match probe:
    case True{}: (count_resolver(count),Done{Term.Var{count}})
    case False{}:
      (count_resolver(Succ{count}),Bool.pick(Result<&2,&2,String,Term.T>,Bool.and(String.eq(name,"F"),levels_match(levels)),Done{Term.Univ{Level.zero()}},Fail{"wrong source reference or universe substitution"}))

@unsafe def count_resolver(count):
  L.Resolver{+name => levels => count_call(String.eq(name,"__count"),count,name,levels)}

def count_result(value:Result<&2,&2,String,Term.T>) -> Result<&2,&2,String,Unit>:
  match value:
    case Done{Term.Var{count}}: require(Nat.is_eq(count,1n),"resolver did not run once")
    case _: Fail{"resolver counter could not be read"}

def count_pair(value:L.Resolver & Result<&2,&2,String,Term.T>) -> Result<&2,&2,String,Unit>:
  (next,value) = value
  count_result(value)

def check_count(state:L.State) -> Result<&2,&2,String,Unit>:
  match state:
    case L.State{work,budget,resolver}:
      count_pair(L.resolver_call(resolver,"__count",[]))

def counted_result(value:L.State & Result<&2,&2,L.Error,Term.T>) -> Result<&2,&2,String,Unit>:
  (state,value) = value
  do Result<&2,&2,String,Unit>:
    term : Term.T <- successful(value)
    check_count(state)
    equivalent(Term.Univ{Level.zero()},term)

def counted(+table:T.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    declaration : T.Row <- row(T.rows(table))
    action : L.Comp(Term.T) = L.lower_row(declaration,G.initial(),B.of_polls(100000n),table)
    counted_result(action(L.State{100000n,B.of_polls(100000n),count_resolver(0n)}))

law nat_resolver: L.Resolver

@unsafe def nat_call(name:String,levels:List<&2,Level.T>) -> L.Resolver & Result<&2,&2,String,Term.T>:
  (nat_resolver(),Bool.pick(Result<&2,&2,String,Term.T>,Bool.and(String.eq(name,"Nat"),Nat.is_eq(List.length(&2,Level.T,levels),0n)),Done{Prim.nat_ty()},Fail{"unexpected Nat resolution"}))

@unsafe def nat_resolver():
  L.Resolver{name => levels => nat_call(name,levels)}

def budget_error(value:Result<&2,&2,L.Error,Term.T>) -> Result<&2,&2,String,Unit>:
  match value:
    case Fail{L.KernelError{Error.Budget_exhausted{message}}}: Done{Unit{}}
    case Fail{other}: Fail{"shared Pi expansion failed without a budget error: " ++ L.message(other)}
    case Done{x}: Fail{"shared Pi expansion escaped its budget"}

def inspect(probe:D.Probe,+table:T.T) -> Result<&2,&2,String,Unit>:
  match probe:
    case D.EquivalentPi{}: do Result<&2,&2,String,Unit>:
      term : Term.T <- successful(lower(table,missing()))
      equivalent(Rules.arrow(Q.QMany{},"x",Term.Univ{Level.one()},Term.Var{0n}),term)
    case D.EquivalentZero{}: do Result<&2,&2,String,Unit>:
      term : Term.T <- successful(lower(table,missing()))
      equivalent(Term.Univ{Level.zero()},term)
    case D.Infer{}: do Result<&2,&2,String,Unit>:
      term : Term.T <- successful(lower(table,missing()))
      value : Value.T <- kernel(Value.T,Check.infer_term(G.initial(),B.unlimited(),term))
      return Unit{}
    case D.Refuse{kind}: refusal(kind,lower(table,missing()))
    case D.GlobalResolver{}: refusal("kernel",lower(table,L.fixed(Term.Global{"uninstalled"})))
    case D.LocalResolver{}: refusal("kernel",lower(table,L.fixed(Term.Var{0n})))
    case D.StringNode{}: do Result<&2,&2,String,Unit>:
      require(retained_string(Table.get(T.Node,T.type_nodes(table),"1")),"string node vanished")
      refusal("unsupported",lower(table,missing()))
    case D.PrenexResolver{}: counted(table)
    case D.Alias{}: do Result<&2,&2,String,Unit>:
      declaration : T.Row <- row(T.rows(table))
      require(canonical_parameter(declaration),"parameter identity was not canonical")
      term : Term.T <- successful(lower(table,missing()))
      return Unit{}
    case D.Metadata{}: do Result<&2,&2,String,Unit>:
      require(retained_metadata(Table.get(T.Node,T.type_nodes(table),"2")),"metadata payload changed")
      term : Term.T <- successful(lower(table,missing()))
      equivalent(Term.Univ{Level.zero()},term)
    case D.NatResolver{}: do Result<&2,&2,String,Unit>:
      term : Term.T <- successful(lower(table,nat_resolver()))
      return Unit{}
    case D.Shared{}:
      require(Bool.and(Nat.is_eq(Table.size(T.Node,T.type_nodes(table)),3n),Nat.is_eq(List.length(&2,T.Row,T.rows(table)),2n)),"shared type nodes were copied or lost")
    case D.Budget{}: do Result<&2,&2,String,Unit>:
      declaration : T.Row <- row(T.rows(table))
      refusal("kernel",L.lower_type(missing(),G.initial(),table,declaration,B.of_polls(0n)))
    case D.SharedBudget{}: do Result<&2,&2,String,Unit>:
      require(Nat.is_eq(Table.size(T.Node,T.type_nodes(table)),25n),"shared Pi input was expanded in the source table")
      budget_error(lower(table,missing()))
    case D.Semantic{prefix}: Fail{"semantic probe was routed to successful translation"}

def translated(probe:D.Probe,value:Result<&2,&2,E.Error,T.T>) -> Result<&2,&2,String,Unit>:
  match probe:
    case D.Semantic{prefix}: semantic(prefix,value)
    case other: do Result<&2,&2,String,Unit>:
      table : T.T <- imported(T.T,value)
      inspect(other,table)

def test(source:String,probe:D.Probe) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    table : E.T <- imported(E.T,E.read_string(source))
    translated(probe,T.translate(table))

def report(value:Result<&2,&2,String,Unit>,+name:String) -> IO(List<&2,String>):
  match value:
    case Done{x}: do IO<List<&2,String>>:
      IO.print("PASS import-types " ++ name)
      return []
    case Fail{+message}: do IO<List<&2,String>>:
      IO.print("FAIL import-types " ++ name ++ ": " ++ message)
      return [name ++ ": " ++ message]

def run(rows:List<&2,D.Case>) -> IO(List<&2,String>):
  match rows:
    case Nil{}: IO.pure(List<&2,String>,[])
    case Con{D.Case{name,source,probe},rest}: do IO<List<&2,String>>:
      failed : List<&2,String> <- report(test(source,probe),name)
      remaining : List<&2,String> <- run(rest)
      return List.append(&2,String,failed,remaining)

def verdict(failures:List<&2,String>) -> IO(Unit):
  match failures:
    case Nil{}: IO.print("IMPORT-TYPES-OK")
    case other: IO.die(Unit,1,"IMPORT-TYPES-FAIL " ++ Json.join_strings(other,"","; "))

def arguments(args:List<String>) -> IO(Unit):
  do IO<Unit>:
    failures : List<&2,String> <- run(D.cases())
    verdict(failures)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
