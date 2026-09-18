"""Verify the retained mutation executions against the final source and predicates."""
from pathlib import Path
import gzip
import hashlib
import importlib.util
import json
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location('mutations', ROOT / 'dev/shared-left-kan-mutations.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, message):
    if not condition:
        print(f'MUTATION-EVIDENCE FAIL {message}', file=sys.stderr)
        raise SystemExit(1)


record = json.loads((HERE / 'mutation-executions.json').read_text())
corrected = json.loads((HERE / 'mutation-predicates.json').read_text())
outputs = json.loads(gzip.decompress((HERE / 'mutation-outputs.json.gz').read_bytes()))
require(digest((HERE / 'mutation-executions.json').read_bytes()) == corrected['execution_results_sha256'],
        'execution_results_sha256 does not cover mutation-executions.json')
require(digest((ROOT / 'dev/shared-left-kan-mutations.py').read_bytes()) == corrected['final_driver_sha256'],
        'final_driver_sha256 does not cover dev/shared-left-kan-mutations.py')
require(digest((ROOT / 'test/prelude_shared_left_kan.ml').read_bytes()) == record['suite_source_sha256'],
        'suite_source_sha256 does not cover test/prelude_shared_left_kan.ml')
for path, expected in record['source_sha256'].items():
    require(digest((ROOT / path).read_bytes()) == expected, f'source_sha256 does not cover {path}')
rows = {row['id']: row for row in record['rows']}
require(set(rows) == set(outputs) == {'baseline', 'restored', *(item[0] for item in module.CONTROLS)},
        'the execution ids, the retained outputs and the controls do not agree')
for label, row in rows.items():
    for stream in ['stdout', 'stderr']:
        require(digest(outputs[label][stream].encode()) == row[f'{stream}_sha256'],
                f'{label}: the retained {stream} does not match its hash')
for label in ['baseline', 'restored']:
    require(rows[label]['exit'] == 0 and outputs[label]['stdout'] == module.OK,
            f'{label}: the retained run is not a clean run')
    require(outputs[label]['stderr'] == '', f'{label}: the retained run wrote to stderr')
prefixes = {}
for label, path, before, after, expected in module.CONTROLS:
    original = (ROOT / path).read_text()
    require(before is None or original.count(before) == 1, f'{label}: the anchor is not unique in {path}')
    mutated = after if before is None else original.replace(before, after)
    row = rows[label]
    require(digest(original.encode()) == row['before_sha256'], f'{label}: before_sha256 does not match {path}')
    require(digest(mutated.encode()) == row['mutated_sha256'], f'{label}: mutated_sha256 does not match {path}')
    prefix = module.FAIL + expected
    require(row['exit'] == 1 and outputs[label]['stderr'] == '',
            f'{label}: the retained run is not a refusal with an empty stderr')
    require(outputs[label]['stdout'].startswith(prefix), f'{label}: the retained stdout misses its prefix')
    prefixes[label] = prefix
values = list(prefixes.values())
require(len(set(values)) == len(values), 'two controls share one diagnostic prefix')
require(all(a == b or not a.startswith(b) for a in values for b in values),
        'one diagnostic prefix is a prefix of another')
predicates = {row['id']: row for row in corrected['rows']}
require(set(predicates) == set(prefixes), 'the predicate ids and the control ids do not agree')
for label, prefix in prefixes.items():
    predicate = predicates[label]
    require(predicate['killed'] is True, f'{label}: the predicate row is not killed')
    require(predicate['expected'] == prefix, f'{label}: the predicate prefix is not the recomputed prefix')
    require(predicate['stdout_sha256'] == rows[label]['stdout_sha256'],
            f'{label}: the predicate output hash is not the retained output hash')
    require(predicate['stdout_bytes'] == len(outputs[label]['stdout'].encode()),
            f'{label}: the predicate output size is not the retained output size')
    require(predicate['exit'] == rows[label]['exit'],
            f'{label}: the predicate exit is not the retained exit')
require(corrected['killed'] == corrected['controls'] == len(module.CONTROLS),
        'the predicate totals do not count every control as killed')
for flag in ['passed', 'baseline_passed', 'restored_passed', 'sources_unchanged']:
    require(corrected[flag] is True, f'the predicate flag {flag} is not true')
print(f'MUTATION-EVIDENCE OK controls={len(prefixes)} sources={len(record["source_sha256"])}')
