#!/usr/bin/env python3
"""Check the imported LeftKanExtension record and its composition dependencies."""

import argparse
import collections
import copy
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nattrans_compatibility", ROOT / "dev/prelude-nattrans-compatibility.py")
nattrans = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nattrans)
functor = nattrans.functor
pilot = nattrans.pilot

COMPOSITION_SOURCE = "prelude/compatibility/composition.mech"
WHISKERING_SOURCE = "prelude/compatibility/whiskering.mech"
LEFT_KAN_SOURCE = "prelude/compatibility/left-kan.mech"
COMPUTATIONS = "test/fixtures/prelude/compatibility-left-kan.mech"
CLOSED = ("test/fixtures/prelude/compatibility-left-kan-closed-0.mech",
          "test/fixtures/prelude/compatibility-left-kan-closed-1.mech",
          "test/fixtures/prelude/compatibility-left-kan-closed-2.mech")
REGRESSIONS = (*nattrans.REGRESSIONS, nattrans.COMPUTATIONS,
               "test/fixtures/prelude/compatibility-composition.mech",
               "test/fixtures/prelude/compatibility-whiskering.mech")
MEMBERS = ("LeftKanExtension", "LeftKanExtension.mk", "LeftKanExtension.functor",
           "LeftKanExtension.unit", "LeftKanExtension.desc", "LeftKanExtension.fac",
           "LeftKanExtension.uniq", "LeftKanExtension.desc_unique")
GROUP = "  specialize MechSignatureLeftKan (u0, u1, u2, u3, u4, u5) as Candidate"
CONTROL_TESTS = 11
CONTROL_CALLS = 51 + 6 * len(MEMBERS)
COPIED_FAMILIES = (("First", ("Source", "Middle"), ("u", "v", "w", "z"), False),
                   ("Second", ("Middle", "Target"), ("w", "z", "p", "q"), True),
                   ("Composite", ("Source", "Target"), ("u", "v", "p", "q"), True))
TOKEN = re.compile(r"[A-Za-z_][A-Za-z0-9_']*")


def inputs():
    return (pilot.EQUALITY_SOURCE, functor.CATEGORY_SOURCE, functor.FUNCTOR_SOURCE,
            nattrans.NATTRANS_SOURCE, COMPOSITION_SOURCE, WHISKERING_SOURCE, LEFT_KAN_SOURCE,
            COMPUTATIONS, *CLOSED, *REGRESSIONS, "dev/denominators.json",
            "dev/prelude-compatibility-pilot.py", "dev/prelude-functor-compatibility.py",
            "dev/prelude-nattrans-compatibility.py", "dev/prelude-left-kan-compatibility.py",
            "test/prelude_left_kan_compatibility.py")


def source():
    return "\n".join([nattrans.source(), *((ROOT / path).read_text() for path in
                       (COMPOSITION_SOURCE, WHISKERING_SOURCE, LEFT_KAN_SOURCE))])


def definitions(text):
    """Map each top-level `def NAME :` block to its text without comments and with collapsed whitespace."""
    result = {}
    for match in re.finditer(r"^def (\S+) :", text, re.M):
        ends = [index for index in (text.find("\ndef ", match.start() + 1), text.find("\nend\n", match.start()))
                if index >= 0]
        block = text[match.start():min(ends, default=len(text))]
        result[match.group(1)] = " ".join(re.sub(r"--[^\n]*", "", block).split())
    return result


def renamed(text, mapping):
    return TOKEN.sub(lambda match, mapping=mapping: mapping.get(match.group(0), match.group(0)), text)


def family_renaming(family, edges, levels, functor_definitions, nattrans_definitions):
    """Return the token renamings that turn the gated Functor and NatTrans adapters into one copied family."""
    universes = dict(zip(("u", "v", "w", "z"), levels))
    functor_map, nattrans_map = dict(universes), dict(universes)
    tokens = set(TOKEN.findall(" ".join([*functor_definitions.values(), *nattrans_definitions.values()])))
    for token in sorted(tokens):
        for side, edge in zip(("Source_", "Target_"), edges):
            if token.startswith(side):
                functor_map[token] = edge + token[len(side) - 1:]
                nattrans_map["Base_" + token] = edge + token[len(side) - 1:]
            if token.startswith("Base_" + side):
                nattrans_map[token] = edge + token[len("Base_" + side) - 1:]
    for name in functor_definitions:
        functor_map[name] = family + "_Base_" + name
        nattrans_map["Base_" + name] = family + "_Base_" + name
    for name in nattrans_definitions:
        nattrans_map[name] = family + "_" + name
    return functor_map, nattrans_map


def copy_checks(composition=None):
    """Compare each Functor and NatTrans copy in composition.mech with its renamed gated adapter."""
    if composition is None:
        composition = (ROOT / COMPOSITION_SOURCE).read_text()
    copies = definitions(composition)
    functor_definitions = definitions((ROOT / functor.FUNCTOR_SOURCE).read_text())
    nattrans_definitions = definitions((ROOT / nattrans.NATTRANS_SOURCE).read_text())
    result = {}
    for family, edges, levels, has_nattrans in COPIED_FAMILIES:
        functor_map, nattrans_map = family_renaming(family, edges, levels, functor_definitions,
                                                    nattrans_definitions)
        expected = {family + "_Base_" + name: renamed(text, functor_map)
                    for name, text in functor_definitions.items()}
        if has_nattrans:
            expected.update({family + "_" + name: renamed(text, nattrans_map)
                             for name, text in nattrans_definitions.items()})
        for name, text in expected.items():
            result[name] = "MISSING"
            if name in copies:
                result[name] = "MATCH" if copies[name] == text else "DIFFERS"
    return result


def composition_aliases(prefix="Candidate", levels=("u0", "u1", "u2", "u3", "u4", "u5")):
    result = {}
    first, second, composite = levels[:4], levels[2:], (*levels[:2], *levels[4:])
    for edge, pair in (("First", first), ("Second", second), ("Composite", composite)):
        result.update(nattrans.aliases(prefix + "_" + edge, pair))
    for side, pair in (("Source", levels[:2]), ("Middle", levels[2:4]), ("Target", levels[4:])):
        for member in ("Category", "Category.Hom", "Category.id", "Category.comp"):
            result[(pilot.U1 + member, pair)] = prefix + "_" + side + "_" + member.split(".")[-1]
    for level, side in ((levels[1], "Source"), (levels[3], "Middle"), (levels[5], "Target")):
        result[("Eq", ("(succ " + level + ")",))] = prefix + "_" + side + "_Eq"
    result[(pilot.U1 + "Functor.comp", levels)] = prefix + "_comp"
    return result


def whiskering_aliases(prefix="Candidate", levels=("u0", "u1", "u2", "u3", "u4", "u5")):
    result = composition_aliases(prefix + "_Base", (*levels[4:], *levels[:4]))
    result[(pilot.U1 + "NatTrans.whiskerRight", levels)] = prefix + "_whiskerRight"
    return result


def aliases(prefix="Candidate", levels=("u0", "u1", "u2", "u3", "u4", "u5")):
    result = composition_aliases(prefix + "_Base", levels)
    result[(pilot.U1 + "NatTrans.whiskerRight", (*levels[2:], *levels[:2]))] = prefix + "_Base_whiskerRight"
    for member in MEMBERS:
        result[(pilot.U1 + member, levels)] = prefix + "_" + member.split(".")[-1]
    return result


def reused_support(graph, directory, mech, report_path):
    """Reuse the committed NatTrans checks only when their complete inputs still match."""
    if report_path is None or not report_path.is_file():
        return None
    report = json.loads(report_path.read_text())
    if report.get("version") != 1 or not report.get("gate_passed"):
        return None
    expected = json.loads((ROOT / "dev/denominators.json").read_text())["uat_export_sha256"]
    if report.get("export_sha256") != expected or report.get("import_graph_sha256") != pilot.digest(directory / "import/types.ndjson"):
        return None
    if set(report["inputs"]) != set(nattrans.inputs()):
        return None
    for path, expected_hash in report["inputs"].items():
        if pilot.digest(ROOT / path) != expected_hash:
            return None
    for path in (mech, mech.parent / "mechanism-native"):
        if report["binaries"].get(str(path)) != pilot.digest(path):
            return None
    rows = copy.deepcopy(report["support"] + report["signatures"])
    required = {"Eq", *(pilot.U1 + name for name in
        ("Category", "Category.Hom", "Category.id", "Category.comp", *functor.MEMBERS, *nattrans.MEMBERS))}
    if len(rows) != 15 or {row["name"] for row in rows} != required:
        return None
    for row in rows:
        if (row["status"] != "NAME_AND_TYPE"
                or row.get("checker_exit", row.get("checker", {}).get("exit")) != 0):
            return None
        if row["dependencies"] != graph["declaration"][row["name"]]["dependencies"]:
            return None
        fixture = report_path.parent / row["fixture"]
        if pilot.digest(fixture) != row["fixture_sha256"]:
            return None
    for row in rows:
        for suffix in (".mech", ".stdout", ".stderr", ".check.stdout", ".check.stderr"):
            filename = Path(row["fixture"]).stem + suffix
            src = report_path.parent / filename
            if src.is_file():
                (directory / filename).write_bytes(src.read_bytes())
        row.update(status="KERNEL_TYPE_MATCH", reused_from=str(report_path),
                   reuse_report_sha256=pilot.digest(report_path))
    return rows


def support_signatures(graph, prelude, directory, mech, timeout, support_report=None):
    dependency_prelude = nattrans.source()
    rows = reused_support(graph, directory, mech, support_report)
    if rows is None:
        rows = nattrans.support_signatures(graph, dependency_prelude, directory, mech, timeout)
        for member in nattrans.MEMBERS:
            target = "Candidate_" + member.split(".")[-1]
            rows.append(pilot.check_signature(
                graph, {}, pilot.U1 + member, target, dependency_prelude, directory, mech, timeout,
                group=nattrans.GROUP, aliases=nattrans.aliases(), adapter=target))
    for member, schema, member_aliases in (
            ("Functor.comp", "MechSignatureComposition", composition_aliases()),
            ("NatTrans.whiskerRight", "MechSignatureWhiskering", whiskering_aliases())):
        target = "Candidate_" + member.split(".")[-1]
        dependency_source = dependency_prelude + "\n" + (ROOT / COMPOSITION_SOURCE).read_text()
        if member == "NatTrans.whiskerRight":
            dependency_source += "\n" + (ROOT / WHISKERING_SOURCE).read_text()
        rows.append(pilot.check_signature(
            graph, {}, pilot.U1 + member, target, dependency_source, directory, mech, timeout,
            group=f"  specialize {schema} (u0, u1, u2, u3, u4, u5) as Candidate",
            aliases=member_aliases, adapter=target))
    return rows


def record_signatures(graph, prelude, directory, mech, timeout):
    """Check every imported LeftKanExtension type in one symbolic instance of the record."""
    rows, witnesses = [], []
    for member in MEMBERS:
        name = pilot.U1 + member
        declaration = graph["declaration"][name]
        target = "Candidate_" + member.split(".")[-1]
        renderer = pilot.Renderer(graph, {}, declaration["parameters"], aliases())
        expected = renderer.expression(declaration["type"])
        body = renderer.witness(declaration["type"], target)
        witness = "witness_" + member.replace(".", "_")
        witnesses.append(f"def {witness} : {expected} := {body}\n")
        rows.append({"name": name, "target": target, "adapter": target,
                     "scope": "type-signature", "source_line": declaration["source_line"],
                     "universes": [entry["name"] for entry in declaration["parameters"]],
                     "dependencies": declaration["dependencies"], "witness": witness})
    path = directory / "left-kan-signatures.mech"
    path.write_text(prelude + "\npoly (u0, u1, u2, u3, u4, u5) group CompatibilitySignatures where\n"
                    + GROUP + "\n" + "\n".join(witnesses) + "end\n")
    seconds = timeout * len(MEMBERS)
    try:
        checked = pilot.invoke(mech, "check", path, seconds)
    except (subprocess.TimeoutExpired, pilot.Blocked) as error:
        if getattr(error, "code", "CHECK_TIMEOUT") != "CHECK_TIMEOUT":
            raise
        raise pilot.Blocked("CHECK_TIMEOUT", f"{path.name} exceeded {seconds} s") from error
    path.with_suffix(".stdout").write_text(checked.stdout)
    path.with_suffix(".stderr").write_text(checked.stderr)
    if checked.returncode not in (0, 1):
        raise pilot.Blocked("CHECKER_FAILURE", f"{path.name}: checker exit {checked.returncode}")
    for row in rows:
        row.update(fixture=path.name, fixture_sha256=pilot.digest(path), checker_exit=checked.returncode)
        if checked.returncode:
            row.update(status="BLOCKED", code="KERNEL_REJECTED", detail=checked.stderr.strip() or checked.stdout.strip())
        else:
            row.update(status="KERNEL_TYPE_MATCH", code="CHECKED")
    return rows


def computation_rows(mech, path, timeout):
    return [functor.run_fixture(mech, command, path, timeout) for command in ("check", "axioms")]


def computation_failed(rows):
    return bool([row["command"] for row in rows] != ["check", "axioms"]
                or any(row["exit"] != 0 for row in rows) or rows[1]["stdout"])


def closed_instance(prelude, fixture, directory, mech, timeout):
    # The kernel accepts one instance of the full adapter in a file.
    # Thus each closed instance has its own file.
    path = directory / Path(fixture).name
    path.write_text(prelude + "\n" + (ROOT / fixture).read_text())
    return {"fixture": path.name, "fixture_sha256": pilot.digest(path),
            "computations": computation_rows(mech, path, timeout)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, default=Path("/Users/oobi/Documents/kanon-m2-corpus/corpus/lean-parity/uat/uat.export"))
    parser.add_argument("--mech", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--audit", type=Path, default=ROOT / "_bend2/test/prelude.exe")
    parser.add_argument("--audit-artifact", type=Path,
                        default=ROOT / "_bend2/test/bend_protocol.js")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--support-report", type=Path,
                        default=ROOT / "dev/validation/prelude-nattrans-compatibility/report.json")
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("timeout must be positive")
    args.export, args.mech, args.out = args.export.resolve(), args.mech.resolve(), args.out.resolve()
    args.audit, args.audit_artifact = args.audit.resolve(), args.audit_artifact.resolve()
    expected = json.loads((ROOT / "dev/denominators.json").read_text())["uat_export_sha256"]
    if pilot.digest(args.export) != expected:
        parser.error("export differs from the frozen UAT denominator")
    args.out.mkdir(parents=True, exist_ok=False)
    try:
        input_hashes = {path: pilot.digest(ROOT / path) for path in inputs()}
        binaries = {str(path): pilot.digest(path) for path in
                    (args.mech, args.mech.parent / "mechanism-native", args.audit, args.audit_artifact)}
        imported = args.out / "import"
        result = subprocess.run([str(args.mech), "import", str(args.export), "--out", str(imported)],
                                capture_output=True, text=True, timeout=args.timeout, cwd=ROOT)
        (args.out / "import.stdout").write_text(result.stdout)
        (args.out / "import.stderr").write_text(result.stderr)
        if result.returncode:
            raise pilot.Blocked("IMPORT_FAILED", result.stderr.strip() or result.stdout.strip())
        graph = pilot.read_graph(imported / "types.ndjson")
        if len(graph["declaration"]) != 3202:
            raise pilot.Blocked("DENOMINATOR_DRIFT", "expected 3202 imported declarations")
        graph_hash = pilot.digest(imported / "types.ndjson")
        prelude = source()
        copies = copy_checks()
        drift = sorted(name for name, status in copies.items() if status != "MATCH")
        if drift:
            raise pilot.Blocked("COPY_DRIFT", COMPOSITION_SOURCE + " copies differ from the gated adapters: "
                                + ", ".join(drift))
        support = support_signatures(graph, prelude, args.out, args.mech, args.timeout, args.support_report.resolve())
        (args.out / "support.json").write_text(json.dumps(support, indent=2) + "\n")
        rows = record_signatures(graph, prelude, args.out, args.mech, args.timeout)
        (args.out / "signatures.json").write_text(json.dumps(rows, indent=2) + "\n")
        pilot.discharge_dependencies(rows + support)
        path = args.out / "projection-computations.mech"
        path.write_text(prelude + "\n" + (ROOT / COMPUTATIONS).read_text())
        computations = computation_rows(args.mech, path, args.timeout)
        audit = functor.run_fixture(args.audit, "--audit", path, args.timeout, "audit")
        closed = [closed_instance(prelude, fixture, args.out, args.mech, args.timeout)
                  for fixture in CLOSED]
        regressions = args.out / "dependency-projection-regressions.mech"
        regressions.write_text("\n".join([prelude, *((ROOT / fixture).read_text() for fixture in REGRESSIONS)]))
        regression = functor.run_fixture(args.mech, "check", regressions, args.timeout)
        report = {"version": 1, "gate_passed": False,
                  "scope": ("eight LeftKanExtension signatures, seventeen support signatures, twenty-six copied"
                            " Functor and NatTrans definitions, stored shape and projection computations,"
                            " three closed adapter instances, with Category, Functor and NatTrans regressions"
                            " and the empty-environment prelude audit"),
                  "export_sha256": expected, "import_graph_sha256": graph_hash,
                  "inputs": input_hashes, "binaries": binaries, "copies": copies,
                  "signatures": rows, "support": support,
                  "counts": dict(collections.Counter(row["status"] for row in rows)),
                  "computation_fixture": path.name, "computation_sha256": pilot.digest(path),
                  "computations": computations, "closed": closed, "prelude_audit": audit,
                  "regression_fixture": regressions.name,
                  "regression_sha256": pilot.digest(regressions), "regression": regression}
        report_path = args.out / "report.json"
        functor.write_report(report_path, report)
        if any(row["status"] != "NAME_AND_TYPE" for row in rows + support):
            raise pilot.Blocked("SIGNATURE_FAILED", "LeftKanExtension signature or dependency did not match")
        if computation_failed(computations) or any(computation_failed(row["computations"]) for row in closed):
            raise pilot.Blocked("COMPUTATION_FAILED", "projection check, closed instance or mech axioms failed")
        if audit["exit"] != 0 or audit["stdout"] != "PRELUDE-AXIOMS OK":
            raise pilot.Blocked("PRELUDE_AUDIT_FAILED", "empty-environment prelude audit failed")
        if regression["exit"] != 0:
            raise pilot.Blocked("REGRESSION_FAILED", "dependency projections failed with the LeftKanExtension prelude")
        controls = subprocess.run(
            [sys.executable, "-P", str(ROOT / "test/prelude_left_kan_compatibility.py"),
             "--mech", str(args.mech), "--report", str(report_path), "--timeout", str(args.timeout)],
            capture_output=True, text=True, cwd=ROOT, timeout=args.timeout * CONTROL_CALLS)
        (args.out / "controls.stdout").write_text(controls.stdout)
        (args.out / "controls.stderr").write_text(controls.stderr)
        report["controls"] = json.loads((args.out / "controls.json").read_text())
        functor.write_report(report_path, report)
        if (controls.returncode or not report["controls"]["passed"]
                or report["controls"]["tests"] != CONTROL_TESTS):
            raise pilot.Blocked("CONTROL_FAILED", controls.stderr.strip() or controls.stdout.strip())
        if pilot.digest(args.export) != expected or pilot.digest(imported / "types.ndjson") != graph_hash:
            raise pilot.Blocked("IMPORT_CHANGED", "export or imported graph changed during checking")
        if any(pilot.digest(ROOT / path) != value for path, value in input_hashes.items()):
            raise pilot.Blocked("INPUT_CHANGED", "LeftKanExtension compatibility inputs changed during checking")
        if any(pilot.digest(path) != value for path, value in binaries.items()):
            raise pilot.Blocked("CHECKER_CHANGED", "checker changed during checking")
        report["gate_passed"] = True
        functor.write_report(report_path, report)
        print(f"PRELUDE-LEFT-KAN-COMPATIBILITY signatures={len(rows)} support={len(support)} "
              f"computations=2 audits=2 prelude-audit=CHECKED closed={len(closed)} regressions=1 "
              f"controls={report['controls']['tests']} OK")
        print(f"Evidence: {args.out}", file=sys.stderr)
        return 0
    except (pilot.Blocked, subprocess.TimeoutExpired, OSError, ValueError, KeyError) as error:
        functor.write_report(args.out / "failure.json", {
            "code": getattr(error, "code", "LEFT_KAN_ERROR"), "detail": str(error)})
        print("PRELUDE-LEFT-KAN-COMPATIBILITY FAIL " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
