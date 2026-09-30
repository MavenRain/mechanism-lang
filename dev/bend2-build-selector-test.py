#!/usr/bin/env python3
"""Check isolated test entry generation and preservation of normal shards."""
from __future__ import annotations

import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    builder = runpy.run_path(str(ROOT / "dev/bend2-build.py"))
    generate = builder["test_entries"]
    namespace = generate.__globals__
    original = json.loads((ROOT / "dev/bend2/test-manifest.json").read_text())
    checks = 0
    with tempfile.TemporaryDirectory(prefix="bend2-selectors-") as temporary:
        root = Path(temporary).resolve()
        manifest_path = root / "dev/bend2/test-manifest.json"
        manifest_path.parent.mkdir(parents=True)
        manifest_path.write_text(json.dumps(original))
        namespace.update(ROOT=root, OUTPUT=root / "_bend2")
        manifest, entries = generate()
        baseline = {path: path.read_bytes() for path in entries.values()}
        for kind, prefix in (("units", "unit-"), ("fixtures", "fixture-"), ("drivers", "")):
            for row in original[kind]:
                mode = prefix + row["mode"]
                selected_manifest, selected = generate(mode)
                assert len(selected) == 1, mode
                entry = next(iter(selected.values()))
                code = entry.read_text()
                source = root / row["source"]
                assert code.count("import ") == 2, mode
                assert "import " + os.path.relpath(source, entry.parent) + " as Test1" in code, mode
                assert code.count("    case Con{") == 1, mode
                tail = "rest" if kind == "drivers" else "Nil{}"
                assert 'case Con{' + json.dumps(mode) + ', ' + tail + '}:' in code, mode
                if kind == "drivers":
                    argument = "rest" if row.get("forward_args", True) else ""
                    assert f'Test1.{row["entry"]}({argument})' in code, mode
                elif kind == "units" and row["kind"] == "bool":
                    assert 'boolean(Test1.main(), ' + json.dumps(row["mode"]) + ')' in code, mode
                else:
                    assert "      Test1.main()" in code, mode
                assert selected_manifest.keys() == manifest.keys(), mode
                checks += 1
        assert all(path.read_bytes() == content for path, content in baseline.items())
        restored_manifest, restored = generate()
        assert restored_manifest == original
        assert {path: path.read_bytes() for path in restored.values()} == baseline
        for mode in ("unit-no-such-test", "../unit-kernel", "fixture-no-such-test"):
            try:
                generate(mode)
            except RuntimeError:
                checks += 1
            else:
                raise AssertionError(f"accepted unknown test mode: {mode}")
        duplicate = json.loads(json.dumps(original))
        duplicate["units"].append(duplicate["units"][0].copy())
        manifest_path.write_text(json.dumps(duplicate))
        try:
            generate("unit-" + duplicate["units"][0]["mode"])
        except RuntimeError:
            checks += 1
        else:
            raise AssertionError("accepted an ambiguous test mode")
        conflict = json.loads(json.dumps(original))
        first_mode = "unit-" + conflict["units"][0]["mode"]
        conflict["units"][1]["shard"] = "single-" + first_mode
        manifest_path.write_text(json.dumps(conflict))
        try:
            generate(first_mode)
        except RuntimeError:
            checks += 1
        else:
            raise AssertionError("accepted an isolated shard collision")
    for arguments in (("--target", "production", "--test-mode", "surface-check"),
                      ("--target", "tests", "--test-mode", "surface-check", "--test-shard", "surface")):
        result = subprocess.run([sys.executable, "-I", str(ROOT / "dev/bend2-build.py"), *arguments],
                                capture_output=True, text=True)
        assert result.returncode == 2, result.stderr
        checks += 1
    print(f"BEND2 BUILD SELECTORS PASS cases={checks}")


if __name__ == "__main__":
    main()
