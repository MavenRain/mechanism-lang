open Kanon_kernel

(** Universally checked templates for one recursive family. Templates stay
    outside ordinary globals and cannot refer to other templates. *)
type t

val empty : t
val arity : t -> string -> int option
val members : t -> string -> string list option

(** The result universe must always be Prop or always be Type. Constructor
    names are unique within the family and retain their names in instances.
    Members are definitions checked in order under the symbolic family.
    They may refer to that family and earlier members.  They share its
    universe scope and stay outside the caller's ordinary globals.  A member
    named like a catalog template is refused as a collision. *)
val declare : ?budget:Budget.t -> ?members:Check.decl list -> Global.t -> t -> arity:int ->
  Check.family_decl -> Check.ctor_decl list -> (t, Error.t) result

(** Specialize all universe occurrences and self references using exactly
    the declared number of closed levels. Recheck the closed family and
    constructors before returning globals with the fresh instance installed.
    Each member is installed as [as_name ^ "_" ^ member_name], with every
    family and member reference renamed.  Definitions are rechecked in
    order and name collisions are rejected.  Failure leaves both immutable
    input values unchanged. *)
val instantiate : ?budget:Budget.t -> Global.t -> t -> name:string ->
  levels:Level.t list -> as_name:string -> (Global.t, Error.t) result
