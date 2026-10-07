import Base
import ./common.bend as Common
import ./term.bend as Term
import ./shape.bend as Shape
import ./quantity.bend as Quantity
import ./level.bend as Level
import ./error.bend as Error
import ./level_var.bend as LevelVar

type Binder is Data:
  Binder{quantity: Quantity.T, name: String, ty: Term.T}

def Telescope() -> Data:
  List<&2, Binder>

type CtorStatus is Data:
  Provisional{}
  Builtin{}
  Complete{names: List<&2, String>}

type Ctor is Data:
  Ctor{name: String, args: Telescope, res_idx: List<&2, Term.T>, full_arity: Nat, self_rec: Bool}

type Family is Data:
  Family{name: String, params: Telescope, indices: Telescope, level: Level.T, status: CtorStatus, ctors: List<&2, Ctor>, positive: Bool}

def find_ctor(ctors: List<&2, Ctor>, +name: String) -> Maybe<&2, Ctor>:
  match ctors:
    case Nil{}:
      None{}
    case Con{Ctor{+n, args, idx, arity, rec}, rest}:
      selected = Bool.pick(Unit -> Maybe<&2, Ctor>, String.eq(n, name), unit => Some{Ctor{n, args, idx, arity, rec}}, unit => find_ctor(rest, name))
      selected(Unit{})

def ctor_of(name: String, family: Family) -> Maybe<&2, Ctor>:
  match family:
    case Family{n, params, indices, level, status, ctors, positive}:
      find_ctor(ctors, name)

def occurs(names: List<&2, String>, term: Term.T) -> Bool:
  Term.exists_name(True{}, names, term)

def absent(names: List<&2, String>, term: Term.T) -> Bool:
  Bool.not(occurs(names, term))

def absent_list(terms: List<&2, Term.T>, +names: List<&2, String>) -> Bool:
  match terms:
    case Nil{}:
      True{}
    case Con{term, rest}:
      Bool.and(absent(names, term), absent_list(rest, names))

def point_dom(s: Shape.T<Term.T>) -> Maybe<&2, Term.T>:
  match s:
    case Shape.SPi{q, name, dom}:
      Some{dom}
    case Shape.SZk{q, name, dom}:
      Some{dom}
    case Shape.SArg{q, name}:
      None{}
    case Shape.SColl{count}:
      None{}
    case Shape.SPar{left, right}:
      None{}
    case Shape.SMu{name, args}:
      None{}
    case Shape.SNu{name, args}:
      None{}
    case Shape.SFhc{level}:
      None{}
    case Shape.SMpc{parties, access}:
      None{}

law positive:
  for term: Term.T
  for +names: List<&2, String>
  Bool

@unsafe def positive_ran(dom: Maybe<&2, Term.T>, cod: Term.T, whole: Term.T, +names: List<&2, String>) -> Bool:
  match dom:
    case None{}:
      absent(names, whole)
    case Some{ty}:
      Bool.and(absent(names, ty), positive(cod, names))

@unsafe def positive_out(dom: Maybe<&2, Term.T>, addr: Term.Addr<Term.T>, head: Term.T, whole: Term.T, +names: List<&2, String>) -> Bool:
  match dom addr:
    case None{} _:
      absent(names, whole)
    case Some{ty} Term.APt{q, arg}:
      Bool.and(absent(names, arg), positive(head, names))
    case Some{ty} Term.ALeg{index}:
      absent(names, whole)
    case Some{ty} Term.ACtor{name}:
      absent(names, whole)

@unsafe def positive(term, names):
  match term:
    case Term.Lan{+shape, +body}:
      Bool.pick(Bool, Term.family_member(Shape.family(Term.T, shape), names), Bool.and(absent_list(Shape.payload(Term.T, shape), names), absent(names, body)), absent(names, Term.Lan{shape, body}))
    case Term.Ran{+shape, +cod}:
      positive_ran(point_dom(shape), cod, Term.Ran{shape, cod}, names)
    case Term.Out{+shape, +addr, +head}:
      positive_out(point_dom(shape), addr, head, Term.Out{shape, addr, head}, names)
    case Term.Global{name}:
      True{}
    case Term.Var{index}:
      True{}
    case Term.Univ{level}:
      True{}
    case Term.Lit{value}:
      True{}
    case Term.Auto{}:
      True{}
    case Term.In{shape, addr, args}:
      absent(names, Term.In{shape, addr, args})
    case Term.Elim{data}:
      absent(names, Term.Elim{data})
    case Term.Sec{shape, legs}:
      absent(names, Term.Sec{shape, legs})
    case Term.Let{name, ty, value, body}:
      absent(names, Term.Let{name, ty, value, body})
    case Term.Ann{body, ty}:
      absent(names, Term.Ann{body, ty})

def fields_positive(args: Telescope, +names: List<&2, String>) -> Bool:
  match args:
    case Nil{}:
      True{}
    case Con{Binder{q, x, ty}, rest}:
      Bool.and(positive(ty, names), fields_positive(rest, names))

def ctor_fields(names: List<&2, String>, args: Telescope) -> Result<&2, &2, Error.T, Unit>:
  Bool.pick(Result<&2, &2, Error.T, Unit>, fields_positive(args, names), Done{Unit{}}, Fail{Error.Not_yet{"a family that is not strictly positive arrives at M2"}})

def self_rec(args: Telescope, +names: List<&2, String>) -> Bool:
  match args:
    case Nil{}:
      False{}
    case Con{Binder{q, x, ty}, rest}:
      Bool.or(occurs(names, ty), self_rec(rest, names))

def same_telescope(a: Telescope, b: Telescope) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Binder{q, x, t}, xs} Con{Binder{r, y, u}, ys}:
      Bool.and(Quantity.equal(q, r), Bool.and(String.eq(x, y), Bool.and(Term.equal(t, u), same_telescope(xs, ys))))
    case _ _:
      False{}

def same_status(a: CtorStatus, b: CtorStatus) -> Bool:
  match a b:
    case Provisional{} Provisional{}:
      True{}
    case Builtin{} Builtin{}:
      True{}
    case Complete{xs} Complete{ys}:
      Term.equal_strings(xs, ys)
    case _ _:
      False{}

def same_bool(a: Bool, +b: Bool) -> Bool:
  Bool.pick(Bool, a, b, Bool.not(b))

def same_ctors(a: List<&2, Ctor>, b: List<&2, Ctor>, +ignore_self_rec: Bool) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Ctor{x, args, idx, arity, rec}, xs} Con{Ctor{y, args2, idx2, arity2, rec2}, ys}:
      Bool.and(String.eq(x, y), Bool.and(same_telescope(args, args2), Bool.and(Term.equal_list(idx, idx2), Bool.and(Nat.is_eq(arity, arity2), Bool.and(Bool.or(ignore_self_rec, same_bool(rec, rec2)), same_ctors(xs, ys, ignore_self_rec))))))
    case _ _:
      False{}

def same_family_except(a: Family, b: Family, ignore_level: Bool, ignore_self_rec: Bool) -> Bool:
  match a b:
    case Family{n, p, i, l, s, cs, pos} Family{n2, p2, i2, l2, s2, cs2, pos2}:
      Bool.and(String.eq(n, n2), Bool.and(same_telescope(p, p2), Bool.and(same_telescope(i, i2), Bool.and(Bool.or(ignore_level, LevelVar.structural_equal(l, l2)), Bool.and(same_status(s, s2), Bool.and(same_ctors(cs, cs2, ignore_self_rec), same_bool(pos, pos2)))))))

def same_family(a: Family, b: Family) -> Bool:
  same_family_except(a, b, False{}, False{})
