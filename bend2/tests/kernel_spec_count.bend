import Base
import ../kernel/spec_count.bend as Spec

def main() -> IO(Unit):
  IO.print(Spec.print())
