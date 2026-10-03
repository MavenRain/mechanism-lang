#!/usr/bin/env python3
"""Refuse incompatible Functor signatures and projection behavior."""

import argparse
import copy
import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("functor_compatibility", ROOT / "dev/prelude-functor-compatibility.py")
functor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(functor)
MECH = None
REPORT = None
TIMEOUT = 180


class FunctorControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text())
        for path, expected in cls.report["inputs"].items():
            if functor.pilot.digest(ROOT / path) != expected:
                raise AssertionError("input changed: " + path)
        for path, expected in cls.report["binaries"].items():
            if functor.pilot.digest(path) != expected:
                raise AssertionError("checker changed: " + path)
        if str(MECH) not in cls.report["binaries"]:
            raise AssertionError("checker not hashed: " + str(MECH))
        if functor.pilot.digest(REPORT.parent / "import/types.ndjson") != cls.report["import_graph_sha256"]:
            raise AssertionError("import graph changed")
        for row in cls.report["signatures"] + cls.report["support"]:
            if functor.pilot.digest(REPORT.parent / row["fixture"]) != row["fixture_sha256"]:
                raise AssertionError("signature fixture changed: " + row["name"])
        if functor.pilot.digest(REPORT.parent / cls.report["computation_fixture"]) != cls.report["computation_sha256"]:
            raise AssertionError("computation fixture changed")
        cls.graph = functor.pilot.read_graph(REPORT.parent / "import/types.ndjson")
        cls.prelude = functor.source()
        cls.fixture = (ROOT / functor.COMPUTATIONS).read_text()
        cls.adapter = (ROOT / functor.FUNCTOR_SOURCE).read_text()

    def evidence(self, label):
        path = REPORT.parent / "controls" / label
        path.mkdir(parents=True, exist_ok=False)
        return path

    def replace(self, text, old, new, count=1):
        self.assertEqual(text.count(old), count, old)
        return text.replace(old, new)

    def check_text(self, directory, name, text):
        path = directory / name
        path.write_text(text)
        return functor.run_fixture(MECH, "check", path, TIMEOUT)

    def accept(self, directory, text):
        result = self.check_text(directory, "prelude.mech", text)
        functor.write_report(directory / "prelude.json", result)
        self.assertEqual(result["exit"], 0, result)

    def reject_computations(self, directory, prelude, needle, fixture=None, name="computations"):
        result = self.check_text(directory, name + ".mech", prelude + "\n" + (fixture or self.fixture))
        functor.write_report(directory / (name + ".json"), result)
        self.assertEqual(result["exit"], 1, result)
        self.assertIn(needle, result["stderr"] or result["stdout"])

    def accept_computations(self, directory, prelude, fixture):
        result = self.check_text(directory, "twin.mech", prelude + "\n" + fixture)
        functor.write_report(directory / "twin.json", result)
        self.assertEqual(result["exit"], 0, result)

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

    def erase_source(self, member):
        start, end = self.span(self.adapter, member)
        bare = int(member == "Functor")
        chunk = self.replace(self.adapter[start:end], "    Source_Category C -> Target_Category D ->\n",
                             "    (0 c : Source_Category C) -> Target_Category D ->\n", bare)
        chunk = self.replace(chunk, "(c : Source_Category C)", "(0 c : Source_Category C)", 2 - bare)
        return self.adapter[:start] + chunk + self.adapter[end:]

    def symmetric(self, field, binders, arguments, hom, left, right):
        return self.replace(self.adapter, "=> F." + field + "\n",
                            "=> fun " + binders + " =>\n    Target_Eq_symm " + " ".join((hom, right, left)) +
                            "\n      (Target_Eq_symm " + " ".join((hom, left, right)) +
                            " (F." + field + " " + arguments + "))\n")

    def signature(self, directory, member, prelude, group=None, aliases=None, adapter=None, output="result.json"):
        target = "Candidate_" + member.split(".")[-1]
        row = functor.pilot.check_signature(
            self.graph, {}, functor.pilot.U1 + member, target, prelude, directory, MECH, TIMEOUT,
            group=group or functor.GROUP, aliases=functor.aliases() if aliases is None else aliases,
            adapter=adapter or target)
        functor.write_report(directory / output, row)
        return row

    def equality(self, directory, prelude):
        row = functor.check_equality(self.graph, prelude, directory, MECH, TIMEOUT)
        functor.write_report(directory / "equality.json", row)
        return row

    def test_all_signatures_and_support_match(self):
        self.assertEqual({row["name"] for row in self.report["signatures"]},
                         {functor.pilot.U1 + member for member in functor.MEMBERS})
        self.assertEqual(len(self.report["signatures"]), 6)
        self.assertEqual({row["name"] for row in self.report["support"]},
                         {"Eq", *(functor.pilot.U1 + member for member in
                           ("Category", "Category.Hom", "Category.id", "Category.comp"))})
        self.assertEqual(len(self.report["support"]), 5)
        for row in self.report["signatures"] + self.report["support"]:
            self.assertEqual((row["status"], row["code"]), ("NAME_AND_TYPE", "CHECKED"))

    def test_projections_check_without_axioms(self):
        self.assertEqual([row["exit"] for row in self.report["computations"]], [0, 0, 0])
        self.assertEqual(self.report["computations"][1]["stdout"], "")
        self.assertEqual(self.report["computations"][2]["stdout"], "PRELUDE-AXIOMS OK")

    def test_consistent_law_reorder_breaks_stored_order(self):
        start = self.adapter.index("    (identities :")
        middle = self.adapter.index(" *\n", start)
        end = self.adapter.index("\n\ndef mk", middle)
        identity = self.adapter[start:middle].replace("    (identities : ", "    (", 1)
        composition = self.adapter[middle + 3:end]
        reordered = self.adapter[:start] + composition + " *\n" + identity + self.adapter[end:]
        reordered = self.replace(reordered,
                                 "(objects, (morphisms, (identities, composition)))",
                                 "(objects, (morphisms, (composition, identities)))")
        reordered = self.replace(reordered, "=> F.2.2.1\n", "=> F.2.2.2\n")
        position = reordered.rindex("=> F.2.2.2\n")
        reordered = reordered[:position] + reordered[position:].replace("=> F.2.2.2\n", "=> F.2.2.1\n", 1)
        prelude = self.replace(self.prelude, self.adapter, reordered)
        directory = self.evidence("reordered-laws")
        self.accept(directory, prelude)
        self.accept_computations(directory, prelude, self.without("storedIdentity", "storedComposition"))
        self.reject_computations(directory, prelude, "the term has type (Ran SPi w y C (Ran SPi w a C")
        self.reject_computations(directory, prelude, "the head of an application is not a function",
                                 self.without("storedIdentity"), "composition")
        boundary = self.adapter.index("\n\ndef mk")
        stored = self.adapter[:boundary]
        for old, new in (("(x : C) -> (y : C) -> Source_Hom", "(y : C) -> (x : C) -> Source_Hom"),
                         ("morphisms x a", "morphisms a x"),
                         ("(morphisms x y f) (morphisms y a g)", "(morphisms y x f) (morphisms a y g)")):
            stored = self.replace(stored, old, new)
        flipped = self.replace(stored + self.adapter[boundary:], "(objects, (morphisms, ",
                               "(objects, ((fun (y : C) (x : C) (f : Source_Hom C c x y) => morphisms x y f), ")
        flipped = self.replace(flipped, "(F : Functor C D c d) => F.2.1\n",
                               "(F : Functor C D c d) (x : C) (y : C) (f : Source_Hom C c x y) =>\n"
                               "    F.2.1 y x f\n")
        prelude = self.replace(self.prelude, self.adapter, flipped)
        directory = self.evidence("flipped-morphisms")
        self.accept(directory, prelude)
        self.accept_computations(directory, prelude, self.without("storedMorphisms"))
        self.reject_computations(directory, prelude,
                                 "the term has type (Out SPi w y C (APt w y) (Out SPi w x C (APt w x)")

    def test_nested_object_quantities_match_export(self):
        erased = self.adapter
        for name, count in (("x", 12), ("y", 8), ("a", 4)):
            erased = self.replace(erased, "(" + name + " : C)", "(0 " + name + " : C)", count)
        prelude = self.replace(self.prelude, self.adapter, erased)
        directory = self.evidence("erased-objects")
        self.accept(directory, prelude)
        row = self.signature(directory, "Functor.mk", prelude)
        self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED"))
        self.assertIn("and the expected type is (Ran SPi 0 x b0 (Ran SPi 0 y b0", row["detail"])
        erased = self.replace(self.adapter, "(c : Source_Category C)", "(0 c : Source_Category C)", 11)
        erased = self.replace(erased, "(d : Target_Category D)", "(0 d : Target_Category D)", 11)
        erased = self.replace(erased, "    Source_Category C -> Target_Category D ->\n",
                              "    (0 c : Source_Category C) -> (0 d : Target_Category D) ->\n")
        prelude = self.replace(self.prelude, self.adapter, erased)
        directory = self.evidence("erased-categories")
        self.accept(directory, prelude)
        needle = "the term has type (Ran SPi 0 C Type (u0 + 1) (Ran SPi 0 D Type (u2 + 1) (Ran SPi 0 c (Lan SPi"
        self.reject_computations(directory, prelude, needle)
        pins = ("functorQuantities", "mkQuantities", "objQuantities", "mapQuantities", "mapIdQuantities",
                "mapCompQuantities")
        self.accept_computations(directory, prelude, self.without(*pins))
        for member, pin in zip(functor.MEMBERS, pins):
            name = member.split(".")[-1]
            prelude = self.replace(self.prelude, self.adapter, self.erase_source(name))
            directory = self.evidence("erased-source-" + name)
            self.accept(directory, prelude)
            self.reject_computations(directory, prelude, needle)
            self.accept_computations(directory, prelude, self.without(pin))

    def test_object_universes_are_independent(self):
        directory = self.evidence("swapped-object-levels")
        twin = directory / "twin"
        twin.mkdir()
        row = self.signature(twin, "Functor.mk", self.prelude)
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
        row = self.signature(directory, "Functor.mk", self.prelude,
                             "  specialize MechSignatureFunctor (u2, u1, u0, u3) as Candidate")
        self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED"))
        self.assertIn("the term has type Type (u0 + 1) and the expected type is Type (u2 + 1)", row["detail"])

    def test_morphism_universes_are_independent(self):
        crossed = self.replace(self.adapter, "(u, v) as Source", "(u, z) as Source")
        crossed = self.replace(crossed, "(w, z) as Target", "(w, v) as Target")
        prelude = self.replace(self.prelude, self.adapter, crossed)
        directory = self.evidence("swapped-morphism-levels")
        self.accept(directory, prelude)
        row = self.signature(directory, "Functor.mk", prelude, output="mk.json")
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
        twin = directory / "twin"
        twin.mkdir()
        row = self.equality(twin, self.prelude)
        self.assertEqual(row["status"], "KERNEL_TYPE_MATCH", row)
        row = self.equality(directory, prelude)
        self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED"))
        self.assertIn("the term has type Type (u3 + 1) and the expected type is Type (u1 + 1)",
                      row["checker"]["stderr"] or row["checker"]["stdout"])

    def test_same_type_map_must_preserve_computation(self):
        old = "(F : Functor C D c d) => F.2.1"
        new = ("(F : Functor C D c d) (x : C) (y : C) (f : Source_Hom C c x y) =>\n"
               "    Target_comp D d (obj C D c d F x) (obj C D c d F y) (obj C D c d F y)\n"
               "      (F.2.1 x y f) (Target_id D d (obj C D c d F y))")
        altered = self.replace(self.adapter, old, new)
        prelude = self.replace(self.prelude, self.adapter, altered)
        # Keep the other accessors on the stored map so this adapter stays well typed.
        boundary = prelude.index("\ndef map_id :", prelude.index("group MechSignatureFunctor"))
        prelude = prelude[:boundary] + prelude[boundary:].replace("map C D c d F", "F.2.1")
        directory = self.evidence("altered-map")
        self.accept(directory, prelude)
        laws = ("Functor.map_id", "Functor.map_comp")
        for member in functor.MEMBERS:
            row = self.signature(directory, member, prelude, output=member + ".json")
            self.assertEqual((row["status"], row["code"]), ("BLOCKED", "KERNEL_REJECTED") if member in laws
                             else ("KERNEL_TYPE_MATCH", "CHECKED"), row)
        pins = ("mapIdQuantities", "mapCompQuantities")
        needle = "the term has type (Lan SMu F_Target_Eq ["
        self.accept_computations(directory, prelude, self.without("morphismProjection", *pins))
        self.reject_computations(directory, prelude, needle, self.without(*pins), "projection")
        self.reject_computations(directory, prelude, "the term has type (Ran SPi 0 C Type (u0 + 1) "
                                 "(Ran SPi 0 D Type (u2 + 1) (Ran SPi w c (Lan SPi",
                                 self.without("morphismProjection"), "pins")
        identity = self.symmetric("2.2.1", "(x : C)", "x", "(Target_Hom D d (obj C D c d F x) (obj C D c d F x))",
                                  "(map C D c d F x x (Source_id C c x))", "(Target_id D d (obj C D c d F x))")
        composition = self.symmetric(
            "2.2.2", "(x : C) (y : C) (a : C) (f : Source_Hom C c x y) (g : Source_Hom C c y a)", "x y a f g",
            "(Target_Hom D d (obj C D c d F x) (obj C D c d F a))", "(map C D c d F x a (Source_comp C c x y a f g))",
            "(Target_comp D d (obj C D c d F x) (obj C D c d F y) (obj C D c d F a)\n"
            "        (map C D c d F x y f) (map C D c d F y a g))")
        for label, adapter, projection, needle in (
                ("symmetric-map-id", identity, "identityProjection",
                 "the term has type (Lan SMu Proofs [(Out SPi w x C (APt w x) identities)]"),
                ("symmetric-map-comp", composition, "compositionProjection",
                 "the term has type (Lan SMu Proofs [(Out SPi w g (Out SPi w y C (APt w a)")):
            prelude = self.replace(self.prelude, self.adapter, adapter)
            directory = self.evidence(label)
            self.accept(directory, prelude)
            for member in functor.MEMBERS:
                row = self.signature(directory, member, prelude, output=member + ".json")
                self.assertEqual((row["status"], row["code"]), ("KERNEL_TYPE_MATCH", "CHECKED"), row)
            self.reject_computations(directory, prelude, needle)
            self.accept_computations(directory, prelude, self.without(projection))

    def test_missing_dependencies_are_refused(self):
        rows = copy.deepcopy(self.report["signatures"] + self.report["support"])
        rows = [row for row in rows if row["name"] != "Eq"]
        for row in rows:
            row["status"] = "KERNEL_TYPE_MATCH"
        functor.pilot.discharge_dependencies(rows)
        by_name = {row["name"]: row for row in rows}
        for member in ("Functor.mk", "Functor.map_id", "Functor.map_comp"):
            self.assertEqual((by_name[functor.pilot.U1 + member]["status"],
                              by_name[functor.pilot.U1 + member]["code"]),
                             ("BLOCKED", "UNPROVEN_DEPENDENCY"))
        for member in ("Functor", "Functor.obj", "Functor.map"):
            self.assertEqual(by_name[functor.pilot.U1 + member]["status"], "NAME_AND_TYPE")
        directory = self.evidence("unproven-equality")
        functor.write_report(directory / "result.json", rows)
        for label, key in (("missing-equality", ("Eq", ("(succ u3)",))),
                           ("missing-category", (functor.pilot.U1 + "Category", ("u0", "u1")))):
            aliases = functor.aliases()
            del aliases[key]
            directory = self.evidence(label)
            row = self.signature(directory, "Functor.mk", self.prelude, aliases=aliases)
            self.assertEqual((row["status"], row["code"]), ("BLOCKED", "UNMAPPED_DEPENDENCY"))
            self.assertNotIn("checker_exit", row)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mech", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    MECH, REPORT, TIMEOUT = args.mech.resolve(), args.report.resolve(), args.timeout
    if TIMEOUT < 1:
        parser.error("timeout must be positive")
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(FunctorControls))
    functor.write_report(REPORT.parent / "controls.json", {"tests": result.testsRun,
        "passed": result.wasSuccessful(), "failures": len(result.failures), "errors": len(result.errors)})
    raise SystemExit(0 if result.wasSuccessful() else 1)
