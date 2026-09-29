import Base
import ./relational_common.bend as R
import ./surface_template_common.bend as Test
import ./surface_template_data.bend as Data
import ./surface_raw_common.bend as Raw
import ./surface_prenex_common.bend as Prenex
import ./surface_prenex_exports.bend as Exports
import ./surface_foundation_common.bend as F
import ./surface_family_groups.bend as Groups
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/level.bend as L
import ../kernel/quantity.bend as Q
import ../kernel/common.bend as C
import ../kernel/term.bend as T
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def universe(family: P.Family, expected: L.T, name: String) -> Result<&2, &2, String, Unit>:
  match family:
    case P.Family{n, params, indices, level, status, ctors, positive}: R.require(L.equal(level, expected), "wrong universe: " ++ name)

def universes(rows: List<&2, C.Pair<String, L.T>>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{+name, expected}, rest}:
      do Result<&2, &2, String, Unit>:
        family : P.Family <- Test.family(name, globals)
        checked : Unit <- universe(family, expected, name)
        universes(rest, globals)

def positive(+source: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Exports.roundtrip(source, "composition parse/print changed")
    symbolic : Elab.Checked <- R.checked(G.empty(), source)
    checked : Unit <- R.require(R.empty_symbolic(symbolic), "composition leaked symbolic globals")
    +program : Elab.Checked <- R.checked(G.empty(), source ++ Data.template_composition_client())
    +globals : G.T = R.globals(program)
    checked : Unit <- R.require(R.same_names(Prenex.family_names(globals), ["I_Item", "M_Item", "N_Inner_Left", "N_Inner_Right", "P_Left", "P_Right", "Tiny"]), "composition family inventory changed")
    checked : Unit <- R.require(R.strings(R.entry_names(R.rows(program)), ["P_Left_unbox", "P_Right_unbox", "P_transfer", "N_Inner_Left_unbox", "N_Inner_Right_unbox", "N_Inner_transfer", "N_project", "M_Item_unbox", "I_Item_unbox", "low", "high", "result", "nested"]) && Nat.is_eq(R.length(C.Pair<String, G.Entry>, Test.entries(globals)), 13n), "composition definition order changed")
    checked : Unit <- R.require(Test.no_axioms(R.rows(program)), "composition introduced axioms")
    checked : Unit <- R.audit_entries(Test.entries(globals), G.empty())
    checked : Unit <- universes([C.Pair{"P_Left", L.one()}, C.Pair{"P_Right", L.succ(L.one())}, C.Pair{"N_Inner_Right", L.succ(L.one())}, C.Pair{"M_Item", L.succ(L.succ(L.one()))}, C.Pair{"I_Item", L.one()}], globals)
    R.computations([C.Pair{"result", R.tiny_type()}, C.Pair{"nested", R.tiny_type()}], globals)

def dependencies() -> List<&2, Family.Dependency>:
  [Family.Dependency{"Seed", [L.zero()], "Local", [], None{}}]

def compose(globals: G.T, catalog: Family.T, dependencies: List<&2, Family.Dependency>, members: List<&1, Family.MemberElaborator>) -> B.Comp(Family.T):
  Family.compose_comp(globals, catalog, 1n, "Composed", dependencies, members)

def composed(catalog: Family.T, members: List<&1, Family.MemberElaborator>) -> Result<&2, &2, E.T, Family.T>:
  B.run(Family.T, compose(G.empty(), catalog, dependencies(), members), B.unlimited())

def forbidden(globals: G.T) -> B.Comp(Check.Decl):
  B.failed(Check.Decl, E.Unbound{"later"})

def cancelled_dependencies(catalog: Family.T) -> Result<&2, &2, String, Unit>:
  Raw.exact(Family.T, Raw.budget_error(), B.run_poll(Family.T, compose(G.empty(), catalog, [Family.Dependency{"Absent", [L.zero()], "Local", [], None{}}], [forbidden]), F.countdown(1n)))

def cancelled_members(+catalog: Family.T) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    count : Nat <- Raw.measure(Family.T, B.run_state(Family.T, compose(G.empty(), catalog, dependencies(), []), B.of_polls(100000n)))
    Raw.exact(Family.T, Raw.budget_error(), B.run_poll(Family.T, compose(G.empty(), catalog, dependencies(), [globals => B.pure(Check.Decl, Groups.definition("first", T.Univ{L.one()}, T.Univ{L.zero()})), forbidden]), F.countdown(1n+count)))

def cases(+catalog: Family.T) -> List<&1, Raw.Case>:
  [Raw.Case{"empty-dependencies", unit => Raw.exact(Family.T, E.Mismatch{"a family schema group must be nonempty"}, B.run(Family.T, compose(G.empty(), catalog, [], []), B.unlimited()))},
   Raw.Case{"raw-scope", unit => Raw.exact(Family.T, Raw.scope_error(), B.run(Family.T, compose(G.empty(), catalog, [Family.Dependency{"Seed", [L.var(1n)], "Local", [], None{}}], []), B.unlimited()))},
   Raw.Case{"raw-member-body", unit => Raw.exact(Family.T, E.Unbound{"de Bruijn index 0 is outside the context"}, composed(catalog, [globals => B.pure(Check.Decl, Groups.definition("bad", T.Univ{L.one()}, T.Var{0n})), forbidden]))},
   Raw.Case{"raw-group-name", unit => Raw.exact(Family.T, Raw.collision("Composed"), composed(catalog, [globals => B.pure(Check.Decl, Groups.definition("Composed", T.Univ{L.one()}, T.Univ{L.zero()}))]))},
   Raw.Case{"raw-postulate", unit => Raw.exact(Family.T, E.Not_yet{"family schema members must be definitions"}, composed(catalog, [globals => B.pure(Check.Decl, Check.Decl{"postulated", Check.Postulate{}, T.Univ{L.one()}, None{}})]))},
   Raw.Case{"raw-hidden-scope", unit => Raw.exact(Family.T, Raw.scope_error(), composed(catalog, [globals => B.pure(Check.Decl, Groups.hidden_member())]))},
   Raw.Case{"callback-error", unit => Raw.exact(Family.T, E.Unbound{"callback"}, composed(catalog, [globals => B.failed(Check.Decl, E.Unbound{"callback"}), forbidden]))},
   Raw.Case{"cancelled-dependencies", unit => cancelled_dependencies(catalog)},
   Raw.Case{"cancelled-members", unit => cancelled_members(catalog)}]

def raw() -> Result<&2, &2, String, Nat>:
  do Result<&2, &2, String, Nat>:
    catalog : Family.T <- R.kernel(Family.T, Family.declare(G.empty(), [], B.unlimited(), 1n, Check.FamilyDecl{"Seed", [], [], L.one()}, [Check.CtorDecl{"mark", [], [], []}], []))
    Test.run(cases(catalog), 0n)

def current_globals() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +globals : G.T = G.add("External", G.Axiom{T.Univ{L.one()}}, G.empty())
    +catalog : Family.T <- R.kernel(Family.T, Family.declare(globals, [], B.unlimited(), 1n, Check.FamilyDecl{"Seed", [], [], L.one()}, [Check.CtorDecl{"mark", [P.Binder{Q.QMany{}, "item", T.Global{"External"}}], [], []}], []))
    checked : Unit <- Raw.exact(Family.T, E.Unbound{"External"}, composed(catalog, []))
    +catalog : Family.T <- R.kernel(Family.T, B.run(Family.T, compose(globals, catalog, dependencies(), []), B.unlimited()))
    checked : Unit <- Raw.exact(G.T, E.Unbound{"External"}, Family.instantiate(G.empty(), catalog, B.unlimited(), "Composed", [L.zero()], "Bad", [], None{}))
    restored : G.T <- R.kernel(G.T, Family.instantiate(globals, catalog, B.unlimited(), "Composed", [L.zero()], "Good", [], None{}))
    R.require(Raw.present(P.Family, G.find_family("Good_Local", restored)), "failed specialization prevented a compatible instance")

def direct_parity() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +actual : Elab.Checked <- R.checked(G.empty(), Data.template_composition_box() ++ "poly (u) group Group where " ++ Data.template_composition_dependency() ++ "def member : Local := box end specialize Group (0) as G")
    +expected : Elab.Checked <- R.checked(G.empty(), Data.template_composition_box() ++ "specialize Box (0) as G_Local def G_member : G_Local := box")
    R.require(Raw.same_globals(R.globals(actual), R.globals(expected)) && R.strings(R.entry_names(R.rows(actual)), R.entry_names(R.rows(expected))) && Raw.entries(R.rows(actual), R.globals(expected)), "composition differs from explicit closed declarations")

def prefix(members: List<&2, Syntax.RecDef<Syntax.T>>, +reversed: List<&2, Syntax.RecDef<Syntax.T>>) -> Result<&2, &2, String, List<&2, Syntax.RecDef<Syntax.T>>>:
  match members:
    case Nil{}: Fail{"category prefix has no assoc member"}
    case Con{Syntax.RecDef{+name, +ty, +body}, rest}:
      next = Bool.pick(Unit -> Result<&2, &2, String, List<&2, Syntax.RecDef<Syntax.T>>>, String.eq(name, "assoc"), unit => Done{List.reverse(&2, Syntax.RecDef<Syntax.T>, Syntax.RecDef{name, ty, body} <> reversed)}, unit => prefix(rest, Syntax.RecDef{name, ty, body} <> reversed))
      next(Unit{})

def category_prefix(parsed: List<&2, Syntax.Decl>) -> Result<&2, &2, String, List<&2, Syntax.Decl>>:
  match parsed:
    case Con{Syntax.DPolyGroup{arity, family, companions, members}, Nil{}}:
      do Result<&2, &2, String, List<&2, Syntax.Decl>>:
        members : List<&2, Syntax.RecDef<Syntax.T>> <- prefix(members, [])
        Done{[Syntax.DPolyGroup{arity, family, companions, members}]}
    case other: Fail{"category prelude is not one family group"}

def category(+root: String) -> R.Task(Unit):
  do R.Task<Unit>:
    source : String <- R.read(root, "prelude/cat/category.mech")
    +fixture : String <- R.read(root, "test/fixtures/prelude/composition-category.mech")
    parsed : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.kernel(List<&2, Syntax.Decl>, Parser.program(source)))
    parsed : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, category_prefix(parsed))
    checked : Unit <- R.lift(Unit, Exports.roundtrip(fixture, "category composition round trip changed"))
    client : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.kernel(List<&2, Syntax.Decl>, Parser.program(fixture)))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.kernel(Elab.Checked, Elab.elab_program_in(G.empty(), B.unlimited(), List.append(&2, Syntax.Decl, parsed, client))))
    checked : Unit <- R.assert(R.same_names(Prenex.family_names(R.globals(program)), ["First_Source", "First_Target", "Second_Source", "Second_Target"]), "mixed-universe category family inventory changed")
    R.assert(Nat.is_eq(R.length(C.Pair<String, G.Entry>, R.rows(program)), 40n) && Nat.is_eq(R.length(C.Pair<String, G.Entry>, Test.entries(R.globals(program))), 40n) && Test.no_axioms(R.rows(program)), "mixed-universe category entry inventory changed")

def suite(+root: String) -> R.Task(String):
  do R.Task<String>:
    source : String <- R.read(root, "test/fixtures/prelude/template-composition.mech")
    checked : Unit <- R.lift(Unit, positive(source))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.template_composition_negatives(), G.empty()))
    checked : Unit <- R.lift(Unit, Prenex.parsers(Data.template_composition_parsers(), False{}))
    checked : Unit <- R.lift(Unit, direct_parity())
    checked : Unit <- R.lift(Unit, current_globals())
    checked : Unit <- category(root)
    count : Nat <- R.lift(Nat, raw())
    R.lift(String, Done{"negatives=24 parser=9 raw=" ++ Nat.show(count) ++ " type-excluded=negative-arity"})

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(R.root(args))
    R.finish("TEMPLATE-COMPOSITION", result)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
