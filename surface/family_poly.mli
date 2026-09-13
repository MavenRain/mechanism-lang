open Kanon_kernel

(** Universally checked templates for recursive families. Templates stay
    outside ordinary globals and cannot refer to other templates. *)
type t

val empty : t
val arity : t -> string -> int option
val members : t -> string -> string list option
val companions : t -> string -> string list option

(** The result universe must always be Prop or always be Type. Constructor
    names are unique within the family and retain their names in instances.
    Members are definitions checked in order under the symbolic family.
    They may refer to that family and earlier members.  They share its
    universe scope and stay outside the caller's ordinary globals.  A member
    named like a catalog template is refused as a collision.  Members are
    traversed first and kernel checked after, so a traversal refusal of a
    later member precedes a kernel refusal of an earlier one. *)
val declare : ?budget:Budget.t -> ?members:Check.decl list -> Global.t -> t -> arity:int ->
  Check.family_decl -> Check.ctor_decl list -> (t, Error.t) result

(** A nonempty ordered group shares one universe scope. Each family may
    refer to earlier families; mutual recursion and forward references are
    refused. Members check after all families. The first family names the
    template and specializes to [as_name]; subsequent families specialize
    to [as_name ^ "_" ^ family_name]. All references are renamed together.
    No symbolic entry escapes and specialization is atomic on failure.
    Members are traversed and kernel checked one at a time in order, so the
    first member that fails either step names the refusal.  That precedence
    differs from [declare], which traverses every member first. *)
val declare_group : ?budget:Budget.t -> ?members:Check.decl list -> Global.t -> t ->
  arity:int -> (Check.family_decl * Check.ctor_decl list) list -> (t, Error.t) result

(** Elaborate ordered members against the checked families and preceding
    checked members.  Each callback returns raw syntax, which is scope
    checked and kernel checked before the next callback runs.  Callbacks
    cannot certify entries or replace the environment.  The catalog is
    returned only after every member succeeds; no callback runs for an
    empty or invalid family group or after an earlier failure.  The first
    member that fails either step names the refusal. *)
val declare_group_elaborated : ?budget:Budget.t ->
  members:(Global.t -> (Check.decl, Error.t) result) list -> Global.t -> t ->
  arity:int -> (Check.family_decl * Check.ctor_decl list) list -> (t, Error.t) result

(** Specialize all universe occurrences and self references using exactly
    the declared number of closed levels. Recheck the closed family and
    constructors before returning globals with the fresh instance installed.
    Each member is installed as [as_name ^ "_" ^ member_name], with every
    family and member reference renamed.  Definitions are rechecked in
    order and name collisions are rejected.  Failure leaves both immutable
    input values unchanged. *)
val instantiate : ?budget:Budget.t -> Global.t -> t -> name:string ->
  levels:Level.t list -> as_name:string -> (Global.t, Error.t) result
