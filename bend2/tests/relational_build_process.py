"""Compile timeout must stop the compiler wrapper and its descendants."""
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
compile_protocol = runpy.run_path(str(ROOT / "dev/bend2-mutation-build.py"))["compile_protocol"]
bounded_run = runpy.run_path(str(ROOT / "dev/bend2-process.py"))["run"]
with tempfile.TemporaryDirectory(prefix="bend-build-process-") as directory:
    root = Path(directory)
    (root / "dev").mkdir()
    late = root / "late"
    child = "import time;from pathlib import Path;time.sleep(1);Path(" + repr(str(late)) + ").write_text('leaked')"
    wrapper = "import subprocess,sys,time;subprocess.Popen([sys.executable,'-c'," + repr(child) + "]);time.sleep(20)"
    (root / "dev/bend2-mutation-build.py").write_text(wrapper)

    def shortened(*args, **kwargs):
        assert kwargs["timeout"] == 3600
        kwargs["timeout"] = 0.2
        return bounded_run(*args, **kwargs)

    with patch.object(runpy, "run_path", return_value={"run": shortened}):
        try:
            compile_protocol(root, "functor")
            raise AssertionError("compiler timeout was accepted")
        except subprocess.TimeoutExpired:
            pass
    time.sleep(1.1)
    assert not late.exists(), "compiler timeout left a descendant alive"
print("RELATIONAL-BUILD-PROCESS-OK cases=1")
