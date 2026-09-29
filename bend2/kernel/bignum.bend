import Base
import ./common.bend as Common

# Signed, canonical base-32768 limbs, least significant first.
type T is Data:
  Big{negative: Bool, digits: List<&2, U32>}

def zero() -> T:
  Big{False{}, []}

def one() -> T:
  Big{False{}, [1]}

def trim_put(tail: List<&2, U32>, +digit: U32) -> List<&2, U32>:
  match tail:
    case Nil{}:
      Bool.pick(List<&2, U32>, U32.is_eq(digit, 0), [], [digit])
    case Con{h, t}:
      digit <> h <> t

def trim(xs: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      []
    case Con{h, t}:
      trim_put(trim(t), h)

def canonical(xs: List<&2, U32>, negative: Bool) -> T:
  match xs:
    case Nil{}:
      zero()
    case Con{h, t}:
      Big{negative, h <> t}

def make(negative: Bool, digits: List<&2, U32>) -> T:
  canonical(trim(digits), negative)

@unsafe def digits_u32_step(done: Bool, +n: U32) -> List<&2, U32>:
  match done:
    case True{}:
      []
    case False{}:
      +q = (n / 32768 : U32)
      (n % 32768 : U32) <> digits_u32_step(U32.is_eq(q, 0), q)

def digits_u32(+n: U32) -> List<&2, U32>:
  digits_u32_step(U32.is_eq(n, 0), n)

def of_u32(n: U32) -> T:
  Big{False{}, digits_u32(n)}

def cmp_low(high: Cmp, a: U32, b: U32) -> Cmp:
  match high:
    case LT{}:
      LT{}
    case EQ{}:
      U32.cmp(a, b)
    case GT{}:
      GT{}

def cmp_digits(a: List<&2, U32>, b: List<&2, U32>) -> Cmp:
  match a b:
    case Nil{} Nil{}:
      EQ{}
    case Nil{} Con{h, t}:
      LT{}
    case Con{h, t} Nil{}:
      GT{}
    case Con{x, xs} Con{y, ys}:
      cmp_low(cmp_digits(xs, ys), x, y)

def reverse_cmp(c: Cmp) -> Cmp:
  match c:
    case LT{}:
      GT{}
    case EQ{}:
      EQ{}
    case GT{}:
      LT{}

def compare(a: T, b: T) -> Cmp:
  match a b:
    case Big{False{}, x} Big{False{}, y}:
      cmp_digits(x, y)
    case Big{True{}, x} Big{True{}, y}:
      reverse_cmp(cmp_digits(x, y))
    case Big{True{}, x} Big{False{}, y}:
      LT{}
    case Big{False{}, x} Big{True{}, y}:
      GT{}

def cmp_eq(c: Cmp) -> Bool:
  match c:
    case EQ{}:
      True{}
    case LT{}:
      False{}
    case GT{}:
      False{}

def cmp_le(c: Cmp) -> Bool:
  match c:
    case GT{}:
      False{}
    case LT{}:
      True{}
    case EQ{}:
      True{}

def equal(a: T, b: T) -> Bool:
  cmp_eq(compare(a, b))

def le(a: T, b: T) -> Bool:
  cmp_le(compare(a, b))

def sign(x: T) -> Cmp:
  compare(x, zero())

def positive(x: T) -> Bool:
  Bool.not(le(x, zero()))

def add_digits(a: List<&2, U32>, b: List<&2, U32>, carry: U32) -> List<&2, U32>:
  match a b:
    case Nil{} Nil{}:
      digits_u32(carry)
    case Con{x, xs} Nil{}:
      +s = (x + carry : U32)
      (s % 32768 : U32) <> add_digits(xs, [], (s / 32768 : U32))
    case Nil{} Con{y, ys}:
      +s = (y + carry : U32)
      (s % 32768 : U32) <> add_digits([], ys, (s / 32768 : U32))
    case Con{x, xs} Con{y, ys}:
      +s = (x + y + carry : U32)
      (s % 32768 : U32) <> add_digits(xs, ys, (s / 32768 : U32))

def sub_digits(a: List<&2, U32>, b: List<&2, U32>, carry: U32) -> List<&2, U32>:
  match a b:
    case Nil{} Nil{}:
      []
    case Nil{} Con{y, ys}:
      []
    case Con{x, xs} Nil{}:
      +s = (32768 + x - carry : U32)
      (s % 32768 : U32) <> sub_digits(xs, [], (1 - (s / 32768 : U32) : U32))
    case Con{x, xs} Con{y, ys}:
      +s = (32768 + x - y - carry : U32)
      (s % 32768 : U32) <> sub_digits(xs, ys, (1 - (s / 32768 : U32) : U32))

def add_signed(c: Cmp, negative: Bool, a: List<&2, U32>, b: List<&2, U32>) -> T:
  match c:
    case EQ{}:
      zero()
    case GT{}:
      make(negative, sub_digits(a, b, 0))
    case LT{}:
      make(Bool.not(negative), sub_digits(b, a, 0))

def add(a: T, b: T) -> T:
  match a b:
    case Big{False{}, x} Big{False{}, y}:
      make(False{}, add_digits(x, y, 0))
    case Big{True{}, x} Big{True{}, y}:
      make(True{}, add_digits(x, y, 0))
    case Big{False{}, +x} Big{True{}, +y}:
      add_signed(cmp_digits(x, y), False{}, x, y)
    case Big{True{}, +x} Big{False{}, +y}:
      add_signed(cmp_digits(x, y), True{}, x, y)

def neg(x: T) -> T:
  match x:
    case Big{negative, digits}:
      canonical(digits, Bool.not(negative))

def sub_keep(c: Cmp, a: T, b: T) -> T:
  match c:
    case GT{}:
      add(a, neg(b))
    case EQ{}:
      zero()
    case LT{}:
      zero()

def sub(+a: T, +b: T) -> T:
  sub_keep(compare(a, b), a, b)

def mul_digit(xs: List<&2, U32>, +digit: U32, carry: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      digits_u32(carry)
    case Con{x, xt}:
      +s = (x * digit + carry : U32)
      (s % 32768 : U32) <> mul_digit(xt, digit, (s / 32768 : U32))

def mul_digits(a: List<&2, U32>, +b: List<&2, U32>) -> List<&2, U32>:
  match a:
    case Nil{}:
      []
    case Con{x, xs}:
      add_digits(mul_digit(b, x, 0), 0 <> mul_digits(xs, b), 0)

def mul(a: T, b: T) -> T:
  match a b:
    case Big{sa, xs} Big{sb, ys}:
      make(Bool.xor(sa, sb), mul_digits(xs, ys))

def maximum(+a: T, +b: T) -> T:
  Bool.pick(T, le(a, b), b, a)

def read_digit(valid: Bool, c: U32, value: T) -> Maybe<&2, T>:
  match valid:
    case False{}:
      None{}
    case True{}:
      Some{add(mul(value, of_u32(10)), of_u32((c - 48 : U32)))}

def read_chars(s: String, value: Maybe<&2, T>) -> Maybe<&2, T>:
  match s value:
    case SNil{} None{}:
      None{}
    case SNil{} Some{x}:
      Some{x}
    case SCon{c, rest} None{}:
      None{}
    case SCon{c, rest} Some{x}:
      +code = Char.to_u32(c)
      read_chars(rest, read_digit(Bool.and(U32.is_ge(code, 48), U32.is_le(code, 57)), code, x))

def of_decimal(s: String) -> Maybe<&2, T>:
  match s:
    case SNil{}:
      None{}
    case SCon{c, rest}:
      +code = Char.to_u32(c)
      read_chars(rest, read_digit(Bool.and(U32.is_ge(code, 48), U32.is_le(code, 57)), code, zero()))

def div_small_put(result: Common.Pair<List<&2, U32>, U32>, x: U32, +divisor: U32) -> Common.Pair<List<&2, U32>, U32>:
  match result:
    case Common.Pair{qs, rem}:
      +n = (rem * 32768 + x : U32)
      Common.Pair{(n / divisor : U32) <> qs, (n % divisor : U32)}

def div_small(xs: List<&2, U32>, +divisor: U32) -> Common.Pair<List<&2, U32>, U32>:
  match xs:
    case Nil{}:
      Common.Pair{[], 0}
    case Con{x, rest}:
      div_small_put(div_small(rest, divisor), x, divisor)

def div_small_put_trim(result: Common.Pair<List<&2, U32>, U32>) -> Common.Pair<List<&2, U32>, U32>:
  match result:
    case Common.Pair{q, r}:
      Common.Pair{trim(q), r}

@unsafe def show_digits(state: Maybe<&2, Common.Pair<List<&2, U32>, U32>>, suffix: String) -> String:
  match state:
    case None{}:
      suffix
    case Some{Common.Pair{Nil{}, r}}:
      SCon{Char.from_u32((48 + r : U32)), suffix}
    case Some{Common.Pair{Con{x, rest}, r}}:
      show_digits(Some{div_small_put_trim(div_small(x <> rest, 10))}, SCon{Char.from_u32((48 + r : U32)), suffix})

def to_string(x: T) -> String:
  match x:
    case Big{negative, Nil{}}:
      "0"
    case Big{negative, Con{x, rest}}:
      Bool.pick(String, negative, "-", "") ++ show_digits(Some{div_small_put_trim(div_small(x <> rest, 10))}, "")

def limbs15(x: T) -> Maybe<&2, List<&2, U32>>:
  match x:
    case Big{True{}, digits}:
      None{}
    case Big{False{}, digits}:
      Some{digits}

def narrow_put(rest: Maybe<&2, U32>, +digit: U32) -> Maybe<&2, U32>:
  match rest:
    case None{}:
      None{}
    case Some{+n}:
      Bool.pick(Maybe<&2, U32>, U32.is_le(n, ((4294967295 - digit : U32) / 32768 : U32)), Some{(n * 32768 + digit : U32)}, None{})

def narrow_digits(xs: List<&2, U32>) -> Maybe<&2, U32>:
  match xs:
    case Nil{}:
      Some{0}
    case Con{x, rest}:
      narrow_put(narrow_digits(rest), x)

def to_u32(value: T) -> Maybe<&2, U32>:
  match value:
    case Big{True{}, digits}:
      None{}
    case Big{False{}, digits}:
      narrow_digits(digits)

def i31_select(value: Maybe<&2, U32>) -> Maybe<&2, U32>:
  match value:
    case None{}:
      None{}
    case Some{+n}:
      Bool.pick(Maybe<&2, U32>, U32.is_le(n, 1073741823), Some{n}, None{})

def to_i31(value: T) -> Maybe<&2, U32>:
  i31_select(to_u32(value))

@unsafe def digits_nat(n: Nat) -> List<&2, U32>:
  match n:
    case 0n: []
    case 1n+ +p:
      U32.from_nat(Nat.mod(1n+p,32768n)) <> digits_nat(Nat.div(1n+p,32768n))

def of_nat(n: Nat) -> T:
  Big{False{},digits_nat(n)}

def bounded_nat(inside: Bool, value: T) -> Maybe<&2, Nat>:
  match inside:
    case False{}: None{}
    case True{}: Nat.read(to_string(value))

def to_nat_bounded(+value: T, upper: Nat) -> Maybe<&2, Nat>:
  bounded_nat(Bool.and(le(zero(),value),le(value,of_nat(upper))),value)

def at_narrowed(-A: Data, xs: List<&2, A>, index: Maybe<&2, Nat>) -> Maybe<&2, A>:
  match index:
    case None{}: None{}
    case Some{k}: Common.at(A,xs,k)

def at(-A: Data, +xs: List<&2, A>, index: T) -> Maybe<&2, A>:
  at_narrowed(A,xs,to_nat_bounded(index,List.length(&2,A,xs)))
