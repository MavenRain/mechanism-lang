#!/usr/bin/env python3
"""Build the Bend programs, keeping source and artifact fingerprints."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "_bend2"
VERSION = "2.0.27"
IMPORT = re.compile(r'^\s*import\s+(?:"([^"]+)"|([^\s]+))', re.MULTILINE)
PRODUCTION = {"mech": "mech", "mech_cert": "cert", "kanon": "kanon", "main": "kanon"}
COMPATIBILITY = {"prelude_runtime": "prelude-runtime", "wasm": "wasm-protocol",
                 "prelude": "prelude-protocol", "mapping": "mapping-protocol",
                 "import_json": "import-json",
                 "import_export": "import-export", "import_translate": "import-translate",
                 "import_pipeline": "import-pipeline", "surface_driver": "surface-parser",
                 "surface_check_driver": "surface-check", "kernel_erase_driver": "erase",
                 "io_driver": "io"}
SHARDS = ("kernel", "surface", "wasm", "import", "protocol")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def executable(variable: str, name: str, fallback: Path | None = None) -> Path:
    configured = os.environ.get(variable)
    found = (shutil.which(configured) or configured) if configured else (
        shutil.which(name) or (str(fallback) if fallback and fallback.is_file() else None))
    if found is None:
        raise RuntimeError(f"missing {name}; set {variable} to its executable path")
    return Path(found).expanduser().resolve()


def tool_identity(path: Path, env: dict | None = None) -> dict:
    """Pin the invoked entrypoint and Darwin developer-tool dispatch target.

    Keep invoking the entrypoint: a system shim can supply SDK/environment
    behavior that direct invocation of its selected binary would omit.
    """
    entry = Path(path).resolve()
    identity = {"version": 1, "entrypoint": str(entry),
                "entrypoint_sha256": digest(entry)}
    if sys.platform == "darwin" and entry.parent == Path("/usr/bin") and entry.name in {
            "clang", "clang++", "cc", "c++", "gcc", "g++", "make"}:
        environment = os.environ if env is None else env
        resolver = Path("/usr/bin/xcrun")
        resolver_hash = digest(resolver)
        result = subprocess.run([str(resolver), "--no-cache", "--find", entry.name],
                                env=environment, capture_output=True, text=True,
                                check=True, timeout=10)
        selected_text = result.stdout.strip()
        selected = Path(selected_text)
        if not selected_text or "\n" in selected_text or not selected.is_absolute():
            raise RuntimeError("invalid Darwin developer-tool resolution")
        selected = selected.resolve()
        if selected == entry or not selected.is_file() or not os.access(selected, os.X_OK):
            raise RuntimeError("Darwin developer-tool resolution did not select an executable")
        identity.update(selected_tool=str(selected), selected_tool_sha256=digest(selected),
                        resolver=str(resolver), resolver_sha256=resolver_hash,
                        selection_environment={key: environment.get(key) for key in
                            ("DEVELOPER_DIR", "TOOLCHAINS", "SDKROOT")})
        if digest(resolver) != resolver_hash or digest(entry) != identity["entrypoint_sha256"]:
            raise RuntimeError("Darwin developer-tool entrypoint changed during resolution")
    return identity


def verify_tool_identity(identity: dict, env: dict | None = None) -> None:
    if tool_identity(Path(identity["entrypoint"]), env) != identity:
        raise RuntimeError("selected native tool changed; rerun the build")


def source_graph(entry: Path, base: Path) -> dict[str, str]:
    pending, seen = [entry], set()
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        if not path.is_file():
            raise RuntimeError(f"missing build input: {path}")
        seen.add(path)
        if path.suffix == ".bend":
            for quoted, bare in IMPORT.findall(path.read_text()):
                name = quoted or bare
                dependency = base if name == "Base" else path.parent / name
                pending.append(dependency)
    return {str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path): digest(path)
            for path in sorted(seen)}


def production_sources() -> dict[str, str]:
    return {str(path.relative_to(ROOT)): digest(path)
            for path in sorted((ROOT / "bend2").rglob("*"))
            if path.is_file() and path.suffix in {".bend", ".c", ".js"}
            and "tests" not in path.relative_to(ROOT / "bend2").parts}


def atomic_text(path: Path, text: str, executable_file: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".tmp-{os.getpid()}")
    temporary.write_text(text)
    temporary.chmod(0o755 if executable_file else 0o644)
    temporary.replace(path)


def shell_word(text: str) -> str:
    return "'" + text.replace("'", "'\"'\"'") + "'"


def launcher(path: Path, artifact: Path, mode: str, backend: str) -> None:
    relative = artifact.relative_to(OUTPUT)
    root = 'MECHANISM_BUILD_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\n'
    stack = ('MECHANISM_STACK_KB=$(ulimit -Hs)\n'
             'if [ "$MECHANISM_STACK_KB" = unlimited ] || [ "$MECHANISM_STACK_KB" -gt 65536 ]; then\n'
             '  MECHANISM_STACK_KB=65536\nfi\n'
             'ulimit -s "$MECHANISM_STACK_KB"\n'
             'MECHANISM_NODE_STACK_KB=16384\n'
             'if [ "$MECHANISM_STACK_KB" -lt 32768 ]; then\n'
             '  MECHANISM_NODE_STACK_KB=$((MECHANISM_STACK_KB / 2))\nfi\n')
    program = '"$MECHANISM_BUILD_ROOT/' + str(relative) + '"'
    run = 'exec "${NODE:-node}" --stack-size="$MECHANISM_NODE_STACK_KB" ' + program if backend == "javascript" else "exec " + program
    atomic_text(path, "#!/bin/sh\nset -eu\n" + root + stack + run + " -- " + shell_word(mode) + ' "$@"\n', True)


def test_shard(kind: str, row: dict) -> str:
    mode = row["mode"]
    if "shard" in row:
        shard = row["shard"]
        if not isinstance(shard, str) or not re.fullmatch(r"[a-z0-9-]+", shard):
            raise RuntimeError(f"invalid test shard: {shard!r}")
        return shard
    if kind == "fixtures":
        return "wasm"
    if kind == "drivers":
        return "import" if mode.startswith("import-") else "protocol"
    if mode.startswith(("kernel-", "levels-")):
        return "kernel"
    if mode.startswith("surface-"):
        return "surface"
    if mode.startswith("wasm-"):
        return "wasm"
    raise RuntimeError(f"no test shard for {kind}: {mode}")


def test_entries(selected_mode: str | None = None) -> tuple[dict, dict[str, Path]]:
    manifest = json.loads((ROOT / "dev/bend2/test-manifest.json").read_text())
    entries = {}
    if selected_mode is None:
        shards = list(SHARDS)
        for kind in ("units", "fixtures", "drivers"):
            for row in manifest[kind]:
                shard = test_shard(kind, row)
                if shard not in shards:
                    shards.append(shard)
    else:
        matches = [(kind, row) for kind, prefix in
                   (("units", "unit-"), ("fixtures", "fixture-"), ("drivers", ""))
                   for row in manifest[kind] if prefix + row["mode"] == selected_mode]
        if len(matches) != 1:
            raise RuntimeError(f"expected one test mode, found {len(matches)}: {selected_mode!r}")
        kind, selected_row = matches[0]
        if not re.fullmatch(r"[a-z0-9-]+", selected_mode):
            raise RuntimeError(f"invalid test mode: {selected_mode!r}")
        shard = "single-" + selected_mode
        if any(test_shard(other_kind, row) == shard
               for other_kind in ("units", "fixtures", "drivers")
               for row in manifest[other_kind] if row is not selected_row):
            raise RuntimeError(f"single test shard conflicts with manifest: {shard}")
        selected_row["shard"] = shard
        shards = [shard]
    for shard in shards:
        path = OUTPUT / "test/entries" / (shard + ".bend")
        imports, branches = ["import Base"], []
        for kind in ("units", "fixtures", "drivers"):
            for row in manifest[kind]:
                mode = row["mode"]
                if not re.fullmatch(r"[a-z0-9-]+", mode):
                    raise RuntimeError(f"invalid test mode: {mode}")
                if test_shard(kind, row) != shard:
                    continue
                source = (ROOT / row["source"]).resolve()
                if not source.is_relative_to(ROOT / "bend2/tests"):
                    raise RuntimeError(f"test source is outside bend2/tests: {source}")
                alias = "Test" + str(len(imports))
                imports.append(f"import {os.path.relpath(source, path.parent)} as {alias}")
                if kind == "drivers":
                    entry = row["entry"]
                    if not re.fullmatch(r"[a-z_]+", entry):
                        raise RuntimeError(f"invalid driver entry: {entry}")
                    pattern = f'Con{{{json.dumps(mode)}, rest}}'
                    argument = "rest" if row.get("forward_args", True) else ""
                    action = f"{alias}.{entry}({argument})"
                else:
                    prefix = "unit-" if kind == "units" else "fixture-"
                    pattern = f'Con{{{json.dumps(prefix + mode)}, Nil{{}}}}'
                    action = f"{alias}.main()"
                    if kind == "units" and row["kind"] == "bool":
                        action = f"boolean({action}, {json.dumps(mode)})"
                branches.extend([f"    case {pattern}:", "      " + action])
        code = "\n".join(imports) + '''

def boolean(value: Bool, name: String) -> IO(Unit):
  match value:
    case True{}: IO.print("PASS " ++ name)
    case False{}: IO.die(Unit, 1, "FAIL " ++ name)

def arguments(args: List<String>) -> IO(Unit):
  match args:
''' + "\n".join(branches) + '''
    case other:
      IO.die(Unit, 64, "unknown test mode or arguments")

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    arguments(args)
'''
        if not path.exists() or path.read_text() != code:
            atomic_text(path, code)
        entries[shard] = path
    return manifest, entries


def test_router(kind: str, rows: list[dict]) -> None:
    lines = ["#!/bin/sh", "set -eu",
             'MECHANISM_TEST_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)',
             'case "${1-}" in']
    for row in rows:
        mode = row["mode"]
        lines.extend(["  " + shell_word(mode) + ")", "    shift",
                      f'    exec "$MECHANISM_TEST_ROOT/{kind}_{mode}.exe" "$@"', "    ;;"])
    lines.extend(['  *) printf "%s\\n" "unknown ' + kind + ' name" >&2; exit 64;;', "esac", ""])
    atomic_text(OUTPUT / "test" / (kind + ".exe"), "\n".join(lines), True)


def verify_build_inputs(entry: Path, base: Path, inputs: dict[str, str],
                        tool_hashes: dict[Path, str]) -> None:
    for tool, expected in tool_hashes.items():
        if digest(tool) != expected:
            raise RuntimeError(f"build tool changed during compilation: {tool}; rerun the build")
    if source_graph(entry, base) != inputs:
        raise RuntimeError("source changed during compilation; rerun the build")


def build(entry: Path, directory: str, name: str, bend: Path, base: Path,
          backend: str, compiler_hash: str, check: bool) -> Path:
    artifact = OUTPUT / directory / (name + (".js" if backend == "javascript" else "-native"))
    artifact.parent.mkdir(parents=True, exist_ok=True)
    if digest(bend) != compiler_hash:
        raise RuntimeError("Bend compiler changed before compilation; rerun the build")
    inputs = source_graph(entry, base)
    tool_hashes = {bend: compiler_hash}
    if check:
        subprocess.run([str(bend), str(entry), "--check-only"], check=True, cwd=ROOT)
        verify_build_inputs(entry, base, inputs, tool_hashes)
        return artifact
    signature = dict(version=1, bend=VERSION, compiler_sha256=compiler_hash,
                     backend=backend, optimization="O1" if backend == "native" else None,
                     inputs=inputs)
    if backend == "native":
        cc = executable("CC", "clang")
        tool_hashes[cc] = digest(cc)
        native_identity = tool_identity(cc)
        signature.update(c_compiler_sha256=tool_hashes[cc], host_platform=sys.platform,
                          host_machine=os.uname().machine, native_tool_identity=native_identity)
    stamp = artifact.with_name(artifact.name + ".build.json")
    if artifact.is_file() and stamp.is_file():
        previous = json.loads(stamp.read_text())
        if previous.get("signature") == signature and previous.get("artifact_sha256") == digest(artifact):
            verify_build_inputs(entry, base, inputs, tool_hashes)
            if backend == "native":
                verify_tool_identity(native_identity)
            print(f"current: {artifact.relative_to(ROOT)}", flush=True)
            return artifact
    temporary = artifact.with_name(name + f".tmp-{os.getpid()}" + (".js" if backend == "javascript" else ""))
    source = temporary if backend == "javascript" else Path(str(temporary) + ".c")
    print(f"compile: {entry.relative_to(ROOT)} ({backend})", flush=True)
    subprocess.run([str(bend), str(entry), "-o", str(source)], check=True, cwd=ROOT)
    if backend == "native":
        verify_build_inputs(entry, base, inputs, tool_hashes)
        verify_tool_identity(native_identity)
        subprocess.run([str(cc), "-std=c11", "-O1", str(source), "-lpthread", "-lm",
                        "-o", str(temporary)], check=True, cwd=ROOT)
    verify_build_inputs(entry, base, inputs, tool_hashes)
    if backend == "native":
        verify_tool_identity(native_identity)
    temporary.replace(artifact)
    atomic_text(stamp, json.dumps(dict(signature=signature, artifact_sha256=digest(artifact)), indent=2) + "\n")
    return artifact


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--backend", choices=["javascript", "native"], default="javascript")
    parser.add_argument("--target", choices=["all", "production", "tests"], default="all")
    parser.add_argument("--check", action="store_true")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--test-shard", help="build one shard from the test manifest")
    selection.add_argument("--test-mode", help="build one driver, unit-MODE, or fixture-MODE in its own shard")
    args = parser.parse_args()
    if (args.test_shard or args.test_mode) and args.target != "tests":
        parser.error("--test-shard and --test-mode require --target tests")
    bend = executable("BEND", "bend", Path.home() / ".bend/bin/bend")
    version = subprocess.run([str(bend), "version"], check=True, capture_output=True, text=True).stdout.strip()
    if not re.search(r"(?<![\d.])" + re.escape(VERSION) + r"(?![\d.])", version):
        raise RuntimeError(f"expected Bend {VERSION}, got {version!r}")
    base = (bend.parent.parent / "bend2/base.bend").resolve()
    compiler_hash = digest(bend)
    sources = production_sources()
    if args.target in {"all", "production"}:
        artifact = build(ROOT / "bend2/main.bend", "bin", "mechanism", bend, base, args.backend, compiler_hash, args.check)
        if not args.check:
            for name, mode in PRODUCTION.items():
                launcher(OUTPUT / "bin" / (name + ".exe"), artifact, mode, args.backend)
    if args.target in {"all", "tests"}:
        manifest, entries = test_entries(args.test_mode)
        if args.test_shard and args.test_shard not in entries:
            parser.error(f"unknown test shard: {args.test_shard}")
        artifacts = {}
        for shard, entry in entries.items():
            if args.test_shard and shard != args.test_shard:
                continue
            artifacts[shard] = build(entry, "test", "bend_" + shard, bend, base,
                                     args.backend, compiler_hash, args.check)
        if not args.check:
            compatibility = dict(COMPATIBILITY)
            drivers = {row["mode"]: row for row in manifest["drivers"]}
            for row in manifest["drivers"]:
                mode = row["mode"]
                shard = test_shard("drivers", row)
                if shard in artifacts:
                    launcher(OUTPUT / "test" / ("driver_" + mode + ".exe"),
                             artifacts[shard], mode, args.backend)
                for name in row.get("aliases", []):
                    if not re.fullmatch(r"[a-z0-9_-]+", name):
                        raise RuntimeError(f"invalid executable alias: {name!r}")
                    if name in compatibility and compatibility[name] != row["mode"]:
                        raise RuntimeError(f"conflicting executable alias: {name}")
                    compatibility[name] = row["mode"]
            for name, mode in compatibility.items():
                shard = test_shard("drivers", drivers[mode])
                if shard in artifacts:
                    launcher(OUTPUT / "test" / (name + ".exe"), artifacts[shard], mode, args.backend)
            for kind, prefix in (("units", "unit"), ("fixtures", "fixture")):
                for row in manifest[kind]:
                    mode = row["mode"]
                    shard = test_shard(kind, row)
                    if shard in artifacts:
                        launcher(OUTPUT / "test" / (prefix + "_" + mode + ".exe"),
                                 artifacts[shard], prefix + "-" + mode, args.backend)
                test_router(prefix, manifest[kind])
    if production_sources() != sources:
        raise RuntimeError("production source inventory changed during the build; rerun")
    if args.target in {"all", "production"} and not args.check:
        atomic_text(OUTPUT / "bend2-sources.sha256", "".join(f"{value}  {path}\n" for path, value in sources.items()))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Bend build failed: {error}", file=sys.stderr)
        sys.exit(1)
