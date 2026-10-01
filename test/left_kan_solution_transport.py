"""Check solution transport laws, independent universes and refusals."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MARKER = "PRELUDE-LEFT-KAN-SOLUTION-TRANSPORT-OK laws=2 contracts=2 computations=8 negatives=6"

def main():
    subprocess.run([sys.executable, "-I", ROOT / "dev/left-kan-cocone-congruence-focus.py",
                    "--solution-transport", "--contracts"], cwd=ROOT, check=True,
                   capture_output=True, text=True, timeout=30)
    result = subprocess.run([ROOT / "_bend2/test/prelude_left_kan_solution_transport.exe", ROOT],
                            cwd=ROOT, capture_output=True, text=True, timeout=840)
    if result.returncode or result.stderr or result.stdout != MARKER + "\n":
        print(f"PRELUDE-LEFT-KAN-SOLUTION-TRANSPORT FAIL exit={result.returncode} "
              f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
        return 1
    print(MARKER)
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError) as error:
        print(f"PRELUDE-LEFT-KAN-SOLUTION-TRANSPORT FAIL {error}")
        raise SystemExit(1)
