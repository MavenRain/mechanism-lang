open Mechanism_import
let num n = Ndjson.Number (string_of_int n)
let nums xs = Ndjson.Array (List.map num xs)
let maybe_text value = Option.fold ~none:Ndjson.Null ~some:(fun x -> Ndjson.String x) value
let expr_kind = function
  | Exprs.Bvar _ -> "bvar" | Exprs.Sort _ -> "sort" | Exprs.Const _ -> "const"
  | Exprs.App _ -> "app" | Exprs.Lam _ -> "lam" | Exprs.Forall _ -> "forallE"
  | Exprs.Let _ -> "letE" | Exprs.Proj _ -> "proj" | Exprs.Nat _ -> "natVal"
  | Exprs.String _ -> "strVal" | Exprs.Mdata _ -> "mdata"
let decl (d : Decls.t) = Ndjson.Array [
  num d.name; Ndjson.String (Decls.kind_name d.kind); num d.typ;
  nums d.level_params; nums (Decls.name_references d);
  nums (Decls.expr_references d); num d.line]
let summary source =
  let open Ndjson in
  let m = Export.metadata source in
  let c = Export.counts source in
  Array [
    Array (List.map (fun s -> String s)
      [m.exporter_name; m.exporter_version; m.lean_githash; m.lean_version; m.format_version]);
    nums [c.lines; c.names; c.levels; c.exprs; c.declarations];
    Array (Export.names source |> List.map (fun (i, _node) ->
      Array [num i; maybe_text (Export.name_string source i);
        maybe_text (Option.map string_of_int (Export.canonical_name source i))]));
    Array (List.map decl (Export.declarations source));
    Array (Export.expressions source |> List.map (fun (i, node) ->
      Array [num i; String (expr_kind node); nums (Exprs.name_references node);
        nums (Exprs.level_references node); nums (Exprs.expr_references node)]));
    num (List.length (Export.groups source))]
let () =
  match Array.to_list Sys.argv with
  | [_program; source] -> Export.read_string source |> Result.fold
      ~ok:(fun value -> print_endline (Ndjson.to_string (summary value)))
      ~error:(fun (e : Export.error) ->
        prerr_endline (string_of_int e.line ^ ":" ^ e.message); exit 1)
  | [] | [_] | _ :: _ :: _ :: _ -> exit 64
