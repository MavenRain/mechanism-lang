#!/usr/bin/env python3
"""Synthetic native cache and recorder controls, without running a compiler."""

import contextlib
import fcntl
import hashlib
import io
import json
import os
from pathlib import Path
import runpy
import shutil
import sys
import types


ROOT = Path(__file__).resolve().parents[1]
RECORDER = ROOT / "dev/bend2-mutation.py"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def executable(path, body):
    path.write_text("#!" + sys.executable + "\n" + body)
    path.chmod(0o755)


def main(output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    source_pin = digest(RECORDER)
    module = runpy.run_path(str(RECORDER))
    recorder_globals = module["execute"].__globals__
    original_run = recorder_globals["bounded_run"]
    original_env = dict(os.environ)
    rows = []
    cases = ["stable", "fixture-only", "source-before", "compiler-before", "header-before",
             "artifact-before", "link-failure", "header-during-link", "header-during-run",
             "artifact-during-run", "compiler-during-emit", "mutation-stderr", "invalid-backend",
             "inherited-native", "explicit-native-over-js", "explicit-js-over-native", "default-javascript",
             "inherited-javascript", "invalid-inherited-backend", "header-map-omitted", "header-entry-omitted",
             "dependency-metadata-omitted", "dependency-capture-corrupt", "local-header-mutation"]
    try:
        for case in cases:
            folder = output / case
            root, work, tools, cache = folder / "source", folder / "work", folder / "tools", folder / "cache"
            for directory in [root / "dev", root / "bend2", root / "prelude", root / "test", tools / "bin", tools / "bend2"]:
                directory.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / "dev/bend2-build.py", root / "dev/bend2-build.py")
            source = root / "bend2/protocol.bend"
            source.write_text("import Base\nstatus good\n")
            fixture = root / "prelude/fixture.mech"
            fixture.write_text("fixture good\n")
            (tools / "bend2/base.bend").write_text("synthetic base\n")
            header = root / "bend2/native header.h" if case == "local-header-mutation" else tools / "native header.h"
            header.write_text("synthetic header good\n")
            bend, cc = tools / "bin/bend", tools / "bin/clang"
            emit_log = folder / "emissions.jsonl"
            runtime = '''import json,os,pathlib,sys
data=PAYLOAD
root=pathlib.Path(sys.argv[1]);case=os.environ['NATIVE_RECORDER_CASE']
bad=data['bad'] or 'fixture bad' in (root/'prelude/fixture.mech').read_text()
if case=='header-during-run':
 with open(data['header'],'a') as out:out.write('drift\\n')
if case=='artifact-during-run':
 with open(__file__,'a') as out:out.write('# drift\\n')
if case=='mutation-stderr' and bad:print('unexpected stderr',file=sys.stderr)
print('BAD intended refusal' if bad else 'RECORDER-OK')
sys.exit(1 if bad else 0)
'''
            executable(bend, '''import json,os,pathlib,sys
if sys.argv[1:]==['version']:print('bend 2.0.27');sys.exit(0)
source=pathlib.Path(sys.argv[1]);artifact=pathlib.Path(sys.argv[sys.argv.index('-o')+1])
header=source.parent/'native header.h' if (source.parent/'native header.h').exists() else pathlib.Path(os.environ['NATIVE_RECORDER_HEADER'])
data={'bad':'status bad' in source.read_text() or 'header bad' in header.read_text(),'header':str(header)}
artifact.write_text(json.dumps(data))
with open(os.environ['NATIVE_RECORDER_EMISSIONS'],'a') as out:out.write(json.dumps(data)+'\\n')
if os.environ['NATIVE_RECORDER_CASE']=='compiler-during-emit':
 with open(__file__,'a') as out:out.write('# drift\\n')
''')
            executable(cc, '''import json,os,pathlib,sys
if sys.argv[1:]==['--version']:print('synthetic clang');sys.exit(0)
source=next(pathlib.Path(arg) for arg in sys.argv[1:] if arg.endswith('.c'))
data=json.loads(source.read_text());case=os.environ['NATIVE_RECORDER_CASE']
if '-M' in sys.argv:
 print('program.o: '+str(source).replace(' ','\\\\ ')+' '+data['header'].replace(' ','\\\\ '));sys.exit(0)
if case=='link-failure' and data['bad']:sys.exit(2)
if case=='header-during-link':
 with open(data['header'],'a') as out:out.write('drift\\n')
artifact=pathlib.Path(sys.argv[sys.argv.index('-o')+1])
artifact.write_text(#!RUNTIME)
artifact.chmod(0o755)
'''.replace("#!RUNTIME", "'#!' + " + repr(sys.executable) + " + '\\n' + " + repr(runtime) + ".replace('PAYLOAD', repr(data))"))
            executable(tools / "bin/xcrun", "print(" + repr(str(tools / "sdk")) + ")\n")
            executable(tools / "bin/node", '''import json,pathlib,sys
if sys.argv[1:]==['--version']:print('synthetic node');sys.exit(0)
artifact=pathlib.Path(sys.argv[2]);data=json.loads(artifact.read_text());sys.argv=[str(artifact),*sys.argv[3:]]
exec(RUNTIME.replace('PAYLOAD',repr(data)))
'''.replace("RUNTIME", repr(runtime)))
            os.environ.clear()
            os.environ.update(original_env)
            os.environ.update(BEND=str(bend), CC=str(cc), BEND_MUTATION_BACKEND="native", BEND_MUTATION_CACHE=str(cache),
                              PATH=str(tools / "bin") + ":" + original_env["PATH"], NATIVE_RECORDER_CASE="stable",
                              NATIVE_RECORDER_HEADER=str(header), NATIVE_RECORDER_EMISSIONS=str(emit_log))
            prepared = module["prepare_native"](root, ["bend2/protocol.bend"], cache)
            assert module["verify_prepared_native"](root, prepared) is True
            receipt = prepared["protocols"][0]["receipt"]
            malformed = {"header-map-omitted", "header-entry-omitted", "dependency-metadata-omitted", "dependency-capture-corrupt"}
            if case in malformed:
                signature = {key: receipt[key] for key in ["format", "inputs", "identity"]}
                key = hashlib.sha256(json.dumps(signature, sort_keys=True).encode()).hexdigest()
                index = cache / (key + ".json")
                bad = json.loads(json.dumps(receipt))
                if case == "header-map-omitted":
                    header.write_text(header.read_text() + "changed header\n")
                    del bad["headers"]
                elif case == "header-entry-omitted":
                    header.write_text(header.read_text() + "changed header\n")
                    del bad["headers"][str(header)]
                elif case == "dependency-metadata-omitted":
                    del bad["dependencies"]
                else:
                    Path(bad["dependencies"]["capture"]).write_text("program.o: fabricated.h\n")
                index.write_text(json.dumps(bad))
                # Matching corruption of the second receipt must still fail
                # dependency completeness checks, beyond simple record equality.
                if case != "header-map-omitted":
                    (Path(receipt["c_source"]).parent / "receipt.json").write_text(json.dumps(bad))
            if case == "source-before":
                source.write_text(source.read_text() + "extra source\n")
            elif case == "compiler-before":
                cc.write_text(cc.read_text() + "# new compiler\n")
            elif case == "header-before":
                header.write_text(header.read_text() + "changed header\n")
            elif case == "artifact-before":
                Path(receipt["artifact"]).write_text("corrupt artifact\n")
            if case in {"compiler-during-emit", "header-during-link"}:
                source.write_text(source.read_text() + "force new compilation\n")
            drifted = {"source-before", "compiler-before", "header-before", "artifact-before", "compiler-during-emit", "header-during-link"} | malformed
            try:
                verified = module["verify_prepared_native"](root, prepared)
            except RuntimeError:
                verified = False
            assert verified is (case not in drifted), (case, "prepared receipt drift check", verified)
            os.environ["NATIVE_RECORDER_CASE"] = case
            if case == "invalid-backend":
                os.environ["BEND_MUTATION_BACKEND"] = "automatic"
            elif case.startswith("inherited-") or case == "invalid-inherited-backend":
                os.environ.pop("BEND_MUTATION_BACKEND", None)
                os.environ["BEND_TEST_BACKEND"] = "native" if case == "inherited-native" else "javascript" if case == "inherited-javascript" else "automatic"
            elif case == "explicit-native-over-js":
                os.environ["BEND_TEST_BACKEND"] = "javascript"
            elif case == "explicit-js-over-native":
                os.environ.update(BEND_MUTATION_BACKEND="javascript", BEND_TEST_BACKEND="native")
            elif case == "default-javascript":
                os.environ.pop("BEND_MUTATION_BACKEND", None)
                os.environ.pop("BEND_TEST_BACKEND", None)
            mutation = ("guarded mutation", "prelude/fixture.mech", "fixture good", "fixture bad", "probe", "BAD intended refusal") if case == "fixture-only" else (
                "guarded mutation", "bend2/protocol.bend", "status good", "status bad", "probe", "BAD intended refusal")
            if case == "local-header-mutation":
                mutation = ("guarded mutation", "bend2/native header.h", "header good", "header bad", "probe", "BAD intended refusal")
            captured = io.StringIO()
            requested_timeout = {"fixture-only": 120, "inherited-native": 180,
                                 "explicit-js-over-native": 900}.get(case, 300)
            requested_build_timeout = {"fixture-only": 120, "inherited-native": 180,
                                       "explicit-js-over-native": 1800}.get(case, 300)
            observed_timeouts = []
            observed_build_timeouts = []
            def observe_run(command, **kwargs):
                if command[:2] == ["zsh", "-c"] and command[4] != str(bend):
                    observed_timeouts.append(kwargs["timeout"])
                else:
                    observed_build_timeouts.append(kwargs["timeout"])
                return original_run(command, **kwargs)
            recorder_globals["bounded_run"] = observe_run
            timeout_options = {} if requested_timeout == 300 else {"suite_timeout": requested_timeout}
            if requested_build_timeout != 300:
                timeout_options["build_timeout"] = requested_build_timeout
            with contextlib.redirect_stdout(captured):
                try:
                    module["execute"](root, work, {"probe": ("bend2/protocol.bend", "RECORDER-OK")}, [mutation], **timeout_options)
                except SystemExit as error:
                    code = error.code
            report = json.loads((work / "results.json").read_text()) if (work / "results.json").is_file() else None
            recorder_globals["bounded_run"] = original_run
            javascript_cases = {"explicit-js-over-native", "default-javascript", "inherited-javascript"}
            native_cases = {"stable", "inherited-native", "explicit-native-over-js"}
            successes = {"fixture-only", "source-before", "compiler-before", "header-before", "artifact-before", "local-header-mutation"} | javascript_cases | native_cases | malformed
            passed = code == 0 if case in successes else code != 0
            passed = passed and all(limit == requested_timeout for limit in observed_timeouts)
            passed = passed and all(0 < limit <= requested_build_timeout for limit in observed_build_timeouts)
            if case in successes:
                passed = passed and len(observed_timeouts) == 3
            if report is not None:
                passed = passed and report["suite_timeout_seconds"] == requested_timeout
                passed = passed and report["build_timeout_seconds"] == requested_build_timeout
            emissions = len(emit_log.read_text().splitlines())
            reuse = []
            if case in successes:
                if case != "fixture-only":
                    passed = passed and bool(observed_build_timeouts)
                reuse = [json.loads(Path(row["receipt"]).read_text()).get("cache_reused", False) for row in report["builds"]]
                expected = [True, False, False] if case == "local-header-mutation" else [False, False, False] if case in javascript_cases else [True, True, True] if case == "fixture-only" else [True, False, True] if case in native_cases else [False, False, True]
                expected_emissions = 4 if case in javascript_cases else 1 if case == "fixture-only" else 2 if case in native_cases else 3
                passed = passed and report["passed"] and report["controls"][0]["killed"] and reuse == expected and emissions == expected_emissions
                passed = passed and len(report["baselines"]) == 2
                passed = passed and report["toolchain"]["backend"] == ("javascript" if case in javascript_cases else "native")
            if case == "link-failure":
                passed = passed and report is None and "build failed; this is not a killed mutation" in str(code)
            if case == "mutation-stderr":
                passed = passed and report is not None and not report["controls"][0]["killed"]
            rows.append({"case": case, "passed": bool(passed), "exit": code, "emissions": emissions,
                         "requested_timeout": requested_timeout, "observed_timeouts": observed_timeouts,
                         "requested_build_timeout": requested_build_timeout,
                         "observed_build_timeouts": observed_build_timeouts,
                         "cache_reused": reuse, "work": str(work), "stdout": captured.getvalue()})
        os.environ["NATIVE_RECORDER_CASE"] = "stable"
        engine = module["NativeCache"](root, cache, bend, dict(os.environ))
        receipt, _ = engine.build("bend2/protocol.bend")
        signature = {key: receipt[key] for key in ["format", "inputs", "identity"]}
        key = hashlib.sha256(json.dumps(signature, sort_keys=True).encode()).hexdigest()
        namespace = engine.build.__func__.__globals__
        old_time = namespace["time"]
        ticks = iter([0, 10, 4000])
        with (cache / (key + ".lock")).open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            namespace["time"] = types.SimpleNamespace(monotonic=lambda: next(ticks), sleep=lambda seconds: None)
            try:
                engine.build("bend2/protocol.bend")
                lock_passed = False
            except SystemExit as error:
                lock_passed = "native cache lock timeout; this is not a killed mutation" == str(error)
            finally:
                namespace["time"] = old_time
        rows.append({"case": "lock-wait-in-build-budget", "passed": lock_passed})
    finally:
        recorder_globals["bounded_run"] = original_run
        os.environ.clear()
        os.environ.update(original_env)
    result = {"scope": "Synthetic native tools validate recorder and cache behavior, not Bend semantics",
              "source_sha256": source_pin, "source_unchanged": digest(RECORDER) == source_pin,
              "checks": rows, "passed": all(row["passed"] for row in rows)}
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": result["passed"], "cases": len(rows), "failed": [row for row in rows if not row["passed"]]}))
    return 0 if result["passed"] and result["source_unchanged"] else 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: bend2-mutation-recorder-test.py NEW_OUTPUT_DIRECTORY")
    raise SystemExit(main(sys.argv[1]))
