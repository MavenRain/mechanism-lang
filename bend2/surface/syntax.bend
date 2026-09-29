import Base
import ../kernel/bignum.bend as Bignum
import ../kernel/quantity.bend as Quantity
import ../kernel/common.bend as Common
import ./universe.bend as Universe

type Prim is Data:
  PAdd{}
  PSub{}
  PMul{}
  PEq{}
  PLt{}

type Binder<-A: Data> is Data:
  Binder{quantity: Quantity.T, name: String, ty: A}

type Motive<-A: Data> is Data:
  Motive{self: String, ind: Maybe<&2, String>, indices: List<&2, String>, body: A}

type Field<-A: Data> is Data:
  Field{quantity: Quantity.T, name: String, ty: Maybe<&2, A>}

type Branch<-A: Data> is Data:
  BrLeg{index: Bignum.T, binders: List<&2, Binder<A>>, body: A}
  BrCtor{name: String, fields: List<&2, Field<A>>, body: A}

type FamCtor<-A: Data> is Data:
  FamCtor{name: String, ty: A}

type Fam<-A: Data> is Data:
  Fam{name: String, params: List<&2, Binder<A>>, ty: A, ctors: List<&2, FamCtor<A>>}

type RecDef<-A: Data> is Data:
  RecDef{name: String, ty: A, body: A}

type T is Data:
  SVar{name: String}
  SNat{value: Bignum.T}
  SProp{}
  SType{level: Bignum.T}
  SSort{level: Universe.T}
  SPrim{prim: Prim}
  SUnit{}
  SAuto{}
  SPair{left: T, right: T}
  STuple{items: List<&2, T>}
  SSum{items: List<&2, T>}
  SProd{items: List<&2, T>}
  SProj{head: T, index: Bignum.T}
  SInj{index: Bignum.T, count: Bignum.T, value: T}
  SAbsurd{value: T}
  SApp{head: T, arg: T}
  SFun{binders: List<&2, Binder<T>>, body: T}
  SArrow{binder: Binder<T>, body: T}
  SStar{binder: Binder<T>, body: T}
  SZkTy{binder: Binder<T>, body: T}
  SProve{key: T, witness: T, relation: T}
  SVerify{key: T, proof: T}
  SFhcTy{level: T, ty: T}
  SEnc{key: T, value: T}
  SEval{function: T, value: T}
  SDec{key: T, value: T}
  SMpcTy{parties: T, access: T, ty: T}
  SShare{value: T}
  SInput{party: T, value: T}
  SJoin{parties: T, function: T, values: List<&2, T>}
  SOpen{quantity: Maybe<&2, Quantity.T>, parties: T, proof: T, value: T}
  SLet{name: String, ty: T, value: T, body: T}
  SAnn{value: T, ty: T}
  SCase{scrut: T, motive: Maybe<&2, Motive<T>>, branches: List<&2, Branch<T>>}
  SMatch{scrut: T, motive: Maybe<&2, Motive<T>>, branches: List<&2, Branch<T>>}

type Specialization is Data:
  Specialization{template: String, levels: List<&2, Universe.T>, name: String,
    names: List<&2, Common.Pair<String, String>>,
    helpers: Maybe<&2, List<&2, Common.Pair<String, String>>>}

type Decl is Data:
  DPoly{universes: Nat, name: String, ty: T, body: T}
  DPolyMu{universes: Nat, family: Fam<T>}
  DPolyGroup{universes: Nat, family: Fam<T>, families: List<&2, Fam<T>>, helpers: List<&2, RecDef<T>>}
  DPolyCompose{universes: Nat, name: String, specializations: List<&2, Specialization>, helpers: List<&2, RecDef<T>>}
  DSpecialize{specialization: Specialization}
  DDef{name: String, ty: T, body: T}
  DAxiom{name: String, ty: T}
  DMu{families: List<&2, Fam<T>>}
  DRec{definitions: List<&2, RecDef<T>>}

def prim_name(prim: Prim) -> String:
  match prim:
    case PAdd{}:
      "natAdd"
    case PSub{}:
      "natSub"
    case PMul{}:
      "natMul"
    case PEq{}:
      "natEq"
    case PLt{}:
      "natLt"

# Printing uses explicit parentheses to preserve precedence and branch scope.
law raw:
  for term: T
  String

def mark(quantity: Quantity.T) -> String:
  match quantity:
    case Quantity.QZero{}:
      "0 "
    case Quantity.QOne{}:
      "1 "
    case Quantity.QMany{}:
      ""

@unsafe
def binder_text(binder: Binder<T>) -> String:
  match binder:
    case Binder{quantity, name, ty}:
      "(" ++ mark(quantity) ++ name ++ " : " ++ raw(ty) ++ ")"

@unsafe
def binders_text(binders: List<&2, Binder<T>>) -> String:
  match binders:
    case Nil{}:
      ""
    case Con{binder, rest}:
      " " ++ binder_text(binder) ++ binders_text(rest)

@unsafe
def fields_text(fields: List<&2, Field<T>>) -> String:
  match fields:
    case Nil{}:
      ""
    case Con{Field{quantity, name, None{}}, rest}:
      " " ++ mark(quantity) ++ name ++ fields_text(rest)
    case Con{Field{quantity, name, Some{ty}}, rest}:
      " " ++ binder_text(Binder{quantity, name, ty}) ++ fields_text(rest)

def names_text(names: List<&2, String>) -> String:
  match names:
    case Nil{}:
      ""
    case Con{name, rest}:
      " " ++ name ++ names_text(rest)

def index_text(ind: Maybe<&2, String>, indices: List<&2, String>) -> String:
  match ind:
    case None{}:
      ""
    case Some{name}:
      " in " ++ name ++ names_text(indices)

@unsafe
def motive_text(motive: Maybe<&2, Motive<T>>) -> String:
  match motive:
    case None{}:
      ""
    case Some{Motive{self, ind, indices, body}}:
      " as " ++ self ++ index_text(ind, indices) ++ " return " ++ raw(body)

@unsafe
def branches_text(branches: List<&2, Branch<T>>) -> String:
  match branches:
    case Nil{}:
      ""
    case Con{BrLeg{index, binders, body}, rest}:
      " | " ++ Bignum.to_string(index) ++ binders_text(binders) ++ " => " ++ raw(body) ++ branches_text(rest)
    case Con{BrCtor{name, fields, body}, rest}:
      " | " ++ name ++ fields_text(fields) ++ " => " ++ raw(body) ++ branches_text(rest)

@unsafe
def terms_text(terms: List<&2, T>, +separator: String) -> String:
  match terms:
    case Nil{}:
      ""
    case Con{term, Nil{}}:
      raw(term)
    case Con{term, Con{head, tail}}:
      raw(term) ++ separator ++ terms_text(head <> tail, separator)

def optional_mark(quantity: Maybe<&2, Quantity.T>) -> String:
  match quantity:
    case None{}:
      ""
    case Some{q}:
      mark(q)

@unsafe
def raw(term):
  match term:
    case SVar{name}:
      name
    case SNat{value}:
      Bignum.to_string(value)
    case SProp{}:
      "Prop"
    case SType{level}:
      "Type " ++ Bignum.to_string(level)
    case SSort{level}:
      "Sort " ++ Universe.text(level)
    case SPrim{prim}:
      prim_name(prim)
    case SUnit{}:
      "()"
    case SAuto{}:
      "auto"
    case SPair{left, right}:
      "(" ++ raw(left) ++ ", " ++ raw(right) ++ ")"
    case SProj{head, index}:
      "(" ++ raw(head) ++ ")." ++ Bignum.to_string(index)
    case SInj{index, count, value}:
      "(inj " ++ Bignum.to_string(index) ++ " of " ++ Bignum.to_string(count) ++ " " ++ raw(value) ++ ")"
    case SAbsurd{value}:
      "(absurd " ++ raw(value) ++ ")"
    case SApp{head, arg}:
      "(" ++ raw(head) ++ " " ++ raw(arg) ++ ")"
    case SFun{binders, body}:
      "(fun" ++ binders_text(binders) ++ " => " ++ raw(body) ++ ")"
    case SArrow{binder, body}:
      "(" ++ binder_text(binder) ++ " -> " ++ raw(body) ++ ")"
    case SStar{binder, body}:
      "(" ++ binder_text(binder) ++ " * " ++ raw(body) ++ ")"
    case SZkTy{binder, body}:
      "(zk " ++ binder_text(binder) ++ " * " ++ raw(body) ++ ")"
    case SMpcTy{parties, access, ty}:
      "(mpc[" ++ raw(parties) ++ ", " ++ raw(access) ++ "] " ++ raw(ty) ++ ")"
    case SJoin{parties, function, values}:
      "(mpc " ++ raw(parties) ++ " " ++ raw(function) ++ " " ++ terms_text(values, " ") ++ ")"
    case SOpen{quantity, parties, proof, value}:
      "(open " ++ raw(parties) ++ " " ++ optional_mark(quantity) ++ raw(proof) ++ " " ++ raw(value) ++ ")"
    case SLet{name, ty, value, body}:
      "(let " ++ name ++ " : " ++ raw(ty) ++ " := " ++ raw(value) ++ " in " ++ raw(body) ++ ")"
    case SAnn{value, ty}:
      "(" ++ raw(value) ++ " : " ++ raw(ty) ++ ")"
    case STuple{items}:
      "tuple (" ++ terms_text(items, ", ") ++ ")"
    case SSum{items}:
      "sum (" ++ terms_text(items, ", ") ++ ")"
    case SProd{items}:
      "prod (" ++ terms_text(items, ", ") ++ ")"
    case SProve{key, witness, relation}:
      "(prove " ++ raw(key) ++ " " ++ raw(witness) ++ " " ++ raw(relation) ++ ")"
    case SVerify{key, proof}:
      "(verify " ++ raw(key) ++ " " ++ raw(proof) ++ ")"
    case SFhcTy{level, ty}:
      "(fhc " ++ raw(level) ++ " " ++ raw(ty) ++ ")"
    case SEnc{key, value}:
      "(enc " ++ raw(key) ++ " " ++ raw(value) ++ ")"
    case SEval{function, value}:
      "(eval " ++ raw(function) ++ " " ++ raw(value) ++ ")"
    case SDec{key, value}:
      "(dec " ++ raw(key) ++ " " ++ raw(value) ++ ")"
    case SShare{value}:
      "(share " ++ raw(value) ++ ")"
    case SInput{party, value}:
      "(input " ++ raw(party) ++ " " ++ raw(value) ++ ")"
    case SCase{scrut, motive, branches}:
      "(case " ++ raw(scrut) ++ motive_text(motive) ++ " with" ++ branches_text(branches) ++ ")"
    case SMatch{scrut, motive, branches}:
      "(match " ++ raw(scrut) ++ motive_text(motive) ++ " with" ++ branches_text(branches) ++ ")"

def show(term: T) -> String:
  raw(term)

def fam_ctor_text(ctor: FamCtor<T>) -> String:
  match ctor:
    case FamCtor{name, ty}:
      "| " ++ name ++ " : " ++ raw(ty) ++ "\n"

def fam_ctors_text(ctors: List<&2, FamCtor<T>>) -> String:
  match ctors:
    case Nil{}:
      ""
    case Con{ctor, tail}:
      fam_ctor_text(ctor) ++ fam_ctors_text(tail)

def fam_text(word: String, family: Fam<T>) -> String:
  match family:
    case Fam{name, params, ty, ctors}:
      word ++ " " ++ name ++ binders_text(params) ++ " : " ++ raw(ty) ++ " :=\n" ++ fam_ctors_text(ctors)

def families_text(families: List<&2, Fam<T>>, +word: String) -> String:
  match families:
    case Nil{}:
      ""
    case Con{family, tail}:
      fam_text(word, family) ++ families_text(tail, word)

def bindings_text(bindings: List<&2, Common.Pair<String, String>>) -> String:
  match bindings:
    case Nil{}:
      ""
    case Con{Common.Pair{local, target}, Nil{}}:
      local ++ " := " ++ target
    case Con{Common.Pair{local, target}, Con{head, tail}}:
      local ++ " := " ++ target ++ ", " ++ bindings_text(head <> tail)

def reuse_text(bindings: List<&2, Common.Pair<String, String>>) -> String:
  match bindings:
    case Nil{}:
      ""
    case Con{head, tail}:
      " with (" ++ bindings_text(head <> tail) ++ ")"

def exports_text(exports: Maybe<&2, List<&2, Common.Pair<String, String>>>) -> String:
  match exports:
    case None{}:
      ""
    case Some{bindings}:
      " export (" ++ bindings_text(bindings) ++ ")"

def universe_names(count: Nat, +index: Nat) -> String:
  match count:
    case 0n:
      ""
    case 1n+0n:
      "u" ++ Nat.show(index)
    case 1n+1n+rest:
      "u" ++ Nat.show(index) ++ ", " ++ universe_names(1n+rest, 1n+index)

def universe_arguments(levels: List<&2, Universe.T>) -> String:
  match levels:
    case Nil{}:
      ""
    case Con{level, Nil{}}:
      Universe.text(level)
    case Con{level, Con{head, tail}}:
      Universe.text(level) ++ ", " ++ universe_arguments(head <> tail)

def specialization_text(specialization: Specialization) -> String:
  match specialization:
    case Specialization{template, levels, name, names, helpers}:
      "specialize " ++ template ++ " (" ++ universe_arguments(levels) ++ ") as " ++ name ++ reuse_text(names) ++ exports_text(helpers) ++ "\n"

def specializations_text(specializations: List<&2, Specialization>) -> String:
  match specializations:
    case Nil{}:
      ""
    case Con{specialization, tail}:
      specialization_text(specialization) ++ specializations_text(tail)

def rec_defs_text(definitions: List<&2, RecDef<T>>, word: String, +next: String) -> String:
  match definitions:
    case Nil{}:
      ""
    case Con{RecDef{name, ty, body}, tail}:
      word ++ " " ++ name ++ " : " ++ raw(ty) ++ " := " ++ raw(body) ++ "\n" ++ rec_defs_text(tail, next, next)

def members_text(members: List<&2, RecDef<T>>) -> String:
  match members:
    case Nil{}:
      ""
    case Con{head, tail}:
      "where\n" ++ rec_defs_text(head <> tail, "def", "def") ++ "end\n"

def decl_text(decl: Decl) -> String:
  match decl:
    case DPoly{arity, name, ty, body}:
      "poly (" ++ universe_names(arity, 0n) ++ ") def " ++ name ++ " : " ++ raw(ty) ++ " := " ++ raw(body) ++ "\n"
    case DPolyMu{arity, family}:
      "poly (" ++ universe_names(arity, 0n) ++ ") " ++ fam_text("mu", family)
    case DPolyGroup{arity, family, companions, members}:
      "poly (" ++ universe_names(arity, 0n) ++ ") " ++ fam_text("mu", family) ++ families_text(companions, "and") ++ members_text(members)
    case DPolyCompose{arity, name, dependencies, members}:
      "poly (" ++ universe_names(arity, 0n) ++ ") group " ++ name ++ " where\n" ++ specializations_text(dependencies) ++ rec_defs_text(members, "def", "def") ++ "end\n"
    case DSpecialize{specialization}:
      specialization_text(specialization)
    case DDef{name, ty, body}:
      "def " ++ name ++ " : " ++ raw(ty) ++ " := " ++ raw(body) ++ "\n"
    case DAxiom{name, ty}:
      "axiom " ++ name ++ " : " ++ raw(ty) ++ "\n"
    case DMu{Nil{}}:
      ""
    case DMu{Con{family, Nil{}}}:
      fam_text("mu", family)
    case DMu{Con{first, Con{second, rest}}}:
      "mutual\n" ++ families_text(first <> second <> rest, "mu") ++ "end\n"
    case DRec{definitions}:
      rec_defs_text(definitions, "def rec", "and")

def print(decls: List<&2, Decl>) -> String:
  match decls:
    case Nil{}:
      ""
    case Con{decl, rest}:
      decl_text(decl) ++ print(rest)

# Original Syntax.at accepts a signed OCaml int. Big preserves that domain,
# including negative precedence and values beyond the three grammar levels.
def level_of(term: T) -> Nat:
  match term:
    case SVar{_}: 2n
    case SNat{_}: 2n
    case SProp{}: 2n
    case SType{_}: 2n
    case SSort{_}: 2n
    case SPrim{_}: 2n
    case SUnit{}: 2n
    case SAuto{}: 2n
    case SPair{_, _}: 2n
    case STuple{_}: 2n
    case SSum{_}: 2n
    case SProd{_}: 2n
    case SProj{_, _}: 2n
    case SAnn{_, _}: 2n
    case SInj{_, _, _}: 1n
    case SAbsurd{_}: 1n
    case SApp{_, _}: 1n
    case SFun{_, _}: 0n
    case SArrow{_, _}: 0n
    case SStar{_, _}: 0n
    case SZkTy{_, _}: 0n
    case SProve{_, _, _}: 1n
    case SVerify{_, _}: 1n
    case SFhcTy{_, _}: 1n
    case SEnc{_, _}: 1n
    case SEval{_, _}: 1n
    case SDec{_, _}: 1n
    case SMpcTy{_, _, _}: 1n
    case SShare{_}: 1n
    case SInput{_, _}: 1n
    case SJoin{_, _, _}: 1n
    case SOpen{_, _, _, _}: 1n
    case SLet{_, _, _, _}: 0n
    case SCase{_, _, _}: 0n
    case SMatch{_, _, _}: 0n

law at:
  for level: Bignum.T
  for term: T
  String

@unsafe
def precedence_at_level(level: Nat, term: T) -> String:
  at(Bignum.of_nat(level), term)

@unsafe
def precedence_binder_text(binder: Binder<T>) -> String:
  match binder:
    case Binder{quantity, name, ty}:
      "(" ++ mark(quantity) ++ name ++ " : " ++ precedence_at_level(0n, ty) ++ ")"

@unsafe
def precedence_binders_text(binders: List<&2, Binder<T>>) -> String:
  match binders:
    case Nil{}: ""
    case Con{binder, Nil{}}: precedence_binder_text(binder)
    case Con{binder, Con{head, tail}}:
      precedence_binder_text(binder) ++ " " ++ precedence_binders_text(head <> tail)

@unsafe
def precedence_branch_binders(binders: List<&2, Binder<T>>) -> String:
  match binders:
    case Nil{}: ""
    case Con{binder, tail}: " " ++ precedence_binder_text(binder) ++ precedence_branch_binders(tail)

@unsafe
def precedence_field_text(field: Field<T>) -> String:
  match field:
    case Field{quantity, name, None{}}: " " ++ mark(quantity) ++ name
    case Field{quantity, name, Some{ty}}:
      " " ++ precedence_binder_text(Binder{quantity, name, ty})

@unsafe
def precedence_fields_text(fields: List<&2, Field<T>>) -> String:
  match fields:
    case Nil{}: ""
    case Con{field, tail}: precedence_field_text(field) ++ precedence_fields_text(tail)

def precedence_ind_text(ind: Maybe<&2, String>, indices: List<&2, String>) -> String:
  match ind:
    case None{}: ""
    case Some{name}: " in " ++ name ++ names_text(indices)

@unsafe
def precedence_motive_text(motive: Motive<T>) -> String:
  match motive:
    case Motive{self, ind, indices, body}:
      " as " ++ self ++ precedence_ind_text(ind, indices) ++ " return " ++ precedence_at_level(1n, body)

@unsafe
def precedence_optional_motive(motive: Maybe<&2, Motive<T>>) -> String:
  match motive:
    case None{}: ""
    case Some{value}: precedence_motive_text(value)

@unsafe
def precedence_branch_text(branch: Branch<T>) -> String:
  match branch:
    case BrLeg{index, binders, body}:
      " | " ++ Bignum.to_string(index) ++ precedence_branch_binders(binders) ++ " => " ++ precedence_at_level(1n, body)
    case BrCtor{name, fields, body}:
      " | " ++ name ++ precedence_fields_text(fields) ++ " => " ++ precedence_at_level(1n, body)

@unsafe
def precedence_branches_text(branches: List<&2, Branch<T>>) -> String:
  match branches:
    case Nil{}: ""
    case Con{branch, tail}: precedence_branch_text(branch) ++ precedence_branches_text(tail)

@unsafe
def precedence_terms_text(terms: List<&2, T>) -> String:
  match terms:
    case Nil{}: ""
    case Con{term, Nil{}}: precedence_at_level(0n, term)
    case Con{term, Con{head, tail}}:
      precedence_at_level(0n, term) ++ ", " ++ precedence_terms_text(head <> tail)

@unsafe
def precedence_join_values(terms: List<&2, T>) -> String:
  match terms:
    case Nil{}: ""
    case Con{term, tail}: " " ++ precedence_at_level(2n, term) ++ precedence_join_values(tail)

@unsafe
def precedence_raw(term: T) -> String:
  match term:
    case SVar{name}: name
    case SNat{value}: Bignum.to_string(value)
    case SProp{}: "Prop"
    case SType{level}: "Type " ++ Bignum.to_string(level)
    case SSort{level}: "Sort " ++ Universe.text(level)
    case SPrim{prim}: prim_name(prim)
    case SUnit{}: "()"
    case SAuto{}: "auto"
    case SPair{left, right}: "(" ++ precedence_at_level(0n, left) ++ ", " ++ precedence_at_level(0n, right) ++ ")"
    case STuple{items}: "tuple (" ++ precedence_terms_text(items) ++ ")"
    case SSum{items}: "sum (" ++ precedence_terms_text(items) ++ ")"
    case SProd{items}: "prod (" ++ precedence_terms_text(items) ++ ")"
    case SProj{head, index}: precedence_at_level(2n, head) ++ "." ++ Bignum.to_string(index)
    case SInj{index, count, value}:
      "inj " ++ Bignum.to_string(index) ++ " of " ++ Bignum.to_string(count) ++ " " ++ precedence_at_level(1n, value)
    case SAbsurd{value}: "absurd " ++ precedence_at_level(1n, value)
    case SApp{head, arg}: precedence_at_level(1n, head) ++ " " ++ precedence_at_level(2n, arg)
    case SFun{binders, body}: "fun " ++ precedence_binders_text(binders) ++ " => " ++ precedence_at_level(0n, body)
    case SArrow{binder, body}: precedence_binder_text(binder) ++ " -> " ++ precedence_at_level(0n, body)
    case SStar{binder, body}: precedence_binder_text(binder) ++ " * " ++ precedence_at_level(0n, body)
    case SZkTy{binder, body}: "zk " ++ precedence_binder_text(binder) ++ " * " ++ precedence_at_level(0n, body)
    case SProve{key, witness, relation}:
      "prove " ++ precedence_at_level(2n, key) ++ " " ++ precedence_at_level(2n, witness) ++ " " ++ precedence_at_level(2n, relation)
    case SVerify{key, proof}: "verify " ++ precedence_at_level(2n, key) ++ " " ++ precedence_at_level(2n, proof)
    case SFhcTy{level, ty}: "fhc " ++ precedence_at_level(2n, level) ++ " " ++ precedence_at_level(2n, ty)
    case SEnc{key, value}: "enc " ++ precedence_at_level(2n, key) ++ " " ++ precedence_at_level(2n, value)
    case SEval{function, value}: "eval " ++ precedence_at_level(2n, function) ++ " " ++ precedence_at_level(2n, value)
    case SDec{key, value}: "dec " ++ precedence_at_level(2n, key) ++ " " ++ precedence_at_level(2n, value)
    case SMpcTy{parties, access, ty}:
      "mpc[" ++ precedence_at_level(0n, parties) ++ ", " ++ precedence_at_level(0n, access) ++ "] " ++ precedence_at_level(2n, ty)
    case SShare{value}: "share " ++ precedence_at_level(2n, value)
    case SInput{party, value}: "input " ++ precedence_at_level(2n, party) ++ " " ++ precedence_at_level(2n, value)
    case SJoin{parties, function, values}:
      "mpc " ++ precedence_at_level(2n, parties) ++ " " ++ precedence_at_level(2n, function) ++ precedence_join_values(values)
    case SOpen{quantity, parties, proof, value}:
      "open " ++ precedence_at_level(2n, parties) ++ " " ++ optional_mark(quantity) ++ precedence_at_level(2n, proof) ++ " " ++ precedence_at_level(2n, value)
    case SLet{name, ty, value, body}:
      "let " ++ name ++ " : " ++ precedence_at_level(1n, ty) ++ " := " ++ precedence_at_level(1n, value) ++ " in " ++ precedence_at_level(0n, body)
    case SAnn{value, ty}: "(" ++ precedence_at_level(0n, value) ++ " : " ++ precedence_at_level(0n, ty) ++ ")"
    case SCase{scrut, motive, branches}:
      "case " ++ precedence_at_level(1n, scrut) ++ precedence_optional_motive(motive) ++ " with" ++ precedence_branches_text(branches)
    case SMatch{scrut, motive, branches}:
      "match " ++ precedence_at_level(1n, scrut) ++ precedence_optional_motive(motive) ++ " with" ++ precedence_branches_text(branches)

def precedence_wrap(enough: Bool, text: String) -> String:
  match enough:
    case True{}: text
    case False{}: "(" ++ text ++ ")"

@unsafe
def at(level, term):
  +term : T = term
  precedence_wrap(Bignum.le(level, Bignum.of_nat(level_of(term))), precedence_raw(term))
