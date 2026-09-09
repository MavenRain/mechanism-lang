(** Programmatic prenex prelude families.  MechEq takes the carrier's Sort
    level; MechSum takes the two Type levels, before their successors.
    These templates are checked by the kernel and do not enter globals.
    Source syntax and imported source-type parity remain separate work. *)
val catalog : ?budget:Mechanism_kernel.Budget.t -> Mechanism_kernel.Global.t ->
  (Mechanism_surface.Family_poly.t, Mechanism_kernel.Error.t) result
