import Base
import ../kernel/level.bend as L
import ../kernel/level_var.bend as V
import ../kernel/bignum.bend as Big
import ../kernel/common.bend as Pair

def subst_ok(value: Maybe<&2, L.T>) -> Bool:
  match value:
    case Some{x}:
      L.equal(x, L.of_u32(3))
    case None{}:
      False{}

def absent(value: Maybe<&2, L.T>) -> Bool:
  match value:
    case None{}:
      True{}
    case Some{x}:
      False{}

def big_check(value: Maybe<&2, Big.T>) -> Bool:
  match value:
    case None{}:
      False{}
    case Some{amount}:
      +largest = V.offset(amount, L.zero())
      +next = L.succ(largest)
      Bool.and(L.le(largest, next), Bool.and(Bool.not(L.le(next, largest)), Bool.and(Bool.not(L.equal(next, L.zero())), String.eq(L.to_string(next), "4611686018427387904"))))

def cases() -> List<&2, Pair.Pair<String, Bool>>:
  +u = L.var(0n)
  +v = L.var(1n)
  +w = L.var(2n)
  [Pair.Pair{"imax-zero", L.equal(L.imax(u, L.zero()), L.zero())},
   Pair.Pair{"imax-zero-left", L.equal(L.imax(L.zero(), u), u)},
   Pair.Pair{"imax-self", L.equal(L.imax(u, u), u)},
   Pair.Pair{"imax-distribution", L.equal(L.max(L.imax(u,v), L.imax(u,w)), L.imax(u, L.max(v,w)))},
   Pair.Pair{"successor-distribution", L.equal(L.max(L.succ(u),L.succ(v)), L.succ(L.max(u,v)))},
   Pair.Pair{"successor-domination", L.equal(L.succ(u), L.max(L.succ(u),L.max(u,L.one())))},
   Pair.Pair{"max-idempotent", L.equal(L.max(u,v), L.max(L.max(v,u),L.max(u,v)))},
   Pair.Pair{"imax-positive", L.equal(L.max(u,L.succ(v)),L.imax(u,L.succ(v)))},
   Pair.Pair{"imax-conditional", Bool.not(L.equal(L.imax(u,v),L.max(u,v)))},
   Pair.Pair{"symbolic-zero", Bool.and(Bool.not(L.equal(u,L.zero())),Bool.and(Bool.not(L.always_positive(u)),L.always_positive(L.succ(u))))},
   Pair.Pair{"conditional-positive", Bool.and(Bool.not(L.always_positive(L.imax(L.one(),u))),L.always_positive(L.imax(u,L.one())))},
   Pair.Pair{"le-successor", Bool.and(L.le(u,L.succ(u)),Bool.not(L.le(L.succ(u),u)))},
   Pair.Pair{"le-imax", Bool.and(L.le(L.imax(u,v),L.max(u,v)),Bool.not(L.le(L.max(u,v),L.imax(u,v))))},
   Pair.Pair{"no-finite-sampling", Bool.and(Bool.not(L.equal(L.max(L.of_u32(8),u),L.of_u32(8))),Bool.not(L.le(u,L.of_u32(8))))},
   Pair.Pair{"beyond-host-int", big_check(Big.of_decimal("4611686018427387903"))},
   Pair.Pair{"scope", Bool.and(L.in_scope(1n,u),Bool.and(Bool.not(L.in_scope(0n,u)),Bool.not(L.in_scope(1n,L.max(u,v)))))},
   Pair.Pair{"substitution", Bool.and(subst_ok(L.subst([L.of_u32(3),L.zero()],L.imax(u,L.max(v,L.one())))),absent(L.subst([L.of_u32(3)],L.max(u,v))))}]

@unsafe def run(cases: List<&2, Pair.Pair<String, Bool>>) -> IO(Unit):
  match cases:
    case Nil{}:
      IO.print("PASS 17 original universe algebra regression families")
    case Con{Pair.Pair{name, False{}}, rest}:
      IO.die(Unit, 1, "FAIL " ++ name)
    case Con{Pair.Pair{name, True{}}, rest}:
      run(rest)

def main() -> IO(Unit):
  run(cases())
