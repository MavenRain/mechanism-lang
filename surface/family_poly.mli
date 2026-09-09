open Kanon_kernel

(** Universally checked templates for one recursive family. Templates stay
    outside ordinary globals and cannot refer to other templates. *)
type t

val empty : t
val arity : t -> string -> int option

(** The result universe must always be Prop or always be Type. Constructor
    names are unique within the family and retain their names in instances. *)
val declare : ?budget:Budget.t -> Global.t -> t -> arity:int ->
  Check.family_decl -> Check.ctor_decl list -> (t, Error.t) result

(** Specialize all universe occurrences and self references using exactly
    the declared number of closed levels. Recheck the closed family and
    constructors before returning globals with the fresh instance installed.
    Failure leaves both immutable input values unchanged. *)
val instantiate : ?budget:Budget.t -> Global.t -> t -> name:string ->
  levels:Level.t list -> as_name:string -> (Global.t, Error.t) result
