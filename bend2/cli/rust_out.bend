import Base
import ../io.bend as Files
import ../kernel/common.bend as C
import ../kernel/global.bend as Global
import ../kernel/error.bend as Error
import ../surface/syntax.bend as Syntax
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab
import ../rust/rir.bend as R
import ../rust/emit.bend as Em
import ../rust/erase_typed.bend as Et

# `mech rust-out FILE.mech... OUT_DIR`: one Rust module for each input file
# (file i is checked in the globals of the files before it), `src/lib.rs`,
# `src/nat.rs` if a module uses the kernel `Nat`, `Cargo.toml` and
# `mech-carrier.mech`. An output directory that exists is refused.

# Module names (D7).

def code_is(c: Char, +n: U32) -> Bool:
  +u = Char.to_u32(c)
  U32.is_ge(u, n) && U32.is_le(u, n)

def is_digit(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 48) && U32.is_le(Char.to_u32(c), 57)

def mod_char(+c: Char) -> Bool:
  Et.is_lower(c) || is_digit(c) || code_is(c, 95) || code_is(c, 45)

def mod_chars(s: String) -> Bool:
  match s:
    case SNil{}:
      True{}
    case SCon{c, rest}:
      mod_char(c) && mod_chars(rest)

# A stem is a lower-case letter, then lower-case letters, digits, `_` or `-`.
def mod_ok(s: String) -> Bool:
  match s:
    case SNil{}:
      False{}
    case SCon{c, rest}:
      Et.is_lower(c) && mod_chars(rest)

def dash(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{+c, rest}:
      SCon{Bool.pick(Char, code_is(c, 45), '_', c), dash(rest)}

def mod_name(stem: String) -> String:
  Et.rust_name(dash(stem))

# The text after the last `/`.
def base_name(s: String, cur: String) -> String:
  match s:
    case SNil{}:
      String.reverse(cur)
    case SCon{+c, rest}:
      base_name(rest, Bool.pick(String, code_is(c, 47), SNil{}, SCon{c, cur}))

def name_bad(+name: String) -> List<&2, String>:
  +st = Et.stem(name)
  Bool.pick(List<&2, String>, String.eq(st, name), ["refused " ++ name ++ ": the name of an input file must end in .mech"],
    Bool.pick(List<&2, String>, mod_ok(st), [],
      ["refused " ++ name ++ ": the file stem must be a lower-case letter, then lower-case letters, digits, `_` or `-` (D7)"]))

# Module names that the crate layout uses.
def reserved() -> List<&2, String>:
  ["bin", "lib", "main", "nat"]

def nat_names() -> List<&2, String>:
  ["Nat", "nat_small", "nat_add", "nat_decimal"]

# Sorted `use` lines (R7). rustfmt sorts lower-case names by byte.

def char_lt(a: Char, b: Char) -> Bool:
  Bool.pick(Bool, U32.is_ge(Char.to_u32(a), Char.to_u32(b)), False{}, True{})

def str_le(a: String, b: String) -> Bool:
  match a:
    case SNil{}:
      True{}
    case SCon{+x, xs}:
      match b:
        case SNil{}:
          False{}
        case SCon{+y, ys}:
          Bool.pick(Bool, char_lt(x, y), True{}, Bool.pick(Bool, char_lt(y, x), False{}, str_le(xs, ys)))

def insert(xs: List<&2, String>, +x: String) -> List<&2, String>:
  match xs:
    case Nil{}:
      [x]
    case Con{+y, +rest}:
      Bool.pick(List<&2, String>, str_le(x, y), x <> (y <> rest), y <> insert(rest, x))

def sorted(xs: List<&2, String>) -> List<&2, String>:
  match xs:
    case Nil{}:
      []
    case Con{x, rest}:
      insert(sorted(rest), x)

# The names that an item list mentions: types, called functions and
# families. A local closure with the name of a function of a module before
# counts as a use of that module.

@unsafe def tys_refs(ts: List<&2, R.Ty>) -> List<&2, String>:
  match ts:
    case Nil{}:
      []
    case Con{R.TBool{}, rest}:
      tys_refs(rest)
    case Con{R.TCon{name, args}, rest}:
      name <> tys_refs(List.append(&2, R.Ty, args, rest))
    case Con{R.TBox{inner}, rest}:
      tys_refs(inner <> rest)
    case Con{R.TRef{inner}, rest}:
      tys_refs(inner <> rest)
    case Con{R.TFn{args, ret}, rest}:
      tys_refs(List.append(&2, R.Ty, args, ret <> rest))

def arm_fams(arms: List<&2, R.Arm<R.Ex>>) -> List<&2, String>:
  match arms:
    case Nil{}:
      []
    case Con{R.Arm{R.PCtor{family, ctor, binds}, body}, rest}:
      family <> arm_fams(rest)

@unsafe def exs_refs(es: List<&2, R.Ex>) -> List<&2, String>:
  match es:
    case Nil{}:
      []
    case Con{R.XVar{name}, rest}:
      exs_refs(rest)
    case Con{R.XInt{text}, rest}:
      exs_refs(rest)
    case Con{R.XBool{value}, rest}:
      exs_refs(rest)
    case Con{R.XCall{func, targs, args}, rest}:
      func <> List.append(&2, String, tys_refs(targs), exs_refs(List.append(&2, R.Ex, args, rest)))
    case Con{R.XCtor{family, ctor, args}, rest}:
      family <> exs_refs(List.append(&2, R.Ex, args, rest))
    case Con{R.XMatch{scrut, +arms}, rest}:
      List.append(&2, String, arm_fams(arms), exs_refs(scrut <> List.append(&2, R.Ex, Em.arm_bodies(arms), rest)))
    case Con{R.XIf{cond, yes, no}, rest}:
      exs_refs(cond <> (yes <> (no <> rest)))
    case Con{R.XClosure{params, body}, rest}:
      List.append(&2, String, tys_refs(Et.param_tys(params)), exs_refs(body <> rest))
    case Con{R.XBorrow{inner}, rest}:
      exs_refs(inner <> rest)
    case Con{R.XClone{inner}, rest}:
      exs_refs(inner <> rest)
    case Con{R.XBox{inner}, rest}:
      exs_refs(inner <> rest)

def variant_tys(vs: List<&2, R.Variant>) -> List<&2, R.Ty>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fields}, rest}:
      List.append(&2, R.Ty, fields, variant_tys(rest))

def item_refs(item: R.Item) -> List<&2, String>:
  match item:
    case R.IUse{module}:
      []
    case R.IEnum{name, generics, copy, variants}:
      tys_refs(variant_tys(variants))
    case R.IUnit{name, ctor}:
      []
    case R.IFn{name, generics, params, ret, body}:
      List.append(&2, String, tys_refs(ret <> Et.param_tys(params)), exs_refs([body]))

def items_refs(items: List<&2, R.Item>) -> List<&2, String>:
  match items:
    case Nil{}:
      []
    case Con{item, rest}:
      List.append(&2, String, item_refs(item), items_refs(rest))

# The Rust item names of a module, for the collision check (D7).
def item_names(item: R.Item) -> List<&2, String>:
  match item:
    case R.IUse{module}:
      []
    case R.IEnum{name, generics, copy, variants}:
      [name]
    case R.IUnit{name, ctor}:
      [name, ctor]
    case R.IFn{name, generics, params, ret, body}:
      [name]

def items_names(items: List<&2, R.Item>) -> List<&2, String>:
  match items:
    case Nil{}:
      []
    case Con{item, rest}:
      List.append(&2, String, item_names(item), items_names(rest))

def hits(names: List<&2, String>, +refs: List<&2, String>) -> Bool:
  match names:
    case Nil{}:
      False{}
    case Con{x, rest}:
      Et.has(refs, x) || hits(rest, refs)

# `text`: the Rust text of the module. `names`: its item names.
type Mod is Data:
  Mod{name: String, text: String, names: List<&2, String>, nat: Bool}

def used_mods(mods: List<&2, Mod>, +refs: List<&2, String>) -> List<&2, String>:
  match mods:
    case Nil{}:
      []
    case Con{Mod{+name, text, names, nat}, rest}:
      List.append(&2, String, Bool.pick(List<&2, String>, hits(names, refs), [name], []), used_mods(rest, refs))

def use_items(names: List<&2, String>) -> List<&2, R.Item>:
  match names:
    case Nil{}:
      []
    case Con{name, rest}:
      R.IUse{name} <> use_items(rest)

def nat_mod(nat: Bool) -> List<&2, String>:
  Bool.pick(List<&2, String>, nat, ["nat"], [])

# The module of one file. `mods`: the modules of the files before it.
def new_mod(mods: List<&2, Mod>, +name: String, +items: List<&2, R.Item>) -> Mod:
  +refs = items_refs(items)
  +nat = hits(nat_names(), refs)
  Mod{mod_name(Et.stem(name)),
    Em.text(R.Module{name, List.append(&2, R.Item, use_items(sorted(List.append(&2, String, nat_mod(nat), used_mods(mods, refs)))), items)}),
    items_names(items), nat}

# The carrier (D2). A chunk starts at a line with a lower-case letter in
# column 1. The comment lines directly before that line go with the chunk.

# The lines of a text. `cur` and `done` are in reverse order. The call is a
# tail call: the depth of the stack does not grow with the text.
def lines_go(s: String, +cur: String, +done: List<&2, String>) -> List<&2, String>:
  match s:
    case SNil{}:
      List.reverse(&2, String, String.reverse(cur) <> done)
    case SCon{+c, rest}:
      +nl = code_is(c, 10)
      lines_go(rest, Bool.pick(String, nl, "", SCon{c, cur}), Bool.pick(List<&2, String>, nl, String.reverse(cur) <> done, done))

def lines_of(s: String) -> List<&2, String>:
  lines_go(s, "", [])

def words_go(s: String, +cur: String, +done: List<&2, String>) -> List<&2, String>:
  match s:
    case SNil{}:
      List.reverse(&2, String, String.reverse(cur) <> done)
    case SCon{+c, rest}:
      +sp = code_is(c, 32)
      words_go(rest, Bool.pick(String, sp, "", SCon{c, cur}), Bool.pick(List<&2, String>, sp, String.reverse(cur) <> done, done))

# The words of a line: the parts between single spaces.
def words(s: String) -> List<&2, String>:
  words_go(s, "", [])

def join_lines(lines: List<&2, String>) -> String:
  Et.join("\n", lines)

type Lk is Data:
  LHead{}
  LBlank{}
  LNote{}
  LBody{}

def dash_first(s: String) -> Bool:
  match s:
    case SNil{}:
      False{}
    case SCon{c, rest}:
      code_is(c, 45)

def line_kind(line: String) -> Lk:
  match line:
    case SNil{}:
      LBlank{}
    case SCon{+c, rest}:
      Bool.pick(Lk, code_is(c, 45) && dash_first(rest), LNote{}, Bool.pick(Lk, Et.is_lower(c), LHead{}, LBody{}))

# A chunk is a list of lines. Each list here is in reverse order. `pend`: the
# blank and comment lines after the last line of `cur`. `lead`: the comment
# lines after the last blank line.
type Ch is Data:
  Ch{done: List<&2, List<&2, String>>, cur: List<&2, String>, pend: List<&2, String>, lead: List<&2, String>}

def closed(cur: List<&2, String>, done: List<&2, List<&2, String>>) -> List<&2, List<&2, String>>:
  match cur:
    case Nil{}:
      done
    case Con{line, rest}:
      Con{Con{line, rest}, done}

def ch_kind(k: Lk, done: List<&2, List<&2, String>>, cur: List<&2, String>, pend: List<&2, String>, lead: List<&2, String>, +line: String) -> Ch:
  match k:
    case LHead{}:
      Ch{closed(cur, done), line <> lead, [], []}
    case LBlank{}:
      Ch{done, cur, "" <> pend, []}
    case LNote{}:
      Ch{done, cur, line <> pend, line <> lead}
    case LBody{}:
      Ch{done, List.append(&2, String, line <> pend, cur), [], []}

def ch_step(ch: Ch, +line: String) -> Ch:
  match ch:
    case Ch{done, cur, pend, lead}:
      ch_kind(line_kind(line), done, cur, pend, lead, line)

def ch_steps(lines: List<&2, String>, ch: Ch) -> Ch:
  match lines:
    case Nil{}:
      ch
    case Con{line, rest}:
      ch_steps(rest, ch_step(ch, line))

def chunk_lists(done: List<&2, List<&2, String>>, out: List<&2, List<&2, String>>) -> List<&2, List<&2, String>>:
  match done:
    case Nil{}:
      out
    case Con{lines, rest}:
      chunk_lists(rest, Con{List.reverse(&2, String, lines), out})

def ch_done(ch: Ch) -> List<&2, List<&2, String>>:
  match ch:
    case Ch{done, cur, pend, lead}:
      chunk_lists(closed(cur, done), [])

def chunks_of(text: String) -> List<&2, List<&2, String>>:
  ch_done(ch_steps(lines_of(text), Ch{[], [], [], []}))

# The side of a declaration: runtime (Rust), non-runtime (carrier), or both.
type Vd is Data:
  VNone{}
  VRun{}
  VProof{}
  VMixed{}

def class_vd(c: Et.Class) -> Vd:
  match c:
    case Et.CFn{name, generics, params, ret}:
      VRun{}
    case Et.CData{item}:
      VRun{}
    case Et.CBool{}:
      VRun{}
    case Et.CProp{}:
      VProof{}
    case Et.CType{}:
      VProof{}
    case Et.CAbsurd{}:
      VProof{}
    case Et.CNo{why}:
      VRun{}
    case Et.CSkip{}:
      VNone{}

def with_run(b: Vd) -> Vd:
  match b:
    case VNone{}:
      VRun{}
    case VRun{}:
      VRun{}
    case VProof{}:
      VMixed{}
    case VMixed{}:
      VMixed{}

def with_proof(b: Vd) -> Vd:
  match b:
    case VNone{}:
      VProof{}
    case VRun{}:
      VMixed{}
    case VProof{}:
      VProof{}
    case VMixed{}:
      VMixed{}

def both(a: Vd, b: Vd) -> Vd:
  match a:
    case VNone{}:
      b
    case VRun{}:
      with_run(b)
    case VProof{}:
      with_proof(b)
    case VMixed{}:
      VMixed{}

def names_vd(names: List<&2, String>, +env: Et.Env) -> Vd:
  match names:
    case Nil{}:
      VNone{}
    case Con{name, rest}:
      both(class_vd(Et.name_class(env, name)), names_vd(rest, env))

def prefix(p: String, s: String) -> Bool:
  match p:
    case SNil{}:
      True{}
    case SCon{a, pr}:
      match s:
        case SNil{}:
          False{}
        case SCon{b, sr}:
          String.eq(SCon{a, SNil{}}, SCon{b, SNil{}}) && prefix(pr, sr)

def contains(s: String, +p: String) -> Bool:
  match s:
    case SNil{}:
      String.eq(p, "")
    case SCon{c, +rest}:
      prefix(p, SCon{c, rest}) || contains(rest, p)

# `text`: the carried chunks of one file. `bad`: the refusal lines.
type Car is Data:
  Car{text: List<&2, String>, bad: List<&2, String>}

def car_text(car: Car, chunk: List<&2, String>) -> Car:
  match car:
    case Car{text, bad}:
      Car{List.append(&2, String, chunk, "" <> text), bad}

def car_bad(car: Car, why: String) -> Car:
  match car:
    case Car{text, bad}:
      Car{text, why <> bad}

# The key line of a carried chunk (unit D, D2): the file of the chunk and
# `prev`, the first name of the nearest runtime declaration before the chunk
# in that file. `prev` is "" if there is no such declaration. `rust-in` reads
# the key and puts the chunk back at that place.
def key_line(+file: String, +prev: String) -> String:
  Bool.pick(String, String.eq(prev, ""), "-- at " ++ file ++ " start", "-- at " ++ file ++ " after " ++ prev)

def is_run(v: Vd) -> Bool:
  match v:
    case VNone{}:
      False{}
    case VRun{}:
      True{}
    case VProof{}:
      False{}
    case VMixed{}:
      False{}

def add_vd(chunk: List<&2, String>, name: String, v: Vd, car: Car, file: String, prev: String) -> Car:
  match v:
    case VNone{}:
      car
    case VRun{}:
      car
    case VProof{}:
      car_text(car, key_line(file, prev) <> chunk)
    case VMixed{}:
      car_bad(car, "refused " ++ file ++ ": carrier: the declaration of " ++ name ++ " has runtime names and non-runtime names (D2)")

def any_has(lines: List<&2, String>, +p: String) -> Bool:
  match lines:
    case Nil{}:
      False{}
    case Con{line, rest}:
      contains(line, p) || any_has(rest, p)

def add_chunk(+chunk: List<&2, String>, +name: String, v: Vd, +car: Car, +file: String, prev: String) -> Car:
  Bool.pick(Car, any_has(chunk, " " ++ name), add_vd(chunk, name, v, car, file, prev),
    car_bad(car, "refused " ++ file ++ ": carrier: a top-level chunk does not have the name " ++ name ++ " (D2)"))

# Chunk i is declaration i. A count mismatch is a refusal. `prev`: the first
# name of the nearest runtime declaration before chunk i, "" if there is none.
def carry(chunks: List<&2, List<&2, String>>, decls: List<&2, Syntax.Decl>, +env: Et.Env, +file: String, +prev: String) -> Car:
  match chunks:
    case Nil{}:
      match decls:
        case Nil{}:
          Car{[],[]}
        case Con{d, rest}:
          Car{[],["refused " ++ file ++ ": carrier: more declarations than top-level chunks (D2)"]}
    case Con{chunk, more}:
      match decls:
        case Nil{}:
          Car{[],["refused " ++ file ++ ": carrier: more top-level chunks than declarations (D2)"]}
        case Con{d, rest}:
          +names = Elab.declared_names(d)
          +first = Et.first(names)
          +v = names_vd(names, env)
          add_chunk(chunk, first, v, carry(more, rest, env, file, Bool.pick(String, is_run(v), first, prev)), file, prev)

def car_parsed(r: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>, chunks: List<&2, List<&2, String>>, env: Et.Env, file: String) -> Car:
  match r:
    case Fail{e}:
      Car{[],["refused " ++ file ++ ": carrier: " ++ Error.to_string(e)]}
    case Done{decls}:
      carry(chunks, decls, env, file, "")

def carried(env: Et.Env, file: String, +text: String) -> Car:
  car_parsed(Parser.program(text), chunks_of(text), env, file)

def block(name: String, lines: List<&2, String>) -> List<&2, String>:
  match lines:
    case Nil{}:
      []
    case Con{line, rest}:
      ("-- from " ++ name) <> ("" <> (line <> rest))

# The crate.

# One input file: the name with no directory, and the text.
type Src is Data:
  Src{name: String, text: String}

# `mods` and `bad` hold the files so far; `mods` is in reverse order.
type Acc is Data:
  Acc{env: Et.Env, mods: List<&2, Mod>, carrier: List<&2, String>, bad: List<&2, String>}

def file_car(car: Car, env: Et.Env, mods: List<&2, Mod>, carrier: List<&2, String>, bad: List<&2, String>, name: String) -> Acc:
  match car:
    case Car{text, more}:
      Acc{env, mods, List.append(&2, String, carrier, block(name, text)), List.append(&2, String, bad, more)}

def file_es(es: Et.Es, +mods: List<&2, Mod>, carrier: List<&2, String>, +name: String, text: String) -> Acc:
  match es:
    case Et.Es{+env, items, bad}:
      file_car(carried(env, name, text), env, new_mod(mods, name, List.reverse(&2, R.Item, items)) <> mods, carrier,
        List.append(&2, String, name_bad(name), List.reverse(&2, String, bad)), name)

def file_go(env: Et.Env, mods: List<&2, Mod>, carrier: List<&2, String>, src: Src) -> Acc:
  match src:
    case Src{name, +text}:
      file_es(Et.emit_source(env, text), mods, carrier, name, text)

# A file after a refused file is not read: its globals are not there.
def file_step(acc: Acc, src: Src) -> Acc:
  match acc:
    case Acc{env, mods, carrier, bad}:
      match bad:
        case Nil{}:
          file_go(env, mods, carrier, src)
        case Con{why, more}:
          Acc{env, mods, carrier, why <> more}

def file_steps(srcs: List<&2, Src>, acc: Acc) -> Acc:
  match srcs:
    case Nil{}:
      acc
    case Con{src, rest}:
      file_steps(rest, file_step(acc, src))

def mod_names(mods: List<&2, Mod>) -> List<&2, String>:
  match mods:
    case Nil{}:
      []
    case Con{Mod{name, text, names, nat}, rest}:
      name <> mod_names(rest)

def all_names(mods: List<&2, Mod>) -> List<&2, String>:
  match mods:
    case Nil{}:
      []
    case Con{Mod{name, text, names, nat}, rest}:
      List.append(&2, String, names, all_names(rest))

def any_nat(mods: List<&2, Mod>) -> Bool:
  match mods:
    case Nil{}:
      False{}
    case Con{Mod{name, text, names, nat}, rest}:
      nat || any_nat(rest)

def dup_line(what: String, +d: String) -> List<&2, String>:
  Bool.pick(List<&2, String>, String.eq(d, ""), [], ["refused: " ++ what ++ " collision (D7): " ++ d])

def dup_bad(+mods: List<&2, Mod>) -> List<&2, String>:
  List.append(&2, String, dup_line("module name", Et.first_dup(List.append(&2, String, mod_names(mods), reserved()))),
    dup_line("name", Et.first_dup(List.append(&2, String, all_names(mods), nat_names()))))

def quote() -> String:
  SCon{Char.from_u32(34), SNil{}}

def cargo_toml() -> String:
  "[package]\nname = " ++ quote() ++ "mech_out" ++ quote() ++ "\nversion = " ++ quote() ++ "0.1.0" ++ quote() ++ "\nedition = " ++ quote() ++ "2021" ++ quote() ++ "\nlicense = " ++ quote() ++ "MIT OR Apache-2.0" ++ quote() ++ "\n\n[dependencies]\n"

def pub_mods(names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{name, rest}:
      "pub mod " ++ name ++ ";\n" ++ pub_mods(rest)

def lib_rs(names: List<&2, String>) -> String:
  "//! Emitted by `mech rust-out`.\n\n" ++ pub_mods(sorted(names))

def carrier_head() -> String:
  "-- Carrier of `mech rust-out`: the declarations with no Rust form, in source order."

# The kernel `Nat` module (Q5): limbs in base 10^9, no dependency.
def nat_rs() -> String:
  Et.join("\n", [
    "//! Kernel `Nat` for emitted crates: an unbounded natural number.",
    "",
    "use std::fmt;",
    "",
    "const BASE: u64 = 1_000_000_000;",
    "",
    "/// Little-endian limbs in base 10^9 with no trailing zero limb.",
    "#[derive(Clone, PartialEq, Eq)]",
    "pub struct Nat(Vec<u32>);",
    "",
    "fn limb(value: u64) -> u32 {",
    "    u32::try_from(value % BASE).unwrap_or(0)",
    "}",
    "",
    "fn pushed(limbs: Vec<u32>, last: u32) -> Vec<u32> {",
    "    limbs.into_iter().chain(std::iter::once(last)).collect()",
    "}",
    "",
    "fn trimmed(limbs: Vec<u32>) -> Vec<u32> {",
    "    let kept = limbs.iter().rposition(|l| *l != 0).map_or(0, |i| i + 1);",
    "    limbs.into_iter().take(kept).collect()",
    "}",
    "",
    "pub fn nat_small(value: u32) -> Nat {",
    "    let wide = u64::from(value);",
    "    Nat(trimmed(vec![limb(wide), limb(wide / BASE)]))",
    "}",
    "",
    "pub fn nat_add(left: &Nat, right: &Nat) -> Nat {",
    "    let width = left.0.len().max(right.0.len());",
    "    let (limbs, carry) = (0..width).fold((Vec::new(), 0u64), |(limbs, carry), i| {",
    "        let sum = u64::from(left.0.get(i).copied().unwrap_or(0))",
    "            + u64::from(right.0.get(i).copied().unwrap_or(0))",
    "            + carry;",
    "        (pushed(limbs, limb(sum)), sum / BASE)",
    "    });",
    "    Nat(trimmed(pushed(limbs, limb(carry))))",
    "}",
    "",
    "pub fn nat_decimal(n: &Nat) -> String {",
    "    n.0.split_last().map_or_else(",
    "        || String::from(" ++ quote() ++ "0" ++ quote() ++ "),",
    "        |(top, rest)| {",
    "            rest.iter()",
    "                .rev()",
    "                .fold(top.to_string(), |text, l| format!(" ++ quote() ++ "{text}{l:09}" ++ quote() ++ "))",
    "        },",
    "    )",
    "}",
    "",
    "impl fmt::Debug for Nat {",
    "    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {",
    "        f.write_str(&nat_decimal(self))",
    "    }",
    "}"]) ++ "\n"

def src_files(mods: List<&2, Mod>) -> List<&2, C.Pair<String, String>>:
  match mods:
    case Nil{}:
      []
    case Con{Mod{name, text, names, nat}, rest}:
      C.Pair{name ++ ".rs", text} <> src_files(rest)

def nat_file(nat: Bool) -> List<&2, C.Pair<String, String>>:
  Bool.pick(List<&2, C.Pair<String, String>>, nat, [C.Pair{"nat.rs", nat_rs()}], [])

# `root`: the files of the crate directory. `src`: the files of `src/`.
type Out is Data:
  Out{root: List<&2, C.Pair<String, String>>, src: List<&2, C.Pair<String, String>>}

def crate_out(bad: List<&2, String>, +mods: List<&2, Mod>, carrier: List<&2, String>, files: List<&2, String>) -> Result<&2, &2, String, Out>:
  match bad:
    case Nil{}:
      +nat = any_nat(mods)
      Done{Out{[C.Pair{"Cargo.toml", cargo_toml()}, C.Pair{"mech-carrier.mech", Et.join("\n", carrier_head() <> (("-- files " ++ Et.join(" ", files)) <> ("" <> carrier)))}],
        C.Pair{"lib.rs", lib_rs(List.append(&2, String, nat_mod(nat), mod_names(mods)))} <>
          List.append(&2, C.Pair<String, String>, nat_file(nat), src_files(mods))}}
    case Con{why, more}:
      Fail{Et.join("\n", why <> more) ++ "\n"}

def finish(acc: Acc, files: List<&2, String>) -> Result<&2, &2, String, Out>:
  match acc:
    case Acc{env, +mods, carrier, bad}:
      crate_out(List.append(&2, String, bad, dup_bad(mods)), List.reverse(&2, Mod, mods), carrier, files)

# Rust dependencies omit proof-only edges. Keep every source file in order,
# including files with only runtime declarations or only carried declarations.
def source_files(srcs: List<&2, Src>) -> List<&2, String>:
  match srcs:
    case Nil{}:
      []
    case Con{Src{name, text}, rest}:
      name <> source_files(rest)

# The files of the crate, or the refusal lines.
def crate_of(+srcs: List<&2, Src>) -> Result<&2, &2, String, Out>:
  finish(file_steps(srcs, Acc{Et.Env{Global.initial(), []}, [], [], []}), source_files(srcs))

# The command.

def usage() -> IO(Unit):
  IO.die(Unit, 64, "usage: mech rust-out FILE.mech... OUT_DIR")

def read_result(result: Result<&2, &2, String, String>, path: String) -> IO(String):
  match result:
    case Done{text}:
      IO.pure(String, text)
    case Fail{message}:
      IO.die(String, 64, "mech: cannot read " ++ path)

def read_file(+path: String) -> IO(String):
  do IO<String>:
    result : Result<&2, &2, String, String> <- Files.read_file(path)
    read_result(result, path)

def read_srcs(paths: List<&2, String>) -> IO(List<&2, Src>):
  match paths:
    case Nil{}:
      IO.pure(List<&2, Src>, [])
    case Con{+path, rest}:
      do IO<List<&2, Src>>:
        text : String <- read_file(path)
        more : List<&2, Src> <- read_srcs(rest)
        return Src{base_name(path, SNil{}), text} <> more

def wrote(result: Result<&2, &2, String, Unit>) -> IO(Unit):
  match result:
    case Done{unit}:
      IO.pure(Unit, Unit{})
    case Fail{message}:
      IO.die(Unit, 1, "mech: " ++ message)

def made(result: Result<&2, &2, String, String>) -> IO(String):
  match result:
    case Done{path}:
      IO.pure(String, path)
    case Fail{message}:
      IO.die(String, 1, "mech: " ++ message)

def absent(present: Bool, out: String) -> Result<&2, &2, String, Unit>:
  Bool.pick(Result<&2, &2, String, Unit>, present, Fail{"output already exists: " ++ out}, Done{Unit{}})

# The crate goes to a temporary directory next to the target, then one
# rename publishes it. `src` is made the same way (the host has no mkdir).
# A write failure leaves the temporary directory.
def write_crate(+out: String, o: Out) -> IO(Unit):
  match o:
    case Out{root, src}:
      do IO<Unit>:
        +target : String = Files.normalize_target(out)
        staged : Result<&2, &2, String, String> <- Files.fs(1, target, "")
        +top : String <- made(staged)
        root_result : Result<&2, &2, String, Unit> <- Files.write_files(root, top)
        root_done : Unit <- wrote(root_result)
        src_staged : Result<&2, &2, String, String> <- Files.fs(1, top ++ "/src", "")
        +part : String <- made(src_staged)
        src_result : Result<&2, &2, String, Unit> <- Files.write_files(src, part)
        src_done : Unit <- wrote(src_result)
        src_moved : Result<&2, &2, String, Unit> <- Files.publish_rename(False{}, part, top ++ "/src", out)
        src_ok : Unit <- wrote(src_moved)
        present : Bool <- Files.exists(target)
        moved : Result<&2, &2, String, Unit> <- Files.publish_rename(present, top, target, out)
        wrote(moved)

def emit_crate(r: Result<&2, &2, String, Out>, out: String) -> IO(Unit):
  match r:
    case Done{o}:
      write_crate(out, o)
    case Fail{why}:
      IO.die(Unit, 65, why)

# `paths`: the input files in order. `out`: the last argument.
type Cmd is Data:
  Cmd{paths: List<&2, String>, out: String}
  CmdNone{}

def cmd_push(path: String, c: Cmd) -> Cmd:
  match c:
    case Cmd{paths, out}:
      Cmd{path <> paths, out}
    case CmdNone{}:
      CmdNone{}

def cmd_of(args: List<String>) -> Cmd:
  match args:
    case Nil{}:
      CmdNone{}
    case Con{a, Nil{}}:
      Cmd{[], a}
    case Con{a, rest}:
      cmd_push(a, cmd_of(rest))

def run_cmd(c: Cmd) -> IO(Unit):
  match c:
    case CmdNone{}:
      usage()
    case Cmd{Nil{}, out}:
      usage()
    case Cmd{paths, +out}:
      do IO<Unit>:
        present : Bool <- Files.exists(Files.normalize_target(out))
        free : Unit <- wrote(absent(present, out))
        srcs : List<&2, Src> <- read_srcs(paths)
        emit_crate(crate_of(srcs), out)

def run(args: List<String>) -> IO(Unit):
  run_cmd(cmd_of(args))
