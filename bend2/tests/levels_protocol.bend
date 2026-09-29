import Base
import ./levels_regression.bend as Algebra
import ../kernel/common.bend as C
import ../kernel/level.bend as L
import ../kernel/level_var.bend as LV
import ../kernel/level_eq.bend as LE
import ../kernel/bignum.bend as Big
import ../kernel/budget.bend as B
import ../kernel/check.bend as Check
import ../kernel/check_engine.bend as Engine
import ../kernel/error.bend as E
import ../kernel/eval.bend as Eval
import ../kernel/global.bend as G
import ../kernel/literal.bend as Lit
import ../kernel/positivity.bend as P
import ../kernel/prim.bend as Prim
import ../kernel/quantity.bend as Q
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/value.bend as V
import ../surface/poly.bend as Poly

def R(-A: Data) -> Data:
  Result<&2, &2, String, A>

def require(ok: Bool, message: String) -> R(Unit):
  match ok:
    case True{}: Done{Unit{}}
    case False{}: Fail{message}

def kernel(-A: Data, result: Result<&2, &2, E.T, A>) -> R(A):
  match result:
    case Done{value}: Done{value}
    case Fail{error}: Fail{E.to_string(error)}

def expect_message(expected: Maybe<&2, String>, +actual: String) -> R(Unit):
  match expected:
    case None{}: Done{Unit{}}
    case Some{message}: require(String.eq(message, actual), "wrong diagnostic: " ++ actual)

def expect(-A: Data, result: Result<&2, &2, E.T, A>, prefix: String, message: Maybe<&2, String>) -> R(Unit):
  match result:
    case Done{value}: Fail{"expected " ++ prefix ++ " refusal"}
    case Fail{+error}: do Result<&2, &2, String, Unit>:
      ok : Unit <- require(String.starts_with(E.to_string(error), prefix ++ ":"), "wrong refusal: " ++ E.to_string(error))
      expect_message(message, E.message(error))

def scope(-A: Data, result: Result<&2, &2, E.T, A>) -> R(Unit):
  expect(A, result, "universe", Some{"universe level is outside the global parameter scope"})

def collision(-A: Data, result: Result<&2, &2, E.T, A>, name: String) -> R(Unit):
  expect(A, result, "mismatch", Some{"the name " ++ name ++ " is already declared"})

def same(expected: L.T, actual: L.T) -> R(Unit):
  require(L.equal(expected, actual), "universe differs")

def universe(expected: L.T, value: V.T) -> R(Unit):
  match value:
    case V.VUniv{actual}: same(expected, actual)
    case _: Fail{"expected a universe value"}

def absent(-A: Data, value: Maybe<&2, A>) -> Bool:
  match value:
    case None{}: True{}
    case Some{item}: False{}

def definition(name: String, +level: L.T) -> Check.Decl:
  Check.Decl{name, Check.Definition{}, T.Univ{L.succ(level)}, Some{T.Univ{level}}}

def body(decl: Check.Decl, body: T.T) -> Check.Decl:
  match decl:
    case Check.Decl{name, kind, ty, old}: Check.Decl{name, kind, ty, Some{body}}

def distributed(name: String) -> Check.Decl:
  +u = L.var(0n)
  +v = L.var(1n)
  +w = L.var(2n)
  body(definition(name, L.max(L.imax(u,v),L.imax(u,w))), T.Univ{L.imax(u,L.max(v,w))})

def primitive_count(globals: G.T) -> Nat:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: List.length(&2, C.Pair<String,G.Entry>, entries)

def stored_entry(entry: Maybe<&2, G.Entry>, +globals: G.T, +name: String, +level: L.T, +reference: T.T) -> R(Unit):
  match entry:
    case Some{G.Def{G.DefEntry{ty, body, reducible, rec_arg, partial}}}: do Result<&2, &2, String, Unit>:
      ok : Unit <- require(T.equal(reference, T.Global{name}), "instantiation did not return its global reference")
      tyv : V.T <- kernel(V.T, Eval.eval(globals, [], ty))
      ok : Unit <- universe(L.succ(level), tyv)
      value : V.T <- kernel(V.T, Eval.eval(globals, [], body))
      ok : Unit <- universe(level, value)
      inferred : V.T <- kernel(V.T, Check.infer_term(globals, B.unlimited(), reference))
      universe(L.succ(level), inferred)
    case _: Fail{"instance was not installed"}

def stored(+globals: G.T, +name: String, level: L.T, reference: T.T) -> R(Unit):
  stored_entry(G.find(name,globals),globals,name,level,reference)

@unsafe def callback(limit: Maybe<&2, U32>) -> B.Callback:
  match limit:
    case None{}: B.of_poll(unit => (callback(None{}),False{}))
    case Some{+left}:
      +spent = U32.is_eq(left,0)
      B.of_poll(unit => (callback(Some{Bool.pick(U32,spent,0,U32.sub(left,1))}),spent))

@unsafe def count_step(-A: Data, -Z: Data, step: B.Step<A>, count: U32, done: Result<&2, &2, E.T, C.Pair<U32,Result<&2,&2,E.T,A>>> -> B.Step<Z>) -> B.Step<Z>:
  match step:
    case B.Finished{result}: done(Done{C.Pair{count,result}})
    case B.Poll{resume}: B.Poll{spent => count_step(A,Z,resume(spent),U32.add(count,1),done)}

def counted(-A: Data, action: B.Comp(A)) -> B.Comp(C.Pair<U32,Result<&2,&2,E.T,A>>):
  B.CompLater{Z => done => count_step(A,Z,B.start_comp(A,A,action,result => B.Finished{result}),0,done)}

def measure(-A: Data, action: B.Comp(A), limit: Maybe<&2,U32>) -> R(C.Pair<U32,Result<&2,&2,E.T,A>>):
  kernel(C.Pair<U32,Result<&2,&2,E.T,A>>,B.run_poll(C.Pair<U32,Result<&2,&2,E.T,A>>,counted(A,action),callback(limit)))

def completed(-A: Data, measured: C.Pair<U32,Result<&2,&2,E.T,A>>) -> R(C.Pair<U32,A>):
  match measured:
    case C.Pair{count,result}: do Result<&2,&2,String,C.Pair<U32,A>>:
      value : A <- kernel(A,result)
      return C.Pair{count,value}

def late_result(-A: Data, measured: C.Pair<U32,Result<&2,&2,E.T,A>>, +limit: U32, +complete: U32) -> R(Unit):
  match measured:
    case C.Pair{+count,result}: do Result<&2,&2,String,Unit>:
      ok : Unit <- expect(A,result,"budget",Some{"the check budget is exhausted"})
      ok : Unit <- require(U32.is_gt(count,limit),"comparison returned before the late poll threshold")
      require(U32.is_lt(count,complete),"comparison continued through all work after exhaustion")

def relation_tail(n: Nat, acc: List<&2,L.T>) -> List<&2,L.T>:
  match n:
    case 0n: acc
    case 1n+ +p: relation_tail(p,L.var(p) <> acc)

def max_all(levels: List<&2,L.T>, acc: L.T) -> L.T:
  match levels:
    case Nil{}: acc
    case Con{value,rest}: max_all(rest,L.max(acc,value))

def imax_all(levels: List<&2,L.T>, +head: L.T, acc: L.T) -> L.T:
  match levels:
    case Nil{}: acc
    case Con{value,rest}: imax_all(rest,head,L.max(acc,L.imax(head,value)))

def relation() -> C.Pair<L.T,L.T>:
  +levels = List.reverse(&2,L.T,relation_tail(11n,[]))
  +u = L.var(11n)
  C.Pair{L.imax(u,max_all(levels,L.zero())),imax_all(levels,u,L.zero())}

def comparison_finish(measured: C.Pair<U32,Bool>, +left: L.T, +right: L.T, equality: Bool) -> R(Unit):
  match measured:
    case C.Pair{+complete,equivalent}: do Result<&2,&2,String,Unit>:
      ok : Unit <- require(equivalent,"unbounded symbolic comparison changed its result")
      ok : Unit <- require(U32.is_gt(complete,64),"symbolic comparison did not poll during its work")
      +limit : U32 = U32.shr(complete)
      late : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- measure(Bool,LE.decide_m(left,right,equality),Some{limit})
      late_result(Bool,late,limit,complete)

def comparison_relation(relation: C.Pair<L.T,L.T>, +equality: Bool) -> R(Unit):
  match relation:
    case C.Pair{+left,+right}: do Result<&2,&2,String,Unit>:
      measured : C.Pair<U32,Result<&2,&2,E.T,Bool>> <- measure(Bool,LE.decide_m(left,right,equality),None{})
      done : C.Pair<U32,Bool> <- completed(Bool,measured)
      comparison_finish(done,left,right,equality)

def comparison_budget(equality: Bool) -> R(Unit):
  comparison_relation(relation(),equality)

def nat_shape(domain: T.T) -> S.T<T.T>:
  S.SPi{Q.QMany{},"x",domain}

def nat_identity() -> Check.Decl:
  Check.Decl{"shapeIdentity",Check.Definition{},T.Ran{nat_shape(Prim.nat_ty()),Prim.nat_ty()},Some{T.Sec{nat_shape(Prim.nat_ty()),[T.Leg{[C.Pair{Q.QMany{},"x"}],T.Var{0n}}]}}}

def ignored_shape(+globals: G.T, good: Check.Decl, +bad_body: T.T) -> R(Unit):
  match good:
    case Check.Decl{+name,+kind,+ty,+body}: do Result<&2,&2,String,Unit>:
      entry : G.Entry <- kernel(G.Entry,Check.check_decl(globals,B.unlimited(),Check.Decl{name,kind,ty,body}))
      expected : V.T <- kernel(V.T,Eval.eval(globals,[],ty))
      +bad : Check.Decl = Check.Decl{name,kind,ty,Some{bad_body}}
      ok : Unit <- scope(G.Entry,Check.check_decl(globals,B.unlimited(),bad))
      ok : Unit <- scope(Unit,Check.check_term(globals,B.unlimited(),bad_body,expected))
      scope(Unit,Check.check_scheme(globals,B.unlimited(),0n,bad))

def symbolic_prop() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    entry : G.Entry <- kernel(G.Entry,Check.check_decl(G.initial(),B.unlimited(),Check.Decl{"Proposition",Check.Postulate{},T.Univ{L.zero()},None{}}))
    kernel(Unit,Check.check_scheme(G.add("Proposition",entry,G.initial()),B.unlimited(),1n,Check.Decl{"impredicative",Check.Definition{},T.Univ{L.zero()},Some{T.Ran{S.SPi{Q.QMany{},"x",T.Univ{L.var(0n)}},T.Global{"Proposition"}}}}))

def impredicative() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    ty : V.T <- kernel(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Ran{S.SPi{Q.QZero{},"P",T.Univ{L.zero()}},T.Var{0n}}))
    universe(L.zero(),ty)

def hidden_scope() -> R(Unit):
  +hidden = L.imax(L.var(0n),L.zero())
  do Result<&2,&2,String,Unit>:
    ok : Unit <- same(L.zero(),hidden)
    scope(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Univ{hidden}))

def ignored_section() -> R(Unit):
  ignored_shape(G.initial(),nat_identity(),T.Sec{nat_shape(T.Univ{L.var(0n)}),[T.Leg{[C.Pair{Q.QMany{},"x"}],T.Var{0n}}]})

def pair_body(shape: S.T<T.T>) -> T.T:
  T.In{shape,T.APt{Q.QMany{},T.Lit{Lit.LInt{Big.zero()}}},[T.Lit{Lit.LInt{Big.zero()}}]}

def ignored_injection() -> R(Unit):
  ignored_shape(G.initial(),Check.Decl{"shapePair",Check.Definition{},T.Lan{nat_shape(Prim.nat_ty()),Prim.nat_ty()},Some{pair_body(nat_shape(Prim.nat_ty()))}},pair_body(nat_shape(T.Univ{L.var(0n)})))

def application_body(shape: S.T<T.T>) -> T.T:
  T.Out{shape,T.APt{Q.QMany{},T.Lit{Lit.LInt{Big.zero()}}},T.Global{"shapeIdentity"}}

def ignored_application() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    identity : G.Entry <- kernel(G.Entry,Check.check_decl(G.initial(),B.unlimited(),nat_identity()))
    +globals : G.T = G.add("shapeIdentity",identity,G.initial())
    +bad : T.T = application_body(nat_shape(T.Univ{L.var(0n)}))
    ok : Unit <- ignored_shape(globals,Check.Decl{"shapeApplication",Check.Definition{},Prim.nat_ty(),Some{application_body(nat_shape(Prim.nat_ty()))}},bad)
    scope(V.T,Check.infer_term(globals,B.unlimited(),bad))

def closed_family() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    globals : G.T <- kernel(G.T,Check.declare_family(G.initial(),B.unlimited(),Check.FamilyDecl{"ClosedFamily",[],[],L.one()}))
    require(Bool.not(absent(P.Family,G.find_family("ClosedFamily",globals))),"closed family disappeared")

def high_closed(value: Maybe<&2,Big.T>) -> R(Unit):
  match value:
    case None{}: Fail{"large universe failed to parse"}
    case Some{amount}: do Result<&2,&2,String,Unit>:
      +largest : L.T = LV.offset(amount,L.zero())
      inferred : V.T <- kernel(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Univ{largest}))
      universe(L.succ(largest),inferred)

def scheme_budget() -> R(Unit):
  expect(Unit,B.run_poll(Unit,Check.check_scheme_comp(G.initial(),3n,distributed("budgeted")),callback(Some{0})),"budget",Some{"the check budget is exhausted"})

def scheme_exact_failure(measured: C.Pair<U32,Result<&2,&2,E.T,Unit>>, complete: U32) -> R(Unit):
  match measured:
    case C.Pair{count,result}: do Result<&2,&2,String,Unit>:
      ok : Unit <- expect(Unit,result,"budget",Some{"the check budget is exhausted"})
      require(U32.is_eq(count,complete),"last-poll refusal did not stop at the measured threshold")

def scheme_exact_success(measured: C.Pair<U32,Unit>, complete: U32) -> R(Unit):
  match measured:
    case C.Pair{count,unit}: require(U32.is_eq(count,complete),"one-higher budget changed the measured poll count")

def scheme_late_counts(baseline: C.Pair<U32,Unit>, full: C.Pair<U32,Unit>, +declaration: Check.Decl) -> R(Unit):
  match baseline full:
    case C.Pair{+base,unit} C.Pair{+complete,unit}: do Result<&2,&2,String,Unit>:
      ok : Unit <- require(U32.is_gt(complete,U32.add(base,64)),"complex comparison did not add polls beyond the same declaration shape")
      +half : U32 = U32.shr(complete)
      +limit : U32 = Bool.pick(U32,U32.is_gt(U32.add(base,1),half),U32.add(base,1),half)
      late : C.Pair<U32,Result<&2,&2,E.T,Unit>> <- measure(Unit,Check.check_scheme_comp(G.initial(),12n,declaration),Some{limit})
      ok : Unit <- late_result(Unit,late,limit,complete)
      last : C.Pair<U32,Result<&2,&2,E.T,Unit>> <- measure(Unit,Check.check_scheme_comp(G.initial(),12n,declaration),Some{U32.sub(complete,1)})
      ok : Unit <- scheme_exact_failure(last,complete)
      exact : C.Pair<U32,Result<&2,&2,E.T,Unit>> <- measure(Unit,Check.check_scheme_comp(G.initial(),12n,declaration),Some{complete})
      done : C.Pair<U32,Unit> <- completed(Unit,exact)
      scheme_exact_success(done,complete)

def scheme_late_relation(pair: C.Pair<L.T,L.T>) -> R(Unit):
  match pair:
    case C.Pair{left,right}: do Result<&2,&2,String,Unit>:
      +declaration : Check.Decl = body(definition("largeComparison",right),T.Univ{left})
      baseline : C.Pair<U32,Result<&2,&2,E.T,Unit>> <- measure(Unit,Check.check_scheme_comp(G.initial(),12n,definition("plainComparison",L.zero())),None{})
      full : C.Pair<U32,Result<&2,&2,E.T,Unit>> <- measure(Unit,Check.check_scheme_comp(G.initial(),12n,declaration),None{})
      baseline : C.Pair<U32,Unit> <- completed(Unit,baseline)
      full : C.Pair<U32,Unit> <- completed(Unit,full)
      scheme_late_counts(baseline,full,declaration)

def catalog(name: String) -> R(Poly.T):
  kernel(Poly.T,Poly.declare(G.initial(),Poly.empty(),B.unlimited(),1n,definition(name,L.var(0n))))

def distributed_catalog() -> R(Poly.T):
  kernel(Poly.T,Poly.declare(G.initial(),Poly.empty(),B.unlimited(),3n,distributed("distribution")))

def inst_globals(instance: C.Pair<G.T,T.T>) -> G.T:
  match instance:
    case C.Pair{globals,reference}: globals

def inst_ref(instance: C.Pair<G.T,T.T>) -> T.T:
  match instance:
    case C.Pair{globals,reference}: reference

def template_symbolic() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    templates : Poly.T <- distributed_catalog()
    ok : Unit <- require(absent(G.Entry,G.find("distribution",G.initial())),"bare template entered globals")
    expect(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Global{"distribution"}),"unbound",Some{"distribution"})

def template_branches() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    +templates : Poly.T <- distributed_catalog()
    +zero : C.Pair<G.T,T.T> <- kernel(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"distribution",[L.of_u32(2),L.zero(),L.zero()],"atZero"))
    ok : Unit <- stored(inst_globals(zero),"atZero",L.zero(),inst_ref(zero))
    +three : C.Pair<G.T,T.T> <- kernel(C.Pair<G.T,T.T>,Poly.instantiate(inst_globals(zero),templates,B.unlimited(),"distribution",[L.of_u32(2),L.zero(),L.of_u32(3)],"atThree"))
    ok : Unit <- stored(inst_globals(three),"atThree",L.of_u32(3),inst_ref(three))
    ok : Unit <- stored(inst_globals(three),"atZero",L.zero(),inst_ref(zero))
    require(absent(G.Entry,G.find("distribution",inst_globals(three))),"template entered instantiated globals")

def template_duplicate() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    templates : Poly.T <- catalog("same")
    collision(Poly.T,Poly.declare(G.initial(),templates,B.unlimited(),1n,definition("same",L.var(0n))),"same")

def instance_arity() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    +templates : Poly.T <- distributed_catalog()
    ok : Unit <- expect(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"distribution",[L.zero(),L.one()],"tooFew"),"universe",Some{"the schema distribution expects 3 universe arguments, got 2"})
    expect(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"distribution",[L.zero(),L.one(),L.one(),L.one()],"tooMany"),"universe",Some{"the schema distribution expects 3 universe arguments, got 4"})

def instance_open() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    templates : Poly.T <- catalog("poly")
    expect(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"poly",[L.var(0n)],"open"),"universe",Some{"universe arguments must be closed"})

def instance_collision(+name: String) -> R(Unit):
  do Result<&2,&2,String,Unit>:
    templates : Poly.T <- catalog("poly")
    collision(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"poly",[L.zero()],name),name)

def failed_instances() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    +templates : Poly.T <- catalog("poly")
    +original_count : Nat = primitive_count(G.initial())
    ok : Unit <- expect(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"poly",[],"retry"),"universe",Some{"the schema poly expects 1 universe arguments, got 0"})
    ok : Unit <- require(Bool.and(Nat.is_eq(original_count,primitive_count(G.initial())),absent(G.Entry,G.find("retry",G.initial()))),"failed instance changed original globals")
    +instance : C.Pair<G.T,T.T> <- kernel(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),templates,B.unlimited(),"poly",[L.one()],"retry"))
    ok : Unit <- require(Nat.is_eq(primitive_count(inst_globals(instance)),Nat.add(original_count,1n)),"failed instance leaked an entry into the next environment")
    ok : Unit <- stored(inst_globals(instance),"retry",L.one(),inst_ref(instance))
    ok : Unit <- collision(C.Pair<G.T,T.T>,Poly.instantiate(inst_globals(instance),templates,B.unlimited(),"poly",[L.zero()],"retry"),"retry")
    stored(inst_globals(instance),"retry",L.one(),inst_ref(instance))

def instance_budget() -> R(Unit):
  do Result<&2,&2,String,Unit>:
    templates : Poly.T <- catalog("poly")
    expect(C.Pair<G.T,T.T>,B.run_poll(C.Pair<G.T,T.T>,Poly.instantiate_comp(G.initial(),templates,"poly",[L.one()],"budgetInstance"),callback(Some{0})),"budget",Some{"the check budget is exhausted"})

def named_algebra(names: List<&2,String>, cases: List<&2,C.Pair<String,Bool>>) -> List<&2,C.Pair<String,R(Unit)>>:
  match names cases:
    case Nil{} Nil{}: []
    case Con{+name,rest} Con{C.Pair{old,ok},tail}: C.Pair{name,require(ok,name)} <> named_algebra(rest,tail)
    case _ _: [C.Pair{"algebra-name-map",Fail{"algebra case inventory changed"}}]

def algebra_cases() -> List<&2,C.Pair<String,R(Unit)>>:
  List.append(&2,C.Pair<String,R(Unit)>,named_algebra(["imax-zero", "imax-zero-left", "imax-self", "imax-distributes-over-max", "successor-distributes-over-max", "successor-dominates-variable-and-one", "max-flattens-and-drops-duplicates", "imax-positive-right-is-max", "imax-symbolic-right-is-not-max", "symbolic-zero-is-not-positive", "imax-preserves-conditional-positivity", "le-successor-domination", "le-imax-case-split", "no-finite-sampling-of-unbounded-variables", "closed-levels-beyond-host-int", "scope-and-constructor-boundaries", "substitution-visits-imax-and-max"],Algebra.cases()),[C.Pair{"equal-budget-expires-inside-symbolic-comparison",comparison_budget(True{})},C.Pair{"le-budget-expires-inside-symbolic-comparison",comparison_budget(False{})}])

def checker_cases() -> List<&2,C.Pair<String,R(Unit)>>:
  [C.Pair{"scheme-checks-imax-distribution-symbolically",kernel(Unit,Check.check_scheme(G.initial(),B.unlimited(),3n,distributed("distribution")))},
   C.Pair{"point-former-preserves-prop-impredicativity",impredicative()},
   C.Pair{"symbolic-point-former-preserves-prop-impredicativity",symbolic_prop()},
   C.Pair{"scheme-refuses-finite-sampling-counterfeit",expect(Unit,Check.check_scheme(G.initial(),B.unlimited(),1n,body(definition("counterfeit",L.of_u32(8)),T.Univ{L.max(L.of_u32(8),L.var(0n))})),"mismatch",None{})},
   C.Pair{"scheme-refuses-imax-with-zero-branch-deleted",expect(Unit,Check.check_scheme(G.initial(),B.unlimited(),2n,body(definition("badImMax",L.max(L.var(0n),L.var(1n))),T.Univ{L.imax(L.var(0n),L.var(1n))})),"mismatch",None{})},
   C.Pair{"ordinary-inference-refuses-free-level",scope(V.T,Check.infer_term(G.initial(),B.unlimited(),T.Univ{L.var(0n)}))},
   C.Pair{"ordinary-inference-refuses-erased-free-level",hidden_scope()},
   C.Pair{"raw-inference-refuses-free-level",scope(Engine.Inferred,Check.infer_node(Check.make(G.initial(),B.unlimited(),0n),Q.QMany{},T.Univ{L.var(0n)}))},
   C.Pair{"ordinary-check-refuses-free-level",scope(Unit,Check.check_term(G.initial(),B.unlimited(),T.Univ{L.var(0n)},V.VUniv{L.succ(L.var(0n))}))},
   C.Pair{"ordinary-declaration-refuses-template",scope(G.Entry,Check.check_decl(G.initial(),B.unlimited(),definition("escaped",L.var(0n))))},
   C.Pair{"scheme-refuses-out-of-range-type-level",scope(Unit,Check.check_scheme(G.initial(),B.unlimited(),1n,definition("outside",L.var(1n))))},
   C.Pair{"scheme-refuses-hidden-annotation-level",scope(Unit,Check.check_scheme(G.initial(),B.unlimited(),1n,body(definition("annotation",L.zero()),T.Ann{T.Univ{L.zero()},T.Univ{L.var(1n)}})))},
   C.Pair{"scheme-checks-bound-shape-domain",kernel(Unit,Check.check_scheme(G.initial(),B.unlimited(),1n,body(definition("shape",L.var(0n)),T.Ran{S.SPi{Q.QZero{},"A",T.Univ{L.var(0n)}},T.Univ{L.zero()}})))},
   C.Pair{"scheme-refuses-hidden-shape-domain-level",scope(Unit,Check.check_scheme(G.initial(),B.unlimited(),1n,body(definition("shapeOutside",L.var(0n)),T.Ran{S.SPi{Q.QZero{},"A",T.Univ{L.var(1n)}},T.Univ{L.zero()}})))},
   C.Pair{"ignored-section-shape-refuses-free-level",ignored_section()},
   C.Pair{"ignored-injection-shape-refuses-free-level",ignored_injection()},
   C.Pair{"ignored-application-shape-refuses-free-level",ignored_application()},
   C.Pair{"closed-family-remains-supported",closed_family()},
   C.Pair{"family-refuses-open-universe",expect(G.T,Check.declare_family(G.initial(),B.unlimited(),Check.FamilyDecl{"OpenFamily",[],[],L.var(0n)}),"universe",Some{"family universe levels must be closed at M0 Stage A"})},
   C.Pair{"family-refuses-open-parameter-type",scope(G.T,Check.declare_family(G.initial(),B.unlimited(),Check.FamilyDecl{"OpenParameter",[P.Binder{Q.QZero{},"A",T.Univ{L.var(0n)}}],[],L.one()}))},
   C.Pair{"closed-high-universe-checks-without-overflow",high_closed(Big.of_decimal("4611686018427387903"))},
   C.Pair{"scheme-honors-check-budget",scheme_budget()},
   C.Pair{"scheme-budget-expires-during-level-comparison",scheme_late_relation(relation())}]

def poly_cases() -> List<&2,C.Pair<String,R(Unit)>>:
  [C.Pair{"template-does-not-enter-monomorphic-globals",template_symbolic()},
   C.Pair{"closed-specializations-take-both-imax-branches",template_branches()},
   C.Pair{"template-refuses-invalid-universal-body",expect(Poly.T,Poly.declare(G.initial(),Poly.empty(),B.unlimited(),1n,body(definition("counterfeit",L.of_u32(8)),T.Univ{L.max(L.of_u32(8),L.var(0n))})),"mismatch",None{})},
   C.Pair{"template-refuses-duplicate-name",template_duplicate()},
   C.Pair{"template-refuses-existing-global-name",collision(Poly.T,Poly.declare(G.initial(),Poly.empty(),B.unlimited(),0n,definition("Nat",L.zero())),"Nat")},
   C.Pair{"instance-refuses-wrong-arity",instance_arity()},
   C.Pair{"instance-refuses-open-argument",instance_open()},
   C.Pair{"instance-refuses-template-name",instance_collision("poly")},
   C.Pair{"instance-refuses-existing-global-name",instance_collision("Nat")},
   C.Pair{"instance-refuses-unknown-template",expect(C.Pair<G.T,T.T>,Poly.instantiate(G.initial(),Poly.empty(),B.unlimited(),"missing",[],"unknown"),"unbound",Some{"unknown universe schema missing"})},
   C.Pair{"failed-instances-preserve-environments",failed_instances()},
   C.Pair{"instance-rechecks-with-caller-budget",instance_budget()}]

def add_failure(result: R(Unit), name: String, failures: List<&2,String>) -> List<&2,String>:
  match result:
    case Done{unit}: failures
    case Fail{error}: (name ++ ": " ++ error) <> failures

def failures(cases: List<&2,C.Pair<String,R(Unit)>>, found: List<&2,String>) -> List<&2,String>:
  match cases:
    case Nil{}: List.reverse(&2,String,found)
    case Con{C.Pair{name,result},rest}: failures(rest,add_failure(result,name,found))

def finish(errors: List<&2,String>) -> IO(Unit):
  match errors:
    case Nil{}: IO.print("LEVELS-RUNTIME-OK algebra=19 checker=23 poly=12 boundary_cases=1")
    case Con{head,tail}: IO.die(Unit,1,String.join(head <> tail,"\n"))

def run() -> IO(Unit):
  finish(failures(List.concat(&2,C.Pair<String,R(Unit)>,[algebra_cases(),checker_cases(),poly_cases()]),[]))

def arguments(args: List<String>) -> IO(Unit):
  run()

def main() -> IO(Unit):
  run()
