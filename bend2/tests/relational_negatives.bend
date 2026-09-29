import Base
import ./relational_common.bend as R
import ./relational_digest.bend as MD5
import ../kernel/common.bend as C
import ../kernel/global.bend as G
import ../kernel/error.bend as E
import ../kernel/budget.bend as B
import ../surface/elab.bend as Elab

type Pin is Data:
  Pin{name: String, source: String, prefix: String, digest: String}

type Policy is Data:
  Prefix{minimum: Nat, distinct: Bool, common: Bool}
  Digest{minimum: Nat, limit: U32, full: List<&2, String>, distinct: Bool, nominal: Bool}

def needs_digest(policy: Policy) -> Bool:
  match policy:
    case Prefix{minimum, distinct, common}: False{}
    case Digest{minimum, limit, full, distinct, nominal}: True{}

def fingerprint(needed: Bool, root: String, path: String) -> R.Task(String):
  match needed:
    case False{}: R.Task.pure(String, "")
    case True{}: do R.Task<String>:
      value : String <- R.read(root, path ++ ".digest")
      return String.trim(value)

def load(names: List<&2, String>, +root: String, +directory: String, +policy: Policy) -> R.Task(List<&2, Pin>):
  match names:
    case Nil{}: R.Task.pure(List<&2, Pin>, Nil{})
    case Con{+name, rest}: do R.Task<List<&2, Pin>>:
      source : String <- R.read(root, directory ++ "/" ++ name ++ ".mech")
      expected : String <- R.read(root, directory ++ "/" ++ name ++ ".err")
      digest : String <- fingerprint(needs_digest(policy), root, directory ++ "/" ++ name)
      pins : List<&2, Pin> <- load(rest, root, directory, policy)
      return Con{Pin{name, source, String.trim(expected), digest}, pins}

def prefix_of_all(+prefix: String, others: List<&2, Pin>) -> Bool:
  match others:
    case Nil{}: True{}
    case Con{Pin{name, source, text, digest}, rest}: Bool.and(String.starts_with(text, prefix), prefix_of_all(prefix, rest))

def different_field(digest_mode: Bool, prefix: String, other_prefix: String, digest: String, other_digest: String) -> Bool:
  match digest_mode:
    case True{}: Bool.not(String.eq(digest, other_digest))
    case False{}: Bool.not(String.starts_with(other_prefix, prefix))

def distinct_row(same: Bool, digest_mode: Bool, prefix: String, other_prefix: String, digest: String, other_digest: String) -> Bool:
  match same:
    case True{}: True{}
    case False{}: different_field(digest_mode, prefix, other_prefix, digest, other_digest)

# Explicit branches preserve the native result for a self row followed by a
# different prefix. The equivalent nested Bool.pick/or expression returned
# false on Bend 2.0.25 C while JavaScript returned true.
def distinct_fields(+name: String, +prefix: String, +digest: String, others: List<&2, Pin>, +digest_mode: Bool) -> Bool:
  match others:
    case Nil{}: True{}
    case Con{Pin{other_name, other_source, other_prefix, other_digest}, rest}:
      Bool.and(distinct_row(String.eq(name, other_name), digest_mode, prefix, other_prefix, digest, other_digest), distinct_fields(name, prefix, digest, rest, digest_mode))

def distinct_from(pin: Pin, others: List<&2, Pin>, digest_mode: Bool) -> Bool:
  match pin:
    case Pin{name, source, prefix, digest}: distinct_fields(name, prefix, digest, others, digest_mode)

def oracle_valid(pin: Pin, +all: List<&2, Pin>, policy: Policy) -> Bool:
  match pin:
    case Pin{+name, +source, +prefix, +digest}:
      match policy:
        case Prefix{minimum, distinct, common}:
          Bool.and(Nat.is_gt(String.length(prefix), minimum), Bool.and(Bool.or(Bool.not(distinct), distinct_from(Pin{name, source, prefix, digest}, all, False{})), Bool.or(Bool.not(common), Bool.not(prefix_of_all(prefix, all)))))
        case Digest{minimum, limit, full, distinct, nominal}:
          Bool.and(Nat.is_gt(String.length(prefix), minimum), Bool.and(Nat.is_eq(String.length(digest), 32n), Bool.or(Bool.not(distinct), distinct_from(Pin{name, source, prefix, digest}, all, True{}))))

def message(result: Result<&2, &2, E.T, Elab.Checked>, name: String) -> Result<&2, &2, String, String>:
  match result:
    case Done{checked}: Fail{name ++ ": expected refusal"}
    case Fail{error}: Done{E.to_string(error)}

def check_digest(policy: Policy, name: String, message: String, digest: String) -> R.Task(Bool):
  match policy:
    case Prefix{minimum, distinct, common}: R.Task.pure(Bool, True{})
    case Digest{minimum, limit, full, distinct, nominal}: do IO<Result<&2, &2, String, Bool>>:
      got : String <- MD5.relational_digest(message, Bool.pick(U32, R.contains(full, name), 0, limit))
      return Done{String.eq(got, digest)}

def nominal_ok(policy: Policy, name: String, message: String) -> Bool:
  match policy:
    case Prefix{minimum, distinct, common}: True{}
    case Digest{minimum, limit, full, distinct, nominal}:
      Bool.or(Bool.not(Bool.and(nominal, String.eq(name, "nominal-target"))), R.contains(String.split(message, ' '), "SeparateTarget"))

def check_one(pin: Pin, +all: List<&2, Pin>, +globals: G.T, +policy: Policy) -> R.Task(Unit):
  match pin:
    case Pin{+name, +source, +prefix, +digest}: do R.Task<Unit>:
      ok : Unit <- R.assert(oracle_valid(Pin{name, source, prefix, digest}, all, policy), name ++ ": invalid refusal oracle")
      +actual : String <- R.lift(String, message(Elab.check_in(globals, B.unlimited(), source), name))
      hash_ok : Bool <- check_digest(policy, name, actual, digest)
      R.assert(Bool.and(String.starts_with(actual, prefix), Bool.and(hash_ok, nominal_ok(policy, name, actual))), name ++ ": wrong refusal: " ++ actual)

def check_all(pins: List<&2, Pin>, +all: List<&2, Pin>, +globals: G.T, +policy: Policy) -> R.Task(Unit):
  match pins:
    case Nil{}: R.Task.pure(Unit, Unit{})
    case Con{pin, rest}: do R.Task<Unit>:
      ok : Unit <- check_one(pin, all, globals, policy)
      check_all(rest, all, globals, policy)

def run(+root: String, +directory: String, +names: List<&2, String>, globals: G.T, +policy: Policy) -> R.Task(Unit):
  do R.Task<Unit>:
    ok : Unit <- R.negative_inventory(root, directory, names)
    +pins : List<&2, Pin> <- load(names, root, directory, policy)
    check_all(pins, pins, globals, policy)
