"""Build isolated Veil shape mutants and require the new gate legs to refuse.

Usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY

Each control edits one line of lib/rules.ml in a copy outside this
repository, builds that copy, and runs the suite of the leg that must
kill the mutant: VEIL-TEMPLATES (test/veil_templates.exe) or
VEIL-CIRCUIT (test/circuit_bounds.exe). A control is killed when its
suite exits non-zero. The primary source is never mutated.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY")
WORK = Path(sys.argv[1]).resolve()
if WORK.exists() or WORK == ROOT or ROOT in WORK.parents:
    raise SystemExit("the work directory must be new and outside the source repository")
WORK.mkdir(parents=True)
COPY = WORK / "copy"
shutil.copytree(ROOT, COPY, ignore=shutil.ignore_patterns(
    ".git", "_build", ".gatework", ".kanon-exec", ".kanon-wait", "__pycache__"))
ENV = dict(os.environ)
ENV.pop("OPAM_SWITCH_PREFIX", None)
ENV.pop("CAML_LD_LIBRARY_PATH", None)

CONTROLS = [
    ("C-VEIL-M1", "lib/rules.ml",
     "      let* p' = f p in\n      let* a' = f a in\n      Ok (Shape.SMpc (p', a'))",
     "      let* p' = f p in\n      Ok (Shape.SMpc (p', a))",
     "veil_templates.exe",
     "drop the traversal of the second SMpc argument"),
    ("C-VEIL-M2", "lib/rules.ml",
     "  | Shape.SZk (q, w, ty) -> Result.map (fun v -> Shape.SZk (q, w, v)) (f ty)",
     "  | Shape.SZk (q, w, ty) -> Result.map (fun _ -> Shape.SZk (q, w, ty)) (f ty)",
     "veil_templates.exe",
     "keep the old SZk payload, so the level substitution is lost"),
    ("C-VEIL-M3", "lib/rules.ml",
     "  | Shape.SFhc l -> Result.map (fun v -> Shape.SFhc v) (f l)",
     "  | Shape.SFhc l -> Result.map (fun _ -> Shape.SFhc l) (f l)",
     "circuit_bounds.exe",
     "keep the old SFhc payload, so a free universe stays inside the shape"),
]


def capture(name, command):
    result = subprocess.run(command, cwd=COPY, env=ENV, capture_output=True,
                            timeout=900, check=False)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    result = capture(f"{name}-build", ["zsh", str(COPY / "dev/dunecho.sh"), "build"])
    if result.returncode != 0 or b"0 errors, 0 warnings" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def edit(path, old, new):
    target = COPY / path
    text = target.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"{path}: the control text is not present exactly once")
    target.write_text(text.replace(old, new))


def control(name, path, old, new, suite, note):
    edit(path, old, new)
    build(name)
    result = capture(name, [str(COPY / "_build/default/test" / suite)])
    edit(path, new, old)
    return {"control": name, "file": path, "suite": suite, "change": note,
            "exit_code": result.returncode,
            "stdout": result.stdout.decode(errors="replace").strip(),
            "killed": result.returncode != 0}


def main():
    build("C-VEIL-BASE")
    rows = [control(*row) for row in CONTROLS]
    build("C-VEIL-RESTORED")
    restored = [capture("restored-templates",
                        [str(COPY / "_build/default/test/veil_templates.exe")]),
                capture("restored-circuit",
                        [str(COPY / "_build/default/test/circuit_bounds.exe")])]
    report = {"passed": all(row["killed"] for row in rows),
              "killed": sum(1 for row in rows if row["killed"]),
              "controls": len(rows), "rows": rows,
              "restored": [item.stdout.decode(errors="replace").strip().splitlines()[-1]
                           for item in restored if item.stdout]}
    (WORK / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps({key: report[key] for key in ("passed", "killed", "controls")}))
    return 0 if report["passed"] else 1


sys.exit(main())
