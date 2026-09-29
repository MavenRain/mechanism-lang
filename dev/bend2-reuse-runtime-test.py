#!/usr/bin/env python3
"""Exercise concrete and symbolic reuse replay refusals with a real OCaml build."""
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
import threading

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("reuse_reference", ROOT / "dev/bend2-reference-check.py")
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    source_helper = reference.load_helper("reuse-runtime")
    passed = []

    def accepted(name):
        passed.append(name)
        print(f"REUSE REFERENCE CONTROL {name} PASS", flush=True)

    def refused(name, action, message):
        try:
            action()
        except ValueError as error:
            if str(error) != message:
                raise AssertionError(f"reuse refused {name} for another reason: {error}") from error
        else:
            raise AssertionError(f"reuse accepted {name}")
        accepted(name)

    with tempfile.TemporaryDirectory(prefix="mechanism-reuse-controls-") as directory:
        root = Path(directory).resolve()
        inputs = ("dev/bend2-cli-check.py", "dev/bend2-surface-check.py",
                  "dev/bend2-process.py", "dev/bend2-reuse-runtime-check.py",
                  "bend2/tests/cli_core_check.py", "dev/bend2/reuse-runtime-cases.json",
                  *(str(path.relative_to(ROOT)) for path in source_helper.SOURCE_PATHS))
        for relative in inputs:
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        reference.ROOT = root
        helper = reference.load_helper("reuse-runtime")
        corpus = root / "dev/bend2/reuse-runtime-cases.json"
        frozen = json.loads(corpus.read_text())
        names = [case["name"] for case in helper.load_cases(corpus)]
        if len(names) != 104 or len(set(names)) != 104:
            raise AssertionError("reuse runtime inventory changed")

        delete = object()
        inventory_error = "reuse runtime expected observations do not match the case inventory"
        source_error = "reuse runtime source hashes do not match the frozen corpus"
        emit = "concrete/original/repeated/emit"
        check = "concrete/original/check"
        answer = "symbolic/changed/offset/kernel"
        controls = (
            ("missing-case", ["observations", check], delete, inventory_error),
            ("missing-symbolic-case", ["observations", "symbolic/original/check"], delete, inventory_error),
            ("extra-case", ["observations", "extra"], {}, inventory_error),
            ("invalid-bytes", ["observations", emit, "bytes"], "abc",
             f"invalid reuse runtime artifact: {emit}"),
            ("invalid-status", ["observations", emit, "code"], True,
             f"invalid reuse runtime exit status: {emit}"),
            ("missing-wasm", ["observations", emit, "bytes"], None,
             f"missing or unexpected reuse runtime Wasm: {emit}"),
            ("extra-wasm", ["observations", check, "bytes"], "0061736d01000000",
             f"missing or unexpected reuse runtime Wasm: {check}"),
            ("changed-answer", ["observations", answer, "stdout"], "80\n",
             f"unexpected reuse runtime answer: {answer}"),
            ("export-swap", ["observations", "concrete/original/repeated/kernel", "stdout"], "80\n",
             "unexpected reuse runtime answer: concrete/original/repeated/kernel"),
            ("missing-source", ["sources"], delete, source_error),
            ("changed-template", ["sources", "test/fixtures/prelude/template-composition.mech"], "0" * 64, source_error),
            ("changed-chain", ["sources", "test/fixtures/prelude/reuse-runtime-shared.mech"], "0" * 64, source_error),
            ("changed-concrete-input", ["sources", "test/fixtures/prelude/reuse-runtime-concrete.mech"], "0" * 64, source_error),
            ("changed-symbolic-input", ["sources", "test/fixtures/prelude/reuse-runtime-symbolic.mech"], "0" * 64, source_error),
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
            refused(name, lambda: helper.load_cases(corpus), message)
        corpus.write_text(json.dumps(frozen))

        anchor = "def reuseInput : Nat := 37"
        for mode, fixture in helper.FIXTURES.items():
            original = fixture.read_text()
            for label, changed in (("missing", original.replace(anchor, "def reuseInput : Nat := 38")),
                                   ("duplicate", original + "\n" + anchor + "\n")):
                fixture.write_text(changed)
                refused(f"{mode}-{label}-anchor", lambda: helper.sources(mode, "changed"),
                        "reuse runtime payload must occur exactly once")
            fixture.write_text(original)
        refused("unknown-mode", lambda: helper.sources("unknown", "original"),
                "unknown reuse runtime mode: unknown")
        refused("unknown-variant", lambda: helper.sources("concrete", "unknown"),
                "unknown reuse runtime variant: unknown")

        original_load = reference.load_helper
        seen_timeouts = []
        wrong_answers = {"concrete/original/repeated/kernel": "80\n",
                         "symbolic/changed/identityOffset/kernel": "44\n"}
        wrong_bytes = ("concrete/original/offset/emit", "symbolic/changed/identity/build")
        timeout_case = "symbolic/changed/repeated/node"
        selected = {*wrong_answers, *wrong_bytes, timeout_case, names[-1]}
        replay_names = [name for name in names if name in selected]

        def controlled_load(name):
            loaded = original_load(name)
            if name != "reuse-runtime":
                return loaded
            observe = loaded.observe
            load_cases = loaded.load_cases

            # Validate the entire inventory, then damage six live observations.
            def control_cases(path):
                return [case for case in load_cases(path) if case["name"] in selected]

            loaded.load_cases = control_cases

            def damaged_observation(command, case, cwd, **options):
                if case["name"] == timeout_case:
                    seen_timeouts.append(options.get("timeout",
                        inspect.signature(observe).parameters["timeout"].default))
                    child = [sys.executable, "-S", "-c",
                             "import time; print('reuse timeout', flush=True); time.sleep(60)"]

                    def sleeping_child(limits):
                        attempt = observe(child, case, cwd=cwd, timeout=limits[0])
                        stalled = attempt["code"] == "timeout" and attempt["stdout"] == "" and len(limits) > 1
                        return sleeping_child(limits[1:]) if stalled else attempt
                    return sleeping_child((2, 5, 10))
                actual = observe(command, case, cwd=cwd, **options)
                if case["name"] in wrong_answers:
                    actual["stdout"] = wrong_answers[case["name"]]
                elif case["name"] in wrong_bytes:
                    binary = bytes.fromhex(actual["bytes"])
                    actual["bytes"] = (binary[:-1] + bytes([binary[-1] ^ 1])).hex()
                return actual

            completed = 0
            lock = threading.Lock()

            def tracked_observation(*arguments, **options):
                nonlocal completed
                actual = damaged_observation(*arguments, **options)
                with lock:
                    completed += 1
                    if completed == len(selected):
                        for source in loaded.SOURCE_PATHS:
                            source.write_text(source.read_text() + "\n")
                return actual

            loaded.observe = tracked_observation
            return loaded

        reference.load_helper = controlled_load
        output = root / "replay"
        output.mkdir()
        report = reference.verify(args.reference.resolve(), output, ["reuse-runtime"], jobs=4)
        if (report["status"] != "failed" or report["failed"] != 5 or report["runtime_jobs"] != 4 or
                set(report["drift"]) != {str(path) for path in helper.SOURCE_PATHS}):
            raise AssertionError(f"reuse replay controls were not rejected: {report['status']}, "
                                 f"{report['failed']}, {report['drift']}")
        rows = json.loads((output / "reuse-runtime-observations.json").read_text())["observations"]
        if len(rows) != len(replay_names):
            raise AssertionError("reuse control replay lost observations")
        failed = {replay_names[row["index"]]: row for row in rows if not row["passed"]}
        if set(failed) != {*wrong_answers, *wrong_bytes, timeout_case}:
            raise AssertionError(f"unexpected reuse failures: {list(failed)}")
        timeout = failed[timeout_case]["actual"]
        if (seen_timeouts != [20] or timeout["code"] != "timeout" or
                timeout["stdout"] != "reuse timeout\n"):
            raise AssertionError(f"reuse timeout or its output was lost: {timeout}")
        for name in ("concrete-answer", "symbolic-answer", "wasm-bytes", "build-bytes", "timeout-output", "source-drift"):
            accepted(name)
    print(f"REUSE REFERENCE CONTROLS PASS controls={len(passed)}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
