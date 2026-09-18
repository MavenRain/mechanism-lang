open Mechanism_kernel

module Family_poly = Mechanism_surface.Family_poly

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let variable index = Level.var index |> Option.to_result ~none:"invalid universe variable"

let marker : Check.family_decl = {
  fam_name = "Marker"; fam_params = []; fam_indices = []; fam_level = Level.one;
}
let constructor : Check.ctor_decl = {
  ct_name = "mark"; ct_args = []; ct_res_params = []; ct_res_idx = [];
}

(* Application inference ignores the written domain. The raw template
   traversal must still scope-check and specialize every shape payload. *)
let hidden shape : Check.decl =
  let domain = Term.Univ Level.one in
  let pi = Shape.SPi (Quantity.Many, "A", domain) in
  let identity = Term.Ann
    (Term.Sec (pi, [{ Term.l_binders = [Quantity.Many, "A"]; l_body = Term.Var 0 }]),
     Term.Ran (pi, domain)) in
  { d_name = "hidden"; d_kind = Check.Definition; d_ty = domain;
    d_body = Some (Term.Out
      (Shape.SPi (Quantity.Many, "ignored", Term.Lan (shape, Term.Univ Level.zero)),
       Term.APt (Quantity.Many, Term.Univ Level.zero), identity)) }

let declare shape =
  Family_poly.declare ~members:[hidden shape] Global.empty Family_poly.empty
    ~arity:1 marker [constructor]

let payload name level = Term.Ann (Term.Global name, Term.Univ level)
let shapes = [
  "zk-witness", (fun name level -> Shape.SZk (Quantity.Zero, "w", payload name level));
  "fhc-level", (fun name level -> Shape.SFhc (payload name level));
  "mpc-parties", (fun name level -> Shape.SMpc (payload name level, Term.Univ Level.zero));
  "mpc-access", (fun name level -> Shape.SMpc (Term.Univ Level.zero, payload name level));
]

let check_shape make =
  let* u = variable 0 in
  let* catalog = kernel (declare (make "Marker" u)) in
  let* globals = kernel (Family_poly.instantiate Global.empty catalog ~name:"Marker"
    ~levels:[Level.one] ~as_name:"Closed") in
  let* entry = Global.find "Closed_hidden" globals
    |> Option.to_result ~none:"specialized member missing" in
  let* () = match entry with
    | Global.Def d -> require "shape payload did not substitute levels and rename families"
        (Some d.Global.def = (hidden (make "Closed" Level.one)).Check.d_body)
    | Global.Axiom _ | Global.Prim _ -> Error "specialized member is not a definition" in
  let* free = variable 1 in
  Result.fold (declare (make "Marker" free))
    ~ok:(fun _catalog -> Error "accepted a free universe inside a Veil shape")
    ~error:(fun error -> require ("wrong hidden-level refusal: " ^ Error.to_string error)
      (error = Error.Universe "universe level is outside the global parameter scope"))

let rec check_all = function
  | [] -> Ok ()
  | (name, make) :: rest ->
      let* () = Result.map_error (fun message -> name ^ ": " ^ message) (check_shape make) in
      check_all rest

let () =
  Result.fold (check_all shapes)
    ~ok:(fun () -> Printf.printf "VEIL-TEMPLATES OK shapes=%d\n" (List.length shapes))
    ~error:(fun message -> prerr_endline ("VEIL-TEMPLATES FAIL " ^ message); exit 1)
