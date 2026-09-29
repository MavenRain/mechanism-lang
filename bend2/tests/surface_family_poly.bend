import Base
import ./relational_common.bend as R
import ./surface_template_common.bend as Test
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
import ./surface_family_groups.bend as Groups
import ./surface_universes.bend as Universes
import ./surface_boundary.bend as Boundary
import ./surface_prenex_common.bend as Prenex
import ../prelude/dependent.bend as D
import ../surface/family_poly.bend as Family
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/value.bend as V
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as L
import ../kernel/bignum.bend as Big
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E
import ../cli/output.bend as Out

def empty(name: String, level: L.T) -> Check.FamilyDecl:
  Check.FamilyDecl{name, [], [], level}

def eq(carrier: L.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"TemplateEq", [P.Binder{Q.QZero{}, "A", T.Univ{carrier}}, P.Binder{Q.QZero{}, "x", T.Var{0n}}], [P.Binder{Q.QZero{}, "y", T.Var{1n}}], L.zero()}

def refl() -> Check.CtorDecl:
  Check.CtorDecl{"templateRefl", [], [T.Var{1n}, T.Var{0n}], [T.Var{0n}]}

def declare(globals: G.T, catalog: Family.T, arity: Nat, family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>) -> Result<&2, &2, E.T, Family.T>:
  Family.declare(globals, catalog, B.unlimited(), arity, family, ctors, [])

def standard() -> Result<&2, &2, String, Family.T>:
  R.kernel(Family.T, declare(G.empty(), [], 1n, eq(L.var(0n)), [refl()]))

def instance(globals: G.T, catalog: Family.T, name: String, levels: List<&2, L.T>, target: String) -> Result<&2, &2, E.T, G.T>:
  Family.instantiate(globals, catalog, B.unlimited(), name, levels, target, [], None{})

def certificate(family: P.Family) -> Result<&2, &2, String, P.Family>:
  match family:
    case P.Family{name, params, indices, level, P.Complete{names}, ctors, +positive}:
      do Result<&2, &2, String, P.Family>:
        checked : Unit <- R.require(positive, "family has no positivity verdict")
        Done{P.Family{name, params, indices, level, P.Complete{names}, ctors, positive}}
    case other: Fail{"instance is not a completed family"}

def family(globals: G.T, name: String) -> Result<&2, &2, String, P.Family>:
  do Result<&2, &2, String, P.Family>:
    family : P.Family <- Test.family(name, globals)
    certificate(family)

def check_refl(globals: G.T, +name: String, carrier: L.T) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), D.definition("reflClient", [P.Binder{Q.QZero{}, "A", T.Univ{carrier}}, P.Binder{Q.QZero{}, "x", T.Var{0n}}], Universes.member(name, [T.Var{1n}, T.Var{0n}], [T.Var{0n}]), T.In{S.SMu{name, [T.Var{0n}]}, T.ACtor{"templateRefl"}, []})))
    Done{Unit{}}

def arity_one(value: Maybe<&2, Nat>) -> Bool:
  match value:
    case Some{Succ{Zero{}}}: True{}
    case other: False{}

def catalog_only() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- standard()
    checked : Unit <- R.require(arity_one(Family.arity(catalog, "TemplateEq")), "template arity disappeared")
    checked : Unit <- R.require(Raw.absent(Nat, Family.arity(catalog, "missing")), "unknown template has an arity")
    checked : Unit <- R.require(Raw.absent(P.Family, G.find_family("TemplateEq", G.empty())), "template entered the family table")
    Raw.refusal(V.T, E.Unbound{"the family TemplateEq is not declared"}, Check.infer_term(G.empty(), B.unlimited(), Universes.member("TemplateEq", [], [])))

def equalities() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- standard()
    +high : L.T <- R.kernel(L.T, Universes.high_value(Big.of_decimal("4611686018427387903")))
    globals : G.T <- R.kernel(G.T, instance(G.empty(), catalog, "TemplateEq", [L.zero()], "EqProp"))
    globals : G.T <- R.kernel(G.T, instance(globals, catalog, "TemplateEq", [L.one()], "EqData"))
    +globals : G.T <- R.kernel(G.T, instance(globals, catalog, "TemplateEq", [high], "EqHigh"))
    checked : Unit <- check_refl(globals, "EqProp", L.zero())
    checked : Unit <- check_refl(globals, "EqData", L.one())
    checked : Unit <- check_refl(globals, "EqHigh", high)
    +fam : P.Family <- family(globals, "EqHigh")
    checked : Unit <- R.require(L.equal(Family.kernel_level(fam), L.zero()), "equality no longer lives in Prop")
    checked : Unit <- R.require(Engine.mu_zero_eliminable(fam), "equality lost its singleton elimination criterion")
    R.require(Raw.absent(P.Family, G.find_family("TemplateEq", globals)), "specialization installed the original template")

def sums() -> Result<&2, &2, String, Unit>:
  +left = L.succ(L.var(0n))
  +right = L.succ(L.var(1n))
  decl : Check.FamilyDecl = Check.FamilyDecl{"TemplateSum", [P.Binder{Q.QZero{}, "A", T.Univ{left}}, P.Binder{Q.QZero{}, "B", T.Univ{right}}], [], L.max(left, right)}
  ctors : List<&2, Check.CtorDecl> = [Check.CtorDecl{"templateInl", [P.Binder{Q.QMany{}, "left", T.Var{1n}}], [T.Var{2n}, T.Var{1n}], []}, Check.CtorDecl{"templateInr", [P.Binder{Q.QMany{}, "right", T.Var{0n}}], [T.Var{2n}, T.Var{1n}], []}]
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], 2n, decl, ctors))
    globals : G.T <- R.kernel(G.T, instance(G.empty(), catalog, "TemplateSum", [L.zero(), L.of_u32(2)], "SumLowHigh"))
    +globals : G.T <- R.kernel(G.T, instance(globals, catalog, "TemplateSum", [L.of_u32(2), L.zero()], "SumHighLow"))
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), Universes.sum_client("SumLowHigh", L.zero(), L.of_u32(2), "templateInl", T.Var{1n})))
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), Universes.sum_client("SumLowHigh", L.zero(), L.of_u32(2), "templateInr", T.Var{0n})))
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), Universes.sum_client("SumHighLow", L.of_u32(2), L.zero(), "templateInl", T.Var{1n})))
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), Universes.sum_client("SumHighLow", L.of_u32(2), L.zero(), "templateInr", T.Var{0n})))
    fam : P.Family <- family(globals, "SumLowHigh")
    R.require(L.equal(Family.kernel_level(fam), L.of_u32(3)), "sum result universe did not specialize max")

def hidden(+name: String, motive_name: String, +carrier: L.T, +visible: L.T, argument: T.T) -> T.T:
  hidden : T.T = T.Elim{T.ElimData{S.SMu{name, []}, T.Var{0n}, Q.QZero{}, Some{T.Motive{Some{motive_name}, [], "self", T.Univ{carrier}}}, [C.Pair{T.ACtor{"templateNil"}, T.Leg{[], T.Univ{carrier}}}]}}
  identity : T.T = T.Ann{D.lambda(P.Binder{Q.QZero{}, "T", T.Univ{visible}}, T.Var{0n}), D.arrow(P.Binder{Q.QZero{}, "T", T.Univ{visible}}, T.Univ{visible})}
  T.Out{S.SPi{Q.QZero{}, "ignored", hidden}, T.APt{Q.QZero{}, argument}, identity}

def recursive_decl(+carrier: L.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"TemplateList", [P.Binder{Q.QZero{}, "A", T.Univ{L.succ(carrier)}}], [], L.succ(carrier)}

def recursive_ctors(head: T.T) -> List<&2, Check.CtorDecl>:
  [Check.CtorDecl{"templateNil", [], [T.Var{0n}], []}, Check.CtorDecl{"templateCons", [P.Binder{Q.QMany{}, "head", head}, P.Binder{Q.QMany{}, "tail", Universes.member("TemplateList", [T.Var{1n}], [])}], [T.Var{2n}], []}]

def recursive() -> Result<&2, &2, String, Family.T>:
  R.kernel(Family.T, declare(G.empty(), [], 1n, recursive_decl(L.var(0n)), recursive_ctors(hidden("TemplateList", "TemplateList", L.succ(L.var(0n)), L.succ(L.var(0n)), T.Var{0n}))))

def hidden_metadata(+expected: String, term: T.T) -> Result<&2, &2, String, Unit>:
  match term:
    case T.Out{S.SPi{q, n, T.Elim{T.ElimData{S.SMu{name, indices}, scrut, sq, Some{T.Motive{Some{motive_name}, idx, self, body}}, branches}}}, addr, head}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- R.require(String.eq(name, expected), "stored recursive shape was not renamed")
        R.require(String.eq(motive_name, expected), "stored recursive motive name was not renamed")
    case other: Fail{"stored metadata field lost its application, elimination, or motive"}

def recursive_metadata(+name: String, ctor: Maybe<&2, P.Ctor>) -> Result<&2, &2, String, Unit>:
  match ctor:
    case Some{P.Ctor{n, Con{P.Binder{hq, hn, head}, Con{P.Binder{tq, tn, +tail}, Nil{}}}, indices, arity, rec}}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- R.require(rec, "recursive constructor lost its recursion flag")
        checked : Unit <- hidden_metadata(name, head)
        checked : Unit <- R.require(T.exists_name(True{}, [name], tail), "recursive tail lost its specialized family")
        R.require(Bool.not(T.exists_name(True{}, ["TemplateList"], tail)), "recursive tail retained its template name")
    case None{}: Fail{"recursive constructor disappeared"}
    case other: Fail{"recursive constructor has the wrong field count"}

def recursive_instance(+globals: G.T, +name: String, +carrier: L.T) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    fam : P.Family <- family(globals, name)
    checked : Unit <- recursive_metadata(name, P.ctor_of("templateCons", fam))
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(globals, B.unlimited(), D.definition("listClient", [P.Binder{Q.QZero{}, "A", T.Univ{L.succ(carrier)}}, P.Binder{Q.QMany{}, "value", T.Var{0n}}], Universes.member(name, [T.Var{1n}], []), T.In{S.SMu{name, []}, T.ACtor{"templateCons"}, [T.Var{0n}, T.In{S.SMu{name, []}, T.ACtor{"templateNil"}, []}]})))
    ty : V.T <- R.kernel(V.T, Check.infer_term(globals, B.unlimited(), D.arrow(P.Binder{Q.QZero{}, "A", T.Univ{L.succ(carrier)}}, Universes.member(name, [T.Var{0n}], []))))
    Done{Unit{}}

def recursives() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- recursive()
    globals : G.T <- R.kernel(G.T, instance(G.empty(), catalog, "TemplateList", [L.zero()], "ListLow"))
    +globals : G.T <- R.kernel(G.T, instance(globals, catalog, "TemplateList", [L.of_u32(2)], "ListHigh"))
    checked : Unit <- recursive_instance(globals, "ListLow", L.zero())
    recursive_instance(globals, "ListHigh", L.of_u32(2))

def scoped(decl: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>) -> Result<&2, &2, String, Unit>:
  Raw.refusal(Family.T, Raw.scope_error(), declare(G.empty(), [], 1n, decl, ctors))

def sampled() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    declared : G.T <- R.kernel(G.T, Check.declare_family(G.empty(), B.unlimited(), Boundary.counterfeit(L.zero())))
    closed : G.T <- R.kernel(G.T, Check.define_ctors(declared, B.unlimited(), ["Counterfeit"], "Counterfeit", [Boundary.counterfeit_ctor()]))
    Raw.refusal(Family.T, E.Universe{"a field of counterfeit exceeds its family universe"}, declare(G.empty(), [], 1n, Boundary.counterfeit(L.var(0n)), [Boundary.counterfeit_ctor()]))

def named_carrier(level: L.T) -> Result<&2, &2, String, G.T>:
  do Result<&2, &2, String, G.T>:
    entry : G.Entry <- R.kernel(G.Entry, Check.check_decl(G.empty(), B.unlimited(), Check.Decl{"Carrier", Check.Postulate{}, T.Univ{level}, None{}}))
    Done{G.add("Carrier", entry, G.empty())}

def nonpositive() -> Result<&2, &2, String, Unit>:
  +carrier = L.succ(L.var(0n))
  decl : Check.FamilyDecl = Check.FamilyDecl{"NegativeRecursive", [P.Binder{Q.QZero{}, "A", T.Univ{carrier}}], [], carrier}
  ctor : Check.CtorDecl = Check.CtorDecl{"negative", [P.Binder{Q.QMany{}, "function", D.arrow(P.Binder{Q.QMany{}, "self", Universes.member("NegativeRecursive", [T.Var{0n}], [])}, T.Var{1n})}], [T.Var{1n}], []}
  Raw.refusal(Family.T, E.Not_yet{"a family that is not strictly positive arrives at M2"}, declare(G.empty(), [], 1n, decl, [ctor]))

def duplicate() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- standard()
    Raw.refusal(Family.T, Raw.collision("TemplateEq"), declare(G.empty(), catalog, 1n, eq(L.var(0n)), [refl()]))

def global_collision() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    globals : G.T <- named_carrier(L.one())
    Raw.refusal(Family.T, Raw.collision("Carrier"), declare(globals, [], 0n, empty("Carrier", L.one()), []))

def family_collision() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    globals : G.T <- R.kernel(G.T, Check.declare_family(G.empty(), B.unlimited(), empty("Existing", L.one())))
    Raw.refusal(Family.T, Raw.collision("Existing"), declare(globals, [], 0n, empty("Existing", L.one()), []))

def instance_arity() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- standard()
    checked : Unit <- Raw.refusal(G.T, E.Universe{"the family schema TemplateEq expects 1 universe arguments, got 0"}, instance(G.empty(), catalog, "TemplateEq", [], "TooFew"))
    Raw.refusal(G.T, E.Universe{"the family schema TemplateEq expects 1 universe arguments, got 2"}, instance(G.empty(), catalog, "TemplateEq", [L.zero(), L.one()], "TooMany"))

def free_instance() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- standard()
    Raw.refusal(G.T, E.Universe{"universe arguments must be closed"}, instance(G.empty(), catalog, "TemplateEq", [L.var(0n)], "Open"))

def other_template() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- standard()
    Raw.refusal(Family.T, E.Not_yet{"references between family schemas are not supported"}, declare(G.empty(), catalog, 1n, recursive_decl(L.var(0n)), recursive_ctors(hidden("TemplateList", "TemplateEq", L.succ(L.var(0n)), L.succ(L.var(0n)), T.Var{0n}))))

def target_collision(+target: String, globals: G.T) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- standard()
    Raw.refusal(G.T, Raw.collision(target), instance(globals, catalog, "TemplateEq", [L.zero()], target))

def global_target() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    globals : G.T <- named_carrier(L.one())
    target_collision("Carrier", globals)

def empty_instance_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], 0n, empty("EmptyBudget", L.one()), []))
    Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, Family.instantiate_comp(G.empty(), catalog, "EmptyBudget", [], "Budgeted", [], None{}), F.exhausted()))

def late_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    catalog : Family.T <- recursive()
    Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, Family.instantiate_comp(G.empty(), catalog, "TemplateList", [L.zero()], "Budgeted", [], None{}), F.countdown(4n)))

def wrap(depth: Nat, +carrier: L.T, term: T.T) -> T.T:
  match depth:
    case Zero{}: term
    case Succ{rest}: wrap(rest, carrier, T.Ann{term, T.Univ{L.succ(carrier)}})

def annotated(depth: Nat, +universe: L.T) -> Check.FamilyDecl:
  Check.FamilyDecl{"AnnotationBudget", [P.Binder{Q.QZero{}, "A", wrap(depth, L.succ(universe), T.Univ{L.succ(universe)})}], [], L.one()}

def closed_check(+decl: Check.FamilyDecl) -> B.Comp(G.T):
  do B.Comp<G.T>:
    globals : G.T <- Check.declare_family_comp(G.empty(), 0n, decl)
    Check.define_ctors_comp(globals, 0n, ["AnnotationBudget"], "AnnotationBudget", [])

type Measurement is Data:
  Measurement{catalog: Family.T, kernel_polls: Nat, mapping_polls: Nat}

def measure(+depth: Nat) -> Result<&2, &2, String, Measurement>:
  do Result<&2, &2, String, Measurement>:
    +catalog : Family.T <- R.kernel(Family.T, declare(G.empty(), [], 1n, annotated(depth, L.var(0n)), []))
    +direct : Nat <- Raw.measure(G.T, B.run_state(G.T, closed_check(annotated(depth, L.zero())), B.of_polls(100000n)))
    +complete : Nat <- Raw.measure(G.T, B.run_state(G.T, Family.instantiate_comp(G.empty(), catalog, "AnnotationBudget", [L.zero()], "Budgeted", [], None{}), B.of_polls(100000n)))
    checked : Unit <- R.require(Bool.not(Nat.is_lt(complete, direct)), "instance polling omitted part of closed kernel checking")
    Done{Measurement{catalog, direct, (complete - direct : Nat)}}

def measured_mapping(shallow: Measurement, deep: Measurement) -> Result<&2, &2, String, Unit>:
  match shallow deep:
    case Measurement{unused, shallow_kernel, +shallow_mapping} Measurement{catalog, deep_kernel, +deep_mapping}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- R.require(Nat.is_lt(shallow_mapping, deep_mapping), "substituting more raw Term nodes consumed no additional caller budget")
        Raw.refusal(G.T, Raw.budget_error(), B.run_poll(G.T, Family.instantiate_comp(G.empty(), catalog, "AnnotationBudget", [L.zero()], "Budgeted", [], None{}), F.countdown((deep_kernel + shallow_mapping : Nat))))

def substitution_budget() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    shallow : Measurement <- measure(0n)
    deep : Measurement <- measure(64n)
    measured_mapping(shallow, deep)

def rollback() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +catalog : Family.T <- standard()
    +original : G.T <- R.kernel(G.T, instance(G.empty(), catalog, "TemplateEq", [L.zero()], "First"))
    checked : Unit <- Raw.refusal(G.T, E.Universe{"universe arguments must be closed"}, instance(original, catalog, "TemplateEq", [L.var(0n)], "Retry"))
    checked : Unit <- R.require(Nat.is_eq(Prenex.family_count(original), 1n) && Raw.absent(P.Family, G.find_family("Retry", original)), "failed instance changed the input family table")
    +result : G.T <- R.kernel(G.T, instance(original, catalog, "TemplateEq", [L.one()], "Retry"))
    checked : Unit <- R.require(Nat.is_eq(Prenex.family_count(result), 2n), "successful retry installed an incorrect number of families")
    checked : Unit <- Raw.refusal(G.T, Raw.collision("Retry"), instance(result, catalog, "TemplateEq", [L.zero()], "Retry"))
    checked : Unit <- check_refl(result, "Retry", L.one())
    check_refl(original, "First", L.zero())

def current_types() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    +low : G.T <- named_carrier(L.one())
    +high : G.T <- named_carrier(L.succ(L.one()))
    +catalog : Family.T <- R.kernel(Family.T, declare(low, [], 0n, empty("UsesCarrier", L.one()), [Check.CtorDecl{"carry", [P.Binder{Q.QMany{}, "value", T.Global{"Carrier"}}], [], []}]))
    checked : Unit <- Raw.refusal(G.T, E.Universe{"a field of carry exceeds its family universe"}, instance(high, catalog, "UsesCarrier", [], "Retry"))
    checked : Unit <- R.require(Nat.is_eq(Prenex.family_count(high), 0n), "failed recheck leaked its provisional family")
    result : G.T <- R.kernel(G.T, instance(low, catalog, "UsesCarrier", [], "Retry"))
    fam : P.Family <- family(result, "Retry")
    R.require(Nat.is_eq(Prenex.family_count(low), 0n), "retry changed the original environment")

def missing_dependency() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    globals : G.T <- named_carrier(L.one())
    catalog : Family.T <- R.kernel(Family.T, declare(globals, [], 0n, Check.FamilyDecl{"UsesCarrier", [P.Binder{Q.QZero{}, "value", T.Global{"Carrier"}}], [], L.one()}, []))
    Raw.refusal(G.T, E.Unbound{"Carrier"}, instance(G.empty(), catalog, "UsesCarrier", [], "MissingDependency"))

def cases() -> List<&1, Raw.Case>:
  [Raw.Case{"catalog-keeps-templates-out-of-globals", unit => catalog_only()},
   Raw.Case{"equality-at-prop-and-high-carrier-universes", unit => equalities()},
   Raw.Case{"sum-at-unequal-universes-in-both-directions", unit => sums()},
   Raw.Case{"recursive-fields-and-motive-metadata-are-renamed", unit => recursives()},
   Raw.Case{"closed-sample-does-not-prove-universal-field-bound", unit => sampled()},
   Raw.Case{"mixed-prop-type-result-sort-is-refused", unit => Raw.refusal(Family.T, E.Universe{"a family schema must remain in Prop or Type for every universe substitution"}, declare(G.empty(), [], 1n, empty("Mixed", L.var(0n)), []))},
   Raw.Case{"free-header-level-is-refused", unit => scoped(empty("FreeHeader", L.succ(L.var(1n))), [])},
   Raw.Case{"free-parameter-level-is-refused", unit => scoped(eq(L.var(1n)), [refl()])},
   Raw.Case{"free-index-level-is-refused", unit => scoped(Check.FamilyDecl{"FreeIndex", [], [P.Binder{Q.QZero{}, "P", T.Univ{L.var(1n)}}], L.zero()}, [])},
   Raw.Case{"free-constructor-field-level-is-refused", unit => scoped(empty("FreeField", L.succ(L.one())), [Check.CtorDecl{"freeField", [P.Binder{Q.QZero{}, "P", T.Univ{L.var(1n)}}], [], []}])},
   Raw.Case{"free-level-in-result-parameter-annotation-is-refused", unit => scoped(eq(L.var(0n)), [Check.CtorDecl{"templateRefl", [], [T.Ann{T.Var{1n}, T.Univ{L.var(1n)}}, T.Var{0n}], [T.Var{0n}]}])},
   Raw.Case{"free-level-in-result-index-annotation-is-refused", unit => scoped(eq(L.var(0n)), [Check.CtorDecl{"templateRefl", [], [T.Var{1n}, T.Var{0n}], [T.Ann{T.Var{0n}, T.Univ{L.var(1n)}}]}])},
   Raw.Case{"hidden-free-level-in-ignored-shape-is-refused", unit => scoped(recursive_decl(L.var(0n)), recursive_ctors(hidden("TemplateList", "TemplateList", L.var(1n), L.succ(L.var(0n)), T.Var{0n})))},
   Raw.Case{"negative-recursive-position-is-refused", unit => nonpositive()},
   Raw.Case{"duplicate-template-name-is-refused", unit => duplicate()},
   Raw.Case{"existing-global-template-name-is-refused", unit => global_collision()},
   Raw.Case{"existing-family-template-name-is-refused", unit => family_collision()},
   Raw.Case{"duplicate-constructor-name-is-refused", unit => Raw.refusal(Family.T, E.Mismatch{"the constructor templateRefl is already declared in TemplateEq"}, declare(G.empty(), [], 1n, eq(L.var(0n)), [refl(), refl()]))},
   Raw.Case{"wrong-instance-arity-is-refused", unit => instance_arity()},
   Raw.Case{"free-instance-argument-is-refused", unit => free_instance()},
   Raw.Case{"unknown-family-template-is-refused", unit => Raw.refusal(G.T, E.Unbound{"unknown family universe schema Missing"}, instance(G.empty(), [], "Missing", [], "Unknown"))},
   Raw.Case{"other-template-reference-in-motive-metadata-is-refused", unit => other_template()},
   Raw.Case{"instance-cannot-reuse-template-name", unit => target_collision("TemplateEq", G.empty())},
   Raw.Case{"instance-cannot-replace-existing-global", unit => global_target()},
   Raw.Case{"empty-schema-honors-budget", unit => Raw.refusal(Unit, Raw.budget_error(), B.run_poll(Unit, Check.check_family_scheme_comp(G.empty(), 0n, empty("EmptyBudget", L.one()), []), F.exhausted()))},
   Raw.Case{"kernel-scheme-refuses-a-free-header-level", unit => Raw.refusal(Unit, Raw.scope_error(), Check.check_family_scheme(G.empty(), B.unlimited(), 1n, empty("KernelFreeHeader", L.succ(L.var(3n))), []))},
   Raw.Case{"empty-catalog-declaration-honors-budget", unit => Raw.refusal(Family.T, Raw.budget_error(), B.run_poll(Family.T, Family.declare_comp(G.empty(), [], 0n, empty("EmptyBudget", L.one()), [], []), F.exhausted()))},
   Raw.Case{"empty-instance-honors-budget", unit => empty_instance_budget()},
   Raw.Case{"instance-budget-expires-after-entry", unit => late_budget()},
   Raw.Case{"raw-substitution-consumes-the-caller-budget", unit => substitution_budget()},
   Raw.Case{"failed-instances-preserve-globals-and-catalog", unit => rollback()},
   Raw.Case{"instance-rechecks-against-current-global-types", unit => current_types()},
   Raw.Case{"instance-rechecks-missing-global-dependency", unit => missing_dependency()}]

def suite() -> Result<&2, &2, String, String>:
  do Result<&2, &2, String, String>:
    count : Nat <- Test.run(cases(), 0n)
    Done{"cases=" ++ Nat.show(count) ++ " type-excluded=negative-arity"}

def report_case(name: String, result: Result<&2, &2, String, Unit>) -> IO(Nat):
  match result:
    case Done{unit}:
      do IO<Nat>:
        printed : Unit <- IO.print("PASS family-poly " ++ name)
        IO.pure(Nat, 0n)
    case Fail{message}:
      do IO<Nat>:
        printed : Unit <- IO.print("FAIL family-poly " ++ name ++ ": " ++ message)
        IO.pure(Nat, 1n)

def finish_cases(count: Nat, failures: Nat) -> IO(Unit):
  match failures:
    case Zero{}: IO.print("FAMILY-POLY-OK cases=" ++ Nat.show(count) ++ " type-excluded=negative-arity")
    case Succ{rest}: Out.cli_exit(1)

@unsafe def run_cases(cases: List<&1, Raw.Case>, count: Nat, failures: Nat) -> IO(Unit):
  match cases:
    case Nil{}: finish_cases(count, failures)
    case Con{Raw.Case{name, test}, rest}:
      do IO<Unit>:
        failed : Nat <- report_case(name, test(Unit{}))
        run_cases(rest, 1n+count, (failures + failed : Nat))

def arguments(args: List<String>) -> IO(Unit):
  run_cases(cases(), 0n, 0n)

def main() -> IO(Unit):
  arguments([])
