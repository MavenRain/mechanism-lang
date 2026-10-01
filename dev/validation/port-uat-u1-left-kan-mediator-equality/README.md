# Left Kan mediator equality validation

`checks.json` records the targeted checks and hashes of the changed
sources. `kernel.stdout` contains the native gate result; `runtime.stdout`
contains the 24 comparisons across three hosts and two payloads.
`native-build.json` pins the final native driver inputs and compiler.
`runtime-reuse.json` records 118 unchanged inputs checked before reusing
the runtime binary.

The contract and runtime focused sources have their own extraction
receipts. `legacy-extractions.json` pins eight older modes that match
the committed base. `refusal-pins.json` records complete diagnostic
SHA-256 hashes and the MD5 checksums consumed by the native gate.
Its raw capture pointer refers to a retained local artifact, while the
compact refusal prefixes and digests live in the test fixture directory.

The initial reflection proof exceeded the unchanged runtime emission
allowance. The final proof uses the factorization fields directly and
passes the same allowance. The overlapping standalone check and the
superseded prefix-only gate were cancelled. The full battery and full
left Kan suite were not run for this slice.

`proof-consumption.*` contains the focus gate result. The positive
extraction passes, and the extraction guard refuses the reference
substitution. `selectors.*` contains the 115 build-selector regressions.
