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
# The command puts each chunk of the carrier back in its file before the
# kernel check (unit D, D2), so the check reads each carried proof.
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

# The carrier merge (unit D, D2). `rust-out` writes a key line before each
# carried chunk: `-- at FILE start` or `-- at FILE after NAME`. NAME is the
# first name of the nearest runtime declaration before the chunk in FILE.

type Key is Data:
  KBad{}
  KAt{file: String, after: String}

# One carried chunk: its file, its anchor ("" for the start of the file) and
# its lines with no key line.
type Kept is Data:
  Kept{file: String, after: String, lines: List<&2, String>}

def key_tail(file: String, how: String, rest: List<&2, String>) -> Key:
  match rest:
    case Nil{}:
      Bool.pick(Key, String.eq(how, "start"), KAt{file, ""}, KBad{})
    case Con{+name, more}:
      match more:
        case Nil{}:
          Bool.pick(Key, String.eq(how, "after"), Bool.pick(Key, String.eq(name, ""), KBad{}, KAt{file, name}), KBad{})
        case Con{x, y}:
          KBad{}

def key_of(ws: List<&2, String>) -> Key:
  match ws:
    case Con{a, Con{b, Con{file, Con{how, rest}}}}:
      Bool.pick(Key, String.eq(a, "--") && String.eq(b, "at"), key_tail(file, how, rest), KBad{})
    case other:
      KBad{}

def kept_key(k: Key, line: String, rest: List<&2, String>) -> Result<&2, &2, String, Kept>:
  match k:
    case KBad{}:
      Fail{"REFUSED carrier: the first line of a chunk is not a key line: " ++ line ++ "\n"}
    case KAt{file, after}:
      Done{Kept{file, after, rest}}

def kept_chunk(chunk: List<&2, String>) -> Result<&2, &2, String, Kept>:
  match chunk:
    case Nil{}:
      Fail{"REFUSED carrier: a chunk has no line\n"}
    case Con{+line, rest}:
      kept_key(key_of(Ro.words(line)), line, rest)

def kept_more(a: Result<&2, &2, String, Kept>, b: Result<&2, &2, String, List<&2, Kept>>) -> Result<&2, &2, String, List<&2, Kept>>:
  match a:
    case Fail{why}:
      Fail{why}
    case Done{k}:
      match b:
        case Fail{why}:
          Fail{why}
        case Done{ks}:
          Done{k <> ks}

# The chunks of the carrier text, in the order of the carrier.
def kept_all(chunks: List<&2, List<&2, String>>) -> Result<&2, &2, String, List<&2, Kept>>:
  match chunks:
    case Nil{}:
      Done{[]}
    case Con{chunk, more}:
      kept_more(kept_chunk(chunk), kept_all(more))

def first_word(ws: List<&2, String>) -> String:
  match ws:
    case Nil{}:
      ""
    case Con{w, rest}:
      w

def name_after(ws: List<&2, String>) -> String:
  match ws:
    case Nil{}:
      ""
    case Con{+w, rest}:
      Bool.pick(String, String.eq(w, "rec"), first_word(rest), w)

def head_name(ws: List<&2, String>) -> String:
  match ws:
    case Nil{}:
      ""
    case Con{kw, rest}:
      name_after(rest)

# The declared name of a printed chunk: `def NAME`, `def rec NAME`, `mu NAME`.
def chunk_name(chunk: List<&2, String>) -> String:
  match chunk:
    case Nil{}:
      ""
    case Con{line, rest}:
      head_name(Ro.words(line))

# The lines of the kept chunks of `file` with the anchor `after`.
def kept_at(ks: List<&2, Kept>, +file: String, +after: String) -> List<&2, String>:
  match ks:
    case Nil{}:
      []
    case Con{Kept{f, a, lines}, rest}:
      List.append(&2, String, Bool.pick(List<&2, String>, String.eq(f, file) && String.eq(a, after), lines, []), kept_at(rest, file, after))

def named_at(+ks: List<&2, Kept>, +file: String, +name: String) -> List<&2, String>:
  Bool.pick(List<&2, String>, String.eq(name, ""), [], kept_at(ks, file, name))

def merged_chunks(chunks: List<&2, List<&2, String>>, +ks: List<&2, Kept>, +file: String) -> List<&2, String>:
  match chunks:
    case Nil{}:
      []
    case Con{+chunk, more}:
      List.append(&2, String, chunk, List.append(&2, String, named_at(ks, file, chunk_name(chunk)), merged_chunks(more, ks, file)))

def merged_text(+file: String, text: String, +ks: List<&2, Kept>) -> String:
  Ro.join_lines(List.append(&2, String, kept_at(ks, file, ""), merged_chunks(Ro.chunks_of(text), ks, file))) ++ "\n"

def has_file(ks: List<&2, Kept>, +file: String) -> Bool:
  match ks:
    case Nil{}:
      False{}
    case Con{Kept{f, a, lines}, rest}:
      String.eq(f, file) || has_file(rest, file)

# A file with no kept chunk keeps its text with no change.
def merged_low(+file: String, +text: String, +ks: List<&2, Kept>) -> Lo.Low:
  Lo.Low{file, Bool.pick(String, has_file(ks, file), merged_text(file, text, ks), text)}

def merged_lows(ls: List<&2, Lo.Low>, +ks: List<&2, Kept>) -> List<&2, Lo.Low>:
  match ls:
    case Nil{}:
      []
    case Con{Lo.Low{file, text}, rest}:
      merged_low(file, text, ks) <> merged_lows(rest, ks)

def names_have(chunks: List<&2, List<&2, String>>, +after: String) -> Bool:
  match chunks:
    case Nil{}:
      False{}
    case Con{chunk, more}:
      String.eq(chunk_name(chunk), after) || names_have(more, after)

def file_place(+after: String, text: String) -> Bool:
  String.eq(after, "") || names_have(Ro.chunks_of(text), after)

# A key has a place if its file is a file of the import and its anchor is
# the start or the name of a declaration of that file.
def anchor_ok(ls: List<&2, Lo.Low>, +file: String, +after: String) -> Bool:
  match ls:
    case Nil{}:
      False{}
    case Con{Lo.Low{f, text}, rest}:
      Bool.pick(Bool, String.eq(f, file), file_place(after, text), anchor_ok(rest, file, after))

def keys_bad(ks: List<&2, Kept>, +ls: List<&2, Lo.Low>) -> List<&2, String>:
  match ks:
    case Nil{}:
      []
    case Con{Kept{+file, +after, lines}, rest}:
      List.append(&2, String, Bool.pick(List<&2, String>, anchor_ok(ls, file, after), [], ["REFUSED carrier: the import has no place for the key line: " ++ Ro.key_line(file, after) ++ "\n"]), keys_bad(rest, ls))

def merge_ok(bad: List<&2, String>, ls: List<&2, Lo.Low>, ks: List<&2, Kept>) -> Result<&2, &2, String, List<&2, Lo.Low>>:
  match bad:
    case Nil{}:
      Done{merged_lows(ls, ks)}
    case Con{why, more}:
      Fail{why}

def merge_kept(r: Result<&2, &2, String, List<&2, Kept>>, +ls: List<&2, Lo.Low>) -> Result<&2, &2, String, List<&2, Lo.Low>>:
  match r:
    case Fail{why}:
      Fail{why}
    case Done{+ks}:
      merge_ok(keys_bad(ks, ls), ls, ks)

def checked(r: Result<&2, &2, String, List<&2, Lo.Low>>) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{+ls}:
      kernel_ok(Elab.check_text(Global.initial(), Budget.unlimited(), Lo.source(ls)), ls)
    case Fail{why}:
      Fail{why}

# The second carrier line records source order. Rust's module dependency
# graph cannot order files whose only dependency is in a carried proof.
# Taking each file once verifies that the order is a complete permutation.
def take_mod(ls: List<&2, Rs.Mod>, +file: String, +before: List<&2, Rs.Mod>) -> Result<&2, &2, String, C.Pair<Rs.Mod, List<&2, Rs.Mod>>>:
  match ls:
    case Nil{}:
      Fail{"REFUSED carrier: unknown or repeated file in source order: " ++ file ++ "\n"}
    case Con{Rs.Mod{+name, +path, +items}, +rest}:
      Bool.pick(Result<&2, &2, String, C.Pair<Rs.Mod, List<&2, Rs.Mod>>>, String.eq(file, path),
        Done{C.Pair{Rs.Mod{name, path, items}, List.append(&2, Rs.Mod, List.reverse(&2, Rs.Mod, before), rest)}},
        take_mod(rest, file, Rs.Mod{name, path, items} <> before))

def taken_mod(taken: C.Pair<Rs.Mod, List<&2, Rs.Mod>>) -> Rs.Mod:
  match taken:
    case C.Pair{low, rest}:
      low

def taken_rest(taken: C.Pair<Rs.Mod, List<&2, Rs.Mod>>) -> List<&2, Rs.Mod>:
  match taken:
    case C.Pair{low, rest}:
      rest

def ordered_mods(files: List<&2, String>, ls: List<&2, Rs.Mod>) -> Result<&2, &2, String, List<&2, Rs.Mod>>:
  match files:
    case Nil{}:
      match ls:
        case Nil{}:
          Done{[]}
        case Con{Rs.Mod{name, file, items}, rest}:
          Fail{"REFUSED carrier: source order omits file: " ++ file ++ "\n"}
    case Con{file, rest}:
      do Result<&2, &2, String, List<&2, Rs.Mod>>:
        +taken : C.Pair<Rs.Mod, List<&2, Rs.Mod>> <- take_mod(ls, file, [])
        more : List<&2, Rs.Mod> <- ordered_mods(rest, taken_rest(taken))
        return taken_mod(taken) <> more

def order_line(ws: List<&2, String>, +ls: List<&2, Rs.Mod>) -> Result<&2, &2, String, List<&2, Rs.Mod>>:
  match ws:
    case Con{a, Con{b, files}}:
      Bool.pick(Result<&2, &2, String, List<&2, Rs.Mod>>, String.eq(a, "--") && String.eq(b, "files"), ordered_mods(files, ls), Done{ls})
    case other:
      Done{ls}

def carrier_order(lines: List<&2, String>, ls: List<&2, Rs.Mod>) -> Result<&2, &2, String, List<&2, Rs.Mod>>:
  match lines:
    case Con{header, Con{line, rest}}:
      order_line(Ro.words(line), ls)
    case other:
      Done{ls}

def low_ok(r: Result<&2, &2, Rs.Bad, List<&2, Lo.Low>>, carrier: String) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{ls}:
      checked(merge_kept(kept_all(Ro.chunks_of(carrier)), ls))
    case Fail{b}:
      Fail{"REFUSED " ++ Rs.show(b) ++ "\n"}

# Apply carrier order before lowering: the lowerer places the implicit
# MechBool family in the first file, where carried anchors may need it.
def resolved_carrier(r: Result<&2, &2, Rs.Bad, Rs.Out>, +carrier: String) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  match r:
    case Done{Rs.Out{mods, rows}}:
      do Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
        ordered : List<&2, Rs.Mod> <- carrier_order(Ro.lines_of(carrier), mods)
        files : List<&2, C.Pair<String, String>> <- low_ok(lows_of(Done{Rs.Out{ordered, rows}}), carrier)
        return files
    case Fail{b}:
      Fail{"REFUSED " ++ Rs.show(b) ++ "\n"}

# The pure part: the text of src/lib.rs, of each module file and of the
# carrier ("" if the crate has none) give the `.mech` files and MANIFEST, or
# the refusal text.
def crate_merged(lib: String, +srcs: List<&2, Rs.Src>, carrier: String) -> Result<&2, &2, String, List<&2, C.Pair<String, String>>>:
  resolved_carrier(Rs.resolve(lib, srcs), carrier)

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

# The carrier is also copied with no change. The merge reads the same text.
def carrier_of(present: Bool, +path: String) -> IO(List<&2, C.Pair<String, String>>):
  Bool.pick(IO(List<&2, C.Pair<String, String>>), present, carrier_file(path), IO.pure(List<&2, C.Pair<String, String>>, []))

# A module's CD4 file name may be `mech-carrier.mech`. Refuse a collision
# before publication so the carrier cannot overwrite a checked module.
def carried_text(files: List<&2, C.Pair<String, String>>) -> String:
  match files:
    case Nil{}:
      ""
    case Con{C.Pair{name, text}, rest}:
      text

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
    +carrier : List<&2, C.Pair<String, String>> <- carrier_of(has_car, car)
    emit_files(with_carrier(crate_merged(lib, srcs, carried_text(carrier)), carrier), out)

def run(args: List<String>) -> IO(Unit):
  match args:
    case Con{dir, Con{out, Nil{}}}:
      import_crate(dir, out)
    case other:
      usage()
