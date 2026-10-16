"""Check cocone equality laws, independent universes and five refusals."""
from pathlib import Path
import os
import subprocess
import sys
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# Hop 329 (USER ruling 1 on the memcap-blind python legs): when MEMCAP names the
# memcap wrapper, the driver runs as `MEMCAP 3800 <secs> driver...`, so the
# 3800 MB cap sits on the driver and not only on this python process.  The
# wrapper prints one `memcap cmd=...` record on stdout after the driver exits;
# uncapped moves those lines to stderr so the stdout checks stay byte for byte.
MEMCAP = os.environ.get("MEMCAP")

def capped(command, seconds):
    prefix = [MEMCAP, "3800", str(seconds)] if MEMCAP else []
    return [str(part) for part in prefix + list(command)]

def uncapped(result):
    lines = result.stdout.splitlines(keepends=True)
    sys.stderr.write("".join(line for line in lines if line.startswith("memcap cmd=")))
    result.stdout = "".join(line for line in lines if not line.startswith("memcap cmd="))
    return result
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
        result = uncapped(subprocess.run(capped([ROOT / "_tcc/prelude_left_kan_laws", work, "--cocone-equality"], 840),
                                         cwd=ROOT, capture_output=True, text=True, timeout=840))
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
