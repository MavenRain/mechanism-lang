import Base
import ./token.bend as Tk
import ./lexer.bend as Lexer
import ./ast.bend as A

# Parser-local token kinds. The lexer keeps keywords as `Ident`; `classify`
# maps each identifier and punctuation text to one constructor here, once,
# so the parser matches constructors and never string literals.
type K is Data:
  Name{}
  RawName{}
  IntL{}
  StrL{}
  ChrL{}
  LifeT{}
  DocO{}
  DocI{}
  Cmt{}
  KFn{}
  KPub{}
  KCrate{}
  KMod{}
  KUse{}
  KStruct{}
  KEnum{}
  KImpl{}
  KTrait{}
  KFor{}
  KConst{}
  KType{}
  KLet{}
  KMatch{}
  KIf{}
  KElse{}
  KSelfV{}
  KSelfT{}
  KSuper{}
  KAs{}
  KTrue{}
  KFalse{}
  KMove{}
  KUnder{}
  LParen{}
  RParen{}
  LBrack{}
  RBrack{}
  LBrace{}
  RBrace{}
  Comma{}
  Semi{}
  Colon{}
  Dot{}
  Eq{}
  Lt{}
  Gt{}
  Plus{}
  Minus{}
  Star{}
  Slash{}
  Percent{}
  Bang{}
  Amp{}
  Pipe{}
  Caret{}
  Question{}
  Hash{}
  PathSep{}
  Arrow{}
  FatArrow{}
  EqEq{}
  Ne{}
  Le{}
  Ge{}
  AndAnd{}
  OrOr{}
  DotDot{}
  OtherP{}
  EofK{}

# `text` is the source text (names, literal bodies, punctuation, doc lines).
type Info is Data:
  Info{text: String, loc: Tk.Loc, gap: Bool}

type PT is Data:
  PT{k: K, i: Info}

type Entry is Data:
  Entry{text: String, k: K}

def keywords() -> List<&2, Entry>:
  Entry{"fn", KFn{}} <> Entry{"pub", KPub{}} <> Entry{"crate", KCrate{}} <> Entry{"mod", KMod{}} <>
  Entry{"use", KUse{}} <> Entry{"struct", KStruct{}} <> Entry{"enum", KEnum{}} <> Entry{"impl", KImpl{}} <>
  Entry{"trait", KTrait{}} <> Entry{"for", KFor{}} <> Entry{"const", KConst{}} <> Entry{"type", KType{}} <>
  Entry{"let", KLet{}} <> Entry{"match", KMatch{}} <> Entry{"if", KIf{}} <> Entry{"else", KElse{}} <>
  Entry{"self", KSelfV{}} <> Entry{"Self", KSelfT{}} <> Entry{"super", KSuper{}} <> Entry{"as", KAs{}} <>
  Entry{"true", KTrue{}} <> Entry{"false", KFalse{}} <> Entry{"move", KMove{}} <> Entry{"_", KUnder{}} <> []

def puncts() -> List<&2, Entry>:
  Entry{"(", LParen{}} <> Entry{")", RParen{}} <> Entry{"[", LBrack{}} <> Entry{"]", RBrack{}} <>
  Entry{"{", LBrace{}} <> Entry{"}", RBrace{}} <> Entry{",", Comma{}} <> Entry{";", Semi{}} <>
  Entry{":", Colon{}} <> Entry{".", Dot{}} <> Entry{"=", Eq{}} <> Entry{"<", Lt{}} <> Entry{">", Gt{}} <>
  Entry{"+", Plus{}} <> Entry{"-", Minus{}} <> Entry{"*", Star{}} <> Entry{"/", Slash{}} <>
  Entry{"%", Percent{}} <> Entry{"!", Bang{}} <> Entry{"&", Amp{}} <> Entry{"|", Pipe{}} <>
  Entry{"^", Caret{}} <> Entry{"?", Question{}} <> Entry{"#", Hash{}} <> Entry{"::", PathSep{}} <>
  Entry{"->", Arrow{}} <> Entry{"=>", FatArrow{}} <> Entry{"==", EqEq{}} <> Entry{"!=", Ne{}} <>
  Entry{"<=", Le{}} <> Entry{">=", Ge{}} <> Entry{"&&", AndAnd{}} <> Entry{"||", OrOr{}} <>
  Entry{"..", DotDot{}} <> []

def lookup(+text: String, table: List<&2, Entry>, fallback: K) -> K:
  match table:
    case Nil{}:
      fallback
    case Con{Entry{s, k}, rest}:
      Bool.pick(K, String.eq(text, s), k, lookup(text, rest, fallback))

def classify_kind(kind: Tk.KindT, loc: Tk.Loc, gap: Bool) -> PT:
  match kind:
    case Tk.Ident{+name}:
      PT{lookup(name, keywords(), Name{}), Info{name, loc, gap}}
    case Tk.RawIdent{name}:
      PT{RawName{}, Info{name, loc, gap}}
    case Tk.Lifetime{name}:
      PT{LifeT{}, Info{name, loc, gap}}
    case Tk.Int{text}:
      PT{IntL{}, Info{text, loc, gap}}
    case Tk.Str{text}:
      PT{StrL{}, Info{text, loc, gap}}
    case Tk.ChrLit{text}:
      PT{ChrL{}, Info{text, loc, gap}}
    case Tk.DocOuter{text}:
      PT{DocO{}, Info{text, loc, gap}}
    case Tk.DocInner{text}:
      PT{DocI{}, Info{text, loc, gap}}
    case Tk.Comment{text}:
      PT{Cmt{}, Info{"a comment", loc, gap}}
    case Tk.Punct{+text}:
      PT{lookup(text, puncts(), OtherP{}), Info{text, loc, gap}}
    case Tk.Eof{}:
      PT{EofK{}, Info{"end of input", loc, gap}}

def classify(ts: List<&2, Tk.T>) -> List<&2, PT>:
  match ts:
    case Nil{}:
      []
    case Con{Tk.Tok{kind, loc, gap}, rest}:
      classify_kind(kind, loc, gap) <> classify(rest)

def head_kind(ts: List<&2, PT>) -> K:
  match ts:
    case Con{PT{k, i}, tail}:
      k
    case Nil{}:
      EofK{}

def head_gap(ts: List<&2, PT>) -> Bool:
  match ts:
    case Con{PT{k, Info{text, loc, gap}}, tail}:
      gap
    case Nil{}:
      False{}

# The position of the next token; it goes into the `Meta` of a construct.
def head_pos(ts: List<&2, PT>) -> A.Pos:
  match ts:
    case Con{PT{k, Info{text, Tk.Loc{line, col}, gap}}, tail}:
      A.Pos{line, col}
    case Nil{}:
      A.no_pos()

# Literal-like kinds never satisfy `expect`, even when their text matches.
def is_lit(k: K) -> Bool:
  match k:
    case StrL{}:
      True{}
    case ChrL{}:
      True{}
    case DocO{}:
      True{}
    case DocI{}:
      True{}
    case other:
      False{}

def is_seg(k: K) -> Bool:
  match k:
    case Name{}:
      True{}
    case RawName{}:
      True{}
    case KSelfV{}:
      True{}
    case KSelfT{}:
      True{}
    case KCrate{}:
      True{}
    case KSuper{}:
      True{}
    case other:
      False{}

def is_item(k: K) -> Bool:
  match k:
    case KFn{}:
      True{}
    case KPub{}:
      True{}
    case KStruct{}:
      True{}
    case KEnum{}:
      True{}
    case KUse{}:
      True{}
    case KConst{}:
      True{}
    case KImpl{}:
      True{}
    case KTrait{}:
      True{}
    case KMod{}:
      True{}
    case KType{}:
      True{}
    case other:
      False{}

def is_end(k: K) -> Bool:
  match k:
    case RBrace{}:
      True{}
    case EofK{}:
      True{}
    case other:
      False{}

# Binary operator precedence (higher binds tighter); all are left-associative.
def prec(k: K) -> Maybe<&2, Nat>:
  match k:
    case Star{}:
      Some{10n}
    case Slash{}:
      Some{10n}
    case Percent{}:
      Some{10n}
    case Plus{}:
      Some{9n}
    case Minus{}:
      Some{9n}
    case Amp{}:
      Some{7n}
    case Caret{}:
      Some{6n}
    case Pipe{}:
      Some{5n}
    case EqEq{}:
      Some{4n}
    case Ne{}:
      Some{4n}
    case Lt{}:
      Some{4n}
    case Gt{}:
      Some{4n}
    case Le{}:
      Some{4n}
    case Ge{}:
      Some{4n}
    case AndAnd{}:
      Some{3n}
    case OrOr{}:
      Some{2n}
    case other:
      None{}

type Parsed<-A: Data> is Data:
  Parsed{value: A, rest: List<&2, PT>}

def pv(-A: Data, p: Parsed<A>) -> A:
  match p:
    case Parsed{x, rest}:
      x

def pr(-A: Data, p: Parsed<A>) -> List<&2, PT>:
  match p:
    case Parsed{x, rest}:
      rest

def err(-A: Data, what: String, tokens: List<&2, PT>) -> Result<&2, &2, String, A>:
  match tokens:
    case Con{PT{k, Info{text, loc, gap}}, tail}:
      Fail{Tk.show_loc(loc) ++ " expected " ++ what ++ ", found `" ++ text ++ "`"}
    case Nil{}:
      Fail{"expected " ++ what ++ ", found end of input"}

def expect(+sym: String, +tokens: List<&2, PT>) -> Result<&2, &2, String, List<&2, PT>>:
  match tokens:
    case Con{PT{k, Info{text, loc, gap}}, tail}:
      Bool.pick(Result<&2, &2, String, List<&2, PT>>, String.eq(text, sym) && Bool.not(is_lit(k)),
        Done{tail}, err(List<&2, PT>, "`" ++ sym ++ "`", tokens))
    case Nil{}:
      err(List<&2, PT>, "`" ++ sym ++ "`", [])

type Close is Data:
  CParen{}
  CAngle{}
  CBrack{}

# What `MMeta` parses once outer docs and attributes are collected.
type After is Data:
  AItem{}
  AField{named: Bool}
  AVariant{}
  AStmt{}
  AArm{}

type Value is Data:
  VNode{value: A.Node, rest: List<&2, PT>}
  VNodes{value: List<&2, A.Node>, rest: List<&2, PT>}
  VTy{value: A.Ty, rest: List<&2, PT>}
  VTys{value: List<&2, A.Ty>, rest: List<&2, PT>}
  VPat{value: A.Pat, rest: List<&2, PT>}
  VPats{value: List<&2, A.Pat>, rest: List<&2, PT>}
  VSegs{value: List<&2, A.Seg<A.Ty>>, rest: List<&2, PT>}
  VStmts{value: List<&2, A.Stmt<A.Node>>, rest: List<&2, PT>}
  VStmt{value: A.Stmt<A.Node>, rest: List<&2, PT>}
  VArm{value: A.Arm<A.Node>, rest: List<&2, PT>}
  VArms{value: List<&2, A.Arm<A.Node>>, rest: List<&2, PT>}
  VCParams{value: List<&2, A.CParam>, rest: List<&2, PT>}
  VGens{value: List<&2, A.GParam>, rest: List<&2, PT>}
  VParams{value: List<&2, A.Param>, rest: List<&2, PT>}
  VField{value: A.Field, rest: List<&2, PT>}
  VFields{value: List<&2, A.Field>, rest: List<&2, PT>}
  VBody{value: A.Body, rest: List<&2, PT>}
  VVariant{value: A.Variant, rest: List<&2, PT>}
  VVariants{value: List<&2, A.Variant>, rest: List<&2, PT>}
  VUse{value: A.UseTree, rest: List<&2, PT>}
  VUses{value: List<&2, A.UseTree>, rest: List<&2, PT>}
  VStrs{value: List<&2, String>, rest: List<&2, PT>}
  VAttr{value: A.Attr, rest: List<&2, PT>}

def mismatch(-A: Data, what: String) -> Result<&2, &2, String, Parsed<A>>:
  Fail{"internal parser result mismatch: " ++ what}

def read_node(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Node>>:
  match r:
    case Done{VNode{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Node, "Node")

def read_nodes(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Node>>>:
  match r:
    case Done{VNodes{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Node>, "Nodes")

def read_ty(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Ty>>:
  match r:
    case Done{VTy{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Ty, "Ty")

def read_tys(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Ty>>>:
  match r:
    case Done{VTys{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Ty>, "Tys")

def read_pat(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Pat>>:
  match r:
    case Done{VPat{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Pat, "Pat")

def read_pats(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Pat>>>:
  match r:
    case Done{VPats{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Pat>, "Pats")

def read_segs(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Seg<A.Ty>>>>:
  match r:
    case Done{VSegs{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Seg<A.Ty>>, "Segs")

def read_stmts(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Stmt<A.Node>>>>:
  match r:
    case Done{VStmts{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Stmt<A.Node>>, "Stmts")

def read_stmt(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Stmt<A.Node>>>:
  match r:
    case Done{VStmt{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Stmt<A.Node>, "Stmt")

def read_arm(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Arm<A.Node>>>:
  match r:
    case Done{VArm{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Arm<A.Node>, "Arm")

def read_arms(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Arm<A.Node>>>>:
  match r:
    case Done{VArms{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Arm<A.Node>>, "Arms")

def read_cparams(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.CParam>>>:
  match r:
    case Done{VCParams{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.CParam>, "CParams")

def read_gens(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.GParam>>>:
  match r:
    case Done{VGens{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.GParam>, "Gens")

def read_params(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Param>>>:
  match r:
    case Done{VParams{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Param>, "Params")

def read_field(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Field>>:
  match r:
    case Done{VField{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Field, "Field")

def read_fields(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Field>>>:
  match r:
    case Done{VFields{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Field>, "Fields")

def read_body(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Body>>:
  match r:
    case Done{VBody{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Body, "Body")

def read_variant(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Variant>>:
  match r:
    case Done{VVariant{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Variant, "Variant")

def read_variants(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.Variant>>>:
  match r:
    case Done{VVariants{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.Variant>, "Variants")

def read_use(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.UseTree>>:
  match r:
    case Done{VUse{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.UseTree, "Use")

def read_uses(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, A.UseTree>>>:
  match r:
    case Done{VUses{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, A.UseTree>, "Uses")

def read_strs(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<List<&2, String>>>:
  match r:
    case Done{VStrs{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(List<&2, String>, "Strs")

def read_attr(r: Result<&2, &2, String, Value>) -> Result<&2, &2, String, Parsed<A.Attr>>:
  match r:
    case Done{VAttr{x, rest}}:
      Done{Parsed{x, rest}}
    case Fail{e}:
      Fail{e}
    case Done{other}:
      mismatch(A.Attr, "Attr")

# `ns`: no struct literal (an `if`/`match` scrutinee). Modes ending in a list
# accumulate in reverse and reverse once at the close.
type Mode is Data:
  MBin{ns: Bool, min: Nat}
  MBinRest{ns: Bool, min: Nat, lhs: A.Node}
  MBinOp{ns: Bool, min: Nat, lhs: A.Node, p: Maybe<&2, Nat>}
  MBinGo{ns: Bool, min: Nat, lhs: A.Node, p: Nat}
  MDone{e: A.Node}
  MUnary{ns: Bool}
  MUnaryGo{ns: Bool, op: String}
  MPost{e: A.Node}
  MPrim{ns: Bool}
  MPathExpr{ns: Bool, segs: List<&2, A.Seg<A.Ty>>}
  MParen{}
  MParenRest{e: A.Node}
  MExprs{acc: List<&2, A.Node>, close: Close}
  MClosureRet{mv: Bool, params: List<&2, A.CParam>}
  MCParams{acc: List<&2, A.CParam>}
  MCParamTy{acc: List<&2, A.CParam>, pat: A.Pat}
  MIf{}
  MIfBody{cond: A.Node}
  MElse{cond: A.Node, body: List<&2, A.Stmt<A.Node>>}
  MMatch{}
  MArms{acc: List<&2, A.Arm<A.Node>>}
  MArm{meta: A.Meta}
  MArmGuard{meta: A.Meta, pat: A.Pat}
  MArmBody{meta: A.Meta, pat: A.Pat, guard: Maybe<&2, A.Node>}
  MArmEnd{arm: A.Arm<A.Node>}
  MInits{segs: List<&2, A.Seg<A.Ty>>, acc: List<&2, A.FieldInit<A.Node>>}
  MStmts{acc: List<&2, A.Stmt<A.Node>>}
  MStmt{meta: A.Meta}
  MLetTy{meta: A.Meta, pat: A.Pat}
  MLetInit{meta: A.Meta, pat: A.Pat, ty: Maybe<&2, A.Ty>}
  MStmtItem{meta: A.Meta}
  MStmtExpr{meta: A.Meta}
  MStmtEnd{meta: A.Meta, e: A.Node}
  MPatTop{}
  MPatOr{acc: List<&2, A.Pat>}
  MPat{}
  MPatPath{segs: List<&2, A.Seg<A.Ty>>}
  MPats{acc: List<&2, A.Pat>}
  MFieldPats{segs: List<&2, A.Seg<A.Ty>>, acc: List<&2, A.FieldPat<A.Pat>>}
  MTy{}
  MFnRet{name: String, args: List<&2, A.Ty>}
  MTys{acc: List<&2, A.Ty>, close: Close}
  MBounds{acc: List<&2, A.Ty>}
  MBoundsNext{acc: List<&2, A.Ty>}
  MSeg{ty: Bool, acc: List<&2, A.Seg<A.Ty>>, ok: Bool}
  MSegArgs{ty: Bool, acc: List<&2, A.Seg<A.Ty>>, name: String}
  MSegNext{ty: Bool, acc: List<&2, A.Seg<A.Ty>>}
  MMeta{after: After, gap: Bool, docs: List<&2, String>, attrs: List<&2, A.Attr>}
  MAttr{}
  MStrs{acc: List<&2, String>}
  MStrsPath{acc: List<&2, String>, s: String}
  MInner{acc: List<&2, String>}
  MItems{acc: List<&2, A.Node>}
  MItemsGo{acc: List<&2, A.Node>}
  MDoneNodes{acc: List<&2, A.Node>}
  MItem{meta: A.Meta}
  MItemV{meta: A.Meta, vis: A.Vis}
  MFnSig{meta: A.Meta, vis: A.Vis, name: String, gens: List<&2, A.GParam>, params: List<&2, A.Param>}
  MFnBody{meta: A.Meta, vis: A.Vis, sig: A.Sig}
  MGenerics{}
  MGParams{acc: List<&2, A.GParam>}
  MParams{acc: List<&2, A.Param>}
  MBody{st: Bool}
  MBodyEnd{st: Bool, body: A.Body}
  MFields{acc: List<&2, A.Field>, named: Bool}
  MField{meta: A.Meta, named: Bool}
  MFieldV{meta: A.Meta, named: Bool, vis: A.Vis}
  MVariants{acc: List<&2, A.Variant>}
  MVariant{meta: A.Meta}
  MUse{}
  MUseSeg{ok: Bool}
  MUses{acc: List<&2, A.UseTree>}
  MImplFor{meta: A.Meta, gens: List<&2, A.GParam>, ty: A.Ty}
  MImplBody{meta: A.Meta, gens: List<&2, A.GParam>, tr: Maybe<&2, A.Ty>, self_ty: A.Ty}
  MTraitSup{meta: A.Meta, vis: A.Vis, name: String, gens: List<&2, A.GParam>}
  MTraitBody{meta: A.Meta, vis: A.Vis, name: String, gens: List<&2, A.GParam>, supers: List<&2, A.Ty>}
  MConstVal{meta: A.Meta, vis: A.Vis, name: String, ty: A.Ty}
  MTypeVal{meta: A.Meta, vis: A.Vis, name: String, gens: List<&2, A.GParam>}

def after_meta(after: After, meta: A.Meta) -> Mode:
  match after:
    case AItem{}:
      MItem{meta}
    case AField{named}:
      MField{meta, named}
    case AVariant{}:
      MVariant{meta}
    case AStmt{}:
      MStmt{meta}
    case AArm{}:
      MArm{meta}

def lit(k: K, text: String) -> A.Lit:
  match k:
    case StrL{}:
      A.LStr{text}
    case ChrL{}:
      A.LChar{text}
    case KTrue{}:
      A.LBool{True{}}
    case KFalse{}:
      A.LBool{False{}}
    case other:
      A.LInt{text}

def is_lit_start(k: K) -> Bool:
  match k:
    case IntL{}:
      True{}
    case StrL{}:
      True{}
    case ChrL{}:
      True{}
    case KTrue{}:
      True{}
    case KFalse{}:
      True{}
    case other:
      False{}

@unsafe
def parse(+mode: Mode, +tokens: List<&2, PT>) -> Result<&2, &2, String, Value>:
  match mode tokens:
    # ---- expressions: precedence climbing ----
    case MBin{ns, min} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MUnary{ns}, tokens))
        parse(MBinRest{ns, min, pv(A.Node, a)}, pr(A.Node, a))
    case MBinRest{ns, min, lhs} tokens:
      parse(MBinOp{ns, min, lhs, prec(head_kind(tokens))}, tokens)
    case MBinOp{ns, min, lhs, None{}} tokens:
      Done{VNode{lhs, tokens}}
    case MBinOp{ns, min, lhs, Some{p}} tokens:
      parse(Bool.pick(Mode, Nat.is_ge(p, min), MBinGo{ns, min, lhs, p}, MDone{lhs}), tokens)
    case MDone{e} tokens:
      Done{VNode{e, tokens}}
    case MBinGo{ns, min, lhs, p} Con{PT{k, Info{op, loc, gap}}, tail}:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MBin{ns, 1n+p}, tail))
        parse(MBinRest{ns, min, A.EBinary{op, lhs, pv(A.Node, a)}}, pr(A.Node, a))
    case MBinGo{ns, min, lhs, p} tokens:
      err(Value, "an operator", tokens)
    case MUnary{ns} Con{PT{Minus{}, Info{op, loc, gap}}, tail}:
      parse(MUnaryGo{ns, op}, tail)
    case MUnary{ns} Con{PT{Bang{}, Info{op, loc, gap}}, tail}:
      parse(MUnaryGo{ns, op}, tail)
    case MUnary{ns} Con{PT{Star{}, Info{op, loc, gap}}, tail}:
      parse(MUnaryGo{ns, op}, tail)
    case MUnary{ns} Con{PT{Amp{}, Info{op, loc, gap}}, tail}:
      parse(MUnaryGo{ns, op}, tail)
    case MUnary{ns} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MPrim{ns}, tokens))
        parse(MPost{pv(A.Node, a)}, pr(A.Node, a))
    case MUnaryGo{ns, op} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MUnary{ns}, tokens))
        return VNode{A.EUnary{op, pv(A.Node, a)}, pr(A.Node, a)}
    # ---- postfix: `?`, method calls, fields, calls ----
    case MPost{e} Con{PT{Question{}, i}, tail}:
      parse(MPost{A.ETry{e}}, tail)
    case MPost{e} Con{PT{Dot{}, i}, Con{PT{Name{}, Info{name, l, g}}, Con{PT{PathSep{}, j}, Con{PT{Lt{}, m}, tail}}}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.Ty>> <- read_tys(parse(MTys{[], CAngle{}}, tail))
        t2 : List<&2, PT> <- expect("(", pr(List<&2, A.Ty>, gs))
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{[], CParen{}}, t2))
        parse(MPost{A.EMethod{e, name, pv(List<&2, A.Ty>, gs), pv(List<&2, A.Node>, as)}}, pr(List<&2, A.Node>, as))
    case MPost{e} Con{PT{Dot{}, i}, Con{PT{Name{}, Info{name, l, g}}, Con{PT{LParen{}, j}, tail}}}:
      do Result<&2, &2, String, Value>:
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{[], CParen{}}, tail))
        parse(MPost{A.EMethod{e, name, [], pv(List<&2, A.Node>, as)}}, pr(List<&2, A.Node>, as))
    case MPost{e} Con{PT{Dot{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      parse(MPost{A.EField{e, name}}, tail)
    case MPost{e} Con{PT{Dot{}, i}, Con{PT{IntL{}, Info{name, l, g}}, tail}}:
      parse(MPost{A.EField{e, name}}, tail)
    case MPost{e} Con{PT{LParen{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{[], CParen{}}, tail))
        parse(MPost{A.ECall{e, pv(List<&2, A.Node>, as)}}, pr(List<&2, A.Node>, as))
    case MPost{e} tokens:
      Done{VNode{e, tokens}}
    # ---- primary expressions ----
    case MPrim{ns} Con{PT{LParen{}, i}, tail}:
      parse(MParen{}, tail)
    case MPrim{ns} Con{PT{LBrace{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +b : Parsed<List<&2, A.Stmt<A.Node>>> <- read_stmts(parse(MStmts{[]}, tail))
        return VNode{A.EBlock{pv(List<&2, A.Stmt<A.Node>>, b)}, pr(List<&2, A.Stmt<A.Node>>, b)}
    case MPrim{ns} Con{PT{KIf{}, i}, tail}:
      parse(MIf{}, tail)
    case MPrim{ns} Con{PT{KMatch{}, i}, tail}:
      parse(MMatch{}, tail)
    case MPrim{ns} Con{PT{Pipe{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +ps : Parsed<List<&2, A.CParam>> <- read_cparams(parse(MCParams{[]}, tail))
        parse(MClosureRet{False{}, pv(List<&2, A.CParam>, ps)}, pr(List<&2, A.CParam>, ps))
    case MPrim{ns} Con{PT{OrOr{}, i}, tail}:
      parse(MClosureRet{False{}, []}, tail)
    case MPrim{ns} Con{PT{KMove{}, i}, Con{PT{Pipe{}, j}, tail}}:
      do Result<&2, &2, String, Value>:
        +ps : Parsed<List<&2, A.CParam>> <- read_cparams(parse(MCParams{[]}, tail))
        parse(MClosureRet{True{}, pv(List<&2, A.CParam>, ps)}, pr(List<&2, A.CParam>, ps))
    case MPrim{ns} Con{PT{KMove{}, i}, Con{PT{OrOr{}, j}, tail}}:
      parse(MClosureRet{True{}, []}, tail)
    case MPrim{ns} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Bang{}, i}, Con{PT{LBrack{}, j}, tail}}}:
      do Result<&2, &2, String, Value>:
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{[], CBrack{}}, tail))
        return VNode{A.EMacro{name, True{}, pv(List<&2, A.Node>, as)}, pr(List<&2, A.Node>, as)}
    case MPrim{ns} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Bang{}, i}, Con{PT{LParen{}, j}, tail}}}:
      do Result<&2, &2, String, Value>:
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{[], CParen{}}, tail))
        return VNode{A.EMacro{name, False{}, pv(List<&2, A.Node>, as)}, pr(List<&2, A.Node>, as)}
    case MPrim{ns} Con{PT{k, Info{text, l, g}}, tail}:
      parse(Bool.pick(Mode, is_lit_start(k), MDone{A.ELit{lit(k, text)}}, MPathExpr{ns, []}),
        Bool.pick(List<&2, PT>, is_lit_start(k), tail, tokens))
    case MPrim{ns} tokens:
      err(Value, "an expression", tokens)
    case MPathExpr{ns, Nil{}} tokens:
      do Result<&2, &2, String, Value>:
        +s : Parsed<List<&2, A.Seg<A.Ty>>> <- read_segs(parse(MSeg{False{}, [], is_seg(head_kind(tokens))}, tokens))
        parse(MPathExpr{ns, pv(List<&2, A.Seg<A.Ty>>, s)}, pr(List<&2, A.Seg<A.Ty>>, s))
    case MPathExpr{False{}, segs} Con{PT{LBrace{}, i}, tail}:
      parse(MInits{segs, []}, tail)
    case MPathExpr{ns, segs} tokens:
      Done{VNode{A.EPath{segs}, tokens}}
    case MParen{} Con{PT{RParen{}, i}, tail}:
      Done{VNode{A.ETuple{[]}, tail}}
    case MParen{} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tokens))
        parse(MParenRest{pv(A.Node, a)}, pr(A.Node, a))
    case MParenRest{e} Con{PT{RParen{}, i}, tail}:
      Done{VNode{A.EParen{e}, tail}}
    case MParenRest{e} Con{PT{Comma{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +as : Parsed<List<&2, A.Node>> <- read_nodes(parse(MExprs{e <> [], CParen{}}, tail))
        return VNode{A.ETuple{pv(List<&2, A.Node>, as)}, pr(List<&2, A.Node>, as)}
    case MParenRest{e} tokens:
      err(Value, "`)` or `,`", tokens)
    case MExprs{acc, CParen{}} Con{PT{RParen{}, i}, tail}:
      Done{VNodes{List.reverse(&2, A.Node, acc), tail}}
    case MExprs{acc, CBrack{}} Con{PT{RBrack{}, i}, tail}:
      Done{VNodes{List.reverse(&2, A.Node, acc), tail}}
    case MExprs{acc, close} Con{PT{Comma{}, i}, tail}:
      parse(MExprs{acc, close}, tail)
    case MExprs{acc, close} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tokens))
        parse(MExprs{pv(A.Node, a) <> acc, close}, pr(A.Node, a))
    # ---- closures ----
    case MCParams{acc} Con{PT{Pipe{}, i}, tail}:
      Done{VCParams{List.reverse(&2, A.CParam, acc), tail}}
    case MCParams{acc} Con{PT{Comma{}, i}, tail}:
      parse(MCParams{acc}, tail)
    case MCParams{acc} tokens:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPat{}, tokens))
        parse(MCParamTy{acc, pv(A.Pat, p)}, pr(A.Pat, p))
    case MCParamTy{acc, pat} Con{PT{Colon{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        parse(MCParams{A.CParam{pat, Some{pv(A.Ty, t)}} <> acc}, pr(A.Ty, t))
    case MCParamTy{acc, pat} tokens:
      parse(MCParams{A.CParam{pat, None{}} <> acc}, tokens)
    case MClosureRet{mv, params} Con{PT{Arrow{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        +b : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, pr(A.Ty, t)))
        return VNode{A.EClosure{mv, params, Some{pv(A.Ty, t)}, pv(A.Node, b)}, pr(A.Node, b)}
    case MClosureRet{mv, params} tokens:
      do Result<&2, &2, String, Value>:
        +b : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tokens))
        return VNode{A.EClosure{mv, params, None{}, pv(A.Node, b)}, pr(A.Node, b)}
    # ---- if / match ----
    case MIf{} Con{PT{KLet{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPatTop{}, tail))
        t2 : List<&2, PT> <- expect("=", pr(A.Pat, p))
        +e : Parsed<A.Node> <- read_node(parse(MBin{True{}, 0n}, t2))
        parse(MIfBody{A.ELet{pv(A.Pat, p), pv(A.Node, e)}}, pr(A.Node, e))
    case MIf{} tokens:
      do Result<&2, &2, String, Value>:
        +c : Parsed<A.Node> <- read_node(parse(MBin{True{}, 0n}, tokens))
        parse(MIfBody{pv(A.Node, c)}, pr(A.Node, c))
    case MIfBody{cond} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect("{", tokens)
        +b : Parsed<List<&2, A.Stmt<A.Node>>> <- read_stmts(parse(MStmts{[]}, t2))
        parse(MElse{cond, pv(List<&2, A.Stmt<A.Node>>, b)}, pr(List<&2, A.Stmt<A.Node>>, b))
    case MElse{cond, body} Con{PT{KElse{}, i}, Con{PT{KIf{}, j}, tail}}:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MIf{}, tail))
        return VNode{A.EIf{cond, body, Some{pv(A.Node, e)}}, pr(A.Node, e)}
    case MElse{cond, body} Con{PT{KElse{}, i}, Con{PT{LBrace{}, j}, tail}}:
      do Result<&2, &2, String, Value>:
        +s : Parsed<List<&2, A.Stmt<A.Node>>> <- read_stmts(parse(MStmts{[]}, tail))
        return VNode{A.EIf{cond, body, Some{A.EBlock{pv(List<&2, A.Stmt<A.Node>>, s)}}}, pr(List<&2, A.Stmt<A.Node>>, s)}
    case MElse{cond, body} tokens:
      Done{VNode{A.EIf{cond, body, None{}}, tokens}}
    case MMatch{} tokens:
      do Result<&2, &2, String, Value>:
        +s : Parsed<A.Node> <- read_node(parse(MBin{True{}, 0n}, tokens))
        t2 : List<&2, PT> <- expect("{", pr(A.Node, s))
        +a : Parsed<List<&2, A.Arm<A.Node>>> <- read_arms(parse(MArms{[]}, t2))
        return VNode{A.EMatch{pv(A.Node, s), pv(List<&2, A.Arm<A.Node>>, a)}, pr(List<&2, A.Arm<A.Node>>, a)}
    case MArms{acc} Con{PT{RBrace{}, i}, tail}:
      Done{VArms{List.reverse(&2, A.Arm<A.Node>, acc), tail}}
    case MArms{acc} tokens:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Arm<A.Node>> <- read_arm(parse(MMeta{AArm{}, head_gap(tokens), [], []}, tokens))
        parse(MArms{pv(A.Arm<A.Node>, a) <> acc}, pr(A.Arm<A.Node>, a))
    case MArm{meta} tokens:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPatTop{}, tokens))
        parse(MArmGuard{meta, pv(A.Pat, p)}, pr(A.Pat, p))
    case MArmGuard{meta, pat} Con{PT{KIf{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +g : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tail))
        t2 : List<&2, PT> <- expect("=>", pr(A.Node, g))
        parse(MArmBody{meta, pat, Some{pv(A.Node, g)}}, t2)
    case MArmGuard{meta, pat} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect("=>", tokens)
        parse(MArmBody{meta, pat, None{}}, t2)
    case MArmBody{meta, pat, guard} tokens:
      do Result<&2, &2, String, Value>:
        +b : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tokens))
        parse(MArmEnd{A.Arm{meta, pat, guard, pv(A.Node, b)}}, pr(A.Node, b))
    case MArmEnd{arm} Con{PT{Comma{}, i}, tail}:
      Done{VArm{arm, tail}}
    case MArmEnd{arm} tokens:
      Done{VArm{arm, tokens}}
    # ---- struct literals ----
    case MInits{segs, acc} Con{PT{RBrace{}, i}, tail}:
      Done{VNode{A.EStruct{segs, List.reverse(&2, A.FieldInit<A.Node>, acc), None{}}, tail}}
    case MInits{segs, acc} Con{PT{Comma{}, i}, tail}:
      parse(MInits{segs, acc}, tail)
    case MInits{segs, acc} Con{PT{DotDot{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tail))
        t2 : List<&2, PT> <- expect("}", pr(A.Node, e))
        return VNode{A.EStruct{segs, List.reverse(&2, A.FieldInit<A.Node>, acc), Some{pv(A.Node, e)}}, t2}
    case MInits{segs, acc} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Colon{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tail))
        parse(MInits{segs, A.FieldInit{name, Some{pv(A.Node, e)}} <> acc}, pr(A.Node, e))
    case MInits{segs, acc} Con{PT{Name{}, Info{name, l, g}}, tail}:
      parse(MInits{segs, A.FieldInit{name, None{}} <> acc}, tail)
    case MInits{segs, acc} tokens:
      err(Value, "a field initializer or `}`", tokens)
    # ---- blocks and statements ----
    case MStmts{acc} Con{PT{RBrace{}, i}, tail}:
      Done{VStmts{List.reverse(&2, A.Stmt<A.Node>, acc), tail}}
    case MStmts{acc} Con{PT{Semi{}, i}, tail}:
      parse(MStmts{acc}, tail)
    case MStmts{acc} tokens:
      do Result<&2, &2, String, Value>:
        +s : Parsed<A.Stmt<A.Node>> <- read_stmt(parse(MMeta{AStmt{}, head_gap(tokens), [], []}, tokens))
        parse(MStmts{pv(A.Stmt<A.Node>, s) <> acc}, pr(A.Stmt<A.Node>, s))
    case MStmt{meta} Con{PT{KLet{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPatTop{}, tail))
        parse(MLetTy{meta, pv(A.Pat, p)}, pr(A.Pat, p))
    case MStmt{meta} tokens:
      parse(Bool.pick(Mode, is_item(head_kind(tokens)), MStmtItem{meta}, MStmtExpr{meta}), tokens)
    case MLetTy{meta, pat} Con{PT{Colon{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        parse(MLetInit{meta, pat, Some{pv(A.Ty, t)}}, pr(A.Ty, t))
    case MLetTy{meta, pat} tokens:
      parse(MLetInit{meta, pat, None{}}, tokens)
    case MLetInit{meta, pat, ty} Con{PT{Eq{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tail))
        t2 : List<&2, PT> <- expect(";", pr(A.Node, e))
        return VStmt{A.SLet{meta, pat, ty, Some{pv(A.Node, e)}}, t2}
    case MLetInit{meta, pat, ty} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect(";", tokens)
        return VStmt{A.SLet{meta, pat, ty, None{}}, t2}
    case MStmtItem{meta} tokens:
      do Result<&2, &2, String, Value>:
        +n : Parsed<A.Node> <- read_node(parse(MItem{meta}, tokens))
        return VStmt{A.SItem{pv(A.Node, n)}, pr(A.Node, n)}
    case MStmtExpr{meta} tokens:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tokens))
        parse(MStmtEnd{meta, pv(A.Node, e)}, pr(A.Node, e))
    case MStmtEnd{meta, e} Con{PT{Semi{}, i}, tail}:
      Done{VStmt{A.SExpr{meta, e, True{}}, tail}}
    case MStmtEnd{meta, e} tokens:
      Done{VStmt{A.SExpr{meta, e, False{}}, tokens}}
    # ---- patterns ----
    case MPatTop{} tokens:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPat{}, tokens))
        parse(MPatOr{pv(A.Pat, p) <> []}, pr(A.Pat, p))
    case MPatOr{acc} Con{PT{Pipe{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPat{}, tail))
        parse(MPatOr{pv(A.Pat, p) <> acc}, pr(A.Pat, p))
    case MPatOr{Con{p, Nil{}}} tokens:
      Done{VPat{p, tokens}}
    case MPatOr{acc} tokens:
      Done{VPat{A.POr{List.reverse(&2, A.Pat, acc)}, tokens}}
    case MPat{} Con{PT{KUnder{}, i}, tail}:
      Done{VPat{A.PWild{}, tail}}
    case MPat{} Con{PT{Amp{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPat{}, tail))
        return VPat{A.PRef{pv(A.Pat, p)}, pr(A.Pat, p)}
    case MPat{} Con{PT{LParen{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +ps : Parsed<List<&2, A.Pat>> <- read_pats(parse(MPats{[]}, tail))
        return VPat{A.PTuple{pv(List<&2, A.Pat>, ps)}, pr(List<&2, A.Pat>, ps)}
    case MPat{} Con{PT{Minus{}, i}, Con{PT{IntL{}, Info{text, l, g}}, tail}}:
      Done{VPat{A.PLit{A.LInt{"-" ++ text}}, tail}}
    case MPat{} Con{PT{k, Info{text, l, g}}, tail}:
      Bool.pick(Result<&2, &2, String, Value>, is_lit_start(k), Done{VPat{A.PLit{lit(k, text)}, tail}},
        parse(MPatPath{[]}, tokens))
    case MPat{} tokens:
      err(Value, "a pattern", tokens)
    case MPatPath{Nil{}} tokens:
      do Result<&2, &2, String, Value>:
        +s : Parsed<List<&2, A.Seg<A.Ty>>> <- read_segs(parse(MSeg{False{}, [], is_seg(head_kind(tokens))}, tokens))
        parse(MPatPath{pv(List<&2, A.Seg<A.Ty>>, s)}, pr(List<&2, A.Seg<A.Ty>>, s))
    case MPatPath{segs} Con{PT{LParen{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +ps : Parsed<List<&2, A.Pat>> <- read_pats(parse(MPats{[]}, tail))
        return VPat{A.PTupleStruct{segs, pv(List<&2, A.Pat>, ps)}, pr(List<&2, A.Pat>, ps)}
    case MPatPath{segs} Con{PT{LBrace{}, i}, tail}:
      parse(MFieldPats{segs, []}, tail)
    case MPatPath{segs} tokens:
      Done{VPat{A.PPath{segs}, tokens}}
    case MPats{acc} Con{PT{RParen{}, i}, tail}:
      Done{VPats{List.reverse(&2, A.Pat, acc), tail}}
    case MPats{acc} Con{PT{Comma{}, i}, tail}:
      parse(MPats{acc}, tail)
    case MPats{acc} tokens:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPatTop{}, tokens))
        parse(MPats{pv(A.Pat, p) <> acc}, pr(A.Pat, p))
    case MFieldPats{segs, acc} Con{PT{RBrace{}, i}, tail}:
      Done{VPat{A.PStruct{segs, List.reverse(&2, A.FieldPat<A.Pat>, acc), False{}}, tail}}
    case MFieldPats{segs, acc} Con{PT{DotDot{}, i}, Con{PT{RBrace{}, j}, tail}}:
      Done{VPat{A.PStruct{segs, List.reverse(&2, A.FieldPat<A.Pat>, acc), True{}}, tail}}
    case MFieldPats{segs, acc} Con{PT{Comma{}, i}, tail}:
      parse(MFieldPats{segs, acc}, tail)
    case MFieldPats{segs, acc} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Colon{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPatTop{}, tail))
        parse(MFieldPats{segs, A.FieldPat{name, Some{pv(A.Pat, p)}} <> acc}, pr(A.Pat, p))
    case MFieldPats{segs, acc} Con{PT{Name{}, Info{name, l, g}}, tail}:
      parse(MFieldPats{segs, A.FieldPat{name, None{}} <> acc}, tail)
    case MFieldPats{segs, acc} tokens:
      err(Value, "a field pattern or `}`", tokens)
    # ---- types ----
    case MTy{} Con{PT{Amp{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        return VTy{A.TRef{pv(A.Ty, t)}, pr(A.Ty, t)}
    case MTy{} Con{PT{LParen{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +ts : Parsed<List<&2, A.Ty>> <- read_tys(parse(MTys{[], CParen{}}, tail))
        return VTy{A.TTuple{pv(List<&2, A.Ty>, ts)}, pr(List<&2, A.Ty>, ts)}
    case MTy{} Con{PT{LBrack{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        t2 : List<&2, PT> <- expect("]", pr(A.Ty, t))
        return VTy{A.TSlice{pv(A.Ty, t)}, t2}
    case MTy{} Con{PT{KUnder{}, i}, tail}:
      Done{VTy{A.TInfer{}, tail}}
    case MTy{} Con{PT{KImpl{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +bs : Parsed<List<&2, A.Ty>> <- read_tys(parse(MBounds{[]}, tail))
        return VTy{A.TImpl{pv(List<&2, A.Ty>, bs)}, pr(List<&2, A.Ty>, bs)}
    case MTy{} Con{PT{Name{}, Info{name, l, g}}, Con{PT{LParen{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +ts : Parsed<List<&2, A.Ty>> <- read_tys(parse(MTys{[], CParen{}}, tail))
        parse(MFnRet{name, pv(List<&2, A.Ty>, ts)}, pr(List<&2, A.Ty>, ts))
    case MTy{} tokens:
      do Result<&2, &2, String, Value>:
        +s : Parsed<List<&2, A.Seg<A.Ty>>> <- read_segs(parse(MSeg{True{}, [], is_seg(head_kind(tokens))}, tokens))
        return VTy{A.TPath{pv(List<&2, A.Seg<A.Ty>>, s)}, pr(List<&2, A.Seg<A.Ty>>, s)}
    case MFnRet{name, args} Con{PT{Arrow{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        return VTy{A.TFnSugar{name, args, Some{pv(A.Ty, t)}}, pr(A.Ty, t)}
    case MFnRet{name, args} tokens:
      Done{VTy{A.TFnSugar{name, args, None{}}, tokens}}
    case MTys{acc, CParen{}} Con{PT{RParen{}, i}, tail}:
      Done{VTys{List.reverse(&2, A.Ty, acc), tail}}
    case MTys{acc, CAngle{}} Con{PT{Gt{}, i}, tail}:
      Done{VTys{List.reverse(&2, A.Ty, acc), tail}}
    case MTys{acc, close} Con{PT{Comma{}, i}, tail}:
      parse(MTys{acc, close}, tail)
    case MTys{acc, close} tokens:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tokens))
        parse(MTys{pv(A.Ty, t) <> acc, close}, pr(A.Ty, t))
    case MBounds{acc} tokens:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tokens))
        parse(MBoundsNext{pv(A.Ty, t) <> acc}, pr(A.Ty, t))
    case MBoundsNext{acc} Con{PT{Plus{}, i}, tail}:
      parse(MBounds{acc}, tail)
    case MBoundsNext{acc} tokens:
      Done{VTys{List.reverse(&2, A.Ty, acc), tokens}}
    # ---- paths: `ty` True takes `<..>` generics, False takes `::<..>` ----
    case MSeg{ty, acc, True{}} Con{PT{k, Info{name, l, g}}, tail}:
      parse(MSegArgs{ty, acc, name}, tail)
    case MSeg{ty, acc, ok} tokens:
      err(Value, "a path segment", tokens)
    case MSegArgs{True{}, acc, name} Con{PT{Lt{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +ts : Parsed<List<&2, A.Ty>> <- read_tys(parse(MTys{[], CAngle{}}, tail))
        parse(MSegNext{True{}, A.Seg{name, pv(List<&2, A.Ty>, ts)} <> acc}, pr(List<&2, A.Ty>, ts))
    case MSegArgs{False{}, acc, name} Con{PT{PathSep{}, i}, Con{PT{Lt{}, j}, tail}}:
      do Result<&2, &2, String, Value>:
        +ts : Parsed<List<&2, A.Ty>> <- read_tys(parse(MTys{[], CAngle{}}, tail))
        parse(MSegNext{False{}, A.Seg{name, pv(List<&2, A.Ty>, ts)} <> acc}, pr(List<&2, A.Ty>, ts))
    case MSegArgs{ty, acc, name} tokens:
      parse(MSegNext{ty, A.Seg{name, []} <> acc}, tokens)
    case MSegNext{ty, acc} Con{PT{PathSep{}, i}, tail}:
      parse(MSeg{ty, acc, is_seg(head_kind(tail))}, tail)
    case MSegNext{ty, acc} tokens:
      Done{VSegs{List.reverse(&2, A.Seg<A.Ty>, acc), tokens}}
    # ---- outer docs and attributes ----
    case MMeta{after, gap, docs, attrs} Con{PT{DocO{}, Info{text, l, g}}, tail}:
      parse(MMeta{after, gap, text <> docs, attrs}, tail)
    case MMeta{after, gap, docs, attrs} Con{PT{Hash{}, i}, Con{PT{LBrack{}, j}, tail}}:
      do Result<&2, &2, String, Value>:
        +a : Parsed<A.Attr> <- read_attr(parse(MAttr{}, tail))
        parse(MMeta{after, gap, docs, pv(A.Attr, a) <> attrs}, pr(A.Attr, a))
    case MMeta{after, gap, docs, attrs} tokens:
      parse(after_meta(after, A.Meta{gap, List.reverse(&2, String, docs), List.reverse(&2, A.Attr, attrs), head_pos(tokens)}), tokens)
    case MAttr{} Con{PT{Name{}, Info{name, l, g}}, Con{PT{RBrack{}, i}, tail}}:
      Done{VAttr{A.Attr{name, None{}}, tail}}
    case MAttr{} Con{PT{Name{}, Info{name, l, g}}, Con{PT{LParen{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +s : Parsed<List<&2, String>> <- read_strs(parse(MStrs{[]}, tail))
        t2 : List<&2, PT> <- expect("]", pr(List<&2, String>, s))
        return VAttr{A.Attr{name, Some{pv(List<&2, String>, s)}}, t2}
    case MAttr{} tokens:
      err(Value, "an attribute", tokens)
    case MStrs{acc} Con{PT{RParen{}, i}, tail}:
      Done{VStrs{List.reverse(&2, String, acc), tail}}
    case MStrs{acc} Con{PT{Comma{}, i}, tail}:
      parse(MStrs{acc}, tail)
    case MStrs{acc} Con{PT{Name{}, Info{name, l, g}}, tail}:
      parse(MStrsPath{acc, name}, tail)
    case MStrs{acc} tokens:
      err(Value, "an attribute argument", tokens)
    case MStrsPath{acc, s} Con{PT{PathSep{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      parse(MStrsPath{acc, s ++ "::" ++ name}, tail)
    case MStrsPath{acc, s} tokens:
      parse(MStrs{s <> acc}, tokens)
    case MInner{acc} Con{PT{DocI{}, Info{text, l, g}}, tail}:
      parse(MInner{text <> acc}, tail)
    case MInner{acc} tokens:
      Done{VStrs{List.reverse(&2, String, acc), tokens}}
    # ---- items ----
    case MItems{acc} tokens:
      parse(Bool.pick(Mode, is_end(head_kind(tokens)), MDoneNodes{acc}, MItemsGo{acc}), tokens)
    case MDoneNodes{acc} tokens:
      Done{VNodes{List.reverse(&2, A.Node, acc), tokens}}
    case MItemsGo{acc} tokens:
      do Result<&2, &2, String, Value>:
        +n : Parsed<A.Node> <- read_node(parse(MMeta{AItem{}, head_gap(tokens), [], []}, tokens))
        parse(MItems{pv(A.Node, n) <> acc}, pr(A.Node, n))
    case MItem{meta} Con{PT{KPub{}, i}, Con{PT{LParen{}, j}, Con{PT{KCrate{}, k}, Con{PT{RParen{}, m}, tail}}}}:
      parse(MItemV{meta, A.PubCrate{}}, tail)
    case MItem{meta} Con{PT{KPub{}, i}, tail}:
      parse(MItemV{meta, A.Pub{}}, tail)
    case MItem{meta} tokens:
      parse(MItemV{meta, A.Priv{}}, tokens)
    case MItemV{meta, vis} Con{PT{KFn{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        t2 : List<&2, PT> <- expect("(", pr(List<&2, A.GParam>, gs))
        +ps : Parsed<List<&2, A.Param>> <- read_params(parse(MParams{[]}, t2))
        parse(MFnSig{meta, vis, name, pv(List<&2, A.GParam>, gs), pv(List<&2, A.Param>, ps)}, pr(List<&2, A.Param>, ps))
    case MItemV{meta, vis} Con{PT{KStruct{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        +b : Parsed<A.Body> <- read_body(parse(MBody{True{}}, pr(List<&2, A.GParam>, gs)))
        return VNode{A.IStruct{meta, vis, name, pv(List<&2, A.GParam>, gs), pv(A.Body, b)}, pr(A.Body, b)}
    case MItemV{meta, vis} Con{PT{KEnum{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        t2 : List<&2, PT> <- expect("{", pr(List<&2, A.GParam>, gs))
        +vs : Parsed<List<&2, A.Variant>> <- read_variants(parse(MVariants{[]}, t2))
        return VNode{A.IEnum{meta, vis, name, pv(List<&2, A.GParam>, gs), pv(List<&2, A.Variant>, vs)}, pr(List<&2, A.Variant>, vs)}
    case MItemV{meta, vis} Con{PT{KUse{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +u : Parsed<A.UseTree> <- read_use(parse(MUse{}, tail))
        t2 : List<&2, PT> <- expect(";", pr(A.UseTree, u))
        return VNode{A.IUse{meta, vis, pv(A.UseTree, u)}, t2}
    case MItemV{meta, vis} Con{PT{KMod{}, i}, Con{PT{Name{}, Info{name, l, g}}, Con{PT{Semi{}, j}, tail}}}:
      Done{VNode{A.IMod{meta, vis, name, False{}, [], []}, tail}}
    case MItemV{meta, vis} Con{PT{KMod{}, i}, Con{PT{Name{}, Info{name, l, g}}, Con{PT{LBrace{}, j}, tail}}}:
      do Result<&2, &2, String, Value>:
        +d : Parsed<List<&2, String>> <- read_strs(parse(MInner{[]}, tail))
        +n : Parsed<List<&2, A.Node>> <- read_nodes(parse(MItems{[]}, pr(List<&2, String>, d)))
        t2 : List<&2, PT> <- expect("}", pr(List<&2, A.Node>, n))
        return VNode{A.IMod{meta, vis, name, True{}, pv(List<&2, String>, d), pv(List<&2, A.Node>, n)}, t2}
    case MItemV{meta, vis} Con{PT{KImpl{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, pr(List<&2, A.GParam>, gs)))
        parse(MImplFor{meta, pv(List<&2, A.GParam>, gs), pv(A.Ty, t)}, pr(A.Ty, t))
    case MItemV{meta, vis} Con{PT{KTrait{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        parse(MTraitSup{meta, vis, name, pv(List<&2, A.GParam>, gs)}, pr(List<&2, A.GParam>, gs))
    case MItemV{meta, vis} Con{PT{KConst{}, i}, Con{PT{Name{}, Info{name, l, g}}, Con{PT{Colon{}, j}, tail}}}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        parse(MConstVal{meta, vis, name, pv(A.Ty, t)}, pr(A.Ty, t))
    case MItemV{meta, vis} Con{PT{KType{}, i}, Con{PT{Name{}, Info{name, l, g}}, tail}}:
      do Result<&2, &2, String, Value>:
        +gs : Parsed<List<&2, A.GParam>> <- read_gens(parse(MGenerics{}, tail))
        parse(MTypeVal{meta, vis, name, pv(List<&2, A.GParam>, gs)}, pr(List<&2, A.GParam>, gs))
    case MItemV{meta, vis} tokens:
      err(Value, "an item", tokens)
    case MFnSig{meta, vis, name, gens, params} Con{PT{Arrow{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        parse(MFnBody{meta, vis, A.Sig{name, gens, params, Some{pv(A.Ty, t)}}}, pr(A.Ty, t))
    case MFnSig{meta, vis, name, gens, params} tokens:
      parse(MFnBody{meta, vis, A.Sig{name, gens, params, None{}}}, tokens)
    case MFnBody{meta, vis, sig} Con{PT{Semi{}, i}, tail}:
      Done{VNode{A.IFn{meta, vis, sig, None{}}, tail}}
    case MFnBody{meta, vis, sig} Con{PT{LBrace{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +b : Parsed<List<&2, A.Stmt<A.Node>>> <- read_stmts(parse(MStmts{[]}, tail))
        return VNode{A.IFn{meta, vis, sig, Some{pv(List<&2, A.Stmt<A.Node>>, b)}}, pr(List<&2, A.Stmt<A.Node>>, b)}
    case MFnBody{meta, vis, sig} tokens:
      err(Value, "`{` or `;`", tokens)
    case MGenerics{} Con{PT{Lt{}, i}, tail}:
      parse(MGParams{[]}, tail)
    case MGenerics{} tokens:
      Done{VGens{[], tokens}}
    case MGParams{acc} Con{PT{Gt{}, i}, tail}:
      Done{VGens{List.reverse(&2, A.GParam, acc), tail}}
    case MGParams{acc} Con{PT{Comma{}, i}, tail}:
      parse(MGParams{acc}, tail)
    case MGParams{acc} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Colon{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +bs : Parsed<List<&2, A.Ty>> <- read_tys(parse(MBounds{[]}, tail))
        parse(MGParams{A.GParam{name, pv(List<&2, A.Ty>, bs)} <> acc}, pr(List<&2, A.Ty>, bs))
    case MGParams{acc} Con{PT{Name{}, Info{name, l, g}}, tail}:
      parse(MGParams{A.GParam{name, []} <> acc}, tail)
    case MGParams{acc} tokens:
      err(Value, "a generic parameter", tokens)
    case MParams{acc} Con{PT{RParen{}, i}, tail}:
      Done{VParams{List.reverse(&2, A.Param, acc), tail}}
    case MParams{acc} Con{PT{Comma{}, i}, tail}:
      parse(MParams{acc}, tail)
    case MParams{acc} Con{PT{Amp{}, i}, Con{PT{KSelfV{}, j}, tail}}:
      parse(MParams{A.PSelf{True{}} <> acc}, tail)
    case MParams{acc} Con{PT{KSelfV{}, i}, tail}:
      parse(MParams{A.PSelf{False{}} <> acc}, tail)
    case MParams{acc} tokens:
      do Result<&2, &2, String, Value>:
        +p : Parsed<A.Pat> <- read_pat(parse(MPat{}, tokens))
        t2 : List<&2, PT> <- expect(":", pr(A.Pat, p))
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, t2))
        parse(MParams{A.PTyped{pv(A.Pat, p), pv(A.Ty, t)} <> acc}, pr(A.Ty, t))
    # `st` True: a struct body, which a `;` closes when unit or tuple.
    case MBody{True{}} Con{PT{Semi{}, i}, tail}:
      Done{VBody{A.BUnit{}, tail}}
    case MBody{st} Con{PT{LParen{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +f : Parsed<List<&2, A.Field>> <- read_fields(parse(MFields{[], False{}}, tail))
        parse(MBodyEnd{st, A.BTuple{pv(List<&2, A.Field>, f)}}, pr(List<&2, A.Field>, f))
    case MBody{st} Con{PT{LBrace{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +f : Parsed<List<&2, A.Field>> <- read_fields(parse(MFields{[], True{}}, tail))
        return VBody{A.BNamed{pv(List<&2, A.Field>, f)}, pr(List<&2, A.Field>, f)}
    case MBody{st} tokens:
      Done{VBody{A.BUnit{}, tokens}}
    case MBodyEnd{True{}, body} Con{PT{Semi{}, i}, tail}:
      Done{VBody{body, tail}}
    case MBodyEnd{st, body} tokens:
      Done{VBody{body, tokens}}
    case MFields{acc, named} Con{PT{RParen{}, i}, tail}:
      Done{VFields{List.reverse(&2, A.Field, acc), tail}}
    case MFields{acc, named} Con{PT{RBrace{}, i}, tail}:
      Done{VFields{List.reverse(&2, A.Field, acc), tail}}
    case MFields{acc, named} Con{PT{Comma{}, i}, tail}:
      parse(MFields{acc, named}, tail)
    case MFields{acc, named} tokens:
      do Result<&2, &2, String, Value>:
        +f : Parsed<A.Field> <- read_field(parse(MMeta{AField{named}, head_gap(tokens), [], []}, tokens))
        parse(MFields{pv(A.Field, f) <> acc, named}, pr(A.Field, f))
    case MField{meta, named} Con{PT{KPub{}, i}, Con{PT{LParen{}, j}, Con{PT{KCrate{}, k}, Con{PT{RParen{}, m}, tail}}}}:
      parse(MFieldV{meta, named, A.PubCrate{}}, tail)
    case MField{meta, named} Con{PT{KPub{}, i}, tail}:
      parse(MFieldV{meta, named, A.Pub{}}, tail)
    case MField{meta, named} tokens:
      parse(MFieldV{meta, named, A.Priv{}}, tokens)
    case MFieldV{meta, True{}, vis} Con{PT{Name{}, Info{name, l, g}}, Con{PT{Colon{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        return VField{A.NField{meta, vis, name, pv(A.Ty, t)}, pr(A.Ty, t)}
    case MFieldV{meta, False{}, vis} tokens:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tokens))
        return VField{A.TField{meta, vis, pv(A.Ty, t)}, pr(A.Ty, t)}
    case MFieldV{meta, named, vis} tokens:
      err(Value, "a field", tokens)
    case MVariants{acc} Con{PT{RBrace{}, i}, tail}:
      Done{VVariants{List.reverse(&2, A.Variant, acc), tail}}
    case MVariants{acc} Con{PT{Comma{}, i}, tail}:
      parse(MVariants{acc}, tail)
    case MVariants{acc} tokens:
      do Result<&2, &2, String, Value>:
        +v : Parsed<A.Variant> <- read_variant(parse(MMeta{AVariant{}, head_gap(tokens), [], []}, tokens))
        parse(MVariants{pv(A.Variant, v) <> acc}, pr(A.Variant, v))
    case MVariant{meta} Con{PT{Name{}, Info{name, l, g}}, tail}:
      do Result<&2, &2, String, Value>:
        +b : Parsed<A.Body> <- read_body(parse(MBody{False{}}, tail))
        return VVariant{A.Variant{meta, name, pv(A.Body, b)}, pr(A.Body, b)}
    case MVariant{meta} tokens:
      err(Value, "a variant", tokens)
    case MUse{} Con{PT{Star{}, i}, tail}:
      Done{VUse{A.UGlob{}, tail}}
    case MUse{} Con{PT{LBrace{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +us : Parsed<List<&2, A.UseTree>> <- read_uses(parse(MUses{[]}, tail))
        return VUse{A.UGroup{pv(List<&2, A.UseTree>, us)}, pr(List<&2, A.UseTree>, us)}
    case MUse{} tokens:
      parse(MUseSeg{is_seg(head_kind(tokens))}, tokens)
    case MUseSeg{True{}} Con{PT{k, Info{name, l, g}}, Con{PT{PathSep{}, i}, tail}}:
      do Result<&2, &2, String, Value>:
        +u : Parsed<A.UseTree> <- read_use(parse(MUse{}, tail))
        return VUse{A.UPath{name, pv(A.UseTree, u)}, pr(A.UseTree, u)}
    case MUseSeg{True{}} Con{PT{k, Info{name, l, g}}, Con{PT{KAs{}, i}, Con{PT{Name{}, Info{alias, l2, g2}}, tail}}}:
      Done{VUse{A.UName{name, Some{alias}}, tail}}
    case MUseSeg{True{}} Con{PT{k, Info{name, l, g}}, tail}:
      Done{VUse{A.UName{name, None{}}, tail}}
    case MUseSeg{ok} tokens:
      err(Value, "a use path", tokens)
    case MUses{acc} Con{PT{RBrace{}, i}, tail}:
      Done{VUses{List.reverse(&2, A.UseTree, acc), tail}}
    case MUses{acc} Con{PT{Comma{}, i}, tail}:
      parse(MUses{acc}, tail)
    case MUses{acc} tokens:
      do Result<&2, &2, String, Value>:
        +u : Parsed<A.UseTree> <- read_use(parse(MUse{}, tokens))
        parse(MUses{pv(A.UseTree, u) <> acc}, pr(A.UseTree, u))
    case MImplFor{meta, gens, ty} Con{PT{KFor{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +s : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        parse(MImplBody{meta, gens, Some{ty}, pv(A.Ty, s)}, pr(A.Ty, s))
    case MImplFor{meta, gens, ty} tokens:
      parse(MImplBody{meta, gens, None{}, ty}, tokens)
    case MImplBody{meta, gens, tr, self_ty} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect("{", tokens)
        +n : Parsed<List<&2, A.Node>> <- read_nodes(parse(MItems{[]}, t2))
        t3 : List<&2, PT> <- expect("}", pr(List<&2, A.Node>, n))
        return VNode{A.IImpl{meta, gens, tr, self_ty, pv(List<&2, A.Node>, n)}, t3}
    case MTraitSup{meta, vis, name, gens} Con{PT{Colon{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +bs : Parsed<List<&2, A.Ty>> <- read_tys(parse(MBounds{[]}, tail))
        parse(MTraitBody{meta, vis, name, gens, pv(List<&2, A.Ty>, bs)}, pr(List<&2, A.Ty>, bs))
    case MTraitSup{meta, vis, name, gens} tokens:
      parse(MTraitBody{meta, vis, name, gens, []}, tokens)
    case MTraitBody{meta, vis, name, gens, supers} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect("{", tokens)
        +n : Parsed<List<&2, A.Node>> <- read_nodes(parse(MItems{[]}, t2))
        t3 : List<&2, PT> <- expect("}", pr(List<&2, A.Node>, n))
        return VNode{A.ITrait{meta, vis, name, gens, supers, pv(List<&2, A.Node>, n)}, t3}
    case MConstVal{meta, vis, name, ty} Con{PT{Eq{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +e : Parsed<A.Node> <- read_node(parse(MBin{False{}, 0n}, tail))
        t2 : List<&2, PT> <- expect(";", pr(A.Node, e))
        return VNode{A.IConst{meta, vis, name, ty, Some{pv(A.Node, e)}}, t2}
    case MConstVal{meta, vis, name, ty} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect(";", tokens)
        return VNode{A.IConst{meta, vis, name, ty, None{}}, t2}
    case MTypeVal{meta, vis, name, gens} Con{PT{Eq{}, i}, tail}:
      do Result<&2, &2, String, Value>:
        +t : Parsed<A.Ty> <- read_ty(parse(MTy{}, tail))
        t2 : List<&2, PT> <- expect(";", pr(A.Ty, t))
        return VNode{A.IType{meta, vis, name, gens, Some{pv(A.Ty, t)}}, t2}
    case MTypeVal{meta, vis, name, gens} tokens:
      do Result<&2, &2, String, Value>:
        t2 : List<&2, PT> <- expect(";", tokens)
        return VNode{A.IType{meta, vis, name, gens, None{}}, t2}

def finish(f: A.SrcFile, tokens: List<&2, PT>) -> Result<&2, &2, String, A.SrcFile>:
  match tokens:
    case Con{PT{EofK{}, i}, tail}:
      Done{f}
    case other:
      err(A.SrcFile, "an item or end of input", other)

# Classify and parse the lexer tokens of a whole source file.
def from_tokens(raw: List<&2, Tk.T>) -> Result<&2, &2, String, A.SrcFile>:
  do Result<&2, &2, String, A.SrcFile>:
    +d : Parsed<List<&2, String>> <- read_strs(parse(MInner{[]}, classify(raw)))
    +n : Parsed<List<&2, A.Node>> <- read_nodes(parse(MItems{[]}, pr(List<&2, String>, d)))
    finish(A.SrcFile{pv(List<&2, String>, d), pv(List<&2, A.Node>, n)}, pr(List<&2, A.Node>, n))

# Lex, classify and parse a whole source file.
def file(source: String) -> Result<&2, &2, String, A.SrcFile>:
  do Result<&2, &2, String, A.SrcFile>:
    raw : List<&2, Tk.T> <- Lexer.lex(source)
    from_tokens(raw)
