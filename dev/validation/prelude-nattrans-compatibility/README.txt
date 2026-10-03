Source-compatible NatTrans record receipt, 2026-10-02, base ba35171.

Reproduce from the repository root with:
  make prelude-nattrans-compatibility-test

The make target uses a default --export path on the machine that produced this
receipt. Elsewhere, prepare the native production checker and JavaScript test
build, then run:
  python3 -P dev/prelude-nattrans-compatibility.py \
    --export /path/to/uat.export \
    --mech "$PWD/_bend2/bin/mech.exe" --audit "$PWD/_bend2/test/prelude.exe" \
    --out "$(mktemp -d)/evidence"

The export must match dev/denominators.json. The evidence directory must be new.
The driver verifies the frozen export, 3202-declaration graph, input files and
checker hashes before recording gate_passed. The complete import/types.ndjson
graph is reproducible and omitted from Git; its native import manifest and
summary are retained.

This run used a writable checkout of ba35171 and the prepared native and audit
executables from the main repository. The delivered inputs have exactly the
hashes in report.json. Kernel and frontend sources were unchanged.

Four NatTrans signatures and eleven Category, Functor and equality support
signatures reach NAME_AND_TYPE after dependency discharge. The generic fixture
checks app and naturality projection computation, the complete stored pair
shape, and four bare-constant outer-quantity pins. The sourceHomLevel pin fixes
the source hom level v, which the four signature rows cannot detect. Three
closed specializations only put closed instances into the axiom and audit
rows. The template check covers every level, and the sourceHomLevel pin
guards the level. The kernel axiom listing is empty, and the independent
empty-environment prelude audit prints PRELUDE-AXIOMS OK.

All ten controls pass. Coverage and dependency refusal run no kernel check.
Unmapped equality and Functor controls mock the invocation to require that it
never runs. Missing proven equality blocks the constructor and naturality rows.
Eight individual erasures cover the source category and target functor in each
of the four members. Their translated signatures match, but the corresponding
bare-constant quantity pins reject them. Twins without those pins pass.

The nested object erasure is consistent across the adapter and checks on its
own, but the imported constructor and the unaltered fixture reject it. Its
correspondingly erased fixture passes. Swapped object universes are rejected
by the constructor signature, and the unswapped twin passes. A consistently
reversed naturality square checks on its own; its constructor and naturality
signatures and the generic fixture reject it, and a reversed fixture passes.

An extra stored component, with a matching constructor and accessor positions,
checks and matches all four signatures. The stored-shape conversion witness
rejects it; the fixture without that witness passes. An app accessor with an
extra identity composition matches its own signature, changes the naturality
signature and fails the component projection. The fixture without the affected
law witnesses and projection passes. A double-symmetry naturality accessor
matches all four signatures but fails its projection. Its twin without that
projection passes. Rejections require kernel type diagnostics, so parser and
scope errors cannot count as successful controls.

controls/source-hom-level specializes the Functor schema with z in place of
the source hom level v. The altered prelude checks on its own, and all four
NatTrans signatures match. The full fixture fails with the kernel diagnostic
"Type (u3 + 1) and the expected type is Type (u1 + 1)". The fixture without
the sourceHomLevel pin passes.

The driver writes dependency-projection-regressions.mech from the NatTrans
prelude and the unchanged Category and Functor generic projection fixtures. It
checks the file with the native checker and records its sha256 and exit status
in report.json. gate_passed requires exit 0, and the gate line counts this
check as regressions=1. Recheck the saved file with:
  _bend2/bin/mech.exe check dev/validation/prelude-nattrans-compatibility/\
dependency-projection-regressions.mech

attempts/initial-controls/ records the first gate run, which passed all
signatures and audits and seven of eight controls. The quantity control's
source-count assertion expected two named G binders, although the original
NatTrans type used an anonymous functor arrow. Naming the outer F and G binders
made this mutation explicit without changing the type. The failed report,
diagnostics and original computation fixture remain as failed evidence. The
final gate adds the stored-field and source hom level controls and runs all
ten controls. attempts/initial-controls/original-sources/ contains the initial
adapter, driver and control suite, with hashes matching the failed report.
Other inputs remained unchanged. In failure.json and controls.stderr,
<checkout>/ replaces the absolute path of the original checkout. The Evidence
line in gate.stderr is shown relative to the checkout.

The mapping and NEVER inventories remain unchanged. Source-compatible
recursors, whole-record equality, operation and theorem bodies, runtime parity
and conversion to the erased core NatTrans remain open. This scoped gate does
not close M0, Stage C or U1.
