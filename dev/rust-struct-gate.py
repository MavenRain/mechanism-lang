#!/usr/bin/env python3
"""Check struct walkers and compile, format, and execute direct RIR emission."""

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BEND = Path.home() / ".bend/bin/bend"
WALKER_LINES = [
    "PASS struct field dependencies",
    "PASS struct pattern dependencies",
    "PASS struct item collision names",
    "PASS struct class collision names",
    "PASS struct is not Copy",
    "PASS unused struct bindings",
    "PASS struct match is preserved",
    "PASS named struct description",
    "PASS tuple struct description",
]
IMPORT_LINES = [
    "PASS resolve named struct refusal",
    "PASS resolve tuple struct refusal",
    "PASS infer named struct refusal",
    "PASS infer tuple struct refusal",
    "PASS resolve named struct pattern refusal",
    "PASS resolve tuple struct pattern refusal",
    "PASS infer named struct pattern refusal",
    "PASS infer tuple struct pattern refusal",
    "PASS named struct pattern has no enum coverage",
    "PASS tuple struct pattern has no enum coverage",
    "PASS named struct binding walker",
    "PASS tuple struct binding walker",
]

RUST_MAIN = """extern crate struct_regression as generated;

fn main() {
    let record = generated::named(true, String::from("named text"), false);
    let enabled: bool = record.enabled();
    assert!(enabled);
    let payload: &String = record.payload();
    assert_eq!(payload, "named text");
    assert!(std::ptr::eq(record.payload(), record.payload()));
    assert_eq!(generated::named_renamed(&record), "named text");
    assert_eq!(generated::named_shorthand(&record), "named text");
    assert_eq!(generated::named_owned(record), "named text");
    let record = generated::named(false, String::from("false text"), true);
    assert!(!record.enabled());
    assert_eq!(generated::named_owned(record), "false text");
    let record = generated::tuple(false, String::from("tuple text"), true);
    assert_eq!(generated::tuple_borrowed(&record), "tuple text");
    assert_eq!(generated::tuple_owned(record), "tuple text");
    assert!(generated::empty_named_match(generated::empty_named()));
    assert!(generated::empty_tuple_match(generated::empty_tuple()));
    println!("PASS emitted struct execution");
}
"""


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def driver(source: str, supplied: Path | None, directory: Path) -> Path:
    if supplied is not None:
        return supplied.resolve()
    output = directory / f"{Path(source).stem}.js"
    run([str(BEND), str(ROOT / source), "-o", str(output)])
    return output


def check_driver(source: str, supplied: Path | None, directory: Path,
                 expected: list[str]) -> None:
    executable = driver(source, supplied, directory)
    result = run(["node", "--stack-size=65536", str(executable)])
    if result.stdout.splitlines() != expected:
        raise SystemExit(f"unexpected {source} output:\n{result.stdout}")
    print(result.stdout, end="")


def check_walkers(supplied: Path | None, directory: Path) -> None:
    check_driver("bend2/tests/rust_struct_walkers.bend", supplied,
                 directory, WALKER_LINES)


def check_importer(supplied: Path | None, directory: Path) -> None:
    check_driver("bend2/tests/rust_struct_import.bend", supplied,
                 directory, IMPORT_LINES)


def check_all(args: argparse.Namespace, directory: Path) -> None:
    check_walkers(args.walkers_driver, directory)
    check_importer(args.import_driver, directory)
    check_emitter(args.emit_driver, directory)


def check_emitter(supplied: Path | None, directory: Path) -> None:
    executable = driver("bend2/tests/rust_struct_emit.bend", supplied, directory)
    emitted = run(["node", "--stack-size=65536", str(executable)]).stdout
    expected = [
        "pub struct Named {",
        "enabled: bool,",
        "payload: String,",
        "Named {",
        "pub struct Tuple(bool, String, bool);",
        "Tuple(first, second, third)",
        "payload: text,",
        "Named {",
        "enabled: _,",
        "spare: _",
        "            payload,\n",
        "Tuple(_, text, _)",
        "pub fn enabled(&self) -> bool",
        "pub fn payload(&self) -> &String",
        "&self.payload",
    ]
    missing = [fragment for fragment in expected if fragment not in emitted]
    if missing:
        raise SystemExit(f"missing struct emission: {missing}\n{emitted}")
    if "pub enabled:" in emitted or "pub payload:" in emitted:
        raise SystemExit("struct fields must remain private")
    source = directory / "structs.rs"
    source.write_text(emitted)
    run(["rustfmt", "--edition", "2021", "--check", str(source)])
    print("PASS emitted struct rustfmt")
    library = directory / "libstruct_regression.rlib"
    run([
        "rustc", "--edition", "2021", "--crate-type", "lib",
        "--crate-name", "struct_regression", str(source), "-o", str(library),
    ])
    print("PASS emitted struct rustc")
    main = directory / "main.rs"
    main.write_text(RUST_MAIN)
    binary = directory / "struct-execution"
    run([
        "rustc", "--edition", "2021", str(main),
        "--extern", f"struct_regression={library}", "-o", str(binary),
    ])
    result = run([str(binary)])
    if result.stdout != "PASS emitted struct execution\n":
        raise SystemExit(f"unexpected Rust execution output:\n{result.stdout}")
    print(result.stdout, end="")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emit-driver", type=Path)
    parser.add_argument("--walkers-driver", type=Path)
    parser.add_argument("--import-driver", type=Path)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    if args.work_dir is not None:
        directory = args.work_dir.resolve()
        directory.mkdir(parents=True, exist_ok=True)
        check_all(args, directory)
        return
    with tempfile.TemporaryDirectory(prefix="rust-struct-") as temporary:
        directory = Path(temporary)
        check_all(args, directory)


if __name__ == "__main__":
    main()
