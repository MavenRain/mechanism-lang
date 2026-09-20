# Horizontal composition law validation

Base: `dbaa9554673f7003eb17892fb54099a75cb77da3`. Date: 2026-09-19.

The build passed with zero errors and warnings. Both new gates passed:

- PRELUDE-HORIZONTAL-LAWS: 175.350 seconds, exit 0.
- PRELUDE-HORIZONTAL-LAWS-RUNTIME: 151.143 seconds, exit 0.

The kernel suite checks 1,205 definitions, twelve families, 28 component
computations and seven refusals. It checks independent universes in both
directions, reuse of all three category families, parse/print stability,
checked family certificates, unchanged initial globals and no added axioms.
The runtime suite compares fourteen exports at two payloads on the kernel,
Node and Wasmtime, including 56 cross-host comparisons.

Each refusal checks a readable prefix and a digest of the entire diagnostic.
The standard-library Digest fingerprint is used only to compare diagnostic
text. `diagnostics.json` additionally records SHA-256 hashes and byte lengths
of the full messages. The unrelated-family refusal retains its complete
message. The longest expanded endpoint diagnostic stays in the capture.

Both new gates use the existing 900-second CATEGORY tier. Runtime limits
are 540 seconds overall, 480 seconds for check/evaluation/emission, and
30 seconds per host command, matching shared left Kan validation. Earlier
300-second kernel and 240-second runtime probes timed out. Their captures,
the diagnostic calibration, and the initial erasure correction are recorded
in `attempts.json`. All existing gate commands, expectations and tier limits
are unchanged.

PIN-DELTA and DENOMINATORS passed. TRUSTED-LINES retains the inherited
failure: kernel=5475/3000, encoder=246/900. The full battery was not run;
this record claims the scoped checks in `checks.json` only.

Reproduce from the repository root:

```sh
zsh dev/dunecho.sh build
_build/default/test/prelude_horizontal_laws.exe .
python3 -I test/horizontal_laws_runtime.py
zsh dev/pin-delta.sh
zsh dev/gates.sh --leg denominators
zsh dev/trusted-lines.sh .
shasum -a 256 -c dev/validation/port-uat-u1-horizontal-laws/sources.sha256
git diff --cached --check
```

The trusted-lines command is expected to exit 1 until the kernel-limit
ruling changes. `checks.json` records every scoped command, deadline,
duration, exit, output hash and executable hash. The corresponding
`*.stdout` and `*.stderr` files retain the final output. No performance
baseline or milestone exit is claimed.

The close stage of the review appended a close paragraph to the review
block in `dev/M0-BUILD-LOG.md` after the ladder verdict, and the
`sources.sha256` row for that file is re-pinned to the new staged hash.
