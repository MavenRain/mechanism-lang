import Base
import ../kernel/check.bend as Check
import ../kernel/common.bend as Pair
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/prim.bend as Prim
import ../kernel/bignum.bend as Big
import ../kernel/literal.bend as Lit
import ../kernel/positivity.bend as Pos
import ../kernel/error.bend as Error

def success(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Done{x}:
      True{}
    case Fail{error}:
      False{}

def index_error(result: Result<&2, &2, Error.T, Global.T>) -> Bool:
  match result:
    case Fail{Error.Index_not_zero{msg}}:
      True{}
    case _:
      False{}

def budget_error(result: Result<&2, &2, Error.T, Global.T>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{msg}}:
      True{}
    case _:
      False{}

def positive_family(fam: Maybe<&2, Pos.Family>) -> Bool:
  match fam:
    case Some{Pos.Family{name, p, i, l, Pos.Complete{[ctor]}, [Pos.Ctor{c, [], [], 0n, False{}}], True{}}}:
      Bool.and(String.eq(name, "Flag"), Bool.and(String.eq(ctor, "flag"), String.eq(c, "flag")))
    case _:
      False{}

def installed(result: Result<&2, &2, Error.T, Global.T>) -> Bool:
  match result:
    case Done{globals}:
      positive_family(Global.find_family("Flag", globals))
    case Fail{e}:
      False{}

def family_test(result: Result<&2, &2, Error.T, Global.T>) -> Bool:
  match result:
    case Done{+globals}:
      Bool.and(installed(Check.define_ctors(globals, Budget.unlimited(), ["Flag"], "Flag", [Check.CtorDecl{"flag", [], [], []}])), budget_error(Check.define_ctors(globals, Budget.of_polls(1n), ["Flag"], "Flag", [Check.CtorDecl{"flag", [], [], []}])))
    case Fail{error}:
      False{}

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS declarations, family installation, erased indices, late budget")
    case False{}:
      IO.die(Unit, 1, "FAIL declaration kernel")

def declaration() -> Check.Decl:
  Check.Decl{"zero", Check.Definition{}, Prim.nat_ty(), Some{Term.Lit{Lit.LInt{Big.zero()}}}}

def main() -> IO(Unit):
  +g = Global.initial()
  +d = declaration()
  verify(Bool.and(success(Global.Entry, Check.check_decl(g, Budget.unlimited(), d)), Bool.and(success(List<&2, Pair.Pair<String, Global.Entry>>, Check.check_decls(g, Budget.unlimited(), [d, Check.Decl{"copy", Check.Definition{}, Prim.nat_ty(), Some{Term.Global{"zero"}}}])), Bool.and(family_test(Check.declare_family(g, Budget.unlimited(), Check.FamilyDecl{"Flag", [], [], Level.one()})), index_error(Check.declare_family(g, Budget.unlimited(), Check.FamilyDecl{"Bad", [], [Pos.Binder{Q.QOne{}, "i", Prim.nat_ty()}], Level.one()}))))))
