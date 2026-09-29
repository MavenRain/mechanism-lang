import ./kernel_large_addresses.bend as Wide
import ./kernel_projection_motive.bend as Projection
import ./kernel_conversion_budget.bend as Conversion
import ./levels_protocol.bend as Levels

def arguments(args: List<String>) -> IO(Unit):
  do IO<Unit>:
    ok : Unit <- Wide.arguments([])
    ok : Unit <- Projection.main()
    ok : Unit <- Conversion.main()
    Levels.arguments([])

def main() -> IO(Unit):
  arguments([])
