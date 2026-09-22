# Dependency export validation

Date: 2026-09-22. Base: a86e84b742ac51ac3d72ad40210ae45a8a5aa371.
Build copy: `/Users/oobi/Documents/gpt6/mechanism-lang-group-exports-20260922`.

| Check | Result | Evidence |
| --- | --- | --- |
| Build | Zero errors and warnings | `build.stdout` |
| Dependency exports | 6 positives, 18 semantic refusals, 6 parser refusals, 2 budget checks | `suite.stdout` |
| Runtime | 2 computations, 3 hosts, input mutation | `runtime.stdout` |
| Mutations | 6 compiling mutants killed; restored suite passes | `mutations.stdout`, `mutation-builds.stdout`, `*-test.log` |
| PIN-DELTA | Pass, syntax 79, parser 262, elaborator 389 | `pin-delta.stdout` |

The suite checks symbolic isolation, closed specialization, references in
member types and bodies, nested composition, family reuse, and exact name
reservations. Runtime validation compares the kernel, Node and Wasmtime
at payloads 37 and 41 and requires empty axiom reports.

The mutation runner is `mutations.py`. It takes a new work directory
outside the repository, builds the isolated copy there, and copies every
per-attempt test log into this directory before it exits: `baseline-test.log`,
one `<name>-test.log` per mutant and `restored-test.log`. The work directory
stays with the caller. The aggregate build record includes baseline, every
mutant and the restored build.
The final scoped build and suite captures are also available in
`.kanon-exec/run-5Pow1w` and `.kanon-exec/run-1yzs9J` in the build copy.

## Full battery

`zsh dev/gates.sh` completed all 84 measurements: 82 passed, with
`PIN-DELTA` and `TRUSTED-LINES` reporting failures. The complete, unedited
streams are `gates.stdout` and `gates.stderr`; timings are in `measurements.stdout`.
That battery ran on an earlier elaborator: its `PIN-DELTA` rows pin
`surface/elab.ml` at 385, before the export-mapping check ahead of name
planning and the composed-alias guard in `Family_poly.compose` landed.
`sources.sha256` pins the final bytes (elaborator 389). The legs that drive
`elab_composition` and `Family_poly.compose` were rerun on the pinned bytes
after `zsh dev/dunecho.sh build`; the capture is `elab-legs.stdout`.

The initial `PIN-DELTA` failure identified stale overlay counts. The final
ledger records syntax 79, parser 262 and elaborator 389, including the
budget fix. The final standalone audit passes in `pin-delta.stdout`.

`TRUSTED-LINES` remains an inherited failure: kernel 5475/3000 and encoder
246/900. The predecessor record in `../prenex-exports/gates.stdout` reports
the same counts and failure. This slice changes neither kernel sources
nor the limits. The complete run ends `GATES-FAIL`; the successful
PIN-DELTA rerun resolves its other failure.

All other battery legs passed on the earlier bytes, including kernel,
surface, WASM, import, category laws, runtime comparisons, pin and
denominator audits. On the pinned bytes, `elab-legs.stdout` records
PIN-DELTA (elaborator 389), PRENEX-GROUPS, SUITE-SURFACE, FAMILY-POLY,
FAMILY-MEMBERS, PRELUDE-TRANSPORT, FAMILY-GROUPS, AXIOMS, CORPUS-UAT and
MAP-INVENTORY, all PASS with the oracles of `dev/gates.sh`. Final
supplemental results are preserved in `suite.stdout`, `runtime.stdout`,
`exports.stdout`, `composition.stdout`, `symbolic-reuse.stdout` and the
mutation records. `budget-before.stderr` preserves the reproduced budget
failure before the fix; the final suite checks that regression successfully.

## Provenance

The full battery started before the additional alias, member-type and
budget regressions. Its initial feature suite has 5 positives, 17 semantic
refusals and 1 budget check. The final implementation also validates
explicit dependency exports before name planning, so budget exhaustion
takes precedence over a name collision. That prevalidation runs only for
explicit dependency export clauses; the existing prelude groups omit them.
The final build, dependency suite, runtime comparison, existing export and
composition suites, symbolic-reuse suite and all six mutations were rerun
after this fix. `suite.stdout` captures 6 positives, 18 semantic refusals
and 2 budget checks. The named supplemental captures preserve these results.

`sources.sha256` pins the final source, tests, gate runner and contract.
The source contract and rerun commands are in
`../../M0-STAGE-C-DEPENDENCY-EXPORTS.md`.
