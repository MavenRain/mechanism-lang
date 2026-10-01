#!/usr/bin/env python3
"""Run the Bend kernel, frontend, importer, CLI, and executable Wasm checks."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "_bend2/bin"
TEST = ROOT / "_bend2/test"
LOG = ROOT / "build/bend2-tests"


def stop_group(process: subprocess.Popen) -> None:
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        process.wait()
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def terminate(signum, _frame):
    raise SystemExit(128 + signum)


def main() -> int:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--suite", choices=["all", "import", "extras"], default="all")
    parser.add_argument("--backend", choices=["mixed", "javascript", "native"], default="mixed",
                        help="mixed builds JavaScript test shards and the native production CLI")
    parser.add_argument("--no-build", action="store_true",
                        help="use the launchers already built by the caller")
    args = parser.parse_args()
    signal.signal(signal.SIGTERM, terminate)
    LOG.mkdir(parents=True, exist_ok=True)
    results = []

    def run(name, command, *, timeout=600, marker=None, stdout=None):
        output = stdout or LOG / (name + ".stdout")
        errors = LOG / (name + ".stderr")
        started = time.monotonic()
        record = dict(name=name, command=[str(x) for x in command],
                      stdout=str(output.relative_to(ROOT)), stderr=str(errors.relative_to(ROOT)))
        try:
            with output.open("w") as out, errors.open("w") as err:
                process = subprocess.Popen(command, cwd=ROOT, stdout=out, stderr=err,
                                           start_new_session=True)
                try:
                    process.wait(timeout=timeout)
                except BaseException:
                    stop_group(process)
                    raise
            record["code"] = process.returncode
            record["passed"] = process.returncode == 0 and (marker is None or marker in output.read_text())
        except subprocess.TimeoutExpired:
            record.update(passed=False, timeout=timeout)
        except OSError as error:
            record.update(passed=False, error=str(error))
        record["seconds"] = round(time.monotonic() - started, 3)
        results.append(record)
        (LOG / "progress.json").write_text(json.dumps(results, indent=2) + "\n")
        print(("PASS " if record["passed"] else "FAIL ") + name, flush=True)
        return record["passed"]

    if not args.no_build:
        builds = ([('build-tests', ['--target', 'tests', '--backend', 'javascript']),
                   ('build-production', ['--target', 'production', '--backend', 'native'])]
                  if args.backend == 'mixed' else [('build', ['--backend', args.backend])])
        build_started = time.monotonic()
        for name, options in builds:
            remaining = 7200 - (time.monotonic() - build_started)
            if remaining <= 0 or not run(name, [sys.executable, '-P', 'dev/bend2-build.py', *options],
                                         timeout=remaining):
                return 1
    manifest_path = ROOT / "dev/bend2/test-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if args.suite in {"all", "extras"}:
        run("build-selectors", [sys.executable, "-I", "dev/bend2-build-selector-test.py"],
            marker="BEND2 BUILD SELECTORS PASS")
        run("template-arity-boundary", [sys.executable, "-I", "dev/bend2-template-arity-check.py"],
            marker="TEMPLATE-ARITY-OK positive=1 signed-refusal=1", timeout=120)
        run("relational-process", [sys.executable, "-I", "bend2/tests/relational_process.py"],
            marker="RELATIONAL-PROCESS-OK cases=6")
        run("relational-process-nested", [sys.executable, "-I", "bend2/tests/relational_process_nested.py"],
            marker="RELATIONAL-PROCESS-NESTED-OK cases=3")
        run("relational-build-process", [sys.executable, "-I", "bend2/tests/relational_build_process.py"],
            marker="RELATIONAL-BUILD-PROCESS-OK cases=1")
        run("levels-boundaries", [sys.executable, "-I", "bend2/tests/levels_protocol_boundaries.py"],
            marker="LEVELS-BOUNDARIES-OK assertions=4")
        run("surface-boundaries", [sys.executable, "-I", "bend2/tests/surface_protocol_boundaries.py"],
            marker="SURFACE-BOUNDARIES-OK assertions=2")
        for unit in manifest["units"]:
            run(unit["mode"], [TEST / "unit.exe", unit["mode"]])
        for fixture in manifest["fixtures"]:
            mode = fixture["mode"]
            output = LOG / ("fixture-" + mode + ".jsonl")
            if run("fixture-" + mode, [TEST / "fixture.exe", mode], stdout=output):
                run("wasm-" + mode, ["node", "bend2/wasm/check_fixtures.mjs", mode, output])

    for name in ("json", "export", "translate", "pipeline"):
        run("import-" + name, [sys.executable, "-P", "dev/bend2-" + name + "-check.py",
                              "--native", TEST / ("import_" + name + ".exe")], timeout=1800)
    run("filesystem", [sys.executable, "-P", "dev/bend2-io-check.py", "--driver", TEST / "io_driver.exe"])
    run("import-certificate-cli", [sys.executable, "-P", "dev/bend2-cli-check.py",
                                   "--mech", BIN / "mech.exe", "--cert", BIN / "mech_cert.exe"])
    run("mapping-protocol", [TEST / "mapping.exe"], marker="MAPPING-OK")
    if args.suite in {"all", "extras"}:
        run("parser-goldens", [sys.executable, "-P", "dev/bend2-parser-check.py",
                               "--native", TEST / "surface_driver.exe"])
        run("surface-goldens", [sys.executable, "-P", "dev/bend2-surface-check.py",
                                "--driver", TEST / "surface_check_driver.exe"], timeout=5400)
        run("erasure-goldens", [sys.executable, "-P", "dev/bend2-erase-check.py",
                                "--driver", TEST / "kernel_erase_driver.exe"])
        run("core-cli", [sys.executable, "-P", "bend2/tests/cli_core_check.py",
                          "--driver", BIN / "kanon.exe"])
        run("runtime-slicing", [TEST / "prelude_runtime.exe", "--slice-self-test"], timeout=600)
    if args.suite == "all":
        run("prelude-protocol", [TEST / "prelude.exe", ROOT], marker="PRELUDE-OK", timeout=1800)
        run("wasm-protocol", [TEST / "wasm.exe", ROOT / "test/veil/test", LOG / "wasm-suite"],
            marker="WASM-OK", timeout=1800)
    for driver in manifest["drivers"]:
        if args.suite not in driver.get("suites", ["all"]):
            continue
        preparation = driver.get("prepare")
        if preparation is not None:
            run(driver["mode"] + "-prepare",
                [sys.executable, "-I", ROOT / preparation["script"], *preparation.get("args", [])],
                timeout=preparation.get("timeout", 30))
        for index, check in enumerate(driver.get("checks", [])):
            arguments = [str(value).replace("{root}", str(ROOT)).replace("{log}", str(LOG))
                         for value in check.get("args", [])]
            name = driver["mode"] + "-" + str(index + 1)
            run(name, [TEST / ("driver_" + driver["mode"] + ".exe"), *arguments],
                marker=check.get("marker"), timeout=check.get("timeout", 600))

    failures = [result["name"] for result in results if not result["passed"]]
    source_manifest = ROOT / "_bend2/bend2-sources.sha256"
    acceptance_profile = args.no_build and os.environ.get("BEND_TEST_BACKEND") == "native"
    profile = ("native-acceptance" if acceptance_profile else "existing" if args.no_build else
               "native-cli-js-tests" if args.backend == "mixed" else "uniform")
    report = dict(suite=args.suite, backend="mixed" if acceptance_profile else args.backend,
                  profile=profile,
                  build_backend=None if args.no_build else args.backend, checks=len(results), failures=failures,
                  manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(), results=results)
    if source_manifest.exists():
        report["source_manifest_sha256"] = hashlib.sha256(source_manifest.read_bytes()).hexdigest()
    (LOG / (args.suite + "-report.json")).write_text(json.dumps(report, indent=2) + "\n")
    marker = {"all": "BEND2 TESTS", "import": "BEND2 IMPORT TESTS",
              "extras": "BEND2 EXTRAS"}[args.suite]
    print(marker + (" FAIL" if failures else " PASS"), flush=True)
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
