#!/usr/bin/env python3
"""Controls that separate Category signature matching from projection behavior."""

import argparse
import copy
import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("category_compatibility", ROOT / "dev/prelude-category-compatibility.py")
category = importlib.util.module_from_spec(spec)
spec.loader.exec_module(category)
MECH = None
REPORT = None
TIMEOUT = 180
RIGHT_FIELD = ("    (right : (x : Obj) -> (y : Obj) -> (f : hom x y) ->\n"
               "      Eq (hom x y) (compose x y y f (identity y)) f) *\n")
LEFT_FIELD = ("    (left : (x : Obj) -> (y : Obj) -> (f : hom x y) ->\n"
              "      Eq (hom x y) (compose x x y (identity x) f) f) *\n")


def altered_accessor(prefix, member, universe):
    common = f"(0 Obj : Sort (succ {universe})) -> (record : {prefix}_Category Obj) -> "
    if member == "id":
        type_tail = f"(x : Obj) -> {prefix}_Hom Obj record x x"
        binders = f"(0 Obj : Sort (succ {universe})) (record : {prefix}_Category Obj) (x : Obj)"
        body = f"{prefix}_comp Obj record x x x ({prefix}_id Obj record x) ({prefix}_id Obj record x)"
    else:
        type_tail = (f"(x : Obj) -> (y : Obj) -> (z : Obj) -> "
                     f"{prefix}_Hom Obj record x y -> {prefix}_Hom Obj record y z -> {prefix}_Hom Obj record x z")
        binders = (f"(0 Obj : Sort (succ {universe})) (record : {prefix}_Category Obj) "
                   f"(x : Obj) (y : Obj) (z : Obj) (f : {prefix}_Hom Obj record x y) "
                   f"(g : {prefix}_Hom Obj record y z)")
        body = (f"{prefix}_comp Obj record x x z ({prefix}_id Obj record x) "
                f"({prefix}_comp Obj record x y z f g)")
    return f"def altered{member} : {common}{type_tail} := fun {binders} => {body}\n"


class CategoryControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text())
        for path, expected in cls.report["inputs"].items():
            if category.pilot.digest(ROOT / path) != expected:
                raise AssertionError("input changed: " + path)
        for path, expected in cls.report["binaries"].items():
            if category.pilot.digest(path) != expected:
                raise AssertionError("checker changed: " + path)
        if category.pilot.digest(REPORT.parent / "import/types.ndjson") != cls.report["import_graph_sha256"]:
            raise AssertionError("import graph changed")
        for row in cls.report["signatures"] + cls.report["support"]:
            if category.pilot.digest(REPORT.parent / row["fixture"]) != row["fixture_sha256"]:
                raise AssertionError("signature fixture changed: " + row["name"])
        if category.pilot.digest(REPORT.parent / cls.report["computation_fixture"]) != cls.report["computation_sha256"]:
            raise AssertionError("computation fixture changed")
        cls.graph = category.pilot.read_graph(REPORT.parent / "import/types.ndjson")
        cls.prelude = category.source()
        cls.fixture = (ROOT / category.COMPUTATIONS).read_text()

    def evidence(self, label):
        path = REPORT.parent / "controls" / label
        path.mkdir(parents=True, exist_ok=False)
        return path

    def replace(self, text, old, new, count=1):
        self.assertEqual(text.count(old), count, old)
        return text.replace(old, new)

    def swap(self, text, first, second):
        self.assertEqual((text.count(first), text.count(second)), (1, 1), (first, second))
        return second.join(part.replace(second, first) for part in text.split(first))

    def check_text(self, directory, name, text):
        path = directory / name
        path.write_text(text)
        return category.run_fixture(MECH, "check", path, TIMEOUT)

    def accept(self, directory, name, text, record):
        result = self.check_text(directory, name, text)
        category.write_report(directory / record, result)
        self.assertEqual(result["exit"], 0, result)

    def reject_computations(self, directory, prelude, fixture=None):
        result = self.check_text(directory, "computations.mech",
                                 prelude + "\n" + (self.fixture if fixture is None else fixture))
        category.write_report(directory / "result.json", result)
        self.assertEqual(result["exit"], 1, result)
        self.assertIn("mismatch", result["stderr"] or result["stdout"])

    def test_all_signatures_and_contextual_equality_match(self):
        self.assertEqual({row["name"] for row in self.report["signatures"]},
                         {category.pilot.U1 + member for member in category.MEMBERS})
        self.assertEqual(self.report["counts"], {"NAME_AND_TYPE": 8})
        self.assertEqual([(row["name"], row["status"], row["specialization"]) for row in self.report["support"]],
                         [("Eq", "NAME_AND_TYPE", "succ u1")])

    def test_projection_computations_check_without_axioms(self):
        self.assertEqual([row["exit"] for row in self.report["computations"]], [0, 0, 0])
        self.assertEqual(self.report["computations"][1]["stdout"], "")
        self.assertEqual(self.report["computations"][2]["stdout"], "PRELUDE-AXIOMS OK")

    def test_constructor_proof_fields_cannot_be_swapped(self):
        mutated = self.swap(self.prelude, RIGHT_FIELD, LEFT_FIELD)
        mutated = self.replace(mutated, "(compose, (right, (left, associativity)))",
                               "(compose, (left, (right, associativity)))")
        mutated = self.swap(mutated, "=> category.2.2.2.1\n", "=> category.2.2.2.2.1\n")
        directory = self.evidence("swapped-proof-fields")
        self.accept(directory, "prelude.mech", mutated, "prelude.json")
        self.reject_computations(directory, mutated)

    def test_nested_object_quantities_are_preserved(self):
        mutated = self.replace(self.prelude, "(hom : (x : Obj) -> (y : Obj) -> Sort (succ v))",
                               "(hom : (0 x : Obj) -> (0 y : Obj) -> Sort (succ v))", 3)
        mutated = self.replace(mutated, "(x : Obj) -> (y : Obj) -> Sort (succ v) :=",
                               "(0 x : Obj) -> (0 y : Obj) -> Sort (succ v) :=")
        directory = self.evidence("erased-nested-objects")
        self.accept(directory, "prelude.mech", mutated, "prelude.json")
        row = category.pilot.check_signature(self.graph, {}, category.pilot.U1 + "Category.mk", "Candidate_mk",
                                             mutated, directory, MECH, TIMEOUT, group=category.GROUP,
                                             aliases=category.aliases(), adapter="Candidate_mk")
        category.write_report(directory / "result.json", row)
        self.assertEqual(row["code"], "KERNEL_REJECTED", row)
        self.assertIn("the expected type is (Ran SPi 0 x b0 (Ran SPi 0 y b0 Type (u1 + 1)))", row["detail"])

    def level_row(self, label, prefix, levels):
        directory = self.evidence(label)
        row = category.pilot.check_signature(self.graph, {}, category.pilot.U1 + "Category.Hom",
                                             "Candidate_Hom", self.prelude, directory, MECH, TIMEOUT,
                                             group=category.GROUP + f"\n  specialize MechSignatureCategory {levels} as {prefix}",
                                             aliases=category.aliases(prefix), adapter=prefix + "_Hom")
        category.write_report(directory / "result.json", row)
        return row

    def test_symbolic_object_and_morphism_levels_cannot_be_swapped(self):
        twin = self.level_row("same-level-twin", "Twin", "(u0, u1)")
        self.assertEqual(twin["status"], "KERNEL_TYPE_MATCH", twin)
        row = self.level_row("swapped-universes", "Swapped", "(u1, u0)")
        self.assertEqual(row["code"], "KERNEL_REJECTED", row)
        self.assertIn("the term has type Type (u0 + 1) and the expected type is Type (u1 + 1)", row["detail"])

    def altered_behavior(self, member):
        directory = self.evidence("altered-" + member + "-signature")
        row = category.pilot.check_signature(self.graph, {}, category.pilot.U1 + "Category." + member,
                                             "Candidate_" + member, self.prelude, directory, MECH, TIMEOUT,
                                             group=category.GROUP + "\n" + altered_accessor("Candidate", member, "u0"),
                                             aliases=category.aliases(), adapter="altered" + member)
        category.write_report(directory / "result.json", row)
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
        anchor = "specialize MechSignatureEq ((succ (succ v))) as Types\n"
        fixture = self.replace(self.fixture, anchor, anchor + altered_accessor("C", member, "u"))
        self.accept(self.evidence("altered-" + member + "-helper-only"), "computations.mech",
                    self.prelude + "\n" + fixture, "result.json")
        fixture = self.replace(fixture, "C_" + member + " Obj (C_mk", "altered" + member + " Obj (C_mk")
        self.reject_computations(self.evidence("altered-" + member + "-computation"), self.prelude, fixture)

    def test_same_type_identity_adapter_must_preserve_computation(self):
        self.altered_behavior("id")

    def test_same_type_composition_adapter_must_preserve_computation(self):
        self.altered_behavior("comp")

    def test_missing_equality_dependency_blocks_constructor_and_laws(self):
        rows = copy.deepcopy(self.report["signatures"])
        for row in rows:
            row["status"] = "KERNEL_TYPE_MATCH"
        category.pilot.discharge_dependencies(rows)
        by_name = {row["name"]: row for row in rows}
        self.assertEqual(by_name[category.pilot.U1 + "Category.Hom"]["status"], "NAME_AND_TYPE")
        for member in ("mk", "comp_id", "id_comp", "assoc"):
            row = by_name[category.pilot.U1 + "Category." + member]
            self.assertEqual(row["status"], "BLOCKED")
            self.assertEqual(row["code"], "UNPROVEN_DEPENDENCY")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mech", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    MECH, REPORT, TIMEOUT = args.mech.resolve(), args.report.resolve(), args.timeout
    if TIMEOUT < 1:
        parser.error("timeout must be positive")
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CategoryControls)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    (REPORT.parent / "controls.json").write_text(json.dumps({"tests": result.testsRun,
        "passed": result.wasSuccessful(), "failures": len(result.failures), "errors": len(result.errors)}, indent=2) + "\n")
    raise SystemExit(0 if result.wasSuccessful() else 1)
