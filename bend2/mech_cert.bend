import Base
import ./cli/cert.bend as Cert

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    Cert.arguments(args)
