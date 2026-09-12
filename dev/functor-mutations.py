"""Replay source controls with the current built functor checker."""
import hashlib
import json
import os
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
        raise SystemExit("usage: python3 -I dev/functor-mutations.py NEW_DIRECTORY")
    work = Path(sys.argv[1]).resolve()
    if work.exists() or work == ROOT or ROOT in work.parents:
        raise SystemExit("the output directory must be new and outside the repository")
    executable = ROOT / "_build/default/test/prelude_functor.exe"
    if not executable.is_file():
        raise SystemExit("build the functor checker before replay")
    copy = work / "source"
    files = [ROOT / "prelude/cat/category.mech", ROOT / "test/fixtures/prelude/functor.mech"]
    files.extend(sorted((ROOT / "test/neg/functor").iterdir()))
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
                and result.stdout.startswith(b"PRELUDE-FUNCTOR-OK entries="))

    if not passed(suite("baseline")):
        raise SystemExit("baseline failed; no control ran")
    controls = [
        ("U1-F-M1", "test/fixtures/prelude/functor.mech",
         "Uniform Uniform Uniform NextObj ZeroObj)", "Uniform Uniform Uniform ZeroObj NextObj)",
         "PRELUDE-FUNCTOR-FAIL wrong computation: objectForward ="),
        ("U1-F-M2", "test/fixtures/prelude/functor.mech",
         "DoubleEnd DoubleEnd DoubleEnd Swap CopyFirst)",
         "DoubleEnd DoubleEnd DoubleEnd CopyFirst Swap)",
         "PRELUDE-FUNCTOR-FAIL wrong computation: mapForward ="),
        ("U1-F-M3", "prelude/cat/category.mech", "=> F.2.2.2", "=> F.2.2.1",
         "PRELUDE-FUNCTOR-FAIL mismatch: the term has type"),
        ("U1-F-M4", "prelude/cat/category.mech",
         "(functorMapId D E d e G (F.1 x))", "categoryRefl",
         "PRELUDE-FUNCTOR-FAIL mismatch: the constructor categoryRefl"),
    ]
    reports = []
    for name, relative, old, new, diagnostic in controls:
        path = copy / relative
        original = path.read_bytes()
        source = original.decode()
        # M2 names both the simple and the nested composition on purpose.
        expected_count = 2 if name == "U1-F-M2" else 1
        if source.count(old) != expected_count:
            raise SystemExit(f"{name}: unexpected mutation anchor count")
        try:
            path.write_text(source.replace(old, new))
            mutant_hash = hashlib.sha256(path.read_bytes()).hexdigest()
            result = suite(name)
            killed = (result.returncode == 1 and result.stderr == b""
                      and result.stdout.startswith(diagnostic.encode()))
            reports.append({"name": name, "path": relative, "old": old, "new": new,
                            "replacements": expected_count, "diagnostic": diagnostic,
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
    print(f"FUNCTOR-MUTATIONS {'OK' if success else 'FAIL'} controls={len(reports)} "
          f"restored={int(restored)}")
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"FUNCTOR-MUTATIONS FAIL {error}", file=sys.stderr)
        sys.exit(2)
