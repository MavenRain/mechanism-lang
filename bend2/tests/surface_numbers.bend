import Base
import ./relational_common.bend as R
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

type NumericCase is Data:
  NumericCase{source: String, success: Bool, expected: String}

def cases() -> List<&2, NumericCase>:
  [NumericCase{"def p : Type 999999999999999999 := Prop", False{}, "mismatch: the term has type Type 1 and the expected type is Type 1000000000000000000\n"},
   NumericCase{"def p : Sort (succ 999999999999999999) := Sort 999999999999999999", True{}, "def p : Type 1000000000000000000 := Type 999999999999999999\n\n"},
   NumericCase{"def p : Nat := (tuple (0)).999999999999999999", False{}, "cannot infer: a section has no type of its own;  it needs an expected type\n"},
   NumericCase{"def p : Nat := (tuple (0) : prod (Nat)).999999999999999999", False{}, "wrong leg: the leg address is outside the collection\n"},
   NumericCase{"def p : Nat := (tuple (missing) : prod (Nat)).999999999999999999", False{}, "unbound: missing\n"},
   NumericCase{"def p : sum (Nat) := inj 999999999999999999 of 1 0", False{}, "wrong leg: the leg address is outside the collection\n"},
   NumericCase{"def p : sum (Nat) := inj 999999999999999999 of 1 missing", False{}, "wrong leg: the leg address is outside the collection\n"},
   NumericCase{"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 (x : Nat) => x", False{}, "wrong leg: the leg address is outside the collection\n"},
   NumericCase{"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 => 0", False{}, "missing branch: each branch binds its payload once\n"},
   NumericCase{"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 999999999999999999 (x : Nat) (y : Nat) => 0", False{}, "missing branch: each branch binds its payload once\n"},
   NumericCase{"def p : Nat := (0).999999999999999999", False{}, "mismatch: a projection reads a pair or a tuple, and this scrutinee is neither\n"},
   NumericCase{"def p : Nat := (tuple (0) : prod (Nat)).0", True{}, "def p : Nat := (Out SColl 1 (ALeg 0) ((Sec SColl 1 [ => 0]) : (Ran SColl 1 (Sec SColl 1 [ => Nat]))))\n\n"},
   NumericCase{"def p : sum (Nat) := inj 0 of 1 0", True{}, "def p : (Lan SColl 1 (Sec SColl 1 [ => Nat])) := (In SColl 1 (ALeg 0) [0])\n\n"},
   NumericCase{"def p : Nat := case (inj 0 of 1 0 : sum (Nat)) as s return Nat with | 0 (x : Nat) => x", True{}, "def p : Nat := (Elim SColl 1 ((In SColl 1 (ALeg 0) [0]) : (Lan SColl 1 (Sec SColl 1 [ => Nat]))) as s return Nat with | (ALeg 0) x => x)\n\n"},
   NumericCase{"def p : Type 1000000000000000000 := Prop", False{}, "line 1, column 14: numeric literal too long\n"},
   NumericCase{"def p : sum (Nat) := inj 1000000000000000000 of 1 0", False{}, "line 1, column 26: numeric literal too long\n"}]

def verify(success: Bool, expected: String, actual: Result<&2, &2, E.T, Elab.Rows>) -> Result<&2, &2, String, Unit>:
  match success actual:
    case True{} Done{rows}: R.require(String.eq(Elab.checked_form(rows) ++ "\n", expected), "numeric checked form differs from original")
    case False{} Fail{+error}: R.require(String.eq(E.to_string(error) ++ "\n", expected), "numeric refusal differs from original: " ++ E.to_string(error))
    case True{} Fail{error}: Fail{E.to_string(error)}
    case False{} Done{rows}: Fail{"numeric negative was accepted"}

def replay(rows: List<&2, NumericCase>, count: Nat) -> Result<&2, &2, String, Nat>:
  match rows:
    case Nil{}: Done{count}
    case Con{NumericCase{+source, success, expected}, rest}:
      do Result<&2, &2, String, Nat>:
        checked : Unit <- verify(success, expected, Elab.check_text(G.initial(), B.unlimited(), source))
        replay(rest, 1n+count)

def suite() -> Result<&2, &2, String, String>:
  do Result<&2, &2, String, String>:
    count : Nat <- replay(cases(), 0n)
    parsed : List<&2, Syntax.Decl> <- R.parse_roundtrip("def p : sum (Nat) := inj 999999999999999999 of 999999999999999999 0")
    Done{"oracle=" ++ Nat.show(count) ++ " huge-width-roundtrip=1"}

def arguments(args: List<String>) -> IO(Unit):
  R.finish("SURFACE-NUMBERS", suite())

def main() -> IO(Unit):
  arguments([])
