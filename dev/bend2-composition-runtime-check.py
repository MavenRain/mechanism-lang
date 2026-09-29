#!/usr/bin/env python3
"""Compare template composition, Wasm bytes and runtime hosts with live OCaml."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = ROOT / "dev/bend2/composition-runtime-cases.json"
SOURCE_PATHS = (ROOT / "test/fixtures/prelude/template-composition.mech",
                ROOT / "test/fixtures/prelude/composition-runtime.mech")
spec = importlib.util.spec_from_file_location("composition_core_cli", ROOT / "bend2/tests/cli_core_check.py")
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)


def sources(variant: str) -> dict[str, str]:
    templates, fixture = (path.read_text() for path in SOURCE_PATHS)
    anchor = "def compositionInput : Nat := 37"
    if fixture.count(anchor) != 1:
        raise ValueError("composition runtime payload must occur exactly once")
    if variant == "changed":
        fixture = fixture.replace(anchor, "def compositionInput : Nat := 41")
    elif variant != "original":
        raise ValueError(f"unknown composition runtime variant: {variant}")
    return {"composition.mech": templates + "\n" + fixture,
            "templates.mech": templates, "runtime.mech": fixture}


def cases() -> list[dict]:
    result = []
    source, artifact = "$ROOT/composition.mech", "$ROOT/out.wasm"
    for variant, payload in (("original", 37), ("changed", 41)):
        def add(name, args, *, output="", wasm=None):
            result.append(dict(name=f"{variant}/{name}", variant=variant,
                               args=args, artifact=wasm, output=output))

        add("check", ["check", source])
        add("axioms", ["axioms", source])
        for export, answer in (("forward", (payload + 2) * 2),
                               ("reverse", payload * 2 + 2), ("nested", payload * 2)):
            add(f"{export}/emit", ["emit", source, "-o", artifact, "--export", export], wasm=artifact)
            add(f"{export}/build", ["build", "$ROOT/templates.mech", "$ROOT/runtime.mech",
                                    "-o", artifact, "--export", export], wasm=artifact)
            for host in ("kernel", "node", "wasmtime", "both"):
                add(f"{export}/{host}", ["run", source, "--export", export, "--host", host],
                    output=f"{answer}\n")
    return result


def source_hashes() -> dict[str, str]:
    return {str(source.relative_to(ROOT)): hashlib.sha256(source.read_bytes()).hexdigest()
            for source in SOURCE_PATHS}


def load_cases(path: Path = EXPECTED) -> list[dict]:
    evidence = json.loads(path.read_text())
    if evidence.get("sources") != source_hashes():
        raise ValueError("composition runtime source hashes do not match the frozen corpus")
    inventory = core.load_cases(path, inventory=cases(), label="composition runtime")
    for case in inventory:
        expected = case["expected"]
        if expected["code"] != 0 or expected["stdout"] != case["output"] or expected["stderr"]:
            raise ValueError(f"unexpected composition runtime answer: {case['name']}")
        artifact = expected["bytes"]
        if ((case["artifact"] is None and artifact is not None) or
                (case["artifact"] is not None and
                 (artifact is None or not artifact.startswith("0061736d01000000")))):
            raise ValueError(f"missing or unexpected composition runtime Wasm: {case['name']}")
    return inventory


def observe(command: list[str], case: dict, cwd: Path = ROOT, timeout: float = 20) -> dict:
    return core.observe(command, case, cwd=cwd, timeout=timeout, sources=sources(case["variant"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    options = parser.parse_args()
    driver = options.driver.resolve()
    command = ["node", "--stack-size=16384", str(driver), "--"] if driver.suffix == ".js" else [str(driver)]
    pinned = source_hashes()
    inventory, failures = load_cases(), []
    for case in inventory:
        actual = observe(command, case)
        if actual != case["expected"]:
            failures.append(case["name"])
            print(json.dumps(dict(case=case["name"], expected=case["expected"], actual=actual)), flush=True)
        else:
            print(f"passed: {case['name']}", flush=True)
    current = source_hashes()
    drift = sorted(path for path, digest in pinned.items() if current[path] != digest)
    if drift:
        print(json.dumps(dict(drift=drift)), flush=True)
    print(json.dumps(dict(passed=len(inventory) - len(failures), failed=failures)), flush=True)
    return int(bool(failures or drift))


if __name__ == "__main__":
    raise SystemExit(main())
