"""Check proof and computation controls in a temporary source copy."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
bounded_run = runpy.run_path(str(ROOT / "dev/c-process.py"))["run"]
TEMPLATES = ["category-core", "heterogeneous-functor", "composable-functors",
             "heterogeneous-nattrans", "heterogeneous-whiskering", "shared-nattrans",
             "heterogeneous-left-kan", "shared-left-kan", "left-kan-laws"]


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/left-kan-laws-mutations.py NEW_OUTPUT_DIRECTORY", file=sys.stderr)
        return 64
    output = Path(sys.argv[1]).resolve()
    if output == ROOT or output.is_relative_to(ROOT) or output.exists():
        raise ValueError("choose a fresh output directory outside the repository")
    output.mkdir(parents=True, exist_ok=False)
    def c_build(target):
        executable = ROOT / target
        executable.unlink(missing_ok=True)
        built = bounded_run(["make", "-C", str(ROOT), target], cwd=ROOT, capture_output=True, text=True, timeout=600)
        if built.returncode or not os.access(executable, os.X_OK):
            return None
        return {"target": target, "exit_code": built.returncode,
                "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}

    build_evidence = c_build("_tcc/prelude_left_kan_laws")
    cli_build_evidence = c_build("_tcc/bin/mech")
    if build_evidence is None or cli_build_evidence is None:
        print("LEFT-KAN-LAWS-MUTATIONS FAIL c build", file=sys.stderr)
        return 1
    checker = ROOT / build_evidence["target"]
    paths = [ROOT / "prelude/cat" / f"{name}.mech" for name in TEMPLATES]
    original = "\n".join(path.read_text() for path in paths)
    attempts = []

    def run(name, command, accepted, marker):
        started = time.monotonic()
        stdout = output / f"{name}.stdout"
        stderr = output / f"{name}.stderr"
        with stdout.open("w") as out, stderr.open("w") as err:
            try:
                result = bounded_run(list(map(str, command)), cwd=ROOT, stdout=out,
                                        stderr=err, timeout=900)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        text = stdout.read_text() + stderr.read_text()
        passed = (code == 0 and not text) if accepted else (code == 1 and text.count(marker) == 1)
        attempts.append({"name": name, "exit_code": code, "passed": passed,
                         "seconds": round(time.monotonic() - started, 3),
                         "stdout": stdout.name, "stderr": stderr.name,
                         "expected_marker": marker})
        print(json.dumps(attempts[-1]), flush=True)
        return passed

    def replace_once(text, before, after):
        if text.count(before) != 1:
            raise ValueError("mutation anchor is not unique")
        return text.replace(before, after, 1)

    with tempfile.TemporaryDirectory(prefix="mechanism-left-kan-law-controls-") as directory:
        work = Path(directory)
        source = work / "source.mech"
        source.write_text(original)
        command = [ROOT / cli_build_evidence["target"], "check", source]
        control = run("control", command, True, "")
        identity_start = original.index("    Base_Base_Second_targetEqSymm", original.index("def descId :"))
        identity_end = original.index("\n\n-- Equal cocones", identity_start)
        post_start = original.index("    Base_Lan_lanUniq", original.index("def descPostcomp :"))
        post_end = original.index("\n\n-- The cocone relation", post_start)
        mutations = [
            ("identity-reflexivity", original[:identity_start] + "    categoryRefl" + original[identity_end:]),
            ("missing-cocone-equality", replace_once(original,
                "(Base_Lan_lanFac J C D j c d K F H G eta alpha s x) (equal x)",
                "(Base_Lan_lanFac J C D j c d K F H G eta alpha s x) categoryRefl")),
            ("postcomp-reflexivity", original[:post_start] + "    categoryRefl" + original[post_end:]),
            ("postcomp-missing-factor", original[:post_start] + replace_once(original[post_start:post_end],
                "(Base_Lan_lanFac J C D j c d K F H G eta alpha s x)", "categoryRefl")
                + original[post_end:]),
        ]
        proof_marker = "mismatch: the constructor categoryRefl of Base_Base_Target gives the index"
        for name, changed in mutations:
            source.write_text(changed)
            run(name, command, False, proof_marker)
        source.write_text(original)
        restored = run("restored", command, True, "")
        scratch = work / "runtime"
        shutil.copytree(ROOT / "prelude", scratch / "prelude")
        shutil.copytree(ROOT / "test/fixtures", scratch / "test/fixtures")
        shutil.copytree(ROOT / "test/neg/left-kan-laws", scratch / "test/neg/left-kan-laws")
        fixture = scratch / "test/fixtures/prelude/left-kan-laws-runtime.mech"
        fixture.write_text(replace_once(fixture.read_text(),
            "natAdd (natMul 101 (left.1 (natAdd n 7))) (left.2 (natAdd n 11))",
            "natAdd (natMul 101 (left.2 (natAdd n 7))) (left.1 (natAdd n 11))"))
        run("swapped-closure-fields", [checker, scratch],
            False, "PRELUDE-LEFT-KAN-LAWS-FAIL wrong computation: lanCongr =")
        fixture.write_text((ROOT / "test/fixtures/prelude/left-kan-laws-runtime.mech").read_text())
        fixture.write_text(replace_once(fixture.read_text(),
            "def lanPostcomp : Nat := lanPostcompAt sharedLanInput",
            "def lanPostcomp : Nat := lanReversePostcompAt sharedLanInput"))
        run("reversed-runtime-composition", [checker, scratch],
            False, "PRELUDE-LEFT-KAN-LAWS-FAIL wrong computation: lanPostcomp =")
        fixture.write_text((ROOT / "test/fixtures/prelude/left-kan-laws-runtime.mech").read_text())

        def trivialize(text, law, body, stop, conclusion, replacement="((s.1).1 y) ((s.1).1 y) :="):
            head = text.index(f"def {law} :")
            start = text.index(body, head)
            end = text.index(stop, start)
            return (text[:head] + replace_once(text[head:start], conclusion, replacement)
                    + "    categoryRefl" + text[end:])

        law = paths[-1].read_text()
        statements = [
            ("trivial-id-conclusion", trivialize(law, "descId", "    Base_Base_Second_targetEqSymm",
                "\n\n-- Equal cocones", "((s.1).1 y) (Base_Base_Target_id D d (H.1 y)) :=")),
            ("trivial-congr-conclusion", trivialize(law, "descCongr", "    Base_Lan_lanUniq",
                "\n\n-- Postcompose", "((s.1).1 y) ((t.1).1 y) :=")),
            ("trivial-postcomp-conclusion", trivialize(law, "descPostcomp", "    Base_Lan_lanUniq",
                "\n\n-- The cocone relation",
                "(Base_Base_Target_comp D d (H.1 y) (G.1 y) (I.1 y) ((s.1).1 y) (tau.1 y))\n"
                "      ((t.1).1 y) :=",
                "(Base_Base_Target_comp D d (H.1 y) (G.1 y) (I.1 y) ((s.1).1 y) (tau.1 y))\n"
                "      (Base_Base_Target_comp D d (H.1 y) (G.1 y) (I.1 y) ((s.1).1 y) (tau.1 y)) :=")),
        ]
        template = scratch / "prelude/cat/left-kan-laws.mech"
        for name, changed in statements:
            template.write_text(changed)
            run(name, [checker, scratch],
                False, "PRELUDE-LEFT-KAN-LAWS-FAIL mismatch: the term has type")
    killed = sum(row["passed"] for row in attempts if row["name"] not in {"control", "restored"})
    report = {"c_build": build_evidence, "cli_build": cli_build_evidence, "passed": control and restored and killed == 9, "killed": killed,
              "attempts": attempts,
              "sources": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in paths + [Path(__file__), ROOT / "c/tests/prelude_left_kan_laws.c",
                              ROOT / "test/fixtures/prelude/left-kan-laws.mech",
                              ROOT / "test/fixtures/prelude/left-kan-laws-runtime.mech",
                              ROOT / "test/fixtures/prelude/shared-left-kan-runtime.mech"]
                          + sorted((ROOT / "test/neg/left-kan-laws").glob("*.mech"))
                          + sorted((ROOT / "test/neg/left-kan-laws").glob("*.err"))}}
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": report["passed"], "killed": killed, "report": str(output / "results.json")}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"LEFT-KAN-LAWS-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
