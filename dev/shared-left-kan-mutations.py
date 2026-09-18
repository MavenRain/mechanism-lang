"""Replay source controls in a small isolated fixture tree without rebuilding."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OK = "PRELUDE-SHARED-LEFT-KAN-OK entries=944 families=13 computations=6 negatives=7\n"
FAIL = "PRELUDE-SHARED-LEFT-KAN-FAIL "
TEMPLATE = "prelude/cat/shared-left-kan.mech"
RUNTIME = "test/fixtures/prelude/shared-left-kan-runtime.mech"
NEGATIVE = "test/neg/shared-left-kan/nominal-category.mech"
CONTROLS = [
    ("sharing", TEMPLATE,
     "Base_Base_Middle := Base_Middle,\n          Base_Base_Target := Base_Target",
     "Base_Base_Middle := Base_Middle",
     "mismatch: the term has type (Lan SPi w hom (Ran SPi 0 x D (Ran SPi 0 y D Type (u5 + 1)))"),
    ("unit-accessor", TEMPLATE, ":= Lan_lanUnit", ":= Lan_lanFunctor",
     "mismatch: the term has type (Ran SPi 0 J Type (u0 + 1) (Ran SPi 0 C Type (u2 + 1) (Ran SPi 0 D Type (u4 + 1)"),
    ("cocone-component", RUNTIME, "natAdd (natAdd n x) 4", "natAdd (natAdd n x) 5",
     "wrong computation: lanDescSecond"),
    ("vertical-order", RUNTIME,
     "Constant Constant Constant Mediator OtherMediator",
     "Constant Constant Constant OtherMediator Mediator", "wrong computation: lanVerticalFirst"),
    ("refusal-control", NEGATIVE, None, "def bad : Nat := 0\n",
     "nominal-category: expected refusal"),
]
FILES = ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
         "prelude/cat/composable-functors.mech", "prelude/cat/heterogeneous-nattrans.mech",
         "prelude/cat/heterogeneous-whiskering.mech", "prelude/cat/shared-nattrans.mech",
         "prelude/cat/heterogeneous-left-kan.mech", TEMPLATE,
         "test/fixtures/prelude/shared-left-kan.mech", RUNTIME]


def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/shared-left-kan-mutations.py NEW-WORK-DIRECTORY", file=sys.stderr)
        return 64
    work = Path(sys.argv[1]).resolve()
    if work == ROOT or work.is_relative_to(ROOT) or work.exists():
        raise ValueError("choose a fresh work directory outside the repository")
    work.mkdir(parents=True)
    copy = work / "copy"
    files = FILES + [str(file.relative_to(ROOT)) for file in
                    sorted((ROOT / "test/neg/shared-left-kan").iterdir()) if file.is_file()]
    for name in files:
        destination = copy / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, destination)
    exe = ROOT / "_build/default/test/prelude_shared_left_kan.exe"
    hashes = {name: digest(copy / name) for name in files}
    rows = []

    def run(label):
        started = time.monotonic()
        command = [str(exe), str(copy)]
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=840)
        (work / f"{label}.stdout").write_text(result.stdout)
        (work / f"{label}.stderr").write_text(result.stderr)
        row = {"id": label, "command": ["_build/default/test/prelude_shared_left_kan.exe", "COPY"],
               "exit": result.returncode, "seconds": round(time.monotonic() - started, 3),
               "stdout_sha256": digest(work / f"{label}.stdout"),
               "stderr_sha256": digest(work / f"{label}.stderr")}
        rows.append(row)
        return result, row

    baseline, baseline_row = run("baseline")
    baseline_row["passed"] = baseline.returncode == 0 and baseline.stdout == OK and not baseline.stderr
    if not baseline_row["passed"]:
        raise ValueError("baseline did not pass; see baseline.stdout and baseline.stderr")
    for label, name, before, after, expected in CONTROLS:
        target = copy / name
        original = target.read_text()
        if before is not None and original.count(before) != 1:
            raise ValueError(f"{label}: mutation anchor is not unique")
        try:
            target.write_text(after if before is None else original.replace(before, after))
            mutation_sha256 = digest(target)
            result, row = run(label)
            row.update({"path": name, "before_sha256": hashes[name],
                        "mutated_sha256": mutation_sha256, "expected": FAIL + expected,
                        "killed": result.returncode == 1 and not result.stderr
                        and result.stdout.startswith(FAIL + expected)})
            print(f"{label}: killed={row['killed']}", flush=True)
        finally:
            target.write_text(original)
    restored, restored_row = run("restored")
    restored_row["passed"] = restored.returncode == 0 and restored.stdout == OK and not restored.stderr
    unchanged = all(digest(copy / name) == sha and digest(ROOT / name) == sha
                    for name, sha in hashes.items())
    killed = sum(row.get("killed", False) for row in rows)
    passed = baseline_row["passed"] and restored_row["passed"] and unchanged and killed == len(CONTROLS)
    report = {"passed": passed, "killed": killed, "controls": len(CONTROLS),
              "sources_unchanged": unchanged, "source_sha256": hashes,
              "suite_source_sha256": digest(ROOT / "test/prelude_shared_left_kan.ml"),
              "executable_sha256": digest(exe), "rows": rows}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": passed, "killed": killed, "controls": len(CONTROLS)}))
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"SHARED-LEFT-KAN-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
