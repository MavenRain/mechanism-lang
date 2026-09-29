import Base
import ../wasm/link.bend as L
import ../wasm/emit.bend as Emit
import ../wasm/gc_encode.bend as G
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as R
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as Err

def literal() -> E.Ktm:
  E.KLit{Lit.LInt{B.of_u32(42)}}

def basic() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[E.KFun{E.Fid{"main"},[],L.nat_repr(),literal()}]}}]

def recursive() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[
    E.KRec{[E.Tid{"leg<mu<Tree>,0>"},E.Tid{"leg<mu<Tree>,1,union mu<Tree>>"},E.Tid{"leg<mu<Tree>,2,union mu<Tree>>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"mu<Tree>"}},E.KTag{E.Tid{"mu<Tree>"},0n,[]}}]}}]

def closure() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[
    E.KFun{E.Fid{"lift"},[L.nat_repr(),L.nat_repr()],L.nat_repr(),E.KVar{1n}},
    E.KFun{E.Fid{"main"},[],L.nat_repr(),E.KApp{E.KClos{E.Fid{"lift"},1n,[literal()]},[literal()]}}]}}]

def bytes_text(xs:List<&2,U32>) -> String:
  match xs:
    case Nil{}: ""
    case Con{x,Nil{}}: U32.show(x)
    case Con{x,rest}: U32.show(x) ++ "," ++ bytes_text(rest)

def layout(rows:List<&2,C.Pair<String,R.Entry>>) -> Result<&2,&2,Err.T,List<&2,U32>>:
  Emit.program(rows,"main")

def output(r:Result<&2,&2,Err.T,List<&2,U32>>) -> IO(Unit):
  match r:
    case Fail{e}: IO.die(Unit,1,Err.to_string(e))
    case Done{bs}: IO.print("[" ++ bytes_text(bs) ++ "]")

def partial() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[
    E.KFun{E.Fid{"add"},[L.nat_repr(),L.nat_repr()],L.nat_repr(),E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"main"},[],L.nat_repr(),E.KLet{"f",E.KApp{E.KGlobal{"add"},[E.KLit{Lit.LInt{B.of_u32(40)}}]},E.KApp{E.KVar{0n},[E.KLit{Lit.LInt{B.of_u32(2)}}]}}}
  ]}}]

def tag_case() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[
    E.KRec{[E.Tid{"leg<mu<Tree>,0>"},E.Tid{"leg<mu<Tree>,1,union nat>"}]},
    E.KFun{E.Fid{"main"},[],L.nat_repr(),E.KCase{E.Tid{"mu<Tree>"},E.KTag{E.Tid{"mu<Tree>"},1n,[literal()]},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.zero()}}},E.Branch{1n,1n,E.KVar{0n}}]}}
  ]}}]

def big_arithmetic() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"main",R.Code{[E.KFun{E.Fid{"main"},[],L.nat_repr(),E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.of_u32(1073741865)}},E.KLit{Lit.LInt{B.of_u32(1073741823)}}]}}]}}]

def main() -> IO(Unit):
  do IO<Unit>:
    output(layout(basic()))
    output(layout(closure()))
    output(layout(partial()))
    output(layout(tag_case()))
    output(layout(big_arithmetic()))

