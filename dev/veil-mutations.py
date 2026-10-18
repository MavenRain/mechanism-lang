"""Build isolated Veil shape mutants and require the new gate legs to refuse.

Usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY

Each control edits the C shape traversal (c/surface/poly.c) in an isolated copy.
The complete hidden-shape metadata suite tests scope and specialization
for zk, fhc, mpc parties and access. Build failures never count as kills.
"""

import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
bounded_run = runpy.run_path(str(ROOT / "dev/c-process.py"))["run"]
if len(sys.argv) != 2:
    raise SystemExit("usage: python3 -I dev/veil-mutations.py NEW_WORK_DIRECTORY")
WORK = Path(sys.argv[1]).resolve()
if WORK.exists() or WORK == ROOT or ROOT in WORK.parents:
    raise SystemExit("the work directory must be new and outside the source repository")
WORK.mkdir(parents=True)
COPY = WORK / "copy"
shutil.copytree(ROOT, COPY, ignore=shutil.ignore_patterns(
    ".git", "_build", "_bend2", "_tcc", ".gatework", ".kanon-exec", ".kanon-wait", ".kanon-replies", "build", "logs",
    "__pycache__"))
ENV = dict(os.environ)
TARGET = "_tcc/surface_shape_metadata"

# The C forms of the former Bend map_term controls (decision 279): each one drops or discards
# the same traversal result and keeps the control name and the kill messages.
SOURCE = "c/surface/poly.c"
CONTROLS = [
    ("C-VEIL-M1", SOURCE,
     "  return bind_frame(heap, poly_map_term(heap, frame->action, frame->shape.access), shape_body_mpc_k2, frame);\n",
     "  return shape_body_mpc_k2(heap, frame, (void *)frame->shape.access);\n",
     "drop the traversal of the second SMpc argument"),
    ("C-VEIL-M2", SOURCE,
     "shape_szk(frame->shape.quantity, frame->shape.name, value)",
     "shape_szk(frame->shape.quantity, frame->shape.name, (void *)frame->shape.domain)",
     "keep the old SZk payload, so the level substitution is lost"),
    ("C-VEIL-M3", SOURCE,
     "  (void)ctx;\n  return pure_shape(heap, shape_sfhc(value));\n",
     "  (void)value;\n  return pure_shape(heap, shape_sfhc((void *)((Frame *)ctx)->shape.level));\n",
     "keep the old SFhc payload, so a free universe stays inside the shape"),
]



def capture(name, command):
    result = bounded_run(command, cwd=COPY, env=ENV, capture_output=True,
                            timeout=900, check=False)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    return result


def build(name):
    # make -B: make 3.81 compares mtimes to the second, so an edit and its rebuild in the
    # same second would keep the stale object.
    binary = COPY / TARGET
    binary.unlink(missing_ok=True)
    result = capture(f"{name}-build", ["make", "-B", "-C", str(COPY), TARGET])
    if result.returncode != 0 or not os.access(binary, os.X_OK):
        raise SystemExit(f"{name}: build failed; this is not a killed mutation")
    return hashlib.sha256(binary.read_bytes()).hexdigest()


def edit(path, old, new):
    target = COPY / path
    text = target.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"{path}: the control text is not present exactly once")
    target.write_text(text.replace(old, new))


def suite(name):
    return capture(name, [str(COPY / TARGET)])


def control(name, path, old, new, note):
    original = (COPY / path).read_text()
    edit(path, old, new)
    try:
        binary_sha256 = build(name)
        result = suite(name)
        return {"control": name, "file": path, "suite": "surface_shape_metadata",
                "change": note, "exit_code": result.returncode, "binary_sha256": binary_sha256,
                "stdout": result.stdout.decode(errors="replace").strip(),
                "stderr": result.stderr.decode(errors="replace").strip(),
                "killed": result.returncode == 1 and any(message in result.stderr for message in (
                    b"crypto shape payload",
                    b"universe: universe level is outside the global parameter scope"))}
    finally:
        (COPY / path).write_text(original)


def main():
    report = {"passed": False, "rows": [], "target": TARGET}
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
