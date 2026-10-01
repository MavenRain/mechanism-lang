"""Reject runtime exports that substitute a reference for a certified result."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = 'dev/left-kan-cocone-congruence-focus.py'
FIXTURE = 'test/fixtures/prelude/left-kan-solution-transport-runtime.mech'

def extract(root):
    return subprocess.run([sys.executable, '-I', root / SCRIPT, '--solution-transport'],
                          cwd=root, capture_output=True, text=True, timeout=30)

def main():
    positive = extract(ROOT)
    if positive.returncode:
        raise RuntimeError(positive.stderr)
    metadata = json.loads((ROOT / '_bend2/left-kan-solution-transport/focused-extraction.json').read_text())
    with tempfile.TemporaryDirectory(prefix='mechanism-solution-transport-focus-') as directory:
        mutant = Path(directory)
        for relative in [*metadata['source_files'], SCRIPT, 'bend2/tests/prelude_left_kan_laws.bend']:
            target = mutant / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / relative).read_bytes())
        fixture = mutant / FIXTURE
        text = fixture.read_text()
        for export, proof in [
            ('lanSolutionRuntimeTargetFactor', 'solutionRuntimeTargetFactorLaw'),
            ('lanSolutionRuntimeTargetUnique', 'solutionRuntimeTargetUniqueLaw'),
            ('lanSolutionRuntimeUnitFactor', 'solutionRuntimeUnitFactorLaw'),
            ('lanSolutionRuntimeUnitUnique', 'solutionRuntimeUnitUniqueLaw'),
        ]:
            correct = f'def {export} : Nat := {export}At sharedLanInput'
            if text.count(correct) != 1:
                raise ValueError(f'expected one certified export: {export}')
            fixture.write_text(text.replace(correct, f'def {export} : Nat := {export}Reference'))
            negative = extract(mutant)
            diagnostic = f'runtime exports omit solution transport proofs: {proof}'
            if negative.returncode == 0 or diagnostic not in negative.stderr:
                raise RuntimeError(f'reference substitution was not rejected: {export}')
    print('LEFT-KAN-SOLUTION-TRANSPORT-FOCUS-OK positive=1 mutation=4')

if __name__ == '__main__':
    main()
