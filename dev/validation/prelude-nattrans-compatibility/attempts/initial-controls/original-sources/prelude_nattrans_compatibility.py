#!/usr/bin/env python3
"""Reject incompatible NatTrans records and altered projection computations."""

import argparse
import copy
import importlib.util
import json
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nattrans_compatibility", ROOT / "dev/prelude-nattrans-compatibility.py")
nattrans = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nattrans)
MECH = None
REPORT = None
TIMEOUT = 180


class NatTransControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text())
        for path, expected in cls.report["inputs"].items():
            if nattrans.pilot.digest(ROOT / path) != expected:
                raise AssertionError("input changed: " + path)
        for path, expected in cls.report["binaries"].items():
            if nattrans.pilot.digest(path) != expected:
                raise AssertionError("checker changed: " + path)
        if str(MECH) not in cls.report["binaries"]:
            raise AssertionError("checker not hashed: " + str(MECH))
        if nattrans.pilot.digest(REPORT.parent / "import/types.ndjson") != cls.report["import_graph_sha256"]:
            raise AssertionError("import graph changed")
        for row in cls.report["signatures"] + cls.report["support"]:
            if nattrans.pilot.digest(REPORT.parent / row["fixture"]) != row["fixture_sha256"]:
                raise AssertionError("signature fixture changed: " + row["name"])
        if nattrans.pilot.digest(REPORT.parent / cls.report["computation_fixture"]) != cls.report["computation_sha256"]:
            raise AssertionError("computation fixture changed")
        cls.graph = nattrans.pilot.read_graph(REPORT.parent / "import/types.ndjson")
        cls.prelude = nattrans.source()
        cls.fixture = (ROOT / nattrans.COMPUTATIONS).read_text()
        cls.adapter = (ROOT / nattrans.NATTRANS_SOURCE).read_text()

    def evidence(self, label):
        path = REPORT.parent / "controls" / label
        path.mkdir(parents=True, exist_ok=False)
        return path

    def replace(self, text, old, new):
        self.assertEqual(text.count(old), 1, old)
        return text.replace(old, new)

    def span(self, text, name):
        start = text.index("\n\ndef " + name + " :")
        ends = (text.find("\n\ndef ", start + 2), text.index("\nend\n", start))
        return start, min(index for index in ends if index > 0)

    def without(self, *names):
        text = self.fixture
        for name in names:
            start, end = self.span(text, name)
            text = text[:start] + text[end:]
        return text

    def check(self, directory, name, text, accepted=True):
        path = directory / (name + ".mech")
        path.write_text(text)
        result = nattrans.functor.run_fixture(MECH, "check", path, TIMEOUT)
        nattrans.functor.write_report(directory / (name + ".json"), result)
        self.assertEqual(result["exit"], 0 if accepted else 1, result)
        if not accepted:
            self.assertIn("the term has type", result["stderr"] or result["stdout"])
        return result

    def signature(self, directory, member, prelude, group=None, aliases=None):
        target = "Candidate_" + member.split(".")[-1]
        row = nattrans.pilot.check_signature(
            self.graph, {}, nattrans.pilot.U1 + member, target, prelude, directory, MECH, TIMEOUT,
            group=group or nattrans.GROUP, aliases=nattrans.aliases() if aliases is None else aliases,
            adapter=target)
        nattrans.functor.write_report(directory / (member + ".json"), row)
        return row

    def test_all_signatures_and_support_match(self):
        self.assertEqual({row["name"] for row in self.report["signatures"]},
                         {nattrans.pilot.U1 + member for member in nattrans.MEMBERS})
        self.assertEqual(len(self.report["signatures"]), 4)
        self.assertEqual({row["name"] for row in self.report["support"]},
                         {"Eq", *(nattrans.pilot.U1 + member for member in
                                  ("Category", "Category.Hom", "Category.id", "Category.comp",
                                   *nattrans.functor.MEMBERS))})
        self.assertEqual(len(self.report["support"]), 11)
        self.assertTrue(all(row["status"] == "NAME_AND_TYPE" for row in
                            self.report["signatures"] + self.report["support"]))
        self.assertEqual(self.report["counts"], {"NAME_AND_TYPE": 4})
        self.assertEqual([row["exit"] for row in self.report["computations"]], [0, 0, 0])
        self.assertEqual(self.report["computations"][1]["stdout"], "")
        self.assertEqual(self.report["computations"][2]["stdout"], "PRELUDE-AXIOMS OK")

    def test_unproven_dependencies_are_refused(self):
        rows = copy.deepcopy(self.report["signatures"] + self.report["support"])
        for row in rows:
            row["status"] = "KERNEL_TYPE_MATCH"
        rows = [row for row in rows if row["name"] != "Eq"]
        nattrans.pilot.discharge_dependencies(rows)
        by_name = {row["name"]: row for row in rows}
        for member in ("NatTrans.mk", "NatTrans.naturality"):
            self.assertEqual((by_name[nattrans.pilot.U1 + member]["status"],
                              by_name[nattrans.pilot.U1 + member]["code"]),
                             ("BLOCKED", "UNPROVEN_DEPENDENCY"))
        directory = self.evidence("unproven-equality")
        nattrans.functor.write_report(directory / "result.json", rows)
        for label, key in (("missing-equality", ("Eq", ("(succ u3)",))),
                           ("missing-functor", (nattrans.pilot.U1 + "Functor", ("u0", "u1", "u2", "u3")))):
            aliases = nattrans.aliases()
            del aliases[key]
            directory = self.evidence(label)
            with mock.patch.object(nattrans.pilot, "invoke", side_effect=AssertionError("checker must not run")):
                row = self.signature(directory, "NatTrans.mk", self.prelude, aliases=aliases)
            self.assertEqual((row["status"], row["code"]), ("BLOCKED", "UNMAPPED_DEPENDENCY"))
            self.assertNotIn("checker_exit", row)

    def test_outer_categories_and_functors_remain_relevant(self):
        for member, pin in (("NatTrans", "typeQuantities"), ("mk", "constructorQuantities"),
                            ("app", "appQuantities"), ("naturality", "naturalityQuantities")):
            for binder in ("c : Base_Source_Category C", "G : Base_Functor C D c d"):
                start, end = self.span(self.adapter, member)
                chunk = self.adapter[start:end]
                self.assertEqual(chunk.count("(" + binder + ")"), 2)
                chunk = chunk.replace("(" + binder + ")", "(0 " + binder + ")")
                altered = self.adapter[:start] + chunk + self.adapter[end:]
                prelude = self.replace(self.prelude, self.adapter, altered)
                directory = self.evidence("erased-" + member + "-" + binder[0])
                self.check(directory, "prelude", prelude)
                qualified = member if member == "NatTrans" else "NatTrans." + member
                row = self.signature(directory, qualified, prelude)
                self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
                self.check(directory, "quantity-pin", prelude + "\n" + self.fixture, False)
                self.check(directory, "twin", prelude + "\n" + self.without(pin))

    def test_nested_object_erasure_is_refused(self):
        altered = self.adapter.replace("(x : C)", "(0 x : C)").replace("(y : C)", "(0 y : C)")
        self.assertNotEqual(altered, self.adapter)
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence("erased-nested-objects")
        self.check(directory, "prelude", prelude)
        row = self.signature(directory, "NatTrans.mk", prelude)
        self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED"), row)
        self.check(directory, "computation", prelude + "\n" + self.fixture, False)
        twin = self.fixture.replace("(x : C)", "(0 x : C)").replace("(y : C)", "(0 y : C)")
        self.check(directory, "twin", prelude + "\n" + twin)

    def test_object_universe_swap_is_refused(self):
        directory = self.evidence("swapped-object-universes")
        group = nattrans.GROUP.replace("(u0, u1, u2, u3)", "(u2, u1, u0, u3)")
        row = self.signature(directory, "NatTrans.mk", self.prelude, group=group)
        self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED"), row)
        twin = self.evidence("original-object-universes")
        row = self.signature(twin, "NatTrans.mk", self.prelude)
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)

    def reverse_squares(self, text, prefix):
        spans = []
        cursor = 0
        needle = "(" + prefix + "Target_comp D d\n"
        while True:
            start = text.find(needle, cursor)
            if start < 0:
                break
            depth = 1
            end = start + 1
            while depth:
                self.assertLess(end, len(text))
                depth += int(text[end] == "(") - int(text[end] == ")")
                end += 1
            spans.append((start, end))
            cursor = end
        self.assertGreater(len(spans), 0)
        self.assertEqual(len(spans) % 2, 0)
        for index in reversed(range(0, len(spans), 2)):
            (a, b), (c, d) = spans[index:index + 2]
            text = text[:a] + text[c:d] + text[b:c] + text[a:b] + text[d:]
        return text

    def test_reversed_naturality_square_is_refused(self):
        altered = self.reverse_squares(self.adapter, "Base_")
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence("reversed-square")
        self.check(directory, "prelude", prelude)
        for member in nattrans.MEMBERS:
            row = self.signature(directory, member, prelude)
            expected = "BLOCKED" if member in ("NatTrans.mk", "NatTrans.naturality") else "KERNEL_TYPE_MATCH"
            self.assertEqual(row["status"], expected, row)
            if expected == "BLOCKED":
                self.assertEqual(row["code"], "KERNEL_REJECTED")
        self.check(directory, "computation", prelude + "\n" + self.fixture, False)
        self.check(directory, "twin", prelude + "\n" + self.reverse_squares(self.fixture, "N_Base_"))

    def test_same_type_app_must_preserve_computation(self):
        altered = self.replace(self.adapter, "(alpha : NatTrans C D c d F G) => alpha.1\n",
                               "(alpha : NatTrans C D c d F G) (x : C) =>\n"
                               "    Base_Target_comp D d\n"
                               "      (Base_obj C D c d F x) (Base_obj C D c d G x) (Base_obj C D c d G x)\n"
                               "      (alpha.1 x) (Base_Target_id D d (Base_obj C D c d G x))\n")
        # The law retains its stored components while the accessor is perturbed.
        altered = altered.replace("(app C D c d F G alpha y)", "(alpha.1 y)")
        altered = altered.replace("(app C D c d F G alpha x)", "(alpha.1 x)")
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence("altered-app")
        self.check(directory, "prelude", prelude)
        for member in nattrans.MEMBERS:
            row = self.signature(directory, member, prelude)
            self.assertEqual(row["status"], "BLOCKED" if member == "NatTrans.naturality" else "KERNEL_TYPE_MATCH", row)
            if member == "NatTrans.naturality":
                self.assertEqual(row["code"], "KERNEL_REJECTED")
        fixture = self.without("naturalityProjection", "naturalityQuantities")
        self.check(directory, "projection", prelude + "\n" + fixture, False)
        self.check(directory, "twin", prelude + "\n" + self.without(
            "componentProjection", "naturalityProjection", "naturalityQuantities"))

    def test_same_type_naturality_must_preserve_computation(self):
        hom = "(Base_Target_Hom D d (Base_obj C D c d F x) (Base_obj C D c d G y))"
        left = ("(Base_Target_comp D d (Base_obj C D c d F x) (Base_obj C D c d F y) "
                "(Base_obj C D c d G y) (Base_map C D c d F x y f) (app C D c d F G alpha y))")
        right = ("(Base_Target_comp D d (Base_obj C D c d F x) (Base_obj C D c d G x) "
                 "(Base_obj C D c d G y) (app C D c d F G alpha x) (Base_map C D c d G x y f))")
        altered = self.replace(self.adapter, "(alpha : NatTrans C D c d F G) => alpha.2\n",
                               "(alpha : NatTrans C D c d F G) =>\n"
                               "    fun (x : C) (y : C) (f : Base_Source_Hom C c x y) =>\n"
                               "      Base_Target_Eq_symm " + hom + " " + right + " " + left + "\n"
                               "        (Base_Target_Eq_symm " + hom + " " + left + " " + right +
                               " (alpha.2 x y f))\n")
        prelude = self.replace(self.prelude, self.adapter, altered)
        directory = self.evidence("altered-naturality")
        self.check(directory, "prelude", prelude)
        for member in nattrans.MEMBERS:
            row = self.signature(directory, member, prelude)
            self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
        self.check(directory, "projection", prelude + "\n" + self.fixture, False)
        self.check(directory, "twin", prelude + "\n" + self.without("naturalityProjection"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mech", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    MECH, REPORT, TIMEOUT = args.mech.resolve(), args.report.resolve(), args.timeout
    if TIMEOUT < 1:
        parser.error("timeout must be positive")
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NatTransControls))
    nattrans.functor.write_report(REPORT.parent / "controls.json", {
        "tests": result.testsRun, "failures": len(result.failures), "errors": len(result.errors),
        "passed": result.wasSuccessful()})
    raise SystemExit(0 if result.wasSuccessful() else 1)
