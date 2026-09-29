import Base
import ./cli/mech.bend as Mech

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    Mech.arguments(args)
