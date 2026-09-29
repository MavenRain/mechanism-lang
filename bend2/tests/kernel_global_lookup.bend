import Base
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/term.bend as T
import ../kernel/level.bend as L

def entry(name: String) -> G.Entry:
  G.Axiom{T.Global{name}}

def entries() -> List<&2, C.Pair<String, G.Entry>>:
  [C.Pair{"x", entry("first")}, C.Pair{"y", entry("tail")}, C.Pair{"x", entry("second")}]

def entry_is(result: Maybe<&2, G.Entry>, expected: String) -> Bool:
  match result:
    case Some{G.Axiom{T.Global{name}}}: String.eq(name, expected)
    case other: False{}

def family(name: String) -> P.Family:
  P.Family{name, [], [], L.zero(), P.Builtin{}, [], False{}}

def families() -> List<&2, C.Pair<String, P.Family>>:
  [C.Pair{"x", family("first")}, C.Pair{"y", family("tail")}, C.Pair{"x", family("second")}]

def family_is(result: Maybe<&2, P.Family>, expected: String) -> Bool:
  match result:
    case Some{P.Family{name, p, i, l, status, ctors, positive}}: String.eq(name, expected)
    case other: False{}

def ctors() -> List<&2, P.Ctor>:
  [P.Ctor{"x", [], [], 1n, False{}}, P.Ctor{"y", [], [], 2n, False{}}, P.Ctor{"x", [], [], 3n, False{}}]

def ctor_is(result: Maybe<&2, P.Ctor>, expected: Nat) -> Bool:
  match result:
    case Some{P.Ctor{name, args, idx, arity, rec}}: Nat.is_eq(arity, expected)
    case other: False{}

def missing(-A: Data, result: Maybe<&2, A>) -> Bool:
  match result:
    case None{}: True{}
    case Some{value}: False{}

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}: IO.print("PASS global, family and constructor first-match lookup boundaries")
    case False{}: IO.die(Unit, 1, "FAIL first-match lookup boundaries")

def main() -> IO(Unit):
  verify(entry_is(G.find_entry(entries(), "x"), "first") && entry_is(G.find_entry(entries(), "y"), "tail") && missing(G.Entry, G.find_entry(entries(), "absent")) && missing(G.Entry, G.find_entry([], "x")) && family_is(G.find_family_entry(families(), "x"), "first") && family_is(G.find_family_entry(families(), "y"), "tail") && missing(P.Family, G.find_family_entry(families(), "absent")) && missing(P.Family, G.find_family_entry([], "x")) && ctor_is(P.find_ctor(ctors(), "x"), 1n) && ctor_is(P.find_ctor(ctors(), "y"), 2n) && missing(P.Ctor, P.find_ctor(ctors(), "absent")) && missing(P.Ctor, P.find_ctor([], "x")))
