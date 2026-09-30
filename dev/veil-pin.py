#!/usr/bin/env python3
"""Verify the exact pinned fixture snapshot and explicit WAT overlays."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
want, manifest_sha = sys.argv[2:4]
snapshot = root / "test/veil"

try:
    raw = (snapshot / "provenance.json").read_bytes()
    if hashlib.sha256(raw).hexdigest() != manifest_sha:
        raise ValueError("fixture provenance digest changed")
    manifest = json.loads(raw)
    pin = (root / "PIN").read_text().strip()
    if pin != want or manifest["commit"] != want:
        raise ValueError("fixture origin PIN changed")
    expected = manifest["files"]
    actual = {str(p.relative_to(snapshot)) for p in snapshot.rglob("*") if p.is_file()}
    if actual != set(expected) | {"README.md", "provenance.json"}:
        raise ValueError("fixture file inventory changed")
    for name, record in expected.items():
        contents = (snapshot / name).read_bytes()
        if len(contents) != record["bytes"] or hashlib.sha256(contents).hexdigest() != record["sha256"]:
            raise ValueError("fixture bytes changed: " + name)
        if record["kind"] == "overlay":
            original = root / record["path"]
            if original.read_bytes() != contents:
                raise ValueError("active overlay copy is stale: " + record["path"])
    print(f"PASS PIN pin={pin} assets={len(expected)} provenance={manifest_sha}")
except (OSError, ValueError, KeyError) as error:
    print(f"FAIL PIN {error}")
    sys.exit(1)
