import Base
import ./ndjson.bend as Json

type T is Data:
  LZero{}
  LSucc{index: String}
  LMax{left: String, right: String}
  LImax{left: String, right: String}
  LParam{index: String}

type LevelPair is Data:
  LevelPair{left: String, right: String}

def pair_items(values: List<&2, String>) -> Result<&2, &2, String, LevelPair>:
  match values:
    case Con{left, Con{right, Nil{}}}:
      Done{LevelPair{left, right}}
    case other:
      Fail{"expected exactly two level indices"}

def pair(value: Json.T) -> Result<&2, &2, String, LevelPair>:
  do Result<&2, &2, String, LevelPair>:
    items : List<&2, String> <- Json.nat_list(value)
    pair_items(items)

def maximum(value: LevelPair, imax: Bool) -> T:
  match value:
    case LevelPair{+left, +right}:
      Bool.pick(T, imax, LImax{left, right}, LMax{left, right})

def parse(tag: String, value: Json.T) -> Result<&2, &2, String, T>:
  match tag:
    case "succ":
      do Result<&2, &2, String, T>:
        index : String <- Json.nat(value)
        return LSucc{index}
    case "param":
      do Result<&2, &2, String, T>:
        index : String <- Json.nat(value)
        return LParam{index}
    case "max":
      do Result<&2, &2, String, T>:
        both : LevelPair <- pair(value)
        return maximum(both, False{})
    case "imax":
      do Result<&2, &2, String, T>:
        both : LevelPair <- pair(value)
        return maximum(both, True{})
    case unknown:
      Fail{"unknown level tag " ++ unknown}

def name_references(value: T) -> List<&2, String>:
  match value:
    case LParam{index}:
      [index]
    case other:
      Nil{}

def level_references(value: T) -> List<&2, String>:
  match value:
    case LSucc{index}:
      [index]
    case LMax{left, right}:
      [left, right]
    case LImax{left, right}:
      [left, right]
    case other:
      Nil{}
