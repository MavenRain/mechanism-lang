import Base
import ../kernel/common.bend as C
import ../kernel/error.bend as Err
import ./gc_encode.bend as G

type T is Data:
  State{base:Nat,extra:List<&2,G.Valtype>}

def Comp(-A:Data) -> Type:
  T -> C.Pair<T,Result<&2,&2,Err.T,A>>

def Comp.pure(-A:Data,value:A) -> Comp(A):
  state => C.Pair{state,Done{value}}

def bind_result(-A:Data,-B:Data,state:C.Pair<T,Result<&2,&2,Err.T,A>>,next:A -> Comp(B)) -> C.Pair<T,Result<&2,&2,Err.T,B>>:
  match state:
    case C.Pair{s,Fail{e}}: C.Pair{s,Fail{e}}
    case C.Pair{s,Done{a}}:
      action = next(a)
      action(s)

def Comp.bind(-A:Data,-B:Data,action:Comp(A),next:A -> Comp(B)) -> Comp(B):
  state => bind_result(A,B,action(state),next)

def lift(-A:Data,result:Result<&2,&2,Err.T,A>) -> Comp(A):
  state => C.Pair{state,result}

def fail(-A:Data,error:Err.T) -> Comp(A):
  lift(A,Fail{error})

def allocate(state:T,v:G.Valtype) -> C.Pair<T,Result<&2,&2,Err.T,U32>>:
  match state:
    case State{+base,+extra}:
      C.Pair{State{base,List.append(&2,G.Valtype,extra,[v])},Done{U32.from_nat(Nat.add(base,List.length(&2,G.Valtype,extra)))}}

def alloc(v:G.Valtype) -> Comp(U32):
  state => allocate(state,v)

def pure(-A:Data,value:A) -> Comp(A):
  Comp.pure(A,value)

def finish(ft:U32,state:C.Pair<T,Result<&2,&2,Err.T,List<&2,G.Instr>>>) -> Result<&2,&2,Err.T,G.Func>:
  match state:
    case C.Pair{s,Fail{e}}: Fail{e}
    case C.Pair{State{b,extra},Done{code}}: Done{G.Func{ft,extra,code}}
