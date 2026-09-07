let () =
  match Array.to_list Sys.argv with
  | [] ->
      Kanon.usage ();
      exit 64
  | _prog :: rest -> (
      match rest with
      | [] ->
          Kanon.usage ();
          exit 64
      | cmd :: args -> Kanon.dispatch cmd args)
