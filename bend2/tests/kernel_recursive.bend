import Base
import ../kernel/bignum.bend as Width
import ../surface/elab.bend as Elab
import ../kernel/check.bend as Check
import ../kernel/common.bend as P
import ../kernel/global.bend as G
import ../kernel/term.bend as T
import ../kernel/shape.bend as S
import ../kernel/value.bend as V
import ../kernel/eval.bend as Eval
import ../kernel/level.bend as L
import ../kernel/budget.bend as B
import ../kernel/error.bend as E
import ../kernel/pp.bend as Pp
import ../kernel/totality.bend as Totality

def source() -> String:
  "\nmu N : Type 0 with | zero : N | succ : N -> N\nmu A : Type 0 with | leaf : A | node : B -> A\nand B : Type 0 with | cons : A -> B\nmu V : (0 i : N) -> Type 0 with\n| vz : V zero\n| vs : (0 i : N) -> V i -> V (succ i)\ndef rec length : (0 i : N) -> V i -> N := fun (0 i : N) (v : V i) => case v as x in V j return N with | vz => zero | vs 0 j w => succ (length j w)\ndef rec double : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ m => succ (succ (double m))\ndef rec sizeA : A -> Nat := fun (a : A) => case a as x in A return Nat with | leaf => 0 | node b => natAdd 1 (sizeB b)\nand sizeB : B -> Nat := fun (b : B) => case b as x in B return Nat with | cons a => natAdd 1 (sizeA a)\ndef rec f : N -> N := fun (n : N) => case n as x in N return N with | zero => zero | succ m => g m\nand g : N -> N := fun (n : N) => zero\nand spare : N := zero\ndef rec keep : Nat -> N -> Nat := fun (a : Nat) => fun (n : N) => case n as x in N return Nat with | zero => a | succ m => keep a m\ndef rec choose : N -> Nat -> Nat := fun (n : N) => case n as x in N return (Nat -> Nat) with | zero => fun (a : Nat) => a | succ m => choose m\ndef doubled : N := double (succ (succ zero))\ndef four : N := succ (succ (succ (succ zero)))\ndef z : N := zero\ndef mutualValue : Nat := sizeA (node (cons leaf))\ndef two : Nat := 2\ndef helper : N := f (succ zero)\ndef helperDirect : N := g zero\ndef kept : Nat := keep 7 (succ (succ zero))\ndef seven : Nat := 7\ndef chosen : Nat := choose (succ (succ zero)) 7\ndef indexed : N := length (succ zero) (vs zero vz)\ndef one : N := succ zero\ndef partial : N -> Nat := keep 7\ndef neutral : N -> Nat := fun (n : N) => keep 7 n\n"

def require(ok: Bool, message: String) -> Result<&2,&2,E.T,Unit>:
  Bool.pick(Result<&2,&2,E.T,Unit>,ok,Done{Unit{}},Fail{E.Mismatch{message}})

def optional(-A: Data, value: Maybe<&2,A>, message: String) -> Result<&2,&2,E.T,A>:
  match value:
    case Some{x}: Done{x}
    case None{}: Fail{E.Mismatch{message}}

def normal(+globals: G.T, name: String) -> Result<&2,&2,E.T,String>:
  do Result<&2,&2,E.T,String>:
    value : V.T <- Eval.eval(globals,[],T.Global{name})
    term : T.T <- Eval.quote(globals,0n,value)
    return Pp.term([],term)

def compare_pairs(pairs: List<&2,P.Pair<String,String>>, +globals: G.T) -> Result<&2,&2,E.T,Unit>:
  match pairs:
    case Nil{}: Done{Unit{}}
    case Con{P.Pair{+actual,expected},rest}:
      do Result<&2,&2,E.T,Unit>:
        got : String <- normal(globals,actual)
        want : String <- normal(globals,expected)
        ok : Unit <- require(String.eq(got,want),actual ++ " did not compute")
        compare_pairs(rest,globals)

def is_neutral(value: V.T) -> Bool:
  match value:
    case V.VNeutral{head,spine}: True{}
    case _: False{}

def neutrals(names: List<&2,String>, +globals: G.T) -> Result<&2,&2,E.T,Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{+name,rest}:
      do Result<&2,&2,E.T,Unit>:
        value : V.T <- Eval.eval(globals,[],T.Global{name})
        ok : Unit <- require(is_neutral(value),name ++ " unfolded before its guarded argument")
        neutrals(rest,globals)

def neutral_lambda(value: V.T, globals: G.T) -> Result<&2,&2,E.T,Unit>:
  match value:
    case V.VSec{s,[V.VLeg{binders,clo}]}:
      do Result<&2,&2,E.T,Unit>:
        body : V.T <- Eval.open_closure(globals,clo,[V.var(0n)])
        require(is_neutral(body),"recursive call unfolded on a neutral argument")
    case _: Fail{E.Mismatch{"neutral lambda"}}

def opaque_def(entry: G.DefEntry) -> G.Entry:
  match entry:
    case G.DefEntry{ty,body,r,rec,partial}: G.Def{G.DefEntry{ty,body,False{},rec,partial}}

def validate(checked: Elab.Checked) -> Result<&2,&2,E.T,Unit>:
  match checked:
    case P.Pair{+globals,rows}:
      do Result<&2,&2,E.T,Unit>:
        ok : Unit <- compare_pairs([P.Pair{"doubled","four"},P.Pair{"mutualValue","two"},P.Pair{"helper","z"},P.Pair{"helperDirect","z"},P.Pair{"spare","z"},P.Pair{"kept","seven"},P.Pair{"chosen","seven"},P.Pair{"indexed","one"}],globals)
        ok : Unit <- neutrals(["double","keep","partial"],globals)
        value : V.T <- Eval.eval(globals,[],T.Global{"neutral"})
        ok : Unit <- neutral_lambda(value,globals)
        entry : G.DefEntry <- optional(G.DefEntry,G.find_def("double",globals),"double definition")
        opaque : G.T = G.add("double",opaque_def(entry),globals)
        value : V.T <- Eval.eval(opaque,[],T.Global{"doubled"})
        require(is_neutral(value),"an explicitly opaque recursive definition unfolded")

def mu_negative(result: Result<&2,&2,E.T,V.T>) -> Bool:
  match result:
    case Fail{E.Not_yet{message}}: String.eq(message,"a right former at a mu shape arrives at M2")
    case _: False{}

def self_negative(result: Result<&2,&2,E.T,Maybe<&2,Nat>>) -> Bool:
  match result:
    case Fail{error}: String.eq(E.message(error),"recursive definition self failed the structural termination guard")
    case _: False{}

def suite() -> Result<&2,&2,E.T,Unit>:
  do Result<&2,&2,E.T,Unit>:
    ok : Unit <- require(mu_negative(Check.infer_term(G.initial(),B.unlimited(),T.Ran{S.SMu{"F",[T.Univ{L.zero()}]},T.Sec{S.SColl{Width.of_nat(0n)},[]}})),"right mu former was admitted")
    ok : Unit <- require(self_negative(Totality.guard(B.unlimited(),G.initial(),"self",T.Global{"Nat"},T.Global{"self"})),"unguarded recursion was admitted")
    checked : Elab.Checked <- Elab.check_in(G.initial(),B.unlimited(),source())
    validate(checked)

def report(result: Result<&2,&2,E.T,Unit>) -> IO(Unit):
  match result:
    case Done{ok}: IO.print("PASS original recursive values, guarded neutrals, opacity and raw kernel negatives")
    case Fail{error}: IO.die(Unit,1,E.to_string(error))

def main() -> IO(Unit):
  report(suite())
