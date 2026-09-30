#!/usr/bin/env python3
"""Compile fresh by default; explicitly pinned native bundles permit fixture replay."""
import os
import hashlib
import json
from pathlib import Path
import runpy
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
ENTRIES = {"cli": "cli_core", "shape": "surface_shape_metadata"}
PROTOCOLS = ("category", "functor", "nattrans", "left-kan", "heterogeneous-functor",
             "composable-functors", "heterogeneous-nattrans", "heterogeneous-left-kan",
             "heterogeneous-whiskering", "left-kan-laws", "shared-left-kan",
             "shared-nattrans", "nattrans-laws", "whiskering-laws", "horizontal-laws",
             "iterated-whiskering", "horizontal-associativity", "nattrans-units")
ENTRIES.update({name: "prelude_" + name.replace("-", "_") for name in PROTOCOLS})
ENTRIES["template-cost"] = "template_cost"
PROTOCOL_GROUPS = tuple(PROTOCOLS[index * 4:index * 4 + 4] for index in range(4)) + (
    ("horizontal-associativity",), ("nattrans-units",))
PROTOCOL_BUNDLES = {mode: index for index, modes in enumerate(PROTOCOL_GROUPS) for mode in modes}
BUNDLE_ENTRIES = {"bend2/tests/relational_protocols.bend": PROTOCOLS}
BUNDLE_ENTRIES.update({f"bend2/tests/relational_native_{index}.bend": modes
                       for index, modes in enumerate(PROTOCOL_GROUPS)})
BUNDLE_ENTRIES["bend2/tests/cli_core.bend"] = ("cli",)


class BundleSourceChanged(RuntimeError):
    """A source mutant must compile its own executable."""


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def dependency_fingerprint(root, entry):
    """Reuse the production resolver, including Base and foreign effect imports."""
    root = Path(root).resolve()
    compiler = os.environ.get("BEND") or shutil.which("bend") or str(Path.home() / ".bend/bin/bend")
    compiler = Path(shutil.which(compiler) or compiler).resolve()
    base = compiler.parent.parent / "bend2/base.bend"
    resolver = runpy.run_path(str(root / "dev/bend2-build.py"))["source_graph"]
    return resolver(root / entry, base)


def validate_bundle(manifest, mode, root, compiler_sha256):
    """Validate data only. No manifest value is evaluated as a command."""
    if manifest.get("schema") != "bend-relational-bundle-v1":
        raise RuntimeError("unsupported bundle schema")
    if mode not in (*PROTOCOLS, "cli") or mode not in manifest.get("modes", []):
        raise RuntimeError("mode is not allowed by the pinned bundle")
    allowed_modes = BUNDLE_ENTRIES.get(manifest.get("entry"))
    if allowed_modes is None or mode not in allowed_modes:
        raise RuntimeError("unexpected bundle entry point")
    if any(name not in allowed_modes for name in manifest.get("modes", [])):
        raise RuntimeError("bundle mode does not belong to the selected entry point")
    if manifest.get("compiler_version") != "bend 2.0.27" or manifest.get("compiler_sha256") != compiler_sha256:
        raise RuntimeError("bundle compiler identity changed")
    cc = shutil.which(os.environ.get("CC", "clang"))
    if cc is None or manifest.get("native_compiler_sha256") != digest(cc):
        raise RuntimeError("bundle native compiler identity changed")
    builder = runpy.run_path(str(Path(root) / "dev/bend2-build.py"))
    if manifest.get("native_tool_identity") != builder["tool_identity"](Path(cc)):
        raise BundleSourceChanged("bundle selected native compiler changed; a fresh compilation is required")
    if manifest.get("source_provenance") != "before-and-after-generation":
        raise RuntimeError("bundle requires source pins before and after generation")
    if manifest.get("compiler_pin_provenance") != "before-and-after-generation-and-native-compilation":
        raise RuntimeError("bundle requires compiler pins before and after generation and native compilation")
    before = manifest.get("source_before")
    after = manifest.get("source_after")
    if not isinstance(before, dict) or not before or before != after:
        raise RuntimeError("bundle generation source guard failed")
    current = dependency_fingerprint(root, manifest["entry"])
    if current != before:
        raise BundleSourceChanged("bundle source graph changed; a fresh compilation is required")
    artifact = Path(manifest["artifact"])
    if not artifact.is_absolute():
        artifact = Path(root) / artifact
    if mode == "cli":
        allowed_artifacts = {Path(root) / "_bend2/mutation" / name
                             for name in ("cli-isolated-native", "cli-native")}
        if artifact.resolve() not in {path.resolve() for path in allowed_artifacts} or manifest["modes"] != ["cli"]:
            raise RuntimeError("native CLI bundle requires its fixed artifact path and sole CLI mode")
    if not artifact.is_file() or digest(artifact) != manifest.get("artifact_sha256"):
        raise RuntimeError("bundle artifact identity changed")
    return artifact.resolve()


def compiler_identity():
    bend = os.environ.get("BEND") or shutil.which("bend") or str(Path.home() / ".bend/bin/bend")
    bend = str(Path(shutil.which(bend) or bend).resolve())
    version = subprocess.run([bend, "version"], capture_output=True, text=True, check=True)
    if version.stdout.strip() != "bend 2.0.27":
        raise RuntimeError("mutation gates require Bend 2.0.27")
    return bend, version.stdout.strip(), digest(bend)


def packaged_cli_pin(manifest_path):
    """Reuse only the existing packaged JavaScript language CLI, with its guards."""
    manifest_path = Path(manifest_path).resolve()
    expected = (ROOT / "_bend2/bin/mechanism.js.build.json").resolve()
    if manifest_path != expected:
        raise RuntimeError("packaged CLI manifest must be the production JavaScript build stamp")
    metadata = json.loads(manifest_path.read_text())
    signature = metadata["signature"]
    bend, version, bend_hash = compiler_identity()
    if signature.get("backend") != "javascript" or signature.get("bend") != "2.0.27" or signature.get("compiler_sha256") != bend_hash:
        raise RuntimeError("packaged CLI toolchain or backend mismatch")
    if signature.get("inputs") != dependency_fingerprint(ROOT, "bend2/main.bend"):
        raise BundleSourceChanged("packaged CLI source changed; compile fresh")
    artifact = ROOT / "_bend2/bin/mechanism.js"
    if metadata.get("artifact_sha256") != digest(artifact):
        raise RuntimeError("packaged CLI artifact changed")
    launcher = ROOT / "_bend2/bin/mech.exe"
    # Derive the exact expected launcher from the same production writer.
    build = runpy.run_path(str(ROOT / "dev/bend2-build.py"))
    probe = ROOT / "_bend2/mutation" / f"cli-launcher-check-{os.getpid()}"
    probe.parent.mkdir(parents=True, exist_ok=True)
    try:
        build["launcher"](probe, artifact, "mech", "javascript")
        if launcher.read_bytes() != probe.read_bytes():
            raise RuntimeError("packaged CLI launcher is not the JavaScript mech entry")
    finally:
        probe.unlink(missing_ok=True)
    node = shutil.which(os.environ.get("NODE", "node"))
    if node is None:
        raise RuntimeError("packaged CLI needs Node")
    return {"backend": "javascript", "manifest": str(manifest_path), "manifest_sha256": digest(manifest_path),
            "artifact": str(artifact.relative_to(ROOT)), "artifact_sha256": digest(artifact),
            "packaged_launcher": str(launcher.relative_to(ROOT)), "packaged_launcher_sha256": digest(launcher),
            "compiler_sha256": bend_hash, "compiler_version": version,
            "node": str(Path(node).resolve()), "node_sha256": digest(node), "source_sha256": signature["inputs"]}


def packaged_cli_execute(pin_path, arguments):
    pin_path = Path(pin_path)
    pin_hash = digest(pin_path)
    expected = json.loads(pin_path.read_text())
    if packaged_cli_pin(expected["manifest"]) != expected:
        raise RuntimeError("packaged CLI inputs changed before execution")
    result = subprocess.run([str(ROOT / expected["packaged_launcher"]), *arguments], cwd=ROOT)
    if digest(pin_path) != pin_hash or packaged_cli_pin(expected["manifest"]) != expected:
        raise RuntimeError("packaged CLI inputs changed during execution")
    return result.returncode


def install_packaged_cli(manifest_path, evidence_path):
    pin = packaged_cli_pin(manifest_path)
    pin_path = ROOT / "_bend2/mutation/cli-packaged.pin.json"
    pin_path.write_text(json.dumps(pin, indent=2) + "\n")
    launcher = ROOT / "_bend2/mutation/cli-packaged.exe"
    launcher.write_text("#!/bin/sh\nset -eu\nexec " + shlex.quote(sys.executable) + " -I "
                        + shlex.quote(str(Path(__file__).resolve())) + " --packaged-cli-exec "
                        + shlex.quote(str(pin_path)) + ' "$@"\n')
    launcher.chmod(0o755)
    if packaged_cli_pin(manifest_path) != pin:
        raise RuntimeError("packaged CLI changed during guard installation")
    evidence = dict(pin, mode="cli", build_kind="explicit-pinned-packaged-cli",
                    launcher=str(launcher.relative_to(ROOT)), launcher_sha256=digest(launcher),
                    scope="fixture-only proof controls use the JavaScript shared language checker")
    evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def bundle_execute(manifest_path, mode, arguments):
    """Guard both sides of execution, including fixture-only mutation runs."""
    manifest_path = Path(manifest_path)
    manifest_hash = digest(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    bend, version, compiler_sha256 = compiler_identity()
    artifact = validate_bundle(manifest, mode, ROOT, compiler_sha256)
    command = [str(artifact), *arguments] if mode == "cli" else [str(artifact), mode, *arguments]
    result = subprocess.run(command, cwd=ROOT)
    if digest(manifest_path) != manifest_hash or digest(bend) != compiler_sha256:
        raise RuntimeError("bundle manifest or compiler changed during execution")
    validate_bundle(manifest, mode, ROOT, compiler_sha256)
    return result.returncode


def install_bundle(manifest_path, mode, output, launcher, compiler_sha256):
    manifest_path = Path(manifest_path).resolve()
    original_bytes = manifest_path.read_bytes()
    manifest = json.loads(original_bytes)
    artifact = validate_bundle(manifest, mode, ROOT, compiler_sha256)
    shutil.copyfile(artifact, output)
    output.chmod(0o755)
    if manifest_path.read_bytes() != original_bytes:
        output.unlink(missing_ok=True)
        raise RuntimeError("bundle manifest changed during installation")
    validate_bundle(manifest, mode, ROOT, compiler_sha256)
    installed = dict(manifest, artifact=str(output.relative_to(ROOT)))
    installed_path = output.with_suffix(".bundle.json")
    installed_path.write_text(json.dumps(installed, indent=2) + "\n")
    validate_bundle(installed, mode, ROOT, compiler_sha256)
    launcher.parent.mkdir(parents=True, exist_ok=True)
    launcher.write_text("#!/bin/sh\nset -eu\nexec " + shlex.quote(sys.executable)
                        + " -I " + shlex.quote(str(Path(__file__).resolve()))
                        + " --bundle-exec " + shlex.quote(str(installed_path))
                        + " " + shlex.quote(mode) + " \"$@\"\n")
    launcher.chmod(0o755)
    evidence = {"mode": mode, "backend": "native", "compiler_version": manifest["compiler_version"],
                "compiler_sha256": compiler_sha256, "artifact": str(output.relative_to(ROOT)),
                "artifact_sha256": manifest["artifact_sha256"], "source_sha256": manifest["source_before"],
                "build_kind": "explicit-pinned-bundle", "bundle_manifest_sha256": digest(installed_path),
                "bundle_source_provenance": manifest["source_provenance"],
                "compiler_pin_provenance": manifest.get("compiler_pin_provenance", "unspecified")}
    evidence["launcher"] = str(launcher.relative_to(ROOT))
    evidence["launcher_sha256"] = digest(launcher)
    output.with_suffix(".build.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def build_bundle(index):
    if index != "cli" and index not in range(len(PROTOCOL_GROUPS)):
        raise RuntimeError("invalid relational bundle shard")
    entry = "bend2/tests/cli_core.bend" if index == "cli" else f"bend2/tests/relational_native_{index}.bend"
    bend, version, bend_hash = compiler_identity()
    cc = shutil.which(os.environ.get("CC", "clang"))
    if cc is None:
        raise RuntimeError("native bundle requires clang")
    cc = str(Path(cc).resolve())
    cc_hash = digest(cc)
    builder = runpy.run_path(str(ROOT / "dev/bend2-build.py"))
    native_identity = builder["tool_identity"](Path(cc))
    before = dependency_fingerprint(ROOT, entry)
    name = "cli-isolated-native" if index == "cli" else f"relational-shard-{index}-native"
    output = ROOT / "_bend2/mutation" / name
    source = output.with_suffix(".c")
    manifest_path = output.with_suffix(".bundle.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    for old in (output, source, manifest_path):
        old.unlink(missing_ok=True)
    result = subprocess.run([bend, str(ROOT / entry), "-o", str(source)], cwd=ROOT)
    if result.returncode or not source.is_file():
        return result.returncode or 1
    if dependency_fingerprint(ROOT, entry) != before or digest(bend) != bend_hash:
        raise RuntimeError("bundle inputs changed during C generation")
    builder["verify_tool_identity"](native_identity)
    result = subprocess.run([cc, "-std=c11", "-O1", str(source), "-lpthread", "-lm", "-o", str(output)], cwd=ROOT)
    if result.returncode:
        return result.returncode
    after = dependency_fingerprint(ROOT, entry)
    builder["verify_tool_identity"](native_identity)
    if after != before or digest(bend) != bend_hash or digest(cc) != cc_hash:
        output.unlink(missing_ok=True)
        raise RuntimeError("bundle inputs or compilers changed during native compilation")
    manifest = {"schema": "bend-relational-bundle-v1", "entry": entry,
                "modes": list(BUNDLE_ENTRIES[entry]), "compiler_version": version,
                "compiler_sha256": bend_hash, "native_compiler_sha256": cc_hash,
                "native_tool_identity": native_identity,
                "source_provenance": "before-and-after-generation",
                "compiler_pin_provenance": "before-and-after-generation-and-native-compilation",
                "source_before": before, "source_after": after,
                "artifact": str(output.resolve()), "artifact_sha256": digest(output),
                "generated_c_sha256": digest(source), "native_flags": ["-std=c11", "-O1", "-lpthread", "-lm"]}
    validate_bundle(manifest, manifest["modes"][0], ROOT, bend_hash)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print("BEND2 NATIVE CLI BUNDLE PASS" if index == "cli" else "BEND2 RELATIONAL BUNDLE PASS " + str(index))
    print(str(manifest_path))
    return 0


def source_fingerprint():
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted((ROOT / "bend2").rglob("*"))
            if path.is_file() and path.suffix in (".bend", ".c", ".js")}


def mutation_backend(mode):
    explicit = os.environ.get("BEND_MUTATION_BACKEND")
    if explicit is not None:
        return explicit
    if mode == "cli" and os.environ.get("BEND_MUTATION_PACKAGED_CLI"):
        return "javascript"
    return os.environ.get("BEND_TEST_BACKEND", "javascript")


def compile_protocol(root, mode):
    """Compile before any baseline or mutant; a build failure aborts the gate."""
    bounded_run = runpy.run_path(str(root / "dev/bend2-process.py"))["run"]
    result = bounded_run([sys.executable, "-I", str(root / "dev/bend2-mutation-build.py"), mode],
                            cwd=root, capture_output=True, text=True, timeout=3600)
    if result.returncode or f"BEND2 MUTATION BUILD PASS {mode}" not in result.stdout:
        raise RuntimeError(f"{mode}: Bend protocol build failed, no mutation was classified\n"
                           + result.stdout + result.stderr)
    backend = mutation_backend(mode)
    suffix = ".build.json" if backend == "javascript" else "-native.build.json"
    return json.loads((root / "_bend2/mutation" / (mode + suffix)).read_text())


def main():
    if sys.argv[1:] == ["--build-native-cli"]:
        return build_bundle("cli")
    if len(sys.argv) >= 3 and sys.argv[1] == "--packaged-cli-exec":
        return packaged_cli_execute(sys.argv[2], sys.argv[3:])
    if len(sys.argv) == 3 and sys.argv[1] == "--build-bundle":
        return build_bundle(int(sys.argv[2]))
    if len(sys.argv) >= 4 and sys.argv[1] == "--bundle-exec":
        return bundle_execute(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) != 2 or sys.argv[1] not in ENTRIES:
        return 64
    mode = sys.argv[1]
    backend = mutation_backend(mode)
    if backend not in ("javascript", "native"):
        raise RuntimeError("BEND_MUTATION_BACKEND must be javascript or native")
    bend, version, compiler_sha256 = compiler_identity()
    before_sources = source_fingerprint()
    output = ROOT / "_bend2/mutation" / (mode + (".js" if backend == "javascript" else "-native"))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    native_cli = os.environ.get("BEND_MUTATION_NATIVE_CLI")
    packaged_cli = os.environ.get("BEND_MUTATION_PACKAGED_CLI")
    native_test_root = Path(os.environ.get("BEND_NATIVE_TEST_ROOT", ROOT)).resolve()
    automatic_cli = (mode == "cli" and backend == "native"
                     and os.environ.get("BEND_TEST_BACKEND") == "native"
                     and not native_cli and not packaged_cli)
    if automatic_cli:
        native_cli = str(ROOT / "_bend2/mutation/cli-isolated-native.bundle.json")
        if not Path(native_cli).is_file():
            if ROOT.resolve() == native_test_root:
                raise RuntimeError("native CLI acceptance artifact is missing; run make native-tests")
            code = build_bundle("cli")
            if code:
                return code
    if mode == "cli" and native_cli:
        if backend != "native":
            raise RuntimeError("BEND_MUTATION_NATIVE_CLI requires explicit native backend")
        expected_manifest = ROOT / "_bend2/mutation/cli-isolated-native.bundle.json"
        if Path(native_cli).resolve() != expected_manifest.resolve():
            raise RuntimeError("native CLI reuse requires the fixed isolated build manifest")
        launcher = ROOT / "_bend2/mutation/cli-native.exe"
        try:
            install_bundle(native_cli, mode, output, launcher, compiler_sha256)
        except BundleSourceChanged:
            print("BEND2 NATIVE CLI SOURCE CHANGED: compiling fresh in isolated output")
            code = build_bundle("cli")
            if code:
                return code
            install_bundle(native_cli, mode, output, launcher, compiler_sha256)
        print("BEND2 MUTATION BUILD PASS cli")
        print("BEND2 MUTATION BACKEND native (explicit isolated CLI)")
        return 0
    if mode == "cli" and packaged_cli:
        if backend != "javascript":
            raise RuntimeError("packaged CLI reuse requires JavaScript backend")
        try:
            install_packaged_cli(packaged_cli, output.with_suffix(".build.json"))
        except BundleSourceChanged:
            print("BEND2 MUTATION PACKAGED CLI SOURCE CHANGED: compiling fresh")
        else:
            print("BEND2 MUTATION BUILD PASS cli")
            print("BEND2 MUTATION BACKEND javascript (explicit packaged CLI)")
            return 0
    launcher = ROOT / "_bend2/bin/mech.exe"
    if mode in PROTOCOLS or mode == "template-cost":
        launcher = ROOT / "_bend2/test" / (ENTRIES[mode] + ".exe")
    installs_launcher = mode != "shape"
    if installs_launcher:
        launcher.unlink(missing_ok=True)
    bundle_path = os.environ.get("BEND_MUTATION_BUNDLE")
    if (not bundle_path and mode in PROTOCOLS and backend == "native"
            and os.environ.get("BEND_TEST_BACKEND") == "native"):
        index = PROTOCOL_BUNDLES[mode]
        bundle_path = str(native_test_root / "_bend2/mutation"
                          / f"relational-shard-{index}-native.bundle.json")
    if bundle_path and mode in PROTOCOLS:
        if backend != "native":
            raise RuntimeError("BEND_MUTATION_BUNDLE requires explicit native backend")
        try:
            install_bundle(bundle_path, mode, output, launcher, compiler_sha256)
        except BundleSourceChanged:
            output.unlink(missing_ok=True)
            launcher.unlink(missing_ok=True)
            print("BEND2 MUTATION BUNDLE SOURCE CHANGED: compiling fresh " + mode)
        else:
            if digest(bend) != compiler_sha256:
                output.unlink(missing_ok=True)
                launcher.unlink(missing_ok=True)
                raise RuntimeError("compiler changed during bundle installation")
            print("BEND2 MUTATION BUILD PASS " + mode)
            print("BEND2 MUTATION BACKEND native")
            print("BEND2 MUTATION PINNED BUNDLE")
            return 0
    source = output if backend == "javascript" else output.with_suffix(".c")
    source.unlink(missing_ok=True)
    result = subprocess.run([bend, str(ROOT / "bend2/tests" / (ENTRIES[mode] + ".bend")),
                             "-o", str(source)], cwd=ROOT)
    if result.returncode or not source.is_file():
        print(f"BEND2 MUTATION BUILD FAIL {mode} compiler_exit={result.returncode} "
              f"backend={backend} artifact={int(source.is_file())}", file=sys.stderr)
        return result.returncode or 1
    if backend == "native":
        result = subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-O1",
                                 str(source), "-lpthread", "-lm", "-o", str(output)], cwd=ROOT)
        if result.returncode:
            return result.returncode
    if source_fingerprint() != before_sources or hashlib.sha256(Path(bend).read_bytes()).hexdigest() != compiler_sha256:
        output.unlink(missing_ok=True)
        raise RuntimeError("source or compiler changed during the build; no mutation was classified")
    if installs_launcher:
        launcher.parent.mkdir(parents=True, exist_ok=True)
        stack = ('MUTATION_STACK_KB=$(ulimit -Hs)\n'
                 'if [ "$MUTATION_STACK_KB" = unlimited ] || [ "$MUTATION_STACK_KB" -gt 65536 ]; then\n'
                 '  MUTATION_STACK_KB=65536\nfi\n'
                 'ulimit -s "$MUTATION_STACK_KB"\n'
                 'MUTATION_NODE_STACK_KB=16384\n'
                 'if [ "$MUTATION_STACK_KB" -lt 32768 ]; then\n'
                 '  MUTATION_NODE_STACK_KB=$((MUTATION_STACK_KB / 2))\nfi\n')
        runner = '"${NODE:-node}" --stack-size="$MUTATION_NODE_STACK_KB" ' if backend == "javascript" else ""
        launcher.write_text("#!/bin/sh\nset -eu\n" + stack + "exec " + runner
                            + shlex.quote(str(output)) + " -- \"$@\"\n")
        launcher.chmod(0o755)
    evidence = {"mode": mode, "backend": backend, "compiler_version": version,
                "compiler_sha256": compiler_sha256,
                "artifact": str(output.relative_to(ROOT)),
                "artifact_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                "source_sha256": before_sources}
    output.with_suffix(".build.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("BEND2 MUTATION BUILD PASS " + mode)
    print("BEND2 MUTATION BACKEND " + backend)
    return 0


if __name__ == "__main__":
    sys.exit(main())
