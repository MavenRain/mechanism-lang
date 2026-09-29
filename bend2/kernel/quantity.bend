import Base
import ./common.bend as Common

type T is Data:
  QZero{}
  QOne{}
  QMany{}

def rank(q: T) -> U32:
  match q:
    case QZero{}:
      0
    case QOne{}:
      1
    case QMany{}:
      2

def equal(a: T, b: T) -> Bool:
  U32.is_eq(rank(a), rank(b))

def to_string(q: T) -> String:
  match q:
    case QZero{}:
      "0"
    case QOne{}:
      "1"
    case QMany{}:
      "w"

def runtime(q: T) -> T:
  match q:
    case QZero{}:
      QZero{}
    case QOne{}:
      QOne{}
    case QMany{}:
      QOne{}

def mul(a: T, b: T) -> T:
  match a b:
    case QZero{} _:
      QZero{}
    case QOne{} q:
      q
    case QMany{} QZero{}:
      QZero{}
    case QMany{} QOne{}:
      QMany{}
    case QMany{} QMany{}:
      QMany{}

def add(a: T, b: T) -> T:
  match a b:
    case QZero{} q:
      q
    case QOne{} QZero{}:
      QOne{}
    case QMany{} QZero{}:
      QMany{}
    case QOne{} QOne{}:
      QMany{}
    case QOne{} QMany{}:
      QMany{}
    case QMany{} QOne{}:
      QMany{}
    case QMany{} QMany{}:
      QMany{}

def minimum(+a: T, +b: T) -> T:
  Bool.pick(T, U32.is_le(rank(a), rank(b)), a, b)

def maximum(+a: T, +b: T) -> T:
  Bool.pick(T, U32.is_ge(rank(a), rank(b)), a, b)


type Interval is Data:
  Interval{lo: T, hi: T}

type Usage is Data:
  Usage{paths: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, reads: List<&2, Common.Pair<Nat, Interval>>}

def empty() -> Usage:
  Usage{Some{[]}, []}

def unreachable() -> Usage:
  Usage{None{}, []}

def interval(xs: List<&2, Common.Pair<Nat, Interval>>, +level: Nat) -> Interval:
  match xs:
    case Nil{}:
      Interval{QZero{}, QZero{}}
    case Con{Common.Pair{index, value}, rest}:
      Bool.pick(Interval, Nat.is_eq(index, level), value, interval(rest, level))

def occurrence_reads(level: Nat, +q: T) -> List<&2, Common.Pair<Nat, Interval>>:
  [Common.Pair{level, Interval{q, q}}]

def occurrence(level: Nat, q: T) -> Usage:
  +reads = occurrence_reads(level, q)
  Usage{Some{reads}, reads}

# Mode 0 adds counts; 1 takes minimum; 2 takes maximum; 3 keeps left.

def combine_mark(+mode: U32, +a: T, +b: T) -> T:
  Bool.pick(T, U32.is_eq(mode, 0), add(a, b), Bool.pick(T, U32.is_eq(mode, 1), minimum(a, b), Bool.pick(T, U32.is_eq(mode, 2), maximum(a, b), a)))

def combine_interval(a: Interval, b: Interval, lo_mode: U32, hi_mode: U32) -> Interval:
  match a b:
    case Interval{lo, hi} Interval{lo2, hi2}:
      Interval{combine_mark(lo_mode, lo, lo2), combine_mark(hi_mode, hi, hi2)}

def keys(xs: List<&2, Common.Pair<Nat, Interval>>) -> List<&2, Nat>:
  match xs:
    case Nil{}:
      []
    case Con{Common.Pair{key, value}, rest}:
      key <> keys(rest)

def contains(xs: List<&2, Nat>, +key: Nat) -> Bool:
  match xs:
    case Nil{}:
      False{}
    case Con{x, rest}:
      Bool.or(Nat.is_eq(x, key), contains(rest, key))

def unique(xs: List<&2, Nat>, +seen: List<&2, Nat>) -> List<&2, Nat>:
  match xs:
    case Nil{}:
      seen
    case Con{+x, rest}:
      unique(rest, Bool.pick(List<&2, Nat>, contains(seen, x), seen, x <> seen))

def combine_rows(rows: List<&2, Nat>, +a: List<&2, Common.Pair<Nat, Interval>>, +b: List<&2, Common.Pair<Nat, Interval>>, +lo_mode: U32, +hi_mode: U32) -> List<&2, Common.Pair<Nat, Interval>>:
  match rows:
    case Nil{}:
      []
    case Con{+key, rest}:
      Common.Pair{key, combine_interval(interval(a, key), interval(b, key), lo_mode, hi_mode)} <> combine_rows(rest, a, b, lo_mode, hi_mode)

def combine(+a: List<&2, Common.Pair<Nat, Interval>>, +b: List<&2, Common.Pair<Nat, Interval>>, lo_mode: U32, hi_mode: U32) -> List<&2, Common.Pair<Nat, Interval>>:
  combine_rows(unique(List.append(&2, Nat, keys(a), keys(b)), []), a, b, lo_mode, hi_mode)

def sequence_paths(a: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, b: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>) -> Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>:
  match a b:
    case None{} _:
      None{}
    case Some{x} None{}:
      None{}
    case Some{x} Some{y}:
      Some{combine(x, y, 0, 0)}

def sequence(a: Usage, b: Usage) -> Usage:
  match a b:
    case Usage{paths, reads} Usage{paths2, reads2}:
      Usage{sequence_paths(paths, paths2), combine(reads, reads2, 0, 0)}

def alternative_paths(a: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, b: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>) -> Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>:
  match a b:
    case None{} other:
      other
    case Some{x} None{}:
      Some{x}
    case Some{x} Some{y}:
      Some{combine(x, y, 1, 2)}

def alternative(a: Usage, b: Usage) -> Usage:
  match a b:
    case Usage{paths, reads} Usage{paths2, reads2}:
      Usage{alternative_paths(paths, paths2), combine(reads, reads2, 2, 2)}

def scale_map(xs: List<&2, Common.Pair<Nat, Interval>>, +lo: T, +hi: T) -> List<&2, Common.Pair<Nat, Interval>>:
  match xs:
    case Nil{}:
      []
    case Con{Common.Pair{key, Interval{a, b}}, rest}:
      Common.Pair{key, Interval{mul(lo, a), mul(hi, b)}} <> scale_map(rest, lo, hi)

def scale_paths(paths: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, lo: T, hi: T) -> Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>:
  match paths:
    case None{}:
      None{}
    case Some{xs}:
      Some{scale_map(xs, lo, hi)}

def scale_range(bounds: Interval, usage: Usage) -> Usage:
  match bounds usage:
    case Interval{+lo, +hi} Usage{paths, reads}:
      Usage{scale_paths(paths, lo, hi), scale_map(reads, lo, hi)}

def scale(q: T, usage: Usage) -> Usage:
  match q:
    case QZero{}:
      empty()
    case QOne{}:
      usage
    case QMany{}:
      scale_range(Interval{QMany{}, QMany{}}, usage)

def get_paths(paths: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, reads: Interval, level: Nat) -> Interval:
  match paths reads:
    case None{} Interval{lo, hi}:
      Interval{QOne{}, maximum(QOne{}, hi)}
    case Some{xs} _:
      interval(xs, level)

def get(usage: Usage, +level: Nat) -> Interval:
  match usage:
    case Usage{paths, reads}:
      get_paths(paths, interval(reads, level), level)

def remove_map(xs: List<&2, Common.Pair<Nat, Interval>>, +level: Nat) -> List<&2, Common.Pair<Nat, Interval>>:
  match xs:
    case Nil{}:
      []
    case Con{Common.Pair{+key, value}, rest}:
      +tail = remove_map(rest, level)
      Bool.pick(List<&2, Common.Pair<Nat, Interval>>, Nat.is_eq(key, level), tail, Common.Pair{key, value} <> tail)

def remove_paths(paths: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, level: Nat) -> Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>:
  match paths:
    case None{}:
      None{}
    case Some{xs}:
      Some{remove_map(xs, level)}

def remove(usage: Usage, +level: Nat) -> Usage:
  match usage:
    case Usage{paths, reads}:
      Usage{remove_paths(paths, level), remove_map(reads, level)}

def interval_once(value: Interval) -> Bool:
  match value:
    case Interval{lo, hi}:
      Bool.and(equal(lo, QOne{}), equal(hi, QOne{}))

def exactly_once(level: Nat, usage: Usage) -> Bool:
  interval_once(get(usage, level))

def interval_used(value: Interval) -> Bool:
  match value:
    case Interval{lo, hi}:
      Bool.not(equal(hi, QZero{}))

def used(level: Nat, usage: Usage) -> Bool:
  match usage:
    case Usage{paths, reads}:
      interval_used(interval(reads, level))

def interval_affine(value: Interval) -> Bool:
  match value:
    case Interval{lo, hi}:
      Bool.not(equal(hi, QMany{}))

def at_most_once(level: Nat, usage: Usage) -> Bool:
  interval_affine(get(usage, level))

def capture_paths(paths: Maybe<&2, List<&2, Common.Pair<Nat, Interval>>>, reads: List<&2, Common.Pair<Nat, Interval>>) -> List<&2, Common.Pair<Nat, Interval>>:
  match paths:
    case None{}:
      reads
    case Some{xs}:
      combine(xs, reads, 3, 2)

def captures(usage: Usage) -> Usage:
  match usage:
    case Usage{paths, +reads}:
      Usage{Some{capture_paths(paths, reads)}, reads}
