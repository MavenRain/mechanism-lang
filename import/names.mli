type t = Anonymous | Str of int * string | Num of int * int
val parse : string -> Ndjson.t -> (t, string) result
val references : t -> int list
