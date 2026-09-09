open Kanon_kernel

let ( let* ) = Result.bind

type scheme = {
  arity : int;
  family : Check.family_decl;
  ctors : Check.ctor_decl list;
}

type t = (string * scheme) list

let empty : t = []
let arity catalog name = Option.map (fun s -> s.arity) (List.assoc_opt name catalog)

let occupied globals catalog name =
  Option.is_some (Global.find name globals)
  || Option.is_some (Global.find_family name globals)
  || List.mem_assoc name catalog

let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let scope_error = Error.Universe "universe level is outside the global parameter scope"

let poll budget =
  if Budget.exhausted budget then
    Error (Error.Budget_exhausted "the check budget is exhausted")
  else Ok ()

let map_list f xs =
  let rec loop rows = function
    | [] -> Ok (List.rev rows)
    | x :: rest -> let* row = f x in loop (row :: rows) rest
  in
  loop [] xs

(** This is a raw syntax traversal, including annotations which checking may
    ignore. Every shape is reconstructed, even currently unsupported shapes,
    so level substitution and family renaming cover every nested field. *)
let rec map_term budget level name tm =
  let* () = poll budget in
  let walk = map_term budget level name in
  let shape = map_shape budget level name in
  match tm with
  | Term.Var _ | Term.Lit _ | Term.Auto -> Ok tm
  | Term.Global n -> Result.map (fun n -> Term.Global n) (name n)
  | Term.Univ l -> Result.map (fun l -> Term.Univ l) (level l)
  | Term.Lan (s, d) -> let* s = shape s in let* d = walk d in Ok (Term.Lan (s, d))
  | Term.Ran (s, d) -> let* s = shape s in let* d = walk d in Ok (Term.Ran (s, d))
  | Term.In (s, a, args) ->
      let* s = shape s in let* a = map_addr budget level name a in
      let* args = map_list walk args in Ok (Term.In (s, a, args))
  | Term.Sec (s, legs) ->
      let* s = shape s in let* legs = map_list (map_leg budget level name) legs in
      Ok (Term.Sec (s, legs))
  | Term.Out (s, a, head) ->
      let* s = shape s in let* a = map_addr budget level name a in
      let* head = walk head in Ok (Term.Out (s, a, head))
  | Term.Elim e ->
      let* s = shape e.Term.e_shape in let* scrut = walk e.Term.e_scrut in
      let* motive = e.Term.e_motive |> Option.fold ~none:(Ok None)
        ~some:(fun mo ->
          let* ind = mo.Term.m_ind |> Option.fold ~none:(Ok None)
            ~some:(fun n -> Result.map Option.some (name n)) in
          let* body = walk mo.Term.m_body in
          Ok (Some { mo with Term.m_ind = ind; m_body = body })) in
      let* branches = map_list (fun (a, leg) ->
        let* a = map_addr budget level name a in
        let* leg = map_leg budget level name leg in Ok (a, leg)) e.Term.e_branches in
      Ok (Term.Elim { e with Term.e_shape = s; e_scrut = scrut;
        e_motive = motive; e_branches = branches })
  | Term.Let (x, ty, value, body) ->
      let* ty = walk ty in let* value = walk value in let* body = walk body in
      Ok (Term.Let (x, ty, value, body))
  | Term.Ann (body, ty) ->
      let* body = walk body in let* ty = walk ty in Ok (Term.Ann (body, ty))
and map_shape budget level name s =
  let* () = poll budget in
  let walk = map_term budget level name in
  match s with
  | Shape.SPi (q, x, dom) ->
      Result.map (fun dom -> Shape.SPi (q, x, dom)) (walk dom)
  | Shape.SColl n -> Ok (Shape.SColl n)
  | Shape.SPar (a, b) ->
      let* a = walk a in let* b = walk b in Ok (Shape.SPar (a, b))
  | Shape.SMu (n, args) ->
      let* n = name n in let* args = map_list walk args in Ok (Shape.SMu (n, args))
  | Shape.SNu (n, args) ->
      let* n = name n in let* args = map_list walk args in Ok (Shape.SNu (n, args))
and map_addr budget level name addr =
  let* () = poll budget in
  match addr with
  | Term.APt (q, arg) ->
      Result.map (fun arg -> Term.APt (q, arg)) (map_term budget level name arg)
  | Term.ALeg _ | Term.ACtor _ -> Ok addr
and map_leg budget level name leg =
  let* () = poll budget in
  Result.map (fun body -> { leg with Term.l_body = body })
    (map_term budget level name leg.Term.l_body)

let map_family budget level name (family : Check.family_decl) ctors =
  let* () = poll budget in
  let walk = map_term budget level name in
  let telescope = map_list (fun (q, x, ty) ->
    Result.map (fun ty -> q, x, ty) (walk ty)) in
  let* fam_name = name family.fam_name in
  let* fam_level = level family.fam_level in
  let* fam_params = telescope family.fam_params in
  let* fam_indices = telescope family.fam_indices in
  let* ctors = map_list (fun (ctor : Check.ctor_decl) ->
    let* () = poll budget in
    let* ct_args = telescope ctor.ct_args in
    let* ct_res_params = map_list walk ctor.ct_res_params in
    let* ct_res_idx = map_list walk ctor.ct_res_idx in
    Ok { ctor with Check.ct_args; ct_res_params; ct_res_idx }) ctors in
  Ok ({ Check.fam_name; fam_params; fam_indices; fam_level }, ctors)

let unique_ctors budget family ctors =
  let rec loop names = function
    | [] -> Ok ()
    | (ctor : Check.ctor_decl) :: rest ->
        let* () = poll budget in
        if List.mem ctor.ct_name names then
          Error (Error.Mismatch ("the constructor " ^ ctor.ct_name
            ^ " is already declared in " ^ family))
        else loop (ctor.ct_name :: names) rest
  in
  loop [] ctors

let declare ?(budget = Budget.unlimited) globals catalog ~arity
    (family : Check.family_decl) ctors =
  if occupied globals catalog family.fam_name then Error (collision family.fam_name)
  else if arity < 0 then Error (Error.Universe "a universe parameter arity must be nonnegative")
  else
    let* () = unique_ctors budget family.fam_name ctors in
    let name n =
      if List.mem_assoc n catalog then
        Error (Error.Not_yet "references between family schemas are not supported")
      else Ok n in
    let level l = if Level.in_scope arity l then Ok l else Error scope_error in
    let* family, ctors = map_family budget level name family ctors in
    let* () = Check.check_family_scheme globals budget ~arity family ctors in
    Ok ((family.fam_name, { arity; family; ctors }) :: catalog)

let instantiate ?(budget = Budget.unlimited) globals catalog ~name ~levels ~as_name =
  let* scheme = List.assoc_opt name catalog
    |> Option.to_result ~none:(Error.Unbound ("unknown family universe schema " ^ name)) in
  match () with
  | () when occupied globals catalog as_name -> Error (collision as_name)
  | () when List.length levels <> scheme.arity ->
      Error (Error.Universe (Printf.sprintf
        "the family schema %s expects %d universe arguments, got %d"
        name scheme.arity (List.length levels)))
  | () when not (List.for_all (Level.in_scope 0) levels) ->
      Error (Error.Universe "universe arguments must be closed")
  | () ->
      let level l = Level.subst levels l |> Option.to_result ~none:scope_error in
      let rename n = Ok (if String.equal n name then as_name else n) in
      let* family, ctors = map_family budget level rename scheme.family scheme.ctors in
      let* provisional = Check.declare_family ~budget globals family in
      Check.define_ctors ~budget provisional ~group:[as_name] ~name:as_name ctors
