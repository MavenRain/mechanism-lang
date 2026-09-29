#!/usr/bin/env python3
"""Re-record live OCaml observations and compare them with the frozen Bend 2 cases."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SUITES = ("json", "export", "translate", "pipeline", "parser", "surface", "erase", "cli", "core-cli")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_helper(name: str):
    path = (ROOT / "bend2/tests/cli_core_check.py" if name == "core-cli"
            else ROOT / "dev" / f"bend2-{name}-check.py")
    spec = importlib.util.spec_from_file_location(f"reference_{name}", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def checked(command: list[str], cwd: Path, timeout: int = 300) -> str:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command!r}\n"
                           f"{result.stdout}{result.stderr}")
    return result.stdout.strip()


def observe(command: list[str], cwd: Path, timeout: int) -> dict:
    try:
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                                timeout=timeout)
        return dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
    except subprocess.TimeoutExpired:
        return dict(code="timeout", stdout="", stderr=f"exceeded {timeout} seconds")


def write_report(path: Path, report: dict) -> None:
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(report, indent=2) + "\n")
    temporary.replace(path)


def verify(reference: Path, output: Path, suites: list[str]) -> dict:
    build = output / "ocaml-build"
    adapters = output / "adapters"
    adapters.mkdir()
    report = dict(version=1, status="running", reference=str(reference), suites=[],
                  scope=suites, normalizations=["CLI temporary root",
                  "CLI mkdir process-specific staging path"], inputs={}, tools={})
    report_path = output / "report.json"
    write_report(report_path, report)
    pins = report["inputs"]

    def pin(path: Path) -> None:
        digest = sha(path)
        if pins.setdefault(str(path), digest) != digest:
            raise ValueError(f"input changed during reference replay: {path}")

    native = any(name in suites for name in ("surface", "erase"))
    hosts = ("node", "wasmtime", "zsh", "rg") if "core-cli" in suites else ()
    for name in ("dune", "ocamlfind", "ocamlc", "ocamlrun", *(["ocamlopt"] if native else []), *hosts):
        tool = shutil.which(name)
        if tool is None:
            raise RuntimeError(f"missing reference tool: {name}")
        report["tools"][name] = dict(path=tool, sha256=sha(Path(tool).resolve()))
        pin(Path(tool).resolve())
    top = checked(["git", "rev-parse", "--show-toplevel"], reference)
    if Path(top).resolve() != reference:
        raise RuntimeError(f"reference is not the root of a git work tree: {reference}")
    report["reference_head"] = checked(["git", "rev-parse", "HEAD"], reference)
    # Pin local sources, the root dune files, and the Veil modules copied by dune.
    for directory in ("lib", "surface", "import", "bin", "wasm", "prelude",
                      "vendor/veil/lib", "vendor/veil/surface", "vendor/veil/wasm",
                      "vendor/veil/bin"):
        for path in sorted((reference / directory).glob("*")):
            if path.is_file() and (path.suffix in (".ml", ".mli") or path.name == "dune"):
                pin(path)
    pin(reference / "dune-project")
    pin(reference / "dune")
    workspace = reference / "dune-workspace"
    if workspace.is_file():
        pin(workspace)
    pin(Path(__file__).resolve())
    for name in ("cli", "surface"):
        pin(ROOT / "dev" / f"bend2-{name}-check.py")
    if "core-cli" in suites:
        pin(ROOT / "bend2/tests/cli_core_check.py")
        pin(ROOT / "dev/bend2-process.py")
        # The OCaml host resolves dev/ four parents above its executable.
        runtime = output / "dev"
        runtime.mkdir()
        for name in ("run-node.mjs", "run-wasmtime.sh"):
            pin(reference / "dev" / name)
            shutil.copyfile(reference / "dev" / name, runtime / name)
            pin(runtime / name)
    libraries = [("lib", "mechanism_kernel"), ("surface", "mechanism_surface"),
                 ("import", "mechanism_import")]
    targets = [f"{directory}/{name}.cma" for directory, name in libraries]
    if native:
        targets += [f"{directory}/{name}.cmxa" for directory, name in libraries]
    if any(name in suites for name in ("cli", "core-cli")):
        targets += ["bin/mech.exe", "bin/mech_cert.exe"]
    # dunecho accepts only a mode, so invoke dune for these isolated targets.
    checked(["dune", "build", "--root", str(reference), "--build-dir", str(build),
             *targets], reference, 3600)
    includes, archives, native_archives = [], [], []
    for directory, name in libraries:
        base = build / "default" / directory
        includes += ["-I", str(base / f".{name}.objs/byte")]
        archives.append(str(base / f"{name}.cma"))
        pin(base / f"{name}.cma")
        if native:
            native_archives.append(str(base / f"{name}.cmxa"))
            pin(base / f"{name}.cmxa")
            pin(base / f"{name}.a")
        for path in sorted((base / f".{name}.objs/byte").glob("*.cmi")):
            pin(path)
    drivers = {}
    for suite in suites:
        if suite in ("cli", "core-cli"):
            continue
        name = suite
        if name in drivers:
            continue
        source = ROOT / "dev/bend2/reference" / f"{name}.ml"
        pin(source)
        copied = adapters / source.name
        shutil.copyfile(source, copied)
        driver = adapters / f"{name}.exe"
        compiler = "ocamlopt" if name in ("surface", "erase") else "ocamlc"
        selected_archives = native_archives if compiler == "ocamlopt" else archives
        checked(["ocamlfind", compiler, "-package", "zarith,unix", "-linkpkg",
                 *includes, *selected_archives, str(copied), "-o", str(driver)], adapters)
        drivers[name] = driver
        pin(driver)
    cli = load_helper("cli")
    surface = load_helper("surface")
    core_cli = load_helper("core-cli") if "core-cli" in suites else None
    for suite in suites:
        corpus = (ROOT / "bend2/tests/cli_core_expected.json" if suite == "core-cli"
                  else ROOT / "dev/bend2" / f"{suite}-cases.json")
        pin(corpus)
        evidence = (dict(cases=core_cli.load_cases(corpus)) if suite == "core-cli"
                    else json.loads(corpus.read_text()))
        if not isinstance(evidence.get("cases"), list) or not evidence["cases"]:
            raise ValueError(f"empty or missing case inventory: {suite}")
        if suite == "surface":
            surface.validate(evidence)
            baseline = ROOT / evidence["baseline"]["path"]
            pin(baseline)
            if sha(baseline) != evidence["baseline"]["sha256"]:
                raise ValueError("surface baseline hash mismatch")
            report["unresolved_surface_attempts"] = evidence["baseline"]["unresolved_attempts"]
        observations = []
        started = time.monotonic()
        for index, case in enumerate(evidence["cases"]):
            expected = case.get("expected")
            if expected is None:
                expected = {key: case[key] for key in ("code", "stdout", "stderr")}
            if type(expected.get("code")) is not int or not 0 <= expected["code"] <= 255:
                raise ValueError(f"invalid historical exit status: {suite}/{index}")
            if any(not isinstance(expected.get(key), str) for key in ("stdout", "stderr")):
                raise ValueError(f"invalid historical output: {suite}/{index}")
            if suite in ("cli", "core-cli"):
                filename = ("mech.exe" if suite == "core-cli"
                            else {"mech": "mech.exe", "cert": "mech_cert.exe"}[case["mode"]])
                driver = build / "default/bin" / filename
                pin(driver)
                try:
                    actual = (core_cli.observe([str(driver)], case, cwd=reference) if suite == "core-cli"
                              else cli.observe([str(driver)], case))
                except (OSError, subprocess.TimeoutExpired) as error:
                    actual = dict(error=str(error))
                passed = (actual == expected if suite == "core-cli" else
                          cli.comparable(case["name"], actual) == cli.comparable(case["name"], expected))
            else:
                source = case.get("source")
                if source is None:
                    path = ROOT / case["path"]
                    pin(path)
                    if sha(path) != case["sha256"]:
                        raise ValueError(f"stale corpus input: {path}")
                    source = path.read_text()
                if "source_sha256" in case and hashlib.sha256(source.encode()).hexdigest() != case["source_sha256"]:
                    raise ValueError(f"changed source identity: {suite}/{index}")
                name = suite
                arguments = [case["mode"], source, case["targets"]] if suite == "pipeline" else [source]
                if case.get("empty"):
                    arguments.insert(0, "--empty")
                timeout = 900 if suite == "surface" else 120 if suite in ("erase", "pipeline") else 30
                actual = observe([str(drivers[name]), *arguments], reference, timeout)
                passed = actual == expected
            observations.append(dict(index=index, actual=actual, passed=passed,
                                     **({} if passed else dict(expected=expected))))
            if (index + 1) % 100 == 0:
                print(f"REFERENCE {suite}: observed {index + 1}/{len(evidence['cases'])}", flush=True)
        failures = sum(not row["passed"] for row in observations)
        detail = output / f"{suite}-observations.json"
        write_report(detail, dict(corpus_sha256=sha(corpus), observations=observations))
        report["suites"].append(dict(name=suite, cases=len(observations), failed=failures,
                                    seconds=round(time.monotonic() - started, 3),
                                    observations=detail.name, sha256=sha(detail)))
        write_report(report_path, report)
        print(f"REFERENCE {suite}: {len(observations)} cases, {failures} differences", flush=True)
    drift = [path for path, digest in pins.items() if not Path(path).is_file() or sha(Path(path)) != digest]
    report["drift"] = drift
    report["failed"] = sum(suite["failed"] for suite in report["suites"])
    report["status"] = "passed" if not drift and not report["failed"] else "failed"
    write_report(report_path, report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=ROOT,
                        help="live OCaml checkout (default: this repository)")
    parser.add_argument("--output", type=Path,
                        help="fresh evidence directory (default: unique directory under _bend2/reference)")
    parser.add_argument("--suite", choices=SUITES, action="append")
    args = parser.parse_args()
    reference = args.reference.resolve()
    if not (reference / "dune-project").is_file():
        parser.error("reference must contain the live OCaml dune project")
    local_output = (ROOT / "_bend2/reference").resolve()
    if args.output:
        output = args.output.resolve()
        if (output == reference or reference in output.parents) and local_output not in output.parents:
            parser.error("output inside the reference checkout must be under _bend2/reference")
        if output.exists():
            parser.error("output must be a fresh evidence directory")
        output.mkdir(parents=True, exist_ok=False)
    else:
        local_output.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="recording-", dir=local_output))
    try:
        report = verify(reference, output, list(dict.fromkeys(args.suite or SUITES)))
    except (OSError, ValueError, LookupError, TypeError, AttributeError, RuntimeError,
            subprocess.TimeoutExpired) as error:
        message = f"{type(error).__name__}: {error}"
        write_report(output / "error.json", dict(status="error", message=message))
        report_path = output / "report.json"
        report = json.loads(report_path.read_text()) if report_path.exists() else {}
        report.update(status="error", error=message)
        write_report(report_path, report)
        print(f"REFERENCE ERROR: {message}", file=sys.stderr)
        return 2
    print(f"REFERENCE {report['status'].upper()}: {output / 'report.json'}")
    return int(report["status"] != "passed")


if __name__ == "__main__":
    sys.exit(main())
