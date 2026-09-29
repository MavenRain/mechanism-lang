import Base
import ./cli/mech.bend as Mech
import ./cli/cert.bend as Cert
import ./cli/kanon.bend as Kanon

def kanon(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}:
      Kanon.usage()
    case Con{command, rest}:
      Kanon.dispatch(command, rest)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{"mech", rest}:
      Mech.arguments(rest)
    case Con{"cert", rest}:
      Cert.arguments(rest)
    case Con{"kanon", rest}:
      kanon(rest)
    case other:
      IO.die(Unit, 64, "usage: mechanism-driver mech|cert|kanon ARGUMENTS...")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
