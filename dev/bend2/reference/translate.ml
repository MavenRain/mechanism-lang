open Mechanism_import
let num n = Ndjson.Number (string_of_int n)
let text s = Ndjson.String s
let parameter (id, name) = Ndjson.Object ["source_name", num id; "name", text name]
let row (r : Translate.row) = Ndjson.Array [
  num r.declaration.name; text r.name; Ndjson.Array (List.map parameter r.parameters);
  num r.root; Ndjson.Array (List.map text r.dependencies)]
let summary table = Ndjson.Array [
  Ndjson.Array (List.map row (Translate.rows table));
  Ndjson.Array (List.map Report.level_record (Translate.levels table));
  Ndjson.Array (List.map Report.node_record (Translate.nodes table))]
let () =
  match Array.to_list Sys.argv with
  | [_program; source] ->
      Result.bind (Export.read_string source) Translate.translate |> Result.fold
        ~ok:(fun value -> print_endline (Ndjson.to_string (summary value)))
        ~error:(fun (e : Export.error) ->
          prerr_endline (string_of_int e.line ^ ":" ^ e.message); exit 1)
  | [] | [_] | _ :: _ :: _ :: _ -> exit 64
