"""Build isolated C mutation protocols with tcc and require their intended refusal."""

import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import time


bounded_run = runpy.run_path(str(Path(__file__).with_name("c-process.py")))["run"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
        ".git", "_build", "_bend2", "_tcc", "build", ".gatework", ".kanon-exec", ".kanon-wait",
        ".kanonx", ".kanon-replies", "__pycache__"))
    env = dict(os.environ)
    located_tcc = shutil.which(env.get("TCC", "tcc"))
    if located_tcc is None:
        raise SystemExit("tcc is required for the C build")
    located_make = shutil.which("make")
    if located_make is None:
        raise SystemExit("make is required for the C build")
    tcc = str(Path(located_tcc).resolve())
    make = str(Path(located_make).resolve())
    version = subprocess.check_output([tcc, "-v"], env=env, text=True).strip()
    tool_paths = [Path(tcc), Path(make)]
    tools = {str(path): sha(path) for path in tool_paths}
    identity = {"backend": "c", "tcc": tcc, "tcc_version": version, "make": make, "tools": tools}
    (work / "toolchain.json").write_text(json.dumps(identity, indent=2) + "\n")

    def fixture_inputs():
        paths = [path for directory in [copy / "prelude", copy / "test"]
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
    c_build = {}

    def expected_fixtures():
        return {**fixtures, **fixture_overrides}

    def c_sources():
        return {str(path.relative_to(copy)): sha(path)
                for path in sorted((copy / "c").rglob("*")) if path.is_file() and path.suffix in {".c", ".h"}}

    source_manifest = c_sources()
    (work / "source-manifest.json").write_text(json.dumps(source_manifest, indent=2) + "\n")

    def target_of(protocol):
        return "_tcc/" + Path(protocols[protocol][0]).stem

    def run(label, command, timeout):
        result = bounded_run(command, cwd=copy, env=env, capture_output=True, timeout=timeout)
        (work / f"{label}.stdout").write_bytes(result.stdout)
        (work / f"{label}.stderr").write_bytes(result.stderr)
        return result

    def build(label, protocol):
        target = target_of(protocol)
        guard_tools()
        inputs = c_sources()
        current_fixtures = fixture_inputs()
        if current_fixtures != expected_fixtures():
            raise SystemExit(f"{label}: fixture inputs changed outside the intended mutation")
        artifact = copy / target
        artifact.unlink(missing_ok=True)
        result = run(label, [make, "-C", str(copy), target], build_timeout)
        # make 3.81 compares mtimes to the second: keep every later edit at least one second after this compile.
        time.sleep(1.05)
        if result.returncode != 0 or not artifact.is_file() or not os.access(artifact, os.X_OK):
            raise SystemExit(f"{label}: build failed; this is not a killed mutation")
        guard_tools()
        if c_sources() != inputs:
            raise SystemExit(f"{label}: source inputs changed during compilation")
        metadata = {"target": target, "inputs": inputs, "artifact_sha256": sha(artifact), "fixtures": current_fixtures,
                    "build_exit_code": result.returncode,
                    "build_timeout_seconds": build_timeout,
                    "toolchain_sha256": sha(work / "toolchain.json"),
                    "fixture_inputs_sha256": sha(work / "fixture-inputs.json"),
                    "build_stdout_sha256": sha(work / f"{label}.stdout"),
                    "build_stderr_sha256": sha(work / f"{label}.stderr")}
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
                    c_sources() != metadata["inputs"] or fixture_inputs() != metadata["fixtures"]):
                raise SystemExit(f"{label}: frozen inputs or artifact changed")

        guard_inputs()
        started = time.monotonic()
        result = run(label, [str(artifact), *arguments], suite_timeout)
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
            c_build[f"{stage}-{protocol}"] = {"target": target_of(protocol), "exit_code": 0,
                                              "binary_sha256": sha(artifact)}
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
              "toolchain": identity, "c_build": c_build, "builds": builds, "baselines": baselines,
              "fixture_inputs_sha256": sha(work / "fixture-inputs.json"),
              "protocols": {name: {"source": descriptor[0], "source_sha256": sha(copy / descriptor[0]),
                                    "target": target_of(name), "marker": descriptor[1]}
                            for name, descriptor in protocols.items()}}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": passed, "killed": sum(row["killed"] for row in observations),
                      "controls": len(observations)}))
    raise SystemExit(0 if passed else 1)
