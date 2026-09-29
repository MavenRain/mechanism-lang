import Base

type Probe is Data:
  EquivalentPi{}
  EquivalentZero{}
  Infer{}
  Semantic{prefix:String}
  Refuse{kind:String}
  GlobalResolver{}
  LocalResolver{}
  StringNode{}
  PrenexResolver{}
  Alias{}
  Metadata{}
  NatResolver{}
  Shared{}
  Budget{}
  SharedBudget{}

type Case is Data:
  Case{name:String,source:String,probe:Probe}

def prefix() -> String:
  "{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"68218e876d2a38b1985b8590fff244a83c321783\",\"version\":\"4.31.0\"}}}\n{\"in\":1,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"in\":2,\"str\":{\"pre\":0,\"str\":\"A\"}}\n{\"in\":3,\"str\":{\"pre\":0,\"str\":\"x\"}}\n{\"in\":4,\"str\":{\"pre\":0,\"str\":\"test\"}}\n{\"in\":5,\"str\":{\"pre\":0,\"str\":\"F\"}}\n{\"in\":6,\"str\":{\"pre\":0,\"str\":\"v\"}}\n{\"in\":7,\"str\":{\"pre\":0,\"str\":\"Nat\"}}\n{\"il\":1,\"succ\":0}\n{\"il\":2,\"param\":1}\n{\"il\":3,\"param\":6}\n{\"il\":4,\"imax\":[2,3]}\n{\"il\":5,\"succ\":4}\n{\"il\":6,\"max\":[2,3]}\n"

def expression_chain(left:Nat,+index:U32) -> String:
  match left:
    case Zero{}: ""
    case Succ{rest}:
      "{\"ie\":" ++ U32.show(index) ++ ",\"mdata\":{\"expr\":" ++ U32.show(U32.sub(index,1)) ++ ",\"data\":{}}}\n" ++ expression_chain(rest,U32.add(index,1))

def level_chain(left:Nat,+index:U32) -> String:
  match left:
    case Zero{}: ""
    case Succ{rest}:
      "{\"il\":" ++ U32.show(index) ++ ",\"succ\":" ++ U32.show(U32.sub(index,1)) ++ "}\n" ++ level_chain(rest,U32.add(index,1))

def cases() -> List<&2,Case>:
  [
    Case{"closed-dependent-pi",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n",EquivalentPi{}},
    Case{"local-application-infers-domain",prefix() ++ "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":5,\"app\":{\"fn\":3,\"arg\":1}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":4,\"body\":5,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":6,\"isUnsafe\":false}}\n",Infer{}},
    Case{"lambda-result-type-and-application",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"lam\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"app\":{\"fn\":3,\"arg\":2}}\n{\"ie\":5,\"forallE\":{\"name\":3,\"type\":1,\"body\":4,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":5,\"isUnsafe\":false}}\n",EquivalentPi{}},
    Case{"let-preserves-dependent-scope",prefix() ++ "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"letE\":{\"name\":3,\"type\":2,\"value\":1,\"body\":3,\"nondep\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":4,\"isUnsafe\":false}}\n",EquivalentZero{}},
    Case{"unbound-term-index",prefix() ++ "{\"ie\":1,\"bvar\":0}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"type of test has 1 unbound"}},
    Case{"binder-domain-is-outside-own-scope",prefix() ++ "{\"ie\":1,\"bvar\":0}\n{\"ie\":2,\"sort\":1}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n",Semantic{"type of test has 1 unbound"}},
    Case{"shared-node-scope-is-per-root",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"bvar\":0}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",Semantic{"type of test has 1 unbound"}},
    Case{"unbound-universe",prefix() ++ "{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"type of test has unbound universe"}},
    Case{"universe-instantiation-arity",prefix() ++ "{\"ie\":1,\"sort\":2}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[1],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",Semantic{"constant F expects 1 universe"}},
    Case{"duplicate-universe-parameters",prefix() ++ "{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,1],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"duplicate universe parameters"}},
    Case{"unknown-source-constant",prefix() ++ "{\"ie\":1,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"type references undeclared constant F"}},
    Case{"mapping-is-explicit-even-for-nat",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":7,\"us\":[]}}\n{\"axiom\":{\"name\":7,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",Refuse{"deferred"}},
    Case{"resolver-cannot-return-an-unknown-global",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",GlobalResolver{}},
    Case{"resolver-cannot-capture-a-local",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[]}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":1,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n",LocalResolver{}},
    Case{"non-type-root-is-refused",prefix() ++ "{\"ie\":1,\"natVal\":\"13\"}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",Refuse{"kernel"}},
    Case{"string-refusal-retains-node",prefix() ++ "{\"ie\":1,\"strVal\":\"retained\"}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",StringNode{}},
    Case{"projection-needs-target-representation",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"proj\":{\"typeName\":5,\"idx\":0,\"struct\":1}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",Refuse{"unsupported"}},
    Case{"resolver-receives-exact-prenex-instantiation",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":5,\"us\":[5,6]}}\n{\"axiom\":{\"name\":5,\"levelParams\":[1,6],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[6,1],\"type\":2,\"isUnsafe\":false}}\n",PrenexResolver{}},
    Case{"aliased-universe-identity",prefix() ++ "{\"in\":8,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[8],\"type\":1,\"isUnsafe\":false}}\n",Alias{}},
    Case{"aliased-duplicate-universe-identity",prefix() ++ "{\"in\":8,\"str\":{\"pre\":0,\"str\":\"u\"}}\n{\"ie\":1,\"sort\":2}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,8],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"duplicate universe parameters"}},
    Case{"metadata-is-preserved-and-transparent",prefix() ++ "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"mdata\":{\"expr\":1,\"data\":{\"opaque\":{\"array\":[\"x\",true]}}}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n",Metadata{}},
    Case{"natural-literal-in-dependent-type",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":7,\"us\":[]}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"bvar\":0}\n{\"ie\":5,\"natVal\":\"123456789012345678901234567890\"}\n{\"ie\":6,\"app\":{\"fn\":4,\"arg\":5}}\n{\"ie\":7,\"forallE\":{\"name\":3,\"type\":3,\"body\":6,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":7,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":7,\"isUnsafe\":false}}\n",NatResolver{}},
    Case{"application-checks-argument-domain",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"bvar\":0}\n{\"ie\":4,\"natVal\":\"1\"}\n{\"ie\":5,\"app\":{\"fn\":3,\"arg\":4}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":2,\"body\":5,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":6,\"isUnsafe\":false}}\n",Refuse{"kernel"}},
    Case{"shared-type-dag-is-not-expanded",prefix() ++ "{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":5,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n",Shared{}},
    Case{"expression-depth-is-an-explicit-refusal",prefix() ++ "{\"ie\":1,\"sort\":0}\n" ++ expression_chain(1100n,2) ++ "{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1101,\"isUnsafe\":false}}\n",Semantic{"type expression DAG exceeds the depth limit"}},
    Case{"level-depth-is-an-explicit-refusal",prefix() ++ level_chain(1100n,7) ++ "{\"ie\":1,\"sort\":1106}\n{\"axiom\":{\"name\":4,\"levelParams\":[1,6],\"type\":1,\"isUnsafe\":false}}\n",Semantic{"type level DAG exceeds the depth limit"}},
    Case{"lowering-observes-budget",prefix() ++ "{\"ie\":1,\"sort\":0}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n",Budget{}},
    Case{"shared-pi-expansion-has-a-finite-budget",prefix() ++ "{\"ie\":1,\"sort\":0}\n{\"ie\":2,\"forallE\":{\"name\":3,\"type\":1,\"body\":1,\"binderInfo\":\"default\"}}\n{\"ie\":3,\"forallE\":{\"name\":3,\"type\":2,\"body\":2,\"binderInfo\":\"default\"}}\n{\"ie\":4,\"forallE\":{\"name\":3,\"type\":3,\"body\":3,\"binderInfo\":\"default\"}}\n{\"ie\":5,\"forallE\":{\"name\":3,\"type\":4,\"body\":4,\"binderInfo\":\"default\"}}\n{\"ie\":6,\"forallE\":{\"name\":3,\"type\":5,\"body\":5,\"binderInfo\":\"default\"}}\n{\"ie\":7,\"forallE\":{\"name\":3,\"type\":6,\"body\":6,\"binderInfo\":\"default\"}}\n{\"ie\":8,\"forallE\":{\"name\":3,\"type\":7,\"body\":7,\"binderInfo\":\"default\"}}\n{\"ie\":9,\"forallE\":{\"name\":3,\"type\":8,\"body\":8,\"binderInfo\":\"default\"}}\n{\"ie\":10,\"forallE\":{\"name\":3,\"type\":9,\"body\":9,\"binderInfo\":\"default\"}}\n{\"ie\":11,\"forallE\":{\"name\":3,\"type\":10,\"body\":10,\"binderInfo\":\"default\"}}\n{\"ie\":12,\"forallE\":{\"name\":3,\"type\":11,\"body\":11,\"binderInfo\":\"default\"}}\n{\"ie\":13,\"forallE\":{\"name\":3,\"type\":12,\"body\":12,\"binderInfo\":\"default\"}}\n{\"ie\":14,\"forallE\":{\"name\":3,\"type\":13,\"body\":13,\"binderInfo\":\"default\"}}\n{\"ie\":15,\"forallE\":{\"name\":3,\"type\":14,\"body\":14,\"binderInfo\":\"default\"}}\n{\"ie\":16,\"forallE\":{\"name\":3,\"type\":15,\"body\":15,\"binderInfo\":\"default\"}}\n{\"ie\":17,\"forallE\":{\"name\":3,\"type\":16,\"body\":16,\"binderInfo\":\"default\"}}\n{\"ie\":18,\"forallE\":{\"name\":3,\"type\":17,\"body\":17,\"binderInfo\":\"default\"}}\n{\"ie\":19,\"forallE\":{\"name\":3,\"type\":18,\"body\":18,\"binderInfo\":\"default\"}}\n{\"ie\":20,\"forallE\":{\"name\":3,\"type\":19,\"body\":19,\"binderInfo\":\"default\"}}\n{\"ie\":21,\"forallE\":{\"name\":3,\"type\":20,\"body\":20,\"binderInfo\":\"default\"}}\n{\"ie\":22,\"forallE\":{\"name\":3,\"type\":21,\"body\":21,\"binderInfo\":\"default\"}}\n{\"ie\":23,\"forallE\":{\"name\":3,\"type\":22,\"body\":22,\"binderInfo\":\"default\"}}\n{\"ie\":24,\"forallE\":{\"name\":3,\"type\":23,\"body\":23,\"binderInfo\":\"default\"}}\n{\"ie\":25,\"forallE\":{\"name\":3,\"type\":24,\"body\":24,\"binderInfo\":\"default\"}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":25,\"isUnsafe\":false}}\n",SharedBudget{}}]
