"""Reject runtime exports that substitute a reference for a certified result."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import os

# Hop 330 (USER ruling 1 on the memcap-blind python legs, D-329a): when MEMCAP
# names the memcap wrapper, every driver this harness spawns runs as
# `MEMCAP 3800 <secs> driver...`, so the cap sits on the driver, not on python3.
# <secs> is the call's own timeout less one second (memcap fires first and
# leaves a record), or MEMCAP_SECONDS when the call has no timeout.  Without
# MEMCAP the commands are unchanged.  memcap prints its record line on the
# driver's stdout; uncapped() moves such lines to this harness's stderr so the
# byte-for-byte stdout checks stay.
MEMCAP = os.environ.get("MEMCAP")
MEMCAP_SECONDS = int(os.environ.get("MEMCAP_SECONDS", "900"))


def capped(command, seconds=None):
    budget = MEMCAP_SECONDS if seconds is None else max(1, int(seconds) - 1)
    prefix = [MEMCAP, "3800", str(budget)] if MEMCAP else []
    return prefix + [str(part) for part in command]


def uncapped(result):
    stdout = getattr(result, "stdout", None)
    if not MEMCAP or stdout is None:
        return result
    marker = "memcap " if isinstance(stdout, str) else b"memcap "
    lines = stdout.splitlines(keepends=True)
    moved = [line for line in lines if line.startswith(marker)]
    kept = [line for line in lines if not line.startswith(marker)]
    if moved:
        text = marker[:0].join(moved)
        sys.stderr.write(text if isinstance(text, str) else text.decode("utf-8", "replace"))
        sys.stderr.flush()
    result.stdout = marker[:0].join(kept)
    return result

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = 'dev/left-kan-cocone-congruence-focus.py'
FIXTURE = 'test/fixtures/prelude/left-kan-roundtrip.mech'

def extract(root):
    return uncapped(subprocess.run(capped([sys.executable, '-I', root / SCRIPT, '--roundtrip'], 30),
                          cwd=root, capture_output=True, text=True, timeout=30))

def main():
    positive = extract(ROOT)
    if positive.returncode:
        raise RuntimeError(positive.stderr)
    metadata = json.loads((ROOT / '_bend2/left-kan-roundtrip/focused-extraction.json').read_text())
    with tempfile.TemporaryDirectory(prefix='mechanism-roundtrip-focus-') as directory:
        mutant = Path(directory)
        for relative in [*metadata['source_files'], SCRIPT, 'bend2/tests/prelude_left_kan_laws.bend']:
            target = mutant / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / relative).read_bytes())
        fixture = mutant / FIXTURE
        text = fixture.read_text()
        correct = 'def lanRoundtripOtherDesc : Nat := lanRoundtripOtherDescAt sharedLanInput'
        if text.count(correct) != 1:
            raise ValueError('expected one certified recovery export')
        fixture.write_text(text.replace(correct,
            'def lanRoundtripOtherDesc : Nat := lanRoundtripOtherDescReferenceAt sharedLanInput'))
        negative = extract(mutant)
        if negative.returncode == 0 or 'runtime exports omit round-trip proofs: roundtripOtherDescLaw' not in negative.stderr:
            raise RuntimeError('reference substitution was not rejected')
    print('LEFT-KAN-ROUNDTRIP-FOCUS-OK positive=1 mutation=1')

if __name__ == '__main__':
    main()
