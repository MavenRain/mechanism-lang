import Base
import ./common.bend as Common
import ./quantity.bend as Quantity
import ./term.bend as Term
import ./shape.bend as Shape
import ./level.bend as Level
import ./literal.bend as Literal
import ./bignum.bend as Bignum

type Head is Data:
  HLocal{level: Nat}
  HGlobal{name: String}

type Closure<-A: Data> is Data:
  Closure{env: List<&2, A>, body: Term.T}

type VLeg<-A: Data> is Data:
  VLeg{binders: List<&2, Common.Pair<Quantity.T, String>>, clo: Closure<A>}

type VAddr<-A: Data> is Data:
  VAPt{quantity: Quantity.T, arg: A}
  VALeg{index: Bignum.T}
  VACtor{name: String}

def valeg(-A: Data, index: Nat) -> VAddr<A>:
  VALeg{Bignum.of_nat(index)}

type StuckElim<-A: Data> is Data:
  StuckElim{shape: Shape.T<A>, scrut_q: Quantity.T, motive: Maybe<&2, Term.Motive<Term.T>>, branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, env: List<&2, A>}

type Spine<-A: Data> is Data:
  SOut{shape: Shape.T<A>, addr: VAddr<A>}
  SElim{data: StuckElim<A>}

type T is Data:
  VUniv{level: Level.T}
  VLan{shape: Shape.T<T>, diagram: Closure<T>, level: Maybe<&2, Level.T>}
  VRan{shape: Shape.T<T>, diagram: Closure<T>, level: Maybe<&2, Level.T>}
  VIn{shape: Shape.T<T>, addr: VAddr<T>, args: List<&2, T>}
  VSec{shape: Shape.T<T>, legs: List<&2, VLeg<T>>}
  VLit{value: Literal.T}
  VNeutral{head: Head, spine: List<&2, Spine<T>>}

def var(level: Nat) -> T:
  VNeutral{HLocal{level}, []}

def close(env: List<&2, T>, body: Term.T) -> Closure<T>:
  Closure{env, body}

def as_univ(value: T) -> Maybe<&2, Level.T>:
  match value:
    case VUniv{level}:
      Some{level}
    case VLan{s, d, u}:
      None{}
    case VRan{s, d, u}:
      None{}
    case VIn{s, a, args}:
      None{}
    case VSec{s, legs}:
      None{}
    case VLit{lit}:
      None{}
    case VNeutral{head, spine}:
      None{}

def head_equal(a: Head, b: Head) -> Bool:
  match a b:
    case HLocal{x} HLocal{y}:
      Nat.is_eq(x, y)
    case HGlobal{x} HGlobal{y}:
      String.eq(x, y)
    case HLocal{x} HGlobal{y}:
      False{}
    case HGlobal{x} HLocal{y}:
      False{}
