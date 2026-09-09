open Mechanism_kernel

let ( let* ) = Result.bind

let variable index =
  Level.var index |> Option.to_result
    ~none:(Error.Universe "a prelude universe parameter must be nonnegative")

let catalog ?(budget = Budget.unlimited) globals =
  let* u = variable 0 in
  let* v = variable 1 in
  let equality : Check.family_decl = {
    fam_name = "MechEq";
    fam_params = [ Quantity.Zero, "A", Term.Univ u;
                   Quantity.Zero, "x", Term.Var 0 ];
    fam_indices = [ Quantity.Zero, "y", Term.Var 1 ];
    fam_level = Level.zero;
  } in
  let reflexivity : Check.ctor_decl = {
    ct_name = "mechReflCtor"; ct_args = [];
    ct_res_params = [ Term.Var 1; Term.Var 0 ];
    ct_res_idx = [ Term.Var 0 ];
  } in
  let* catalog = Mechanism_surface.Family_poly.declare ~budget globals
      Mechanism_surface.Family_poly.empty ~arity:1 equality [ reflexivity ] in
  let sum : Check.family_decl = {
    fam_name = "MechSum";
    fam_params = [ Quantity.Zero, "A", Term.Univ (Level.succ u);
                   Quantity.Zero, "B", Term.Univ (Level.succ v) ];
    fam_indices = [];
    fam_level = Level.max (Level.succ u) (Level.succ v);
  } in
  let left : Check.ctor_decl = {
    ct_name = "mechInl"; ct_args = [ Quantity.Many, "left", Term.Var 1 ];
    ct_res_params = [ Term.Var 2; Term.Var 1 ]; ct_res_idx = [];
  } in
  let right : Check.ctor_decl = {
    ct_name = "mechInr"; ct_args = [ Quantity.Many, "right", Term.Var 0 ];
    ct_res_params = [ Term.Var 2; Term.Var 1 ]; ct_res_idx = [];
  } in
  Mechanism_surface.Family_poly.declare ~budget globals catalog ~arity:2 sum
    [ left; right ]
