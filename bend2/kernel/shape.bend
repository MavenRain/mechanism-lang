import Base
import ./quantity.bend as Quantity
import ./bignum.bend as Bignum

type T<-A: Data> is Data:
  SPi{quantity: Quantity.T, name: String, domain: A}
  SArg{quantity: Quantity.T, name: String}
  SColl{count: Bignum.T}
  SPar{left: A, right: A}
  SMu{name: String, args: List<&2, A>}
  SNu{name: String, args: List<&2, A>}
  SZk{quantity: Quantity.T, name: String, domain: A}
  SFhc{level: A}
  SMpc{parties: A, access: A}

def coll(-A: Data, count: Nat) -> T<A>:
  SColl{Bignum.of_nat(count)}

def payload(-A: Data, s: T<A>) -> List<&2, A>:
  match s:
    case SPi{q, name, dom}:
      [dom]
    case SArg{q, name}:
      []
    case SColl{count}:
      []
    case SPar{left, right}:
      [left, right]
    case SMu{name, args}:
      args
    case SNu{name, args}:
      args
    case SZk{q, name, dom}:
      [dom]
    case SFhc{level}:
      [level]
    case SMpc{parties, access}:
      [parties, access]

def family(-A: Data, s: T<A>) -> Maybe<&2, String>:
  match s:
    case SMu{name, args}:
      Some{name}
    case SNu{name, args}:
      Some{name}
    case SPi{q, name, dom}:
      None{}
    case SArg{q, name}:
      None{}
    case SColl{count}:
      None{}
    case SPar{left, right}:
      None{}
    case SZk{q, name, dom}:
      None{}
    case SFhc{level}:
      None{}
    case SMpc{parties, access}:
      None{}

def point_dom(-A: Data, s: T<A>) -> Maybe<&2, A>:
  match s:
    case SPi{q, name, dom}:
      Some{dom}
    case SZk{q, name, dom}:
      Some{dom}
    case _:
      None{}

def is_pi(-A: Data, s: T<A>) -> Bool:
  match s:
    case SPi{q, name, dom}:
      True{}
    case _:
      False{}

def name(-A: Data, shape: T<A>) -> String:
  match shape:
    case SPi{q,x,dom}: "SPi"
    case SArg{q,x}: "SArg"
    case SColl{n}: "SColl"
    case SPar{a,b}: "SPar"
    case SMu{n,args}: "SMu"
    case SNu{n,args}: "SNu"
    case SZk{q,x,dom}: "SZk"
    case SFhc{level}: "SFhc"
    case SMpc{p,a}: "SMpc"

def declared() -> List<&2,String>:
  ["SPi","SArg","SColl","SPar","SMu","SNu","SZk","SFhc","SMpc"]
