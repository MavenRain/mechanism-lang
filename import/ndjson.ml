type t = Null | Bool of bool | Number of string | String of string | Array of t list | Object of (string * t) list

let ( let* ) = Result.bind

type cursor = { rest : char list; column : int }

let advance rest c = { rest; column = c.column + 1 }
let fail c message = Error (Printf.sprintf "JSON column %d: %s" c.column message)
let digit c = c >= '0' && c <= '9'

let rec whitespace c =
  match c.rest with
  | [] -> c
  | ch :: rest ->
      if ch = ' ' || ch = '\t' || ch = '\r' || ch = '\n' then
        whitespace (advance rest c)
      else c

let consume expected c =
  match c.rest with
  | [] -> fail c (Printf.sprintf "expected '%c'" expected)
  | ch :: rest ->
      if ch = expected then Ok (advance rest c)
      else fail c (Printf.sprintf "expected '%c'" expected)

let hex c =
  match () with
  | () when c >= '0' && c <= '9' -> Some (Char.code c - Char.code '0')
  | () when c >= 'a' && c <= 'f' -> Some (Char.code c - Char.code 'a' + 10)
  | () when c >= 'A' && c <= 'F' -> Some (Char.code c - Char.code 'A' + 10)
  | () -> None

let rec hex_quad remaining acc c =
  if remaining = 0 then Ok (acc, c)
  else match c.rest with
  | [] -> fail c "incomplete Unicode escape"
  | ch :: rest ->
      let* n = Option.to_result ~none:("JSON column " ^ string_of_int c.column ^ ": invalid Unicode escape") (hex ch) in
      hex_quad (remaining - 1) ((acc * 16) + n) (advance rest c)

let unicode_escape c =
  let* first, c = hex_quad 4 0 c in
  if first >= 0xd800 && first <= 0xdbff then
    let* c = consume '\\' c in
    let* c = consume 'u' c in
    let* second, c = hex_quad 4 0 c in
    if second < 0xdc00 || second > 0xdfff then fail c "invalid low surrogate"
    else Ok (0x10000 + ((first - 0xd800) * 1024) + second - 0xdc00, c)
  else if first >= 0xdc00 && first <= 0xdfff then fail c "unpaired low surrogate"
  else Ok (first, c)

(* A total byte lookup avoids partial character conversion in UTF-8 encoding. *)
let byte_values = List.of_seq (String.to_seq "\000\001\002\003\004\005\006\007\008\009\010\011\012\013\014\015\016\017\018\019\020\021\022\023\024\025\026\027\028\029\030\031\032\033\034\035\036\037\038\039\040\041\042\043\044\045\046\047\048\049\050\051\052\053\054\055\056\057\058\059\060\061\062\063\064\065\066\067\068\069\070\071\072\073\074\075\076\077\078\079\080\081\082\083\084\085\086\087\088\089\090\091\092\093\094\095\096\097\098\099\100\101\102\103\104\105\106\107\108\109\110\111\112\113\114\115\116\117\118\119\120\121\122\123\124\125\126\127\128\129\130\131\132\133\134\135\136\137\138\139\140\141\142\143\144\145\146\147\148\149\150\151\152\153\154\155\156\157\158\159\160\161\162\163\164\165\166\167\168\169\170\171\172\173\174\175\176\177\178\179\180\181\182\183\184\185\186\187\188\189\190\191\192\193\194\195\196\197\198\199\200\201\202\203\204\205\206\207\208\209\210\211\212\213\214\215\216\217\218\219\220\221\222\223\224\225\226\227\228\229\230\231\232\233\234\235\236\237\238\239\240\241\242\243\244\245\246\247\248\249\250\251\252\253\254\255")

let add_scalar buffer scalar =
  let bytes =
    match () with
    | () when scalar <= 0x7f -> [scalar]
    | () when scalar <= 0x7ff ->
        [0xc0 lor (scalar lsr 6); 0x80 lor (scalar land 0x3f)]
    | () when scalar <= 0xffff ->
        [0xe0 lor (scalar lsr 12); 0x80 lor ((scalar lsr 6) land 0x3f);
         0x80 lor (scalar land 0x3f)]
    | () ->
        [0xf0 lor (scalar lsr 18); 0x80 lor ((scalar lsr 12) land 0x3f);
         0x80 lor ((scalar lsr 6) land 0x3f); 0x80 lor (scalar land 0x3f)]
  in
  let rec add = function
    | [] -> Ok ()
    | n :: rest ->
        let* byte = Option.to_result ~none:"invalid Unicode byte" (List.nth_opt byte_values n) in
        Buffer.add_char buffer byte;
        add rest
  in
  add bytes

let parse_string c =
  let* c = consume '"' c in
  let buffer = Buffer.create 32 in
  let rec chars c =
    match c.rest with
    | [] -> fail c "unterminated string"
    | ch :: rest ->
        let next = advance rest c in
        (match () with
        | () when ch = '"' ->
            let value = Buffer.contents buffer in
            if String.is_valid_utf_8 value then Ok (value, next)
            else fail c "invalid UTF-8 in string"
        | () when ch = '\\' -> escaped next
        | () when Char.code ch < 0x20 -> fail c "unescaped control character"
        | () -> Buffer.add_char buffer ch; chars next)
  and escaped c =
    match c.rest with
    | [] -> fail c "incomplete escape"
    | ch :: rest ->
        let next = advance rest c in
        if ch = 'u' then
          let* scalar, next = unicode_escape next in
          let* () = add_scalar buffer scalar in
          chars next
        else
          let decoded = List.assoc_opt ch
            ['"', '"'; '\\', '\\'; '/', '/'; 'b', '\b'; 'f', '\012';
             'n', '\n'; 'r', '\r'; 't', '\t'] in
          let* decoded = Option.to_result ~none:"invalid JSON escape" decoded in
          Buffer.add_char buffer decoded;
          chars next
  in
  chars c

let parse_number c =
  let buffer = Buffer.create 24 in
  let take ch rest c = Buffer.add_char buffer ch; advance rest c in
  let rec digits c =
    match c.rest with
    | [] -> c
    | ch :: rest -> if digit ch then digits (take ch rest c) else c
  in
  let required_digits c =
    match c.rest with
    | [] -> fail c "expected digit"
    | ch :: rest ->
        if digit ch then Ok (digits (take ch rest c)) else fail c "expected digit"
  in
  let signed c =
    match c.rest with
    | [] -> c
    | ch :: rest -> if ch = '-' then take ch rest c else c
  in
  let integer c =
    match c.rest with
    | [] -> fail c "expected number"
    | ch :: rest ->
        if ch = '0' then Ok (take ch rest c)
        else if ch >= '1' && ch <= '9' then Ok (digits (take ch rest c))
        else fail c "expected number"
  in
  let fraction c =
    match c.rest with
    | [] -> Ok c
    | ch :: rest ->
        if ch = '.' then required_digits (take ch rest c) else Ok c
  in
  let exponent c =
    match c.rest with
    | [] -> Ok c
    | ch :: rest ->
        if ch = 'e' || ch = 'E' then
          let next = take ch rest c in
          let next =
            match next.rest with
            | [] -> next
            | sign :: tail ->
                if sign = '+' || sign = '-' then take sign tail next else next
          in
          required_digits next
        else Ok c
  in
  let* c = integer (signed c) in
  let* c = fraction c in
  let* c = exponent c in
  Ok (Number (Buffer.contents buffer), c)

let rec literal expected value c =
  match expected with
  | [] -> Ok (value, c)
  | ch :: rest -> let* c = consume ch c in literal rest value c

let parse source =
  let rec value depth c =
    let c = whitespace c in
    if depth > 512 then fail c "nesting exceeds 512 levels"
    else match c.rest with
    | [] -> fail c "expected value"
    | ch :: rest ->
        (match () with
        | () when ch = '"' ->
            let* text, next = parse_string c in Ok (String text, next)
        | () when ch = '{' -> object_start (depth + 1) (advance rest c)
        | () when ch = '[' -> array_start (depth + 1) (advance rest c)
        | () when ch = 't' -> literal ['t'; 'r'; 'u'; 'e'] (Bool true) c
        | () when ch = 'f' -> literal ['f'; 'a'; 'l'; 's'; 'e'] (Bool false) c
        | () when ch = 'n' -> literal ['n'; 'u'; 'l'; 'l'] Null c
        | () when ch = '-' || digit ch -> parse_number c
        | () -> fail c "unexpected character")
  and array_start depth c =
    let c = whitespace c in
    match c.rest with
    | [] -> fail c "unterminated array"
    | ch :: rest ->
        if ch = ']' then Ok (Array [], advance rest c)
        else array_items depth [] c
  and array_items depth reversed c =
    let* item, c = value depth c in
    let c = whitespace c in
    match c.rest with
    | [] -> fail c "unterminated array"
    | ch :: rest ->
        if ch = ']' then Ok (Array (List.rev (item :: reversed)), advance rest c)
        else if ch = ',' then array_items depth (item :: reversed) (advance rest c)
        else fail c "expected ',' or ']'"
  and object_start depth c =
    let c = whitespace c in
    match c.rest with
    | [] -> fail c "unterminated object"
    | ch :: rest ->
        if ch = '}' then Ok (Object [], advance rest c)
        else object_items depth (Hashtbl.create 8) [] c
  and object_items depth keys reversed c =
    let* key, c = parse_string (whitespace c) in
    if Hashtbl.mem keys key then fail c ("duplicate object key " ^ key)
    else (
      Hashtbl.add keys key ();
      let* c = consume ':' (whitespace c) in
      let* item, c = value depth c in
      let c = whitespace c in
      match c.rest with
      | [] -> fail c "unterminated object"
      | ch :: rest ->
          if ch = '}' then Ok (Object (List.rev ((key, item) :: reversed)), advance rest c)
          else if ch = ',' then object_items depth keys ((key, item) :: reversed) (advance rest c)
          else fail c "expected ',' or '}'")
  in
  let* parsed, c = value 0 { rest = List.of_seq (String.to_seq source); column = 1 } in
  match (whitespace c).rest with
  | [] -> Ok parsed
  | _ch :: _tail -> fail c "trailing content"

let quoted text =
  let buffer = Buffer.create (String.length text + 2) in
  Buffer.add_char buffer '"';
  String.iter (fun ch ->
    match () with
    | () when ch = '"' -> Buffer.add_string buffer "\\\""
    | () when ch = '\\' -> Buffer.add_string buffer "\\\\"
    | () when Char.code ch < 0x20 ->
        Buffer.add_string buffer (Printf.sprintf "\\u%04x" (Char.code ch))
    | () -> Buffer.add_char buffer ch) text;
  Buffer.add_char buffer '"';
  Buffer.contents buffer

let rec to_string = function
  | Null -> "null"
  | Bool value -> if value then "true" else "false"
  | Number value -> value
  | String value -> quoted value
  | Array values -> "[" ^ String.concat "," (List.map to_string values) ^ "]"
  | Object fields ->
      "{" ^ String.concat "," (List.map (fun (key, value) -> quoted key ^ ":" ^ to_string value) fields) ^ "}"

let object_fields = function
  | Object fields -> Ok fields
  | Null | Bool _ | Number _ | String _ | Array _ -> Error "expected object"

let exact_object expected value =
  let* fields = object_fields value in
  if List.length fields = List.length expected &&
     List.for_all (fun key -> List.mem_assoc key fields) expected then Ok fields
  else Error ("expected exactly fields " ^ String.concat "," expected)

let field key fields =
  Option.to_result ~none:("missing field " ^ key) (List.assoc_opt key fields)

let string = function
  | String value -> Ok value
  | Null | Bool _ | Number _ | Array _ | Object _ -> Error "expected string"

let nat = function
  | Number text ->
      if String.length text > 0 && String.for_all digit text then
        Option.to_result ~none:"natural index exceeds host integer range" (int_of_string_opt text)
      else Error "expected natural integer"
  | Null | Bool _ | String _ | Array _ | Object _ -> Error "expected natural integer"

let bool = function
  | Bool value -> Ok value
  | Null | Number _ | String _ | Array _ | Object _ -> Error "expected boolean"

let array = function
  | Array values -> Ok values
  | Null | Bool _ | Number _ | String _ | Object _ -> Error "expected array"

let traverse fn values =
  let rec go reversed = function
    | [] -> Ok (List.rev reversed)
    | value :: rest -> let* mapped = fn value in go (mapped :: reversed) rest
  in
  go [] values

let list fn value = let* values = array value in traverse fn values
let string_field key fields = let* value = field key fields in string value
let nat_field key fields = let* value = field key fields in nat value
let bool_field key fields = let* value = field key fields in bool value
let nats_field key fields = let* value = field key fields in list nat value
