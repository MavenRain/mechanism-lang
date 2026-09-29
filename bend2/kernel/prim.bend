import Base
import ./bignum.bend as Bignum
import ./bignum.bend as Bignum
import ./literal.bend as Literal
import ./term.bend as Term
import ./value.bend as Value
import ./quantity.bend as Quantity
import ./shape.bend as Shape
import ./level.bend as Level

type T is Data:
  Nat_add{}
  Nat_sub{}
  Nat_mul{}
  Nat_eq{}
  Nat_lt{}

def name(p: T) -> String:
  match p:
    case Nat_add{}:
      "natAdd"
    case Nat_sub{}:
      "natSub"
    case Nat_mul{}:
      "natMul"
    case Nat_eq{}:
      "natEq"
    case Nat_lt{}:
      "natLt"

def catalog() -> List<&2, T>:
  [Nat_add{}, Nat_sub{}, Nat_mul{}, Nat_eq{}, Nat_lt{}]

def arity(p: T) -> Nat:
  2n

def find(ps: List<&2, T>, +n: String) -> Maybe<&2, T>:
  match ps:
    case Nil{}:
      None{}
    case Con{+p, rest}:
      Bool.pick(Maybe<&2, T>, String.eq(name(p), n), Some{p}, find(rest, n))

def of_name(n: String) -> Maybe<&2, T>:
  find(catalog(), n)

def nat_ty() -> Term.T:
  Term.Global{"Nat"}

def unit_ty() -> Term.T:
  Term.Ann{Term.Ran{Shape.SColl{Bignum.zero()}, Term.Sec{Shape.SColl{Bignum.zero()}, []}}, Term.Univ{Level.one()}}

def bool_ty() -> Term.T:
  Term.Lan{Shape.SColl{Bignum.of_u32(2)}, Term.Sec{Shape.SColl{Bignum.of_u32(2)}, [Term.Leg{[], unit_ty()}, Term.Leg{[], unit_ty()}]}}

def arrow(q: Quantity.T, n: String, dom: Term.T, cod: Term.T) -> Term.T:
  Term.Ran{Shape.SPi{q, n, dom}, cod}

def result_ty(p: T) -> Term.T:
  match p:
    case Nat_add{}:
      nat_ty()
    case Nat_sub{}:
      nat_ty()
    case Nat_mul{}:
      nat_ty()
    case Nat_eq{}:
      bool_ty()
    case Nat_lt{}:
      bool_ty()

def ty(p: T) -> Term.T:
  arrow(Quantity.QMany{}, "a", nat_ty(), arrow(Quantity.QMany{}, "b", nat_ty(), result_ty(p)))

def bool_value(b: Bool) -> Value.T:
  Value.VIn{Shape.SColl{Bignum.of_u32(2)}, Value.VALeg{Bool.pick(Bignum.T, b, Bignum.one(), Bignum.zero())}, [Value.VSec{Shape.SColl{Bignum.zero()}, []}]}

def apply_pair(p: T, a: Bignum.T, b: Bignum.T) -> Value.T:
  match p:
    case Nat_add{}:
      Value.VLit{Literal.LInt{Bignum.add(a, b)}}
    case Nat_sub{}:
      Value.VLit{Literal.LInt{Bignum.sub(a, b)}}
    case Nat_mul{}:
      Value.VLit{Literal.LInt{Bignum.mul(a, b)}}
    case Nat_eq{}:
      bool_value(Bignum.equal(a, b))
    case Nat_lt{}:
      bool_value(Bool.not(Bignum.le(b, a)))

def apply(p: T, args: List<&2, Literal.T>) -> Maybe<&2, Value.T>:
  match args:
    case Con{Literal.LInt{+a}, Con{Literal.LInt{+b}, Nil{}}}:
      Bool.pick(Maybe<&2, Value.T>, Bool.and(Bignum.le(Bignum.zero(), a), Bignum.le(Bignum.zero(), b)), Some{apply_pair(p, a, b)}, None{})
    case _:
      None{}
