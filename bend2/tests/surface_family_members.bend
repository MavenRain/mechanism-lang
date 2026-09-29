import Base
import ./relational_common.bend as R
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
import ./surface_family_groups.bend as Groups
import ../prelude/dependent.bend as D
import ../surface/family_poly.bend as Family
import ../surface/elab.bend as Elab
import ../kernel/check.bend as Check
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as L
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def witness() -> Check.Decl:
  Groups.definition("witness", Groups.ty("Marker"), Groups.value("Marker", []))

def alias() -> Check.Decl:
  Groups.definition("alias", Groups.ty("Marker"), T.Global{"witness"})

def identity(+level: L.T) -> Check.Decl:
  Groups.definition("identity", D.arrow(P.Binder{Q.QZero{}, "A", T.Univ{level}}, D.arrow(P.Binder{Q.QMany{}, "value", T.Var{0n}}, T.Var{1n})),
    D.lambda(P.Binder{Q.QZero{}, "A", T.Univ{level}}, D.lambda(P.Binder{Q.QMany{}, "value", T.Var{0n}}, T.Var{0n})))

def declaration(globals: G.T, catalog: Family.T, members: List<&2, Check.Decl>) -> B.Comp(Family.T):
  Family.declare_comp(globals, catalog, 1n, Groups.marker("Marker"), [Groups.ctor()], members)

def declare(globals: G.T, catalog: Family.T, members: List<&2, Check.Decl>) -> Result<&2, &2, E.T, Family.T>:
  B.run(Family.T, declaration(globals, catalog, members), B.unlimited())

def instantiate(globals: G.T, catalog: Family.T, name: String, exports: Maybe<&2, Family.Bindings>) -> Result<&2, &2, E.T, G.T>:
  Family.instantiate(globals, catalog, B.unlimited(), "Marker", [L.one()], name, [], exports)

def instance(globals: G.T, catalog: Family.T, name: String, exports: Maybe<&2, Family.Bindings>) -> B.Comp(G.T):
  Family.instantiate_comp(globals, catalog, "Marker", [L.one()], name, [], exports)

def inventory(value: Maybe<&2, List<&2, String>>) -> Bool:
  match value:
    case Some{names}: R.strings(names, ["witness", "alias", "identity"])
    case None{}: False{}

def alias_entry(entry: Maybe<&2, G.Entry>) -> Result<&2, &2, String, Unit>:
  match entry:
    case None{}: Fail{"alias missing"}
    case Some{G.Def{G.DefEntry{ty, body, reducible, rec_arg, partial}}}:
      R.require(T.equal(body, T.Global{"First_witness"}) && T.equal(ty, Groups.ty("First")), "member reference was not renamed")
    case other: Fail{"member is not a definition"}

def positive() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias(), identity(L.var(0n))]))
    checked : Unit <- R.require(inventory(Family.members(catalog, "Marker")) && Raw.absent(List<&2, String>, Family.members(catalog, "missing")), "member inventory differs")
    first : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "First", None{}))
    +globals : G.T <- R.kernel(G.T, instantiate(first, catalog, "Second", None{}))
    checked : Unit <- R.require(Raw.absent(G.Entry, G.find("witness", globals)) && Raw.absent(P.Family, G.find_family("Marker", globals)), "symbolic names escaped into ordinary globals")
    checked : Elab.Checked <- R.checked(globals, "def firstClient : First := First_identity First First_alias\ndef secondClient : Second := Second_identity Second Second_alias\n")
    checked : Unit <- alias_entry(G.find("First_alias", globals))
    Raw.refusal(Elab.Checked, E.Mismatch{"the term has type"}, Elab.check_in(globals, B.unlimited(), "def mixed : First := Second_alias\n"))

def chosen() -> Family.Bindings:
  [C.Pair{"witness", "chosenWitness"}, C.Pair{"alias", "chosenAlias"}]

def planned(value: Maybe<&2, C.Pair<List<&2, String>, List<&2, String>>>, families: List<&2, String>, members: List<&2, String>) -> Bool:
  match value:
    case Some{C.Pair{actual_families, actual_members}}: R.strings(actual_families, families) && R.strings(actual_members, members)
    case None{}: False{}

def name_plan(catalog: Family.T, reuse: Family.Bindings, exports: Family.Bindings) -> Maybe<&2, C.Pair<List<&2, String>, List<&2, String>>>:
  Family.instance_names(catalog, "Marker", "Chosen", reuse, Some{exports})

def no_plan(value: Maybe<&2, C.Pair<List<&2, String>, List<&2, String>>>) -> Bool:
  Raw.absent(C.Pair<List<&2, String>, List<&2, String>>, value)

def bad_exports(expected: E.T, +exports: Family.Bindings) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    checked : Unit <- R.require(no_plan(name_plan(catalog, [], exports)), "invalid exports produced a name plan")
    Raw.refusal(G.T, expected, instantiate(G.empty(), catalog, "Chosen", Some{exports}))

def chosen_names() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    +exports : Family.Bindings = List.reverse(&2, C.Pair<String, String>, chosen())
    checked : Unit <- R.require(planned(name_plan(catalog, [], exports), ["Chosen"], ["chosenWitness", "chosenAlias"]), "export name plan differs")
    +globals : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "Chosen", Some{exports}))
    checked : Elab.Checked <- R.checked(globals, "mu ExportWitness : (0 x : Chosen) -> Type 0 with\n| exportWitness : ExportWitness chosenWitness\ndef exportedComputes : ExportWitness chosenAlias := exportWitness\n")
    R.require(Raw.absent(G.Entry, G.find("Chosen_witness", globals)) && Raw.absent(G.Entry, G.find("Chosen_alias", globals)), "default names escaped an explicit export mapping")

def export_companion() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, Family.declare_group(G.empty(), [], B.unlimited(), 1n,
      [C.Pair{Groups.marker("Marker"), [Groups.ctor()]}, C.Pair{Groups.marker("Companion"), [Groups.ctor()]}], [witness(), alias()]))
    +exports : Family.Bindings = [C.Pair{"witness", "Chosen_Companion"}, C.Pair{"alias", "chosenAlias"}]
    checked : Unit <- R.require(no_plan(name_plan(catalog, [], exports)), "companion collision produced a name plan")
    Raw.refusal(G.T, Raw.collision("Chosen_Companion"), instantiate(G.empty(), catalog, "Chosen", Some{exports}))

def export_ambient_constructor() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    ambient : Elab.Checked <- R.checked(G.empty(), "mu Flag : Type 0 with\n| other : Flag\n")
    +globals : G.T = R.globals(ambient)
    installed : G.T <- R.kernel(G.T, instantiate(globals, catalog, "Chosen", Some{chosen()}))
    Raw.refusal(G.T, Raw.collision("other"), instantiate(globals, catalog, "Chosen", Some{[C.Pair{"witness", "other"}, C.Pair{"alias", "chosenAlias"}]}))

def reuse_as_name() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    globals : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "First", None{}))
    +reuse : Family.Bindings = [C.Pair{"Marker", "First"}]
    +exports : Family.Bindings = [C.Pair{"witness", "Chosen"}, C.Pair{"alias", "chosenAlias"}]
    checked : Unit <- R.require(no_plan(name_plan(catalog, reuse, exports)), "as_name collision under reuse produced a name plan")
    Raw.refusal(G.T, Raw.collision("Chosen"), Family.instantiate(globals, catalog, B.unlimited(), "Marker", [L.one()], "Chosen", reuse, Some{exports}))

def reuse_exports() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    +globals : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "First", None{}))
    +reuse : Family.Bindings = [C.Pair{"Marker", "First"}]
    checked : Unit <- R.require(planned(name_plan(catalog, reuse, chosen()), [], ["chosenWitness", "chosenAlias"]), "reused family leaked into exported name plan")
    installed : G.T <- R.kernel(G.T, Family.instantiate(globals, catalog, B.unlimited(), "Marker", [L.one()], "Chosen", reuse, Some{chosen()}))
    checked : Elab.Checked <- R.checked(installed, "def reused : First := chosenAlias")
    Raw.refusal(G.T, Raw.collision("First"), Family.instantiate(globals, catalog, B.unlimited(), "Marker", [L.one()], "Chosen", reuse, Some{[C.Pair{"witness", "First"}, C.Pair{"alias", "otherAlias"}]}))

def export_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    +plain : Nat <- Raw.measure(G.T, B.run_state(G.T, instance(G.empty(), catalog, "Plain", None{}), B.of_polls(100000n)))
    +exported : Nat <- Raw.measure(G.T, B.run_state(G.T, instance(G.empty(), catalog, "Chosen", Some{chosen()}), B.of_polls(100000n)))
    checked : Unit <- R.require(Nat.is_eq(exported, (plain + 2n : Nat)), "export polls " ++ Nat.show(exported) ++ ", expected " ++ Nat.show((plain + 2n : Nat)))
    fitted : G.T <- R.kernel(G.T, B.run_poll(G.T, instance(G.empty(), catalog, "Fitted", None{}), F.countdown(plain)))
    checked : Unit <- Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, instance(G.empty(), catalog, "Chosen", Some{chosen()}), F.countdown(plain)))
    Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, instance(G.empty(), catalog, "Chosen", Some{[C.Pair{"witness", "same"}, C.Pair{"alias", "same"}]}), F.countdown(1n)))

def export_rollback() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
    +prepared : G.T = Groups.with_entry("seed")
    count : Nat <- Raw.measure(G.T, B.run_state(G.T, instance(prepared, catalog, "Chosen", Some{chosen()}), B.of_polls(100000n)))
    checked : Unit <- Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, instance(prepared, catalog, "Chosen", Some{chosen()}), F.countdown((count - 1n : Nat))))
    checked : Unit <- R.require(Raw.absent(G.Entry, G.find("chosenWitness", prepared)) && Raw.absent(G.Entry, G.find("chosenAlias", prepared)) && Raw.absent(P.Family, G.find_family("Chosen", prepared)), "a refused run changed the input environment")
    +installed : G.T <- R.kernel(G.T, instantiate(prepared, catalog, "Chosen", Some{chosen()}))
    R.require(Raw.present(G.Entry, G.find("chosenWitness", installed)) && Raw.present(G.Entry, G.find("chosenAlias", installed)) && Raw.present(P.Family, G.find_family("Chosen", installed)), "the repeated run installed no exported member")

def hidden_scope(level: L.T) -> Check.Decl:
  +domain : T.T = T.Univ{L.one()}
  identity : T.T = T.Ann{D.lambda(P.Binder{Q.QMany{}, "A", domain}, T.Var{0n}), D.arrow(P.Binder{Q.QMany{}, "A", domain}, domain)}
  Groups.definition("hidden", domain, T.Out{S.SPi{Q.QMany{}, "ignored", T.Univ{level}}, T.APt{Q.QMany{}, T.Univ{L.zero()}}, identity})

def hidden_body(decl: Check.Decl, entry: Maybe<&2, G.Entry>) -> Result<&2, &2, String, Unit>:
  match decl entry:
    case Check.Decl{name, kind, ty, Some{expected}} Some{G.Def{G.DefEntry{actual_ty, body, reducible, rec_arg, partial}}}:
      R.require(T.equal(body, expected), "a hidden universe argument was not specialized")
    case _ None{}: Fail{"hidden member missing"}
    case _ _: Fail{"hidden member is not a definition"}

def hidden_specialization() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [hidden_scope(L.var(0n))]))
    globals : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "Hidden", None{}))
    hidden_body(hidden_scope(L.one()), G.find("Hidden_hidden", globals))

def closed_rechecking() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +globals : G.T = Groups.with_entry("seed")
    +catalog : Family.T <- R.kernel(Family.T, declare(globals, [], [Groups.definition("seedUse", T.Univ{L.one()}, T.Global{"seed"})]))
    installed : G.T <- R.kernel(G.T, instantiate(globals, catalog, "Compatible", None{}))
    Raw.refusal(G.T, E.Mismatch{"the term has type"}, instantiate(G.add("seed", G.Axiom{T.Univ{L.succ(L.one())}}, G.empty()), catalog, "Changed", None{}))

def declaration_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    base : Nat <- Raw.measure(Family.T, B.run_state(Family.T, declaration(G.empty(), [], []), B.of_polls(100000n)))
    +actual : Nat <- Raw.measure(Family.T, B.run_state(Family.T, declaration(G.empty(), [], [witness()]), B.of_polls(100000n)))
    +allowance : Nat = (base + 16n : Nat)
    checked : Unit <- R.require(Nat.is_eq(actual, allowance), "member declaration polls " ++ Nat.show(actual) ++ ", expected " ++ Nat.show(allowance))
    Raw.refusal(Family.T, Raw.budget_error(), B.run_poll(Family.T, declaration(G.empty(), [], [witness()]), F.countdown((allowance - 1n : Nat))))

def specialization_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    empty : Family.T <- R.kernel(Family.T, declare(G.empty(), [], []))
    base : Nat <- Raw.measure(G.T, B.run_state(G.T, instance(G.empty(), empty, "Empty", None{}), B.of_polls(100000n)))
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness()]))
    +actual : Nat <- Raw.measure(G.T, B.run_state(G.T, instance(G.empty(), catalog, "Limited", None{}), B.of_polls(100000n)))
    +allowance : Nat = (base + 13n : Nat)
    checked : Unit <- R.require(Nat.is_eq(actual, allowance), "member specialization polls " ++ Nat.show(actual) ++ ", expected " ++ Nat.show(allowance))
    Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, instance(G.empty(), catalog, "Limited", None{}), F.countdown((allowance - 1n : Nat))))

def cases() -> List<&1, Raw.Case>:
  [Raw.Case{"ordered-members-and-renaming", unit => positive()},
   Raw.Case{"chosen-export-names-and-computation", unit => chosen_names()},
   Raw.Case{"exports-empty-mapping", unit => bad_exports(E.Mismatch{"every family member needs an export name"}, [])},
   Raw.Case{"exports-missing-member", unit => bad_exports(E.Mismatch{"every family member needs an export name"}, [C.Pair{"witness", "chosenWitness"}])},
   Raw.Case{"exports-unknown-member", unit => bad_exports(E.Mismatch{"member exports must name each member once"}, [C.Pair{"witness", "chosenWitness"}, C.Pair{"alias", "chosenAlias"}, C.Pair{"unknown", "extra"}])},
   Raw.Case{"exports-duplicate-source", unit => bad_exports(E.Mismatch{"member exports must name each member once"}, [C.Pair{"witness", "first"}, C.Pair{"witness", "second"}])},
   Raw.Case{"exports-duplicate-target", unit => bad_exports(E.Mismatch{"member exports repeat the target same"}, [C.Pair{"witness", "same"}, C.Pair{"alias", "same"}])},
   Raw.Case{"exports-family-collision", unit => bad_exports(Raw.collision("Chosen"), [C.Pair{"witness", "Chosen"}, C.Pair{"alias", "chosenAlias"}])},
   Raw.Case{"exports-ambient-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
     Raw.refusal(G.T, Raw.collision("chosenAlias"), instantiate(Groups.with_entry("chosenAlias"), catalog, "Chosen", Some{chosen()}))},
   Raw.Case{"exports-template-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
     Raw.refusal(G.T, Raw.collision("Marker"), instantiate(G.empty(), catalog, "Chosen", Some{[C.Pair{"witness", "Marker"}, C.Pair{"alias", "chosenAlias"}]}))},
   Raw.Case{"exports-companion-collision", unit => export_companion()},
   Raw.Case{"exports-constructor-collision", unit => bad_exports(Raw.collision("mark"), [C.Pair{"witness", "mark"}, C.Pair{"alias", "chosenAlias"}])},
   Raw.Case{"exports-ambient-constructor-collision", unit => export_ambient_constructor()},
   Raw.Case{"exports-as-name-under-reuse", unit => reuse_as_name()},
   Raw.Case{"exports-with-family-reuse", unit => reuse_exports()},
   Raw.Case{"export-validation-budget", unit => export_budget()},
   Raw.Case{"exports-late-budget-rollback", unit => export_rollback()},
   Raw.Case{"definition-must-check", unit => Raw.refusal(Family.T, E.Unbound{"de Bruijn index 0 is outside the context"}, declare(G.empty(), [], [Groups.definition("witness", Groups.ty("Marker"), T.Var{0n})]))},
   Raw.Case{"postulate-refused", unit => Raw.refusal(Family.T, E.Not_yet{"family schema members must be definitions"}, declare(G.empty(), [], [Check.Decl{"witness", Check.Postulate{}, Groups.ty("Marker"), None{}}]))},
   Raw.Case{"body-required", unit => Raw.refusal(Family.T, E.Cannot_infer{"the definition witness has no body"}, declare(G.empty(), [], [Check.Decl{"witness", Check.Definition{}, Groups.ty("Marker"), None{}}]))},
   Raw.Case{"forward-reference", unit => Raw.refusal(Family.T, E.Unbound{"witness"}, declare(G.empty(), [], [alias(), witness()]))},
   Raw.Case{"self-reference", unit => Raw.refusal(Family.T, E.Unbound{"witness"}, declare(G.empty(), [], [Groups.definition("witness", Groups.ty("Marker"), T.Global{"witness"})]))},
   Raw.Case{"duplicate-member", unit => Raw.refusal(Family.T, Raw.collision("witness"), declare(G.empty(), [], [witness(), witness()]))},
   Raw.Case{"family-name-collision", unit => Raw.refusal(Family.T, Raw.collision("Marker"), declare(G.empty(), [], [Groups.definition("Marker", Groups.ty("Marker"), Groups.value("Marker", []))]))},
   Raw.Case{"existing-global-collision", unit => Raw.refusal(Family.T, Raw.collision("witness"), declare(Groups.with_entry("witness"), [], [witness()]))},
   Raw.Case{"hidden-level-scope", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [hidden_scope(L.var(0n))]))
     Raw.refusal(Family.T, Raw.scope_error(), declare(G.empty(), [], [hidden_scope(L.var(1n))]))},
   Raw.Case{"hidden-level-specialization", unit => hidden_specialization()},
   Raw.Case{"member-template-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, Groups.single("Other", []))
     Raw.refusal(Family.T, Raw.collision("Other"), declare(G.empty(), catalog, [Groups.definition("Other", Groups.ty("Marker"), Groups.value("Marker", []))]))},
   Raw.Case{"cross-template-reference", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, Groups.single("Other", []))
     Raw.refusal(Family.T, E.Not_yet{"references between family schemas are not supported"}, declare(G.empty(), catalog, [Groups.definition("other", Groups.ty("Other"), Groups.value("Other", []))]))},
   Raw.Case{"target-global-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness(), alias()]))
     +globals : G.T = Groups.with_entry("First_alias")
     checked : Unit <- Raw.refusal(G.T, Raw.collision("First_alias"), instantiate(globals, catalog, "First", None{}))
     R.require(Raw.absent(P.Family, G.find_family("First", globals)) && Raw.absent(G.Entry, G.find("First_witness", globals)), "failed specialization modified input globals")},
   Raw.Case{"target-family-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness()]))
     globals : G.T <- R.kernel(G.T, Check.declare_family_at(G.empty(), B.unlimited(), 0n, Groups.marker("First_witness")))
     Raw.refusal(G.T, Raw.collision("First_witness"), instantiate(globals, catalog, "First", None{}))},
   Raw.Case{"target-template-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [witness()]))
     catalog : Family.T <- R.kernel(Family.T, Groups.single("First_witness", catalog))
     Raw.refusal(G.T, Raw.collision("First_witness"), instantiate(G.empty(), catalog, "First", None{}))},
   Raw.Case{"closed-rechecking", unit => closed_rechecking()},
   Raw.Case{"declaration-member-budget", unit => declaration_budget()},
   Raw.Case{"specialization-member-budget", unit => specialization_budget()}]

def selected(cases: List<&1, Raw.Case>, +name: String) -> Result<&2, &2, String, String>:
  match cases:
    case Nil{}: Fail{"unknown case: " ++ name}
    case Con{Raw.Case{+key, test}, rest}:
      choose = Bool.pick(Unit -> Result<&2, &2, String, String>, String.eq(key, name), unit => Raw.run([Raw.Case{key, test}], 0n), unit => selected(rest, name))
      choose(Unit{})

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}: R.finish("FAMILY-MEMBERS", Raw.run(cases(), 0n))
    case Con{name, rest}: R.finish("FAMILY-MEMBERS", selected(cases(), name))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
