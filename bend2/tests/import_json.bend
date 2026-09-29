import Base
import ../import/ndjson.bend as Json

def result(value: Result<&2, &2, String, Json.T>) -> IO(Unit):
  match value:
    case Done{json}:
      IO.print(Json.to_string(json))
    case Fail{message}:
      IO.die(Unit, 1, message)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{source, Nil{}}:
      result(Json.parse(source))
    case other:
      IO.die(Unit, 64, "usage: import_json JSON")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
