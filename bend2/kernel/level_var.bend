import Base
import ./bignum.bend as Bignum

type T is Data:
  LZero{}
  LSucc{amount: Bignum.T, base: T}
  LMax{left: T, right: T}
  LIMax{left: T, right: T}
  LVar{index: Nat}

def offset_positive(level: T, amount: Bignum.T) -> T:
  match level:
    case LSucc{prior, base}:
      LSucc{Bignum.add(amount, prior), base}
    case LZero{}:
      LSucc{amount, LZero{}}
    case LMax{left, right}:
      LSucc{amount, LMax{left, right}}
    case LIMax{left, right}:
      LSucc{amount, LIMax{left, right}}
    case LVar{index}:
      LSucc{amount, LVar{index}}

def offset_select(positive: Bool, amount: Bignum.T, level: T) -> T:
  match positive:
    case False{}:
      level
    case True{}:
      offset_positive(level, amount)

def offset(+amount: Bignum.T, level: T) -> T:
  offset_select(Bignum.positive(amount), amount, level)

def in_scope(level: T, +arity: Nat) -> Bool:
  match level:
    case LZero{}:
      True{}
    case LSucc{amount, base}:
      Bool.and(Bignum.positive(amount), in_scope(base, arity))
    case LMax{left, right}:
      Bool.and(in_scope(left, arity), in_scope(right, arity))
    case LIMax{left, right}:
      Bool.and(in_scope(left, arity), in_scope(right, arity))
    case LVar{index}:
      Nat.is_lt(index, arity)

def well_formed(level: T) -> Bool:
  match level:
    case LZero{}:
      True{}
    case LSucc{amount, base}:
      Bool.and(Bignum.positive(amount), well_formed(base))
    case LMax{left, right}:
      Bool.and(well_formed(left), well_formed(right))
    case LIMax{left, right}:
      Bool.and(well_formed(left), well_formed(right))
    case LVar{index}:
      True{}

def subst_succ(value: Maybe<&2, T>, amount: Bignum.T) -> Maybe<&2, T>:
  match value:
    case None{}:
      None{}
    case Some{base}:
      Some{offset(amount, base)}

def subst_pair(a: Maybe<&2, T>, b: Maybe<&2, T>, imax: Bool) -> Maybe<&2, T>:
  match a b:
    case None{} _:
      None{}
    case Some{x} None{}:
      None{}
    case Some{+x} Some{+y}:
      Some{Bool.pick(T, imax, LIMax{x, y}, LMax{x, y})}

def subst_visit(level: T, +args: List<&2, T>) -> Maybe<&2, T>:
  match level:
    case LZero{}:
      Some{LZero{}}
    case LSucc{amount, base}:
      subst_succ(subst_visit(base, args), amount)
    case LMax{left, right}:
      subst_pair(subst_visit(left, args), subst_visit(right, args), False{})
    case LIMax{left, right}:
      subst_pair(subst_visit(left, args), subst_visit(right, args), True{})
    case LVar{index}:
      List.get(&2, T, args, index)

def subst_valid(valid: Bool, args: List<&2, T>, level: T) -> Maybe<&2, T>:
  match valid:
    case False{}:
      None{}
    case True{}:
      subst_visit(level, args)

def subst(+args: List<&2, T>, +level: T) -> Maybe<&2, T>:
  subst_valid(Bool.and(in_scope(level, List.length(&2, T, args)), List.all(~&2, ~T, ~well_formed, args)), args, level)

def structural_equal(a: T, b: T) -> Bool:
  match a b:
    case LZero{} LZero{}:
      True{}
    case LSucc{x, p} LSucc{y, q}:
      Bool.and(Bignum.equal(x, y), structural_equal(p, q))
    case LMax{x, p} LMax{y, q}:
      Bool.and(structural_equal(x, y), structural_equal(p, q))
    case LIMax{x, p} LIMax{y, q}:
      Bool.and(structural_equal(x, y), structural_equal(p, q))
    case LVar{x} LVar{y}:
      Nat.is_eq(x, y)
    case _ _:
      False{}
