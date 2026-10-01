"""Check cocone equality laws, independent universes and five refusals."""
from pathlib import Path
import subprocess
import sys
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MARKER = "PRELUDE-LEFT-KAN-COCONE-EQUALITY-OK laws=3 contracts=3 computations=6 negatives=5"

def main():
    subprocess.run([sys.executable, "-I", ROOT / "dev/left-kan-cocone-congruence-focus.py",
                    "--cocone-equality", "--contracts"], cwd=ROOT, check=True,
                   capture_output=True, text=True, timeout=30)
    with tempfile.TemporaryDirectory(prefix="mechanism-cocone-equality-") as directory:
        work = Path(directory)
        source = work / "_bend2/left-kan-cocone-equality-contracts/focused-source.mech"
        source.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / "_bend2/left-kan-cocone-equality-contracts/focused-source.mech", source)
        negatives = work / "test/neg/left-kan-laws"
        negatives.mkdir(parents=True)
        for path in (ROOT / "test/neg/left-kan-laws").glob("cocone-eq-*"):
            shutil.copyfile(path, negatives / path.name)
        result = subprocess.run([ROOT / "_bend2/test/prelude_left_kan_laws.exe", work, "--cocone-equality"],
                                cwd=ROOT, capture_output=True, text=True, timeout=840)
    if result.returncode or result.stderr or result.stdout != MARKER + "\n":
        print(f"PRELUDE-LEFT-KAN-COCONE-EQUALITY FAIL exit={result.returncode} "
              f"stdout={result.stdout[:1500]!r} stderr={result.stderr[:1500]!r}")
        return 1
    print(MARKER)
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError) as error:
        print(f"PRELUDE-LEFT-KAN-COCONE-EQUALITY FAIL {error}")
        raise SystemExit(1)
