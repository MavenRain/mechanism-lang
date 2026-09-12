"""Replay left Kan extension controls against the built checker."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
REPLAY = 600


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python3 -I dev/left-kan-mutations.py NEW_DIRECTORY")
    work = Path(sys.argv[1]).resolve()
    if work.exists() or work == ROOT or ROOT in work.parents:
        raise SystemExit("the output directory must be new and outside the repository")
    executable = ROOT / "_build/default/test/prelude_left_kan.exe"
    if not executable.is_file():
        raise SystemExit("build the left Kan extension checker before replay")
    copy = work / "source"
    files = [ROOT / "prelude/cat/category.mech"]
    files.extend(ROOT / f"test/fixtures/prelude/{name}.mech" for name in
                 ["nattrans", "left-kan-identity", "left-kan-common", "left-kan"])
    files.extend(sorted((ROOT / "test/neg/left-kan").iterdir()))
    sources = {}
    for path in files:
        relative = path.relative_to(ROOT)
        target = copy / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        sources[str(relative)] = hashlib.sha256(path.read_bytes()).hexdigest()
    env = dict(os.environ)
    env.pop("OPAM_SWITCH_PREFIX", None)
    env.pop("CAML_LD_LIBRARY_PATH", None)

    def suite(label):
        print(f"LEFT-KAN-MUTATIONS running {label}", flush=True)
        started = time.monotonic()
        result = subprocess.run([str(executable), str(copy)], cwd=ROOT, env=env,
                                capture_output=True, timeout=REPLAY)
        (work / f"{label}.stdout").write_bytes(result.stdout)
        (work / f"{label}.stderr").write_bytes(result.stderr)
        print(f"LEFT-KAN-MUTATIONS finished {label} exit={result.returncode} "
              f"elapsed_s={time.monotonic() - started:.3f}", flush=True)
        return result

    def passed(result):
        return (result.returncode == 0 and result.stderr == b""
                and result.stdout == b"PRELUDE-LEFT-KAN-OK entries=203 instances=4 "
                b"computations=6 negatives=7\n")

    if not passed(suite("baseline")):
        raise SystemExit("baseline failed; no control ran")
    controls = [
        ("U1-LAN-M1", "test/fixtures/prelude/left-kan.mech",
         "(Mediator.1 (next zero)).1 zero", "(Mediator.1 zero).1 zero",
         "PRELUDE-LEFT-KAN-FAIL wrong computation: lanDescValue = "
         "(In SMu Tiny [] (ACtor zero) [])"),
        ("U1-LAN-M2", "test/fixtures/prelude/left-kan.mech",
         "Constant Constant Eta Beta OtherChosen",
         "Constant Constant Eta Alpha Chosen",
         "PRELUDE-LEFT-KAN-FAIL wrong computation: lanDescOther = "
         "(In SMu Tiny [] (ACtor zero) [])"),
        ("U1-LAN-M3", "prelude/cat/category.mech",
         "=> s.1", "=> s.2",
         "PRELUDE-LEFT-KAN-FAIL mismatch: the term has type "
         "(Lan SPi w fac"),
        ("U1-LAN-M4", "prelude/cat/category.mech",
         "(lanUniq J C D j c d K F H G eta alpha s beta2 h2 y)",
         "(lanUniq J C D j c d K F H G eta alpha s beta1 h1 y)",
         "PRELUDE-LEFT-KAN-FAIL mismatch: the term has type "
         "(Lan SMu MechCategory [(Out SPi w x C (APt w y)"),
        ("U1-LAN-M5", "prelude/cat/category.mech",
         "(eta.1 x) (beta.1 (K.1 x))", "(beta.1 (K.1 x)) (eta.1 x)",
         "PRELUDE-LEFT-KAN-FAIL mismatch: the term has type "
         "(Out SPi 0 y D"),
        ("U1-LAN-M6", "test/fixtures/prelude/left-kan.mech",
         "((fun (n : Tiny) => next n), (fun (n : Tiny) => n))).2 (next zero)",
         "((fun (n : Tiny) => n), (fun (n : Tiny) => n))).2 (next zero)",
         "PRELUDE-LEFT-KAN-FAIL wrong computation: lanMapValue = "
         "(In SMu Tiny [] (ACtor next) [(In SMu Tiny [] (ACtor zero) [])])"),
    ]
    reports = []
    for name, relative, old, new, diagnostic in controls:
        path = copy / relative
        original = path.read_bytes()
        source = original.decode()
        pattern = r"\s+".join(map(re.escape, old.split()))
        matches = list(re.finditer(pattern, source))
        if len(matches) != 1:
            raise SystemExit(f"{name}: expected one mutation anchor")
        chosen, = matches
        try:
            path.write_text(source[:chosen.start()] + new + source[chosen.end():])
            mutant_hash = hashlib.sha256(path.read_bytes()).hexdigest()
            result = suite(name)
            killed = (result.returncode == 1 and result.stderr == b""
                      and result.stdout.startswith(diagnostic.encode()))
            reports.append({"name": name, "path": relative, "old": old, "new": new,
                            "replacements": 1, "diagnostic": diagnostic,
                            "mutant_sha256": mutant_hash, "exit_code": result.returncode,
                            "killed": killed})
            print(json.dumps({"control": name, "killed": killed}), flush=True)
        finally:
            path.write_bytes(original)
    restored = passed(suite("restored"))
    success = restored and all(row["killed"] for row in reports)
    captures = {}
    for label in ["baseline", *(row["name"] for row in reports), "restored"]:
        captures[label] = {
            stream: hashlib.sha256((work / f"{label}.{stream}").read_bytes()).hexdigest()
            for stream in ["stdout", "stderr"]}
    report = {"passed": success, "baseline": True, "restored": restored,
              "sources": sources, "capture_sha256": captures,
              "checker_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "controls": reports}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"LEFT-KAN-MUTATIONS {'OK' if success else 'FAIL'} "
          f"controls={len(reports)} restored={int(restored)}")
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"LEFT-KAN-MUTATIONS FAIL {error}", file=sys.stderr)
        sys.exit(2)
