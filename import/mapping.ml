let ( let* ) = Result.bind
module Name_map = Map.Make (String)
module Name_set = Set.Make (String)

type verdict = Name_only | Unmapped | Never
type row = { lean_name : string; target : string option; verdict : verdict }
type exception_row = { lean_name : string; ratification : string; milestone : string }
type t = {
  rows : row list;
  never : exception_row list;
  external_declared : int;
  const_names : int;
}

let verdict_name = function
  | Name_only -> "NAME_ONLY"
  | Unmapped -> "UNMAPPED"
  | Never -> "NEVER"

let omega_reflection name =
  String.equal name "Lean.Omega" || String.starts_with ~prefix:"Lean.Omega." name

let cell value =
  String.length value > 0 && not (String.equal value "-")
  && not (String.exists (fun c -> List.mem c ['\t'; '\n'; '\r'; '\000']) value)

let inventory ?(targets = []) source =
  let constants = Report.constant_names source in
  let* names = Report.Names_set.fold (fun id result ->
      let* names = result in
      let* name = Export.name_string source id
        |> Option.to_result ~none:(Printf.sprintf "missing canonical source name %d" id) in
      Ok (if Report.external_name name then Name_set.add name names else names))
      constants (Ok Name_set.empty) in
  let* targets = List.fold_left (fun result (name, target) ->
      let* found = result in
      match () with
      | () when not (cell name && cell target) -> Error "mapping targets require nonempty TSV cells"
      | () when Name_map.mem name found -> Error ("duplicate mapping target for " ^ name)
      | () when not (Name_set.mem name names) ->
          Error ("mapping target is not a referenced external constant: " ^ name)
      | () when omega_reflection name -> Error ("mapping target conflicts with ratified NEVER: " ^ name)
      | () -> Ok (Name_map.add name target found)) (Ok Name_map.empty) targets in
  let rows = Name_set.elements names |> List.map (fun lean_name ->
      if omega_reflection lean_name then { lean_name; target = None; verdict = Never }
      else Name_map.find_opt lean_name targets |> Option.fold
        ~some:(fun target -> { lean_name; target = Some target; verdict = Name_only })
        ~none:{ lean_name; target = None; verdict = Unmapped }) in
  let never = List.filter_map (fun (row : row) -> match row.verdict with
      | Never -> Some { lean_name = row.lean_name;
          ratification = "R-V4:omega-reflection"; milestone = "NEVER" }
      | Name_only | Unmapped -> None) rows in
  let external_declared = Export.declarations source |> List.fold_left (fun count declaration ->
      Export.name_string source declaration.Decls.name |> Option.fold ~none:count
        ~some:(fun name -> if Report.external_name name then count + 1 else count)) 0 in
  Ok { rows; never; external_declared; const_names = Report.Names_set.cardinal constants }

let foundation_targets source =
  let names = Report.Names_set.fold (fun id names ->
      Export.name_string source id |> Option.fold ~none:names
        ~some:(fun name -> Name_set.add name names)) (Report.constant_names source) Name_set.empty in
  List.filter (fun (name, _target) -> Name_set.mem name names)
    [ "Nat", "MechNat"; "Nat.zero", "mechZero"; "Nat.succ", "mechSucc";
      "Bool", "MechBool"; "Bool.false", "mechFalse"; "Bool.true", "mechTrue";
      "Unit", "MechUnit"; "Unit.unit", "mechUnit"; "Empty", "MechEmpty";
      "False", "MechFalse"; "Sum", "MechSum"; "Sum.inl", "mechInl";
      "Sum.inr", "mechInr"; "Decidable", "MechDecidable";
      "Decidable.isFalse", "mechIsFalse"; "Decidable.isTrue", "mechIsTrue" ]

let map_tsv inventory =
  let lines = List.map (fun (row : row) -> String.concat "\t"
      [ row.lean_name; Option.value ~default:"-" row.target; verdict_name row.verdict ]) inventory.rows in
  String.concat "\n" ("lean_name\ttarget\tverdict" :: lines) ^ "\n"

let never_tsv inventory =
  let lines = List.map (fun (row : exception_row) -> String.concat "\t"
      [ row.lean_name; row.ratification; row.milestone ]) inventory.never in
  String.concat "\n" ("lean_name\tratification\tmilestone" :: lines) ^ "\n"
