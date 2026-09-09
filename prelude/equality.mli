(** Universally checked equality operations, without postulates.
    MechEq takes carrier and motive Sort levels and includes refl and
    transport.  MechTypeEq takes a Type level and includes refl and cast.
    Instantiate through Family_poly; the member names gain the instance
    name and an underscore.  These are separate from Families.catalog. *)
val catalog : ?budget:Mechanism_kernel.Budget.t -> Mechanism_kernel.Global.t ->
  (Mechanism_surface.Family_poly.t, Mechanism_kernel.Error.t) result
