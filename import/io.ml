(** The import boundary converts only host file errors into Result values.
    Parser and translation errors never use host exceptions. *)
let attempt (action : unit -> 'a) : ('a, string) result =
  try Ok (action ()) with
  | Sys_error message -> Error message
  | Unix.Unix_error (code, operation, path) ->
      Error (Printf.sprintf "%s %s: %s" operation path (Unix.error_message code))

let ( let* ) = Result.bind

(** The output path may carry a trailing separator, so the staging sibling
    is derived from the parent and the last component, never by concatenation. *)
let publish ~directory (files : (string * (out_channel -> unit)) list) =
  let parent = Filename.dirname directory in
  let base = Filename.basename directory in
  let target = Filename.concat parent base in
  let temporary = Filename.concat parent (base ^ ".tmp-" ^ string_of_int (Unix.getpid ())) in
  if Sys.file_exists target then Error ("output already exists: " ^ directory)
  else
    let* () = attempt (fun () -> Unix.mkdir temporary 0o700) in
    let result =
      let* () =
        List.fold_left
          (fun result (name, write) ->
            let* () = result in
            attempt (fun () ->
                Out_channel.with_open_bin (Filename.concat temporary name) write))
          (Ok ()) files
      in
      if Sys.file_exists target then Error ("output already exists: " ^ directory)
      else attempt (fun () -> Unix.rename temporary target)
    in
    Result.fold ~ok:(fun () -> Ok ()) ~error:(fun message ->
        let cleanup = attempt (fun () ->
            List.iter (fun (name, _write) ->
                let path = Filename.concat temporary name in
                if Sys.file_exists path then Sys.remove path) files;
            Unix.rmdir temporary) in
        Result.fold ~ok:(fun () -> Error message)
          ~error:(fun reason -> Error (message ^ "; temporary output: " ^ reason))
          cleanup) result
