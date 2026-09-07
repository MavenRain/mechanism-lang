type t = Zero | Succ of int | Max of int * int | Imax of int * int | Param of int
val parse : string -> Ndjson.t -> (t, string) result
val name_references : t -> int list
val level_references : t -> int list
