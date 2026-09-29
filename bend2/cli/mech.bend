import Base
import ./kanon.bend as Kanon
import ../io.bend as Files
import ../kernel/common.bend as Common
import ../import/export.bend as Export
import ../import/report.bend as Report
import ../import/mapping.bend as Mapping

def usage() -> IO(Unit):
  IO.die(Unit, 64, "usage: mech import FILE [--out DIR] | diff-parity --export FILE\n       mech map-inventory --export FILE [--never]\n       mech check|axioms|circuit|emit|build|run|spec-count (inherited arguments)")

def export_error(error: Export.Error, path: String) -> String:
  match error:
    case Export.ImportError{line, message}:
      "mech: " ++ path ++ ":" ++ U32.show(line) ++ ": " ++ message

def parsed(result: Result<&2, &2, Export.Error, Export.T>, path: String) -> IO(Export.T):
  match result:
    case Done{source}:
      IO.pure(Export.T, source)
    case Fail{error}:
      IO.die(Export.T, 1, export_error(error, path))

def read_result(result: Result<&2, &2, String, String>, path: String) -> IO(Export.T):
  match result:
    case Done{text}:
      parsed(Export.read_string(text), path)
    case Fail{message}:
      IO.die(Export.T, 1, "mech: " ++ path ++ ":0: " ++ message)

def read_source(+path: String) -> IO(Export.T):
  do IO<Export.T>:
    result : Result<&2, &2, String, String> <- Files.read_file(path)
    read_result(result, path)

def prepared(result: Result<&2, &2, Export.Error, Report.T>, path: String) -> IO(Report.T):
  match result:
    case Done{report}:
      IO.pure(Report.T, report)
    case Fail{error}:
      IO.die(Report.T, 1, export_error(error, path))

def imported(+path: String) -> IO(Report.T):
  do IO<Report.T>:
    source : Export.T <- read_source(path)
    prepared(Report.prepare(source), path)

def show(lines: List<&2, String>) -> IO(Unit):
  match lines:
    case Nil{}:
      IO.pure(Unit, Unit{})
    case Con{line, rest}:
      do IO<Unit>:
        unit : Unit <- IO.print(line)
        show(rest)

def output_files(files: List<&2, Report.OutputFile>) -> List<&2, Common.Pair<String, String>>:
  match files:
    case Nil{}:
      []
    case Con{Report.OutputFile{name, contents}, rest}:
      Con{Common.Pair{name, contents}, output_files(rest)}

def published(result: Result<&2, &2, String, Unit>, report: Report.T) -> IO(Unit):
  match result:
    case Done{unit}:
      show(Report.summary(report))
    case Fail{message}:
      IO.die(Unit, 1, "mech: " ++ message)

def output(directory: Maybe<&2, String>, +report: Report.T) -> IO(Unit):
  match directory:
    case None{}:
      show(Report.summary(report))
    case Some{directory}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.publish(directory, output_files(Report.files(report)))
        published(result, report)

def run_import(path: String, directory: Maybe<&2, String>) -> IO(Unit):
  do IO<Unit>:
    report : Report.T <- imported(path)
    output(directory, report)

def mapping_text(never: Bool, inventory: Mapping.T) -> String:
  match never:
    case True{}:
      Mapping.never_tsv(inventory)
    case False{}:
      Mapping.map_tsv(inventory)

def mapped(result: Result<&2, &2, String, Mapping.T>, never: Bool) -> IO(Unit):
  match result:
    case Done{inventory}:
      IO.write(mapping_text(never, inventory))
    case Fail{message}:
      IO.die(Unit, 1, "mech: " ++ message)

def map_inventory(path: String, never: Bool) -> IO(Unit):
  do IO<Unit>:
    +source : Export.T <- read_source(path)
    mapped(Mapping.inventory(source, Mapping.foundation_targets(source)), never)

def dispatch(command: String, args: List<String>) -> IO(Unit):
  match command:
    case "import":
      match args:
        case Con{path, Nil{}}:
          run_import(path, None{})
        case Con{path, Con{"--out", Con{directory, Nil{}}}}:
          run_import(path, Some{directory})
        case other:
          usage()
    case "diff-parity":
      match args:
        case Con{"--export", Con{path, Nil{}}}:
          run_import(path, None{})
        case other:
          usage()
    case "map-inventory":
      match args:
        case Con{"--export", Con{path, Nil{}}}:
          map_inventory(path, False{})
        case Con{"--export", Con{path, Con{"--never", Nil{}}}}:
          map_inventory(path, True{})
        case other:
          usage()
    case command:
      Kanon.dispatch(command, args)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Nil{}:
      usage()
    case Con{command, rest}:
      dispatch(command, rest)
