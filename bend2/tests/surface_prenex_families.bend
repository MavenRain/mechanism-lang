import ../surface/parser.bend as Parser
import ../kernel/level.bend as Level
import Base
import ../kernel/bignum.bend as Bignum
import ./relational_common.bend as R
import ./surface_prenex_common.bend as Test
import ./surface_prenex_data.bend as Data
import ./surface_prenex.bend as Prenex
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/check.bend as Check
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def rows() -> List<&2, String>:
  ["dataBox", "typeBox", "proofBox", "againBox", "unbox", "dataValue", "typeValue", "items", "types", "head", "listValue", "up", "down", "upType", "dataRefl", "proofRefl", "boxIdentity", "boxValue"]

def families() -> List<&2, String>:
  ["Tiny", "Truth", "DataBox", "TypeBox", "ProofBox", "AgainBox", "TinyList", "TypeList", "Up", "Down", "DataEq", "ProofEq", "AnnotatedBox"]

def boundary() -> Result<&2, &2, String, Unit>:
  Raw.refusal(Check.Decl, E.Mismatch{"a prenex declaration requires a program catalog"},
    Elab.elab_decl(Syntax.DPolyMu{1n, Syntax.Fam{"Empty", [], Syntax.SType{Bignum.zero()}, []}}, Check.make(G.empty(), B.unlimited(), 0n)))


def specialization_poll(state: Elab.Program) -> Result<&2, &2, String, Unit>:
  match state:
    case Elab.Program{globals, definitions, families, rows}:
      Test.refusal(Elab.Program, "specialize-entry-budget", "budget", "the check budget is exhausted",
        B.run(Elab.Program, Elab.specialize_kind(True{}, globals, definitions, families, rows,
          "Box", [Level.succ(Level.zero())], "DataBox", [], None{}), B.Polls{0n}))

def specialization_boundary() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    declarations : List<&2, Syntax.Decl> <- R.kernel(List<&2, Syntax.Decl>, Parser.program(Data.prenex_families_box()))
    state : Elab.Program <- R.kernel(Elab.Program, B.run(Elab.Program,
      Elab.program_loop(declarations, Elab.Program{G.empty(), [], [], []}), B.unlimited()))
    specialization_poll(state)

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read(root, "test/fixtures/prelude/prenex-families.mech")
    parsed : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.parse_roundtrip(source))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +globals : G.T = R.globals(program)
    checked : Unit <- R.assert(R.same_names(Test.family_names(globals), families()), "family inventory or symbolic isolation changed")
    checked : Unit <- R.assert(R.strings(R.entry_names(R.rows(program)), rows()) && Nat.is_eq(F.entry_count(globals), R.length(String, rows())), "entry inventory or row order changed")
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.lift(Unit, R.computations([C.Pair{"dataValue", Prenex.succ(Prenex.zero())}, C.Pair{"typeValue", Prenex.tiny()}, C.Pair{"listValue", Prenex.succ(Prenex.zero())}, C.Pair{"upType", Prenex.tiny()}, C.Pair{"boxValue", Prenex.succ(Prenex.zero())}], globals))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.prenex_families_negatives(), G.empty()))
    checked : Unit <- R.lift(Unit, Test.exhausted("budget", Data.prenex_families_box(), 0n))
    checked : Unit <- R.lift(Unit, Test.exhausted("specialize-budget", Data.prenex_families_instance(), 60n))
    checked : Unit <- R.lift(Unit, specialization_boundary())
    checked : Unit <- R.lift(Unit, Test.refusal(Elab.Checked, "lifetime", "unbound", "unknown universe schema Box", Elab.check_in(globals, B.unlimited(), "specialize Box (1) as Later")))
    checked : Unit <- R.lift(Unit, boundary())
    return "families=" ++ Nat.show(Test.family_count(globals)) ++ " entries=" ++ Nat.show(F.entry_count(globals)) ++ " computations=5 negatives=" ++ Nat.show((R.length(Data.Negative, Data.prenex_families_negatives()) + 4n : Nat))

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRENEX-FAMILIES", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
