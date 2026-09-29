import Base
import ../wasm/emit.bend as Emit
import ../kernel/eterm.bend as E
import ../kernel/erase.bend as R
import ../kernel/literal.bend as Lit
import ../kernel/bignum.bend as B
import ../kernel/common.bend as C
import ../kernel/error.bend as Err

# Static erased snapshots from the original Wasm fixtures, with original runtime answers.
# Regenerate explicitly with wasm/generate_oracle_fixture.py; no compiler fallback is used.

def rows_0() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"circuit-named-tree",R.Code{[
    E.KRec{[E.Tid{"mu<CircuitTree>"},E.Tid{"leg<mu<CircuitTree>,0,union nat>"},E.Tid{"leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"}]},
    E.KFun{E.Fid{"tree"},[],E.RUnion{E.Tid{"mu<CircuitTree>"}},E.KTag{E.Tid{"mu<CircuitTree>"},1n,[E.KTag{E.Tid{"mu<CircuitTree>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[3]}}}]},E.KTag{E.Tid{"mu<CircuitTree>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]}},
    E.KRec{[E.Tid{"mu<CircuitTree>"}]},
    E.KFun{E.Fid{"treeAlias"},[],E.RUnion{E.Tid{"mu<CircuitTree>"}},E.KGlobal{"tree"}},
    E.KRec{[E.Tid{"mu<CircuitTree>"},E.Tid{"leg<mu<CircuitTree>,0,union nat>"},E.Tid{"leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"}]},
    E.KFun{E.Fid{"leaf"},[],E.RUnion{E.Tid{"mu<CircuitTree>"}},E.KTag{E.Tid{"mu<CircuitTree>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<CircuitTree>"},E.Tid{"leg<mu<CircuitTree>,0,union nat>"},E.Tid{"leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"}]},
    E.KFun{E.Fid{"bump"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<CircuitTree>"},E.KGlobal{"leaf"},[E.Branch{0n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KVar{1n}]}},E.Branch{1n,2n,E.KLit{Lit.LInt{B.Big{False{},[]}}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<CircuitTree>"},E.Tid{"leg<mu<CircuitTree>,0,union nat>"},E.Tid{"leg<mu<CircuitTree>,1,union mu<CircuitTree>,union mu<CircuitTree>>"}]},
    E.KFun{E.Fid{"branch"},[],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<CircuitTree>"},E.KGlobal{"treeAlias"},[E.Branch{0n,1n,E.KVar{0n}},E.Branch{1n,2n,E.KLit{Lit.LInt{B.Big{False{},[34]}}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"bump"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KGlobal{"branch"}]}}
  ]}}]

def rows_1() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d01-lit-prims",R.Code{[
    E.KRec{[E.Tid{"sum<unit|unit>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"toNat"},[E.RUnion{E.Tid{"sum<unit|unit>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,0n,E.KLit{Lit.LInt{B.Big{False{},[1]}}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"scaled"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natMul"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[2]}}},E.KLit{Lit.LInt{B.Big{False{},[3]}}}]},E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}},E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"flags"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"toNat"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[4]}}},E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]},E.KApp{E.KGlobal{"toNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}]}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KGlobal{"scaled"},E.KGlobal{"flags"}]}}
  ]}}]

def rows_2() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d02-tail-call",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"step"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"chain"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"step"},[E.KApp{E.KGlobal{"natAdd"},[E.KVar{1n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"outer"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"chain"},[E.KApp{E.KGlobal{"natAdd"},[E.KVar{1n},E.KLit{Lit.LInt{B.Big{False{},[2]}}}]},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"outer"},[E.KLit{Lit.LInt{B.Big{False{},[3]}}},E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}}
  ]}}]

def rows_3() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d03-pair",R.Code{[
    E.KRec{[E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"p"},[],E.RStruct{E.Tid{"pair<union nat,union nat>"}},E.KStruct{E.Tid{"pair<union nat,union nat>"},[E.KLit{Lit.LInt{B.Big{False{},[11]}}},E.KLit{Lit.LInt{B.Big{False{},[6]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"mk"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"pair<union nat,union nat>"}},E.KStruct{E.Tid{"pair<union nat,union nat>"},[E.KVar{0n},E.KApp{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}]}},
    E.KRec{[E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"q"},[],E.RStruct{E.Tid{"pair<union nat,union nat>"}},E.KTail{E.KGlobal{"mk"},[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KLet{"scrut",E.KGlobal{"p"},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},E.KLet{"scrut",E.KGlobal{"p"},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{0n}}}}]},E.KApp{E.KGlobal{"natAdd"},[E.KLet{"scrut",E.KGlobal{"q"},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},E.KLet{"scrut",E.KGlobal{"q"},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{0n}}}}]}]}}
  ]}}]

def rows_4() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d04-tuple",R.Code{[
    E.KRec{[E.Tid{"tuple<union nat,union nat,union nat>"}]},
    E.KFun{E.Fid{"t"},[],E.RStruct{E.Tid{"tuple<union nat,union nat,union nat>"}},E.KStruct{E.Tid{"tuple<union nat,union nat,union nat>"},[E.KLit{Lit.LInt{B.Big{False{},[2]}}},E.KLit{Lit.LInt{B.Big{False{},[3]}}},E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,union nat,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natMul"},[E.KProj{E.Tid{"tuple<union nat,union nat,union nat>"},0n,E.KGlobal{"t"}},E.KProj{E.Tid{"tuple<union nat,union nat,union nat>"},1n,E.KGlobal{"t"}}]},E.KProj{E.Tid{"tuple<union nat,union nat,union nat>"},2n,E.KGlobal{"t"}}]}}
  ]}}]

def rows_5() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d05-sum-legs",R.Code{[
    E.KRec{[E.Tid{"sum<unit|unit|union nat|union nat>"}]},
    E.KFun{E.Fid{"z0"},[],E.RUnion{E.Tid{"sum<unit|unit|union nat|union nat>"}},E.KTag{E.Tid{"sum<unit|unit|union nat|union nat>"},0n,[]}},
    E.KRec{[E.Tid{"sum<unit|unit|union nat|union nat>"}]},
    E.KFun{E.Fid{"z1"},[],E.RUnion{E.Tid{"sum<unit|unit|union nat|union nat>"}},E.KTag{E.Tid{"sum<unit|unit|union nat|union nat>"},1n,[]}},
    E.KRec{[E.Tid{"sum<unit|unit|union nat|union nat>"}]},
    E.KFun{E.Fid{"p2"},[],E.RUnion{E.Tid{"sum<unit|unit|union nat|union nat>"}},E.KTag{E.Tid{"sum<unit|unit|union nat|union nat>"},2n,[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]}},
    E.KRec{[E.Tid{"sum<unit|unit|union nat|union nat>"}]},
    E.KFun{E.Fid{"p3"},[],E.RUnion{E.Tid{"sum<unit|unit|union nat|union nat>"}},E.KTag{E.Tid{"sum<unit|unit|union nat|union nat>"},3n,[E.KLit{Lit.LInt{B.Big{False{},[6]}}}]}},
    E.KRec{[E.Tid{"sum<unit|unit|union nat|union nat>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"read"},[E.RUnion{E.Tid{"sum<unit|unit|union nat|union nat>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit|union nat|union nat>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[1]}}}},E.Branch{1n,0n,E.KLit{Lit.LInt{B.Big{False{},[2]}}}},E.Branch{2n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[10]}}}]}},E.Branch{3n,1n,E.KTail{E.KGlobal{"natMul"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[10]}}}]}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"read"},[E.KGlobal{"z0"}]},E.KApp{E.KGlobal{"read"},[E.KGlobal{"z1"}]}]},E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"read"},[E.KGlobal{"p2"}]},E.KApp{E.KGlobal{"read"},[E.KGlobal{"p3"}]}]}]}}
  ]}}]

def rows_6() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d06-closure-capture",R.Code{[
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<func fn<1>,union nat>"}]},
    E.KFun{E.Fid{"mk$0"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"mk"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"pair<func fn<1>,union nat>"}},E.KStruct{E.Tid{"pair<func fn<1>,union nat>"},[E.KClos{E.Fid{"mk$0"},1n,[E.KVar{0n}]},E.KApp{E.KGlobal{"natMul"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<func fn<1>,union nat>"}]},
    E.KFun{E.Fid{"mkLet$0"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"mkLet"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"pair<func fn<1>,union nat>"}},E.KLet{"z",E.KApp{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KStruct{E.Tid{"pair<func fn<1>,union nat>"},[E.KClos{E.Fid{"mkLet$0"},1n,[E.KVar{0n}]},E.KVar{0n}]}}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<func fn<1>,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KLet{"scrut",E.KApp{E.KGlobal{"mk"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]},E.KLet{"f",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},[E.KLit{Lit.LInt{B.Big{False{},[6]}}}]},E.KLet{"scrut",E.KApp{E.KGlobal{"mk"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]},E.KLet{"f",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},1n,E.KVar{1n}},E.KVar{0n}}}}]},E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KLet{"scrut",E.KApp{E.KGlobal{"mkLet"},[E.KLit{Lit.LInt{B.Big{False{},[2]}}}]},E.KLet{"f",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]},E.KLet{"scrut",E.KApp{E.KGlobal{"mkLet"},[E.KLit{Lit.LInt{B.Big{False{},[2]}}}]},E.KLet{"f",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},1n,E.KVar{1n}},E.KVar{0n}}}}]}]}}
  ]}}]

def rows_7() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d07-partial-over",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"add3"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]},E.KLit{Lit.LInt{B.Big{False{},[3]}}}]}},
    E.KRec{[E.Tid{"fn<2>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"later"},[E.RFunc{E.Tid{"fn<2>"}},E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KApp{E.KVar{2n},[E.KVar{1n}]},[E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"half"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KApp{E.KGlobal{"add3"},[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]},[E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"alias"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},[E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"half"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]},E.KApp{E.KGlobal{"alias"},[E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}]},E.KApp{E.KGlobal{"later"},[E.KGlobal{"natAdd"},E.KLit{Lit.LInt{B.Big{False{},[4]}}},E.KLit{Lit.LInt{B.Big{False{},[6]}}}]}]}}
  ]}}]

def rows_8() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d08-erased-poly",R.Code{[
    E.KRec{[E.Tid{"any"}]},
    E.KFun{E.Fid{"id"},[E.RUnion{E.Tid{"any"}}],E.RUnion{E.Tid{"any"}},E.KVar{0n}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"idNat"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"id"},[E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"id"},[E.KLit{Lit.LInt{B.Big{False{},[3]}}}]},E.KApp{E.KGlobal{"idNat"},[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]}}
  ]}}]

def rows_9() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d09-let-chain",R.Code{[
    E.KRec{[E.Tid{"sum<union nat|union nat>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"pick"},[E.RUnion{E.Tid{"sum<union nat|union nat>"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"base",E.KLit{Lit.LInt{B.Big{False{},[10]}}},E.KLet{"bump",E.KCase{E.Tid{"sum<union nat|union nat>"},E.KVar{1n},[E.Branch{0n,1n,E.KApp{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},E.Branch{1n,1n,E.KApp{E.KGlobal{"natMul"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}}]},E.KLet{"total",E.KApp{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]},E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[100]}}}]}}}}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<union nat|union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"pick"},[E.KTag{E.Tid{"sum<union nat|union nat>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[3]}}}]}]},E.KApp{E.KGlobal{"pick"},[E.KTag{E.Tid{"sum<union nat|union nat>"},1n,[E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]}]}}
  ]}}]

def rows_10() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d10-i31-boundary",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[32767,32767]}}},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}}
  ]}}]

def rows_11() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d11-poly-pair",R.Code{[
    E.KRec{[E.Tid{"any"},E.Tid{"pair<union any,union any>"}]},
    E.KFun{E.Fid{"box"},[E.RUnion{E.Tid{"any"}}],E.RStruct{E.Tid{"pair<union any,union any>"}},E.KStruct{E.Tid{"pair<union any,union any>"},[E.KVar{0n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KLet{"scrut",E.KApp{E.KGlobal{"box"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}}}
  ]}}]

def rows_12() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d12-poly-case",R.Code{[
    E.KRec{[E.Tid{"any"}]},
    E.KFun{E.Fid{"id"},[E.RUnion{E.Tid{"any"}}],E.RUnion{E.Tid{"any"}},E.KVar{0n}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KApp{E.KGlobal{"id"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}]},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,0n,E.KLit{Lit.LInt{B.Big{False{},[1]}}}}]}}
  ]}}]

def rows_13() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d13-function-case",R.Code{[
    E.KRec{[E.Tid{"fn<1>"},E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"read"},[E.RFunc{E.Tid{"fn<1>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KApp{E.KVar{0n},[E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,0n,E.KLit{Lit.LInt{B.Big{False{},[1]}}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"eqOne"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"sum<unit|unit>"}},E.KTail{E.KGlobal{"natEq"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"read"},[E.KGlobal{"eqOne"}]}}
  ]}}]

def rows_14() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d14-generic-capture",R.Code{[
    E.KRec{[E.Tid{"any"}]},
    E.KFun{E.Fid{"id"},[E.RUnion{E.Tid{"any"}}],E.RUnion{E.Tid{"any"}},E.KVar{0n}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<func fn<1>,union nat>"}]},
    E.KFun{E.Fid{"mk$0"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"mk"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"pair<func fn<1>,union nat>"}},E.KLet{"z",E.KApp{E.KGlobal{"id"},[E.KVar{0n}]},E.KStruct{E.Tid{"pair<func fn<1>,union nat>"},[E.KClos{E.Fid{"mk$0"},1n,[E.KVar{0n}]},E.KVar{0n}]}}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<func fn<1>,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KLet{"scrut",E.KApp{E.KGlobal{"mk"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]},E.KLet{"f",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<func fn<1>,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},[E.KLit{Lit.LInt{B.Big{False{},[6]}}}]}}
  ]}}]

def rows_15() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"d15-generic-aggregates",R.Code{[
    E.KRec{[E.Tid{"any"}]},
    E.KFun{E.Fid{"id"},[E.RUnion{E.Tid{"any"}}],E.RUnion{E.Tid{"any"}},E.KVar{0n}},
    E.KRec{[E.Tid{"any"},E.Tid{"pair<union any,union any>"}]},
    E.KFun{E.Fid{"box"},[E.RUnion{E.Tid{"any"}}],E.RStruct{E.Tid{"pair<union any,union any>"}},E.KStruct{E.Tid{"pair<union any,union any>"},[E.KVar{0n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"pair<union any,union any>"},E.Tid{"any"}]},
    E.KFun{E.Fid{"first"},[E.RStruct{E.Tid{"pair<union any,union any>"}}],E.RUnion{E.Tid{"any"}},E.KLet{"scrut",E.KVar{0n},E.KLet{"a",E.KProj{E.Tid{"pair<union any,union any>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union any,union any>"},1n,E.KVar{1n}},E.KVar{1n}}}}},
    E.KRec{[E.Tid{"any"},E.Tid{"tuple<union any,union any>"}]},
    E.KFun{E.Fid{"pack"},[E.RUnion{E.Tid{"any"}}],E.RStruct{E.Tid{"tuple<union any,union any>"}},E.KStruct{E.Tid{"tuple<union any,union any>"},[E.KVar{0n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"any"},E.Tid{"sum<union any|union any>"}]},
    E.KFun{E.Fid{"choose"},[E.RUnion{E.Tid{"any"}}],E.RUnion{E.Tid{"sum<union any|union any>"}},E.KTag{E.Tid{"sum<union any|union any>"},1n,[E.KVar{0n}]}},
    E.KRec{[E.Tid{"sum<union any|union any>"},E.Tid{"any"}]},
    E.KFun{E.Fid{"unchoose"},[E.RUnion{E.Tid{"sum<union any|union any>"}}],E.RUnion{E.Tid{"any"}},E.KCase{E.Tid{"sum<union any|union any>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}},E.Branch{1n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"any"}]},
    E.KFun{E.Fid{"fromEmpty"},[],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"any"},E.KErased{},[]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<union nat|union nat>"}]},
    E.KFun{E.Fid{"sumNat"},[],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<union nat|union nat>"},E.KApp{E.KGlobal{"id"},[E.KApp{E.KGlobal{"choose"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}]},[E.Branch{0n,1n,E.KVar{0n}},E.Branch{1n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"nested"},[],E.RUnion{E.Tid{"nat"}},E.KLet{"scrut",E.KLet{"scrut",E.KApp{E.KGlobal{"box"},[E.KApp{E.KGlobal{"box"},[E.KLit{Lit.LInt{B.Big{False{},[5]}}}]}]},E.KLet{"a",E.KProj{E.Tid{"pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<struct pair<union nat,union nat>,struct pair<union nat,union nat>>"},1n,E.KVar{1n}},E.KVar{1n}}}},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{0n}}}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"inc"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<func fn<1>,func fn<1>>"}]},
    E.KFun{E.Fid{"fromClosure"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KProj{E.Tid{"tuple<func fn<1>,func fn<1>>"},0n,E.KApp{E.KGlobal{"pack"},[E.KGlobal{"inc"}]}},[E.KLit{Lit.LInt{B.Big{False{},[8]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"},E.Tid{"tuple<union nat,union nat>"},E.Tid{"sum<union nat|union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"first"},[E.KStruct{E.Tid{"pair<union nat,union nat>"},[E.KLit{Lit.LInt{B.Big{False{},[3]}}},E.KLit{Lit.LInt{B.Big{False{},[4]}}}]}]},E.KProj{E.Tid{"tuple<union nat,union nat>"},1n,E.KApp{E.KGlobal{"pack"},[E.KLit{Lit.LInt{B.Big{False{},[9]}}}]}}]},E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"unchoose"},[E.KTag{E.Tid{"sum<union nat|union nat>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}]},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"sumNat"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"nested"},E.KGlobal{"fromClosure"}]}]}]}]}}
  ]}}]

def rows_16() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"mu-cata-depth",R.Code{[
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"dbl"},[E.RUnion{E.Tid{"mu<N>"}}],E.RUnion{E.Tid{"mu<N>"}},E.KCase{E.Tid{"mu<N>"},E.KVar{0n},[E.Branch{0n,0n,E.KTag{E.Tid{"mu<N>"},0n,[]}},E.Branch{1n,1n,E.KTag{E.Tid{"mu<N>"},1n,[E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KVar{0n}]}]}]}}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n0"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KTag{E.Tid{"mu<N>"},0n,[]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n1"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n0"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n2"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n1"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n3"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n2"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n4"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n3"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n5"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n4"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n6"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n5"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n7"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n6"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n8"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n7"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n9"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n8"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n10"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n9"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"n11"},[],E.RUnion{E.Tid{"mu<N>"}},E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"dbl"},[E.KGlobal{"n10"}]}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"nat"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"toNat"},[E.RUnion{E.Tid{"mu<N>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<N>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KApp{E.KGlobal{"toNat"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[]}}},E.KApp{E.KGlobal{"toNat"},[E.KGlobal{"n11"}]}]}}
  ]}}]

def rows_17() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"mu-dependent-copy",R.Code{[
    E.KRec{[E.Tid{"mu<V>"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"copy"},[E.RUnion{E.Tid{"mu<V>"}}],E.RUnion{E.Tid{"mu<V>"}},E.KCase{E.Tid{"mu<V>"},E.KVar{0n},[E.Branch{0n,0n,E.KTag{E.Tid{"mu<V>"},0n,[]}},E.Branch{1n,2n,E.KTag{E.Tid{"mu<V>"},1n,[E.KVar{1n},E.KApp{E.KGlobal{"copy"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<V>"},E.Tid{"nat"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"total"},[E.RUnion{E.Tid{"mu<V>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<V>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,2n,E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KApp{E.KGlobal{"total"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<V>"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"empty"},[],E.RUnion{E.Tid{"mu<V>"}},E.KTail{E.KGlobal{"copy"},[E.KTag{E.Tid{"mu<V>"},0n,[]}]}},
    E.KRec{[E.Tid{"mu<V>"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"values"},[],E.RUnion{E.Tid{"mu<V>"}},E.KTag{E.Tid{"mu<V>"},1n,[E.KLit{Lit.LInt{B.Big{False{},[17]}}},E.KTag{E.Tid{"mu<V>"},1n,[E.KLit{Lit.LInt{B.Big{False{},[25]}}},E.KGlobal{"empty"}]}]}},
    E.KRec{[E.Tid{"mu<V>"}]},
    E.KFun{E.Fid{"copied"},[],E.RUnion{E.Tid{"mu<V>"}},E.KTail{E.KGlobal{"copy"},[E.KGlobal{"values"}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"total"},[E.KGlobal{"copied"}]}}
  ]}}]

def rows_18() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"mu-dependent-layout",R.Code{[
    E.KRec{[E.Tid{"mu<Pack>"},E.Tid{"leg<mu<Pack>,0,union any,union nat>"}]},
    E.KFun{E.Fid{"pn"},[],E.RUnion{E.Tid{"mu<Pack>"}},E.KTag{E.Tid{"mu<Pack>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[7]}}},E.KLit{Lit.LInt{B.Big{False{},[8]}}}]}},
    E.KRec{[E.Tid{"mu<Pack>"},E.Tid{"leg<mu<Pack>,0,union any,union nat>"}]},
    E.KFun{E.Fid{"pu"},[],E.RUnion{E.Tid{"mu<Pack>"}},E.KTag{E.Tid{"mu<Pack>"},0n,[E.KErased{},E.KLit{Lit.LInt{B.Big{False{},[9]}}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<Pack>"},E.Tid{"leg<mu<Pack>,0,union any,union nat>"}]},
    E.KFun{E.Fid{"pf$0"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KFun{E.Fid{"pf"},[],E.RUnion{E.Tid{"mu<Pack>"}},E.KTag{E.Tid{"mu<Pack>"},0n,[E.KClos{E.Fid{"pf$0"},1n,[]},E.KLit{Lit.LInt{B.Big{False{},[10]}}}]}},
    E.KRec{[E.Tid{"mu<Pack>"},E.Tid{"nat"},E.Tid{"leg<mu<Pack>,0,union any,union nat>"}]},
    E.KFun{E.Fid{"last"},[E.RUnion{E.Tid{"mu<Pack>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Pack>"},E.KVar{0n},[E.Branch{0n,2n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"last"},[E.KGlobal{"pn"}]},E.KApp{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"last"},[E.KGlobal{"pu"}]},E.KApp{E.KGlobal{"last"},[E.KGlobal{"pf"}]}]}]}}
  ]}}]

def rows_19() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"mu-mutual-emit",R.Code{[
    E.KRec{[E.Tid{"mu<A>"},E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"},E.Tid{"leg<mu<A>,0>"},E.Tid{"leg<mu<A>,1,union mu<B>>"}]},
    E.KFun{E.Fid{"sizeA"},[E.RUnion{E.Tid{"mu<A>"}}],E.RUnion{E.Tid{"mu<N>"}},E.KCase{E.Tid{"mu<A>"},E.KVar{0n},[E.Branch{0n,0n,E.KTag{E.Tid{"mu<N>"},0n,[]}},E.Branch{1n,1n,E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"sizeB"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<B>"},E.Tid{"mu<N>"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"},E.Tid{"leg<mu<B>,0,union mu<A>>"}]},
    E.KFun{E.Fid{"sizeB"},[E.RUnion{E.Tid{"mu<B>"}}],E.RUnion{E.Tid{"mu<N>"}},E.KCase{E.Tid{"mu<B>"},E.KVar{0n},[E.Branch{0n,1n,E.KTag{E.Tid{"mu<N>"},1n,[E.KApp{E.KGlobal{"sizeA"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<N>"},E.Tid{"nat"},E.Tid{"leg<mu<N>,0>"},E.Tid{"leg<mu<N>,1,union mu<N>>"}]},
    E.KFun{E.Fid{"toNat"},[E.RUnion{E.Tid{"mu<N>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<N>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KApp{E.KGlobal{"toNat"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<A>"},E.Tid{"mu<B>"},E.Tid{"leg<mu<A>,0>"},E.Tid{"leg<mu<A>,1,union mu<B>>"},E.Tid{"leg<mu<B>,0,union mu<A>>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"toNat"},[E.KApp{E.KGlobal{"sizeA"},[E.KTag{E.Tid{"mu<A>"},1n,[E.KTag{E.Tid{"mu<B>"},0n,[E.KTag{E.Tid{"mu<A>"},1n,[E.KTag{E.Tid{"mu<B>"},0n,[E.KTag{E.Tid{"mu<A>"},0n,[]}]}]}]}]}]}]}}
  ]}}]

def rows_20() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"mu-parameter-layout",R.Code{[
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union any>"}]},
    E.KFun{E.Fid{"unboxN"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union any>"}]},
    E.KFun{E.Fid{"unboxF"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KTail{E.KVar{0n},[E.KLit{Lit.LInt{B.Big{False{},[8]}}}]}}]}},
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"tuple<union nat,union nat>"},E.Tid{"leg<mu<Box>,0,union any>"}]},
    E.KFun{E.Fid{"unboxP"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KProj{E.Tid{"tuple<union nat,union nat>"},1n,E.KVar{0n}}}]}},
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union any>"}]},
    E.KFun{E.Fid{"unboxU"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KLit{Lit.LInt{B.Big{False{},[9]}}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[9]}}}}
  ]}}]

def rows_21() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"nat-big",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"mul"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natMul"},[E.KVar{1n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natSub"},[E.KApp{E.KGlobal{"mul"},[E.KLit{Lit.LInt{B.Big{False{},[1,0,0,1]}}},E.KLit{Lit.LInt{B.Big{False{},[5,0,0,1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[32766,32767,32767,5,0,0,1]}}}]}}
  ]}}]

def rows_22() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"nat-runtime-capture",R.Code{[
    E.KRec{[E.Tid{"any"},E.Tid{"pair<union any,union any>"}]},
    E.KFun{E.Fid{"box"},[E.RUnion{E.Tid{"any"}}],E.RStruct{E.Tid{"pair<union any,union any>"}},E.KStruct{E.Tid{"pair<union any,union any>"},[E.KVar{0n},E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"mk"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natSub"},[E.KApp{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]},E.KVar{1n}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"mk"},[E.KLet{"scrut",E.KApp{E.KGlobal{"box"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLet{"a",E.KProj{E.Tid{"pair<union nat,union nat>"},0n,E.KVar{0n}},E.KLet{"y",E.KProj{E.Tid{"pair<union nat,union nat>"},1n,E.KVar{1n}},E.KVar{1n}}}},E.KLit{Lit.LInt{B.Big{False{},[19]}}}]}}
  ]}}]

def rows_23() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"nat-runtime-edges",R.Code{[
    E.KRec{[E.Tid{"sum<unit|unit>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"boolNat"},[E.RUnion{E.Tid{"sum<unit|unit>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,0n,E.KLit{Lit.LInt{B.Big{False{},[1]}}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"checkNat"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natEq"},[E.KVar{1n},E.KVar{0n}]}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"smallAdd"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[32767,32767]}}},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[0,0,1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"smallMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[0,1]}}},E.KLit{Lit.LInt{B.Big{False{},[0,1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[0,0,1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"bigAdd"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[28949,24680,7906,18564,8]}}}]},E.KLit{Lit.LInt{B.Big{False{},[29226,15579,17832,32378,13100,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"mixedAdd"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[17]}}}]},E.KLit{Lit.LInt{B.Big{False{},[294,23667,9925,13814,13092,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"reverseAdd"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[17]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[294,23667,9925,13814,13092,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"bigSub"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[28949,24680,7906,18564,8]}}}]},E.KLit{Lit.LInt{B.Big{False{},[4096,31754,2018,28018,13083,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"mixedSub"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[17]}}}]},E.KLit{Lit.LInt{B.Big{False{},[260,23667,9925,13814,13092,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"truncated"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[17]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"zeroSub"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"borrow"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[0,0,0,0,0,0,1]}}},E.KLit{Lit.LInt{B.Big{False{},[32767,32767,32767,32767,32767,32767]}}}]},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"carry"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[32767,32767,32767,32767,32767,32767]}}},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[0,0,0,0,0,0,1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"bigMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[28949,24680,7906,18564,8]}}}]},E.KLit{Lit.LInt{B.Big{False{},[23481,10731,3816,6581,397,9285,32068,24649,30124,14155,85]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"mixedMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[32767]}}}]},E.KLit{Lit.LInt{B.Big{False{},[32491,9377,13741,28879,721,13985,31865,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"reverseMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[32767]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[32491,9377,13741,28879,721,13985,31865,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"zeroMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[]}}}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"reverseZero"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"oneMul"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"reverseOne"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"demote"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[278,23667,9924,13814,13092,31875,9]}}}]},E.KLit{Lit.LInt{B.Big{False{},[32767,32767]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"heterogeneous"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KLit{Lit.LInt{B.Big{False{},[3,2,1]}}},E.KLit{Lit.LInt{B.Big{False{},[32766,1,1]}}}]},E.KLit{Lit.LInt{B.Big{False{},[5]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"eqSame"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"eqDifferent"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[278,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"eqMixed"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[3]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"eqReverse"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natEq"},[E.KLit{Lit.LInt{B.Big{False{},[3]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ltSame"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ltNext"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[278,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ltPrevious"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[278,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ltMixed"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[3]}}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[1]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ltReverse"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"boolNat"},[E.KApp{E.KGlobal{"natLt"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLit{Lit.LInt{B.Big{False{},[3]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"alias"},[],E.RUnion{E.Tid{"nat"}},E.KLet{"a",E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}},E.KLet{"twice",E.KApp{E.KGlobal{"natAdd"},[E.KVar{0n},E.KVar{0n}]},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"checkNat"},[E.KVar{1n},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KApp{E.KGlobal{"checkNat"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[554,14566,19851,27628,26184,30982,19]}}}]}]}}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"partial"},[],E.RUnion{E.Tid{"nat"}},E.KLet{"mul",E.KApp{E.KGlobal{"natMul"},[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]},E.KTail{E.KGlobal{"checkNat"},[E.KApp{E.KGlobal{"natSub"},[E.KApp{E.KVar{0n},[E.KLit{Lit.LInt{B.Big{False{},[3]}}}]},E.KLit{Lit.LInt{B.Big{False{},[829,5465,29777,8674,6509,30090,29]}}}]},E.KLit{Lit.LInt{B.Big{False{},[2]}}}]}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KGlobal{"smallAdd"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"smallMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"bigAdd"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"mixedAdd"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"reverseAdd"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"bigSub"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"mixedSub"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"truncated"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"zeroSub"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"borrow"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"carry"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"bigMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"mixedMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"reverseMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"zeroMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"reverseZero"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"oneMul"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"reverseOne"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"demote"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"heterogeneous"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"eqSame"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"eqDifferent"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"eqMixed"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"eqReverse"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"ltSame"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"ltNext"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"ltPrevious"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"ltMixed"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"ltReverse"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"alias"},E.KApp{E.KGlobal{"natAdd"},[E.KGlobal{"partial"},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}}
  ]}}]

def rows_24() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"nat-runtime-export",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}}
  ]}}]

def rows_25() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"nat-runtime-mu",R.Code{[
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union nat>"}]},
    E.KFun{E.Fid{"read"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<Box>"},E.Tid{"leg<mu<Box>,0,union nat>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natSub"},[E.KApp{E.KGlobal{"read"},[E.KTag{E.Tid{"mu<Box>"},0n,[E.KLit{Lit.LInt{B.Big{False{},[277,23667,9925,13814,13092,31875,9]}}}]}]},E.KLit{Lit.LInt{B.Big{False{},[254,23667,9925,13814,13092,31875,9]}}}]}}
  ]}}]

def rows_26() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"one-branches",R.Code{[
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"choose"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"sum<unit|unit>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,E.KVar{1n}},E.Branch{1n,0n,E.KVar{1n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"nested"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"sum<unit|unit>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,E.KCase{E.Tid{"sum<unit|unit>"},E.KVar{0n},[E.Branch{0n,0n,E.KVar{1n}},E.Branch{1n,0n,E.KVar{1n}}]}},E.Branch{1n,0n,E.KVar{1n}}]}},
    E.KRec{[E.Tid{"sum<union nat|union nat>"},E.Tid{"nat"}]},
    E.KFun{E.Fid{"payload"},[E.RUnion{E.Tid{"sum<union nat|union nat>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"sum<union nat|union nat>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}},E.Branch{1n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"sum<unit|unit>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"choose"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}},E.KTag{E.Tid{"sum<unit|unit>"},0n,[]}]}}
  ]}}]

def rows_27() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"one-fields",R.Code{[
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<Box>"},E.Tid{"leg<mu<Box>,0,union nat>"}]},
    E.KFun{E.Fid{"pack"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"mu<Box>"}},E.KTag{E.Tid{"mu<Box>"},0n,[E.KVar{0n}]}},
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union nat>"}]},
    E.KFun{E.Fid{"unpack"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"pair<union nat,union nat>"}]},
    E.KFun{E.Fid{"pair"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"pair<union nat,union nat>"}},E.KStruct{E.Tid{"pair<union nat,union nat>"},[E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[]}}}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"unpack"},[E.KApp{E.KGlobal{"pack"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}]}}
  ]}}]

def rows_28() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"one-identity",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"identity"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KVar{0n}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"through"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"identity"},[E.KVar{0n}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"alias"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"y",E.KVar{0n},E.KVar{0n}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"discardAlias"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"y",E.KVar{0n},E.KLit{Lit.LInt{B.Big{False{},[]}}}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ghost"},[],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[]}}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"erasedThenOnce"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"y",E.KApp{E.KGlobal{"ghost"},[]},E.KVar{1n}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"through"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}}
  ]}}]

def rows_29() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"one-runtime-capture",R.Code{[
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,func fn<0>>"}]},
    E.KFun{E.Fid{"duplicate$0"},[],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[]}}}},
    E.KFun{E.Fid{"duplicate"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"tuple<union nat,func fn<0>>"}},E.KStruct{E.Tid{"tuple<union nat,func fn<0>>"},[E.KVar{0n},E.KClos{E.Fid{"duplicate$0"},0n,[]}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,func fn<1>>"}]},
    E.KFun{E.Fid{"split$0"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KVar{0n}]}},
    E.KFun{E.Fid{"split"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"tuple<union nat,func fn<1>>"}},E.KStruct{E.Tid{"tuple<union nat,func fn<1>>"},[E.KVar{1n},E.KClos{E.Fid{"split$0"},1n,[E.KVar{0n}]}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<At>"},E.Tid{"tuple<union nat,func fn<1>>"},E.Tid{"leg<mu<At>,0>"}]},
    E.KFun{E.Fid{"constructorCapture$0"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"mu<At>"}},E.KTag{E.Tid{"mu<At>"},0n,[]}},
    E.KFun{E.Fid{"constructorCapture"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"tuple<union nat,func fn<1>>"}},E.KStruct{E.Tid{"tuple<union nat,func fn<1>>"},[E.KVar{0n},E.KClos{E.Fid{"constructorCapture$0"},1n,[]}]}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"ignore"},[],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[]}}}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,func fn<1>>"}]},
    E.KFun{E.Fid{"callCapture$0"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"ignore"},[]}},
    E.KFun{E.Fid{"callCapture"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"tuple<union nat,func fn<1>>"}},E.KStruct{E.Tid{"tuple<union nat,func fn<1>>"},[E.KVar{0n},E.KClos{E.Fid{"callCapture$0"},1n,[]}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,func fn<1>>"}]},
    E.KFun{E.Fid{"proofCapture$0"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLit{Lit.LInt{B.Big{False{},[]}}}},
    E.KFun{E.Fid{"proofCapture"},[E.RUnion{E.Tid{"nat"}}],E.RStruct{E.Tid{"tuple<union nat,func fn<1>>"}},E.KStruct{E.Tid{"tuple<union nat,func fn<1>>"},[E.KVar{0n},E.KClos{E.Fid{"proofCapture$0"},1n,[]}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"tuple<union nat,func fn<0>>"},E.Tid{"tuple<union nat,func fn<1>>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"natAdd"},[E.KApp{E.KGlobal{"natAdd"},[E.KProj{E.Tid{"tuple<union nat,func fn<0>>"},0n,E.KApp{E.KGlobal{"duplicate"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}},E.KApp{E.KProj{E.Tid{"tuple<union nat,func fn<1>>"},1n,E.KApp{E.KGlobal{"split"},[E.KLit{Lit.LInt{B.Big{False{},[11]}}},E.KLit{Lit.LInt{B.Big{False{},[13]}}}]}},[E.KLit{Lit.LInt{B.Big{False{},[17]}}}]}]},E.KApp{E.KGlobal{"natAdd"},[E.KProj{E.Tid{"tuple<union nat,func fn<1>>"},0n,E.KApp{E.KGlobal{"constructorCapture"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}},E.KApp{E.KGlobal{"natAdd"},[E.KProj{E.Tid{"tuple<union nat,func fn<1>>"},0n,E.KApp{E.KGlobal{"callCapture"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}},E.KProj{E.Tid{"tuple<union nat,func fn<1>>"},0n,E.KApp{E.KGlobal{"proofCapture"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}}]}]}]}}
  ]}}]

def rows_30() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"one-shadowing",R.Code{[
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"identity"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KVar{0n}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"shadow"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"y",E.KVar{0n},E.KLet{"x",E.KLit{Lit.LInt{B.Big{False{},[]}}},E.KVar{1n}}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"closure$0"},[E.RUnion{E.Tid{"nat"}},E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KVar{1n}},
    E.KFun{E.Fid{"closure"},[E.RUnion{E.Tid{"nat"}}],E.RUnion{E.Tid{"nat"}},E.KLet{"f",E.KClos{E.Fid{"closure$0"},1n,[E.KVar{0n}]},E.KTail{E.KVar{0n},[E.KLit{Lit.LInt{B.Big{False{},[]}}}]}}},
    E.KRec{[E.Tid{"nat"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"shadow"},[E.KLit{Lit.LInt{B.Big{False{},[7]}}}]}}
  ]}}]

def rows_31() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"sl-match-indexed",R.Code{[
    E.KRec{[E.Tid{"mu<V>"},E.Tid{"nat"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"total"},[E.RUnion{E.Tid{"mu<V>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<V>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,2n,E.KTail{E.KGlobal{"natAdd"},[E.KVar{1n},E.KApp{E.KGlobal{"total"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<Box>"},E.Tid{"nat"},E.Tid{"leg<mu<Box>,0,union any>"}]},
    E.KFun{E.Fid{"unbox"},[E.RUnion{E.Tid{"mu<Box>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<Box>"},E.KVar{0n},[E.Branch{0n,1n,E.KVar{0n}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<V>"},E.Tid{"leg<mu<V>,0>"},E.Tid{"leg<mu<V>,1,union nat,union mu<V>>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"total"},[E.KTag{E.Tid{"mu<V>"},1n,[E.KLit{Lit.LInt{B.Big{False{},[17]}}},E.KTag{E.Tid{"mu<V>"},1n,[E.KLit{Lit.LInt{B.Big{False{},[25]}}},E.KTag{E.Tid{"mu<V>"},0n,[]}]}]}]}}
  ]}}]

def rows_32() -> List<&2,C.Pair<String,R.Entry>>:
  [C.Pair{"sl-mutual-three",R.Code{[
    E.KRec{[E.Tid{"mu<A>"},E.Tid{"nat"},E.Tid{"leg<mu<A>,0>"},E.Tid{"leg<mu<A>,1,union mu<B>>"}]},
    E.KFun{E.Fid{"sizeA"},[E.RUnion{E.Tid{"mu<A>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<A>"},E.KVar{0n},[E.Branch{0n,0n,E.KLit{Lit.LInt{B.Big{False{},[]}}}},E.Branch{1n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KApp{E.KGlobal{"sizeB"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<B>"},E.Tid{"nat"},E.Tid{"leg<mu<B>,0,union mu<C>>"}]},
    E.KFun{E.Fid{"sizeB"},[E.RUnion{E.Tid{"mu<B>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<B>"},E.KVar{0n},[E.Branch{0n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KApp{E.KGlobal{"sizeC"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"mu<C>"},E.Tid{"nat"},E.Tid{"leg<mu<C>,0,union mu<A>>"}]},
    E.KFun{E.Fid{"sizeC"},[E.RUnion{E.Tid{"mu<C>"}}],E.RUnion{E.Tid{"nat"}},E.KCase{E.Tid{"mu<C>"},E.KVar{0n},[E.Branch{0n,1n,E.KTail{E.KGlobal{"natAdd"},[E.KLit{Lit.LInt{B.Big{False{},[1]}}},E.KApp{E.KGlobal{"sizeA"},[E.KVar{0n}]}]}}]}},
    E.KRec{[E.Tid{"nat"},E.Tid{"mu<A>"},E.Tid{"mu<B>"},E.Tid{"mu<C>"},E.Tid{"leg<mu<A>,0>"},E.Tid{"leg<mu<A>,1,union mu<B>>"},E.Tid{"leg<mu<C>,0,union mu<A>>"},E.Tid{"leg<mu<B>,0,union mu<C>>"}]},
    E.KFun{E.Fid{"main"},[],E.RUnion{E.Tid{"nat"}},E.KTail{E.KGlobal{"sizeA"},[E.KTag{E.Tid{"mu<A>"},1n,[E.KTag{E.Tid{"mu<B>"},0n,[E.KTag{E.Tid{"mu<C>"},0n,[E.KTag{E.Tid{"mu<A>"},0n,[]}]}]}]}]}}
  ]}}]

def bytes_text(xs:List<&2,U32>) -> String:
  match xs:
    case Nil{}: ""
    case Con{x,Nil{}}: U32.show(x)
    case Con{x,rest}: U32.show(x) ++ "," ++ bytes_text(rest)

def output(name:String,expected:String,r:Result<&2,&2,Err.T,List<&2,U32>>) -> IO(Unit):
  match r:
    case Fail{e}: IO.die(Unit,1,name ++ ": " ++ Err.to_string(e))
    case Done{bs}: IO.print("{\"name\":\"" ++ name ++ "\",\"expected\":" ++ expected ++ ",\"bytes\":[" ++ bytes_text(bs) ++ "]}")

def main() -> IO(Unit):
  do IO<Unit>:
    output("circuit-named-tree","{\"value\":42}",Emit.program(rows_0(),"main"))
    output("d01-lit-prims","{\"value\":17}",Emit.program(rows_1(),"main"))
    output("d02-tail-call","{\"value\":10}",Emit.program(rows_2(),"main"))
    output("d03-pair","{\"value\":26}",Emit.program(rows_3(),"main"))
    output("d04-tuple","{\"value\":10}",Emit.program(rows_4(),"main"))
    output("d05-sum-legs","{\"value\":78}",Emit.program(rows_5(),"main"))
    output("d06-closure-capture","{\"value\":31}",Emit.program(rows_6(),"main"))
    output("d07-partial-over","{\"value\":25}",Emit.program(rows_7(),"main"))
    output("d08-erased-poly","{\"value\":7}",Emit.program(rows_8(),"main"))
    output("d09-let-chain","{\"value\":232}",Emit.program(rows_9(),"main"))
    output("d10-i31-boundary","{\"trap\":true}",Emit.program(rows_10(),"main"))
    output("d11-poly-pair","{\"value\":5}",Emit.program(rows_11(),"main"))
    output("d12-poly-case","{\"value\":1}",Emit.program(rows_12(),"main"))
    output("d13-function-case","{\"value\":1}",Emit.program(rows_13(),"main"))
    output("d14-generic-capture","{\"value\":11}",Emit.program(rows_14(),"main"))
    output("d15-generic-aggregates","{\"value\":36}",Emit.program(rows_15(),"main"))
    output("mu-cata-depth","{\"value\":4095}",Emit.program(rows_16(),"main"))
    output("mu-dependent-copy","{\"value\":42}",Emit.program(rows_17(),"main"))
    output("mu-dependent-layout","{\"value\":27}",Emit.program(rows_18(),"main"))
    output("mu-mutual-emit","{\"value\":4}",Emit.program(rows_19(),"main"))
    output("mu-parameter-layout","{\"value\":9}",Emit.program(rows_20(),"main"))
    output("nat-big","{\"value\":7}",Emit.program(rows_21(),"main"))
    output("nat-runtime-capture","{\"value\":19}",Emit.program(rows_22(),"main"))
    output("nat-runtime-edges","{\"value\":32}",Emit.program(rows_23(),"main"))
    output("nat-runtime-export","{\"trap\":true}",Emit.program(rows_24(),"main"))
    output("nat-runtime-mu","{\"value\":23}",Emit.program(rows_25(),"main"))
    output("one-branches","{\"value\":7}",Emit.program(rows_26(),"main"))
    output("one-fields","{\"value\":7}",Emit.program(rows_27(),"main"))
    output("one-identity","{\"value\":7}",Emit.program(rows_28(),"main"))
    output("one-runtime-capture","{\"value\":58}",Emit.program(rows_29(),"main"))
    output("one-shadowing","{\"value\":7}",Emit.program(rows_30(),"main"))
    output("sl-match-indexed","{\"value\":42}",Emit.program(rows_31(),"main"))
    output("sl-mutual-three","{\"value\":3}",Emit.program(rows_32(),"main"))
