"""Supported nested helper cancellation, independently bounded and drained."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import tempfile
import time

SELF = Path(__file__).resolve()
HELPER = SELF.parents[2] / "dev/bend2-process.py"
COMMAND_TIMEOUT = 0.5


def save(path, value):
    path.write_text(json.dumps(value, sort_keys=True) + "\n")


def actor(role, directory, mode):
    save(directory / (str(os.getpid()) + ".pid.json"),
         {"pid": os.getpid(), "pgid": os.getpgrp(), "role": role})
    if role == "leaf":
        print("partial-out", flush=True)
        print("partial-err", file=sys.stderr, flush=True)
        (directory / "ready").touch()
        time.sleep(2)
        (directory / "late").touch()
        return
    run = runpy.run_path(str(HELPER))["run"]
    child = "middle" if role == "driver" else "leaf"
    command = [sys.executable, "-I", str(SELF), "--actor", child, str(directory), mode]
    if role == "middle":
        kwargs = ({"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
                  if mode == "timeout-no-pipes" else {})
        run(command, timeout=5, **kwargs)
        return
    started = time.monotonic()
    try:
        run(command, capture_output=True, timeout=5 if mode == "interrupt" else COMMAND_TIMEOUT)
        save(directory / "result.json", {"kind": "unexpected-success"})
    except subprocess.TimeoutExpired as error:
        save(directory / "result.json", {"kind": "timeout", "timeout": error.timeout,
             "stdout": (error.output or b"").hex(), "stderr": (error.stderr or b"").hex(),
             "seconds": time.monotonic() - started})


def identities(directory):
    return [json.loads(p.read_text()) for p in directory.glob("*.pid.json")]


def alive(identity):
    try:
        return os.getpgid(identity["pid"]) == identity["pgid"]
    except ProcessLookupError:
        return False


def case(root, mode):
    directory = root / mode
    directory.mkdir()
    command = [sys.executable, "-I", str(SELF), "--actor", "driver", str(directory), mode]
    process = subprocess.Popen(command, start_new_session=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    record = {"mode": mode, "command": command, "outer_cap": 7}
    owned = [{"pid": process.pid, "pgid": process.pid, "role": "driver"}]
    started = time.monotonic()
    try:
        deadline = started + 3
        while not (directory / "ready").exists() and time.monotonic() < deadline:
            if process.poll() is not None:
                break
            time.sleep(0.005)
        assert (directory / "ready").exists(), "nested leaf did not become ready"
        if mode == "interrupt":
            os.kill(process.pid, signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=max(0.01, 7 - (time.monotonic() - started)))
        record.update(exit_code=process.returncode, stdout=stdout.hex(), stderr=stderr.hex())
        owned = identities(directory)
        record["live_before_emergency"] = [p for p in owned if alive(p)]
        assert not record["live_before_emergency"], "supported nested child survived"
        assert not (directory / "late").exists(), "supported nested child completed after timeout"
        assert not stdout and not stderr, "driver emitted unexpected output"
        if mode == "interrupt":
            assert process.returncode == 143, "original cancellation code changed"
        else:
            assert process.returncode == 0
            result = json.loads((directory / "result.json").read_text())
            record["result"] = result
            assert result["kind"] == "timeout" and result["timeout"] == COMMAND_TIMEOUT
            if mode == "timeout-pipes":
                assert bytes.fromhex(result["stdout"]) == b"partial-out\n"
                assert bytes.fromhex(result["stderr"]) == b"partial-err\n"
        record["passed"] = True
    except BaseException as error:
        record.update(passed=False, error=repr(error))
    finally:
        owned += identities(directory)
        cleanup = []
        for item in {p["pgid"]: p for p in owned}.values():
            if alive(item):
                try:
                    os.killpg(item["pgid"], signal.SIGKILL)
                    cleanup.append({"pgid": item["pgid"], "killed": True})
                except ProcessLookupError:
                    pass
        process.communicate(timeout=2)
        deadline = time.monotonic() + 1
        while any(alive(p) for p in owned) and time.monotonic() < deadline:
            time.sleep(0.005)
        record.update(identities=owned, emergency_cleanup=cleanup,
                      live_after_emergency=[p for p in owned if alive(p)],
                      seconds=time.monotonic() - started)
        if record["live_after_emergency"]:
            record.update(passed=False, cleanup_failed=True)
        save(directory / "REPORT.json", record)
    return record


def suite(root):
    before = hashlib.sha256(HELPER.read_bytes()).hexdigest()
    rows = [case(root, mode) for mode in ["timeout-no-pipes", "timeout-pipes", "interrupt"]]
    after = hashlib.sha256(HELPER.read_bytes()).hexdigest()
    save(root / "REPORT.json", {"helper_before": before, "helper_after": after, "cases": rows})
    assert before == after, "helper changed during test"
    assert all(row["passed"] for row in rows), json.dumps(rows)
    print("RELATIONAL-PROCESS-NESTED-OK cases=3")


if len(sys.argv) > 1 and sys.argv[1] == "--actor":
    actor(sys.argv[2], Path(sys.argv[3]), sys.argv[4])
elif len(sys.argv) == 3 and sys.argv[1] == "--evidence":
    destination = Path(sys.argv[2])
    destination.mkdir()
    suite(destination)
else:
    assert len(sys.argv) == 1
    with tempfile.TemporaryDirectory(prefix="bend-process-nested-") as directory:
        suite(Path(directory))
