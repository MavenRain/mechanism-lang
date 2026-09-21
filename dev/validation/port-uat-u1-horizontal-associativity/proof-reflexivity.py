"""Regenerate the deliberately invalid reflexivity proof for this slice."""
from pathlib import Path
import sys


def main():
    if len(sys.argv) != 2:
        print("usage: proof-reflexivity.py OUTPUT", file=sys.stderr)
        return 64
    root = Path(__file__).resolve().parents[3]
    category = root / "prelude/cat"
    dependencies = ["category-core", "heterogeneous-functor", "composable-functors",
                    "heterogeneous-nattrans", "heterogeneous-whiskering", "shared-nattrans"]
    prefix = "\n".join((category / f"{name}.mech").read_text() for name in dependencies)
    source = (category / "horizontal-associativity.mech").read_text()
    head, marker, tail = source.partition("    let f :")
    if not marker or not tail.endswith("end\n") or "def hcompAssoc" not in head:
        print("proof-reflexivity: source marker changed", file=sys.stderr)
        return 1
    Path(sys.argv[1]).write_text(prefix + "\n" + head + "    categoryRefl\nend\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OSError as error:
        print(f"proof-reflexivity: {error}", file=sys.stderr)
        raise SystemExit(1)
