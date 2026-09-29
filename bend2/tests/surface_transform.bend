import Base
import ../surface/poly.bend as Poly
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Quantity
import ../kernel/level.bend as Level
import ../kernel/common.bend as Common
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error

def source() -> Term.T:
  Term.Elim{Term.ElimData{
    Shape.SMu{"Box", [Term.Univ{Level.var(0n)}]},
    Term.Global{"value"}, Quantity.QOne{},
    Some{Term.Motive{Some{"Box"}, ["index"], "self", Term.Univ{Level.var(0n)}}},
    [Common.Pair{Term.ACtor{"box"}, Term.Leg{[Common.Pair{Quantity.QZero{}, "Box"}], Term.Ann{Term.Global{"Box"}, Term.Univ{Level.var(0n)}}}}]}}

def transformed(result: Result<&2, &2, Error.T, Term.T>) -> Bool:
  match result:
    case Done{Term.Elim{Term.ElimData{
        Shape.SMu{family, Con{Term.Univ{shape_level}, Nil{}}},
        Term.Global{scrut}, quantity,
        Some{Term.Motive{Some{motive_family}, indices, self, Term.Univ{motive_level}}},
        Con{Common.Pair{Term.ACtor{ctor}, Term.Leg{Con{Common.Pair{q, binder}, Nil{}}, Term.Ann{Term.Global{global}, Term.Univ{annotation_level}}}}, Nil{}}}}}:
      String.eq(family, "SmallBox") && String.eq(motive_family, "SmallBox") &&
      String.eq(global, "SmallBox") && String.eq(binder, "Box") && String.eq(ctor, "box") &&
      Level.equal(shape_level, Level.one()) && Level.equal(motive_level, Level.one()) &&
      Level.equal(annotation_level, Level.one())
    case other:
      False{}

def exhausted(result: Result<&2, &2, Error.T, Term.T>) -> Bool:
  match result:
    case Fail{Error.Budget_exhausted{message}}:
      True{}
    case other:
      False{}

def out_of_scope(result: Result<&2, &2, Error.T, Term.T>) -> Bool:
  match result:
    case Fail{Error.Universe{message}}:
      True{}
    case other:
      False{}

def main() -> Bool:
  +action = {Poly.Action{Poly.SubstituteLevels{[Level.one()]}, [Common.Pair{"Box", "SmallBox"}], []} : Poly.Action}
  transformed(Budget.run(Term.T, Poly.map_term(action, source()), Budget.of_polls(100n))) &&
  exhausted(Budget.run(Term.T, Poly.map_term(action, source()), Budget.of_polls(5n))) &&
  out_of_scope(Poly.map_levels(Poly.ScopeLevels{0n}, Term.Ann{Term.Auto{}, Term.Univ{Level.var(0n)}})) &&
  out_of_scope(Poly.map_levels(Poly.SubstituteLevels{[]}, Term.Univ{Level.var(0n)}))
