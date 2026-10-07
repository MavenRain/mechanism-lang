import Base
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Quantity
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Bignum
import ../kernel/level_var.bend as LevelVar
import ../kernel/common.bend as Common
import ../kernel/global.bend as Global
import ../surface/elab.bend as Elab

# RT-MECH (unit D): two checked programs are equal up to bound names.
# A kernel term has de Bruijn indices, and each binder keeps its source name
# for the printer only. So `alpha` is `Term.equal` (kernel/term.bend) with
# these names ignored: the name of `SPi` and `SZk`, the names of a leg, the
# `self` name and the index names of a motive, and the name of `Let`. Each
# other field is compared: quantities, family names, constructor names,
# global names, literals, levels, and an ascription (`Ann`) with its type.
law alpha:
  for a: Term.T
  for b: Term.T
  Bool

@unsafe def alpha_list(a: List<&2, Term.T>, b: List<&2, Term.T>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      Bool.and(alpha(x, y), alpha_list(xs, ys))
    case _ _:
      False{}

# The index names of a motive are binders: the count is compared.
def same_count(a: List<&2, String>, b: List<&2, String>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      same_count(xs, ys)
    case _ _:
      False{}

def alpha_binders(a: List<&2, Common.Pair<Quantity.T, String>>, b: List<&2, Common.Pair<Quantity.T, String>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Common.Pair{q, x}, xs} Con{Common.Pair{r, y}, ys}:
      Bool.and(Quantity.equal(q, r), alpha_binders(xs, ys))
    case _ _:
      False{}

@unsafe def alpha_shape(a: Shape.T<Term.T>, b: Shape.T<Term.T>) -> Bool:
  match a b:
    case Shape.SPi{q, x, p} Shape.SPi{r, y, t}:
      Bool.and(Quantity.equal(q, r), alpha(p, t))
    case Shape.SArg{q, x} Shape.SArg{r, y}:
      Quantity.equal(q, r)
    case Shape.SZk{q, x, p} Shape.SZk{r, y, t}:
      Bool.and(Quantity.equal(q, r), alpha(p, t))
    case Shape.SColl{x} Shape.SColl{y}:
      Bignum.equal(x, y)
    case Shape.SPar{x, p} Shape.SPar{y, q}:
      Bool.and(alpha(x, y), alpha(p, q))
    case Shape.SMu{x, p} Shape.SMu{y, q}:
      Bool.and(String.eq(x, y), alpha_list(p, q))
    case Shape.SNu{x, p} Shape.SNu{y, q}:
      Bool.and(String.eq(x, y), alpha_list(p, q))
    case Shape.SFhc{x} Shape.SFhc{y}:
      alpha(x, y)
    case Shape.SMpc{x, p} Shape.SMpc{y, q}:
      Bool.and(alpha(x, y), alpha(p, q))
    case _ _:
      False{}

@unsafe def alpha_addr(a: Term.Addr<Term.T>, b: Term.Addr<Term.T>) -> Bool:
  match a b:
    case Term.APt{q, x} Term.APt{r, y}:
      Bool.and(Quantity.equal(q, r), alpha(x, y))
    case Term.ALeg{x} Term.ALeg{y}:
      Bignum.equal(x, y)
    case Term.ACtor{x} Term.ACtor{y}:
      String.eq(x, y)
    case _ _:
      False{}

@unsafe def alpha_leg(a: Term.Leg<Term.T>, b: Term.Leg<Term.T>) -> Bool:
  match a b:
    case Term.Leg{x, p} Term.Leg{y, q}:
      Bool.and(alpha_binders(x, y), alpha(p, q))

@unsafe def alpha_legs(a: List<&2, Term.Leg<Term.T>>, b: List<&2, Term.Leg<Term.T>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{x, xs} Con{y, ys}:
      Bool.and(alpha_leg(x, y), alpha_legs(xs, ys))
    case _ _:
      False{}

@unsafe def alpha_motive(a: Maybe<&2, Term.Motive<Term.T>>, b: Maybe<&2, Term.Motive<Term.T>>) -> Bool:
  match a b:
    case None{} None{}:
      True{}
    case Some{Term.Motive{i, xs, s, p}} Some{Term.Motive{j, ys, t, q}}:
      Bool.and(Term.equal_option_string(i, j), Bool.and(same_count(xs, ys), alpha(p, q)))
    case _ _:
      False{}

@unsafe def alpha_branches(a: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, b: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case Con{Common.Pair{x, p}, xs} Con{Common.Pair{y, q}, ys}:
      Bool.and(alpha_addr(x, y), Bool.and(alpha_leg(p, q), alpha_branches(xs, ys)))
    case _ _:
      False{}

@unsafe def alpha_elim(a: Term.ElimData<Term.T>, b: Term.ElimData<Term.T>) -> Bool:
  match a b:
    case Term.ElimData{s, t, q, m, bs} Term.ElimData{u, v, r, n, cs}:
      Bool.and(alpha_shape(s, u), Bool.and(alpha(t, v), Bool.and(Quantity.equal(q, r), Bool.and(alpha_motive(m, n), alpha_branches(bs, cs)))))

@unsafe def alpha(a, b):
  match a b:
    case Term.Var{x} Term.Var{y}:
      Nat.is_eq(x, y)
    case Term.Univ{x} Term.Univ{y}:
      LevelVar.structural_equal(x, y)
    case Term.Lan{s, x} Term.Lan{t, y}:
      Bool.and(alpha_shape(s, t), alpha(x, y))
    case Term.Ran{s, x} Term.Ran{t, y}:
      Bool.and(alpha_shape(s, t), alpha(x, y))
    case Term.In{s, a, x} Term.In{t, b, y}:
      Bool.and(alpha_shape(s, t), Bool.and(alpha_addr(a, b), alpha_list(x, y)))
    case Term.Elim{x} Term.Elim{y}:
      alpha_elim(x, y)
    case Term.Sec{s, x} Term.Sec{t, y}:
      Bool.and(alpha_shape(s, t), alpha_legs(x, y))
    case Term.Out{s, a, x} Term.Out{t, b, y}:
      Bool.and(alpha_shape(s, t), Bool.and(alpha_addr(a, b), alpha(x, y)))
    case Term.Let{x, a, b, c} Term.Let{y, d, e, f}:
      Bool.and(alpha(a, d), Bool.and(alpha(b, e), alpha(c, f)))
    case Term.Ann{x, a} Term.Ann{y, b}:
      Bool.and(alpha(x, y), alpha(a, b))
    case Term.Global{x} Term.Global{y}:
      String.eq(x, y)
    case Term.Lit{x} Term.Lit{y}:
      Literal.equal(x, y)
    case Term.Auto{} Term.Auto{}:
      True{}
    case _ _:
      False{}

# Two rows agree if they have the same class and their terms are equal up
# to bound names. The fields are the fields that `Elab.checked_form` prints:
# the type of each class, and the body of a `def`.
@unsafe def same_entry(a: Global.Entry, b: Global.Entry) -> Bool:
  match a b:
    case Global.Def{Global.DefEntry{t, x, r, k, p}} Global.Def{Global.DefEntry{u, y, s, l, q}}:
      Bool.and(alpha(t, u), alpha(x, y))
    case Global.Axiom{t} Global.Axiom{u}:
      alpha(t, u)
    case Global.Prim{t, p} Global.Prim{u, q}:
      alpha(t, u)
    case _ _:
      False{}

# The name of the first row that differs, in the order of the programs: the
# rows are compared in pairs, so the row order and the row names are part
# of the law. `None` if all rows agree. `Bool.pick` is eager, so one call
# compares each pair of rows.
@unsafe def first_diff(a: Elab.Rows, b: Elab.Rows) -> Maybe<&2, String>:
  match a b:
    case Nil{} Nil{}:
      None{}
    case Con{Common.Pair{+x, p}, xs} Con{Common.Pair{y, q}, ys}:
      Bool.pick(Maybe<&2, String>, Bool.and(String.eq(x, y), same_entry(p, q)), first_diff(xs, ys), Some{x})
    case Con{Common.Pair{x, p}, xs} Nil{}:
      Some{x}
    case Nil{} Con{Common.Pair{y, q}, ys}:
      Some{y}
