import Base
import ./relational_negatives.bend as Neg

def require(label: String, valid: Bool) -> IO(Unit):
  match valid:
    case True{}: IO.pure(Unit, Unit{})
    case False{}: IO.die(Unit, 1, "RELATIONAL-ORACLE-FAIL " ++ label)

def suite(+seed: String) -> IO(Unit):
  +prefix = "quantity: " ++ seed
  +pin : Neg.Pin = Neg.Pin{"self", "", prefix, "0123456789abcdef0123456789abcdef"}
  +other : Neg.Pin = Neg.Pin{"other", "", "mismatch: unrelated", "1123456789abcdef0123456789abcdef"}
  +duplicate : Neg.Pin = Neg.Pin{"other", "", prefix, "0123456789abcdef0123456789abcdef"}
  +longer : Neg.Pin = Neg.Pin{"other", "", prefix ++ " suffix", ""}
  +shorter : Neg.Pin = Neg.Pin{"other", "", "quantity:", ""}
  +short : Neg.Pin = Neg.Pin{"short", "", "mismatch:", ""}
  +bad_digest : Neg.Pin = Neg.Pin{"bad", "", prefix, "0123456789abcdef0123456789abcde"}
  do IO<Unit>:
    ok : Unit <- require("self prefix", Neg.distinct_from(pin, [pin], False{}))
    ok : Unit <- require("different prefix", Neg.distinct_from(pin, [pin, other], False{}))
    ok : Unit <- require("duplicate prefix", Bool.not(Neg.distinct_from(pin, [duplicate], False{})))
    ok : Unit <- require("prefix overlap", Bool.not(Neg.distinct_from(pin, [longer], False{})))
    ok : Unit <- require("prefix direction", Neg.distinct_from(pin, [shorter], False{}))
    ok : Unit <- require("self digest", Neg.distinct_from(pin, [pin], True{}))
    ok : Unit <- require("different digest", Neg.distinct_from(pin, [pin, other], True{}))
    ok : Unit <- require("duplicate digest", Bool.not(Neg.distinct_from(pin, [duplicate], True{})))
    ok : Unit <- require("valid prefix oracle", Neg.oracle_valid(pin, [pin, other], Neg.Prefix{9n, True{}, False{}}))
    ok : Unit <- require("duplicate prefix oracle", Bool.not(Neg.oracle_valid(pin, [pin, duplicate], Neg.Prefix{9n, True{}, False{}})))
    ok : Unit <- require("distinct exemption", Neg.oracle_valid(pin, [pin, duplicate], Neg.Prefix{9n, False{}, False{}}))
    ok : Unit <- require("strict minimum", Bool.not(Neg.oracle_valid(short, [short], Neg.Prefix{9n, False{}, False{}})))
    ok : Unit <- require("common prefix refused", Bool.not(Neg.oracle_valid(pin, [pin, longer], Neg.Prefix{9n, False{}, True{}})))
    ok : Unit <- require("different common prefix", Neg.oracle_valid(pin, [pin, other], Neg.Prefix{9n, False{}, True{}}))
    ok : Unit <- require("valid digest oracle", Neg.oracle_valid(pin, [pin, other], Neg.Digest{9n, 128, [], True{}, False{}}))
    ok : Unit <- require("digest length", Bool.not(Neg.oracle_valid(bad_digest, [bad_digest], Neg.Digest{9n, 128, [], False{}, False{}})))
    ok : Unit <- require("duplicate digest oracle", Bool.not(Neg.oracle_valid(pin, [pin, duplicate], Neg.Digest{9n, 128, [], True{}, False{}})))
    ok : Unit <- require("digest distinct exemption", Neg.oracle_valid(pin, [pin, duplicate], Neg.Digest{9n, 128, [], False{}, False{}}))
    IO.print("RELATIONAL-ORACLE-OK cases=18")

def arguments(args: List<String>) -> IO(Unit):
  match args:
    case Con{seed, Nil{}}: suite(seed)
    case other: IO.die(Unit, 2, "expected one oracle seed")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
