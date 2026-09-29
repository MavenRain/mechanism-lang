import Base
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../kernel/error.bend as Error

def exact(result: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>, expected: String) -> Bool:
  match result:
    case Fail{error}: String.eq(Error.to_string(error), expected)
    case Done{decls}: False{}

def main() -> Bool:
  exact(Parser.program("poly (u) mu P : Prop with where end"), "line 1, column 33: expected a nonempty list of member definitions, found 'end'") &&
  exact(Parser.program("poly (u) mu P : Prop with where def end"), "line 1, column 33: expected 'def' or 'end' in family members, found 'def'") &&
  exact(Parser.program("poly (u) mu P : Prop with where def 0 end"), "line 1, column 33: expected 'def' or 'end' in family members, found 'def'") &&
  exact(Parser.program("\n\npoly (u) def rec p : Sort u := p"), "line 3, column 1: expected a nonrecursive definition or single family after universe binders, found 'poly'")
