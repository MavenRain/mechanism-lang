open Kanon_kernel

let ( let* ) = Result.bind

type scheme = {
  arity : int;
  family : Check.family_decl;
  ctors : Check.ctor_decl list;
  companions : (Check.family_decl * Check.ctor_decl list) list;
  members : Check.decl list;
}

type t = (string * scheme) list

let empty : t = []
let arity catalog name = Option.map (fun s -> s.arity) (List.assoc_opt name catalog)
let members catalog name =
  Option.map (fun s -> List.map (fun d -> d.Check.d_name) s.members)
    (List.assoc_opt name catalog)

let companions catalog name =
  Option.map (fun s -> List.map (fun (f, _ctors) -> f.Check.fam_name) s.companions)
    (List.assoc_opt name catalog)

let constructors catalog name =
  Option.map (fun scheme ->
    List.concat_map (fun (_family, ctors) ->
      List.map (fun ctor -> ctor.Check.ct_name) ctors)
      ((scheme.family, scheme.ctors) :: scheme.companions))
    (List.assoc_opt name catalog)

let rename_instance scheme ~name ~as_name n = match () with
  | () when String.equal n name -> as_name
  | () when String.equal n scheme.family.Check.fam_name
      || List.exists (fun (f, _ctors) -> String.equal f.Check.fam_name n)
        scheme.companions -> as_name ^ "_" ^ n
  | () when List.exists (fun d -> String.equal d.Check.d_name n) scheme.members ->
      as_name ^ "_" ^ n
  | () -> n

let instance_names ?(reuse = []) catalog ~name ~as_name =
  Option.map (fun scheme ->
    let rename = rename_instance scheme ~name ~as_name in
    List.filter_map (fun (family, _ctors) ->
      if List.mem_assoc family.Check.fam_name reuse then None
      else Some (rename family.Check.fam_name))
      ((scheme.family, scheme.ctors) :: scheme.companions),
    List.map (fun member -> rename member.Check.d_name) scheme.members)
    (List.assoc_opt name catalog)

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

let map_member budget level name (decl : Check.decl) =
  let* () = poll budget in
  let* () = match decl.d_kind with
    | Check.Definition -> Ok ()
    | Check.Postulate -> Error (Error.Not_yet "family schema members must be definitions") in
  let* d_name = name decl.d_name in
  let* d_ty = map_term budget level name decl.d_ty in
  let* body = decl.d_body |> Option.to_result ~none:(Check.missing_body decl.d_name) in
  let* body = map_term budget level name body in
  Ok { decl with Check.d_name; d_ty; d_body = Some body }

let check_members budget catalog ~arity globals declarations =
  List.fold_left (fun acc (decl : Check.decl) ->
    let* globals = acc in
    let* () = poll budget in
    if occupied globals catalog decl.d_name then Error (collision decl.d_name)
    else
      let* entry = Check.check_decl_at ~arity globals budget decl in
      Ok (Global.add decl.d_name entry globals)) (Ok globals) declarations

let declare ?(budget = Budget.unlimited) ?(members = []) globals catalog ~arity
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
    (* A member named like another template is a collision, not a reference. *)
    let* () = List.find_opt (fun (d : Check.decl) -> List.mem_assoc d.d_name catalog) members
      |> Option.fold ~none:(Ok ()) ~some:(fun (d : Check.decl) -> Error (collision d.d_name)) in
    let* members = map_list (map_member budget level name) members in
    let* () = match members with
      | [] -> Ok ()
      | _member :: _rest ->
          let* provisional = Check.declare_family_at ~arity budget globals family in
          let* symbolic = Check.define_ctors_at ~arity budget provisional
            ~group:[family.fam_name] ~name:family.fam_name ctors in
          let* _checked = check_members budget catalog ~arity symbolic members in
          Ok () in
    Ok ((family.fam_name, { arity; family; ctors; companions = []; members }) :: catalog)

(** A mutual group's recursion flags can differ when rechecked alone. *)
let self_rec_only (actual : Positivity.family) (expected : Positivity.family) =
  let erase (f : Positivity.family) =
    { f with Positivity.f_ctors = List.map
        (fun (c : Positivity.ctor) -> { c with Positivity.c_self_rec = false })
        f.Positivity.f_ctors } in
  erase actual = erase expected

(** Recheck in a temporary table at the caller's universe scope. The complete
    certificate, including binder names, must agree. The table never escapes. *)
let check_reuse ?(arity = 0) budget globals family ctors =
  let name = family.Check.fam_name in
  let* actual = Global.find_family name globals |> Option.to_result
    ~none:(Error.Unbound ("unknown reused family " ^ name)) in
  let temporary = { globals with
    Global.families = Global.StringMap.remove name globals.Global.families } in
  let* provisional = Check.declare_family_at ~arity budget temporary family in
  let* checked = Check.define_ctors_at ~arity budget provisional ~group:[name] ~name ctors in
  let* expected = Global.find_family name checked |> Option.to_result
    ~none:(Error.Cannot_infer "reuse rechecking returned no family") in
  let* () = poll budget in
  (* Family levels use the kernel equality test, including symbolic levels.
     Telescopes retain the conservative structural certificate comparison. *)
  let* level_ok = Level.equal_budget budget
    actual.Positivity.f_level expected.Positivity.f_level in
  let leveled = { expected with Positivity.f_level = actual.Positivity.f_level } in
  match () with
  | () when level_ok && actual = leveled -> Ok globals
  | () when level_ok && self_rec_only actual leveled ->
      Error (Error.Mismatch ("the reused family " ^ name ^
        " is a member of a mutual group, which family reuse does not support"))
  | () -> Error (Error.Mismatch ("the reused family " ^ name ^ " does not match the template"))

let check_bindings budget ~available families reuse =
  let* _bound = List.fold_left (fun acc (local, existing) ->
    let* bound = acc in
    let* () = poll budget in
    match () with
    | () when List.mem local bound ->
        Error (Error.Mismatch ("the family " ^ local ^ " is bound more than once"))
    | () when not (List.exists (fun (family, _ctors) ->
        String.equal family.Check.fam_name local) families) ->
        Error (Error.Unbound ("unknown template family " ^ local))
    | () when not (available existing) ->
        Error (Error.Unbound ("unknown reused family " ^ existing))
    | () -> Ok (local :: bound)) (Ok []) reuse in
  Ok ()

let declare_group_with_reuse ~budget ~members globals catalog ~arity imports =
  let families = List.filter_map (fun (reused, family) ->
    if reused then None else Some family) imports in
  match families with
  | [] -> Error (Error.Mismatch "a family schema group must be nonempty")
  | (family, ctors) :: companions ->
      (* Ordered families may refer to predecessors, never to successors.
         The symbolic environment is discarded after universal checking. *)
      let* symbolic = List.fold_left (fun acc (reused, (family, ctors)) ->
        let* symbolic = acc in
        let* () = poll budget in
        if reused then check_reuse ~arity budget symbolic family ctors
        else
          let* _checked = declare ~budget symbolic catalog ~arity family ctors in
          let* provisional = Check.declare_family_at ~arity budget symbolic family in
          Check.define_ctors_at ~arity budget provisional
            ~group:[family.Check.fam_name] ~name:family.fam_name ctors)
        (Ok globals) imports in
      let name n = if List.mem_assoc n catalog then
          Error (Error.Not_yet "references between family schemas are not supported")
        else Ok n in
      let level l = if Level.in_scope arity l then Ok l else Error scope_error in
      let* _symbolic, reversed = List.fold_left (fun acc elaborate ->
        let* symbolic, reversed = acc in
        let* () = poll budget in
        let* member = elaborate symbolic in
        let* member = map_member budget level name member in
        let* checked = check_members budget catalog ~arity symbolic [member] in
        Ok (checked, member :: reversed)) (Ok (symbolic, [])) members in
      let members = List.rev reversed in
      Ok ((family.fam_name, { arity; family; ctors; companions; members }) :: catalog)

let declare_group_elaborated ?(budget = Budget.unlimited) ~members globals catalog
    ~arity families =
  declare_group_with_reuse ~budget ~members globals catalog ~arity
    (List.map (fun family -> false, family) families)

let declare_group ?(budget = Budget.unlimited) ?(members = []) globals catalog ~arity families =
  declare_group_elaborated ~budget ~members:(List.map (fun member _globals -> Ok member) members)
    globals catalog ~arity families

(** Dependencies contribute raw, renamed syntax.  The ordered group checker
    certifies the complete result under the new universe scope. *)
let compose ?(budget = Budget.unlimited) ~members globals catalog ~arity ~name dependencies =
  let* () = poll budget in
  if occupied globals catalog name then Error (collision name)
  else if arity < 0 then Error (Error.Universe "a universe parameter arity must be nonnegative")
  else
    let* _aliases, _available, groups =
      List.fold_left (fun acc (source, levels, as_name, reuse) ->
      let* aliases, available, groups = acc in
      let* () = poll budget in
      let* scheme = List.assoc_opt source catalog |> Option.to_result
        ~none:(Error.Unbound ("unknown family universe schema " ^ source)) in
      match () with
      | () when String.equal as_name name || List.mem as_name aliases
          || occupied globals catalog as_name -> Error (collision as_name)
      | () when List.length levels <> scheme.arity ->
          Error (Error.Universe (Printf.sprintf
            "the family schema %s expects %d universe arguments, got %d"
            source scheme.arity (List.length levels)))
      | () when not (List.for_all (Level.in_scope arity) levels) -> Error scope_error
      | () ->
          let raw_families = (scheme.family, scheme.ctors) :: scheme.companions in
          let* () = check_bindings budget raw_families reuse ~available:(fun existing ->
            List.mem existing available || Option.is_some (Global.find_family existing globals)) in
          let level l = Level.subst levels l |> Option.to_result ~none:scope_error in
          let rename n = Ok (List.assoc_opt n reuse |> Option.value
            ~default:(rename_instance scheme ~name:source ~as_name n)) in
          let* families = map_list (fun (family, ctors) ->
            let* mapped = map_family budget level rename family ctors in
            Ok (List.mem_assoc family.Check.fam_name reuse, mapped)) raw_families in
          let* definitions = map_list (map_member budget level rename) scheme.members in
          let fresh = List.filter_map (fun (reused, (family, _ctors)) ->
            if reused then None else Some family.Check.fam_name) families in
          let generated = fresh @ List.map (fun definition -> definition.Check.d_name) definitions in
          Ok (as_name :: generated @ aliases, fresh @ available,
            (families, definitions) :: groups)) (Ok ([], [], [])) dependencies in
    let groups = List.rev groups in
    let families = List.concat_map fst groups in
    let definitions = List.concat_map snd groups in
    let* () = if List.exists (fun (_reused, (family, _ctors)) ->
        String.equal name family.Check.fam_name) families then Error (collision name)
      else Ok () in
    let members = List.map (fun member _globals -> Ok member) definitions @ members in
    let members = List.map (fun elaborate symbolic ->
      let* declaration = elaborate symbolic in
      if String.equal declaration.Check.d_name name then Error (collision name)
      else Ok declaration) members in
    match List.filter_map (fun (reused, family) -> if reused then None else Some family) families with
    | [] -> Error (Error.Mismatch "a family schema group must be nonempty")
    | (family, _ctors) :: _rest ->
        let* checked =
          declare_group_with_reuse ~budget ~members globals catalog ~arity families in
        let* scheme = List.assoc_opt family.Check.fam_name checked
          |> Option.to_result ~none:(Error.Cannot_infer "composition returned no schema") in
        Ok ((name, scheme) :: catalog)

(** SC-L1-2, review round 2:  a closed level argument that is not normal,
    like (max 0 0), reaches the parameter, the index and the constructor
    telescopes through Level.subst, where the certificate match compares
    the terms as written.  The normal form of a closed level argument is
    the natural number the semantic test accepts, so the search asks the
    kernel test for each candidate and keeps the first answer.  A level
    above the search bound keeps its written form and refuses as before,
    which stays conservative. *)
let normal_level (level : Level.t) : Level.t =
  List.init 64 Level.of_int
  |> List.find_map (fun candidate ->
    Option.bind candidate (fun written ->
      match () with
      | () when Level.equal level written -> Some written
      | () -> None))
  |> Option.value ~default:level

let instantiate ?(budget = Budget.unlimited) ?(reuse = []) globals catalog ~name ~levels ~as_name =
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
      let* () = poll budget in
      let levels = List.map normal_level levels in
      let raw_families = (scheme.family, scheme.ctors) :: scheme.companions in
      let* () = check_bindings budget raw_families reuse
        ~available:(fun existing -> Option.is_some (Global.find_family existing globals)) in
      let level l = Level.subst levels l |> Option.to_result ~none:scope_error in
      let rename n = Ok (List.assoc_opt n reuse |> Option.value
        ~default:(rename_instance scheme ~name ~as_name n)) in
      let* families = map_list (fun (family, ctors) ->
        let* mapped = map_family budget level rename family ctors in
        Ok (List.mem_assoc family.Check.fam_name reuse, mapped)) raw_families in
      let* members = map_list (map_member budget level rename) scheme.members in
      let* installed = List.fold_left (fun acc (reused, (family, ctors)) ->
        let* globals = acc in
        let* () = poll budget in
        if reused then
          check_reuse budget globals family ctors
        else if occupied globals catalog family.Check.fam_name then Error (collision family.fam_name)
        else
          let* provisional = Check.declare_family ~budget globals family in
          Check.define_ctors ~budget provisional ~group:[family.fam_name]
            ~name:family.fam_name ctors) (Ok globals) families in
      check_members budget catalog ~arity:0 installed members
