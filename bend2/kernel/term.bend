import Base
import ./common.bend as Common
import ./shape.bend as Shape
import ./quantity.bend as Quantity
import ./level.bend as Level
import ./literal.bend as Literal
import ./bignum.bend as Bignum
import ./level_var.bend as LevelVar

type Addr<-A: Data> is Data:
  APt{quantity: Quantity.T, arg: A}
  ALeg{index: Bignum.T}
  ACtor{name: String}

def aleg(-A: Data, index: Nat) -> Addr<A>:
  ALeg{Bignum.of_nat(index)}

type Leg<-A: Data> is Data:
  Leg{binders: List<&2, Common.Pair<Quantity.T, String>>, body: A}

type Motive<-A: Data> is Data:
  Motive{ind: Maybe<&2, String>, indices: List<&2, String>, self: String, body: A}

type ElimData<-A: Data> is Data:
  ElimData{shape: Shape.T<A>, scrut: A, scrut_q: Quantity.T, motive: Maybe<&2, Motive<A>>, branches: List<&2, Common.Pair<Addr<A>, Leg<A>>>}

type T is Data:
  Var{index: Nat}
  Univ{level: Level.T}
  Lan{shape: Shape.T<T>, body: T}
  Ran{shape: Shape.T<T>, body: T}
  In{shape: Shape.T<T>, addr: Addr<T>, args: List<&2, T>}
  Elim{data: ElimData<T>}
  Sec{shape: Shape.T<T>, legs: List<&2, Leg<T>>}
  Out{shape: Shape.T<T>, addr: Addr<T>, head: T}
  Let{name: String, ty: T, value: T, body: T}
  Ann{body: T, ty: T}
  Global{name: String}
  Lit{value: Literal.T}
  Auto{}

def as_apt(-A: Data, addr: Addr<A>) -> Maybe<&2, Common.Pair<Quantity.T, A>>:
  match addr:
    case APt{q, arg}:
      Some{Common.Pair{q, arg}}
    case ALeg{index}:
      None{}
    case ACtor{name}:
      None{}

def as_aleg(-A: Data, addr: Addr<A>) -> Maybe<&2, Bignum.T>:
  match addr:
    case APt{q, arg}:
      None{}
    case ALeg{index}:
      Some{index}
    case ACtor{name}:
      None{}

def as_actor(-A: Data, addr: Addr<A>) -> Maybe<&2, String>:
  match addr:
    case APt{q, arg}:
      None{}
    case ALeg{index}:
      None{}
    case ACtor{name}:
      Some{name}

def addr_children(addr: Addr<T>) -> List<&2, T>:
  match addr:
    case APt{q, arg}:
      [arg]
    case ALeg{index}:
      []
    case ACtor{name}:
      []

def leg_children(legs: List<&2, Leg<T>>) -> List<&2, T>:
  match legs:
    case Nil{}:
      []
    case Con{Leg{binders, body}, rest}:
      body <> leg_children(rest)

def motive_children(motive: Maybe<&2, Motive<T>>) -> List<&2, T>:
  match motive:
    case None{}:
      []
    case Some{Motive{ind, indices, self, body}}:
      [body]

def branch_children(branches: List<&2, Common.Pair<Addr<T>, Leg<T>>>) -> List<&2, T>:
  match branches:
    case Nil{}:
      []
    case Con{Common.Pair{addr, Leg{binders, body}}, rest}:
      List.append(&2, T, addr_children(addr), body <> branch_children(rest))

def children(term: T) -> List<&2, T>:
  match term:
    case Var{index}:
      []
    case Univ{level}:
      []
    case Lan{shape, body}:
      List.append(&2, T, Shape.payload(T, shape), [body])
    case Ran{shape, body}:
      List.append(&2, T, Shape.payload(T, shape), [body])
    case In{shape, addr, args}:
      List.append(&2, T, Shape.payload(T, shape), List.append(&2, T, addr_children(addr), args))
    case Elim{ElimData{shape, scrut, scrut_q, motive, branches}}:
      List.append(&2, T, Shape.payload(T, shape), scrut <> List.append(&2, T, motive_children(motive), branch_children(branches)))
    case Sec{shape, legs}:
      List.append(&2, T, Shape.payload(T, shape), leg_children(legs))
    case Out{shape, addr, head}:
      List.append(&2, T, Shape.payload(T, shape), List.append(&2, T, addr_children(addr), [head]))
    case Let{name, ty, value, body}:
      [ty, value, body]
    case Ann{body, ty}:
      [body, ty]
    case Global{name}:
      []
    case Lit{value}:
      []
    case Auto{}:
      []

def member(names: List<&2, String>, +name: String) -> Bool:
  match names:
    case Nil{}:
      False{}
    case Con{x, rest}:
      Bool.or(String.eq(x, name), member(rest, name))

def family_member(name: Maybe<&2, String>, names: List<&2, String>) -> Bool:
  match name:
    case None{}:
      False{}
    case Some{x}:
      member(names, x)

def node_name(term: T, families: Bool, names: List<&2, String>) -> Bool:
  match term:
    case Global{name}:
      member(names, name)
    case Lan{shape, body}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case Ran{shape, body}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case In{shape, addr, args}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case Elim{ElimData{shape, scrut, scrut_q, motive, branches}}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case Sec{shape, legs}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case Out{shape, addr, head}:
      Bool.and(families, family_member(Shape.family(T, shape), names))
    case Var{index}:
      False{}
    case Univ{level}:
      False{}
    case Let{name, ty, value, body}:
      False{}
    case Ann{body, ty}:
      False{}
    case Lit{value}:
      False{}
    case Auto{}:
      False{}

@unsafe def exists_walk(todo: List<&2, T>, +families: Bool, +names: List<&2, String>) -> Bool:
  match todo:
    case Nil{}:
      False{}
    case Con{+term, rest}:
      Bool.or(node_name(term, families, names), exists_walk(List.append(&2, T, children(term), rest), families, names))

def exists_name(include_families: Bool, names: List<&2, String>, term: T) -> Bool:
  exists_walk([term], include_families, names)

law equal:
  for a: T
  for b: T
  Bool

@unsafe def equal_list(a: List<&2, T>, b: List<&2, T>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      Bool.and(equal(x, y), equal_list(xs, ys))
    case _ _:
      False{}

def equal_strings(a: List<&2, String>, b: List<&2, String>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      Bool.and(String.eq(x, y), equal_strings(xs, ys))
    case _ _:
      False{}

def equal_option_string(a: Maybe<&2, String>, b: Maybe<&2, String>) -> Bool:
  match a b:
    case None{} None{}:
      True{}
    case Some{x} Some{y}:
      String.eq(x, y)
    case _ _:
      False{}

def equal_binders(a: List<&2, Common.Pair<Quantity.T, String>>, b: List<&2, Common.Pair<Quantity.T, String>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Common.Pair{q, x}, xs} Con{Common.Pair{r, y}, ys}:
      Bool.and(Quantity.equal(q, r), Bool.and(String.eq(x, y), equal_binders(xs, ys)))
    case _ _:
      False{}

@unsafe def equal_shape(a: Shape.T<T>, b: Shape.T<T>) -> Bool:
  match a b:
    case Shape.SPi{q, x, p} Shape.SPi{r, y, t}:
      Bool.and(Quantity.equal(q, r), Bool.and(String.eq(x, y), equal(p, t)))
    case Shape.SZk{q, x, p} Shape.SZk{r, y, t}:
      Bool.and(Quantity.equal(q, r), Bool.and(String.eq(x, y), equal(p, t)))
    case Shape.SColl{x} Shape.SColl{y}:
      Bignum.equal(x, y)
    case Shape.SPar{x, p} Shape.SPar{y, q}:
      Bool.and(equal(x, y), equal(p, q))
    case Shape.SMu{x, p} Shape.SMu{y, q}:
      Bool.and(String.eq(x, y), equal_list(p, q))
    case Shape.SNu{x, p} Shape.SNu{y, q}:
      Bool.and(String.eq(x, y), equal_list(p, q))
    case Shape.SFhc{x} Shape.SFhc{y}:
      equal(x, y)
    case Shape.SMpc{x, p} Shape.SMpc{y, q}:
      Bool.and(equal(x, y), equal(p, q))
    case _ _:
      False{}

@unsafe def equal_addr(a: Addr<T>, b: Addr<T>) -> Bool:
  match a b:
    case APt{q, x} APt{r, y}:
      Bool.and(Quantity.equal(q, r), equal(x, y))
    case ALeg{x} ALeg{y}:
      Bignum.equal(x, y)
    case ACtor{x} ACtor{y}:
      String.eq(x, y)
    case _ _:
      False{}

@unsafe def equal_leg(a: Leg<T>, b: Leg<T>) -> Bool:
  match a b:
    case Leg{x, p} Leg{y, q}:
      Bool.and(equal_binders(x, y), equal(p, q))

@unsafe def equal_legs(a: List<&2, Leg<T>>, b: List<&2, Leg<T>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      Bool.and(equal_leg(x, y), equal_legs(xs, ys))
    case _ _:
      False{}

@unsafe def equal_motive(a: Maybe<&2, Motive<T>>, b: Maybe<&2, Motive<T>>) -> Bool:
  match a b:
    case None{} None{}:
      True{}
    case Some{Motive{i, xs, s, p}} Some{Motive{j, ys, t, q}}:
      Bool.and(equal_option_string(i, j), Bool.and(equal_strings(xs, ys), Bool.and(String.eq(s, t), equal(p, q))))
    case _ _:
      False{}

@unsafe def equal_branches(a: List<&2, Common.Pair<Addr<T>, Leg<T>>>, b: List<&2, Common.Pair<Addr<T>, Leg<T>>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Common.Pair{x, p}, xs} Con{Common.Pair{y, q}, ys}:
      Bool.and(equal_addr(x, y), Bool.and(equal_leg(p, q), equal_branches(xs, ys)))
    case _ _:
      False{}

@unsafe def equal_elim(a: ElimData<T>, b: ElimData<T>) -> Bool:
  match a b:
    case ElimData{s, t, q, m, bs} ElimData{u, v, r, n, cs}:
      Bool.and(equal_shape(s, u), Bool.and(equal(t, v), Bool.and(Quantity.equal(q, r), Bool.and(equal_motive(m, n), equal_branches(bs, cs)))))

@unsafe def equal(a, b):
  match a b:
    case Var{x} Var{y}:
      Nat.is_eq(x, y)
    case Univ{x} Univ{y}:
      LevelVar.structural_equal(x, y)
    case Lan{s, x} Lan{t, y}:
      Bool.and(equal_shape(s, t), equal(x, y))
    case Ran{s, x} Ran{t, y}:
      Bool.and(equal_shape(s, t), equal(x, y))
    case In{s, a, x} In{t, b, y}:
      Bool.and(equal_shape(s, t), Bool.and(equal_addr(a, b), equal_list(x, y)))
    case Elim{x} Elim{y}:
      equal_elim(x, y)
    case Sec{s, x} Sec{t, y}:
      Bool.and(equal_shape(s, t), equal_legs(x, y))
    case Out{s, a, x} Out{t, b, y}:
      Bool.and(equal_shape(s, t), Bool.and(equal_addr(a, b), equal(x, y)))
    case Let{x, a, b, c} Let{y, d, e, f}:
      Bool.and(String.eq(x, y), Bool.and(equal(a, d), Bool.and(equal(b, e), equal(c, f))))
    case Ann{x, a} Ann{y, b}:
      Bool.and(equal(x, y), equal(a, b))
    case Global{x} Global{y}:
      String.eq(x, y)
    case Lit{x} Lit{y}:
      Literal.equal(x, y)
    case Auto{} Auto{}:
      True{}
    case _ _:
      False{}

def formers() -> List<&2,String>:
  ["Lan","Ran"]

def schema() -> List<&2,String>:
  ["In","Elim","Sec","Out"]
