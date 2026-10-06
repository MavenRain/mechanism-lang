import Base
import ./ast.bend as A
import ./rir.bend as R
import ./print.bend as Pr

# Lowers the erased program IR (rir.bend) to the Rust AST of the frontend
# and prints it with print.bend, which has the rustfmt layout rules. Bend 2
# has no mutual recursion, so each recursive walker takes a list, and a
# single value goes through it as a list of one element.

# An emitted construct has no source text, so each `Meta` has the position 0:0.
def m0() -> A.Meta:
  A.Meta{False{}, [], [], A.no_pos()}

def gap(ats: List<&2, A.Attr>) -> A.Meta:
  A.Meta{True{}, [], ats, A.no_pos()}

def path1(a: String) -> List<&2, A.Seg<A.Ty>>:
  [A.Seg{a, []}]

def path2(a: String, b: String) -> List<&2, A.Seg<A.Ty>>:
  [A.Seg{a, []}, A.Seg{b, []}]

def one_ty(xs: List<&2, A.Ty>) -> A.Ty:
  match xs:
    case Nil{}:
      A.TInfer{}
    case Con{x, rest}:
      x

def one(xs: List<&2, A.Node>) -> A.Node:
  match xs:
    case Nil{}:
      A.ETuple{[]}
    case Con{x, rest}:
      x

@unsafe def tys(xs: List<&2, R.Ty>) -> List<&2, A.Ty>:
  match xs:
    case Nil{}:
      []
    case Con{R.TBool{}, rest}:
      A.TPath{path1("bool")} <> tys(rest)
    case Con{R.TCon{name, args}, rest}:
      A.TPath{[A.Seg{name, tys(args)}]} <> tys(rest)
    case Con{R.TBox{i}, rest}:
      A.TPath{[A.Seg{"Box", tys([i])}]} <> tys(rest)
    case Con{R.TRef{i}, rest}:
      A.TRef{one_ty(tys([i]))} <> tys(rest)
    case Con{R.TFn{args, ret}, rest}:
      A.TImpl{[A.TFnSugar{"Fn", tys(args), Some{one_ty(tys([ret]))}}]} <> tys(rest)

def ty(t: R.Ty) -> A.Ty:
  one_ty(tys([t]))

def name_pat(name: String) -> A.Pat:
  A.PPath{path1(name)}

def params(xs: List<&2, R.Param>) -> List<&2, A.Param>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      A.PTyped{name_pat(name), ty(t)} <> params(rest)

def cparams(xs: List<&2, R.Param>) -> List<&2, A.CParam>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      A.CParam{name_pat(name), Some{ty(t)}} <> cparams(rest)

def binds(xs: List<&2, R.Bind>) -> List<&2, A.Pat>:
  match xs:
    case Nil{}:
      []
    case Con{R.BVar{name}, rest}:
      name_pat(name) <> binds(rest)
    case Con{R.BSkip{}, rest}:
      A.PWild{} <> binds(rest)

# The field names of a struct pattern beside its binds.
type FB is Data:
  FB{names: List<&2, String>, binds: List<&2, R.Bind>}

# `a` when the bind is the field name, else `a: x` or `a: _` (E2).
def field_pat(+n: String, +x: String) -> A.FieldPat<A.Pat>:
  A.FieldPat{n, Bool.pick(Maybe<&2, A.Pat>, String.eq(n, x), None{}, Some{name_pat(x)})}

@unsafe def field_pats(t: FB) -> List<&2, A.FieldPat<A.Pat>>:
  match t:
    case FB{Con{n, ns}, Con{R.BVar{x}, bs}}:
      field_pat(n, x) <> field_pats(FB{ns, bs})
    case FB{Con{n, ns}, Con{R.BSkip{}, bs}}:
      A.FieldPat{n, Some{A.PWild{}}} <> field_pats(FB{ns, bs})
    case other:
      []

def struct_pat(+family: String, named: Bool, fs: List<&2, String>, +bs: List<&2, R.Bind>) -> A.Pat:
  Bool.pick(A.Pat, named, A.PStruct{path1(family), field_pats(FB{fs, bs}), False{}}, A.PTupleStruct{path1(family), binds(bs)})

def pat(p: R.Pat) -> A.Pat:
  match p:
    case R.PCtor{family, ctor, Nil{}}:
      A.PPath{path2(family, ctor)}
    case R.PCtor{family, ctor, bs}:
      A.PTupleStruct{path2(family, ctor), binds(bs)}
    case R.PStruct{family, named, fs, bs}:
      struct_pat(family, named, fs, bs)

def arm_pats(xs: List<&2, R.Arm<R.Ex>>) -> List<&2, A.Pat>:
  match xs:
    case Nil{}:
      []
    case Con{R.Arm{p, body}, rest}:
      pat(p) <> arm_pats(rest)

def arm_bodies(xs: List<&2, R.Arm<R.Ex>>) -> List<&2, R.Ex>:
  match xs:
    case Nil{}:
      []
    case Con{R.Arm{p, body}, rest}:
      body <> arm_bodies(rest)

# A block with `e` as its tail expression. A let in tail position gives its
# statements, so `fn f() -> T { let x: T = v; x }` has no inner block (E1).
def tail(e: A.Node) -> List<&2, A.Stmt<A.Node>>:
  match e:
    case A.EBlock{stmts}:
      stmts
    case other:
      [A.SExpr{m0(), other, False{}}]

def let_ty(t: Maybe<&2, R.Ty>) -> Maybe<&2, A.Ty>:
  match t:
    case None{}:
      None{}
    case Some{x}:
      Some{ty(x)}

# rustfmt puts an `if` that is an arm body in a block.
def arm_body(e: A.Node) -> A.Node:
  match e:
    case A.EIf{c, b, x}:
      A.EBlock{tail(A.EIf{c, b, x})}
    case other:
      other

# The patterns and the lowered bodies of the arms of one match.
type Two is Data:
  Two{pats: List<&2, A.Pat>, bodies: List<&2, A.Node>}

@unsafe def zip_arms(t: Two) -> List<&2, A.Arm<A.Node>>:
  match t:
    case Two{Con{p, ps}, Con{b, bs}}:
      A.Arm{m0(), p, None{}, arm_body(b)} <> zip_arms(Two{ps, bs})
    case other:
      []

@unsafe def nodes(xs: List<&2, R.Ex>) -> List<&2, A.Node>:
  match xs:
    case Nil{}:
      []
    case Con{R.XVar{name}, rest}:
      A.EPath{path1(name)} <> nodes(rest)
    case Con{R.XInt{text}, rest}:
      A.ELit{A.LInt{text}} <> nodes(rest)
    case Con{R.XBool{value}, rest}:
      A.ELit{A.LBool{value}} <> nodes(rest)
    case Con{R.XCall{func, targs, args}, rest}:
      A.ECall{A.EPath{[A.Seg{func, tys(targs)}]}, nodes(args)} <> nodes(rest)
    case Con{R.XCtor{family, ctor, Nil{}}, rest}:
      A.EPath{path2(family, ctor)} <> nodes(rest)
    case Con{R.XCtor{family, ctor, args}, rest}:
      A.ECall{A.EPath{path2(family, ctor)}, nodes(args)} <> nodes(rest)
    case Con{R.XMatch{scrut, +arms}, rest}:
      A.EMatch{one(nodes([scrut])), zip_arms(Two{arm_pats(arms), nodes(arm_bodies(arms))})} <> nodes(rest)
    case Con{R.XIf{c, yes, no}, rest}:
      A.EIf{one(nodes([c])), tail(one(nodes([yes]))), Some{A.EBlock{tail(one(nodes([no])))}}} <> nodes(rest)
    case Con{R.XClosure{ps, body}, rest}:
      A.EClosure{False{}, cparams(ps), None{}, one(nodes([body]))} <> nodes(rest)
    case Con{R.XBorrow{i}, rest}:
      A.EUnary{"&", one(nodes([i]))} <> nodes(rest)
    case Con{R.XClone{i}, rest}:
      A.EMethod{one(nodes([i])), "clone", [], []} <> nodes(rest)
    case Con{R.XBox{i}, rest}:
      A.ECall{A.EPath{path2("Box", "new")}, nodes([i])} <> nodes(rest)
    case Con{R.XLet{name, t, value, body}, rest}:
      A.EBlock{A.SLet{m0(), name_pat(name), let_ty(t), Some{one(nodes([value]))}} <> tail(one(nodes([body])))} <> nodes(rest)

def ex(e: R.Ex) -> A.Node:
  one(nodes([e]))

def derive(copy: Bool) -> List<&2, A.Attr>:
  [A.Attr{"derive", Some{Bool.pick(List<&2, String>, copy, ["Clone", "Copy", "Debug", "PartialEq", "Eq"], ["Clone", "Debug", "PartialEq", "Eq"])}}]

# One generic parameter for each name, each with the bounds `bs`.
def bounds(xs: List<&2, String>, +bs: List<&2, A.Ty>) -> List<&2, A.GParam>:
  match xs:
    case Nil{}:
      []
    case Con{x, rest}:
      A.GParam{x, bs} <> bounds(rest, bs)

def fields(xs: List<&2, A.Ty>) -> List<&2, A.Field>:
  match xs:
    case Nil{}:
      []
    case Con{t, rest}:
      A.TField{m0(), A.Priv{}, t} <> fields(rest)

def variants(xs: List<&2, R.Variant>) -> List<&2, A.Variant>:
  match xs:
    case Nil{}:
      []
    case Con{R.Variant{name, Nil{}}, rest}:
      A.Variant{m0(), name, A.BUnit{}} <> variants(rest)
    case Con{R.Variant{name, fs}, rest}:
      A.Variant{m0(), name, A.BTuple{fields(tys(fs))}} <> variants(rest)

def fn_item(name: String, gs: List<&2, String>, ps: List<&2, R.Param>, ret: A.Ty, body: A.Node) -> A.Node:
  A.IFn{gap([]), A.Pub{}, A.Sig{name, bounds(gs, [A.TPath{path1("Clone")}]), params(ps), Some{ret}}, Some{tail(body)}}

def nfields(xs: List<&2, R.Param>) -> List<&2, A.Field>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      A.NField{m0(), A.Priv{}, name, ty(t)} <> nfields(rest)

def param_tys(xs: List<&2, R.Param>) -> List<&2, R.Ty>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      t <> param_tys(rest)

def inits(xs: List<&2, R.Param>) -> List<&2, A.FieldInit<A.Node>>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      A.FieldInit{name, None{}} <> inits(rest)

def args(xs: List<&2, R.Param>) -> List<&2, A.Node>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      A.EPath{path1(name)} <> args(rest)

# `pub struct S { a: A }` or `pub struct N(A);`, private fields (E2).
def struct_body(named: Bool, +fs: List<&2, R.Param>) -> A.Body:
  Bool.pick(A.Body, named, A.BNamed{nfields(fs)}, A.BTuple{fields(tys(param_tys(fs)))})

# `S { a, b }` or `N(x0, x1)`.
def struct_new(+name: String, named: Bool, +fs: List<&2, R.Param>) -> A.Node:
  Bool.pick(A.Node, named, A.EStruct{path1(name), inits(fs), None{}}, A.ECall{A.EPath{path1(name)}, args(fs)})

def getter_body(+name: String, t: R.Ty) -> A.Node:
  match t:
    case R.TRef{i}:
      A.EUnary{"&", A.EField{A.EPath{path1("self")}, name}}
    case other:
      A.EField{A.EPath{path1("self")}, name}

# `pub fn a(&self) -> &A { &self.a }`, or `-> A { self.a }` for a Copy field.
def getter(m: A.Meta, +name: String, +t: R.Ty) -> A.Node:
  A.IFn{m, A.Pub{}, A.Sig{name, [], [A.PSelf{True{}}], Some{ty(t)}}, Some{tail(getter_body(name, t))}}

# No blank line before the first accessor of the `impl` block.
def getters(xs: List<&2, R.Param>, b: Bool) -> List<&2, A.Node>:
  match xs:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      getter(A.Meta{b, [], [], A.no_pos()}, name, t) <> getters(rest, True{})

# One `impl` block with the accessors, before `tl`; a tuple struct has none.
def impl_block(name: String, gs: List<&2, R.Param>, tl: List<&2, A.Node>) -> List<&2, A.Node>:
  match gs:
    case Nil{}:
      tl
    case Con{g, rest}:
      A.IImpl{gap([]), [], None{}, A.TPath{path1(name)}, getters(g <> rest, False{})} <> tl

# `use_gap`: a blank line goes before the next `use` (not between two of them).
def items(xs: List<&2, R.Item>, use_gap: Bool) -> List<&2, A.Node>:
  match xs:
    case Nil{}:
      []
    case Con{R.IUse{m}, rest}:
      A.IUse{A.Meta{use_gap, [], [], A.no_pos()}, A.Priv{}, A.UPath{"crate", A.UPath{m, A.UGlob{}}}} <> items(rest, False{})
    case Con{R.IEnum{name, gs, copy, vs}, rest}:
      A.IEnum{gap(derive(copy)), A.Pub{}, name, bounds(gs, []), variants(vs)} <> items(rest, True{})
    case Con{R.IUnit{+name, ctor}, rest}:
      A.IStruct{gap(derive(True{})), A.Pub{}, name, [], A.BUnit{}} <>
        (fn_item(ctor, [], [], A.TPath{path1(name)}, A.EPath{path1(name)}) <> items(rest, True{}))
    case Con{R.IStruct{+name, ctor, +named, +fs, gs}, rest}:
      A.IStruct{gap(derive(False{})), A.Pub{}, name, [], struct_body(named, fs)} <>
        (fn_item(ctor, [], fs, A.TPath{path1(name)}, struct_new(name, named, fs)) <> impl_block(name, gs, items(rest, True{})))
    case Con{R.IFn{name, gs, ps, ret, body}, rest}:
      fn_item(name, gs, ps, ty(ret), ex(body)) <> items(rest, True{})

def lower(m: R.Module) -> A.SrcFile:
  match m:
    case R.Module{source, its}:
      A.SrcFile{[" Emitted by `mech rust-out` from `" ++ source ++ "`."], items(its, True{})}

# The text of one emitted module.
def text(m: R.Module) -> String:
  Pr.file(lower(m))
