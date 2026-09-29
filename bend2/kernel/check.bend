import Base
import ./context.bend as Context
import ./check_engine.bend as Engine
import ./common.bend as Common
import ./global.bend as Global
import ./value.bend as Value
import ./term.bend as Term
import ./quantity.bend as Quantity
import ./budget.bend as Budget
import ./level_scope.bend as LevelScope
import ./error.bend as Error
import ./level.bend as Level
import ./positivity.bend as Positivity
import ./eval.bend as Eval
import ./shape.bend as Shape
import ./level_eq.bend as LevelEq

type DeclKind is Data:
  Definition{}
  Postulate{}

type Decl is Data:
  Decl{name: String, kind: DeclKind, ty: Term.T, body: Maybe<&2, Term.T>}

type FamilyDecl is Data:
  FamilyDecl{name: String, params: Positivity.Telescope, indices: Positivity.Telescope, level: Level.T}

type CtorDecl is Data:
  CtorDecl{name: String, args: Positivity.Telescope, res_params: List<&2, Term.T>, res_idx: List<&2, Term.T>}


def Local() -> Data:
  Context.Local

def Ctx() -> Data:
  Context.Ctx

def make(globals: Global.T, budget: Budget.T, level_arity: Nat) -> Ctx:
  Context.Ctx{globals, [], Context.LNil{}, 0n, budget, level_arity, False{}}

def globals_of(ctx: Ctx) -> Global.T:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      globals

def env_of(ctx: Ctx) -> List<&2, Value.T>:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      env

def size_of(ctx: Ctx) -> Nat:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      size

def names(locals: List<&2, Local>) -> List<&2, String>:
  match locals:
    case Nil{}:
      []
    case Con{Context.Local{name, q, ty}, rest}:
      name <> names(rest)

def names_of(ctx: Ctx) -> List<&2, String>:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      names(Context.to_list(locals))

@unsafe def name_index(view: Context.LView, +name: String, +index: Nat) -> Maybe<&2, Nat>:
  match view:
    case Context.LVNil{}:
      None{}
    case Context.LVCons{Context.Local{local, q, ty}, rest}:
      Bool.pick(Maybe<&2, Nat>, String.eq(local, name), Some{index}, name_index(Context.lview(rest), name, 1n+index))

def index_of_name(ctx: Ctx, +name: String) -> Maybe<&2, Nat>:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      name_index(Context.lview(locals), name, 0n)

def bind(name: String, q: Quantity.T, ty: Value.T, ctx: Ctx) -> Ctx:
  match ctx:
    case Context.Ctx{globals, env, locals, +size, budget, arity, checked}:
      Context.Ctx{globals, Value.var(size) <> env, Context.lcons(Context.Local{name, q, ty}, locals), 1n+size, budget, arity, checked}

def define(name: String, q: Quantity.T, ty: Value.T, value: Value.T, ctx: Ctx) -> Ctx:
  match ctx:
    case Context.Ctx{globals, env, locals, size, budget, arity, checked}:
      Context.Ctx{globals, value <> env, Context.lcons(Context.Local{name, q, ty}, locals), 1n+size, budget, arity, checked}

def readable(mode: Quantity.T, q: Quantity.T) -> Bool:
  Bool.or(Quantity.equal(mode, Quantity.QZero{}), Bool.not(Quantity.equal(q, Quantity.QZero{})))

def context_budget(ctx: Ctx) -> Budget.T:
  match ctx:
    case Context.Ctx{g, env, locals, size, budget, arity, checked}:
      budget

def infer_result(result: Result<&2, &2, Error.T, Engine.Inferred>) -> Result<&2, &2, Error.T, Value.T>:
  match result:
    case Fail{error}:
      Fail{error}
    case Done{Engine.Inferred{ty, uses}}:
      Done{ty}

def infer(+ctx: Ctx, mode: Quantity.T, term: Term.T) -> Result<&2, &2, Error.T, Value.T>:
  infer_result(Budget.run(Engine.Inferred, Engine.infer_uses(ctx, Quantity.runtime(mode), term), context_budget(ctx)))

def infer_node(+ctx: Ctx, mode: Quantity.T, term: Term.T) -> Result<&2, &2, Error.T, Engine.Inferred>:
  Budget.run(Engine.Inferred, Engine.infer_scoped(ctx, mode, term), context_budget(ctx))

def check_result(result: Result<&2, &2, Error.T, Quantity.Usage>) -> Result<&2, &2, Error.T, Unit>:
  match result:
    case Fail{error}:
      Fail{error}
    case Done{uses}:
      Done{Unit{}}

def check(+ctx: Ctx, mode: Quantity.T, term: Term.T, expected: Value.T) -> Result<&2, &2, Error.T, Unit>:
  check_result(Budget.run(Quantity.Usage, Engine.check_uses(ctx, Quantity.runtime(mode), term, expected), context_budget(ctx)))

def infer_univ(+ctx: Ctx, term: Term.T) -> Result<&2, &2, Error.T, Level.T>:
  Budget.run(Level.T, Engine.infer_univ(ctx, term), context_budget(ctx))

law type_valued:
  for +globals: Global.T
  for +size: Nat
  for value: Value.T
  Budget.Comp(Bool)

@unsafe def type_valued_node(+globals: Global.T, +size: Nat, value: Value.T) -> Budget.Comp(Bool):
  match value:
    case Value.VUniv{level}:
      Budget.pure(Bool, True{})
    case Value.VRan{Shape.SPi{q, name, domain}, clo, level}:
      do Budget.Comp<Bool>:
        cod : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals, clo, [Value.var(size)]))
        type_valued(globals, 1n+size, cod)
    case _:
      Budget.pure(Bool, False{})

@unsafe def type_valued(globals, size, value):
  do Budget.Comp<Bool>:
    value : Value.T <- Budget.lift(Value.T, Eval.whnf(globals, value))
    type_valued_node(globals, size, value)

def check_definition(+ctx: Ctx, name: String, ty: Term.T, body: Maybe<&2, Term.T>, +tyv: Value.T) -> Budget.Comp(Global.Entry):
  do Budget.Comp<Global.Entry>:
    +body : Term.T <- Engine.optional(Term.T, body, Error.Cannot_infer{"the definition " ++ name ++ " has no body"})
    erased : Bool <- type_valued(globals_of(ctx), 0n, tyv)
    uses : Quantity.Usage <- Engine.check_uses(ctx, Bool.pick(Quantity.T, erased, Quantity.QZero{}, Quantity.QOne{}), body, tyv)
    return Global.Def{Global.DefEntry{ty, body, True{}, None{}, False{}}}

def check_decl_kind(kind: DeclKind, ctx: Ctx, name: String, ty: Term.T, body: Maybe<&2, Term.T>, tyv: Value.T) -> Budget.Comp(Global.Entry):
  match kind:
    case Postulate{}:
      Budget.pure(Global.Entry, Global.Axiom{ty})
    case Definition{}:
      check_definition(ctx, name, ty, body, tyv)

def check_decl_comp(globals: Global.T, arity: Nat, decl: Decl) -> Budget.Comp(Global.Entry):
  match decl:
    case Decl{name, kind, +ty, body}:
      do Budget.Comp<Global.Entry>:
        +ctx : Ctx = make(globals, Budget.Unlimited{}, arity)
        level : Level.T <- Engine.infer_univ(ctx, ty)
        tyv : Value.T <- Engine.eval(ctx, ty)
        check_decl_kind(kind, ctx, name, ty, body, tyv)

def check_decl_at(globals: Global.T, budget: Budget.T, arity: Nat, decl: Decl) -> Result<&2, &2, Error.T, Global.Entry>:
  Budget.run(Global.Entry, check_decl_comp(globals, arity, decl), budget)

def check_decl(globals: Global.T, budget: Budget.T, decl: Decl) -> Result<&2, &2, Error.T, Global.Entry>:
  check_decl_at(globals, budget, 0n, decl)

def check_scheme_comp(globals: Global.T, arity: Nat, decl: Decl) -> Budget.Comp(Unit):
  do Budget.Comp<Unit>:
    entry : Global.Entry <- check_decl_comp(globals, arity, decl)
    return Unit{}

def check_scheme(globals: Global.T, budget: Budget.T, arity: Nat, decl: Decl) -> Result<&2, &2, Error.T, Unit>:
  Budget.run(Unit, check_scheme_comp(globals, arity, decl), budget)

def decl_name(decl: Decl) -> String:
  match decl:
    case Decl{name, kind, ty, body}:
      name

@unsafe def check_decls_comp(+globals: Global.T, decls: List<&2, Decl>) -> Budget.Comp(List<&2, Common.Pair<String, Global.Entry>>):
  match decls:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<String, Global.Entry>>, [])
    case Con{+decl, rest}:
      do Budget.Comp<List<&2, Common.Pair<String, Global.Entry>>>:
        +entry : Global.Entry <- check_decl_comp(globals, 0n, decl)
        +name : String = decl_name(decl)
        rows : List<&2, Common.Pair<String, Global.Entry>> <- check_decls_comp(Global.add(name, entry, globals), rest)
        return Common.Pair{name, entry} <> rows

def check_decls(globals: Global.T, budget: Budget.T, decls: List<&2, Decl>) -> Result<&2, &2, Error.T, List<&2, Common.Pair<String, Global.Entry>>>:
  Budget.run(List<&2, Common.Pair<String, Global.Entry>>, check_decls_comp(globals, decls), budget)

@unsafe def check_telescope(tele: Positivity.Telescope, +ctx: Ctx) -> Budget.Comp(Ctx):
  match tele:
    case Nil{}:
      Budget.pure(Ctx, ctx)
    case Con{Positivity.Binder{q, name, +ty}, rest}:
      do Budget.Comp<Ctx>:
        level : Level.T <- Engine.infer_univ(ctx, ty)
        tyv : Value.T <- Engine.eval(ctx, ty)
        check_telescope(rest, bind(name, q, tyv, ctx))

def index_rules(ctx: Ctx, +family: String, +level: Level.T, binder: Positivity.Binder) -> Budget.Comp(Unit):
  match binder:
    case Positivity.Binder{+q, +name, ty}:
      do Budget.Comp<Unit>:
        ok : Unit <- Engine.require(Quantity.equal(q, Quantity.QZero{}), Error.Index_not_zero{"the index " ++ name ++ " of " ++ family ++ " is at quantity " ++ Quantity.to_string(q) ++ " and every index binder is at 0"})
        +actual : Level.T <- Engine.infer_univ(ctx, ty)
        below : Bool <- Bool.pick(Budget.Comp(Bool), Level.always_zero(level), Budget.pure(Bool, True{}), LevelEq.decide_m(actual, level, False{}))
        Engine.require(below, Error.Index_above_universe{"the index " ++ name ++ " of " ++ family ++ " lives at " ++ Level.to_string(actual) ++ " and " ++ family ++ " is declared at " ++ Level.to_string(level)})

@unsafe def check_index_telescope(tele: Positivity.Telescope, +ctx: Ctx, +name: String, +level: Level.T) -> Budget.Comp(Ctx):
  match tele:
    case Nil{}:
      Budget.pure(Ctx, ctx)
    case Con{+binder, rest}:
      do Budget.Comp<Ctx>:
        ok : Unit <- index_rules(ctx, name, level, binder)
        next : Ctx <- check_telescope([binder], ctx)
        check_index_telescope(rest, next, name, level)

def absent_family(value: Maybe<&2, Positivity.Family>) -> Bool:
  match value:
    case None{}:
      True{}
    case Some{family}:
      False{}

def declare_family_comp(+globals: Global.T, +arity: Nat, decl: FamilyDecl) -> Budget.Comp(Global.T):
  match decl:
    case FamilyDecl{+name, +params, +indices, +level}:
      do Budget.Comp<Global.T>:
        tick : Unit <- Budget.tick()
        scope : Unit <- Engine.require(Level.in_scope(arity, level), Error.Universe{Bool.pick(String, Nat.is_eq(arity, 0n), "family universe levels must be closed at M0 Stage A", "universe level is outside the global parameter scope")})
        absent : Unit <- Engine.require(absent_family(Global.find_family(name, globals)), Error.Mismatch{"the family " ++ name ++ " is already declared"})
        pctx : Ctx <- check_telescope(params, make(globals, Budget.Unlimited{}, arity))
        ictx : Ctx <- check_index_telescope(indices, pctx, name, level)
        return Global.add_family(name, Positivity.Family{name, params, indices, level, Positivity.Provisional{}, [], False{}}, globals)

def declare_family_at(globals: Global.T, budget: Budget.T, arity: Nat, decl: FamilyDecl) -> Result<&2, &2, Error.T, Global.T>:
  Budget.run(Global.T, declare_family_comp(globals, arity, decl), budget)

def declare_family(globals: Global.T, budget: Budget.T, decl: FamilyDecl) -> Result<&2, &2, Error.T, Global.T>:
  declare_family_at(globals, budget, 0n, decl)

def parameter_at(term: Term.T, +index: Nat) -> Bool:
  match term:
    case Term.Var{actual}:
      Nat.is_eq(actual, index)
    case Term.Ann{body, ty}:
      parameter_at(body, index)
    case _:
      False{}

def parameters_at(terms: List<&2, Term.T>, +index: Nat) -> Bool:
  match terms:
    case Nil{}:
      True{}
    case Con{term, rest}:
      Bool.and(parameter_at(term, Nat.sub(index, 1n)), parameters_at(rest, Nat.sub(index, 1n)))

@unsafe def ctor_fields(tele: Positivity.Telescope, +ctx: Ctx, +name: String, +level: Level.T) -> Budget.Comp(Ctx):
  match tele:
    case Nil{}:
      Budget.pure(Ctx, ctx)
    case Con{Positivity.Binder{+q, +x, +ty}, rest}:
      do Budget.Comp<Ctx>:
        actual : Level.T <- Engine.infer_univ(ctx, ty)
        below : Bool <- LevelEq.decide_m(actual, level, False{})
        ok : Unit <- Engine.require(below, Error.Universe{"a field of " ++ name ++ " exceeds its family universe"})
        next : Ctx <- check_telescope([Positivity.Binder{q, x, ty}], ctx)
        ctor_fields(rest, next, name, level)

def check_ctor_comp(ctx: Ctx, family: Positivity.Family, +group: List<&2, String>, decl: CtorDecl) -> Budget.Comp(Positivity.Ctor):
  match family decl:
    case Positivity.Family{+family_name, +params, +indices, level, status, ctors, positive} CtorDecl{+name, +args, +res_params, +res_idx}:
      do Budget.Comp<Positivity.Ctor>:
        +actx : Ctx <- ctor_fields(args, ctx, name, level)
        positive : Unit <- Budget.lift(Unit, Positivity.ctor_fields(group, args))
        +np : Nat = List.length(&2, Positivity.Binder, params)
        +depth : Nat = List.length(&2, Positivity.Binder, args)
        preserved : Unit <- Engine.require(Bool.and(Nat.is_eq(List.length(&2, Term.T, res_params), np), parameters_at(res_params, Nat.add(depth, np))), Error.Mismatch{"the constructor " ++ name ++ " must preserve its family parameters"})
        penv : Engine.EnvUses <- Engine.telescope_uses(actx, Quantity.QZero{}, "the parameters of " ++ name, params, res_params, [])
        +ni : Nat = List.length(&2, Positivity.Binder, indices)
        count : Unit <- Engine.require(Nat.is_eq(List.length(&2, Term.T, res_idx), ni), Error.Mismatch{name ++ " gives " ++ Nat.show(List.length(&2, Term.T, res_idx)) ++ " result indices and " ++ family_name ++ " takes " ++ Nat.show(ni)})
        ienv : Engine.EnvUses <- Engine.telescope_uses(actx, Quantity.QZero{}, "the result indices of " ++ name, indices, res_idx, Engine.env_values(penv))
        return Positivity.Ctor{name, args, res_idx, Nat.add(np, depth), Positivity.self_rec(args, group)}

def provisional(family: Positivity.Family) -> Bool:
  match family:
    case Positivity.Family{n, p, i, l, Positivity.Provisional{}, c, pos}:
      True{}
    case _:
      False{}

@unsafe def check_ctors_comp(ctors: List<&2, CtorDecl>, +ctx: Ctx, +family: Positivity.Family, +group: List<&2, String>) -> Budget.Comp(List<&2, Positivity.Ctor>):
  match ctors:
    case Nil{}:
      Budget.pure(List<&2, Positivity.Ctor>, [])
    case Con{ctor, rest}:
      do Budget.Comp<List<&2, Positivity.Ctor>>:
        tick : Unit <- Budget.tick()
        ctor : Positivity.Ctor <- check_ctor_comp(ctx, family, group, ctor)
        rest : List<&2, Positivity.Ctor> <- check_ctors_comp(rest, ctx, family, group)
        return ctor <> rest

def ctor_names(ctors: List<&2, Positivity.Ctor>) -> List<&2, String>:
  match ctors:
    case Nil{}:
      []
    case Con{Positivity.Ctor{name, args, idx, arity, rec}, rest}:
      name <> ctor_names(rest)

def completed(family: Positivity.Family, +ctors: List<&2, Positivity.Ctor>) -> Positivity.Family:
  match family:
    case Positivity.Family{name, params, indices, level, status, old, positive}:
      Positivity.Family{name, params, indices, level, Positivity.Complete{ctor_names(ctors)}, ctors, True{}}

def define_ctors_comp(+globals: Global.T, arity: Nat, group: List<&2, String>, +name: String, ctors: List<&2, CtorDecl>) -> Budget.Comp(Global.T):
  do Budget.Comp<Global.T>:
    tick : Unit <- Budget.tick()
    +family : Positivity.Family <- Engine.optional(Positivity.Family, Global.find_family(name, globals), Error.Unbound{"the family " ++ name ++ " is not declared"})
    state : Unit <- Engine.require(provisional(family), Error.Mismatch{"the constructors of " ++ name ++ " are already installed"})
    ctx : Ctx <- check_telescope(Engine.fam_params(family), make(globals, Budget.Unlimited{}, arity))
    ctors : List<&2, Positivity.Ctor> <- check_ctors_comp(ctors, ctx, family, group)
    return Global.add_family(name, completed(family, ctors), globals)

def define_ctors_at(globals: Global.T, budget: Budget.T, arity: Nat, group: List<&2, String>, name: String, ctors: List<&2, CtorDecl>) -> Result<&2, &2, Error.T, Global.T>:
  Budget.run(Global.T, define_ctors_comp(globals, arity, group, name, ctors), budget)

def define_ctors(globals: Global.T, budget: Budget.T, group: List<&2, String>, name: String, ctors: List<&2, CtorDecl>) -> Result<&2, &2, Error.T, Global.T>:
  define_ctors_at(globals, budget, 0n, group, name, ctors)

def family_decl_name(decl: FamilyDecl) -> String:
  match decl:
    case FamilyDecl{name, p, i, l}:
      name

def family_decl_level(decl: FamilyDecl) -> Level.T:
  match decl:
    case FamilyDecl{name, p, i, l}:
      l

def check_family_scheme_comp(globals: Global.T, +arity: Nat, +decl: FamilyDecl, ctors: List<&2, CtorDecl>) -> Budget.Comp(Unit):
  do Budget.Comp<Unit>:
    globals : Global.T <- declare_family_comp(globals, arity, decl)
    +level : Level.T = family_decl_level(decl)
    stable : Unit <- Engine.require(Bool.or(Level.always_zero(level), Level.always_positive(level)), Error.Universe{"a family schema must remain in Prop or Type for every universe substitution"})
    +name : String = family_decl_name(decl)
    globals : Global.T <- define_ctors_comp(globals, arity, [name], name, ctors)
    return Unit{}

def check_family_scheme(globals: Global.T, budget: Budget.T, arity: Nat, decl: FamilyDecl, ctors: List<&2, CtorDecl>) -> Result<&2, &2, Error.T, Unit>:
  Budget.run(Unit, check_family_scheme_comp(globals, arity, decl, ctors), budget)

def infer_term(globals: Global.T, budget: Budget.T, term: Term.T) -> Result<&2, &2, Error.T, Value.T>:
  infer(make(globals, budget, 0n), Quantity.QMany{}, term)

def check_term(globals: Global.T, budget: Budget.T, term: Term.T, ty: Value.T) -> Result<&2, &2, Error.T, Unit>:
  check(make(globals, budget, 0n), Quantity.QMany{}, term, ty)
