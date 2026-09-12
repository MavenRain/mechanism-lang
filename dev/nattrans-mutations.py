"""Replay natural-transformation controls against the built checker."""
import hashlib
from itertools import islice
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
# The replay is not a gate leg.  It runs the SLOW suite six times, and the
# same suite has taken several minutes under load, so each run gets 600
# seconds.  A shorter cap reports a failure that no control caused.
REPLAY = 600


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python3 -I dev/nattrans-mutations.py NEW_DIRECTORY")
    work = Path(sys.argv[1]).resolve()
    if work.exists() or work == ROOT or ROOT in work.parents:
        raise SystemExit("the output directory must be new and outside the repository")
    executable = ROOT / "_build/default/test/prelude_nattrans.exe"
    if not executable.is_file():
        raise SystemExit("build the natural-transformation checker before replay")
    copy = work / "source"
    files = [ROOT / "prelude/cat/category.mech", ROOT / "test/fixtures/prelude/nattrans.mech"]
    files.extend(sorted((ROOT / "test/neg/nattrans").iterdir()))
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
        result = subprocess.run([str(executable), str(copy)], cwd=ROOT, env=env,
                                capture_output=True, timeout=REPLAY)
        (work / f"{label}.stdout").write_bytes(result.stdout)
        (work / f"{label}.stderr").write_bytes(result.stderr)
        return result

    def passed(result):
        return (result.returncode == 0 and result.stderr == b""
                and result.stdout.startswith(b"PRELUDE-NATTRANS-OK entries="))

    if not passed(suite("baseline")):
        raise SystemExit("baseline failed; no control ran")
    # An anchor is matched token by token, so a reflow of the proof text
    # that changes only whitespace keeps the control alive.  The count is
    # the number of matches the whole file must hold, and the occurrence
    # selects the one the control replaces.
    controls = [
        ("U1-NT-M1", "test/fixtures/prelude/nattrans.mech",
         "Constant Constant Constant Alpha Beta).1 zero", 1, 0,
         "Constant Constant Constant Beta Alpha).1 zero",
         "PRELUDE-NATTRANS-FAIL wrong computation: verticalForward = "
         "(In SMu Tiny [] (ACtor zero) [])"),
        ("U1-NT-M2", "test/fixtures/prelude/nattrans.mech",
         "((fun (x : Tiny) => next zero),", 1, 0, "((fun (x : Tiny) => zero),",
         "PRELUDE-NATTRANS-FAIL wrong computation: rightValue = "
         "(In SMu Tiny [] (ACtor zero) [])"),
        ("U1-NT-M3", "test/fixtures/prelude/nattrans.mech",
         "(f.2, f.1)", 1, 0, "(f.1, f.2)",
         "PRELUDE-NATTRANS-FAIL wrong computation: leftValue = "
         "(In SMu Tiny [] (ACtor zero) [])"),
        # The tail of M4 and M5 names the functor of the index, so the two
        # kills cannot share a prefix and a different mismatch of the same
        # constructor does not count as a kill.
        ("U1-NT-M4", "prelude/cat/category.mech",
         "(naturality C D c d F G alpha x y f)", 2, 0, "categoryRefl",
         "PRELUDE-NATTRANS-FAIL mismatch: the constructor "
         "categoryRefl of MechCategory gives the index (Out SPi w _ "
         "(Out SPi 0 y D (APt 0 (Out SPi w _ C (APt w y) (Elim SPi w"
         " obj (Ran SPi w _ C D) G as self return (Ran SPi w _ C D) "
         "with "),
        ("U1-NT-M5", "prelude/cat/category.mech",
         "(compId D d (F.1 x) (F.1 y) (functorMap C D c d F x y f))", 1, 0,
         "categoryRefl",
         "PRELUDE-NATTRANS-FAIL mismatch: the constructor "
         "categoryRefl of MechCategory gives the index (Out SPi w _ "
         "(Out SPi 0 y D (APt 0 (Out SPi w _ C (APt w y) (Elim SPi w"
         " obj (Ran SPi w _ C D) F as self return (Ran SPi w _ C D) "
         "with "),
    ]
    reports = []
    for name, relative, old, anchors, occurrence, new, diagnostic in controls:
        path = copy / relative
        original = path.read_bytes()
        source = original.decode()
        pattern = r"\s+".join(map(re.escape, old.split()))
        found = list(re.finditer(pattern, source))
        chosen = next(islice(iter(found), occurrence, None), None)
        if len(found) != anchors or chosen is None:
            raise SystemExit(f"{name}: unexpected mutation anchor count")
        try:
            path.write_text(source[:chosen.start()] + new + source[chosen.end():])
            mutant_hash = hashlib.sha256(path.read_bytes()).hexdigest()
            result = suite(name)
            killed = (result.returncode == 1 and result.stderr == b""
                      and result.stdout.startswith(diagnostic.encode()))
            reports.append({"name": name, "path": relative, "old": old, "new": new,
                            "anchors": anchors, "occurrence": occurrence,
                            "replacements": 1, "diagnostic": diagnostic,
                            "mutant_sha256": mutant_hash, "exit_code": result.returncode,
                            "killed": killed})
            print(json.dumps({"control": name, "killed": killed}), flush=True)
        finally:
            path.write_bytes(original)
    restored = passed(suite("restored"))
    success = restored and all(row["killed"] for row in reports)
    report = {"passed": success, "restored": restored, "sources": sources,
              "checker_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "controls": reports}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"NATTRANS-MUTATIONS {'OK' if success else 'FAIL'} controls={len(reports)} "
          f"restored={int(restored)}")
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"NATTRANS-MUTATIONS FAIL {error}", file=sys.stderr)
        sys.exit(2)
