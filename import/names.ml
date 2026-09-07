type t = Anonymous | Str of int * string | Num of int * int
let ( let* ) = Result.bind

let parse tag value =
  if tag = "str" then
    let* fields = Ndjson.exact_object ["pre"; "str"] value in
    let* pre = Ndjson.nat_field "pre" fields in
    let* text = Ndjson.string_field "str" fields in
    Ok (Str (pre, text))
  else if tag = "num" then
    let* fields = Ndjson.exact_object ["pre"; "i"] value in
    let* pre = Ndjson.nat_field "pre" fields in
    let* n = Ndjson.nat_field "i" fields in
    Ok (Num (pre, n))
  else Error ("unknown name tag " ^ tag)

let references = function
  | Anonymous -> []
  | Str (pre, _) | Num (pre, _) -> [pre]
