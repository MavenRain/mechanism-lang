import Base
import ../kernel/bignum.bend as Width
import ../kernel/check_engine.bend as Engine
import ../kernel/check.bend as Check
import ../kernel/context.bend as C
import ../kernel/global.bend as G
import ../kernel/quantity.bend as Q
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../kernel/shape.bend as Shape
import ../kernel/level.bend as L
import ../kernel/budget.bend as B
import ../kernel/common.bend as Pair
import ../kernel/error.bend as E

# Exact pre-correction helper, retained only as an executable negative control.
def legacy_body(which: Nat, +ctx: C.Ctx, s: Shape.T<V.T>, d: V.Closure<V.T>, +q: Q.T, dom: V.T) -> B.Comp(V.T):
  match which:
    case 0n: B.pure(V.T, dom)
    case 1n+p: do B.Comp<V.T>:
      point : V.T <- Engine.conv_project(ctx, s, q, q, None{}, 0n, V.var(C.size_of(ctx)))
      Engine.open(ctx, d, [point])

def legacy_motive(+ctx: C.Ctx, s: Shape.T<V.T>, d: V.Closure<V.T>, +q: Q.T, n: String, +dom: V.T, which: Nat) -> B.Comp(Maybe<&2, T.Motive<T.T>>):
  do B.Comp<Maybe<&2, T.Motive<T.T>>>:
    v : V.T <- legacy_body(which, ctx, s, d, q, dom)
    body : T.T <- Engine.quote(C.bind(n, q, dom, ctx), v)
    return Some{T.Motive{None{}, [], "self", body}}

def motive_body(value: Maybe<&2, T.Motive<T.T>>) -> Result<&2, &2, E.T, T.T>:
  match value:
    case None{}: Fail{E.Cannot_infer{"missing projection motive"}}
    case Some{T.Motive{ind, indices, self, body}}: Done{body}

def motive(+q: Q.T, old: Bool, +which: Nat) -> Result<&2, &2, E.T, T.T>:
  +ctx = C.make(G.initial(), B.unlimited(), 0n)
  +dom : V.T = V.VUniv{L.one()}
  +shape : Shape.T<V.T> = Shape.SPi{q, "A", dom}
  +diagram = V.close([], T.Var{0n})
  action = Bool.pick(Unit -> B.Comp(Maybe<&2, T.Motive<T.T>>), old,
    unit => legacy_motive(ctx, shape, diagram, q, "A", dom, which),
    unit => Engine.conv_proj_motive(ctx, shape, diagram, q, "A", dom, which))
  do Result<&2, &2, E.T, T.T>:
    value : Maybe<&2, T.Motive<T.T>> <- B.run(Maybe<&2, T.Motive<T.T>>, action(Unit{}), B.unlimited())
    motive_body(value)

def pair_context(+q: Q.T) -> Check.Ctx:
  Check.bind("pair", Q.QMany{}, V.VLan{Shape.SPi{q, "A", V.VUniv{L.one()}}, V.close([], T.Var{0n}), None{}}, Check.make(G.initial(), B.unlimited(), 0n))

def expected(+q: Q.T) -> T.T:
  T.Elim{T.ElimData{Shape.SPi{q, "A", T.Univ{L.one()}}, T.Var{0n}, Q.QOne{}, Some{T.Motive{None{}, [], "self", T.Univ{L.one()}}}, [Pair.Pair{T.ALeg{Width.of_nat(0n)}, T.Leg{[Pair.Pair{q, "x"}, Pair.Pair{Q.QMany{}, "y"}], T.Var{1n}}}]}}

def universe(value: V.T) -> Bool:
  match value:
    case V.VUniv{level}: L.equal(level, L.one())
    case _: False{}

def current(+q: Q.T) -> Result<&2, &2, E.T, Bool>:
  do Result<&2, &2, E.T, Bool>:
    +body : T.T <- motive(q, False{}, 1n)
    inferred : V.T <- Check.infer(pair_context(q), Q.QZero{}, body)
    first : T.T <- motive(q, False{}, 0n)
    return Bool.and(T.equal(body, expected(q)), Bool.and(universe(inferred), T.equal(first, T.Univ{L.one()})))

def legacy_error(result: Result<&2, &2, E.T, V.T>) -> Bool:
  match result:
    case Fail{E.Cannot_infer{message}}: String.eq(message, "an elimination without a motive needs an expected type")
    case _: False{}

def old_refused(+q: Q.T) -> Result<&2, &2, E.T, Bool>:
  do Result<&2, &2, E.T, Bool>:
    body : T.T <- motive(q, True{}, 1n)
    return legacy_error(Check.infer(pair_context(q), Q.QZero{}, body))

def require(result: Result<&2, &2, E.T, Bool>, label: String) -> IO(Unit):
  match result:
    case Done{True{}}: IO.pure(Unit, Unit{})
    case Done{False{}}: IO.die(Unit, 1, "FAIL " ++ label)
    case Fail{error}: IO.die(Unit, 1, "FAIL " ++ label ++ ": " ++ E.to_string(error))

def main() -> IO(Unit):
  do IO<Unit>:
    ok : Unit <- require(current(Q.QZero{}), "current zero")
    ok : Unit <- require(current(Q.QOne{}), "current one")
    ok : Unit <- require(current(Q.QMany{}), "current many")
    ok : Unit <- require(old_refused(Q.QZero{}), "legacy zero")
    ok : Unit <- require(old_refused(Q.QOne{}), "legacy one")
    ok : Unit <- require(old_refused(Q.QMany{}), "legacy many")
    IO.print("PASS dependent pair projection motives: 9 positive assertions, 3 old-port refusals")
