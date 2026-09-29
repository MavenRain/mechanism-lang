import Base
import ./level_var.bend as V
import ./bignum.bend as Bignum
import ./level_eq.bend as Eq
import ./budget.bend as Budget
import ./error.bend as Error

def T() -> Data:
  V.T

def zero() -> T:
  V.LZero{}

def one() -> T:
  V.LSucc{Bignum.one(), V.LZero{}}

def succ(level: T) -> T:
  V.offset(Bignum.one(), level)

def max(left: T, right: T) -> T:
  V.LMax{left, right}

def imax(left: T, right: T) -> T:
  V.LIMax{left, right}

def of_u32(value: U32) -> T:
  V.offset(Bignum.of_u32(value), V.LZero{})

def var(index: Nat) -> T:
  V.LVar{index}

def in_scope(arity: Nat, level: T) -> Bool:
  V.in_scope(level, arity)

def subst(args: List<&2, T>, level: T) -> Maybe<&2, T>:
  V.subst(args, level)

def equal(left: T, right: T) -> Bool:
  Eq.equal(left, right)

def le(left: T, right: T) -> Bool:
  Eq.le(left, right)

def equal_budget(budget: Budget.T, left: T, right: T) -> Result<&2, &2, Error.T, Bool>:
  Eq.equal_budget(budget, left, right)

def le_budget(budget: Budget.T, left: T, right: T) -> Result<&2, &2, Error.T, Bool>:
  Eq.le_budget(budget, left, right)

def always_zero(level: T) -> Bool:
  Eq.always_zero(level)

def always_positive(level: T) -> Bool:
  Eq.always_positive(level)

def render(level: T) -> String:
  match level:
    case V.LZero{}:
      "0"
    case V.LSucc{amount, base}:
      "(" ++ render(base) ++ " + " ++ Bignum.to_string(amount) ++ ")"
    case V.LMax{left, right}:
      "max(" ++ render(left) ++ ", " ++ render(right) ++ ")"
    case V.LIMax{left, right}:
      "imax(" ++ render(left) ++ ", " ++ render(right) ++ ")"
    case V.LVar{index}:
      "u" ++ Nat.show(index)

def show_closed(closed: Maybe<&2, Bignum.T>, level: T) -> String:
  match closed:
    case None{}:
      render(level)
    case Some{value}:
      Bignum.to_string(value)

def to_string(+level: T) -> String:
  show_closed(Eq.closed_value(level), level)
