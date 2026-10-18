#!/usr/bin/env python3
"""Exercise filesystem effects and atomic report publication."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", type=Path, default=ROOT / "build/io-driver.js")
    args = parser.parse_args()
    driver = args.driver.resolve()
    command = (["node"] if driver.suffix == ".js" else []) + [str(driver)]
    checks = 0

    def invoke(*arguments: str, success: bool = True) -> subprocess.CompletedProcess[str]:
        nonlocal checks
        result = subprocess.run([*command, *arguments], capture_output=True, text=True, timeout=30)
        assert (result.returncode == 0) == success, (arguments, result.returncode, result.stderr)
        checks += 1
        return result

    with tempfile.TemporaryDirectory(prefix="mechanism-io-test-") as temporary:
        root = Path(temporary)
        file = root / "quoted ' file.txt"
        contents = ["", "hello\nworld\n", "a" * 65535 + "λ😀" * 20000, "before\x00after"]
        for content in contents:
            file.write_text(content)
            assert invoke("read", str(file)).stdout == content + "\n"
        invoke("read", str(root / "missing"), success=False)
        invoke("read", str(root), success=False)
        invoke("write", str(file), "Unicode λ😀\n")
        assert file.read_text() == "Unicode λ😀\n"
        invoke("bytes", str(file))
        assert file.read_bytes() == bytes([0, 255, 128, 10])
        result = invoke("temp")
        temp = Path(result.stdout.rstrip("\n"))
        assert temp.is_file() and temp.name.startswith("mechanism-io-") and temp.suffix == ".wasm"
        temp.unlink()
        output = root / "report"
        invoke("publish", str(output) + "///")
        assert (output / "first.json").read_text() == '{"value":42}\n'
        assert (output / "second.tsv").read_text() == "name\tvalue\n"
        invoke("publish", str(output), success=False)
        assert (output / "first.json").read_text() == '{"value":42}\n'
        empty = root / "empty"
        empty.mkdir()
        invoke("publish", str(empty), success=False)
        assert empty.is_dir() and list(empty.iterdir()) == []
        invoke("publish", str(file), success=False)
        assert file.read_bytes() == bytes([0, 255, 128, 10])
        failed = root / "failed"
        invoke("publish-fail", str(failed), success=False)
        assert not failed.exists() and not list(root.glob("failed.tmp-*"))
        invoke("publish", str(root / "absent" / "report"), success=False)
        assert not list(root.glob("*.tmp-*"))
    print(json.dumps(dict(checks=checks, driver=str(driver), failed=0)))


if __name__ == "__main__":
    main()
