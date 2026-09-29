import Base
import ../cli/output.bend as Out
import ../io.bend as FileIO
import ./relational_fs.bend as FS
import ./relational_syntax_equal.bend as Same
import ../surface/syntax.bend as S
import ../surface/parser.bend as Parser
import ../surface/elab.bend as Elab
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/error.bend as E
import ../kernel/term.bend as T
import ../kernel/shape.bend as Shape
import ../kernel/quantity.bend as Q
import ../kernel/bignum.bend as N
import ../kernel/literal.bend as Literal
import ../kernel/value.bend as V
import ../kernel/prim.bend as Prim
import ../kernel/positivity.bend as P
import ../kernel/budget.bend as B
import ../kernel/eval.bend as Eval
import ../kernel/check.bend as Check
import ../kernel/conv.bend as Conv
import ../kernel/pp.bend as Pp
import ../kernel/rules.bend as Rules

def Task(-A: Data) -> Type:
  IO(Result<&2, &2, String, A>)

def lift(-A: Data, result: Result<&2, &2, String, A>) -> Task(A):
  IO.pure(Result<&2, &2, String, A>, result)

def Task.pure(-A: Data, value: A) -> Task(A):
  lift(A, Done{value})

def task_next(-A: Data, -B: Data, result: Result<&2, &2, String, A>, next: A -> Task(B)) -> Task(B):
  match result:
    case Fail{message}: lift(B, Fail{message})
    case Done{value}: next(value)

def Task.bind(-A: Data, -B: Data, first: Task(A), next: A -> Task(B)) -> Task(B):
  IO.bind(Result<&2, &2, String, A>, Result<&2, &2, String, B>, first, result => task_next(A, B, result, next))

def kernel(-A: Data, result: Result<&2, &2, E.T, A>) -> Result<&2, &2, String, A>:
  match result:
    case Fail{error}: Fail{E.to_string(error)}
    case Done{value}: Done{value}

def require(ok: Bool, message: String) -> Result<&2, &2, String, Unit>:
  Bool.pick(Result<&2, &2, String, Unit>, ok, Done{Unit{}}, Fail{message})

def assert(ok: Bool, message: String) -> Task(Unit):
  lift(Unit, require(ok, message))

def read(root: String, path: String) -> Task(String):
  FileIO.read_file(root ++ "/" ++ path)

@unsafe def read_all(+root: String, paths: List<&2, String>) -> Task(String):
  match paths:
    case Nil{}: Task.pure(String, "")
    case Con{path, rest}: do Task<String>:
      head : String <- read(root, path)
      tail : String <- read_all(root, rest)
      return "\n" ++ head ++ tail

def directory(root: String, +path: String) -> Task(List<&2, String>):
  do IO<Result<&2, &2, String, List<&2, String>>>:
    result : Result<&1, &1, U32 & String, List<&2, String>> <- FS.relational_list_dir(root ++ "/" ++ path)
    return FileIO.convert(List<&2, String>, result, path)

def root(args: List<String>) -> String:
  match args:
    case Nil{}: "."
    case Con{name, rest}: name

def length(-A: Data, values: List<&2, A>) -> Nat:
  List.length(&2, A, values)

def strings(a: List<&2, String>, b: List<&2, String>) -> Bool:
  match a b:
    case Nil{} Nil{}: True{}
    case Con{x, xs} Con{y, ys}: String.eq(x, y) && strings(xs, ys)
    case _ _: False{}

def sorted(values: List<&2, String>) -> List<&2, String>:
  List.sort(String, String.is_le, values)

def same_names(a: List<&2, String>, b: List<&2, String>) -> Bool:
  strings(sorted(a), sorted(b))

def prefix(+name: String, members: List<&2, String>) -> List<&2, String>:
  match members:
    case Nil{}: []
    case Con{member, rest}: (name ++ "_" ++ member) <> prefix(name, rest)

def prefixes(names: List<&2, String>, +members: List<&2, String>) -> List<&2, String>:
  match names:
    case Nil{}: []
    case Con{name, rest}: List.append(&2, String, prefix(name, members), prefixes(rest, members))

def contains(names: List<&2, String>, +name: String) -> Bool:
  match names:
    case Nil{}: False{}
    case Con{head, tail}: String.eq(head, name) || contains(tail, name)

def parse_roundtrip(source: String) -> Result<&2, &2, String, List<&2, S.Decl>>:
  do Result<&2, &2, String, List<&2, S.Decl>>:
    +parsed : List<&2, S.Decl> <- kernel(List<&2, S.Decl>, Parser.program(source))
    printed : List<&2, S.Decl> <- kernel(List<&2, S.Decl>, Parser.program(S.print(parsed)))
    ok : Unit <- require(Same.declarations(parsed, printed), "parse/print changed")
    return parsed

def checked(globals: G.T, source: String) -> Result<&2, &2, String, Elab.Checked>:
  kernel(Elab.Checked, Elab.check_in(globals, B.unlimited(), source))

def elaborated(globals: G.T, parsed: List<&2, S.Decl>) -> Result<&2, &2, String, Elab.Checked>:
  kernel(Elab.Checked, Elab.elab_program_in(globals, B.unlimited(), parsed))

def entry_names(rows: Elab.Rows) -> List<&2, String>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{name, entry}, tail}: name <> entry_names(tail)

def optional_nat(a: Maybe<&2, Nat>, b: Maybe<&2, Nat>) -> Bool:
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: Nat.is_eq(x, y)
    case _ _: False{}

def same_entry(a: G.Entry, b: G.Entry) -> Bool:
  match a b:
    case G.Def{G.DefEntry{ty, body, red, rec, part}} G.Def{G.DefEntry{ty2, body2, red2, rec2, part2}}:
      T.equal(ty, ty2) && T.equal(body, body2) && Bool.not(Bool.xor(red, red2)) && optional_nat(rec, rec2) && Bool.not(Bool.xor(part, part2))
    case G.Axiom{ty} G.Axiom{ty2}: T.equal(ty, ty2)
    case G.Prim{ty, prim} G.Prim{ty2, prim2}: T.equal(ty, ty2) && String.eq(Prim.name(prim), Prim.name(prim2))
    case _ _: False{}

def audit_entry(initial: Maybe<&2, G.Entry>, name: String, entry: G.Entry) -> Result<&2, &2, String, Unit>:
  match initial:
    case Some{original}: require(same_entry(original, entry), "builtin changed: " ++ name)
    case None{}:
      match entry:
        case G.Def{value}: Done{Unit{}}
        case other: Fail{"trusted entry: " ++ name}

def audit_entries(entries: Elab.Rows, +initial: G.T) -> Result<&2, &2, String, Unit>:
  match entries:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{+name, entry}, rest}: do Result<&2, &2, String, Unit>:
      ok : Unit <- audit_entry(G.find(name, initial), name, entry)
      audit_entries(rest, initial)

def new_family(original: Maybe<&2, P.Family>, row: C.Pair<String, P.Family>, rest: List<&2, C.Pair<String, P.Family>>) -> List<&2, C.Pair<String, P.Family>>:
  match original:
    case Some{value}: rest
    case None{}: row <> rest

def new_families(rows: List<&2, C.Pair<String, P.Family>>, +initial: G.T) -> List<&2, C.Pair<String, P.Family>>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{+name, family}, rest}: new_family(G.find_family(name, initial), C.Pair{name, family}, new_families(rest, initial))

def family_names(rows: List<&2, C.Pair<String, P.Family>>) -> List<&2, String>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{name, family}, rest}: name <> family_names(rest)

def positive(family: P.Family, name: String) -> Result<&2, &2, String, Unit>:
  match family:
    case P.Family{n, p, i, l, P.Complete{constructors}, ctors, good}: require(good, "unchecked family: " ++ name)
    case other: Fail{"unexpected family: " ++ name}

def audit_families(rows: List<&2, C.Pair<String, P.Family>>) -> Result<&2, &2, String, Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{name, family}, rest}: do Result<&2, &2, String, Unit>:
      ok : Unit <- positive(family, name)
      audit_families(rest)

def inventories(+initial: G.T, globals: G.T, +rows: Elab.Rows, expected_entries: List<&2, String>, expected_families: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: do Result<&2, &2, String, Unit>:
      ok : Unit <- require(same_names(entry_names(rows), expected_entries), "entry inventory changed")
      ok : Unit <- require(strings(Elab.axiom_names(rows), []), "unexpected axioms")
      ok : Unit <- audit_entries(entries, initial)
      +families : List<&2, C.Pair<String, P.Family>> = new_families(families, initial)
      ok : Unit <- require(same_names(family_names(families), expected_families), "family inventory changed")
      audit_families(families)

def normalize(+globals: G.T, name: String) -> Result<&2, &2, String, T.T>:
  do Result<&2, &2, String, T.T>:
    value : V.T <- kernel(V.T, Eval.eval(globals, [], T.Global{name}))
    kernel(T.T, Eval.quote(globals, 0n, value))

def computations(rows: List<&2, C.Pair<String, T.T>>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match rows:
    case Nil{}: Done{Unit{}}
    case Con{C.Pair{+name, expected}, rest}: do Result<&2, &2, String, Unit>:
      +actual : T.T <- normalize(globals, name)
      ok : Unit <- require(T.equal(actual, expected), "wrong computation: " ++ name ++ " = " ++ Pp.term([], actual))
      computations(rest, globals)

def natural(value: U32) -> T.T:
  T.Lit{Literal.LInt{N.of_u32(value)}}

def tiny(value: Nat) -> T.T:
  match value:
    case 0n: T.In{Shape.SMu{"Tiny", []}, T.ACtor{"zero"}, []}
    case 1n+rest: T.In{Shape.SMu{"Tiny", []}, T.ACtor{"next"}, [tiny(rest)]}

def filename_entry(keep: Bool, +name: String, rest: List<&2, String>) -> List<&2, String>:
  match keep:
    case True{}: String.take(name, Nat.sub(String.length(name), 5n)) <> rest
    case False{}: rest

def filenames(names: List<&2, String>) -> List<&2, String>:
  match names:
    case Nil{}: []
    case Con{+name, rest}:
      filename_entry(String.ends_with(name, ".mech"), name, filenames(rest))

def negative_inventory(root: String, path: String, expected: List<&2, String>) -> Task(Unit):
  do Task<Unit>:
    names : List<&2, String> <- directory(root, path)
    assert(same_names(filenames(names), expected), "negative inventory changed")

def refuse_exact(result: Result<&2, &2, E.T, Elab.Checked>, expected: String, name: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}: Fail{name ++ ": expected refusal"}
    case Fail{error}:
      +actual = E.to_string(error) ++ "\n"
      require(String.eq(actual, expected), name ++ ": " ++ actual)

def negative_exact(+root: String, +directory: String, names: List<&2, String>, +globals: G.T) -> Task(Unit):
  match names:
    case Nil{}: Task.pure(Unit, Unit{})
    case Con{+name, rest}: do Task<Unit>:
      text : String <- read(root, directory ++ name ++ ".mech")
      expected : String <- read(root, directory ++ name ++ ".err")
      ok : Unit <- lift(Unit, refuse_exact(Elab.check_in(globals, B.unlimited(), text), expected, name))
      negative_exact(root, directory, rest, globals)

@unsafe def refuse_callback() -> B.Callback:
  B.of_poll(unit => (refuse_callback(), True{}))

def budget_error(result: Result<&2, &2, E.T, Elab.Checked>) -> Bool:
  match result:
    case Fail{E.Budget_exhausted{message}}: String.eq(message, "the check budget is exhausted")
    case other: False{}

def budget_refusal(source: String) -> Result<&2, &2, String, Unit>:
  do Result<&2, &2, String, Unit>:
    parsed : List<&2, S.Decl> <- kernel(List<&2, S.Decl>, Parser.program(source))
    require(budget_error(B.run_poll(Elab.Checked, Elab.elab_program_comp(G.empty(), parsed), refuse_callback())), "budget: expected immediate exhaustion")

def finish(tag: String, result: Result<&2, &2, String, String>) -> IO(Unit):
  match result:
    case Done{fields}: IO.print(tag ++ "-OK " ++ fields)
    case Fail{message}: do IO<Unit>:
      IO.print(tag ++ "-FAIL " ++ message)
      Out.cli_exit(1)

def run(tag: String, action: Task(String)) -> IO(Unit):
  do IO<Unit>:
    result : Result<&2, &2, String, String> <- action
    finish(tag, result)

def globals(checked: Elab.Checked) -> G.T:
  match checked:
    case C.Pair{globals, rows}: globals

def rows(checked: Elab.Checked) -> Elab.Rows:
  match checked:
    case C.Pair{globals, rows}: rows

def empty_symbolic(checked: Elab.Checked) -> Bool:
  match checked:
    case C.Pair{G.Globals{Nil{}, Nil{}, entry_index, family_index}, Nil{}}: True{}
    case other: False{}

def counted_inventory(checked: Elab.Checked, +count: Nat, family_names_expected: List<&2, String>) -> Result<&2, &2, String, Unit>:
  match checked:
    case C.Pair{G.Globals{+entries, +families, entry_index, family_index}, +rows}: do Result<&2, &2, String, Unit>:
      ok : Unit <- require(Nat.is_eq(length(C.Pair<String, G.Entry>, rows), count) && Nat.is_eq(length(C.Pair<String, G.Entry>, entries), count), "entry inventory changed")
      ok : Unit <- require(same_names(family_names(families), family_names_expected), "family inventory changed")
      ok : Unit <- require(strings(Elab.axiom_names(rows), []), "unexpected axioms")
      ok : Unit <- audit_entries(entries, G.empty())
      audit_families(families)

def all_families(globals: G.T) -> Result<&2, &2, String, Unit>:
  match globals:
    case G.Globals{entries, families, entry_index, family_index}: audit_families(families)

def found(entry: Maybe<&2, G.Entry>) -> Bool:
  match entry:
    case None{}: False{}
    case Some{value}: True{}

def required(names: List<&2, String>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{+name, rest}: do Result<&2, &2, String, Unit>:
      ok : Unit <- require(found(G.find(name, globals)), "missing member: " ++ name)
      required(rest, globals)

def tiny_type() -> T.T:
  T.Lan{Shape.SMu{"Tiny", []}, Rules.diagram_of([])}

def quotations(names: List<&2, String>, +globals: G.T) -> Result<&2, &2, String, Unit>:
  match names:
    case Nil{}: Done{Unit{}}
    case Con{+name, rest}: do Result<&2, &2, String, Unit>:
      +term : T.T = T.Global{name}
      +expected : V.T <- kernel(V.T, Check.infer(Check.make(globals, B.unlimited(), 0n), Q.QZero{}, term))
      +value : V.T <- kernel(V.T, Eval.eval(globals, [], term))
      +quoted : T.T <- kernel(T.T, Eval.quote(globals, 0n, value))
      ok : Unit <- kernel(Unit, Check.check_term(globals, B.unlimited(), quoted, expected))
      quoted_value : V.T <- kernel(V.T, Eval.eval(globals, [], quoted))
      same : Bool <- kernel(Bool, Conv.conv(Check.make(globals, B.unlimited(), 0n), expected, value, quoted_value))
      ok : Unit <- require(same, "quotation changed " ++ name)
      quotations(rest, globals)
