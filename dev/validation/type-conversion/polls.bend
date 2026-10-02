import Base
import ../../../bend2/kernel/check_engine.bend as Engine
import ../../../bend2/kernel/context.bend as Context
import ../../../bend2/kernel/global.bend as Global
import ../../../bend2/kernel/budget.bend as Budget
import ../../../bend2/kernel/value.bend as Value
import ../../../bend2/kernel/level.bend as Level
import ../../../bend2/kernel/common.bend as Common
import ../../../bend2/kernel/bignum.bend as Big
import ../../../bend2/kernel/error.bend as Error

def measured_state(label: String, state: Common.Pair<Budget.T, Result<&2, &2, Error.T, Bool>>) -> IO(Unit):
  match state:
    case Common.Pair{Budget.Polls{remaining}, Done{True{}}}:
      IO.print(label ++ " remaining=" ++ Big.to_string(Big.of_nat(remaining)))
    case _:
      IO.die(Unit, 1, "conversion measurement failed")

def measured(label: String, action: Budget.Comp(Bool)) -> IO(Unit):
  measured_state(label, Budget.run_state(Bool, action, Budget.of_polls(100n)))

def main() -> IO(Unit):
  +ctx = Context.make(Global.initial(), Budget.unlimited(), 0n)
  +a = {Value.VUniv{Level.max(Level.one(), Level.zero())}: Value.T}
  +b = {Value.VUniv{Level.one()}: Value.T}
  do IO<Unit>:
    unit : Unit <- measured("previous", Engine.conv(ctx, Engine.conv_placeholder(), a, b))
    measured("structural", Engine.conv_type(ctx, a, b))
