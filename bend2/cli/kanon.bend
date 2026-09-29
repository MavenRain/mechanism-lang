import Base
import ../io.bend as F
import ./host.bend as Host
import ./output.bend as Out
import ../kernel/common.bend as C
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/error.bend as Error
import ../kernel/budget.bend as Budget
import ../kernel/bignum.bend as Big
import ../kernel/erase.bend as Erase
import ../kernel/circuit.bend as Circuit
import ../kernel/spec_count.bend as SpecCount
import ../surface/elab.bend as Elab
import ../wasm/emit.bend as Emit

def Rows() -> Data:
  List<&2,C.Pair<String,Global.Entry>>

def Checked() -> Data:
  C.Pair<Global.T,Rows>

def Erased() -> Data:
  List<&2,C.Pair<String,Erase.Entry>>

def usage_text() -> String:
  "usage: kanon check [--print|--erased] FILE | axioms FILE | circuit FILE | emit FILE -o OUT.wasm --export NAME | run FILE --export NAME [--host node|wasmtime|kernel|both] | build FILE... -o OUT.wasm --export NAME... | spec-count"

def usage() -> IO(Unit):
  IO.die(Unit,64,usage_text())

def accept(-A:Data,result:Result<&2,&2,Error.T,A>,code:U32,prefix:String) -> IO(A):
  match result:
    case Done{x}: IO.pure(A,x)
    case Fail{e}: IO.die(A,code,prefix ++ Error.to_string(e))

def read_result(result:Result<&2,&2,String,String>,path:String) -> IO(String):
  match result:
    case Done{text}: IO.pure(String,text)
    case Fail{e}: IO.die(String,64,"kanon: cannot read " ++ path)

def read_file(+path:String) -> IO(String):
  do IO<String>:
    result : Result<&2,&2,String,String> <- F.read_file(path)
    read_result(result,path)

def checked_in(path:String) -> IO(Checked):
  do IO<Checked>:
    source : String <- read_file(path)
    accept(Checked,Elab.check_in(Global.initial(),Budget.unlimited(),source),1,"")

def checked_rows(c:Checked) -> Rows:
  match c:
    case C.Pair{g,rows}: rows

def checked(path:String) -> IO(Rows):
  do IO<Rows>:
    c : Checked <- checked_in(path)
    return checked_rows(c)

def print_checked(print_form:Bool,rows:Rows) -> IO(Unit):
  match print_form:
    case True{}: IO.write(Elab.checked_form(rows))
    case False{}: IO.pure(Unit,Unit{})

def run_check(print_form:Bool,path:String) -> IO(Unit):
  do IO<Unit>:
    rows : Rows <- checked(path)
    print_checked(print_form,rows)

def erase_checked(c:Checked) -> IO(Erased):
  match c:
    case C.Pair{g,rows}: accept(Erased,Erase.program(g,rows),1,"")

def run_erased(path:String) -> IO(Unit):
  do IO<Unit>:
    c : Checked <- checked_in(path)
    erased : Erased <- erase_checked(c)
    IO.write(Erase.print(erased))

def print_lines(lines:List<&2,String>) -> IO(Unit):
  match lines:
    case Nil{}: IO.pure(Unit,Unit{})
    case Con{line,rest}:
      do IO<Unit>:
        IO.print(line)
        print_lines(rest)

def run_axioms(path:String) -> IO(Unit):
  do IO<Unit>:
    rows : Rows <- checked(path)
    print_lines(Elab.disclosure_names(rows))

def ensure_output(found:Bool,out:String) -> IO(Unit):
  match found:
    case True{}: IO.pure(Unit,Unit{})
    case False{}: IO.die(Unit,64,"kanon: cannot write " ++ out)

def ensure_write(+out:String) -> IO(Unit):
  do IO<Unit>:
    found : Bool <- F.exists(Host.dirname(out))
    ensure_output(found,out)

def wrote(result:Result<&2,&2,String,Unit>,out:String) -> IO(Unit):
  match result:
    case Done{u}: IO.pure(Unit,Unit{})
    case Fail{e}: IO.die(Unit,64,"kanon: cannot write " ++ out)

def write_bytes(+out:String,bytes:List<&2,U32>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2,&2,String,Unit> <- F.write_bytes(out,bytes)
    wrote(result,out)

def run_emit(path:String,+out:String,export:String) -> IO(Unit):
  do IO<Unit>:
    ensure_write(out)
    c : Checked <- checked_in(path)
    erased : Erased <- erase_checked(c)
    bytes : List<&2,U32> <- accept(List<&2,U32>,Emit.program(erased,export),2,"kanon: emit: ")
    write_bytes(out,bytes)

def dispatch_emit(args:List<&2,String>) -> IO(Unit):
  match args:
    case Con{path,Con{"-o",Con{out,Con{"--export",Con{name,Nil{}}}}}}: run_emit(path,out,name)
    case _: usage()

def read_files(paths:List<&2,String>) -> IO(String):
  match paths:
    case Nil{}: IO.pure(String,"")
    case Con{p,Nil{}}: read_file(p)
    case Con{p,rest}:
      do IO<String>:
        first : String <- read_file(p)
        more : String <- read_files(rest)
        return first ++ "\n" ++ more

def run_build(paths:List<&2,String>,+out:String,exports:List<&2,String>) -> IO(Unit):
  do IO<Unit>:
    ensure_write(out)
    source : String <- read_files(paths)
    c : Checked <- accept(Checked,Elab.check_in(Global.initial(),Budget.unlimited(),source),1,"")
    rows : Erased <- erase_checked(c)
    bytes : List<&2,U32> <- accept(List<&2,U32>,Emit.reactor(rows,exports),2,"kanon: build: ")
    write_bytes(out,bytes)

def build_finish(paths:List<&2,String>,out:Maybe<&2,String>,exports:List<&2,String>) -> IO(Unit):
  match paths out exports:
    case Nil{} _ _: usage()
    case _ None{} _: usage()
    case _ _ Nil{}: usage()
    case _ Some{output} _: run_build(List.reverse(&2,String,paths),output,List.reverse(&2,String,exports))

law build_flags:
  for paths:List<&2,String>
  for out:Maybe<&2,String>
  for exports:List<&2,String>
  for args:List<&2,String>
  IO(Unit)

@unsafe def build_out(out:Maybe<&2,String>,paths:List<&2,String>,output:String,exports:List<&2,String>,rest:List<&2,String>) -> IO(Unit):
  match out:
    case None{}: build_flags(paths,Some{output},exports,rest)
    case Some{o}: usage()

@unsafe def build_path(flag:Bool,path:String,paths:List<&2,String>,out:Maybe<&2,String>,exports:List<&2,String>,rest:List<&2,String>) -> IO(Unit):
  match flag:
    case True{}: usage()
    case False{}: build_flags(path <> paths,out,exports,rest)

@unsafe def build_flags(paths,out,exports,args):
  match args:
    case Nil{}: build_finish(paths,out,exports)
    case Con{"-o",Con{output,rest}}: build_out(out,paths,output,exports,rest)
    case Con{"--export",Con{name,rest}}: build_flags(paths,out,name <> exports,rest)
    case Con{"-o",Nil{}}: usage()
    case Con{"--export",Nil{}}: usage()
    case Con{+path,rest}: build_path(String.starts_with(path,"-"),path,paths,out,exports,rest)

def dispatch_build(args:List<&2,String>) -> IO(Unit):
  build_flags([],None{},[],args)

type Mode is Data:
  One{host:Host.T}
  Both{}

def add_rows(rows:Rows,g:Global.T) -> Global.T:
  match rows:
    case Nil{}: g
    case Con{C.Pair{n,e},rest}: add_rows(rest,Global.add(n,e,g))

def globals_of(rows:Rows) -> Global.T:
  add_rows(rows,Global.initial())

def module_checked(c:Checked,export:String) -> IO(C.Pair<Global.T,List<&2,U32>>):
  match c:
    case C.Pair{globals,+rows}:
      do IO<C.Pair<Global.T,List<&2,U32>>>:
        erased : Erased <- accept(Erased,Erase.program(globals,rows),1,"")
        bytes : List<&2,U32> <- accept(List<&2,U32>,Emit.program(erased,export),2,"kanon: emit: ")
        return C.Pair{globals_of(rows),bytes}

def module_bytes(path:String,export:String) -> IO(C.Pair<Global.T,List<&2,U32>>):
  do IO<C.Pair<Global.T,List<&2,U32>>>:
    c : Checked <- checked_in(path)
    module_checked(c,export)

def cleanup(paths:List<&2,String>) -> IO(Unit):
  match paths:
    case Nil{}: IO.pure(Unit,Unit{})
    case Con{+path,rest}:
      do IO<Unit>:
        exists : Bool <- F.exists(path)
        r : Result<&2,&2,String,Unit> <- F.remove_if_exists(exists,path)
        cleanup(rest)

def answer(n:Big.T) -> IO(Unit):
  IO.print(Big.to_string(n))

def refused(h:Host.T,m:String) -> IO(Unit):
  IO.die(Unit,2,"kanon: run: " ++ Host.name(h) ++ " invalid: " ++ m)

def show(o:Host.Outcome) -> String:
  match o:
    case Host.Value{n}: Big.to_string(n)
    case Host.Trap{m}: "trap"
    case Host.Invalid{m}: "invalid"

def disagree(a:Host.Outcome,b:Host.Outcome) -> IO(Unit):
  IO.die(Unit,3,"kanon: run: hosts disagree: node " ++ show(a) ++ " wasmtime " ++ show(b))

def report_one(h:Host.T,o:Host.Outcome) -> IO(Unit):
  match o:
    case Host.Value{n}: answer(n)
    case Host.Trap{text}: IO.die(Unit,4,"kanon: run: " ++ Host.name(h) ++ " trap: " ++ text)
    case Host.Invalid{text}: refused(h,text)

def report_equal(same:Bool,x:Big.T,a:Host.Outcome,b:Host.Outcome) -> IO(Unit):
  match same:
    case True{}: answer(x)
    case False{}: disagree(a,b)

def report_both(+a:Host.Outcome,+b:Host.Outcome) -> IO(Unit):
  match a b:
    case Host.Value{x} Host.Value{y}: report_equal(Big.equal(x,y),x,a,b)
    case Host.Value{x} Host.Trap{m}: disagree(a,b)
    case Host.Trap{m} Host.Value{x}: disagree(a,b)
    case Host.Trap{x} Host.Trap{y}: IO.die(Unit,4,"kanon: run: trap on both hosts")
    case Host.Value{x} Host.Invalid{text}: refused(Host.Wasmtime{},text)
    case Host.Trap{x} Host.Invalid{text}: refused(Host.Wasmtime{},text)
    case Host.Invalid{text} _: refused(Host.Node{},text)

def need_missing(m:Maybe<&2,String>) -> IO(Unit):
  match m:
    case None{}: IO.pure(Unit,Unit{})
    case Some{line}: IO.die(Unit,64,line)

def need_runners(hosts:List<&2,Host.T>,+root:String) -> IO(Unit):
  match hosts:
    case Nil{}: IO.pure(Unit,Unit{})
    case Con{h,rest}:
      do IO<Unit>:
        missing : Maybe<&2,String> <- Host.missing_runner(root,h)
        need_missing(missing)
        need_runners(rest,root)

def mode_hosts(m:Mode) -> List<&2,Host.T>:
  match m:
    case One{h}: [h]
    case Both{}: [Host.Node{},Host.Wasmtime{}]

def run_hosts(m:Mode,+root:String,+globals:Global.T,+wasm:String,+export:String) -> IO(Unit):
  match m:
    case One{+h}:
      do IO<Unit>:
        o : Host.Outcome <- Host.run_module(root,globals,h,wasm,export)
        cleanup(wasm <> Host.captures(wasm))
        report_one(h,o)
    case Both{}:
      do IO<Unit>:
        node : Host.Outcome <- Host.run_module(root,globals,Host.Node{},wasm,export)
        wasmtime : Host.Outcome <- Host.run_module(root,globals,Host.Wasmtime{},wasm,export)
        cleanup(wasm <> Host.captures(wasm))
        report_both(node,wasmtime)

def temp_result(result:Result<&2,&2,String,String>) -> IO(String):
  match result:
    case Done{path}: IO.pure(String,path)
    case Fail{message}: IO.die(String,64,"kanon: run: " ++ message)

def run_compiled(c:C.Pair<Global.T,List<&2,U32>>,root:String,export:String,m:Mode) -> IO(Unit):
  match c:
    case C.Pair{globals,bytes}:
      do IO<Unit>:
        temp : Result<&2,&2,String,String> <- F.temp_file("kanon-run-",".wasm")
        +wasm : String <- temp_result(temp)
        write_bytes(wasm,bytes)
        run_hosts(m,root,globals,wasm,export)

def run_existing(exists:Bool,path:String,+export:String,+m:Mode) -> IO(Unit):
  match exists:
    case False{}: IO.die(Unit,64,"kanon: run: cannot read " ++ path ++ "\n" ++ usage_text())
    case True{}:
      do IO<Unit>:
        +root : String <- Host.root()
        need_runners(mode_hosts(m),root)
        c : C.Pair<Global.T,List<&2,U32>> <- module_bytes(path,export)
        run_compiled(c,root,export,m)

def run_run(+path:String,export:String,m:Mode) -> IO(Unit):
  do IO<Unit>:
    exists : Bool <- F.exists(path)
    run_existing(exists,path,export,m)

def dispatch_run(args:List<&2,String>) -> IO(Unit):
  match args:
    case Con{path,Con{"--export",Con{name,Nil{}}}}: run_run(path,name,Both{})
    case Con{path,Con{"--export",Con{name,Con{"--host",Con{"node",Nil{}}}}}}: run_run(path,name,One{Host.Node{}})
    case Con{path,Con{"--export",Con{name,Con{"--host",Con{"wasmtime",Nil{}}}}}}: run_run(path,name,One{Host.Wasmtime{}})
    case Con{path,Con{"--export",Con{name,Con{"--host",Con{"kernel",Nil{}}}}}}: run_run(path,name,One{Host.Kernel{}})
    case Con{path,Con{"--export",Con{name,Con{"--host",Con{"both",Nil{}}}}}}: run_run(path,name,Both{})
    case _: usage()

def dispatch_check(args:List<&2,String>) -> IO(Unit):
  match args:
    case Con{"--print",Con{path,rest}}: run_check(True{},path)
    case Con{"--print",Nil{}}: usage()
    case Con{"--erased",Con{path,rest}}: run_erased(path)
    case Con{"--erased",Nil{}}: usage()
    case Con{path,rest}: run_check(False{},path)
    case Nil{}: usage()

def dispatch_axioms(args:List<&2,String>) -> IO(Unit):
  match args:
    case Con{path,rest}: run_axioms(path)
    case Nil{}: usage()

def definition_rows(rows:Rows) -> List<&2,C.Pair<String,Term.T>>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{n,Global.Def{Global.DefEntry{ty,body,red,recarg,partial}}},rest}: C.Pair{n,body} <> definition_rows(rest)
    case Con{x,rest}: definition_rows(rest)

def global_lookup(g:Global.T) -> List<&2,C.Pair<String,Term.T>>:
  match g:
    case Global.Globals{entries,families, entry_index, family_index}: definition_rows(entries)

def circuit_line(name:String,result:Result<&2,&2,String,Big.T>,bad:Bool) -> IO(Bool):
  match result:
    case Done{n}:
      do IO<Bool>:
        IO.print(name ++ ": depth " ++ Big.to_string(n))
        return bad
    case Fail{head}:
      do IO<Bool>:
        IO.print(name ++ ": refused: " ++ head)
        return True{}

def circuit_rows(rows:Rows,+globals:List<&2,C.Pair<String,Term.T>>,bad:Bool) -> IO(Bool):
  match rows:
    case Nil{}: IO.pure(Bool,bad)
    case Con{C.Pair{n,Global.Def{Global.DefEntry{ty,body,red,recarg,partial}}},rest}:
      do IO<Bool>:
        refused : Bool <- circuit_line(n,Circuit.depth(globals,body),bad)
        circuit_rows(rest,globals,refused)
    case Con{x,rest}: circuit_rows(rest,globals,bad)

def circuit_exit(bad:Bool) -> IO(Unit):
  match bad:
    case False{}: IO.pure(Unit,Unit{})
    case True{}: Out.cli_exit(1)

def circuit_checked(c:Checked) -> IO(Unit):
  match c:
    case C.Pair{globals,rows}:
      do IO<Unit>:
        bad : Bool <- circuit_rows(rows,global_lookup(globals),False{})
        circuit_exit(bad)

def run_circuit(path:String) -> IO(Unit):
  do IO<Unit>:
    c : Checked <- checked_in(path)
    circuit_checked(c)

def dispatch_circuit(args:List<&2,String>) -> IO(Unit):
  match args:
    case Con{path,rest}: run_circuit(path)
    case Nil{}: usage()

def dispatch_args(cmd:String,args:List<&2,String>) -> IO(Unit):
  match cmd:
    case "spec-count": IO.write(SpecCount.print())
    case "check": dispatch_check(args)
    case "axioms": dispatch_axioms(args)
    case "circuit": dispatch_circuit(args)
    case "emit": dispatch_emit(args)
    case "build": dispatch_build(args)
    case "run": dispatch_run(args)
    case _: usage()

def reusable(args:List<String>) -> List<&2,String>:
  match args:
    case Nil{}: []
    case Con{x,rest}: x <> reusable(rest)

def dispatch(cmd:String,args:List<String>) -> IO(Unit):
  dispatch_args(cmd,reusable(args))
