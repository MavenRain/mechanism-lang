#!/usr/bin/env python3
"""Exercise composition replay refusal paths against a real OCaml build."""
from __future__ import annotations

import argparse
import copy
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
    spec = importlib.util.spec_from_file_location("composition_reference", ROOT / "dev/bend2-reference-check.py")
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    with tempfile.TemporaryDirectory(prefix="mechanism-composition-controls-") as directory:
        root = Path(directory).resolve()
        for relative in ("dev/bend2-cli-check.py", "dev/bend2-surface-check.py",
                         "dev/bend2-process.py", "dev/bend2-composition-runtime-check.py",
                         "bend2/tests/cli_core_check.py",
                         "test/fixtures/prelude/template-composition.mech",
                         "test/fixtures/prelude/composition-runtime.mech",
                         "dev/bend2/composition-runtime-cases.json"):
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        reference.ROOT = root
        helper = reference.load_helper("composition-runtime")
        corpus = root / "dev/bend2/composition-runtime-cases.json"
        frozen = json.loads(corpus.read_text())
        names = [case["name"] for case in helper.load_cases(corpus)]
        if len(names) != 40 or len(set(names)) != 40:
            raise AssertionError("composition runtime inventory changed")

        delete = object()
        inventory_error = "composition runtime expected observations do not match the case inventory"
        source_error = "composition runtime source hashes do not match the frozen corpus"
        controls = (
            ("missing-case", ["observations", "original/check"], delete, inventory_error),
            ("extra-case", ["observations", "extra"], {}, inventory_error),
            ("invalid-bytes", ["observations", "original/forward/emit", "bytes"], "abc",
             "invalid composition runtime artifact: original/forward/emit"),
            ("invalid-status", ["observations", "original/forward/emit", "code"], True,
             "invalid composition runtime exit status: original/forward/emit"),
            ("missing-wasm", ["observations", "original/forward/emit", "bytes"], None,
             "missing or unexpected composition runtime Wasm: original/forward/emit"),
            ("extra-wasm", ["observations", "original/check", "bytes"], "0061736d01000000",
             "missing or unexpected composition runtime Wasm: original/check"),
            ("changed-answer", ["observations", "changed/forward/kernel", "stdout"], "78\n",
             "unexpected composition runtime answer: changed/forward/kernel"),
            ("composition-order", ["observations", "original/forward/kernel", "stdout"], "76\n",
             "unexpected composition runtime answer: original/forward/kernel"),
            ("missing-source", ["sources"], delete, source_error),
            ("changed-template", ["sources", "test/fixtures/prelude/template-composition.mech"],
             "0" * 64, source_error),
            ("changed-input", ["sources", "test/fixtures/prelude/composition-runtime.mech"],
             "0" * 64, source_error),
        )
        for name, path, value, message in controls:
            damaged = copy.deepcopy(frozen)
            target = damaged
            for key in path[:-1]:
                target = target[key]
            if value is delete:
                del target[path[-1]]
            else:
                target[path[-1]] = value
            corpus.write_text(json.dumps(damaged))
            try:
                helper.load_cases(corpus)
            except ValueError as error:
                if str(error) != message:
                    raise AssertionError(f"composition refused {name} for another reason: {error}")
            else:
                raise AssertionError(f"composition accepted {name}")
            print(f"COMPOSITION REFERENCE CONTROL {name} PASS", flush=True)
        corpus.write_text(json.dumps(frozen))

        fixture = helper.SOURCE_PATHS[1]
        original = fixture.read_text()
        fixture.write_text(original.replace("def compositionInput : Nat := 37",
                                            "def compositionInput : Nat := 38"))
        try:
            helper.sources("changed")
        except ValueError as error:
            if str(error) != "composition runtime payload must occur exactly once":
                raise
        else:
            raise AssertionError("composition accepted a stale mutation anchor")
        fixture.write_text(original)
        print("COMPOSITION REFERENCE CONTROL mutation-anchor PASS", flush=True)
        try:
            helper.sources("unknown")
        except ValueError as error:
            if str(error) != "unknown composition runtime variant: unknown":
                raise
        else:
            raise AssertionError("composition accepted an unknown variant")
        print("COMPOSITION REFERENCE CONTROL unknown-variant PASS", flush=True)

        original_load = reference.load_helper
        seen_timeouts = []

        def controlled_load(name):
            loaded = original_load(name)
            if name != "composition-runtime":
                return loaded
            observe = loaded.observe

            def damaged_observation(command, case, cwd, **options):
                if case["name"] == "changed/reverse/node":
                    seen_timeouts.append(options.get("timeout",
                        inspect.signature(observe).parameters["timeout"].default))
                    child = [sys.executable, "-S", "-c",
                             "import time; print('composition timeout', flush=True); time.sleep(60)"]

                    def sleeping_child(limits):
                        attempt = observe(child, case, cwd=cwd, timeout=limits[0])
                        stalled = attempt["code"] == "timeout" and attempt["stdout"] == "" and len(limits) > 1
                        return sleeping_child(limits[1:]) if stalled else attempt
                    return sleeping_child((2, 5, 10))
                actual = observe(command, case, cwd=cwd, **options)
                if case["name"] == "original/forward/kernel":
                    actual["stdout"] = "76\n"
                elif case["name"] in ("original/nested/emit", "original/reverse/build"):
                    binary = bytes.fromhex(actual["bytes"])
                    actual["bytes"] = (binary[:-1] + bytes([binary[-1] ^ 1])).hex()
                elif case["name"] == names[-1]:
                    for source in loaded.SOURCE_PATHS:
                        source.write_text(source.read_text() + "\n")
                return actual

            loaded.observe = damaged_observation
            return loaded

        reference.load_helper = controlled_load
        output = root / "replay"
        output.mkdir()
        report = reference.verify(args.reference.resolve(), output, ["composition-runtime"])
        if (report["status"] != "failed" or report["failed"] != 4 or
                set(report["drift"]) != {str(path) for path in helper.SOURCE_PATHS}):
            raise AssertionError(f"composition replay controls were not rejected: {report['status']}, "
                                 f"{report['failed']}, {report['drift']}")
        rows = json.loads((output / "composition-runtime-observations.json").read_text())["observations"]
        failed = {names[row["index"]]: row for row in rows if not row["passed"]}
        if set(failed) != {"original/forward/kernel", "original/nested/emit",
                           "original/reverse/build", "changed/reverse/node"}:
            raise AssertionError(f"unexpected composition failures: {list(failed)}")
        timeout = failed["changed/reverse/node"]["actual"]
        if (seen_timeouts != [20] or timeout["code"] != "timeout" or
                timeout["stdout"] != "composition timeout\n"):
            raise AssertionError(f"composition timeout or its output was lost: {timeout}")
        for name in ("runtime-answer", "wasm-bytes", "build-bytes", "timeout-output", "source-drift"):
            print(f"COMPOSITION REFERENCE CONTROL {name} PASS", flush=True)
    print(f"COMPOSITION REFERENCE CONTROLS PASS controls={len(controls) + 7}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
