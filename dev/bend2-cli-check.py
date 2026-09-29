#!/usr/bin/env python3
"""Replay exact command output, status, and publication observations."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "dev/bend2/cli-cases.json"


def cases() -> list[dict]:
    pipeline = json.loads((ROOT / "dev/bend2/pipeline-cases.json").read_text())["cases"]
    exports = {f"export-{i}.jsonl": pipeline[i]["source"] for i in (1083, 1086, 1087, 1100, 1117)}
    result = []

    def add(name, mode, args, files=None, directories=None):
        result.append(dict(name=name, mode=mode, args=args, files=files or {},
                           directories=directories or []))

    for name, args in (("usage", []), ("import-usage", ["import"]),
                       ("import-extra", ["import", "a", "b"]),
                       ("parity-usage", ["diff-parity"]),
                       ("mapping-usage", ["map-inventory", "--never"]),
                       ("unknown-command", ["wat"]),
                       ("missing-export", ["import", "$ROOT/missing.jsonl"])):
        add(name, "mech", args)
    add("malformed-export", "mech", ["import", "$ROOT/bad.jsonl"], {"bad.jsonl": "not json\n"})
    for filename, source in exports.items():
        files = {filename: source}
        add(filename + "-summary", "mech", ["import", "$ROOT/" + filename], files)
        add(filename + "-parity", "mech", ["diff-parity", "--export", "$ROOT/" + filename], files)
    source = exports["export-1117.jsonl"]
    for never in (False, True):
        add("mapping-never" if never else "mapping", "mech",
            ["map-inventory", "--export", "$ROOT/export.jsonl"] + (["--never"] if never else []),
            {"export.jsonl": source})
    publish = ["import", "$ROOT/export.jsonl", "--out", "$ROOT/reports"]
    add("publish", "mech", publish, {"export.jsonl": source})
    add("publish-existing-directory", "mech", publish,
        {"export.jsonl": source, "reports/keep": "preserve me\n"})
    add("publish-existing-file", "mech", publish,
        {"export.jsonl": source, "reports": "preserve me\n"})
    add("publish-missing-parent", "mech", [*publish[:-1], "$ROOT/missing/reports"],
        {"export.jsonl": source})
    source = "def small : Nat := 42\ndef large : Nat := 184467440737095516170\ndef plus : Nat := natAdd small 1\n"
    for name, exports_ in (("check", []), ("one", ["small"]),
                           ("big", ["large"]), ("computed", ["plus"]),
                           ("ordered", ["large", "small", "plus"]),
                           ("duplicate", ["small", "small"]),
                           ("missing", ["small", "unknown"])):
        add("cert-" + name, "cert", ["$ROOT/cert.mech", *exports_], {"cert.mech": source})
    add("cert-usage", "cert", [])
    add("cert-missing-file", "cert", ["$ROOT/missing.mech"])
    add("cert-axioms", "cert", ["$ROOT/cert.mech", "main"],
        {"cert.mech": "axiom hole : Nat\ndef main : Nat := 42\n"})
    add("cert-nonliteral", "cert", ["$ROOT/cert.mech", "main"],
        {"cert.mech": "def main : Nat -> Nat := fun (x : Nat) => x\n"})
    add("cert-string", "cert", ["$ROOT/cert.mech", "main"],
        {"cert.mech": 'def main : String := "hello"\n'})
    add("cert-invalid", "cert", ["$ROOT/cert.mech"],
        {"cert.mech": "def main : Nat := unknown\n"})
    return result


def observe(command: list[str], case: dict) -> dict:
    with tempfile.TemporaryDirectory(prefix="mechanism-cli-") as tmp:
        directory = Path(tmp)
        for name in case["directories"]:
            (directory / name).mkdir(parents=True, exist_ok=True)
        for name, contents in case["files"].items():
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        expand = lambda value: value.replace("$ROOT", tmp)
        normalize = lambda value: value.replace(tmp, "$ROOT")
        process = subprocess.run(command + [expand(x) for x in case["args"]],
                                 capture_output=True, text=True, cwd=ROOT, timeout=120)
        tree = {}
        for path in sorted(directory.rglob("*")):
            name = str(path.relative_to(directory))
            tree[name] = None if path.is_dir() else normalize(path.read_text())
        return dict(code=process.returncode, stdout=normalize(process.stdout),
                    stderr=normalize(process.stderr), tree=tree)


def comparable(name: str, observation: dict) -> dict:
    result = dict(observation)
    if name == "publish-missing-parent" and "stderr" in result:
        # The previous mkdir diagnostic exposed a process-specific staging name.
        # mkdtemp reports its destination instead. Preserve the error and target.
        result["stderr"] = re.sub(r"mkdir (\$ROOT/missing/reports)\.tmp-[0-9]+:",
                                  r"\1:", result["stderr"])
    return result


def main() -> int:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--mech", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--cert", type=Path, default=ROOT / "_bend2/bin/mech_cert.exe")
    parser.add_argument("--combined", type=Path)
    parser.add_argument("--record-reference", type=Path)
    args = parser.parse_args()
    if args.record_reference:
        drivers = {mode: args.record_reference / "_build/default/bin" / name
                   for mode, name in (("mech", "mech.exe"), ("cert", "mech_cert.exe"))}
        recorded = cases()
        for case in recorded:
            case["expected"] = observe([str(drivers[case["mode"]])], case)
        fixture = dict(version=1, reference={mode: hashlib.sha256(path.read_bytes()).hexdigest()
                                            for mode, path in drivers.items()}, cases=recorded)
        FIXTURE.write_text(json.dumps(fixture, indent=2) + "\n")
    fixture = json.loads(FIXTURE.read_text())
    commands = {"mech": [str(args.mech.resolve())], "cert": [str(args.cert.resolve())]}
    if args.combined:
        commands = {mode: ["node", "--stack-size=16384", str(args.combined.resolve()), "--", mode]
                    for mode in commands}
    failures, observations = [], []
    for case in fixture["cases"]:
        try:
            actual = observe(commands[case["mode"]], case)
        except (OSError, subprocess.TimeoutExpired) as error:
            actual = dict(error=str(error))
        observations.append(dict(name=case["name"], actual=actual))
        if comparable(case["name"], actual) != comparable(case["name"], case["expected"]):
            failures.append(dict(name=case["name"], expected=case["expected"], actual=actual))
    report = ROOT / "build/bend2-cli/report.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(cases=len(fixture["cases"]), failed=len(failures),
                                     fixture_sha256=hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
                                     normalizations=["temporary root", "mkdir process-specific staging path"],
                                     failures=failures, observations=observations), indent=2) + "\n")
    print(json.dumps(dict(cases=len(fixture["cases"]), failed=len(failures), report=str(report))))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
