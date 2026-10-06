#!/usr/bin/env python3
"""C3 inference regressions. Build once, or reuse a driver with --driver."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
HEADER = "//! Emitted by `mech rust-out` from `demo.mech`.\n\n"
LIB = "//! Emitted by `mech rust-out`.\n\npub mod demo;\n"
COPY = "#[derive(Clone, Copy, Debug, PartialEq, Eq)]\n"
CLONE = "#[derive(Clone, Debug, PartialEq, Eq)]\n"
ONE = COPY + "pub enum Foo { Bar(bool) }\n"
TWO = COPY + "pub enum Foo { Bar, Baz }\n"
GEN = CLONE + "pub enum Foo<A> { Bar(A) }\n"
UNIT = COPY + "pub struct Unit;\npub fn unit() -> Unit { Unit }\n"
ROW = re.compile(r"(\S+) (\S+) : (.+) annotated (\d+)")


# Each negative is a resolved source that the original inference pass accepted.
NEGATIVES = [
    ("extra pattern field", ONE + "pub fn bad(x: Foo) -> bool { match x { Foo::Bar(y, _) => y, } }", "pattern field count (CD9)"),
    ("missing pattern field", ONE + "pub fn bad(x: Foo) -> bool { match x { Foo::Bar => true, } }", "pattern field count (CD9)"),
    ("missing match arm", TWO + "pub fn bad(x: Foo) -> bool { match x { Foo::Bar => true, } }", "match arms (CD9)"),
    ("duplicate match arm", TWO + "pub fn bad(x: Foo) -> bool { match x { Foo::Bar => true, Foo::Bar => false, Foo::Baz => false, } }", "match arms (CD9)"),
    ("empty match on inhabited family", TWO + "pub fn bad(x: Foo) -> bool { match x {} }", "match arms (CD9)"),
    ("missing family type argument", GEN + "pub fn bad(x: Foo) -> Foo { x }", "type argument count (CD9)"),
    ("extra family type argument", TWO + "pub fn bad(x: Foo<bool>) -> Foo<bool> { x }", "type argument count (CD9)"),
    ("applied generic type variable", "pub fn bad<A: Clone>(x: A<bool>) -> A<bool> { x }", "type argument count (CD9)"),
    ("extra unit type argument", UNIT + "pub fn bad(x: Unit<bool>) -> Unit<bool> { x }", "type argument count (CD9)"),
    ("bad nested function type", GEN + "pub fn bad(f: &impl Fn(Foo) -> bool) -> bool { true }", "type argument count (CD9)"),
    ("bad enum field type", CLONE + "pub enum Foo<A> { Bar(Box<Foo>) }", "type argument count (CD9)"),
    ("bad unused call type argument", GEN + "pub fn ignore<T: Clone>(x: bool) -> bool { x }\npub fn bad(x: bool) -> bool { ignore::<Foo>(x) }", "type argument count (CD9)"),
    ("type arguments on local callback", "pub fn bad(f: &impl Fn(bool) -> bool, x: bool) -> bool { f::<bool>(x) }", "argument count (CD9)"),
    ("type arguments on local scrutinee call", TWO + "pub fn bad(f: &impl Fn(bool) -> Foo, x: bool) -> bool { match f::<bool>(x) { Foo::Bar => true, Foo::Baz => false, } }", "argument count (CD9)"),
]


# Existing refusals must keep their diagnostics.
EXISTING_NEGATIVES = [
    ("generic constructor scrutinee", GEN + "pub fn bad<A: Clone>(x: &A) -> bool { match Foo::Bar(x.clone()) { Foo::Bar(_) => true, } }", "constructor of a generic family with no expected type (CD9)"),
    ("wrong function argument count", "pub fn id(x: bool) -> bool { x }\npub fn bad() -> bool { id() }", "argument count (CD9)"),
    ("wrong function argument type", "pub fn id(x: bool) -> bool { x }\npub fn bad() -> bool { id(0) }", "type mismatch (CD9)"),
    ("wrong closure argument count", "pub fn apply(f: &impl Fn(bool) -> bool, x: bool) -> bool { f(x) }\npub fn bad(x: bool) -> bool { apply(&|y: bool, z: bool| y, x) }", "type mismatch (CD9)"),
]


# Positive cases cover erased wrappers, nested annotations and generic scopes.
POSITIVES = [
    ("identity", "pub fn id(x: bool) -> bool { x }", [("id", 0)]),
    ("let with annotation", "pub fn good(x: bool) -> bool { let y: bool = x; y }", [("good", 0)]),
    ("let without annotation", "pub fn good(x: bool) -> bool { let y = x; y }", [("good", 0)]),
    ("unit signature", UNIT + "pub fn id(x: Unit) -> Unit { x }", [("id", 0)]),
    ("generic constructor", GEN + "pub fn good<A: Clone>(x: &A) -> Foo<A> { Foo::Bar(x.clone()) }", [("good", 1)]),
    ("nested generic constructors", GEN + "pub fn good<A: Clone>(x: &A) -> Foo<Foo<A>> { Foo::Bar(Foo::Bar(x.clone())) }", [("good", 2)]),
    ("generic match", GEN + "pub fn good<A: Clone>(x: &Foo<A>) -> A { match x { Foo::Bar(y) => y.clone(), } }", [("good", 1)]),
    ("match with wildcard field", ONE + "pub fn good(x: Foo) -> bool { match x { Foo::Bar(_) => true, } }", [("good", 1)]),
    ("reordered exhaustive match", TWO + "pub fn good(x: Foo) -> bool { match x { Foo::Baz => false, Foo::Bar => true, } }", [("good", 1)]),
    ("empty family elimination", COPY + "pub enum Void {}\npub fn good(x: Void) -> bool { match x {} }", [("good", 1)]),
    ("non generic constructor scrutinee", TWO + "pub fn good() -> bool { match Foo::Bar { Foo::Bar => true, Foo::Baz => false, } }", [("good", 2)]),
    ("valid recursive field", CLONE + "pub enum Foo<A> { Bar(Box<Foo<A>>), Baz(A) }", []),
    ("local callback", "pub fn apply(f: &impl Fn(bool) -> bool, x: bool) -> bool { f(x) }", [("apply", 0)]),
    ("typed closure", "pub fn apply(f: &impl Fn(bool) -> bool, x: bool) -> bool { f(x) }\npub fn good(x: bool) -> bool { apply(&|y: bool| y, x) }", [("apply", 0), ("good", 0)]),
    ("call with scoped generic", "pub fn id<A: Clone>(x: &A) -> A { x.clone() }\npub fn good<B: Clone>(x: &B) -> B { id::<B>(x) }", [("id", 0), ("good", 0)]),
    ("call with family type argument", GEN + "pub fn id<A: Clone>(x: &A) -> A { x.clone() }\npub fn good(x: &Foo<bool>) -> Foo<bool> { id::<Foo<bool>>(x) }", [("id", 0), ("good", 0)]),
]


def run(driver, mode, lib, modules):
    args = ["node", "--stack-size=16384", str(driver), mode, lib]
    for name, source in modules:
        args.extend([name, source])
    result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60)
    if result.returncode or result.stderr:
        raise RuntimeError(f"driver exit={result.returncode}: {result.stderr[:1000]}")
    return result.stdout


def rows(output):
    matches = [ROW.fullmatch(line) for line in output.splitlines()]
    if any(match is None for match in matches):
        raise ValueError(f"unexpected type output: {output[:500]}")
    return [match.groups() for match in matches]


def check(driver, golden, baseline):
    failed = 0
    for name, source, diagnostic in NEGATIVES + EXISTING_NEGATIVES:
        output = run(driver, "types", LIB, [("demo", HEADER + source + "\n")])
        pattern = r"REFUSED src/demo\.rs:\d+:1: " + re.escape(diagnostic) + "\n"
        if re.fullmatch(pattern, output) is None:
            print(f"FAIL {name}: expected {diagnostic!r}, got {output.strip()!r}")
            failed += 1
    for name, source, expected in POSITIVES:
        output = run(driver, "types", LIB, [("demo", HEADER + source + "\n")])
        try:
            actual = [(row[1], int(row[3])) for row in rows(output)]
            if actual != expected:
                raise ValueError(f"expected {expected}, got {actual}")
        except ValueError as error:
            print(f"FAIL {name}: {error}")
            failed += 1

    lib = (golden / "lib.rs").read_text()
    modules = [(name, (golden / (name + ".rs")).read_text()) for name in ("init", "nat", "second_price")]
    typed = run(driver, "types", lib, modules)
    resolved = run(driver, "resolve", lib, modules)
    try:
        actual = rows(typed)
        fn_names = [(line.split()[0], line.split()[4]) for line in resolved.splitlines() if re.fullmatch(r"\S+ fn \S+ -> \S+", line)]
        if len(actual) != 22 or [(row[0], row[1]) for row in actual] != fn_names:
            raise ValueError("golden function order or count differs from resolver")
    except ValueError as error:
        print(f"FAIL golden: {error}")
        failed += 1

    if baseline is not None:
        comparisons = [("types", typed), ("resolve", resolved)]
        for mode, expected in comparisons:
            if run(baseline, mode, lib, modules) != expected:
                print(f"FAIL golden {mode}: differs from staged baseline")
                failed += 1
        for name, source in modules:
            if name != "nat" and run(driver, "lift", source, []) != run(baseline, "lift", source, []):
                print(f"FAIL golden lift: {name} differs from staged baseline")
                failed += 1

    total = len(NEGATIVES) + len(EXISTING_NEGATIVES) + len(POSITIVES)
    print(f"cases={total} golden-functions=22 failures={failed}")
    print("RUST-INFER-OK" if failed == 0 else "RUST-INFER-FAIL")
    return 0 if failed == 0 else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, help="reuse a compiled JS rust_import driver")
    parser.add_argument("--baseline-driver", type=Path, help="also compare golden output with the original staged driver")
    parser.add_argument("--golden-dir", type=Path, default=ROOT / "test/rust/emit/crate/src")
    parser.add_argument("--bend", type=Path, default=Path.home() / ".bend/bin/bend")
    args = parser.parse_args()
    if args.driver is not None:
        return check(args.driver.resolve(), args.golden_dir, args.baseline_driver)
    with tempfile.TemporaryDirectory(prefix="rust-infer-") as work:
        driver = Path(work) / "rust_import.js"
        subprocess.run([str(args.bend), str(ROOT / "bend2/tests/rust_import.bend"), "-o", str(driver)], check=True, timeout=180)
        return check(driver, args.golden_dir, args.baseline_driver)


if __name__ == "__main__":
    raise SystemExit(main())
