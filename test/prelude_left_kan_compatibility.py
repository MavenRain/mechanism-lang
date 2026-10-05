#!/usr/bin/env python3
"""Reject incompatible LeftKanExtension records and changed computations."""

import argparse
import copy
import importlib.util
import json
import re
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('leftkan', ROOT / 'dev/prelude-left-kan-compatibility.py')
leftkan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(leftkan)
MECH = REPORT = None
TIMEOUT = 180


class LeftKanControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text())
        for path, expected in cls.report['inputs'].items():
            if leftkan.pilot.digest(ROOT / path) != expected:
                raise AssertionError('input changed: ' + path)
        for path, expected in cls.report['binaries'].items():
            if leftkan.pilot.digest(path) != expected:
                raise AssertionError('checker changed: ' + path)
        if str(MECH) not in cls.report['binaries']:
            raise AssertionError('checker not hashed: ' + str(MECH))
        if leftkan.pilot.digest(REPORT.parent / 'import/types.ndjson') != cls.report['import_graph_sha256']:
            raise AssertionError('import graph changed')
        for row in cls.report['signatures'] + cls.report['support']:
            if leftkan.pilot.digest(REPORT.parent / row['fixture']) != row['fixture_sha256']:
                raise AssertionError('signature fixture changed: ' + row['name'])
        if leftkan.pilot.digest(REPORT.parent / cls.report['computation_fixture']) != cls.report['computation_sha256']:
            raise AssertionError('computation fixture changed')
        for row in cls.report['closed']:
            if leftkan.pilot.digest(REPORT.parent / row['fixture']) != row['fixture_sha256']:
                raise AssertionError('closed fixture changed: ' + row['fixture'])
        cls.graph = leftkan.pilot.read_graph(REPORT.parent / 'import/types.ndjson')
        cls.prelude = leftkan.source()
        cls.fixture = (ROOT / leftkan.COMPUTATIONS).read_text()
        cls.adapter = (ROOT / leftkan.LEFT_KAN_SOURCE).read_text()

    def evidence(self, label):
        path = REPORT.parent / 'controls' / label
        path.mkdir(parents=True, exist_ok=False)
        return path

    def replace(self, text, old, new):
        self.assertEqual(text.count(old), 1, old)
        return text.replace(old, new)

    def span(self, text, name):
        start = text.index('\ndef ' + name + ' :')
        ends = (text.find('\ndef ', start + 2), text.index('\nend\n', start))
        return start, min(index for index in ends if index >= 0)

    def without(self, *names):
        result = self.fixture
        for name in names:
            start, end = self.span(result, name)
            result = result[:start] + result[end:]
        return result

    def check(self, directory, name, text, accepted=True):
        path = directory / (name + '.mech')
        path.write_text(text)
        result = leftkan.functor.run_fixture(MECH, 'check', path, TIMEOUT)
        leftkan.functor.write_report(directory / (name + '.json'), result)
        self.assertEqual(result['exit'], 0 if accepted else 1, result)
        if not accepted:
            self.assertRegex(result['stderr'] or result['stdout'], r'(mismatch:|quantity:|wrong-level:)')
        return result

    def signature(self, directory, member, prelude, aliases=None, needle=None):
        target = 'Candidate_' + member.split('.')[-1]
        row = leftkan.pilot.check_signature(self.graph, {}, leftkan.pilot.U1 + member, target,
            prelude, directory, MECH, TIMEOUT, group=leftkan.GROUP,
            aliases=leftkan.aliases() if aliases is None else aliases, adapter=target)
        (directory / (member.replace('.', '-') + '.json')).write_text(json.dumps(row, indent=2) + '\n')
        if needle is not None:
            # The kernel must reject the changed field, not fail for an unrelated reason.
            self.assertEqual(row['code'], 'KERNEL_REJECTED', row)
            self.assertRegex(row['detail'], needle)
        return row

    def assert_signatures(self, directory, prelude, accepted=True):
        rows = leftkan.record_signatures(self.graph, prelude, directory, MECH, TIMEOUT)
        (directory / 'signatures.json').write_text(json.dumps(rows, indent=2) + '\n')
        self.assertEqual(len(rows), len(leftkan.MEMBERS))
        key, value = ('status', 'KERNEL_TYPE_MATCH') if accepted else ('code', 'KERNEL_REJECTED')
        self.assertTrue(all(row.get(key) == value for row in rows), rows)

    def test_all_signatures_and_support_match(self):
        self.assertEqual(len(self.report['signatures']), len(leftkan.MEMBERS))
        self.assertEqual(len(self.report['support']), 17)
        self.assertTrue(all(row['status'] == 'NAME_AND_TYPE'
                            for row in self.report['signatures'] + self.report['support']))
        self.assertEqual(self.report['regression']['exit'], 0)
        self.assertFalse(leftkan.computation_failed(self.report['computations']))
        self.assertEqual(self.report['prelude_audit']['exit'], 0)
        self.assertEqual(self.report['prelude_audit']['stdout'], 'PRELUDE-AXIOMS OK')
        # The kernel accepts one instance of the full adapter in a file.
        self.assertEqual(self.fixture.count('specialize MechSignatureLeftKan '), 1)
        self.assertEqual([row['fixture'] for row in self.report['closed']],
                         [Path(path).name for path in leftkan.CLOSED])
        for path, row in zip(leftkan.CLOSED, self.report['closed']):
            self.assertRegex((ROOT / path).read_text(),
                             r'\Aspecialize MechSignatureLeftKan \(\d+(, \d+){5}\) as ClosedAdapter\d+\n\Z')
            self.assertFalse(leftkan.computation_failed(row['computations']))

    def test_unproven_dependencies_are_refused(self):
        # Pinned from the import graph: the members that each blocked dependency must refuse.
        members = {leftkan.pilot.U1 + member for member in leftkan.MEMBERS}
        refusals = {
            'Eq': ('mk', 'fac', 'uniq', 'desc_unique'),
            leftkan.pilot.U1 + 'Functor.comp': ('mk', 'unit', 'desc', 'fac', 'uniq', 'desc_unique'),
            leftkan.pilot.U1 + 'NatTrans.whiskerRight': ('mk', 'fac', 'uniq', 'desc_unique')}
        for name, refused in refusals.items():
            # discharge_dependencies only changes KERNEL_TYPE_MATCH rows, so start every row there.
            rows = [dict(row, status='KERNEL_TYPE_MATCH')
                    for row in copy.deepcopy(self.report['signatures'] + self.report['support'])]
            next(row for row in rows if row['name'] == name).update(status='BLOCKED', code='KERNEL_REJECTED')
            leftkan.pilot.discharge_dependencies(rows)
            self.assertEqual({row['name'] for row in rows if row['name'] in members
                              and (row['status'], row.get('code')) == ('BLOCKED', 'UNPROVEN_DEPENDENCY')},
                             {leftkan.pilot.U1 + 'LeftKanExtension.' + member for member in refused}, name)

    def test_unmapped_dependencies_do_not_invoke_checker(self):
        for name in ('Functor.comp', 'NatTrans.whiskerRight'):
            aliases = leftkan.aliases()
            aliases = {key: value for key, value in aliases.items() if key[0] != leftkan.pilot.U1 + name}
            directory = self.evidence('unmapped-' + name.replace('.', '-'))
            with mock.patch.object(leftkan.pilot, 'invoke', side_effect=AssertionError('checker ran')) as invoke:
                row = self.signature(directory, 'LeftKanExtension.mk', self.prelude, aliases)
            self.assertEqual(row['code'], 'UNMAPPED_DEPENDENCY', row)
            invoke.assert_not_called()

    def test_outer_categories_remain_relevant(self):
        for member in leftkan.MEMBERS:
            name = member.split('.')[-1]
            start, end = self.span(self.adapter, name)
            block = self.adapter[start:end]
            self.assertEqual(block.count('(c :'), 2)
            altered = self.adapter[:start] + block.replace('(c :', '(0 c :') + self.adapter[end:]
            prelude = self.replace(self.prelude, self.adapter, altered)
            directory = self.evidence('outer-' + name)
            if name == 'desc_unique':
                # The body passes c to desc_unique_core, so the kernel refuses the erased binder.
                result = self.check(directory, 'prelude', prelude, False)
                self.assertRegex(result['stderr'] or result['stdout'], r'quantity: the erased binder ')
                continue
            self.check(directory, 'prelude', prelude)
            self.assertEqual(self.signature(directory, member, prelude)['status'], 'KERNEL_TYPE_MATCH')
            self.check(directory, 'quantity-pin', prelude + '\n' + self.fixture, False)
            self.check(directory, 'twin', prelude + '\n' + self.without(name + 'Quantity'))

    def test_nested_object_erasure_is_refused(self):
        pattern, erased = r'\(([bx][0-9]+) : C\)', r'(0 \1 : C)'
        altered, count = re.subn(pattern, erased, self.adapter)
        self.assertGreater(count, 0)
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence('nested-objects')
        self.check(directory, 'prelude', prelude)
        self.signature(directory, 'LeftKanExtension.mk', prelude,
                       needle=r'(?s)the term has type .* and the expected type is \(.*Ran SPi 0 b13 b0 ')
        self.check(directory, 'projection', prelude + '\n' + self.fixture, False)
        # With the same erasure in the fixture, the fixture checks. Thus the refusal comes from the erased binders.
        fixture, count = re.subn(pattern, erased, self.fixture)
        self.assertGreater(count, 0)
        self.check(directory, 'twin', prelude + '\n' + fixture)

    def test_extra_stored_field_is_refused(self):
        # Preserve all observable accessors while adding one hidden functor field.
        altered = self.replace(self.adapter, '(L, (unit, (desc, (fac, unique))))',
                               '(L, (L, (unit, (desc, (fac, unique)))))')
        start, end = self.span(altered, 'LeftKanExtension')
        block = altered[start:end]
        field = re.search(r'\(L : (.+)\) \*', block).group(1)
        block = self.replace(block, '(L : ' + field + ') *',
                             '(L : ' + field + ') *\n    (padding : ' + field + ') *')
        altered = altered[:start] + block + altered[end:]
        for name, projection in [('unit', '.2.1'), ('desc', '.2.2.1'),
                                 ('fac', '.2.2.2.1'), ('uniq', '.2.2.2.2')]:
            start, end = self.span(altered, name)
            block = self.replace(altered[start:end], 'self' + projection, 'self.2' + projection)
            altered = altered[:start] + block + altered[end:]
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence('stored-shape')
        self.check(directory, 'prelude', prelude)
        self.assert_signatures(directory, prelude)
        self.check(directory, 'shape-pin', prelude + '\n' + self.fixture, False)
        self.check(directory, 'twin', prelude + '\n' + self.without('storedShape'))

    def test_constructor_field_order_is_refused(self):
        start, end = self.span(self.adapter, 'mk')
        block = self.adapter[start:end]
        for separator in (' ->\n    ', '\n      '):
            unit = next(line for line in block.split(separator) if line.startswith('(unit :'))
            desc = next(line for line in block.split(separator) if line.startswith('(desc :'))
            old = unit + separator + desc
            block = self.replace(block, old, desc + separator + unit)
        altered = self.adapter[:start] + block + self.adapter[end:]
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence('constructor-order')
        self.check(directory, 'prelude', prelude)
        self.signature(directory, 'LeftKanExtension.mk', prelude,
                       needle=r'(?s)the term has type .* and the expected type is \(Ran SPi w b10 ')

    def arguments(self, term):
        parts, depth, current = [], 0, ''
        for char in term:
            if char == ' ' and depth == 0:
                if current: parts.append(current); current = ''
            else:
                current += char
                depth += (char == '(') - (char == ')')
        if current: parts.append(current)
        return parts

    def uncommented(self, text):
        return '\n'.join(line for line in text.split('\n') if not line.lstrip().startswith('--'))

    def double_symmetry(self, name):
        # The field type applies facAt or uniqAt. Unfold that predicate to get the equation.
        start, end = self.span(self.adapter, name)
        block = self.adapter[start:end]
        typ, body = block.split(' :=\n', 1)
        predicate, *values = self.arguments(typ.split(' ->\n    ')[-1].strip()[1:-1])
        first, last = self.span(self.adapter, predicate)
        binders, equation = self.uncommented(self.adapter[first:last]).split(' :=\n', 1)[1].split('=>\n    ', 1)
        parameters = re.findall(r"\((?:0 )?([A-Za-z_][A-Za-z0-9_']*) :", binders)
        self.assertEqual(len(parameters), len(values), predicate)
        binding = dict(zip(parameters, values))
        parts = [re.sub(r"[A-Za-z_][A-Za-z0-9_']*", lambda token: binding.get(token.group(0), token.group(0)), part)
                 for part in self.arguments(equation.strip()[1:-1])]
        self.assertEqual(len(parts), 4)
        equality, domain, left, right = parts
        lambdas, projection = body.rsplit('=>\n    ', 1)
        projection = self.uncommented(projection).strip()
        changed = (f'{equality}_symm {domain} {right} {left} '
                   f'({equality}_symm {domain} {left} {right} ({projection}))')
        altered = self.adapter[:start] + typ + ' :=\n' + lambdas + '=>\n    ' + changed + '\n' + self.adapter[end:]
        return self.replace(self.prelude, self.adapter, altered)

    def test_fac_computation_is_pinned(self):
        prelude = self.double_symmetry('fac')
        directory = self.evidence('fac-computation')
        self.check(directory, 'prelude', prelude)
        self.assert_signatures(directory, prelude)
        self.check(directory, 'projection', prelude + '\n' + self.fixture, False)
        self.check(directory, 'twin', prelude + '\n' + self.without('facProjection'))

    def test_uniqueness_computation_is_pinned(self):
        prelude = self.double_symmetry('uniq')
        directory = self.evidence('uniqueness-computation')
        self.check(directory, 'prelude', prelude)
        self.assert_signatures(directory, prelude)
        self.check(directory, 'projection', prelude + '\n' + self.fixture, False)
        self.check(directory, 'twin', prelude + '\n' + self.without('uniqProjection'))

    def test_hom_universes_are_pinned(self):
        # Rename one hom universe after the group header. The record universe also names z and q,
        # so the middle and target cases change the record type and have no twin.
        head, body = self.adapter.split(' where\n', 1)
        for label, old, new in (('source', 'v', 'q'), ('middle', 'z', 'v'), ('target', 'q', 'v')):
            changed, count = re.subn(r'\b' + old + r'\b', new, body)
            self.assertGreater(count, 0, label)
            prelude = self.replace(self.prelude, self.adapter, head + ' where\n' + changed)
            directory = self.evidence(label + '-hom-level')
            self.check(directory, 'prelude', prelude)
            self.assert_signatures(directory, prelude, label == 'source')
            self.check(directory, 'level-pin', prelude + '\n' + self.fixture, False)
            if label == 'source':
                self.check(directory, 'twin', prelude + '\n' + self.without('sourceHomLevel'))

    def test_copied_families_match_gated_adapters(self):
        source = (ROOT / leftkan.COMPOSITION_SOURCE).read_text()
        copies = leftkan.copy_checks(source)
        self.assertEqual(self.report['copies'], copies)
        self.assertEqual(len(copies), 26)
        self.assertEqual(set(copies.values()), {'MATCH'})
        # Swap the two sides of one copied naturality equation. Only that copy can differ.
        start, end = self.span(source, 'Second_naturality')
        self.assertEqual(source[start:end].count('\n    (Target_comp '), 2)
        head, left, rest = source[start:end].split('\n    (Target_comp ', 2)
        right, tail = rest.split(' :=', 1)
        block = head + '\n    (Target_comp ' + right + '\n    (Target_comp ' + left + ' :=' + tail
        checks = leftkan.copy_checks(source[:start] + block + source[end:])
        self.assertEqual({name for name, status in checks.items() if status != 'MATCH'}, {'Second_naturality'})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mech', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()
    MECH, REPORT, TIMEOUT = args.mech.resolve(), args.report.resolve(), args.timeout
    if TIMEOUT < 1: parser.error('timeout must be positive')
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(LeftKanControls))
    leftkan.functor.write_report(REPORT.parent / 'controls.json', {
        'tests': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
        'passed': result.wasSuccessful()})
    raise SystemExit(0 if result.wasSuccessful() else 1)
