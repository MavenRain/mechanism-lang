import Base
import ./relational_common.bend as R
import ./surface_prenex_data.bend as Data
import ./surface_prelude_poly.bend as Audit
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as Syntax
import ../surface/elab.bend as Elab
import ../kernel/check.bend as Check
import ../kernel/global.bend as G
import ../kernel/positivity.bend as P
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def kind(error: E.T) -> String:
  match error:
    case E.Parse{message, line, column}: "parse"
    case E.Unbound{message}: "unbound"
    case E.Universe{message}: "universe"
    case E.Mismatch{message}: "mismatch"
    case E.Budget_exhausted{message}: "budget"
    case other: "unexpected"

def refusal(-A: Data, label: String, +expected_kind: String, +expected: String, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{label ++ ": expected refusal"}
    case Fail{+error}:
      R.require(Bool.pick(Bool, String.eq(expected_kind, "rendered"),
        Audit.starts(expected, E.to_string(error)),
        String.eq(kind(error), expected_kind) && Audit.starts(expected, E.message(error))), label ++ ": wrong refusal: " ++ E.to_string(error))

def control(source: Maybe<&2, String>, globals: G.T) -> Result<&2, &2, String, Unit>:
  match source:
    case None{}: Done{Unit{}}
    case Some{source}:
      do Result<&2, &2, String, Unit>:
        checked : Elab.Checked <- R.checked(globals, source)
        Done{Unit{}}

def negatives(cases: List<&2, Data.Negative>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Negative{name, kind, detail, good, source}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- control(good, globals)
        checked : Unit <- refusal(Elab.Checked, name, kind, detail, Elab.check_in(globals, B.unlimited(), source))
        negatives(rest, globals)

def parse_refusal(label: String, +expected: String, prefix: Bool, result: Result<&2, &2, E.T, List<&2, Syntax.Decl>>) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{label ++ ": expected parse refusal"}
    case Fail{E.Parse{+message, line, column}}:
      R.require(Bool.pick(Bool, prefix, Audit.starts(expected, message), String.eq(expected, message)), label ++ ": " ++ message)
    case Fail{error}: Fail{label ++ ": " ++ E.to_string(error)}

def parsers(cases: List<&2, Data.ParseCase>, +prefix: Bool) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.ParseCase{name, source, expected}, rest}:
      do Result<&2, &2, String, Unit>:
        checked : Unit <- parse_refusal(name, expected, prefix, Parser.program(source))
        parsers(rest, prefix)

def positives(cases: List<&2, Data.Positive>) -> Result<&2, &2, String, Unit>:
  match cases:
    case Nil{}: Done{Unit{}}
    case Con{Data.Positive{+source, rows}, rest}:
      do Result<&2, &2, String, Unit>:
        parsed : List<&2, Syntax.Decl> <- R.parse_roundtrip(source)
        checked : Elab.Checked <- R.checked(G.empty(), source)
        positives(rest)

def absent_entries(names: List<&2, String>, +globals: G.T) -> Bool:
  match names:
    case Nil{}: True{}
    case Con{name, rest}: Raw.absent(G.Entry, G.find(name, globals)) && absent_entries(rest, globals)

def family_count(globals: G.T) -> Nat:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: R.length(C.Pair<String, P.Family>, families)

def family_names(globals: G.T) -> List<&2, String>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: R.family_names(families)

def action(globals: G.T, source: String) -> B.Comp(Elab.Checked):
  do B.Comp<Elab.Checked>:
    parsed : List<&2, Syntax.Decl> <- B.lift(List<&2, Syntax.Decl>, Parser.program(source))
    Elab.elab_program_comp(globals, parsed)

def polls(source: String) -> Result<&2, &2, String, Nat>:
  Raw.measure(Elab.Checked, B.run_state(Elab.Checked, action(G.empty(), source), B.of_polls(100000n)))

def exhausted(label: String, source: String, allowance: Nat) -> Result<&2, &2, String, Unit>:
  refusal(Elab.Checked, label, "budget", "the check budget is exhausted", B.run_poll(Elab.Checked, action(G.empty(), source), F.countdown(allowance)))

def boundary_decl(declarations: List<&2, Syntax.Decl>) -> Result<&2, &2, String, Unit>:
  match declarations:
    case Con{decl, Nil{}}:
      Raw.refusal(Check.Decl, E.Mismatch{"a prenex declaration requires a program catalog"}, Elab.elab_decl(decl, Check.make(G.empty(), B.unlimited(), 0n)))
    case other: Fail{"expected one boundary declaration"}

def ast_boundary(source: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    parsed : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(source))
    boundary_decl(parsed)
