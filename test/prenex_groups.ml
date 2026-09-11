open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let first = "poly (u) mu First : Sort (succ u) with | first : First "
let second = "and Second : Sort (succ u) with | second : First -> Second "
let members = "where def witness : Second := second first def alias : Second := witness end "
let group = first ^ second ^ members
let instance = group ^ "specialize First (0) as One "
let poly name = "poly (u) def " ^ name ^ " : Sort (succ u) := Sort u "
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let negative_cases = [
  "forward-family", Error.Unbound "Second",
    "poly (u) mu First : Sort (succ u) with | first : Second -> First " ^ second;
  "forward-member", Error.Unbound "later",
    first ^ "where def witness : First := later def later : First := first end";
  "recursive-member", Error.Unbound "witness",
    first ^ "where def witness : First := witness end";
  "duplicate-family", Error.Mismatch "the family First is already declared",
    first ^ "and First : Sort (succ u) with";
  "duplicate-member", collision "witness",
    first ^ "where def witness : First := first def witness : First := first end";
  "member-family-name", collision "First",
    first ^ "where def First : First := first end";
  "member-label-name", collision "first",
    first ^ "where def first : First := first end";
  "companion-definition-template", collision "Second", poly "Second" ^ group;
  "member-definition-template", collision "witness", poly "witness" ^ group;
  "member-family-template", collision "witness",
    "poly (u) mu witness : Prop with " ^ group;
  "unstable-companion", Error.Universe
    "a family schema must remain in Prop or Type for every universe substitution",
    first ^ "and Second : Sort u with";
  "bad-member-type", Error.Mismatch "the term has type Type 1 and the expected type is Type 0",
    first ^ "where def bad : Sort 0 := Sort 0 end";
  "symbolic-family-escape", Error.Unbound "Second", group ^ "def escape : Type 0 := Second";
  "symbolic-member-escape", Error.Unbound "witness", group ^ "def escape : Type 0 := witness";
  "wrong-arity", Error.Universe "the family schema First expects 1 universe arguments, got 2",
    group ^ "specialize First (0, 1) as One";
  "companion-template-specialization", Error.Unbound "unknown universe schema Second",
    group ^ "specialize Second (0) as One";
  "companion-template-target", collision "One_Second",
    group ^ poly "One_Second" ^ "specialize First (0) as One";
  "member-template-target", collision "One_witness",
    group ^ poly "One_witness" ^ "specialize First (0) as One";
  "member-family-template-target", collision "One_witness",
    group ^ "poly (u) mu One_witness : Prop with specialize First (0) as One";
  "companion-global-target", collision "One_Second",
    group ^ "def One_Second : Type 0 := Prop specialize First (0) as One";
  "member-global-target", collision "One_witness",
    group ^ "def One_witness : Type 0 := Prop specialize First (0) as One";
  "companion-constructor-target", collision "One_Second",
    group ^ "mu Other : Type 0 with | One_Second : Other specialize First (0) as One";
  "member-constructor-target", collision "One_witness",
    group ^ "mu Other : Type 0 with | One_witness : Other specialize First (0) as One";
  "companion-label-definition-template", collision "second",
    group ^ poly "second" ^ "specialize First (0) as One";
  "companion-label-family-template", collision "second",
    group ^ "poly (u) mu second : Prop with specialize First (0) as One";
  "generated-companion-label", collision "One_Second",
    "poly (u) mu First : Sort (succ u) with | One_Second : First "
      ^ second ^ "specialize First (0) as One";
  "generated-member-label", collision "One_witness",
    "poly (u) mu First : Sort (succ u) with | One_witness : First "
      ^ "where def witness : First := One_witness end specialize First (0) as One";
  "late-label-instance-name", collision "One",
    instance ^ "poly (u) mu Holder : Sort (succ u) with | One : Holder "
      ^ "specialize Holder (0) as Two";
  "late-label-generated-member", collision "One_witness",
    instance ^ "poly (u) mu Holder : Sort (succ u) with | One_witness : Holder "
      ^ "specialize Holder (0) as Two";
  "companion-constructor-name", collision "Second",
    "mu Other : Type 0 with | Second : Other " ^ group;
]

let refusal label expected actual = Result.fold actual
  ~ok:(fun _value -> Error (label ^ ": expected refusal"))
  ~error:(fun error -> require (label ^ ": wrong refusal: " ^ Error.to_string error)
    (error = expected))

let suite root =
  let* source = Mechanism_import.Io.attempt (fun () ->
    In_channel.with_open_bin (Filename.concat root "test/fixtures/prelude/prenex-groups.mech")
      In_channel.input_all) in
  let* tree = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print tree)) in
  let* () = require "group parse/print changed the tree" (tree = printed) in
  let* globals, rows = kernel (Elab.check_in Global.empty source) in
  let expected_rows = List.concat_map (fun name ->
    List.map (fun member -> name ^ "_" ^ member) ["unbox"; "pack"; "unpack"; "level"])
    ["Data"; "Types"; "Proofs"; "Again"]
    @ ["dataValue"; "typeValue"; "proofValue"; "againValue";
       "One_make"; "oneValue"; "plainValue"] in
  let* () = require "member inventory or row order changed"
    (List.map fst rows = expected_rows
      && Global.StringMap.cardinal globals.entries = List.length expected_rows) in
  let expected_families = ["Tiny"; "Truth"; "Data"; "Data_Wrapped"; "Types";
    "Types_Wrapped"; "Proofs"; "Proofs_Wrapped"; "Again"; "Again_Wrapped";
    "One"; "Plain"; "Plain_Second"] in
  let* () = require "family inventory or symbolic isolation changed"
    (List.map fst (Global.StringMap.bindings globals.families)
      = List.sort String.compare expected_families) in
  let* () = require "unexpected axiom disclosure" (Elab.axiom_names rows = []) in
  let* () = Global.StringMap.fold (fun name entry acc -> let* () = acc in
    match entry with
    | Global.Def _ -> Ok ()
    | Global.Axiom _ | Global.Prim _ -> Error ("unexpected trusted entry: " ^ name))
    globals.entries (Ok ()) in
  let zero = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "zero", []) in
  let one = Term.In (Shape.SMu ("Tiny", []), Term.ACtor "next", [zero]) in
  let tiny = Term.Lan (Shape.SMu ("Tiny", []), Rules.diagram_of []) in
  let computations = ["dataValue", one; "typeValue", tiny; "againValue", zero;
    "Data_level", Term.Univ (Level.succ (Level.succ Level.zero));
    "Types_level", Term.Univ (Level.succ Level.zero);
    "oneValue", Term.In (Shape.SMu ("One", []), Term.ACtor "single", [zero]);
    "plainValue", Term.In (Shape.SMu ("Plain_Second", []), Term.ACtor "second",
      [Term.In (Shape.SMu ("Plain", []), Term.ACtor "first", [])])] in
  let* () = List.fold_left (fun acc (name, expected) -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* actual = kernel (Eval.quote globals 0 value) in
    require ("wrong computation: " ^ name ^ " = " ^ Pp.term [] actual) (actual = expected))
    (Ok ()) computations in
  let* _good = kernel (Elab.check_in Global.empty instance) in
  let positives = [
    "poly (u) mu First : Sort (succ u) with | mark : First "
      ^ "and Second : Sort (succ u) with | mark : First -> Second "
      ^ "where def witness : Second := mark mark end specialize First (0) as One "
      ^ "def value : One_Second := One_witness";
    "poly (u) mu P : Prop with | p : P and Q : Prop with | q : P -> Q "
      ^ "where def witness : Q := q p end specialize P (0) as Proof "
      ^ "def value : Proof_Q := Proof_witness";
    first ^ second ^ "def ordinary : Type 0 := Prop specialize First (0) as One";
  ] in
  let* () = List.fold_left (fun acc text -> let* () = acc in
    let* parsed = kernel (Parser.parse text) in
    let* printed = kernel (Parser.parse (Syntax.print parsed)) in
    let* () = require "positive group round-trip changed" (parsed = printed) in
    let* _checked = kernel (Elab.check_in Global.empty text) in Ok ()) (Ok ()) positives in
  let* () = List.fold_left (fun acc (label, expected, source) -> let* () = acc in
    refusal label expected (Elab.check_in Global.empty source)) (Ok ()) negative_cases in
  let parse_cases = [
    "empty-members", first ^ "where end", "expected a nonempty list of member definitions, found 'end'";
    "postulated-member", first ^ "where axiom x : First end", "expected 'def' or 'end' in family members, found 'axiom'";
    "recursive-syntax", first ^ "where def rec x : First := first end", "expected a nonrecursive member definition, found 'def'";
    "missing-end", first ^ "where def x : First := first", "expected 'def' or 'end' in family members, found end of input";
    "member-scope", first ^ "where def x : Sort (succ v) := Sort v end", "unbound universe v";
  ] in
  let* () = List.fold_left (fun acc (label, source, expected) -> let* () = acc in
    Result.fold (Parser.parse source)
      ~ok:(fun _tree -> Error (label ^ ": expected parse refusal"))
      ~error:(fun error -> match error with
        | Error.Parse (message, _line, _col) -> require (label ^ ": " ^ message) (message = expected)
        | Error.Unbound _ | Error.Universe _ | Error.Mismatch _ | Error.Budget_exhausted _
        | Error.Not_yet _ | Error.Carry _ | Error.Quantity _ | Error.Wrong_leg _
        | Error.Missing_branch _ | Error.Overflow _ | Error.Cannot_infer _
        | Error.Index_not_zero _ | Error.Index_above_universe _ | Error.Termination _ ->
            Error (label ^ ": " ^ Error.to_string error))) (Ok ()) parse_cases in
  let* () = refusal "declaration-budget" (Error.Budget_exhausted Check.budget_msg)
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> true)) Global.empty group) in
  let count = ref 0 in
  let* _declaration = kernel (Elab.check_in
    ~budget:(Budget.of_poll (fun () -> incr count; false)) Global.empty group) in
  let ceiling = !count in
  let polls = ref 0 in
  let* () = refusal "instance-budget" (Error.Budget_exhausted Check.budget_msg)
    (Elab.check_in ~budget:(Budget.of_poll (fun () -> incr polls; !polls > ceiling))
      Global.empty instance) in
  let count_polls text =
    let polls = ref 0 in
    let* _checked = kernel (Elab.check_in
      ~budget:(Budget.of_poll (fun () -> incr polls; false)) Global.empty text) in
    Ok !polls in
  let solo = "poly (u) mu Second : Sort (succ u) with | second : Second " in
  let* templates = count_polls (first ^ solo) in
  let* grouped = count_polls
    (first ^ "and Second : Sort (succ u) with | second : Second ") in
  let* () = require ("group elaboration repeats the universal check: "
      ^ string_of_int grouped ^ " polls against " ^ string_of_int templates
      ^ " for the same two families as templates")
    (grouped <= 2 * templates) in
  let* () = refusal "catalog-lifetime" (Error.Unbound "unknown universe schema Box")
    (Elab.check_in globals "specialize Box (1, 2) as Later") in
  let* () = refusal "group-ast-boundary"
    (Error.Mismatch "a prenex declaration requires a program catalog")
    (Elab.elab_decl (Check.make Global.empty Budget.unlimited)
      (Syntax.DPolyGroup (1, { Syntax.fm_name = "Empty"; fm_params = [];
        fm_ty = Syntax.SType 0; fm_ctors = [] }, [], []))) in
  Ok (List.length expected_families, List.length rows, List.length computations,
      List.length negative_cases + List.length parse_cases + 4)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (families, entries, computations, negatives) ->
      Printf.printf "PRENEX-GROUPS-OK families=%d entries=%d computations=%d negatives=%d\n"
        families entries computations negatives)
    ~error:(fun message -> Printf.printf "PRENEX-GROUPS-FAIL %s\n" message; exit 1)
