import Base
import ../wasm/emit.bend as Emit
import ../wasm/link.bend as L
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as R
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as Err

def pair_tid() -> E.Tid:
  E.Tid{"pair<union nat,union nat>"}

def literal(n:U32) -> E.Ktm:
  E.KLit{Lit.LInt{B.of_u32(n)}}

def rows() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"exports",R.Code{[
    E.KFun{E.Fid{"add"},[L.nat_repr(),L.nat_repr()],L.nat_repr(),E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"pair"},[L.nat_repr(),L.nat_repr()],E.RStruct{pair_tid()},E.KStruct{pair_tid(),[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"sum"},[E.RStruct{pair_tid()}],L.nat_repr(),E.KApp{E.KGlobal{"natAdd"},[E.KProj{pair_tid(),0n,E.KVar{0n}},E.KProj{pair_tid(),1n,E.KVar{0n}}]}},
    E.KFun{E.Fid{"bool"},[],E.RUnion{E.Tid{"sum<unit|unit>"}},E.KTag{E.Tid{"sum<unit|unit>"},1n,[]}},
    E.KFun{E.Fid{"unbool"},[E.RUnion{E.Tid{"sum<unit|unit>"}}],L.nat_repr(),E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,literal(0)},E.Branch{1n,0n,literal(1)}]}},
    E.KFun{E.Fid{"big"},[],L.nat_repr(),literal(1073741824)}
  ]}}]

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
  output(Emit.reactor(rows(),["add","pair","sum","bool","unbool","big"]))
