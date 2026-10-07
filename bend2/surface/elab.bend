import Base
import ../kernel/pp.bend as Pp
import ../kernel/shape.bend as Shape
import ./elab_term.bend as E
import ./syntax.bend as Syntax
import ./parser.bend as Parser
import ./universe.bend as Universe
import ./poly.bend as Poly
import ./family_poly.bend as FamilyPoly
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/quantity.bend as Quantity
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as Positivity
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/common.bend as Common
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../kernel/order.bend as Order
import ../kernel/totality.bend as Totality

# The program pass owns one budget state across elaboration and trusted checking.
def Rows() -> Data:
  List<&2, Common.Pair<String, Global.Entry>>

def Checked() -> Data:
  Common.Pair<Global.T, Rows>

def elab(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, syntax: Syntax.T) -> Result<&2, &2, Error.T, Term.T>:
  Budget.run(Term.T, E.elab(ctx, expected, syntax), Check.context_budget(ctx))

def elab_decl(decl: Syntax.Decl, +ctx: Check.Ctx) -> Result<&2, &2, Error.T, Check.Decl>:
  Budget.run(Check.Decl, E.elab_decl(decl, ctx), Check.context_budget(ctx))

def elab_fam_decl(family: Syntax.Fam<Syntax.T>, +ctx: Check.Ctx) -> Result<&2, &2, Error.T, Check.FamilyDecl>:
  Budget.run(Check.FamilyDecl, E.elab_fam_decl(family, ctx), Check.context_budget(ctx))

def elab_ctor_decl(ctor: Syntax.FamCtor<Syntax.T>, +ctx: Check.Ctx, name: String) -> Result<&2, &2, Error.T, Check.CtorDecl>:
  Budget.run(Check.CtorDecl, E.elab_ctor_decl(ctor, ctx, name), Check.context_budget(ctx))

def context(globals: Global.T, arity: Nat) -> Check.Ctx:
  Check.make(globals, Budget.unlimited(), arity)

def family_names(families: List<&2, Syntax.Fam<Syntax.T>>) -> List<&2, String>:
  match families:
    case Nil{}:
      []
    case Con{Syntax.Fam{name, params, ty, ctors}, tail}:
      name <> family_names(tail)

def rec_names(definitions: List<&2, Syntax.RecDef<Syntax.T>>) -> List<&2, String>:
  match definitions:
    case Nil{}:
      []
    case Con{Syntax.RecDef{name, ty, body}, tail}:
      name <> rec_names(tail)

def syntax_ctor_names(ctors: List<&2, Syntax.FamCtor<Syntax.T>>) -> List<&2, String>:
  match ctors:
    case Nil{}:
      []
    case Con{Syntax.FamCtor{name, ty}, tail}:
      name <> syntax_ctor_names(tail)

def all_ctor_names(families: List<&2, Syntax.Fam<Syntax.T>>) -> List<&2, String>:
  match families:
    case Nil{}:
      []
    case Con{Syntax.Fam{name, params, ty, ctors}, tail}:
      List.append(&2, String, syntax_ctor_names(ctors), all_ctor_names(tail))

def declare_headers(families: List<&2, Syntax.Fam<Syntax.T>>, +globals: Global.T) -> Budget.Comp(Global.T):
  match families:
    case Nil{}:
      Budget.pure(Global.T, globals)
    case Con{head, tail}:
      do Budget.Comp<Global.T>:
        family : Check.FamilyDecl <- E.elab_fam_decl(head, context(globals, 0n))
        globals : Global.T <- Check.declare_family_comp(globals, 0n, family)
        declare_headers(tail, globals)

def define_families(families: List<&2, Syntax.Fam<Syntax.T>>, +globals: Global.T, +group: List<&2, String>) -> Budget.Comp(Global.T):
  match families:
    case Nil{}:
      Budget.pure(Global.T, globals)
    case Con{Syntax.Fam{+name, params, ty, ctors}, tail}:
      do Budget.Comp<Global.T>:
        telescope : E.TelescopeState <- E.elab_telescope(params, context(globals, 0n), [])
        ctors : List<&2, Check.CtorDecl> <- E.elab_ctor_decls(ctors, E.telescope_context(telescope), name)
        globals : Global.T <- Check.define_ctors_comp(globals, 0n, group, name, ctors)
        define_families(tail, globals, group)

def elab_mu_group_comp(globals: Global.T, +families: List<&2, Syntax.Fam<Syntax.T>>) -> Budget.Comp(Global.T):
  do Budget.Comp<Global.T>:
    globals : Global.T <- declare_headers(families, globals)
    define_families(families, globals, family_names(families))

def elab_mu_group(globals: Global.T, budget: Budget.T, families: List<&2, Syntax.Fam<Syntax.T>>) -> Result<&2, &2, Error.T, Global.T>:
  Budget.run(Global.T, elab_mu_group_comp(globals, families), budget)

def add_rows(rows: Rows, globals: Global.T) -> Global.T:
  match rows:
    case Nil{}:
      globals
    case Con{Common.Pair{name, entry}, tail}:
      add_rows(tail, Global.add(name, entry, globals))

type RecTyped is Data:
  RecTyped{name: String, ty: Term.T, syntax: Syntax.T}

type RecBody is Data:
  RecBody{name: String, ty: Term.T, expected: Value.T, body: Term.T}

def rec_types(definitions: List<&2, Syntax.RecDef<Syntax.T>>, +globals: Global.T) -> Budget.Comp(List<&2, RecTyped>):
  match definitions:
    case Nil{}:
      Budget.pure(List<&2, RecTyped>, [])
    case Con{Syntax.RecDef{name, ty, body}, tail}:
      do Budget.Comp<List<&2, RecTyped>>:
        ty : Term.T <- E.elab(context(globals, 0n), None{}, ty)
        tail : List<&2, RecTyped> <- rec_types(tail, globals)
        return RecTyped{name, ty, body} <> tail

def rec_postulates(definitions: List<&2, RecTyped>) -> List<&2, Check.Decl>:
  match definitions:
    case Nil{}:
      []
    case Con{RecTyped{name, ty, syntax}, tail}:
      Check.Decl{name, Check.Postulate{}, ty, None{}} <> rec_postulates(tail)

def rec_bodies(definitions: List<&2, RecTyped>, +globals: Global.T) -> Budget.Comp(List<&2, RecBody>):
  match definitions:
    case Nil{}:
      Budget.pure(List<&2, RecBody>, [])
    case Con{RecTyped{name, +ty, syntax}, tail}:
      do Budget.Comp<List<&2, RecBody>>:
        +ctx : Check.Ctx = context(globals, 0n)
        +expected : Value.T <- E.eval_in(ctx, ty)
        body : Term.T <- E.elab(ctx, Some{expected}, syntax)
        tail : List<&2, RecBody> <- rec_bodies(tail, globals)
        return RecBody{name, ty, expected, body} <> tail

def rec_pairs(bodies: List<&2, RecBody>) -> List<&2, Common.Pair<String, Term.T>>:
  match bodies:
    case Nil{}:
      []
    case Con{RecBody{name, ty, expected, body}, tail}:
      Common.Pair{name, body} <> rec_pairs(tail)

def rec_declarations(bodies: List<&2, RecBody>) -> List<&2, Check.Decl>:
  match bodies:
    case Nil{}:
      []
    case Con{RecBody{name, ty, expected, body}, tail}:
      Check.Decl{name, Check.Definition{}, ty, Some{body}} <> rec_declarations(tail)

def rec_arg(cert: Order.T, body: Term.T) -> Maybe<&2, Nat>:
  match cert:
    case Order.Certificate{group, arg, rows}:
      Bool.pick(Maybe<&2, Nat>, Order.mentions(group, body), Some{arg}, None{})

def guarded_bodies(bodies: List<&2, RecBody>, +globals: Global.T, +cert: Order.T) -> Budget.Comp(Rows):
  match bodies:
    case Nil{}:
      Budget.pure(Rows, [])
    case Con{RecBody{name, ty, expected, +body}, tail}:
      do Budget.Comp<Rows>:
        +term : Term.T <- Budget.lift(Term.T, Order.translate(cert, body))
        checked : Quantity.Usage <- Engine.check_uses(context(globals, 0n), Quantity.QMany{}, term, expected)
        tail : Rows <- guarded_bodies(tail, globals, cert)
        return Common.Pair{name, Global.Def{Global.DefEntry{ty, term, True{}, rec_arg(cert, body), False{}}}} <> tail

def check_rec_bodies(cert: Maybe<&2, Order.T>, bodies: List<&2, RecBody>, globals: Global.T, provisional: Global.T) -> Budget.Comp(Rows):
  match cert:
    case None{}:
      Check.check_decls_comp(globals, rec_declarations(bodies))
    case Some{cert}:
      guarded_bodies(bodies, provisional, cert)

def elab_rec_group_comp(+globals: Global.T, definitions: List<&2, Syntax.RecDef<Syntax.T>>) -> Budget.Comp(Checked):
  do Budget.Comp<Checked>:
    +typed : List<&2, RecTyped> <- rec_types(definitions, globals)
    declared : Rows <- Check.check_decls_comp(globals, rec_postulates(typed))
    +provisional : Global.T = add_rows(declared, globals)
    +bodies : List<&2, RecBody> <- rec_bodies(typed, provisional)
    cert : Maybe<&2, Order.T> <- Totality.guard_group_comp(provisional, rec_pairs(bodies))
    +rows : Rows <- check_rec_bodies(cert, bodies, globals, provisional)
    return Common.Pair{add_rows(rows, globals), rows}

def elab_rec_group(globals: Global.T, budget: Budget.T, definitions: List<&2, Syntax.RecDef<Syntax.T>>) -> Result<&2, &2, Error.T, Checked>:
  Budget.run(Checked, elab_rec_group_comp(globals, definitions), budget)

type FamilyParts is Data:
  FamilyParts{family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>, provisional: Global.T}

def ctor_distinct(ctors: List<&2, Syntax.FamCtor<Syntax.T>>, +family: String) -> Budget.Comp(Unit):
  match ctors:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{Syntax.FamCtor{+name, ty}, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, String.eq(name, family),
          Fail{Error.Mismatch{"the constructor " ++ name ++ " repeats the family name " ++ family}}, Done{Unit{}}))
        ctor_distinct(tail, family)

def elab_family_parts(syntax: Syntax.Fam<Syntax.T>, +globals: Global.T, +arity: Nat) -> Budget.Comp(FamilyParts):
  match syntax:
    case Syntax.Fam{+name, +params, ty, +ctors}:
      do Budget.Comp<FamilyParts>:
        checked : Unit <- ctor_distinct(ctors, name)
        +family : Check.FamilyDecl <- E.elab_fam_decl(Syntax.Fam{name, params, ty, ctors}, context(globals, arity))
        +provisional : Global.T <- Check.declare_family_comp(globals, arity, family)
        telescope : E.TelescopeState <- E.elab_telescope(params, context(provisional, arity), [])
        ctors : List<&2, Check.CtorDecl> <- E.elab_ctor_decls(ctors, E.telescope_context(telescope), name)
        return FamilyParts{family, ctors, provisional}

def declare_template(parts: FamilyParts, globals: Global.T, catalog: FamilyPoly.T, arity: Nat) -> Budget.Comp(FamilyPoly.T):
  match parts:
    case FamilyParts{family, ctors, provisional}:
      FamilyPoly.declare_comp(globals, catalog, arity, family, ctors, [])

def elab_family_template(+globals: Global.T, catalog: FamilyPoly.T, +arity: Nat, family: Syntax.Fam<Syntax.T>) -> Budget.Comp(FamilyPoly.T):
  do Budget.Comp<FamilyPoly.T>:
    parts : FamilyParts <- elab_family_parts(family, globals, arity)
    declare_template(parts, globals, catalog, arity)

def template_reservations(names: List<&2, String>, +globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):
  match names:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{+name, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, name)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, name)) || FamilyPoly.ctor_declared(globals, name), name)
        template_reservations(tail, globals, definitions, catalog)

def member_callback(member: Syntax.RecDef<Syntax.T>, arity: Nat) -> FamilyPoly.MemberElaborator:
  match member:
    case Syntax.RecDef{+name, ty, body}:
      +arity = arity
      symbolic => do Budget.Comp<Check.Decl>:
        +symbolic : Global.T = symbolic
        checked : Unit <- FamilyPoly.require_fresh(Poly.occupied(symbolic, [], name) || FamilyPoly.ctor_declared(symbolic, name), name)
        E.elab_decl(Syntax.DDef{name, ty, body}, context(symbolic, arity))

def member_callbacks(members: List<&2, Syntax.RecDef<Syntax.T>>, +arity: Nat) -> List<&1, FamilyPoly.MemberElaborator>:
  match members:
    case Nil{}:
      []
    case Con{head, tail}:
      member_callback(head, arity) <> member_callbacks(tail, arity)

law symbolic_families:
  for families: List<&2, Syntax.Fam<Syntax.T>>
  for globals: Global.T
  for +arity: Nat
  Budget.Comp(List<&2, FamilyPoly.RawFamily>)

@unsafe
def symbolic_family_rest(parts: FamilyParts, tail: List<&2, Syntax.Fam<Syntax.T>>, +arity: Nat) -> Budget.Comp(List<&2, FamilyPoly.RawFamily>):
  match parts:
    case FamilyParts{+family, +ctors, provisional}:
      do Budget.Comp<List<&2, FamilyPoly.RawFamily>>:
        symbolic : Global.T <- Check.define_ctors_comp(provisional, arity, [FamilyPoly.family_name(family)], FamilyPoly.family_name(family), ctors)
        tail : List<&2, FamilyPoly.RawFamily> <- symbolic_families(tail, symbolic, arity)
        return Common.Pair{family, ctors} <> tail

@unsafe
def symbolic_families(families, globals, arity):
  match families:
    case Nil{}:
      Budget.pure(List<&2, FamilyPoly.RawFamily>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, FamilyPoly.RawFamily>>:
        parts : FamilyParts <- elab_family_parts(head, globals, arity)
        symbolic_family_rest(parts, tail, arity)

def elab_family_group(+globals: Global.T, +catalog: FamilyPoly.T, definitions: Poly.T, +arity: Nat, +group: List<&2, Syntax.Fam<Syntax.T>>, +members: List<&2, Syntax.RecDef<Syntax.T>>) -> Budget.Comp(FamilyPoly.T):
  do Budget.Comp<FamilyPoly.T>:
    checked : Unit <- template_reservations(List.append(&2, String, family_names(group), rec_names(members)), globals, definitions, catalog)
    families : List<&2, FamilyPoly.RawFamily> <- symbolic_families(group, globals, arity)
    FamilyPoly.declare_group_elaborated_comp(globals, catalog, arity, families, member_callbacks(members, arity))

def lower_levels(levels: List<&2, Universe.T>) -> Budget.Comp(List<&2, Level.T>):
  match levels:
    case Nil{}:
      Budget.pure(List<&2, Level.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Level.T>>:
        head : Level.T <- Budget.lift(Level.T, Universe.lower(head))
        tail : List<&2, Level.T> <- lower_levels(tail)
        return head <> tail

def lower_dependencies(dependencies: List<&2, Syntax.Specialization>) -> Budget.Comp(List<&2, FamilyPoly.Dependency>):
  match dependencies:
    case Nil{}:
      Budget.pure(List<&2, FamilyPoly.Dependency>, [])
    case Con{Syntax.Specialization{source, levels, alias, reuse, exports}, tail}:
      do Budget.Comp<List<&2, FamilyPoly.Dependency>>:
        levels : List<&2, Level.T> <- lower_levels(levels)
        tail : List<&2, FamilyPoly.Dependency> <- lower_dependencies(tail)
        return FamilyPoly.Dependency{source, levels, alias, reuse, exports} <> tail

def planned_names(value: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>) -> List<&2, String>:
  match value:
    case None{}:
      []
    case Some{Common.Pair{families, members}}:
      List.append(&2, String, families, members)

def planned_families(value: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>) -> List<&2, String>:
  match value:
    case None{}:
      []
    case Some{Common.Pair{families, members}}:
      families

def validate_dependency_exports(exports: Maybe<&2, FamilyPoly.Bindings>, globals: Global.T, available: List<&2, String>, catalog: FamilyPoly.T, source: String, alias: String, reuse: FamilyPoly.Bindings) -> Budget.Comp(Unit):
  match exports:
    case None{}:
      Budget.pure(Unit, Unit{})
    case Some{exports}:
      FamilyPoly.validate_exports_comp(globals, available, catalog, source, alias, reuse, exports)

def validate_dependencies(dependencies: List<&2, FamilyPoly.Dependency>, +globals: Global.T, +catalog: FamilyPoly.T, +available: List<&2, String>) -> Budget.Comp(Unit):
  match dependencies:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{FamilyPoly.Dependency{+source, levels, +alias, +reuse, +exports}, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- validate_dependency_exports(exports, globals, available, catalog, source, alias, reuse)
        fresh : List<&2, String> = planned_families(FamilyPoly.instance_names(catalog, source, alias, reuse, exports))
        validate_dependencies(tail, globals, catalog, List.append(&2, String, fresh, available))

def imported_names(dependencies: List<&2, FamilyPoly.Dependency>, +catalog: FamilyPoly.T) -> List<&2, String>:
  match dependencies:
    case Nil{}:
      []
    case Con{FamilyPoly.Dependency{source, levels, +alias, reuse, exports}, tail}:
      alias <> List.append(&2, String, planned_names(FamilyPoly.instance_names(catalog, source, alias, reuse, exports)), imported_names(tail, catalog))

def option_names(value: Maybe<&2, List<&2, String>>) -> List<&2, String>:
  match value:
    case None{}:
      []
    case Some{names}:
      names

def imported_labels(dependencies: List<&2, FamilyPoly.Dependency>, +catalog: FamilyPoly.T) -> List<&2, String>:
  match dependencies:
    case Nil{}:
      []
    case Con{FamilyPoly.Dependency{source, levels, alias, reuse, exports}, tail}:
      List.append(&2, String, option_names(FamilyPoly.constructors(catalog, source)), imported_labels(tail, catalog))

def label_reservations(labels: List<&2, String>, +names: List<&2, String>, +root: String, +globals: Global.T, +definitions: Poly.T, +catalog: FamilyPoly.T) -> Budget.Comp(Unit):
  match labels:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{+label, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- FamilyPoly.require_fresh(String.eq(label, root) || Poly.member(names, label) ||
          Poly.is_some(Nat, Poly.arity(definitions, label)) || Poly.is_some(Nat, FamilyPoly.arity(catalog, label)) ||
          Poly.is_some(Positivity.Family, Global.find_family(label, globals)) ||
          (Poly.is_some(Global.Entry, Global.find(label, globals)) && Bool.not(FamilyPoly.ctor_declared(globals, label))), label)
        label_reservations(tail, names, root, globals, definitions, catalog)

def elab_composition(+globals: Global.T, +catalog: FamilyPoly.T, +definitions: Poly.T, +arity: Nat, +name: String, dependencies: List<&2, Syntax.Specialization>, +members: List<&2, Syntax.RecDef<Syntax.T>>) -> Budget.Comp(FamilyPoly.T):
  do Budget.Comp<FamilyPoly.T>:
    +dependencies : List<&2, FamilyPoly.Dependency> <- lower_dependencies(dependencies)
    checked : Unit <- validate_dependencies(dependencies, globals, catalog, [])
    +names : List<&2, String> = List.append(&2, String, imported_names(dependencies, catalog), rec_names(members))
    tick : Unit <- Budget.tick()
    checked : Unit <- FamilyPoly.collision_list(names, [name])
    checked : Unit <- template_reservations(names, globals, definitions, catalog)
    checked : Unit <- label_reservations(imported_labels(dependencies, catalog), names, name, globals, definitions, catalog)
    FamilyPoly.compose_comp(globals, catalog, arity, name, dependencies, member_callbacks(members, arity))

type Program is Data:
  Program{globals: Global.T, definitions: Poly.T, families: FamilyPoly.T, reversed: Rows}

def declared_names(decl: Syntax.Decl) -> List<&2, String>:
  match decl:
    case Syntax.DDef{name, ty, body}:
      [name]
    case Syntax.DAxiom{name, ty}:
      [name]
    case Syntax.DPoly{arity, name, ty, body}:
      [name]
    case Syntax.DPolyCompose{arity, name, dependencies, members}:
      [name]
    case Syntax.DSpecialize{Syntax.Specialization{source, levels, name, reuse, exports}}:
      [name]
    case Syntax.DPolyMu{arity, Syntax.Fam{name, params, ty, ctors}}:
      [name]
    case Syntax.DPolyGroup{arity, Syntax.Fam{name, params, ty, ctors}, companions, members}:
      [name]
    case Syntax.DMu{+families}:
      List.append(&2, String, family_names(families), all_ctor_names(families))
    case Syntax.DRec{members}:
      rec_names(members)

def ctor_reserved(decl: Syntax.Decl) -> List<&2, String>:
  match decl:
    case Syntax.DPoly{arity, name, ty, body}:
      [name]
    case Syntax.DPolyCompose{arity, name, dependencies, members}:
      [name]
    case Syntax.DSpecialize{Syntax.Specialization{source, levels, name, reuse, exports}}:
      [name]
    case Syntax.DPolyMu{arity, Syntax.Fam{name, params, ty, ctors}}:
      [name]
    case Syntax.DPolyGroup{arity, Syntax.Fam{name, params, ty, ctors}, companions, members}:
      [name]
    case other:
      []

def reserve_catalogs(names: List<&2, String>, +definitions: Poly.T, +families: FamilyPoly.T) -> Budget.Comp(Unit):
  match names:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{+name, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- FamilyPoly.require_fresh(Poly.is_some(Nat, Poly.arity(definitions, name)) || Poly.is_some(Nat, FamilyPoly.arity(families, name)), name)
        reserve_catalogs(tail, definitions, families)

def reserve_ctors(names: List<&2, String>, +globals: Global.T) -> Budget.Comp(Unit):
  match names:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{+name, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- FamilyPoly.require_fresh(FamilyPoly.ctor_declared(globals, name), name)
        reserve_ctors(tail, globals)

def global_entry(value: Maybe<&2, Global.Entry>, message: String) -> Budget.Comp(Global.Entry):
  match value:
    case None{}:
      Budget.failed(Global.Entry, Error.Cannot_infer{message})
    case Some{entry}:
      Budget.pure(Global.Entry, entry)

def instance_members(names: List<&2, String>, +globals: Global.T) -> Budget.Comp(Rows):
  match names:
    case Nil{}:
      Budget.pure(Rows, [])
    case Con{+name, tail}:
      do Budget.Comp<Rows>:
        entry : Global.Entry <- global_entry(Global.find(name, globals), "specialization returned no member")
        tail : Rows <- instance_members(tail, globals)
        return Common.Pair{name, entry} <> tail

def kernel_ctor_names(family: Positivity.Family) -> List<&2, String>:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      Check.ctor_names(ctors)

def instance_labels(names: List<&2, String>, +globals: Global.T) -> Budget.Comp(List<&2, String>):
  match names:
    case Nil{}:
      Budget.pure(List<&2, String>, [])
    case Con{name, tail}:
      do Budget.Comp<List<&2, String>>:
        family : Positivity.Family <- FamilyPoly.family_option(Global.find_family(name, globals), Error.Cannot_infer{"specialization returned no family"})
        tail : List<&2, String> <- instance_labels(tail, globals)
        return List.append(&2, String, kernel_ctor_names(family), tail)

def specialize_family_names(names: Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>, installed: Global.T, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: Rows, source: String, +alias: String) -> Budget.Comp(Program):
  match names:
    case None{}:
      Budget.failed(Program, Error.Unbound{"unknown family universe schema " ++ source})
    case Some{Common.Pair{+family_names, +members}}:
      do Budget.Comp<Program>:
        +installed : Global.T = installed
        +generated : List<&2, String> = List.append(&2, String, family_names, members)
        checked : Unit <- template_reservations(alias <> generated, globals, definitions, [])
        labels : List<&2, String> <- instance_labels(family_names, installed)
        checked : Unit <- label_reservations(labels, generated, alias, globals, definitions, families)
        member_rows : Rows <- instance_members(members, installed)
        return Program{installed, definitions, families, List.append(&2, Common.Pair<String, Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, member_rows), rows)}

def definition_clauses(reuse: FamilyPoly.Bindings, exports: Maybe<&2, FamilyPoly.Bindings>, known: Bool) -> Budget.Comp(Unit):
  match reuse exports known:
    case Nil{} None{} known:
      Budget.pure(Unit, Unit{})
    case reuse exports False{}:
      Budget.pure(Unit, Unit{})
    case Con{head, tail} exports True{}:
      Budget.failed(Unit, Error.Not_yet{"family reuse requires a family template"})
    case Nil{} Some{exports} True{}:
      Budget.failed(Unit, Error.Not_yet{"member exports require a family template"})

def specialize_definition(result: Common.Pair<Global.T, Term.T>, definitions: Poly.T, families: FamilyPoly.T, rows: Rows, +alias: String) -> Budget.Comp(Program):
  match result:
    case Common.Pair{+globals, term}:
      do Budget.Comp<Program>:
        entry : Global.Entry <- global_entry(Global.find(alias, globals), "specialization returned no global entry")
        return Program{globals, definitions, families, Common.Pair{alias, entry} <> rows}

def specialize_kind(is_family: Bool, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: Rows, +source: String, levels: List<&2, Level.T>, +alias: String, +reuse: FamilyPoly.Bindings, +exports: Maybe<&2, FamilyPoly.Bindings>) -> Budget.Comp(Program):
  match is_family:
    case True{}:
      do Budget.Comp<Program>:
        installed : Global.T <- FamilyPoly.instantiate_comp(globals, families, source, levels, alias, reuse, exports)
        specialize_family_names(FamilyPoly.instance_names(families, source, alias, reuse, exports), installed, globals, definitions, families, rows, source, alias)
    case False{}:
      do Budget.Comp<Program>:
        checked : Unit <- definition_clauses(reuse, exports, Poly.is_some(Nat, Poly.arity(definitions, source)))
        result : Common.Pair<Global.T, Term.T> <- Poly.instantiate_comp(globals, definitions, source, levels, alias)
        specialize_definition(result, definitions, families, rows, alias)

def append_checked(result: Checked, definitions: Poly.T, families: FamilyPoly.T, rows: Rows) -> Program:
  match result:
    case Common.Pair{globals, checked}:
      Program{globals, definitions, families, List.append(&2, Common.Pair<String, Global.Entry>, List.reverse(&2, Common.Pair<String, Global.Entry>, checked), rows)}

def append_one(checked: Rows, globals: Global.T, definitions: Poly.T, families: FamilyPoly.T, rows: Rows) -> Budget.Comp(Program):
  match checked:
    case Con{Common.Pair{+name, +entry}, Nil{}}:
      Budget.pure(Program, Program{Global.add(name, entry, globals), definitions, families, Common.Pair{name, entry} <> rows})
    case other:
      Budget.failed(Program, Error.Cannot_infer{"the checker answered no entry for a declaration"})

def elab_declaration(decl: Syntax.Decl, +globals: Global.T, +definitions: Poly.T, +families: FamilyPoly.T, rows: Rows) -> Budget.Comp(Program):
  match decl:
    case Syntax.DPoly{+arity, name, ty, body}:
      do Budget.Comp<Program>:
        raw : Check.Decl <- E.elab_decl(Syntax.DDef{name, ty, body}, context(globals, arity))
        definitions : Poly.T <- Poly.declare_comp(globals, definitions, arity, raw)
        return Program{globals, definitions, families, rows}
    case Syntax.DPolyMu{arity, family}:
      do Budget.Comp<Program>:
        families : FamilyPoly.T <- elab_family_template(globals, families, arity, family)
        return Program{globals, definitions, families, rows}
    case Syntax.DPolyGroup{arity, family, companions, members}:
      do Budget.Comp<Program>:
        families : FamilyPoly.T <- elab_family_group(globals, families, definitions, arity, family <> companions, members)
        return Program{globals, definitions, families, rows}
    case Syntax.DPolyCompose{arity, name, dependencies, members}:
      do Budget.Comp<Program>:
        families : FamilyPoly.T <- elab_composition(globals, families, definitions, arity, name, dependencies, members)
        return Program{globals, definitions, families, rows}
    case Syntax.DSpecialize{Syntax.Specialization{+source, levels, alias, reuse, exports}}:
      do Budget.Comp<Program>:
        levels : List<&2, Level.T> <- lower_levels(levels)
        specialize_kind(Poly.is_some(Nat, FamilyPoly.arity(families, source)), globals, definitions, families, rows, source, levels, alias, reuse, exports)
    case Syntax.DMu{group}:
      do Budget.Comp<Program>:
        globals : Global.T <- elab_mu_group_comp(globals, group)
        return Program{globals, definitions, families, rows}
    case Syntax.DRec{members}:
      do Budget.Comp<Program>:
        result : Checked <- elab_rec_group_comp(globals, members)
        return append_checked(result, definitions, families, rows)
    case other:
      do Budget.Comp<Program>:
        raw : Check.Decl <- E.elab_decl(other, context(globals, 0n))
        checked : Rows <- Check.check_decls_comp(globals, [raw])
        append_one(checked, globals, definitions, families, rows)

def program_step(state: Program, +decl: Syntax.Decl) -> Budget.Comp(Program):
  match state:
    case Program{+globals, +definitions, +families, rows}:
      do Budget.Comp<Program>:
        checked : Unit <- reserve_catalogs(declared_names(decl), definitions, families)
        checked : Unit <- reserve_ctors(ctor_reserved(decl), globals)
        elab_declaration(decl, globals, definitions, families, rows)

def program_loop(decls: List<&2, Syntax.Decl>, state: Program) -> Budget.Comp(Program):
  match decls:
    case Nil{}:
      Budget.pure(Program, state)
    case Con{head, tail}:
      do Budget.Comp<Program>:
        state : Program <- program_step(state, head)
        program_loop(tail, state)

def finish_program(state: Program) -> Checked:
  match state:
    case Program{globals, definitions, families, reversed}:
      Common.Pair{globals, List.reverse(&2, Common.Pair<String, Global.Entry>, reversed)}

def elab_program_comp(globals: Global.T, decls: List<&2, Syntax.Decl>) -> Budget.Comp(Checked):
  do Budget.Comp<Checked>:
    state : Program <- program_loop(decls, Program{globals, Poly.empty(), FamilyPoly.empty(), []})
    return finish_program(state)

def elab_program_in(globals: Global.T, budget: Budget.T, decls: List<&2, Syntax.Decl>) -> Result<&2, &2, Error.T, Checked>:
  Budget.run(Checked, elab_program_comp(globals, decls), budget)

def result_rows(result: Result<&2, &2, Error.T, Checked>) -> Result<&2, &2, Error.T, Rows>:
  match result:
    case Fail{error}:
      Fail{error}
    case Done{Common.Pair{globals, rows}}:
      Done{rows}

def elab_program(globals: Global.T, budget: Budget.T, decls: List<&2, Syntax.Decl>) -> Result<&2, &2, Error.T, Rows>:
  result_rows(elab_program_in(globals, budget, decls))

def check_in(globals: Global.T, budget: Budget.T, source: String) -> Result<&2, &2, Error.T, Checked>:
  do Result<&2, &2, Error.T, Checked>:
    decls : List<&2, Syntax.Decl> <- Parser.program(source)
    elab_program_in(globals, budget, decls)

def check_text(globals: Global.T, budget: Budget.T, source: String) -> Result<&2, &2, Error.T, Rows>:
  result_rows(check_in(globals, budget, source))

def entry_text(row: Common.Pair<String, Global.Entry>) -> String:
  match row:
    case Common.Pair{name, Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}}:
      "def " ++ name ++ " : " ++ Pp.term([], ty) ++ " := " ++ Pp.term([], body) ++ "\n"
    case Common.Pair{name, Global.Axiom{ty}}:
      "axiom " ++ name ++ " : " ++ Pp.term([], ty) ++ "\n"
    case Common.Pair{name, Global.Prim{ty, prim}}:
      "prim " ++ name ++ " : " ++ Pp.term([], ty) ++ "\n"

def checked_form(rows: Rows) -> String:
  match rows:
    case Nil{}:
      ""
    case Con{head, tail}:
      entry_text(head) ++ checked_form(tail)

def axiom_names(rows: Rows) -> List<&2, String>:
  match rows:
    case Nil{}:
      []
    case Con{Common.Pair{name, Global.Axiom{ty}}, tail}:
      name <> axiom_names(tail)
    case Con{other, tail}:
      axiom_names(tail)

def disclosure_of_shape(-A: Data, shape: Shape.T<A>) -> Maybe<&2, String>:
  match shape:
    case Shape.SZk{q, name, witness}:
      Some{"zk-soundness"}
    case Shape.SFhc{level}:
      Some{"fhc-correctness"}
    case Shape.SMpc{parties, access}:
      Some{"mpc-simulatability"}
    case Shape.SPi{q, name, domain}:
      None{}
    case Shape.SArg{q, name}:
      None{}
    case Shape.SColl{count}:
      None{}
    case Shape.SPar{left, right}:
      None{}
    case Shape.SMu{name, indices}:
      None{}
    case Shape.SNu{name, indices}:
      None{}

def prepend_disclosure(value: Maybe<&2, String>, tail: List<&2, String>) -> List<&2, String>:
  match value:
    case None{}:
      tail
    case Some{name}:
      name <> tail

law term_disclosures:
  for term: Term.T
  List<&2, String>

@unsafe
def term_list_disclosures(terms: List<&2, Term.T>) -> List<&2, String>:
  match terms:
    case Nil{}:
      []
    case Con{head, tail}:
      List.append(&2, String, term_disclosures(head), term_list_disclosures(tail))

@unsafe
def shape_disclosures(+shape: Shape.T<Term.T>) -> List<&2, String>:
  prepend_disclosure(disclosure_of_shape(Term.T, shape), term_list_disclosures(Shape.payload(Term.T, shape)))

@unsafe
def addr_disclosures(addr: Term.Addr<Term.T>) -> List<&2, String>:
  match addr:
    case Term.APt{q, arg}:
      term_disclosures(arg)
    case Term.ALeg{index}:
      []
    case Term.ACtor{name}:
      []

@unsafe
def leg_disclosures(leg: Term.Leg<Term.T>) -> List<&2, String>:
  match leg:
    case Term.Leg{binders, body}:
      term_disclosures(body)

@unsafe
def legs_disclosures(legs: List<&2, Term.Leg<Term.T>>) -> List<&2, String>:
  match legs:
    case Nil{}:
      []
    case Con{head, tail}:
      List.append(&2, String, leg_disclosures(head), legs_disclosures(tail))

@unsafe
def motive_disclosures(motive: Maybe<&2, Term.Motive<Term.T>>) -> List<&2, String>:
  match motive:
    case None{}:
      []
    case Some{Term.Motive{family, indices, name, body}}:
      term_disclosures(body)

@unsafe
def branches_disclosures(branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> List<&2, String>:
  match branches:
    case Nil{}:
      []
    case Con{Common.Pair{addr, leg}, tail}:
      List.append(&2, String, addr_disclosures(addr), List.append(&2, String, leg_disclosures(leg), branches_disclosures(tail)))

@unsafe
def elim_disclosures(elim: Term.ElimData<Term.T>) -> List<&2, String>:
  match elim:
    case Term.ElimData{shape, scrut, quantity, motive, branches}:
      List.append(&2, String, shape_disclosures(shape), List.append(&2, String, term_disclosures(scrut), List.append(&2, String, motive_disclosures(motive), branches_disclosures(branches))))

@unsafe
def term_disclosures(term):
  match term:
    case Term.Var{index}:
      []
    case Term.Univ{level}:
      []
    case Term.Global{name}:
      []
    case Term.Lit{value}:
      []
    case Term.Auto{}:
      []
    case Term.Lan{shape, codomain}:
      List.append(&2, String, shape_disclosures(shape), term_disclosures(codomain))
    case Term.Ran{shape, codomain}:
      List.append(&2, String, shape_disclosures(shape), term_disclosures(codomain))
    case Term.In{shape, addr, args}:
      List.append(&2, String, shape_disclosures(shape), List.append(&2, String, addr_disclosures(addr), term_list_disclosures(args)))
    case Term.Elim{elim}:
      elim_disclosures(elim)
    case Term.Sec{shape, legs}:
      List.append(&2, String, shape_disclosures(shape), legs_disclosures(legs))
    case Term.Out{shape, addr, head}:
      List.append(&2, String, shape_disclosures(shape), List.append(&2, String, addr_disclosures(addr), term_disclosures(head)))
    case Term.Let{name, ty, value, body}:
      List.append(&2, String, term_disclosures(ty), List.append(&2, String, term_disclosures(value), term_disclosures(body)))
    case Term.Ann{term, ty}:
      List.append(&2, String, term_disclosures(term), term_disclosures(ty))

def entry_disclosures(entry: Global.Entry) -> List<&2, String>:
  match entry:
    case Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}:
      List.append(&2, String, term_disclosures(ty), term_disclosures(body))
    case Global.Axiom{ty}:
      term_disclosures(ty)
    case Global.Prim{ty, prim}:
      term_disclosures(ty)

def row_disclosures(rows: Rows) -> List<&2, String>:
  match rows:
    case Nil{}:
      []
    case Con{Common.Pair{name, entry}, tail}:
      List.append(&2, String, entry_disclosures(entry), row_disclosures(tail))

def unique_disclosures(names: List<&2, String>, +seen: List<&2, String>) -> List<&2, String>:
  match names:
    case Nil{}:
      []
    case Con{+name, +tail}:
      Bool.pick(List<&2, String>, Poly.member(seen, name), unique_disclosures(tail, seen), name <> unique_disclosures(tail, name <> seen))

def disclosure_names(+rows: Rows) -> List<&2, String>:
  List.append(&2, String, axiom_names(rows), unique_disclosures(row_disclosures(rows), []))
