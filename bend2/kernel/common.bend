import Base

type Pair<-A: Data, -B: Data> is Data:
  Pair{first: A, second: B}

def at(-A: Data, xs: List<&2, A>, index: Nat) -> Maybe<&2, A>:
  List.get(&2, A, xs, index)
