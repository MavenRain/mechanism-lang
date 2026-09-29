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
import ../kernel/term.bend as T
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def positive() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Exports.roundtrip(Data.template_reuse_positive(), "reuse round trip changed")
    +program : Elab.Checked <- R.checked(G.initial(), Data.template_reuse_positive())
    original : Elab.Checked <- R.checked(G.initial(), Data.template_reuse_base())
    +globals : G.T = R.globals(program)
    existing : P.Family <- Test.family("Existing", R.globals(original))
    checked : Unit <- R.require(Raw.matched_family(existing, G.find_family("Existing", globals)), "reused family was replaced")
    checked : Unit <- R.require(R.same_names(Test.new_family_names(globals, G.initial()), ["Existing", "Partial_Right"]), "wrong reuse family inventory")
    checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), ["Existing_unbox", "Shared_Left_unbox", "Shared_Right_unbox", "Partial_Left_unbox", "Partial_Right_unbox", "N_Inner_Left_unbox", "N_Inner_Right_unbox", "payload", "value", "partial", "nested"]) && Test.no_axioms(R.rows(program)), "wrong definition inventory")
    checked : Unit <- R.audit_entries(Test.entries(globals), G.initial())
    R.computations([C.Pair{"value", R.natural(37)}, C.Pair{"partial", R.natural(37)}, C.Pair{"nested", R.natural(37)}], globals)

def dependent() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    original : Elab.Checked <- R.checked(G.empty(), Data.template_reuse_dependent_base())
    program : Elab.Checked <- R.checked(G.empty(), Data.template_reuse_dependent_base() ++ "specialize Marker (0) as Shared with (Marker := Old, Wrapper := Old_Wrapper) def witness : Old_Wrapper mark := Shared_value")
    R.require(Test.same_families(R.globals(program), R.globals(original)), "dependent reuse changed family identities")

def partial_dependent() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    original : Elab.Checked <- R.checked(G.empty(), Data.template_reuse_dependent_base())
    +program : Elab.Checked <- R.checked(G.empty(), Data.template_reuse_dependent_base() ++ "specialize Marker (0) as Sh with (Marker := Old)")
    +globals : G.T = R.globals(program)
    +before : G.T = R.globals(original)
    old : P.Family <- Test.family("Old", before)
    checked : Unit <- R.require(Raw.matched_family(old, G.find_family("Old", globals)), "partial reuse replaced the reused family")
    checked : Unit <- R.require(R.same_names(Test.new_family_names(globals, before), ["Sh_Wrapper"]), "wrong partial reuse family inventory")
    checked : Unit <- R.require(Test.no_axioms(R.rows(program)), "partial reuse introduced axioms")
    value : T.T <- R.normalize(globals, "Sh_value")
    R.require(R.contains(R.entry_names(R.rows(program)), "Sh_value"), "the fresh companion member is missing")

def level_reuse(+source: String, argument: String, message: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    original : Elab.Checked <- R.checked(G.empty(), source)
    program : Elab.Checked <- R.checked(G.empty(), source ++ "specialize R (" ++ argument ++ ") as Y with (R := E)")
    original : P.Family <- Test.family("E", R.globals(original))
    R.require(Raw.matched_family(original, G.find_family("E", R.globals(program))), message)

def instance(globals: G.T, catalog: Family.T) -> B.Comp(G.T):
  Family.instantiate_comp(globals, catalog, "Marker", [L.zero()], "Alias", [C.Pair{"Marker", "Existing"}], None{})

def weakened(+globals: G.T, catalog: Family.T, original: P.Family, mode: Nat) -> Result<&2, &2, String, Unit>:
  Raw.exact(G.T, Test.mismatch("Existing"), B.run(G.T, instance(G.add_family("Existing", Test.altered(original, mode), globals), catalog), B.unlimited()))

def raw() -> Result<&2, &2, String, Nat>:
  do Result<&2, &2, String, Nat>:
    +catalog : Family.T <- R.kernel(Family.T, Family.declare(G.empty(), [], B.unlimited(), 1n, Check.FamilyDecl{"Marker", [], [], L.one()}, [Check.CtorDecl{"marker", [], [], []}], []))
    +globals : G.T <- R.kernel(G.T, Family.instantiate(G.empty(), catalog, B.unlimited(), "Marker", [L.zero()], "Existing", [], None{}))
    +reused : G.T <- R.kernel(G.T, B.run(G.T, instance(globals, catalog), B.unlimited()))
    +original : P.Family <- Test.family("Existing", globals)
    Test.run([
      Raw.Case{"identity reuse keeps globals", unit => R.require(Raw.same_globals(globals, reused), "root family reuse changed globals")},
      Raw.Case{"an exhausted budget refuses the reuse", unit => Raw.exact(G.T, Raw.budget_error(), B.run_poll(G.T, instance(globals, catalog), F.exhausted()))},
      Raw.Case{"a weakened certificate refuses the reuse", unit => weakened(globals, catalog, original, 0n)},
      Raw.Case{"a weakened certificate refuses the reuse", unit => weakened(globals, catalog, original, 1n)},
      Raw.Case{"a weakened certificate refuses the reuse", unit => weakened(globals, catalog, original, 2n)},
      Raw.Case{"a failed reuse keeps the original family", unit => R.require(Raw.matched_family(original, G.find_family("Existing", globals)), "failed reuse changed original family")}], 0n)

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    checked : Unit <- R.lift(Unit, positive())
    checked : Unit <- R.lift(Unit, dependent())
    checked : Unit <- R.lift(Unit, partial_dependent())
    checked : Unit <- R.lift(Unit, level_reuse(Data.template_reuse_level_base(), "(max 0 0)", "level reuse changed the reused family"))
    checked : Unit <- R.lift(Unit, level_reuse(Data.template_reuse_param_level_base(), "(max 0 0)", "parameterized level reuse changed the reused family"))
    checked : Unit <- R.lift(Unit, level_reuse(Data.template_reuse_decl_level_base(), "0", "declared level reuse changed the reused family"))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.template_reuse_negatives(), G.initial()))
    checked : Unit <- R.lift(Unit, Prenex.parsers(Data.template_reuse_parsers(), True{}))
    count : Nat <- R.lift(Nat, raw())
    source : String <- R.read_all(root, ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech", "prelude/cat/composable-functors.mech", "prelude/cat/identity-functor.mech", "test/fixtures/prelude/reuse-contracts.mech"])
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.initial(), source))
    checked : Unit <- R.assert(Test.no_axioms(R.rows(program)), "category reuse introduced axioms")
    checked : Unit <- R.assert(R.same_names(Test.new_family_names(R.globals(program), G.initial()), ["Middle", "Source", "Target"]), "category specializations allocated fresh families")
    R.lift(String, Done{"negatives=17 parser=6 raw=" ++ Nat.show(count)})

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(R.root(args))
    R.finish("TEMPLATE-REUSE", result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
