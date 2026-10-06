import Base
import ./ast.bend as A
import ./rir.bend as R
import ./parser.bend as Parser
import ./lift.bend as Li
import ./erase_typed.bend as Et
import ../cli/rust_out.bend as Ro

# Resolve for the Rust importer: the module list of lib.rs and its order
# (CD3), the item table with the mech name of each Rust name (CD5, the
# inverse of D7), the collision checks, the name check of each body, and the
# `.mech` file of each module (CD4). `src/nat.rs` is not parsed: it must be
# the fixed text of the emitter (CD7). The D7 functions, the module name
# rules and the text of `src/nat.rs` come from the emitter, so the two
# directions use one set of rules.
#
# A refusal has the position of its item (CD13), and the shown text is
# `src/<module>.rs:<line>:<col>: <construct>`. Bend 2 has no mutual
# recursion, so `walk` is the one recursive walker of types and bodies.

type Bad is Data:
  Bad{module: String, pos: A.Pos, what: String}

def show(b: Bad) -> String:
  match b:
    case Bad{module, pos, what}:
      "src/" ++ module ++ ".rs:" ++ A.show_pos(pos) ++ ": " ++ what

# The position of the `//!` line.
def top() -> A.Pos:
  A.Pos{1n, 1n}

# A module name and the text of its file.
type Src is Data:
  Src{name: String, text: String}

# An item of the erased IR with the position of its first Rust item.
type Placed is Data:
  Placed{pos: A.Pos, item: R.Item}

# `file`: the `.mech` file of the module (CD4).
type Mod is Data:
  Mod{name: String, file: String, items: List<&2, Placed>}

type Rk is Data:
  RkEnum{}
  RkVariant{}
  RkStruct{}
  RkCtor{}
  RkFn{}

# One row of the item table. `rust`: the Rust name (a variant as
# `<enum>::<variant>`). `mech`: the mech name (CD5).
type Row is Data:
  Row{module: String, kind: Rk, rust: String, mech: String, pos: A.Pos}

# `mods`: the modules in manifest order. `rows`: the item table in the same
# order.
type Out is Data:
  Out{mods: List<&2, Mod>, rows: List<&2, Row>}

# ---- lib.rs ----

def in_module(module: String, r: Result<&2, &2, Li.Refusal, Bool>) -> Result<&2, &2, Bad, Bool>:
  match r:
    case Done{b}:
      Done{b}
    case Fail{Li.Refusal{pos, what}}:
      Fail{Bad{module, pos, what}}

def plain_in(module: String, m: A.Meta) -> Result<&2, &2, Bad, Bool>:
  in_module(module, Li.plain(m))

def lib_mod(+meta: A.Meta, vis: A.Vis, inline: Bool, items: List<&2, A.Node>) -> Result<&2, &2, Bad, Bool>:
  match vis items:
    case A.Pub{} Nil{}:
      Bool.pick(Result<&2, &2, Bad, Bool>, inline, Fail{Bad{"lib", A.meta_pos(meta), "inline `mod` in lib.rs"}}, plain_in("lib", meta))
    case v xs:
      Fail{Bad{"lib", A.meta_pos(meta), "item other than `pub mod <name>;` in lib.rs"}}

# A module name that the emitter writes: the D7 module name of itself, and
# not one of the names of the crate layout. `nat` is the kernel module.
def mod_named(pos: A.Pos, +name: String) -> Result<&2, &2, Bad, Bool>:
  Bool.pick(Result<&2, &2, Bad, Bool>,
    Ro.mod_ok(name) && String.eq(Ro.mod_name(name), name) && (String.eq(name, "nat") || Bool.pick(Bool, Et.has(Ro.reserved(), name), False{}, True{})),
    Done{True{}},
    Fail{Bad{"lib", pos, "module name `" ++ name ++ "` that the emitter does not write (D7)"}})

@unsafe def mod_decls(xs: List<&2, A.Node>) -> Result<&2, &2, Bad, List<&2, String>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.IMod{+meta, vis, +name, inline, inner, items}, rest}:
      do Result<&2, &2, Bad, List<&2, String>>:
        ok : Bool <- lib_mod(meta, vis, inline, items)
        n : Bool <- mod_named(A.meta_pos(meta), name)
        more : List<&2, String> <- mod_decls(rest)
        return name <> more
    case Con{+other, rest}:
      Fail{Bad{"lib", A.item_pos(other), "item `" ++ A.item_line(other) ++ "` in lib.rs"}}

def distinct_mods(+names: List<&2, String>) -> Result<&2, &2, Bad, List<&2, String>>:
  Bool.pick(Result<&2, &2, Bad, List<&2, String>>, String.eq(Et.first_dup(names), ""), Done{names},
    Fail{Bad{"lib", top(), "module name collision (D7): " ++ Et.first_dup(names)}})

def lib_mods(r: Result<&2, &2, String, A.SrcFile>) -> Result<&2, &2, Bad, List<&2, String>>:
  match r:
    case Fail{msg}:
      Fail{Bad{"lib", A.no_pos(), "parse: " ++ msg}}
    case Done{A.SrcFile{inner, items}}:
      do Result<&2, &2, Bad, List<&2, String>>:
        names : List<&2, String> <- mod_decls(items)
        ok : List<&2, String> <- distinct_mods(names)
        return ok

# ---- module files ----

def text_of(srcs: List<&2, Src>, +name: String) -> Maybe<&2, String>:
  match srcs:
    case Nil{}:
      None{}
    case Con{Src{n, t}, rest}:
      Bool.pick(Maybe<&2, String>, String.eq(n, name), Some{t}, text_of(rest, name))

def drop_nat(xs: List<&2, String>) -> List<&2, String>:
  match xs:
    case Nil{}:
      []
    case Con{+x, rest}:
      +tail = drop_nat(rest)
      Bool.pick(List<&2, String>, String.eq(x, "nat"), tail, x <> tail)

# CD7: the kernel module is the fixed text of the emitter, byte for byte.
def nat_text(t: Maybe<&2, String>) -> Result<&2, &2, Bad, Bool>:
  match t:
    case None{}:
      Fail{Bad{"nat", A.no_pos(), "no text for the module"}}
    case Some{s}:
      Bool.pick(Result<&2, &2, Bad, Bool>, String.eq(s, Ro.nat_rs()), Done{True{}},
        Fail{Bad{"nat", top(), "text other than the fixed `nat` module of the emitter (CD7)"}})

def nat_need(has: Bool, t: Maybe<&2, String>) -> Result<&2, &2, Bad, Bool>:
  Bool.pick(Result<&2, &2, Bad, Bool>, has, nat_text(t), Done{False{}})

# The position of each item of the IR: a unit struct and its constructor fn
# are two Rust items and one IR item.
@unsafe def placed(xs: List<&2, A.Node>, rs: List<&2, R.Item>) -> List<&2, Placed>:
  match xs rs:
    case Con{x, Con{y, xr}} Con{R.IUnit{name, ctor}, rr}:
      Placed{A.item_pos(x), R.IUnit{name, ctor}} <> placed(xr, rr)
    case Con{x, xr} Con{r, rr}:
      Placed{A.item_pos(x), r} <> placed(xr, rr)
    case a b:
      []

# CD4: the file of the `//!` line of the emitter, else `<module>.mech`. The
# stem of the file must give the module name back (D7), so `rust-out` on the
# file writes the same module.
def file_ok(+module: String, +file: String) -> Result<&2, &2, Bad, String>:
  Bool.pick(Result<&2, &2, Bad, String>,
    String.eq(Et.stem(file) ++ ".mech", file) && Ro.mod_ok(Et.stem(file)) && String.eq(Ro.mod_name(Et.stem(file)), module),
    Done{file},
    Fail{Bad{module, top(), "file `" ++ file ++ "` that does not give the module name (CD4)"}})

def file_name(+module: String, +source: String) -> Result<&2, &2, Bad, String>:
  file_ok(module, Bool.pick(String, String.eq(source, ""), module ++ ".mech", source))

def src_items(f: A.SrcFile) -> List<&2, A.Node>:
  match f:
    case A.SrcFile{inner, items}:
      items

def with_lift(+name: String, items: List<&2, A.Node>, r: Result<&2, &2, Li.Refusal, R.Module>) -> Result<&2, &2, Bad, Mod>:
  match r:
    case Fail{Li.Refusal{pos, what}}:
      Fail{Bad{name, pos, what}}
    case Done{R.Module{source, its}}:
      do Result<&2, &2, Bad, Mod>:
        file : String <- file_name(name, source)
        return Mod{name, file, placed(items, its)}

def parsed(+name: String, r: Result<&2, &2, String, A.SrcFile>) -> Result<&2, &2, Bad, Mod>:
  match r:
    case Fail{msg}:
      Fail{Bad{name, A.no_pos(), "parse: " ++ msg}}
    case Done{+f}:
      with_lift(name, src_items(f), Li.lift(f))

def load_one(+name: String, t: Maybe<&2, String>) -> Result<&2, &2, Bad, Mod>:
  match t:
    case None{}:
      Fail{Bad{name, A.no_pos(), "no text for the module"}}
    case Some{s}:
      parsed(name, Parser.file(s))

@unsafe def load(names: List<&2, String>, +srcs: List<&2, Src>) -> Result<&2, &2, Bad, List<&2, Mod>>:
  match names:
    case Nil{}:
      Done{[]}
    case Con{+name, rest}:
      do Result<&2, &2, Bad, List<&2, Mod>>:
        m : Mod <- load_one(name, text_of(srcs, name))
        more : List<&2, Mod> <- load(rest, srcs)
        return m <> more

# ---- names (CD5) ----

# The inverse of rule D7 is in erase_typed.bend (`Et.fn_back`,
# `Et.lower_first`): the emitter and the importer use one definition.

# A row if the name is canonical: D7 of the mech name (`back`) is the Rust
# name. `shown` is the Rust name in the table.
def named(+module: String, k: Rk, shown: String, +rust: String, +mech: String, +back: String, +pos: A.Pos) -> Result<&2, &2, Bad, Row>:
  Bool.pick(Result<&2, &2, Bad, Row>, String.eq(back, rust), Done{Row{module, k, shown, mech, pos}},
    Fail{Bad{module, pos, "name `" ++ rust ++ "` that is not canonical: D7 of `" ++ mech ++ "` is `" ++ back ++ "` (CD5)"}})

# A fn or the constructor fn of a unit struct: snake_case -> camelCase.
def fn_row(+module: String, k: Rk, +rust: String, +pos: A.Pos) -> Result<&2, &2, Bad, Row>:
  +mech = Et.fn_back(rust)
  named(module, k, rust, rust, mech, Et.fn_name(mech), pos)

# A variant: UpperCamel -> a constructor with a lower-case first letter.
def variant_row(+module: String, +family: String, +v: String, +pos: A.Pos) -> Result<&2, &2, Bad, Row>:
  +mech = Et.lower_first(v)
  named(module, RkVariant{}, family ++ "::" ++ v, v, mech, Et.upper_first(mech), pos)

def variant_rows(+module: String, +family: String, +pos: A.Pos, vs: List<&2, R.Variant>) -> Result<&2, &2, Bad, List<&2, Row>>:
  match vs:
    case Nil{}:
      Done{[]}
    case Con{R.Variant{v, fs}, rest}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        r : Row <- variant_row(module, family, v, pos)
        more : List<&2, Row> <- variant_rows(module, family, pos, rest)
        return r <> more

# A family name is kept (D7).
def item_rows(+module: String, p: Placed) -> Result<&2, &2, Bad, List<&2, Row>>:
  match p:
    case Placed{pos, R.IUse{m}}:
      Done{[]}
    case Placed{+pos, R.IEnum{+name, gs, cp, vs}}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        vr : List<&2, Row> <- variant_rows(module, name, pos, vs)
        return Row{module, RkEnum{}, name, name, pos} <> vr
    case Placed{+pos, R.IUnit{+name, ctor}}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        c : Row <- fn_row(module, RkCtor{}, ctor, pos)
        return [Row{module, RkStruct{}, name, name, pos}, c]
    case Placed{pos, R.IFn{name, gs, ps, rt, body}}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        f : Row <- fn_row(module, RkFn{}, name, pos)
        return [f]

@unsafe def mod_rows(+module: String, ps: List<&2, Placed>) -> Result<&2, &2, Bad, List<&2, Row>>:
  match ps:
    case Nil{}:
      Done{[]}
    case Con{p, rest}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        rs : List<&2, Row> <- item_rows(module, p)
        more : List<&2, Row> <- mod_rows(module, rest)
        return List.append(&2, Row, rs, more)

@unsafe def all_rows(ms: List<&2, Mod>) -> Result<&2, &2, Bad, List<&2, Row>>:
  match ms:
    case Nil{}:
      Done{[]}
    case Con{Mod{name, file, items}, rest}:
      do Result<&2, &2, Bad, List<&2, Row>>:
        rs : List<&2, Row> <- mod_rows(name, items)
        more : List<&2, Row> <- all_rows(rest)
        return List.append(&2, Row, rs, more)

# The Rust item names of all modules and the names of the `nat` module must
# be different (D7, as the emitter checks). A variant is not an item.
@unsafe def rust_dup(+seen: List<&2, String>, rows: List<&2, Row>) -> Result<&2, &2, Bad, Bool>:
  match rows:
    case Nil{}:
      Done{True{}}
    case Con{Row{module, RkVariant{}, rust, mech, pos}, rest}:
      rust_dup(seen, rest)
    case Con{Row{module, k, +rust, mech, pos}, rest}:
      Bool.pick(Result<&2, &2, Bad, Bool>, Et.has(seen, rust), Fail{Bad{module, pos, "name collision (D7): " ++ rust}},
        rust_dup(rust <> seen, rest))

# The mech names of all rows must be different, and different from the
# names that the importer writes itself (MechBool, CD6) or maps to the
# kernel (CD7).
@unsafe def mech_dup(+seen: List<&2, String>, rows: List<&2, Row>) -> Result<&2, &2, Bad, Bool>:
  match rows:
    case Nil{}:
      Done{True{}}
    case Con{Row{module, k, rust, +mech, pos}, rest}:
      Bool.pick(Result<&2, &2, Bad, Bool>, Et.has(seen, mech),
        Fail{Bad{module, pos, "mech name collision (CD5): `" ++ rust ++ "` gives `" ++ mech ++ "`"}},
        mech_dup(mech <> seen, rest))

def mech_reserved() -> List<&2, String>:
  ["MechBool", "mechFalse", "mechTrue", "Nat", "natAdd"]

# ---- scope ----

type Fam is Data:
  Fam{name: String, ctors: List<&2, String>}

# The visible names of one module: types (enums, unit structs, `Nat`),
# values (fns, constructor fns, `nat_small`, `nat_add`) and the families with
# their variants.
type Scope is Data:
  Scope{tys: List<&2, String>, fns: List<&2, String>, fams: List<&2, Fam>}

def empty() -> Scope:
  Scope{[], [], []}

# CD7: `nat_decimal` has no kernel form, so it is not visible.
def nat_scope(sc: Scope) -> Scope:
  match sc:
    case Scope{tys, fns, fams}:
      Scope{"Nat" <> tys, "nat_small" <> ("nat_add" <> fns), fams}

def tys_of(sc: Scope) -> List<&2, String>:
  match sc:
    case Scope{tys, fns, fams}:
      tys

def fns_of(sc: Scope) -> List<&2, String>:
  match sc:
    case Scope{tys, fns, fams}:
      fns

def fams_of(sc: Scope) -> List<&2, Fam>:
  match sc:
    case Scope{tys, fns, fams}:
      fams

def variant_names(vs: List<&2, R.Variant>) -> List<&2, String>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fields}, rest}:
      name <> variant_names(rest)

def param_names(ps: List<&2, R.Param>) -> List<&2, String>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      name <> param_names(rest)

def bind_names(bs: List<&2, R.Bind>) -> List<&2, String>:
  match bs:
    case Nil{}:
      []
    case Con{R.BVar{name}, rest}:
      name <> bind_names(rest)
    case Con{R.BSkip{}, rest}:
      bind_names(rest)

def item_scope(p: Placed, sc: Scope) -> Scope:
  match p sc:
    case Placed{pos, R.IUse{m}} s:
      s
    case Placed{pos, R.IEnum{+name, gs, cp, vs}} Scope{tys, fns, fams}:
      Scope{name <> tys, fns, Fam{name, variant_names(vs)} <> fams}
    case Placed{pos, R.IUnit{name, ctor}} Scope{tys, fns, fams}:
      Scope{name <> tys, ctor <> fns, fams}
    case Placed{pos, R.IFn{name, gs, ps, rt, body}} Scope{tys, fns, fams}:
      Scope{tys, name <> fns, fams}

def items_scope(ps: List<&2, Placed>, sc: Scope) -> Scope:
  match ps:
    case Nil{}:
      sc
    case Con{p, rest}:
      items_scope(rest, item_scope(p, sc))

def has_ctor(fams: List<&2, Fam>, +family: String, +ctor: String) -> Bool:
  match fams:
    case Nil{}:
      False{}
    case Con{Fam{name, ctors}, rest}:
      (String.eq(name, family) && Et.has(ctors, ctor)) || has_ctor(rest, family, ctor)

# ---- bodies ----

# `gens`: the generics of the item. `locals`: the parameters, closure
# parameters and pattern binders in scope; a local shadows an item.
type Job is Data:
  JTy{gens: List<&2, String>, t: R.Ty}
  JEx{gens: List<&2, String>, locals: List<&2, String>, e: R.Ex}
  JArm{gens: List<&2, String>, locals: List<&2, String>, arm: R.Arm<R.Ex>}

def tjobs(+gens: List<&2, String>, ts: List<&2, R.Ty>) -> List<&2, Job>:
  match ts:
    case Nil{}:
      []
    case Con{t, rest}:
      JTy{gens, t} <> tjobs(gens, rest)

def ejobs(+gens: List<&2, String>, +locals: List<&2, String>, es: List<&2, R.Ex>) -> List<&2, Job>:
  match es:
    case Nil{}:
      []
    case Con{e, rest}:
      JEx{gens, locals, e} <> ejobs(gens, locals, rest)

def arm_jobs(+gens: List<&2, String>, +locals: List<&2, String>, arms: List<&2, R.Arm<R.Ex>>) -> List<&2, Job>:
  match arms:
    case Nil{}:
      []
    case Con{a, rest}:
      JArm{gens, locals, a} <> arm_jobs(gens, locals, rest)

# The type of a let binder, when the Rust text gives one (E1).
def let_tys(t: Maybe<&2, R.Ty>) -> List<&2, R.Ty>:
  match t:
    case None{}:
      []
    case Some{x}:
      [x]

# The first name that does not resolve, as the text of a refusal.
@unsafe def walk(+sc: Scope, js: List<&2, Job>) -> Maybe<&2, String>:
  match js:
    case Nil{}:
      None{}
    case Con{JTy{gens, R.TBool{}}, rest}:
      walk(sc, rest)
    case Con{JTy{+gens, R.TCon{+name, args}}, rest}:
      Bool.pick(Maybe<&2, String>, Et.has(gens, name) || Et.has(tys_of(sc), name),
        walk(sc, List.append(&2, Job, tjobs(gens, args), rest)),
        Some{"unknown type `" ++ name ++ "`"})
    case Con{JTy{gens, R.TBox{inner}}, rest}:
      walk(sc, JTy{gens, inner} <> rest)
    case Con{JTy{gens, R.TRef{inner}}, rest}:
      walk(sc, JTy{gens, inner} <> rest)
    case Con{JTy{gens, R.TFn{args, ret}}, rest}:
      walk(sc, List.append(&2, Job, tjobs(gens, ret <> args), rest))
    case Con{JEx{gens, locals, R.XVar{+name}}, rest}:
      Bool.pick(Maybe<&2, String>, Et.has(locals, name) || Et.has(fns_of(sc), name), walk(sc, rest),
        Some{"unknown name `" ++ name ++ "`"})
    case Con{JEx{gens, locals, R.XInt{t}}, rest}:
      walk(sc, rest)
    case Con{JEx{gens, locals, R.XBool{v}}, rest}:
      walk(sc, rest)
    case Con{JEx{+gens, +locals, R.XCall{+func, targs, args}}, rest}:
      Bool.pick(Maybe<&2, String>, Et.has(locals, func) || Et.has(fns_of(sc), func),
        walk(sc, List.append(&2, Job, tjobs(gens, targs), List.append(&2, Job, ejobs(gens, locals, args), rest))),
        Some{"unknown function `" ++ func ++ "`"})
    case Con{JEx{+gens, +locals, R.XCtor{+family, +ctor, args}}, rest}:
      Bool.pick(Maybe<&2, String>, has_ctor(fams_of(sc), family, ctor),
        walk(sc, List.append(&2, Job, ejobs(gens, locals, args), rest)),
        Some{"unknown constructor `" ++ family ++ "::" ++ ctor ++ "`"})
    case Con{JEx{+gens, +locals, R.XMatch{scrut, arms}}, rest}:
      walk(sc, JEx{gens, locals, scrut} <> List.append(&2, Job, arm_jobs(gens, locals, arms), rest))
    case Con{JEx{+gens, +locals, R.XIf{cond, yes, no}}, rest}:
      walk(sc, JEx{gens, locals, cond} <> (JEx{gens, locals, yes} <> (JEx{gens, locals, no} <> rest)))
    case Con{JEx{+gens, locals, R.XClosure{+ps, body}}, rest}:
      walk(sc, List.append(&2, Job, tjobs(gens, Et.param_tys(ps)), JEx{gens, List.append(&2, String, param_names(ps), locals), body} <> rest))
    case Con{JEx{gens, locals, R.XBorrow{inner}}, rest}:
      walk(sc, JEx{gens, locals, inner} <> rest)
    case Con{JEx{gens, locals, R.XClone{inner}}, rest}:
      walk(sc, JEx{gens, locals, inner} <> rest)
    case Con{JEx{gens, locals, R.XBox{inner}}, rest}:
      walk(sc, JEx{gens, locals, inner} <> rest)
    case Con{JEx{+gens, +locals, R.XLet{+name, t, value, body}}, rest}:
      walk(sc, List.append(&2, Job, tjobs(gens, let_tys(t)), JEx{gens, locals, value} <> (JEx{gens, name <> locals, body} <> rest)))
    case Con{JArm{gens, locals, R.Arm{R.PCtor{+family, +ctor, binds}, body}}, rest}:
      Bool.pick(Maybe<&2, String>, has_ctor(fams_of(sc), family, ctor),
        walk(sc, JEx{gens, List.append(&2, String, bind_names(binds), locals), body} <> rest),
        Some{"unknown constructor `" ++ family ++ "::" ++ ctor ++ "`"})

def item_jobs(p: Placed) -> List<&2, Job>:
  match p:
    case Placed{pos, R.IUse{m}}:
      []
    case Placed{pos, R.IEnum{name, +gs, cp, vs}}:
      tjobs(gs, Ro.variant_tys(vs))
    case Placed{pos, R.IUnit{name, ctor}}:
      []
    case Placed{pos, R.IFn{name, +gs, +ps, ret, body}}:
      List.append(&2, Job, tjobs(gs, ret <> Et.param_tys(ps)), [JEx{gs, param_names(ps), body}])

def place_pos(p: Placed) -> A.Pos:
  match p:
    case Placed{pos, item}:
      pos

def problem_at(module: String, pos: A.Pos, m: Maybe<&2, String>, later: Result<&2, &2, Bad, Bool>) -> Result<&2, &2, Bad, Bool>:
  match m:
    case Some{what}:
      Fail{Bad{module, pos, what}}
    case None{}:
      later

# The text of a name problem after its first word: "unknown function `f`"
# gives "function `f`".
def after_space(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{+c, +rest}:
      Bool.pick(String, Ro.code_is(c, 32), rest, after_space(rest))

# A name that the module has, but not at this item or before it. The mech
# text keeps the item order, and a mech declaration sees only the earlier
# declarations and itself.
def early_at(module: String, pos: A.Pos, m: Maybe<&2, String>, later: Result<&2, &2, Bad, Bool>) -> Result<&2, &2, Bad, Bool>:
  match m:
    case Some{what}:
      Fail{Bad{module, pos, "use of the " ++ after_space(what) ++ " before its declaration (mutual recursion or a forward reference)"}}
    case None{}:
      later

# `sc`: the used modules and all items of the module. `seen`: the used
# modules and the items before `ps`.
@unsafe def check_items(+module: String, +sc: Scope, seen: Scope, ps: List<&2, Placed>) -> Result<&2, &2, Bad, Bool>:
  match ps:
    case Nil{}:
      Done{True{}}
    case Con{+p, rest}:
      +now = item_scope(p, seen)
      problem_at(module, place_pos(p), walk(sc, item_jobs(p)),
        early_at(module, place_pos(p), walk(now, item_jobs(p)), check_items(module, sc, now, rest)))

# ---- modules ----

type UseAt is Data:
  UseAt{pos: A.Pos, module: String}

def uses_of(ps: List<&2, Placed>) -> List<&2, UseAt>:
  match ps:
    case Nil{}:
      []
    case Con{Placed{pos, R.IUse{m}}, rest}:
      UseAt{pos, m} <> uses_of(rest)
    case Con{other, rest}:
      uses_of(rest)

def non_nat(us: List<&2, UseAt>) -> List<&2, UseAt>:
  match us:
    case Nil{}:
      []
    case Con{UseAt{pos, +m}, rest}:
      +tail = non_nat(rest)
      Bool.pick(List<&2, UseAt>, String.eq(m, "nat"), tail, UseAt{pos, m} <> tail)

def mod_names(ms: List<&2, Mod>) -> List<&2, String>:
  match ms:
    case Nil{}:
      []
    case Con{Mod{name, file, items}, rest}:
      name <> mod_names(rest)

def find_mod(ms: List<&2, Mod>, +name: String) -> Maybe<&2, Mod>:
  match ms:
    case Nil{}:
      None{}
    case Con{Mod{+n, file, items}, rest}:
      Bool.pick(Maybe<&2, Mod>, String.eq(n, name), Some{Mod{n, file, items}}, find_mod(rest, name))

def mod_scope(found: Maybe<&2, Mod>, sc: Scope) -> Scope:
  match found:
    case Some{Mod{name, file, items}}:
      items_scope(items, sc)
    case None{}:
      sc

def add_mod(+m: String, found: Maybe<&2, Mod>, +sc: Scope) -> Scope:
  Bool.pick(Scope, String.eq(m, "nat"), nat_scope(sc), mod_scope(found, sc))

# The `use` lines give the visible modules (each was checked by `known_uses`).
def use_scope(+ms: List<&2, Mod>, us: List<&2, UseAt>, sc: Scope) -> Scope:
  match us:
    case Nil{}:
      sc
    case Con{UseAt{pos, +m}, rest}:
      use_scope(ms, rest, add_mod(m, find_mod(ms, m), sc))

@unsafe def check_mods(+all: List<&2, Mod>, ms: List<&2, Mod>) -> Result<&2, &2, Bad, Bool>:
  match ms:
    case Nil{}:
      Done{True{}}
    case Con{Mod{+name, file, +items}, rest}:
      do Result<&2, &2, Bad, Bool>:
        ok : Bool <- check_items(name, use_scope(all, uses_of(items), items_scope(items, empty())), use_scope(all, uses_of(items), empty()), items)
        more : Bool <- check_mods(all, rest)
        return more

def known_uses(+names: List<&2, String>, +nat: Bool, +module: String, us: List<&2, UseAt>) -> Result<&2, &2, Bad, Bool>:
  match us:
    case Nil{}:
      Done{True{}}
    case Con{UseAt{pos, +m}, rest}:
      Bool.pick(Result<&2, &2, Bad, Bool>, Et.has(names, m) || (String.eq(m, "nat") && nat), known_uses(names, nat, module, rest),
        Fail{Bad{module, pos, "`use crate::" ++ m ++ "::*;` of a module that lib.rs does not declare"}})

@unsafe def known_mods(+names: List<&2, String>, +nat: Bool, ms: List<&2, Mod>) -> Result<&2, &2, Bad, Bool>:
  match ms:
    case Nil{}:
      Done{True{}}
    case Con{Mod{name, file, items}, rest}:
      do Result<&2, &2, Bad, Bool>:
        ok : Bool <- known_uses(names, nat, name, uses_of(items))
        more : Bool <- known_mods(names, nat, rest)
        return more

# ---- order (CD3) ----

# A module and its `use` lines other than `nat`.
type Dep is Data:
  Dep{name: String, uses: List<&2, UseAt>}

def deps_of(ms: List<&2, Mod>) -> List<&2, Dep>:
  match ms:
    case Nil{}:
      []
    case Con{Mod{name, file, items}, rest}:
      Dep{name, non_nat(uses_of(items))} <> deps_of(rest)

def all_in(+acc: List<&2, String>, us: List<&2, UseAt>) -> Bool:
  match us:
    case Nil{}:
      True{}
    case Con{UseAt{pos, m}, rest}:
      Et.has(acc, m) && all_in(acc, rest)

# The modules whose `use` lines name only modules in `acc`.
def ready(+acc: List<&2, String>, ds: List<&2, Dep>) -> List<&2, String>:
  match ds:
    case Nil{}:
      []
    case Con{Dep{name, us}, rest}:
      +tail = ready(acc, rest)
      Bool.pick(List<&2, String>, all_in(acc, us), name <> tail, tail)

def without(ds: List<&2, Dep>, +r: String) -> List<&2, Dep>:
  match ds:
    case Nil{}:
      []
    case Con{Dep{+name, us}, rest}:
      +tail = without(rest, r)
      Bool.pick(List<&2, Dep>, String.eq(name, r), tail, Dep{name, us} <> tail)

def stuck(+acc: List<&2, String>, +module: String, us: List<&2, UseAt>) -> Bad:
  match us:
    case Nil{}:
      Bad{module, A.no_pos(), "module cycle (CD3)"}
    case Con{UseAt{pos, +m}, rest}:
      Bool.pick(Bad, Et.has(acc, m), stuck(acc, module, rest), Bad{module, pos, "`use crate::" ++ m ++ "::*;` in a module cycle (CD3)"})

def finish(pending: List<&2, Dep>, +acc: List<&2, String>) -> Result<&2, &2, Bad, List<&2, String>>:
  match pending:
    case Nil{}:
      Done{List.reverse(&2, String, acc)}
    case Con{Dep{name, us}, rest}:
      Fail{stuck(acc, name, us)}

def push(+r: String, acc: List<&2, String>) -> List<&2, String>:
  r <> acc

# Kahn's order: each step takes the least ready module by byte order (the
# order of lib.rs). `rs`: the ready modules, sorted. No ready module and a
# module left: a cycle.
@unsafe def order(+pending: List<&2, Dep>, +acc: List<&2, String>, rs: List<&2, String>) -> Result<&2, &2, Bad, List<&2, String>>:
  match rs:
    case Nil{}:
      finish(pending, acc)
    case Con{+r, more}:
      +left = without(pending, r)
      +now = push(r, acc)
      order(left, now, Ro.sorted(ready(now, left)))

def start(+ds: List<&2, Dep>) -> Result<&2, &2, Bad, List<&2, String>>:
  order(ds, [], Ro.sorted(ready([], ds)))

def found_list(m: Maybe<&2, Mod>) -> List<&2, Mod>:
  match m:
    case Some{x}:
      [x]
    case None{}:
      []

def in_order(names: List<&2, String>, +ms: List<&2, Mod>) -> List<&2, Mod>:
  match names:
    case Nil{}:
      []
    case Con{n, rest}:
      List.append(&2, Mod, found_list(find_mod(ms, n)), in_order(rest, ms))

# ---- crate ----

def collided(+ms: List<&2, Mod>, +rows: List<&2, Row>) -> Result<&2, &2, Bad, Out>:
  do Result<&2, &2, Bad, Out>:
    a : Bool <- rust_dup(Ro.nat_names(), rows)
    b : Bool <- mech_dup(mech_reserved(), rows)
    c : Bool <- check_mods(ms, ms)
    return Out{ms, rows}

def ordered(+ms: List<&2, Mod>) -> Result<&2, &2, Bad, Out>:
  do Result<&2, &2, Bad, Out>:
    rows : List<&2, Row> <- all_rows(ms)
    out : Out <- collided(ms, rows)
    return out

def checked(+nat: Bool, +ms: List<&2, Mod>) -> Result<&2, &2, Bad, Out>:
  do Result<&2, &2, Bad, Out>:
    ok : Bool <- known_mods(mod_names(ms), nat, ms)
    names : List<&2, String> <- start(deps_of(ms))
    out : Out <- ordered(in_order(names, ms))
    return out

def crate_of(+names: List<&2, String>, +srcs: List<&2, Src>) -> Result<&2, &2, Bad, Out>:
  do Result<&2, &2, Bad, Out>:
    nat : Bool <- nat_need(Et.has(names, "nat"), text_of(srcs, "nat"))
    ms : List<&2, Mod> <- load(drop_nat(names), srcs)
    out : Out <- checked(nat, ms)
    return out

# `lib`: the text of src/lib.rs. `srcs`: the text of each module file.
def resolve(lib: String, +srcs: List<&2, Src>) -> Result<&2, &2, Bad, Out>:
  do Result<&2, &2, Bad, Out>:
    names : List<&2, String> <- lib_mods(Parser.file(lib))
    out : Out <- crate_of(names, srcs)
    return out

# ---- text ----

def kind_text(k: Rk) -> String:
  match k:
    case RkEnum{}:
      "enum"
    case RkVariant{}:
      "variant"
    case RkStruct{}:
      "struct"
    case RkCtor{}:
      "ctor"
    case RkFn{}:
      "fn"

def rows_text(rows: List<&2, Row>) -> String:
  match rows:
    case Nil{}:
      ""
    case Con{Row{module, k, rust, mech, pos}, rest}:
      module ++ " " ++ kind_text(k) ++ " " ++ rust ++ " -> " ++ mech ++ "\n" ++ rows_text(rest)

def manifest_text(ms: List<&2, Mod>) -> String:
  match ms:
    case Nil{}:
      ""
    case Con{Mod{name, file, items}, rest}:
      "manifest " ++ name ++ " " ++ file ++ "\n" ++ manifest_text(rest)

# One line for each row, `<module> <kind> <rust name> -> <mech name>`, then
# one `manifest <module> <file>` line for each module, in manifest order.
def table_text(o: Out) -> String:
  match o:
    case Out{ms, rows}:
      rows_text(rows) ++ manifest_text(ms)
