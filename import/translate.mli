open Mechanism_kernel

(** A shared type DAG.  Source identities and named universe parameters are
    retained until a declaration supplies its parameter order.  This table
    is scoped source syntax, not a claim of kernel or prelude parity.
    Parameter identities use canonical source names, so aliases of the
    same structural name share a binder.  Source declaration records and
    level node IDs still identify the exact original records. *)
type level =
  | Zero
  | Succ of int
  | Max of int * int
  | Imax of int * int
  | Param of { source_name : int; name : string }

type binder = {
  name : string;
  typ : int;
  body : int;
  info : Exprs.binder_info;
}

type node =
  | Bound of int
  | Sort of int
  | Constant of { source_name : int; name : string; universes : int list }
  | Apply of int * int
  | Lambda of binder
  | Pi of binder
  | Let of { name : string; typ : int; value : int; body : int; nondep : bool }
  | Projection of { source_type_name : int; type_name : string; index : int; structure : int }
  | Nat of string
  | String of string
  | Metadata of { expr : int; data : (string * Ndjson.t) list }

type row = {
  declaration : Decls.t;
  name : string;
  parameters : (int * string) list;
  root : int;
  dependencies : string list;
}

type t
val translate : Export.t -> (t, Export.error) result
val rows : t -> row list
val nodes : t -> (int * node) list
val levels : t -> (int * level) list

type lowering_error =
  | Deferred of string
  | Unsupported of string
  | Kernel_error of Error.t

val lowering_message : lowering_error -> string

(** The resolver receives the exact universe instantiation, with the row's
    named parameters replaced by its prenex positions.  It must resolve to
    a term in the supplied checked environment.  Every result is checked
    as a type before success; no declaration is installed by this function.
    Absent mappings and unsupported nodes remain explicit errors.  The
    caller must establish mapping equivalence separately; successful kernel
    checking does not establish the resolver's source correspondence.

    Lean binders lower at quantity Many.  Implicitness does not imply an
    erasure policy.  Depth is limited to 1024 and expression plus universe
    visits to 100000.  With no supplied budget, kernel and level work also
    share a limit of 100000 polls. *)
val lower_type : ?budget:Budget.t ->
  resolve:(name:string -> levels:Level.t list -> (Term.t, string) result) ->
  Global.t -> t -> row -> (Term.t, lowering_error) result
