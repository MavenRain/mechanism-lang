import Base
import ./common.bend as Common
import ./global.bend as Global
import ./value.bend as Value
import ./term.bend as Term
import ./quantity.bend as Quantity
import ./budget.bend as Budget
import ./level_scope.bend as LevelScope
import ./error.bend as Error
import ./level.bend as Level
import ./positivity.bend as Positivity

type Local is Data:
  Local{name: String, quantity: Quantity.T, ty: Value.T}

type LTree is Data:
  LLeaf{x: Local}
  LNode{x: Local, l: LTree, r: LTree}

type Locals is Data:
  LNil{}
  LCons{w: Nat, t: LTree, rest: Locals}

type LView is Data:
  LVNil{}
  LVCons{head: Local, tail: Locals}

def lcons_pair(same: Bool, x: Local, w1: Nat, t1: LTree, w2: Nat, t2: LTree, rest: Locals) -> Locals:
  match same:
    case True{}:
      LCons{Nat.add(1n, Nat.add(w1, w2)), LNode{x, t1, t2}, rest}
    case False{}:
      LCons{1n, LLeaf{x}, LCons{w1, t1, LCons{w2, t2, rest}}}

def lcons_one(x: Local, +w1: Nat, t1: LTree, rest: Locals) -> Locals:
  match rest:
    case LNil{}:
      LCons{1n, LLeaf{x}, LCons{w1, t1, LNil{}}}
    case LCons{+w2, t2, tail}:
      lcons_pair(Nat.is_eq(w1, w2), x, w1, t1, w2, t2, tail)

def lcons(x: Local, xs: Locals) -> Locals:
  match xs:
    case LNil{}:
      LCons{1n, LLeaf{x}, LNil{}}
    case LCons{w1, t1, rest}:
      lcons_one(x, w1, t1, rest)

law ltree_get:
  for +w: Nat
  for t: LTree
  for +i: Nat
  Maybe<&2, Local>

@unsafe def ltree_side(left: Bool, +half: Nat, l: LTree, r: LTree, +p: Nat) -> Maybe<&2, Local>:
  match left:
    case True{}:
      ltree_get(half, l, p)
    case False{}:
      ltree_get(half, r, Nat.sub(p, half))

@unsafe def ltree_node(root: Bool, +half: Nat, x: Local, l: LTree, r: LTree, +p: Nat) -> Maybe<&2, Local>:
  match root:
    case True{}:
      Some{x}
    case False{}:
      ltree_side(Nat.is_lt(p, half), half, l, r, p)

@unsafe def ltree_get(w, t, i):
  match t:
    case LLeaf{x}:
      Some{x}
    case LNode{x, l, r}:
      ltree_node(Nat.is_eq(i, 0n), Nat.div(w, 2n), x, l, r, Nat.sub(i, 1n))

law lget:
  for xs: Locals
  for +i: Nat
  Maybe<&2, Local>

@unsafe def lget_step(inside: Bool, +w: Nat, t: LTree, rest: Locals, +i: Nat) -> Maybe<&2, Local>:
  match inside:
    case True{}:
      ltree_get(w, t, i)
    case False{}:
      lget(rest, Nat.sub(i, w))

@unsafe def lget(xs, i):
  match xs:
    case LNil{}:
      None{}
    case LCons{+w, t, rest}:
      lget_step(Nat.is_lt(i, w), w, t, rest, i)

def lview_tree(+w: Nat, t: LTree, rest: Locals) -> LView:
  match t:
    case LLeaf{x}:
      LVCons{x, rest}
    case LNode{x, l, r}:
      LVCons{x, LCons{Nat.div(w, 2n), l, LCons{Nat.div(w, 2n), r, rest}}}

def lview(xs: Locals) -> LView:
  match xs:
    case LNil{}:
      LVNil{}
    case LCons{w, t, rest}:
      lview_tree(w, t, rest)

def ltree_list(t: LTree, acc: List<&2, Local>) -> List<&2, Local>:
  match t:
    case LLeaf{x}:
      x <> acc
    case LNode{x, l, r}:
      x <> ltree_list(l, ltree_list(r, acc))

def to_list(xs: Locals) -> List<&2, Local>:
  match xs:
    case LNil{}:
      []
    case LCons{w, t, rest}:
      ltree_list(t, to_list(rest))

def from_list(xs: List<&2, Local>) -> Locals:
  match xs:
    case Nil{}:
      LNil{}
    case Con{x, rest}:
      lcons(x, from_list(rest))

type Ctx is Data:
  Ctx{globals: Global.T, env: List<&2, Value.T>, locals: Locals, size: Nat, budget: Budget.T, level_arity: Nat, levels_checked: Bool}

def make(globals: Global.T, budget: Budget.T, level_arity: Nat) -> Ctx:
  Ctx{globals, [], LNil{}, 0n, budget, level_arity, False{}}

def globals_of(ctx: Ctx) -> Global.T:
  match ctx:
    case Ctx{globals, env, locals, size, budget, arity, checked}:
      globals

def env_of(ctx: Ctx) -> List<&2, Value.T>:
  match ctx:
    case Ctx{globals, env, locals, size, budget, arity, checked}:
      env

def size_of(ctx: Ctx) -> Nat:
  match ctx:
    case Ctx{globals, env, locals, size, budget, arity, checked}:
      size

def names(locals: List<&2, Local>) -> List<&2, String>:
  match locals:
    case Nil{}:
      []
    case Con{Local{name, q, ty}, rest}:
      name <> names(rest)

def names_of(ctx: Ctx) -> List<&2, String>:
  match ctx:
    case Ctx{globals, env, locals, size, budget, arity, checked}:
      names(to_list(locals))

def bind(name: String, q: Quantity.T, ty: Value.T, ctx: Ctx) -> Ctx:
  match ctx:
    case Ctx{globals, env, locals, +size, budget, arity, checked}:
      Ctx{globals, Value.var(size) <> env, lcons(Local{name, q, ty}, locals), 1n+size, budget, arity, checked}

def define(name: String, q: Quantity.T, ty: Value.T, value: Value.T, ctx: Ctx) -> Ctx:
  match ctx:
    case Ctx{globals, env, locals, size, budget, arity, checked}:
      Ctx{globals, value <> env, lcons(Local{name, q, ty}, locals), 1n+size, budget, arity, checked}

def readable(mode: Quantity.T, q: Quantity.T) -> Bool:
  Bool.or(Quantity.equal(mode, Quantity.QZero{}), Bool.not(Quantity.equal(q, Quantity.QZero{})))
