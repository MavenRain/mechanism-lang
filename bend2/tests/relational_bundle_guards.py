#!/usr/bin/env python3
"""Exercise pinned-bundle guards with an explicitly synthetic executable fixture."""
import copy
import json
from pathlib import Path
import runpy
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
helper = runpy.run_path(str(ROOT / "dev/bend2-mutation-build.py"))
validate = helper["validate_bundle"]
digest = helper["digest"]
bend, version, compiler_hash = helper["compiler_identity"]()
passed = []


def refused(label, action, expected=RuntimeError):
    try:
        action()
    except expected:
        passed.append(label)
    else:
        raise AssertionError(label + " was accepted")


with tempfile.TemporaryDirectory(prefix="bend-bundle-guards-") as directory:
    root = Path(directory)
    (root / "dev").mkdir()
    shutil.copyfile(ROOT / "dev/bend2-build.py", root / "dev/bend2-build.py")
    tests = root / "bend2/tests"
    tests.mkdir(parents=True)
    entry = tests / "relational_protocols.bend"
    entry.write_text("import Base\nimport ./dependency.bend as Dep\n")
    dependency = tests / "dependency.bend"
    dependency.write_text('import Base\n@foreign:\n  import "./effect.c"\n')
    effect = tests / "effect.c"
    effect.write_text("/* controlled foreign dependency */\n")
    artifact = root / "synthetic-fixture"
    artifact.write_text("#!/bin/sh\nexit 0\n")
    artifact.chmod(0o755)
    graph = helper["dependency_fingerprint"](root, "bend2/tests/relational_protocols.bend")
    assert any(path.endswith("base.bend") for path in graph)
    assert "bend2/tests/effect.c" in graph
    passed.append("complete Base and foreign graph")
    manifest = {"schema": "bend-relational-bundle-v1", "modes": ["category"],
                "entry": "bend2/tests/relational_protocols.bend", "compiler_version": version,
                "compiler_sha256": compiler_hash, "native_compiler_sha256": digest(shutil.which("clang")),
                "source_provenance": "before-and-after-generation", "source_before": graph,
                "source_after": dict(graph), "artifact": str(artifact), "artifact_sha256": digest(artifact),
                "compiler_pin_provenance": "before-and-after-generation-and-native-compilation",
                "fixture_scope": "synthetic test fixture, not a compiled protocol"}
    assert validate(manifest, "category", root, compiler_hash) == artifact.resolve()
    passed.append("valid controlled fixture")
    refused("mode whitelist", lambda: validate(manifest, "cli", root, compiler_hash))
    refused("unlisted protocol", lambda: validate(manifest, "functor", root, compiler_hash))
    refused("compiler identity", lambda: validate(manifest, "category", root, "0" * 64))
    bad = copy.deepcopy(manifest)
    bad["source_provenance"] = "observed-after-build"
    refused("post-only source pins", lambda: validate(bad, "category", root, compiler_hash))
    bad = copy.deepcopy(manifest)
    bad["compiler_pin_provenance"] = "observed-after-build"
    refused("post-only compiler pins", lambda: validate(bad, "category", root, compiler_hash))
    bad = copy.deepcopy(manifest)
    bad["source_after"]["bend2/tests/dependency.bend"] = "0" * 64
    refused("pre/post mismatch", lambda: validate(bad, "category", root, compiler_hash))
    original = dependency.read_text()
    dependency.write_text(original + "# source mutant\n")
    refused("source mutant requires fresh build", lambda: validate(manifest, "category", root, compiler_hash),
            helper["BundleSourceChanged"])
    dependency.write_text(original)
    effect.write_text("/* stale foreign effect */\n")
    refused("foreign source mutant requires fresh build", lambda: validate(manifest, "category", root, compiler_hash),
            helper["BundleSourceChanged"])
    effect.write_text("/* controlled foreign dependency */\n")
    original_artifact = artifact.read_text()
    artifact.write_text(original_artifact + "# corrupt artifact\n")
    refused("artifact corruption", lambda: validate(manifest, "category", root, compiler_hash))
    artifact.write_text(original_artifact)
    bad = copy.deepcopy(manifest)
    bad["native_compiler_sha256"] = "0" * 64
    refused("native compiler identity", lambda: validate(bad, "category", root, compiler_hash))
    # The executable intentionally edits its own dependency after the pre-run guard.
    artifact.write_text("#!/bin/sh\nprintf '# changed during execution\\n' >> bend2/tests/dependency.bend\n")
    manifest["artifact_sha256"] = digest(artifact)
    manifest_path = root / "bundle.json"
    manifest_path.write_text(json.dumps(manifest))
    execute = helper["bundle_execute"]
    old_root = execute.__globals__["ROOT"]
    execute.__globals__["ROOT"] = root
    try:
        refused("post-execution source guard", lambda: execute(manifest_path, "category", []),
                helper["BundleSourceChanged"])
    finally:
        execute.__globals__["ROOT"] = old_root

    # Native CLI reuse has a separate fixed entry, artifact and sole mode.
    dependency.write_text(original)
    cli_entry = tests / "cli_core.bend"
    cli_entry.write_text("import Base\nimport ./dependency.bend as Dep\n")
    cli_artifact = root / "_bend2/mutation/cli-isolated-native"
    cli_artifact.parent.mkdir(parents=True)
    cli_artifact.write_text("#!/bin/sh\nexit 0\n")
    cli_artifact.chmod(0o755)
    cli_graph = helper["dependency_fingerprint"](root, "bend2/tests/cli_core.bend")
    cli = dict(manifest, entry="bend2/tests/cli_core.bend", modes=["cli"],
               source_before=cli_graph, source_after=dict(cli_graph),
               artifact=str(cli_artifact), artifact_sha256=digest(cli_artifact))
    assert validate(cli, "cli", root, compiler_hash) == cli_artifact.resolve()
    passed.append("valid isolated native CLI")
    bad = dict(cli, artifact=str(artifact), artifact_sha256=digest(artifact))
    refused("native CLI fixed artifact", lambda: validate(bad, "cli", root, compiler_hash))
    bad = dict(cli, modes=["cli", "category"])
    refused("native CLI sole mode", lambda: validate(bad, "cli", root, compiler_hash))
    refused("native CLI cannot run relational mode", lambda: validate(cli, "category", root, compiler_hash))

report = {"status": "pass", "assertions": passed,
          "scope": "guard-only synthetic executable controls, not protocol or mutation kills"}
(ROOT / "build/relational-bundle-guards.json").write_text(json.dumps(report, indent=2) + "\n")
print("RELATIONAL-BUNDLE-GUARDS-OK assertions=" + str(len(passed)))
