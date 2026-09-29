import Base
import ../cli/host.bend as H
import ../io.bend as F
import ../wasm/gc_encode.bend as G
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as B

def checked(ok:Bool,name:String,expected:String,actual:String) -> IO(Unit):
  match ok:
    case True{}: IO.print(name ++ ": ok")
    case False{}: IO.die(Unit,1,name ++ ": expected " ++ expected ++ ", got " ++ actual)

def check(name:String,+expected:String,+actual:String) -> IO(Unit):
  checked(String.eq(expected,actual),name,expected,actual)

def result_text(r:H.Outcome) -> String:
  match r:
    case H.Value{n}: "value " ++ B.to_string(n)
    case H.Trap{s}: "trap " ++ s
    case H.Invalid{s}: "invalid " ++ s

def number_text(n:Maybe<&2,B.T>) -> String:
  match n:
    case None{}: "none"
    case Some{x}: B.to_string(x)

def missing_text(n:Maybe<&2,String>) -> String:
  match n:
    case None{}: "none"
    case Some{x}: x

def unit_result(r:Result<&2,&2,String,Unit>) -> IO(Unit):
  match r:
    case Done{u}: IO.pure(Unit,Unit{})
    case Fail{e}: IO.die(Unit,1,e)

def write(path:String,text:String) -> IO(Unit):
  do IO<Unit>:
    r : Result<&2,&2,String,Unit> <- F.write_file(path,text)
    unit_result(r)

def remove(path:String) -> IO(Unit):
  do IO<Unit>:
    r : Result<&2,&2,String,Unit> <- F.remove(path)
    unit_result(r)

def globals(n:U32) -> Global.T:
  Global.add("main",Global.Def{Global.DefEntry{Term.Global{"Nat"},Term.Lit{Lit.LInt{B.of_u32(n)}},True{},None{},False{}}},Global.initial())

def module() -> List<&2,U32>:
  G.encode(G.Module{[[G.CFunc{[],[G.I32{}]}]],[],[G.Func{0,[],[G.I32_const{42}]}],[G.Export{"main",0},G.Export{"quoted ' ;$()",0}],[]})

def custom_script() -> String:
  "const op=process.argv[3]; if(op==='value'){console.log('  -42 ')}else if(op==='trap'){console.error('\\n  trap: deliberate\\nignored');process.exit(1)}else if(op==='invalid'){console.error('\\ninvalid: rejected');process.exit(2)}else{console.log('not a number')}"

def custom(+wasm:String,name:String,expected:String) -> IO(Unit):
  do IO<Unit>:
    result : H.Outcome <- H.spawn(H.Runner{"node","mechanism host ' ;$().js"},"/private/tmp",wasm,name)
    check("custom capture",expected,result_text(result))

def main() -> IO(Unit):
  +wasm = {"/private/tmp/mechanism host ' ;$().wasm":String}
  do IO<Unit>:
    check("signed number","-42",number_text(H.parse_number("-42")))
    check("positive sign","42",number_text(H.parse_number("+42")))
    check("underscores","12",number_text(H.parse_number("1__2_")))
    check("bad underscore","none",number_text(H.parse_number("_1")))
    check("base hex","42",number_text(H.parse_number("0x2a")))
    check("base binary","2",number_text(H.parse_number("0b1_0")))
    check("unsigned wrap","-4611686018427387904",number_text(H.parse_number("0u4611686018427387904")))
    check("signed minimum","-4611686018427387904",number_text(H.parse_number("-4611686018427387904")))
    check("signed overflow","none",number_text(H.parse_number("4611686018427387904")))
    check("unsigned overflow","none",number_text(H.parse_number("0xffffffffffffffff")))
    check("relative dirname","a",H.dirname("a//b/"))
    check("root dirname","/",H.dirname("/a"))
    check("empty dirname",".",H.dirname(""))
    absent : Maybe<&2,String> <- H.missing_runner("/private/tmp/mechanism-no-runner-directory",H.Node{})
    check("missing runner","kanon: run: cannot find dev/run-node.mjs beside the executable",missing_text(absent))
    kernel_runner : Maybe<&2,String> <- H.missing_runner("/private/tmp/mechanism-no-runner-directory",H.Kernel{})
    check("kernel needs no runner","none",missing_text(kernel_runner))
    check("kernel value","value 42",result_text(H.kernel_outcome(globals(42),"main")))
    check("kernel bound","trap 1073741824 is outside the i31 range",result_text(H.kernel_outcome(globals(1073741824),"main")))
    written : Result<&2,&2,String,Unit> <- F.write_bytes(wasm,module())
    unit_result(written)
    node : H.Outcome <- H.run_module(".",Global.initial(),H.Node{},wasm,"main")
    check("Node","value 42",result_text(node))
    quoted : H.Outcome <- H.run_module(".",Global.initial(),H.Node{},wasm,"quoted ' ;$()")
    check("quoted argv","value 42",result_text(quoted))
    wasmtime : H.Outcome <- H.run_module(".",Global.initial(),H.Wasmtime{},wasm,"main")
    check("Wasmtime","value 42",result_text(wasmtime))
    write("/private/tmp/mechanism host ' ;$().js",custom_script())
    custom(wasm,"value","value -42")
    custom(wasm,"trap","trap deliberate")
    custom(wasm,"invalid","invalid rejected")
    custom(wasm,"bad","invalid runner printed not a number")
    remove("/private/tmp/mechanism host ' ;$().js")
    remove(H.out_path(wasm))
    remove(H.err_path(wasm))
    remove(wasm)
