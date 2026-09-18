"""Replay source controls in a small isolated fixture tree without rebuilding."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OK = "PRELUDE-NATTRANS-LAWS-OK entries=234 families=10 computations=12 negatives=6\n"
FAIL = "PRELUDE-NATTRANS-LAWS-FAIL "
TEMPLATE = "prelude/cat/nattrans-laws.mech"
RUNTIME = "test/fixtures/prelude/nattrans-laws-runtime.mech"
NEGATIVE = "test/neg/nattrans-laws/false-components.mech"
EXPECT_RELATION_ENDPOINT = (
    "mismatch: the term has type (Lan SMu Ops_Base_Target [(Out SPi"
    " w x C (APt w x) (Elim SPi w app (Ran SPi w x C (Out SPi 0 y D"
    " (APt 0 (Out SPi w _ C (APt w x) (Elim SPi w obj (Ran SPi w _ "
    "C D) G as self return (Ran SPi w _ C D) with | (ALeg 0) x y =>"
    " x))) (Out SPi 0 x D (APt 0 (Out SPi w _ C (APt w x) (Elim SPi"
    " w obj (Ran SPi w _ C D) F as self return (Ran SPi w _ C D) wi"
    "th | (ALeg 0) x y => x))) (Elim SPi w hom (Ran SPi 0 x D (Ran "
    "SPi 0 y D Type (u3 + 1))) d as self return (Ran SPi 0 x D (Ran"
    " SPi 0 y D Type (u3 + 1))) with | (ALeg 0) x y => x)))) alpha "
    "as self return (Ran SPi")
EXPECT_IDENTITY_PROOF = (
    "mismatch: the constructor categoryRefl of Ops_Base_Target give"
    "s the index (Out SPi w _ (Out SPi 0 y D (APt 0 (Out SPi w _ C "
    "(APt w x) (Elim SPi w obj (Ran SPi w _ C D) G as self return ("
    "Ran SP")
EXPECT_ASSOCIATIVITY_PROOF = (
    "mismatch: the constructor categoryRefl of Ops_Base_Target give"
    "s the index (Out SPi w _ (Out SPi 0 y D (APt 0 (Out SPi w _ C "
    "(APt w x) (Elim SPi w obj (Ran SPi w _ C D) I as self return ("
    "Ran SP")
EXPECT_CONGRUENCE_PROOF = (
    "mismatch: the term has type (Lan SMu Ops_Base_Target [(Out SPi"
    " w x C (APt w x) (Elim SPi w app (Ran SPi w x C (Out SPi 0 y D"
    " (APt 0 (Out SPi w _ C (APt w x) (Elim SPi w obj (Ran SPi w _ "
    "C D) G as self return (Ran SPi w _ C D) with | (ALeg 0) x y =>"
    " x))) (Out SPi 0 x D (APt 0 (Out SPi w _ C (APt w x) (Elim SPi"
    " w obj (Ran SPi w _ C D) F as self return (Ran SPi w _ C D) wi"
    "th | (ALeg 0) x y => x))) (Elim SPi w hom (Ran SPi 0 x D (Ran "
    "SPi 0 y D Type (u3 + 1))) d as self return (Ran SPi 0 x D (Ran"
    " SPi 0 y D Type (u3 + 1))) with | (ALeg 0) x y => x)))) alpha2"
    " as self return (Ran SP")
EXPECT_SHARING = (
    "mismatch: the term has type (Lan SPi w hom (Ran SPi 0 x (L")
CONTROLS = [
    ("relation-endpoint", TEMPLATE,
     "(Ops_natApp C D c d F G alpha x) (Ops_natApp C D c d F G beta x)",
     "(Ops_natApp C D c d F G alpha x) (Ops_natApp C D c d F G alpha x)",
     EXPECT_RELATION_ENDPOINT),
    ("identity-proof", TEMPLATE,
     "Ops_Base_Target_idComp D d (F.1 x) (G.1 x) (alpha.1 x)",
     "categoryRefl", EXPECT_IDENTITY_PROOF),
    ("associativity-proof", TEMPLATE,
     "Ops_Base_Target_assoc D d (F.1 x) (G.1 x) (H.1 x) (I.1 x)\n"
     "      (alpha.1 x) (beta.1 x) (gamma.1 x)",
     "categoryRefl", EXPECT_ASSOCIATIVITY_PROOF),
    ("congruence-proof", TEMPLATE,
     "(beta.1 x) (beta2.1 x) (second x)",
     "(beta.1 x) (beta2.1 x) (first x)", EXPECT_CONGRUENCE_PROOF),
    ("sharing", RUNTIME,
     "with (Ops_Base_Source := Run_Base_Source, Ops_Base_Target := Run_Base_Target)",
     "with (Ops_Base_Source := Run_Base_Source)", EXPECT_SHARING),
    ("component", RUNTIME, "(x : Nat) => (alpha.1 x).1 0",
     "(x : Nat) => (alpha.1 (natAdd x 1)).1 0", "wrong computation: leftIdentityAt/37"),
    ("refusal-control", NEGATIVE, None, "def bad : Nat := 0\n",
     "false-components: expected refusal"),
]
FILES = ["prelude/cat/category-core.mech", "prelude/cat/heterogeneous-functor.mech",
         "prelude/cat/heterogeneous-nattrans.mech", TEMPLATE,
         "test/fixtures/prelude/nattrans-laws.mech",
         "test/fixtures/prelude/heterogeneous-nattrans-runtime.mech", RUNTIME]


def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/nattrans-laws-mutations.py NEW-WORK-DIRECTORY", file=sys.stderr)
        return 64
    work = Path(sys.argv[1]).resolve()
    if work == ROOT or work.is_relative_to(ROOT) or work.exists():
        raise ValueError("choose a fresh work directory outside the repository")
    work.mkdir(parents=True)
    copy = work / "copy"
    files = FILES + [str(file.relative_to(ROOT)) for file in
                    sorted((ROOT / "test/neg/nattrans-laws").iterdir()) if file.is_file()]
    for name in files:
        destination = copy / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, destination)
    exe = ROOT / "_build/default/test/prelude_nattrans_laws.exe"
    hashes = {name: digest(copy / name) for name in files}
    rows = []

    def run(label):
        started = time.monotonic()
        command = [str(exe), str(copy)]
        result = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=290)
        (work / f"{label}.stdout").write_text(result.stdout)
        (work / f"{label}.stderr").write_text(result.stderr)
        row = {"id": label, "command": ["_build/default/test/prelude_nattrans_laws.exe", "COPY"],
               "exit": result.returncode, "seconds": round(time.monotonic() - started, 3),
               "stdout_sha256": digest(work / f"{label}.stdout"),
               "stderr_sha256": digest(work / f"{label}.stderr")}
        rows.append(row)
        return result, row

    baseline, baseline_row = run("baseline")
    baseline_row["passed"] = baseline.returncode == 0 and baseline.stdout == OK and not baseline.stderr
    if not baseline_row["passed"]:
        raise ValueError("baseline did not pass; see baseline.stdout and baseline.stderr")
    for label, name, before, after, expected in CONTROLS:
        target = copy / name
        original = target.read_text()
        if before is not None and original.count(before) != 1:
            raise ValueError(f"{label}: mutation anchor is not unique")
        try:
            target.write_text(after if before is None else original.replace(before, after))
            mutation_sha256 = digest(target)
            result, row = run(label)
            row.update({"path": name, "before_sha256": hashes[name],
                        "mutated_sha256": mutation_sha256, "expected": FAIL + expected,
                        "killed": result.returncode == 1 and not result.stderr
                        and result.stdout.startswith(FAIL + expected)})
            print(f"{label}: killed={row['killed']}", flush=True)
        finally:
            target.write_text(original)
    restored, restored_row = run("restored")
    restored_row["passed"] = restored.returncode == 0 and restored.stdout == OK and not restored.stderr
    unchanged = all(digest(copy / name) == sha and digest(ROOT / name) == sha
                    for name, sha in hashes.items())
    killed = sum(row.get("killed", False) for row in rows)
    passed = baseline_row["passed"] and restored_row["passed"] and unchanged and killed == len(CONTROLS)
    report = {"passed": passed, "killed": killed, "controls": len(CONTROLS),
              "sources_unchanged": unchanged, "source_sha256": hashes,
              "suite_source_sha256": digest(ROOT / "test/prelude_nattrans_laws.ml"),
              "executable_sha256": digest(exe), "rows": rows}
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": passed, "killed": killed, "controls": len(CONTROLS)}))
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"NATTRANS-LAWS-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
