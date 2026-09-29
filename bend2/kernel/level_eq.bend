import Base
import ./common.bend as Common
import ./bignum.bend as Bignum
import ./level_var.bend as V
import ./budget.bend as Budget
import ./error.bend as Error

type Normal is Data:
  Normal{constant: Bignum.T, arms: List<&2, Common.Pair<Nat, Bignum.T>>}

def member(xs: List<&2, Nat>, +index: Nat) -> Bool:
  match xs:
    case Nil{}:
      False{}
    case Con{x, rest}:
      Bool.or(Nat.is_eq(x, index), member(rest, index))

def insert(+xs: List<&2, Nat>, +index: Nat) -> List<&2, Nat>:
  Bool.pick(List<&2, Nat>, member(xs, index), xs, index <> xs)

def variables(level: V.T, found: List<&2, Nat>) -> List<&2, Nat>:
  match level:
    case V.LZero{}:
      found
    case V.LSucc{amount, base}:
      variables(base, found)
    case V.LMax{left, right}:
      variables(right, variables(left, found))
    case V.LIMax{left, right}:
      variables(right, variables(left, found))
    case V.LVar{index}:
      insert(found, index)

def find_arm(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +index: Nat) -> Maybe<&2, Bignum.T>:
  match xs:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, amount}, rest}:
      Bool.pick(Maybe<&2, Bignum.T>, Nat.is_eq(key, index), Some{amount}, find_arm(rest, index))

def merge_arm(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +index: Nat, +amount: Bignum.T) -> List<&2, Common.Pair<Nat, Bignum.T>>:
  match xs:
    case Nil{}:
      [Common.Pair{index, amount}]
    case Con{Common.Pair{+key, +prior}, +rest}:
      Bool.pick(List<&2, Common.Pair<Nat, Bignum.T>>, Nat.is_eq(key, index), Common.Pair{key, Bignum.maximum(amount, prior)} <> rest, Common.Pair{key, prior} <> merge_arm(rest, index, amount))

def merge_arms(xs: List<&2, Common.Pair<Nat, Bignum.T>>, acc: List<&2, Common.Pair<Nat, Bignum.T>>) -> List<&2, Common.Pair<Nat, Bignum.T>>:
  match xs:
    case Nil{}:
      acc
    case Con{Common.Pair{key, amount}, rest}:
      merge_arms(rest, merge_arm(acc, key, amount))

def maximum(a: Normal, b: Normal) -> Normal:
  match a b:
    case Normal{x, xs} Normal{y, ys}:
      Normal{Bignum.maximum(x, y), merge_arms(xs, ys)}

def shift_arms(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +amount: Bignum.T) -> List<&2, Common.Pair<Nat, Bignum.T>>:
  match xs:
    case Nil{}:
      []
    case Con{Common.Pair{key, prior}, rest}:
      Common.Pair{key, Bignum.add(amount, prior)} <> shift_arms(rest, amount)

def shift(value: Normal, +amount: Bignum.T) -> Normal:
  match value:
    case Normal{constant, arms}:
      Normal{Bignum.add(constant, amount), shift_arms(arms, amount)}

def imax_combine(right: Normal, left: Normal, +amount: Bignum.T) -> Normal:
  match right:
    case Normal{+constant, Nil{}}:
      Bool.pick(Normal, Bignum.equal(constant, Bignum.zero()), Normal{amount, []}, maximum(left, Normal{Bignum.add(constant, amount), []}))
    case Normal{constant, Con{h, t}}:
      maximum(left, shift(Normal{constant, h <> t}, amount))

def normal(level: V.T, +positive: List<&2, Nat>, +amount: Bignum.T) -> Normal:
  match level:
    case V.LZero{}:
      Normal{amount, []}
    case V.LSucc{extra, base}:
      normal(base, positive, Bignum.add(amount, extra))
    case V.LVar{+index}:
      Normal{amount, Bool.pick(List<&2, Common.Pair<Nat, Bignum.T>>, member(positive, index), [Common.Pair{index, amount}], [])}
    case V.LMax{left, right}:
      maximum(normal(left, positive, amount), normal(right, positive, amount))
    case V.LIMax{left, right}:
      imax_combine(normal(right, positive, Bignum.zero()), normal(left, positive, amount), amount)

def minimum_value(xs: List<&2, Common.Pair<Nat, Bignum.T>>, acc: Bignum.T) -> Bignum.T:
  match xs:
    case Nil{}:
      acc
    case Con{Common.Pair{index, amount}, rest}:
      minimum_value(rest, Bignum.maximum(acc, Bignum.add(amount, Bignum.one())))

def arm_below(other: Maybe<&2, Bignum.T>, amount: Bignum.T) -> Bool:
  match other:
    case None{}:
      False{}
    case Some{value}:
      Bignum.le(amount, value)

def arms_below(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +ys: List<&2, Common.Pair<Nat, Bignum.T>>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{Common.Pair{index, amount}, rest}:
      Bool.and(arm_below(find_arm(ys, index), amount), arms_below(rest, ys))

def below(a: Normal, b: Normal) -> Bool:
  match a b:
    case Normal{x, xs} Normal{y, +ys}:
      Bool.and(Bignum.le(x, minimum_value(ys, y)), arms_below(xs, ys))

def cases(remaining: List<&2, Nat>, +positive: List<&2, Nat>, +left: V.T, +right: V.T) -> Bool:
  match remaining:
    case Nil{}:
      below(normal(left, positive, Bignum.zero()), normal(right, positive, Bignum.zero()))
    case Con{index, +rest}:
      Bool.and(cases(rest, positive, left, right), cases(rest, index <> positive, left, right))

def le(+left: V.T, +right: V.T) -> Bool:
  Bool.and(Bool.and(V.well_formed(left), V.well_formed(right)), cases(variables(left, variables(right, [])), [], left, right))

def equal(+left: V.T, +right: V.T) -> Bool:
  Bool.and(le(left, right), le(right, left))

def always_zero(level: V.T) -> Bool:
  match level:
    case V.LZero{}:
      True{}
    case V.LSucc{amount, base}:
      False{}
    case V.LMax{left, right}:
      Bool.and(always_zero(left), always_zero(right))
    case V.LIMax{left, right}:
      always_zero(right)
    case V.LVar{index}:
      False{}

def always_positive(level: V.T) -> Bool:
  match level:
    case V.LZero{}:
      False{}
    case V.LSucc{amount, base}:
      Bignum.positive(amount)
    case V.LMax{left, right}:
      Bool.or(always_positive(left), always_positive(right))
    case V.LIMax{left, right}:
      always_positive(right)
    case V.LVar{index}:
      False{}

def constant(value: Normal) -> Bignum.T:
  match value:
    case Normal{c, arms}:
      c

def closed_value(+level: V.T) -> Maybe<&2, Bignum.T>:
  Bool.pick(Maybe<&2, Bignum.T>, V.in_scope(level, 0n), Some{constant(normal(level, [], Bignum.zero()))}, None{})

law variables_m:
  for level: V.T
  for found: List<&2, Nat>
  Budget.Comp(List<&2, Nat>)

law merge_node:
  for xs: List<&2, Common.Pair<Nat, Bignum.T>>
  for acc: List<&2, Common.Pair<Nat, Bignum.T>>
  Budget.Comp(List<&2, Common.Pair<Nat, Bignum.T>>)

law shift_arms_m:
  for xs: List<&2, Common.Pair<Nat, Bignum.T>>
  for +amount: Bignum.T
  Budget.Comp(List<&2, Common.Pair<Nat, Bignum.T>>)

law normal_m:
  for level: V.T
  for +positive: List<&2, Nat>
  for +amount: Bignum.T
  Budget.Comp(Normal)

law minimum_m:
  for xs: List<&2, Common.Pair<Nat, Bignum.T>>
  for acc: Bignum.T
  Budget.Comp(Bignum.T)

law arms_below_m:
  for xs: List<&2, Common.Pair<Nat, Bignum.T>>
  for +ys: List<&2, Common.Pair<Nat, Bignum.T>>
  for same: Bool
  Budget.Comp(Bool)

law cases_m:
  for remaining: List<&2, Nat>
  for +positive: List<&2, Nat>
  for +left: V.T
  for +right: V.T
  for +equality: Bool
  Budget.Comp(Bool)


@unsafe def validate_offset(valid: Bool, base: V.T, found: List<&2, Nat>) -> Budget.Comp(List<&2, Nat>):
  match valid:
    case False{}:
      Budget.failed(List<&2, Nat>, Error.Universe{"a universe successor offset must be positive"})
    case True{}:
      variables_m(base, found)

@unsafe def variables_node(level: V.T, found: List<&2, Nat>) -> Budget.Comp(List<&2, Nat>):
  match level:
    case V.LZero{}:
      Budget.pure(List<&2, Nat>, found)
    case V.LVar{index}:
      Budget.pure(List<&2, Nat>, insert(found, index))
    case V.LSucc{amount, base}:
      validate_offset(Bignum.positive(amount), base, found)
    case V.LMax{left, right}:
      do Budget.Comp<List<&2, Nat>>:
        next : List<&2, Nat> <- variables_m(left, found)
        variables_m(right, next)
    case V.LIMax{left, right}:
      do Budget.Comp<List<&2, Nat>>:
        next : List<&2, Nat> <- variables_m(left, found)
        variables_m(right, next)

@unsafe def variables_m(level, found):
  do Budget.Comp<List<&2, Nat>>:
    tick : Unit <- Budget.tick()
    variables_node(level, found)

@unsafe def merge_m(xs: List<&2, Common.Pair<Nat, Bignum.T>>, acc: List<&2, Common.Pair<Nat, Bignum.T>>) -> Budget.Comp(List<&2, Common.Pair<Nat, Bignum.T>>):
  do Budget.Comp<List<&2, Common.Pair<Nat, Bignum.T>>>:
    tick : Unit <- Budget.tick()
    merge_node(xs, acc)

@unsafe def merge_node(xs, acc):
  match xs:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<Nat, Bignum.T>>, acc)
    case Con{Common.Pair{key, amount}, rest}:
      merge_m(rest, merge_arm(acc, key, amount))

@unsafe def maximum_m(a: Normal, b: Normal) -> Budget.Comp(Normal):
  match a b:
    case Normal{x, xs} Normal{y, ys}:
      do Budget.Comp<Normal>:
        arms : List<&2, Common.Pair<Nat, Bignum.T>> <- merge_m(xs, ys)
        return Normal{Bignum.maximum(x, y), arms}

@unsafe def shift_arms_node(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +amount: Bignum.T) -> Budget.Comp(List<&2, Common.Pair<Nat, Bignum.T>>):
  match xs:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<Nat, Bignum.T>>, [])
    case Con{Common.Pair{key, prior}, rest}:
      do Budget.Comp<List<&2, Common.Pair<Nat, Bignum.T>>>:
        tail : List<&2, Common.Pair<Nat, Bignum.T>> <- shift_arms_m(rest, amount)
        return Common.Pair{key, Bignum.add(amount, prior)} <> tail

@unsafe def shift_arms_m(xs, amount):
  do Budget.Comp<List<&2, Common.Pair<Nat, Bignum.T>>>:
    tick : Unit <- Budget.tick()
    shift_arms_node(xs, amount)

@unsafe def shift_m(value: Normal, +amount: Bignum.T) -> Budget.Comp(Normal):
  match value:
    case Normal{constant, arms}:
      do Budget.Comp<Normal>:
        shifted : List<&2, Common.Pair<Nat, Bignum.T>> <- shift_arms_m(arms, amount)
        return Normal{Bignum.add(constant, amount), shifted}

@unsafe def imax_nonzero(zero: Bool, right: Normal, left: V.T, +positive: List<&2, Nat>, +amount: Bignum.T) -> Budget.Comp(Normal):
  match zero:
    case True{}:
      Budget.pure(Normal, Normal{amount, []})
    case False{}:
      do Budget.Comp<Normal>:
        lhs : Normal <- normal_m(left, positive, amount)
        rhs : Normal <- shift_m(right, amount)
        maximum_m(lhs, rhs)

def normal_zero(value: Normal) -> Bool:
  match value:
    case Normal{constant, Nil{}}:
      Bignum.equal(constant, Bignum.zero())
    case _:
      False{}

@unsafe def imax_helper(+right: Normal, left: V.T, positive: List<&2, Nat>, amount: Bignum.T) -> Budget.Comp(Normal):
  imax_nonzero(normal_zero(right), right, left, positive, amount)

@unsafe def normal_node(level: V.T, +positive: List<&2, Nat>, +amount: Bignum.T) -> Budget.Comp(Normal):
  match level:
    case V.LZero{}:
      Budget.pure(Normal, Normal{amount, []})
    case V.LVar{+index}:
      Budget.pure(Normal, Normal{amount, Bool.pick(List<&2, Common.Pair<Nat, Bignum.T>>, member(positive, index), [Common.Pair{index, amount}], [])})
    case V.LSucc{extra, base}:
      normal_m(base, positive, Bignum.add(amount, extra))
    case V.LMax{left, right}:
      do Budget.Comp<Normal>:
        lhs : Normal <- normal_m(left, positive, amount)
        rhs : Normal <- normal_m(right, positive, amount)
        maximum_m(lhs, rhs)
    case V.LIMax{left, right}:
      do Budget.Comp<Normal>:
        rhs : Normal <- normal_m(right, positive, Bignum.zero())
        imax_helper(rhs, left, positive, amount)

@unsafe def normal_m(level, positive, amount):
  do Budget.Comp<Normal>:
    tick : Unit <- Budget.tick()
    normal_node(level, positive, amount)

@unsafe def minimum_node(xs: List<&2, Common.Pair<Nat, Bignum.T>>, acc: Bignum.T) -> Budget.Comp(Bignum.T):
  match xs:
    case Nil{}:
      Budget.pure(Bignum.T, acc)
    case Con{Common.Pair{index, amount}, rest}:
      minimum_m(rest, Bignum.maximum(acc, Bignum.add(amount, Bignum.one())))

@unsafe def minimum_m(xs, acc):
  do Budget.Comp<Bignum.T>:
    tick : Unit <- Budget.tick()
    minimum_node(xs, acc)

@unsafe def arms_below_node(xs: List<&2, Common.Pair<Nat, Bignum.T>>, +ys: List<&2, Common.Pair<Nat, Bignum.T>>, same: Bool) -> Budget.Comp(Bool):
  match xs:
    case Nil{}:
      Budget.pure(Bool, same)
    case Con{Common.Pair{index, amount}, rest}:
      arms_below_m(rest, ys, Bool.and(same, arm_below(find_arm(ys, index), amount)))

@unsafe def arms_below_m(xs, ys, same):
  do Budget.Comp<Bool>:
    tick : Unit <- Budget.tick()
    arms_below_node(xs, ys, same)

@unsafe def below_m(a: Normal, b: Normal) -> Budget.Comp(Bool):
  match a b:
    case Normal{x, xs} Normal{y, +ys}:
      do Budget.Comp<Bool>:
        min : Bignum.T <- minimum_m(ys, y)
        arms_below_m(xs, ys, Bignum.le(x, min))

@unsafe def relation_reverse(holds: Bool, equality: Bool, left: Normal, right: Normal) -> Budget.Comp(Bool):
  match holds equality:
    case True{} True{}:
      below_m(right, left)
    case holds _:
      Budget.pure(Bool, holds)

@unsafe def compare_normal(+left: Normal, +right: Normal, equality: Bool) -> Budget.Comp(Bool):
  do Budget.Comp<Bool>:
    holds : Bool <- below_m(left, right)
    relation_reverse(holds, equality, left, right)

@unsafe def cases_continue(holds: Bool, remaining: List<&2, Nat>, positive: List<&2, Nat>, left: V.T, right: V.T, equality: Bool) -> Budget.Comp(Bool):
  match holds:
    case False{}:
      Budget.pure(Bool, False{})
    case True{}:
      cases_m(remaining, positive, left, right, equality)

@unsafe def cases_node(remaining: List<&2, Nat>, +positive: List<&2, Nat>, +left: V.T, +right: V.T, +equality: Bool) -> Budget.Comp(Bool):
  match remaining:
    case Nil{}:
      do Budget.Comp<Bool>:
        lhs : Normal <- normal_m(left, positive, Bignum.zero())
        rhs : Normal <- normal_m(right, positive, Bignum.zero())
        compare_normal(lhs, rhs, equality)
    case Con{index, +rest}:
      do Budget.Comp<Bool>:
        holds : Bool <- cases_m(rest, positive, left, right, equality)
        cases_continue(holds, rest, index <> positive, left, right, equality)

@unsafe def cases_m(remaining, positive, left, right, equality):
  do Budget.Comp<Bool>:
    tick : Unit <- Budget.tick()
    cases_node(remaining, positive, left, right, equality)

@unsafe def decide_m(+left: V.T, +right: V.T, equality: Bool) -> Budget.Comp(Bool):
  do Budget.Comp<Bool>:
    first : List<&2, Nat> <- variables_m(left, [])
    all : List<&2, Nat> <- variables_m(right, first)
    cases_m(all, [], left, right, equality)

def le_budget(budget: Budget.T, left: V.T, right: V.T) -> Result<&2, &2, Error.T, Bool>:
  Budget.run(Bool, decide_m(left, right, False{}), budget)

def equal_budget(budget: Budget.T, left: V.T, right: V.T) -> Result<&2, &2, Error.T, Bool>:
  Budget.run(Bool, decide_m(left, right, True{}), budget)
