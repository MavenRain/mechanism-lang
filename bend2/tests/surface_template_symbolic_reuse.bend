import Base
import ./relational_common.bend as R
import ./surface_template_common.bend as Test
import ./surface_template_data.bend as Data
import ./surface_raw_common.bend as Raw
import ./surface_prenex_common.bend as Prenex
import ./surface_prenex_exports.bend as Exports
import ./surface_foundation_common.bend as F
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/level.bend as L
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def expected_members(+prefix: String) -> List<&2, String>:
  [prefix ++ "_Inner_Base_unbox", prefix ++ "_Inner_Both_Left_unbox", prefix ++ "_Inner_Both_Right_unbox", prefix ++ "_Inner_Again_unbox", prefix ++ "_Other_unbox"]

def expected_rows() -> List<&2, String>:
  List.append(&2, String, expected_members("N"), List.append(&2, String, expected_members("Higher"), List.append(&2, String, expected_members("Reused"), ["payload", "value", "again", "reused", "right"])))

def positive() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Exports.roundtrip(Data.template_symbolic_reuse_templates(), "symbolic reuse round trip changed")
    symbolic : Elab.Checked <- R.checked(G.empty(), Data.template_symbolic_reuse_templates())
    checked : Unit <- R.require(R.empty_symbolic(symbolic), "symbolic reuse leaked globals")
    +instances : String = Data.template_symbolic_reuse_templates() ++ "specialize Nested (0, 0) as N specialize Nested (1, 2) as Higher "
    original : Elab.Checked <- R.checked(G.initial(), instances)
    +program : Elab.Checked <- R.checked(G.initial(), instances ++ "specialize Nested (0, 0) as Reused with (Inner_Base := N_Inner_Base, Inner_Both_Right := N_Inner_Both_Right) def payload : N_Inner_Base Nat := box 37 def value : Nat := N_Inner_Both_Left_unbox Nat payload def again : Nat := N_Inner_Again_unbox Nat payload def reused : Nat := Reused_Inner_Again_unbox Nat payload def right : Nat := N_Other_unbox Nat (box 41 : N_Inner_Both_Right Nat)")
    +globals : G.T = R.globals(program)
    checked : Unit <- R.require(R.same_names(Test.new_family_names(globals, G.initial()), ["Higher_Inner_Base", "Higher_Inner_Both_Right", "N_Inner_Base", "N_Inner_Both_Right"]) && Test.same_families(globals, R.globals(original)), "wrong symbolic reuse family inventory")
    checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), expected_rows()) && Test.no_axioms(R.rows(program)), "wrong symbolic reuse member inventory")
    R.computations([C.Pair{"value", R.natural(37)}, C.Pair{"again", R.natural(37)}, C.Pair{"reused", R.natural(37)}, C.Pair{"right", R.natural(41)}], globals)

def dependent() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +program : Elab.Checked <- R.checked(G.empty(), Data.template_symbolic_reuse_marker() ++ "poly (u) group Dependent where specialize Marker (u) as Original specialize Marker (u) as Partial with (Marker := Original) specialize Marker (u) as Shared with (Marker := Original, Wrapper := Original_Wrapper) def witness : Original_Wrapper mark := Shared_value end specialize Dependent (1) as D")
    R.require(R.same_names(Prenex.family_names(R.globals(program)), ["D_Original", "D_Original_Wrapper", "D_Partial_Wrapper"]) && R.strings(R.entry_names(R.rows(program)), ["D_Original_value", "D_Partial_value", "D_Shared_value", "D_witness"]) && Test.no_axioms(R.rows(program)), "dependent symbolic reuse changed identities")

def compose(globals: G.T, catalog: Family.T, members: List<&1, Family.MemberElaborator>) -> B.Comp(Family.T):
  Family.compose_comp(globals, catalog, 1n, "Shared", [Family.Dependency{"Seed", [L.zero()], "Fresh", [], None{}}, Family.Dependency{"Seed", [L.zero()], "Copy", [C.Pair{"Seed", "Existing"}], None{}}], members)

def forbidden(globals: G.T) -> B.Comp(Check.Decl):
  B.failed(Check.Decl, E.Unbound{"callback"})

def certificate_failure(result: Result<&2, &2, E.T, Family.T>) -> Bool:
  match result:
    case Fail{E.Mismatch{message}}: String.eq(message, "the reused family Existing does not match the template")
    case other: False{}

@unsafe def frontier(+globals: G.T, +catalog: Family.T, +allowance: Nat) -> Result<&2, &2, String, Nat>:
  result = B.run_poll(Family.T, compose(globals, catalog, [g => forbidden(g)]), F.countdown(allowance))
  next = Bool.pick(Unit -> Result<&2, &2, String, Nat>, certificate_failure(result), unit => Done{allowance},
    Bool.pick(Unit -> Result<&2, &2, String, Nat>, Nat.is_lt(allowance, 512n), unit => frontier(globals, catalog, 1n+allowance), unit => Fail{"no certificate refusal below 512 polls"}))
  next(Unit{})

def cancelled(+globals: G.T, +catalog: Family.T, original: P.Family) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +corrupted : G.T = G.add_family("Existing", Test.altered(original, 1n), globals)
    +poll : Nat <- frontier(corrupted, catalog, 1n)
    checked : Unit <- Raw.exact(Family.T, Raw.budget_error(), B.run_poll(Family.T, compose(corrupted, catalog, [g => forbidden(g)]), F.countdown((poll - 1n : Nat))))
    R.require(Nat.is_lt(3n, poll), "cancellation preempted no certificate poll or ran a member")

def empty_companions(value: Maybe<&2, List<&2, String>>) -> Bool:
  match value:
    case Some{Nil{}}: True{}
    case other: False{}

def preserve(+globals: G.T, catalog: Family.T, original: P.Family) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +shared : Family.T <- R.kernel(Family.T, B.run(Family.T, compose(globals, catalog, []), B.unlimited()))
    installed : G.T <- R.kernel(G.T, Family.instantiate(globals, shared, B.unlimited(), "Shared", [L.zero()], "Instance", [], None{}))
    R.require(Raw.matched_family(original, G.find_family("Existing", installed)) && empty_companions(Family.companions(shared, "Shared")), "caller certificate changed")

def arity_one(value: Maybe<&2, Nat>) -> Bool:
  match value:
    case Some{Succ{Zero{}}}: True{}
    case other: False{}

def before_members(+globals: G.T, +catalog: Family.T, original: P.Family) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Raw.exact(Family.T, Test.mismatch("Existing"), B.run(Family.T, compose(G.add_family("Existing", Test.altered(original, 0n), globals), catalog, [g => forbidden(g)]), B.unlimited()))
    shared : Family.T <- R.kernel(Family.T, B.run(Family.T, compose(globals, catalog, []), B.unlimited()))
    R.require(arity_one(Family.arity(shared, "Shared")), "failed reuse ran a member or hid the composed schema")

def weakened(globals: G.T, catalog: Family.T, original: P.Family, mode: Nat) -> Result<&2, &2, String, Unit>:
  Raw.exact(Family.T, Test.mismatch("Existing"), B.run(Family.T, compose(G.add_family("Existing", Test.altered(original, mode), globals), catalog, []), B.unlimited()))

def raw() -> Result<&2, &2, String, Nat>:
  do Result<&2, &2, String, Nat>:
    +catalog : Family.T <- R.kernel(Family.T, Family.declare(G.empty(), [], B.unlimited(), 1n, Check.FamilyDecl{"Seed", [], [], L.one()}, [Check.CtorDecl{"seed", [], [], []}], []))
    +globals : G.T <- R.kernel(G.T, Family.instantiate(G.empty(), catalog, B.unlimited(), "Seed", [L.zero()], "Existing", [], None{}))
    +original : P.Family <- Test.family("Existing", globals)
    Test.run([
      Raw.Case{"preserve-caller", unit => preserve(globals, catalog, original)},
      Raw.Case{"cancelled", unit => cancelled(globals, catalog, original)},
      Raw.Case{"refuse-before-members", unit => before_members(globals, catalog, original)},
      Raw.Case{"provisional", unit => weakened(globals, catalog, original, 1n)},
      Raw.Case{"constructors", unit => weakened(globals, catalog, original, 2n)}], 0n)

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    checked : Unit <- R.lift(Unit, positive())
    checked : Unit <- R.lift(Unit, dependent())
    checked : Unit <- R.lift(Unit, Test.negatives(Data.template_symbolic_reuse_negatives(), G.initial()))
    checked : Unit <- R.lift(Unit, Prenex.parsers(Data.template_symbolic_reuse_parsers(), True{}))
    count : Nat <- R.lift(Nat, raw())
    source : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech", "prelude/cat/composable-functors.mech", "prelude/cat/identity-functor.mech", "prelude/cat/shared-functor-chain.mech", "test/fixtures/prelude/symbolic-reuse-contracts.mech"])
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.initial(), source))
    checked : Unit <- R.assert(R.same_names(Test.new_family_names(R.globals(program), G.initial()), ["Chain_Middle", "Chain_Source", "Chain_Target"]) && Test.no_axioms(R.rows(program)), "shared category inventory or axioms changed")
    R.lift(String, Done{"negatives=12 parser=6 raw=" ++ Nat.show(count)})

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(R.root(args))
    R.finish("TEMPLATE-SYMBOLIC-REUSE", result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
