"""Reject runtime exports that substitute a reference for a certified result."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = 'dev/left-kan-cocone-congruence-focus.py'
FIXTURE = 'test/fixtures/prelude/left-kan-mediator-equality.mech'

def extract(root):
    return subprocess.run([sys.executable, '-I', root / SCRIPT, '--mediator-equality'],
                          cwd=root, capture_output=True, text=True, timeout=30)

def main():
    positive = extract(ROOT)
    if positive.returncode:
        raise RuntimeError(positive.stderr)
    metadata = json.loads((ROOT / '_bend2/left-kan-mediator-equality/focused-extraction.json').read_text())
    with tempfile.TemporaryDirectory(prefix='mechanism-mediator-equality-focus-') as directory:
        mutant = Path(directory)
        for relative in [*metadata['source_files'], SCRIPT, 'bend2/tests/prelude_left_kan_laws.bend']:
            target = mutant / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / relative).read_bytes())
        fixture = mutant / FIXTURE
        text = fixture.read_text()
        correct = 'def lanMediatorEqOtherChoice : Nat := lanMediatorEqOtherChoiceAt sharedLanInput'
        if text.count(correct) != 1:
            raise ValueError('expected one certified recovery export')
        fixture.write_text(text.replace(correct,
            'def lanMediatorEqOtherChoice : Nat := lanMediatorEqOtherChoiceReference'))
        negative = extract(mutant)
        if negative.returncode == 0 or 'runtime exports omit mediator equality proofs: mediatorOtherChoiceLaw' not in negative.stderr:
            raise RuntimeError('reference substitution was not rejected')
    print('LEFT-KAN-MEDIATOR-EQUALITY-FOCUS-OK positive=1 mutation=1')

if __name__ == '__main__':
    main()
