import Base
import ../kernel/common.bend as C
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/eval.bend as E
import ../kernel/global.bend as G
import ../kernel/quantity.bend as Q
import ../kernel/shape.bend as S
import ../kernel/prim.bend as P
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as B
import ../kernel/error.bend as Error

def app(head: T.T, arg: T.T) -> T.T:
  T.Out{S.SPi{Q.QMany{}, "x", P.nat_ty()}, T.APt{Q.QMany{}, arg}, head}

def nat(n: U32) -> T.T:
  T.Lit{Lit.LInt{B.of_u32(n)}}

def check_nat(result: Result<&2, &2, Error.T, V.T>, want: U32) -> Bool:
  match result:
    case Done{V.VLit{Lit.LInt{got}}}:
      B.equal(got, B.of_u32(want))
    case _:
      False{}

def identity() -> T.T:
  T.Sec{S.SPi{Q.QMany{}, "x", P.nat_ty()}, [T.Leg{[C.Pair{Q.QMany{}, "x"}], T.Var{0n}}]}

def captured() -> T.T:
  T.Let{"outer", P.nat_ty(), nat(41), T.Sec{S.SPi{Q.QMany{}, "x", P.nat_ty()}, [T.Leg{[C.Pair{Q.QMany{}, "x"}], T.Var{1n}}]}}

def check_capture(result: Result<&2, &2, Error.T, T.T>) -> Bool:
  match result:
    case Done{T.Sec{s, Con{T.Leg{bs, T.Lit{Lit.LInt{got}}}, Nil{}}}}:
      B.equal(got, B.of_u32(41))
    case _:
      False{}

def readback(result: Result<&2, &2, Error.T, V.T>, globals: G.T) -> Bool:
  match result:
    case Done{value}:
      check_capture(E.quote(globals, 0n, value))
    case Fail{error}:
      False{}

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS evaluator beta, primitives, closure readback")
    case False{}:
      IO.die(Unit, 1, "FAIL evaluator beta, primitives, closure readback")

def main() -> IO(Unit):
  +globals = G.initial()
  verify(Bool.and(check_nat(E.eval(globals, [], app(identity(), nat(42))), 42), Bool.and(check_nat(E.eval(globals, [], app(app(T.Global{"natAdd"}, nat(17)), nat(25))), 42), Bool.and(check_nat(E.eval(globals, [], app(captured(), nat(99))), 41), readback(E.eval(globals, [], captured()), globals)))))
