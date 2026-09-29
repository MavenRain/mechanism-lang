import Base
import ./import_protocol_data.bend as D
import ../import/export.bend as E
import ../import/names.bend as Names
import ../import/decls.bend as Decls
import ../import/exprs.bend as Exprs
import ../import/table.bend as Table
import ../io.bend as Files

def require(ok:Bool,message:String) -> Result<&2,&2,String,Unit>:
  match ok:
    case True{}: Done{Unit{}}
    case False{}: Fail{message}

def parsed(value:Result<&2,&2,E.Error,E.T>) -> Result<&2,&2,String,E.T>:
  match value:
    case Done{x}: Done{x}
    case Fail{E.ImportError{line,message}}: Fail{"line " ++ U32.show(line) ++ ": " ++ message}

def reject(line:U32,value:Result<&2,&2,E.Error,E.T>) -> Result<&2,&2,String,Unit>:
  match value:
    case Done{x}: Fail{"malformed input was accepted"}
    case Fail{E.ImportError{+actual,+message}}:
      require(Bool.and(U32.is_eq(line,actual),Bool.not(String.eq(message,""))),"wrong location or empty diagnostic: " ++ U32.show(actual) ++ ": " ++ message)

def str_name(value:Maybe<&2,Names.T>,prefix:String,text:String) -> Bool:
  match value:
    case Some{Names.Str{p,t}}: Bool.and(String.eq(p,prefix),String.eq(t,text))
    case _: False{}

def num_name(value:Maybe<&2,Names.T>) -> Bool:
  match value:
    case Some{Names.Num{p,t}}: Bool.and(String.eq(p,"0"),String.eq(t,"1"))
    case _: False{}

def declarations(value:E.Counts,n:Nat) -> Bool:
  match value:
    case E.Counts{lines,names,levels,exprs,decls}: Nat.is_eq(decls,n)

def different(a:Maybe<&2,String>,b:Maybe<&2,String>) -> Bool:
  match a b:
    case Some{x} Some{y}: Bool.not(String.eq(x,y))
    case _ _: False{}

def inspect(probe:D.Probe,+table:E.T) -> Result<&2,&2,String,Unit>:
  match probe:
    case D.Numeric{}:
      do Result<&2,&2,String,Unit>:
        require(str_name(E.name(table,"6"),"0","1"),"string name segment changed")
        require(num_name(E.name(table,"7")),"numeric name segment changed")
        require(declarations(E.counts(table),2n),"distinct names lost a declaration")
    case D.Group{}:
      do Result<&2,&2,String,Unit>:
        require(declarations(E.counts(table),3n),"group members missing")
        require(Nat.is_eq(List.length(&2,Decls.Group,E.groups(table)),1n),"group identity lost")
    case D.Dotted{}:
      do Result<&2,&2,String,Unit>:
        require(str_name(E.name(table,"6"),"0","x.y"),"dotted component structure changed")
        require(str_name(E.name(table,"8"),"7","y"),"nested name structure changed")
        require(different(E.name_string(table,"6"),E.name_string(table,"8")),"distinct name structures have the same display")
    case _: Done{Unit{}}

def test(probe:D.Probe,value:Result<&2,&2,E.Error,E.T>) -> Result<&2,&2,String,Unit>:
  match probe:
    case D.Reject{line}: reject(line,value)
    case other: do Result<&2,&2,String,Unit>:
      table : E.T <- parsed(value)
      inspect(other,table)

def inhouse(+name:String) -> Bool:
  Bool.or(Bool.or(String.eq(name,"UnifiedAggregation"),String.starts_with(name,"UnifiedAggregation.")),Bool.or(Bool.or(String.eq(name,"ArrowCat"),String.starts_with(name,"ArrowCat.")),Bool.or(String.eq(name,"CompCatTheory"),String.starts_with(name,"CompCatTheory."))))

def name_of(source:E.T,id:String) -> Result<&2,&2,String,String>:
  Table.require(String,E.name_string(source,id),"missing name after read")

def frozen_counts(value:E.Counts) -> Bool:
  match value:
    case E.Counts{lines,names,levels,exprs,decls}:
      Bool.and(U32.is_eq(lines,219778),Bool.and(Nat.is_eq(names,17759n),Bool.and(Nat.is_eq(levels,146n),Bool.and(Nat.is_eq(exprs,198927n),Nat.is_eq(decls,3202n)))))

def kind_count(rows:List<&2,Decls.T>,+kind:String) -> Nat:
  match rows:
    case Nil{}: 0n
    case Con{Decls.Decl{name,levels,typ,k,line},rest}:
      Nat.add(Bool.pick(Nat,String.eq(Decls.kind_name(k),kind),1n,0n),kind_count(rest,kind))

def kind_counts(+rows:List<&2,Decls.T>) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    require(Nat.is_eq(kind_count(rows,"axiom"),3n),"kind axiom count changed")
    require(Nat.is_eq(kind_count(rows,"def"),1176n),"kind def count changed")
    require(Nat.is_eq(kind_count(rows,"thm"),1649n),"kind thm count changed")
    require(Nat.is_eq(kind_count(rows,"opaque"),1n),"kind opaque count changed")
    require(Nat.is_eq(kind_count(rows,"quot"),4n),"kind quot count changed")
    require(Nat.is_eq(kind_count(rows,"inductive"),112n),"kind inductive count changed")
    require(Nat.is_eq(kind_count(rows,"constructor"),143n),"kind constructor count changed")
    require(Nat.is_eq(kind_count(rows,"recursor"),114n),"kind recursor count changed")

def axiom_set(value:List<&2,String>) -> Bool:
  match value:
    case Con{a,Con{b,Con{c,Nil{}}}}: Bool.and(String.eq(a,"Classical.choice"),Bool.and(String.eq(b,"Quot.sound"),String.eq(c,"propext")))
    case _: False{}

def declaration_stats(rows:List<&2,Decls.T>,+source:E.T,total:Nat,+axioms:List<&2,String>) -> Result<&2,&2,String,Unit>:
  match rows:
    case Nil{}: do Result<&2,&2,String,Unit>:
      require(Nat.is_eq(total,2543n),"external declaration denominator changed")
      require(axiom_set(List.sort(String,String.is_le,axioms)),"axiom set changed")
    case Con{Decls.Decl{name,levels,typ,kind,line},rest}: do Result<&2,&2,String,Unit>:
      +text : String <- name_of(source,name)
      declaration_stats(rest,source,Nat.add(total,Bool.pick(Nat,inhouse(text),0n,1n)),Bool.pick(List<&2,String>,String.eq(Decls.kind_name(kind),"axiom"),Con{text,axioms},axioms))

def constant(expr:Exprs.T,source:E.T,constants:Table.T<Unit>) -> Result<&2,&2,String,Table.T<Unit>>:
  match expr:
    case Exprs.Const{name,universes}: do Result<&2,&2,String,Table.T<Unit>>:
      text : String <- name_of(source,name)
      return Table.put(Unit,constants,text,Unit{})
    case _: Done{constants}

@unsafe def constants_loop(remaining:Nat,+index:Nat,+source:E.T,constants:Table.T<Unit>) -> Result<&2,&2,String,Table.T<Unit>>:
  match remaining:
    case Zero{}: Done{constants}
    case Succ{rest}: do Result<&2,&2,String,Table.T<Unit>>:
      expr : Exprs.T <- Table.require(Exprs.T,E.expr(source,Nat.show(index)),"missing frozen expression " ++ Nat.show(index))
      next : Table.T<Unit> <- constant(expr,source,constants)
      constants_loop(rest,Succ{index},source,next)

def external_names(names:List<&2,String>) -> Nat:
  match names:
    case Nil{}: 0n
    case Con{name,rest}: Nat.add(Bool.pick(Nat,inhouse(name),0n,1n),external_names(rest))

def corpus_table(+source:E.T) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    require(frozen_counts(E.counts(source)),"frozen table counts changed")
    kind_counts(E.declarations(source))
    declaration_stats(E.declarations(source),source,0n,[])
    +constants : Table.T<Unit> <- constants_loop(198927n,0n,source,Table.empty(Unit))
    require(Bool.and(Nat.is_eq(Table.size(Unit,constants),3017n),Nat.is_eq(external_names(Table.keys(Unit,constants)),2477n)),"const-node denominator changed")

def corpus_input(value:Result<&2,&2,String,String>) -> Result<&2,&2,String,String>:
  match value:
    case Done{text}: Done{text}
    case Fail{message}: Fail{"line 0: " ++ message}

def corpus_result(value:Result<&2,&2,String,String>) -> Result<&2,&2,String,Unit>:
  do Result<&2,&2,String,Unit>:
    text : String <- corpus_input(value)
    table : E.T <- parsed(E.read_string(text))
    corpus_table(table)

def report(value:Result<&2,&2,String,Unit>,name:String) -> IO(Nat):
  match value:
    case Done{x}: do IO<Nat>:
      IO.print("PASS import " ++ name)
      return 0n
    case Fail{message}: do IO<Nat>:
      IO.print("FAIL import " ++ name ++ ": " ++ message)
      return 1n

def run(rows:List<&2,D.Case>,failed:Nat) -> IO(Nat):
  match rows:
    case Nil{}: IO.pure(Nat,failed)
    case Con{D.Case{name,source,probe},rest}: do IO<Nat>:
      bad : Nat <- report(test(probe,E.read_string(source)),name)
      run(rest,Nat.add(failed,bad))

def verdict(failed:Nat,total:Nat) -> IO(Unit):
  match failed:
    case Zero{}: IO.print("IMPORT-OK")
    case Succ{n}: IO.die(Unit,1,"IMPORT-FAIL " ++ Nat.show(Succ{n}) ++ " of " ++ Nat.show(total) ++ " cases failed")

def corpus_report(+value:Result<&2,&2,String,Unit>) -> IO(Nat):
  match value:
    case Done{x}: do IO<Nat>:
      IO.print("CORPUS-IMPORT declarations=3202 external=2477 external_declared=2543 axioms=3")
      report(value,"frozen-corpus")
    case Fail{message}: report(value,"frozen-corpus")

def arguments(args:List<String>) -> IO(Unit):
  match args:
    case Nil{}: do IO<Unit>:
      failed : Nat <- run(D.cases(),0n)
      verdict(failed,120n)
    case Con{"--corpus",Con{path,Nil{}}}: do IO<Unit>:
      failed : Nat <- run(D.cases(),0n)
      text : Result<&2,&2,String,String> <- Files.read_file(path)
      bad : Nat <- corpus_report(corpus_result(text))
      verdict(Nat.add(failed,bad),121n)
    case _: IO.die(Unit,1,"IMPORT-FAIL usage: import.exe [--corpus FILE]")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
