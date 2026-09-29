import Base
import ../io.bend as F
import ../surface/elab.bend as E
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/error.bend as Error
import ../kernel/budget.bend as B
import ../kernel/positivity.bend as P

def require(ok:Bool,message:String) -> Result<&2,&2,String,Unit>:
  Bool.pick(Result<&2,&2,String,Unit>,ok,Done{Unit{}},Fail{message})

def checked_result(r:Result<&2,&2,Error.T,E.Checked>) -> Result<&2,&2,String,G.T>:
  match r:
    case Fail{e}: Fail{Error.to_string(e)}
    case Done{C.Pair{globals,rows}}: Done{globals}

def checked(globals:G.T,source:String) -> Result<&2,&2,String,G.T>:
  checked_result(E.check_in(globals,B.unlimited(),source))

def entry_le(a:C.Pair<String,G.Entry>,b:C.Pair<String,G.Entry>) -> Bool:
  match a b:
    case C.Pair{x,v} C.Pair{y,w}: String.is_le(x,y)

def no_entries(rows:List<&2,C.Pair<String,G.Entry>>) -> Result<&2,&2,String,Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{name,G.Def{value}},rest}: no_entries(rest)
    case Con{C.Pair{name,G.Axiom{ty}},rest}: Fail{"prelude postulate: " ++ name}
    case Con{C.Pair{name,G.Prim{ty,prim}},rest}: Fail{"prelude primitive: " ++ name}

def no_postulates(globals:G.T) -> Result<&2,&2,String,Unit>:
  match globals:
    case G.Globals{entries,families, entry_index, family_index}: no_entries(List.sort(C.Pair<String,G.Entry>,entry_le,entries))

def audit(source:String) -> Result<&2,&2,String,G.T>:
  do Result<&2,&2,String,G.T>:
    +globals : G.T <- checked(G.empty(),source)
    no_postulates(globals)
    return globals

def accepted(r:Result<&2,&2,String,G.T>) -> Result<&2,&2,String,Unit>:
  match r:
    case Fail{message}: Fail{message}
    case Done{globals}: Done{Unit{}}

def clean(r:Result<&2,&2,String,G.T>) -> Result<&2,&2,String,Unit>:
  match r:
    case Fail{message}: Fail{message}
    case Done{globals}: no_postulates(globals)

def refused(r:Result<&2,&2,String,G.T>,expected:String) -> Result<&2,&2,String,Unit>:
  match r:
    case Done{globals}: Fail{"negative source was accepted"}
    case Fail{+message}: require(String.eq(message,expected),"wrong diagnostic: " ++ message)

def some(-A:Data,m:Maybe<&2,A>) -> Bool:
  match m:
    case None{}: False{}
    case Some{x}: True{}

def ctor_named(families:List<&2,C.Pair<String,P.Family>>,+name:String) -> Bool:
  match families:
    case Nil{}: False{}
    case Con{C.Pair{n,f},rest}: Bool.or(some(P.Ctor,P.ctor_of(name,f)),ctor_named(rest,name))

def declared(+globals:G.T,+name:String) -> Bool:
  match globals:
    case G.Globals{entries,+families, entry_index, family_index}:
      Bool.or(some(G.Entry,G.find_entry(entries,name)),Bool.or(some(P.Family,G.find_family_entry(families,name)),ctor_named(families,name)))

def map_cells(cells:List<&2,String>,+globals:G.T) -> C.Pair<Nat,Bool>:
  match cells:
    case Con{lean,Con{target,Con{"NAME_ONLY",Nil{}}}}: C.Pair{1n,declared(globals,target)}
    case other: C.Pair{0n,True{}}

def map_combine(a:C.Pair<Nat,Bool>,b:C.Pair<Nat,Bool>) -> C.Pair<Nat,Bool>:
  match a b:
    case C.Pair{n,x} C.Pair{m,y}: C.Pair{Nat.add(n,m),Bool.and(x,y)}

def map_lines(lines:List<&2,String>,+globals:G.T) -> C.Pair<Nat,Bool>:
  match lines:
    case Nil{}: C.Pair{0n,True{}}
    case Con{line,rest}: map_combine(map_cells(String.split(line,Chr{9}),globals),map_lines(rest,globals))

def map_checked(r:C.Pair<Nat,Bool>) -> Result<&2,&2,String,Unit>:
  match r:
    case C.Pair{n,ok}: require(Bool.and(Nat.is_gt(n,0n),ok),"NAME_ONLY target is not a checked prelude name, or no NAME_ONLY rows")

def empty(globals:G.T) -> Bool:
  match globals:
    case G.Globals{Nil{},Nil{}, entry_index, family_index}: True{}
    case other: False{}

def family_result(m:Maybe<&2,P.Family>,name:String) -> Result<&2,&2,String,Unit>:
  match m:
    case None{}: Fail{"missing checked family: " ++ name}
    case Some{P.Family{n,p,i,l,P.Complete{names},ctors,positive}}: require(positive,"nonpositive family: " ++ name)
    case Some{other}: Fail{"family was not defined by the prelude: " ++ name}

def families(names:List<&2,String>,+globals:G.T) -> List<&2,C.Pair<String,Result<&2,&2,String,Unit>>>:
  match names:
    case Nil{}: []
    case Con{+name,rest}: C.Pair{"family-" ++ name,family_result(G.find_family(name,globals),name)} <> families(rest,globals)

def report(name:String,r:Result<&2,&2,String,Unit>) -> IO(Nat):
  match r:
    case Done{u}: do IO<Nat>:
      IO.print("PASS prelude " ++ name)
      return 0n
    case Fail{message}: do IO<Nat>:
      IO.print("FAIL prelude " ++ name ++ ": " ++ message)
      return 1n

def reports(rows:List<&2,C.Pair<String,Result<&2,&2,String,Unit>>>,failed:Nat) -> IO(Nat):
  match rows:
    case Nil{}: IO.pure(Nat,failed)
    case Con{C.Pair{name,r},rest}: do IO<Nat>:
      bad : Nat <- report(name,r)
      reports(rest,Nat.add(failed,bad))

def accept(-A:Data,r:Result<&2,&2,String,A>) -> IO(A):
  match r:
    case Done{x}: IO.pure(A,x)
    case Fail{message}: IO.die(A,1,message)

def read(path:String) -> IO(String):
  do IO<String>:
    r : Result<&2,&2,String,String> <- F.read_file(path)
    accept(String,r)

def negative_result(r:Result<&2,&2,String,G.T>,+expected:String) -> Result<&2,&2,String,Unit>:
  match r:
    case Done{g}: Fail{"negative source was accepted"}
    case Fail{+message}: require(Bool.and(Bool.not(String.is_empty(expected)),String.starts_with(message,expected)),"wrong diagnostic: " ++ message)

def negatives(names:List<&2,String>,+root:String,+globals:G.T,failed:Nat) -> IO(Nat):
  match names:
    case Nil{}: IO.pure(Nat,failed)
    case Con{+name,rest}: do IO<Nat>:
      source : String <- read(root ++ "/test/neg/prelude/" ++ name ++ ".mech")
      expected : String <- read(root ++ "/test/neg/prelude/" ++ name ++ ".err")
      bad : Nat <- report("reject-" ++ name,negative_result(checked(globals,source),String.trim(expected)))
      negatives(rest,root,globals,Nat.add(failed,bad))

def passed(globals:G.T) -> IO(Unit):
  match globals:
    case G.Globals{entries,families, entry_index, family_index}:
      IO.print("PRELUDE-OK families=" ++ Nat.show(List.length(&2,C.Pair<String,P.Family>,families)) ++ " definitions=" ++ Nat.show(List.length(&2,C.Pair<String,G.Entry>,entries)) ++ " axioms=0 primitives=0")

def finish(failed:Nat,globals:G.T) -> IO(Unit):
  match failed:
    case Zero{}: passed(globals)
    case Succ{n}: IO.die(Unit,1,Nat.show(Succ{n}) ++ " prelude cases failed")

def suite(+root:String) -> IO(Unit):
  do IO<Unit>:
    +source : String <- read(root ++ "/prelude/init.mech")
    +globals : G.T <- accept(G.T,audit(source))
    map : String <- read(root ++ "/map/prelude.map.tsv")
    client : String <- read(root ++ "/test/fixtures/prelude/client.mech")
    equality : String <- read(root ++ "/test/fixtures/prelude/equality.mech")
    failed : Nat <- reports([
      C.Pair{"empty-environment",require(empty(G.empty()),"the initial environment is not empty")},
      C.Pair{"map-name-only-targets",map_checked(map_lines(String.split(map,Chr{10}),globals))},
      C.Pair{"reject-postulate",refused(audit(source ++ "\naxiom CounterfeitInit : Type 0\n"),"prelude postulate: CounterfeitInit")},
      C.Pair{"reject-hidden-builtin",refused(checked(G.empty(),"def HiddenBuiltin : Type 0 := Nat\n"),"unbound: Nat")},
      C.Pair{"duplicate-constructor-name",accepted(checked(globals,"mu AltSum (0 A : Type 0) : Type 0 with\n| mechInl : A -> AltSum A\n\ndef stillSum : MechSum MechNat MechUnit := mechInl mechZero\ndef altUse : AltSum MechBool := mechInl mechTrue\n"))},
      C.Pair{"client",clean(checked(globals,client))},
      C.Pair{"equality-client",clean(checked(globals,equality))}],0n)
    failed : Nat <- reports(families(["MechNat","MechBool","MechUnit","MechEmpty","MechFalse","MechSum","MechDecidable","MechTrue","MechEq","MechProofEq"],globals),failed)
    failed : Nat <- negatives(["eq-relevant-index","eq-erased-endpoint","eq-wrong-endpoint","eq-multiple-large-elim","eq-constructor-field","eq-type-index","nat-wrong-constructor","nat-rec-wrong-step","proof-eq-large-elim","proof-eq-wrong-endpoint","sum-missing-field","sum-wrong-parameter","sum-wrong-nested","sum-wrong-family","decidable-wrong-proof","dependent-wrong-field","indexed-wrong-result"],root,globals,failed)
    finish(failed,globals)

def audit_file(path:String) -> IO(Unit):
  do IO<Unit>:
    text : String <- read(path)
    globals : G.T <- accept(G.T,audit(text))
    IO.print("PRELUDE-AXIOMS OK")

def arguments(args:List<String>) -> IO(Unit):
  match args:
    case Con{"--audit",Con{path,Nil{}}}: audit_file(path)
    case Con{root,Nil{}}: suite(root)
    case other: IO.die(Unit,1,"usage: prelude ROOT | prelude --audit FILE")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
