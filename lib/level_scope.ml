(** Inspect raw fields before rules may ignore an introduction annotation. *)
let check budget arity term : (unit, Error.t) result =
  let ( let* ) = Result.bind in
  let all f = List.fold_left (fun acc x -> let* () = acc in f x) (Ok ()) in
  let rec walk tm =
    if Budget.exhausted budget then
      Error (Error.Budget_exhausted "the check budget is exhausted")
    else
      match tm with
      | Term.Var _ | Term.Global _ | Term.Lit _ | Term.Auto -> Ok ()
      | Term.Univ l ->
          if Level.in_scope arity l then Ok ()
          else Error (Error.Universe "universe level is outside the global parameter scope")
      | Term.Lan (s, d) | Term.Ran (s, d) -> let* () = shape s in walk d
      | Term.In (s, a, args) ->
          let* () = shape s in let* () = addr a in all walk args
      | Term.Sec (s, legs) -> let* () = shape s in all leg legs
      | Term.Out (s, a, head) -> let* () = shape s in let* () = addr a in walk head
      | Term.Elim e ->
          let* () = shape e.Term.e_shape in let* () = walk e.Term.e_scrut in
          let* () = e.Term.e_motive |> Option.fold ~none:(Ok ())
            ~some:(fun mo -> walk mo.Term.m_body) in
          all (fun (a, l) -> let* () = addr a in leg l) e.Term.e_branches
      | Term.Let (_x, ty, value, body) -> all walk [ty; value; body]
      | Term.Ann (body, ty) -> all walk [body; ty]
  and shape s = all walk (Shape.payload s)
  and addr a =
    match a with
    | Term.APt (_q, arg) -> walk arg
    | Term.ALeg _ | Term.ACtor _ -> Ok ()
  and leg l = walk l.Term.l_body in
  walk term
