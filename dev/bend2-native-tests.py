#!/usr/bin/env python3
"""Prepare the explicit native acceptance drivers before timed test legs."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import runpy
import shutil

ROOT = Path(__file__).resolve().parents[1]
TEMPLATES = ("surface-template-reuse", "surface-template-symbolic-reuse")
NATIVE_DRIVERS = (*TEMPLATES, "prelude-runtime", "surface-check",
                  "import-protocol", "import-types-protocol")

def validate_group(mutation: dict, data: dict, modes: tuple[str, ...], compiler_hash: str) -> None:
    if data.get("modes") != list(modes):
        raise RuntimeError("native bundle mode inventory changed")
    mutation["validate_bundle"](data, modes[0], ROOT, compiler_hash)


def prepare(*, rebuild=False):
    builder = runpy.run_path(str(ROOT / "dev/bend2-build.py"))
    mutation = runpy.run_path(str(ROOT / "dev/bend2-mutation-build.py"))
    digest = builder["digest"]
    inputs = builder["production_sources"]()
    setup_paths = [ROOT / path for path in (
        "dev/bend2-native-tests.py", "dev/bend2-build.py",
        "dev/bend2-mutation-build.py", "dev/bend2-mutation.py",
        "dev/bend2/test-manifest.json")]
    setup_pins = {path: digest(path) for path in setup_paths}
    bend, version, compiler_hash = mutation["compiler_identity"]()
    bend = Path(bend)
    base = (bend.parent.parent / "bend2/base.bend").resolve()
    cc = builder["executable"]("CC", "clang")
    tool_pins = {bend: compiler_hash, cc: digest(cc)}
    manifest, entries = builder["test_entries"]()
    rows = {row["mode"]: row for row in manifest["drivers"]}
    bundles = []

    for index in (*range(len(mutation["PROTOCOL_GROUPS"])), "cli"):
        name = "cli-isolated-native" if index == "cli" else f"relational-shard-{index}-native"
        path = ROOT / "_bend2/mutation" / (name + ".bundle.json")
        modes = ("cli",) if index == "cli" else mutation["PROTOCOL_GROUPS"][index]
        needs_build = rebuild or not path.is_file()
        if not needs_build:
            data = json.loads(path.read_text())
            try:
                validate_group(mutation, data, modes, compiler_hash)
            except mutation["BundleSourceChanged"]:
                needs_build = True
        if needs_build and mutation["build_bundle"](index):
            raise RuntimeError(f"native acceptance bundle failed: {name}")
        data = json.loads(path.read_text())
        validate_group(mutation, data, modes, compiler_hash)
        bundles.append((path, digest(path), data, modes))

    templates = {}
    template_pins = {}
    for mode in NATIVE_DRIVERS:
        shard = builder["test_shard"]("drivers", rows[mode])
        artifact = builder["build"](
            entries[shard], "test", "bend_" + shard, bend, base,
            "native", compiler_hash, False)
        templates[mode] = artifact
        stamp = artifact.with_name(artifact.name + ".build.json")
        template_pins[mode] = (entries[shard], artifact, digest(artifact), stamp,
                               digest(stamp), json.loads(stamp.read_text())["signature"]["inputs"])

    cache = Path(os.environ.get("BEND_MUTATION_CACHE", ROOT / "build/bend2-native-mutations")).resolve()
    frontend = runpy.run_path(str(ROOT / "dev/bend2-mutation.py"))
    mutation_receipts = frontend["prepare_native"](
        ROOT, [rows[mode]["source"] for mode in TEMPLATES], cache)

    published_pins = {}

    def verify_setup():
        frontend["verify_prepared_native"](ROOT, mutation_receipts)
        if builder["production_sources"]() != inputs:
            raise RuntimeError("production sources changed during native acceptance setup")
        if any(digest(path) != pin for path, pin in (setup_pins | tool_pins).items()):
            raise RuntimeError("native acceptance setup inputs changed; rerun the build")
        for entry, artifact, artifact_hash, stamp, stamp_hash, sources in template_pins.values():
            if digest(artifact) != artifact_hash or digest(stamp) != stamp_hash:
                raise RuntimeError("native template artifact changed during acceptance setup")
            builder["verify_build_inputs"](entry, base, sources, tool_pins)
        for path, manifest_hash, data, modes in bundles:
            if digest(path) != manifest_hash:
                raise RuntimeError("native bundle manifest changed during acceptance setup")
            validate_group(mutation, data, modes, compiler_hash)
        if any(digest(path) != pin for path, pin in published_pins.items()):
            raise RuntimeError("published native acceptance artifact changed during setup")

    verify_setup()
    records = []
    for path, manifest_hash, data, modes in bundles:
        for mode in modes:
            if mode == "cli":
                output = ROOT / "_bend2/mutation/cli-native"
                aliases = [ROOT / "_bend2/mutation/cli-native.exe"]
            else:
                row = rows["prelude-" + mode]
                output = ROOT / "_bend2/mutation" / (mode + "-native")
                names = ["driver_" + row["mode"], *row.get("aliases", [])]
                primary = mutation["ENTRIES"][mode]
                if primary not in names:
                    raise RuntimeError("native protocol lacks its mutation launcher alias")
                # Both setup paths must record the same primary launcher.
                names = [primary, *(name for name in names if name != primary)]
                if any(not re.fullmatch(r"[a-z0-9_-]+", name) for name in names):
                    raise RuntimeError("invalid native protocol alias")
                aliases = [ROOT / "_bend2/test" / (name + ".exe") for name in names]
            mutation["install_bundle"](path, mode, output, aliases[0], compiler_hash)
            for alias in aliases[1:]:
                shutil.copyfile(aliases[0], alias)
                alias.chmod(0o755)
            published_pins[output] = data["artifact_sha256"]
            for published in (output.with_suffix(".bundle.json"), output.with_suffix(".build.json"), *aliases):
                published_pins[published] = digest(published)
            records.append(dict(mode=mode, backend="native", bundle=str(path.relative_to(ROOT)),
                                bundle_sha256=digest(path), artifact_sha256=data["artifact_sha256"],
                                aliases={str(alias.relative_to(ROOT)): digest(alias) for alias in aliases}))

    for mode, artifact in templates.items():
        row = rows[mode]
        aliases = ["driver_" + mode, *row.get("aliases", [])]
        for name in aliases:
            if not re.fullmatch(r"[a-z0-9_-]+", name):
                raise RuntimeError("invalid native template alias")
            builder["launcher"](ROOT / "_bend2/test" / (name + ".exe"), artifact, mode, "native")
            alias = ROOT / "_bend2/test" / (name + ".exe")
            published_pins[alias] = digest(alias)
        stamp = artifact.with_name(artifact.name + ".build.json")
        records.append(dict(mode=mode, backend="native", artifact=str(artifact.relative_to(ROOT)),
                            artifact_sha256=digest(artifact), stamp_sha256=digest(stamp)))
    verify_setup()
    receipt = dict(version=1, backend="native", compiler_version=version,
                   compiler_sha256=compiler_hash, protocols=records,
                   setup_pins={str(path.relative_to(ROOT)): pin for path, pin in setup_pins.items()},
                   published_pins={str(path.relative_to(ROOT)): pin for path, pin in published_pins.items()},
                   mutation_cache=str(cache), mutation_receipts=mutation_receipts)
    receipt_path = ROOT / "build/bend2-native-tests.json"
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"BEND2 NATIVE TESTS PASS protocols={len(records) - 1} cli=1", flush=True)
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rebuild", action="store_true", help="explicitly regenerate all native bundles")
    args = parser.parse_args()
    raise SystemExit(prepare(rebuild=args.rebuild))
