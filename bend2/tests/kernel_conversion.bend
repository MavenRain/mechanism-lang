import Base
import ../kernel/check.bend as Check
import ../kernel/conv.bend as Conv
import ../kernel/context.bend as Context
import ../kernel/common.bend as Pair
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/term.bend as Term
import ../kernel/value.bend as Value
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/level.bend as Level
import ../kernel/prim.bend as Prim
import ../kernel/bignum.bend as Big
import ../kernel/literal.bend as Lit
import ../kernel/eval.bend as Eval
import ../kernel/error.bend as Error

def result_true(result: Result<&2, &2, Error.T, Bool>) -> Bool:
  match result:
    case Done{ok}:
      ok
    case Fail{error}:
      False{}

def result_false(result: Result<&2, &2, Error.T, Bool>) -> Bool:
  match result:
    case Done{ok}:
      Bool.not(ok)
    case Fail{error}:
      False{}

def eta_value(+ctx: Check.Ctx, ty: Value.T, value: Result<&2, &2, Error.T, Value.T>) -> Bool:
  match value:
    case Done{eta}:
      result_true(Conv.conv(ctx, ty, Value.var(0n), eta))
    case Fail{error}:
      False{}

def eta(+ctx: Check.Ctx, ty: Result<&2, &2, Error.T, Value.T>) -> Bool:
  match ty:
    case Fail{error}:
      False{}
    case Done{+tyv}:
      eta_value(Context.bind("f", Q.QMany{}, tyv, ctx), tyv, Eval.eval(Check.globals_of(ctx), [Value.var(0n)], Term.Sec{Shape.SPi{Q.QOne{}, "x", Prim.nat_ty()}, [Term.Leg{[Pair.Pair{Q.QOne{}, "x"}], Term.Out{Shape.SPi{Q.QOne{}, "x", Prim.nat_ty()}, Term.APt{Q.QOne{}, Term.Var{0n}}, Term.Var{1n}}}]}))

def literals(+ctx: Check.Ctx, nat: Result<&2, &2, Error.T, Value.T>) -> Bool:
  match nat:
    case Done{ty}:
      result_false(Conv.conv(ctx, ty, Value.VLit{Lit.LInt{Big.zero()}}, Value.VLit{Lit.LInt{Big.one()}}))
    case Fail{error}:
      False{}

def type_pair(+ctx: Check.Ctx, a: Result<&2, &2, Error.T, Value.T>, b: Result<&2, &2, Error.T, Value.T>) -> Bool:
  match a b:
    case Done{x} Done{y}: result_true(Conv.conv_type(ctx, x, y))
    case _ _: False{}

def type_conversions() -> Bool:
  +g = Global.add("P", Global.Axiom{Term.Univ{Level.zero()}}, Global.add("Q", Global.Axiom{Term.Univ{Level.zero()}}, Global.initial()))
  +ctx = Check.make(g, Budget.unlimited(), 0n)
  +p = {Value.VNeutral{Value.HGlobal{"P"}, []}: Value.T}
  +q = {Value.VNeutral{Value.HGlobal{"Q"}, []}: Value.T}
  +proofs = Context.bind("second", Q.QZero{}, p, Context.bind("first", Q.QZero{}, p, ctx))
  Bool.and(result_false(Conv.conv_type(ctx, p, q)),
    Bool.and(result_true(Conv.conv_type(ctx, p, p)),
      Bool.and(result_false(Conv.conv_type(ctx, Value.VUniv{Level.zero()}, Value.VUniv{Level.one()})),
        Bool.and(result_true(Conv.conv(proofs, p, Value.var(0n), Value.var(1n))),
          type_pair(ctx,
            Eval.eval(g, [], Prim.arrow(Q.QOne{}, "left", Prim.nat_ty(), Prim.nat_ty())),
            Eval.eval(g, [], Prim.arrow(Q.QOne{}, "right", Prim.nat_ty(), Prim.nat_ty())))))))

def universe_values() -> Bool:
  +g = Global.add("P", Global.Axiom{Term.Univ{Level.zero()}}, Global.add("Q", Global.Axiom{Term.Univ{Level.zero()}}, Global.add("R", Global.Axiom{Term.Univ{Level.one()}}, Global.add("S", Global.Axiom{Term.Univ{Level.one()}}, Global.initial()))))
  +ctx = Check.make(g, Budget.unlimited(), 0n)
  +p = {Value.VNeutral{Value.HGlobal{"P"}, []}: Value.T}
  +q = {Value.VNeutral{Value.HGlobal{"Q"}, []}: Value.T}
  +r = {Value.VNeutral{Value.HGlobal{"R"}, []}: Value.T}
  +s = {Value.VNeutral{Value.HGlobal{"S"}, []}: Value.T}
  Bool.and(result_true(Conv.conv(ctx, Value.VUniv{Level.zero()}, p, p)),
    Bool.and(result_false(Conv.conv(ctx, Value.VUniv{Level.zero()}, p, q)),
      Bool.and(result_true(Conv.conv(ctx, Value.VUniv{Level.one()}, r, r)),
        result_false(Conv.conv(ctx, Value.VUniv{Level.one()}, r, s)))))

def verify(ok: Bool) -> IO(Unit):
  match ok:
    case True{}:
      IO.print("PASS conversion function eta, universe equality, unequal literals, type distinction, proof irrelevance, universe inhabitants")
    case False{}:
      IO.die(Unit, 1, "FAIL conversion")

def main() -> IO(Unit):
  +g = Global.initial()
  +ctx = Check.make(g, Budget.unlimited(), 0n)
  verify(Bool.and(universe_values(), Bool.and(type_conversions(), Bool.and(eta(ctx, Eval.eval(g, [], Prim.arrow(Q.QOne{}, "x", Prim.nat_ty(), Prim.nat_ty()))), Bool.and(literals(ctx, Eval.eval(g, [], Prim.nat_ty())), result_true(Conv.conv_type(ctx, Value.VUniv{Level.max(Level.one(), Level.zero())}, Value.VUniv{Level.one()})))))))
