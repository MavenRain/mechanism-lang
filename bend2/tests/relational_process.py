"""Exercise bounded wrapper success, failure, timeout and cancellation."""
import json
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "dev/bend2-process.py"
run = runpy.run_path(str(HELPER))["run"]
assert run([sys.executable, "-c", "print('ok')"], capture_output=True).stdout == b"ok\n"
assert run([sys.executable, "-c", "print('ok')"], capture_output=True, text=True).stdout == "ok\n"
assert run([sys.executable, "-c", "import sys;sys.stdout.write(sys.stdin.read())"], input="input", capture_output=True, text=True).stdout == "input"
try:
    run([sys.executable, "-c", "raise SystemExit(7)"], check=True)
    raise AssertionError("nonzero exit was accepted")
except subprocess.CalledProcessError as error:
    assert error.returncode == 7

with tempfile.TemporaryDirectory(prefix="bend-process-controls-") as directory:
    root = Path(directory)
    late = root / "late"
    child = "import time;from pathlib import Path;time.sleep(1);Path(" + repr(str(late)) + ").write_text('leaked')"
    wrapper = "import subprocess,sys;subprocess.Popen([sys.executable,'-c'," + repr(child) + "]);import time;time.sleep(20)"
    started = time.monotonic()
    try:
        run([sys.executable, "-c", wrapper], capture_output=True, timeout=0.2)
        raise AssertionError("timeout was accepted")
    except subprocess.TimeoutExpired:
        assert time.monotonic() - started < 5
    time.sleep(1.1)
    assert not late.exists(), "timeout left a descendant alive"

    ready = root / "ready"
    ready_child = "from pathlib import Path;Path(" + repr(str(ready)) + ").write_text('ready');" + child
    wrapper = "import subprocess,sys;subprocess.Popen([sys.executable,'-c'," + repr(ready_child) + "]);import time;time.sleep(20)"
    host = "import runpy,sys;runpy.run_path(" + repr(str(HELPER)) + ")[\"run\"]([sys.executable,'-c'," + repr(wrapper) + "],timeout=20)"
    process = subprocess.Popen([sys.executable, "-c", host])
    try:
        deadline = time.monotonic() + 5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        assert ready.exists(), "cancellation probe did not start"
        os.kill(process.pid, signal.SIGTERM)
        assert process.wait(timeout=5) == 143
        time.sleep(1.1)
        assert not late.exists(), "cancellation left a descendant alive"
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
print("RELATIONAL-PROCESS-OK cases=6")
