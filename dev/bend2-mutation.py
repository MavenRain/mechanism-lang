"""Compile isolated Bend mutation protocols and require their intended refusal."""

import hashlib
import json
import os
from pathlib import Path
import runpy
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import fcntl


bounded_run = runpy.run_path(str(Path(__file__).with_name("bend2-process.py")))["run"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def executable(configured):
    return Path(shutil.which(configured) or configured).expanduser().resolve()


def file_pins(paths):
    return {str(path): sha(Path(path)) for path in sorted(set(map(str, paths)))}


def pins_match(pins):
    return all(Path(path).is_file() and sha(Path(path)) == digest for path, digest in pins.items())


def dependency_paths(capture, root):
    tokens = shlex.split(Path(capture).read_text().replace("\\\n", " "))[1:]
    return [Path(token) if Path(token).is_absolute() else Path(root) / token for token in tokens]


class NativeCache:
    """Explicit native compilation shared by preparation and isolated controls."""

    def __init__(self, root, directory, bend, env):
        self.root, self.directory, self.bend, self.env = Path(root).resolve(), Path(directory).resolve(), Path(bend), env
        self.directory.mkdir(parents=True, exist_ok=True)
        self.compiler = executable(env.get("CC", "/usr/bin/clang"))
        self.base = self.bend.parent.parent / "bend2/base.bend"
        builder = runpy.run_path(str(self.root / "dev/bend2-build.py"))
        self.resolver = builder["source_graph"]
        self.native_tool_identity = builder["tool_identity"]
        self.flags = ["-std=c11", "-O1"]
        identity_tools = [self.bend, self.compiler]
        sdk = None
        if sys.platform == "darwin":
            xcrun = executable("xcrun")
            identity_tools.append(xcrun)
            sdk = subprocess.check_output([str(xcrun), "--show-sdk-path"], env=env, text=True).strip()
            self.flags += ["-isysroot", sdk]
        version = subprocess.check_output([str(self.bend), "version"], env=env, text=True).strip()
        if version != "bend 2.0.27":
            raise SystemExit(f"expected bend 2.0.27, got {version}")
        self.identity = {"backend": "native", "bend": str(self.bend), "bend_version": version,
                         "native_tool_identity": self.native_tool_identity(self.compiler, env),
                         "compiler": str(self.compiler), "compiler_version": subprocess.check_output(
                             [str(self.compiler), "--version"], env=env, text=True).strip(),
                         "sdk": sdk, "flags": self.flags, "link_flags": ["-lpthread", "-lm"],
                         "tools": file_pins(identity_tools),
                         "environment": {key: env.get(key) for key in ["SDKROOT", "CPATH", "C_INCLUDE_PATH",
                             "LIBRARY_PATH", "MACOSX_DEPLOYMENT_TARGET"]}}

    def graph(self, source):
        return self.resolver(self.root / source, self.base)

    def valid(self, receipt, source):
        try:
            c_source = Path(receipt["c_source"])
            capture = c_source.parent / "headers.stdout"
            dependencies = receipt["dependencies"]
            headers = receipt["headers"]
            preserved = json.loads((c_source.parent / "receipt.json").read_text())
            if (receipt["format"] != 2 or preserved != receipt or not headers or
                    dependencies["capture"] != str(capture) or sha(capture) != dependencies["sha256"]):
                return False
            paths = dependency_paths(capture, dependencies["root"])
            if str(c_source) not in headers or set(map(str, paths)) != set(headers):
                return False
            # A prepared entry can be used in an isolated source copy. Check
            # project headers against that copy as well as the original pins.
            original_root = Path(dependencies["root"])
            copied_headers = {
                str(self.root / Path(path).relative_to(original_root)): digest
                for path, digest in headers.items()
                if Path(path).is_relative_to(original_root) and not Path(path).is_relative_to(c_source.parent)
            }
            return (receipt["identity"] == self.identity and self.tools_match() and
                    receipt["inputs"] == self.graph(source) and pins_match(headers) and pins_match(copied_headers) and
                    all(Path(receipt[key]).is_file() and sha(Path(receipt[key])) == receipt[key + "_sha256"]
                        for key in ["artifact", "c_source"]))
        except (OSError, ValueError, KeyError, TypeError):
            return False

    def tools_match(self):
        return (pins_match(self.identity["tools"]) and
                self.native_tool_identity(self.compiler, self.env) == self.identity["native_tool_identity"])

    def build(self, source, *, timeout=3600):
        if type(timeout) is not int or timeout <= 0:
            raise SystemExit("native build timeout must be a positive integer")
        deadline = time.monotonic() + timeout
        inputs = self.graph(source)
        if not self.tools_match():
            raise SystemExit("native compiler changed during mutation validation")
        signature = {"format": 2, "inputs": inputs, "identity": self.identity}
        key = hashlib.sha256(json.dumps(signature, sort_keys=True).encode()).hexdigest()
        index = self.directory / (key + ".json")
        with (self.directory / (key + ".lock")).open("a") as lock:
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise SystemExit("native cache lock timeout; this is not a killed mutation")
                try:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    time.sleep(min(0.05, remaining))
            if index.is_file():
                try:
                    old = json.loads(index.read_text())
                    if self.valid(old, source):
                        return old, True
                except (OSError, ValueError, KeyError, TypeError):
                    pass
            output = Path(tempfile.mkdtemp(prefix=key + ".", dir=self.directory))
            c_source, artifact = output / "program.c", output / "program"
            commands = []

            def command(label, arguments):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise SystemExit("native build timeout; this is not a killed mutation")
                result = bounded_run(arguments, cwd=self.root, env=self.env, capture_output=True, timeout=remaining)
                (output / (label + ".stdout")).write_bytes(result.stdout)
                (output / (label + ".stderr")).write_bytes(result.stderr)
                commands.append({"label": label, "command": arguments, "exit_code": result.returncode,
                                 "stdout_sha256": sha(output / (label + ".stdout")),
                                 "stderr_sha256": sha(output / (label + ".stderr"))})
                if result.returncode != 0:
                    raise SystemExit(f"{label}: build failed; this is not a killed mutation")
                return result

            command("emit", [str(self.bend), str(self.root / source), "-o", str(c_source)])
            if self.graph(source) != inputs or not self.tools_match():
                raise SystemExit("source or compiler changed during native emission")
            command("headers", [str(self.compiler), *self.flags, "-M", str(c_source)])
            capture = output / "headers.stdout"
            headers = file_pins(dependency_paths(capture, self.root))
            command("link", [str(self.compiler), *self.flags, str(c_source), "-lpthread", "-lm", "-o", str(artifact)])
            if (self.graph(source) != inputs or not self.tools_match() or not pins_match(headers)):
                raise SystemExit("source, compiler or headers changed during native compilation")
            receipt = {**signature, "headers": headers,
                       "dependencies": {"capture": str(capture), "sha256": sha(capture), "root": str(self.root)},
                       "artifact": str(artifact), "artifact_sha256": sha(artifact),
                       "c_source": str(c_source), "c_source_sha256": sha(c_source), "commands": commands}
            (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
            temporary = output / "index.json"
            temporary.write_text(json.dumps(receipt, indent=2) + "\n")
            os.replace(temporary, index)
            return receipt, False


def prepare_native(root, sources, cache_dir):
    """Prewarm exact protocol entries without changing launcher routing."""
    env = dict(os.environ)
    bend = executable(env.get("BEND", str(Path.home() / ".bend/bin/bend")))
    cache = NativeCache(root, cache_dir, bend, env)
    receipts = []
    for source in sources:
        receipt, reused = cache.build(source)
        receipts.append({"source": source, "reused": reused, "receipt": receipt})
    result = {"backend": "native", "cache": str(cache.directory), "protocols": receipts}
    target = cache.directory / "preparation.json"
    target.write_text(json.dumps(result, indent=2) + "\n")
    return result


def verify_prepared_native(root, prepared):
    """Revalidate prepared native entries without compiling or publishing them."""
    if prepared.get("backend") != "native" or not prepared.get("protocols"):
        raise RuntimeError("missing native mutation preparation")
    env = dict(os.environ)
    bend = executable(env.get("BEND", str(Path.home() / ".bend/bin/bend")))
    cache = NativeCache(root, prepared["cache"], bend, env)
    for row in prepared["protocols"]:
        if not cache.valid(row["receipt"], row["source"]):
            raise RuntimeError("prepared native mutation inputs changed: " + row["source"])
    return True


def execute(root, work, protocols, controls, *, suite_timeout=300, build_timeout=300):
    if type(suite_timeout) is not int or suite_timeout <= 0:
        raise SystemExit("mutation suite timeout must be a positive integer")
    if type(build_timeout) is not int or build_timeout <= 0:
        raise SystemExit("mutation build timeout must be a positive integer")
    root, work = Path(root).resolve(), Path(work).resolve()
    if work.exists() or work == root or root in work.parents:
        raise SystemExit("the work directory must be new and outside the source repository")
    work.mkdir(parents=True)
    copy = work / "copy"
    shutil.copytree(root, copy, ignore=shutil.ignore_patterns(
        ".git", "_build", "_bend2", "build", ".gatework", ".kanon-exec", ".kanon-wait",
        ".kanonx", ".kanon-replies", "__pycache__"))
    env = dict(os.environ)
    backend = env.get("BEND_MUTATION_BACKEND", env.get("BEND_TEST_BACKEND", "javascript"))
    if backend not in {"javascript", "native"}:
        raise SystemExit("BEND_MUTATION_BACKEND must be javascript or native")
    configured_bend = env.get("BEND", str(Path.home() / ".bend/bin/bend"))
    bend = str(Path(shutil.which(configured_bend) or configured_bend).resolve())
    node = shutil.which("node")
    if node is None and backend == "javascript":
        raise SystemExit("Node is required for the official Bend JavaScript target")
    node = str(Path(node).resolve()) if node else None
    version = subprocess.check_output([bend, "version"], env=env, text=True).strip()
    if version != "bend 2.0.27":
        raise SystemExit(f"expected bend 2.0.27, got {version}")
    native = NativeCache(copy, env.get("BEND_MUTATION_CACHE", root / "build/bend2-native-mutations"), bend, env) if backend == "native" else None
    tool_paths = [Path(bend), Path(shutil.which("zsh"))]
    tool_paths += list(map(Path, native.identity["tools"])) if native else [Path(node)]
    tools = {str(path): sha(path) for path in tool_paths}
    identity = {**native.identity, "tools": tools} if native else {
                "backend": "javascript", "bend": bend, "bend_version": version,
                "node": node, "node_version": subprocess.check_output([node, "--version"], text=True).strip(), "tools": tools}
    (work / "toolchain.json").write_text(json.dumps(identity, indent=2) + "\n")
    resolver = runpy.run_path(str(copy / "dev/bend2-build.py"))["source_graph"]
    base = Path(bend).parent.parent / "bend2/base.bend"

    def fixture_inputs():
        paths = [path for directory in [copy / "prelude", copy / "test"]
                 for path in directory.rglob("*") if path.is_file()]
        paths += [path for directory in [base.parent, Path.home() / ".bend/bend2"]
                  for path in directory.rglob("*") if path.is_file()]
        return {str(path): sha(path) for path in sorted(set(paths))}

    fixtures = fixture_inputs()
    (work / "fixture-inputs.json").write_text(json.dumps(fixtures, indent=2) + "\n")

    def guard_tools():
        if {str(path): sha(path) for path in tool_paths} != tools:
            raise SystemExit("compiler or runtime changed during mutation validation")

    observations = []
    baselines = []
    builds = []
    artifacts = {}
    fixture_overrides = {}

    def expected_fixtures():
        return {**fixtures, **fixture_overrides}
    source_manifest = {str(path.relative_to(copy)): sha(path)
                       for path in sorted((copy / "bend2").rglob("*.bend"))}
    (work / "source-manifest.json").write_text(json.dumps(source_manifest, indent=2) + "\n")

    def run(label, command, timeout):
        command = ["zsh", "-c", 'ulimit -s "$(ulimit -Hs)"; exec "$@"', "bend2-mutation", *command]
        result = bounded_run(command, cwd=copy, env=env, capture_output=True, timeout=timeout)
        (work / f"{label}.stdout").write_bytes(result.stdout)
        (work / f"{label}.stderr").write_bytes(result.stderr)
        return result

    def build(label, protocol):
        source, marker, *arguments = protocols[protocol]
        guard_tools()
        inputs = resolver(copy / source, base)
        current_fixtures = fixture_inputs()
        if current_fixtures != expected_fixtures():
            raise SystemExit(f"{label}: fixture inputs changed outside the intended mutation")
        if native:
            native_receipt, reused = native.build(source, timeout=build_timeout)
            artifact = Path(native_receipt["artifact"])
            (work / f"{label}.stdout").write_bytes(b"")
            (work / f"{label}.stderr").write_bytes(b"")
        else:
            artifact = work / (protocol + ".js")
            result = run(label, [bend, str(copy / source), "-o", str(artifact)], build_timeout)
            if result.returncode != 0 or not artifact.is_file():
                raise SystemExit(f"{label}: build failed; this is not a killed mutation")
        guard_tools()
        if resolver(copy / source, base) != inputs:
            raise SystemExit(f"{label}: source inputs changed during compilation")
        metadata = {"inputs": inputs, "artifact_sha256": sha(artifact), "fixtures": current_fixtures,
                    "build_timeout_seconds": build_timeout,
                    "toolchain_sha256": sha(work / "toolchain.json"),
                    "fixture_inputs_sha256": sha(work / "fixture-inputs.json"),
                    "build_stdout_sha256": sha(work / f"{label}.stdout"),
                    "build_stderr_sha256": sha(work / f"{label}.stderr")}
        if native:
            metadata.update(native=native_receipt, cache_reused=reused)
        receipt = work / f"{label}.build.json"
        receipt.write_text(json.dumps(metadata, indent=2) + "\n")
        builds.append({"label": label, "receipt": str(receipt), "sha256": sha(receipt)})
        artifacts[str(artifact)] = metadata
        return artifact

    def suite(label, protocol, artifact, override=None):
        source, marker, *configured = protocols[protocol]
        arguments = override if override is not None else configured[0] if configured else ["{root}"]
        arguments = [argument.replace("{root}", str(copy)) for argument in arguments]
        metadata = artifacts[str(artifact)]

        def guard_inputs():
            guard_tools()
            if (sha(artifact) != metadata["artifact_sha256"] or
                    resolver(copy / source, base) != metadata["inputs"] or fixture_inputs() != metadata["fixtures"] or
                    (native and not native.valid(metadata["native"], source))):
                raise SystemExit(f"{label}: frozen inputs or artifact changed")

        guard_inputs()
        started = time.monotonic()
        command = [str(artifact), *arguments] if native else [node, "--stack-size=16384", str(artifact), *arguments]
        result = run(label, command, suite_timeout)
        elapsed = time.monotonic() - started
        guard_inputs()
        receipt = {"artifact_sha256": metadata["artifact_sha256"], "exit_code": result.returncode,
                    "seconds": elapsed, "timeout_seconds": suite_timeout,
                    "stdout_sha256": sha(work / f"{label}.stdout"),
                   "stderr_sha256": sha(work / f"{label}.stderr"), "inputs_unchanged": True}
        (work / f"{label}.runtime.json").write_text(json.dumps(receipt, indent=2) + "\n")
        return result

    def baseline(stage):
        for protocol, descriptor in protocols.items():
            source, marker, *arguments = descriptor
            artifact = build(f"{stage}-{protocol}-build", protocol)
            result = suite(f"{stage}-{protocol}", protocol, artifact)
            if result.returncode != 0 or marker.encode() not in result.stdout.splitlines() or result.stderr:
                raise SystemExit(f"{stage} failed: {protocol}")
            receipt = work / f"{stage}-{protocol}.runtime.json"
            baselines.append({"stage": stage, "protocol": protocol, "receipt": str(receipt), "sha256": sha(receipt)})

    baseline("baseline")
    for control in controls:
        name, relative, old, new, protocol, diagnostic, *options = control
        options = options[0] if options else {}
        path = copy / relative
        edits = [(relative, old, new), *options.get("extra_edits", [])]
        originals = {copy / edit[0]: (copy / edit[0]).read_bytes() for edit in edits}
        original = originals[path]
        try:
            for edit_relative, edit_old, edit_new in edits:
                target = copy / edit_relative
                text = target.read_text()
                if text.count(edit_old) != 1:
                    raise SystemExit(f"{name}: mutation anchor is not unique in {edit_relative}")
                target.write_text(text.replace(edit_old, edit_new))
                if str(target) in fixtures:
                    fixture_overrides[str(target)] = sha(target)
            mutated = sha(path)
            edited_sources = {str(target.relative_to(copy)): {"original_sha256": hashlib.sha256(contents).hexdigest(),
                              "mutated_sha256": sha(target)} for target, contents in originals.items()}
            artifact = build(name + "-build", protocol)
            result = suite(name, protocol, artifact, options.get("arguments"))
            expected = diagnostic.encode()
            stream = result.stderr if options.get("stream") == "stderr" else result.stdout
            observed = expected in stream if options.get("contains") else stream.startswith(expected)
            quiet_other = not (result.stdout if options.get("stream") == "stderr" else result.stderr)
            killed = result.returncode == options.get("exit_code", 1) and observed and quiet_other
            observations.append({"name": name, "path": relative, "old": old, "new": new,
                                 "source_sha256": hashlib.sha256(original).hexdigest(),
                                 "mutant_sha256": mutated, "artifact_sha256": sha(artifact),
                                 "exit_code": result.returncode, "diagnostic": diagnostic,
                                 "options": options, "killed": killed})
            observations[-1]["edited_sources"] = edited_sources
            (work / "checkpoint.json").write_text(json.dumps(observations, indent=2) + "\n")
            print(json.dumps({"control": name, "killed": killed}), flush=True)
        finally:
            for target, contents in originals.items():
                target.write_bytes(contents)
            fixture_overrides.clear()
    baseline("restored")
    passed = all(row["killed"] for row in observations)
    report = {"passed": passed, "controls": observations, "source_manifest": source_manifest,
              "suite_timeout_seconds": suite_timeout,
              "build_timeout_seconds": build_timeout,
              "toolchain": identity, "builds": builds, "baselines": baselines,
              "fixture_inputs_sha256": sha(work / "fixture-inputs.json"),
              "protocols": {name: {"source": descriptor[0], "source_sha256": sha(copy / descriptor[0]),
                                    "marker": descriptor[1]} for name, descriptor in protocols.items()}}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": passed, "killed": sum(row["killed"] for row in observations),
                      "controls": len(observations)}))
    raise SystemExit(0 if passed else 1)


if __name__ == "__main__":
    if len(sys.argv) < 5 or sys.argv[1] != "--prepare-native":
        raise SystemExit("usage: bend2-mutation.py --prepare-native ROOT CACHE SOURCE...")
    prepared = prepare_native(sys.argv[2], sys.argv[4:], sys.argv[3])
    print(json.dumps({"backend": "native", "cache": prepared["cache"], "protocols": len(prepared["protocols"]),
                      "reused": sum(row["reused"] for row in prepared["protocols"])}))
