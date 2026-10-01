"""Reject runtime exports that substitute a reference for a certified result."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = 'dev/left-kan-cocone-congruence-focus.py'
FIXTURE = 'test/fixtures/prelude/left-kan-roundtrip.mech'

def extract(root):
    return subprocess.run([sys.executable, '-I', root / SCRIPT, '--roundtrip'],
                          cwd=root, capture_output=True, text=True, timeout=30)

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
