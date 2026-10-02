import Base
import ./common.bend as Pair
import ./context.bend as C
import ./term.bend as Term
import ./value.bend as Value
import ./shape.bend as Shape
import ./quantity.bend as Q
import ./level.bend as Level
import ./level_eq.bend as LevelEq
import ./level_var.bend as LevelVar
import ./global.bend as Global
import ./positivity.bend as Positivity
import ./budget.bend as B
import ./eval.bend as Eval
import ./rules.bend as R
import ./prim.bend as Prim
import ./literal.bend as Literal
import ./bignum.bend as Big
import ./pp.bend as Pp
import ./error.bend as Error
import ./circuit.bend as Circuit

type Inferred is Data:
  Inferred{ty: Value.T, uses: Q.Usage}

type Former is Data:
  Former{whole: Value.T, shape: Shape.T<Value.T>, diagram: Value.Closure<Value.T>, level: Maybe<&2, Level.T>}

type Point is Data:
  Point{quantity: Q.T, name: String, domain: Value.T}

law infer_uses:
  for +ctx: C.Ctx
  for +mode: Q.T
  for term: Term.T
  B.Comp(Inferred)

law check_uses:
  for +ctx: C.Ctx
  for +mode: Q.T
  for +term: Term.T
  for expected: Value.T
  B.Comp(Q.Usage)

law infer_univ:
  for +ctx: C.Ctx
  for term: Term.T
  B.Comp(Level.T)

law conv:
  for +ctx: C.Ctx
  for ty: Value.T
  for a: Value.T
  for b: Value.T
  B.Comp(Bool)

law conv_type:
  for +ctx: C.Ctx
  for a: Value.T
  for b: Value.T
  B.Comp(Bool)

law form:
  for +ctx: C.Ctx
  for left: Bool
  for shape: Shape.T<Term.T>
  for diagram: Term.T
  for expected: Maybe<&2, Level.T>
  B.Comp(Level.T)

law intro_sec:
  for +ctx: C.Ctx
  for +mode: Q.T
  for shape: Shape.T<Term.T>
  for legs: List<&2, Term.Leg<Term.T>>
  for expected: Value.T
  B.Comp(Q.Usage)

law intro_in:
  for +ctx: C.Ctx
  for +mode: Q.T
  for shape: Shape.T<Term.T>
  for addr: Term.Addr<Term.T>
  for args: List<&2, Term.T>
  for expected: Value.T
  B.Comp(Q.Usage)

law elim_out:
  for +ctx: C.Ctx
  for +mode: Q.T
  for shape: Shape.T<Term.T>
  for addr: Term.Addr<Term.T>
  for head: Term.T
  B.Comp(Inferred)

law elim_elim:
  for +ctx: C.Ctx
  for +mode: Q.T
  for data: Term.ElimData<Term.T>
  for expected: Maybe<&2, Value.T>
  B.Comp(Inferred)

law scope_walk:
  for todo: List<&2, Term.T>
  for +arity: Nat
  B.Comp(Unit)

law let_body:
  for +ctx: C.Ctx
  for +mode: Q.T
  for name: String
  for +ty: Term.T
  for +value: Term.T
  for body: Term.T
  for expected: Maybe<&2, Value.T>
  B.Comp(Inferred)

def require(valid: Bool, error: Error.T) -> B.Comp(Unit):
  match valid:
    case True{}:
      B.pure(Unit, Unit{})
    case False{}:
      B.failed(Unit, error)

def optional(-A: Data, value: Maybe<&2, A>, error: Error.T) -> B.Comp(A):
  match value:
    case None{}:
      B.failed(A, error)
    case Some{x}:
      B.pure(A, x)

# The error is built only when the check fails. A strict error string costs an
# allocation on every success, for example on each variable read.
def require_by(valid: Bool, error: Unit -> Error.T) -> B.Comp(Unit):
  match valid:
    case True{}:
      B.pure(Unit, Unit{})
    case False{}:
      B.failed(Unit, error(Unit{}))

def optional_by(-A: Data, value: Maybe<&2, A>, error: Unit -> Error.T) -> B.Comp(A):
  match value:
    case None{}:
      B.failed(A, error(Unit{}))
    case Some{x}:
      B.pure(A, x)

def inferred_ty(value: Inferred) -> Value.T:
  match value:
    case Inferred{ty, uses}:
      ty

def inferred_uses(value: Inferred) -> Q.Usage:
  match value:
    case Inferred{ty, uses}:
      uses

def eval(+ctx: C.Ctx, term: Term.T) -> B.Comp(Value.T):
  B.lift(Value.T, Eval.eval(C.globals_of(ctx), C.env_of(ctx), term))

def whnf(ctx: C.Ctx, value: Value.T) -> B.Comp(Value.T):
  B.lift(Value.T, Eval.whnf(C.globals_of(ctx), value))

def open(ctx: C.Ctx, clo: Value.Closure<Value.T>, args: List<&2, Value.T>) -> B.Comp(Value.T):
  B.lift(Value.T, Eval.open_closure(C.globals_of(ctx), clo, args))

def quote(+ctx: C.Ctx, value: Value.T) -> B.Comp(Term.T):
  B.lift(Term.T, Eval.quote(C.globals_of(ctx), C.size_of(ctx), value))

def pp_result(result: Result<&2, &2, Error.T, Term.T>, names: List<&2, String>) -> String:
  match result:
    case Fail{error}:
      "a value that does not read back"
    case Done{term}:
      Pp.term(names, term)

def pp_value(+ctx: C.Ctx, value: Value.T) -> String:
  pp_result(Eval.quote(C.globals_of(ctx), C.size_of(ctx), value), C.names_of(ctx))

def check_scope_node(term: Term.T, arity: Nat) -> B.Comp(Unit):
  match term:
    case Term.Univ{level}:
      require(Level.in_scope(arity, level), Error.Universe{"universe level is outside the global parameter scope"})
    case _:
      B.pure(Unit, Unit{})

@unsafe def scope_walk(todo, arity):
  match todo:
    case Nil{}:
      B.pure(Unit, Unit{})
    case Con{+term, rest}:
      do B.Comp<Unit>:
        tick : Unit <- B.tick()
        valid : Unit <- check_scope_node(term, arity)
        scope_walk(List.append(&2, Term.T, Term.children(term), rest), arity)

@unsafe def scope(ctx: C.Ctx, term: Term.T) -> B.Comp(C.Ctx):
  match ctx:
    case C.Ctx{g, env, locals, size, budget, arity, True{}}:
      B.pure(C.Ctx, C.Ctx{g, env, locals, size, budget, arity, True{}})
    case C.Ctx{g, env, locals, size, budget, +arity, False{}}:
      do B.Comp<C.Ctx>:
        ok : Unit <- scope_walk([term], arity)
        return C.Ctx{g, env, locals, size, budget, arity, True{}}

def close_valid(is_outer: Bool, mode: Q.T, q: Q.T, affine: Bool, level: Nat, uses: Q.Usage) -> Bool:
  match is_outer mode q affine:
    case True{} _ _ _: True{}
    case False{} Q.QZero{} _ _: True{}
    case False{} _ Q.QZero{} _: True{}
    case False{} _ Q.QMany{} _: True{}
    case False{} _ Q.QOne{} True{}: Q.at_most_once(level,uses)
    case False{} _ Q.QOne{} False{}: Q.exactly_once(level,uses)

def close_local(valid: Bool, +name: String, affine: Bool) -> B.Comp(Unit):
  match valid:
    case True{}:
      B.pure(Unit, Unit{})
    case False{}:
      B.failed(Unit, Error.Quantity{Bool.pick(String, affine, "the linear alias " ++ name ++ " may be used at most once", "the linear binder " ++ name ++ " must be used exactly once on every runtime path")})

law close_loop:
  for locals: C.LView
  for +size: Nat
  for +outer: Nat
  for +mode: Q.T
  for +affine: Bool
  for +uses: Q.Usage
  B.Comp(Q.Usage)

@unsafe def close_continue(outside: Bool, rest: C.Locals, +level: Nat, +outer: Nat, +mode: Q.T, +affine: Bool, +uses: Q.Usage, name: String, q: Q.T) -> B.Comp(Q.Usage):
  match outside:
    case True{}:
      B.pure(Q.Usage,uses)
    case False{}:
      do B.Comp<Q.Usage>:
        ok : Unit <- close_local(close_valid(False{},mode,q,affine,level,uses),name,affine)
        close_loop(C.lview(rest),level,outer,mode,affine,Q.remove(uses,level))

@unsafe def close_loop(locals,size,outer,mode,affine,uses):
  match locals size:
    case C.LVNil{} _:
      B.pure(Q.Usage,uses)
    case C.LVCons{C.Local{name,q,ty},rest} 1n+level:
      close_continue(Nat.is_lt(level,outer),rest,level,outer,mode,affine,uses,name,q)
    case _ 0n:
      B.pure(Q.Usage,uses)

def close(ctx: C.Ctx, outer: Nat, mode: Q.T, uses: Q.Usage, affine: Bool) -> B.Comp(Q.Usage):
  match ctx:
    case C.Ctx{g, env, locals, size, budget, arity, checked}:
      close_loop(C.lview(locals), size, outer, mode, affine, uses)

def head_local(value: Maybe<&2, C.Local>) -> B.Comp(Value.T):
  match value:
    case Some{C.Local{name, q, ty}}:
      B.pure(Value.T, ty)
    case None{}:
      B.failed(Value.T, Error.Unbound{"a local level is outside the context"})

def head_global(entry: Maybe<&2, Global.Entry>, globals: Global.T, name: String) -> B.Comp(Value.T):
  match entry:
    case None{}:
      B.failed(Value.T, Error.Unbound{name})
    case Some{e}:
      B.lift(Value.T, Eval.eval(globals, [], Global.entry_ty(e)))

def head_local_bounded(in_bounds: Bool, locals: C.Locals, size: Nat, level: Nat) -> B.Comp(Value.T):
  match in_bounds:
    case True{}:
      head_local(C.lget(locals, Nat.sub(size, 1n+level)))
    case False{}:
      head_local(None{})

def head_ty(ctx: C.Ctx, head: Value.Head) -> B.Comp(Value.T):
  match ctx head:
    case C.Ctx{g, env, locals, +size, budget, arity, checked} Value.HLocal{+level}:
      head_local_bounded(Nat.is_lt(level, size), locals, size, level)
    case C.Ctx{+g, env, locals, size, budget, arity, checked} Value.HGlobal{+name}:
      head_global(Global.find(name, g), g, name)

def universe_value(value: Value.T, ctx: C.Ctx) -> B.Comp(Level.T):
  match value:
    case Value.VUniv{level}: B.pure(Level.T,level)
    case _: B.failed(Level.T,Error.Universe{"a term used as a type is not a universe:  " ++ pp_value(ctx,value)})


@unsafe def infer_univ(ctx, term):
  do B.Comp<Level.T>:
    got : Inferred <- infer_uses(ctx, Q.QZero{}, term)
    value : Value.T <- whnf(ctx, inferred_ty(got))
    universe_value(value, ctx)


def local_infer(local: Maybe<&2, C.Local>, index: Nat, size: Nat, +mode: Q.T) -> B.Comp(Inferred):
  match local:
    case None{}:
      B.failed(Inferred, Error.Unbound{"de Bruijn index " ++ Nat.show(index) ++ " is outside the context"})
    case Some{C.Local{name, q, ty}}:
      do B.Comp<Inferred>:
        ok : Unit <- require_by(C.readable(mode, q), unit => Error.Quantity{"the erased binder " ++ name ++ " is read in a runtime position"})
        return Inferred{ty, Q.occurrence(Nat.sub(size, 1n+index), mode)}

def infer_var(ctx: C.Ctx, mode: Q.T, +index: Nat) -> B.Comp(Inferred):
  match ctx:
    case C.Ctx{g, env, locals, size, budget, arity, checked}:
      local_infer(C.lget(locals, index), index, size, mode)

@unsafe def infer_ann(ctx: C.Ctx, mode: Q.T, body: Term.T, +ty: Value.T) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    uses : Q.Usage <- check_uses(ctx, mode, body, ty)
    return Inferred{ty, uses}


@unsafe def infer_node(+ctx: C.Ctx, +mode: Q.T, term: Term.T) -> B.Comp(Inferred):
  match term:
    case Term.Var{index}:
      infer_var(ctx, mode, index)
    case Term.Univ{level}:
      B.pure(Inferred, Inferred{Value.VUniv{Level.succ(level)}, Q.empty()})
    case Term.Lan{s, d}:
      do B.Comp<Inferred>:
        level : Level.T <- form(ctx, True{}, s, d, None{})
        return Inferred{Value.VUniv{level}, Q.empty()}
    case Term.Ran{s, d}:
      do B.Comp<Inferred>:
        level : Level.T <- form(ctx, False{}, s, d, None{})
        return Inferred{Value.VUniv{level}, Q.empty()}
    case Term.Out{s, addr, head}:
      elim_out(ctx, mode, s, addr, head)
    case Term.Elim{data}:
      elim_elim(ctx, mode, data, None{})
    case Term.Let{name, ty, value, body}:
      let_body(ctx, mode, name, ty, value, body, None{})
    case Term.Ann{body, +ty}:
      do B.Comp<Inferred>:
        level : Level.T <- infer_univ(ctx, ty)
        tyv : Value.T <- eval(ctx, ty)
        infer_ann(ctx, mode, body, tyv)
    case Term.Global{name}:
      do B.Comp<Inferred>:
        ty : Value.T <- head_ty(ctx, Value.HGlobal{name})
        return Inferred{ty, Q.empty()}
    case Term.Lit{Literal.LInt{value}}:
      do B.Comp<Inferred>:
        ok : Unit <- require(Big.le(Big.zero(), value), Error.Mismatch{"a Nat literal must be nonnegative"})
        ty : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), [], Prim.nat_ty()))
        return Inferred{ty, Q.empty()}
    case Term.Lit{Literal.LString{value}}:
      B.failed(Inferred, Error.Not_yet{"string types arrive at M1"})
    case Term.In{s, addr, args}:
      B.failed(Inferred, Error.Cannot_infer{"an injection has no type of its own;  it needs an expected type"})
    case Term.Sec{s, legs}:
      B.failed(Inferred, Error.Cannot_infer{"a section has no type of its own;  it needs an expected type"})
    case Term.Auto{}:
      B.failed(Inferred, Error.Not_yet{"instances arrive at M2"})


@unsafe def infer_scoped(ctx: C.Ctx, mode: Q.T, +term: Term.T) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    inner : C.Ctx <- scope(ctx, term)
    infer_node(inner, mode, term)


@unsafe def infer_uses(ctx, mode, term):
  do B.Comp<Inferred>:
    tick : Unit <- B.tick()
    infer_scoped(ctx, mode, term)


def ensure_result(same: Bool, +ctx: C.Ctx, got: Value.T, expected: Value.T) -> B.Comp(Unit):
  match same:
    case True{}: B.pure(Unit,Unit{})
    case False{}: B.failed(Unit,Error.Mismatch{"the term has type " ++ pp_value(ctx,got) ++ " and the expected type is " ++ pp_value(ctx,expected)})

@unsafe def ensure(+ctx: C.Ctx, +got: Value.T, +expected: Value.T) -> B.Comp(Unit):
  do B.Comp<Unit>:
    same : Bool <- conv_type(ctx, got, expected)
    ensure_result(same,ctx,got,expected)

@unsafe def check_inferred(got: Inferred, ctx: C.Ctx, expected: Value.T) -> B.Comp(Q.Usage):
  match got:
    case Inferred{ty, uses}:
      do B.Comp<Q.Usage>:
        ok : Unit <- ensure(ctx, ty, expected)
        return uses

def former_equal_result(same: Bool, got: Level.T, expected: Level.T) -> B.Comp(Unit):
  match same:
    case True{}: B.pure(Unit,Unit{})
    case False{}: B.failed(Unit,Error.Universe{"the former lives at " ++ Level.to_string(got) ++ " and the expected universe is " ++ Level.to_string(expected)})

@unsafe def check_former_equal(+got: Level.T, +expected: Level.T) -> B.Comp(Unit):
  do B.Comp<Unit>:
    same : Bool <- LevelEq.decide_m(got, expected, True{})
    former_equal_result(same,got,expected)


@unsafe def check_former_level(+level: Level.T, +ctx: C.Ctx, left: Bool, shape: Shape.T<Term.T>, diagram: Term.T) -> B.Comp(Unit):
  do B.Comp<Unit>:
    got : Level.T <- form(ctx, left, shape, diagram, Some{level})
    check_former_equal(got, level)


@unsafe def check_former_value(+expected: Value.T, ctx: C.Ctx, left: Bool, shape: Shape.T<Term.T>, diagram: Term.T) -> B.Comp(Unit):
  match expected:
    case Value.VUniv{level}:
      check_former_level(level, ctx, left, shape, diagram)
    case _:
      B.failed(Unit, Error.Universe{"a type former is checked against a type that is not a universe:  " ++ pp_value(ctx, expected)})

@unsafe def check_former(+ctx: C.Ctx, left: Bool, shape: Shape.T<Term.T>, diagram: Term.T, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    value : Value.T <- whnf(ctx, expected)
    ok : Unit <- check_former_value(value, ctx, left, shape, diagram)
    return Q.empty()

@unsafe def check_node(+ctx: C.Ctx, +mode: Q.T, term: Term.T, +expected: Value.T) -> B.Comp(Q.Usage):
  match term:
    case Term.Sec{s, legs}:
      intro_sec(ctx, mode, s, legs, expected)
    case Term.In{s, addr, args}:
      intro_in(ctx, mode, s, addr, args, expected)
    case Term.Elim{data}:
      do B.Comp<Q.Usage>:
        got : Inferred <- elim_elim(ctx, mode, data, Some{expected})
        check_inferred(got, ctx, expected)
    case Term.Lan{s, d}:
      check_former(ctx, True{}, s, d, expected)
    case Term.Ran{s, d}:
      check_former(ctx, False{}, s, d, expected)
    case Term.Let{name, ty, value, body}:
      do B.Comp<Q.Usage>:
        got : Inferred <- let_body(ctx, mode, name, ty, value, body, Some{expected})
        return inferred_uses(got)
    case other:
      do B.Comp<Q.Usage>:
        got : Inferred <- infer_uses(ctx, mode, other)
        check_inferred(got, ctx, expected)

@unsafe def check_uses(ctx, mode, term, expected):
  do B.Comp<Q.Usage>:
    inner : C.Ctx <- scope(ctx, term)
    check_node(inner, mode, term, expected)

@unsafe def linear_used(locals: C.LView, +size: Nat, +uses: Q.Usage) -> Bool:
  match locals size:
    case C.LVCons{C.Local{name, q, ty}, rest} 1n+level:
      Bool.or(Bool.and(Q.equal(q, Q.QOne{}), Q.used(level, uses)), linear_used(C.lview(rest), level, uses))
    case _ _:
      False{}

def has_linear(ctx: C.Ctx, uses: Q.Usage) -> Bool:
  match ctx:
    case C.Ctx{g, env, locals, size, budget, arity, checked}:
      linear_used(C.lview(locals), size, uses)

@unsafe def let_finish(result: Inferred, inner: C.Ctx, size: Nat, +mode: Q.T, def_uses: Q.Usage, linear: Bool) -> B.Comp(Inferred):
  match result:
    case Inferred{ty, uses}:
      do B.Comp<Inferred>:
        free : Q.Usage <- close(inner, size, mode, uses, linear)
        return Inferred{ty, Q.sequence(Q.scale(mode, def_uses), free)}

@unsafe def let_inner(expected: Maybe<&2, Value.T>, +inner: C.Ctx, +mode: Q.T, body: Term.T) -> B.Comp(Inferred):
  match expected:
    case None{}:
      infer_uses(inner, mode, body)
    case Some{+ty}:
      do B.Comp<Inferred>:
        uses : Q.Usage <- check_uses(inner, mode, body, ty)
        return Inferred{ty, uses}

@unsafe def let_defined(+ctx: C.Ctx, +mode: Q.T, name: String, ty: Value.T, value: Value.T, body: Term.T, expected: Maybe<&2, Value.T>, +uses: Q.Usage) -> B.Comp(Inferred):
  +linear = has_linear(ctx, uses)
  q = Bool.pick(Q.T, Q.equal(mode, Q.QZero{}), Q.QZero{}, Bool.pick(Q.T, linear, Q.QOne{}, Q.QMany{}))
  +inner = C.define(name, q, ty, value, ctx)
  do B.Comp<Inferred>:
    result : Inferred <- let_inner(expected, inner, mode, body)
    let_finish(result, inner, C.size_of(ctx), mode, uses, linear)

@unsafe def let_typed(+ctx: C.Ctx, +mode: Q.T, name: String, +ty: Value.T, +value: Term.T, body: Term.T, expected: Maybe<&2, Value.T>) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    uses : Q.Usage <- check_uses(ctx, Q.runtime(mode), value, ty)
    val : Value.T <- eval(ctx, value)
    let_defined(ctx, mode, name, ty, val, body, expected, uses)


@unsafe def let_body(ctx, mode, name, ty, value, body, expected):
  do B.Comp<Inferred>:
    level : Level.T <- infer_univ(ctx, ty)
    tyv : Value.T <- eval(ctx, ty)
    let_typed(ctx, mode, name, tyv, value, body, expected)


def former_value(value: Value.T, left: Bool, message: String) -> B.Comp(Former):
  match value left:
    case Value.VLan{+s, +d, +u} True{}:
      B.pure(Former, Former{Value.VLan{s, d, u}, s, d, u})
    case Value.VRan{+s, +d, +u} False{}:
      B.pure(Former, Former{Value.VRan{s, d, u}, s, d, u})
    case _ _:
      B.failed(Former, Error.Mismatch{message})

def former_view(ctx: C.Ctx, left: Bool, value: Value.T, message: String) -> B.Comp(Former):
  do B.Comp<Former>:
    reduced : Value.T <- whnf(ctx, value)
    former_value(reduced, left, message)

def former_shape(f: Former) -> Shape.T<Value.T>:
  match f:
    case Former{whole, shape, clo, level}:
      shape

def former_clo(f: Former) -> Value.Closure<Value.T>:
  match f:
    case Former{whole, shape, clo, level}:
      clo

def former_whole(f: Former) -> Value.T:
  match f:
    case Former{whole, shape, clo, level}:
      whole

def view_point(shape: Shape.T<Value.T>) -> B.Comp(Point):
  match shape:
    case Shape.SPi{q, name, dom}:
      B.pure(Point, Point{q, name, dom})
    case Shape.SZk{q, name, dom}:
      B.pure(Point, Point{q, name, dom})
    case _:
      B.failed(Point, Error.Mismatch{"the rule pack does not match the shape of the term"})

def point_q(point: Point) -> Q.T:
  match point:
    case Point{q, name, dom}:
      q

def point_name(point: Point) -> String:
  match point:
    case Point{q, name, dom}:
      name

def point_dom(point: Point) -> Value.T:
  match point:
    case Point{q, name, dom}:
      dom

def view_coll(shape: Shape.T<Value.T>) -> B.Comp(Big.T):
  match shape:
    case Shape.SColl{count}:
      B.pure(Big.T, count)
    case _:
      B.failed(Big.T, Error.Mismatch{"the rule pack does not match the shape of the term"})

def one(-A: Data, items: List<&2, A>, error: Error.T) -> B.Comp(A):
  match items:
    case Con{x, Nil{}}:
      B.pure(A, x)
    case _:
      B.failed(A, error)

def binder_quantity(subject: String, source: String, name: String, +marked: Q.T, +declared: Q.T) -> B.Comp(Unit):
  require_by(Q.equal(marked, declared), unit => Error.Quantity{"the " ++ subject ++ " " ++ name ++ " is marked " ++ Q.to_string(marked) ++ " and the " ++ source ++ " marks it " ++ Q.to_string(declared)})

def leg_body(leg: Term.Leg<Term.T>) -> Term.T:
  match leg:
    case Term.Leg{binders, body}:
      body

def leg_binders(leg: Term.Leg<Term.T>) -> List<&2, Pair.Pair<Q.T, String>>:
  match leg:
    case Term.Leg{binders, body}:
      binders

def pair_q(pair: Pair.Pair<Q.T, String>) -> Q.T:
  match pair:
    case Pair.Pair{q, name}:
      q

def pair_name(pair: Pair.Pair<Q.T, String>) -> String:
  match pair:
    case Pair.Pair{q, name}:
      name

def point_arg(addr: Term.Addr<Term.T>, message: String) -> B.Comp(Term.T):
  match addr:
    case Term.APt{q, arg}:
      B.pure(Term.T, arg)
    case _:
      B.failed(Term.T, Error.Wrong_leg{message})

def leg_index(addr: Term.Addr<Term.T>, message: String) -> B.Comp(Big.T):
  optional(Big.T, Term.as_aleg(Term.T, addr), Error.Wrong_leg{message})

@unsafe def point_form(+ctx: C.Ctx, left: Bool, q: Q.T, name: String, +dom: Term.T, diagram: Term.T) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    +dom_level : Level.T <- infer_univ(ctx, dom)
    dom_value : Value.T <- eval(ctx, dom)
    +cod_level : Level.T <- infer_univ(C.bind(name, q, dom_value, ctx), diagram)
    return Bool.pick(Level.T, left, Level.max(dom_level, cod_level), Level.imax(dom_level, cod_level))

@unsafe def point_intro_sec(+ctx: C.Ctx, +mode: Q.T, legs: List<&2, Term.Leg<Term.T>>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, False{}, expected, "a section needs a right former as its expected type")
    +point : Point <- view_point(former_shape(f))
    +leg : Term.Leg<Term.T> <- one(Term.Leg<Term.T>, legs, Error.Mismatch{"a section at the point shape has one leg"})
    +binder : Pair.Pair<Q.T, String> <- one(Pair.Pair<Q.T, String>, leg_binders(leg), Error.Mismatch{"the leg binds the point once"})
    ok : Unit <- binder_quantity("binder", "type", pair_name(binder), pair_q(binder), point_q(point))
    cod : Value.T <- open(ctx, former_clo(f), [Value.var(C.size_of(ctx))])
    +inner : C.Ctx = C.bind(pair_name(binder), point_q(point), point_dom(point), ctx)
    uses : Q.Usage <- check_uses(inner, Q.runtime(mode), leg_body(leg), cod)
    free : Q.Usage <- close(inner, C.size_of(ctx), mode, uses, False{})
    return Q.scale(mode, Q.captures(free))

@unsafe def point_intro_in(+ctx: C.Ctx, +mode: Q.T, addr: Term.Addr<Term.T>, args: List<&2, Term.T>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, True{}, expected, "a pair needs a left former as its expected type")
    +point : Point <- view_point(former_shape(f))
    +arg : Term.T <- point_arg(addr, "a pair takes the point address")
    fibre : Term.T <- one(Term.T, args, Error.Mismatch{"a pair carries one fibre element"})
    point_uses : Q.Usage <- check_uses(ctx, Q.mul(mode, point_q(point)), arg, point_dom(point))
    value : Value.T <- eval(ctx, arg)
    cod : Value.T <- open(ctx, former_clo(f), [value])
    fibre_uses : Q.Usage <- check_uses(ctx, mode, fibre, cod)
    return Q.sequence(point_uses, fibre_uses)

@unsafe def point_elim_out(+ctx: C.Ctx, +mode: Q.T, addr: Term.Addr<Term.T>, head: Term.T) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    +arg : Term.T <- point_arg(addr, "an application takes the point address")
    +got : Inferred <- infer_uses(ctx, mode, head)
    +f : Former <- former_view(ctx, False{}, inferred_ty(got), "the head of an application is not a function")
    +point : Point <- view_point(former_shape(f))
    uses : Q.Usage <- check_uses(ctx, Q.mul(mode, point_q(point)), arg, point_dom(point))
    value : Value.T <- eval(ctx, arg)
    result : Value.T <- open(ctx, former_clo(f), [value])
    return Inferred{result, Q.sequence(inferred_uses(got), uses)}

@unsafe def elim_result(motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>, +ctx: C.Ctx, self: Value.T) -> B.Comp(Value.T):
  match motive:
    case None{}:
      optional(Value.T, expected, Error.Cannot_infer{"an elimination without a motive needs an expected type"})
    case Some{Term.Motive{ind, indices, name, body}}:
      B.lift(Value.T, Eval.eval(C.globals_of(ctx), self <> C.env_of(ctx), body))

@unsafe def check_motive(motive: Maybe<&2, Term.Motive<Term.T>>, ctx: C.Ctx, scrut_ty: Value.T) -> B.Comp(Unit):
  match motive:
    case None{}:
      B.pure(Unit, Unit{})
    case Some{Term.Motive{ind, indices, name, body}}:
      do B.Comp<Unit>:
        level : Level.T <- infer_univ(C.bind(name, Q.QZero{}, scrut_ty, ctx), body)
        return Unit{}

def elim_motive(data: Term.ElimData<Term.T>) -> Maybe<&2, Term.Motive<Term.T>>:
  match data:
    case Term.ElimData{s, scrut, q, motive, branches}:
      motive

def elim_scrut(data: Term.ElimData<Term.T>) -> Term.T:
  match data:
    case Term.ElimData{s, scrut, q, motive, branches}:
      scrut

def elim_branches(data: Term.ElimData<Term.T>) -> List<&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>:
  match data:
    case Term.ElimData{s, scrut, q, motive, branches}:
      branches

@unsafe def infer_scrutinee(ctx: C.Ctx, +mode: Q.T, data: Term.ElimData<Term.T>) -> B.Comp(Inferred):
  match data:
    case Term.ElimData{s, scrut, +q, motive, branches}:
      do B.Comp<Inferred>:
        ok : Unit <- require(Bool.or(Q.equal(mode, Q.QZero{}), Bool.not(Q.equal(q, Q.QZero{}))), Error.Quantity{"an erased scrutinee cannot be eliminated at runtime"})
        infer_uses(ctx, Q.mul(mode, q), scrut)

def first_branch_pair(branch: Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>) -> B.Comp(Term.Leg<Term.T>):
  match branch:
    case Pair.Pair{addr, leg}:
      do B.Comp<Term.Leg<Term.T>>:
        index : Big.T <- leg_index(addr, "the branch takes the leg address")
        ok : Unit <- require(Big.equal(index, Big.zero()), Error.Wrong_leg{"the leg address is outside the collection"})
        return leg

@unsafe def point_branch(leg: Term.Leg<Term.T>, +ctx: C.Ctx, +mode: Q.T, +f: Former, +point: Point, motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>) -> B.Comp(Q.Usage):
  match leg:
    case Term.Leg{Con{Pair.Pair{+q1, +x1}, Con{Pair.Pair{q2, x2}, Nil{}}}, body}:
      +v1 = Value.var(C.size_of(ctx))
      +v2 = Value.var(Nat.add(C.size_of(ctx), 1n))
      do B.Comp<Q.Usage>:
        ok : Unit <- binder_quantity("branch binder", "type", x1, q1, point_q(point))
        cod : Value.T <- open(ctx, former_clo(f), [v1])
        +inner : C.Ctx = C.bind(x2, q2, cod, C.bind(x1, q1, point_dom(point), ctx))
        target : Value.T <- elim_result(motive, expected, ctx, Value.VIn{former_shape(f), Value.VAPt{point_q(point), v1}, [v2]})
        uses : Q.Usage <- check_uses(inner, Q.runtime(mode), body, target)
        close(inner, C.size_of(ctx), mode, uses, False{})
    case _:
      B.failed(Q.Usage, Error.Missing_branch{"the branch binds the point and the fibre element (D-M0-3)"})

@unsafe def point_elim_elim(+ctx: C.Ctx, +mode: Q.T, +data: Term.ElimData<Term.T>, +expected: Maybe<&2, Value.T>) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    +got : Inferred <- infer_scrutinee(ctx, mode, data)
    +f : Former <- former_view(ctx, True{}, inferred_ty(got), "the scrutinee is not a left former")
    +point : Point <- view_point(former_shape(f))
    ok : Unit <- check_motive(elim_motive(data), ctx, former_whole(f))
    scrut : Value.T <- eval(ctx, elim_scrut(data))
    result : Value.T <- elim_result(elim_motive(data), expected, ctx, scrut)
    branch : Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>> <- one(Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>, elim_branches(data), Error.Missing_branch{"an elimination at the point shape has one branch"})
    leg : Term.Leg<Term.T> <- first_branch_pair(branch)
    uses : Q.Usage <- point_branch(leg, ctx, mode, f, point, elim_motive(data), expected)
    return Inferred{result, Q.sequence(inferred_uses(got), Q.scale(mode, uses))}

@unsafe def coll_levels(legs: List<&2, Term.Leg<Term.T>>, +ctx: C.Ctx, level: Level.T) -> B.Comp(Level.T):
  match legs:
    case Nil{}:
      B.pure(Level.T, level)
    case Con{Term.Leg{binders, body}, rest}:
      do B.Comp<Level.T>:
        ok : Unit <- require(List.is_empty(&2, Pair.Pair<Q.T, String>, binders), Error.Mismatch{"a diagram leg binds nothing"})
        next : Level.T <- infer_univ(ctx, body)
        coll_levels(rest, ctx, Level.max(level, next))

def expected_level(expected: Maybe<&2, Level.T>) -> Level.T:
  match expected:
    case None{}:
      Level.zero()
    case Some{level}:
      level

@unsafe def coll_form(ctx: C.Ctx, +count: Big.T, diagram: Term.T, expected: Maybe<&2, Level.T>) -> B.Comp(Level.T):
  match diagram:
    case Term.Sec{Shape.SColl{n}, +legs}:
      do B.Comp<Level.T>:
        ok : Unit <- require(Bool.and(Big.equal(n, count), Big.equal(Big.of_nat(List.length(&2, Term.Leg<Term.T>, legs)), count)), Error.Mismatch{"the diagram is not a section of the declared width"})
        coll_levels(legs, ctx, Bool.pick(Level.T, Big.equal(count, Big.zero()), expected_level(expected), Level.zero()))
    case _:
      B.failed(Level.T, Error.Mismatch{"the diagram is not a section of the declared width"})

@unsafe def coll_check_legs(legs: List<&2, Term.Leg<Term.T>>, dlegs: List<&2, Value.VLeg<Value.T>>, +ctx: C.Ctx, +mode: Q.T) -> B.Comp(Q.Usage):
  match legs dlegs:
    case Nil{} Nil{}:
      B.pure(Q.Usage, Q.empty())
    case Con{Term.Leg{binders, body}, rest} Con{Value.VLeg{bs, clo}, drest}:
      do B.Comp<Q.Usage>:
        ty : Value.T <- open(ctx, clo, [])
        uses : Q.Usage <- check_uses(ctx, mode, body, ty)
        others : Q.Usage <- coll_check_legs(rest, drest, ctx, mode)
        return Q.sequence(uses, others)
    case _ _:
      B.failed(Q.Usage, Error.Mismatch{"the diagram is not a section of the declared width"})

@unsafe def coll_intro_sec(+ctx: C.Ctx, mode: Q.T, +count: Big.T, +legs: List<&2, Term.Leg<Term.T>>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, False{}, expected, "a tuple needs a right former as its expected type")
    width : Big.T <- view_coll(former_shape(f))
    ok : Unit <- require(Bool.and(Big.equal(width, count), Big.equal(Big.of_nat(List.length(&2, Term.Leg<Term.T>, legs)), count)), Error.Mismatch{"the tuple width and the type width differ"})
    dlegs : List<&2, Value.VLeg<Value.T>> <- B.lift(List<&2, Value.VLeg<Value.T>>, R.coll_legs_of(ctx, former_clo(f)))
    coll_check_legs(legs, dlegs, ctx, mode)

@unsafe def coll_intro_in(+ctx: C.Ctx, mode: Q.T, addr: Term.Addr<Term.T>, args: List<&2, Term.T>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, True{}, expected, "an injection needs a left former as its expected type")
    count : Big.T <- view_coll(former_shape(f))
    +index : Big.T <- leg_index(addr, "an injection takes the leg address")
    ok : Unit <- require(Bool.and(Big.le(Big.zero(),index),Bool.not(Big.le(count,index))), Error.Wrong_leg{"the leg address is outside the collection"})
    dlegs : List<&2, Value.VLeg<Value.T>> <- B.lift(List<&2, Value.VLeg<Value.T>>, R.coll_legs_of(ctx, former_clo(f)))
    ty : Value.T <- B.lift(Value.T, R.coll_leg_ty(ctx, dlegs, index))
    payload : Term.T <- one(Term.T, args, Error.Mismatch{"an injection carries one payload"})
    check_uses(ctx, mode, payload, ty)

@unsafe def coll_elim_out(+ctx: C.Ctx, mode: Q.T, addr: Term.Addr<Term.T>, head: Term.T) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    +index : Big.T <- leg_index(addr, "a projection takes the leg address")
    +got : Inferred <- infer_uses(ctx, mode, head)
    +f : Former <- former_view(ctx, False{}, inferred_ty(got), "a projection needs a right former")
    count : Big.T <- view_coll(former_shape(f))
    ok : Unit <- require(Bool.and(Big.le(Big.zero(),index),Bool.not(Big.le(count,index))), Error.Wrong_leg{"the leg address is outside the collection"})
    dlegs : List<&2, Value.VLeg<Value.T>> <- B.lift(List<&2, Value.VLeg<Value.T>>, R.coll_legs_of(ctx, former_clo(f)))
    ty : Value.T <- B.lift(Value.T, R.coll_leg_ty(ctx, dlegs, index))
    return Inferred{ty, inferred_uses(got)}

@unsafe def coll_branch(+ctx: C.Ctx, +mode: Q.T, +data: Term.ElimData<Term.T>, shape: Shape.T<Value.T>, dlegs: List<&2, Value.VLeg<Value.T>>, expected: Maybe<&2, Value.T>, +index: Nat) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +leg : Term.Leg<Term.T> <- optional_by(Term.Leg<Term.T>, Eval.find_branch(elim_branches(data), Value.valeg(Value.T,index)), unit => Error.Missing_branch{"no branch at leg " ++ Nat.show(index)})
    +binder : Pair.Pair<Q.T, String> <- one(Pair.Pair<Q.T, String>, leg_binders(leg), Error.Missing_branch{"each branch binds its payload once"})
    ty : Value.T <- B.lift(Value.T, R.coll_leg_ty(ctx, dlegs, Big.of_nat(index)))
    target : Value.T <- elim_result(elim_motive(data), expected, ctx, Value.VIn{shape, Value.valeg(Value.T,index), [Value.var(C.size_of(ctx))]})
    +inner : C.Ctx = C.bind(pair_name(binder), pair_q(binder), ty, ctx)
    uses : Q.Usage <- check_uses(inner, Q.runtime(mode), leg_body(leg), target)
    close(inner, C.size_of(ctx), mode, uses, False{})

@unsafe def coll_branches(count: Nat, +index: Nat, +ctx: C.Ctx, +mode: Q.T, +data: Term.ElimData<Term.T>, +shape: Shape.T<Value.T>, +dlegs: List<&2, Value.VLeg<Value.T>>, +expected: Maybe<&2, Value.T>, uses: Q.Usage) -> B.Comp(Q.Usage):
  match count:
    case 0n:
      B.pure(Q.Usage, uses)
    case 1n+p:
      do B.Comp<Q.Usage>:
        branch : Q.Usage <- coll_branch(ctx, mode, data, shape, dlegs, expected, index)
        coll_branches(p, 1n+index, ctx, mode, data, shape, dlegs, expected, Q.alternative(uses, branch))

@unsafe def coll_elim_elim(+ctx: C.Ctx, +mode: Q.T, +data: Term.ElimData<Term.T>, +expected: Maybe<&2, Value.T>) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    +got : Inferred <- infer_scrutinee(ctx, mode, data)
    +f : Former <- former_view(ctx, True{}, inferred_ty(got), "the scrutinee is not a left former")
    +count : Big.T <- view_coll(former_shape(f))
    ok : Unit <- check_motive(elim_motive(data), ctx, former_whole(f))
    scrut : Value.T <- eval(ctx, elim_scrut(data))
    result : Value.T <- elim_result(elim_motive(data), expected, ctx, scrut)
    +branches_count : Nat = List.length(&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>, elim_branches(data))
    ok2 : Unit <- require_by(Big.equal(Big.of_nat(branches_count), count), unit => Error.Missing_branch{"the case has " ++ Nat.show(branches_count) ++ " branches and the type has " ++ Big.to_string(count) ++ " legs"})
    dlegs : List<&2, Value.VLeg<Value.T>> <- B.lift(List<&2, Value.VLeg<Value.T>>, R.coll_legs_of(ctx, former_clo(f)))
    uses : Q.Usage <- coll_branches(branches_count, 0n, ctx, mode, data, former_shape(f), dlegs, expected, Q.unreachable())
    return Inferred{result, Q.sequence(inferred_uses(got), Q.scale(mode, uses))}

type Mu is Data:
  Mu{name: String, indices: List<&2, Value.T>}

type EnvUses is Data:
  EnvUses{env: List<&2, Value.T>, uses: Q.Usage}

type Fields is Data:
  Fields{ctx: C.Ctx, env: List<&2, Value.T>, values: List<&2, Value.T>}

def mu_view(shape: Shape.T<Value.T>) -> B.Comp(Mu):
  match shape:
    case Shape.SMu{name, indices}:
      B.pure(Mu, Mu{name, indices})
    case _:
      B.failed(Mu, Error.Mismatch{"the rule pack does not match the shape of the term"})

def mu_name(value: Mu) -> String:
  match value:
    case Mu{name, indices}:
      name

def mu_index_values(value: Mu) -> List<&2, Value.T>:
  match value:
    case Mu{name, indices}:
      indices

def fam_params(f: Positivity.Family) -> Positivity.Telescope:
  match f:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      params

def fam_indices(f: Positivity.Family) -> Positivity.Telescope:
  match f:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      indices

def fam_level(f: Positivity.Family) -> Level.T:
  match f:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      level

def ctor_args(c: Positivity.Ctor) -> Positivity.Telescope:
  match c:
    case Positivity.Ctor{name, args, idx, arity, rec}:
      args

def ctor_indices(c: Positivity.Ctor) -> List<&2, Term.T>:
  match c:
    case Positivity.Ctor{name, args, idx, arity, rec}:
      idx

def env_values(value: EnvUses) -> List<&2, Value.T>:
  match value:
    case EnvUses{env, uses}:
      env

def env_uses(value: EnvUses) -> Q.Usage:
  match value:
    case EnvUses{env, uses}:
      uses

def fields_ctx(value: Fields) -> C.Ctx:
  match value:
    case Fields{ctx, env, vals}:
      ctx

def fields_env(value: Fields) -> List<&2, Value.T>:
  match value:
    case Fields{ctx, env, vals}:
      env

def fields_values(value: Fields) -> List<&2, Value.T>:
  match value:
    case Fields{ctx, env, vals}:
      vals

def params_of(diagram: Term.T) -> B.Comp(List<&2, Term.T>):
  match diagram:
    case Term.Sec{shape, legs}:
      B.pure(List<&2, Term.T>, Term.leg_children(legs))
    case _:
      B.failed(List<&2, Term.T>, Error.Mismatch{"the diagram is not a section of the declared width"})

@unsafe def telescope_loop(tele: Positivity.Telescope, args: List<&2, Term.T>, +ctx: C.Ctx, +mode: Q.T, +env: List<&2, Value.T>, uses: Q.Usage) -> B.Comp(EnvUses):
  match tele args:
    case Nil{} Nil{}:
      B.pure(EnvUses, EnvUses{env, uses})
    case Con{Positivity.Binder{q, name, ty}, rest} Con{+arg, others}:
      do B.Comp<EnvUses>:
        tyv : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), env, ty))
        next_uses : Q.Usage <- check_uses(ctx, Q.mul(mode, q), arg, tyv)
        value : Value.T <- eval(ctx, arg)
        telescope_loop(rest, others, ctx, mode, value <> env, Q.sequence(uses, next_uses))
    case _ _:
      B.failed(EnvUses, Error.Mismatch{"the telescope has the wrong arity"})

@unsafe def telescope_uses(ctx: C.Ctx, mode: Q.T, what: String, +tele: Positivity.Telescope, +args: List<&2, Term.T>, env: List<&2, Value.T>) -> B.Comp(EnvUses):
  +want = List.length(&2, Positivity.Binder, tele)
  +got = List.length(&2, Term.T, args)
  do B.Comp<EnvUses>:
    ok : Unit <- require_by(Nat.is_eq(want, got), unit => Error.Mismatch{what ++ " takes " ++ Nat.show(want) ++ " arguments and the term gives " ++ Nat.show(got)})
    telescope_loop(tele, args, ctx, mode, env, Q.empty())

@unsafe def mu_form(+ctx: C.Ctx, +name: String, indices: List<&2, Term.T>, diagram: Term.T) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    +fam : Positivity.Family <- B.lift(Positivity.Family, R.mu_family(ctx, name))
    params : List<&2, Term.T> <- params_of(diagram)
    penv : EnvUses <- telescope_uses(ctx, Q.QZero{}, name, fam_params(fam), params, [])
    done : EnvUses <- telescope_uses(ctx, Q.QZero{}, "the indices of " ++ name, fam_indices(fam), indices, env_values(penv))
    return fam_level(fam)

def index_equal_result(same: Bool, +ctx: C.Ctx, ctor: String, name: String, got: Value.T, want: Value.T) -> B.Comp(Unit):
  match same:
    case True{}: B.pure(Unit,Unit{})
    case False{}: B.failed(Unit,Error.Mismatch{"the constructor " ++ ctor ++ " of " ++ name ++ " gives the index " ++ pp_value(ctx,got) ++ " and the type asks for " ++ pp_value(ctx,want)})

@unsafe def index_loop(tele: Positivity.Telescope, results: List<&2, Term.T>, wants: List<&2, Value.T>, +ctx: C.Ctx, +name: String, +ctor: String, +env: List<&2, Value.T>, +index_env: List<&2, Value.T>) -> B.Comp(Unit):
  match tele results wants:
    case Nil{} Nil{} Nil{}:
      B.pure(Unit, Unit{})
    case Con{Positivity.Binder{q, x, ty}, ts} Con{result, rs} Con{+want, ws}:
      do B.Comp<Unit>:
        tyv : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), index_env, ty))
        +got : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), env, result))
        eq : Bool <- conv(ctx, tyv, got, want)
        ok : Unit <- index_equal_result(eq,ctx,ctor,name,got,want)
        index_loop(ts, rs, ws, ctx, name, ctor, env, want <> index_env)
    case _ _ _:
      B.failed(Unit, Error.Mismatch{"the family index telescope has the wrong arity"})

@unsafe def mu_indices(ctx: C.Ctx, +name: String, +ctor: String, tele: Positivity.Telescope, +results: List<&2, Term.T>, penv: List<&2, Value.T>, +wants: List<&2, Value.T>, env: List<&2, Value.T>) -> B.Comp(Unit):
  +count = List.length(&2, Term.T, results)
  +want = List.length(&2, Value.T, wants)
  do B.Comp<Unit>:
    ok : Unit <- require_by(Nat.is_eq(count, want), unit => Error.Mismatch{ctor ++ " gives " ++ Nat.show(count) ++ " result indices and " ++ name ++ " takes " ++ Nat.show(want)})
    index_loop(tele, results, wants, ctx, name, ctor, env, penv)

@unsafe def mu_intro_in(+ctx: C.Ctx, mode: Q.T, addr: Term.Addr<Term.T>, args: List<&2, Term.T>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, True{}, expected, "a constructor needs a left former")
    +mu : Mu <- mu_view(former_shape(f))
    +ctor : String <- optional(String, Term.as_actor(Term.T, addr), Error.Wrong_leg{"a constructor takes a constructor address"})
    +fam : Positivity.Family <- B.lift(Positivity.Family, R.mu_family(ctx, mu_name(mu)))
    +ct : Positivity.Ctor <- optional_by(Positivity.Ctor, Positivity.ctor_of(ctor, fam), unit => Error.Unbound{ctor ++ " is not a constructor of " ++ mu_name(mu)})
    +penv : List<&2, Value.T> <- B.lift(List<&2, Value.T>, R.mu_param_env(ctx, former_clo(f)))
    +got : EnvUses <- telescope_uses(ctx, mode, ctor, ctor_args(ct), args, penv)
    ok : Unit <- mu_indices(ctx, mu_name(mu), ctor, fam_indices(fam), ctor_indices(ct), penv, mu_index_values(mu), env_values(got))
    return env_uses(got)

def motive_indices(motive: Term.Motive<Term.T>) -> List<&2, String>:
  match motive:
    case Term.Motive{ind, indices, self, body}:
      indices

def motive_body(motive: Term.Motive<Term.T>) -> Term.T:
  match motive:
    case Term.Motive{ind, indices, self, body}:
      body

def motive_self(motive: Term.Motive<Term.T>) -> String:
  match motive:
    case Term.Motive{ind, indices, self, body}:
      self

def motive_ind(motive: Term.Motive<Term.T>) -> Maybe<&2, String>:
  match motive:
    case Term.Motive{ind, indices, self, body}:
      ind

@unsafe def mu_motive_of(+name: String, fam: Positivity.Family, +indices: List<&2, Value.T>, data: Term.ElimData<Term.T>) -> B.Comp(Term.Motive<Term.T>):
  +want = List.length(&2, Positivity.Binder, fam_indices(fam))
  do B.Comp<Term.Motive<Term.T>>:
    +motive : Term.Motive<Term.T> <- optional(Term.Motive<Term.T>, elim_motive(data), Error.Cannot_infer{"an elimination at a mu shape needs a motive"})
    +mn : String <- optional_by(String, motive_ind(motive), unit => Error.Mismatch{"the motive of an elimination at " ++ name ++ " names no family"})
    ok1 : Unit <- require_by(String.eq(mn, name), unit => Error.Mismatch{"the motive is built for " ++ mn ++ " and the scrutinee is at " ++ name})
    +got : Nat = List.length(&2, String, motive_indices(motive))
    ok2 : Unit <- require_by(Nat.is_eq(got, want), unit => Error.Mismatch{"the motive of " ++ name ++ " binds " ++ Nat.show(got) ++ " indices and the family has " ++ Nat.show(want)})
    +actual : Nat = List.length(&2, Value.T, indices)
    ok3 : Unit <- require_by(Nat.is_eq(actual, want), unit => Error.Mismatch{name ++ " is applied to " ++ Nat.show(actual) ++ " indices and the family has " ++ Nat.show(want)})
    return motive

def mu_result(+ctx: C.Ctx, motive: Term.Motive<Term.T>, indices: List<&2, Value.T>, self: Value.T) -> B.Comp(Value.T):
  B.lift(Value.T, Eval.eval(C.globals_of(ctx), self <> List.append(&2, Value.T, List.reverse(&2, Value.T, indices), C.env_of(ctx)), motive_body(motive)))

@unsafe def motive_bind_indices(tele: Positivity.Telescope, names: List<&2, String>, +ctx: C.Ctx, +env: List<&2, Value.T>, values: List<&2, Value.T>, +family: String) -> B.Comp(Fields):
  match tele names:
    case Nil{} Nil{}:
      B.pure(Fields, Fields{ctx, env, values})
    case Con{Positivity.Binder{q, x, ty}, rest} Con{name, others}:
      +value = Value.var(C.size_of(ctx))
      do B.Comp<Fields>:
        tyv : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), env, ty))
        motive_bind_indices(rest, others, C.bind(name, Q.QZero{}, tyv, ctx), value <> env, value <> values, family)
    case _ _:
      B.failed(Fields, Error.Mismatch{"the motive of " ++ family ++ " does not bind the indices"})

def former_level(f: Former) -> Maybe<&2, Level.T>:
  match f:
    case Former{whole, shape, clo, level}:
      level

@unsafe def mu_motive_level(ctx: C.Ctx, +name: String, fam: Positivity.Family, +motive: Term.Motive<Term.T>, +f: Former, penv: List<&2, Value.T>) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    +fields : Fields <- motive_bind_indices(fam_indices(fam), motive_indices(motive), ctx, penv, [], name)
    infer_univ(C.bind(motive_self(motive), Q.QZero{}, Value.VLan{Shape.SMu{name, List.reverse(&2, Value.T, fields_values(fields))}, former_clo(f), former_level(f)}, fields_ctx(fields)), motive_body(motive))

def erased_fields(tele: Positivity.Telescope) -> Bool:
  match tele:
    case Nil{}:
      True{}
    case Con{Positivity.Binder{q, name, ty}, rest}:
      Bool.and(Q.equal(q, Q.QZero{}), erased_fields(rest))

def zero_ctor(ctor: Maybe<&2, Positivity.Ctor>) -> Bool:
  match ctor:
    case Some{Positivity.Ctor{name, args, idx, arity, recursive}}:
      Bool.and(erased_fields(args), Bool.not(recursive))
    case None{}:
      False{}

def mu_zero_eliminable(fam: Positivity.Family) -> Bool:
  match fam:
    case Positivity.Family{name, params, indices, level, Positivity.Complete{Nil{}}, ctors, positive}:
      True{}
    case Positivity.Family{name, params, indices, level, Positivity.Complete{Con{ctor, Nil{}}}, ctors, positive}:
      zero_ctor(Positivity.find_ctor(ctors, ctor))
    case _:
      False{}

def mu_large(name: String, +fam: Positivity.Family, level: Level.T) -> B.Comp(Unit):
  require_by(Bool.or(Level.always_positive(fam_level(fam)), Bool.or(Level.always_zero(level), mu_zero_eliminable(fam))), unit => Error.Universe{"a large elimination out of a proposition needs a subsingleton family at " ++ name})

def mu_ctor_names(fam: Positivity.Family, name: String) -> B.Comp(List<&2, String>):
  match fam:
    case Positivity.Family{n, params, indices, level, Positivity.Complete{names}, ctors, positive}:
      B.pure(List<&2, String>, names)
    case Positivity.Family{n, params, indices, level, Positivity.Provisional{}, ctors, positive}:
      B.failed(List<&2, String>, Error.Unbound{"the family " ++ name ++ " is still under declaration"})
    case Positivity.Family{n, params, indices, level, Positivity.Builtin{}, ctors, positive}:
      B.failed(List<&2, String>, Error.Mismatch{"the family " ++ name ++ " is a kernel family and has no branches"})

def branch_keys(branches: List<&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +name: String) -> B.Comp(List<&2, String>):
  match branches:
    case Nil{}:
      B.pure(List<&2, String>, [])
    case Con{Pair.Pair{Term.ACtor{key}, leg}, rest}:
      do B.Comp<List<&2, String>>:
        keys : List<&2, String> <- branch_keys(rest, name)
        return key <> keys
    case _:
      B.failed(List<&2, String>, Error.Wrong_leg{"a branch of " ++ name ++ " takes the constructor address"})

def count_name(keys: List<&2, String>, +name: String) -> Nat:
  match keys:
    case Nil{}:
      0n
    case Con{key, rest}:
      Nat.add(Bool.pick(Nat, String.eq(key, name), 1n, 0n), count_name(rest, name))

def cover_count(count: Nat, +name: String, +ctor: String) -> B.Comp(Unit):
  match count:
    case 0n:
      B.failed(Unit, Error.Missing_branch{"the elimination of " ++ name ++ " has no branch at " ++ ctor})
    case 1n+0n:
      B.pure(Unit, Unit{})
    case _:
      B.failed(Unit, Error.Wrong_leg{"the elimination of " ++ name ++ " repeats the branch at " ++ ctor})

def cover_names(names: List<&2, String>, +keys: List<&2, String>, +family: String) -> B.Comp(Unit):
  match names:
    case Nil{}:
      B.pure(Unit, Unit{})
    case Con{+name, rest}:
      do B.Comp<Unit>:
        ok : Unit <- cover_count(count_name(keys, name), family, name)
        cover_names(rest, keys, family)

def cover_known(keys: List<&2, String>, +names: List<&2, String>, +family: String) -> B.Comp(Unit):
  match keys:
    case Nil{}:
      B.pure(Unit, Unit{})
    case Con{+key, rest}:
      do B.Comp<Unit>:
        ok : Unit <- require_by(Term.member(names, key), unit => Error.Unbound{key ++ " is not a constructor of " ++ family})
        cover_known(rest, names, family)

def mu_cover(+name: String, +names: List<&2, String>, branches: List<&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> B.Comp(Unit):
  do B.Comp<Unit>:
    +keys : List<&2, String> <- branch_keys(branches, name)
    ok : Unit <- cover_names(names, keys, name)
    cover_known(keys, names, name)

@unsafe def branch_bind_fields(tele: Positivity.Telescope, binders: List<&2, Pair.Pair<Q.T, String>>, +ctx: C.Ctx, +env: List<&2, Value.T>, values: List<&2, Value.T>) -> B.Comp(Fields):
  match tele binders:
    case Nil{} Nil{}:
      B.pure(Fields, Fields{ctx, env, values})
    case Con{Positivity.Binder{+q, name, ty}, rest} Con{Pair.Pair{bq, +bx}, others}:
      +value = Value.var(C.size_of(ctx))
      do B.Comp<Fields>:
        tyv : Value.T <- B.lift(Value.T, Eval.eval(C.globals_of(ctx), env, ty))
        ok : Unit <- binder_quantity("branch binder", "field", bx, bq, q)
        branch_bind_fields(rest, others, C.bind(bx, q, tyv, ctx), value <> env, value <> values)
    case _ _:
      B.failed(Fields, Error.Missing_branch{"the branch binds the wrong number of fields"})

@unsafe def mu_branch(+ctx: C.Ctx, +mode: Q.T, +name: String, fam: Positivity.Family, motive: Term.Motive<Term.T>, penv: List<&2, Value.T>, branches: List<&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +ctor: String) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +ct : Positivity.Ctor <- optional_by(Positivity.Ctor, Positivity.ctor_of(ctor, fam), unit => Error.Unbound{ctor ++ " is not a constructor of " ++ name})
    +leg : Term.Leg<Term.T> <- optional_by(Term.Leg<Term.T>, Eval.find_branch(branches, Value.VACtor{ctor}), unit => Error.Missing_branch{"the elimination of " ++ name ++ " has no branch at " ++ ctor})
    +want : Nat = List.length(&2, Positivity.Binder, ctor_args(ct))
    +got : Nat = List.length(&2, Pair.Pair<Q.T, String>, leg_binders(leg))
    ok : Unit <- require_by(Nat.is_eq(want, got), unit => Error.Missing_branch{"the branch at " ++ ctor ++ " binds " ++ Nat.show(got) ++ " fields and " ++ ctor ++ " takes " ++ Nat.show(want)})
    +fields : Fields <- branch_bind_fields(ctor_args(ct), leg_binders(leg), ctx, penv, [])
    +indices : List<&2, Value.T> <- B.lift(List<&2, Value.T>, Eval.eval_list(ctor_indices(ct), C.globals_of(ctx), fields_env(fields)))
    target : Value.T <- mu_result(ctx, motive, indices, Value.VIn{Shape.SMu{name, indices}, Value.VACtor{ctor}, List.reverse(&2, Value.T, fields_values(fields))})
    uses : Q.Usage <- check_uses(fields_ctx(fields), Q.runtime(mode), leg_body(leg), target)
    close(fields_ctx(fields), C.size_of(ctx), mode, uses, False{})

@unsafe def mu_branches(names: List<&2, String>, +ctx: C.Ctx, +mode: Q.T, +name: String, +fam: Positivity.Family, +motive: Term.Motive<Term.T>, +penv: List<&2, Value.T>, +branches: List<&2, Pair.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, uses: Q.Usage) -> B.Comp(Q.Usage):
  match names:
    case Nil{}:
      B.pure(Q.Usage, uses)
    case Con{ctor, rest}:
      do B.Comp<Q.Usage>:
        next : Q.Usage <- mu_branch(ctx, mode, name, fam, motive, penv, branches, ctor)
        mu_branches(rest, ctx, mode, name, fam, motive, penv, branches, Q.alternative(uses, next))

@unsafe def mu_elim_elim(+ctx: C.Ctx, +mode: Q.T, +data: Term.ElimData<Term.T>) -> B.Comp(Inferred):
  do B.Comp<Inferred>:
    +got : Inferred <- infer_scrutinee(ctx, mode, data)
    +f : Former <- former_view(ctx, True{}, inferred_ty(got), "the scrutinee is not a left former")
    +mu : Mu <- mu_view(former_shape(f))
    +fam : Positivity.Family <- B.lift(Positivity.Family, R.mu_family(ctx, mu_name(mu)))
    +motive : Term.Motive<Term.T> <- mu_motive_of(mu_name(mu), fam, mu_index_values(mu), data)
    +penv : List<&2, Value.T> <- B.lift(List<&2, Value.T>, R.mu_param_env(ctx, former_clo(f)))
    level : Level.T <- mu_motive_level(ctx, mu_name(mu), fam, motive, f, penv)
    large : Unit <- mu_large(mu_name(mu), fam, level)
    +names : List<&2, String> <- mu_ctor_names(fam, mu_name(mu))
    covered : Unit <- mu_cover(mu_name(mu), names, elim_branches(data))
    uses : Q.Usage <- mu_branches(names, ctx, mode, mu_name(mu), fam, motive, penv, elim_branches(data), Q.unreachable())
    scrut : Value.T <- eval(ctx, elim_scrut(data))
    result : Value.T <- mu_result(ctx, motive, mu_index_values(mu), scrut)
    return Inferred{result, Q.sequence(inferred_uses(got), Q.scale(mode, uses))}

def definition_rows(entries: List<&2, Pair.Pair<String, Global.Entry>>) -> List<&2, Pair.Pair<String, Term.T>>:
  match entries:
    case Nil{}:
      []
    case Con{Pair.Pair{name, Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}}, rest}:
      Pair.Pair{name, body} <> definition_rows(rest)
    case Con{entry, rest}:
      definition_rows(rest)

def globals_rows(globals: Global.T) -> List<&2, Pair.Pair<String, Term.T>>:
  match globals:
    case Global.Globals{entries, families, entry_index, family_index}:
      definition_rows(entries)

def circuit_result(result: Result<&2, &2, String, Big.T>) -> B.Comp(Big.T):
  match result:
    case Done{value}:
      B.pure(Big.T, value)
    case Fail{message}:
      B.failed(Big.T, Error.Not_yet{Circuit.word(message)})

def circuit(ctx: C.Ctx, term: Term.T) -> B.Comp(Big.T):
  circuit_result(Circuit.depth(globals_rows(C.globals_of(ctx)), term))

@unsafe def zk_form(+ctx: C.Ctx, q: Q.T, name: String, +dom: Term.T, +diagram: Term.T) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    dom_level : Level.T <- infer_univ(ctx, dom)
    dom_value : Value.T <- eval(ctx, dom)
    +inner : C.Ctx = C.bind(name, q, dom_value, ctx)
    +cod_level : Level.T <- infer_univ(inner, diagram)
    ok : Unit <- require(Level.equal(cod_level, Level.zero()), Error.Mismatch{"a zk type hides the witness of a proposition"})
    depth : Big.T <- circuit(inner, diagram)
    return Level.max(dom_level, cod_level)

type ProofArgs is Data:
  ProofArgs{fibre: Term.T, uses: Q.Usage}

@unsafe def proof_args(args: List<&2, Term.T>, ctx: C.Ctx, mode: Q.T) -> B.Comp(ProofArgs):
  match args:
    case Con{fibre, Nil{}}:
      B.pure(ProofArgs, ProofArgs{fibre, Q.empty()})
    case Con{fibre, Con{inst, Nil{}}}:
      do B.Comp<ProofArgs>:
        got : Inferred <- infer_uses(ctx, mode, inst)
        return ProofArgs{fibre, inferred_uses(got)}
    case _:
      B.failed(ProofArgs, Error.Mismatch{"a proof carries one relation leg"})

@unsafe def zk_prove(args: ProofArgs, +ctx: C.Ctx, +mode: Q.T, dom: Value.T, clo: Value.Closure<Value.T>, +point: Term.T) -> B.Comp(Q.Usage):
  match args:
    case ProofArgs{fibre, instance_uses}:
      do B.Comp<Q.Usage>:
        point_uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QOne{}), point, dom)
        value : Value.T <- eval(ctx, point)
        cod : Value.T <- open(ctx, clo, [value])
        fibre_uses : Q.Usage <- check_uses(ctx, mode, fibre, cod)
        return Q.sequence(point_uses, Q.sequence(fibre_uses, instance_uses))

@unsafe def zk_intro_in(+ctx: C.Ctx, +mode: Q.T, addr: Term.Addr<Term.T>, args: List<&2, Term.T>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, True{}, expected, "a proof needs a left former as its expected type")
    point : Point <- view_point(former_shape(f))
    arg : Term.T <- point_arg(addr, "a proof takes the point address")
    proof : ProofArgs <- proof_args(args, ctx, mode)
    zk_prove(proof, ctx, mode, point_dom(point), former_clo(f), arg)

def fhc_shape(shape: Shape.T<Value.T>, message: String) -> B.Comp(Value.T):
  match shape:
    case Shape.SFhc{level}:
      B.pure(Value.T, level)
    case _:
      B.failed(Value.T, Error.Mismatch{message})

def literal_level(value: Value.T) -> B.Comp(Big.T):
  match value:
    case Value.VLit{Literal.LInt{level}}:
      B.pure(Big.T, level)
    case _:
      B.failed(Big.T, Error.Mismatch{"the level of a ciphertext type is a Nat literal"})

def fhc_level(ctx: C.Ctx, value: Value.T) -> B.Comp(Big.T):
  do B.Comp<Big.T>:
    reduced : Value.T <- whnf(ctx, value)
    literal_level(reduced)

law fhc_fragment:
  for +ctx: C.Ctx
  for ty: Value.T
  B.Comp(Unit)

@unsafe def fragment_legs(legs: List<&2, Value.VLeg<Value.T>>, +ctx: C.Ctx) -> B.Comp(Unit):
  match legs:
    case Nil{}:
      B.pure(Unit, Unit{})
    case Con{Value.VLeg{binders, clo}, rest}:
      do B.Comp<Unit>:
        value : Value.T <- open(ctx, clo, [])
        ok : Unit <- fhc_fragment(ctx, value)
        fragment_legs(rest, ctx)

@unsafe def fragment_former(shape: Shape.T<Value.T>, clo: Value.Closure<Value.T>, +ctx: C.Ctx) -> B.Comp(Unit):
  match shape:
    case Shape.SColl{count}:
      do B.Comp<Unit>:
        legs : List<&2, Value.VLeg<Value.T>> <- B.lift(List<&2, Value.VLeg<Value.T>>, R.coll_legs_of(ctx, clo))
        fragment_legs(legs, ctx)
    case _:
      B.failed(Unit, Error.Mismatch{"a ciphertext carries a fragment type"})

@unsafe def fragment_value(value: Value.T, ctx: C.Ctx) -> B.Comp(Unit):
  match value:
    case Value.VNeutral{Value.HGlobal{name}, Nil{}}:
      require(String.eq(name, "Nat"), Error.Mismatch{"a ciphertext carries a fragment type"})
    case Value.VLan{s, clo, level}:
      fragment_former(s, clo, ctx)
    case Value.VRan{s, clo, level}:
      fragment_former(s, clo, ctx)
    case _:
      B.failed(Unit, Error.Mismatch{"a ciphertext carries a fragment type"})

@unsafe def fhc_fragment(ctx, ty):
  do B.Comp<Unit>:
    reduced : Value.T <- whnf(ctx, ty)
    fragment_value(reduced, ctx)

@unsafe def fhc_form(+ctx: C.Ctx, level: Term.T, +diagram: Term.T) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    value : Value.T <- eval(ctx, level)
    number : Big.T <- fhc_level(ctx, value)
    universe : Level.T <- infer_univ(ctx, diagram)
    ty : Value.T <- eval(ctx, diagram)
    fragment : Unit <- fhc_fragment(ctx, ty)
    return universe

@unsafe def fhc_eval(+ctx: C.Ctx, +mode: Q.T, out_level: Big.T, out_ty: Value.T, fty: Value.T, f_uses: Q.Usage, function: Term.T, cipher: Term.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    depth : Big.T <- circuit(ctx, function)
    +ff : Former <- former_view(ctx, False{}, fty, "eval takes a function from the plaintext type to the result type")
    +point : Point <- view_point(former_shape(ff))
    +got : Inferred <- infer_uses(ctx, mode, cipher)
    +cf : Former <- former_view(ctx, False{}, inferred_ty(got), "the argument of eval or dec is not a ciphertext")
    lv : Value.T <- fhc_shape(former_shape(cf), "the argument of eval or dec is not a ciphertext")
    in_level : Big.T <- fhc_level(ctx, lv)
    in_ty : Value.T <- open(ctx, former_clo(cf), [])
    grow : Unit <- require(Big.equal(out_level, Big.add(in_level, depth)), Error.Mismatch{"the level of the result is the level of the argument plus the depth of the function"})
    dom_ok : Bool <- conv_type(ctx, point_dom(point), in_ty)
    cod : Value.T <- open(ctx, former_clo(ff), [Value.var(C.size_of(ctx))])
    cod_ok : Bool <- conv_type(C.bind(point_name(point), point_q(point), point_dom(point), ctx), cod, out_ty)
    ok : Unit <- require(Bool.and(dom_ok, cod_ok), Error.Mismatch{"eval takes a function from the plaintext type to the result type"})
    return Q.sequence(f_uses, inferred_uses(got))

@unsafe def fhc_const_pi(+ctx: C.Ctx, +dom: Value.T, cod: Value.T) -> B.Comp(Value.T):
  do B.Comp<Value.T>:
    term : Term.T <- quote(C.bind("_", Q.QMany{}, dom, ctx), cod)
    return Value.VRan{Shape.SPi{Q.QMany{}, "_", dom}, Value.close(C.env_of(ctx), term), None{}}

@unsafe def fhc_cipher_first(+ctx: C.Ctx, +mode: Q.T, out_level: Big.T, out_ty: Value.T, +function: Term.T, cipher: Term.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    depth : Big.T <- circuit(ctx, function)
    +got : Inferred <- infer_uses(ctx, Q.mul(mode, Q.QMany{}), cipher)
    +cf : Former <- former_view(ctx, False{}, inferred_ty(got), "the argument of eval or dec is not a ciphertext")
    lv : Value.T <- fhc_shape(former_shape(cf), "the argument of eval or dec is not a ciphertext")
    in_level : Big.T <- fhc_level(ctx, lv)
    in_ty : Value.T <- open(ctx, former_clo(cf), [])
    grow : Unit <- require(Big.equal(out_level, Big.add(in_level, depth)), Error.Mismatch{"the level of the result is the level of the argument plus the depth of the function"})
    pi : Value.T <- fhc_const_pi(ctx, in_ty, out_ty)
    uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QMany{}), function, pi)
    return Q.sequence(uses, inferred_uses(got))

def is_function(value: Value.T) -> Bool:
  match value:
    case Value.VRan{Shape.SPi{q, name, dom}, clo, level}:
      True{}
    case Value.VRan{Shape.SZk{q, name, dom}, clo, level}:
      True{}
    case _:
      False{}

@unsafe def fhc_head_function(is_fun: Bool, +ctx: C.Ctx, +mode: Q.T, out_level: Big.T, out_ty: Value.T, fw: Value.T, uses: Q.Usage, first: Term.T, second: Term.T) -> B.Comp(Q.Usage):
  match is_fun:
    case True{}:
      fhc_eval(ctx, mode, out_level, out_ty, fw, uses, first, second)
    case False{}:
      do B.Comp<Q.Usage>:
        zero : Unit <- require(Big.equal(out_level, Big.zero()), Error.Mismatch{"enc starts at level 0"})
        tail : Q.Usage <- check_uses(ctx, mode, second, out_ty)
        return Q.sequence(uses, tail)

@unsafe def fhc_head_result(result: Result<&2, &2, Error.T, Inferred>, +ctx: C.Ctx, mode: Q.T, out_level: Big.T, out_ty: Value.T, first: Term.T, second: Term.T) -> B.Comp(Q.Usage):
  match result:
    case Fail{error}:
      fhc_cipher_first(ctx, mode, out_level, out_ty, first, second)
    case Done{Inferred{ty, uses}}:
      do B.Comp<Q.Usage>:
        +fw : Value.T <- whnf(ctx, ty)
        fhc_head_function(is_function(fw), ctx, mode, out_level, out_ty, fw, uses, first, second)

@unsafe def fhc_legs(legs: List<&2, Term.Leg<Term.T>>, +ctx: C.Ctx, +mode: Q.T, out_level: Big.T, out_ty: Value.T) -> B.Comp(Q.Usage):
  match legs:
    case Con{Term.Leg{Nil{}, +first}, Con{Term.Leg{Nil{}, second}, Nil{}}}:
      do B.Comp<Q.Usage>:
        attempt : Result<&2, &2, Error.T, Inferred> <- B.attempt(Inferred, infer_uses(ctx, Q.mul(mode, Q.QMany{}), first))
        fhc_head_result(attempt, ctx, mode, out_level, out_ty, first, second)
    case _:
      B.failed(Q.Usage, Error.Mismatch{"a ciphertext section takes two arguments"})

@unsafe def fhc_intro_sec(+ctx: C.Ctx, mode: Q.T, legs: List<&2, Term.Leg<Term.T>>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, False{}, expected, "a ciphertext needs a right former as its expected type")
    lv : Value.T <- fhc_shape(former_shape(f), "the rule pack does not match the shape of the term")
    level : Big.T <- fhc_level(ctx, lv)
    ty : Value.T <- open(ctx, former_clo(f), [])
    fhc_legs(legs, ctx, mode, level, ty)

@unsafe def fhc_elim_out(+ctx: C.Ctx, +mode: Q.T, addr: Term.Addr<Term.T>, head: Term.T) -> B.Comp(Inferred):
  match addr:
    case Term.APt{aq, key}:
      do B.Comp<Inferred>:
        ok : Unit <- require(Q.equal(aq, Q.QOne{}), Error.Quantity{"the key of dec is consumed once"})
        +got : Inferred <- infer_uses(ctx, mode, head)
        +f : Former <- former_view(ctx, False{}, inferred_ty(got), "the argument of eval or dec is not a ciphertext")
        lv : Value.T <- fhc_shape(former_shape(f), "the argument of eval or dec is not a ciphertext")
        n : Big.T <- fhc_level(ctx, lv)
        key_got : Inferred <- infer_uses(ctx, Q.mul(mode, Q.QOne{}), key)
        ty : Value.T <- open(ctx, former_clo(f), [])
        return Inferred{ty, Q.sequence(inferred_uses(got), inferred_uses(key_got))}
    case _:
      B.failed(Inferred, Error.Wrong_leg{"dec takes the key address"})

type Mpc is Data:
  Mpc{parties: Value.T, access: Value.T}

def mpc_view(shape: Shape.T<Value.T>, message: String) -> B.Comp(Mpc):
  match shape:
    case Shape.SMpc{p, a}:
      B.pure(Mpc, Mpc{p, a})
    case _:
      B.failed(Mpc, Error.Mismatch{message})

def mpc_parties(value: Mpc) -> Value.T:
  match value:
    case Mpc{p, a}:
      p

def mpc_access(value: Mpc) -> Value.T:
  match value:
    case Mpc{p, a}:
      a

@unsafe def mpc_form(+ctx: C.Ctx, parties: Term.T, access: Term.T, diagram: Term.T) -> B.Comp(Level.T):
  do B.Comp<Level.T>:
    pv : Value.T <- eval(ctx, parties)
    count : Big.T <- B.lift(Big.T, R.mpc_parties(ctx, pv))
    pred_term : Term.T <- B.lift(Term.T,R.mpc_pred_ty(count))
    pred : Value.T <- eval(ctx, pred_term)
    uses : Q.Usage <- check_uses(ctx, Q.QZero{}, access, pred)
    infer_univ(ctx, diagram)

@unsafe def mpc_share_ty(+ctx: C.Ctx, pv: Value.T, av: Value.T, ty: Value.T) -> B.Comp(Value.T):
  do B.Comp<Value.T>:
    term : Term.T <- quote(ctx, ty)
    return Value.VRan{Shape.SMpc{pv, av}, Value.close(C.env_of(ctx), term), None{}}

@unsafe def mpc_shares(shares: List<&2, Term.T>, +ctx: C.Ctx, +mode: Q.T, +mpc: Mpc, fty: Value.T) -> B.Comp(Inferred):
  match shares:
    case Nil{}:
      B.pure(Inferred, Inferred{fty, Q.empty()})
    case Con{share, rest}:
      do B.Comp<Inferred>:
        +f : Former <- former_view(ctx, False{}, fty, "mpc takes a function from the share types to the result type")
        point : Point <- view_point(former_shape(f))
        ty : Value.T <- mpc_share_ty(ctx, mpc_parties(mpc), mpc_access(mpc), point_dom(point))
        uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QOne{}), share, ty)
        cod : Value.T <- open(ctx, former_clo(f), [Value.var(C.size_of(ctx))])
        +tail : Inferred <- mpc_shares(rest, ctx, mode, mpc, cod)
        return Inferred{inferred_ty(tail), Q.sequence(uses, inferred_uses(tail))}

@unsafe def mpc_join(+ctx: C.Ctx, +mode: Q.T, +mpc: Mpc, out_ty: Value.T, ps: Term.T, +function: Term.T, shares: List<&2, Term.T>) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    count : Big.T <- B.lift(Big.T, R.mpc_parties(ctx, mpc_parties(mpc)))
    +sub_term : Term.T <- B.lift(Term.T,R.mpc_sub_ty(count))
    sub : Value.T <- eval(ctx, sub_term)
    set_uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QMany{}), ps, sub)
    depth : Big.T <- circuit(ctx, function)
    +got : Inferred <- infer_uses(ctx, Q.mul(mode, Q.QMany{}), function)
    +rest : Inferred <- mpc_shares(shares, ctx, mode, mpc, inferred_ty(got))
    same : Bool <- conv_type(ctx, inferred_ty(rest), out_ty)
    ok : Unit <- require(same, Error.Mismatch{"the result of mpc is the type that remains after the last share"})
    return Q.sequence(set_uses, Q.sequence(inferred_uses(got), inferred_uses(rest)))

def mpc_args(legs: List<&2, Term.Leg<Term.T>>) -> B.Comp(List<&2, Term.T>):
  match legs:
    case Nil{}:
      B.pure(List<&2, Term.T>, [])
    case Con{Term.Leg{Nil{}, body}, rest}:
      do B.Comp<List<&2, Term.T>>:
        others : List<&2, Term.T> <- mpc_args(rest)
        return body <> others
    case _:
      B.failed(List<&2, Term.T>, Error.Mismatch{"an mpc section leg binds nothing"})

@unsafe def mpc_intro_args(args: List<&2, Term.T>, +ctx: C.Ctx, +mode: Q.T, mpc: Mpc, out_ty: Value.T) -> B.Comp(Q.Usage):
  match args:
    case Nil{}:
      B.failed(Q.Usage, Error.Mismatch{"an mpc section takes one argument or more"})
    case Con{x, Nil{}}:
      check_uses(ctx, Q.mul(mode, Q.QOne{}), x, out_ty)
    case Con{party, Con{x, Nil{}}}:
      do B.Comp<Q.Usage>:
        party_uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QMany{}), party, mpc_parties(mpc))
        uses : Q.Usage <- check_uses(ctx, Q.mul(mode, Q.QOne{}), x, out_ty)
        return Q.sequence(party_uses, uses)
    case Con{ps, Con{function, Con{share, shares}}}:
      mpc_join(ctx, mode, mpc, out_ty, ps, function, share <> shares)

@unsafe def mpc_intro_sec(+ctx: C.Ctx, mode: Q.T, legs: List<&2, Term.Leg<Term.T>>, expected: Value.T) -> B.Comp(Q.Usage):
  do B.Comp<Q.Usage>:
    +f : Former <- former_view(ctx, False{}, expected, "a share needs a right former as its expected type")
    mpc : Mpc <- mpc_view(former_shape(f), "the rule pack does not match the shape of the term")
    ty : Value.T <- open(ctx, former_clo(f), [])
    args : List<&2, Term.T> <- mpc_args(legs)
    mpc_intro_args(args, ctx, mode, mpc, ty)

def mpc_address(point: Term.T) -> B.Comp(Pair.Pair<Term.T, Term.T>):
  match point:
    case Term.In{Shape.SPi{q, name, dom}, Term.APt{aq, qset}, args}:
      do B.Comp<Pair.Pair<Term.T, Term.T>>:
        proof : Term.T <- one(Term.T, args, Error.Mismatch{"open carries one authorization proof"})
        return Pair.Pair{qset, proof}
    case Term.In{Shape.SZk{q, name, dom}, Term.APt{aq, qset}, args}:
      do B.Comp<Pair.Pair<Term.T, Term.T>>:
        proof : Term.T <- one(Term.T, args, Error.Mismatch{"open carries one authorization proof"})
        return Pair.Pair{qset, proof}
    case _:
      B.failed(Pair.Pair<Term.T, Term.T>, Error.Wrong_leg{"open takes the party set address with the authorization proof"})

@unsafe def mpc_open_pair(pair: Pair.Pair<Term.T, Term.T>, +ctx: C.Ctx, mode: Q.T, head: Term.T) -> B.Comp(Inferred):
  match pair:
    case Pair.Pair{+qset, proof}:
      do B.Comp<Inferred>:
        +got : Inferred <- infer_uses(ctx, mode, head)
        +f : Former <- former_view(ctx, False{}, inferred_ty(got), "the argument of open is not a share")
        +mpc : Mpc <- mpc_view(former_shape(f), "the argument of open is not a share")
        +count : Big.T <- B.lift(Big.T, R.mpc_parties(ctx, mpc_parties(mpc)))
        +sub_term : Term.T <- B.lift(Term.T,R.mpc_sub_ty(count))
        sub : Value.T <- eval(ctx, sub_term)
        set_uses : Q.Usage <- check_uses(ctx, Q.QZero{}, qset, sub)
        access : Term.T <- quote(ctx, mpc_access(mpc))
        prop : Value.T <- eval(ctx, Term.Out{Shape.SPi{Q.QZero{}, "q", sub_term}, Term.APt{Q.QZero{}, qset}, access})
        proof_uses : Q.Usage <- check_uses(ctx, Q.QZero{}, proof, prop)
        ty : Value.T <- open(ctx, former_clo(f), [])
        return Inferred{ty, Q.sequence(inferred_uses(got), Q.sequence(set_uses, proof_uses))}

@unsafe def mpc_elim_out(ctx: C.Ctx, mode: Q.T, addr: Term.Addr<Term.T>, head: Term.T) -> B.Comp(Inferred):
  match addr:
    case Term.APt{aq, point}:
      do B.Comp<Inferred>:
        ok : Unit <- require(Q.equal(aq, Q.QZero{}), Error.Quantity{"the authorization proof of open is erased"})
        pair : Pair.Pair<Term.T, Term.T> <- mpc_address(point)
        mpc_open_pair(pair, ctx, mode, head)
    case _:
      B.failed(Inferred, Error.Wrong_leg{"open takes the party set address with the authorization proof"})

@unsafe def form_dispatch(shape: Shape.T<Term.T>, left: Bool, ctx: C.Ctx, diagram: Term.T, expected: Maybe<&2, Level.T>) -> B.Comp(Level.T):
  match shape left:
    case Shape.SPi{q, name, dom} left:
      point_form(ctx, left, q, name, dom, diagram)
    case Shape.SColl{count} left:
      coll_form(ctx, count, diagram, expected)
    case Shape.SPar{a, b} left:
      B.failed(Level.T, Error.Not_yet{"SPar arrives at M1"})
    case Shape.SMu{name, indices} True{}:
      mu_form(ctx, name, indices, diagram)
    case Shape.SMu{name, indices} False{}:
      B.failed(Level.T, Error.Not_yet{"a right former at a mu shape arrives at M2"})
    case Shape.SNu{name, args} left:
      B.failed(Level.T, Error.Not_yet{"SNu arrives at M2"})
    case Shape.SZk{q, name, dom} True{}:
      zk_form(ctx, q, name, dom, diagram)
    case Shape.SZk{q, name, dom} False{}:
      B.failed(Level.T, Error.Not_yet{"Ran SZk arrives at V5"})
    case Shape.SFhc{level} True{}:
      B.failed(Level.T, Error.Not_yet{"Lan SFhc arrives at V5"})
    case Shape.SFhc{level} False{}:
      fhc_form(ctx, level, diagram)
    case Shape.SMpc{parties, access} True{}:
      B.failed(Level.T, Error.Not_yet{"Session arrives at V5"})
    case Shape.SMpc{parties, access} False{}:
      mpc_form(ctx, parties, access, diagram)

@unsafe def form(ctx, left, shape, diagram, expected):
  form_dispatch(shape, left, ctx, diagram, expected)

@unsafe def intro_sec(ctx, mode, shape, legs, expected):
  match shape:
    case Shape.SPi{q, name, dom}:
      point_intro_sec(ctx, mode, legs, expected)
    case Shape.SColl{count}:
      coll_intro_sec(ctx, mode, count, legs, expected)
    case Shape.SPar{a, b}:
      B.failed(Q.Usage, Error.Not_yet{"SPar arrives at M1"})
    case Shape.SMu{name, indices}:
      B.failed(Q.Usage, Error.Not_yet{"a right former at a mu shape arrives at M2"})
    case Shape.SNu{name, indices}:
      B.failed(Q.Usage, Error.Not_yet{"SNu arrives at M2"})
    case Shape.SZk{q, name, dom}:
      B.failed(Q.Usage, Error.Not_yet{"Ran SZk arrives at V5"})
    case Shape.SFhc{level}:
      fhc_intro_sec(ctx, mode, legs, expected)
    case Shape.SMpc{parties, access}:
      mpc_intro_sec(ctx, mode, legs, expected)

@unsafe def intro_in(ctx, mode, shape, addr, args, expected):
  match shape:
    case Shape.SPi{q, name, dom}:
      point_intro_in(ctx, mode, addr, args, expected)
    case Shape.SColl{count}:
      coll_intro_in(ctx, mode, addr, args, expected)
    case Shape.SPar{a, b}:
      B.failed(Q.Usage, Error.Not_yet{"SPar arrives at M1"})
    case Shape.SMu{name, indices}:
      mu_intro_in(ctx, mode, addr, args, expected)
    case Shape.SNu{name, indices}:
      B.failed(Q.Usage, Error.Not_yet{"SNu arrives at M2"})
    case Shape.SZk{q, name, dom}:
      zk_intro_in(ctx, mode, addr, args, expected)
    case Shape.SFhc{level}:
      B.failed(Q.Usage, Error.Not_yet{"Lan SFhc arrives at V5"})
    case Shape.SMpc{parties, access}:
      B.failed(Q.Usage, Error.Not_yet{"Session arrives at V5"})

@unsafe def elim_out(ctx, mode, shape, addr, head):
  match shape:
    case Shape.SPi{q, name, dom}:
      point_elim_out(ctx, mode, addr, head)
    case Shape.SColl{count}:
      coll_elim_out(ctx, mode, addr, head)
    case Shape.SPar{a, b}:
      B.failed(Inferred, Error.Not_yet{"SPar arrives at M1"})
    case Shape.SMu{name, indices}:
      B.failed(Inferred, Error.Not_yet{"a right former at a mu shape arrives at M2"})
    case Shape.SNu{name, indices}:
      B.failed(Inferred, Error.Not_yet{"SNu arrives at M2"})
    case Shape.SZk{q, name, dom}:
      B.failed(Inferred, Error.Not_yet{"Ran SZk arrives at V5"})
    case Shape.SFhc{level}:
      fhc_elim_out(ctx, mode, addr, head)
    case Shape.SMpc{parties, access}:
      mpc_elim_out(ctx, mode, addr, head)

@unsafe def elim_elim(ctx, mode, data, expected):
  match data:
    case Term.ElimData{Shape.SPi{q, name, dom}, scrut, sq, motive, branches}:
      point_elim_elim(ctx, mode, Term.ElimData{Shape.SPi{q, name, dom}, scrut, sq, motive, branches}, expected)
    case Term.ElimData{Shape.SColl{count}, scrut, sq, motive, branches}:
      coll_elim_elim(ctx, mode, Term.ElimData{Shape.SColl{count}, scrut, sq, motive, branches}, expected)
    case Term.ElimData{Shape.SPar{a, b}, scrut, sq, motive, branches}:
      B.failed(Inferred, Error.Not_yet{"SPar arrives at M1"})
    case Term.ElimData{Shape.SMu{name, indices}, scrut, sq, motive, branches}:
      mu_elim_elim(ctx, mode, Term.ElimData{Shape.SMu{name, indices}, scrut, sq, motive, branches})
    case Term.ElimData{Shape.SNu{name, indices}, scrut, sq, motive, branches}:
      B.failed(Inferred, Error.Not_yet{"SNu arrives at M2"})
    case Term.ElimData{Shape.SZk{q, name, dom}, scrut, sq, motive, branches}:
      point_elim_elim(ctx, mode, Term.ElimData{Shape.SZk{q, name, dom}, scrut, sq, motive, branches}, expected)
    case Term.ElimData{Shape.SFhc{level}, scrut, sq, motive, branches}:
      B.failed(Inferred, Error.Not_yet{"Lan SFhc arrives at V5"})
    case Term.ElimData{Shape.SMpc{p, a}, scrut, sq, motive, branches}:
      B.failed(Inferred, Error.Not_yet{"Session arrives at V5"})
# Conversion implementation appended to check_engine.bend by its owner.

def conv_placeholder() -> Value.T:
  Value.VUniv{Level.zero()}

def conv_and(flag:Bool,next:B.Comp(Bool)) -> B.Comp(Bool):
  match flag:
    case True{}: next
    case False{}: B.pure(Bool,False{})

def conv_supported(s:Shape.T<Value.T>) -> B.Comp(Unit):
  match s:
    case Shape.SPar{a,b}: B.failed(Unit,Error.Not_yet{"SPar arrives at M1"})
    case Shape.SNu{n,args}: B.failed(Unit,Error.Not_yet{"SNu arrives at M2"})
    case _: B.pure(Unit,Unit{})

def conv_grow(n:Nat,ctx:C.Ctx) -> C.Ctx:
  match n:
    case 0n: ctx
    case 1n+p: conv_grow(p,C.bind("_",Q.QMany{},conv_placeholder(),ctx))

def conv_fresh(n:Nat,+size:Nat) -> List<&2,Value.T>:
  match n:
    case 0n: []
    case 1n+ +p: Value.var(Nat.add(size,p)) <> conv_fresh(p,size)

@unsafe def conv_list(+ctx:C.Ctx,xs:List<&2,Value.T>,ys:List<&2,Value.T>) -> B.Comp(Bool):
  match xs ys:
    case Nil{} Nil{}: B.pure(Bool,True{})
    case Con{x,xs} Con{y,ys}:
      do B.Comp<Bool>:
        same : Bool <- conv_type(ctx,x,y)
        conv_and(same,conv_list(ctx,xs,ys))
    case _ _: B.pure(Bool,False{})

@unsafe def conv_shape(+ctx:C.Ctx,a:Shape.T<Value.T>,b:Shape.T<Value.T>) -> B.Comp(Bool):
  match a b:
    case Shape.SPi{q1,n1,d1} Shape.SPi{q2,n2,d2}: conv_and(Q.equal(q1,q2),conv_type(ctx,d1,d2))
    case Shape.SColl{n1} Shape.SColl{n2}: B.pure(Bool,Big.equal(n1,n2))
    case Shape.SPar{a1,b1} Shape.SPar{a2,b2}: B.failed(Bool,Error.Not_yet{"SPar arrives at M1"})
    case Shape.SMu{n1,as1} Shape.SMu{n2,as2}: conv_and(String.eq(n1,n2),conv_list(ctx,as1,as2))
    case Shape.SNu{n1,as1} Shape.SNu{n2,as2}: B.failed(Bool,Error.Not_yet{"SNu arrives at M2"})
    case Shape.SZk{q1,n1,d1} Shape.SZk{q2,n2,d2}: conv_and(Q.equal(q1,q2),conv_type(ctx,d1,d2))
    case Shape.SFhc{l1} Shape.SFhc{l2}: conv_type(ctx,l1,l2)
    case Shape.SMpc{p1,a1} Shape.SMpc{p2,a2}:
      do B.Comp<Bool>:
        same : Bool <- conv_type(ctx,p1,p2)
        conv_and(same,conv_type(ctx,a1,a2))
    case _ _: B.pure(Bool,False{})

@unsafe def conv_at_type(+ctx:C.Ctx,ty:Maybe<&2,Value.T>,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match ty:
    case None{}: conv_type(ctx,a,b)
    case Some{t}: conv(ctx,t,a,b)

def conv_addr(+ctx:C.Ctx,ty:Maybe<&2,Value.T>,a:Value.VAddr<Value.T>,b:Value.VAddr<Value.T>) -> B.Comp(Bool):
  match a b:
    case Value.VAPt{q1,v1} Value.VAPt{q2,v2}: conv_at_type(ctx,ty,v1,v2)
    case Value.VALeg{k1} Value.VALeg{k2}: B.pure(Bool,Big.equal(k1,k2))
    case Value.VACtor{c1} Value.VACtor{c2}: B.pure(Bool,String.eq(c1,c2))
    case _ _: B.pure(Bool,False{})

@unsafe def conv_closure_same(same:Bool,+ctx:C.Ctx,+arity:Nat,x:Value.Closure<Value.T>,y:Value.Closure<Value.T>) -> B.Comp(Bool):
  match same x y:
    case False{} _ _: B.pure(Bool,False{})
    case True{} Value.Closure{e1,b1} Value.Closure{e2,b2}:
      +fresh = conv_fresh(arity,C.size_of(ctx))
      do B.Comp<Bool>:
        v1 : Value.T <- B.lift(Value.T,Eval.eval(C.globals_of(ctx),List.append(&2,Value.T,fresh,e1),b1))
        v2 : Value.T <- B.lift(Value.T,Eval.eval(C.globals_of(ctx),List.append(&2,Value.T,fresh,e2),b2))
        conv_type(conv_grow(arity,ctx),v1,v2)

def conv_closures(+ctx:C.Ctx,+arity:Nat,x:Value.Closure<Value.T>,other:Nat,y:Value.Closure<Value.T>) -> B.Comp(Bool):
  conv_closure_same(Nat.is_eq(arity,other),ctx,arity,x,y)

@unsafe def conv_legs(+ctx:C.Ctx,xs:List<&2,Value.VLeg<Value.T>>,ys:List<&2,Value.VLeg<Value.T>>) -> B.Comp(Bool):
  match xs ys:
    case Nil{} Nil{}: B.pure(Bool,True{})
    case Con{Value.VLeg{bs1,c1},xs} Con{Value.VLeg{bs2,c2},ys}:
      do B.Comp<Bool>:
        same : Bool <- conv_closures(ctx,List.length(&2,Pair.Pair<Q.T,String>,bs1),c1,List.length(&2,Pair.Pair<Q.T,String>,bs2),c2)
        conv_and(same,conv_legs(ctx,xs,ys))
    case _ _: B.pure(Bool,False{})

@unsafe def conv_branch_addr(+ctx:C.Ctx,env1:List<&2,Value.T>,env2:List<&2,Value.T>,a:Term.Addr<Term.T>,b:Term.Addr<Term.T>) -> B.Comp(Bool):
  match a b:
    case Term.APt{q1,t1} Term.APt{q2,t2}:
      do B.Comp<Bool>:
        v1 : Value.T <- B.lift(Value.T,Eval.eval(C.globals_of(ctx),env1,t1))
        v2 : Value.T <- B.lift(Value.T,Eval.eval(C.globals_of(ctx),env2,t2))
        conv_type(ctx,v1,v2)
    case Term.ALeg{k1} Term.ALeg{k2}: B.pure(Bool,Big.equal(k1,k2))
    case Term.ACtor{c1} Term.ACtor{c2}: B.pure(Bool,String.eq(c1,c2))
    case _ _: B.pure(Bool,False{})

@unsafe def conv_branches(+ctx:C.Ctx,+env1:List<&2,Value.T>,+env2:List<&2,Value.T>,bs1:List<&2,Pair.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>,bs2:List<&2,Pair.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>) -> B.Comp(Bool):
  match bs1 bs2:
    case Nil{} Nil{}: B.pure(Bool,True{})
    case Con{Pair.Pair{a1,Term.Leg{bind1,b1}},rest1} Con{Pair.Pair{a2,Term.Leg{bind2,b2}},rest2}:
      do B.Comp<Bool>:
        addr : Bool <- conv_branch_addr(ctx,env1,env2,a1,a2)
        same : Bool <- conv_and(addr,conv_closures(ctx,List.length(&2,Pair.Pair<Q.T,String>,bind1),Value.Closure{env1,b1},List.length(&2,Pair.Pair<Q.T,String>,bind2),Value.Closure{env2,b2}))
        conv_and(same,conv_branches(ctx,env1,env2,rest1,rest2))
    case _ _: B.pure(Bool,False{})

def conv_motives(+ctx:C.Ctx,env1:List<&2,Value.T>,env2:List<&2,Value.T>,m1:Maybe<&2,Term.Motive<Term.T>>,m2:Maybe<&2,Term.Motive<Term.T>>) -> B.Comp(Bool):
  match m1 m2:
    case None{} None{}: B.pure(Bool,True{})
    case Some{Term.Motive{ind1,ix1,self1,b1}} Some{Term.Motive{ind2,ix2,self2,b2}}:
      conv_closures(ctx,1n+List.length(&2,String,ix1),Value.Closure{env1,b1},1n+List.length(&2,String,ix2),Value.Closure{env2,b2})
    case _ _: B.pure(Bool,False{})

def conv_stuck(+ctx:C.Ctx,m1:Value.StuckElim<Value.T>,m2:Value.StuckElim<Value.T>) -> B.Comp(Bool):
  match m1 m2:
    case Value.StuckElim{s1,q1,mo1,bs1,+env1} Value.StuckElim{s2,q2,mo2,bs2,+env2}:
      do B.Comp<Bool>:
        same : Bool <- conv_shape(ctx,s1,s2)
        mot : Bool <- conv_motives(ctx,env1,env2,mo1,mo2)
        conv_and(same && Q.equal(q1,q2) && mot,conv_branches(ctx,env1,env2,bs1,bs2))

type ConvStep is Data:
  ConvStep{argument:Maybe<&2,Value.T>,next:Value.T}

def conv_step_default() -> ConvStep:
  ConvStep{None{},conv_placeholder()}

def conv_step_arg(s:ConvStep) -> Maybe<&2,Value.T>:
  match s:
    case ConvStep{a,n}: a

def conv_step_next(s:ConvStep) -> Value.T:
  match s:
    case ConvStep{a,n}: n

def conv_spine_leg(+ctx:C.Ctx,lg:Maybe<&2,Value.VLeg<Value.T>>) -> B.Comp(ConvStep):
  match lg:
    case None{}: B.pure(ConvStep,conv_step_default())
    case Some{Value.VLeg{bs,clo}}:
      do B.Comp<ConvStep>:
        t : Value.T <- open(ctx,clo,[])
        return ConvStep{None{},t}

def conv_spine_former(+ctx:C.Ctx,w:Value.T,s:Shape.T<Value.T>,addr:Value.VAddr<Value.T>) -> B.Comp(ConvStep):
  match w s addr:
    case Value.VRan{ws,d,u} Shape.SPi{q,n,dom} Value.VAPt{qa,arg}:
      do B.Comp<ConvStep>:
        cod : Value.T <- open(ctx,d,[arg])
        return ConvStep{Some{dom},cod}
    case Value.VRan{ws,d,u} Shape.SColl{n} Value.VALeg{k}:
      do B.Comp<ConvStep>:
        ls : List<&2,Value.VLeg<Value.T>> <- B.lift(List<&2,Value.VLeg<Value.T>>,R.coll_legs_of(ctx,d))
        conv_spine_leg(ctx,Big.at(Value.VLeg<Value.T>,ls,k))
    case Value.VRan{ws,d,u} _ _:
      do B.Comp<ConvStep>:
        conv_supported(s)
        return conv_step_default()
    case _ _ _: B.pure(ConvStep,conv_step_default())

def conv_spine_step(+ctx:C.Ctx,hty:Value.T,s:Shape.T<Value.T>,a:Value.VAddr<Value.T>) -> B.Comp(ConvStep):
  do B.Comp<ConvStep>:
    w : Value.T <- whnf(ctx,hty)
    conv_spine_former(ctx,w,s,a)

@unsafe def conv_spines(+ctx:C.Ctx,hty:Value.T,xs:List<&2,Value.Spine<Value.T>>,ys:List<&2,Value.Spine<Value.T>>) -> B.Comp(Bool):
  match xs ys:
    case Nil{} Nil{}: B.pure(Bool,True{})
    case Con{Value.SOut{+s1,+a1},r1} Con{Value.SOut{s2,a2},r2}:
      do B.Comp<Bool>:
        same : Bool <- conv_shape(ctx,s1,s2)
        +step : ConvStep <- conv_spine_step(ctx,hty,s1,a1)
        addr : Bool <- conv_addr(ctx,conv_step_arg(step),a1,a2)
        conv_and(same && addr,conv_spines(ctx,conv_step_next(step),r1,r2))
    case Con{Value.SElim{m1},r1} Con{Value.SElim{m2},r2}:
      do B.Comp<Bool>:
        same : Bool <- conv_stuck(ctx,m1,m2)
        conv_and(same,conv_spines(ctx,conv_placeholder(),r1,r2))
    case _ _: B.pure(Bool,False{})

def conv_level_default(u:Maybe<&2,Level.T>) -> Level.T:
  match u:
    case None{}: Level.zero()
    case Some{l}: l

def conv_annotation_width(zero:Bool,u1:Maybe<&2,Level.T>,u2:Maybe<&2,Level.T>) -> B.Comp(Bool):
  match zero:
    case True{}: LevelEq.decide_m(conv_level_default(u1),conv_level_default(u2),True{})
    case False{}: B.pure(Bool,True{})

def conv_annotations(s:Shape.T<Value.T>,u1:Maybe<&2,Level.T>,u2:Maybe<&2,Level.T>) -> B.Comp(Bool):
  match s:
    case Shape.SColl{n}: conv_annotation_width(Big.equal(n,Big.zero()),u1,u2)
    case _: B.pure(Bool,True{})

@unsafe def conv_diagram(+ctx:C.Ctx,s:Shape.T<Value.T>,d1:Value.Closure<Value.T>,d2:Value.Closure<Value.T>) -> B.Comp(Bool):
  match s:
    case Shape.SPi{q,n,dom}:
      +v = Value.var(C.size_of(ctx))
      do B.Comp<Bool>:
        a : Value.T <- open(ctx,d1,[v])
        b : Value.T <- open(ctx,d2,[v])
        conv_type(C.bind(n,q,dom,ctx),a,b)
    case Shape.SZk{q,n,dom}:
      +v = Value.var(C.size_of(ctx))
      do B.Comp<Bool>:
        a : Value.T <- open(ctx,d1,[v])
        b : Value.T <- open(ctx,d2,[v])
        conv_type(C.bind(n,q,dom,ctx),a,b)
    case _:
      do B.Comp<Bool>:
        a : Value.T <- open(ctx,d1,[])
        b : Value.T <- open(ctx,d2,[])
        conv_type(ctx,a,b)

def conv_former_annotations(same:Bool,+ctx:C.Ctx,+s:Shape.T<Value.T>,d1:Value.Closure<Value.T>,u1:Maybe<&2,Level.T>,d2:Value.Closure<Value.T>,u2:Maybe<&2,Level.T>) -> B.Comp(Bool):
  match same:
    case False{}: B.pure(Bool,False{})
    case True{}:
      do B.Comp<Bool>:
        levels : Bool <- conv_annotations(s,u1,u2)
        conv_and(levels,conv_diagram(ctx,s,d1,d2))

def conv_formers(+ctx:C.Ctx,+s1:Shape.T<Value.T>,d1:Value.Closure<Value.T>,u1:Maybe<&2,Level.T>,s2:Shape.T<Value.T>,d2:Value.Closure<Value.T>,u2:Maybe<&2,Level.T>) -> B.Comp(Bool):
  do B.Comp<Bool>:
    conv_supported(s1)
    same : Bool <- conv_shape(ctx,s1,s2)
    conv_former_annotations(same,ctx,s1,d1,u1,d2,u2)

def conv_neutrals(same:Bool,+ctx:C.Ctx,h:Value.Head,sp1:List<&2,Value.Spine<Value.T>>,sp2:List<&2,Value.Spine<Value.T>>) -> B.Comp(Bool):
  match same:
    case False{}: B.pure(Bool,False{})
    case True{}:
      do B.Comp<Bool>:
        ty : Value.T <- head_ty(ctx,h)
        conv_spines(ctx,ty,List.reverse(&2,Value.Spine<Value.T>,sp1),List.reverse(&2,Value.Spine<Value.T>,sp2))

def conv_structural_whnf(+ctx:C.Ctx,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match a b:
    case Value.VUniv{l1} Value.VUniv{l2}: LevelEq.decide_m(l1,l2,True{})
    case Value.VLit{x} Value.VLit{y}: B.pure(Bool,Literal.equal(x,y))
    case Value.VRan{s1,d1,u1} Value.VRan{s2,d2,u2}: conv_formers(ctx,s1,d1,u1,s2,d2,u2)
    case Value.VLan{s1,d1,u1} Value.VLan{s2,d2,u2}: conv_formers(ctx,s1,d1,u1,s2,d2,u2)
    case Value.VIn{s1,a1,args1} Value.VIn{s2,a2,args2}:
      do B.Comp<Bool>:
        same : Bool <- conv_shape(ctx,s1,s2)
        addr : Bool <- conv_addr(ctx,None{},a1,a2)
        conv_and(same && addr,conv_list(ctx,args1,args2))
    case Value.VSec{s1,ls1} Value.VSec{s2,ls2}:
      do B.Comp<Bool>:
        same : Bool <- conv_shape(ctx,s1,s2)
        conv_and(same,conv_legs(ctx,ls1,ls2))
    case Value.VNeutral{+h1,sp1} Value.VNeutral{h2,sp2}: conv_neutrals(Value.head_equal(h1,h2),ctx,h1,sp1,sp2)
    case _ _: B.pure(Bool,False{})

def conv_structural(+ctx:C.Ctx,a:Value.T,b:Value.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    wa : Value.T <- whnf(ctx,a)
    wb : Value.T <- whnf(ctx,b)
    conv_structural_whnf(ctx,wa,wb)

@unsafe def conv_prop_probe(+ctx:C.Ctx,ty:Value.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    t : Term.T <- quote(ctx,ty)
    l : Level.T <- infer_univ(ctx,t)
    return Level.always_zero(l)

def conv_prop_failure(error:Error.T) -> B.Comp(Bool):
  match error:
    case Error.Budget_exhausted{message}: B.failed(Bool,Error.Budget_exhausted{message})
    case _: B.pure(Bool,False{})

def conv_is_prop(ctx:C.Ctx,ty:Value.T) -> B.Comp(Bool):
  match ty:
    case Value.VUniv{level}: B.pure(Bool,False{})
    case _: B.recover(Bool,conv_prop_probe(ctx,ty),conv_prop_failure)

def conv_fields_zero(xs:Positivity.Telescope) -> Bool:
  match xs:
    case Nil{}: True{}
    case Con{Positivity.Binder{q,n,ty},rest}: Q.equal(q,Q.QZero{}) && conv_fields_zero(rest)

def conv_ctor_zero(c:Maybe<&2,Positivity.Ctor>) -> Bool:
  match c:
    case None{}: False{}
    case Some{Positivity.Ctor{n,args,indices,arity,rec}}: conv_fields_zero(args) && Bool.not(rec)

def conv_family_zero(f:Positivity.Family) -> Bool:
  match f:
    case Positivity.Family{n,ps,ix,level,Positivity.Complete{Nil{}},cs,positive}: Level.always_zero(level)
    case Positivity.Family{+n,+ps,+ix,+level,Positivity.Complete{Con{+name,Nil{}}},+cs,+positive}:
      Level.always_zero(level) && conv_ctor_zero(Positivity.ctor_of(name,Positivity.Family{n,ps,ix,level,Positivity.Complete{[name]},cs,positive}))
    case _: False{}

def conv_family_probe(r:Result<&2,&2,Error.T,Positivity.Family>) -> Bool:
  match r:
    case Fail{e}: False{}
    case Done{f}: conv_family_zero(f)

def conv_subsingleton(ctx:C.Ctx,w:Value.T) -> B.Comp(Bool):
  match w:
    case Value.VLan{Shape.SMu{n,args},d,u}: B.pure(Bool,conv_family_probe(R.mu_family(ctx,n)))
    case Value.VLan{s,d,u}:
      do B.Comp<Bool>:
        conv_supported(s)
        return False{}
    case _: B.pure(Bool,False{})

def conv_out(ctx:C.Ctx,s:Shape.T<Value.T>,a:Value.VAddr<Value.T>,v:Value.T) -> B.Comp(Value.T):
  B.lift(Value.T,Eval.out_value(C.globals_of(ctx),s,a,v))

def conv_proj_branches(q:Q.T,which:Nat) -> List<&2,Pair.Pair<Term.Addr<Term.T>,Term.Leg<Term.T>>>:
  [Pair.Pair{Term.aleg(Term.T,0n),Term.Leg{[Pair.Pair{q,"x"},Pair.Pair{Q.QMany{},"y"}],Term.Var{Bool.pick(Nat,Nat.is_eq(which,0n),1n,0n)}}}]

def conv_project(+ctx:C.Ctx,s:Shape.T<Value.T>,q:Q.T,scrut_q:Q.T,mo:Maybe<&2,Term.Motive<Term.T>>,which:Nat,v:Value.T) -> B.Comp(Value.T):
  B.lift(Value.T,Eval.elim_value(C.globals_of(ctx),Value.StuckElim{s,scrut_q,mo,conv_proj_branches(q,which),C.env_of(ctx)},v))

def conv_proj_motive_body(which:Nat,+ctx:C.Ctx,s:Shape.T<Value.T>,d:Value.Closure<Value.T>,+q:Q.T,+dom:Value.T) -> B.Comp(Value.T):
  match which:
    case 0n: B.pure(Value.T,dom)
    case 1n+p:
      do B.Comp<Value.T>:
        domain : Term.T <- quote(C.bind("self",q,dom,ctx),dom)
        point : Value.T <- conv_project(ctx,s,q,Q.QOne{},Some{Term.Motive{None{},[],"self",domain}},0n,Value.var(C.size_of(ctx)))
        open(ctx,d,[point])

def conv_proj_motive(+ctx:C.Ctx,s:Shape.T<Value.T>,d:Value.Closure<Value.T>,+q:Q.T,n:String,+dom:Value.T,which:Nat) -> B.Comp(Maybe<&2,Term.Motive<Term.T>>):
  do B.Comp<Maybe<&2,Term.Motive<Term.T>>>:
    v : Value.T <- conv_proj_motive_body(which,ctx,s,d,q,dom)
    body : Term.T <- quote(C.bind(n,q,dom,ctx),v)
    return Some{Term.Motive{None{},[],"self",body}}

@unsafe def conv_eta_pair_second(first:Bool,+ctx:C.Ctx,+s:Shape.T<Value.T>,+d:Value.Closure<Value.T>,+q:Q.T,n:String,dom:Value.T,a1:Value.T,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match first:
    case False{}: B.pure(Bool,False{})
    case True{}:
      do B.Comp<Bool>:
        cod : Value.T <- open(ctx,d,[a1])
        +mo : Maybe<&2,Term.Motive<Term.T>> <- conv_proj_motive(ctx,s,d,q,n,dom,1n)
        a2 : Value.T <- conv_project(ctx,s,q,Q.QOne{},mo,1n,a)
        b2 : Value.T <- conv_project(ctx,s,q,Q.QOne{},mo,1n,b)
        conv(ctx,cod,a2,b2)

@unsafe def conv_eta_pair(+ctx:C.Ctx,+s:Shape.T<Value.T>,+d:Value.Closure<Value.T>,+q:Q.T,+n:String,+dom:Value.T,+a:Value.T,+b:Value.T) -> B.Comp(Bool):
  do B.Comp<Bool>:
    +mo : Maybe<&2,Term.Motive<Term.T>> <- conv_proj_motive(ctx,s,d,q,n,dom,0n)
    +a1 : Value.T <- conv_project(ctx,s,q,Q.QOne{},mo,0n,a)
    b1 : Value.T <- conv_project(ctx,s,q,Q.QOne{},mo,0n,b)
    first : Bool <- conv(ctx,dom,a1,b1)
    conv_eta_pair_second(first,ctx,s,d,q,n,dom,a1,a,b)

@unsafe def conv_eta_coll(sign:Cmp,n:Big.T,+index:Big.T,+ctx:C.Ctx,+s:Shape.T<Value.T>,+legs:List<&2,Value.VLeg<Value.T>>,+a:Value.T,+b:Value.T) -> B.Comp(Bool):
  match sign:
    case EQ{}: B.pure(Bool,True{})
    case LT{}: B.failed(Bool,Error.Mismatch{"a collection width must be nonnegative"})
    case GT{}:
      do B.Comp<Bool>:
        ty : Value.T <- B.lift(Value.T,R.coll_leg_ty(ctx,legs,index))
        x : Value.T <- conv_out(ctx,s,Value.VALeg{index},a)
        y : Value.T <- conv_out(ctx,s,Value.VALeg{index},b)
        same : Bool <- conv(ctx,ty,x,y)
        +next : Big.T = Big.sub(n,Big.one())
        conv_and(same,conv_eta_coll(Big.sign(next),next,Big.add(index,Big.one()),ctx,s,legs,a,b))

@unsafe def conv_eta_or_structural(+ctx:C.Ctx,w:Value.T,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match w:
    case Value.VRan{Shape.SPi{+q,+n,+dom},d,u}:
      +v = Value.var(C.size_of(ctx))
      +s = {Shape.SPi{q,n,dom}:Shape.T<Value.T>}
      do B.Comp<Bool>:
        cod : Value.T <- open(ctx,d,[v])
        fa : Value.T <- conv_out(ctx,s,Value.VAPt{q,v},a)
        fb : Value.T <- conv_out(ctx,s,Value.VAPt{q,v},b)
        conv(C.bind(n,q,dom,ctx),cod,fa,fb)
    case Value.VLan{Shape.SPi{+q,+n,+dom},d,u}: conv_eta_pair(ctx,Shape.SPi{q,n,dom},d,q,n,dom,a,b)
    case Value.VRan{Shape.SColl{+n},d,u}:
      do B.Comp<Bool>:
        legs : List<&2,Value.VLeg<Value.T>> <- B.lift(List<&2,Value.VLeg<Value.T>>,R.coll_legs_of(ctx,d))
        conv_eta_coll(Big.sign(n),n,Big.zero(),ctx,Shape.SColl{n},legs,a,b)
    case Value.VRan{s,d,u}:
      do B.Comp<Bool>:
        conv_supported(s)
        conv_structural(ctx,a,b)
    case Value.VLan{s,d,u}:
      do B.Comp<Bool>:
        conv_supported(s)
        conv_structural(ctx,a,b)
    case _: conv_structural(ctx,a,b)

def conv_after_sub(sub:Bool,ctx:C.Ctx,w:Value.T,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match sub:
    case True{}: B.pure(Bool,True{})
    case False{}: conv_eta_or_structural(ctx,w,a,b)

def conv_after_prop(prop:Bool,+ctx:C.Ctx,ty:Value.T,a:Value.T,b:Value.T) -> B.Comp(Bool):
  match prop:
    case True{}: B.pure(Bool,True{})
    case False{}:
      do B.Comp<Bool>:
        +w : Value.T <- whnf(ctx,ty)
        sub : Bool <- conv_subsingleton(ctx,w)
        conv_after_sub(sub,ctx,w,a,b)

@unsafe def conv(ctx,ty,a,b):
  +ty = ty
  do B.Comp<Bool>:
    prop : Bool <- conv_is_prop(ctx,ty)
    conv_after_prop(prop,ctx,ty,a,b)

@unsafe def conv_type(ctx,a,b):
  conv_structural(ctx,a,b)
