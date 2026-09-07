type binder_info = Default | Implicit | Strict_implicit | Inst_implicit
type binder = { name : int; typ : int; body : int; info : binder_info }
type t =
  | Bvar of int
  | Sort of int
  | Const of { name : int; universes : int list }
  | App of int * int
  | Lam of binder
  | Forall of binder
  | Let of { name : int; typ : int; value : int; body : int; nondep : bool }
  | Proj of { type_name : int; index : int; structure : int }
  | Nat of string
  | String of string
  | Mdata of { expr : int; data : (string * Ndjson.t) list }

let ( let* ) = Result.bind

let binder_info text =
  Option.to_result ~none:("invalid binderInfo " ^ text)
    (List.assoc_opt text ["default", Default; "implicit", Implicit;
      "strictImplicit", Strict_implicit; "instImplicit", Inst_implicit])

let binder value =
  let* fields = Ndjson.exact_object ["name"; "type"; "body"; "binderInfo"] value in
  let* name = Ndjson.nat_field "name" fields in
  let* typ = Ndjson.nat_field "type" fields in
  let* body = Ndjson.nat_field "body" fields in
  let* info = Ndjson.string_field "binderInfo" fields in
  let* info = binder_info info in
  Ok { name; typ; body; info }

let parse tag value =
  match () with
  | () when tag = "bvar" -> let* index = Ndjson.nat value in Ok (Bvar index)
  | () when tag = "sort" -> let* index = Ndjson.nat value in Ok (Sort index)
  | () when tag = "const" ->
      let* fields = Ndjson.exact_object ["name"; "us"] value in
      let* name = Ndjson.nat_field "name" fields in
      let* universes = Ndjson.nats_field "us" fields in
      Ok (Const { name; universes })
  | () when tag = "app" ->
      let* fields = Ndjson.exact_object ["fn"; "arg"] value in
      let* fn = Ndjson.nat_field "fn" fields in
      let* arg = Ndjson.nat_field "arg" fields in
      Ok (App (fn, arg))
  | () when tag = "lam" -> let* binder = binder value in Ok (Lam binder)
  | () when tag = "forallE" -> let* binder = binder value in Ok (Forall binder)
  | () when tag = "letE" ->
      let* fields = Ndjson.exact_object ["name"; "type"; "value"; "body"; "nondep"] value in
      let* name = Ndjson.nat_field "name" fields in
      let* typ = Ndjson.nat_field "type" fields in
      let* value = Ndjson.nat_field "value" fields in
      let* body = Ndjson.nat_field "body" fields in
      let* nondep = Ndjson.bool_field "nondep" fields in
      Ok (Let { name; typ; value; body; nondep })
  | () when tag = "proj" ->
      let* fields = Ndjson.exact_object ["typeName"; "idx"; "struct"] value in
      let* type_name = Ndjson.nat_field "typeName" fields in
      let* index = Ndjson.nat_field "idx" fields in
      let* structure = Ndjson.nat_field "struct" fields in
      Ok (Proj { type_name; index; structure })
  | () when tag = "natVal" ->
      let* text = Ndjson.string value in
      if String.length text > 0 && String.for_all (fun c -> c >= '0' && c <= '9') text
      then Ok (Nat text) else Error "invalid natural literal"
  | () when tag = "strVal" -> let* text = Ndjson.string value in Ok (String text)
  | () when tag = "mdata" ->
      let* fields = Ndjson.exact_object ["expr"; "data"] value in
      let* expr = Ndjson.nat_field "expr" fields in
      let* data = Ndjson.field "data" fields in
      let* data = Ndjson.object_fields data in
      Ok (Mdata { expr; data })
  | () -> Error ("unknown expression tag " ^ tag)

let name_references = function
  | Const { name; universes = _ } -> [name]
  | Lam binder | Forall binder -> [binder.name]
  | Let { name; typ = _; value = _; body = _; nondep = _ } -> [name]
  | Proj { type_name; index = _; structure = _ } -> [type_name]
  | Bvar _ | Sort _ | App _ | Nat _ | String _ | Mdata _ -> []

let level_references = function
  | Sort index -> [index]
  | Const { name = _; universes } -> universes
  | Bvar _ | App _ | Lam _ | Forall _ | Let _ | Proj _ | Nat _ | String _ | Mdata _ -> []

let expr_references = function
  | App (fn, arg) -> [fn; arg]
  | Lam binder | Forall binder -> [binder.typ; binder.body]
  | Let { name = _; typ; value; body; nondep = _ } -> [typ; value; body]
  | Proj { type_name = _; index = _; structure } -> [structure]
  | Mdata { expr; data = _ } -> [expr]
  | Bvar _ | Sort _ | Const _ | Nat _ | String _ -> []
