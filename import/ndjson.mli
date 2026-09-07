type t = Null | Bool of bool | Number of string | String of string | Array of t list | Object of (string * t) list
(** Rejects duplicate keys, invalid UTF-8, unpaired surrogates, trailing input,
    and JSON nesting deeper than 512 levels. *)
val parse : string -> (t, string) result
(** Encodes validated JSON values.  Constructed [Number] values must contain
    a valid JSON number and constructed strings must contain valid UTF-8. *)
val to_string : t -> string
val object_fields : t -> ((string * t) list, string) result
val exact_object : string list -> t -> ((string * t) list, string) result
val field : string -> (string * t) list -> (t, string) result
val string : t -> (string, string) result
val nat : t -> (int, string) result
val bool : t -> (bool, string) result
val array : t -> (t list, string) result
val list : (t -> ('a, string) result) -> t -> ('a list, string) result
val traverse : ('a -> ('b, string) result) -> 'a list -> ('b list, string) result
val string_field : string -> (string * t) list -> (string, string) result
val nat_field : string -> (string * t) list -> (int, string) result
val bool_field : string -> (string * t) list -> (bool, string) result
val nats_field : string -> (string * t) list -> (int list, string) result
