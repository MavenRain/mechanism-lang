import Base
import ./bignum.bend as Bignum
import ./common.bend as Common
import ./term.bend as Term
import ./value.bend as Value
import ./shape.bend as Shape
import ./quantity.bend as Quantity
import ./global.bend as Global
import ./prim.bend as Prim
import ./literal.bend as Literal
import ./level.bend as Level
import ./error.bend as Error

law eval:
  for +globals: Global.T
  for +env: List<&2, Value.T>
  for term: Term.T
  Result<&2, &2, Error.T, Value.T>

law out_value:
  for +globals: Global.T
  for shape: Shape.T<Value.T>
  for addr: Value.VAddr<Value.T>
  for value: Value.T
  Result<&2, &2, Error.T, Value.T>

law elim_value:
  for +globals: Global.T
  for data: Value.StuckElim<Value.T>
  for value: Value.T
  Result<&2, &2, Error.T, Value.T>

law whnf:
  for +globals: Global.T
  for value: Value.T
  Result<&2, &2, Error.T, Value.T>

law replay:
  for +globals: Global.T
  for value: Value.T
  for frames: List<&2, Value.Spine<Value.T>>
  Result<&2, &2, Error.T, Value.T>

def local_value(value: Maybe<&2, Value.T>, index: Nat) -> Result<&2, &2, Error.T, Value.T>:
  match value:
    case Some{x}:
      Done{x}
    case None{}:
      Fail{Error.Unbound{"de Bruijn index " ++ Nat.show(index) ++ " is outside the environment"}}

@unsafe def eval_list(terms: List<&2, Term.T>, +globals: Global.T, +env: List<&2, Value.T>) -> Result<&2, &2, Error.T, List<&2, Value.T>>:
  match terms:
    case Nil{}:
      Done{[]}
    case Con{term, rest}:
      do Result<&2, &2, Error.T, List<&2, Value.T>>:
        value : Value.T <- eval(globals, env, term)
        values : List<&2, Value.T> <- eval_list(rest, globals, env)
        return value <> values

@unsafe def eval_shape(shape: Shape.T<Term.T>, +globals: Global.T, +env: List<&2, Value.T>) -> Result<&2, &2, Error.T, Shape.T<Value.T>>:
  match shape:
    case Shape.SPi{q, name, dom}:
      do Result<&2, &2, Error.T, Shape.T<Value.T>>:
        value : Value.T <- eval(globals, env, dom)
        return Shape.SPi{q, name, value}
    case Shape.SColl{count}:
      Done{Shape.SColl{count}}
    case Shape.SPar{a, b}:
      Fail{Error.Not_yet{"SPar arrives at M1"}}
    case Shape.SMu{name, args}:
      do Result<&2, &2, Error.T, Shape.T<Value.T>>:
        values : List<&2, Value.T> <- eval_list(args, globals, env)
        return Shape.SMu{name, values}
    case Shape.SNu{name, args}:
      Fail{Error.Not_yet{"SNu arrives at M2"}}
    case Shape.SZk{q, name, dom}:
      do Result<&2, &2, Error.T, Shape.T<Value.T>>:
        value : Value.T <- eval(globals, env, dom)
        return Shape.SZk{q, name, value}
    case Shape.SFhc{level}:
      do Result<&2, &2, Error.T, Shape.T<Value.T>>:
        value : Value.T <- eval(globals, env, level)
        return Shape.SFhc{value}
    case Shape.SMpc{parties, access}:
      do Result<&2, &2, Error.T, Shape.T<Value.T>>:
        p : Value.T <- eval(globals, env, parties)
        a : Value.T <- eval(globals, env, access)
        return Shape.SMpc{p, a}

@unsafe def eval_addr(globals: Global.T, env: List<&2, Value.T>, addr: Term.Addr<Term.T>) -> Result<&2, &2, Error.T, Value.VAddr<Value.T>>:
  match addr:
    case Term.APt{q, arg}:
      do Result<&2, &2, Error.T, Value.VAddr<Value.T>>:
        value : Value.T <- eval(globals, env, arg)
        return Value.VAPt{q, value}
    case Term.ALeg{index}:
      Done{Value.VALeg{index}}
    case Term.ACtor{name}:
      Done{Value.VACtor{name}}

def close_legs(legs: List<&2, Term.Leg<Term.T>>, +env: List<&2, Value.T>) -> List<&2, Value.VLeg<Value.T>>:
  match legs:
    case Nil{}:
      []
    case Con{Term.Leg{binders, body}, rest}:
      Value.VLeg{binders, Value.close(env, body)} <> close_legs(rest, env)

@unsafe def open_closure(globals: Global.T, closure: Value.Closure<Value.T>, args: List<&2, Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match closure:
    case Value.Closure{env, body}:
      eval(globals, List.append(&2, Value.T, List.reverse(&2, Value.T, args), env), body)

@unsafe def eval_global_entry(entry: Maybe<&2, Global.Entry>, globals: Global.T, name: String) -> Result<&2, &2, Error.T, Value.T>:
  match entry:
    case None{}:
      Fail{Error.Unbound{name}}
    case Some{Global.Def{Global.DefEntry{ty, body, True{}, None{}, partial}}}:
      eval(globals, [], body)
    case Some{entry}:
      Done{Value.VNeutral{Value.HGlobal{name}, []}}

def annotation_level(result: Result<&2, &2, Error.T, Value.T>) -> Maybe<&2, Level.T>:
  match result:
    case Done{value}:
      Value.as_univ(value)
    case Fail{error}:
      None{}

@unsafe def fill_level(level: Maybe<&2, Level.T>, globals: Global.T, env: List<&2, Value.T>, ty: Term.T) -> Maybe<&2, Level.T>:
  match level:
    case Some{x}:
      Some{x}
    case None{}:
      annotation_level(eval(globals, env, ty))

@unsafe def annotate(value: Value.T, globals: Global.T, env: List<&2, Value.T>, ty: Term.T) -> Value.T:
  match value:
    case Value.VLan{s, d, u}:
      Value.VLan{s, d, fill_level(u, globals, env, ty)}
    case Value.VRan{s, d, u}:
      Value.VRan{s, d, fill_level(u, globals, env, ty)}
    case other:
      other

@unsafe def eval(globals, env, term):
  match term:
    case Term.Var{+index}:
      local_value(List.get(&2, Value.T, env, index), index)
    case Term.Univ{level}:
      Done{Value.VUniv{level}}
    case Term.Lit{literal}:
      Done{Value.VLit{literal}}
    case Term.Lan{s, body}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        return Value.VLan{shape, Value.close(env, body), None{}}
    case Term.Ran{s, body}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        return Value.VRan{shape, Value.close(env, body), None{}}
    case Term.In{s, a, args}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        addr : Value.VAddr<Value.T> <- eval_addr(globals, env, a)
        values : List<&2, Value.T> <- eval_list(args, globals, env)
        return Value.VIn{shape, addr, values}
    case Term.Sec{s, legs}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        return Value.VSec{shape, close_legs(legs, env)}
    case Term.Out{s, a, head}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        addr : Value.VAddr<Value.T> <- eval_addr(globals, env, a)
        value : Value.T <- eval(globals, env, head)
        applied : Value.T <- out_value(globals, shape, addr, value)
        whnf(globals, applied)
    case Term.Elim{Term.ElimData{s, scrut, q, motive, branches}}:
      do Result<&2, &2, Error.T, Value.T>:
        shape : Shape.T<Value.T> <- eval_shape(s, globals, env)
        value : Value.T <- eval(globals, env, scrut)
        elim_value(globals, Value.StuckElim{shape, q, motive, branches, env}, value)
    case Term.Let{name, ty, value, body}:
      do Result<&2, &2, Error.T, Value.T>:
        v : Value.T <- eval(globals, env, value)
        eval(globals, v <> env, body)
    case Term.Ann{body, ty}:
      do Result<&2, &2, Error.T, Value.T>:
        v : Value.T <- eval(globals, env, body)
        return annotate(v, globals, env, ty)
    case Term.Global{+name}:
      eval_global_entry(Global.find(name, globals), globals, name)
    case Term.Auto{}:
      Fail{Error.Not_yet{"instances arrive at M2"}}

def stuck_out(value: Value.T, shape: Shape.T<Value.T>, addr: Value.VAddr<Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match value:
    case Value.VNeutral{head, spine}:
      Done{Value.VNeutral{head, Value.SOut{shape, addr} <> spine}}
    case other:
      Fail{Error.Mismatch{"an elimination met a value it cannot eliminate"}}

@unsafe def selected_leg(leg: Maybe<&2, Value.VLeg<Value.T>>, globals: Global.T) -> Result<&2, &2, Error.T, Value.T>:
  match leg:
    case None{}:
      Fail{Error.Mismatch{"an elimination met a value it cannot eliminate"}}
    case Some{Value.VLeg{binders, clo}}:
      open_closure(globals, clo, [])

@unsafe def out_point(value: Value.T, globals: Global.T, shape: Shape.T<Value.T>, addr: Value.VAddr<Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match value addr:
    case Value.VSec{s, Con{Value.VLeg{binders, clo}, Nil{}}} Value.VAPt{q, arg}:
      open_closure(globals, clo, [arg])
    case value addr:
      stuck_out(value, shape, addr)

@unsafe def out_coll(value: Value.T, globals: Global.T, shape: Shape.T<Value.T>, addr: Value.VAddr<Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match value addr:
    case Value.VSec{s, legs} Value.VALeg{index}:
      selected_leg(Bignum.at(Value.VLeg<Value.T>, legs, index), globals)
    case value addr:
      stuck_out(value, shape, addr)

@unsafe def out_value(globals, shape, addr, value):
  match shape:
    case Shape.SPi{q, name, dom}:
      out_point(value, globals, Shape.SPi{q, name, dom}, addr)
    case Shape.SColl{count}:
      out_coll(value, globals, Shape.SColl{count}, addr)
    case Shape.SPar{a, b}:
      Fail{Error.Not_yet{"SPar arrives at M1"}}
    case Shape.SMu{name, args}:
      Fail{Error.Not_yet{"a right former at a mu shape arrives at M2"}}
    case Shape.SNu{name, args}:
      Fail{Error.Not_yet{"SNu arrives at M2"}}
    case Shape.SZk{q, name, dom}:
      out_point(value, globals, Shape.SZk{q, name, dom}, addr)
    case Shape.SFhc{level}:
      stuck_out(value, Shape.SFhc{level}, addr)
    case Shape.SMpc{parties, access}:
      stuck_out(value, Shape.SMpc{parties, access}, addr)

def branch_match(addr: Term.Addr<Term.T>, target: Value.VAddr<Value.T>) -> Bool:
  match addr target:
    case Term.ALeg{x} Value.VALeg{y}:
      Bignum.equal(x, y)
    case Term.ACtor{x} Value.VACtor{y}:
      String.eq(x, y)
    case _ _:
      False{}

def find_branch(branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +addr: Value.VAddr<Value.T>) -> Maybe<&2, Term.Leg<Term.T>>:
  match branches:
    case Nil{}:
      None{}
    case Con{Common.Pair{a, leg}, rest}:
      Bool.pick(Maybe<&2, Term.Leg<Term.T>>, branch_match(a, addr), Some{leg}, find_branch(rest, addr))

def stuck_elim(value: Value.T, data: Value.StuckElim<Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match value:
    case Value.VNeutral{head, spine}:
      Done{Value.VNeutral{head, Value.SElim{data} <> spine}}
    case other:
      Fail{Error.Mismatch{"an elimination met a value it cannot eliminate"}}

@unsafe def selected_branch(branch: Maybe<&2, Term.Leg<Term.T>>, globals: Global.T, env: List<&2, Value.T>, args: List<&2, Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match branch:
    case None{}:
      Fail{Error.Mismatch{"an elimination met a value it cannot eliminate"}}
    case Some{Term.Leg{binders, body}}:
      eval(globals, List.append(&2, Value.T, List.reverse(&2, Value.T, args), env), body)

@unsafe def elim_zk(zk: Bool, globals: Global.T, env: List<&2, Value.T>, fibre: Value.T, arg: Value.T, body: Term.T) -> Result<&2, &2, Error.T, Value.T>:
  match zk:
    case True{}:
      eval(globals, fibre <> arg <> env, body)
    case False{}:
      Fail{Error.Mismatch{"an elimination met a value it cannot eliminate"}}

@unsafe def elim_point(value: Value.T, branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, globals: Global.T, env: List<&2, Value.T>, data: Value.StuckElim<Value.T>, zk: Bool) -> Result<&2, &2, Error.T, Value.T>:
  match value branches:
    case Value.VIn{s, Value.VAPt{q, arg}, Con{fibre, Nil{}}} Con{Common.Pair{addr, Term.Leg{binders, body}}, Nil{}}:
      eval(globals, fibre <> arg <> env, body)
    case Value.VIn{s, Value.VAPt{q, arg}, Con{fibre, Con{instance, Nil{}}}} Con{Common.Pair{addr, Term.Leg{binders, body}}, Nil{}}:
      elim_zk(zk, globals, env, fibre, arg, body)
    case value branches:
      stuck_elim(value, data)

@unsafe def elim_named(value: Value.T, branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, globals: Global.T, env: List<&2, Value.T>, data: Value.StuckElim<Value.T>) -> Result<&2, &2, Error.T, Value.T>:
  match value:
    case Value.VIn{s, addr, args}:
      selected_branch(find_branch(branches, addr), globals, env, args)
    case value:
      stuck_elim(value, data)

@unsafe def elim_value(globals, data, value):
  match data:
    case Value.StuckElim{Shape.SPar{a, b}, q, motive, branches, env}:
      Fail{Error.Not_yet{"SPar arrives at M1"}}
    case Value.StuckElim{Shape.SNu{name, args}, q, motive, branches, env}:
      Fail{Error.Not_yet{"SNu arrives at M2"}}
    case Value.StuckElim{Shape.SPi{sq, name, dom}, q, motive, +branches, +env}:
      elim_point(value, branches, globals, env, Value.StuckElim{Shape.SPi{sq, name, dom}, q, motive, branches, env}, False{})
    case Value.StuckElim{Shape.SZk{sq, name, dom}, q, motive, +branches, +env}:
      elim_point(value, branches, globals, env, Value.StuckElim{Shape.SZk{sq, name, dom}, q, motive, branches, env}, True{})
    case Value.StuckElim{Shape.SColl{count}, q, motive, +branches, +env}:
      elim_named(value, branches, globals, env, Value.StuckElim{Shape.SColl{count}, q, motive, branches, env})
    case Value.StuckElim{Shape.SMu{name, args}, q, motive, +branches, +env}:
      elim_named(value, branches, globals, env, Value.StuckElim{Shape.SMu{name, args}, q, motive, branches, env})
    case Value.StuckElim{shape, q, motive, branches, env}:
      stuck_elim(value, Value.StuckElim{shape, q, motive, branches, env})

def spine_literals(spine: List<&2, Value.Spine<Value.T>>, acc: List<&2, Literal.T>) -> Maybe<&2, List<&2, Literal.T>>:
  match spine:
    case Nil{}:
      Some{acc}
    case Con{Value.SOut{s, Value.VAPt{q, Value.VLit{lit}}}, rest}:
      spine_literals(rest, lit <> acc)
    case _:
      None{}

def canonical_ctor(value: Value.T) -> Bool:
  match value:
    case Value.VIn{s, Value.VACtor{name}, args}:
      True{}
    case _:
      False{}

def guarded_frame(frames: List<&2, Value.Spine<Value.T>>, index: Nat) -> Bool:
  match frames index:
    case Con{Value.SOut{s, Value.VAPt{q, value}}, rest} 0n:
      canonical_ctor(value)
    case Con{Value.SOut{s, Value.VAPt{q, value}}, rest} 1n+p:
      guarded_frame(rest, p)
    case _ _:
      False{}

@unsafe def unfold_guarded(ready: Bool, +globals: Global.T, body: Term.T, frames: List<&2, Value.Spine<Value.T>>, value: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match ready:
    case False{}:
      Done{value}
    case True{}:
      do Result<&2, &2, Error.T, Value.T>:
        head : Value.T <- eval(globals, [], body)
        replay(globals, head, frames)

@unsafe def unfold_entry(entry: Maybe<&2, Global.DefEntry>, +globals: Global.T, +frames: List<&2, Value.Spine<Value.T>>, value: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match entry:
    case Some{Global.DefEntry{ty, body, True{}, Some{index}, partial}}:
      unfold_guarded(guarded_frame(frames, index), globals, body, frames, value)
    case _:
      Done{value}

@unsafe def whnf_neutral(value: Value.T, +globals: Global.T) -> Result<&2, &2, Error.T, Value.T>:
  match value:
    case Value.VNeutral{Value.HGlobal{+name}, +spine}:
      unfold_entry(Global.find_def(name, globals), globals, List.reverse(&2, Value.Spine<Value.T>, spine), Value.VNeutral{Value.HGlobal{name}, spine})
    case value:
      Done{value}

@unsafe def prim_after(result: Maybe<&2, Value.T>, +globals: Global.T, fallback: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match result:
    case None{}:
      Done{fallback}
    case Some{value}:
      whnf_neutral(value, globals)

@unsafe def prim_args(args: Maybe<&2, List<&2, Literal.T>>, p: Prim.T, +globals: Global.T, fallback: Value.T) -> Result<&2, &2, Error.T, Value.T>:
  match args:
    case None{}:
      Done{fallback}
    case Some{xs}:
      prim_after(Prim.apply(p, xs), globals, fallback)

@unsafe def whnf_global(entry: Maybe<&2, Global.Entry>, +globals: Global.T, name: String, +spine: List<&2, Value.Spine<Value.T>>) -> Result<&2, &2, Error.T, Value.T>:
  match entry:
    case Some{Global.Prim{ty, p}}:
      prim_args(spine_literals(spine, []), p, globals, Value.VNeutral{Value.HGlobal{name}, spine})
    case Some{Global.Def{Global.DefEntry{ty, body, True{}, Some{index}, partial}}}:
      +frames = List.reverse(&2, Value.Spine<Value.T>, spine)
      unfold_guarded(guarded_frame(frames, index), globals, body, frames, Value.VNeutral{Value.HGlobal{name}, spine})
    case _:
      Done{Value.VNeutral{Value.HGlobal{name}, spine}}

@unsafe def whnf(globals, value):
  match value:
    case Value.VNeutral{Value.HGlobal{+name}, spine}:
      whnf_global(Global.find(name, globals), globals, name, spine)
    case value:
      Done{value}

@unsafe def replay(globals, value, frames):
  match frames:
    case Nil{}:
      Done{value}
    case Con{Value.SOut{shape, addr}, rest}:
      do Result<&2, &2, Error.T, Value.T>:
        applied : Value.T <- out_value(globals, shape, addr, value)
        reduced : Value.T <- whnf(globals, applied)
        replay(globals, reduced, rest)
    case Con{Value.SElim{data}, rest}:
      do Result<&2, &2, Error.T, Value.T>:
        reduced : Value.T <- elim_value(globals, data, value)
        replay(globals, reduced, rest)

law quote:
  for +globals: Global.T
  for +size: Nat
  for value: Value.T
  Result<&2, &2, Error.T, Term.T>

@unsafe def quote_list(values: List<&2, Value.T>, +globals: Global.T, +size: Nat) -> Result<&2, &2, Error.T, List<&2, Term.T>>:
  match values:
    case Nil{}:
      Done{[]}
    case Con{value, rest}:
      do Result<&2, &2, Error.T, List<&2, Term.T>>:
        term : Term.T <- quote(globals, size, value)
        terms : List<&2, Term.T> <- quote_list(rest, globals, size)
        return term <> terms

@unsafe def quote_shape(shape: Shape.T<Value.T>, +globals: Global.T, +size: Nat) -> Result<&2, &2, Error.T, Shape.T<Term.T>>:
  match shape:
    case Shape.SPi{q, name, dom}:
      do Result<&2, &2, Error.T, Shape.T<Term.T>>:
        value : Term.T <- quote(globals, size, dom)
        return Shape.SPi{q, name, value}
    case Shape.SColl{count}:
      Done{Shape.SColl{count}}
    case Shape.SPar{a, b}:
      Fail{Error.Not_yet{"SPar arrives at M1"}}
    case Shape.SMu{name, args}:
      do Result<&2, &2, Error.T, Shape.T<Term.T>>:
        values : List<&2, Term.T> <- quote_list(args, globals, size)
        return Shape.SMu{name, values}
    case Shape.SNu{name, args}:
      Fail{Error.Not_yet{"SNu arrives at M2"}}
    case Shape.SZk{q, name, dom}:
      do Result<&2, &2, Error.T, Shape.T<Term.T>>:
        value : Term.T <- quote(globals, size, dom)
        return Shape.SZk{q, name, value}
    case Shape.SFhc{level}:
      do Result<&2, &2, Error.T, Shape.T<Term.T>>:
        value : Term.T <- quote(globals, size, level)
        return Shape.SFhc{value}
    case Shape.SMpc{parties, access}:
      do Result<&2, &2, Error.T, Shape.T<Term.T>>:
        p : Term.T <- quote(globals, size, parties)
        a : Term.T <- quote(globals, size, access)
        return Shape.SMpc{p, a}

def fresh_vars(count: Nat, +size: Nat) -> List<&2, Value.T>:
  match count:
    case 0n:
      []
    case 1n+p:
      Value.var(size) <> fresh_vars(p, 1n+size)

@unsafe def quote_addr(globals: Global.T, size: Nat, addr: Value.VAddr<Value.T>) -> Result<&2, &2, Error.T, Term.Addr<Term.T>>:
  match addr:
    case Value.VAPt{q, value}:
      do Result<&2, &2, Error.T, Term.Addr<Term.T>>:
        term : Term.T <- quote(globals, size, value)
        return Term.APt{q, term}
    case Value.VALeg{index}:
      Done{Term.ALeg{index}}
    case Value.VACtor{name}:
      Done{Term.ACtor{name}}

@unsafe def quote_leg(+globals: Global.T, +size: Nat, leg: Value.VLeg<Value.T>) -> Result<&2, &2, Error.T, Term.Leg<Term.T>>:
  match leg:
    case Value.VLeg{+binders, clo}:
      +arity = List.length(&2, Common.Pair<Quantity.T, String>, binders)
      do Result<&2, &2, Error.T, Term.Leg<Term.T>>:
        value : Value.T <- open_closure(globals, clo, fresh_vars(arity, size))
        body : Term.T <- quote(globals, Nat.add(size, arity), value)
        return Term.Leg{binders, body}

@unsafe def quote_legs(legs: List<&2, Value.VLeg<Value.T>>, +globals: Global.T, +size: Nat) -> Result<&2, &2, Error.T, List<&2, Term.Leg<Term.T>>>:
  match legs:
    case Nil{}:
      Done{[]}
    case Con{leg, rest}:
      do Result<&2, &2, Error.T, List<&2, Term.Leg<Term.T>>>:
        item : Term.Leg<Term.T> <- quote_leg(globals, size, leg)
        items : List<&2, Term.Leg<Term.T>> <- quote_legs(rest, globals, size)
        return item <> items

def diagram_arity(shape: Shape.T<Value.T>) -> Nat:
  match shape:
    case Shape.SPi{q, name, dom}:
      1n
    case Shape.SZk{q, name, dom}:
      1n
    case _:
      0n

def quote_annotate(level: Maybe<&2, Level.T>, body: Term.T) -> Term.T:
  match level:
    case None{}:
      body
    case Some{x}:
      Term.Ann{body, Term.Univ{x}}

def build_former(lan: Bool, shape: Shape.T<Term.T>, body: Term.T) -> Term.T:
  match lan:
    case True{}:
      Term.Lan{shape, body}
    case False{}:
      Term.Ran{shape, body}

@unsafe def quote_former(+globals: Global.T, +size: Nat, +shape: Shape.T<Value.T>, clo: Value.Closure<Value.T>, level: Maybe<&2, Level.T>, lan: Bool) -> Result<&2, &2, Error.T, Term.T>:
  +arity = diagram_arity(shape)
  do Result<&2, &2, Error.T, Term.T>:
    s : Shape.T<Term.T> <- quote_shape(shape, globals, size)
    value : Value.T <- open_closure(globals, clo, fresh_vars(arity, size))
    body : Term.T <- quote(globals, Nat.add(size, arity), value)
    return quote_annotate(level, build_former(lan, s, body))

@unsafe def quote_motive(motive: Maybe<&2, Term.Motive<Term.T>>, +globals: Global.T, env: List<&2, Value.T>, +size: Nat) -> Result<&2, &2, Error.T, Maybe<&2, Term.Motive<Term.T>>>:
  match motive:
    case None{}:
      Done{None{}}
    case Some{Term.Motive{ind, +indices, self, body}}:
      +arity = Nat.add(1n, List.length(&2, String, indices))
      do Result<&2, &2, Error.T, Maybe<&2, Term.Motive<Term.T>>>:
        value : Value.T <- open_closure(globals, Value.close(env, body), fresh_vars(arity, size))
        term : Term.T <- quote(globals, Nat.add(size, arity), value)
        return Some{Term.Motive{ind, indices, self, term}}

@unsafe def quote_branches(branches: List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>, +globals: Global.T, +env: List<&2, Value.T>, +size: Nat) -> Result<&2, &2, Error.T, List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>>:
  match branches:
    case Nil{}:
      Done{[]}
    case Con{Common.Pair{addr, Term.Leg{binders, body}}, rest}:
      do Result<&2, &2, Error.T, List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>>>:
        va : Value.VAddr<Value.T> <- eval_addr(globals, env, addr)
        a : Term.Addr<Term.T> <- quote_addr(globals, size, va)
        leg : Term.Leg<Term.T> <- quote_leg(globals, size, Value.VLeg{binders, Value.close(env, body)})
        items : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- quote_branches(rest, globals, env, size)
        return Common.Pair{a, leg} <> items

@unsafe def quote_frames(frames: List<&2, Value.Spine<Value.T>>, +globals: Global.T, +size: Nat, head: Term.T) -> Result<&2, &2, Error.T, Term.T>:
  match frames:
    case Nil{}:
      Done{head}
    case Con{Value.SOut{shape, addr}, rest}:
      do Result<&2, &2, Error.T, Term.T>:
        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)
        a : Term.Addr<Term.T> <- quote_addr(globals, size, addr)
        quote_frames(rest, globals, size, Term.Out{s, a, head})
    case Con{Value.SElim{Value.StuckElim{shape, q, motive, branches, +env}}, rest}:
      do Result<&2, &2, Error.T, Term.T>:
        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)
        mo : Maybe<&2, Term.Motive<Term.T>> <- quote_motive(motive, globals, env, size)
        bs : List<&2, Common.Pair<Term.Addr<Term.T>, Term.Leg<Term.T>>> <- quote_branches(branches, globals, env, size)
        quote_frames(rest, globals, size, Term.Elim{Term.ElimData{s, head, q, mo, bs}})

def quote_head(head: Value.Head, size: Nat) -> Term.T:
  match head:
    case Value.HLocal{level}:
      Term.Var{Nat.sub(size, 1n+level)}
    case Value.HGlobal{name}:
      Term.Global{name}

@unsafe def quote(globals, size, value):
  match value:
    case Value.VUniv{level}:
      Done{Term.Univ{level}}
    case Value.VLit{lit}:
      Done{Term.Lit{lit}}
    case Value.VLan{shape, clo, level}:
      quote_former(globals, size, shape, clo, level, True{})
    case Value.VRan{shape, clo, level}:
      quote_former(globals, size, shape, clo, level, False{})
    case Value.VIn{shape, addr, args}:
      do Result<&2, &2, Error.T, Term.T>:
        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)
        a : Term.Addr<Term.T> <- quote_addr(globals, size, addr)
        xs : List<&2, Term.T> <- quote_list(args, globals, size)
        return Term.In{s, a, xs}
    case Value.VSec{shape, legs}:
      do Result<&2, &2, Error.T, Term.T>:
        s : Shape.T<Term.T> <- quote_shape(shape, globals, size)
        ls : List<&2, Term.Leg<Term.T>> <- quote_legs(legs, globals, size)
        return Term.Sec{s, ls}
    case Value.VNeutral{head, spine}:
      quote_frames(List.reverse(&2, Value.Spine<Value.T>, spine), globals, size, quote_head(head, size))
