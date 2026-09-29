import Base

type T<-A: Data> is Data:
  Table{tree: Map<&2, Maybe<&2, A>>}

type Indexed<-A: Data> is Data:
  Indexed{index: String, value: A}

def empty(-A: Data) -> T<A>:
  Table{Map.new(&2, Maybe<&2, A>)}

def put(-A: Data, table: T<A>, key: String, value: A) -> T<A>:
  match table:
    case Table{tree}:
      Table{Map.set(&2, Maybe<&2, A>, tree, key, Some{value})}

def get_result(-A: Data, result: Map<&2, Maybe<&2, A>> & Maybe<&2, A>) -> Maybe<&2, A>:
  (remaining, value) = result
  value

def get(-A: Data, table: T<A>, key: String) -> Maybe<&2, A>:
  match table:
    case Table{tree}:
      get_result(A, Map.get(Maybe<&2, A>, None{}, tree, key))

def present(-A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case None{}:
      False{}
    case Some{item}:
      True{}

def has(-A: Data, table: T<A>, key: String) -> Bool:
  present(A, get(A, table, key))

def keys(-A: Data, table: T<A>) -> List<&2, String>:
  match table:
    case Table{tree}:
      Map.keys(&2, Maybe<&2, A>, tree)

def size(-A: Data, table: T<A>) -> Nat:
  List.length(&2, String, keys(A, table))

def default(-A: Data, value: Maybe<&2, A>, fallback: A) -> A:
  match value:
    case None{}:
      fallback
    case Some{item}:
      item

def require(-A: Data, value: Maybe<&2, A>, message: String) -> Result<&2, &2, String, A>:
  match value:
    case None{}:
      Fail{message}
    case Some{item}:
      Done{item}

def lookup(-A: Data, table: T<A>, +kind: String, +index: String) -> Result<&2, &2, String, A>:
  require(A, get(A, table, index), "undefined " ++ kind ++ " index " ++ index)

def reference_present(yes: Bool, kind: String, index: String) -> Result<&2, &2, String, Unit>:
  match yes:
    case True{}:
      Done{Unit{}}
    case False{}:
      Fail{"undefined or forward " ++ kind ++ " index " ++ index}

def references(-A: Data, indices: List<&2, String>, +table: T<A>, +kind: String) -> Result<&2, &2, String, Unit>:
  match indices:
    case Nil{}:
      Done{Unit{}}
    case Con{+index, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- reference_present(has(A, table, index), kind, index)
        references(A, rest, table, kind)
