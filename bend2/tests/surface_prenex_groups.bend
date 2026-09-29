import Base
import ../kernel/bignum.bend as Bignum
import ./relational_common.bend as R
import ./surface_prenex_common.bend as Test
import ./surface_prenex_data.bend as Data
import ./surface_raw_common.bend as Raw
import ./surface_foundation_common.bend as F
import ../surface/elab.bend as Elab
import ../surface/syntax.bend as Syntax
import ../kernel/check.bend as Check
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/shape.bend as S
import ../kernel/term.bend as T
import ../kernel/level.bend as L
import ../kernel/rules.bend as Rules
import ../kernel/budget.bend as B
import ../kernel/error.bend as E

def rows() -> List<&2, String>:
  List.append(&2, String, R.prefixes(["Data", "Types", "Proofs", "Again"], ["unbox", "pack", "unpack", "level"]),
    ["dataValue", "typeValue", "proofValue", "againValue", "One_make", "oneValue", "plainValue"])

def families() -> List<&2, String>:
  ["Tiny", "Truth", "Data", "Data_Wrapped", "Types", "Types_Wrapped", "Proofs", "Proofs_Wrapped", "Again", "Again_Wrapped", "One", "Plain", "Plain_Second"]

def computations() -> List<&2, C.Pair<String, T.T>>:
  +zero : T.T = T.In{S.SMu{"Tiny", []}, T.ACtor{"zero"}, []}
  [C.Pair{"dataValue", T.In{S.SMu{"Tiny", []}, T.ACtor{"next"}, [zero]}},
   C.Pair{"typeValue", T.Lan{S.SMu{"Tiny", []}, Rules.diagram_of([])}}, C.Pair{"againValue", zero},
   C.Pair{"Data_level", T.Univ{L.succ(L.succ(L.zero()))}}, C.Pair{"Types_level", T.Univ{L.succ(L.zero())}},
   C.Pair{"oneValue", T.In{S.SMu{"One", []}, T.ACtor{"single"}, [zero]}},
   C.Pair{"plainValue", T.In{S.SMu{"Plain_Second", []}, T.ACtor{"second"}, [T.In{S.SMu{"Plain", []}, T.ACtor{"first"}, []}]}}]

def boundary() -> Result<&2, &2, String, Unit>:
  Raw.refusal(Check.Decl, E.Mismatch{"a prenex declaration requires a program catalog"},
    Elab.elab_decl(Syntax.DPolyGroup{1n, Syntax.Fam{"Empty", [], Syntax.SType{Bignum.zero()}, []}, [], []}, Check.make(G.empty(), B.unlimited(), 0n)))

def budgets() -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    checked : Unit <- Test.exhausted("declaration-budget", Data.prenex_groups_group(), 0n)
    ceiling : Nat <- Test.polls(Data.prenex_groups_group())
    checked : Unit <- Test.exhausted("instance-budget", Data.prenex_groups_instance(), ceiling)
    +templates : Nat <- Test.polls(Data.prenex_groups_first() ++ "poly (u) mu Second : Sort (succ u) with | second : Second ")
    +grouped : Nat <- Test.polls(Data.prenex_groups_first() ++ "and Second : Sort (succ u) with | second : Second ")
    R.require(Nat.is_lt(grouped, (1n + (2n * templates : Nat) : Nat)), "group elaboration repeats the universal check: " ++ Nat.show(grouped) ++ " polls against " ++ Nat.show(templates) ++ " for the same two families as templates")

def suite(root: String) -> R.Task(String):
  do R.Task<String>:
    +source : String <- R.read(root, "test/fixtures/prelude/prenex-groups.mech")
    parsed : List<&2, Syntax.Decl> <- R.lift(List<&2, Syntax.Decl>, R.parse_roundtrip(source))
    +program : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), source))
    +globals : G.T = R.globals(program)
    checked : Unit <- R.assert(R.strings(R.entry_names(R.rows(program)), rows()) && Nat.is_eq(F.entry_count(globals), R.length(String, rows())), "member inventory or row order changed")
    checked : Unit <- R.assert(R.same_names(Test.family_names(globals), families()), "family inventory or symbolic isolation changed")
    checked : Unit <- R.assert(R.strings(Elab.disclosure_names(R.rows(program)), []), "unexpected axiom disclosure")
    checked : Unit <- R.lift(Unit, F.audit(globals))
    checked : Unit <- R.lift(Unit, R.computations(computations(), globals))
    checked : Elab.Checked <- R.lift(Elab.Checked, R.checked(G.empty(), Data.prenex_groups_instance()))
    checked : Unit <- R.lift(Unit, Test.positives(Data.prenex_groups_positives()))
    checked : Unit <- R.lift(Unit, Test.negatives(Data.prenex_groups_negatives(), G.empty()))
    checked : Unit <- R.lift(Unit, Test.parsers(Data.prenex_groups_parsers(), False{}))
    checked : Unit <- R.lift(Unit, budgets())
    checked : Unit <- R.lift(Unit, Test.refusal(Elab.Checked, "catalog-lifetime", "unbound", "unknown universe schema Box", Elab.check_in(globals, B.unlimited(), "specialize Box (1, 2) as Later")))
    checked : Unit <- R.lift(Unit, boundary())
    return "families=" ++ Nat.show(Test.family_count(globals)) ++ " entries=" ++ Nat.show(F.entry_count(globals)) ++ " computations=7 negatives=" ++ Nat.show((R.length(Data.Negative, Data.prenex_groups_negatives()) + R.length(Data.ParseCase, Data.prenex_groups_parsers()) + 4n : Nat))

def arguments(args: List<String>) -> IO(Unit):
  R.run("PRENEX-GROUPS", suite(R.root(args)))

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
