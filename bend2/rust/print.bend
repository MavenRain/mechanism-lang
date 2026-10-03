import Base
import ./ast.bend as A

# Rustfmt-stable printer for the M0 Rust fragment (default config, edition
# 2021). Law: for an accepted fixture `f`, already rustfmt-clean,
# `file(parse(f)) == f` byte for byte. Bend 2 has no forward references, so
# the printer is one self-recursive `pp` over the job sum `Job`. A flat
# rendering carries a newline when the construct cannot stay on one line;
# rustfmt's default width heuristics add one when a part is too wide:
# max_width 100, fn_call_width 60, chain_width 60, struct_lit_width 18,
# struct_variant_width 35, single_line_if_else_max_width 50.

# ---- widths ----

def slen(s: String) -> Nat:
  match s:
    case SNil{}:
      0n
    case SCon{c, tail}:
      Nat.add(1n, slen(tail))

# Width of the first line.
def flen(s: String) -> Nat:
  match s:
    case SNil{}:
      0n
    case SCon{'\n', tail}:
      0n
    case SCon{c, tail}:
      Nat.add(1n, flen(tail))

def one_line(s: String) -> Bool:
  match s:
    case SNil{}:
      True{}
    case SCon{'\n', tail}:
      False{}
    case SCon{c, tail}:
      one_line(tail)

def le(a: Nat, b: Nat) -> Bool:
  match a b:
    case 0n b:
      True{}
    case 1n+p 0n:
      False{}
    case 1n+p 1n+q:
      le(p, q)

def sub(a: Nat, b: Nat) -> Nat:
  match a b:
    case 0n b:
      0n
    case 1n+p 0n:
      1n+p
    case 1n+p 1n+q:
      sub(p, q)

def min(+a: Nat, +b: Nat) -> Nat:
  Bool.pick(Nat, le(a, b), a, b)

def sp(n: Nat) -> String:
  match n:
    case 0n:
      ""
    case 1n+p:
      SCon{' ', sp(p)}

# Columns left on a line indented by `ind` that already holds `used` more.
def room(ind: Nat, used: Nat) -> Nat:
  sub(100n, Nat.add(ind, used))

def fit(+s: String, avail: Nat) -> Maybe<&2, String>:
  Bool.pick(Maybe<&2, String>, one_line(s) && le(slen(s), avail), Some{s}, None{})

# Marks `s` as not flat when it is wider than a rustfmt heuristic width.
def capped(+s: String, limit: Nat) -> String:
  s ++ Bool.pick(String, le(slen(s), limit), "", "\n")

def qs(q: Nat) -> String:
  match q:
    case 0n:
      ""
    case 1n+p:
      "?" ++ qs(p)

# ---- leaves ----

def join_tail(xs: List<&2, String>, +sep: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      sep ++ x ++ join_tail(rest, sep)

def join(xs: List<&2, String>, sep: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      x ++ join_tail(rest, sep)

def none_s(xs: List<&2, String>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{x, rest}:
      False{}

def none_a(xs: List<&2, A.Attr>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{x, rest}:
      False{}

def none_n(xs: List<&2, A.Node>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{x, rest}:
      False{}

def none_fi(xs: List<&2, A.FieldInit<A.Node>>) -> Bool:
  match xs:
    case Nil{}:
      True{}
    case Con{x, rest}:
      False{}

def vis(v: A.Vis) -> String:
  match v:
    case A.Priv{}:
      ""
    case A.Pub{}:
      "pub "
    case A.PubCrate{}:
      "pub(crate) "

def lit(l: A.Lit) -> String:
  match l:
    case A.LInt{text}:
      text
    case A.LStr{text}:
      "\"" ++ text ++ "\""
    case A.LChar{text}:
      "'" ++ text ++ "'"
    case A.LBool{value}:
      Bool.pick(String, value, "true", "false")

def attr_args(m: Maybe<&2, List<&2, String>>) -> String:
  match m:
    case None{}:
      ""
    case Some{xs}:
      "(" ++ join(xs, ", ") ++ ")"

def attrs(xs: List<&2, A.Attr>, +pre: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{A.Attr{name, args}, rest}:
      pre ++ "#[" ++ name ++ attr_args(args) ++ "]\n" ++ attrs(rest, pre)

def lines(xs: List<&2, String>, +pre: String) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{x, rest}:
      pre ++ x ++ "\n" ++ lines(rest, pre)

# What a `Meta` puts ahead of its construct: a blank line, docs, attributes.
def meta(m: A.Meta, +ind: Nat) -> String:
  match m:
    case A.Meta{gap, docs, ats}:
      Bool.pick(String, gap, "\n", "") ++ lines(docs, sp(ind) ++ "///") ++ attrs(ats, sp(ind))

# A field with docs or attributes forces its enclosing body vertical.
def meta_mark(m: A.Meta) -> String:
  match m:
    case A.Meta{gap, docs, ats}:
      Bool.pick(String, none_s(docs) && none_a(ats), "", "\n")

def body_len(b: Maybe<&2, List<&2, A.Stmt<A.Node>>>) -> Nat:
  match b:
    case None{}:
      1n
    case Some{ss}:
      2n

def last_from(xs: List<&2, A.Node>, x: A.Node) -> A.Node:
  match xs:
    case Nil{}:
      x
    case Con{y, rest}:
      last_from(rest, y)

def init_from(xs: List<&2, A.Node>, x: A.Node) -> List<&2, A.Node>:
  match xs:
    case Nil{}:
      []
    case Con{y, rest}:
      x <> init_from(rest, y)

# rustfmt's `can_be_overflowed_expr`: may the last call argument keep its
# head on the call line and spill below? `single`: it is the only argument.
def ovf(e: A.Node, +single: Bool) -> Bool:
  match e:
    case A.EClosure{mv, ps, r, b}:
      True{}
    case A.EBlock{ss}:
      True{}
    case A.EMatch{s, arms}:
      single
    case A.EIf{c, b, x}:
      single
    case A.EStruct{segs, fs, base}:
      single
    case A.ECall{f, args}:
      single
    case A.EMethod{r, n, g, args}:
      single
    case A.ETuple{items}:
      single
    case A.EMacro{n, br, args}:
      single
    case A.ETry{i}:
      ovf(i, single)
    case A.EUnary{op, i}:
      ovf(i, single)
    case other:
      False{}

# ---- method chains ----

# One chain element after the root; `q` counts the `?` that follow it.
type Link is Data:
  LMeth{name: String, gens: List<&2, A.Ty>, args: List<&2, A.Node>, q: Nat}
  LField{name: String, q: Nat}

type Chain is Data:
  Chain{root: A.Node, rq: Nat, links: List<&2, Link>}

def chain(e: A.Node, q: Nat, acc: List<&2, Link>) -> Chain:
  match e:
    case A.ETry{inner}:
      chain(inner, Nat.add(q, 1n), acc)
    case A.EMethod{recv, name, gens, args}:
      chain(recv, 0n, LMeth{name, gens, args, q} <> acc)
    case A.EField{recv, name}:
      chain(recv, 0n, LField{name, q} <> acc)
    case other:
      Chain{other, q, acc}

# rustfmt's chain child count: every link and every `?`.
def cnt(ls: List<&2, Link>, n: Nat) -> Nat:
  match ls:
    case Nil{}:
      n
    case Con{LMeth{name, gens, args, q}, rest}:
      cnt(rest, Nat.add(n, Nat.add(q, 1n)))
    case Con{LField{name, q}, rest}:
      cnt(rest, Nat.add(n, Nat.add(q, 1n)))

def chain_cnt(c: Chain) -> Nat:
  match c:
    case Chain{root, rq, links}:
      cnt(links, rq)

# A flat chain of two or more children is not flat past chain_width.
def chain_mark(+s: String, c: Chain) -> String:
  s ++ Bool.pick(String, le(2n, chain_cnt(c)) && Bool.not(le(slen(s), 60n)), "\n", "")

def last_link(xs: List<&2, Link>, x: Link) -> Link:
  match xs:
    case Nil{}:
      x
    case Con{y, rest}:
      last_link(rest, y)

def init_links(xs: List<&2, Link>, x: Link) -> List<&2, Link>:
  match xs:
    case Nil{}:
      []
    case Con{y, rest}:
      x <> init_links(rest, y)

# Accept an overflowed last argument: the head stays on one line, the
# argument really spills, and the first line fits.
def ovf_pick(+pre: String, +body: String, close: String, ok: Bool, avail: Nat) -> Maybe<&2, String>:
  Bool.pick(Maybe<&2, String>,
    ok && one_line(pre) && Bool.not(one_line(body)) && le(Nat.add(slen(pre), flen(body)), avail),
    Some{pre ++ body ++ close}, None{})

# Struct variant width: 0 when the flags hold both an "m" and an "s".
def mixed(s: String, m: Bool, o: Bool) -> Nat:
  match s:
    case SNil{}:
      Bool.pick(Nat, m && o, 0n, 35n)
    case SCon{'m', tail}:
      mixed(tail, True{}, o)
    case SCon{c, tail}:
      mixed(tail, m, True{})

# ---- jobs ----

type Job is Data:
  JPick{m: Maybe<&2, String>, alt: Job}
  JSel{m: Maybe<&2, Job>, alt: Job}
  JTy{t: A.Ty}
  JTys{ts: List<&2, A.Ty>, sep: String, first: Bool}
  JRet{r: Maybe<&2, A.Ty>}
  JAnn{t: Maybe<&2, A.Ty>}
  JEqTy{t: Maybe<&2, A.Ty>}
  JTurbo{ts: List<&2, A.Ty>}
  JSegs{segs: List<&2, A.Seg<A.Ty>>, turbo: Bool, first: Bool}
  JPat{p: A.Pat}
  JPats{ps: List<&2, A.Pat>, sep: String, first: Bool}
  JFPats{fs: List<&2, A.FieldPat<A.Pat>>, first: Bool}
  JPStruct{fs: List<&2, A.FieldPat<A.Pat>>, rest: Bool}
  JGens{gs: List<&2, A.GParam>}
  JGList{gs: List<&2, A.GParam>, first: Bool}
  JParam{p: A.Param}
  JParams{ps: List<&2, A.Param>, first: Bool}
  JParamLines{ps: List<&2, A.Param>, ind: Nat}
  JCParams{cs: List<&2, A.CParam>, first: Bool}
  JUse{u: A.UseTree}
  JUses{us: List<&2, A.UseTree>, first: Bool}
  JFl{e: A.Node}
  JFls{es: List<&2, A.Node>, first: Bool}
  JFlsSep{es: List<&2, A.Node>}
  JIfFlat{c: A.Node, b: List<&2, A.Stmt<A.Node>>, x: Maybe<&2, A.Node>}
  JStructFl{fs: List<&2, A.FieldInit<A.Node>>, base: Maybe<&2, A.Node>}
  JInitsFl{fs: List<&2, A.FieldInit<A.Node>>, first: Bool}
  JBaseFl{base: Maybe<&2, A.Node>, empty: Bool}
  JLinksFl{ls: List<&2, Link>}
  JEx{e: A.Node, ind: Nat, avail: Nat}
  JBrk{e: A.Node, ind: Nat, avail: Nat}
  JCat{h: String, e: A.Node, ind: Nat, avail: Nat}
  JArgs{head: String, close: String, args: List<&2, A.Node>, ind: Nat, avail: Nat}
  JOvf{head: String, close: String, args: List<&2, A.Node>, ind: Nat, avail: Nat, alt: Job}
  JOvf3{pre: String, close: String, last: A.Node, ok: Bool, ind: Nat, avail: Nat, alt: Job}
  JVert{head: String, close: String, args: List<&2, A.Node>, ind: Nat}
  JArgLines{args: List<&2, A.Node>, ind: Nat}
  JChain{c: Chain, ind: Nat, avail: Nat}
  JChOvf{pre: String, ls: List<&2, Link>, ind: Nat, budget: Nat, alt: Job}
  JChOvf2{pre: String, l: Link, ind: Nat, budget: Nat, alt: Job}
  JChainV{rs: String, ls: List<&2, Link>, ind: Nat, avail: Nat}
  JChainV2{rs: String, ls: List<&2, Link>, ind: Nat}
  JAbsorb{rs: String, ls: List<&2, Link>, ind: Nat, avail: Nat}
  JLinks{ls: List<&2, Link>, ind: Nat}
  JLink{l: Link, ind: Nat, avail: Nat}
  JInits{fs: List<&2, A.FieldInit<A.Node>>, ind: Nat}
  JBase{base: Maybe<&2, A.Node>, ind: Nat}
  JBlock{ss: List<&2, A.Stmt<A.Node>>, ind: Nat}
  JStmts{ss: List<&2, A.Stmt<A.Node>>, ind: Nat}
  JStmt{s: A.Stmt<A.Node>, ind: Nat}
  JLet{h: String, v: Maybe<&2, A.Node>, ind: Nat}
  JIf{c: A.Node, b: List<&2, A.Stmt<A.Node>>, x: Maybe<&2, A.Node>, ind: Nat}
  JElse{x: Maybe<&2, A.Node>, ind: Nat}
  JArms{arms: List<&2, A.Arm<A.Node>>, ind: Nat}
  JArm{h: String, b: A.Node, ind: Nat}
  JGuard{g: Maybe<&2, A.Node>}
  JItems{ns: List<&2, A.Node>, ind: Nat}
  JItem{n: A.Node, ind: Nat}
  JFn{h: String, ps: List<&2, A.Param>, r: Maybe<&2, A.Ty>, body: Maybe<&2, List<&2, A.Stmt<A.Node>>>, ind: Nat}
  JFnV{h: String, ps: List<&2, A.Param>, r: Maybe<&2, A.Ty>, ind: Nat}
  JBody{body: Maybe<&2, List<&2, A.Stmt<A.Node>>>, ind: Nat}
  JSBody{b: A.Body, ind: Nat}
  JVBody{b: A.Body, ind: Nat, w: Nat}
  JVFlags{vs: List<&2, A.Variant>, ind: Nat}
  JNFields{fs: List<&2, A.Field>, ind: Nat}
  JNFieldsFl{fs: List<&2, A.Field>, first: Bool}
  JTFields{fs: List<&2, A.Field>, first: Bool}
  JVariants{vs: List<&2, A.Variant>, ind: Nat, w: Nat}
  JModBody{inner: List<&2, String>, ns: List<&2, A.Node>, ind: Nat}
  JIBody{ns: List<&2, A.Node>, ind: Nat}

def when(c: Bool, j: Job) -> Maybe<&2, Job>:
  Bool.pick(Maybe<&2, Job>, c, Some{j}, None{})

def sep(first: Bool, s: String) -> String:
  Bool.pick(String, first, "", s)

def ind4(ind: Nat) -> Nat:
  Nat.add(ind, 4n)

@unsafe
def pp(j: Job) -> String:
  match j:
    # ---- choice ----
    case JPick{Some{s}, alt}:
      s
    case JPick{None{}, alt}:
      pp(alt)
    case JSel{Some{a}, alt}:
      pp(a)
    case JSel{None{}, alt}:
      pp(alt)
    # ---- types ----
    case JTy{A.TPath{segs}}:
      pp(JSegs{segs, False{}, True{}})
    case JTy{A.TRef{t}}:
      "&" ++ pp(JTy{t})
    case JTy{A.TTuple{Con{t, Nil{}}}}:
      "(" ++ pp(JTy{t}) ++ ",)"
    case JTy{A.TTuple{ts}}:
      "(" ++ pp(JTys{ts, ", ", True{}}) ++ ")"
    case JTy{A.TSlice{t}}:
      "[" ++ pp(JTy{t}) ++ "]"
    case JTy{A.TInfer{}}:
      "_"
    case JTy{A.TImpl{bs}}:
      "impl " ++ pp(JTys{bs, " + ", True{}})
    case JTy{A.TFnSugar{name, args, r}}:
      name ++ "(" ++ pp(JTys{args, ", ", True{}}) ++ ")" ++ pp(JRet{r})
    case JTys{Nil{}, s, first}:
      ""
    case JTys{Con{t, rest}, +s, first}:
      sep(first, s) ++ pp(JTy{t}) ++ pp(JTys{rest, s, False{}})
    case JRet{None{}}:
      ""
    case JRet{Some{t}}:
      " -> " ++ pp(JTy{t})
    case JAnn{None{}}:
      ""
    case JAnn{Some{t}}:
      ": " ++ pp(JTy{t})
    case JEqTy{None{}}:
      ""
    case JEqTy{Some{t}}:
      " = " ++ pp(JTy{t})
    case JTurbo{Nil{}}:
      ""
    case JTurbo{ts}:
      "::<" ++ pp(JTys{ts, ", ", True{}}) ++ ">"
    case JSegs{Nil{}, turbo, first}:
      ""
    case JSegs{Con{A.Seg{name, Nil{}}, rest}, turbo, first}:
      sep(first, "::") ++ name ++ pp(JSegs{rest, turbo, False{}})
    case JSegs{Con{A.Seg{name, args}, rest}, +turbo, first}:
      sep(first, "::") ++ name ++ Bool.pick(String, turbo, "::<", "<") ++ pp(JTys{args, ", ", True{}}) ++ ">" ++
        pp(JSegs{rest, turbo, False{}})
    # ---- patterns ----
    case JPat{A.PWild{}}:
      "_"
    case JPat{A.PPath{segs}}:
      pp(JSegs{segs, True{}, True{}})
    case JPat{A.PTupleStruct{segs, ps}}:
      pp(JSegs{segs, True{}, True{}}) ++ "(" ++ pp(JPats{ps, ", ", True{}}) ++ ")"
    case JPat{A.PStruct{segs, fs, rest}}:
      pp(JSegs{segs, True{}, True{}}) ++ pp(JPStruct{fs, rest})
    case JPat{A.PTuple{Con{p, Nil{}}}}:
      "(" ++ pp(JPat{p}) ++ ",)"
    case JPat{A.PTuple{ps}}:
      "(" ++ pp(JPats{ps, ", ", True{}}) ++ ")"
    case JPat{A.PLit{l}}:
      lit(l)
    case JPat{A.PRef{p}}:
      "&" ++ pp(JPat{p})
    case JPat{A.POr{ps}}:
      pp(JPats{ps, " | ", True{}})
    case JPats{Nil{}, s, first}:
      ""
    case JPats{Con{p, rest}, +s, first}:
      sep(first, s) ++ pp(JPat{p}) ++ pp(JPats{rest, s, False{}})
    case JPStruct{Nil{}, rest}:
      Bool.pick(String, rest, " { .. }", " {}")
    case JPStruct{fs, rest}:
      " { " ++ pp(JFPats{fs, True{}}) ++ Bool.pick(String, rest, ", ..", "") ++ " }"
    case JFPats{Nil{}, first}:
      ""
    case JFPats{Con{A.FieldPat{name, None{}}, rest}, first}:
      sep(first, ", ") ++ name ++ pp(JFPats{rest, False{}})
    case JFPats{Con{A.FieldPat{name, Some{p}}, rest}, first}:
      sep(first, ", ") ++ name ++ ": " ++ pp(JPat{p}) ++ pp(JFPats{rest, False{}})
    # ---- generics and parameters ----
    case JGens{Nil{}}:
      ""
    case JGens{gs}:
      "<" ++ pp(JGList{gs, True{}}) ++ ">"
    case JGList{Nil{}, first}:
      ""
    case JGList{Con{A.GParam{name, Nil{}}, rest}, first}:
      sep(first, ", ") ++ name ++ pp(JGList{rest, False{}})
    case JGList{Con{A.GParam{name, bs}, rest}, first}:
      sep(first, ", ") ++ name ++ ": " ++ pp(JTys{bs, " + ", True{}}) ++ pp(JGList{rest, False{}})
    case JParam{A.PSelf{r}}:
      Bool.pick(String, r, "&self", "self")
    case JParam{A.PTyped{p, t}}:
      pp(JPat{p}) ++ ": " ++ pp(JTy{t})
    case JParams{Nil{}, first}:
      ""
    case JParams{Con{p, rest}, first}:
      sep(first, ", ") ++ pp(JParam{p}) ++ pp(JParams{rest, False{}})
    case JParamLines{Nil{}, ind}:
      ""
    case JParamLines{Con{p, rest}, +ind}:
      sp(ind) ++ pp(JParam{p}) ++ ",\n" ++ pp(JParamLines{rest, ind})
    case JCParams{Nil{}, first}:
      ""
    case JCParams{Con{A.CParam{p, t}, rest}, first}:
      sep(first, ", ") ++ pp(JPat{p}) ++ pp(JAnn{t}) ++ pp(JCParams{rest, False{}})
    # ---- use trees ----
    case JUse{A.UPath{seg, u}}:
      seg ++ "::" ++ pp(JUse{u})
    case JUse{A.UName{name, None{}}}:
      name
    case JUse{A.UName{name, Some{alias}}}:
      name ++ " as " ++ alias
    case JUse{A.UGlob{}}:
      "*"
    case JUse{A.UGroup{us}}:
      "{" ++ pp(JUses{us, True{}}) ++ "}"
    case JUses{Nil{}, first}:
      ""
    case JUses{Con{u, rest}, first}:
      sep(first, ", ") ++ pp(JUse{u}) ++ pp(JUses{rest, False{}})
    # ---- flat expressions ----
    case JFl{A.EPath{segs}}:
      pp(JSegs{segs, True{}, True{}})
    case JFl{A.ELit{l}}:
      lit(l)
    case JFl{A.ECall{f, args}}:
      pp(JFl{f}) ++ "(" ++ capped(pp(JFls{args, True{}}), 60n) ++ ")"
    case JFl{A.EMethod{+r, +n, +g, +args}}:
      chain_mark(pp(JFl{r}) ++ "." ++ n ++ pp(JTurbo{g}) ++ "(" ++ capped(pp(JFls{args, True{}}), 60n) ++ ")",
        chain(A.EMethod{r, n, g, args}, 0n, []))
    case JFl{A.EField{+r, +n}}:
      chain_mark(pp(JFl{r}) ++ "." ++ n, chain(A.EField{r, n}, 0n, []))
    case JFl{A.ETry{+i}}:
      chain_mark(pp(JFl{i}) ++ "?", chain(A.ETry{i}, 0n, []))
    case JFl{A.EUnary{op, i}}:
      op ++ pp(JFl{i})
    case JFl{A.EBinary{op, l, r}}:
      pp(JFl{l}) ++ " " ++ op ++ " " ++ pp(JFl{r})
    case JFl{A.EClosure{mv, ps, r, b}}:
      Bool.pick(String, mv, "move ", "") ++ "|" ++ pp(JCParams{ps, True{}}) ++ "|" ++ pp(JRet{r}) ++ " " ++ pp(JFl{b})
    case JFl{A.EBlock{Nil{}}}:
      "{}"
    case JFl{A.EBlock{ss}}:
      "{\n}"
    case JFl{A.EIf{c, b, x}}:
      pp(JIfFlat{c, b, x})
    case JFl{A.ELet{p, i}}:
      "let " ++ pp(JPat{p}) ++ " = " ++ pp(JFl{i})
    case JFl{A.EMatch{s, arms}}:
      "match\n"
    case JFl{A.EStruct{segs, fs, base}}:
      pp(JSegs{segs, True{}, True{}}) ++ pp(JStructFl{fs, base})
    case JFl{A.ETuple{Con{x, Nil{}}}}:
      "(" ++ pp(JFl{x}) ++ ",)"
    case JFl{A.ETuple{xs}}:
      "(" ++ capped(pp(JFls{xs, True{}}), 60n) ++ ")"
    case JFl{A.EParen{i}}:
      "(" ++ pp(JFl{i}) ++ ")"
    case JFl{A.EMacro{name, +br, args}}:
      name ++ "!" ++ Bool.pick(String, br, "[", "(") ++ capped(pp(JFls{args, True{}}), 60n) ++ Bool.pick(String, br, "]", ")")
    case JFl{other}:
      "item\n"
    case JFls{Nil{}, first}:
      ""
    case JFls{Con{e, rest}, first}:
      sep(first, ", ") ++ pp(JFl{e}) ++ pp(JFls{rest, False{}})
    case JFlsSep{Nil{}}:
      ""
    case JFlsSep{Con{e, rest}}:
      pp(JFl{e}) ++ ", " ++ pp(JFlsSep{rest})
    # Only `if c { a } else { b }` with two bare tails may stay on one line.
    case JIfFlat{c, Con{A.SExpr{m1, a, False{}}, Nil{}}, Some{A.EBlock{Con{A.SExpr{m2, b, False{}}, Nil{}}}}}:
      capped("if " ++ pp(JFl{c}) ++ " { " ++ pp(JFl{a}) ++ " } else { " ++ pp(JFl{b}) ++ " }", 50n)
    case JIfFlat{c, b, x}:
      "if\n"
    case JStructFl{Nil{}, None{}}:
      " {}"
    case JStructFl{+fs, base}:
      " { " ++ capped(pp(JInitsFl{fs, True{}}) ++ pp(JBaseFl{base, none_fi(fs)}), 18n) ++ " }"
    case JInitsFl{Nil{}, first}:
      ""
    case JInitsFl{Con{A.FieldInit{name, None{}}, rest}, first}:
      sep(first, ", ") ++ name ++ pp(JInitsFl{rest, False{}})
    case JInitsFl{Con{A.FieldInit{name, Some{v}}, rest}, first}:
      sep(first, ", ") ++ name ++ ": " ++ pp(JFl{v}) ++ pp(JInitsFl{rest, False{}})
    case JBaseFl{None{}, empty}:
      ""
    case JBaseFl{Some{b}, empty}:
      Bool.pick(String, empty, "..", ", ..") ++ pp(JFl{b})
    case JLinksFl{Nil{}}:
      ""
    case JLinksFl{Con{LField{name, q}, rest}}:
      "." ++ name ++ qs(q) ++ pp(JLinksFl{rest})
    case JLinksFl{Con{LMeth{name, gens, args, q}, rest}}:
      "." ++ name ++ pp(JTurbo{gens}) ++ "(" ++ pp(JFls{args, True{}}) ++ ")" ++ qs(q) ++ pp(JLinksFl{rest})
    # ---- expressions with a width budget ----
    case JEx{+e, +ind, +avail}:
      pp(JPick{fit(pp(JFl{e}), avail), JBrk{e, ind, avail}})
    case JCat{+h, e, ind, avail}:
      h ++ pp(JEx{e, ind, sub(avail, slen(h))})
    case JBrk{A.ECall{f, args}, ind, avail}:
      pp(JArgs{pp(JFl{f}) ++ "(", ")", args, ind, avail})
    case JBrk{A.EMacro{name, +br, args}, ind, avail}:
      pp(JArgs{name ++ "!" ++ Bool.pick(String, br, "[", "("), Bool.pick(String, br, "]", ")"), args, ind, avail})
    case JBrk{A.EMethod{r, n, g, args}, ind, avail}:
      pp(JChain{chain(A.EMethod{r, n, g, args}, 0n, []), ind, avail})
    case JBrk{A.EField{r, n}, ind, avail}:
      pp(JChain{chain(A.EField{r, n}, 0n, []), ind, avail})
    case JBrk{A.ETry{i}, ind, avail}:
      pp(JChain{chain(A.ETry{i}, 0n, []), ind, avail})
    case JBrk{A.EStruct{segs, fs, base}, +ind, avail}:
      pp(JSegs{segs, True{}, True{}}) ++ " {\n" ++ pp(JInits{fs, ind4(ind)}) ++ pp(JBase{base, ind4(ind)}) ++ sp(ind) ++ "}"
    case JBrk{A.EBlock{ss}, ind, avail}:
      pp(JBlock{ss, ind})
    case JBrk{A.EIf{c, b, x}, ind, avail}:
      pp(JIf{c, b, x, ind})
    case JBrk{A.EMatch{s, arms}, +ind, avail}:
      "match " ++ pp(JFl{s}) ++ " {\n" ++ pp(JArms{arms, ind4(ind)}) ++ sp(ind) ++ "}"
    case JBrk{A.EClosure{mv, ps, r, b}, ind, avail}:
      pp(JCat{Bool.pick(String, mv, "move ", "") ++ "|" ++ pp(JCParams{ps, True{}}) ++ "|" ++ pp(JRet{r}) ++ " ", b, ind, avail})
    case JBrk{A.EBinary{op, l, r}, ind, avail}:
      pp(JCat{pp(JFl{l}) ++ " " ++ op ++ " ", r, ind, avail})
    case JBrk{A.EUnary{op, i}, ind, avail}:
      pp(JCat{op, i, ind, avail})
    case JBrk{A.ELet{p, i}, ind, avail}:
      pp(JCat{"let " ++ pp(JPat{p}) ++ " = ", i, ind, avail})
    case JBrk{A.ETuple{items}, ind, avail}:
      pp(JArgs{"(", ")", items, ind, avail})
    case JBrk{A.EParen{i}, ind, avail}:
      pp(JCat{"(", i, ind, sub(avail, 1n)}) ++ ")"
    case JBrk{other, ind, avail}:
      pp(JFl{other})
    # ---- call arguments: flat, overflow the last, or one per line ----
    case JArgs{+head, +close, +args, +ind, +avail}:
      pp(JPick{fit(head ++ capped(pp(JFls{args, True{}}), 60n) ++ close, avail),
        JOvf{head, close, args, ind, avail, JVert{head, close, args, ind}}})
    case JOvf{head, close, Nil{}, ind, avail, alt}:
      pp(alt)
    case JOvf{head, close, Con{+x, +rest}, ind, avail, alt}:
      pp(JOvf3{head ++ pp(JFlsSep{init_from(rest, x)}), close, last_from(rest, x), ovf(last_from(rest, x), none_n(rest)),
        ind, avail, alt})
    case JOvf3{+pre, +close, last, ok, ind, +avail, alt}:
      pp(JPick{ovf_pick(pre, pp(JEx{last, ind, sub(avail, Nat.add(slen(pre), slen(close)))}), close, ok, avail), alt})
    case JVert{head, close, args, +ind}:
      head ++ "\n" ++ pp(JArgLines{args, ind4(ind)}) ++ sp(ind) ++ close
    case JArgLines{Nil{}, ind}:
      ""
    case JArgLines{Con{a, rest}, +ind}:
      sp(ind) ++ pp(JEx{a, ind, room(ind, 1n)}) ++ ",\n" ++ pp(JArgLines{rest, ind})
    # ---- chains: overflow the last child, else root absorption then one child per line ----
    case JChain{Chain{+root, +rq, +ls}, +ind, +avail}:
      pp(JChOvf{pp(JFl{root}) ++ qs(rq), ls, ind,
        Bool.pick(Nat, le(2n, cnt(ls, rq)), min(avail, 60n), avail),
        JChainV{pp(JEx{root, ind, avail}) ++ qs(rq), ls, ind, avail}})
    case JChOvf{pre, Nil{}, ind, budget, alt}:
      pp(alt)
    case JChOvf{pre, Con{+x, +rest}, ind, budget, alt}:
      pp(JChOvf2{pre ++ pp(JLinksFl{init_links(rest, x)}), last_link(rest, x), ind, budget, alt})
    case JChOvf2{pre, LField{name, q}, ind, budget, alt}:
      pp(alt)
    case JChOvf2{pre, LMeth{name, gens, args, q}, ind, budget, alt}:
      pp(JOvf{pre ++ "." ++ name ++ pp(JTurbo{gens}) ++ "(", ")" ++ qs(q), args, ind, budget, alt})
    case JChainV{+rs, +ls, +ind, +avail}:
      pp(JSel{when(le(slen(rs), 4n) && one_line(rs), JAbsorb{rs, ls, ind, avail}), JChainV2{rs, ls, ind}})
    case JAbsorb{rs, Nil{}, ind, avail}:
      rs
    case JAbsorb{+rs, Con{l, rest}, +ind, +avail}:
      pp(JChainV{rs ++ pp(JLink{l, ind, sub(avail, slen(rs))}), rest, ind, avail})
    case JChainV2{rs, ls, ind}:
      rs ++ pp(JLinks{ls, ind4(ind)})
    case JLinks{Nil{}, ind}:
      ""
    case JLinks{Con{l, rest}, +ind}:
      "\n" ++ sp(ind) ++ pp(JLink{l, ind, room(ind, 1n)}) ++ pp(JLinks{rest, ind})
    case JLink{LField{name, q}, ind, avail}:
      "." ++ name ++ qs(q)
    case JLink{LMeth{name, gens, args, q}, ind, avail}:
      pp(JArgs{"." ++ name ++ pp(JTurbo{gens}) ++ "(", ")" ++ qs(q), args, ind, avail})
    # ---- struct literal fields, one per line ----
    case JInits{Nil{}, ind}:
      ""
    case JInits{Con{A.FieldInit{name, None{}}, rest}, +ind}:
      sp(ind) ++ name ++ ",\n" ++ pp(JInits{rest, ind})
    case JInits{Con{A.FieldInit{+name, Some{v}}, rest}, +ind}:
      sp(ind) ++ name ++ ": " ++ pp(JEx{v, ind, room(ind, Nat.add(slen(name), 3n))}) ++ ",\n" ++ pp(JInits{rest, ind})
    case JBase{None{}, ind}:
      ""
    case JBase{Some{b}, ind}:
      sp(ind) ++ ".." ++ pp(JFl{b}) ++ "\n"
    # ---- blocks and statements ----
    case JBlock{Nil{}, ind}:
      "{}"
    case JBlock{ss, +ind}:
      "{\n" ++ pp(JStmts{ss, ind4(ind)}) ++ sp(ind) ++ "}"
    case JStmts{Nil{}, ind}:
      ""
    case JStmts{Con{s, rest}, +ind}:
      pp(JStmt{s, ind}) ++ "\n" ++ pp(JStmts{rest, ind})
    case JStmt{A.SLet{m, p, t, v}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JLet{"let " ++ pp(JPat{p}) ++ pp(JAnn{t}), v, ind})
    case JStmt{A.SExpr{m, A.EIf{c, b, x}, semi}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JIf{c, b, x, ind}) ++ Bool.pick(String, semi, ";", "")
    case JStmt{A.SExpr{m, e, +semi}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JEx{e, ind, room(ind, Bool.pick(Nat, semi, 1n, 0n))}) ++ Bool.pick(String, semi, ";", "")
    case JStmt{A.SItem{n}, ind}:
      pp(JItem{n, ind})
    case JLet{h, None{}, ind}:
      h ++ ";"
    case JLet{+h, Some{v}, +ind}:
      h ++ " = " ++ pp(JEx{v, ind, room(ind, Nat.add(slen(h), 4n))}) ++ ";"
    case JIf{c, b, x, +ind}:
      "if " ++ pp(JFl{c}) ++ " " ++ pp(JBlock{b, ind}) ++ pp(JElse{x, ind})
    case JElse{None{}, ind}:
      ""
    case JElse{Some{A.EIf{c, b, x}}, ind}:
      " else " ++ pp(JIf{c, b, x, ind})
    case JElse{Some{A.EBlock{ss}}, ind}:
      " else " ++ pp(JBlock{ss, ind})
    case JElse{Some{other}, ind}:
      " else " ++ pp(JFl{other})
    case JArms{Nil{}, ind}:
      ""
    case JArms{Con{A.Arm{m, p, g, b}, rest}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JArm{pp(JPat{p}) ++ pp(JGuard{g}) ++ " =>", b, ind}) ++ "\n" ++ pp(JArms{rest, ind})
    case JArm{h, A.EBlock{ss}, ind}:
      h ++ " " ++ pp(JBlock{ss, ind})
    case JArm{+h, b, +ind}:
      h ++ " " ++ pp(JEx{b, ind, room(ind, Nat.add(slen(h), 2n))}) ++ ","
    case JGuard{None{}}:
      ""
    case JGuard{Some{g}}:
      " if " ++ pp(JFl{g})
    # ---- items ----
    case JItems{Nil{}, ind}:
      ""
    case JItems{Con{n, rest}, +ind}:
      pp(JItem{n, ind}) ++ "\n" ++ pp(JItems{rest, ind})
    case JItem{A.IFn{m, v, A.Sig{name, gs, ps, r}, body}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JFn{vis(v) ++ "fn " ++ name ++ pp(JGens{gs}), ps, r, body, ind})
    case JItem{A.IStruct{m, v, name, gs, b}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "struct " ++ name ++ pp(JGens{gs}) ++ pp(JSBody{b, ind})
    case JItem{A.IEnum{m, v, name, gs, Nil{}}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "enum " ++ name ++ pp(JGens{gs}) ++ " {}"
    case JItem{A.IEnum{m, v, name, gs, +vs}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "enum " ++ name ++ pp(JGens{gs}) ++ " {\n" ++ pp(JVariants{vs, ind4(ind), mixed(pp(JVFlags{vs, ind4(ind)}), False{}, False{})}) ++
        sp(ind) ++ "}"
    case JItem{A.IUse{m, v, u}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "use " ++ pp(JUse{u}) ++ ";"
    case JItem{A.IMod{m, v, name, inline, inner, ns}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "mod " ++ name ++ Bool.pick(String, inline, pp(JModBody{inner, ns, ind}), ";")
    case JItem{A.IImpl{m, gs, None{}, t, ns}, +ind}:
      meta(m, ind) ++ sp(ind) ++ "impl" ++ pp(JGens{gs}) ++ " " ++ pp(JTy{t}) ++ pp(JIBody{ns, ind})
    case JItem{A.IImpl{m, gs, Some{tr}, t, ns}, +ind}:
      meta(m, ind) ++ sp(ind) ++ "impl" ++ pp(JGens{gs}) ++ " " ++ pp(JTy{tr}) ++ " for " ++ pp(JTy{t}) ++ pp(JIBody{ns, ind})
    case JItem{A.ITrait{m, v, name, gs, Nil{}, ns}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "trait " ++ name ++ pp(JGens{gs}) ++ pp(JIBody{ns, ind})
    case JItem{A.ITrait{m, v, name, gs, sups, ns}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "trait " ++ name ++ pp(JGens{gs}) ++ ": " ++ pp(JTys{sups, " + ", True{}}) ++
        pp(JIBody{ns, ind})
    case JItem{A.IConst{m, v, name, t, x}, +ind}:
      meta(m, ind) ++ sp(ind) ++ pp(JLet{vis(v) ++ "const " ++ name ++ ": " ++ pp(JTy{t}), x, ind})
    case JItem{A.IType{m, v, name, gs, t}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ "type " ++ name ++ pp(JGens{gs}) ++ pp(JEqTy{t}) ++ ";"
    case JItem{other, ind}:
      pp(JFl{other})
    case JFn{+h, +ps, +r, +body, +ind}:
      pp(JPick{fit(h ++ "(" ++ pp(JParams{ps, True{}}) ++ ")" ++ pp(JRet{r}), room(ind, body_len(body))), JFnV{h, ps, r, ind}}) ++
        pp(JBody{body, ind})
    case JFnV{h, ps, r, +ind}:
      h ++ "(\n" ++ pp(JParamLines{ps, ind4(ind)}) ++ sp(ind) ++ ")" ++ pp(JRet{r})
    case JBody{None{}, ind}:
      ";"
    case JBody{Some{Nil{}}, ind}:
      " {}"
    case JBody{Some{ss}, ind}:
      " " ++ pp(JBlock{ss, ind})
    case JSBody{A.BUnit{}, ind}:
      ";"
    case JSBody{A.BTuple{fs}, ind}:
      "(" ++ pp(JTFields{fs, True{}}) ++ ");"
    case JSBody{A.BNamed{Nil{}}, ind}:
      " {}"
    case JSBody{A.BNamed{fs}, +ind}:
      " {\n" ++ pp(JNFields{fs, ind4(ind)}) ++ sp(ind) ++ "}"
    case JVBody{A.BUnit{}, ind, w}:
      ""
    case JVBody{A.BTuple{fs}, ind, w}:
      "(" ++ pp(JTFields{fs, True{}}) ++ ")"
    case JVBody{A.BNamed{Nil{}}, ind, w}:
      " {}"
    case JVBody{A.BNamed{+fs}, +ind, w}:
      pp(JPick{fit(" { " ++ capped(pp(JNFieldsFl{fs, True{}}), w) ++ " }", 100n),
        JSBody{A.BNamed{fs}, ind}})
    case JNFields{Nil{}, ind}:
      ""
    case JNFields{Con{A.NField{m, v, name, t}, rest}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ name ++ ": " ++ pp(JTy{t}) ++ ",\n" ++ pp(JNFields{rest, ind})
    case JNFields{Con{A.TField{m, v, t}, rest}, +ind}:
      meta(m, ind) ++ sp(ind) ++ vis(v) ++ pp(JTy{t}) ++ ",\n" ++ pp(JNFields{rest, ind})
    case JNFieldsFl{Nil{}, first}:
      ""
    case JNFieldsFl{Con{A.NField{m, v, name, t}, rest}, first}:
      sep(first, ", ") ++ meta_mark(m) ++ vis(v) ++ name ++ ": " ++ pp(JTy{t}) ++ pp(JNFieldsFl{rest, False{}})
    case JNFieldsFl{Con{A.TField{m, v, t}, rest}, first}:
      sep(first, ", ") ++ meta_mark(m) ++ vis(v) ++ pp(JTy{t}) ++ pp(JNFieldsFl{rest, False{}})
    case JTFields{Nil{}, first}:
      ""
    case JTFields{Con{A.TField{m, v, t}, rest}, first}:
      sep(first, ", ") ++ vis(v) ++ pp(JTy{t}) ++ pp(JTFields{rest, False{}})
    case JTFields{Con{A.NField{m, v, name, t}, rest}, first}:
      sep(first, ", ") ++ vis(v) ++ name ++ ": " ++ pp(JTy{t}) ++ pp(JTFields{rest, False{}})
    case JVariants{Nil{}, ind, w}:
      ""
    case JVariants{Con{A.Variant{m, name, b}, rest}, +ind, +w}:
      meta(m, ind) ++ sp(ind) ++ name ++ pp(JVBody{b, ind, w}) ++ ",\n" ++ pp(JVariants{rest, ind, w})
    # rustfmt: one multi-line variant (docs and attributes count) next to a
    # one-line variant puts every struct variant on its own lines.
    case JVFlags{Nil{}, ind}:
      ""
    case JVFlags{Con{A.Variant{m, name, b}, rest}, +ind}:
      Bool.pick(String, one_line(meta_mark(m) ++ pp(JVBody{b, ind, 35n})), "s", "m") ++ pp(JVFlags{rest, ind})
    case JModBody{Nil{}, Nil{}, ind}:
      " {}"
    case JModBody{inner, ns, +ind}:
      " {\n" ++ lines(inner, sp(ind4(ind)) ++ "//!") ++ pp(JItems{ns, ind4(ind)}) ++ sp(ind) ++ "}"
    case JIBody{Nil{}, ind}:
      " {}"
    case JIBody{ns, +ind}:
      " {\n" ++ pp(JItems{ns, ind4(ind)}) ++ sp(ind) ++ "}"

# The whole file: inner docs, then the items, one trailing newline.
def file(f: A.SrcFile) -> String:
  match f:
    case A.SrcFile{inner, ns}:
      lines(inner, "//!") ++ pp(JItems{ns, 0n})
