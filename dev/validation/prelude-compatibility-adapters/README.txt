# Symbolic prelude compatibility adapters

Base: c4793ae3d477c7ee72a0f7825118cfb333912892. Date: 2026-10-02.

The fresh scoped gate exits 0. All 19 candidate signatures match with the
report's explicit equality adapters, Not matches as separate support, and
all 19 controls pass. All four U1 record signatures match. The unchanged
CompCatTheory.LeftKanExtension fixture checks in about 64 seconds at load 12,
within the existing 180-second limit. This is not a full typed-parity or M0
acceptance result.

`report.json` fingerprints the checker, imported graph, consumed source files
and generated fixtures. The full imported types.ndjson graph is reproducible
from the frozen export and is not duplicated in Git. Its manifest, summary
and import output are retained. The source witnesses and kernel diagnostics
are retained beside the report, including the closed client and refusals.

Reproduce with `make prelude-compatibility-test`, or select an existing
production checker with `python3 -P dev/prelude-compatibility-gates.py --mech
_bend2/bin/mech.exe`. Every run creates a fresh evidence directory. The report
retains blocked rows independently of the control gate's status.

`attempts/` retains the initial report with a LeftKanExtension timeout and
the first full gate's report and failed closed-fixture control log. The
closed client initially omitted its family reuse bindings. The corrected
client passes both the kernel check and the axiom audit in the final run.
The final gate was rerun after that change, so its input hashes cover the
corrected fixture. No time limit was raised. After review fixes to the
controls, the gate was rerun again at lower load.
`attempts/mechanism-compatibility-adapter-load-timeout/` retains the
pre-review final report and gate logs, in which LeftKanExtension reached the
180-second limit, and that report's record comparison with the c4793ae pilot.

The kernel implementation did not change; the prepared production checker
was reused. The full acceptance battery and Bend 2 performance targets have
no new result in this increment.
