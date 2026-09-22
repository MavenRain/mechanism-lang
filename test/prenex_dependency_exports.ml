open Mechanism_kernel
open Mechanism_surface

let ( let* ) = Result.bind
let kernel result = Result.map_error Error.to_string result
let require message condition = if condition then Ok () else Error message
let read root path = Mechanism_import.Io.attempt (fun () ->
  In_channel.with_open_bin (Filename.concat root path) In_channel.input_all)
let exact label expected result = Result.fold result
  ~ok:(fun _value -> Error (label ^ ": expected refusal"))
  ~error:(fun actual -> require (label ^ ": " ^ Error.to_string actual) (actual = expected))
let collision name = Error.Mismatch ("the name " ^ name ^ " is already declared")
let seed =
  "poly (u) mu Box : Sort (succ u) with | box : Box "
  ^ "and Other : Sort (succ u) with | other : Other "
  ^ "where def witness : Box := box def alias : Box := witness end "
let dependency = "specialize Box (v) as Local "
let mapping = "export (alias := a, witness := w) "
let group body = "poly (v) group Pair where " ^ body ^ " end "
let complete = seed ^ group (dependency ^ mapping ^ "def pick : Local := a")
let instance = complete ^ "specialize Pair (0) as Run "

let negative_cases =
  let mapped text = seed ^ group (dependency ^ text) in
  [
    "missing", Error.Mismatch "every family member needs an export name", mapped "export (witness := w)";
    "empty", Error.Mismatch "every family member needs an export name", mapped "export ()";
    "duplicate-source", Error.Mismatch "member exports must name each member once",
      mapped "export (witness := w, witness := a)";
    "unknown-source", Error.Mismatch "member exports must name each member once",
      mapped "export (absent := w, alias := a)";
    "duplicate-target", Error.Mismatch "member exports repeat the target same",
      mapped "export (witness := same, alias := same)";
    "alias", collision "Local", mapped "export (witness := Local, alias := a)";
    "family", collision "Local_Other", mapped "export (witness := Local_Other, alias := a)";
    "constructor", collision "box", mapped "export (witness := box, alias := a)";
    "companion-constructor", collision "other", mapped "export (witness := other, alias := a)";
    "group", collision "Pair", mapped "export (witness := Pair, alias := a)";
    "own-member", collision "w", mapped (mapping ^ "def w : Local := a");
    "poly-name", collision "w", "poly (u) def w : Sort (succ u) := Sort u " ^ mapped mapping;
    "existing-global", collision "w", "def w : Type := Nat " ^ mapped mapping;
    "earlier-alias", collision "First", seed ^ group
      ("specialize Box (v) as First " ^ dependency ^ "export (witness := First, alias := a)");
    "earlier-composed-alias", collision "First", seed
      ^ "poly (u) group Wrapper where specialize Box (u) as Wrapped end "
      ^ group ("specialize Wrapper (v) as First " ^ dependency
        ^ "export (witness := First, alias := a)");
    "later-alias", collision "Later", seed ^ group
      (dependency ^ "export (witness := Later, alias := a) specialize Box (v) as Later");
    "earlier-member", collision "First_witness", seed ^ group
      ("specialize Box (v) as First " ^ dependency ^ "export (witness := First_witness, alias := a)");
    "reused-family", collision "First", seed ^ group
      ("specialize Box (v) as First specialize Box (v) as Second "
       ^ "with (Box := First, Other := First_Other) export (witness := First, alias := First)");
    "own-composed-alias", collision "First", seed
      ^ "poly (u) group Wrapper where specialize Box (u) as Wrapped end "
      ^ group "specialize Wrapper (v) as First export (Wrapped_witness := First, Wrapped_alias := a)";
    "template-name", collision "Box", mapped "export (witness := Box, alias := a)";
    "installed-family", collision "Shared", seed ^ "specialize Box (0) as Shared "
      ^ group (dependency ^ "export (witness := Shared, alias := a)");
    "installed-constructor", collision "empty",
      "poly (u) mu Empty : Sort (succ u) with | empty : Empty " ^ seed
      ^ "specialize Empty (0) as E " ^ group (dependency ^ "export (witness := empty, alias := a)");
    "unbound-reuse-with-export", Error.Unbound "unknown reused family Nope",
      mapped "with (Box := Nope) export (witness := Nope, alias := a)";
  ]

let parse_cases = [
  "missing-open", "export w", "expected '(' after 'export'";
  "missing-target", "export (witness :=)", "expected a global name after ':='";
  "trailing-comma", "export (witness := w,)", "expected a member export 'MEMBER := NAME'";
  "second-export", "export () export ()", "one export clause per specialization";
  "with-after-export", "export () with (Box := First)", "'with' must precede 'export'";
  "second-with", "with (Box := First) with (Other := Second)", "one 'with' clause per specialization";
]

let roundtrip text =
  let* parsed = kernel (Parser.parse text) in
  let* reparsed = kernel (Parser.parse (Syntax.print parsed)) in
  require "parse/print changed a dependency export mapping" (parsed = reparsed)

let suite root =
  let positives = [complete, ["Closed_w"; "Closed_a"; "Closed_pick"];
    seed ^ group (dependency ^ "export (witness := Local_alias, alias := Local_witness)"),
      ["Closed_Local_alias"; "Closed_Local_witness"];
    "poly (u) mu Empty : Sort (succ u) with | empty : Empty "
      ^ group "specialize Empty (v) as Local export ()", [];
    "poly (u) mu Empty : Sort (succ u) with | empty : Empty "
      ^ group "specialize Empty (v) as Local export ( )", [];
    seed ^ group (dependency ^ "export (witness := export, alias := a) def pick : Local := export"),
      ["Closed_export"; "Closed_a"; "Closed_pick"];
    "poly (u) mu Box : Sort (succ u) with | box : Box "
      ^ "where def Ty : Sort (succ u) := Box def value : Ty := box end "
      ^ group (dependency ^ "export (value := w, Ty := Carrier) def pick : Carrier := w"),
      ["Closed_Carrier"; "Closed_w"; "Closed_pick"];
    "poly (u) mu Box : Sort (succ u) with | box : Box "
      ^ "where def Ty : Sort (succ u) := Box def value : Ty := box end "
      ^ group (dependency ^ "export (Ty := value, value := Ty) def pick : value := Ty"),
      ["Closed_value"; "Closed_Ty"; "Closed_pick"]
  ] in
  let* () = List.fold_left (fun acc (text, closed_rows) -> let* () = acc in
    let* () = roundtrip text in
    let* globals, rows = kernel (Elab.check_in Global.empty text) in
    let* () = require "symbolic declarations escaped composition"
      (Global.StringMap.is_empty globals.entries && Global.StringMap.is_empty globals.families
       && rows = []) in
    let* _globals, closed = kernel (Elab.check_in Global.empty
      (text ^ "specialize Pair (0) as Closed")) in
    require "the closed instance rows differ from the chosen names in declaration order"
      (List.map fst closed = closed_rows)) (Ok ()) positives in
  let* globals, rows = kernel (Elab.check_in Global.empty instance) in
  let* () = require "dependency exports changed declaration order"
    (List.map fst rows = ["Run_w"; "Run_a"; "Run_pick"]) in
  let* () = require "the closed instance lost the prefixed dependency family"
    (Option.is_some (Global.find_family "Run_Local" globals)) in
  let* () = require "default or local export names escaped"
    (List.for_all (fun name -> Option.is_none (Global.find name globals))
      ["w"; "a"; "Local_witness"; "Local_alias"; "Run_Local_witness"; "Run_Local_alias"])
  in
  let* () = List.fold_left (fun acc (label, expected, text) -> let* () = acc in
    exact label expected (Elab.check_in Global.initial text)) (Ok ()) negative_cases in
  let* () = List.fold_left (fun acc (label, clause, prefix) -> let* () = acc in
    Result.fold (Parser.parse (seed ^ group (dependency ^ clause)))
      ~ok:(fun _tree -> Error (label ^ ": expected parse refusal"))
      ~error:(fun error -> match error with
        | Error.Parse (message, _line, _column) ->
            require (label ^ ": " ^ message) (String.starts_with ~prefix message)
        | Error.Unbound _ | Error.Universe _ | Error.Mismatch _ | Error.Budget_exhausted _
        | Error.Not_yet _ | Error.Carry _ | Error.Quantity _ | Error.Wrong_leg _
        | Error.Missing_branch _ | Error.Overflow _ | Error.Cannot_infer _
        | Error.Index_not_zero _ | Error.Index_above_universe _ | Error.Termination _ ->
            Error (label ^ ": " ^ Error.to_string error))) (Ok ()) parse_cases in
  let reservation = "poly (u) def w : Sort (succ u) := Sort u " in
  (* Each budget case measures the polls of a baseline source (None: the
     baseline must pass; Some error: it must stop with that error), adds the
     extra polls the failing source may spend, and cancels the failing source
     at the first poll past that ceiling. collision-budget grants the three
     mapping validation polls (entry plus one per binding), so the failing
     source passes mapping validation and dies at the name-planning poll
     before the collision with w is reported. collision-budget stays first
     because the planning-budget mutant in mutations.py names it: with
     validation unbudgeted the planning poll is the only one, lands under the
     ceiling, and the collision is reported. The composition-budget baseline
     stops at the empty-mapping refusal, the last stop before the per-binding
     poll, so the failing source dies at the first binding poll while a
     mutant without that poll reports the alias collision instead. *)
  let budget_cases = [
    "collision-budget", None, reservation ^ seed, 3, reservation ^ complete;
    "composition-budget", Some (Error.Mismatch "every family member needs an export name"),
      seed ^ group (dependency ^ "export ()"), 0,
      seed ^ group (dependency ^ "export (witness := Local, alias := a)");
  ] in
  let polls_of label expected source =
    let count = ref 0 in
    let checked = Elab.check_in Global.empty source
      ~budget:(Budget.of_poll (fun () -> incr count; false)) in
    let* () = Option.fold expected
      ~none:(Result.map (fun _checked -> ()) (kernel checked))
      ~some:(fun error -> exact (label ^ " baseline") error checked) in
    Ok !count in
  let* () = List.fold_left (fun acc (label, expected, baseline, extra, failing) -> let* () = acc in
    let* ceiling = Result.map (fun polls -> polls + extra) (polls_of label expected baseline) in
    let spent = ref 0 in
    exact label (Error.Budget_exhausted Check.budget_msg)
      (Elab.check_in Global.empty failing
        ~budget:(Budget.of_poll (fun () -> incr spent; !spent > ceiling)))) (Ok ()) budget_cases in
  let* base = read root "test/fixtures/prelude/prenex-exports-runtime.mech" in
  let* extra = read root "test/fixtures/prelude/dependency-exports-runtime.mech" in
  let source = base ^ extra in
  let* () = roundtrip source in
  let* globals, rows = kernel (Elab.check_in Global.initial source) in
  let* () = require "dependency exports introduced an axiom" (Elab.axiom_names rows = []) in
  let* () = require "nested reuse installed fresh families"
    (Option.is_none (Global.find_family "NestedRun_Again" globals)
      && Global.StringMap.cardinal globals.families
        = Global.StringMap.cardinal Global.initial.families + 8) in
  let* () = List.fold_left (fun acc name -> let* () = acc in
    let* value = kernel (Eval.eval globals [] (Term.Global name)) in
    let* value = kernel (Eval.whnf globals value) in
    require (name ^ ": wrong runtime value")
      (Value.as_lit value = Some (Literal.LInt (Bignum.of_int 37))))
    (Ok ()) ["dependencyPayload"; "nestedPayload"] in
  Ok (List.length positives, List.length negative_cases, List.length parse_cases,
    List.length budget_cases)

let () =
  let root = Array.to_list Sys.argv |> List.to_seq |> Seq.drop 1 |> Seq.uncons
    |> Option.map fst |> Option.value ~default:"." in
  Result.fold (suite root)
    ~ok:(fun (positives, negatives, parser, budget) ->
      Printf.printf "PRENEX-DEPENDENCY-EXPORTS-OK positives=%d negatives=%d parser=%d budget=%d\n"
        positives negatives parser budget)
    ~error:(fun message -> prerr_endline ("PRENEX-DEPENDENCY-EXPORTS FAIL " ^ message); exit 1)
