import Base
import ../kernel/global.bend as G
import ../kernel/common.bend as C
import ../kernel/positivity.bend as P
import ./kernel_global_lookup.bend as L

@unsafe
def require(ok: Bool, label: String) -> IO(Unit):
  match ok:
    case True{}: IO.pure(Unit, Unit{})
    case False{}: IO.die(Unit, 1, label)

def names(-A: Data, rows: List<&2, C.Pair<String, A>>) -> List<&2, String>:
  match rows:
    case Nil{}: []
    case Con{C.Pair{name, value}, rest}: name <> names(A, rest)

def same_names(a: List<&2, String>, b: List<&2, String>) -> Bool:
  match a b:
    case Nil{} Nil{}: True{}
    case Con{x, xs} Con{y, ys}: String.eq(x, y) && same_names(xs, ys)
    case _ _: False{}

def inventory(globals: G.T, expected: List<&2, String>, expected_families: List<&2, String>) -> Bool:
  match globals:
    case G.Globals{entries, families, ei, fi}:
      same_names(names(G.Entry, entries), expected) && same_names(names(P.Family, families), expected_families)

def keys() -> List<&2, String>:
  ["", "a", "ab", "abc", "b", "é", "é", "😀", "中", SCon{Chr{0}, SNil{}}, SCon{Chr{0}, "a"}, "a😀", "😀a"]

def populate(keys: List<&2, String>, globals: G.T) -> G.T:
  match keys:
    case Nil{}: globals
    case Con{+key, rest}: populate(rest, G.add_family(key, L.family(key), G.add(key, L.entry(key), globals)))

@unsafe
def verify_keys(keys: List<&2, String>, +globals: G.T) -> IO(Unit):
  match keys:
    case Nil{}: IO.pure(Unit, Unit{})
    case Con{+key, rest}: do IO<Unit>:
      require(L.entry_is(G.find(key, globals), key), "entry key mismatch")
      require(L.family_is(G.find_family(key, globals), key), "family key mismatch")
      verify_keys(rest, globals)
      IO.pure(Unit, Unit{})

def stress_keys(count: Nat) -> List<&2, String>:
  match count:
    case Zero{}: []
    case Succ{+n}: Nat.show(n) <> stress_keys(n)

@unsafe
def fixed() -> IO(Unit):
  +start = G.of_lists(L.entries(), L.families())
  +changed = G.add("x", L.entry("replacement"), start)
  +family_changed = G.add_family("x", L.family("replacement"), changed)
  +removed = G.without_family("x", family_changed)
  +unicode = populate(keys(), G.empty())
  +stress = populate(stress_keys(1000n), G.empty())
  do IO<Unit>:
    require(L.entry_is(G.find("x", start), "first"), "raw duplicate first entry")
    require(L.family_is(G.find_family("x", start), "first"), "raw duplicate first family")
    require(L.entry_is(G.find("x", changed), "replacement"), "replacement lookup")
    require(inventory(changed, ["x", "y"], ["x", "y", "x"]), "replacement ordering")
    require(L.entry_is(G.find("x", start), "first"), "entry persistence")
    require(L.family_is(G.find_family("x", family_changed), "replacement"), "family replacement")
    require(inventory(family_changed, ["x", "y"], ["x", "y"]), "family ordering")
    require(L.family_is(G.find_family("x", changed), "first"), "family persistence")
    require(L.missing(P.Family, G.find_family("x", removed)), "family removal")
    require(L.entry_is(G.find("x", removed), "replacement"), "namespace separation")
    require(inventory(removed, ["x", "y"], ["y"]), "removal ordering")
    require(L.missing(G.Entry, G.find("absent", unicode)), "missing entry")
    require(L.missing(P.Family, G.find_family("absent", unicode)), "missing family")
    verify_keys(keys(), unicode)
    verify_keys(stress_keys(1000n), stress)
    IO.pure(Unit, Unit{})

def dynamic_keys(args: List<String>) -> List<&2, String>:
  match args:
    case Nil{}: []
    case Con{+key, rest}:
      key <> SCon{Chr{0},key} <> (key ++ SCon{Chr{0},SNil{}}) <> dynamic_keys(rest)

@unsafe
def boundary(+key: String) -> IO(Unit):
  +other = key ++ "/other"
  +missing = key ++ "/missing"
  es : List<&2,C.Pair<String,G.Entry>> = [C.Pair{key,L.entry("first")},C.Pair{other,L.entry("other")},C.Pair{key,L.entry("last")}]
  fs : List<&2,C.Pair<String,P.Family>> = [C.Pair{key,L.family("first")},C.Pair{other,L.family("other")},C.Pair{key,L.family("last")}]
  +raw = G.of_lists(es,fs)
  +changed = G.add(key,L.entry("new-entry"),raw)
  +fchanged = G.add_family(key,L.family("new-family"),changed)
  +removed = G.without_family(key,fchanged)
  +readded = G.add_family(key,L.family("again"),removed)
  +fresh = G.add(missing,L.entry("fresh"),fchanged)
  do IO<Unit>:
    require(L.entry_is(G.find(key,raw),"first"),"dynamic duplicate entry")
    require(L.family_is(G.find_family(key,raw),"first"),"dynamic duplicate family")
    require(inventory(raw,[key,other,key],[key,other,key]),"raw order")
    require(L.entry_is(G.find(key,changed),"new-entry"),"replace entry")
    require(L.family_is(G.find_family(key,changed),"first"),"independent namespaces")
    require(inventory(changed,[key,other],[key,other,key]),"entry replacement order")
    require(L.family_is(G.find_family(key,fchanged),"new-family"),"replace family")
    require(inventory(fchanged,[key,other],[key,other]),"family replacement order")
    require(L.missing(P.Family,G.find_family(key,removed)),"family delete")
    require(L.entry_is(G.find(key,removed),"new-entry"),"delete preserves entry")
    require(L.family_is(G.find_family(other,removed),"other"),"delete preserves neighbor")
    require(inventory(removed,[key,other],[other]),"delete order")
    require(L.family_is(G.find_family(key,readded),"again"),"reinsert deleted family")
    require(inventory(readded,[key,other],[key,other]),"reinsert order")
    require(L.entry_is(G.find(missing,fresh),"fresh"),"fresh insert")
    require(inventory(fresh,[missing,key,other],[key,other]),"fresh insertion order")
    require(inventory(G.without_family(missing,raw),[key,other,key],[key,other,key]),"absent delete")
    require(L.missing(G.Entry,G.find(missing,raw)),"missing entry")
    require(L.missing(P.Family,G.find_family(missing,raw)),"missing family")
    require(L.entry_is(G.find(key,raw),"first"),"entry persistence")
    require(L.family_is(G.find_family(key,raw),"first"),"family persistence")
    IO.pure(Unit,Unit{})

@unsafe
def dynamic_cases(keys: List<&2,String>, +globals: G.T) -> IO(Unit):
  match keys:
    case Nil{}: IO.pure(Unit,Unit{})
    case Con{+key,rest}:
      +updated = G.add_family(key,L.family("updated-family"),G.add(key,L.entry("updated-entry"),globals))
      +removed = G.without_family(key,updated)
      do IO<Unit>:
        boundary(key)
        require(L.entry_is(G.find(key,updated),"updated-entry"),"pooled replace entry")
        require(L.family_is(G.find_family(key,updated),"updated-family"),"pooled replace family")
        require(L.missing(P.Family,G.find_family(key,removed)),"pooled delete family")
        require(L.entry_is(G.find(key,globals),key),"pooled entry persistence")
        require(L.family_is(G.find_family(key,globals),key),"pooled family persistence")
        dynamic_cases(rest,globals)

@unsafe
def arguments(args: List<String>) -> IO(Unit):
  +runtime_keys = dynamic_keys(args)
  +globals = populate(runtime_keys, G.empty())
  do IO<Unit>:
    require(Bool.not(List.is_empty(&2, String, runtime_keys)), "runtime keys required")
    fixed()
    verify_keys(runtime_keys, globals)
    dynamic_cases(runtime_keys, globals)
    verify_keys(runtime_keys, globals)
    IO.print("GLOBAL-INDEX-OK boundaries=13 unicode=13 stress=1000 namespaces=2 runtime-keys=" ++ Nat.show(List.length(&2, String, runtime_keys)) ++ " per-key=30")

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
