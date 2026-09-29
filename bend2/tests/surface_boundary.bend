import Base
import ./surface_catalog.bend as Test
import ../surface/family_poly.bend as Family
import ../surface/poly.bend as Poly
import ../kernel/check.bend as Check
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as P
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def exact(-A: Data, expected: Error.T, result: Result<&2, &2, Error.T, A>) -> Bool:
  match result:
    case Fail{error}: String.eq(Error.to_string(expected), Error.to_string(error))
    case Done{value}: False{}

def eq_family(carrier: Level.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"Eq", [P.Binder{Q.QZero{}, "A", Term.Univ{carrier}}, P.Binder{Q.QZero{}, "x", Term.Var{0n}}], [P.Binder{Q.QZero{}, "y", Term.Var{1n}}], Level.zero()}

def refl_ctor() -> Check.CtorDecl:
  Check.CtorDecl{"refl", [], [Term.Var{1n}, Term.Var{0n}], [Term.Var{0n}]}

def empty(name: String, level: Level.T) -> Check.FamilyDecl:
  Check.FamilyDecl{name, [], [], level}

def scoped(family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>) -> Bool:
  Test.scope_refused(Family.T, Family.declare(Global.empty(), [], Budget.unlimited(), 1n, family, ctors, []))

def ignored_metadata() -> Term.T:
  +shape : Shape.T<Term.T> = Shape.SPi{Q.QMany{}, "A", Term.Univ{Level.one()}}
  identity : Term.T = Term.Ann{Term.Sec{shape, [Term.Leg{[Common.Pair{Q.QMany{}, "A"}], Term.Var{0n}}]}, Term.Ran{shape, Term.Univ{Level.one()}}}
  Term.Out{Shape.SPi{Q.QMany{}, "ignored", Term.Univ{Level.var(1n)}}, Term.APt{Q.QMany{}, Term.Univ{Level.zero()}}, identity}

def scope_fields() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    control : Family.T <- Family.declare(Global.empty(), [], Budget.unlimited(), 1n, eq_family(Level.var(0n)), [refl_ctor()], [])
    checked : Unit <- Test.ensure(scoped(empty("Header", Level.succ(Level.var(1n))), []), "free family header level accepted")
    checked : Unit <- Test.ensure(scoped(eq_family(Level.var(1n)), [refl_ctor()]), "free family parameter level accepted")
    checked : Unit <- Test.ensure(scoped(Check.FamilyDecl{"Index", [], [P.Binder{Q.QZero{}, "P", Term.Univ{Level.var(1n)}}], Level.zero()}, []), "free family index level accepted")
    checked : Unit <- Test.ensure(scoped(empty("Field", Level.succ(Level.one())), [Check.CtorDecl{"field", [P.Binder{Q.QZero{}, "P", Term.Univ{Level.var(1n)}}], [], []}]), "free constructor field level accepted")
    checked : Unit <- Test.ensure(scoped(eq_family(Level.var(0n)), [Check.CtorDecl{"refl", [], [Term.Ann{Term.Var{1n}, Term.Univ{Level.var(1n)}}, Term.Var{0n}], [Term.Var{0n}]}]), "free constructor result parameter annotation accepted")
    checked : Unit <- Test.ensure(scoped(eq_family(Level.var(0n)), [Check.CtorDecl{"refl", [], [Term.Var{1n}, Term.Var{0n}], [Term.Ann{Term.Var{0n}, Term.Univ{Level.var(1n)}}]}]), "free constructor result index annotation accepted")
    Test.ensure(scoped(empty("Hidden", Level.succ(Level.one())), [Check.CtorDecl{"hidden", [P.Binder{Q.QMany{}, "field", ignored_metadata()}], [], []}]), "ignored shape metadata bypassed scope traversal")

def counterfeit(carrier: Level.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"Counterfeit", [P.Binder{Q.QZero{}, "A", Term.Univ{Level.succ(carrier)}}], [], Level.one()}

def counterfeit_ctor() -> Check.CtorDecl:
  Check.CtorDecl{"counterfeit", [P.Binder{Q.QMany{}, "value", Term.Var{0n}}], [Term.Var{1n}], []}

def universality() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    globals : Global.T <- Check.declare_family_at(Global.empty(), Budget.unlimited(), 0n, counterfeit(Level.zero()))
    closed : Global.T <- Check.define_ctors_at(globals, Budget.unlimited(), 0n, ["Counterfeit"], "Counterfeit", [counterfeit_ctor()])
    checked : Unit <- Test.ensure(exact(Family.T, Error.Universe{"a field of counterfeit exceeds its family universe"}, Family.declare(Global.empty(), [], Budget.unlimited(), 1n, counterfeit(Level.var(0n)), [counterfeit_ctor()], [])), "closed sample substituted for universal field checking")
    Test.ensure(exact(Family.T, Error.Universe{"a family schema must remain in Prop or Type for every universe substitution"}, Family.declare(Global.empty(), [], Budget.unlimited(), 1n, empty("Mixed", Level.var(0n)), [], [])), "mixed Prop and Type family accepted")

def measured(-A: Data, state: Common.Pair<Budget.T, Result<&2, &2, Error.T, A>>) -> Result<&2, &2, Error.T, Nat>:
  match state:
    case Common.Pair{Budget.Polls{remaining}, Done{value}}: Done{Nat.sub(10000n, remaining)}
    case Common.Pair{budget, Fail{error}}: Fail{error}
    case other: Fail{Error.Mismatch{"measurement lost finite budget"}}

def measured_error(-A: Data, expected: Error.T, state: Common.Pair<Budget.T, Result<&2, &2, Error.T, A>>) -> Result<&2, &2, Error.T, Nat>:
  match state:
    case Common.Pair{Budget.Polls{remaining}, Fail{+error}}:
      Bool.pick(Result<&2, &2, Error.T, Nat>, String.eq(Error.to_string(expected), Error.to_string(error)), Done{Nat.sub(10000n, remaining)}, Fail{error})
    case other: Fail{Error.Mismatch{"expected a measured refusal"}}

def sentinel(globals: Global.T) -> Budget.Comp(Check.Decl):
  Budget.failed(Check.Decl, Error.Cannot_infer{"callback must not run"})

def cancel_callback(globals: Global.T) -> Budget.Comp(Check.Decl):
  Budget.pure(Check.Decl, Test.witness())

@unsafe def cancel_after(remaining: Nat) -> Budget.Callback:
  match remaining:
    case 0n:
      Budget.of_poll(unit => (cancel_after(0n), True{}))
    case 1n+rest:
      Budget.of_poll(unit => (cancel_after(rest), False{}))

def callback_boundaries() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    action : Budget.Comp(Family.T) = Family.declare_group_elaborated_comp(Global.empty(), [], 0n, [Common.Pair{Test.marker(), Test.ctors()}], [])
    +used : Nat <- measured(Family.T, Budget.run_state(Family.T, action, Budget.of_polls(10000n)))
    checked : Unit <- Test.ensure(Test.budget_refused(Family.T, Family.declare_group_elaborated(Global.empty(), [], Budget.of_polls(used), 0n, [Common.Pair{Test.marker(), Test.ctors()}], [sentinel])), "callback ran before the family/member budget guard")
    checked : Unit <- Test.ensure(Test.budget_refused(Family.T, Budget.run_poll(Family.T, Family.declare_group_elaborated_comp(Global.empty(), [], 0n, [Common.Pair{Test.marker(), Test.ctors()}], [cancel_callback, sentinel]), cancel_after(Nat.add(1n, used)))), "callback ran after member cancellation")
    checked : Unit <- Test.ensure(exact(Family.T, Error.Mismatch{"a family schema group must be nonempty"}, Family.declare_group_elaborated(Global.empty(), [], Budget.unlimited(), 0n, [], [sentinel])), "callback ran before family validation")
    checked : Unit <- Test.ensure(exact(Family.T, Error.Cannot_infer{"callback must not run"}, Family.declare_group_elaborated(Global.empty(), [], Budget.unlimited(), 0n, [Common.Pair{Test.marker(), Test.ctors()}], [sentinel, Test.second_callback])), "later callback replaced first refusal")
    return Unit{}

def count(-A: Data, values: List<&2, A>) -> Nat:
  match values:
    case Nil{}: 0n
    case Con{head, tail}: (1n + count(A, tail) : Nat)

def inventories(globals: Global.T, entries_count: Nat, families_count: Nat) -> Bool:
  match globals:
    case Global.Globals{entries, families, entry_index, family_index}:
      Nat.is_eq(count(Common.Pair<String, Global.Entry>, entries), entries_count) && Nat.is_eq(count(Common.Pair<String, P.Family>, families), families_count)

def dependency(name: String, alias: String) -> Family.Dependency:
  Family.Dependency{name, [], alias, [], None{}}

def nested(+catalog: Family.T, budget: Budget.T, members: List<&1, Family.MemberElaborator>) -> Result<&2, &2, Error.T, Family.T>:
  Family.compose(Global.empty(), catalog, budget, 0n, "Outer", [dependency("Inner", "Nested")], members)

def nested_comp(catalog: Family.T, members: List<&1, Family.MemberElaborator>) -> Budget.Comp(Family.T):
  Family.compose_comp(Global.empty(), catalog, 0n, "Outer", [dependency("Inner", "Nested")], members)

def nested_boundaries() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    catalog : Family.T <- Family.declare(Global.empty(), [], Budget.unlimited(), 0n, Test.marker(), Test.ctors(), [])
    +catalog : Family.T <- Family.compose(Global.empty(), catalog, Budget.unlimited(), 0n, "Inner", [dependency("Marker", "Local")], [])
    action : Budget.Comp(Family.T) = nested_comp(catalog, [])
    +used : Nat <- measured(Family.T, Budget.run_state(Family.T, action, Budget.of_polls(10000n)))
    checked : Unit <- Test.ensure(Test.budget_refused(Family.T, nested(catalog, Budget.of_polls(Nat.sub(used, 1n)), [])), "nested composition accepted exhausted budget")
    +closed : Family.T <- nested(catalog, Budget.of_polls(used), [])
    checked : Unit <- Test.ensure(Nat.is_eq(count(Common.Pair<String, Family.Scheme>, catalog), 2n) && Bool.not(Poly.is_some(Family.Scheme, Family.lookup(catalog, "Outer"))), "failed nested composition changed caller catalog")
    checked : Unit <- Test.ensure(Nat.is_eq(count(Common.Pair<String, Family.Scheme>, closed), 3n), "nested composition inventory changed")
    action : Budget.Comp(Family.T) = nested_comp(catalog, [sentinel, Test.second_callback])
    +failed_at : Nat <- measured_error(Family.T, Error.Cannot_infer{"callback must not run"}, Budget.run_state(Family.T, action, Budget.of_polls(10000n)))
    checked : Unit <- Test.ensure(Test.budget_refused(Family.T, nested(catalog, Budget.of_polls(Nat.sub(failed_at, 1n)), [sentinel, Test.second_callback])), "nested member refusal preceded its budget guard")
    checked : Unit <- Test.ensure(exact(Family.T, Error.Cannot_infer{"callback must not run"}, nested(catalog, Budget.of_polls(failed_at), [sentinel, Test.second_callback])), "nested callback first error changed")
    action : Budget.Comp(Global.T) = Family.instantiate_comp(Global.empty(), closed, "Outer", [], "Instance", [], None{})
    +instance_polls : Nat <- measured(Global.T, Budget.run_state(Global.T, action, Budget.of_polls(10000n)))
    checked : Unit <- Test.ensure(Test.budget_refused(Global.T, Family.instantiate(Global.empty(), closed, Budget.of_polls(Nat.sub(instance_polls, 1n)), "Outer", [], "Instance", [], None{})), "nested specialization accepted exhausted budget")
    +globals : Global.T <- Family.instantiate(Global.empty(), closed, Budget.of_polls(instance_polls), "Outer", [], "Instance", [], None{})
    checked : Unit <- Test.ensure(inventories(globals, 0n, 1n), "nested specialization family inventory changed")
    checked : Unit <- Test.ensure(Test.budget_refused(Family.T, Family.compose(Global.empty(), catalog, Budget.of_polls(1n), 0n, "Absent", [dependency("Missing", "Local")], [sentinel])), "dependency lookup preceded its budget guard")
    Test.ensure(inventories(Global.empty(), 0n, 0n) && Nat.is_eq(count(Common.Pair<String, Family.Scheme>, closed), 3n), "failed specialization altered caller inventories")

def suite() -> Result<&2, &2, Error.T, Unit>:
  do Result<&2, &2, Error.T, Unit>:
    checked : Unit <- scope_fields()
    checked : Unit <- universality()
    checked : Unit <- callback_boundaries()
    nested_boundaries()

def report(result: Result<&2, &2, Error.T, Unit>) -> IO(Unit):
  match result:
    case Done{unit}: IO.print("PASS raw family scope, universality, callback cancellation, nested budgets and inventories")
    case Fail{error}: IO.die(Unit, 1, Error.to_string(error))

def main() -> IO(Unit):
  report(suite())
