import Base
import ../kernel/bignum.bend as Width
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/value.bend as V
import ../kernel/level.bend as L
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/common.bend as Pair
import ../kernel/error.bend as E
import ../kernel/prim.bend as Prim
import ../kernel/positivity.bend as Pos


def success(-A: Data, result: Result<&2, &2, E.T, A>) -> Bool:
  match result:
    case Done{x}:
      True{}
    case Fail{e}:
      False{}

def scope(-A: Data, result: Result<&2, &2, E.T, A>) -> Bool:
  match result:
    case Fail{E.Universe{msg}}:
      String.eq(msg, "universe level is outside the global parameter scope")
    case _:
      False{}

def mismatch(result: Result<&2, &2, E.T, Unit>) -> Bool:
  match result:
    case Fail{E.Mismatch{msg}}:
      True{}
    case _:
      False{}

def budget_error(result: Result<&2, &2, E.T, Unit>) -> Bool:
  match result:
    case Fail{E.Budget_exhausted{msg}}:
      True{}
    case _:
      False{}

def universe(result: Result<&2, &2, E.T, V.T>) -> Bool:
  match result:
    case Done{V.VUniv{level}}:
      L.equal(level, L.zero())
    case _:
      False{}

def definition(name: String, +level: L.T) -> Check.Decl:
  Check.Decl{name, Check.Definition{}, T.Univ{L.succ(level)}, Some{T.Univ{level}}}

def distributed() -> Check.Decl:
  +u = L.var(0n)
  +v = L.var(1n)
  +w = L.var(2n)
  Check.Decl{"distribution", Check.Definition{}, T.Univ{L.succ(L.max(L.imax(u,v),L.imax(u,w)))}, Some{T.Univ{L.imax(u,L.max(v,w))}}}

def open_universe() -> T.T:
  T.Univ{L.var(0n)}

def raw_scope_terms() -> List<&2, T.T>:
  +u = open_universe()
  [u, T.Univ{L.imax(L.var(0n),L.zero())},
   T.Ann{T.Univ{L.zero()},u},
   T.Sec{S.SPi{Q.QMany{},"x",u},[T.Leg{[Pair.Pair{Q.QMany{},"x"}],T.Var{0n}}]},
   T.In{S.SPi{Q.QMany{},"x",u},T.ALeg{Width.of_nat(0n)},[]},
   T.Out{S.SPi{Q.QMany{},"x",u},T.ALeg{Width.of_nat(0n)},T.Global{"Nat"}},
   T.In{S.SColl{Width.of_nat(1n)},T.APt{Q.QZero{},u},[]},
   T.Elim{T.ElimData{S.SColl{Width.of_nat(0n)},T.Global{"Nat"},Q.QZero{},Some{T.Motive{None{},[],"self",u}},[]}},
   T.Elim{T.ElimData{S.SColl{Width.of_nat(1n)},T.Global{"Nat"},Q.QZero{},None{},[Pair.Pair{T.ALeg{Width.of_nat(0n)},T.Leg{[],u}}]}},
   T.Let{"x",T.Univ{L.zero()},u,T.Var{0n}}]

def all_scoped(terms: List<&2, T.T>) -> Bool:
  match terms:
    case Nil{}:
      True{}
    case Con{+term, rest}:
      Bool.and(scope(V.T,Check.infer_term(G.initial(),B.unlimited(),term)),Bool.and(scope(Unit,Check.check_term(G.initial(),B.unlimited(),term,V.VUniv{L.one()})),all_scoped(rest)))

def cases() -> List<&2, Pair.Pair<String, Bool>>:
  +g = G.initial()
  [Pair.Pair{"symbolic-distribution",success(Unit,Check.check_scheme(g,B.unlimited(),3n,distributed()))},
   Pair.Pair{"prop-impredicativity",universe(Check.infer_term(g,B.unlimited(),T.Ran{S.SPi{Q.QZero{},"P",T.Univ{L.zero()}},T.Var{0n}}))},
   Pair.Pair{"finite-sampling-counterfeit",mismatch(Check.check_scheme(g,B.unlimited(),1n,Check.Decl{"bad",Check.Definition{},T.Univ{L.succ(L.of_u32(8))},Some{T.Univ{L.max(L.of_u32(8),L.var(0n))}}}))},
   Pair.Pair{"imax-zero-branch",mismatch(Check.check_scheme(g,B.unlimited(),2n,Check.Decl{"bad",Check.Definition{},T.Univ{L.succ(L.max(L.var(0n),L.var(1n)))},Some{T.Univ{L.imax(L.var(0n),L.var(1n))}}}))},
   Pair.Pair{"raw-fields-scope",all_scoped(raw_scope_terms())},
   Pair.Pair{"ordinary-template-escape",scope(G.Entry,Check.check_decl(g,B.unlimited(),definition("escaped",L.var(0n))))},
   Pair.Pair{"outside-scheme-scope",scope(Unit,Check.check_scheme(g,B.unlimited(),1n,definition("outside",L.var(1n))))},
   Pair.Pair{"closed-family",success(G.T,Check.declare_family(g,B.unlimited(),Check.FamilyDecl{"F",[],[],L.one()}))},
   Pair.Pair{"family-parameter-scope",scope(G.T,Check.declare_family(g,B.unlimited(),Check.FamilyDecl{"F",[Pos.Binder{Q.QZero{}, "A", T.Univ{L.var(0n)}}],[],L.one()}))},
   Pair.Pair{"scheme-budget",budget_error(Check.check_scheme(g,B.of_polls(0n),3n,distributed()))}]

@unsafe def run(rows: List<&2, Pair.Pair<String, Bool>>) -> IO(Unit):
  match rows:
    case Nil{}:
      IO.print("PASS universe schemes, impredicativity, 10 raw-field scopes, family scope, budget")
    case Con{Pair.Pair{name, False{}}, rest}:
      IO.die(Unit, 1, "FAIL " ++ name)
    case Con{Pair.Pair{name, True{}}, rest}:
      run(rest)

def main() -> IO(Unit):
  run(cases())
