import Base
import ../surface/family_poly.bend as Family
import ../surface/poly.bend as Poly
import ../kernel/check.bend as Check
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def scheme() -> Family.Scheme:
  Family.Scheme{1n, Check.FamilyDecl{"Box", [], [], Level.var(0n)},
    [Check.CtorDecl{"box", [], [], []}],
    [Common.Pair{Check.FamilyDecl{"Tag", [], [], Level.var(0n)}, []}],
    [Check.Decl{"unwrap", Check.Definition{}, Term.Univ{Level.var(0n)}, Some{Term.Auto{}}}], Global.empty()}

def expected_names(result: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, family: String, companion: String, member: String) -> Bool:
  match result:
    case Some{Common.Pair{Con{first, Con{second, Nil{}}}, Con{helper, Nil{}}}}:
      String.eq(first, family) && String.eq(second, companion) && String.eq(helper, member)
    case other:
      False{}

def reused(result: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>) -> Bool:
  match result:
    case Some{Common.Pair{Con{name, Nil{}}, Con{member, Nil{}}}}:
      String.eq(name, "Small") && String.eq(member, "Small_unwrap")
    case other:
      False{}

def refused(result: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>) -> Bool:
  match result:
    case None{}:
      True{}
    case Some{value}:
      False{}

def postulate_refused(result: Result<&2, &2, Error.T, Check.Decl>) -> Bool:
  match result:
    case Fail{Error.Not_yet{message}}:
      String.eq(message, "family schema members must be definitions")
    case other:
      False{}

def main() -> Bool:
  +catalog = {[Common.Pair{"Box", scheme()}] : Family.T}
  expected_names(Family.instance_names(catalog, "Box", "Small", [], None{}), "Small", "Small_Tag", "Small_unwrap") &&
  expected_names(Family.instance_names(catalog, "Box", "Small", [], Some{[Common.Pair{"unwrap", "get"}]}), "Small", "Small_Tag", "get") &&
  reused(Family.instance_names(catalog, "Box", "Small", [Common.Pair{"Tag", "ExistingTag"}], None{})) &&
  refused(Family.instance_names(catalog, "Box", "Small", [], Some{[]})) &&
  refused(Family.instance_names(catalog, "Box", "Small", [], Some{[Common.Pair{"unwrap", "box"}]})) &&
  refused(Family.instance_names(catalog, "Box", "Small", [], Some{[Common.Pair{"unwrap", "Small_Tag"}]})) &&
  refused(Family.instance_names(catalog, "Box", "Small", [], Some{[Common.Pair{"missing", "get"}]})) &&
  postulate_refused(Budget.run(Check.Decl, Family.map_member(Check.Decl{"bad", Check.Postulate{}, Term.Univ{Level.var(9n)}, None{}}, Poly.Action{Poly.ScopeLevels{0n}, [], []}), Budget.unlimited()))
