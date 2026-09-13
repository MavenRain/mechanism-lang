open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result

let prefix last members =
  let rec loop reversed = function
    | [] -> Error ("unknown last member: " ^ last)
    | member :: rest ->
        let reversed = member :: reversed in
        if String.equal member.Syntax.rd_name last then Ok (List.rev reversed)
        else loop reversed rest in
  loop [] members

let run path last =
  let* source = Mechanism_import.Io.attempt (fun () ->
    In_channel.with_open_bin path In_channel.input_all) in
  let* parsed = kernel (Parser.parse source) in
  let* parsed, members = match parsed with
    | [Syntax.DPolyGroup (arity, family, companions, members)] ->
        let* members = last |> Option.fold ~none:(Ok members)
          ~some:(fun last -> prefix last members) in
        Ok ([Syntax.DPolyGroup (arity, family, companions, members)], List.length members)
    | [] | _ :: _ -> Error "the benchmark requires one family group" in
  let polls = ref 0 in
  let budget = Budget.of_poll (fun () -> incr polls; false) in
  Gc.full_major ();
  let start_gc = Gc.quick_stat () in
  let start_cpu = Sys.time () in
  let checked = Elab.elab_program_in ~budget Global.empty parsed in
  let cpu = Sys.time () -. start_cpu in
  let stop_gc = Gc.quick_stat () in
  let* globals, rows = kernel checked in
  let allocated stats = stats.Gc.minor_words +. stats.major_words -. stats.promoted_words in
  Printf.printf
    "{\"checked\":true,\"members\":%d,\"entries\":%d,\"families\":%d,\"polls\":%d,\"allocated_words\":%.0f,\"cpu_seconds\":%.6f}\n"
    members (List.length rows) (Global.StringMap.cardinal globals.families) !polls
    (allocated stop_gc -. allocated start_gc) cpu;
  Ok ()

let () =
  let result = match Array.to_list Sys.argv with
    | [_program; path] -> run path None
    | [_program; path; "--through"; last] -> run path (Some last)
    | [] | _ :: _ -> Error "usage: template_cost.exe SOURCE [--through MEMBER]" in
  Result.fold result ~ok:(fun () -> ()) ~error:(fun message ->
    Printf.eprintf "TEMPLATE-COST-FAIL %s\n" message; exit 1)
