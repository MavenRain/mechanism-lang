"""Run the kernel and frontend unit groups for the universe conversion slice."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
manifest = json.loads((ROOT / "dev/bend2/test-manifest.json").read_text())
suite = sys.argv[1]
if suite not in {"kernel", "surface"}:
    raise SystemExit("expected kernel or surface")
prefixes = ("kernel-", "levels-") if suite == "kernel" else ("surface-",)
records = []
for row in manifest["units"]:
    if not row["mode"].startswith(prefixes):
        continue
    started = time.monotonic()
    result = subprocess.run([ROOT / "_bend2/test/unit.exe", row["mode"]],
                            cwd=ROOT, capture_output=True, text=True, timeout=600)
    records.append(dict(mode=row["mode"], exit_code=result.returncode,
                        stdout=result.stdout, stderr=result.stderr,
                        seconds=round(time.monotonic() - started, 3),
                        passed=result.returncode == 0 and not result.stderr
                        and bool(result.stdout.strip())))
    (OUT / (suite + ".json")).write_text(json.dumps(records, indent=2) + "\n")
passed = sum(record["passed"] for record in records)
print(f"{suite}: {passed}/{len(records)} unit groups passed")
raise SystemExit(0 if records and passed == len(records) else 1)
