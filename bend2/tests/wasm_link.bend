import Base
import ../wasm/link.bend as L
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as R
import ../kernel/error.bend as Err
import ../kernel/common.bend as C

def report(ok:Bool,name:String,expected:String,actual:String) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS " ++ name)
    case False{}: IO.die(Unit,1,name ++ ": expected " ++ expected ++ ", observed " ++ actual)

def check(name:String,+expected:String,+actual:String) -> IO(Unit):
  report(String.eq(expected,actual),name,expected,actual)

def repr(r:Result<&2,&2,Err.T,E.Repr>) -> String:
  match r:
    case Done{x}: E.print_repr(x)
    case Fail{e}: Err.to_string(e)

def reprs(rs:List<&2,E.Repr>) -> String:
  match rs:
    case Nil{}: ""
    case Con{r,Nil{}}: E.print_repr(r)
    case Con{r,rest}: E.print_repr(r) ++ ";" ++ reprs(rest)

def bindings(r:Result<&2,&2,Err.T,List<&2,E.Repr>>) -> String:
  match r:
    case Done{rs}: reprs(rs)
    case Fail{e}: Err.to_string(e)

def program() -> L.Prog:
  L.program_table([
    C.Pair{"f",R.Code{[E.KFun{E.Fid{"f"},[L.nat_repr(),L.nat_repr()],E.RFunc{E.Tid{"fn<1>"}},E.KErased{}}]}},
    C.Pair{"tree",R.Code{[E.KRec{[E.Tid{"leg<mu<Tree>,0>"},E.Tid{"leg<mu<Tree>,1,union nat,func fn<2>>"}]}]}},
    C.Pair{"axiom",R.Postulate{L.nat_repr()}}])

def main() -> IO(Unit):
  do IO<Unit>:
    check("partial","func fn<1>",repr(L.infer(program(),[],E.KApp{E.KGlobal{"f"},[E.KErased{}]})))
    check("exact","func fn<1>",repr(L.infer(program(),[],E.KApp{E.KGlobal{"f"},[E.KErased{},E.KErased{}]})))
    check("over","union any",repr(L.infer(program(),[],E.KApp{E.KGlobal{"f"},[E.KErased{},E.KErased{},E.KErased{}]})))
    check("zero specialized","union any",repr(L.steps_result(E.RFunc{E.Tid{"fn<0>"}},0n)))
    check("mu fields","func fn<2>;union nat",bindings(L.branch_binders(program(),E.RUnion{E.Tid{"mu<Tree>"}},E.Branch{1n,2n,E.KErased{}})))
    check("nested fields","struct pair<union nat,func fn<2>>;union sum<unit|union nat>",bindings(L.fields_of("tuple<struct pair<union nat,func fn<2>>,union sum<unit|union nat>>")))
    check("capture","union nat",bindings(L.capture_reprs(program(),"f",1n)))
    check("unbound","unbound: KVar 3 is out of scope",repr(L.infer(program(),[],E.KVar{3n})))
    check("postulate","unbound: axiom axiom has no body",repr(L.infer(program(),[],E.KGlobal{"axiom"})))
    check("force","not yet: KForce arrives at M2",repr(L.infer(program(),[],E.KForce{E.KErased{}})))

