#!/usr/bin/env python3
"""Exercise reference replay refusal paths against a real OCaml build."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("reference_check", ROOT / "dev/bend2-reference-check.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="mechanism-reference-controls-") as tmp:
        root = Path(tmp)
        corpus = root / "dev/bend2/json-cases.json"
        corpus.parent.mkdir(parents=True)
        shutil.copytree(ROOT / "dev/bend2/reference", root / "dev/bend2/reference")
        for name in ("cli", "surface"):
            shutil.copyfile(ROOT / "dev" / f"bend2-{name}-check.py",
                            root / "dev" / f"bend2-{name}-check.py")
        module.ROOT = root
        cases = [dict(source="null", code=0, stdout="null\n", stderr=""),
                 dict(source="true", code=0, stdout="changed expectation\n", stderr=""),
                 dict(source="\"timeout-control\"", code=0, stdout="\"timeout-control\"\n", stderr="")]
        corpus.write_text(json.dumps(dict(version=1, cases=cases)))
        output = root / "mismatch"
        output.mkdir()
        original = module.observe
        seen = []

        def hang_on_sentinel(command, cwd, timeout):
            if command[-1] != cases[2]["source"]:
                return original(command, cwd, timeout)
            seen.append(timeout)
            return original([sys.executable, "-c", "import time; time.sleep(10)"], cwd, 0.05)

        module.observe = hang_on_sentinel
        report = module.verify(args.reference.resolve(), output, ["json"])
        module.observe = original
        if report["status"] != "failed" or report["failed"] != 2 or report["drift"]:
            raise AssertionError("changed expectation and timeout were not both rejected")
        rows = json.loads((output / "json-observations.json").read_text())["observations"]
        if [row["passed"] for row in rows] != [True, False, False]:
            raise AssertionError("changed expectation was not rejected independently")
        if rows[2]["actual"]["code"] != "timeout":
            raise AssertionError("timeout was not preserved by the replay")
        if seen != [30]:
            raise AssertionError(f"replay did not pass the JSON case timeout: {seen!r}")
        print("REFERENCE CONTROL changed-expectation PASS", flush=True)
        print("REFERENCE CONTROL timeout PASS", flush=True)
        corpus.write_text(json.dumps(dict(version=1, cases=[cases[0], dict(cases[1], stdout="true\n")])))
        observe = module.observe

        def mutate_input(command, cwd, timeout):
            result = observe(command, cwd, timeout)
            corpus.write_text(corpus.read_text() + "\n")
            return result

        module.observe = mutate_input
        output = root / "drift"
        output.mkdir()
        report = module.verify(args.reference.resolve(), output, ["json"])
        if report["status"] != "failed" or report["failed"] or report["drift"] != [str(corpus)]:
            raise AssertionError("input drift was not rejected independently")
        print("REFERENCE CONTROL input-drift PASS", flush=True)
        module.observe = original

        core_directory = root / "bend2/tests"
        core_directory.mkdir(parents=True)
        for name in ("cli_core_check.py", "cli_core_expected.json"):
            shutil.copyfile(ROOT / "bend2/tests" / name, core_directory / name)
        shutil.copyfile(ROOT / "dev/bend2-process.py", root / "dev/bend2-process.py")
        core = module.load_helper("core-cli")
        corpus = core_directory / "cli_core_expected.json"
        frozen = json.loads(corpus.read_text())
        inventory = core.load_cases(corpus)
        names = [case["name"] for case in inventory]
        if len(names) != 28 or len(set(names)) != 28:
            raise AssertionError("core CLI inventory changed")
        for control in ("missing-case", "extra-case", "invalid-bytes", "invalid-status"):
            damaged = json.loads(json.dumps(frozen))
            observations = damaged["observations"]
            if control == "missing-case":
                del observations["emit"]
            elif control == "extra-case":
                observations["unknown case"] = observations["emit"]
            elif control == "invalid-bytes":
                observations["emit"]["bytes"] = "abc"
            else:
                observations["emit"]["code"] = True
            corpus.write_text(json.dumps(damaged))
            try:
                core.load_cases(corpus)
            except ValueError as error:
                if control == "invalid-bytes" and "invalid core CLI artifact: emit" not in str(error):
                    raise AssertionError(f"core CLI invalid-bytes control gave {error}")
                print(f"REFERENCE CONTROL core-{control} PASS", flush=True)
            else:
                raise AssertionError(f"core CLI accepted {control}")

        frozen["observations"]["emit"]["bytes"] = "ff" + frozen["observations"]["emit"]["bytes"][2:]
        frozen["observations"]["run node"]["stdout"] = "changed runtime answer\n"
        corpus.write_text(json.dumps(frozen))
        core_observe = core.observe

        def core_timeout(command, case, cwd):
            if case["name"] == "usage":
                command = [sys.executable, "-c",
                           "import time; print('timeout control', flush=True); time.sleep(10)"]
                return core_observe(command, case, cwd, timeout=0.2)
            return core_observe(command, case, cwd)

        core.observe = core_timeout
        load_helper = module.load_helper
        module.load_helper = lambda name: core if name == "core-cli" else load_helper(name)
        output = root / "core-controls"
        output.mkdir()
        report = module.verify(args.reference.resolve(), output, ["core-cli"])
        if report["status"] != "failed" or report["failed"] != 3 or report["drift"]:
            raise AssertionError("Wasm bytes, runtime answer and timeout were not rejected independently")
        rows = json.loads((output / "core-cli-observations.json").read_text())["observations"]
        failed = {names[row["index"]] for row in rows if not row["passed"]}
        if failed != {"usage", "emit", "run node"}:
            raise AssertionError(f"unexpected core CLI control failures: {failed}")
        timeout = rows[names.index("usage")]["actual"]
        if timeout["code"] != "timeout" or timeout["stdout"] != "timeout control\n":
            raise AssertionError("core timeout did not preserve its verdict and captured output")
        for control in ("wasm-bytes", "runtime-answer", "timeout"):
            print(f"REFERENCE CONTROL core-{control} PASS", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
