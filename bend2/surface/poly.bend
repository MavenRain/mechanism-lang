import Base
import ../kernel/check.bend as Check
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as Positivity
import ../kernel/common.bend as Common
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/level.bend as Level
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

# Defunctionalized level/name callbacks preserve reusable traversal state.
type LevelAction is Data:
  PreserveLevels{}
  ScopeLevels{arity: Nat}
  SubstituteLevels{levels: List<&2, Level.T>}

type Action is Data:
  Action{levels: LevelAction, names: List<&2, Common.Pair<String, String>>, forbidden: List<&2, String>}

def substitution(value: Maybe<&2, Level.T>) -> Result<&2, &2, Error.T, Level.T>:
  match value:
    case Some{level}:
      Done{level}
    case None{}:
      Fail{Error.Universe{"universe level is outside the global parameter scope"}}

def level_action(action: LevelAction, +level: Level.T) -> Result<&2, &2, Error.T, Level.T>:
  match action:
    case PreserveLevels{}:
      Done{level}
    case ScopeLevels{arity}:
      Bool.pick(Result<&2, &2, Error.T, Level.T>, Level.in_scope(arity, level), Done{level},
        Fail{Error.Universe{"universe level is outside the global parameter scope"}})
    case SubstituteLevels{levels}:
      substitution(Level.subst(levels, level))

def map_level(action: Action, level: Level.T) -> Result<&2, &2, Error.T, Level.T>:
  match action:
    case Action{levels, names, forbidden}:
      level_action(levels, level)

def member(names: List<&2, String>, +name: String) -> Bool:
  match names:
    case Nil{}:
      False{}
    case Con{head, tail}:
      String.eq(head, name) || member(tail, name)

def find_name(names: List<&2, Common.Pair<String, String>>, +name: String) -> String:
  match names:
    case Nil{}:
      name
    case Con{Common.Pair{source, target}, tail}:
      Bool.pick(String, String.eq(source, name), target, find_name(tail, name))

def map_name(action: Action, +name: String) -> Result<&2, &2, Error.T, String>:
  match action:
    case Action{levels, names, forbidden}:
      Bool.pick(Result<&2, &2, Error.T, String>, member(forbidden, name),
        Fail{Error.Not_yet{"references between family schemas are not supported"}}, Done{find_name(names, name)})

law map_term:
  for +action: Action
  for term: Term.T
  Budget.Comp(Term.T)

@unsafe
def map_terms(terms: List<&2, Term.T>, +action: Action) -> Budget.Comp(List<&2, Term.T>):
  match terms:
    case Nil{}:
      Budget.pure(List<&2, Term.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Term.T>>:
        head : Term.T <- map_term(action, head)
        tail : List<&2, Term.T> <- map_terms(tail, action)
        return head <> tail

@unsafe
def shape_body(shape: Shape.T<Term.T>, +action: Action) -> Budget.Comp(Shape.T<Term.T>):
  match shape:
    case Shape.SPi{q, name, domain}:
      do Budget.Comp<Shape.T<Term.T>>:
        domain : Term.T <- map_term(action, domain)
        return Shape.SPi{q, name, domain}
    case Shape.SZk{q, name, domain}:
      do Budget.Comp<Shape.T<Term.T>>:
        domain : Term.T <- map_term(action, domain)
        return Shape.SZk{q, name, domain}
    case Shape.SArg{q, name}:
      do Budget.Comp<Shape.T<Term.T>>:
        return Shape.SArg{q, name}
    case Shape.SColl{count}:
      do Budget.Comp<Shape.T<Term.T>>:
        return Shape.SColl{count}
    case Shape.SPar{left, right}:
      do Budget.Comp<Shape.T<Term.T>>:
        left : Term.T <- map_term(action, left)
        right : Term.T <- map_term(action, right)
        return Shape.SPar{left, right}
    case Shape.SMu{name, args}:
      do Budget.Comp<Shape.T<Term.T>>:
        name : String <- Budget.lift(String, map_name(action, name))
        args : List<&2, Term.T> <- map_terms(args, action)
        return Shape.SMu{name, args}
    case Shape.SNu{name, args}:
      do Budget.Comp<Shape.T<Term.T>>:
        name : String <- Budget.lift(String, map_name(action, name))
        args : List<&2, Term.T> <- map_terms(args, action)
        return Shape.SNu{name, args}
    case Shape.SFhc{level}:
      do Budget.Comp<Shape.T<Term.T>>:
        level : Term.T <- map_term(action, level)
        return Shape.SFhc{level}
    case Shape.SMpc{parties, access}:
      do Budget.Comp<Shape.T<Term.T>>:
        parties : Term.T <- map_term(action, parties)
        access : Term.T <- map_term(action, access)
        return Shape.SMpc{parties, access}

def map_shape(shape: Shape.T<Term.T>, action: Action) -> Budget.Comp(Shape.T<Term.T>):
  do Budget.Comp<Shape.T<Term.T>>:
    tick : Unit <- Budget.tick()
    shape_body(shape, action)

@unsafe
def addr_body(addr: Term.Addr<Term.T>, action: Action) -> Budget.Comp(Term.Addr<Term.T>):
  match addr:
    case Term.APt{quantity, arg}:
      do Budget.Comp<Term.Addr<Term.T>>:
        arg : Term.T <- map_term(action, arg)
        return Term.APt{quantity, arg}
    case other:
      Budget.pure(Term.Addr<Term.T>, other)

def map_addr(addr: Term.Addr<Term.T>, action: Action) -> Budget.Comp(Term.Addr<Term.T>):
  do Budget.Comp<Term.Addr<Term.T>>:
    tick : Unit <- Budget.tick()
    addr_body(addr, action)

@unsafe
def map_leg(leg: Term.Leg<Term.T>, action: Action) -> Budget.Comp(Term.Leg<Term.T>):
  match leg:
    case Term.Leg{binders, body}:
      do Budget.Comp<Term.Leg<Term.T>>:
        tick : Unit <- Budget.tick()
        body : Term.T <- map_term(action, body)
        return Term.Leg{binders, body}

@unsafe
def map_legs(legs: List<&2, Term.Leg<Term.T>>, +action: Action) -> Budget.Comp(List<&2, Term.Leg<Term.T>>):
  match legs:
    case Nil{}:
      Budget.pure(List<&2, Term.Leg<Term.T>>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Term.Leg<Term.T>>>:
        head : Term.Leg<Term.T> <- map_leg(head, action)
        tail : List<&2, Term.Leg<Term.T>> <- map_legs(tail, action)
        return head <> tail

def map_optional_name(name: Maybe<&2, String>, action: Action) -> Result<&2, &2, Error.T, Maybe<&2, String>>:
  match name:
    case None{}:
      Done{None{}}
    case Some{name}:
      do Result<&2, &2, Error.T, Maybe<&2, String>>:
        name : String <- map_name(action, name)
        return Some{name}

@unsafe
def map_motive(motive: Maybe<&2, Term.Motive<Term.T>>, +action: Action) -> Budget.Comp(Maybe<&2, Term.Motive<Term.T>>):
  match motive:
    case None{}:
      Budget.pure(Maybe<&2, Term.Motive<Term.T>>, None{})
    case Some{Term.Motive{ind, indices, self, body}}:
      do Budget.Comp<Maybe<&2, Term.Motive<Term.T>>>:
        ind : Maybe<&2, String> <- Budget.lift(Maybe<&2, String>, map_optional_name(ind, action))
        body : Term.T <- map_term(action, body)
        return Some{Term.Motive{ind, indices, self, body}}

@unsafe
def map_branches(branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +action: Action) -> Budget.Comp(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>):
  match branches:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, [])
    case Con{Common.Pair{addr, leg}, tail}:
      do Budget.Comp<List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>>:
        addr : Term.Addr<Term.T> <- map_addr(addr, action)
        leg : Term.Leg<Term.T> <- map_leg(leg, action)
        tail : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- map_branches(tail, action)
        return Common.Pair{addr, leg} <> tail

@unsafe
def term_body(term: Term.T, +action: Action) -> Budget.Comp(Term.T):
  match term:
    case Term.Var{index}:
      do Budget.Comp<Term.T>:
        return Term.Var{index}
    case Term.Lit{literal}:
      do Budget.Comp<Term.T>:
        return Term.Lit{literal}
    case Term.Auto{}:
      do Budget.Comp<Term.T>:
        return Term.Auto{}
    case Term.Global{name}:
      do Budget.Comp<Term.T>:
        name : String <- Budget.lift(String, map_name(action, name))
        return Term.Global{name}
    case Term.Univ{level}:
      do Budget.Comp<Term.T>:
        level : Level.T <- Budget.lift(Level.T, map_level(action, level))
        return Term.Univ{level}
    case Term.Lan{shape, body}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        body : Term.T <- map_term(action, body)
        return Term.Lan{shape, body}
    case Term.Ran{shape, body}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        body : Term.T <- map_term(action, body)
        return Term.Ran{shape, body}
    case Term.In{shape, addr, args}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        addr : Term.Addr<Term.T> <- map_addr(addr, action)
        args : List<&2, Term.T> <- map_terms(args, action)
        return Term.In{shape, addr, args}
    case Term.Sec{shape, legs}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        legs : List<&2, Term.Leg<Term.T>> <- map_legs(legs, action)
        return Term.Sec{shape, legs}
    case Term.Out{shape, addr, head}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        addr : Term.Addr<Term.T> <- map_addr(addr, action)
        head : Term.T <- map_term(action, head)
        return Term.Out{shape, addr, head}
    case Term.Elim{Term.ElimData{shape, scrut, q, motive, branches}}:
      do Budget.Comp<Term.T>:
        shape : Shape.T<Term.T> <- map_shape(shape, action)
        scrut : Term.T <- map_term(action, scrut)
        motive : Maybe<&2, Term.Motive<Term.T>> <- map_motive(motive, action)
        branches : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- map_branches(branches, action)
        return Term.Elim{Term.ElimData{shape, scrut, q, motive, branches}}
    case Term.Let{name, ty, value, body}:
      do Budget.Comp<Term.T>:
        ty : Term.T <- map_term(action, ty)
        value : Term.T <- map_term(action, value)
        body : Term.T <- map_term(action, body)
        return Term.Let{name, ty, value, body}
    case Term.Ann{body, ty}:
      do Budget.Comp<Term.T>:
        body : Term.T <- map_term(action, body)
        ty : Term.T <- map_term(action, ty)
        return Term.Ann{body, ty}

@unsafe
def map_term(action, term):
  do Budget.Comp<Term.T>:
    tick : Unit <- Budget.tick()
    term_body(term, action)

def map_levels(action: LevelAction, term: Term.T) -> Result<&2, &2, Error.T, Term.T>:
  Budget.run(Term.T, map_term(Action{action, [], []}, term), Budget.unlimited())

# The level-only declaration adapter uses the same full traversal as families.
def map_optional_term(body: Maybe<&2, Term.T>, action: Action) -> Budget.Comp(Maybe<&2, Term.T>):
  match body:
    case None{}:
      Budget.pure(Maybe<&2, Term.T>, None{})
    case Some{body}:
      do Budget.Comp<Maybe<&2, Term.T>>:
        body : Term.T <- map_term(action, body)
        return Some{body}


type Scheme is Data:
  Scheme{arity: Nat, decl: Check.Decl}

def T() -> Data:
  List<&2, Common.Pair<String, Scheme>>

def empty() -> T:
  []

def is_some(-A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case Some{value}:
      True{}
    case None{}:
      False{}

def lookup(catalog: T, +name: String) -> Maybe<&2, Scheme>:
  match catalog:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, scheme}, tail}:
      Bool.pick(Maybe<&2, Scheme>, String.eq(key, name), Some{scheme}, lookup(tail, name))

def occupied(+globals: Global.T, catalog: T, +name: String) -> Bool:
  is_some(Global.Entry, Global.find(name, globals)) ||
  is_some(Positivity.Family, Global.find_family(name, globals)) || is_some(Scheme, lookup(catalog, name))

def collision(name: String) -> Error.T:
  Error.Mismatch{"the name " ++ name ++ " is already declared"}

def decl_name(decl: Check.Decl) -> String:
  match decl:
    case Check.Decl{name, kind, ty, body}:
      name

def map_decl_action(decl: Check.Decl, +action: Action) -> Budget.Comp(Check.Decl):
  match decl:
    case Check.Decl{name, kind, ty, body}:
      do Budget.Comp<Check.Decl>:
        ty : Term.T <- map_term(action, ty)
        body : Maybe<&2, Term.T> <- map_optional_term(body, action)
        return Check.Decl{name, kind, ty, body}

def map_decl(levels: LevelAction, decl: Check.Decl) -> Result<&2, &2, Error.T, Check.Decl>:
  Budget.run(Check.Decl, map_decl_action(decl, Action{levels, [], []}), Budget.unlimited())

def scheme_arity(scheme: Maybe<&2, Scheme>) -> Maybe<&2, Nat>:
  match scheme:
    case None{}:
      None{}
    case Some{Scheme{arity, decl}}:
      Some{arity}

def arity(catalog: T, name: String) -> Maybe<&2, Nat>:
  scheme_arity(lookup(catalog, name))

def closed_levels(levels: List<&2, Level.T>) -> Bool:
  match levels:
    case Nil{}:
      True{}
    case Con{head, tail}:
      Level.in_scope(0n, head) && closed_levels(tail)

def rename_decl(decl: Check.Decl, name: String) -> Check.Decl:
  match decl:
    case Check.Decl{old, kind, ty, body}:
      Check.Decl{name, kind, ty, body}

def validate_instance(+fresh: Bool, name: String, +as_name: String, +arity: Nat, +levels: List<&2, Level.T>) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, Bool.not(fresh), Fail{collision(as_name)},
    Bool.pick(Result<&2, &2, Error.T, Unit>, Bool.not(Nat.is_eq(arity, List.length(&2, Level.T, levels))),
      Fail{Error.Universe{"the schema " ++ name ++ " expects " ++ Nat.show(arity) ++ " universe arguments, got " ++ Nat.show(List.length(&2, Level.T, levels))}},
      Bool.pick(Result<&2, &2, Error.T, Unit>, closed_levels(levels), Done{Unit{}}, Fail{Error.Universe{"universe arguments must be closed"}})))

def declare_comp(+globals: Global.T, +catalog: T, +arity: Nat, +decl: Check.Decl) -> Budget.Comp(T):
  do Budget.Comp<T>:
    checked : Unit <- Budget.lift(Unit, Bool.pick(Result<&2, &2, Error.T, Unit>, occupied(globals, catalog, decl_name(decl)), Fail{collision(decl_name(decl))}, Done{Unit{}}))
    checked : Unit <- Check.check_scheme_comp(globals, arity, decl)
    return Common.Pair{decl_name(decl), Scheme{arity, decl}} <> catalog

def declare(globals: Global.T, catalog: T, budget: Budget.T, arity: Nat, decl: Check.Decl) -> Result<&2, &2, Error.T, T>:
  Budget.run(T, declare_comp(globals, catalog, arity, decl), budget)

def instantiate_scheme(scheme: Maybe<&2, Scheme>, +globals: Global.T, catalog: T, name: String, +levels: List<&2, Level.T>, +as_name: String) -> Budget.Comp(Common.Pair<Global.T, Term.T>):
  match scheme:
    case None{}:
      Budget.failed(Common.Pair<Global.T, Term.T>, Error.Unbound{"unknown universe schema " ++ name})
    case Some{Scheme{arity, decl}}:
      do Budget.Comp<Common.Pair<Global.T, Term.T>>:
        checked : Unit <- Budget.lift(Unit, validate_instance(Bool.not(occupied(globals, catalog, as_name)), name, as_name, arity, levels))
        decl : Check.Decl <- Budget.lift(Check.Decl, map_decl(SubstituteLevels{levels}, decl))
        entry : Global.Entry <- Check.check_decl_comp(globals, 0n, rename_decl(decl, as_name))
        return Common.Pair{Global.add(as_name, entry, globals), Term.Global{as_name}}

def instantiate_comp(+globals: Global.T, +catalog: T, +name: String, levels: List<&2, Level.T>, as_name: String) -> Budget.Comp(Common.Pair<Global.T, Term.T>):
  instantiate_scheme(lookup(catalog, name), globals, catalog, name, levels, as_name)

def instantiate(globals: Global.T, catalog: T, budget: Budget.T, name: String, levels: List<&2, Level.T>, as_name: String) -> Result<&2, &2, Error.T, Common.Pair<Global.T, Term.T>>:
  Budget.run(Common.Pair<Global.T, Term.T>, instantiate_comp(globals, catalog, name, levels, as_name), budget)
