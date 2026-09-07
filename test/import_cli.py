#!/usr/bin/env python3
"""Exercise the import process boundary and its published type table."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def fixture():
    return [
        {"meta": {"exporter": {"name": "lean4export", "version": "3.1.0"},
                  "format": {"version": "3.1.0"},
                  "lean": {"githash": "fixture", "version": "4.31.0"}}},
        {"in": 1, "str": {"pre": 0, "str": "P"}},
        {"ie": 0, "sort": 0},
        {"axiom": {"name": 1, "levelParams": [], "type": 0, "isUnsafe": False}},
        {"in": 2, "str": {"pre": 0, "str": "D"}},
        {"ie": 1, "const": {"name": 1, "us": []}},
        {"axiom": {"name": 2, "levelParams": [], "type": 1, "isUnsafe": False}},
        {"in": 3, "str": {"pre": 0, "str": "u"}},
        {"in": 4, "str": {"pre": 0, "str": "S"}},
        {"in": 5, "str": {"pre": 0, "str": "x"}},
        {"in": 6, "str": {"pre": 0, "str": "R"}},
        {"il": 1, "param": 3},
        {"ie": 2, "sort": 1},
        {"ie": 3, "bvar": 0},
        {"ie": 4, "proj": {"typeName": 4, "idx": 0, "struct": 3}},
        {"ie": 5, "mdata": {"expr": 4, "data": {"note": "kept"}}},
        {"ie": 6, "letE": {"name": 5, "type": 0, "value": 1, "body": 5, "nondep": True}},
        {"ie": 7, "lam": {"name": 5, "type": 0, "body": 6, "binderInfo": "implicit"}},
        {"ie": 8, "forallE": {"name": 5, "type": 2, "body": 7, "binderInfo": "default"}},
        {"axiom": {"name": 6, "levelParams": [3], "type": 8, "isUnsafe": False}},
    ]


def main():
    executable = str(Path(sys.argv[1]).resolve())
    failures = []
    cases = 0

    def check(condition, message):
        if not condition:
            failures.append(message)
            print(f"IMPORT-CLI FAIL {message}")
        return condition

    def invoke(*arguments):
        return subprocess.run([executable, *map(str, arguments)],
                              capture_output=True, text=True, timeout=15)

    def write(path, rows):
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    with tempfile.TemporaryDirectory(prefix="mechanism-import-") as directory:
        root = Path(directory)
        source = root / "input.export"
        write(source, fixture())
        output = root / "output"
        result = invoke("import", source, "--out", output)
        check(result.returncode == 0, f"import exited {result.returncode}: {result.stderr}")
        check(result.stderr == "", f"import wrote to stderr: {result.stderr}")
        check("declarations=3 external_referenced=1 external_declared=3 const_names=1"
              in result.stdout, "the census line changed")
        manifest = json.loads((output / "manifest.json").read_text())
        check(manifest["declarations"] == 3, "the manifest declaration count changed")
        check(manifest["mapping_checked"] is False, "the manifest claims checked mappings")
        check(manifest["values_translated"] is False, "the manifest claims translated values")
        records = [json.loads(line) for line in (output / "types.ndjson").read_text().splitlines()]
        declarations = [row["declaration"] for row in records if "declaration" in row]
        check([(row["name"], row["status"]) for row in declarations] == [
            ("P", "KERNEL_TYPE"), ("D", "DEFERRED"), ("R", "DEFERRED")],
            "the published declaration statuses changed")
        check("P" in declarations[1]["reason"], "the deferred reason does not name P")
        nodes = {row["expr"]: row for row in records if "expr" in row}
        check(nodes[7]["lam"]["binderInfo"] == "implicit", "the lambda binderInfo changed")
        check(nodes[8]["forallE"]["type"] == 2, "the forall domain changed")
        check(nodes[6]["letE"]["nondep"] is True, "the let nondep flag changed")
        check(nodes[4]["proj"]["typeName"] == "S" and nodes[4]["proj"]["idx"] == 0,
              "the projection type name or index changed")
        check(nodes[5]["mdata"]["data"] == {"note": "kept"}, "the metadata object changed")
        saved_levels = {row["level"]: row for row in records if "level" in row}
        check(saved_levels[1]["param"]["name"] == "u", "the universe parameter name changed")
        check((output / "summary.txt").read_text() == result.stdout,
              "summary.txt differs from the report on stdout")
        verified = subprocess.run(
            [sys.executable, "-P", str(Path(__file__).with_name("import_output.py")),
             str(source), str(output)], capture_output=True, text=True, timeout=15)
        check(verified.returncode == 0, f"the artifact checker failed: {verified.stderr}")
        check("IMPORT-OUTPUT OK" in verified.stdout, "the artifact checker printed no verdict")
        cases += 1

        second = root / "second"
        again = invoke("import", source, "--out", second)
        check(again.returncode == 0, f"the second import exited {again.returncode}: {again.stderr}")
        check(all(path.read_bytes() == (second / path.name).read_bytes()
                  for path in output.iterdir()), "two imports of one input differ")
        cases += 1

        snapshot = {path.name: path.read_bytes() for path in output.iterdir()}
        refused = invoke("import", source, "--out", output)
        check(refused.returncode == 1 and "already exists" in refused.stderr,
              "an existing output directory must be refused")
        check({path.name: path.read_bytes() for path in output.iterdir()} == snapshot,
              "the refused import changed the existing output")
        cases += 1

        parity = invoke("diff-parity", "--export", source)
        check(parity.returncode == 0 and parity.stdout == result.stdout,
              "diff-parity differs from the import report")
        check("mapping=not-checked" in parity.stdout, "diff-parity claims a checked mapping")
        cases += 1

        identity_rows = [fixture()[0],
                         {"in": 1, "str": {"pre": 0, "str": "1"}},
                         {"in": 2, "num": {"pre": 0, "i": 1}},
                         {"in": 3, "str": {"pre": 0, "str": "«1»"}},
                         {"ie": 0, "sort": 0}]
        for index in range(1, 4):
            identity_rows.append({"axiom": {
                "name": index, "levelParams": [], "type": 0, "isUnsafe": False}})
            identity_rows.append({"ie": index, "const": {"name": index, "us": []}})
            identity_rows.append({"in": index + 3, "str": {"pre": 0, "str": f"D{index}"}})
            identity_rows.append({"axiom": {
                "name": index + 3, "levelParams": [], "type": index, "isUnsafe": False}})
        identity_source = root / "identity.export"
        write(identity_source, identity_rows)
        identity_output = root / "identity"
        identity = invoke("import", identity_source, "--out", identity_output)
        check(identity.returncode == 0,
              f"the identity import exited {identity.returncode}: {identity.stderr}")
        check("declarations=6 external_referenced=3 external_declared=6 const_names=3"
              in identity.stdout, "the identity census line changed")
        identity_records = [json.loads(line) for line in
                            (identity_output / "types.ndjson").read_text().splitlines()]
        names = {row["name"]: row for row in identity_records if "name" in row}
        check(names[1] == {"name": 1, "str": {"pre": 0, "str": "1"}}, "raw name 1 changed")
        check(names[2] == {"name": 2, "num": {"pre": 0, "i": 1}}, "raw name 2 changed")
        check(names[3] == {"name": 3, "str": {"pre": 0, "str": "«1»"}}, "raw name 3 changed")
        displays = [row["declaration"]["name"] for row in identity_records
                    if "declaration" in row and row["declaration"]["source_name"] in (1, 2, 3)]
        check(len(displays) == len(set(displays)) == 3, "display names are not injective")
        references = [row["const"]["source_name"] for row in identity_records if "const" in row]
        check(sorted(references) == [1, 2, 3], "constant references lost their name identity")
        cases += 1

        kernel_error_source = root / "kernel-error.export"
        kernel_error_output = root / "kernel-error"
        malformed_type = fixture()[:4]
        malformed_type[2] = {"ie": 0, "natVal": "0"}
        write(kernel_error_source, malformed_type)
        collected = invoke("import", kernel_error_source, "--out", kernel_error_output)
        check(collected.returncode == 0 and collected.stderr == "",
              f"a kernel error must not fail the command: {collected.stderr}")
        check("kernel_errors=1" in collected.stdout, "the kernel error is not counted")
        collected_rows = [json.loads(line) for line in
                          (kernel_error_output / "types.ndjson").read_text().splitlines()]
        collected_declarations = [row["declaration"] for row in collected_rows if "declaration" in row]
        check(len(collected_declarations) == 1, "the kernel error dropped a declaration")
        check(collected_declarations[0]["status"] == "KERNEL_ERROR", "the error status changed")
        check(collected_declarations[0]["reason"] != "", "the error row carries no reason")
        collected_manifest = json.loads((kernel_error_output / "manifest.json").read_text())
        check(collected_manifest["mapping_checked"] is False,
              "the error manifest claims a checked mapping")
        cases += 1

        bad = fixture()
        bad[0]["meta"]["format"]["version"] = "3.2.0"
        invalid = root / "invalid.export"
        write(invalid, bad)
        rejected = invoke("import", invalid, "--out", root / "invalid-output")
        check(rejected.returncode == 1 and ":1:" in rejected.stderr,
              "an unsupported format version must be refused with a location")
        check("3.2.0" in rejected.stderr and rejected.stdout == "",
              "the refusal must name the version and print no report")
        check(not (root / "invalid-output").exists(),
              "a refused import must publish nothing")
        cases += 1

        bad = fixture()
        bad[2] = {"ie": 0, "bvar": 0}
        write(invalid, bad)
        rejected = invoke("import", invalid)
        check(rejected.returncode == 1 and "unbound" in rejected.stderr,
              "an unbound de Bruijn index must be refused")
        check(rejected.stdout == "", "the refused import printed a report")
        cases += 1

        unreadable = [invoke("import", missing) for missing in [root / "absent", root]]
        check(all(run.returncode == 1 and run.stderr != "" for run in unreadable),
              "an unreadable input must be refused with a diagnostic")
        check(all("Fatal error" not in run.stderr for run in unreadable),
              "an unreadable input must not escape as a host exception")
        cases += 1

        trailing = root / "trailing"
        published = invoke("import", source, "--out", f"{trailing}{os.sep}")
        check(published.returncode == 0,
              f"a trailing separator must publish: {published.stderr}")
        check(sorted(path.name for path in trailing.iterdir()) == [
            "manifest.json", "summary.txt", "types.ndjson"],
            "a trailing separator changed the published artifacts")
        check(list(root.glob("trailing.tmp-*")) == [],
              "the staging directory was left behind")
        cases += 1

        rejected = invoke("import", source, "--out", root / "missing" / "output")
        check(rejected.returncode == 1 and rejected.stderr != "",
              "a missing output parent must be refused with a diagnostic")
        check("Fatal error" not in rejected.stderr,
              "a missing output parent must not escape as a host exception")
        cases += 1

        usages = [invoke(*arguments) for arguments in
                  [[], ["import"], ["diff-parity"], ["import", str(source), "--bad"]]]
        check(all(run.returncode == 64 and "usage: mech" in run.stderr for run in usages),
              "each usage error must exit 64 and print the usage line")
        cases += 1

    if failures != []:
        print(f"IMPORT-CLI FAIL failing_checks={len(failures)} cases={cases}")
        sys.exit(1)
    print(f"IMPORT-CLI OK cases={cases}")
    sys.exit(0)


if __name__ == "__main__":
    main()
