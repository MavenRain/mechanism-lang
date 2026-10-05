import Base
import ../io.bend as Files
import ../kernel/common.bend as C
import ../kernel/global.bend as Global
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as E
import ../surface/elab.bend as Elab
import ../rust/parser.bend as Parser
import ../rust/resolve.bend as Rs
import ../rust/infer.bend as In
import ../rust/lower.bend as Lo
import ./rust_out.bend as Ro

# `mech rust-in CRATE_DIR OUT_DIR`: read src/lib.rs and the file of each
# `pub mod` line, import the crate (resolve, infer, lower, kernel check) and
# write one `.mech` file for each module, `MANIFEST` (the file names in
# dependency order) and a copy of `mech-carrier.mech` if the crate has one.
# The exit codes are the codes of rust-out: 64 for a usage error or a file
# that cannot be read, 65 for a refused crate, 1 for an output directory
# that exists or a write failure. A refused crate writes no file.

# The lowered files, or the refusal of resolve or infer.
def lower_if(+o: Rs.Out, r: Result<&2, &2, Rs.Bad, List<&2, In.Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>:
  match r:
    case Done{ts}:
      Done{Lo.lower(o)}
    case Fail{b}:
      Fail{b}

def lows_of(r: Result<&2, &2, Rs.Bad, Rs.Out>) -> Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>:
  match r:
    case Done{+o}:
      lower_if(o, In.infer(o))
    case Fail{b}:
      Fail{b}

def manifest(ls: List<&2, Lo.Low>) -> String:
  match ls:
    case Nil{}:
      ""
    case Con{Lo.Low{file, text}, rest}:
      file ++ "\n" ++ manifest(rest)

def low_files(ls: List<&2, Lo.Low>) -> List<&2, C.Pair<String, String>>:
  match ls:
    case Nil{}:
      []
    case Con{Lo.Low{file, text}, rest}:
      C.Pair{file, text} <> low_files(rest)

def kernel_ok(r: Result<&2, &2, E.T, Elab.Rows>, +ls: List<&2, Lo.Low>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{rows}:
      Done{List.append(&2, C.Pair<String, String>, low_files(ls), [C.Pair{"MANIFEST", manifest(ls)}])}
    case Fail{e}:
      Fail{"MECH-CHECK-FAIL " ++ E.message(e) ++ "\n"}

def low_ok(r: Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{+ls}:
      kernel_ok(Elab.check_text(Global.initial(), Budget.unlimited(), Lo.source(ls)), ls)
    case Fail{b}:
      Fail{"REFUSED " ++ Rs.show(b) ++ "\n"}

# The pure part: the text of src/lib.rs and of each module file give the
# `.mech` files and MANIFEST, or the refusal text.
def crate_files(lib: String, +srcs: List<&2, Rs.Src>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  low_ok(lows_of(Rs.resolve(lib, srcs)))

# The command.

def usage() -> IO(Unit):
  IO.die(Unit, 64, "usage: mech rust-in CRATE_DIR OUT_DIR")

def read_srcs(names: List<&2, String>, +dir: String) -> IO(List<&2, Rs.Src>):
  match names:
    case Nil{}:
      IO.pure(List<&2, Rs.Src>, [])
    case Con{+name, rest}:
      do IO<List<&2, Rs.Src>>:
        text : String <- Ro.read_file(dir ++ "/src/" ++ name ++ ".rs")
        more : List<&2, Rs.Src> <- read_srcs(rest, dir)
        return Rs.Src{name, text} <> more

def mods_read(r: Result<&2, &2, Rs.Bad, List<&2, String>>, +dir: String) -> IO(List<&2, Rs.Src>):
  match r:
    case Done{names}:
      read_srcs(names, dir)
    case Fail{b}:
      IO.die(List<&2, Rs.Src>, 65, "REFUSED " ++ Rs.show(b) ++ "\n")

def carrier_file(+path: String) -> IO(List<&2, C.Pair<String, String>>):
  do IO<List<&2, C.Pair<String, String>>>:
    text : String <- Ro.read_file(path)
    return [C.Pair{"mech-carrier.mech", text}]

# The carrier is copied with no change (no merge in unit C).
def carrier_of(present: Bool, +path: String) -> IO(List<&2, C.Pair<String, String>>):
  Bool.pick(IO(List<&2, C.Pair<String, String>>), present, carrier_file(path), IO.pure(List<&2, C.Pair<String, String>>, []))

# A module's CD4 file name may be `mech-carrier.mech`. Refuse a collision
# before publication so the carrier cannot overwrite a checked module.
def carrier_name(files: List<&2, C.Pair<String, String>>) -> Bool:
  match files:
    case Nil{}:
      False{}
    case Con{C.Pair{name, text}, rest}:
      String.eq(name, "mech-carrier.mech") || carrier_name(rest)

def add_carrier(+files: List<&2, C.Pair<String, String>>, carrier: List<&2, C.Pair<String, String>>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match carrier:
    case Nil{}:
      Done{files}
    case Con{file, rest}:
      Bool.pick(Result<&2, &2, String, List<&2, C.Pair<String, String>>>, carrier_name(files),
        Fail{"REFUSED output file `mech-carrier.mech` collides with the proof carrier\n"},
        Done{List.append(&2, C.Pair<String, String>, files, file <> rest)})

def with_carrier(r: Result<&2, &2, String, List<&2, C.Pair<String, String>>>, carrier: List<&2, C.Pair<String, String>>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{files}:
      add_carrier(files, carrier)
    case Fail{why}:
      Fail{why}

def emit_files(r: Result<&2, &2, String, List<&2, C.Pair<String, String>>>, +out: String) -> IO(Unit):
  match r:
    case Done{files}:
      do IO<Unit>:
        result : Result<&2, &2, String, Unit> <- Files.publish(out, files)
        Ro.wrote(result)
    case Fail{why}:
      IO.die(Unit, 65, why)

def import_crate(+dir: String, +out: String) -> IO(Unit):
  do IO<Unit>:
    present : Bool <- Files.exists(Files.normalize_target(out))
    free : Unit <- Ro.wrote(Ro.absent(present, out))
    +lib : String <- Ro.read_file(dir ++ "/src/lib.rs")
    srcs : List<&2, Rs.Src> <- mods_read(Rs.lib_mods(Parser.file(lib)), dir)
    +car : String = dir ++ "/mech-carrier.mech"
    has_car : Bool <- Files.exists(car)
    carrier : List<&2, C.Pair<String, String>> <- carrier_of(has_car, car)
    emit_files(with_carrier(crate_files(lib, srcs), carrier), out)

def run(args: List<String>) -> IO(Unit):
  match args:
    case Con{dir, Con{out, Nil{}}}:
      import_crate(dir, out)
    case other:
      usage()
