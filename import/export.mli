type error = { line : int; message : string }
type metadata = {
  exporter_name : string; exporter_version : string;
  lean_githash : string; lean_version : string; format_version : string;
}
type counts = { lines : int; names : int; levels : int; exprs : int; declarations : int }
type t
(** Format 3.1.0 only.  Each physical line must be one object.  Only one
    trailing newline is accepted, and no other blank line.  The initial
    metadata is mandatory.  Indices are nonnegative host integers
    and every referenced table entry must already exist. *)
val read_string : string -> (t, error) result
val read_file : string -> (t, error) result
val metadata : t -> metadata
(** Counts exclude the implicit anonymous name and zero level. *)
val counts : t -> counts
val name : t -> int -> Names.t option
(** Includes the implicit anonymous name, in ascending index order. *)
val names : t -> (int * Names.t) list
(** An injective display path.  Ambiguous components use quoted JSON text.
    Exact structural names remain available through [name] and [names]. *)
val name_string : t -> int -> string option
(** The first name index with the same structural name. *)
val canonical_name : t -> int -> int option
val level : t -> int -> Levels.t option
val expr : t -> int -> Exprs.t option
val expr_line : t -> int -> int option
val expressions : t -> (int * Exprs.t) list
(** Lookup by source name index, including aliases of that name. *)
val declaration : t -> int -> Decls.t option
val declarations : t -> Decls.t list
val groups : t -> Decls.group list
