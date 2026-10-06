import Base
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as Pos
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Big
import ../kernel/value.bend as V
import ../kernel/eval.bend as Eval
import ../kernel/error.bend as Error
import ../surface/syntax.bend as Syntax
import ../surface/parser.bend as Parser
import ../rust/rir.bend as R
import ../rust/erase_typed.bend as Et
import ../cli/rust_out.bend as Ro

# The DIFF-EXEC oracle. A probe is a closed definition of the last input file.
# `values`: the kernel value of each probe, in the Rust `{:?}` form.
# `probes`: the generated probes of each first-order runtime function.
# `bin`: the text of `src/bin/diff_exec.rs`, one `println!` for each probe.

# The Rust `{:?}` text of a kernel value. `whnf` forces the head only, so each
# kept field is forced in turn. An erased field is not forced and not printed.
# A def can call only itself and the defs above it, so the printer is one loop
# on a work list. `IVal`: a value that is not forced yet. `IText`: final text.
type Item is Data:
  IVal{v: V.T}
  IText{s: String}

# One field in front of the kept fields `tail`. `None` is an erased field.
def keep_one(t: Maybe<&2, R.Ty>, a: V.T, tail: List<&2, V.T>) -> List<&2, V.T>:
  match t:
    case None{}:
      tail
    case Some{ty}:
      a <> tail

# The kept fields of a constructor value. `None` in `tys` is an erased field.
def kept(tys: List<&2, Maybe<&2, R.Ty>>, args: List<&2, V.T>) -> List<&2, V.T>:
  match tys:
    case Nil{}:
      []
    case Con{t, rest}:
      match args:
        case Nil{}:
          []
        case Con{a, more}:
          keep_one(t, a, kept(rest, more))

def more_items(vals: List<&2, V.T>, rest: List<&2, Item>) -> List<&2, Item>:
  match vals:
    case Nil{}:
      IText{")"} <> rest
    case Con{v, more}:
      IText{", "} <> (IVal{v} <> more_items(more, rest))

def field_items(vals: List<&2, V.T>, rest: List<&2, Item>) -> List<&2, Item>:
  match vals:
    case Nil{}:
      rest
    case Con{v, more}:
      IText{"("} <> (IVal{v} <> more_items(more, rest))

def ctor_items(c: String, args: List<&2, V.T>, tys: List<&2, Maybe<&2, R.Ty>>, cls: Et.Class, rest: List<&2, Item>) -> Result<&2, &2, String, List<&2, Item>>:
  match cls:
    case Et.CBool{}:
      Done{IText{Bool.pick(String, String.eq(c, "mechTrue"), "true", "false")} <> rest}
    case Et.CData{R.IUnit{name, ctor}}:
      Done{IText{name} <> rest}
    case Et.CData{R.IEnum{name, generics, copy, variants}}:
      Done{IText{Et.upper_first(c)} <> field_items(kept(tys, args), rest)}
    case other:
      Fail{"a constructor of a family that is not runtime data: " ++ c}

def whnf_items(+env: Et.Env, r: Result<&2, &2, Error.T, V.T>, rest: List<&2, Item>) -> Result<&2, &2, String, List<&2, Item>>:
  match r:
    case Fail{e}:
      Fail{Error.to_string(e)}
    case Done{V.VLit{Literal.LInt{n}}}:
      Done{IText{Big.to_string(n)} <> rest}
    case Done{V.VIn{Shape.SMu{+fam, idx}, V.VACtor{+c}, args}}:
      ctor_items(c, args, Et.ctor_tys(env, Global.find_family(fam, Et.globals_of(env)), c),
        Et.family_or_skip(env, Global.find_family(fam, Et.globals_of(env))), rest)
    case other:
      Fail{"the value is not a constructor and not an integer"}

def items_of(+env: Et.Env, v: V.T, rest: List<&2, Item>) -> Result<&2, &2, String, List<&2, Item>>:
  whnf_items(env, Eval.whnf(Et.globals_of(env), v), rest)

# `@unsafe`: a forced value puts its fields on the work list, so the self-call
# is not structural. The loop stops because a kernel value is a finite tree.
@unsafe def render(+env: Et.Env, todo: Result<&2, &2, String, List<&2, Item>>, out: String) -> Result<&2, &2, String, String>:
  match todo:
    case Fail{why}:
      Fail{why}
    case Done{items}:
      match items:
        case Nil{}:
          Done{out}
        case Con{item, rest}:
          match item:
            case IText{s}:
              render(env, Done{rest}, out ++ s)
            case IVal{v}:
              render(env, items_of(env, v, rest), out)

def shown(+env: Et.Env, r: Result<&2, &2, Error.T, V.T>) -> Result<&2, &2, String, String>:
  match r:
    case Fail{e}:
      Fail{Error.to_string(e)}
    case Done{v}:
      render(env, items_of(env, v, []), "")

def eval_show(+env: Et.Env, name: String) -> Result<&2, &2, String, String>:
  shown(env, Eval.eval(Et.globals_of(env), [], Term.Global{name}))

def value_line(name: String, r: Result<&2, &2, String, String>) -> String:
  match r:
    case Done{text}:
      name ++ " = " ++ text ++ "\n"
    case Fail{why}:
      name ++ " ! " ++ why ++ "\n"

def value_lines(+env: Et.Env, names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{+name, rest}:
      value_line(name, eval_show(env, name)) ++ value_lines(env, rest)

# Probe generation. `Combo`: a name suffix and the argument text of one call.
type Combo is Data:
  Combo{label: String, args: String}

def nat_vals() -> List<&2, Combo>:
  [Combo{"Zero", "mechZero"}, Combo{"One", "(mechSucc mechZero)"}, Combo{"Two", "(mechSucc (mechSucc mechZero))"}]

def bool_vals() -> List<&2, Combo>:
  [Combo{"False", "mechFalse"}, Combo{"True", "mechTrue"}]

def fieldless(ctors: List<&2, Pos.Ctor>) -> Bool:
  match ctors:
    case Nil{}:
      True{}
    case Con{Pos.Ctor{name, args, res_idx, full_arity, self_rec}, rest}:
      match args:
        case Nil{}:
          fieldless(rest)
        case Con{a, more}:
          False{}

def ctor_vals(ctors: List<&2, Pos.Ctor>) -> List<&2, Combo>:
  match ctors:
    case Nil{}:
      []
    case Con{Pos.Ctor{+name, args, res_idx, full_arity, self_rec}, rest}:
      Combo{Et.upper_first(name), name} <> ctor_vals(rest)

# A plain family has no parameter and no index: its name is a closed type.
def plain(fam: Maybe<&2, Pos.Family>) -> Maybe<&2, List<&2, Pos.Ctor>>:
  match fam:
    case None{}:
      None{}
    case Some{Pos.Family{name, params, indices, level, status, ctors, positive}}:
      match params:
        case Con{p, more}:
          None{}
        case Nil{}:
          match indices:
            case Con{i, more}:
              None{}
            case Nil{}:
              Some{ctors}

def family_vals(ctors: Maybe<&2, List<&2, Pos.Ctor>>) -> List<&2, Combo>:
  match ctors:
    case None{}:
      []
    case Some{+cs}:
      Bool.pick(List<&2, Combo>, fieldless(cs), ctor_vals(cs), [])

# The small inputs of one parameter type. No input = no generated probe.
def ty_vals(env: Et.Env, t: R.Ty) -> List<&2, Combo>:
  match t:
    case R.TBool{}:
      bool_vals()
    case R.TRef{inner}:
      ty_vals(env, inner)
    case R.TCon{+name, args}:
      match args:
        case Nil{}:
          Bool.pick(List<&2, Combo>, String.eq(name, "MechNat"), nat_vals(),
            family_vals(plain(Global.find_family(name, Et.globals_of(env)))))
        case Con{a, more}:
          []
    case other:
      []

def plain_name(name: String, ctors: Maybe<&2, List<&2, Pos.Ctor>>) -> String:
  match ctors:
    case None{}:
      ""
    case Some{cs}:
      name

# The mech text of a closed result type, or "" for a type outside the rule.
def ret_text(env: Et.Env, t: R.Ty) -> String:
  match t:
    case R.TBool{}:
      "MechBool"
    case R.TCon{+name, args}:
      match args:
        case Nil{}:
          Bool.pick(String, String.eq(name, "Nat"), "Nat",
            plain_name(name, plain(Global.find_family(name, Et.globals_of(env)))))
        case Con{a, more}:
          ""
    case other:
      ""

def glue(c: Combo, v: Combo) -> Combo:
  match c:
    case Combo{label, args}:
      match v:
        case Combo{vlabel, vargs}:
          Combo{label ++ vlabel, args ++ " " ++ vargs}

def cross_one(+c: Combo, vs: List<&2, Combo>, tail: List<&2, Combo>) -> List<&2, Combo>:
  match vs:
    case Nil{}:
      tail
    case Con{v, rest}:
      glue(c, v) <> cross_one(c, rest, tail)

def cross(cs: List<&2, Combo>, +vs: List<&2, Combo>) -> List<&2, Combo>:
  match cs:
    case Nil{}:
      []
    case Con{c, rest}:
      cross_one(c, vs, cross(rest, vs))

def combos(+env: Et.Env, ps: List<&2, R.Param>, acc: List<&2, Combo>) -> List<&2, Combo>:
  match ps:
    case Nil{}:
      acc
    case Con{R.Param{name, ty}, rest}:
      combos(env, rest, cross(acc, ty_vals(env, ty)))

def gen_lines(+name: String, +ret: String, cs: List<&2, Combo>) -> String:
  match cs:
    case Nil{}:
      ""
    case Con{Combo{label, args}, rest}:
      "def probe" ++ Et.upper_first(name) ++ label ++ " : " ++ ret ++ " := " ++ name ++ args ++ "\n" ++ gen_lines(name, ret, rest)

def gen_fn(name: String, generics: List<&2, String>, +ret: String, cs: List<&2, Combo>) -> String:
  match generics:
    case Nil{}:
      Bool.pick(String, String.eq(ret, ""), "", gen_lines(name, ret, cs))
    case Con{g, more}:
      ""

def gen_class(+env: Et.Env, name: String, cls: Et.Class) -> String:
  match cls:
    case Et.CFn{rust, generics, params, ret}:
      gen_fn(name, generics, ret_text(env, ret), combos(env, params, [Combo{"", ""}]))
    case other:
      ""

def gen_file(+env: Et.Env, names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{+name, rest}:
      gen_class(env, name, Et.name_class(env, name)) ++ gen_file(env, rest)

def bin_lines(names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{name, rest}:
      "    println!(\"{:?}\", " ++ Et.fn_name(name) ++ "());\n" ++ bin_lines(rest)

def bin_text(module: String, names: List<&2, String>) -> String:
  "//! DIFF-EXEC: prints the value of each probe, one line for each probe.\n\nuse mech_out::" ++ module ++ "::*;\n\nfn main() {\n" ++ bin_lines(names) ++ "}\n"

# The files in order. `last`: the declared names of the last file. `gen`: the
# generated probes of all files.
type St is Data:
  St{env: Et.Env, module: String, last: List<&2, String>, gen: String, bad: List<&2, String>}

def names_of(r: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> List<&2, String>:
  match r:
    case Fail{e}:
      []
    case Done{decls}:
      Et.decl_names(decls)

def after(es: Et.Es, module: String, +names: List<&2, String>, gen: String) -> St:
  match es:
    case Et.Es{+env, items, bad}:
      St{env, module, names, gen ++ gen_file(env, names), bad}

def check_src(env: Et.Env, gen: String, src: Ro.Src) -> St:
  match src:
    case Ro.Src{name, +text}:
      after(Et.emit_source(env, text), Et.stem(name), names_of(Parser.program(text)), gen)

# A file after a refused file is not read: its globals are not there.
def check_step(st: St, src: Ro.Src) -> St:
  match st:
    case St{env, module, last, gen, bad}:
      match bad:
        case Nil{}:
          check_src(env, gen, src)
        case Con{why, more}:
          St{env, module, last, gen, why <> more}

def check_all(srcs: List<&2, Ro.Src>, st: St) -> St:
  match srcs:
    case Nil{}:
      st
    case Con{src, rest}:
      check_all(rest, check_step(st, src))

type Md is Data:
  MValues{}
  MProbes{}
  MBin{}

def text_of(md: Md, +env: Et.Env, module: String, last: List<&2, String>, gen: String) -> String:
  match md:
    case MValues{}:
      value_lines(env, last)
    case MProbes{}:
      gen
    case MBin{}:
      bin_text(module, last)

def result_of(md: Md, st: St) -> Result<&2, &2, String, String>:
  match st:
    case St{env, module, last, gen, bad}:
      match bad:
        case Nil{}:
          Done{text_of(md, env, module, last, gen)}
        case Con{why, more}:
          Fail{Et.join("\n", List.reverse(&2, String, why <> more)) ++ "\n"}

def finish(r: Result<&2, &2, String, String>) -> IO(Unit):
  match r:
    case Done{text}:
      IO.write(text)
    case Fail{why}:
      IO.die(Unit, 65, why)

def read_all(paths: List<String>) -> IO(List<&2, Ro.Src>):
  match paths:
    case Nil{}:
      IO.pure(List<&2, Ro.Src>, [])
    case Con{+path, rest}:
      do IO<List<&2, Ro.Src>>:
        text : String <- Ro.read_file(path)
        more : List<&2, Ro.Src> <- read_all(rest)
        IO.pure(List<&2, Ro.Src>, Ro.Src{path, text} <> more)

def run(md: Md, paths: List<String>) -> IO(Unit):
  do IO<Unit>:
    srcs : List<&2, Ro.Src> <- read_all(paths)
    finish(result_of(md, check_all(srcs, St{Et.Env{Global.initial(), []}, "", [], "", []})))

def values(paths: List<String>) -> IO(Unit):
  run(MValues{}, paths)

def probes(paths: List<String>) -> IO(Unit):
  run(MProbes{}, paths)

def bin(paths: List<String>) -> IO(Unit):
  run(MBin{}, paths)
