import Base
import ./relational_common.bend as R
import ../io.bend as F
import ../surface/parser.bend as Parser
import ../surface/syntax.bend as S
import ../surface/elab.bend as Elab
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/budget.bend as B
import ../kernel/error.bend as E
import ../kernel/positivity.bend as P

@unsafe def unspent() -> B.Callback:
  B.of_poll(unit => (unspent(), False{}))

# Count the exact Poll requests serviced by the callback runner. The wrapper
# preserves answers and results; the always-unspent callback supplies policy.
def counted_result(-A: Data, count: Nat, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, E.T, C.Pair<Nat, A>>:
  match result:
    case Fail{error}: Fail{error}
    case Done{value}: Done{C.Pair{count, value}}

@unsafe def count_step(-A: Data, -Z: Data, step: B.Step<A>, count: Nat, done: Result<&2, &2, E.T, C.Pair<Nat, A>> -> B.Step<Z>) -> B.Step<Z>:
  match step:
    case B.Finished{result}: done(counted_result(A, count, result))
    case B.Poll{resume}: B.Poll{spent => count_step(A, Z, resume(spent), Nat.add(count, 1n), done)}

def count_polls(-A: Data, action: B.Comp(A)) -> B.Comp(C.Pair<Nat, A>):
  B.CompLater{Z => done => count_step(A, Z, B.start_comp(A, A, action, result => B.Finished{result}), 0n, done)}

law prefix:
  for +last: String
  for members: List<&2, S.RecDef<S.T>>
  for reversed: List<&2, S.RecDef<S.T>>
  Result<&2, &2, String, List<&2, S.RecDef<S.T>>>

@unsafe def prefix_next(found: Bool, last: String, rest: List<&2, S.RecDef<S.T>>, reversed: List<&2, S.RecDef<S.T>>) -> Result<&2, &2, String, List<&2, S.RecDef<S.T>>>:
  match found:
    case True{}: Done{List.reverse(&2, S.RecDef<S.T>, reversed)}
    case False{}: prefix(last, rest, reversed)

@unsafe def prefix(last, members, reversed):
  match members:
    case Nil{}: Fail{"unknown last member: " ++ last}
    case Con{S.RecDef{+name, ty, body}, rest}:
      prefix_next(String.eq(name, last), last, rest, S.RecDef{name, ty, body} <> reversed)

def selected(last: Maybe<&2, String>, members: List<&2, S.RecDef<S.T>>) -> Result<&2, &2, String, List<&2, S.RecDef<S.T>>>:
  match last:
    case None{}: Done{members}
    case Some{name}: prefix(name, members, [])

def selection(parsed: List<&2, S.Decl>, last: Maybe<&2, String>) -> Result<&2, &2, String, C.Pair<List<&2, S.Decl>, Nat>>:
  match parsed:
    case Con{S.DPolyGroup{arity, family, companions, members}, Nil{}}:
      do Result<&2, &2, String, C.Pair<List<&2, S.Decl>, Nat>>:
        +members : List<&2, S.RecDef<S.T>> <- selected(last, members)
        return C.Pair{[S.DPolyGroup{arity, family, companions, members}], R.length(S.RecDef<S.T>, members)}
    case other: Fail{"the benchmark requires one family group"}

def report(checked: C.Pair<Nat, Elab.Checked>, members: Nat, elapsed: Nat) -> String:
  match checked:
    case C.Pair{polls, C.Pair{G.Globals{entries, families, entry_index, family_index}, rows}}:
      "{\"checked\":true,\"members\":" ++ Nat.show(members) ++ ",\"entries\":" ++ Nat.show(R.length(C.Pair<String, G.Entry>, rows)) ++ ",\"families\":" ++ Nat.show(R.length(C.Pair<String, P.Family>, families)) ++ ",\"polls\":" ++ Nat.show(polls) ++ ",\"elapsed_milliseconds\":" ++ Nat.show(elapsed) ++ ",\"allocated_words\":null,\"cpu_seconds\":null,\"unsupported_metrics\":[\"allocated_words\",\"cpu_seconds\"]}"

def measured_result(result: Result<&2, &2, E.T, C.Pair<Nat, Elab.Checked>>, members: Nat, elapsed: Nat) -> Result<&2, &2, String, String>:
  match result:
    case Fail{error}: Fail{E.to_string(error)}
    case Done{checked}: Done{report(checked, members, elapsed)}

def measured(selection: C.Pair<List<&2, S.Decl>, Nat>) -> R.Task(String):
  match selection:
    case C.Pair{parsed, members}: do IO<Result<&2, &2, String, String>>:
      start : Nat <- IO.now()
      checked : Result<&2, &2, E.T, C.Pair<Nat, Elab.Checked>> = B.run_poll(C.Pair<Nat, Elab.Checked>, count_polls(Elab.Checked, Elab.elab_program_comp(G.empty(), parsed)), unspent())
      stop : Nat <- IO.now()
      return measured_result(checked, members, Nat.sub(stop, start))

def suite(path: String, last: Maybe<&2, String>) -> R.Task(String):
  do R.Task<String>:
    source : String <- F.read_file(path)
    parsed : List<&2, S.Decl> <- R.lift(List<&2, S.Decl>, R.kernel(List<&2, S.Decl>, Parser.program(source)))
    selected : C.Pair<List<&2, S.Decl>, Nat> <- R.lift(C.Pair<List<&2, S.Decl>, Nat>, selection(parsed, last))
    measured(selected)

def finish(result: Result<&2, &2, String, String>) -> IO(Unit):
  match result:
    case Done{text}: IO.print(text)
    case Fail{message}: IO.die(Unit, 1, "TEMPLATE-COST-FAIL " ++ message)

def execute(path: String, last: Maybe<&2, String>) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- suite(path, last)
    finish(result)

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{path, Nil{}}: execute(path, None{})
    case Con{path, Con{"--through", Con{last, Nil{}}}}: execute(path, Some{last})
    case other: IO.die(Unit, 1, "usage: template_cost.exe SOURCE [--through MEMBER]")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
