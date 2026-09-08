(** Stage C inventory of referenced external constants.  A target names a
    candidate only: this module never claims type parity.  Stage D must
    resolve targets and recompute verdicts against translated export types. *)
type verdict = Name_only | Unmapped | Never
type row = { lean_name : string; target : string option; verdict : verdict }
type exception_row = { lean_name : string; ratification : string; milestone : string }
type t = {
  rows : row list;
  never : exception_row list;
  external_declared : int;
  const_names : int;
}

(** Rows are unique and sorted by their exact source display name.  The
    denominator uses every const node, including nodes outside declaration
    types.  Declared but unreferenced names remain in [external_declared].
    Targets must have unique, referenced external source names and nonempty
    single-line TSV cells.  A target for a ratified NEVER name is refused.
    Only the exact [Lean.Omega] namespace receives R-V4's omega-reflection
    exception; private helpers and other Lean names are not inferred. *)
val inventory : ?targets:(string * string) list -> Export.t -> (t, string) result
(** Foundation candidates present in this source's referenced inventory.
    These names do not certify loading, universe generality, or type parity. *)
val foundation_targets : Export.t -> (string * string) list
val verdict_name : verdict -> string
val map_tsv : t -> string
val never_tsv : t -> string
