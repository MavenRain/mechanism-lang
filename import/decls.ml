type hints = Opaque_hint | Abbrev | Regular of int
type safety = Unsafe | Safe | Partial
type quot_kind = Quot_type | Quot_ctor | Quot_lift | Quot_ind
type rule = { ctor : int; nfields : int; rhs : int }
type inductive = {
  num_params : int; num_indices : int; all : int list; ctors : int list;
  num_nested : int; is_rec : bool; is_unsafe : bool; is_reflexive : bool;
}
type constructor = {
  induct : int; cidx : int; num_params : int; num_fields : int; is_unsafe : bool;
}
type recursor = {
  all : int list; num_params : int; num_indices : int; num_motives : int;
  num_minors : int; rules : rule list; k : bool; is_unsafe : bool;
}
type kind =
  | Axiom of bool
  | Definition of { value : int; hints : hints; safety : safety; all : int list }
  | Theorem of { value : int; all : int list }
  | Opaque of { value : int; is_unsafe : bool; all : int list }
  | Quotient of quot_kind
  | Inductive of inductive
  | Constructor of constructor
  | Recursor of recursor
type t = { name : int; level_params : int list; typ : int; kind : kind; line : int }
type group = { types : t list; ctors : t list; recs : t list; line : int }

let ( let* ) = Result.bind
let common = ["name"; "levelParams"; "type"]

let base ~line fields kind =
  let* name = Ndjson.nat_field "name" fields in
  let* level_params = Ndjson.nats_field "levelParams" fields in
  let* typ = Ndjson.nat_field "type" fields in
  Ok { name; level_params; typ; kind; line }

let enum choices text =
  Option.to_result ~none:("invalid enum " ^ text) (List.assoc_opt text choices)

let hints = function
  | Ndjson.String text -> enum ["opaque", Opaque_hint; "abbrev", Abbrev] text
  | Ndjson.Object fields ->
      let* fields = Ndjson.exact_object ["regular"] (Ndjson.Object fields) in
      let* value = Ndjson.nat_field "regular" fields in
      if Int64.compare (Int64.of_int value) 4294967295L <= 0 then Ok (Regular value)
      else Error "regular reducibility hint exceeds UInt32"
  | Ndjson.Null | Ndjson.Bool _ | Ndjson.Number _ | Ndjson.Array _ ->
      Error "invalid reducibility hints"

let rule value =
  let* fields = Ndjson.exact_object ["ctor"; "nfields"; "rhs"] value in
  let* ctor = Ndjson.nat_field "ctor" fields in
  let* nfields = Ndjson.nat_field "nfields" fields in
  let* rhs = Ndjson.nat_field "rhs" fields in
  Ok { ctor; nfields; rhs }

let inductive ~line value =
  let* fields = Ndjson.exact_object
    (common @ ["numParams"; "numIndices"; "all"; "ctors"; "numNested";
      "isRec"; "isUnsafe"; "isReflexive"]) value in
  let* num_params = Ndjson.nat_field "numParams" fields in
  let* num_indices = Ndjson.nat_field "numIndices" fields in
  let* all = Ndjson.nats_field "all" fields in
  let* ctors = Ndjson.nats_field "ctors" fields in
  let* num_nested = Ndjson.nat_field "numNested" fields in
  let* is_rec = Ndjson.bool_field "isRec" fields in
  let* is_unsafe = Ndjson.bool_field "isUnsafe" fields in
  let* is_reflexive = Ndjson.bool_field "isReflexive" fields in
  base ~line fields (Inductive {
    num_params; num_indices; all; ctors; num_nested; is_rec; is_unsafe; is_reflexive })

let constructor ~line value =
  let* fields = Ndjson.exact_object
    (common @ ["induct"; "cidx"; "numParams"; "numFields"; "isUnsafe"]) value in
  let* induct = Ndjson.nat_field "induct" fields in
  let* cidx = Ndjson.nat_field "cidx" fields in
  let* num_params = Ndjson.nat_field "numParams" fields in
  let* num_fields = Ndjson.nat_field "numFields" fields in
  let* is_unsafe = Ndjson.bool_field "isUnsafe" fields in
  base ~line fields (Constructor { induct; cidx; num_params; num_fields; is_unsafe })

let recursor ~line value =
  let* fields = Ndjson.exact_object
    (common @ ["all"; "numParams"; "numIndices"; "numMotives"; "numMinors";
      "rules"; "k"; "isUnsafe"]) value in
  let* all = Ndjson.nats_field "all" fields in
  let* num_params = Ndjson.nat_field "numParams" fields in
  let* num_indices = Ndjson.nat_field "numIndices" fields in
  let* num_motives = Ndjson.nat_field "numMotives" fields in
  let* num_minors = Ndjson.nat_field "numMinors" fields in
  let* rules = Ndjson.field "rules" fields in
  let* rules = Ndjson.list rule rules in
  let* k = Ndjson.bool_field "k" fields in
  let* is_unsafe = Ndjson.bool_field "isUnsafe" fields in
  base ~line fields (Recursor {
    all; num_params; num_indices; num_motives; num_minors; rules; k; is_unsafe })

let ordinary ~line tag value =
  let* kind, fields =
    match () with
    | () when tag = "axiom" ->
        let* fields = Ndjson.exact_object (common @ ["isUnsafe"]) value in
        let* is_unsafe = Ndjson.bool_field "isUnsafe" fields in
        Ok (Axiom is_unsafe, fields)
    | () when tag = "def" ->
        let* fields = Ndjson.exact_object (common @ ["value"; "hints"; "safety"; "all"]) value in
        let* value = Ndjson.nat_field "value" fields in
        let* hint_value = Ndjson.field "hints" fields in
        let* hints = hints hint_value in
        let* safety_text = Ndjson.string_field "safety" fields in
        let* safety = enum ["unsafe", Unsafe; "safe", Safe; "partial", Partial] safety_text in
        let* all = Ndjson.nats_field "all" fields in
        Ok (Definition { value; hints; safety; all }, fields)
    | () when tag = "thm" ->
        let* fields = Ndjson.exact_object (common @ ["value"; "all"]) value in
        let* value = Ndjson.nat_field "value" fields in
        let* all = Ndjson.nats_field "all" fields in
        Ok (Theorem { value; all }, fields)
    | () when tag = "opaque" ->
        let* fields = Ndjson.exact_object (common @ ["value"; "isUnsafe"; "all"]) value in
        let* value = Ndjson.nat_field "value" fields in
        let* is_unsafe = Ndjson.bool_field "isUnsafe" fields in
        let* all = Ndjson.nats_field "all" fields in
        Ok (Opaque { value; is_unsafe; all }, fields)
    | () when tag = "quot" ->
        let* fields = Ndjson.exact_object (common @ ["kind"]) value in
        let* kind_text = Ndjson.string_field "kind" fields in
        let* kind = enum ["type", Quot_type; "ctor", Quot_ctor; "lift", Quot_lift; "ind", Quot_ind] kind_text in
        Ok (Quotient kind, fields)
    | () -> Error ("unknown declaration tag " ^ tag)
  in
  base ~line fields kind

(** An inductive group must agree with itself.  Reference checks alone accept a
    constructor with the wrong index and a recursor rule with the wrong arity.
    A rule of a nested inductive names a constructor of an earlier group, so a
    name outside this group stays a reference check only. *)
let group_checks types ctors recs =
  let positions = List.concat_map (fun declaration ->
    match declaration.kind with
    | Inductive info -> List.mapi (fun index ctor -> ctor, (declaration.name, index)) info.ctors
    | Axiom _ | Definition _ | Theorem _ | Opaque _ | Quotient _
    | Constructor _ | Recursor _ -> []) types in
  let members = List.map (fun declaration -> declaration.name, declaration.kind)
    (types @ ctors @ recs) in
  let constructors = List.filter_map (fun declaration ->
    match declaration.kind with
    | Constructor info -> Some (declaration.name, info)
    | Axiom _ | Definition _ | Theorem _ | Opaque _ | Quotient _
    | Inductive _ | Recursor _ -> None) ctors in
  let constructor_info = function
    | Constructor info -> Some info
    | Axiom _ | Definition _ | Theorem _ | Opaque _ | Quotient _
    | Inductive _ | Recursor _ -> None in
  let rules = List.concat_map (fun declaration ->
    match declaration.kind with
    | Recursor info -> info.rules
    | Axiom _ | Definition _ | Theorem _ | Opaque _ | Quotient _
    | Inductive _ | Constructor _ -> []) recs in
  let constructor_ok (name, info) =
    let* declared, index = List.assoc_opt name positions
      |> Option.to_result ~none:(Printf.sprintf
        "constructor %d is not listed by an inductive type of its group" name) in
    if declared = info.induct && index = info.cidx then Ok ()
    else Error (Printf.sprintf
      "constructor %d disagrees with the ctors list of inductive type %d" name declared)
  in
  let rule_ok rule =
    List.assoc_opt rule.ctor members
    |> Option.fold ~none:(Ok ()) ~some:(fun kind ->
        let* info = constructor_info kind
          |> Option.to_result ~none:(Printf.sprintf
            "recursor rule names %d, a declaration of its group that is not a constructor"
            rule.ctor) in
        if info.num_fields = rule.nfields then Ok ()
        else Error (Printf.sprintf
          "recursor rule for constructor %d declares %d fields but the constructor has %d"
          rule.ctor rule.nfields info.num_fields))
  in
  let* _constructors = Ndjson.traverse constructor_ok constructors in
  let* _rules = Ndjson.traverse rule_ok rules in
  Ok ()

let parse ~line tag value =
  if tag = "inductive" then
    let* fields = Ndjson.exact_object ["types"; "ctors"; "recs"] value in
    let* types = Ndjson.field "types" fields in
    let* types = Ndjson.list (inductive ~line) types in
    let* ctors = Ndjson.field "ctors" fields in
    let* ctors = Ndjson.list (constructor ~line) ctors in
    let* recs = Ndjson.field "recs" fields in
    let* recs = Ndjson.list (recursor ~line) recs in
    let* () = group_checks types ctors recs in
    Ok (types @ ctors @ recs, Some { types; ctors; recs; line })
  else
    let* declaration = ordinary ~line tag value in
    Ok ([declaration], None)

let name_references declaration =
  let extra =
    match declaration.kind with
    | Axiom _ | Quotient _ -> []
    | Definition { value = _; hints = _; safety = _; all }
    | Theorem { value = _; all }
    | Opaque { value = _; is_unsafe = _; all } -> all
    | Inductive info -> info.all @ info.ctors
    | Constructor info -> [info.induct]
    | Recursor info -> info.all @ List.map (fun rule -> rule.ctor) info.rules
  in
  declaration.name :: declaration.level_params @ extra

let expr_references declaration =
  let extra =
    match declaration.kind with
    | Definition { value; hints = _; safety = _; all = _ }
    | Theorem { value; all = _ }
    | Opaque { value; is_unsafe = _; all = _ } -> [value]
    | Recursor info -> List.map (fun rule -> rule.rhs) info.rules
    | Axiom _ | Quotient _ | Inductive _ | Constructor _ -> []
  in
  declaration.typ :: extra

let kind_name = function
  | Axiom _ -> "axiom"
  | Definition _ -> "def"
  | Theorem _ -> "thm"
  | Opaque _ -> "opaque"
  | Quotient _ -> "quot"
  | Inductive _ -> "inductive"
  | Constructor _ -> "constructor"
  | Recursor _ -> "recursor"
