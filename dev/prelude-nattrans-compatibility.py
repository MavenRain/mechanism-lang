#!/usr/bin/env python3
"""Check the imported NatTrans signatures, stored shape and projection computations."""

import argparse
import collections
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("functor_compatibility", ROOT / "dev/prelude-functor-compatibility.py")
functor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(functor)
pilot = functor.pilot

NATTRANS_SOURCE = "prelude/compatibility/nattrans.mech"
COMPUTATIONS = "test/fixtures/prelude/compatibility-nattrans.mech"
REGRESSIONS = ("test/fixtures/prelude/compatibility-category.mech", functor.COMPUTATIONS)
MEMBERS = ("NatTrans", "NatTrans.mk", "NatTrans.app", "NatTrans.naturality")
GROUP = "  specialize MechSignatureNatTrans (u0, u1, u2, u3) as Candidate"
CONTROL_TESTS = 10
CONTROL_CALLS = 80


def inputs():
    return (pilot.EQUALITY_SOURCE, functor.CATEGORY_SOURCE, functor.FUNCTOR_SOURCE,
            NATTRANS_SOURCE, COMPUTATIONS, *REGRESSIONS, "dev/denominators.json",
            "dev/prelude-compatibility-pilot.py", "dev/prelude-functor-compatibility.py",
            "dev/prelude-nattrans-compatibility.py", "test/prelude_nattrans_compatibility.py")


def source():
    return functor.source() + "\n" + (ROOT / NATTRANS_SOURCE).read_text()


def aliases(prefix="Candidate", levels=("u0", "u1", "u2", "u3")):
    result = functor.aliases(prefix + "_Base", levels)
    result[(pilot.U1 + "NatTrans", levels)] = prefix + "_NatTrans"
    result[(pilot.U1 + "NatTrans.app", levels)] = prefix + "_app"
    return result


def support_signatures(graph, prelude, directory, mech, timeout):
    rows = [functor.check_equality(graph, prelude, directory, mech, timeout)]
    category_aliases = {(pilot.U1 + "Category", ("u0", "u1")): "Dependency_Category",
                        ("Eq", ("(succ u1)",)): "Dependency_Eq"}
    for member in ("Hom", "id", "comp"):
        category_aliases[(pilot.U1 + "Category." + member, ("u0", "u1"))] = "Dependency_" + member
    for member in ("Category", "Category.Hom", "Category.id", "Category.comp"):
        target = "Dependency_" + member.split(".")[-1]
        rows.append(pilot.check_signature(
            graph, {}, pilot.U1 + member, target, prelude, directory, mech, timeout,
            group="  specialize MechSignatureCategory (u0, u1) as Dependency",
            aliases=category_aliases, adapter=target))
    for member in functor.MEMBERS:
        target = "Candidate_" + member.split(".")[-1]
        rows.append(pilot.check_signature(
            graph, {}, pilot.U1 + member, target, prelude, directory, mech, timeout,
            group=functor.GROUP, aliases=functor.aliases(), adapter=target))
    return rows


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
        support = support_signatures(graph, prelude, args.out, args.mech, args.timeout)
        rows = []
        for member in MEMBERS:
            target = "Candidate_" + member.split(".")[-1]
            rows.append(pilot.check_signature(
                graph, {}, pilot.U1 + member, target, prelude, args.out, args.mech, args.timeout,
                group=GROUP, aliases=aliases(), adapter=target))
        pilot.discharge_dependencies(rows + support)
        path = args.out / "projection-computations.mech"
        path.write_text(prelude + "\n" + (ROOT / COMPUTATIONS).read_text())
        computations = [functor.run_fixture(args.mech, command, path, args.timeout)
                        for command in ("check", "axioms")]
        computations.append(functor.run_fixture(args.audit, "--audit", path, args.timeout, "audit"))
        regressions = args.out / "dependency-projection-regressions.mech"
        regressions.write_text("\n".join([prelude, *((ROOT / fixture).read_text() for fixture in REGRESSIONS)]))
        regression = functor.run_fixture(args.mech, "check", regressions, args.timeout)
        report = {"version": 1, "gate_passed": False,
                  "scope": ("four NatTrans signatures, eleven support signatures, projection computations"
                            " and Category and Functor projection regressions"),
                  "export_sha256": expected, "import_graph_sha256": graph_hash,
                  "inputs": input_hashes, "binaries": binaries, "signatures": rows, "support": support,
                  "counts": dict(collections.Counter(row["status"] for row in rows)),
                  "computation_fixture": path.name, "computation_sha256": pilot.digest(path),
                  "computations": computations, "regression_fixture": regressions.name,
                  "regression_sha256": pilot.digest(regressions), "regression": regression}
        report_path = args.out / "report.json"
        functor.write_report(report_path, report)
        if any(row["status"] != "NAME_AND_TYPE" for row in rows + support):
            raise pilot.Blocked("SIGNATURE_FAILED", "NatTrans signature or dependency did not match")
        if (any(row["exit"] != 0 for row in computations) or computations[1]["stdout"]
                or computations[2]["stdout"] != "PRELUDE-AXIOMS OK"):
            raise pilot.Blocked("COMPUTATION_FAILED", "projection check or axiom audit failed")
        if regression["exit"] != 0:
            raise pilot.Blocked("REGRESSION_FAILED", "Category or Functor projections failed with the NatTrans prelude")
        controls = subprocess.run(
            [sys.executable, "-P", str(ROOT / "test/prelude_nattrans_compatibility.py"),
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
            raise pilot.Blocked("INPUT_CHANGED", "NatTrans compatibility inputs changed during checking")
        if any(pilot.digest(path) != value for path, value in binaries.items()):
            raise pilot.Blocked("CHECKER_CHANGED", "checker changed during checking")
        report["gate_passed"] = True
        functor.write_report(report_path, report)
        print(f"PRELUDE-NATTRANS-COMPATIBILITY signatures={len(rows)} support={len(support)} "
              f"computations=3 audits=2 regressions=1 controls={report['controls']['tests']} OK")
        print(f"Evidence: {args.out}", file=sys.stderr)
        return 0
    except (pilot.Blocked, subprocess.TimeoutExpired, OSError, ValueError, KeyError) as error:
        functor.write_report(args.out / "failure.json", {
            "code": getattr(error, "code", "NATTRANS_ERROR"), "detail": str(error)})
        print("PRELUDE-NATTRANS-COMPATIBILITY FAIL " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
