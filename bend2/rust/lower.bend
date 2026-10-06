import Base
import ./ast.bend as A
import ./rir.bend as R
import ./resolve.bend as Rs
import ./erase_typed.bend as Et
import ./infer.bend as In
import ../kernel/bignum.bend as Bignum
import ../kernel/quantity.bend as Quantity
import ../surface/syntax.bend as Syntax
import ../surface/lexer.bend as Lex
import ../surface/token.bend as Token

# Lower of the Rust importer (mech-rust C4, CD6, CD7, CD10, CD11).
# Input: the resolved crate of resolve.bend, after infer.bend gave no refusal.
# Output: one `.mech` text for each module in manifest order (the `nat`
# module has no file, CD7), printed with Syntax.print (CD11, C-LEARN L4).
# Every expression retains its checking type and local typing context.
# `lower` drops XBorrow, XClone, XBox,
# TBox and TRef (CD8); the emitter makes them again.

type Low is Data:
  Low{file: String, text: String}

def type0() -> Syntax.T:
  Syntax.SType{Bignum.zero()}

# The head of a list of one term (the default is never used).
def one_t(ts: List<&2, Syntax.T>) -> Syntax.T:
  match ts:
    case Con{t, rest}:
      t
    case Nil{}:
      Syntax.SVar{"MechBool"}

def nat_lit(m: Maybe<&2, Bignum.T>) -> Syntax.T:
  match m:
    case Some{n}:
      Syntax.SNat{n}
    case None{}:
      Syntax.SNat{Bignum.zero()}

# CD7: nat_add is natAdd, nat_small(<n>) is the literal <n>.
def head_name(+func: String) -> String:
  Bool.pick(String, String.eq(func, "nat_add"), "natAdd", In.binder_name(func))

# Type arguments first, then the arguments.
def call_t(+func: String, head: String, ms: List<&2, Syntax.T>, +ts: List<&2, Syntax.T>, local: Bool) -> Syntax.T:
  Bool.pick(Syntax.T, String.eq(func, "nat_small") && Bool.pick(Bool, local, False{}, True{}), one_t(ts), In.apps(List.append(&2, Syntax.T, ms, ts), Syntax.SVar{head}))

def is_word(+s: String) -> Bool:
  Bool.pick(Bool, String.eq(s, ""), False{}, True{})

# The emitter writes an unused local binder `x` as `_x`; `_x` lowers to `x`.
def under_rest(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{+c, +rest}:
      Bool.pick(String, String.eq(SCon{c, SNil{}}, "_") && is_word(rest), rest, SCon{c, rest})

def identifier(k: Token.KindT, name: String) -> String:
  match k:
    case Token.Ident{raw}:
      name
    case other:
      name ++ "Local"

def local_name(name: String) -> String:
  +candidate = In.binder_name(under_rest(name))
  identifier(Lex.ident_kind(candidate), candidate)

# One injective map for all local spellings in a function. Repeated spellings
# still shadow normally. Fresh names also avoid globals and generic binders.
type Name is Data:
  Name{rust: String, mech: String}

def lookup(ns: List<&2, Name>, +raw: String) -> String:
  match ns:
    case Nil{}:
      local_name(raw)
    case Con{Name{r, m}, rest}:
      Bool.pick(String, String.eq(r, raw), m, lookup(rest, raw))

def named(ns: List<&2, Name>, +raw: String) -> Bool:
  match ns:
    case Nil{}:
      False{}
    case Con{Name{r, m}, rest}:
      String.eq(r, raw) || named(rest, raw)

def has(ss: List<&2, String>, +s: String) -> Bool:
  match ss:
    case Nil{}:
      False{}
    case Con{x, rest}:
      String.eq(x, s) || has(rest, s)

def fresh(fuel: List<&2, String>, +used: List<&2, String>, +s: String) -> String:
  match fuel:
    case Nil{}:
      s
    case Con{x, rest}:
      Bool.pick(String, has(used, s), fresh(rest, used, s ++ "Local"), s)

def name_map(raws: List<&2, String>, +used: List<&2, String>, +ns: List<&2, Name>) -> List<&2, Name>:
  match raws:
    case Nil{}:
      ns
    case Con{+raw, rest}:
      +m = fresh(used, used, local_name(raw))
      +seen = named(ns, raw)
      name_map(rest, Bool.pick(List<&2, String>, seen, used, m <> used), Bool.pick(List<&2, Name>, seen, ns, Name{raw, m} <> ns))

def param_names(ps: List<&2, R.Param>) -> List<&2, String>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{n, t}, rest}:
      n <> param_names(rest)

def bind_names(bs: List<&2, R.Bind>) -> List<&2, String>:
  match bs:
    case Nil{}:
      []
    case Con{R.BVar{n}, rest}:
      n <> bind_names(rest)
    case Con{R.BSkip{}, rest}:
      bind_names(rest)

def arm_names(arms: List<&2, R.Arm<R.Ex>>) -> List<&2, String>:
  match arms:
    case Nil{}:
      []
    case Con{R.Arm{R.PCtor{f, c, bs}, body}, rest}:
      List.append(&2, String, bind_names(bs), arm_names(rest))

# A BSkip is a binder with no use.
def field_of(b: R.Bind, +ns: List<&2, Name>) -> Syntax.Field<Syntax.T>:
  match b:
    case R.BVar{name}:
      Syntax.Field{Quantity.QMany{}, lookup(ns, name), None{}}
    case R.BSkip{}:
      Syntax.Field{Quantity.QMany{}, "_", None{}}

def fields_of(bs: List<&2, R.Bind>, +ns: List<&2, Name>) -> List<&2, Syntax.Field<Syntax.T>>:
  match bs:
    case Nil{}:
      []
    case Con{b, rest}:
      field_of(b, ns) <> fields_of(rest, ns)

def arm_bodies(arms: List<&2, R.Arm<R.Ex>>) -> List<&2, R.Ex>:
  match arms:
    case Nil{}:
      []
    case Con{R.Arm{p, body}, rest}:
      body <> arm_bodies(rest)

# `ts` are the lowered arm bodies, in the order of the arms.
def branches(arms: List<&2, R.Arm<R.Ex>>, ts: List<&2, Syntax.T>, +ns: List<&2, Name>) -> List<&2, Syntax.Branch<Syntax.T>>:
  match arms ts:
    case Con{R.Arm{R.PCtor{family, ctor, bs}, body}, rest} Con{t, tr}:
      Syntax.BrCtor{Et.lower_first(ctor), fields_of(bs, ns), t} <> branches(rest, tr, ns)
    case rest tr:
      []

# A `case` needs a motive (`as self in <family> return <type>`, as in
# prelude/init.mech): the return type is the expected type of the match.
def motive_of(+family: String, w: Maybe<&2, Syntax.T>) -> Maybe<&2, Syntax.Motive<Syntax.T>>:
  match w:
    case Some{t}:
      Some{Syntax.Motive{"self", Some{family}, [], t}}
    case None{}:
      None{}

# `ts` is the lowered scrutinee, then the lowered arm bodies.
def match_t(arms: List<&2, R.Arm<R.Ex>>, ts: List<&2, Syntax.T>, m: Maybe<&2, Syntax.Motive<Syntax.T>>, +ns: List<&2, Name>) -> Syntax.T:
  match ts:
    case Con{s, rest}:
      Syntax.SCase{s, m, branches(arms, rest, ns)}
    case Nil{}:
      Syntax.SVar{"MechBool"}

# `ts` is the condition, the yes branch and the no branch (CD6).
def if_t(ts: List<&2, Syntax.T>, m: Maybe<&2, Syntax.Motive<Syntax.T>>) -> Syntax.T:
  match ts:
    case Con{c, Con{y, Con{n, rest}}}:
      Syntax.SCase{c, m, [Syntax.BrCtor{"mechFalse", [], n}, Syntax.BrCtor{"mechTrue", [], y}]}
    case other:
      Syntax.SVar{"MechBool"}

# The family of a type (a reference or a box is its inner type).
def ty_family(t: R.Ty) -> String:
  match t:
    case R.TCon{name, args}:
      name
    case R.TRef{i}:
      ty_family(i)
    case R.TBox{i}:
      ty_family(i)
    case other:
      "MechBool"

# The same checking jobs as inference, including each binder's local scope.
type Job is Data:
  Job{locals: List<&2, In.Loc>, ex: R.Ex, want: R.Ty}

# All failures here are excluded by the successful In.infer precondition.
def step_jobs(r: Result<&2, &2, String, In.St>) -> List<&2, In.Job>:
  match r:
    case Done{In.St{js, notes}}:
      js
    case Fail{message}:
      []

@unsafe def jobs_of(+e: In.Env, js: List<&2, In.Job>) -> List<&2, Job>:
  match js:
    case Nil{}:
      []
    case Con{In.JChk{ls, x, w}, rest}:
      Job{ls, x, w} <> jobs_of(e, rest)
    case Con{+j, rest}:
      jobs_of(e, List.append(&2, In.Job, step_jobs(In.step(e, j, In.St{[], []})), rest))

def children(+e: In.Env, ls: List<&2, In.Loc>, x: R.Ex, w: R.Ty) -> List<&2, Job>:
  jobs_of(e, step_jobs(In.step(e, In.JChk{ls, x, w}, In.St{[], []})))

def match_fam_jobs(+e: In.Env, +ls: List<&2, In.Loc>, scrut: R.Ex, arms: List<&2, R.Arm<R.Ex>>, want: R.Ty, +t: R.Ty, f: Maybe<&2, In.Fam>) -> List<&2, Job>:
  match f:
    case Some{fam}:
      Job{ls, scrut, t} <> jobs_of(e, In.arm_list(ls, fam, In.scrut_args(t), arms, want))
    case None{}:
      []

def match_jobs(+e: In.Env, +ls: List<&2, In.Loc>, scrut: R.Ex, arms: List<&2, R.Arm<R.Ex>>, want: R.Ty, r: Result<&2, &2, String, In.Syn>) -> List<&2, Job>:
  match r:
    case Done{In.Syn{+t, js}}:
      match_fam_jobs(e, ls, scrut, arms, want, t, In.scrut_fam(e, t))
    case Fail{message}:
      []

def scrut_type(js: List<&2, Job>) -> R.Ty:
  match js:
    case Con{Job{ls, x, w}, rest}:
      w
    case Nil{}:
      R.TBool{}

def is_local(m: Maybe<&2, R.Ty>) -> Bool:
  match m:
    case Some{t}:
      True{}
    case None{}:
      False{}

def param_binders(ps: List<&2, R.Param>, +copies: List<&2, String>, +ns: List<&2, Name>) -> List<&2, Syntax.Binder<Syntax.T>>:
  match ps:
    case Nil{}:
      []
    case Con{R.Param{name, +t}, rest}:
      Syntax.Binder{In.qty(copies, t), lookup(ns, name), In.mty(copies, t)} <> param_binders(rest, copies, ns)

def gen_binders(gs: List<&2, String>) -> List<&2, Syntax.Binder<Syntax.T>>:
  match gs:
    case Nil{}:
      []
    case Con{g, rest}:
      Syntax.Binder{Quantity.QZero{}, g, type0()} <> gen_binders(rest)

# A fn with no binder is its body.
def fun_of(bs: List<&2, Syntax.Binder<Syntax.T>>, body: Syntax.T) -> Syntax.T:
  match bs:
    case Nil{}:
      body
    case Con{b, rest}:
      Syntax.SFun{b <> rest, body}

# Reuse inference's child jobs so calls, constructors and closures retain
# their expected types. A constructor is annotated even in synthesis position.
@unsafe def lows(+e: In.Env, +copies: List<&2, String>, +ns: List<&2, Name>, js: List<&2, Job>) -> List<&2, Syntax.T>:
  match js:
    case Nil{}:
      []
    case Con{Job{ls, R.XVar{name}, w}, rest}:
      Syntax.SVar{lookup(ns, name)} <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XInt{text}, w}, rest}:
      nat_lit(Bignum.of_decimal(text)) <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XBool{v}, w}, rest}:
      Syntax.SVar{Bool.pick(String, v, "mechTrue", "mechFalse")} <> lows(e, copies, ns, rest)
    case Con{Job{+ls, R.XCall{+func, +targs, +args}, w}, rest}:
      +local = is_local(In.find_loc(ls, func))
      head = Bool.pick(String, local, lookup(ns, func), head_name(func))
      call_t(func, head, In.mtys(copies, targs), lows(e, copies, ns, children(e, ls, R.XCall{func, targs, args}, w)), local) <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XCtor{+family, +ctor, +args}, +w}, rest}:
      t = In.apps(lows(e, copies, ns, children(e, ls, R.XCtor{family, ctor, args}, w)), Syntax.SVar{Et.lower_first(ctor)})
      Syntax.SAnn{t, In.mty(copies, w)} <> lows(e, copies, ns, rest)
    case Con{Job{+ls, R.XMatch{+scrut, +arms}, +w}, rest}:
      +jobs = match_jobs(e, ls, scrut, arms, w, In.synth(e, ls, scrut))
      match_t(arms, lows(e, copies, ns, jobs), motive_of(ty_family(scrut_type(jobs)), Some{In.mty(copies, w)}), ns) <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XIf{c, y, n}, +w}, rest}:
      if_t(lows(e, copies, ns, children(e, ls, R.XIf{c, y, n}, w)), motive_of("MechBool", Some{In.mty(copies, w)})) <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XClosure{+cps, body}, w}, rest}:
      fun_of(param_binders(cps, copies, ns), one_t(lows(e, copies, ns, children(e, ls, R.XClosure{cps, body}, w)))) <> lows(e, copies, ns, rest)
    case Con{Job{ls, R.XBorrow{i}, w}, rest}:
      lows(e, copies, ns, Job{ls, i, w} <> rest)
    case Con{Job{ls, R.XClone{i}, w}, rest}:
      lows(e, copies, ns, Job{ls, i, w} <> rest)
    case Con{Job{ls, R.XBox{i}, w}, rest}:
      lows(e, copies, ns, Job{ls, i, w} <> rest)

# CD10: a fn that calls itself is a `def rec`.
@unsafe def calls(+name: String, es: List<&2, R.Ex>) -> Bool:
  match es:
    case Nil{}:
      False{}
    case Con{R.XVar{x}, rest}:
      calls(name, rest)
    case Con{R.XInt{t}, rest}:
      calls(name, rest)
    case Con{R.XBool{v}, rest}:
      calls(name, rest)
    case Con{R.XCall{func, targs, args}, rest}:
      Bool.pick(Bool, String.eq(func, name), True{}, calls(name, List.append(&2, R.Ex, args, rest)))
    case Con{R.XCtor{f, c, args}, rest}:
      calls(name, List.append(&2, R.Ex, args, rest))
    case Con{R.XMatch{scrut, arms}, rest}:
      calls(name, scrut <> List.append(&2, R.Ex, arm_bodies(arms), rest))
    case Con{R.XIf{c, y, n}, rest}:
      calls(name, List.append(&2, R.Ex, [c, y, n], rest))
    case Con{R.XClosure{ps, body}, rest}:
      calls(name, body <> rest)
    case Con{R.XBorrow{i}, rest}:
      calls(name, i <> rest)
    case Con{R.XClone{i}, rest}:
      calls(name, i <> rest)
    case Con{R.XBox{i}, rest}:
      calls(name, i <> rest)

def rec_or_def(r: Bool, +name: String, +tele: Syntax.T, +body: Syntax.T) -> Syntax.Decl:
  Bool.pick(Syntax.Decl, r, Syntax.DRec{[Syntax.RecDef{name, tele, body}]}, Syntax.DDef{name, tele, body})

@unsafe def all_names(es: List<&2, R.Ex>) -> List<&2, String>:
  match es:
    case Nil{}:
      []
    case Con{R.XVar{x}, rest}:
      all_names(rest)
    case Con{R.XInt{t}, rest}:
      all_names(rest)
    case Con{R.XBool{v}, rest}:
      all_names(rest)
    case Con{R.XCall{f, ts, args}, rest}:
      all_names(List.append(&2, R.Ex, args, rest))
    case Con{R.XCtor{f, c, args}, rest}:
      all_names(List.append(&2, R.Ex, args, rest))
    case Con{R.XMatch{scrut, +arms}, rest}:
      List.append(&2, String, arm_names(arms), all_names(scrut <> List.append(&2, R.Ex, arm_bodies(arms), rest)))
    case Con{R.XIf{c, y, n}, rest}:
      all_names(List.append(&2, R.Ex, [c, y, n], rest))
    case Con{R.XClosure{ps, body}, rest}:
      List.append(&2, String, param_names(ps), all_names(body <> rest))
    case Con{R.XBorrow{i}, rest}:
      all_names(i <> rest)
    case Con{R.XClone{i}, rest}:
      all_names(i <> rest)
    case Con{R.XBox{i}, rest}:
      all_names(i <> rest)

def arrows(bs: List<&2, Syntax.Binder<Syntax.T>>, body: Syntax.T) -> Syntax.T:
  match bs:
    case Nil{}:
      body
    case Con{b, rest}:
      Syntax.SArrow{b, arrows(rest, body)}

def fn_decl(e: In.Env, +globals: List<&2, String>, +copies: List<&2, String>, +name: String, +gs: List<&2, String>, +ps: List<&2, R.Param>, +ret: R.Ty, +body: R.Ex) -> Syntax.Decl:
  +ns = name_map(List.append(&2, String, param_names(ps), all_names([body])), List.append(&2, String, gs, globals), [])
  +bs = List.append(&2, Syntax.Binder<Syntax.T>, gen_binders(gs), param_binders(ps, copies, ns))
  rec_or_def(calls(name, [body]), In.binder_name(name), arrows(bs, In.mty(copies, ret)), fun_of(bs, one_t(lows(In.fn_env(e, gs), copies, ns, [Job{In.closure_locs(ps, []), body, In.strip(ret)}]))))

def gen_vars(gs: List<&2, String>) -> List<&2, Syntax.T>:
  match gs:
    case Nil{}:
      []
    case Con{g, rest}:
      Syntax.SVar{g} <> gen_vars(rest)

# A constructor field is a binder at quantity many of the constructor type.
def field_arrows(fs: List<&2, R.Ty>, +copies: List<&2, String>, res: Syntax.T) -> Syntax.T:
  match fs:
    case Nil{}:
      res
    case Con{f, rest}:
      Syntax.SArrow{Syntax.Binder{Quantity.QMany{}, "_", In.mty(copies, f)}, field_arrows(rest, copies, res)}

def fam_ctors(vs: List<&2, R.Variant>, +copies: List<&2, String>, +res: Syntax.T) -> List<&2, Syntax.FamCtor<Syntax.T>>:
  match vs:
    case Nil{}:
      []
    case Con{R.Variant{name, fs}, rest}:
      Syntax.FamCtor{Et.lower_first(name), field_arrows(fs, copies, res)} <> fam_ctors(rest, copies, res)

# IEnum: a `mu` family, the generics are quantity-0 type parameters.
def enum_decl(+copies: List<&2, String>, +name: String, +gs: List<&2, String>, vs: List<&2, R.Variant>) -> Syntax.Decl:
  Syntax.DMu{[Syntax.Fam{name, gen_binders(gs), type0(), fam_ctors(vs, copies, In.apps(gen_vars(gs), Syntax.SVar{name}))}]}

# IUnit: a family with one constructor.
def unit_decl(+name: String, ctor: String) -> Syntax.Decl:
  Syntax.DMu{[Syntax.Fam{name, [], type0(), [Syntax.FamCtor{In.binder_name(ctor), Syntax.SVar{name}}]}]}

# CD6: MechBool is `bool` on the way back and makes no Rust item.
def bool_decl() -> Syntax.Decl:
  Syntax.DMu{[Syntax.Fam{"MechBool", [], type0(), [Syntax.FamCtor{"mechFalse", Syntax.SVar{"MechBool"}}, Syntax.FamCtor{"mechTrue", Syntax.SVar{"MechBool"}}]}]}

def item_decls(ps: List<&2, Rs.Placed>, +copies: List<&2, String>, +e: In.Env, +globals: List<&2, String>) -> List<&2, Syntax.Decl>:
  match ps:
    case Nil{}:
      []
    case Con{Rs.Placed{pos, R.IFn{name, gs, prms, ret, body}}, rest}:
      fn_decl(e, globals, copies, name, gs, prms, ret, body) <> item_decls(rest, copies, e, globals)
    case Con{Rs.Placed{pos, R.IEnum{name, gs, cp, vs}}, rest}:
      enum_decl(copies, name, gs, vs) <> item_decls(rest, copies, e, globals)
    case Con{Rs.Placed{pos, R.IUnit{name, ctor}}, rest}:
      unit_decl(name, ctor) <> item_decls(rest, copies, e, globals)
    case Con{Rs.Placed{pos, R.IUse{m}}, rest}:
      item_decls(rest, copies, e, globals)

def with_bool(first: Bool, +ds: List<&2, Syntax.Decl>) -> List<&2, Syntax.Decl>:
  Bool.pick(List<&2, Syntax.Decl>, first, bool_decl() <> ds, ds)

def not_nat(m: Rs.Mod, +rest: List<&2, Rs.Mod>) -> List<&2, Rs.Mod>:
  match m:
    case Rs.Mod{+name, +file, +items}:
      Bool.pick(List<&2, Rs.Mod>, String.eq(name, "nat"), rest, Rs.Mod{name, file, items} <> rest)

def code_mods(ms: List<&2, Rs.Mod>) -> List<&2, Rs.Mod>:
  match ms:
    case Nil{}:
      []
    case Con{m, rest}:
      not_nat(m, code_mods(rest))

def mods_low(ms: List<&2, Rs.Mod>, first: Bool, +copies: List<&2, String>, +e: In.Env, +globals: List<&2, String>) -> List<&2, Low>:
  match ms:
    case Nil{}:
      []
    case Con{Rs.Mod{name, file, items}, rest}:
      Low{file, Syntax.print(with_bool(first, item_decls(items, copies, e, globals)))} <> mods_low(rest, False{}, copies, e, globals)

def global_names(rows: List<&2, Rs.Row>) -> List<&2, String>:
  match rows:
    case Nil{}:
      ["MechBool", "mechTrue", "mechFalse", "Nat", "natAdd"]
    case Con{Rs.Row{module, kind, rust, mech, pos}, rest}:
      mech <> global_names(rest)

def lower(o: Rs.Out) -> List<&2, Low>:
  match o:
    case Rs.Out{+mods, rows}:
      mods_low(code_mods(mods), True{}, In.mods_copies(mods, []), In.mods_env(mods, In.Env{In.nat_sigs(), [], []}), global_names(rows))

# Mode `lower`: a `-- file <name>` comment line, then the text of the file.
def files_text(ls: List<&2, Low>) -> String:
  match ls:
    case Nil{}:
      ""
    case Con{Low{file, text}, rest}:
      "-- file " ++ file ++ "\n" ++ text ++ files_text(rest)

# Mode `check`: the files joined in manifest order (as `kanon build`, CF11).
def source(ls: List<&2, Low>) -> String:
  match ls:
    case Nil{}:
      ""
    case Con{Low{file, text}, rest}:
      text ++ "\n" ++ source(rest)
