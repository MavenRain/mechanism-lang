import Base

# Rust AST for the M0 frontend. Bend 2 has no mutually recursive types, so
# expressions and items share the one recursive type `Node`, and the helper
# types take it (or `Ty`, `Pat`) as a parameter, as bend2/surface/syntax.bend does.

type Vis is Data:
  Priv{}
  Pub{}
  PubCrate{}

# `#[name]` or `#[name(arg, ...)]`; each arg is a `::`-joined path.
type Attr is Data:
  Attr{name: String, args: Maybe<&2, List<&2, String>>}

# `gap`: a blank line precedes the construct. `docs`: outer `///` lines.
type Meta is Data:
  Meta{gap: Bool, docs: List<&2, String>, attrs: List<&2, Attr>}

# One path segment; `args` are its generic arguments (`::<..>` in expressions).
type Seg<-T: Data> is Data:
  Seg{name: String, args: List<&2, T>}

type Ty is Data:
  TPath{segs: List<&2, Seg<Ty>>}
  TRef{inner: Ty}
  TTuple{items: List<&2, Ty>}
  TSlice{inner: Ty}
  TInfer{}
  TImpl{bounds: List<&2, Ty>}
  TFnSugar{name: String, args: List<&2, Ty>, ret: Maybe<&2, Ty>}

type GParam is Data:
  GParam{name: String, bounds: List<&2, Ty>}

type Lit is Data:
  LInt{text: String}
  LStr{text: String}
  LChar{text: String}
  LBool{value: Bool}

type FieldPat<-P: Data> is Data:
  FieldPat{name: String, pat: Maybe<&2, P>}

type Pat is Data:
  PWild{}
  PPath{segs: List<&2, Seg<Ty>>}
  PTupleStruct{segs: List<&2, Seg<Ty>>, items: List<&2, Pat>}
  PStruct{segs: List<&2, Seg<Ty>>, fields: List<&2, FieldPat<Pat>>, rest: Bool}
  PTuple{items: List<&2, Pat>}
  PLit{lit: Lit}
  PRef{inner: Pat}
  POr{alts: List<&2, Pat>}

type CParam is Data:
  CParam{pat: Pat, ty: Maybe<&2, Ty>}

type Param is Data:
  PSelf{by_ref: Bool}
  PTyped{pat: Pat, ty: Ty}

type Sig is Data:
  Sig{name: String, generics: List<&2, GParam>, params: List<&2, Param>, ret: Maybe<&2, Ty>}

type Field is Data:
  NField{meta: Meta, vis: Vis, name: String, ty: Ty}
  TField{meta: Meta, vis: Vis, ty: Ty}

# A struct or variant body.
type Body is Data:
  BUnit{}
  BTuple{fields: List<&2, Field>}
  BNamed{fields: List<&2, Field>}

type Variant is Data:
  Variant{meta: Meta, name: String, body: Body}

type UseTree is Data:
  UPath{seg: String, sub: UseTree}
  UName{name: String, alias: Maybe<&2, String>}
  UGlob{}
  UGroup{items: List<&2, UseTree>}

# A block is a list of statements; its tail is the last `SExpr` with `semi` False.
type Stmt<-N: Data> is Data:
  SLet{meta: Meta, pat: Pat, ty: Maybe<&2, Ty>, init: Maybe<&2, N>}
  SExpr{meta: Meta, expr: N, semi: Bool}
  SItem{item: N}

type Arm<-N: Data> is Data:
  Arm{meta: Meta, pat: Pat, guard: Maybe<&2, N>, body: N}

type FieldInit<-N: Data> is Data:
  FieldInit{name: String, value: Maybe<&2, N>}

type Node is Data:
  EPath{segs: List<&2, Seg<Ty>>}
  ELit{lit: Lit}
  ECall{func: Node, args: List<&2, Node>}
  EMethod{recv: Node, name: String, generics: List<&2, Ty>, args: List<&2, Node>}
  EField{recv: Node, name: String}
  ETry{inner: Node}
  EUnary{op: String, inner: Node}
  EBinary{op: String, lhs: Node, rhs: Node}
  EClosure{mv: Bool, params: List<&2, CParam>, ret: Maybe<&2, Ty>, body: Node}
  EBlock{stmts: List<&2, Stmt<Node>>}
  EIf{cond: Node, body: List<&2, Stmt<Node>>, els: Maybe<&2, Node>}
  ELet{pat: Pat, init: Node}
  EMatch{scrut: Node, arms: List<&2, Arm<Node>>}
  EStruct{segs: List<&2, Seg<Ty>>, fields: List<&2, FieldInit<Node>>, base: Maybe<&2, Node>}
  ETuple{items: List<&2, Node>}
  EParen{inner: Node}
  EMacro{name: String, bracket: Bool, args: List<&2, Node>}
  IFn{meta: Meta, vis: Vis, sig: Sig, body: Maybe<&2, List<&2, Stmt<Node>>>}
  IStruct{meta: Meta, vis: Vis, name: String, generics: List<&2, GParam>, body: Body}
  IEnum{meta: Meta, vis: Vis, name: String, generics: List<&2, GParam>, variants: List<&2, Variant>}
  IUse{meta: Meta, vis: Vis, tree: UseTree}
  IMod{meta: Meta, vis: Vis, name: String, inline: Bool, inner: List<&2, String>, items: List<&2, Node>}
  IImpl{meta: Meta, generics: List<&2, GParam>, tr: Maybe<&2, Ty>, self_ty: Ty, items: List<&2, Node>}
  ITrait{meta: Meta, vis: Vis, name: String, generics: List<&2, GParam>, supers: List<&2, Ty>, items: List<&2, Node>}
  IConst{meta: Meta, vis: Vis, name: String, ty: Ty, value: Maybe<&2, Node>}
  IType{meta: Meta, vis: Vis, name: String, generics: List<&2, GParam>, ty: Maybe<&2, Ty>}

# `inner`: the file's leading `//!` lines.
type SrcFile is Data:
  SrcFile{inner: List<&2, String>, items: List<&2, Node>}

def item_line(n: Node) -> String:
  match n:
    case IFn{meta, vis, Sig{name, generics, params, ret}, body}:
      "fn " ++ name
    case IStruct{meta, vis, name, generics, body}:
      "struct " ++ name
    case IEnum{meta, vis, name, generics, variants}:
      "enum " ++ name
    case IUse{meta, vis, tree}:
      "use"
    case IMod{meta, vis, name, inline, inner, items}:
      "mod " ++ name
    case IImpl{meta, generics, tr, self_ty, items}:
      "impl"
    case ITrait{meta, vis, name, generics, supers, items}:
      "trait " ++ name
    case IConst{meta, vis, name, ty, value}:
      "const " ++ name
    case IType{meta, vis, name, generics, ty}:
      "type " ++ name
    case other:
      "expr"
