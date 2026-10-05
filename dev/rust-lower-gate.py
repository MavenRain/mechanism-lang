#!/usr/bin/env python3
"""C4 lowering regressions, semantic checks and the golden Rust round trip."""

import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("infer_gate", ROOT / "dev/rust-infer-gate.py")
INFER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INFER)

CONTEXTS = [
    ("if argument", "pub fn id(x: bool) -> bool { x } pub fn good(b: bool) -> bool { id(if b { true } else { false }) }"),
    ("if condition", "pub fn good(b: bool) -> bool { if if b { false } else { true } { true } else { false } }"),
    ("closure if", "pub fn apply(f: &impl Fn(bool) -> bool, x: bool) -> bool { f(x) } pub fn good(x: bool) -> bool { apply(&|y: bool| if y { false } else { true }, x) }"),
    ("constructor field if", INFER.ONE + "pub fn good(b: bool) -> Foo { Foo::Bar(if b { true } else { false }) }"),
    ("match argument", INFER.TWO + "pub fn id(x: bool) -> bool { x } pub fn good(x: Foo) -> bool { id(match x { Foo::Bar => true, Foo::Baz => false, }) }"),
    ("closure match", INFER.TWO + "pub fn apply(f: &impl Fn(Foo) -> bool, x: Foo) -> bool { f(x) } pub fn good(x: Foo) -> bool { apply(&|y: Foo| match y { Foo::Bar => true, Foo::Baz => false, }, x) }"),
    ("empty closure", INFER.COPY + "pub enum Void {} pub fn apply(f: &impl Fn(Void) -> bool, v: Void) -> bool { f(v) } pub fn good(v: Void) -> bool { apply(&|x: Void| match x {}, v) }"),
    ("empty pattern field", INFER.COPY + "pub enum Void {} " + INFER.COPY + "pub enum Wrap { Wrap(Void) } pub fn good(w: Wrap) -> bool { match w { Wrap::Wrap(v) => match v {}, } }"),
    ("empty call scrutinee", INFER.COPY + "pub enum Void {} pub fn id(v: Void) -> Void { v } pub fn good(v: Void) -> bool { match id(v) {} }"),
    ("local underscore callback", "pub fn good(_f: &impl Fn(bool) -> bool, b: bool) -> bool { _f(b) }"),
    ("local mechanism keyword", "pub fn good(fun: bool) -> bool { fun }"),
    ("local Rust keyword escape", "pub fn good(match_: bool) -> bool { match_ }"),
]

# Each closed result is evaluated by the kernel through the existing oracle.
SEMANTICS = [
    ("parameter capture", "pub fn first(x: bool, _x: bool) -> bool { x } pub fn result() -> bool { first(true, false) }", "true"),
    ("second parameter", "pub fn second(x: bool, _x: bool) -> bool { _x } pub fn result() -> bool { second(true, false) }", "false"),
    ("pattern capture", INFER.ONE + "pub fn first(x: bool, f: Foo) -> bool { match f { Foo::Bar(_x) => x, } } pub fn result() -> bool { first(true, Foo::Bar(false)) }", "true"),
    ("closure capture", "pub fn apply(f: &impl Fn(bool) -> bool) -> bool { f(false) } pub fn first(x: bool) -> bool { apply(&|_x: bool| x) } pub fn result() -> bool { first(true) }", "true"),
    ("local shadows nat_small", "pub fn apply(nat_small: &impl Fn(bool) -> bool) -> bool { nat_small(false) } pub fn result() -> bool { apply(&|x: bool| true) }", "true"),
    ("local shadows nat_add", "pub fn apply(nat_add: &impl Fn(bool) -> bool) -> bool { nat_add(false) } pub fn result() -> bool { apply(&|x: bool| true) }", "true"),
    ("literal constructor capture", "pub fn first(mech_true: bool) -> bool { true } pub fn result() -> bool { first(false) }", "true"),
    ("global call capture", "pub fn target(x: bool) -> bool { true } pub fn first(_target: &impl Fn(bool) -> bool) -> bool { target(false) } pub fn result() -> bool { first(&|x: bool| false) }", "true"),
    ("intentional shadow", INFER.ONE + "pub fn first(x: bool, f: Foo) -> bool { match f { Foo::Bar(x) => x, } } pub fn result() -> bool { first(true, Foo::Bar(false)) }", "false"),
]


def invoke(driver, mode, lib, modules):
    return INFER.run(driver, mode, lib, modules)


def oracle(driver, mode, paths, cwd=None):
    result = subprocess.run(
        ["node", "--stack-size=16384", str(driver), mode, *map(str, paths)],
        cwd=cwd, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=60,
    )
    if result.returncode or result.stderr:
        raise RuntimeError(f"oracle exit={result.returncode}: {result.stderr[:1000]}")
    return result.stdout


def split_files(text, directory):
    parts = re.split(r"^-- file ([^\n]+)\n", text, flags=re.MULTILINE)
    if parts[0] or len(parts) < 3:
        raise ValueError(f"unexpected lower output: {text[:500]}")
    paths = []
    for name, source in zip(parts[1::2], parts[2::2]):
        if Path(name).name != name or name in {p.name for p in paths}:
            raise ValueError(f"invalid output filename: {name}")
        path = directory / name
        path.write_text(source)
        paths.append(path)
    return paths


def check(driver, emitter, work):
    failed = 0
    cases = [(name, source) for name, source, _ in INFER.POSITIVES] + CONTEXTS
    for name, source in cases:
        modules = [("demo", INFER.HEADER + source + "\n")]
        output = invoke(driver, "check", INFER.LIB, modules)
        if re.fullmatch(r"MECH-CHECK-OK \d+ rows axioms=0\n", output):
            print(f"PASS {name}")
        else:
            print(f"FAIL {name}: {output.strip()}")
            failed += 1

    for name, source, expected in SEMANTICS:
        modules = [("demo", INFER.HEADER + source + "\n")]
        try:
            paths = split_files(invoke(driver, "lower", INFER.LIB, modules), work)
            output = oracle(emitter, "values", paths)
        except (RuntimeError, ValueError) as error:
            print(f"FAIL {name}: {error}")
            failed += 1
            continue
        if f"result = {expected}" in output.splitlines():
            print(f"PASS {name}: result={expected}")
        else:
            print(f"FAIL {name}: {output[-600:].strip()}")
            failed += 1

    golden = ROOT / "test/rust/emit/crate"
    lib = (golden / "src/lib.rs").read_text()
    modules = [(name, (golden / f"src/{name}.rs").read_text()) for name in ("init", "nat", "second_price")]
    output = invoke(driver, "check", lib, modules)
    if output != "MECH-CHECK-OK 22 rows axioms=0\n":
        print(f"FAIL golden check: {output.strip()}")
        failed += 1
    paths = split_files(invoke(driver, "lower", lib, modules), work)
    oracle(emitter, "crate", [*paths, work / "crate"], cwd=work)
    for name in ("init", "second_price"):
        path = Path(f"src/{name}.rs")
        if (work / "crate" / path).read_bytes() == (golden / path).read_bytes():
            print(f"PASS golden round trip {name}")
        else:
            print(f"FAIL golden round trip {name}")
            failed += 1

    recursive = [(name, source.replace("auction_compare(tie_wins, previous, other)", "auction_compare(tie_wins, bid, price)")) for name, source in modules]
    if recursive == modules:
        raise ValueError("totality control did not mutate the golden crate")
    output = invoke(driver, "check", lib, recursive)
    if output.startswith("MECH-CHECK-FAIL ") and "structural termination guard" in output:
        print("PASS non structural recursion rejected")
    else:
        print(f"FAIL totality control: {output.strip()}")
        failed += 1
    print(f"cases={len(cases) + len(SEMANTICS)} failures={failed}")
    print("RUST-LOWER-OK" if failed == 0 else "RUST-LOWER-FAIL")
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, help="reuse a compiled JS rust_import driver")
    parser.add_argument("--emitter", type=Path, help="reuse a compiled JS rust_emit driver")
    parser.add_argument("--bend", type=Path, default=Path.home() / ".bend/bin/bend")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rust-lower-") as temp:
        work = Path(temp)
        drivers = []
        for existing, name in ((args.driver, "rust_import"), (args.emitter, "rust_emit")):
            driver = existing.resolve() if existing else work / f"{name}.js"
            if existing is None:
                subprocess.run([str(args.bend), str(ROOT / f"bend2/tests/{name}.bend"), "-o", str(driver)], check=True, timeout=180)
            drivers.append(driver)
        return check(*drivers, work)


if __name__ == "__main__":
    raise SystemExit(main())
