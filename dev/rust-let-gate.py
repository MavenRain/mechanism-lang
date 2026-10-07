#!/usr/bin/env python3
"""Check direct let RIR lowering before the Rust lifter supports lets."""

import argparse
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
EXPECTED = [
    "PASS variable initializer",
    "PASS call initializer",
    "PASS nullary initializer",
    "PASS different result type",
    "PASS initializer type checking",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path)
    parser.add_argument("--bend", type=Path, default=Path.home() / ".bend/bin/bend")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rust-let-") as directory:
        driver = args.driver or Path(directory) / "rust-let.js"
        if args.driver is None:
            subprocess.run(
                [str(args.bend),
                 str(ROOT / "bend2/tests/rust_let.bend"), "-o", str(driver)],
                check=True, capture_output=True, text=True,
            )
        result = subprocess.run(
            ["node", "--stack-size=65536", str(driver)],
            check=True, capture_output=True, text=True,
        )
        print(result.stdout, end="")
        if result.stdout.splitlines() != EXPECTED:
            raise SystemExit("RUST-LET-FAIL")
        print("RUST-LET-OK 5")


if __name__ == "__main__":
    main()
