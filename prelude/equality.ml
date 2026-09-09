open Mechanism_kernel

let ( let* ) = Result.bind

let variable index = Level.var index |> Option.to_result
  ~none:(Error.Universe "a prelude universe parameter must be nonnegative")

let arrow (q, name, domain) body = Term.Ran (Shape.SPi (q, name, domain), body)
let lambda (q, name, domain) body = Term.Sec (Shape.SPi (q, name, domain),
  [{ Term.l_binders = [q, name]; l_body = body }])
let definition name binders result body : Check.decl = {
  d_name = name; d_kind = Check.Definition;
  d_ty = List.fold_right arrow binders result;
  d_body = Some (List.fold_right lambda binders body);
}
let family name params indices = Term.Lan (Shape.SMu (name, indices), Rules.diagram_of params)
let apply domain head argument = Term.Out (Shape.SPi (Quantity.Many, "point", domain),
  Term.APt (Quantity.Many, argument), head)
let eliminate name index motive = Term.Elim {
  e_shape = Shape.SMu (name, [index]); e_scrut = Term.Var 1; e_scrut_q = Quantity.One;
  e_motive = Some { m_ind = Some name; m_idx = ["right"]; m_self = "self"; m_body = motive };
  e_branches = [Term.ACtor "mechReflCtor", { l_binders = []; l_body = Term.Var 0 }];
}

let catalog ?(budget = Budget.unlimited) globals =
  let* u = variable 0 in
  let* v = variable 1 in
  let equality : Check.family_decl = {
    fam_name = "MechEq";
    fam_params = [Quantity.Zero, "A", Term.Univ u; Quantity.Zero, "x", Term.Var 0];
    fam_indices = [Quantity.Zero, "y", Term.Var 1]; fam_level = Level.zero;
  } in
  let reflexivity : Check.ctor_decl = {
    ct_name = "mechReflCtor"; ct_args = [];
    ct_res_params = [Term.Var 1; Term.Var 0]; ct_res_idx = [Term.Var 0];
  } in
  let refl = definition "refl"
    [Quantity.Zero, "A", Term.Univ u; Quantity.Zero, "x", Term.Var 0]
    (family "MechEq" [Term.Var 1; Term.Var 0] [Term.Var 0])
    (Term.In (Shape.SMu ("MechEq", [Term.Var 0]), Term.ACtor "mechReflCtor", [])) in
  let transport = definition "transport"
    [Quantity.Zero, "A", Term.Univ u;
     Quantity.Zero, "B", arrow (Quantity.Many, "point", Term.Var 0) (Term.Univ v);
     Quantity.Zero, "x", Term.Var 1; Quantity.Zero, "y", Term.Var 2;
     Quantity.Many, "proof", family "MechEq" [Term.Var 3; Term.Var 1] [Term.Var 0];
     Quantity.Many, "value", apply (Term.Var 4) (Term.Var 3) (Term.Var 2)]
    (apply (Term.Var 5) (Term.Var 4) (Term.Var 2))
    (eliminate "MechEq" (Term.Var 2) (apply (Term.Var 7) (Term.Var 6) (Term.Var 1))) in
  let* catalog = Mechanism_surface.Family_poly.declare ~budget ~members:[refl; transport]
    globals Mechanism_surface.Family_poly.empty ~arity:2 equality [reflexivity] in
  let type_equality : Check.family_decl = {
    fam_name = "MechTypeEq";
    fam_params = [Quantity.Zero, "A", Term.Univ (Level.succ u)];
    fam_indices = [Quantity.Zero, "B", Term.Univ (Level.succ u)]; fam_level = Level.zero;
  } in
  let type_refl : Check.ctor_decl = {
    ct_name = "mechReflCtor"; ct_args = [];
    ct_res_params = [Term.Var 0]; ct_res_idx = [Term.Var 0];
  } in
  let refl = definition "refl" [Quantity.Zero, "A", Term.Univ (Level.succ u)]
    (family "MechTypeEq" [Term.Var 0] [Term.Var 0])
    (Term.In (Shape.SMu ("MechTypeEq", [Term.Var 0]), Term.ACtor "mechReflCtor", [])) in
  let cast = definition "cast"
    [Quantity.Zero, "A", Term.Univ (Level.succ u);
     Quantity.Zero, "B", Term.Univ (Level.succ u);
     Quantity.Many, "proof", family "MechTypeEq" [Term.Var 1] [Term.Var 0];
     Quantity.Many, "value", Term.Var 2]
    (Term.Var 2) (eliminate "MechTypeEq" (Term.Var 2) (Term.Var 1)) in
  Mechanism_surface.Family_poly.declare ~budget ~members:[refl; cast]
    globals catalog ~arity:1 type_equality [type_refl]
