#!/usr/bin/env python3
"""Replay 28 frozen original CLI observations against a Bend launcher."""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--driver', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[2] / '_bend2/bin/mech.exe')
options = parser.parse_args()
new = options.driver.resolve()
command = ['node', '--stack-size=16384', str(new), '--'] if new.suffix == '.js' else [str(new)]
frozen = json.loads(pathlib.Path(__file__).with_name('cli_core_expected.json').read_text())['observations']
checks = []
failures = []

def compare(label, args, artifact=None):
    if artifact is not None and artifact.exists(): artifact.unlink()
    actual = subprocess.run([*command, *map(str,args)], capture_output=True)
    observed = artifact.read_bytes() if artifact is not None and artifact.exists() else None
    if artifact is not None and artifact.exists(): artifact.unlink()
    expected = frozen[label]
    right = {'code':actual.returncode, 'stdout':actual.stdout.decode(errors='replace').replace(str(root),'{TMP}'),
             'stderr':actual.stderr.decode(errors='replace').replace(str(root),'{TMP}'),
             'bytes':None if observed is None else observed.hex()}
    if expected != right:
        print(json.dumps({'case':label,'expected':expected,'actual':right},indent=2),flush=True)
        failures.append(label)
        return
    checks.append(label)
    print(f'passed: {label}', flush=True)

with tempfile.TemporaryDirectory(prefix='bend-cli-core-') as directory:
    root = pathlib.Path(directory)
    good = root / "good ' ;$().kan"
    good.write_text('def main : Nat := 42\n')
    bad = root / 'bad.kan'; bad.write_text('def main : Nat := unknown\n')
    proof = root / 'axiom.kan'; proof.write_text('axiom hole : Nat\ndef main : Nat := 42\n')
    circuit = root / 'circuit.kan'; circuit.write_text('def main : Nat := natAdd 1 2\n')
    refused = root / 'refused.kan'; refused.write_text('axiom opaque : Nat\ndef main : Nat := opaque\n')
    big = root / 'big.kan'; big.write_text('def main : Nat := 1073741824\n')
    a = root / 'a.kan'; a.write_text('def add : Nat -> Nat -> Nat := fun (a : Nat) (b : Nat) => natAdd a b\n')
    b = root / 'b.kan'; b.write_text('def main : Nat := add 40 2\n')
    out = root / 'out.wasm'
    compare('usage',['check'])
    compare('help passthrough',['--help'])
    compare('unknown',['unknown'])
    compare('spec-count',['spec-count','ignored'])
    compare('check',['check',good])
    compare('checked form',['check','--print',good])
    compare('checked trailing args',['check','--print',good,'ignored'])
    compare('erased form',['check','--erased',good])
    compare('check failure',['check',bad])
    compare('missing input',['check',root/'missing.kan'])
    compare('axioms',['axioms',proof])
    compare('circuit',['circuit',circuit])
    compare('circuit refusal silent stderr',['circuit',refused])
    compare('emit',['emit',good,'-o',out,'--export','main'],out)
    compare('emit missing export',['emit',good,'-o',out,'--export','absent'],out)
    compare('emit output path',['emit',good,'-o',root/'absent'/'out.wasm','--export','main'])
    compare('build',['build',a,b,'-o',out,'--export','add','--export','main'],out)
    compare('build interspersed flags',['build',a,'--export','main','-o',out,b],out)
    compare('build repeated output',['build',a,b,'-o',out,'-o',out,'--export','main'])
    compare('build repeated export',['build',good,'-o',out,'--export','main','--export','main'],out)
    compare('run kernel',['run',good,'--export','main','--host','kernel'])
    compare('run node',['run',good,'--export','main','--host','node'])
    compare('run wasmtime',['run',good,'--export','main','--host','wasmtime'])
    compare('run both',['run',good,'--export','main'])
    compare('run kernel trap',['run',big,'--export','main','--host','kernel'])
    compare('run both trap',['run',big,'--export','main','--host','both'])
    compare('run bad host',['run',good,'--export','main','--host','unknown'])
    compare('run missing file',['run',root/'missing.kan','--export','main'])
assert set(checks + failures) == set(frozen), 'CLI case inventory changed'
print(json.dumps({'checks':len(checks)+len(failures),'passed':checks,'failed':failures}))
raise SystemExit(bool(failures))
