import Base
import ./ast.bend as A
import ./rir.bend as R
import ./erase_typed.bend as Et
import ./resolve.bend as Rs
import ../kernel/bignum.bend as Bignum
import ../kernel/quantity.bend as Quantity
import ../surface/syntax.bend as Syntax

# Type inference of the Rust importer (mech-rust C3, CD8 and CD9).
# Input: the resolved crate of resolve.bend (names are known to resolve).
# Output: for each fn, its mech telescope (a Syntax.T) and the notes of its
# body. A note is the result of inference for one node: NCtor gives the type
# arguments of an XCtor, NMatch gives the result type of an XMatch. The notes
# are in pre-order of the body (a node before its children, children left to
# right; the scrutinee of a match before its arms), so `lower` (C4) walks the
# body in the same order and takes one note for each XCtor and XMatch.
# Types compare with TRef and TBox removed: `lower` drops both.

type Note is Data:
  NCtor{targs: List<&2, R.Ty>}
  NMatch{ret: R.Ty}

type Typed is Data:
  Typed{module: String, rust: String, mech: String, pos: A.Pos, tele: Syntax.T, notes: List<&2, Note>}

type Sig is Data:
  Sig{name: String, gens: List<&2, String>, params: List<&2, R.Ty>, ret: R.Ty}

type Fam is Data:
  Fam{name: String, gens: List<&2, String>, variants: List<&2, R.Variant>}

type Loc is Data:
  Loc{name: String, ty: R.Ty}

type Env is Data:
  Env{sigs: List<&2, Sig>, fams: List<&2, Fam>, gens: List<&2, String>}

# ---- types ----

def u32() -> R.Ty:
  R.TCon{"u32", []}

def nat() -> R.Ty:
  R.TCon{"Nat", []}

# Bend 2 has no forward references, so each walker over types is one def over a
# list of types. A TFn{args, ret} goes in as the list `ret <> args`.

# The head of a list of one type (the default is never used).
def one(ts: List<&2, R.Ty>) -> R.Ty:
  match ts:
    case Con{t, rest}:
      t
    case Nil{}:
      R.TBool{}

# `ret <> args` back to the TFn.
def fn_of(ts: List<&2, R.Ty>) -> R.Ty:
  match ts:
    case Con{r, xs}:
      R.TFn{xs, r}
    case Nil{}:
      R.TBool{}

@unsafe def strips(ts: List<&2, R.Ty>) -> List<&2, R.Ty>:
  match ts:
    case Nil{}:
      []
    case Con{R.TBool{}, rest}:
      R.TBool{} <> strips(rest)
    case Con{R.TCon{name, args}, rest}:
      R.TCon{name, strips(args)} <> strips(rest)
    case Con{R.TBox{i}, rest}:
      strips(i <> rest)
    case Con{R.TRef{i}, rest}:
      strips(i <> rest)
    case Con{R.TFn{args, ret}, rest}:
      fn_of(strips(ret <> args)) <> strips(rest)

def strip(t: R.Ty) -> R.Ty:
  one(strips([t]))

@unsafe def teqs(xs: List<&2, R.Ty>, ys: List<&2, R.Ty>) -> Bool:
  match xs ys:
    case Nil{} Nil{}:
      True{}
    case Con{R.TBool{}, xr} Con{R.TBool{}, yr}:
      teqs(xr, yr)
    case Con{R.TCon{n, as}, xr} Con{R.TCon{m, bs}, yr}:
      String.eq(n, m) && teqs(as, bs) && teqs(xr, yr)
    case Con{R.TBox{a}, xr} Con{R.TBox{b}, yr}:
      teqs(a <> xr, b <> yr)
    case Con{R.TRef{a}, xr} Con{R.TRef{b}, yr}:
      teqs(a <> xr, b <> yr)
    case Con{R.TFn{as, a}, xr} Con{R.TFn{bs, b}, yr}:
      teqs(a <> as, b <> bs) && teqs(xr, yr)
    case xs ys:
      False{}

def teq(a: R.Ty, b: R.Ty) -> Bool:
  teqs([a], [b])

def pick_ty(xs: List<&2, R.Ty>, +name: String, +gens: List<&2, String>, +args: List<&2, R.Ty>) -> R.Ty:
  match gens args:
    case Con{g, gr} Con{a, ar}:
      Bool.pick(R.Ty, String.eq(g, name), a, pick_ty(xs, name, gr, ar))
    case gr ar:
      R.TCon{name, xs}

# Replace each generic of `gens` by the type at the same place in `args`.
@unsafe def substs(+gens: List<&2, String>, +args: List<&2, R.Ty>, ts: List<&2, R.Ty>) -> List<&2, R.Ty>:
  match ts:
    case Nil{}:
      []
    case Con{R.TBool{}, rest}:
      R.TBool{} <> substs(gens, args, rest)
    case Con{R.TCon{+name, xs}, rest}:
      pick_ty(substs(gens, args, xs), name, gens, args) <> substs(gens, args, rest)
    case Con{R.TBox{i}, rest}:
      R.TBox{one(substs(gens, args, [i]))} <> substs(gens, args, rest)
    case Con{R.TRef{i}, rest}:
      R.TRef{one(substs(gens, args, [i]))} <> substs(gens, args, rest)
    case Con{R.TFn{xs, r}, rest}:
      fn_of(substs(gens, args, r <> xs)) <> substs(gens, args, rest)

def subst(+gens: List<&2, String>, +args: List<&2, R.Ty>, t: R.Ty) -> R.Ty:
  one(substs(gens, args, [t]))

# ---- telescopes (CD8) ----

def qty(+copies: List<&2, String>, t: R.Ty) -> Quantity.T:
  match t:
    case R.TBool{}:
      Quantity.QMany{}
    case R.TCon{+name, xs}:
      Bool.pick(Quantity.T, Et.has(copies, name), Quantity.QMany{}, Quantity.QOne{})
    case R.TBox{i}:
      Quantity.QOne{}
    case R.TRef{i}:
      Quantity.QMany{}
    case R.TFn{xs, r}:
      Quantity.QMany{}

# The list comes first: the termination check reads the args left to right.
def apps(xs: List<&2, Syntax.T>, h: Syntax.T) -> Syntax.T:
  match xs:
    case Nil{}:
      h
    case Con{x, rest}:
      apps(rest, Syntax.SApp{h, x})

# The arrows of a TFn: `ts` gives the quantities, `ms` the mech types of the
# same arguments.
def arrows(+copies: List<&2, String>, ts: List<&2, R.Ty>, ms: List<&2, Syntax.T>, body: Syntax.T) -> Syntax.T:
  match ts ms:
    case Con{t, rest} Con{m, mr}:
      Syntax.SArrow{Syntax.Binder{qty(copies, t), "_", m}, arrows(copies, rest, mr, body)}
    case rest mr:
      body

# `ms` is the list of mech types of `ret <> ts`.
def fn_mty(+copies: List<&2, String>, ts: List<&2, R.Ty>, ms: List<&2, Syntax.T>) -> Syntax.T:
  match ms:
    case Con{b, mr}:
      arrows(copies, ts, mr, b)
    case Nil{}:
      Syntax.SVar{"MechBool"}

@unsafe def mtys(+copies: List<&2, String>, ts: List<&2, R.Ty>) -> List<&2, Syntax.T>:
  match ts:
    case Nil{}:
      []
    case Con{R.TBool{}, rest}:
      Syntax.SVar{"MechBool"} <> mtys(copies, rest)
    case Con{R.TCon{name, xs}, rest}:
      apps(mtys(copies, xs), Syntax.SVar{name}) <> mtys(copies, rest)
    case Con{R.TBox{i}, rest}:
      mtys(copies, i <> rest)
    case Con{R.TRef{i}, rest}:
      mtys(copies, i <> rest)
    case Con{R.TFn{+xs, r}, rest}:
      fn_mty(copies, xs, mtys(copies, r <> xs)) <> mtys(copies, rest)

def one_m(ms: List<&2, Syntax.T>) -> Syntax.T:
  match ms:
    case Con{m, rest}:
      m
    case Nil{}:
      Syntax.SVar{"MechBool"}

def mty(+copies: List<&2, String>, t: R.Ty) -> Syntax.T:
  one_m(mtys(copies, [t]))

def binder_name(+name: String) -> String:
  Et.fn_back(name)

@unsafe def params_tele(+copies: List<&2, String>, ps: List<&2, R.Param>, body: Syntax.T) -> Syntax.T:
  match ps:
    case Nil{}:
      body
    case Con{R.Param{name, +t}, rest}:
      Syntax.SArrow{Syntax.Binder{qty(copies, t), binder_name(name), mty(copies, t)}, params_tele(copies, rest, body)}

def gens_tele(gs: List<&2, String>, body: Syntax.T) -> Syntax.T:
  match gs:
    case Nil{}:
      body
    case Con{g, rest}:
      Syntax.SArrow{Syntax.Binder{Quantity.QZero{}, g, Syntax.SType{Bignum.zero()}}, gens_tele(rest, body)}

def telescope(+copies: List<&2, String>, gs: List<&2, String>, ps: List<&2, R.Param>, ret: R.Ty) -> Syntax.T:
  gens_tele(gs, params_tele(copies, ps, mty(copies, ret)))

# ---- tables ----

def param_tys(ps: List<&2, R.Param>) -> List<&2, R.Ty>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{name, t}, rest}:
      t <> param_tys(rest)

def nat_sigs() -> List<&2, Sig>:
  [Sig{"nat_small", [], [u32()], nat()}, Sig{"nat_add", [], [R.TRef{nat()}, R.TRef{nat()}], nat()}]

def item_env(p: Rs.Placed, e: Env) -> Env:
  match p e:
    case Rs.Placed{pos, R.IEnum{name, gs, cp, vs}} Env{sigs, fams, scope}:
      Env{sigs, Fam{name, gs, vs} <> fams, scope}
    case Rs.Placed{pos, R.IUnit{+name, +ctor}} Env{sigs, fams, scope}:
      Env{Sig{ctor, [], [], R.TCon{name, []}} <> sigs, Fam{name, [], [R.Variant{ctor, []}]} <> fams, scope}
    case Rs.Placed{pos, R.IFn{name, gs, ps, rt, body}} Env{sigs, fams, scope}:
      Env{Sig{name, gs, param_tys(ps), rt} <> sigs, fams, scope}
    case Rs.Placed{pos, it} e:
      e

def items_env(ps: List<&2, Rs.Placed>, e: Env) -> Env:
  match ps:
    case Nil{}:
      e
    case Con{p, rest}:
      items_env(rest, item_env(p, e))

def mods_env(ms: List<&2, Rs.Mod>, e: Env) -> Env:
  match ms:
    case Nil{}:
      e
    case Con{Rs.Mod{name, file, items}, rest}:
      mods_env(rest, items_env(items, e))

def item_copies(p: Rs.Placed, +cs: List<&2, String>) -> List<&2, String>:
  match p:
    case Rs.Placed{pos, R.IEnum{name, gs, cp, vs}}:
      Bool.pick(List<&2, String>, cp, name <> cs, cs)
    case Rs.Placed{pos, R.IUnit{name, ctor}}:
      name <> cs
    case Rs.Placed{pos, it}:
      cs

def items_copies(ps: List<&2, Rs.Placed>, cs: List<&2, String>) -> List<&2, String>:
  match ps:
    case Nil{}:
      cs
    case Con{p, rest}:
      items_copies(rest, item_copies(p, cs))

def mods_copies(ms: List<&2, Rs.Mod>, cs: List<&2, String>) -> List<&2, String>:
  match ms:
    case Nil{}:
      cs
    case Con{Rs.Mod{name, file, items}, rest}:
      mods_copies(rest, items_copies(items, cs))

def find_sig(sigs: List<&2, Sig>, +name: String) -> Maybe<&2, Sig>:
  match sigs:
    case Nil{}:
      None{}
    case Con{Sig{+n, gs, ps, r}, rest}:
      Bool.pick(Maybe<&2, Sig>, String.eq(n, name), Some{Sig{n, gs, ps, r}}, find_sig(rest, name))

def find_fam(fams: List<&2, Fam>, +name: String) -> Maybe<&2, Fam>:
  match fams:
    case Nil{}:
      None{}
    case Con{Fam{+n, gs, vs}, rest}:
      Bool.pick(Maybe<&2, Fam>, String.eq(n, name), Some{Fam{n, gs, vs}}, find_fam(rest, name))

def find_loc(ls: List<&2, Loc>, +name: String) -> Maybe<&2, R.Ty>:
  match ls:
    case Nil{}:
      None{}
    case Con{Loc{n, +t}, rest}:
      Bool.pick(Maybe<&2, R.Ty>, String.eq(n, name), Some{t}, find_loc(rest, name))

def find_fields(vs: List<&2, R.Variant>, +ctor: String) -> Maybe<&2, List<&2, R.Ty>>:
  match vs:
    case Nil{}:
      None{}
    case Con{R.Variant{n, +fs}, rest}:
      Bool.pick(Maybe<&2, List<&2, R.Ty>>, String.eq(n, ctor), Some{fs}, find_fields(rest, ctor))

def is_generic(f: Maybe<&2, Fam>) -> Bool:
  match f:
    case Some{Fam{n, Nil{}, vs}}:
      False{}
    case other:
      True{}

def env_sigs(e: Env) -> List<&2, Sig>:
  match e:
    case Env{sigs, fams, gens}:
      sigs

def env_fams(e: Env) -> List<&2, Fam>:
  match e:
    case Env{sigs, fams, gens}:
      fams

def env_gens(e: Env) -> List<&2, String>:
  match e:
    case Env{sigs, fams, gens}:
      gens

def fn_env(e: Env, gs: List<&2, String>) -> Env:
  match e:
    case Env{sigs, fams, gens}:
      Env{sigs, fams, gs}

# ---- the walker (CD9) ----

# JChk: check `e` against `want` with the locals in scope.
type Job is Data:
  JChk{locals: List<&2, Loc>, e: R.Ex, want: R.Ty}
  JArm{locals: List<&2, Loc>, fam: Fam, args: List<&2, R.Ty>, arm: R.Arm<R.Ex>, want: R.Ty}

type St is Data:
  St{jobs: List<&2, Job>, notes: List<&2, Note>}

# Synthesis: the type of the scrutinee and the jobs of its parts.
type Syn is Data:
  Syn{ty: R.Ty, jobs: List<&2, Job>}

def cons_job(j: Job, m: Maybe<&2, List<&2, Job>>) -> Maybe<&2, List<&2, Job>>:
  match m:
    case Some{js}:
      Some{j <> js}
    case None{}:
      None{}

def chk_all(+ls: List<&2, Loc>, es: List<&2, R.Ex>, ts: List<&2, R.Ty>) -> Maybe<&2, List<&2, Job>>:
  match es ts:
    case Nil{} Nil{}:
      Some{[]}
    case Con{e, er} Con{t, tr}:
      cons_job(JChk{ls, e, strip(t)}, chk_all(ls, er, tr))
    case er tr:
      None{}

def mismatch() -> String:
  "type mismatch (CD9)"

def arity() -> String:
  "argument count (CD9)"

def no_want() -> String:
  "constructor of a generic family with no expected type (CD9)"

def args_jobs(m: Maybe<&2, List<&2, Job>>) -> Result<&2, &2, String, List<&2, Job>>:
  match m:
    case Some{js}:
      Done{js}
    case None{}:
      Fail{arity()}

def same_len(xs: List<&2, R.Ty>, ys: List<&2, String>) -> Bool:
  match xs ys:
    case Nil{} Nil{}:
      True{}
    case Con{x, xr} Con{y, yr}:
      same_len(xr, yr)
    case xr yr:
      False{}

def type_arity() -> String:
  "type argument count (CD9)"

def fam_arity(+args: List<&2, R.Ty>, +name: String, f: Maybe<&2, Fam>) -> Bool:
  match f:
    case Some{Fam{n, gs, vs}}:
      same_len(args, gs)
    case None{}:
      (String.eq(name, "Nat") || String.eq(name, "u32")) && same_len(args, [])

@unsafe def valid_tys(+e: Env, +gens: List<&2, String>, ts: List<&2, R.Ty>) -> Bool:
  match ts:
    case Nil{}:
      True{}
    case Con{R.TBool{}, rest}:
      valid_tys(e, gens, rest)
    case Con{R.TCon{+name, +args}, rest}:
      Bool.pick(Bool, Et.has(gens, name), same_len(args, []), fam_arity(args, name, find_fam(env_fams(e), name))) && valid_tys(e, gens, List.append(&2, R.Ty, args, rest))
    case Con{R.TBox{i}, rest}:
      valid_tys(e, gens, i <> rest)
    case Con{R.TRef{i}, rest}:
      valid_tys(e, gens, i <> rest)
    case Con{R.TFn{args, ret}, rest}:
      valid_tys(e, gens, ret <> List.append(&2, R.Ty, args, rest))

def syn_of(t: R.Ty, r: Result<&2, &2, String, List<&2, Job>>) -> Result<&2, &2, String, Syn>:
  match r:
    case Done{js}:
      Done{Syn{t, js}}
    case Fail{m}:
      Fail{m}

# A call of a fn of the table: the result type and the jobs of the arguments.
def call_sig(+ls: List<&2, Loc>, +targs: List<&2, R.Ty>, args: List<&2, R.Ex>, s: Maybe<&2, Sig>) -> Result<&2, &2, String, Syn>:
  match s:
    case None{}:
      Fail{"unknown function (CD9)"}
    case Some{Sig{n, +gs, ps, r}}:
      Bool.pick(Result<&2, &2, String, Syn>, same_len(targs, gs),
        syn_of(strip(subst(gs, targs, r)), args_jobs(chk_all(ls, args, substs(gs, targs, ps)))),
        Fail{arity()})

def call_local(+ls: List<&2, Loc>, args: List<&2, R.Ex>, t: R.Ty) -> Result<&2, &2, String, Syn>:
  match t:
    case R.TFn{xs, r}:
      syn_of(strip(r), args_jobs(chk_all(ls, args, xs)))
    case other:
      Fail{"call of a local that is not a function (CD9)"}

def call(+e: Env, +ls: List<&2, Loc>, +func: String, +targs: List<&2, R.Ty>, args: List<&2, R.Ex>, l: Maybe<&2, R.Ty>) -> Result<&2, &2, String, Syn>:
  match l:
    case Some{t}:
      Bool.pick(Result<&2, &2, String, Syn>, same_len(targs, []), call_local(ls, args, strip(t)), Fail{arity()})
    case None{}:
      Bool.pick(Result<&2, &2, String, Syn>, valid_tys(e, env_gens(e), targs), call_sig(ls, targs, args, find_sig(env_sigs(e), func)), Fail{type_arity()})

def var_syn(+name: String, l: Maybe<&2, R.Ty>) -> Result<&2, &2, String, Syn>:
  match l:
    case Some{t}:
      Done{Syn{strip(t), []}}
    case None{}:
      Fail{"unknown local `" ++ name ++ "` (CD9)"}

# The type of `e` in a synthesis position (a scrutinee).
@unsafe def synth(+e: Env, +ls: List<&2, Loc>, x: R.Ex) -> Result<&2, &2, String, Syn>:
  match x:
    case R.XVar{+name}:
      var_syn(name, find_loc(ls, name))
    case R.XCall{+func, targs, args}:
      call(e, ls, func, targs, args, find_loc(ls, func))
    case R.XCtor{+family, ctor, args}:
      Bool.pick(Result<&2, &2, String, Syn>, is_generic(find_fam(env_fams(e), family)),
        Fail{no_want()},
        Done{Syn{R.TCon{family, []}, [JChk{ls, R.XCtor{family, ctor, args}, R.TCon{family, []}}]}})
    case R.XBorrow{i}:
      synth(e, ls, i)
    case R.XClone{i}:
      synth(e, ls, i)
    case R.XBox{i}:
      synth(e, ls, i)
    case other:
      Fail{no_want()}

def ok(+b: Bool, js: List<&2, Job>, ns: List<&2, Note>, +st: St) -> Result<&2, &2, String, St>:
  match st:
    case St{rest, acc}:
      Bool.pick(Result<&2, &2, String, St>, b, Done{St{List.append(&2, Job, js, rest), List.append(&2, Note, ns, acc)}}, Fail{mismatch()})

def got(+want: R.Ty, r: Result<&2, &2, String, Syn>, +st: St) -> Result<&2, &2, String, St>:
  match r:
    case Done{Syn{t, js}}:
      ok(teq(t, want), js, [], st)
    case Fail{m}:
      Fail{m}

def ctor_ok(+wargs: List<&2, R.Ty>, r: Result<&2, &2, String, List<&2, Job>>, +st: St) -> Result<&2, &2, String, St>:
  match r:
    case Done{js}:
      ok(True{}, js, [NCtor{wargs}], st)
    case Fail{m}:
      Fail{m}

def ctor_fields(+ls: List<&2, Loc>, +wargs: List<&2, R.Ty>, +gs: List<&2, String>, args: List<&2, R.Ex>, f: Maybe<&2, List<&2, R.Ty>>, +st: St) -> Result<&2, &2, String, St>:
  match f:
    case Some{fs}:
      Bool.pick(Result<&2, &2, String, St>, same_len(wargs, gs),
        ctor_ok(wargs, args_jobs(chk_all(ls, args, substs(gs, wargs, fs))), st),
        Fail{mismatch()})
    case None{}:
      Fail{"unknown constructor (CD9)"}

def ctor_jobs(+ls: List<&2, Loc>, +wargs: List<&2, R.Ty>, args: List<&2, R.Ex>, f: Maybe<&2, Fam>, +ctor: String, +st: St) -> Result<&2, &2, String, St>:
  match f:
    case Some{Fam{n, +gs, vs}}:
      ctor_fields(ls, wargs, gs, args, find_fields(vs, ctor), st)
    case None{}:
      Fail{"unknown family (CD9)"}

def arm_list(+ls: List<&2, Loc>, +fam: Fam, +args: List<&2, R.Ty>, arms: List<&2, R.Arm<R.Ex>>, +want: R.Ty) -> List<&2, Job>:
  match arms:
    case Nil{}:
      []
    case Con{a, rest}:
      JArm{ls, fam, args, a, want} <> arm_list(ls, fam, args, rest, want)

def remove_variant(vs: List<&2, R.Variant>, +ctor: String) -> List<&2, R.Variant>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{+n, fs}, +rest}:
      Bool.pick(List<&2, R.Variant>, String.eq(n, ctor), rest, R.Variant{n, fs} <> remove_variant(rest, ctor))

def found_fields(f: Maybe<&2, List<&2, R.Ty>>) -> Bool:
  match f:
    case Some{fs}:
      True{}
    case None{}:
      False{}

def covers(arms: List<&2, R.Arm<R.Ex>>, vs: List<&2, R.Variant>, +name: String) -> Bool:
  match arms vs:
    case Nil{} Nil{}:
      True{}
    case Nil{} Con{v, rest}:
      False{}
    case Con{R.Arm{R.PCtor{pf, +pc, bs}, body}, rest} +remaining:
      String.eq(pf, name) && found_fields(find_fields(remaining, pc)) && covers(rest, remove_variant(remaining, pc), name)

def match_jobs(+ls: List<&2, Loc>, +arms: List<&2, R.Arm<R.Ex>>, +want: R.Ty, f: Maybe<&2, Fam>, args: List<&2, R.Ty>, js: List<&2, Job>, +st: St) -> Result<&2, &2, String, St>:
  match f:
    case Some{Fam{+n, +gs, +vs}}:
      Bool.pick(Result<&2, &2, String, St>, covers(arms, vs, n), ok(True{}, List.append(&2, Job, js, arm_list(ls, Fam{n, gs, vs}, args, arms, want)), [NMatch{want}], st), Fail{"match arms (CD9)"})
    case None{}:
      Fail{"match on a type that is not a family (CD9)"}

def scrut_fam(+e: Env, t: R.Ty) -> Maybe<&2, Fam>:
  match t:
    case R.TCon{+n, xs}:
      find_fam(env_fams(e), n)
    case other:
      None{}

def scrut_args(t: R.Ty) -> List<&2, R.Ty>:
  match t:
    case R.TCon{n, xs}:
      xs
    case other:
      []

def on_match(+e: Env, +ls: List<&2, Loc>, arms: List<&2, R.Arm<R.Ex>>, +want: R.Ty, r: Result<&2, &2, String, Syn>, +st: St) -> Result<&2, &2, String, St>:
  match r:
    case Done{Syn{+t, js}}:
      match_jobs(ls, arms, want, scrut_fam(e, t), scrut_args(t), js, st)
    case Fail{m}:
      Fail{m}

def binds(bs: List<&2, R.Bind>, fs: List<&2, R.Ty>, ls: List<&2, Loc>) -> List<&2, Loc>:
  match bs fs:
    case Con{R.BVar{n}, br} Con{t, fr}:
      binds(br, fr, Loc{n, t} <> ls)
    case Con{b, br} Con{t, fr}:
      binds(br, fr, ls)
    case br fr:
      ls

def bind_arity(bs: List<&2, R.Bind>, fs: List<&2, R.Ty>) -> Bool:
  match bs fs:
    case Nil{} Nil{}:
      True{}
    case Con{b, br} Con{f, fr}:
      bind_arity(br, fr)
    case br fr:
      False{}

def closure_locs(ps: List<&2, R.Param>, ls: List<&2, Loc>) -> List<&2, Loc>:
  match ps:
    case Nil{}:
      ls
    case Con{R.Param{n, t}, rest}:
      closure_locs(rest, Loc{n, t} <> ls)

def arm_step(+ls: List<&2, Loc>, +gs: List<&2, String>, +args: List<&2, R.Ty>, body: R.Ex, +want: R.Ty, +bs: List<&2, R.Bind>, f: Maybe<&2, List<&2, R.Ty>>, +st: St) -> Result<&2, &2, String, St>:
  match f:
    case Some{+fs}:
      Bool.pick(Result<&2, &2, String, St>, bind_arity(bs, fs), ok(True{}, [JChk{binds(bs, substs(gs, args, fs), ls), body, want}], [], st), Fail{"pattern field count (CD9)"})
    case None{}:
      Fail{"unknown constructor (CD9)"}

def step(+e: Env, j: Job, +st: St) -> Result<&2, &2, String, St>:
  match j:
    case JArm{+ls, Fam{fam_name, +gs, vs}, +args, R.Arm{R.PCtor{pf, +pc, bs}, body}, +want}:
      arm_step(ls, gs, args, body, want, bs, find_fields(vs, pc), st)
    case JChk{+ls, R.XVar{+name}, +want}:
      got(want, var_syn(name, find_loc(ls, name)), st)
    case JChk{ls, R.XInt{text}, +want}:
      ok(teq(u32(), want), [], [], st)
    case JChk{ls, R.XBool{v}, +want}:
      ok(teq(R.TBool{}, want), [], [], st)
    case JChk{+ls, R.XCall{+func, targs, args}, +want}:
      got(want, call(e, ls, func, targs, args, find_loc(ls, func)), st)
    case JChk{+ls, R.XCtor{+family, +ctor, args}, R.TCon{+wn, +wargs}}:
      Bool.pick(Result<&2, &2, String, St>, String.eq(family, wn), ctor_jobs(ls, wargs, args, find_fam(env_fams(e), family), ctor, st), Fail{mismatch()})
    case JChk{+ls, R.XMatch{scrut, arms}, +want}:
      on_match(e, ls, arms, want, synth(e, ls, scrut), st)
    case JChk{+ls, R.XIf{c, y, n}, +want}:
      ok(True{}, [JChk{ls, c, R.TBool{}}, JChk{ls, y, want}, JChk{ls, n, want}], [], st)
    case JChk{+ls, R.XClosure{+ps, body}, R.TFn{xs, r}}:
      ok(teqs(strips(param_tys(ps)), xs), [JChk{closure_locs(ps, ls), body, r}], [], st)
    case JChk{+ls, R.XBorrow{i}, +want}:
      ok(True{}, [JChk{ls, i, want}], [], st)
    case JChk{+ls, R.XClone{i}, +want}:
      ok(True{}, [JChk{ls, i, want}], [], st)
    case JChk{+ls, R.XBox{i}, +want}:
      ok(True{}, [JChk{ls, i, want}], [], st)
    case other:
      Fail{mismatch()}

def rev(xs: List<&2, Note>, acc: List<&2, Note>) -> List<&2, Note>:
  match xs:
    case Nil{}:
      acc
    case Con{x, rest}:
      rev(rest, x <> acc)

@unsafe def walk(+e: Env, r: Result<&2, &2, String, St>) -> Result<&2, &2, String, List<&2, Note>>:
  match r:
    case Fail{m}:
      Fail{m}
    case Done{St{Nil{}, acc}}:
      Done{rev(acc, [])}
    case Done{St{Con{j, rest}, acc}}:
      walk(e, step(e, j, St{rest, acc}))

# ---- the crate ----

def typed_of(+module: String, +pos: A.Pos, +name: String, tele: Syntax.T, r: Result<&2, &2, String, List<&2, Note>>) -> Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>:
  match r:
    case Done{ns}:
      Done{Some{Typed{module, name, binder_name(name), pos, tele, ns}}}
    case Fail{m}:
      Fail{Rs.Bad{module, pos, m}}

def fn_typed(+e: Env, +copies: List<&2, String>, +module: String, +pos: A.Pos, +name: String, +gs: List<&2, String>, +ps: List<&2, R.Param>, +rt: R.Ty, body: R.Ex) -> Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>:
  typed_of(module, pos, name, telescope(copies, gs, ps, rt), walk(fn_env(e, gs), Done{St{[JChk{closure_locs(ps, []), body, strip(rt)}], []}}))

def variant_tys(vs: List<&2, R.Variant>) -> List<&2, R.Ty>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fs}, rest}:
      List.append(&2, R.Ty, fs, variant_tys(rest))

def item_typed(+e: Env, +copies: List<&2, String>, +module: String, p: Rs.Placed) -> Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>:
  match p:
    case Rs.Placed{+pos, R.IFn{name, +gs, +ps, +rt, body}}:
      Bool.pick(Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>, valid_tys(e, gs, rt <> param_tys(ps)), fn_typed(e, copies, module, pos, name, gs, ps, rt, body), Fail{Rs.Bad{module, pos, type_arity()}})
    case Rs.Placed{+pos, R.IEnum{name, gs, cp, vs}}:
      Bool.pick(Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>, valid_tys(e, gs, variant_tys(vs)), Done{None{}}, Fail{Rs.Bad{module, pos, type_arity()}})
    case other:
      Done{None{}}

def keep(m: Maybe<&2, Typed>, rest: List<&2, Typed>) -> List<&2, Typed>:
  match m:
    case Some{t}:
      t <> rest
    case None{}:
      rest

def keep_done(m: Maybe<&2, Typed>, r: Result<&2, &2, Rs.Bad, List<&2, Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match r:
    case Done{ts}:
      Done{keep(m, ts)}
    case Fail{b}:
      Fail{b}

# The first refusal wins: the refusal of this item before one of a later item.
def keep_item(m: Result<&2, &2, Rs.Bad, Maybe<&2, Typed>>, r: Result<&2, &2, Rs.Bad, List<&2, Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match m:
    case Done{x}:
      keep_done(x, r)
    case Fail{b}:
      Fail{b}

def append_done(ts: List<&2, Typed>, r: Result<&2, &2, Rs.Bad, List<&2, Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match r:
    case Done{us}:
      Done{List.append(&2, Typed, ts, us)}
    case Fail{b}:
      Fail{b}

def append_typed(m: Result<&2, &2, Rs.Bad, List<&2, Typed>>, r: Result<&2, &2, Rs.Bad, List<&2, Typed>>) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match m:
    case Done{ts}:
      append_done(ts, r)
    case Fail{b}:
      Fail{b}

def items_typed(ps: List<&2, Rs.Placed>, +e: Env, +copies: List<&2, String>, +module: String) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match ps:
    case Nil{}:
      Done{[]}
    case Con{p, rest}:
      keep_item(item_typed(e, copies, module, p), items_typed(rest, e, copies, module))

def mods_typed(ms: List<&2, Rs.Mod>, +e: Env, +copies: List<&2, String>) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match ms:
    case Nil{}:
      Done{[]}
    case Con{Rs.Mod{+name, file, items}, rest}:
      append_typed(items_typed(items, e, copies, name), mods_typed(rest, e, copies))

def infer(o: Rs.Out) -> Result<&2, &2, Rs.Bad, List<&2, Typed>>:
  match o:
    case Rs.Out{+mods, rows}:
      mods_typed(mods, mods_env(mods, Env{nat_sigs(), [], []}), mods_copies(mods, []))

def types_text(ts: List<&2, Typed>) -> String:
  match ts:
    case Nil{}:
      ""
    case Con{Typed{module, rust, mech, pos, tele, ns}, rest}:
      module ++ " " ++ mech ++ " : " ++ Syntax.show(tele) ++ " annotated " ++ Nat.show(List.length(&2, Note, ns)) ++ "\n" ++ types_text(rest)
