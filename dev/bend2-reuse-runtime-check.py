#!/usr/bin/env python3
"""Compare concrete and symbolic family reuse with live OCaml runtime cases."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
from pathlib import Path
import signal
import threading

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = ROOT / "dev/bend2/reuse-runtime-cases.json"
PRELUDE_PATHS = (ROOT / "test/fixtures/prelude/template-composition.mech",)
CHAIN = ROOT / "test/fixtures/prelude/reuse-runtime-shared.mech"
FIXTURES = {mode: ROOT / "test/fixtures/prelude" / name for mode, name in (
    ("concrete", "reuse-runtime-concrete.mech"), ("symbolic", "reuse-runtime-symbolic.mech"))}
SOURCE_PATHS = (*PRELUDE_PATHS, CHAIN, *FIXTURES.values())
spec = importlib.util.spec_from_file_location("reuse_core_cli", ROOT / "bend2/tests/cli_core_check.py")
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)


def source_paths(mode: str) -> tuple[Path, ...]:
    if mode not in FIXTURES:
        raise ValueError(f"unknown reuse runtime mode: {mode}")
    return (*PRELUDE_PATHS, *((CHAIN,) if mode == "symbolic" else ()), FIXTURES[mode])


def sources(mode: str, variant: str) -> dict[str, str]:
    parts = {path.name: path.read_text() for path in source_paths(mode)}
    fixture = FIXTURES[mode].name
    anchor = "def reuseInput : Nat := 37"
    if sum(source.count(anchor) for source in parts.values()) != 1:
        raise ValueError("reuse runtime payload must occur exactly once")
    if variant == "changed":
        parts[fixture] = parts[fixture].replace(anchor, "def reuseInput : Nat := 41")
    elif variant != "original":
        raise ValueError(f"unknown reuse runtime variant: {variant}")
    return {"reuse.mech": "\n".join(parts.values()), **parts}


def cases() -> list[dict]:
    result = []
    source, artifact = "$ROOT/reuse.mech", "$ROOT/out.wasm"
    for mode in FIXTURES:
        inputs = [f"$ROOT/{path.name}" for path in source_paths(mode)]
        for variant, payload in (("original", 37), ("changed", 41)):
            def add(name, args, *, output="", wasm=None):
                result.append(dict(name=f"{mode}/{variant}/{name}", mode=mode, variant=variant,
                                   args=args, artifact=wasm, output=output))

            add("check", ["check", source])
            add("axioms", ["axioms", source])
            for export, answer in (("repeated", (payload + 2) * 2 + 5),
                                   ("offset", (payload + 3) * 2),
                                   ("identity", payload), ("identityOffset", payload + 7)):
                add(f"{export}/emit", ["emit", source, "-o", artifact, "--export", export], wasm=artifact)
                add(f"{export}/build", ["build", *inputs, "-o", artifact, "--export", export], wasm=artifact)
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
        raise ValueError("reuse runtime source hashes do not match the frozen corpus")
    inventory = core.load_cases(path, inventory=cases(), label="reuse runtime")
    for case in inventory:
        expected = case["expected"]
        if expected["code"] != 0 or expected["stdout"] != case["output"] or expected["stderr"]:
            raise ValueError(f"unexpected reuse runtime answer: {case['name']}")
        artifact = expected["bytes"]
        if ((case["artifact"] is None and artifact is not None) or
                (case["artifact"] is not None and
                 (artifact is None or not artifact.startswith("0061736d01000000")))):
            raise ValueError(f"missing or unexpected reuse runtime Wasm: {case['name']}")
    return inventory


def observe(command: list[str], case: dict, cwd: Path = ROOT, timeout: float = 20) -> dict:
    return core.observe(command, case, cwd=cwd, timeout=timeout,
                        sources=sources(case["mode"], case["variant"]))


def terminated(signum, frame):
    raise SystemExit(128 + signum)


def compare(inventory: list[dict], results) -> list[str]:
    failures = []
    for case, actual in zip(inventory, results, strict=True):
        if actual != case["expected"]:
            failures.append(case["name"])
            print(json.dumps(dict(case=case["name"], expected=case["expected"], actual=actual)), flush=True)
        else:
            print(f"passed: {case['name']}", flush=True)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--jobs", type=int, choices=range(1, 9), default=1)
    options = parser.parse_args()
    driver = options.driver.resolve()
    command = ["node", "--stack-size=16384", str(driver), "--"] if driver.suffix == ".js" else [str(driver)]
    pinned = source_hashes()
    inventory = load_cases()
    if options.jobs == 1:
        failures = compare(inventory, map(lambda case: observe(command, case), inventory))
    else:
        on_main = threading.current_thread() is threading.main_thread()
        previous = signal.signal(signal.SIGTERM, terminated) if on_main else None
        try:
            with ThreadPoolExecutor(max_workers=options.jobs) as pool:
                failures = compare(inventory, pool.map(lambda case: observe(command, case), inventory))
        finally:
            if on_main:
                signal.signal(signal.SIGTERM, previous)
    current = source_hashes()
    drift = sorted(path for path, digest in pinned.items() if current[path] != digest)
    if drift:
        print(json.dumps(dict(drift=drift)), flush=True)
    print(json.dumps(dict(passed=len(inventory) - len(failures), failed=failures)), flush=True)
    return int(bool(failures or drift))


if __name__ == "__main__":
    raise SystemExit(main())
