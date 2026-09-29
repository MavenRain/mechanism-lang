import Base
import ../io.bend as F
import ../cli/host.bend as Host
import ../cli/output.bend as Out
import ../surface/elab.bend as E
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/error.bend as Error
import ../kernel/budget.bend as B
import ../kernel/bignum.bend as Big
import ../kernel/erase.bend as Erase
import ../kernel/eval.bend as Eval
import ../kernel/term.bend as Term
import ../kernel/value.bend as V
import ../kernel/literal.bend as L
import ../wasm/emit.bend as Emit

def wasm_fixture_io(operation:U32,first:String,second:String) -> IO(String):
  import "./effects/wasm_fixture.c"
  import "./effects/wasm_fixture.js"

def foreign_result(text:String) -> Result<&2,&2,String,Unit>:
  match text:
    case SCon{Chr{43},rest}: Done{Unit{}}
    case SCon{Chr{45},message}: Fail{"validate: " ++ message}
    case other: Fail{"invalid fixture effect response"}

def require(ok:Bool,message:String) -> Result<&2,&2,String,Unit>:
  Bool.pick(Result<&2,&2,String,Unit>,ok,Done{Unit{}},Fail{message})

def defines_main(rows:List<&2,C.Pair<String,G.Entry>>) -> Bool:
  match rows:
    case Nil{}: False{}
    case Con{C.Pair{name,entry},rest}: Bool.or(String.eq(name,"main"),defines_main(rest))

def emitted(globals:G.T,rows:List<&2,C.Pair<String,G.Entry>>) -> Result<&2,&2,Error.T,List<&2,U32>>:
  do Result<&2,&2,Error.T,List<&2,U32>>:
    erased : List<&2,C.Pair<String,Erase.Entry>> <- Erase.program(globals,rows)
    Emit.program(erased,"main")

def value(result:Result<&2,&2,Error.T,V.T>) -> Result<&2,&2,String,Big.T>:
  match result:
    case Fail{e}: Fail{Error.to_string(e)}
    case Done{V.VLit{L.LInt{n}}}: Done{n}
    case Done{other}: Fail{"kernel value not a literal"}

def weak(+globals:G.T,result:Result<&2,&2,Error.T,V.T>) -> Result<&2,&2,Error.T,V.T>:
  match result:
    case Fail{e}: Fail{e}
    case Done{v}: Eval.whnf(globals,v)

def kernel_value(+globals:G.T) -> Result<&2,&2,String,Big.T>:
  value(weak(globals,Eval.eval(globals,[],Term.Global{"main"})))

def observed(empty:Bool,code:U32,text:String) -> String:
  match empty:
    case True{}: "exit " ++ U32.show(code)
    case False{}: text

def answer(expected:Maybe<&2,U32>,+code:U32,+printed:String) -> Result<&2,&2,String,Unit>:
  match expected:
    case None{}:
      require(Bool.and(U32.is_eq(code,1),String.is_empty(printed)),"node: expected a trap got " ++ observed(String.is_empty(printed),code,printed))
    case Some{+n}:
      require(Bool.and(U32.is_eq(code,0),String.eq(printed,U32.show(n))),"node: expected " ++ U32.show(n) ++ " got " ++ observed(String.is_empty(printed),code,printed))

def output(expected:Maybe<&2,U32>,code:U32,r:Result<&2,&2,String,String>) -> Result<&2,&2,String,Unit>:
  match r:
    case Fail{message}: Fail{"node: " ++ message}
    case Done{text}: answer(expected,code,String.trim(text))

def run_expected(expected:Result<&2,&2,String,Big.T>,+base:String,repo:String) -> IO(Result<&2,&2,String,Unit>):
  match expected:
    case Fail{message}: IO.pure(Result<&2,&2,String,Unit>,Fail{message})
    case Done{n}: do IO<Result<&2,&2,String,Unit>>:
      code : U32 <- Host.host_process("node",repo ++ "/dev/run-node.mjs",base ++ ".wasm","main",base ++ ".out",base ++ ".nodeerr")
      printed : Result<&2,&2,String,String> <- F.read_file(base ++ ".out")
      return output(Big.to_i31(n),code,printed)

def golden_same(a:Result<&2,&2,String,String>,b:Result<&2,&2,String,String>) -> Result<&2,&2,String,Unit>:
  match a b:
    case Done{x} Done{y}: require(String.eq(x,y),"golden differs")
    case _ _: Fail{"golden differs"}

def golden_checked(r:Result<&2,&2,String,Unit>,globals:G.T,base:String,repo:String) -> IO(Result<&2,&2,String,Unit>):
  match r:
    case Fail{message}: IO.pure(Result<&2,&2,String,Unit>,Fail{message})
    case Done{u}: run_expected(kernel_value(globals),base,repo)

def validated(r:Result<&2,&2,String,Unit>,root:String,name:String,+base:String,globals:G.T,repo:String) -> IO(Result<&2,&2,String,Unit>):
  match r:
    case Fail{message}: IO.pure(Result<&2,&2,String,Unit>,Fail{message})
    case Done{u}: do IO<Result<&2,&2,String,Unit>>:
      got : Result<&2,&2,String,String> <- F.read_file(base ++ ".wat")
      want : Result<&2,&2,String,String> <- F.read_file(root ++ "/golden/" ++ name ++ ".wat")
      golden_checked(golden_same(got,want),globals,base,repo)

def written(r:Result<&2,&2,String,Unit>,root:String,name:String,+base:String,globals:G.T,repo:String) -> IO(Result<&2,&2,String,Unit>):
  match r:
    case Fail{message}: IO.pure(Result<&2,&2,String,Unit>,Fail{"emit: " ++ message})
    case Done{u}: do IO<Result<&2,&2,String,Unit>>:
      validation : String <- wasm_fixture_io(1,base,"")
      validated(foreign_result(validation),root,name,base,globals,repo)

def emit_result(r:Result<&2,&2,Error.T,List<&2,U32>>,root:String,name:String,+base:String,globals:G.T,repo:String) -> IO(Result<&2,&2,String,Unit>):
  match r:
    case Fail{e}: IO.pure(Result<&2,&2,String,Unit>,Fail{"emit: " ++ Error.to_string(e)})
    case Done{bytes}: do IO<Result<&2,&2,String,Unit>>:
      r : Result<&2,&2,String,Unit> <- F.write_bytes(base ++ ".wasm",bytes)
      written(r,root,name,base,globals,repo)

def report(name:String,r:Result<&2,&2,String,Unit>) -> IO(C.Pair<Nat,Nat>):
  match r:
    case Done{u}: do IO<C.Pair<Nat,Nat>>:
      IO.print("EMIT " ++ name ++ " OK")
      return C.Pair{1n,1n}
    case Fail{message}: do IO<C.Pair<Nat,Nat>>:
      IO.print("EMIT " ++ name ++ " FAIL: " ++ message)
      return C.Pair{0n,1n}

def selected(yes:Bool,+root:String,+name:String,outdir:String,+globals:G.T,rows:List<&2,C.Pair<String,G.Entry>>) -> IO(C.Pair<Nat,Nat>):
  match yes:
    case False{}: IO.pure(C.Pair<Nat,Nat>,C.Pair{0n,0n})
    case True{}: do IO<C.Pair<Nat,Nat>>:
      r : Result<&2,&2,String,Unit> <- emit_result(emitted(globals,rows),root,name,outdir ++ "/" ++ name,globals,Host.dirname(root))
      report(name,r)

def checked(r:Result<&2,&2,Error.T,E.Checked>,root:String,name:String,outdir:String) -> IO(C.Pair<Nat,Nat>):
  match r:
    case Fail{e}: IO.pure(C.Pair<Nat,Nat>,C.Pair{0n,0n})
    case Done{C.Pair{globals,+rows}}: selected(defines_main(rows),root,name,outdir,globals,rows)

def read_result(r:Result<&2,&2,String,String>,root:String,name:String,outdir:String) -> IO(C.Pair<Nat,Nat>):
  match r:
    case Fail{message}: IO.pure(C.Pair<Nat,Nat>,C.Pair{0n,0n})
    case Done{source}: checked(E.check_in(G.initial(),B.unlimited(),source),root,name,outdir)

def counts(a:C.Pair<Nat,Nat>,b:C.Pair<Nat,Nat>) -> C.Pair<Nat,Nat>:
  match a b:
    case C.Pair{p,n} C.Pair{q,m}: C.Pair{Nat.add(p,q),Nat.add(n,m)}

def fixtures(names:List<&2,String>,+root:String,+outdir:String,total:C.Pair<Nat,Nat>) -> IO(C.Pair<Nat,Nat>):
  match names:
    case Nil{}: IO.pure(C.Pair<Nat,Nat>,total)
    case Con{"",rest}: fixtures(rest,root,outdir,total)
    case Con{+name,rest}: do IO<C.Pair<Nat,Nat>>:
      source : Result<&2,&2,String,String> <- F.read_file(root ++ "/fixtures/" ++ name ++ ".kan")
      result : C.Pair<Nat,Nat> <- read_result(source,root,name,outdir)
      fixtures(rest,root,outdir,counts(total,result))

def finish(ok:Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("SUITE-WASM OK")
    case False{}: do IO<Unit>:
      IO.print("SUITE-WASM FAIL")
      Out.cli_exit(1)

def fail_out(message:String) -> IO(Unit):
  do IO<Unit>:
    IO.print("SUITE " ++ message)
    finish(False{})

def verdict(total:C.Pair<Nat,Nat>) -> IO(Unit):
  match total:
    case C.Pair{+passed,+total}: do IO<Unit>:
      IO.print("WASM-OK " ++ Nat.show(passed) ++ "/" ++ Nat.show(total))
      finish(Bool.and(Nat.is_eq(passed,total),Nat.is_gt(total,0n)))

def listed(result:String,root:String,outdir:String) -> IO(Unit):
  match result:
    case SCon{Chr{43},names}: do IO<Unit>:
      total : C.Pair<Nat,Nat> <- fixtures(String.split(names,Chr{10}),root,outdir,C.Pair{0n,0n})
      verdict(total)
    case SCon{Chr{45},message}: fail_out(message)
    case other: fail_out("invalid fixture listing")

def run(+root:String,+outdir:String) -> IO(Unit):
  do IO<Unit>:
    names : String <- wasm_fixture_io(0,root,outdir)
    listed(names,root,outdir)

def arguments(args:List<String>) -> IO(Unit):
  match args:
    case Nil{}: run("test","_build/wasm-suite")
    case Con{+root,Nil{}}: run(root,Host.dirname(root) ++ "/_build/wasm-suite")
    case Con{root,Con{outdir,rest}}: run(root,outdir)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
