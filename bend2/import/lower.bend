import Base
import ./translate.bend as Translate
import ./table.bend as Table
import ../kernel/common.bend as Common
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as KernelError
import ../kernel/bignum.bend as Big
import ../kernel/level.bend as Level
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/eval.bend as Eval
import ../kernel/rules.bend as Rules
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/literal.bend as Literal
import ../kernel/global.bend as Global

type Error is Data:
  Deferred{message: String}
  Unsupported{message: String}
  KernelError{error: KernelError.T}

# A handler returns its successor, so arbitrary resolver state remains affine.
type Resolver is Type:
  Resolver{call: String -> List<&2, Level.T> -> Resolver & Result<&2, &2, String, Term.T>}

type State is Type:
  State{work: Nat, budget: Budget.T, resolver: Resolver}

def Comp(-A: Data) -> Type:
  State -> State & Result<&2, &2, Error, A>

def message(error: Error) -> String:
  match error:
    case Deferred{message}:
      message
    case Unsupported{message}:
      message
    case KernelError{error}:
      KernelError.to_string(error)

def pure(-A: Data, value: A) -> Comp(A):
  state => (state, Done{value})

def failed(-A: Data, error: Error) -> Comp(A):
  state => (state, Fail{error})

def bind_value(-A: Data, -B: Data, value: Result<&2, &2, Error, A>, state: State, next: A -> Comp(B)) -> State & Result<&2, &2, Error, B>:
  match value:
    case Fail{error}:
      (state, Fail{error})
    case Done{value}:
      action = next(value)
      action(state)

def bind_result(-A: Data, -B: Data, result: State & Result<&2, &2, Error, A>, next: A -> Comp(B)) -> State & Result<&2, &2, Error, B>:
  (state, value) = result
  bind_value(A, B, value, state, next)

def Comp.pure(-A: Data, value: A) -> Comp(A):
  pure(A, value)

def Comp.bind(-A: Data, -B: Data, action: Comp(A), next: A -> Comp(B)) -> Comp(B):
  state => bind_result(A, B, action(state), next)

def kernel_error(-A: Data, value: Result<&2, &2, KernelError.T, A>) -> Result<&2, &2, Error, A>:
  match value:
    case Fail{error}:
      Fail{KernelError{error}}
    case Done{value}:
      Done{value}

def kernel_result(-A: Data, value: Common.Pair<Budget.T, Result<&2, &2, KernelError.T, A>>, work: Nat, resolver: Resolver) -> State & Result<&2, &2, Error, A>:
  match value:
    case Common.Pair{budget, value}:
      (State{work, budget, resolver}, kernel_error(A, value))

def kernel_state(-A: Data, state: State, action: Budget.Comp(A)) -> State & Result<&2, &2, Error, A>:
  match state:
    case State{work, budget, resolver}:
      kernel_result(A, Budget.run_state(A, action, budget), work, resolver)

def kernel(-A: Data, action: Budget.Comp(A)) -> Comp(A):
  state => kernel_state(A, state, action)

def lift(-A: Data, value: Result<&2, &2, KernelError.T, A>) -> Comp(A):
  kernel(A, Budget.lift(A, value))

def depth_result(ok: Bool, work: Nat, budget: Budget.T, resolver: Resolver, domain: String) -> State & Result<&2, &2, Error, Unit>:
  match ok:
    case True{}:
      (State{work, budget, resolver}, Done{Unit{}})
    case False{}:
      (State{work, budget, resolver}, Fail{Unsupported{domain ++ " lowering exceeds the depth limit of 1024"}})

def polled(value: Maybe<&2, Budget.T>, work: Nat, resolver: Resolver, depth: U32, domain: String) -> State & Result<&2, &2, Error, Unit>:
  match value:
    case None{}:
      (State{work, Budget.Polls{0n}, resolver}, Fail{KernelError{KernelError.Budget_exhausted{domain ++ " lowering budget is exhausted"}}})
    case Some{budget}:
      depth_result(U32.is_le(depth, 1024), work, budget, resolver, domain)

def stepped(state: State, depth: U32, domain: String) -> State & Result<&2, &2, Error, Unit>:
  match state:
    case State{0n, budget, resolver}:
      (State{0n, budget, resolver}, Fail{KernelError{KernelError.Budget_exhausted{"type lowering exceeds the work limit of 100000 nodes"}}})
    case State{1n+remaining, budget, resolver}:
      polled(Budget.poll(budget), remaining, resolver, depth, domain)

def step(depth: U32, domain: String) -> Comp(Unit):
  state => stepped(state, depth, domain)

def require(-A: Data, value: Maybe<&2, A>, error: Error) -> Comp(A):
  match value:
    case None{}:
      failed(A, error)
    case Some{value}:
      pure(A, value)

def resolved_value(value: Result<&2, &2, String, Term.T>, resolver: Resolver, work: Nat, budget: Budget.T) -> State & Result<&2, &2, Error, Term.T>:
  match value:
    case Fail{message}:
      (State{work, budget, resolver}, Fail{Deferred{message}})
    case Done{term}:
      (State{work, budget, resolver}, Done{term})

def resolved(result: Resolver & Result<&2, &2, String, Term.T>, work: Nat, budget: Budget.T) -> State & Result<&2, &2, Error, Term.T>:
  (resolver, value) = result
  resolved_value(value, resolver, work, budget)

def resolver_call(resolver: Resolver, name: String, levels: List<&2, Level.T>) -> Resolver & Result<&2, &2, String, Term.T>:
  match resolver:
    case Resolver{call}:
      call(name, levels)

def resolve_state(state: State, name: String, levels: List<&2, Level.T>) -> State & Result<&2, &2, Error, Term.T>:
  match state:
    case State{work, budget, resolver}:
      resolved(resolver_call(resolver, name, levels), work, budget)

def resolve(name: String, levels: List<&2, Level.T>) -> Comp(Term.T):
  state => resolve_state(state, name, levels)

@unsafe def missing(+prefix: String) -> Resolver:
  Resolver{name => levels => (missing(prefix), Fail{prefix ++ name})}

@unsafe def fixed(+term: Term.T) -> Resolver:
  Resolver{name => levels => (fixed(term), Done{term})}

def inferred(value: Engine.Inferred) -> Value.T:
  match value:
    case Engine.Inferred{ty, uses}:
      ty

def infer(ctx: Check.Ctx, term: Term.T) -> Comp(Value.T):
  do Comp<Value.T>:
    result : Engine.Inferred <- kernel(Engine.Inferred, Engine.infer_uses(ctx, Q.QZero{}, term))
    pure(Value.T, inferred(result))

def check(ctx: Check.Ctx, term: Term.T, ty: Value.T) -> Comp(Unit):
  do Comp<Unit>:
    uses : Q.Usage <- kernel(Q.Usage, Engine.check_uses(ctx, Q.QZero{}, term, ty))
    pure(Unit, Unit{})

def universe(ctx: Check.Ctx, term: Term.T) -> Comp(Level.T):
  kernel(Level.T, Engine.infer_univ(ctx, term))

def evaluate(+ctx: Check.Ctx, term: Term.T) -> Comp(Value.T):
  lift(Value.T, Eval.eval(Check.globals_of(ctx), Check.env_of(ctx), term))

def quote(+ctx: Check.Ctx, value: Value.T) -> Comp(Term.T):
  lift(Term.T, Eval.quote(Check.globals_of(ctx), Check.size_of(ctx), value))

type LevelResult is Data:
  LevelResult{cache: Table.T<Level.T>, value: Level.T}

def level_cache(result: LevelResult) -> Table.T<Level.T>:
  match result:
    case LevelResult{cache, value}:
      cache

def level_value(result: LevelResult) -> Level.T:
  match result:
    case LevelResult{cache, value}:
      value

def positions(parameters: List<&2, Translate.Parameter>, +index: Nat, table: Table.T<Nat>) -> Table.T<Nat>:
  match parameters:
    case Nil{}:
      table
    case Con{Translate.Parameter{id, name}, rest}:
      positions(rest, 1n+index, Table.put(Nat, table, id, index))

law lower_level_cached:
  for depth: U32
  for +table: Table.T<Translate.Level>
  for +positions: Table.T<Nat>
  for +cache: Table.T<Level.T>
  for +id: String
  Comp(LevelResult)

law lower_level_node:
  for node: Translate.Level
  for +depth: U32
  for +table: Table.T<Translate.Level>
  for +positions: Table.T<Nat>
  for cache: Table.T<Level.T>
  Comp(LevelResult)

@unsafe def level_miss(found: Maybe<&2, Level.T>, +depth: U32, +table: Table.T<Translate.Level>, positions: Table.T<Nat>, cache: Table.T<Level.T>, +id: String) -> Comp(LevelResult):
  match found:
    case Some{value}:
      pure(LevelResult, LevelResult{cache, value})
    case None{}:
      do Comp<LevelResult>:
        Unit <- step(depth, "universe")
        node : Translate.Level <- require(Translate.Level, Table.get(Translate.Level, table, id), Unsupported{"missing type level " ++ id})
        +result : LevelResult <- lower_level_node(node, depth, table, positions, cache)
        pure(LevelResult, LevelResult{Table.put(Level.T, level_cache(result), id, level_value(result)), level_value(result)})

@unsafe def lower_level_cached(depth, table, positions, cache, id):
  level_miss(Table.get(Level.T, cache, id), depth, table, positions, cache, id)

def combine_levels(imax: Bool, a: Level.T, b: Level.T) -> Level.T:
  match imax:
    case True{}:
      Level.imax(a, b)
    case False{}:
      Level.max(a, b)

@unsafe def level_pair(+depth: U32, +table: Table.T<Translate.Level>, +positions: Table.T<Nat>, cache: Table.T<Level.T>, left: String, right: String, imax: Bool) -> Comp(LevelResult):
  do Comp<LevelResult>:
    +a : LevelResult <- lower_level_cached(U32.add(depth, 1), table, positions, cache, left)
    +b : LevelResult <- lower_level_cached(U32.add(depth, 1), table, positions, level_cache(a), right)
    pure(LevelResult, LevelResult{level_cache(b), combine_levels(imax, level_value(a), level_value(b))})

@unsafe def lower_level_node(node, depth, table, positions, cache):
  match node:
    case Translate.LZero{}:
      pure(LevelResult, LevelResult{cache, Level.zero()})
    case Translate.LParam{id, name}:
      do Comp<LevelResult>:
        position : Nat <- require(Nat, Table.get(Nat, positions, id), Unsupported{"unbound universe parameter " ++ name})
        pure(LevelResult, LevelResult{cache, Level.var(position)})
    case Translate.LSucc{child}:
      do Comp<LevelResult>:
        +a : LevelResult <- lower_level_cached(U32.add(depth, 1), table, positions, cache, child)
        pure(LevelResult, LevelResult{level_cache(a), Level.succ(level_value(a))})
    case Translate.LMax{left, right}:
      level_pair(depth, table, positions, cache, left, right, False{})
    case Translate.LImax{left, right}:
      level_pair(depth, table, positions, cache, left, right, True{})

@unsafe def lower_level(table: Translate.T, params: List<&2, Translate.Parameter>, id: String) -> Comp(Level.T):
  do Comp<Level.T>:
    result : LevelResult <- lower_level_cached(0, Translate.level_nodes(table), positions(params, 0n, Table.empty(Nat)), Table.empty(Level.T), id)
    pure(Level.T, level_value(result))

@unsafe def lower_levels(ids: List<&2, String>, +table: Translate.T, +params: List<&2, Translate.Parameter>) -> Comp(List<&2, Level.T>):
  match ids:
    case Nil{}:
      pure(List<&2, Level.T>, Nil{})
    case Con{id, rest}:
      do Comp<List<&2, Level.T>>:
        level : Level.T <- lower_level(table, params, id)
        rest : List<&2, Level.T> <- lower_levels(rest, table, params)
        pure(List<&2, Level.T>, Con{level, rest})

type BoundContext is Data:
  BoundContext{domain: Term.T, ctx: Check.Ctx}

def bound_domain(result: BoundContext) -> Term.T:
  match result:
    case BoundContext{domain, ctx}:
      domain

def bound_ctx(result: BoundContext) -> Check.Ctx:
  match result:
    case BoundContext{domain, ctx}:
      ctx

law lower:
  for +depth: U32
  for +initial: Check.Ctx
  for +ctx: Check.Ctx
  for +table: Translate.T
  for +params: List<&2, Translate.Parameter>
  for +id: String
  Comp(Term.T)

law lower_node:
  for node: Translate.Node
  for +depth: U32
  for +initial: Check.Ctx
  for +ctx: Check.Ctx
  for +table: Translate.T
  for +params: List<&2, Translate.Parameter>
  Comp(Term.T)

@unsafe def bind_domain(+depth: U32, initial: Check.Ctx, +ctx: Check.Ctx, table: Translate.T, params: List<&2, Translate.Parameter>, typ: String, name: String) -> Comp(BoundContext):
  do Comp<BoundContext>:
    +domain : Term.T <- lower(U32.add(depth, 1), initial, ctx, table, params, typ)
    level : Level.T <- universe(ctx, domain)
    value : Value.T <- evaluate(ctx, domain)
    pure(BoundContext, BoundContext{domain, Check.bind(name, Q.QMany{}, value, ctx)})

def function_shape(value: Value.T) -> Comp(Shape.T<Value.T>):
  match value:
    case Value.VRan{shape, diagram, level}:
      pure(Shape.T<Value.T>, shape)
    case other:
      failed(Shape.T<Value.T>, KernelError{KernelError.Mismatch{"the head of an imported application is not a function"}})

type FunctionDomain is Data:
  FunctionDomain{quantity: Q.T, name: String, value: Value.T}

def function_domain(shape: Shape.T<Value.T>) -> Comp(FunctionDomain):
  match shape:
    case Shape.SPi{q, name, domain}:
      pure(FunctionDomain, FunctionDomain{q, name, domain})
    case other:
      failed(FunctionDomain, KernelError{KernelError.Mismatch{"the head of an imported application is not a dependent function"}})

@unsafe def application(domain: FunctionDomain, +depth: U32, initial: Check.Ctx, +ctx: Check.Ctx, table: Translate.T, params: List<&2, Translate.Parameter>, argument: String, head: Term.T) -> Comp(Term.T):
  match domain:
    case FunctionDomain{+q, name, +value}:
      do Comp<Term.T>:
        +argument : Term.T <- lower(U32.add(depth, 1), initial, ctx, table, params, argument)
        Unit <- check(ctx, argument, value)
        pure(Term.T, Term.Out{Shape.SArg{q, name}, Term.APt{q, argument}, head})

def bound_index(value: Maybe<&2, U32>) -> Comp(Term.T):
  match value:
    case None{}:
      failed(Term.T, Unsupported{"invalid de Bruijn index in a type"})
    case Some{index}:
      pure(Term.T, Term.Var{U32.to_nat(index)})

def bound_number(value: Maybe<&2, Big.T>) -> Comp(Term.T):
  match value:
    case None{}:
      failed(Term.T, Unsupported{"invalid de Bruijn index in a type"})
    case Some{value}:
      bound_index(Big.to_u32(value))

@unsafe def lower_node(node, depth, initial, ctx, table, params):
  match node:
    case Translate.Bound{index}:
      bound_number(Big.of_decimal(index))
    case Translate.Sort{level}:
      do Comp<Term.T>:
        value : Level.T <- lower_level(table, params, level)
        pure(Term.T, Term.Univ{value})
    case Translate.Constant{id, name, universes}:
      do Comp<Term.T>:
        levels : List<&2, Level.T> <- lower_levels(universes, table, params)
        +term : Term.T <- resolve(name, levels)
        ty : Value.T <- infer(initial, term)
        pure(Term.T, term)
    case Translate.Apply{head, argument}:
      do Comp<Term.T>:
        +head : Term.T <- lower(U32.add(depth, 1), initial, ctx, table, params, head)
        head_type : Value.T <- infer(ctx, head)
        head_type : Value.T <- lift(Value.T, Eval.whnf(Check.globals_of(ctx), head_type))
        shape : Shape.T<Value.T> <- function_shape(head_type)
        domain : FunctionDomain <- function_domain(shape)
        application(domain, depth, initial, ctx, table, params, argument, head)
    case Translate.Pi{Translate.Binder{+name, typ, body, info}}:
      do Comp<Term.T>:
        +inner : BoundContext <- bind_domain(depth, initial, ctx, table, params, typ, name)
        body : Term.T <- lower(U32.add(depth, 1), initial, bound_ctx(inner), table, params, body)
        pure(Term.T, Rules.arrow(Q.QMany{}, name, bound_domain(inner), body))
    case Translate.Lambda{Translate.Binder{+name, typ, body, info}}:
      do Comp<Term.T>:
        +inner : BoundContext <- bind_domain(depth, initial, ctx, table, params, typ, name)
        +body : Term.T <- lower(U32.add(depth, 1), initial, bound_ctx(inner), table, params, body)
        body_type : Value.T <- infer(bound_ctx(inner), body)
        codomain : Term.T <- quote(bound_ctx(inner), body_type)
        pure(Term.T, Term.Ann{Term.Sec{Shape.SPi{Q.QMany{}, name, bound_domain(inner)},
          [Term.Leg{[Common.Pair{Q.QMany{}, name}], body}]},
          Rules.arrow(Q.QMany{}, name, bound_domain(inner), codomain)})
    case Translate.Let{+name, typ, value, body, nondep}:
      do Comp<Term.T>:
        +typ : Term.T <- lower(U32.add(depth, 1), initial, ctx, table, params, typ)
        level : Level.T <- universe(ctx, typ)
        +typ_value : Value.T <- evaluate(ctx, typ)
        +value : Term.T <- lower(U32.add(depth, 1), initial, ctx, table, params, value)
        Unit <- check(ctx, value, typ_value)
        value_value : Value.T <- evaluate(ctx, value)
        inner : Check.Ctx = Check.define(name, Q.QMany{}, typ_value, value_value, ctx)
        body : Term.T <- lower(U32.add(depth, 1), initial, inner, table, params, body)
        pure(Term.T, Term.Let{name, typ, value, body})
    case Translate.Projection{id, name, index, structure}:
      failed(Term.T, Unsupported{"projection " ++ name ++ "." ++ index ++ " needs a checked prelude representation"})
    case Translate.NatLiteral{value}:
      do Comp<Term.T>:
        value : Big.T <- require(Big.T, Big.of_decimal(value), Unsupported{"invalid natural literal in a type"})
        pure(Term.T, Term.Lit{Literal.LInt{value}})
    case Translate.StringLiteral{value}:
      failed(Term.T, Unsupported{"strVal in a type arrives at M1 with String"})
    case Translate.Metadata{expr, data}:
      lower(U32.add(depth, 1), initial, ctx, table, params, expr)

@unsafe def lower(depth, initial, ctx, table, params, id):
  do Comp<Term.T>:
    Unit <- step(depth, "type")
    node : Translate.Node <- require(Translate.Node, Table.get(Translate.Node, Translate.type_nodes(table), id), Unsupported{"missing type expression " ++ id})
    lower_node(node, depth, initial, ctx, table, params)

@unsafe def lower_row(row: Translate.Row, globals: Global.T, +budget: Budget.T, table: Translate.T) -> Comp(Term.T):
  match row:
    case Translate.Row{declaration, name, +params, root, dependencies}:
      do Comp<Term.T>:
        +initial : Check.Ctx = Check.make(globals, budget, List.length(&2, Translate.Parameter, params))
        +term : Term.T <- lower(0, initial, initial, table, params, root)
        level : Level.T <- universe(initial, term)
        pure(Term.T, term)

def discard(-A: Data, result: State & Result<&2, &2, Error, A>) -> Result<&2, &2, Error, A>:
  (state, value) = result
  value

@unsafe def lower_type(resolver: Resolver, globals: Global.T, table: Translate.T, row: Translate.Row, +budget: Budget.T) -> Result<&2, &2, Error, Term.T>:
  action = lower_row(row, globals, budget, table)
  discard(Term.T, action(State{100000n, budget, resolver}))

@unsafe def lower_default(resolver: Resolver, globals: Global.T, table: Translate.T, row: Translate.Row) -> Result<&2, &2, Error, Term.T>:
  lower_type(resolver, globals, table, row, Budget.of_polls(100000n))
