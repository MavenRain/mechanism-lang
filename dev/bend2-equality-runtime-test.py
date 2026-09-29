#!/usr/bin/env python3
"""Exercise equality runtime replay refusal paths against a real OCaml build."""
from __future__ import annotations

import argparse
import importlib.util
import inspect
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
    spec = importlib.util.spec_from_file_location("equality_reference", ROOT / "dev/bend2-reference-check.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="mechanism-equality-controls-") as directory:
        root = Path(directory).resolve()
        for relative in ("dev/bend2-cli-check.py", "dev/bend2-surface-check.py",
                         "dev/bend2-process.py", "dev/bend2-equality-runtime-check.py",
                         "bend2/tests/cli_core_check.py", "prelude/init.mech",
                         "test/fixtures/prelude/equality-runtime.mech",
                         "dev/bend2/equality-runtime-cases.json"):
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        module.ROOT = root
        helper = module.load_helper("equality-runtime")
        corpus = root / "dev/bend2/equality-runtime-cases.json"
        frozen = json.loads(corpus.read_text())
        inventory = helper.load_cases(corpus)
        names = [case["name"] for case in inventory]
        if len(names) != 34 or len(set(names)) != 34:
            raise AssertionError("equality runtime inventory changed")
        messages = {
            "missing-case": "equality runtime expected observations do not match the case inventory",
            "extra-case": "equality runtime expected observations do not match the case inventory",
            "invalid-bytes": "invalid equality runtime artifact: original/Literal/emit",
            "invalid-status": "invalid equality runtime exit status: original/Literal/emit",
            "missing-wasm": "missing or unexpected equality runtime Wasm: original/Literal/emit",
            "extra-wasm": "missing or unexpected equality runtime Wasm: original/check",
            "changed-answer": "unexpected equality runtime answer: changed/Closure/kernel",
            "missing-source": "equality runtime source hashes do not match the frozen corpus",
            "changed-source": "equality runtime source hashes do not match the frozen corpus",
        }
        for control, message in messages.items():
            damaged = json.loads(json.dumps(frozen))
            observations = damaged["observations"]
            emit = observations["original/Literal/emit"]
            if control == "missing-case":
                del observations["original/check"]
            elif control == "extra-case":
                observations["unknown case"] = emit
            elif control == "invalid-bytes":
                emit["bytes"] = "abc"
            elif control == "invalid-status":
                emit["code"] = True
            elif control == "missing-wasm":
                emit["bytes"] = None
            elif control == "extra-wasm":
                observations["original/check"]["bytes"] = emit["bytes"]
            elif control == "changed-answer":
                observations["changed/Closure/kernel"]["stdout"] = "42\n"
            elif control == "missing-source":
                damaged.pop("sources")
            elif control == "changed-source":
                damaged["sources"]["prelude/init.mech"] = "0" * 64
            corpus.write_text(json.dumps(damaged))
            try:
                helper.load_cases(corpus)
            except ValueError as error:
                if str(error) != message:
                    raise AssertionError(f"equality runtime refused {control} for another reason: {error}")
            else:
                raise AssertionError(f"equality runtime accepted {control}")
            print(f"EQUALITY REFERENCE CONTROL {control} PASS", flush=True)
        corpus.write_text(json.dumps(frozen))
        fixture = root / "test/fixtures/prelude/equality-runtime.mech"
        original = fixture.read_text()
        fixture.write_text(original.replace("def equalityRuntimePayload : Nat := 37",
                                            "def equalityRuntimePayload : Nat := 38"))
        try:
            helper.sources("changed")
        except ValueError:
            pass
        else:
            raise AssertionError("equality runtime accepted a stale mutation anchor")
        fixture.write_text(original)
        print("EQUALITY REFERENCE CONTROL mutation-anchor PASS", flush=True)

        original_load = module.load_helper
        seen_timeouts = []

        def controlled_load(name):
            loaded = original_load(name)
            if name != "equality-runtime":
                return loaded
            observe = loaded.observe

            def damaged_observation(command, case, cwd, **options):
                if case["name"] == "changed/Struct/node":
                    seen_timeouts.append(options.get("timeout",
                                                     inspect.signature(observe).parameters["timeout"].default))
                    return observe([sys.executable, "-c",
                                    "import time; print('equality timeout', flush=True); time.sleep(10)"],
                                   case, cwd=cwd, timeout=0.5)
                actual = observe(command, case, cwd=cwd, **options)
                if case["name"] == "original/Literal/kernel":
                    actual["stdout"] = "38\n"
                elif case["name"] == "original/Closure/emit":
                    binary = bytes.fromhex(actual["bytes"])
                    actual["bytes"] = (binary[:-1] + bytes([binary[-1] ^ 1])).hex()
                elif case["name"] == names[-1]:
                    fixture.write_text(fixture.read_text() + "\n")
                return actual

            loaded.observe = damaged_observation
            return loaded

        module.load_helper = controlled_load
        output = root / "replay"
        output.mkdir()
        report = module.verify(args.reference.resolve(), output, ["equality-runtime"])
        if (report["status"] != "failed" or report["failed"] != 3 or
                report["drift"] != [str(fixture)]):
            raise AssertionError(f"equality runtime replay controls were not rejected: {report['status']}, "
                                 f"{report['failed']}, {report['drift']}")
        rows = json.loads((output / "equality-runtime-observations.json").read_text())["observations"]
        failed = {names[row["index"]]: row for row in rows if not row["passed"]}
        if set(failed) != {"original/Literal/kernel", "original/Closure/emit", "changed/Struct/node"}:
            raise AssertionError(f"unexpected equality runtime failures: {list(failed)}")
        timeout = failed["changed/Struct/node"]["actual"]
        if (seen_timeouts != [20] or timeout["code"] != "timeout" or
                timeout["stdout"] != "equality timeout\n"):
            raise AssertionError(f"equality runtime timeout or its output was lost: {timeout}")
        for control in ("runtime-answer", "wasm-bytes", "timeout-output", "source-drift"):
            print(f"EQUALITY REFERENCE CONTROL {control} PASS", flush=True)
    print("EQUALITY REFERENCE CONTROLS PASS controls=14", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
