"""Run the scoped auction checks and optionally a real NVIDIA GPU round trip."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gpu", action="store_true", help="require a live cuOpt solve, never skip if unavailable")
    parser.add_argument("--category", action="store_true", help="include the slower category integration proof")
    parser.add_argument("--schemas", action="store_true", help="check JSON schemas (install .[test])")
    parser.add_argument("--record", type=Path, help="write a JSON validation record")
    args = parser.parse_args()
    checks = [("combinatorial", "test/combinatorial.py", 120),
              ("kernel-certificates", "test/combinatorial_kernel.py", 300),
              ("second-price", "test/gpu_auction.py", 180)]
    if args.category:
        checks.append(("category", "test/gpu_auction_category.py", 900))
    if args.schemas:
        checks.append(("schemas", "test/combinatorial_schema.py", 60))
    results = []
    for name, script, timeout in checks:
        started = time.monotonic()
        try:
            result = subprocess.run([sys.executable, "-P", str(ROOT / script)], cwd=ROOT,
                                    timeout=timeout, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
        results.append({"name": name, "exit_code": code, "seconds": round(time.monotonic() - started, 3)})
        print(f"{'PASS' if code == 0 else 'FAIL'} {name}", flush=True)
    if args.gpu:
        from mechanism_cuopt.artifacts import audit_settlement, export_auction, solve_and_publish
        from mechanism_cuopt.model import load_auction
        try:
            with tempfile.TemporaryDirectory(prefix="mech-cuopt-live-") as temporary:
                artifact, output = Path(temporary) / "model", Path(temporary) / "settlement"
                export_auction(load_auction(ROOT / "examples/combinatorial/three-bidders.json"), artifact, ROOT)
                result = solve_and_publish(artifact, output, "cuopt", ROOT)
                audit_settlement(artifact, output, ROOT)
                if result["payments"] != {"A": 0, "B": 5, "C": 3} or result["welfare"] != 16:
                    raise ValueError("live result disagrees with the hand-solved case")
                results.append({"name": "live-cuopt", "exit_code": 0, "payments": result["payments"]})
                print("PASS live-cuopt", flush=True)
        except (OSError, ValueError, RuntimeError, ImportError) as error:
            print(f"FAIL live-cuopt: {error}", file=sys.stderr, flush=True)
            results.append({"name": "live-cuopt", "exit_code": 1, "reason": str(error)})
    report = {"version": 1, "checks": results, "live_gpu_requested": args.gpu,
              "passed": all(result["exit_code"] == 0 for result in results)}
    if args.record:
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
