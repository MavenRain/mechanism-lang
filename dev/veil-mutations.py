"""Build isolated Veil shape mutants and require the new gate legs to refuse.

Usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY

Each control edits the native Bend shape traversal in an isolated copy.
The complete hidden-shape metadata suite tests scope and specialization
for zk, fhc, mpc parties and access. Build failures never count as kills.
"""

import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
bounded_run = runpy.run_path(str(ROOT / "dev/bend2-process.py"))["run"]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY")
WORK = Path(sys.argv[1]).resolve()
if WORK.exists() or WORK == ROOT or ROOT in WORK.parents:
    raise SystemExit("the work directory must be new and outside the source repository")
WORK.mkdir(parents=True)
COPY = WORK / "copy"
shutil.copytree(ROOT, COPY, ignore=shutil.ignore_patterns(
    ".git", "_build", "_bend2", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs", "__pycache__"))
ENV = dict(os.environ)
BACKEND = ENV.get("BEND_MUTATION_BACKEND", ENV.get("BEND_TEST_BACKEND", "javascript"))

SOURCE = "bend2/surface/poly.bend"
CONTROLS = [
    ("C-VEIL-M1", SOURCE,
     "        access : Term.T <- map_term(action, access)\n",
     "",
     "drop the traversal of the second SMpc argument"),
    ("C-VEIL-M2", SOURCE,
     """    case Shape.SZk{q, name, domain}:
      do Budget.Comp<Shape.T<Term.T>>:
        domain : Term.T <- map_term(action, domain)
        return Shape.SZk{q, name, domain}""",
     """    case Shape.SZk{q, name, +domain}:
      do Budget.Comp<Shape.T<Term.T>>:
        mapped_domain : Term.T <- map_term(action, domain)
        return Shape.SZk{q, name, domain}""",
     "keep the old SZk payload, so the level substitution is lost"),
    ("C-VEIL-M3", SOURCE,
     """    case Shape.SFhc{level}:
      do Budget.Comp<Shape.T<Term.T>>:
        level : Term.T <- map_term(action, level)
        return Shape.SFhc{level}""",
     """    case Shape.SFhc{+level}:
      do Budget.Comp<Shape.T<Term.T>>:
        mapped_level : Term.T <- map_term(action, level)
        return Shape.SFhc{level}""",
     "keep the old SFhc payload, so a free universe stays inside the shape"),
]



def capture(name, command):
    result = bounded_run(command, cwd=COPY, env=ENV, capture_output=True,
                            timeout=900, check=False)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    result = capture(f"{name}-build", [sys.executable, "-P", "dev/bend2-mutation-build.py", "shape"])
    if result.returncode != 0 or b"BEND2 MUTATION BUILD PASS shape" not in result.stdout:
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")


def edit(path, old, new):
    target = COPY / path
    text = target.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"{path}: the control text is not present exactly once")
    target.write_text(text.replace(old, new))


def suite(name):
    if BACKEND == "native":
        return capture(name, [str(COPY / "_bend2/mutation/shape-native"), "--"])
    return capture(name, [os.environ.get("NODE", "node"), "--stack-size=16384",
                         str(COPY / "_bend2/mutation/shape.js"), "--"])


def control(name, path, old, new, note):
    original = (COPY / path).read_text()
    edit(path, old, new)
    try:
        build(name)
        result = suite(name)
        return {"control": name, "file": path, "suite": "surface_shape_metadata",
                "change": note, "exit_code": result.returncode,
                "stdout": result.stdout.decode(errors="replace").strip(),
                "killed": result.returncode == 1 and any(message in result.stderr for message in (
                    b"crypto shape payload",
                    b"universe: universe level is outside the global parameter scope"))}
    finally:
        (COPY / path).write_text(original)


def main():
    report = {"passed": False, "rows": [],
              "backend": BACKEND}
    try:
        build("C-VEIL-BASE")
        baseline = suite("baseline")
        if baseline.returncode != 0:
            return 1
        for row in CONTROLS:
            report["rows"].append(control(*row))
        build("C-VEIL-RESTORED")
        restored = suite("restored")
        report.update({"killed": sum(row["killed"] for row in report["rows"]),
                       "controls": len(report["rows"]),
                       "restored": restored.returncode == 0,
                       "passed": restored.returncode == 0 and
                       all(row["killed"] for row in report["rows"])})
        print(json.dumps({key: report[key] for key in ("passed", "killed", "controls")}))
        return 0 if report["passed"] else 1
    finally:
        (WORK / "report.json").write_text(json.dumps(report, indent=2) + "\n")


sys.exit(main())
