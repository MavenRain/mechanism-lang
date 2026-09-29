import Base
import ../kernel/quantity.bend as Q

def show(label: String, value: Bool) -> IO(Unit):
  IO.print(label ++ ": " ++ Bool.pick(String,value,"true","false"))

def close_valid(+level: Nat, +outer: Nat, +mode: Q.T, +q: Q.T, affine: Bool, +uses: Q.Usage) -> Bool:
  Bool.or(Nat.is_lt(level, outer), Bool.or(Q.equal(mode, Q.QZero{}), Bool.or(Bool.not(Q.equal(q, Q.QOne{})), Bool.pick(Bool, affine, Q.at_most_once(level, uses), Q.exactly_once(level, uses)))))

def affine_pick(affine: Bool, +level: Nat, +uses: Q.Usage) -> Bool:
  Bool.pick(Bool,affine,Q.at_most_once(level,uses),Q.exactly_once(level,uses))

def inner(q: Q.T, affine: Bool, +level: Nat, +uses: Q.Usage) -> Bool:
  Bool.or(Bool.not(Q.equal(q,Q.QOne{})),Bool.pick(Bool,affine,Q.at_most_once(level,uses),Q.exactly_once(level,uses)))

def middle(mode: Q.T, q: Q.T, affine: Bool, +level: Nat, +uses: Q.Usage) -> Bool:
  Bool.or(Q.equal(mode,Q.QZero{}),inner(q,affine,level,uses))

def outer(+level: Nat, bound: Nat, mode: Q.T, q: Q.T, affine: Bool, uses: Q.Usage) -> Bool:
  Bool.or(Nat.is_lt(level,bound),middle(mode,q,affine,level,uses))

def run(+n: Nat) -> IO(Unit):
  do IO<Unit>:
    ok : Unit <- show("or FT",Bool.or(False{},Nat.is_eq(n,n)))
    ok : Unit <- show("or TF",Bool.or(Nat.is_eq(n,n),False{}))
    ok : Unit <- show("or FF refused",Bool.not(Bool.or(False{},False{})))
    ok : Unit <- show("or TT",Bool.or(Nat.is_eq(n,n),Nat.is_eq(n,n)))
    ok : Unit <- show("pick F",Bool.pick(Bool,False{},False{},Nat.is_eq(n,n)))
    ok : Unit <- show("nested OR",Bool.or(False{},Bool.or(False{},Bool.or(False{},Nat.is_eq(n,n)))))
    ok : Unit <- show("not T or T",Bool.or(Bool.not(Nat.is_eq(n,n)),Nat.is_eq(n,n)))
    ok : Unit <- show("not T or false refused",Bool.not(Bool.or(Bool.not(Nat.is_eq(n,n)),False{})))
    ok : Unit <- show("not Qequal or T",Bool.or(Bool.not(Q.equal(Q.QOne{},Q.QOne{})),Nat.is_eq(n,n)))
    ok : Unit <- show("qone equality",Q.equal(Q.QOne{},Q.QOne{}))
    ok : Unit <- show("nat zero equality",Nat.is_eq(n,n))
    ok : Unit <- show("once singleton",Q.exactly_once(n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("once sequence left",Q.exactly_once(n,Q.sequence(Q.empty(),Q.occurrence(n,Q.QOne{}))))
    ok : Unit <- show("once sequence right",Q.exactly_once(n,Q.sequence(Q.occurrence(n,Q.QOne{}),Q.empty())))
    ok : Unit <- show("once scale",Q.exactly_once(n,Q.scale(Q.QOne{},Q.occurrence(n,Q.QOne{}))))
    ok : Unit <- show("nested close valid",close_valid(n,n,Q.QOne{},Q.QOne{},False{},Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("at most singleton",Q.at_most_once(n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("affine pick false",affine_pick(False{},n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("affine pick true",affine_pick(True{},n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("inner",inner(Q.QOne{},False{},n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("middle",middle(Q.QOne{},Q.QOne{},False{},n,Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("outer",outer(n,n,Q.QOne{},Q.QOne{},False{},Q.occurrence(n,Q.QOne{})))
    ok : Unit <- show("twice refused",Bool.not(Q.exactly_once(n,Q.sequence(Q.occurrence(n,Q.QOne{}),Q.occurrence(n,Q.QOne{})))))
    show("zero refused",Bool.not(Q.exactly_once(n,Q.empty())))

def parsed(value: Maybe<&2,Nat>) -> IO(Unit):
  match value:
    case Some{n}: run(n)
    case None{}: IO.die(Unit,2,"invalid natural level")

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{text,Nil{}}: parsed(Nat.read(text))
    case other: IO.die(Unit,2,"expected a natural level")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
