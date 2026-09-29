import Base
import ./ndjson.bend as Json

type Hints is Data:
  OpaqueHint{}
  Abbrev{}
  Regular{height: String}

type Safety is Data:
  Unsafe{}
  Safe{}
  Partial{}

type QuotKind is Data:
  QuotType{}
  QuotCtor{}
  QuotLift{}
  QuotInd{}

type Rule is Data:
  Rule{ctor: String, nfields: String, rhs: String}

type DeclKind is Data:
  Axiom{unsafe: Bool}
  Definition{value: String, hint: Hints, safe: Safety, all: List<&2, String>}
  Theorem{value: String, all: List<&2, String>}
  Opaque{value: String, unsafe: Bool, all: List<&2, String>}
  Quotient{kind: QuotKind}
  Inductive{num_params: String, num_indices: String, all: List<&2, String>, ctors: List<&2, String>, num_nested: String, recursive: Bool, unsafe: Bool, reflexive: Bool}
  Constructor{induct: String, cidx: String, num_params: String, num_fields: String, unsafe: Bool}
  Recursor{all: List<&2, String>, num_params: String, num_indices: String, num_motives: String, num_minors: String, rules: List<&2, Rule>, k: Bool, unsafe: Bool}

type T is Data:
  Decl{name: String, level_params: List<&2, String>, typ: String, kind: DeclKind, line: U32}

type Group is Data:
  Group{types: List<&2, T>, ctors: List<&2, T>, recs: List<&2, T>, line: U32}

type Parsed is Data:
  Parsed{declarations: List<&2, T>, group: Maybe<&2, Group>}

def append(-A: Data, xs: List<&2, A>, ys: List<&2, A>) -> List<&2, A>:
  match xs:
    case Nil{}:
      ys
    case Con{h, t}:
      Con{h, append(A, t, ys)}

def safety(text: String) -> Result<&2, &2, String, Safety>:
  match text:
    case "unsafe":
      Done{Unsafe{}}
    case "safe":
      Done{Safe{}}
    case "partial":
      Done{Partial{}}
    case other:
      Fail{"invalid enum " ++ other}

def quot_kind(text: String) -> Result<&2, &2, String, QuotKind>:
  match text:
    case "type":
      Done{QuotType{}}
    case "ctor":
      Done{QuotCtor{}}
    case "lift":
      Done{QuotLift{}}
    case "ind":
      Done{QuotInd{}}
    case other:
      Fail{"invalid enum " ++ other}

def hints_text(text: String) -> Result<&2, &2, String, Hints>:
  match text:
    case "opaque":
      Done{OpaqueHint{}}
    case "abbrev":
      Done{Abbrev{}}
    case other:
      Fail{"invalid enum " ++ other}

def regular_range(order: Cmp, height: String) -> Result<&2, &2, String, Hints>:
  match order:
    case GT{}:
      Fail{"regular reducibility hint exceeds UInt32"}
    case other:
      Done{Regular{height}}

def regular_length(order: Cmp, +height: String) -> Result<&2, &2, String, Hints>:
  match order:
    case LT{}:
      Done{Regular{height}}
    case GT{}:
      Fail{"regular reducibility hint exceeds UInt32"}
    case EQ{}:
      regular_range(Json.lexical(height, "4294967295"), height)

def hints(value: Json.T) -> Result<&2, &2, String, Hints>:
  match value:
    case Json.Text{text}:
      hints_text(text)
    case Json.Object{fields}:
      do Result<&2, &2, String, Hints>:
        exact : List<&2, Json.Field<Json.T>> <- Json.exact_object(Json.Object{fields}, ["regular"])
        +height : String <- Json.nat_field(exact, "regular")
        regular_length(Nat.cmp(Json.string_length(height), 10n), height)
    case other:
      Fail{"invalid reducibility hints"}

def hints_field(fields: List<&2, Json.Field<Json.T>>, key: String) -> Result<&2, &2, String, Hints>:
  do Result<&2, &2, String, Hints>:
    value : Json.T <- Json.field(fields, key)
    hints(value)

def safety_field(fields: List<&2, Json.Field<Json.T>>, key: String) -> Result<&2, &2, String, Safety>:
  do Result<&2, &2, String, Safety>:
    value : String <- Json.string_field(fields, key)
    safety(value)

def quot_field(fields: List<&2, Json.Field<Json.T>>, key: String) -> Result<&2, &2, String, QuotKind>:
  do Result<&2, &2, String, QuotKind>:
    value : String <- Json.string_field(fields, key)
    quot_kind(value)

def rule(value: Json.T) -> Result<&2, &2, String, Rule>:
  do Result<&2, &2, String, Rule>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["ctor", "nfields", "rhs"])
    ctor : String <- Json.nat_field(fields, "ctor")
    nfields : String <- Json.nat_field(fields, "nfields")
    rhs : String <- Json.nat_field(fields, "rhs")
    return Rule{ctor, nfields, rhs}

def rules(values: List<&2, Json.T>) -> Result<&2, &2, String, List<&2, Rule>>:
  match values:
    case Nil{}:
      Done{Nil{}}
    case Con{head, tail}:
      do Result<&2, &2, String, List<&2, Rule>>:
        item : Rule <- rule(head)
        rest : List<&2, Rule> <- rules(tail)
        return Con{item, rest}

def rules_field(fields: List<&2, Json.Field<Json.T>>, key: String) -> Result<&2, &2, String, List<&2, Rule>>:
  do Result<&2, &2, String, List<&2, Rule>>:
    value : Json.T <- Json.field(fields, key)
    values : List<&2, Json.T> <- Json.array(value)
    rules(values)

def base(+fields: List<&2, Json.Field<Json.T>>, kind: DeclKind, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    name : String <- Json.nat_field(fields, "name")
    levels : List<&2, String> <- Json.nats_field(fields, "levelParams")
    typ : String <- Json.nat_field(fields, "type")
    return Decl{name, levels, typ, kind, line}

def axiom(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","isUnsafe"])
    unsafe : Bool <- Json.bool_field(fields, "isUnsafe")
    base(fields, Axiom{unsafe}, line)

def definition(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","value","hints","safety","all"])
    value : String <- Json.nat_field(fields, "value")
    hint : Hints <- hints_field(fields, "hints")
    safe : Safety <- safety_field(fields, "safety")
    all : List<&2, String> <- Json.nats_field(fields, "all")
    base(fields, Definition{value, hint, safe, all}, line)

def theorem(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","value","all"])
    value : String <- Json.nat_field(fields, "value")
    all : List<&2, String> <- Json.nats_field(fields, "all")
    base(fields, Theorem{value, all}, line)

def opaque(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","value","isUnsafe","all"])
    value : String <- Json.nat_field(fields, "value")
    unsafe : Bool <- Json.bool_field(fields, "isUnsafe")
    all : List<&2, String> <- Json.nats_field(fields, "all")
    base(fields, Opaque{value, unsafe, all}, line)

def quotient(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","kind"])
    kind : QuotKind <- quot_field(fields, "kind")
    base(fields, Quotient{kind}, line)

def inductive(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","numParams","numIndices","all","ctors","numNested","isRec","isUnsafe","isReflexive"])
    num_params : String <- Json.nat_field(fields, "numParams")
    num_indices : String <- Json.nat_field(fields, "numIndices")
    all : List<&2, String> <- Json.nats_field(fields, "all")
    ctors : List<&2, String> <- Json.nats_field(fields, "ctors")
    num_nested : String <- Json.nat_field(fields, "numNested")
    recursive : Bool <- Json.bool_field(fields, "isRec")
    unsafe : Bool <- Json.bool_field(fields, "isUnsafe")
    reflexive : Bool <- Json.bool_field(fields, "isReflexive")
    base(fields, Inductive{num_params, num_indices, all, ctors, num_nested, recursive, unsafe, reflexive}, line)

def constructor(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","induct","cidx","numParams","numFields","isUnsafe"])
    induct : String <- Json.nat_field(fields, "induct")
    cidx : String <- Json.nat_field(fields, "cidx")
    num_params : String <- Json.nat_field(fields, "numParams")
    num_fields : String <- Json.nat_field(fields, "numFields")
    unsafe : Bool <- Json.bool_field(fields, "isUnsafe")
    base(fields, Constructor{induct, cidx, num_params, num_fields, unsafe}, line)

def recursor(value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  do Result<&2, &2, String, T>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name","levelParams","type","all","numParams","numIndices","numMotives","numMinors","rules","k","isUnsafe"])
    all : List<&2, String> <- Json.nats_field(fields, "all")
    num_params : String <- Json.nat_field(fields, "numParams")
    num_indices : String <- Json.nat_field(fields, "numIndices")
    num_motives : String <- Json.nat_field(fields, "numMotives")
    num_minors : String <- Json.nat_field(fields, "numMinors")
    rules : List<&2, Rule> <- rules_field(fields, "rules")
    k : Bool <- Json.bool_field(fields, "k")
    unsafe : Bool <- Json.bool_field(fields, "isUnsafe")
    base(fields, Recursor{all, num_params, num_indices, num_motives, num_minors, rules, k, unsafe}, line)

type GroupPart is Data:
  GroupType{}
  GroupCtor{}
  GroupRec{}

def group_item(part: GroupPart, value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  match part:
    case GroupType{}:
      inductive(value, line)
    case GroupCtor{}:
      constructor(value, line)
    case GroupRec{}:
      recursor(value, line)

def group_items(items: List<&2, Json.T>, +part: GroupPart, +line: U32) -> Result<&2, &2, String, List<&2, T>>:
  match items:
    case Nil{}:
      Done{Nil{}}
    case Con{head, tail}:
      do Result<&2, &2, String, List<&2, T>>:
        item : T <- group_item(part, head, line)
        rest : List<&2, T> <- group_items(tail, part, line)
        return Con{item, rest}

def group_field(fields: List<&2, Json.Field<Json.T>>, key: String, part: GroupPart, line: U32) -> Result<&2, &2, String, List<&2, T>>:
  do Result<&2, &2, String, List<&2, T>>:
    raw : Json.T <- Json.field(fields, key)
    items : List<&2, Json.T> <- Json.array(raw)
    group_items(items, part, line)

type Position is Data:
  Position{ctor: String, owner: String, index: String}

def positions(ctors: List<&2, String>, +owner: String, +index: Nat) -> List<&2, Position>:
  match ctors:
    case Nil{}:
      Nil{}
    case Con{ctor, rest}:
      Con{Position{ctor, owner, Nat.show(index)}, positions(rest, owner, 1n+index)}

def type_positions(types: List<&2, T>) -> List<&2, Position>:
  match types:
    case Nil{}:
      Nil{}
    case Con{Decl{name, levels, typ, Inductive{params, indices, all, ctors, nested, rec, unsafe, refl}, line}, rest}:
      append(Position, positions(ctors, name, 0n), type_positions(rest))
    case Con{other, rest}:
      type_positions(rest)

def position_choice(same: Bool, value: Position, rest: Maybe<&2, Position>) -> Maybe<&2, Position>:
  match same:
    case True{}:
      Some{value}
    case False{}:
      rest

def find_position(positions: List<&2, Position>, +name: String) -> Maybe<&2, Position>:
  match positions:
    case Nil{}:
      None{}
    case Con{Position{+ctor, owner, index}, rest}:
      position_choice(Json.str_eq(ctor, name), Position{ctor, owner, index}, find_position(rest, name))

def constructor_matches(valid: Bool, name: String, owner: String) -> Result<&2, &2, String, Unit>:
  match valid:
    case True{}:
      Done{Unit{}}
    case False{}:
      Fail{"constructor " ++ name ++ " disagrees with the ctors list of inductive type " ++ owner}

def constructor_position(found: Maybe<&2, Position>, name: String, induct: String, cidx: String) -> Result<&2, &2, String, Unit>:
  match found:
    case None{}:
      Fail{"constructor " ++ name ++ " is not listed by an inductive type of its group"}
    case Some{Position{ctor, +owner, index}}:
      constructor_matches(Bool.and(Json.str_eq(induct, owner), Json.str_eq(cidx, index)), name, owner)

def check_constructor(value: T, positions: List<&2, Position>) -> Result<&2, &2, String, Unit>:
  match value:
    case Decl{+name, levels, typ, Constructor{induct, cidx, params, fields, unsafe}, line}:
      constructor_position(find_position(positions, name), name, induct, cidx)
    case other:
      Done{Unit{}}

def check_constructors(values: List<&2, T>, +positions: List<&2, Position>) -> Result<&2, &2, String, Unit>:
  match values:
    case Nil{}:
      Done{Unit{}}
    case Con{head, tail}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- check_constructor(head, positions)
        check_constructors(tail, positions)

def member_choice(same: Bool, kind: DeclKind, rest: Maybe<&2, DeclKind>) -> Maybe<&2, DeclKind>:
  match same:
    case True{}:
      Some{kind}
    case False{}:
      rest

def member(values: List<&2, T>, +name: String) -> Maybe<&2, DeclKind>:
  match values:
    case Nil{}:
      None{}
    case Con{Decl{key, levels, typ, kind, line}, tail}:
      member_choice(Json.str_eq(key, name), kind, member(tail, name))

def rule_matches(valid: Bool, name: String, expected: String, actual: String) -> Result<&2, &2, String, Unit>:
  match valid:
    case True{}:
      Done{Unit{}}
    case False{}:
      Fail{"recursor rule for constructor " ++ name ++ " declares " ++ expected
        ++ " fields but the constructor has " ++ actual}

def rule_member(found: Maybe<&2, DeclKind>, name: String, +nfields: String) -> Result<&2, &2, String, Unit>:
  match found:
    case None{}:
      Done{Unit{}}
    case Some{Constructor{induct, cidx, params, +fields, unsafe}}:
      rule_matches(Json.str_eq(fields, nfields), name, nfields, fields)
    case Some{other}:
      Fail{"recursor rule names " ++ name ++ ", a declaration of its group that is not a constructor"}

def check_rule(rule: Rule, members: List<&2, T>) -> Result<&2, &2, String, Unit>:
  match rule:
    case Rule{+name, nfields, rhs}:
      rule_member(member(members, name), name, nfields)

def check_rules(rules: List<&2, Rule>, +members: List<&2, T>) -> Result<&2, &2, String, Unit>:
  match rules:
    case Nil{}:
      Done{Unit{}}
    case Con{head, tail}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- check_rule(head, members)
        check_rules(tail, members)

def recursor_rules(recs: List<&2, T>) -> List<&2, Rule>:
  match recs:
    case Nil{}:
      Nil{}
    case Con{Decl{name, levels, typ, Recursor{all, params, indices, motives, minors, rules, k, unsafe}, line}, rest}:
      append(Rule, rules, recursor_rules(rest))
    case Con{other, rest}:
      recursor_rules(rest)

def group_checks(+types: List<&2, T>, +ctors: List<&2, T>, +recs: List<&2, T>) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- check_constructors(ctors, type_positions(types))
    check_rules(recursor_rules(recs), append(T, types, append(T, ctors, recs)))

def ordinary(tag: String, value: Json.T, line: U32) -> Result<&2, &2, String, T>:
  match tag:
    case "axiom":
      axiom(value, line)
    case "def":
      definition(value, line)
    case "thm":
      theorem(value, line)
    case "opaque":
      opaque(value, line)
    case "quot":
      quotient(value, line)
    case unknown:
      Fail{"unknown declaration tag " ++ unknown}

def parse(tag: String, value: Json.T, +line: U32) -> Result<&2, &2, String, Parsed>:
  match tag:
    case "inductive":
      do Result<&2, &2, String, Parsed>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["types", "ctors", "recs"])
        +types : List<&2, T> <- group_field(fields, "types", GroupType{}, line)
        +ctors : List<&2, T> <- group_field(fields, "ctors", GroupCtor{}, line)
        +recs : List<&2, T> <- group_field(fields, "recs", GroupRec{}, line)
        checked : Unit <- group_checks(types, ctors, recs)
        return Parsed{append(T, types, append(T, ctors, recs)), Some{Group{types, ctors, recs, line}}}
    case other:
      do Result<&2, &2, String, Parsed>:
        declaration : T <- ordinary(other, value, line)
        return Parsed{[declaration], None{}}

def rule_names(rules: List<&2, Rule>) -> List<&2, String>:
  match rules:
    case Nil{}:
      Nil{}
    case Con{Rule{ctor, fields, rhs}, rest}:
      Con{ctor, rule_names(rest)}

def rule_exprs(rules: List<&2, Rule>) -> List<&2, String>:
  match rules:
    case Nil{}:
      Nil{}
    case Con{Rule{ctor, fields, rhs}, rest}:
      Con{rhs, rule_exprs(rest)}

def extra_names(kind: DeclKind) -> List<&2, String>:
  match kind:
    case Definition{value, hints, safety, all}:
      all
    case Theorem{value, all}:
      all
    case Opaque{value, unsafe, all}:
      all
    case Inductive{params, indices, all, ctors, nested, rec, unsafe, refl}:
      append(String, all, ctors)
    case Constructor{induct, cidx, params, fields, unsafe}:
      [induct]
    case Recursor{all, params, indices, motives, minors, rules, k, unsafe}:
      append(String, all, rule_names(rules))
    case other:
      Nil{}

def name_references(declaration: T) -> List<&2, String>:
  match declaration:
    case Decl{name, levels, typ, kind, line}:
      Con{name, append(String, levels, extra_names(kind))}

def extra_exprs(kind: DeclKind) -> List<&2, String>:
  match kind:
    case Definition{value, hints, safety, all}:
      [value]
    case Theorem{value, all}:
      [value]
    case Opaque{value, unsafe, all}:
      [value]
    case Recursor{all, params, indices, motives, minors, rules, k, unsafe}:
      rule_exprs(rules)
    case other:
      Nil{}

def expr_references(declaration: T) -> List<&2, String>:
  match declaration:
    case Decl{name, levels, typ, kind, line}:
      Con{typ, extra_exprs(kind)}

def kind_name(kind: DeclKind) -> String:
  match kind:
    case Axiom{unsafe}:
      "axiom"
    case Definition{value, hints, safety, all}:
      "def"
    case Theorem{value, all}:
      "thm"
    case Opaque{value, unsafe, all}:
      "opaque"
    case Quotient{kind}:
      "quot"
    case Inductive{params, indices, all, ctors, nested, rec, unsafe, refl}:
      "inductive"
    case Constructor{induct, cidx, params, fields, unsafe}:
      "constructor"
    case Recursor{all, params, indices, motives, minors, rules, k, unsafe}:
      "recursor"
