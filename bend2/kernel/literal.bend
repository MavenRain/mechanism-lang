import Base
import ./bignum.bend as Bignum

type T is Data:
  LString{value: String}
  LInt{value: Bignum.T}

def equal(a: T, b: T) -> Bool:
  match a b:
    case LString{x} LString{y}:
      String.eq(x, y)
    case LInt{x} LInt{y}:
      Bignum.equal(x, y)
    case LString{x} LInt{y}:
      False{}
    case LInt{x} LString{y}:
      False{}
