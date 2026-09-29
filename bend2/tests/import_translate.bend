import Base
import ../import/export.bend as Export
import ../import/translate.bend as Translate
import ../import/render.bend as Render
import ../import/ndjson.bend as Json

def translated(value: Result<&2, &2, Export.Error, Translate.T>) -> IO(Unit):
  match value:
    case Done{table}:
      IO.print(Json.to_string(Render.scoped_summary(table)))
    case Fail{Export.ImportError{line, message}}:
      IO.die(Unit, 1, U32.show(line) ++ ":" ++ message)

def parsed(value: Result<&2, &2, Export.Error, Export.T>) -> IO(Unit):
  match value:
    case Done{source}:
      translated(Translate.translate(source))
    case Fail{Export.ImportError{line, message}}:
      IO.die(Unit, 1, U32.show(line) ++ ":" ++ message)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{source, Nil{}}:
      parsed(Export.read_string(source))
    case other:
      IO.die(Unit, 64, "usage: import_translate NDJSON")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
