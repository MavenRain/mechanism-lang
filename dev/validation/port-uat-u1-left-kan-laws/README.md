# Left Kan mediator law validation

Base: 186efb925d8cb1358704a43d767a212203b1a7f7. Date: 2026-09-22.

The build, symbolic template check, kernel suite and runtime suite pass.
The kernel suite reports 942 entries, nine families, six computations and
six negative cases. The runtime suite reports six exports, three hosts,
two payloads and 36 comparisons. See `checks.json` and its referenced
stdout and stderr files. The initial OCaml build failed on a partially
applied helper; eta expansion fixed the value restriction. That attempt
is retained separately from the passing build.

All three mutation controls were killed at their designated checks:

- Replacing the identity law with reflexivity fails kernel type checking.
- Omitting the cocone equality in congruence fails kernel type checking.
- Swapping the runtime function fields fails the `lanCongr` arithmetic oracle.

Both unmodified template controls pass. `mutations/results.json` records
exit codes, elapsed times, expected diagnostics, source hashes and full
attempt logs. The runner uses temporary source copies. Reproduce with
`python3 -I dev/left-kan-laws-mutations.py NEW_OUTPUT_DIRECTORY`.

PIN, PIN-DELTA and DENOMINATORS pass. Gate shell syntax, Python compilation,
gate wiring and whitespace checks pass. The existing gate bodies and
predicates are unchanged; both new predicates match the captured success
lines. `sources.sha256` records the checked source files relative to the
repository root. Recheck with
`shasum -a 256 -c dev/validation/port-uat-u1-left-kan-laws/sources.sha256`.

Validation is scoped to this additive prelude slice. The full battery was
not rerun. TRUSTED-LINES retains the inherited failure at
`kernel=5475/3000 encoder=246/900`. No threshold or trusted source changed.
