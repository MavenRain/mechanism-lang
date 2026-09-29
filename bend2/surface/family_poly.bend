import Base
import ../kernel/global.bend as Global
import ../kernel/level_eq.bend as LevelEq
import ./poly.bend as Poly
import ../kernel/common.bend as Common
import ../kernel/check.bend as Check
import ../kernel/positivity.bend as Positivity
import ../kernel/term.bend as Term
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

# Raw family traversals precede trusted checking. They never certify entries.
def map_telescope(telescope: Positivity.Telescope, +action: Poly.Action) -> Budget.Comp(Positivity.Telescope):
  match telescope:
    case Nil{}:
      Budget.pure(Positivity.Telescope, [])
    case Con{Positivity.Binder{quantity, name, ty}, tail}:
      do Budget.Comp<Positivity.Telescope>:
        ty : Term.T <- Poly.map_term(action, ty)
        tail : Positivity.Telescope <- map_telescope(tail, action)
        return Positivity.Binder{quantity, name, ty} <> tail

def map_ctor(ctor: Check.CtorDecl, +action: Poly.Action) -> Budget.Comp(Check.CtorDecl):
  match ctor:
    case Check.CtorDecl{name, args, res_params, res_idx}:
      do Budget.Comp<Check.CtorDecl>:
        tick : Unit <- Budget.tick()
        args : Positivity.Telescope <- map_telescope(args, action)
        res_params : List<&2, Term.T> <- Poly.map_terms(res_params, action)
        res_idx : List<&2, Term.T> <- Poly.map_terms(res_idx, action)
        return Check.CtorDecl{name, args, res_params, res_idx}

def map_ctors(ctors: List<&2, Check.CtorDecl>, +action: Poly.Action) -> Budget.Comp(List<&2, Check.CtorDecl>):
  match ctors:
    case Nil{}:
      Budget.pure(List<&2, Check.CtorDecl>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Check.CtorDecl>>:
        head : Check.CtorDecl <- map_ctor(head, action)
        tail : List<&2, Check.CtorDecl> <- map_ctors(tail, action)
        return head <> tail

def map_family(family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>, +action: Poly.Action) -> Budget.Comp(Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>):
  match family:
    case Check.FamilyDecl{name, params, indices, level}:
      do Budget.Comp<Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>>:
        tick : Unit <- Budget.tick()
        name : String <- Budget.lift(String, Poly.map_name(action, name))
        level : Level.T <- Budget.lift(Level.T, Poly.map_level(action, level))
        params : Positivity.Telescope <- map_telescope(params, action)
        indices : Positivity.Telescope <- map_telescope(indices, action)
        ctors : List<&2, Check.CtorDecl> <- map_ctors(ctors, action)
        return Common.Pair{Check.FamilyDecl{name, params, indices, level}, ctors}

def member_kind(kind: Check.DeclKind) -> Result<&2, &2, Error.T, Unit>:
  match kind:
    case Check.Definition{}:
      Done{Unit{}}
    case Check.Postulate{}:
      Fail{Error.Not_yet{"family schema members must be definitions"}}

def member_body(body: Maybe<&2, Term.T>, name: String) -> Result<&2, &2, Error.T, Term.T>:
  match body:
    case Some{body}:
      Done{body}
    case None{}:
      Fail{Error.Cannot_infer{"the definition " ++ name ++ " has no body"}}

def map_member(decl: Check.Decl, +action: Poly.Action) -> Budget.Comp(Check.Decl):
  match decl:
    case Check.Decl{+name, +kind, ty, body}:
      do Budget.Comp<Check.Decl>:
        tick : Unit <- Budget.tick()
        checked : Unit <- Budget.lift(Unit, member_kind(kind))
        mapped_name : String <- Budget.lift(String, Poly.map_name(action, name))
        ty : Term.T <- Poly.map_term(action, ty)
        body : Term.T <- Budget.lift(Term.T, member_body(body, name))
        body : Term.T <- Poly.map_term(action, body)
        return Check.Decl{mapped_name, kind, ty, Some{body}}

def unique_ctors(ctors: List<&2, Check.CtorDecl>, +family: String, +seen: List<&2, String>) -> Budget.Comp(Unit):
  match ctors:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{Check.CtorDecl{+name, args, params, indices}, tail}:
      do Budget.Comp<Unit>:
        tick : Unit <- Budget.tick()
        checked : Unit <- Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, Poly.member(seen, name),
          Fail{Error.Mismatch{"the constructor " ++ name ++ " is already declared in " ++ family}}, Done{Unit{}}))
        unique_ctors(tail, family, name <> seen)

type Scheme is Data:
  Scheme{arity: Nat, family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>,
    companions: List<&2, Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>>, members: List<&2, Check.Decl>}

def T() -> Data:
  List<&2, Common.Pair<String, Scheme>>

def empty() -> T:
  []

def lookup(catalog: T, +name: String) -> Maybe<&2, Scheme>:
  match catalog:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, scheme}, tail}:
      Bool.pick(Maybe<&2, Scheme>, String.eq(key, name), Some{scheme}, lookup(tail, name))

def scheme_arity(scheme: Maybe<&2, Scheme>) -> Maybe<&2, Nat>:
  match scheme:
    case None{}:
      None{}
    case Some{Scheme{arity, family, ctors, companions, members}}:
      Some{arity}

def arity(catalog: T, name: String) -> Maybe<&2, Nat>:
  scheme_arity(lookup(catalog, name))

def member_names(members: List<&2, Check.Decl>) -> List<&2, String>:
  match members:
    case Nil{}:
      []
    case Con{Check.Decl{name, kind, ty, body}, tail}:
      name <> member_names(tail)

def family_name(family: Check.FamilyDecl) -> String:
  match family:
    case Check.FamilyDecl{name, params, indices, level}:
      name

def companion_names(companions: List<&2, Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>>) -> List<&2, String>:
  match companions:
    case Nil{}:
      []
    case Con{Common.Pair{family, ctors}, tail}:
      family_name(family) <> companion_names(tail)

def ctor_names(ctors: List<&2, Check.CtorDecl>) -> List<&2, String>:
  match ctors:
    case Nil{}:
      []
    case Con{Check.CtorDecl{name, args, params, indices}, tail}:
      name <> ctor_names(tail)

def companion_ctors(companions: List<&2, Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>>) -> List<&2, String>:
  match companions:
    case Nil{}:
      []
    case Con{Common.Pair{family, ctors}, tail}:
      List.append(&2, String, ctor_names(ctors), companion_ctors(tail))

def scheme_members(scheme: Maybe<&2, Scheme>) -> Maybe<&2, List<&2, String>>:
  match scheme:
    case None{}:
      None{}
    case Some{Scheme{arity, family, ctors, companions, members}}:
      Some{member_names(members)}

def members(catalog: T, name: String) -> Maybe<&2, List<&2, String>>:
  scheme_members(lookup(catalog, name))

def scheme_companions(scheme: Maybe<&2, Scheme>) -> Maybe<&2, List<&2, String>>:
  match scheme:
    case None{}:
      None{}
    case Some{Scheme{arity, family, ctors, companions, members}}:
      Some{companion_names(companions)}

def companions(catalog: T, name: String) -> Maybe<&2, List<&2, String>>:
  scheme_companions(lookup(catalog, name))

def scheme_constructors(scheme: Maybe<&2, Scheme>) -> Maybe<&2, List<&2, String>>:
  match scheme:
    case None{}:
      None{}
    case Some{Scheme{arity, family, ctors, companions, members}}:
      Some{List.append(&2, String, ctor_names(ctors), companion_ctors(companions))}

def constructors(catalog: T, name: String) -> Maybe<&2, List<&2, String>>:
  scheme_constructors(lookup(catalog, name))

def rename_instance(scheme: Scheme, +name: String, +as_name: String, +value: String) -> String:
  match scheme:
    case Scheme{arity, family, ctors, companions, members}:
      Bool.pick(String, String.eq(value, name), as_name,
        Bool.pick(String, String.eq(value, family_name(family)) || Poly.member(companion_names(companions), value) || Poly.member(member_names(members), value),
          as_name ++ "_" ++ value, value))

def mapping_find(mapping: List<&2, Common.Pair<String, String>>, +name: String) -> Maybe<&2, String>:
  match mapping:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, value}, tail}:
      Bool.pick(Maybe<&2, String>, String.eq(key, name), Some{value}, mapping_find(tail, name))

def mapped_name(value: Maybe<&2, String>, fallback: String) -> String:
  match value:
    case None{}:
      fallback
    case Some{value}:
      value

def rename_export(+scheme: Scheme, +name: String, +as_name: String, exports: List<&2, Common.Pair<String, String>>, +value: String) -> String:
  mapped_name(mapping_find(exports, value), rename_instance(scheme, name, as_name, value))

def planned_families(families: List<&2, String>, +scheme: Scheme, +name: String, +as_name: String, +reuse: List<&2, Common.Pair<String, String>>) -> List<&2, String>:
  match families:
    case Nil{}:
      []
    case Con{+family, tail}:
      mapped_name(mapping_find(reuse, family), rename_instance(scheme, name, as_name, family)) <> planned_families(tail, scheme, name, as_name, reuse)

def collision(name: String) -> Error.T:
  Error.Mismatch{"the name " ++ name ++ " is already declared"}

def validate_export_binding(+source: String, +target: String, +names: List<&2, String>, sources: List<&2, String>, reserved: List<&2, String>, targets: List<&2, String>) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, Bool.not(Poly.member(names, source)) || Poly.member(sources, source),
    Fail{Error.Mismatch{"member exports must name each member once"}},
    Bool.pick(Result<&2, &2, Error.T, Unit>, Poly.member(reserved, target), Fail{collision(target)},
      Bool.pick(Result<&2, &2, Error.T, Unit>, Poly.member(targets, target),
        Fail{Error.Mismatch{"member exports repeat the target " ++ target}}, Done{Unit{}})))

def export_loop(bindings: List<&2, Common.Pair<String, String>>, +names: List<&2, String>, +reserved: List<&2, String>, +sources: List<&2, String>, +targets: List<&2, String>) -> Budget.Comp(Unit):
  match bindings:
    case Nil{}:
      Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, Nat.is_eq(List.length(&2, String, sources), List.length(&2, String, names)), Done{Unit{}},
        Fail{Error.Mismatch{"every family member needs an export name"}}))
    case Con{Common.Pair{+source, +target}, tail}:
      do Budget.Comp<Unit>:
        tick : Unit <- Budget.tick()
        checked : Unit <- Budget.lift(Unit, validate_export_binding(source, target, names, sources, reserved, targets))
        export_loop(tail, names, reserved, source <> sources, target <> targets)

def check_exports(scheme: Scheme, name: String, +as_name: String, reuse: List<&2, Common.Pair<String, String>>, exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> Budget.Comp(List<&2, Common.Pair<String, String>>):
  match scheme exports:
    case scheme None{}:
      Budget.pure(List<&2, Common.Pair<String, String>>, [])
    case Scheme{+arity, +family, +ctors, +companions, +members} Some{+bindings}:
      do Budget.Comp<List<&2, Common.Pair<String, String>>>:
        families : List<&2, String> = planned_families(family_name(family) <> companion_names(companions), Scheme{arity, family, ctors, companions, members}, name, as_name, reuse)
        labels : List<&2, String> = List.append(&2, String, ctor_names(ctors), companion_ctors(companions))
        checked : Unit <- export_loop(bindings, member_names(members), as_name <> List.append(&2, String, families, labels), [], [])
        return bindings

def keep_fresh(reused: Maybe<&2, String>, name: String, tail: List<&2, String>) -> List<&2, String>:
  match reused:
    case Some{existing}:
      tail
    case None{}:
      name <> tail

def fresh_family_names(families: List<&2, String>, +scheme: Scheme, +name: String, +as_name: String, +reuse: List<&2, Common.Pair<String, String>>, +exports: List<&2, Common.Pair<String, String>>) -> List<&2, String>:
  match families:
    case Nil{}:
      []
    case Con{+family, tail}:
      keep_fresh(mapping_find(reuse, family), rename_export(scheme, name, as_name, exports, family),
        fresh_family_names(tail, scheme, name, as_name, reuse, exports))

def exported_member_names(names: List<&2, String>, +scheme: Scheme, +name: String, +as_name: String, +exports: List<&2, Common.Pair<String, String>>) -> List<&2, String>:
  match names:
    case Nil{}:
      []
    case Con{member, tail}:
      rename_export(scheme, name, as_name, exports, member) <> exported_member_names(tail, scheme, name, as_name, exports)

def instance_names_result(result: Result<&2, &2, Error.T, List<&2, Common.Pair<String, String>>>, scheme: Scheme, +name: String, +as_name: String, reuse: List<&2, Common.Pair<String, String>>) -> Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>:
  match result scheme:
    case Fail{error} scheme:
      None{}
    case Done{+exports} Scheme{+arity, +family, +ctors, +companions, +members}:
      +scheme = {Scheme{arity, family, ctors, companions, members} : Scheme}
      Some{Common.Pair{
        fresh_family_names(family_name(family) <> companion_names(companions), scheme, name, as_name, reuse, exports),
        exported_member_names(member_names(members), scheme, name, as_name, exports)}}

def instance_names_scheme(scheme: Maybe<&2, Scheme>, +name: String, +as_name: String, +reuse: List<&2, Common.Pair<String, String>>, exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>:
  match scheme:
    case None{}:
      None{}
    case Some{+scheme}:
      instance_names_result(Budget.run(List<&2, Common.Pair<String, String>>, check_exports(scheme, name, as_name, reuse, exports), Budget.unlimited()), scheme, name, as_name, reuse)

def instance_names(catalog: T, +name: String, as_name: String, reuse: List<&2, Common.Pair<String, String>>, exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> Maybe<&2, Common.Pair<List<&2, String>, List<&2, String>>>:
  instance_names_scheme(lookup(catalog, name), name, as_name, reuse, exports)


def RawFamily() -> Data:
  Common.Pair<Check.FamilyDecl, List<&2, Check.CtorDecl>>

def Bindings() -> Data:
  List<&2, Common.Pair<String, String>>

type Imported is Data:
  Imported{reused: Bool, family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>}

type Dependency is Data:
  Dependency{source: String, levels: List<&2, Level.T>, alias: String, reuse: Bindings, exports: Maybe<&2, Bindings>}

def MemberElaborator() -> Type:
  Global.T -> Budget.Comp(Check.Decl)

def catalog_names(catalog: T) -> List<&2, String>:
  match catalog:
    case Nil{}:
      []
    case Con{Common.Pair{name, scheme}, tail}:
      name <> catalog_names(tail)

def has_ctor(families: List<&2, Common.Pair<String, Positivity.Family>>, +name: String) -> Bool:
  match families:
    case Nil{}:
      False{}
    case Con{Common.Pair{key, Positivity.Family{family, params, indices, level, status, ctors, positive}}, tail}:
      Poly.is_some(Positivity.Ctor, Positivity.find_ctor(ctors, name)) || has_ctor(tail, name)

def ctor_declared(globals: Global.T, name: String) -> Bool:
  match globals:
    case Global.Globals{entries, families, entry_index, family_index}:
      has_ctor(families, name)

def occupied(+globals: Global.T, catalog: T, +name: String) -> Bool:
  Poly.is_some(Global.Entry, Global.find(name, globals)) || Poly.is_some(Positivity.Family, Global.find_family(name, globals)) ||
  ctor_declared(globals, name) || Poly.is_some(Scheme, lookup(catalog, name))

def require_fresh(used: Bool, name: String) -> Budget.Comp(Unit):
  Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, used, Fail{collision(name)}, Done{Unit{}}))

def map_members(members: List<&2, Check.Decl>, +action: Poly.Action) -> Budget.Comp(List<&2, Check.Decl>):
  match members:
    case Nil{}:
      Budget.pure(List<&2, Check.Decl>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Check.Decl>>:
        head : Check.Decl <- map_member(head, action)
        tail : List<&2, Check.Decl> <- map_members(tail, action)
        return head <> tail

def check_members(members: List<&2, Check.Decl>, +catalog: T, +arity: Nat, +globals: Global.T) -> Budget.Comp(Global.T):
  match members:
    case Nil{}:
      Budget.pure(Global.T, globals)
    case Con{+decl, tail}:
      do Budget.Comp<Global.T>:
        tick : Unit <- Budget.tick()
        +name : String = Poly.decl_name(decl)
        checked : Unit <- require_fresh(occupied(globals, catalog, name), name)
        entry : Global.Entry <- Check.check_decl_comp(globals, arity, decl)
        check_members(tail, catalog, arity, Global.add(name, entry, globals))

def reserve_members(members: List<&2, Check.Decl>, +catalog: T) -> Budget.Comp(Unit):
  match members:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{Check.Decl{+name, kind, ty, body}, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- require_fresh(Poly.is_some(Scheme, lookup(catalog, name)), name)
        reserve_members(tail, catalog)

def family_members(members: List<&2, Check.Decl>, globals: Global.T, catalog: T, +arity: Nat, +family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>) -> Budget.Comp(Unit):
  match members:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{head, tail}:
      do Budget.Comp<Unit>:
        provisional : Global.T <- Check.declare_family_comp(globals, arity, family)
        symbolic : Global.T <- Check.define_ctors_comp(provisional, arity, [family_name(family)], family_name(family), ctors)
        checked : Global.T <- check_members(head <> tail, catalog, arity, symbolic)
        return Unit{}

def declared_family(raw: RawFamily, +globals: Global.T, +catalog: T, +arity: Nat, +members: List<&2, Check.Decl>, action: Poly.Action) -> Budget.Comp(T):
  match raw:
    case Common.Pair{+family, +ctors}:
      do Budget.Comp<T>:
        checked : Unit <- Check.check_family_scheme_comp(globals, arity, family, ctors)
        reserved : Unit <- reserve_members(members, catalog)
        +members : List<&2, Check.Decl> <- map_members(members, action)
        checked : Unit <- family_members(members, globals, catalog, arity, family, ctors)
        return Common.Pair{family_name(family), Scheme{arity, family, ctors, [], members}} <> catalog

def declare_comp(+globals: Global.T, +catalog: T, +arity: Nat, +family: Check.FamilyDecl, +ctors: List<&2, Check.CtorDecl>, members: List<&2, Check.Decl>) -> Budget.Comp(T):
  do Budget.Comp<T>:
    checked : Unit <- require_fresh(occupied(globals, catalog, family_name(family)), family_name(family))
    checked : Unit <- unique_ctors(ctors, family_name(family), [])
    +action : Poly.Action = Poly.Action{Poly.ScopeLevels{arity}, [], catalog_names(catalog)}
    raw : RawFamily <- map_family(family, ctors, action)
    declared_family(raw, globals, catalog, arity, members, action)

def declare(globals: Global.T, catalog: T, budget: Budget.T, arity: Nat, family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>, members: List<&2, Check.Decl>) -> Result<&2, &2, Error.T, T>:
  Budget.run(T, declare_comp(globals, catalog, arity, family, ctors, members), budget)

def family_option(value: Maybe<&2, Positivity.Family>, error: Error.T) -> Budget.Comp(Positivity.Family):
  match value:
    case Some{family}:
      Budget.pure(Positivity.Family, family)
    case None{}:
      Budget.failed(Positivity.Family, error)

def drop_family(globals: Global.T, name: String) -> Global.T:
  match globals:
    case Global.Globals{entries, families, entry_index, family_index}:
      Global.without_family(name, Global.Globals{entries, families, entry_index, family_index})

def replace_level(family: Positivity.Family, level: Level.T) -> Positivity.Family:
  match family:
    case Positivity.Family{name, params, indices, old, status, ctors, positive}:
      Positivity.Family{name, params, indices, level, status, ctors, positive}

def kernel_level(family: Positivity.Family) -> Level.T:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      level

def erase_rec(ctors: List<&2, Positivity.Ctor>) -> List<&2, Positivity.Ctor>:
  match ctors:
    case Nil{}:
      []
    case Con{Positivity.Ctor{name, args, indices, arity, self_rec}, tail}:
      Positivity.Ctor{name, args, indices, arity, False{}} <> erase_rec(tail)

def erase_self_rec(family: Positivity.Family) -> Positivity.Family:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      Positivity.Family{name, params, indices, level, status, erase_rec(ctors), positive}

def reuse_agrees(+level_ok: Bool, +actual: Positivity.Family, +expected: Positivity.Family, +name: String, globals: Global.T) -> Budget.Comp(Global.T):
  Budget.lift(Global.T, Bool.pick(Result<&2, &2, Error.T, Global.T>, level_ok && Positivity.same_family(actual, expected), Done{globals},
    Bool.pick(Result<&2, &2, Error.T, Global.T>, level_ok && Positivity.same_family(erase_self_rec(actual), erase_self_rec(expected)),
      Fail{Error.Mismatch{"the reused family " ++ name ++ " is a member of a mutual group, which family reuse does not support"}},
      Fail{Error.Mismatch{"the reused family " ++ name ++ " does not match the template"}})))

def check_reuse(+globals: Global.T, +arity: Nat, +family: Check.FamilyDecl, ctors: List<&2, Check.CtorDecl>) -> Budget.Comp(Global.T):
  do Budget.Comp<Global.T>:
    +name : String = family_name(family)
    +actual : Positivity.Family <- family_option(Global.find_family(name, globals), Error.Unbound{"unknown reused family " ++ name})
    provisional : Global.T <- Check.declare_family_comp(drop_family(globals, name), arity, family)
    checked : Global.T <- Check.define_ctors_comp(provisional, arity, [name], name, ctors)
    +expected : Positivity.Family <- family_option(Global.find_family(name, checked), Error.Cannot_infer{"reuse rechecking returned no family"})
    tick : Unit <- Budget.tick()
    agrees : Bool <- LevelEq.decide_m(kernel_level(actual), kernel_level(expected), True{})
    reuse_agrees(agrees, actual, replace_level(expected, kernel_level(actual)), name, globals)

def check_binding(+local: String, +existing: String, bound: List<&2, String>, families: List<&2, String>, available: List<&2, String>, globals: Global.T) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, Poly.member(bound, local), Fail{Error.Mismatch{"the family " ++ local ++ " is bound more than once"}},
    Bool.pick(Result<&2, &2, Error.T, Unit>, Bool.not(Poly.member(families, local)), Fail{Error.Unbound{"unknown template family " ++ local}},
      Bool.pick(Result<&2, &2, Error.T, Unit>, Poly.member(available, existing) || Poly.is_some(Positivity.Family, Global.find_family(existing, globals)), Done{Unit{}}, Fail{Error.Unbound{"unknown reused family " ++ existing}})))

def check_bindings(reuse: Bindings, +families: List<&2, String>, +available: List<&2, String>, +globals: Global.T, +bound: List<&2, String>) -> Budget.Comp(Unit):
  match reuse:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{Common.Pair{+local, existing}, tail}:
      do Budget.Comp<Unit>:
        tick : Unit <- Budget.tick()
        checked : Unit <- Budget.lift(Unit, check_binding(local, existing, bound, families, available, globals))
        check_bindings(tail, families, available, globals, local <> bound)

def raw_families(scheme: Scheme) -> List<&2, RawFamily>:
  match scheme:
    case Scheme{arity, family, ctors, companions, members}:
      Common.Pair{family, ctors} <> companions

def schema_members(scheme: Scheme) -> List<&2, Check.Decl>:
  match scheme:
    case Scheme{arity, family, ctors, companions, members}:
      members

def schema_arity(scheme: Scheme) -> Nat:
  match scheme:
    case Scheme{arity, family, ctors, companions, members}:
      arity

def require_schema(scheme: Maybe<&2, Scheme>, name: String) -> Budget.Comp(Scheme):
  match scheme:
    case None{}:
      Budget.failed(Scheme, Error.Unbound{"unknown family universe schema " ++ name})
    case Some{scheme}:
      Budget.pure(Scheme, scheme)

def validate_exports_comp(globals: Global.T, available: List<&2, String>, catalog: T, +name: String, as_name: String, +reuse: Bindings, exports: Bindings) -> Budget.Comp(Unit):
  do Budget.Comp<Unit>:
    tick : Unit <- Budget.tick()
    +scheme : Scheme <- require_schema(lookup(catalog, name), name)
    checked : Unit <- check_bindings(reuse, companion_names(raw_families(scheme)), available, globals, [])
    bindings : Bindings <- check_exports(scheme, name, as_name, reuse, Some{exports})
    return Unit{}

def validate_exports(globals: Global.T, available: List<&2, String>, catalog: T, budget: Budget.T, name: String, as_name: String, reuse: Bindings, exports: Bindings) -> Result<&2, &2, Error.T, Unit>:
  Budget.run(Unit, validate_exports_comp(globals, available, catalog, name, as_name, reuse, exports), budget)

def fresh_imports(imports: List<&2, Imported>) -> List<&2, RawFamily>:
  match imports:
    case Nil{}:
      []
    case Con{Imported{reused, family, ctors}, tail}:
      +tail = fresh_imports(tail)
      Bool.pick(List<&2, RawFamily>, reused, tail, Common.Pair{family, ctors} <> tail)

def install_symbolic_import(imported: Imported, +globals: Global.T, +catalog: T, +arity: Nat) -> Budget.Comp(Global.T):
  match imported:
    case Imported{True{}, family, ctors}:
      check_reuse(globals, arity, family, ctors)
    case Imported{False{}, +family, +ctors}:
      do Budget.Comp<Global.T>:
        checked : T <- declare_comp(globals, catalog, arity, family, ctors, [])
        provisional : Global.T <- Check.declare_family_comp(globals, arity, family)
        Check.define_ctors_comp(provisional, arity, [family_name(family)], family_name(family), ctors)

def install_symbolic(imports: List<&2, Imported>, globals: Global.T, +catalog: T, +arity: Nat) -> Budget.Comp(Global.T):
  match imports:
    case Nil{}:
      Budget.pure(Global.T, globals)
    case Con{head, tail}:
      do Budget.Comp<Global.T>:
        tick : Unit <- Budget.tick()
        globals : Global.T <- install_symbolic_import(head, globals, catalog, arity)
        install_symbolic(tail, globals, catalog, arity)

def elaborate_members(members: List<&1, MemberElaborator>, +globals: Global.T, +catalog: T, +arity: Nat, reversed: List<&2, Check.Decl>) -> Budget.Comp(List<&2, Check.Decl>):
  match members:
    case Nil{}:
      Budget.pure(List<&2, Check.Decl>, List.reverse(&2, Check.Decl, reversed))
    case Con{elaborate, tail}:
      do Budget.Comp<List<&2, Check.Decl>>:
        tick : Unit <- Budget.tick()
        member : Check.Decl <- elaborate(globals)
        +member : Check.Decl <- map_member(member, Poly.Action{Poly.ScopeLevels{arity}, [], catalog_names(catalog)})
        globals : Global.T <- check_members([member], catalog, arity, globals)
        elaborate_members(tail, globals, catalog, arity, member <> reversed)

def declare_nonempty(families: List<&2, RawFamily>, imports: List<&2, Imported>, members: List<&1, MemberElaborator>, globals: Global.T, +catalog: T, +arity: Nat) -> Budget.Comp(T):
  match families:
    case Nil{}:
      Budget.failed(T, Error.Mismatch{"a family schema group must be nonempty"})
    case Con{Common.Pair{+family, ctors}, companions}:
      do Budget.Comp<T>:
        symbolic : Global.T <- install_symbolic(imports, globals, catalog, arity)
        members : List<&2, Check.Decl> <- elaborate_members(members, symbolic, catalog, arity, [])
        return Common.Pair{family_name(family), Scheme{arity, family, ctors, companions, members}} <> catalog

def declare_group_with_reuse(globals: Global.T, catalog: T, arity: Nat, +imports: List<&2, Imported>, members: List<&1, MemberElaborator>) -> Budget.Comp(T):
  declare_nonempty(fresh_imports(imports), imports, members, globals, catalog, arity)

def fresh_raw(families: List<&2, RawFamily>) -> List<&2, Imported>:
  match families:
    case Nil{}:
      []
    case Con{Common.Pair{family, ctors}, tail}:
      Imported{False{}, family, ctors} <> fresh_raw(tail)

def raw_callbacks(members: List<&2, Check.Decl>) -> List<&1, MemberElaborator>:
  match members:
    case Nil{}:
      []
    case Con{head, tail}:
      (globals => Budget.pure(Check.Decl, head)) <> raw_callbacks(tail)

def declare_group_elaborated_comp(globals: Global.T, catalog: T, arity: Nat, families: List<&2, RawFamily>, members: List<&1, MemberElaborator>) -> Budget.Comp(T):
  declare_group_with_reuse(globals, catalog, arity, fresh_raw(families), members)

def declare_group_elaborated(globals: Global.T, catalog: T, budget: Budget.T, arity: Nat, families: List<&2, RawFamily>, members: List<&1, MemberElaborator>) -> Result<&2, &2, Error.T, T>:
  Budget.run(T, declare_group_elaborated_comp(globals, catalog, arity, families, members), budget)

def declare_group(globals: Global.T, catalog: T, budget: Budget.T, arity: Nat, families: List<&2, RawFamily>, members: List<&2, Check.Decl>) -> Result<&2, &2, Error.T, T>:
  declare_group_elaborated(globals, catalog, budget, arity, families, raw_callbacks(members))

def rename_bindings(names: List<&2, String>, +scheme: Scheme, +source: String, +alias: String, +reuse: Bindings, +exports: Bindings) -> Bindings:
  match names:
    case Nil{}:
      []
    case Con{+name, tail}:
      Common.Pair{name, mapped_name(mapping_find(reuse, name), rename_export(scheme, source, alias, exports, name))} <> rename_bindings(tail, scheme, source, alias, reuse, exports)

def instance_action(+scheme: Scheme, +source: String, alias: String, levels: List<&2, Level.T>, reuse: Bindings, exports: Bindings) -> Poly.Action:
  +names = List.append(&2, String, companion_names(raw_families(scheme)), member_names(schema_members(scheme)))
  Poly.Action{Poly.SubstituteLevels{levels}, rename_bindings(source <> names, scheme, source, alias, reuse, exports), []}

def mapped_import(raw: RawFamily, reused: Bool) -> Imported:
  match raw:
    case Common.Pair{family, ctors}:
      Imported{reused, family, ctors}

def map_imports(families: List<&2, RawFamily>, +action: Poly.Action, +reuse: Bindings) -> Budget.Comp(List<&2, Imported>):
  match families:
    case Nil{}:
      Budget.pure(List<&2, Imported>, [])
    case Con{Common.Pair{+family, ctors}, tail}:
      do Budget.Comp<List<&2, Imported>>:
        raw : RawFamily <- map_family(family, ctors, action)
        tail : List<&2, Imported> <- map_imports(tail, action, reuse)
        return mapped_import(raw, Poly.is_some(String, mapping_find(reuse, family_name(family)))) <> tail

def scopes(+arity: Nat, levels: List<&2, Level.T>) -> Bool:
  match levels:
    case Nil{}:
      True{}
    case Con{head, tail}:
      Level.in_scope(arity, head) && scopes(arity, tail)

def level_count_error(source: String, want: Nat, got: Nat) -> Error.T:
  Error.Universe{"the family schema " ++ source ++ " expects " ++ Nat.show(want) ++ " universe arguments, got " ++ Nat.show(got)}

def check_levels(+levels: List<&2, Level.T>, +want: Nat, source: String, arity: Nat, closed: Bool) -> Budget.Comp(Unit):
  Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, Bool.not(Nat.is_eq(want, List.length(&2, Level.T, levels))),
    Fail{level_count_error(source, want, List.length(&2, Level.T, levels))},
    Bool.pick(Result<&2, &2, Error.T, Unit>, scopes(arity, levels), Done{Unit{}},
      Fail{Error.Universe{Bool.pick(String, closed, "universe arguments must be closed", "universe level is outside the global parameter scope")}})))

type Composed is Data:
  Composed{aliases: List<&2, String>, available: List<&2, String>, imports: List<&2, Imported>, members: List<&2, Check.Decl>}

def collision_list(names: List<&2, String>, +occupied: List<&2, String>) -> Budget.Comp(Unit):
  match names:
    case Nil{}:
      Budget.pure(Unit, Unit{})
    case Con{+name, tail}:
      do Budget.Comp<Unit>:
        checked : Unit <- require_fresh(Poly.member(occupied, name), name)
        collision_list(tail, occupied)

def compose_dependency(dependency: Dependency, state: Composed, +globals: Global.T, +catalog: T, arity: Nat, +root: String) -> Budget.Comp(Composed):
  match dependency state:
    case Dependency{+source, +levels, +alias, +reuse, exports} Composed{+aliases, +available, imports, members}:
      do Budget.Comp<Composed>:
        tick : Unit <- Budget.tick()
        +scheme : Scheme <- require_schema(lookup(catalog, source), source)
        checked : Unit <- require_fresh(String.eq(alias, root) || Poly.member(aliases, alias) || occupied(globals, catalog, alias), alias)
        checked : Unit <- check_levels(levels, schema_arity(scheme), source, arity, False{})
        checked : Unit <- check_bindings(reuse, companion_names(raw_families(scheme)), available, globals, [])
        exports : Bindings <- check_exports(scheme, source, alias, reuse, exports)
        +action : Poly.Action = instance_action(scheme, source, alias, levels, reuse, exports)
        +families : List<&2, Imported> <- map_imports(raw_families(scheme), action, reuse)
        +definitions : List<&2, Check.Decl> <- map_members(schema_members(scheme), action)
        +fresh : List<&2, String> = companion_names(fresh_imports(families))
        +generated : List<&2, String> = List.append(&2, String, fresh, member_names(definitions))
        checked : Unit <- collision_list(generated, aliases)
        return Composed{alias <> List.append(&2, String, generated, aliases), List.append(&2, String, fresh, available),
          List.append(&2, Imported, imports, families), List.append(&2, Check.Decl, members, definitions)}

def compose_dependencies(dependencies: List<&2, Dependency>, state: Composed, +globals: Global.T, +catalog: T, +arity: Nat, +root: String) -> Budget.Comp(Composed):
  match dependencies:
    case Nil{}:
      Budget.pure(Composed, state)
    case Con{head, tail}:
      do Budget.Comp<Composed>:
        state : Composed <- compose_dependency(head, state, globals, catalog, arity, root)
        compose_dependencies(tail, state, globals, catalog, arity, root)

def check_root_member(+decl: Check.Decl, +root: String) -> Budget.Comp(Check.Decl):
  do Budget.Comp<Check.Decl>:
    checked : Unit <- require_fresh(String.eq(Poly.decl_name(decl), root), root)
    return decl

def guarded_callback(elaborate: MemberElaborator, +root: String) -> MemberElaborator:
  symbolic => do Budget.Comp<Check.Decl>:
    declaration : Check.Decl <- elaborate(symbolic)
    check_root_member(declaration, root)

def guard_callbacks(members: List<&1, MemberElaborator>, +root: String) -> List<&1, MemberElaborator>:
  match members:
    case Nil{}:
      []
    case Con{head, tail}:
      guarded_callback(head, root) <> guard_callbacks(tail, root)

def composition_schema(scheme: Maybe<&2, Scheme>, name: String, catalog: T) -> Budget.Comp(T):
  match scheme:
    case None{}:
      Budget.failed(T, Error.Cannot_infer{"composition returned no schema"})
    case Some{scheme}:
      Budget.pure(T, Common.Pair{name, scheme} <> catalog)

def finish_composition(fresh: List<&2, RawFamily>, imports: List<&2, Imported>, members: List<&1, MemberElaborator>, globals: Global.T, +catalog: T, arity: Nat, name: String) -> Budget.Comp(T):
  match fresh:
    case Nil{}:
      Budget.failed(T, Error.Mismatch{"a family schema group must be nonempty"})
    case Con{Common.Pair{family, ctors}, tail}:
      do Budget.Comp<T>:
        checked : T <- declare_group_with_reuse(globals, catalog, arity, imports, members)
        composition_schema(lookup(checked, family_name(family)), name, catalog)

def import_families(imports: List<&2, Imported>) -> List<&2, RawFamily>:
  match imports:
    case Nil{}:
      []
    case Con{Imported{reused, family, ctors}, tail}:
      Common.Pair{family, ctors} <> import_families(tail)

def composed_group(state: Composed, supplied: List<&1, MemberElaborator>, globals: Global.T, catalog: T, arity: Nat, +root: String) -> Budget.Comp(T):
  match state:
    case Composed{aliases, available, +imports, members}:
      do Budget.Comp<T>:
        checked : Unit <- collision_list(companion_names(import_families(imports)), [root])
        members : List<&1, MemberElaborator> = List.append(&1, MemberElaborator, raw_callbacks(members), supplied)
        finish_composition(fresh_imports(imports), imports, guard_callbacks(members, root), globals, catalog, arity, root)

def compose_comp(+globals: Global.T, +catalog: T, +arity: Nat, +name: String, dependencies: List<&2, Dependency>, members: List<&1, MemberElaborator>) -> Budget.Comp(T):
  do Budget.Comp<T>:
    tick : Unit <- Budget.tick()
    checked : Unit <- require_fresh(occupied(globals, catalog, name), name)
    state : Composed <- compose_dependencies(dependencies, Composed{[], [], [], []}, globals, catalog, arity, name)
    composed_group(state, members, globals, catalog, arity, name)

def compose(globals: Global.T, catalog: T, budget: Budget.T, arity: Nat, name: String, dependencies: List<&2, Dependency>, members: List<&1, MemberElaborator>) -> Result<&2, &2, Error.T, T>:
  Budget.run(T, compose_comp(globals, catalog, arity, name, dependencies, members), budget)

def normalize_from(remaining: Nat, +candidate: Level.T, +level: Level.T) -> Level.T:
  match remaining:
    case 0n:
      level
    case 1n+rest:
      Bool.pick(Level.T, Level.equal(candidate, level), candidate, normalize_from(rest, Level.succ(candidate), level))

def normal_level(level: Level.T) -> Level.T:
  normalize_from(64n, Level.zero(), level)

def normal_levels(levels: List<&2, Level.T>) -> List<&2, Level.T>:
  match levels:
    case Nil{}:
      []
    case Con{head, tail}:
      normal_level(head) <> normal_levels(tail)

def install_import(imported: Imported, +globals: Global.T, +catalog: T) -> Budget.Comp(Global.T):
  match imported:
    case Imported{True{}, family, ctors}:
      check_reuse(globals, 0n, family, ctors)
    case Imported{False{}, +family, ctors}:
      do Budget.Comp<Global.T>:
        checked : Unit <- require_fresh(occupied(globals, catalog, family_name(family)), family_name(family))
        provisional : Global.T <- Check.declare_family_comp(globals, 0n, family)
        Check.define_ctors_comp(provisional, 0n, [family_name(family)], family_name(family), ctors)

def install_imports(imports: List<&2, Imported>, globals: Global.T, +catalog: T) -> Budget.Comp(Global.T):
  match imports:
    case Nil{}:
      Budget.pure(Global.T, globals)
    case Con{head, tail}:
      do Budget.Comp<Global.T>:
        tick : Unit <- Budget.tick()
        globals : Global.T <- install_import(head, globals, catalog)
        install_imports(tail, globals, catalog)

def instantiate_comp(+globals: Global.T, +catalog: T, +name: String, +levels: List<&2, Level.T>, +as_name: String, +reuse: Bindings, exports: Maybe<&2, Bindings>) -> Budget.Comp(Global.T):
  do Budget.Comp<Global.T>:
    +scheme : Scheme <- require_schema(lookup(catalog, name), name)
    checked : Unit <- require_fresh(occupied(globals, catalog, as_name), as_name)
    checked : Unit <- check_levels(levels, schema_arity(scheme), name, 0n, True{})
    tick : Unit <- Budget.tick()
    checked : Unit <- check_bindings(reuse, companion_names(raw_families(scheme)), [], globals, [])
    exports : Bindings <- check_exports(scheme, name, as_name, reuse, exports)
    +action : Poly.Action = instance_action(scheme, name, as_name, normal_levels(levels), reuse, exports)
    families : List<&2, Imported> <- map_imports(raw_families(scheme), action, reuse)
    members : List<&2, Check.Decl> <- map_members(schema_members(scheme), action)
    installed : Global.T <- install_imports(families, globals, catalog)
    check_members(members, catalog, 0n, installed)

def instantiate(globals: Global.T, catalog: T, budget: Budget.T, name: String, levels: List<&2, Level.T>, as_name: String, reuse: Bindings, exports: Maybe<&2, Bindings>) -> Result<&2, &2, Error.T, Global.T>:
  Budget.run(Global.T, instantiate_comp(globals, catalog, name, levels, as_name, reuse, exports), budget)
