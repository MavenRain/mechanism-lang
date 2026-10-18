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
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="rust-let-") as directory:
        driver = args.driver.resolve() if args.driver else ROOT / "_tcc/rust_let"
        if args.driver is None:
            subprocess.run(
                ["make", "_tcc/rust_let"], cwd=ROOT,
                check=True, capture_output=True, text=True,
            )
        prefix = ["node", "--stack-size=65536"] if driver.suffix == ".js" else []
        result = subprocess.run(
            [*prefix, str(driver)],
            check=True, capture_output=True, text=True,
        )
        print(result.stdout, end="")
        if result.stdout.splitlines() != EXPECTED:
            raise SystemExit("RUST-LET-FAIL")
        print("RUST-LET-OK 5")


if __name__ == "__main__":
    main()
