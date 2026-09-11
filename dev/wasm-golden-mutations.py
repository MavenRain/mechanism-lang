#!/usr/bin/env python3
"""Check golden comparisons and inventory refusal in an isolated copy."""
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/wasm-golden-mutations.py NEW_DIRECTORY")
        return 64
    source = Path(__file__).resolve().parent.parent
    work = Path(sys.argv[1]).resolve()
    if work == source or source in work.parents:
        print("golden control workspace must be outside the repository")
        return 64
    work.mkdir(parents=True, exist_ok=False)
    copy = work / "copy"
    (copy / "dev").mkdir(parents=True)
    shutil.copy2(source / "dev/wasm-gates.py", copy / "dev/wasm-gates.py")
    (copy / "dev/run-node.mjs").symlink_to(source / "dev/run-node.mjs")
    for name in ("_build", "vendor"):
        (copy / name).symlink_to(source / name, target_is_directory=True)
    shutil.copytree(source / "test/golden/wasm", copy / "test/golden/wasm")
    golden = copy / "test/golden/wasm/d06-closure-capture.wat"
    original = golden.read_bytes()
    results = []

    def run(label):
        result = subprocess.run([sys.executable, "-P", copy / "dev/wasm-gates.py"],
                                capture_output=True, text=True, timeout=60)
        (work / f"{label}.stdout").write_text(result.stdout)
        (work / f"{label}.stderr").write_text(result.stderr)
        return result

    golden.write_bytes(original + b"\n;; golden comparison control\n")
    result = run("changed")
    results.append({"id": "C-CLOS-G1", "killed": result.returncode == 1
                    and result.stderr == ""
                    and "EMIT d06-closure-capture FAIL: golden differs" in result.stdout
                    and "SUITE-WASM FAIL" in result.stdout})
    golden.unlink()
    result = run("missing")
    results.append({"id": "C-CLOS-G2", "killed": result.returncode == 1
                    and result.stderr == ""
                    and result.stdout == "SUITE-WASM FAIL golden overlay inventory\n"})
    golden.write_bytes(original)
    result = run("restored")
    restored = (result.returncode == 0 and result.stderr == ""
                and "SUITE-WASM OK\n" in result.stdout)
    report = {"controls": results, "restored": restored,
              "passed": restored and all(row["killed"] for row in results)}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"WASM-GOLDEN-MUTATIONS FAIL {error}", file=sys.stderr)
        sys.exit(2)
