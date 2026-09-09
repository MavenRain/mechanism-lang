open Mechanism_kernel

let ( let* ) = Result.bind
let require message condition = if condition then Ok () else Error message
let kernel result = Result.map_error Error.to_string result
let read path =
  Mechanism_import.Io.attempt (fun () -> In_channel.with_open_bin path In_channel.input_all)

let audit (globals : Global.t) =
  let* () = Global.StringMap.fold (fun name entry acc ->
      let* () = acc in
      match entry with
      | Global.Def _ -> Ok ()
      | Global.Axiom _ | Global.Prim _ -> Error ("unexpected trusted entry: " ^ name))
      globals.entries (Ok ()) in
  Global.StringMap.fold (fun name (family : Positivity.family) acc ->
      let* () = acc in
      match family.f_status with
      | Positivity.Complete _ -> require ("unchecked family: " ^ name) family.f_positive
      | Positivity.Provisional | Positivity.Builtin -> Error ("unexpected family: " ^ name))
      globals.families (Ok ())

let refusal expected message result =
  Result.fold result ~ok:(fun _checked -> Error message)
    ~error:(fun actual ->
      require ("wrong refusal: " ^ Error.to_string actual)
        (String.starts_with ~prefix:expected (Error.to_string actual)))

let endpoint_text =
  "mismatch: the constructor mechReflCtor of PolyEqData gives the index "
  ^ "(In SMu MechNat [] (ACtor mechZero) []) and the type asks for "
  ^ "(In SMu MechNat [] (ACtor mechSucc) [(In SMu MechNat [] (ACtor mechZero) [])])"

let sum_arguments =
  "(Sec SColl 2 [ => (Lan SMu MechNat [] (Sec SColl 0 []));  => (Lan SMu MechUnit [] (Sec SColl 0 []))])"

let instance_text =
  "mismatch: the term has type (Lan SMu PolySumLow [] " ^ sum_arguments
  ^ ") and the expected type is (Lan SMu MechSum [] " ^ sum_arguments ^ ")"

let suite root =
  let* catalog = kernel (Mechanism_prelude.Families.catalog Global.empty) in
  let* () = require "prelude catalog arities changed"
      (Mechanism_surface.Family_poly.arity catalog "MechEq" = Some 1
       && Mechanism_surface.Family_poly.arity catalog "MechSum" = Some 2) in
  let* source = read (Filename.concat root "prelude/init.mech") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty source) in
  let instances = [
    "MechEq", [ Level.one ], "PolyEqData";
    "MechEq", [ Level.succ Level.one ], "PolyEqTypes";
    "MechEq", [ Level.succ (Level.succ Level.one) ], "PolyEqHigher";
    "MechSum", [ Level.zero; Level.zero ], "PolySumLow";
    "MechSum", [ Level.zero; Level.one ], "PolySumMixed";
  ] in
  let* installed = List.fold_left (fun acc (name, levels, as_name) ->
      let* globals = acc in
      kernel (Mechanism_surface.Family_poly.instantiate globals catalog
        ~name ~levels ~as_name)) (Ok globals) instances in
  let* () = audit installed in
  let* client = read (Filename.concat root "test/fixtures/prelude/polymorphic.mech") in
  let* checked, _rows = kernel (Mechanism_surface.Elab.check_in installed client) in
  let* () = audit checked in
  let wrong = "def polyWrong : PolyEqData MechNat mechZero (mechSucc mechZero) := mechReflCtor\n" in
  let* () = refusal endpoint_text
      "unequal endpoints accepted in an instantiated family"
      (Mechanism_surface.Elab.check_in installed wrong) in
  let wrong_instance = "def polyWrongInstance : MechSum MechNat MechUnit := polyLowLeft\n" in
  refusal instance_text "distinct family instances became convertible"
    (Mechanism_surface.Elab.check_in checked wrong_instance)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun () -> Printf.printf "PRELUDE-POLY-OK templates=2 instances=5 negatives=2\n")
    ~error:(fun message -> Printf.printf "PRELUDE-POLY-FAIL %s\n" message; exit 1)
