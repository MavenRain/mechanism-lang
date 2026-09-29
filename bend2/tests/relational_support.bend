import Base
import ./relational_fs.bend as FS
import ./relational_digest.bend as MD5

def require(ok: Bool, message: String) -> IO(Unit):
  match ok:
    case True{}: IO.pure(Unit, Unit{})
    case False{}: IO.die(Unit, 1, message)

def missing(result: Result<&1, &1, U32 & String, List<&2, String>>) -> Bool:
  match result:
    case Fail{error}: True{}
    case Done{names}: False{}

def has(names: List<&2, String>, +name: String) -> Bool:
  match names:
    case Nil{}: False{}
    case Con{head, rest}: Bool.or(String.eq(head, name), has(rest, name))

def present(result: Result<&1, &1, U32 & String, List<&2, String>>) -> Bool:
  match result:
    case Fail{error}: False{}
    case Done{+names}: Bool.and(has(names, "plain"), has(names, "line\nbreak"))

def digest(text: String, limit: U32, +expected: String) -> IO(Unit):
  do IO<Unit>:
    actual : String <- MD5.relational_digest(text, limit)
    require(String.eq(actual, expected), "digest " ++ expected)

def run(+path: String) -> IO(Unit):
  do IO<Unit>:
    ok : Unit <- digest("", 0, "d41d8cd98f00b204e9800998ecf8427e")
    ok : Unit <- digest("abc", 0, "900150983cd24fb0d6963f7d28e17f72")
    ok : Unit <- digest("abcdefghijklmnopqrstuvwxyz", 0, "c3fcd3d76192e4007dfb496cca67e13b")
    ok : Unit <- digest("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0, "d174ab98d277d9f5a5611c2c9f419d9f")
    ok : Unit <- digest("12345678901234567890123456789012345678901234567890123456789012345678901234567890", 0, "57edf4a22be3c955ac49da2e2107b67a")
    ok : Unit <- digest("abcdef", 3, "900150983cd24fb0d6963f7d28e17f72")
    bad : Result<&1, &1, U32 & String, List<&2, String>> <- FS.relational_list_dir(path ++ "/absent")
    ok : Unit <- require(missing(bad), "missing directory accepted")
    good : Result<&1, &1, U32 & String, List<&2, String>> <- FS.relational_list_dir(path)
    ok : Unit <- require(present(good), "filenames or reused IO status changed")
    IO.print("RELATIONAL-SUPPORT-OK assertions=8")

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}: IO.die(Unit, 1, "expected fixture directory")
    case Con{path, rest}: run(path)

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
