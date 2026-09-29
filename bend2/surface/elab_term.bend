import Base
import ../kernel/bignum.bend as Bignum
import ../kernel/budget.bend as Budget
import ../kernel/check_engine.bend as Engine
import ./syntax.bend as Syntax
import ./universe.bend as Universe
import ../kernel/check.bend as Check
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Quantity
import ../kernel/level.bend as Level
import ../kernel/positivity.bend as Positivity
import ../kernel/eval.bend as Eval
import ../kernel/error.bend as Error
import ../kernel/rules.bend as Rules
import ../kernel/conv.bend as Conv
import ../kernel/literal.bend as Literal

type MuRef is Data:
  RFam{family: Positivity.Family}
  RCtor{family: Positivity.Family, ctor: Positivity.Ctor}

type Point is Data:
  Point{quantity: Quantity.T, name: String, domain: Value.T}

type Spine is Data:
  Spine{head: Syntax.T, args: List<&2, Syntax.T>}

def no_expect(what: String) -> Error.T:
  Error.Cannot_infer{what ++ " needs an expected type"}

def globals_of(ctx: Check.Ctx) -> Global.T:
  Check.globals_of(ctx)

def env_of(ctx: Check.Ctx) -> List<&2, Value.T>:
  Check.env_of(ctx)

def size_of(ctx: Check.Ctx) -> Nat:
  Check.size_of(ctx)

def eval_in(+ctx: Check.Ctx, term: Term.T) -> Budget.Comp(Value.T):
  Budget.lift(Value.T, Eval.eval(globals_of(ctx), env_of(ctx), term))

def whnf_in(ctx: Check.Ctx, value: Value.T) -> Budget.Comp(Value.T):
  Budget.lift(Value.T, Eval.whnf(globals_of(ctx), value))

def quote_in(+ctx: Check.Ctx, value: Value.T) -> Budget.Comp(Term.T):
  Budget.lift(Term.T, Eval.quote(globals_of(ctx), size_of(ctx), value))

def univ_of_int(level: Bignum.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    level : Level.T <- Budget.lift(Level.T, Universe.lower(Universe.NatLevel{level}))
    return Term.Univ{level}

def point_of(shape: Shape.T<Value.T>) -> Maybe<&2, Point>:
  match shape:
    case Shape.SPi{q, name, domain}:
      Some{Point{q, name, domain}}
    case Shape.SZk{q, name, domain}:
      Some{Point{q, name, domain}}
    case other:
      None{}

def zk_point(shape: Shape.T<Value.T>) -> Maybe<&2, Point>:
  match shape:
    case Shape.SZk{q, name, domain}:
      Some{Point{q, name, domain}}
    case other:
      None{}

def fhc_level(shape: Shape.T<Value.T>) -> Maybe<&2, Value.T>:
  match shape:
    case Shape.SFhc{level}:
      Some{level}
    case other:
      None{}

def app_split(term: Syntax.T) -> Maybe<&2, Common.Pair<Syntax.T, Syntax.T>>:
  match term:
    case Syntax.SApp{head, arg}:
      Some{Common.Pair{head, arg}}
    case other:
      None{}

def arrow_split(term: Syntax.T) -> Maybe<&2, Common.Pair<Syntax.Binder<Syntax.T>, Syntax.T>>:
  match term:
    case Syntax.SArrow{binder, body}:
      Some{Common.Pair{binder, body}}
    case other:
      None{}

def var_name(term: Syntax.T) -> Maybe<&2, String>:
  match term:
    case Syntax.SVar{name}:
      Some{name}
    case other:
      None{}

def spine_of(term: Syntax.T, args: List<&2, Syntax.T>) -> Spine:
  match term:
    case Syntax.SApp{head, arg}:
      spine_of(head, arg <> args)
    case other:
      Spine{other, args}

def family_name(family: Positivity.Family) -> String:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      name

def select_ctor(candidate: Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>, selected: Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match candidate selected:
    case None{} selected:
      selected
    case Some{entry} None{}:
      Some{entry}
    case Some{Common.Pair{+family, ctor}} Some{Common.Pair{+previous, previous_ctor}}:
      Bool.pick(Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>,
        String.is_lt(family_name(family), family_name(previous)),
        Some{Common.Pair{family, ctor}}, Some{Common.Pair{previous, previous_ctor}})

def ctor_candidate(ctor: Maybe<&2, Positivity.Ctor>, family: Positivity.Family) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match ctor:
    case None{}:
      None{}
    case Some{ctor}:
      Some{Common.Pair{family, ctor}}

def scan_ctors(families: List<&2, Common.Pair<String, Positivity.Family>>, +name: String) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match families:
    case Nil{}:
      None{}
    case Con{Common.Pair{key, +family}, tail}:
      select_ctor(ctor_candidate(Positivity.ctor_of(name, family), family), scan_ctors(tail, name))

def find_ctor(name: String, globals: Global.T) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match globals:
    case Global.Globals{entries, families, entry_index, family_index}:
      scan_ctors(families, name)

def ctor_ref(ctor: Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>) -> Maybe<&2, MuRef>:
  match ctor:
    case None{}:
      None{}
    case Some{Common.Pair{family, ctor}}:
      Some{RCtor{family, ctor}}

def family_ref(family: Maybe<&2, Positivity.Family>, name: String, globals: Global.T) -> Maybe<&2, MuRef>:
  match family:
    case Some{family}:
      Some{RFam{family}}
    case None{}:
      ctor_ref(find_ctor(name, globals))

def local_ref(local: Maybe<&2, Nat>, +name: String, +globals: Global.T) -> Maybe<&2, MuRef>:
  match local:
    case Some{index}:
      None{}
    case None{}:
      family_ref(Global.find_family(name, globals), name, globals)

def mu_ref_of(+ctx: Check.Ctx, +name: String) -> Maybe<&2, MuRef>:
  local_ref(Check.index_of_name(ctx, name), name, globals_of(ctx))

def expected_family_result(result: Result<&2, &2, Error.T, Value.T>) -> Maybe<&2, String>:
  match result:
    case Done{Value.VLan{Shape.SMu{name, args}, diagram, level}}:
      Some{name}
    case other:
      None{}

def expected_family(ctx: Check.Ctx, expected: Maybe<&2, Value.T>) -> Maybe<&2, String>:
  match expected:
    case None{}:
      None{}
    case Some{ty}:
      expected_family_result(Eval.whnf(globals_of(ctx), ty))

def arity_is(what: String, +want: Nat, +got: Nat) -> Budget.Comp(Unit):
  Bool.pick(Budget.Comp(Unit), Nat.is_eq(want, got), Budget.pure(Unit, Unit{}),
    Budget.failed(Unit, Error.Mismatch{what ++ " takes " ++ Nat.show(want) ++ " arguments and the term gives " ++ Nat.show(got)}))

def legs_of(terms: List<&2, Term.T>) -> List<&2, Term.Leg<Term.T>>:
  match terms:
    case Nil{}:
      []
    case Con{term, tail}:
      Term.Leg{[], term} <> legs_of(tail)

def diagram_of(+terms: List<&2, Term.T>) -> Term.T:
  Term.Sec{Shape.coll(Term.T, List.length(&2, Term.T, terms)), legs_of(terms)}

def sum_ty(+terms: List<&2, Term.T>) -> Term.T:
  Term.Lan{Shape.coll(Term.T, List.length(&2, Term.T, terms)), diagram_of(terms)}

def prod_ty(+terms: List<&2, Term.T>) -> Term.T:
  Term.Ran{Shape.coll(Term.T, List.length(&2, Term.T, terms)), diagram_of(terms)}

def unit_val() -> Term.T:
  Term.Sec{Shape.coll(Term.T, 0n), []}

law elab:
  for +ctx: Check.Ctx
  for expected: Maybe<&2, Value.T>
  for syntax: Syntax.T
  Budget.Comp(Term.T)

type Former is Data:
  Former{shape: Shape.T<Value.T>, diagram: Value.Closure<Value.T>, level: Maybe<&2, Level.T>}

type Bound is Data:
  Bound{quantity: Quantity.T, name: String, ty: Term.T, context: Check.Ctx}

def require(-A: Data, value: Maybe<&2, A>, error: Error.T) -> Budget.Comp(A):
  match value:
    case Some{value}:
      Budget.pure(A, value)
    case None{}:
      Budget.failed(A, error)

def first(-A: Data, -B: Data, pair: Common.Pair<A, B>) -> A:
  match pair:
    case Common.Pair{x, y}:
      x

def second(-A: Data, -B: Data, pair: Common.Pair<A, B>) -> B:
  match pair:
    case Common.Pair{x, y}:
      y

def former(left: Bool, value: Value.T, message: String) -> Budget.Comp(Former):
  match left value:
    case True{} Value.VLan{shape, diagram, level}:
      Budget.pure(Former, Former{shape, diagram, level})
    case False{} Value.VRan{shape, diagram, level}:
      Budget.pure(Former, Former{shape, diagram, level})
    case left other:
      Budget.failed(Former, Error.Mismatch{message})

def former_shape(former: Former) -> Shape.T<Value.T>:
  match former:
    case Former{shape, diagram, level}:
      shape

def former_diagram(former: Former) -> Value.Closure<Value.T>:
  match former:
    case Former{shape, diagram, level}:
      diagram

def point_q(point: Point) -> Quantity.T:
  match point:
    case Point{q, name, domain}:
      q

def point_name(point: Point) -> String:
  match point:
    case Point{q, name, domain}:
      name

def point_domain(point: Point) -> Value.T:
  match point:
    case Point{q, name, domain}:
      domain

def bound_q(bound: Bound) -> Quantity.T:
  match bound:
    case Bound{q, name, ty, context}:
      q

def bound_name(bound: Bound) -> String:
  match bound:
    case Bound{q, name, ty, context}:
      name

def bound_type(bound: Bound) -> Term.T:
  match bound:
    case Bound{q, name, ty, context}:
      ty

def bound_context(bound: Bound) -> Check.Ctx:
  match bound:
    case Bound{q, name, ty, context}:
      context

def inferred_type(value: Engine.Inferred) -> Value.T:
  match value:
    case Engine.Inferred{ty, uses}:
      ty

def infer_type(ctx: Check.Ctx, mode: Quantity.T, term: Term.T) -> Budget.Comp(Value.T):
  do Budget.Comp<Value.T>:
    inferred : Engine.Inferred <- Engine.infer_uses(ctx, Quantity.runtime(mode), term)
    return inferred_type(inferred)

def type_of(+ctx: Check.Ctx, term: Term.T) -> Budget.Comp(Value.T):
  do Budget.Comp<Value.T>:
    ty : Value.T <- infer_type(ctx, Quantity.QZero{}, term)
    whnf_in(ctx, ty)

def expected_former(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, left: Bool, word: String, message: String) -> Budget.Comp(Former):
  do Budget.Comp<Former>:
    ty : Value.T <- require(Value.T, expected, no_expect(word))
    ty : Value.T <- whnf_in(ctx, ty)
    former(left, ty, message)

def none_expectations(count: Nat) -> List<&2, Maybe<&2, Value.T>>:
  match count:
    case 0n:
      []
    case 1n+rest:
      None{} <> none_expectations(rest)

def leg_types(legs: List<&2, Value.VLeg<Value.T>>, +ctx: Check.Ctx, +count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match legs:
    case Nil{}:
      Bool.pick(Budget.Comp(List<&2, Maybe<&2, Value.T>>), Bignum.equal(count, Bignum.zero()),
        Budget.pure(List<&2, Maybe<&2, Value.T>>, []),
        Budget.failed(List<&2, Maybe<&2, Value.T>>, Error.Wrong_leg{"the leg address is outside the collection"}))
    case Con{Value.VLeg{binders, closure}, tail}:
      action = Bool.pick(Unit -> Budget.Comp(List<&2, Maybe<&2, Value.T>>), Bignum.equal(count, Bignum.zero()),
        unit => Budget.pure(List<&2, Maybe<&2, Value.T>>, []),
        unit => do Budget.Comp<List<&2, Maybe<&2, Value.T>>>:
          ty : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), closure, []))
          tail : List<&2, Maybe<&2, Value.T>> <- leg_types(tail, ctx, Bignum.sub(count, Bignum.one()))
          return Some{ty} <> tail)
      action(Unit{})

def requested_leg_types_nonnegative(+ctx: Check.Ctx, diagram: Value.Closure<Value.T>, count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  do Budget.Comp<List<&2, Maybe<&2, Value.T>>>:
    legs : List<&2, Value.VLeg<Value.T>> <- Budget.lift(List<&2, Value.VLeg<Value.T>>, Rules.coll_legs_of(ctx, diagram))
    leg_types(legs, ctx, count)

def requested_leg_types(ctx: Check.Ctx, diagram: Value.Closure<Value.T>, +count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  action = Bool.pick(Unit -> Budget.Comp(List<&2, Maybe<&2, Value.T>>), Bignum.le(Bignum.zero(), count),
    unit => requested_leg_types_nonnegative(ctx, diagram, count),
    unit => Budget.failed(List<&2, Maybe<&2, Value.T>>, Error.Mismatch{"a collection width must be nonnegative"}))
  action(Unit{})

def matching_leg_types(same: Bool, +ctx: Check.Ctx, diagram: Value.Closure<Value.T>, count: Nat) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match same:
    case False{}:
      Budget.pure(List<&2, Maybe<&2, Value.T>>, none_expectations(count))
    case True{}:
      requested_leg_types(ctx, diagram, Bignum.of_nat(count))

def expected_leg_types(left: Bool, value: Value.T, ctx: Check.Ctx, +count: Nat) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match left value:
    case True{} Value.VLan{Shape.SColl{width}, diagram, level}:
      matching_leg_types(Bignum.equal(width, Bignum.of_nat(count)), ctx, diagram, count)
    case False{} Value.VRan{Shape.SColl{width}, diagram, level}:
      matching_leg_types(Bignum.equal(width, Bignum.of_nat(count)), ctx, diagram, count)
    case left other:
      Budget.pure(List<&2, Maybe<&2, Value.T>>, none_expectations(count))

def leg_expectations(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, count: Nat, left: Bool) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match expected:
    case None{}:
      Budget.pure(List<&2, Maybe<&2, Value.T>>, none_expectations(count))
    case Some{ty}:
      do Budget.Comp<List<&2, Maybe<&2, Value.T>>>:
        ty : Value.T <- whnf_in(ctx, ty)
        expected_leg_types(left, ty, ctx, count)

# An absent expected leg is represented sparsely for injections. This avoids
# allocating a list proportional to an untrusted arbitrary-precision width.
def injection_leg_types(value: Value.T, +ctx: Check.Ctx, +count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match value:
    case Value.VLan{Shape.SColl{width}, diagram, level}:
      action = Bool.pick(Unit -> Budget.Comp(List<&2, Maybe<&2, Value.T>>), Bignum.equal(width, count),
        unit => requested_leg_types(ctx, diagram, count), unit => Budget.pure(List<&2, Maybe<&2, Value.T>>, []))
      action(Unit{})
    case other: Budget.pure(List<&2, Maybe<&2, Value.T>>, [])

def injection_expectations_nonnegative(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  match expected:
    case None{}: Budget.pure(List<&2, Maybe<&2, Value.T>>, [])
    case Some{ty}:
      do Budget.Comp<List<&2, Maybe<&2, Value.T>>>:
        ty : Value.T <- whnf_in(ctx, ty)
        injection_leg_types(ty, ctx, count)

def injection_expectations(ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +count: Bignum.T) -> Budget.Comp(List<&2, Maybe<&2, Value.T>>):
  action = Bool.pick(Unit -> Budget.Comp(List<&2, Maybe<&2, Value.T>>), Bignum.le(Bignum.zero(), count),
    unit => injection_expectations_nonnegative(ctx, expected, count),
    unit => Budget.failed(List<&2, Maybe<&2, Value.T>>, Error.Mismatch{"a collection width must be nonnegative"}))
  action(Unit{})

@unsafe
def elab_terms(terms: List<&2, Syntax.T>, +ctx: Check.Ctx) -> Budget.Comp(List<&2, Term.T>):
  match terms:
    case Nil{}:
      Budget.pure(List<&2, Term.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Term.T>>:
        head : Term.T <- elab(ctx, None{}, head)
        tail : List<&2, Term.T> <- elab_terms(tail, ctx)
        return head <> tail

@unsafe
def elab_typed_items(items: List<&2, Syntax.T>, expectations: List<&2, Maybe<&2, Value.T>>, +ctx: Check.Ctx) -> Budget.Comp(List<&2, Term.T>):
  match items expectations:
    case Nil{} Nil{}:
      Budget.pure(List<&2, Term.T>, [])
    case Con{head, tail} Con{expected, rest}:
      do Budget.Comp<List<&2, Term.T>>:
        head : Term.T <- elab(ctx, expected, head)
        tail : List<&2, Term.T> <- elab_typed_items(tail, rest, ctx)
        return head <> tail
    case items expectations:
      Budget.failed(List<&2, Term.T>, Error.Mismatch{"the item count and the leg count differ"})

def elab_items(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +items: List<&2, Syntax.T>, left: Bool) -> Budget.Comp(List<&2, Term.T>):
  do Budget.Comp<List<&2, Term.T>>:
    expected : List<&2, Maybe<&2, Value.T>> <- leg_expectations(ctx, expected, List.length(&2, Syntax.T, items), left)
    elab_typed_items(items, expected, ctx)

@unsafe
def elab_pair(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, left: Syntax.T, right: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +shape : Former <- expected_former(ctx, expected, True{}, "a pair", "a pair needs a left former as its expected type")
    +point : Point <- require(Point, point_of(former_shape(shape)), Error.Mismatch{"a pair needs a point former"})
    +left : Term.T <- elab(ctx, Some{point_domain(point)}, left)
    value : Value.T <- eval_in(ctx, left)
    codomain : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), former_diagram(shape), [value]))
    right : Term.T <- elab(ctx, Some{codomain}, right)
    domain : Term.T <- quote_in(ctx, point_domain(point))
    return Term.In{Shape.SPi{point_q(point), point_name(point), domain}, Term.APt{point_q(point), left}, [right]}

@unsafe
def elab_binder(+ctx: Check.Ctx, binder: Syntax.Binder<Syntax.T>) -> Budget.Comp(Bound):
  match binder:
    case Syntax.Binder{+quantity, +name, ty}:
      do Budget.Comp<Bound>:
        +ty : Term.T <- elab(ctx, None{}, ty)
        value : Value.T <- eval_in(ctx, ty)
        return Bound{quantity, name, ty, Check.bind(name, quantity, value, ctx)}

def codomain_value(value: Value.T, +ctx: Check.Ctx) -> Budget.Comp(Maybe<&2, Value.T>):
  match value:
    case Value.VRan{Shape.SPi{q, name, domain}, diagram, level}:
      do Budget.Comp<Maybe<&2, Value.T>>:
        value : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), diagram, [Value.var(size_of(ctx))]))
        return Some{value}
    case Value.VRan{Shape.SZk{q, name, domain}, diagram, level}:
      do Budget.Comp<Maybe<&2, Value.T>>:
        value : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), diagram, [Value.var(size_of(ctx))]))
        return Some{value}
    case other:
      Budget.pure(Maybe<&2, Value.T>, None{})

def cod_of(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>) -> Budget.Comp(Maybe<&2, Value.T>):
  match expected:
    case None{}:
      Budget.pure(Maybe<&2, Value.T>, None{})
    case Some{value}:
      do Budget.Comp<Maybe<&2, Value.T>>:
        value : Value.T <- whnf_in(ctx, value)
        codomain_value(value, ctx)

@unsafe
def elab_fun(binders: List<&2, Syntax.Binder<Syntax.T>>, +ctx: Check.Ctx, expected: Maybe<&2, Value.T>, body: Syntax.T) -> Budget.Comp(Term.T):
  match binders:
    case Nil{}:
      elab(ctx, expected, body)
    case Con{binder, tail}:
      do Budget.Comp<Term.T>:
        +bound : Bound <- elab_binder(ctx, binder)
        expected : Maybe<&2, Value.T> <- cod_of(ctx, expected)
        body : Term.T <- elab_fun(tail, bound_context(bound), expected, body)
        return Term.Sec{Shape.SPi{bound_q(bound), bound_name(bound), bound_type(bound)}, [Term.Leg{[Common.Pair{bound_q(bound), bound_name(bound)}], body}]}

def group_term(kind: Nat, bound: Bound, body: Term.T) -> Term.T:
  match kind bound:
    case 0n Bound{q, name, ty, context}:
      Term.Ran{Shape.SPi{q, name, ty}, body}
    case 1n Bound{q, name, ty, context}:
      Term.Lan{Shape.SPi{q, name, ty}, body}
    case other Bound{q, name, ty, context}:
      Term.Lan{Shape.SZk{q, name, ty}, body}

@unsafe
def elab_group(ctx: Check.Ctx, binder: Syntax.Binder<Syntax.T>, body: Syntax.T, kind: Nat) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +bound : Bound <- elab_binder(ctx, binder)
    body : Term.T <- elab(bound_context(bound), None{}, body)
    return group_term(kind, bound, body)

@unsafe
def elab_app_point(+ctx: Check.Ctx, head: Syntax.T, arg: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +head : Term.T <- elab(ctx, None{}, head)
    ty : Value.T <- type_of(ctx, head)
    shape : Former <- former(False{}, ty, "the head of an application is not a function")
    +point : Point <- require(Point, point_of(former_shape(shape)), Error.Mismatch{"the head of an application is not a function"})
    arg : Term.T <- elab(ctx, Some{point_domain(point)}, arg)
    domain : Term.T <- quote_in(ctx, point_domain(point))
    return Term.Out{Shape.SPi{point_q(point), point_name(point), domain}, Term.APt{point_q(point), arg}, head}

def fhc_expect(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +word: String) -> Budget.Comp(Common.Pair<Value.T, Value.T>):
  do Budget.Comp<Common.Pair<Value.T, Value.T>>:
    +shape : Former <- expected_former(ctx, expected, False{}, word, word ++ " needs a ciphertext type as its expected type")
    level : Value.T <- require(Value.T, fhc_level(former_shape(shape)), Error.Mismatch{word ++ " needs a ciphertext former"})
    ty : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), former_diagram(shape), []))
    return Common.Pair{level, ty}

@unsafe
def elab_cipher(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, left: Syntax.T, right: Syntax.T, +evaluate: Bool) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +cipher : Common.Pair<Value.T, Value.T> <- fhc_expect(ctx, expected, Bool.pick(String, evaluate, "eval", "enc"))
    left : Term.T <- elab(ctx, None{}, left)
    right : Term.T <- elab(ctx, Bool.pick(Maybe<&2, Value.T>, evaluate, None{}, Some{second(Value.T, Value.T, cipher)}), right)
    level : Term.T <- quote_in(ctx, first(Value.T, Value.T, cipher))
    return Term.Sec{Shape.SFhc{level}, legs_of([left, right])}

@unsafe
def elab_dec(+ctx: Check.Ctx, key: Syntax.T, cipher: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    key : Term.T <- elab(ctx, None{}, key)
    +cipher : Term.T <- elab(ctx, None{}, cipher)
    ty : Value.T <- type_of(ctx, cipher)
    ty : Value.T <- whnf_in(ctx, ty)
    shape : Former <- former(False{}, ty, "dec needs a ciphertext as its argument")
    level : Value.T <- require(Value.T, fhc_level(former_shape(shape)), Error.Mismatch{"dec needs a ciphertext former"})
    level : Term.T <- quote_in(ctx, level)
    return Term.Out{Shape.SFhc{level}, Term.APt{Quantity.QOne{}, key}, cipher}

def mpc_shape(shape: Shape.T<Value.T>, message: String) -> Budget.Comp(Common.Pair<Value.T, Value.T>):
  match shape:
    case Shape.SMpc{parties, access}:
      Budget.pure(Common.Pair<Value.T, Value.T>, Common.Pair{parties, access})
    case other:
      Budget.failed(Common.Pair<Value.T, Value.T>, Error.Mismatch{message})

type MpcExpected is Data:
  MpcExpected{parties: Term.T, access: Term.T, parties_value: Value.T, ty: Value.T}

def mpc_parties(expected: MpcExpected) -> Term.T:
  match expected:
    case MpcExpected{parties, access, value, ty}:
      parties

def mpc_access(expected: MpcExpected) -> Term.T:
  match expected:
    case MpcExpected{parties, access, value, ty}:
      access

def mpc_value(expected: MpcExpected) -> Value.T:
  match expected:
    case MpcExpected{parties, access, value, ty}:
      value

def mpc_type(expected: MpcExpected) -> Value.T:
  match expected:
    case MpcExpected{parties, access, value, ty}:
      ty

def mpc_expect(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +word: String) -> Budget.Comp(MpcExpected):
  do Budget.Comp<MpcExpected>:
    +shape : Former <- expected_former(ctx, expected, False{}, word, word ++ " needs a share type as its expected type")
    +mpc : Common.Pair<Value.T, Value.T> <- mpc_shape(former_shape(shape), word ++ " needs a share former")
    parties : Term.T <- quote_in(ctx, first(Value.T, Value.T, mpc))
    access : Term.T <- quote_in(ctx, second(Value.T, Value.T, mpc))
    ty : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), former_diagram(shape), []))
    return MpcExpected{parties, access, first(Value.T, Value.T, mpc), ty}

@unsafe
def elab_share(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, value: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +mpc : MpcExpected <- mpc_expect(ctx, expected, "share")
    value : Term.T <- elab(ctx, Some{mpc_type(mpc)}, value)
    return Term.Sec{Shape.SMpc{mpc_parties(mpc), mpc_access(mpc)}, legs_of([value])}

@unsafe
def elab_input(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, party: Syntax.T, value: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +mpc : MpcExpected <- mpc_expect(ctx, expected, "input")
    party : Term.T <- elab(ctx, Some{mpc_value(mpc)}, party)
    value : Term.T <- elab(ctx, Some{mpc_type(mpc)}, value)
    return Term.Sec{Shape.SMpc{mpc_parties(mpc), mpc_access(mpc)}, legs_of([party, value])}

@unsafe
def elab_join(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, parties: Syntax.T, function: Syntax.T, shares: List<&2, Syntax.T>) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +mpc : MpcExpected <- mpc_expect(ctx, expected, "mpc")
    count : Bignum.T <- Budget.lift(Bignum.T, Rules.mpc_parties(ctx, mpc_value(mpc)))
    subset : Term.T <- Budget.lift(Term.T, Rules.mpc_sub_ty(count))
    subset : Value.T <- eval_in(ctx, subset)
    parties : Term.T <- elab(ctx, Some{subset}, parties)
    function : Term.T <- elab(ctx, None{}, function)
    shares : List<&2, Term.T> <- elab_terms(shares, ctx)
    return Term.Sec{Shape.SMpc{mpc_parties(mpc), mpc_access(mpc)}, legs_of(parties <> function <> shares)}

def proof_quantity(quantity: Maybe<&2, Quantity.T>) -> Quantity.T:
  match quantity:
    case None{}:
      Quantity.QZero{}
    case Some{q}:
      q

@unsafe
def elab_open(+ctx: Check.Ctx, quantity: Maybe<&2, Quantity.T>, parties: Syntax.T, proof: Syntax.T, share: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +share : Term.T <- elab(ctx, None{}, share)
    ty : Value.T <- type_of(ctx, share)
    ty : Value.T <- whnf_in(ctx, ty)
    shape : Former <- former(False{}, ty, "open needs a share as its argument")
    +mpc : Common.Pair<Value.T, Value.T> <- mpc_shape(former_shape(shape), "open needs a share former")
    count : Bignum.T <- Budget.lift(Bignum.T, Rules.mpc_parties(ctx, first(Value.T, Value.T, mpc)))
    +subset : Term.T <- Budget.lift(Term.T, Rules.mpc_sub_ty(count))
    subset_value : Value.T <- eval_in(ctx, subset)
    parties : Term.T <- elab(ctx, Some{subset_value}, parties)
    proof : Term.T <- elab(ctx, None{}, proof)
    parties_type : Term.T <- quote_in(ctx, first(Value.T, Value.T, mpc))
    access : Term.T <- quote_in(ctx, second(Value.T, Value.T, mpc))
    return Term.Out{Shape.SMpc{parties_type, access},
      Term.APt{proof_quantity(quantity), Term.In{Shape.SPi{Quantity.QZero{}, "q", subset}, Term.APt{Quantity.QZero{}, parties}, [proof]}}, share}

@unsafe
def elab_prove(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, instance: Syntax.T, witness: Syntax.T, relation: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +shape : Former <- expected_former(ctx, expected, True{}, "a proof", "a proof needs a zk type as its expected type")
    +point : Point <- require(Point, zk_point(former_shape(shape)), Error.Mismatch{"a proof needs a zk former"})
    +instance : Term.T <- elab(ctx, None{}, instance)
    instance_type : Value.T <- type_of(ctx, instance)
    +witness : Term.T <- elab(ctx, Some{point_domain(point)}, witness)
    witness_value : Value.T <- eval_in(ctx, witness)
    relation_type : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), former_diagram(shape), [witness_value]))
    relation : Term.T <- elab(ctx, Some{relation_type}, relation)
    domain : Term.T <- quote_in(ctx, point_domain(point))
    return Term.In{Shape.SZk{point_q(point), point_name(point), domain}, Term.APt{point_q(point), witness}, [relation, instance]}

@unsafe
def elab_verify(+ctx: Check.Ctx, instance: Syntax.T, proof: Syntax.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    instance : Term.T <- elab(ctx, None{}, instance)
    instance_type : Value.T <- type_of(ctx, instance)
    +proof : Term.T <- elab(ctx, None{}, proof)
    ty : Value.T <- type_of(ctx, proof)
    ty : Value.T <- whnf_in(ctx, ty)
    +shape : Former <- former(True{}, ty, "verify needs a zk type as the type of its proof")
    +point : Point <- require(Point, zk_point(former_shape(shape)), Error.Mismatch{"verify needs a zk former"})
    domain : Term.T <- quote_in(ctx, point_domain(point))
    relation_value : Value.T <- Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), former_diagram(shape), [Value.var(size_of(ctx))]))
    relation : Term.T <- Budget.lift(Term.T, Eval.quote(globals_of(ctx), 2n+size_of(ctx), relation_value))
    return Term.Elim{Term.ElimData{Shape.SZk{point_q(point), point_name(point), domain}, proof, Quantity.QOne{},
      Some{Term.Motive{None{}, [], "p", Term.Univ{Level.zero()}}},
      [Common.Pair{Term.aleg(Term.T, 0n), Term.Leg{[Common.Pair{point_q(point), point_name(point)}, Common.Pair{Quantity.QZero{}, "r"}], relation}}]}}


def proj_branch(+quantity: Quantity.T, which: Nat) -> List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>:
  [Common.Pair{Term.aleg(Term.T, 0n), Term.Leg{[Common.Pair{quantity, "x"}, Common.Pair{Quantity.QMany{}, "y"}], Term.Var{Bool.pick(Nat, Nat.is_eq(which, 0n), 1n, 0n)}}}]

def projection_index(+index: Bignum.T) -> Budget.Comp(Nat):
  Bool.pick(Budget.Comp(Nat), Bignum.equal(index, Bignum.one()), Budget.pure(Nat, 0n),
    Bool.pick(Budget.Comp(Nat), Bignum.equal(index, Bignum.of_u32(2)), Budget.pure(Nat, 1n),
      Budget.failed(Nat, Error.Wrong_leg{"a pair carries the projections .1 and .2 only"})))

def projection_body(which: Nat, +ctx: Check.Ctx, shape: Shape.T<Value.T>, +point: Point, diagram: Value.Closure<Value.T>) -> Budget.Comp(Value.T):
  match which:
    case 0n:
      Budget.pure(Value.T, point_domain(point))
    case other:
      do Budget.Comp<Value.T>:
        domain : Term.T <- Budget.lift(Term.T, Eval.quote(globals_of(ctx), 1n+size_of(ctx), point_domain(point)))
        point_value : Value.T <- Budget.lift(Value.T, Eval.elim_value(globals_of(ctx), Value.StuckElim{shape, Quantity.QOne{}, Some{Term.Motive{None{}, [], "self", domain}}, proj_branch(point_q(point), 0n), env_of(ctx)}, Value.var(size_of(ctx))))
        Budget.lift(Value.T, Eval.open_closure(globals_of(ctx), diagram, [point_value]))

def elab_pair_proj(+ctx: Check.Ctx, scrut: Term.T, +shape: Shape.T<Value.T>, diagram: Value.Closure<Value.T>, index: Bignum.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +point : Point <- require(Point, point_of(shape), Error.Mismatch{"a pair projection needs a point former"})
    +which : Nat <- projection_index(index)
    body_value : Value.T <- projection_body(which, ctx, shape, point, diagram)
    body : Term.T <- Budget.lift(Term.T, Eval.quote(globals_of(ctx), 1n+size_of(ctx), body_value))
    domain : Term.T <- quote_in(ctx, point_domain(point))
    return Term.Elim{Term.ElimData{Shape.SPi{point_q(point), point_name(point), domain}, scrut, Quantity.QOne{}, Some{Term.Motive{None{}, [], "self", body}}, proj_branch(point_q(point), which)}}

def projection_type(ty: Value.T, ctx: Check.Ctx, scrut: Term.T, index: Bignum.T) -> Budget.Comp(Term.T):
  match ty:
    case Value.VLan{shape, diagram, level}:
      elab_pair_proj(ctx, scrut, shape, diagram, index)
    case Value.VRan{Shape.SColl{count}, diagram, level}:
      Budget.pure(Term.T, Term.Out{Shape.SColl{count}, Term.ALeg{index}, scrut})
    case Value.VRan{shape, diagram, level}:
      Budget.failed(Term.T, Error.Mismatch{"a leg projection needs a collection former"})
    case other:
      Budget.failed(Term.T, Error.Mismatch{"a projection reads a pair or a tuple, and this scrutinee is neither"})

@unsafe
def elab_proj(+ctx: Check.Ctx, scrut: Syntax.T, index: Bignum.T) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +scrut : Term.T <- elab(ctx, None{}, scrut)
    ty : Value.T <- type_of(ctx, scrut)
    projection_type(ty, ctx, scrut, index)

def family_params(family: Positivity.Family) -> Positivity.Telescope:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      params

def family_indices(family: Positivity.Family) -> Positivity.Telescope:
  match family:
    case Positivity.Family{name, params, indices, level, status, ctors, positive}:
      indices

def ctor_name(ctor: Positivity.Ctor) -> String:
  match ctor:
    case Positivity.Ctor{name, args, indices, arity, self_rec}:
      name

def ctor_args(ctor: Positivity.Ctor) -> Positivity.Telescope:
  match ctor:
    case Positivity.Ctor{name, args, indices, arity, self_rec}:
      args

def ctor_indices(ctor: Positivity.Ctor) -> List<&2, Term.T>:
  match ctor:
    case Positivity.Ctor{name, args, indices, arity, self_rec}:
      indices

def prefer_ctor(candidate: Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>, fallback: Common.Pair<Positivity.Family, Positivity.Ctor>) -> Common.Pair<Positivity.Family, Positivity.Ctor>:
  match candidate:
    case Some{found}:
      found
    case None{}:
      fallback

def expected_ctor_family(family: Maybe<&2, Positivity.Family>, name: String) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match family:
    case None{}:
      None{}
    case Some{+family}:
      ctor_candidate(Positivity.ctor_of(name, family), family)

def expected_ctor(name: Maybe<&2, String>, ctx: Check.Ctx, ctor: String) -> Maybe<&2, Common.Pair<Positivity.Family, Positivity.Ctor>>:
  match name:
    case None{}:
      None{}
    case Some{name}:
      expected_ctor_family(Global.find_family(name, globals_of(ctx)), ctor)

def ctor_in(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, family: Positivity.Family, +ctor: Positivity.Ctor) -> Common.Pair<Positivity.Family, Positivity.Ctor>:
  prefer_ctor(expected_ctor(expected_family(ctx, expected), ctx, ctor_name(ctor)), Common.Pair{family, ctor})

def prepend_split(head: Term.T, parts: Common.Pair<List<&2, Term.T>, List<&2, Term.T>>) -> Common.Pair<List<&2, Term.T>, List<&2, Term.T>>:
  match parts:
    case Common.Pair{before, after}:
      Common.Pair{head <> before, after}

def split_at(count: Nat, terms: List<&2, Term.T>) -> Common.Pair<List<&2, Term.T>, List<&2, Term.T>>:
  match count terms:
    case 0n terms:
      Common.Pair{[], terms}
    case 1n+count Nil{}:
      Common.Pair{[], []}
    case 1n+count Con{head, tail}:
      prepend_split(head, split_at(count, tail))

@unsafe
def elab_fam_ref(+ctx: Check.Ctx, +family: Positivity.Family, +args: List<&2, Syntax.T>) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +params_count : Nat = List.length(&2, Positivity.Binder, family_params(family))
    checked : Unit <- arity_is(family_name(family), Nat.add(params_count, List.length(&2, Positivity.Binder, family_indices(family))), List.length(&2, Syntax.T, args))
    args : List<&2, Term.T> <- elab_terms(args, ctx)
    +parts : Common.Pair<List<&2, Term.T>, List<&2, Term.T>> = split_at(params_count, args)
    return Term.Lan{Shape.SMu{family_name(family), second(List<&2, Term.T>, List<&2, Term.T>, parts)}, diagram_of(first(List<&2, Term.T>, List<&2, Term.T>, parts))}

def matching_param_env(same: Bool, +ctx: Check.Ctx, diagram: Value.Closure<Value.T>, error: Error.T) -> Budget.Comp(List<&2, Value.T>):
  match same:
    case True{}:
      Budget.lift(List<&2, Value.T>, Rules.mu_param_env(ctx, diagram))
    case False{}:
      Budget.failed(List<&2, Value.T>, error)

def ctor_param_shape(value: Value.T, ctx: Check.Ctx, family: String, +error: Error.T) -> Budget.Comp(List<&2, Value.T>):
  match value:
    case Value.VLan{Shape.SMu{name, indices}, diagram, level}:
      matching_param_env(String.eq(name, family), ctx, diagram, error)
    case other:
      Budget.failed(List<&2, Value.T>, error)

def ctor_param_env(params: Positivity.Telescope, +ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +family: String, +ctor: String) -> Budget.Comp(List<&2, Value.T>):
  match params:
    case Nil{}:
      Budget.pure(List<&2, Value.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Value.T>>:
        ty : Value.T <- require(Value.T, expected, no_expect("the constructor " ++ ctor))
        ty : Value.T <- whnf_in(ctx, ty)
        ctor_param_shape(ty, ctx, family, Error.Mismatch{"the constructor " ++ ctor ++ " expects family " ++ family})

type ConstructorFields is Data:
  ConstructorFields{terms: List<&2, Term.T>, env: List<&2, Value.T>}

def constructor_terms(fields: ConstructorFields) -> List<&2, Term.T>:
  match fields:
    case ConstructorFields{terms, env}:
      List.reverse(&2, Term.T, terms)

def constructor_env(fields: ConstructorFields) -> List<&2, Value.T>:
  match fields:
    case ConstructorFields{terms, env}:
      env

@unsafe
def elab_ctor_fields(telescope: Positivity.Telescope, args: List<&2, Syntax.T>, +ctx: Check.Ctx, +env: List<&2, Value.T>, terms: List<&2, Term.T>) -> Budget.Comp(ConstructorFields):
  match telescope args:
    case Nil{} Nil{}:
      Budget.pure(ConstructorFields, ConstructorFields{terms, env})
    case Con{Positivity.Binder{quantity, name, ty}, tail} Con{arg, args}:
      do Budget.Comp<ConstructorFields>:
        ty : Value.T <- Budget.lift(Value.T, Eval.eval(globals_of(ctx), env, ty))
        +term : Term.T <- elab(ctx, Some{ty}, arg)
        value : Value.T <- eval_in(ctx, term)
        elab_ctor_fields(tail, args, ctx, value <> env, term <> terms)
    case telescope args:
      Budget.failed(ConstructorFields, Error.Mismatch{"constructor field arity changed during elaboration"})

def eval_terms(terms: List<&2, Term.T>, +ctx: Check.Ctx, +env: List<&2, Value.T>) -> Budget.Comp(List<&2, Value.T>):
  match terms:
    case Nil{}:
      Budget.pure(List<&2, Value.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Value.T>>:
        head : Value.T <- Budget.lift(Value.T, Eval.eval(globals_of(ctx), env, head))
        tail : List<&2, Value.T> <- eval_terms(tail, ctx, env)
        return head <> tail

def quote_values(values: List<&2, Value.T>, +ctx: Check.Ctx) -> Budget.Comp(List<&2, Term.T>):
  match values:
    case Nil{}:
      Budget.pure(List<&2, Term.T>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Term.T>>:
        head : Term.T <- quote_in(ctx, head)
        tail : List<&2, Term.T> <- quote_values(tail, ctx)
        return head <> tail

@unsafe
def elab_ctor_ref(+ctx: Check.Ctx, +expected: Maybe<&2, Value.T>, family: Positivity.Family, ctor: Positivity.Ctor, +args: List<&2, Syntax.T>) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +resolved : Common.Pair<Positivity.Family, Positivity.Ctor> = ctor_in(ctx, expected, family, ctor)
    +family : Positivity.Family = first(Positivity.Family, Positivity.Ctor, resolved)
    +ctor : Positivity.Ctor = second(Positivity.Family, Positivity.Ctor, resolved)
    checked : Unit <- arity_is(ctor_name(ctor), List.length(&2, Positivity.Binder, ctor_args(ctor)), List.length(&2, Syntax.T, args))
    env : List<&2, Value.T> <- ctor_param_env(family_params(family), ctx, expected, family_name(family), ctor_name(ctor))
    +fields : ConstructorFields <- elab_ctor_fields(ctor_args(ctor), args, ctx, env, [])
    indices : List<&2, Value.T> <- eval_terms(ctor_indices(ctor), ctx, constructor_env(fields))
    indices : List<&2, Term.T> <- quote_values(indices, ctx)
    return Term.In{Shape.SMu{family_name(family), indices}, Term.ACtor{ctor_name(ctor)}, constructor_terms(fields)}

@unsafe
def elab_mu_ref(reference: MuRef, ctx: Check.Ctx, expected: Maybe<&2, Value.T>, args: List<&2, Syntax.T>) -> Budget.Comp(Term.T):
  match reference:
    case RFam{family}:
      elab_fam_ref(ctx, family, args)
    case RCtor{family, ctor}:
      elab_ctor_ref(ctx, expected, family, ctor, args)

def variable(index: Maybe<&2, Nat>, name: String) -> Term.T:
  match index:
    case None{}:
      Term.Global{name}
    case Some{index}:
      Term.Var{index}

@unsafe
def elaborate_name(reference: Maybe<&2, MuRef>, +ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +name: String) -> Budget.Comp(Term.T):
  match reference:
    case None{}:
      Budget.pure(Term.T, variable(Check.index_of_name(ctx, name), name))
    case Some{reference}:
      elab_mu_ref(reference, ctx, expected, [])

def spine_reference(spine: Spine, ctx: Check.Ctx) -> Maybe<&2, MuRef>:
  match spine:
    case Spine{Syntax.SVar{name}, args}:
      mu_ref_of(ctx, name)
    case other:
      None{}

def spine_args(spine: Spine) -> List<&2, Syntax.T>:
  match spine:
    case Spine{head, args}:
      args

@unsafe
def elaborate_application(reference: Maybe<&2, MuRef>, ctx: Check.Ctx, expected: Maybe<&2, Value.T>, head: Syntax.T, arg: Syntax.T, args: List<&2, Syntax.T>) -> Budget.Comp(Term.T):
  match reference:
    case None{}:
      elab_app_point(ctx, head, arg)
    case Some{reference}:
      elab_mu_ref(reference, ctx, expected, args)

@unsafe
def elab_app(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, +head: Syntax.T, +arg: Syntax.T) -> Budget.Comp(Term.T):
  +spine = spine_of(Syntax.SApp{head, arg}, [])
  elaborate_application(spine_reference(spine, ctx), ctx, expected, head, arg, spine_args(spine))

def branch_target(motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>, +ctx: Check.Ctx, indices: List<&2, Value.T>, self: Value.T) -> Budget.Comp(Maybe<&2, Value.T>):
  match motive:
    case None{}:
      Budget.pure(Maybe<&2, Value.T>, expected)
    case Some{Term.Motive{ind, names, name, body}}:
      do Budget.Comp<Maybe<&2, Value.T>>:
        value : Value.T <- Budget.lift(Value.T, Eval.eval(globals_of(ctx), self <> List.append(&2, Value.T, List.reverse(&2, Value.T, indices), env_of(ctx)), body))
        return Some{value}

def coll_motive_plain(ind: Maybe<&2, String>) -> Budget.Comp(Unit):
  match ind:
    case None{}:
      Budget.pure(Unit, Unit{})
    case Some{name}:
      Budget.failed(Unit, Error.Mismatch{"a case at a collection takes no index clause and its motive names " ++ name})

@unsafe
def elab_motive(motive: Maybe<&2, Syntax.Motive<Syntax.T>>, ctx: Check.Ctx, ty: Value.T) -> Budget.Comp(Maybe<&2, Term.Motive<Term.T>>):
  match motive:
    case None{}:
      Budget.pure(Maybe<&2, Term.Motive<Term.T>>, None{})
    case Some{Syntax.Motive{+self, ind, indices, body}}:
      do Budget.Comp<Maybe<&2, Term.Motive<Term.T>>>:
        checked : Unit <- coll_motive_plain(ind)
        body : Term.T <- elab(Check.bind(self, Quantity.QZero{}, ty, ctx), None{}, body)
        return Some{Term.Motive{None{}, [], self, body}}

@unsafe
def coll_branch_body(binders: List<&2, Syntax.Binder<Syntax.T>>, +ctx: Check.Ctx, +shape: Shape.T<Value.T>, legs: List<&2, Value.VLeg<Value.T>>, motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>, +index: Bignum.T, body: Syntax.T) -> Budget.Comp(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>):
  match binders:
    case Con{Syntax.Binder{+quantity, +name, annotation}, Nil{}}:
      do Budget.Comp<Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>:
        ty : Value.T <- Budget.lift(Value.T, Rules.coll_leg_ty(ctx, legs, index))
        target : Maybe<&2, Value.T> <- branch_target(motive, expected, ctx, [], Value.VIn{shape, Value.VALeg{index}, [Value.var(size_of(ctx))]})
        body : Term.T <- elab(Check.bind(name, quantity, ty, ctx), target, body)
        return Common.Pair{Term.ALeg{index}, Term.Leg{[Common.Pair{quantity, name}], body}}
    case other:
      Budget.failed(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>, Error.Missing_branch{"each branch binds its payload once"})

@unsafe
def elab_branch(branch: Syntax.Branch<Syntax.T>, ctx: Check.Ctx, shape: Shape.T<Value.T>, legs: List<&2, Value.VLeg<Value.T>>, motive: Maybe<&2, Term.Motive<Term.T>>, expected: Maybe<&2, Value.T>) -> Budget.Comp(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>):
  match branch:
    case Syntax.BrCtor{name, fields, body}:
      Budget.failed(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>, Error.Wrong_leg{"the branch at " ++ name ++ " keys a constructor and a case at a collection keys a leg"})
    case Syntax.BrLeg{index, binders, body}:
      coll_branch_body(binders, ctx, shape, legs, motive, expected, index, body)

@unsafe
def elab_coll_branches(branches: List<&2, Syntax.Branch<Syntax.T>>, +ctx: Check.Ctx, +shape: Shape.T<Value.T>, +legs: List<&2, Value.VLeg<Value.T>>, +motive: Maybe<&2, Term.Motive<Term.T>>, +expected: Maybe<&2, Value.T>) -> Budget.Comp(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>):
  match branches:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>>:
        head : Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>> <- elab_branch(head, ctx, shape, legs, motive, expected)
        tail : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- elab_coll_branches(tail, ctx, shape, legs, motive, expected)
        return head <> tail

@unsafe
def elab_coll_case(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, scrut: Term.T, +count: Bignum.T, +diagram: Value.Closure<Value.T>, level: Maybe<&2, Level.T>, motive: Maybe<&2, Syntax.Motive<Syntax.T>>, branches: List<&2, Syntax.Branch<Syntax.T>>) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    legs : List<&2, Value.VLeg<Value.T>> <- Budget.lift(List<&2, Value.VLeg<Value.T>>, Rules.coll_legs_of(ctx, diagram))
    +motive : Maybe<&2, Term.Motive<Term.T>> <- elab_motive(motive, ctx, Value.VLan{Shape.SColl{count}, diagram, level})
    branches : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- elab_coll_branches(branches, ctx, Shape.SColl{count}, legs, motive, expected)
    return Term.Elim{Term.ElimData{Shape.SColl{count}, scrut, Quantity.QOne{}, motive, branches}}

type BindState is Data:
  BindState{ctx: Check.Ctx, env: List<&2, Value.T>, values: List<&2, Value.T>}

def bind_context(state: BindState) -> Check.Ctx:
  match state:
    case BindState{ctx, env, values}:
      ctx

def bind_env(state: BindState) -> List<&2, Value.T>:
  match state:
    case BindState{ctx, env, values}:
      env

def bind_values(state: BindState) -> List<&2, Value.T>:
  match state:
    case BindState{ctx, env, values}:
      List.reverse(&2, Value.T, values)

def bind_indices(telescope: Positivity.Telescope, names: List<&2, String>, +ctx: Check.Ctx, +env: List<&2, Value.T>, values: List<&2, Value.T>) -> Budget.Comp(BindState):
  match telescope names:
    case Nil{} Nil{}:
      Budget.pure(BindState, BindState{ctx, env, values})
    case Con{Positivity.Binder{q, original, ty}, tail} Con{name, names}:
      do Budget.Comp<BindState>:
        ty : Value.T <- Budget.lift(Value.T, Eval.eval(globals_of(ctx), env, ty))
        +value : Value.T = Value.var(size_of(ctx))
        bind_indices(tail, names, Check.bind(name, Quantity.QZero{}, ty, ctx), value <> env, value <> values)
    case telescope names:
      Budget.failed(BindState, Error.Mismatch{"motive index arity changed during elaboration"})

def motive_arity(name: String, +want: Nat, +got: Nat) -> Budget.Comp(Unit):
  Bool.pick(Budget.Comp(Unit), Nat.is_eq(want, got), Budget.pure(Unit, Unit{}),
    Budget.failed(Unit, Error.Mismatch{"the motive of " ++ name ++ " binds " ++ Nat.show(got) ++ " index names and the family has " ++ Nat.show(want)}))

@unsafe
def elab_mu_motive(motive: Maybe<&2, Syntax.Motive<Syntax.T>>, ctx: Check.Ctx, +name: String, +family: Positivity.Family, diagram: Value.Closure<Value.T>, level: Maybe<&2, Level.T>, env: List<&2, Value.T>) -> Budget.Comp(Maybe<&2, Term.Motive<Term.T>>):
  match motive:
    case None{}:
      Budget.pure(Maybe<&2, Term.Motive<Term.T>>, None{})
    case Some{Syntax.Motive{+self, ind, +indices, body}}:
      do Budget.Comp<Maybe<&2, Term.Motive<Term.T>>>:
        checked : Unit <- motive_arity(name, List.length(&2, Positivity.Binder, family_indices(family)), List.length(&2, String, indices))
        +state : BindState <- bind_indices(family_indices(family), indices, ctx, env, [])
        self_type : Value.T = Value.VLan{Shape.SMu{name, bind_values(state)}, diagram, level}
        body : Term.T <- elab(Check.bind(self, Quantity.QZero{}, self_type, bind_context(state)), None{}, body)
        return Some{Term.Motive{ind, indices, self, body}}

def field_agrees(agrees: Bool, name: String) -> Budget.Comp(Unit):
  match agrees:
    case True{}:
      Budget.pure(Unit, Unit{})
    case False{}:
      Budget.failed(Unit, Error.Mismatch{"the annotation of constructor field " ++ name ++ " differs from its declared type"})

@unsafe
def check_field_annotation(annotation: Maybe<&2, Syntax.T>, +ctx: Check.Ctx, expected: Value.T, name: String) -> Budget.Comp(Unit):
  match annotation:
    case None{}:
      Budget.pure(Unit, Unit{})
    case Some{annotation}:
      do Budget.Comp<Unit>:
        +term : Term.T <- elab(ctx, None{}, annotation)
        level : Level.T <- Engine.infer_univ(ctx, term)
        actual : Value.T <- eval_in(ctx, term)
        agrees : Bool <- Engine.conv_type(ctx, actual, expected)
        field_agrees(agrees, name)

@unsafe
def bind_fields(telescope: Positivity.Telescope, fields: List<&2, Syntax.Field<Syntax.T>>, +ctx: Check.Ctx, +env: List<&2, Value.T>, values: List<&2, Value.T>) -> Budget.Comp(BindState):
  match telescope fields:
    case Nil{} Nil{}:
      Budget.pure(BindState, BindState{ctx, env, values})
    case Con{Positivity.Binder{q, original, ty}, tail} Con{Syntax.Field{quantity, +name, annotation}, fields}:
      do Budget.Comp<BindState>:
        +ty : Value.T <- Budget.lift(Value.T, Eval.eval(globals_of(ctx), env, ty))
        checked : Unit <- check_field_annotation(annotation, ctx, ty, name)
        +value : Value.T = Value.var(size_of(ctx))
        bind_fields(tail, fields, Check.bind(name, quantity, ty, ctx), value <> env, value <> values)
    case telescope fields:
      Budget.failed(BindState, Error.Missing_branch{"constructor field arity changed during elaboration"})

def branch_arity(+name: String, +want: Nat, +got: Nat) -> Budget.Comp(Unit):
  Bool.pick(Budget.Comp(Unit), Nat.is_eq(want, got), Budget.pure(Unit, Unit{}),
    Budget.failed(Unit, Error.Missing_branch{"the branch at " ++ name ++ " binds " ++ Nat.show(got) ++ " fields and " ++ name ++ " takes " ++ Nat.show(want)}))

def field_binders(fields: List<&2, Syntax.Field<Syntax.T>>) -> List<&2, Common.Pair<Quantity.T, String>>:
  match fields:
    case Nil{}:
      []
    case Con{Syntax.Field{q, name, ty}, tail}:
      Common.Pair{q, name} <> field_binders(tail)

@unsafe
def elab_mu_branch(branch: Syntax.Branch<Syntax.T>, +ctx: Check.Ctx, +name: String, family: Positivity.Family, motive: Maybe<&2, Term.Motive<Term.T>>, env: List<&2, Value.T>, expected: Maybe<&2, Value.T>) -> Budget.Comp(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>):
  match branch:
    case Syntax.BrLeg{index, binders, body}:
      Budget.failed(Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>, Error.Wrong_leg{"the branch " ++ Bignum.to_string(index) ++ " keys a leg and a case at " ++ name ++ " keys a constructor"})
    case Syntax.BrCtor{+ctor_name, +fields, body}:
      do Budget.Comp<Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>:
        +ctor : Positivity.Ctor <- require(Positivity.Ctor, Positivity.ctor_of(ctor_name, family), Error.Unbound{ctor_name ++ " is not a constructor of " ++ name})
        checked : Unit <- branch_arity(ctor_name, List.length(&2, Positivity.Binder, ctor_args(ctor)), List.length(&2, Syntax.Field<Syntax.T>, fields))
        +state : BindState <- bind_fields(ctor_args(ctor), fields, ctx, env, [])
        +indices : List<&2, Value.T> <- eval_terms(ctor_indices(ctor), ctx, bind_env(state))
        target : Maybe<&2, Value.T> <- branch_target(motive, expected, ctx, indices, Value.VIn{Shape.SMu{name, indices}, Value.VACtor{ctor_name}, bind_values(state)})
        body : Term.T <- elab(bind_context(state), target, body)
        return Common.Pair{Term.ACtor{ctor_name}, Term.Leg{field_binders(fields), body}}

@unsafe
def elab_mu_branches(branches: List<&2, Syntax.Branch<Syntax.T>>, +ctx: Check.Ctx, +name: String, +family: Positivity.Family, +motive: Maybe<&2, Term.Motive<Term.T>>, +env: List<&2, Value.T>, +expected: Maybe<&2, Value.T>) -> Budget.Comp(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>):
  match branches:
    case Nil{}:
      Budget.pure(List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>>:
        head : Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>> <- elab_mu_branch(head, ctx, name, family, motive, env, expected)
        tail : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- elab_mu_branches(tail, ctx, name, family, motive, env, expected)
        return head <> tail

@unsafe
def elab_mu_case(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, scrut: Term.T, +name: String, indices: List<&2, Value.T>, +diagram: Value.Closure<Value.T>, level: Maybe<&2, Level.T>, motive: Maybe<&2, Syntax.Motive<Syntax.T>>, branches: List<&2, Syntax.Branch<Syntax.T>>) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +family : Positivity.Family <- require(Positivity.Family, Global.find_family(name, globals_of(ctx)), Error.Unbound{"the family " ++ name ++ " is not declared"})
    +env : List<&2, Value.T> <- Budget.lift(List<&2, Value.T>, Rules.mu_param_env(ctx, diagram))
    +motive : Maybe<&2, Term.Motive<Term.T>> <- elab_mu_motive(motive, ctx, name, family, diagram, level, env)
    branches : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- elab_mu_branches(branches, ctx, name, family, motive, env, expected)
    indices : List<&2, Term.T> <- quote_values(indices, ctx)
    return Term.Elim{Term.ElimData{Shape.SMu{name, indices}, scrut, Quantity.QOne{}, motive, branches}}

@unsafe
def eliminate_type(ty: Value.T, fibered: Bool, ctx: Check.Ctx, expected: Maybe<&2, Value.T>, scrut: Term.T, motive: Maybe<&2, Syntax.Motive<Syntax.T>>, branches: List<&2, Syntax.Branch<Syntax.T>>) -> Budget.Comp(Term.T):
  match ty fibered:
    case Value.VLan{Shape.SMu{name, indices}, diagram, level} fibered:
      elab_mu_case(ctx, expected, scrut, name, indices, diagram, level, motive, branches)
    case Value.VLan{Shape.SColl{count}, diagram, level} False{}:
      elab_coll_case(ctx, expected, scrut, count, diagram, level, motive, branches)
    case Value.VLan{shape, diagram, level} False{}:
      Budget.failed(Term.T, Error.Mismatch{"a case eliminates a collection"})
    case other True{}:
      Budget.failed(Term.T, Error.Mismatch{"a match needs a mu family as the type of its scrutinee"})
    case other False{}:
      Budget.failed(Term.T, Error.Mismatch{"a case needs a left former as the type of its scrutinee"})

@unsafe
def elab_case(+ctx: Check.Ctx, expected: Maybe<&2, Value.T>, scrut: Syntax.T, motive: Maybe<&2, Syntax.Motive<Syntax.T>>, branches: List<&2, Syntax.Branch<Syntax.T>>, fibered: Bool) -> Budget.Comp(Term.T):
  do Budget.Comp<Term.T>:
    +scrut : Term.T <- elab(ctx, None{}, scrut)
    ty : Value.T <- type_of(ctx, scrut)
    eliminate_type(ty, fibered, ctx, expected, scrut, motive, branches)

def optional_expectation(value: Maybe<&2, Maybe<&2, Value.T>>) -> Maybe<&2, Value.T>:
  match value:
    case None{}:
      None{}
    case Some{value}:
      value

@unsafe
def elab(ctx, expected, syntax):
  match syntax:
    case Syntax.SVar{+name}:
      elaborate_name(mu_ref_of(ctx, name), ctx, expected, name)
    case Syntax.SNat{value}:
      Budget.pure(Term.T, Term.Lit{Literal.LInt{value}})
    case Syntax.SProp{}:
      Budget.pure(Term.T, Term.Univ{Level.zero()})
    case Syntax.SType{level}:
      univ_of_int(Bignum.add(Bignum.one(), level))
    case Syntax.SSort{level}:
      do Budget.Comp<Term.T>:
        level : Level.T <- Budget.lift(Level.T, Universe.lower(level))
        return Term.Univ{level}
    case Syntax.SPrim{prim}:
      Budget.pure(Term.T, Term.Global{Syntax.prim_name(prim)})
    case Syntax.SUnit{}:
      Budget.pure(Term.T, unit_val())
    case Syntax.SAuto{}:
      Budget.pure(Term.T, Term.Auto{})
    case Syntax.SPair{left, right}:
      elab_pair(ctx, expected, left, right)
    case Syntax.STuple{+items}:
      do Budget.Comp<Term.T>:
        terms : List<&2, Term.T> <- elab_items(ctx, expected, items, False{})
        return Term.Sec{Shape.coll(Term.T, List.length(&2, Syntax.T, items)), legs_of(terms)}
    case Syntax.SSum{items}:
      do Budget.Comp<Term.T>:
        terms : List<&2, Term.T> <- elab_terms(items, ctx)
        return sum_ty(terms)
    case Syntax.SProd{items}:
      do Budget.Comp<Term.T>:
        terms : List<&2, Term.T> <- elab_terms(items, ctx)
        return prod_ty(terms)
    case Syntax.SProj{head, index}:
      elab_proj(ctx, head, index)
    case Syntax.SInj{+index, +count, value}:
      do Budget.Comp<Term.T>:
        expectations : List<&2, Maybe<&2, Value.T>> <- injection_expectations(ctx, expected, count)
        value : Term.T <- elab(ctx, optional_expectation(Bignum.at(Maybe<&2, Value.T>, expectations, index)), value)
        return Term.In{Shape.SColl{count}, Term.ALeg{index}, [value]}
    case Syntax.SAbsurd{value}:
      do Budget.Comp<Term.T>:
        value : Term.T <- elab(ctx, None{}, value)
        return Term.Elim{Term.ElimData{Shape.coll(Term.T, 0n), value, Quantity.QOne{}, None{}, []}}
    case Syntax.SApp{head, arg}:
      elab_app(ctx, expected, head, arg)
    case Syntax.SFun{binders, body}:
      elab_fun(binders, ctx, expected, body)
    case Syntax.SArrow{binder, body}:
      elab_group(ctx, binder, body, 0n)
    case Syntax.SStar{binder, body}:
      elab_group(ctx, binder, body, 1n)
    case Syntax.SZkTy{binder, body}:
      elab_group(ctx, binder, body, 2n)
    case Syntax.SProve{key, witness, relation}:
      elab_prove(ctx, expected, key, witness, relation)
    case Syntax.SVerify{key, proof}:
      elab_verify(ctx, key, proof)
    case Syntax.SFhcTy{level, ty}:
      do Budget.Comp<Term.T>:
        level : Term.T <- elab(ctx, None{}, level)
        ty : Term.T <- elab(ctx, None{}, ty)
        return Term.Ran{Shape.SFhc{level}, ty}
    case Syntax.SEnc{key, value}:
      elab_cipher(ctx, expected, key, value, False{})
    case Syntax.SEval{function, value}:
      elab_cipher(ctx, expected, function, value, True{})
    case Syntax.SDec{key, value}:
      elab_dec(ctx, key, value)
    case Syntax.SMpcTy{parties, access, ty}:
      do Budget.Comp<Term.T>:
        parties : Term.T <- elab(ctx, None{}, parties)
        access : Term.T <- elab(ctx, None{}, access)
        ty : Term.T <- elab(ctx, None{}, ty)
        return Term.Ran{Shape.SMpc{parties, access}, ty}
    case Syntax.SShare{value}:
      elab_share(ctx, expected, value)
    case Syntax.SInput{party, value}:
      elab_input(ctx, expected, party, value)
    case Syntax.SJoin{parties, function, values}:
      elab_join(ctx, expected, parties, function, values)
    case Syntax.SOpen{quantity, parties, proof, value}:
      elab_open(ctx, quantity, parties, proof, value)
    case Syntax.SLet{+name, ty, value, body}:
      do Budget.Comp<Term.T>:
        +ty : Term.T <- elab(ctx, None{}, ty)
        +ty_value : Value.T <- eval_in(ctx, ty)
        +value : Term.T <- elab(ctx, Some{ty_value}, value)
        value_value : Value.T <- eval_in(ctx, value)
        body : Term.T <- elab(Check.define(name, Quantity.QMany{}, ty_value, value_value, ctx), expected, body)
        return Term.Let{name, ty, value, body}
    case Syntax.SAnn{value, ty}:
      do Budget.Comp<Term.T>:
        +ty : Term.T <- elab(ctx, None{}, ty)
        ty_value : Value.T <- eval_in(ctx, ty)
        value : Term.T <- elab(ctx, Some{ty_value}, value)
        return Term.Ann{value, ty}
    case Syntax.SCase{scrut, motive, branches}:
      elab_case(ctx, expected, scrut, motive, branches, False{})
    case Syntax.SMatch{scrut, motive, branches}:
      elab_case(ctx, expected, scrut, motive, branches, True{})

type TelescopeState is Data:
  TelescopeState{telescope: Positivity.Telescope, ctx: Check.Ctx}

def telescope_terms(state: TelescopeState) -> Positivity.Telescope:
  match state:
    case TelescopeState{telescope, ctx}:
      telescope

def telescope_context(state: TelescopeState) -> Check.Ctx:
  match state:
    case TelescopeState{telescope, ctx}:
      ctx

@unsafe
def elab_telescope(binders: List<&2, Syntax.Binder<Syntax.T>>, ctx: Check.Ctx, telescope: Positivity.Telescope) -> Budget.Comp(TelescopeState):
  match binders:
    case Nil{}:
      Budget.pure(TelescopeState, TelescopeState{List.reverse(&2, Positivity.Binder, telescope), ctx})
    case Con{binder, tail}:
      do Budget.Comp<TelescopeState>:
        +bound : Bound <- elab_binder(ctx, binder)
        elab_telescope(tail, bound_context(bound), Positivity.Binder{bound_q(bound), bound_name(bound), bound_type(bound)} <> telescope)

type Chain is Data:
  Chain{telescope: Positivity.Telescope, body: Syntax.T, ctx: Check.Ctx}

def chain_telescope(chain: Chain) -> Positivity.Telescope:
  match chain:
    case Chain{telescope, body, ctx}:
      telescope

def chain_body(chain: Chain) -> Syntax.T:
  match chain:
    case Chain{telescope, body, ctx}:
      body

def chain_context(chain: Chain) -> Check.Ctx:
  match chain:
    case Chain{telescope, body, ctx}:
      ctx

@unsafe
def elab_chain(syntax: Syntax.T, ctx: Check.Ctx, telescope: Positivity.Telescope) -> Budget.Comp(Chain):
  match syntax:
    case Syntax.SArrow{binder, body}:
      do Budget.Comp<Chain>:
        +bound : Bound <- elab_binder(ctx, binder)
        elab_chain(body, bound_context(bound), Positivity.Binder{bound_q(bound), bound_name(bound), bound_type(bound)} <> telescope)
    case other:
      Budget.pure(Chain, Chain{List.reverse(&2, Positivity.Binder, telescope), other, ctx})

def header_universe(term: Term.T) -> Budget.Comp(Level.T):
  match term:
    case Term.Univ{level}:
      Budget.pure(Level.T, level)
    case other:
      Budget.failed(Level.T, Error.Universe{"the header of a family ends in a universe"})

def elab_univ(ctx: Check.Ctx, syntax: Syntax.T) -> Budget.Comp(Level.T):
  do Budget.Comp<Level.T>:
    term : Term.T <- elab(ctx, None{}, syntax)
    header_universe(term)

def elab_fam_decl(family: Syntax.Fam<Syntax.T>, ctx: Check.Ctx) -> Budget.Comp(Check.FamilyDecl):
  match family:
    case Syntax.Fam{name, params, ty, ctors}:
      do Budget.Comp<Check.FamilyDecl>:
        +params : TelescopeState <- elab_telescope(params, ctx, [])
        +indices : Chain <- elab_chain(ty, telescope_context(params), [])
        level : Level.T <- elab_univ(chain_context(indices), chain_body(indices))
        return Check.FamilyDecl{name, telescope_terms(params), chain_telescope(indices), level}

def diagram_bodies(legs: List<&2, Term.Leg<Term.T>>) -> List<&2, Term.T>:
  match legs:
    case Nil{}:
      []
    case Con{Term.Leg{binders, body}, tail}:
      body <> diagram_bodies(tail)

def mu_params_of(term: Term.T) -> Budget.Comp(List<&2, Term.T>):
  match term:
    case Term.Sec{shape, legs}:
      Budget.pure(List<&2, Term.T>, diagram_bodies(legs))
    case other:
      Budget.failed(List<&2, Term.T>, Error.Mismatch{"the diagram is not a section of the declared width"})

def matching_mu_result(same: Bool, name: String, diagram: Term.T, indices: List<&2, Term.T>) -> Budget.Comp(Common.Pair<List<&2, Term.T>, List<&2, Term.T>>):
  match same:
    case True{}:
      do Budget.Comp<Common.Pair<List<&2, Term.T>, List<&2, Term.T>>>:
        params : List<&2, Term.T> <- mu_params_of(diagram)
        return Common.Pair{params, indices}
    case False{}:
      Budget.failed(Common.Pair<List<&2, Term.T>, List<&2, Term.T>>, Error.Mismatch{"a constructor of " ++ name ++ " ends at another type"})

def mu_result(term: Term.T, +name: String) -> Budget.Comp(Common.Pair<List<&2, Term.T>, List<&2, Term.T>>):
  match term:
    case Term.Lan{Shape.SMu{family, indices}, diagram}:
      matching_mu_result(String.eq(name, family), name, diagram, indices)
    case other:
      Budget.failed(Common.Pair<List<&2, Term.T>, List<&2, Term.T>>, Error.Mismatch{"a constructor of " ++ name ++ " ends at another type"})

def elab_ctor_decl(ctor: Syntax.FamCtor<Syntax.T>, ctx: Check.Ctx, name: String) -> Budget.Comp(Check.CtorDecl):
  match ctor:
    case Syntax.FamCtor{ctor_name, ty}:
      do Budget.Comp<Check.CtorDecl>:
        +chain : Chain <- elab_chain(ty, ctx, [])
        result : Term.T <- elab(chain_context(chain), None{}, chain_body(chain))
        +result : Common.Pair<List<&2, Term.T>, List<&2, Term.T>> <- mu_result(result, name)
        return Check.CtorDecl{ctor_name, chain_telescope(chain), first(List<&2, Term.T>, List<&2, Term.T>, result), second(List<&2, Term.T>, List<&2, Term.T>, result)}

def elab_ctor_decls(ctors: List<&2, Syntax.FamCtor<Syntax.T>>, +ctx: Check.Ctx, +name: String) -> Budget.Comp(List<&2, Check.CtorDecl>):
  match ctors:
    case Nil{}:
      Budget.pure(List<&2, Check.CtorDecl>, [])
    case Con{head, tail}:
      do Budget.Comp<List<&2, Check.CtorDecl>>:
        head : Check.CtorDecl <- elab_ctor_decl(head, ctx, name)
        tail : List<&2, Check.CtorDecl> <- elab_ctor_decls(tail, ctx, name)
        return head <> tail

def elab_decl(decl: Syntax.Decl, +ctx: Check.Ctx) -> Budget.Comp(Check.Decl):
  match decl:
    case Syntax.DDef{name, ty, body}:
      do Budget.Comp<Check.Decl>:
        +ty : Term.T <- elab(ctx, None{}, ty)
        value : Value.T <- eval_in(ctx, ty)
        body : Term.T <- elab(ctx, Some{value}, body)
        return Check.Decl{name, Check.Definition{}, ty, Some{body}}
    case Syntax.DAxiom{name, ty}:
      do Budget.Comp<Check.Decl>:
        ty : Term.T <- elab(ctx, None{}, ty)
        return Check.Decl{name, Check.Postulate{}, ty, None{}}
    case Syntax.DMu{families}:
      Budget.failed(Check.Decl, Error.Mismatch{"a mu group declares no entry of its own"})
    case Syntax.DRec{definitions}:
      Budget.failed(Check.Decl, Error.Mismatch{"a recursive group is elaborated as a group"})
    case other:
      Budget.failed(Check.Decl, Error.Mismatch{"a prenex declaration requires a program catalog"})
