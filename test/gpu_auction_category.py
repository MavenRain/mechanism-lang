"""Check the auction map through the existing full category prelude."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    parts = ("prelude/init.mech", "prelude/cat/category.mech",
             "prelude/mechanism/second-price.mech", "examples/gpu-auction/category-bridge.mech")
    try:
        with tempfile.TemporaryDirectory(prefix="gpu-auction-category-") as temporary:
            source = Path(temporary) / "category.mech"
            source.write_text("\n".join((root / part).read_text() for part in parts))
            result = subprocess.run([str(root / "_build/default/bin/mech.exe"), "check", str(source)],
                                    capture_output=True, text=True, timeout=240, check=False)
            if result.returncode:
                print(result.stderr, file=sys.stderr)
                return 1
        print("GPU-AUCTION-CATEGORY PASS: category composition, allocation map, equality proofs")
        return 0
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"GPU-AUCTION-CATEGORY FAIL: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
