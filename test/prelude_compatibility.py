#!/usr/bin/env python3
"""Adversarial controls for the bounded prelude compatibility pilot."""

import argparse
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("compatibility_pilot", ROOT / "dev/prelude-compatibility-pilot.py")
pilot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pilot)
MECH = None
REPORT = None


def tiny_graph():
    return {"level": {0: {"zero": True}}, "expr": {0: {"sort": 0}}, "declaration": {}}


class GraphControls(unittest.TestCase):
    def blocked(self, code, operation):
        with self.assertRaises(pilot.Blocked) as error:
            operation()
        self.assertEqual(error.exception.code, code)

    def test_unbound_universe_is_not_zero(self):
        graph = tiny_graph()
        graph["level"][1] = {"param": {"name": "missing"}}
        self.blocked("UNBOUND_UNIVERSE", lambda: pilot.Renderer(graph, {}).level(1))

    def test_universe_argument_is_not_discarded(self):
        graph = tiny_graph()
        graph["declaration"]["Nat"] = {"parameters": []}
        node = {"name": "Nat", "universes": [0]}
        self.blocked("UNIVERSE_SPECIALIZATION_REQUIRED",
                     lambda: pilot.Renderer(graph, {"Nat": "MechNat"}).constant(node))

    def test_symbolic_instances_remain_distinct(self):
        graph = tiny_graph()
        graph["level"].update({1: {"param": {"name": "left"}}, 2: {"param": {"name": "right"}}})
        aliases = {("Category", ("u0",)): "Source_Category", ("Category", ("u1",)): "Target_Category"}
        renderer = pilot.Renderer(graph, {}, [{"name": "left"}, {"name": "right"}], aliases)
        self.assertEqual(renderer.constant({"name": "Category", "universes": [1]}), "Source_Category")
        self.assertEqual(renderer.constant({"name": "Category", "universes": [2]}), "Target_Category")

    def test_missing_dependency_is_not_invented(self):
        self.blocked("UNMAPPED_DEPENDENCY", lambda: pilot.Renderer(tiny_graph(), {}).constant({"name": "Not", "universes": []}))

    def test_projection_is_not_replaced_with_a_variable(self):
        graph = tiny_graph()
        graph["expr"][1] = {"proj": {"typeName": "Pair", "idx": 1, "struct": 0}}
        self.blocked("UNSUPPORTED_EXPRESSION", lambda: pilot.Renderer(graph, {}).expression(1))

    def test_escaping_bound_variable_is_rejected(self):
        graph = tiny_graph()
        graph["expr"][1] = {"bvar": 1}
        self.blocked("UNBOUND_VARIABLE", lambda: pilot.Renderer(graph, {}).expression(1, ("x",)))

    def test_cyclic_graph_has_a_work_limit(self):
        graph = tiny_graph()
        graph["expr"][1] = {"app": {"fn": 1, "arg": 0}}
        self.blocked("RENDER_LIMIT", lambda: pilot.Renderer(graph, {}).expression(1))

    def test_duplicate_graph_node_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "types.ndjson"
            path.write_text('{"expr":0,"sort":0}\n{"expr":0,"bvar":0}\n')
            self.blocked("DUPLICATE_NODE", lambda: pilot.read_graph(path))

    def test_dependency_failure_propagates(self):
        rows = [{"name": "A", "status": "BLOCKED", "dependencies": []},
                {"name": "B", "status": "KERNEL_TYPE_MATCH", "dependencies": ["A"]},
                {"name": "C", "status": "KERNEL_TYPE_MATCH", "dependencies": ["B"]}]
        pilot.discharge_dependencies(rows)
        self.assertEqual([row["status"] for row in rows], ["BLOCKED"] * 3)
        self.assertEqual(rows[2]["code"], "UNPROVEN_DEPENDENCY")

    def test_dependency_cycle_cannot_certify_itself(self):
        rows = [{"name": "A", "status": "KERNEL_TYPE_MATCH", "dependencies": ["B"]},
                {"name": "B", "status": "KERNEL_TYPE_MATCH", "dependencies": ["A"]}]
        pilot.discharge_dependencies(rows)
        self.assertEqual([row["status"] for row in rows], ["BLOCKED", "BLOCKED"])

    def test_source_binder_names_cannot_inject_target_code(self):
        graph = tiny_graph()
        graph["expr"].update({1: {"bvar": 0}, 2: {"forallE": {"name": "x); axiom forged : Prop", "type": 0, "body": 1}}})
        rendered = pilot.Renderer(graph, {}).expression(2)
        self.assertNotIn("forged", rendered)
        self.assertIn("b0", rendered)


@unittest.skipUnless(MECH is not None and REPORT is not None, "provide --mech and --report for kernel controls")
class KernelControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        report = json.loads(REPORT.read_text())
        if pilot.digest(REPORT.parent / "import/types.ndjson") != report["import_graph_sha256"]:
            raise AssertionError("imported graph changed after the pilot")
        for path, expected in report["inputs"].items():
            if pilot.digest(ROOT / path) != expected:
                raise AssertionError("pilot input changed: " + path)
        if pilot.digest(MECH.resolve().parent / "mechanism-native") != report["native_sha256"]:
            raise AssertionError("native checker changed after the pilot")

    def test_all_candidates_and_support_match(self):
        report = json.loads(REPORT.read_text())
        self.assertEqual(len(report["candidates"]), 19)
        self.assertEqual(report["counts"], {"NAME_AND_TYPE": 19})
        self.assertEqual(report["support_counts"], {"NAME_AND_TYPE": 1})
        self.assertEqual(report["support"][0]["name"], "Not")
        self.assertEqual({row["name"] for row in report["candidates"] if "adapter" in row},
                         pilot.EQUALITY)

    def equality_control(self, name, label, diagnostic, swap_levels=False, wrong_helper=False):
        graph = pilot.read_graph(REPORT.parent / "import/types.ndjson")
        mappings = pilot.mappings_from(ROOT / "map/prelude.map.tsv")
        source, group, aliases, adapter = pilot.equality_inputs(name)
        prelude = (ROOT / "prelude/init.mech").read_text() + "\n" + source
        evidence = REPORT.parent / "controls" / label

        def check(directory, adapter):
            directory.mkdir(parents=True, exist_ok=True)
            row = pilot.check_signature(graph, mappings, name, mappings[name], prelude,
                                        directory, MECH, 30, group=group,
                                        aliases=aliases, adapter=adapter)
            (directory / "result.json").write_text(json.dumps(row, indent=2) + "\n")
            return row

        if swap_levels:
            # The expected type keeps its aliases; only the adapter uses swapped levels.
            group += "\n  specialize " + group.split()[1] + " (u1, u0) as Swapped"
            twin = check(evidence / "well-formed", adapter)
            self.assertEqual(twin["status"], "KERNEL_TYPE_MATCH", twin.get("detail"))
            adapter = adapter.replace("Candidate_", "Swapped_", 1)
        if wrong_helper:
            adapter = "Candidate_symm"
        row = check(evidence, adapter)
        self.assertEqual(row["status"], "BLOCKED")
        self.assertEqual(row["code"], "KERNEL_REJECTED")
        self.assertIn(diagnostic, row["detail"])

    def test_eliminator_universes_cannot_be_swapped(self):
        self.equality_control("Eq.rec", "swapped-elimination",
                              "mismatch: the term has type Type u1 and the expected type is Type u0",
                              swap_levels=True)

    def test_congruence_universes_cannot_be_swapped(self):
        self.equality_control("congrArg", "swapped-congruence",
                              "mismatch: the term has type Type u0 and the expected type is Type u1",
                              swap_levels=True)

    def test_wrong_equality_helper_fails(self):
        self.equality_control("Eq.trans", "wrong-equality-helper",
                              "mismatch: the term has type b0 and the expected type is",
                              wrong_helper=True)

    def test_not_mapping_is_required(self):
        graph = pilot.read_graph(REPORT.parent / "import/types.ndjson")
        mappings = pilot.mappings_from(ROOT / "map/prelude.map.tsv")
        prelude = (ROOT / "prelude/init.mech").read_text() + "\n" + (ROOT / pilot.NOT_SOURCE).read_text()
        evidence = REPORT.parent / "controls/missing-not"
        evidence.mkdir(parents=True, exist_ok=True)
        row = pilot.check_signature(graph, mappings, "Decidable.isFalse", "mechIsFalse",
                                    prelude, evidence, MECH, 30)
        (evidence / "result.json").write_text(json.dumps(row, indent=2) + "\n")
        self.assertEqual(row["code"], "UNMAPPED_DEPENDENCY")
        self.assertEqual(row["detail"], "Not")

    def test_closed_equality_instances_check_without_axioms(self):
        evidence = REPORT.parent / "controls/closed-equality"
        evidence.mkdir(parents=True, exist_ok=True)
        source = "\n".join((ROOT / path).read_text() for path in
                           ("prelude/init.mech", pilot.EQUALITY_SOURCE,
                            "test/fixtures/prelude/compatibility-equality.mech"))
        path = evidence / "closed.mech"
        path.write_text(source)
        checked = pilot.invoke(MECH, "check", path, 30)
        axioms = pilot.invoke(MECH, "axioms", path, 30)
        for command, result in (("check", checked), ("axioms", axioms)):
            (evidence / (command + ".stdout")).write_text(result.stdout)
            (evidence / (command + ".stderr")).write_text(result.stderr)
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(axioms.stdout.strip(), "")

    def test_wrong_constructor_target_fails(self):
        directory = REPORT.parent
        graph = pilot.read_graph(directory / "import/types.ndjson")
        mappings = pilot.mappings_from(ROOT / "map/prelude.map.tsv")
        prelude = (ROOT / "prelude/init.mech").read_text()
        evidence = directory / "controls/wrong-target"
        evidence.mkdir(parents=True, exist_ok=True)
        row = pilot.check_signature(graph, mappings, "Bool.false", "mechZero", prelude,
                                    evidence, MECH, 30)
        (evidence / "result.json").write_text(json.dumps(row, indent=2) + "\n")
        self.assertEqual(row["status"], "BLOCKED")
        self.assertEqual(row["code"], "KERNEL_REJECTED")
        self.assertIn("mechZero is not a constructor of MechBool", row["detail"])

    def test_correct_constructor_target_checks(self):
        directory = REPORT.parent
        graph = pilot.read_graph(directory / "import/types.ndjson")
        mappings = pilot.mappings_from(ROOT / "map/prelude.map.tsv")
        prelude = (ROOT / "prelude/init.mech").read_text()
        evidence = directory / "controls/correct-constructor"
        evidence.mkdir(parents=True, exist_ok=True)
        row = pilot.check_signature(graph, mappings, "Nat.succ", "mechSucc", prelude,
                                    evidence, MECH, 30)
        (evidence / "result.json").write_text(json.dumps(row, indent=2) + "\n")
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mech", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    MECH, REPORT = args.mech, args.report
    # Decorators evaluate at import time; the command line enables these controls.
    KernelControls.__unittest_skip__ = MECH is None or REPORT is None
    suite = unittest.defaultTestLoader.loadTestsFromModule(__import__(__name__))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)
