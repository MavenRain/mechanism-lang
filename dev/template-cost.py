"""Compare C template checking in two trees with identical inputs.

Poll counts and monotonic elapsed milliseconds are measured by the C driver.
OCaml GC-word allocation and process CPU counters have no C driver counterpart here.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
bounded_run = runpy.run_path(str(ROOT / "dev/c-process.py"))["run"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


EXECUTABLE = Path("_tcc/template_cost")


def build_tree(env, root):
    """Build the benchmark driver in one tree, so the timing uses its sources."""
    executable = root / EXECUTABLE
    executable.unlink(missing_ok=True)
    result = bounded_run(["make", "-C", str(root), str(EXECUTABLE)], cwd=root,
                            env=env, capture_output=True, text=True, timeout=3600)
    if result.returncode or not os.access(executable, os.X_OK):
        raise SystemExit(f"the driver does not build cleanly in {root}")
    return {"target": str(EXECUTABLE), "exit_code": result.returncode,
            "artifact": str(EXECUTABLE), "artifact_sha256": digest(executable)}


def tree_state(env, root, executable):
    """Name the tree that produced one executable: base commit and worktree state."""
    def git(*arguments):
        return subprocess.run(["git", "-C", str(root), *arguments], env=env,
                              capture_output=True, text=True, timeout=120,
                              check=True).stdout.strip()
    return {"root": str(root), "base": git("rev-parse", "HEAD"),
            "status": git("status", "--porcelain"),
            "executable_sha256": digest(root / executable)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--runs", type=int, default=2)
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    if baseline == ROOT or not 1 <= args.runs <= 10:
        parser.error("use a separate baseline tree and between 1 and 10 runs")
    if args.output.exists():
        parser.error("the output must be a new file")

    paths = ["prelude/cat/category.mech", "c/tests/template_cost.c"]
    for path in paths:
        if digest(baseline / path) != digest(ROOT / path):
            parser.error(f"the two trees have different benchmark input: {path}")
    executable = EXECUTABLE
    env = dict(os.environ)
    env.pop("OPAM_SWITCH_PREFIX", None)
    env.pop("CAML_LD_LIBRARY_PATH", None)
    trees = [("before", baseline), ("after", ROOT)]
    builds = {}
    for label, root in trees:
        builds[label] = build_tree(env, root)
        if not (root / executable).is_file():
            parser.error(f"the benchmark driver is missing in {root}")

    report = {
        "runtime": "C compiled with tcc",
        "unsupported_metrics": ["allocated_words", "cpu_seconds"],
        "source_sha256": digest(ROOT / paths[0]),
        "driver_sha256": digest(ROOT / paths[1]),
        "builds": builds,
        "executables": {label: digest(root / executable) for label, root in trees},
        "trees": {label: tree_state(env, root, executable) for label, root in trees},
        "runs": [],
    }
    scope_members = {}
    for scope, suffix in [("functors", ["--through", "compFunctor"]), ("full", [])]:
        for repetition in range(args.runs):
            order = [("before", baseline), ("after", ROOT)]
            if repetition % 2:
                order.reverse()
            for label, root in order:
                command = [str(root / executable), str(root / paths[0]), *suffix]
                start = time.monotonic()
                result = bounded_run(command, cwd=root, env=env, capture_output=True,
                                        text=True, timeout=600, check=True)
                measured = json.loads(result.stdout)
                if measured.get("checked") is not True:
                    raise ValueError("the driver did not report a successful check")
                if measured.get("allocated_words") is not None or measured.get("cpu_seconds") is not None:
                    raise ValueError("unsupported runtime metrics must remain explicitly null")
                if type(measured.get("polls")) is not int or measured["polls"] < 0:
                    raise ValueError("the driver did not report an exact nonnegative poll count")
                if type(measured.get("elapsed_milliseconds")) is not int or measured["elapsed_milliseconds"] < 0:
                    raise ValueError("the driver did not report monotonic elapsed milliseconds")
                members = measured.get("members")
                expected = scope_members.setdefault(scope, members)
                if type(members) is not int or members != expected:
                    raise ValueError(f"the {scope} scope checked {members} members"
                                     f" against {expected} in the other tree")
                row = {"scope": scope, "repetition": repetition + 1,
                       "compiler": label, "wall_seconds": time.monotonic() - start,
                       **measured}
                report["runs"].append(row)
                print(json.dumps(row, sort_keys=True), flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as output:
        json.dump(report, output, indent=2, sort_keys=True)
        output.write("\n")


if __name__ == "__main__":
    main()
