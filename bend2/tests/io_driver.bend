import Base
import ../io.bend as Files
import ../kernel/common.bend as Common

def unit_result(result: Result<&2, &2, String, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.print("OK")
    case Fail{message}:
      IO.die(Unit, 1, message)

def text_result(result: Result<&2, &2, String, String>) -> IO(Unit):
  match result:
    case Done{text}:
      IO.print(text)
    case Fail{message}:
      IO.die(Unit, 1, message)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{"read", Con{path, Nil{}}}:
      do IO<Unit>:
        result : Result<&2, &2, String, String> <- Files.read_file(path)
        text_result(result)
    case Con{"write", Con{path, Con{contents, Nil{}}}}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.write_file(path, contents)
        unit_result(result)
    case Con{"bytes", Con{path, Nil{}}}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.write_bytes(path, [0, 255, 128, 10])
        unit_result(result)
    case Con{"publish", Con{directory, Nil{}}}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.publish(directory, [Common.Pair{"first.json", "{\"value\":42}\n"}, Common.Pair{"second.tsv", "name\tvalue\n"}])
        unit_result(result)
    case Con{"publish-fail", Con{directory, Nil{}}}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.publish(directory, [Common.Pair{"first", "written"}, Common.Pair{"absent/second", "failure"}])
        unit_result(result)
    case Con{"temp", Nil{}}:
      do IO<Unit>:
        result : Result<&2, &2, String, String> <- Files.temp_file("mechanism-io-", ".wasm")
        text_result(result)
    case other:
      IO.die(Unit, 64, "usage: io_driver read|write|bytes|publish|publish-fail PATH [CONTENTS] | temp")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
