#!/usr/bin/env python3
"""Replay frozen historical frontend observations without a reference compiler."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import re
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def case_id(source: str, empty: bool) -> str:
    return hashlib.sha256(json.dumps([source, empty], ensure_ascii=True, separators=(',', ':')).encode()).hexdigest()

def validate(evidence: dict) -> None:
    if evidence.get('version') != 2:
        raise ValueError('expected frozen surface corpus version2')
    ids = set()
    for case in evidence['cases']:
        if case['id'] in ids or case['id'] != case_id(case['source'], case['empty']):
            raise ValueError('duplicate or changed input identity')
        ids.add(case['id'])
        if case['source_sha256'] != hashlib.sha256(case['source'].encode()).hexdigest():
            raise ValueError('source hash mismatch')
        if set(case['expected']) != {'code', 'stdout', 'stderr'} or case['expected']['code'] not in (0, 1):
            raise ValueError('missing or invalid historical observation')
        if not case['aliases']:
            raise ValueError('missing historical provenance')
    if len(ids) != evidence['unique_cases']:
        raise ValueError('case inventory mismatch')
    if sum(len(c['aliases']) for c in evidence['cases']) != evidence['represented_observations']:
        raise ValueError('observation inventory mismatch')

def observe(command: list[str], case: dict, timeout: float) -> dict:
    started = time.monotonic()
    try:
        result = subprocess.run([*command, *(['--empty'] if case['empty'] else []), case['source']], capture_output=True, text=True, timeout=timeout)
        actual = dict(code=result.returncode, stdout=result.stdout, stderr=result.stderr)
    except subprocess.TimeoutExpired as error:
        actual = dict(code='timeout', stdout=(error.stdout or b'').decode(errors='replace'), stderr=(error.stderr or b'').decode(errors='replace'))
    except OSError as error:
        actual = dict(code='launch-error', stdout='', stderr=str(error))
    return dict(id=case['id'], labels=[a['label'] for a in case['aliases']], actual=actual, expected=case['expected'], passed=actual == case['expected'], elapsed_seconds=round(time.monotonic()-started, 6))

def artifacts(driver: Path) -> dict[str, str]:
    paths = [driver]
    if driver.suffix == '.exe':
        launcher = driver.read_bytes()
        if launcher.startswith(b'#!'):
            targets = re.findall(r'"\$MECHANISM_BUILD_ROOT/([^"\n]+)"', launcher.decode())
            if len(targets) != 1:
                raise ValueError('cannot identify the launcher artifact')
            build_root = driver.parent.parent.resolve()
            artifact = (build_root / targets[0]).resolve()
            if build_root not in artifact.parents:
                raise ValueError('launcher artifact escapes its build directory')
            paths.append(artifact)
            stamp = artifact.with_name(artifact.name + '.build.json')
            if stamp.is_file():
                paths.append(stamp)
    return {str(path): sha(path) for path in paths}

def checkpoint(path: Path, header: dict, results: list[dict]) -> None:
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(dict(**header, results=results))+'\n')
    temporary.replace(path)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--cases', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--driver', type=Path)
    parser.add_argument('--driver-arg', action='append', default=[])
    parser.add_argument('--report', type=Path)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--timeout', type=float, default=900)
    parser.add_argument('--require-complete-baseline', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout <= 0:
        parser.error('jobs and timeout must be positive')
    root = args.root.resolve()
    path = args.cases or root/'dev/bend2/surface-cases.json'
    evidence = json.loads(path.read_text())
    validate(evidence)
    baseline_path = args.baseline or root/evidence['baseline']['path']
    if sha(baseline_path) != evidence['baseline']['sha256']:
        raise ValueError('unresolved baseline evidence hash mismatch')
    baseline = json.loads(baseline_path.read_text())
    if len(baseline['cases']) != evidence['baseline']['unresolved_attempts']:
        raise ValueError('unresolved baseline inventory mismatch')
    driver = (args.driver or root/'_bend2/test/surface_check_driver.exe').resolve()
    command = ['node', '--stack-size=16384', str(driver)] if driver.suffix == '.js' else [str(driver)]
    command += args.driver_arg
    if driver.suffix == '.js':
        command = ['zsh', '-c', 'ulimit -s "$(ulimit -Hs)"; exec "$@"', 'surface-replay', *command]
    report_path = args.report or root/'build/bend2-surface-check/report.json'
    report_path.parent.mkdir(parents=True, exist_ok=True)
    results = []
    failures = []
    fingerprints = artifacts(driver)
    header = dict(driver_sha256=sha(driver), artifact_sha256=fingerprints, corpus_sha256=sha(path), command=command)
    partial_path = report_path.with_name(report_path.stem+'.partial.json')
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(observe, command, case, args.timeout) for case in evidence['cases']]
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            if not result['passed']:
                failures.append(result)
                print(json.dumps(dict(failed=result['labels'], code=result['actual']['code'], stdout_bytes=len(result['actual']['stdout'].encode()), stderr=result['actual']['stderr'][:512])), flush=True)
            if len(results)%25 == 0 or not result['passed']:
                checkpoint(partial_path, header, results)
            if len(results)%100 == 0:
                print(json.dumps(dict(compared=len(results), failed=len(failures))), flush=True)
    if artifacts(driver) != fingerprints:
        raise ValueError('driver artifact changed during replay; checkpoint is not final evidence')
    checkpoint(partial_path, header, results)
    report = dict(cases=len(results), represented_observations=evidence['represented_observations'], failures=failures, baseline_unresolved=len(baseline['cases']), baseline_evidence=str(baseline_path), baseline_status='unresolved, not passing', **header, timeout_seconds=args.timeout, jobs=args.jobs, elapsed_seconds=round(time.monotonic()-started, 6))
    report_path.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(cases=len(results), failed=len(failures), baseline_unresolved=len(baseline['cases']), report=str(report_path))), flush=True)
    return 1 if failures else 2 if args.require_complete_baseline and baseline['cases'] else 0

if __name__ == '__main__':
    sys.exit(main())
