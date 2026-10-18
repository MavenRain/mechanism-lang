#!/usr/bin/env python3
"""Check build-cache invalidation with real C executables in an isolated tree."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="c-build-cache-") as directory:
        work = Path(directory)
        (work / "dev").mkdir()
        (work / "c").mkdir()
        shutil.copyfile(ROOT / "dev/c-build.py", work / "dev/c-build.py")
        source = work / "c/main.c"
        source.write_text("int main(void) { return BUILD_VALUE; }\n")
        (work / "Makefile").write_text('''PYTHON ?= python3
TCC ?= tcc
.PHONY: c-build c-build-artifacts
c-build:
\t$(PYTHON) dev/c-build.py --tcc "$(TCC)" --cflags="$(CFLAGS)" --ldflags="$(LDFLAGS)"
c-build-artifacts: _tcc/bin/mech _tcc/bin/mech_cert
_tcc/bin/%: c/main.c
\t@mkdir -p _tcc/bin
\t$(TCC) $(CFLAGS) -o $@ $< $(LDFLAGS)
''')

        def build(value, direct=False):
            command = ([sys.executable, "dev/c-build.py", "--tcc", os.environ.get("TCC", "tcc"),
                        f"--cflags=-DBUILD_VALUE={value}"] if direct else
                       ["make", "c-build", f"CFLAGS=-DBUILD_VALUE={value}"])
            subprocess.run(command,
                           cwd=work, check=True, timeout=30)

        def result():
            return subprocess.run([str(work / "_tcc/bin/mech")], timeout=10).returncode

        build(0, direct=True)
        assert result() == 0
        build(1)
        assert result() == 1, "a flag change reused the old executable"
        stamp = source.stat()
        source.write_text("int main(void) { return BUILD_VALUE + 1; }\n")
        os.utime(source, ns=(stamp.st_atime_ns, 1))
        build(1)
        assert result() == 2, "a source change with an old timestamp reused the executable"
        binary = work / "_tcc/bin/mech"
        stamp = binary.stat().st_mtime_ns
        build(1)
        assert binary.stat().st_mtime_ns == stamp, "unchanged inputs rebuilt the executable"
        guard = work / "dev/c-build.py"
        guard.write_text(guard.read_text() + "\n# Audit-only guard edit control.\n")
        build(1)
        assert binary.stat().st_mtime_ns == stamp, "a guard-only edit rebuilt unchanged C inputs"
    print("C-BUILD-CACHE-OK flags=1 preserved-mtime-source=1 unchanged=1 direct-config=1 guard-only=1")


if __name__ == "__main__":
    main()
