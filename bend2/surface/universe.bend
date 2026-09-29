import Base
import ../kernel/level.bend as Level
import ../kernel/level_var.bend as LevelVar
import ../kernel/bignum.bend as Bignum
import ../kernel/error.bend as Error

# Surface universe expressions retain their syntax; lowering is separate.
type T is Data:
  NatLevel{value: Bignum.T}
  VarLevel{index: Nat}
  SuccLevel{value: T}
  MaxLevel{left: T, right: T}
  IMaxLevel{left: T, right: T}

def text(level: T) -> String:
  match level:
    case NatLevel{value}:
      Bignum.to_string(value)
    case VarLevel{index}:
      "u" ++ Nat.show(index)
    case SuccLevel{value}:
      "(succ " ++ text(value) ++ ")"
    case MaxLevel{left, right}:
      "(max " ++ text(left) ++ " " ++ text(right) ++ ")"
    case IMaxLevel{left, right}:
      "(imax " ++ text(left) ++ " " ++ text(right) ++ ")"

def lower_nat(value: Maybe<&2, Bignum.T>) -> Result<&2, &2, Error.T, Level.T>:
  match value:
    case Some{n}:
      Done{LevelVar.offset(n, Level.zero())}
    case None{}:
      Fail{Error.Universe{"a sort level must be nonnegative"}}

def lower(level: T) -> Result<&2, &2, Error.T, Level.T>:
  match level:
    case NatLevel{value}:
      lower_nat(Some{value})
    case VarLevel{index}:
      Done{Level.var(index)}
    case SuccLevel{value}:
      do Result<&2, &2, Error.T, Level.T>:
        value : Level.T <- lower(value)
        return Level.succ(value)
    case MaxLevel{left, right}:
      do Result<&2, &2, Error.T, Level.T>:
        left : Level.T <- lower(left)
        right : Level.T <- lower(right)
        return Level.max(left, right)
    case IMaxLevel{left, right}:
      do Result<&2, &2, Error.T, Level.T>:
        left : Level.T <- lower(left)
        right : Level.T <- lower(right)
        return Level.imax(left, right)
