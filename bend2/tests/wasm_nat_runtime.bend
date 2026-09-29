import Base
import ../wasm/link.bend as L
import ../wasm/gc_encode.bend as G
import ../wasm/nat_runtime.bend as N
import ../kernel/common.bend as C
import ../kernel/error.bend as Err

def cat(a:List<&2,G.Instr>,b:List<&2,G.Instr>) -> List<&2,G.Instr>:
  List.append(&2,G.Instr,a,b)

def funcs(+l:L.T,fs:List<&2,C.Pair<String,L.Fspec>>) -> Result<&2,&2,Err.T,List<&2,G.Func>>:
  match fs:
    case Nil{}: Done{[]}
    case Con{C.Pair{k,L.FSRuntime{name}},rest}:
      do Result<&2,&2,Err.T,List<&2,G.Func>>:
        f : G.Func <- N.runtime_func(l,name)
        tail : List<&2,G.Func> <- funcs(l,rest)
        return Con{f,tail}
    case Con{C.Pair{k,other},rest}: Fail{Err.Mismatch{"unexpected runtime fixture function"}}

def wrapper(+l:L.T,ft:U32,name:String) -> Result<&2,&2,Err.T,G.Func>:
  do Result<&2,&2,Err.T,G.Func>:
    op : U32 <- L.func_index(l,"nat-runtime:" ++ name)
    digit : U32 <- L.func_index(l,"nat-runtime:digit")
    return G.Func{ft,[],[G.Local_get{0},G.Ref_i31{},G.Local_get{1},G.Ref_i31{},G.Call{op},G.Local_get{2},G.Call{digit}]}

def wrappers(+l:L.T,+ft:U32,ns:List<&2,String>) -> Result<&2,&2,Err.T,List<&2,G.Func>>:
  match ns:
    case Nil{}: Done{[]}
    case Con{n,rest}:
      do Result<&2,&2,Err.T,List<&2,G.Func>>:
        f : G.Func <- wrapper(l,ft,n)
        tail : List<&2,G.Func> <- wrappers(l,ft,rest)
        return Con{f,tail}

def exports(ns:List<&2,String>,+i:U32) -> List<&2,G.Export>:
  match ns:
    case Nil{}: []
    case Con{n,rest}: Con{G.Export{n,i},exports(rest,U32.add(i,1))}

def module(+l:L.T) -> Result<&2,&2,Err.T,List<&2,U32>>:
  match l:
    case L.Linked{p,ts,gs,tm,+fs,fm,a,k,d}:
      do Result<&2,&2,Err.T,List<&2,U32>>:
        groups : List<&2,List<&2,G.Comptype>> <- L.comptype_groups(l)
        rs : List<&2,G.Func> <- funcs(l,fs)
        ws : List<&2,G.Func> <- wrappers(l,U32.from_nat(List.length(&2,C.Pair<String,L.Tspec>,ts)),["natAdd","natSub","natMul","natEq","natLt"])
        return G.encode(G.Module{List.append(&2,List<&2,G.Comptype>,groups,[[G.CFunc{[G.I32{},G.I32{},G.I32{}],[G.I32{}]}]]),[],List.append(&2,G.Func,rs,ws),exports(["natAdd","natSub","natMul","natEq","natLt"],U32.from_nat(List.length(&2,C.Pair<String,L.Fspec>,fs))),[]})

def bytes_text(xs:List<&2,U32>) -> String:
  match xs:
    case Nil{}: ""
    case Con{x,Nil{}}: U32.show(x)
    case Con{x,rest}: U32.show(x) ++ "," ++ bytes_text(rest)

def output(r:Result<&2,&2,Err.T,List<&2,U32>>) -> IO(Unit):
  match r:
    case Fail{e}: IO.die(Unit,1,Err.to_string(e))
    case Done{bs}: IO.print("[" ++ bytes_text(bs) ++ "]")

def main() -> IO(Unit):
  output(module(L.finish_layout(L.Prog{[],[],[]},L.nat_runtime_keys())))
