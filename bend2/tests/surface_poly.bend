import Base
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../surface/universe.bend as Universe
import ../kernel/error.bend as Error

def accepted(result: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> Bool:
  match result:
    case Done{decls}:
      True{}
    case Fail{error}:
      False{}

def rejected(source: String) -> Bool:
  Bool.not(accepted(Parser.program(source)))

def poly(result: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> Bool:
  match result:
    case Done{Con{Syntax.DPoly{1n, name, Syntax.SSort{Universe.SuccLevel{Universe.VarLevel{0n}}}, body}, Nil{}}}:
      String.eq(name, "universe")
    case other:
      False{}

def roundtrip_result(expected: String, result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{term}:
      String.eq(expected, Syntax.show(term))
    case Fail{error}:
      False{}

def roundtrip(result: Result<&2, &2, Error.T, Syntax.T>) -> Bool:
  match result:
    case Done{+term}:
      roundtrip_result(Syntax.show(term), Parser.term(Syntax.show(term)))
    case Fail{error}:
      False{}

def main() -> Bool:
  poly(Parser.program("poly (u) def universe : Sort (succ u) := Sort u")) &&
  accepted(Parser.program("poly (u) mu Box (A : Sort u) : Sort u with | box : A -> Box A")) &&
  accepted(Parser.program("poly (u) mu Box (A : Sort u) : Sort u with | box : A -> Box A where def id (x : Sort u) : Sort u := x end")) &&
  accepted(Parser.program("specialize Box (0) as SmallBox with (A := Nat) export (id := smallId)")) &&
  accepted(Parser.program("poly (u) group Bundle where specialize Box (u) as B export () def id (x : Sort u) : Sort u := x end")) &&
  accepted(Parser.program("poly (u) group Bundle where specialize Box (u) as B end")) &&
  rejected("poly (u, u) def a : Type := Type") &&
  rejected("poly (succ) def a : Type := Type") &&
  rejected("poly (u) def a : Sort missing := Type") &&
  rejected("poly (u) group Empty where end") &&
  rejected("specialize Box (0) as B export () with (A := Nat)") &&
  rejected("specialize Box (0) as B export () export ()") &&
  roundtrip(Parser.term("fun (0 A : Type) (x : A) => case x as z return A with | 0 => x | 1 => x"))
