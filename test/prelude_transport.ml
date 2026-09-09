open Mechanism_kernel

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read path = Mechanism_import.Io.attempt
  (fun () -> In_channel.with_open_bin path In_channel.input_all)

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

let refusal expected result = Result.fold result
  ~ok:(fun _checked -> Error ("expected refusal: " ^ expected))
  ~error:(fun actual -> require ("wrong refusal: " ^ Error.to_string actual)
    (String.starts_with ~prefix:expected (Error.to_string actual)))

let suite root =
  let* catalog = kernel (Mechanism_prelude.Equality.catalog Global.empty) in
  let templates = [
    "MechEq", 2, ["refl"; "transport"; "j"; "symm"; "trans"; "congr"];
    "MechTypeEq", 1, ["refl"; "cast"; "symm"; "trans"];
  ] in
  let* () = require "polymorphic equality inventory differs"
    (List.for_all (fun (name, arity, members) ->
      Mechanism_surface.Family_poly.arity catalog name = Some arity
        && Mechanism_surface.Family_poly.members catalog name = Some members) templates) in
  let* source = read (Filename.concat root "prelude/init.mech") in
  let* globals, _rows = kernel (Mechanism_surface.Elab.check_in Global.empty source) in
  let instances = [
    "MechEq", [Level.one; Level.one], "TransportData";
    "MechEq", [Level.one; Level.succ Level.one], "TransportHigher";
    "MechEq", [Level.zero; Level.zero], "TransportProof";
    "MechEq", [Level.zero; Level.one], "TransportProofData";
    "MechEq", [Level.one; Level.zero], "TransportToProof";
    "MechTypeEq", [Level.zero], "CastData";
    "MechTypeEq", [Level.one], "CastTypes";
  ] in
  let* installed = List.fold_left (fun acc (name, levels, as_name) ->
    let* globals = acc in
    kernel (Mechanism_surface.Family_poly.instantiate globals catalog
      ~name ~levels ~as_name)) (Ok globals) instances in
  let* client = read (Filename.concat root "test/fixtures/prelude/transport.mech") in
  let* checked, _rows = kernel (Mechanism_surface.Elab.check_in installed client) in
  let* () = audit checked in
  (* Each row carries its own scope.  The first four are misuse diagnostics
     in the installed environment.  The last row is a scope control: it
     shows that a member is unbound before its instance exists. *)
  let negatives = [
    (installed, "mismatch: the term has type (Lan SMu TransportData",
     "def wrongEndpoint : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) \
      mechZero (mechSucc mechZero) (TransportData_refl MechNat mechZero) mechZero\n");
    (installed, "mismatch: the term has type (Lan SMu CastData",
     "def wrongTypes : MechUnit := CastData_cast MechNat MechUnit (CastData_refl MechNat) mechZero\n");
    (installed, "mismatch: the term has type Type 2 and the expected type is Type 1",
     "def wrongUniverse : Type 0 := CastData_cast (Type 0) (Type 0) \
      (CastData_refl (Type 0)) MechNat\n");
    (installed, "mismatch: the term has type (Lan SMu TransportHigher",
     "def mixedInstance : MechNat := TransportData_transport MechNat (fun (n : MechNat) => MechNat) \
      mechZero mechZero (TransportHigher_refl MechNat mechZero) mechZero\n");
    (globals, "unbound: CastData_cast",
     "def unavailable : MechNat := CastData_cast MechNat MechNat \
      (CastData_refl MechNat) mechZero\n");
  ] in
  let* () = List.fold_left (fun acc (scope, prefix, source) ->
    let* () = acc in
    refusal prefix (Mechanism_surface.Elab.check_in scope source)) (Ok ()) negatives in
  let* () = require "template checking changed ordinary globals"
    (Global.find "cast" globals = None && Global.find_family "MechTypeEq" globals = None) in
  Ok (List.length templates, List.length instances, List.length negatives)

let () =
  let root = Sys.argv |> Array.to_list |> fun args ->
    List.nth_opt args 1 |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (templates, instances, negatives) ->
      Printf.printf "PRELUDE-TRANSPORT-OK templates=%d instances=%d negatives=%d\n"
        templates instances negatives)
    ~error:(fun message -> Printf.printf "PRELUDE-TRANSPORT-FAIL %s\n" message; exit 1)
