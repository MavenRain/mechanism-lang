import Base
import ../kernel/common.bend as C
import ../kernel/term.bend as Term
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Quantity
import ../kernel/level.bend as Level
import ../kernel/global.bend as Global
import ../kernel/positivity.bend as Pos
import ../kernel/literal.bend as Literal
import ../kernel/bignum.bend as Bignum
import ../kernel/pp.bend as Pp
import ../kernel/budget.bend as Budget
import ../kernel/error.bend as Error
import ../surface/syntax.bend as Syntax
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab
import ./rir.bend as R

# Typed erasure for `mech rust-out`, part 1: declarations. Each checked
# declaration gets a class: runtime function, runtime family, proposition,
# type-valued definition, absurd, or refused with a reason. The sort of a
# type is read from its shape (a family level, a universe at the end of a
# telescope, the kind of a bound variable); each other shape is refused.
# Bend 2 has no mutual recursion, so each recursive walker takes a list.

# Names (D7).

def is_upper(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 65) && U32.is_le(Char.to_u32(c), 90)

def is_lower(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 97) && U32.is_le(Char.to_u32(c), 122)

def lower(+c: Char) -> Char:
  Bool.pick(Char, is_upper(c), Char.from_u32(U32.add(Char.to_u32(c), 32)), c)

def upper(+c: Char) -> Char:
  Bool.pick(Char, is_lower(c), Char.from_u32(U32.sub(Char.to_u32(c), 32)), c)

def snake_rest(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{+c, rest}:
      +tail = snake_rest(rest)
      Bool.pick(String, is_upper(c), SCon{'_', SCon{lower(c), tail}}, SCon{c, tail})

def snake(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      SCon{lower(c), snake_rest(rest)}

def upper_first(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      SCon{upper(c), rest}

def keywords() -> List<&2, String>:
  ["as", "async", "await", "box", "break", "const", "continue", "crate", "do", "dyn", "else", "enum", "extern",
    "false", "final", "fn", "for", "if", "impl", "in", "let", "loop", "macro", "match", "mod", "move", "mut",
    "override", "priv", "pub", "ref", "return", "self", "static", "struct", "super", "trait", "true", "try", "type",
    "typeof", "unsafe", "unsized", "use", "virtual", "where", "while", "yield"]

def has(xs: List<&2, String>, +name: String) -> Bool:
  match xs:
    case Nil{}:
      False{}
    case Con{x, rest}:
      String.eq(x, name) || has(rest, name)

def rust_name(+s: String) -> String:
  Bool.pick(String, has(keywords(), s), s ++ "_", s)

def fn_name(s: String) -> String:
  rust_name(snake(s))

# The inverse of the name map. The importer reads a Rust name with it. The
# emitter refuses a mech name that does not come back from its Rust name.

def is_under(+c: Char) -> Bool:
  U32.is_ge(Char.to_u32(c), 95) && U32.is_le(Char.to_u32(c), 95)

# `r` is the camelCase form of the text after `c`. Its first character is
# lower case exactly when the first character after `c` is, so the test of a
# `_` before a lower-case letter can use it.
def camel_step(+c: Char, r: String) -> String:
  match r:
    case SNil{}:
      SCon{c, SNil{}}
    case SCon{+h, +t}:
      Bool.pick(String, is_under(c) && is_lower(h), SCon{upper(h), t}, SCon{c, SCon{h, t}})

def camel_rest(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      camel_step(c, camel_rest(rest))

# `snake_case` -> `camelCase`: a `_` before a lower-case letter goes, and the
# letter becomes upper case. The first character stays.
def camel(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      SCon{c, camel_rest(rest)}

def lower_first(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      SCon{lower(c), rest}

def drop_head(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{c, rest}:
      rest

# The inverse of `rust_name`: a keyword with `_` at its end loses the `_`.
def unkw(+s: String) -> String:
  +base = String.reverse(drop_head(String.reverse(s)))
  Bool.pick(String, has(keywords(), base) && String.eq(base ++ "_", s), base, s)

# The inverse of `fn_name`.
def fn_back(rust: String) -> String:
  camel(unkw(rust))

def back_text(rust: String, back: String) -> String:
  "a name that does not come back from the Rust name `" ++ rust ++ "`: the importer gives `" ++ back ++ "` (D7)"

def first(xs: List<&2, String>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      x

def no_names(xs: List<&2, String>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{x, rest}:
      False{}

def same_count(xs: List<&2, String>, ys: List<&2, String>) -> Bool:
  match xs:
    case Nil{}:
      no_names(ys)
    case Con{x, rest}:
      match ys:
        case Nil{}:
          False{}
        case Con{y, more}:
          same_count(rest, more)

# Kinds and sorts.

# The kind of a bound variable: a generic, a proposition or predicate, or a
# value.
type Knd is Data:
  KGen{name: String}
  KProp{}
  KVal{}

# The end of a telescope: `Prop`, `Type n`, or no universe.
type End is Data:
  EndProp{}
  EndType{}
  EndNone{}

# The erasure of a type.
type Er is Data:
  EData{ty: R.Ty}
  EProp{}
  ENo{why: String}

# `copies`: the runtime families that derive `Copy`, in declaration order.
type Env is Data:
  Env{globals: Global.T, copies: List<&2, String>}

def globals_of(env: Env) -> Global.T:
  match env:
    case Env{globals, copies}:
      globals

def copies_of(env: Env) -> List<&2, String>:
  match env:
    case Env{globals, copies}:
      copies

@unsafe def end_of(t: Term.T) -> End:
  match t:
    case Term.Ran{Shape.SPi{q, x, dom}, body}:
      end_of(body)
    case Term.Univ{level}:
      Bool.pick(End, Level.always_zero(level), EndProp{}, EndType{})
    case other:
      EndNone{}

def kind_of(e: End, name: String) -> Knd:
  match e:
    case EndProp{}:
      KProp{}
    case EndType{}:
      KGen{name}
    case EndNone{}:
      KVal{}

def kind_at(ctx: List<&2, Knd>, index: Nat) -> Knd:
  match ctx:
    case Nil{}:
      KVal{}
    case Con{k, rest}:
      match index:
        case 0n:
          k
        case 1n+p:
          kind_at(rest, p)

def is_zero(q: Quantity.T) -> Bool:
  match q:
    case Quantity.QZero{}:
      True{}
    case Quantity.QOne{}:
      False{}
    case Quantity.QMany{}:
      False{}

def is_one(q: Quantity.T) -> Bool:
  match q:
    case Quantity.QZero{}:
      False{}
    case Quantity.QOne{}:
      True{}
    case Quantity.QMany{}:
      False{}

# Type erasure.

def one_er(xs: List<&2, Er>) -> Er:
  match xs:
    case Nil{}:
      ENo{"no type"}
    case Con{x, rest}:
      x

def leg_bodies(legs: List<&2, Term.Leg<Term.T>>) -> List<&2, Term.T>:
  match legs:
    case Nil{}:
      []
    case Con{Term.Leg{binders, body}, rest}:
      body <> leg_bodies(rest)

def push_ty(t: R.Ty, r: Result<&2, &2, String, List<&2, R.Ty>>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  match r:
    case Done{ts}:
      Done{t <> ts}
    case Fail{why}:
      Fail{why}

# The arguments of a family that stay: one for each `Type`-kinded parameter.
def keep_arg(e: End, a: Er, tail: Result<&2, &2, String, List<&2, R.Ty>>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  match e:
    case EndType{}:
      match a:
        case EData{t}:
          push_ty(t, tail)
        case EProp{}:
          Fail{"a proposition as a type argument"}
        case ENo{why}:
          Fail{why}
    case EndProp{}:
      tail
    case EndNone{}:
      tail

def keep_args(params: List<&2, Pos.Binder>, args: List<&2, Er>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  match params:
    case Nil{}:
      Done{[]}
    case Con{Pos.Binder{q, name, ty}, prest}:
      match args:
        case Nil{}:
          Fail{"a family with too few arguments"}
        case Con{a, arest}:
          keep_arg(end_of(ty), a, keep_args(prest, arest))

def con_ty(name: String, r: Result<&2, &2, String, List<&2, R.Ty>>) -> Er:
  match r:
    case Done{ts}:
      EData{R.TCon{name, ts}}
    case Fail{why}:
      ENo{why}

# `MechBool` is `bool` by name (prelude contract).
def family_ty(+name: String, fam: Maybe<&2, Pos.Family>, args: List<&2, Er>) -> Er:
  match fam:
    case None{}:
      ENo{"an unknown family: " ++ name}
    case Some{Pos.Family{fname, params, indices, level, status, ctors, positive}}:
      Bool.pick(Er, Level.always_zero(level), EProp{},
        Bool.pick(Er, String.eq(name, "MechBool"), EData{R.TBool{}}, con_ty(name, keep_args(params, args))))

def global_end(+name: String, e: End) -> Er:
  match e:
    case EndProp{}:
      EProp{}
    case EndType{}:
      Bool.pick(Er, String.eq(name, "Nat"), EData{R.TCon{"Nat", []}},
        ENo{"a type-valued definition in a runtime type (D3): " ++ name})
    case EndNone{}:
      ENo{"a value in a type: " ++ name}

def global_ty(name: String, entry: Maybe<&2, Global.Entry>) -> Er:
  match entry:
    case None{}:
      ENo{"an unknown name in a type: " ++ name}
    case Some{e}:
      global_end(name, end_of(Global.entry_ty(e)))

def var_ty(k: Knd) -> Er:
  match k:
    case KGen{name}:
      EData{R.TCon{name, []}}
    case KProp{}:
      EProp{}
    case KVal{}:
      ENo{"a value in a type"}

def own_or_ref(by_value: Bool, +t: R.Ty) -> R.Ty:
  Bool.pick(R.Ty, by_value, t, R.TRef{t})

# Ownership of a binder (D5): a function is `&impl Fn`, a Copy value or a
# quantity-1 value is owned, each other value is a borrow.
def mode(copies: List<&2, String>, q: Quantity.T, t: R.Ty) -> R.Ty:
  match t:
    case R.TBool{}:
      R.TBool{}
    case R.TCon{+name, args}:
      own_or_ref(is_one(q) || has(copies, name), R.TCon{name, args})
    case R.TBox{inner}:
      R.TBox{inner}
    case R.TRef{inner}:
      R.TRef{inner}
    case R.TFn{args, ret}:
      R.TRef{R.TFn{args, ret}}

def dropped(ret: R.Ty) -> Er:
  match ret:
    case R.TFn{args, r}:
      EData{R.TFn{args, r}}
    case other:
      ENo{"a function type with erased arguments only"}

def kept(p: R.Ty, ret: R.Ty) -> Er:
  match ret:
    case R.TFn{args, r}:
      EData{R.TFn{p <> args, r}}
    case other:
      EData{R.TFn{[p], other}}

# The erasure of `(q x : A) -> B` as the type of a value.
def fn_er(copies: List<&2, String>, +q: Quantity.T, b: Er, e: End, a: Er) -> Er:
  match b:
    case EProp{}:
      EProp{}
    case ENo{why}:
      ENo{why}
    case EData{+ret}:
      match e:
        case EndType{}:
          ENo{"a polymorphic function type"}
        case EndProp{}:
          dropped(ret)
        case EndNone{}:
          match a:
            case EProp{}:
              dropped(ret)
            case ENo{why}:
              Bool.pick(Er, is_zero(q), dropped(ret), ENo{why})
            case EData{t}:
              Bool.pick(Er, is_zero(q), dropped(ret), kept(mode(copies, q, t), ret))

@unsafe def ers(+env: Env, +ctx: List<&2, Knd>, ts: List<&2, Term.T>) -> List<&2, Er>:
  match ts:
    case Nil{}:
      []
    case Con{t, rest}:
      match t:
        case Term.Var{index}:
          var_ty(kind_at(ctx, index)) <> ers(env, ctx, rest)
        case Term.Lan{Shape.SMu{+name, idx}, Term.Sec{s, legs}}:
          family_ty(name, Global.find_family(name, globals_of(env)), ers(env, ctx, leg_bodies(legs))) <> ers(env, ctx, rest)
        case Term.Ran{Shape.SPi{q, x, +dom}, body}:
          fn_er(copies_of(env), q, one_er(ers(env, kind_of(end_of(dom), x) <> ctx, [body])), end_of(dom),
            one_er(ers(env, ctx, [dom]))) <> ers(env, ctx, rest)
        case Term.Out{Shape.SPi{q, x, dom}, addr, head}:
          ers(env, ctx, head <> rest)
        case Term.Global{+name}:
          global_ty(name, Global.find(name, globals_of(env))) <> ers(env, ctx, rest)
        case Term.Ann{body, ty}:
          ers(env, ctx, body <> rest)
        case other:
          ENo{"a type shape outside the M0 fragment"} <> ers(env, ctx, rest)

# The erasure of the result type of a telescope.
@unsafe def result_of(env: Env, ctx: List<&2, Knd>, ty: Term.T) -> Er:
  match ty:
    case Term.Ran{Shape.SPi{q, x, dom}, rest}:
      result_of(env, kind_of(end_of(dom), x) <> ctx, rest)
    case other:
      one_er(ers(env, ctx, [other]))

# Binders.

type Bd is Data:
  BGen{}
  BDrop{}
  BParam{ty: R.Ty}
  BNo{why: String}

def binder_er(q: Quantity.T, e: End, a: Er) -> Bd:
  match e:
    case EndType{}:
      Bool.pick(Bd, is_zero(q), BGen{}, BNo{"a type argument with a runtime quantity"})
    case EndProp{}:
      BDrop{}
    case EndNone{}:
      match a:
        case EProp{}:
          BDrop{}
        case ENo{why}:
          Bool.pick(Bd, is_zero(q), BDrop{}, BNo{why})
        case EData{t}:
          Bool.pick(Bd, is_zero(q), BDrop{}, BParam{t})

# The name of the lambda binder at the head of a body, and the body below it.
@unsafe def peel_name(body: Term.T, fallback: String) -> String:
  match body:
    case Term.Sec{Shape.SPi{q, x, dom}, legs}:
      x
    case Term.Ann{inner, ty}:
      peel_name(inner, fallback)
    case other:
      fallback

@unsafe def peel_body(body: Term.T) -> Term.T:
  match body:
    case Term.Sec{Shape.SPi{q, x, dom}, Con{Term.Leg{binders, inner}, rest}}:
      inner
    case Term.Ann{inner, ty}:
      peel_body(inner)
    case other:
      other

@unsafe def mentions(+g: String, ts: List<&2, R.Ty>) -> Bool:
  match ts:
    case Nil{}:
      False{}
    case Con{t, rest}:
      match t:
        case R.TBool{}:
          mentions(g, rest)
        case R.TCon{name, args}:
          String.eq(name, g) || mentions(g, List.append(&2, R.Ty, args, rest))
        case R.TBox{inner}:
          mentions(g, inner <> rest)
        case R.TRef{inner}:
          mentions(g, inner <> rest)
        case R.TFn{args, ret}:
          mentions(g, ret <> List.append(&2, R.Ty, args, rest))

# R1: a generic stays only if the erased types mention it.
def used(gens: List<&2, String>, +tys: List<&2, R.Ty>) -> List<&2, String>:
  match gens:
    case Nil{}:
      []
    case Con{+g, rest}:
      +tail = used(rest, tys)
      Bool.pick(List<&2, String>, mentions(g, tys), g <> tail, tail)

def param_tys(ps: List<&2, R.Param>) -> List<&2, R.Ty>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{name, ty}, rest}:
      ty <> param_tys(rest)

type Head is Data:
  HFn{generics: List<&2, String>, params: List<&2, R.Param>, ret: R.Ty}
  HProp{}
  HNo{why: String}

# `bad`: the refusal reasons of the binders, last binder first.
type Acc is Data:
  Acc{gens: List<&2, String>, params: List<&2, R.Param>, bad: List<&2, String>}

def fn_head(gens: List<&2, String>, params: List<&2, R.Param>, e: Er) -> Head:
  match e:
    case EProp{}:
      HProp{}
    case ENo{why}:
      HNo{why}
    case EData{+ret}:
      +ps = List.reverse(&2, R.Param, params)
      HFn{used(List.reverse(&2, String, gens), ret <> param_tys(ps)), ps, ret}

def head_of(acc: Acc, e: Er) -> Head:
  match acc:
    case Acc{gens, params, bad}:
      match bad:
        case Con{why, more}:
          HNo{first(List.reverse(&2, String, why <> more))}
        case Nil{}:
          fn_head(gens, params, e)

def tele_step(copies: List<&2, String>, q: Quantity.T, x: String, name: String, acc: Acc, bd: Bd) -> Acc:
  match acc:
    case Acc{gens, params, bad}:
      match bd:
        case BGen{}:
          Acc{x <> gens, params, bad}
        case BDrop{}:
          Acc{gens, params, bad}
        case BParam{t}:
          Acc{gens, R.Param{fn_name(name), mode(copies, q, t)} <> params, bad}
        case BNo{why}:
          Acc{gens, params, (why ++ " (binder " ++ x ++ ")") <> bad}

# A Pi telescope and the lambdas of the body, in step. Parameter names come
# from the lambdas, generic names from the telescope. `gens` and `params`
# are in reverse order.
@unsafe def tele(+env: Env, +ctx: List<&2, Knd>, ty: Term.T, +body: Term.T, acc: Acc) -> Head:
  match ty:
    case Term.Ran{Shape.SPi{+q, +x, +dom}, rest}:
      tele(env, kind_of(end_of(dom), x) <> ctx, rest, peel_body(body),
        tele_step(copies_of(env), q, x, peel_name(body, x), acc, binder_er(q, end_of(dom), one_er(ers(env, ctx, [dom])))))
    case other:
      head_of(acc, one_er(ers(env, ctx, [other])))

# Families.

def box_if(self: Bool, +t: R.Ty) -> R.Ty:
  Bool.pick(R.Ty, self, R.TBox{t}, t)

# A field that holds its own family goes in a `Box`.
def boxed(own: String, t: R.Ty) -> R.Ty:
  match t:
    case R.TCon{+name, args}:
      box_if(String.eq(name, own), R.TCon{name, args})
    case other:
      other

def field_step(own: String, bd: Bd, tail: Result<&2, &2, String, List<&2, R.Ty>>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  match bd:
    case BGen{}:
      Fail{"a type-valued constructor field"}
    case BDrop{}:
      tail
    case BParam{t}:
      push_ty(boxed(own, t), tail)
    case BNo{why}:
      Fail{why}

@unsafe def fields(+env: Env, +ctx: List<&2, Knd>, +own: String, args: List<&2, Pos.Binder>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  match args:
    case Nil{}:
      Done{[]}
    case Con{Pos.Binder{q, x, +ty}, rest}:
      field_step(own, binder_er(q, end_of(ty), one_er(ers(env, ctx, [ty]))),
        fields(env, kind_of(end_of(ty), x) <> ctx, own, rest))

def push_variant(name: String, fs: Result<&2, &2, String, List<&2, R.Ty>>, vs: Result<&2, &2, String, List<&2, R.Variant>>) -> Result<&2, &2, String, List<&2, R.Variant>>:
  match fs:
    case Fail{why}:
      Fail{why}
    case Done{f}:
      match vs:
        case Fail{why}:
          Fail{why}
        case Done{v}:
          Done{R.Variant{name, f} <> v}

# The fields of a constructor, if its name comes back from the variant name
# (D7).
def ctor_kept(+name: String, fs: Result<&2, &2, String, List<&2, R.Ty>>) -> Result<&2, &2, String, List<&2, R.Ty>>:
  +rust = upper_first(name)
  +back = lower_first(rust)
  Bool.pick(Result<&2, &2, String, List<&2, R.Ty>>, String.eq(back, name), fs, Fail{back_text(rust, back)})

def variants(+env: Env, +ctx: List<&2, Knd>, +own: String, ctors: List<&2, Pos.Ctor>) -> Result<&2, &2, String, List<&2, R.Variant>>:
  match ctors:
    case Nil{}:
      Done{[]}
    case Con{Pos.Ctor{+name, args, res_idx, full_arity, self_rec}, rest}:
      push_variant(upper_first(name), ctor_kept(name, fields(env, ctx, own, args)), variants(env, ctx, own, rest))

def param_kinds(params: List<&2, Pos.Binder>, ctx: List<&2, Knd>) -> List<&2, Knd>:
  match params:
    case Nil{}:
      ctx
    case Con{Pos.Binder{q, name, ty}, rest}:
      param_kinds(rest, kind_of(end_of(ty), name) <> ctx)

# `ks` has the innermost binder first, so the result is in source order.
def gen_names(ks: List<&2, Knd>, acc: List<&2, String>) -> List<&2, String>:
  match ks:
    case Nil{}:
      acc
    case Con{k, rest}:
      match k:
        case KGen{name}:
          gen_names(rest, name <> acc)
        case KProp{}:
          gen_names(rest, acc)
        case KVal{}:
          gen_names(rest, acc)

def all_unit(vs: List<&2, R.Variant>) -> Bool:
  match vs:
    case Nil{}:
      True{}
    case Con{R.Variant{name, fs}, rest}:
      match fs:
        case Nil{}:
          all_unit(rest)
        case Con{f, more}:
          False{}

def variant_tys(vs: List<&2, R.Variant>) -> List<&2, R.Ty>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fs}, rest}:
      List.append(&2, R.Ty, fs, variant_tys(rest))

def unit_ctor(vs: List<&2, R.Variant>) -> Maybe<&2, String>:
  match vs:
    case Con{R.Variant{name, Nil{}}, Nil{}}:
      Some{name}
    case other:
      None{}

# Classes.

type Class is Data:
  CFn{name: String, generics: List<&2, String>, params: List<&2, R.Param>, ret: R.Ty}
  CData{item: R.Item}
  CBool{}
  CProp{}
  CType{}
  CAbsurd{}
  CNo{why: String}
  CSkip{}

# One constructor with no field and no generic is a unit struct. Each other
# runtime family is an enum.
# The class `c` of the mech name, if the name comes back from its Rust name
# (D7).
def name_kept(mech: String, +rust: String, +back: String, c: Class) -> Class:
  Bool.pick(Class, String.eq(back, mech), c, CNo{back_text(rust, back)})

# `ctor` is the variant name of the constructor. `variants` refuses a
# constructor with an upper-case first letter, so the mech name is
# `lower_first(ctor)`.
def unit_kept(+name: String, ctor: String) -> Class:
  +mech = lower_first(ctor)
  +rust = fn_name(mech)
  name_kept(mech, rust, fn_back(rust), CData{R.IUnit{name, rust}})

def fn_class(+name: String, gens: List<&2, String>, params: List<&2, R.Param>, ret: R.Ty) -> Class:
  +rust = fn_name(name)
  name_kept(name, rust, fn_back(rust), CFn{rust, gens, params, ret})

def unit_class(+name: String, +gens: List<&2, String>, +vars: List<&2, R.Variant>, u: Maybe<&2, String>) -> Class:
  match u:
    case Some{+ctor}:
      Bool.pick(Class, no_names(gens), unit_kept(name, ctor), CNo{"a type parameter with no use in a field"})
    case None{}:
      Bool.pick(Class, same_count(used(gens, variant_tys(vars)), gens),
        CData{R.IEnum{name, gens, all_unit(vars), vars}}, CNo{"a type parameter with no use in a field"})

def data_class(name: String, gens: List<&2, String>, +vars: List<&2, R.Variant>) -> Class:
  unit_class(name, gens, vars, unit_ctor(vars))

def family_class(+name: String, gens: List<&2, String>, vs: Result<&2, &2, String, List<&2, R.Variant>>) -> Class:
  match vs:
    case Fail{why}:
      CNo{why}
    case Done{vars}:
      Bool.pick(Class, String.eq(name, "MechBool"), CBool{}, data_class(name, gens, vars))

def classify_family(env: Env, fam: Pos.Family) -> Class:
  match fam:
    case Pos.Family{+name, params, indices, level, status, ctors, positive}:
      +ctx = param_kinds(params, [])
      Bool.pick(Class, Level.always_zero(level), CProp{},
        family_class(name, gen_names(ctx, []), variants(env, ctx, name, ctors)))

def prop_family(fam: Maybe<&2, Pos.Family>) -> Bool:
  match fam:
    case None{}:
      False{}
    case Some{Pos.Family{name, params, indices, level, status, ctors, positive}}:
      Level.always_zero(level)

# D4: the body is, below its lambdas, a case with no branch on a value of a
# Prop family.
@unsafe def absurd(env: Env, body: Term.T) -> Bool:
  match body:
    case Term.Sec{Shape.SPi{q, x, dom}, Con{Term.Leg{binders, inner}, rest}}:
      absurd(env, inner)
    case Term.Ann{inner, ty}:
      absurd(env, inner)
    case Term.Elim{Term.ElimData{Shape.SMu{name, args}, scrut, q, motive, Nil{}}}:
      prop_family(Global.find_family(name, globals_of(env)))
    case other:
      False{}

def head_class(name: String, h: Head) -> Class:
  match h:
    case HFn{gens, params, ret}:
      fn_class(name, gens, params, ret)
    case HProp{}:
      CProp{}
    case HNo{why}:
      CNo{why}

def run_class(+env: Env, name: String, ty: Term.T, +body: Term.T) -> Class:
  Bool.pick(Class, absurd(env, body), CAbsurd{}, head_class(name, tele(env, [], ty, body, Acc{[], [], []})))

def by_result(e: Er, c: Class) -> Class:
  match e:
    case EProp{}:
      CProp{}
    case ENo{why}:
      CNo{why}
    case EData{t}:
      c

def by_end(e: End, c: Class) -> Class:
  match e:
    case EndProp{}:
      CProp{}
    case EndType{}:
      CType{}
    case EndNone{}:
      c

def def_class(+env: Env, name: String, +ty: Term.T, body: Term.T) -> Class:
  by_end(end_of(ty), by_result(result_of(env, [], ty), run_class(env, name, ty, body)))

def axiom_class(env: Env, +ty: Term.T) -> Class:
  by_end(end_of(ty), by_result(result_of(env, [], ty), CNo{"a postulate in the runtime"}))

def entry_class(env: Env, name: String, entry: Global.Entry) -> Class:
  match entry:
    case Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}:
      def_class(env, name, ty, body)
    case Global.Axiom{ty}:
      axiom_class(env, ty)
    case Global.Prim{ty, prim}:
      CNo{"a primitive"}

# A name with no entry and no family is a constructor: no line.
def family_or_skip(env: Env, fam: Maybe<&2, Pos.Family>) -> Class:
  match fam:
    case Some{f}:
      classify_family(env, f)
    case None{}:
      CSkip{}

def entry_or(env: Env, name: String, entry: Maybe<&2, Global.Entry>, fam: Maybe<&2, Pos.Family>) -> Class:
  match entry:
    case Some{e}:
      entry_class(env, name, e)
    case None{}:
      family_or_skip(env, fam)

def name_class(+env: Env, +name: String) -> Class:
  entry_or(env, name, Global.find(name, globals_of(env)), Global.find_family(name, globals_of(env)))

# The classify table.

def join_rest(+sep: String, xs: List<&2, String>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      sep ++ x ++ join_rest(sep, rest)

def join(sep: String, xs: List<&2, String>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      x ++ join_rest(sep, rest)

def wrap(open: String, close: String, xs: List<&2, String>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      open ++ join(", ", x <> rest) ++ close

@unsafe def show_tys(ts: List<&2, R.Ty>) -> List<&2, String>:
  match ts:
    case Nil{}:
      []
    case Con{t, rest}:
      match t:
        case R.TBool{}:
          "bool" <> show_tys(rest)
        case R.TCon{name, args}:
          (name ++ wrap("<", ">", show_tys(args))) <> show_tys(rest)
        case R.TBox{inner}:
          ("Box<" ++ first(show_tys([inner])) ++ ">") <> show_tys(rest)
        case R.TRef{inner}:
          ("&" ++ first(show_tys([inner]))) <> show_tys(rest)
        case R.TFn{args, ret}:
          ("impl Fn(" ++ join(", ", show_tys(args)) ++ ") -> " ++ first(show_tys([ret]))) <> show_tys(rest)

def show_ty(t: R.Ty) -> String:
  first(show_tys([t]))

def show_params(ps: List<&2, R.Param>) -> List<&2, String>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{name, ty}, rest}:
      (name ++ ": " ++ show_ty(ty)) <> show_params(rest)

def bounds(gens: List<&2, String>) -> List<&2, String>:
  match gens:
    case Nil{}:
      []
    case Con{g, rest}:
      (g ++ ": Clone") <> bounds(rest)

def show_variants(vs: List<&2, R.Variant>) -> List<&2, String>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fs}, rest}:
      (name ++ wrap("(", ")", show_tys(fs))) <> show_variants(rest)

def show_item(item: R.Item) -> String:
  match item:
    case R.IUse{module}:
      "use " ++ module
    case R.IEnum{name, gens, copy, vars}:
      Bool.pick(String, copy, "copy ", "") ++ "enum " ++ name ++ wrap("<", ">", gens) ++ " { " ++ join(", ", show_variants(vars)) ++ " }"
    case R.IUnit{name, ctor}:
      "struct " ++ name ++ "; fn " ++ ctor
    case R.IFn{name, gens, params, ret, body}:
      "fn " ++ name

def line(name: String, c: Class) -> String:
  match c:
    case CFn{rust, gens, params, ret}:
      "fn " ++ name ++ " = pub fn " ++ rust ++ wrap("<", ">", bounds(gens)) ++ "(" ++ join(", ", show_params(params)) ++ ") -> " ++ show_ty(ret) ++ "\n"
    case CData{item}:
      "data " ++ name ++ " = " ++ show_item(item) ++ "\n"
    case CBool{}:
      "bool " ++ name ++ "\n"
    case CProp{}:
      "prop " ++ name ++ "\n"
    case CType{}:
      "type " ++ name ++ "\n"
    case CAbsurd{}:
      "absurd " ++ name ++ "\n"
    case CNo{why}:
      "refused " ++ name ++ ": " ++ why ++ "\n"
    case CSkip{}:
      ""

# The Rust item names of a class, for the collision check (D7).
def item_names(c: Class) -> List<&2, String>:
  match c:
    case CFn{rust, gens, params, ret}:
      [rust]
    case CData{R.IEnum{name, gens, copy, vars}}:
      [name]
    case CData{R.IUnit{name, ctor}}:
      [name, ctor]
    case other:
      []

def copy_names(c: Class) -> List<&2, String>:
  match c:
    case CData{R.IEnum{name, gens, copy, vars}}:
      Bool.pick(List<&2, String>, copy, [name], [])
    case CData{R.IUnit{name, ctor}}:
      [name]
    case other:
      []

# `names`: the Rust item names so far. `out`: the table text.
type St is Data:
  St{env: Env, names: List<&2, String>, out: String}

def step(st: St, +name: String) -> St:
  match st:
    case St{+env, names, out}:
      +c = name_class(env, name)
      St{Env{globals_of(env), List.append(&2, String, copy_names(c), copies_of(env))},
        List.append(&2, String, item_names(c), names), out ++ line(name, c)}

@unsafe def steps(st: St, names: List<&2, String>) -> St:
  match names:
    case Nil{}:
      st
    case Con{name, rest}:
      steps(step(st, name), rest)

# Declared names in source order (a `mu` adds no row, so the rows do not
# have the position of a family).
def decl_names(decls: List<&2, Syntax.Decl>) -> List<&2, String>:
  match decls:
    case Nil{}:
      []
    case Con{d, rest}:
      List.append(&2, String, Elab.declared_names(d), decl_names(rest))

def failed(e: Error.T) -> St:
  St{Env{Global.empty(), []}, [], "error: " ++ Error.to_string(e) ++ "\n"}

# Checks one source file on top of `env` and classifies its declarations.
def checked_steps(copies: List<&2, String>, names: List<&2, String>, decls: List<&2, Syntax.Decl>, r: Result<&2, &2, Error.T, C.Pair<Global.T, List<&2, C.Pair<String, Global.Entry>>>>) -> St:
  match r:
    case Fail{e}:
      failed(e)
    case Done{C.Pair{checked, rows}}:
      steps(St{Env{checked, copies}, names, ""}, decl_names(decls))

def parsed_steps(globals: Global.T, copies: List<&2, String>, names: List<&2, String>, r: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> St:
  match r:
    case Fail{e}:
      failed(e)
    case Done{+decls}:
      checked_steps(copies, names, decls, Elab.elab_program_in(globals, Budget.unlimited(), decls))

def classify_source(env: Env, names: List<&2, String>, source: String) -> St:
  match env:
    case Env{globals, copies}:
      parsed_steps(globals, copies, names, Parser.program(source))

def first_dup(names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{+x, +rest}:
      Bool.pick(String, has(rest, x), x, first_dup(rest))

def collision_line(names: List<&2, String>) -> String:
  +d = first_dup(names)
  Bool.pick(String, String.eq(d, ""), "names: no collision\n", "refused: name collision (D7): " ++ d ++ "\n")

def finish(st: St) -> String:
  match st:
    case St{env, names, out}:
      out ++ collision_line(names)

def classify1(source: String) -> String:
  finish(classify_source(Env{Global.initial(), []}, [], source))

def second(st: St, source: String) -> String:
  match st:
    case St{env, names, out}:
      "## 1\n" ++ out ++ "## 2\n" ++ finish(classify_source(env, names, source))

def classify2(first_source: String, second_source: String) -> String:
  second(classify_source(Env{Global.initial(), []}, [], first_source), second_source)

# Part 2: bodies. A context is two lists in step, innermost binder first:
# `ks` (as part 1) and `vs` (`Some` = a runtime variable with its Rust name
# and type, `None` = an erased binder).

# `WFn`: a borrow of a function. `args` are the declared argument types.
type Want is Data:
  WOwn{}
  WRef{}
  WFn{args: List<&2, R.Ty>}

type Rx is Data:
  XOk{ex: R.Ex}
  XNo{why: String}

type Job is Data:
  Job{ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, term: Term.T}

# How to make an expression from the results of the jobs of a plan.
# `BMatch`: the first result is the scrutinee, then one result for each arm.
type Build is Data:
  BLeaf{rx: Rx}
  BSame{}
  BCall{func: String, targs: List<&2, R.Ty>, want: Want}
  BCtor{family: String, ctor: String, boxes: List<&2, Bool>, want: Want}
  BMatch{pats: List<&2, R.Pat>, want: Want}
  BIf{true_first: Bool, want: Want}
  BClosure{params: List<&2, R.Param>, want: Want}
  BLet{name: String, ty: R.Ty, want: Want}

type Plan is Data:
  Plan{jobs: List<&2, Job>, build: Build}

def slot_at(vs: List<&2, Maybe<&2, R.Param>>, index: Nat) -> Maybe<&2, R.Param>:
  match vs:
    case Nil{}:
      None{}
    case Con{v, rest}:
      match index:
        case 0n:
          v
        case 1n+p:
          slot_at(rest, p)

def wanted(want: Want, ex: R.Ex) -> Rx:
  match want:
    case WOwn{}:
      XOk{ex}
    case WRef{}:
      XOk{R.XBorrow{ex}}
    case WFn{args}:
      XOk{R.XBorrow{ex}}

def is_box(t: R.Ty) -> Bool:
  match t:
    case R.TBox{inner}:
      True{}
    case other:
      False{}

def ref_use(name: String, in_box: Bool, want: Want) -> Rx:
  match want:
    case WOwn{}:
      Bool.pick(Rx, in_box, XNo{"a boxed field by value (M0)"}, XOk{R.XClone{R.XVar{name}}})
    case WRef{}:
      XOk{R.XVar{name}}
    case WFn{args}:
      XOk{R.XVar{name}}

def var_use(name: String, ty: R.Ty, want: Want) -> Rx:
  match ty:
    case R.TRef{inner}:
      ref_use(name, is_box(inner), want)
    case other:
      wanted(want, R.XVar{name})

def var_rx(slot: Maybe<&2, R.Param>, want: Want) -> Rx:
  match slot:
    case None{}:
      XNo{"an erased variable in a runtime position"}
    case Some{R.Param{name, ty}}:
      var_use(name, ty, want)

def leaf(rx: Rx) -> Plan:
  Plan{[], BLeaf{rx}}

def no_plan(why: String) -> Plan:
  Plan{[], BLeaf{XNo{why}}}

def fn_want(inner: R.Ty) -> Want:
  match inner:
    case R.TFn{args, ret}:
      WFn{args}
    case other:
      WRef{}

def want_of(t: R.Ty) -> Want:
  match t:
    case R.TRef{inner}:
      fn_want(inner)
    case other:
      WOwn{}

# Constructors. One entry for each constructor argument: `Some` = a kept
# field with its variant type, `None` = an erased field.

def kept_ty(own: String, bd: Bd) -> Maybe<&2, R.Ty>:
  match bd:
    case BParam{t}:
      Some{boxed(own, t)}
    case other:
      None{}

@unsafe def field_tys(+env: Env, +ctx: List<&2, Knd>, +own: String, args: List<&2, Pos.Binder>) -> List<&2, Maybe<&2, R.Ty>>:
  match args:
    case Nil{}:
      []
    case Con{Pos.Binder{q, x, +ty}, rest}:
      kept_ty(own, binder_er(q, end_of(ty), one_er(ers(env, ctx, [ty])))) <>
        field_tys(env, kind_of(end_of(ty), x) <> ctx, own, rest)

def ctor_args_tys(env: Env, ctx: List<&2, Knd>, own: String, ctor: Maybe<&2, Pos.Ctor>) -> List<&2, Maybe<&2, R.Ty>>:
  match ctor:
    case None{}:
      []
    case Some{Pos.Ctor{name, args, res_idx, full_arity, self_rec}}:
      field_tys(env, ctx, own, args)

def ctor_tys(env: Env, fam: Maybe<&2, Pos.Family>, c: String) -> List<&2, Maybe<&2, R.Ty>>:
  match fam:
    case None{}:
      []
    case Some{Pos.Family{name, params, indices, level, status, ctors, positive}}:
      ctor_args_tys(env, param_kinds(params, []), name, Pos.find_ctor(ctors, c))

def kept_job(ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, t: Maybe<&2, R.Ty>, f: Term.T, tail: List<&2, Job>) -> List<&2, Job>:
  match t:
    case None{}:
      tail
    case Some{ty}:
      Job{ks, vs, WOwn{}, f} <> tail

def kept_jobs(+ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, tys: List<&2, Maybe<&2, R.Ty>>, fs: List<&2, Term.T>) -> List<&2, Job>:
  match tys:
    case Nil{}:
      []
    case Con{t, more}:
      match fs:
        case Nil{}:
          []
        case Con{f, rest}:
          kept_job(ks, vs, t, f, kept_jobs(ks, vs, more, rest))

def box_step(t: Maybe<&2, R.Ty>, tail: List<&2, Bool>) -> List<&2, Bool>:
  match t:
    case None{}:
      tail
    case Some{ty}:
      is_box(ty) <> tail

def box_flags(tys: List<&2, Maybe<&2, R.Ty>>) -> List<&2, Bool>:
  match tys:
    case Nil{}:
      []
    case Con{t, rest}:
      box_step(t, box_flags(rest))

def enum_plan(ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, family: String, ctor: String, fs: List<&2, Term.T>, +tys: List<&2, Maybe<&2, R.Ty>>) -> Plan:
  Plan{kept_jobs(ks, vs, tys, fs), BCtor{family, ctor, box_flags(tys), want}}

def item_plan(+env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, fam: String, +c: String, fs: List<&2, Term.T>, item: R.Item) -> Plan:
  match item:
    case R.IUnit{name, ctor}:
      leaf(wanted(want, R.XCall{ctor, [], []}))
    case R.IEnum{name, generics, copy, variants}:
      enum_plan(ks, vs, want, name, upper_first(c), fs, ctor_tys(env, Global.find_family(fam, globals_of(env)), c))
    case other:
      no_plan("a constructor of an item that is not data")

def ctor_plan(env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, fam: String, c: String, fs: List<&2, Term.T>, cls: Class) -> Plan:
  match cls:
    case CBool{}:
      leaf(wanted(want, R.XBool{String.eq(c, "mechTrue")}))
    case CData{item}:
      item_plan(env, ks, vs, want, fam, c, fs, item)
    case other:
      no_plan("a constructor outside the M0 fragment: " ++ c)

# Applications.

type Sarg is Data:
  Sarg{q: Quantity.T, dom: Term.T, arg: Term.T}

type Spine is Data:
  Spine{head: Term.T, args: List<&2, Sarg>}

@unsafe def spine(t: Term.T, acc: List<&2, Sarg>) -> Spine:
  match t:
    case Term.Out{Shape.SPi{q, x, dom}, Term.APt{aq, arg}, head}:
      spine(head, Sarg{q, dom, arg} <> acc)
    case other:
      Spine{other, acc}

def ref_jobs(+ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, args: List<&2, Sarg>) -> List<&2, Job>:
  match args:
    case Nil{}:
      []
    case Con{Sarg{q, dom, arg}, more}:
      Job{ks, vs, WRef{}, arg} <> ref_jobs(ks, vs, more)

def keep_term(bd: Bd, arg: Term.T, tail: List<&2, Term.T>) -> List<&2, Term.T>:
  match bd:
    case BParam{t}:
      arg <> tail
    case other:
      tail

# The runtime arguments of a call of a local function.
@unsafe def kept_args(+env: Env, +ks: List<&2, Knd>, args: List<&2, Sarg>) -> List<&2, Term.T>:
  match args:
    case Nil{}:
      []
    case Con{Sarg{q, +dom, arg}, more}:
      keep_term(binder_er(q, end_of(dom), one_er(ers(env, ks, [dom]))), arg, kept_args(env, ks, more))

def zip_jobs(+ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, tys: List<&2, R.Ty>, args: List<&2, Term.T>) -> List<&2, Job>:
  match tys:
    case Nil{}:
      []
    case Con{t, more}:
      match args:
        case Nil{}:
          []
        case Con{a, rest}:
          Job{ks, vs, want_of(t), a} <> zip_jobs(ks, vs, more, rest)

# The erasure of a type argument: the type below its lambdas.
@unsafe def targ_er(+env: Env, ks: List<&2, Knd>, arg: Term.T) -> Er:
  match arg:
    case Term.Sec{Shape.SPi{q, x, dom}, Con{Term.Leg{binders, inner}, rest}}:
      targ_er(env, kind_of(end_of(dom), x) <> ks, inner)
    case other:
      one_er(ers(env, ks, [other]))

type Cargs is Data:
  Cargs{targs: List<&2, R.Ty>, jobs: List<&2, Job>, bad: List<&2, String>}

def push_job(j: Job, ca: Cargs) -> Cargs:
  match ca:
    case Cargs{targs, jobs, bad}:
      Cargs{targs, j <> jobs, bad}

def push_bad(why: String, ca: Cargs) -> Cargs:
  match ca:
    case Cargs{targs, jobs, bad}:
      Cargs{targs, jobs, why <> bad}

def push_ty_arg(t: R.Ty, ca: Cargs) -> Cargs:
  match ca:
    case Cargs{targs, jobs, bad}:
      Cargs{t <> targs, jobs, bad}

def push_targ(e: Er, ca: Cargs) -> Cargs:
  match e:
    case EData{t}:
      push_ty_arg(t, ca)
    case EProp{}:
      push_bad("a Prop as a type argument", ca)
    case ENo{why}:
      push_bad(why, ca)

def gen_step(turbofish: Bool, e: Er, +ca: Cargs) -> Cargs:
  Bool.pick(Cargs, turbofish, push_targ(e, ca), ca)

def arg_step(+env: Env, +ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, turbofish: Bool, q: Quantity.T, arg: Term.T, bd: Bd, tail: Cargs) -> Cargs:
  match bd:
    case BGen{}:
      gen_step(turbofish, targ_er(env, ks, arg), tail)
    case BDrop{}:
      tail
    case BParam{t}:
      push_job(Job{ks, vs, want_of(mode(copies_of(env), q, t)), arg}, tail)
    case BNo{why}:
      push_bad(why, tail)

def args_end(args: List<&2, Sarg>) -> Cargs:
  match args:
    case Nil{}:
      Cargs{[], [], []}
    case Con{a, more}:
      Cargs{[], [], ["more arguments than the telescope of the callee (M0)"]}

# The telescope of the callee and the arguments, in step. The mode of each
# argument comes from the telescope, not from the application node.
@unsafe def call_args(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, +gens: List<&2, String>, +tctx: List<&2, Knd>, ty: Term.T, args: List<&2, Sarg>) -> Cargs:
  match ty:
    case Term.Ran{Shape.SPi{+q, +x, +dom}, rest}:
      match args:
        case Nil{}:
          Cargs{[], [], ["a partial application (M0)"]}
        case Con{Sarg{aq, adom, arg}, more}:
          arg_step(env, ks, vs, has(gens, x), q, arg, binder_er(q, end_of(dom), one_er(ers(env, tctx, [dom]))),
            call_args(env, ks, vs, gens, kind_of(end_of(dom), x) <> tctx, rest, more))
    case other:
      args_end(args)

def call_built(name: String, want: Want, ca: Cargs) -> Plan:
  match ca:
    case Cargs{targs, jobs, bad}:
      match bad:
        case Nil{}:
          Plan{jobs, BCall{name, targs, want}}
        case Con{why, more}:
          no_plan(why)

def fn_plan(+env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, cls: Class, entry: Maybe<&2, Global.Entry>, args: List<&2, Sarg>) -> Plan:
  match cls:
    case CFn{name, generics, params, ret}:
      match entry:
        case Some{e}:
          call_built(name, want, call_args(env, ks, vs, generics, [], Global.entry_ty(e), args))
        case None{}:
          no_plan("a call of a name with no entry")
    case CAbsurd{}:
      no_plan("a call of an absurd function (D4)")
    case CNo{why}:
      no_plan(why)
    case other:
      no_plan("a call of a name that is not a function")

def call_plan(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, +want: Want, +name: String, +args: List<&2, Sarg>) -> Plan:
  Bool.pick(Plan, String.eq(name, "natAdd"), Plan{ref_jobs(ks, vs, args), BCall{"nat_add", [], want}},
    fn_plan(env, ks, vs, want, name_class(env, name), Global.find(name, globals_of(env)), args))

def local_plan(env: Env, +ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, slot: Maybe<&2, R.Param>, args: List<&2, Sarg>) -> Plan:
  match slot:
    case Some{R.Param{name, R.TRef{R.TFn{tys, ret}}}}:
      Plan{zip_jobs(ks, vs, tys, kept_args(env, ks, args)), BCall{name, [], want}}
    case other:
      no_plan("a call of a variable that is not a function")

def app_plan(+env: Env, ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, sp: Spine) -> Plan:
  match sp:
    case Spine{head, args}:
      match head:
        case Term.Global{name}:
          call_plan(env, ks, vs, want, name, args)
        case Term.Var{index}:
          local_plan(env, ks, vs, want, slot_at(vs, index), args)
        case other:
          no_plan("an application head outside the M0 fragment")

# Lambdas: nested lambdas give one closure.

def slot_of(copies: List<&2, String>, q: Quantity.T, name: String, bd: Bd) -> Maybe<&2, R.Param>:
  match bd:
    case BParam{t}:
      Some{R.Param{fn_name(name), mode(copies, q, t)}}
    case other:
      None{}

def push_slot(slot: Maybe<&2, R.Param>, params: List<&2, R.Param>) -> List<&2, R.Param>:
  match slot:
    case None{}:
      params
    case Some{p}:
      p <> params

type Lam is Data:
  Lam{ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, params: List<&2, R.Param>, body: Term.T}

# A closure that is an argument takes the reference mode of each declared
# argument of the parameter: a generic argument is a borrow also when the
# type of the binder is Copy. `decl` is empty for each other closure.
def borrowed(t: R.Ty) -> R.Ty:
  match t:
    case R.TRef{inner}:
      R.TRef{inner}
    case other:
      R.TRef{other}

def as_decl(d: R.Ty, ty: R.Ty) -> R.Ty:
  match d:
    case R.TRef{inner}:
      borrowed(ty)
    case other:
      ty

def decl_ty(decl: List<&2, R.Ty>, ty: R.Ty) -> R.Ty:
  match decl:
    case Nil{}:
      ty
    case Con{d, rest}:
      as_decl(d, ty)

def decl_slot(slot: Maybe<&2, R.Param>, decl: List<&2, R.Ty>) -> Maybe<&2, R.Param>:
  match slot:
    case None{}:
      None{}
    case Some{R.Param{name, ty}}:
      Some{R.Param{name, decl_ty(decl, ty)}}

def decl_tail(decl: List<&2, R.Ty>) -> List<&2, R.Ty>:
  match decl:
    case Nil{}:
      []
    case Con{d, rest}:
      rest

# An erased binder has no declared argument.
def decl_rest(slot: Maybe<&2, R.Param>, decl: List<&2, R.Ty>) -> List<&2, R.Ty>:
  match slot:
    case None{}:
      decl
    case Some{p}:
      decl_tail(decl)

@unsafe def lams(+env: Env, +ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, params: List<&2, R.Param>, +decl: List<&2, R.Ty>, t: Term.T) -> Lam:
  match t:
    case Term.Sec{Shape.SPi{+q, +x, +dom}, Con{Term.Leg{binders, inner}, rest}}:
      +own = slot_of(copies_of(env), q, x, binder_er(q, end_of(dom), one_er(ers(env, ks, [dom]))))
      +slot = decl_slot(own, decl)
      lams(env, kind_of(end_of(dom), x) <> ks, slot <> vs, push_slot(slot, params), decl_rest(own, decl), inner)
    case other:
      Lam{ks, vs, List.reverse(&2, R.Param, params), other}

def lam_done(want: Want, lam: Lam) -> Plan:
  match lam:
    case Lam{ks, vs, params, body}:
      Plan{[Job{ks, vs, WOwn{}, body}], BClosure{params, want}}

def lam_plan(env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, t: Term.T) -> Plan:
  match want:
    case WOwn{}:
      lam_done(WOwn{}, lams(env, ks, vs, [], [], t))
    case WRef{}:
      lam_done(WRef{}, lams(env, ks, vs, [], [], t))
    case WFn{args}:
      lam_done(WRef{}, lams(env, ks, vs, [], args, t))

# Cases. `binds` is in reverse order.

type Scope is Data:
  Scope{ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, binds: List<&2, R.Bind>}

def bind_some(+name: String, ty: R.Ty, sc: Scope) -> Scope:
  match sc:
    case Scope{ks, vs, binds}:
      Scope{KVal{} <> ks, Some{R.Param{name, R.TRef{ty}}} <> vs, R.BVar{name} <> binds}

def bind_none(sc: Scope) -> Scope:
  match sc:
    case Scope{ks, vs, binds}:
      Scope{KProp{} <> ks, None{} <> vs, binds}

def bind_step(x: String, t: Maybe<&2, R.Ty>, sc: Scope) -> Scope:
  match t:
    case None{}:
      bind_none(sc)
    case Some{ty}:
      bind_some(fn_name(x), ty, sc)

@unsafe def scoped(binders: List<&2, C.Pair<Quantity.T, String>>, tys: List<&2, Maybe<&2, R.Ty>>, sc: Scope) -> Scope:
  match binders:
    case Nil{}:
      sc
    case Con{C.Pair{q, x}, rest}:
      match tys:
        case Nil{}:
          scoped(rest, [], bind_none(sc))
        case Con{t, more}:
          scoped(rest, more, bind_step(x, t, sc))

type Br is Data:
  Br{pat: R.Pat, job: Job}

def br(fam: String, c: String, body: Term.T, sc: Scope) -> Br:
  match sc:
    case Scope{ks, vs, binds}:
      Br{R.PCtor{fam, upper_first(c), List.reverse(&2, R.Bind, binds)}, Job{ks, vs, WOwn{}, body}}

def br_of(env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, fam: String, family: Maybe<&2, Pos.Family>, addr: Term.Addr<Term.T>, leg: Term.Leg<Term.T>) -> Br:
  match addr:
    case Term.ACtor{+c}:
      match leg:
        case Term.Leg{binders, body}:
          br(fam, c, body, scoped(binders, ctor_tys(env, family, c), Scope{ks, vs, []}))
    case other:
      Br{R.PCtor{fam, "?", []}, Job{ks, vs, WOwn{}, Term.Auto{}}}

@unsafe def brs(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, +fam: String, +family: Maybe<&2, Pos.Family>, branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> List<&2, Br>:
  match branches:
    case Nil{}:
      []
    case Con{C.Pair{addr, leg}, rest}:
      br_of(env, ks, vs, fam, family, addr, leg) <> brs(env, ks, vs, fam, family, rest)

def br_jobs(bs: List<&2, Br>) -> List<&2, Job>:
  match bs:
    case Nil{}:
      []
    case Con{Br{pat, job}, rest}:
      job <> br_jobs(rest)

def br_pats(bs: List<&2, Br>) -> List<&2, R.Pat>:
  match bs:
    case Nil{}:
      []
    case Con{Br{pat, job}, rest}:
      pat <> br_pats(rest)

def nones_k(binders: List<&2, C.Pair<Quantity.T, String>>, ks: List<&2, Knd>) -> List<&2, Knd>:
  match binders:
    case Nil{}:
      ks
    case Con{b, rest}:
      nones_k(rest, KProp{} <> ks)

def nones_v(binders: List<&2, C.Pair<Quantity.T, String>>, vs: List<&2, Maybe<&2, R.Param>>) -> List<&2, Maybe<&2, R.Param>>:
  match binders:
    case Nil{}:
      vs
    case Con{b, rest}:
      nones_v(rest, None{} <> vs)

# R6: a case on a proof with one branch is the body of the branch.
def prop_plan(ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> Plan:
  match branches:
    case Nil{}:
      no_plan("a case with no branch (D4)")
    case Con{C.Pair{addr, Term.Leg{+binders, body}}, rest}:
      Plan{[Job{nones_k(binders, ks), nones_v(binders, vs), want, body}], BSame{}}

def leg_jobs(+ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> List<&2, Job>:
  match branches:
    case Nil{}:
      []
    case Con{C.Pair{addr, Term.Leg{binders, body}}, rest}:
      Job{ks, vs, WOwn{}, body} <> leg_jobs(ks, vs, rest)

def first_is_true(branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> Bool:
  match branches:
    case Con{C.Pair{Term.ACtor{c}, leg}, rest}:
      String.eq(c, "mechTrue")
    case other:
      True{}

def bool_plan(+ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, scrut: Term.T, +branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>) -> Plan:
  Plan{Job{ks, vs, WOwn{}, scrut} <> leg_jobs(ks, vs, branches), BIf{first_is_true(branches), want}}

def match_plan(ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, by_value: Bool, scrut: Term.T, +bs: List<&2, Br>) -> Plan:
  Plan{Job{ks, vs, Bool.pick(Want, by_value, WOwn{}, WRef{}), scrut} <> br_jobs(bs), BMatch{br_pats(bs), want}}

def elim_plan(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, +fam: String, scrut: Term.T, branches: List<&2, C.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, cls: Class) -> Plan:
  match cls:
    case CProp{}:
      prop_plan(ks, vs, want, branches)
    case CBool{}:
      bool_plan(ks, vs, want, scrut, branches)
    case CData{item}:
      match_plan(ks, vs, want, has(copies_of(env), fam), scrut,
        brs(env, ks, vs, fam, Global.find_family(fam, globals_of(env)), branches))
    case other:
      no_plan("a case on a family outside the M0 fragment")

def lit_rx(want: Want, lit: Literal.T) -> Rx:
  match lit:
    case Literal.LInt{+value}:
      Bool.pick(Rx, Bignum.le(value, Bignum.of_u32(4294967295)),
        wanted(want, R.XCall{"nat_small", [], [R.XInt{Pp.literal(Literal.LInt{value})}]}),
        XNo{"a literal above u32 (M0)"})
    case other:
      XNo{"a literal that is not an integer (M0)"}

# A let (E1). The binder enters `vs` as an owned `Param{rn, t}`: `var_use`
# moves it on an owned use and borrows it on a `WRef` use. Two owned uses of
# a non-Copy binder do not compile (rustc E0382), so E1 accepts a binder of
# a Copy type only; a non-Copy, function, type or Prop-sorted binder is a
# located refusal. A proof binder (an `EProp` type) is erased like a proof
# parameter. ED2: the Rust name of the binder must not collide with a slot
# of `vs` or with the Rust name of a global after the name map.
def let_tys(t: Maybe<&2, R.Ty>) -> List<&2, R.Ty>:
  match t:
    case None{}:
      []
    case Some{x}:
      [x]

def has_slot(vs: List<&2, Maybe<&2, R.Param>>, +name: String) -> Bool:
  match vs:
    case Nil{}:
      False{}
    case Con{Some{R.Param{n, t}}, rest}:
      String.eq(n, name) || has_slot(rest, name)
    case Con{None{}, rest}:
      has_slot(rest, name)

def global_clash(entries: List<&2, C.Pair<String, Global.Entry>>, +rn: String) -> Bool:
  match entries:
    case Nil{}:
      False{}
    case Con{C.Pair{g, e}, rest}:
      String.eq(fn_name(g), rn) || global_clash(rest, rn)

def entries_of(g: Global.T) -> List<&2, C.Pair<String, Global.Entry>>:
  match g:
    case Global.Globals{entries, families, entry_index, family_index}:
      entries

def let_clash(env: Env, vs: List<&2, Maybe<&2, R.Param>>, +rn: String) -> Bool:
  has_slot(vs, rn) || global_clash(entries_of(globals_of(env)), rn)

def let_copy(copies: List<&2, String>, t: R.Ty) -> Bool:
  match t:
    case R.TBool{}:
      True{}
    case R.TCon{+name, args}:
      has(copies, name)
    case other:
      False{}

def let_typed(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, +rn: String, +t: R.Ty, value: Term.T, body: Term.T) -> Plan:
  match t:
    case R.TFn{args, ret}:
      no_plan("a let of a function type (E1)")
    case other:
      Bool.pick(Plan, let_copy(copies_of(env), t),
        Plan{[Job{ks, vs, WOwn{}, value}, Job{KVal{} <> ks, Some{R.Param{rn, t}} <> vs, WOwn{}, body}], BLet{rn, t, want}},
        no_plan("a let of a non-Copy type (E1)"))

def let_er(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, +rn: String, er: Er, value: Term.T, body: Term.T) -> Plan:
  match er:
    case EData{t}:
      let_typed(env, ks, vs, want, rn, t, value, body)
    case EProp{}:
      Plan{[Job{KProp{} <> ks, None{} <> vs, want, body}], BSame{}}
    case ENo{why}:
      no_plan(why)

def let_kind(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, +rn: String, k: Knd, ty: Term.T, value: Term.T, body: Term.T) -> Plan:
  match k:
    case KGen{g}:
      no_plan("a let of a type (E1)")
    case KProp{}:
      no_plan("a let of a type (E1)")
    case KVal{}:
      Bool.pick(Plan, let_clash(env, vs, rn),
        no_plan("the let binder `" ++ rn ++ "` collides with a name in scope after the name map (E1)"),
        let_er(env, ks, vs, want, rn, one_er(ers(env, ks, [ty])), value, body))

def let_plan(+env: Env, ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, want: Want, +name: String, +ty: Term.T, value: Term.T, body: Term.T) -> Plan:
  let_kind(env, ks, vs, want, fn_name(name), kind_of(end_of(ty), name), ty, value, body)

# The plan of one term: its top node or its application spine only.
def plan_term(+env: Env, +ks: List<&2, Knd>, +vs: List<&2, Maybe<&2, R.Param>>, want: Want, term: Term.T) -> Plan:
  match term:
    case Term.Var{index}:
      leaf(var_rx(slot_at(vs, index), want))
    case Term.In{Shape.SMu{+fam, idx}, Term.ACtor{c}, fs}:
      ctor_plan(env, ks, vs, want, fam, c, fs, name_class(env, fam))
    case Term.Elim{Term.ElimData{Shape.SMu{+fam, idx}, scrut, sq, motive, branches}}:
      elim_plan(env, ks, vs, want, fam, scrut, branches, name_class(env, fam))
    case Term.Out{Shape.SPi{q, x, dom}, Term.APt{aq, arg}, head}:
      app_plan(env, ks, vs, want, spine(head, [Sarg{q, dom, arg}]))
    case Term.Global{name}:
      call_plan(env, ks, vs, want, name, [])
    case Term.Sec{Shape.SPi{q, x, dom}, legs}:
      lam_plan(env, ks, vs, want, Term.Sec{Shape.SPi{q, x, dom}, legs})
    case Term.Ann{body, ty}:
      Plan{[Job{ks, vs, want, body}], BSame{}}
    case Term.Lit{value}:
      leaf(lit_rx(want, value))
    case Term.Let{name, ty, value, body}:
      let_plan(env, ks, vs, want, name, ty, value, body)
    case other:
      no_plan("a term shape outside the M0 fragment")

def plan_of(env: Env, job: Job) -> Plan:
  match job:
    case Job{ks, vs, want, term}:
      plan_term(env, ks, vs, want, term)

# Builders.

def push_ex(r: Rx, tail: Result<&2, &2, String, List<&2, R.Ex>>) -> Result<&2, &2, String, List<&2, R.Ex>>:
  match r:
    case XNo{why}:
      Fail{why}
    case XOk{ex}:
      match tail:
        case Fail{why}:
          Fail{why}
        case Done{xs}:
          Done{ex <> xs}

def oks(rs: List<&2, Rx>) -> Result<&2, &2, String, List<&2, R.Ex>>:
  match rs:
    case Nil{}:
      Done{[]}
    case Con{r, rest}:
      push_ex(r, oks(rest))

def arm_bodies(arms: List<&2, R.Arm<R.Ex>>) -> List<&2, R.Ex>:
  match arms:
    case Nil{}:
      []
    case Con{R.Arm{pat, body}, rest}:
      body <> arm_bodies(rest)

# A use of the name in the expressions (a shadow counts as a use).
@unsafe def uses(+name: String, xs: List<&2, R.Ex>) -> Bool:
  match xs:
    case Nil{}:
      False{}
    case Con{x, rest}:
      match x:
        case R.XVar{n}:
          String.eq(n, name) || uses(name, rest)
        case R.XInt{text}:
          uses(name, rest)
        case R.XBool{value}:
          uses(name, rest)
        case R.XCall{func, targs, args}:
          String.eq(func, name) || uses(name, List.append(&2, R.Ex, args, rest))
        case R.XCtor{family, ctor, args}:
          uses(name, List.append(&2, R.Ex, args, rest))
        case R.XMatch{scrut, arms}:
          uses(name, scrut <> List.append(&2, R.Ex, arm_bodies(arms), rest))
        case R.XIf{cond, yes, no}:
          uses(name, cond <> (yes <> (no <> rest)))
        case R.XClosure{params, body}:
          uses(name, body <> rest)
        case R.XBorrow{inner}:
          uses(name, inner <> rest)
        case R.XClone{inner}:
          uses(name, inner <> rest)
        case R.XBox{inner}:
          uses(name, inner <> rest)
        case R.XLet{n, t, value, body}:
          uses(name, value <> (body <> rest))

# R2: a parameter with no use in the body gets `_` in front of its name.
def under(params: List<&2, R.Param>, +body: R.Ex) -> List<&2, R.Param>:
  match params:
    case Nil{}:
      []
    case Con{R.Param{+name, ty}, rest}:
      R.Param{Bool.pick(String, uses(name, [body]), name, "_" ++ name), ty} <> under(rest, body)

def skip_bind(b: R.Bind, body: R.Ex) -> R.Bind:
  match b:
    case R.BVar{+name}:
      Bool.pick(R.Bind, uses(name, [body]), R.BVar{name}, R.BSkip{})
    case R.BSkip{}:
      R.BSkip{}

def skip_binds(binds: List<&2, R.Bind>, +body: R.Ex) -> List<&2, R.Bind>:
  match binds:
    case Nil{}:
      []
    case Con{b, rest}:
      skip_bind(b, body) <> skip_binds(rest, body)

def skip_pat(p: R.Pat, body: R.Ex) -> R.Pat:
  match p:
    case R.PCtor{family, ctor, binds}:
      R.PCtor{family, ctor, skip_binds(binds, body)}

def zip_arms(pats: List<&2, R.Pat>, bodies: List<&2, R.Ex>) -> List<&2, R.Arm<R.Ex>>:
  match pats:
    case Nil{}:
      []
    case Con{p, more}:
      match bodies:
        case Nil{}:
          []
        case Con{+b, rest}:
          R.Arm{skip_pat(p, b), b} <> zip_arms(more, rest)

# R3: an arm with no field that builds its own constructor again.
def same_arm(ctor: String, binds: List<&2, R.Bind>, body: R.Ex) -> Bool:
  match binds:
    case Nil{}:
      match body:
        case R.XCtor{family, c, args}:
          String.eq(c, ctor)
        case other:
          False{}
    case Con{b, rest}:
      False{}

def all_same(arms: List<&2, R.Arm<R.Ex>>) -> Bool:
  match arms:
    case Nil{}:
      True{}
    case Con{R.Arm{R.PCtor{family, ctor, binds}, body}, rest}:
      same_arm(ctor, binds, body) && all_same(rest)

def any_arm(arms: List<&2, R.Arm<R.Ex>>) -> Bool:
  match arms:
    case Nil{}:
      False{}
    case Con{a, rest}:
      True{}

def match_ex(+scrut: R.Ex, +arms: List<&2, R.Arm<R.Ex>>) -> R.Ex:
  Bool.pick(R.Ex, any_arm(arms) && all_same(arms), scrut, R.XMatch{scrut, arms})

def match_rx(want: Want, pats: List<&2, R.Pat>, xs: List<&2, R.Ex>) -> Rx:
  match xs:
    case Nil{}:
      XNo{"internal: a match with no scrutinee"}
    case Con{scrut, bodies}:
      wanted(want, match_ex(scrut, zip_arms(pats, bodies)))

def is_true(x: R.Ex) -> Bool:
  match x:
    case R.XBool{value}:
      value
    case other:
      False{}

def is_false(x: R.Ex) -> Bool:
  match x:
    case R.XBool{value}:
      Bool.pick(Bool, value, False{}, True{})
    case other:
      False{}

# R4: `if c { true } else { false }` is `c`.
def if_ex(+c: R.Ex, +yes: R.Ex, +no: R.Ex) -> R.Ex:
  Bool.pick(R.Ex, is_true(yes) && is_false(no), c, R.XIf{c, yes, no})

def if_rx(+true_first: Bool, want: Want, xs: List<&2, R.Ex>) -> Rx:
  match xs:
    case Con{c, Con{+a, Con{+b, rest}}}:
      wanted(want, if_ex(c, Bool.pick(R.Ex, true_first, a, b), Bool.pick(R.Ex, true_first, b, a)))
    case other:
      XNo{"internal: an if without two arms"}

def let_rx(+name: String, ty: R.Ty, want: Want, xs: List<&2, R.Ex>) -> Rx:
  match xs:
    case Con{v, Con{b, rest}}:
      wanted(want, R.XLet{name, Some{ty}, v, b})
    case other:
      XNo{"internal: a let without two parts"}

def closure_rx(want: Want, params: List<&2, R.Param>, xs: List<&2, R.Ex>) -> Rx:
  match xs:
    case Nil{}:
      XNo{"internal: a closure with no body"}
    case Con{+body, rest}:
      wanted(want, R.XClosure{under(params, body), body})

def box_args(flags: List<&2, Bool>, xs: List<&2, R.Ex>) -> List<&2, R.Ex>:
  match flags:
    case Nil{}:
      xs
    case Con{flag, more}:
      match xs:
        case Nil{}:
          []
        case Con{+x, rest}:
          Bool.pick(R.Ex, flag, R.XBox{x}, x) <> box_args(more, rest)

def same_rx(xs: List<&2, R.Ex>) -> Rx:
  match xs:
    case Nil{}:
      XNo{"internal: no result"}
    case Con{x, rest}:
      XOk{x}

def built(r: Result<&2, &2, String, List<&2, R.Ex>>, b: Build) -> Rx:
  match r:
    case Fail{why}:
      XNo{why}
    case Done{xs}:
      match b:
        case BLeaf{rx}:
          rx
        case BSame{}:
          same_rx(xs)
        case BCall{func, targs, want}:
          wanted(want, R.XCall{func, targs, xs})
        case BCtor{family, ctor, boxes, want}:
          wanted(want, R.XCtor{family, ctor, box_args(boxes, xs)})
        case BMatch{pats, want}:
          match_rx(want, pats, xs)
        case BIf{true_first, want}:
          if_rx(true_first, want, xs)
        case BClosure{params, want}:
          closure_rx(want, params, xs)
        case BLet{name, ty, want}:
          let_rx(name, ty, want, xs)

def jobs_of(p: Plan) -> List<&2, Job>:
  match p:
    case Plan{jobs, build}:
      jobs

def build_of(p: Plan) -> Build:
  match p:
    case Plan{jobs, build}:
      build

# The one walker: a job list gives one result for each job.
@unsafe def run_jobs(+env: Env, jobs: List<&2, Job>) -> List<&2, Rx>:
  match jobs:
    case Nil{}:
      []
    case Con{job, rest}:
      +plan = plan_of(env, job)
      built(oks(run_jobs(env, jobs_of(plan))), build_of(plan)) <> run_jobs(env, rest)

def one_rx(rs: List<&2, Rx>) -> Rx:
  match rs:
    case Nil{}:
      XNo{"internal: no result"}
    case Con{r, rest}:
      r

# Definitions and modules.

type Fctx is Data:
  Fctx{ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, body: Term.T}

# The context of a function body: the telescope and the lambdas, in step
# (as `tele`).
@unsafe def fctx(+env: Env, +ks: List<&2, Knd>, vs: List<&2, Maybe<&2, R.Param>>, ty: Term.T, +body: Term.T) -> Fctx:
  match ty:
    case Term.Ran{Shape.SPi{+q, +x, +dom}, rest}:
      fctx(env, kind_of(end_of(dom), x) <> ks,
        slot_of(copies_of(env), q, peel_name(body, x), binder_er(q, end_of(dom), one_er(ers(env, ks, [dom])))) <> vs,
        rest, peel_body(body))
    case other:
      Fctx{ks, vs, body}

def body_rx(+env: Env, f: Fctx) -> Rx:
  match f:
    case Fctx{ks, vs, body}:
      one_rx(run_jobs(env, [Job{ks, vs, WOwn{}, body}]))

type It is Data:
  ItNone{}
  ItOne{item: R.Item}
  ItBad{why: String}

def fn_it(name: String, generics: List<&2, String>, params: List<&2, R.Param>, ret: R.Ty, r: Rx) -> It:
  match r:
    case XOk{+ex}:
      ItOne{R.IFn{name, generics, under(params, ex), ret, ex}}
    case XNo{why}:
      ItBad{why}

def fn_entry(+env: Env, name: String, generics: List<&2, String>, params: List<&2, R.Param>, ret: R.Ty, entry: Maybe<&2, Global.Entry>) -> It:
  match entry:
    case Some{Global.Def{Global.DefEntry{ty, body, reducible, rec_arg, partial}}}:
      fn_it(name, generics, params, ret, body_rx(env, fctx(env, [], [], ty, body)))
    case other:
      ItBad{"a function with no definition"}

def class_item(env: Env, c: Class, entry: Maybe<&2, Global.Entry>) -> It:
  match c:
    case CFn{name, generics, params, ret}:
      fn_entry(env, name, generics, params, ret, entry)
    case CData{item}:
      ItOne{item}
    case CNo{why}:
      ItBad{why}
    case other:
      ItNone{}

# `items` and `bad` are in reverse order.
type Es is Data:
  Es{env: Env, items: List<&2, R.Item>, bad: List<&2, String>}

def item_step(env: Env, items: List<&2, R.Item>, bad: List<&2, String>, name: String, it: It) -> Es:
  match it:
    case ItNone{}:
      Es{env, items, bad}
    case ItOne{item}:
      Es{env, item <> items, bad}
    case ItBad{why}:
      Es{env, items, ("refused " ++ name ++ ": " ++ why) <> bad}

def emit_step(es: Es, +name: String) -> Es:
  match es:
    case Es{+env, items, bad}:
      +c = name_class(env, name)
      item_step(Env{globals_of(env), List.append(&2, String, copy_names(c), copies_of(env))}, items, bad, name,
        class_item(env, c, Global.find(name, globals_of(env))))

@unsafe def emit_steps(es: Es, names: List<&2, String>) -> Es:
  match names:
    case Nil{}:
      es
    case Con{name, rest}:
      emit_steps(emit_step(es, name), rest)

def emit_failed(e: Error.T) -> Es:
  Es{Env{Global.empty(), []}, [], ["error: " ++ Error.to_string(e)]}

def emit_checked(copies: List<&2, String>, decls: List<&2, Syntax.Decl>, r: Result<&2, &2, Error.T, C.Pair<Global.T, List<&2, C.Pair<String, Global.Entry>>>>) -> Es:
  match r:
    case Fail{e}:
      emit_failed(e)
    case Done{C.Pair{checked, rows}}:
      emit_steps(Es{Env{checked, copies}, [], []}, decl_names(decls))

def emit_parsed(globals: Global.T, copies: List<&2, String>, r: Result<&2, &2, Error.T, List<&2, Syntax.Decl>>) -> Es:
  match r:
    case Fail{e}:
      emit_failed(e)
    case Done{+decls}:
      emit_checked(copies, decls, Elab.elab_program_in(globals, Budget.unlimited(), decls))

def emit_source(env: Env, source: String) -> Es:
  match env:
    case Env{globals, copies}:
      emit_parsed(globals, copies, Parser.program(source))

def item_nat(item: R.Item) -> Bool:
  match item:
    case R.IFn{name, generics, params, ret, body}:
      mentions("Nat", ret <> param_tys(params))
    case other:
      False{}

def nat_used(items: List<&2, R.Item>) -> Bool:
  match items:
    case Nil{}:
      False{}
    case Con{item, rest}:
      item_nat(item) || nat_used(rest)

def nat_use(nat: Bool) -> List<&2, R.Item>:
  Bool.pick(List<&2, R.Item>, nat, [R.IUse{"nat"}], [])

# The file name with no `.mech` at its end.
def stem(s: String) -> String:
  match s:
    case SNil{}:
      ""
    case SCon{+c, +rest}:
      Bool.pick(String, String.eq(rest, ".mech"), SCon{c, SNil{}}, SCon{c, stem(rest)})

def module_items(pre: List<&2, R.Item>, +items: List<&2, R.Item>) -> List<&2, R.Item>:
  List.append(&2, R.Item, pre, List.append(&2, R.Item, nat_use(nat_used(items)), items))

def module_of(source: String, pre: List<&2, R.Item>, es: Es) -> Result<&2, &2, String, R.Module>:
  match es:
    case Es{env, items, bad}:
      match bad:
        case Nil{}:
          Done{R.Module{source, module_items(pre, List.reverse(&2, R.Item, items))}}
        case Con{why, more}:
          Fail{join("\n", List.reverse(&2, String, why <> more)) ++ "\n"}

# One module. `name` is the file name for the `//!` line.
def emit1(name: String, source: String) -> Result<&2, &2, String, R.Module>:
  module_of(name, [], emit_source(Env{Global.initial(), []}, source))

def emit2_of(st: St, first_name: String, name: String, source: String) -> Result<&2, &2, String, R.Module>:
  match st:
    case St{env, names, out}:
      module_of(name, [R.IUse{stem(first_name)}], emit_source(env, source))

# The module of file 2, checked in the globals of file 1.
def emit2(first_name: String, first_source: String, name: String, source: String) -> Result<&2, &2, String, R.Module>:
  emit2_of(classify_source(Env{Global.initial(), []}, [], first_source), first_name, name, source)
