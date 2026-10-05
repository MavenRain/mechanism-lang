# LeftKanExtension compatibility validation

Base: `58bdc3b`. Recorded on 2026-10-03.

The production gate did not complete. Its combined seven-signature record
fixture exceeded the 600-second checker deadline and produced
`CHECK_TIMEOUT`. Projection checks, regression fixtures, axiom audits and
mutation controls were not reached. This evidence does not establish that the
LeftKanExtension adapter passes the kernel.

The preceding composition and right-whiskering checker commands completed
without diagnostics. The gate reused 15 committed NatTrans support fixtures
after checking their source, import graph and checker hashes. Individual
return codes for the two new support rows were not persisted before the
record timeout, so they are retained as diagnostic evidence rather than a
passing acceptance report.

Python compilation and both worktree and staged diff whitespace checks pass.
The machine had a load average around 62 during validation. A redundant
native compiler rebuild was cancelled; the validation uses the unchanged
native checker and the completed JavaScript audit build.

Full import output, signature fixtures and captured commands remain in
`/Users/oobi/Documents/gpt6/mechanism-leftkan-evidence-06/` and managed job
`/Users/oobi/Documents/gpt6/mechanism-lang-leftkan-compat-20261002/.kanon-wait/job-ngRnBW`.
Earlier attempts 01-05 remain alongside it and were interrupted while
refining or batching the slice, except attempt 01, which also timed out.

Rerun `make prelude-left-kan-compatibility-test` before accepting this slice.
The full gate and its ten mutation controls remain required. If the deadline
needs extending, use the gate driver's `--timeout` option and retain the
resulting acceptance report and all mutation evidence.

## Staged review (2026-10-05)

The frozen import exposes `LeftKanExtension.uniq`, but the original staged
adapter omitted that projection. The fix exposes the fifth stored field,
adds its signature and computation and quantity fixtures, and keeps
`desc_unique` as a derived lemma. The reviewed gate requires the
empty-environment prelude audit; its controls no longer accept `NOT_RUN`.

Python syntax checks, both diff whitespace checks and the Makefile dry run
pass. All 26 copied definitions match the gated Functor and NatTrans adapters.
The copied-definition, unproven-dependency and unmapped-dependency controls pass.
All 17 support rows returned `KERNEL_TYPE_MATCH`, including the two new rows.
The missing-projection regression fails on the original staged adapter and
passes on the fixed adapter as a source API check.

Acceptance remains blocked. The production JavaScript prelude audit timed out
after 900 seconds without diagnostics. A supplementary OCaml record-signature
check timed out after 180 seconds without diagnostics. The older production
gate was interrupted because its driver and control inputs changed during
this review. It did not produce a passing record-signature or full control
report. A native audit build attempt was killed during code generation;
the gate retains its existing JavaScript audit runner.

Full captures and the initial index and working source snapshots are retained
in `/Users/oobi/Documents/gpt6/mechanism-review-20261005/`. Run the complete
production gate on the staged sources before accepting this slice. All eight
record signatures, projection computations, closed instances, both audits,
dependency regressions and the full suite of 11 controls remain required.
