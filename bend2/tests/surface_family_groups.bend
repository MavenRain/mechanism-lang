import Base
import ./relational_common.bend as R
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
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
import ../kernel/rules.bend as Rules
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def marker(name: String) -> Check.FamilyDecl:
  Check.FamilyDecl{name, [], [], L.one()}

def ctor() -> Check.CtorDecl:
  Check.CtorDecl{"mark", [], [], []}

def ty(name: String) -> T.T:
  T.Lan{S.SMu{name, []}, Rules.diagram_of([])}

def value(name: String, args: List<&2, T.T>) -> T.T:
  T.In{S.SMu{name, []}, T.ACtor{"mark"}, args}

def definition(name: String, ty: T.T, body: T.T) -> Check.Decl:
  Check.Decl{name, Check.Definition{}, ty, Some{body}}

def families() -> List<&2, Family.RawFamily>:
  [C.Pair{marker("First"), [ctor()]}, C.Pair{marker("Second"),
    [Check.CtorDecl{"mark", [P.Binder{Q.QMany{}, "first", ty("First")}], [], []}]}]

def witness() -> Check.Decl:
  definition("witness", ty("Second"), value("Second", [value("First", [])]))

def alias() -> Check.Decl:
  definition("alias", ty("Second"), T.Global{"witness"})

def declare(globals: G.T, catalog: Family.T, members: List<&2, Check.Decl>, group: List<&2, Family.RawFamily>) -> Result<&2, &2, E.T, Family.T>:
  Family.declare_group(globals, catalog, B.unlimited(), 1n, group, members)

def standard() -> Result<&2, &2, E.T, Family.T>:
  declare(G.empty(), [], [witness(), alias()], families())

def instantiate(globals: G.T, catalog: Family.T, name: String) -> Result<&2, &2, E.T, G.T>:
  Family.instantiate(globals, catalog, B.unlimited(), "First", [L.one()], name, [], None{})

def positive() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, standard())
    one : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "One"))
    two : G.T <- R.kernel(G.T, instantiate(one, catalog, "Two"))
    client : Elab.Checked <- R.checked(two, "def client : One := case One_alias as self in One_Second return One with\n| mark (first : One) => first\n")
    +globals : G.T = R.globals(client)
    actual : T.T <- R.normalize(globals, "client")
    checked : Unit <- R.require(T.equal(actual, value("One", [])), "cross-family constructor reference was not specialized")
    checked : Unit <- R.require(Raw.absent(P.Family, G.find_family("First", globals)) && Raw.absent(P.Family, G.find_family("Second", globals)) && Raw.absent(G.Entry, G.find("witness", globals)), "symbolic names escaped")
    Raw.refusal(Elab.Checked, E.Mismatch{"the term has type (Lan SMu Two_Second"}, Elab.check_in(globals, B.unlimited(), "def mixed : One_Second := Two_alias"))

def complete(family: Maybe<&2, P.Family>) -> Bool:
  match family:
    case Some{P.Family{name, params, indices, level, P.Complete{group}, ctors, positive}}: positive
    case other: False{}

def guard(ok: Bool, message: String) -> B.Comp(Unit):
  B.lift(Unit, Bool.pick(Result<&2, &2, E.T, Unit>, ok, Done{Unit{}}, Fail{E.Mismatch{message}}))

def first_callback(+globals: G.T) -> B.Comp(Check.Decl):
  do B.Comp<Check.Decl>:
    checked : Unit <- guard(complete(G.find_family("First", globals)) && complete(G.find_family("Second", globals)), "callback saw an incomplete family")
    checked : Unit <- guard(Raw.absent(G.Entry, G.find("witness", globals)) && Raw.absent(G.Entry, G.find("alias", globals)), "callback saw a future member")
    return witness()

def predecessor(entry: Maybe<&2, G.Entry>) -> Result<&2, &2, E.T, Unit>:
  match entry:
    case None{}: Fail{E.Unbound{"callback predecessor"}}
    case Some{G.Def{G.DefEntry{ty, body, reducible, rec_arg, partial}}}:
      Bool.pick(Result<&2, &2, E.T, Unit>, T.equal(body, value("Second", [value("First", [])])), Done{Unit{}}, Fail{E.Mismatch{"callback predecessor changed"}})
    case other: Fail{E.Mismatch{"unchecked predecessor"}}

def second_callback(globals: G.T) -> B.Comp(Check.Decl):
  do B.Comp<Check.Decl>:
    checked : Unit <- B.lift(Unit, predecessor(G.find("witness", globals)))
    return alias()

def elaborate(catalog: Family.T, group: List<&2, Family.RawFamily>, members: List<&1, Family.MemberElaborator>) -> B.Comp(Family.T):
  Family.declare_group_elaborated_comp(G.empty(), catalog, 1n, group, members)

def catalog_members(value: Maybe<&2, List<&2, String>>) -> Bool:
  match value:
    case Some{names}: R.strings(names, ["witness", "alias"])
    case None{}: False{}

def callback_order() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +callbacks : Family.T <- R.kernel(Family.T, B.run(Family.T, elaborate([], families(), [globals => first_callback(globals), second_callback]), B.unlimited()))
    raw : Family.T <- R.kernel(Family.T, standard())
    actual : G.T <- R.kernel(G.T, instantiate(G.empty(), callbacks, "One"))
    expected : G.T <- R.kernel(G.T, instantiate(G.empty(), raw, "One"))
    R.require(Raw.same_globals(actual, expected) && catalog_members(Family.members(callbacks, "First")), "callback declaration changed the installed program")

def sentinel(globals: G.T) -> B.Comp(Check.Decl):
  B.failed(Check.Decl, E.Mismatch{"a callback ran after an invalid predecessor"})

def callback_failure(expected: E.T, member: Check.Decl) -> Result<&2, &2, String, Unit>:
  Raw.exact(Family.T, expected, B.run(Family.T, elaborate([], families(), [globals => B.pure(Check.Decl, member), sentinel]), B.unlimited()))

def single(name: String, catalog: Family.T) -> Result<&2, &2, E.T, Family.T>:
  Family.declare(G.empty(), catalog, B.unlimited(), 0n, marker(name), [ctor()], [])

def with_entry(name: String) -> G.T:
  G.add(name, G.Axiom{T.Univ{L.one()}}, G.empty())

def companion_level(family: Maybe<&2, P.Family>) -> Result<&2, &2, String, Unit>:
  match family:
    case None{}: Fail{"specialized companion missing"}
    case Some{P.Family{name, params, indices, level, status, ctors, positive}}:
      R.require(L.equal(level, L.succ(L.one())), "companion universe was not specialized")

def companion_specialization() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], [],
      [C.Pair{marker("First"), [ctor()]}, C.Pair{Check.FamilyDecl{"Second", [], [], L.succ(L.var(0n))},
      [Check.CtorDecl{"mark", [P.Binder{Q.QMany{}, "A", T.Univ{L.var(0n)}}], [], []}]}]))
    +globals : G.T <- R.kernel(G.T, instantiate(G.empty(), catalog, "Levels"))
    checked : Unit <- companion_level(G.find_family("Levels_Second", globals))
    checked : Elab.Checked <- R.checked(globals, "def levelWitness : Levels_Second := mark Levels")
    Done{Unit{}}

def target_collision(+name: String, companion: Bool) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, standard())
    +globals : G.T = with_entry(name)
    checked : Unit <- Raw.refusal(G.T, Raw.collision(name), instantiate(globals, catalog, "One"))
    +fresh : G.T <- R.kernel(G.T, instantiate(globals, catalog, "Two"))
    R.require(Bool.pick(Bool, companion, Raw.present(P.Family, G.find_family("Two_Second", fresh)), Raw.present(G.Entry, G.find("Two_alias", fresh))) && Raw.present(G.Entry, G.find("Two_witness", fresh)), "the refused instance left the caller globals unusable")

def closed_rechecking() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +globals : G.T = with_entry("Seed")
    +catalog : Family.T <- R.kernel(Family.T, declare(globals, [], [], [C.Pair{marker("First"), [ctor()]}, C.Pair{marker("Second"), [Check.CtorDecl{"mark", [P.Binder{Q.QMany{}, "seed", T.Global{"Seed"}}], [], []}]}]))
    installed : G.T <- R.kernel(G.T, instantiate(globals, catalog, "Compatible"))
    Raw.refusal(G.T, E.Unbound{"Seed"}, instantiate(G.empty(), catalog, "Missing"))

def hidden_member() -> Check.Decl:
  +domain : T.T = T.Univ{L.one()}
  +shape : S.T<T.T> = S.SPi{Q.QMany{}, "A", domain}
  identity : T.T = T.Ann{T.Sec{shape, [T.Leg{[C.Pair{Q.QMany{}, "A"}], T.Var{0n}}]}, T.Ran{shape, domain}}
  definition("hidden", domain, T.Out{S.SPi{Q.QMany{}, "ignored", T.Univ{L.var(1n)}}, T.APt{Q.QMany{}, T.Univ{L.zero()}}, identity})

def callback_error() -> Result<&2, &2, String, Unit>:
  Raw.exact(Family.T, E.Cannot_infer{"callback failed"}, B.run(Family.T, elaborate([], families(),
    [globals => B.pure(Check.Decl, witness()), globals => B.failed(Check.Decl, E.Cannot_infer{"callback failed"}), sentinel]), B.unlimited()))

def reverse_group() -> List<&2, Family.RawFamily>:
  List.reverse(&2, Family.RawFamily, families())

def callback_invalid() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Raw.exact(Family.T, E.Mismatch{"a family schema group must be nonempty"}, B.run(Family.T, elaborate([], [], [sentinel]), B.unlimited()))
    Raw.exact(Family.T, E.Unbound{"the family First is not declared"}, B.run(Family.T, elaborate([], reverse_group(), [sentinel]), B.unlimited()))

def callback_entry() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    count : Nat <- Raw.measure(Family.T, B.run_state(Family.T, elaborate([], families(), []), B.of_polls(100000n)))
    Raw.exact(Family.T, Raw.budget_error(), B.run_poll(Family.T, elaborate([], families(), [sentinel, globals => B.pure(Check.Decl, alias())]), F.countdown(count)))

def callback_cancel() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    count : Nat <- Raw.measure(Family.T, B.run_state(Family.T, elaborate([], families(), []), B.of_polls(100000n)))
    Raw.exact(Family.T, Raw.budget_error(), B.run_poll(Family.T, elaborate([], families(), [globals => B.pure(Check.Decl, witness()), sentinel]), F.countdown(1n+count)))

def member_precedence() -> Result<&2, &2, String, Unit>:
  +members : List<&2, Check.Decl> = [definition("badBody", ty("First"), T.Var{0n}), definition("badLevel", T.Univ{L.var(1n)}, T.Univ{L.zero()})]
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Raw.refusal(Family.T, E.Unbound{"de Bruijn index 0"}, declare(G.empty(), [], members, families()))
    Raw.refusal(Family.T, Raw.scope_error(), Family.declare(G.empty(), [], B.unlimited(), 1n, marker("First"), [ctor()], members))

def cases() -> List<&1, Raw.Case>:
  [Raw.Case{"ordered-families-members-and-renaming", unit => positive()},
   Raw.Case{"empty", unit => Raw.refusal(Family.T, E.Mismatch{"a family schema group must be nonempty"}, declare(G.empty(), [], [], []))},
   Raw.Case{"duplicate-family", unit => Raw.refusal(Family.T, Raw.collision("First"), declare(G.empty(), [], [], [C.Pair{marker("First"), [ctor()]}, C.Pair{marker("First"), [ctor()]}]))},
   Raw.Case{"forward-reference", unit => Raw.refusal(Family.T, E.Unbound{"the family First is not declared"}, declare(G.empty(), [], [witness(), alias()], reverse_group()))},
   Raw.Case{"duplicate-constructor", unit => Raw.refusal(Family.T, E.Mismatch{"the constructor mark is already declared in Second"}, declare(G.empty(), [], [], [C.Pair{marker("First"), [ctor()]}, C.Pair{marker("Second"), [ctor(), ctor()]}]))},
   Raw.Case{"member-family-collision", unit => Raw.refusal(Family.T, Raw.collision("Second"), declare(G.empty(), [], [definition("Second", ty("Second"), value("Second", [value("First", [])]))], families()))},
   Raw.Case{"member-template-reference", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, single("Other", []))
     Raw.refusal(Family.T, E.Not_yet{"references between family schemas are not supported"}, declare(G.empty(), catalog, [definition("witness", ty("Other"), value("Second", [value("First", [])]))], families()))},
   Raw.Case{"member-body", unit => Raw.refusal(Family.T, E.Mismatch{"the term has type (Lan SMu First"}, declare(G.empty(), [], [definition("witness", ty("Second"), T.Ann{value("First", []), ty("First")})], families()))},
   Raw.Case{"member-forward-reference", unit => Raw.refusal(Family.T, E.Unbound{"witness"}, declare(G.empty(), [], [alias(), witness()], families()))},
   Raw.Case{"postulate", unit => Raw.refusal(Family.T, E.Not_yet{"family schema members must be definitions"}, declare(G.empty(), [], [Check.Decl{"witness", Check.Postulate{}, ty("Second"), None{}}], families()))},
   Raw.Case{"companion-level-scope", unit => Raw.refusal(Family.T, Raw.scope_error(), declare(G.empty(), [], [], [C.Pair{marker("First"), [ctor()]}, C.Pair{Check.FamilyDecl{"Second", [], [], L.succ(L.var(1n))}, [ctor()]}]))},
   Raw.Case{"companion-level-specialization", unit => companion_specialization()},
   Raw.Case{"companion-global-collision", unit => Raw.refusal(Family.T, Raw.collision("Second"), declare(with_entry("Second"), [], [witness(), alias()], families()))},
   Raw.Case{"companion-template-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, single("Second", []))
     Raw.refusal(Family.T, Raw.collision("Second"), declare(G.empty(), catalog, [witness(), alias()], families()))},
   Raw.Case{"target-companion-collision-atomic", unit => target_collision("One_Second", True{})},
   Raw.Case{"target-member-collision-atomic", unit => target_collision("One_alias", False{})},
   Raw.Case{"target-companion-template-collision", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, standard())
     catalog : Family.T <- R.kernel(Family.T, single("One_Second", catalog))
     Raw.refusal(G.T, Raw.collision("One_Second"), instantiate(G.empty(), catalog, "One"))},
   Raw.Case{"closed-rechecking", unit => closed_rechecking()},
   Raw.Case{"arity", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, standard())
     Raw.refusal(G.T, E.Universe{"the family schema First expects 1 universe arguments, got 0"}, Family.instantiate(G.empty(), catalog, B.unlimited(), "First", [], "One", [], None{}))},
   Raw.Case{"closed-levels", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, standard())
     Raw.refusal(G.T, E.Universe{"universe arguments must be closed"}, Family.instantiate(G.empty(), catalog, B.unlimited(), "First", [L.var(0n)], "One", [], None{}))},
   Raw.Case{"declaration-budget", unit => Raw.refusal(Family.T, Raw.budget_error(), B.run_poll(Family.T, elaborate([], families(), Family.raw_callbacks([witness(), alias()])), F.exhausted()))},
   Raw.Case{"specialization-budget", unit => do Result<&2, &2, String, Unit>:
     catalog : Family.T <- R.kernel(Family.T, standard())
     Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, Family.instantiate_comp(G.empty(), catalog, "First", [L.one()], "One", [], None{}), F.exhausted()))},
   Raw.Case{"callback-order-and-parity", unit => callback_order()},
   Raw.Case{"callback-body-checked", unit => callback_failure(E.Unbound{"de Bruijn index 0 is outside the context"}, definition("witness", ty("Second"), T.Var{0n}))},
   Raw.Case{"callback-postulate-refused", unit => callback_failure(E.Not_yet{"family schema members must be definitions"}, Check.Decl{"witness", Check.Postulate{}, ty("Second"), None{}})},
   Raw.Case{"callback-hidden-level-scope", unit => callback_failure(Raw.scope_error(), hidden_member())},
   Raw.Case{"callback-error-stops", unit => callback_error()},
   Raw.Case{"callback-invalid-family-stops", unit => callback_invalid()},
   Raw.Case{"callback-budget-stops", unit => callback_cancel()},
   Raw.Case{"callback-budget-stops-at-entry", unit => callback_entry()},
   Raw.Case{"member-refusal-precedence", unit => member_precedence()}]

def arguments(args: List<String>) -> IO(Unit):
  R.finish("FAMILY-GROUPS", Raw.run(cases(), 0n))

def main() -> IO(Unit):
  R.finish("FAMILY-GROUPS", Raw.run(cases(), 0n))
