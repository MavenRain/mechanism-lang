import Base
import ./context.bend as Context
import ./check_engine.bend as Engine
import ./value.bend as Value
import ./budget.bend as Budget
import ./error.bend as Error

def budget_of(ctx: Context.Ctx) -> Budget.T:
  match ctx:
    case Context.Ctx{g, env, locals, size, budget, arity, checked}:
      budget

def conv(+ctx: Context.Ctx, ty: Value.T, a: Value.T, b: Value.T) -> Result<&2, &2, Error.T, Bool>:
  Budget.run(Bool, Engine.conv(ctx, ty, a, b), budget_of(ctx))

def conv_type(+ctx: Context.Ctx, a: Value.T, b: Value.T) -> Result<&2, &2, Error.T, Bool>:
  Budget.run(Bool, Engine.conv_type(ctx, a, b), budget_of(ctx))
