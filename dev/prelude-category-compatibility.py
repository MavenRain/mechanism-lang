#!/usr/bin/env python3
"""Check the source-compatible Category record and its projection computations."""

import argparse
import collections
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("compatibility_pilot", ROOT / "dev/prelude-compatibility-pilot.py")
pilot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pilot)

CATEGORY_SOURCE = "prelude/compatibility/category.mech"
COMPUTATIONS = "test/fixtures/prelude/compatibility-category.mech"
MEMBERS = ("Category", "Category.mk", "Category.Hom", "Category.id", "Category.comp",
           "Category.comp_id", "Category.id_comp", "Category.assoc")
GROUP = "  specialize MechSignatureCategory (u0, u1) as Candidate"


def inputs():
    return (pilot.EQUALITY_SOURCE, CATEGORY_SOURCE, COMPUTATIONS,
            "dev/denominators.json", "dev/prelude-compatibility-pilot.py",
            "dev/prelude-category-compatibility.py", "test/prelude_category_compatibility.py")


def source():
    return "\n".join((ROOT / path).read_text() for path in (pilot.EQUALITY_SOURCE, CATEGORY_SOURCE))


def aliases(prefix="Candidate"):
    result = {(pilot.U1 + "Category", ("u0", "u1")): prefix + "_Category",
              ("Eq", ("(succ u1)",)): "Candidate_Eq"}
    for member in ("Hom", "id", "comp"):
        result[(pilot.U1 + "Category." + member, ("u0", "u1"))] = prefix + "_" + member
    return result


def run_fixture(mech, command, path, timeout, label=None):
    result = pilot.invoke(mech, command, path, timeout)
    stem = str(path.with_suffix("")) + "." + (label or command)
    Path(stem + ".stdout").write_text(result.stdout)
    Path(stem + ".stderr").write_text(result.stderr)
    return {"command": command, "exit": result.returncode,
            "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}


def check_equality(graph, prelude, directory, mech, timeout):
    declaration = graph["declaration"]["Eq"]
    renderer = pilot.Renderer(graph, {}, declaration["parameters"])
    if len(renderer.parameters) != 1:
        raise pilot.Blocked("EQUALITY_SCOPE_DRIFT", "expected one Eq universe parameter")
    renderer.parameters = {name: "(succ u1)" for name in renderer.parameters}
    expected = renderer.expression(declaration["type"])
    body = renderer.witness(declaration["type"], "Candidate_Eq")
    path = directory / "contextual-equality.mech"
    path.write_text(prelude + "\npoly (u0, u1) group ContextualEquality where\n" + GROUP
                    + f"\ndef witness : {expected} := {body}\nend\n")
    result = run_fixture(mech, "check", path, timeout)
    return {"name": "Eq", "adapter": "Candidate_Eq", "scope": "type-signature-at-morphism-universe",
            "specialization": "succ u1", "dependencies": declaration["dependencies"],
            "fixture": path.name, "fixture_sha256": pilot.digest(path),
            "status": "KERNEL_TYPE_MATCH" if result["exit"] == 0 else "BLOCKED",
            "code": ("CHECKED" if result["exit"] == 0 else
                     "KERNEL_REJECTED" if result["exit"] == 1 else "CHECKER_FAILURE"), "checker": result}


def write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", type=Path, default=Path("/Users/oobi/Documents/kanon-m2-corpus/corpus/lean-parity/uat/uat.export"))
    parser.add_argument("--mech", type=Path, default=ROOT / "_bend2/bin/mech.exe")
    parser.add_argument("--audit", type=Path, default=ROOT / "_bend2/test/prelude.exe")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("timeout must be positive")
    args.export, args.mech, args.audit, args.out = (args.export.resolve(), args.mech.resolve(),
                                                    args.audit.resolve(), args.out.resolve())
    expected = json.loads((ROOT / "dev/denominators.json").read_text())["uat_export_sha256"]
    if pilot.digest(args.export) != expected:
        parser.error("export differs from the frozen UAT denominator")
    args.out.mkdir(parents=True, exist_ok=False)
    try:
        input_hashes = {path: pilot.digest(ROOT / path) for path in inputs()}
        binaries = {str(path): pilot.digest(path) for path in
                    (args.mech, args.mech.parent / "mechanism-native",
                     args.audit, args.audit.parent / "bend_protocol.js")}
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
        support = [check_equality(graph, prelude, args.out, args.mech, args.timeout)]
        rows = []
        for member in MEMBERS:
            target = "Candidate_" + member.split(".")[-1]
            rows.append(pilot.check_signature(graph, {}, pilot.U1 + member, target, prelude,
                                              args.out, args.mech, args.timeout, group=GROUP,
                                              aliases=aliases(), adapter=target))
        pilot.discharge_dependencies(rows + support)
        path = args.out / "projection-computations.mech"
        path.write_text(prelude + "\n" + (ROOT / COMPUTATIONS).read_text())
        computations = [run_fixture(args.mech, command, path, args.timeout) for command in ("check", "axioms")]
        computations.append(run_fixture(args.audit, "--audit", path, args.timeout, "audit"))
        report = {"version": 1, "gate_passed": False,
                  "scope": "eight Category signatures with a source-compatible record adapter and projection computations",
                  "export_sha256": expected, "import_graph_sha256": graph_hash,
                  "inputs": input_hashes, "binaries": binaries, "signatures": rows, "support": support,
                  "counts": dict(collections.Counter(row["status"] for row in rows)),
                  "computation_fixture": path.name, "computation_sha256": pilot.digest(path),
                  "computations": computations}
        report_path = args.out / "report.json"
        write_report(report_path, report)
        if any(row["status"] != "NAME_AND_TYPE" for row in rows + support):
            raise pilot.Blocked("SIGNATURE_FAILED", "Category signature or dependency did not match")
        if (any(row["exit"] != 0 for row in computations) or computations[1]["stdout"]
                or computations[2]["stdout"] != "PRELUDE-AXIOMS OK"):
            raise pilot.Blocked("COMPUTATION_FAILED", "projection check or axiom audit failed")
        controls = subprocess.run([sys.executable, "-P", str(ROOT / "test/prelude_category_compatibility.py"),
                                   "--mech", str(args.mech), "--report", str(report_path),
                                   "--timeout", str(args.timeout)], capture_output=True, text=True, cwd=ROOT,
                                  timeout=args.timeout * 16)
        (args.out / "controls.stdout").write_text(controls.stdout)
        (args.out / "controls.stderr").write_text(controls.stderr)
        report["controls"] = json.loads((args.out / "controls.json").read_text())
        write_report(report_path, report)
        if controls.returncode or not report["controls"]["passed"] or report["controls"]["tests"] != 8:
            raise pilot.Blocked("CONTROL_FAILED", controls.stderr.strip() or controls.stdout.strip())
        if pilot.digest(args.export) != expected or pilot.digest(imported / "types.ndjson") != graph_hash:
            raise pilot.Blocked("IMPORT_CHANGED", "export or imported graph changed during checking")
        if any(pilot.digest(ROOT / path) != value for path, value in input_hashes.items()):
            raise pilot.Blocked("INPUT_CHANGED", "Category compatibility inputs changed during checking")
        if any(pilot.digest(path) != value for path, value in binaries.items()):
            raise pilot.Blocked("CHECKER_CHANGED", "checker changed during checking")
        report["gate_passed"] = True
        write_report(report_path, report)
        print(f"PRELUDE-CATEGORY-COMPATIBILITY signatures={len(rows)} support={len(support)} "
              f"computations=3 audits=2 controls={report['controls']['tests']} OK")
        print(f"Evidence: {args.out}", file=sys.stderr)
        return 0
    except (pilot.Blocked, subprocess.TimeoutExpired, OSError, ValueError, KeyError) as error:
        write_report(args.out / "failure.json", {"code": getattr(error, "code", "CATEGORY_ERROR"), "detail": str(error)})
        print("PRELUDE-CATEGORY-COMPATIBILITY FAIL " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
