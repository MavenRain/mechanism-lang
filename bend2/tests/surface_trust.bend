import Base
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/check.bend as Check
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as Positivity
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/rules.bend as Rules
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def ensure(ok: Bool, message: String) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, ok, Done{Unit{}}, Fail{Error.Mismatch{message}})

def refused(-A: Data, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{error}: True{}
    case Done{value}: False{}

def exact(-A: Data, expected: String, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{error}: String.eq(expected, Error.to_string(error))
    case Done{value}: False{}

def expect_exact(-A: Data, +expected: Error.T, result: Result<&2, &2, Error.T, A>) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Fail{+error}:
      ensure(String.eq(Error.to_string(expected), Error.to_string(error)), "expected " ++ Error.to_string(expected) ++ "; actual " ++ Error.to_string(error))
    case Done{value}: Fail{Error.Mismatch{"expected rejection: " ++ Error.to_string(expected)}}

def family_type(name: String) -> Term.T:
  Term.Lan{Shape.SMu{name, []}, Rules.diagram_of([])}

def precedence() -> Result<&2, &2, Error.T, Unit>:
  +family : Check.FamilyDecl = Check.FamilyDecl{"First", [], [], Level.one()}
  +ctors : List<&2, Check.CtorDecl> = [Check.CtorDecl{"first", [], [], []}]
  +members : List<&2, Check.Decl> = [Check.Decl{"badBody", Check.Definition{}, family_type("First"), Some{Term.Var{0n}}}, Check.Decl{"badLevel", Check.Definition{}, Term.Univ{Level.var(1n)}, Some{Term.Univ{Level.zero()}}}]
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- expect_exact(Family.T, Error.Unbound{"de Bruijn index 0 is outside the context"}, Family.declare_group(Global.empty(), [], Budget.unlimited(), 1n, [Common.Pair{family, ctors}], members))
    expect_exact(Family.T, Error.Universe{"universe level is outside the global parameter scope"}, Family.declare(Global.empty(), [], Budget.unlimited(), 1n, family, ctors, members))

def permutation() -> Result<&2, &2, Error.T, Unit>:
  +family : Check.FamilyDecl = Check.FamilyDecl{"Pair", [Positivity.Binder{Q.QZero{}, "A", Term.Univ{Level.one()}}, Positivity.Binder{Q.QZero{}, "B", Term.Univ{Level.one()}}], [], Level.one()}
  do Result<&2, &2, Error.T, Unit>:
    catalog : Family.T <- Family.declare(Global.empty(), [], Budget.unlimited(), 0n, family, [Check.CtorDecl{"pair", [], [Term.Var{1n}, Term.Var{0n}], []}], [])
    ensure(refused(Family.T, Family.declare(Global.empty(), [], Budget.unlimited(), 0n, family, [Check.CtorDecl{"pair", [], [Term.Var{0n}, Term.Var{1n}], []}], [])), "constructor parameter permutation accepted")

def indexed() -> Check.FamilyDecl:
  Check.FamilyDecl{"Indexed", [], [Positivity.Binder{Q.QZero{}, "i", Term.Global{"Nat"}}, Positivity.Binder{Q.QZero{}, "j", Term.Global{"Nat"}}], Level.one()}

def indexed_ctor() -> Check.CtorDecl:
  Check.CtorDecl{"indexed", [Positivity.Binder{Q.QMany{}, "x", Term.Global{"Nat"}}, Positivity.Binder{Q.QMany{}, "y", Term.Global{"Nat"}}], [], [Term.Var{1n}, Term.Var{0n}]}

def altered_ctor(ctor: Positivity.Ctor, +mode: Nat) -> Positivity.Ctor:
  match ctor:
    case Positivity.Ctor{name, args, indices, +arity, +recursive}:
      Positivity.Ctor{name, args, Bool.pick(List<&2, Term.T>, Nat.is_eq(mode, 0n), [Term.Var{0n}, Term.Var{1n}], indices), Bool.pick(Nat, Nat.is_eq(mode, 1n), (arity + 1n : Nat), arity), Bool.pick(Bool, Nat.is_eq(mode, 2n), Bool.not(recursive), recursive)}

def altered_ctors(ctors: List<&2, Positivity.Ctor>, +mode: Nat) -> List<&2, Positivity.Ctor>:
  match ctors:
    case Nil{}: []
    case Con{ctor, rest}: Con{altered_ctor(ctor, mode), altered_ctors(rest, mode)}

def altered(family: Positivity.Family, +mode: Nat) -> Positivity.Family:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      Positivity.Family{name, params, indices, level, status, altered_ctors(ctors, mode), Bool.pick(Bool, Nat.is_eq(mode, 3n), False{}, positive)}

def reuse_refused(+globals: Global.T, +catalog: Family.T, +family: Positivity.Family, mode: Nat) -> Bool:
  globals = Global.add_family("Existing", altered(family, mode), globals)
  refused(Global.T, Family.instantiate(globals, catalog, Budget.unlimited(), "Indexed", [], "Alias", [Common.Pair{"Indexed", "Existing"}], None{}))

def reuse_metadata(+globals: Global.T, +catalog: Family.T, family: Maybe<&2, Positivity.Family>) -> Result<&2, &2, Error.T, Unit>:
  match family:
    case None{}: Fail{Error.Mismatch{"indexed positive control missing family"}}
    case Some{+family}:
      do Result<&2, &2, Error.T, Unit>:
        checked : Unit <- ensure(reuse_refused(globals, catalog, family, 0n), "reused index permutation accepted")
        checked : Unit <- ensure(reuse_refused(globals, catalog, family, 1n), "reused constructor arity mutation accepted")
        checked : Unit <- ensure(reuse_refused(globals, catalog, family, 2n), "reused self-recursion mutation accepted")
        ensure(reuse_refused(globals, catalog, family, 3n), "reused positivity mutation accepted")

def reuse() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Family.declare(Global.initial(), [], Budget.unlimited(), 0n, indexed(), [indexed_ctor()], [])
    +globals : Global.T <- Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "Indexed", [], "Existing", [], None{})
    reused : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "Indexed", [], "Alias", [Common.Pair{"Indexed", "Existing"}], None{})
    reuse_metadata(globals, catalog, Global.find_family("Existing", globals))

def ambient() -> Result<&2, &2, Error.T, Unit>:
  +globals = Global.add("Ambient", Global.Axiom{Term.Univ{Level.one()}}, Global.initial())
  family : Check.FamilyDecl = Check.FamilyDecl{"Box", [], [], Level.one()}
  ctor : Check.CtorDecl = Check.CtorDecl{"box", [Positivity.Binder{Q.QMany{}, "value", Term.Global{"Ambient"}}], [], []}
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Family.declare(globals, [], Budget.unlimited(), 0n, family, [ctor], [])
    control : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "Box", [], "Good", [], None{})
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "Box", [], "Missing", [], None{})), "missing ambient global bypassed rechecking")
    ensure(refused(Global.T, Family.instantiate(Global.add("Ambient", Global.Axiom{Term.Global{"Nat"}}, globals), catalog, Budget.unlimited(), "Box", [], "Changed", [], None{})), "changed ambient global bypassed rechecking")

def ambient_member() -> Result<&2, &2, Error.T, Unit>:
  +globals = Global.add("Ambient", Global.Axiom{Term.Univ{Level.one()}}, Global.initial())
  family : Check.FamilyDecl = Check.FamilyDecl{"Box", [], [], Level.one()}
  ctor : Check.CtorDecl = Check.CtorDecl{"box", [], [], []}
  member : Check.Decl = Check.Decl{"m", Check.Definition{}, Term.Univ{Level.one()}, Some{Term.Global{"Ambient"}}}
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Family.declare(globals, [], Budget.unlimited(), 0n, family, [ctor], [member])
    control : Global.T <- Family.instantiate(globals, catalog, Budget.unlimited(), "Box", [], "Good", [], None{})
    checked : Unit <- ensure(refused(Global.T, Family.instantiate(Global.initial(), catalog, Budget.unlimited(), "Box", [], "Missing", [], None{})), "missing ambient global bypassed member rechecking")
    ensure(refused(Global.T, Family.instantiate(Global.add("Ambient", Global.Axiom{Term.Global{"Nat"}}, globals), catalog, Budget.unlimited(), "Box", [], "Changed", [], None{})), "changed ambient global bypassed member rechecking")

def box_dependency() -> Family.Dependency:
  Family.Dependency{"Box", [], "Inner", [], None{}}

def ambient_composed() -> Result<&2, &2, Error.T, Unit>:
  +globals = Global.add("Ambient", Global.Axiom{Term.Univ{Level.one()}}, Global.initial())
  family : Check.FamilyDecl = Check.FamilyDecl{"Box", [], [], Level.one()}
  ctor : Check.CtorDecl = Check.CtorDecl{"box", [], [], []}
  member : Check.Decl = Check.Decl{"m", Check.Definition{}, Term.Univ{Level.one()}, Some{Term.Global{"Ambient"}}}
  do Result<&2, &2, Error.T, Unit>:
    +catalog : Family.T <- Family.declare(globals, [], Budget.unlimited(), 0n, family, [ctor], [member])
    control : Family.T <- Family.compose(globals, catalog, Budget.unlimited(), 0n, "Good", [box_dependency()], [])
    checked : Unit <- ensure(refused(Family.T, Family.compose(Global.initial(), catalog, Budget.unlimited(), 0n, "Missing", [box_dependency()], [])), "missing ambient global bypassed composed member rechecking")
    ensure(refused(Family.T, Family.compose(Global.add("Ambient", Global.Axiom{Term.Global{"Nat"}}, globals), catalog, Budget.unlimited(), 0n, "Changed", [box_dependency()], [])), "changed ambient global bypassed composed member rechecking")

def mutual() -> Result<&2, &2, Error.T, Unit>:
  source : String = "poly (u) mu P : Sort (succ u) with | pp : P and Q : Sort (succ u) with | qq : P -> Q where def mkp : P := pp end mu A : Sort (succ 0) with | pp : A and B : Sort (succ 0) with | qq : A -> B specialize P (0) as X with (P := A, Q := B)"
  ensure(exact(Elab.Checked, Error.to_string(Error.Mismatch{"the reused family B is a member of a mutual group, which family reuse does not support"}), Elab.check_in(Global.empty(), Budget.unlimited(), source)), "mutual group member reuse refusal changed")

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- precedence()
    checked : Unit <- permutation()
    checked : Unit <- reuse()
    checked : Unit <- ambient()
    checked : Unit <- ambient_member()
    checked : Unit <- ambient_composed()
    mutual()

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}: IO.print("PASS family order, reuse metadata, rechecking, and refusal precedence")
    case Fail{error}: IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
