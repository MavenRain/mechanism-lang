#!/usr/bin/env python3
"""Replay the frozen core CLI cases, including Wasm bytes and runtime hosts."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = Path(__file__).with_name("cli_core_expected.json")
spec = importlib.util.spec_from_file_location("core_cli_process", ROOT / "dev/bend2-process.py")
process = importlib.util.module_from_spec(spec)
spec.loader.exec_module(process)

SOURCES = {
    "good ' ;$().kan": "def main : Nat := 42\n",
    "bad.kan": "def main : Nat := unknown\n",
    "axiom.kan": "axiom hole : Nat\ndef main : Nat := 42\n",
    "circuit.kan": "def main : Nat := natAdd 1 2\n",
    "refused.kan": "axiom opaque : Nat\ndef main : Nat := opaque\n",
    "big.kan": "def main : Nat := 1073741824\n",
    "a.kan": "def add : Nat -> Nat -> Nat := fun (a : Nat) (b : Nat) => natAdd a b\n",
    "b.kan": "def main : Nat := add 40 2\n",
}


def cases() -> list[dict]:
    good = "$ROOT/good ' ;$().kan"
    bad, proof, circuit = "$ROOT/bad.kan", "$ROOT/axiom.kan", "$ROOT/circuit.kan"
    refused, big = "$ROOT/refused.kan", "$ROOT/big.kan"
    a, b, out = "$ROOT/a.kan", "$ROOT/b.kan", "$ROOT/out.wasm"
    result = []

    def add(name, args, artifact=None):
        result.append(dict(name=name, args=args, artifact=artifact))

    add("usage", ["check"])
    add("help passthrough", ["--help"])
    add("unknown", ["unknown"])
    add("spec-count", ["spec-count", "ignored"])
    add("check", ["check", good])
    add("checked form", ["check", "--print", good])
    add("checked trailing args", ["check", "--print", good, "ignored"])
    add("erased form", ["check", "--erased", good])
    add("check failure", ["check", bad])
    add("missing input", ["check", "$ROOT/missing.kan"])
    add("axioms", ["axioms", proof])
    add("circuit", ["circuit", circuit])
    add("circuit refusal silent stderr", ["circuit", refused])
    add("emit", ["emit", good, "-o", out, "--export", "main"], out)
    add("emit missing export", ["emit", good, "-o", out, "--export", "absent"], out)
    add("emit output path", ["emit", good, "-o", "$ROOT/absent/out.wasm", "--export", "main"])
    add("build", ["build", a, b, "-o", out, "--export", "add", "--export", "main"], out)
    add("build interspersed flags", ["build", a, "--export", "main", "-o", out, b], out)
    add("build repeated output", ["build", a, b, "-o", out, "-o", out, "--export", "main"])
    add("build repeated export", ["build", good, "-o", out, "--export", "main", "--export", "main"], out)
    add("run kernel", ["run", good, "--export", "main", "--host", "kernel"])
    add("run node", ["run", good, "--export", "main", "--host", "node"])
    add("run wasmtime", ["run", good, "--export", "main", "--host", "wasmtime"])
    add("run both", ["run", good, "--export", "main"])
    add("run kernel trap", ["run", big, "--export", "main", "--host", "kernel"])
    add("run both trap", ["run", big, "--export", "main", "--host", "both"])
    add("run bad host", ["run", good, "--export", "main", "--host", "unknown"])
    add("run missing file", ["run", "$ROOT/missing.kan", "--export", "main"])
    return result


def load_cases(path: Path = EXPECTED) -> list[dict]:
    expected = json.loads(path.read_text())["observations"]
    inventory = cases()
    if not isinstance(expected, dict) or set(expected) != {case["name"] for case in inventory}:
        raise ValueError("core CLI expected observations do not match the case inventory")
    for case in inventory:
        observation = expected[case["name"]]
        if not isinstance(observation, dict) or set(observation) != {"code", "stdout", "stderr", "bytes"}:
            raise ValueError(f"invalid core CLI observation: {case['name']}")
        if type(observation["code"]) is not int or not 0 <= observation["code"] <= 255:
            raise ValueError(f"invalid core CLI exit status: {case['name']}")
        if any(not isinstance(observation[key], str) for key in ("stdout", "stderr")):
            raise ValueError(f"invalid core CLI output: {case['name']}")
        artifact = observation["bytes"]
        if artifact is not None:
            if not isinstance(artifact, str) or re.fullmatch(r"(?:[0-9a-f]{2})*", artifact) is None:
                raise ValueError(f"invalid core CLI artifact: {case['name']}")
        case["expected"] = observation
    return inventory


def observe(command: list[str], case: dict, cwd: Path = ROOT, timeout: float = 30) -> dict:
    with tempfile.TemporaryDirectory(prefix="bend-cli-core-") as directory:
        root = Path(directory)
        for name, source in SOURCES.items():
            (root / name).write_text(source)
        args = [argument.replace("$ROOT", directory) for argument in case["args"]]
        artifact = case["artifact"]
        path = Path(artifact.replace("$ROOT", directory)) if artifact is not None else None

        def output(value):
            return (value or b"").decode(errors="replace").replace(directory, "{TMP}")

        try:
            result = process.run([*command, *args], cwd=cwd, capture_output=True, timeout=timeout)
            return dict(code=result.returncode, stdout=output(result.stdout), stderr=output(result.stderr),
                        bytes=path.read_bytes().hex() if path is not None and path.is_file() else None)
        except subprocess.TimeoutExpired as error:
            return dict(code="timeout", stdout=output(error.stdout), stderr=output(error.stderr),
                        timeout=timeout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    options = parser.parse_args()
    driver = options.driver.resolve()
    command = ["node", "--stack-size=16384", str(driver), "--"] if driver.suffix == ".js" else [str(driver)]
    checks, failures = [], []
    for case in load_cases():
        actual = observe(command, case)
        if actual != case["expected"]:
            print(json.dumps(dict(case=case["name"], expected=case["expected"], actual=actual), indent=2), flush=True)
            failures.append(case["name"])
        else:
            checks.append(case["name"])
            print(f"passed: {case['name']}", flush=True)
    print(json.dumps(dict(passed=len(checks), failed=failures)), flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
