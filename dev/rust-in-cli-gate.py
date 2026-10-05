#!/usr/bin/env python3
"""Exercise rust-in publication and refusal through its driver and CLI."""

import argparse
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
GOOD = "pub fn good(x: bool) -> bool { x }\n"
CARRIER = "-- proof carrier: λ\r\n".encode()
COLLISION = "//! Emitted by `mech rust-out` from `mech-carrier.mech`.\n" + GOOD


def snapshot(directory):
    return {
        str(path.relative_to(directory)): path.read_bytes()
        for path in directory.rglob("*")
        if path.is_file()
    }


def invoke(program, arguments):
    return subprocess.run(
        ["node", str(program[0]), program[1], *map(str, arguments)],
        capture_output=True, text=True, timeout=30,
    )


def check(program, work):
    cases = [
        ("plain", "foo", GOOD, None, 0),
        ("carrier", "foo", GOOD, CARRIER, 0),
        ("carrier name without sidecar", "mech_carrier", COLLISION, None, 0),
        ("distinct underscore name", "mech_carrier", GOOD, CARRIER, 0),
        ("carrier collision", "mech_carrier", COLLISION, CARRIER, 65),
        ("empty carrier collision", "mech_carrier", COLLISION, b"", 65),
        ("parse refusal", "foo", "pub fn bad() { let x = true; }", None, 65),
        ("type refusal", "foo", "pub fn bad() -> bool { () }", None, 65),
        ("missing module", "foo", None, None, 64),
        ("missing lib", "foo", GOOD, None, 64),
        ("carrier directory", "foo", GOOD, None, 64),
        ("existing directory", "foo", GOOD, None, 1),
        ("existing file", "foo", GOOD, None, 1),
        ("write failure", "foo", GOOD, None, 1),
    ]
    failures = []
    for index, (name, module, source, carrier, code) in enumerate(cases):
        case = work / str(index)
        crate = case / "crate"
        src = crate / "src"
        src.mkdir(parents=True)
        if name != "missing lib":
            (src / "lib.rs").write_text(f"pub mod {module};\n")
        if source is not None:
            (src / f"{module}.rs").write_text(source)
        if carrier is not None:
            (crate / "mech-carrier.mech").write_bytes(carrier)
        if name == "carrier directory":
            (crate / "mech-carrier.mech").mkdir()
        out = case / "out"
        if name == "existing directory":
            out.mkdir()
            (out / "sentinel").write_bytes(b"preserve")
        if name == "existing file":
            out.write_bytes(b"preserve")
        if name == "write failure":
            blocker = case / "blocker"
            blocker.write_bytes(b"preserve")
            out = blocker / "out"
        before = snapshot(case)
        result = invoke(program, [crate, out])
        try:
            assert result.returncode == code, (result.returncode, code, result.stderr)
            if code:
                assert snapshot(case) == before, "refusal changed files"
                if not name.startswith("existing"):
                    assert not out.exists(), "refusal published a directory"
                if "collision" in name:
                    assert "collides with the proof carrier" in result.stderr
            else:
                filename = "mech-carrier.mech" if source == COLLISION else f"{module}.mech"
                expected = {"MANIFEST", filename}
                if carrier is not None:
                    expected.add("mech-carrier.mech")
                    assert (out / "mech-carrier.mech").read_bytes() == carrier
                assert {p.name for p in out.iterdir()} == expected
                assert (out / "MANIFEST").read_text() == filename + "\n"
                assert "def good" in (out / filename).read_text()
                assert snapshot(crate) == {
                    key.removeprefix("crate/"): value
                    for key, value in before.items() if key.startswith("crate/")
                }
            print(f"PASS {program[1]}: {name}")
        except AssertionError as error:
            failures.append(name)
            print(f"FAIL {program[1]}: {name}: {error}")
    for arguments in ([], ["one"], ["one", "two", "three"]):
        result = invoke(program, arguments)
        if result.returncode != 64:
            failures.append(f"usage {arguments}")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path)
    parser.add_argument("--cli", type=Path)
    parser.add_argument("--bend", type=Path, default=Path.home() / ".bend/bin/bend")
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory(prefix="rust-in-cli-") as directory:
        work = Path(directory)
        for existing, source, verb in (
            (args.driver, "tests/rust_import.bend", "run"),
            (args.cli, "mech.bend", "rust-in"),
        ):
            program = existing.resolve() if existing else work / f"{verb}.js"
            if existing is None:
                subprocess.run(
                    [str(args.bend), str(ROOT / "bend2" / source), "-o", str(program)],
                    check=True, timeout=180,
                )
            failures.extend(check((program, verb), work / verb))
    print("RUST-IN-CLI-FAIL" if failures else "RUST-IN-CLI-OK")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
