open Kanon_kernel

let ( let* ) = Result.bind

type scheme = { arity : int; decl : Check.decl }
type t = (string * scheme) list
let empty : t = []

let occupied globals catalog name =
  Option.is_some (Global.find name globals)
  || Option.is_some (Global.find_family name globals)
  || List.mem_assoc name catalog

let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let scope_error = Error.Universe "universe level is outside the global parameter scope"

(** The traversal carries level substitutions through every existing term field.
    Rules owns shape reconstruction, so no new shape dispatch lives here. *)
let rec map_levels f tm =
  let walk = map_levels f in
  let shape = Rules.map_shape walk in
  match tm with
  | Term.Var _ | Term.Global _ | Term.Lit _ | Term.Auto -> Ok tm
  | Term.Univ l -> Result.map (fun l -> Term.Univ l) (f l)
  | Term.Lan (s, d) -> let* s = shape s in let* d = walk d in Ok (Term.Lan (s, d))
  | Term.Ran (s, d) -> let* s = shape s in let* d = walk d in Ok (Term.Ran (s, d))
  | Term.In (s, a, args) ->
      let* s = shape s in let* a = map_addr f a in
      let* args = Rules.all_ok (List.map walk args) in Ok (Term.In (s, a, args))
  | Term.Sec (s, legs) ->
      let* s = shape s in let* legs = Rules.all_ok (List.map (map_leg f) legs) in
      Ok (Term.Sec (s, legs))
  | Term.Out (s, a, head) ->
      let* s = shape s in let* a = map_addr f a in let* head = walk head in
      Ok (Term.Out (s, a, head))
  | Term.Elim e ->
      let* s = shape e.Term.e_shape in let* scrut = walk e.Term.e_scrut in
      let* motive = e.Term.e_motive |> Option.fold ~none:(Ok None)
        ~some:(fun mo -> Result.map (fun body -> Some { mo with Term.m_body = body })
          (walk mo.Term.m_body)) in
      let* branches = Rules.all_ok (List.map (fun (a, leg) ->
        let* a = map_addr f a in let* leg = map_leg f leg in Ok (a, leg)) e.Term.e_branches) in
      Ok (Term.Elim { e with Term.e_shape = s; e_scrut = scrut;
        e_motive = motive; e_branches = branches })
  | Term.Let (x, ty, value, body) ->
      let* ty = walk ty in let* value = walk value in let* body = walk body in
      Ok (Term.Let (x, ty, value, body))
  | Term.Ann (body, ty) ->
      let* body = walk body in let* ty = walk ty in Ok (Term.Ann (body, ty))
and map_addr f addr =
  match addr with
  | Term.APt (q, arg) -> Result.map (fun arg -> Term.APt (q, arg)) (map_levels f arg)
  | Term.ALeg _ | Term.ACtor _ -> Ok addr
and map_leg f leg =
  Result.map (fun body -> { leg with Term.l_body = body }) (map_levels f leg.Term.l_body)

let map_decl f (d : Check.decl) =
  let* ty = map_levels f d.d_ty in
  let* body = d.d_body |> Option.fold ~none:(Ok None)
    ~some:(fun body -> Result.map Option.some (map_levels f body)) in
  Ok { d with Check.d_ty = ty; d_body = body }

(** Checked templates stay outside Global.t.  Only closed, rechecked instances
    enter that environment, so catalog bookkeeping is not a kernel rule. *)
let declare ?(budget = Budget.unlimited) globals catalog ~arity (decl : Check.decl) =
  if occupied globals catalog decl.d_name then Error (collision decl.d_name)
  else if arity < 0 then Error (Error.Universe "a universe parameter arity must be nonnegative")
  else
    let* () = Check.check_scheme globals budget ~arity decl in
    Ok ((decl.d_name, { arity; decl }) :: catalog)

let arity catalog name = Option.map (fun s -> s.arity) (List.assoc_opt name catalog)

let instantiate ?(budget = Budget.unlimited) globals catalog ~name ~levels ~as_name =
  let* scheme = List.assoc_opt name catalog
    |> Option.to_result ~none:(Error.Unbound ("unknown universe schema " ^ name)) in
  match () with
  | () when occupied globals catalog as_name -> Error (collision as_name)
  | () when List.length levels <> scheme.arity ->
      Error (Error.Universe (Printf.sprintf "the schema %s expects %d universe arguments, got %d"
        name scheme.arity (List.length levels)))
  | () when not (List.for_all (Level.in_scope 0) levels) ->
      Error (Error.Universe "universe arguments must be closed")
  | () ->
      let* decl = map_decl (fun l -> Level.subst levels l |> Option.to_result ~none:scope_error)
        scheme.decl in
      let decl = { decl with Check.d_name = as_name } in
      let* entry = Check.check_decl globals budget decl in
      Ok (Global.add as_name entry globals, Term.Global as_name)
