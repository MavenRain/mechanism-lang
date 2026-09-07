open Mechanism_kernel

let ( let* ) = Result.bind
module Int_map = Map.Make (Int)
module Int_set = Set.Make (Int)

type level =
  | Zero
  | Succ of int
  | Max of int * int
  | Imax of int * int
  | Param of { source_name : int; name : string }

type binder = { name : string; typ : int; body : int; info : Exprs.binder_info }

type node =
  | Bound of int
  | Sort of int
  | Constant of { source_name : int; name : string; universes : int list }
  | Apply of int * int
  | Lambda of binder
  | Pi of binder
  | Let of { name : string; typ : int; value : int; body : int; nondep : bool }
  | Projection of { source_type_name : int; type_name : string; index : int; structure : int }
  | Nat of string
  | String of string
  | Metadata of { expr : int; data : (string * Ndjson.t) list }

type row = {
  declaration : Decls.t;
  name : string;
  parameters : (int * string) list;
  root : int;
  dependencies : string list;
}

type summary = { binders : int; params : Int_set.t; constants : Int_set.t }

type t = {
  type_nodes : node Int_map.t;
  level_nodes : level Int_map.t;
  summaries : summary Int_map.t;
  level_params : Int_set.t Int_map.t;
  decl_rows : row list;
}

let rows table = table.decl_rows
let nodes table = Int_map.bindings table.type_nodes
let levels table = Int_map.bindings table.level_nodes
let empty_summary = { binders = 0; params = Int_set.empty; constants = Int_set.empty }
let empty = {
  type_nodes = Int_map.empty; level_nodes = Int_map.empty;
  summaries = Int_map.empty; level_params = Int_map.empty; decl_rows = [];
}
let merge a b = {
  binders = Int.max a.binders b.binders;
  params = Int_set.union a.params b.params;
  constants = Int_set.union a.constants b.constants;
}
let error line message = Error { Export.line; message }
let named source line id =
  Export.name_string source id
  |> Option.to_result ~none:{ Export.line;
    message = Printf.sprintf "missing source name %d" id }
let canonical source line id =
  Export.canonical_name source id
  |> Option.to_result ~none:{ Export.line;
    message = Printf.sprintf "missing source name identity %d" id }
let all f values =
  List.fold_left (fun acc value ->
    let* results = acc in
    let* result = f value in
    Ok (result :: results)) (Ok []) values
  |> Result.map List.rev

let cached found ~hit ~miss =
  Option.fold ~none:miss ~some:(fun value () -> hit value) found ()

let rec visit_level ?(depth = 0) source line table id =
  let create () =
    let* () = if depth > 1024 then error line "type level DAG exceeds the depth limit of 1024"
      else Ok () in
    let* raw = Export.level source id
      |> Option.to_result ~none:{ Export.line;
        message = Printf.sprintf "missing source level %d" id } in
    let* table, node, params =
      match raw with
      | Levels.Zero -> Ok (table, Zero, Int_set.empty)
      | Levels.Param source_name ->
          let* name = named source line source_name in
          let* source_name = canonical source line source_name in
          Ok (table, Param { source_name; name }, Int_set.singleton source_name)
      | Levels.Succ child ->
          let* table, params = visit_level ~depth:(depth + 1) source line table child in
          Ok (table, Succ child, params)
      | Levels.Max (left, right) ->
          visit_level_pair depth source line table left right (Max (left, right))
      | Levels.Imax (left, right) ->
          visit_level_pair depth source line table left right (Imax (left, right))
    in
    Ok ({ table with level_nodes = Int_map.add id node table.level_nodes;
      level_params = Int_map.add id params table.level_params }, params)
  in
  cached (Int_map.find_opt id table.level_params)
    ~hit:(fun params -> Ok (table, params)) ~miss:create

and visit_level_pair depth source line table left right node =
  let* table, a = visit_level ~depth:(depth + 1) source line table left in
  let* table, b = visit_level ~depth:(depth + 1) source line table right in
  Ok (table, node, Int_set.union a b)

let visit_levels source line table ids =
  List.fold_left (fun acc id ->
    let* table, params = acc in
    let* table, next = visit_level source line table id in
    Ok (table, Int_set.union params next)) (Ok (table, Int_set.empty)) ids

let rec visit_expr ?(depth = 0) source table id =
  let create () =
    let line = Option.value ~default:0 (Export.expr_line source id) in
    let* () = if depth > 1024 then error line "type expression DAG exceeds the depth limit of 1024"
      else Ok () in
    let* raw = Export.expr source id
      |> Option.to_result ~none:{ Export.line;
        message = Printf.sprintf "missing source expression %d" id } in
    let* table, node, summary =
      match raw with
      | Exprs.Bvar index ->
          if index < 0 || index = Int.max_int then
            error line "a type has an invalid de Bruijn index"
          else Ok (table, Bound index, { empty_summary with binders = index + 1 })
      | Exprs.Sort level ->
          let* table, params = visit_level source line table level in
          Ok (table, Sort level, { empty_summary with params })
      | Exprs.Const { name = source_name; universes } ->
          let* name = named source line source_name in
          let* declaration = Export.declaration source source_name
            |> Option.to_result ~none:{ Export.line;
              message = "type references undeclared constant " ^ name } in
          let expected = List.length declaration.Decls.level_params in
          let given = List.length universes in
          let* () = if expected = given then Ok () else
            error line (Printf.sprintf
              "constant %s expects %d universe arguments but has %d" name expected given) in
          let* table, params = visit_levels source line table universes in
          let* identity = canonical source line source_name in
          Ok (table, Constant { source_name; name; universes },
            { empty_summary with params; constants = Int_set.singleton identity })
      | Exprs.App (head, argument) ->
          let* table, a = visit_expr ~depth:(depth + 1) source table head in
          let* table, b = visit_expr ~depth:(depth + 1) source table argument in
          Ok (table, Apply (head, argument), merge a b)
      | Exprs.Lam binder -> visit_binder depth source line table binder (fun b -> Lambda b)
      | Exprs.Forall binder -> visit_binder depth source line table binder (fun b -> Pi b)
      | Exprs.Let { name; typ; value; body; nondep } ->
          let* name = named source line name in
          let* table, a = visit_expr ~depth:(depth + 1) source table typ in
          let* table, b = visit_expr ~depth:(depth + 1) source table value in
          let* table, c = visit_expr ~depth:(depth + 1) source table body in
          let summary = merge (merge a b) { c with binders = Int.max 0 (c.binders - 1) } in
          Ok (table, Let { name; typ; value; body; nondep }, summary)
      | Exprs.Proj { type_name; index; structure } ->
          let* name = named source line type_name in
          let* identity = canonical source line type_name in
          let* table, summary = visit_expr ~depth:(depth + 1) source table structure in
          Ok (table, Projection { source_type_name = type_name; type_name = name; index; structure },
            { summary with constants = Int_set.add identity summary.constants })
      | Exprs.Nat value -> Ok (table, Nat value, empty_summary)
      | Exprs.String value -> Ok (table, String value, empty_summary)
      | Exprs.Mdata { expr; data } ->
          let* table, summary = visit_expr ~depth:(depth + 1) source table expr in
          Ok (table, Metadata { expr; data }, summary)
    in
    Ok ({ table with type_nodes = Int_map.add id node table.type_nodes;
      summaries = Int_map.add id summary table.summaries }, summary)
  in
  cached (Int_map.find_opt id table.summaries)
    ~hit:(fun summary -> Ok (table, summary)) ~miss:create

and visit_binder depth source line table (binder : Exprs.binder) make_node =
  let* name = named source line binder.name in
  let* table, domain = visit_expr ~depth:(depth + 1) source table binder.typ in
  let* table, body = visit_expr ~depth:(depth + 1) source table binder.body in
  let summary = merge domain { body with binders = Int.max 0 (body.binders - 1) } in
  Ok (table, make_node { name; typ = binder.typ; body = binder.body; info = binder.info }, summary)

let translate source =
  let add acc (declaration : Decls.t) =
    let* table, reversed = acc in
    let line = declaration.line in
    let* name = named source line declaration.name in
    let* parameters = all (fun id ->
      let* name = named source line id in
      let* identity = canonical source line id in
      Ok (identity, name)) declaration.level_params in
    let bound_params = Int_set.of_list (List.map fst parameters) in
    let* () = if Int_set.cardinal bound_params = List.length parameters then Ok ()
      else error line ("duplicate universe parameters in " ^ name) in
    let* table, summary = visit_expr source table declaration.typ in
    let* () = if summary.binders = 0 then Ok () else
      error line (Printf.sprintf "type of %s has %d unbound term binders" name summary.binders) in
    let unbound = Int_set.diff summary.params bound_params |> Int_set.elements in
    let* unbound_names = all (named source line) unbound in
    let* () = if List.length unbound_names = 0 then Ok () else
      error line ("type of " ^ name ^ " has unbound universe parameters: "
        ^ String.concat ", " unbound_names) in
    let* dependencies = all (named source line) (Int_set.elements summary.constants) in
    let row = { declaration; name; parameters; root = declaration.typ; dependencies } in
    Ok (table, row :: reversed)
  in
  let* table, reversed = List.fold_left add (Ok (empty, [])) (Export.declarations source) in
  Ok { table with decl_rows = List.rev reversed }

type lowering_error =
  | Deferred of string
  | Unsupported of string
  | Kernel_error of Error.t

let lowering_message = function
  | Deferred message -> message
  | Unsupported message -> message
  | Kernel_error error -> Error.to_string error

let kernel result = Result.map_error (fun error -> Kernel_error error) result

let lower_level budget step table parameters id =
  let positions = List.mapi (fun position (source, _name) -> source, position) parameters
    |> List.fold_left (fun found (source, position) -> Int_map.add source position found) Int_map.empty in
  let rec lower depth cache id =
    let create () =
      let* () = step () in
      let* () = if Budget.exhausted budget then
        Error (Kernel_error (Error.Budget_exhausted "universe lowering budget is exhausted"))
        else if depth > 1024 then Error (Unsupported "universe lowering exceeds the depth limit of 1024")
        else Ok () in
      let* node = Int_map.find_opt id table.level_nodes
        |> Option.to_result ~none:(Unsupported (Printf.sprintf "missing type level %d" id)) in
      let* cache, value =
        match node with
        | Zero -> Ok (cache, Level.zero)
        | Param { source_name; name } ->
            let* position = Int_map.find_opt source_name positions
              |> Option.to_result ~none:(Unsupported ("unbound universe parameter " ^ name)) in
            let* value = Level.var position
              |> Option.to_result ~none:(Unsupported "invalid universe parameter position") in
            Ok (cache, value)
        | Succ child ->
            let* cache, value = lower (depth + 1) cache child in
            Ok (cache, Level.succ value)
        | Max (left, right) -> lower_pair depth cache left right Level.max
        | Imax (left, right) -> lower_pair depth cache left right Level.imax
      in
      Ok (Int_map.add id value cache, value)
    in
    cached (Int_map.find_opt id cache) ~hit:(fun value -> Ok (cache, value)) ~miss:create
  and lower_pair depth cache left right combine =
    let* cache, a = lower (depth + 1) cache left in
    let* cache, b = lower (depth + 1) cache right in
    Ok (cache, combine a b)
  in
  Result.map snd (lower 0 Int_map.empty id)

let lower_type ?budget ~resolve globals table row =
  let polls = ref 0 in
  let bounded = Budget.of_poll (fun () -> incr polls; !polls > 100000) in
  let budget = Option.value ~default:bounded budget in
  let work = ref 0 in
  let step () =
    incr work;
    if !work > 100000 then Error (Kernel_error (Error.Budget_exhausted
      "type lowering exceeds the work limit of 100000 nodes"))
    else Ok () in
  let arity = List.length row.parameters in
  let initial = Check.make ~level_arity:arity globals budget in
  let eval (context : Check.ctx) term = kernel (Eval.eval globals context.env term) in
  let infer context term = kernel (Check.infer context Quantity.Zero term) in
  let universe context term = kernel (Check.infer_univ context term) in
  let rec lower depth (context : Check.ctx) id =
    let* () = step () in
    let* () =
      if Budget.exhausted budget then
        Error (Kernel_error (Error.Budget_exhausted "type lowering budget is exhausted"))
      else if depth > 1024 then Error (Unsupported "type lowering exceeds the depth limit of 1024")
      else Ok () in
    let* node = Int_map.find_opt id table.type_nodes
      |> Option.to_result ~none:(Unsupported (Printf.sprintf "missing type expression %d" id)) in
    let descend = lower (depth + 1) context in
    match node with
    | Bound index -> Ok (Term.Var index)
    | Sort level ->
        let* value = lower_level budget step table row.parameters level in
        Ok (Term.Univ value)
    | Constant { source_name = _source_name; name; universes } ->
        let* levels = all (lower_level budget step table row.parameters) universes in
        let* term = resolve ~name ~levels |> Result.map_error (fun reason -> Deferred reason) in
        let* _ty = infer initial term in
        Ok term
    | Apply (head, argument) ->
        let* head = descend head in
        let* head_type = infer context head in
        let* head_type = kernel (Eval.whnf globals head_type) in
        let* shape, _codomain, _universe = Value.as_ran head_type
          |> Option.to_result ~none:(Kernel_error
            (Error.Mismatch "the head of an imported application is not a function")) in
        let* quantity, name, domain_value = Rules.as_vpi shape
          |> Option.to_result ~none:(Kernel_error
            (Error.Mismatch "the head of an imported application is not a dependent function")) in
        let* argument = descend argument in
        let* () = kernel (Check.check context Quantity.Zero argument domain_value) in
        let* domain = kernel (Eval.quote globals context.size domain_value) in
        Ok (Term.Out (Shape.SPi (quantity, name, domain), Term.APt (quantity, argument), head))
    | Pi binder ->
        let* domain, inner = bind depth context binder in
        let* body = lower (depth + 1) inner binder.body in
        Ok (Rules.arrow Quantity.Many binder.name domain body)
    | Lambda binder ->
        let* domain, inner = bind depth context binder in
        let* body = lower (depth + 1) inner binder.body in
        let* body_type = infer inner body in
        let* codomain = kernel (Eval.quote globals inner.size body_type) in
        let shape = Shape.SPi (Quantity.Many, binder.name, domain) in
        let section = Term.Sec (shape,
          [{ Term.l_binders = [Quantity.Many, binder.name]; l_body = body }]) in
        Ok (Term.Ann (section, Rules.arrow Quantity.Many binder.name domain codomain))
    | Let { name; typ; value; body; nondep = _nondep } ->
        let* typ = descend typ in
        let* _level = universe context typ in
        let* typ_value = eval context typ in
        let* value = descend value in
        let* () = kernel (Check.check context Quantity.Zero value typ_value) in
        let* value_value = eval context value in
        let inner = Check.define name Quantity.Many typ_value value_value context in
        let* body = lower (depth + 1) inner body in
        Ok (Term.Let (name, typ, value, body))
    | Projection { source_type_name = _source_type_name; type_name; index; structure = _structure } ->
        Error (Unsupported (Printf.sprintf
          "projection %s.%d needs a checked prelude representation" type_name index))
    | Nat value ->
        let* number = Bignum.of_decimal value
          |> Option.to_result ~none:(Unsupported "invalid natural literal in a type") in
        Ok (Term.Lit (Literal.LInt number))
    | String _value -> Error (Unsupported "strVal in a type arrives at M1 with String")
    | Metadata { expr; data = _data } -> descend expr
  and bind depth context (binder : binder) =
    let* domain = lower (depth + 1) context binder.typ in
    let* _level = universe context domain in
    let* value = eval context domain in
    Ok (domain, Check.bind binder.name Quantity.Many value context)
  in
  let* term = lower 0 initial row.root in
  let* _level = universe initial term in
  Ok term
