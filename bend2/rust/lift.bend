import Base
import ./ast.bend as A
import ./rir.bend as R
import ./emit.bend as Em

# Lifts the Rust AST of the frontend back to the erased program IR: the
# inverse of each lowering function of emit.bend, so `Em.text(lift(f))` is the
# text of `f` for each file that emit.bend writes. A shape outside the image
# of emit.bend is a refusal with the position of the nearest construct that
# has a `Meta` (CD13): an expression, type or pattern has no position of its
# own, so it takes the position of its statement, arm or item. Layout is not
# checked: the `gap` flag (a blank line) is the printer's job.
#
# Bend 2 has no mutual recursion, so `walk` is the one recursive walker of
# expressions. It takes a list of jobs; `kind` (not recursive) classifies the
# head of each job, so the string tests are out of the walker.

type Refusal is Data:
  Refusal{pos: A.Pos, what: String}

# `<line>:<col>: <construct>`; the caller that knows the file adds `<file>:`.
def show(r: Refusal) -> String:
  match r:
    case Refusal{pos, what}:
      A.show_pos(pos) ++ ": " ++ what

# ---- shared checks ----

# A construct with no docs and no attributes.
def plain(m: A.Meta) -> Result<&2, &2, Refusal, Bool>:
  match m:
    case A.Meta{gap, Nil{}, Nil{}, pos}:
      Done{True{}}
    case A.Meta{gap, Nil{}, attrs, pos}:
      Fail{Refusal{pos, "attribute"}}
    case A.Meta{gap, docs, attrs, pos}:
      Fail{Refusal{pos, "doc comment"}}

def strs_eq(xs: List<&2, String>, ys: List<&2, String>) -> Bool:
  match xs ys:
    case Nil{} Nil{}:
      True{}
    case Con{x, xr} Con{y, yr}:
      String.eq(x, y) && strs_eq(xr, yr)
    case a b:
      False{}

def attr_args(ats: List<&2, A.Attr>) -> List<&2, String>:
  match ats:
    case Con{A.Attr{name, Some{xs}}, rest}:
      xs
    case other:
      []

# The copy flag of an item whose only attribute is one of the two derive
# lists of the emitter (`Em.derive`).
def derived(m: A.Meta) -> Result<&2, &2, Refusal, Bool>:
  match m:
    case A.Meta{gap, Nil{}, Con{A.Attr{+name, Some{+args}}, Nil{}}, pos}:
      Bool.pick(Result<&2, &2, Refusal, Bool>, String.eq(name, "derive") && strs_eq(args, attr_args(Em.derive(True{}))), Done{True{}},
        Bool.pick(Result<&2, &2, Refusal, Bool>, String.eq(name, "derive") && strs_eq(args, attr_args(Em.derive(False{}))), Done{False{}},
          Fail{Refusal{pos, "derive list other than the emitter's"}}))
    case A.Meta{gap, Nil{}, attrs, pos}:
      Fail{Refusal{pos, "attributes other than one `derive` list"}}
    case A.Meta{gap, docs, attrs, pos}:
      Fail{Refusal{pos, "doc comment"}}

def is_pub(pos: A.Pos, vis: A.Vis) -> Result<&2, &2, Refusal, Bool>:
  match vis:
    case A.Pub{}:
      Done{True{}}
    case A.Priv{}:
      Fail{Refusal{pos, "item that is not `pub`"}}
    case A.PubCrate{}:
      Fail{Refusal{pos, "`pub(crate)` item"}}

def is_priv(pos: A.Pos, vis: A.Vis) -> Result<&2, &2, Refusal, Bool>:
  match vis:
    case A.Priv{}:
      Done{True{}}
    case A.Pub{}:
      Fail{Refusal{pos, "`pub` other than on an item"}}
    case A.PubCrate{}:
      Fail{Refusal{pos, "`pub(crate)` other than on an item"}}

# ---- types ----

# The head of one type: the string tests of `tks`.
type TK is Data:
  TKBool{}
  TKCon{name: String, args: List<&2, A.Ty>}
  TKBox{inner: A.Ty}
  TKRef{inner: A.Ty}
  TKFn{args: List<&2, A.Ty>, ret: A.Ty}
  TKBad{what: String}

def bool_ty(args: List<&2, A.Ty>) -> TK:
  match args:
    case Nil{}:
      TKBool{}
    case other:
      TKBad{"`bool` with generic arguments"}

def box_ty(args: List<&2, A.Ty>) -> TK:
  match args:
    case Con{i, Nil{}}:
      TKBox{i}
    case other:
      TKBad{"`Box` with other than one generic argument"}

def ty_kind(t: A.Ty) -> TK:
  match t:
    case A.TPath{Con{A.Seg{+name, +args}, Nil{}}}:
      Bool.pick(TK, String.eq(name, "bool"), bool_ty(args),
        Bool.pick(TK, String.eq(name, "Box"), box_ty(args), TKCon{name, args}))
    case A.TPath{segs}:
      TKBad{"type path with more than one segment"}
    case A.TRef{i}:
      TKRef{i}
    case A.TImpl{Con{A.TFnSugar{+f, args, Some{r}}, Nil{}}}:
      Bool.pick(TK, String.eq(f, "Fn"), TKFn{args, r}, TKBad{"`impl " ++ f ++ "`"})
    case A.TImpl{bs}:
      TKBad{"`impl` type other than `impl Fn(..) -> T`"}
    case A.TTuple{ts}:
      TKBad{"tuple type"}
    case A.TSlice{i}:
      TKBad{"slice type"}
    case A.TInfer{}:
      TKBad{"type `_`"}
    case A.TFnSugar{f, args, r}:
      TKBad{"`" ++ f ++ "(..)` type"}

def ty_kinds(ts: List<&2, A.Ty>) -> List<&2, TK>:
  match ts:
    case Nil{}:
      []
    case Con{t, rest}:
      ty_kind(t) <> ty_kinds(rest)

def one_ty(xs: List<&2, R.Ty>) -> R.Ty:
  match xs:
    case Nil{}:
      R.TBool{}
    case Con{x, rest}:
      x

@unsafe def tks(+pos: A.Pos, xs: List<&2, TK>) -> Result<&2, &2, Refusal, List<&2, R.Ty>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{TKBool{}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        more : List<&2, R.Ty> <- tks(pos, rest)
        return R.TBool{} <> more
    case Con{TKCon{name, args}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        ys : List<&2, R.Ty> <- tks(pos, ty_kinds(args))
        more : List<&2, R.Ty> <- tks(pos, rest)
        return R.TCon{name, ys} <> more
    case Con{TKBox{i}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        ys : List<&2, R.Ty> <- tks(pos, [ty_kind(i)])
        more : List<&2, R.Ty> <- tks(pos, rest)
        return R.TBox{one_ty(ys)} <> more
    case Con{TKRef{i}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        ys : List<&2, R.Ty> <- tks(pos, [ty_kind(i)])
        more : List<&2, R.Ty> <- tks(pos, rest)
        return R.TRef{one_ty(ys)} <> more
    case Con{TKFn{args, r}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        ys : List<&2, R.Ty> <- tks(pos, ty_kinds(args))
        rs : List<&2, R.Ty> <- tks(pos, [ty_kind(r)])
        more : List<&2, R.Ty> <- tks(pos, rest)
        return R.TFn{ys, one_ty(rs)} <> more
    case Con{TKBad{what}, rest}:
      Fail{Refusal{pos, what}}

def tys(pos: A.Pos, ts: List<&2, A.Ty>) -> Result<&2, &2, Refusal, List<&2, R.Ty>>:
  tks(pos, ty_kinds(ts))

def ty(pos: A.Pos, t: A.Ty) -> Result<&2, &2, Refusal, R.Ty>:
  do Result<&2, &2, Refusal, R.Ty>:
    ys : List<&2, R.Ty> <- tys(pos, [t])
    return one_ty(ys)

# ---- parameters and patterns ----

def params(+pos: A.Pos, ps: List<&2, A.Param>) -> Result<&2, &2, Refusal, List<&2, R.Param>>:
  match ps:
    case Nil{}:
      Done{[]}
    case Con{A.PTyped{A.PPath{Con{A.Seg{name, Nil{}}, Nil{}}}, t}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Param>>:
        rt : R.Ty <- ty(pos, t)
        more : List<&2, R.Param> <- params(pos, rest)
        return R.Param{name, rt} <> more
    case Con{other, rest}:
      Fail{Refusal{pos, "parameter other than `name: Type`"}}

def cparams(+pos: A.Pos, ps: List<&2, A.CParam>) -> Result<&2, &2, Refusal, List<&2, R.Param>>:
  match ps:
    case Nil{}:
      Done{[]}
    case Con{A.CParam{A.PPath{Con{A.Seg{name, Nil{}}, Nil{}}}, Some{t}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Param>>:
        rt : R.Ty <- ty(pos, t)
        more : List<&2, R.Param> <- cparams(pos, rest)
        return R.Param{name, rt} <> more
    case Con{other, rest}:
      Fail{Refusal{pos, "closure parameter other than `name: Type`"}}

def binds(+pos: A.Pos, ps: List<&2, A.Pat>) -> Result<&2, &2, Refusal, List<&2, R.Bind>>:
  match ps:
    case Nil{}:
      Done{[]}
    case Con{A.PPath{Con{A.Seg{name, Nil{}}, Nil{}}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Bind>>:
        more : List<&2, R.Bind> <- binds(pos, rest)
        return R.BVar{name} <> more
    case Con{A.PWild{}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Bind>>:
        more : List<&2, R.Bind> <- binds(pos, rest)
        return R.BSkip{} <> more
    case Con{other, rest}:
      Fail{Refusal{pos, "constructor field pattern other than a name or `_`"}}

def pat(pos: A.Pos, p: A.Pat) -> Result<&2, &2, Refusal, R.Pat>:
  match p:
    case A.PPath{Con{A.Seg{family, Nil{}}, Con{A.Seg{ctor, Nil{}}, Nil{}}}}:
      Done{R.PCtor{family, ctor, []}}
    case A.PTupleStruct{Con{A.Seg{family, Nil{}}, Con{A.Seg{ctor, Nil{}}, Nil{}}}, Con{b, bs}}:
      do Result<&2, &2, Refusal, R.Pat>:
        rbs : List<&2, R.Bind> <- binds(pos, Con{b, bs})
        return R.PCtor{family, ctor, rbs}
    case other:
      Fail{Refusal{pos, "pattern other than `Family::Ctor` or `Family::Ctor(x, _)`"}}

def no_guard(pos: A.Pos, g: Maybe<&2, A.Node>) -> Result<&2, &2, Refusal, Bool>:
  match g:
    case None{}:
      Done{True{}}
    case Some{x}:
      Fail{Refusal{pos, "match guard"}}

def arm_pats(xs: List<&2, A.Arm<A.Node>>) -> Result<&2, &2, Refusal, List<&2, R.Pat>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.Arm{+meta, p, guard, body}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Pat>>:
        ok : Bool <- plain(meta)
        g : Bool <- no_guard(A.meta_pos(meta), guard)
        rp : R.Pat <- pat(A.meta_pos(meta), p)
        more : List<&2, R.Pat> <- arm_pats(rest)
        return rp <> more

# ---- expressions ----

# The head of one expression: the string tests of `walk`. `KIf` holds the
# statements of the two branches.
type K is Data:
  KVar{name: String}
  KInt{text: String}
  KBool{value: Bool}
  KCall{func: String, targs: List<&2, A.Ty>, args: List<&2, A.Node>}
  KCtor{family: String, ctor: String, args: List<&2, A.Node>}
  KMatch{scrut: A.Node, arms: List<&2, A.Arm<A.Node>>}
  KIf{cond: A.Node, yes: List<&2, A.Stmt<A.Node>>, no: List<&2, A.Stmt<A.Node>>}
  KClosure{params: List<&2, A.CParam>, body: A.Node}
  KBorrow{inner: A.Node}
  KClone{inner: A.Node}
  KBox{inner: A.Node}
  KLet{name: String, ty: Maybe<&2, A.Ty>, init: A.Node, rest: List<&2, A.Stmt<A.Node>>}
  KBlock{stmts: List<&2, A.Stmt<A.Node>>}
  KBad{what: String}

def box_kind(args: List<&2, A.Node>) -> K:
  match args:
    case Con{a, Nil{}}:
      KBox{a}
    case other:
      KBad{"`Box::new` with other than one argument"}

def ctor_kind(family: String, ctor: String, args: List<&2, A.Node>) -> K:
  match args:
    case Nil{}:
      KBad{"constructor call with no argument"}
    case Con{a, rest}:
      KCtor{family, ctor, Con{a, rest}}

def closure_kind(ps: List<&2, A.CParam>, r: Maybe<&2, A.Ty>, body: A.Node) -> K:
  match r:
    case None{}:
      KClosure{ps, body}
    case Some{t}:
      KBad{"closure with a return type"}

def kind(n: A.Node) -> K:
  match n:
    case A.EPath{Con{A.Seg{name, Nil{}}, Nil{}}}:
      KVar{name}
    case A.EPath{Con{A.Seg{family, Nil{}}, Con{A.Seg{ctor, Nil{}}, Nil{}}}}:
      KCtor{family, ctor, []}
    case A.EPath{segs}:
      KBad{"path other than `name` or `Family::Ctor`"}
    case A.ELit{A.LInt{text}}:
      KInt{text}
    case A.ELit{A.LBool{value}}:
      KBool{value}
    case A.ELit{lit}:
      KBad{"string or char literal"}
    case A.ECall{A.EPath{Con{A.Seg{func, targs}, Nil{}}}, args}:
      KCall{func, targs, args}
    case A.ECall{A.EPath{Con{A.Seg{+family, Nil{}}, Con{A.Seg{+ctor, Nil{}}, Nil{}}}}, +args}:
      Bool.pick(K, String.eq(family, "Box") && String.eq(ctor, "new"), box_kind(args), ctor_kind(family, ctor, args))
    case A.ECall{func, args}:
      KBad{"call of an expression other than a fn or constructor path"}
    case A.EMethod{recv, +name, Nil{}, Nil{}}:
      Bool.pick(K, String.eq(name, "clone"), KClone{recv}, KBad{"method call `." ++ name ++ "()`"})
    case A.EMethod{recv, name, gs, args}:
      KBad{"method call `." ++ name ++ "`"}
    case A.EUnary{+op, inner}:
      Bool.pick(K, String.eq(op, "&"), KBorrow{inner}, KBad{"unary operator `" ++ op ++ "`"})
    case A.EClosure{mv, +ps, +r, +body}:
      Bool.pick(K, mv, KBad{"`move` closure"}, closure_kind(ps, r, body))
    case A.EIf{cond, yes, Some{A.EBlock{no}}}:
      KIf{cond, yes, no}
    case A.EIf{cond, yes, None{}}:
      KBad{"`if` without `else`"}
    case A.EIf{cond, yes, els}:
      KBad{"`else if`"}
    case A.EMatch{scrut, arms}:
      KMatch{scrut, arms}
    case A.EField{recv, name}:
      KBad{"field access"}
    case A.ETry{inner}:
      KBad{"`?` operator"}
    case A.EBinary{op, lhs, rhs}:
      KBad{"binary operator `" ++ op ++ "`"}
    case A.EBlock{Con{A.SLet{meta, p, t, init}, rest}}:
      KBlock{Con{A.SLet{meta, p, t, init}, rest}}
    case A.EBlock{stmts}:
      KBad{"block"}
    case A.ELet{p, init}:
      KBad{"`let` expression"}
    case A.EStruct{segs, fields, base}:
      KBad{"struct expression"}
    case A.ETuple{items}:
      KBad{"tuple expression"}
    case A.EParen{inner}:
      KBad{"parenthesized expression"}
    case A.EMacro{name, bracket, args}:
      KBad{"macro call `" ++ name ++ "!`"}
    case other:
      KBad{"item in an expression"}

# One expression to lift, with the position of its nearest statement, arm or item.
type Job is Data:
  Job{at: A.Pos, k: K}

def jobs(+at: A.Pos, ns: List<&2, A.Node>) -> List<&2, Job>:
  match ns:
    case Nil{}:
      []
    case Con{n, rest}:
      Job{at, kind(n)} <> jobs(at, rest)

def stmt_refusal(s: A.Stmt<A.Node>) -> Refusal:
  match s:
    case A.SLet{meta, p, t, init}:
      Refusal{A.meta_pos(meta), "`let` statement"}
    case A.SExpr{meta, e, semi}:
      Refusal{A.meta_pos(meta), "statement before the tail expression"}
    case A.SItem{item}:
      Refusal{A.item_pos(item), "item in a block"}

# A `let` statement before the tail (E1): `let name: T = init; rest` with a
# plain name, an initializer and a tail after it.  Lifted to `KLet`.
def let_rest(at: A.Pos, meta: A.Meta, name: String, t: Maybe<&2, A.Ty>, v: A.Node,
    rest: List<&2, A.Stmt<A.Node>>) -> Result<&2, &2, Refusal, Job>:
  match rest:
    case Nil{}:
      Fail{Refusal{at, "`let` with no tail expression"}}
    case Con{s, more}:
      do Result<&2, &2, Refusal, Job>:
        ok : Bool <- plain(meta)
        return Job{at, KLet{name, t, v, Con{s, more}}}

def let_init(at: A.Pos, meta: A.Meta, name: String, t: Maybe<&2, A.Ty>, init: Maybe<&2, A.Node>,
    rest: List<&2, A.Stmt<A.Node>>) -> Result<&2, &2, Refusal, Job>:
  match init:
    case None{}:
      Fail{Refusal{at, "`let` without an initializer"}}
    case Some{v}:
      let_rest(at, meta, name, t, v, rest)

def let_job(at: A.Pos, meta: A.Meta, p: A.Pat, t: Maybe<&2, A.Ty>, init: Maybe<&2, A.Node>,
    rest: List<&2, A.Stmt<A.Node>>) -> Result<&2, &2, Refusal, Job>:
  match p:
    case A.PPath{Con{A.Seg{name, Nil{}}, Nil{}}}:
      let_init(at, meta, name, t, init, rest)
    case other:
      Fail{Refusal{at, "`let` with a pattern"}}

# The inverse of `Em.tail`: a block that is one tail expression, or lets
# before it.
def tail_job(pos: A.Pos, stmts: List<&2, A.Stmt<A.Node>>) -> Result<&2, &2, Refusal, Job>:
  match stmts:
    case Con{A.SExpr{+meta, e, semi}, Nil{}}:
      do Result<&2, &2, Refusal, Job>:
        ok : Bool <- plain(meta)
        Bool.pick(Result<&2, &2, Refusal, Job>, semi, Fail{Refusal{A.meta_pos(meta), "expression statement"}},
          Done{Job{A.meta_pos(meta), kind(e)}})
    case Con{A.SLet{+meta, p, t, init}, rest}:
      let_job(A.meta_pos(meta), meta, p, t, init, rest)
    case Con{s, rest}:
      Fail{stmt_refusal(s)}
    case Nil{}:
      Fail{Refusal{pos, "empty block"}}

def if_only(pos: A.Pos, j: Job) -> Result<&2, &2, Refusal, Job>:
  match j:
    case Job{at, KIf{c, y, n}}:
      Done{Job{at, KIf{c, y, n}}}
    case other:
      Fail{Refusal{pos, "block in a match arm"}}

# The inverse of `Em.arm_body`: a block in an arm body holds an `if`.
def arm_job(+pos: A.Pos, body: A.Node) -> Result<&2, &2, Refusal, Job>:
  match body:
    case A.EBlock{stmts}:
      do Result<&2, &2, Refusal, Job>:
        j : Job <- tail_job(pos, stmts)
        if_only(pos, j)
    case other:
      Done{Job{pos, kind(other)}}

def arm_jobs(xs: List<&2, A.Arm<A.Node>>) -> Result<&2, &2, Refusal, List<&2, Job>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.Arm{meta, p, guard, body}, rest}:
      do Result<&2, &2, Refusal, List<&2, Job>>:
        j : Job <- arm_job(A.meta_pos(meta), body)
        more : List<&2, Job> <- arm_jobs(rest)
        return j <> more

def one(xs: List<&2, R.Ex>) -> R.Ex:
  match xs:
    case Nil{}:
      R.XBool{False{}}
    case Con{x, rest}:
      x

# The type of a let binder: `None` when the Rust text gives none (E1).
def let_ty(pos: A.Pos, t: Maybe<&2, A.Ty>) -> Result<&2, &2, Refusal, Maybe<&2, R.Ty>>:
  match t:
    case None{}:
      Done{None{}}
    case Some{a}:
      do Result<&2, &2, Refusal, Maybe<&2, R.Ty>>:
        r : R.Ty <- ty(pos, a)
        return Some{r}

def if_of(xs: List<&2, R.Ex>) -> R.Ex:
  match xs:
    case Con{c, Con{y, Con{n, rest}}}:
      R.XIf{c, y, n}
    case other:
      R.XBool{False{}}

type Zip is Data:
  Zip{pats: List<&2, R.Pat>, bodies: List<&2, R.Ex>}

@unsafe def zip_arms(z: Zip) -> List<&2, R.Arm<R.Ex>>:
  match z:
    case Zip{Con{p, ps}, Con{b, bs}}:
      R.Arm{p, b} <> zip_arms(Zip{ps, bs})
    case other:
      []

@unsafe def walk(xs: List<&2, Job>) -> Result<&2, &2, Refusal, List<&2, R.Ex>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{Job{at, KVar{name}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        more : List<&2, R.Ex> <- walk(rest)
        return R.XVar{name} <> more
    case Con{Job{at, KInt{text}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        more : List<&2, R.Ex> <- walk(rest)
        return R.XInt{text} <> more
    case Con{Job{at, KBool{value}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        more : List<&2, R.Ex> <- walk(rest)
        return R.XBool{value} <> more
    case Con{Job{+at, KCall{func, targs, args}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ts : List<&2, R.Ty> <- tys(at, targs)
        ys : List<&2, R.Ex> <- walk(jobs(at, args))
        more : List<&2, R.Ex> <- walk(rest)
        return R.XCall{func, ts, ys} <> more
    case Con{Job{+at, KCtor{family, ctor, args}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ys : List<&2, R.Ex> <- walk(jobs(at, args))
        more : List<&2, R.Ex> <- walk(rest)
        return R.XCtor{family, ctor, ys} <> more
    case Con{Job{+at, KMatch{scrut, +arms}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ss : List<&2, R.Ex> <- walk([Job{at, kind(scrut)}])
        ps : List<&2, R.Pat> <- arm_pats(arms)
        bj : List<&2, Job> <- arm_jobs(arms)
        bs : List<&2, R.Ex> <- walk(bj)
        more : List<&2, R.Ex> <- walk(rest)
        return R.XMatch{one(ss), zip_arms(Zip{ps, bs})} <> more
    case Con{Job{+at, KIf{cond, yes, no}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        yj : Job <- tail_job(at, yes)
        nj : Job <- tail_job(at, no)
        parts : List<&2, R.Ex> <- walk([Job{at, kind(cond)}, yj, nj])
        more : List<&2, R.Ex> <- walk(rest)
        return if_of(parts) <> more
    case Con{Job{+at, KClosure{ps, body}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        rps : List<&2, R.Param> <- cparams(at, ps)
        bs : List<&2, R.Ex> <- walk([Job{at, kind(body)}])
        more : List<&2, R.Ex> <- walk(rest)
        return R.XClosure{rps, one(bs)} <> more
    case Con{Job{+at, KBorrow{inner}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ys : List<&2, R.Ex> <- walk([Job{at, kind(inner)}])
        more : List<&2, R.Ex> <- walk(rest)
        return R.XBorrow{one(ys)} <> more
    case Con{Job{+at, KClone{inner}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ys : List<&2, R.Ex> <- walk([Job{at, kind(inner)}])
        more : List<&2, R.Ex> <- walk(rest)
        return R.XClone{one(ys)} <> more
    case Con{Job{+at, KBox{inner}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        ys : List<&2, R.Ex> <- walk([Job{at, kind(inner)}])
        more : List<&2, R.Ex> <- walk(rest)
        return R.XBox{one(ys)} <> more
    case Con{Job{+at, KLet{name, t, init, stmts}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        rt : Maybe<&2, R.Ty> <- let_ty(at, t)
        vs : List<&2, R.Ex> <- walk([Job{at, kind(init)}])
        bj : Job <- tail_job(at, stmts)
        bs : List<&2, R.Ex> <- walk([bj])
        more : List<&2, R.Ex> <- walk(rest)
        return R.XLet{name, rt, one(vs), one(bs)} <> more
    case Con{Job{+at, KBlock{stmts}}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ex>>:
        j : Job <- tail_job(at, stmts)
        walk(j <> rest)
    case Con{Job{at, KBad{what}}, rest}:
      Fail{Refusal{at, what}}

# ---- items ----

def use_module(+pos: A.Pos, tree: A.UseTree) -> Result<&2, &2, Refusal, String>:
  match tree:
    case A.UPath{c, A.UPath{m, A.UGlob{}}}:
      Bool.pick(Result<&2, &2, Refusal, String>, String.eq(c, "crate"), Done{m},
        Fail{Refusal{pos, "`use` other than `use crate::<module>::*;`"}})
    case other:
      Fail{Refusal{pos, "`use` other than `use crate::<module>::*;`"}}

def enum_gens(+pos: A.Pos, gs: List<&2, A.GParam>) -> Result<&2, &2, Refusal, List<&2, String>>:
  match gs:
    case Nil{}:
      Done{[]}
    case Con{A.GParam{name, Nil{}}, rest}:
      do Result<&2, &2, Refusal, List<&2, String>>:
        more : List<&2, String> <- enum_gens(pos, rest)
        return name <> more
    case Con{A.GParam{name, bs}, rest}:
      Fail{Refusal{pos, "bound on the enum generic `" ++ name ++ "`"}}

def clone_bound(pos: A.Pos, b: String) -> Result<&2, &2, Refusal, Bool>:
  Bool.pick(Result<&2, &2, Refusal, Bool>, String.eq(b, "Clone"), Done{True{}},
    Fail{Refusal{pos, "fn generic bound other than `Clone`"}})

# The inverse of `Em.bounds(gs, [Clone])`.
def fn_gens(+pos: A.Pos, gs: List<&2, A.GParam>) -> Result<&2, &2, Refusal, List<&2, String>>:
  match gs:
    case Nil{}:
      Done{[]}
    case Con{A.GParam{name, Con{A.TPath{Con{A.Seg{b, Nil{}}, Nil{}}}, Nil{}}}, rest}:
      do Result<&2, &2, Refusal, List<&2, String>>:
        ok : Bool <- clone_bound(pos, b)
        more : List<&2, String> <- fn_gens(pos, rest)
        return name <> more
    case Con{A.GParam{name, bs}, rest}:
      Fail{Refusal{pos, "fn generic `" ++ name ++ "` without exactly the bound `Clone`"}}

def fields(xs: List<&2, A.Field>) -> Result<&2, &2, Refusal, List<&2, R.Ty>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.TField{+meta, vis, t}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Ty>>:
        ok : Bool <- plain(meta)
        v : Bool <- is_priv(A.meta_pos(meta), vis)
        rt : R.Ty <- ty(A.meta_pos(meta), t)
        more : List<&2, R.Ty> <- fields(rest)
        return rt <> more
    case Con{A.NField{meta, vis, name, t}, rest}:
      Fail{Refusal{A.meta_pos(meta), "named field"}}

def variant_fields(pos: A.Pos, body: A.Body) -> Result<&2, &2, Refusal, List<&2, R.Ty>>:
  match body:
    case A.BUnit{}:
      Done{[]}
    case A.BTuple{Con{f, fs}}:
      fields(Con{f, fs})
    case A.BTuple{Nil{}}:
      Fail{Refusal{pos, "variant with empty parentheses"}}
    case A.BNamed{fs}:
      Fail{Refusal{pos, "variant with named fields"}}

def variants(xs: List<&2, A.Variant>) -> Result<&2, &2, Refusal, List<&2, R.Variant>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.Variant{+meta, name, body}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Variant>>:
        ok : Bool <- plain(meta)
        fs : List<&2, R.Ty> <- variant_fields(A.meta_pos(meta), body)
        more : List<&2, R.Variant> <- variants(rest)
        return R.Variant{name, fs} <> more

def ret_ty(pos: A.Pos, r: Maybe<&2, A.Ty>) -> Result<&2, &2, Refusal, R.Ty>:
  match r:
    case Some{t}:
      ty(pos, t)
    case None{}:
      Fail{Refusal{pos, "fn without a return type"}}

def fn_body(pos: A.Pos, b: Maybe<&2, List<&2, A.Stmt<A.Node>>>) -> Result<&2, &2, Refusal, Job>:
  match b:
    case Some{stmts}:
      tail_job(pos, stmts)
    case None{}:
      Fail{Refusal{pos, "fn without a body"}}

# The inverse of `Em.fn_item`.
def fn_item(+meta: A.Meta, vis: A.Vis, sig: A.Sig, body: Maybe<&2, List<&2, A.Stmt<A.Node>>>) -> Result<&2, &2, Refusal, R.Item>:
  match sig:
    case A.Sig{name, gens, ps, ret}:
      do Result<&2, &2, Refusal, R.Item>:
        ok : Bool <- plain(meta)
        v : Bool <- is_pub(A.meta_pos(meta), vis)
        gs : List<&2, String> <- fn_gens(A.meta_pos(meta), gens)
        rps : List<&2, R.Param> <- params(A.meta_pos(meta), ps)
        rt : R.Ty <- ret_ty(A.meta_pos(meta), ret)
        b : Job <- fn_body(A.meta_pos(meta), body)
        es : List<&2, R.Ex> <- walk([b])
        return R.IFn{name, gs, rps, rt, one(es)}

def unit_copy(pos: A.Pos, cp: Bool) -> Result<&2, &2, Refusal, Bool>:
  Bool.pick(Result<&2, &2, Refusal, Bool>, cp, Done{True{}}, Fail{Refusal{pos, "unit struct without `Copy` in its derive list"}})

def body_ok(pos: A.Pos, +name: String, body: A.Body) -> Result<&2, &2, Refusal, Bool>:
  match body:
    case A.BUnit{}:
      Done{True{}}
    case A.BTuple{fs}:
      Fail{Refusal{pos, "struct `" ++ name ++ "` with fields"}}
    case A.BNamed{fs}:
      Fail{Refusal{pos, "struct `" ++ name ++ "` with fields"}}

# A struct of this fragment is a unit struct: no generics and no fields.
def unit_body(pos: A.Pos, +name: String, gs: List<&2, A.GParam>, body: A.Body) -> Result<&2, &2, Refusal, Bool>:
  match gs:
    case Nil{}:
      body_ok(pos, name, body)
    case Con{g, rest}:
      Fail{Refusal{pos, "struct `" ++ name ++ "` with generics"}}

# The constructor fn of the unit struct `name`: `fn <ctor>() -> name { name }`.
def ctor_of(pos: A.Pos, +name: String, f: R.Item) -> Result<&2, &2, Refusal, String>:
  match f:
    case R.IFn{ctor, Nil{}, Nil{}, R.TCon{r, Nil{}}, R.XVar{b}}:
      Bool.pick(Result<&2, &2, Refusal, String>, String.eq(r, name) && String.eq(b, name), Done{ctor},
        Fail{Refusal{pos, "unit struct `" ++ name ++ "` without its constructor fn"}})
    case other:
      Fail{Refusal{pos, "unit struct `" ++ name ++ "` without its constructor fn"}}

@unsafe def items(xs: List<&2, A.Node>) -> Result<&2, &2, Refusal, List<&2, R.Item>>:
  match xs:
    case Nil{}:
      Done{[]}
    case Con{A.IUse{+meta, vis, tree}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Item>>:
        ok : Bool <- plain(meta)
        v : Bool <- is_priv(A.meta_pos(meta), vis)
        m : String <- use_module(A.meta_pos(meta), tree)
        more : List<&2, R.Item> <- items(rest)
        return R.IUse{m} <> more
    case Con{A.IEnum{+meta, vis, name, gens, vs}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Item>>:
        cp : Bool <- derived(meta)
        v : Bool <- is_pub(A.meta_pos(meta), vis)
        gs : List<&2, String> <- enum_gens(A.meta_pos(meta), gens)
        rvs : List<&2, R.Variant> <- variants(vs)
        more : List<&2, R.Item> <- items(rest)
        return R.IEnum{name, gs, cp, rvs} <> more
    case Con{A.IStruct{+meta, vis, +name, gens, body}, Con{A.IFn{fm, fv, fs, fb}, rest}}:
      do Result<&2, &2, Refusal, List<&2, R.Item>>:
        u : Bool <- unit_body(A.meta_pos(meta), name, gens, body)
        cp : Bool <- derived(meta)
        c : Bool <- unit_copy(A.meta_pos(meta), cp)
        v : Bool <- is_pub(A.meta_pos(meta), vis)
        f : R.Item <- fn_item(fm, fv, fs, fb)
        ctor : String <- ctor_of(A.meta_pos(meta), name, f)
        more : List<&2, R.Item> <- items(rest)
        return R.IUnit{name, ctor} <> more
    case Con{A.IStruct{+meta, vis, +name, gens, body}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Item>>:
        u : Bool <- unit_body(A.meta_pos(meta), name, gens, body)
        none : List<&2, R.Item> <- Fail{Refusal{A.meta_pos(meta), "unit struct `" ++ name ++ "` without its constructor fn"}}
        return none
    case Con{A.IFn{meta, vis, sig, body}, rest}:
      do Result<&2, &2, Refusal, List<&2, R.Item>>:
        f : R.Item <- fn_item(meta, vis, sig, body)
        more : List<&2, R.Item> <- items(rest)
        return f <> more
    case Con{+other, rest}:
      Fail{Refusal{A.item_pos(other), "item `" ++ A.item_line(other) ++ "`"}}

# ---- file ----

# `s` without the prefix `pre`, or None.
def strip(pre: String, s: String) -> Maybe<&2, String>:
  match pre s:
    case SNil{} rest:
      Some{rest}
    case SCon{p, ps} SCon{c, cs}:
      Bool.pick(Maybe<&2, String>, String.eq(SCon{p, SNil{}}, SCon{c, SNil{}}), strip(ps, cs), None{})
    case a b:
      None{}

def unreversed(m: Maybe<&2, String>) -> String:
  match m:
    case Some{r}:
      String.reverse(r)
    case None{}:
      ""

def source_in(m: Maybe<&2, String>) -> String:
  match m:
    case Some{rest}:
      unreversed(strip(".`", String.reverse(rest)))
    case None{}:
      ""

# The inverse of the `//!` line of `Em.lower`. Any other `//!` text gives "":
# CD4 then names the file `<module>.mech` (C2).
def source_of(inner: List<&2, String>) -> String:
  match inner:
    case Con{line, Nil{}}:
      source_in(strip(" Emitted by `mech rust-out` from `", line))
    case other:
      ""

def lift(f: A.SrcFile) -> Result<&2, &2, Refusal, R.Module>:
  match f:
    case A.SrcFile{inner, its}:
      do Result<&2, &2, Refusal, R.Module>:
        xs : List<&2, R.Item> <- items(its)
        return R.Module{source_of(inner), xs}
