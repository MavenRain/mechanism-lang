import Base
import ./ndjson.bend as Json

type BinderInfo is Data:
  Default{}
  Implicit{}
  StrictImplicit{}
  InstImplicit{}

type Binder is Data:
  Binder{name: String, typ: String, body: String, info: BinderInfo}

type T is Data:
  Bvar{index: String}
  Sort{index: String}
  Const{name: String, universes: List<&2, String>}
  Apply{fn: String, arg: String}
  Lam{binder: Binder}
  Forall{binder: Binder}
  Let{name: String, typ: String, value: String, body: String, nondep: Bool}
  Proj{type_name: String, index: String, structure: String}
  NatLiteral{text: String}
  StringLiteral{text: String}
  Mdata{expr: String, data: List<&2, Json.Field<Json.T>>}

def binder_info(text: String) -> Result<&2, &2, String, BinderInfo>:
  match text:
    case "default":
      Done{Default{}}
    case "implicit":
      Done{Implicit{}}
    case "strictImplicit":
      Done{StrictImplicit{}}
    case "instImplicit":
      Done{InstImplicit{}}
    case other:
      Fail{"invalid binderInfo " ++ other}

def binder(value: Json.T) -> Result<&2, &2, String, Binder>:
  do Result<&2, &2, String, Binder>:
    +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name", "type", "body", "binderInfo"])
    name : String <- Json.nat_field(fields, "name")
    typ : String <- Json.nat_field(fields, "type")
    body : String <- Json.nat_field(fields, "body")
    text : String <- Json.string_field(fields, "binderInfo")
    info : BinderInfo <- binder_info(text)
    return Binder{name, typ, body, info}

def natural(valid: Bool, text: String) -> Result<&2, &2, String, T>:
  match valid:
    case True{}:
      Done{NatLiteral{text}}
    case False{}:
      Fail{"invalid natural literal"}

def natural_text(text: String) -> Result<&2, &2, String, T>:
  match text:
    case SNil{}:
      Fail{"invalid natural literal"}
    case SCon{+ch, +rest}:
      natural(Json.decimal_digits(SCon{ch, rest}), SCon{ch, rest})

def parse(tag: String, value: Json.T) -> Result<&2, &2, String, T>:
  match tag:
    case "bvar":
      do Result<&2, &2, String, T>:
        index : String <- Json.nat(value)
        return Bvar{index}
    case "sort":
      do Result<&2, &2, String, T>:
        index : String <- Json.nat(value)
        return Sort{index}
    case "const":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name", "us"])
        name : String <- Json.nat_field(fields, "name")
        universes : List<&2, String> <- Json.nats_field(fields, "us")
        return Const{name, universes}
    case "app":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["fn", "arg"])
        fn : String <- Json.nat_field(fields, "fn")
        arg : String <- Json.nat_field(fields, "arg")
        return Apply{fn, arg}
    case "lam":
      do Result<&2, &2, String, T>:
        bound : Binder <- binder(value)
        return Lam{bound}
    case "forallE":
      do Result<&2, &2, String, T>:
        bound : Binder <- binder(value)
        return Forall{bound}
    case "letE":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["name", "type", "value", "body", "nondep"])
        name : String <- Json.nat_field(fields, "name")
        typ : String <- Json.nat_field(fields, "type")
        val : String <- Json.nat_field(fields, "value")
        body : String <- Json.nat_field(fields, "body")
        nondep : Bool <- Json.bool_field(fields, "nondep")
        return Let{name, typ, val, body, nondep}
    case "proj":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["typeName", "idx", "struct"])
        type_name : String <- Json.nat_field(fields, "typeName")
        index : String <- Json.nat_field(fields, "idx")
        structure : String <- Json.nat_field(fields, "struct")
        return Proj{type_name, index, structure}
    case "natVal":
      do Result<&2, &2, String, T>:
        text : String <- Json.string(value)
        natural_text(text)
    case "strVal":
      do Result<&2, &2, String, T>:
        text : String <- Json.string(value)
        return StringLiteral{text}
    case "mdata":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["expr", "data"])
        expr : String <- Json.nat_field(fields, "expr")
        raw : Json.T <- Json.field(fields, "data")
        data : List<&2, Json.Field<Json.T>> <- Json.object_fields(raw)
        return Mdata{expr, data}
    case unknown:
      Fail{"unknown expression tag " ++ unknown}

def name_references(value: T) -> List<&2, String>:
  match value:
    case Const{name, universes}:
      [name]
    case Lam{Binder{name, typ, body, info}}:
      [name]
    case Forall{Binder{name, typ, body, info}}:
      [name]
    case Let{name, typ, val, body, nondep}:
      [name]
    case Proj{type_name, index, structure}:
      [type_name]
    case other:
      Nil{}

def level_references(value: T) -> List<&2, String>:
  match value:
    case Sort{index}:
      [index]
    case Const{name, universes}:
      universes
    case other:
      Nil{}

def expr_references(value: T) -> List<&2, String>:
  match value:
    case Apply{fn, arg}:
      [fn, arg]
    case Lam{Binder{name, typ, body, info}}:
      [typ, body]
    case Forall{Binder{name, typ, body, info}}:
      [typ, body]
    case Let{name, typ, val, body, nondep}:
      [typ, val, body]
    case Proj{type_name, index, structure}:
      [structure]
    case Mdata{expr, data}:
      [expr]
    case other:
      Nil{}
