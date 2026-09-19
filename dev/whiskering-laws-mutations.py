"""Check proof and sharing mutations using isolated symbolic template sources."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = "prelude/cat/whiskering-laws.mech"
SOURCES = ["prelude/cat/" + name + ".mech" for name in (
    "category-core", "heterogeneous-functor", "composable-functors",
    "heterogeneous-nattrans", "heterogeneous-whiskering", "shared-nattrans",
    "nattrans-laws", "whiskering-laws")]
DIAGNOSTICS = "dev/validation/port-uat-u1-whiskering-laws/mutation-diagnostics.json"
CONTROLS = [
    ("left-identity-proof", "Ops_Second_Base_functorMapId D E d e H (F.1 x)", "categoryRefl"),
    ("left-composition-proof",
     "Ops_Second_Base_functorMapComp D E d e H (F.1 x) (G.1 x) (J.1 x)\n"
     "      (alpha.1 x) (beta.1 x)", "categoryRefl"),
    ("left-congruence-proof", "(alpha.1 x) (beta.1 x) (proof x)",
     "(alpha.1 x) (beta.1 x) categoryRefl"),
    ("right-congruence-proof", "proof (K.1 x)", "categoryRefl"),
    ("right-composition-order", "Ops_Second_vcomp D E d e F G J\n        (alpha) (beta)",
     "Ops_Second_vcomp D E d e F G J\n        (beta) (alpha)"),
    ("shared-target", "with (Ops_Base_Source := Ops_Middle, Ops_Base_Target := Ops_Target)",
     "with (Ops_Base_Source := Ops_Middle, Ops_Base_Target := Ops_Middle)"),
    ("right-identity-target",
     "(Ops_Composite_idNat C E c e (Ops_Compose_Base_compFunctor C D E c d e K F))",
     "(Ops_Composite_idNat C E c e (Ops_Compose_Base_compFunctor C D E c d e F K))"),
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if len(sys.argv) != 2:
        print("usage: python3 -I dev/whiskering-laws-mutations.py NEW-WORK-DIRECTORY", file=sys.stderr)
        return 64
    work = Path(sys.argv[1]).resolve()
    if work.exists() or work == ROOT or work.is_relative_to(ROOT):
        raise ValueError("choose a fresh work directory outside the repository")
    work.mkdir(parents=True)
    sources = {name: (ROOT / name).read_bytes() for name in SOURCES}
    pins = json.loads((ROOT / DIAGNOSTICS).read_text())
    if set(pins) != {label for label, _, _ in CONTROLS}:
        raise ValueError("mutation diagnostic inventory changed")
    exe = ROOT / "_build/default/bin/mech.exe"
    fixture = work / "templates.mech"
    original = sources[TEMPLATE].decode()
    rows = []

    def write(template):
        fixture.write_text("\n".join(template if name == TEMPLATE else sources[name].decode()
                                    for name in SOURCES))

    def run(label):
        started = time.monotonic()
        result = subprocess.run([str(exe), "check", str(fixture)], cwd=work,
                                capture_output=True, timeout=290)
        (work / f"{label}.stdout").write_bytes(result.stdout)
        (work / f"{label}.stderr").write_bytes(result.stderr)
        row = {"id": label, "exit": result.returncode,
               "seconds": round(time.monotonic() - started, 3),
               "input_sha256": digest(fixture.read_bytes()),
               "stdout_sha256": digest(result.stdout), "stderr_sha256": digest(result.stderr)}
        rows.append(row)
        return result, row

    write(original)
    baseline, row = run("baseline")
    row["passed"] = baseline.returncode == 0 and not baseline.stdout and not baseline.stderr
    if not row["passed"]:
        raise ValueError("symbolic baseline failed; inspect baseline.stdout and baseline.stderr")
    for label, before, after in CONTROLS:
        if original.count(before) != 1:
            raise ValueError(f"{label}: mutation anchor is not unique")
        try:
            write(original.replace(before, after))
            result, row = run(label)
            row["expected_stderr_sha256"] = pins[label]
            row["killed"] = (result.returncode == 1 and not result.stdout
                             and row["stderr_sha256"] == pins[label])
            print(f"{label}: killed={row['killed']}", flush=True)
        finally:
            write(original)
    restored, row = run("restored")
    row["passed"] = restored.returncode == 0 and not restored.stdout and not restored.stderr
    unchanged = all((ROOT / name).read_bytes() == data for name, data in sources.items())
    killed = sum(row.get("killed", False) for row in rows)
    passed = rows[0]["passed"] and rows[-1]["passed"] and unchanged and killed == len(CONTROLS)
    report = {"passed": passed, "killed": killed, "controls": len(CONTROLS),
              "sources_unchanged": unchanged,
              "source_sha256": {name: digest(data) for name, data in sources.items()},
              "executable_sha256": digest(exe.read_bytes()),
              "runner_sha256": digest(Path(__file__).read_bytes()),
              "diagnostics_sha256": digest((ROOT / DIAGNOSTICS).read_bytes()), "rows": rows}
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": passed, "killed": killed, "controls": len(CONTROLS)}))
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"WHISKERING-LAWS-MUTATIONS FAIL {error}", file=sys.stderr)
        raise SystemExit(1)
