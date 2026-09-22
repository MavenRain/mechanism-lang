open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let refusal label expected actual = Result.fold actual
  ~ok:(fun _value -> Error (label ^ ": expected refusal"))
  ~error:(fun error -> require (label ^ ": " ^ Error.to_string error) (error = expected))

let group =
  "poly (u) mu Box : Sort (succ u) with | box : Box "
  ^ "and Other : Sort (succ u) with | other : Other "
  ^ "where def witness : Box := box def alias : Box := witness end "
let specialization = "specialize Box (0) as One "
let mapping = "export (alias := chosenAlias, witness := chosenWitness) "
let instance = group ^ specialization ^ mapping
let composed = group
  ^ "poly (v) group Pair where specialize Box (v) as Local "
  ^ "def pick : Local := Local_witness end "
let composed_instance = composed ^ "specialize Pair (0) as Inst "
  ^ "export (pick := chosenPick, Local_alias := chosenLocalAlias, "
  ^ "Local_witness := chosenWitness) "
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")

let negative_cases = [
  "missing", Error.Mismatch "every family member needs an export name",
    group ^ specialization ^ "export (witness := chosenWitness)";
  "composed-own-members-only", Error.Mismatch "every family member needs an export name",
    composed ^ "specialize Pair (0) as Inst export (pick := chosenPick)";
  "empty", Error.Mismatch "every family member needs an export name",
    group ^ specialization ^ "export ()";
  "empty-spaced", Error.Mismatch "every family member needs an export name",
    group ^ specialization ^ "export ( )";
  "duplicate-source", Error.Mismatch "member exports must name each member once",
    group ^ specialization ^ "export (witness := first, witness := second)";
  "unknown-source", Error.Mismatch "member exports must name each member once",
    group ^ specialization ^ "export (absent := first, alias := second)";
  "duplicate-target", Error.Mismatch "member exports repeat the target same",
    group ^ specialization ^ "export (witness := same, alias := same)";
  "instance-name", collision "One",
    group ^ specialization ^ "export (witness := One, alias := chosenAlias)";
  "companion-name", collision "One_Other",
    group ^ specialization ^ "export (witness := One_Other, alias := chosenAlias)";
  "constructor", collision "box",
    group ^ specialization ^ "export (witness := box, alias := chosenAlias)";
  "companion-constructor", collision "other",
    group ^ specialization ^ "export (witness := other, alias := chosenAlias)";
  "ambient-global", collision "chosenWitness",
    "def chosenWitness : Type 0 := Prop " ^ instance;
  "ambient-family", collision "chosenWitness",
    "mu chosenWitness : Type 0 with " ^ instance;
  "ambient-constructor", collision "chosenWitness",
    "mu Ambient : Type 0 with | chosenWitness : Ambient " ^ instance;
  "definition-catalog", collision "chosenWitness",
    "poly (v) def chosenWitness : Sort (succ v) := Sort v " ^ instance;
  "family-catalog", collision "chosenWitness",
    "poly (v) mu chosenWitness : Sort (succ v) with " ^ instance;
  "reused-family", collision "Shared",
    group ^ "specialize Box (0) as Shared " ^ specialization
      ^ "with (Box := Shared, Other := Shared_Other) "
      ^ "export (witness := Shared, alias := chosenAlias)";
  "reused-instance-name", collision "One",
    group ^ "specialize Box (0) as Shared " ^ specialization
      ^ "with (Box := Shared, Other := Shared_Other) "
      ^ "export (witness := One, alias := chosenAlias)";
  "definition-template", Error.Not_yet "member exports require a family template",
    "poly (u) def identity : Sort (succ u) := Sort u "
      ^ "specialize identity (0) as concrete export ()";
  "unknown-template", Error.Unbound "unknown universe schema Missing",
    "specialize Missing (0) as concrete export ()";
  "default-name-absent", Error.Unbound "One_witness",
    instance ^ "def value : One := One_witness";
]

let parse_cases = [
  "missing-list", "export", "expected '(' after 'export'";
  "missing-source", "export (:= target)", "expected a member export";
  "missing-assignment", "export (witness target)", "expected a member export";
  "missing-target", "export (witness :=)", "expected a global name after ':=', found ')'";
  "keyword-target", "export (witness := def, alias := a)", "expected a global name after ':=', found 'def'";
  "trailing-comma", "export (witness := target,)", "expected a member export";
  "missing-close", "export (witness := target", "expected ',' or ')' after a member export";
  "with-after-export", "export (witness := w, alias := a) with (Box := Shared)",
    "'with' must precede 'export' in a specialization";
  "second-export", "export (witness := w) export (alias := a)",
    "one export clause per specialization";
  "dependency-export",
    "poly (v) group Pair where specialize Box (v) as Inner export (witness := w) end",
    "export clauses on group dependencies are not supported";
]

let polls_of source =
  let count = ref 0 in
  Result.map (fun _checked -> !count) (kernel (Elab.check_in Global.empty source
    ~budget:(Budget.of_poll (fun () -> incr count; false))))
let exhausted_after ceiling source =
  let spent = ref 0 in
  Elab.check_in Global.empty source
    ~budget:(Budget.of_poll (fun () -> incr spent; !spent > ceiling))

let roundtrip source =
  let* parsed = kernel (Parser.parse source) in
  let* printed = kernel (Parser.parse (Syntax.print parsed)) in
  require "parse/print changed the export mapping" (parsed = printed)

let suite root =
  let* source = Mechanism_import.Io.attempt (fun () ->
    In_channel.with_open_bin
      (Filename.concat root "test/fixtures/prelude/prenex-exports-runtime.mech")
      In_channel.input_all) in
  let* () = roundtrip source in
  let* globals, rows = kernel (Elab.check_in Global.initial source) in
  let names = ["Shared_unbox"; "Shared_pack"; "Shared_unpack";
    "freshUnbox"; "freshPack"; "freshUnpack";
    "sharedUnbox"; "sharedPack"; "sharedUnpack";
    "exportInput"; "namedPayload"; "reusedPayload"] in
  let* () = require "export row names or declaration order changed"
    (List.map fst rows = names) in
  let* () = require "default member names leaked"
    (List.for_all (fun name -> Option.is_none (Global.find name globals))
      ["Named_unbox"; "Named_pack"; "Named_unpack";
       "Reused_unbox"; "Reused_pack"; "Reused_unpack"]) in
  let* () = require "reuse installed a fresh family"
    (Option.is_none (Global.find_family "Reused" globals)
      && Option.is_none (Global.find_family "Reused_Wrapped" globals)
      && Global.StringMap.cardinal globals.families
         = Global.StringMap.cardinal Global.initial.families + 4) in
  let* () = require "exported members introduced an axiom" (Elab.axiom_names rows = []) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* value = kernel (Eval.whnf globals value) in
    let* literal = Value.as_lit value |> Option.to_result ~none:(name ^ ": not a literal") in
    match literal with
    | Literal.LInt n -> require (name ^ ": wrong value") (Bignum.to_string n = "37")
    | Literal.LString _ -> Error (name ^ ": not a natural number"))
    (Ok ()) ["namedPayload"; "reusedPayload"] in
  let positives = [instance;
    "def One_witness : Type 0 := Prop " ^ instance;
    group ^ specialization;
    "poly (u) mu Bare : Sort (succ u) with specialize Bare (0) as Empty export ()";
    "poly (u) mu Bare : Sort (succ u) with specialize Bare (0) as Empty export ( )";
    "def export : Type 0 := Prop def value : Type 0 := export";
    group ^ specialization ^ "export (witness := alias, alias := witness) "
      ^ "def value : One := witness";
    composed_instance;
  ] in
  let* composed_globals, composed_rows = kernel (Elab.check_in Global.empty composed_instance) in
  let* () = require "composed export rows or declaration order changed"
    (List.map fst composed_rows = ["chosenWitness"; "chosenLocalAlias"; "chosenPick"]) in
  let* () = require "composed default member names leaked"
    (List.for_all (fun name -> Option.is_none (Global.find name composed_globals))
      ["Inst_Local_witness"; "Inst_Local_alias"; "Inst_pick"]) in
  let* () = List.fold_left (fun acc text -> let* () = acc in
    let* () = roundtrip text in
    let* _checked = kernel (Elab.check_in Global.empty text) in Ok ()) (Ok ()) positives in
  let* () = List.fold_left (fun acc (label, expected, text) -> let* () = acc in
    refusal label expected (Elab.check_in Global.empty text)) (Ok ()) negative_cases in
  let* () = List.fold_left (fun acc (label, suffix, prefix) -> let* () = acc in
    Result.fold (Parser.parse (specialization ^ suffix))
      ~ok:(fun _tree -> Error (label ^ ": expected parse refusal"))
      ~error:(fun error -> match error with
        | Error.Parse (message, _line, _col) ->
            require (label ^ ": " ^ message) (String.starts_with ~prefix message)
        | Error.Unbound _ | Error.Universe _ | Error.Mismatch _ | Error.Budget_exhausted _
        | Error.Not_yet _ | Error.Carry _ | Error.Quantity _ | Error.Wrong_leg _
        | Error.Missing_branch _ | Error.Overflow _ | Error.Cannot_infer _
        | Error.Index_not_zero _ | Error.Index_above_universe _ | Error.Termination _ ->
            Error (label ^ ": " ^ Error.to_string error))) (Ok ()) parse_cases in
  let* declaration_polls = polls_of group in
  let* () = refusal "instance-budget" (Error.Budget_exhausted Check.budget_msg)
    (exhausted_after declaration_polls instance) in
  let* export_polls = polls_of instance in
  let* () = require "the export specialization consumed no budget"
    (export_polls > declaration_polls) in
  let* () = refusal "late-budget" (Error.Budget_exhausted Check.budget_msg)
    (exhausted_after (export_polls - 1) instance) in
  let reservation = "poly (v) def chosenWitness : Sort (succ v) := Sort v " in
  let* reserved_polls = polls_of (reservation ^ group) in
  let* () = refusal "collision-budget" (Error.Budget_exhausted Check.budget_msg)
    (exhausted_after reserved_polls (reservation ^ instance)) in
  Ok (List.length rows, List.length positives, List.length negative_cases, List.length parse_cases)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (entries, positives, negatives, parser) ->
      Printf.printf "PRENEX-EXPORTS-OK entries=%d positives=%d negatives=%d parser=%d budget=3\n"
        entries positives negatives parser)
    ~error:(fun message -> prerr_endline ("PRENEX-EXPORTS FAIL " ^ message); exit 1)
